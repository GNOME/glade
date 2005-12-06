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

#include "glade.h"
#include "glade-cursor.h"

GladeCursor *cursor = NULL;

/**
 * glade_cursor_set:
 * @window: a #GdkWindow
 * @type: a #GladeCursorType
 *
 * Sets the cursor for @window to something appropriate based on @type.
 */
void
glade_cursor_set (GdkWindow *window, GladeCursorType type)
{
	switch (type) {
	case GLADE_CURSOR_SELECTOR:
		gdk_window_set_cursor (window, cursor->selector);
		break;
	case GLADE_CURSOR_ADD_WIDGET:
		gdk_window_set_cursor (window, cursor->add_widget);
		break;
	case GLADE_CURSOR_RESIZE_TOP_LEFT:
		gdk_window_set_cursor (window, cursor->resize_top_left);
		break;
	case GLADE_CURSOR_RESIZE_TOP_RIGHT:
		gdk_window_set_cursor (window, cursor->resize_top_right);
		break;
	case GLADE_CURSOR_RESIZE_BOTTOM_LEFT:
		gdk_window_set_cursor (window, cursor->resize_bottom_left);
		break;
	case GLADE_CURSOR_RESIZE_BOTTOM_RIGHT:
		gdk_window_set_cursor (window, cursor->resize_bottom_right);
		break;
	case GLADE_CURSOR_RESIZE_LEFT:
		gdk_window_set_cursor (window, cursor->resize_left);
		break;
	case GLADE_CURSOR_RESIZE_RIGHT:
		gdk_window_set_cursor (window, cursor->resize_right);
		break;
	case GLADE_CURSOR_RESIZE_TOP:
		gdk_window_set_cursor (window, cursor->resize_top);
		break;
	case GLADE_CURSOR_RESIZE_BOTTOM:
		gdk_window_set_cursor (window, cursor->resize_bottom);
		break;
	case GLADE_CURSOR_DRAG:
		gdk_window_set_cursor (window, cursor->drag);
		break;
	default:
		break;
	}

}

/**
 * glade_cursor_init:
 *
 * Initializes cursors for use with glade_cursor_set().
 */
void
glade_cursor_init (void)
{
	cursor = g_new0 (GladeCursor, 1);

	cursor->selector           = gdk_cursor_new (GDK_TOP_LEFT_ARROW);
	cursor->add_widget         = gdk_cursor_new (GDK_PLUS);

	cursor->resize_top_left     = gdk_cursor_new (GDK_TOP_LEFT_CORNER);
	cursor->resize_top_right    = gdk_cursor_new (GDK_TOP_RIGHT_CORNER);
	cursor->resize_bottom_left  = gdk_cursor_new (GDK_BOTTOM_LEFT_CORNER);
	cursor->resize_bottom_right = gdk_cursor_new (GDK_BOTTOM_RIGHT_CORNER);
	cursor->resize_left         = gdk_cursor_new (GDK_LEFT_SIDE);
	cursor->resize_right        = gdk_cursor_new (GDK_RIGHT_SIDE);
	cursor->resize_top          = gdk_cursor_new (GDK_TOP_SIDE);
	cursor->resize_bottom       = gdk_cursor_new (GDK_BOTTOM_SIDE);
	cursor->drag                = gdk_cursor_new (GDK_FLEUR);
}
