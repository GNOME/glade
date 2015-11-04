/*
 * glade-gtk-popover.c - GladeWidgetAdaptor for GtkPopover
 *
 * Copyright (C) 2014 Red Hat, Inc
 *
 * Authors:
 *      Matthias Clasen <mclasen@redhat.com>
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

#include "glade-popover-editor.h"

GObject *
glade_gtk_popover_constructor (GType type,
                               guint n_construct_properties,
                               GObjectConstructParam * construct_properties)
{
  GladeWidgetAdaptor *adaptor;
  GObject *ret_obj;

  ret_obj = GWA_GET_OCLASS (GTK_TYPE_CONTAINER)->constructor
      (type, n_construct_properties, construct_properties);

  adaptor = GLADE_WIDGET_ADAPTOR (ret_obj);

  glade_widget_adaptor_action_remove (adaptor, "add_parent");
  glade_widget_adaptor_action_remove (adaptor, "remove_parent");

  return ret_obj;
}

static void
glade_gtk_stop_emission_POINTER (gpointer instance, gpointer dummy,
                                 gpointer data)
{
  g_signal_stop_emission (instance, GPOINTER_TO_UINT (data), 0);
}

static void
glade_gtk_popover_stop_offending_signals (GtkWidget * widget)
{
  static gpointer button_press = NULL,
                  button_release,
                  key_press;

  if (button_press == NULL)
    {
      button_press = GUINT_TO_POINTER (g_signal_lookup ("button-press-event", GTK_TYPE_WIDGET));
      button_release = GUINT_TO_POINTER (g_signal_lookup ("button-release-event", GTK_TYPE_WIDGET));
      key_press = GUINT_TO_POINTER (g_signal_lookup ("key-press-event", GTK_TYPE_WIDGET));
    }

  g_signal_connect (widget, "button-press-event",
                    G_CALLBACK (glade_gtk_stop_emission_POINTER), button_press);
  g_signal_connect (widget, "button-release-event",
                    G_CALLBACK (glade_gtk_stop_emission_POINTER), button_release);
  g_signal_connect (widget, "key-press-event",
                    G_CALLBACK (glade_gtk_stop_emission_POINTER), key_press);
}

void
glade_gtk_popover_post_create (GladeWidgetAdaptor *adaptor,
                               GObject            *object,
                               GladeCreateReason   reason)
{
  if (reason == GLADE_CREATE_USER)
    {
      gtk_container_add (GTK_CONTAINER (object), glade_placeholder_new ());
    }

  glade_gtk_popover_stop_offending_signals (GTK_WIDGET (object));
}

GladeEditable *
glade_gtk_popover_create_editable (GladeWidgetAdaptor * adaptor,
                                   GladeEditorPageType  type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_popover_editor_new ();
  else
    return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}

