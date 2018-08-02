/*
 * glade-gtk-menu-shell.c - GladeWidgetAdaptor for GtkMenuShell
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

#include "glade-gtk-menu-shell.h"
#include "glade-gtk.h"

gboolean
glade_gtk_menu_shell_add_verify (GladeWidgetAdaptor *adaptor,
                                 GtkWidget          *container,
                                 GtkWidget          *child,
                                 gboolean            user_feedback)
{
  if (!GTK_IS_MENU_ITEM (child))
    {
      if (user_feedback)
        {
          GladeWidgetAdaptor *menu_item_adaptor = 
            glade_widget_adaptor_get_by_type (GTK_TYPE_MENU_ITEM);

          glade_util_ui_message (glade_app_get_window (),
                                 GLADE_UI_INFO, NULL,
                                 ONLY_THIS_GOES_IN_THAT_MSG,
                                 glade_widget_adaptor_get_title (menu_item_adaptor),
                                 glade_widget_adaptor_get_title (adaptor));
        }

      return FALSE;
    }

  return TRUE;
}

void
glade_gtk_menu_shell_add_child (GladeWidgetAdaptor *adaptor,
                                GObject            *object,
                                GObject            *child)
{

  g_return_if_fail (GTK_IS_MENU_SHELL (object));
  g_return_if_fail (GTK_IS_MENU_ITEM (child));

  gtk_menu_shell_append (GTK_MENU_SHELL (object), GTK_WIDGET (child));
}


void
glade_gtk_menu_shell_remove_child (GladeWidgetAdaptor *adaptor,
                                   GObject            *object,
                                   GObject            *child)
{
  g_return_if_fail (GTK_IS_MENU_SHELL (object));
  g_return_if_fail (GTK_IS_MENU_ITEM (child));

  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
}

static gint
glade_gtk_menu_shell_get_item_position (GObject * container, GObject * child)
{
  gint position = 0;
  GList *list = gtk_container_get_children (GTK_CONTAINER (container));

  while (list)
    {
      if (G_OBJECT (list->data) == child)
        break;

      list = list->next;
      position++;
    }

  g_list_free (list);

  return position;
}

void
glade_gtk_menu_shell_get_child_property (GladeWidgetAdaptor *adaptor,
                                         GObject            *container,
                                         GObject            *child,
                                         const gchar        *property_name,
                                         GValue             *value)
{
  g_return_if_fail (GTK_IS_MENU_SHELL (container));
  g_return_if_fail (GTK_IS_MENU_ITEM (child));

  if (strcmp (property_name, "position") == 0)
    {
      g_value_set_int (value,
                       glade_gtk_menu_shell_get_item_position (container,
                                                               child));
    }
  else
    /* Chain Up */
    GWA_GET_CLASS
        (GTK_TYPE_CONTAINER)->child_get_property (adaptor,
                                                  container,
                                                  child, property_name, value);
}

void
glade_gtk_menu_shell_set_child_property (GladeWidgetAdaptor *adaptor,
                                         GObject            *container,
                                         GObject            *child,
                                         const gchar        *property_name,
                                         GValue             *value)
{
  g_return_if_fail (GTK_IS_MENU_SHELL (container));
  g_return_if_fail (GTK_IS_MENU_ITEM (child));
  g_return_if_fail (property_name != NULL || value != NULL);

  if (strcmp (property_name, "position") == 0)
    {
      GladeWidget *gitem;
      gint position;

      gitem = glade_widget_get_from_gobject (child);
      g_return_if_fail (GLADE_IS_WIDGET (gitem));

      position = g_value_get_int (value);

      if (position < 0)
        {
          position = glade_gtk_menu_shell_get_item_position (container, child);
          g_value_set_int (value, position);
        }

      g_object_ref (child);
      gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET (child));
      gtk_menu_shell_insert (GTK_MENU_SHELL (container), GTK_WIDGET (child),
                             position);
      g_object_unref (child);

    }
  else
    /* Chain Up */
    GWA_GET_CLASS
        (GTK_TYPE_CONTAINER)->child_set_property (adaptor,
                                                  container, child,
                                                  property_name, value);
}

