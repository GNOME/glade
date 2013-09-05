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
 *W
 */

#include <string.h>
#include <glib/gi18n-lib.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-widget-adaptor.h"
#include "glade-signal.h"
#include "glade-signal-editor.h"
#include "glade-cell-renderer-icon.h"
#include "glade-editor.h"
#include "glade-command.h"
#include "glade-marshallers.h"
#include "glade-accumulators.h"

typedef gboolean (*IsVoidFunc) (const gchar *signal_handler);

enum
{
	GSE_COLUMN_SIGNAL,
	GSE_COLUMN_HANDLER,
	GSE_COLUMN_AFTER,
	GSE_COLUMN_USERDATA,
	GSE_COLUMN_SWAPPED,

	GSE_COLUMN_USERDATA_SLOT,
	GSE_COLUMN_SWAPPED_VISIBLE,
	GSE_COLUMN_AFTER_VISIBLE,
	GSE_COLUMN_HANDLER_EDITABLE,
	GSE_COLUMN_USERDATA_EDITABLE,
	GSE_COLUMN_SLOT, /* if this row contains a "<Type...>" label */
	GSE_COLUMN_BOLD,
	GSE_COLUMN_CONTENT,

	GSE_COLUMN_WARN,
	GSE_COLUMN_TOOLTIP,
	GSE_NUM_COLUMNS
};

enum
{
	HANDLER_EDITING_STARTED,
	USERDATA_EDITING_STARTED,
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

G_DEFINE_TYPE (GladeSignalEditor, glade_signal_editor, GTK_TYPE_VBOX)

struct _GladeSignalEditorPrivate
{
	GladeWidget *widget;
	GladeWidgetAdaptor *adaptor;

	gpointer  *editor;

	GtkWidget *signals_list;
	GtkTreeStore *model;

	GtkTreeModel *handler_store;
	GtkTreeModel *userdata_store;
	GtkCellRenderer *handler_renderer;
	GtkCellRenderer *userdata_renderer;

	GtkTreeViewColumn *handler_column;
	GtkTreeViewColumn *userdata_column;
	GtkTreeViewColumn *swapped_column_ptr;
	IsVoidFunc is_void_handler;
	IsVoidFunc is_void_userdata;
	
	gulong refresh_id;
};

#define HANDLER_DEFAULT  _("<Type here>")
#define USERDATA_DEFAULT _("<Object>")

static gboolean
glade_signal_editor_handler_editing_started_default (GladeSignalEditor *editor,
                                                     gchar *signal_name,
                                                     GtkTreeIter *iter,
                                                     GtkCellEditable *editable);
static gboolean
glade_signal_editor_userdata_editing_started_default (GladeSignalEditor *editor,
                                                      gchar *signal_name,
                                                      GtkTreeIter *iter,
                                                      GtkCellEditable *editable);

static gboolean
is_void_handler (const gchar *signal_handler)
{
	return ( signal_handler == NULL ||
		*signal_handler == 0    ||
		 g_utf8_collate (signal_handler, HANDLER_DEFAULT) == 0);
}

static gboolean
is_void_userdata (const gchar *user_data)
{
	return ( user_data == NULL ||
		*user_data == 0    ||
		 g_utf8_collate (user_data, USERDATA_DEFAULT) == 0);
}

static void
glade_signal_editor_after_swapped_toggled (GtkCellRendererToggle *cell,
					   gchar                 *path_str,
					   gpointer               data)
{
	GladeSignalEditor *editor = (GladeSignalEditor*) data;
	GladeSignalEditorPrivate* priv = editor->priv;
	GtkTreeModel *model = GTK_TREE_MODEL (priv->model);
	GtkTreeIter  iter;
	GtkTreeIter iter_parent;
	GladeSignal *old_signal;
	GladeSignal *new_signal;
	GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
	gchar     *signal_name;
	gchar     *handler;
	gchar     *userdata;
	gboolean   swapped, new_swapped;
	gboolean   after, new_after;

	/* get toggled iter */
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    GSE_COLUMN_SIGNAL,  &signal_name,
			    GSE_COLUMN_HANDLER, &handler,
			    GSE_COLUMN_USERDATA,&userdata,
			    GSE_COLUMN_SWAPPED, &swapped,
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

	new_after = after;
	new_swapped = swapped;
	if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "signal-after-cell")))
		new_after = !after;
	else
		new_swapped = !swapped;

	old_signal = glade_signal_new (signal_name, handler, userdata, after, swapped);
	new_signal = glade_signal_new (signal_name, handler, userdata, new_after, new_swapped);

	glade_command_change_signal (priv->widget, old_signal, new_signal);

	gtk_tree_store_set (GTK_TREE_STORE (model), &iter, 
			    GSE_COLUMN_AFTER, new_after, 
			    GSE_COLUMN_SWAPPED, new_swapped, 
			    -1);

	glade_signal_free (old_signal);
	glade_signal_free (new_signal);
	gtk_tree_path_free (path);
	g_free (signal_name);
	g_free (handler);
	g_free (userdata);
}

