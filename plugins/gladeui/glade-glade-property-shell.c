/*
 * glade-glade-property-shell.c
 *
 * Copyright (C) 2013 Tristan Van Berkom.
 *
 * Author:
 *   Tristan Van Berkom <tvb@gnome.org>
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
 *
 */

#include <config.h>
#include <gladeui/glade.h>

G_MODULE_EXPORT void
glade_glade_property_shell_post_create (GladeWidgetAdaptor *adaptor,
					GObject            *object,
					GladeCreateReason   reason)
{
  gtk_container_add (GTK_CONTAINER (object),
		     gtk_label_new ("[ editor ]"));
}

