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
 *   Chema Celorio <chema@celorio.com>
 */

#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include "glade.h"
#include "glade-widget.h"
#include "glade-editor-property.h"

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

/* GtkBox */

void GLADEGTK_API
glade_gtk_box_set_child_property (GObject *container,
				  GObject *child,
				  const gchar *property_name,
				  GValue *value)
{
	GladeWidget *gbox;
	
	g_return_if_fail (GTK_IS_BOX (container));
	g_return_if_fail (GTK_IS_WIDGET (child));
	gbox = glade_widget_get_from_gobject (container);
	g_return_if_fail (GLADE_IS_WIDGET (gbox));
	g_return_if_fail (property_name != NULL || value != NULL);
	
	/* Chain Up */
	gtk_container_child_set_property (GTK_CONTAINER (container),
					  GTK_WIDGET (child),
					  property_name,
					  value);

	if (strcmp (property_name, "position") == 0)
	{
		gint position, size;
		
		position = g_value_get_int (value) + 1;
		glade_widget_property_get (gbox, "size", &size);

		if (size < position)
			glade_widget_property_set (gbox, "size", position);
	}
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

		g_object_ref (G_OBJECT (child_widget));
		gtk_container_remove (GTK_CONTAINER (box), child_widget);
		gtk_widget_destroy (child_widget);
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
glade_gtk_box_add_child (GObject *object, GObject *child)
{
	GladeWidget *gbox;
	GladeProject *project;
	
	g_return_if_fail (GTK_IS_BOX (object));
	g_return_if_fail (GTK_IS_WIDGET (child));
	
	gbox = glade_widget_get_from_gobject (object);
	project = glade_widget_get_project (gbox);

	/*
	  Try to remove the last placeholder if any, this way GtkBox`s size 
	  will not be changed.
	*/
	if (!GLADE_IS_PLACEHOLDER (child))
	{
		GList *l;
		GtkBox *box = GTK_BOX (object);
		
		for (l = g_list_last (box->children); l; l = g_list_previous (l))
		{
			GtkWidget *child_widget = ((GtkBoxChild *) (l->data))->widget;			
			if (GLADE_IS_PLACEHOLDER (child_widget))
			{
				gtk_container_remove (GTK_CONTAINER (box), child_widget);
				break;
			}
		}
	}
	
	gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));
	
	if (glade_project_is_loading (project))
	{
		GladeWidget *gchild;
		gint num_children;
		
		num_children = g_list_length (GTK_BOX (object)->children);
		glade_widget_property_set (gbox, "size", num_children);
		
		gchild = glade_widget_get_from_gobject (child);
		
		/* Packing props arent around when parenting during a glade_widget_dup() */
		if (gchild && gchild->packing_properties)
			glade_widget_pack_property_set (gchild, "position", num_children - 1);
	}
}

void GLADEGTK_API
glade_gtk_box_remove_child (GObject *object, GObject *child)
{
	GladeWidget *gbox;
	gint size;
	
	g_return_if_fail (GTK_IS_BOX (object));
	g_return_if_fail (GTK_IS_WIDGET (child));
	
	gbox = glade_widget_get_from_gobject (object);
	
	gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));

	glade_widget_property_get (gbox, "size", &size);
	glade_widget_property_set (gbox, "size", size);
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

/* GtkButton */

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
					   glade_widget_get_project (gbutton), FALSE);

		glade_widget_property_set
			(glabel, "label", gbutton->widget_class->generic_name);

		if (child) gtk_container_remove (GTK_CONTAINER (button), child);
		gtk_container_add (GTK_CONTAINER (button), GTK_WIDGET (glabel->object));

		glade_project_add_object (GLADE_PROJECT (gbutton->project), 
					  NULL, glabel->object);
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

static void
glade_gtk_image_backup_filename (GladeWidget *gwidget)
{
	GladeProperty *p;
	gchar *file;
	
	if (glade_widget_property_default (gwidget, "pixbuf") == FALSE)
	{
		p = glade_widget_get_property (gwidget, "pixbuf");
		
		file = glade_property_class_make_string_from_gvalue (p->class,
								     p->value);
		
		g_object_set_data_full (G_OBJECT (gwidget), "glade-file", file, g_free);
	}
}

static void
glade_gtk_image_disable_filename (GladeWidget *gwidget)
{
	glade_gtk_image_backup_filename (gwidget);
	glade_widget_property_set (gwidget, "pixbuf", NULL);
	glade_widget_property_set_sensitive
		(gwidget, "pixbuf", FALSE,
		 _("This only applies with file type images"));
}

static void
glade_gtk_image_restore_filename (GladeWidget *gwidget)
{
	GladeProperty *p;
	GValue *value;
	gchar *file;
	
	glade_widget_property_set_sensitive (gwidget, "pixbuf", TRUE, NULL);
	
	file = g_object_get_data (G_OBJECT (gwidget), "glade-file");
	p = glade_widget_get_property (gwidget, "pixbuf");
	value = glade_property_class_make_gvalue_from_string
		(p->class, file, gwidget->project);
	
	glade_property_set_value (p, value);
	
	g_value_unset (value);
	g_free (value);
}

static void
glade_gtk_image_backup_icon_name (GladeWidget *gwidget)
{
	gchar *str;
	if (glade_widget_property_default (gwidget, "icon-name") == FALSE)
	{
		glade_widget_property_get (gwidget, "icon-name", &str);
		g_object_set_data_full (G_OBJECT (gwidget), "glade-icon-name", 
					g_strdup (str), g_free);
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
glade_gtk_image_set_icon_name (GObject *object, GValue *value)
{
	GladeWidget *gimage;
	gint icon_size;
	
	g_return_if_fail (GTK_IS_IMAGE (object));
	gimage = glade_widget_get_from_gobject (object);
	g_return_if_fail (GLADE_IS_WIDGET (gimage));
	
	glade_widget_property_get (gimage, "icon-size", &icon_size);
	
	gtk_image_set_from_icon_name (GTK_IMAGE (object),
					g_value_get_string (value), 
					icon_size);
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
	gint          icon_size;

	g_return_if_fail (GTK_IS_IMAGE (object));
	gwidget = glade_widget_get_from_gobject (object);
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
	}
	
	/* Set the real property */
	glade_widget_property_get (gwidget, "icon-size", &icon_size);
	gtk_image_set_from_stock (GTK_IMAGE (object), str, icon_size);
	
	if (str) g_free (str);
}

void GLADEGTK_API
glade_gtk_image_set_stock (GObject *object, GValue *value)
{
	GladeWidget   *gwidget;
	GEnumClass    *eclass;
	GEnumValue    *eval;
	gint           val;

	g_return_if_fail (GTK_IS_IMAGE (object));
	gwidget = glade_widget_get_from_gobject (object);
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

void GLADEGTK_API
glade_gtk_combo_box_set_items (GObject *object, GValue *value)
{
	GtkComboBox *combo;
	gchar **split;
	gint    i;

	g_return_if_fail (GTK_IS_COMBO_BOX (object));

	combo = GTK_COMBO_BOX (object);

	/* Empty the combo box */
	gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (combo)));

	/* Refill the combo box */
	split = g_value_get_boxed (value);

	if (split)
		for (i = 0; split[i] != NULL; i++)
			if (split[i][0] != '\0')
				gtk_combo_box_append_text (combo, split[i]);
}

/* This function does absolutely nothing
 * (and is for use in overriding post_create functions).
 */
void GLADEGTK_API
empty (GObject *container, GladeCreateReason reason)
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

void GLADEGTK_API
glade_gtk_dialog_post_create (GObject *object, GladeCreateReason reason)
{
	GtkDialog    *dialog = GTK_DIALOG (object);
	GladeWidget  *widget;
	GladeWidget  *vbox_widget;
	GladeWidget  *actionarea_widget;

	g_return_if_fail (GTK_IS_DIALOG (dialog));

	widget = glade_widget_get_from_gobject (GTK_WIDGET (dialog));
	if (!widget)
		return;

	/* create the GladeWidgets for internal children */
	vbox_widget = glade_widget_new_for_internal_child 
		(widget, G_OBJECT(dialog->vbox), "vbox", "dialog", FALSE);
	g_assert (vbox_widget);


	actionarea_widget = glade_widget_new_for_internal_child
		(vbox_widget, G_OBJECT(dialog->action_area), "action_area", "dialog", FALSE);
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
					   glade_widget_get_project (gframe), FALSE);
		
		if (GTK_IS_ASPECT_FRAME (frame))
			glade_widget_property_set (glabel, "label", "aspect frame");
		else
			glade_widget_property_set (glabel, "label", "frame");

		g_object_set_data (glabel->object, "special-child-type", "label_item");
		gtk_frame_set_label_widget (GTK_FRAME (frame), GTK_WIDGET (glabel->object));

		glade_project_add_object (GLADE_PROJECT (gframe->project), 
					  NULL, glabel->object);
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
					   glade_widget_get_project (gexpander), FALSE);
		
		glade_widget_property_set (glabel, "label", "expander");

		g_object_set_data (glabel->object, "special-child-type", "label_item");
		gtk_expander_set_label_widget (GTK_EXPANDER (expander), 
					       GTK_WIDGET (glabel->object));

		glade_project_add_object (GLADE_PROJECT (gexpander->project), 
					  NULL, glabel->object);
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
	GladeWidget    *gimage;
	GladeProperty  *property;
	gint            size;

	g_return_val_if_fail (GTK_IS_IMAGE (image), FALSE);
	gimage = glade_widget_get_from_gobject (image);
	g_return_val_if_fail (GLADE_IS_WIDGET (gimage), FALSE);

	glade_gtk_image_backup_stock (gimage);
	glade_gtk_image_backup_icon_name (gimage);
	glade_gtk_image_backup_filename (gimage);
	
	g_object_set_data (image, "glade-image-post-ran", GINT_TO_POINTER (1));

	if (glade_widget_property_default (gimage, "icon-name") == FALSE)
		glade_widget_property_set (gimage, "glade-type", GLADEGTK_IMAGE_ICONTHEME);
	else if (glade_widget_property_default (gimage, "stock") == FALSE)
		glade_widget_property_set (gimage, "glade-type", GLADEGTK_IMAGE_STOCK);
	else if (glade_widget_property_default (gimage, "pixbuf") == FALSE)
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

	return FALSE;
}


void GLADEGTK_API
glade_gtk_image_post_create (GObject *object, GladeCreateReason reason)
{
	g_return_if_fail (GTK_IS_IMAGE (object));

	g_idle_add ((GSourceFunc)glade_gtk_image_post_create_idle, object);
}

