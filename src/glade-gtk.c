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
#include "glade.h"

#include "fixed_bg.xpm"

#define GETTEXT_PACKAGE "glade-gtk"
#include <glib/gi18n-lib.h>

#ifdef G_OS_WIN32
#define GLADEGTK_API __declspec(dllexport)
#else
#define GLADEGTK_API
#endif

/* ------------------------------------ Constants ------------------------------ */
#define FIXED_DEFAULT_CHILD_WIDTH  100
#define FIXED_DEFAULT_CHILD_HEIGHT 60


/* ------------------------------ Types & ParamSpecs --------------------------- */
typedef enum {
	GLADEGTK_IMAGE_FILENAME = 0,
	GLADEGTK_IMAGE_STOCK,
	GLADEGTK_IMAGE_ICONTHEME
} GladeGtkImageType;

static GType
glade_gtk_image_type_get_type (void)
{
	static GType etype = 0;
	if (etype == 0) {
		static const GEnumValue values[] = {
			{ GLADEGTK_IMAGE_FILENAME,  "Filename",   "glade-gtk-image-filename" },
			{ GLADEGTK_IMAGE_STOCK,     "Stock",      "glade-gtk-image-stock" },
			{ GLADEGTK_IMAGE_ICONTHEME, "Icon Theme", "glade-gtk-image-icontheme" },
			{ 0, NULL, NULL }
		};
		etype = g_enum_register_static ("GladeGtkImageType", values);
	}
	return etype;
}

GParamSpec * GLADEGTK_API
glade_gtk_image_type_spec (void)
{
	return g_param_spec_enum ("type", _("Type"), 
				  _("Chose the image type"),
				  glade_gtk_image_type_get_type (),
				  0, G_PARAM_READWRITE);
}


/* ------------------------------------ Custom Properties ------------------------------ */
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
glade_gtk_widget_set_tooltip (GObject *object, GValue *value)
{
	GladeWidget   *glade_widget = glade_widget_get_from_gobject (GTK_WIDGET (object));
	GladeProject       *project = (GladeProject *)glade_widget_get_project (glade_widget);
	GtkTooltips *tooltips = glade_project_get_tooltips (project);
	const char *tooltip;

	/* TODO: handle GtkToolItems with gtk_tool_item_set_tooltip() */
	tooltip = g_value_get_string (value);
	if (tooltip && *tooltip)
		gtk_tooltips_set_tip (tooltips, GTK_WIDGET (object), tooltip, NULL);
	else
		gtk_tooltips_set_tip (tooltips, GTK_WIDGET (object), NULL, NULL);
}

void GLADEGTK_API
glade_gtk_widget_get_tooltip (GObject *object, GValue *value)
{
	GtkTooltipsData *tooltips_data = gtk_tooltips_data_get (GTK_WIDGET (object));
	
	g_value_reset (value);
	g_value_set_string (value,
			    tooltips_data ? tooltips_data->tip_text : NULL);
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

			if (gwidget_position > position)
				return position;
		}
	}
	return position;
}

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
	
	if (old_size == new_size)
		return;

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

		/* Refuse to remove any widgets that are either GladeWidget objects
		 * or internal to the hierarchic entity (may be a composite widget,
		 * not all internal widgets have GladeWidgets).
		 */
		if (glade_widget_get_from_gobject (child_widget) ||
		    GLADE_IS_PLACEHOLDER (child_widget) == FALSE)
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

