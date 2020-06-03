/*
 * glade-tsort.h: a topological sorting algorithm implementation
 * 
 * Copyright (C) 2013 Juan Pablo Ugarte.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#ifndef __GLADE_TSORT_H__
#define __GLADE_TSORT_H__

#include <glib.h>
#include <gladeui/glade-macros.h>

G_BEGIN_DECLS

typedef struct __NodeEdge _NodeEdge;

struct __NodeEdge
{
  gpointer predecessor;
  gpointer successor;
};

GLADEUI_EXPORTS
GList *_node_edge_prepend   (GList *list,
                             gpointer predecessor,
                             gpointer successor);

GLADEUI_EXPORTS
void   _node_edge_list_free (GList *list);

GLADEUI_EXPORTS
GList *_glade_tsort         (GList **nodes,
                             GList **edges);

G_END_DECLS

#endif /* __GLADE_TSORT_H__ */
