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

#include "glade.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-popup.h"
#include "glade-placeholder.h"
#include "glade-clipboard.h"
#include "glade-command.h"
#include "glade-project.h"
#include "glade-project-window.h"

static void
glade_popup_select_cb (GtkMenuItem *item, GladeWidget *widget)
{
	if (widget)
		glade_project_selection_set (widget->project, widget->widget, TRUE);
}

static void
glade_popup_cut_cb (GtkMenuItem *item, GladeWidget *widget)
{
	glade_command_cut (widget);
}

static void
glade_popup_copy_cb (GtkMenuItem *item, GladeWidget *widget)
{
	GladeProjectWindow *gpw;

	gpw = glade_project_window_get ();
	glade_clipboard_add_copy (gpw->clipboard, widget);
}

static void
glade_popup_paste_cb (GtkMenuItem *item, GladeWidget *widget)
{
	glade_implement_me ();
	/*
	 * look in glade-placeholder.c (glade_placeholder_on_button_press_event
	 * for the "paste" operation code.
	 */
}

static void
glade_popup_delete_cb (GtkMenuItem *item, GladeWidget *widget)
{
	glade_command_delete (widget);
}

static void
glade_popup_placeholder_paste_cb (GtkMenuItem *item,
				  GladePlaceholder *placeholder)
{
	glade_command_paste (placeholder);
}

static void
glade_popup_menu_detach (GtkWidget *attach_widget, GtkMenu *menu)
{

}

/*
 * If stock_id != NULL, label is ignored
 */
static void
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
}

static GtkWidget *
glade_popup_create_menu (GladeWidget *widget, gboolean add_childs);

static void
glade_popup_populate_childs(GtkWidget* popup_menu, GladeWidget* parent)
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

	popup_menu = gtk_menu_new ();

	glade_popup_append_item (popup_menu, NULL, _("_Select"), TRUE,
				 glade_popup_select_cb, widget);
	glade_popup_append_item (popup_menu, GTK_STOCK_CUT, NULL, TRUE,
				 glade_popup_cut_cb, widget);
	glade_popup_append_item (popup_menu, GTK_STOCK_COPY, NULL, TRUE,
				 glade_popup_copy_cb, widget);
	glade_popup_append_item (popup_menu, GTK_STOCK_PASTE, NULL, TRUE,
				 glade_popup_paste_cb, widget);
	glade_popup_append_item (popup_menu, GTK_STOCK_DELETE, NULL, TRUE,
				 glade_popup_delete_cb, widget);

	if (add_childs && !GLADE_WIDGET_IS_TOPLEVEL (widget)) {
		GladeWidget *parent = glade_widget_get_parent (widget);
		g_return_val_if_fail (GLADE_IS_WIDGET (parent), popup_menu);
		glade_popup_populate_childs(popup_menu, parent);
	}

	return popup_menu;
}

static GtkWidget *
glade_popup_create_placeholder_menu (GladePlaceholder *placeholder)
{
	GtkWidget *popup_menu;
	GladeWidget *parent;
	
	popup_menu = gtk_menu_new ();

	glade_popup_append_item (popup_menu, GTK_STOCK_PASTE, NULL, TRUE,
				 glade_popup_placeholder_paste_cb, placeholder);

	if ((parent = glade_placeholder_get_parent(placeholder)) != NULL)
		glade_popup_populate_childs(popup_menu, parent);

	return popup_menu;
}

void
glade_popup_pop (GladeWidget *widget, GdkEventButton *event)
{
	GtkWidget *popup_menu;

	popup_menu = glade_popup_create_menu (widget, TRUE);

	gtk_menu_attach_to_widget (GTK_MENU (popup_menu),
				   GTK_WIDGET (widget->widget),
				   glade_popup_menu_detach);

	gtk_menu_popup (GTK_MENU (popup_menu), NULL, NULL,
			NULL, NULL,
			event->button, event->time);

}

void
glade_popup_placeholder_pop (GladePlaceholder *placeholder,
			     GdkEventButton *event)
{
	GtkWidget *popup_menu;

	popup_menu = glade_popup_create_placeholder_menu (placeholder);

	gtk_menu_attach_to_widget (GTK_MENU (popup_menu),
				   GTK_WIDGET (placeholder),
				   glade_popup_menu_detach);

	gtk_menu_popup (GTK_MENU (popup_menu), NULL, NULL,
			NULL, NULL,
			event->button, event->time);
}

