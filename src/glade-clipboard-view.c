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

#include <glib.h>
#include <gtk/gtk.h>
#include "glade.h"
#include "glade-clipboard.h"
#include "glade-clipboard-view.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-project-window.h"

static void
glade_clipboard_view_selection_changed_cb (GtkTreeSelection * sel,
					   GladeClipboardView * view)
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
glade_clipboard_view_cell_function (GtkTreeViewColumn * tree_column,
				    GtkCellRenderer * cell,
				    GtkTreeModel * tree_model,
				    GtkTreeIter * iter, gpointer data)
{
	GladeWidget *widget;

	gtk_tree_model_get (tree_model, iter, 0, &widget, -1);

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (widget->name != NULL);
	
	g_object_set (G_OBJECT (cell), "text", widget->name, NULL);
}

static void
glade_clipboard_view_populate_model_real (GtkTreeStore * model,
					  GList * widgets,
					  GtkTreeIter * parent_iter,
					  gboolean add_childs)
{
	GladeWidget *widget;
	GList *list;
	GtkTreeIter iter;
	GtkTreeIter *copy = NULL;

	list = g_list_copy (widgets);
	for (; list != NULL; list = list->next) {
		widget = list->data;
		gtk_tree_store_append (model, &iter, parent_iter);
		gtk_tree_store_set (model, &iter, 0, widget, -1);
		if (add_childs && widget->children != NULL) {
			copy = gtk_tree_iter_copy (&iter);
			glade_clipboard_view_populate_model_real (model,
								  widget->children,
								  copy, TRUE);
			gtk_tree_iter_free (copy);
		}
	}
	g_list_free (list);
}

static void
glade_clipboard_view_populate_model (GladeClipboardView * view)
{
	GList *list;

	view->model = gtk_tree_store_new (1, G_TYPE_POINTER);
	list = GLADE_CLIPBOARD (view->clipboard)->widgets;

	/* add the widgets and recurse */
	glade_clipboard_view_populate_model_real (view->model, list,
						  NULL, FALSE);
}

static void
glade_clipboard_view_create_tree_view (GladeClipboardView * view)
{
	GtkTreeSelection *sel;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *col;

	view->widget =
	    gtk_tree_view_new_with_model (GTK_TREE_MODEL (view->model));

	cell = gtk_cell_renderer_text_new ();
	col =
	    gtk_tree_view_column_new_with_attributes (_("Widget"), cell,
						      NULL);
	gtk_tree_view_column_set_cell_data_func (col, cell,
						 glade_clipboard_view_cell_function,
						 NULL, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (view->widget), col);

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->widget));
	g_signal_connect_data (G_OBJECT (sel), "changed",
			       G_CALLBACK (glade_clipboard_view_selection_changed_cb),
			       view, NULL, 0);

	gtk_widget_set_usize (view->widget, 272, 130);
	gtk_widget_show (view->widget);
}

static void
glade_clipboard_view_construct (GladeClipboardView * view)
{
	GtkWidget *scrolled_window;

	glade_clipboard_view_populate_model (view);
	glade_clipboard_view_create_tree_view (view);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_usize (scrolled_window, 272, 130);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW
					(scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_set_usize (scrolled_window, 272, 130);
	gtk_container_add (GTK_CONTAINER (scrolled_window), view->widget);
	gtk_container_add (GTK_CONTAINER (view), scrolled_window);
	gtk_widget_show (scrolled_window);

	return;
}

static void
glade_clipboard_view_class_init (GladeClipboardViewClass * klass)
{

}

static void
glade_clipboard_view_init (GladeClipboardView * view)
{
	view->widget = NULL;
	view->clipboard = NULL;
	view->model = NULL;
}

/**
 * glade_clipboard_view_get_type:
 *
 * Creates the typecode for the GladeClipboardView Widget type.
 *
 * Return value: the typecode for the GladeClipboardView widget type.
 **/
GtkType
glade_clipboard_view_get_type ()
{
	static GtkType type = 0;

	if (!type) {
		static const GtkTypeInfo info = {
			"GladeClipboardView",
			sizeof (GladeClipboardView),
			sizeof (GladeClipboardViewClass),
			(GtkClassInitFunc) glade_clipboard_view_class_init,
			(GtkObjectInitFunc) glade_clipboard_view_init,
			/* reserved 1 */ NULL,
			/* reserved 2 */ NULL,
			(GtkClassInitFunc) NULL
		};

		type = gtk_type_unique (gtk_window_get_type (), &info);
	}

	return type;
}

/**
 * glade_clipboard_view_new:
 * 
 * Create a new #GladeClipboardView widget.
 *
 * Return Value: a #GtkWidget.
 **/
GtkWidget *
glade_clipboard_view_new (GladeClipboard * clipboard)
{
	GladeClipboardView *view;

	g_return_val_if_fail (clipboard != NULL, NULL);

	view = gtk_type_new (glade_clipboard_view_get_type ());
	view->clipboard = clipboard;
	glade_clipboard_view_construct (view);

	return GTK_WIDGET (view);
}

void
glade_clipboard_view_add (GladeClipboardView * view, GladeWidget * widget)
{
	GtkTreeIter iter;

	g_return_if_fail (GLADE_IS_CLIPBOARD_VIEW (view));
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	gtk_tree_store_append (view->model, &iter, NULL);
	gtk_tree_store_set (view->model, &iter, 0, widget, -1);
}

void
glade_clipboard_view_remove (GladeClipboardView * view,
			     GladeWidget * widget)
{
	GtkTreeSelection *sel;
	GtkTreeIter iter;
	GtkTreeModel *model;

	g_return_if_fail (GLADE_IS_CLIPBOARD_VIEW (view));
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	model = GTK_TREE_MODEL (view->model);
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->widget));
	if (gtk_tree_selection_get_selected (sel, &model, &iter))
		gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
}