static void
append_slot (GladeSignalEditor *self, GtkTreeIter *iter_signal, const gchar *signal_name)
{
	GtkTreeIter iter_new_slot;
	GtkTreeIter iter_class;
	GladeSignalEditorPrivate* priv = self->priv;
	GtkTreeModel *model = GTK_TREE_MODEL (priv->model);
	GladeSignal *sig = glade_signal_new (signal_name, NULL, NULL, FALSE, FALSE);

	/* Check versioning warning here with a virtual signal */
	glade_project_update_signal_support_warning (priv->widget, sig);

	gtk_tree_store_append (GTK_TREE_STORE (model), &iter_new_slot, iter_signal);
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter_new_slot,
			    GSE_COLUMN_HANDLER,          HANDLER_DEFAULT,
			    GSE_COLUMN_USERDATA,         USERDATA_DEFAULT,
			    GSE_COLUMN_SWAPPED,          FALSE,
			    GSE_COLUMN_SWAPPED_VISIBLE,  FALSE,
			    GSE_COLUMN_HANDLER_EDITABLE, TRUE,
			    GSE_COLUMN_USERDATA_EDITABLE,FALSE,
			    GSE_COLUMN_AFTER,            FALSE,
			    GSE_COLUMN_AFTER_VISIBLE,    FALSE,
			    GSE_COLUMN_SLOT,             TRUE,
			    GSE_COLUMN_USERDATA_SLOT,    TRUE,
			    GSE_COLUMN_CONTENT,          TRUE,
			    GSE_COLUMN_WARN,             FALSE,
			    GSE_COLUMN_TOOLTIP,          sig->support_warning,
			    -1);
	gtk_tree_model_iter_parent (model, &iter_class, iter_signal);

	/* mark the signal & class name as bold */
	gtk_tree_store_set (GTK_TREE_STORE (model), iter_signal, GSE_COLUMN_BOLD, TRUE, -1);
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter_class, GSE_COLUMN_BOLD, TRUE, -1);

	glade_signal_free (sig);
}

