/*
 * Copyright (C) 2014 Red Hat, Inc.
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
 *   Matthias Clasen <mclasen@redhat.com>
 */
#ifndef _GLADE_MODEL_BUTTON_EDITOR_H_
#define _GLADE_MODEL_BUTTON_EDITOR_H_

#include <gtk/gtk.h>
#include <gladeui/glade.h>

G_BEGIN_DECLS

#define GLADE_TYPE_MODEL_BUTTON_EDITOR            (glade_model_button_editor_get_type ())
#define GLADE_MODEL_BUTTON_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_MODEL_BUTTON_EDITOR, GladeModelButtonEditor))
#define GLADE_MODEL_BUTTON_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_MODEL_BUTTON_EDITOR, GladeModelButtonEditorClass))
#define GLADE_IS_MODEL_BUTTON_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_MODEL_BUTTON_EDITOR))
#define GLADE_IS_MODEL_BUTTON_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_MODEL_BUTTON_EDITOR))
#define GLADE_MODEL_BUTTON_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_MODEL_BUTTON_EDITOR, GladeModelButtonEditorClass))

typedef struct _GladeModelButtonEditor        GladeModelButtonEditor;
typedef struct _GladeModelButtonEditorClass   GladeModelButtonEditorClass;
typedef struct _GladeModelButtonEditorPrivate GladeModelButtonEditorPrivate;

struct _GladeModelButtonEditor
{
  GladeEditorSkeleton  parent;

  GladeModelButtonEditorPrivate *priv;
};

struct _GladeModelButtonEditorClass
{
  GladeEditorSkeletonClass parent;
};

GType            glade_model_button_editor_get_type       (void) G_GNUC_CONST;
GtkWidget       *glade_model_button_editor_new            (void);

G_END_DECLS

#endif  /* _GLADE_MODEL_BUTTON_EDITOR_H_ */
