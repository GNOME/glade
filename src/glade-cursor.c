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

	cursor->selector   = gdk_cursor_new (GDK_TOP_LEFT_ARROW);
	cursor->add_widget = gdk_cursor_new (GDK_PLUS);
}
