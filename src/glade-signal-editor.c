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
#include "glade-signal-editor.h"


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
					   GtkTreeModel *lst_model, GtkTreePath *lst_path, GladeSignalEditor *editor)
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
		GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);

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
					  const gchar *label_text, GtkTreeIter *parent)
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

	g_return_if_fail (editor->class->signals != NULL);

	dialog = glade_signal_editor_dialog_construct (editor, &view);
	glade_signal_editor_dialog_load_signals (editor, view);

	switch (gtk_dialog_run (GTK_DIALOG (dialog))) {
  		case GTK_RESPONSE_ACCEPT:
			lst_model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
			lst_selected = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  			iter = g_new0 (GtkTreeIter, 1);
  			if (gtk_tree_selection_get_selected (lst_selected, &lst_model, iter)) {
  				label = g_new0 (GValue, 1);
				gtk_tree_model_get_value (lst_model, iter, 0, label);
  				gtk_entry_set_text (GTK_ENTRY (editor->signal_name_entry), (gchar *) label->data[0].v_pointer);
  			}
  			break;
  		default:
			break;
	}

	gtk_widget_destroy (dialog);
}

/* glade_signal_editor_update_signal () will add a 'signal' to the list of 
 * signals displayed by the signal editor if 'iter' is NULL, otherwise it will
 * update the item at position 'iter' to the value of 'signal'
 */
static void 
glade_signal_editor_update_signal (GladeSignalEditor *editor, GladeWidgetSignal *signal,
		GtkTreeIter *iter)
{
	GtkTreeStore *lst_model;
	GValue *label;

	lst_model = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (editor->signals_list)));

	if (iter == NULL) {
		iter = g_new0 (GtkTreeIter, 1);
		gtk_tree_store_append (lst_model, iter, NULL);
	}

	label = g_new0 (GValue, 1);
	label = g_value_init (label, G_TYPE_STRING);

	label->data[0].v_pointer = g_strdup (signal->name);
	gtk_tree_store_set_value (lst_model, iter, 0, label);

	label->data[0].v_pointer = g_strdup (signal->handler);
	gtk_tree_store_set_value (lst_model, iter, 1, label);

	if (signal->after == TRUE)
		label->data[0].v_pointer = _("Yes");
	else
		label->data[0].v_pointer = _("No");
	gtk_tree_store_set_value (lst_model, iter, 2, label);
}

static gboolean
glade_signal_editor_list_select_cb (GtkTreeSelection *lst_select, 
		GtkTreeModel *lst_model, GtkTreePath *lst_path, GladeSignalEditor *editor)
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

