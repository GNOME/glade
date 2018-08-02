/*
 * glade-gtk-list-box.c - GladeWidgetAdaptor for GtkListBox widget
 *
 * Copyright (C) 2013 Kalev Lember
 *
 * Authors:
 *      Kalev Lember <kalevlember@gmail.com>
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

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include <gladeui/glade.h>
#include "glade-gtk.h"

static void
sync_row_positions (GtkListBox *listbox)
{
  GList *l, *rows;
  int position;
  static gboolean recursion = FALSE;

  /* Avoid feedback loop */
  if (recursion)
    return;

  rows = gtk_container_get_children (GTK_CONTAINER (listbox));

  position = 0;
  for (l = rows; l; l = g_list_next (l))
    {
      gint old_position;

      glade_widget_pack_property_get (glade_widget_get_from_gobject (l->data),
                                      "position", &old_position);
      if (position != old_position)
        {
          /* Update glade with the new value */
          recursion = TRUE;
          glade_widget_pack_property_set (glade_widget_get_from_gobject (l->data),
                                          "position", position);
          recursion = FALSE;
        }

      position++;
    }

  g_list_free (rows);
}

static void
glade_gtk_listbox_insert (GtkListBox    *listbox,
                          GtkListBoxRow *row,
                          gint           position)
{
  gtk_list_box_insert (listbox, GTK_WIDGET (row), position);

  sync_row_positions (listbox);
}

static void
glade_gtk_listbox_reorder (GtkListBox    *listbox,
                           GtkListBoxRow *row,
                           gint           position)
{
  gtk_container_remove (GTK_CONTAINER (listbox), GTK_WIDGET (row));
  gtk_list_box_insert (listbox, GTK_WIDGET (row), position);

  sync_row_positions (listbox);
}

void
glade_gtk_listbox_get_child_property (GladeWidgetAdaptor *adaptor,
                                      GObject            *container,
                                      GObject            *child,
                                      const gchar        *property_name,
                                      GValue             *value)
{
  g_return_if_fail (GTK_IS_LIST_BOX (container));
  g_return_if_fail (GTK_IS_LIST_BOX_ROW (child));

  if (strcmp (property_name, "position") == 0)
    {
      gint position = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (child));
      g_value_set_int (value, position);
    }
  else
    {
      /* Chain Up */
      GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_get_property (adaptor,
                                                              container,
                                                              child,
                                                              property_name,
                                                              value);
    }
}

void
glade_gtk_listbox_set_child_property (GladeWidgetAdaptor *adaptor,
                                      GObject            *container,
                                      GObject            *child,
                                      const gchar        *property_name,
                                      GValue             *value)
{
  g_return_if_fail (GTK_IS_LIST_BOX (container));
  g_return_if_fail (GTK_IS_LIST_BOX_ROW (child));

  g_return_if_fail (property_name != NULL || value != NULL);

  if (strcmp (property_name, "position") == 0)
    {
      gint position;

      position = g_value_get_int (value);
      glade_gtk_listbox_reorder (GTK_LIST_BOX (container),
                                 GTK_LIST_BOX_ROW (child),
                                 position);
    }
  else
    {
      /* Chain Up */
      GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_set_property (adaptor,
                                                              container,
                                                              child,
                                                              property_name,
                                                              value);
    }
}

gboolean
glade_gtk_listbox_add_verify (GladeWidgetAdaptor *adaptor,
                              GtkWidget          *container,
                              GtkWidget          *child,
                              gboolean            user_feedback)
{
  if (!GTK_IS_LIST_BOX_ROW (child))
    {
      if (user_feedback)
        {
          GladeWidgetAdaptor *tool_item_adaptor =
            glade_widget_adaptor_get_by_type (GTK_TYPE_LIST_BOX_ROW);

          glade_util_ui_message (glade_app_get_window (),
                                 GLADE_UI_INFO, NULL,
                                 ONLY_THIS_GOES_IN_THAT_MSG,
                                 glade_widget_adaptor_get_title (tool_item_adaptor),
                                 glade_widget_adaptor_get_title (adaptor));
        }

      return FALSE;
    }

  return TRUE;
}

void
glade_gtk_listbox_add_child (GladeWidgetAdaptor *adaptor,
                             GObject            *object,
                             GObject            *child)
{
  g_return_if_fail (GTK_IS_LIST_BOX (object));
  g_return_if_fail (GTK_IS_LIST_BOX_ROW (child));

  /* Insert to the end of the list */
  glade_gtk_listbox_insert (GTK_LIST_BOX (object),
                            GTK_LIST_BOX_ROW (child),
                            -1);
}

void
glade_gtk_listbox_remove_child (GladeWidgetAdaptor *adaptor,
                                GObject            *object,
                                GObject            *child)
{
  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));

  sync_row_positions (GTK_LIST_BOX (object));
}

static void
glade_gtk_listbox_child_insert_action (GladeWidgetAdaptor *adaptor,
                                       GObject            *container,
                                       GObject            *object,
                                       gboolean            after)
{
  GladeWidget *parent;
  GladeWidget *gchild;
  gint position;

  parent = glade_widget_get_from_gobject (container);
  glade_command_push_group (_("Insert Row on %s"), glade_widget_get_name (parent));

  position = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (object));
  if (after)
    position++;

  gchild = glade_command_create (glade_widget_adaptor_get_by_type (GTK_TYPE_LIST_BOX_ROW),
                                 parent,
                                 NULL,
                                 glade_widget_get_project (parent));
  glade_widget_pack_property_set (gchild, "position", position);

  glade_command_pop_group ();
}

void
glade_gtk_listbox_action_activate (GladeWidgetAdaptor *adaptor,
                                   GObject            *object,
                                   const gchar        *action_path)
{
  if (strcmp (action_path, "add_row") == 0)
    {
      GladeWidgetAdaptor *adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_LIST_BOX_ROW);
      GladeWidget *gparent = glade_widget_get_from_gobject (object);
      GladeProject *project = glade_widget_get_project (gparent);

      glade_command_create (adaptor, gparent, NULL, project);

      glade_project_selection_set (project, object, TRUE);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->action_activate (adaptor,
                                                         object, action_path);
}

void
glade_gtk_listbox_child_action_activate (GladeWidgetAdaptor *adaptor,
                                         GObject            *container,
                                         GObject            *object,
                                         const gchar        *action_path)
{
  if (strcmp (action_path, "insert_after") == 0)
    {
      glade_gtk_listbox_child_insert_action (adaptor, container, object,
                                             TRUE);
    }
  else if (strcmp (action_path, "insert_before") == 0)
    {
      glade_gtk_listbox_child_insert_action (adaptor, container, object,
                                             FALSE);
    }
  else
    {
      GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_action_activate (adaptor,
                                                                 container,
                                                                 object,
                                                                 action_path);
    }
}
