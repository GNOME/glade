/*
 * Copyright (C) 2013 Tristan Van Berkom.
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
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#ifndef _GLADE_GTK_CELL_LAYOUT_H_
#define _GLADE_GTK_CELL_LAYOUT_H_

#include <gtk/gtk.h>
#include <gladeui/glade.h>

G_BEGIN_DECLS

gboolean glade_gtk_cell_layout_sync_attributes (GObject *layout);

/* Base editor handlers */
gchar   *glade_gtk_cell_layout_get_display_name (GladeBaseEditor *editor,
                                                 GladeWidget     *gchild,
                                                 gpointer         user_data);
void     glade_gtk_cell_layout_child_selected   (GladeBaseEditor *editor,
                                                 GladeWidget     *gchild,
                                                 gpointer         data);
gboolean glade_gtk_cell_layout_move_child       (GladeBaseEditor *editor,
                                                 GladeWidget     *gparent,
                                                 GladeWidget     *gchild,
                                                 gpointer         data); 
GList   *glade_gtk_cell_layout_get_children     (GladeWidgetAdaptor *adaptor,
                                                 GObject            *container);

G_END_DECLS

#endif  /* _GLADE_GTK_CELL_LAYOUT_H_ */
