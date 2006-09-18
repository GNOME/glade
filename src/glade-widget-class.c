/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2004 Imendio AB
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Chema Celorio <chema@celorio.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* For g_file_exists */
#include <sys/types.h>
#include <string.h>

#include <glib/gdir.h>
#include <gmodule.h>
#include <ctype.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtkenums.h> /* This should go away. Chema */

#include "glade.h"
#include "glade-widget-class.h"
#include "glade-xml-utils.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-signal.h"
#include "glade-parameter.h"
#include "glade-debug.h"
#include "glade-fixed.h"

/* hash table that will contain all the GtkWidgetClass'es created, indexed by its name */
static GHashTable *widget_classes = NULL;


#define GLADE_LARGE_ICON_SUBDIR "22x22"
#define GLADE_LARGE_ICON_SIZE 22

#define GLADE_SMALL_ICON_SUBDIR "16x16"
#define GLADE_SMALL_ICON_SIZE 16


typedef struct {
	gchar *parent_name;
	GList *packing_defaults;
} GladeChildPacking;

static void
glade_widget_class_free_child (GladeSupportedChild *child)
{
	g_list_foreach (child->properties, (GFunc) glade_property_class_free, NULL);
	g_list_free (child->properties);
	g_free (child);
}

static void
glade_widget_class_packing_default_free (GladePackingDefault *def)
{
	g_free (def->id);
	g_free (def->value);
}

static void
glade_widget_class_child_packing_free (GladeChildPacking *packing)
{
        g_free (packing->parent_name);

        g_list_foreach (packing->packing_defaults,
                        (GFunc) glade_widget_class_packing_default_free, NULL);
        g_list_free (packing->packing_defaults);
}

/**
 * glade_widget_class_free:
 * @widget_class: a #GladeWidgetClass
 *
 * Frees @widget_class and its associated memory.
 */
void
glade_widget_class_free (GladeWidgetClass *widget_class)
{
	if (widget_class == NULL)
		return;

	g_free (widget_class->generic_name);
	g_free (widget_class->name);
	g_free (widget_class->catalog);
	if (widget_class->book)
		g_free (widget_class->book);

	g_list_foreach (widget_class->properties, (GFunc) glade_property_class_free, NULL);
	g_list_free (widget_class->properties);

	g_list_foreach (widget_class->children, (GFunc) glade_widget_class_free_child, NULL);
	g_list_free (widget_class->children);

	g_list_foreach (widget_class->child_packings,
			(GFunc) glade_widget_class_child_packing_free,
			NULL);

	g_list_free (widget_class->child_packings);

	g_list_foreach (widget_class->signals, (GFunc) glade_signal_free, NULL);
	g_list_free (widget_class->signals);

	if (widget_class->cursor != NULL)
		gdk_cursor_unref (widget_class->cursor);

	if (widget_class->large_icon != NULL)
		g_object_unref (G_OBJECT (widget_class->large_icon));

	if (widget_class->small_icon != NULL)
		g_object_unref (G_OBJECT (widget_class->small_icon));

}

static GList *
gwc_props_from_pspecs (GladeWidgetClass  *class,
		       GParamSpec       **specs,
		       gint               n_specs)
{
	GladePropertyClass *property_class;
	gint                i;
	GList              *list = NULL;

	for (i = 0; i < n_specs; i++)
	{
		if ((property_class = 
		     glade_property_class_new_from_spec (class, specs[i])) != NULL)
			list = g_list_prepend (list, property_class);
	}
	return g_list_reverse (list);
}

static GList *
glade_widget_class_list_properties (GladeWidgetClass *class)
{
	GObjectClass  *object_class;
	GParamSpec   **specs = NULL;
	guint          n_specs = 0;
	GList         *list, *atk_list = NULL;

	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), NULL);

	/* Let it leek */
	if ((object_class = g_type_class_ref (class->type)) == NULL)
	{
		g_critical ("Failed to get class for type %s\n", 
			    g_type_name (class->type));
		return NULL;
	}

	/* list class properties */
	specs = g_object_class_list_properties (object_class, &n_specs);
	list = gwc_props_from_pspecs (class, specs, n_specs);
	g_free (specs);

	/* GtkWidgetClass & descendants have accelerators */
	if (g_type_is_a (class->type, GTK_TYPE_WIDGET))
		list = g_list_append (list, glade_property_class_accel_property
				      (class, class->type));

	/* list the (hard-coded) atk relation properties if applicable */
	if (glade_util_class_implements_interface (class->type, 
						   ATK_TYPE_IMPLEMENTOR))
		atk_list = glade_property_class_list_atk_relations (class, class->type);

	return g_list_concat (list, atk_list);
}

/*
This function assignes "weight" to each property in its natural order staring from 1.
If @parent is 0 weight will be set for every GladePropertyClass in the list.
This function will not override weight if it is already set (weight >= 0.0)
*/
static void
glade_widget_class_properties_set_weight (GList **properties, GType parent)
{
	gint normal = 0, common = 0, packing = 0;
	GList *l;

	for (l = *properties; l && l->data; l = g_list_next (l))
	{
		GladePropertyClass *class = l->data;
		GPCType type = class->type;
	
		if (class->visible &&
		    (parent) ? parent == class->pspec->owner_type : TRUE &&
	    	    (type == GPC_NORMAL || type == GPC_ACCEL_PROPERTY))
		{
			/* Use a different counter for each tab (common, packing and normal) */
			if (class->common) common++;
			else if (class->packing) packing++;
			else normal++;

			/* Skip if it is already set */
			if (class->weight >= 0.0) continue;
			
			/* Special-casing weight of properties for seperate tabs */
			if (class->common) class->weight = common;
			else if (class->packing) class->weight = packing;
			else class->weight = normal;
		}
	}
}

static GList * 
glade_widget_class_list_child_properties (GladeWidgetClass *class) 
{
	GladePropertyClass  *property_class;
	GObjectClass        *object_class;
	GParamSpec         **specs = NULL;
	guint                n_specs = 0;
	GList               *list = NULL, *l;

	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), NULL);

	/* only GtkContainer child propeties can be introspected */
	if (!g_type_is_a (class->type, GTK_TYPE_CONTAINER))
		return NULL;

	/* Let it leek */
	if ((object_class = g_type_class_ref (class->type)) == NULL)
	{
		g_warning ("Failed to get class for type %s\n", 
			   g_type_name (class->type));
		return NULL;
	}

	specs = gtk_container_class_list_child_properties (object_class, &n_specs);
	list  = gwc_props_from_pspecs (class, specs, n_specs);
	g_free (specs);

	/* We have to mark packing properties from GladeWidgetClass
	 * because GladePropertyClass doesnt have a valid parent GType
	 * to introspect it.
	 *
	 * (which could be used to call gtk_container_class_find_child_property()
	 * and properly introspect whether or not its a packing property).
	 */
	for (l = list; l; l = l->next)
	{
		property_class = l->data;
		property_class->packing = TRUE;
	}
	
	/* Set default weight on packing properties */
	glade_widget_class_properties_set_weight (&list, 0);
	
	return list;
}

