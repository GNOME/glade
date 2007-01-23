/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include "glade.h"
#include "glade-clipboard.h"
#include "glade-clipboard-view.h"
#include "glade-widget.h"
#include "glade-widget-adaptor.h"
#include "glade-popup.h"


const gint GLADE_CLIPBOARD_VIEW_WIDTH  = 230;
const gint GLADE_CLIPBOARD_VIEW_HEIGHT = 200;

static void
glade_clipboard_view_class_init (GladeClipboardViewClass *klass)
{

}

static void
glade_clipboard_view_init (GladeClipboardView *view)
{
	view->widget = NULL;
	view->clipboard = NULL;
	view->model = NULL;
}

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
gcv_foreach_add_selection (GtkTreeModel *model, 
			   GtkTreePath  *path,
			   GtkTreeIter  *iter, 
			   gpointer      data)
{
	GladeWidget        *widget = NULL;
	GladeClipboardView *view   = (GladeClipboardView *)data;
	gtk_tree_model_get (model, iter, 0, &widget, -1);
	glade_clipboard_selection_add (view->clipboard, (widget));
}

static void
glade_clipboard_view_selection_changed_cb (GtkTreeSelection   *sel,
					   GladeClipboardView *view)
{
	if (view->updating == FALSE)
	{
		glade_clipboard_selection_clear (view->clipboard);
		gtk_tree_selection_selected_foreach
			(sel, gcv_foreach_add_selection, view);
	}
}

static void
glade_clipboard_view_populate_model (GladeClipboardView *view)
{
	GladeClipboard   *clipboard;
	GtkTreeModel     *model;
	GladeWidget      *widget;
	GList            *list;
	GtkTreeIter       iter;

	clipboard = GLADE_CLIPBOARD (view->clipboard);
	model     = GTK_TREE_MODEL (view->model);

	for (list = clipboard->widgets; list; list = list->next) 
	{
		widget = list->data;
		view->updating = TRUE;
		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, widget, -1);
		view->updating = FALSE;
	}
}

static void
glade_clipboard_view_cell_function (GtkTreeViewColumn *tree_column,
				    GtkCellRenderer *cell,
				    GtkTreeModel *tree_model,
				    GtkTreeIter *iter,
				    gpointer data)
{
	gboolean     is_icon = GPOINTER_TO_INT (data);
	GladeWidget *widget;
	GdkPixbuf   *pixbuf = NULL;

	gtk_tree_model_get (tree_model, iter, 0, &widget, -1);

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (widget->name != NULL);

	g_object_get (widget->adaptor, "small-icon", &pixbuf, NULL);
	g_return_if_fail (pixbuf != NULL);

	if (is_icon)
		g_object_set (G_OBJECT (cell),
			      "pixbuf", pixbuf,
			      NULL);
	else
		g_object_set (G_OBJECT (cell),
			      "text", widget->name,
			      NULL);

	g_object_unref (pixbuf);
}

static gint
glade_clipboard_view_button_press_cb (GtkWidget          *widget,
				      GdkEventButton     *event,
				      GladeClipboardView *view)
{
	GtkTreeView      *tree_view = GTK_TREE_VIEW (widget);
	GtkTreePath      *path      = NULL;
	gboolean          handled   = FALSE;

	if (event->window == gtk_tree_view_get_bin_window (tree_view) &&
	    gtk_tree_view_get_path_at_pos (tree_view,
					   (gint) event->x, (gint) event->y,
					   &path, NULL, 
					   NULL, NULL) && path != NULL)
	{
		GtkTreeIter  iter;
		GladeWidget *widget = NULL;
		if (gtk_tree_model_get_iter (GTK_TREE_MODEL (view->model),
					     &iter, path))
		{
			/* Alright, now we can obtain 
			 * the widget from the iter.
			 */
			gtk_tree_model_get (GTK_TREE_MODEL (view->model), &iter,
					    0, &widget, -1);
			if (widget != NULL &&
				    event->button == 3)
			{
				glade_popup_clipboard_pop (widget, event);
				handled = TRUE;
			}
			gtk_tree_path_free (path);
		}
	}
	return handled;
}

static void
glade_clipboard_view_create_tree_view (GladeClipboardView *view)
{
	GtkTreeSelection  *sel;
	GtkCellRenderer   *renderer;
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
	g_object_set (G_OBJECT (renderer), 
		      "xpad", 6, NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 glade_clipboard_view_cell_function,
						 GINT_TO_POINTER (0), NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (view->widget), column);

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->widget));
	gtk_tree_selection_set_mode (sel, GTK_SELECTION_MULTIPLE);
	
	g_signal_connect_data (G_OBJECT (sel), "changed",
			       G_CALLBACK (glade_clipboard_view_selection_changed_cb),
			       view, NULL, 0);

	/* Popup menu */
	g_signal_connect (G_OBJECT (view->widget), "button-press-event",
			  G_CALLBACK (glade_clipboard_view_button_press_cb), view);

}

static void
glade_clipboard_view_construct (GladeClipboardView *view)
{
	GtkWidget *scrolled_window, *viewport;

	view->model = gtk_list_store_new (1, G_TYPE_POINTER);

	glade_clipboard_view_populate_model (view);
	glade_clipboard_view_create_tree_view (view);
	glade_clipboard_view_refresh_sel (view);

	viewport = gtk_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_OUT);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type
		(GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 
					GLADE_GENERIC_BORDER_WIDTH);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scrolled_window), view->widget);
	gtk_container_add (GTK_CONTAINER (viewport), scrolled_window);
	gtk_container_add (GTK_CONTAINER (view), viewport);

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

	view->updating = TRUE;
	gtk_list_store_append (view->model, &iter);
	gtk_list_store_set    (view->model, &iter, 0, widget, -1);
	view->updating = FALSE;
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
	GtkTreeIter   iter;
	GtkTreeModel *model;
	GladeWidget  *clip_widget;

	g_return_if_fail (GLADE_IS_CLIPBOARD_VIEW (view));
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	model = GTK_TREE_MODEL (view->model);
	if (gtk_tree_model_get_iter_first (model, &iter)) 
	{
		do 
		{
			gtk_tree_model_get (model, &iter, 0, &clip_widget, -1);
			if (widget == clip_widget)
				break;
		}
		while (gtk_tree_model_iter_next (model, &iter));
	}

	view->updating = TRUE;
	gtk_list_store_remove (view->model, &iter);
	view->updating = FALSE;
}

/**
 * glade_clipboard_view_refresh_sel:
 * @view: a #GladeClipboardView
 *
 * Synchronizes the treeview selection to the clipboard selection.
 */
void
glade_clipboard_view_refresh_sel (GladeClipboardView *view)
{
	GladeWidget      *widget;
	GtkTreeSelection *sel;
	GList            *list;
	GtkTreeIter      *iter;
	
	g_return_if_fail (GLADE_IS_CLIPBOARD_VIEW (view));

	if (view->updating) return;
	view->updating = TRUE;

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->widget));

	gtk_tree_selection_unselect_all (sel);
	for (list = view->clipboard->selection; 
	     list && list->data; list = list->next)
	{
		widget = list->data;
		if ((iter = glade_util_find_iter_by_widget 
		     (GTK_TREE_MODEL (view->model), widget, 0)) != NULL)
		{
			gtk_tree_selection_select_iter (sel, iter);
			// gtk_tree_iter_free (iter);
		}
	}
	view->updating = FALSE;
}
