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


static void
glade_signal_editor_append_column (GtkTreeView *view, const gint col_num, const gchar *name)
{
	GtkTreeViewColumn *column;
 	GtkCellRenderer *renderer;
 	
 	renderer = gtk_cell_renderer_text_new ();
 	column = gtk_tree_view_column_new_with_attributes (name, renderer,
 							   "text", col_num, NULL);
 	gtk_tree_view_append_column (view, column);
}
 

static gboolean
glade_signal_editor_dialog_list_select_cb (GtkTreeSelection *lst_select, 
					   GtkTreeModel *lst_model, GtkTreePath *lst_path, gboolean path_selected, GladeSignalEditor *editor)
{
	GtkTreeIter iter;
	GValue *label;
	GList *list = NULL;
	GladeWidgetClassSignal *signal;
	
	label = g_new0 (GValue, 1);
	gtk_tree_model_get_iter (lst_model, &iter, lst_path);
	gtk_tree_model_get_value (lst_model, &iter, 0, label);

	list = editor->class->signals;
	for ( ; list != NULL; list = list->next) {
		signal = (GladeWidgetClassSignal *) list->data;
		if (!strcmp (signal->name, label->data[0].v_pointer)) {
			return TRUE;
		}
	}

	return FALSE;
}

static GtkWidget *
glade_signal_editor_dialog_construct_list (GladeSignalEditor *editor)
{
	GtkTreeSelection *selection;
	GtkTreeStore *model;
	GtkTreeView *view;
	GtkWidget *view_widget;
	
	model = gtk_tree_store_new (1, G_TYPE_STRING);

	view_widget = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
	view = GTK_TREE_VIEW (view_widget);

	g_object_unref (G_OBJECT (model));

	glade_signal_editor_append_column (view, 0, _("Signals"));

	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_select_function (selection,
						(GtkTreeSelectionFunc) glade_signal_editor_dialog_list_select_cb,
						editor, NULL);

	gtk_widget_set_usize (view_widget, 150, 200);

	return view_widget;
}

static GtkWidget * 
glade_signal_editor_dialog_construct (GladeSignalEditor *editor, GtkWidget **view)
{
	GtkWidget *dialog;
	GtkWidget *scroll;
	GtkWidget *vbox;

	dialog = gtk_dialog_new_with_buttons (
		_("Select signal"),
		NULL,
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

	vbox = gtk_bin_get_child (GTK_BIN (dialog));

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start_defaults (GTK_BOX (vbox), GTK_WIDGET (scroll));

	*view = glade_signal_editor_dialog_construct_list (editor);
	gtk_container_add (GTK_CONTAINER (scroll), *view);

	gtk_widget_show_all (vbox);

	return dialog;
}

static GtkTreeIter *
glade_signal_editor_dialog_append_signal (GtkTreeStore *lst_model,
					  const gchar *label_text,
					  GtkTreeIter *parent)
{
	GtkTreeIter *iter;
	GValue *label;
	
	label = g_new0 (GValue, 1);
	label = g_value_init (label, G_TYPE_STRING);

	iter = g_new0 (GtkTreeIter, 1);
	gtk_tree_store_append (GTK_TREE_STORE (lst_model), iter, parent);
	label->data[0].v_pointer = (gchar *) label_text;
	gtk_tree_store_set_value (GTK_TREE_STORE (lst_model), iter, 0, label);

	return iter;
}

static void
glade_signal_editor_dialog_load_signals (GladeSignalEditor *editor, GtkWidget *view)
{
	GtkTreeStore *lst_model;
	gchar *type;
	GtkTreeIter *parent = NULL;
	GList *list = NULL;
	GladeWidgetClassSignal *signal;
  
	type = "";
	lst_model = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (view)));
  
	for (list = editor->class->signals; list != NULL; list = list->next) {
		signal = (GladeWidgetClassSignal *) list->data;
  
		if (strcmp (type, signal->type)) {
 			parent = glade_signal_editor_dialog_append_signal (lst_model, signal->type, NULL);
  			type = signal->type;
  		}
 		glade_signal_editor_dialog_append_signal (lst_model, signal->name, parent);
	}
}