static void
move_row (GtkTreeModel *model, GtkTreeIter *from, GtkTreeIter *to)
{
	gchar *handler;
	gchar *userdata, *support_warning;
	gboolean after;
	gboolean slot;
	gboolean visible;
	gboolean userdata_slot;
	gboolean handler_editable;
	gboolean userdata_editable;
	gboolean swapped;
	gboolean swapped_visible;
	gboolean bold, content, warn;

	gtk_tree_model_get (model,                        from,
			    GSE_COLUMN_HANDLER,           &handler,
			    GSE_COLUMN_USERDATA,          &userdata,
			    GSE_COLUMN_AFTER,             &after,
			    GSE_COLUMN_SLOT,              &slot,
			    GSE_COLUMN_AFTER_VISIBLE,     &visible,
			    GSE_COLUMN_HANDLER_EDITABLE,  &handler_editable,
			    GSE_COLUMN_USERDATA_EDITABLE, &userdata_editable,
			    GSE_COLUMN_USERDATA_SLOT,     &userdata_slot,
			    GSE_COLUMN_SWAPPED,           &swapped,
			    GSE_COLUMN_SWAPPED_VISIBLE,   &swapped_visible,
			    GSE_COLUMN_BOLD,              &bold,
			    GSE_COLUMN_CONTENT,           &content,
			    GSE_COLUMN_WARN,              &warn,
			    GSE_COLUMN_TOOLTIP,           &support_warning,
			    -1);

	gtk_tree_store_set (GTK_TREE_STORE (model),       to,
			    GSE_COLUMN_HANDLER,           handler,
			    GSE_COLUMN_USERDATA,          userdata,
			    GSE_COLUMN_AFTER,             after,
			    GSE_COLUMN_SLOT,              slot,
			    GSE_COLUMN_AFTER_VISIBLE,     visible,
			    GSE_COLUMN_HANDLER_EDITABLE,  handler_editable,
			    GSE_COLUMN_USERDATA_EDITABLE, userdata_editable,
			    GSE_COLUMN_USERDATA_SLOT,     userdata_slot,
			    GSE_COLUMN_SWAPPED,           swapped,
			    GSE_COLUMN_SWAPPED_VISIBLE,   swapped_visible,
			    GSE_COLUMN_BOLD,              bold,
			    GSE_COLUMN_CONTENT,           content,
			    GSE_COLUMN_WARN,              warn,
			    GSE_COLUMN_TOOLTIP,           support_warning,
			    -1);

	g_free (support_warning);
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
glade_signal_editor_handler_editing_done  (GladeSignalEditor *self,
                                           const gchar *signal_name,
                                           const gchar *old_handler,
                                           const gchar *new_handler,
                                           GtkTreeIter *iter)
{
	GladeSignalEditorPrivate *priv = self->priv;
	GladeWidget *glade_widget = priv->widget;
	GtkTreeModel *model = GTK_TREE_MODEL (priv->model);
	gchar *tmp_signal_name;
	gchar *userdata;
	GtkTreeIter iter_signal;
	gboolean  after, swapped;
	gboolean is_top_handler;

	gtk_tree_model_get (model,               iter,
			    GSE_COLUMN_SIGNAL,   &tmp_signal_name,
			    GSE_COLUMN_USERDATA, &userdata,
			    GSE_COLUMN_AFTER,    &after,
			    GSE_COLUMN_SWAPPED,  &swapped,
			    -1);

	if (priv->is_void_userdata (userdata))
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
							    NULL, FALSE, FALSE);
		glade_command_add_signal (glade_widget, new_signal);
		glade_signal_free (new_signal);
		gtk_tree_store_set (GTK_TREE_STORE (model), iter,
				    GSE_COLUMN_HANDLER,          new_handler,
				    GSE_COLUMN_AFTER_VISIBLE,    TRUE,
				    GSE_COLUMN_SLOT,             FALSE,
				    GSE_COLUMN_USERDATA_EDITABLE,TRUE, -1);

		/* append a <Type...> slot */
		append_slot (self, &iter_signal, signal_name);
	}

	/* we're removing a signal handler */
	else if (old_handler && new_handler == NULL)
	{
		GladeSignal *old_signal =
			glade_signal_new (signal_name,
					  old_handler,
					  userdata, 
					  after, 
					  swapped);
		glade_command_remove_signal (glade_widget, old_signal);
		glade_signal_free (old_signal);

		gtk_tree_store_set
			(GTK_TREE_STORE (model),          iter,
				 GSE_COLUMN_HANDLER,          HANDLER_DEFAULT,
				 GSE_COLUMN_AFTER,            FALSE,
				 GSE_COLUMN_USERDATA,         USERDATA_DEFAULT,
				 GSE_COLUMN_SWAPPED,          FALSE,
				 GSE_COLUMN_SWAPPED_VISIBLE,  FALSE,
				 GSE_COLUMN_HANDLER_EDITABLE, TRUE,
				 GSE_COLUMN_USERDATA_EDITABLE,FALSE,
				 GSE_COLUMN_AFTER_VISIBLE,    FALSE,
				 GSE_COLUMN_SLOT,             TRUE,
				 GSE_COLUMN_USERDATA_SLOT,    TRUE,
				 GSE_COLUMN_CONTENT,          TRUE,
				 -1);

		remove_slot (model, is_top_handler ? NULL : iter, &iter_signal);
	}

	/* we're changing a signal handler */
	else if (old_handler && new_handler)
	{
		GladeSignal *old_signal =
			glade_signal_new
			(signal_name,
			 old_handler,
			 userdata,
			 after,
			 swapped);
		GladeSignal *new_signal =
			glade_signal_new
			(signal_name,
			 new_handler,
			 userdata,
			 after, 
			 swapped);

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
glade_signal_editor_userdata_editing_done (GladeSignalEditor *self,
                                           const gchar *signal_name,
                                           const gchar *old_userdata,
                                           const gchar *new_userdata,
                                           GtkTreeIter *iter)
{
	GladeSignalEditorPrivate *priv = self->priv;
	GtkTreeModel *model = GTK_TREE_MODEL (priv->model);
	GladeWidget  *glade_widget = priv->widget;
	gchar *handler;
	gboolean after, swapped;
	GladeSignal *old_signal, *new_signal;

	gtk_tree_model_get (model,               iter,
			    GSE_COLUMN_HANDLER,  &handler,
			    GSE_COLUMN_AFTER,    &after, 
			    GSE_COLUMN_SWAPPED,  &swapped, 
			    -1);

	/* We are removing userdata */
	if (new_userdata == NULL)
	{
		gtk_tree_store_set (GTK_TREE_STORE (model), iter,
				    GSE_COLUMN_USERDATA_SLOT,   TRUE,
				    GSE_COLUMN_USERDATA,        USERDATA_DEFAULT,
				    GSE_COLUMN_SWAPPED,         FALSE,
				    GSE_COLUMN_SWAPPED_VISIBLE, FALSE, -1);
	}
	else
	{
		gtk_tree_store_set (GTK_TREE_STORE (model), iter,
				    GSE_COLUMN_USERDATA_SLOT,   FALSE,
				    GSE_COLUMN_USERDATA,        new_userdata,
				    GSE_COLUMN_SWAPPED_VISIBLE, TRUE,
				    -1);
	}

	old_signal = glade_signal_new (signal_name, handler, old_userdata, after, swapped);

	new_signal = glade_signal_new (signal_name, handler, new_userdata, after, swapped);

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
					IsVoidFunc   is_void_callback)
{
	GladeSignalEditor* self = GLADE_SIGNAL_EDITOR (data);
	GladeSignalEditorPrivate *priv = self->priv;
	GtkTreeModel *model = GTK_TREE_MODEL (priv->model);
	GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
	GtkTreeIter iter;
	gchar    *signal_name;
	gchar    *old_value;

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
	if (old_value || new_value);
	{
		switch (column)
		{
			case GSE_COLUMN_HANDLER:
				glade_signal_editor_handler_editing_done (self,
				                                          signal_name,
				                                          old_value,
				                                          new_value,
				                                          &iter);
				break;
			case GSE_COLUMN_USERDATA:
				glade_signal_editor_userdata_editing_done (self,
				                                           signal_name,
				                                           old_value,
				                                           new_value,
				                                           &iter);
				break;
			
		}
	}

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
	                                        editor->priv->is_void_handler);
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
						editor->priv->is_void_userdata);
}

