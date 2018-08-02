/*
 * glade-gtk-scrolled-window.c - GladeWidgetAdaptor for GtkScrolledWindow
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

#include "glade-scrolled-window-editor.h"

GladeEditable *
glade_gtk_scrolled_window_create_editable (GladeWidgetAdaptor *adaptor,
                                           GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    {
      return (GladeEditable *)glade_scrolled_window_editor_new ();
    }

  return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}

void
glade_gtk_scrolled_window_set_property (GladeWidgetAdaptor *adaptor,
                                        GObject            *object,
                                        const gchar        *id,
                                        const GValue       *value)
{
  GladeWidget *widget = glade_widget_get_from_gobject (object);
  GladeProperty *property = glade_widget_get_property (widget, id);

  if (strcmp (id, "window-placement-set") == 0)
    {
      if (g_value_get_boolean (value))
        glade_widget_property_set_sensitive (widget, "window-placement", TRUE, NULL);
      else
        glade_widget_property_set_sensitive (widget, "window-placement", FALSE,
                                             _("This property is disabled"));
    }
  else if (GPC_VERSION_CHECK (glade_property_get_class (property), gtk_major_version, gtk_minor_version + 1))
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id, value);
}

gboolean
glade_gtk_scrolled_window_add_child_verify (GladeWidgetAdaptor *adaptor,
                                            GtkWidget          *container,
                                            GtkWidget          *child,
                                            gboolean            user_feedback)
{
  GladeWidget *gcontainer = glade_widget_get_from_gobject (container);
  GladeWidget *gchild = glade_widget_get_from_gobject (child);

  return !glade_util_check_and_warn_scrollable (gcontainer,
                                                glade_widget_get_adaptor (gchild),
                                                glade_app_get_window ());
}
