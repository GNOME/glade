/*
 * glade-gtk-progress-bar.c - GladeWidgetAdaptor for GtkProgressBar
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

#include "glade-progress-bar-editor.h"

#define TEXT_DISABLED_MSG _("This progressbar does not show text")

GladeEditable *
glade_gtk_progress_bar_create_editable (GladeWidgetAdaptor *adaptor,
                                        GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    {
      return (GladeEditable *)glade_progress_bar_editor_new ();
    }

  return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}

void
glade_gtk_progress_bar_set_property (GladeWidgetAdaptor *adaptor,
                                     GObject            *object,
                                     const gchar        *id,
                                     const GValue       *value)
{
  GladeWidget *widget = glade_widget_get_from_gobject (object);
  GladeProperty *property = glade_widget_get_property (widget, id);

  if (strcmp (id, "show-text") == 0)
    {
      if (g_value_get_boolean (value))
        {
          glade_widget_property_set_sensitive (widget, "text", TRUE, NULL);
          glade_widget_property_set_sensitive (widget, "ellipsize", TRUE, NULL);
        }
      else
        {
          glade_widget_property_set_sensitive (widget, "text", FALSE, TEXT_DISABLED_MSG);
          glade_widget_property_set_sensitive (widget, "ellipsize", FALSE, TEXT_DISABLED_MSG);
        }
    }

  if (GPC_VERSION_CHECK (glade_property_get_class (property), gtk_major_version, gtk_minor_version + 1))
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id, value);
}
