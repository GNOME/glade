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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
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

#include <gtk/gtkenums.h> /* This should go away. Chema */

#include "glade.h"
#include "glade-widget-class.h"
#include "glade-xml-utils.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-catalog.h"
#include "glade-parameter.h"
#include "glade-debug.h"

/* hash table that will contain all the GtkWidgetClass'es created, indexed by its name */
static GHashTable *widget_classes = NULL;

static gchar *
glade_widget_class_compose_get_type_func (GladeWidgetClass *class)
{
	gchar *retval;
	GString *tmp;
	gint i = 1, j;

	tmp = g_string_new (class->name);

	while (tmp->str[i])
	{
		if (g_ascii_isupper (tmp->str[i]))
		{
			tmp = g_string_insert_c (tmp, i++, '_');

			j = 0;
			while (g_ascii_isupper (tmp->str[i++]))
				j++;

			if (j > 2)
				g_string_insert_c (tmp, i-2, '_');

			continue;
		}
		i++;
	}

	tmp = g_string_append (tmp, "_get_type");
        retval = g_ascii_strdown (tmp->str, tmp->len);
	g_string_free (tmp, TRUE);

	return retval;
}

void
glade_widget_class_free (GladeWidgetClass *widget_class)
{
	if (widget_class == NULL)
		return;

	g_free (widget_class->generic_name);
	g_free (widget_class->name);

	g_list_foreach (widget_class->properties, (GFunc) g_free, NULL);
	g_list_free (widget_class->properties);

	g_list_foreach (widget_class->child_properties, (GFunc) g_free, NULL);
	g_list_free (widget_class->child_properties);

	g_list_foreach (widget_class->signals, (GFunc) g_free, NULL);
	g_list_free (widget_class->signals);
}

static GList *
glade_widget_class_list_properties (GladeWidgetClass *class)
{
	GladePropertyClass *property_class;
	GObjectClass *object_class;
	GParamSpec **specs = NULL;
	GParamSpec *spec;
	GType last;
	gint n_specs = 0;
	gint i;
	GList *list = NULL;

	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), NULL);

	g_type_class_ref (class->type); /* hmm */
	 /* We count on the fact we have an instance, or else we'd have
	  * touse g_type_class_ref ();
	  */

	object_class = g_type_class_peek (class->type);
	if (!object_class)
	{
		g_warning ("Class peek failed\n");
		return NULL;
	}

	specs = g_object_class_list_properties (object_class, &n_specs);

	last = 0;
	for (i = 0; i < n_specs; i++)
	{

		spec = specs[i];

		/* We only use the writable properties */
		if (spec->flags & G_PARAM_WRITABLE)
		{
			property_class = glade_property_class_new_from_spec (spec);
			if (!property_class)
				continue;

			/* should this if go into property_class_new_from_spec ? */
			if (!g_ascii_strcasecmp (g_type_name (spec->owner_type), "GtkWidget") &&
			    g_ascii_strcasecmp (spec->name, "name"))
				property_class->common = TRUE;
			else
				property_class->common = FALSE;

			property_class->optional = FALSE;

			list = g_list_prepend (list, property_class);
		}
	}

	list = g_list_reverse (list);

	g_free (specs);

	return list;
}

static GList * 
glade_widget_class_list_child_properties (GladeWidgetClass *class) 
{
	GladePropertyClass *property_class;
	GObjectClass *object_class;
	GParamSpec **specs = NULL;
	GParamSpec *spec;
	gint n_specs = 0;
	gint i;
	GList *list = NULL;

	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), NULL);

	/* only containers have child propeties */
	if (!g_type_is_a (class->type, GTK_TYPE_CONTAINER))
		return NULL;

	object_class = g_type_class_peek (class->type);
	if (!object_class)
	{
		g_warning ("Class peek failed\n");
		return NULL;
	}

	specs = gtk_container_class_list_child_properties (object_class, &n_specs);

	for (i = 0; i < n_specs; i++)
	{
		spec = specs[i];

		property_class = glade_property_class_new_from_spec (spec);
		if (!property_class)
			continue;

		property_class->optional = FALSE;
		property_class->packing = TRUE;

		list = g_list_prepend (list, property_class);
	}	

	list = g_list_reverse (list);

	g_free (specs);

	return list;
}

