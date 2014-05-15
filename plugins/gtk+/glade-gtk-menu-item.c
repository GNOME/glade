/*
 * glade-gtk-menu-item.c - GladeWidgetAdaptor for GtkMenuItem
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

#include "glade-activatable-editor.h"
#include "glade-gtk-menu-shell.h"
#include "glade-gtk.h"

void
glade_gtk_menu_item_action_activate (GladeWidgetAdaptor * adaptor,
                                     GObject * object,
                                     const gchar * action_path)
{
  GObject *obj = NULL, *shell = NULL;
  GladeWidget *w = glade_widget_get_from_gobject (object);

  while ((w = glade_widget_get_parent (w)))
    {
      obj = glade_widget_get_object (w);
      if (GTK_IS_MENU_SHELL (obj))
        shell = obj;
    }

  if (strcmp (action_path, "launch_editor") == 0)
    {
      if (shell)
        object = shell;

      if (GTK_IS_MENU_BAR (object))
        glade_gtk_menu_shell_launch_editor (object, _("Edit Menu Bar"));
      else if (GTK_IS_MENU (object))
        glade_gtk_menu_shell_launch_editor (object, _("Edit Menu"));
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->action_activate (adaptor,
                                                         object, action_path);

  if (shell)
    gtk_menu_shell_deactivate (GTK_MENU_SHELL (shell));
}

GObject *
glade_gtk_menu_item_constructor (GType type,
                                 guint n_construct_properties,
                                 GObjectConstructParam * construct_properties)
{
  GladeWidgetAdaptor *adaptor;
  GObject *ret_obj;

  ret_obj = GWA_GET_OCLASS (GTK_TYPE_CONTAINER)->constructor
      (type, n_construct_properties, construct_properties);

  adaptor = GLADE_WIDGET_ADAPTOR (ret_obj);

  glade_widget_adaptor_action_remove (adaptor, "add_parent");
  glade_widget_adaptor_action_remove (adaptor, "remove_parent");

  return ret_obj;
}

void
glade_gtk_menu_item_post_create (GladeWidgetAdaptor * adaptor,
                                 GObject * object, GladeCreateReason reason)
{
  if (GTK_IS_SEPARATOR_MENU_ITEM (object))
    return;

  if (gtk_bin_get_child (GTK_BIN (object)) == NULL)
    {
      GtkWidget *label = gtk_label_new ("");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_container_add (GTK_CONTAINER (object), label);
    }
}

GList *
glade_gtk_menu_item_get_children (GladeWidgetAdaptor * adaptor,
                                  GObject * object)
{
  GList *list = NULL;
  GtkWidget *child;

  g_return_val_if_fail (GTK_IS_MENU_ITEM (object), NULL);

  if ((child = gtk_menu_item_get_submenu (GTK_MENU_ITEM (object))))
    list = g_list_append (list, child);

  return list;
}

gboolean
glade_gtk_menu_item_add_verify (GladeWidgetAdaptor *adaptor,
				GtkWidget          *container,
				GtkWidget          *child,
				gboolean            user_feedback)
{
  if (!GTK_IS_MENU (child))
    {
      if (user_feedback)
	{
	  GladeWidgetAdaptor *menu_adaptor = 
	    glade_widget_adaptor_get_by_type (GTK_TYPE_MENU);

	  glade_util_ui_message (glade_app_get_window (),
				 GLADE_UI_INFO, NULL,
				 ONLY_THIS_GOES_IN_THAT_MSG,
				 glade_widget_adaptor_get_title (menu_adaptor),
				 glade_widget_adaptor_get_title (adaptor));
	}

      return FALSE;
    }
  else if (GTK_IS_SEPARATOR_MENU_ITEM (container))
    {
      if (user_feedback)
	{
	  glade_util_ui_message (glade_app_get_window (),
				 GLADE_UI_INFO, NULL,
				 _("An object of type %s cannot have any children."),
				 glade_widget_adaptor_get_title (adaptor));
	}

      return FALSE;
    }

  return TRUE;
}

void
glade_gtk_menu_item_add_child (GladeWidgetAdaptor * adaptor,
                               GObject * object, GObject * child)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (object));
  g_return_if_fail (GTK_IS_MENU (child));

  if (GTK_IS_SEPARATOR_MENU_ITEM (object))
    {
      g_warning
          ("You shouldn't try to add a GtkMenu to a GtkSeparatorMenuItem");
      return;
    }

  g_object_set_data (child, "special-child-type", "submenu");

  gtk_menu_item_set_submenu (GTK_MENU_ITEM (object), GTK_WIDGET (child));
}

void
glade_gtk_menu_item_remove_child (GladeWidgetAdaptor * adaptor,
                                  GObject * object, GObject * child)
{
  g_return_if_fail (GTK_IS_MENU_ITEM (object));
  g_return_if_fail (GTK_IS_MENU (child));

  g_object_set_data (child, "special-child-type", NULL);

  gtk_menu_item_set_submenu (GTK_MENU_ITEM (object), NULL);
}

static void
glade_gtk_menu_item_set_label (GObject * object, const GValue * value)
{
  GladeWidget *gitem;
  GtkWidget *label;
  gboolean use_underline;

  gitem = glade_widget_get_from_gobject (object);

  label = gtk_bin_get_child (GTK_BIN (object));
  gtk_label_set_text (GTK_LABEL (label), g_value_get_string (value));

  /* Update underline incase... */
  glade_widget_property_get (gitem, "use-underline", &use_underline);
  gtk_label_set_use_underline (GTK_LABEL (label), use_underline);
}

static void
glade_gtk_menu_item_set_use_underline (GObject * object, const GValue * value)
{
  GtkWidget *label;

  label = gtk_bin_get_child (GTK_BIN (object));
  gtk_label_set_use_underline (GTK_LABEL (label), g_value_get_boolean (value));
}


void
glade_gtk_menu_item_set_property (GladeWidgetAdaptor * adaptor,
                                  GObject * object,
                                  const gchar * id, const GValue * value)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);
  GladeProperty *property = glade_widget_get_property (gwidget, id);

  if (!strcmp (id, "use-underline"))
    glade_gtk_menu_item_set_use_underline (object, value);
  else if (!strcmp (id, "label"))
    glade_gtk_menu_item_set_label (object, value);
  else if (GPC_VERSION_CHECK
           (glade_property_get_class (property), gtk_major_version, gtk_minor_version + 1))
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id,
                                                      value);
}

GladeEditable *
glade_gtk_activatable_create_editable (GladeWidgetAdaptor * adaptor,
                                       GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_activatable_editor_new (adaptor, NULL);

  return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}