void GLADEGTK_API
glade_gtk_notebook_get_n_pages (GObject *object, GValue *value)
{
	GtkNotebook *notebook;

	g_value_reset (value);

	notebook = GTK_NOTEBOOK (object);
	g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

	g_value_set_int (value, gtk_notebook_get_n_pages (notebook));
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
		GtkWidget *placeholder     = glade_placeholder_new ();
		GtkWidget *tab_placeholder = glade_placeholder_new ();

		gtk_notebook_insert_page (notebook, placeholder,
					  NULL, position);

		gtk_notebook_set_tab_label (notebook, placeholder, tab_placeholder);

		g_object_set_data (G_OBJECT (tab_placeholder), "page-num",
				   GINT_TO_POINTER (position));
		g_object_set_data (G_OBJECT (tab_placeholder), "special-child-type", "tab");
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

static void
glade_gtk_table_set_n_common (GObject *object, GValue *value, gboolean for_rows)
{
	GladeWidget *widget;
	GtkTable    *table;
	GList       *children, *placeholders, *l;
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

	/* Empty the old placeholders first */
	children = gtk_container_get_children (GTK_CONTAINER (table));
	for (placeholders = NULL, l = children; l; l = l->next)
		if (GLADE_IS_PLACEHOLDER (l->data))
			placeholders = g_list_prepend (placeholders, l->data);
	for (l = placeholders; l; l = l->next)
		gtk_container_remove (GTK_CONTAINER (table), GTK_WIDGET (l->data));

	if (children)
		g_list_free (children);
	if (placeholders)
		g_list_free (placeholders);

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

static gboolean
glade_gtk_button_ensure_glabel (GtkWidget *button)
{
	GladeWidgetClass *wclass;
	GladeWidget      *gbutton, *glabel;
	GtkWidget        *child;

	gbutton = glade_widget_get_from_gobject (button);

	/* If we didnt put this object here... (or its a placeholder) */
	if ((child = gtk_bin_get_child (GTK_BIN (button))) == NULL ||
	    (glade_widget_get_from_gobject (child) == NULL))
	{
		wclass = glade_widget_class_get_by_type (GTK_TYPE_LABEL);
		glabel = glade_widget_new (gbutton, wclass,
					   glade_widget_get_project (gbutton));

		glade_widget_property_set
			(glabel, "label", gbutton->widget_class->generic_name);

		if (child) gtk_container_remove (GTK_CONTAINER (button), child);
		gtk_container_add (GTK_CONTAINER (button), GTK_WIDGET (glabel->object));

		glade_project_add_object (GLADE_PROJECT (gbutton->project), glabel->object);
		gtk_widget_show (GTK_WIDGET (glabel->object));
	}

	glade_widget_property_set_sensitive 
		(gbutton, "stock", FALSE, 
		 _("There must be no children in the button"));
	return FALSE;
}


void GLADEGTK_API
glade_gtk_button_set_stock (GObject *object, GValue *value)
{
	GladeWidget   *gwidget;
	GEnumClass    *eclass;
	GEnumValue    *eval;
	gint           val;

	gwidget = glade_widget_get_from_gobject (object);
	g_return_if_fail (GTK_IS_BUTTON (object));
	g_return_if_fail (GLADE_IS_WIDGET (gwidget));

	val = g_value_get_enum (value);	
	if (val == GPOINTER_TO_INT (g_object_get_data (G_OBJECT (gwidget), "stock")))
		return;
	g_object_set_data (G_OBJECT (gwidget), "stock", GINT_TO_POINTER (val));

	eclass   = g_type_class_ref (G_VALUE_TYPE (value));
	if ((eval = g_enum_get_value (eclass, val)) != NULL)
	{
		/* setting to "none", ensure an appropriate label */
		if (val == 0)
		{
			glade_widget_property_set (gwidget, "use-stock", FALSE);
			glade_widget_property_set (gwidget, "label", NULL);

			glade_gtk_button_ensure_glabel (GTK_WIDGET (gwidget->object));
			glade_project_selection_set (GLADE_PROJECT (gwidget->project), 
						     G_OBJECT (gwidget->object), TRUE);
		}
		else
		{
			if (GTK_BIN (object)->child)
			{
				/* Here we would delete the coresponding GladeWidget from
				 * the project (like we created it and added it), but this
				 * screws up the undo stack, so instead we keep the stock
				 * button insensitive while ther are usefull children in the
				 * button.
				 */
				gtk_container_remove (GTK_CONTAINER (object), 
						      GTK_BIN (object)->child);
			}
			
			/* Here we should remove any previously added GladeWidgets manualy
			 * and from the project, not to leak them.
			 */
			glade_widget_property_set (gwidget, "use-stock", TRUE);
			glade_widget_property_set (gwidget, "label", eval->value_nick);
		}
	}
	g_type_class_unref (eclass);
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

static void
glade_gtk_image_backup_filename (GladeWidget *gwidget)
{
	gchar  *str;
	if (glade_widget_property_default (gwidget, "file") == FALSE)
	{
		if ((str = g_object_get_data (G_OBJECT (gwidget), 
					      "glade-file")) != NULL)
			g_free (str);
		glade_widget_property_get (gwidget, "file", &str);
		g_object_set_data (G_OBJECT (gwidget), "glade-file", g_strdup (str));
	}
}

static void
glade_gtk_image_disable_filename (GladeWidget *gwidget)
{
	glade_gtk_image_backup_filename (gwidget);
	glade_widget_property_reset (gwidget, "file");
	glade_widget_property_set_sensitive
		(gwidget, "file", FALSE,
		 _("This only applies with file type images"));
}

static void
glade_gtk_image_restore_filename (GladeWidget *gwidget)
{
	gchar  *str = g_object_get_data (G_OBJECT (gwidget), "glade-file");
	glade_widget_property_set_sensitive (gwidget, "file", 
					     TRUE, NULL);
	if (str) glade_widget_property_set (gwidget, "file", str);
}

static void
glade_gtk_image_backup_icon_name (GladeWidget *gwidget)
{
	gchar *str;
	if (glade_widget_property_default (gwidget, "icon-name") == FALSE)
	{
		if ((str = g_object_get_data (G_OBJECT (gwidget), 
					      "glade-icon-name")) != NULL)
			g_free (str);
		glade_widget_property_get (gwidget, "icon-name", &str);
		g_object_set_data (G_OBJECT (gwidget), "glade-icon-name", g_strdup (str));
	}
}

static void
glade_gtk_image_disable_icon_name (GladeWidget *gwidget)
{
	glade_gtk_image_backup_icon_name (gwidget);
	glade_widget_property_reset (gwidget, "icon-name");
	glade_widget_property_set_sensitive
		(gwidget, "icon-name", FALSE,
		 _("This only applies to Icon Theme type images"));
}

static void
glade_gtk_image_restore_icon_name (GladeWidget *gwidget)
{
	gchar *str = g_object_get_data (G_OBJECT (gwidget), "glade-icon-name");
	glade_widget_property_set_sensitive (gwidget, "icon-name", 
					     TRUE, NULL);
	if (str) glade_widget_property_set (gwidget, "icon-name", str);
}


static void
glade_gtk_image_backup_stock (GladeWidget *gwidget)
{
	gint stock_num;
	if (glade_widget_property_default (gwidget, "glade-stock") == FALSE)
	{
		glade_widget_property_get (gwidget, "glade-stock", &stock_num);
		g_object_set_data (G_OBJECT (gwidget), "glade-stock", 
				   GINT_TO_POINTER (stock_num));
	}
}

static void
glade_gtk_image_disable_stock (GladeWidget *gwidget)
{
	glade_gtk_image_backup_stock (gwidget);
	glade_widget_property_reset (gwidget, "glade-stock");
	glade_widget_property_set_sensitive
		(gwidget, "glade-stock", FALSE,
		 _("This only applies with stock type images"));
}

static void
glade_gtk_image_restore_stock (GladeWidget *gwidget)
{
	gint stock_num = GPOINTER_TO_INT
		(g_object_get_data (G_OBJECT (gwidget), "glade-stock"));
	glade_widget_property_set_sensitive (gwidget, "glade-stock", 
					     TRUE, NULL);
	glade_widget_property_set (gwidget, "glade-stock", stock_num);
}

void GLADEGTK_API
glade_gtk_image_set_type (GObject *object, GValue *value)
{
	GladeWidget       *gwidget;
	GladeGtkImageType  type;
	
	gwidget = glade_widget_get_from_gobject (object);
	g_return_if_fail (GTK_IS_IMAGE (object));
	g_return_if_fail (GLADE_IS_WIDGET (gwidget));

	/* Since this gets called at object construction
	 * (during the sync_custom_props phase), we need to make
	 * sure this isn't called before the post_create_idle
	 * function, otherwise file loading breaks due to the
	 * cascade of dependant properties in GtkImage.
	 */
	if (GPOINTER_TO_INT (g_object_get_data
			     (object, "glade-image-post-ran")) == 0)
		return;

	switch ((type = g_value_get_enum (value)))
	{
	case GLADEGTK_IMAGE_STOCK:
		glade_gtk_image_disable_filename (gwidget);
		glade_gtk_image_disable_icon_name (gwidget);
		glade_gtk_image_restore_stock (gwidget);
		break;

	case GLADEGTK_IMAGE_ICONTHEME:
		glade_gtk_image_disable_filename (gwidget);
		glade_gtk_image_disable_stock (gwidget);
		glade_gtk_image_restore_icon_name (gwidget);
		break;

	case GLADEGTK_IMAGE_FILENAME:
	default:
		glade_gtk_image_disable_stock (gwidget);
		glade_gtk_image_disable_icon_name (gwidget);
		glade_gtk_image_restore_filename (gwidget);
		break;
	}		
}

/* This basicly just sets the virtual "glade-stock" property
 * based on the image's "stock" property (for glade file loading purposes)
 */
void GLADEGTK_API
glade_gtk_image_set_real_stock (GObject *object, GValue *value)
{
	GladeWidget  *gwidget;
	GEnumClass   *eclass;
	GEnumValue   *eval;
	gchar        *str;
	gboolean      loaded = FALSE;

	gwidget = glade_widget_get_from_gobject (object);
	g_return_if_fail (GTK_IS_IMAGE (object));
	g_return_if_fail (GLADE_IS_WIDGET (gwidget));

	loaded = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (gwidget), "glade-loaded"));
	g_object_set_data (G_OBJECT (gwidget), "glade-loaded", GINT_TO_POINTER (TRUE));

	if ((str = g_value_dup_string (value)) != NULL)
	{
		if (loaded == FALSE)
		{
			eclass = g_type_class_ref (GLADE_TYPE_STOCK);
			if ((eval = g_enum_get_value_by_nick (eclass, str)) != NULL)
			{
				g_object_set_data (G_OBJECT (gwidget), "glade-stock", GINT_TO_POINTER (eval->value));
				glade_widget_property_set (gwidget, "glade-stock", eval->value);
			}
			g_type_class_unref (eclass);
		}

		/* Forward it along to the real property */
		g_object_set (object, "stock", str, NULL);
		g_free (str);
	} else {
		/* Forward it along to the real property */
		g_object_set (object, "stock", NULL, NULL);
	}
}

void GLADEGTK_API
glade_gtk_image_set_stock (GObject *object, GValue *value)
{
	GladeWidget   *gwidget;
	GEnumClass    *eclass;
	GEnumValue    *eval;
	gint           val;

	gwidget = glade_widget_get_from_gobject (object);
	g_return_if_fail (GTK_IS_IMAGE (object));
	g_return_if_fail (GLADE_IS_WIDGET (gwidget));

	val    = g_value_get_enum (value);	
	eclass = g_type_class_ref (G_VALUE_TYPE (value));
	if ((eval = g_enum_get_value (eclass, val)) != NULL)
	{
		if (val == 0)
			glade_widget_property_reset (gwidget, "stock");
		else
			glade_widget_property_set (gwidget, "stock", eval->value_nick);
			
	}
	g_type_class_unref (eclass);
}

/* This function does absolutely nothing
 * (and is for use in overriding post_create functions).
 */
void GLADEGTK_API
empty (GObject *container)
{
}

/* ------------------------- Post Create functions ------------------------- */
void GLADEGTK_API
glade_gtk_container_post_create (GObject *container, GladeCreateReason reason)
{
	GList *children;
	g_return_if_fail (GTK_IS_CONTAINER (container));

	if (reason == GLADE_CREATE_USER)
	{
		if ((children = gtk_container_get_children (GTK_CONTAINER (container))) == NULL)
			gtk_container_add (GTK_CONTAINER (container), glade_placeholder_new ());
		else
			g_list_free (children);
	}
}

void GLADEGTK_API
glade_gtk_paned_post_create (GObject *paned, GladeCreateReason reason)
{
	g_return_if_fail (GTK_IS_PANED (paned));

	if (reason == GLADE_CREATE_USER && gtk_paned_get_child1 (GTK_PANED (paned)) == NULL)
		gtk_paned_add1 (GTK_PANED (paned), glade_placeholder_new ());
	
	if (reason == GLADE_CREATE_USER && gtk_paned_get_child2 (GTK_PANED (paned)) == NULL)
		gtk_paned_add2 (GTK_PANED (paned), glade_placeholder_new ());
}

void
glade_gtk_fixed_layout_finalize(GdkPixmap *backing)
{
	g_object_unref(backing);
}

void
glade_gtk_fixed_layout_realize (GtkWidget *widget)
{
	GdkPixmap *backing =
		gdk_pixmap_colormap_create_from_xpm_d (NULL, gtk_widget_get_colormap (widget),
						       NULL, NULL, fixed_bg_xpm);

	if (GTK_IS_LAYOUT (widget))
		gdk_window_set_back_pixmap (GTK_LAYOUT (widget)->bin_window, backing, FALSE);
	else
		gdk_window_set_back_pixmap (widget->window, backing, FALSE);

	/* For cleanup later
	 */
	g_object_weak_ref(G_OBJECT(widget), (GWeakNotify)glade_gtk_fixed_layout_finalize, backing);
}

void GLADEGTK_API
glade_gtk_fixed_layout_post_create (GObject *object, GladeCreateReason reason)
{
	GladeWidget *fixed_layout = 
		glade_widget_get_from_gobject (object);

	/* Only once please */
	if (fixed_layout->manager != NULL) return;

	/* Only widgets with windows can recieve
	 * mouse events (I'm not sure if this is nescisary).
	 */
	GTK_WIDGET_UNSET_FLAGS(object, GTK_NO_WINDOW);

	/* For backing pixmap
	 */
	g_signal_connect_after(object, "realize",
			       G_CALLBACK(glade_gtk_fixed_layout_realize), NULL);

	/* Create fixed manager (handles resizes, drags, add/remove etc.).
	 */
	glade_fixed_manager_new
		(fixed_layout, "x", "y", "width-request", "height-request");

}

void GLADEGTK_API
glade_gtk_window_post_create (GObject *object, GladeCreateReason reason)
{
	GtkWindow *window = GTK_WINDOW (object);

	g_return_if_fail (GTK_IS_WINDOW (window));

	/* Chain her up first */
	glade_gtk_container_post_create (object, reason);

	gtk_window_set_default_size (window, 440, 250);
}

/*
 * XXX TODO: Support GtkMenu !
 */
void GLADEGTK_API
glade_gtk_menu_bar_post_create (GObject *object, GladeCreateReason reason)
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

void
glade_gtk_menu_bar_launch_editor (GObject *menubar)
{
	GtkWidget *dialog;

	dialog = gtk_dialog_new_with_buttons
		("Custom editor entry point", NULL,
		 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT |
		 GTK_DIALOG_NO_SEPARATOR,
		 GTK_STOCK_OK, GTK_RESPONSE_OK,
		 NULL);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
}

void GLADEGTK_API
glade_gtk_dialog_post_create (GObject *object, GladeCreateReason reason)
{
	GtkDialog   *dialog = GTK_DIALOG (object);
	GladeWidget *widget;
	GladeWidget *vbox_widget;
	GladeWidget *actionarea_widget;

	g_return_if_fail (GTK_IS_DIALOG (dialog));

	widget = glade_widget_get_from_gobject (GTK_WIDGET (dialog));
	if (!widget)
		return;

	/* create the GladeWidgets for internal children */
	vbox_widget = glade_widget_new_for_internal_child
		(widget, G_OBJECT(dialog->vbox), "vbox");
	g_assert (vbox_widget);


	actionarea_widget = glade_widget_new_for_internal_child
		(vbox_widget, G_OBJECT(dialog->action_area), "action_area");
	g_assert (actionarea_widget);

	/*
	if (GTK_IS_MESSAGE_DIALOG (dialog))
	{
		GList *children = 
			gtk_container_get_children (GTK_CONTAINER (actionarea_widget->object));

		if (children)
		{
			glade_widget_property_set (actionarea_widget, "size", g_list_length (children));
			g_list_free (children);
		}
	}
	else
	*/

	/* Only set these on the original create. */
	if (reason == GLADE_CREATE_USER)
	{
		if (GTK_IS_MESSAGE_DIALOG (dialog))
			glade_widget_property_set (vbox_widget, "size", 2);
		else
			glade_widget_property_set (vbox_widget, "size", 3);

		glade_widget_property_set (actionarea_widget, "size", 2);
		glade_widget_property_set (actionarea_widget, "layout-style", GTK_BUTTONBOX_END);
	}

	/* set a reasonable default size for a dialog */
	if (GTK_IS_MESSAGE_DIALOG (dialog))
		gtk_window_set_default_size (GTK_WINDOW (object), 400, 115);
	else
		gtk_window_set_default_size (GTK_WINDOW (dialog), 320, 260);
}

void GLADEGTK_API
glade_gtk_tree_view_post_create (GObject *object, GladeCreateReason reason)
{
	GtkWidget *tree_view = GTK_WIDGET (object);
	GtkTreeStore *store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

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

static gboolean
glade_gtk_frame_create_idle (gpointer data)
{
	GladeWidget       *gframe, *glabel;
	GtkWidget         *label, *frame;
	GladeWidgetClass  *wclass;

	g_return_val_if_fail (GLADE_IS_WIDGET (data), FALSE);
	g_return_val_if_fail (GTK_IS_FRAME (GLADE_WIDGET (data)->object), FALSE);
	gframe = GLADE_WIDGET (data);
	frame  = GTK_WIDGET (gframe->object);

	/* If we didnt put this object here... */
	if ((label = gtk_frame_get_label_widget (GTK_FRAME (frame))) == NULL ||
	    (glade_widget_get_from_gobject (label) == NULL))
	{
		wclass = glade_widget_class_get_by_type (GTK_TYPE_LABEL);
		glabel = glade_widget_new (gframe, wclass,
					   glade_widget_get_project (gframe));
		
		if (GTK_IS_ASPECT_FRAME (frame))
			glade_widget_property_set (glabel, "label", "aspect frame");
		else
			glade_widget_property_set (glabel, "label", "frame");

		g_object_set_data (glabel->object, "special-child-type", "label_item");
		gtk_frame_set_label_widget (GTK_FRAME (frame), GTK_WIDGET (glabel->object));

		glade_project_add_object (GLADE_PROJECT (gframe->project), glabel->object);
		gtk_widget_show (GTK_WIDGET (glabel->object));
	}
	return FALSE;
}

static gboolean
glade_gtk_expander_create_idle (gpointer data)
{
	GladeWidget       *gexpander, *glabel;
	GtkWidget         *label, *expander;
	GladeWidgetClass  *wclass;

	g_return_val_if_fail (GLADE_IS_WIDGET (data), FALSE);
	g_return_val_if_fail (GTK_IS_EXPANDER (GLADE_WIDGET (data)->object), FALSE);
	gexpander = GLADE_WIDGET (data);
	expander  = GTK_WIDGET (gexpander->object);

	/* If we didnt put this object here... */
	if ((label = gtk_expander_get_label_widget (GTK_EXPANDER (expander))) == NULL ||
	    (glade_widget_get_from_gobject (label) == NULL))
	{
		wclass = glade_widget_class_get_by_type (GTK_TYPE_LABEL);
		glabel = glade_widget_new (gexpander, wclass,
					   glade_widget_get_project (gexpander));
		
		glade_widget_property_set (glabel, "label", "expander");

		g_object_set_data (glabel->object, "special-child-type", "label_item");
		gtk_expander_set_label_widget (GTK_EXPANDER (expander), GTK_WIDGET (glabel->object));

		glade_project_add_object (GLADE_PROJECT (gexpander->project), glabel->object);
		gtk_widget_show (GTK_WIDGET (glabel->object));
	}
	return FALSE;
}

void GLADEGTK_API
glade_gtk_frame_post_create (GObject *frame, GladeCreateReason reason)
{
	GladeWidget *gframe;

	g_return_if_fail (GTK_IS_FRAME (frame));

	/* Wait to be filled */
	if (reason == GLADE_CREATE_USER)
	{
		glade_gtk_container_post_create (frame, reason);
		if ((gframe = glade_widget_get_from_gobject (frame)) != NULL)
			g_idle_add (glade_gtk_frame_create_idle, gframe);
	}
}

void GLADEGTK_API
glade_gtk_expander_post_create (GObject *expander, GladeCreateReason reason)
{
	GladeWidget *gexpander;

	g_return_if_fail (GTK_IS_EXPANDER (expander));

	/* Wait to be filled */
	if (reason == GLADE_CREATE_USER)
	{
		glade_gtk_container_post_create (expander, reason);
		if ((gexpander = glade_widget_get_from_gobject (expander)) != NULL)
			g_idle_add (glade_gtk_expander_create_idle, gexpander);
	}
}

/* Use the font-buttons launch dialog to actually set the font-name
 * glade property through the glade-command api.
 */
static void
glade_gtk_font_button_refresh_font_name (GtkFontButton  *button,
					 GladeWidget    *gbutton)
{
	GladeProperty *property;
	GValue         value = { 0, };
	
	if ((property =
	     glade_widget_get_property (gbutton, "font-name")) != NULL)
	{
		g_value_init (&value, G_TYPE_STRING);
		g_value_set_string (&value, 
				    gtk_font_button_get_font_name (button));
		
		glade_command_set_property  (property, &value);
		g_value_unset (&value);
	}
}

static void
glade_gtk_color_button_refresh_color (GtkColorButton  *button,
				      GladeWidget     *gbutton)
{
	GladeProperty *property;
	GdkColor       color = { 0, };
	GValue         value = { 0, };
	
	if ((property =
	     glade_widget_get_property (gbutton, "color")) != NULL)
	{
		g_value_init (&value, GDK_TYPE_COLOR);

		gtk_color_button_get_color (button, &color);
		g_value_set_boxed (&value, &color);
		
		glade_command_set_property  (property, &value);
		g_value_unset (&value);
	}
}

void GLADEGTK_API
glade_gtk_button_post_create (GObject *button, GladeCreateReason reason)
{
	gboolean     use_stock = FALSE;
	gchar       *label = NULL;
	GladeWidget *gbutton = 
		gbutton = glade_widget_get_from_gobject (button);
	GEnumValue  *eval;
	GEnumClass  *eclass;

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GLADE_IS_WIDGET (gbutton));

	if (GTK_IS_FONT_BUTTON (button))
		g_signal_connect
			(button, "font-set", 
			 G_CALLBACK (glade_gtk_font_button_refresh_font_name), gbutton);
	else if (GTK_IS_COLOR_BUTTON (button))
		g_signal_connect
			(button, "color-set", 
			 G_CALLBACK (glade_gtk_color_button_refresh_color), gbutton);


	if (GTK_IS_COLOR_BUTTON (button) ||
	    GTK_IS_FONT_BUTTON (button))
		return;

	eclass   = g_type_class_ref (GLADE_TYPE_STOCK);

	glade_widget_property_get (gbutton, "use-stock", &use_stock);
	if (use_stock)
	{
		glade_widget_property_get (gbutton, "label", &label);
		
		eval = g_enum_get_value_by_nick (eclass, label);
		g_object_set_data (G_OBJECT (gbutton), "stock", GINT_TO_POINTER (eval->value));

		if (label != NULL && strcmp (label, "glade-none") != 0)
			glade_widget_property_set (gbutton, "stock", eval->value);
	} 
	else if (reason == GLADE_CREATE_USER)
	{
		/* We need to use an idle function so as not to screw up
		 * the widget tree (i.e. the hierarchic order of widget creation
		 * needs to be parent first child last).
		 */
		g_idle_add ((GSourceFunc)glade_gtk_button_ensure_glabel, button);
		glade_project_selection_set (GLADE_PROJECT (gbutton->project), 
					     G_OBJECT (button), TRUE);
	}
	g_type_class_unref (eclass);
}


static void 
glade_gtk_image_pixel_size_changed (GladeProperty *property,
				    GValue        *value,
				    GladeWidget   *gimage)
{
	gint size = g_value_get_int (value);
	glade_widget_property_set_sensitive 
		(gimage, "icon-size", size < 0 ? TRUE : FALSE,
		 _("Pixel Size takes precedence over Icon Size; "
		   "if you want to use Icon Size, set Pixel size to -1"));
}

static gboolean
glade_gtk_image_post_create_idle (GObject *image)
{
	GladeWidget    *gimage = glade_widget_get_from_gobject (image);
	GladeProperty  *property;
	GEnumClass     *eclass;
	gint            size;

	g_return_val_if_fail (GTK_IS_IMAGE (image), FALSE);
	g_return_val_if_fail (GLADE_IS_WIDGET (gimage), FALSE);

	eclass   = g_type_class_ref (GLADE_TYPE_STOCK);

	glade_gtk_image_backup_stock (gimage);
	glade_gtk_image_backup_icon_name (gimage);
	glade_gtk_image_backup_filename (gimage);
	
	g_object_set_data (image, "glade-image-post-ran", GINT_TO_POINTER (1));

	if (glade_widget_property_default (gimage, "icon-name") == FALSE)
		glade_widget_property_set (gimage, "glade-type", GLADEGTK_IMAGE_ICONTHEME);
	else if (glade_widget_property_default (gimage, "stock") == FALSE)
		glade_widget_property_set (gimage, "glade-type", GLADEGTK_IMAGE_STOCK);
	else if (glade_widget_property_default (gimage, "file") == FALSE)
		glade_widget_property_set (gimage, "glade-type", GLADEGTK_IMAGE_FILENAME);
	else 
		glade_widget_property_reset (gimage, "glade-type");

	if ((property = glade_widget_get_property (gimage, "pixel-size")) != NULL)
	{
		glade_widget_property_get (gimage, "pixel-size", &size);
		if (size >= 0)
			glade_widget_property_set_sensitive 
				(gimage, "icon-size", FALSE,
				 _("Pixel Size takes precedence over Icon size"));
		g_signal_connect (G_OBJECT (property), "value-changed", 
				  G_CALLBACK (glade_gtk_image_pixel_size_changed),
				  gimage);
	}
	g_type_class_unref (eclass);
	return FALSE;
}


void GLADEGTK_API
glade_gtk_image_post_create (GObject *object, GladeCreateReason reason)
{
	g_return_if_fail (GTK_IS_IMAGE (object));

	g_idle_add ((GSourceFunc)glade_gtk_image_post_create_idle, object);
}


/* ------------------------ Replace child functions ------------------------ */
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

void GLADEGTK_API
glade_gtk_notebook_replace_child (GtkWidget *container,
				  GtkWidget *current,
				  GtkWidget *new)
{
	GtkNotebook *notebook;
	GtkWidget *page;
	GtkWidget *label;
	gint page_num;
	gchar *special_child_type;

	notebook = GTK_NOTEBOOK (container);

	special_child_type =
		g_object_get_data (G_OBJECT (current), "special-child-type");

	if (special_child_type && !strcmp (special_child_type, "tab"))
	{
		page_num = (gint) g_object_get_data (G_OBJECT (current),
						     "page-num");
		g_object_set_data (G_OBJECT (new), "page-num",
				   GINT_TO_POINTER (page_num));
		g_object_set_data (G_OBJECT (new), "special-child-type",
				   "tab");
		page = gtk_notebook_get_nth_page (notebook, page_num);
		gtk_notebook_set_tab_label (notebook, page, new);
		return;
	}

	page_num = gtk_notebook_page_num (notebook, current);
	if (page_num == -1)
	{
		g_warning ("GtkNotebookPage not found\n");
		return;
	}

	label = gtk_notebook_get_tab_label (notebook, current);

	/* label may be NULL if the label was not set explicitely;
	 * we probably sholud always craete our GladeWidget label
	 * and add set it as tab label, but at the moment we don't.
	 */
	if (label)
		gtk_widget_ref (label);

	gtk_notebook_remove_page (notebook, page_num);
	gtk_notebook_insert_page (notebook, new, label, page_num);

	if (label)
		gtk_widget_unref (label);

	/* There seems to be a GtkNotebook bug where if the page is
	 * not shown first it will not set the current page */
	gtk_widget_show (new);
	gtk_notebook_set_current_page (notebook, page_num);
}


void GLADEGTK_API
glade_gtk_frame_replace_child (GtkWidget *container,
			       GtkWidget *current,
			       GtkWidget *new)
{
	gchar *special_child_type;

	special_child_type =
		g_object_get_data (G_OBJECT (current), "special-child-type");

	if (special_child_type && !strcmp (special_child_type, "label_item"))
	{
		g_object_set_data (G_OBJECT (new), "special-child-type", "label_item");
		gtk_frame_set_label_widget (GTK_FRAME (container), new);
		return;
	}

	glade_gtk_container_replace_child (container, current, new);
}

void GLADEGTK_API
glade_gtk_expander_replace_child (GtkWidget *container,
				  GtkWidget *current,
				  GtkWidget *new)
{
	gchar *special_child_type;

	special_child_type =
		g_object_get_data (G_OBJECT (current), "special-child-type");

	if (special_child_type && !strcmp (special_child_type, "label_item"))
	{
		g_object_set_data (G_OBJECT (new), "special-child-type", "label_item");
		gtk_expander_set_label_widget (GTK_EXPANDER (container), new);
		return;
	}

	glade_gtk_container_replace_child (container, current, new);
}

void GLADEGTK_API
glade_gtk_button_replace_child (GtkWidget *container,
				GtkWidget *current,
				GtkWidget *new)
{
	GladeWidget *gbutton = glade_widget_get_from_gobject (container);

	g_return_if_fail (GLADE_IS_WIDGET (gbutton));
	glade_gtk_container_replace_child (container, current, new);

	if (GLADE_IS_PLACEHOLDER (new))
		glade_widget_property_set_sensitive (gbutton, 
						     "stock", 
						     TRUE, NULL);
	else
		glade_widget_property_set_sensitive 
			(gbutton, "stock", FALSE, 
			 _("There must be no children in the button"));
}

/* ---------------------- Get Internal Child functions ---------------------- */
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

void GLADEGTK_API
glade_gtk_radio_button_set_group (GObject *object, GValue *value)
{
	GtkRadioButton *button = GTK_RADIO_BUTTON (object);
	const char *name = g_value_get_string (value);

	/* FIXME: now what?  */
}

GLADEGTK_API void
glade_gtk_box_add_child (GObject *object, GObject *child)
{
	gint		 num_children;
	GladeWidget     *gbox   = glade_widget_get_from_gobject (object);
	GladeWidget     *gchild = glade_widget_get_from_gobject (child);

	gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));

	num_children = g_list_length (GTK_BOX (object)->children);

	glade_widget_property_set (gbox, "size", num_children);

	/* Packing props arent around when parenting during a glade_widget_dup() */
	if (gchild && gchild->packing_properties)
		glade_widget_pack_property_set (gchild, "position", 
						num_children - 1);
}

