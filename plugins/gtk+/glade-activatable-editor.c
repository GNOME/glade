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
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#include "glade-activatable-editor.h"

static void glade_activatable_editor_grab_focus (GtkWidget * widget);

struct _GladeActivatableEditorPrivate {
  GtkWidget *embed;
};

G_DEFINE_TYPE (GladeActivatableEditor, glade_activatable_editor, GLADE_TYPE_EDITOR_SKELETON);

static void
glade_activatable_editor_class_init (GladeActivatableEditorClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = glade_activatable_editor_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-activatable-editor.ui");
  gtk_widget_class_bind_child (widget_class, GladeActivatableEditorPrivate, embed);

  g_type_class_add_private (object_class, sizeof (GladeActivatableEditorPrivate));  

}

static void
glade_activatable_editor_init (GladeActivatableEditor * self)
{
  self->priv = 
    G_TYPE_INSTANCE_GET_PRIVATE (self,
				 GLADE_TYPE_ACTIVATABLE_EDITOR,
				 GladeActivatableEditorPrivate);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_activatable_editor_grab_focus (GtkWidget * widget)
{
  GladeActivatableEditor *activatable_editor =
      GLADE_ACTIVATABLE_EDITOR (widget);

  gtk_widget_grab_focus (activatable_editor->priv->embed);
}

GtkWidget *
glade_activatable_editor_new (GladeWidgetAdaptor * adaptor,
                              GladeEditable * embed)
{
  return g_object_new (GLADE_TYPE_ACTIVATABLE_EDITOR, NULL);
}