static GList *
glade_widget_class_list_children (GladeWidgetClass *class)
{
	GladeSupportedChild *child;
	GList               *children = NULL;

	/* Implicitly handle GtkContainer and derivitive types and retrieve packing properties.
	 */
	if (g_type_is_a (class->type, GTK_TYPE_CONTAINER))
	{
		child = g_new0 (GladeSupportedChild, 1);
		child->type             = GTK_TYPE_WIDGET;
		child->properties       = glade_widget_class_list_child_properties (class);

		if (class->type == GTK_TYPE_CONTAINER)
		{
			child->add              = (GladeAddChildFunc)    gtk_container_add;
			child->remove           = (GladeRemoveChildFunc) gtk_container_remove;
			child->get_children     =
				(GladeGetChildrenFunc) glade_util_container_get_all_children;
			child->set_property     =
				(GladeChildSetPropertyFunc) gtk_container_child_set_property;
			child->get_property     =
				(GladeChildGetPropertyFunc) gtk_container_child_get_property;
		}

		children = g_list_append (children, child);
	}
	return children;
}

static gint
glade_widget_class_signal_comp (gconstpointer a, gconstpointer b)
{
	const GladeSignalClass *signal_a = a, *signal_b = b;	
	return strcmp (signal_b->query.signal_name, signal_a->query.signal_name);
}

static void
glade_widget_class_add_signals (GList **signals, GType type)
{
	guint count, *sig_ids, num_signals;
	GladeSignalClass *cur;
	GList *list = NULL;
	
	if (G_TYPE_IS_INSTANTIATABLE (type) || G_TYPE_IS_INTERFACE (type))
	{
		sig_ids = g_signal_list_ids (type, &num_signals);

		for (count = 0; count < num_signals; count++)
		{
			cur = g_new0 (GladeSignalClass, 1);
			
			g_signal_query (sig_ids[count], &(cur->query));

			/* Since glib gave us this signal id... it should
			 * exist no matter what.
			 */
			g_assert (cur->query.signal_id != 0);

			cur->name = (cur->query.signal_name);
			cur->type = (gchar *) g_type_name (type);

			list = g_list_prepend (list, cur);
		}
		g_free (sig_ids);
		
		list = g_list_sort (list, glade_widget_class_signal_comp);
		*signals = g_list_concat (list, *signals);
	}
}

static GList * 
glade_widget_class_list_signals (GladeWidgetClass *class) 
{
	GList *signals = NULL;
	GType type, parent, *i, *p;

	g_return_val_if_fail (class->type != 0, NULL);

	for (type = class->type; g_type_is_a (type, G_TYPE_OBJECT); type = parent)
	{
		parent = g_type_parent (type);
		
		/* Add class signals */
		glade_widget_class_add_signals (&signals, type);
	
		/* Add class interfaces signals */
		for (i = p = g_type_interfaces (type, NULL); *i; i++)
			if (!glade_util_class_implements_interface (parent, *i))
				glade_widget_class_add_signals (&signals, *i);

		g_free (p);
	}

	return g_list_reverse (signals);
}

static void
glade_widget_class_load_icons (GladeWidgetClass *class)
{
	GError    *error = NULL;
	gchar     *icon_path;


	/* only certain widget classes need to have icons */
	if (G_TYPE_IS_INSTANTIATABLE (class->type) == FALSE ||
            G_TYPE_IS_ABSTRACT (class->type) != FALSE ||
            class->generic_name == NULL)
	{
		return;
	}

	/* load large 22x22 icon */
	icon_path = g_strdup_printf ("%s" G_DIR_SEPARATOR_S GLADE_LARGE_ICON_SUBDIR G_DIR_SEPARATOR_S "%s.png", 
				     glade_pixmaps_dir, 
				     class->generic_name);

	class->large_icon = gdk_pixbuf_new_from_file_at_size (icon_path, 
							      GLADE_LARGE_ICON_SIZE, 
							      GLADE_LARGE_ICON_SIZE, 
							      &error);
	
	if (class->large_icon == NULL)
	{
		g_warning (_("Unable to load icon for %s (%s)"), class->name, error->message);

		g_error_free (error);
		error = NULL;

		/* use stock missing icon */
		class->large_icon = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), 
							      GTK_STOCK_MISSING_IMAGE, 
						  	      GLADE_LARGE_ICON_SIZE, 
							      GTK_ICON_LOOKUP_USE_BUILTIN, &error);
		if (class->large_icon == NULL)
		{
			g_critical (_("Unable to load stock icon (%s)"),
				    error->message);

			g_error_free (error);
			error = NULL;
		}
	}
	g_free (icon_path);


	/* load small 16x16 icon */
	icon_path = g_strdup_printf ("%s" G_DIR_SEPARATOR_S GLADE_SMALL_ICON_SUBDIR G_DIR_SEPARATOR_S "%s.png", 
				     glade_pixmaps_dir, 
				     class->generic_name);

	class->small_icon = gdk_pixbuf_new_from_file_at_size (icon_path, 
							      GLADE_SMALL_ICON_SIZE, 
							      GLADE_SMALL_ICON_SIZE, 
							      &error);
	
	if (class->small_icon == NULL)
	{
		g_warning (_("Unable to load icon for %s (%s)"), class->name, error->message);

		g_error_free (error);
		error = NULL;

		/* use stock missing icon */
		class->small_icon = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), 
							      GTK_STOCK_MISSING_IMAGE, 
						  	      GLADE_SMALL_ICON_SIZE, 
							      GTK_ICON_LOOKUP_USE_BUILTIN, &error);
		if (class->small_icon == NULL)
		{
			g_critical (_("Unable to load stock icon (%s)"),
				    error->message);

			g_error_free (error);
			error = NULL;
		}
	}

	g_free (icon_path);

}

