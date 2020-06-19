/*
 * glade-gtk-flow-box.c - GladeWidgetAdaptor for GtkFlowBox widget
 *
 * Copyright (C) 2014 Red Hat, Inc.
 *
 * Authors:
 *      Matthias Clasen <mclasen@redhat.com>
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
sync_child_positions (GtkFlowBox *flowbox)
{
  GList *l, *children;
  int position;
  static gboolean recursion = FALSE;

  /* Avoid feedback loop */
  if (recursion)
    return;

  children = gtk_container_get_children (GTK_CONTAINER (flowbox));

  position = 0;
  for (l = children; l; l = g_list_next (l))
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

  g_list_free (children);
}

static void
glade_gtk_flowbox_insert (GtkFlowBox      *flowbox,
                          GtkFlowBoxChild *child,
                          gint             position)
{
  gtk_flow_box_insert (flowbox, GTK_WIDGET (child), position);
  sync_child_positions (flowbox);
}

static void
glade_gtk_flowbox_reorder (GtkFlowBox    *flowbox,
                           GtkFlowBoxChild *child,
                           gint           position)
{
  gtk_container_remove (GTK_CONTAINER (flowbox), GTK_WIDGET (child));
  gtk_flow_box_insert (flowbox, GTK_WIDGET (child), position);
  sync_child_positions (flowbox);
}

void
glade_gtk_flowbox_get_child_property (GladeWidgetAdaptor *adaptor,
                                      GObject            *container,
                                      GObject            *child,
                                      const gchar        *property_name,
                                      GValue             *value)
{
  g_return_if_fail (GTK_IS_FLOW_BOX (container));
  g_return_if_fail (GTK_IS_FLOW_BOX_CHILD (child));

  if (strcmp (property_name, "position") == 0)
    {
      gint position = gtk_flow_box_child_get_index (GTK_FLOW_BOX_CHILD (child));
      g_value_set_int (value, position);
    }
  else
    {
      /* Chain Up */
      GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->child_get_property (adaptor,
                                                              container,
                                                              child,
                                                              property_name,
                                                              value);
    }
}

void
glade_gtk_flowbox_set_child_property (GladeWidgetAdaptor *adaptor,
                                      GObject            *container,
                                      GObject            *child,
                                      const gchar        *property_name,
                                      GValue             *value)
{
  g_return_if_fail (GTK_IS_FLOW_BOX (container));
  g_return_if_fail (GTK_IS_FLOW_BOX_CHILD (child));

  g_return_if_fail (property_name != NULL || value != NULL);

  if (strcmp (property_name, "position") == 0)
    {
      gint position;

      position = g_value_get_int (value);
      glade_gtk_flowbox_reorder (GTK_FLOW_BOX (container),
                                 GTK_FLOW_BOX_CHILD (child),
                                 position);
    }
  else
    {
      /* Chain Up */
      GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->child_set_property (adaptor,
                                                              container,
                                                              child,
                                                              property_name,
                                                              value);
    }
}

gboolean
glade_gtk_flowbox_add_verify (GladeWidgetAdaptor *adaptor,
                              GtkWidget          *container,
                              GtkWidget          *child,
                              gboolean            user_feedback)
{
  if (!GTK_IS_FLOW_BOX_CHILD (child))
    {
      if (user_feedback)
        {
          GladeWidgetAdaptor *tool_item_adaptor =
            glade_widget_adaptor_get_by_type (GTK_TYPE_FLOW_BOX_CHILD);

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
glade_gtk_flowbox_add_child (GladeWidgetAdaptor *adaptor,
                             GObject            *object,
                             GObject            *child)
{
  g_return_if_fail (GTK_IS_FLOW_BOX (object));
  g_return_if_fail (GTK_IS_FLOW_BOX_CHILD (child));

  /* Insert to the end of the list */
  glade_gtk_flowbox_insert (GTK_FLOW_BOX (object),
                            GTK_FLOW_BOX_CHILD (child),
                            -1);
}

void
glade_gtk_flowbox_remove_child (GladeWidgetAdaptor *adaptor,
                                GObject            *object,
                                GObject            *child)
{
  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
  sync_child_positions (GTK_FLOW_BOX (object));
}

static void
glade_gtk_flowbox_child_insert_action (GladeWidgetAdaptor *adaptor,
                                       GObject            *container,
                                       GObject            *object,
                                       gboolean            after)
{
  GladeWidget *parent;
  GladeWidget *gchild;
  gint position;

  parent = glade_widget_get_from_gobject (container);
  glade_command_push_group (_("Insert Child on %s"), glade_widget_get_name (parent));

  position = gtk_flow_box_child_get_index (GTK_FLOW_BOX_CHILD (object));
  if (after)
    position++;

  gchild = glade_command_create (glade_widget_adaptor_get_by_type (GTK_TYPE_FLOW_BOX_CHILD),
                                 parent,
                                 NULL,
                                 glade_widget_get_project (parent));
  glade_widget_pack_property_set (gchild, "position", position);

  glade_command_pop_group ();
}

void
glade_gtk_flowbox_action_activate (GladeWidgetAdaptor *adaptor,
                                   GObject            *object,
                                   const gchar        *action_path)
{
  if (strcmp (action_path, "add_child") == 0)
    {
      GladeWidgetAdaptor *adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_FLOW_BOX_CHILD);
      GladeWidget *gparent = glade_widget_get_from_gobject (object);
      GladeProject *project = glade_widget_get_project (gparent);

      glade_command_create (adaptor, gparent, NULL, project);

      glade_project_selection_set (project, object, TRUE);
    }
  else
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->action_activate (adaptor,
                                                         object, action_path);
}

void
glade_gtk_flowbox_child_action_activate (GladeWidgetAdaptor *adaptor,
                                         GObject            *container,
                                         GObject            *object,
                                         const gchar        *action_path)
{
  if (strcmp (action_path, "insert_after") == 0)
    {
      glade_gtk_flowbox_child_insert_action (adaptor, container, object,
                                             TRUE);
    }
  else if (strcmp (action_path, "insert_before") == 0)
    {
      glade_gtk_flowbox_child_insert_action (adaptor, container, object,
                                             FALSE);
    }
  else
    {
      GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->child_action_activate (adaptor,
                                                                 container,
                                                                 object,
                                                                 action_path);
    }
}
