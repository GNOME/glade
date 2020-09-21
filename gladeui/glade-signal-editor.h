/*
 * Copyright (C) 2001 Ximian, Inc.
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
 *   Shane Butler <shane_b@users.sourceforge.net>
 *   Joaquin Cuenca Abela <e98cuenc@yahoo.com>
 */

#ifndef __GLADE_SIGNAL_EDITOR_H__
#define __GLADE_SIGNAL_EDITOR_H__

#include <gladeui/glade.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_SIGNAL_EDITOR glade_signal_editor_get_type ()
G_DECLARE_DERIVABLE_TYPE (GladeSignalEditor, glade_signal_editor, GLADE, SIGNAL_EDITOR, GtkBox)

/* The GladeSignalEditor is used to house the signal editor interface and
 * associated functionality.
 */

struct _GladeSignalEditorClass
{
  GtkBoxClass parent_class;

  gchar ** (* callback_suggestions) (GladeSignalEditor *editor, GladeSignal *signal);
  gchar ** (* detail_suggestions) (GladeSignalEditor *editor, GladeSignal *signal);

  gpointer padding[4];
};

GladeSignalEditor *glade_signal_editor_new                    (void);
void               glade_signal_editor_load_widget            (GladeSignalEditor *editor,
                                                               GladeWidget       *widget);
GladeWidget       *glade_signal_editor_get_widget             (GladeSignalEditor *editor);

void               glade_signal_editor_enable_dnd             (GladeSignalEditor *editor,
                                                               gboolean           enabled);

G_END_DECLS

#endif /* __GLADE_SIGNAL_EDITOR_H__ */
