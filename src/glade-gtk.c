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
 *   Chema Celorio <chema@celorio.com>
 */

#include <string.h>
#include <stdlib.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-placeholder.h"
#include "glade-project.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-choice.h"
#include "glade-command.h"


void
glade_gtk_entry_set_text (GObject *object, GValue *value)
{
	GtkEditable *editable = GTK_EDITABLE (object);
	gint pos;
	gint insert_pos = 0;
	const gchar *text = g_value_get_string (value);

	g_return_if_fail (GTK_IS_EDITABLE (object));

	pos = gtk_editable_get_position (editable);
	gtk_editable_delete_text (editable, 0, -1);
	/* FIXME: will not work with multibyte languages (strlen) */
	gtk_editable_insert_text (editable,
				  text,
				  strlen (text),
				  &insert_pos);
	gtk_editable_set_position (editable, pos);
}

void
glade_gtk_entry_get_text (GObject *object, GValue *value)
{
	GtkEntry *entry = GTK_ENTRY (object);
						     
	const gchar *text;

	g_return_if_fail (GTK_IS_ENTRY (entry));

	text = gtk_entry_get_text (entry);

	g_value_set_string (value, text);
}

void
glade_gtk_option_menu_set_items (GObject *object, GValue *value)
{
	GtkOptionMenu *option_menu; 
	GtkWidget *menu;
	const gchar *items;
	const gchar *pos;
	const gchar *items_end;

	items = g_value_get_string (value);
	pos = items;
	items_end = &items[strlen (items)];
	
	option_menu = GTK_OPTION_MENU (object);
	g_return_if_fail (GTK_IS_OPTION_MENU (option_menu));
	menu = gtk_option_menu_get_menu (option_menu);
	if (menu != NULL)
		gtk_option_menu_remove_menu (option_menu);
	
	menu = gtk_menu_new ();
	
	while (pos < items_end) {
		GtkWidget *menu_item;
		gchar *item_end = strchr (pos, '\n');
		if (item_end == NULL)
			item_end = (gchar *) items_end;
		*item_end = '\0';
		
		menu_item = gtk_menu_item_new_with_label (pos);
		gtk_widget_show (menu_item);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
		
		pos = item_end + 1;
	}
	
	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
}


void
glade_gtk_progress_bar_set_format (GObject *object, GValue *value)
{
	GtkProgressBar *bar;
	const gchar *format = g_value_get_string (value);

	bar = GTK_PROGRESS_BAR (object);
	g_return_if_fail (GTK_IS_PROGRESS_BAR (bar));
	
	gtk_progress_set_format_string (GTK_PROGRESS (bar), format);
}

void
glade_gtk_spin_button_set_max (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (object)));

	adjustment->upper = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}


void
glade_gtk_spin_button_set_min (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (object)));

	adjustment->lower = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}

void
glade_gtk_spin_button_set_step_increment (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (object)));

	adjustment->step_increment = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}

void
glade_gtk_spin_button_set_page_increment (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (object)));

	adjustment->page_increment = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}

void
glade_gtk_spin_button_set_page_size (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (object)));

	adjustment->page_size = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}


void
glade_gtk_box_get_size (GObject *object, GValue *value)
{
	GtkBox *box;

	g_value_reset (value);

	box = GTK_BOX (object);
	g_return_if_fail (GTK_IS_BOX (box));
	
	g_value_set_int (value, g_list_length (box->children));
}

void
glade_gtk_box_set_size (GObject *object, GValue *value)
{
	GladeWidget *widget;
	GtkBox *box;
	gint new_size;
	gint old_size;

	box = GTK_BOX (object);
	g_return_if_fail (GTK_IS_BOX (box));

	widget = glade_widget_get_from_gtk_widget (GTK_WIDGET (box));
	g_return_if_fail (widget != NULL);

	old_size = g_list_length (box->children);
	new_size = g_value_get_int (value);

	if (new_size == old_size)
		return;

	if (new_size > old_size) {
		/* The box has grown. Add placeholders */
		while (new_size > old_size) {
			GladePlaceholder *placeholder = glade_placeholder_new ();
			gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (placeholder));
			old_size++;
		}
	} else {/* new_size < old_size */
		/* The box has shrunk. Remove the widgets that are on those slots */
		GList *child = g_list_last (box->children);

		while (child && old_size > new_size) {
			GtkWidget *child_widget = ((GtkBoxChild *) (child->data))->widget;
			GladeWidget *glade_widget;

			glade_widget = glade_widget_get_from_gtk_widget (child_widget);
			if (glade_widget) /* it may be NULL, e.g a placeholder */
				glade_project_remove_widget (glade_widget->project, child_widget);

			gtk_container_remove (GTK_CONTAINER (box), child_widget);

			child = g_list_last (box->children);
			old_size--;
		}
	}

	g_object_set_data (object, "glade_nb_placeholders", GINT_TO_POINTER (new_size));
}

