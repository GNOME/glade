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

#include <config.h>
#include <gladeui/glade.h>
#include <glib/gi18n-lib.h>

#include "glade-scrolled-window-editor.h"

struct _GladeScrolledWindowEditorPrivate
{
  GtkWidget *embed;
};

static void glade_scrolled_window_editor_grab_focus (GtkWidget * widget);

G_DEFINE_TYPE_WITH_PRIVATE (GladeScrolledWindowEditor, glade_scrolled_window_editor, GLADE_TYPE_EDITOR_SKELETON)

static void
glade_scrolled_window_editor_class_init (GladeScrolledWindowEditorClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = glade_scrolled_window_editor_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-scrolled-window-editor.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GladeScrolledWindowEditor, embed);
}

static void
glade_scrolled_window_editor_init (GladeScrolledWindowEditor * self)
{
  self->priv = glade_scrolled_window_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_scrolled_window_editor_grab_focus (GtkWidget * widget)
{
  GladeScrolledWindowEditor *swindow_editor = GLADE_SCROLLED_WINDOW_EDITOR (widget);

  gtk_widget_grab_focus (swindow_editor->priv->embed);
}

GtkWidget *
glade_scrolled_window_editor_new (void)
{
  return g_object_new (GLADE_TYPE_SCROLLED_WINDOW_EDITOR, NULL);
}
