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


static gboolean
glade_signal_editor_dialog_list_cb (GtkTreeSelection *lst_select, 
				    GtkTreeModel *lst_model, GtkTreePath *lst_path, GList *list)
{
	GtkTreeIter iter;
	GValue *label;
	
	label = g_new0 (GValue, 1);
	gtk_tree_model_get_iter (lst_model, &iter, lst_path);
	gtk_tree_model_get_value (lst_model, &iter, 0, label);

	for ( ; list != NULL; list = list->next) {
		if (!strcmp (list->data, label->data[0].v_pointer)) {
			return FALSE;
		}
	}

	return TRUE;
}

static void
glade_signal_editor_dialog_cb (GtkButton *button, GladeSignalEditor *editor)
{
	GtkWidget *dialog;
	GtkWidget *vbox;
	GList *list;
	GladeWidgetClassSignal *signal;
	gchar *type;
	gint response;
	GtkTreeModel *lst_model;
	GtkTreeView *lst_view;
	GtkCellRenderer *lst_cellr;
	GtkTreeViewColumn *lst_column;
	GtkTreeSelection *lst_select;
	GValue *label;
	GtkTreeIter *parent_iter = NULL;
	GtkTreeIter *iter = NULL;
	GtkTreeSelection *lst_selected;
	GtkWidget *scroll;
	GList *widget_class_list = NULL;

	g_return_if_fail (editor->class->signals != NULL);

	dialog = gtk_dialog_new_with_buttons (_("Select signal"),
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


	lst_model = GTK_TREE_MODEL (gtk_tree_store_new_with_types (1, G_TYPE_STRING));

	lst_view = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL (lst_model)));
	gtk_widget_set_usize (GTK_WIDGET (lst_view), 150, 200);
	gtk_container_add (GTK_CONTAINER (scroll), GTK_WIDGET (lst_view));

	g_object_unref (G_OBJECT (lst_model));

	lst_cellr = gtk_cell_renderer_text_new ();
	lst_column = gtk_tree_view_column_new_with_attributes (_("Signals"), lst_cellr,
			"text", 0, NULL);
	gtk_tree_view_append_column (lst_view, lst_column);


	type = "";
	label = g_new0 (GValue, 1);
	label = g_value_init (label, G_TYPE_STRING);

	for (list = editor->class->signals; list != NULL; list = list->next) {
		signal = (GladeWidgetClassSignal *) list->data;

		if (!strcmp (type, signal->type)) {

			iter = g_new0 (GtkTreeIter, 1);
			gtk_tree_store_append (GTK_TREE_STORE (lst_model), iter, parent_iter);
			label->data[0].v_pointer = signal->name;
			gtk_tree_store_set_value (GTK_TREE_STORE (lst_model), iter, 0, label);
		} else {

			parent_iter = g_new0 (GtkTreeIter, 1);
			gtk_tree_store_append (GTK_TREE_STORE (lst_model), parent_iter, NULL);
			label->data[0].v_pointer = signal->type;
			gtk_tree_store_set_value (GTK_TREE_STORE (lst_model), parent_iter, 0, label);

			widget_class_list = g_list_append(widget_class_list, g_strdup (signal->type));

			iter = g_new0 (GtkTreeIter, 1);
			gtk_tree_store_append (GTK_TREE_STORE (lst_model), iter, parent_iter);
			label->data[0].v_pointer = signal->name;
			gtk_tree_store_set_value (GTK_TREE_STORE (lst_model), iter, 0, label);

			type = signal->type;
		}
	}

	lst_select = gtk_tree_view_get_selection (GTK_TREE_VIEW (lst_view));
	gtk_tree_selection_set_select_function (lst_select, (GtkTreeSelectionFunc) glade_signal_editor_dialog_list_cb,
			widget_class_list, NULL);

	gtk_widget_show_all (vbox);
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	switch (response) {
		case GTK_RESPONSE_ACCEPT:
			lst_selected = gtk_tree_view_get_selection (lst_view);
			iter = g_new0 (GtkTreeIter, 1);
			if (gtk_tree_selection_get_selected (lst_selected, &lst_model, iter)) {
				label = g_new0 (GValue, 1);
				gtk_tree_model_get_value (GTK_TREE_MODEL (lst_model), iter, 0, label);
				gtk_entry_set_text (GTK_ENTRY (editor->signals_name_entry), (gchar *) label->data[0].v_pointer);
			}
			break;

		default:
			break;
	}

	gtk_widget_destroy (dialog);
}

