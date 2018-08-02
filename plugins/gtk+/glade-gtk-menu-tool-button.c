/*
 * glade-gtk-menu-tool-button.c - GladeWidgetAdaptor for GtkMenuToolButton
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

#include "glade-gtk.h"

GList *
glade_gtk_menu_tool_button_get_children (GladeWidgetAdaptor *adaptor,
                                         GtkMenuToolButton  *button)
{
  GList *list = NULL;
  GtkWidget *menu = gtk_menu_tool_button_get_menu (button);

  list = glade_util_container_get_all_children (GTK_CONTAINER (button));

  /* Ensure that we only return one 'menu' */
  if (menu && g_list_find (list, menu) == NULL)
    list = g_list_append (list, menu);

  return list;
}

gboolean
glade_gtk_menu_tool_button_add_verify (GladeWidgetAdaptor *adaptor,
                                       GtkWidget          *container,
                                       GtkWidget          *child,
                                       gboolean            user_feedback)
{
  if (!GTK_IS_MENU (child))
    {
      if (user_feedback)
        {
          GladeWidgetAdaptor *menu_adaptor = 
            glade_widget_adaptor_get_by_type (GTK_TYPE_MENU);

          glade_util_ui_message (glade_app_get_window (),
                                 GLADE_UI_INFO, NULL,
                                 ONLY_THIS_GOES_IN_THAT_MSG,
                                 glade_widget_adaptor_get_title (menu_adaptor),
                                 glade_widget_adaptor_get_title (adaptor));
        }

      return FALSE;
    }

  return TRUE;
}

void
glade_gtk_menu_tool_button_add_child (GladeWidgetAdaptor *adaptor,
                                      GObject            *object,
                                      GObject            *child)
{
  if (GTK_IS_MENU (child))
    {
      g_object_set_data (child, "special-child-type", "menu");

      gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (object),
                                     GTK_WIDGET (child));
    }
}

void
glade_gtk_menu_tool_button_remove_child (GladeWidgetAdaptor *adaptor,
                                         GObject            *object,
                                         GObject            *child)
{
  if (GTK_IS_MENU (child))
    {
      gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (object), NULL);

      g_object_set_data (child, "special-child-type", NULL);
    }
}

void
glade_gtk_menu_tool_button_replace_child (GladeWidgetAdaptor *adaptor,
                                          GObject            *container,
                                          GObject            *current,
                                          GObject            *new_object)
{
  glade_gtk_menu_tool_button_remove_child (adaptor, container, current);
  glade_gtk_menu_tool_button_add_child (adaptor, container, new_object);
}
