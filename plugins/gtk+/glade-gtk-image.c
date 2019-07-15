/*
 * glade-gtk-image.c - GladeWidgetAdaptor for GtkImage class
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

#include "glade-image-editor.h"
#include "glade-gtk-image.h"
#include "glade-gtk.h"

void
glade_gtk_image_read_widget (GladeWidgetAdaptor *adaptor,
                             GladeWidget        *widget,
                             GladeXmlNode       *node)
{
  GladeProperty *property;

  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (GTK_TYPE_WIDGET)->read_widget (adaptor, widget, node);

  if (glade_widget_property_original_default (widget, "icon-name") == FALSE)
    {
      property = glade_widget_get_property (widget, "icon-name");
      glade_widget_property_set (widget, "image-mode", GLADE_IMAGE_MODE_ICON);
    }
  else if (glade_widget_property_original_default (widget, "resource") == FALSE)
    {
      property = glade_widget_get_property (widget, "resource");
      glade_widget_property_set (widget, "image-mode", GLADE_IMAGE_MODE_RESOURCE);
    }
  else if (glade_widget_property_original_default (widget, "pixbuf") == FALSE)
    {
      property = glade_widget_get_property (widget, "pixbuf");
      glade_widget_property_set (widget, "image-mode",
                                 GLADE_IMAGE_MODE_FILENAME);
    }
  else  /*  if (glade_widget_property_original_default (widget, "stock") == FALSE) */
    {
      property = glade_widget_get_property (widget, "stock");
      glade_widget_property_set (widget, "image-mode", GLADE_IMAGE_MODE_STOCK);
    }

  glade_property_sync (property);
}

void
glade_gtk_image_write_widget (GladeWidgetAdaptor *adaptor,
                              GladeWidget        *widget,
                              GladeXmlContext    *context,
                              GladeXmlNode       *node)
{

  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and write all the normal properties (including "use-stock")... */
  GWA_GET_CLASS (GTK_TYPE_WIDGET)->write_widget (adaptor, widget, context, node);

  glade_gtk_write_icon_size (widget, context, node, "icon-size");
}


static void
glade_gtk_image_set_image_mode (GObject *object, const GValue *value)
{
  GladeWidget *gwidget;
  GladeImageEditMode type;

  gwidget = glade_widget_get_from_gobject (object);
  g_return_if_fail (GTK_IS_IMAGE (object));
  g_return_if_fail (GLADE_IS_WIDGET (gwidget));

  glade_widget_property_set_sensitive (gwidget, "stock", FALSE,
                                       NOT_SELECTED_MSG);
  glade_widget_property_set_sensitive (gwidget, "icon-name", FALSE,
                                       NOT_SELECTED_MSG);
  glade_widget_property_set_sensitive (gwidget, "pixbuf", FALSE,
                                       NOT_SELECTED_MSG);
  glade_widget_property_set_sensitive (gwidget, "resource", FALSE,
                                       NOT_SELECTED_MSG);
  glade_widget_property_set_sensitive (gwidget, "icon-size", FALSE,
                                       _("This property only applies to stock images or named icons"));
  glade_widget_property_set_sensitive (gwidget, "pixel-size", FALSE,
                                       _("This property only applies to named icons"));
  glade_widget_property_set_sensitive (gwidget, "use-fallback", FALSE,
                                       _("This property only applies to named icons"));

  switch ((type = g_value_get_int (value)))
    {
      case GLADE_IMAGE_MODE_STOCK:
        glade_widget_property_set_sensitive (gwidget, "stock", TRUE, NULL);
        glade_widget_property_set_sensitive (gwidget, "icon-size", TRUE, NULL);
        break;

      case GLADE_IMAGE_MODE_ICON:
        glade_widget_property_set_sensitive (gwidget, "icon-name", TRUE, NULL);
        glade_widget_property_set_sensitive (gwidget, "icon-size", TRUE, NULL);
        glade_widget_property_set_sensitive (gwidget, "pixel-size", TRUE, NULL);
        glade_widget_property_set_sensitive (gwidget, "use-fallback", TRUE, NULL);
        break;

      case GLADE_IMAGE_MODE_RESOURCE:
        glade_widget_property_set_sensitive (gwidget, "resource", TRUE, NULL);
        break;

      case GLADE_IMAGE_MODE_FILENAME:
        glade_widget_property_set_sensitive (gwidget, "pixbuf", TRUE, NULL);
      default:
        break;
    }
}

