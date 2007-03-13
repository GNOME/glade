/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 Ximian, Inc.
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
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#include <config.h>

#include "glade.h"
#include "glade-widget-adaptor.h"
#include "glade-xml-utils.h"
#include "glade-property-class.h"
#include "glade-signal.h"
#include "glade-marshallers.h"
#include "glade-accumulators.h"
#include "glade-binding.h"

/* For g_file_exists */
#include <sys/types.h>
#include <string.h>

#include <glib/gdir.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <ctype.h>

#define LARGE_ICON_SUBDIR "22x22"
#define LARGE_ICON_SIZE 22
#define SMALL_ICON_SUBDIR "16x16"
#define SMALL_ICON_SIZE 16

struct _GladeWidgetAdaptorPriv {

	gchar       *catalog;      /* The name of the widget catalog this class
				    * was declared by.
				    */

	gchar       *book;         /* DevHelp search namespace for this widget class
				    */

	GdkPixbuf   *large_icon;   /* The 22x22 icon for the widget */
	GdkPixbuf   *small_icon;   /* The 16x16 icon for the widget */

	GdkCursor   *cursor;       /* a cursor for inserting widgets */

	gchar       *special_child_type; /* Special case code for children that
					  * are special children (like notebook tab 
					  * widgets for example).
					  */
};

struct _GladeChildPacking {
	gchar *parent_name;
	GList *packing_defaults;
};

enum {
	PROP_0,
	PROP_NAME,
	PROP_TYPE,
	PROP_TITLE,
	PROP_GENERIC_NAME,
	PROP_CATALOG,
	PROP_BOOK,
	PROP_SPECIAL_TYPE,
	PROP_SMALL_ICON,
	PROP_LARGE_ICON,
	PROP_CURSOR
};

enum
{
	SIGNAL_ACTION_ACTIVATED,
	LAST_SIGNAL
};

typedef struct _GladeChildPacking GladeChildPacking;

static guint gwa_signals [LAST_SIGNAL] = {0,};

static GObjectClass *parent_class = NULL;
static GHashTable   *adaptor_hash = NULL;

/*******************************************************************************
                              Helper functions
 *******************************************************************************/
static GladeWidgetAdaptor *
gwa_get_parent_adaptor (GladeWidgetAdaptor *adaptor)
{
	GladeWidgetAdaptor *parent_adaptor = NULL;
	GType               iter_type;

	for (iter_type = g_type_parent (adaptor->type);
	     iter_type > 0;
	     iter_type = g_type_parent (iter_type))
	{
		if ((parent_adaptor = 
		     glade_widget_adaptor_get_by_type (iter_type)) != NULL)
			return parent_adaptor;
	}

	return NULL;
}

/*
  This function assignes "weight" to each property in its natural order staring from 1.
  If parent is 0 weight will be set for every GladePropertyClass in the list.
  This function will not override weight if it is already set (weight >= 0.0)
*/
static void
gwa_properties_set_weight (GList **properties, GType parent)
{
	gint normal = 0, common = 0, packing = 0;
	GList *l;

	for (l = *properties; l && l->data; l = g_list_next (l))
	{
		GladePropertyClass *klass = l->data;
		GPCType type = klass->type;
	
		if (klass->visible &&
		    (parent) ? parent == klass->pspec->owner_type : TRUE &&
	    	    (type == GPC_NORMAL || type == GPC_ACCEL_PROPERTY))
		{
			/* Use a different counter for each tab (common, packing and normal) */
			if (klass->common) common++;
			else if (klass->packing) packing++;
			else normal++;

			/* Skip if it is already set */
			if (klass->weight >= 0.0) continue;
			
			/* Special-casing weight of properties for seperate tabs */
			if (klass->common) klass->weight = common;
			else if (klass->packing) klass->weight = packing;
			else klass->weight = normal;
		}
	}
}

static void
gwa_load_icons (GladeWidgetAdaptor *adaptor)
{
	GError    *error = NULL;
	gchar     *icon_path;

	/* only certain widget classes need to have icons */
	if (G_TYPE_IS_INSTANTIATABLE (adaptor->type) == FALSE ||
            G_TYPE_IS_ABSTRACT (adaptor->type) != FALSE ||
            adaptor->generic_name == NULL)
		return;

	/* load large 22x22 icon */
	icon_path = g_strdup_printf ("%s" G_DIR_SEPARATOR_S
				     LARGE_ICON_SUBDIR
				     G_DIR_SEPARATOR_S "%s.png", 
				     glade_app_get_pixmaps_dir (), 
				     adaptor->generic_name);

	adaptor->priv->large_icon = 
		gdk_pixbuf_new_from_file_at_size (icon_path, 
						  LARGE_ICON_SIZE, 
						  LARGE_ICON_SIZE, 
						  &error);
	
	if (adaptor->priv->large_icon == NULL)
	{
		g_warning (_("Unable to load icon for %s (%s)"), 
			   adaptor->name, error->message);

		g_error_free (error);
		error = NULL;

		/* use stock missing icon */
		adaptor->priv->large_icon = 
			gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), 
						  GTK_STOCK_MISSING_IMAGE, 
						  LARGE_ICON_SIZE, 
						  GTK_ICON_LOOKUP_USE_BUILTIN, 
						  &error);
		if (adaptor->priv->large_icon == NULL)
		{
			g_critical (_("Unable to load stock icon (%s)"),
				    error->message);

			g_error_free (error);
			error = NULL;
		}
	}
	g_free (icon_path);


	/* load small 16x16 icon */
	icon_path = g_strdup_printf ("%s" G_DIR_SEPARATOR_S 
				     SMALL_ICON_SUBDIR
				     G_DIR_SEPARATOR_S "%s.png", 
				     glade_app_get_pixmaps_dir (), 
				     adaptor->generic_name);

	adaptor->priv->small_icon = 
		gdk_pixbuf_new_from_file_at_size (icon_path, 
						  SMALL_ICON_SIZE, 
						  SMALL_ICON_SIZE, 
						  &error);
	
	if (adaptor->priv->small_icon == NULL)
	{
		g_warning (_("Unable to load icon for %s (%s)"), 
			   adaptor->name, error->message);

		g_error_free (error);
		error = NULL;

		/* use stock missing icon */
		adaptor->priv->small_icon = 
			gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), 
						  GTK_STOCK_MISSING_IMAGE, 
						  SMALL_ICON_SIZE, 
						  GTK_ICON_LOOKUP_USE_BUILTIN, 
						  &error);
		if (adaptor->priv->small_icon == NULL)
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
gwa_create_cursor (GladeWidgetAdaptor *adaptor)
{
	GdkPixbuf *tmp_pixbuf;
	const GdkPixbuf *add_pixbuf;
	GdkDisplay *display;

	/* only certain widget classes need to have cursors */
	if (G_TYPE_IS_INSTANTIATABLE (adaptor->type) == FALSE ||
            G_TYPE_IS_ABSTRACT (adaptor->type) != FALSE ||
            adaptor->generic_name == NULL)
		return;

	/* cannot continue if 'add widget' cursor pixbuf has not been loaded for any reason */
	if ((add_pixbuf = glade_cursor_get_add_widget_pixbuf ()) == NULL)
		return;

	display = gdk_display_get_default ();

	/* create a temporary pixbuf clear to transparent black*/
	tmp_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 32, 32);
	gdk_pixbuf_fill (tmp_pixbuf, 0x00000000);


	/* composite pixbufs */
	gdk_pixbuf_composite (adaptor->priv->large_icon, tmp_pixbuf,
			      8, 8, 22, 22,
			      8, 8, 1, 1,
                              GDK_INTERP_NEAREST, 255);

	gdk_pixbuf_composite (add_pixbuf, tmp_pixbuf,
			      0, 0, 12, 12,
			      0, 0, 1, 1,
                              GDK_INTERP_NEAREST, 255);


	adaptor->priv->cursor = gdk_cursor_new_from_pixbuf (display, tmp_pixbuf, 6, 6);

	g_object_unref(tmp_pixbuf);
}



static gboolean
gwa_gtype_equal (gconstpointer v1,
		 gconstpointer v2)
{
  return *((const GType*) v1) == *((const GType*) v2);
}

static guint
gwa_gtype_hash (gconstpointer v)
{
  return *(const GType*) v;
}

static gboolean
glade_widget_adaptor_hash_find (gpointer key, gpointer value, gpointer user_data)
{
	GladeWidgetAdaptor *adaptor = value;
	GType *type = user_data;
	
	if (g_type_is_a (adaptor->type, *type))
	{
		*type = adaptor->type;
		return TRUE;
	}
		
	return FALSE;
}

static void
glade_abort_if_derived_adaptors_exist (GType type)
{
	if (adaptor_hash)
	{
		GType retval = type;
	
		g_hash_table_find (adaptor_hash,
				   glade_widget_adaptor_hash_find,
				   &retval);
		if (retval != type)
			g_error (_("A derived adaptor (%s) of %s already exist!"),
				 g_type_name (retval), g_type_name (type));
	}
}

