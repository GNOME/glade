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
#include "glade-signal-model.h"
#include "glade-cell-renderer-icon.h"
#include "glade-editor.h"
#include "glade-command.h"
#include "glade-marshallers.h"
#include "glade-accumulators.h"
#include "glade-project.h"

G_DEFINE_TYPE (GladeSignalEditor, glade_signal_editor, GTK_TYPE_VBOX)

struct _GladeSignalEditorPrivate
{
	GtkTreeModel *model;
	
	GladeWidget *widget;
	GladeWidgetAdaptor* adaptor;

	GtkWidget* signal_tree;
	GtkTreeViewColumn* column_name;	
	GtkTreeViewColumn* column_handler;
	GtkTreeViewColumn* column_userdata;
	GtkTreeViewColumn* column_swap;
	GtkTreeViewColumn* column_after;

	GtkCellRenderer* renderer_userdata;
};

/* Signal handlers */
static void
on_handler_edited (GtkCellRendererText* renderer,
                   gchar* path,
                   gchar* handler,
                   gpointer user_data)
{
	GladeSignalEditor* self = GLADE_SIGNAL_EDITOR(user_data);
	GtkTreePath* tree_path = gtk_tree_path_new_from_string (path);
	GtkTreeIter iter;
	gchar* name;
	gchar* old_handler;
	gboolean not_dummy;

	g_return_if_fail (self->priv->widget != NULL);
	
	gtk_tree_model_get_iter (self->priv->model,
	                         &iter,
	                         tree_path);

	gtk_tree_model_get (self->priv->model, &iter,
	                    GLADE_SIGNAL_COLUMN_NOT_DUMMY, &not_dummy,
	                    GLADE_SIGNAL_COLUMN_HANDLER, &old_handler, -1);
	
	/* False alarm ? */
	if (handler && !g_str_equal (old_handler, handler))
	{
		if (not_dummy)
		{
			/* change an existing signal handler */
			GladeSignal* old_signal;
			GladeSignal* new_signal;

			gtk_tree_model_get (self->priv->model,
			                    &iter,
			                    GLADE_SIGNAL_COLUMN_SIGNAL,
			                    &old_signal, -1);

			new_signal = glade_signal_clone (old_signal);

			/* Change the new signal handler */
			g_free (new_signal->handler);
			new_signal->handler = g_strdup(handler);

			glade_command_change_signal (self->priv->widget, old_signal, new_signal);

			glade_signal_free (new_signal);
		}
		else
		{
			GtkTreeIter parent;
			GladeSignal* signal;

			/* Get the signal name */
			gtk_tree_model_iter_parent (self->priv->model, &parent, &iter);
			gtk_tree_model_get (self->priv->model, &parent,
			                    GLADE_SIGNAL_COLUMN_NAME, &name, 
			                    -1);
			
			/* Add a new signal handler */
			signal = glade_signal_new (name, handler, NULL, FALSE, FALSE);
			glade_command_add_signal (self->priv->widget, signal);

			/* Select next column */
			gtk_tree_view_set_cursor (GTK_TREE_VIEW(self->priv->signal_tree),
			                          tree_path,
			                          self->priv->column_userdata,
			                          TRUE);
			glade_signal_free (signal);
		}
	}
	g_free (name);
	g_free (old_handler);
	gtk_tree_path_free (tree_path);
}

static void
on_userdata_edited (GtkCellRendererText* renderer,
                    gchar* path,
                    gchar* new_userdata,
                    gpointer user_data)
{
	GladeSignalEditor* self = GLADE_SIGNAL_EDITOR(user_data);
	GtkTreePath* tree_path = gtk_tree_path_new_from_string (path);
	GtkTreeIter iter;
	gchar* old_userdata;

	g_return_if_fail (self->priv->widget != NULL);
	
	gtk_tree_model_get_iter (self->priv->model,
	                         &iter,
	                         tree_path);

	gtk_tree_model_get (self->priv->model, &iter,
	                    GLADE_SIGNAL_COLUMN_OBJECT, &old_userdata, -1);
	
	/* False alarm ? */
	if (new_userdata && !g_str_equal (old_userdata, new_userdata))
	{
		/* change an existing signal handler */
		GladeSignal* old_signal;
		GladeSignal* new_signal;

		gtk_tree_model_get (self->priv->model,
		                    &iter,
		                    GLADE_SIGNAL_COLUMN_SIGNAL,
		                    &old_signal, -1);

		new_signal = glade_signal_clone (old_signal);

		/* Change the new signal handler */
		g_free (new_signal->userdata);
		new_signal->userdata = g_strdup(new_userdata);

		glade_command_change_signal (self->priv->widget, old_signal, new_signal);

		glade_signal_free (new_signal);
	}
	g_free (old_userdata);
	gtk_tree_path_free (tree_path);
}

