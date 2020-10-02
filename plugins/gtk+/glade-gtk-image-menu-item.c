/*
 * glade-gtk-image-menu-item.c - GladeWidgetAdaptor for GtkImageMenuItem
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

#include "glade-image-item-editor.h"
#include "glade-gtk-button.h"
#include "glade-gtk.h"

static void
glade_gtk_image_menu_item_set_use_stock (GObject *object, const GValue *value)
{
  GladeWidget *widget = glade_widget_get_from_gobject (object);
  gboolean use_stock;

  use_stock = g_value_get_boolean (value);

  /* Set some things */
  if (use_stock)
    {
      glade_widget_property_set_sensitive (widget, "stock", TRUE, NULL);
      glade_widget_property_set_sensitive (widget, "accel-group", TRUE, NULL);
    }
  else
    {
      glade_widget_property_set_sensitive (widget, "stock", FALSE,
                                           NOT_SELECTED_MSG);
      glade_widget_property_set_sensitive (widget, "accel-group", FALSE,
                                           NOT_SELECTED_MSG);
    }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_image_menu_item_set_use_stock (GTK_IMAGE_MENU_ITEM (object), use_stock);
G_GNUC_END_IGNORE_DEPRECATIONS

  glade_gtk_sync_use_appearance (widget);
}

static gboolean
glade_gtk_image_menu_item_set_label (GObject *object, const GValue *value)
{
  GladeWidget *gitem;
  GtkWidget *label;
  gboolean use_underline = FALSE, use_stock = FALSE;
  const gchar *text;

  gitem = glade_widget_get_from_gobject (object);
  label = gtk_bin_get_child (GTK_BIN (object));

  glade_widget_property_get (gitem, "use-stock", &use_stock);
  glade_widget_property_get (gitem, "use-underline", &use_underline);
  text = g_value_get_string (value);

  /* In "use-stock" mode we dont have a GladeWidget child image */
  if (use_stock)
    {
      GtkWidget *image;
      GtkStockItem item;
      gboolean valid_item;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      image = gtk_image_new_from_stock (g_value_get_string (value),
                                        GTK_ICON_SIZE_MENU);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (object), image);
      valid_item = (text) ? gtk_stock_lookup (text, &item) : FALSE;
G_GNUC_END_IGNORE_DEPRECATIONS

      if (use_underline)
        gtk_label_set_use_underline (GTK_LABEL (label), TRUE);

      /* Get the label string... */
      if (valid_item)
        gtk_label_set_label (GTK_LABEL (label), item.label);
      else
        gtk_label_set_label (GTK_LABEL (label), text ? text : "");

      return TRUE;
    }

  return FALSE;
}

static void
glade_gtk_image_menu_item_set_stock (GObject *object, const GValue *value)
{
  GladeWidget *gitem;
  gboolean use_stock = FALSE;

  gitem = glade_widget_get_from_gobject (object);

  glade_widget_property_get (gitem, "use-stock", &use_stock);

  /* Forward the work along to the label handler...  */
  if (use_stock)
    glade_gtk_image_menu_item_set_label (object, value);
}

void
glade_gtk_image_menu_item_set_property (GladeWidgetAdaptor *adaptor,
                                        GObject            *object,
                                        const gchar        *id,
                                        const GValue       *value)
{
  if (!strcmp (id, "stock"))
    glade_gtk_image_menu_item_set_stock (object, value);
  else if (!strcmp (id, "use-stock"))
    glade_gtk_image_menu_item_set_use_stock (object, value);
  else if (!strcmp (id, "label"))
    {
      if (!glade_gtk_image_menu_item_set_label (object, value))
        GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_MENU_ITEM)->set_property (adaptor, object,
                                                          id, value);
    }
  else
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_MENU_ITEM)->set_property (adaptor, object,
                                                      id, value);
}

static void
glade_gtk_image_menu_item_parse_finished (GladeProject *project, GObject *object)
{
  GladeWidget *widget = glade_widget_get_from_gobject (object);
  GladeWidget *gimage;
  GtkWidget *image = NULL;
  glade_widget_property_get (widget, "image", &image);

  if (image && (gimage = glade_widget_get_from_gobject (image)))
    glade_widget_lock (widget, gimage);
}

void
glade_gtk_image_menu_item_read_widget (GladeWidgetAdaptor *adaptor,
                                       GladeWidget        *widget,
                                       GladeXmlNode       *node)
{
  GladeProperty *property;
  gboolean use_stock;
  gchar *label = NULL;

  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_MENU_ITEM)->read_widget (adaptor, widget, node);

  glade_widget_property_get (widget, "use-stock", &use_stock);
  if (use_stock)
    {
      property = glade_widget_get_property (widget, "label");

      glade_property_get (property, &label);
      glade_widget_property_set (widget, "use-underline", TRUE);
      glade_widget_property_set (widget, "stock", label);
      glade_property_sync (property);
    }

  /* Update sensitivity of related properties...  */
  property = glade_widget_get_property (widget, "use-stock");
  glade_property_sync (property);

  /* Run this after the load so that image is resolved. */
  g_signal_connect_object (glade_widget_get_project (widget), "parse-finished",
                           G_CALLBACK (glade_gtk_image_menu_item_parse_finished),
                           glade_widget_get_object (widget),
                           0);
}


void
glade_gtk_image_menu_item_write_widget (GladeWidgetAdaptor *adaptor,
                                        GladeWidget        *widget,
                                        GladeXmlContext    *context,
                                        GladeXmlNode       *node)
{
  GladeProperty *label_prop;
  gboolean use_stock;
  gchar *stock;

  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* Make a copy of the GladeProperty, override its value if use-stock is TRUE */
  label_prop = glade_widget_get_property (widget, "label");
  label_prop = glade_property_dup (label_prop, widget);
  glade_widget_property_get (widget, "use-stock", &use_stock);
  if (use_stock)
    {
      glade_widget_property_get (widget, "stock", &stock);
      glade_property_set (label_prop, stock);
      glade_property_i18n_set_translatable (label_prop, FALSE);
    }
  glade_property_write (label_prop, context, node);
  g_object_unref (G_OBJECT (label_prop));

  /* Chain up and write all the normal properties ... */
  GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_MENU_ITEM)->write_widget (adaptor, widget, context,
                                                    node);

}

/* We need write_widget to write child images as internal, in builder, they are
 * attached as a property
 */

GladeEditable *
glade_gtk_image_menu_item_create_editable (GladeWidgetAdaptor *adaptor,
                                           GladeEditorPageType type)
{
  GladeEditable *editable;

  /* Get base editable */
  editable =
      GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_MENU_ITEM)->create_editable (adaptor, type);

  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_image_item_editor_new (adaptor, editable);

  return editable;
}
