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

#include <gtk/gtk.h>
#include "glade-plugin.h"

/* Borrow from libgnome/libgnome.h */
#ifdef ENABLE_NLS
#    include <libintl.h>
#    ifdef GNOME_EXPLICIT_TRANSLATION_DOMAIN
#        undef _
#        define _(String) dgettext (GNOME_EXPLICIT_TRANSLATION_DOMAIN, String)
#    else 
#        define _(String) gettext (String)
#    endif
#    ifdef gettext_noop
#        define N_(String) gettext_noop (String)
#    else
#        define N_(String) (String)
#    endif
#else
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#endif

#ifdef G_OS_WIN32
#define GLADEGTK_API __declspec(dllexport)
#else
#define GLADEGTK_API
#endif

void GLADEGTK_API
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

void GLADEGTK_API
glade_gtk_progress_bar_set_format (GObject *object, GValue *value)
{
	GtkProgressBar *bar;
	const gchar *format = g_value_get_string (value);

	bar = GTK_PROGRESS_BAR (object);
	g_return_if_fail (GTK_IS_PROGRESS_BAR (bar));
	
	gtk_progress_set_format_string (GTK_PROGRESS (bar), format);
}

void GLADEGTK_API
glade_gtk_spin_button_set_max (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (object)));

	adjustment->upper = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}


void GLADEGTK_API
glade_gtk_spin_button_set_min (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (object)));

	adjustment->lower = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}

void GLADEGTK_API
glade_gtk_spin_button_set_step_increment (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (object)));

	adjustment->step_increment = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}

void GLADEGTK_API
glade_gtk_spin_button_set_page_increment (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (object)));

	adjustment->page_increment = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}

void GLADEGTK_API
glade_gtk_spin_button_set_page_size (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (object)));

	adjustment->page_size = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}


void GLADEGTK_API
glade_gtk_box_get_size (GObject *object, GValue *value)
{
	GtkBox *box;

	g_value_reset (value);

	box = GTK_BOX (object);
	g_return_if_fail (GTK_IS_BOX (box));
	
	g_value_set_int (value, g_list_length (box->children));
}