static void
on_swap_toggled (GtkCellRendererToggle* renderer,
                 gchar* path,
                 gpointer user_data)
{
	GladeSignalEditor* self = GLADE_SIGNAL_EDITOR(user_data);
	GtkTreePath* tree_path = gtk_tree_path_new_from_string (path);
	GtkTreeIter iter;
	GladeSignal* old_signal;
	GladeSignal* new_signal;

	g_return_if_fail (self->priv->widget != NULL);
	
	gtk_tree_model_get_iter (self->priv->model,
	                         &iter,
	                         tree_path);

	gtk_tree_model_get (self->priv->model,
		                    &iter,
		                    GLADE_SIGNAL_COLUMN_SIGNAL,
		                    &old_signal, -1);

	new_signal = glade_signal_clone (old_signal);

	/* Change the new signal handler */
	new_signal->swapped = !gtk_cell_renderer_toggle_get_active (renderer);

	glade_command_change_signal (self->priv->widget, old_signal, new_signal);

	glade_signal_free (new_signal);
	
	gtk_tree_path_free (tree_path);		
}

static void
on_after_toggled (GtkCellRendererToggle* renderer,
                  gchar* path,
                  gpointer user_data)
{
	GladeSignalEditor* self = GLADE_SIGNAL_EDITOR(user_data);
	GtkTreePath* tree_path = gtk_tree_path_new_from_string (path);
	GtkTreeIter iter;
	GladeSignal* old_signal;
	GladeSignal* new_signal;

	g_return_if_fail (self->priv->widget != NULL);
	
	gtk_tree_model_get_iter (self->priv->model,
	                         &iter,
	                         tree_path);

	gtk_tree_model_get (self->priv->model,
		                    &iter,
		                    GLADE_SIGNAL_COLUMN_SIGNAL,
		                    &old_signal, -1);

	new_signal = glade_signal_clone (old_signal);

	/* Change the new signal handler */
	new_signal->after = !gtk_cell_renderer_toggle_get_active (renderer);

	glade_command_change_signal (self->priv->widget, old_signal, new_signal);

	glade_signal_free (new_signal);
	
	gtk_tree_path_free (tree_path);	
}

/**
 * glade_signal_editor_new:
 *
 * Returns: a new #GladeSignalEditor
 */
GladeSignalEditor *
glade_signal_editor_new ()
{
	GladeSignalEditor *signal_editor;

	signal_editor = GLADE_SIGNAL_EDITOR (g_object_new (GLADE_TYPE_SIGNAL_EDITOR,
	                                                   NULL, NULL));

	return signal_editor;
}

/**
 * glade_signal_editor_load_widget:
 * @editor: a #GladeSignalEditor
 * @widget: a #GladeWidget or NULL
 *
 * Load a widget in the signal editor. This will show all signals of the widget
 * an it's accessors in the signal editor where the user can edit them.
 */
