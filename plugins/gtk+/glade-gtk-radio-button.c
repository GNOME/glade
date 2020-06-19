/*
 * glade-gtk-radio-button.c - GladeWidgetAdaptor for GtkRadioButton
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

void
glade_gtk_radio_button_set_property (GladeWidgetAdaptor * adaptor,
                                     GObject * object,
                                     const gchar * property_name,
                                     const GValue * value)
{
  if (strcmp (property_name, "group") == 0)
    {
      GtkRadioButton *radio = g_value_get_object (value);
      /* g_object_set () on this property produces a bogus warning,
       * so we better use the API GtkRadioButton provides.
       */
      gtk_radio_button_set_group (GTK_RADIO_BUTTON (object),
                                  radio ? gtk_radio_button_get_group (radio) :
                                  NULL);
      return;
    }

  /* Chain Up */
  GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CHECK_BUTTON)->set_property (adaptor,
                                                       object,
                                                       property_name, value);
}
