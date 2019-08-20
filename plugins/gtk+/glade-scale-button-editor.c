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

#include "glade-scale-button-editor.h"

static void glade_scale_button_editor_editable_init (GladeEditableInterface * iface);

struct _GladeScaleButtonEditorPrivate
{
  GtkWidget *dummy;
};

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeScaleButtonEditor, glade_scale_button_editor, GLADE_TYPE_BUTTON_EDITOR,
                         G_ADD_PRIVATE (GladeScaleButtonEditor)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_scale_button_editor_editable_init));

static void
glade_scale_button_editor_class_init (GladeScaleButtonEditorClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-scale-button-editor.ui");
}

static void
glade_scale_button_editor_init (GladeScaleButtonEditor * self)
{
  self->priv = glade_scale_button_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_scale_button_editor_load (GladeEditable * editable, GladeWidget * widget)
{
  /* GladeScaleButtonEditor *button_editor = GLADE_SCALE_BUTTON_EDITOR (editable); */
  /* GladeScaleButtonEditorPrivate *priv = button_editor->priv; */

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);
}

static void
glade_scale_button_editor_editable_init (GladeEditableInterface * iface)
{
  parent_editable_iface = g_type_interface_peek_parent (iface);

  iface->load = glade_scale_button_editor_load;
}

GtkWidget *
glade_scale_button_editor_new (void)
{
  return g_object_new (GLADE_TYPE_SCALE_BUTTON_EDITOR, NULL);
}
