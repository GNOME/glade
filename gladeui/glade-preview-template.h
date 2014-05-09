/*
 * glade-preview-template.h
 *
 * Copyright (C) 2013 Juan Pablo Ugarte
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

#ifndef _GLADE_PREVIEW_TEMPLATE_H_
#define _GLADE_PREVIEW_TEMPLATE_H_

#include <glib-object.h>

G_BEGIN_DECLS

GObject *
glade_preview_template_object_new (const gchar          *template_data,
                                   gsize                 len,
                                   GtkBuilderConnectFunc connect_func,
                                   gpointer              connect_data);

G_END_DECLS

#endif /* _GLADE_PREVIEW_TEMPLATE_H_ */
