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

#include <config.h>
#include <string.h>

#include "glade.h"
#include "glade-placeholder.h"
#include "glade-cursor.h"
#include "glade-widget-class.h"
#include "glade-widget.h"
#include "glade-property.h"
#include "glade-project.h"
#include "glade-project-window.h"

#define GLADE_PLACEHOLDER_ROW_STRING "GladePlaceholderRow"
#define GLADE_PLACEHOLDER_COL_STRING "GladePlaceholderColumn"

void
glade_placeholder_replace_box (GladePlaceholder *placeholder,
			       GladeWidget *widget,
			       GladeWidget *parent)
{
	/* Some Gtk Hackery. Not beautifull but needed. */
	GtkBoxChild *child_info = NULL;
	GtkWidget *child;
	GtkBox *box;
	GList *list;
		
	box = GTK_BOX (parent->widget);

	list = box->children;
	for (; list != NULL; list = list->next) {
		child_info = list->data;
		if (child_info->widget == placeholder)
			break;
	}
	if (list == NULL) {
		g_warning ("Error while adding a widget to a GtkBox\n");
		return;
	}
	gtk_widget_unparent (child_info->widget);
	child_info->widget = widget->widget;
	gtk_widget_set_parent (child_info->widget, GTK_WIDGET (box));

	/* */
	child = widget->widget;
	if (GTK_WIDGET_REALIZED (box))
		gtk_widget_realize (child);
	if (GTK_WIDGET_VISIBLE (box) && GTK_WIDGET_VISIBLE (child)) {
		if (GTK_WIDGET_MAPPED (box))
			gtk_widget_map (child);
		gtk_widget_queue_resize (child);
	}
	/* */
}

void
glade_placeholder_replace_table (GladePlaceholder *placeholder,
				 GladeWidget *widget,
				 GladeWidget *parent)
{
	/* Some Gtk Hackery. Not beautifull, but needed */
	GtkTableChild *table_child = NULL;
	GtkWidget *child;
	GtkTable *table;
	GList *list;

	table = GTK_TABLE (parent->widget);
	list = table->children;
		
	for (; list != NULL; list = list->next) {
		table_child = list->data;
		if (table_child->widget == placeholder)
			break;
	}
	if (list == NULL) {
		g_warning ("Error while adding a widget to a GtkTable\n");
		return;
	}
	
	gtk_widget_unparent (table_child->widget);
	table_child->widget = widget->widget;
	gtk_widget_set_parent (table_child->widget, GTK_WIDGET (table));
	
	/* */
	child = widget->widget;
	if (GTK_WIDGET_REALIZED (child->parent))
		gtk_widget_realize (child);
	if (GTK_WIDGET_VISIBLE (child->parent) && GTK_WIDGET_VISIBLE (child)) {
		if (GTK_WIDGET_MAPPED (child->parent))
			gtk_widget_map (child);
		gtk_widget_queue_resize (child);
	}
	/* */
}

void
glade_placeholder_replace_container (GladePlaceholder *placeholder,
				     GladeWidget *widget,
				     GladeWidget *parent)
{
	gtk_container_remove (GTK_CONTAINER (parent->widget), placeholder);
	gtk_container_add (GTK_CONTAINER (parent->widget), widget->widget);
}

void
glade_placeholder_replace_notebook (GladePlaceholder *placeholder,
				    GladeWidget *widget,
				    GladeWidget *parent)
{
	GtkNotebook *notebook;
	GtkWidget *page;
	GtkWidget *label;
	gint page_num;

	notebook = GTK_NOTEBOOK (parent->widget);
	page_num = gtk_notebook_page_num (notebook,  GTK_WIDGET (placeholder));
	if (page_num == -1) {
		g_warning ("GtkNotebookPage not found\n");
		return;
	}

	page = gtk_notebook_get_nth_page (notebook, page_num);
	label = gtk_notebook_get_tab_label (notebook, GTK_WIDGET (placeholder));
	
	gtk_widget_ref (page);
	gtk_widget_ref (label);

	gtk_notebook_remove_page (notebook, page_num);
	gtk_notebook_insert_page (notebook, widget->widget,
				  label, page_num);
	gtk_notebook_set_tab_label (notebook,
				    widget->widget,
				    label);
	
	gtk_widget_unref (label);
	gtk_widget_unref (page);

	gtk_notebook_set_current_page (notebook, page_num);
}

