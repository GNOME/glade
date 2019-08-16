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

#include "glade-button-editor.h"

static void glade_button_editor_editable_init (GladeEditableInterface * iface);

static void glade_button_editor_grab_focus (GtkWidget * widget);

static void custom_toggled (GtkWidget * widget, GladeButtonEditor * button_editor);
static void stock_toggled (GtkWidget * widget, GladeButtonEditor * button_editor);
static void label_toggled (GtkWidget * widget, GladeButtonEditor * button_editor);

struct _GladeButtonEditorPrivate
{
  GtkWidget *embed;
  GtkWidget *extension_port;

  /* Radio Button */
  GtkWidget *group_label;
  GtkWidget *group_shell;

  /* Toggle Button */
  GtkWidget *active_shell;
  GtkWidget *inconsistent_shell;
  GtkWidget *draw_indicator_shell;

  /* Button */
  GtkWidget *content_label;

  GtkWidget *relief_label;
  GtkWidget *relief_shell;

  GtkWidget *response_label;
  GtkWidget *response_shell;

  GtkWidget *custom_check;   /* Use a placeholder in the button */

  /* Available in standard mode: */
  GtkWidget *stock_radio;    /* Create the button using the stock (Available: stock, image-position) */
  GtkWidget *label_radio;    /* Create the button with a custom label
                              * (Available: label, use-underline, image, image-position */

  GtkWidget *standard_frame; /* Contains all the button configurations
                              */
  GtkWidget *stock_frame;    /* Contains stock and image-position properties
                              */
  GtkWidget *label_frame;    /* Contains label, use-underline, image and image-position properties 
                              */
};

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeButtonEditor, glade_button_editor, GLADE_TYPE_EDITOR_SKELETON,
                         G_ADD_PRIVATE (GladeButtonEditor)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_button_editor_editable_init));

static void
glade_button_editor_class_init (GladeButtonEditorClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = glade_button_editor_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-button-editor.ui");

  gtk_widget_class_bind_template_child_internal_private (widget_class, GladeButtonEditor, extension_port);
  gtk_widget_class_bind_template_child_private (widget_class, GladeButtonEditor, embed);
  gtk_widget_class_bind_template_child_private (widget_class, GladeButtonEditor, relief_label);
  gtk_widget_class_bind_template_child_private (widget_class, GladeButtonEditor, relief_shell);
  gtk_widget_class_bind_template_child_private (widget_class, GladeButtonEditor, response_label);
  gtk_widget_class_bind_template_child_private (widget_class, GladeButtonEditor, response_shell);
  gtk_widget_class_bind_template_child_private (widget_class, GladeButtonEditor, content_label);
  gtk_widget_class_bind_template_child_private (widget_class, GladeButtonEditor, group_shell);
  gtk_widget_class_bind_template_child_private (widget_class, GladeButtonEditor, group_label);
  gtk_widget_class_bind_template_child_private (widget_class, GladeButtonEditor, active_shell);
  gtk_widget_class_bind_template_child_private (widget_class, GladeButtonEditor, inconsistent_shell);
  gtk_widget_class_bind_template_child_private (widget_class, GladeButtonEditor, draw_indicator_shell);
  gtk_widget_class_bind_template_child_private (widget_class, GladeButtonEditor, custom_check);
  gtk_widget_class_bind_template_child_private (widget_class, GladeButtonEditor, stock_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeButtonEditor, label_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeButtonEditor, standard_frame);
  gtk_widget_class_bind_template_child_private (widget_class, GladeButtonEditor, stock_frame);
  gtk_widget_class_bind_template_child_private (widget_class, GladeButtonEditor, label_frame);

  gtk_widget_class_bind_template_callback (widget_class, custom_toggled);
  gtk_widget_class_bind_template_callback (widget_class, stock_toggled);
  gtk_widget_class_bind_template_callback (widget_class, label_toggled);
}

