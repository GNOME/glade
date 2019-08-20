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

static void glade_activatable_editor_editable_init (GladeEditableInterface * iface);
static void glade_activatable_editor_grab_focus (GtkWidget * widget);

struct _GladeActivatableEditorPrivate {
  GtkWidget *embed;

  GtkWidget *action_name_label;
  GtkWidget *action_name_editor;
};

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeActivatableEditor, glade_activatable_editor, GLADE_TYPE_EDITOR_SKELETON,
                         G_ADD_PRIVATE (GladeActivatableEditor)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_activatable_editor_editable_init));

static void
glade_activatable_editor_class_init (GladeActivatableEditorClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = glade_activatable_editor_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-activatable-editor.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GladeActivatableEditor, embed);
  gtk_widget_class_bind_template_child_private (widget_class, GladeActivatableEditor, action_name_label);
  gtk_widget_class_bind_template_child_private (widget_class, GladeActivatableEditor, action_name_editor);
}

static void
glade_activatable_editor_init (GladeActivatableEditor * self)
{
  self->priv = glade_activatable_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_activatable_editor_grab_focus (GtkWidget * widget)
{
  GladeActivatableEditor *activatable_editor =
      GLADE_ACTIVATABLE_EDITOR (widget);

  gtk_widget_grab_focus (activatable_editor->priv->embed);
}

static void
glade_activatable_editor_load (GladeEditable *editable,
                               GladeWidget   *gwidget)
{
  GladeActivatableEditor *activatable_editor = GLADE_ACTIVATABLE_EDITOR (editable);
  GladeActivatableEditorPrivate *priv = activatable_editor->priv;
  GtkWidget *widget;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, gwidget);

  if (gwidget)
    {
      gboolean actionable;

      widget = (GtkWidget *)glade_widget_get_object (gwidget);
      actionable = GTK_IS_ACTIONABLE (widget);

      gtk_widget_set_visible (priv->action_name_label, actionable);
      gtk_widget_set_visible (priv->action_name_editor, actionable);
    }
}

static void
glade_activatable_editor_editable_init (GladeEditableInterface * iface)
{
  parent_editable_iface = g_type_interface_peek_parent (iface);

  iface->load = glade_activatable_editor_load;
}

GtkWidget *
glade_activatable_editor_new (GladeWidgetAdaptor * adaptor,
                              GladeEditable * embed)
{
  return g_object_new (GLADE_TYPE_ACTIVATABLE_EDITOR, NULL);
}