void GLADEGTK_API
glade_gtk_combo_post_create (GObject *object, GladeCreateReason reason)
{
	GladeWidget  *gcombo, *gentry, *glist;

	g_return_if_fail (GTK_IS_COMBO (object));

	if ((gcombo = glade_widget_get_from_gobject (object)) == NULL)
		return;
	
	gentry = glade_widget_new_for_internal_child
		(gcombo, G_OBJECT (GTK_COMBO (object)->entry), "entry", "combo", FALSE);

	/* We mark this 'anarchist' since its outside of the heirarchy */
	glist  = glade_widget_new_for_internal_child
		(gcombo, G_OBJECT (GTK_COMBO (object)->list), "list", "combo", TRUE);

}

void GLADEGTK_API
glade_gtk_list_item_post_create (GObject *object, GladeCreateReason reason)
{
	GtkWidget *label;

	g_return_if_fail (GTK_IS_LIST_ITEM (object));

	label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 0, 1);

	gtk_container_add (GTK_CONTAINER (object), label);
	gtk_widget_show (label);
}

void GLADEGTK_API
glade_gtk_combo_box_post_create (GObject *object, GladeCreateReason reason)
{
	GtkCellRenderer *cell;
	GtkListStore *store;

	g_return_if_fail (GTK_IS_COMBO_BOX (object));

	/* Add store */
	store = gtk_list_store_new (1, G_TYPE_STRING);
	gtk_combo_box_set_model (GTK_COMBO_BOX (object), GTK_TREE_MODEL (store));
	g_object_unref (store);
	
	/* Add cell renderer */
	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (object), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (object), cell,
					"text", 0, NULL);
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
glade_gtk_container_add_child (GtkWidget *container,
			       GtkWidget *child)
{
	/* Get a placeholder out of the way before adding the child if its a GtkBin
	 */
	if (GTK_IS_BIN (container) && GTK_BIN (container)->child != NULL && 
	    GLADE_IS_PLACEHOLDER (GTK_BIN (container)->child))
		gtk_container_remove (GTK_CONTAINER (container), GTK_BIN (container)->child);

	gtk_container_add (GTK_CONTAINER (container), child);
}

void GLADEGTK_API
glade_gtk_container_remove_child (GtkWidget *container,
				  GtkWidget *child)
{
	gtk_container_remove (GTK_CONTAINER (container), child);

	/* If this is the last one, add a placeholder by default.
	 */
	if (gtk_container_get_children (GTK_CONTAINER (container)) == NULL)
		gtk_container_add (GTK_CONTAINER (container), glade_placeholder_new ());
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
	 * we probably should always craete our GladeWidget label
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
glade_gtk_dialog_get_internal_child (GtkDialog   *dialog,
				     const gchar *name,
				     GtkWidget **child)
{
	g_return_if_fail (GTK_IS_DIALOG (dialog));
	
	if (strcmp ("vbox", name) == 0)
		*child = dialog->vbox;
	else if (strcmp ("action_area", name) == 0)
		*child = dialog->action_area;
	else
		*child = NULL;
}

void GLADEGTK_API
glade_gtk_combo_get_internal_child (GtkCombo    *combo,
				    const gchar *name,
				    GtkWidget **child)
{
	g_return_if_fail (GTK_IS_COMBO (combo));
	
	if (strcmp ("list", name) == 0)
		*child = combo->list;
	else if (strcmp ("entry", name) == 0)
		*child = combo->entry;
	else
		*child = NULL;
}

GList * GLADEGTK_API
glade_gtk_combo_get_children (GtkCombo *combo)
{
	GList *list = NULL;

	g_return_val_if_fail (GTK_IS_COMBO (combo), NULL);

	list = glade_util_container_get_all_children (GTK_CONTAINER (combo));

	/* Ensure that we only return one 'combo->list' */
	if (g_list_find (list, combo->list) == NULL)
		list = g_list_append (list, combo->list);

	return list;
}

void GLADEGTK_API
glade_gtk_list_item_set_label (GObject *object, GValue *value)
{
	GtkWidget *label;

	g_return_if_fail (GTK_IS_LIST_ITEM (object));

	label = GTK_BIN (object)->child;

	gtk_label_set_text (GTK_LABEL (label), g_value_get_string (value));
}

void GLADEGTK_API
glade_gtk_list_item_get_label (GObject *object, GValue *value)
{
	GtkWidget *label;

	g_return_if_fail (GTK_IS_LIST_ITEM (object));

	label = GTK_BIN (object)->child;

	g_value_set_string (value, gtk_label_get_text (GTK_LABEL (label)));
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

/* GtkTable */

static gboolean
glade_gtk_table_has_child (GtkTable *table, guint left_attach, guint top_attach)
{
	GList *list;

	for (list = table->children; list && list->data; list = list->next)
	{
		GtkTableChild *child = list->data;
		
		if (left_attach >= child->left_attach && left_attach < child->right_attach &&
		    top_attach >= child->top_attach && top_attach < child->bottom_attach)
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
glade_gtk_table_refresh_placeholders (GtkTable *table)
{
	GList *list, *toremove = NULL;
	gint i, j;
	
	for (list = table->children; list && list->data; list = list->next)
	{
		GtkTableChild *child = list->data;
		
		if (GLADE_IS_PLACEHOLDER (child->widget))
			toremove = g_list_prepend (toremove, child->widget);
	}

	if (toremove)
	{
		for (list = toremove; list; list = list->next)
			gtk_container_remove (GTK_CONTAINER (table),
					      GTK_WIDGET (list->data));
		g_list_free (toremove);
	}

	for (i = 0; i < table->ncols; i++)
		for (j = 0; j < table->nrows; j++)
			if (glade_gtk_table_has_child (table, i, j) == FALSE)
				gtk_table_attach_defaults (table,
							   glade_placeholder_new (),
					 		   i, i + 1, j, j + 1);
}

void GLADEGTK_API
glade_gtk_table_add_child (GObject *object, GObject *child)
{
	g_return_if_fail (GTK_IS_TABLE (object));
	g_return_if_fail (GTK_IS_WIDGET (child));

	gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));
	
	glade_gtk_table_refresh_placeholders (GTK_TABLE (object));
}

void GLADEGTK_API
glade_gtk_table_remove_child (GObject *object, GObject *child)
{
	g_return_if_fail (GTK_IS_TABLE (object));
	g_return_if_fail (GTK_IS_WIDGET (child));
	
	gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
	
	glade_gtk_table_refresh_placeholders (GTK_TABLE (object));
}

void GLADEGTK_API
glade_gtk_table_replace_child (GtkWidget *container,
			       GtkWidget *current,
			       GtkWidget *new)
{
	g_return_if_fail (GTK_IS_TABLE (container));
	
	/* Chain Up */
	glade_gtk_container_replace_child (container, current, new);

	glade_gtk_table_refresh_placeholders (GTK_TABLE (container));
}

static void
glade_gtk_table_set_n_common (GObject *object, GValue *value, gboolean for_rows)
{
	GladeWidget *widget;
	GtkTable    *table;
	guint new_size, old_size;

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

	/* Fill table with placeholders */
	glade_gtk_table_refresh_placeholders (table);
	
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
				g_object_ref (G_OBJECT (list->data));
				gtk_container_remove (GTK_CONTAINER (table),
						      GTK_WIDGET(list->data));
				/* This placeholder is no longer valid, force destroy */
				gtk_widget_destroy (GTK_WIDGET(list->data));
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

void GLADEGTK_API
glade_gtk_table_set_child_property (GObject *container,
				    GObject *child,
				    const gchar *property_name,
				    GValue *value)
{
	g_return_if_fail (GTK_IS_TABLE (container));
	g_return_if_fail (GTK_IS_WIDGET (child));
	g_return_if_fail (property_name != NULL && value != NULL);

	/* Chain Up first */
	gtk_container_child_set_property (GTK_CONTAINER (container),
					  GTK_WIDGET (child),
					  property_name,
					  value);
	
	if (strcmp (property_name, "bottom-attach") == 0 ||
	    strcmp (property_name, "left-attach") == 0 ||
	    strcmp (property_name, "right-attach") == 0 ||
	    strcmp (property_name, "top-attach") == 0)
	{
		/* Refresh placeholders */
		glade_gtk_table_refresh_placeholders (GTK_TABLE (container));
	}
}

static gboolean
glade_gtk_table_verify_attach_common (GObject *object,
				      GValue *value,
				      guint *val,
				      const gchar *prop,
				      guint *prop_val,
				      const gchar *parent_prop,
				      guint *parent_val)
{
	GladeWidget *widget, *parent;
	
	widget = glade_widget_get_from_gobject (object);
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), TRUE);
	parent = glade_widget_get_parent (widget);
	g_return_val_if_fail (GLADE_IS_WIDGET (parent), TRUE);
	
	*val = g_value_get_uint (value);
	glade_widget_property_get (widget, prop, prop_val);
	glade_widget_property_get (parent, parent_prop, parent_val);

	return FALSE;
}

static gboolean
glade_gtk_table_verify_left_top_attach (GObject *object,
					GValue *value,
					const gchar *prop,
					const gchar *parent_prop)
{
	guint val, prop_val, parent_val;
	GladeWidget *widget;
	
	if (glade_gtk_table_verify_attach_common (object, value, &val,
						  prop, &prop_val,
						  parent_prop, &parent_val))
		return FALSE;
	
	if ((widget = glade_widget_get_from_gobject (object)))
	{
		GladeProject *project = glade_widget_get_project (widget);	
		if (project && glade_project_is_loading (project)) return TRUE;
	}
	
	if (val >= parent_val || val >= prop_val) return FALSE;
		
	return TRUE;
}

static gboolean
glade_gtk_table_verify_right_bottom_attach (GObject *object,
					    GValue *value,
					    const gchar *prop,
					    const gchar *parent_prop)
{
	guint val, prop_val, parent_val;
	
	if (glade_gtk_table_verify_attach_common (object, value, &val,
						  prop, &prop_val,
						  parent_prop, &parent_val))
		return FALSE;
	
	if (val <= prop_val || val > parent_val) return FALSE;
		
	return TRUE;
}

gboolean GLADEGTK_API
glade_gtk_table_verify_left_attach (GObject *object, GValue *value)
{
	return glade_gtk_table_verify_left_top_attach (object, 
						       value,
						       "right-attach",
						       "n-columns");
}

gboolean GLADEGTK_API
glade_gtk_table_verify_right_attach (GObject *object, GValue *value)
{
	return glade_gtk_table_verify_right_bottom_attach (object,
							   value,
							   "left-attach",
							   "n-columns");
}

gboolean GLADEGTK_API
glade_gtk_table_verify_top_attach (GObject *object, GValue *value)
{
	return glade_gtk_table_verify_left_top_attach (object, 
						       value,
						       "bottom-attach",
						       "n-rows");
}

gboolean GLADEGTK_API
glade_gtk_table_verify_bottom_attach (GObject *object, GValue *value)
{
	return glade_gtk_table_verify_right_bottom_attach (object,
							   value,
							   "top-attach",
							   "n-rows");
}

/* GtkMenu Support */

void GLADEGTK_API
glade_gtk_menu_shell_add_item (GObject *object, GObject *child)
{
	
	g_return_if_fail (GTK_IS_MENU_SHELL (object));
	g_return_if_fail (GTK_IS_MENU_ITEM (child));

	gtk_menu_shell_append (GTK_MENU_SHELL (object), GTK_WIDGET (child));
}


void GLADEGTK_API
glade_gtk_menu_shell_remove_item (GObject *object, GObject *child)
{
	g_return_if_fail (GTK_IS_MENU_SHELL (object));
	g_return_if_fail (GTK_IS_MENU_ITEM (child));
	
	gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
}

static gint
glade_gtk_menu_shell_get_item_position (GObject *container, GObject *child)
{
	gint position = 0;
	GList *list = GTK_MENU_SHELL (container)->children;
	
	while (list)
	{
		if (G_OBJECT (list->data) == child) break;
		
		list = list->next;
		position++;
	}
	
	return position;
}

void GLADEGTK_API
glade_gtk_menu_shell_get_child_property (GObject *container,
					 GObject *child,
					 const gchar *property_name,
					 GValue *value)
{
	gint position;
	
	g_return_if_fail (GTK_IS_MENU_SHELL (container));
	g_return_if_fail (GTK_IS_MENU_ITEM (child));
	
	if (strcmp (property_name, "position")) return;
		
	position = glade_gtk_menu_shell_get_item_position (container, child);
	
	g_value_set_int (value, position);
	
}

void GLADEGTK_API
glade_gtk_menu_shell_set_child_property (GObject *container,
					 GObject *child,
					 const gchar *property_name,
					 GValue *value)
{
	GladeWidget *gitem;
	gint position;
	
	g_return_if_fail (GTK_IS_MENU_SHELL (container));
	g_return_if_fail (GTK_IS_MENU_ITEM (child));
	g_return_if_fail (property_name != NULL || value != NULL);
	
	if (strcmp (property_name, "position"))
	{
		/* Chain Up */
		gtk_container_child_set_property (GTK_CONTAINER (container),
						  GTK_WIDGET (child),
						  property_name,
						  value);
		return;
	}
		
	gitem = glade_widget_get_from_gobject (child);
	g_return_if_fail (GLADE_IS_WIDGET (gitem));
	
	position = g_value_get_int (value);
	
	if (position < 0)
	{
		position = glade_gtk_menu_shell_get_item_position (container, child);
		g_value_set_int (value, position);
	}
	
	g_object_ref (child);
	gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET (child));
	gtk_menu_shell_insert (GTK_MENU_SHELL (container), GTK_WIDGET (child), position);
	g_object_unref (child);
}