void GLADEGTK_API
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

	if (new_size > old_size)
	{
		/* The box has grown. Add placeholders */
		while (new_size > old_size)
		{
			GladePlaceholder *placeholder = GLADE_PLACEHOLDER (glade_placeholder_new ());
			gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (placeholder));
			old_size++;
		}
	}
	else
	{
		/* new_size < old_size */
		/* The box has shrunk. Remove the widgets that are on those slots */
		GList *child = g_list_last (box->children);

		while (child && old_size > new_size)
		{
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

void GLADEGTK_API
glade_gtk_toolbar_get_size (GObject *object, GValue *value)
{
	GtkToolbar *toolbar;

	g_return_if_fail (GTK_IS_TOOLBAR (object));

	g_value_reset (value);
	toolbar = GTK_TOOLBAR (object);

	g_value_set_int (value, toolbar->num_children);
}

void GLADEGTK_API
glade_gtk_toolbar_set_size (GObject *object, GValue *value)
{
	GladeWidget *widget;
	GtkToolbar *toolbar;
	gint new_size;
	gint old_size;

	g_return_if_fail (GTK_IS_TOOLBAR (object));

	toolbar = GTK_TOOLBAR (object);
	widget = glade_widget_get_from_gtk_widget (GTK_WIDGET (toolbar));
	g_return_if_fail (widget != NULL);

	old_size = toolbar->num_children;
	new_size = g_value_get_int (value);

	if (new_size == old_size)
		return;

	if (new_size > old_size) {
		while (new_size > old_size) {
			GtkWidget *placeholder = glade_placeholder_new ();
			gtk_toolbar_append_widget (toolbar, placeholder, NULL, NULL);
			old_size++;
		}
	} else {
		GList *child = g_list_last (toolbar->children);

		while (child && old_size > new_size) {
			GtkWidget *child_widget = ((GtkToolbarChild *) child->data)->widget;
			GladeWidget *glade_widget;

			glade_widget = glade_widget_get_from_gtk_widget (child_widget);
			if (glade_widget)
				glade_project_remove_widget (glade_widget->project, child_widget);

			gtk_container_remove (GTK_CONTAINER (toolbar), child_widget);

			child = g_list_last (toolbar->children);
			old_size--;
		}
	}
}

void GLADEGTK_API
glade_gtk_notebook_get_n_pages (GObject *object, GValue *value)
{
	GtkNotebook *notebook;

	g_value_reset (value);

	notebook = GTK_NOTEBOOK (object);
	g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

	g_value_set_int (value, g_list_length (notebook->children));
}

void GLADEGTK_API
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
			GladePlaceholder *placeholder = GLADE_PLACEHOLDER (glade_placeholder_new ());
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

void GLADEGTK_API
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
					gtk_table_attach_defaults (table, glade_placeholder_new (),
								   i, i + 1, j, j + 1);
		} else {
			gtk_table_resize (table, table->nrows, new_size);

			for (i = old_size; i < table->ncols; i++)
				for (j = 0; j < table->nrows; j++)
					gtk_table_attach_defaults (table, glade_placeholder_new (),
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

void GLADEGTK_API
glade_gtk_table_set_n_rows (GObject *object, GValue *value)
{
	glade_gtk_table_set_n_common (object, value, TRUE);
}

void GLADEGTK_API
glade_gtk_table_set_n_columns (GObject *object, GValue *value)
{
	glade_gtk_table_set_n_common (object, value, FALSE);
}

void GLADEGTK_API
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

	property = glade_widget_get_property (glade_widget, "stock");
	text = glade_widget_get_property (glade_widget, "label");
	g_return_if_fail (property != NULL);
	g_return_if_fail (text != NULL);

	list = property->class->choices;
	for (; list; list = list->next)
	{
		choice = list->data;
		if (val == choice->value)
			break;
	}
	g_return_if_fail (list != NULL);

	if (GTK_BIN (button)->child)
		gtk_container_remove (GTK_CONTAINER (button),
				      GTK_BIN (button)->child);
	
	if (!gtk_stock_lookup (choice->id, &item))
	{
		GtkWidget *label;
		
		label = gtk_label_new (g_value_get_string (text->value));
		gtk_container_add (GTK_CONTAINER (button), label);
		gtk_widget_show_all (button);
	}
	else
	{
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

void GLADEGTK_API
glade_gtk_statusbar_get_has_resize_grip (GObject *object, GValue *value)
{
	GtkStatusbar *statusbar;

	g_return_if_fail (GTK_IS_STATUSBAR (object));

	statusbar = GTK_STATUSBAR (object);

	g_value_reset (value);
	g_value_set_boolean (value, gtk_statusbar_get_has_resize_grip (statusbar));
}

void GLADEGTK_API
glade_gtk_statusbar_set_has_resize_grip (GObject *object, GValue *value)
{
	GtkStatusbar *statusbar;
	gboolean has_resize_grip;

	g_return_if_fail (GTK_IS_STATUSBAR (object));

	statusbar = GTK_STATUSBAR (object);
	has_resize_grip = g_value_get_boolean (value);
	gtk_statusbar_set_has_resize_grip (statusbar, has_resize_grip);
}

void GLADEGTK_API
empty (GObject *object, GValue *value)
{
}

void GLADEGTK_API
ignore (GObject *object, GValue *value)
{
}


/* ------------------------------------ Pre Create functions ------------------------------ */
void GLADEGTK_API
glade_gtk_tree_view_pre_create_function (GObject *object)
{
	GtkWidget *tree_view = GTK_WIDGET (object);
	GtkTreeStore *store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), GTK_TREE_MODEL (store));
	g_object_unref (G_OBJECT (store));

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Column 1", renderer, "text", 0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Column 2", renderer, "text", 1, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);
}

/* ------------------------------------ Post Create functions ------------------------------ */
static int
ask_for_number (const char *title, const char *name, int min, int max, int def)
{
	int number;
	GtkWidget *dialog = gtk_dialog_new_with_buttons (title, NULL,
							 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
							 GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	GtkWidget *label = gtk_label_new (name);
	GtkWidget *spin_button = gtk_spin_button_new_with_range ((double) min, (double) max, 1.0);
	GtkWidget *hbox = gtk_hbox_new (FALSE, 4);

	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spin_button), 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin_button), FALSE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_button), (double) def);

	gtk_box_pack_end (GTK_BOX (hbox), spin_button, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (hbox), label, TRUE, TRUE, 0);

	gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE, 4);

	gtk_widget_show_all (hbox);
	/* even if the user destroys the dialog box, we retrieve the number and we accept it.  I.e., this function never fails */
	gtk_dialog_run (GTK_DIALOG (dialog));

	number = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin_button));

	gtk_widget_destroy (dialog);

	return number;
}

