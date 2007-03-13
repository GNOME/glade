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

#include <string.h>
#include <glib/gi18n-lib.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-widget-adaptor.h"
#include "glade-signal.h"
#include "glade-signal-editor.h"
#include "glade-editor.h"
#include "glade-command.h"


enum
{
	COLUMN_SIGNAL,
	COLUMN_HANDLER,
	COLUMN_AFTER,
	COLUMN_USERDATA,
	COLUMN_LOOKUP,

	COLUMN_USERDATA_SLOT,
	COLUMN_LOOKUP_VISIBLE,
	COLUMN_AFTER_VISIBLE,
	COLUMN_HANDLER_EDITABLE,
	COLUMN_USERDATA_EDITABLE,
	COLUMN_SLOT, /* if this row contains a "<Type...>" label */
	COLUMN_BOLD,
	NUM_COLUMNS
};

#define HANDLER_DEFAULT  _("<Type the signal's handler here>")
#define USERDATA_DEFAULT _("<Type the object's name here>")

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
			    COLUMN_SIGNAL,  &signal_name,
			    COLUMN_HANDLER, &handler,
			    COLUMN_USERDATA,&userdata,
			    COLUMN_LOOKUP,  &lookup,
			    COLUMN_AFTER,   &after, -1);
	if (signal_name == NULL)
	{
		if (!gtk_tree_model_iter_parent (model, &iter_parent, &iter))
			g_assert (FALSE);

		gtk_tree_model_get (model, &iter_parent, COLUMN_SIGNAL, &signal_name, -1);
		g_assert (signal_name != NULL);
	}

	old_signal = glade_signal_new (signal_name, handler, userdata, lookup, after);
	new_signal = glade_signal_new (signal_name, handler, userdata, lookup, !after);

	glade_command_change_signal (editor->widget, old_signal, new_signal);
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_AFTER, !after, -1);

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
			    COLUMN_SIGNAL,  &signal_name,
			    COLUMN_HANDLER, &handler,
			    COLUMN_USERDATA,&userdata,
			    COLUMN_LOOKUP,  &lookup,
			    COLUMN_AFTER,   &after, -1);
	if (signal_name == NULL)
	{
		if (!gtk_tree_model_iter_parent (model, &iter_parent, &iter))
			g_assert (FALSE);

		gtk_tree_model_get (model, &iter_parent, COLUMN_SIGNAL, &signal_name, -1);
		g_assert (signal_name != NULL);
	}

	old_signal = glade_signal_new (signal_name, handler, userdata, lookup, after);
	new_signal = glade_signal_new (signal_name, handler, userdata, !lookup, after);

	glade_command_change_signal (editor->widget, old_signal, new_signal);
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter, COLUMN_LOOKUP, !lookup, -1);

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
			    COLUMN_HANDLER,          _(HANDLER_DEFAULT),
			    COLUMN_USERDATA,         _(USERDATA_DEFAULT),
			    COLUMN_LOOKUP,           FALSE,
			    COLUMN_LOOKUP_VISIBLE,   FALSE,
			    COLUMN_HANDLER_EDITABLE, TRUE,
			    COLUMN_USERDATA_EDITABLE,FALSE,
			    COLUMN_AFTER,            FALSE,
			    COLUMN_AFTER_VISIBLE,    FALSE,
			    COLUMN_SLOT,             TRUE,
			    COLUMN_USERDATA_SLOT,    TRUE,
			    -1);
	gtk_tree_model_iter_parent (model, &iter_class, iter_signal);

	/* mark the signal & class name as bold */
	gtk_tree_store_set (GTK_TREE_STORE (model), iter_signal, COLUMN_BOLD, TRUE, -1);
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter_class, COLUMN_BOLD, TRUE, -1);
}

