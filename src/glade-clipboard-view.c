/*
 * glade-clipboard-view.c - The View for the Clipboard.
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
#include "glade-clipboard.h"
#include "glade-clipboard-view.h"
#include "glade-widget.h"
#include "glade-widget-class.h"


const gint GLADE_CLIPBOARD_VIEW_WIDTH  = 230;
const gint GLADE_CLIPBOARD_VIEW_HEIGHT = 200;

static void
glade_clipboard_view_class_init (GladeClipboardViewClass *class)
{

}

static void
glade_clipboard_view_init (GladeClipboardView *view)
{
	view->widget = NULL;
	view->clipboard = NULL;
	view->model = NULL;
}

/**
 * glade_clipboard_view_get_type:
 *
 * Creates the typecode for the #GladeClipboardView object type.
 *
 * Returns: the typecode for the #GladeClipboardView object type
 */
GType
glade_clipboard_view_get_type ()
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GladeClipboardViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) glade_clipboard_view_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (GladeClipboardView),
			0,
			(GInstanceInitFunc) glade_clipboard_view_init
		};

		type = g_type_register_static (GTK_TYPE_WINDOW, "GladeClipboardView", &info, 0);
	}

	return type;
}

static void
glade_clipboard_view_selection_changed_cb (GtkTreeSelection *sel,
					   GladeClipboardView *view)
{
	GtkTreeIter iter;
	GladeWidget *widget;
	GtkTreeModel *model;
	
	model = GTK_TREE_MODEL (view->model);
	if (!gtk_tree_selection_get_selected (sel, &model, &iter))
		return;

	gtk_tree_model_get (model, &iter, 0, &widget, -1);

	if (widget)
		view->clipboard->curr = widget;
}

static void
glade_clipboard_view_populate_model (GladeClipboardView *view)
{
	GladeClipboard *clipboard;
	GtkListStore *model;
	GList *list;
	GtkTreeIter iter;

	clipboard = GLADE_CLIPBOARD (view->clipboard);
	model = view->model;

	for (list = clipboard->widgets; list; list = list->next) {
		GladeWidget *widget;

		widget = list->data;
		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter, 0, widget, -1);
	}
}

static void
glade_clipboard_view_cell_function (GtkTreeViewColumn *tree_column,
				    GtkCellRenderer *cell,
				    GtkTreeModel *tree_model,
				    GtkTreeIter *iter,
				    gpointer data)
{
	gboolean is_icon = GPOINTER_TO_INT (data);
	GladeWidget *widget;

	gtk_tree_model_get (tree_model, iter, 0, &widget, -1);

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (widget->name != NULL);
	g_return_if_fail (widget->widget_class != NULL);
	g_return_if_fail (widget->widget_class->icon != NULL);

	if (is_icon)
		g_object_set (G_OBJECT (cell),
			      "pixbuf", gtk_image_get_pixbuf (GTK_IMAGE (widget->widget_class->icon)),
			      NULL);
	else
		g_object_set (G_OBJECT (cell),
			      "text", widget->name,
			      NULL);
}

static void
glade_clipboard_view_create_tree_view (GladeClipboardView *view)
{
	GtkTreeSelection *sel;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	view->widget =
	    gtk_tree_view_new_with_model (GTK_TREE_MODEL (view->model));

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Widget"));

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 glade_clipboard_view_cell_function,
						 GINT_TO_POINTER (1), NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 glade_clipboard_view_cell_function,
						 GINT_TO_POINTER (0), NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (view->widget), column);

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->widget));
	g_signal_connect_data (G_OBJECT (sel), "changed",
			       G_CALLBACK (glade_clipboard_view_selection_changed_cb),
			       view, NULL, 0);
}

static void
glade_clipboard_view_construct (GladeClipboardView *view)
{
	GtkWidget *scrolled_window;

	view->model = gtk_list_store_new (1, G_TYPE_POINTER);
	glade_clipboard_view_populate_model (view);

	glade_clipboard_view_create_tree_view (view);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scrolled_window), view->widget);
	gtk_container_add (GTK_CONTAINER (view), scrolled_window);

	gtk_window_set_default_size (GTK_WINDOW (view),
				     GLADE_CLIPBOARD_VIEW_WIDTH,
				     GLADE_CLIPBOARD_VIEW_HEIGHT);

	gtk_window_set_type_hint (GTK_WINDOW (view), GDK_WINDOW_TYPE_HINT_UTILITY);

	gtk_widget_show_all (scrolled_window);

	return;
}

/**
 * glade_clipboard_view_new:
 * @clipboard: a #GladeClipboard
 * 
 * Create a new #GladeClipboardView widget for @clipboard.
 *
 * Returns: a new #GladeClipboardView cast to be a #GtkWidget
 */
GtkWidget *
glade_clipboard_view_new (GladeClipboard *clipboard)
{
	GladeClipboardView *view;

	g_return_val_if_fail (GLADE_IS_CLIPBOARD (clipboard), NULL);

	view = gtk_type_new (glade_clipboard_view_get_type ());
	view->clipboard = clipboard;
	glade_clipboard_view_construct (view);

	return GTK_WIDGET (view);
}

/**
 * glade_clipboard_view_add:
 * @view: a #GladeClipboardView
 * @widget: a #GladeWidget
 *
 * Adds @widget to @view.
 */
void
glade_clipboard_view_add (GladeClipboardView *view, GladeWidget *widget)
{
	GtkTreeIter iter;

	g_return_if_fail (GLADE_IS_CLIPBOARD_VIEW (view));
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	gtk_list_store_append (view->model, &iter);
	gtk_list_store_set (view->model, &iter, 0, widget, -1);
}

/**
 * glade_cliboard_view_remove:
 * @view: a #GladeClipboardView
 * @widget: a #GladeWidget
 *
 * Removes @widget from @view.
 */
void
glade_clipboard_view_remove (GladeClipboardView *view, GladeWidget *widget)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GladeWidget *clip_widget;

	g_return_if_fail (GLADE_IS_CLIPBOARD_VIEW (view));
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	model = GTK_TREE_MODEL (view->model);
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			gtk_tree_model_get (model, &iter, 0, &clip_widget, -1);
			if (widget == clip_widget)
				break;
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	gtk_list_store_remove (view->model, &iter);
}