static void
glade_widget_class_create_cursor (GladeWidgetClass *widget_class)
{
	GdkPixbuf *tmp_pixbuf;
	const GdkPixbuf *add_pixbuf;
	GdkDisplay *display;

	/* only certain widget classes need to have cursors */
	if (G_TYPE_IS_INSTANTIATABLE (widget_class->type) == FALSE ||
            G_TYPE_IS_ABSTRACT (widget_class->type) != FALSE ||
            widget_class->generic_name == NULL)
		return;

	/* cannot continue if 'add widget' cursor pixbuf has not been loaded for any reason */
	if ((add_pixbuf = glade_cursor_get_add_widget_pixbuf ()) == NULL)
		return;

	display = gdk_display_get_default ();

	/* create a temporary pixbuf clear to transparent black*/
	tmp_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 32, 32);
	gdk_pixbuf_fill (tmp_pixbuf, 0x00000000);


	/* composite pixbufs */
	gdk_pixbuf_composite (widget_class->large_icon, tmp_pixbuf,
			      8, 8, 22, 22,
			      8, 8, 1, 1,
                              GDK_INTERP_NEAREST, 255);

	gdk_pixbuf_composite (add_pixbuf, tmp_pixbuf,
			      0, 0, 12, 12,
			      0, 0, 1, 1,
                              GDK_INTERP_NEAREST, 255);


	widget_class->cursor = gdk_cursor_new_from_pixbuf (display, tmp_pixbuf, 6, 6);

	g_object_unref(tmp_pixbuf);
}

static void
glade_widget_class_update_properties_from_node (GladeXmlNode      *node,
						GladeWidgetClass  *widget_class,
						GList            **properties,
						const gchar       *domain)
{
	GladeXmlNode *child;

	for (child = glade_xml_node_get_children (node);
	     child; child = glade_xml_node_next (child))
	{
		gchar *id;
		GList *list;
		GladePropertyClass *property_class;
		gboolean updated;

		if (!glade_xml_node_verify (child, GLADE_TAG_PROPERTY))
			continue;

		id = glade_xml_get_property_string_required
			(child, GLADE_TAG_ID, widget_class->name);
		if (!id)
			continue;

		/* property names from catalogs also need to have the '-' form */
		glade_util_replace (id, '_', '-');
		
		/* find the property in our list, if not found append a new property */
		for (list = *properties; list && list->data; list = list->next)
		{
			property_class = GLADE_PROPERTY_CLASS (list->data);
			if (property_class->id != NULL &&
			    g_ascii_strcasecmp (id, property_class->id) == 0)
				break;
		}

		if (list)
		{
			property_class = GLADE_PROPERTY_CLASS (list->data);
		}
		else
		{
			property_class = glade_property_class_new (widget_class);
			property_class->id = g_strdup (id);
			*properties = g_list_append (*properties, property_class);
			list = g_list_last (*properties);
		}

		updated = glade_property_class_update_from_node
			(child, widget_class->module, 
			 widget_class->type,
			 &property_class, domain);
		if (!updated)
		{
			g_warning ("failed to update %s property of %s from xml",
				   id, widget_class->name);
			g_free (id);
			continue;
		}

		/* the property has Disabled=TRUE ... */
		if (!property_class)
			*properties = g_list_delete_link (*properties, list);

		g_free (id);
	}
}

static void
glade_widget_class_set_packing_defaults_from_node (GladeXmlNode     *node,
						   GladeWidgetClass *widget_class)
{
        GladeXmlNode *child;

        child = glade_xml_node_get_children (node);
        for (; child; child = glade_xml_node_next (child))
        {
                gchar             *name;
                GladeXmlNode      *prop_node;
		GladeChildPacking *packing;

                if (!glade_xml_node_verify (child, GLADE_TAG_PARENT_CLASS))
                        continue;

                name = glade_xml_get_property_string_required (child, 
                                                               GLADE_TAG_NAME, 
                                                               widget_class->name);

                if (!name)
                        continue;

		packing = g_new0 (GladeChildPacking, 1);
		packing->parent_name = name;
		packing->packing_defaults = NULL;

		widget_class->child_packings = g_list_prepend (widget_class->child_packings, packing);

		prop_node = glade_xml_node_get_children (child);
                for (; prop_node; prop_node = glade_xml_node_next (prop_node))
                {
			GladePackingDefault *def;
			gchar               *id;
			gchar               *value;

			id = glade_xml_get_property_string_required (prop_node,
								     GLADE_TAG_ID,
								     widget_class->name);
			if (!id)
				continue;

			value = glade_xml_get_property_string_required (prop_node,
									GLADE_TAG_DEFAULT,
									widget_class->name);
			if (!value)
			{
				g_free (id);
				continue;
			}

			def = g_new0 (GladePackingDefault, 1);
			def->id = id;
			def->value = value;

			packing->packing_defaults = g_list_prepend (packing->packing_defaults,
								    def);
                }
	}
}

static gint
glade_widget_class_find_child_by_type (GladeSupportedChild *child, GType type)
{
	return child->type - type;
}

static void
glade_widget_class_update_children_from_node (GladeXmlNode     *node,
					      GladeWidgetClass *widget_class,
					      const gchar      *domain)
{
	GladeSupportedChild *child;
	GladeXmlNode        *child_node, *properties;
	gchar               *buff;
	GList               *list;
	GType                type;
	
	for (child_node = glade_xml_node_get_children (node);
	     child_node;
	     child_node = glade_xml_node_next (child_node))
	{

		if (!glade_xml_node_verify (child_node, GLADE_TAG_CHILD))
			continue;

		/* Use alot of emacs realastate to ensure that we have a type. */
		if ((buff = glade_xml_get_value_string (child_node, GLADE_TAG_TYPE)) != NULL)
		{
			if ((type = glade_util_get_type_from_name (buff)) == 0)
			{
				g_free (buff);
				continue;
			}
			g_free (buff);
		} else {
			g_warning ("Child specified without a type, ignoring");
			continue;
		}

		if (widget_class->children &&
		    (list = g_list_find_custom
		     (widget_class->children, GINT_TO_POINTER(type),
		      (GCompareFunc)glade_widget_class_find_child_by_type)) != NULL)
		{
			child = (GladeSupportedChild *)list->data;
		}
		else
		{
			child                  = g_new0 (GladeSupportedChild, 1);
			child->type            = type;
			widget_class->children = g_list_append (widget_class->children, child);
		}

		if ((buff = 
		     glade_xml_get_value_string (child_node,
						 GLADE_TAG_SPECIAL_CHILD_TYPE)) != NULL)
		{
			g_free (child->special_child_type);
			child->special_child_type = buff;
		}

		if (widget_class->module)
		{
			glade_xml_load_sym_from_node (child_node, widget_class->module,
						      GLADE_TAG_ADD_CHILD_FUNCTION,
						      (gpointer *)&child->add);
			glade_xml_load_sym_from_node (child_node, widget_class->module,
						      GLADE_TAG_REMOVE_CHILD_FUNCTION,
						      (gpointer *)&child->remove);
			glade_xml_load_sym_from_node (child_node, widget_class->module,
						      GLADE_TAG_GET_CHILDREN_FUNCTION,
						      (gpointer *)&child->get_children);
			glade_xml_load_sym_from_node (child_node, widget_class->module,
						      GLADE_TAG_CHILD_SET_PROP_FUNCTION,
						      (gpointer *)&child->set_property);
			glade_xml_load_sym_from_node (child_node, widget_class->module,
						      GLADE_TAG_CHILD_GET_PROP_FUNCTION,
						      (gpointer *)&child->get_property);
			glade_xml_load_sym_from_node (child_node, widget_class->module,
						      GLADE_TAG_REPLACE_CHILD_FUNCTION,
						      (gpointer *)&child->replace_child);
		}

		/* if we found a <Properties> tag on the xml file, we add the 
		 * properties that we read from the xml file to the class.
		 */
		if ((properties =
		     glade_xml_search_child (child_node, GLADE_TAG_PROPERTIES)) != NULL)
		{
			glade_widget_class_update_properties_from_node
				(properties, widget_class, &child->properties, domain);

			for (list = child->properties; list != NULL; list = list->next)
				((GladePropertyClass *)list->data)->packing = TRUE;
		}
	}
}