static void
remove_slot (GtkTreeModel *model, GtkTreeIter *iter)
{
	GtkTreeIter iter_next;
	GtkTreeIter iter_class;
	GtkTreeIter iter_signal;
	gboolean removing_top_signal_handler = TRUE;
	char *signal_name;

	gtk_tree_model_get (model, iter, COLUMN_SIGNAL, &signal_name, -1);
	if (signal_name == NULL)
	{
		gtk_tree_model_iter_parent (model, &iter_signal, iter);
		removing_top_signal_handler = FALSE;
	}
	else
		iter_signal = *iter;

	g_free (signal_name);

	gtk_tree_model_iter_parent (model, &iter_class, &iter_signal);

	/* special case for removing the handler of the first row */
	if (removing_top_signal_handler)
		gtk_tree_model_iter_nth_child (model, &iter_next, iter, 0);
	else
	{
		iter_next = *iter;
		gtk_tree_model_iter_next (model, &iter_next);
	}

	do
	{
		gchar *handler;
		gboolean after;
		gboolean slot;
		gboolean visible;

		gtk_tree_model_get (model,                &iter_next,
				    COLUMN_HANDLER,       &handler,
				    COLUMN_AFTER,         &after,
				    COLUMN_SLOT,          &slot,
				    COLUMN_AFTER_VISIBLE, &visible, -1);

		gtk_tree_store_set (GTK_TREE_STORE (model), iter,
				    COLUMN_HANDLER,         handler,
				    COLUMN_AFTER,           after,
				    COLUMN_SLOT,            slot,
				    COLUMN_AFTER_VISIBLE,   visible, -1);
		g_free (handler);

		*iter = iter_next;
	}
	while (gtk_tree_model_iter_next (model, &iter_next));

	gtk_tree_store_remove (GTK_TREE_STORE (model), iter);

	if (!gtk_tree_model_iter_has_child (model, &iter_signal))
	{
		/* mark the signal & class name as normal */
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter_signal,
				    COLUMN_BOLD, FALSE, -1);
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter_class,
				    COLUMN_BOLD, FALSE, -1);
	}
}

static gboolean
is_void_signal_handler (const gchar *signal_handler)
{
	return ( signal_handler == NULL ||
		*signal_handler == 0    ||
		 g_utf8_collate (signal_handler, _(HANDLER_DEFAULT)) == 0);
}

static gboolean
is_void_user_data (const gchar *user_data)
{
	return ( user_data == NULL ||
		*user_data == 0    ||
		 g_utf8_collate (user_data, _(USERDATA_DEFAULT)) == 0);
}

static void
glade_signal_editor_handler_cell_edited (GtkCellRendererText *cell,
					 const gchar         *path_str,
					 const gchar         *new_handler,
					 gpointer             data)
{
	GladeWidget *glade_widget = ((GladeSignalEditor*) data)->widget;
	GtkTreeModel *model = GTK_TREE_MODEL (((GladeSignalEditor*) data)->model);
	GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
	GtkTreeIter iter;
	GtkTreeIter iter_signal;
	gchar    *signal_name;
	gchar    *old_handler;
	gchar    *userdata;
	gboolean  lookup;
	gboolean  after;
	gboolean  slot;

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model,           &iter,
			    COLUMN_SIGNAL,   &signal_name,
			    COLUMN_HANDLER,  &old_handler,
			    COLUMN_USERDATA, &userdata,
			    COLUMN_LOOKUP,   &lookup,
			    COLUMN_AFTER,    &after,
			    COLUMN_SLOT,     &slot, -1);

	if (signal_name == NULL)
	{
		if (!gtk_tree_model_iter_parent (model, &iter_signal, &iter))
			g_assert (FALSE);

		gtk_tree_model_get (model, &iter_signal, COLUMN_SIGNAL, &signal_name, -1);
		g_assert (signal_name != NULL);
	}
	else
		iter_signal = iter;

	/* false alarm */
	if (slot && is_void_signal_handler(new_handler))
		return;

	/* we're adding a new handler */
	if (slot && !is_void_signal_handler(new_handler))
	{
		GladeSignal *new_signal = glade_signal_new (signal_name, new_handler,
							    NULL, FALSE, FALSE);
		glade_command_add_signal (glade_widget, new_signal);
		glade_signal_free (new_signal);
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
				    COLUMN_HANDLER,          new_handler,
				    COLUMN_AFTER_VISIBLE,    TRUE,
				    COLUMN_SLOT,             FALSE,
				    COLUMN_USERDATA_EDITABLE,TRUE, -1);

		/* append a <Type...> slot */
		append_slot (model, &iter_signal);
	}

	/* we're removing a signal handler */
	if (!slot && is_void_signal_handler(new_handler))
	{
		GladeSignal *old_signal = 
			glade_signal_new (signal_name, 
					  old_handler,
					  is_void_user_data (userdata) ? NULL : userdata, 
					  lookup, after);
		glade_command_remove_signal (glade_widget, old_signal);
		glade_signal_free (old_signal);

		gtk_tree_store_set
			(GTK_TREE_STORE (model),          &iter,
				 COLUMN_HANDLER,          _(HANDLER_DEFAULT),
				 COLUMN_AFTER,            FALSE,
				 COLUMN_USERDATA,         _(USERDATA_DEFAULT),
				 COLUMN_LOOKUP,           FALSE,
				 COLUMN_LOOKUP_VISIBLE,   FALSE,
				 COLUMN_HANDLER_EDITABLE, TRUE,
				 COLUMN_USERDATA_EDITABLE,FALSE,
				 COLUMN_AFTER_VISIBLE,    FALSE,
				 COLUMN_SLOT,             TRUE,
				 COLUMN_USERDATA_SLOT,    TRUE,
				 -1);

		remove_slot (model, &iter);
	}

	/* we're changing a signal handler */
	if (!slot && !is_void_signal_handler(new_handler))
	{
		GladeSignal *old_signal =
			glade_signal_new
			(signal_name,
			 old_handler,
			 is_void_user_data(userdata) ? NULL : userdata,
			 lookup,
			 after);
		GladeSignal *new_signal =
			glade_signal_new
			(signal_name,
			 new_handler,
			 is_void_user_data(userdata) ? NULL : userdata,
			 lookup,
			 after);

		glade_command_change_signal (glade_widget, old_signal, new_signal);

		glade_signal_free (old_signal);
		glade_signal_free (new_signal);

		gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
				    COLUMN_HANDLER,       new_handler,
				    COLUMN_AFTER_VISIBLE, TRUE,
				    COLUMN_SLOT,          FALSE,
				    COLUMN_USERDATA_EDITABLE,TRUE, -1);
	}

	gtk_tree_path_free (path);
	g_free (signal_name);
	g_free (old_handler);
	g_free (userdata);
}

