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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Chema Celorio <chema@celorio.com>
 */


#include "glade-app.h"
#include "glade-palette.h"
#include "glade-cursor.h"
#include "glade-widget-class.h"

#include <glib.h>
#include <glib/gi18n.h>

#define ADD_PIXBUF_FILENAME "plus.png"

static GladeCursor *cursor = NULL;


static void
set_cursor_recurse (GtkWidget *widget, 
		    GdkCursor *gdk_cursor)
{
	GList *children, *list;

	if (!GTK_WIDGET_VISIBLE (widget) || 
	    !GTK_WIDGET_REALIZED (widget))
		return;

	gdk_window_set_cursor (widget->window, gdk_cursor);

	if (GTK_IS_CONTAINER (widget) &&
	    (children = 
	     glade_util_container_get_all_children (GTK_CONTAINER (widget))) != NULL)
	{
		for (list = children; list; list = list->next)
		{
			set_cursor_recurse (GTK_WIDGET (list->data), gdk_cursor);
		}
		g_list_free (children);
	}
}


static void
set_cursor (GdkCursor *gdk_cursor)
{
	GladeProject *project;
	GList        *list, *projects;

	for (projects = glade_app_get_projects ();
	     projects; projects = projects->next)
	{
		project = projects->data;

		for (list = project->objects; 
		     list; list = list->next)
		{
			GObject *object = list->data;

			if (GTK_IS_WINDOW (object))
			{
				set_cursor_recurse (GTK_WIDGET (object), gdk_cursor);
			}
		}
	}
}

/**
 * glade_cursor_set:
 * @window: a #GdkWindow
 * @type: a #GladeCursorType
 *
 * Sets the cursor for @window to something appropriate based on @type.
 * (also sets the cursor on all visible project widgets)
 */
void
glade_cursor_set (GdkWindow *window, GladeCursorType type)
{
	GladeWidgetClass *widget_class;
	GdkCursor        *the_cursor = NULL;
	g_return_if_fail (cursor != NULL);

	switch (type) {
	case GLADE_CURSOR_SELECTOR:
		set_cursor (cursor->selector);
 		gdk_window_set_cursor (window, cursor->selector);
		break;
	case GLADE_CURSOR_ADD_WIDGET:

		widget_class = glade_palette_get_current_item_class (glade_app_get_palette ());

		if (widget_class != NULL)
		{
			if (widget_class->cursor != NULL)
				the_cursor = widget_class->cursor;
			else
				the_cursor = cursor->add_widget;
		}
		else
		{ 
			the_cursor = cursor->add_widget;
		}
		break;
	case GLADE_CURSOR_RESIZE_TOP_LEFT:
		the_cursor = cursor->resize_top_left;
		break;
	case GLADE_CURSOR_RESIZE_TOP_RIGHT:
		the_cursor = cursor->resize_top_right;
		break;
	case GLADE_CURSOR_RESIZE_BOTTOM_LEFT:
		the_cursor = cursor->resize_bottom_left;
		break;
	case GLADE_CURSOR_RESIZE_BOTTOM_RIGHT:
		the_cursor = cursor->resize_bottom_right;
		break;
	case GLADE_CURSOR_RESIZE_LEFT:
		the_cursor = cursor->resize_left;
		break;
	case GLADE_CURSOR_RESIZE_RIGHT:
		the_cursor = cursor->resize_right;
		break;
	case GLADE_CURSOR_RESIZE_TOP:
		the_cursor = cursor->resize_top;
		break;
	case GLADE_CURSOR_RESIZE_BOTTOM:
		the_cursor = cursor->resize_bottom;
		break;
	case GLADE_CURSOR_DRAG:
		the_cursor = cursor->drag;
		break;
	default:
		break;
	}

	gdk_window_set_cursor (window, the_cursor);
	set_cursor (the_cursor);
	
}

/**
 * glade_cursor_init:
 *
 * Initializes cursors for use with glade_cursor_set().
 */
void
glade_cursor_init (void)
{
	gchar *path;
	GError *error = NULL;

	cursor = g_new0 (GladeCursor, 1);

	cursor->selector            = gdk_cursor_new (GDK_TOP_LEFT_ARROW);
	cursor->add_widget          = gdk_cursor_new (GDK_PLUS);
	cursor->resize_top_left     = gdk_cursor_new (GDK_TOP_LEFT_CORNER);
	cursor->resize_top_right    = gdk_cursor_new (GDK_TOP_RIGHT_CORNER);
	cursor->resize_bottom_left  = gdk_cursor_new (GDK_BOTTOM_LEFT_CORNER);
	cursor->resize_bottom_right = gdk_cursor_new (GDK_BOTTOM_RIGHT_CORNER);
	cursor->resize_left         = gdk_cursor_new (GDK_LEFT_SIDE);
	cursor->resize_right        = gdk_cursor_new (GDK_RIGHT_SIDE);
	cursor->resize_top          = gdk_cursor_new (GDK_TOP_SIDE);
	cursor->resize_bottom       = gdk_cursor_new (GDK_BOTTOM_SIDE);
	cursor->drag                = gdk_cursor_new (GDK_FLEUR);
	cursor->add_widget_pixbuf   = NULL;

	/* load "add" cursor pixbuf */
	path = g_build_filename (glade_pixmaps_dir, ADD_PIXBUF_FILENAME, NULL);

	cursor->add_widget_pixbuf = gdk_pixbuf_new_from_file (path, &error);

	if (cursor->add_widget_pixbuf == NULL)
	{
		g_critical (_("Unable to load image (%s)"), error->message);

		g_error_free (error);
		error = NULL;
	}
	g_free (path);
}

const GdkPixbuf*
glade_cursor_get_add_widget_pixbuf (void)
{
	g_return_val_if_fail (cursor != NULL, NULL);

	return cursor->add_widget_pixbuf;
}
