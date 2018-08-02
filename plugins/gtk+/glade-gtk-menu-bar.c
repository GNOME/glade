/*
 * glade-gtk-menu-bar.c - GladeWidgetAdaptor for GtkMenuBar
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

static GladeWidget *
glade_gtk_menu_bar_append_new_submenu (GladeWidget  *parent,
                                       GladeProject *project)
{
  static GladeWidgetAdaptor *submenu_adaptor = NULL;
  GladeWidget *gsubmenu;

  if (submenu_adaptor == NULL)
    submenu_adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_MENU);

  gsubmenu = glade_widget_adaptor_create_widget (submenu_adaptor, FALSE,
                                                 "parent", parent,
                                                 "project", project, NULL);

  glade_widget_add_child (parent, gsubmenu, FALSE);

  return gsubmenu;
}

static GladeWidget *
glade_gtk_menu_bar_append_new_item (GladeWidget  *parent,
                                    GladeProject *project,
                                    const gchar  *label,
                                    gboolean      use_stock)
{
  static GladeWidgetAdaptor *item_adaptor =
      NULL, *image_item_adaptor, *separator_adaptor;
  GladeWidget *gitem;

  if (item_adaptor == NULL)
    {
      item_adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_MENU_ITEM);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      image_item_adaptor =
          glade_widget_adaptor_get_by_type (GTK_TYPE_IMAGE_MENU_ITEM);
G_GNUC_END_IGNORE_DEPRECATIONS
      separator_adaptor =
          glade_widget_adaptor_get_by_type (GTK_TYPE_SEPARATOR_MENU_ITEM);
    }

  if (label)
    {
      gitem =
          glade_widget_adaptor_create_widget ((use_stock) ? image_item_adaptor :
                                              item_adaptor, FALSE, "parent",
                                              parent, "project", project, NULL);

      glade_widget_property_set (gitem, "use-underline", TRUE);

      if (use_stock)
        {
          glade_widget_property_set (gitem, "use-stock", TRUE);
          glade_widget_property_set (gitem, "stock", label);
        }
      else
        glade_widget_property_set (gitem, "label", label);
    }
  else
    {
      gitem = glade_widget_adaptor_create_widget (separator_adaptor,
                                                  FALSE, "parent", parent,
                                                  "project", project, NULL);
    }

  glade_widget_add_child (parent, gitem, FALSE);

  return gitem;
}

void
glade_gtk_menu_bar_post_create (GladeWidgetAdaptor *adaptor,
                                GObject            *object,
                                GladeCreateReason   reason)
{
  GladeProject *project;
  GladeWidget *gmenubar, *gitem, *gsubmenu;

  g_return_if_fail (GTK_IS_MENU_BAR (object));
  gmenubar = glade_widget_get_from_gobject (object);
  g_return_if_fail (GLADE_IS_WIDGET (gmenubar));

  if (reason != GLADE_CREATE_USER)
    return;

  project = glade_widget_get_project (gmenubar);

  /* File */
  gitem =
      glade_gtk_menu_bar_append_new_item (gmenubar, project, _("_File"), FALSE);
  gsubmenu = glade_gtk_menu_bar_append_new_submenu (gitem, project);
  glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-new", TRUE);
  glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-open", TRUE);
  glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-save", TRUE);
  glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-save-as", TRUE);
  glade_gtk_menu_bar_append_new_item (gsubmenu, project, NULL, FALSE);
  glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-quit", TRUE);

  /* Edit */
  gitem =
      glade_gtk_menu_bar_append_new_item (gmenubar, project, _("_Edit"), FALSE);
  gsubmenu = glade_gtk_menu_bar_append_new_submenu (gitem, project);
  glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-cut", TRUE);
  glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-copy", TRUE);
  glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-paste", TRUE);
  glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-delete", TRUE);

  /* View */
  gitem =
      glade_gtk_menu_bar_append_new_item (gmenubar, project, _("_View"), FALSE);

  /* Help */
  gitem =
      glade_gtk_menu_bar_append_new_item (gmenubar, project, _("_Help"), FALSE);
  gsubmenu = glade_gtk_menu_bar_append_new_submenu (gitem, project);
  glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-about", TRUE);
}
