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
 *   Joaquin Cuenca Abela <e98cuenc@yahoo.com>
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

	COLUMN_AFTER_VISIBLE,
	COLUMN_SLOT, /* if this row contains a "<Type...>" label */
	COLUMN_BOLD,
	NUM_COLUMNS
};

static void
glade_signal_editor_after_toggled (GtkCellRendererToggle *cell,
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
	GladeSignal *signal;
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
	signal = glade_widget_find_signal (editor->widget, old_signal);

	after = !after;
	signal->after = after;

	/* set new value */
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_AFTER, after, -1);

	/* clean up */
	gtk_tree_path_free (path);
}

static gboolean
glade_signal_editor_is_valid_identifier (const char *text)
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

/* TODO: this signal handler has grown well beyond what I was expecting.  We should chop it */
static void
glade_signal_editor_cell_edited (GtkCellRendererText *cell,
				 const gchar         *path_str,
				 const gchar         *new_text,
				 gpointer             data)
{
	GladeSignalEditor *editor = (GladeSignalEditor*) data;
	GtkTreeModel *model = GTK_TREE_MODEL (editor->model);
	GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
	GtkTreeIter iter;
	GtkTreeIter iter_parent;
	GtkTreeIter iter_class;
	GtkTreeIter iter_new_slot;
	GtkTreeIter *iter2;
	GladeSignal *old_signal;
	GladeSignal *signal;
	gchar *signal_name;
	gboolean after;
	char *old_handler;

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

	gtk_tree_model_iter_parent (model, &iter_class, &iter_parent);

	old_signal = glade_signal_new (signal_name, old_handler, after);
	g_free (old_handler);
	g_free (signal_name);
	signal = glade_widget_find_signal (editor->widget, old_signal);

	/* check that the new_text is a valid identifier.  TODO: I don't like that! We're throwing away the text of the user
	 * without even giving him an explanation.  We should keep the text and say him that it's invalid, and why.  The
	 * text should be marked specially (maybe in red) and not added to the list of signals, but we want to keep it just
	 * in case it was just a little typo.  Otherwise he will write its signal handler, make the type, and lost the whole
	 * string because it got substituted by <Type...> stuff */
	if (glade_signal_editor_is_valid_identifier (new_text))
	{
		if (signal == NULL)
		{
			/* we're adding a new signal */
			g_free (old_signal->handler);
			old_signal->handler = g_strdup (new_text);

			glade_widget_add_signal (editor->widget, old_signal);
			gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_HANDLER, old_signal->handler, COLUMN_AFTER_VISIBLE, TRUE, COLUMN_SLOT, FALSE, -1);

			/* append a <Type...> slot */
			gtk_tree_store_append (GTK_TREE_STORE (model), &iter_new_slot, &iter_parent);
			gtk_tree_store_set (GTK_TREE_STORE (model), &iter_new_slot,
					    COLUMN_HANDLER, _("<Type the signal's handler here>"),
					    COLUMN_AFTER, FALSE,
					    COLUMN_AFTER_VISIBLE, FALSE,
					    COLUMN_SLOT, TRUE, -1);

			/* mark the signal & class name as bold */
			gtk_tree_store_set (GTK_TREE_STORE (model), &iter_parent, COLUMN_BOLD, TRUE, -1);
			gtk_tree_store_set (GTK_TREE_STORE (model), &iter_class, COLUMN_BOLD, TRUE, -1);
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
					    COLUMN_AFTER, FALSE, COLUMN_AFTER_VISIBLE, FALSE, COLUMN_SLOT, TRUE, -1);
		}
		else
		{
			/* we're erasing a signal */
			glade_widget_remove_signal (editor->widget, signal);
			/* TODO: remove the bold when we remove the last signal */
			if (memcmp(&iter_parent, &iter, sizeof(GtkTreeIter)) == 0)
			{
				/* ok, we're editing the very first signal, and thus we can't just remove the entire row,
				 * as it also contains the signal's name */
				char *next_handler;
				gboolean next_after;
				gboolean next_visible;
				gboolean next_slot;
				GtkTreeIter next_iter;

				gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_HANDLER, _("<Type the signal's handler here>"),
						    COLUMN_AFTER, FALSE, COLUMN_AFTER_VISIBLE, FALSE, COLUMN_SLOT, TRUE, -1);

				/* Copy the first children of iter to iter, and remove it */
				/* we're at the top, and we're erasing something.  There should be at least a <Type...> child! */
				g_assert (gtk_tree_model_iter_has_child (model, &iter));

				gtk_tree_model_iter_nth_child (model, &next_iter, &iter, 0);
				gtk_tree_model_get (model, &next_iter, COLUMN_HANDLER, &next_handler, COLUMN_AFTER, &next_after, COLUMN_AFTER_VISIBLE, &next_visible, COLUMN_SLOT, &next_slot, -1);
				gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_HANDLER, next_handler, COLUMN_AFTER, next_after, COLUMN_AFTER_VISIBLE, next_visible, COLUMN_SLOT, next_slot, -1);
				gtk_tree_store_remove (GTK_TREE_STORE (model), &next_iter);
				g_free (next_handler);

				/* we've erased the last handler for this signal.  We should thus remove the bold */
				if (next_slot)
				{
					gboolean keep_bold = FALSE;
					gtk_tree_store_set (GTK_TREE_STORE (model), &iter_parent, COLUMN_BOLD, FALSE, -1);

					/* loop on all the children of the class, if at least one of them still is bold,
					 * keep the signal class bold */
					gtk_tree_model_iter_children (model, &iter, &iter_class);
					do
					{
						gboolean bold;
						gtk_tree_model_get (model, &iter, COLUMN_BOLD, &bold, -1);
						if (bold)
						{
							keep_bold = TRUE;
							break;
						}
					}
					while (gtk_tree_model_iter_next (model, &iter));

					if (!keep_bold)
						gtk_tree_store_set (GTK_TREE_STORE (model), &iter_class, COLUMN_BOLD, FALSE, -1);
				}
			}
			else
				gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
		}
	}

	gtk_tree_iter_free (iter2);
	gtk_tree_path_free (path);
	glade_signal_free (old_signal);
}

