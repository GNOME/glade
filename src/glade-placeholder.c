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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#define PIXMAP_DIR		"C:/Program Files/glade/pixmap"
#include "glade.h"
#include "glade-placeholder.h"
#include "glade-cursor.h"
#include "glade-widget-class.h"
#include "glade-widget.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-project.h"
#include "glade-project-window.h"
#include "glade-packing.h"
#include "glade-clipboard.h"
#include "glade-popup.h"
#include "glade-command.h"

#define GLADE_PLACEHOLDER_ROW_STRING  "GladePlaceholderRow"
#define GLADE_PLACEHOLDER_COL_STRING  "GladePlaceholderColumn"
#define GLADE_PLACEHOLDER_PARENT_DATA "GladePlaceholderParentData"
#define GLADE_PLACEHOLDER_IS_DATA     "GladeIsPlaceholderData"

#define GLADE_PLACEHOLDER_SELECTION_NODE_SIZE 7

static void
glade_placeholder_replace_container (GtkWidget *current,
				     GtkWidget *new,
				     GtkWidget *container)
{
	GParamSpec **param_spec;
	GValue *value;
	guint nproperties;
	guint i;

	if (current->parent != container)
		return;

	param_spec = gtk_container_class_list_child_properties (G_OBJECT_GET_CLASS (container), &nproperties);
	value = calloc (nproperties, sizeof (GValue));
	if (nproperties != 0 && (param_spec == 0 || value == 0))
		// TODO: Signal the not enough memory condition
		return;

	for (i = 0; i < nproperties; i++)
	{
		g_value_init (&value[i], param_spec[i]->value_type);
		gtk_container_child_get_property (GTK_CONTAINER (container), current, param_spec[i]->name, &value[i]);
	}

	gtk_container_remove (GTK_CONTAINER (container), current);
	gtk_container_add (GTK_CONTAINER (container), new);

	for (i = 0; i < nproperties; i++)
		gtk_container_child_set_property (GTK_CONTAINER (container), new, param_spec[i]->name, &value[i]);

	for (i = 0; i < nproperties; i++)
		g_value_unset (&value[i]);

	g_free (param_spec);
	free (value);

#if 0
	gtk_widget_unparent (child_info->widget);
	child_info->widget = new;
	gtk_widget_set_parent (child_info->widget, GTK_WIDGET (container));

	/* */
	child = new;
	if (GTK_WIDGET_REALIZED (container))
		gtk_widget_realize (child);
	if (GTK_WIDGET_VISIBLE (container) && GTK_WIDGET_VISIBLE (child)) {
		if (GTK_WIDGET_MAPPED (container))
			gtk_widget_map (child);
		gtk_widget_queue_resize (child);
	}
	/* */
#endif
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
	gtk_widget_ref (page);

	label = gtk_notebook_get_tab_label (notebook, current);

	/* label may be NULL if the label was not set explicitely;
	 * we probably sholud always craete our GladeWidget label
	 * and add set it as tab label, but at the moment we don't.
	 */
	if (label)
		gtk_widget_ref (label);

	gtk_notebook_remove_page (notebook, page_num);
	gtk_notebook_insert_page (notebook, new, label, page_num);
	gtk_notebook_set_tab_label (notebook, new, label);

	gtk_widget_unref (page);
	if (label)
		gtk_widget_unref (label);

	gtk_notebook_set_current_page (notebook, page_num);
}

void
glade_placeholder_add_methods_to_class (GladeWidgetClass *class)
{
	/* This is ugly, make it better. Chema */
	if (g_type_is_a (class->type, GTK_TYPE_NOTEBOOK))
		class->placeholder_replace = glade_placeholder_replace_notebook;
	else if (g_type_is_a (class->type, GTK_TYPE_CONTAINER))
		class->placeholder_replace = glade_placeholder_replace_container;
}

static GdkWindow*
glade_placeholder_get_gdk_window (GladePlaceholder *placeholder,
				  GtkWidget **paint_widget)
{
	GtkWidget *parent = GTK_WIDGET (placeholder)->parent;

	if (parent) {
		*paint_widget = parent;
		return parent->window;
	}

	*paint_widget = GTK_WIDGET (placeholder);
	return GTK_WIDGET (placeholder)->window;
}

