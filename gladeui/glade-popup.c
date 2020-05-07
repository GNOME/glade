/*
 * Copyright (C) 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Chema Celorio <chema@celorio.com>
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-widget-adaptor.h"
#include "glade-popup.h"
#include "glade-placeholder.h"
#include "glade-clipboard.h"
#include "glade-command.h"
#include "glade-project.h"
#include "glade-app.h"

static void
glade_popup_docs_cb (GtkMenuItem *item, GladeWidgetAdaptor *adaptor)
{
  g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));

  glade_app_search_docs (glade_widget_adaptor_get_book (adaptor),
                         glade_widget_adaptor_get_display_name (adaptor),
                         NULL);
}

/********************************************************
                      WIDGET POPUP
 *******************************************************/
static void
glade_popup_select_cb (GtkMenuItem *item, GladeWidget *widget)
{
  glade_project_selection_set (glade_widget_get_project (widget),
                               glade_widget_get_object (widget), TRUE);
}

typedef struct {
  GladeWidgetAdaptor *adaptor;
  GladeProject       *project;
  GladeWidget        *parent;
  GladePlaceholder   *placeholder;
} RootAddData;

static void
glade_popup_widget_add_cb (GtkMenuItem *item, RootAddData *data)
{
  g_return_if_fail (data->adaptor != NULL);

  if (glade_command_create (data->adaptor, data->parent,
                            data->placeholder, data->project))

    glade_project_set_add_item (data->project, NULL);
}

static void
glade_popup_root_add_cb (GtkMenuItem *item, RootAddData *data)
{
  if (glade_command_create (data->adaptor, NULL, NULL, data->project))
    glade_project_set_add_item (data->project, NULL);
}

static void
glade_popup_cut_cb (GtkMenuItem *item, GladeWidget *widget)
{
  GladeProject *project = glade_widget_get_project (widget);

  /* Assign selection first only if its not already assigned (it may be a delete
   * of multiple widgets) */
  if (!glade_project_is_selected (project, glade_widget_get_object (widget)))
    glade_project_selection_set (project, glade_widget_get_object (widget), FALSE);

  glade_project_command_cut (project);
}

static void
glade_popup_copy_cb (GtkMenuItem *item, GladeWidget *widget)
{
  GladeProject *project = glade_widget_get_project (widget);

  /* Assign selection first */
  if (!glade_project_is_selected (project, glade_widget_get_object (widget)))
    glade_project_selection_set (project, glade_widget_get_object (widget), FALSE);

  glade_project_copy_selection (project);
}

static void
glade_popup_paste_cb (GtkMenuItem *item, gpointer data)
{
  GladeWidget  *widget = NULL;
  GladeProject *project;

  if (GLADE_IS_WIDGET (data))
    {
      widget  = GLADE_WIDGET (data);
      project = glade_widget_get_project (widget);
    }
  else if (GLADE_IS_PROJECT (data))
    project = GLADE_PROJECT (data);
  else
    g_return_if_reached ();

  /* The selected widget is the paste destination */
  if (widget)
    glade_project_selection_set (project, glade_widget_get_object (widget), FALSE);
  else
    glade_project_selection_clear (project, FALSE);

  glade_project_command_paste (project, NULL);
}

static void
glade_popup_delete_cb (GtkMenuItem *item, GladeWidget *widget)
{
  GladeProject *project = glade_widget_get_project (widget);

  /* Assign selection first */
  if (glade_project_is_selected
      (project, glade_widget_get_object (widget)) == FALSE)
    glade_project_selection_set (project, glade_widget_get_object (widget), FALSE);

  glade_project_command_delete (project);
}

/********************************************************
                  PLACEHOLDER POPUP
 *******************************************************/
static void
glade_popup_placeholder_paste_cb (GtkMenuItem      *item, 
                                  GladePlaceholder *placeholder)
{
  GladeProject *project;

  project = glade_placeholder_get_project (placeholder);

  glade_project_selection_clear (project, FALSE);

  glade_project_command_paste (project, placeholder);
}

