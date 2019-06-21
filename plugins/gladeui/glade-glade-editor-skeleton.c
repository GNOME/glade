/*
 * glade-glade-editor-skeleton.c
 *
 * Copyright (C) 2013 Tristan Van Berkom.
 *
 * Author:
 *   Tristan Van Berkom <tvb@gnome.org>
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
#include <gladeui/glade.h>

#define GLADE_TAG_SKELETON_EDITORS "child-editors"
#define GLADE_TAG_SKELETON_EDITOR  "editor"

void
glade_glade_editor_skeleton_read_widget (GladeWidgetAdaptor *adaptor,
					 GladeWidget        *widget,
					 GladeXmlNode       *node)
{
  GladeXmlNode *editors_node;
  GladeProperty *property;
  gchar *string = NULL;

  GWA_GET_CLASS (GTK_TYPE_BOX)->read_widget (adaptor, widget, node);

  if ((editors_node =
       glade_xml_search_child (node, GLADE_TAG_SKELETON_EDITORS)) != NULL)
    {
      GladeXmlNode *node;

      for (node = glade_xml_node_get_children (editors_node);
           node; node = glade_xml_node_next (node))
        {
          gchar *widget_name, *tmp;

          if (!glade_xml_node_verify (node, GLADE_TAG_SKELETON_EDITOR))
            continue;

          widget_name = glade_xml_get_property_string_required (node, GLADE_XML_TAG_ID, NULL);

          if (string == NULL)
            string = widget_name;
          else if (widget_name != NULL)
            {
              tmp =
                  g_strdup_printf ("%s%s%s", string, GLADE_PROPERTY_DEF_OBJECT_DELIMITER,
                                   widget_name);
              string = (g_free (string), tmp);
              g_free (widget_name);
            }
        }
    }

  if (string)
    {
      property = glade_widget_get_property (widget, "editors");
      g_assert (property);

      /* we must synchronize this directly after loading this project
       * (i.e. lookup the actual objects after they've been parsed and
       * are present).
       */
      g_object_set_data_full (G_OBJECT (property),
                              "glade-loaded-object", string, g_free);
    }
}

void
glade_glade_editor_skeleton_write_widget_after (GladeWidgetAdaptor *adaptor,
						GladeWidget        *widget,
						GladeXmlContext    *context,
						GladeXmlNode       *node)
{
  GladeXmlNode *widgets_node, *widget_node;
  GList *editors = NULL, *list;
  GladeWidget *awidget;

  GWA_GET_CLASS (GTK_TYPE_BOX)->write_widget_after (adaptor, widget, context, node);

  widgets_node = glade_xml_node_new (context, GLADE_TAG_SKELETON_EDITORS);

  if (glade_widget_property_get (widget, "editors", &editors))
    {
      for (list = editors; list; list = list->next)
        {
          awidget = glade_widget_get_from_gobject (list->data);
          widget_node = glade_xml_node_new (context, GLADE_TAG_SKELETON_EDITOR);
          glade_xml_node_append_child (widgets_node, widget_node);
          glade_xml_node_set_property_string (widget_node, GLADE_XML_TAG_ID,
                                              glade_widget_get_name (awidget));
        }
    }

  if (!glade_xml_node_get_children (widgets_node))
    glade_xml_node_delete (widgets_node);
  else
    glade_xml_node_append_child (node, widgets_node);

}
