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

#include "../pixmaps/fixed_bg.xpm"

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

/* ------------------------------------ Constants ------------------------------ */
#define FIXED_DEFAULT_CHILD_WIDTH  100
#define FIXED_DEFAULT_CHILD_HEIGHT 60



/* ---------------------------- Custom Property definitions ------------------------ */
static GType
glade_gtk_stock_get_type (void)
{
	static GType etype = 0;
	if (etype == 0) {
		static const GEnumValue values[] = {
			{ 0,   "None",   "glade-none" },
			{ 1,   "Ok",     "gtk-ok"     },
			{ 2,   "Cancel", "gtk-cancel" },
			{ 3,   "Apply",  "gtk-apply"  },
			{ 4,   "Close",  "gtk-close"  },
			{ 0, NULL, NULL }
		};
		etype = g_enum_register_static ("GladeGtkStockType", values);
	}
	return etype;
}

GLADEGTK_API GParamSpec *
glade_gtk_stock_spec (void)
{
	return g_param_spec_enum ("stock", "stock", "stock",
				  glade_gtk_stock_get_type (),
				  0, G_PARAM_READWRITE);
}

GLADEGTK_API GParamSpec *
glade_gtk_standard_int_spec (void)
{
	static GParamSpec *int_spec = NULL;
	if (!int_spec)
		int_spec = g_param_spec_int ("int", "int", "int",
					     0, G_MAXINT,
					     0, G_PARAM_READWRITE);
	return int_spec;
}

GLADEGTK_API GParamSpec *
glade_gtk_standard_string_spec (void)
{
	static GParamSpec *str_spec = NULL;
	if (!str_spec)
		str_spec = g_param_spec_string ("string", "string", "string",
						"", G_PARAM_READWRITE);
	return str_spec;
}

GLADEGTK_API GParamSpec *
glade_gtk_standard_float_spec (void)
{
	static GParamSpec *float_spec = NULL;
	if (!float_spec)
		float_spec = g_param_spec_float ("float", "float", "float",
						 0.0F, G_MAXFLOAT, 0.0F,
						 G_PARAM_READWRITE);
	return float_spec;
}

/* ------------------------------------ Custom Properties ------------------------------ */
/**
 * glade_gtk_option_menu_set_items:
 * @object:
 * @value:
 *
 * TODO: write me
 */
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

/**
 * glade_gtk_widget_condition:
 * @klass:
 *
 * TODO: write me
 */
gint GLADEGTK_API
glade_gtk_widget_condition (GladeWidgetClass *klass)
{
	GtkObject *object = (GtkObject*)g_object_new (klass->type, NULL);
	gboolean result;

	/* Only widgets with windows can have tooltips at present. Though
	buttons seem to be a special case, as they are NO_WINDOW widgets
	but have InputOnly windows, so tooltip still work. In GTK+ 2
	menuitems are like buttons. */
	result = (!GTK_WIDGET_NO_WINDOW (object) ||
		  GTK_IS_BUTTON (object) ||
		  GTK_IS_MENU_ITEM (object));

	gtk_object_ref (object);
	gtk_object_sink (object);
	gtk_object_unref (object);

	return result;
}

/**
 * glade_gtk_widget_set_tooltip:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_widget_set_tooltip (GObject *object, GValue *value)
{
	GladeWidget *glade_widget = glade_widget_get_from_gobject (GTK_WIDGET (object));
	GladeProject *project = glade_widget_get_project (glade_widget);
	GtkTooltips *tooltips = glade_project_get_tooltips (project);
	const char *tooltip;

	/* TODO: handle GtkToolItems with gtk_tool_item_set_tooltip() */
	tooltip = g_value_get_string (value);
	if (tooltip && *tooltip)
		gtk_tooltips_set_tip (tooltips, GTK_WIDGET (object), tooltip, NULL);
	else
		gtk_tooltips_set_tip (tooltips, GTK_WIDGET (object), NULL, NULL);
}