static gboolean
glade_widget_class_extend_with_node (GladeWidgetClass *widget_class, 
				     GladeXmlNode     *node,
				     const gchar      *domain)
{
	GladeXmlNode *child;

	g_return_val_if_fail (widget_class != NULL, FALSE);
	g_return_val_if_fail (node != NULL, FALSE);

	if (widget_class->module)
	{
		glade_xml_load_sym_from_node (node, widget_class->module,
					      GLADE_TAG_POST_CREATE_FUNCTION,
					      (void **)&widget_class->post_create_function);

		glade_xml_load_sym_from_node (node, widget_class->module,
					      GLADE_TAG_GET_INTERNAL_CHILD_FUNCTION,
					      (void **)&widget_class->get_internal_child);

		glade_xml_load_sym_from_node (node, widget_class->module,
					      GLADE_TAG_LAUNCH_EDITOR_FUNCTION,
					      (void **)
					      &widget_class->launch_editor);
	}
	widget_class->fixed = 
		glade_xml_get_property_boolean (node, GLADE_TAG_FIXED, widget_class->fixed);

	/* Check if this class is toplevel */
	widget_class->toplevel =
		glade_xml_get_property_boolean (node, GLADE_XML_TAG_TOPLEVEL,
						widget_class->toplevel);

	/* if we found a <properties> tag on the xml file, we add the properties
	 * that we read from the xml file to the class.
	 */
	child = glade_xml_search_child (node, GLADE_TAG_PROPERTIES);
	if (child)
	{
		glade_widget_class_update_properties_from_node
			(child, widget_class, &widget_class->properties, domain);
	}

	child = glade_xml_search_child (node, GLADE_TAG_CHILDREN);
	if (child)
	{
		glade_widget_class_update_children_from_node
			(child, widget_class, domain);
	}

	child = glade_xml_search_child (node, GLADE_TAG_PACKING_DEFAULTS);
	if (child)
	{
		glade_widget_class_set_packing_defaults_from_node (child,
								   widget_class);
	}

	return TRUE;
}

/**
 * glade_widget_class_get_by_name:
 * @name: name of the widget class (for instance: GtkButton)
 *
 * Returns: an existing #GladeWidgetClass with the name equaling @name,
 *          or %NULL if such a class doesn't exist
 **/
GladeWidgetClass *
glade_widget_class_get_by_name (const char *name)
{
	if (widget_classes != NULL)
		return g_hash_table_lookup (widget_classes, name);
	else
		return NULL;
}

typedef struct
{
	GType              type;
	GladeWidgetClass  *class;
} GladeClassSearchPair;

static void
search_class_by_type (gchar                *name,
		      GladeWidgetClass     *class,
		      GladeClassSearchPair *pair)
{
	if (class->type == pair->type)
		pair->class = class;
}

GladeWidgetClass *
glade_widget_class_get_by_type (GType type)
{
	GladeClassSearchPair pair = { type, NULL };
	
	if (widget_classes != NULL)
	{
		g_hash_table_foreach (widget_classes, (GHFunc)search_class_by_type, &pair);
	}
	return pair.class;
}

typedef struct
{
	GType  type;
	GList *list;
} GladeClassAccumPair;

static void
accum_class_by_type (gchar                *name,
		     GladeWidgetClass     *class,
		     GladeClassAccumPair  *pair)
{
	if (g_type_is_a (class->type, pair->type))
		pair->list = g_list_prepend (pair->list, class);
}

GList *
glade_widget_class_get_derived_types (GType type)
{
	GladeClassAccumPair pair = { type, NULL };

	if (widget_classes)
		g_hash_table_foreach (widget_classes, (GHFunc) accum_class_by_type, &pair);

	return pair.list;
}


/**
 * glade_widget_class_merge_properties:
 * @widget_properties: Pointer to the list of properties in the widget.
 * @parent_class: List of properties in the parent.
 *
 * Merges the properties found in the parent_properties list with the
 * properties found in the @widget_properties list.
 * The properties in the parent_properties will be prepended to 
 * @widget_properties.
 **/
static void
glade_widget_class_merge_properties (GType   parent_type, 
				     GList **widget_properties, 
				     GList  *parent_properties)
{
	GladePropertyClass  *property_class;
	GObjectClass        *object_class;
	GParamSpec         **specs = NULL, *spec;
	GList               *list, *list2, *remove;
	guint                 i, n_specs;
	gboolean             found;

	for (list = parent_properties; list; list = list->next)
	{
		GladePropertyClass *parent_p_class = list->data;
		GladePropertyClass *child_p_class;

		
		/* search the child's properties for one with the same id */
		for (list2 = *widget_properties; list2; list2 = list2->next)
		{
			child_p_class = list2->data;
			if (strcmp (parent_p_class->id, child_p_class->id) == 0)
				break;
		}

		/* if not found, append a clone of the parent's one; if found
		 * but the parent one was modified (and not the child one)
		 * substitute it (note it is importand to *append* and not prepend
		 * since this would screw up the order of custom properties in
		 * derived objects).
		 */
		if (!list2)
		{
			property_class = glade_property_class_clone (parent_p_class);
			*widget_properties = g_list_append (*widget_properties, property_class);
		}
		else if (parent_p_class->is_modified && !child_p_class->is_modified)
		{
			glade_property_class_free (child_p_class);
			list2->data = glade_property_class_clone (parent_p_class);
		}
	}

	/* Remove any properties found in widget_properties not found in parent_properties
	 * if parent_properties should have it through introspection
	 */
	object_class = g_type_class_ref (parent_type); /* Let it leek please */
	specs        = g_object_class_list_properties (object_class, &n_specs);

	for (i = 0; i < n_specs; i++)
	{
		spec = specs[i];
		found = FALSE;
		
		for (list = parent_properties; list; list = list->next)
		{
			property_class = list->data;

			/* We only use the writable properties */
			if ((spec->flags & G_PARAM_WRITABLE) &&
			    !strcmp (property_class->id, spec->name)) 
			{
				found = TRUE;
				break;
			}
		}

		/* If we didnt find a property in the parents properties thats
		 * listed in its real properties, remove those properties from
		 * the child properties.
		 */
		if (!found)
		{
			remove = NULL;
			for (list = *widget_properties; list; list = list->next)
			{
				property_class = list->data;

				if (!strcmp (property_class->id, spec->name))
				{
					remove = g_list_prepend (remove, list);
					break;
				}
			}
			for (list = remove; list; list = list->next)
			{
				*widget_properties =
					g_list_delete_link (*widget_properties,
							    (GList *)list->data);
			}
			g_list_free (remove);
		}
	} // for i in specs
	g_free (specs);
}

