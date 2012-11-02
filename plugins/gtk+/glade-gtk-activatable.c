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
void
glade_gtk_activatable_parse_finished (GladeProject *project, 
                                      GladeWidget  *widget)
{
  GObject *related_action = NULL;

  glade_widget_property_get (widget, "related-action", &related_action);
  if (related_action == NULL)
    {
      glade_widget_property_set_sensitive (widget, "use-action-appearance", FALSE, ACTION_APPEARANCE_MSG);
      glade_widget_property_set (widget, "use-action-appearance", FALSE);
    }
}

void
glade_gtk_activatable_evaluate_property_sensitivity (GObject *object,
                                                     const gchar *id,
                                                     const GValue *value)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);
  gboolean sensitivity;
  gchar *msg;

  if (!strcmp (id, "related-action"))
    {
      GtkAction *action = g_value_get_object (value);
      
      if (action)
        {
          sensitivity = FALSE;
          msg = ACTION_APPEARANCE_MSG;
        }
      else
        {
          sensitivity = TRUE;
          msg = NULL;
        }

      glade_widget_property_set_sensitive (gwidget, "visible", sensitivity, msg);
      glade_widget_property_set_sensitive (gwidget, "sensitive", sensitivity, msg);
      glade_widget_property_set_sensitive (gwidget, "accel-group", sensitivity, msg);
      glade_widget_property_set_sensitive (gwidget, "use-action-appearance", !sensitivity, sensitivity ? msg : NULL);
    }
  else if (!strcmp (id, "use-action-appearance"))
    {
      if (g_value_get_boolean (value))
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
    }
}