static GList * 
glade_widget_class_list_signals (GladeWidgetClass *class) 
{
	GList *signals = NULL;
	GType type;
	guint count;
	guint *sig_ids;
	guint num_signals;
	GladeWidgetClassSignal *cur;

	g_return_val_if_fail (class->type != 0, NULL);

	type = class->type;
	while (g_type_is_a (type, GTK_TYPE_OBJECT))
	{
		if (G_TYPE_IS_INSTANTIATABLE (type) || G_TYPE_IS_INTERFACE (type))
		{
			sig_ids = g_signal_list_ids (type, &num_signals);

			for (count = 0; count < num_signals; count++)
			{
				cur = g_new0 (GladeWidgetClassSignal, 1);
				cur->name = (gchar *) g_signal_name (sig_ids[count]);
				cur->type = (gchar *) g_type_name (type);

				signals = g_list_append (signals, (GladeWidgetClassSignal *) cur);
			}
			g_free (sig_ids);
		}

		type = g_type_parent (type);
	}

	return signals;
}

static GtkWidget *
glade_widget_class_create_icon (GladeWidgetClass *class)
{
	GtkWidget *icon;
	gchar *icon_path;

	icon_path = g_strdup_printf (PIXMAPS_DIR "/%s.png", class->generic_name);
	icon = gtk_image_new_from_file (icon_path);
	g_free (icon_path);

	return icon;
}

static void
glade_widget_class_update_properties_from_node (GladeXmlNode *node,
						GladeWidgetClass *widget_class)
{
	GladeXmlNode *child;
	GList *properties = widget_class->properties;

	child = glade_xml_node_get_children (node);
	for (; child; child = glade_xml_node_next (child))
	{
		gchar *id;
		GList *list;
		GladePropertyClass *property_class;
		gboolean updated;

		if (!glade_xml_node_verify (child, GLADE_TAG_PROPERTY))
			continue;

		id = glade_xml_get_property_string_required (child, GLADE_TAG_ID, widget_class->name);
		if (!id)
			continue;

		/* find the property in our list, if not found append a new property */
		for (list = properties; list; list = list->next)
		{
			gchar *tmp = GLADE_PROPERTY_CLASS (list->data)->id;
			if (!g_ascii_strcasecmp (id, tmp))
				break;
		}
		if (list)
		{
			property_class = GLADE_PROPERTY_CLASS (list->data);
		}
		else
		{
			property_class = glade_property_class_new ();
			property_class->id = g_strdup (id);
			properties = g_list_append (properties, property_class);
			list = g_list_last (properties);
		}

		updated = glade_property_class_update_from_node (child, widget_class, &property_class);
		if (!updated)
		{
			g_warning ("failed to update %s property of %s from xml", id, widget_class->name);
			g_free (id);
			continue;
		}

		/* the property has Disabled=TRUE ... */
		if (!property_class)
			properties = g_list_delete_link (properties, list);

		g_free (id);
	}
}

/**
 * glade_widget_class_extend_with_file:
 * @filename: complete path name of the xml file with the description of the GladeWidgetClass
 *
 * This function extends an existing GladeWidgetClass with the data found on the file
 * with name @filename (if it exists).  Notably, it will add new properties to the
 * GladeWidgetClass, or modify existing ones, in function of the contents of the file.
 *
 * @returns: TRUE if the file exists and its format is correct, FALSE otherwise.
 **/
