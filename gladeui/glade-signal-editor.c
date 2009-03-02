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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Shane Butler <shane_b@users.sourceforge.net>
 *   Joaquin Cuenca Abela <e98cuenc@yahoo.com>
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 * SECTION:glade-signal-editor
 * @Title: GladeSignalEditor
 * @Short_Description: An interface to edit signals for a #GladeWidget.
 *
 * This isnt really a dockable widget, since you need to access the
 * #GladeSignalEditor struct's '->main_window' widget, the signal editor
 * is mostly of interest when implementing a custom object editor.
 */

#include <string.h>
#include <glib/gi18n-lib.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-widget-adaptor.h"
#include "glade-signal.h"
#include "glade-signal-editor.h"
#include "glade-editor.h"
#include "glade-command.h"
#include "glade-marshallers.h"
#include "glade-accumulators.h"


enum
{
	HANDLER_EDITING_STARTED,
	HANDLER_EDITING_DONE,
	USERDATA_EDITING_STARTED,
	USERDATA_EDITING_DONE,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_HANDLER_COLUMN,
	PROP_USERDATA_COLUMN,
	PROP_HANDLER_COMPLETION,
	PROP_USERDATA_COMPLETION,
	PROP_HANDLER_RENDERER,
	PROP_USERDATA_RENDERER
};

static guint glade_signal_editor_signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE (GladeSignalEditor, glade_signal_editor, G_TYPE_OBJECT)


#define HANDLER_DEFAULT  _("<Type here>")
#define USERDATA_DEFAULT HANDLER_DEFAULT

static gboolean
is_void_handler (const gchar *signal_handler)
{
	return ( signal_handler == NULL ||
		*signal_handler == 0    ||
		 g_utf8_collate (signal_handler, _(HANDLER_DEFAULT)) == 0);
}

static gboolean
is_void_userdata (const gchar *user_data)
{
	return ( user_data == NULL ||
		*user_data == 0    ||
		 g_utf8_collate (user_data, _(USERDATA_DEFAULT)) == 0);
}

static void
glade_signal_editor_after_toggled (GtkCellRendererToggle *cell,
				   gchar                 *path_str,
				   gpointer               data)
{
	GladeSignalEditor *editor = (GladeSignalEditor*) data;
	GtkTreeModel *model = GTK_TREE_MODEL (editor->model);
	GtkTreeIter  iter;
	GtkTreeIter iter_parent;
	GladeSignal *old_signal;
	GladeSignal *new_signal;
	GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
	gchar     *signal_name;
	gchar     *handler;
	gchar     *userdata;
	gboolean   lookup;
	gboolean   after;

	/* get toggled iter */
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    GSE_COLUMN_SIGNAL,  &signal_name,
			    GSE_COLUMN_HANDLER, &handler,
			    GSE_COLUMN_USERDATA,&userdata,
			    GSE_COLUMN_LOOKUP,  &lookup,
			    GSE_COLUMN_AFTER,   &after, -1);
	if (signal_name == NULL)
	{
		if (!gtk_tree_model_iter_parent (model, &iter_parent, &iter))
			g_assert (FALSE);

		gtk_tree_model_get (model, &iter_parent, GSE_COLUMN_SIGNAL, &signal_name, -1);
		g_assert (signal_name != NULL);
	}

	if (is_void_userdata (userdata))
	{
		g_free (userdata);
		userdata = NULL;
	}
	
	old_signal = glade_signal_new (signal_name, handler, userdata, after);
	new_signal = glade_signal_new (signal_name, handler, userdata, !after);

	glade_command_change_signal (editor->widget, old_signal, new_signal);
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter, GSE_COLUMN_AFTER, !after, -1);

	glade_signal_free (old_signal);
	glade_signal_free (new_signal);
	gtk_tree_path_free (path);
	g_free (signal_name);
	g_free (handler);
	g_free (userdata);
}

/*
  glade files do not support symbol names as signal handler user_data arguments
  yet, so we disabled the lookup column.
*/
#define LOOKUP_COLUMN 0

#if LOOKUP_COLUMN
static void
glade_signal_editor_lookup_toggled (GtkCellRendererToggle *cell,
				    gchar                 *path_str,
				    gpointer               data)
{
	GladeSignalEditor *editor = (GladeSignalEditor*) data;
	GtkTreeModel *model = GTK_TREE_MODEL (editor->model);
	GtkTreeIter  iter;
	GtkTreeIter iter_parent;
	GladeSignal *old_signal;
	GladeSignal *new_signal;
	GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
	gchar     *signal_name;
	gchar     *handler;
	gchar     *userdata;
	gboolean   lookup;
	gboolean   after;

	/* get toggled iter */
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    GSE_COLUMN_SIGNAL,  &signal_name,
			    GSE_COLUMN_HANDLER, &handler,
			    GSE_COLUMN_USERDATA,&userdata,
			    GSE_COLUMN_LOOKUP,  &lookup,
			    GSE_COLUMN_AFTER,   &after, -1);
	if (signal_name == NULL)
	{
		if (!gtk_tree_model_iter_parent (model, &iter_parent, &iter))
			g_assert (FALSE);

		gtk_tree_model_get (model, &iter_parent, GSE_COLUMN_SIGNAL, &signal_name, -1);
		g_assert (signal_name != NULL);
	}

	if (is_void_userdata (userdata))
	{
		g_free (userdata);
		userdata = NULL;
	}
	
	old_signal = glade_signal_new (signal_name, handler, userdata, lookup, after);
	new_signal = glade_signal_new (signal_name, handler, userdata, !lookup, after);

	glade_command_change_signal (editor->widget, old_signal, new_signal);
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter, GSE_COLUMN_LOOKUP, !lookup, -1);

	glade_signal_free (old_signal);
	glade_signal_free (new_signal);
	gtk_tree_path_free (path);
	g_free (signal_name);
	g_free (handler);
	g_free (userdata);
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
#endif

