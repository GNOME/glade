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
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-widget-adaptor.h"
#include "glade-popup.h"
#include "glade-placeholder.h"
#include "glade-clipboard.h"
#include "glade-command.h"
#include "glade-project.h"
#include "glade-app.h"

/********************************************************
                      WIDGET POPUP
 *******************************************************/
static void
glade_popup_select_cb (GtkMenuItem *item, GladeWidget *widget)
{
	glade_util_clear_selection ();
	glade_app_selection_set
		(glade_widget_get_object (widget), TRUE);
}

static GladePlaceholder *
find_placeholder (GObject *object)
{
	GtkContainer *container;
	GladePlaceholder *retval = NULL;
	GtkWidget *child;
	GList *c, *l;

	if (!GTK_IS_CONTAINER (object))
		return NULL;

	container = GTK_CONTAINER (object);
	
	for (c = l = glade_util_container_get_all_children (container);
	     l;
	     l = g_list_next (l))
	{
		child = l->data;
		
		if (GLADE_IS_PLACEHOLDER (child))
		{
			retval = GLADE_PLACEHOLDER (child);
			break;
		}
	}
	
	g_list_free (c);

	return retval;
}

static void
glade_popup_placeholder_add_cb (GtkMenuItem *item, GladePlaceholder *placeholder)
{
	GladeWidgetAdaptor *adaptor;
	GladeWidget        *parent;

	adaptor = glade_palette_get_current_item (glade_app_get_palette ());
	g_return_if_fail (adaptor != NULL);

	parent = glade_placeholder_get_parent (placeholder);

	if (!glade_util_check_and_warn_scrollable (parent, adaptor, glade_app_get_window()))
	{
		glade_command_create (adaptor, parent,
				      placeholder, glade_placeholder_get_project (placeholder));
		
		glade_palette_deselect_current_item (glade_app_get_palette(), TRUE);
	}
}

static void
glade_popup_action_add_cb (GtkMenuItem *item, GladeWidget *group)
{
	GladeWidgetAdaptor *adaptor;

	adaptor = glade_palette_get_current_item (glade_app_get_palette ());
	g_return_if_fail (adaptor != NULL);

	glade_command_create (adaptor, group,
			      NULL, glade_widget_get_project (group));
	
	glade_palette_deselect_current_item (glade_app_get_palette(), TRUE);
}

static void
glade_popup_root_add_cb (GtkMenuItem *item, gpointer *user_data)
{
	GladeWidgetAdaptor *adaptor = (GladeWidgetAdaptor *)user_data;
	GladePalette *palette = glade_app_get_palette ();
	
	if (!adaptor)
		adaptor = glade_palette_get_current_item (palette);
	g_return_if_fail (adaptor != NULL);
	
	glade_palette_create_root_widget (palette, adaptor);
}

static void
glade_popup_cut_cb (GtkMenuItem *item, GladeWidget *widget)
{
	GladeProject *project = glade_app_get_project ();

	glade_util_clear_selection ();
		
	/* Assign selection first */
	if (glade_project_is_selected
	    (project, glade_widget_get_object (widget)) == FALSE)
	{
		glade_app_selection_set
			(glade_widget_get_object (widget), FALSE);
	}
	glade_app_command_cut ();
}

static void
glade_popup_copy_cb (GtkMenuItem *item, GladeWidget *widget)
{
	GladeProject       *project = glade_app_get_project ();

	glade_util_clear_selection ();

	/* Assign selection first */
	if (glade_project_is_selected
	    (project, glade_widget_get_object (widget)) == FALSE)
		glade_app_selection_set
			(glade_widget_get_object (widget), FALSE);

	glade_app_command_copy ();
}

static void
glade_popup_paste_cb (GtkMenuItem *item, GladeWidget *widget)
{
	glade_util_clear_selection ();

	/* The selected widget is the paste destination */
	if (widget)
		glade_app_selection_set	(glade_widget_get_object (widget), FALSE);
	else
		glade_app_selection_clear (FALSE);

	glade_app_command_paste (NULL);
}