/********************************************************
                    POPUP BUILDING
 *******************************************************/
static GtkWidget *
glade_popup_append_item (GtkWidget   *popup_menu,
                         const gchar *label,
                         gboolean     sensitive,
                         gpointer     callback,
                         gpointer     data)
{
  GtkWidget *menu_item = gtk_menu_item_new_with_mnemonic (label);

  if (callback)
    g_signal_connect (G_OBJECT (menu_item), "activate",
                      G_CALLBACK (callback), data);

  gtk_widget_set_sensitive (menu_item, sensitive);
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), menu_item);

  return menu_item;
}


static void
glade_popup_menuitem_activated (GtkMenuItem *item, const gchar *action_path)
{
  GladeWidget *widget;

  if ((widget = g_object_get_data (G_OBJECT (item), "gwa-data")))
    glade_widget_adaptor_action_activate (glade_widget_get_adaptor (widget),
                                          glade_widget_get_object (widget), action_path);
}

static void
glade_popup_menuitem_packing_activated (GtkMenuItem *item,
                                        const gchar *action_path)
{
  GladeWidget *widget, *parent;

  if ((widget = g_object_get_data (G_OBJECT (item), "gwa-data")))
    {
      parent = glade_widget_get_parent (widget);
      glade_widget_adaptor_child_action_activate (glade_widget_get_adaptor (parent),
                                                  glade_widget_get_object (parent),
                                                  glade_widget_get_object (widget), action_path);
    }
}

static void
glade_popup_menuitem_ph_packing_activated (GtkMenuItem *item,
                                           const gchar *action_path)
{
  GladePlaceholder *ph;
  GladeWidget *parent;

  if ((ph = g_object_get_data (G_OBJECT (item), "gwa-data")))
    {
      parent = glade_placeholder_get_parent (ph);
      glade_widget_adaptor_child_action_activate (glade_widget_get_adaptor (parent),
                                                  glade_widget_get_object (parent),
                                                  G_OBJECT (ph), action_path);
    }
}

static gint
glade_popup_action_populate_menu_real (GtkWidget   *menu,
                                       GladeWidget *gwidget,
                                       GList       *actions,
                                       GCallback    callback,
                                       gpointer     data)
{
  GtkWidget *item;
  GList *list;
  gint n = 0;

  for (list = actions; list; list = g_list_next (list))
    {
      GladeWidgetAction    *action = list->data;
      GladeWidgetActionDef *adef = glade_widget_action_get_def (action);
      GList                *children = glade_widget_action_get_children (action);
      GtkWidget            *submenu = NULL;

      if (!glade_widget_action_get_visible (action))
        continue;

      if (children)
        {
          submenu = gtk_menu_new ();
          n += glade_popup_action_populate_menu_real (submenu,
                                                      gwidget,
                                                      children,
                                                      callback, data);
        }
      else
        submenu = glade_widget_adaptor_action_submenu (glade_widget_get_adaptor (gwidget),
                                                       glade_widget_get_object (gwidget),
                                                       adef->path);


      item = glade_popup_append_item (menu, adef->label, TRUE,
                                      (children) ? NULL : callback,
                                      (children) ? NULL : adef->path);

      g_object_set_data (G_OBJECT (item), "gwa-data", data);

      gtk_widget_set_sensitive (item, glade_widget_action_get_sensitive (action));

      if (submenu)
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);

      n++;
    }

  return n;
}

/*
 * glade_popup_action_populate_menu:
 * @menu: a GtkMenu to put the actions menu items.
 * @widget: A #GladeWidget
 * @action: a @widget subaction or NULL to include all actions.
 * @packing: TRUE to include packing actions
 *
 * Populate a GtkMenu with widget's actions
 *
 * Returns the number of action appended to the menu.
 */