static gboolean 
glade_signal_editor_validate_entries (GladeSignalEditor *editor)
{
	GtkWidget *dialog;
	gchar *name_text;
	gchar *handler_text;
	GList *list;
	gboolean found;
	GladeWidgetClassSignal *signal;

	name_text = (gchar *) gtk_entry_get_text (GTK_ENTRY (editor->signal_name_entry));
	if (!strcmp (name_text, "")) {
		/* FIXME: Should fix to use Gnome APIs or a glade dialog */
		dialog = gtk_message_dialog_new (NULL, 
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, 
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_CLOSE,
				_("Please enter a valid signal name"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return FALSE;
	}

	/* check that signal exists */
	found = FALSE;
	for (list = editor->class->signals; list != NULL; list = list->next) {
		signal = (GladeWidgetClassSignal *) list->data;
		if (!strcmp (signal->name, name_text)) {
			found = TRUE;
			break;
		}
	}
	if (found == FALSE) {
		/* FIXME: Should fix to use Gnome APIs or a glade dialog */
		dialog = gtk_message_dialog_new (NULL, 
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, 
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_CLOSE,
				_("Please enter a valid signal name"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return FALSE;
	}

	handler_text = (gchar *) gtk_entry_get_text (GTK_ENTRY (editor->signal_handler_entry));
	if (!strcmp (handler_text, "")) {
		/* FIXME: Should fix to use Gnome APIs or a glade dialog */
		dialog = gtk_message_dialog_new (NULL, 
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, 
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_CLOSE,
				_("Please enter a signal handler"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return FALSE;
	}

	return TRUE;
}

static void 
glade_signal_editor_clear_entries (GladeSignalEditor *editor)
{
	g_return_if_fail (editor != NULL);

	gtk_entry_set_text (GTK_ENTRY (editor->signal_name_entry), "");
	gtk_entry_set_text (GTK_ENTRY (editor->signal_handler_entry), "");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
			(editor->signal_after_button), FALSE);
}

static GladeWidgetSignal *
glade_signal_editor_get_signal_at_iter (GladeSignalEditor *editor, GtkTreeIter *iter)
{
	GladeWidgetSignal *signal;
	GtkTreeModel *model;
	GValue *label;

	signal = g_new0 (GladeWidgetSignal, 1);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (editor->signals_list));

	label = g_new0 (GValue, 1);
	gtk_tree_model_get_value (GTK_TREE_MODEL (model), iter, 0, label);
	signal->name = (gchar *) label->data[0].v_pointer;

	label = g_new0 (GValue, 1);
	gtk_tree_model_get_value (GTK_TREE_MODEL (model), iter, 1, label);
	signal->handler = (gchar *) label->data[0].v_pointer;

	label = g_new0 (GValue, 1);
	gtk_tree_model_get_value (GTK_TREE_MODEL (model), iter, 2, label);
	if (!strcmp ((gchar *) label->data[0].v_pointer, _("Yes")))
		signal->after = TRUE;
	else 
		signal->after = FALSE;

	return signal;
}

/* glade_signal_editor_update_widget_signal () is used to add signal to a 
 * widget's internal list of signals.  If old_signal is NULL, the signal
 * is appended to the list, however if a signal is supplied as old_signal,
 * the func will find that signal and update it rather than add a new signal
 */
static GladeWidgetSignal *
glade_signal_editor_update_widget_signal (GladeSignalEditor *editor, 
		GladeWidgetSignal *old_signal)
{
	GladeWidgetSignal *signal = NULL;
	GladeWidgetSignal *sigtmp;
	GList *list;

	if (old_signal == NULL) {
		signal = g_new0 (GladeWidgetSignal, 1);
		editor->widget->signals = g_list_append (editor->widget->signals, signal);
	} else {
		for (list = editor->widget->signals; list != NULL; list = list->next) {
			sigtmp = (GladeWidgetSignal *) list->data;
			if (!strcmp (sigtmp->name, old_signal->name) &&
					!strcmp (sigtmp->handler, old_signal->handler) &&
					sigtmp->after == old_signal->after) {
				signal = sigtmp;
				break;
			}
		}
	}

	g_return_val_if_fail (signal != NULL, NULL);

	signal->name = g_strdup ((gchar *) gtk_entry_get_text (GTK_ENTRY 
				(editor->signal_name_entry)));
	signal->handler = g_strdup ((gchar *) gtk_entry_get_text (GTK_ENTRY 
				(editor->signal_handler_entry)));
	signal->after = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON 
			(editor->signal_after_button));

	return signal;
}

static void
glade_signal_editor_add_cb (GladeSignalEditor *editor)
{
	GladeWidgetSignal *signal;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (editor->widget != NULL);

	if (!glade_signal_editor_validate_entries (editor))
		return;

	signal = glade_signal_editor_update_widget_signal (editor, NULL);
	glade_signal_editor_update_signal (editor, signal, NULL);

	glade_signal_editor_clear_entries (editor);
}

static void
glade_signal_editor_update_cb (GladeSignalEditor *editor)
{
	GtkTreeIter iter;
	GtkTreeSelection *select;
	GtkTreeModel *model;
	GladeWidgetSignal *old_sig;
	GladeWidgetSignal *new_sig;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (editor->widget != NULL);

	if (!glade_signal_editor_validate_entries (editor))
		return;

	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->signals_list));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (editor->signals_list));

	if (gtk_tree_selection_get_selected (select, &model, &iter) == TRUE) {
		old_sig = glade_signal_editor_get_signal_at_iter (editor, &iter);

		new_sig = glade_signal_editor_update_widget_signal (editor, old_sig);
		glade_signal_editor_update_signal (editor, new_sig, &iter);
	}
}

static void
glade_signal_editor_delete_cb (GladeSignalEditor *editor)
{
	GtkTreeIter iter;
	GtkTreeSelection *select;
	GtkTreeModel *model;
	GladeWidgetSignal *signal;
	GladeWidgetSignal *sig;
	GList *list;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (editor->widget != NULL);

	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->signals_list));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (editor->signals_list));

	if (gtk_tree_selection_get_selected (select, &model, &iter) == TRUE) {
		signal = glade_signal_editor_get_signal_at_iter (editor, &iter);

		gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

		list = editor->widget->signals;
		for ( ; list != NULL; list = list->next) {
			sig = (GladeWidgetSignal *) list->data;
			if (!strcmp (sig->name, signal->name) &&
					!strcmp (sig->handler, signal->handler) &&
					sig->after == signal->after) {
					editor->widget->signals = g_list_remove (editor->widget->signals, list->data);
					break;
			}
		}

		glade_signal_editor_clear_entries (editor);
	}
}

