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

#include "glade-recent-chooser-editor.h"

static void glade_recent_chooser_editor_editable_init (GladeEditableInterface *iface);

struct _GladeRecentChooserEditorPrivate {
  GtkWidget *select_multiple_editor;
  GtkWidget *show_numbers_editor;
};

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeRecentChooserEditor, glade_recent_chooser_editor, GLADE_TYPE_EDITOR_SKELETON,
                         G_ADD_PRIVATE (GladeRecentChooserEditor)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_recent_chooser_editor_editable_init));

static void
glade_recent_chooser_editor_class_init (GladeRecentChooserEditorClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-recent-chooser-editor.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GladeRecentChooserEditor, select_multiple_editor);
  gtk_widget_class_bind_template_child_private (widget_class, GladeRecentChooserEditor, show_numbers_editor);
}

static void
glade_recent_chooser_editor_init (GladeRecentChooserEditor *self)
{
  self->priv = glade_recent_chooser_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_recent_chooser_editor_load (GladeEditable *editable,
                                  GladeWidget   *gwidget)
{
  GladeRecentChooserEditor *recent_editor = GLADE_RECENT_CHOOSER_EDITOR (editable);
  GladeRecentChooserEditorPrivate *priv = recent_editor->priv;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, gwidget);

  if (gwidget)
    {
      GObject *object = glade_widget_get_object (gwidget);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gboolean has_show_numbers = (GTK_IS_RECENT_ACTION (object) || GTK_IS_RECENT_CHOOSER_MENU (object));
G_GNUC_END_IGNORE_DEPRECATIONS

      /* Update subclass specific editor visibility */
      gtk_widget_set_visible (priv->select_multiple_editor, !has_show_numbers);
      gtk_widget_set_visible (priv->show_numbers_editor, has_show_numbers);
    }
}

static void
glade_recent_chooser_editor_editable_init (GladeEditableInterface *iface)
{
  parent_editable_iface = g_type_interface_peek_parent (iface);

  iface->load = glade_recent_chooser_editor_load;
}


GtkWidget *
glade_recent_chooser_editor_new (void)
{
  return g_object_new (GLADE_TYPE_RECENT_CHOOSER_EDITOR, NULL);
}
