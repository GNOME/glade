/*
 * glade-gtk-revealer.c - GladeWidgetAdaptor for GtkRevealer
 *
 * Copyright (C) 2013 Juan Pablo Ugarte
 *
 * Authors:
 *      Juan Pablo Ugarte <juanpablougarte@gmail.com>
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

void
glade_gtk_revealer_post_create (GladeWidgetAdaptor *adaptor,
                                GObject            *container,
                                GladeCreateReason   reason)
{
  if (reason == GLADE_CREATE_USER)
    {
      gtk_container_add (GTK_CONTAINER (container), glade_placeholder_new ());
    }
  gtk_revealer_set_reveal_child (GTK_REVEALER (container), TRUE);
}

void
glade_gtk_revealer_set_property (GladeWidgetAdaptor *adaptor,
                                 GObject            *object,
                                 const gchar        *id,
                                 const GValue       *value)
{
  if (!g_strcmp0 (id, "glade-test-reveal"))
    gtk_revealer_set_reveal_child (GTK_REVEALER (object),
                                   g_value_get_boolean (value));
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id, value);
}
