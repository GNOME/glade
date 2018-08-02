/*
 * glade-gtk-action-group.c - GladeWidgetAdaptor for GtkActionGroup
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
#include "glade-accels.h"

gboolean
glade_gtk_action_group_add_verify (GladeWidgetAdaptor *adaptor,
                                   GtkWidget          *container,
                                   GtkWidget          *child,
                                   gboolean            user_feedback)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (!GTK_IS_ACTION (child))
G_GNUC_END_IGNORE_DEPRECATIONS
    {
      if (user_feedback)
        {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
          GladeWidgetAdaptor *action_adaptor = 
            glade_widget_adaptor_get_by_type (GTK_TYPE_ACTION);
G_GNUC_END_IGNORE_DEPRECATIONS
          glade_util_ui_message (glade_app_get_window (),
                                 GLADE_UI_INFO, NULL,
                                 ONLY_THIS_GOES_IN_THAT_MSG,
                                 glade_widget_adaptor_get_title (action_adaptor),
                                 glade_widget_adaptor_get_title (adaptor));
        }

      return FALSE;
    }

  return TRUE;
}

void
glade_gtk_action_group_add_child (GladeWidgetAdaptor *adaptor,
                                  GObject            *container,
                                  GObject            *child)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    if (GTK_IS_ACTION (child))
G_GNUC_END_IGNORE_DEPRECATIONS
    {
      /* Dont really add/remove actions (because name conflicts inside groups)
       */
      GladeWidget *ggroup = glade_widget_get_from_gobject (container);
      GladeWidget *gaction = glade_widget_get_from_gobject (child);
      GList *actions = g_object_get_data (G_OBJECT (ggroup), "glade-actions");

      actions = g_list_copy (actions);
      actions = g_list_append (actions, child);

      g_object_set_data_full (G_OBJECT (ggroup), "glade-actions", actions,
                              (GDestroyNotify) g_list_free);

      glade_widget_property_set_sensitive (gaction, "accelerator", TRUE, NULL);
      glade_widget_set_action_sensitive (gaction, "launch_editor", TRUE);
    }
}

void
glade_gtk_action_group_remove_child (GladeWidgetAdaptor *adaptor,
                                     GObject            *container,
                                     GObject            *child)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (GTK_IS_ACTION (child))
G_GNUC_END_IGNORE_DEPRECATIONS
    {
      /* Dont really add/remove actions (because name conflicts inside groups)
       */
      GladeWidget *ggroup = glade_widget_get_from_gobject (container);
      GladeWidget *gaction = glade_widget_get_from_gobject (child);
      GList *actions = g_object_get_data (G_OBJECT (ggroup), "glade-actions");

      actions = g_list_copy (actions);
      actions = g_list_remove (actions, child);

      g_object_set_data_full (G_OBJECT (ggroup), "glade-actions", actions,
                              (GDestroyNotify) g_list_free);
      
      glade_widget_property_set_sensitive (gaction, "accelerator", FALSE, 
                                           ACTION_ACCEL_INSENSITIVE_MSG);
      glade_widget_set_action_sensitive (gaction, "launch_editor", FALSE);
    }
}

void
glade_gtk_action_group_replace_child (GladeWidgetAdaptor *adaptor,
                                      GObject            *container,
                                      GObject            *current,
                                      GObject            *new_action)
{
  glade_gtk_action_group_remove_child (adaptor, container, current);
  glade_gtk_action_group_add_child (adaptor, container, new_action);
}

GList *
glade_gtk_action_group_get_children (GladeWidgetAdaptor *adaptor,
                                     GObject            *container)
{
  GladeWidget *ggroup = glade_widget_get_from_gobject (container);
  GList *actions = g_object_get_data (G_OBJECT (ggroup), "glade-actions");

  return g_list_copy (actions);
}


void
glade_gtk_action_group_read_child (GladeWidgetAdaptor *adaptor,
                                   GladeWidget        *widget,
                                   GladeXmlNode       *node)
{
  GladeXmlNode *widget_node;
  GladeWidget *child_widget;

  if (!glade_xml_node_verify (node, GLADE_XML_TAG_CHILD))
    return;

  if ((widget_node =
       glade_xml_search_child (node, GLADE_XML_TAG_WIDGET)) != NULL)
    {
      if ((child_widget = glade_widget_read (glade_widget_get_project (widget),
                                             widget, widget_node,
                                             NULL)) != NULL)
        {
          glade_widget_add_child (widget, child_widget, FALSE);

          /* Read in accelerator */
          glade_gtk_read_accels (child_widget, node, FALSE);
        }
    }
}


void
glade_gtk_action_group_write_child (GladeWidgetAdaptor *adaptor,
                                    GladeWidget        *widget,
                                    GladeXmlContext    *context,
                                    GladeXmlNode       *node)
{
  GladeXmlNode *child_node;

  child_node = glade_xml_node_new (context, GLADE_XML_TAG_CHILD);
  glade_xml_node_append_child (node, child_node);

  /* Write out the widget */
  glade_widget_write (widget, context, child_node);

  /* Write accelerator here  */
  glade_gtk_write_accels (widget, context, child_node, FALSE);
}

static void
glade_gtk_action_child_selected (GladeBaseEditor *editor,
                                 GladeWidget     *gchild,
                                 gpointer         data)
{
  glade_base_editor_add_label (editor, _("Action"));
        
  glade_base_editor_add_default_properties (editor, gchild);
        
  glade_base_editor_add_label (editor, _("Properties"));
  glade_base_editor_add_editable (editor, gchild, GLADE_PAGE_GENERAL);
}

static gboolean
glade_gtk_action_move_child (GladeBaseEditor *editor,
                             GladeWidget     *gparent,
                             GladeWidget     *gchild,
                             gpointer         data)
{        
  return FALSE;
}

static void
glade_gtk_action_launch_editor (GObject  *action)
{
  GladeWidget        *widget  = glade_widget_get_from_gobject (action);
  GladeBaseEditor    *editor;
  GtkWidget          *window;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GType type_action = GTK_TYPE_ACTION;
  GType type_toggle_action = GTK_TYPE_TOGGLE_ACTION;
  GType type_radio_action  = GTK_TYPE_RADIO_ACTION;
  GType type_recent_action = GTK_TYPE_RECENT_ACTION;
G_GNUC_END_IGNORE_DEPRECATIONS

  /* Make sure we get the group here */
  widget = glade_widget_get_toplevel (widget);

  /* Editor */
  editor = glade_base_editor_new (glade_widget_get_object (widget), NULL,
                                  _("Action"), type_action,
                                  _("Toggle"), type_toggle_action,
                                  _("Radio"),  type_radio_action,
                                  _("Recent"), type_recent_action,
                                  NULL);

  g_signal_connect (editor, "child-selected", G_CALLBACK (glade_gtk_action_child_selected), NULL);
  g_signal_connect (editor, "move-child", G_CALLBACK (glade_gtk_action_move_child), NULL);

  gtk_widget_show (GTK_WIDGET (editor));

  window = glade_base_editor_pack_new_window (editor, _("Action Group Editor"), NULL);
  gtk_widget_show (window);
}

void
glade_gtk_action_action_activate (GladeWidgetAdaptor *adaptor,
                                  GObject *object,
                                  const gchar *action_path)
{
  if (strcmp (action_path, "launch_editor") == 0)
    {
      glade_gtk_action_launch_editor (object);
    }
}