static GladeSupportedChild *
glade_widget_class_clone_child (GladeSupportedChild *child,
				GladeWidgetClass *parent_class)
{
	GladeSupportedChild *clone;
	
	clone = g_new0 (GladeSupportedChild, 1);

	clone->type              = child->type;
	clone->add               = child->add;
	clone->remove            = child->remove;
	clone->get_children      = child->get_children;
	clone->set_property      = child->set_property;
	clone->get_property      = child->get_property;
	clone->replace_child     = child->replace_child;

	clone->properties = glade_widget_class_list_child_properties (parent_class);
	glade_widget_class_merge_properties
		(parent_class->type, &clone->properties, child->properties);

	return clone;
}

static void
glade_widget_class_merge_child (GladeSupportedChild *widgets_child,
				GladeSupportedChild *parents_child)
{
	if (!widgets_child->add)
		widgets_child->add              = parents_child->add;
	if (!widgets_child->remove)
		widgets_child->remove           = parents_child->remove;
	if (!widgets_child->get_children)
		widgets_child->get_children     = parents_child->get_children;
	if (!widgets_child->set_property)
		widgets_child->set_property     = parents_child->set_property;
	if (!widgets_child->get_property)
		widgets_child->get_property     = parents_child->get_property;
	if (!widgets_child->replace_child)
		widgets_child->replace_child    = parents_child->replace_child;

	glade_widget_class_merge_properties
		(parents_child->type, 
		 &widgets_child->properties, parents_child->properties);
}

static void
glade_widget_class_merge_children (GList **widget_children,
				   GList  *parent_children,
				   GladeWidgetClass *widget_class)
{
	GList *list;
	GList *list2;

	for (list = parent_children; list && list->data; list = list->next)
	{
		GladeSupportedChild *parents_child = list->data;
		GladeSupportedChild *widgets_child = NULL;

		for (list2 = *widget_children; list2 && list2->data; list2 = list2->next)
		{
			widgets_child = list2->data;
			if (widgets_child->type == parents_child->type)
				break;
		}

		/* if not found, prepend a clone of the parent's one; if found
		 * but the parent one was modified (and not the child one)
		 * substitute it.
		 */
		if (!list2)
		{
			*widget_children =
				g_list_prepend (*widget_children,
						glade_widget_class_clone_child (parents_child,
										widget_class));
		}
		else
		{
			glade_widget_class_merge_child (widgets_child, parents_child);
		}
	}
}


/**
 * glade_widget_class_merge:
 * @widget_class: main class.
 * @parent_class: secondary class.
 *
 * Merges the contents of the @parent_class on the @widget_class.
 * The properties of the @parent_class will be prepended to
 * those of @widget_class.
 */
static void
glade_widget_class_merge (GladeWidgetClass *widget_class,
			  GladeWidgetClass *parent_class)
{
	g_return_if_fail (GLADE_IS_WIDGET_CLASS (widget_class));
	g_return_if_fail (GLADE_IS_WIDGET_CLASS (parent_class));

	if (widget_class->fixed == FALSE)
		widget_class->fixed = parent_class->fixed;
	
	if (widget_class->post_create_function == NULL)
		widget_class->post_create_function = parent_class->post_create_function;

	if (widget_class->get_internal_child == NULL)
		widget_class->get_internal_child = parent_class->get_internal_child;

	if (widget_class->launch_editor == NULL)
		widget_class->launch_editor = parent_class->launch_editor;

	if (widget_class->toplevel == FALSE)
		widget_class->toplevel = parent_class->toplevel;
	
	/* merge the parent's properties */
	glade_widget_class_merge_properties
		(parent_class->type,
		 &widget_class->properties, parent_class->properties);

	/* merge the parent's supported children */
	glade_widget_class_merge_children
		(&widget_class->children, parent_class->children, widget_class);
}

/**
 * glade_widget_class_new:
 * @class_node: A #GladeXmlNode
 * @catname: the name of the owning catalog
 * @library: the name of the library used to load class methods from
 * @domain: the domain to translate strings from this plugin from
 * @book: the devhelp search domain for the owning catalog.
 *
 * Merges the contents of the @parent_class on the @widget_class.
 * The properties of the @parent_class will be prepended to
 * those of @widget_class.
 */