gint
glade_popup_action_populate_menu (GtkWidget         *menu,
                                  GladeWidget       *widget,
                                  GladeWidgetAction *action,
                                  gboolean           packing)
{
  gint n;

  g_return_val_if_fail (GTK_IS_MENU (menu), 0);
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), 0);

  g_return_val_if_fail (action == NULL || GLADE_IS_WIDGET_ACTION (action), 0);

  if (action)
    {
      GladeWidgetActionDef *adef = glade_widget_action_get_def (action);
      GList                *children = glade_widget_action_get_children (action);
      
      if (glade_widget_get_action (widget, adef->path) &&
          glade_widget_action_get_visible (action))
        return glade_popup_action_populate_menu_real (menu,
                                                      widget,
                                                      children,
                                                      G_CALLBACK
                                                      (glade_popup_menuitem_activated),
                                                      widget);

      if (glade_widget_get_pack_action (widget, adef->path) &&
          glade_widget_action_get_visible (action))
        return glade_popup_action_populate_menu_real (menu,
                                                      glade_widget_get_parent
                                                      (widget), children,
                                                      G_CALLBACK
                                                      (glade_popup_menuitem_packing_activated),
                                                      widget);

      return 0;
    }

  n = glade_popup_action_populate_menu_real (menu,
                                             widget,
                                             glade_widget_get_actions (widget),
                                             G_CALLBACK
                                             (glade_popup_menuitem_activated),
                                             widget);

  if (packing && glade_widget_get_pack_actions (widget))
    {
      if (n)
        {
          GtkWidget *separator = gtk_separator_menu_item_new ();
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), separator);
          gtk_widget_show (separator);
        }

      n += glade_popup_action_populate_menu_real 
        (menu, glade_widget_get_parent (widget),
         glade_widget_get_pack_actions (widget),
         G_CALLBACK (glade_popup_menuitem_packing_activated), widget);
    }

  return n;
}

static GtkWidget *
glade_popup_create_menu (GladeWidget      *widget,
                         GladePlaceholder *placeholder, 
                         GladeProject     *project,
                         gboolean          packing)
{
  GtkWidget          *popup_menu;
  GtkWidget          *separator;
  gboolean            sensitive;
  GladeWidgetAdaptor *adaptor;

  popup_menu = gtk_menu_new ();

  adaptor = glade_project_get_add_item (project);

  if (adaptor)
    {
      RootAddData *data = g_new (RootAddData, 1);
      
      data->adaptor = adaptor;
      data->project = project;
      data->parent = placeholder ? glade_placeholder_get_parent (placeholder) : widget;
      data->placeholder = placeholder;
      
      g_object_set_data_full (G_OBJECT (popup_menu), "root-data-destroy-me", 
                              data, (GDestroyNotify)g_free);

      glade_popup_append_item (popup_menu, _("_Add widget here"),
                               data->parent != NULL,
                               glade_popup_widget_add_cb,
                               data);

      glade_popup_append_item (popup_menu, _("Add widget as _toplevel"), TRUE,
                               glade_popup_root_add_cb, data);

      separator = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), separator);
      gtk_widget_show (separator);
    }

  sensitive = (widget != NULL);

  glade_popup_append_item (popup_menu, _("_Select"), sensitive,
                           glade_popup_select_cb, widget);
  glade_popup_append_item (popup_menu, _("Cu_t"), sensitive,
                           glade_popup_cut_cb, widget);
  glade_popup_append_item (popup_menu, _("_Copy"), sensitive,
                           glade_popup_copy_cb, widget);

  /* paste is placholder specific when the popup is on a placeholder */
  sensitive = glade_clipboard_get_has_selection (glade_app_get_clipboard ());

  if (placeholder)
    glade_popup_append_item (popup_menu, _("_Paste"), sensitive,
                             glade_popup_placeholder_paste_cb, placeholder);
  else if (widget)
    glade_popup_append_item (popup_menu, _("_Paste"), sensitive,
                             glade_popup_paste_cb, widget);
  else
    glade_popup_append_item (popup_menu, _("_Paste"), sensitive,
                             glade_popup_paste_cb, NULL);


  glade_popup_append_item (popup_menu, _("_Delete"), (widget != NULL),
                           glade_popup_delete_cb, widget);


  /* packing actions are a little different on placholders */
  if (placeholder)
    {
      if (widget && glade_widget_get_actions (widget))
        {
          GtkWidget *separator = gtk_separator_menu_item_new ();
          gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), separator);
          gtk_widget_show (separator);

          glade_popup_action_populate_menu_real
              (popup_menu,
               widget,
               glade_widget_get_actions (widget),
               G_CALLBACK (glade_popup_menuitem_activated), widget);
        }

      if (glade_placeholder_packing_actions (placeholder))
        {
          GtkWidget *separator = gtk_separator_menu_item_new ();
          gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), separator);
          gtk_widget_show (separator);

          glade_popup_action_populate_menu_real
              (popup_menu,
               widget,
               glade_placeholder_packing_actions (placeholder),
               G_CALLBACK (glade_popup_menuitem_ph_packing_activated),
               placeholder);
        }
    }
  else if (widget && (glade_widget_get_actions (widget) || 
                      (packing && glade_widget_get_pack_actions (widget))))
    {
      GtkWidget *separator = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (popup_menu), separator);
      gtk_widget_show (separator);

      glade_popup_action_populate_menu (popup_menu, widget, NULL, packing);
    }

  return popup_menu;
}