static void
glade_signal_editor_construct_handler_store (GladeSignalEditor *editor)
{
	const gchar *handlers[] = {"gtk_widget_show", 
				   "gtk_widget_hide",
				   "gtk_widget_grab_focus",
				   "gtk_widget_destroy",
				   "gtk_true",
				   "gtk_false",
				   "gtk_main_quit",
				   NULL};
	GtkTreeIter iter, *iters = editor->iters;
	GtkEntryCompletion *completion;
	GtkListStore *store;
	gint i;
				   
	store = gtk_list_store_new (1, G_TYPE_STRING);
		
	gtk_list_store_append (store, &iters[0]);
	gtk_list_store_append (store, &iters[1]);
		
	for (i = 0; handlers[i]; i++)
	{
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, handlers[i], -1);
	}
	
	completion = gtk_entry_completion_new ();
	gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (store));
	gtk_entry_completion_set_text_column (completion, 0);
	gtk_entry_completion_set_inline_completion (completion, TRUE);
	gtk_entry_completion_set_popup_completion (completion, FALSE);
	
	editor->handler_store = store;
	editor->completion = completion;
}

static void
glade_signal_editor_handler_store_update (GladeSignalEditor *editor,
					  const gchar *path)
{
	GtkTreeModel *model = GTK_TREE_MODEL (editor->model);
	GtkListStore *store = editor->handler_store;
	GtkTreeIter iter, *iters = editor->iters;
	gchar *handler, *signal, *name;
	
	name = (gchar *) glade_widget_get_name (editor->widget);
		
	if (gtk_tree_model_get_iter_from_string (model, &iter, path))
	{
		gtk_tree_model_get (model, &iter, COLUMN_SIGNAL, &signal, -1);
		if (signal) glade_util_replace (signal, '-', '_');
		else return;
	}
	else return;

	handler = g_strdup_printf ("on_%s_%s", name, signal);
	gtk_list_store_set (store, &iters[0], 0, handler, -1);
	g_free (handler);

	handler = g_strdup_printf ("%s_%s_cb", name, signal);
	gtk_list_store_set (store, &iters[1], 0, handler, -1);
	g_free (handler);
	
	g_free (signal);
}