void
glade_gtk_menu_shell_action_activate (GladeWidgetAdaptor *adaptor,
                                      GObject            *object,
                                      const gchar        *action_path)
{
  if (strcmp (action_path, "launch_editor") == 0)
    {
      if (GTK_IS_MENU_BAR (object))
        glade_gtk_menu_shell_launch_editor (object, _("Edit Menu Bar"));
      else if (GTK_IS_MENU (object))
        glade_gtk_menu_shell_launch_editor (object, _("Edit Menu"));
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->action_activate (adaptor,
                                                         object, action_path);

  gtk_menu_shell_deactivate (GTK_MENU_SHELL (object));
}

gchar *
glade_gtk_menu_shell_tool_item_get_display_name (GladeBaseEditor *editor,
                                                 GladeWidget     *gchild,
                                                 gpointer         user_data)
{
  GObject *child = glade_widget_get_object (gchild);
  gchar *name;

  if (GTK_IS_SEPARATOR_MENU_ITEM (child) || GTK_IS_SEPARATOR_TOOL_ITEM (child))
    name = _("<separator>");
  else if (GTK_IS_MENU_ITEM (child))
    glade_widget_property_get (gchild, "label", &name);
  else if (GTK_IS_TOOL_BUTTON (child))
    {
      glade_widget_property_get (gchild, "label", &name);
      if (name == NULL || strlen (name) == 0)
        glade_widget_property_get (gchild, "stock-id", &name);
    }
  else if (GTK_IS_TOOL_ITEM_GROUP (child))
    glade_widget_property_get (gchild, "label", &name);
  else if (GTK_IS_RECENT_CHOOSER_MENU (child))
    name = (gchar *)glade_widget_get_name (gchild);
  else
    name = _("<custom>");

  return g_strdup (name);
}

static GladeWidget *
glade_gtk_menu_shell_item_get_parent (GladeWidget * gparent, GObject * parent)
{
  GtkWidget *submenu = NULL;

  if (GTK_IS_MENU_TOOL_BUTTON (parent))
    submenu = gtk_menu_tool_button_get_menu (GTK_MENU_TOOL_BUTTON (parent));
  else if (GTK_IS_MENU_ITEM (parent))
    submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (parent));

  if (submenu && glade_widget_get_from_gobject (submenu))
    gparent = glade_widget_get_from_gobject (submenu);
  else
    gparent =
      glade_command_create (glade_widget_adaptor_get_by_type (GTK_TYPE_MENU),
                            gparent, NULL,
                            glade_widget_get_project (gparent));

  return gparent;
}

GladeWidget *
glade_gtk_menu_shell_build_child (GladeBaseEditor *editor,
                                  GladeWidget     *gparent,
                                  GType            type,
                                  gpointer         data)
{
  GObject *parent = glade_widget_get_object (gparent);
  GladeWidget *gitem_new;

  if (GTK_IS_SEPARATOR_MENU_ITEM (parent))
    {
      glade_util_ui_message (glade_app_get_window (),
                             GLADE_UI_INFO, NULL,
                             _("Children cannot be added to a separator."));
      return NULL;
    }

  if (GTK_IS_RECENT_CHOOSER_MENU (parent))
    {
      glade_util_ui_message (glade_app_get_window (),
                             GLADE_UI_INFO, NULL,
                             _("Children cannot be added to a Recent Chooser Menu."));
      return NULL;
    }

  if (g_type_is_a (type, GTK_TYPE_MENU) && GTK_IS_MENU_TOOL_BUTTON (parent) &&
      gtk_menu_tool_button_get_menu (GTK_MENU_TOOL_BUTTON (parent)) != NULL)
    {
      glade_util_ui_message (glade_app_get_window (),
                             GLADE_UI_INFO, NULL,
                             _("%s already has a menu."),
                             glade_widget_get_name (gparent));
      return NULL;
    }

  if (g_type_is_a (type, GTK_TYPE_MENU) && GTK_IS_MENU_ITEM (parent) &&
      gtk_menu_item_get_submenu (GTK_MENU_ITEM (parent)) != NULL)
    {
      glade_util_ui_message (glade_app_get_window (),
                             GLADE_UI_INFO, NULL,
                             _("%s item already has a submenu."),
                             glade_widget_get_name (gparent));
      return NULL;
    }

  /* Get or build real parent */
  if (!g_type_is_a (type, GTK_TYPE_MENU) &&
      (GTK_IS_MENU_ITEM (parent) || GTK_IS_MENU_TOOL_BUTTON (parent)))
    gparent = glade_gtk_menu_shell_item_get_parent (gparent, parent);

  /* Build child */
  gitem_new = glade_command_create (glade_widget_adaptor_get_by_type (type),
                                    gparent, NULL,
                                    glade_widget_get_project (gparent));

  if (type != GTK_TYPE_SEPARATOR_MENU_ITEM &&
      type != GTK_TYPE_SEPARATOR_TOOL_ITEM &&
      !g_type_is_a (type, GTK_TYPE_MENU))
    {
      glade_widget_property_set (gitem_new, "label",
                                 glade_widget_get_name (gitem_new));
      glade_widget_property_set (gitem_new, "use-underline", TRUE);
    }

  return gitem_new;
}

