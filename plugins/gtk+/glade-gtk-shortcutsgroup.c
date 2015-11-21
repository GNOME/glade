/*
 * glade-gtk-shortcutsgroup.c - GladeWidgetAdaptor for GtkShortcutsGroup
 *
 * Copyright (C) 2015 Red Hat, Inc
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
#include "glade-gtk.h"
#include "glade-fixed.h"
#include "glade-shortcutsgroup-editor.h"

static gboolean
glade_gtk_shortcutsgroup_configure_child (GladeFixed   *fixed,
                                          GladeWidget  *child,
                                          GdkRectangle *rect,
                                          GtkWidget    *group)
{
  return TRUE;
}

void
glade_gtk_shortcutsgroup_post_create (GladeWidgetAdaptor *adaptor,
                                      GObject            *container,
                                      GladeCreateReason   reason)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (container);

  g_signal_connect (G_OBJECT (gwidget), "configure-child",
                    G_CALLBACK (glade_gtk_shortcutsgroup_configure_child), container);

  if (reason == GLADE_CREATE_USER)
    {
      GladeWidgetAdaptor *shortcut_adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_SHORTCUTS_SHORTCUT);
      GladeWidget *shortcut;

      shortcut = glade_widget_adaptor_create_widget (shortcut_adaptor, FALSE,
                                                    "parent", gwidget,
                                                    "project", glade_widget_get_project (gwidget),
                                                   NULL);
      glade_widget_add_child (gwidget, shortcut, FALSE);
    }
}

gboolean
glade_gtk_shortcutsgroup_add_verify (GladeWidgetAdaptor *adaptor,
                              GtkWidget          *container,
                              GtkWidget          *child,
                              gboolean            user_feedback)
{
  if (!GTK_IS_SHORTCUTS_SHORTCUT (child))
    {
      if (user_feedback)
        {
          GladeWidgetAdaptor *tool_item_adaptor =
            glade_widget_adaptor_get_by_type (GTK_TYPE_SHORTCUTS_SHORTCUT);

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

static void
add_new_shortcut (GObject *object)
{
  GladeWidgetAdaptor *adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_SHORTCUTS_SHORTCUT);
  GladeWidget *gparent = glade_widget_get_from_gobject (object);
  GladeProject *project = glade_widget_get_project (gparent);

  glade_command_push_group (_("Adding a shortcut to %s"), glade_widget_get_name (gparent));
  glade_command_create (adaptor, gparent, NULL, project);
  glade_project_selection_set (project, object, TRUE);
  glade_command_pop_group ();
}

GladeEditable *
glade_gtk_shortcutsgroup_create_editable (GladeWidgetAdaptor * adaptor,
                                           GladeEditorPageType  type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_shortcutsgroup_editor_new ();

  return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}

typedef struct {
  gint size;
} ChildData;

static void
count_child (GtkWidget *child, gpointer data)
{
  ChildData *cdata = data;

  cdata->size++;
}

static gint
glade_gtk_shortcutsgroup_get_n_shortcuts (GObject *object)
{
  ChildData data;

  data.size = 0;
  gtk_container_foreach (GTK_CONTAINER (object), count_child, &data);
  return data.size;
}

static void
glade_gtk_shortcutsgroup_set_n_shortcuts (GObject *object,
                                          const GValue *value)
{
  gint old_size;
  gint new_size;
  gint i;
  GList *children, *l, *list;

  new_size = g_value_get_int (value);
  old_size = glade_gtk_shortcutsgroup_get_n_shortcuts (object);

  if (old_size == new_size)
    return;

  for (i = old_size; i < new_size; i++)
    add_new_shortcut (object);

  list = NULL;
  children = gtk_container_get_children (GTK_CONTAINER (object));
  for (i = old_size, l = g_list_last (children); i > 0; i--, l = l->prev)
    {
      if (old_size <= new_size)
        break;

      list = g_list_append (list, glade_widget_get_from_gobject (G_OBJECT (l->data)));
    }
  g_list_free (children);

  if (list)
    {
      glade_command_delete (list);
      g_list_free (list);
    }
}

void
glade_gtk_shortcutsgroup_set_property (GladeWidgetAdaptor * adaptor,
                              GObject * object,
                              const gchar * id,
                              const GValue * value)
{
  if (!strcmp (id, "shortcuts"))
    glade_gtk_shortcutsgroup_set_n_shortcuts (object, value);
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id, value);
}

void
glade_gtk_shortcutsgroup_get_property (GladeWidgetAdaptor * adaptor,
                              GObject * object,
                              const gchar * id,
                              GValue * value)
{
  if (!strcmp (id, "shortcuts"))
    {
      g_value_reset (value);
      g_value_set_int (value, glade_gtk_shortcutsgroup_get_n_shortcuts (object));
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->get_property (adaptor, object, id, value);
}

void
glade_gtk_shortcutsgroup_action_activate (GladeWidgetAdaptor * adaptor,
                                            GObject * object,
                                            const gchar * action_path)
{
  if (strcmp (action_path, "add_shortcut") == 0)
    {
      GladeWidget *gwidget = glade_widget_get_from_gobject (object);
      GladeProperty *property;
      gint shortcuts;

      shortcuts = glade_gtk_shortcutsgroup_get_n_shortcuts (object);
      property = glade_widget_get_property (gwidget, "shortcuts");
      glade_command_set_property (property, shortcuts + 1);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->action_activate (adaptor,
                                                         object, action_path);
}

void
glade_gtk_shortcutsgroup_add_child (GladeWidgetAdaptor * adaptor,
                                      GObject * object,
                                      GObject * child)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);
  GladeProperty *property;
  gint shortcuts;

  gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));

  shortcuts = glade_gtk_shortcutsgroup_get_n_shortcuts (object);
  property = glade_widget_get_property (gwidget, "shortcuts");
  glade_command_set_property (property, shortcuts);
}

void
glade_gtk_shortcutsgroup_remove_child (GladeWidgetAdaptor * adaptor,
                                         GObject * object,
                                         GObject * child)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);
  GladeProperty *property;
  gint shortcuts;

  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));

  shortcuts = glade_gtk_shortcutsgroup_get_n_shortcuts (object);
  property = glade_widget_get_property (gwidget, "shortcuts");
  glade_command_set_property (property, shortcuts);
}

void
glade_gtk_shortcutsgroup_replace_child (GladeWidgetAdaptor * adaptor,
                                          GObject * container,
                                          GObject * current,
                                          GObject * new_widget)
{
  GWA_GET_CLASS (GTK_TYPE_CONTAINER)->replace_child (adaptor, container, current, new_widget);
}