static void
append_slot (GtkTreeModel *model, GtkTreeIter *iter_signal)
{
	GtkTreeIter iter_new_slot;
	GtkTreeIter iter_class;

	gtk_tree_store_append (GTK_TREE_STORE (model), &iter_new_slot, iter_signal);
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter_new_slot,
			    GSE_COLUMN_HANDLER,          _(HANDLER_DEFAULT),
			    GSE_COLUMN_USERDATA,         _(USERDATA_DEFAULT),
			    GSE_COLUMN_LOOKUP,           FALSE,
			    GSE_COLUMN_LOOKUP_VISIBLE,   FALSE,
			    GSE_COLUMN_HANDLER_EDITABLE, TRUE,
			    GSE_COLUMN_USERDATA_EDITABLE,FALSE,
			    GSE_COLUMN_AFTER,            FALSE,
			    GSE_COLUMN_AFTER_VISIBLE,    FALSE,
			    GSE_COLUMN_SLOT,             TRUE,
			    GSE_COLUMN_USERDATA_SLOT,    TRUE,
			    -1);
	gtk_tree_model_iter_parent (model, &iter_class, iter_signal);

	/* mark the signal & class name as bold */
	gtk_tree_store_set (GTK_TREE_STORE (model), iter_signal, GSE_COLUMN_BOLD, TRUE, -1);
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter_class, GSE_COLUMN_BOLD, TRUE, -1);
}

static void
move_row (GtkTreeModel *model, GtkTreeIter *from, GtkTreeIter *to)
{
	gchar *handler;
	gchar *userdata;
	gboolean after;
	gboolean slot;
	gboolean visible;
	gboolean userdata_slot;
	gboolean handler_editable;
	gboolean userdata_editable;
	gboolean lookup;
	gboolean lookup_visible;
	gboolean bold;

	gtk_tree_model_get (model,                     from,
			    GSE_COLUMN_HANDLER,           &handler,
			    GSE_COLUMN_USERDATA,          &userdata,
			    GSE_COLUMN_AFTER,             &after,
			    GSE_COLUMN_SLOT,              &slot,
			    GSE_COLUMN_AFTER_VISIBLE,     &visible,
			    GSE_COLUMN_HANDLER_EDITABLE,  &handler_editable,
			    GSE_COLUMN_USERDATA_EDITABLE, &userdata_editable,
			    GSE_COLUMN_USERDATA_SLOT,     &userdata_slot,
			    GSE_COLUMN_LOOKUP,            &lookup,
			    GSE_COLUMN_LOOKUP_VISIBLE,    &lookup_visible,
			    GSE_COLUMN_BOLD,              &bold,
			    -1);

	gtk_tree_store_set (GTK_TREE_STORE (model),   to,
			    GSE_COLUMN_HANDLER,           handler,
			    GSE_COLUMN_USERDATA,          userdata,
			    GSE_COLUMN_AFTER,             after,
			    GSE_COLUMN_SLOT,              slot,
			    GSE_COLUMN_AFTER_VISIBLE,     visible,
			    GSE_COLUMN_HANDLER_EDITABLE,  handler_editable,
			    GSE_COLUMN_USERDATA_EDITABLE, userdata_editable,
			    GSE_COLUMN_USERDATA_SLOT,     userdata_slot,
			    GSE_COLUMN_LOOKUP,            lookup,
			    GSE_COLUMN_LOOKUP_VISIBLE,    lookup_visible,
			    GSE_COLUMN_BOLD,              &bold,
			    -1);
	g_free (handler);
	g_free (userdata);
}

static void
remove_slot (GtkTreeModel *model, GtkTreeIter *iter, GtkTreeIter *iter_signal)
{
	GtkTreeIter iter_class;

	gtk_tree_model_iter_parent (model, &iter_class, iter_signal);

	/* special case for removing the handler of the first row */
	if (iter == NULL)
	{
		GtkTreeIter first_iter;

		gtk_tree_model_iter_nth_child (model, &first_iter, iter_signal, 0);
		move_row (model, &first_iter, iter_signal);
		gtk_tree_store_remove (GTK_TREE_STORE (model), &first_iter);
	}
	else
		gtk_tree_store_remove (GTK_TREE_STORE (model), iter);

	if (!gtk_tree_model_iter_has_child (model, iter_signal))
	{
		/* mark the signal & class name as normal */
		gtk_tree_store_set (GTK_TREE_STORE (model), iter_signal,
				    GSE_COLUMN_BOLD, FALSE, -1);
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter_class,
				    GSE_COLUMN_BOLD, FALSE, -1);
	}
}

