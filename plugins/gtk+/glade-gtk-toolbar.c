/*
 * glade-gtk-toolbar.c - GladeWidgetAdaptor for GtkToolbar
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

#include "glade-gtk-image.h"
#include "glade-gtk-menu-shell.h"
#include "glade-gtk.h"

/* need to unset/reset toolbar-style/icon-size when property is disabled/enabled */
static void
property_toolbar_style_notify_enabled (GladeProperty *property,
                                       GParamSpec    *spec, 
                                       GtkWidget     *widget)
{
  GtkToolbarStyle style;

  if (glade_property_get_enabled (property))
    {
      glade_property_get (property, &style);

      if (GTK_IS_TOOLBAR (widget))
        gtk_toolbar_set_style (GTK_TOOLBAR (widget), style);
      else if (GTK_IS_TOOL_PALETTE (widget))
        gtk_tool_palette_set_style (GTK_TOOL_PALETTE (widget), style);
    }
  else
    {
      if (GTK_IS_TOOLBAR (widget))
        gtk_toolbar_unset_style (GTK_TOOLBAR (widget));
      else if (GTK_IS_TOOL_PALETTE (widget))
        gtk_tool_palette_unset_style (GTK_TOOL_PALETTE (widget));
    }
}

static void
property_icon_size_notify_enabled (GladeProperty *property,
                                   GParamSpec    *spec, 
                                   GtkWidget     *widget)
{
  gint size;

  if (glade_property_get_enabled (property))
    {
      glade_property_get (property, &size);

      if (GTK_IS_TOOLBAR (widget))
        gtk_toolbar_set_icon_size (GTK_TOOLBAR (widget), size);
      else if (GTK_IS_TOOL_PALETTE (widget))
        gtk_tool_palette_set_icon_size (GTK_TOOL_PALETTE (widget), size);
    }
  else
    {
      if (GTK_IS_TOOLBAR (widget))
        gtk_toolbar_unset_icon_size (GTK_TOOLBAR (widget));
      else if (GTK_IS_TOOL_PALETTE (widget))
        gtk_tool_palette_unset_icon_size (GTK_TOOL_PALETTE (widget));
    }
}

G_MODULE_EXPORT void
glade_gtk_toolbar_post_create (GladeWidgetAdaptor *adaptor,
                               GObject            *object,
                               GladeCreateReason   reason)
{
  GladeWidget *widget;
  GladeProperty *property;

  widget   = glade_widget_get_from_gobject (object);

  property = glade_widget_get_property (widget, "toolbar-style");
  g_signal_connect (property, "notify::enabled",
                    G_CALLBACK (property_toolbar_style_notify_enabled), object);

  property = glade_widget_get_property (widget, "icon-size");
  g_signal_connect (property, "notify::enabled",
                    G_CALLBACK (property_icon_size_notify_enabled), object);
}