static void 
glade_signal_editor_append_signal (GladeSignalEditor *editor, GladeWidgetSignal *signal)
{
	GtkTreeStore *lst_model;
	GtkTreeIter *iter;
	GValue *label;

	lst_model = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (editor->signals_list)));
	iter = g_new0 (GtkTreeIter, 1);
	gtk_tree_store_append (lst_model, iter, NULL);

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
	gtk_entry_set_text (GTK_ENTRY (editor->signals_name_entry), label->data[0].v_pointer);

	label = g_new0 (GValue, 1);
	gtk_tree_model_get_value (lst_model, &iter, 1, label);
	gtk_entry_set_text (GTK_ENTRY (editor->signals_handler_entry), label->data[0].v_pointer);

	label = g_new0 (GValue, 1);
	gtk_tree_model_get_value (lst_model, &iter, 2, label);
	if (!strcmp (_("Yes"), label->data[0].v_pointer)) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (editor->signals_after_button), TRUE);
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (editor->signals_after_button), FALSE);
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

static void
glade_signal_editor_add_cb (GladeSignalEditor *editor)
{
	GladeWidgetSignal *signal;
	gchar *name_text;
	gchar *handler_text;
	GtkWidget *dialog;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (editor->widget != NULL);

	name_text = (gchar *) gtk_entry_get_text (GTK_ENTRY (editor->signals_name_entry));
	if (!strcmp (name_text, "")) {
		/* FIXME: Should fix to use Gnome APIs or a glade dialog */
		dialog = gtk_message_dialog_new (NULL, 
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, 
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_CLOSE,
				_("Please enter a signal name"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return;
	}

	handler_text = (gchar *) gtk_entry_get_text (GTK_ENTRY (editor->signals_handler_entry));
	if (!strcmp (handler_text, "")) {
		/* FIXME: Should fix to use Gnome APIs or a glade dialog */
		dialog = gtk_message_dialog_new (NULL, 
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, 
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_CLOSE,
				_("Please enter a signal handler"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return;
	}
	
	signal = g_new0 (GladeWidgetSignal, 1);
	signal->name = g_strdup (name_text);
	signal->handler = g_strdup (handler_text);
	signal->after = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON 
			(editor->signals_after_button));
	editor->widget->signals = g_list_append (editor->widget->signals, signal);
	
	glade_signal_editor_append_signal (editor, signal);

	gtk_entry_set_text (GTK_ENTRY (editor->signals_name_entry), "");
	gtk_entry_set_text (GTK_ENTRY (editor->signals_handler_entry), "");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
			(editor->signals_after_button), FALSE);
}

static void
glade_signal_editor_update_cb (GtkButton *button, GladeSignalEditor *editor)
{
	GtkTreeIter iter;
	GtkTreeSelection *select;
	GtkTreeModel *model;
	GladeWidgetSignal signal;
	GladeWidgetSignal *sig;
	GValue *label;
	GList *list;
	gchar *name_text;
	gchar *handler_text;
	GtkWidget *dialog;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (editor->widget != NULL);

	name_text = (gchar *) gtk_entry_get_text (GTK_ENTRY (editor->signals_name_entry));
	if (!strcmp (name_text, "")) {
		/* FIXME: Should fix to use Gnome APIs or a glade dialog */
		dialog = gtk_message_dialog_new (NULL, 
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, 
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_CLOSE,
				_("Please enter a signal name"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return;
	}

	handler_text = (gchar *) gtk_entry_get_text (GTK_ENTRY (editor->signals_handler_entry));
	if (!strcmp (handler_text, "")) {
		/* FIXME: Should fix to use Gnome APIs or a glade dialog */
		dialog = gtk_message_dialog_new (NULL, 
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, 
				GTK_MESSAGE_INFO,
				GTK_BUTTONS_CLOSE,
				_("Please enter a signal handler"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		return;
	}

	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->signals_list));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (editor->signals_list));

	if (gtk_tree_selection_get_selected (select, &model, &iter)) {
		label = g_new0 (GValue, 1);
		gtk_tree_model_get_value (GTK_TREE_MODEL (model), &iter, 0, label);
		signal.name = (gchar *) label->data[0].v_pointer;
		label = g_new0 (GValue, 1);
		gtk_tree_model_get_value (GTK_TREE_MODEL (model), &iter, 1, label);
		signal.handler = (gchar *) label->data[0].v_pointer;
		label = g_new0 (GValue, 1);
		gtk_tree_model_get_value (GTK_TREE_MODEL (model), &iter, 2, label);
		if (!strcmp ((gchar *) label->data[0].v_pointer, _("Yes")))
			signal.after = TRUE;
		else 
			signal.after = FALSE;

		label = g_new0 (GValue, 1);
		label = g_value_init (label, G_TYPE_STRING);
		label->data[0].v_pointer = name_text;
		gtk_tree_store_set_value (GTK_TREE_STORE (model), &iter, 0, label);
		label->data[0].v_pointer = handler_text;
		gtk_tree_store_set_value (GTK_TREE_STORE (model), &iter, 1, label);
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (editor->signals_after_button))) {
			label->data[0].v_pointer = _("Yes");
		} else {
			label->data[0].v_pointer = _("No");
		}
		gtk_tree_store_set_value (GTK_TREE_STORE (model), &iter, 2, label);

		list = editor->widget->signals;
		for ( ; list != NULL; list = list->next) {
			sig = (GladeWidgetSignal *) list->data;
			if (!strcmp (sig->name, signal.name) &&
					!strcmp (sig->handler, signal.handler) &&
					sig->after == signal.after) {
					sig->name = g_strdup (name_text);
					sig->handler = g_strdup (handler_text);
					sig->after = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (editor->signals_after_button));
					break;
			}
		}
	}
}

