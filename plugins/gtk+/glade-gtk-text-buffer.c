/*
 * glade-gtk-text-buffer.c - GladeWidgetAdaptor for GtkTextBuffer
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

static void
glade_gtk_text_buffer_changed (GtkTextBuffer * buffer, GladeWidget * gbuffy)
{
  const gchar *text_prop = NULL;
  GladeProperty *prop;
  gchar *text = NULL;

  g_object_get (buffer, "text", &text, NULL);

  if ((prop = glade_widget_get_property (gbuffy, "text")))
    {
      glade_property_get (prop, &text_prop);

      if (g_strcmp0 (text, text_prop))
        glade_command_set_property (prop, text);
    }
  g_free (text);
}

G_MODULE_EXPORT void
glade_gtk_text_buffer_post_create (GladeWidgetAdaptor * adaptor,
                                   GObject * object, GladeCreateReason reason)
{
  GladeWidget *gbuffy;

  gbuffy = glade_widget_get_from_gobject (object);

  g_signal_connect (object, "changed",
                    G_CALLBACK (glade_gtk_text_buffer_changed), gbuffy);
}

G_MODULE_EXPORT void
glade_gtk_text_buffer_set_property (GladeWidgetAdaptor * adaptor,
                                    GObject * object,
                                    const gchar * id, const GValue * value)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);
  GladeProperty *property = glade_widget_get_property (gwidget, id);

  if (!strcmp (id, "text"))
    {
      g_signal_handlers_block_by_func (object, glade_gtk_text_buffer_changed,
                                       gwidget);

      if (g_value_get_string (value))
        gtk_text_buffer_set_text (GTK_TEXT_BUFFER (object),
                                  g_value_get_string (value), -1);
      else
        gtk_text_buffer_set_text (GTK_TEXT_BUFFER (object), "", -1);

      g_signal_handlers_unblock_by_func (object, glade_gtk_text_buffer_changed,
                                         gwidget);

    }
  else if (GLADE_PROPERTY_DEF_VERSION_CHECK
           (glade_property_get_def (property), gtk_major_version, gtk_minor_version + 1))
    GWA_GET_CLASS (G_TYPE_OBJECT)->set_property (adaptor, object, id, value);
}
