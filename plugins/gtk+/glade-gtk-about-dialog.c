/*
 * glade-gtk-about-dialog.c - GladeWidgetAdaptor for GtkAboutDialog
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

void
glade_gtk_about_dialog_read_widget (GladeWidgetAdaptor *adaptor,
                                    GladeWidget        *widget,
                                    GladeXmlNode       *node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_WIDGET)->read_widget (adaptor, widget, node);

  /* Sync the logo icon mode */
  if (glade_widget_property_original_default (widget, "logo") == FALSE)
    glade_widget_property_set (widget, "glade-logo-as-file", TRUE);
  else
    glade_widget_property_set (widget, "glade-logo-as-file", FALSE);
}

void
glade_gtk_about_dialog_set_property (GladeWidgetAdaptor *adaptor,
                                     GObject            *object,
                                     const gchar        *id,
                                     const GValue       *value)
{
  if (!strcmp (id, "glade-logo-as-file"))
    {
      GladeWidget *gwidget = glade_widget_get_from_gobject (object);
      GladeProperty *logo = glade_widget_get_property (gwidget, "logo");
      GladeProperty *icon = glade_widget_get_property (gwidget, "logo-icon-name");
      gboolean as_file = g_value_get_boolean (value);

      glade_property_set_sensitive (icon, !as_file, as_file ? NOT_SELECTED_MSG : NULL);
      glade_property_set_enabled (icon, !as_file);

      glade_property_set_sensitive (logo, as_file, as_file ? NULL : NOT_SELECTED_MSG);
      glade_property_set_enabled (logo, as_file);
    }
  else
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_DIALOG)->set_property (adaptor, object, id, value);
}