/**
 * glade_gtk_widget_get_tooltip:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_widget_get_tooltip (GObject *object, GValue *value)
{
	GtkTooltipsData *tooltips_data = gtk_tooltips_data_get (GTK_WIDGET (object));
	
	g_value_reset (value);
	g_value_set_string (value,
			    tooltips_data ? tooltips_data->tip_text : NULL);
}

/**
 * glade_gtk_progress_bar_set_format:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_progress_bar_set_format (GObject *object, GValue *value)
{
	GtkProgressBar *bar;
	const gchar *format = g_value_get_string (value);

	bar = GTK_PROGRESS_BAR (object);
	g_return_if_fail (GTK_IS_PROGRESS_BAR (bar));
	
	gtk_progress_set_format_string (GTK_PROGRESS (bar), format);
}

/**
 * glade_gtk_spin_button_set_max:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_spin_button_set_max (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (object)));

	adjustment->upper = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}

/**
 * glade_gtk_spin_button_set_min:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_spin_button_set_min (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (object)));

	adjustment->lower = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}

/**
 * glade_gtk_spin_button_set_step_increment:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_spin_button_set_step_increment (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (object)));

	adjustment->step_increment = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}

/**
 * glade_gtk_spin_button_set_page_increment:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_spin_button_set_page_increment (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (object)));

	adjustment->page_increment = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}

/**
 * glade_gtk_spin_button_set_page_size:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_spin_button_set_page_size (GObject *object, GValue *value)
{
	GtkAdjustment *adjustment;

	adjustment = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (object)));

	adjustment->page_size = g_value_get_float (value);
	gtk_adjustment_changed (adjustment);
}

/**
 * glade_gtk_box_get_size:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_box_get_size (GObject *object, GValue *value)
{
	GtkBox *box = GTK_BOX (object);

	g_value_reset (value);
	g_value_set_int (value, g_list_length (box->children));
}

static gint
glade_gtk_box_get_first_blank (GtkBox *box)
{
	GList       *child;
	GladeWidget *gwidget;
	gint         position;
	
	for (child = box->children, position = 0;
	     child && child->data;
	     child = child->next, position++)
	{
		GtkWidget *widget = ((GtkBoxChild *) (child->data))->widget;

		if ((gwidget = glade_widget_get_from_gobject (widget)) != NULL)
		{
			gint gwidget_position;
			GladeProperty *property =
				glade_widget_get_property (gwidget, "position");
			gwidget_position = g_value_get_int (property->value);

			if ((gwidget_position - position) > 0)
				return position;
		}
	}
	return position;
}

/**
 * glade_gtk_box_set_size:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_box_set_size (GObject *object, GValue *value)
{
	GtkBox      *box;
	GList       *child;
	guint new_size, old_size, i;

	box = GTK_BOX (object);
	g_return_if_fail (GTK_IS_BOX (box));

	old_size = g_list_length (box->children);
	new_size = g_value_get_int (value);

	/* Ensure placeholders first...
	 */
	for (i = 0; i < new_size; i++)
	{
		if (g_list_length(box->children) < (i + 1))
		{
			GtkWidget *placeholder = glade_placeholder_new ();
			gint       blank       = glade_gtk_box_get_first_blank (box);

			gtk_container_add (GTK_CONTAINER (box), placeholder);
			gtk_box_reorder_child (box, placeholder, blank);
		}
	}

	/* The box has shrunk. Remove the widgets that are on those slots */
	for (child = g_list_last (box->children);
	     child && old_size > new_size;
	     child = g_list_last (box->children), old_size--)
	{
		GtkWidget *child_widget = ((GtkBoxChild *) (child->data))->widget;

		if (glade_widget_get_from_gobject (child_widget))
			/* In this case, refuse to shrink */
			break;

		gtk_container_remove (GTK_CONTAINER (box), child_widget);
	}
}

gboolean GLADEGTK_API
glade_gtk_box_verify_size (GObject *object, GValue *value)
{
	GtkBox *box = GTK_BOX(object);
	GList  *child;
	gint    old_size = g_list_length (box->children);
	gint    new_size = g_value_get_int (value);

	for (child = g_list_last (box->children);
	     child && old_size > new_size;
	     child = g_list_previous (child), old_size--)
	{
		GtkWidget *widget = ((GtkBoxChild *) (child->data))->widget;
		if (glade_widget_get_from_gobject (widget) != NULL)
			/* In this case, refuse to shrink */
			return FALSE;
	}
	return TRUE;
}