static void
glade_signal_editor_column_editing_started (GtkCellEditable *editable,
					    const gchar *path_str,
					    GladeSignalEditor *editor,
					    guint signal)
{
	GladeSignalEditorPrivate *priv = editor->priv;
	GtkTreeModel *model = GTK_TREE_MODEL (priv->model);
	GtkTreePath *path;
	GtkTreeIter iter;
	gchar *signal_name;

	path = gtk_tree_path_new_from_string (path_str);
	g_return_if_fail (gtk_tree_model_get_iter (model, &iter, path));
	gtk_tree_path_free (path);
	signal_name = glade_signal_editor_get_signal_name (model, &iter);
	switch (signal)
	{
		case HANDLER_EDITING_STARTED:
			glade_signal_editor_handler_editing_started_default (editor,
			                                                     signal_name,
			                                                     &iter,
			                                                     editable);
			break;
		case USERDATA_EDITING_STARTED:
			glade_signal_editor_userdata_editing_started_default (editor,
			                                                      signal_name,
			                                                      &iter,
			                                                      editable);
			break;
	}
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

static void 
glade_signal_editor_devhelp_cb (GtkCellRenderer   *cell,
				const gchar       *path_str,
				GladeSignalEditor *editor)
{
	GladeSignalEditorPrivate *priv = editor->priv;
	GtkTreePath  *path = gtk_tree_path_new_from_string (path_str);
	GtkTreeModel *model = GTK_TREE_MODEL (priv->model);
	GtkTreeIter   iter;
	GladeSignalClass *signal_class;
	gchar        *signal, *search, *book = NULL;

	g_return_if_fail (gtk_tree_model_get_iter (model, &iter, path));
	gtk_tree_path_free (path);

	signal  = glade_signal_editor_get_signal_name (model, &iter);
	search  = g_strdup_printf ("The %s signal", signal);

	signal_class = glade_widget_adaptor_get_signal_class (priv->widget->adaptor,
							      signal);
	g_assert (signal_class);

	g_object_get (signal_class->adaptor, "book", &book, NULL);

	glade_editor_search_doc_search (glade_app_get_editor (),
					book, signal_class->adaptor->name, search);

	g_free (search);
	g_free (book);
	g_free (signal);
}

static void
set_column_header_tooltip_on_realize (GtkWidget *label,
				      const gchar *tooltip_txt)
{
	GtkWidget *header = 
		gtk_widget_get_ancestor (label, GTK_TYPE_BUTTON);

	if (header)
		gtk_widget_set_tooltip_text (header, tooltip_txt);
}

/* convenience function: tooltip_txt must be static memory */
static void
column_header_widget (GtkTreeViewColumn *column,
		      const gchar *txt, 
		      const gchar *tooltip_txt)
{
	GtkWidget *event_box, *label;

	event_box = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (event_box), FALSE);

	gtk_widget_set_tooltip_text (event_box, tooltip_txt);

	label = gtk_label_new (txt);

	gtk_widget_show (event_box);
	gtk_widget_show (label);
	
	g_signal_connect_after (G_OBJECT (label), "realize",
				G_CALLBACK (set_column_header_tooltip_on_realize), (gpointer)tooltip_txt);

	gtk_container_add (GTK_CONTAINER (event_box), label);
	
	gtk_tree_view_column_set_widget (column, event_box);
}	

static void
glade_signal_editor_user_data_activate (GtkCellRenderer *icon_renderer,
					const gchar     *path_str,
					GladeSignalEditor *editor)
{
	GladeSignalEditorPrivate *priv = editor->priv;
	GtkTreePath  *path = gtk_tree_path_new_from_string (path_str);
	GtkTreeModel *model = GTK_TREE_MODEL (priv->model);
	GtkTreeIter   iter;
	gchar        *object_name = NULL, *signal_name = NULL, *handler = NULL;
	gboolean      after, swapped;
	GladeWidget  *project_object = NULL;
	GladeProject *project;
	GList        *selected = NULL, *exception = NULL;

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    GSE_COLUMN_HANDLER, &handler,
			    GSE_COLUMN_USERDATA,&object_name,
			    GSE_COLUMN_SWAPPED, &swapped,
			    GSE_COLUMN_AFTER,   &after, -1);

	signal_name  = glade_signal_editor_get_signal_name (model, &iter);
	project      = glade_widget_get_project (priv->widget);
	
	if (object_name)
	{
		project_object = glade_project_get_widget_by_name (project, NULL, object_name);
		selected = g_list_prepend (selected, project_object);
	}

	exception = g_list_prepend (exception, priv->widget);
	
	if (glade_editor_property_show_object_dialog (project,
						      _("Select an object to pass to the handler"),
						      gtk_widget_get_toplevel (GTK_WIDGET(editor)), 
						      G_TYPE_OBJECT,
						      priv->widget, &project_object))
	{
		GladeSignal *old_signal = glade_signal_new (signal_name, handler, object_name, after, swapped);
		GladeSignal *new_signal = glade_signal_new (signal_name, handler, 
							    project_object ? project_object->name : NULL, 
							    after, swapped);

		glade_command_change_signal (priv->widget, old_signal, new_signal);
		glade_signal_free (old_signal);
		glade_signal_free (new_signal);

		/* We are removing userdata */
		if (project_object == NULL)
		{
			gtk_tree_store_set (GTK_TREE_STORE (model),     &iter,
					    GSE_COLUMN_USERDATA_SLOT,   TRUE,
					    GSE_COLUMN_USERDATA,        USERDATA_DEFAULT,
					    GSE_COLUMN_SWAPPED,         FALSE,
					    GSE_COLUMN_SWAPPED_VISIBLE, FALSE, -1);
		}
		else
		{
			gtk_tree_store_set (GTK_TREE_STORE (model),     &iter,
					    GSE_COLUMN_USERDATA_SLOT,   FALSE,
					    GSE_COLUMN_USERDATA,        project_object->name,
					    GSE_COLUMN_SWAPPED_VISIBLE, TRUE,
					    -1);
		}
	}

	gtk_tree_path_free (path);
	g_free (signal_name);
	g_free (object_name);
	g_free (handler);
}


