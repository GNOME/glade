/*
 * glade-gtk-action.c - GladeWidgetAdaptor for GtkAction
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
glade_gtk_action_post_create (GladeWidgetAdaptor * adaptor,
                              GObject * object, GladeCreateReason reason)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);

  if (reason == GLADE_CREATE_REBUILD)
    return;

  if (!gtk_action_get_name (GTK_ACTION (object)))
    glade_widget_property_set (gwidget, "name", "untitled");

  glade_widget_set_action_sensitive (gwidget, "launch_editor", FALSE);
  glade_widget_property_set_sensitive (gwidget, "accelerator", FALSE, 
				       ACTION_ACCEL_INSENSITIVE_MSG);
}