/**
 * glade_gtk_toolbar_get_size:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_toolbar_get_size (GObject *object, GValue *value)
{
	GtkToolbar *toolbar;

	g_return_if_fail (GTK_IS_TOOLBAR (object));

	g_value_reset (value);
	toolbar = GTK_TOOLBAR (object);

	g_value_set_int (value, toolbar->num_children);
}

/**
 * glade_gtk_toolbar_set_size:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_toolbar_set_size (GObject *object, GValue *value)
{
	GtkToolbar  *toolbar  = GTK_TOOLBAR (object);
	gint         new_size = g_value_get_int (value);
	gint         old_size = toolbar->num_children;
	GList       *child;

	g_print ("Toolbar (set) old size: %d, new size %d\n", old_size, new_size);
	/* Ensure base size
	 */
	while (new_size > old_size) {
		gtk_toolbar_append_widget (toolbar, glade_placeholder_new (), NULL, NULL);
		old_size++;
	}

	for (child = g_list_last (toolbar->children);
	     child && old_size > new_size;
	     child = g_list_last (toolbar->children), old_size--)
	{
		GtkWidget *child_widget = ((GtkToolbarChild *) child->data)->widget;
		
		if (glade_widget_get_from_gobject (child_widget))
			break;

		gtk_container_remove (GTK_CONTAINER (toolbar), child_widget);
	}
	g_print ("Toolbar (set) now size %d\n", toolbar->num_children);
}

gboolean GLADEGTK_API
glade_gtk_toolbar_verify_size (GObject *object, GValue *value)
{
	GtkToolbar  *toolbar  = GTK_TOOLBAR (object);
	gint         new_size = g_value_get_int (value);
	gint         old_size = toolbar->num_children;
	GList       *child;

	g_print ("Toolbar (verify) old size: %d, new size %d\n", old_size, new_size);

	for (child = g_list_last (toolbar->children);
	     child && old_size > new_size;
	     child = g_list_previous (child), old_size--)
	{
		GtkWidget *child_widget = ((GtkToolbarChild *) child->data)->widget;
		
		if (glade_widget_get_from_gobject (child_widget))
			return FALSE;
	}
	return TRUE;
}

/**
 * glade_gtk_notebook_get_n_pages:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_notebook_get_n_pages (GObject *object, GValue *value)
{
	GtkNotebook *notebook;

	g_value_reset (value);

	notebook = GTK_NOTEBOOK (object);
	g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

	g_value_set_int (value, g_list_length (notebook->children));
}

/**
 * glade_gtk_notebook_get_tab_label_text:
 * @object: A GtkWidget which is a page on the notebook.
 * @value: The label text is returned as a GValue.
 *
 * Gets the tab label text of a GtkNotebook child widget.
 */
void GLADEGTK_API
glade_gtk_notebook_get_tab_label_text (GObject *object, GValue *value)
{
	GtkNotebook *notebook;
	GtkWidget *child;

	g_value_reset (value);
	child = GTK_WIDGET (object);
	g_return_if_fail (GTK_IS_WIDGET (child));
	notebook = GTK_NOTEBOOK (gtk_widget_get_parent (child));
	g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

	g_value_set_string (value, 
		gtk_notebook_get_tab_label_text (notebook, child));
}

/**
 * glade_gtk_notebook_set_tab_label_text:
 * @object: A GtkWidget which is a page on the notebook.
 * @value: The new label text to be set.
 *
 * Sets the tab label text of a GtkNotebook child widget.
 */
void GLADEGTK_API
glade_gtk_notebook_set_tab_label_text (GObject *object, GValue *value)
{
	GtkNotebook *notebook;
	GtkWidget *child;

	child = GTK_WIDGET (object);
	g_return_if_fail (GTK_IS_WIDGET (child));
	notebook = GTK_NOTEBOOK (gtk_widget_get_parent (child));
	g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

	gtk_notebook_set_tab_label_text (notebook, child, 
					 g_value_get_string (value));
}

static gint
glade_gtk_notebook_get_first_blank_page (GtkNotebook *notebook)
{
	GladeWidget *gwidget;
	GtkWidget   *widget;
	gint         position;
	for (position = 0; position < gtk_notebook_get_n_pages (notebook); position++)
	{
		widget = gtk_notebook_get_nth_page (notebook, position);
		if ((gwidget = glade_widget_get_from_gobject (widget)) != NULL)
		{
 			GladeProperty *property =
				glade_widget_get_property (gwidget, "position");
			gint gwidget_position = g_value_get_int (property->value);

			if ((gwidget_position - position) > 0)
				return position;
		}
	}
	return position;
}