void
glade_signal_editor_construct_signals_list (GladeSignalEditor *editor)
{
	GtkTreeView *view;
	GtkWidget *view_widget;
	GtkTreeViewColumn *column;
 	GtkCellRenderer *renderer;
	GtkTreeModel *model;

	GladeSignalEditorPrivate *priv = editor->priv;
	
	priv->model = gtk_tree_store_new
		(GSE_NUM_COLUMNS,
		 G_TYPE_STRING,   /* Signal  value      */
		 G_TYPE_STRING,   /* Handler value      */
		 G_TYPE_BOOLEAN,  /* After   value      */
		 G_TYPE_STRING,   /* User data value    */
		 G_TYPE_BOOLEAN,  /* Swapped value */
		 G_TYPE_BOOLEAN,  /* Whether userdata is a slot */
		 G_TYPE_BOOLEAN,  /* Swapped visibility  */
		 G_TYPE_BOOLEAN,  /* After visibility */
		 G_TYPE_BOOLEAN,  /* Handler editable   */
		 G_TYPE_BOOLEAN,  /* Userdata editable  */
		 G_TYPE_BOOLEAN,  /* New slot           */
		 G_TYPE_BOOLEAN,  /* Mark with bold     */
		 G_TYPE_BOOLEAN,  /* Not a class title slot */ 
		 G_TYPE_BOOLEAN,  /* Show a warning icon for the signal */ 
		 G_TYPE_STRING);  /* A tooltip for the signal row */ 

	model = GTK_TREE_MODEL (priv->model);

	view_widget = gtk_tree_view_new_with_model (model);
	g_object_set (G_OBJECT (view_widget), "enable-search", FALSE, NULL);

	view = GTK_TREE_VIEW (view_widget);

	gtk_tree_view_set_tooltip_column (view, GSE_COLUMN_TOOLTIP);

	/* the view now holds a reference, we can get rid of our own */
	g_object_unref (G_OBJECT (priv->model));

	/************************ signal column ************************/
	column = gtk_tree_view_column_new ();
	column_header_widget (column, _("Signal"), _("The name of the signal to connect to"));

	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (G_OBJECT (renderer), "icon-name", GTK_STOCK_DIALOG_WARNING, NULL);

	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer, 
					     "visible", GSE_COLUMN_WARN, 
					     NULL);


 	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer),
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      "width-chars", 20,
		      NULL);
	gtk_tree_view_column_pack_end (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer, 
					     "text", GSE_COLUMN_SIGNAL, 
					     NULL);

	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 glade_signal_editor_signal_cell_data_func,
						 NULL, NULL);

 	gtk_tree_view_column_set_resizable (column, TRUE);
 	gtk_tree_view_column_set_expand (column, TRUE);
 	gtk_tree_view_append_column (view, column);

	/************************ handler column ************************/
 	if (!priv->handler_store)
			priv->handler_store = GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING));

	if (!priv->handler_renderer)
 	{
 		priv->handler_renderer = gtk_cell_renderer_combo_new ();
		g_object_set (G_OBJECT (priv->handler_renderer),
			      "model", priv->handler_store,
			      "text-column", 0,
			      "ellipsize", PANGO_ELLIPSIZE_END,
			      "width-chars", 14,
			      NULL);
	}

	g_signal_connect (priv->handler_renderer, "edited",
			  G_CALLBACK (glade_signal_editor_handler_cell_edited), editor);

	g_signal_connect (priv->handler_renderer, "editing-started",
			  G_CALLBACK (glade_signal_editor_handler_editing_started),
			  editor);

	if (!priv->handler_column)
	{
		priv->handler_column = gtk_tree_view_column_new_with_attributes
			(NULL,             priv->handler_renderer,
			 "editable",       GSE_COLUMN_HANDLER_EDITABLE,
			 "text",           GSE_COLUMN_HANDLER, NULL);

		column_header_widget (priv->handler_column, _("Handler"), 
				      _("Enter the handler to run for this signal"));

		gtk_tree_view_column_set_cell_data_func (priv->handler_column, priv->handler_renderer,
							 glade_signal_editor_handler_cell_data_func,
							 NULL, NULL);
	}

 	gtk_tree_view_column_set_resizable (priv->handler_column, TRUE);
 	gtk_tree_view_column_set_expand (priv->handler_column, TRUE);
 	gtk_tree_view_append_column (view, priv->handler_column);

	/************************ userdata column ************************/
 	if (!priv->userdata_renderer)
 	{
 		priv->userdata_renderer = gtk_cell_renderer_text_new ();
	}
			      
	if (!priv->userdata_store)
		priv->userdata_store = GTK_TREE_MODEL (gtk_list_store_new (1, G_TYPE_STRING));

	g_signal_connect (priv->userdata_renderer, "edited",
			  G_CALLBACK (glade_signal_editor_userdata_cell_edited), editor);

	g_signal_connect (priv->userdata_renderer, "editing-started",
			  G_CALLBACK (glade_signal_editor_userdata_editing_started),
			  editor);
	
	if (!priv->userdata_column)
	{
		priv->userdata_column =
			gtk_tree_view_column_new_with_attributes
				(NULL,        priv->userdata_renderer,
				 "text",      GSE_COLUMN_USERDATA, NULL);

		column_header_widget (priv->userdata_column, _("Object"), 
				      _("An object to pass to the handler"));

		gtk_tree_view_column_set_cell_data_func (priv->userdata_column, priv->userdata_renderer,
							 glade_signal_editor_userdata_cell_data_func,
							 NULL, NULL);

		g_object_set (G_OBJECT (priv->userdata_renderer), 
			      "editable", FALSE, 
			      "ellipsize", PANGO_ELLIPSIZE_END,
			      "width-chars", 10,
			      NULL);

		renderer = glade_cell_renderer_icon_new ();
		g_object_set (G_OBJECT (renderer), "icon-name", GTK_STOCK_EDIT, NULL);

		g_signal_connect (G_OBJECT (renderer), "activate",
				  G_CALLBACK (glade_signal_editor_user_data_activate), editor);
		gtk_tree_view_column_pack_end (priv->userdata_column, renderer, FALSE);
		gtk_tree_view_column_set_attributes (priv->userdata_column, renderer, 
						     "activatable", GSE_COLUMN_USERDATA_EDITABLE,
						     "visible",     GSE_COLUMN_USERDATA_EDITABLE, 
						     NULL);
	}

 	gtk_tree_view_column_set_resizable (priv->userdata_column, TRUE);
 	gtk_tree_view_column_set_expand (priv->userdata_column, TRUE);
 	gtk_tree_view_append_column (view, priv->userdata_column);

	/************************ swapped column ************************/
 	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled",
			  G_CALLBACK (glade_signal_editor_after_swapped_toggled), editor);
	column = gtk_tree_view_column_new_with_attributes
		(NULL, renderer,
		 "active",  GSE_COLUMN_SWAPPED,
		 "sensitive", GSE_COLUMN_SWAPPED_VISIBLE, 
		 "activatable", GSE_COLUMN_SWAPPED_VISIBLE,
		 "visible", GSE_COLUMN_CONTENT,
		 NULL);

	column_header_widget (column,_("Swap"), 
			      _("Whether the instance and object should be swapped when calling the handler"));

 	gtk_tree_view_append_column (view, column);

	/* - No need for a ref here - */
	priv->swapped_column_ptr = column;

	/************************ after column ************************/
	renderer = gtk_cell_renderer_toggle_new ();

	g_object_set (G_OBJECT (renderer), 
		      "xpad", 15,
		      NULL);
	g_object_set_data (G_OBJECT (renderer), "signal-after-cell",
			       GINT_TO_POINTER (TRUE));

	g_signal_connect (renderer, "toggled",
			  G_CALLBACK (glade_signal_editor_after_swapped_toggled), editor);
 	column = gtk_tree_view_column_new_with_attributes
		(NULL, renderer,
		 "active",  GSE_COLUMN_AFTER,
		 "sensitive", GSE_COLUMN_AFTER_VISIBLE, 
		 "activatable", GSE_COLUMN_AFTER_VISIBLE, 
		 "visible", GSE_COLUMN_CONTENT,
		 NULL);

	column_header_widget (column, _("After"), 
			      _("Whether the handler should be called before "
				"or after the default handler of the signal"));

	/* Append the devhelp icon if we have it */
	if (glade_util_have_devhelp ())
	{
		renderer = glade_cell_renderer_icon_new ();

		g_object_set (G_OBJECT (renderer), 
			      "activatable", TRUE,
			      NULL);

		if (gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), GLADE_DEVHELP_ICON_NAME))
			g_object_set (G_OBJECT (renderer), "icon-name", GLADE_DEVHELP_ICON_NAME, NULL);
		else
			g_object_set (G_OBJECT (renderer), "icon-name", GTK_STOCK_INFO, NULL);

		g_signal_connect (G_OBJECT (renderer), "activate",
				  G_CALLBACK (glade_signal_editor_devhelp_cb), editor);

		gtk_tree_view_column_pack_end (column, renderer, FALSE);
		gtk_tree_view_column_set_attributes (column, renderer, 
						     "visible", GSE_COLUMN_CONTENT, NULL);

	}

 	gtk_tree_view_append_column (view, column);


	priv->signals_list = view_widget;
}

