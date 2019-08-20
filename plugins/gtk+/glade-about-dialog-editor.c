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

#include "glade-about-dialog-editor.h"

static void glade_about_dialog_editor_editable_init (GladeEditableInterface *iface);

/* Callbacks */
static void license_type_pre_commit     (GladePropertyShell     *shell,
                                         GValue                 *value,
                                         GladeAboutDialogEditor *editor);
static void license_type_post_commit    (GladePropertyShell     *shell,
                                         GValue                 *value,
                                         GladeAboutDialogEditor *editor);
static void logo_file_toggled           (GtkWidget              *widget,
                                         GladeAboutDialogEditor *editor);
static void logo_icon_toggled           (GtkWidget              *widget,
                                         GladeAboutDialogEditor *editor);


struct _GladeAboutDialogEditorPrivate
{
  GtkWidget *license_label;
  GtkWidget *license_editor;
  GtkWidget *wrap_license_editor;

  GtkWidget *logo_file_radio;
  GtkWidget *logo_icon_radio;
};

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeAboutDialogEditor, glade_about_dialog_editor, GLADE_TYPE_WINDOW_EDITOR,
                         G_ADD_PRIVATE (GladeAboutDialogEditor)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_about_dialog_editor_editable_init));

static void
glade_about_dialog_editor_class_init (GladeAboutDialogEditorClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-about-dialog-editor.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GladeAboutDialogEditor, license_label);
  gtk_widget_class_bind_template_child_private (widget_class, GladeAboutDialogEditor, license_editor);
  gtk_widget_class_bind_template_child_private (widget_class, GladeAboutDialogEditor, wrap_license_editor);
  gtk_widget_class_bind_template_child_private (widget_class, GladeAboutDialogEditor, logo_file_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeAboutDialogEditor, logo_icon_radio);

  gtk_widget_class_bind_template_callback (widget_class, license_type_pre_commit);
  gtk_widget_class_bind_template_callback (widget_class, license_type_post_commit);
  gtk_widget_class_bind_template_callback (widget_class, logo_file_toggled);
  gtk_widget_class_bind_template_callback (widget_class, logo_icon_toggled);
}

static void
glade_about_dialog_editor_init (GladeAboutDialogEditor *self)
{
  self->priv = glade_about_dialog_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_about_dialog_editor_load (GladeEditable *editable, GladeWidget *widget)
{
  GladeAboutDialogEditor *dialog_editor = GLADE_ABOUT_DIALOG_EDITOR (editable);
  GladeAboutDialogEditorPrivate *priv = dialog_editor->priv;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  if (widget)
    {
      GtkLicense license = GTK_LICENSE_UNKNOWN;
      gboolean   as_file;
      gboolean   sensitive;

      /* Set sensitivity of the custom license text */
      glade_widget_property_get (widget, "license-type", &license);

      sensitive = (license == GTK_LICENSE_UNKNOWN || license == GTK_LICENSE_CUSTOM);
      gtk_widget_set_sensitive (priv->license_label, sensitive);
      gtk_widget_set_sensitive (priv->license_editor, sensitive);
      gtk_widget_set_sensitive (priv->wrap_license_editor, sensitive);

      /* Set the radio button state to our virtual property */
      glade_widget_property_get (widget, "glade-logo-as-file", &as_file);
      
      glade_widget_property_set_enabled (widget, "logo-icon-name", !as_file);
      glade_widget_property_set_enabled (widget, "logo", as_file);
      
      if (as_file)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->logo_file_radio), TRUE);
      else
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->logo_icon_radio), TRUE);
    }
}

static void
glade_about_dialog_editor_editable_init (GladeEditableInterface *iface)
{
  parent_editable_iface = g_type_interface_peek_parent (iface);

  iface->load = glade_about_dialog_editor_load;
}


static void
license_type_pre_commit (GladePropertyShell     *shell,
                         GValue                 *value,
                         GladeAboutDialogEditor *editor)
{
  GladeWidget   *widget = glade_editable_loaded_widget (GLADE_EDITABLE (editor));
  GladeProperty *property;
  GtkLicense     license;

  glade_command_push_group (_("Setting License type of %s"),
                            glade_widget_get_name (widget));

  license = g_value_get_enum (value);

  if (!(license == GTK_LICENSE_UNKNOWN || license == GTK_LICENSE_CUSTOM))
    {
      property = glade_widget_get_property (widget, "license");
      glade_command_set_property (property, NULL);

      property = glade_widget_get_property (widget, "wrap-license");
      glade_command_set_property (property, FALSE);
    }
}

static void
license_type_post_commit (GladePropertyShell     *shell,
                          GValue                 *value,
                          GladeAboutDialogEditor *editor)
{
  glade_command_pop_group ();
}

static void
glade_about_dialog_editor_set_logo_as_file (GladeAboutDialogEditor *editor,
                                            gboolean               logo_as_file)
{
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (editor));
  GladeProperty *property;

  if (glade_editable_loading (GLADE_EDITABLE (editor)) || !gwidget)
    return;

  glade_editable_block (GLADE_EDITABLE (editor));

  glade_command_push_group (logo_as_file ? _("Setting %s to use logo file") :
                              _("Setting %s to use a logo icon"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "glade-logo-as-file");
  glade_command_set_property (property, logo_as_file);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (editor), gwidget);
}

static void
logo_icon_toggled (GtkWidget *widget, GladeAboutDialogEditor *editor)
{
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    return;

  glade_about_dialog_editor_set_logo_as_file (editor, FALSE);
}

static void
logo_file_toggled (GtkWidget *widget, GladeAboutDialogEditor *editor)
{
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    return;

  glade_about_dialog_editor_set_logo_as_file (editor, TRUE);
}

GtkWidget *
glade_about_dialog_editor_new (void)
{
  return g_object_new (GLADE_TYPE_ABOUT_DIALOG_EDITOR, NULL);
}