/*******************************************************************************
                     Base Object Implementation detail
 *******************************************************************************/
static gint
gwa_signal_comp (gconstpointer a, gconstpointer b)
{
	const GladeSignalClass *signal_a = a, *signal_b = b;	
	return strcmp (signal_b->query.signal_name, signal_a->query.signal_name);
}

static void
gwa_add_signals (GList **signals, GType type)
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
		
		list = g_list_sort (list, gwa_signal_comp);
		*signals = g_list_concat (list, *signals);
	}
}

static GList * 
gwa_list_signals (GladeWidgetAdaptor *adaptor) 
{
	GList *signals = NULL;
	GType type, parent, *i, *p;

	g_return_val_if_fail (adaptor->type != 0, NULL);

	for (type = adaptor->type; g_type_is_a (type, G_TYPE_OBJECT); type = parent)
	{
		parent = g_type_parent (type);
		
		/* Add class signals */
		gwa_add_signals (&signals, type);
	
		/* Add class interfaces signals */
		for (i = p = g_type_interfaces (type, NULL); *i; i++)
			if (!glade_util_class_implements_interface (parent, *i))
				gwa_add_signals (&signals, *i);

		g_free (p);
	}

	return g_list_reverse (signals);
}

static GList * 
gwa_clone_parent_properties (GladeWidgetAdaptor *adaptor, gboolean is_packing) 
{
	GladeWidgetAdaptor *parent_adaptor;
	GList              *properties = NULL, *list, *proplist;
	
	if ((parent_adaptor = gwa_get_parent_adaptor (adaptor)) != NULL)
	{
		proplist = is_packing ? 
			parent_adaptor->packing_props : 
			parent_adaptor->properties;

		for (list = proplist; list; list = list->next)
		{
			GladePropertyClass *pclass =
				glade_property_class_clone (list->data);
			properties = g_list_prepend (properties, pclass);
		}
	}

	return g_list_reverse (properties);
}

static void
gwa_setup_introspected_props_from_pspecs (GladeWidgetAdaptor   *adaptor,
					  GParamSpec          **specs,
					  gint                  n_specs,
					  gboolean              is_packing)
{
	GladeWidgetAdaptor *parent_adaptor = gwa_get_parent_adaptor (adaptor);
	GladePropertyClass *property_class;
	GType               class_type;
	gint                i;
	GList              *list = NULL;

	for (i = 0; i < n_specs; i++)
	{
		gboolean found;

		/* Only create properties that dont exist on the adaptor yet */
		for (found = FALSE, class_type = adaptor->type;
		     ((!parent_adaptor && class_type != 0) ||
		      ( parent_adaptor && class_type != parent_adaptor->type));
		     class_type = g_type_parent (class_type))
			if (specs[i]->owner_type == class_type ||
			    (G_TYPE_IS_INTERFACE (specs[i]->owner_type) &&
			    glade_util_class_implements_interface (class_type, specs[i]->owner_type)))
			{
				found = TRUE;
				break;
			}

		if (found && 
		    (property_class = 
		     glade_property_class_new_from_spec (adaptor, specs[i])) != NULL)
			list = g_list_prepend (list, property_class);
	}

	if (is_packing)
		adaptor->packing_props =
			g_list_concat (adaptor->packing_props, 
				       g_list_reverse (list));
	else
		adaptor->properties =
			g_list_concat (adaptor->properties, 
				       g_list_reverse (list));
}

/* XXX Atk relations and accel props disregarded - they should
 *     be implemented from the gtk+ catalog instead I think.
 */
static void
gwa_setup_properties (GladeWidgetAdaptor *adaptor,
		      GObjectClass       *object_class,
		      gboolean            is_packing) 
{
	GParamSpec   **specs = NULL;
	guint          n_specs = 0;
	GList         *l;

	/* only GtkContainer child propeties can be introspected */
	if (is_packing && !g_type_is_a (adaptor->type, GTK_TYPE_CONTAINER))
		return;

	/* First clone the parents properties */
	if (is_packing)
		adaptor->packing_props = gwa_clone_parent_properties (adaptor, is_packing);
	else
		adaptor->properties = gwa_clone_parent_properties (adaptor, is_packing);

	/* Now automaticly introspect new properties added in this class,
	 * allow the class writer to decide what to override in the resulting
	 * list of properties.
	 */
	if (is_packing)
		specs = gtk_container_class_list_child_properties (object_class, &n_specs);
	else
		specs = g_object_class_list_properties (object_class, &n_specs);
	gwa_setup_introspected_props_from_pspecs (adaptor, specs, n_specs, is_packing);
	g_free (specs);

	if (is_packing)
	{
		/* We have to mark packing properties from GladeWidgetAdaptor
		 * because GladePropertyClass doesnt have a valid parent GType
		 * to introspect it.
		 *
		 * (which could be used to call gtk_container_class_find_child_property()
		 * and properly introspect whether or not its a packing property).
		 */
		for (l = adaptor->packing_props; l; l = l->next)
		{
			GladePropertyClass *property_class = l->data;
			property_class->packing = TRUE;
		}
	}
}

static GList *
gwa_inherit_child_packing (GladeWidgetAdaptor *adaptor)
{
	GladeWidgetAdaptor *parent_adaptor;
	GList              *child_packings = NULL, *packing_list, *default_list;

	if ((parent_adaptor = gwa_get_parent_adaptor (adaptor)) != NULL)
	{
		for (packing_list = parent_adaptor->child_packings;
		     packing_list; packing_list = packing_list->next)
		{
			GladeChildPacking *packing     = packing_list->data;
			GladeChildPacking *packing_dup = g_new0 (GladeChildPacking, 1);

			packing_dup->parent_name = g_strdup (packing->parent_name);

			for (default_list = packing->packing_defaults;
			     default_list; default_list = default_list->next)
			{
				GladePackingDefault *def     = default_list->data;
				GladePackingDefault *def_dup = g_new0 (GladePackingDefault, 1);

				def_dup->id    = g_strdup (def->id);
				def_dup->value = g_strdup (def->value);

				packing_dup->packing_defaults = 
					g_list_prepend (packing_dup->packing_defaults, def_dup);
			}

			child_packings = g_list_prepend (child_packings, packing_dup);
		}
	}
	return child_packings;
}

static GList *
gwa_action_copy (GList *src)
{
	GList *l, *list = NULL;
	
	for (l = src; l; l = g_list_next (l))
	{
		GWAAction *action = l->data, *copy = g_new0 (GWAAction, 1);
	
		copy->id = g_strdup (action->id);
		copy->label = g_strdup (action->label);
		copy->stock = g_strdup (action->stock);
		copy->is_a_group = action->is_a_group;
		
		list = g_list_append (list, l->data);
		
		if (action->actions)
			copy->actions = gwa_action_copy (action->actions);
	}
	
	return list;
}

static void
gwa_action_setup (GladeWidgetAdaptor *adaptor)
{
	GladeWidgetAdaptor *parent = gwa_get_parent_adaptor (adaptor);
	
	if (parent && parent->actions)
		adaptor->actions = gwa_action_copy (parent->actions);
	else
		adaptor->actions = NULL;
}

static GObject *
glade_widget_adaptor_constructor (GType                  type,
				  guint                  n_construct_properties,
				  GObjectConstructParam *construct_properties)
{
	GladeWidgetAdaptor   *adaptor, *parent_adaptor;
	GObject              *ret_obj;
	GObjectClass         *object_class;

	glade_abort_if_derived_adaptors_exist (type);
	
	ret_obj = G_OBJECT_CLASS (parent_class)->constructor
		(type, n_construct_properties, construct_properties);

	adaptor = GLADE_WIDGET_ADAPTOR (ret_obj);
	parent_adaptor = gwa_get_parent_adaptor (adaptor);
	
	if (adaptor->type == G_TYPE_NONE)
		g_warning ("Adaptor created without a type");
	if (adaptor->name == NULL)
		g_warning ("Adaptor created without a name");

	/* Build decorations */
	gwa_load_icons (adaptor);
	gwa_create_cursor (adaptor);

	/* Let it leek */
	if ((object_class = g_type_class_ref (adaptor->type)))
	{
		/* Build signals & properties */
		adaptor->signals = gwa_list_signals (adaptor);
		gwa_setup_properties (adaptor, object_class, FALSE);
		gwa_setup_properties (adaptor, object_class, TRUE);
	}
	else
		g_critical ("Failed to get class for type %s\n", 
			    g_type_name (adaptor->type));

	/* Inherit packing defaults here */
	adaptor->child_packings = gwa_inherit_child_packing (adaptor);

	/* Inherit special-child-type */
	if (parent_adaptor)
		adaptor->priv->special_child_type =
			parent_adaptor->priv->special_child_type ?
			g_strdup (parent_adaptor->priv->special_child_type) : NULL;
	
	gwa_action_setup (adaptor);
	
	return ret_obj;
}