static GObject*
glade_signal_editor_constructor (GType                  type,
				 guint                  n_construct_properties,
				 GObjectConstructParam *construct_properties)
{
	GtkWidget *scroll;
	GladeSignalEditor *editor;
	GladeSignalEditorPrivate *priv;
	GObject *retval;

	retval = G_OBJECT_CLASS (glade_signal_editor_parent_class)->constructor
		(type, n_construct_properties, construct_properties);
	editor = GLADE_SIGNAL_EDITOR (retval);
	priv = editor->priv;

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_NONE);

	g_signal_emit_by_name (glade_app_get(), "signal-editor-created", retval);

	gtk_container_add (GTK_CONTAINER (scroll), priv->signals_list);

	gtk_box_pack_start (GTK_BOX (editor), scroll, TRUE, TRUE, 0);

	gtk_widget_show_all (GTK_WIDGET(editor));

	return retval;
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
	signal_editor->priv->editor = editor;

	return signal_editor;
}

static void
glade_signal_editor_refresh_support (GladeWidget *widget,
				     GladeSignalEditor *editor)
{
	GladeSignalEditorPrivate *priv = editor->priv;
	
	g_assert (priv->widget == widget);
	glade_signal_editor_load_widget (editor, priv->widget);
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

	GladeSignalEditorPrivate *priv = editor->priv;


	g_return_if_fail (GLADE_IS_SIGNAL_EDITOR (editor));
	g_return_if_fail (widget == NULL || GLADE_IS_WIDGET (widget));

	if (priv->widget != widget)
	{
		if (priv->widget)
		{
			g_signal_handler_disconnect (priv->widget, priv->refresh_id);
			priv->refresh_id = 0;
			g_object_unref (priv->widget);
		}
		
		priv->widget = widget;
		priv->adaptor = widget ? widget->adaptor : NULL;
		
		if (priv->widget)
		{
			g_object_ref (priv->widget);
			priv->refresh_id =
				g_signal_connect (G_OBJECT (priv->widget), "support-changed",
						  G_CALLBACK (glade_signal_editor_refresh_support), editor);
		}
	}
	
	if (!widget)
		return;

	gtk_tree_store_clear (priv->model);

	if (glade_project_get_format (glade_widget_get_project (widget)) == GLADE_PROJECT_FORMAT_GTKBUILDER)
		gtk_tree_view_column_set_visible (priv->swapped_column_ptr, TRUE);
	else
		gtk_tree_view_column_set_visible (priv->swapped_column_ptr, FALSE);

	/* Loop over every signal type
	 */
	for (list = priv->adaptor->signals; list; list = list->next)
	{
		GladeSignalClass *signal = (GladeSignalClass *) list->data;
		GladeSignal *sig = glade_signal_new (signal->name, NULL, NULL, FALSE, FALSE);

		/* Check versioning here with a virtual signal */
		glade_project_update_signal_support_warning (priv->widget, sig);

		/* Add class name that this signal belongs to.
		 */
		if (strcmp(last_type, signal->type))
		{
			gtk_tree_store_append (priv->model, &parent_class, NULL);
			gtk_tree_store_set    (priv->model,          &parent_class,
					       GSE_COLUMN_SIGNAL,           signal->type,
					       GSE_COLUMN_AFTER_VISIBLE,    FALSE,
					       GSE_COLUMN_HANDLER_EDITABLE, FALSE,
					       GSE_COLUMN_USERDATA_EDITABLE,FALSE,
					       GSE_COLUMN_SLOT,             FALSE,
					       GSE_COLUMN_BOLD,             FALSE, 
					       GSE_COLUMN_CONTENT,          FALSE,
					       -1);
			last_type = signal->type;
		}

		gtk_tree_store_append (priv->model, &parent_signal, &parent_class);
		signals = glade_widget_list_signal_handlers (widget, signal->name);

		if (!signals || signals->len == 0)
		{

			gtk_tree_store_set
				(priv->model,              &parent_signal,
				 GSE_COLUMN_SIGNAL,           signal->name,
				 GSE_COLUMN_HANDLER,          HANDLER_DEFAULT,
				 GSE_COLUMN_AFTER,            FALSE,
				 GSE_COLUMN_USERDATA,         USERDATA_DEFAULT,
				 GSE_COLUMN_SWAPPED,          FALSE,
				 GSE_COLUMN_SWAPPED_VISIBLE,  FALSE,
				 GSE_COLUMN_HANDLER_EDITABLE, TRUE,
				 GSE_COLUMN_USERDATA_EDITABLE,FALSE,
				 GSE_COLUMN_AFTER_VISIBLE,    FALSE,
				 GSE_COLUMN_SLOT,             TRUE,
				 GSE_COLUMN_USERDATA_SLOT,    TRUE,
				 GSE_COLUMN_CONTENT,          TRUE,
				 GSE_COLUMN_WARN,             sig->support_warning != NULL,
				 GSE_COLUMN_TOOLTIP,          sig->support_warning,	
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
			gtk_tree_store_set (priv->model, &parent_class, GSE_COLUMN_BOLD, TRUE, -1);
			path_parent_class =
				gtk_tree_model_get_path (GTK_TREE_MODEL (priv->model),
							 &parent_class);
			gtk_tree_view_expand_row (GTK_TREE_VIEW (priv->signals_list),
						  path_parent_class, FALSE);
			gtk_tree_path_free (path_parent_class);

			gtk_tree_store_set
				(priv->model,            &parent_signal,
				 GSE_COLUMN_SIGNAL,             signal->name,
				 GSE_COLUMN_HANDLER,            widget_signal->handler,
				 GSE_COLUMN_AFTER,              widget_signal->after,
				 GSE_COLUMN_USERDATA,
				 widget_signal->userdata ?
				 widget_signal->userdata : USERDATA_DEFAULT,
				 GSE_COLUMN_SWAPPED,            widget_signal->swapped,
				 GSE_COLUMN_SWAPPED_VISIBLE,
				 widget_signal->userdata ?  TRUE : FALSE,
				 GSE_COLUMN_AFTER_VISIBLE,      TRUE,
				 GSE_COLUMN_HANDLER_EDITABLE,   TRUE,
				 GSE_COLUMN_USERDATA_EDITABLE,  TRUE,
				 GSE_COLUMN_SLOT,               FALSE,
				 GSE_COLUMN_USERDATA_SLOT,
				 widget_signal->userdata  ? FALSE : TRUE,
				 GSE_COLUMN_BOLD,               TRUE, 
				 GSE_COLUMN_CONTENT,            TRUE,
				 GSE_COLUMN_WARN,               widget_signal->support_warning != NULL,
				 GSE_COLUMN_TOOLTIP,            widget_signal->support_warning,
				 -1);

			for (i = 1; i < signals->len; i++)
			{
				widget_signal = (GladeSignal*) g_ptr_array_index (signals, i);
				gtk_tree_store_append (priv->model, &iter, &parent_signal);

				gtk_tree_store_set
					(priv->model,            &iter,
					 GSE_COLUMN_HANDLER,            widget_signal->handler,
					 GSE_COLUMN_AFTER,              widget_signal->after,
					 GSE_COLUMN_USERDATA,
					 widget_signal->userdata  ?
					 widget_signal->userdata : USERDATA_DEFAULT,
					 GSE_COLUMN_SWAPPED,            widget_signal->swapped,
					 GSE_COLUMN_SWAPPED_VISIBLE,
					 widget_signal->userdata  ? TRUE : FALSE,
					 GSE_COLUMN_AFTER_VISIBLE,      TRUE,
					 GSE_COLUMN_HANDLER_EDITABLE,   TRUE,
					 GSE_COLUMN_USERDATA_EDITABLE,  TRUE,
					 GSE_COLUMN_SLOT,               FALSE,
					 GSE_COLUMN_USERDATA_SLOT,
					 widget_signal->userdata  ? FALSE : TRUE,
					 GSE_COLUMN_CONTENT,            TRUE,
					 GSE_COLUMN_WARN,               FALSE,
					 GSE_COLUMN_TOOLTIP,            widget_signal->support_warning,
					 -1);
			}

			/* add the <Type...> slot */
			gtk_tree_store_append (priv->model, &iter, &parent_signal);
			gtk_tree_store_set
				(priv->model,          &iter,
				 GSE_COLUMN_HANDLER,          HANDLER_DEFAULT,
				 GSE_COLUMN_AFTER,            FALSE,
				 GSE_COLUMN_USERDATA,         USERDATA_DEFAULT,
				 GSE_COLUMN_SWAPPED,          FALSE,
				 GSE_COLUMN_SWAPPED_VISIBLE,  FALSE,
				 GSE_COLUMN_HANDLER_EDITABLE, TRUE,
				 GSE_COLUMN_USERDATA_EDITABLE,FALSE,
				 GSE_COLUMN_AFTER_VISIBLE,    FALSE,
				 GSE_COLUMN_SLOT,             TRUE,
				 GSE_COLUMN_USERDATA_SLOT,    TRUE,
				 GSE_COLUMN_CONTENT,          TRUE,
				 GSE_COLUMN_WARN,             FALSE,
				 GSE_COLUMN_TOOLTIP,          sig->support_warning,
				 -1);
		}

		glade_signal_free (sig);

	}

	path_first = gtk_tree_path_new_first ();
	gtk_tree_view_expand_row (GTK_TREE_VIEW (priv->signals_list), path_first, FALSE);
	gtk_tree_path_free (path_first);
}

static void
glade_signal_editor_get_property  (GObject      *object,
				   guint         prop_id,
				   GValue *value,
				   GParamSpec   *pspec)
{
	GladeSignalEditor *self = GLADE_SIGNAL_EDITOR (object);
	GladeSignalEditorPrivate *priv = self->priv;

	switch (prop_id)
	{
	case PROP_HANDLER_COLUMN:
		g_value_set_object (value, priv->handler_column);
		break;
	case PROP_USERDATA_COLUMN:
		g_value_set_object (value, priv->userdata_column);
		break;
	case PROP_HANDLER_COMPLETION:
		g_value_set_object (value, priv->handler_store);
		break;
	case PROP_USERDATA_COMPLETION:
		g_value_set_object (value, priv->userdata_store);
		break;
	case PROP_HANDLER_RENDERER:
		g_value_set_object (value, priv->handler_renderer);
		break;
	case PROP_USERDATA_RENDERER:
		g_value_set_object (value, priv->userdata_renderer);
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
	GladeSignalEditorPrivate *priv = self->priv;

	switch (prop_id)
	{
	case PROP_HANDLER_COLUMN:
		priv->handler_column = g_value_get_object (value);
		break;
	case PROP_USERDATA_COLUMN:
		priv->userdata_column = g_value_get_object (value);
		break;
	case PROP_HANDLER_COMPLETION:
		priv->handler_store = g_value_get_object (value);
		break;
	case PROP_USERDATA_COMPLETION:
		priv->userdata_store = g_value_get_object (value);
		break;
	case PROP_HANDLER_RENDERER:
		priv->handler_renderer = g_value_get_object (value);
		break;
	case PROP_USERDATA_RENDERER:
		priv->userdata_renderer = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_signal_editor_dispose (GObject *object)
{
	GladeSignalEditor *self = GLADE_SIGNAL_EDITOR (object);
	
	glade_signal_editor_load_widget (self, NULL);

	G_OBJECT_CLASS (glade_signal_editor_parent_class)->dispose (object);
}

static void
glade_signal_editor_init (GladeSignalEditor *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GLADE_TYPE_SIGNAL_EDITOR, GladeSignalEditorPrivate);
	
	self->priv->is_void_handler = is_void_handler;
	self->priv->is_void_userdata = is_void_userdata;
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
	object_class->dispose = glade_signal_editor_dispose;

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
	g_type_class_add_private (klass, sizeof (GladeSignalEditorPrivate));
}

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
	GladeSignalEditorPrivate *priv = editor->priv;
	
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

	name = (gchar *) glade_widget_get_name (priv->widget);

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

static gboolean
glade_signal_editor_handler_editing_started_default (GladeSignalEditor *editor,
                                                     gchar *signal_name,
                                                     GtkTreeIter *iter,
                                                     GtkCellEditable *editable)
{
	GladeSignalEditorPrivate *priv = editor->priv;
	GtkEntry *entry;
	GtkEntryCompletion *completion;
	GtkTreeModel *completion_store = priv->handler_store;

	g_return_val_if_fail (GTK_IS_BIN (editable), FALSE);
	g_return_val_if_fail (GTK_IS_LIST_STORE (completion_store), FALSE);

	entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (editable)));

	glade_signal_editor_editing_started (entry, priv->is_void_handler);

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

	GladeSignalEditorPrivate *priv = self->priv;

	gtk_list_store_clear (store);

	for (list = (GList *) glade_project_get_objects (priv->widget->project);
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

static gboolean
glade_signal_editor_userdata_editing_started_default (GladeSignalEditor *editor,
                                                      gchar *signal_name,
                                                      GtkTreeIter *iter,
                                                      GtkCellEditable *editable)
{
	GtkEntry *entry;
	GtkEntryCompletion *completion;
	GladeSignalEditorPrivate *priv = editor->priv;
	GtkTreeModel *completion_store = priv->userdata_store;
	
	g_return_val_if_fail (priv->widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_LIST_STORE (completion_store), FALSE);
	g_return_val_if_fail (GTK_IS_ENTRY (editable), FALSE);

	entry = GTK_ENTRY (editable);

	glade_signal_editor_editing_started (entry, priv->is_void_handler);

	glade_signal_editor_userdata_store_update (editor, GTK_LIST_STORE (completion_store));

	completion = gtk_entry_completion_new ();
	gtk_entry_completion_set_text_column (completion, 0);
	gtk_entry_completion_set_model (completion, completion_store);
	gtk_entry_set_completion (entry, completion);

	return FALSE;
}
