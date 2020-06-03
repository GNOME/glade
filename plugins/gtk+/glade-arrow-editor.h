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
#ifndef _GLADE_ARROW_EDITOR_H_
#define _GLADE_ARROW_EDITOR_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_ARROW_EDITOR             (glade_arrow_editor_get_type ())
#define GLADE_ARROW_EDITOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_ARROW_EDITOR, GladeArrowEditor))
#define GLADE_ARROW_EDITOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_ARROW_EDITOR, GladeArrowEditorClass))
#define GLADE_IS_ARROW_EDITOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_ARROW_EDITOR))
#define GLADE_IS_ARROW_EDITOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_ARROW_EDITOR))
#define GLADE_ARROW_EDITOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_ARROW_EDITOR, GladeArrowEditorClass))

typedef struct _GladeArrowEditor        GladeArrowEditor;
typedef struct _GladeArrowEditorClass   GladeArrowEditorClass;
typedef struct _GladeArrowEditorPrivate GladeArrowEditorPrivate;

typedef enum {
  GLADE_ARROW_MODE_ATTRIBUTES = 0, /* default */
  GLADE_ARROW_MODE_MARKUP,
  GLADE_ARROW_MODE_PATTERN
} GladeArrowContentMode;

typedef enum {
  GLADE_ARROW_WRAP_FREE = 0, /* default */
  GLADE_ARROW_SINGLE_LINE,
  GLADE_ARROW_WRAP_MODE
} GladeArrowWrapMode;

struct _GladeArrowEditor
{
  GladeEditorSkeleton parent;

  GladeArrowEditorPrivate *priv;
};

struct _GladeArrowEditorClass
{
  GladeEditorSkeletonClass parent;
};

G_MODULE_EXPORT
GType            glade_arrow_editor_get_type (void) G_GNUC_CONST;
G_MODULE_EXPORT
GtkWidget       *glade_arrow_editor_new      (void);

G_END_DECLS

#endif  /* _GLADE_ARROW_EDITOR_H_ */
