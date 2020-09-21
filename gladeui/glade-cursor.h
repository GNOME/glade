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

#ifndef __GLADE_CURSOR_H__
#define __GLADE_CURSOR_H__

G_BEGIN_DECLS

/* GladeCursor is just a structures that has a pointer to all the cursors
 * that we are going to use. The benefit of this struct is that once
 * glade_cursor_init is called you just need to call glade_cursor_set
 * with it's enumed value to set the window cursor.
 */

/* Has a pointer to the loaded GdkCursors. It is loaded when _init
 * is called
 */
typedef struct _GladeCursor {
  GdkCursor *selector;
  GdkCursor *add_widget;        /* fallback cursor if we cannot use widget_class->cursor */ 
  GdkCursor *resize_top_left;
  GdkCursor *resize_top_right;
  GdkCursor *resize_bottom_left;
  GdkCursor *resize_bottom_right;
  GdkCursor *resize_left;
  GdkCursor *resize_right;
  GdkCursor *resize_top;
  GdkCursor *resize_bottom;
  GdkCursor *drag;

  GdkPixbuf *add_widget_pixbuf;   /* a pixbuf of the generic 'add' cursor */
} GladeCursor;

/* Enumed values for each of the cursors for GladeCursor. For every
 * GdkCursor above there should be a enum here
 */
typedef enum {
  GLADE_CURSOR_SELECTOR,
  GLADE_CURSOR_ADD_WIDGET,
  GLADE_CURSOR_RESIZE_TOP_LEFT,
  GLADE_CURSOR_RESIZE_TOP_RIGHT,
  GLADE_CURSOR_RESIZE_BOTTOM_LEFT,
  GLADE_CURSOR_RESIZE_BOTTOM_RIGHT,
  GLADE_CURSOR_RESIZE_LEFT,
  GLADE_CURSOR_RESIZE_RIGHT,
  GLADE_CURSOR_RESIZE_TOP,
  GLADE_CURSOR_RESIZE_BOTTOM,
  GLADE_CURSOR_DRAG
} GladeCursorType;

void               glade_cursor_init                  (void);
void               glade_cursor_set                   (GladeProject    *project,
                                                       GdkWindow       *window, 
                                                       GladeCursorType  type);
const GdkPixbuf*   glade_cursor_get_add_widget_pixbuf (void);

G_END_DECLS

#endif /* __GLADE_CURSOR_H__ */