gboolean
glade_gtk_menu_shell_delete_child (GladeBaseEditor *editor,
                                   GladeWidget     *gparent,
                                   GladeWidget     *gchild,
                                   gpointer         data)
{
  GObject *item = glade_widget_get_object (gparent);
  GtkWidget *submenu = NULL;
  GList list = { 0, };
  gint n_children = 0;

  if (GTK_IS_MENU_ITEM (item) &&
      (submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (item))))
    {
      GList *l = gtk_container_get_children (GTK_CONTAINER (submenu));
      n_children = g_list_length (l);
      g_list_free (l);
    }

  if (submenu && n_children == 1)
    list.data = glade_widget_get_parent (gchild);
  else
    list.data = gchild;

  /* Remove widget */
  glade_command_delete (&list);

  return TRUE;
}

gboolean
glade_gtk_menu_shell_move_child (GladeBaseEditor *editor,
                                 GladeWidget     *gparent,
                                 GladeWidget     *gchild,
                                 gpointer         data)
{
  GObject     *parent     = glade_widget_get_object (gparent);
  GObject     *child      = glade_widget_get_object (gchild);
  GladeWidget *old_parent = glade_widget_get_parent (gchild);
  GladeWidget *old_parent_parent;
  GList list = { 0, };

  /* Some parents just dont take children at all */
  if (GTK_IS_SEPARATOR_MENU_ITEM (parent) ||
      GTK_IS_SEPARATOR_TOOL_ITEM (parent) ||
      GTK_IS_RECENT_CHOOSER_MENU (parent))
    return FALSE;

  /* Moving a menu item child */
  if (GTK_IS_MENU_ITEM (child))
    {
      if (GTK_IS_TOOLBAR (parent))         return FALSE;
      if (GTK_IS_TOOL_ITEM_GROUP (parent)) return FALSE;
      if (GTK_IS_TOOL_PALETTE (parent))    return FALSE;

      if (GTK_IS_TOOL_ITEM (parent) && !GTK_IS_MENU_TOOL_BUTTON (parent))
        return FALSE;
    }

  /* Moving a toolitem child */
  if (GTK_IS_TOOL_ITEM (child))
    {
      if (GTK_IS_MENU (parent))         return FALSE;
      if (GTK_IS_MENU_BAR (parent))     return FALSE;
      if (GTK_IS_MENU_ITEM (parent))    return FALSE;
      if (GTK_IS_TOOL_PALETTE (parent)) return FALSE;
      if (GTK_IS_TOOL_ITEM (parent))    return FALSE;
    }

  /* Moving a Recent Chooser Menu */
  if (GTK_IS_RECENT_CHOOSER_MENU (child))
    {
      if (GTK_IS_MENU_ITEM (parent))
        {
          if (gtk_menu_item_get_submenu (GTK_MENU_ITEM (parent)) != NULL)
            return FALSE;
        }
      else if (GTK_IS_MENU_TOOL_BUTTON (parent))
        {
          if (gtk_menu_tool_button_get_menu (GTK_MENU_TOOL_BUTTON (parent)) != NULL)
            return FALSE;
        }
      else
        return FALSE;
    }

  /* Moving a toolitem group */
  if (GTK_IS_TOOL_ITEM_GROUP (child))
    {
      if (!GTK_IS_TOOL_PALETTE (parent)) return FALSE;
    }

  if (!GTK_IS_MENU (child) &&
      (GTK_IS_MENU_ITEM (parent) || 
       GTK_IS_MENU_TOOL_BUTTON (parent)))
    gparent = glade_gtk_menu_shell_item_get_parent (gparent, parent);

  if (gparent != glade_widget_get_parent (gchild))
    {
      list.data = gchild;
      glade_command_dnd (&list, gparent, NULL);
    }

  /* Delete dangling childless menus */
  old_parent_parent = glade_widget_get_parent (old_parent);
  if (GTK_IS_MENU (glade_widget_get_object (old_parent)) &&
      old_parent_parent && GTK_IS_MENU_ITEM (glade_widget_get_object (old_parent_parent)))
    {
      GList del = { 0, }, *children;

      children =
        gtk_container_get_children (GTK_CONTAINER (glade_widget_get_object (old_parent)));
      if (!children)
        {
          del.data = old_parent;
          glade_command_delete (&del);
        }
      g_list_free (children);
    }

  return TRUE;
}

