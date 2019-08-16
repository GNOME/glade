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

#include "glade-tool-button-editor.h"


static void glade_tool_button_editor_editable_init (GladeEditableInterface * iface);
static void glade_tool_button_editor_grab_focus    (GtkWidget * widget);

/* Callbacks */
static void standard_label_toggled (GtkWidget             *widget,
                                    GladeToolButtonEditor *button_editor);
static void custom_label_toggled   (GtkWidget             *widget,
                                    GladeToolButtonEditor *button_editor);
static void stock_toggled          (GtkWidget             *widget,
                                    GladeToolButtonEditor *button_editor);
static void icon_toggled           (GtkWidget             *widget,
                                    GladeToolButtonEditor *button_editor);
static void custom_toggled         (GtkWidget             *widget,
                                    GladeToolButtonEditor *button_editor);

struct _GladeToolButtonEditorPrivate
{
  GtkWidget *embed;           /* Embedded activatable editor */

  GtkWidget *standard_label_radio; /* Set label with label property */
  GtkWidget *custom_label_radio;   /* Set a widget to be placed as the tool button's label */

  GtkWidget *stock_radio;    /* Create the image from stock-id */
  GtkWidget *icon_radio;     /* Create the image with the icon theme */
  GtkWidget *custom_radio;   /* Set a widget to be used in the image position */

  /* Subclass specific stuff */
  GtkWidget *toggle_active_editor;
  GtkWidget *radio_group_label;
  GtkWidget *radio_group_editor;
};

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeToolButtonEditor, glade_tool_button_editor, GLADE_TYPE_EDITOR_SKELETON,
                         G_ADD_PRIVATE (GladeToolButtonEditor)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_tool_button_editor_editable_init));

static void
glade_tool_button_editor_class_init (GladeToolButtonEditorClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = glade_tool_button_editor_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-tool-button-editor.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GladeToolButtonEditor, embed);
  gtk_widget_class_bind_template_child_private (widget_class, GladeToolButtonEditor, standard_label_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeToolButtonEditor, custom_label_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeToolButtonEditor, stock_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeToolButtonEditor, icon_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeToolButtonEditor, custom_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeToolButtonEditor, toggle_active_editor);
  gtk_widget_class_bind_template_child_private (widget_class, GladeToolButtonEditor, radio_group_label);
  gtk_widget_class_bind_template_child_private (widget_class, GladeToolButtonEditor, radio_group_editor);

  gtk_widget_class_bind_template_callback (widget_class, standard_label_toggled);
  gtk_widget_class_bind_template_callback (widget_class, custom_label_toggled);
  gtk_widget_class_bind_template_callback (widget_class, stock_toggled);
  gtk_widget_class_bind_template_callback (widget_class, icon_toggled);
  gtk_widget_class_bind_template_callback (widget_class, custom_toggled);
}

static void
glade_tool_button_editor_init (GladeToolButtonEditor *self)
{
  self->priv = glade_tool_button_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_tool_button_editor_load (GladeEditable *editable, GladeWidget *widget)
{
  GladeToolButtonEditor *button_editor = GLADE_TOOL_BUTTON_EDITOR (editable);
  GladeToolButtonEditorPrivate *priv = button_editor->priv;
  GladeToolButtonImageMode image_mode = 0;
  gboolean custom_label = FALSE;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  if (widget)
    {
      GObject *object = glade_widget_get_object (widget);

      glade_widget_property_get (widget, "image-mode", &image_mode);
      glade_widget_property_get (widget, "custom-label", &custom_label);

      if (custom_label)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->custom_label_radio), TRUE);
      else
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->standard_label_radio), TRUE);

      switch (image_mode)
        {
          case GLADE_TB_MODE_STOCK:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->stock_radio), TRUE);
            break;
          case GLADE_TB_MODE_ICON:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->icon_radio), TRUE);
            break;
          case GLADE_TB_MODE_CUSTOM:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->custom_radio), TRUE);
            break;
          default:
            break;
        }

      gtk_widget_set_visible (priv->toggle_active_editor, GTK_IS_TOGGLE_TOOL_BUTTON (object));
      gtk_widget_set_visible (priv->radio_group_label, GTK_IS_RADIO_TOOL_BUTTON (object));
      gtk_widget_set_visible (priv->radio_group_editor, GTK_IS_RADIO_TOOL_BUTTON (object));
    }
}


