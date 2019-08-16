/*
 * glade-object-stub.h
 *
 * Copyright (C) 2011 Juan Pablo Ugarte
 *
 * Author: Juan Pablo Ugarte <juanpablougarte@gmail.com>
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

#ifndef _GLADE_OBJECT_STUB_H_
#define _GLADE_OBJECT_STUB_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_OBJECT_STUB glade_object_stub_get_type ()
G_DECLARE_FINAL_TYPE (GladeObjectStub, glade_object_stub, GLADE, OBJECT_STUB, GtkInfoBar)

G_END_DECLS

#endif /* _GLADE_OBJECT_STUB_H_ */
