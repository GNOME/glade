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

/* For g_file_exists */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <config.h>

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

#if 0 /* Keep arround */
static gchar *
glade_widget_class_compose_get_type_func (GladeWidgetClass *class)
{
	gchar *retval;
	GString *tmp;
	gint i = 1;

	tmp = g_string_new (class->name);

	while (tmp->str[i]) {
		if (isupper (tmp->str[i])) {
			tmp = g_string_insert_c (tmp, i, '_');
			i+=2;
			continue;
		}
		i++;
	}

	retval = g_strconcat (g_strdup (tmp->str), "_get_type", NULL);
	g_strdown (retval);
	g_string_free (tmp, TRUE);

	return retval;
}
#endif

static GladeWidgetClass *
glade_widget_class_new (void)
{
	GladeWidgetClass *widget;

	widget = g_new0 (GladeWidgetClass, 1);
	widget->flags = 0;
	widget->placeholder_replace = NULL;
	widget->type = 0;

	return widget;
}

static void
glade_widget_class_add_virtual_methods (GladeWidgetClass *class)
{
	g_return_if_fail (class->name != NULL);

	if (GLADE_WIDGET_CLASS_ADD_PLACEHOLDER(class)) {
		/* I don't love this. Make it better. Chema */
#warning FIXME	
		if ((strcmp (class->name, "GtkVBox") == 0) ||
		    (strcmp (class->name, "GtkHBox") == 0))
			class->placeholder_replace = glade_placeholder_replace_box;
		if (strcmp (class->name, "GtkTable") == 0)
			class->placeholder_replace = glade_placeholder_replace_table;
		if (strcmp (class->name, "GtkNotebook") == 0)
			class->placeholder_replace = glade_placeholder_replace_notebook;
		if ((strcmp (class->name, "GtkWindow")    == 0) ||
		    (strcmp (class->name, "GtkFrame")     == 0) ||
		    (strcmp (class->name, "GtkHandleBox") == 0 ))
			class->placeholder_replace = glade_placeholder_replace_container;
		
		if (class->placeholder_replace == NULL)
			g_warning ("placeholder_replace has not been implemented for %s\n",
				   class->name);
	}
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
		}

		type = g_type_parent (type);
	}

	return signals;
}

static gboolean
glade_widget_class_set_type (GladeWidgetClass *class, const gchar *init_function_name)
{
	static GModule *allsymbols;
	guint (*get_type) ();
	GType type;

	class->type = 0;

	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), FALSE);
	g_return_val_if_fail (init_function_name != NULL, FALSE);
	
	if (!allsymbols)
		allsymbols = g_module_open (NULL, 0);
	
	if (!g_module_symbol (allsymbols, init_function_name,
			      (gpointer) &get_type)) {
		g_warning (_("We could not find the symbol \"%s\" while trying to load \"%s\""),
			   init_function_name, class->name);
		return FALSE;
	}

	g_assert (get_type);
	type = get_type ();

	if (type == 0) {
		g_warning(_("Could not get the type from \"%s\" while trying to load \"%s\""), class->name);
		return FALSE;
	}

	if (!g_type_is_a (type, gtk_widget_get_type ())) {
		g_warning (_("The loaded type is not a GtkWidget, while trying to load \"%s\""),
			   class->name);
		return FALSE;
	}

	class->type = type;
	
	return TRUE;
}
		
