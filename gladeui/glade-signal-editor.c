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

G_DEFINE_TYPE (GladeSignalEditor, glade_signal_editor, GTK_TYPE_VBOX)

struct _GladeSignalEditorPrivate
{
	GtkTreeModel *model;
	
	GladeWidget *widget;
	GladeWidgetAdaptor* adaptor;

	GtkWidget* signal_tree;
};

#define HANDLER_DEFAULT  _("<Type here>")
#define USERDATA_DEFAULT _("<Object>")

/**
 * glade_signal_editor_new:
 * @editor: a #GladeEditor
 *
 * Returns: a new #GladeSignalEditor associated with @editor
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
 * @widget: a #GladeWidget
 *
 * TODO: write me
 */
void
glade_signal_editor_load_widget (GladeSignalEditor *editor,
				 GladeWidget *widget)
{
	GladeSignalEditorPrivate *priv = editor->priv;
	
	if (priv->widget != widget)
	{
		if (priv->widget)
		
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
	GtkTreeViewColumn* column;

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GLADE_TYPE_SIGNAL_EDITOR, GladeSignalEditorPrivate);

	/* Create signal tree */
	self->priv->signal_tree = gtk_tree_view_new ();

	/* Create columns */
	/* Signal name */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Signal"),
	                                                   renderer,
	                                                   "text", GLADE_SIGNAL_COLUMN_NAME,
	                                                   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (self->priv->signal_tree), column);

	/* Signal handler */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "foreground", "grey", NULL);
	column = gtk_tree_view_column_new_with_attributes (_("Handler"),
	                                                   renderer,
	                                                   "text", GLADE_SIGNAL_COLUMN_HANDLER,
	                                                   "visible", GLADE_SIGNAL_COLUMN_IS_HANDLER,
	                                                   "foreground-set", GLADE_SIGNAL_COLUMN_IS_DUMMY,
	                                                   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (self->priv->signal_tree), column);

	/* Signal user_data */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("User data"),
	                                                   renderer,
	                                                   "text", GLADE_SIGNAL_COLUMN_OBJECT,
	                                                   "visible", GLADE_SIGNAL_COLUMN_IS_HANDLER,
	                                                   "sensitive", GLADE_SIGNAL_COLUMN_IS_DUMMY, 
	                                                   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (self->priv->signal_tree), column);

	/* Swap signal */
	renderer = gtk_cell_renderer_toggle_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Swap"),
	                                                   renderer,
	                                                   "active", GLADE_SIGNAL_COLUMN_SWAP,
	                                                   "visible", GLADE_SIGNAL_COLUMN_IS_HANDLER,
	                                                   "sensitive", GLADE_SIGNAL_COLUMN_IS_DUMMY,
	                                                   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (self->priv->signal_tree), column);

	/* After */
	renderer = gtk_cell_renderer_toggle_new ();
	column = gtk_tree_view_column_new_with_attributes (_("After"),
	                                                   renderer,
	                                                   "active", GLADE_SIGNAL_COLUMN_AFTER,
	                                                   "visible", GLADE_SIGNAL_COLUMN_IS_HANDLER,
	                                                   "sensitive", GLADE_SIGNAL_COLUMN_IS_DUMMY,
	                                                   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (self->priv->signal_tree), column);
	
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
