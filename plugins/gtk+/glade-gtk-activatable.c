/*
 * glade-gtk-activatable.c
 *
 * Copyright (C) 2010 Tristan Van Berkom.
 *
 * Author:
 *   Tristan Van Berkom <tvb@gnome.org>
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
 *
 */

#include <config.h>
#include "glade-gtk-activatable.h"

/* ----------------------------- GtkActivatable ------------------------------ */
static void
update_use_action_appearance (GladeWidget *gwidget,
                              gboolean related_action,
                              gboolean use_appearance)
{
  gboolean sensitivity;
  gchar *msg;
  
  if (use_appearance)
    {
      sensitivity = FALSE;
      msg = ACTION_APPEARANCE_MSG;
    }
  else
    {
      sensitivity = TRUE;
      msg = NULL;
    }

  glade_widget_property_set_sensitive (gwidget, "label", sensitivity, msg);
  glade_widget_property_set_sensitive (gwidget, "use-underline", sensitivity, msg);
  glade_widget_property_set_sensitive (gwidget, "stock", sensitivity, msg);
  //glade_widget_property_set_sensitive (gwidget, "use-stock", sensitivity, msg);
  glade_widget_property_set_sensitive (gwidget, "image", sensitivity, msg);
  glade_widget_property_set_sensitive (gwidget, "image-position", sensitivity, msg);
  glade_widget_property_set_sensitive (gwidget, "custom-child", sensitivity, msg);
  glade_widget_property_set_sensitive (gwidget, "stock-id", sensitivity, msg);
  glade_widget_property_set_sensitive (gwidget, "label-widget", sensitivity, msg);
  glade_widget_property_set_sensitive (gwidget, "icon-name", sensitivity, msg);
  glade_widget_property_set_sensitive (gwidget, "icon-widget", sensitivity, msg);
  glade_widget_property_set_sensitive (gwidget, "icon", sensitivity, msg);

  if (related_action)
    {
      glade_widget_property_set_sensitive (gwidget, "visible", sensitivity, msg);
      glade_widget_property_set_sensitive (gwidget, "sensitive", sensitivity, msg);
      glade_widget_property_set_sensitive (gwidget, "accel-group", sensitivity, msg);
      glade_widget_property_set_sensitive (gwidget, "use-action-appearance",
                                           !sensitivity, sensitivity ? msg : NULL);
    }
}

void
glade_gtk_activatable_evaluate_property_sensitivity (GObject *object,
                                                     const gchar *id,
                                                     const GValue *value)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);

  if (!strcmp (id, "related-action"))
    {
      gboolean use_appearance = gtk_activatable_get_use_action_appearance (GTK_ACTIVATABLE (object));
      GtkAction *action = g_value_get_object (value);

      update_use_action_appearance (gwidget, TRUE, action);
    }
  else if (!strcmp (id, "use-action-appearance"))
    {
      GtkAction *action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (object));
      gboolean use_appearance = g_value_get_boolean (value);
      
      update_use_action_appearance (gwidget, FALSE, action && use_appearance);
    }
}
