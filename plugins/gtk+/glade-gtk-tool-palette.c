/*
 * glade-gtk-tool-palette.c - GladeWidgetAdaptor for GtkToolPalette
 *
 * Copyright (C) 2013 Tristan Van Berkom
 *
 * Authors:
 *      Tristan Van Berkom <tristan.van.berkom@gmail.com>
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
 */

#include <config.h>
#include <glib/gi18n-lib.h>
#include <gladeui/glade.h>

#include "glade-tool-palette-editor.h"
#include "glade-gtk-menu-shell.h"
#include "glade-gtk.h"

G_MODULE_EXPORT GladeEditable *
glade_gtk_tool_palette_create_editable (GladeWidgetAdaptor *adaptor,
                                        GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    {
      return (GladeEditable *)glade_tool_palette_editor_new ();
    }

  return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}

G_MODULE_EXPORT void
glade_gtk_tool_palette_get_child_property (GladeWidgetAdaptor *adaptor,
                                           GObject            *container,
                                           GObject            *child,
                                           const gchar        *property_name,
                                           GValue             *value)
{
  g_return_if_fail (GTK_IS_TOOL_PALETTE (container));
  if (GTK_IS_TOOL_ITEM_GROUP (child) == FALSE)
    return;

  if (strcmp (property_name, "position") == 0)
    {
      g_value_set_int (value,
                       gtk_tool_palette_get_group_position (GTK_TOOL_PALETTE (container),
                                                            GTK_TOOL_ITEM_GROUP (child)));
    }
  else
    {                           /* Chain Up */
      GWA_GET_CLASS
          (GTK_TYPE_CONTAINER)->child_get_property (adaptor,
                                                    container, child,
                                                    property_name, value);
    }
}

G_MODULE_EXPORT void
glade_gtk_tool_palette_set_child_property (GladeWidgetAdaptor *adaptor,
                                           GObject            *container,
                                           GObject            *child,
                                           const gchar        *property_name,
                                           GValue             *value)
{
  g_return_if_fail (GTK_IS_TOOL_PALETTE (container));
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (child));

  g_return_if_fail (property_name != NULL || value != NULL);

  if (strcmp (property_name, "position") == 0)
    {
      GtkToolPalette *palette = GTK_TOOL_PALETTE (container);
      GList *children;
      gint position, size;

      children = glade_util_container_get_all_children (GTK_CONTAINER (palette));
      size = g_list_length (children);
      g_list_free (children);

      position = g_value_get_int (value);

      if (position >= size)
        position = size - 1;

      gtk_tool_palette_set_group_position (palette, GTK_TOOL_ITEM_GROUP (child), position);
    }
  else
    /* Chain Up */
    GWA_GET_CLASS
        (GTK_TYPE_CONTAINER)->child_set_property (adaptor,
                                                  container, child,
                                                  property_name, value);
}

G_MODULE_EXPORT gboolean
glade_gtk_tool_palette_add_verify (GladeWidgetAdaptor *adaptor,
                                   GtkWidget          *container,
                                   GtkWidget          *child,
                                   gboolean            user_feedback)
{
  if (!GTK_IS_TOOL_ITEM_GROUP (child))
    {
      if (user_feedback)
        {
          GladeWidgetAdaptor *tool_item_group_adaptor = 
            glade_widget_adaptor_get_by_type (GTK_TYPE_TOOL_ITEM_GROUP);

          glade_util_ui_message (glade_app_get_window (),
                                 GLADE_UI_INFO, NULL,
                                 ONLY_THIS_GOES_IN_THAT_MSG,
                                 glade_widget_adaptor_get_title (tool_item_group_adaptor),
                                 glade_widget_adaptor_get_title (adaptor));
        }

      return FALSE;
    }

  return TRUE;
}

G_MODULE_EXPORT void
glade_gtk_tool_palette_add_child (GladeWidgetAdaptor *adaptor,
                                  GObject            *object,
                                  GObject            *child)
{
  GtkToolPalette *palette;
  GtkToolItemGroup *group;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (object));
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (child));

  palette = GTK_TOOL_PALETTE (object);
  group   = GTK_TOOL_ITEM_GROUP (child);

  gtk_container_add (GTK_CONTAINER (palette), GTK_WIDGET (group));

  if (glade_util_object_is_loading (object))
    {
      GladeWidget *gchild = glade_widget_get_from_gobject (child);

      /* Packing props arent around when parenting during a glade_widget_dup() */
      if (gchild && glade_widget_get_packing_properties (gchild))
        glade_widget_pack_property_set (gchild, "position",
                                        gtk_tool_palette_get_group_position (palette, group));
    }
}

