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

#include <config.h>
#include <gladeui/glade.h>
#include <glib/gi18n-lib.h>

#include "glade-toggle-button-editor.h"

static void glade_toggle_button_editor_grab_focus (GtkWidget * widget);

struct _GladeToggleButtonEditorPrivate
{
  GtkWidget *embed;
};

G_DEFINE_TYPE (GladeToggleButtonEditor, glade_toggle_button_editor, GLADE_TYPE_EDITOR_SKELETON);

static void
glade_toggle_button_editor_class_init (GladeToggleButtonEditorClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = glade_toggle_button_editor_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-toggle-button-editor.ui");
  gtk_widget_class_bind_child (widget_class, GladeToggleButtonEditorPrivate, embed);

  g_type_class_add_private (object_class, sizeof (GladeToggleButtonEditorPrivate));  
}

static void
glade_toggle_button_editor_init (GladeToggleButtonEditor * self)
{
  self->priv = 
    G_TYPE_INSTANCE_GET_PRIVATE (self,
				 GLADE_TYPE_TOGGLE_BUTTON_EDITOR,
				 GladeToggleButtonEditorPrivate);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_toggle_button_editor_grab_focus (GtkWidget * widget)
{
  GladeToggleButtonEditor *toggle_button_editor = GLADE_TOGGLE_BUTTON_EDITOR (widget);

  gtk_widget_grab_focus (toggle_button_editor->priv->embed);
}

GtkWidget *
glade_toggle_button_editor_new (void)
{
  return g_object_new (GLADE_TYPE_TOGGLE_BUTTON_EDITOR, NULL);
}