gboolean
glade_gtk_menu_shell_change_type (GladeBaseEditor *editor,
                                  GladeWidget     *gchild,
                                  GType            type,
                                  gpointer         data)
{
  GObject *child = glade_widget_get_object (gchild);

  if ((type == GTK_TYPE_SEPARATOR_MENU_ITEM &&
       gtk_menu_item_get_submenu (GTK_MENU_ITEM (child))) ||
      (GTK_IS_MENU_TOOL_BUTTON (child) &&
       gtk_menu_tool_button_get_menu (GTK_MENU_TOOL_BUTTON (child))) ||
      GTK_IS_MENU (child) || g_type_is_a (type, GTK_TYPE_MENU))
    return TRUE;

  /* Delete the internal image of an image menu item before going ahead and changing types. */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  if (GTK_IS_IMAGE_MENU_ITEM (child))
G_GNUC_END_IGNORE_DEPRECATIONS
    {
      GList list = { 0, };
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      GtkWidget *image =
          gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (child));
G_GNUC_END_IGNORE_DEPRECATIONS
      GladeWidget *widget;

      if (image && (widget = glade_widget_get_from_gobject (image)))
        {
          list.data = widget;
          glade_command_unlock_widget (widget);
          glade_command_delete (&list);
        }
    }

  return FALSE;
}

