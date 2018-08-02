/*
 * glade-gtk-recent-file-filter.c - GladeWidgetAdaptor for GtkRecentFilter and GtkFileFilter
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

#include "glade-string-list.h"

#define GLADE_TAG_PATTERNS     "patterns"
#define GLADE_TAG_PATTERN      "pattern"
#define GLADE_TAG_MIME_TYPES   "mime-types"
#define GLADE_TAG_MIME_TYPE    "mime-type"
#define GLADE_TAG_APPLICATIONS "applications"
#define GLADE_TAG_APPLICATION  "application"

typedef enum {
  FILTER_PATTERN,
  FILTER_MIME,
  FILTER_APPLICATION
} FilterType;

static void
glade_gtk_filter_read_strings (GladeWidget  *widget,
                               GladeXmlNode *node,
                               FilterType    type,
                               const gchar  *property_name)
{
  GladeXmlNode *items_node;
  GladeXmlNode *item_node;
  GList        *string_list = NULL;
  const gchar  *string_group_tag;
  const gchar  *string_tag;

  switch (type)
    {
    case FILTER_PATTERN:
      string_group_tag = GLADE_TAG_PATTERNS;
      string_tag       = GLADE_TAG_PATTERN;
      break;
    case FILTER_MIME:
      string_group_tag = GLADE_TAG_MIME_TYPES;
      string_tag       = GLADE_TAG_MIME_TYPE;
      break;
    case FILTER_APPLICATION:
      string_group_tag = GLADE_TAG_APPLICATIONS;
      string_tag       = GLADE_TAG_APPLICATION;
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  if ((items_node =
       glade_xml_search_child (node, string_group_tag)) != NULL)
    {

      for (item_node = glade_xml_node_get_children (items_node);
           item_node; item_node = glade_xml_node_next (item_node))
        {
          gchar *str;

          if (!glade_xml_node_verify (item_node, string_tag))
            continue;

          if ((str = glade_xml_get_content (item_node)) == NULL)
            continue;

          string_list = glade_string_list_append (string_list, str, NULL, NULL, FALSE, NULL);
          g_free (str);
        }

      glade_widget_property_set (widget, property_name, string_list);
      glade_string_list_free (string_list);
    }
}

static void
glade_gtk_filter_write_strings (GladeWidget     *widget,
                                GladeXmlContext *context,
                                GladeXmlNode    *node,
                                FilterType       type,
                                const gchar     *property_name)
{
  GladeXmlNode *item_node;
  GList        *string_list = NULL, *l;
  GladeString  *string;
  const gchar  *string_tag;

  switch (type)
    {
    case FILTER_PATTERN:     string_tag = GLADE_TAG_PATTERN;     break;
    case FILTER_MIME:        string_tag = GLADE_TAG_MIME_TYPE;   break;
    case FILTER_APPLICATION: string_tag = GLADE_TAG_APPLICATION; break;
    default:
      g_assert_not_reached ();
      break;
    }

  if (!glade_widget_property_get (widget, property_name, &string_list) || !string_list)
    return;

  for (l = string_list; l; l = l->next)
    {
      string = l->data;

      item_node = glade_xml_node_new (context, string_tag);
      glade_xml_node_append_child (node, item_node);

      glade_xml_set_content (item_node, string->string);
    }
}

GladeEditorProperty *
glade_gtk_recent_file_filter_create_eprop (GladeWidgetAdaptor *adaptor,
                                           GladePropertyClass *klass, 
                                           gboolean            use_command)
{
  GladeEditorProperty *eprop;
  GParamSpec          *pspec;

  pspec = glade_property_class_get_pspec (klass);

  if (pspec->value_type == GLADE_TYPE_STRING_LIST)
    {
      eprop = glade_eprop_string_list_new (klass, use_command, FALSE, FALSE);
    }
  else
    eprop = GWA_GET_CLASS
        (G_TYPE_OBJECT)->create_eprop (adaptor, klass, use_command);

  return eprop;
}

gchar *
glade_gtk_recent_file_filter_string_from_value (GladeWidgetAdaptor *adaptor,
                                                GladePropertyClass *klass,
                                                const GValue       *value)
{
  GParamSpec *pspec;

  pspec = glade_property_class_get_pspec (klass);

  if (pspec->value_type == GLADE_TYPE_STRING_LIST)
    {
      GList *list = g_value_get_boxed (value);

      return glade_string_list_to_string (list);
    }
  else
    return GWA_GET_CLASS
        (G_TYPE_OBJECT)->string_from_value (adaptor, klass, value);
}

void
glade_gtk_recent_filter_read_widget (GladeWidgetAdaptor *adaptor,
                                     GladeWidget        *widget, 
                                     GladeXmlNode       *node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->read_widget (adaptor, widget, node);

  glade_gtk_filter_read_strings (widget, node, FILTER_MIME, "glade-mime-types");
  glade_gtk_filter_read_strings (widget, node, FILTER_PATTERN, "glade-patterns");
  glade_gtk_filter_read_strings (widget, node, FILTER_APPLICATION, "glade-applications");
}

void
glade_gtk_recent_filter_write_widget (GladeWidgetAdaptor *adaptor,
                                      GladeWidget        *widget,
                                      GladeXmlContext    *context, 
                                      GladeXmlNode       *node)
{
  GladeXmlNode *strings_node;

  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);

  strings_node = glade_xml_node_new (context, GLADE_TAG_MIME_TYPES);
  glade_gtk_filter_write_strings (widget, context, strings_node, FILTER_MIME, "glade-mime-types");

  if (!glade_xml_node_get_children (strings_node))
    glade_xml_node_delete (strings_node);
  else
    glade_xml_node_append_child (node, strings_node);


  strings_node = glade_xml_node_new (context, GLADE_TAG_PATTERNS);
  glade_gtk_filter_write_strings (widget, context, strings_node, FILTER_PATTERN, "glade-patterns");

  if (!glade_xml_node_get_children (strings_node))
    glade_xml_node_delete (strings_node);
  else
    glade_xml_node_append_child (node, strings_node);

  strings_node = glade_xml_node_new (context, GLADE_TAG_APPLICATIONS);
  glade_gtk_filter_write_strings (widget, context, strings_node, 
                                  FILTER_APPLICATION, "glade-applications");

  if (!glade_xml_node_get_children (strings_node))
    glade_xml_node_delete (strings_node);
  else
    glade_xml_node_append_child (node, strings_node);
}

void
glade_gtk_file_filter_read_widget (GladeWidgetAdaptor *adaptor,
                                   GladeWidget        *widget, 
                                   GladeXmlNode       *node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->read_widget (adaptor, widget, node);

  glade_gtk_filter_read_strings (widget, node, FILTER_MIME, "glade-mime-types");
  glade_gtk_filter_read_strings (widget, node, FILTER_PATTERN, "glade-patterns");
}

void
glade_gtk_file_filter_write_widget (GladeWidgetAdaptor *adaptor,
                                    GladeWidget        *widget,
                                    GladeXmlContext    *context, 
                                    GladeXmlNode       *node)
{
  GladeXmlNode *strings_node;

  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);

  strings_node = glade_xml_node_new (context, GLADE_TAG_MIME_TYPES);
  glade_gtk_filter_write_strings (widget, context, strings_node, FILTER_MIME, "glade-mime-types");

  if (!glade_xml_node_get_children (strings_node))
    glade_xml_node_delete (strings_node);
  else
    glade_xml_node_append_child (node, strings_node);


  strings_node = glade_xml_node_new (context, GLADE_TAG_PATTERNS);
  glade_gtk_filter_write_strings (widget, context, strings_node, FILTER_PATTERN, "glade-patterns");

  if (!glade_xml_node_get_children (strings_node))
    glade_xml_node_delete (strings_node);
  else
    glade_xml_node_append_child (node, strings_node);
}
