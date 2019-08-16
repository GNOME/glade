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
#include <gdk/gdkkeysyms.h>

#include "glade-box-editor.h"

static void glade_box_editor_editable_init (GladeEditableInterface * iface);
static void glade_box_editor_grab_focus (GtkWidget * widget);
static void use_center_child_toggled (GtkWidget *widget, GladeBoxEditor * box_editor);


struct _GladeBoxEditorPrivate
{
  GtkWidget *embed;
  GtkWidget *use_center_child;
};

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeBoxEditor, glade_box_editor, GLADE_TYPE_EDITOR_SKELETON,
                         G_ADD_PRIVATE (GladeBoxEditor)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_box_editor_editable_init));

static void
glade_box_editor_class_init (GladeBoxEditorClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = glade_box_editor_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-box-editor.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GladeBoxEditor, embed);
  gtk_widget_class_bind_template_child_private (widget_class, GladeBoxEditor, use_center_child);

  gtk_widget_class_bind_template_callback (widget_class, use_center_child_toggled);
}

static void
glade_box_editor_init (GladeBoxEditor * self)
{
  self->priv = glade_box_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_box_editor_grab_focus (GtkWidget * widget)
{
  GladeBoxEditor *box_editor = GLADE_BOX_EDITOR (widget);

  gtk_widget_grab_focus (box_editor->priv->embed);
}

static void
glade_box_editor_load (GladeEditable *editable,
                       GladeWidget   *gwidget)
{
  GladeBoxEditor *box_editor = GLADE_BOX_EDITOR (editable);
  GladeBoxEditorPrivate *priv = box_editor->priv;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, gwidget);

  if (gwidget)
    {
      gboolean use_center_child;

      glade_widget_property_get (gwidget, "use-center-child", &use_center_child);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->use_center_child), use_center_child);
    }
}

static void
glade_box_editor_editable_init (GladeEditableInterface * iface)
{
  parent_editable_iface = g_type_interface_peek_parent (iface);

  iface->load = glade_box_editor_load;
}

static void
use_center_child_toggled (GtkWidget      * widget,
                          GladeBoxEditor * box_editor)
{
  GladeBoxEditorPrivate *priv = box_editor->priv;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (box_editor));
  GladeWidget   *gcenter = NULL;
  GtkWidget     *box;
  GtkWidget     *center;
  GladeProperty *property;
  gboolean       use_center_child;

  if (glade_editable_loading (GLADE_EDITABLE (box_editor)) || !gwidget)
    return;

  /* Get new desired property state */
  use_center_child = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->use_center_child));

  /* Get any existing center child */
  box = (GtkWidget *)glade_widget_get_object (gwidget);
  center = gtk_box_get_center_widget (GTK_BOX (box));

  if (center && !GLADE_IS_PLACEHOLDER (center))
    gcenter = glade_widget_get_from_gobject (center);

  glade_editable_block (GLADE_EDITABLE (box_editor));

  if (use_center_child)
    glade_command_push_group (_("Setting %s to use a center child"),
                              glade_widget_get_name (gwidget));
  else
    glade_command_push_group (_("Setting %s to not use a center child"),
                              glade_widget_get_name (gwidget));

  /* If a project widget exists when were disabling center child, it needs
   * to be removed first as a part of the issuing GladeCommand group
   */
  if (gcenter)
    {
      GList list;
      list.prev = list.next = NULL;
      list.data = gcenter;
      glade_command_delete (&list);
    }

  property = glade_widget_get_property (gwidget, "use-center-child");
  glade_command_set_property (property, use_center_child);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (box_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (box_editor), gwidget);
}

GtkWidget *
glade_box_editor_new (void)
{
  return g_object_new (GLADE_TYPE_BOX_EDITOR, NULL);
}
