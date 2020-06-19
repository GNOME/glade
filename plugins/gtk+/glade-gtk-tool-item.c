/*
 * glade-gtk-tool-item.c - GladeWidgetAdaptor for GtkToolItem
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

GObject *
glade_gtk_tool_item_constructor (GType type,
                                 guint n_construct_properties,
                                 GObjectConstructParam *construct_properties)
{
  GladeWidgetAdaptor *adaptor;
  GObject *ret_obj;

  ret_obj = GLADE_WIDGET_ADAPTOR_GET_OCLASS (GTK_TYPE_CONTAINER)->constructor
      (type, n_construct_properties, construct_properties);

  adaptor = GLADE_WIDGET_ADAPTOR (ret_obj);

  glade_widget_adaptor_action_remove (adaptor, "add_parent");
  glade_widget_adaptor_action_remove (adaptor, "remove_parent");

  return ret_obj;
}

void
glade_gtk_tool_item_post_create (GladeWidgetAdaptor *adaptor,
                                 GObject            *object, 
                                 GladeCreateReason   reason)
{
  if (GTK_IS_SEPARATOR_TOOL_ITEM (object))
    return;

  if (reason == GLADE_CREATE_USER &&
      gtk_bin_get_child (GTK_BIN (object)) == NULL)
    gtk_container_add (GTK_CONTAINER (object), glade_placeholder_new ());
}

void
glade_gtk_tool_item_set_property (GladeWidgetAdaptor *adaptor,
                                  GObject            *object,
                                  const gchar        *id,
                                  const GValue       *value)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);
  GladeProperty *property = glade_widget_get_property (gwidget, id);

  if (GLADE_PROPERTY_DEF_VERSION_CHECK
      (glade_property_get_def (property), gtk_major_version, gtk_minor_version + 1))
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id,
                                                      value);
}