GList * GLADEGTK_API
glade_gtk_menu_item_get_submenu (GObject *object)
{
	GList *list = NULL;
	GtkWidget *child;
	
	g_return_val_if_fail (GTK_IS_MENU_ITEM (object), NULL);
	
	child = gtk_menu_item_get_submenu (GTK_MENU_ITEM (object));
	
	if (child) list = g_list_append (list, child);
	
	return list;
}

void GLADEGTK_API
glade_gtk_menu_item_add_submenu (GObject *object, GObject *child)
{
	g_return_if_fail (GTK_IS_MENU_ITEM (object));
	g_return_if_fail (GTK_IS_MENU (child));
	
	if (GTK_IS_SEPARATOR_MENU_ITEM (object))
	{
		g_warning ("You shouldn't try to add a GtkMenu to a GtkSeparatorMenuItem");
		return;
	}
	
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (object), GTK_WIDGET (child));
}

void GLADEGTK_API
glade_gtk_menu_item_remove_submenu (GObject *object, GObject *child)
{
	g_return_if_fail (GTK_IS_MENU_ITEM (object));
	g_return_if_fail (GTK_IS_MENU (child));
	
	gtk_menu_item_remove_submenu (GTK_MENU_ITEM (object));
}

#define glade_return_if_re_entrancy(o,p,v) \
	if ((v) == GPOINTER_TO_INT (g_object_get_data (G_OBJECT (o), p))) return; g_object_set_data (G_OBJECT (o), p, GINT_TO_POINTER ((v)))
	
void GLADEGTK_API
glade_gtk_menu_item_post_create (GObject *object, GladeCreateReason reason)
{
	GladeWidget  *gitem, *gimage;

	g_return_if_fail (GTK_IS_MENU_ITEM (object));
	gitem = glade_widget_get_from_gobject (object);
	g_return_if_fail (GLADE_IS_WIDGET (gitem));

	if (GTK_IS_SEPARATOR_MENU_ITEM (object)) return;
	
	if (gtk_bin_get_child (GTK_BIN (object)) == NULL)
	{
		GtkWidget *label = gtk_label_new ("");
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_container_add (GTK_CONTAINER (object), label);
	}

	if (GTK_IS_IMAGE_MENU_ITEM (object))
	{
		gboolean use_stock;
	
		glade_widget_property_get (gitem, "use-stock", &use_stock);
		if (use_stock)
		{
			GEnumClass *eclass;
			GEnumValue *eval;
			gchar *label;
			
			glade_widget_property_get (gitem, "label", &label);
			
			eclass = g_type_class_ref (GLADE_TYPE_STOCK);
			eval = g_enum_get_value_by_nick (eclass, label);
			if (eval)
				glade_widget_property_set(gitem, "stock", eval->value);
			
			glade_widget_property_set (gitem, "use-underline", TRUE);
		}
		else
		{
			if (reason == GLADE_CREATE_USER)
			{
				GtkWidget *image = gtk_image_new ();
				gimage = glade_widget_new_for_internal_child
					(gitem, G_OBJECT (image), "image", "menu-item", FALSE);
				gtk_image_menu_item_set_image
					(GTK_IMAGE_MENU_ITEM (object), image);
				glade_gtk_image_post_create_idle (G_OBJECT (image));
			}
		}
	}
}

void GLADEGTK_API
glade_gtk_menu_item_set_label (GObject *object, GValue *value)
{
	GladeWidget *gitem;
	GtkWidget *label;
	gboolean use_underline, use_stock;
	const gchar *label_str, *last_label_str;

	g_return_if_fail (GTK_IS_MENU_ITEM (object));
	gitem = glade_widget_get_from_gobject (object);
	g_return_if_fail (GLADE_IS_WIDGET (gitem));
	
	if (GTK_IS_SEPARATOR_MENU_ITEM (object)) return;

	label_str = g_value_get_string (value);
	
	last_label_str = g_object_get_data (G_OBJECT (gitem), "label");
	if (last_label_str)
		if (strcmp(label_str, last_label_str) == 0) return;
	g_object_set_data_full (G_OBJECT (gitem), "label", g_strdup(label_str), g_free);

	if (GTK_IS_IMAGE_MENU_ITEM (object))
	{
		glade_widget_property_get (gitem, "use-stock", &use_stock);
		
		if (use_stock)
		{
			GtkWidget *image;
			GEnumClass *eclass;
			GEnumValue *eval;
			
			eclass = g_type_class_ref (GLADE_TYPE_STOCK);
			eval = g_enum_get_value_by_nick (eclass, label_str);
			
			if (eval)
			{
				label_str = eval->value_name;
			
				image = gtk_image_new_from_stock (eval->value_nick, GTK_ICON_SIZE_MENU);
				gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (object), image);
			}
			
			g_type_class_unref (eclass);
		}
	}
	
	label = gtk_bin_get_child (GTK_BIN (object));
	gtk_label_set_text (GTK_LABEL (label), label_str);
	
	glade_widget_property_get (gitem, "use-underline", &use_underline);
	gtk_label_set_use_underline (GTK_LABEL (label), use_underline);
}

void GLADEGTK_API
glade_gtk_menu_item_set_use_underline (GObject *object, GValue *value)
{
	GtkMenuItem *item;
	GtkWidget *label;

	g_return_if_fail (GTK_IS_MENU_ITEM (object));

	item = GTK_MENU_ITEM (object);

	if (GTK_IS_SEPARATOR_MENU_ITEM (item)) return;
	
	label = gtk_bin_get_child (GTK_BIN (item));

	gtk_label_set_use_underline (GTK_LABEL (label), g_value_get_boolean (value));
}

void GLADEGTK_API
glade_gtk_image_menu_item_get_internal_child (GObject *parent,
					      const gchar *name,
					      GObject **child)
{
	GtkWidget *image;
	
	if (GTK_IS_IMAGE_MENU_ITEM (parent) && strcmp (name, "image") == 0)
	{
		image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (parent));
		if (image == NULL)
		{
			GladeWidget  *gitem, *gimage;
			
			gitem = glade_widget_get_from_gobject (parent);
			image = gtk_image_new ();

			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (parent), image);
			
			gimage = glade_widget_new_for_internal_child
				(gitem, G_OBJECT (image), "image", "menu-item", FALSE);
		}
		*child = G_OBJECT (image);
		glade_gtk_image_post_create (G_OBJECT (image), GLADE_CREATE_LOAD);
	}
	else *child = NULL;

	return;
}

void GLADEGTK_API
glade_gtk_image_menu_item_set_use_stock (GObject *object, GValue *value)
{
	GladeWidget  *gitem, *gimage;
	gboolean      use_stock;
	GtkWidget    *image;
	
	g_return_if_fail (GTK_IS_IMAGE_MENU_ITEM (object));
	gitem = glade_widget_get_from_gobject (object);
	g_return_if_fail (GLADE_IS_WIDGET (gitem));

	use_stock = g_value_get_boolean (value);
	
	glade_return_if_re_entrancy (gitem, "use-stock", use_stock);

	if ((image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (object))))
		if(glade_widget_get_from_gobject (G_OBJECT(image)))
		{
			glade_project_remove_object (glade_widget_get_project (gitem), G_OBJECT(image));
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (object), NULL);
		}
	
	if (use_stock)
	{
		glade_widget_property_set_sensitive (gitem, "label", FALSE,
					_("This does not apply with stock items"));
	}
	else
	{
		image = gtk_image_new ();
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (object), image);
		gimage = glade_widget_new_for_internal_child
			(gitem, G_OBJECT (image), "image", "menu-item", FALSE);
		glade_project_add_object (glade_widget_get_project (gitem), 
					  NULL, G_OBJECT (image));
		glade_gtk_image_post_create_idle (G_OBJECT (image));
		glade_widget_property_set_sensitive (gitem, "label", TRUE, NULL);
	}
}

