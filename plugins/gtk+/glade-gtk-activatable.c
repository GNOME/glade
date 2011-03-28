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
#include <gladeui/glade.h>

/* ----------------------------- GtkActivatable ------------------------------ */
void
glade_gtk_activatable_parse_finished (GladeProject *project, 
                                      GladeWidget  *widget)
{
	GObject *related_action = NULL;

	glade_widget_property_get (widget, "related-action", &related_action);
	if (related_action == NULL)
		glade_widget_property_set (widget, "use-action-appearance", FALSE);
}

void
glade_gtk_evaluate_activatable_property_sensitivity (GObject *object,
                                                     const gchar *id,
                                                     const GValue *value)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);

  if (!strcmp (id, "related-action"))
    {
      GtkAction *action = g_value_get_object (value);
      
      if (action)
        {
          glade_widget_property_set_sensitive (gwidget, "visible", FALSE,
                                               ACTION_APPEARANCE_MSG);
          glade_widget_property_set_sensitive (gwidget, "sensitive", FALSE,
                                               ACTION_APPEARANCE_MSG);

          glade_widget_property_set_sensitive (gwidget, "accel-group", FALSE,
                                               ACTION_APPEARANCE_MSG);
        }
      else
        {
          glade_widget_property_set_sensitive (gwidget, "visible", TRUE, NULL);
          glade_widget_property_set_sensitive (gwidget, "sensitive", TRUE,
                                               NULL);

          glade_widget_property_set_sensitive (gwidget, "accel-group", TRUE,
                                               NULL);
        }

    }
  else if (!strcmp (id, "use-action-appearance"))
    {
      gboolean use_appearance = g_value_get_boolean (value);


      if (use_appearance)
        {
          glade_widget_property_set_sensitive (gwidget, "label", FALSE,
                                               ACTION_APPEARANCE_MSG);
          glade_widget_property_set_sensitive (gwidget, "use-underline", FALSE,
                                               ACTION_APPEARANCE_MSG);
          glade_widget_property_set_sensitive (gwidget, "stock", FALSE,
                                               ACTION_APPEARANCE_MSG);
          //glade_widget_property_set_sensitive (gwidget, "use-stock", FALSE, ACTION_APPEARANCE_MSG);
          glade_widget_property_set_sensitive (gwidget, "image", FALSE,
                                               ACTION_APPEARANCE_MSG);
          glade_widget_property_set_sensitive (gwidget, "custom-child", FALSE,
                                               ACTION_APPEARANCE_MSG);
          glade_widget_property_set_sensitive (gwidget, "stock-id", FALSE,
                                               ACTION_APPEARANCE_MSG);
          glade_widget_property_set_sensitive (gwidget, "label-widget", FALSE,
                                               ACTION_APPEARANCE_MSG);
          glade_widget_property_set_sensitive (gwidget, "icon-name", FALSE,
                                               ACTION_APPEARANCE_MSG);
          glade_widget_property_set_sensitive (gwidget, "icon-widget", FALSE,
                                               ACTION_APPEARANCE_MSG);
          glade_widget_property_set_sensitive (gwidget, "icon", FALSE,
                                               ACTION_APPEARANCE_MSG);
        }
      else
        {
          glade_widget_property_set_sensitive (gwidget, "label", TRUE, NULL);
          glade_widget_property_set_sensitive (gwidget, "use-underline", TRUE,
                                               NULL);
          glade_widget_property_set_sensitive (gwidget, "stock", TRUE, NULL);
          //glade_widget_property_set_sensitive (gwidget, "use-stock", TRUE, NULL);
          glade_widget_property_set_sensitive (gwidget, "image", TRUE, NULL);
          glade_widget_property_set_sensitive (gwidget, "custom-child", TRUE,
                                               NULL);
          glade_widget_property_set_sensitive (gwidget, "stock-id", TRUE, NULL);
          glade_widget_property_set_sensitive (gwidget, "label-widget", TRUE,
                                               NULL);
          glade_widget_property_set_sensitive (gwidget, "icon-name", TRUE,
                                               NULL);
          glade_widget_property_set_sensitive (gwidget, "icon-widget", TRUE,
                                               NULL);
          glade_widget_property_set_sensitive (gwidget, "icon", TRUE, NULL);
        }
    }
}