static void
glade_button_editor_init (GladeButtonEditor * self)
{
  self->priv = glade_button_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_button_editor_load (GladeEditable * editable, GladeWidget * widget)
{
  GladeButtonEditor *button_editor = GLADE_BUTTON_EDITOR (editable);
  GladeButtonEditorPrivate *priv = button_editor->priv;
  GladeWidget *gchild = NULL;
  GtkWidget *child, *button;
  gboolean use_stock = FALSE;

  if (widget)
    button = GTK_WIDGET (glade_widget_get_object (widget));

  /* If the widget changed, adjust some visibility
   */
  if (widget && glade_editable_loaded_widget (editable) != widget)
    {
      gboolean is_toggle = GTK_IS_TOGGLE_BUTTON (button);
      gboolean is_check  = GTK_IS_CHECK_BUTTON (button);
      gboolean is_radio  = GTK_IS_RADIO_BUTTON (button);
      gboolean modify_content = TRUE;

      if (GTK_IS_MENU_BUTTON (button) ||
          GTK_IS_SCALE_BUTTON (button))
        modify_content = FALSE;

      gtk_widget_set_visible (priv->active_shell, is_toggle);
      gtk_widget_set_visible (priv->inconsistent_shell, is_toggle);
      gtk_widget_set_visible (priv->draw_indicator_shell, is_toggle);

      gtk_widget_set_visible (priv->relief_label, !is_check);
      gtk_widget_set_visible (priv->relief_shell, !is_check);

      gtk_widget_set_visible (priv->response_label, !is_toggle);
      gtk_widget_set_visible (priv->response_shell, !is_toggle);

      gtk_widget_set_visible (priv->group_label, is_radio);
      gtk_widget_set_visible (priv->group_shell, is_radio);

      gtk_widget_set_visible (priv->content_label,  modify_content);
      gtk_widget_set_visible (priv->custom_check,   modify_content);
      gtk_widget_set_visible (priv->standard_frame, modify_content);
    }

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  if (widget)
    {
      child = gtk_bin_get_child (GTK_BIN (button));
      if (child)
        gchild = glade_widget_get_from_gobject (child);

      /* Setup radio and sensitivity states */
      if ((gchild && glade_widget_get_parent (gchild)) || // a widget is manually inside
          GLADE_IS_PLACEHOLDER (child)) // placeholder there, custom mode
        {
          /* Custom */
          gtk_widget_set_sensitive (priv->standard_frame, FALSE);
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->custom_check), TRUE);
        }
      else
        {
          /* Standard */
          gtk_widget_set_sensitive (priv->standard_frame, TRUE);
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->custom_check), FALSE);

          glade_widget_property_get (widget, "use-stock", &use_stock);

          if (use_stock)
            {
              gtk_widget_set_sensitive (priv->stock_frame, TRUE);
              gtk_widget_set_sensitive (priv->label_frame, FALSE);
              gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->stock_radio), TRUE);
            }
          else
            {
              gtk_widget_set_sensitive (priv->stock_frame, FALSE);
              gtk_widget_set_sensitive (priv->label_frame, TRUE);
              gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->label_radio), TRUE);
            }
        }
    }
}

static void
glade_button_editor_editable_init (GladeEditableInterface * iface)
{
  parent_editable_iface = g_type_interface_peek_parent (iface);

  iface->load = glade_button_editor_load;
}

static void
glade_button_editor_grab_focus (GtkWidget * widget)
{
  GladeButtonEditor *button_editor = GLADE_BUTTON_EDITOR (widget);

  gtk_widget_grab_focus (button_editor->priv->embed);
}