static void
gwa_packing_default_free (GladePackingDefault *def)
{
	g_free (def->id);
	g_free (def->value);
}

static void
gwa_child_packing_free (GladeChildPacking *packing)
{
        g_free (packing->parent_name);

        g_list_foreach (packing->packing_defaults,
                        (GFunc) gwa_packing_default_free, NULL);
        g_list_free (packing->packing_defaults);
}

static  void
gwa_actions_free (GList *actions)
{
	GList *l;
	
	for (l = actions; l; l = g_list_next (l))
	{
		GWAAction *action = l->data;

		if (action->actions) gwa_actions_free (action->actions);
		
		g_free (action->id);
		g_free (action->label);
		g_free (action->stock);
		g_free (action);		
	}
	g_list_free (actions);
}

static void
glade_widget_adaptor_finalize (GObject *object)
{
	GladeWidgetAdaptor *adaptor = GLADE_WIDGET_ADAPTOR (object);

	/* Free properties and signals */
	g_list_foreach (adaptor->properties, (GFunc) glade_property_class_free, NULL);
	g_list_free (adaptor->properties);

	g_list_foreach (adaptor->packing_props, (GFunc) glade_property_class_free, NULL);
	g_list_free (adaptor->packing_props);

	g_list_foreach (adaptor->signals, (GFunc) glade_signal_free, NULL);
	g_list_free (adaptor->signals);


	/* Free child packings */
	g_list_foreach (adaptor->child_packings,
			(GFunc) gwa_child_packing_free,
			NULL);
	g_list_free (adaptor->child_packings);

	if (adaptor->priv->book)       g_free (adaptor->priv->book);
	if (adaptor->priv->catalog)    g_free (adaptor->priv->catalog);
	if (adaptor->priv->special_child_type)
		g_free (adaptor->priv->special_child_type);

	if (adaptor->priv->cursor != NULL)
		gdk_cursor_unref (adaptor->priv->cursor);

	if (adaptor->priv->large_icon != NULL)
		g_object_unref (G_OBJECT (adaptor->priv->large_icon));

	if (adaptor->priv->small_icon != NULL)
		g_object_unref (G_OBJECT (adaptor->priv->small_icon));

	if (adaptor->name)         g_free (adaptor->name);
	if (adaptor->generic_name) g_free (adaptor->generic_name);
	if (adaptor->title)        g_free (adaptor->title);

	if (adaptor->actions)      gwa_actions_free (adaptor->actions);
	
	g_free (adaptor->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
glade_widget_adaptor_real_set_property (GObject         *object,
					guint            prop_id,
					const GValue    *value,
					GParamSpec      *pspec)
{
	GladeWidgetAdaptor *adaptor;

	adaptor = GLADE_WIDGET_ADAPTOR (object);

	switch (prop_id)
	{
	case PROP_NAME:
		/* assume once (construct-only) */
		adaptor->name = g_value_dup_string (value);
		break;
	case PROP_TYPE:
		adaptor->type = g_value_get_gtype (value);
		break;
	case PROP_TITLE:
		if (adaptor->title) g_free (adaptor->title);
		adaptor->title = g_value_dup_string (value);
		break;
	case PROP_GENERIC_NAME:
		if (adaptor->generic_name) g_free (adaptor->generic_name);
		adaptor->generic_name = g_value_dup_string (value);
		break;
	case PROP_CATALOG:
		/* assume once (construct-only) */
		adaptor->priv->catalog = g_value_dup_string (value);
		break;
	case PROP_BOOK:
		/* assume once (construct-only) */
		adaptor->priv->book = g_value_dup_string (value);
		break;
	case PROP_SPECIAL_TYPE:
		/* assume once (construct-only) */
		adaptor->priv->special_child_type = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_widget_adaptor_real_get_property (GObject         *object,
					guint            prop_id,
					GValue          *value,
					GParamSpec      *pspec)
{

	GladeWidgetAdaptor *adaptor;

	adaptor = GLADE_WIDGET_ADAPTOR (object);

	switch (prop_id)
	{
	case PROP_NAME:         g_value_set_string (value, adaptor->name);          break;
	case PROP_TYPE:	        g_value_set_gtype  (value, adaptor->type);          break;
	case PROP_TITLE:        g_value_set_string (value, adaptor->title);         break;
	case PROP_GENERIC_NAME: g_value_set_string (value, adaptor->generic_name);  break;
	case PROP_CATALOG:      g_value_set_string (value, adaptor->priv->catalog); break;
	case PROP_BOOK:         g_value_set_string (value, adaptor->priv->book);    break;
	case PROP_SPECIAL_TYPE: 
		g_value_set_string (value, adaptor->priv->special_child_type); 
		break;
	case PROP_SMALL_ICON: g_value_set_object  (value, adaptor->priv->small_icon); break;
	case PROP_LARGE_ICON: g_value_set_object  (value, adaptor->priv->large_icon); break;
	case PROP_CURSOR:     g_value_set_pointer (value, adaptor->priv->cursor);     break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/*******************************************************************************
                  GladeWidgetAdaptor base class implementations
 *******************************************************************************/
static void
glade_widget_adaptor_object_set_property (GladeWidgetAdaptor *adaptor,
					  GObject            *object,
					  const gchar        *property_name,
					  const GValue       *value)
{
	g_object_set_property (object, property_name, value);
}

static void
glade_widget_adaptor_object_get_property (GladeWidgetAdaptor *adaptor,
					  GObject            *object,
					  const gchar        *property_name,
					  GValue             *value)
{
	g_object_get_property (object, property_name, value);
}


/*******************************************************************************
            GladeWidgetAdaptor type registration and class initializer
 *******************************************************************************/
static void
glade_widget_adaptor_init (GladeWidgetAdaptor *adaptor)
{
	adaptor->priv = g_new0 (GladeWidgetAdaptorPriv, 1);
}

static gboolean
gwa_action_activated_impl (GladeWidgetAdaptor *adaptor,
			   GladeWidget *widget,
			   const gchar *action_id)
{
	static guint signal = 0;
	gboolean retval;
	
	if (signal == 0)
		signal = g_signal_lookup ("action-activated", GLADE_TYPE_WIDGET);
	
	g_signal_emit (widget, signal, g_quark_from_string (action_id),
		       action_id, &retval);

	return retval;
}

static void
glade_widget_adaptor_class_init (GladeWidgetAdaptorClass *adaptor_class)
{
	GObjectClass       *object_class;
	g_return_if_fail (adaptor_class != NULL);
	
	parent_class = g_type_class_peek_parent (adaptor_class);
	object_class = G_OBJECT_CLASS (adaptor_class);

	/* GObjectClass */
	object_class->constructor           = glade_widget_adaptor_constructor;
	object_class->finalize              = glade_widget_adaptor_finalize;
	object_class->set_property          = glade_widget_adaptor_real_set_property;
	object_class->get_property          = glade_widget_adaptor_real_get_property;

	/* Class methods */
	adaptor_class->post_create          = NULL;
	adaptor_class->launch_editor        = NULL;
	adaptor_class->get_internal_child   = NULL;
	adaptor_class->verify_property      = NULL;
	adaptor_class->set_property         = glade_widget_adaptor_object_set_property;
	adaptor_class->get_property         = glade_widget_adaptor_object_get_property;
	adaptor_class->add                  = NULL;
	adaptor_class->remove               = NULL;
	adaptor_class->replace_child        = NULL;
	adaptor_class->get_children         = NULL;
	adaptor_class->child_set_property   = NULL;
	adaptor_class->child_get_property   = NULL;

	adaptor_class->action_activated     = gwa_action_activated_impl;

	/* Base defaults here */
	adaptor_class->fixed                = FALSE;
	adaptor_class->toplevel             = FALSE;
	adaptor_class->use_placeholders     = FALSE;
	adaptor_class->default_width        = -1;
	adaptor_class->default_height       = -1;

	/**
	 * GladeWidgetAdaptor::action-activated:
	 * @adaptor: the GladeWidgetAdaptor which received the signal.
	 * @widget: the action's GladeWidget or NULL.
	 * @action_id: the action id (signal detail) or NULL.
	 *
	 * Use this to catch up actions.
	 *
	 * Returns TRUE to stop others handlers being invoked.
	 *
	 */
	gwa_signals [SIGNAL_ACTION_ACTIVATED] =
		g_signal_new ("action-activated",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      G_STRUCT_OFFSET (GladeWidgetAdaptorClass, action_activated),
			      glade_boolean_handled_accumulator, NULL,
			      glade_marshal_BOOLEAN__OBJECT_STRING,
			      G_TYPE_BOOLEAN, 2, GLADE_TYPE_WIDGET, G_TYPE_STRING);	

	/* Properties */
	g_object_class_install_property 
		(object_class, PROP_NAME,
		 g_param_spec_string 
		 ("name", _("Name"), 
		  _("Name of the class"),
		  NULL, 
		  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property 
		(object_class, PROP_TYPE,
		 g_param_spec_gtype 
		 ("type", _("Type"), 
		  _("GType of the class"),
		  G_TYPE_NONE,
		  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property 
		(object_class, PROP_TITLE,
		 g_param_spec_string 
		 ("title", _("Title"), 
		  _("Translated title for the class used in the glade UI"),
		  NULL, 
		  G_PARAM_READWRITE));

	g_object_class_install_property 
		(object_class, PROP_GENERIC_NAME,
		 g_param_spec_string 
		 ("generic-name", _("Generic Name"), 
		  _("Used to generate names of new widgets"),
		  NULL, 
		  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property 
		(object_class, PROP_CATALOG,
		 g_param_spec_string 
		 ("catalog", _("Catalog"), 
		  _("The name of the widget catalog this class was declared by"),
		  NULL, 
		  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property 
		(object_class, PROP_BOOK,
		 g_param_spec_string 
		 ("book", _("Book"), 
		  _("DevHelp search namespace for this widget class"),
		  NULL, 
		  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property 
		(object_class, PROP_SPECIAL_TYPE,
		 g_param_spec_string 
		 ("special-child-type", _("Special Child Type"), 
		  _("Holds the name of the packing property to depict "
		    "special children for this container class"),
		  NULL, 
		  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property 
		(object_class, PROP_LARGE_ICON,
		 g_param_spec_object 
		 ("large-icon", _("Large Icon"), 
		  _("The 22x22 icon for this widget class"),
		  GDK_TYPE_PIXBUF, 
		  G_PARAM_READABLE));

	g_object_class_install_property 
		(object_class, PROP_SMALL_ICON,
		 g_param_spec_object 
		 ("small-icon", _("Small Icon"), 
		  _("The 16x16 icon for this widget class"),
		  GDK_TYPE_PIXBUF, 
		  G_PARAM_READABLE));

	g_object_class_install_property 
		(object_class, PROP_CURSOR,
		 g_param_spec_pointer 
		 ("cursor", _("Cursor"), 
		  _("A cursor for inserting widgets in the UI"),
		  G_PARAM_READABLE));
}

GType
glade_widget_adaptor_get_type (void)
{
	static GType adaptor_type = 0;
	
	if (!adaptor_type)
	{
		static const GTypeInfo adaptor_info = 
		{
			sizeof (GladeWidgetAdaptorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) glade_widget_adaptor_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,           /* class_data */
			sizeof (GladeWidgetAdaptor),
			0,              /* n_preallocs */
			(GInstanceInitFunc) glade_widget_adaptor_init,
		};
		adaptor_type = 
			g_type_register_static (G_TYPE_OBJECT,
						"GladeGObjectAdaptor",
						&adaptor_info, 0);
	}
	return adaptor_type;
}

GType
glade_create_reason_get_type (void)
{
	static GType etype = 0;

	if (etype == 0)
	{
		static const GEnumValue values[] = {
			{ GLADE_CREATE_USER,    "GLADE_CREATE_USER",    "create-user" },
			{ GLADE_CREATE_COPY,    "GLADE_CREATE_COPY",    "create-copy" },
			{ GLADE_CREATE_LOAD,    "GLADE_CREATE_LOAD",    "create-load" },
			{ GLADE_CREATE_REBUILD, "GLADE_CREATE_REBUILD", "create-rebuild" },
			{ 0, NULL, NULL }
		};

		etype = g_enum_register_static ("GladeCreateReason", values);

	}

	return etype;
}

/*******************************************************************************
                        Synthetic Object Derivation
 *******************************************************************************/
static void
gwa_derived_init (GladeWidgetAdaptor *adaptor)
{

}

static void
gwa_derived_class_init (GladeWidgetAdaptorClass *adaptor_class)
{

}

static GType
gwa_derive_adaptor_for_type (GType object_type)
{
	GladeWidgetAdaptor *adaptor;
	GType               iter_type, derived_type;
	GType               parent_type = GLADE_TYPE_WIDGET_ADAPTOR;
	gchar              *type_name;

	for (iter_type = g_type_parent (object_type); 
	     iter_type > 0; 
	     iter_type = g_type_parent (iter_type))
	{
		if ((adaptor = 
		     glade_widget_adaptor_get_by_type (iter_type)) != NULL)
		{
			parent_type = G_TYPE_FROM_INSTANCE (adaptor);
			break;
		}
	}

	/* Note that we must pass ownership of the type_name string
	 * to the type system 
	 */
	type_name    = g_strdup_printf ("Glade%sAdaptor", g_type_name (object_type));
	derived_type = 
		g_type_register_static_simple (parent_type, type_name,
					       sizeof (GladeWidgetAdaptorClass),
					       (GClassInitFunc)gwa_derived_class_init,
					       sizeof (GladeWidgetAdaptor),
					       (GInstanceInitFunc)gwa_derived_init, 0);
	return derived_type;
}


/*******************************************************************************
                                     API
 *******************************************************************************/

/**
 * glade_widget_adaptor_register:
 * @adaptor: A #GladeWidgetAdaptor
 *
 * Registers @adaptor into the Glade core (every supported
 * object type must have a registered adaptor).
 */
void
glade_widget_adaptor_register (GladeWidgetAdaptor *adaptor)
{

	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));

	if (glade_widget_adaptor_get_by_name (adaptor->name))
	{
		g_warning ("Adaptor class for '%s' already registered", 
			   adaptor->name);
		return;
	}

	if (!adaptor_hash)
		adaptor_hash = g_hash_table_new_full (gwa_gtype_hash, gwa_gtype_equal,
						      g_free, g_object_unref);

	g_hash_table_insert (adaptor_hash, 
			     g_memdup (&adaptor->type, 
				       sizeof (GType)), adaptor);
}

static GladePackingDefault *
gwa_default_from_child_packing (GladeChildPacking *packing, const gchar *id)
{
	GList *list;

	for (list = packing->packing_defaults; list; list = list->next)
	{
		GladePackingDefault *def = list->data;

		if (id && !strcmp (id, def->id))
			return def;
	}

	return NULL;
}

static GladeChildPacking *
glade_widget_adaptor_get_child_packing (GladeWidgetAdaptor *child_adaptor,
					const gchar        *parent_name)
{
	GList *l;

	for (l = child_adaptor->child_packings; l; l = l->next) 
	{
		GladeChildPacking *packing;

		packing = (GladeChildPacking *) l->data;

		if (!strcmp (packing->parent_name, parent_name))
			return packing;
	}

	return NULL;
}

static void
gwa_set_packing_defaults_from_node (GladeWidgetAdaptor *adaptor,
				    GladeXmlNode       *node)
{
        GladeXmlNode *child;

        for (child = glade_xml_node_get_children (node); 
	     child; child = glade_xml_node_next (child))
        {
                gchar             *name;
                GladeXmlNode      *prop_node;
		GladeChildPacking *packing;

                if (!glade_xml_node_verify (child, GLADE_TAG_PARENT_CLASS))
                        continue;

                if ((name = glade_xml_get_property_string_required
		     (child, GLADE_TAG_NAME, adaptor->name)) == NULL)
                        continue;

		/* If a GladeChildPacking exists for this parent, use it -
		 * otherwise prepend a new one
		 */
		if ((packing = 
		     glade_widget_adaptor_get_child_packing (adaptor, name)) == NULL)
		{

			packing = g_new0 (GladeChildPacking, 1);
			packing->parent_name = name;

			adaptor->child_packings = 
				g_list_prepend (adaptor->child_packings, packing);

		}

                for (prop_node = glade_xml_node_get_children (child); 
		     prop_node; prop_node = glade_xml_node_next (prop_node))
                {
			GladePackingDefault *def;
			gchar               *id;
			gchar               *value;

			if ((id = 
			     glade_xml_get_property_string_required
			     (prop_node, GLADE_TAG_ID, adaptor->name)) == NULL)
				continue;

			if ((value = 
			     glade_xml_get_property_string_required
			     (prop_node, GLADE_TAG_DEFAULT, adaptor->name)) == NULL)
			{
				g_free (id);
				continue;
			}

			if ((def = gwa_default_from_child_packing (packing, id)) == NULL)
			{
				def        = g_new0 (GladePackingDefault, 1);
				def->id    = id;
				def->value = value;

				packing->packing_defaults = 
					g_list_prepend (packing->packing_defaults, def);
			}
			else
			{
				g_free (id);
				g_free (def->value);
				def->value = value;
			}

			adaptor->child_packings = 
				g_list_prepend (adaptor->child_packings, packing);

                }
	}
}

static void
gwa_update_properties_from_node (GladeWidgetAdaptor  *adaptor,
				 GladeXmlNode        *node,
				 GModule             *module,
				 GList              **properties,
				 const gchar         *domain,
				 gboolean             is_packing)
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
			(child, GLADE_TAG_ID, adaptor->name);
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
			property_class = glade_property_class_new (adaptor);
			property_class->id = g_strdup (id);

			/* When creating new virtual packing properties,
			 * make sure we mark them as such here. 
			 */
			if (is_packing)
				property_class->packing = TRUE;

			*properties = g_list_append (*properties, property_class);
			list = g_list_last (*properties);
		}

		if ((updated = glade_property_class_update_from_node
		     (child, module, adaptor->type,
		      &property_class, domain)) == FALSE)
		{
			g_warning ("failed to update %s property of %s from xml",
				   id, adaptor->name);
			g_free (id);
			continue;
		}

		/* the property has Disabled=TRUE ... */
		if (!property_class)
			*properties = g_list_delete_link (*properties, list);

		g_free (id);
	}
}

static GList **
gwa_action_lookup (GList **actions, const gchar *id, gboolean only_group)
{
	GList **group = actions, *l;
	
	for (l = *actions; l; l = g_list_next (l))
	{
		GWAAction *action = l->data;
		
		if (only_group && action->is_a_group == FALSE) continue;
		
		if (strcmp (action->id, id) == 0)
			return (only_group) ? &action->actions : group;
			
		if (action->is_a_group && action->actions &&
		    (group = gwa_action_lookup (&action->actions, id, only_group)))
			return group;
	}
	
	return NULL;
}

/*
 * gwa_action_append:
 *
 * @adaptor: the GladeWidgetAdaptor.
 * @group_id: the group id weater to append this action or NULL.
 * @id: the id of the action.
 * @label: the label of the action or NULL.
 * @stock: the stock id of the action or NULL (ie: "gtk-delete").
 * @is_a_group: if this is an action group.
 *
 * Append a new GWAAction to @adaptor.
 *
 * Returns weater or not the action wass appended
 *
 */
static gboolean
gwa_action_append (GladeWidgetAdaptor *adaptor,
		   const gchar *group_id,
		   const gchar *id,
		   const gchar *label,
		   const gchar *stock,
		   gboolean is_a_group)
{
	GList **group;
	GWAAction *action;

	g_return_val_if_fail (id != NULL, FALSE);

	if (group_id)
	{
		group = gwa_action_lookup (&adaptor->actions, group_id, TRUE);
		if (group == NULL) return FALSE;
	}
	else group = &adaptor->actions;
		
	if (gwa_action_lookup (&adaptor->actions, id, FALSE)) return FALSE;
			
	action = g_new0 (GWAAction, 1);
	action->id = g_strdup (id);
	action->label = (label) ? g_strdup (label) : NULL;
	action->stock = (stock) ? g_strdup (stock) : NULL;
	action->is_a_group = is_a_group;	
	
	*group = g_list_prepend (*group, action);
	
	return TRUE;
}

static void
gwa_update_actions (GladeWidgetAdaptor *adaptor,
		    GladeXmlNode *node,
		    gchar *group_id)
{
	GladeXmlNode *child;
	gchar *id, *label, *stock;
	gboolean group;
	
	for (child = glade_xml_node_get_children (node);
	     child; child = glade_xml_node_next (child))
	{
		if ((group = glade_xml_node_verify_silent (child, GLADE_TAG_ACTION_GROUP)) == FALSE &&
		    glade_xml_node_verify_silent (child, GLADE_TAG_ACTION) == FALSE)
			continue;

		id = glade_xml_get_property_string_required
					(child,	GLADE_TAG_ID, adaptor->name);
		if (id == NULL)
			continue;

		label = glade_xml_get_property_string (child, GLADE_TAG_NAME);
		stock = glade_xml_get_property_string (child, GLADE_TAG_STOCK);

		gwa_action_append (adaptor, group_id, id,
				   (label == NULL && stock == NULL) ? id : label,
				   stock, group);
		if (group) gwa_update_actions (adaptor, child, id);
			
		g_free (id);
		g_free (label);
		g_free (stock);
	}
}

static gboolean
gwa_extend_with_node (GladeWidgetAdaptor *adaptor, 
		      GladeXmlNode       *node,
		      GModule            *module,
		      const gchar        *domain)
{
	GladeWidgetAdaptorClass *adaptor_class = 
		GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor);
	GladeXmlNode *child;
	gchar        *child_type;
	
	if (module)
	{
		glade_xml_load_sym_from_node (node, module,
					      GLADE_TAG_POST_CREATE_FUNCTION,
					      (gpointer *)&adaptor_class->post_create);

		glade_xml_load_sym_from_node (node, module,
					      GLADE_TAG_GET_INTERNAL_CHILD_FUNCTION,
					      (gpointer *)&adaptor_class->get_internal_child);

		glade_xml_load_sym_from_node (node, module,
					      GLADE_TAG_LAUNCH_EDITOR_FUNCTION,
					      (gpointer *)&adaptor_class->launch_editor);

		glade_xml_load_sym_from_node (node, module, 
					      GLADE_TAG_SET_FUNCTION, 
					      (gpointer *)&adaptor_class->set_property);

		glade_xml_load_sym_from_node (node, module, 
					      GLADE_TAG_GET_FUNCTION, 
					      (gpointer *)&adaptor_class->get_property);

		glade_xml_load_sym_from_node (node, module, 
					      GLADE_TAG_VERIFY_FUNCTION, 
					      (gpointer *)&adaptor_class->verify_property);

		glade_xml_load_sym_from_node (node, module,
					      GLADE_TAG_ADD_CHILD_FUNCTION,
					      (gpointer *)&adaptor_class->add);

		glade_xml_load_sym_from_node (node, module,
					      GLADE_TAG_REMOVE_CHILD_FUNCTION,
					      (gpointer *)&adaptor_class->remove);

		glade_xml_load_sym_from_node (node, module,
					      GLADE_TAG_GET_CHILDREN_FUNCTION,
					      (gpointer *)&adaptor_class->get_children);

		glade_xml_load_sym_from_node (node, module,
					      GLADE_TAG_CHILD_SET_PROP_FUNCTION,
					      (gpointer *)&adaptor_class->child_set_property);

		glade_xml_load_sym_from_node (node, module,
					      GLADE_TAG_CHILD_GET_PROP_FUNCTION,
					      (gpointer *)&adaptor_class->child_get_property);

		glade_xml_load_sym_from_node (node, module,
					      GLADE_TAG_CHILD_VERIFY_FUNCTION,
					      (gpointer *)&adaptor_class->child_verify_property);

		glade_xml_load_sym_from_node (node, module,
					      GLADE_TAG_REPLACE_CHILD_FUNCTION,
					      (gpointer *)&adaptor_class->replace_child);

	}
	adaptor_class->fixed = 
		glade_xml_get_property_boolean
		(node, GLADE_TAG_FIXED, adaptor_class->fixed);

	/* Check if this class is toplevel */
	adaptor_class->toplevel =
		glade_xml_get_property_boolean
		(node, GLADE_TAG_TOPLEVEL, adaptor_class->toplevel);

	/* Check if this class uses placeholders for child widgets */
	adaptor_class->use_placeholders =
		glade_xml_get_property_boolean
		(node, GLADE_TAG_USE_PLACEHOLDERS, adaptor_class->use_placeholders);

	/* Check default size when used as a toplevel in the GladeDesignView */
	adaptor_class->default_width =
		glade_xml_get_property_int
		(node, GLADE_TAG_DEFAULT_WIDTH, adaptor_class->default_width);
	adaptor_class->default_height =
		glade_xml_get_property_int
		(node, GLADE_TAG_DEFAULT_HEIGHT, adaptor_class->default_height);

	/* Override the special-child-type here */
	if ((child_type =
	     glade_xml_get_value_string (node, GLADE_TAG_SPECIAL_CHILD_TYPE)) != NULL)
		adaptor->priv->special_child_type =
			(g_free (adaptor->priv->special_child_type), child_type);
	
	/* if we found a <properties> tag on the xml file, we add the properties
	 * that we read from the xml file to the class.
	 */
	if ((child = 
	     glade_xml_search_child (node, GLADE_TAG_PROPERTIES)) != NULL)
		gwa_update_properties_from_node
			(adaptor, child, module, &adaptor->properties, domain, FALSE);

	if ((child = 
	     glade_xml_search_child (node, GLADE_TAG_PACKING_PROPERTIES)) != NULL)
		gwa_update_properties_from_node
			(adaptor, child, module, &adaptor->packing_props, domain, TRUE);

	if ((child = 
	     glade_xml_search_child (node, GLADE_TAG_PACKING_DEFAULTS)) != NULL)
		gwa_set_packing_defaults_from_node (adaptor, child);
	
	/* Search for actions */
	gwa_update_actions (adaptor, node, NULL);
	
	return TRUE;
}

static gboolean
gwa_script_item_activate_cb (GladeWidgetAdaptor *adaptor,
			     GladeWidget *widget,
			     const gchar *action_id,
			     GladeBindingScript *script)
{
	gchar *argv[2] = {widget->name, NULL};
	
	glade_binding_run_script (script->binding, script->path, argv);
	return TRUE;
}

static void
gwa_setup_binding_scripts (GladeWidgetAdaptor *adaptor)
{
	GList *l, *bindings;

	if ((bindings = glade_binding_get_all ()) == NULL) return;
	
	gwa_action_append (adaptor, NULL, "scripts", "Scripts", NULL, TRUE);
	
	for (l = bindings; l; l = g_list_next (l))
	{
		GladeBinding *binding = l->data;
		GList *list;
	
		for (list = g_hash_table_lookup (binding->context_scripts, adaptor->name);
		     list; list = g_list_next (list))
		{
			GladeBindingScript *script = list->data;
			gchar *detailed_signal, *name = g_strdup (script->name);
			
			detailed_signal = g_strdup_printf ("action-activated::%s", script->name);
			glade_util_replace (name, '_', ' ');

			gwa_action_append (adaptor, "scripts", script->name,
					   name, NULL, FALSE);

			g_signal_connect (adaptor, detailed_signal,
					  G_CALLBACK (gwa_script_item_activate_cb),
					  script);

			g_free (name);
			g_free (detailed_signal);
		}
	}
	
	g_list_free (bindings);
}

/**
 * glade_widget_adaptor_from_catalog:
 * @class_node: A #GladeXmlNode
 * @catname: the name of the owning catalog
 * @library: the name of the library used to load class methods from
 * @domain: the domain to translate strings from this plugin from
 * @book: the devhelp search domain for the owning catalog.
 *
 * Dynamicly creates a subclass of #GladeWidgetAdaptor and subclasses
 * the closest parent adaptor (parent class adapters must be creates/registerd
 * prior to child classes, otherwise inheritance wont work) and parses in
 * the relevent catalog info.
 */
GladeWidgetAdaptor *
glade_widget_adaptor_from_catalog (GladeXmlNode     *class_node,
				   const gchar      *catname,
				   GModule          *module,
				   const gchar      *domain,
				   const gchar      *book)
{
	GladeWidgetAdaptor *adaptor = NULL;
	gchar              *name, *generic_name, *adaptor_name, *func_name;
	GType               object_type, adaptor_type, parent_type;

	if (!glade_xml_node_verify (class_node, GLADE_TAG_GLADE_WIDGET_CLASS))
	{
		g_warning ("Widget class node is not '%s'", 
			   GLADE_TAG_GLADE_WIDGET_CLASS);
		return NULL;
	}
	
	if ((name = glade_xml_get_property_string_required
	     (class_node, GLADE_TAG_NAME, NULL)) == NULL)
		return NULL;

	/* get the object type directly by function, if possible, else by name hack */
	if ((func_name = glade_xml_get_property_string (class_node, GLADE_TAG_GET_TYPE_FUNCTION)) != NULL)
		object_type = glade_util_get_type_from_name (func_name, TRUE);
	else
		object_type = glade_util_get_type_from_name (name, FALSE);
		
	if (object_type == 0)
	{
		g_warning ("Failed to load the GType for '%s'", name);
		g_free (name);
		return NULL;
	}

	if (glade_widget_adaptor_get_by_type (object_type))
	{
		g_warning ("Adaptor class for '%s' already defined", 
			   g_type_name (object_type));
		g_free (name);
		return NULL;
	}
	
	if ((adaptor_name = glade_xml_get_property_string (class_node, GLADE_TAG_ADAPTOR)))
		adaptor_type = g_type_from_name (adaptor_name);
	else
		adaptor_type = gwa_derive_adaptor_for_type (object_type);
	
	if (adaptor_type == 0)
	{
		g_warning ("Failed to get %s's adaptor %s", name,
			   (adaptor_name) ? adaptor_name : "");
		g_free (adaptor_name);
		g_free (name);
		return NULL;
	}
	
	generic_name = glade_xml_get_property_string (class_node, GLADE_TAG_GENERIC_NAME);
	adaptor      = g_object_new (adaptor_type, 
				     "type", object_type,
				     "name", name,
				     "generic-name", generic_name,
				     NULL);
	if (generic_name)
		g_free (generic_name);
	
	if ((adaptor->title = glade_xml_get_property_string_required
	     (class_node, GLADE_TAG_TITLE,
	      "This value is needed to display object class names in the UI")) == NULL)
	{
		g_warning ("Class '%s' built without a '%s'", name, GLADE_TAG_TITLE);
		adaptor->title = g_strdup (name);
	}
	
	/* Translate title */
	if (adaptor->title != dgettext (domain, adaptor->title))
	{
		gchar *ptr   = dgettext (domain, adaptor->title);
		g_free (adaptor->title);
		adaptor->title = ptr;
	}

	if (G_TYPE_IS_INSTANTIATABLE (adaptor->type)    &&
	    G_TYPE_IS_ABSTRACT (adaptor->type) == FALSE &&
	    adaptor->generic_name == NULL)
	{
		g_warning ("Instantiatable class '%s' built without a '%s'",
			   name, GLADE_TAG_GENERIC_NAME);
	}

	/* Perform a stoopid fallback just incase */
	if (adaptor->generic_name == NULL)
		adaptor->generic_name = g_strdup ("widget");
	
	/* Dont mention gtk+ as a required lib in the generated glade file */
	if (strcmp (catname, "gtk+"))
		adaptor->priv->catalog = g_strdup (catname);

	if (book)
		adaptor->priv->book = g_strdup (book);

	gwa_extend_with_node (adaptor, class_node, module, domain);

	/* Set default weight on properties */
	for (parent_type = adaptor->type;
	     parent_type != 0;
	     parent_type = g_type_parent (parent_type))
	{
		gwa_properties_set_weight (&adaptor->properties, parent_type);
		gwa_properties_set_weight (&adaptor->packing_props, parent_type);
	}

	gwa_setup_binding_scripts (adaptor);
	
	glade_widget_adaptor_register (adaptor);

	return adaptor;
}

/**
 * glade_widget_adaptor_create_internal:
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
glade_widget_adaptor_create_internal  (GladeWidget      *parent,
				       GObject          *internal_object,
				       const gchar      *internal_name,
				       const gchar      *parent_name,
				       gboolean          anarchist,
				       GladeCreateReason reason)
{
	GladeWidgetAdaptor *adaptor;
	GladeProject       *project;

	g_return_val_if_fail (GLADE_IS_WIDGET (parent), NULL);
	project = glade_widget_get_project (parent);

        if ((adaptor = glade_widget_adaptor_get_by_name 
	     (G_OBJECT_TYPE_NAME (internal_object))) == NULL)
	{
		g_critical ("Unable to find widget class for type %s", 
			    G_OBJECT_TYPE_NAME (internal_object));
		return NULL;
	}

	return glade_widget_adaptor_create_widget (adaptor, FALSE,
						   "anarchist", anarchist,
						   "parent", parent,
						   "project", project,
						   "internal", internal_name,
						   "internal-name", parent_name,
						   "reason", reason,
						   "object", internal_object,
						   NULL);
}

/**
 * glade_widget_adaptor_create_widget:
 * @adaptor: a #GladeWidgetAdaptor
 * @query:   whether to display query dialogs if
 *           applicable to the class
 * @...:     a %NULL terminated list of string/value pairs of #GladeWidget 
 *           properties
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
glade_widget_adaptor_create_widget_real (gboolean          query, 
					 const gchar      *first_property,
					 ...)
{
	GladeWidgetAdaptor *adaptor;
	GType               gwidget_type;
	GladeWidget        *gwidget;
	va_list             vl, vl_copy;

	g_return_val_if_fail (strcmp (first_property, "adaptor") == 0, NULL);

	va_start (vl, first_property);
	va_copy (vl_copy, vl);

	adaptor = va_arg (vl, GladeWidgetAdaptor *);

	va_end (vl);

	if (GLADE_IS_WIDGET_ADAPTOR (adaptor) == FALSE)
	{
		g_critical ("No adaptor found in glade_widget_adaptor_create_widget_real args");
		va_end (vl_copy);
		return NULL;
	}

	if (GWA_IS_FIXED (adaptor))
		gwidget_type = GLADE_TYPE_FIXED;
	else 
		gwidget_type = GLADE_TYPE_WIDGET;


	gwidget = (GladeWidget *)g_object_new_valist (gwidget_type,
						      first_property, 
						      vl_copy);
	va_end (vl_copy);
	
	if (query && glade_widget_adaptor_query (adaptor))
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

typedef struct
{
	const gchar        *name;
	GladeWidgetAdaptor *adaptor;
} GladeAdaptorSearchPair;


static void
search_adaptor_by_name (GType                  *type,
			GladeWidgetAdaptor     *adaptor,
			GladeAdaptorSearchPair *pair)
{
	if (!strcmp (adaptor->name, pair->name))
		pair->adaptor = adaptor;
}

/**
 * glade_widget_adaptor_get_by_name:
 * @name: name of the widget class (for instance: GtkButton)
 *
 * Returns: an existing #GladeWidgetAdaptor with the name equaling @name,
 *          or %NULL if such a class doesn't exist
 **/
GladeWidgetAdaptor  *
glade_widget_adaptor_get_by_name (const gchar  *name)
{


	GladeAdaptorSearchPair pair = { name, NULL };
	
	if (adaptor_hash != NULL)
	{
		g_hash_table_foreach (adaptor_hash, 
				      (GHFunc)search_adaptor_by_name, &pair);
	}
	return pair.adaptor;
}


/**
 * glade_widget_adaptor_get_by_type:
 * @type: the #GType of an object class
 *
 * Returns: an existing #GladeWidgetAdaptor with the type equaling @type,
 *          or %NULL if such a class doesn't exist
 **/
GladeWidgetAdaptor  *
glade_widget_adaptor_get_by_type (GType  type)
{
	if (adaptor_hash != NULL)
		return g_hash_table_lookup (adaptor_hash, &type);
	else
		return NULL;
}

/**
 * glade_widget_adaptor_get_property_class:
 * @adaptor: a #GladeWidgetAdaptor
 * @name: a string
 *
 * Retrieves the #GladePropertyClass for @name in @adaptor
 *
 * Returns: A #GladePropertyClass object
 */
GladePropertyClass  *
glade_widget_adaptor_get_property_class (GladeWidgetAdaptor *adaptor,
					 const gchar        *name)
{
	GList *list;
	GladePropertyClass *pclass;

	for (list = adaptor->properties; list && list->data; list = list->next)
	{
		pclass = list->data;
		if (strcmp (pclass->id, name) == 0)
			return pclass;
	}
	return NULL;
}

/**
 * glade_widget_adaptor_get_pack_property_class:
 * @adaptor: a #GladeWidgetAdaptor
 * @name: a string
 *
 * Retrieves the #GladePropertyClass for @name in 
 * @adaptor's child properties
 * 
 * Returns: A #GladePropertyClass object
 */
GladePropertyClass  *
glade_widget_adaptor_get_pack_property_class (GladeWidgetAdaptor *adaptor,
					      const gchar        *name)
{
	GList *list;
	GladePropertyClass *pclass;

	for (list = adaptor->packing_props; list && list->data; list = list->next)
	{
		pclass = list->data;
		if (strcmp (pclass->id, name) == 0)
			return pclass;
	}
	return NULL;
}

/**
 * glade_widget_class_default_params:
 * @adaptor: a #GladeWidgetAdaptor
 * @construct: whether to return construct params or not construct params
 * @n_params: return location if any defaults are specified for this class.
 * 
 * Returns: A list of params for use in g_object_newv ()
 */
GParameter *
glade_widget_adaptor_default_params (GladeWidgetAdaptor *adaptor,
				     gboolean            construct,
				     guint              *n_params)
{
	GArray              *params;
	GObjectClass        *oclass;
	GParamSpec         **pspec;
	GladePropertyClass  *pclass;
	guint                n_props, i;

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (n_params != NULL, NULL);

	/* As a slight optimization, we never unref the class
	 */
	oclass = g_type_class_ref (adaptor->type);
	pspec  = g_object_class_list_properties (oclass, &n_props);
	params = g_array_new (FALSE, FALSE, sizeof (GParameter));

	for (i = 0; i < n_props; i++)
	{
		GParameter parameter = { 0, };

		pclass = glade_widget_adaptor_get_property_class
			(adaptor, pspec[i]->name);
		
		/* Ignore properties based on some criteria
		 */
		if (pclass == NULL       || /* Unaccounted for in the builder */
		    pclass->virt         || /* should not be set before 
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
				    parameter.name, adaptor->name);
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


/**
 * glade_widget_adaptor_post_create:
 * @adaptor:   A #GladeWidgetAdaptor
 * @object:    The #GObject
 * @reason:    The #GladeCreateReason that @object was created for
 *
 * An adaptor function to be called after the object is created
 */
void
glade_widget_adaptor_post_create (GladeWidgetAdaptor *adaptor,
				  GObject            *object,
				  GladeCreateReason   reason)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->type));

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->post_create)
		GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->post_create (adaptor, object, reason);
	/* XXX Dont complain here if no implementation is found */
}

/**
 * glade_widget_adaptor_get_internal_child:
 * @adaptor:       A #GladeWidgetAdaptor
 * @object:        The #GObject
 * @internal_name: The string identifier of the internal object
 *
 * Retrieves the internal object @internal_name from @object
 *
 * Returns: The internal #GObject
 */
GObject *
glade_widget_adaptor_get_internal_child (GladeWidgetAdaptor *adaptor,
					 GObject            *object,
					 const gchar        *internal_name)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (G_IS_OBJECT (object), NULL);
	g_return_val_if_fail (internal_name != NULL, NULL);
	g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->type), NULL);

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->get_internal_child)
		return GLADE_WIDGET_ADAPTOR_GET_CLASS
			(adaptor)->get_internal_child (adaptor, object, internal_name);
	else
		g_critical ("No get_internal_child() support in adaptor %s", adaptor->name);

	return NULL;
}

/**
 * glade_widget_adaptor_launch_editor:
 * @adaptor:       A #GladeWidgetAdaptor
 * @object:        The #GObject
 *
 * Launches a custom object editor for @object.
 */
void
glade_widget_adaptor_launch_editor (GladeWidgetAdaptor *adaptor,
				    GObject            *object)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->type));

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->launch_editor)
		GLADE_WIDGET_ADAPTOR_GET_CLASS
			(adaptor)->launch_editor (adaptor, object);
	else
		g_critical ("No launch_editor() support in adaptor %s", adaptor->name);
}

/**
 * glade_widget_adaptor_set_property:
 * @adaptor:       A #GladeWidgetAdaptor
 * @object:        The #GObject
 * @property_name: The property identifier
 * @value:         The #GValue
 *
 * This delagate function is used to apply the property value on
 * the runtime object.
 *
 */
void
glade_widget_adaptor_set_property (GladeWidgetAdaptor *adaptor,
				   GObject            *object,
				   const gchar        *property_name,
				   const GValue       *value)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (property_name != NULL && value != NULL);
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->type));

	/* The base class provides an implementation */
	GLADE_WIDGET_ADAPTOR_GET_CLASS
		(adaptor)->set_property (adaptor, object, property_name, value);
}


