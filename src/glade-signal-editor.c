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
 *   Shane Butler <shane_b@users.sourceforge.net>
 */


#include <string.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-signal.h"
#include "glade-signal-editor.h"
#include "glade-editor.h"


enum
{
	COLUMN_SIGNAL,
	COLUMN_HANDLER,
	COLUMN_AFTER,

	COLUMN_VISIBLE,
	COLUMN_SLOT,
	NUM_COLUMNS
};

static void
after_toggled (GtkCellRendererToggle *cell,
	       gchar                 *path_str,
	       gpointer               data)
{
	GladeSignalEditor *editor = (GladeSignalEditor*) data;
	GtkTreeModel *model = GTK_TREE_MODEL (editor->model);
	GtkTreeIter  iter;
	GtkTreeIter iter_parent;
	char *handler;
	char *signal_name;
	GladeSignal *old_signal;
	GList *lsignal;
	GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
	gboolean after;

	/* get toggled iter */
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, COLUMN_SIGNAL, &signal_name, COLUMN_HANDLER, &handler, COLUMN_AFTER, &after, -1);
	if (signal_name == NULL)
	{
		if (!gtk_tree_model_iter_parent (model, &iter_parent, &iter))
			g_assert (FALSE);

		gtk_tree_model_get (model, &iter_parent, COLUMN_SIGNAL, &signal_name, -1);
		g_assert (signal_name != NULL);
	}

	old_signal = glade_signal_new (signal_name, handler, after);
	lsignal = glade_widget_find_signal (editor->widget, old_signal);

	after = !after;
	((GladeSignal*) lsignal->data)->after = after;

	/* set new value */
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_AFTER, after, -1);

	/* clean up */
	gtk_tree_path_free (path);
}

static gboolean
is_valid_identifier (const char *text)
{
	char ch;

	if (text == NULL)
		return FALSE;

	ch = *text++;
	if (!(ch == '_' || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')))
		return FALSE;

	while ((ch = *text++) != 0)
		if (!(ch == '_' || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')))
			return FALSE;

	return TRUE;
}

static void
cell_edited (GtkCellRendererText *cell,
	     const gchar         *path_str,
	     const gchar         *new_text,
	     gpointer             data)
{
	GladeSignalEditor *editor = (GladeSignalEditor*) data;
	GtkTreeModel *model = GTK_TREE_MODEL (editor->model);
	GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
	GtkTreeIter iter;
	GtkTreeIter iter_parent;
	GtkTreeIter iter_new_slot;
	GtkTreeIter *iter2;
	GladeSignal *old_signal;
	GladeSignal *signal;
	GList *lsignal;
	gchar *signal_name;
	gboolean after;
	char *old_handler;
	gboolean child = TRUE;

	gtk_tree_model_get_iter (model, &iter, path);
	iter2 = gtk_tree_iter_copy (&iter);

	gtk_tree_model_get (model, &iter, COLUMN_SIGNAL, &signal_name, COLUMN_HANDLER, &old_handler, COLUMN_AFTER, &after, -1);
	if (signal_name == NULL)
	{
		if (!gtk_tree_model_iter_parent (model, &iter_parent, &iter))
			g_assert (FALSE);

		gtk_tree_model_get (model, &iter_parent, COLUMN_SIGNAL, &signal_name, -1);
		g_assert (signal_name != NULL);
	}
	else
		iter_parent = iter;

	old_signal = glade_signal_new (signal_name, old_handler, after);
	lsignal = glade_widget_find_signal (editor->widget, old_signal);
	signal = lsignal ? lsignal->data : NULL;

	/* check that the new_text is a valid identifier.  TODO: I don't like that! We're throwing away the text of the user
	 * without even giving him an explanation.  We should keep the text and say him that it's invalid, and why.  The
	 * text should be marked specially (maybe in red) and not added to the list of signals, but we want to keep it just
	 * in case it was just a little typo.  Otherwise he will write its signal handler, make the type, and lost the whole
	 * string because it got substituted by <Type...> stuff */
	if (is_valid_identifier (new_text))
	{
		if (signal == NULL)
		{
			/* we're adding a new signal */
			signal = glade_signal_copy (old_signal);
			g_free (signal->handler);
			signal->handler = g_strdup (new_text);

			glade_widget_add_signal (editor->widget, signal);
			gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_HANDLER, signal->handler, COLUMN_VISIBLE, TRUE, COLUMN_SLOT, FALSE, -1);

			/* TODO: Add the <Type...> item */
			gtk_tree_store_append (GTK_TREE_STORE (model), &iter_new_slot, &iter_parent);
			gtk_tree_store_set (GTK_TREE_STORE (model), &iter_new_slot,
					    COLUMN_HANDLER, _("<Type the signal's handler here>"),
					    COLUMN_AFTER, FALSE,
					    COLUMN_VISIBLE, FALSE,
					    COLUMN_SLOT, TRUE, -1);
		}
		else
		{
			/* we're editing a signal */
			g_free (signal->handler);
			signal->handler = g_strdup (new_text);
			gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_HANDLER, signal->handler, -1);
		}
	}
	else
	{
		if (signal == NULL)
		{
			/* we've erased a <Type...> item, so we reput the "<Type...>" text */
			gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_HANDLER, _("<Type the signal's handler here>"),
					    COLUMN_AFTER, FALSE, COLUMN_VISIBLE, FALSE, COLUMN_SLOT, TRUE, -1);
		}
		else
		{
			/* we're erasing a signal */
			glade_widget_remove_signal (editor->widget, signal);
			gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_HANDLER, _("<Type the signal's handler here>"),
					    COLUMN_AFTER, FALSE, COLUMN_VISIBLE, FALSE, COLUMN_SLOT, TRUE, -1);
		}
	}

	gtk_tree_iter_free (iter2);
	gtk_tree_path_free (path);
	glade_signal_free (old_signal);
}

