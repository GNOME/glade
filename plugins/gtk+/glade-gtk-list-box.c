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
  g_return_if_fail (GTK_IS_WIDGET (child));

  if (strcmp (property_name, "position") == 0)
    {
      gint position = 0;

      if (GTK_IS_LIST_BOX_ROW (child)) {
        position = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (child));
      }

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
  g_return_if_fail (GTK_IS_WIDGET (child));

  g_return_if_fail (property_name != NULL || value != NULL);

  if (strcmp (property_name, "position") == 0)
    {
      gint position = g_value_get_int (value);

      if (GTK_IS_LIST_BOX_ROW (child)) {
        glade_gtk_listbox_reorder (GTK_LIST_BOX (container),
                                   GTK_LIST_BOX_ROW (child),
                                   position);
      }
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

static void
glade_listbox_search_placeholder_forall (GtkWidget *widget,
                                         gpointer data)
{
  GtkWidget **placeholder = (GtkWidget **)data;
  /* A simple child should be a GtkListBoxRow, otherwise it's a placeholder */
  if (!GTK_IS_LIST_BOX_ROW (widget) && GTK_IS_WIDGET (widget)) {
    *placeholder = GTK_WIDGET (widget);
  }
}

static GtkWidget*
glade_listbox_get_placeholder (GtkListBox *list_box) {
  GtkWidget *placeholder = NULL;

  gtk_container_forall (GTK_CONTAINER (list_box), glade_listbox_search_placeholder_forall, &placeholder);

  return placeholder;
}

static void
glade_gtk_listbox_parse_finished (GladeProject *project, GladeWidget *gbox)
{
  GObject *box = glade_widget_get_object (gbox);
  glade_widget_property_set (gbox, "use-placeholder", glade_listbox_get_placeholder (GTK_LIST_BOX (box)) != NULL);
}

void
glade_gtk_listbox_post_create (GladeWidgetAdaptor *adaptor,
                                GObject            *container,
                                GladeCreateReason   reason)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (container);
  GladeProject *project = glade_widget_get_project (gwidget);

  if (reason == GLADE_CREATE_LOAD)
    {
      g_signal_connect_object (project, "parse-finished",
                               G_CALLBACK (glade_gtk_listbox_parse_finished),
                               gwidget, 0);
    }
}

void
glade_gtk_listbox_get_property (GladeWidgetAdaptor *adaptor,
                                GObject            *object,
                                const gchar        *id,
                                GValue             *value)
{
  if (!strcmp (id, "use-placeholder"))
    {
      g_value_set_boolean (value, glade_listbox_get_placeholder (GTK_LIST_BOX (object)) != NULL);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->get_property (adaptor, object, id,
                                                      value);
}

void
glade_gtk_listbox_set_property (GladeWidgetAdaptor *adaptor,
                                GObject            *object,
                                const gchar        *id,
                                const GValue       *value)
{
  if (!strcmp (id, "use-placeholder"))
    {
      GtkWidget *child;

      if (g_value_get_boolean (value))
        {
          child = glade_listbox_get_placeholder (GTK_LIST_BOX (object));
          if (!child)
            child = glade_placeholder_new ();
          g_object_set_data (G_OBJECT (child), "special-child-type", "placeholder");
        }
      else
        {
          child = glade_listbox_get_placeholder (GTK_LIST_BOX (object));
          if (child)
            {
              GladeProject *project = glade_widget_get_project (glade_widget_get_from_gobject (object));
              /* Assign selection first */
              if (glade_project_is_selected
                  (project, child) == FALSE)
                glade_project_selection_set (project, child, FALSE);

              glade_project_command_delete (project);
              glade_project_selection_set (project, object, TRUE);
            }
          child = NULL;
        }
      gtk_list_box_set_placeholder (GTK_LIST_BOX (object), child);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id,
                                                      value);
}

void
glade_gtk_listbox_add_child (GladeWidgetAdaptor *adaptor,
                             GObject            *object,
                             GObject            *child)
{
  gchar *special_child_type;

  g_return_if_fail (GTK_IS_LIST_BOX (object));
  g_return_if_fail (GTK_IS_WIDGET (child));

  special_child_type = g_object_get_data (child, "special-child-type");
  if (!g_strcmp0 (special_child_type, "placeholder"))
    {
      gtk_list_box_set_placeholder (GTK_LIST_BOX (object), GTK_WIDGET (child));
       return;
    }

  g_return_if_fail (GTK_IS_LIST_BOX_ROW (child));

  /* Insert to the end of the list */
  glade_gtk_listbox_insert (GTK_LIST_BOX (object),
                            GTK_LIST_BOX_ROW (child),
                            -1);
}

void
glade_gtk_listbox_replace_child (GladeWidgetAdaptor *adaptor,
                                 GObject            *container,
                                 GObject            *current,
                                 GObject            *new_widget)
{
  gchar *special_child_type =
    g_object_get_data (G_OBJECT (current), "special-child-type");

  if (!g_strcmp0 (special_child_type, "placeholder"))
    {
      g_object_set_data (G_OBJECT (new_widget), "special-child-type", "placeholder");
      gtk_list_box_set_placeholder (GTK_LIST_BOX (container), GTK_WIDGET (new_widget));
      return;
    }

  GWA_GET_CLASS (GTK_TYPE_CONTAINER)->replace_child (adaptor,
                                                     container,
                                                     current, new_widget);
}

void
glade_gtk_listbox_remove_child (GladeWidgetAdaptor *adaptor,
                                GObject            *object,
                                GObject            *child)
{
  gchar *special_child_type;

  g_return_if_fail (GTK_IS_LIST_BOX (object));
  g_return_if_fail (GTK_IS_WIDGET (child));

  special_child_type = g_object_get_data (child, "special-child-type");
  if (!g_strcmp0 (special_child_type, "placeholder"))
    {
      GtkWidget *w;

      w = glade_placeholder_new ();
      g_object_set_data (G_OBJECT (w), "special-child-type", "placeholder");
      gtk_list_box_set_placeholder (GTK_LIST_BOX (object), w);
      return;
    }

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
  gint position = 0;

  parent = glade_widget_get_from_gobject (container);
  glade_command_push_group (_("Insert Row on %s"), glade_widget_get_name (parent));

  /* We can right click on the placeholder too */
  if (GTK_IS_LIST_BOX_ROW (object)) {
    position = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (object));
    if (after)
      position++;
  } else {
    if (after)
      position = -1;
  }

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