/**
 * glade_widget_adaptor_get_property:
 * @adaptor:       A #GladeWidgetAdaptor
 * @object:        The #GObject
 * @property_name: The property identifier
 * @value:         The #GValue
 *
 * Gets @value of @property_name on @object.
 *
 */
void
glade_widget_adaptor_get_property (GladeWidgetAdaptor *adaptor,
				   GObject            *object,
				   const gchar        *property_name,
				   GValue             *value)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (property_name != NULL && value != NULL);
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->type));

	/* The base class provides an implementation */
	GLADE_WIDGET_ADAPTOR_GET_CLASS
		(adaptor)->get_property (adaptor, object, property_name, value);
}


/**
 * glade_widget_adaptor_verify_property:
 * @adaptor:       A #GladeWidgetAdaptor
 * @object:        The #GObject
 * @property_name: The property identifier
 * @value:         The #GValue
 *
 * This delagate function is always called whenever setting any
 * properties with the exception of load time, and copy/paste time
 * (basicly the two places where we recreate a hierarchy that we
 * already know "works") its basicly an optional backend provided
 * boundry checker for properties.
 *
 * Returns: whether or not its OK to set @value on @object, this function
 * will silently return TRUE if the class did not provide a verify function.
 */
gboolean
glade_widget_adaptor_verify_property    (GladeWidgetAdaptor *adaptor,
					 GObject            *object,
					 const gchar        *property_name,
					 const GValue       *value)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);
	g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
	g_return_val_if_fail (property_name != NULL && value != NULL, FALSE);
	g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->type), FALSE);

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->verify_property)
		return GLADE_WIDGET_ADAPTOR_GET_CLASS
			(adaptor)->verify_property (adaptor, object, property_name, value);

	return TRUE;
}

