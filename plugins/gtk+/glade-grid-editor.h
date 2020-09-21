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

#ifndef _GLADE_GRID_EDITOR_H_
#define _GLADE_GRID_EDITOR_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_GRID_EDITOR            (glade_grid_editor_get_type ())
#define GLADE_GRID_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_GRID_EDITOR, GladeGridEditor))
#define GLADE_GRID_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_GRID_EDITOR, GladeGridEditorClass))
#define GLADE_IS_GRID_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_GRID_EDITOR))
#define GLADE_IS_GRID_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_GRID_EDITOR))
#define GLADE_GRID_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_GRID_EDITOR, GladeGridEditorClass))

typedef struct _GladeGridEditor        GladeGridEditor;
typedef struct _GladeGridEditorClass   GladeGridEditorClass;
typedef struct _GladeGridEditorPrivate GladeGridEditorPrivate;

struct _GladeGridEditor
{
  GladeEditorSkeleton  parent;

  GladeGridEditorPrivate *priv;
};

struct _GladeGridEditorClass
{
  GladeEditorSkeletonClass parent;
};

GType            glade_grid_editor_get_type (void) G_GNUC_CONST;
GtkWidget       *glade_grid_editor_new      (void);

G_END_DECLS

#endif  /* _GLADE_GRID_EDITOR_H_ */