static void
glade_popup_delete_cb (GtkMenuItem *item, GladeWidget *widget)
{
	GladeProject       *project = glade_app_get_project ();

	/* Assign selection first */
	if (glade_project_is_selected
	    (project, glade_widget_get_object (widget)) == FALSE)
		glade_app_selection_set
			(glade_widget_get_object (widget), FALSE);

	glade_app_command_delete ();
}

/********************************************************
                  PLACEHOLDER POPUP
 *******************************************************/
static void
glade_popup_placeholder_paste_cb (GtkMenuItem *item,
				  GladePlaceholder *placeholder)
{
	glade_util_clear_selection ();
	glade_app_selection_clear (FALSE);
	
	glade_app_command_paste (placeholder);
}


/********************************************************
                    CLIPBOARD POPUP
 *******************************************************/
static void
glade_popup_clipboard_paste_cb (GtkMenuItem      *item,
				GladeWidget      *widget)
{
	glade_util_clear_selection ();
	glade_app_selection_clear (FALSE);

	glade_app_command_paste (NULL);
}

static void
glade_popup_clipboard_delete_cb (GtkMenuItem *item, GladeWidget *widget)
{
	glade_app_command_delete_clipboard ();
}

static void
glade_popup_docs_cb (GtkMenuItem *item,
		     GladeWidgetAdaptor *adaptor)
{
	gchar *book;

	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));

	g_object_get (adaptor, "book", &book, NULL);
	glade_editor_search_doc_search (glade_app_get_editor (), book, adaptor->name, NULL);
	g_free (book);
}


/********************************************************
                    POPUP BUILDING
 *******************************************************/
static GtkWidget *
glade_popup_append_item (GtkWidget *popup_menu,
			 const gchar *stock_id,
			 const gchar *label,
			 GtkWidget   *image,
			 gboolean sensitive,
			 gpointer callback,
			 gpointer data)
{
	GtkWidget *menu_item;

	if (stock_id && label)
	{
		menu_item = gtk_image_menu_item_new_with_mnemonic (label);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item),
			gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU));
	}
	else if (image && label)
	{
		menu_item = gtk_image_menu_item_new_with_mnemonic (label);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
	}
	else if (stock_id)
		menu_item = gtk_image_menu_item_new_from_stock (stock_id, NULL);
	else
		menu_item = gtk_menu_item_new_with_mnemonic (label);

	if (callback)
		g_signal_connect (G_OBJECT (menu_item), "activate",
				  G_CALLBACK (callback), data);

	gtk_widget_set_sensitive (menu_item, sensitive);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), menu_item);

	return menu_item;
}


static void
glade_popup_menuitem_activated (GtkMenuItem *item, const gchar *action_path)
{
	GladeWidget *widget;
	
	if ((widget = g_object_get_data (G_OBJECT (item), "gwa-data")))
		glade_widget_adaptor_action_activate (widget->adaptor,
						      widget->object,
						      action_path);
}

static void
glade_popup_menuitem_packing_activated (GtkMenuItem *item, const gchar *action_path)
{
	GladeWidget *widget;
	
	if ((widget = g_object_get_data (G_OBJECT (item), "gwa-data")))
		glade_widget_adaptor_child_action_activate (widget->parent->adaptor,
							    widget->parent->object,
							    widget->object,
							    action_path);
}

static void
glade_popup_menuitem_ph_packing_activated (GtkMenuItem *item, const gchar *action_path)
{
	GladePlaceholder *ph;
	GladeWidget *parent;
	
	if ((ph = g_object_get_data (G_OBJECT (item), "gwa-data")))
	{
		parent = glade_placeholder_get_parent (ph);
		glade_widget_adaptor_child_action_activate (parent->adaptor,
							    parent->object,
							    G_OBJECT (ph),
							    action_path);
	}
}

