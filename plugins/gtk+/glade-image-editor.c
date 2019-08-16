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
#include <gdk/gdkkeysyms.h>

#include "glade-image-editor.h"

static void glade_image_editor_editable_init (GladeEditableInterface * iface);
static void glade_image_editor_grab_focus (GtkWidget * widget);

static void stock_toggled    (GtkWidget *widget, GladeImageEditor * image_editor);
static void icon_toggled     (GtkWidget *widget, GladeImageEditor * image_editor);
static void resource_toggled (GtkWidget *widget, GladeImageEditor * image_editor);
static void file_toggled     (GtkWidget *widget, GladeImageEditor * image_editor);

struct _GladeImageEditorPrivate
{
  GtkWidget *embed;

  GtkWidget *stock_radio;    /* Create the image from stock-id */
  GtkWidget *icon_radio;     /* Create the image with the icon theme */
  GtkWidget *resource_radio; /* Create the image from GResource data */
  GtkWidget *file_radio;     /* Create the image from filename (libglade only) */
};

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeImageEditor, glade_image_editor, GLADE_TYPE_EDITOR_SKELETON,
                         G_ADD_PRIVATE (GladeImageEditor)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_image_editor_editable_init));

static void
glade_image_editor_class_init (GladeImageEditorClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = glade_image_editor_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-image-editor.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GladeImageEditor, embed);
  gtk_widget_class_bind_template_child_private (widget_class, GladeImageEditor, stock_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeImageEditor, icon_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeImageEditor, resource_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeImageEditor, file_radio);

  gtk_widget_class_bind_template_callback (widget_class, stock_toggled);
  gtk_widget_class_bind_template_callback (widget_class, icon_toggled);
  gtk_widget_class_bind_template_callback (widget_class, resource_toggled);
  gtk_widget_class_bind_template_callback (widget_class, file_toggled);
}

static void
glade_image_editor_init (GladeImageEditor * self)
{
  self->priv = glade_image_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_image_editor_load (GladeEditable * editable, GladeWidget * widget)
{
  GladeImageEditor        *image_editor = GLADE_IMAGE_EDITOR (editable);
  GladeImageEditorPrivate *priv = image_editor->priv;
  GladeImageEditMode       image_mode = 0;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  if (widget)
    {
      glade_widget_property_get (widget, "image-mode", &image_mode);

      switch (image_mode)
        {
          case GLADE_IMAGE_MODE_STOCK:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (priv->stock_radio), TRUE);
            break;
          case GLADE_IMAGE_MODE_ICON:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (priv->icon_radio), TRUE);
            break;
          case GLADE_IMAGE_MODE_RESOURCE:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (priv->resource_radio), TRUE);
            break;
          case GLADE_IMAGE_MODE_FILENAME:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (priv->file_radio), TRUE);
            break;
          default:
            break;
        }
    }
}

static void
glade_image_editor_editable_init (GladeEditableInterface * iface)
{
  parent_editable_iface = g_type_interface_peek_parent (iface);

  iface->load = glade_image_editor_load;
}

static void
glade_image_editor_grab_focus (GtkWidget * widget)
{
  GladeImageEditor        *image_editor = GLADE_IMAGE_EDITOR (widget);
  GladeImageEditorPrivate *priv = image_editor->priv;

  gtk_widget_grab_focus (priv->embed);
}

static void
set_stock_mode (GladeImageEditor * image_editor)
{
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (image_editor));
  GValue value = { 0, };

  property = glade_widget_get_property (gwidget, "icon-name");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "pixbuf");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "resource");
  glade_command_set_property (property, NULL);

  property = glade_widget_get_property (gwidget, "stock");
  glade_property_get_default (property, &value);
  glade_command_set_property_value (property, &value);
  g_value_unset (&value);

  property = glade_widget_get_property (gwidget, "image-mode");
  glade_command_set_property (property, GLADE_IMAGE_MODE_STOCK);
}

static void
set_icon_mode (GladeImageEditor * image_editor)
{
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (image_editor));

  property = glade_widget_get_property (gwidget, "stock");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "pixbuf");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "resource");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "image-mode");
  glade_command_set_property (property, GLADE_IMAGE_MODE_ICON);
}

static void
set_resource_mode (GladeImageEditor * image_editor)
{
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (image_editor));

  property = glade_widget_get_property (gwidget, "stock");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "icon-name");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "pixbuf");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "image-mode");
  glade_command_set_property (property, GLADE_IMAGE_MODE_RESOURCE);
}

static void
set_file_mode (GladeImageEditor * image_editor)
{
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (image_editor));

  property = glade_widget_get_property (gwidget, "stock");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "icon-name");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "resource");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "image-mode");
  glade_command_set_property (property, GLADE_IMAGE_MODE_FILENAME);
}

static void
stock_toggled (GtkWidget * widget, GladeImageEditor * image_editor)
{
  GladeImageEditorPrivate *priv = image_editor->priv;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (image_editor));

  if (glade_editable_loading (GLADE_EDITABLE (image_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->stock_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (image_editor));

  glade_command_push_group (_("Setting %s to use an image from stock"),
                            glade_widget_get_name (gwidget));
  set_stock_mode (image_editor);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (image_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (image_editor), gwidget);
}

static void
icon_toggled (GtkWidget * widget, GladeImageEditor * image_editor)
{
  GladeImageEditorPrivate *priv = image_editor->priv;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (image_editor));

  if (glade_editable_loading (GLADE_EDITABLE (image_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (priv->icon_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (image_editor));

  glade_command_push_group (_("Setting %s to use an image from the icon theme"),
                            glade_widget_get_name (gwidget));
  set_icon_mode (image_editor);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (image_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (image_editor), gwidget);
}

static void
resource_toggled (GtkWidget * widget, GladeImageEditor * image_editor)
{
  GladeImageEditorPrivate *priv = image_editor->priv;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (image_editor));

  if (glade_editable_loading (GLADE_EDITABLE (image_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (priv->resource_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (image_editor));

  glade_command_push_group (_("Setting %s to use a resource name"),
                            glade_widget_get_name (gwidget));
  set_resource_mode (image_editor);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (image_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (image_editor), gwidget);
}

static void
file_toggled (GtkWidget * widget, GladeImageEditor * image_editor)
{ 
  GladeImageEditorPrivate *priv = image_editor->priv;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (image_editor));

  if (glade_editable_loading (GLADE_EDITABLE (image_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (priv->file_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (image_editor));

  glade_command_push_group (_("Setting %s to use an image from filename"),
                            glade_widget_get_name (gwidget));
  set_file_mode (image_editor);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (image_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (image_editor), gwidget);
}

GtkWidget *
glade_image_editor_new (void)
{
  return g_object_new (GLADE_TYPE_IMAGE_EDITOR, NULL);
}
