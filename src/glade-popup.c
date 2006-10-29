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

/*
 * If stock_id != NULL, label is ignored
 */
static GtkWidget *
glade_popup_append_item (GtkWidget *popup_menu,
			 const gchar *stock_id,
			 const gchar *label,
			 gboolean sensitive,
			 gpointer callback,
			 gpointer data)
{
	GtkWidget *menu_item;

	if (stock_id != NULL)
		menu_item = gtk_image_menu_item_new_from_stock (stock_id, NULL);
	else
		menu_item = gtk_menu_item_new_with_mnemonic (label);

	g_signal_connect (G_OBJECT (menu_item), "activate",
			  G_CALLBACK (callback), data);

	gtk_widget_set_sensitive (menu_item, sensitive);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), menu_item);

	return menu_item;
}

static GtkWidget *
glade_popup_create_menu (GladeWidget *widget, gboolean add_childs);

static void
glade_popup_populate_childs (GtkWidget* popup_menu, GladeWidget* parent)
{
	while (parent) {
		GtkWidget *child;
		GtkWidget *child_menu;
		GtkWidget *separator;

		separator = gtk_menu_item_new ();
		gtk_widget_show (separator);
		gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu),
				       separator);
			
		child = gtk_menu_item_new_with_label (parent->name);
		child_menu = glade_popup_create_menu (parent, FALSE);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (child),
					   child_menu);
		gtk_widget_show (child);
		gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu),
				       child);

		parent = glade_widget_get_parent (parent);
	}
}


static GtkWidget *
glade_popup_create_menu (GladeWidget *widget, gboolean add_childs)
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

	sensitive = glade_clipboard_get_has_selection (glade_app_get_clipboard ());
	glade_popup_append_item (popup_menu, GTK_STOCK_PASTE, NULL, sensitive,
				 glade_popup_paste_cb, widget);

	glade_popup_append_item (popup_menu, GTK_STOCK_DELETE, NULL, TRUE,
				 glade_popup_delete_cb, widget);

	if (add_childs &&
	    !g_type_is_a (widget->adaptor->type, GTK_TYPE_WINDOW)) {
		GladeWidget *parent = glade_widget_get_parent (widget);
		g_return_val_if_fail (GLADE_IS_WIDGET (parent), popup_menu);
		glade_popup_populate_childs(popup_menu, parent);
	}

	return popup_menu;
}

static GtkWidget *
glade_popup_create_placeholder_menu (GladePlaceholder *placeholder)
{
	GtkWidget   *popup_menu;
	GladeWidget *parent;
	gboolean     sensitive;

	popup_menu = gtk_menu_new ();

	sensitive = glade_clipboard_get_has_selection (glade_app_get_clipboard ());
	glade_popup_append_item (popup_menu, GTK_STOCK_PASTE, NULL, sensitive,
				 glade_popup_placeholder_paste_cb, placeholder);

	if ((parent = glade_placeholder_get_parent (placeholder)))
		glade_popup_populate_childs(popup_menu, parent);

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
glade_popup_widget_pop (GladeWidget *widget, GdkEventButton *event, gboolean add_children)
{
	GtkWidget *popup_menu;
	gint button;
	gint event_time;

	g_return_if_fail (GLADE_IS_WIDGET (widget));

	popup_menu = glade_popup_create_menu (widget, add_children);

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
	GtkWidget *popup_menu;
	gint button;
	gint event_time;

	g_return_if_fail (GLADE_IS_PLACEHOLDER (placeholder));

	popup_menu = glade_popup_create_placeholder_menu (placeholder);

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
