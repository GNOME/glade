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
#include "glade-placeholder.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-project-window.h"
#include "glade-catalog.h"
#include "glade-choice.h"
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

	while (tmp->str[i]) {
		if (isupper (tmp->str[i])) {
			tmp = g_string_insert_c (tmp, i++, '_');

			j = 0;
			while (isupper (tmp->str[i++]))
				j++;

			if (j > 2) {
				g_string_insert_c (tmp, i-2, '_');
			}

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

	/* delete the list holding the properties */
	g_list_foreach (widget_class->properties, (GFunc) g_free, NULL);
	g_list_free (widget_class->properties);

	/* delete the list holding the child properties */
	g_list_foreach (widget_class->child_properties, (GFunc) g_free, NULL);
	g_list_free (widget_class->child_properties);

	/* delete the list holding the signals */
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
	if (object_class == NULL) {
		g_warning ("Class peek failed\n");
		return NULL;
	}

	specs = g_object_class_list_properties (object_class, &n_specs);

	last = 0;
	for (i = 0; i < n_specs; i++) {
		
		spec = specs[i];

		/* We only use the writable properties */
		if (spec->flags & G_PARAM_WRITABLE) {
			property_class = glade_property_class_new_from_spec (spec);

			if (property_class->type == GLADE_PROPERTY_TYPE_ERROR) {
				/* The property type is not supported.  That's not an error, as there are
				 * several standard properties that are not supposed to be edited through
				 * the palette (as the "attributes" property of a GtkLabel) */
				glade_property_class_free (property_class);
				property_class = NULL;
				continue;
			} else if (property_class->type == GLADE_PROPERTY_TYPE_OBJECT) {
				/* We don't support these properties */
				glade_property_class_free (property_class);
				property_class = NULL;
				continue;
			}

			/* should this if go into property_class_new_from_spec ? */
			if (!g_ascii_strcasecmp (g_type_name (spec->owner_type), "GtkWidget") &&
			    g_ascii_strcasecmp (spec->name, "name")) {
				property_class->common = TRUE;
			} else {
				property_class->common = FALSE;
			}

			property_class->optional = FALSE;
			property_class->update_signals = NULL;

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
	if (object_class == NULL) {
		g_warning ("Class peek failed\n");
		return NULL;
	}

	specs = gtk_container_class_list_child_properties (object_class, &n_specs);

#ifdef DEBUG
	g_print ("class %s has n child props: %d\n", class->name, n_specs);
#endif

	for (i = 0; i < n_specs; i++) {
		spec = specs[i];

		property_class = glade_property_class_new_from_spec (spec);

#ifdef DEBUG
		g_print ("child prop. name: %s, id: %s, type: %d\n", property_class->name, property_class->id, property_class->type);
#endif

		property_class->optional = FALSE;
		property_class->update_signals = NULL;
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
	while (g_type_is_a (type, GTK_TYPE_OBJECT)) { 
		if (G_TYPE_IS_INSTANTIATABLE (type) || G_TYPE_IS_INTERFACE (type)) {
			sig_ids = g_signal_list_ids (type, &num_signals);

			for (count = 0; count < num_signals; count++) {
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

	icon_path = g_strdup_printf (PIXMAPS_DIR "/%s.xpm", class->generic_name);
	icon = gtk_image_new_from_file (icon_path);
	g_free (icon_path);

	return icon;
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

	g_return_val_if_fail (filename != NULL, FALSE);

	context = glade_xml_context_new_from_path (filename, NULL, GLADE_TAG_GLADE_WIDGET_CLASS);
	if (!context)
		return FALSE;

	doc = glade_xml_context_get_doc (context);
	node = glade_xml_doc_get_root (doc);
	if (!doc || !node)
		return FALSE;

	replace_child_function_name = glade_xml_get_value_string (node, GLADE_TAG_REPLACE_CHILD_FUNCTION);
	if (replace_child_function_name && widget_class->module) {
		if (!g_module_symbol (widget_class->module, replace_child_function_name, (void **) &widget_class->replace_child))
			g_warning ("Could not find %s\n", replace_child_function_name);
	}
	g_free (replace_child_function_name);

	post_create_function_name = glade_xml_get_value_string (node, GLADE_TAG_POST_CREATE_FUNCTION);
	if (post_create_function_name && widget_class->module) {
		if (!g_module_symbol (widget_class->module, post_create_function_name, (void **) &widget_class->post_create_function))
			g_warning ("Could not find %s\n", post_create_function_name);
	}
	g_free (post_create_function_name);

	fill_empty_function_name = glade_xml_get_value_string (node, GLADE_TAG_FILL_EMPTY_FUNCTION);
	if (fill_empty_function_name && widget_class->module) {
		if (!g_module_symbol (widget_class->module, fill_empty_function_name, (void **) &widget_class->fill_empty))
			g_warning ("Could not find %s\n", fill_empty_function_name);
	}
	g_free (fill_empty_function_name);

	/* if we found a <properties> tag on the xml file, we add the properties
	 * that we read from the xml file to the class.
	 */
	properties = glade_xml_search_child (node, GLADE_TAG_PROPERTIES);
	if (properties)
		glade_property_class_list_add_from_node (properties, widget_class, &widget_class->properties);

	return TRUE;
}

/**
 * glade_widget_class_store_with_name:
 * @widget_class: widget class to store
 *
 * Store the GladeWidgetClass on the cache, indexed by its name
 **/
static void
glade_widget_class_store_with_name (GladeWidgetClass *widget_class)
{
	/* if it's the first time we store a widget class, then initialize the widget_classes hash */
	if (widget_classes == NULL)
		widget_classes = g_hash_table_new (g_str_hash, g_str_equal);		

	g_hash_table_insert (widget_classes, widget_class->name, widget_class);

	return;
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
 * Merges the contents of the parent_class on the widget_class.  It doesn't handles
 * the duplicated properties / signals.  I.e., if you have the same property / signal
 * on parent_class and on widget_class, widget_class will end having two times
 * this property.  You will have to remove the duplicates to ensure a consistent
 * widget_class.  The properties / signals of the parent_class will be appended to
 * those of widget_class.
 **/
static void
glade_widget_class_merge (GladeWidgetClass *widget_class, GladeWidgetClass *parent_class)
{
	GList *last_property;
	GList *parent_properties;

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

	last_property = g_list_last (widget_class->properties);
	parent_properties = parent_class->properties;
	for (; parent_properties; parent_properties = parent_properties->next) {
		GladePropertyClass *property_class = (GladePropertyClass*) parent_properties->data;
		GList *list = g_list_append (last_property, glade_property_class_clone (property_class));

		if (!last_property) {
			widget_class->properties = list;
			last_property = list;
		}
		else
			last_property = list->next;
	}
}

/**
 * glade_widget_class_remove_duplicated_properties:
 * @widget_class: class of the widget that contains the properties.
 *
 * Takes the list of properties of the widget class, and removes the duplicated properties.
 * The properties modified are prefered over the non modified ones, and the properties
 * that appear first on the list are prefered over these that appear later on the list.
 * (the first rule is more important than the second one).
 **/
static void
glade_widget_class_remove_duplicated_properties (GladeWidgetClass *widget_class)
{
	GHashTable *hash_properties = g_hash_table_new (g_str_hash, g_str_equal);
	GList *properties_classes = widget_class->properties;

	while (properties_classes != NULL) {
		GladePropertyClass *property_class = (GladePropertyClass*) properties_classes->data;
		GList *old_property;

		/* if it's the first time we see this property, then we add it to the list of 
		 * properties that we will keep for this widget.  Idem if the last time we saw
		 * this property, it was not modified, and this time the property is modified
		 * (ie, we change the non modified property by the modified one). */
		if ((old_property = g_hash_table_lookup (hash_properties, property_class->id)) == NULL ||
		    (!((GladePropertyClass*) old_property->data)->is_modified && property_class->is_modified))
		{
			/* remove the old property */
			if (old_property != NULL)
			{
				GList *new_head = widget_class->properties;

				if (old_property == widget_class->properties)
					new_head = g_list_next (old_property);

				g_list_remove_link (widget_class->properties, old_property);
				widget_class->properties = new_head;
			}

			g_hash_table_insert (hash_properties, property_class->id, properties_classes);
			properties_classes = g_list_next (properties_classes);
		} else {
			GList *tmp = properties_classes;
			GList *new_head = widget_class->properties;

			if (tmp == widget_class->properties)
				new_head = g_list_next (tmp);

			properties_classes = g_list_next (properties_classes);
			g_list_remove_link (widget_class->properties, tmp);
			widget_class->properties = new_head;
		}
	}

	g_hash_table_destroy (hash_properties);
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
			const char *base_filename,
			const char *base_library)
{
	GladeWidgetClass *widget_class = NULL;
	char *filename = NULL;
	char *library = NULL;
	char *init_function_name = NULL;
	GType parent_type;

	g_return_val_if_fail (name != NULL, NULL);

	if (glade_widget_class_get_by_name (name) != NULL) {
		g_warning ("The widget class [%s] has at least two different definitions.\n", name);
		goto lblError;
	}

	if (base_filename != NULL) {
		filename = g_strconcat (WIDGETS_DIR, "/", base_filename, NULL);
		if (filename == NULL) {
			g_warning (_("Not enough memory."));
			goto lblError;
		}
	}

	if (base_library != NULL) {
		library = g_strconcat (MODULES_DIR G_DIR_SEPARATOR_S, base_library, NULL);
		if (library == NULL) {
			g_warning (_("Not enough memory."));
			goto lblError;
		}
	}

	widget_class = g_new0 (GladeWidgetClass, 1);
	if (!widget_class) {
		g_warning (_("Not enough memory."));
		goto lblError;
	}

	widget_class->generic_name = generic_name ? g_strdup (generic_name) : NULL;
	widget_class->name = g_strdup (name);
	widget_class->in_palette = generic_name ? TRUE : FALSE;

	/* we can't just call g_type_from_name (name) to get the type, because
	 * that only works for registered types, and the only way to register the
	 * type is to call foo_bar_get_type() */
	init_function_name = glade_widget_class_compose_get_type_func (widget_class);
	if (!init_function_name) {
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

	if (library != NULL) {
		widget_class->module = g_module_open (library, G_MODULE_BIND_LAZY);
		if (!widget_class->module) {
			g_warning (_("Unable to open the module %s."), library);
			goto lblError;
		}
	}

	/* if there is an associated filename, then open and parse it */
	if (filename != NULL)
		glade_widget_class_extend_with_file (widget_class, filename);

	g_free (filename);
	g_free (init_function_name);

	for (parent_type = g_type_parent (widget_class->type);
	     parent_type != 0;
	     parent_type = g_type_parent (parent_type))
	{
		GladeWidgetClass *parent_class = glade_widget_class_get_by_name (g_type_name (parent_type));

		if (parent_class != NULL)
			glade_widget_class_merge (widget_class, parent_class);
	}

	/* remove the duplicate properties on widget_class */
	glade_widget_class_remove_duplicated_properties (widget_class);

	/* store the GladeWidgetClass on the cache */
	glade_widget_class_store_with_name (widget_class);

	return widget_class;

lblError:
	g_free (filename);
	g_free (library);
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

	for (list = class->properties; list; list = list->next) {
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

	for (list = class->properties; list; list = list->next) {
		pclass = list->data;
		if (strcmp (pclass->id, name) == 0)
			return TRUE;
	}

	return FALSE;
}

#if 0 // these two are just called in a commented out function in glade-property-class
static void
glade_widget_class_get_specs (GladeWidgetClass *class, GParamSpec ***specs, gint *n_specs)
{
	GObjectClass *object_class;
	GType type;

	type = glade_widget_class_get_type (class);
	g_type_class_ref (type); /* hmm */
	 /* We count on the fact we have an instance, or else we'd have
	  * touse g_type_class_ref ();
	  */
	object_class = g_type_class_peek (type);
	if (object_class == NULL) {
		g_warning ("Class peek failed\n");
		*specs = NULL;
		*n_specs = 0;
		return;
	}

	*specs = g_object_class_list_properties (object_class, n_specs);
}

GParamSpec *
glade_widget_class_find_spec (GladeWidgetClass *class, const gchar *name)
{
	GParamSpec **specs = NULL;
	GParamSpec *spec;
	gint n_specs = 0;
	gint i;

	glade_widget_class_get_specs (class, &specs, &n_specs);

	for (i = 0; i < n_specs; i++) {
		spec = specs[i];

		if (!spec || !spec->name) {
			g_warning ("Spec does not have a valid name, or invalid spec");
			g_free (specs);
			return NULL;
		}

		if (strcmp (spec->name, name) == 0) {
			GParamSpec *return_me;
			return_me = g_param_spec_ref (spec);
			g_free (specs);
			return return_me;
		}
	}

	g_free (specs);
	
	return NULL;
}
#endif

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
	if (object_class == NULL) {
		g_warning ("Class peek failed\n");
		return;
	}

	specs = g_object_class_list_properties (object_class, &n_specs);

	g_ok_print ("\nDumping ParamSpec for %s\n", class->name);

	last = 0;
	for (i = 0; i < n_specs; i++) {
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

#if 0 //we have another _get_by_name func... can this go away?
/**
 * glade_widget_class_get_by_name:
 * @name: 
 * 
 * Find a GladeWidgetClass with the name @name.
 * 
 * Return Value: 
 **/
GladeWidgetClass *
glade_widget_class_get_by_name (const gchar *name)
{
	GladeWidgetClass *class;
	GList *list;
	
	g_return_val_if_fail (name != NULL, NULL);

	list = glade_catalog_get_widgets ();
	for (; list; list = list->next) {
		class = list->data;
		g_return_val_if_fail (class->name != NULL, NULL);
		if (class->name == NULL)
			return NULL;
		if (strcmp (class->name, name) == 0)
			return class;
	}

	g_warning ("Class not found by name %s\n", name);
	
	return NULL;
}
#endif

gboolean
glade_widget_class_is (GladeWidgetClass *class, const gchar *name)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	if (strcmp (class->name, name) == 0)
		return TRUE;

	return FALSE;
}