static void
glade_placeholder_draw_selection_nodes (GladePlaceholder *placeholder)
{
	GtkWidget *widget, *paint_widget;
	GdkWindow *window;
	GdkGC *gc;
	gint x, y, w, h;
	gint width, height;

	widget = GTK_WIDGET (placeholder);
	window = glade_placeholder_get_gdk_window (placeholder, &paint_widget);

	if (widget->parent) {
		gtk_widget_translate_coordinates (widget, paint_widget,
						  0, 0, &x, &y);
		w = widget->allocation.width;
		h = widget->allocation.height;
	} else {
		x = 0;
		y = 0;
		gdk_window_get_size (window, &w, &h);
	}

	gc = paint_widget->style->black_gc;
	gdk_gc_set_subwindow (gc, GDK_INCLUDE_INFERIORS);

	width = w;
	height = h;
	if (width > GLADE_PLACEHOLDER_SELECTION_NODE_SIZE
	    && height > GLADE_PLACEHOLDER_SELECTION_NODE_SIZE) {
		gdk_draw_rectangle (window, gc, TRUE, x, y,
				    GLADE_PLACEHOLDER_SELECTION_NODE_SIZE,
				    GLADE_PLACEHOLDER_SELECTION_NODE_SIZE);
		gdk_draw_rectangle (window, gc, TRUE, x,
				    y + height - GLADE_PLACEHOLDER_SELECTION_NODE_SIZE,
				    GLADE_PLACEHOLDER_SELECTION_NODE_SIZE,
				    GLADE_PLACEHOLDER_SELECTION_NODE_SIZE);
		gdk_draw_rectangle (window, gc, TRUE,
				    x + width - GLADE_PLACEHOLDER_SELECTION_NODE_SIZE, y,
				    GLADE_PLACEHOLDER_SELECTION_NODE_SIZE,
				    GLADE_PLACEHOLDER_SELECTION_NODE_SIZE);
		gdk_draw_rectangle (window, gc, TRUE,
				    x + width - GLADE_PLACEHOLDER_SELECTION_NODE_SIZE,
				    y + height - GLADE_PLACEHOLDER_SELECTION_NODE_SIZE,
				    GLADE_PLACEHOLDER_SELECTION_NODE_SIZE,
				    GLADE_PLACEHOLDER_SELECTION_NODE_SIZE);
	}

	gdk_draw_rectangle (window, gc, FALSE, x, y, width - 1, height - 1);
	
	gdk_gc_set_subwindow (gc, GDK_CLIP_BY_CHILDREN);
}

static void
glade_placeholder_clear_selection_nodes (GladePlaceholder *placeholder)
{
	g_return_if_fail (GLADE_IS_WIDGET (placeholder));
	
	gdk_window_clear_area (placeholder->window,
			       placeholder->allocation.x,
			       placeholder->allocation.y,
			       placeholder->allocation.width,
			       placeholder->allocation.height);
	
	gtk_widget_queue_draw (GTK_WIDGET (placeholder));
}

