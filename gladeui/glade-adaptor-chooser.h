/*
 * glade-adaptor-chooser.h
 *
 * Copyright (C) 2017 Juan Pablo Ugarte
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
 * Authors:
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#ifndef _GLADE_ADAPTOR_CHOOSER_H_
#define _GLADE_ADAPTOR_CHOOSER_H_

#include <gladeui/glade-widget-adaptor.h>
#include <gladeui/glade-project.h>

G_BEGIN_DECLS

#define GLADE_TYPE_ADAPTOR_CHOOSER (glade_adaptor_chooser_get_type ())
G_DECLARE_FINAL_TYPE (GladeAdaptorChooser, glade_adaptor_chooser, GLADE, ADAPTOR_CHOOSER, GtkBox)

GtkWidget    *glade_adaptor_chooser_new (void);

void          glade_adaptor_chooser_set_project (GladeAdaptorChooser *bar,
                                                 GladeProject        *project);
GladeProject *glade_adaptor_chooser_get_project (GladeAdaptorChooser *bar);

G_END_DECLS

#endif /* _GLADE_ADAPTOR_CHOOSER_H_ */