void
glade_gtk_menu_shell_launch_editor (GObject *object, gchar *title)
{
  GladeBaseEditor *editor;
  GtkWidget *window;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GType type_image_menu_item = GTK_TYPE_IMAGE_MENU_ITEM;
G_GNUC_END_IGNORE_DEPRECATIONS

  /* Editor */
  editor = glade_base_editor_new (object, NULL,
                                  _("Normal item"), GTK_TYPE_MENU_ITEM,
                                  _("Image item"), type_image_menu_item,
                                  _("Check item"), GTK_TYPE_CHECK_MENU_ITEM,
                                  _("Radio item"), GTK_TYPE_RADIO_MENU_ITEM,
                                  _("Separator item"),
                                  GTK_TYPE_SEPARATOR_MENU_ITEM, NULL);

  glade_base_editor_append_types (editor, GTK_TYPE_MENU_ITEM,
                                  _("Normal item"), GTK_TYPE_MENU_ITEM,
                                  _("Image item"), type_image_menu_item,
                                  _("Check item"), GTK_TYPE_CHECK_MENU_ITEM,
                                  _("Radio item"), GTK_TYPE_RADIO_MENU_ITEM,
                                  _("Separator item"), GTK_TYPE_SEPARATOR_MENU_ITEM, 
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

  window = glade_base_editor_pack_new_window (editor, title, NULL);
  gtk_widget_show (window);
}

void
glade_gtk_toolbar_child_selected (GladeBaseEditor *editor,
                                  GladeWidget     *gchild,
                                  gpointer         data)
{
  GladeWidget *gparent = glade_widget_get_parent (gchild);
  GObject     *parent  = glade_widget_get_object (gparent);
  GObject     *child = glade_widget_get_object (gchild);
  GType        type = G_OBJECT_TYPE (child);

  glade_base_editor_add_label (editor, _("Tool Item"));

  glade_base_editor_add_default_properties (editor, gchild);

  glade_base_editor_add_label (editor, _("Properties"));
  glade_base_editor_add_properties (editor, gchild, FALSE, 
                                    "tooltip-text",
                                    "accelerator", 
                                    NULL);
  glade_base_editor_add_editable (editor, gchild, GLADE_PAGE_GENERAL);

  if (type == GTK_TYPE_SEPARATOR_TOOL_ITEM)
    return;

  glade_base_editor_add_label (editor, _("Packing"));
  if (GTK_IS_TOOLBAR (parent))
    glade_base_editor_add_properties (editor, gchild, TRUE,
                                      "expand", "homogeneous", NULL);
  else if (GTK_IS_TOOL_ITEM_GROUP (parent))
    glade_base_editor_add_properties (editor, gchild, TRUE,
                                      "expand", "fill", "homogeneous", "new-row", NULL);
}

void
glade_gtk_tool_palette_child_selected (GladeBaseEditor *editor,
                                       GladeWidget     *gchild,
                                       gpointer         data)
{
  glade_base_editor_add_label (editor, _("Tool Item Group"));

  glade_base_editor_add_default_properties (editor, gchild);

  glade_base_editor_add_label (editor, _("Properties"));
  glade_base_editor_add_properties (editor, gchild, FALSE, 
                                    "tooltip-text",
                                    NULL);
  glade_base_editor_add_editable (editor, gchild, GLADE_PAGE_GENERAL);

  glade_base_editor_add_label (editor, _("Packing"));
  glade_base_editor_add_properties (editor, gchild, TRUE,
                                    "exclusive", "expand", NULL);
}

void
glade_gtk_recent_chooser_menu_child_selected (GladeBaseEditor *editor,
                                              GladeWidget     *gchild,
                                              gpointer         data)
{
  glade_base_editor_add_label (editor, _("Recent Chooser Menu"));

  glade_base_editor_add_default_properties (editor, gchild);

  glade_base_editor_add_label (editor, _("Properties"));
  glade_base_editor_add_editable (editor, gchild, GLADE_PAGE_GENERAL);
}

void
glade_gtk_menu_shell_tool_item_child_selected (GladeBaseEditor *editor,
                                               GladeWidget     *gchild,
                                               gpointer         data)
{
  GObject *child = glade_widget_get_object (gchild);
  GType type = G_OBJECT_TYPE (child);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GType image_menu_item = GTK_TYPE_IMAGE_MENU_ITEM;
G_GNUC_END_IGNORE_DEPRECATIONS

  if (GTK_IS_TOOL_ITEM (child))
    {
      glade_gtk_toolbar_child_selected (editor, gchild, data);
      return;
    }

  if (GTK_IS_TOOL_ITEM_GROUP (child))
    {
      glade_gtk_tool_palette_child_selected (editor, gchild, data);
      return;
    }

  if (GTK_IS_RECENT_CHOOSER_MENU (child))
    {
      glade_gtk_recent_chooser_menu_child_selected (editor, gchild, data);
      return;
    }


  glade_base_editor_add_label (editor, _("Menu Item"));

  glade_base_editor_add_default_properties (editor, gchild);

  if (GTK_IS_SEPARATOR_MENU_ITEM (child))
    return;

  glade_base_editor_add_label (editor, _("Properties"));

  if (type != image_menu_item)
    glade_base_editor_add_properties (editor, gchild, FALSE, 
                                      "label", 
                                      "tooltip-text",
                                      "accelerator", 
                                      NULL);
  else
    glade_base_editor_add_properties (editor, gchild, FALSE, 
                                      "tooltip-text",
                                      "accelerator", 
                                      NULL);

  if (type == image_menu_item)
    glade_base_editor_add_editable (editor, gchild, GLADE_PAGE_GENERAL);
  else if (type == GTK_TYPE_CHECK_MENU_ITEM)
    glade_base_editor_add_properties (editor, gchild, FALSE,
                                      "active", 
                                      "draw-as-radio",
                                      "inconsistent", 
                                      NULL);
  else if (type == GTK_TYPE_RADIO_MENU_ITEM)
    glade_base_editor_add_properties (editor, gchild, FALSE,
                                      "active", 
                                      "group", NULL);
}