/**
 * glade_gtk_notebook_set_n_pages:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_notebook_set_n_pages (GObject *object, GValue *value)
{
	GladeWidget *widget;
	GtkNotebook *notebook;
	GtkWidget   *child_widget;
	gint new_size, i;
	gint old_size;

	notebook = GTK_NOTEBOOK (object);
	g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

	widget = glade_widget_get_from_gobject (GTK_WIDGET (notebook));
	g_return_if_fail (widget != NULL);

	new_size = g_value_get_int (value);

	/* Ensure base size of notebook */
	for (i = gtk_notebook_get_n_pages (notebook); i < new_size; i++)
	{
		gint position = glade_gtk_notebook_get_first_blank_page (notebook);
		gtk_notebook_insert_page (notebook, glade_placeholder_new (),
					  NULL, position);
	}

	old_size = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));


	/*
	 * Thing to remember is that GtkNotebook starts the
	 * page numbers from 0, not 1 (C-style). So we need to do
	 * old_size-1, where we're referring to "nth" widget.
	 */
	while (old_size > new_size) {
		/* Get the last widget. */
		child_widget = gtk_notebook_get_nth_page (notebook, old_size-1);

		/* 
		 * If we got it, and its not a placeholder, remove it
		 * from project.
		 */
		if (glade_widget_get_from_gobject (child_widget))
			break;

		gtk_notebook_remove_page (notebook, old_size-1);
		old_size--;
	}
}

gboolean GLADEGTK_API
glade_gtk_notebook_verify_n_pages (GObject *object, GValue *value)
{
	GtkNotebook *notebook = GTK_NOTEBOOK(object);
	GtkWidget *child_widget;
	gint old_size, new_size = g_value_get_int (value);
	
	for (old_size = gtk_notebook_get_n_pages (notebook);
	     old_size > new_size; old_size--) {
		/* Get the last widget. */
		child_widget = gtk_notebook_get_nth_page (notebook, old_size-1);

		/* 
		 * If we got it, and its not a placeholder, remove it
		 * from project.
		 */
		if (glade_widget_get_from_gobject (child_widget))
			return FALSE;
	}
	return TRUE;
}

static gboolean
glade_gtk_table_has_child (GtkTable *table,
			   guint left_attach,
			   guint right_attach,
			   guint top_attach,
			   guint bottom_attach)
{
	GList *list;

	for (list = table->children; list && list->data; list = list->next)
	{
		GtkTableChild *child = list->data;

		if (child->left_attach   == left_attach &&
		    child->right_attach  == right_attach &&
		    child->top_attach    == top_attach &&
		    child->bottom_attach == bottom_attach)
			return TRUE;
	}
	return FALSE;
}

static gboolean
glade_gtk_table_widget_exceeds_bounds (GtkTable *table, gint n_rows, gint n_cols)
{
	GList *list;
	for (list = table->children; list && list->data; list = list->next)
	{
		GtkTableChild *child = list->data;
		if (GLADE_IS_PLACEHOLDER(child->widget) == FALSE &&
		    (child->right_attach  > n_cols ||
		     child->bottom_attach > n_rows))
			return TRUE;
	}
	return FALSE;
}

/**
 * glade_gtk_table_set_n_common:
 * @object:
 * @value:
 *
 * TODO: write me
 */
static void
glade_gtk_table_set_n_common (GObject *object, GValue *value, gboolean for_rows)
{
	GladeWidget *widget;
	GtkTable *table;
	guint new_size;
	guint old_size;
	gint i, j;

	table = GTK_TABLE (object);
	g_return_if_fail (GTK_IS_TABLE (table));

	new_size = g_value_get_uint (value);
	old_size = for_rows ? table->nrows : table->ncols;

	if (new_size < 1)
		return;

	if (glade_gtk_table_widget_exceeds_bounds
	    (table,
	     for_rows ? new_size : table->nrows,
	     for_rows ? table->ncols : new_size))
		/* Refuse to shrink if it means orphaning widgets */
		return;

	widget = glade_widget_get_from_gobject (GTK_WIDGET (table));
	g_return_if_fail (widget != NULL);

	if (for_rows)
		gtk_table_resize (table, new_size, table->ncols);
	else
		gtk_table_resize (table, table->nrows, new_size);

	for (i = 0; i < table->ncols; i++)
		for (j = 0; j < table->nrows; j++)
			if (glade_gtk_table_has_child
			    (table, i, i + 1, j, j + 1) == FALSE)
				gtk_table_attach_defaults
					(table, glade_placeholder_new (),
					 i, i + 1, j, j + 1);

	if (new_size < old_size)
	{
		/* Remove from the bottom up */
		GList *list;
		GList *list_to_free = NULL;

		for (list = table->children; list && list->data; list = list->next)
		{
			GtkTableChild *child = list->data;
			guint start = for_rows ? child->top_attach : child->left_attach;
			guint end = for_rows ? child->bottom_attach : child->right_attach;

			/* We need to completely remove it */
			if (start >= new_size)
			{
				list_to_free = g_list_prepend (list_to_free, child->widget);
				continue;
			}

			/* If the widget spans beyond the new border,
			 * we should resize it to fit on the new table */
			if (end > new_size)
				gtk_container_child_set
					(GTK_CONTAINER (table), GTK_WIDGET (child),
					 for_rows ? "bottom_attach" : "right_attach",
					 new_size, NULL);
		}

		if (list_to_free)
		{
			for (list = g_list_first(list_to_free);
			     list && list->data;
			     list = list->next)
			{
				gtk_container_remove (GTK_CONTAINER (table),
						      GTK_WIDGET(list->data));
			}
			g_list_free (list_to_free);
		}
		gtk_table_resize (table,
				  for_rows ? new_size : table->nrows,
				  for_rows ? table->ncols : new_size);
	}
}

