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

#define GLADE_POPUP_WIDGET_POINTER "GladePopupWidgetPointer"

#include "glade.h"
#include "glade-widget.h"
#include "glade-popup.h"

static void
glade_popup_menu_detach (GtkWidget *attach_widget,
			 GtkMenu *menu)
{
	GladeWidget *widget;

	widget = glade_widget_get_from_gtk_widget (attach_widget);
	widget->popup_menu = NULL;
}

static void
glade_popup_item_activate_cb (GtkWidget *menu_item,
			      GladeWidgetFunction function)
{
	GladeWidget *widget;

	widget = g_object_get_data (G_OBJECT (menu_item), GLADE_POPUP_WIDGET_POINTER);

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GTK_IS_WIDGET (widget->widget));

	(*function) (widget);
}
			    
static void
glade_popup_append_item (GladeWidget *widget,
			 const gchar  *label,
			 GladeWidgetFunction function,
			 gboolean sensitive)
{
	GtkWidget *menu_item;

	menu_item = gtk_menu_item_new_with_label (label);

	g_object_set_data (G_OBJECT (menu_item), GLADE_POPUP_WIDGET_POINTER, widget);
	gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
			    GTK_SIGNAL_FUNC (glade_popup_item_activate_cb),
			    function);

	gtk_widget_set_sensitive (menu_item, sensitive);
	gtk_widget_show (menu_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (widget->popup_menu),
			       menu_item);
}

static void
glade_popup_append_items (GladeWidget *widget)
{
	glade_popup_append_item (widget, _("Delete"),
				 glade_widget_delete, TRUE);
}

void
glade_popup_pop (GladeWidget *widget, GdkEventButton *event)
{
	GtkWidget *popup_menu;

	if (widget->popup_menu)
		gtk_widget_destroy (widget->popup_menu);

	popup_menu = gtk_menu_new ();
	widget->popup_menu = popup_menu;

	gtk_menu_attach_to_widget (GTK_MENU (popup_menu),
				   GTK_WIDGET (widget->widget),
				   glade_popup_menu_detach);

	glade_popup_append_items (widget);

	gtk_menu_popup (GTK_MENU (popup_menu), NULL, NULL,
			NULL, NULL,
			event->button, event->time);

}