static gboolean
glade_signal_editor_handler_editing_done_impl  (GladeSignalEditor *self,
						gchar *signal_name,
						gchar *old_handler,
						gchar *new_handler,
						GtkTreeIter *iter)
{
	GladeWidget *glade_widget = self->widget;
	GtkTreeModel *model = GTK_TREE_MODEL (self->model);
	gchar *tmp_signal_name;
	gchar *userdata;
	GtkTreeIter iter_signal;
	gboolean  after;
	gboolean is_top_handler;

	gtk_tree_model_get (model,           iter,
			    GSE_COLUMN_SIGNAL,   &tmp_signal_name,
			    GSE_COLUMN_USERDATA, &userdata,
			    GSE_COLUMN_AFTER,    &after, -1);

	if (self->is_void_userdata (userdata))
	{
		g_free (userdata);
		userdata = NULL;
	}

	if (tmp_signal_name == NULL)
	{
		is_top_handler = FALSE;
		gtk_tree_model_iter_parent (model, &iter_signal, iter);
	}
	else
	{
		is_top_handler = TRUE;
		iter_signal = *iter;
		g_free (tmp_signal_name);
	}

	/* we're adding a new handler */
	if (old_handler == NULL && new_handler)
	{
		GladeSignal *new_signal = glade_signal_new (signal_name, new_handler,
							    NULL, FALSE);
		glade_command_add_signal (glade_widget, new_signal);
		glade_signal_free (new_signal);
		gtk_tree_store_set (GTK_TREE_STORE (model), iter,
				    GSE_COLUMN_HANDLER,          new_handler,
				    GSE_COLUMN_AFTER_VISIBLE,    TRUE,
				    GSE_COLUMN_SLOT,             FALSE,
				    GSE_COLUMN_USERDATA_EDITABLE,TRUE, -1);

		/* append a <Type...> slot */
		append_slot (model, &iter_signal);
	}

	/* we're removing a signal handler */
	if (old_handler && new_handler == NULL)
	{
		GladeSignal *old_signal =
			glade_signal_new (signal_name,
					  old_handler,
					  userdata, 
					  after);
		glade_command_remove_signal (glade_widget, old_signal);
		glade_signal_free (old_signal);

		gtk_tree_store_set
			(GTK_TREE_STORE (model),          iter,
				 GSE_COLUMN_HANDLER,          _(HANDLER_DEFAULT),
				 GSE_COLUMN_AFTER,            FALSE,
				 GSE_COLUMN_USERDATA,         _(USERDATA_DEFAULT),
				 GSE_COLUMN_LOOKUP,           FALSE,
				 GSE_COLUMN_LOOKUP_VISIBLE,   FALSE,
				 GSE_COLUMN_HANDLER_EDITABLE, TRUE,
				 GSE_COLUMN_USERDATA_EDITABLE,FALSE,
				 GSE_COLUMN_AFTER_VISIBLE,    FALSE,
				 GSE_COLUMN_SLOT,             TRUE,
				 GSE_COLUMN_USERDATA_SLOT,    TRUE,
				 -1);

		remove_slot (model, is_top_handler ? NULL : iter, &iter_signal);
	}

	/* we're changing a signal handler */
	if (old_handler && new_handler)
	{
		GladeSignal *old_signal =
			glade_signal_new
			(signal_name,
			 old_handler,
			 userdata,
			 after);
		GladeSignal *new_signal =
			glade_signal_new
			(signal_name,
			 new_handler,
			 userdata,
			 after);

		if (glade_signal_equal (old_signal, new_signal) == FALSE)
			glade_command_change_signal (glade_widget, old_signal, new_signal);

		glade_signal_free (old_signal);
		glade_signal_free (new_signal);

		gtk_tree_store_set (GTK_TREE_STORE (model), iter,
				    GSE_COLUMN_HANDLER,       new_handler,
				    GSE_COLUMN_AFTER_VISIBLE, TRUE,
				    GSE_COLUMN_SLOT,          FALSE,
				    GSE_COLUMN_USERDATA_EDITABLE,TRUE, -1);
	}

	g_free (userdata);

	return FALSE;
}

static gboolean
glade_signal_editor_userdata_editing_done_impl (GladeSignalEditor *self,
						gchar *signal_name,
						gchar *old_userdata,
						gchar *new_userdata,
						GtkTreeIter *iter)
{
	GtkTreeModel *model = GTK_TREE_MODEL (self->model);
	GladeWidget  *glade_widget = self->widget;
	gchar *handler;
	gboolean after;
	GladeSignal *old_signal, *new_signal;

	gtk_tree_model_get (model,           iter,
			    GSE_COLUMN_HANDLER,  &handler,
			    GSE_COLUMN_AFTER,    &after, -1);

	/* We are removing userdata */
	if (new_userdata == NULL)
	{
		gtk_tree_store_set (GTK_TREE_STORE (model), iter,
				    GSE_COLUMN_USERDATA_SLOT,  TRUE,
				    GSE_COLUMN_USERDATA,       _(USERDATA_DEFAULT),
				    GSE_COLUMN_LOOKUP,         FALSE,
				    GSE_COLUMN_LOOKUP_VISIBLE, FALSE, -1);
	}
	else
	{
		gtk_tree_store_set (GTK_TREE_STORE (model), iter,
			    GSE_COLUMN_USERDATA_SLOT,  FALSE,
			    GSE_COLUMN_USERDATA,       new_userdata,
			    GSE_COLUMN_LOOKUP_VISIBLE, TRUE,
			    -1);
	}

	old_signal = glade_signal_new (signal_name, handler, old_userdata, after);

	new_signal = glade_signal_new (signal_name, handler, new_userdata, after);

	if (glade_signal_equal (old_signal, new_signal) == FALSE)
		glade_command_change_signal (glade_widget, old_signal, new_signal);

	glade_signal_free (old_signal);
	glade_signal_free (new_signal);

	g_free (handler);
	return FALSE;
}

static gchar *
glade_signal_editor_get_signal_name (GtkTreeModel *model, GtkTreeIter *iter)
{
	gchar *signal_name;
	gtk_tree_model_get (model,           iter,
			    GSE_COLUMN_SIGNAL,   &signal_name,
			    -1);
	if (signal_name == NULL)
	{
		GtkTreeIter iter_signal;

		if (!gtk_tree_model_iter_parent (model, &iter_signal, iter))
			g_assert (FALSE);

		gtk_tree_model_get (model, &iter_signal, GSE_COLUMN_SIGNAL, &signal_name, -1);
		g_assert (signal_name != NULL);
	}

	return signal_name;
}

static void
glade_signal_editor_column_cell_edited (const gchar *path_str,
					const gchar *new_value,
					gpointer     data,
					gint         column,
					guint        signal,
					IsVoidFunc   is_void_callback)
{
	GladeSignalEditor* self = GLADE_SIGNAL_EDITOR (data);
	GtkTreeModel *model = GTK_TREE_MODEL (self->model);
	GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
	GtkTreeIter iter;
	gchar    *signal_name;
	gchar    *old_value;
	gboolean  handled;

	g_return_if_fail (gtk_tree_model_get_iter (model, &iter, path));
	gtk_tree_path_free (path);
	gtk_tree_model_get (model,  &iter,
			    column, &old_value,
			    -1);

	signal_name = glade_signal_editor_get_signal_name (model, &iter);

	if (is_void_callback (new_value))
		new_value = NULL;

	if (is_void_callback (old_value))
	{
		g_free (old_value);
		old_value = NULL;
	}

	/* if not a false alarm */
	if (old_value || new_value)
		g_signal_emit (self, glade_signal_editor_signals[signal],
			       0, signal_name, old_value, new_value, &iter, &handled);

	g_free (signal_name);
	g_free (old_value);
}

static void
glade_signal_editor_handler_cell_edited (GtkCellRendererText *cell,
					 const gchar         *path_str,
					 const gchar         *new_handler,
					 gpointer             data)
{
	GladeSignalEditor *editor = GLADE_SIGNAL_EDITOR (data);

	glade_signal_editor_column_cell_edited (path_str, new_handler, data,
						GSE_COLUMN_HANDLER,
						HANDLER_EDITING_DONE,
						editor->is_void_handler);
}