/**
 * glade_widget_adaptor_add:
 * @adaptor:   A #GladeWidgetAdaptor
 * @container: The #GObject container
 * @child:     The #GObject child
 *
 * Adds @child to @container.
 */
void
glade_widget_adaptor_add (GladeWidgetAdaptor *adaptor,
			  GObject            *container,
			  GObject            *child)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (container));
	g_return_if_fail (G_IS_OBJECT (child));
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->type));

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->add)
		GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->add (adaptor, container, child);
	else
		g_critical ("No add() support in adaptor %s", adaptor->name);
}


/**
 * glade_widget_adaptor_remove:
 * @adaptor:   A #GladeWidgetAdaptor
 * @container: The #GObject container
 * @child:     The #GObject child
 *
 * Removes @child from @container.
 */
void
glade_widget_adaptor_remove (GladeWidgetAdaptor *adaptor,
			     GObject            *container,
			     GObject            *child)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (container));
	g_return_if_fail (G_IS_OBJECT (child));
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->type));

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->remove)
		GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->remove (adaptor, container, child);
	else
		g_critical ("No remove() support in adaptor %s", adaptor->name);
}

/**
 * glade_widget_adaptor_remove:
 * @adaptor:   A #GladeWidgetAdaptor
 * @container: The #GObject container
 *
 * Lists the children of @container.
 *
 * Returns: A #GList of children
 */
