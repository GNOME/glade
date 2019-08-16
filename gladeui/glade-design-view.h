/*
 * glade-design-view.h
 *
 * Copyright (C) 2006 Vincent Geddes
 *
 * Authors:
 *   Vincent Geddes <vincent.geddes@gmail.com>
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
 
#ifndef __GLADE_DESIGN_VIEW_H__
#define __GLADE_DESIGN_VIEW_H__

#include <gladeui/glade.h>
#include <gladeui/glade-project.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_DESIGN_VIEW glade_design_view_get_type ()
G_DECLARE_DERIVABLE_TYPE (GladeDesignView, glade_design_view, GLADE, DESIGN_VIEW, GtkBox)

struct _GladeDesignViewClass
{
  GtkBoxClass parent_class;

  gpointer padding[4];
};

GtkWidget         *glade_design_view_new              (GladeProject *project);

GladeProject      *glade_design_view_get_project      (GladeDesignView *view);

GladeDesignView   *glade_design_view_get_from_project (GladeProject *project);

G_END_DECLS

#endif /* __GLADE_DESIGN_VIEW_H__ */