void GLADEGTK_API
glade_gtk_image_menu_item_set_stock (GObject *object, GValue *value)
{
	GladeWidget *gitem;
	GEnumClass *eclass;
	GEnumValue *eval;
	gint val;
	
	g_return_if_fail (GTK_IS_IMAGE_MENU_ITEM (object));
	gitem = glade_widget_get_from_gobject (object);
	g_return_if_fail (GLADE_IS_WIDGET (gitem));
	
	val = g_value_get_enum (value);
	
	glade_return_if_re_entrancy (gitem, "stock", val);
	
	glade_widget_property_set (gitem, "use-stock", val);
		
	eclass = g_type_class_ref (GLADE_TYPE_STOCK);
	eval = g_enum_get_value (eclass, val);
	if (eval && val)
		glade_widget_property_set (gitem, "label", eval->value_nick);
	
	g_type_class_unref (eclass);
}

/* RadioMenuItem */

void GLADEGTK_API
glade_gtk_radio_menu_item_set_group (GObject *object, GValue *value)
{
	GObject *val;
	
	g_return_if_fail (GTK_IS_RADIO_MENU_ITEM (object));
	
	if ((val = g_value_get_object (value)))
	{
		GSList *group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (val));
		
		if (! g_slist_find (group, GTK_RADIO_MENU_ITEM (object)))
			gtk_radio_menu_item_set_group (GTK_RADIO_MENU_ITEM (object), group);
	}
}

/* GtkMenuBar */

static GladeWidget * 
glade_gtk_menu_bar_append_new_submenu (GladeWidget *parent, GladeProject *project)
{
	static GladeWidgetClass *submenu_class = NULL;
	GladeWidget *gsubmenu;
	
	if (submenu_class == NULL)
		submenu_class = glade_widget_class_get_by_type (GTK_TYPE_MENU);

	gsubmenu = glade_widget_new (parent, submenu_class, project, FALSE);

	glade_widget_class_container_add (glade_widget_get_class (parent),
					  glade_widget_get_object (parent),
					  glade_widget_get_object (gsubmenu));
	
	glade_widget_set_parent (gsubmenu, parent);

	return gsubmenu;
}

static GladeWidget * 
glade_gtk_menu_bar_append_new_item (GladeWidget *parent,
				    GladeProject *project,
				    const gchar *label,
				    gboolean use_stock)
{
	static GladeWidgetClass *item_class = NULL, *image_item_class;
	GladeWidget *gitem;
	GObject *item;
	
	if (item_class == NULL)
	{
		item_class = glade_widget_class_get_by_type (GTK_TYPE_MENU_ITEM);
		image_item_class = glade_widget_class_get_by_type (GTK_TYPE_IMAGE_MENU_ITEM);
	}
	
	gitem = glade_widget_new (parent,
				  (use_stock) ? image_item_class : item_class,
				  project, FALSE);
	glade_widget_property_set (gitem, "use-underline", TRUE);
	
	if (use_stock)
	{
		GEnumClass *eclass;
		GEnumValue *eval;
		
		eclass = g_type_class_ref (GLADE_TYPE_STOCK);
		eval = g_enum_get_value_by_nick (eclass, label);
		
		if (eval)
			glade_widget_property_set (gitem, "stock", eval->value);
		
		g_type_class_unref (eclass);
	}
	else
	{
		glade_widget_property_set (gitem, "label", label);
	}
	
	item = glade_widget_get_object (gitem);
	
	glade_widget_class_container_add (glade_widget_get_class (parent),
					  glade_widget_get_object (parent),
					  item);
	
	glade_widget_set_parent (gitem, parent);
	
	return gitem;
}

void GLADEGTK_API
glade_gtk_menu_bar_post_create (GObject *object, GladeCreateReason reason)
{
	GladeProject *project;
	GladeWidget *gmenubar, *gitem, *gsubmenu;
	
	if (reason != GLADE_CREATE_USER) return;
	
	g_return_if_fail (GTK_IS_MENU_BAR (object));
	gmenubar = glade_widget_get_from_gobject (object);
	g_return_if_fail (GLADE_IS_WIDGET (gmenubar));
	
	project = glade_widget_get_project (gmenubar);
	
	/* File */
	gitem = glade_gtk_menu_bar_append_new_item (gmenubar, project, _("_File"), FALSE);	
	gsubmenu = glade_gtk_menu_bar_append_new_submenu (gitem, project);
	glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-quit", TRUE);

	/* Help */
	gitem = glade_gtk_menu_bar_append_new_item (gmenubar, project, _("_Help"), FALSE);	
	gsubmenu = glade_gtk_menu_bar_append_new_submenu (gitem, project);
	glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-about", TRUE);
}

/* Menu Editor */

typedef enum
{
	GLADEGTK_MENU_GWIDGET,
	GLADEGTK_MENU_OBJECT,
	GLADEGTK_MENU_TYPE_NAME,
	GLADEGTK_MENU_LABEL,
	GLADEGTK_MENU_TOOLTIP,
	GLADEGTK_MENU_N_COLUMNS
}GladeGtkMenuEditorEnum;

typedef struct _GladeGtkMenuEditor GladeGtkMenuEditor;

#define MENU_EDITOR_SEPARATOR_LABEL "<separator>"

struct _GladeGtkMenuEditor
{
	GtkWidget *window, *popup, *table, *child_table, *treeview;
	GtkWidget *remove_button, *undo_button, *redo_button;
	GtkTreeStore *store;
	GladeWidget *gmenubar;
	GladeSignalEditor *signal_editor;
	GladeProject *project;
	
	/* Temporal variables used in idle functions */
	GtkTreeIter iter;
};

static const gchar *
glade_gtk_menu_editor_type_name (GType type)
{

	if (type == GTK_TYPE_MENU_ITEM)
		return _("Normal");
	
	if (type == GTK_TYPE_IMAGE_MENU_ITEM)
		return _("Image");
		
	if (type == GTK_TYPE_CHECK_MENU_ITEM)
		return _("Check");

	if (type == GTK_TYPE_RADIO_MENU_ITEM)
		return _("Radio");
	
	if (type == GTK_TYPE_SEPARATOR_MENU_ITEM)
		return _("Separator");
	
	return "";
}

static void
glade_gtk_menu_editor_fill_store (GtkWidget *widget,
				  GtkTreeStore *tree_store,
				  GtkTreeIter *parent)
{
	GList *menu_items = (GTK_MENU_SHELL(widget))->children;

	while(menu_items)
	{
		GtkTreeIter iter;
		GtkWidget *submenu;
		GtkWidget *menu_item = GTK_WIDGET (((GtkMenuItem *)menu_items->data));
		
		if(menu_item)
		{
			GladeWidget *gitem;
			GObject *item;
			gchar *label, *tooltip;
			
			item = G_OBJECT (menu_item);
			gitem = glade_widget_get_from_gobject (item);
			
			gtk_tree_store_append (tree_store, &iter, parent);
			
			if (GTK_IS_SEPARATOR_MENU_ITEM (item))
			{
				gtk_tree_store_set (tree_store, &iter,
					GLADEGTK_MENU_GWIDGET, gitem,
					GLADEGTK_MENU_OBJECT, item,
					GLADEGTK_MENU_TYPE_NAME, glade_gtk_menu_editor_type_name (G_OBJECT_TYPE (item)),
					GLADEGTK_MENU_LABEL, MENU_EDITOR_SEPARATOR_LABEL,
					GLADEGTK_MENU_TOOLTIP, NULL,
					-1);
			}
			else
			{
				glade_widget_property_get (gitem, "label", &label);
				glade_widget_property_get (gitem, "tooltip", &tooltip);
				
				gtk_tree_store_set (tree_store, &iter,
					GLADEGTK_MENU_GWIDGET, gitem,
					GLADEGTK_MENU_OBJECT, item,
					GLADEGTK_MENU_TYPE_NAME, glade_gtk_menu_editor_type_name (G_OBJECT_TYPE (item)),
					GLADEGTK_MENU_LABEL, label,
					GLADEGTK_MENU_TOOLTIP, tooltip,
					-1);
			}
		}
		
		submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (menu_item));
		if (submenu) glade_gtk_menu_editor_fill_store (submenu, tree_store, &iter);

		menu_items = g_list_next (menu_items);
	}
}

typedef enum
{
	GLADEGTK_MENU_ITEM_CLASS,
	GLADEGTK_MENU_ITEM_NAME,
	GLADEGTK_MENU_ITEM_N_COLUMNS
}GladeGtkMenuItemEnum;

static
GtkTreeModel *
glade_gtk_menu_editor_get_item_model ()
{
	static GtkListStore *store = NULL;
	
	if (store == NULL)
	{
		GtkTreeIter iter;
		
		store = gtk_list_store_new (GLADEGTK_MENU_ITEM_N_COLUMNS,
					    G_TYPE_POINTER,
					    G_TYPE_STRING);
		
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    GLADEGTK_MENU_ITEM_CLASS, glade_widget_class_get_by_type (GTK_TYPE_MENU_ITEM),
				    GLADEGTK_MENU_ITEM_NAME, glade_gtk_menu_editor_type_name (GTK_TYPE_MENU_ITEM),
				    -1);
		
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    GLADEGTK_MENU_ITEM_CLASS, glade_widget_class_get_by_type (GTK_TYPE_IMAGE_MENU_ITEM),
				    GLADEGTK_MENU_ITEM_NAME, glade_gtk_menu_editor_type_name (GTK_TYPE_IMAGE_MENU_ITEM),
				    -1);
		
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    GLADEGTK_MENU_ITEM_CLASS, glade_widget_class_get_by_type (GTK_TYPE_CHECK_MENU_ITEM),
				    GLADEGTK_MENU_ITEM_NAME, glade_gtk_menu_editor_type_name (GTK_TYPE_CHECK_MENU_ITEM),
				    -1);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    GLADEGTK_MENU_ITEM_CLASS, glade_widget_class_get_by_type (GTK_TYPE_RADIO_MENU_ITEM),
				    GLADEGTK_MENU_ITEM_NAME, glade_gtk_menu_editor_type_name (GTK_TYPE_RADIO_MENU_ITEM),
				    -1);
		
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    GLADEGTK_MENU_ITEM_CLASS, glade_widget_class_get_by_type (GTK_TYPE_SEPARATOR_MENU_ITEM),
				    GLADEGTK_MENU_ITEM_NAME, glade_gtk_menu_editor_type_name (GTK_TYPE_SEPARATOR_MENU_ITEM),
				    -1);
	}
	
	return GTK_TREE_MODEL (store);
}

static gboolean
glade_gtk_menu_editor_get_item_selected (GladeGtkMenuEditor *e, GtkTreeIter *iter)
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (e->treeview));
	return gtk_tree_selection_get_selected (sel, NULL, iter);
}