void
glade_gtk_notebook_get_n_pages (GObject *object, GValue *value)
{
	GtkNotebook *notebook;

	g_value_reset (value);

	notebook = GTK_NOTEBOOK (object);
	g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

	g_value_set_int (value, g_list_length (notebook->children));
}

void
glade_gtk_notebook_set_n_pages (GObject *object, GValue *value)
{
	GladeWidget *widget;
	GtkNotebook *notebook;
	gint new_size;
	gint old_size;

	notebook = GTK_NOTEBOOK (object);
	g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

	widget = glade_widget_get_from_gtk_widget (GTK_WIDGET (notebook));
	g_return_if_fail (widget != NULL);

	old_size = g_list_length (notebook->children);
	new_size = g_value_get_int (value);

	if (new_size == old_size)
		return;

	if (new_size > old_size) {
		/* The notebook has grown. Add a page. */
		while (new_size > old_size) {
			GladePlaceholder *placeholder = glade_placeholder_new ();
			gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
						  GTK_WIDGET (placeholder),
						  NULL);
			old_size++;
		}
	} else {/* new_size < old_size */
		/* The notebook has shrunk. Remove pages  */
		GladeWidget *child_gwidget;
		GtkWidget *child_widget;

		/*
		 * Thing to remember is that GtkNotebook starts the
		 * page numbers from 0, not 1 (C-style). So we need to do
		 * old_size-1, where we're referring to "nth" widget.
		 */
		while (old_size > new_size) {
			/* Get the last widget. */
			child_widget = gtk_notebook_get_nth_page (notebook, old_size-1);
			child_gwidget = glade_widget_get_from_gtk_widget (child_widget);

			/* 
			 * If we got it, and its not a placeholder, remove it
			 * from project.
			 */
			if (child_gwidget)
				glade_project_remove_widget (child_gwidget->project, child_widget);

			gtk_notebook_remove_page (notebook, old_size-1);
			old_size--;
		}
	}
}

#if 0
/* This code is working but i don't think we need it. Chema */
void
glade_gtk_table_get_n_rows (GObject *object, GValue *value)
{
	GtkTable *table;

	g_value_reset (value);

	table = GTK_TABLE (object);
	g_return_if_fail (GTK_IS_TABLE (table));

	g_value_set_int (value, table->nrows);
}

void
glade_gtk_table_get_n_columns (GObject *object, GValue *value)
{
	GtkTable *table;

	g_value_reset (value);

	table = GTK_TABLE (object);
	g_return_if_fail (GTK_IS_TABLE (table));

	g_value_set_int (value, table->ncols);
}
#endif

void
glade_gtk_table_set_n_common (GObject *object, GValue *value, gboolean for_rows)
{
	GladeWidget *widget;
	GtkTable *table;
	int new_size;
	int old_size;
	int i, j;

	table = GTK_TABLE (object);
	g_return_if_fail (GTK_IS_TABLE (table));

	new_size = g_value_get_int (value);
	old_size = for_rows ? table->nrows : table->ncols;

	if (new_size == old_size)
		return;
	if (new_size < 1)
		return;

	widget = glade_widget_get_from_gtk_widget (GTK_WIDGET (table));
	g_return_if_fail (widget != NULL);

	if (new_size > old_size) {
		if (for_rows) {
			gtk_table_resize (table, new_size, table->ncols);

			for (i = 0; i < table->ncols; i++)
				for (j = old_size; j < table->nrows; j++)
					gtk_table_attach_defaults (table, glade_placeholder_new (widget),
								   i, i + 1, j, j + 1);
		} else {
			gtk_table_resize (table, table->nrows, new_size);

			for (i = old_size; i < table->ncols; i++)
				for (j = 0; j < table->nrows; j++)
					gtk_table_attach_defaults (table, glade_placeholder_new (widget),
								   i, i + 1, j, j + 1);
		}
	} else {
		/* Remove from the bottom up */
		GList *list = g_list_reverse (g_list_copy (gtk_container_get_children (GTK_CONTAINER (table))));
		GList *freeme = list;
		for (; list; list = list->next) {
			GtkTableChild *child = list->data;
			gint start = for_rows ? child->top_attach : child->left_attach;
			gint end = for_rows ? child->bottom_attach : child->right_attach;

			/* We need to completely remove it */
			if (start >= new_size) {
				gtk_container_remove (GTK_CONTAINER (table),
						      child->widget);
				continue;
			}

			/* If the widget spans beyond the new border, we should resize it to fit on the new table */
			if (end > new_size)
				gtk_container_child_set (GTK_CONTAINER (table), GTK_WIDGET (child),
							 for_rows ? "bottom_attach" : "right_attach",
							 new_size, NULL);
			
		}
		g_list_free (freeme);
		gtk_table_resize (table,
				  for_rows ? new_size : table->nrows,
				  for_rows ? table->ncols : new_size);
	}

	g_object_set_data (object, "glade_nb_placeholders", GINT_TO_POINTER (new_size * (for_rows ? table->ncols : table->nrows)));
}

