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
#include "glade-popup.h"
#include "glade-command.h"

#define GLADE_PLACEHOLDER_ROW_STRING  "GladePlaceholderRow"
#define GLADE_PLACEHOLDER_COL_STRING  "GladePlaceholderColumn"
#define GLADE_PLACEHOLDER_PARENT_DATA "GladePlaceholderParentData"
#define GLADE_PLACEHOLDER_IS_DATA     "GladeIsPlaceholderData"


static gboolean
glade_placeholder_on_button_press_event (GladePlaceholder *placeholder,
					 GdkEventButton *event,
					 gpointer not_used)
{
	GladeProjectWindow *gpw = glade_project_window_get ();

	if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {
		if (gpw->add_class != NULL) {
			/* A widget type is selected in the palette.
			 * Add a new widget of that type.
			 */
			glade_command_create (gpw->add_class, placeholder, NULL);
			glade_project_window_set_add_class (gpw, NULL);
		} else {
			GladeWidget *parent = glade_placeholder_get_parent (placeholder);
			glade_project_selection_set (parent->project, placeholder, TRUE);
		}
	} else if (event->button == 3)
		glade_popup_placeholder_pop (placeholder, event);

	return TRUE;
}

static gboolean
glade_placeholder_on_motion_notify_event (GladePlaceholder *placeholder,
					  GdkEventMotion *event,
					  gpointer not_used)
{
	GladeProjectWindow *gpw;

	gpw = glade_project_window_get ();

	if (gpw->add_class == NULL)
		glade_cursor_set (event->window, GLADE_CURSOR_SELECTOR);
	else
		glade_cursor_set (event->window, GLADE_CURSOR_ADD_WIDGET);

	return FALSE;
}

static gboolean
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
	gdk_drawable_get_size (event->window, &w, &h);

	gdk_draw_line (event->window, light_gc, 0, 0, w - 1, 0);
	gdk_draw_line (event->window, light_gc, 0, 0, 0, h - 1);
	gdk_draw_line (event->window, dark_gc, 0, h - 1, w - 1, h - 1);
	gdk_draw_line (event->window, dark_gc, w - 1, 0, w - 1, h - 1);

	return FALSE;
}

static gboolean
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
		return FALSE;
	}
	
	gdk_window_set_back_pixmap (GTK_WIDGET (placeholder)->window, pixmap, FALSE);

	return FALSE;
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

GladeWidget *
glade_placeholder_get_parent (GladePlaceholder *placeholder)
{
	GladeWidget *parent = NULL;
	GtkWidget *widget = gtk_widget_get_parent (placeholder);

	g_return_val_if_fail (GLADE_IS_PLACEHOLDER (placeholder), NULL);

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

	g_return_if_fail (GLADE_IS_PLACEHOLDER (placeholder));
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	parent = glade_placeholder_get_parent (placeholder);

	if (parent->class->replace_child)
		parent->class->replace_child (GTK_WIDGET (placeholder),
					      widget->widget,
					      parent->widget);
	else
		g_warning ("Could not replace a placeholder because a replace "
			   " function has not been implemented for \"%s\"\n",
			   parent->class->name);
}

gboolean
GLADE_IS_PLACEHOLDER (GtkWidget *widget)
{
	gpointer data;
	gboolean is;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	data = g_object_get_data (G_OBJECT (widget),
				  GLADE_PLACEHOLDER_IS_DATA);

	is = GPOINTER_TO_INT (data);

	return is;
}