/* Section control radio/check button callbacks: */
static void
custom_toggled (GtkWidget * widget, GladeButtonEditor * button_editor)
{
  GladeProperty *property;
  GladeWidget *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (button_editor));
  gboolean active;

  if (glade_editable_loading (GLADE_EDITABLE (button_editor)) || !gwidget)
    return;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_editor->priv->custom_check));

  glade_editable_block (GLADE_EDITABLE (button_editor));

  if (active)
    {
      glade_command_push_group (_("Setting %s to use a custom child"),
                                glade_widget_get_name (gwidget));

      /* clear out some things... */
      property = glade_widget_get_property (gwidget, "image");
      glade_command_set_property (property, NULL);

      property = glade_widget_get_property (gwidget, "use-stock");
      glade_command_set_property (property, FALSE);

      property = glade_widget_get_property (gwidget, "stock");
      glade_command_set_property (property, NULL);

      property = glade_widget_get_property (gwidget, "label");
      glade_command_set_property (property, NULL);

      /* Add a placeholder via the custom-child property... */
      property = glade_widget_get_property (gwidget, "custom-child");
      glade_command_set_property (property, TRUE);

      glade_command_pop_group ();
    }
  else
    {
      GtkWidget *button, *child;
      GladeWidget *gchild = NULL;
      GValue value = { 0, };

      glade_command_push_group (_("Setting %s to use standard configuration"),
                                glade_widget_get_name (gwidget));

      /* If theres a widget customly inside... command remove it first... */
      button = GTK_WIDGET (glade_widget_get_object (gwidget));
      child = gtk_bin_get_child (GTK_BIN (button));
      if (child)
        gchild = glade_widget_get_from_gobject (child);

      if (gchild && glade_widget_get_parent (gchild) == gwidget)
        {
          GList widgets = { 0, };
          widgets.data = gchild;
          glade_command_delete (&widgets);
        }

      property = glade_widget_get_property (gwidget, "custom-child");
      glade_command_set_property (property, FALSE);

      /* Setup reasonable defaults for button label. */
      property = glade_widget_get_property (gwidget, "stock");
      glade_command_set_property (property, NULL);

      property = glade_widget_get_property (gwidget, "use-stock");
      glade_command_set_property (property, FALSE);

      property = glade_widget_get_property (gwidget, "label");
      glade_property_get_default (property, &value);
      glade_command_set_property_value (property, &value);
      g_value_unset (&value);

      glade_command_pop_group ();
    }

  glade_editable_unblock (GLADE_EDITABLE (button_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (button_editor), gwidget);
}

static void
stock_toggled (GtkWidget * widget, GladeButtonEditor * button_editor)
{
  GladeProperty *property;
  GladeWidget *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (button_editor));

  if (glade_editable_loading (GLADE_EDITABLE (button_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (button_editor->priv->stock_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (button_editor));

  glade_command_push_group (_("Setting %s to use a stock button"),
                            glade_widget_get_name (gwidget));

  /* clear out stuff... */
  property = glade_widget_get_property (gwidget, "image");
  glade_command_set_property (property, NULL);

  property = glade_widget_get_property (gwidget, "label");
  glade_command_set_property (property, "");

  property = glade_widget_get_property (gwidget, "use-stock");
  glade_command_set_property (property, TRUE);

  property = glade_widget_get_property (gwidget, "stock");
  glade_command_set_property (property, NULL);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (button_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (button_editor), gwidget);
}

static void
label_toggled (GtkWidget * widget, GladeButtonEditor * button_editor)
{
  GladeProperty *property;
  GValue value = { 0, };
  GladeWidget *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (button_editor));

  if (glade_editable_loading (GLADE_EDITABLE (button_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (button_editor->priv->label_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (button_editor));

  glade_command_push_group (_("Setting %s to use a label and image"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "stock");
  glade_command_set_property (property, NULL);

  property = glade_widget_get_property (gwidget, "use-stock");
  glade_command_set_property (property, FALSE);

  property = glade_widget_get_property (gwidget, "label");
  glade_property_get_default (property, &value);
  glade_command_set_property_value (property, &value);
  g_value_unset (&value);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (button_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (button_editor), gwidget);
}

GtkWidget *
glade_button_editor_new (void)
{
  return g_object_new (GLADE_TYPE_BUTTON_EDITOR, NULL);
}

/*************************************
 *     Private Plugin Extensions     *
 *************************************/
void
glade_button_editor_post_create (GladeWidgetAdaptor *adaptor,
                                 GObject            *editor,
                                 GladeCreateReason   reason)
{
  GladeButtonEditorPrivate *priv = GLADE_BUTTON_EDITOR (editor)->priv;

  gtk_widget_show (priv->extension_port);
}