static void
glade_gtk_menu_editor_string_value_changed (GladeGtkMenuEditor *e, GValue *value, gint column)
{
	GtkTreeIter iter;
	const gchar *text;
	
	if (glade_gtk_menu_editor_get_item_selected (e, &iter))
	{
		text = g_value_get_string (value);
		gtk_tree_store_set (e->store, &iter, column, text, -1);
	}
}

static void
glade_gtk_menu_editor_label_changed (GladeProperty *property,
				     GValue *value,
				     GladeGtkMenuEditor *e)
{
	glade_gtk_menu_editor_string_value_changed (e, value, GLADEGTK_MENU_LABEL);
}

static void
glade_gtk_menu_editor_tooltip_changed (GladeProperty *property,
				       GValue *value,
				       GladeGtkMenuEditor *e)
{
	glade_gtk_menu_editor_string_value_changed (e, value, GLADEGTK_MENU_TOOLTIP);
}

static void
glade_gtk_menu_editor_name_activate (GtkEntry *entry, GladeWidget *gitem)
{
	const gchar *text = gtk_entry_get_text (GTK_ENTRY (entry));
	
	if (strcmp (glade_widget_get_name (gitem), text))
		glade_command_set_name (gitem, text);
}

static gboolean
glade_gtk_menu_editor_name_focus_out (GtkWidget *entry,
				      GdkEventFocus *event,
				      GladeWidget *gitem)
{
	glade_gtk_menu_editor_name_activate (GTK_ENTRY (entry), gitem);
	return FALSE;
}

static void
glade_gtk_menu_editor_item_change_type (GladeGtkMenuEditor *e,
					GtkTreeIter *iter,
					GladeWidgetClass *klass,
					const gchar *klass_name);

static void
glade_gtk_menu_editor_type_changed (GtkComboBox *widget, GladeGtkMenuEditor *e)
{
	GtkTreeIter iter, combo_iter;
	GladeWidgetClass *klass;
	gchar *klass_name;
	
	if (! glade_gtk_menu_editor_get_item_selected (e, &iter))
		return;
	
	gtk_combo_box_get_active_iter (widget, &combo_iter);
	
	gtk_tree_model_get (gtk_combo_box_get_model (widget), &combo_iter,
				GLADEGTK_MENU_ITEM_CLASS, &klass,
				GLADEGTK_MENU_ITEM_NAME, &klass_name, -1);
	
	glade_gtk_menu_editor_item_change_type (e, &iter, klass, klass_name);
	
	g_free (klass_name);
}

static void
glade_gtk_menu_editor_remove_widget (GtkWidget *widget, gpointer container)
{
	gtk_container_remove (GTK_CONTAINER (container), widget);
}

static void
glade_gtk_menu_editor_table_attach (GtkWidget *table,
				    GtkWidget *child1,
				    GtkWidget *child2,
				    gint *row_ptr)
{
	gint row = *row_ptr;
	
	gtk_table_attach (GTK_TABLE (table), child1, 0, 1, row, row + 1,
			  GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 0);
	
	gtk_table_attach (GTK_TABLE (table), child2, 1, 2, row, row + 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 0);

	(*row_ptr)++;
}

static void
glade_gtk_menu_editor_treeview_fill_child_table (GladeGtkMenuEditor *e, GladeWidget *internal)
{
	GtkWidget *label, *table;
	GladeEditorProperty *eprop;
	gint row = 1;
	
	table = e->child_table;
	
	label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (label), _("<span rise=\"-20000\"><b>Internal Image Properties</b></span>"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label, 0, 2, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 0);
			
	eprop = glade_editor_property_new_from_widget (internal, "glade-type", TRUE);
	if (eprop)
		glade_gtk_menu_editor_table_attach (table, eprop->eventbox, GTK_WIDGET (eprop), &row);
	
	eprop = glade_editor_property_new_from_widget (internal, "pixbuf", TRUE);
	if (eprop)
		glade_gtk_menu_editor_table_attach (table, eprop->eventbox, GTK_WIDGET (eprop), &row);

	eprop = glade_editor_property_new_from_widget (internal, "glade-stock", TRUE);
	if (eprop)
		glade_gtk_menu_editor_table_attach (table, eprop->eventbox, GTK_WIDGET (eprop), &row);

	eprop = glade_editor_property_new_from_widget (internal, "icon-name", TRUE);
	if (eprop)
		glade_gtk_menu_editor_table_attach (table, eprop->eventbox, GTK_WIDGET (eprop), &row);
	
	gtk_widget_show_all (table);
}

static void
glade_gtk_menu_editor_use_stock_changed (GladeProperty *property,
					 GValue *value,
					 GladeGtkMenuEditor *e)
{
	GtkTreeIter iter;
	GtkWidget *image;
	GObject *item;
	GladeWidget *internal;

	gtk_container_foreach (GTK_CONTAINER (e->child_table),
			       glade_gtk_menu_editor_remove_widget, e->child_table);
	
	if (! glade_gtk_menu_editor_get_item_selected (e, &iter))
		return;

	gtk_tree_model_get (GTK_TREE_MODEL (e->store), &iter,
			    GLADEGTK_MENU_OBJECT, &item, -1);
	
	if ((image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (item))))
	{
		if ((internal = glade_widget_get_from_gobject (image)))
			if (internal->internal)
				glade_gtk_menu_editor_treeview_fill_child_table (e, internal);
	}
}

static void
glade_gtk_menu_editor_eprop_destroyed (GtkWidget *object, gpointer data)
{
	/* This will disconnect signals attatched to the property by the menu-editor */
	g_signal_handlers_disconnect_matched (GLADE_EDITOR_PROPERTY (object)->property,
					      G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, data);
}

static void
glade_gtk_menu_editor_clear (GladeGtkMenuEditor *e)
{
	gtk_container_foreach (GTK_CONTAINER (e->table),
			       glade_gtk_menu_editor_remove_widget, e->table);
	gtk_container_foreach (GTK_CONTAINER (e->child_table),
			       glade_gtk_menu_editor_remove_widget, e->child_table);

	gtk_widget_set_sensitive (e->remove_button, FALSE);
	glade_signal_editor_load_widget (e->signal_editor, NULL);
}

static void
glade_gtk_menu_editor_treeview_cursor_changed (GtkTreeView *treeview,
					       GladeGtkMenuEditor *e)
{
	GtkTreeIter iter, combo_iter;
	GtkWidget *label, *entry;
	GtkTreeModel *item_class;
	GtkCellRenderer *renderer;
	GObject *item;
	GladeWidgetClass *item_klass, *klass;
	GladeEditorProperty *eprop;
	GladeWidget *gitem;
	gint row = 0;

	if (! glade_gtk_menu_editor_get_item_selected (e, &iter))
		return;

	glade_gtk_menu_editor_clear (e);
	gtk_widget_set_sensitive (e->remove_button, TRUE);
	
	gtk_tree_model_get (GTK_TREE_MODEL (e->store), &iter,
			    GLADEGTK_MENU_GWIDGET, &gitem,
			    GLADEGTK_MENU_OBJECT, &item, -1);

	/* Name */
	label = gtk_label_new (_("Name :"));
	gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.0);
	
	entry = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry), glade_widget_get_name (gitem));
	g_signal_connect (entry, "activate", G_CALLBACK (glade_gtk_menu_editor_name_activate), gitem);
	g_signal_connect (entry, "focus-out-event", G_CALLBACK (glade_gtk_menu_editor_name_focus_out), gitem);
	glade_gtk_menu_editor_table_attach (e->table, label, entry, &row);

	/* Type */
	label = gtk_label_new (_("Type :"));
	gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.0);
	
	entry = gtk_combo_box_new ();
	item_class = glade_gtk_menu_editor_get_item_model ();
	gtk_combo_box_set_model (GTK_COMBO_BOX (entry), item_class);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT(entry), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT(entry), renderer, "text",
					GLADEGTK_MENU_ITEM_NAME, NULL);
					
	item_klass = glade_widget_class_get_by_type (G_OBJECT_TYPE (item));
	gtk_tree_model_get_iter_first (item_class, &combo_iter);
	do
	{
		gtk_tree_model_get (item_class, &combo_iter,
				    GLADEGTK_MENU_ITEM_CLASS, &klass, -1);
		if (item_klass == klass)
		{
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (entry), 
							&combo_iter);
			break;
		}
		
	} while (gtk_tree_model_iter_next (item_class, &combo_iter));
	
	g_signal_connect (entry, "changed", G_CALLBACK (glade_gtk_menu_editor_type_changed), e);
	glade_gtk_menu_editor_table_attach (e->table, label, entry, &row);
	
	if (! GTK_IS_SEPARATOR_MENU_ITEM (item))
	{
		/* Label */
		eprop = glade_editor_property_new_from_widget (gitem, "label", TRUE);
		if (eprop)
		{
			glade_gtk_menu_editor_table_attach (e->table, eprop->eventbox, GTK_WIDGET (eprop), &row);
			g_signal_connect (eprop->property, "value-changed", 
					  G_CALLBACK (glade_gtk_menu_editor_label_changed), e);
			g_signal_connect (eprop, "destroy",
					  G_CALLBACK (glade_gtk_menu_editor_eprop_destroyed), e);
		}
	
		/* Tooltip */
		eprop = glade_editor_property_new_from_widget (gitem, "tooltip", TRUE);
		if (eprop)
		{
			glade_gtk_menu_editor_table_attach (e->table, eprop->eventbox, GTK_WIDGET (eprop), &row);
			g_signal_connect (eprop->property, "value-changed",
					  G_CALLBACK (glade_gtk_menu_editor_tooltip_changed), e);
			g_signal_connect (eprop, "destroy",
					  G_CALLBACK (glade_gtk_menu_editor_eprop_destroyed), e);
		}
	}
	
	if (GTK_IS_IMAGE_MENU_ITEM (item))
	{		
		eprop = glade_editor_property_new_from_widget (gitem, "stock", TRUE);
		if (eprop)
			glade_gtk_menu_editor_table_attach (e->table, eprop->eventbox, GTK_WIDGET (eprop), &row);

		eprop = glade_editor_property_new_from_widget (gitem, "use-stock", FALSE);
		if (eprop)
		{
			g_signal_connect (eprop->property, "value-changed",
					  G_CALLBACK (glade_gtk_menu_editor_use_stock_changed), e);
			g_signal_connect (eprop, "destroy",
					  G_CALLBACK (glade_gtk_menu_editor_eprop_destroyed), e);
		}

		/* Internal child properties */
		glade_gtk_menu_editor_use_stock_changed (NULL, NULL, e);
	}
	
	if (GTK_IS_CHECK_MENU_ITEM (item))
	{
		eprop = glade_editor_property_new_from_widget (gitem, "active", TRUE);
		if (eprop)
			glade_gtk_menu_editor_table_attach (e->table, eprop->eventbox, GTK_WIDGET (eprop), &row);
	
		if (GTK_IS_RADIO_MENU_ITEM (item))
		{
			eprop = glade_editor_property_new_from_widget (gitem, "group", TRUE);
			if (eprop)
				glade_gtk_menu_editor_table_attach (e->table, eprop->eventbox, GTK_WIDGET (eprop), &row);
		}
		else
		{
			eprop = glade_editor_property_new_from_widget (gitem, "draw-as-radio", TRUE);
			if (eprop)
				glade_gtk_menu_editor_table_attach (e->table, eprop->eventbox, GTK_WIDGET (eprop), &row);
	
			eprop = glade_editor_property_new_from_widget (gitem, "inconsistent", TRUE);
			if (eprop)
				glade_gtk_menu_editor_table_attach (e->table, eprop->eventbox, GTK_WIDGET (eprop), &row);
		}
	}
	
	/* Update Signal Editor*/
	glade_signal_editor_load_widget (e->signal_editor, gitem);
	
	gtk_widget_show_all (e->table);
}


