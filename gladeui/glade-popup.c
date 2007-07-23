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
	glade_app_selection_set	(glade_widget_get_object (widget), FALSE);

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

/********************************************************
                    POPUP BUILDING
 *******************************************************/
static GtkWidget *
glade_popup_append_item (GtkWidget *popup_menu,
			 const gchar *stock_id,
			 const gchar *label,
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
								    a->actions,
								    callback,
								    data);
		}
				
		item = glade_popup_append_item (menu, 
						a->klass->stock,
						a->klass->label, TRUE,
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
								      action->actions,
								      G_CALLBACK (glade_popup_menuitem_activated),
								      widget);
		
		if (glade_widget_get_pack_action (widget, action->klass->path))
			return glade_popup_action_populate_menu_real (menu,
								      action->actions,
								      G_CALLBACK (glade_popup_menuitem_packing_activated),
								      widget);

		return 0;
	}
	
	n = glade_popup_action_populate_menu_real (menu,
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
	GtkWidget *popup_menu;
	gboolean   sensitive;

	popup_menu = gtk_menu_new ();

	glade_popup_append_item (popup_menu, NULL, _("_Select"), TRUE,
				 glade_popup_select_cb, widget);
	glade_popup_append_item (popup_menu, GTK_STOCK_CUT, NULL, TRUE,
				 glade_popup_cut_cb, widget);
	glade_popup_append_item (popup_menu, GTK_STOCK_COPY, NULL, TRUE,
				 glade_popup_copy_cb, widget);

	/* paste is placholder specific when the popup is on a placeholder */
	sensitive = glade_clipboard_get_has_selection (glade_app_get_clipboard ());
	if (placeholder)
		glade_popup_append_item (popup_menu, GTK_STOCK_PASTE, NULL, sensitive,
					 glade_popup_placeholder_paste_cb, placeholder);
	else
		glade_popup_append_item (popup_menu, GTK_STOCK_PASTE, NULL, sensitive,
					 glade_popup_paste_cb, widget);

	glade_popup_append_item (popup_menu, GTK_STOCK_DELETE, NULL, TRUE,
				 glade_popup_delete_cb, widget);


	/* packing actions are a little different on placholders */
	if (placeholder && placeholder->packing_actions)
	{
		GtkWidget *separator = gtk_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), separator);
		gtk_widget_show (separator);

		glade_popup_action_populate_menu_real (popup_menu,
						       placeholder->packing_actions,
						       G_CALLBACK (glade_popup_menuitem_ph_packing_activated),
						       placeholder);
	}
	else if (widget->actions || (packing && widget->packing_actions))
	{
		GtkWidget *separator = gtk_menu_item_new ();
		gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), separator);
		gtk_widget_show (separator);

		glade_popup_action_populate_menu (popup_menu, widget, NULL, packing);
	}

	return popup_menu;
}

static GtkWidget *
glade_popup_create_clipboard_menu (GladeWidget *widget)
{
	GtkWidget *popup_menu;
	
	popup_menu = gtk_menu_new ();

	if (GTK_WIDGET_TOPLEVEL (glade_widget_get_object (widget)))
	{
		glade_popup_append_item (popup_menu, GTK_STOCK_PASTE, 
					 NULL, TRUE,
					 glade_popup_clipboard_paste_cb, NULL);
	}

	glade_popup_append_item (popup_menu, GTK_STOCK_DELETE, NULL, TRUE,
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

	g_return_if_fail (GLADE_IS_WIDGET (widget));

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