static void
glade_signal_editor_clear_cb (GladeSignalEditor *editor)
{

	g_return_if_fail (editor != NULL);

	glade_signal_editor_clear_entries (editor);
}

static GtkWidget *
glade_signal_editor_construct_signals_list (GladeSignalEditor *editor)
{
	GtkTreeSelection *selection;
	GtkTreeStore *model;
	GtkTreeView *view;
	GtkWidget *view_widget;
	
	model = gtk_tree_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	view_widget = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
	view = GTK_TREE_VIEW (view_widget);

	g_object_unref (G_OBJECT (model));

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
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (glade_signal_editor_dialog_cb),
			    editor);
	gtk_box_pack_start_defaults (GTK_BOX (hbox), button);

	hbox = gtk_hbox_new (FALSE, 0);
	editor->signal_handler_entry = 
		glade_signal_editor_table_append_entry (table, hbox, 1);

	/* The Yes/No button */
	hbox = gtk_hbox_new (FALSE, 0);
	button = gtk_toggle_button_new_with_label (_("No"));
	gtk_signal_connect (GTK_OBJECT (button), "toggled",
			    GTK_SIGNAL_FUNC (glade_signal_editor_after_cb),
			    editor);
	gtk_box_pack_start_defaults (GTK_BOX (hbox), button);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 2, 3);
	editor->signal_after_button =  button;
	
	return table;
}

static void
glade_signal_editor_clicked_cb (GtkWidget *button, GladeSignalEditor *editor)
{
	g_return_if_fail (GLADE_IS_SIGNAL_EDITOR (editor));

	if (button == editor->add_button)
		glade_signal_editor_add_cb (editor);
	else if (button == editor->update_button)
		glade_signal_editor_update_cb (editor);
	else if (button == editor->delete_button)
		glade_signal_editor_delete_cb (editor);
	else if (button == editor->clear_button)
		glade_signal_editor_clear_cb (editor);
}
		
static GtkWidget *
glade_signal_editor_append_button (GladeSignalEditor *editor,
				   GtkWidget *hbox,
				   const gchar *name)
{
	GtkWidget *button;

	button = gtk_button_new_with_label (name);
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    GTK_SIGNAL_FUNC (glade_signal_editor_clicked_cb),
			    editor);
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

	return button;
}

static GtkWidget *
glade_signal_editor_construct_buttons (GladeSignalEditor *editor)
{
	GtkWidget *hbox;

	hbox = gtk_hbox_new (FALSE, 0);

	editor->add_button    = glade_signal_editor_append_button (editor, hbox, _("Add"));
	editor->update_button = glade_signal_editor_append_button (editor, hbox, _("Update"));
	editor->delete_button = glade_signal_editor_append_button (editor, hbox, _("Delete"));
	editor->clear_button  = glade_signal_editor_append_button (editor, hbox, _("Clear"));

	return hbox;
}

void
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
	gtk_box_pack_start (GTK_BOX (vbox), scroll, TRUE, TRUE, 0);

	editor->signals_list = glade_signal_editor_construct_signals_list (editor);
	gtk_container_add (GTK_CONTAINER (scroll), editor->signals_list);

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

GladeSignalEditor*
glade_signal_editor_new (void)
{
	GladeSignalEditor *editor;

	editor = g_new0 (GladeSignalEditor, 1);

	glade_signal_editor_construct (editor);

	return editor;
}

void 
glade_signal_editor_load_widget (GladeSignalEditor *editor, GladeWidget *widget)
{
	GladeWidgetSignal *signal;
	GtkTreeStore *model;
	GList *list;

	model = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (editor->signals_list)));
	gtk_tree_store_clear (model);

	editor->widget = widget;
	editor->class = widget->class;

	glade_signal_editor_clear_entries (editor);
	
	list = widget->signals;
	for (; list != NULL; list = list->next) {
		signal = list->data;
		glade_signal_editor_update_signal (editor, signal, NULL);
	}
}