static void
glade_signal_editor_dialog_cb (GtkButton *button, GladeSignalEditor *editor)
{
	GValue *label;
	GtkTreeIter *iter;
	GtkTreeModel *lst_model;
	GtkTreeSelection *lst_selected;
	GtkWidget *dialog;
	GtkWidget *view;
	gint response;

	g_return_if_fail (editor->class->signals != NULL);

	dialog = glade_signal_editor_dialog_construct (editor, &view);
	glade_signal_editor_dialog_load_signals (editor, view);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	if (response == GTK_RESPONSE_OK) {
		lst_model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
		lst_selected = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

		iter = g_new0 (GtkTreeIter, 1);
		if (gtk_tree_selection_get_selected (lst_selected, &lst_model, iter)) {
			label = g_new0 (GValue, 1);
			gtk_tree_model_get_value (lst_model, iter, 0, label);
			gtk_entry_set_text (GTK_ENTRY (editor->signal_name_entry), (gchar *) label->data[0].v_pointer);
		}
	}

	gtk_widget_destroy (dialog);
}

/**
 * glade_signal_editor_update_signal_view () will add a 'signal' to the list of 
 * signals displayed by the signal editor if 'iter' is NULL, otherwise it will
 * update the item at position 'iter' to the value of 'signal'
 */
static void 
glade_signal_editor_update_signal_view (GladeSignalEditor *editor,
					GladeSignal *signal,
					GtkTreeIter *iter)
{
	GValue *label;

	if (iter == NULL) {
		iter = g_new0 (GtkTreeIter, 1);
		gtk_tree_store_append (editor->model, iter, NULL);
	}

	label = g_new0 (GValue, 1);
	label = g_value_init (label, G_TYPE_STRING);

	label->data[0].v_pointer = g_strdup (signal->name);
	gtk_tree_store_set_value (editor->model, iter, 0, label);

	label->data[0].v_pointer = g_strdup (signal->handler);
	gtk_tree_store_set_value (editor->model, iter, 1, label);

	if (signal->after == TRUE)
		label->data[0].v_pointer = _("Yes");
	else
		label->data[0].v_pointer = _("No");
	gtk_tree_store_set_value (editor->model, iter, 2, label);
}

static gboolean
glade_signal_editor_list_select_cb (GtkTreeSelection *lst_select, 
				    GtkTreeModel *lst_model,
				    GtkTreePath *lst_path,
				    gboolean path_selected,
				    GladeSignalEditor *editor)
{
	GtkTreeIter iter;
	GValue *label;
	
	gtk_tree_model_get_iter (lst_model, &iter, lst_path);

	label = g_new0 (GValue, 1);
	gtk_tree_model_get_value (lst_model, &iter, 0, label);
	gtk_entry_set_text (GTK_ENTRY (editor->signal_name_entry), label->data[0].v_pointer);

	label = g_new0 (GValue, 1);
	gtk_tree_model_get_value (lst_model, &iter, 1, label);
	gtk_entry_set_text (GTK_ENTRY (editor->signal_handler_entry), label->data[0].v_pointer);

	label = g_new0 (GValue, 1);
	gtk_tree_model_get_value (lst_model, &iter, 2, label);
	if (!strcmp (_("Yes"), label->data[0].v_pointer)) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (editor->signal_after_button), TRUE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (editor->signal_after_button), FALSE);
	}

	return TRUE;
}

static void
glade_signal_editor_after_cb (GtkToggleButton *button, GladeSignalEditor *editor)
{
	if (gtk_toggle_button_get_active (button)) {
		g_object_set (G_OBJECT (button), "label", _("Yes"), NULL);
	} else {
		g_object_set (G_OBJECT (button), "label", _("No"), NULL);
	}
}

