/*
 * glade-gtk-size-group.c - GladeWidgetAdaptor for GtkSizeGroup
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

#define GLADE_TAG_SIZEGROUP_WIDGETS "widgets"
#define GLADE_TAG_SIZEGROUP_WIDGET  "widget"

static void
glade_gtk_size_group_read_widgets (GladeWidget *widget, GladeXmlNode *node)
{
  GladeXmlNode *widgets_node;
  GladeProperty *property;
  gchar *string = NULL;

  if ((widgets_node =
       glade_xml_search_child (node, GLADE_TAG_SIZEGROUP_WIDGETS)) != NULL)
    {
      GladeXmlNode *node;

      for (node = glade_xml_node_get_children (widgets_node);
           node; node = glade_xml_node_next (node))
        {
          gchar *widget_name, *tmp;

          if (!glade_xml_node_verify (node, GLADE_TAG_SIZEGROUP_WIDGET))
            continue;

          widget_name = glade_xml_get_property_string_required
              (node, GLADE_TAG_NAME, NULL);

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
      property = glade_widget_get_property (widget, "widgets");
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
glade_gtk_size_group_read_widget (GladeWidgetAdaptor *adaptor,
                                  GladeWidget        *widget,
                                  GladeXmlNode       *node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->read_widget (adaptor, widget, node);

  glade_gtk_size_group_read_widgets (widget, node);
}


static void
glade_gtk_size_group_write_widgets (GladeWidget     *widget,
                                    GladeXmlContext *context,
                                    GladeXmlNode    *node)
{
  GladeXmlNode *widgets_node, *widget_node;
  GList *widgets = NULL, *list;
  GladeWidget *awidget;

  widgets_node = glade_xml_node_new (context, GLADE_TAG_SIZEGROUP_WIDGETS);

  if (glade_widget_property_get (widget, "widgets", &widgets))
    {
      for (list = widgets; list; list = list->next)
        {
          awidget = glade_widget_get_from_gobject (list->data);
          widget_node =
              glade_xml_node_new (context, GLADE_TAG_SIZEGROUP_WIDGET);
          glade_xml_node_append_child (widgets_node, widget_node);
          glade_xml_node_set_property_string (widget_node, GLADE_TAG_NAME,
                                              glade_widget_get_name (awidget));
        }
    }

  if (!glade_xml_node_get_children (widgets_node))
    glade_xml_node_delete (widgets_node);
  else
    glade_xml_node_append_child (node, widgets_node);

}


void
glade_gtk_size_group_write_widget (GladeWidgetAdaptor *adaptor,
                                   GladeWidget        *widget,
                                   GladeXmlContext    *context,
                                   GladeXmlNode       *node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);

  glade_gtk_size_group_write_widgets (widget, context, node);
}


void
glade_gtk_size_group_set_property (GladeWidgetAdaptor *adaptor,
                                   GObject            *object,
                                   const gchar        *property_name,
                                   const GValue       *value)
{
  if (!strcmp (property_name, "widgets"))
    {
      GSList *sg_widgets, *slist;
      GList *widgets, *list;

      /* remove old widgets */
      if ((sg_widgets =
           gtk_size_group_get_widgets (GTK_SIZE_GROUP (object))) != NULL)
        {
          /* copy since we are modifying an internal list */
          sg_widgets = g_slist_copy (sg_widgets);
          for (slist = sg_widgets; slist; slist = slist->next)
            gtk_size_group_remove_widget (GTK_SIZE_GROUP (object),
                                          GTK_WIDGET (slist->data));
          g_slist_free (sg_widgets);
        }

      /* add new widgets */
      if ((widgets = g_value_get_boxed (value)) != NULL)
        {
          for (list = widgets; list; list = list->next)
            gtk_size_group_add_widget (GTK_SIZE_GROUP (object),
                                       GTK_WIDGET (list->data));
        }
    }
  else
    GWA_GET_CLASS (G_TYPE_OBJECT)->set_property (adaptor, object,
                                                 property_name, value);
}