GladeWidgetClass *
glade_widget_class_new (GladeXmlNode *class_node,
			const gchar  *catname, 
			const gchar  *library, 
			const gchar  *domain,
			const gchar  *book)
{
	GladeWidgetClass *widget_class;
	gchar            *name, *generic_name, *ptr;
	gchar            *title;
	GModule          *module;
	GType             parent_type;

	if (!glade_xml_node_verify (class_node, GLADE_TAG_GLADE_WIDGET_CLASS))
	{
		g_warning ("Widget class node is not '%s'", 
			   GLADE_TAG_GLADE_WIDGET_CLASS);
		return NULL;
	}
	
	if ((name = glade_xml_get_property_string_required
	     (class_node, GLADE_TAG_NAME, NULL)) == NULL)
		return NULL;

	if (glade_widget_class_get_by_name (name)) 
	{
		g_warning ("Widget class '%s' already defined", name);

		g_free (name);
		return NULL;
	}

	/* Perform a stupid fallback as generic_name is not required
	 * for uninstantiatable classes
	 */
	generic_name = glade_xml_get_property_string
		(class_node, GLADE_TAG_GENERIC_NAME);

	if ((title = glade_xml_get_property_string_required
	     (class_node, GLADE_TAG_TITLE,
	      "This value is needed to display object class names in the UI")) == NULL)
	{
		g_warning ("Class '%s' built without a '%s'", name, GLADE_TAG_TITLE);
		title = g_strdup (name);
	}
	
	/* Translate title */
	if (title != dgettext (domain, title))
	{
		ptr   = dgettext (domain, title);
		g_free (title);
		title = ptr;
	}

	module = NULL;
	if (library) 
	{
		module = glade_util_load_library (library);
		if (!module)
		{
			g_warning ("Failed to load external library '%s'",
				   library);
			g_free (name);
			g_free (generic_name);
			g_free (title);
			return NULL;
		}
	}
	
	widget_class = g_new0 (GladeWidgetClass, 1);
	widget_class->name         = name;
	widget_class->module       = module;
	widget_class->generic_name = generic_name;
	widget_class->palette_name = title;
	widget_class->type         = glade_util_get_type_from_name (name);
	widget_class->cursor       = NULL;
	widget_class->large_icon   = NULL;
	widget_class->small_icon   = NULL;
	widget_class->toplevel     = FALSE;

	if (G_TYPE_IS_INSTANTIATABLE (widget_class->type)    &&
	    G_TYPE_IS_ABSTRACT (widget_class->type) == FALSE &&
	    widget_class->generic_name == NULL)
	{
		g_warning ("Instantiatable class '%s' built without a '%s'",
			   name, GLADE_TAG_GENERIC_NAME);
	}

	/* Perform a stoopid fallback just incase */
	if (widget_class->generic_name == NULL)
		widget_class->generic_name = g_strdup ("widget");

	
	/* Dont mention gtk+ as a required lib in the generated glade file */
	if (strcmp (catname, "gtk+"))
		widget_class->catalog = g_strdup (catname);

	if (book)
		widget_class->book = g_strdup (book);

	if (widget_class->type == 0)
	{
		glade_widget_class_free (widget_class);
		return NULL;
	}

	widget_class->properties = glade_widget_class_list_properties (widget_class);
	widget_class->signals    = glade_widget_class_list_signals (widget_class);
	widget_class->children   = glade_widget_class_list_children (widget_class);

	glade_widget_class_load_icons (widget_class);
	glade_widget_class_create_cursor (widget_class);

	for (parent_type = g_type_parent (widget_class->type);
	     parent_type != 0;
	     parent_type = g_type_parent (parent_type))
	{
		const gchar      *parent_name;
		GladeWidgetClass *parent_class;

		parent_name  = g_type_name (parent_type);
		parent_class = glade_widget_class_get_by_name (parent_name);

		if (parent_class)
			glade_widget_class_merge (widget_class, parent_class);
	}
	
	glade_widget_class_extend_with_node (widget_class, class_node, domain);
	
	/* store the GladeWidgetClass on the cache,
	 * if it's the first time we store a widget class, then
	 * initialize the global widget_classes hash.
	 */
	if (!widget_classes)
		widget_classes = g_hash_table_new (g_str_hash, g_str_equal);		
	g_hash_table_insert (widget_classes, widget_class->name, widget_class);

	/* Set default weight on properties */
	for (parent_type = widget_class->type;
	     parent_type != 0;
	     parent_type = g_type_parent (parent_type))
		glade_widget_class_properties_set_weight (&widget_class->properties,
							  parent_type);
	
	return widget_class;
}

/**
 * glade_widget_class_get_name:
 * @widget: a #GladeWidgetClass
 *
 * Returns: the name of @widget
 */
const gchar *
glade_widget_class_get_name (GladeWidgetClass *widget)
{
	return widget->name;
}

/**
 * glade_widget_class_get_type:
 * @widget: a #GladeWidgetClass
 *
 * Returns: the #GType of @widget
 */
GType
glade_widget_class_get_type (GladeWidgetClass *widget)
{
	return widget->type;
}

/**
 * glade_widget_class_get_property_class:
 * @class: a #GladeWidgetClass
 * @name: a string
 *
 * Returns: The #GladePropertyClass object if there is one associated to this widget
 *          class.
 */
GladePropertyClass *
glade_widget_class_get_property_class (GladeWidgetClass *class, 
				       const gchar      *name)
{
	GList *list, *l;
	GladePropertyClass *pclass;

	for (list = class->properties; list && list->data; list = list->next)
	{
		pclass = list->data;
		if (strcmp (pclass->id, name) == 0)
			return pclass;
	}

	for (list = class->children; list && list->data; list = list->next)
	{
		GladeSupportedChild *support = list->data;
	
		for (l = support->properties; l && l->data; l = l->next)
		{
			pclass = l->data;
			if (strcmp (pclass->id, name) == 0)
				return pclass;
		}
	}

	return NULL;
}

/**
 * glade_widget_class_dump_param_specs:
 * @class: a #GladeWidgetClass
 * 
 * Dump to the console the properties of @class as specified
 * by gtk+. You can also run glade3 with : "glade-3 --dump GtkWindow" to
 * get dump a widget class properties.
 */
void
glade_widget_class_dump_param_specs (GladeWidgetClass *class)
{
	GObjectClass *object_class;
	GParamSpec **specs = NULL;
	GParamSpec *spec;
	GType last;
	guint n_specs = 0;
	gint i;

	g_return_if_fail (GLADE_IS_WIDGET_CLASS (class));

	g_type_class_ref (class->type); /* hmm */
	 /* We count on the fact we have an instance, or else we'd have
	  * to use g_type_class_ref ();
	  */

	object_class = g_type_class_peek (class->type);
	if (!object_class)
	{
		g_warning ("Class peek failed\n");
		return;
	}

	specs = g_object_class_list_properties (object_class, &n_specs);

	g_print ("\nDumping ParamSpec for %s\n", class->name);

	last = 0;
	for (i = 0; i < n_specs; i++)
	{
		spec = specs[i];
		if (last != spec->owner_type)
			g_print ("\n                    --  %s -- \n",
				 g_type_name (spec->owner_type));
		g_print ("%02d - %-25s %-25s (%s)\n",
			 i,
			 spec->name,
			 g_type_name (spec->value_type),
			 (spec->flags & G_PARAM_WRITABLE) ? "Writable" : "ReadOnly");
		last = spec->owner_type;
	}
	g_print ("\n");

	g_free (specs);
}

/**
 * glade_widget_class_get_child_support:
 * @class: a #GladeWidgetClass
 * @child_type: a #GType
 * 
 * Returns: The #GladeSupportedChild object appropriate to use for
 * container vfuncs for this child_type if this child type is supported,
 * otherwise NULL.
 *
 */