/**
 * glade_gtk_table_set_n_rows:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_table_set_n_rows (GObject *object, GValue *value)
{
	glade_gtk_table_set_n_common (object, value, TRUE);
}

/**
 * glade_gtk_table_set_n_columns:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_table_set_n_columns (GObject *object, GValue *value)
{
	glade_gtk_table_set_n_common (object, value, FALSE);
}

static gboolean 
glade_gtk_table_verify_n_common (GObject *object, GValue *value, gboolean for_rows)
{
	GtkTable *table = GTK_TABLE(object);
	guint new_size = g_value_get_uint (value);

	if (glade_gtk_table_widget_exceeds_bounds
	    (table,
	     for_rows ? new_size : table->nrows,
	     for_rows ? table->ncols : new_size))
		/* Refuse to shrink if it means orphaning widgets */
		return FALSE;

	return TRUE;
}

gboolean GLADEGTK_API
glade_gtk_table_verify_n_rows (GObject *object, GValue *value)
{
	return glade_gtk_table_verify_n_common (object, value, TRUE);
}

gboolean GLADEGTK_API
glade_gtk_table_verify_n_columns (GObject *object, GValue *value)
{
	return glade_gtk_table_verify_n_common (object, value, FALSE);
}

/**
 * glade_gtk_button_set_stock:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_button_set_stock (GObject *object, GValue *value)
{
	GladeWidget *glade_widget;
	GtkWidget *button;
	GtkStockItem item;
	GladeProperty *property;
	GladeProperty *text;
	GEnumClass    *eclass;
	guint i;
	gint val;

	val = g_value_get_enum (value);	

	button = GTK_WIDGET (object);
	g_return_if_fail (GTK_IS_BUTTON (button));
	glade_widget = glade_widget_get_from_gobject (button);
	g_return_if_fail (glade_widget != NULL);

	property = glade_widget_get_property (glade_widget, "stock");
	text = glade_widget_get_property (glade_widget, "label");

	eclass = g_type_class_ref (property->class->pspec->value_type);
	g_return_if_fail (property != NULL);
	g_return_if_fail (text != NULL);

	for (i = 0; i < eclass->n_values; i++)
	{
		if (val == eclass->values[i].value)
			break;
	}

	g_return_if_fail (i < eclass->n_values);

	if (GTK_BIN (button)->child)
		gtk_container_remove (GTK_CONTAINER (button),
				      GTK_BIN (button)->child);
	
	if (!gtk_stock_lookup (eclass->values[i].value_nick, &item))
	{
		GtkWidget *label;
		
		if (g_value_get_boolean (
			glade_widget_get_property (
				glade_widget, "use-underline")->value))
			label = gtk_label_new_with_mnemonic (g_value_get_string (text->value));
		else
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
		image = gtk_image_new_from_stock (eclass->values[i].value_nick,
						  GTK_ICON_SIZE_BUTTON);

		gtk_label_set_mnemonic_widget (GTK_LABEL (label),
					       button);

		gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
		gtk_box_pack_end (GTK_BOX (hbox), label, TRUE, TRUE, 0);
		gtk_container_add (GTK_CONTAINER (button), hbox);
		
		gtk_widget_show_all (button);
	}

	g_type_class_unref (eclass);
}

/**
 * glade_gtk_statusbar_get_has_resize_grip:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_statusbar_get_has_resize_grip (GObject *object, GValue *value)
{
	GtkStatusbar *statusbar;

	g_return_if_fail (GTK_IS_STATUSBAR (object));

	statusbar = GTK_STATUSBAR (object);

	g_value_reset (value);
	g_value_set_boolean (value, gtk_statusbar_get_has_resize_grip (statusbar));
}

/**
 * glade_gtk_statusbar_set_has_resize_grip:
 * @object:
 * @value:
 *
 * TODO: write me
 */
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