void
glade_gtk_table_set_n_rows (GObject *object, GValue *value)
{
	glade_gtk_table_set_n_common (object, value, TRUE);
}

void
glade_gtk_table_set_n_columns (GObject *object, GValue *value)
{
	glade_gtk_table_set_n_common (object, value, FALSE);
}

void
glade_gtk_button_set_stock (GObject *object, GValue *value)
{
	GladeWidget *glade_widget;
	GtkWidget *button;
	GtkStockItem item;
	GladeChoice *choice = NULL;
	GladeProperty *property;
	GladeProperty *text;
	GList *list;
	gint val;

	val = g_value_get_enum (value);	

	button = GTK_WIDGET (object);
	g_return_if_fail (GTK_IS_BUTTON (button));
	glade_widget = glade_widget_get_from_gtk_widget (button);
	g_return_if_fail (glade_widget != NULL);

	property = glade_property_get_from_id (glade_widget->properties,
					       "stock");
	text = glade_property_get_from_id (glade_widget->properties,
					   "label");
	
	g_return_if_fail (property != NULL);
	g_return_if_fail (text != NULL);

	list = property->class->choices;
	for (; list; list = list->next) {
		choice = list->data;
		if (val == choice->value)
			break;
	}
	g_return_if_fail (list != NULL);

	gtk_container_remove (GTK_CONTAINER (button),
			      GTK_BIN (button)->child);
	
	if (!gtk_stock_lookup (choice->id, &item))
	{
		GtkWidget *label;
		
		label = gtk_label_new (g_value_get_string (text->value));
		gtk_container_add (GTK_CONTAINER (button), label);
		gtk_widget_show_all (button);
	} else {
		GtkWidget *label;
		GtkWidget *image;
		GtkWidget *hbox;

		hbox = gtk_hbox_new (FALSE, 1);
		label = gtk_label_new_with_mnemonic (item.label);
		image = gtk_image_new_from_stock (choice->id,
						  GTK_ICON_SIZE_BUTTON);

		gtk_label_set_mnemonic_widget (GTK_LABEL (label),
					       button);

		gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
		gtk_box_pack_end (GTK_BOX (hbox), label, TRUE, TRUE, 0);
		gtk_container_add (GTK_CONTAINER (button), hbox);
		
		gtk_widget_show_all (button);
	}
}

void
empty (GObject *object, GValue *value)
{
}

void
ignore (GObject *object, GValue *value)
{
}


/* ------------------------------------ Post Create functions ------------------------------ */
void
glade_gtk_window_post_create (GObject *object, GValue *not_used)
{
	GtkWindow *window = GTK_WINDOW (object);

	g_return_if_fail (GTK_IS_WINDOW (window));

	gtk_window_set_default_size (window, 440, 250);
}

void
glade_gtk_dialog_post_create (GObject *object, GValue *not_used)
{
	GtkDialog *dialog = GTK_DIALOG (object);

	g_return_if_fail (GTK_IS_DIALOG (dialog));

	gtk_window_set_default_size (GTK_WINDOW (dialog), 320, 260);
}

void
glade_gtk_message_dialog_post_create (GObject *object, GValue *not_used)
{
	GtkMessageDialog *dialog = GTK_MESSAGE_DIALOG (object);

	g_return_if_fail (GTK_IS_MESSAGE_DIALOG (dialog));

	gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 115);
}

void
glade_gtk_check_button_post_create (GObject *object, GValue *not_used)
{
	GtkCheckButton *button = GTK_CHECK_BUTTON (object);
	GtkWidget *label;

	g_return_if_fail (GTK_IS_CHECK_BUTTON (button));

	label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_container_add (GTK_CONTAINER (button), label);
	gtk_widget_show (label);

}

void
glade_gtk_table_post_create (GObject *object, GValue *value)
{
	GtkTable *table = GTK_TABLE (object);
	GList *list;

	g_return_if_fail (GTK_IS_TABLE (table));
	list = table->children;

	/* When we create a table and we are loading from disk a glade
	 * file, we need to add a placeholder because the size can't be
	 * 0 so gtk+ adds a widget that we don't want there.
	 * GtkBox is does not have this problem because
	 * a size of 0x0 is the default one. Check if the size is 0
	 * and that we don't have a children.
	 */
	if (g_list_length (list) == 0) {
		GladeWidget *parent = glade_widget_get_from_gtk_widget (GTK_WIDGET (table));
		gtk_container_add (GTK_CONTAINER (table),
				   glade_placeholder_new (parent));
	}
}

