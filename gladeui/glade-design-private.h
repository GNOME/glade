/*
 * glade-design-private.h
 *
 * Copyright (C) 2011  Juan Pablo Ugarte
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

#ifndef __GLADE_DESIGN_PRIVATE_H__
#define __GLADE_DESIGN_PRIVATE_H__

#include "glade-design-view.h"
#include "glade-design-layout.h"
#include "glade-dnd.h"

G_BEGIN_DECLS

void _glade_design_layout_get_colors (GdkRGBA *c1, GdkRGBA *c2,
                                      GdkRGBA *c3, GdkRGBA *c4);

void _glade_design_layout_draw_node (cairo_t *cr,
                                     gdouble x,
                                     gdouble y,
                                     GdkRGBA *fg,
                                     GdkRGBA *bg);

void _glade_design_layout_draw_pushpin (cairo_t *cr,
                                        gdouble needle_length,
                                        GdkRGBA *outline,
                                        GdkRGBA *fill,
                                        GdkRGBA *bg,
                                        GdkRGBA *fg);

void _glade_design_layout_get_hot_point (GladeDesignLayout *layout,
                                         gint *x,
                                         gint *y);

GladeWidget * _glade_design_layout_get_child (GladeDesignLayout *layout);

GtkWidget   *_glade_design_layout_get_child_at_position (GtkWidget *widget,
                                                         gint       x, 
                                                         gint       y);

void         _glade_design_layout_set_highlight (GladeDesignLayout *layout,
                                                 GladeWidget       *drag);

G_END_DECLS

#endif /* __GLADE_DESIGN_PRIVATE_H__ */
