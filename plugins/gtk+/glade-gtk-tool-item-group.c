/*
 * glade-gtk-tool-item-group.c - GladeWidgetAdaptor for GtkToolItemGroup
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

#include "glade-tool-item-group-editor.h"
#include "glade-gtk.h"

gboolean
glade_gtk_tool_item_group_add_verify (GladeWidgetAdaptor *adaptor,
                                      GtkWidget          *container,
                                      GtkWidget          *child,
                                      gboolean            user_feedback)
{
  if (!GTK_IS_TOOL_ITEM (child))
    {
      if (user_feedback)
        {
          GladeWidgetAdaptor *tool_item_adaptor = 
            glade_widget_adaptor_get_by_type (GTK_TYPE_TOOL_ITEM);

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
glade_gtk_tool_item_group_add_child (GladeWidgetAdaptor *adaptor,
                                     GObject            *object,
                                     GObject            *child)
{
  gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));
}

void
glade_gtk_tool_item_group_remove_child (GladeWidgetAdaptor *adaptor,
                                        GObject            *object,
                                        GObject            *child)
{
  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
}

static void
glade_gtk_tool_item_group_parse_finished (GladeProject *project,
                                          GladeWidget  *widget)
{
  GtkWidget *label_widget = NULL;

  glade_widget_property_get (widget, "label-widget", &label_widget);

  if (label_widget)
    glade_widget_property_set (widget, "custom-label", TRUE);
  else
    glade_widget_property_set (widget, "custom-label", FALSE);
}

void
glade_gtk_tool_item_group_read_widget (GladeWidgetAdaptor *adaptor,
                                       GladeWidget        *widget,
                                       GladeXmlNode       *node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (GTK_TYPE_TOOL_ITEM)->read_widget (adaptor, widget, node);

  /* Run this after the load so that icon-widget is resolved. */
  g_signal_connect (glade_widget_get_project (widget),
                    "parse-finished",
                    G_CALLBACK (glade_gtk_tool_item_group_parse_finished), widget);
}

static void
glade_gtk_tool_item_group_set_custom_label (GObject *object, const GValue *value)
{
  GladeWidget *gbutton;

  gbutton = glade_widget_get_from_gobject (object);

  glade_widget_property_set_sensitive (gbutton, "label", FALSE,
                                       NOT_SELECTED_MSG);
  glade_widget_property_set_sensitive (gbutton, "label-widget", FALSE,
                                       NOT_SELECTED_MSG);

  if (g_value_get_boolean (value))
    glade_widget_property_set_sensitive (gbutton, "label-widget", TRUE, NULL);
  else
    glade_widget_property_set_sensitive (gbutton, "label", TRUE, NULL);
}

void
glade_gtk_tool_item_group_set_property (GladeWidgetAdaptor *adaptor,
                                        GObject            *object,
                                        const gchar        *id,
                                        const GValue       *value)
{
  if (!strcmp (id, "custom-label"))
    glade_gtk_tool_item_group_set_custom_label (object, value);
  else if (!strcmp (id, "label"))
    {
      GladeWidget *widget = glade_widget_get_from_gobject (object);
      gboolean custom = FALSE;

      glade_widget_property_get (widget, "custom-label", &custom);
      if (!custom)
        gtk_tool_item_group_set_label (GTK_TOOL_ITEM_GROUP (object),
                                       g_value_get_string (value));
    } 
  else if (!strcmp (id, "label-widget")) 
    {
      GladeWidget *widget = glade_widget_get_from_gobject (object);
      GtkWidget *label = g_value_get_object (value);
      gboolean custom = FALSE;

      glade_widget_property_get (widget, "custom-label", &custom);
      if (custom || (glade_util_object_is_loading (object) && label != NULL))
        gtk_tool_item_group_set_label_widget (GTK_TOOL_ITEM_GROUP (object), label);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id, value);
}

GladeEditable *
glade_gtk_tool_item_group_create_editable (GladeWidgetAdaptor *adaptor,
                                           GladeEditorPageType type)
{
  GladeEditable *editable;

  /* Get base editable */
  editable =
      GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);

  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_tool_item_group_editor_new (adaptor, editable);

  return editable;
}
