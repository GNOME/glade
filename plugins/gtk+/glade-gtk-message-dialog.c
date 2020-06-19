/*
 * glade-gtk-message-dialog.c - GladeWidgetAdaptor for GtkMessageDialog
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

static gboolean
glade_gtk_message_dialog_reset_image (GtkMessageDialog * dialog)
{
  GtkWidget *image;
  gint message_type;

  g_object_get (dialog, "message-type", &message_type, NULL);
  if (message_type != GTK_MESSAGE_OTHER)
    return FALSE;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  image = gtk_message_dialog_get_image (dialog);
G_GNUC_END_IGNORE_DEPRECATIONS
  if (glade_widget_get_from_gobject (image))
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_message_dialog_set_image (dialog,
                                    gtk_image_new_from_stock (NULL,
                                                              GTK_ICON_SIZE_DIALOG));
G_GNUC_END_IGNORE_DEPRECATIONS
      gtk_widget_show (image);

      return TRUE;
    }
  else
    return FALSE;
}

enum
{
  MD_IMAGE_ACTION_INVALID,
  MD_IMAGE_ACTION_RESET,
  MD_IMAGE_ACTION_SET
};

static gint
glade_gtk_message_dialog_image_determine_action (GtkMessageDialog * dialog,
                                                 const GValue * value,
                                                 GtkWidget ** image,
                                                 GladeWidget ** gimage)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GtkWidget *dialog_image = gtk_message_dialog_get_image (dialog);
G_GNUC_END_IGNORE_DEPRECATIONS

  *image = g_value_get_object (value);

  if (*image == NULL)
    if (dialog_image && glade_widget_get_from_gobject (dialog_image))
      return MD_IMAGE_ACTION_RESET;
    else
      return MD_IMAGE_ACTION_INVALID;
  else
    {
      *image = GTK_WIDGET (*image);
      if (dialog_image == *image)
        return MD_IMAGE_ACTION_INVALID;
      if (gtk_widget_get_parent (*image))
        return MD_IMAGE_ACTION_INVALID;

      *gimage = glade_widget_get_from_gobject (*image);

      if (!*gimage)
        {
          g_warning ("Setting property to an object outside the project");
          return MD_IMAGE_ACTION_INVALID;
        }

      if (glade_widget_get_parent (*gimage) ||
          GLADE_WIDGET_ADAPTOR_IS_TOPLEVEL (glade_widget_get_adaptor (*gimage)))
        return MD_IMAGE_ACTION_INVALID;

      return MD_IMAGE_ACTION_SET;
    }
}

void
glade_gtk_message_dialog_set_property (GladeWidgetAdaptor * adaptor,
                                       GObject * object,
                                       const gchar * id, const GValue * value)
{
  GtkMessageDialog *dialog = GTK_MESSAGE_DIALOG (object);
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);

  g_return_if_fail (gwidget);

  if (strcmp (id, "image") == 0)
    {
      GtkWidget *image = NULL;
      GladeWidget *gimage = NULL;
      gint rslt;

      rslt = glade_gtk_message_dialog_image_determine_action (dialog, value,
                                                              &image, &gimage);
      switch (rslt)
        {
          case MD_IMAGE_ACTION_INVALID:
            return;
          case MD_IMAGE_ACTION_RESET:
            glade_gtk_message_dialog_reset_image (dialog);
            return;
          case MD_IMAGE_ACTION_SET:
            break;              /* continue setting the property */
        }

      if (gtk_widget_get_parent (image))
        g_critical ("Image should have no parent now");

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_message_dialog_set_image (dialog, image);
G_GNUC_END_IGNORE_DEPRECATIONS

      {
        /* syncing "message-type" property */
        GladeProperty *property;

        property = glade_widget_get_property (gwidget, "message-type");
        if (!glade_property_equals (property, GTK_MESSAGE_OTHER))
          glade_command_set_property (property, GTK_MESSAGE_OTHER);
      }
    }
  else
    {
      /* We must reset the image to internal,
       * external image would otherwise become internal
       */
      if (!strcmp (id, "message-type") &&
          g_value_get_enum (value) != GTK_MESSAGE_OTHER)
        {
          GladeProperty *property;

          property = glade_widget_get_property (gwidget, "image");
          if (!glade_property_equals (property, NULL))
            glade_command_set_property (property, NULL);
        }
      /* Chain up, even if property us message-type because
       * it's not fully handled here
       */
      GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_DIALOG)->set_property (adaptor, object,
                                                     id, value);
    }
}

gboolean
glade_gtk_message_dialog_verify_property (GladeWidgetAdaptor * adaptor,
                                          GObject * object,
                                          const gchar * id,
                                          const GValue * value)
{
  if (!strcmp (id, "image"))
    {
      GtkWidget *image;
      GladeWidget *gimage;

      gboolean retval = MD_IMAGE_ACTION_INVALID !=
          glade_gtk_message_dialog_image_determine_action (GTK_MESSAGE_DIALOG
                                                           (object),
                                                           value, &image,
                                                           &gimage);

      return retval;
    }
  else if (GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->verify_property)
    return GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->verify_property (adaptor, object,
                                                                id, value);
  else
    return TRUE;
}

void
glade_gtk_message_dialog_get_property (GladeWidgetAdaptor * adaptor,
                                       GObject * object,
                                       const gchar * property_name,
                                       GValue * value)
{
  if (!strcmp (property_name, "image"))
    {
      GtkMessageDialog *dialog = GTK_MESSAGE_DIALOG (object);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      GtkWidget *image = gtk_message_dialog_get_image (dialog);
G_GNUC_END_IGNORE_DEPRECATIONS

      if (!glade_widget_get_from_gobject (image))
        g_value_set_object (value, NULL);
      else
        g_value_set_object (value, image);
    }
  else
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_DIALOG)->get_property (adaptor, object,
                                                   property_name, value);
}