static gboolean
glade_widget_class_extend_with_file (GladeWidgetClass *widget_class, const char *filename)
{
	GladeXmlContext *context;
	GladeXmlDoc *doc;
	GladeXmlNode *properties;
	GladeXmlNode *node;
	char *replace_child_function_name;
	char *post_create_function_name;
	char *fill_empty_function_name;
	char *get_internal_child_function_name;

	g_return_val_if_fail (filename != NULL, FALSE);

	context = glade_xml_context_new_from_path (filename, NULL, GLADE_TAG_GLADE_WIDGET_CLASS);
	if (!context)
		return FALSE;

	doc = glade_xml_context_get_doc (context);
	node = glade_xml_doc_get_root (doc);
	if (!doc || !node)
		return FALSE;

	replace_child_function_name = glade_xml_get_value_string (node, GLADE_TAG_REPLACE_CHILD_FUNCTION);
	if (replace_child_function_name && widget_class->module)
	{
		if (!g_module_symbol (widget_class->module, replace_child_function_name, (void **) &widget_class->replace_child))
			g_warning ("Could not find %s\n", replace_child_function_name);
	}
	g_free (replace_child_function_name);

	post_create_function_name = glade_xml_get_value_string (node, GLADE_TAG_POST_CREATE_FUNCTION);
	if (post_create_function_name && widget_class->module)
	{
		if (!g_module_symbol (widget_class->module, post_create_function_name, (void **) &widget_class->post_create_function))
			g_warning ("Could not find %s\n", post_create_function_name);
	}
	g_free (post_create_function_name);

	fill_empty_function_name = glade_xml_get_value_string (node, GLADE_TAG_FILL_EMPTY_FUNCTION);
	if (fill_empty_function_name && widget_class->module)
	{
		if (!g_module_symbol (widget_class->module, fill_empty_function_name, (void **) &widget_class->fill_empty))
			g_warning ("Could not find %s\n", fill_empty_function_name);
	}
	g_free (fill_empty_function_name);

	get_internal_child_function_name = glade_xml_get_value_string (node, GLADE_TAG_GET_INTERNAL_CHILD_FUNCTION);
	if (get_internal_child_function_name && widget_class->module)
	{
		if (!g_module_symbol (widget_class->module, get_internal_child_function_name, (void **) &widget_class->get_internal_child))
			g_warning ("Could not find %s\n", get_internal_child_function_name);
	}
	g_free (get_internal_child_function_name);

	/* if we found a <properties> tag on the xml file, we add the properties
	 * that we read from the xml file to the class.
	 */
	properties = glade_xml_search_child (node, GLADE_TAG_PROPERTIES);
	if (properties)
		glade_widget_class_update_properties_from_node (properties, widget_class);

	return TRUE;
}

/**
 * glade_widget_class_get_by_name:
 * @name: name of the widget class (for instance: GtkButton)
 *
 * Returns an already created GladeWidgetClass with the name passed as argument.
 *
 * If we have not yet created any GladeWidgetClass, this function will return %NULL.
 *
 * Return Value: An existing GladeWidgetClass with the name passed as argument,
 * or %NULL if such a class doesn't exist.
 **/
GladeWidgetClass *
glade_widget_class_get_by_name (const char *name)
{
	if (widget_classes != NULL)
		return g_hash_table_lookup (widget_classes, name);
	else
		return NULL;
}

/**
 * glade_widget_class_merge:
 * @widget_class: main class.
 * @parent_class: secondary class.
 *
 * Merges the contents of the parent_class on the widget_class.
 * The properties of the parent_class will be prepended to
 * those of widget_class.
 **/
static void
glade_widget_class_merge (GladeWidgetClass *widget_class,
			  GladeWidgetClass *parent_class)
{
	GList *list;
	GList *list2;

	g_return_if_fail (GLADE_IS_WIDGET_CLASS (widget_class));
	g_return_if_fail (GLADE_IS_WIDGET_CLASS (parent_class));

	if (GLADE_WIDGET_CLASS_FLAGS (widget_class) == 0)
		widget_class->flags = parent_class->flags;

	if (widget_class->replace_child == NULL)
		widget_class->replace_child = parent_class->replace_child;

	if (widget_class->post_create_function == NULL)
		widget_class->post_create_function = parent_class->post_create_function;

	if (widget_class->fill_empty == NULL)
		widget_class->fill_empty = parent_class->fill_empty;

	/* merge the parent's properties */
	for (list = parent_class->properties; list; list = list->next)
	{
		GladePropertyClass *parent_p_class = list->data;
		GladePropertyClass *child_p_class;

		/* search the child's properties for one with the same id */
		for (list2 = widget_class->properties; list2; list2 = list2->next)
		{
			child_p_class = list2->data;
			if (strcmp (parent_p_class->id, child_p_class->id) == 0)
				break;
		}

		/* if not found, prepend a clone of the parent's one; if found
		 * but the parent one was modified substitute it.
		 */
		if (!list2)
		{
			GladePropertyClass *property_class;

			property_class = glade_property_class_clone (parent_p_class);
			widget_class->properties = g_list_prepend (widget_class->properties, property_class);
		}
		else if (parent_p_class->is_modified)
		{
			glade_property_class_free (child_p_class);
			list2->data = glade_property_class_clone (parent_p_class);
		}
	}
}