static void
glade_signal_editor_userdata_cell_edited (GtkCellRendererText *cell,
					  const gchar         *path_str,
					  const gchar         *new_userdata,
					  gpointer             data)
{
	GladeSignalEditor *editor = GLADE_SIGNAL_EDITOR (data);

	glade_signal_editor_column_cell_edited (path_str, new_userdata, data,
						GSE_COLUMN_USERDATA,
						USERDATA_EDITING_DONE,
						editor->is_void_userdata);
}

static void
glade_signal_editor_column_editing_started (GtkCellEditable *editable,
					    const gchar *path_str,
					    GladeSignalEditor *editor,
					    guint signal)
{
	GtkTreeModel *model = GTK_TREE_MODEL (editor->model);
	GtkTreePath *path;
	GtkTreeIter iter;
	gchar *signal_name;
	gboolean handled;

	path = gtk_tree_path_new_from_string (path_str);
	g_return_if_fail (gtk_tree_model_get_iter (model, &iter, path));
	gtk_tree_path_free (path);
	signal_name = glade_signal_editor_get_signal_name (model, &iter);
	g_signal_emit (editor, glade_signal_editor_signals[signal], 0,
		       signal_name, &iter, editable, &handled);
	g_free (signal_name);
}

static void
glade_signal_editor_handler_editing_started (GtkCellRenderer *cell,
					     GtkCellEditable *editable,
					     const gchar *path,
					     GladeSignalEditor *editor)
{
	glade_signal_editor_column_editing_started (editable, path,
						    editor, HANDLER_EDITING_STARTED);
}

static void
glade_signal_editor_userdata_editing_started (GtkCellRenderer *cell,
					      GtkCellEditable *editable,
					      const gchar *path,
					      GladeSignalEditor *editor)
{
	glade_signal_editor_column_editing_started (editable, path,
						    editor, USERDATA_EDITING_STARTED);
}

static void
glade_signal_editor_signal_cell_data_func (GtkTreeViewColumn *tree_column,
					   GtkCellRenderer *cell,
					   GtkTreeModel *tree_model,
					   GtkTreeIter *iter,
					   gpointer data)
{
	gboolean bold;

	gtk_tree_model_get (tree_model, iter, GSE_COLUMN_BOLD, &bold, -1);
	if (bold)
		g_object_set (G_OBJECT (cell), "weight", PANGO_WEIGHT_BOLD, NULL);
	else
		g_object_set (G_OBJECT (cell), "weight", PANGO_WEIGHT_NORMAL, NULL);
}

static void
glade_signal_editor_handler_cell_data_func (GtkTreeViewColumn *tree_column,
					    GtkCellRenderer *cell,
					    GtkTreeModel *tree_model,
					    GtkTreeIter *iter,
					    gpointer data)
{
	gboolean slot;

	gtk_tree_model_get (tree_model, iter, GSE_COLUMN_SLOT, &slot, -1);
	if (slot)
		g_object_set (G_OBJECT (cell),
			      "style", PANGO_STYLE_ITALIC,
			      "foreground", "Gray", NULL);
	else
		g_object_set (G_OBJECT (cell),
			      "style", PANGO_STYLE_NORMAL,
			      "foreground", NULL, NULL);
}

static void
glade_signal_editor_userdata_cell_data_func (GtkTreeViewColumn *tree_column,
					     GtkCellRenderer *cell,
					     GtkTreeModel *tree_model,
					     GtkTreeIter *iter,
					     gpointer data)
{
	gboolean userdata_slot;

	gtk_tree_model_get (tree_model, iter, GSE_COLUMN_USERDATA_SLOT, &userdata_slot, -1);
	if (userdata_slot)
		g_object_set (G_OBJECT (cell),
			      "style", PANGO_STYLE_ITALIC,
			      "foreground", "Gray", NULL);
	else
		g_object_set (G_OBJECT (cell),
			      "style", PANGO_STYLE_NORMAL,
			      "foreground", NULL, NULL);
}

void
glade_signal_editor_construct_signals_list (GladeSignalEditor *editor)
{
	GtkTreeView *view;
	GtkWidget *view_widget;
	GtkTreeViewColumn *column;
 	GtkCellRenderer *renderer;
	GtkTreeModel *model;

	editor->model = gtk_tree_store_new
		(GSE_NUM_COLUMNS,
		 G_TYPE_STRING,   /* Signal  value      */
		 G_TYPE_STRING,   /* Handler value      */
		 G_TYPE_BOOLEAN,  /* After   value      */
		 G_TYPE_STRING,   /* User data value    */
		 G_TYPE_BOOLEAN,  /* module lookup string for
				   * user data */
		 G_TYPE_BOOLEAN,  /* Whether userdata is a slot */
		 G_TYPE_BOOLEAN,  /* Lookup visibility  */
		 G_TYPE_BOOLEAN,  /* After   visibility */
		 G_TYPE_BOOLEAN,  /* Handler editable   */
		 G_TYPE_BOOLEAN,  /* Userdata editable  */
		 G_TYPE_BOOLEAN,  /* New slot           */
		 G_TYPE_BOOLEAN); /* Mark with bold     */

	model = GTK_TREE_MODEL (editor->model);

	view_widget = gtk_tree_view_new_with_model (model);
	g_object_set (G_OBJECT (view_widget), "enable-search", FALSE, NULL);

	view = GTK_TREE_VIEW (view_widget);

	/* the view now holds a reference, we can get rid of our own */
	g_object_unref (G_OBJECT (editor->model));

	/************************ signal column ************************/
 	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes
		(_("Signal"), renderer, "text", GSE_COLUMN_SIGNAL, NULL);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 glade_signal_editor_signal_cell_data_func,
						 NULL, NULL);
 	gtk_tree_view_append_column (view, column);

	/************************ handler column ************************/
 	if (!editor->handler_store)
			editor->handler_store = GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING));

	if (!editor->handler_renderer)
 	{
 		editor->handler_renderer = gtk_cell_renderer_combo_new ();
		g_object_set (G_OBJECT (editor->handler_renderer),
			      "model", editor->handler_store,
			      "text-column", 0,
			      NULL);
	}

	g_signal_connect (editor->handler_renderer, "edited",
			  G_CALLBACK (glade_signal_editor_handler_cell_edited), editor);

	g_signal_connect (editor->handler_renderer, "editing-started",
			  G_CALLBACK (glade_signal_editor_handler_editing_started),
			  editor);

	if (!editor->handler_column)
	{
		editor->handler_column = gtk_tree_view_column_new_with_attributes
			(_("Handler"),     editor->handler_renderer,
			 "text",           GSE_COLUMN_HANDLER,
			 "editable",       GSE_COLUMN_HANDLER_EDITABLE, NULL);

		gtk_tree_view_column_set_cell_data_func (editor->handler_column, editor->handler_renderer,
							 glade_signal_editor_handler_cell_data_func,
							 NULL, NULL);
	}

 	gtk_tree_view_append_column (view, editor->handler_column);

	/************************ userdata column ************************/
 	if (!editor->userdata_renderer)
 	{
 		editor->userdata_renderer = gtk_cell_renderer_text_new ();
	}
			      
	if (!editor->userdata_store)
		editor->userdata_store = GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING));

	g_signal_connect (editor->userdata_renderer, "edited",
			  G_CALLBACK (glade_signal_editor_userdata_cell_edited), editor);

	g_signal_connect (editor->userdata_renderer, "editing-started",
			  G_CALLBACK (glade_signal_editor_userdata_editing_started),
			  editor);

	if (!editor->userdata_column)
	{
		editor->userdata_column =
			gtk_tree_view_column_new_with_attributes
				(_("User data"), editor->userdata_renderer,
				 "text",           GSE_COLUMN_USERDATA,
				 "editable",       GSE_COLUMN_USERDATA_EDITABLE, NULL);

		gtk_tree_view_column_set_cell_data_func (editor->userdata_column, editor->userdata_renderer,
							 glade_signal_editor_userdata_cell_data_func,
							 NULL, NULL);
	}

 	gtk_tree_view_append_column (view, editor->userdata_column);