static void
glade_signal_editor_delete_cb (GtkButton *button, GladeSignalEditor *editor)
{
	GtkTreeIter iter;
	GtkTreeSelection *select;
	GtkTreeModel *model;
	GladeWidgetSignal signal;
	GladeWidgetSignal *sig;
	GValue *label;
	GList *list;

	g_return_if_fail (editor != NULL);
	g_return_if_fail (editor->widget != NULL);

	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (editor->signals_list));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (editor->signals_list));

	if (gtk_tree_selection_get_selected (select, &model, &iter)) {
		label = g_new0 (GValue, 1);
		gtk_tree_model_get_value (GTK_TREE_MODEL (model), &iter, 0, label);
		signal.name = (gchar *) label->data[0].v_pointer;
		label = g_new0 (GValue, 1);
		gtk_tree_model_get_value (GTK_TREE_MODEL (model), &iter, 1, label);
		signal.handler = (gchar *) label->data[0].v_pointer;
		label = g_new0 (GValue, 1);
		gtk_tree_model_get_value (GTK_TREE_MODEL (model), &iter, 2, label);
		if (!strcmp ((gchar *) label->data[0].v_pointer, _("Yes")))
			signal.after = TRUE;
		else 
			signal.after = FALSE;

		gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

		list = editor->widget->signals;
		for ( ; list != NULL; list = list->next) {
			sig = (GladeWidgetSignal *) list->data;
			if (!strcmp (sig->name, signal.name) &&
					!strcmp (sig->handler, signal.handler) &&
					sig->after == signal.after) {
					editor->widget->signals = g_list_remove (editor->widget->signals, list->data);
					break;
			}
		}
	}
}

