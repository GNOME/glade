/*
 * Copyright (C) 2006 Juan Pablo Ugarte.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */
#ifndef __GLADE_BASE_EDITOR_H__
#define __GLADE_BASE_EDITOR_H__

#include <gladeui/glade-widget.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_BASE_EDITOR         (glade_base_editor_get_type ())
#define GLADE_BASE_EDITOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GLADE_TYPE_BASE_EDITOR, GladeBaseEditor))
#define GLADE_BASE_EDITOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GLADE_TYPE_BASE_EDITOR, GladeBaseEditorClass))
#define GLADE_IS_BASE_EDITOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_TYPE_BASE_EDITOR))
#define GLADE_IS_BASE_EDITOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_TYPE_BASE_EDITOR))
#define GLADE_BASE_EDITOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_TYPE_BASE_EDITOR, GladeBaseEditorClass))

typedef struct _GladeBaseEditor        GladeBaseEditor;
typedef struct _GladeBaseEditorPrivate GladeBaseEditorPrivate;
typedef struct _GladeBaseEditorClass   GladeBaseEditorClass;

struct _GladeBaseEditor
{
  GtkBox parent;

  GladeBaseEditorPrivate *priv;
};

struct _GladeBaseEditorClass
{
  GtkBoxClass parent_class;

  void          (*child_selected)   (GladeBaseEditor *editor, GladeWidget *gchild);
  gboolean      (*change_type)      (GladeBaseEditor *editor, GladeWidget *gchild, GType type);
  gchar *       (*get_display_name) (GladeBaseEditor *editor, GladeWidget *gchild);
  GladeWidget * (*build_child)      (GladeBaseEditor *editor, GladeWidget *parent, GType type);
  gboolean      (*delete_child)     (GladeBaseEditor *editor, GladeWidget *parent, GladeWidget *gchild);
  gboolean      (*move_child)       (GladeBaseEditor *editor, GladeWidget *gparent, GladeWidget *gchild);

  void   (* glade_reserved1)   (void);
  void   (* glade_reserved2)   (void);
  void   (* glade_reserved3)   (void);
  void   (* glade_reserved4)   (void);
  void   (* glade_reserved5)   (void);
  void   (* glade_reserved6)   (void);
};


GType                glade_base_editor_get_type               (void);

GladeBaseEditor     *glade_base_editor_new                    (GObject *container,
                                                               GladeEditable *main_editable,
                                                               ...);

void                 glade_base_editor_append_types           (GladeBaseEditor *editor, 
                                                               GType parent_type,
                                                               ...);

void                 glade_base_editor_add_editable           (GladeBaseEditor     *editor,
                                                               GladeWidget         *gchild,
                                                               GladeEditorPageType  page);

void                 glade_base_editor_add_default_properties (GladeBaseEditor *editor,
                                                               GladeWidget *gchild);

void                 glade_base_editor_add_properties         (GladeBaseEditor *editor,
                                                               GladeWidget *gchild,
                                                               gboolean packing,
                                                               ...);

void                 glade_base_editor_add_label              (GladeBaseEditor *editor,
                                                               gchar *str);

void                 glade_base_editor_set_show_signal_editor (GladeBaseEditor *editor,
                                                               gboolean val);

/* Convenience functions */
GtkWidget           *glade_base_editor_pack_new_window        (GladeBaseEditor *editor,
                                                               gchar *title,
                                                               gchar *help_markup);

G_END_DECLS

#endif /* __GLADE_BASE_EDITOR_H__ */