#if LOOKUP_COLUMN
	/************************ lookup column ************************/
 	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled",
			  G_CALLBACK (glade_signal_editor_lookup_toggled), editor);
	column = gtk_tree_view_column_new_with_attributes
		(_("Lookup"), renderer,
		 "active",  GSE_COLUMN_LOOKUP,
		 "visible", GSE_COLUMN_LOOKUP_VISIBLE, NULL);

 	gtk_tree_view_append_column (view, column);
#endif
	/************************ after column ************************/
	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled",
			  G_CALLBACK (glade_signal_editor_after_toggled), editor);
 	column = gtk_tree_view_column_new_with_attributes
		(_("After"), renderer,
		 "active",  GSE_COLUMN_AFTER,
		 "visible", GSE_COLUMN_AFTER_VISIBLE, NULL);
 	gtk_tree_view_append_column (view, column);

	editor->signals_list = view_widget;
}

static GObject*
glade_signal_editor_constructor (GType                  type,
				 guint                  n_construct_properties,
				 GObjectConstructParam *construct_properties)
{
	GtkWidget *vbox;
	GtkWidget *scroll;
	GladeSignalEditor *editor;
	GObject *retval;

	retval = G_OBJECT_CLASS (glade_signal_editor_parent_class)->constructor
		(type, n_construct_properties, construct_properties);
	editor = GLADE_SIGNAL_EDITOR (retval);

	vbox = gtk_vbox_new (FALSE, 0);
	editor->main_window = vbox;

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_IN);

	g_signal_emit_by_name (glade_app_get(), "signal-editor-created", retval);

	gtk_container_add (GTK_CONTAINER (scroll), editor->signals_list);

	gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);

	gtk_widget_show_all (editor->main_window);

	return retval;
}

/**
 * glade_signal_editor_get_widget:
 * @editor: a #GladeSignalEditor
 *
 * Returns: the #GtkWidget that is the main window for @editor, or %NULL if
 *          it does not exist
 */
GtkWidget *
glade_signal_editor_get_widget (GladeSignalEditor *editor)
{
	g_return_val_if_fail (GLADE_IS_SIGNAL_EDITOR (editor), NULL);
	g_return_val_if_fail (GTK_IS_WIDGET (editor->main_window), NULL);

	return editor->main_window;
}

/**
 * glade_signal_editor_new:
 * @editor: a #GladeEditor
 *
 * Returns: a new #GladeSignalEditor associated with @editor
 */
GladeSignalEditor *
glade_signal_editor_new (gpointer *editor)
{
	GladeSignalEditor *signal_editor;

	signal_editor = GLADE_SIGNAL_EDITOR (g_object_new (GLADE_TYPE_SIGNAL_EDITOR,
	                                                   NULL, NULL));
	signal_editor->editor = editor;

	return signal_editor;
}

/**
 * glade_signal_editor_load_widget:
 * @editor: a #GladeSignalEditor
 * @widget: a #GladeWidget
 *
 * TODO: write me
 */
