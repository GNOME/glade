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

#include <string.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-placeholder.h"
#include "glade-project.h"
#include "glade-project-window.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-placeholder.h"

GladeWidget *
glade_widget_get_by_name (GladeProject *project, const gchar *name)
{
	GladeWidget *widget;
	GList *list;

	g_return_val_if_fail (name != NULL, NULL);
	
	list = project->widgets;
	
	for (; list != NULL; list = list->next) {
		widget = list->data;
		g_return_val_if_fail (widget->name != NULL, NULL);
		if (strcmp (widget->name, name) == 0)
			return widget;
	}

	return NULL;
}

gchar *
glade_widget_new_name (GladeProject *project, GladeWidgetClass *class)
{
	gint i = 1;
	gchar *name;

	while (TRUE) {
		name = g_strdup_printf ("%s%i", class->generic_name, i);
		if (glade_widget_get_by_name (project, name) == NULL)
			return name;
		g_free (name);
		i++;
	}

}

static GladeWidget *
glade_widget_new (GladeProject *project, GladeWidgetClass *class, GtkWidget *gtk_widget, const gchar *name)
{
	GladeWidget *widget;

	widget = g_new0 (GladeWidget, 1);
	widget->name = g_strdup (name);
	widget->widget = gtk_widget;
	widget->project = project;
	widget->class = class;
	widget->properties = glade_property_list_new_from_widget_class (class, widget);
	widget->parent = NULL;
	widget->children = NULL;

	return widget;
}


static GladeWidget *
glade_widget_register (GladeProject *project, GladeWidgetClass *class, GtkWidget *gtk_widget, const gchar *name, GladeWidget *parent)
{
	GladeWidget *glade_widget;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

	glade_widget = glade_widget_new (project, class, gtk_widget, name);

	glade_widget->parent = parent;
	if (parent)
		parent->children = g_list_prepend (parent->children, glade_widget);

	
	return glade_widget;
}

static GladeWidget *
glade_widget_class_create_glade_widget (GladeProject *project,
					GladeWidgetClass *class,
					GladeWidget *parent)
{
	GladeWidget *glade_widget;
	GtkWidget *widget;
	gchar *name;

	/* There has to be a better way of doing this with gtk */
	name = glade_widget_new_name (project, class);

#warning HACK HACK !!! ;-/
	if (strcmp (class->name, "GtkWindow") == 0) {
		widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	} else if (strcmp (class->name, "GtkLabel") == 0) {
		widget = gtk_label_new (name);
	} else if (strcmp (class->name, "GtkVBox") == 0) {
		widget = gtk_vbox_new (FALSE, 0);
	} else if (strcmp (class->name, "GtkHBox") == 0) {
		widget = gtk_hbox_new (FALSE, 0);
	} else if (strcmp (class->name, "GtkButton") == 0) {
		widget = gtk_button_new_with_label (name);
	} else if (strcmp (class->name, "GtkTable") == 0) {
		widget = gtk_table_new (0, 0, FALSE);
	} else {
		gchar *text;
		text = g_strdup_printf ("Error, class_new_widget not implemented [%s]\n", class->name);
		widget = gtk_label_new (text);
		g_free (text);
	}

	glade_widget = glade_widget_register (project, class, widget, name, parent);

	if ((strcmp (class->name, "GtkLabel") == 0) ||
	    (strcmp (class->name, "GtkButton") == 0)) {
		GladeProperty *property;
		
		property = glade_property_get_from_name (glade_widget->properties,
							 "Label");
		if (property != NULL)
			glade_property_changed_text (property, name);
		else
			g_warning ("Could not set the label to the widget name\n");
	}
#warning End of hack 
	
	glade_project_add_widget (project, glade_widget);
	
	g_free (name);
	
	return glade_widget;
}

/**
 * glade_widget_new_from_class:
 * @project: 
 * @class:
 * @parent: th parent of the new widget, should be NULL for toplevel widgets
 * 
 * Creates a new GladeWidget from a given class. Takes care of registering
 * the widget in the project, adding it to the views and quering the user
 * if necesary.
 * 
 * Return Value: A newly creatred GladeWidget, NULL on user cancel or error	
 **/
GladeWidget *
glade_widget_new_from_class (GladeProject *project, GladeWidgetClass *class, GladeWidget *parent)
{
	GladePropertyQueryResult *result = NULL;
	GladeWidget *glade_widget;

	g_return_val_if_fail (project != NULL, NULL);

	if (glade_widget_class_has_queries (class)) {
		result = glade_property_query_result_new ();
		if (glade_project_window_query_properties (class, result))
			return NULL;
	}

	glade_widget = glade_widget_class_create_glade_widget (project, class, parent);

	if (GLADE_WIDGET_CLASS_ADD_PLACEHOLDER (class))
		glade_placeholder_add (class, glade_widget, result);

	gtk_widget_show (glade_widget->widget);
	
	if (result) 
		glade_property_query_result_destroy (result);

	return glade_widget;
}
	

const gchar *
glade_widget_get_name (GladeWidget *widget)
{
	return widget->name;
}
		
GladeWidgetClass *
glade_widget_get_class (GladeWidget *widget)
{
	return widget->class;
}


/**
 * glade_widget_get_property_from_class:
 * @widget: 
 * @property_class: 
 * 
 * Given a glade Widget, it returns the property that correspons to @property_class
 * 
 * Return Value: 
 **/
GladeProperty *
glade_widget_get_property_from_class (GladeWidget *widget,
				      GladePropertyClass *property_class)
{
	GladeProperty *property;
	GList *list;
	
	list = widget->properties;
	for (; list != NULL; list = list->next) {
		property = list->data;
		if (property->class == property_class)
			break;
	}
	if (list == NULL) {
		g_warning ("Could not find the GladeProperty to load\n");
		return NULL;
	}

	return property;
}


/**
 * glade_widget_set_name:
 * @widget: 
 * @name: 
 * 
 * Sets the name of a widget
 **/
void
glade_widget_set_name (GladeWidget *widget, const gchar *name)
{
	if (widget->name)
		g_free (widget->name);
	widget->name = g_strdup (name);

	glade_project_widget_name_changed (widget->project, widget);
}
