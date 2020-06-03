/*
 * glade-gtk-bin.c - GladeWidgetAdaptor for GtkBin
 *
 * Copyright (C) 2018 Endless Mobile, Inc.
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

struct _GladeInstantiableGtkBin
{
  GtkBin parent;
};

G_MODULE_EXPORT
G_DECLARE_FINAL_TYPE (GladeInstantiableGtkBin, glade_instantiable_gtk_bin, GLADE, INSTANTIABLE_GTK_BIN, GtkBin)
G_DEFINE_TYPE (GladeInstantiableGtkBin, glade_instantiable_gtk_bin, GTK_TYPE_BIN)

static void
glade_instantiable_gtk_bin_init (GladeInstantiableGtkBin *bin)
{
}

static void
glade_instantiable_gtk_bin_class_init (GladeInstantiableGtkBinClass *class)
{
}