void
glade_popup_widget_pop (GladeWidget    *widget,
                        GdkEventButton *event,
                        gboolean        packing)
{
  GtkWidget *popup_menu;
  gint button;
  gint event_time;

  g_return_if_fail (GLADE_IS_WIDGET (widget) || widget == NULL);

  popup_menu = glade_popup_create_menu (widget, NULL, glade_widget_get_project (widget), packing);

  if (event)
    {
      button = event->button;
      event_time = event->time;
    }
  else
    {
      button = 0;
      event_time = gtk_get_current_event_time ();
    }
  gtk_menu_popup (GTK_MENU (popup_menu), NULL, NULL,
                  NULL, NULL, button, event_time);
}

void
glade_popup_placeholder_pop (GladePlaceholder *placeholder,
                             GdkEventButton   *event)
{
  GladeWidget *widget;
  GtkWidget *popup_menu;
  gint button;
  gint event_time;

  g_return_if_fail (GLADE_IS_PLACEHOLDER (placeholder));

  widget = glade_placeholder_get_parent (placeholder);

  popup_menu = glade_popup_create_menu (widget, placeholder, 
                                        glade_widget_get_project (widget), TRUE);

  if (event)
    {
      button = event->button;
      event_time = event->time;
    }
  else
    {
      button = 0;
      event_time = gtk_get_current_event_time ();
    }

  gtk_menu_popup (GTK_MENU (popup_menu), NULL, NULL,
                  NULL, NULL, button, event_time);
}

void
glade_popup_palette_pop (GladePalette       *palette, 
                         GladeWidgetAdaptor *adaptor, 
                         GdkEventButton     *event)
{
  GladeProject *project;
  GtkWidget *popup_menu;
  gint button;
  gint event_time;
  RootAddData *data;

  g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));

  popup_menu = gtk_menu_new ();

  project = glade_palette_get_project (palette);

  data = g_new (RootAddData, 1);
  data->adaptor = adaptor;
  data->project = project;
  g_object_set_data_full (G_OBJECT (popup_menu), "root-data-destroy-me", 
                          data, (GDestroyNotify)g_free);

  glade_popup_append_item (popup_menu, _("Add widget as _toplevel"), TRUE,
                           glade_popup_root_add_cb, data);

  if (glade_widget_adaptor_get_book (adaptor) && glade_util_have_devhelp ())
    glade_popup_append_item (popup_menu, _("Read _documentation"), TRUE,
                             glade_popup_docs_cb, adaptor);

  if (event)
    {
      button = event->button;
      event_time = event->time;
    }
  else
    {
      button = 0;
      event_time = gtk_get_current_event_time ();
    }

  gtk_menu_popup (GTK_MENU (popup_menu), NULL, NULL,
                  NULL, NULL, button, event_time);
}