static GladeWidgetClass *
glade_widget_class_new_from_node (XmlParseContext *context, xmlNodePtr node)
{
	GladeWidgetClass *class;
	xmlNodePtr child;
	gchar *init_function_name;

	if (!glade_xml_node_verify (node, GLADE_TAG_GLADE_WIDGET_CLASS))
		return NULL;

	child = glade_xml_search_child_required (node, GLADE_TAG_PROPERTIES);
	if (child == NULL)
		return FALSE;

	class = glade_widget_class_new ();

	class->name         = glade_xml_get_value_string_required (node, GLADE_TAG_NAME, NULL);
	class->generic_name = glade_xml_get_value_string_required (node, GLADE_TAG_GENERIC_NAME, NULL);

	init_function_name = glade_xml_get_value_string_required (node, GLADE_TAG_GET_TYPE_FUNCTION, NULL);
	if (!init_function_name)
		return FALSE;
	if (!glade_widget_class_set_type (class, init_function_name))
		return NULL;
	g_free (init_function_name);

	class->properties   = glade_property_class_list_new_from_node (child, class);
	class->signals      = glade_widget_class_list_signals (class);

	if (!class->name ||
	    !class->generic_name) {
		g_warning ("Invalid XML file. Widget Class %s\n", class->name);
		return NULL;
	}
	
	/* Get the flags */
	if (glade_xml_get_boolean (node, GLADE_TAG_TOPLEVEL))
		GLADE_WIDGET_CLASS_SET_FLAGS (class, GLADE_TOPLEVEL);
	else
		GLADE_WIDGET_CLASS_UNSET_FLAGS (class, GLADE_TOPLEVEL);
	if (glade_xml_get_boolean (node, GLADE_TAG_PLACEHOLDER))
		GLADE_WIDGET_CLASS_SET_FLAGS (class, GLADE_ADD_PLACEHOLDER);
	else
		GLADE_WIDGET_CLASS_UNSET_FLAGS (class, GLADE_ADD_PLACEHOLDER);

	glade_widget_class_add_virtual_methods (class);
	
	return class;
}

static gboolean
glade_widget_class_create_pixmap (GladeWidgetClass *class)
{
	struct stat s;
	GtkWidget *widget;
	gchar *full_path;

	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), FALSE);
	
	widget = gtk_button_new ();

	full_path = g_strdup_printf (PIXMAPS_DIR "/%s.xpm", class->generic_name);
	if (stat (full_path, &s) != 0) {
		g_warning ("Could not create a the \"%s\" GladeWidgetClass because \"%s\" does not exist",
			   class->name, full_path);
		return FALSE;
	}

	class->pixbuf = gdk_pixbuf_new_from_file (full_path, NULL);

	class->pixmap = gdk_pixmap_colormap_create_from_xpm (NULL, gtk_widget_get_colormap (GTK_WIDGET (widget)),
							     &class->mask, NULL, full_path);

	gtk_widget_unref (widget);
	g_free (full_path);
	
	return TRUE;
}

GladeWidgetClass *
glade_widget_class_new_from_name (const gchar *name)
{
	XmlParseContext *context;
	GladeWidgetClass *class;
	gchar *file_name;

	file_name = g_strconcat (WIDGETS_DIR, "/", name, ".xml", NULL);
	
	context = glade_xml_parse_context_new_from_path (file_name, NULL, GLADE_TAG_GLADE_WIDGET_CLASS);
	if (context == NULL)
		return NULL;
	class = glade_widget_class_new_from_node (context, context->doc->children);
	glade_xml_parse_context_free (context);

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

void
glade_widget_class_create (GladeWidgetClass *glade_widget)
{
	GtkWidget *widget;

	widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_widget_show (widget);
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

#if 0
	g_print ("Dumping specs for %s\n\n", class->name);
	for (i = 0; i < n_specs; i++) {
		spec = specs[i];
		g_print ("%02d - %s\n", i, spec->name); 
	}
#endif	
	
	for (i = 0; i < n_specs; i++) {
		spec = specs[i];

		if (!spec || !spec->name) {
			g_warning ("Spec does not have a valid name, or invalid spec");
			return NULL;
		}

		if (strcmp (spec->name, name) == 0)
			return spec;
	}

	return NULL;
}
			
void
glade_widget_class_dump_param_specs (GladeWidgetClass *class)
{
	GParamSpec **specs = NULL;
	GParamSpec *spec;
	gint n_specs = 0;
	gint i;

	glade_widget_class_get_specs (class, &specs, &n_specs);

	g_print ("Dumping ParamSpec for %s\n", class->name);
	
	for (i = 0; i < n_specs; i++) {
		spec = specs[i];
		g_print ("%02d - %s\n", i, spec->name); 
	}
}


GladeWidgetClass *
glade_widget_class_get_by_name (const gchar *name)
{
	GladeProjectWindow *gpw;
	GladeWidgetClass *class;
	GList *list;
	
	g_return_val_if_fail (name != NULL, NULL);

	gpw = glade_project_window_get ();
	list = gpw->catalog->widgets;
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