/**
 * glade_widget_class_load_library:
 * @library_name: name of the library .
 *
 * Loads the named library from the Glade modules directory, or failing that
 * from the standard platform specific directories.
 *
 * The @library_name should not include any platform specifix prefix or suffix,
 * those are automatically added, if needed, by g_module_build_path()
 *
 * @returns: a #GModule on success, or %NULL on failure.
 */
static GModule *
glade_widget_class_load_library (const gchar *library_name)
{
	gchar   *path;
	GModule *module;

	path = g_module_build_path (MODULES_DIR, library_name);
	if (!path)
	{
		g_warning (_("Not enough memory."));
		return NULL;
	}

	module = g_module_open (path, G_MODULE_BIND_LAZY);
	g_free (path);

	if (!module)
	{
		path = g_module_build_path (NULL, library_name);
		if (!path)
		{
			g_warning (_("Not enough memory."));
			return NULL;
		}

		module = g_module_open (path, G_MODULE_BIND_LAZY);
		g_free (path);
	}

	if (!module)
	{
		g_warning (_("Unable to open the module %s."), library_name);
	}

	return module;
}

/**
 * glade_widget_class_new:
 * @name: name of the widget class (for instance: GtkButton)
 * @generic_name: base of the name for the widgets of this class (for instance: button).
 * This parameter is optional.  For abstract classes there is no need to supply a generic_name.
 * @base_filename: filename containing a further description of the widget, without
 * the directory (optional).
 *
 * Creates a new GladeWidgetClass, initializing it with the
 * name, generic_name and base_filename.
 *
 * The widget class will be first build using the information that the GLib object system
 * returns, and then it will be expanded (optionally) with the information contained on
 * the xml filename.
 *
 * Return Value: The new GladeWidgetClass, or %NULL if there are any errors.
 **/
GladeWidgetClass *
glade_widget_class_new (const char *name,
			const char *generic_name,
			const char *palette_name,
			const char *base_filename,
			const char *base_library)
{
	GladeWidgetClass *widget_class = NULL;
	char *filename = NULL;
	char *init_function_name = NULL;
	GModule *module = NULL;
	GType parent_type;

	g_return_val_if_fail (name != NULL, NULL);

	if (glade_widget_class_get_by_name (name) != NULL)
	{
		g_warning ("The widget class [%s] has at least two different definitions.\n", name);
		goto lblError;
	}

	if (base_filename != NULL)
	{
		filename = g_strconcat (WIDGETS_DIR, "/", base_filename, NULL);
		if (filename == NULL)
		{
			g_warning (_("Not enough memory."));
			goto lblError;
		}
	}

	if (base_library != NULL)
	{
		module = glade_widget_class_load_library (base_library);
		if (!module)
			goto lblError;
	}

	widget_class = g_new0 (GladeWidgetClass, 1);

	widget_class->module = module;

	widget_class->generic_name = generic_name ? g_strdup (generic_name) : NULL;
	widget_class->palette_name = palette_name ? g_strdup (palette_name) : NULL;
	widget_class->name = g_strdup (name);
	widget_class->in_palette = generic_name ? TRUE : FALSE;

	/* we can't just call g_type_from_name (name) to get the type, because
	 * that only works for registered types, and the only way to register the
	 * type is to call foo_bar_get_type() */
	init_function_name = glade_widget_class_compose_get_type_func (widget_class);
	if (!init_function_name)
	{
		g_warning (_("Not enough memory."));
		goto lblError;
	}

	widget_class->type = glade_util_get_type_from_name (init_function_name);
	if (widget_class->type == 0)
		goto lblError;

	widget_class->properties = glade_widget_class_list_properties (widget_class);
	widget_class->child_properties = glade_widget_class_list_child_properties (widget_class);
	widget_class->signals = glade_widget_class_list_signals (widget_class);

	/* is the widget a toplevel?  TODO: We're going away from this flag, and
	 * just doing this test each time that we want to know if it's a toplevel */
	if (g_type_is_a (widget_class->type, GTK_TYPE_WINDOW))
		GLADE_WIDGET_CLASS_SET_FLAGS (widget_class, GLADE_TOPLEVEL);

	/* is the widget a container?  TODO: We're going away from this flag, and
	 * just doing this test each time that we want to know if it's a container */
	if (g_type_is_a (widget_class->type, GTK_TYPE_CONTAINER))
		GLADE_WIDGET_CLASS_SET_FLAGS (widget_class, GLADE_ADD_PLACEHOLDER);

	widget_class->icon = glade_widget_class_create_icon (widget_class);

	g_free (init_function_name);

	for (parent_type = g_type_parent (widget_class->type);
	     parent_type != 0;
	     parent_type = g_type_parent (parent_type))
	{
		GladeWidgetClass *parent_class;

		parent_class = glade_widget_class_get_by_name (g_type_name (parent_type));
		if (parent_class)
			glade_widget_class_merge (widget_class, parent_class);
	}

	/* if there is an associated filename, then open and parse it */
	if (filename != NULL)
		glade_widget_class_extend_with_file (widget_class, filename);

	g_free (filename);

	/* store the GladeWidgetClass on the cache,
	 * if it's the first time we store a widget class, then
	 * initialize the global widget_classes hash.
	 */
	if (!widget_classes)
		widget_classes = g_hash_table_new (g_str_hash, g_str_equal);		

	g_hash_table_insert (widget_classes, widget_class->name, widget_class);

	return widget_class;

lblError:
	g_free (filename);
	g_free (init_function_name);
	glade_widget_class_free (widget_class);
	return NULL;
}