void
glade_signal_editor_load_widget (GladeSignalEditor *editor,
				 GladeWidget *widget)
{
	GList *list;
	const gchar *last_type = "";
	GtkTreeIter iter;
	GtkTreeIter parent_class;
	GtkTreeIter parent_signal;
	GtkTreePath *path_first;
	GPtrArray *signals;

	g_return_if_fail (GLADE_IS_SIGNAL_EDITOR (editor));
	g_return_if_fail (widget == NULL || GLADE_IS_WIDGET (widget));

	gtk_tree_store_clear (editor->model);

	editor->widget = widget;
	editor->adaptor = widget ? widget->adaptor : NULL;

	if (!widget)
		return;

	/* Loop over every signal type
	 */
	for (list = editor->adaptor->signals; list; list = list->next)
	{
		GladeSignalClass *signal = (GladeSignalClass *) list->data;

		/* Add class name that this signal belongs to.
		 */
		if (strcmp(last_type, signal->type))
		{
			gtk_tree_store_append (editor->model, &parent_class, NULL);
			gtk_tree_store_set    (editor->model,          &parent_class,
					       GSE_COLUMN_SIGNAL,           signal->type,
					       GSE_COLUMN_AFTER_VISIBLE,    FALSE,
					       GSE_COLUMN_HANDLER_EDITABLE, FALSE,
					       GSE_COLUMN_USERDATA_EDITABLE,FALSE,
					       GSE_COLUMN_SLOT,             FALSE,
					       GSE_COLUMN_BOLD,             FALSE, -1);
			last_type = signal->type;
		}

		gtk_tree_store_append (editor->model, &parent_signal, &parent_class);
		signals = glade_widget_list_signal_handlers (widget, signal->name);

		if (!signals || signals->len == 0)
		{
			gtk_tree_store_set
				(editor->model,          &parent_signal,
				 GSE_COLUMN_SIGNAL,           signal->name,
				 GSE_COLUMN_HANDLER,          _(HANDLER_DEFAULT),
				 GSE_COLUMN_AFTER,            FALSE,
				 GSE_COLUMN_USERDATA,         _(USERDATA_DEFAULT),
				 GSE_COLUMN_LOOKUP,           FALSE,
				 GSE_COLUMN_LOOKUP_VISIBLE,   FALSE,
				 GSE_COLUMN_HANDLER_EDITABLE, TRUE,
				 GSE_COLUMN_USERDATA_EDITABLE,FALSE,
				 GSE_COLUMN_AFTER_VISIBLE,    FALSE,
				 GSE_COLUMN_SLOT,             TRUE,
				 GSE_COLUMN_USERDATA_SLOT,    TRUE,
				 -1);
		}
		else
		{
			guint i;
			GtkTreePath *path_parent_class;
			GladeSignal *widget_signal =
				(GladeSignal*) g_ptr_array_index (signals, 0);

			/* mark the class of this signal as bold and expand it,
                         * as there is at least one signal with handler */
			gtk_tree_store_set (editor->model, &parent_class, GSE_COLUMN_BOLD, TRUE, -1);
			path_parent_class =
				gtk_tree_model_get_path (GTK_TREE_MODEL (editor->model),
							 &parent_class);
			gtk_tree_view_expand_row (GTK_TREE_VIEW (editor->signals_list),
						  path_parent_class, FALSE);
			gtk_tree_path_free (path_parent_class);

			gtk_tree_store_set
				(editor->model,            &parent_signal,
				 GSE_COLUMN_SIGNAL,             signal->name,
				 GSE_COLUMN_HANDLER,            widget_signal->handler,
				 GSE_COLUMN_AFTER,              widget_signal->after,
				 GSE_COLUMN_USERDATA,
				 widget_signal->userdata ?
				 widget_signal->userdata : _(USERDATA_DEFAULT),
				 GSE_COLUMN_LOOKUP,             FALSE/* widget_signal->lookup */,
				 GSE_COLUMN_LOOKUP_VISIBLE,
				 widget_signal->userdata ?  TRUE : FALSE,
				 GSE_COLUMN_AFTER_VISIBLE,      TRUE,
				 GSE_COLUMN_HANDLER_EDITABLE,   TRUE,
				 GSE_COLUMN_USERDATA_EDITABLE,  TRUE,
				 GSE_COLUMN_SLOT,               FALSE,
				 GSE_COLUMN_USERDATA_SLOT,
				 widget_signal->userdata  ? FALSE : TRUE,
				 GSE_COLUMN_BOLD,               TRUE, -1);

			for (i = 1; i < signals->len; i++)
			{
				widget_signal = (GladeSignal*) g_ptr_array_index (signals, i);
				gtk_tree_store_append (editor->model, &iter, &parent_signal);

				gtk_tree_store_set
					(editor->model,            &iter,
					 GSE_COLUMN_HANDLER,            widget_signal->handler,
					 GSE_COLUMN_AFTER,              widget_signal->after,
					 GSE_COLUMN_USERDATA,
					 widget_signal->userdata  ?
					 widget_signal->userdata : _(USERDATA_DEFAULT),
					 GSE_COLUMN_LOOKUP,         FALSE/* widget_signal->lookup */,
					 GSE_COLUMN_LOOKUP_VISIBLE,
					 widget_signal->userdata  ? TRUE : FALSE,
					 GSE_COLUMN_AFTER_VISIBLE,      TRUE,
					 GSE_COLUMN_HANDLER_EDITABLE,   TRUE,
					 GSE_COLUMN_USERDATA_EDITABLE,  TRUE,
					 GSE_COLUMN_SLOT,               FALSE,
					 GSE_COLUMN_USERDATA_SLOT,
					 widget_signal->userdata  ? FALSE : TRUE,
					 -1);
			}

			/* add the <Type...> slot */
			gtk_tree_store_append (editor->model, &iter, &parent_signal);
			gtk_tree_store_set
				(editor->model,          &iter,
				 GSE_COLUMN_HANDLER,          _(HANDLER_DEFAULT),
				 GSE_COLUMN_AFTER,            FALSE,
				 GSE_COLUMN_USERDATA,         _(USERDATA_DEFAULT),
				 GSE_COLUMN_LOOKUP,           FALSE,
				 GSE_COLUMN_LOOKUP_VISIBLE,   FALSE,
				 GSE_COLUMN_HANDLER_EDITABLE, TRUE,
				 GSE_COLUMN_USERDATA_EDITABLE,FALSE,
				 GSE_COLUMN_AFTER_VISIBLE,    FALSE,
				 GSE_COLUMN_SLOT,             TRUE,
				 GSE_COLUMN_USERDATA_SLOT,    TRUE, -1);
		}
	}

	path_first = gtk_tree_path_new_first ();
	gtk_tree_view_expand_row (GTK_TREE_VIEW (editor->signals_list), path_first, FALSE);
	gtk_tree_path_free (path_first);
}

