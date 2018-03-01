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

GladeEditable *
glade_gtk_popover_create_editable (GladeWidgetAdaptor * adaptor,
                                   GladeEditorPageType  type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_popover_editor_new ();
  else
    return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}

static gint
popover_key_press (GtkWidget *popover, GdkEventKey *event, gpointer user_data)
{
  if (event->keyval == GDK_KEY_Escape)
    return TRUE;

  return FALSE;
}

void
glade_gtk_popover_post_create (GladeWidgetAdaptor *adaptor,
                               GObject            *popover,
                               GladeCreateReason   reason)
{
  /* Ignore some events causing the popover to disappear from the workspace
   */
  g_signal_connect (popover, "key-press-event",
                    G_CALLBACK (popover_key_press), NULL);

  gtk_popover_set_modal (GTK_POPOVER (popover), FALSE);
  gtk_popover_set_relative_to (GTK_POPOVER (popover), NULL);

  GWA_GET_CLASS (GTK_TYPE_CONTAINER)->post_create (adaptor, popover, reason);
}