static gint
glade_popup_action_populate_menu_real (GtkWidget *menu,
				       GladeWidget *gwidget,
				       GList *actions,
				       GCallback callback,
				       gpointer data)
{
	GtkWidget *item;
	GList *list;
	gint n = 0;

	for (list = actions; list; list = g_list_next (list))
	{
		GladeWidgetAction *a = list->data;
		GtkWidget *submenu = NULL;

		if (a->actions)
		{
			submenu = gtk_menu_new ();
			n += glade_popup_action_populate_menu_real (submenu,
								    gwidget,
								    a->actions,
								    callback,
								    data);
		}
		else
			submenu = glade_widget_adaptor_action_submenu (gwidget->adaptor,
								       gwidget->object,
								       a->klass->path);
				
				
		item = glade_popup_append_item (menu, 
						a->klass->stock,
						a->klass->label, NULL, TRUE,
						(a->actions) ? NULL : callback,
						(a->actions) ? NULL : a->klass->path);
		
		g_object_set_data (G_OBJECT (item), "gwa-data", data);

		gtk_widget_set_sensitive (item, a->sensitive);
		
		if (submenu)
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
		
		n++;
	}

	return n;
}

/*
 * glade_popup_action_populate_menu:
 * @menu: a GtkMenu to put the actions menu items.
 * @widget: A #GladeWidget
 * @action: a @widget subaction or NULL to include all actions.
 * @packing: TRUE to include packing actions
 *
 * Populate a GtkMenu with widget's actions
 *
 * Returns the number of action appended to the menu.
 */
gint
glade_popup_action_populate_menu (GtkWidget *menu,
				  GladeWidget *widget,
				  GladeWidgetAction *action,
				  gboolean packing)
{
	gint n;

	g_return_val_if_fail (GTK_IS_MENU (menu), 0);
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), 0);
	
	if (action)
	{
		g_return_val_if_fail (GLADE_IS_WIDGET_ACTION (action), 0);
		if (glade_widget_get_action (widget, action->klass->path))
			return glade_popup_action_populate_menu_real (menu,
								      widget,
								      action->actions,
								      G_CALLBACK (glade_popup_menuitem_activated),
								      widget);
		
		if (glade_widget_get_pack_action (widget, action->klass->path))
			return glade_popup_action_populate_menu_real (menu,
								      glade_widget_get_parent (widget),
								      action->actions,
								      G_CALLBACK (glade_popup_menuitem_packing_activated),
								      widget);

		return 0;
	}
	
	n = glade_popup_action_populate_menu_real (menu,
						   widget,
						   widget->actions,
						   G_CALLBACK (glade_popup_menuitem_activated),
						   widget);
	
	if (packing && widget->packing_actions)
	{
		if (n)
		{
			GtkWidget *separator = gtk_menu_item_new ();
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), separator);
			gtk_widget_show (separator);
		}
		n += glade_popup_action_populate_menu_real (menu,
							    glade_widget_get_parent (widget),
							    widget->packing_actions,
							    G_CALLBACK (glade_popup_menuitem_packing_activated),
							    widget);
	}
	
	return n;
}