static void
glade_signal_editor_get_property  (GObject      *object,
				   guint         prop_id,
				   GValue *value,
				   GParamSpec   *pspec)
{
	GladeSignalEditor *self = GLADE_SIGNAL_EDITOR (object);

	switch (prop_id)
	{
	case PROP_HANDLER_COLUMN:
		g_value_set_object (value, self->handler_column);
		break;
	case PROP_USERDATA_COLUMN:
		g_value_set_object (value, self->userdata_column);
		break;
	case PROP_HANDLER_COMPLETION:
		g_value_set_object (value, self->handler_store);
		break;
	case PROP_USERDATA_COMPLETION:
		g_value_set_object (value, self->userdata_store);
		break;
	case PROP_HANDLER_RENDERER:
		g_value_set_object (value, self->handler_renderer);
		break;
	case PROP_USERDATA_RENDERER:
		g_value_set_object (value, self->userdata_renderer);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_signal_editor_set_property  (GObject      *object,
				   guint         prop_id,
				   const GValue *value,
				   GParamSpec   *pspec)
{
	GladeSignalEditor *self = GLADE_SIGNAL_EDITOR (object);

	switch (prop_id)
	{
	case PROP_HANDLER_COLUMN:
		self->handler_column = g_value_get_object (value);
		break;
	case PROP_USERDATA_COLUMN:
		self->userdata_column = g_value_get_object (value);
		break;
	case PROP_HANDLER_COMPLETION:
		self->handler_store = g_value_get_object (value);
		break;
	case PROP_USERDATA_COMPLETION:
		self->userdata_store = g_value_get_object (value);
		break;
	case PROP_HANDLER_RENDERER:
		self->handler_renderer = g_value_get_object (value);
		break;
	case PROP_USERDATA_RENDERER:
		self->userdata_renderer = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_signal_editor_finalize (GObject *object)
{
	GladeSignalEditor *self = GLADE_SIGNAL_EDITOR (object);

	g_object_unref (self->handler_store);
	g_object_unref (self->userdata_store);

	G_OBJECT_CLASS (glade_signal_editor_parent_class)->finalize (object);
}

static void
glade_signal_editor_init (GladeSignalEditor *self)
{
	self->is_void_handler = is_void_handler;
	self->is_void_userdata = is_void_userdata;
}

static void
glade_signal_editor_class_init (GladeSignalEditorClass *klass)
{
	GObjectClass *object_class;

	glade_signal_editor_parent_class = g_type_class_peek_parent (klass);

	object_class = G_OBJECT_CLASS (klass);

	object_class->constructor = glade_signal_editor_constructor;
	object_class->get_property = glade_signal_editor_get_property;
	object_class->set_property = glade_signal_editor_set_property;
	object_class->finalize = glade_signal_editor_finalize;
	klass->handler_editing_done = glade_signal_editor_handler_editing_done_impl;
	klass->userdata_editing_done = glade_signal_editor_userdata_editing_done_impl;
	klass->handler_editing_started = NULL;
	klass->userdata_editing_started = NULL;

	g_object_class_install_property (object_class,
	                                 PROP_HANDLER_COLUMN,
	                                 g_param_spec_object ("handler-column",
	                                                      NULL,
	                                                      NULL,
	                                                      GTK_TYPE_TREE_VIEW_COLUMN,
	                                                      G_PARAM_READABLE |
	                                                      G_PARAM_WRITABLE |
	                                                      G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
	                                 PROP_USERDATA_COLUMN,
	                                 g_param_spec_object ("userdata-column",
	                                                      NULL,
	                                                      NULL,
	                                                      GTK_TYPE_TREE_VIEW_COLUMN,
	                                                      G_PARAM_READABLE |
	                                                      G_PARAM_WRITABLE |
	                                                      G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
	                                 PROP_HANDLER_COMPLETION,
	                                 g_param_spec_object ("handler-completion",
	                                                      NULL,
	                                                      NULL,
	                                                      GTK_TYPE_TREE_MODEL,
	                                                      G_PARAM_READABLE |
	                                                      G_PARAM_WRITABLE |
	                                                      G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
	                                 PROP_USERDATA_COMPLETION,
	                                 g_param_spec_object ("userdata-completion",
	                                                      NULL,
	                                                      NULL,
	                                                      GTK_TYPE_TREE_MODEL,
	                                                      G_PARAM_READABLE |
	                                                      G_PARAM_WRITABLE |
	                                                      G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
	                                 PROP_HANDLER_RENDERER,
	                                 g_param_spec_object ("handler-renderer",
	                                                      NULL,
	                                                      NULL,
	                                                      GTK_TYPE_CELL_RENDERER,
	                                                      G_PARAM_READABLE |
	                                                      G_PARAM_WRITABLE |
	                                                      G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
	                                 PROP_USERDATA_RENDERER,
	                                 g_param_spec_object ("userdata-renderer",
	                                                      NULL,
	                                                      NULL,
	                                                      GTK_TYPE_CELL_RENDERER,
	                                                      G_PARAM_READABLE |
	                                                      G_PARAM_WRITABLE |
	                                                      G_PARAM_CONSTRUCT));	                                                      
	/**
	 * GladeSignalEditor::handler-editing-done:
	 * @signal_editor: the #GladeSignalEditor which received the signal.
	 * @signal_name: the name of the edited signal
	 * @old_handler:
	 * @new_handler:
	 * @iter: the #GtkTreeIter of edited signal
	 *
	 * Emitted when editing of signal handler done.
	 * You can cancel changes by returning FALSE before the default handler.
	 */
	glade_signal_editor_signals[HANDLER_EDITING_DONE] =
		g_signal_new ("handler-editing-done",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeSignalEditorClass, handler_editing_done),
			      glade_boolean_handled_accumulator, NULL,
			      glade_marshal_BOOLEAN__STRING_STRING_STRING_BOXED,
			      G_TYPE_BOOLEAN,
			      4,
			      G_TYPE_STRING, G_TYPE_STRING,
			      G_TYPE_STRING, GTK_TYPE_TREE_ITER);

	/**
	 * GladeSignalEditor::userdata-editing-done:
	 * @signal_editor: the #GladeSignalEditor which received the signal.
	 * @signal_name: the name of the edited signal
	 * @old_handler:
	 * @new_handler:
	 * @iter: the #GtkTreeIter of edited signal
	 * 
	 * Emitted when editing of signal user data done.
	 * You can cancel changes by returning FALSE before the default handler.
	 */
	glade_signal_editor_signals[USERDATA_EDITING_DONE] =
		g_signal_new ("userdata-editing-done",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeSignalEditorClass, userdata_editing_done),
			      glade_boolean_handled_accumulator, NULL,
			      glade_marshal_BOOLEAN__STRING_STRING_STRING_BOXED,
			      G_TYPE_BOOLEAN,
			      4,
			      G_TYPE_STRING, G_TYPE_STRING,
			      G_TYPE_STRING, GTK_TYPE_TREE_ITER);

	/**
	 * GladeSignalEditor::handler-editing-started:
	 * @signal_editor: the #GladeSignalEditor which received the signal.
	 * @signal_name: the name of the edited signal
	 * @iter: the #GtkTreeIter of the edited signal
	 * @editable: the #GtkCellEditable
	 * 
	 * Emitted when editing of signal handler started.
	 * Used mainly for setting up completion.
	 */
	glade_signal_editor_signals[HANDLER_EDITING_STARTED] =
		g_signal_new ("handler-editing-started",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeSignalEditorClass, handler_editing_started),
			      glade_boolean_handled_accumulator, NULL,
			      glade_marshal_BOOLEAN__STRING_BOXED_OBJECT,
			      G_TYPE_BOOLEAN,
			      3,
			      G_TYPE_STRING, GTK_TYPE_TREE_ITER,
			      GTK_TYPE_CELL_EDITABLE);

	/**
	 * GladeSignalEditor::userdata-editing-started:
	 * @signal_editor: the #GladeSignalEditor which received the signal.
	 * @signal_name: the name of the edited signal
	 * @iter: the #GtkTreeIter of the edited signal
	 * @editable: the #GtkCellEditable
	 * 
	 * Emitted when editing of signal user data started.
	 * Used mainly for setting up completion.
	 */
	glade_signal_editor_signals[USERDATA_EDITING_STARTED] =
		g_signal_new ("userdata-editing-started",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeSignalEditorClass, userdata_editing_started),
			      glade_boolean_handled_accumulator, NULL,
			      glade_marshal_BOOLEAN__STRING_BOXED_OBJECT,
			      G_TYPE_BOOLEAN,
			      3,
			      G_TYPE_STRING, GTK_TYPE_TREE_ITER,
			      GTK_TYPE_CELL_EDITABLE);
}

/* Default implementation of completion */

static void
glade_signal_editor_editing_started (GtkEntry *entry, IsVoidFunc callback)
{
	if (callback (gtk_entry_get_text (entry)))
		gtk_entry_set_text (entry, "");
}

static void
glade_signal_editor_handler_store_update (GladeSignalEditor *editor,
					  const gchar *signal_name,
					  GtkListStore *store)
{
	const gchar *handlers[] = {"gtk_widget_show",
				   "gtk_widget_hide",
				   "gtk_widget_grab_focus",
				   "gtk_widget_destroy",
				   "gtk_true",
				   "gtk_false",
				   "gtk_main_quit",
				   NULL};

	GtkTreeIter tmp_iter;
	gint i;
	gchar *handler, *signal, *name;

	name = (gchar *) glade_widget_get_name (editor->widget);

	signal = g_strdup (signal_name);
	glade_util_replace (signal, '-', '_');

	gtk_list_store_clear (store);

	gtk_list_store_append (store, &tmp_iter);
	handler = g_strdup_printf ("on_%s_%s", name, signal);
	gtk_list_store_set (store, &tmp_iter, 0, handler, -1);
	g_free (handler);

	gtk_list_store_append (store, &tmp_iter);
	handler = g_strdup_printf ("%s_%s_cb", name, signal);
	gtk_list_store_set (store, &tmp_iter, 0, handler, -1);
	g_free (handler);

	g_free (signal);
	for (i = 0; handlers[i]; i++)
	{
		gtk_list_store_append (store, &tmp_iter);
		gtk_list_store_set (store, &tmp_iter, 0, handlers[i], -1);
	}
}

gboolean
glade_signal_editor_handler_editing_started_default_impl (GladeSignalEditor *editor,
							  gchar *signal_name,
							  GtkTreeIter *iter,
							  GtkCellEditable *editable,
							  gpointer user_data)
{
	
	GtkEntry *entry;
	GtkEntryCompletion *completion;
	GtkTreeModel *completion_store = editor->handler_store;

	g_return_val_if_fail (GTK_IS_BIN (editable), FALSE);
	g_return_val_if_fail (GTK_IS_LIST_STORE (completion_store), FALSE);

	entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (editable)));

	glade_signal_editor_editing_started (entry, editor->is_void_handler);

	glade_signal_editor_handler_store_update (editor, signal_name,
						  GTK_LIST_STORE (completion_store));

	completion = gtk_entry_completion_new ();
	gtk_entry_completion_set_text_column (completion, 0);
	gtk_entry_completion_set_inline_completion (completion, TRUE);
	gtk_entry_completion_set_popup_completion (completion, FALSE);
	gtk_entry_completion_set_model (completion, completion_store);
	gtk_entry_set_completion (entry, completion);

	return FALSE;
}

static void
glade_signal_editor_userdata_store_update (GladeSignalEditor *self,
					   GtkListStore *store)
{
	GtkTreeIter tmp_iter;
	GList *list;

	gtk_list_store_clear (store);

	for (list = (GList *) glade_project_get_objects (self->widget->project);
	     list && list->data;
	     list = g_list_next (list))
	{
		GladeWidget *widget = glade_widget_get_from_gobject (list->data);

		if (widget)
		{
			gtk_list_store_append (store, &tmp_iter);
			gtk_list_store_set (store, &tmp_iter, 0, widget->name, -1);
		}
	}

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), 0,
					      GTK_SORT_DESCENDING);
}

gboolean
glade_signal_editor_userdata_editing_started_default_impl (GladeSignalEditor *editor,
							   gchar *signal_name,
							   GtkTreeIter *iter,
							   GtkCellEditable *editable,
							   gpointer user_data)
{
	GtkEntry *entry;
	GtkEntryCompletion *completion;
	GtkTreeModel *completion_store = editor->userdata_store;

	g_return_val_if_fail (editor->widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_LIST_STORE (completion_store), FALSE);
	g_return_val_if_fail (GTK_IS_ENTRY (editable), FALSE);

	entry = GTK_ENTRY (editable);

	glade_signal_editor_editing_started (entry, editor->is_void_handler);

	glade_signal_editor_userdata_store_update (editor, GTK_LIST_STORE (completion_store));

	completion = gtk_entry_completion_new ();
	gtk_entry_completion_set_text_column (completion, 0);
	gtk_entry_completion_set_model (completion, completion_store);
	gtk_entry_set_completion (entry, completion);

	return FALSE;
}
