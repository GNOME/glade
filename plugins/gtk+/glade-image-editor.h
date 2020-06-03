/*
 * Copyright (C) 2008 Tristan Van Berkom.
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
#ifndef _GLADE_IMAGE_EDITOR_H_
#define _GLADE_IMAGE_EDITOR_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_IMAGE_EDITOR            (glade_image_editor_get_type ())
#define GLADE_IMAGE_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_IMAGE_EDITOR, GladeImageEditor))
#define GLADE_IMAGE_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_IMAGE_EDITOR, GladeImageEditorClass))
#define GLADE_IS_IMAGE_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_IMAGE_EDITOR))
#define GLADE_IS_IMAGE_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_IMAGE_EDITOR))
#define GLADE_IMAGE_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_IMAGE_EDITOR, GladeImageEditorClass))

typedef struct _GladeImageEditor        GladeImageEditor;
typedef struct _GladeImageEditorClass   GladeImageEditorClass;
typedef struct _GladeImageEditorPrivate GladeImageEditorPrivate;

typedef enum {
  GLADE_IMAGE_MODE_STOCK = 0, /* default */
  GLADE_IMAGE_MODE_ICON,
  GLADE_IMAGE_MODE_RESOURCE,
  GLADE_IMAGE_MODE_FILENAME
} GladeImageEditMode;

struct _GladeImageEditor
{
  GladeEditorSkeleton  parent;

  GladeImageEditorPrivate *priv;
};

struct _GladeImageEditorClass
{
  GladeEditorSkeletonClass parent;
};

G_MODULE_EXPORT
GType            glade_image_editor_get_type (void) G_GNUC_CONST;
G_MODULE_EXPORT
GtkWidget       *glade_image_editor_new      (void);

G_END_DECLS

#endif  /* _GLADE_IMAGE_EDITOR_H_ */
