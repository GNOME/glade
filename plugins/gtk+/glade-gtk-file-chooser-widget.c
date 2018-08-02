/*
 * glade-gtk-file-chooser-widget.c - GladeWidgetAdaptor for GtkFileChooserWidget
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

#include "glade-gtk-dialog.h"
#include "glade-file-chooser-widget-editor.h"
#include "glade-file-chooser-button-editor.h"

void
glade_gtk_file_chooser_widget_post_create (GladeWidgetAdaptor *adaptor,
                                           GObject            *object,
                                           GladeCreateReason   reason)
{
  gtk_container_forall (GTK_CONTAINER (object),
                        glade_gtk_file_chooser_default_forall, NULL);
}

GladeEditable *
glade_gtk_file_chooser_widget_create_editable (GladeWidgetAdaptor *adaptor,
                                               GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_file_chooser_widget_editor_new ();

  return GWA_GET_CLASS (GTK_TYPE_WIDGET)->create_editable (adaptor, type);
}


void
glade_gtk_file_chooser_button_set_property (GladeWidgetAdaptor *adaptor,
                                            GObject            *object,
                                            const gchar        *id,
                                            const GValue       *value)
{
  /* Avoid a warning */
  if (!strcmp (id, "action"))
    {
      if (g_value_get_enum (value) == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER ||
          g_value_get_enum (value) == GTK_FILE_CHOOSER_ACTION_SAVE)
        return;
    }

  GWA_GET_CLASS (GTK_TYPE_BOX)->set_property (adaptor, object, id, value);
}


GladeEditable *
glade_gtk_file_chooser_button_create_editable (GladeWidgetAdaptor *adaptor,
                                               GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_file_chooser_button_editor_new ();

  return GWA_GET_CLASS (GTK_TYPE_WIDGET)->create_editable (adaptor, type);
}
