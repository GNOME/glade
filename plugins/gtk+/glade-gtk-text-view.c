/*
 * glade-gtk-text-view.c - GladeWidgetAdaptor for GtkTextView
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

#include "glade-text-view-editor.h"

GladeEditable *
glade_gtk_text_view_create_editable (GladeWidgetAdaptor *adaptor,
                                     GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    {
      return (GladeEditable *)glade_text_view_editor_new ();
    }

  return GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}

static gboolean
glade_gtk_text_view_stop_double_click (GtkWidget      *widget,
                                       GdkEventButton *event,
                                       gpointer        user_data)
{
  /* Return True if the event is double or triple click */
  return (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS);
}

void
glade_gtk_text_view_post_create (GladeWidgetAdaptor *adaptor,
                                 GObject            *object,
                                 GladeCreateReason   reason)
{
  /* This makes gtk_text_view_set_buffer() stop complaining */
  gtk_drag_dest_set (GTK_WIDGET (object), 0, NULL, 0, 0);

  /* Glade hangs when a TextView gets a double click. So we stop them */
  g_signal_connect (object, "button-press-event",
                    G_CALLBACK (glade_gtk_text_view_stop_double_click), NULL);
}

void
glade_gtk_text_view_set_property (GladeWidgetAdaptor *adaptor,
                                  GObject            *object,
                                  const gchar        *property_name,
                                  const GValue       *value)
{
  if (strcmp (property_name, "buffer") == 0)
    {
      if (!g_value_get_object (value))
        return;
    }

  GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor,
                                                    object,
                                                    property_name, value);
}