static GtkWidget *
glade_popup_create_menu (GladeWidget      *widget,
			 GladePlaceholder *placeholder,
			 gboolean          packing)
{
	GladeProjectFormat fmt;
	GladeWidgetAdaptor *current_item;
	GladeProject *project;
	GtkWidget *popup_menu;
	GtkWidget *separator;
	GList *list;
	gboolean   sensitive, non_window;
	GladePlaceholder *tmp_placeholder;
	gchar     *book;

	sensitive = (current_item = glade_palette_get_current_item (glade_app_get_palette ())) != NULL;

	/* Resolve project format first... */
	project = widget ? glade_widget_get_project (widget) :
		placeholder ? glade_placeholder_get_project (placeholder) : glade_app_get_project ();
	fmt = glade_project_get_format (project);


	popup_menu = gtk_menu_new ();
	
	if (current_item)
	{		
		
		/* Special case for GtkAction accelerators  */
		if (widget && GTK_IS_ACTION_GROUP (widget->object) &&
		    (current_item->type == GTK_TYPE_ACTION ||
		     g_type_is_a (current_item->type, GTK_TYPE_ACTION)))
		{
			glade_popup_append_item (popup_menu, NULL, _("_Add widget here"), NULL, TRUE,
						 glade_popup_action_add_cb, widget);
		}
		else
		{
			tmp_placeholder = placeholder;
			if (!tmp_placeholder && widget)
				tmp_placeholder = find_placeholder (glade_widget_get_object (widget));
			
			glade_popup_append_item (popup_menu, NULL, _("_Add widget here"), NULL, tmp_placeholder != NULL,
						 glade_popup_placeholder_add_cb, tmp_placeholder);
		}

		glade_popup_append_item (popup_menu, NULL, _("Add widget as _toplevel"), NULL, 
					 fmt != GLADE_PROJECT_FORMAT_LIBGLADE,
					 glade_popup_root_add_cb, NULL);

		separator = gtk_menu_item_new ();		
		gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), separator);
		gtk_widget_show (separator);
	}

	sensitive = (widget != NULL);

	glade_popup_append_item (popup_menu, NULL, _("_Select"), NULL, sensitive,
				 glade_popup_select_cb, widget);
	glade_popup_append_item (popup_menu, GTK_STOCK_CUT, NULL, NULL, sensitive,
				 glade_popup_cut_cb, widget);
	glade_popup_append_item (popup_menu, GTK_STOCK_COPY, NULL, NULL, sensitive,
				 glade_popup_copy_cb, widget);

	/* paste is placholder specific when the popup is on a placeholder */
	sensitive = glade_clipboard_get_has_selection (glade_app_get_clipboard ());
	non_window = FALSE;
	for (list = glade_app_get_clipboard ()->selection; list; list = list->next)
		if (!GTK_IS_WINDOW (GLADE_WIDGET (list->data)->object))
			non_window = TRUE;

	if (placeholder)
		glade_popup_append_item (popup_menu, GTK_STOCK_PASTE, NULL, NULL, sensitive,
					 glade_popup_placeholder_paste_cb, placeholder);
	else if (widget)
		glade_popup_append_item (popup_menu, GTK_STOCK_PASTE, NULL, NULL, sensitive,
					 glade_popup_paste_cb, widget);
	else
		/* No toplevel non-GtkWindow pastes in libglade */
		glade_popup_append_item (popup_menu, GTK_STOCK_PASTE, NULL, NULL, 
					 sensitive && !(non_window &&  (fmt == GLADE_PROJECT_FORMAT_LIBGLADE)),
					 glade_popup_paste_cb, NULL);



	glade_popup_append_item (popup_menu, GTK_STOCK_DELETE, NULL, NULL, (widget != NULL),
				 glade_popup_delete_cb, widget);


	/* packing actions are a little different on placholders */
	if (placeholder)
	{
		if (widget && widget->actions)
		{
			GtkWidget *separator = gtk_menu_item_new ();
			gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), separator);
			gtk_widget_show (separator);

			glade_popup_action_populate_menu_real
				(popup_menu,
				 widget,
				 widget->actions,
				 G_CALLBACK (glade_popup_menuitem_activated),
				 widget);
		}

		if (placeholder->packing_actions)
		{
			GtkWidget *separator = gtk_menu_item_new ();
			gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), separator);
			gtk_widget_show (separator);
			
			glade_popup_action_populate_menu_real
				(popup_menu,
				 widget,
				 placeholder->packing_actions,
				 G_CALLBACK (glade_popup_menuitem_ph_packing_activated),
				 placeholder);
		}
	}
	else if (widget && (widget->actions || (packing && widget->packing_actions)))
	{
		GtkWidget *separator = gtk_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), separator);
		gtk_widget_show (separator);

		glade_popup_action_populate_menu (popup_menu, widget, NULL, packing);
	}

	if (widget)
	{
		g_object_get (widget->adaptor, "book", &book, NULL);
		if (book)
		{
			GtkWidget *icon = glade_util_get_devhelp_icon (GTK_ICON_SIZE_MENU);
			GtkWidget *separator = gtk_menu_item_new ();
			gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), separator);
			gtk_widget_show (separator);
			
			glade_popup_append_item (popup_menu, NULL, _("Read _documentation"), icon, TRUE,
						 glade_popup_docs_cb, widget->adaptor);
		}
		g_free (book);
	}

	return popup_menu;
}

