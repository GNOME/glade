/*
 * glade-gtk-combo-box-text.c - GladeWidgetAdaptor for GtkComboBoxText
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
#include "glade-combo-box-text-editor.h"

#define GLADE_TAG_ITEMS  "items"
#define GLADE_TAG_ITEM   "item"

GladeEditable *
glade_gtk_combo_box_text_create_editable (GladeWidgetAdaptor *adaptor,
                                          GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    {
      return (GladeEditable *) glade_combo_box_text_editor_new ();
    }

  return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}

void
glade_gtk_combo_box_text_post_create (GladeWidgetAdaptor *adaptor,
                                      GObject            *object, 
                                      GladeCreateReason   reason)
{
  GladeWidget *gwidget;

  /* Chain Up */
  GWA_GET_CLASS (GTK_TYPE_COMBO_BOX)->post_create (adaptor, object, reason);

  /* No editor, no model, no cells on a GtkComboBoxText, just the items. */
  gwidget = glade_widget_get_from_gobject (object);
  glade_widget_set_action_visible (gwidget, "launch_editor", FALSE);
}

GladeEditorProperty *
glade_gtk_combo_box_text_create_eprop (GladeWidgetAdaptor *adaptor,
                                       GladePropertyDef   *def, 
                                       gboolean            use_command)
{
  GladeEditorProperty *eprop;
  GParamSpec          *pspec;

  pspec = glade_property_def_get_pspec (def);

  if (pspec->value_type == GLADE_TYPE_STRING_LIST)
    {
      eprop = glade_eprop_string_list_new (def, use_command, TRUE, TRUE);
    }
  else
    eprop = GWA_GET_CLASS
        (GTK_TYPE_WIDGET)->create_eprop (adaptor, def, use_command);

  return eprop;
}

gchar *
glade_gtk_combo_box_text_string_from_value (GladeWidgetAdaptor *adaptor,
                                            GladePropertyDef   *def,
                                            const GValue       *value)
{
  GParamSpec          *pspec;

  pspec = glade_property_def_get_pspec (def);

  if (pspec->value_type == GLADE_TYPE_STRING_LIST)
    {
      GList *list = g_value_get_boxed (value);

      return glade_string_list_to_string (list);
    }
  else
    return GWA_GET_CLASS
        (GTK_TYPE_COMBO_BOX)->string_from_value (adaptor, def, value);
}

void
glade_gtk_combo_box_text_set_property (GladeWidgetAdaptor *adaptor,
                                       GObject            *object,
                                       const gchar        *id,
                                       const GValue       *value)
{
  if (!strcmp (id, "glade-items"))
    {
      GList *string_list, *l;
      GladeString *string;
      gint active;

      string_list = g_value_get_boxed (value);

      active = gtk_combo_box_get_active (GTK_COMBO_BOX (object));

      /* Update comboboxtext items */
      gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (object));

      for (l = string_list; l; l = l->next)
        {
          string = l->data;

          gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (object), string->id, string->string);
        }

      gtk_combo_box_set_active (GTK_COMBO_BOX (object),
                                CLAMP (active, 0, g_list_length (string_list) - 1));
    }
  else
    GWA_GET_CLASS (GTK_TYPE_COMBO_BOX)->set_property (adaptor, object, id, value);
}

static void
glade_gtk_combo_box_text_read_items (GladeWidget *widget, GladeXmlNode *node)
{
  GladeXmlNode *items_node;
  GladeXmlNode *item_node;
  GList        *string_list = NULL;

  if ((items_node =
       glade_xml_search_child (node, GLADE_TAG_ITEMS)) != NULL)
    {

      for (item_node = glade_xml_node_get_children (items_node);
           item_node; item_node = glade_xml_node_next (item_node))
        {
          gchar *str, *comment, *context;
          gchar *id;
          gboolean translatable;

          if (!glade_xml_node_verify (item_node, GLADE_TAG_ITEM))
            continue;

          if ((str = glade_xml_get_content (item_node)) == NULL)
            continue;

          id           = glade_xml_get_property_string (item_node, GLADE_TAG_ID);
          context      = glade_xml_get_property_string (item_node, GLADE_TAG_CONTEXT);
          comment      = glade_xml_get_property_string (item_node, GLADE_TAG_COMMENT);
          translatable = glade_xml_get_property_boolean (item_node, GLADE_TAG_TRANSLATABLE, FALSE);

          string_list = 
            glade_string_list_append (string_list,
                                      str, comment, context, translatable, id);

          g_free (str);
          g_free (context);
          g_free (comment);
          g_free (id);
        }

      glade_widget_property_set (widget, "glade-items", string_list);
      glade_string_list_free (string_list);
    }
}

void
glade_gtk_combo_box_text_read_widget (GladeWidgetAdaptor *adaptor,
                                      GladeWidget        *widget,
                                      GladeXmlNode       *node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (GTK_TYPE_COMBO_BOX)->read_widget (adaptor, widget, node);

  glade_gtk_combo_box_text_read_items (widget, node);
}

static void
glade_gtk_combo_box_text_write_items (GladeWidget     *widget,
                                      GladeXmlContext *context,
                                      GladeXmlNode    *node)
{
  GladeXmlNode *item_node;
  GList *string_list = NULL, *l;
  GladeString *string;

  if (!glade_widget_property_get (widget, "glade-items", &string_list) || !string_list)
    return;

  for (l = string_list; l; l = l->next)
    {
      string = l->data;

      item_node = glade_xml_node_new (context, GLADE_TAG_ITEM);
      glade_xml_node_append_child (node, item_node);

      glade_xml_set_content (item_node, string->string);

      if (string->id)
        glade_xml_node_set_property_string (item_node,
                                            GLADE_TAG_ID,
                                            string->id);

      if (string->translatable)
        glade_xml_node_set_property_string (item_node,
                                            GLADE_TAG_TRANSLATABLE,
                                            GLADE_XML_TAG_I18N_TRUE);

      if (string->comment)
        glade_xml_node_set_property_string (item_node,
                                            GLADE_TAG_COMMENT,
                                            string->comment);

      if (string->context)
        glade_xml_node_set_property_string (item_node,
                                            GLADE_TAG_CONTEXT,
                                            string->context);
    }
}

void
glade_gtk_combo_box_text_write_widget (GladeWidgetAdaptor *adaptor,
                                       GladeWidget        *widget,
                                       GladeXmlContext    *context,
                                       GladeXmlNode       *node)
{
  GladeXmlNode *attrs_node;

  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (GTK_TYPE_COMBO_BOX)->write_widget (adaptor, widget, context, node);

  attrs_node = glade_xml_node_new (context, GLADE_TAG_ITEMS);

  glade_gtk_combo_box_text_write_items (widget, context, attrs_node);

  if (!glade_xml_node_get_children (attrs_node))
    glade_xml_node_delete (attrs_node);
  else
    glade_xml_node_append_child (node, attrs_node);

}