void GLADEGTK_API
glade_gtk_box_post_create (GObject *object)
{
	GladeProperty *property = glade_widget_get_property (glade_widget_get_from_gtk_widget (object), "size");
	GValue value = {0,};

	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, ask_for_number(_("Create a box"), _("Number of items:"), 0, 10000, 3));

	glade_property_set (property, &value);
}

void GLADEGTK_API
glade_gtk_notebook_post_create (GObject *object)
{
	GladeProperty *property = glade_widget_get_property (glade_widget_get_from_gtk_widget (object), "size");
	GValue value = {0,};

	g_value_init (&value, G_TYPE_INT);
	g_value_set_int (&value, ask_for_number(_("Create a notebook"), _("Number of pages:"), 0, 100, 3));
}

void GLADEGTK_API
glade_gtk_table_post_create (GObject *object)
{
	GladeWidget *widget = glade_widget_get_from_gtk_widget (object);
	GladeProperty *property_rows = glade_widget_get_property (widget, "n-rows");
	GladeProperty *property_cols = glade_widget_get_property (widget, "n-columns");
	GValue gvrows = {0,};
	GValue gvcols = {0,};
	GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Create a table"), NULL,
							 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
							 GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);
	GtkWidget *label_rows = gtk_label_new (_("Number of rows:"));
	GtkWidget *label_cols = gtk_label_new (_("Number of columns:"));
	GtkWidget *spin_button_rows = gtk_spin_button_new_with_range (0.0, 10000.0, 1.0);
	GtkWidget *spin_button_cols = gtk_spin_button_new_with_range (0.0, 10000.0, 1.0);
	GtkWidget *table = gtk_table_new (2, 2, TRUE);

	g_value_init (&gvrows, G_TYPE_INT);
	g_value_init (&gvcols, G_TYPE_INT);

	gtk_misc_set_alignment (GTK_MISC (label_rows), 0.0, 0.5);
	gtk_misc_set_alignment (GTK_MISC (label_cols), 0.0, 0.5);

	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spin_button_rows), 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin_button_rows), FALSE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_button_rows), 3.0);

	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (spin_button_cols), 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin_button_cols), FALSE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin_button_cols), 3.0);

	gtk_table_attach_defaults (GTK_TABLE (table), label_rows, 0, 1, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE (table), spin_button_rows, 1, 2, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE (table), label_cols, 0, 1, 1, 2);
	gtk_table_attach_defaults (GTK_TABLE (table), spin_button_cols, 1, 2, 1, 2);

	gtk_table_set_col_spacings (GTK_TABLE (table), 4);
	gtk_container_set_border_width (GTK_CONTAINER (table), 12);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), table, TRUE, TRUE, 0);

	gtk_widget_show_all (table);
	/* even if the user destroys the dialog box, we retrieve the number and we accept it.  I.e., this function never fails */
	gtk_dialog_run (GTK_DIALOG (dialog));

	g_value_set_int (&gvrows, gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin_button_rows)));
	g_value_set_int (&gvcols, gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin_button_cols)));

	glade_property_set (property_rows, &gvrows);
	glade_property_set (property_cols, &gvcols);

	gtk_widget_destroy (dialog);
}

