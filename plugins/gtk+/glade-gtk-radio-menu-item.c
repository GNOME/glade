/*
 * glade-gtk-radio-menu-item.c - GladeWidgetAdaptor for GtkRadioMenuItem
 *
 * Copyright (C) 2013 Tristan Van Berkom
 *
 * Authors:
 *      Tristan Van Berkom <tristan.van.berkom@gmail.com>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include <glib/gi18n-lib.h>
#include <gladeui/glade.h>

static void
glade_gtk_radio_menu_item_set_group (GObject * object, const GValue * value)
{
  GObject *val;

  g_return_if_fail (GTK_IS_RADIO_MENU_ITEM (object));

  if ((val = g_value_get_object (value)))
    {
      GSList *group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (val));

      if (!g_slist_find (group, GTK_RADIO_MENU_ITEM (object)))
        gtk_radio_menu_item_set_group (GTK_RADIO_MENU_ITEM (object), group);
    }
}

void
glade_gtk_radio_menu_item_set_property (GladeWidgetAdaptor * adaptor,
                                        GObject * object,
                                        const gchar * id, const GValue * value)
{

  if (!strcmp (id, "group"))
    glade_gtk_radio_menu_item_set_group (object, value);
  else
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_MENU_ITEM)->set_property (adaptor, object,
                                                      id, value);
}
