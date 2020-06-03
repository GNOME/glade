/*
 * glade-gtk-tool-button.c - GladeWidgetAdaptor for GtkToolButton
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
#include "glade-tool-button-editor.h"

G_MODULE_EXPORT GladeEditable *
glade_gtk_tool_button_create_editable (GladeWidgetAdaptor *adaptor,
                                       GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_tool_button_editor_new ();
  else
    return GWA_GET_CLASS (GTK_TYPE_TOOL_ITEM)->create_editable (adaptor, type);
}

static void
glade_gtk_tool_button_set_image_mode (GObject *object, const GValue *value)
{
  GladeWidget *gbutton;

  g_return_if_fail (GTK_IS_TOOL_BUTTON (object));
  gbutton = glade_widget_get_from_gobject (object);

  glade_widget_property_set_sensitive (gbutton, "stock-id", FALSE, NOT_SELECTED_MSG);
  glade_widget_property_set_sensitive (gbutton, "icon-name", FALSE, NOT_SELECTED_MSG);
  glade_widget_property_set_sensitive (gbutton, "icon-widget", FALSE, NOT_SELECTED_MSG);

  switch (g_value_get_int (value))
    {
      case GLADE_TB_MODE_STOCK:
        glade_widget_property_set_sensitive (gbutton, "stock-id", TRUE, NULL);
        break;
      case GLADE_TB_MODE_ICON:
        glade_widget_property_set_sensitive (gbutton, "icon-name", TRUE, NULL);
        break;
      case GLADE_TB_MODE_CUSTOM:
        glade_widget_property_set_sensitive (gbutton, "icon-widget", TRUE, NULL);
        break;
      default:
        break;
    }
}

static void
glade_gtk_tool_button_set_custom_label (GObject *object, const GValue *value)
{
  GladeWidget *gbutton;

  g_return_if_fail (GTK_IS_TOOL_BUTTON (object));
  gbutton = glade_widget_get_from_gobject (object);

  glade_widget_property_set_sensitive (gbutton, "label", FALSE, NOT_SELECTED_MSG);
  glade_widget_property_set_sensitive (gbutton, "label-widget", FALSE, NOT_SELECTED_MSG);
  glade_widget_property_set_sensitive (gbutton, "use-underline", FALSE,
                                       _("This property only applies when configuring the label with text"));

  if (g_value_get_boolean (value))
    glade_widget_property_set_sensitive (gbutton, "label-widget", TRUE, NULL);
  else
    {
      glade_widget_property_set_sensitive (gbutton, "label", TRUE, NULL);
      glade_widget_property_set_sensitive (gbutton, "use-underline", TRUE, NULL);
    }
}

static void
glade_gtk_tool_button_set_label (GObject *object, const GValue *value)
{
  const gchar *label;

  g_return_if_fail (GTK_IS_TOOL_BUTTON (object));

  label = g_value_get_string (value);

  if (label && strlen (label) == 0)
    label = NULL;

  gtk_tool_button_set_label (GTK_TOOL_BUTTON (object), label);
}

static void
glade_gtk_tool_button_set_stock_id (GObject *object, const GValue *value)
{
  const gchar *stock_id;

  g_return_if_fail (GTK_IS_TOOL_BUTTON (object));

  stock_id = g_value_get_string (value);

  if (stock_id && strlen (stock_id) == 0)
    stock_id = NULL;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON (object), stock_id);
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
glade_gtk_tool_button_set_icon_name (GObject *object, const GValue *value)
{
  const gchar *name;

  g_return_if_fail (GTK_IS_TOOL_BUTTON (object));

  name = g_value_get_string (value);

  if (name && strlen (name) == 0)
    name = NULL;

  gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (object), name);
}

G_MODULE_EXPORT void
glade_gtk_tool_button_set_property (GladeWidgetAdaptor *adaptor,
                                    GObject            *object,
                                    const gchar        *id,
                                    const GValue       *value)
{
  if (!strcmp (id, "image-mode"))
    glade_gtk_tool_button_set_image_mode (object, value);
  else if (!strcmp (id, "icon-name"))
    glade_gtk_tool_button_set_icon_name (object, value);
  else if (!strcmp (id, "stock-id"))
    glade_gtk_tool_button_set_stock_id (object, value);
  else if (!strcmp (id, "label"))
    glade_gtk_tool_button_set_label (object, value);
  else if (!strcmp (id, "custom-label"))
    glade_gtk_tool_button_set_custom_label (object, value);
  else
    GWA_GET_CLASS (GTK_TYPE_TOOL_ITEM)->set_property (adaptor,
                                                      object, id, value);
}

static void
glade_gtk_tool_button_parse_finished (GladeProject *project,
                                      GladeWidget  *widget)
{
  gchar *stock_str = NULL, *icon_name = NULL;
  gint stock_id = 0;
  GtkWidget *label_widget = NULL, *image_widget = NULL;

  glade_widget_property_get (widget, "stock-id", &stock_str);
  glade_widget_property_get (widget, "icon-name", &icon_name);
  glade_widget_property_get (widget, "icon-widget", &image_widget);
  glade_widget_property_get (widget, "label-widget", &label_widget);

  if (label_widget)
    glade_widget_property_set (widget, "custom-label", TRUE);
  else
    glade_widget_property_set (widget, "custom-label", FALSE);

  if (image_widget)
    glade_widget_property_set (widget, "image-mode", GLADE_TB_MODE_CUSTOM);
  else if (icon_name)
    glade_widget_property_set (widget, "image-mode", GLADE_TB_MODE_ICON);
  else if (stock_str)
    {
      /* Update the stock property */
      stock_id =
          glade_utils_enum_value_from_string (GLADE_TYPE_STOCK_IMAGE,
                                              stock_str);
      if (stock_id < 0)
        stock_id = 0;
      glade_widget_property_set (widget, "glade-stock", stock_id);

      glade_widget_property_set (widget, "image-mode", GLADE_TB_MODE_STOCK);
    }
  else
    glade_widget_property_set (widget, "image-mode", GLADE_TB_MODE_STOCK);
}

G_MODULE_EXPORT void
glade_gtk_tool_button_read_widget (GladeWidgetAdaptor *adaptor,
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
                    G_CALLBACK (glade_gtk_tool_button_parse_finished), widget);
}