GladeSupportedChild *
glade_widget_class_get_child_support  (GladeWidgetClass *class,
				       GType             child_type)
{
	GList               *list;
	GladeSupportedChild *child, *ret = NULL;

	for (list = class->children; list && list->data; list = list->next)
	{
		child = list->data;
		if (g_type_is_a (child_type, child->type))
		{
			if (ret == NULL)
				ret = child;
			else if (g_type_depth (ret->type) < 
				 g_type_depth (child->type))
				ret = child;
		}
	}
	return ret;
}


/**
 * glade_widget_class_default_params:
 * @class: a #GladeWidgetClass
 * @construct: whether to return construct params or not construct params
 * @n_params: return location if any defaults are specified for this class.
 * 
 * Returns: A list of params for use in g_object_newv ()
 */
GParameter *
glade_widget_class_default_params (GladeWidgetClass *class,
				   gboolean          construct,
				   guint            *n_params)
{
	GArray              *params;
	GObjectClass        *oclass;
	GParamSpec         **pspec;
	GladePropertyClass  *pclass;
	guint                n_props, i;

	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), NULL);
	g_return_val_if_fail (n_params != NULL, NULL);

	/* As a slight optimization, we never unref the class
	 */
	oclass = g_type_class_ref (class->type);
	pspec  = g_object_class_list_properties (oclass, &n_props);
	params = g_array_new (FALSE, FALSE, sizeof (GParameter));

	for (i = 0; i < n_props; i++)
	{
		GParameter parameter = { 0, };

		pclass = glade_widget_class_get_property_class
			(class, pspec[i]->name);
		
		/* Ignore properties based on some criteria
		 */
		if (pclass == NULL       || /* Unaccounted for in the builder */
		    pclass->set_function || /* should not be set before 
					       GladeWidget wrapper exists */
		    pclass->ignore)         /* Catalog explicitly ignores the object */
			continue;

		if (construct &&
		    (pspec[i]->flags & 
		     (G_PARAM_CONSTRUCT|G_PARAM_CONSTRUCT_ONLY)) == 0)
			continue;
		else if (!construct &&
			 (pspec[i]->flags & 
			  (G_PARAM_CONSTRUCT|G_PARAM_CONSTRUCT_ONLY)) != 0)
			continue;


		if (g_value_type_compatible (G_VALUE_TYPE (pclass->def),
					     pspec[i]->value_type) == FALSE)
		{
			g_critical ("Type mismatch on %s property of %s",
				    parameter.name, class->name);
			continue;
		}

		if (g_param_values_cmp (pspec[i], 
					pclass->def, 
					pclass->orig_def) == 0)
			continue;

		parameter.name = pspec[i]->name; /* These are not copied/freed */
		g_value_init (&parameter.value, pspec[i]->value_type);
		g_value_copy (pclass->def, &parameter.value);

		g_array_append_val (params, parameter);
	}
	g_free (pspec);

	*n_params = params->len;
	return (GParameter *)g_array_free (params, FALSE);
}

void
glade_widget_class_container_add (GladeWidgetClass *class,
				  GObject          *container,
				  GObject          *child)
{
	GladeSupportedChild *support;

	if ((support =
	     glade_widget_class_get_child_support (class, G_OBJECT_TYPE (child))) != NULL)
	{
		if (support->add) 
			support->add (container, child);
		else
			g_warning ("No add support for type %s in %s",
				   g_type_name (support->type),
				   class->name);
	}
	else
		g_warning ("No support for type %s in %s",
			   g_type_name (G_OBJECT_TYPE (child)),
			   class->name);
}

void
glade_widget_class_container_remove (GladeWidgetClass *class,
				     GObject          *container,
				     GObject          *child)
{
	GladeSupportedChild *support;

	if ((support =
	     glade_widget_class_get_child_support (class, G_OBJECT_TYPE (child))) != NULL)
	{
		if (support->remove)
			support->remove (container, child);
		else
			g_warning ("No remove support for type %s in %s",
				   g_type_name (support->type),
				   class->name);
	}
	else
		g_warning ("No support for type %s in %s",
			   g_type_name (G_OBJECT_TYPE (child)),
			   class->name);
}

gboolean
glade_widget_class_container_has_child (GladeWidgetClass *class,
					GObject          *container,
					GObject          *child)
{
	GList *list, *children = NULL;
	gboolean found = FALSE;

	for (list = class->children; list && list->data; list = list->next)
	{
		GladeSupportedChild *support = list->data;
		if (support->get_children)
			children = g_list_concat (children, support->get_children (container));
	}

	for (list = children; list && list->data; list = list->next)
	{
		if (list->data == child)
		{
			found = TRUE;
			break;
		}
	}
	g_list_free (children);
	return found;
}

GList *
glade_widget_class_container_get_children (GladeWidgetClass *class,
					   GObject          *container)
{
	GList *list, *children = NULL;

	for (list = class->children; list && list->data; list = list->next)
	{
		GladeSupportedChild *support = list->data;
		if (support->get_children)
			children = g_list_concat
				(children, support->get_children (container));
	}

	/* Remove duplicates */
	return glade_util_purify_list (children);
}

void
glade_widget_class_container_set_property (GladeWidgetClass *class,
					   GObject      *container,
					   GObject      *child,
					   const gchar  *property_name,
					   const GValue *value)
{
	GladeSupportedChild *support;

	if ((support =
	     glade_widget_class_get_child_support (class, G_OBJECT_TYPE (child))) != NULL)
	{
		if (support->set_property)
			support->set_property (container, child,
					       property_name, value);
		else
			g_warning ("No set_property support for type %s in %s",
				   g_type_name (support->type), class->name);
	}
	else
		g_warning ("No support for type %s in %s",
			   g_type_name (G_OBJECT_TYPE (child)),
			   class->name);
}


void
glade_widget_class_container_get_property (GladeWidgetClass *class,
					   GObject      *container,
					   GObject      *child,
					   const gchar  *property_name,
					   GValue       *value)
{
	GladeSupportedChild *support;

	if ((support =
	     glade_widget_class_get_child_support (class, G_OBJECT_TYPE (child))) != NULL)
	{
		if (support->get_property)
			support->get_property (container, child,
					       property_name, value);
		else
			g_warning ("No get_property support for type %s in %s",
				   g_type_name (support->type), class->name);
	}
	else
		g_warning ("No support for type %s in %s",
			   g_type_name (G_OBJECT_TYPE (child)),
			   class->name);
}