static GtkWidget *
glade_signal_editor_construct_signals_list (GladeSignalEditor *editor)
{
	GtkTreeView *view;
	GtkWidget *view_widget;
	GtkTreeViewColumn *column;
 	GtkCellRenderer *renderer;
	GtkTreeModel *model;

	editor->model = gtk_tree_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
	model = GTK_TREE_MODEL (editor->model);

	view_widget = gtk_tree_view_new_with_model (model);
	view = GTK_TREE_VIEW (view_widget);

	/* the view now holds a reference, we can get rid of our own */
	g_object_unref (G_OBJECT (editor->model));

	/* signal column */
 	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Signal"), renderer, "text", COLUMN_SIGNAL, NULL);
 	gtk_tree_view_append_column (view, column);

	/* handler column */
 	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), "editable", TRUE, "style", PANGO_STYLE_ITALIC, "foreground", "Gray", NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (cell_edited), editor);
	column = gtk_tree_view_column_new_with_attributes (_("Handler"), renderer, "text", COLUMN_HANDLER, "style_set", COLUMN_SLOT, "foreground_set", COLUMN_SLOT, NULL);
 	gtk_tree_view_append_column (view, column);

	/* after column */
	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled", G_CALLBACK (after_toggled), editor);
 	column = gtk_tree_view_column_new_with_attributes (_("After"), renderer, "active", COLUMN_AFTER, "visible", COLUMN_VISIBLE, NULL);
 	gtk_tree_view_append_column (view, column);

	return view_widget;
}

static void
glade_signal_editor_construct (GladeSignalEditor *editor)
{
	GtkWidget *vbox;
	GtkWidget *scroll;

	vbox = gtk_vbox_new (FALSE, 0);
	editor->main_window = vbox;

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_IN);

	editor->signals_list = glade_signal_editor_construct_signals_list (editor);
	gtk_container_add (GTK_CONTAINER (scroll), editor->signals_list);

	gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);

	gtk_widget_show_all (editor->main_window);
}

GtkWidget *
glade_signal_editor_get_widget (GladeSignalEditor *editor)
{
	g_return_val_if_fail (GLADE_IS_SIGNAL_EDITOR (editor), NULL);
	g_return_val_if_fail (GTK_IS_WIDGET (editor->main_window), NULL);
	
	return editor->main_window;
}

GladeSignalEditor *
glade_signal_editor_new (GladeEditor *editor)
{
	GladeSignalEditor *signal_editor;

	signal_editor = g_new0 (GladeSignalEditor, 1);

	glade_signal_editor_construct (signal_editor);
	signal_editor->editor = editor;
	
	return signal_editor;
}

void 
glade_signal_editor_load_widget (GladeSignalEditor *editor,
				 GladeWidget *widget)
{
	GList *list;
	const char *last_type = "";
	GtkTreeIter iter;
	GtkTreeIter parent_class;
	GtkTreeIter parent_signal;
	GtkTreePath *path_first;
	GList *signals;

	g_return_if_fail (GLADE_IS_SIGNAL_EDITOR (editor));
	g_return_if_fail (widget == NULL || GLADE_IS_WIDGET (widget));

	gtk_tree_store_clear (editor->model);

	editor->widget = widget;
	editor->class = widget ? widget->class : NULL;

	if (!widget)
		return;

	for (list = editor->class->signals; list; list = list->next)
	{
		GladeWidgetClassSignal *signal = (GladeWidgetClassSignal *) list->data;
  
		if (strcmp(last_type, signal->type))
		{
			gtk_tree_store_append (editor->model, &parent_class, NULL);
			gtk_tree_store_set (editor->model, &parent_class, COLUMN_SIGNAL, signal->type, COLUMN_VISIBLE, FALSE, COLUMN_SLOT, FALSE, -1);
			last_type = signal->type;
		}

		gtk_tree_store_append (editor->model, &parent_signal, &parent_class);
		signals = glade_widget_find_signals_by_name (widget, signal->name);

		if (!signals)
			gtk_tree_store_set (editor->model, &parent_signal,
					    COLUMN_SIGNAL, signal->name,
					    COLUMN_HANDLER, _("<Type the signal's handler here>"),
					    COLUMN_AFTER, FALSE,
					    COLUMN_VISIBLE, FALSE,
					    COLUMN_SLOT, TRUE, -1);
		else
		{
			GladeSignal *widget_signal = (GladeSignal*) signals->data;

			gtk_tree_store_set (editor->model, &parent_signal,
					    COLUMN_SIGNAL, signal->name,
					    COLUMN_HANDLER, widget_signal->handler,
					    COLUMN_AFTER, widget_signal->after,
					    COLUMN_VISIBLE, TRUE,
					    COLUMN_SLOT, FALSE, -1);
			for (signals = signals->next; signals; signals = signals->next)
			{
				widget_signal = (GladeSignal*) signals->data;
				gtk_tree_store_append (editor->model, &iter, &parent_signal);
				gtk_tree_store_set (editor->model, &iter,
						    COLUMN_HANDLER, widget_signal->handler,
						    COLUMN_AFTER, widget_signal->after,
						    COLUMN_VISIBLE, TRUE,
						    COLUMN_SLOT, FALSE, -1);
			}
		}
	}

	path_first = gtk_tree_path_new_first ();
	gtk_tree_view_expand_row (GTK_TREE_VIEW (editor->signals_list), path_first, FALSE);
	gtk_tree_path_free (path_first);
}

