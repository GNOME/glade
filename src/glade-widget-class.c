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
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "glade.h"
#include "glade-xml-utils.h"

#include <dirent.h>
#include <gmodule.h>
#include <ctype.h>

#include <gtk/gtkenums.h> /* This should go away. Chema */
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "glade-placeholder.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-project-window.h"
#include "glade-catalog.h"
#include "glade-choice.h"
#include "glade-parameter.h"
#include "glade-widget-class.h"
#include "glade-packing.h"


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

	retval = g_strconcat (tmp->str, "_get_type", NULL);
	g_strdown (retval);
	g_string_free (tmp, TRUE);

	return retval;
}


static GladeWidgetClass *
glade_widget_class_new (void)
{
	GladeWidgetClass *class;

	class = g_new0 (GladeWidgetClass, 1);
	class->flags = 0;
	class->placeholder_replace = NULL;
	class->type = 0;
	class->properties = NULL;

	return class;
}

static void
glade_widget_class_add_virtual_methods (GladeWidgetClass *class)
{
	g_return_if_fail (class->name != NULL);

	if (GLADE_WIDGET_CLASS_ADD_PLACEHOLDER(class))
		glade_placeholder_add_methods_to_class (class);
}

GList * 
glade_widget_class_list_signals (GladeWidgetClass *class) 
{
	GList *signals;
	GType type;
	guint count;
	guint *sig_ids;
	guint num_signals;
	GladeWidgetClassSignal *cur;

	g_return_val_if_fail (class->type != 0, NULL);

	signals = NULL;
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

static gboolean
glade_widget_class_set_type (GladeWidgetClass *class, const gchar *init_function_name)
{
	GType type;

	class->type = 0;

	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), FALSE);
	g_return_val_if_fail (init_function_name != NULL, FALSE);

	type = glade_util_get_type_from_name (init_function_name);
	if (type == 0)
		return FALSE;

	class->type = type;

	if (type == 0)
		return FALSE;
	
	return TRUE;
}

		
GladeWidgetClass *
glade_widget_class_new_from_node (GladeXmlNode *node)
{
	GladeWidgetClass *class;
	GladeXmlNode *child;
	gchar *init_function_name;

	if (!glade_xml_node_verify (node, GLADE_TAG_GLADE_WIDGET_CLASS))
		return NULL;

	class = glade_widget_class_new ();

	class->name         = glade_xml_get_value_string_required (node, GLADE_TAG_NAME, NULL);
	class->generic_name = glade_xml_get_value_string_required (node, GLADE_TAG_GENERIC_NAME, NULL);

	if (!class->name ||
	    !class->generic_name) {
		g_warning ("Invalid XML file. Widget Class %s\n", class->name);
		return NULL;
	}
	
	init_function_name  = glade_xml_get_value_string (node, GLADE_TAG_GET_TYPE_FUNCTION);
	if (!init_function_name) {
		init_function_name = glade_widget_class_compose_get_type_func (class);
		if (!init_function_name)
			return FALSE;
	}

	if (!glade_widget_class_set_type (class, init_function_name))
		return NULL;
	g_free (init_function_name);


	/* <Properties> */
	child = glade_xml_search_child_required (node, GLADE_TAG_PROPERTIES);
	if (child == NULL)
		return FALSE;
	
	class->properties = glade_property_class_list_properties (class);
	glade_property_class_list_add_from_node (child, class, &class->properties);

	class->signals    = glade_widget_class_list_signals (class);

	
	/* Get the flags */
	if (glade_xml_get_boolean (node, GLADE_TAG_TOPLEVEL, FALSE))
		GLADE_WIDGET_CLASS_SET_FLAGS (class, GLADE_TOPLEVEL);
	else
		GLADE_WIDGET_CLASS_UNSET_FLAGS (class, GLADE_TOPLEVEL);
	if (glade_xml_get_boolean (node, GLADE_TAG_PLACEHOLDER, FALSE))
		GLADE_WIDGET_CLASS_SET_FLAGS (class, GLADE_ADD_PLACEHOLDER);
	else
		GLADE_WIDGET_CLASS_UNSET_FLAGS (class, GLADE_ADD_PLACEHOLDER);

	/* <PostCreateFunction> */
	class->post_create_function = glade_xml_get_value_string (node, GLADE_TAG_POST_CREATE_FUNCTION);

	glade_widget_class_add_virtual_methods (class);
	
	return class;
}

static gboolean
glade_widget_class_create_pixmap (GladeWidgetClass *class)
{
	struct stat s;
	GtkWidget *widget;
	gchar *full_path;
	gchar *default_path;

	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), FALSE);
	
	widget = gtk_button_new ();

	full_path = g_strdup_printf (PIXMAPS_DIR "/%s.xpm", class->generic_name);
	if (stat (full_path, &s) != 0) {
		default_path = g_strdup (PIXMAPS_DIR "/custom.xpm");
		if (stat (default_path, &s) != 0) {
			g_warning ("Could not create a the \"%s\" GladeWidgetClass because \"%s\" does not exist", class->name, full_path);
			g_free (full_path);
			g_free (default_path);
			return FALSE;
		} else {
			g_free (full_path);
			full_path = default_path;
		}
	}

	class->pixbuf = gdk_pixbuf_new_from_file (full_path, NULL);

	class->pixmap = gdk_pixmap_colormap_create_from_xpm (NULL, gtk_widget_get_colormap (GTK_WIDGET (widget)),
							     &class->mask, NULL, full_path);

	/* This one is a special case, we create a widget and not add it
	 * in any container so its float reference is never removed, so
	 * to destroy de object we only need to remove this floating ref
	 */
	gtk_object_sink (GTK_OBJECT(widget));