static GtkWidget *
glade_popup_create_clipboard_menu (GladeWidget *widget)
{
	GtkWidget *popup_menu;
	
	popup_menu = gtk_menu_new ();

	if (gtk_widget_is_toplevel (GTK_WIDGET (glade_widget_get_object (widget))))
	{
		glade_popup_append_item (popup_menu, GTK_STOCK_PASTE, 
					 NULL, NULL, TRUE,
					 glade_popup_clipboard_paste_cb, NULL);
	}

	glade_popup_append_item (popup_menu, GTK_STOCK_DELETE, NULL, NULL, TRUE,
				 glade_popup_clipboard_delete_cb, widget);

	return popup_menu;
}

void
glade_popup_widget_pop (GladeWidget *widget,
			GdkEventButton *event,
			gboolean packing)
{
	GtkWidget *popup_menu;
	gint button;
	gint event_time;

	g_return_if_fail (GLADE_IS_WIDGET (widget) || widget == NULL);

	popup_menu = glade_popup_create_menu (widget, NULL, packing);

	if (event)
	{
		button = event->button;
		event_time = event->time;
	}
	else
	{
		button = 0;
		event_time = gtk_get_current_event_time ();
	}
	gtk_menu_popup (GTK_MENU (popup_menu), NULL, NULL,
			NULL, NULL, button, event_time);
}

void
glade_popup_placeholder_pop (GladePlaceholder *placeholder,
			     GdkEventButton *event)
{
	GladeWidget *widget;
	GtkWidget *popup_menu;
	gint button;
	gint event_time;

	g_return_if_fail (GLADE_IS_PLACEHOLDER (placeholder));

	widget = glade_placeholder_get_parent (placeholder);
	
	popup_menu = glade_popup_create_menu (widget, placeholder, TRUE);

	if (event)
	{
		button = event->button;
		event_time = event->time;
	}
	else
	{
		button = 0;
		event_time = gtk_get_current_event_time ();
	}

	gtk_menu_popup (GTK_MENU (popup_menu), NULL, NULL,
			NULL, NULL, button, event_time);
}

void
glade_popup_clipboard_pop (GladeWidget    *widget,
			   GdkEventButton *event)
{
	GtkWidget *popup_menu;
	gint button;
	gint event_time;

	g_return_if_fail (GLADE_IS_WIDGET (widget));

	popup_menu = glade_popup_create_clipboard_menu (widget);

	if (event)
	{
		button = event->button;
		event_time = event->time;
	}
	else
	{
		button = 0;
		event_time = gtk_get_current_event_time ();
	}

	gtk_menu_popup (GTK_MENU (popup_menu), NULL, NULL,
			NULL, NULL, button, event_time);
}

void
glade_popup_palette_pop (GladeWidgetAdaptor *adaptor,
			 GdkEventButton     *event)
{
	GladeProjectFormat fmt;
	GladeProject *project;
	GtkWidget *popup_menu;
	gchar *book = NULL;
	gint button;
	gint event_time;

	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));

	popup_menu = gtk_menu_new ();
	
	project = glade_app_get_project ();
	fmt = glade_project_get_format (project);

	glade_popup_append_item (popup_menu, NULL, _("Add widget as _toplevel"), NULL, 
				 (fmt != GLADE_PROJECT_FORMAT_LIBGLADE),
				 glade_popup_root_add_cb, adaptor);

	g_object_get (adaptor, "book", &book, NULL);
	if (book && glade_util_have_devhelp ())
	{
		GtkWidget *icon = glade_util_get_devhelp_icon (GTK_ICON_SIZE_MENU);
		glade_popup_append_item (popup_menu, NULL, _("Read _documentation"), icon, TRUE,
					 glade_popup_docs_cb, adaptor);
	}
	g_free (book);


	if (event)
	{
		button = event->button;
		event_time = event->time;
	}
	else
	{
		button = 0;
		event_time = gtk_get_current_event_time ();
	}

	gtk_menu_popup (GTK_MENU (popup_menu), NULL, NULL,
			NULL, NULL, button, event_time);
}