static void
glade_gtk_menu_editor_reorder_children (GtkWidget *menushell,
					GtkTreeModel *model,
					GtkTreeIter *child)
{
	GladeWidget   *gitem;
	GladeProperty *property;
	GtkTreeIter parent, iter;
        GValue val = {0, };
	gint position = 0;

	if (gtk_tree_model_iter_parent (model, &parent, child))
		gtk_tree_model_iter_children (model, &iter, &parent);
	else
		gtk_tree_model_get_iter_first (model, &iter);
	
        g_value_init (&val, G_TYPE_INT);

	do
	{
		gtk_tree_model_get (model, &iter, GLADEGTK_MENU_GWIDGET, &gitem, -1);
                g_value_set_int (&val, position++);

		if ((property = glade_widget_get_property (gitem, "position")) != NULL)
			glade_command_set_property (property, &val);
	} while (gtk_tree_model_iter_next (model, &iter));
}

static void
glade_gtk_menu_editor_set_cursor (GladeGtkMenuEditor *e, GtkTreeIter *iter)
{
	GtkTreePath *path;

	if ((path = gtk_tree_model_get_path (GTK_TREE_MODEL (e->store), iter)))
	{
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (e->treeview), path, NULL, FALSE);
		gtk_tree_path_free (path);
	}
}

static gboolean
glade_gtk_menu_editor_find_child_real (GladeGtkMenuEditor *e,
				       GladeWidget *child,
				       GtkTreeIter *iter)
{
	GtkTreeModel *model = GTK_TREE_MODEL (e->store);
	GtkTreeIter child_iter;
	GladeWidget *item;
	
	do
	{
		gtk_tree_model_get (model, iter, GLADEGTK_MENU_GWIDGET, &item, -1);
	
		if (item == child) return TRUE;

		if (gtk_tree_model_iter_children (model, &child_iter, iter))
			if (glade_gtk_menu_editor_find_child_real (e, child, &child_iter))
			{
				*iter = child_iter;
				return TRUE;
			}
	}
	while (gtk_tree_model_iter_next (model, iter));
	
	return FALSE;
}

static gboolean
glade_gtk_menu_editor_find_child (GladeGtkMenuEditor *e,
				  GladeWidget *child,
				  GtkTreeIter *iter)
{
	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (e->store), iter))
		return glade_gtk_menu_editor_find_child_real (e, child, iter);
	
	return FALSE;
}

static void
glade_gtk_menu_editor_select_child (GladeGtkMenuEditor *e,
				    GladeWidget *child)
{
	GtkTreeIter iter;
	
	if (glade_gtk_menu_editor_find_child (e, child, &iter))
		glade_gtk_menu_editor_set_cursor (e, &iter);
}

static void
glade_gtk_menu_editor_block_callbacks (GladeGtkMenuEditor *e, gboolean block);

static void
glade_gtk_menu_editor_item_change_type (GladeGtkMenuEditor *e,
					GtkTreeIter *iter,
					GladeWidgetClass *klass,
					const gchar *klass_name)
{
	GladeWidget *parent, *gitem, *gitem_new;
	GObject *item, *item_new;
	gchar *name, *label, *tooltip, *desc;
	GtkWidget *submenu;
	GList list = {0, };
	
	glade_gtk_menu_editor_block_callbacks (e, TRUE);
	
	/* Get old widget data */
	gtk_tree_model_get (GTK_TREE_MODEL (e->store), iter,
			    GLADEGTK_MENU_GWIDGET, &gitem,
			    GLADEGTK_MENU_OBJECT, &item,
			    GLADEGTK_MENU_LABEL, &label,
			    GLADEGTK_MENU_TOOLTIP, &tooltip,
			    -1);
	parent = glade_widget_get_parent (gitem);
	name = g_strdup (glade_widget_get_name (gitem));
	submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (item));

	/* Start of glade-command */
	desc = g_strdup_printf (_("Setting menu item type on %s to %s"),
				name, klass_name);
	glade_command_push_group (desc);
	g_free (desc);
	
	/* Create new widget */
	gitem_new = glade_command_create (klass, parent, NULL, e->project);
	item_new = glade_widget_get_object (gitem_new);
	glade_widget_set_name (gitem_new, name);
	
	if (GTK_IS_SEPARATOR_MENU_ITEM (item_new))
	{
		gtk_tree_store_set (e->store, iter,
			    GLADEGTK_MENU_LABEL, MENU_EDITOR_SEPARATOR_LABEL,
			    GLADEGTK_MENU_TOOLTIP, NULL,
			    -1);
		if (submenu)
		{
			list.data = glade_widget_get_from_gobject (submenu);
			glade_command_delete (&list);
		}

	}
	else
	{
		/* FIXME: Scrap this, write generic code that will 
		 * loop over common GladeProperties and set them accordingly
		 * (other widget properties can be set through the normal
		 * editor/project-view selection, no need to loose these values).
		 */

		glade_widget_copy_properties (gitem_new, gitem);

		if (submenu)
		{
			list.data = glade_widget_get_from_gobject (submenu);
			glade_command_cut (&list);
			glade_command_paste (&list, gitem_new, NULL);
		}
	}

	/* Delete old widget */
	list.data = gitem;
	glade_command_delete (&list);
	
	gtk_widget_show_all (GTK_WIDGET (item_new));
	
	gtk_tree_store_set (e->store, iter,
			    GLADEGTK_MENU_GWIDGET, gitem_new,
			    GLADEGTK_MENU_OBJECT, item_new,
			    GLADEGTK_MENU_TYPE_NAME, klass_name,
			    -1);

	glade_gtk_menu_editor_reorder_children (GTK_WIDGET (glade_widget_get_object (parent)),
						GTK_TREE_MODEL (e->store), iter);
	glade_gtk_menu_editor_select_child (e, gitem_new);

	g_free (name);
	g_free (label);
	g_free (tooltip);
	
	/* End of glade-command */
	glade_command_pop_group ();
	
	glade_gtk_menu_editor_block_callbacks (e, FALSE);
}

static void
glade_gtk_menu_editor_item_type_edited (GtkCellRendererText *cell,
					const gchar *path_string,
					const gchar *new_text,
					GladeGtkMenuEditor *e)
{
	GtkTreeModel *item_class;
	GtkTreePath *path;
	GtkTreeIter iter, combo_iter;
	GladeWidgetClass *klass;
	gchar *type_name;

	path = gtk_tree_path_new_from_string (path_string);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (e->store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (e->store), &iter,
			    GLADEGTK_MENU_TYPE_NAME, &type_name,
			    -1);
	if (strcmp (type_name, new_text) == 0)
	{
		g_free (type_name);
		return;
	}
	
	/* Lookup GladeWidgetClass */
	item_class = glade_gtk_menu_editor_get_item_model ();
	gtk_tree_model_get_iter_first (item_class, &combo_iter);
	do
	{
		gtk_tree_model_get (item_class, &combo_iter,
				    GLADEGTK_MENU_ITEM_CLASS, &klass,
				    GLADEGTK_MENU_ITEM_NAME, &type_name, -1);
		
		if (strcmp (type_name, new_text) == 0) break;
		
		g_free (type_name);
	} while (gtk_tree_model_iter_next (item_class, &combo_iter));
	
	glade_gtk_menu_editor_item_change_type (e, &iter, klass, new_text);
}

static gint
glade_gtk_menu_editor_popup_handler (GtkWidget *treeview,
				     GdkEventButton *event,
				     GladeGtkMenuEditor *e)
{
	GtkTreePath *path;

	if (event->button == 3)
	{
		if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview),
			(gint) event->x, (gint) event->y, &path, NULL, NULL, NULL))
		{
			gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, NULL, FALSE);
			gtk_tree_path_free (path);
		}
		
		gtk_menu_popup (GTK_MENU (e->popup), NULL, NULL, NULL, NULL, 
				event->button, event->time);
		return TRUE;
	}

	return FALSE;
}

static void
glade_gtk_menu_editor_reorder (GladeGtkMenuEditor *e, GtkTreeIter *iter)
{
	GladeWidget *gitem, *gparent;
	GtkTreeIter parent_iter;
	GObject *parent;
	GList list = {0, };
	gchar *desc;
	
	desc = g_strdup_printf (_("Reorder %s's children"),
				glade_widget_get_name (e->gmenubar));
	glade_command_push_group (desc);
	g_free (desc);
	
	gtk_tree_model_get (GTK_TREE_MODEL (e->store), iter, GLADEGTK_MENU_GWIDGET, &gitem, -1);
	
	if (gtk_tree_model_iter_parent (GTK_TREE_MODEL (e->store), &parent_iter, iter))
	{
		GtkWidget *submenu;
		gtk_tree_model_get (GTK_TREE_MODEL (e->store), &parent_iter,
				    GLADEGTK_MENU_OBJECT, &parent,
				    GLADEGTK_MENU_GWIDGET, &gparent, -1);

		if ((submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (parent))))
			gparent = glade_widget_get_from_gobject (submenu);
		else
			gparent = glade_command_create (glade_widget_class_get_by_type (GTK_TYPE_MENU),
							gparent, NULL, e->project);
	}
	else
		gparent = e->gmenubar;

	list.data = gitem;
	glade_command_cut (&list);
	glade_command_paste (&list, gparent, NULL);

	glade_gtk_menu_editor_reorder_children (GTK_WIDGET (glade_widget_get_object (gparent)),
						GTK_TREE_MODEL (e->store), iter);

	glade_command_pop_group ();
}