static void
glade_placeholder_replace_widget (GladePlaceholder *placeholder, GladeWidgetClass *class, GladeProject *project)
{
	GladeWidget *parent;
	GladeWidget *widget;

	parent = glade_placeholder_get_parent (placeholder);
	g_return_if_fail (parent != NULL);

	widget = glade_widget_new_from_class (class, parent);
	if (widget == NULL)
		return;

	if (parent->class->placeholder_replace != NULL)
		parent->class->placeholder_replace (placeholder, widget, parent);
	else
		g_warning ("A widget was added to a placeholder, but the placeholder_replace "
			   "function has not been implemented for this class (%s)\n",
			   class->name);

	glade_project_selection_set (widget, TRUE);
}

static void
glade_placeholder_on_button_press_event (GladePlaceholder *placeholder, GdkEventButton *event, GladeProject *project)
{
	GladeProjectWindow *gpw;

	gpw = glade_project_window_get ();
	
	if (event->button == 1 && event->type == GDK_BUTTON_PRESS && gpw->add_class != NULL) {
		glade_placeholder_replace_widget (placeholder, gpw->add_class, project);
		gpw->add_class = NULL;
	}
			
}

static void
glade_placeholder_on_motion_notify_event (GladePlaceholder *placeholder, GdkEventMotion *event, gpointer not_used)
{
	GladeProjectWindow *gpw;

	gpw = glade_project_window_get ();

	if (gpw->add_class == NULL)
		glade_cursor_set (event->window, GLADE_CURSOR_SELECTOR);
	else
		glade_cursor_set (event->window, GLADE_CURSOR_ADD_WIDGET);
}


static void
glade_placeholder_on_expose_event (GladePlaceholder *placeholder,
				   GdkEventExpose *event,
				   gpointer not_used)
{
	GtkWidget *widget;
	GdkGC *light_gc;
	GdkGC *dark_gc;
	gint w, h;

	widget = GTK_WIDGET (placeholder);
	
	light_gc = widget->style->light_gc [GTK_STATE_NORMAL];
	dark_gc  = widget->style->dark_gc  [GTK_STATE_NORMAL];
	gdk_window_get_size (widget->window, &w, &h);

	gdk_draw_line (widget->window, light_gc, 0, 0, w - 1, 0);
	gdk_draw_line (widget->window, light_gc, 0, 0, 0, h - 1);
	gdk_draw_line (widget->window, dark_gc, 0, h - 1, w - 1, h - 1);
	gdk_draw_line (widget->window, dark_gc, w - 1, 0, w - 1, h - 1);
}

static void
glade_placeholder_on_realize (GladePlaceholder *placeholder, gpointer not_used)
{
	static GdkPixmap *pixmap = NULL;

	if (pixmap == NULL) {
		gchar *file;
		file = g_strdup (PIXMAPS_DIR "/placeholder.xpm");
		pixmap = gdk_pixmap_colormap_create_from_xpm (NULL,
							      gtk_widget_get_colormap (GTK_WIDGET (placeholder)),
							      NULL, NULL, file);
		g_free (file);
	}
		
	if (pixmap == NULL) {
		g_warning ("Could not create pixmap for the glade-placeholder");
		return;
	}
	
	gdk_window_set_back_pixmap (GTK_WIDGET (placeholder)->window, pixmap, FALSE);
}

static void
glade_placeholder_connect_mouse_signals (GladePlaceholder *placeholder,
					 GladeProject *project)
{
	gtk_signal_connect (GTK_OBJECT (placeholder), "motion_notify_event",
			    GTK_SIGNAL_FUNC (glade_placeholder_on_motion_notify_event), NULL);
	gtk_signal_connect (GTK_OBJECT (placeholder), "button_press_event",
			    GTK_SIGNAL_FUNC (glade_placeholder_on_button_press_event), project);
}
static void
glade_placeholder_connect_draw_signals (GladePlaceholder *placeholder)
{
	gtk_signal_connect_after (GTK_OBJECT (placeholder), "realize",
				  GTK_SIGNAL_FUNC (glade_placeholder_on_realize), NULL);
	gtk_signal_connect_after (GTK_OBJECT (placeholder), "expose_event",
				  GTK_SIGNAL_FUNC (glade_placeholder_on_expose_event), NULL);
}

