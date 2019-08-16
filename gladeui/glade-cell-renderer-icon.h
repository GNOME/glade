/*
 * Copyright (C) 2010 Tristan Van Berkom.
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

#ifndef __GLADE_CELL_RENDERER_ICON_H__
#define __GLADE_CELL_RENDERER_ICON_H__

#include <gtk/gtk.h>


G_BEGIN_DECLS

#define GLADE_TYPE_CELL_RENDERER_ICON glade_cell_renderer_icon_get_type ()
G_DECLARE_DERIVABLE_TYPE (GladeCellRendererIcon, glade_cell_renderer_icon, GLADE, CELL_RENDERER_ICON, GtkCellRendererPixbuf)

struct _GladeCellRendererIconClass
{
  GtkCellRendererPixbufClass parent_class;

  void (* activate) (GladeCellRendererIcon *cell_renderer_icon,
                     const gchar           *path);
};

GtkCellRenderer *glade_cell_renderer_icon_new            (void);

gboolean        glade_cell_renderer_icon_get_active      (GladeCellRendererIcon *icon);
void            glade_cell_renderer_icon_set_active      (GladeCellRendererIcon *icon,
                                                          gboolean               setting);

gboolean        glade_cell_renderer_icon_get_activatable (GladeCellRendererIcon *icon);
void            glade_cell_renderer_icon_set_activatable (GladeCellRendererIcon *icon,
                                                          gboolean               setting);


G_END_DECLS

#endif /* __GLADE_CELL_RENDERER_ICON_H__ */