static void
row_activated (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *tree_view_column, gpointer user_data)
{
	gtk_tree_view_set_cursor (view, path, tree_view_column, TRUE);
	gtk_widget_grab_focus (GTK_WIDGET (view));
}

static GtkWidget *
glade_signal_editor_construct_signals_list (GladeSignalEditor *editor)
{
	GtkTreeView *view;
	GtkWidget *view_widget;
	GtkTreeViewColumn *column;
 	GtkCellRenderer *renderer;
	GtkTreeModel *model;

	editor->model = gtk_tree_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);
	model = GTK_TREE_MODEL (editor->model);

	view_widget = gtk_tree_view_new_with_model (model);
	view = GTK_TREE_VIEW (view_widget);

	/* the view now holds a reference, we can get rid of our own */
	g_object_unref (G_OBJECT (editor->model));

	g_signal_connect(view, "row-activated", (GCallback) row_activated, NULL);

	/* signal column */
 	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), "weight", PANGO_WEIGHT_BOLD, NULL); 
	column = gtk_tree_view_column_new_with_attributes (_("Signal"), renderer, "text", COLUMN_SIGNAL, "weight-set", COLUMN_BOLD, NULL);
 	gtk_tree_view_append_column (view, column);

	/* handler column */
 	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), "editable", TRUE, "style", PANGO_STYLE_ITALIC, "foreground", "Gray", NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (glade_signal_editor_cell_edited), editor);
	column = gtk_tree_view_column_new_with_attributes (_("Handler"), renderer, "text", COLUMN_HANDLER, "style_set", COLUMN_SLOT, "foreground_set", COLUMN_SLOT, NULL);
 	gtk_tree_view_append_column (view, column);

	/* after column */
	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled", G_CALLBACK (glade_signal_editor_after_toggled), editor);
 	column = gtk_tree_view_column_new_with_attributes (_("After"), renderer, "active", COLUMN_AFTER, "visible", COLUMN_AFTER_VISIBLE, NULL);
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
	GArray *signals;

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
			gtk_tree_store_set (editor->model, &parent_class, COLUMN_SIGNAL, signal->type, COLUMN_AFTER_VISIBLE, FALSE, COLUMN_SLOT, FALSE, COLUMN_BOLD, FALSE, -1);
			last_type = signal->type;
		}

		gtk_tree_store_append (editor->model, &parent_signal, &parent_class);
		signals = glade_widget_find_signals_by_name (widget, signal->name);

		if (!signals || signals->len == 0)
			gtk_tree_store_set (editor->model, &parent_signal,
					    COLUMN_SIGNAL, signal->name,
					    COLUMN_HANDLER, _("<Type the signal's handler here>"),
					    COLUMN_AFTER, FALSE,
					    COLUMN_AFTER_VISIBLE, FALSE,
					    COLUMN_SLOT, TRUE, -1);
		else
		{
			size_t i;
			GtkTreePath *path_parent_class;
			GladeSignal *widget_signal = (GladeSignal*) g_array_index (signals, GladeSignal*, 0);

			/* mark the class of this signal as bold and expand it, as there is at least one signal with handler */
			gtk_tree_store_set (editor->model, &parent_class, COLUMN_BOLD, TRUE, -1);
			path_parent_class = gtk_tree_model_get_path (GTK_TREE_MODEL (editor->model), &parent_class);
			gtk_tree_view_expand_row (GTK_TREE_VIEW (editor->signals_list), path_parent_class, FALSE);
			gtk_tree_path_free (path_parent_class);

			gtk_tree_store_set (editor->model, &parent_signal,
					    COLUMN_SIGNAL, signal->name,
					    COLUMN_HANDLER, widget_signal->handler,
					    COLUMN_AFTER, widget_signal->after,
					    COLUMN_AFTER_VISIBLE, TRUE,
					    COLUMN_SLOT, FALSE,
					    COLUMN_BOLD, TRUE, -1);
			for (i = 1; i < signals->len; i++)
			{
				widget_signal = (GladeSignal*) g_array_index (signals, GladeSignal*, i);
				gtk_tree_store_append (editor->model, &iter, &parent_signal);
				gtk_tree_store_set (editor->model, &iter,
						    COLUMN_HANDLER, widget_signal->handler,
						    COLUMN_AFTER, widget_signal->after,
						    COLUMN_AFTER_VISIBLE, TRUE,
						    COLUMN_SLOT, FALSE, -1);
			}

			/* add the <Type...> slot */
			gtk_tree_store_append (editor->model, &iter, &parent_signal);
			gtk_tree_store_set (editor->model, &iter,
					    COLUMN_HANDLER, _("<Type the signal's handler here>"),
					    COLUMN_AFTER, FALSE,
					    COLUMN_AFTER_VISIBLE, FALSE,
					    COLUMN_SLOT, TRUE, -1);
		}
	}

	path_first = gtk_tree_path_new_first ();
	gtk_tree_view_expand_row (GTK_TREE_VIEW (editor->signals_list), path_first, FALSE);
	gtk_tree_path_free (path_first);
}

