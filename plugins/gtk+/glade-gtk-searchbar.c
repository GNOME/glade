/*
 * glade-gtk-searchbar.c
 *
 * Copyright (C) 2014 Red Hat, Inc.
 *
 * Author:
 *   Matthias Clasen <mclasen@redhat.com>
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
 *
 */

#include <config.h>
#include <glib/gi18n-lib.h>
#include <gladeui/glade.h>
#include "glade-gtk.h"

void
glade_gtk_search_bar_post_create (GladeWidgetAdaptor *adaptor,
                              GObject *widget,
                              GladeCreateReason reason)
{
  if (reason == GLADE_CREATE_USER)
    {
      GtkWidget *child;
      child = glade_placeholder_new ();
      gtk_container_add (GTK_CONTAINER (widget), child);
      g_object_set_data (G_OBJECT (widget), "child", child);
    }

  gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (widget), TRUE);
  gtk_search_bar_set_show_close_button (GTK_SEARCH_BAR (widget), FALSE);
}

GList *
glade_gtk_search_bar_get_children (GladeWidgetAdaptor * adaptor,
                                   GtkSearchBar       * searchbar)
{
  GObject *current;

  current = g_object_get_data (G_OBJECT (searchbar), "child");

  return g_list_append (NULL, current);
}

gboolean
glade_gtk_search_bar_add_verify (GladeWidgetAdaptor *adaptor,
                              GtkWidget          *container,
                              GtkWidget          *child,
                              gboolean            user_feedback)
{
  GObject *current;
  current = g_object_get_data (G_OBJECT (container), "child");
  if (!GLADE_IS_PLACEHOLDER (current))
    {
      if (user_feedback)
        {
          glade_util_ui_message (glade_app_get_window (),
                                 GLADE_UI_INFO, NULL,
                                 _("Search bar is already full"));

        }

      return FALSE;
    }

  return TRUE; 
}

void
glade_gtk_search_bar_add_child (GladeWidgetAdaptor *adaptor,
                                GObject            *object,
                                GObject            *child)
{
  GObject *current;
  current = g_object_get_data (G_OBJECT (object), "child");
  gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (GTK_WIDGET (current))), GTK_WIDGET (current));
  gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));
  g_object_set_data (G_OBJECT (object), "child", child);
}

void
glade_gtk_search_bar_remove_child (GladeWidgetAdaptor *adaptor,
                                GObject            *object,
                                GObject            *child)
{
  GObject *current;
  GtkWidget *new_child;

  current = g_object_get_data (G_OBJECT (object), "child");
  if (current == child)
    {
      gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (GTK_WIDGET (child))), GTK_WIDGET (child));
      new_child = glade_placeholder_new ();
      gtk_container_add (GTK_CONTAINER (object), new_child);
      g_object_set_data (G_OBJECT (object), "child", new_child);
    }
}

void
glade_gtk_search_bar_replace_child (GladeWidgetAdaptor * adaptor,
                                   GtkWidget * container,
                                   GtkWidget * current, GtkWidget * new_widget)
{
  if (current == (GtkWidget *)g_object_get_data (G_OBJECT (container), "child"))
    {
      gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (GTK_WIDGET (current))), GTK_WIDGET (current));
      gtk_container_add (GTK_CONTAINER (container), new_widget);
      g_object_set_data (G_OBJECT (container), "child", new_widget);
    }
}
