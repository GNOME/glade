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
#include <stdlib.h>
#include <config.h>

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

static void
glade_placeholder_replace_box (GtkWidget *current,
			       GtkWidget *new,
			       GtkWidget *container)
{
	/* Some Gtk Hackery. Not beautifull but needed. */
	GtkBoxChild *child_info = NULL;
	GtkWidget *child;
	GtkBox *box;
	GList *list;

	box = GTK_BOX (container);

	list = box->children;
	for (; list != NULL; list = list->next) {
		child_info = list->data;
		if (child_info->widget == current)
			break;
	}
	if (list == NULL) {
		g_warning ("Error while replacing a widget in a GtkBox. The old widget could not be found\n");
		return;
	}
	gtk_widget_unparent (child_info->widget);
	child_info->widget = new;
	gtk_widget_set_parent (child_info->widget, GTK_WIDGET (box));

	/* */
	child = new;
	if (GTK_WIDGET_REALIZED (box))
		gtk_widget_realize (child);
	if (GTK_WIDGET_VISIBLE (box) && GTK_WIDGET_VISIBLE (child)) {
		if (GTK_WIDGET_MAPPED (box))
			gtk_widget_map (child);
		gtk_widget_queue_resize (child);
	}
	/* */
}

static void
glade_placeholder_replace_table (GtkWidget *current,
				 GtkWidget *new,
				 GtkWidget *container)
{
	/* Some Gtk Hackery. Not beautifull, but needed */
	GtkTableChild *table_child = NULL;
	GtkWidget *child;
	GtkTable *table;
	GList *list;

	table = GTK_TABLE (container);
	list = table->children;
		
	for (; list != NULL; list = list->next) {
		table_child = list->data;
		if (table_child->widget == current)
			break;
	}
	if (list == NULL) {
		g_warning ("Error while adding a widget to a GtkTable\n");
		return;
	}
	
	gtk_widget_unparent (table_child->widget);
	table_child->widget = new;
	gtk_widget_set_parent (table_child->widget, GTK_WIDGET (table));
	
	/* */
	child = new;
	if (GTK_WIDGET_REALIZED (child->parent))
		gtk_widget_realize (child);
	if (GTK_WIDGET_VISIBLE (child->parent) && GTK_WIDGET_VISIBLE (child)) {
		if (GTK_WIDGET_MAPPED (child->parent))
			gtk_widget_map (child);
		gtk_widget_queue_resize (child);
	}
	/* */
}

static void
glade_placeholder_replace_container (GtkWidget *current,
				     GtkWidget *new,
				     GtkWidget *container)
{
	gtk_container_remove (GTK_CONTAINER (container), current);
	gtk_container_add (GTK_CONTAINER (container), new);
}

/**
 * glade_placeholder_replace_notebook:
 * @current: the current widget that is going to be replaced
 * @new: the new widget that is going to replace
 * @container: the container
 * 
 * Replaces a widget inside a notebook with a new widget.
 **/