void
glade_signal_editor_load_widget (GladeSignalEditor *editor,
				 GladeWidget *widget)
{
	GladeSignalEditorPrivate *priv = editor->priv;
	
	if (priv->widget != widget)
	{	
		priv->widget = widget;
		priv->adaptor = widget ? widget->adaptor : NULL;
		
		if (priv->widget)
		{
			g_object_ref (priv->widget);
		}
	}

	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->signal_tree), NULL);
	if (priv->model)
		g_object_unref (priv->model);
	priv->model = NULL;

	
	if (!widget)
		return;

	priv->model = glade_signal_model_new (widget);
	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->signal_tree), priv->model);
	g_object_set (priv->renderer_userdata, "model", glade_app_get_project (), NULL);
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
	GtkWidget *scroll;
	GtkCellRenderer* renderer;
	GladeSignalEditorPrivate* priv;
	
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GLADE_TYPE_SIGNAL_EDITOR, GladeSignalEditorPrivate);
	priv = self->priv;
	
	/* Create signal tree */
	priv->signal_tree = gtk_tree_view_new ();

	/* Create columns */
	/* Signal name */
	renderer = gtk_cell_renderer_text_new ();
	priv->column_name = gtk_tree_view_column_new_with_attributes (_("Signal"),
	                                                              renderer,
	                                                              "text", GLADE_SIGNAL_COLUMN_NAME,
	                                                              NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (self->priv->signal_tree), priv->column_name);

	/* Signal handler */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, 
	              "editable", TRUE, NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK(on_handler_edited), self);
	priv->column_handler = gtk_tree_view_column_new_with_attributes (_("Handler"),
	                                                                  renderer,
	                                                                  "text", GLADE_SIGNAL_COLUMN_HANDLER,
	                                                                  "visible", GLADE_SIGNAL_COLUMN_IS_HANDLER,
	                                                                  NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (self->priv->signal_tree), priv->column_handler);
	/* Signal user_data */
	priv->renderer_userdata = gtk_cell_renderer_combo_new ();
	g_signal_connect (priv->renderer_userdata, "edited", G_CALLBACK(on_userdata_edited),
	                  self);
	g_object_set (priv->renderer_userdata, 
	              "has-entry", FALSE,
	              "text-column", GLADE_PROJECT_MODEL_COLUMN_NAME,
	              NULL);

	priv->column_userdata = gtk_tree_view_column_new_with_attributes (_("User data"),
	                                                              priv->renderer_userdata,
	                                                              "text", GLADE_SIGNAL_COLUMN_OBJECT,
	                                                              "visible", GLADE_SIGNAL_COLUMN_IS_HANDLER,
	                                                              "sensitive", GLADE_SIGNAL_COLUMN_NOT_DUMMY,
	                                                               "editable", GLADE_SIGNAL_COLUMN_NOT_DUMMY,
	                                                              NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (self->priv->signal_tree), priv->column_userdata);

	/* Swap signal */
	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled", G_CALLBACK (on_swap_toggled), self);
	priv->column_swap = gtk_tree_view_column_new_with_attributes (_("Swap"),
	                                                              renderer,
	                                                              "active", GLADE_SIGNAL_COLUMN_SWAP,
	                                                              "visible", GLADE_SIGNAL_COLUMN_IS_HANDLER,
	                                                              "sensitive", GLADE_SIGNAL_COLUMN_NOT_DUMMY,
	                                                              NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (self->priv->signal_tree), priv->column_swap);

	/* After */
	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled", G_CALLBACK (on_after_toggled), self);
	priv->column_after = gtk_tree_view_column_new_with_attributes (_("After"),
	                                                               renderer,
	                                                               "active", GLADE_SIGNAL_COLUMN_AFTER,
	                                                               "visible", GLADE_SIGNAL_COLUMN_IS_HANDLER,
	                                                               "sensitive", GLADE_SIGNAL_COLUMN_NOT_DUMMY,
	                                                               NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (self->priv->signal_tree), priv->column_after);
	
	/* Create scrolled window */
	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_IN);
	
	gtk_container_add (GTK_CONTAINER (scroll), self->priv->signal_tree);

	gtk_box_pack_start (GTK_BOX (self), scroll, TRUE, TRUE, 0);

	gtk_widget_show_all (GTK_WIDGET(self));
}

static void
glade_signal_editor_class_init (GladeSignalEditorClass *klass)
{
	GObjectClass *object_class;

	glade_signal_editor_parent_class = g_type_class_peek_parent (klass);

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = glade_signal_editor_dispose;

	g_type_class_add_private (klass, sizeof (GladeSignalEditorPrivate));
}
