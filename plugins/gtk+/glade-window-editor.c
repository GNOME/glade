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

#include "glade-window-editor.h"

static void glade_window_editor_editable_init (GladeEditableInterface *iface);
static void glade_window_editor_grab_focus (GtkWidget *widget);

/* Callbacks */
static void icon_name_toggled    (GtkWidget *widget, GladeWindowEditor *window_editor);
static void icon_file_toggled    (GtkWidget *widget, GladeWindowEditor *window_editor);
static void use_csd_toggled      (GtkWidget *widget, GladeWindowEditor *window_editor);

struct _GladeWindowEditorPrivate {
  GtkWidget *embed;

  GtkWidget *extension_port;
  GtkWidget *icon_name_radio;
  GtkWidget *icon_file_radio;
  GtkWidget *use_csd_check;
  GtkWidget *title_editor;
  GtkWidget *decorated_editor;
  GtkWidget *hide_titlebar_editor;
};

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeWindowEditor, glade_window_editor, GLADE_TYPE_EDITOR_SKELETON,
                         G_ADD_PRIVATE (GladeWindowEditor)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_window_editor_editable_init));

static void
glade_window_editor_class_init (GladeWindowEditorClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = glade_window_editor_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-window-editor.ui");
  gtk_widget_class_bind_template_child_internal_private (widget_class, GladeWindowEditor, extension_port);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindowEditor, embed);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindowEditor, icon_name_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindowEditor, icon_file_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindowEditor, use_csd_check);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindowEditor, title_editor);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindowEditor, decorated_editor);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindowEditor, hide_titlebar_editor);

  gtk_widget_class_bind_template_callback (widget_class, icon_name_toggled);
  gtk_widget_class_bind_template_callback (widget_class, icon_file_toggled);
  gtk_widget_class_bind_template_callback (widget_class, use_csd_toggled);
}

static void
glade_window_editor_init (GladeWindowEditor *self)
{
  self->priv = glade_window_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_window_editor_grab_focus (GtkWidget *widget)
{
  GladeWindowEditor *window_editor =
      GLADE_WINDOW_EDITOR (widget);

  gtk_widget_grab_focus (window_editor->priv->embed);
}

static void
glade_window_editor_load (GladeEditable *editable,
                          GladeWidget   *gwidget)
{
  GladeWindowEditor *window_editor = GLADE_WINDOW_EDITOR (editable);
  GladeWindowEditorPrivate *priv = window_editor->priv;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, gwidget);

  if (gwidget)
    {
      gboolean icon_name;
      gboolean use_csd;

      glade_widget_property_get (gwidget, "glade-window-icon-name", &icon_name);
      glade_widget_property_get (gwidget, "use-csd", &use_csd);

      if (icon_name)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->icon_name_radio), TRUE);
      else
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->icon_file_radio), TRUE);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->use_csd_check), use_csd);
    }
}

static void
glade_window_editor_editable_init (GladeEditableInterface *iface)
{
  parent_editable_iface = g_type_interface_peek_parent (iface);

  iface->load = glade_window_editor_load;
}

static void
icon_name_toggled (GtkWidget         *widget,
                   GladeWindowEditor *window_editor)
{
  GladeWindowEditorPrivate *priv = window_editor->priv;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (window_editor));
  GladeProperty *property;

  if (glade_editable_loading (GLADE_EDITABLE (window_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->icon_name_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (window_editor));

  glade_command_push_group (_("Setting %s to use a named icon"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "icon-name");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "icon");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "glade-window-icon-name");
  glade_command_set_property (property, TRUE);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (window_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (window_editor), gwidget);
}

static void
icon_file_toggled (GtkWidget         *widget,
                   GladeWindowEditor *window_editor)
{
  GladeWindowEditorPrivate *priv = window_editor->priv;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (window_editor));
  GladeProperty *property;

  if (glade_editable_loading (GLADE_EDITABLE (window_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->icon_file_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (window_editor));

  glade_command_push_group (_("Setting %s to use an icon file"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "icon-name");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "icon");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "glade-window-icon-name");
  glade_command_set_property (property, FALSE);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (window_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (window_editor), gwidget);
}

static void
use_csd_toggled (GtkWidget         *widget,
                 GladeWindowEditor *window_editor)
{
  GladeWindowEditorPrivate *priv = window_editor->priv;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (window_editor));
  GladeWidget   *gtitlebar = NULL;
  GtkWidget     *window;
  GtkWidget     *titlebar;
  GladeProperty *property;
  gboolean       use_csd;

  if (glade_editable_loading (GLADE_EDITABLE (window_editor)) || !gwidget)
    return;

  /* Get new desired property state */
  use_csd = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->use_csd_check));

  /* Get any existing titlebar widget */
  window = (GtkWidget *)glade_widget_get_object (gwidget);
  titlebar = gtk_window_get_titlebar (GTK_WINDOW (window));

  if (titlebar && !GLADE_IS_PLACEHOLDER (titlebar))
    gtitlebar = glade_widget_get_from_gobject (titlebar);

  glade_editable_block (GLADE_EDITABLE (window_editor));

  if (use_csd)
    glade_command_push_group (_("Setting %s to use a custom titlebar"),
                              glade_widget_get_name (gwidget));
  else
    glade_command_push_group (_("Setting %s to use a system provided titlebar"),
                              glade_widget_get_name (gwidget));

  /* If a project widget exists when were disabling CSD, it needs
   * to be removed first as a part of the issuing GladeCommand group
   */
  if (gtitlebar)
    {
      GList list;
      list.prev = list.next = NULL;
      list.data = gtitlebar;
      glade_command_delete (&list);
    }

  property = glade_widget_get_property (gwidget, "use-csd");
  glade_command_set_property (property, use_csd);

  if (use_csd)
    {
      property = glade_widget_get_property (gwidget, "title");
      glade_command_set_property (property, NULL);
      property = glade_widget_get_property (gwidget, "decorated");
      glade_command_set_property (property, TRUE);
      property = glade_widget_get_property (gwidget, "hide-titlebar-when-maximized");
      glade_command_set_property (property, FALSE);
    }

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (window_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (window_editor), gwidget);
}

/*************************************
 *                API                *
 *************************************/
GtkWidget *
glade_window_editor_new (void)
{
  return g_object_new (GLADE_TYPE_WINDOW_EDITOR, NULL);
}

/*************************************
 *     Private Plugin Extensions     *
 *************************************/
void
glade_window_editor_post_create (GladeWidgetAdaptor *adaptor,
                                 GObject            *editor,
                                 GladeCreateReason   reason)
{
  GladeWindowEditorPrivate *priv = GLADE_WINDOW_EDITOR (editor)->priv;

  gtk_widget_show (priv->extension_port);
}