void
glade_widget_class_container_replace_child (GladeWidgetClass *class,
					    GObject      *container,
					    GObject      *old,
					    GObject      *new)
{
	GladeSupportedChild *support;

	if ((support =
	     glade_widget_class_get_child_support (class, G_OBJECT_TYPE (old))) != NULL)
	{
		if (support->replace_child)
			support->replace_child (container, old, new);
		else
			g_warning ("No replace_child support for type %s in %s",
				   g_type_name (support->type), class->name);
	}
	else
		g_warning ("No support for type %s in %s",
			   g_type_name (G_OBJECT_TYPE (old)), class->name);
}

gboolean
glade_widget_class_contains_extra (GladeWidgetClass *class)
{
	GList *list;
	for (list = class->children; list && list->data; list = list->next)
	{
		GladeSupportedChild *support = list->data;
		if (support->type != GTK_TYPE_WIDGET)
			return TRUE;
	}
	return FALSE;
}

static GladeChildPacking *
glade_widget_class_get_child_packing (GladeWidgetClass *child_class,
				      GladeWidgetClass *parent_class)
{
	GList *l;
	
	for (l = child_class->child_packings; l; l = l->next) 
	{
		GladeChildPacking *packing;

		packing = (GladeChildPacking *) l->data;

		if (strcmp (packing->parent_name, parent_class->name) == 0)
			return packing;
	}

	return NULL;
}

static GladePackingDefault *
glade_widget_class_get_packing_default_internal (GladeChildPacking *packing,
						 const gchar       *id)
{
	GList *l;

	for (l = packing->packing_defaults; l; l = l->next)
	{
		GladePackingDefault *def;

		def = (GladePackingDefault *) l->data;

		if (strcmp (def->id, id) == 0) 
			return def;
	}

	return NULL;
}

GladePackingDefault *
glade_widget_class_get_packing_default (GladeWidgetClass *child_class,
					GladeWidgetClass *container_class,
					const gchar      *id)
{
	GladeChildPacking *packing = NULL;
	GladeWidgetClass  *p_class;
	GType              p_type;

	p_type = container_class->type;
	p_class = container_class;
	while (p_class)
	{
		GType old_p_type;

		packing = glade_widget_class_get_child_packing (child_class,
								p_class);
		if (packing)
		{
			GladePackingDefault *def;

			def = glade_widget_class_get_packing_default_internal (packing, id);
			if (def)
				return def;
		}

		old_p_type = p_type;
		p_type = g_type_parent (p_type);

		if (!p_type)
			break;
		
		p_class = glade_widget_class_get_by_type (p_type);
	}

	return NULL;
}

/**
 * glade_widget_class_query:
 * @class: A #GladeWidgetClass
 *
 * Returns: whether the user needs to be queried for
 * certain properties upon creation of this class.
 */
gboolean
glade_widget_class_query (GladeWidgetClass *class)
{
	GladePropertyClass *pclass;
	GList *l;

	for (l = class->properties; l; l = l->next)
	{
		pclass = l->data;

		if (pclass->query)
			return TRUE;
	}

	return FALSE;
}

/**
 * glade_widget_class_create_widget:
 * @class: a #GladeWidgetClass
 * @query: whether to display query dialogs if
 *         applicable to the class
 * @...: a %NULL terminated list of string/value pairs of #GladeWidget 
 *       properties
 *
 *
 * This factory function returns a new #GladeWidget of the correct type/class
 * with the properties defined in @... and queries the user if nescisary.
 *
 * The resulting object will have all default properties applied to it
 * including the overrides specified in the catalog, unless the catalog
 * has specified 'ignore' for that property.
 *
 * Note that the widget class must be fed twice; once as the
 * leading arg... and also as the property for the #GladeWidget
 *
 * this macro returns the newly created #GladeWidget
 */
GladeWidget *
glade_widget_class_create_widget_real (gboolean          query, 
				       const gchar      *first_property,
				       ...)
{
	GladeWidgetClass *widget_class;
	GType             gwidget_type;
	GladeWidget      *gwidget;
	va_list           vl, vl_copy;

	g_return_val_if_fail (strcmp (first_property, "class") == 0, NULL);

	va_start (vl, first_property);
	va_copy (vl_copy, vl);

	widget_class = va_arg (vl, GladeWidgetClass *);

	va_end (vl);

	if (GLADE_IS_WIDGET_CLASS (widget_class) == FALSE)
	{
		g_critical ("No class found in glade_widget_class_create_widget_real args");
		va_end (vl_copy);
		return NULL;
	}

	if (widget_class->fixed)
		gwidget_type = GLADE_TYPE_FIXED;
	else 
		gwidget_type = GLADE_TYPE_WIDGET;


	gwidget = (GladeWidget *)g_object_new_valist (gwidget_type,
						      first_property, 
						      vl_copy);
	va_end (vl_copy);
	
	if (query && glade_widget_class_query (widget_class))
	{
		GladeEditor *editor = glade_app_get_editor ();
		
		/* If user pressed cancel on query popup. */
		if (!glade_editor_query_dialog (editor, gwidget))
		{
			g_object_unref (G_OBJECT (gwidget));
			return NULL;
		}
	}

	return gwidget;
}



/**
 * glade_widget_class_create_internal:
 * @parent:            The parent #GladeWidget, or %NULL for children
 *                     outside of the hierarchy.
 * @internal_object:   the #GObject
 * @internal_name:     a string identifier for this internal widget.
 * @anarchist:         Whether or not this widget is a widget outside
 *                     of the parent's hierarchy (like a popup window)
 * @reason:            The #GladeCreateReason for which this internal widget
 *                     was created (usually just pass the reason from the post_create
 *                     function; note also this is used only by the plugin code so
 *                     pass something usefull here).
 *
 * A convenienve function to create a #GladeWidget of the prescribed type
 * for internal widgets.
 *
 * Returns: a freshly created #GladeWidget wrapper object for the
 *          @internal_object of name @internal_name
 */
GladeWidget *
glade_widget_class_create_internal (GladeWidget      *parent,
				    GObject          *internal_object,
				    const gchar      *internal_name,
				    const gchar      *parent_name,
				    gboolean          anarchist,
				    GladeCreateReason reason)
{
	GladeWidgetClass *class;
	GladeProject     *project;

	g_return_val_if_fail (GLADE_IS_WIDGET (parent), NULL);
	project = glade_widget_get_project (parent);

        if ((class = glade_widget_class_get_by_name 
	     (G_OBJECT_TYPE_NAME (internal_object))) == NULL)
	{
		g_critical ("Unable to find widget class for type %s", 
			    G_OBJECT_TYPE_NAME (internal_object));
		return NULL;
	}

	return glade_widget_class_create_widget (class, FALSE,
						 "anarchist", anarchist,
						 "parent", parent,
						 "project", project,
						 "internal", internal_name,
						 "internal-name", parent_name,
						 "reason", reason,
						 "object", internal_object,
						 NULL);
}