static gboolean 
glade_gtk_menu_editor_drag_and_drop_idle (gpointer data)
{
	GladeGtkMenuEditor *e = (GladeGtkMenuEditor *) data;
	
	glade_gtk_menu_editor_reorder (e, &e->iter);
	glade_gtk_menu_editor_clear (e);
	gtk_tree_view_expand_all (GTK_TREE_VIEW (e->treeview));
	glade_gtk_menu_editor_set_cursor (e, &e->iter);
	glade_gtk_menu_editor_block_callbacks (e, FALSE);
	
	return FALSE;
}

static void
glade_gtk_menu_editor_row_inserted (GtkTreeModel *model,
				    GtkTreePath *path,
				    GtkTreeIter *iter,
				    GladeGtkMenuEditor *e)
{
	e->iter = *iter;
	glade_gtk_menu_editor_block_callbacks (e, TRUE);
	g_idle_add (glade_gtk_menu_editor_drag_and_drop_idle, e);
}

static void
glade_gtk_menu_editor_add_item (GladeGtkMenuEditor *e,
				GType type,
				gboolean as_child)
{
	GtkTreeIter iter, new_iter;
	GladeWidget *gparent, *gitem_new;
	GValue val = {0, };
	const gchar *name;
	gchar *desc;

	glade_gtk_menu_editor_block_callbacks (e, TRUE);
	
	desc = g_strdup_printf (_("Create a %s item"), glade_gtk_menu_editor_type_name (type));
	glade_command_push_group (desc);
	g_free (desc);

	gparent = e->gmenubar;
	
	if (glade_gtk_menu_editor_get_item_selected (e, &iter))
	{
		GObject *parent;
		gtk_tree_model_get (GTK_TREE_MODEL (e->store), &iter,
				    GLADEGTK_MENU_OBJECT, &parent,
				    GLADEGTK_MENU_GWIDGET, &gparent, -1);
		if (as_child)
		{
			GtkWidget *submenu;
			if ((submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (parent))))
				gparent = glade_widget_get_from_gobject (submenu);
			else
				gparent = glade_command_create (glade_widget_class_get_by_type (GTK_TYPE_MENU),
								gparent, NULL, glade_widget_get_project (gparent));
		
			gtk_tree_store_append (GTK_TREE_STORE (e->store), &new_iter, &iter);
		}
		else
		{
			gtk_tree_store_insert_after (GTK_TREE_STORE (e->store), 
						     &new_iter, NULL, &iter);
			gparent = glade_widget_get_parent (gparent);
		}
		
	}
	else
		gtk_tree_store_append (GTK_TREE_STORE (e->store), &new_iter, NULL);
	
	if (GTK_IS_SEPARATOR_MENU_ITEM (glade_widget_get_object (gparent)))
	{
		glade_command_pop_group ();
		return;
	}
	
	gitem_new = glade_command_create (glade_widget_class_get_by_type (type),
					  gparent, NULL, e->project);
	
	if (type == GTK_TYPE_SEPARATOR_MENU_ITEM)
	{
		name = MENU_EDITOR_SEPARATOR_LABEL;
	}
	else
	{
		name = glade_widget_get_name (gitem_new);
		g_value_init (&val, G_TYPE_STRING);
		g_value_set_string (&val, name);
		glade_command_set_property (glade_widget_get_property (gitem_new, "label"), &val);
		
		g_value_unset(&val);
		
		g_value_init (&val, G_TYPE_BOOLEAN);
		g_value_set_boolean (&val, TRUE);
		glade_command_set_property (glade_widget_get_property (gitem_new, "use-underline"), &val);
	}		
	
	gtk_tree_store_set (GTK_TREE_STORE (e->store), &new_iter, 
			    GLADEGTK_MENU_GWIDGET, gitem_new,
			    GLADEGTK_MENU_OBJECT, glade_widget_get_object (gitem_new),
			    GLADEGTK_MENU_TYPE_NAME, glade_gtk_menu_editor_type_name (type),
			    GLADEGTK_MENU_LABEL, name,
			    GLADEGTK_MENU_TOOLTIP, NULL,
			    -1);
	
	glade_gtk_menu_editor_reorder_children (GTK_WIDGET (glade_widget_get_object (gparent)),
						GTK_TREE_MODEL (e->store),
						&new_iter);
	
	gtk_tree_view_expand_all (GTK_TREE_VIEW (e->treeview));
	glade_gtk_menu_editor_set_cursor (e, &new_iter);
	
	glade_command_pop_group ();
	
	glade_gtk_menu_editor_block_callbacks (e, FALSE);
}

static void
glade_gtk_menu_editor_add_item_activate (GtkMenuItem *menuitem,
					 GladeGtkMenuEditor *e)
{
	glade_gtk_menu_editor_add_item (e, GTK_TYPE_MENU_ITEM, FALSE);
}

static void
glade_gtk_menu_editor_add_child_item_activate (GtkMenuItem *menuitem,
					       GladeGtkMenuEditor *e)
{
	glade_gtk_menu_editor_add_item (e, GTK_TYPE_MENU_ITEM, TRUE);
}

static void
glade_gtk_menu_editor_add_separator_activate (GtkMenuItem *menuitem,
					      GladeGtkMenuEditor *e)
{
	glade_gtk_menu_editor_add_item (e, GTK_TYPE_SEPARATOR_MENU_ITEM, FALSE);
}

static void
glade_gtk_menu_editor_delete_item (GladeGtkMenuEditor *e)
{
	GtkTreeIter iter;
	GList list = {0, };

	if (glade_gtk_menu_editor_get_item_selected (e, &iter))
	{
		gtk_tree_model_get (GTK_TREE_MODEL (e->store), &iter,
				    GLADEGTK_MENU_GWIDGET, &list.data, -1);
			
		gtk_tree_store_remove (e->store, &iter);
		glade_command_delete (&list);
		
		glade_gtk_menu_editor_clear (e);
	}
}

#include <gdk/gdkkeysyms.h>
gboolean
glade_gtk_menu_editor_treeview_key_press_event (GtkWidget *widget,
						GdkEventKey *event,
						GladeGtkMenuEditor *e)
{
	if (event->keyval == GDK_Delete)
		glade_gtk_menu_editor_delete_item (e);
	
	return FALSE;
}

static void
glade_gtk_menu_editor_delete_item_activate (GtkMenuItem *menuitem,
					    GladeGtkMenuEditor *e)
{
	glade_gtk_menu_editor_delete_item (e);
}

static void
glade_gtk_menu_editor_help (GtkButton *button, GtkWidget *window)
{
	GtkWidget *dialog;
	
	dialog = gtk_message_dialog_new (GTK_WINDOW (window),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_INFO,
					 GTK_BUTTONS_CLOSE, " ");
	
	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), 
		_("<big><b>Tips:</b></big>\n"
		  "  * Right click over the treeview to add items.\n"
		  "  * Press Delete to remove the selected item.\n"
		  "  * Drag &amp; Drop to reorder.\n"
		  "  * Type column is editable."));
	
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static gboolean
glade_gtk_menu_editor_is_child (GladeGtkMenuEditor *e, GladeWidget *item)
{
	if (!GTK_IS_MENU_ITEM (glade_widget_get_object (item)))
		return FALSE;
	
	while ((item = glade_widget_get_parent (item)))
		if (item == e->gmenubar) return TRUE;

	return FALSE;
}

static gboolean
glade_gtk_menu_editor_update_treeview_idle (gpointer data)
{
	GladeGtkMenuEditor *e = (GladeGtkMenuEditor *) data;
	GList *selection = glade_project_selection_get (e->project);
	GladeWidget *widget;
	
	glade_gtk_menu_editor_block_callbacks (e, TRUE);
	
	gtk_tree_store_clear (e->store);
	glade_gtk_menu_editor_fill_store (GTK_WIDGET (glade_widget_get_object (e->gmenubar)),
					  e->store, NULL);
	glade_gtk_menu_editor_clear (e);

	gtk_tree_view_expand_all (GTK_TREE_VIEW (e->treeview));
	
	if (selection)
	{
		widget = glade_widget_get_from_gobject (G_OBJECT (selection->data)); 
		if (glade_gtk_menu_editor_is_child (e, widget))
			glade_gtk_menu_editor_select_child (e, widget);
	}
	
	glade_gtk_menu_editor_block_callbacks (e, FALSE);
	
	return FALSE;
}

static void
glade_gtk_menu_editor_project_remove_widget (GladeProject *project,
					     GladeWidget  *widget,
					     GladeGtkMenuEditor *e)
{
	if (widget == e->gmenubar)
	{
		gtk_widget_destroy (e->window);
		return;
	}
	
	if (glade_gtk_menu_editor_is_child (e, widget))
	{
		GtkTreeIter iter;
		if (glade_gtk_menu_editor_find_child (e, widget, &iter))
		{
			gtk_tree_store_remove (e->store, &iter);
			glade_gtk_menu_editor_clear (e);
		}
	}
}

static void
glade_gtk_menu_editor_project_add_widget (GladeProject *project,
					  GladeWidget  *widget,
					  GladeGtkMenuEditor *e)
{
	if (glade_gtk_menu_editor_is_child (e, widget))
		g_idle_add (glade_gtk_menu_editor_update_treeview_idle, e);
}

static void
glade_gtk_menu_editor_block_callbacks (GladeGtkMenuEditor *e, gboolean block)
{
	if (block)
	{
		g_signal_handlers_block_by_func (e->store, glade_gtk_menu_editor_row_inserted, e);
		g_signal_handlers_block_by_func (e->project, glade_gtk_menu_editor_project_remove_widget, e);
		g_signal_handlers_block_by_func (e->project, glade_gtk_menu_editor_project_add_widget, e);
	}
	else
	{
		g_signal_handlers_unblock_by_func (e->store, glade_gtk_menu_editor_row_inserted, e);
		g_signal_handlers_unblock_by_func (e->project, glade_gtk_menu_editor_project_remove_widget, e);
		g_signal_handlers_unblock_by_func (e->project, glade_gtk_menu_editor_project_add_widget, e);
	}
}

static void
glade_gtk_menu_editor_project_widget_name_changed (GladeProject *project,
						   GladeWidget  *widget,
						   GladeGtkMenuEditor *e)
{
	glade_gtk_menu_editor_select_child (e, widget);
}

static void
glade_gtk_menu_editor_project_closed (GladeProject *project,
				      GladeGtkMenuEditor *e)
{
	e->project = NULL;
	gtk_widget_destroy (e->window);
}

static gboolean
glade_gtk_menu_editor_destroyed (GtkWidget *window, GladeGtkMenuEditor *e)
{
	if (e->project)
	{
		g_signal_handlers_disconnect_by_func (e->project,
			glade_gtk_menu_editor_project_closed, e);
		
		g_signal_handlers_disconnect_by_func (e->project,
			glade_gtk_menu_editor_project_remove_widget, e);
		
		g_signal_handlers_disconnect_by_func (e->project,
			glade_gtk_menu_editor_project_add_widget, e);
		
		g_signal_handlers_disconnect_by_func (e->project,
			glade_gtk_menu_editor_project_widget_name_changed, e);
	}

	g_object_set_data (glade_widget_get_object (e->gmenubar),
			   "GladeGtkMenuEditor", NULL);
	
	g_free (e);
	
	return FALSE;
}

