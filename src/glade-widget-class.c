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

#include <gtk/gtkenums.h> /* This should go away. Chema */
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "glade-placeholder.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-choice.h"
#include "glade-parameter.h"
#include "glade-widget-class.h"


static GladeWidgetClass *
glade_widget_class_new (void)
{
	GladeWidgetClass *widget;

	widget = g_new0 (GladeWidgetClass, 1);
	widget->flags = 0;
	widget->placeholder_replace = NULL;

	return widget;
}

static void
glade_widget_class_add_virtual_methods (GladeWidgetClass *class)
{
	g_return_if_fail (class->name != NULL);

	/* I don't love this. Make it better. Chema */
	if ((strcmp (class->name, "GtkVBox") == 0) ||
	    (strcmp (class->name, "GtkHBox") == 0))
		class->placeholder_replace = glade_placeholder_replace_box;
	if (strcmp (class->name, "GtkTable") == 0)
		class->placeholder_replace = glade_placeholder_replace_table;
	if (strcmp (class->name, "GtkWindow") == 0)
		class->placeholder_replace = glade_placeholder_replace_container;
	
	
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

	signals = NULL;
	/* FIXME: This should work. Apparently this is because you need to have an 
	 * instance of an object before you can get its type?!? need to fix this 
	 * to use class->type when bighead applys his patch. - shane
	 */
	type = g_type_from_name (class->name);

	g_return_val_if_fail (type != 0, NULL);

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

static GladeWidgetClass *
glade_widget_class_new_from_node (XmlParseContext *context, xmlNodePtr node)
{
	GladeWidgetClass *class;
	xmlNodePtr child;
	
	if (!glade_xml_node_verify (node, GLADE_TAG_GLADE_WIDGET_CLASS))
		return NULL;

	child = glade_xml_search_child_required (node, GLADE_TAG_PROPERTIES);
	if (child == NULL)
		return FALSE;

	class = glade_widget_class_new ();

	class->name         = glade_xml_get_value_string_required (node, GLADE_TAG_NAME, NULL);
	class->generic_name = glade_xml_get_value_string_required (node, GLADE_TAG_GENERIC_NAME, NULL);
	class->icon         = glade_xml_get_value_string_required (node, GLADE_TAG_ICON, NULL);
	class->properties   = glade_property_class_list_new_from_node (child, class);
	class->signals      = glade_widget_class_list_signals (class);

	if (!class->name ||
	    !class->icon ||
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
	
	widget = gtk_button_new ();

	full_path = g_strdup_printf (PIXMAPS_DIR "/%s.xpm", class->icon);
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
	GladeWidgetClass *widget;
	gchar *file_name;

	file_name = g_strconcat (WIDGETS_DIR, "/", name, ".xml", NULL);
	
	context = glade_xml_parse_context_new_from_path (file_name, NULL, GLADE_TAG_GLADE_WIDGET_CLASS);
	if (context == NULL)
		return NULL;
	widget = glade_widget_class_new_from_node (context, context->doc->children);
	glade_xml_parse_context_free (context);

	if (!glade_widget_class_create_pixmap (widget))
		return NULL;

	g_free (file_name);
	
	return widget;
}

const gchar *
glade_widget_class_get_name (GladeWidgetClass *widget)
{
	return widget->name;
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