void GLADEGTK_API
glade_gtk_window_post_create (GObject *object)
{
	GtkWindow *window = GTK_WINDOW (object);

	g_return_if_fail (GTK_IS_WINDOW (window));

	gtk_window_set_default_size (window, 440, 250);
}

void GLADEGTK_API
glade_gtk_dialog_post_create (GObject *object)
{
	GtkDialog *dialog = GTK_DIALOG (object);
	GladeWidget *widget;
	GladeWidget *vbox_widget;
	GladeWidget *actionarea_widget;
	GladeWidgetClass *child_class;

	g_return_if_fail (GTK_IS_DIALOG (dialog));

	widget = glade_widget_get_from_gtk_widget (GTK_WIDGET (dialog));
	if (!widget)
		return;

	/* create the GladeWidgets for internal children */
	child_class = glade_widget_class_get_by_name ("GtkVBox");
	if (!child_class)
		return;

	vbox_widget = glade_widget_new_for_internal_child (child_class, widget, dialog->vbox, "vbox");
	if (!vbox_widget)
		return;

	child_class = glade_widget_class_get_by_name ("GtkHButtonBox");
	if (!child_class)
		return;

	gtk_box_pack_start (GTK_BOX (dialog->action_area), GTK_WIDGET (glade_placeholder_new ()), TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (dialog->action_area), GTK_WIDGET (glade_placeholder_new ()), TRUE, TRUE, 0);
	actionarea_widget = glade_widget_new_for_internal_child (child_class, vbox_widget, dialog->action_area, "action_area");
	if (!actionarea_widget)
		return;

	/* set a reasonable default size for a dialog */
	gtk_window_set_default_size (GTK_WINDOW (dialog), 320, 260);
}

void GLADEGTK_API
glade_gtk_message_dialog_post_create (GObject *object)
{
	GtkMessageDialog *dialog = GTK_MESSAGE_DIALOG (object);

	g_return_if_fail (GTK_IS_MESSAGE_DIALOG (dialog));

	gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 115);
}

#if 0
void GLADEGTK_API
glade_gtk_table_post_create (GObject *object)
{
	GtkTable *table = GTK_TABLE (object);
	GList *list;

	g_return_if_fail (GTK_IS_TABLE (table));
	list = table->children;

	/* When we create a table and we are loading from disk a glade
	 * file, we need to add a placeholder because the size can't be
	 * 0 so gtk+ adds a widget that we don't want there.
	 * GtkBox does not have this problem because
	 * a size of 0x0 is the default one. Check if the size is 0
	 * and that we don't have a children.
	 */
	if (g_list_length (list) == 0) {
		gtk_container_add (GTK_CONTAINER (table),
				   glade_placeholder_new ());
	}
}
#endif

/* --------------------------------- Replace child functions ------------------------------- */
void GLADEGTK_API
glade_gtk_container_replace_child (GtkWidget *current,
				   GtkWidget *new,
				   GtkWidget *container)
{
	GParamSpec **param_spec;
	GValue *value;
	guint nproperties;
	guint i;

	if (current->parent != container)
		return;

	param_spec = gtk_container_class_list_child_properties (G_OBJECT_GET_CLASS (container), &nproperties);
	value = calloc (nproperties, sizeof (GValue));
	if (nproperties != 0 && (param_spec == 0 || value == 0))
		// TODO: Signal the not enough memory condition
		return;

	for (i = 0; i < nproperties; i++) {
		g_value_init (&value[i], param_spec[i]->value_type);
		gtk_container_child_get_property (GTK_CONTAINER (container), current, param_spec[i]->name, &value[i]);
	}

	gtk_container_remove (GTK_CONTAINER (container), current);
	gtk_container_add (GTK_CONTAINER (container), new);

	for (i = 0; i < nproperties; i++)
		gtk_container_child_set_property (GTK_CONTAINER (container), new, param_spec[i]->name, &value[i]);

	for (i = 0; i < nproperties; i++)
		g_value_unset (&value[i]);

	g_free (param_spec);
	free (value);

#if 0
	gtk_widget_unparent (child_info->widget);
	child_info->widget = new;
	gtk_widget_set_parent (child_info->widget, GTK_WIDGET (container));

	/* */
	child = new;
	if (GTK_WIDGET_REALIZED (container))
		gtk_widget_realize (child);
	if (GTK_WIDGET_VISIBLE (container) && GTK_WIDGET_VISIBLE (child)) {
		if (GTK_WIDGET_MAPPED (container))
			gtk_widget_map (child);
		gtk_widget_queue_resize (child);
	}
	/* */
#endif
}