static void
glade_signal_editor_editing_started (GtkEntry *entry, gboolean handler)
{
	const gchar *text = gtk_entry_get_text (entry);
	
	if ((handler) ? is_void_signal_handler (text) : is_void_user_data (text))
		gtk_entry_set_text (entry, "");
}

static void
glade_signal_editor_handler_editing_started (GtkCellRenderer *cell,
					     GtkCellEditable *editable,
					     const gchar *path,
					     GladeSignalEditor *editor)
{
	GtkEntry *entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (editable)));
	
	glade_signal_editor_editing_started (entry, TRUE);
	
	glade_signal_editor_handler_store_update (editor, path);
	
	gtk_entry_set_completion (entry, editor->completion);
}

static void
glade_signal_editor_user_data_editing_started (GtkCellRenderer *cell,
					       GtkCellEditable *editable,
					       const gchar *path,
					       GladeSignalEditor *editor)
{
	GtkEntry *entry = GTK_ENTRY (editable);
	GtkEntryCompletion *completion;
	GtkListStore *store;
	GtkTreeIter iter;
	GList *list;
	
	g_return_if_fail (editor->widget != NULL);
	
	glade_signal_editor_editing_started (entry, FALSE);
	
	store = gtk_list_store_new (1, G_TYPE_STRING);
	
	for (list = editor->widget->project->objects;
	     list && list->data;
	     list = g_list_next (list))
	{
		GladeWidget *widget = glade_widget_get_from_gobject (list->data);
		
		if (widget)
		{
			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter, 0, widget->name, -1);
		}
	}
	
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), 0,
					      GTK_SORT_DESCENDING);
	
	completion = gtk_entry_completion_new ();
	
	gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (store));
	gtk_entry_completion_set_text_column (completion, 0);	
	gtk_entry_set_completion (entry, completion);
	
	/* Get rid of references */
	g_object_unref (store);
	g_object_unref (completion);
}