static
GladeGtkMenuEditor *
glade_gtk_menu_editor_new (GObject *menubar)
{
	GladeGtkMenuEditor *e;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkWidget *vbox, *signal_editor_w, *paned, *hbox, *prop_vbox, *tree_vbox;
	GtkWidget *label, *scroll, *item, *button, *buttonbox, *button_table;
	gchar *title;

	if (menubar == NULL) return NULL;
		
	if (g_object_get_data (menubar, "GladeGtkMenuEditor")) return NULL;
	
	/* Editor's struct */
	e = g_malloc0 (sizeof (GladeGtkMenuEditor));
	g_object_set_data (menubar, "GladeGtkMenuEditor", e);
	
	e->gmenubar = glade_widget_get_from_gobject (menubar);
	
	e->project = glade_widget_get_project (e->gmenubar);

	g_signal_connect (e->project, "close",
			  G_CALLBACK (glade_gtk_menu_editor_project_closed),
			  e);
	
	g_signal_connect (e->project, "remove-widget",
			  G_CALLBACK (glade_gtk_menu_editor_project_remove_widget),
			  e);
	
	g_signal_connect (e->project, "add-widget",
			  G_CALLBACK (glade_gtk_menu_editor_project_add_widget),
			  e);
	
	g_signal_connect (e->project, "widget-name-changed",
			  G_CALLBACK (glade_gtk_menu_editor_project_widget_name_changed),
			  e);

	/* Store */
	e->store = gtk_tree_store_new (GLADEGTK_MENU_N_COLUMNS,
				       G_TYPE_OBJECT,
				       G_TYPE_OBJECT,
				       G_TYPE_STRING,
				       G_TYPE_STRING,
				       G_TYPE_STRING,
				       G_TYPE_BOOLEAN);
    	glade_gtk_menu_editor_fill_store (GTK_WIDGET (menubar), e->store, NULL);
	g_signal_connect (e->store, "row-inserted", G_CALLBACK (glade_gtk_menu_editor_row_inserted), e);
		
	/* Window */
	e->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_modal (GTK_WINDOW (e->window), TRUE);
	gtk_window_set_type_hint (GTK_WINDOW (e->window), GDK_WINDOW_TYPE_HINT_DIALOG);
	g_signal_connect (e->window, "destroy", G_CALLBACK (glade_gtk_menu_editor_destroyed), e);

	title = g_strdup_printf ("%s - %s", _("Menu Bar Editor"), glade_widget_get_name (e->gmenubar));
	gtk_window_set_title (GTK_WINDOW (e->window), title);
	g_free (title);
	
	/* Vbox */
	vbox = gtk_vbox_new (FALSE, 8);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), GLADE_GENERIC_BORDER_WIDTH);
	gtk_container_add (GTK_CONTAINER (e->window), vbox);

	/* Paned */
	paned = gtk_vpaned_new ();
	gtk_box_pack_start (GTK_BOX (vbox), paned, TRUE, TRUE, 0);
	
	/* Hbox */
	hbox = gtk_hbox_new (FALSE, 8);
	gtk_paned_pack1 (GTK_PANED (paned), hbox, TRUE, FALSE);
	
	/* TreeView Vbox */
	tree_vbox = gtk_vbox_new (FALSE, 8);
	gtk_box_pack_start (GTK_BOX (hbox), tree_vbox, FALSE, TRUE, 0);
	
	/* ScrolledWindow */
	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_box_pack_start (GTK_BOX (tree_vbox), scroll, TRUE, TRUE, 0);

	/* TreeView */
	e->treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (e->store));
	gtk_tree_view_expand_all (GTK_TREE_VIEW (e->treeview));
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (e->treeview), TRUE);
	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (e->treeview), TRUE);

	gtk_widget_add_events (e->treeview, GDK_KEY_PRESS_MASK);
	g_signal_connect (e->treeview, "key-press-event",
			  G_CALLBACK (glade_gtk_menu_editor_treeview_key_press_event), e);

	g_signal_connect (e->treeview, "cursor_changed",
			  G_CALLBACK (glade_gtk_menu_editor_treeview_cursor_changed), e);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Label"), renderer,
						"text", GLADEGTK_MENU_LABEL, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (e->treeview), column);
	
	renderer = gtk_cell_renderer_combo_new ();
	g_object_set (renderer,
			"model", glade_gtk_menu_editor_get_item_model(),
			"text-column", GLADEGTK_MENU_ITEM_NAME,
			"has-entry", FALSE,
			"editable", TRUE,
			NULL);
	g_signal_connect (renderer, "edited", G_CALLBACK (glade_gtk_menu_editor_item_type_edited), e);
	column = gtk_tree_view_column_new_with_attributes (_("Type"), renderer,
						"text", GLADEGTK_MENU_TYPE_NAME, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (e->treeview), column);

	gtk_container_add (GTK_CONTAINER (scroll), e->treeview);
	
	/* Add/Remove buttons */
	button_table = gtk_table_new (1, 2, TRUE);
	gtk_table_set_col_spacings (GTK_TABLE (button_table), 8);
	gtk_box_pack_start (GTK_BOX (tree_vbox), button_table, FALSE, TRUE, 0);
	
	button = gtk_button_new_from_stock (GTK_STOCK_ADD);
	g_signal_connect (button, "clicked", G_CALLBACK (glade_gtk_menu_editor_add_item_activate), e);
	gtk_table_attach_defaults (GTK_TABLE (button_table), button, 0, 1, 0, 1);

	e->remove_button = button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
	g_signal_connect (button, "clicked", G_CALLBACK (glade_gtk_menu_editor_delete_item_activate), e);
	gtk_table_attach_defaults (GTK_TABLE (button_table), button, 1, 2, 0, 1);
	
	/* PopUp */
	e->popup = gtk_menu_new ();
	
	item = gtk_menu_item_new_with_label (_("Add Item"));
	g_signal_connect (item, "activate", G_CALLBACK (glade_gtk_menu_editor_add_item_activate), e);
	gtk_menu_shell_append (GTK_MENU_SHELL (e->popup), item);
	
	item = gtk_menu_item_new_with_label (_("Add Child Item"));
	g_signal_connect (item, "activate", G_CALLBACK (glade_gtk_menu_editor_add_child_item_activate), e);
	gtk_menu_shell_append (GTK_MENU_SHELL (e->popup), item);

	item = gtk_menu_item_new_with_label (_("Add Separator"));
	g_signal_connect (item, "activate", G_CALLBACK (glade_gtk_menu_editor_add_separator_activate), e);
	gtk_menu_shell_append (GTK_MENU_SHELL (e->popup), item);
	
	gtk_widget_show_all (e->popup);
	g_signal_connect (e->treeview, "button_press_event", G_CALLBACK (glade_gtk_menu_editor_popup_handler), e);
	
	/* Properties Vbox */
	prop_vbox = gtk_vbox_new (FALSE, 8);
	gtk_box_pack_start (GTK_BOX (hbox), prop_vbox, TRUE, TRUE, 0);
	
	/* Properties label */
	label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (label), _("<big><b>Properties</b></big>"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (prop_vbox), label, FALSE, TRUE, 0);

	/* Tables */
	e->table = gtk_table_new (1, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (e->table), 4);
	gtk_box_pack_start (GTK_BOX (prop_vbox), e->table, FALSE, TRUE, 0);

	e->child_table = gtk_table_new (1, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (e->child_table), 4);
	gtk_box_pack_start (GTK_BOX (prop_vbox), e->child_table, FALSE, TRUE, 0);

	/* Signal Editor */
	e->signal_editor = glade_signal_editor_new (NULL);
	signal_editor_w = glade_signal_editor_get_widget (e->signal_editor);
	gtk_widget_set_size_request (signal_editor_w, -1, 96);
	gtk_paned_pack2 (GTK_PANED (paned), signal_editor_w, FALSE, FALSE);
	
	/* Button Box */
	buttonbox = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX (buttonbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing (GTK_BOX (buttonbox), 8);
	gtk_box_pack_start (GTK_BOX (vbox), buttonbox, FALSE, TRUE, 0);

	button = glade_default_app_undo_button_new ();
	gtk_container_add (GTK_CONTAINER (buttonbox), button);

	button = glade_default_app_redo_button_new ();
	gtk_container_add (GTK_CONTAINER (buttonbox), button);

	button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_widget_destroy), e->window);
	gtk_container_add (GTK_CONTAINER (buttonbox), button);

	button = gtk_button_new_from_stock (GTK_STOCK_HELP);
	g_signal_connect (button, "clicked", G_CALLBACK (glade_gtk_menu_editor_help), e->window);
	gtk_container_add (GTK_CONTAINER (buttonbox), button);
	gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (buttonbox), button, TRUE);
	
	gtk_widget_show_all (vbox);
	
	return e;
}

void
glade_gtk_menu_bar_launch_editor (GObject *menubar)
{
	GladeGtkMenuEditor *editor;
	
	if ((editor = glade_gtk_menu_editor_new (menubar)))
	{
		gtk_window_set_default_size (GTK_WINDOW (editor->window), 600, 440);
		gtk_widget_show (editor->window);
		return;
	}
	else
	{
		GladeWidget *gmenubar = glade_widget_get_from_gobject (menubar);
		glade_util_ui_message (GTK_WIDGET (glade_default_app_get_transient_parent ()),
				       GLADE_UI_INFO,
				       _("A MenuBar editor is already runing for \"%s\"\n"
					 "Cannot launch more than one editor per menubar."),
					(gmenubar) ? glade_widget_get_name (gmenubar) : _("unknown"));
	}
}

/* GtkLabel */
void GLADEGTK_API
glade_gtk_label_set_label (GObject *object, GValue *value)
{
	GladeWidget *glabel;
	gboolean use_markup = FALSE, use_underline = FALSE;
	
	g_return_if_fail (GTK_IS_LABEL (object));
	glabel = glade_widget_get_from_gobject (object);
	g_return_if_fail (GLADE_IS_WIDGET (glabel));
	
	glade_widget_property_get (glabel, "use-markup", &use_markup);
	
	if (use_markup)
		gtk_label_set_markup (GTK_LABEL (object), g_value_get_string (value));
	else
		gtk_label_set_text (GTK_LABEL (object), g_value_get_string (value));
	
	glade_widget_property_get (glabel, "use-underline", &use_underline);
	if (use_underline)
		gtk_label_set_use_underline (GTK_LABEL (object), use_underline);
}
