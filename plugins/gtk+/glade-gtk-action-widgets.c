/*
 * glade-gtk-action-widgets.c
 *
 * Copyright (C) 2008 - 2010 Tristan Van Berkom
 *                      2011 Juan Pablo Ugarte
 *
 * Authors:
 *   Tristan Van Berkom <tvb@gnome.org>
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
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

#include "glade-gtk-action-widgets.h"

static GladeWidget *
glade_gtk_action_widgets_get_area (GladeWidget *widget, gchar *action_area)
{
  GladeWidgetAdaptor *adaptor = glade_widget_get_adaptor (widget);
  GObject *object;

  object = glade_widget_adaptor_get_internal_child (adaptor, 
                                                    glade_widget_get_object (widget),
                                                    action_area);
  
  return (object) ? glade_widget_get_from_gobject (object) : NULL;
}

static void
glade_gtk_action_widgets_read_responses (GladeWidget  *widget,
                                         GladeXmlNode *widgets_node,
                                         gchar        *action_container)
{
  GladeWidget *action_area;
  GladeXmlNode *node;

  if ((action_area = glade_gtk_action_widgets_get_area (widget, action_container)) == NULL)
    {
      g_warning ("%s: Could not find action widgets container [%s]", __func__, action_container);
      return;
    }

  for (node = glade_xml_node_get_children (widgets_node);
       node; node = glade_xml_node_next (node))
    {
      gchar *response;

      if (!glade_xml_node_verify (node, GLADE_TAG_ACTION_WIDGET))
        continue;

      response =
          glade_xml_get_property_string_required (node, GLADE_TAG_RESPONSE,
                                                  NULL);
      if (response)
        {
          gchar *widget_name = glade_xml_get_content (node);
          GladeWidget *action_widget = glade_widget_find_child (action_area, widget_name);

          if (action_widget)
            {
              gint response_id = (gint)g_ascii_strtoll (response, NULL, 10);
              if (response_id == 0) {
                GEnumClass *enum_class = g_type_class_ref (GTK_TYPE_RESPONSE_TYPE);
                GEnumValue *value = g_enum_get_value_by_name (enum_class, response);
                if (value)
                  {
                    response_id = value->value;
                  }
                else
                  {
                    value = g_enum_get_value_by_nick (enum_class, response);
                    if (value)
                      {
                        response_id = value->value;
                      }
                  }

                g_type_class_unref (enum_class);
              }

              glade_widget_property_set_enabled (action_widget, "response-id", TRUE);
              glade_widget_property_set (action_widget, "response-id", response_id);
            }

          g_free (widget_name);
        }

      g_free (response);
    }
}

void
glade_gtk_action_widgets_read_child (GladeWidget  *widget,
                                     GladeXmlNode *node,
                                     gchar        *action_container)
{
  GladeXmlNode *widgets_node;

  if ((widgets_node =
       glade_xml_search_child (node, GLADE_TAG_ACTION_WIDGETS)) != NULL)
    glade_gtk_action_widgets_read_responses (widget, widgets_node, action_container);
}

static void
glade_gtk_action_widgets_write_responses (GladeWidget     *widget,
                                          GladeXmlContext *context,
                                          GladeXmlNode    *node,
                                          gchar           *action_container)
{
  GladeXmlNode *widget_node;
  GList *l, *action_widgets;
  GladeWidget *action_area;

  if ((action_area = glade_gtk_action_widgets_get_area (widget, action_container)) == NULL)
   {
     g_warning ("%s: Could not find action widgets container [%s]", __func__, action_container);
     return;
   }
  
  action_widgets = glade_widget_get_children (action_area);
    
  for (l = action_widgets; l; l = l->next)
    {
      GladeWidget *action_widget;
      GladeProperty *property;
      gchar *str;

      if ((action_widget = glade_widget_get_from_gobject (l->data)) == NULL)
        continue;

      if ((property =
           glade_widget_get_property (action_widget, "response-id")) == NULL)
        continue;

      if (!glade_property_get_enabled (property))
        continue;

      widget_node = glade_xml_node_new (context, GLADE_TAG_ACTION_WIDGET);
      glade_xml_node_append_child (node, widget_node);

      str =
          glade_property_def_make_string_from_gvalue (glade_property_get_def (property),
                                                        glade_property_inline_value (property));

      glade_xml_node_set_property_string (widget_node, GLADE_TAG_RESPONSE, str);
      glade_xml_set_content (widget_node, glade_widget_get_name (action_widget));

      g_free (str);
    }

  g_list_free (action_widgets);
}

void
glade_gtk_action_widgets_ensure_names (GladeWidget *widget,
                                       gchar       *action_container)
{
  GList *l, *action_widgets;
  GladeWidget *action_area;

  if ((action_area = glade_gtk_action_widgets_get_area (widget, action_container)) == NULL)
   {
     g_warning ("%s: Could not find action widgets container [%s]", __func__, action_container);
     return;
   }

  action_widgets = glade_widget_get_children (action_area);

  for (l = action_widgets; l; l = l->next)
    {
      GladeWidget *action_widget;
      GladeProperty *property;

      if ((action_widget = glade_widget_get_from_gobject (l->data)) == NULL)
        continue;

      if ((property =
           glade_widget_get_property (action_widget, "response-id")) == NULL)
        continue;

      if (!glade_property_get_enabled (property))
        continue;

      glade_widget_ensure_name (action_widget, glade_widget_get_project (action_widget), FALSE);
    }

  g_list_free (action_widgets);
}

void
glade_gtk_action_widgets_write_child (GladeWidget     *widget,
                                      GladeXmlContext *context,
                                      GladeXmlNode    *node,
                                      gchar           *action_container)
{
  GladeXmlNode *widgets_node;

  widgets_node = glade_xml_node_new (context, GLADE_TAG_ACTION_WIDGETS);

  glade_gtk_action_widgets_write_responses (widget, context, widgets_node, action_container);

  if (!glade_xml_node_get_children (widgets_node))
    glade_xml_node_delete (widgets_node);
  else
    glade_xml_node_append_child (node, widgets_node);
}