GList *
glade_widget_adaptor_get_children (GladeWidgetAdaptor *adaptor,
				   GObject            *container)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (G_IS_OBJECT (container), NULL);
	g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->type), NULL);

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->get_children)
		return GLADE_WIDGET_ADAPTOR_GET_CLASS
			(adaptor)->get_children (adaptor, container);

	/* Dont complain here if no implementation is found */

	return NULL;
}

/**
 * glade_widget_adaptor_has_child:
 * @adaptor:   A #GladeWidgetAdaptor
 * @container: The #GObject container
 * @child:     The #GObject child
 *
 * Returns whether @child is infact inside @container.
 */
gboolean
glade_widget_adaptor_has_child (GladeWidgetAdaptor *adaptor,
				GObject            *container,
				GObject            *child)
{
	GList *list, *children = NULL;
	gboolean found = FALSE;

	children = glade_widget_adaptor_get_children (adaptor, container);

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

/**
 * glade_widget_adaptor_child_set_property:
 * @adaptor:       A #GladeWidgetAdaptor
 * @container:     The #GObject container
 * @child:         The #GObject child
 * @property_name: The id of the property
 * @value:         The @GValue
 *
 * Sets @child's packing property identified by @property_name to @value.
 */
void
glade_widget_adaptor_child_set_property (GladeWidgetAdaptor *adaptor,
					 GObject            *container,
					 GObject            *child,
					 const gchar        *property_name,
					 const GValue       *value)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (container));
	g_return_if_fail (G_IS_OBJECT (child));
	g_return_if_fail (property_name != NULL && value != NULL);
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->type));

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->child_set_property)
		GLADE_WIDGET_ADAPTOR_GET_CLASS
			(adaptor)->child_set_property (adaptor, container, child,
						       property_name, value);
	else
		g_critical ("No child_set_property() support in adaptor %s", adaptor->name);

}

