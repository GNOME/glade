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
#include "glade-project-window.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-placeholder.h"
#include "glade-project.h"
#include "glade-packing.h"

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
 * Creates the typecode for the GladeClipboard object type.
 *
 * Return value: the typecode for the GladeClipboard object type.
 **/
GType
glade_clipboard_get_type ()
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
 * @: 
 * 
 * Create a new @GladeClipboard object.
 *
 * Return Value: a @GladeClipboard object.
 **/
GladeClipboard *
glade_clipboard_new ()
{
	return GLADE_CLIPBOARD (g_object_new (GLADE_TYPE_CLIPBOARD, NULL));
}

/**
 * glade_clipboard_add:
 * @clipboard: 
 * @widget: 
 * 
 * Add a GladeWidget to the Clipboard. Basically has stuff common to 
 * Cut/Copy commands.
 **/
static void
glade_clipboard_add (GladeClipboard *clipboard, GladeWidget *widget)
{
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
 * @clipboard: 
 * @widget: 
 * 
 * Remove a GladeWidget from the Clipboard
 **/
static void
glade_clipboard_remove (GladeClipboard *clipboard, GladeWidget *widget)
{
	clipboard->widgets = g_list_remove (clipboard->widgets, widget);
	clipboard->curr = NULL;

	/*
	 * If there is a view present, update it.
	 */
	if (clipboard->view)
		glade_clipboard_view_remove (GLADE_CLIPBOARD_VIEW (clipboard->view), widget);
}

/**
 * glade_clipboard_cut:
 * @clipboard: 
 * @widget: 
 * 
 * Cut a GladeWidget onto the Clipboard. 
 **/
GladePlaceholder *
glade_clipboard_cut (GladeClipboard *clipboard, GladeWidget *widget)
{
	GladePlaceholder *placeholder = NULL;

	g_return_val_if_fail (GLADE_IS_CLIPBOARD (clipboard), NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

	glade_project_remove_widget (widget);

	/*
	 * We ref so that the widget and its children are not destroyed.
	 */
	gtk_widget_ref (GTK_WIDGET (widget->widget));
	if (widget->parent)
		placeholder = glade_widget_replace_with_placeholder (widget);
	else
		gtk_widget_hide (widget->widget);

	glade_clipboard_add (clipboard, widget);

	return placeholder;
}

/**
 * glade_clipboard_copy:
 * @clipboard: 
 * @widget: 
 * 
 * Copy a GladeWidget onto the Clipboard. 
 **/
void
glade_clipboard_copy (GladeClipboard *clipboard, GladeWidget *widget)
{
	GladeWidget *copy;

	g_return_if_fail (GLADE_IS_CLIPBOARD (clipboard));
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	copy = glade_widget_clone (widget);

	glade_clipboard_add (clipboard, copy);
}

/**
 * glade_clipboard_paste:
 * @clipboard: 
 * @parent: 
 * 
 * Paste a GladeWidget from the Clipboard.
 **/
void
glade_clipboard_paste (GladeClipboard *clipboard,
		       GladePlaceholder *placeholder)
{
	GladeWidget *widget;
	GladeWidget *parent;
	GladeProject *project;

	g_return_if_fail (GLADE_IS_CLIPBOARD (clipboard));

	widget = clipboard->curr;

	/*
	 * FIXME: I think that GladePlaceholder should have a pointer
	 * to the project it belongs to, as GladeWidget does. This way
	 * the clipboard can be independent from glade-project-window.
	 * 	Paolo.
	 */
	project = glade_project_window_get_project ();

	parent = glade_placeholder_get_parent (placeholder);

	if (!widget)
		return;
	
	widget->name = glade_widget_new_name (project, widget->class);
	widget->parent = parent;
	glade_packing_add_properties (widget);
	glade_project_add_widget (project, widget);

	if (parent)
		parent->children = g_list_prepend (parent->children, widget);

	glade_widget_set_contents (widget);
	glade_widget_connect_signals (widget);

	/*
	 * Toplevel widgets are not packed into other containers :-)
	 */
	if (!GLADE_WIDGET_IS_TOPLEVEL (widget)) {
		/* Signal error, if placeholder not selected */
		if (!placeholder) {
			glade_util_ui_warn (_("Placeholder not selected!"));
			return;
		}

		glade_placeholder_replace (placeholder, parent, widget);
		glade_widget_set_default_packing_options (widget);
	}

	glade_project_selection_set (widget, TRUE);

	/*
	 * This damned 'if' statement caused a 1 month delay.
	 */
	if (GTK_IS_WIDGET (widget->widget))
		gtk_widget_show_all (GTK_WIDGET (widget->widget));

	/*
	 * Finally remove widget from clipboard.
	 */
	glade_clipboard_remove (clipboard, widget);
}