/**
 * empty:
 * @object: a #GObject
 * @value: a #GValue
 *
 * This function does absolutely nothing
 * (and is for use in overriding fill_empty functions).
 */
void GLADEGTK_API
empty (GObject *container)
{
}

/**
 * ignore:
 * @object: a #GObject
 * @value: a #GValue
 *
 * This function does absolutely nothing
 */
void GLADEGTK_API
ignore (GObject *object, GValue *value)
{
}


/* ------------------------- Post Create functions ------------------------- */
static gboolean
glade_gtk_fixed_button_press (GtkWidget       *widget,
			      GdkEventButton  *event,
			      gpointer         user_data)
{
	GladeWidget   *gwidget;
	GladeWidget   *fixed_gwidget;
	GladeProperty *property;
	GValue         value = { 0, };
	
	if (event->button == 1 && event->type == GDK_BUTTON_PRESS)
	{
		fixed_gwidget = glade_widget_get_from_gobject (widget);

		/* If there is a class to add and we successfully
		 * create a widget.
		 */
		if (((glade_default_app_get_add_class() != NULL)         ||
		     ((event->state & GDK_SHIFT_MASK) &&
		      glade_default_app_get_alt_class() != NULL))        &&
		    ((gwidget = glade_command_create
		      (glade_default_app_get_add_class() ?
			   	glade_default_app_get_add_class() :
			   	 glade_default_app_get_alt_class(),
		       fixed_gwidget, NULL, fixed_gwidget->project)) != NULL))
		{
			/* reset the palette */
			glade_palette_unselect_widget (glade_default_app_get_palette());

			g_value_init (&value, G_TYPE_INT);
			
			property = glade_widget_get_property (gwidget, "x");
			property->enabled = TRUE;
			g_value_set_int (&value, event->x);
			glade_property_set (property, &value);
			
			property = glade_widget_get_property (gwidget, "y");
			property->enabled = TRUE;
			g_value_set_int (&value, event->y);
			glade_property_set (property, &value);
			
			property = glade_widget_get_property
				(gwidget, "width-request");
			property->enabled = TRUE;
			g_value_set_int (&value, FIXED_DEFAULT_CHILD_WIDTH);
			glade_property_set (property, &value);
			
			property = glade_widget_get_property
				(gwidget, "height-request");
			property->enabled = TRUE;
			g_value_set_int (&value, FIXED_DEFAULT_CHILD_HEIGHT);
			glade_property_set (property, &value);
			
			/* We need to resync the editor so that width-request/height-request
			 * are actualy enabled in the editor
			 */
			glade_editor_refresh (glade_default_app_get_editor());
		}
		return TRUE;
	}
	return FALSE;
}

void
glade_gtk_fixed_finalize(GdkPixmap *backing)
{
	g_object_unref(backing);
}

void
glade_gtk_fixed_realize (GtkWidget *widget)
{
	GdkPixmap *backing =
		gdk_pixmap_colormap_create_from_xpm_d (NULL, gtk_widget_get_colormap (widget),
						       NULL, NULL, fixed_bg_xpm);
	gdk_window_set_back_pixmap (widget->window, backing, FALSE);
	/* For cleanup later
	 */
	g_object_weak_ref(G_OBJECT(widget), (GWeakNotify)glade_gtk_fixed_finalize, backing);
}

void GLADEGTK_API
glade_gtk_fixed_post_create (GObject *object)
{
	/* Only widgets with windows can recieve
	 * mouse events
	 */
	GTK_WIDGET_UNSET_FLAGS(object, GTK_NO_WINDOW);
	/* For backing pixmap
	 */
	g_signal_connect_after(object, "realize",
			       G_CALLBACK(glade_gtk_fixed_realize), NULL);
	
	/* Menu will be activated on mouse clicks
	 */
	g_signal_connect(object, "button-press-event",
			 G_CALLBACK(glade_gtk_fixed_button_press), NULL);
}

