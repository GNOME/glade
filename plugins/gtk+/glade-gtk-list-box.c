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

static int
glade_gtk_listboxrow_sort (GtkListBoxRow *row1,
                           GtkListBoxRow *row2,
                           gpointer       user_data)
{
  gint pos1 = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row1), "position"));
  gint pos2 = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row2), "position"));

  return pos1 - pos2;
}

void
glade_gtk_listbox_post_create (GladeWidgetAdaptor *adaptor,
                               GObject            *container,
                               GladeCreateReason   reason)
{
  g_return_if_fail (GTK_IS_LIST_BOX (container));

  gtk_list_box_set_sort_func (GTK_LIST_BOX (container), (GtkListBoxSortFunc)glade_gtk_listboxrow_sort, NULL, NULL);
}

static void
sync_row_positions (GList *rows)
{
  GList *l;
  GList *changed_rows = NULL;
  int position;
  static gboolean recursion = FALSE;

  /* Avoid feedback loop */
  if (recursion)
    return;

  position = 0;
  for (l = rows; l; l = g_list_next (l))
    {
      gint old_position;

      old_position = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (l->data), "position"));
      if (position != old_position)
        {
          /* Update glade with the new value */
          recursion = TRUE;
          glade_widget_pack_property_set (glade_widget_get_from_gobject (l->data),
                                          "position", position);
          recursion = FALSE;

          /* Position has changed; need to resort */
          changed_rows = g_list_prepend (changed_rows, l->data);
        }

      position++;
    }

  /* Resort the changed rows */
  for (l = changed_rows; l; l = g_list_next (l))
    {
      gtk_list_box_row_changed (GTK_LIST_BOX_ROW (l->data));
    }
}

static void
glade_gtk_listbox_insert (GtkListBox    *listbox,
                          GtkListBoxRow *row,
                          gint           position)
{
  GList *children;

  children = gtk_container_get_children (GTK_CONTAINER (listbox));

  gtk_container_add (GTK_CONTAINER (listbox), GTK_WIDGET (row));

  /* Update the positions */
  children = g_list_insert (children, row, position);
  sync_row_positions (children);
  g_list_free (children);
}

static void
glade_gtk_listbox_reorder (GtkListBox    *listbox,
                           GtkListBoxRow *row,
                           gint           position)
{
  GList *children;

  children = gtk_container_get_children (GTK_CONTAINER (listbox));

  /* Update the positions */
  children = g_list_remove (children, row);
  children = g_list_insert (children, row, position);
  sync_row_positions (children);
  g_list_free (children);
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
      gint position = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (child), "position"));
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
      g_object_set_data (G_OBJECT (child), "position", GINT_TO_POINTER (position));

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
  g_return_if_fail (GTK_IS_LIST_BOX (container));

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
  GList *children;

  g_return_if_fail (GTK_IS_LIST_BOX (object));
  g_return_if_fail (GTK_IS_LIST_BOX_ROW (child));

  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));

  /* Update the positions */
  children = gtk_container_get_children (GTK_CONTAINER (object));
  sync_row_positions (children);
  g_list_free (children);
}

static void
glade_gtk_listbox_child_insert_action (GladeWidgetAdaptor *adaptor,
                                       GObject            *container,
                                       GObject            *object,
                                       const gchar        *group_format,
                                       gboolean            after)
{
  GladeWidget *parent;
  GladeWidget *gchild;
  gint position;

  parent = glade_widget_get_from_gobject (container);
  glade_command_push_group (group_format, glade_widget_get_name (parent));

  position = GPOINTER_TO_INT (g_object_get_data (object, "position"));
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
glade_gtk_listbox_child_action_activate (GladeWidgetAdaptor *adaptor,
                                         GObject            *container,
                                         GObject            *object,
                                         const gchar        *action_path)
{
  if (strcmp (action_path, "insert_after") == 0)
    {
      glade_gtk_listbox_child_insert_action (adaptor, container, object,
                                             _("Insert Row on %s"),
                                             TRUE);
    }
  else if (strcmp (action_path, "insert_before") == 0)
    {
      glade_gtk_listbox_child_insert_action (adaptor, container, object,
                                             _("Insert Row on %s"),
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
