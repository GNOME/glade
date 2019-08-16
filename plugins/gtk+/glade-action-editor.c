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
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#include "glade-action-editor.h"

static void glade_action_editor_editable_init (GladeEditableInterface * iface);
static void glade_action_editor_grab_focus (GtkWidget * widget);

struct _GladeActionEditorPrivate {
  GtkWidget *embed;
  GtkWidget *extension_port;

  /* GtkToggleAction/GtkRadioAction widgets */
  GtkWidget *toggle_title;
  GtkWidget *radio_proxy_editor;
  GtkWidget *toggle_active_editor;
  GtkWidget *radio_group_label;
  GtkWidget *radio_group_editor;
  GtkWidget *radio_value_label;
  GtkWidget *radio_value_editor;
};

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeActionEditor, glade_action_editor, GLADE_TYPE_EDITOR_SKELETON,
                         G_ADD_PRIVATE (GladeActionEditor)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_action_editor_editable_init));

static void
glade_action_editor_class_init (GladeActionEditorClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = glade_action_editor_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-action-editor.ui");
  gtk_widget_class_bind_template_child_internal_private (widget_class, GladeActionEditor, extension_port);
  gtk_widget_class_bind_template_child_private (widget_class, GladeActionEditor, embed);
  gtk_widget_class_bind_template_child_private (widget_class, GladeActionEditor, toggle_title);
  gtk_widget_class_bind_template_child_private (widget_class, GladeActionEditor, radio_proxy_editor);
  gtk_widget_class_bind_template_child_private (widget_class, GladeActionEditor, toggle_active_editor);
  gtk_widget_class_bind_template_child_private (widget_class, GladeActionEditor, radio_group_label);
  gtk_widget_class_bind_template_child_private (widget_class, GladeActionEditor, radio_group_editor);
  gtk_widget_class_bind_template_child_private (widget_class, GladeActionEditor, radio_value_label);
  gtk_widget_class_bind_template_child_private (widget_class, GladeActionEditor, radio_value_editor);
}

static void
glade_action_editor_init (GladeActionEditor * self)
{
  self->priv = glade_action_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_action_editor_grab_focus (GtkWidget * widget)
{
  GladeActionEditor *action_editor =
      GLADE_ACTION_EDITOR (widget);

  gtk_widget_grab_focus (action_editor->priv->embed);
}

static void
glade_action_editor_load (GladeEditable *editable,
                          GladeWidget   *gwidget)
{
  GladeActionEditor *action_editor = GLADE_ACTION_EDITOR (editable);
  GladeActionEditorPrivate *priv = action_editor->priv;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, gwidget);

  if (gwidget)
    {
      GObject *object = glade_widget_get_object (gwidget);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gboolean is_toggle = GTK_IS_TOGGLE_ACTION (object);
      gboolean is_radio = GTK_IS_RADIO_ACTION (object);
G_GNUC_END_IGNORE_DEPRECATIONS

      /* Update subclass specific editor visibility */
      gtk_widget_set_visible (priv->toggle_title, is_toggle);
      gtk_widget_set_visible (priv->radio_proxy_editor, is_toggle);
      gtk_widget_set_visible (priv->toggle_active_editor, is_toggle);
      gtk_widget_set_visible (priv->radio_group_label, is_radio);
      gtk_widget_set_visible (priv->radio_group_editor, is_radio);
      gtk_widget_set_visible (priv->radio_value_label, is_radio);
      gtk_widget_set_visible (priv->radio_value_editor, is_radio);
    }
}

static void
glade_action_editor_editable_init (GladeEditableInterface * iface)
{
  parent_editable_iface = g_type_interface_peek_parent (iface);

  iface->load = glade_action_editor_load;
}

/*************************************
 *                API                *
 *************************************/
GtkWidget *
glade_action_editor_new (void)
{
  return g_object_new (GLADE_TYPE_ACTION_EDITOR, NULL);
}

/*************************************
 *     Private Plugin Extensions     *
 *************************************/
void
glade_action_editor_post_create (GladeWidgetAdaptor *adaptor,
                                 GObject            *editor,
                                 GladeCreateReason   reason)
{
  GladeActionEditorPrivate *priv = GLADE_ACTION_EDITOR (editor)->priv;

  gtk_widget_show (priv->extension_port);
}