G_MODULE_EXPORT void
glade_gtk_tool_palette_remove_child (GladeWidgetAdaptor *adaptor,
                                     GObject            *object,
                                     GObject            *child)
{
  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
}

static void
glade_gtk_tool_palette_launch_editor (GladeWidgetAdaptor *adaptor,
                                      GObject            *palette)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GType image_menu_item = GTK_TYPE_IMAGE_MENU_ITEM;
G_GNUC_END_IGNORE_DEPRECATIONS
  GladeBaseEditor *editor;
  GtkWidget *window;

  /* Editor */
  editor = glade_base_editor_new (palette, NULL,
                                  _("Group"), GTK_TYPE_TOOL_ITEM_GROUP,
                                  NULL);

  glade_base_editor_append_types (editor, GTK_TYPE_TOOL_ITEM_GROUP,
                                  _("Button"), GTK_TYPE_TOOL_BUTTON,
                                  _("Toggle"), GTK_TYPE_TOGGLE_TOOL_BUTTON,
                                  _("Radio"), GTK_TYPE_RADIO_TOOL_BUTTON,
                                  _("Menu"), GTK_TYPE_MENU_TOOL_BUTTON,
                                  _("Custom"), GTK_TYPE_TOOL_ITEM,
                                  _("Separator"), GTK_TYPE_SEPARATOR_TOOL_ITEM,
                                  NULL);

  glade_base_editor_append_types (editor, GTK_TYPE_MENU_TOOL_BUTTON,
                                  _("Normal"), GTK_TYPE_MENU_ITEM,
                                  _("Image"), image_menu_item,
                                  _("Check"), GTK_TYPE_CHECK_MENU_ITEM,
                                  _("Radio"), GTK_TYPE_RADIO_MENU_ITEM,
                                  _("Separator"), GTK_TYPE_SEPARATOR_MENU_ITEM,
                                  _("Recent Menu"), GTK_TYPE_RECENT_CHOOSER_MENU,
                                  NULL);

  glade_base_editor_append_types (editor, GTK_TYPE_MENU_ITEM,
                                  _("Normal"), GTK_TYPE_MENU_ITEM,
                                  _("Image"), image_menu_item,
                                  _("Check"), GTK_TYPE_CHECK_MENU_ITEM,
                                  _("Radio"), GTK_TYPE_RADIO_MENU_ITEM,
                                  _("Separator"), GTK_TYPE_SEPARATOR_MENU_ITEM,
                                  _("Recent Menu"), GTK_TYPE_RECENT_CHOOSER_MENU,
                                  NULL);

  g_signal_connect (editor, "get-display-name",
                    G_CALLBACK (glade_gtk_menu_shell_tool_item_get_display_name), NULL);
  g_signal_connect (editor, "child-selected",
                    G_CALLBACK (glade_gtk_menu_shell_tool_item_child_selected),
                    NULL);
  g_signal_connect (editor, "change-type",
                    G_CALLBACK (glade_gtk_menu_shell_change_type), NULL);
  g_signal_connect (editor, "build-child",
                    G_CALLBACK (glade_gtk_menu_shell_build_child), NULL);
  g_signal_connect (editor, "delete-child",
                    G_CALLBACK (glade_gtk_menu_shell_delete_child), NULL);
  g_signal_connect (editor, "move-child",
                    G_CALLBACK (glade_gtk_menu_shell_move_child), NULL);

  gtk_widget_show (GTK_WIDGET (editor));

  window =
      glade_base_editor_pack_new_window (editor, _("Tool Palette Editor"), NULL);
  gtk_widget_show (window);
}

G_MODULE_EXPORT void
glade_gtk_tool_palette_action_activate (GladeWidgetAdaptor *adaptor,
                                        GObject            *object,
                                        const gchar        *action_path)
{
  if (strcmp (action_path, "launch_editor") == 0)
    {
      glade_gtk_tool_palette_launch_editor (adaptor, object);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->action_activate (adaptor,
                                                         object, action_path);
}