static void
glade_signal_editor_clear_cb (GtkButton *button, GladeSignalEditor *editor)
{

	g_return_if_fail (editor != NULL);
	g_return_if_fail (editor->widget != NULL);

	gtk_entry_set_text (GTK_ENTRY (editor->signals_name_entry), "");
	gtk_entry_set_text (GTK_ENTRY (editor->signals_handler_entry), "");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
			(editor->signals_after_button), FALSE);
}

static void
glade_signal_editor_append_column (GtkTreeView *view, const gchar *name)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (name, renderer,
							   "text", 0, NULL);
	gtk_tree_view_append_column (view, column);
}

static GtkWidget *
glade_signal_editor_construct_signals_list (GladeSignalEditor *editor)
{
	GtkTreeSelection *selection;
	GtkTreeStore *model;
	GtkTreeView *view;
	GtkWidget *view_widget;
	
	model = gtk_tree_store_new_with_types (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	view_widget = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
	view = GTK_TREE_VIEW (view_widget);

	g_object_unref (G_OBJECT (model));

	glade_signal_editor_append_column (view, _("Signal"));
	glade_signal_editor_append_column (view, _("Handler"));
	glade_signal_editor_append_column (view, _("After"));

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
	gtk_table_attach_defaults (GTK_TABLE (label), 0, 1, pos, pos + 1);

}

static GtkWidget *
glade_signal_editor_table_append_entry (GtkWidget *table,
					GtkWidget *hbox,
					gint pos)
{
	GtkWidget *entry;

	entry  = gtk_entry_new ();
	gtk_box_pack_start_defaults (GTK_BOX (hbox), entry);
	gtk_table_attach_defaults (GTK_TABLE (entry), hbox, 1, 2, pos, pos + 1);

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
			    GTK_SIGNAL_FUNC (glade_signal_editor_dialog_cb), editor);
	gtk_box_pack_start_defaults (GTK_BOX (hbox), button);

	hbox = gtk_hbox_new (FALSE, 0);
	editor->signals_handler_entry = 
		glade_signal_editor_table_append_entry (table, hbox, 1);

}

static void
glade_signal_editor_clicked_cb (GtkWidget *button, GladeSignalEditor *editor)
{
	g_return_if_fail (GLADE_IS_SIGNAL_EDITOR (editor));

	if (button == editor->add_button)
		glade_signal_editor_add_cb 		
	else if (button == editor->update_button)
	else if (button == editor->delete_button)
	else if (button == editor->clear_button)

		
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
}

static void
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
	GtkWidget *widget;
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
	gtk_box_pack_start_defaults (GTK_BOX (container), table);

	hbox = glade_signal_editor_construct_buttons (editor);
	gtk_box_pack_start_defaults (GTK_BOX (container), hbox);

	gtk_widget_show_all (editor->main_window);
}


GtkWidget 
glade_signal_editor_get_widget (GladeSignalEditor *editor)
{
	g_return_val_if_fail (GLADE_IS_SIGNAL_EDITOR (editor), NULL);
	
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

static void
glade_signal_editor_set_contents (GladeSignalEditor *editor)
{
	gtk_entry_set_text (GTK_ENTRY (editor->signals_name_entry), "");
	gtk_entry_set_text (GTK_ENTRY (editor->signals_handler_entry), "");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
				      (editor->signals_after_button), FALSE);
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

	glade_signal_editor_set_contents (editor);
	
	list = widget->signals;
	for (; list != NULL; list = list->next) {
		signal = list->data;
		glade_signal_editor_append_signal (editor, signal);
	}
}

