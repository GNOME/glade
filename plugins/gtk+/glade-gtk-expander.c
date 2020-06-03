/*
 * glade-gtk-expander.c - GladeWidgetAdaptor for GtkExpander
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

G_MODULE_EXPORT void
glade_gtk_expander_post_create (GladeWidgetAdaptor *adaptor,
                                GObject            *expander,
                                GladeCreateReason   reason)
{
  static GladeWidgetAdaptor *wadaptor = NULL;
  GladeWidget *gexpander, *glabel;
  GtkWidget *label;

  if (wadaptor == NULL)
    wadaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_LABEL);

  if (reason != GLADE_CREATE_USER)
    return;

  g_return_if_fail (GTK_IS_EXPANDER (expander));
  gexpander = glade_widget_get_from_gobject (expander);
  g_return_if_fail (GLADE_IS_WIDGET (gexpander));

  /* If we didnt put this object here... */
  if ((label = gtk_expander_get_label_widget (GTK_EXPANDER (expander))) == NULL
      || (glade_widget_get_from_gobject (label) == NULL))
    {
      glabel = glade_widget_adaptor_create_widget (wadaptor, FALSE,
                                                   "parent", gexpander,
                                                   "project",
                                                   glade_widget_get_project
                                                   (gexpander), NULL);

      glade_widget_property_set (glabel, "label", "expander");

      g_object_set_data (glade_widget_get_object (glabel), "special-child-type", "label_item");
      glade_widget_add_child (gexpander, glabel, FALSE);
    }

  gtk_expander_set_expanded (GTK_EXPANDER (expander), TRUE);

  gtk_container_add (GTK_CONTAINER (expander), glade_placeholder_new ());

}

G_MODULE_EXPORT void
glade_gtk_expander_replace_child (GladeWidgetAdaptor *adaptor,
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
      gtk_expander_set_label_widget (GTK_EXPANDER (container), new_widget);
      return;
    }

  /* Chain Up */
  GWA_GET_CLASS
      (GTK_TYPE_CONTAINER)->replace_child (adaptor,
                                           G_OBJECT (container),
                                           G_OBJECT (current),
                                           G_OBJECT (new_widget));
}


G_MODULE_EXPORT void
glade_gtk_expander_add_child (GladeWidgetAdaptor *adaptor,
                              GObject            *object,
                              GObject            *child)
{
  gchar *special_child_type;

  special_child_type = g_object_get_data (child, "special-child-type");

  if (special_child_type && !strcmp (special_child_type, "label"))
    {
      g_object_set_data (child, "special-child-type", "label_item");
      gtk_expander_set_label_widget (GTK_EXPANDER (object), GTK_WIDGET (child));
    }
  else if (special_child_type && !strcmp (special_child_type, "label_item"))
    {
      gtk_expander_set_label_widget (GTK_EXPANDER (object), GTK_WIDGET (child));
    }
  else
    /* Chain Up */
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->add (adaptor, object, child);
}

G_MODULE_EXPORT void
glade_gtk_expander_remove_child (GladeWidgetAdaptor *adaptor,
                                 GObject            *object,
                                 GObject            *child)
{
  gchar *special_child_type;

  special_child_type = g_object_get_data (child, "special-child-type");
  if (special_child_type && !strcmp (special_child_type, "label_item"))
    {
      gtk_expander_set_label_widget (GTK_EXPANDER (object),
                                     glade_placeholder_new ());
    }
  else
    {
      gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
      gtk_container_add (GTK_CONTAINER (object), glade_placeholder_new ());
    }
}

G_MODULE_EXPORT void
glade_gtk_expander_write_child (GladeWidgetAdaptor *adaptor,
                                GladeWidget        *widget,
                                GladeXmlContext    *context,
                                GladeXmlNode       *node)
{

  if (!glade_gtk_write_special_child_label_item (adaptor, widget, context, node,
                                                 GWA_GET_CLASS (GTK_TYPE_CONTAINER)->
                                                 write_child))
    /* Chain Up */
    GWA_GET_CLASS
        (GTK_TYPE_CONTAINER)->write_child (adaptor, widget, context, node);
}