static void
glade_placeholder_replace_notebook (GtkWidget *current,
				    GtkWidget *new,
				    GtkWidget *container)
{
	GtkNotebook *notebook;
	GtkWidget *page;
	GtkWidget *label;
	gint page_num;

	notebook = GTK_NOTEBOOK (container);
	page_num = gtk_notebook_page_num (notebook, current);
	if (page_num == -1) {
		g_warning ("GtkNotebookPage not found\n");
		return;
	}

	page = gtk_notebook_get_nth_page (notebook, page_num);
	label = gtk_notebook_get_tab_label (notebook, current);
	
	gtk_widget_ref (page);
	gtk_widget_ref (label);

	gtk_notebook_remove_page (notebook, page_num);
	gtk_notebook_insert_page (notebook, new,
				  label, page_num);
	gtk_notebook_set_tab_label (notebook,
				    new,
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

	glade_placeholder_replace (placeholder, parent, widget);

	glade_widget_set_default_packing_options (widget);
	
	glade_project_selection_set (widget, TRUE);
}

void
glade_placeholder_add_methods_to_class (GladeWidgetClass *class)
{
	/* This is ugly, make it better. Chema */
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

static void
glade_placeholder_on_button_press_event (GladePlaceholder *placeholder, GdkEventButton *event, GladeProject *project)
{
	GladeProjectWindow *gpw;

	gpw = glade_project_window_get ();
	
	if (event->button == 1 && event->type == GDK_BUTTON_PRESS && gpw->add_class != NULL) {
		glade_placeholder_replace_widget (placeholder, gpw->add_class, project);
		glade_project_window_set_add_class (gpw, NULL);
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

#define GLADE_PLACEHOLDER_SIZE 16

GladePlaceholder *
glade_placeholder_new (GladeWidget *parent)
{
	GladePlaceholder *placeholder;

	placeholder = gtk_drawing_area_new ();
	gtk_object_set_user_data (GTK_OBJECT (placeholder), parent);

	gtk_widget_set_events (GTK_WIDGET (placeholder),
			       gtk_widget_get_events (GTK_WIDGET (placeholder))
			       | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
			       | GDK_BUTTON_RELEASE_MASK
			       | GDK_POINTER_MOTION_MASK | GDK_BUTTON1_MOTION_MASK);
	gtk_widget_set_usize (GTK_WIDGET (placeholder),
			      GLADE_PLACEHOLDER_SIZE,
			      GLADE_PLACEHOLDER_SIZE);			      
	gtk_widget_show (GTK_WIDGET (placeholder));
	
	glade_placeholder_connect_draw_signals  (placeholder);
	glade_placeholder_connect_mouse_signals (placeholder, parent->project);
	
	gtk_signal_connect (GTK_OBJECT (placeholder), "destroy",
			    GTK_SIGNAL_FUNC (glade_placeholder_on_destroy), NULL);
	
	return placeholder;
}
#undef GLADE_PLACEHOLDER_SIZE


void
glade_placeholder_add (GladeWidgetClass *class,
		       GladeWidget *widget,
		       gint rows, gint columns)
{
	GladePlaceholder *placeholder;

	g_return_if_fail (class != NULL);
	g_return_if_fail (widget != NULL);

	if (GLADE_WIDGET_CLASS_TOPLEVEL (class)) {
		placeholder = glade_placeholder_new (widget);
		gtk_container_add (GTK_CONTAINER (widget->widget), GTK_WIDGET (placeholder));
		return;
	}

	if (glade_widget_class_is (class, "GtkFrame") ||
	    glade_widget_class_is (class, "GtkHandleBox")) {
		placeholder = glade_placeholder_new (widget);
		gtk_container_add (GTK_CONTAINER (widget->widget),
				   placeholder);
		return;
	}
	
	if (glade_widget_class_is (class, "GtkVBox") ||
	    glade_widget_class_is (class, "GtkHBox")) {
#if 0	
		g_print ("Deprecated !!\n");
#endif	
		return;
		/* This function shuold not exist */
	}
	
	if (glade_widget_class_is (class, "GtkTable")) {
		gint row, col;
		gint n_rows = rows, n_cols = columns;

		for (row = 0; row < n_rows; row++) {
			for (col = 0; col < n_cols; col++) {
				placeholder = glade_placeholder_new (widget);
				gtk_table_attach (GTK_TABLE (widget->widget),
						  GTK_WIDGET (placeholder),
						  col, col+1,
						  row, row+1,
						  GTK_EXPAND | GTK_FILL, 
						  GTK_EXPAND | GTK_FILL,
						  0, 0);
			}
		}
		return;
	}


	if (glade_widget_class_is (class, "GtkNotebook")) {
		GladeWidgetClass *label_class;
		GladeWidget *label;
		gint page;
		gint pages = rows;

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

void
glade_placeholder_add_with_result (GladeWidgetClass *class,
				   GladeWidget *widget,
				   GladePropertyQueryResult *result)
{
	gint rows = 0, columns = 0;

	if (glade_widget_class_is (class, "GtkVBox") ||
	    glade_widget_class_is (class, "GtkHBox")) {
		GladeProperty *property;
		gint size;
		glade_property_query_result_get_int (result, "size", &size);
		property = glade_property_get_from_id (widget->properties,
						       "size");
		glade_property_set_integer (property, size);
	}

	if (glade_widget_class_is (class, "GtkTable")) {
		glade_property_query_result_get_int (result, "n-rows", &rows);
		glade_property_query_result_get_int (result, "n-columns", &columns);
	}

	if (glade_widget_class_is (class, "GtkNotebook"))
		glade_property_query_result_get_int (result, "pages", &rows);

	glade_placeholder_add (class, widget, rows, columns);
}


GladeWidget *
glade_placeholder_get_parent (GladePlaceholder *placeholder)
{
	GladeWidget *parent;
	
	parent = gtk_object_get_user_data (GTK_OBJECT (placeholder));

	return parent;
}

void
glade_placeholder_replace (GladePlaceholder *placeholder, GladeWidget *parent, GladeWidget *child)
{
	g_return_if_fail (GTK_IS_WIDGET (placeholder));
	
	if (parent->class->placeholder_replace != NULL)
		parent->class->placeholder_replace (GTK_WIDGET (placeholder), child->widget, parent->widget);
	else
		g_warning ("Could not replace a placeholder because we don't have a replace "
			   " function has not been implemented for \"%s\"\n",
			   parent->class->name);
}



GladePlaceholder *
glade_placeholder_get_from_properties (GladeWidget *parent,
				       GHashTable *properties)
{
	GladePlaceholder *placeholder = NULL;
	GList *list;

	list = gtk_container_children (GTK_CONTAINER (parent->widget));
	
	if (glade_widget_class_is (parent->class, "GtkVBox") ||
	    glade_widget_class_is (parent->class, "GtkHBox")) {
		GtkBoxChild *box_child;
		const gchar *val;
		val = g_hash_table_lookup (properties, "position");
		box_child = (GtkBoxChild *) g_list_nth (list, atoi (val));
		placeholder = box_child->widget;
	}

	/* Get the first free placeholder */
	if (!placeholder && list)
		placeholder = list->data;
	
	return placeholder;
}
