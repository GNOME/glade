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
#ifndef __GLADE_EDITOR_SKELETON_H__
#define __GLADE_EDITOR_SKELETON_H__

#include <gtk/gtk.h>
#include <gladeui/glade-xml-utils.h>
#include <gladeui/glade-editable.h>

G_BEGIN_DECLS

#define GLADE_TYPE_EDITOR_SKELETON            (glade_editor_skeleton_get_type ())
#define GLADE_EDITOR_SKELETON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EDITOR_SKELETON, GladeEditorSkeleton))
#define GLADE_EDITOR_SKELETON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EDITOR_SKELETON, GladeEditorSkeletonClass))
#define GLADE_IS_EDITOR_SKELETON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EDITOR_SKELETON))
#define GLADE_IS_EDITOR_SKELETON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EDITOR_SKELETON))
#define GLADE_EDITOR_SKELETON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_EDITOR_SKELETON, GladeEditorSkeletonClass))

typedef struct _GladeEditorSkeleton             GladeEditorSkeleton;
typedef struct _GladeEditorSkeletonClass        GladeEditorSkeletonClass;
typedef struct _GladeEditorSkeletonPrivate      GladeEditorSkeletonPrivate;

struct _GladeEditorSkeleton
{
  /*< private >*/
  GtkBox box;

  GladeEditorSkeletonPrivate *priv;
};

struct _GladeEditorSkeletonClass
{
  GtkBoxClass parent_class;
};

GType          glade_editor_skeleton_get_type          (void) G_GNUC_CONST;

GtkWidget     *glade_editor_skeleton_new               (void);
void           glade_editor_skeleton_add_editor        (GladeEditorSkeleton *skeleton,
                                                        GladeEditable       *editor);

G_END_DECLS

#endif /* __GLADE_EDITOR_SKELETON_H__ */