G_MODULE_EXPORT void
glade_gtk_toolbar_get_child_property (GladeWidgetAdaptor *adaptor,
                                      GObject            *container,
                                      GObject            *child,
                                      const gchar        *property_name,
                                      GValue            *value)
{
  g_return_if_fail (GTK_IS_TOOLBAR (container));
  if (GTK_IS_TOOL_ITEM (child) == FALSE)
    return;

  if (strcmp (property_name, "position") == 0)
    {
      g_value_set_int (value,
                       gtk_toolbar_get_item_index (GTK_TOOLBAR (container),
                                                   GTK_TOOL_ITEM (child)));
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
glade_gtk_toolbar_set_child_property (GladeWidgetAdaptor *adaptor,
                                      GObject            *container,
                                      GObject            *child,
                                      const gchar        *property_name,
                                      GValue             *value)
{
  g_return_if_fail (GTK_IS_TOOLBAR (container));
  g_return_if_fail (GTK_IS_TOOL_ITEM (child));

  g_return_if_fail (property_name != NULL || value != NULL);

  if (strcmp (property_name, "position") == 0)
    {
      GtkToolbar *toolbar = GTK_TOOLBAR (container);
      gint position, size;

      position = g_value_get_int (value);
      size = gtk_toolbar_get_n_items (toolbar);

      if (position >= size)
        position = size - 1;

      g_object_ref (child);
      gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET (child));
      gtk_toolbar_insert (toolbar, GTK_TOOL_ITEM (child), position);
      g_object_unref (child);
    }
  else
    /* Chain Up */
    GWA_GET_CLASS
        (GTK_TYPE_CONTAINER)->child_set_property (adaptor,
                                                  container, child,
                                                  property_name, value);
}

G_MODULE_EXPORT gboolean
glade_gtk_toolbar_add_verify (GladeWidgetAdaptor *adaptor,
                              GtkWidget          *container,
                              GtkWidget          *child,
                              gboolean            user_feedback)
{
  if (!GTK_IS_TOOL_ITEM (child))
    {
      if (user_feedback)
        {
          GladeWidgetAdaptor *tool_item_adaptor = 
            glade_widget_adaptor_get_by_type (GTK_TYPE_TOOL_ITEM);

          glade_util_ui_message (glade_app_get_window (),
                                 GLADE_UI_INFO, NULL,
                                 ONLY_THIS_GOES_IN_THAT_MSG,
                                 glade_widget_adaptor_get_title (tool_item_adaptor),
                                 glade_widget_adaptor_get_title (adaptor));
        }

      return FALSE;
    }

  return TRUE;
}

G_MODULE_EXPORT void
glade_gtk_toolbar_add_child (GladeWidgetAdaptor *adaptor,
                             GObject            *object,
                             GObject            *child)
{
  GtkToolbar *toolbar;
  GtkToolItem *item;

  g_return_if_fail (GTK_IS_TOOLBAR (object));
  g_return_if_fail (GTK_IS_TOOL_ITEM (child));

  toolbar = GTK_TOOLBAR (object);
  item = GTK_TOOL_ITEM (child);

  gtk_toolbar_insert (toolbar, item, -1);

  if (glade_util_object_is_loading (object))
    {
      GladeWidget *gchild = glade_widget_get_from_gobject (child);

      /* Packing props arent around when parenting during a glade_widget_dup() */
      if (gchild && glade_widget_get_packing_properties (gchild))
        glade_widget_pack_property_set (gchild, "position",
                                        gtk_toolbar_get_item_index (toolbar,
                                                                    item));
    }
}

G_MODULE_EXPORT void
glade_gtk_toolbar_remove_child (GladeWidgetAdaptor *adaptor,
                                GObject            *object,
                                GObject            *child)
{
  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
}

static void
glade_gtk_toolbar_launch_editor (GladeWidgetAdaptor *adaptor,
                                 GObject            *toolbar)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GType image_menu_item = GTK_TYPE_IMAGE_MENU_ITEM;
G_GNUC_END_IGNORE_DEPRECATIONS
  GladeBaseEditor *editor;
  GtkWidget *window;

  /* Editor */
  editor = glade_base_editor_new (toolbar, NULL,
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
                    G_CALLBACK
                    (glade_gtk_menu_shell_tool_item_get_display_name), NULL);
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
      glade_base_editor_pack_new_window (editor, _("Tool Bar Editor"), NULL);
  gtk_widget_show (window);
}

G_MODULE_EXPORT void
glade_gtk_toolbar_action_activate (GladeWidgetAdaptor *adaptor,
                                   GObject            *object,
                                   const gchar        *action_path)
{
  if (strcmp (action_path, "launch_editor") == 0)
    {
      glade_gtk_toolbar_launch_editor (adaptor, object);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->action_activate (adaptor,
                                                         object, action_path);
}

/* Write the GtkIconSize as an integer */
G_MODULE_EXPORT void
glade_gtk_toolbar_write_widget (GladeWidgetAdaptor *adaptor,
                                GladeWidget        *widget,
                                GladeXmlContext    *context,
                                GladeXmlNode       *node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and write all the normal properties (including "use-stock")... */
  GWA_GET_CLASS (GTK_TYPE_CONTAINER)->write_widget (adaptor, widget, context, node);

  glade_gtk_write_icon_size (widget, context, node, "icon-size");
}
