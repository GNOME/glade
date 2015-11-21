/*
 * glade-gtk-shortcutssection.c - GladeWidgetAdaptor for GtkShortcutsSection
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
#include "glade-shortcutssection-editor.h"

void
glade_gtk_shortcutssection_post_create (GladeWidgetAdaptor *adaptor,
                             GObject            *container,
                             GladeCreateReason   reason)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (container);

  if (reason == GLADE_CREATE_USER)
    {
      GladeWidgetAdaptor *group_adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_SHORTCUTS_GROUP);
      GladeWidget *group;

      group = glade_widget_adaptor_create_widget (group_adaptor, FALSE,
                                                    "parent", gwidget,
                                                    "project", glade_widget_get_project (gwidget),
                                                   NULL);
      glade_widget_property_set (group, "title", "Group");
      glade_widget_add_child (gwidget, group, FALSE);
    }
}

gboolean
glade_gtk_shortcutssection_add_verify (GladeWidgetAdaptor *adaptor,
                              GtkWidget          *container,
                              GtkWidget          *child,
                              gboolean            user_feedback)
{
  if (!GTK_IS_SHORTCUTS_GROUP (child))
    {
      if (user_feedback)
        {
          GladeWidgetAdaptor *tool_item_adaptor =
            glade_widget_adaptor_get_by_type (GTK_TYPE_SHORTCUTS_GROUP);

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
add_new_group (GObject *object)
{
  GladeWidgetAdaptor *adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_SHORTCUTS_GROUP);
  GladeWidget *gparent = glade_widget_get_from_gobject (object);
  GladeWidget *gchild;
  GladeProject *project = glade_widget_get_project (gparent);
  GladeProperty *property;

  glade_command_push_group (_("Adding a group to %s"), glade_widget_get_name (gparent));
  gchild = glade_command_create (adaptor, gparent, NULL, project);
  property = glade_widget_get_property (gchild, "title");
  glade_command_set_property (property, _("Group"));
  glade_project_selection_set (project, object, TRUE);
  glade_command_pop_group ();
}

GladeEditable *
glade_gtk_shortcutssection_create_editable (GladeWidgetAdaptor * adaptor,
                                           GladeEditorPageType  type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_shortcutssection_editor_new ();

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
glade_gtk_shortcutssection_get_n_groups (GObject *object)
{
  ChildData data;

  data.size = 0;
  gtk_container_foreach (GTK_CONTAINER (object), count_child, &data);
  return data.size;
}

static void
glade_gtk_shortcutssection_set_n_groups (GObject *object,
                                         gint     new_size)
{
  gint old_size;
  gint i;

  old_size = glade_gtk_shortcutssection_get_n_groups (object);

  if (old_size == new_size)
    return;

  for (i = old_size; i < new_size; i++)
    add_new_group (object);

  /* We never remove children here, they have to be explicitly deleted */
}

static gboolean
glade_gtk_shortcutswindow_verify_groups (GObject *object,
                                         gint     new_size)
{
  GList *children;
  gint old_size;

  children = gtk_container_get_children (GTK_CONTAINER (object));
  old_size = g_list_length (children);
  g_list_free (children);

  return new_size >= old_size;
}

void
glade_gtk_shortcutssection_set_property (GladeWidgetAdaptor * adaptor,
                              GObject * object,
                              const gchar * id,
                              const GValue * value)
{
  if (!strcmp (id, "groups"))
    glade_gtk_shortcutssection_set_n_groups (object, g_value_get_int (value));
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id, value);
}

void
glade_gtk_shortcutssection_get_property (GladeWidgetAdaptor * adaptor,
                              GObject * object,
                              const gchar * id,
                              GValue * value)
{
  if (!strcmp (id, "groups"))
    {
      g_value_reset (value);
      g_value_set_int (value, glade_gtk_shortcutssection_get_n_groups (object));
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->get_property (adaptor, object, id, value);
}

gboolean
glade_gtk_shortcutssection_verify_property (GladeWidgetAdaptor * adaptor,
                               GObject * object,
                               const gchar * id, const GValue * value)
{
  if (!strcmp (id, "groups"))
    return glade_gtk_shortcutswindow_verify_groups (object, g_value_get_int (value));
  else if (GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property)
    return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property (adaptor, object,
                                                                id, value);

  return TRUE;
}

void
glade_gtk_shortcutssection_action_activate (GladeWidgetAdaptor * adaptor,
                                            GObject * object,
                                            const gchar * action_path)
{
  if (strcmp (action_path, "add_group") == 0)
    {
      GladeWidget *gwidget = glade_widget_get_from_gobject (object);
      GladeProperty *property;
      gint groups;

      groups = glade_gtk_shortcutssection_get_n_groups (object);
      property = glade_widget_get_property (gwidget, "groups");
      glade_command_set_property (property, groups + 1);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->action_activate (adaptor,
                                                         object, action_path);
}

void
glade_gtk_shortcutssection_add_child (GladeWidgetAdaptor * adaptor,
                                      GObject * object,
                                      GObject * child)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);
  GladeProperty *property;
  gint groups;

  gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));

  groups = glade_gtk_shortcutssection_get_n_groups (object);
  property = glade_widget_get_property (gwidget, "groups");
  glade_command_set_property (property, groups);
}

void
glade_gtk_shortcutssection_remove_child (GladeWidgetAdaptor * adaptor,
                                         GObject * object,
                                         GObject * child)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);
  GladeProperty *property;
  gint groups;

  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));

  groups = glade_gtk_shortcutssection_get_n_groups (object);
  property = glade_widget_get_property (gwidget, "groups");
  glade_command_set_property (property, groups);
}

void
glade_gtk_shortcutssection_replace_child (GladeWidgetAdaptor * adaptor,
                                          GObject * container,
                                          GObject * current,
                                          GObject * new_widget)
{
  GWA_GET_CLASS (GTK_TYPE_CONTAINER)->replace_child (adaptor, container, current, new_widget);
}