static void
glade_popup_clear_property_cb (GtkMenuItem *item, GladeProperty *property)
{
  GValue value = { 0, };

  glade_property_get_default (property, &value);
  glade_command_set_property_value (property, &value);
  g_value_unset (&value);
}

static void
glade_popup_property_docs_cb (GtkMenuItem *item, GladeProperty *property)
{
  GladeWidgetAdaptor *adaptor, *prop_adaptor;
  GladePropertyDef   *pdef;
  GParamSpec         *pspec;
  gchar              *search;

  pdef         = glade_property_get_def (property);
  pspec        = glade_property_def_get_pspec (pdef);
  prop_adaptor = glade_property_def_get_adaptor (pdef);
  adaptor      = glade_widget_adaptor_from_pspec (prop_adaptor, pspec);
  search       = g_strdup_printf ("The \"%s\" property", glade_property_def_id (pdef));

  glade_app_search_docs (glade_widget_adaptor_get_book (adaptor),
                         g_type_name (pspec->owner_type), search);

  g_free (search);
}

void
glade_popup_property_pop (GladeProperty *property, GdkEventButton *event)
{

  GladeWidgetAdaptor *adaptor, *prop_adaptor;
  GladePropertyDef   *pdef;
  GParamSpec         *pspec;
  GtkWidget *popup_menu;
  gint button;
  gint event_time;

  pdef         = glade_property_get_def (property);
  pspec        = glade_property_def_get_pspec (pdef);
  prop_adaptor = glade_property_def_get_adaptor (pdef);
  adaptor      = glade_widget_adaptor_from_pspec (prop_adaptor, pspec);

  g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));

  popup_menu = gtk_menu_new ();

  glade_popup_append_item (popup_menu, _("Set default value"), TRUE,
                           glade_popup_clear_property_cb, property);

  if (!glade_property_def_get_virtual (pdef) &&
      glade_widget_adaptor_get_book (adaptor) &&
      glade_util_have_devhelp ())
    {
      glade_popup_append_item (popup_menu, _("Read _documentation"), TRUE,
                               glade_popup_property_docs_cb, property);
    }

  if (event)
    {
      button = event->button;
      event_time = event->time;
    }
  else
    {
      button = 0;
      event_time = gtk_get_current_event_time ();
    }

  gtk_menu_popup (GTK_MENU (popup_menu), NULL, NULL,
                  NULL, NULL, button, event_time);
}

void
glade_popup_simple_pop (GladeProject *project, GdkEventButton *event)
{
  GtkWidget *popup_menu;
  gint button;
  gint event_time;

  popup_menu = glade_popup_create_menu (NULL, NULL, project, FALSE);
  if (!popup_menu)
    return;

  if (event)
    {
      button = event->button;
      event_time = event->time;
    }
  else
    {
      button = 0;
      event_time = gtk_get_current_event_time ();
    }
  gtk_menu_popup (GTK_MENU (popup_menu), NULL, NULL,
                  NULL, NULL, button, event_time);
}

gboolean
glade_popup_is_popup_event (GdkEventButton *event)
{
  g_return_val_if_fail (event, FALSE);

#ifdef __APPLE__
  return (event->type == GDK_BUTTON_PRESS && event->button == 1 &&
          ((event->state & GDK_MOD1_MASK) != 0));
#else
  return (event->type == GDK_BUTTON_PRESS && event->button == 3);
#endif
}