/**
 * glade_widget_adaptor_child_get_property:
 * @adaptor:       A #GladeWidgetAdaptor
 * @container:     The #GObject container
 * @child:         The #GObject child
 * @property_name: The id of the property
 * @value:         The @GValue
 *
 * Gets @child's packing property identified by @property_name.
 */
void
glade_widget_adaptor_child_get_property (GladeWidgetAdaptor *adaptor,
					 GObject            *container,
					 GObject            *child,
					 const gchar        *property_name,
					 GValue             *value)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (container));
	g_return_if_fail (G_IS_OBJECT (child));
	g_return_if_fail (property_name != NULL && value != NULL);
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->type));

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->child_get_property)
		GLADE_WIDGET_ADAPTOR_GET_CLASS
			(adaptor)->child_get_property (adaptor, container, child,
						       property_name, value);
	else
		g_critical ("No child_set_property() support in adaptor %s", adaptor->name);
}

/**
 * glade_widget_adaptor_child_verify_property:
 * @adaptor:       A #GladeWidgetAdaptor
 * @container:     The #GObject container
 * @child:         The #GObject child
 * @property_name: The id of the property
 * @value:         The @GValue
 *
 * This delagate function is always called whenever setting any
 * properties with the exception of load time, and copy/paste time
 * (basicly the two places where we recreate a hierarchy that we
 * already know "works") its basicly an optional backend provided
 * boundry checker for properties.
 *
 * Returns: whether or not its OK to set @value on @object, this function
 * will silently return TRUE if the class did not provide a verify function.
 */
