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
#include "glade-action-editor.h"
#include "glade-recent-action-editor.h"

G_MODULE_EXPORT void
glade_gtk_action_post_create (GladeWidgetAdaptor *adaptor,
                              GObject            *object,
                              GladeCreateReason   reason)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);

  if (reason == GLADE_CREATE_REBUILD)
    return;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (!gtk_action_get_name (GTK_ACTION (object)))
G_GNUC_END_IGNORE_DEPRECATIONS
    {
      glade_widget_property_set (gwidget, "name", "untitled");
    }

  glade_widget_set_action_sensitive (gwidget, "launch_editor", FALSE);
  glade_widget_property_set_sensitive (gwidget, "accelerator", FALSE, 
                                       ACTION_ACCEL_INSENSITIVE_MSG);
}

G_MODULE_EXPORT GladeEditable *
glade_gtk_action_create_editable (GladeWidgetAdaptor *adaptor,
                                  GladeEditorPageType type)
{
  GladeEditable *editable;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GType type_recent_action = GTK_TYPE_RECENT_ACTION;
G_GNUC_END_IGNORE_DEPRECATIONS

  if (type == GLADE_PAGE_GENERAL)
    {
      GType action_type = glade_widget_adaptor_get_object_type (adaptor);

      if (g_type_is_a (action_type, type_recent_action))
        editable = (GladeEditable *) glade_recent_action_editor_new ();
      else
        editable = (GladeEditable *) glade_action_editor_new ();
    }
  else
    editable = GWA_GET_CLASS (G_TYPE_OBJECT)->create_editable (adaptor, type);

  return editable;
}
