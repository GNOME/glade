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
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-choice.h"

static gint
glade_widget_ugly_hack (gpointer data)
{
	GtkWidget *widget = data;
	
	gtk_widget_queue_resize (widget);
	
	return FALSE;
}

static void
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

static void
glade_gtk_entry_get_text (GObject *object, GValue *value)
{
	GtkEntry *entry = GTK_ENTRY (object);
						     
	const gchar *text;

	g_return_if_fail (GTK_IS_ENTRY (entry));

	text = gtk_entry_get_text (entry);

	g_value_set_string (value, text);
}

static void
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


static void
glade_gtk_progress_bar_set_format (GObject *object, GValue *value)
{
	GtkProgressBar *bar;
	const gchar *format = g_value_get_string (value);

	bar = GTK_PROGRESS_BAR (object);
	g_return_if_fail (GTK_IS_PROGRESS_BAR (bar));
	
	gtk_progress_set_format_string (GTK_PROGRESS (bar), format);
}

static void
glade_gtk_adjustment_set_max (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (object);

	adjustment->upper = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}


static void
glade_gtk_adjustment_set_min (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (object);

	adjustment->lower = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);

	g_print ("Set min !!\n");
}

static void
glade_gtk_adjustment_set_step_increment (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (object);

	adjustment->step_increment = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}

static void
glade_gtk_adjustment_set_page_increment (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (object);

	adjustment->page_increment = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}

static void
glade_gtk_adjustment_set_page_size (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (object);

	adjustment->page_size = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}


static void
glade_gtk_box_get_size (GObject *object, GValue *value)
{
	GtkBox *box;

	g_value_reset (value);

	box = GTK_BOX (object);
	g_return_if_fail (GTK_IS_BOX (box));
	
	g_value_set_int (value, g_list_length (box->children));
}

static void
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

	glade_placeholder_remove_all (GTK_WIDGET (box));

	if (new_size > old_size) {
		/* The box has grown */
		/* We don't need to do anything here because the box's property "size"
		 * is already updated :-? */

	} else if (new_size < old_size) {
		/* The box has shrunk. Remove the widgets that are on those slots */
		GList *child;
		int i = old_size;
		int position;
		child = g_list_last (box->children);
		while (child && i > new_size) {
			GtkBoxChild *box_child;
			GladeWidget *child_widget;
			GladeProperty *child_property;
			
			box_child = (GtkBoxChild *) child->data;
			child_widget = glade_widget_get_from_gtk_widget (box_child->widget);
			child_property = glade_property_get_from_id (child_widget->properties, "position");
			position = glade_property_get_integer (child_property);
			if (position >= new_size) {
				glade_widget_delete (child_widget);
				gtk_container_remove (GTK_CONTAINER (box),
						      box_child->widget);
			}
			i = position;
			child = g_list_last (box->children);
		}
	} /* else the size is == */
	
	glade_placeholder_fill_empty (GTK_WIDGET (box));

	/* see glade-widget.c#ugly_hack for an explanation */
	gtk_timeout_add ( 100, glade_widget_ugly_hack, box);
}

#if 0
/* This code is working but i don't think we need it. Chema */
static void
glade_gtk_table_get_n_rows (GObject *object, GValue *value)
{
	GtkTable *table;

	g_value_reset (value);

	table = GTK_TABLE (object);
	g_return_if_fail (GTK_IS_TABLE (table));

	g_value_set_int (value, table->nrows);
}

static void
glade_gtk_table_get_n_columns (GObject *object, GValue *value)
{
	GtkTable *table;

	g_value_reset (value);

	table = GTK_TABLE (object);
	g_return_if_fail (GTK_IS_TABLE (table));

	g_value_set_int (value, table->ncols);
}
#endif

static void
glade_gtk_table_set_n_common (GObject *object, GValue *value, gboolean for_rows)
{
	GladeWidget *widget;
	GtkTable *table;
	gint new_size;
	gint old_size;

	table = GTK_TABLE (object);
	g_return_if_fail (GTK_IS_TABLE (table));

	new_size = g_value_get_int (value);
	old_size = for_rows ? table->nrows : table->ncols;

	if (new_size == old_size)
		return;
	if (new_size < 1)
		return;

	glade_placeholder_remove_all (GTK_WIDGET (table));
	
	widget = glade_widget_get_from_gtk_widget (GTK_WIDGET (table));
	g_return_if_fail (widget != NULL);

	if (new_size > old_size) {
		gtk_table_resize (table,
				  for_rows ? new_size : table->nrows,
				  for_rows ? table->ncols : new_size);
	} else {
		/* Remove from the bottom up */
		GList *list = g_list_reverse (g_list_copy (table->children));
		GList *freeme = list;
		for (; list; list = list->next) {
			GtkTableChild *child = list->data;
			gint start = for_rows ?
				child->top_attach : child->left_attach;
			gint end = for_rows ?
				child->bottom_attach : child->right_attach;
			/* We need to completely remove it */
			if (start >= new_size) {
				gtk_container_remove (GTK_CONTAINER (table),
						      child->widget);
				continue;
			}
			/* We need to change the span */
			/* For we to code this, span has to be working */
			if (end > new_size)
				g_print ("Change the span <---------\n");
			
		}
		g_list_free (freeme);
		gtk_table_resize (table,
				  for_rows ? new_size : table->nrows,
				  for_rows ? table->ncols : new_size);
	}

	glade_placeholder_fill_empty (GTK_WIDGET (table));

	/* see glade-widget.c#ugly_hack for an explanation */
	gtk_timeout_add ( 100, glade_widget_ugly_hack, table);
}