/**
 * glade_gtk_window_post_create:
 * @object:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_window_post_create (GObject *object)
{
	GtkWindow *window = GTK_WINDOW (object);

	g_return_if_fail (GTK_IS_WINDOW (window));

	gtk_window_set_default_size (window, 440, 250);
}

void GLADEGTK_API
glade_gtk_menu_bar_post_create (GObject *object)
{
	GtkMenuBar *menu_bar;
	GtkWidget  *item;

	g_return_if_fail (GTK_IS_MENU_BAR (object));
	
	menu_bar = GTK_MENU_BAR (object);
	
	item = gtk_menu_item_new_with_mnemonic (_("_File"));
	gtk_menu_bar_append (menu_bar, item);
	
	item = gtk_menu_item_new_with_mnemonic (_("_Edit"));
	gtk_menu_bar_append (menu_bar, item);
	
	item = gtk_menu_item_new_with_mnemonic (_("_Help"));
	gtk_menu_bar_append (menu_bar, item);
}

/**
 * glade_gtk_dialog_post_create:
 * @object:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_dialog_post_create (GObject *object)
{
	GtkDialog *dialog = GTK_DIALOG (object);
	GladeWidget *widget;
	GladeWidget *vbox_widget;
	GladeWidget *actionarea_widget;
	GladeWidgetClass *child_class;

	g_return_if_fail (GTK_IS_DIALOG (dialog));

	widget = glade_widget_get_from_gobject (GTK_WIDGET (dialog));
	if (!widget)
		return;

	/* create the GladeWidgets for internal children */
	child_class = glade_widget_class_get_by_name ("GtkVBox");
	if (!child_class)
		return;

	vbox_widget = glade_widget_new_for_internal_child (child_class, widget,
							   G_OBJECT(dialog->vbox), "vbox");
	if (!vbox_widget)
		return;

	child_class = glade_widget_class_get_by_name ("GtkHButtonBox");
	if (!child_class)
		return;

	gtk_box_pack_start (GTK_BOX (dialog->action_area),
			    GTK_WIDGET (glade_placeholder_new ()), TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (dialog->action_area),
			    GTK_WIDGET (glade_placeholder_new ()), TRUE, TRUE, 0);

	actionarea_widget =
		glade_widget_new_for_internal_child
		(child_class, vbox_widget, G_OBJECT(dialog->action_area), "action_area");
	if (!actionarea_widget)
		return;

	/* set a reasonable default size for a dialog */
	gtk_window_set_default_size (GTK_WINDOW (dialog), 320, 260);
}


/**
 * glade_gtk_message_dialog_post_create:
 * @object:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_message_dialog_post_create (GObject *object)
{
	GtkDialog   *dialog = GTK_DIALOG (object);
	GladeWidget *widget;
	GladeWidget *vbox_widget;
	GladeWidget *actionarea_widget;
	GladeWidgetClass *child_class;
	
	g_return_if_fail (GTK_IS_MESSAGE_DIALOG (dialog));

	widget = glade_widget_get_from_gobject (GTK_WIDGET (dialog));
	if (!widget)
		return;

	
	/* create the GladeWidgets for internal children */
	child_class = glade_widget_class_get_by_name ("GtkVBox");
	if (!child_class)
		return;

	vbox_widget = glade_widget_new_for_internal_child (child_class, widget,
							   G_OBJECT(dialog->vbox), "vbox");
	if (!vbox_widget)
		return;

	child_class = glade_widget_class_get_by_name ("GtkHButtonBox");
	if (!child_class)
		return;

	actionarea_widget =
		glade_widget_new_for_internal_child
		(child_class, vbox_widget, G_OBJECT(dialog->action_area), "action_area");
	if (!actionarea_widget)
		return;

	
	gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 115);
}

