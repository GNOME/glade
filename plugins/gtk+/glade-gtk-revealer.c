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
#include <gladeui/glade.h>

G_MODULE_EXPORT void
glade_gtk_revealer_post_create (GladeWidgetAdaptor *adaptor,
                                GObject            *container,
                                GladeCreateReason   reason)
{
  if (reason == GLADE_CREATE_USER)
    {
      gtk_container_add (GTK_CONTAINER (container), glade_placeholder_new ());
    }

  /* We always show the widget in the workspace, and ignore reveal-child prop */
  gtk_revealer_set_reveal_child (GTK_REVEALER (container), TRUE);
}
