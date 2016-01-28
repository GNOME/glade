/*
 * glade-gtk-popovermenu.c - GladeWidgetAdaptor for GtkPopoverMenu
 *
 * Copyright (C) 2014 Red Hat, Inc
 *
 * Authors:
 *      Matthias Clasen <mclasen@redhat.com>
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

#include "glade-popover-menu-editor.h"

GObject *
glade_gtk_popover_menu_constructor (GType type,
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

static void
glade_gtk_popover_menu_parse_finished (GladeProject * project,
                                       GObject * object)
{
  GladeWidget *gbox;
  gint submenus;

  gbox = glade_widget_get_from_gobject (object);
  glade_widget_property_get (gbox, "submenus", &submenus);
  glade_widget_property_set (gbox, "submenus", submenus);
}

static void
glade_gtk_popover_menu_selection_changed (GladeProject * project,
                                          GladeWidget * gwidget)
{
  GList *list;
  GtkWidget *page, *sel_widget;
  GtkWidget *popover = GTK_WIDGET (glade_widget_get_object (gwidget));

  if ((list = glade_project_selection_get (project)) != NULL &&
      g_list_length (list) == 1)
    {
      sel_widget = list->data;

      if (GTK_IS_WIDGET (sel_widget) &&
          gtk_widget_is_ancestor (sel_widget, popover))
        {
          GList *children, *l;

          children = gtk_container_get_children (GTK_CONTAINER (popover));
          for (l = children; l; l = l->next)
            {
              page = l->data;
              if (sel_widget == page ||
                  gtk_widget_is_ancestor (sel_widget, page))
                {
                  gint position;
                  glade_widget_property_get (glade_widget_get_from_gobject (page), "position", &position);
                  glade_widget_property_set (glade_widget_get_from_gobject (popover), "current", position);
                  break;
                }
            }
          g_list_free (children);
        }
    }
}

static void
glade_gtk_popover_menu_project_changed (GladeWidget * gwidget,
                                        GParamSpec * pspec,
                                        gpointer userdata)
{
  GladeProject * project = glade_widget_get_project (gwidget);
  GladeProject * old_project = g_object_get_data (G_OBJECT (gwidget), "popover-menu-project-ptr");

  if (old_project)
    g_signal_handlers_disconnect_by_func (G_OBJECT (old_project),
                                          G_CALLBACK (glade_gtk_popover_menu_selection_changed),
                                          gwidget);

  if (project)
    g_signal_connect (G_OBJECT (project), "selection-changed",
                      G_CALLBACK (glade_gtk_popover_menu_selection_changed),
                      gwidget);

  g_object_set_data (G_OBJECT (gwidget), "popover-menu-project-ptr", project);
}

static gint
get_visible_child (GtkPopoverMenu *popover, GtkWidget **visible_child)
{
  gchar *visible;
  GList *children, *l;
  gint ret, i;

  ret = -1;

  g_object_get (G_OBJECT (popover), "visible-submenu", &visible, NULL);
  children = gtk_container_get_children (GTK_CONTAINER (popover));
  for (l = children, i = 0; visible && l; l = l->next, i++)
    {
      GtkWidget *child = l->data;
      gchar *name;
      gboolean found;

      gtk_container_child_get (GTK_CONTAINER (popover), child, "submenu", &name, NULL);
      found = name != NULL && !strcmp (visible, name);
      g_free (name);
      if (found)
        {
          if (visible_child)
            *visible_child = child;
          ret = i;
          break;
        }
    }
  g_list_free (children);
  g_free (visible);

  return ret;
}

static void
glade_gtk_popover_menu_visible_submenu_changed (GObject *popover,
                                                GParamSpec *pspec,
                                                gpointer data)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (popover);
  GladeProject *project = glade_widget_get_project (gwidget);
  gint current;
  GList *list;
  GtkWidget *visible_child;

  current = get_visible_child (GTK_POPOVER_MENU (popover), &visible_child);
  glade_widget_property_set (gwidget, "current", current);

  if ((list = glade_project_selection_get (project)) != NULL &&
      list->next == NULL)
    {
      GObject *selected = list->data;

      if (GTK_IS_WIDGET (selected) &&
          gtk_widget_is_ancestor (GTK_WIDGET (selected), GTK_WIDGET (popover)) &&
          (GtkWidget*)selected != visible_child &&
          !gtk_widget_is_ancestor (GTK_WIDGET (selected), GTK_WIDGET (visible_child)))
        {
          glade_project_selection_clear (project, TRUE);
        }
    }
}

void
glade_gtk_popover_menu_post_create (GladeWidgetAdaptor *adaptor,
                                    GObject *container,
                                    GladeCreateReason reason)
{
  GladeWidget *parent = glade_widget_get_from_gobject (container);
  GladeProject *project = glade_widget_get_project (parent);

  if (reason == GLADE_CREATE_LOAD)
    g_signal_connect (project, "parse-finished",
                      G_CALLBACK (glade_gtk_popover_menu_parse_finished),
                      container);

  g_signal_connect (G_OBJECT (parent), "notify::project",
                    G_CALLBACK (glade_gtk_popover_menu_project_changed), NULL);

  glade_gtk_popover_menu_project_changed (parent, NULL, NULL);

  g_signal_connect (container, "notify::visible-submenu",
                    G_CALLBACK (glade_gtk_popover_menu_visible_submenu_changed), NULL);

  GWA_GET_CLASS (GTK_TYPE_POPOVER)->post_create (adaptor, container, reason);
}

void
glade_gtk_popover_menu_add_child (GladeWidgetAdaptor *adaptor,
                                  GObject *parent,
                                  GObject *child)
{
  gtk_container_add (GTK_CONTAINER (parent), GTK_WIDGET (child));

  if (!glade_widget_superuser ())
    {
      GladeWidget *gbox;
      gint submenus;

      gbox = glade_widget_get_from_gobject (parent);

      glade_widget_property_get (gbox, "submenus", &submenus);
      glade_widget_property_set (gbox, "submenus", submenus);
    }
}

void
glade_gtk_popover_menu_remove_child (GladeWidgetAdaptor *adaptor,
                                     GObject *parent,
                                     GObject *child)
{
  gtk_container_remove (GTK_CONTAINER (parent), GTK_WIDGET (child));

  if (!glade_widget_superuser ())
    {
      GladeWidget *gbox;
      gint submenus;

      gbox = glade_widget_get_from_gobject (parent);

      glade_widget_property_get (gbox, "submenus", &submenus);
      glade_widget_property_set (gbox, "submenus", submenus);
    }
}

void
glade_gtk_popover_menu_replace_child (GladeWidgetAdaptor * adaptor,
                                      GObject * container,
                                      GObject * current,
                                      GObject * new_widget)
{
  gchar *visible;
  gchar *name;
  gint position;
  GladeWidget *gwidget;

  g_object_get (G_OBJECT (container), "visible-submenu", &visible, NULL);

  gtk_container_child_get (GTK_CONTAINER (container),
                           GTK_WIDGET (current),
                           "submenu", &name,
                           "position", &position,
                           NULL);

  gtk_container_add (GTK_CONTAINER (container), GTK_WIDGET (new_widget));
  gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET (current));

  gtk_container_child_set (GTK_CONTAINER (container),
                           GTK_WIDGET (new_widget),
                           "submenu", name,
                           "position", position,
                           NULL);

  g_object_set (G_OBJECT (container), "visible-submenu", visible, NULL);

  gwidget = glade_widget_get_from_gobject (new_widget);
  if (gwidget)
    {
      glade_widget_pack_property_set (gwidget, "submenu", name);
      glade_widget_pack_property_set (gwidget, "position", position);
    }

  g_free (visible);
  g_free (name);
}

typedef struct {
  gint size;
  gboolean include_placeholders;
} ChildData;

static void
count_child (GtkWidget *child, gpointer data)
{
  ChildData *cdata = data;

  if (cdata->include_placeholders || !GLADE_IS_PLACEHOLDER (child))
    cdata->size++;
}

static gint
count_children (GtkContainer *container,
                gboolean      include_placeholders)
{
  ChildData data;

  data.size = 0;
  data.include_placeholders = include_placeholders;
  gtk_container_foreach (container, count_child, &data);
  return data.size;
}

static gchar *
get_unused_name (GtkPopoverMenu *popover)
{
  gint i;
  gchar *name = NULL;
  GList *children, *l;
  gboolean exists;

  children = gtk_container_get_children (GTK_CONTAINER (popover));

  i = g_list_length (children);
  while (1)
    {
      name = g_strdup_printf ("submenu%d", i);
      exists = FALSE;
      for (l = children; l && !exists; l = l->next)
        {
          gchar *submenu;
          gtk_container_child_get (GTK_CONTAINER (popover), GTK_WIDGET (l->data),
                                   "submenu", &submenu, NULL);
          if (!strcmp (submenu, name))
            exists = TRUE;
          g_free (submenu);
        }
      if (!exists)
        break;

      g_free (name);
      i++;
    }

  g_list_free (children);

  return name;
}

static void
glade_gtk_popover_menu_set_submenus (GObject * object,
                                     const GValue * value)
{
  GladeWidget *gbox;
  GtkWidget *child;
  gint new_size, i;
  gint old_size;
  gchar *name;
  gint page;

  new_size = g_value_get_int (value);
  old_size = count_children (GTK_CONTAINER (object), TRUE);

  if (old_size == new_size)
    return;
  else if (old_size < new_size)
    {
      for (i = old_size; i < new_size; i++)
        {
          name = get_unused_name (GTK_POPOVER_MENU (object));
          child = glade_placeholder_new ();
          gtk_container_add_with_properties (GTK_CONTAINER (object), child,
                                             "submenu", name, NULL);
          g_free (name);
        }
    }
  else
    {
      GList *children, *l;

      children = gtk_container_get_children (GTK_CONTAINER (object));
      for (l = g_list_last (children); l; l = l->prev)
        {
          if (old_size <= new_size)
            break;
      
          child = l->data;
          if (GLADE_IS_PLACEHOLDER (child))
            {
              gtk_container_remove (GTK_CONTAINER (object), child);
              old_size--;
            }
        }
    }

  gbox = glade_widget_get_from_gobject (object);
  glade_widget_property_get (gbox, "current", &page);
  glade_widget_property_set (gbox, "current", page);
}

static void
glade_gtk_popover_menu_set_current (GObject *object,
                                    const GValue *value)
{
  gint new_page;
  GList *children;
  GtkWidget *child;
  gchar *submenu;

  new_page = g_value_get_int (value);
  children = gtk_container_get_children (GTK_CONTAINER (object));
  child = g_list_nth_data (children, new_page);
  if (child)
    {
      gtk_container_child_get (GTK_CONTAINER (object), child,
                               "submenu", &submenu,
                               NULL);
      gtk_popover_menu_open_submenu (GTK_POPOVER_MENU (object), submenu);
      g_free (submenu);
    }

  g_list_free (children);
}

void
glade_gtk_popover_menu_set_property (GladeWidgetAdaptor * adaptor,
                                     GObject * object,
                                     const gchar * id,
                                     const GValue * value)
{
  if (!strcmp (id, "submenus"))
    glade_gtk_popover_menu_set_submenus (object, value);
  else if (!strcmp (id, "current"))
    glade_gtk_popover_menu_set_current (object, value);
  else
    GWA_GET_CLASS (GTK_TYPE_POPOVER)->set_property (adaptor, object, id, value);
}

void
glade_gtk_popover_menu_get_property (GladeWidgetAdaptor * adaptor,
                                     GObject * object,
                                     const gchar * id,
                                     GValue * value)
{
  if (!strcmp (id, "submenus"))
    {
      g_value_reset (value);
      g_value_set_int (value, count_children (GTK_CONTAINER (object), TRUE));
    } 
  else if (!strcmp (id, "current"))
    {
      g_value_reset (value);
      g_value_set_int (value, get_visible_child (GTK_POPOVER_MENU (object), NULL));
    }
  else
    GWA_GET_CLASS (GTK_TYPE_POPOVER)->get_property (adaptor, object, id, value);
}

static gboolean
glade_gtk_popover_menu_verify_submenus (GObject * object,
                                        const GValue *value)
{
  gint new_size, old_size;

  new_size = g_value_get_int (value);
  old_size = count_children (GTK_CONTAINER (object), FALSE);

  return old_size <= new_size;
}

static gboolean
glade_gtk_popover_menu_verify_current (GObject *object,
                                       const GValue *value)
{
  gint current;
  gint submenus;

  current = g_value_get_int (value);
  submenus = count_children (GTK_CONTAINER (object), TRUE);

  return 0 <= current && current < submenus;
}

gboolean
glade_gtk_popover_menu_verify_property (GladeWidgetAdaptor * adaptor,
                                        GObject * object,
                                        const gchar * id,
                                        const GValue * value)
{
  if (!strcmp (id, "submenus"))
    return glade_gtk_popover_menu_verify_submenus (object, value);
  else if (!strcmp (id, "current"))
    return glade_gtk_popover_menu_verify_current (object, value);
  else if (GWA_GET_CLASS (GTK_TYPE_POPOVER)->verify_property)
    return GWA_GET_CLASS (GTK_TYPE_POPOVER)->verify_property (adaptor, object, id, value);

  return TRUE;
}

static void
update_position (GtkWidget *widget, gpointer data)
{
  GtkContainer *parent = data;
  GladeWidget *gwidget;
  gint position;

  gwidget = glade_widget_get_from_gobject (widget);
  if (gwidget)
    {
      gtk_container_child_get (parent, widget, "position", &position, NULL);
      glade_widget_pack_property_set (gwidget, "position", position);
    }
}

static void
glade_gtk_popover_menu_set_child_position (GObject * container,
                                           GObject * child,
                                           GValue * value)
{
  static gboolean recursion = FALSE;
  gint new_position, old_position;
  gchar *visible_child;
  GladeWidget *gbox;

  g_object_get (container, "visible-submenu", &visible_child, NULL);

  if (recursion)
    return;

  gtk_container_child_get (GTK_CONTAINER (container), GTK_WIDGET (child), "position", &old_position, NULL);
  new_position = g_value_get_int (value);

  if (old_position != new_position)
    {
      recursion = TRUE;
      gtk_container_child_set (GTK_CONTAINER (container), GTK_WIDGET (child),
                               "position", new_position,
                               NULL);
      gtk_container_forall (GTK_CONTAINER (container), update_position, container);
      recursion = FALSE;
    }

  g_object_set (container, "visible-submenu", visible_child, NULL);
  g_free (visible_child); 

  gbox = glade_widget_get_from_gobject (container);
  glade_widget_pack_property_set (gbox, "visible-submenu", get_visible_child (GTK_POPOVER_MENU (container), NULL));
}

void
glade_gtk_popover_menu_set_child_property (GladeWidgetAdaptor * adaptor,
                                           GObject * container,
                                           GObject * child,
                                           const gchar * id,
                                           GValue * value)
{
  if (!strcmp (id, "position"))
    glade_gtk_popover_menu_set_child_position (container, child, value);
  else if (!strcmp (id, "submenu"))
    gtk_container_child_set_property (GTK_CONTAINER (container),
                                      GTK_WIDGET (child), id, value);
  else    
    GWA_GET_CLASS (GTK_TYPE_POPOVER)->child_set_property (adaptor, container, child, id, value);
}

void
glade_gtk_popover_menu_get_child_property (GladeWidgetAdaptor * adaptor,
                                           GObject * container,
                                           GObject * child,
                                           const gchar * id,
                                           GValue * value)
{
  gtk_container_child_get_property (GTK_CONTAINER (container),
                                    GTK_WIDGET (child), id, value);
}

GladeEditable *
glade_gtk_popover_menu_create_editable (GladeWidgetAdaptor * adaptor,
                                        GladeEditorPageType  type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_popover_menu_editor_new ();
  else
    return GWA_GET_CLASS (GTK_TYPE_POPOVER)->create_editable (adaptor, type);
}