/**
 * glade_gtk_tree_post_create:
 * @object:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_tree_view_post_create (GObject *object)
{
	GtkWidget *tree_view = GTK_WIDGET (object);
	GtkTreeStore *store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), GTK_TREE_MODEL (store));
	g_object_unref (G_OBJECT (store));

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes
		("Column 1", renderer, "text", 0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes
		("Column 2", renderer, "text", 1, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);
}

/* ------------------------ Replace child functions ------------------------ */
/**
 * glade_gtk_container_replace_child:
 * @current:
 * @new:
 * @container:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_container_replace_child (GtkWidget *container,
				   GtkWidget *current,
				   GtkWidget *new)
{
	GParamSpec **param_spec;
	GValue *value;
	guint nproperties;
	guint i;

	if (current->parent != container)
		return;

	param_spec = gtk_container_class_list_child_properties
		(G_OBJECT_GET_CLASS (container), &nproperties);
	value = g_malloc0 (sizeof(GValue) * nproperties);

	for (i = 0; i < nproperties; i++) {
		g_value_init (&value[i], param_spec[i]->value_type);
		gtk_container_child_get_property
			(GTK_CONTAINER (container), current, param_spec[i]->name, &value[i]);
	}

	gtk_container_remove (GTK_CONTAINER (container), current);
	gtk_container_add (GTK_CONTAINER (container), new);

	for (i = 0; i < nproperties; i++) {
		gtk_container_child_set_property
			(GTK_CONTAINER (container), new, param_spec[i]->name, &value[i]);
	}
	
	for (i = 0; i < nproperties; i++)
		g_value_unset (&value[i]);

	g_free (param_spec);
	g_free (value);
}

/**
 * glade_gtk_notebook_replace_child:
 * @current:
 * @new:
 * @container:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_notebook_replace_child (GtkWidget *container,
				  GtkWidget *current,
				  GtkWidget *new)
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

	gtk_widget_unref (page);
	if (label)
		gtk_widget_unref (label);

	/* There seems to be a GtkNotebook bug where if the page is
	 * not shown first it will not set the current page */
	gtk_widget_show (new);
	gtk_notebook_set_current_page (notebook, page_num);
}

/* -------------------------- Fill Empty functions -------------------------- */
/**
 * glade_gtk_container_fill_empty:
 * @container:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_container_fill_empty (GObject *container)
{
	GList *children;
	g_return_if_fail (GTK_IS_CONTAINER (container));

	if ((children = gtk_container_get_children (GTK_CONTAINER (container))) == NULL)
		gtk_container_add (GTK_CONTAINER (container), glade_placeholder_new ());
	else
		g_list_free (children);
}

/**
 * glade_gtk_dialog_fill_empty:
 * @dialog:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_dialog_fill_empty (GObject *dialog)
{
	g_return_if_fail (GTK_IS_DIALOG (dialog));

	/* add a placeholder in the vbox */
	gtk_box_pack_start_defaults (GTK_BOX (GTK_DIALOG (dialog)->vbox),
				     glade_placeholder_new ());
}

/**
 * glade_gtk_paned_fill_empty:
 * @paned:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_paned_fill_empty (GObject *paned)
{
	g_return_if_fail (GTK_IS_PANED (paned));

	gtk_paned_add1 (GTK_PANED (paned), glade_placeholder_new ());
	gtk_paned_add2 (GTK_PANED (paned), glade_placeholder_new ());
}

/* ---------------------- Get Internal Child functions ---------------------- */
/**
 * glade_gtk_dialog_get_internal_child:
 * @dialog:
 * @name:
 * @child:
 *
 * TODO: write me
 */
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

/**
 * glade_gtk_dialog_child_property_applies:
 * @ancestor:
 * @widget:
 * @property_id:
 *
 * TODO: write me
 *
 * Returns:
 */
int GLADEGTK_API
glade_gtk_dialog_child_property_applies (GtkWidget *ancestor,
					 GtkWidget *widget,
					 const char *property_id)
{
	g_return_val_if_fail (GTK_IS_DIALOG (ancestor), FALSE);

	if (strcmp(property_id, "response-id") == 0)
	{
		if (GTK_IS_HBUTTON_BOX (widget->parent) &&
		    GTK_IS_VBOX (widget->parent->parent) &&
		    widget->parent->parent->parent == ancestor)
			return TRUE;
	}
	else if (widget->parent == ancestor)
		return TRUE;

	return FALSE;
}


/**
 * glade_gtk_radio_button_get_group:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_radio_button_get_group (GObject *object, GValue *value)
{
	GtkRadioButton *button = GTK_RADIO_BUTTON (object);
	GSList *group;
	const char *name;

	group = gtk_radio_button_get_group (button);
	name = group
		? gtk_widget_get_name (group->data)
		: NULL;

	g_value_reset (value);
	g_value_set_string (value, name);
}


/**
 * glade_gtk_radio_button_set_group:
 * @object:
 * @value:
 *
 * TODO: write me
 */
void GLADEGTK_API
glade_gtk_radio_button_set_group (GObject *object, GValue *value)
{
	GtkRadioButton *button = GTK_RADIO_BUTTON (object);
	const char *name = g_value_get_string (value);

	/* FIXME: now what?  */
}
