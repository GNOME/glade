/*
 * glade-gtk-frame.c - GladeWidgetAdaptor for GtkFrame
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

#include "glade-gtk-frame.h"

void
glade_gtk_frame_post_create (GladeWidgetAdaptor *adaptor,
                             GObject            *frame,
                             GladeCreateReason   reason)
{
  static GladeWidgetAdaptor *label_adaptor = NULL, *alignment_adaptor = NULL;
  GladeWidget *gframe, *glabel, *galignment;
  GtkWidget *label;

  if (reason != GLADE_CREATE_USER)
    return;

  g_return_if_fail (GTK_IS_FRAME (frame));
  gframe = glade_widget_get_from_gobject (frame);
  g_return_if_fail (GLADE_IS_WIDGET (gframe));

  /* If we didnt put this object here or if frame is an aspect frame... */
  if (((label = gtk_frame_get_label_widget (GTK_FRAME (frame))) == NULL ||
       (glade_widget_get_from_gobject (label) == NULL)) &&
      (GTK_IS_ASPECT_FRAME (frame) == FALSE))
    {

      if (label_adaptor == NULL)
        label_adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_LABEL);
      if (alignment_adaptor == NULL)
        alignment_adaptor =
            glade_widget_adaptor_get_by_type (GTK_TYPE_ALIGNMENT);

      /* add label (as an internal child) */
      glabel = glade_widget_adaptor_create_widget (label_adaptor, FALSE,
                                                   "parent", gframe,
                                                   "project",
                                                   glade_widget_get_project
                                                   (gframe), NULL);

      glade_widget_property_set (glabel, "label", glade_widget_get_name (gframe));
      g_object_set_data (glade_widget_get_object (glabel), 
                         "special-child-type", "label_item");
      glade_widget_add_child (gframe, glabel, FALSE);

      /* add alignment */
      galignment = glade_widget_adaptor_create_widget (alignment_adaptor, FALSE,
                                                       "parent", gframe,
                                                       "project",
                                                       glade_widget_get_project
                                                       (gframe), NULL);

      glade_widget_property_set (galignment, "left-padding", 12);
      glade_widget_add_child (gframe, galignment, FALSE);
    }

  /* Chain Up */
  GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->post_create (adaptor, frame, reason);
}

void
glade_gtk_frame_replace_child (GladeWidgetAdaptor *adaptor,
                               GtkWidget          *container,
                               GtkWidget          *current,
                               GtkWidget          *new_widget)
{
  gchar *special_child_type;

  special_child_type =
      g_object_get_data (G_OBJECT (current), "special-child-type");

  if (special_child_type && !strcmp (special_child_type, "label_item"))
    {
      g_object_set_data (G_OBJECT (new_widget), "special-child-type",
                         "label_item");
      gtk_frame_set_label_widget (GTK_FRAME (container), new_widget);
      return;
    }

  /* Chain Up */
  GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS
      (GTK_TYPE_CONTAINER)->replace_child (adaptor,
                                           G_OBJECT (container),
                                           G_OBJECT (current),
                                           G_OBJECT (new_widget));
}

void
glade_gtk_frame_add_child (GladeWidgetAdaptor *adaptor,
                           GObject            *object,
                           GObject            *child)
{
  GtkWidget *bin_child;
  gchar *special_child_type;

  special_child_type = g_object_get_data (child, "special-child-type");

  if (special_child_type && !strcmp (special_child_type, "label"))
    {
      g_object_set_data (child, "special-child-type", "label_item");
      gtk_frame_set_label_widget (GTK_FRAME (object), GTK_WIDGET (child));
    }
  else if (special_child_type && !strcmp (special_child_type, "label_item"))
    {
      gtk_frame_set_label_widget (GTK_FRAME (object), GTK_WIDGET (child));
    }
  else
    {
      /* Get a placeholder out of the way before adding the child
       */
      bin_child = gtk_bin_get_child (GTK_BIN (object));
      if (bin_child)
        {
          if (GLADE_IS_PLACEHOLDER (bin_child))
            gtk_container_remove (GTK_CONTAINER (object), bin_child);
          else
            {
              g_critical ("Cant add more than one widget to a GtkFrame");
              return;
            }
        }
      gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));
    }
}

void
glade_gtk_frame_remove_child (GladeWidgetAdaptor *adaptor,
                              GObject            *object,
                              GObject            *child)
{
  gchar *special_child_type;

  special_child_type = g_object_get_data (child, "special-child-type");
  if (special_child_type && !strcmp (special_child_type, "label_item"))
    {
      gtk_frame_set_label_widget (GTK_FRAME (object), glade_placeholder_new ());
    }
  else
    {
      gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
      gtk_container_add (GTK_CONTAINER (object), glade_placeholder_new ());
    }
}

void
glade_gtk_frame_write_child (GladeWidgetAdaptor *adaptor,
                             GladeWidget        *widget,
                             GladeXmlContext    *context,
                             GladeXmlNode       *node)
{

  if (!glade_gtk_write_special_child_label_item (adaptor, widget, context, node,
                                                 GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->write_child))
    /* Chain Up */
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS
        (GTK_TYPE_CONTAINER)->write_child (adaptor, widget, context, node);
}

/* Shared with GtkExpander code */
gboolean
glade_gtk_write_special_child_label_item (GladeWidgetAdaptor  *adaptor,
                                          GladeWidget         *widget,
                                          GladeXmlContext     *context,
                                          GladeXmlNode        *node,
                                          GladeWriteWidgetFunc write_func)
{
  gchar *special_child_type = NULL;
  GObject *child;

  child = glade_widget_get_object (widget);
  if (child)
    special_child_type = g_object_get_data (child, "special-child-type");

  if (special_child_type && !strcmp (special_child_type, "label_item"))
    {
      g_object_set_data (child, "special-child-type", "label");
      write_func (adaptor, widget, context, node);
      g_object_set_data (child, "special-child-type", "label_item");
      return TRUE;
    }
  else
    return FALSE;
}