/*	gtk_widget_unref (widget);*/
	g_free (full_path);
	
	return TRUE;
}

GladeWidgetClass *
glade_widget_class_new_from_name (const gchar *name)
{
	GladeWidgetClass *class;
	GladeXmlContext *context;
	GladeXmlDoc *doc;
	gchar *file_name;

	file_name = g_strconcat (WIDGETS_DIR, "/", name, ".xml", NULL);
	
	context = glade_xml_context_new_from_path (file_name, NULL, GLADE_TAG_GLADE_WIDGET_CLASS);
	if (context == NULL)
		return NULL;
	doc = glade_xml_context_get_doc (context);
	class = glade_widget_class_new_from_node (glade_xml_doc_get_root (doc));
	class->xml_file = g_strdup (name);
	glade_xml_context_free (context);

	if (!glade_widget_class_create_pixmap (class))
		return NULL;

	g_free (file_name);
	
	return class;
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

	list = class->properties;
	for (; list != NULL; list = list->next) {
		property_class = list->data;
		if (property_class->query != NULL)
			return TRUE;
	}

	return FALSE;
}


/* ParamSpec stuff */
void
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
/**
 * glade_widget_class_dump_param_specs:
 * @class: 
 * 
 * Dump to the console the properties of the Widget as specified
 * by gtk+. You can also run glade2 with : "glade2 --dump GtkWindow" to
 * get dump a widget class properties.
 * 
 * Return Value: 
 **/
void
glade_widget_class_dump_param_specs (GladeWidgetClass *class)
{
	GParamSpec **specs = NULL;
	GParamSpec *spec;
	GType last;
	gint n_specs = 0;
	gint i;

	glade_widget_class_get_specs (class, &specs, &n_specs);

	g_ok_print ("\nDumping ParamSpec for %s\n", class->name);

	last = 0;
	for (i = 0; i < n_specs; i++) {
		spec = specs[i];
		if (last != spec->owner_type)
			g_ok_print ("\n                    --  %s -- \n",
				 g_type_name (spec->owner_type));
		g_ok_print ("%02d - %-25s %s\n",
			 i,
			 spec->name,
			 g_type_name (spec->value_type));
		last = spec->owner_type;
	}
	g_ok_print ("\n");
}


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
	for (; list != NULL; list = list->next) {
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


gboolean
glade_widget_class_is (GladeWidgetClass *class, const gchar *name)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), FALSE);
	g_return_val_if_fail (name != NULL, FALSE);

	if (strcmp (class->name, name) == 0)
		return TRUE;

	return FALSE;
}

static void
glade_widget_class_load_packing_properties_from_node (GladeXmlNode *node,
						      GladeWidgetClass *class)
{
	GladeXmlNode *child;
	
	if (!glade_xml_node_verify (node, GLADE_TAG_GLADE_WIDGET_CLASS))
		return;

	child = glade_xml_search_child (node, GLADE_TAG_PACKING_PROPERTIES);
	if (!child)
		return;
	
	child = glade_xml_node_get_children (child);
	for (; child; child = glade_xml_node_next (child)) {
		GladePackingProperties *properties;
		GladeWidgetClass *container_class;
		GladeXmlNode *child2;
		GHashTable *hash;
		gchar *container_name;

		hash = g_hash_table_new (g_str_hash, g_str_equal);
		
		child2 = glade_xml_node_get_children (child);
		for (; child2; child2 = glade_xml_node_next (child2)) {
			gchar *container_id;
			gchar *content;
			
			container_id = glade_xml_get_property_string (child2,
								      GLADE_TAG_ID);
			content = glade_xml_get_content (child2);
			g_hash_table_insert (hash, container_id, content);
		}

		container_name = glade_xml_get_property_string (child,
								GLADE_TAG_ID);
		container_class = glade_widget_class_get_by_name (container_name);
		if (container_class == NULL) {
			g_warning ("Could not find GladeWidget for %s\n", container_name);
			return;
		}
		g_free (container_name);
		
		properties = g_new0 (GladePackingProperties, 1);
		properties->container_class = container_class;
		properties->properties = hash;
		class->packing_properties = g_list_prepend (class->packing_properties, properties);
	}
	
}

void
glade_widget_class_load_packing_properties (GladeWidgetClass *class)
{
	GladeXmlContext *context;
	GladeXmlDoc *doc;
	gchar *file_name;

	file_name = g_strconcat (WIDGETS_DIR, "/", class->xml_file, ".xml", NULL);
	
	context = glade_xml_context_new_from_path (file_name, NULL, GLADE_TAG_GLADE_WIDGET_CLASS);
	if (context == NULL)
		return;
	doc = glade_xml_context_get_doc (context);
	glade_widget_class_load_packing_properties_from_node (glade_xml_doc_get_root (doc), class);
	glade_xml_context_free (context);

	g_free (file_name);
}