static void
glade_signal_editor_userdata_cell_edited (GtkCellRendererText *cell,
					  const gchar         *path_str,
					  const gchar         *new_userdata,
					  gpointer             data)
{
	GladeWidget  *glade_widget = ((GladeSignalEditor*) data)->widget;
	GtkTreeModel *model = GTK_TREE_MODEL (((GladeSignalEditor*) data)->model);
	GtkTreePath  *path = gtk_tree_path_new_from_string (path_str);
	GtkTreeIter iter;
	GtkTreeIter iter_signal;
	GladeSignal *old_signal, *new_signal;
	gchar *signal_name;
	gchar *old_userdata;
	gchar *handler;
	gboolean after;
	gboolean lookup;
	
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model,           &iter,
			    COLUMN_SIGNAL,   &signal_name,
			    COLUMN_HANDLER,  &handler,
			    COLUMN_USERDATA, &old_userdata,
			    COLUMN_LOOKUP,   &lookup,
			    COLUMN_AFTER,    &after, -1);

	if (signal_name == NULL)
	{
		if (!gtk_tree_model_iter_parent (model, &iter_signal, &iter))
			g_assert (FALSE);

		gtk_tree_model_get (model, &iter_signal, COLUMN_SIGNAL, &signal_name, -1);
		g_assert (signal_name != NULL);
	}
	else
		iter_signal = iter;

	/* We are removing userdata */
	if (is_void_user_data(new_userdata))
	{
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
				    COLUMN_USERDATA_SLOT,  TRUE,
				    COLUMN_USERDATA,       _(USERDATA_DEFAULT),
				    COLUMN_LOOKUP,         FALSE,
				    COLUMN_LOOKUP_VISIBLE, FALSE, -1);
	}
	else
	{
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			    COLUMN_USERDATA_SLOT,  FALSE,
			    COLUMN_USERDATA,       new_userdata,
			    COLUMN_LOOKUP_VISIBLE, TRUE,
			    -1);
	}

	old_signal =
		glade_signal_new
		(signal_name, handler,
		 is_void_user_data(old_userdata) ? NULL : old_userdata,
		 lookup, after);

	new_signal = 
		glade_signal_new 
		(signal_name, handler,
		 is_void_user_data(new_userdata) ? NULL : new_userdata,
		 lookup, after);

	if (glade_signal_equal (old_signal, new_signal) == FALSE)
		glade_command_change_signal (glade_widget, old_signal, new_signal);
	
	glade_signal_free (old_signal);
	glade_signal_free (new_signal);

	gtk_tree_path_free (path);
	g_free (signal_name);
	g_free (handler);
	g_free (old_userdata);
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

	editor->model = gtk_tree_store_new
		(NUM_COLUMNS,
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

	g_signal_connect(view, "row-activated", (GCallback) row_activated, NULL);

	/* Contruct handler model */
	glade_signal_editor_construct_handler_store (editor);

	/************************ signal column ************************/
 	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), "weight", PANGO_WEIGHT_BOLD, NULL); 
	column = gtk_tree_view_column_new_with_attributes
		(_("Signal"), renderer, "text", COLUMN_SIGNAL, "weight-set", COLUMN_BOLD, NULL);
 	gtk_tree_view_append_column (view, column);

	/************************ handler column ************************/
 	renderer = gtk_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer),
		      "style", PANGO_STYLE_ITALIC,
		      "foreground", "Gray",
		      "model", GTK_TREE_MODEL (editor->handler_store),
		      "text-column", 0,
		      NULL);
	
	g_signal_connect (renderer, "edited",
			  G_CALLBACK (glade_signal_editor_handler_cell_edited), editor);

	g_signal_connect (renderer, "editing-started",
			  G_CALLBACK (glade_signal_editor_handler_editing_started),
			  editor);

	column = gtk_tree_view_column_new_with_attributes
		(_("Handler"),     renderer,
		 "text",           COLUMN_HANDLER,
		 "style_set",      COLUMN_SLOT,
		 "foreground_set", COLUMN_SLOT,
		 "editable",       COLUMN_HANDLER_EDITABLE, NULL);

 	gtk_tree_view_append_column (view, column);

	/************************ userdata column ************************/
 	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer),
		      "style", PANGO_STYLE_ITALIC,
		      "foreground", "Gray", NULL);

	g_signal_connect (renderer, "edited",
			  G_CALLBACK (glade_signal_editor_userdata_cell_edited), editor);
	
	g_signal_connect (renderer, "editing-started",
			  G_CALLBACK (glade_signal_editor_user_data_editing_started),
			  editor);
	
	column = gtk_tree_view_column_new_with_attributes
		(_("User data"), renderer,
		 "text",           COLUMN_USERDATA,
		 "style_set",      COLUMN_USERDATA_SLOT,
		 "foreground_set", COLUMN_USERDATA_SLOT,
		 "editable",       COLUMN_USERDATA_EDITABLE, NULL);

 	gtk_tree_view_append_column (view, column);
	
#if LOOKUP_COLUMN
	/************************ lookup column ************************/
 	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled",
			  G_CALLBACK (glade_signal_editor_lookup_toggled), editor);
	column = gtk_tree_view_column_new_with_attributes
		(_("Lookup"), renderer,
		 "active",  COLUMN_LOOKUP,
		 "visible", COLUMN_LOOKUP_VISIBLE, NULL);

 	gtk_tree_view_append_column (view, column);
