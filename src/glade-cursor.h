/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
struct _GladeCursor {
	GdkCursor *selector;
	GdkCursor *add_widget;
};

/* Enumed values for each of the cursors for GladeCursor. For every
 * GdkCursor above there should be a enum here
 */

typedef enum {
	GLADE_CURSOR_SELECTOR,
	GLADE_CURSOR_ADD_WIDGET,
} GladeCursorType;

void glade_cursor_init (void);
void glade_cursor_set (GdkWindow *window, GladeCursorType type);

G_END_DECLS

#endif /* __GLADE_CURSOR_H__ */
