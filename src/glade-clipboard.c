/*
 * glade-clipboard.c - An object for handling Cut/Copy/Paste.
 *
 * Copyright (C) 2001 The GNOME Foundation.
 *
 * Author(s):
 *      Archit Baweja <bighead@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#include "glade.h"
#include "glade-clipboard-view.h"
#include "glade-clipboard.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-placeholder.h"
#include "glade-project.h"

static void
glade_clipboard_class_init (GladeClipboardClass *klass)
{

}

static void
glade_clipboard_init (GladeClipboard *clipboard)
{
	clipboard->widgets = NULL;
	clipboard->view = NULL;
	clipboard->curr = NULL;
}

/**
 * glade_clipboard_get_type:
 *
 * Creates the typecode for the #GladeClipboard object type.
 *
 * Returns: the typecode for the #GladeClipboard object type
 */
GType
glade_clipboard_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GladeClipboardClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) glade_clipboard_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (GladeClipboard),
			0,
			(GInstanceInitFunc) glade_clipboard_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "GladeClipboard", &info, 0);
	}

	return type;
}

/**
 * glade_clipboard_new:
 * 
 * Returns: a new #GladeClipboard object
 */
GladeClipboard *
glade_clipboard_new (void)
{
	return GLADE_CLIPBOARD (g_object_new (GLADE_TYPE_CLIPBOARD, NULL));
}

/**
 * glade_clipboard_add:
 * @clipboard: a #GladeClipboard
 * @widget: a #GladeWidget
 * 
 * Adds @widget to @clipboard.
 * This increses the reference count of @widget.
 */
void
glade_clipboard_add (GladeClipboard *clipboard, GladeWidget *widget)
{
	g_object_ref (G_OBJECT (widget));
	/*
	 * Add the GladeWidget to the list of children. And set the
	 * latest addition, to currently selected widget in the clipboard.
	 */
	clipboard->widgets = g_list_prepend (clipboard->widgets, widget);
	clipboard->curr = widget;

	/*
	 * If there is view present, update it.
	 */
	if (clipboard->view)
		glade_clipboard_view_add (GLADE_CLIPBOARD_VIEW (clipboard->view), widget);
}

/**
 * glade_clipboard_remove:
 * @clipboard: a #GladeClipboard
 * @widget: a #GladeWidget
 * 
 * Removes a @widget from @clipboard.
 */
void
glade_clipboard_remove (GladeClipboard *clipboard, GladeWidget *widget)
{
	GList *list;

	clipboard->widgets = g_list_remove (clipboard->widgets, widget);
	g_object_unref (G_OBJECT (widget));

	list = g_list_first (clipboard->widgets);
	if (list != NULL)
		clipboard->curr = GLADE_WIDGET (list->data);
	else
		clipboard->curr = NULL;

	/*
	 * If there is a view present, update it.
	 */
	if (clipboard->view)
		glade_clipboard_view_remove (GLADE_CLIPBOARD_VIEW (clipboard->view), widget);
}


