/*
 * glade-inspector.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2007 Vincent Geddes
 *
 * Authors:
 *   Chema Celorio
 *   Tristan Van Berkom <tvb@gnome.org>
 *   Vincent Geddes <vincent.geddes@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
 
#ifndef __GLADE_INSPECTOR_H__
#define __GLADE_INSPECTOR_H__

#include <gtk/gtk.h>

#include <gladeui/glade-xml-utils.h>

G_BEGIN_DECLS

#define GLADE_TYPE_INSPECTOR glade_inspector_get_type ()
G_DECLARE_DERIVABLE_TYPE (GladeInspector, glade_inspector, GLADE, INSPECTOR, GtkBox)

/**
 * GladeInspector:
 *
 * The #GladeInspector struct contains private data only, and should be manipulated using the functions below.
 *
 */

struct _GladeInspectorClass
{
  GtkBoxClass parent_class;

  void   (* selection_changed) (GladeInspector *inspector);
  void   (* item_activated)    (GladeInspector *inspector);

  gpointer padding[4];
};

GtkWidget     *glade_inspector_new                (void);
GtkWidget     *glade_inspector_new_with_project   (GladeProject   *project);
void           glade_inspector_set_project        (GladeInspector *inspector,
                                                   GladeProject   *project);
GladeProject  *glade_inspector_get_project        (GladeInspector *inspector);
GList         *glade_inspector_get_selected_items (GladeInspector *inspector);

G_END_DECLS

#endif /* __GLADE_INSPECTOR_H__ */