void
glade_gtk_image_get_property (GladeWidgetAdaptor *adaptor,
                              GObject            *object,
                              const gchar        *id,
                              GValue             *value)
{
  if (!strcmp (id, "icon-size"))
    {
      /* Make the int an enum... */
      GValue int_value = { 0, };
      g_value_init (&int_value, G_TYPE_INT);
      GWA_GET_CLASS (GTK_TYPE_WIDGET)->get_property (adaptor, object, id,
                                                     &int_value);
      g_value_set_enum (value, g_value_get_int (&int_value));
      g_value_unset (&int_value);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_WIDGET)->get_property (adaptor, object, id, value);
}

void
glade_gtk_image_set_property (GladeWidgetAdaptor *adaptor,
                              GObject            *object,
                              const gchar        *id,
                              const GValue       *value)
{
  if (!strcmp (id, "image-mode"))
    glade_gtk_image_set_image_mode (object, value);
  else if (!strcmp (id, "icon-size"))
    {
      /* Make the enum an int... */
      GValue int_value = { 0, };
      g_value_init (&int_value, G_TYPE_INT);
      g_value_set_int (&int_value, g_value_get_enum (value));
      GWA_GET_CLASS (GTK_TYPE_WIDGET)->set_property (adaptor, object, id,
                                                     &int_value);
      g_value_unset (&int_value);
    }
  else
    {
      GladeWidget *widget = glade_widget_get_from_gobject (object);
      GladeImageEditMode mode = 0;

      glade_widget_property_get (widget, "image-mode", &mode);

      /* avoid setting properties in the wrong mode... */
      switch (mode)
        {
          case GLADE_IMAGE_MODE_STOCK:
            if (!strcmp (id, "icon-name") || !strcmp (id, "pixbuf"))
              return;
            break;
          case GLADE_IMAGE_MODE_ICON:
            if (!strcmp (id, "stock") || !strcmp (id, "pixbuf"))
              return;
            break;
          case GLADE_IMAGE_MODE_FILENAME:
            if (!strcmp (id, "stock") || !strcmp (id, "icon-name"))
              return;
          case GLADE_IMAGE_MODE_RESOURCE:
            /* Screw the resource mode here, we can't apply them at Glade's runtime anyway  */
          default:
            break;
        }

      GWA_GET_CLASS (GTK_TYPE_WIDGET)->set_property (adaptor, object,
                                                     id, value);

      /* Set icon size back to what it should be */
      if (!strcmp (id, "icon-name") || !strcmp (id, "stock"))
        glade_property_sync (glade_widget_get_property (widget, "icon-size"));
    }
}

GladeEditable *
glade_gtk_image_create_editable (GladeWidgetAdaptor *adaptor,
                                 GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_image_editor_new ();
  else
    return GWA_GET_CLASS (GTK_TYPE_WIDGET)->create_editable (adaptor, type);
}

/* Shared with other classes */
void
glade_gtk_write_icon_size (GladeWidget     *widget,
                           GladeXmlContext *context,
                           GladeXmlNode    *node,
                           const gchar     *prop_name)
{
  GladeXmlNode *prop_node;
  GladeProperty *size_prop;
  GtkIconSize icon_size;
  gchar *value;

  /* We have to save icon-size as an integer, the core will take care of 
   * loading the int value though.
   */
  size_prop = glade_widget_get_property (widget, prop_name);
  if (glade_property_get_enabled (size_prop) &&
      !glade_property_original_default (size_prop))
    {
      gchar *write_prop_name = g_strdup (prop_name);

      glade_util_replace (write_prop_name, '-', '_');

      prop_node = glade_xml_node_new (context, GLADE_TAG_PROPERTY);
      glade_xml_node_append_child (node, prop_node);

      glade_xml_node_set_property_string (prop_node, GLADE_TAG_NAME, write_prop_name);

      glade_property_get (size_prop, &icon_size);
      value = g_strdup_printf ("%d", icon_size);
      glade_xml_set_content (prop_node, value);
      g_free (value);
      g_free (write_prop_name);
    }
}
