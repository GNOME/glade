/*
 * Copyright (C) 2014 Red Hat, Inc.
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
 *   Matthias Clasen <mclasen@redhat.com>
 */

#include <config.h>
#include <gladeui/glade.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#include "glade-header-bar-editor.h"

static void glade_header_bar_editor_editable_init (GladeEditableInterface * iface);
static void glade_header_bar_editor_grab_focus    (GtkWidget          * widget);

static void use_custom_title_toggled (GtkWidget *widget, GladeHeaderBarEditor * editor);
static void show_decoration_toggled (GtkWidget *widget, GladeHeaderBarEditor * editor);

struct _GladeHeaderBarEditorPrivate
{
  GtkWidget *embed;
  GtkWidget *use_custom_title_check;
  GtkWidget *show_decoration_check;
};

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeHeaderBarEditor, glade_header_bar_editor, GLADE_TYPE_EDITOR_SKELETON,
                         G_ADD_PRIVATE (GladeHeaderBarEditor)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_header_bar_editor_editable_init))

static void
glade_header_bar_editor_class_init (GladeHeaderBarEditorClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = glade_header_bar_editor_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-header-bar-editor.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GladeHeaderBarEditor, embed);
  gtk_widget_class_bind_template_child_private (widget_class, GladeHeaderBarEditor, use_custom_title_check);
  gtk_widget_class_bind_template_child_private (widget_class, GladeHeaderBarEditor, show_decoration_check);
  gtk_widget_class_bind_template_callback (widget_class, use_custom_title_toggled);
  gtk_widget_class_bind_template_callback (widget_class, show_decoration_toggled);
}

static void
glade_header_bar_editor_init (GladeHeaderBarEditor * self)
{
  self->priv = glade_header_bar_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_header_bar_editor_grab_focus (GtkWidget * widget)
{
  GladeHeaderBarEditor *editor = GLADE_HEADER_BAR_EDITOR (widget);

  gtk_widget_grab_focus (editor->priv->embed);
}

static void
glade_header_bar_editor_load (GladeEditable *editable,
                              GladeWidget   *gwidget)
{
  GladeHeaderBarEditor *editor = GLADE_HEADER_BAR_EDITOR (editable);
  GladeHeaderBarEditorPrivate *priv = editor->priv;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, gwidget);

  if (gwidget)
    {
      gboolean setting;

      glade_widget_property_get (gwidget, "use-custom-title", &setting);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->use_custom_title_check), setting);
      glade_widget_property_get (gwidget, "show-close-button", &setting);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->show_decoration_check), setting);
    }
}

static void
glade_header_bar_editor_editable_init (GladeEditableInterface * iface)
{
  parent_editable_iface = g_type_interface_peek_parent (iface);

  iface->load = glade_header_bar_editor_load;
}

static void
use_custom_title_toggled (GtkWidget            *widget,
                          GladeHeaderBarEditor *editor)
{
  GladeHeaderBarEditorPrivate *priv = editor->priv;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (editor));
  GladeWidget   *gtitle = NULL;
  GtkWidget     *headerbar;
  GtkWidget     *title;
  GladeProperty *property;
  gboolean       use_custom_title;

  if (glade_editable_loading (GLADE_EDITABLE (editor)) || !gwidget)
    return;

  /* Get new desired property state */
  use_custom_title = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->use_custom_title_check));

  headerbar = (GtkWidget *)glade_widget_get_object (gwidget);
  title = gtk_header_bar_get_custom_title (GTK_HEADER_BAR (headerbar));

  if (title && !GLADE_IS_PLACEHOLDER (title))
    gtitle = glade_widget_get_from_gobject (title);

  glade_editable_block (GLADE_EDITABLE (editor));

  if (use_custom_title)
    /* Translators: %s is the name of a widget here, the sentence means:
     * Make the widget use a custom title
     */
    glade_command_push_group (_("Setting %s to use a custom title"),
                              glade_widget_get_name (gwidget));
  else
    /* Translators: %s is the name of a widget here, the sentence means:
     * Make the widget use the standard title
     */
    glade_command_push_group (_("Setting %s to use the standard title"),
                              glade_widget_get_name (gwidget));

  if (gtitle)
    {
      GList list;
      list.prev = list.next = NULL;
      list.data = gtitle;
      glade_command_delete (&list);
    }

  if (use_custom_title)
    {
      property = glade_widget_get_property (gwidget, "title");
      glade_command_set_property (property, NULL);

      property = glade_widget_get_property (gwidget, "subtitle");
      glade_command_set_property (property, NULL);

      property = glade_widget_get_property (gwidget, "has-subtitle");
      glade_command_set_property (property, TRUE);
    }

  property = glade_widget_get_property (gwidget, "use-custom-title");
  glade_command_set_property (property, use_custom_title);

  glade_command_pop_group ();
  glade_editable_unblock (GLADE_EDITABLE (editor));
  glade_editable_load (GLADE_EDITABLE (editor), gwidget);
}

static void
show_decoration_toggled (GtkWidget            *widget,
                         GladeHeaderBarEditor *editor)
{
  GladeHeaderBarEditorPrivate *priv = editor->priv;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (editor));
  GladeProperty *property;
  gboolean       show_decoration;

  if (glade_editable_loading (GLADE_EDITABLE (editor)) || !gwidget)
    return;

  show_decoration = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->show_decoration_check));

  glade_editable_block (GLADE_EDITABLE (editor));

  if (show_decoration)
    glade_command_push_group (_("Setting %s to show window controls"),
                              glade_widget_get_name (gwidget));
  else
    glade_command_push_group (_("Setting %s to not show window controls"),
                              glade_widget_get_name (gwidget));

  if (!show_decoration)
    {
      property = glade_widget_get_property (gwidget, "decoration-layout");
      glade_command_set_property (property, NULL);
    }

  property = glade_widget_get_property (gwidget, "show-close-button");
  glade_command_set_property (property, show_decoration);

  glade_command_pop_group ();
  glade_editable_unblock (GLADE_EDITABLE (editor));
  glade_editable_load (GLADE_EDITABLE (editor), gwidget);
}

GtkWidget *
glade_header_bar_editor_new (void)
{
  return g_object_new (GLADE_TYPE_HEADER_BAR_EDITOR, NULL);
}