static void
glade_placeholder_on_destroy (GladePlaceholder *widget, gpointer not_used)
{
}

static GladePlaceholder *
glade_placeholder_new (GladeWidget *glade_widget)
{
	GladePlaceholder *placeholder;

	placeholder = gtk_drawing_area_new ();
	gtk_object_set_user_data (GTK_OBJECT (placeholder), glade_widget);

	gtk_widget_set_events (GTK_WIDGET (placeholder),
			       gtk_widget_get_events (GTK_WIDGET (placeholder))
			       | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
			       | GDK_BUTTON_RELEASE_MASK
			       | GDK_POINTER_MOTION_MASK | GDK_BUTTON1_MOTION_MASK);

	gtk_widget_show (GTK_WIDGET (placeholder));
	
	glade_placeholder_connect_draw_signals  (placeholder);
	glade_placeholder_connect_mouse_signals (placeholder, glade_widget->project);
	
	gtk_signal_connect (GTK_OBJECT (placeholder), "destroy",
			    GTK_SIGNAL_FUNC (glade_placeholder_on_destroy), NULL);
	
	return placeholder;
}


void
glade_placeholder_add (GladeWidgetClass *class,
		       GladeWidget *widget,
		       GladePropertyQueryResult *result)
{
	GladePlaceholder *placeholder;

	g_return_if_fail (class != NULL);
	g_return_if_fail (widget != NULL);

	if (GLADE_WIDGET_CLASS_TOPLEVEL (class)) {
		placeholder = glade_placeholder_new (widget);
		gtk_container_add (GTK_CONTAINER (widget->widget), GTK_WIDGET (placeholder));
		return;
	}

	if ((strcmp (class->name, "GtkFrame") == 0) ||
	    (strcmp (class->name, "GtkHandleBox") == 0)) {
		placeholder = glade_placeholder_new (widget);
		gtk_container_add (GTK_CONTAINER (widget->widget),
				   placeholder);
		return;
	}
	
	if ((strcmp (class->name, "GtkVBox") == 0) ||
	    (strcmp (class->name, "GtkHBox") == 0)) {
		gint i;
		gint size;
		
		glade_property_query_result_get_int (result, "size", &size);

		for (i = 0; i < size; i++) {
			placeholder = glade_placeholder_new (widget);
			gtk_box_pack_start_defaults (GTK_BOX (widget->widget),
						     GTK_WIDGET (placeholder));
		}
		return;
	}
	
	if ((strcmp (class->name, "GtkTable") == 0)) {
		gint row, col;
		gint rows = 6, cols = 6;

		glade_property_query_result_get_int (result, "n-rows", &rows);
		glade_property_query_result_get_int (result, "n-columns", &cols);

		for (row = 0; row < rows; row++) {
			for (col = 0; col < cols; col++) {
				placeholder = glade_placeholder_new (widget);
				gtk_table_attach_defaults (GTK_TABLE (widget->widget),
							   GTK_WIDGET (placeholder),
							   col, col+1,
							   row, row+1);
			}
		}
		return;
	}


	if ((strcmp (class->name, "GtkNotebook") == 0)) {
		GladeWidgetClass *label_class;
		GladeWidget *label;
		gint page;
		gint pages = 3;

		glade_property_query_result_get_int (result, "pages", &pages);

		label_class = glade_widget_class_get_by_name ("GtkLabel");
		g_return_if_fail (label_class != NULL);

		for (page = 0; page < pages; page++) {
			label = glade_widget_new_from_class_name ("GtkLabel",
								  widget);
			g_return_if_fail (GTK_IS_WIDGET (label->widget));
			placeholder = glade_placeholder_new (widget);
			gtk_notebook_append_page (GTK_NOTEBOOK (widget->widget),
						  placeholder,
						  label->widget);
				
		}
		return;
	}

	g_warning ("A new container was cretated but there isn't any code to add a placeholder "
		   "for this class (%s)", class->name);
}

GladeWidget *
glade_placeholder_get_parent (GladePlaceholder *placeholder)
{
	GladeWidget *parent;
	
	parent = gtk_object_get_user_data (GTK_OBJECT (placeholder));

	return parent;
}
