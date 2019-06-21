/*
 * glade-gtk-recent-chooser-menu.c - GladeWidgetAdaptor for GtkRecentChooserMenu
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

#include "glade-recent-chooser-menu-editor.h"

GladeEditable *
glade_gtk_recent_chooser_menu_create_editable (GladeWidgetAdaptor *adaptor,
                                               GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_recent_chooser_menu_editor_new ();

  return GWA_GET_CLASS (GTK_TYPE_MENU)->create_editable (adaptor, type);
}

void
glade_gtk_recent_chooser_menu_set_property (GladeWidgetAdaptor *adaptor,
                                            GObject            *object,
                                            const gchar        *id,
                                            const GValue       *value)
{
  GladeWidget *widget = glade_widget_get_from_gobject (object);
  GladeProperty *property = glade_widget_get_property (widget, id);

  if (GLADE_PROPERTY_DEF_VERSION_CHECK (glade_property_get_def (property), gtk_major_version, gtk_minor_version + 1))
    GWA_GET_CLASS (GTK_TYPE_MENU)->set_property (adaptor, object, id, value);
}
