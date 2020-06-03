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
#ifndef _GLADE_STACK_SWITCHER_EDITOR_H_
#define _GLADE_STACK_SWITCHER_EDITOR_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_STACK_SWITCHER_EDITOR            (glade_stack_switcher_editor_get_type ())
#define GLADE_STACK_SWITCHER_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_STACK_SWITCHER_EDITOR, GladeStackSwitcherEditor))
#define GLADE_STACK_SWITCHER_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_STACK_SWITCHER_EDITOR, GladeStackSwitcherEditorClass))
#define GLADE_IS_STACK_SWITCHER_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_STACK_SWITCHER_EDITOR))
#define GLADE_IS_STACK_SWITCHER_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_STACK_SWITCHER_EDITOR))
#define GLADE_STACK_SWITCHER_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_STACK_SWITCHER_EDITOR, GladeStackSwitcherEditorClass))

typedef struct _GladeStackSwitcherEditor        GladeStackSwitcherEditor;
typedef struct _GladeStackSwitcherEditorClass   GladeStackSwitcherEditorClass;
typedef struct _GladeStackSwitcherEditorPrivate GladeStackSwitcherEditorPrivate;

struct _GladeStackSwitcherEditor
{
  GladeEditorSkeleton  parent;

  GladeStackSwitcherEditorPrivate *priv;
};

struct _GladeStackSwitcherEditorClass
{
  GladeEditorSkeletonClass parent;
};

G_MODULE_EXPORT
GType            glade_stack_switcher_editor_get_type (void) G_GNUC_CONST;
G_MODULE_EXPORT
GtkWidget       *glade_stack_switcher_editor_new      (void);

G_END_DECLS

#endif  /* _GLADE_STACK_SWITCHER_EDITOR_H_ */
