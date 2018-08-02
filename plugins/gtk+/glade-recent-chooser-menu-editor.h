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
#ifndef _GLADE_RECENT_CHOOSER_MENU_EDITOR_H_
#define _GLADE_RECENT_CHOOSER_MENU_EDITOR_H_

#include <gtk/gtk.h>
#include "glade-window-editor.h"

G_BEGIN_DECLS

#define GLADE_TYPE_RECENT_CHOOSER_MENU_EDITOR            (glade_recent_chooser_menu_editor_get_type ())
#define GLADE_RECENT_CHOOSER_MENU_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_RECENT_CHOOSER_MENU_EDITOR, GladeRecentChooserMenuEditor))
#define GLADE_RECENT_CHOOSER_MENU_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_RECENT_CHOOSER_MENU_EDITOR, GladeRecentChooserMenuEditorClass))
#define GLADE_IS_RECENT_CHOOSER_MENU_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_RECENT_CHOOSER_MENU_EDITOR))
#define GLADE_IS_RECENT_CHOOSER_MENU_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_RECENT_CHOOSER_MENU_EDITOR))
#define GLADE_RECENT_CHOOSER_MENU_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_RECENT_CHOOSER_MENU_EDITOR, GladeRecentChooserMenuEditorClass))

typedef struct _GladeRecentChooserMenuEditor        GladeRecentChooserMenuEditor;
typedef struct _GladeRecentChooserMenuEditorClass   GladeRecentChooserMenuEditorClass;
typedef struct _GladeRecentChooserMenuEditorPrivate GladeRecentChooserMenuEditorPrivate;

struct _GladeRecentChooserMenuEditor
{
  GladeEditorSkeleton  parent;

  GladeRecentChooserMenuEditorPrivate *priv;
};

struct _GladeRecentChooserMenuEditorClass
{
  GladeEditorSkeletonClass parent;
};

GType            glade_recent_chooser_menu_editor_get_type (void) G_GNUC_CONST;
GtkWidget       *glade_recent_chooser_menu_editor_new      (void);

G_END_DECLS

#endif  /* _GLADE_RECENT_CHOOSER_MENU_EDITOR_H_ */