static void
glade_popup_clear_property_cb (GtkMenuItem   *item,
			       GladeProperty *property)
{
	GValue value = { 0, };

	glade_property_get_default (property, &value);
	glade_command_set_property_value (property, &value);
	g_value_unset (&value);
}

static void
glade_popup_property_docs_cb (GtkMenuItem   *item,
			      GladeProperty *property)
{
	GladeWidgetAdaptor *adaptor, *prop_adaptor;
	gchar              *search, *book;

	prop_adaptor = glade_widget_adaptor_from_pclass (property->klass);
	adaptor = glade_widget_adaptor_from_pspec (prop_adaptor, property->klass->pspec);
	search  = g_strdup_printf ("The %s property", property->klass->id);

	g_object_get (adaptor, "book", &book, NULL);

	glade_editor_search_doc_search (glade_app_get_editor (),
					book, g_type_name (property->klass->pspec->owner_type), search);

	g_free (book);
	g_free (search);
}

void
glade_popup_property_pop (GladeProperty  *property,
			  GdkEventButton *event)
{

	GladeWidgetAdaptor *adaptor, *prop_adaptor;
	GtkWidget *popup_menu;
	gchar *book = NULL;
	gint button;
	gint event_time;

	prop_adaptor = glade_widget_adaptor_from_pclass (property->klass);
	adaptor = glade_widget_adaptor_from_pspec (prop_adaptor, property->klass->pspec);

	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));

	popup_menu = gtk_menu_new ();

	glade_popup_append_item (popup_menu, GTK_STOCK_CLEAR, _("Set default value"), NULL, 
				 TRUE, glade_popup_clear_property_cb, property);

	g_object_get (adaptor, "book", &book, NULL);
	if (book && glade_util_have_devhelp ())
	{
		GtkWidget *icon = glade_util_get_devhelp_icon (GTK_ICON_SIZE_MENU);
		glade_popup_append_item (popup_menu, NULL, _("Read _documentation"), icon, TRUE,
					 glade_popup_property_docs_cb, property);
	}
	g_free (book);


	if (event)
	{
		button = event->button;
		event_time = event->time;
	}
	else
	{
		button = 0;
		event_time = gtk_get_current_event_time ();
	}

	gtk_menu_popup (GTK_MENU (popup_menu), NULL, NULL,
			NULL, NULL, button, event_time);
}


void
glade_popup_simple_pop (GdkEventButton *event)
{
	GtkWidget *popup_menu;
	gint button;
	gint event_time;
	
	popup_menu = glade_popup_create_menu (NULL, NULL, FALSE);
	if (!popup_menu)
		return;

	if (event)
	{
		button = event->button;
		event_time = event->time;
	}
	else
	{
		button = 0;
		event_time = gtk_get_current_event_time ();
	}
	gtk_menu_popup (GTK_MENU (popup_menu), NULL, NULL,
		NULL, NULL, button, event_time);
}

gboolean
glade_popup_is_popup_event (GdkEventButton *event)
{
	g_return_val_if_fail (event, FALSE);

#ifdef MAC_INTEGRATION
	return (event->type == GDK_BUTTON_PRESS && event->button == 1 && ((event->state & GDK_MOD1_MASK) != 0));
#else
	return (event->type == GDK_BUTTON_PRESS && event->button == 3);
#endif
}

