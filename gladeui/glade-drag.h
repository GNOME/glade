/*
 * glade-drag.h
 *
 * Copyright (C) 2013  Juan Pablo Ugarte
 *
 * Authors:
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
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

#ifndef _GLADE_DRAG_H_
#define _GLADE_DRAG_H_

#include "glade-widget-adaptor.h"

G_BEGIN_DECLS

#define GLADE_TYPE_DRAG _glade_drag_get_type ()
G_DECLARE_INTERFACE (_GladeDrag, _glade_drag, GLADE, DRAG, GObject)

struct __GladeDragInterface
{
  GTypeInterface parent_instance;
 
  gboolean (*can_drag)  (_GladeDrag *source);
  
  gboolean (*can_drop)  (_GladeDrag *dest,
                         gint       x,
                         gint       y,
                         GObject   *data);

  gboolean (*drop)      (_GladeDrag *dest,
                         gint       x,
                         gint       y,
                         GObject   *data);

  void     (*highlight) (_GladeDrag *dest,
                         gint       x,
                         gint       y);
};

gboolean _glade_drag_can_drag  (_GladeDrag *source);

gboolean _glade_drag_can_drop  (_GladeDrag *dest,
                                gint       x,
                                gint       y,
                                GObject   *data);

gboolean _glade_drag_drop      (_GladeDrag *dest,
                                gint       x,
                                gint       y,
                                GObject   *data);

void     _glade_drag_highlight (_GladeDrag *dest,
                                gint       x,
                                gint       y);

G_END_DECLS

#endif /* _GLADE_DRAG_DEST_H_ */