GLADEGTK_API void
glade_gtk_notebook_add_child (GObject *object, GObject *child)
{
	GtkNotebook 	*notebook;
	guint		num_page;
	GtkWidget	*last_page;
	GladeWidget	*gwidget;
	gchar		*special_child_type;
	
	notebook = GTK_NOTEBOOK (object);

	num_page = gtk_notebook_get_n_pages (notebook);

	/* last_page = gtk_notebook_get_nth_page (notebook, -1);
	   in glade-2 we never use tab-label, we generate a GtkLabel
	   and set it as the label widget (and in fact the tab-label internal
	   is doing more or less the same thing). If the last page has a NULL
	   label, _child_ is a label widget
	if (last_page && !gtk_notebook_get_tab_label (notebook, last_page)) */

	special_child_type = g_object_get_data (child, "special-child-type");
	if (special_child_type &&
	    !strcmp (special_child_type, "tab"))
	{
		last_page = gtk_notebook_get_nth_page (notebook, num_page - 1);
		g_object_set_data (child, "page-num",
				   GINT_TO_POINTER (num_page - 1));
		gtk_notebook_set_tab_label (notebook, last_page,
					    GTK_WIDGET (child));
	}
	else
	{
		gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));

		gwidget = glade_widget_get_from_gobject (object);
		glade_widget_property_set (gwidget, "pages", num_page + 1);
		
		gwidget = glade_widget_get_from_gobject (child);
		if (gwidget && gwidget->packing_properties)
			glade_widget_pack_property_set (gwidget, "position", num_page);
	}
}