static void
glade_gtk_table_set_n_rows (GObject *object, GValue *value)
{
	glade_gtk_table_set_n_common (object, value, TRUE);
}

static void
glade_gtk_table_set_n_columns (GObject *object, GValue *value)
{
	glade_gtk_table_set_n_common (object, value, FALSE);
}

static void
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

static void
empty (GObject *object, GValue *value)
{
}


/* ------------------------------------ Post Create functions ------------------------------ */
static void
glade_gtk_window_post_create (GObject *object, GValue *not_used)
{
	GtkWindow *window = GTK_WINDOW (object);

	g_return_if_fail (GTK_IS_WINDOW (window));

	gtk_window_set_default_size (window, 440, 250);
}

static void
glade_gtk_dialog_post_create (GObject *object, GValue *not_used)
{
	GtkDialog *dialog = GTK_DIALOG (object);

	g_return_if_fail (GTK_IS_DIALOG (dialog));

	gtk_window_set_default_size (GTK_WINDOW (dialog), 320, 260);
}

static void
glade_gtk_message_dialog_post_create (GObject *object, GValue *not_used)
{
	GtkMessageDialog *dialog = GTK_MESSAGE_DIALOG (object);

	g_return_if_fail (GTK_IS_MESSAGE_DIALOG (dialog));

	gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 115);
}

static void
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

static void
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


/* ================ Temp hack =================== */
/* We have this table, but what we should do is use gmodule for this,
 * however this requires that we link with libtool cause right now
 * we are loosing the symbols. Chema
 */
typedef struct _GladeGtkFunction GladeGtkFunction;

struct _GladeGtkFunction {
	const gchar *name;
	void (* function) (GObject *object, GValue *value);
};

GladeGtkFunction functions [] = {
	{"glade_gtk_entry_get_text",          glade_gtk_entry_get_text},
	{"glade_gtk_box_get_size",            glade_gtk_box_get_size},
	{"glade_gtk_widget_get_tooltip",      empty},

	{"glade_gtk_button_set_stock",        glade_gtk_button_set_stock},

#if 0	
	{"glade_gtk_table_get_n_rows",        glade_gtk_table_get_n_rows},
	{"glade_gtk_table_get_n_columns",     glade_gtk_table_get_n_columns},
#endif	
	{"glade_gtk_table_set_n_rows",        glade_gtk_table_set_n_rows},
	{"glade_gtk_table_set_n_columns",     glade_gtk_table_set_n_columns},

	{"glade_gtk_entry_set_text",          glade_gtk_entry_set_text},
	{"glade_gtk_option_menu_set_items",   glade_gtk_option_menu_set_items},
	{"glade_gtk_progress_bar_set_format", glade_gtk_progress_bar_set_format},
	{"glade_gtk_box_set_size",            glade_gtk_box_set_size},
	{"glade_gtk_widget_set_tooltip",      empty},
	{"ignore",                            empty}, /* For example for gtkwindow::modal, we want to ignore the set */

	{"glade_gtk_adjustment_set_max",            glade_gtk_adjustment_set_max},
	{"glade_gtk_adjustment_set_min",            glade_gtk_adjustment_set_min},
	{"glade_gtk_adjustment_set_step_increment", glade_gtk_adjustment_set_step_increment},
	{"glade_gtk_adjustment_set_page_increment", glade_gtk_adjustment_set_page_increment},
	{"glade_gtk_adjustment_set_page_size",      glade_gtk_adjustment_set_page_size},

	{"glade_gtk_check_button_post_create",      glade_gtk_check_button_post_create},
	{"glade_gtk_window_post_create",            glade_gtk_window_post_create},
	{"glade_gtk_dialog_post_create",            glade_gtk_dialog_post_create},
	{"glade_gtk_message_dialog_post_create",    glade_gtk_message_dialog_post_create},
	{"glade_gtk_table_post_create",             glade_gtk_table_post_create},
};

gpointer
glade_gtk_get_function (const gchar *name)
{
	gint num;
	gint i;

	num = sizeof (functions) / sizeof (GladeGtkFunction);
	for (i = 0; i < num; i++) {
		if (strcmp (name, functions[i].name) == 0)
			break;
	}
	if (i == num) {
		g_warning ("Could not find the function %s\n",
			   name);
		return NULL;
	}

	return functions[i].function;
}

gboolean
glade_gtk_get_set_function_hack (GladePropertyClass *class, const gchar *name)
{
	class->set_function = glade_gtk_get_function (name);

	return TRUE;
}

gboolean
glade_gtk_get_get_function_hack (GladePropertyClass *class, const gchar *name)
{
	class->get_function = glade_gtk_get_function (name);

	return TRUE;
}