const gchar *
glade_widget_class_get_name (GladeWidgetClass *widget)
{
	return widget->name;
}

GType
glade_widget_class_get_type (GladeWidgetClass *widget)
{
	return widget->type;
}

/**
 * glade_widget_class_has_queries:
 * @class: 
 * 
 * A way to know if this class has a property that requires us to query the
 * user before creating the widget.
 * 
 * Return Value: TRUE if the GladeWidgetClass requires a query to the user
 *               before creationg.
 **/
gboolean
glade_widget_class_has_queries (GladeWidgetClass *class)
{
	GladePropertyClass *property_class;
	GList *list;

	for (list = class->properties; list; list = list->next)
	{
		property_class = list->data;
		if (property_class->query != NULL)
			return TRUE;
	}

	return FALSE;
}

gboolean
glade_widget_class_has_property (GladeWidgetClass *class, const gchar *name)
{
	GList *list;
	GladePropertyClass *pclass;

	for (list = class->properties; list; list = list->next)
	{
		pclass = list->data;
		if (strcmp (pclass->id, name) == 0)
			return TRUE;
	}

	return FALSE;
}

/**
 * glade_widget_class_dump_param_specs:
 * @class: 
 * 
 * Dump to the console the properties of the Widget as specified
 * by gtk+. You can also run glade3 with : "glade3 --dump GtkWindow" to
 * get dump a widget class properties.
 * 
 * Return Value: 
 **/
void
glade_widget_class_dump_param_specs (GladeWidgetClass *class)
{
	GObjectClass *object_class;
	GParamSpec **specs = NULL;
	GParamSpec *spec;
	GType last;
	gint n_specs = 0;
	gint i;

	g_return_if_fail (GLADE_IS_WIDGET_CLASS (class));

	g_type_class_ref (class->type); /* hmm */
	 /* We count on the fact we have an instance, or else we'd have
	  * touse g_type_class_ref ();
	  */

	object_class = g_type_class_peek (class->type);
	if (!object_class)
	{
		g_warning ("Class peek failed\n");
		return;
	}

	specs = g_object_class_list_properties (object_class, &n_specs);

	g_ok_print ("\nDumping ParamSpec for %s\n", class->name);

	last = 0;
	for (i = 0; i < n_specs; i++)
	{
		spec = specs[i];
		if (last != spec->owner_type)
			g_ok_print ("\n                    --  %s -- \n",
				 g_type_name (spec->owner_type));
		g_ok_print ("%02d - %-25s %-25s (%s)\n",
			 i,
			 spec->name,
			 g_type_name (spec->value_type),
			 (spec->flags & G_PARAM_WRITABLE) ? "Writable" : "ReadOnly");
		last = spec->owner_type;
	}
	g_ok_print ("\n");

	g_free (specs);
}

gboolean
glade_widget_class_is (GladeWidgetClass *class, const gchar *name)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	if (strcmp (class->name, name) == 0)
		return TRUE;

	return FALSE;
}