GLADEGTK_API void
glade_gtk_button_add_child (GObject *object, GObject *child)
{
	GtkWidget *old;

	old = GTK_BIN (object)->child;
	if (old)
		gtk_container_remove (GTK_CONTAINER (object), old);
	
	gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));
}

GLADEGTK_API void
glade_gtk_frame_add_child (GObject *object, GObject *child)
{
	gchar *special_child_type;

	special_child_type = g_object_get_data (child, "special-child-type");

	if (special_child_type &&
	    !strcmp (special_child_type, "label_item"))
	{
		gtk_frame_set_label_widget (GTK_FRAME (object),
					    GTK_WIDGET (child));
	}
	else
	{
		gtk_container_add (GTK_CONTAINER (object),
				   GTK_WIDGET (child));
	}
}

GLADEGTK_API void
glade_gtk_expander_add_child (GObject *object, GObject *child)
{
	gchar *special_child_type;

	special_child_type = g_object_get_data (child, "special-child-type");
	if (special_child_type &&
	    !strcmp (special_child_type, "label_item"))
	{
		gtk_expander_set_label_widget (GTK_EXPANDER (object),
					       GTK_WIDGET (child));
	}
	else
	{
		gtk_container_add (GTK_CONTAINER (object),
				   GTK_WIDGET (child));
	}
}