#endif
	/************************ after column ************************/
	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled",
			  G_CALLBACK (glade_signal_editor_after_toggled), editor);
 	column = gtk_tree_view_column_new_with_attributes
		(_("After"), renderer,
		 "active",  COLUMN_AFTER,
		 "visible", COLUMN_AFTER_VISIBLE, NULL);
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

	signal_editor = g_new0 (GladeSignalEditor, 1);

	glade_signal_editor_construct (signal_editor);
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
					       COLUMN_SIGNAL,           signal->type,
					       COLUMN_AFTER_VISIBLE,    FALSE,
					       COLUMN_HANDLER_EDITABLE, FALSE,
					       COLUMN_USERDATA_EDITABLE,FALSE,
					       COLUMN_SLOT,             FALSE,
					       COLUMN_BOLD,             FALSE, -1);
			last_type = signal->type;
		}

		gtk_tree_store_append (editor->model, &parent_signal, &parent_class);
		signals = glade_widget_list_signal_handlers (widget, signal->name);

		if (!signals || signals->len == 0)
		{
			gtk_tree_store_set
				(editor->model,          &parent_signal,
				 COLUMN_SIGNAL,           signal->name,
				 COLUMN_HANDLER,          _(HANDLER_DEFAULT),
				 COLUMN_AFTER,            FALSE,
				 COLUMN_USERDATA,         _(USERDATA_DEFAULT),
				 COLUMN_LOOKUP,           FALSE,
				 COLUMN_LOOKUP_VISIBLE,   FALSE,
				 COLUMN_HANDLER_EDITABLE, TRUE,
				 COLUMN_USERDATA_EDITABLE,FALSE,
				 COLUMN_AFTER_VISIBLE,    FALSE,
				 COLUMN_SLOT,             TRUE,
				 COLUMN_USERDATA_SLOT,    TRUE,
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
			gtk_tree_store_set (editor->model, &parent_class, COLUMN_BOLD, TRUE, -1);
			path_parent_class =
				gtk_tree_model_get_path (GTK_TREE_MODEL (editor->model),
							 &parent_class);
			gtk_tree_view_expand_row (GTK_TREE_VIEW (editor->signals_list),
						  path_parent_class, FALSE);
			gtk_tree_path_free (path_parent_class);

			gtk_tree_store_set
				(editor->model,            &parent_signal,
				 COLUMN_SIGNAL,             signal->name,
				 COLUMN_HANDLER,            widget_signal->handler,
				 COLUMN_AFTER,              widget_signal->after,
				 COLUMN_USERDATA,
				 widget_signal->userdata ?
				 widget_signal->userdata : _(USERDATA_DEFAULT),
				 COLUMN_LOOKUP,             widget_signal->lookup,
				 COLUMN_LOOKUP_VISIBLE,
				 widget_signal->userdata ?  TRUE : FALSE,
				 COLUMN_AFTER_VISIBLE,      TRUE,
				 COLUMN_HANDLER_EDITABLE,   TRUE,
				 COLUMN_USERDATA_EDITABLE,  TRUE,
				 COLUMN_SLOT,               FALSE,
				 COLUMN_USERDATA_SLOT,
				 widget_signal->userdata  ? FALSE : TRUE,
				 COLUMN_BOLD,               TRUE, -1);

			for (i = 1; i < signals->len; i++)
			{
				widget_signal = (GladeSignal*) g_ptr_array_index (signals, i);
				gtk_tree_store_append (editor->model, &iter, &parent_signal);

				gtk_tree_store_set
					(editor->model,            &iter,
					 COLUMN_HANDLER,            widget_signal->handler,
					 COLUMN_AFTER,              widget_signal->after,
					 COLUMN_USERDATA,
					 widget_signal->userdata  ?
					 widget_signal->userdata : _(USERDATA_DEFAULT),
					 COLUMN_LOOKUP,           widget_signal->lookup,
					 COLUMN_LOOKUP_VISIBLE,
					 widget_signal->userdata  ? TRUE : FALSE,
					 COLUMN_AFTER_VISIBLE,      TRUE,
					 COLUMN_HANDLER_EDITABLE,   TRUE,
					 COLUMN_USERDATA_EDITABLE,  TRUE,
					 COLUMN_SLOT,               FALSE,
					 COLUMN_USERDATA_SLOT,
					 widget_signal->userdata  ? FALSE : TRUE,
					 -1);
			}

			/* add the <Type...> slot */
			gtk_tree_store_append (editor->model, &iter, &parent_signal);
			gtk_tree_store_set
				(editor->model,          &iter,
				 COLUMN_HANDLER,          _(HANDLER_DEFAULT),
				 COLUMN_AFTER,            FALSE,
				 COLUMN_USERDATA,         _(USERDATA_DEFAULT),
				 COLUMN_LOOKUP,           FALSE,
				 COLUMN_LOOKUP_VISIBLE,   FALSE,
				 COLUMN_HANDLER_EDITABLE, TRUE,
				 COLUMN_USERDATA_EDITABLE,FALSE,
				 COLUMN_AFTER_VISIBLE,    FALSE,
				 COLUMN_SLOT,             TRUE,
				 COLUMN_USERDATA_SLOT,    TRUE, -1);
		}
	}

	path_first = gtk_tree_path_new_first ();
	gtk_tree_view_expand_row (GTK_TREE_VIEW (editor->signals_list), path_first, FALSE);
	gtk_tree_path_free (path_first);
}