static void
glade_placeholder_on_button_press_event (GladePlaceholder *placeholder,
					 GdkEventButton *event,
					 gpointer not_used)
{
	GladeProjectWindow *gpw = glade_project_window_get ();

	if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {

		if (gpw->add_class != NULL) {
			/* 
			 * A widget type is selected in the palette.
			 * Add a new widget of that type.
			 */
			glade_command_create (gpw->add_class, placeholder, NULL);
			glade_project_window_set_add_class (gpw, NULL);
			gpw->active_placeholder = NULL;
		} else {
			/*
			 * Else set the current placeholder as selected,
			 * so that Paste from the Main Menu works.
			 */
			if (gpw->active_placeholder != NULL)
				glade_placeholder_clear_selection_nodes (gpw->active_placeholder);
			gpw->active_placeholder = placeholder;
			glade_placeholder_draw_selection_nodes (placeholder);
		}
	} else if (event->button == 3) {
		glade_popup_placeholder_pop (placeholder, event);
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

#define GLADE_PLACEHOLDER_SIZE 16

GladePlaceholder *
glade_placeholder_new ()
{
	GladePlaceholder *placeholder;

	placeholder = gtk_drawing_area_new ();
	g_object_set_data (G_OBJECT (placeholder),
			   GLADE_PLACEHOLDER_IS_DATA,
			   GINT_TO_POINTER (TRUE));

	gtk_widget_set_events (GTK_WIDGET (placeholder),
			       gtk_widget_get_events (GTK_WIDGET (placeholder))
			       | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
			       | GDK_BUTTON_RELEASE_MASK
			       | GDK_POINTER_MOTION_MASK | GDK_BUTTON1_MOTION_MASK);

	gtk_widget_set_size_request (GTK_WIDGET (placeholder),
				     GLADE_PLACEHOLDER_SIZE,
				     GLADE_PLACEHOLDER_SIZE);			      
	/* mouse signals */
	g_signal_connect (G_OBJECT (placeholder), "motion_notify_event",
			  G_CALLBACK (glade_placeholder_on_motion_notify_event), NULL);
	g_signal_connect (G_OBJECT (placeholder), "button_press_event",
			  G_CALLBACK (glade_placeholder_on_button_press_event), NULL);

	/* draw signals */
	g_signal_connect_after (G_OBJECT (placeholder), "realize",
				G_CALLBACK (glade_placeholder_on_realize), NULL);
	g_signal_connect_after (G_OBJECT (placeholder), "expose_event",
				G_CALLBACK (glade_placeholder_on_expose_event), NULL);

	gtk_widget_show (GTK_WIDGET (placeholder));

	return placeholder;
}

#undef GLADE_PLACEHOLDER_SIZE

void glade_placeholder_add (GladeWidgetClass *class, GladeWidget *widget)
{

	if (GLADE_WIDGET_CLASS_TOPLEVEL (class)) {
		GladePlaceholder *placeholder;

		placeholder = glade_placeholder_new (widget);
		if (glade_widget_class_is (class, "GtkDialog")) {
			gtk_container_add (GTK_CONTAINER (GTK_DIALOG (widget->widget)->vbox),
					   GTK_WIDGET (placeholder));
			placeholder = glade_placeholder_new (widget);
			gtk_container_add (GTK_CONTAINER (GTK_DIALOG (widget->widget)->action_area),
					   GTK_WIDGET (placeholder));
		} else
			gtk_container_add (GTK_CONTAINER (widget->widget), GTK_WIDGET (placeholder));
	}

}

GladeWidget *
glade_placeholder_get_parent (GladePlaceholder *placeholder)
{
	GladeWidget *parent = NULL;
	GtkWidget *widget = gtk_widget_get_parent (placeholder);

	g_return_val_if_fail (glade_placeholder_is (placeholder), NULL);

	while (widget != NULL) {
		parent = glade_widget_get_from_gtk_widget (widget);

		if (parent != NULL)
			return parent;

		widget = gtk_widget_get_parent (widget);
	}

	return NULL;
}

void
glade_placeholder_replace_with_widget (GladePlaceholder *placeholder,
			   	       GladeWidget *widget)
{
	GladeWidget *parent;

	g_return_if_fail (glade_placeholder_is (placeholder));
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	parent = glade_placeholder_get_parent (placeholder);

	if (parent->class->placeholder_replace != NULL)
		parent->class->placeholder_replace (GTK_WIDGET (placeholder),
						    widget->widget,
						    parent->widget);
	else
		g_warning ("Could not replace a placeholder because a replace "
			   " function has not been implemented for \"%s\"\n",
			   parent->class->name);
}

gboolean
glade_placeholder_is (GtkWidget *widget)
{
	gpointer data;
	gboolean is;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	data = g_object_get_data (G_OBJECT (widget),
				  GLADE_PLACEHOLDER_IS_DATA);

	is = GPOINTER_TO_INT (data);

	return is;
}