void GLADEGTK_API
glade_gtk_notebook_replace_child (GtkWidget *current,
				  GtkWidget *new,
				  GtkWidget *container)
{
	GtkNotebook *notebook;
	GtkWidget *page;
	GtkWidget *label;
	gint page_num;

	notebook = GTK_NOTEBOOK (container);
	page_num = gtk_notebook_page_num (notebook, current);
	if (page_num == -1) {
		g_warning ("GtkNotebookPage not found\n");
		return;
	}

	page = gtk_notebook_get_nth_page (notebook, page_num);
	gtk_widget_ref (page);

	label = gtk_notebook_get_tab_label (notebook, current);

	/* label may be NULL if the label was not set explicitely;
	 * we probably sholud always craete our GladeWidget label
	 * and add set it as tab label, but at the moment we don't.
	 */
	if (label)
		gtk_widget_ref (label);

	gtk_notebook_remove_page (notebook, page_num);
	gtk_notebook_insert_page (notebook, new, label, page_num);
	gtk_notebook_set_tab_label (notebook, new, label);

	gtk_widget_unref (page);
	if (label)
		gtk_widget_unref (label);

	gtk_notebook_set_current_page (notebook, page_num);
}

/* ------------------------------------ Fill Empty functions ------------------------------- */
void GLADEGTK_API
glade_gtk_container_fill_empty (GObject *container)
{
	g_return_if_fail (GTK_IS_CONTAINER (container));

	gtk_container_add (GTK_CONTAINER (container), glade_placeholder_new ());
}

void GLADEGTK_API
glade_gtk_dialog_fill_empty (GObject *dialog)
{
	g_return_if_fail (GTK_IS_DIALOG (dialog));

	/* add a placeholder in the vbox */
	gtk_box_pack_start_defaults (GTK_BOX (GTK_DIALOG (dialog)->vbox),
				     glade_placeholder_new ());
}

void GLADEGTK_API
glade_gtk_paned_fill_empty (GObject *paned)
{
	g_return_if_fail (GTK_IS_PANED (paned));

	gtk_paned_add1 (GTK_PANED (paned), glade_placeholder_new ());
	gtk_paned_add2 (GTK_PANED (paned), glade_placeholder_new ());
}

/* -------------------------------- Get Internal Child functions --------------------------- */
void GLADEGTK_API
glade_gtk_dialog_get_internal_child (GtkWidget *dialog,
				     const gchar *name,
				     GtkWidget **child)
{
	GtkWidget *child_widget;

	g_return_if_fail (GTK_IS_DIALOG (dialog));

	if (strcmp ("vbox", name) == 0)
		child_widget = GTK_DIALOG (dialog)->vbox;
	else if (strcmp ("action_area", name) == 0)
		child_widget = GTK_DIALOG (dialog)->action_area;
	else
		child_widget = NULL;

	*child = child_widget;
}

int GLADEGTK_API
glade_gtk_dialog_child_property_applies (GtkWidget *ancestor,
					 GtkWidget *widget,
					 const char *property_id)
{
	g_return_val_if_fail (GTK_IS_DIALOG (ancestor), FALSE);

	if (strcmp(property_id, "response-id") == 0)
	{
		if (GTK_IS_HBUTTON_BOX (widget->parent) && GTK_IS_VBOX (widget->parent->parent) &&
		    widget->parent->parent->parent == ancestor)
			return TRUE;
	}
	else if (widget->parent == ancestor)
		return TRUE;

	return FALSE;
}