static GladeSignal * 
glade_signal_editor_validate_entries (GladeSignalEditor *editor)
{
	GtkWidget *dialog;
	const gchar *name;
	const gchar *handler;
	gboolean after;
	guint sig_id;
	GladeSignal *signal;

	name = gtk_entry_get_text (GTK_ENTRY (editor->signal_name_entry));

	/* check that signal exists */
	sig_id = g_signal_lookup (name, editor->widget->class->type);
	if (sig_id == 0) {
		dialog = gtk_message_dialog_new (NULL, 
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, 
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_CLOSE,
				_("Please enter a valid signal name"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return NULL;
	}

	handler = gtk_entry_get_text (GTK_ENTRY (editor->signal_handler_entry));

	/* check hadler is not empty */
	if (!strcmp (handler, "")) {
		dialog = gtk_message_dialog_new (NULL, 
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, 
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_CLOSE,
				_("Please enter a signal handler"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return NULL;
	}

	after = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (editor->signal_after_button));

	signal = glade_signal_new (name, handler, after);

	return signal;

}

static void 
glade_signal_editor_clear_entries (GladeSignalEditor *editor)
{
	g_return_if_fail (editor != NULL);

	gtk_entry_set_text (GTK_ENTRY (editor->signal_name_entry), "");
	gtk_entry_set_text (GTK_ENTRY (editor->signal_handler_entry), "");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (editor->signal_after_button), FALSE);
}

/**
 * Note that this function returns a newly allocated GladeSignal,
 * which the caller shuold take care of freeing.
 */
static GladeSignal *
glade_signal_editor_get_signal_at_iter (GladeSignalEditor *editor,
					GtkTreeIter *iter)
{
	GladeSignal *signal;
	gchar *name;
	gchar *handler;
	gchar *after_label;
	gboolean after;

	gtk_tree_model_get (GTK_TREE_MODEL (editor->model), iter,
			    0, &name, 1, &handler, 2, &after_label, -1);

	if (!strcmp (after_label, _("Yes")))
		after = TRUE;
	else
		after = FALSE;

	signal = glade_signal_new (name, handler, after);

	g_free (name);
	g_free (handler);
	g_free (after_label);

	return signal;
}

static void
glade_signal_editor_add_cb (GtkButton *button, GladeSignalEditor *editor)
{
	GladeSignal *signal;

	g_return_if_fail (GLADE_IS_SIGNAL_EDITOR (editor));
	g_return_if_fail (GLADE_IS_WIDGET (editor->widget));

	signal = glade_signal_editor_validate_entries (editor);
	if (!signal)
		return;

	glade_widget_add_signal (editor->widget, signal);
	glade_editor_add_signal (editor->editor, g_signal_lookup (signal->name,
				 editor->widget->class->type), signal->handler);

	glade_signal_editor_update_signal_view (editor, signal, NULL);
	glade_signal_editor_clear_entries (editor);
}

static void
glade_signal_editor_update_cb (GtkButton *button, GladeSignalEditor *editor)
{
	GtkTreeIter iter;
	GtkTreeSelection *select;
	GtkTreeModel *model;
	GladeSignal *updated_sig;

	g_return_if_fail (GLADE_IS_SIGNAL_EDITOR (editor));
	g_return_if_fail (GLADE_IS_WIDGET (editor->widget));

	updated_sig = glade_signal_editor_validate_entries (editor);
	if (!updated_sig)
		return;

	model = GTK_TREE_MODEL (editor->model);
	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->signals_list));

	if (gtk_tree_selection_get_selected (select, &model, &iter) == TRUE) {
		GladeSignal *signal;
		GList *found;

		signal = glade_signal_editor_get_signal_at_iter (editor, &iter);
		found = glade_widget_find_signal (editor->widget, signal);
		glade_signal_free (signal);

		if (found) {
			glade_signal_free (GLADE_SIGNAL (found->data));
			found->data = updated_sig;
			glade_signal_editor_update_signal_view (editor, updated_sig, &iter);
		}
	}
}

static void
glade_signal_editor_remove_cb (GtkButton *button, GladeSignalEditor *editor)
{
	GtkTreeIter iter;
	GtkTreeSelection *select;
	GtkTreeModel *model;
	GladeSignal *signal;

	g_return_if_fail (GLADE_IS_SIGNAL_EDITOR (editor));
	g_return_if_fail (GLADE_IS_WIDGET (editor->widget));

	model = GTK_TREE_MODEL (editor->model);
	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->signals_list));

	if (gtk_tree_selection_get_selected (select, &model, &iter) == TRUE) {
		signal = glade_signal_editor_get_signal_at_iter (editor, &iter);

		glade_widget_remove_signal (editor->widget, signal);

		gtk_tree_store_remove (GTK_TREE_STORE (editor->model), &iter);
		glade_signal_editor_clear_entries (editor);
	}
}

static void
glade_signal_editor_clear_cb (GtkButton *button, GladeSignalEditor *editor)
{
	g_return_if_fail (GLADE_IS_SIGNAL_EDITOR (editor));

	glade_signal_editor_clear_entries (editor);
}

static GtkWidget *
glade_signal_editor_construct_signals_list (GladeSignalEditor *editor)
{
	GtkTreeSelection *selection;
	GtkTreeView *view;
	GtkWidget *view_widget;
	
	editor->model = gtk_tree_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	view_widget = gtk_tree_view_new_with_model (GTK_TREE_MODEL (editor->model));
	view = GTK_TREE_VIEW (view_widget);

	/* the view now holds a reference, we can get rid of our own */
	g_object_unref (G_OBJECT (editor->model));

	glade_signal_editor_append_column (view, 0, _("Signal"));
	glade_signal_editor_append_column (view, 1, _("Handler"));
	glade_signal_editor_append_column (view, 2, _("After"));

	selection = gtk_tree_view_get_selection (view);
	gtk_tree_selection_set_select_function (selection,
						(GtkTreeSelectionFunc) glade_signal_editor_list_select_cb,
						editor, NULL);
	
	return view_widget;
}

