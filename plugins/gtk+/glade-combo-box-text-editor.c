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

#include "glade-combo-box-text-editor.h"

static void glade_combo_box_text_editor_grab_focus (GtkWidget * widget);

struct _GladeComboBoxTextEditorPrivate
{
  GtkWidget *embed;
};

G_DEFINE_TYPE_WITH_PRIVATE (GladeComboBoxTextEditor, glade_combo_box_text_editor, GLADE_TYPE_EDITOR_SKELETON)

static void
glade_combo_box_text_editor_class_init (GladeComboBoxTextEditorClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = glade_combo_box_text_editor_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-combo-box-text-editor.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GladeComboBoxTextEditor, embed);
}

static void
glade_combo_box_text_editor_init (GladeComboBoxTextEditor * self)
{
  self->priv = glade_combo_box_text_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_combo_box_text_editor_grab_focus (GtkWidget * widget)
{
  GladeComboBoxTextEditor *combo_editor = GLADE_COMBO_BOX_TEXT_EDITOR (widget);

  gtk_widget_grab_focus (combo_editor->priv->embed);
}

G_MODULE_EXPORT GtkWidget *
glade_combo_box_text_editor_new (void)
{
  return g_object_new (GLADE_TYPE_COMBO_BOX_TEXT_EDITOR, NULL);
}