gboolean
glade_widget_adaptor_child_verify_property (GladeWidgetAdaptor *adaptor,
					    GObject            *container,
					    GObject            *child,
					    const gchar        *property_name,
					    GValue             *value)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);
	g_return_val_if_fail (G_IS_OBJECT (container), FALSE);
	g_return_val_if_fail (G_IS_OBJECT (child), FALSE);
	g_return_val_if_fail (property_name != NULL && value != NULL, FALSE);
	g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->type), FALSE);

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->child_verify_property)
		return GLADE_WIDGET_ADAPTOR_GET_CLASS
			(adaptor)->child_verify_property (adaptor, 
							  container, child,
							  property_name, value);

	return TRUE;
}


/**
 * glade_widget_adaptor_replace_child:
 * @adaptor:       A #GladeWidgetAdaptor
 * @container:     The #GObject container
 * @old:           The old #GObject child
 * @new:           The new #GObject child
 *
 * Replaces @old with @new in @container while positioning
 * @new where @old was and assigning it appropriate packing 
 * property values.
 */
void
glade_widget_adaptor_replace_child (GladeWidgetAdaptor *adaptor,
				    GObject            *container,
				    GObject            *old_obj,
				    GObject            *new_obj)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (container));
	g_return_if_fail (G_IS_OBJECT (old_obj));
	g_return_if_fail (G_IS_OBJECT (new_obj));
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->type));

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->replace_child)
		GLADE_WIDGET_ADAPTOR_GET_CLASS 
			(adaptor)->replace_child (adaptor, container, old_obj, new_obj);
	else
		g_critical ("No replace_child() support in adaptor %s", adaptor->name);
}

/**
 * glade_widget_adaptor_query:
 * @adaptor: A #GladeWidgetAdaptor
 *
 * Returns: whether the user needs to be queried for
 * certain properties upon creation of this class.
 */
gboolean
glade_widget_adaptor_query (GladeWidgetAdaptor *adaptor)
{
	GladePropertyClass *pclass;
	GList *l;

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);

	for (l = adaptor->properties; l; l = l->next)
	{
		pclass = l->data;

		if (pclass->query)
			return TRUE;
	}

	return FALSE;
}

/**
 * glade_widget_adaptor_get_packing_default:
 * @child_adaptor:  A #GladeWidgetAdaptor
 * @parent_adaptor: The #GladeWidgetAdaptor for the parent object
 * @property_id:    The string property identifier
 *
 * Gets the default value for @property_id on a widget governed by
 * @child_adaptor when parented in a widget governed by @parent_adaptor
 *
 * Returns: a string representing the default value for @property_id
 */
G_CONST_RETURN gchar *
glade_widget_adaptor_get_packing_default (GladeWidgetAdaptor *child_adaptor,
					  GladeWidgetAdaptor *container_adaptor,
					  const gchar        *id)
{
	GladeChildPacking *packing = NULL;
	GList             *l;

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (child_adaptor), NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (container_adaptor), NULL);

	if ((packing = 
	     glade_widget_adaptor_get_child_packing (child_adaptor,
						     container_adaptor->name)) != NULL)
	{
		for (l = packing->packing_defaults; l; l = l->next)
		{
			GladePackingDefault *def = l->data;
			
			if (strcmp (def->id, id) == 0)
				return def->value;
		}
	}
	return NULL;
}

/*
 * glade_widget_adaptor_action_activate:
 *
 * @widget: the GladeWidget.
 * @action_id: The action id (detail of GladeWidgetAdaptor's action-activated signal).
 *
 * Emit @widget's adaptor action-activated::@action_id signal.
 * GladeWidget's action-activated proxy signal is also emited.
 *
 */
void
glade_widget_adaptor_action_activate (GladeWidget *widget, const gchar *action_id)
{
	GladeWidgetAdaptor *adaptor;
	GQuark detail;
	guint signal;
	gboolean retval;
	
	g_return_if_fail (GLADE_IS_WIDGET (widget) && action_id);

	signal = gwa_signals [SIGNAL_ACTION_ACTIVATED];	
	detail = g_quark_from_string (action_id);
	
	for (adaptor = widget->adaptor;
	     adaptor;
	     adaptor = gwa_get_parent_adaptor (adaptor))
	{
		if (gwa_action_lookup (&adaptor->actions, action_id, FALSE))
			g_signal_emit (adaptor, signal, detail, widget,
				       action_id, &retval);
		else
			return;
	}
}


/**
 * glade_widget_adaptor_is_container:
 * @adaptor: A #GladeWidgetAdaptor
 *
 * Checks whether or not this adaptor has support
 * to interface with child objects.
 *
 * Returns whether or not @adaptor is a container
 */
gboolean
glade_widget_adaptor_is_container (GladeWidgetAdaptor *adaptor)
{

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);

	/* A GWA container must at least implement add/remove/get_children
	 */
	return 	(GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->add &&
		 GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->remove &&
		 GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->get_children);
}