static void
glade_signal_editor_table_append_label (GtkWidget *table, const gchar *name, gint pos)
{
	GtkWidget *label;

	label = gtk_label_new (name);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, pos, pos + 1);
}

static GtkWidget *
glade_signal_editor_table_append_entry (GtkWidget *table,
					GtkWidget *hbox,
					gint pos)
{
	GtkWidget *entry;

	entry  = gtk_entry_new ();
	gtk_box_pack_start_defaults (GTK_BOX (hbox), entry);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, pos, pos + 1);

	return entry;
}

static GtkWidget *
glade_signal_editor_construct_table (GladeSignalEditor *editor)
{
	GtkWidget *table;
	GtkWidget *hbox;
	GtkWidget *button;
	
	table = gtk_table_new (3, 2, FALSE);

	glade_signal_editor_table_append_label (table, _("Signal :"), 0);
	glade_signal_editor_table_append_label (table, _("Handler :"), 1);
	glade_signal_editor_table_append_label (table, _("After :"), 2);

	hbox = gtk_hbox_new (FALSE, 0);
	editor->signal_name_entry =
		glade_signal_editor_table_append_entry (table, hbox, 0);

	/* The "..." button */
	button = gtk_button_new_with_label ("...");
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_signal_editor_dialog_cb), editor);
	gtk_box_pack_start_defaults (GTK_BOX (hbox), button);

	hbox = gtk_hbox_new (FALSE, 0);
	editor->signal_handler_entry = 
		glade_signal_editor_table_append_entry (table, hbox, 1);

	/* The Yes/No button */
	hbox = gtk_hbox_new (FALSE, 0);
	button = gtk_toggle_button_new_with_label (_("No"));
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (glade_signal_editor_after_cb), editor);
	gtk_box_pack_start_defaults (GTK_BOX (hbox), button);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 2, 3);
	editor->signal_after_button =  button;
	
	return table;
}

static GtkWidget *
glade_signal_editor_construct_buttons (GladeSignalEditor *editor)
{
	GtkWidget *hbuttonbox;

	hbuttonbox = gtk_hbutton_box_new ();

	editor->add_button    = gtk_button_new_from_stock (GTK_STOCK_ADD);
	editor->update_button = gtk_button_new_with_mnemonic (_("_Update"));
	editor->remove_button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
	editor->clear_button  = gtk_button_new_from_stock (GTK_STOCK_CLEAR);

	g_signal_connect (G_OBJECT (editor->add_button), "clicked",
			  G_CALLBACK (glade_signal_editor_add_cb), editor);
	g_signal_connect (G_OBJECT (editor->update_button), "clicked",
			  G_CALLBACK (glade_signal_editor_update_cb), editor);
	g_signal_connect (G_OBJECT (editor->remove_button), "clicked",
			  G_CALLBACK (glade_signal_editor_remove_cb), editor);
	g_signal_connect (G_OBJECT (editor->clear_button), "clicked",
			  G_CALLBACK (glade_signal_editor_clear_cb), editor);

	gtk_container_add (GTK_CONTAINER (hbuttonbox), editor->add_button);
	gtk_container_add (GTK_CONTAINER (hbuttonbox), editor->update_button);
	gtk_container_add (GTK_CONTAINER (hbuttonbox), editor->remove_button);
	gtk_container_add (GTK_CONTAINER (hbuttonbox), editor->clear_button);

	return hbuttonbox;
}

static void
glade_signal_editor_construct (GladeSignalEditor *editor)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *scroll;
	GtkWidget *table;

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

	table = glade_signal_editor_construct_table (editor);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);

	hbox = glade_signal_editor_construct_buttons (editor);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

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

	g_return_if_fail (GLADE_IS_SIGNAL_EDITOR (editor));
	g_return_if_fail (widget == NULL || GLADE_IS_WIDGET (widget));

	gtk_tree_store_clear (editor->model);

	editor->widget = widget;
	editor->class = widget ? widget->class : NULL;

	glade_signal_editor_clear_entries (editor);

	if (!widget)
		return;

	for (list = widget->signals; list; list = list->next) {
		GladeSignal *signal;

		signal = list->data;
		glade_signal_editor_update_signal_view (editor, signal, NULL);
	}
}