static void
standard_label_toggled (GtkWidget *widget, GladeToolButtonEditor *button_editor)
{
  GladeToolButtonEditorPrivate *priv = button_editor->priv;
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (button_editor));
  GValue value = { 0, };

  if (glade_editable_loading (GLADE_EDITABLE (button_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->standard_label_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (button_editor));

  glade_command_push_group (_("Setting %s to use standard label text"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "label-widget");
  glade_command_set_property (property, NULL);

  property = glade_widget_get_property (gwidget, "label");
  glade_property_get_default (property, &value);
  glade_command_set_property_value (property, &value);
  g_value_unset (&value);
  property = glade_widget_get_property (gwidget, "custom-label");
  glade_command_set_property (property, FALSE);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (button_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (button_editor), gwidget);
}

static void
custom_label_toggled (GtkWidget *widget, GladeToolButtonEditor *button_editor)
{
  GladeToolButtonEditorPrivate *priv = button_editor->priv;
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (button_editor));

  if (glade_editable_loading (GLADE_EDITABLE (button_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->custom_label_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (button_editor));

  glade_command_push_group (_("Setting %s to use a custom label widget"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "label");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "custom-label");
  glade_command_set_property (property, TRUE);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (button_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (button_editor), gwidget);
}

static void
stock_toggled (GtkWidget *widget, GladeToolButtonEditor *button_editor)
{
  GladeToolButtonEditorPrivate *priv = button_editor->priv;
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (button_editor));

  if (glade_editable_loading (GLADE_EDITABLE (button_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->stock_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (button_editor));

  glade_command_push_group (_("Setting %s to use an image from stock"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "icon-name");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "icon");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "icon-widget");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "image-mode");
  glade_command_set_property (property, GLADE_TB_MODE_STOCK);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (button_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (button_editor), gwidget);
}

static void
icon_toggled (GtkWidget *widget, GladeToolButtonEditor *button_editor)
{
  GladeToolButtonEditorPrivate *priv = button_editor->priv;
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (button_editor));

  if (glade_editable_loading (GLADE_EDITABLE (button_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->icon_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (button_editor));

  glade_command_push_group (_("Setting %s to use an image from the icon theme"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "stock-id");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "icon");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "icon-widget");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "image-mode");
  glade_command_set_property (property, GLADE_TB_MODE_ICON);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (button_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (button_editor), gwidget);
}

static void
custom_toggled (GtkWidget *widget, GladeToolButtonEditor *button_editor)
{
  GladeToolButtonEditorPrivate *priv = button_editor->priv;
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (button_editor));

  if (glade_editable_loading (GLADE_EDITABLE (button_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->custom_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (button_editor));

  glade_command_push_group (_("Setting %s to use an image from the icon theme"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "stock-id");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "icon-name");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "icon");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "image-mode");
  glade_command_set_property (property, GLADE_TB_MODE_CUSTOM);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (button_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (button_editor), gwidget);
}

static void
glade_tool_button_editor_editable_init (GladeEditableInterface *iface)
{
  parent_editable_iface = g_type_interface_peek_parent (iface);

  iface->load = glade_tool_button_editor_load;
}

static void
glade_tool_button_editor_grab_focus (GtkWidget *widget)
{
  GladeToolButtonEditor *button_editor = GLADE_TOOL_BUTTON_EDITOR (widget);

  gtk_widget_grab_focus (button_editor->priv->embed);
}

GtkWidget *
glade_tool_button_editor_new (void)
{
  return g_object_new (GLADE_TYPE_TOOL_BUTTON_EDITOR, NULL);
}
