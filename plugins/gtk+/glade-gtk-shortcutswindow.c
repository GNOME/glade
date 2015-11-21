/*
 * glade-gtk-shortcutswindow.c - GladeWidgetAdaptor for GtkShortcutsWindow
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
#include "glade-shortcutswindow-editor.h"


static void
glade_gtk_shortcutswindow_selection_changed (GladeProject * project,
                                   GladeWidget * gwidget)
{
  GList *list;
  GtkWidget *child, *sel_widget;
  GtkWidget *window = GTK_WIDGET (glade_widget_get_object (gwidget));
  gint position;

  if ((list = glade_project_selection_get (project)) != NULL &&
      g_list_length (list) == 1)
    {
      sel_widget = list->data;

      if (GTK_IS_SHORTCUTS_SECTION (sel_widget) &&
          gtk_widget_is_ancestor (sel_widget, window))
        {
          GList *children, *l;

          children = gtk_container_get_children (GTK_CONTAINER (window));
          for (l = children, position = 0; l; l = l->next, position++)
            {
              child = l->data;
              if (sel_widget == child ||
                  gtk_widget_is_ancestor (sel_widget, child))
                {
                  glade_widget_property_set (gwidget, "section", position);
                  break;
                }
            }
          g_list_free (children);
        }
    }
}

static void
glade_gtk_shortcutswindow_project_changed (GladeWidget * gwidget,
                                 GParamSpec * pspec,
                                 gpointer userdata)
{
  GladeProject * project = glade_widget_get_project (gwidget);
  GladeProject * old_project = g_object_get_data (G_OBJECT (gwidget), "shortcutswindow-project-ptr");
  
  if (old_project)
    g_signal_handlers_disconnect_by_func (G_OBJECT (old_project),
                                          G_CALLBACK (glade_gtk_shortcutswindow_selection_changed),
                                          gwidget);
  
  if (project)
    g_signal_connect (G_OBJECT (project), "selection-changed",
                      G_CALLBACK (glade_gtk_shortcutswindow_selection_changed),
                      gwidget);
  
  g_object_set_data (G_OBJECT (gwidget), "shortcutswindow-project-ptr", project);
}

void
glade_gtk_shortcutswindow_post_create (GladeWidgetAdaptor *adaptor,
                             GObject            *container,
                             GladeCreateReason   reason)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (container);

  if (reason == GLADE_CREATE_USER)
    {
      GladeWidgetAdaptor *section_adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_SHORTCUTS_SECTION);
      GladeWidget *section;

      section = glade_widget_adaptor_create_widget (section_adaptor, FALSE,
                                                    "parent", gwidget,
                                                    "project", glade_widget_get_project (gwidget),
                                                   NULL);
      glade_widget_property_set (section, "title", "Shortcuts");
      glade_widget_property_set (section, "section-name", "shortcuts");
      glade_widget_add_child (gwidget, section, FALSE);
    }

  g_signal_connect (G_OBJECT (gwidget), "notify::project",
                    G_CALLBACK (glade_gtk_shortcutswindow_project_changed), NULL);

  glade_gtk_shortcutswindow_project_changed (gwidget, NULL, NULL);
}

gboolean
glade_gtk_shortcutswindow_add_verify (GladeWidgetAdaptor *adaptor,
                              GtkWidget          *container,
                              GtkWidget          *child,
                              gboolean            user_feedback)
{
  if (!GTK_IS_SHORTCUTS_SECTION (child))
    {
      if (user_feedback)
        {
          GladeWidgetAdaptor *tool_item_adaptor =
            glade_widget_adaptor_get_by_type (GTK_TYPE_SHORTCUTS_SECTION);

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

static gchar *
get_unused_name (GObject *object)
{
  GList *children, *l;
  gchar *name;
  gchar *n;
  gint i;
  gboolean found;

  children = gtk_container_get_children (GTK_CONTAINER (object));

  for (i = 1; i < 100; i++)
    {
      name = g_strdup_printf ("shortcuts%d", i);
      found = FALSE;
      for (l = children; l && !found; l = l->next)
        {
          g_object_get (l->data, "section-name", &n, NULL);
          if (g_strcmp0 (n, name) == 0)
            found = TRUE;
          g_free (n);
        }
      if (!found)
        break;
      g_free (name);
      name = NULL;
    }
  g_list_free (children);

  return name;
}

static void
add_new_section (GObject *object)
{
  GladeWidgetAdaptor *section_adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_SHORTCUTS_SECTION);
  GladeWidget *gparent = glade_widget_get_from_gobject (object);
  GladeProject *project = glade_widget_get_project (gparent);
  GladeWidget *section;

  section = glade_command_create (section_adaptor, gparent, NULL, project);
  glade_widget_property_set (section, "title", "Shortcuts");
  glade_widget_property_set (section, "section-name", get_unused_name (object));
}

GladeEditable *
glade_gtk_shortcutswindow_create_editable (GladeWidgetAdaptor * adaptor,
                                           GladeEditorPageType  type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_shortcutswindow_editor_new ();

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
glade_gtk_shortcutswindow_get_n_sections (GObject *object)
{
  ChildData data;

  data.size = 0;
  gtk_container_foreach (GTK_CONTAINER (object), count_child, &data);
  return data.size;
}

static void
glade_gtk_shortcutswindow_set_n_sections (GObject *object,
                                          gint     new_size)
{
  gint old_size, i;

  old_size = glade_gtk_shortcutswindow_get_n_sections (object);

  if (old_size == new_size)
    return;

  for (i = old_size; i < new_size; i++)
    add_new_section (object);

  /* We never remove children here, they have to be explicitly deleted */
}

static void
glade_gtk_shortcutswindow_set_section (GObject *object,
                                       const GValue *value)
{
  gint new_section;
  GList *children;
  GtkWidget *child;

  new_section = g_value_get_int (value);
  children = gtk_container_get_children (GTK_CONTAINER (object));
  child = g_list_nth_data (children, new_section);

  if (child)
    {
      gchar *n;

      g_object_get (child, "section-name", &n, NULL);
      g_object_set (object, "section-name", n, NULL);
      g_free (n);
    }

  g_list_free (children);
}

static gint
glade_gtk_shortcutswindow_get_section (GObject *object)
{
  gchar *name, *n;
  gint section;
  gboolean found;
  GList *children, *l;

  g_object_get (object, "section-name", &name, NULL);
  children = gtk_container_get_children (GTK_CONTAINER (object));
  found = FALSE;
  for (l = children, section = 0; l && !found; l = l->next, section++)
    {
      g_object_get (l->data, "section-name", &n, NULL);
      found = strcmp (name, n) == 0;
      g_free (n);
    }
  g_list_free (children);
  g_free (name);

  return section;
}

void
glade_gtk_shortcutswindow_set_property (GladeWidgetAdaptor * adaptor,
                              GObject * object,
                              const gchar * id,
                              const GValue * value)
{
  if (!strcmp (id, "section"))
    glade_gtk_shortcutswindow_set_section (object, value);
  else
    GWA_GET_CLASS (GTK_TYPE_WINDOW)->set_property (adaptor, object, id, value);
}

static gboolean
glade_gtk_shortcutswindow_verify_section (GObject *object, const GValue *value)
{
  GList *children;
  gint section, size;

  section = g_value_get_int (value);

  children = gtk_container_get_children (GTK_CONTAINER (object));
  size = g_list_length (children);
  g_list_free (children);

  return 0 <= section && section < size;
}

gboolean
glade_gtk_shortcutswindow_verify_property (GladeWidgetAdaptor * adaptor,
                               GObject * object,
                               const gchar * id, const GValue * value)
{
  if (!strcmp (id, "section"))
    return glade_gtk_shortcutswindow_verify_section (object, value);
  else if (GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property)
    return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property (adaptor, object,
                                                                id, value);

  return TRUE;
}


void
glade_gtk_shortcutswindow_get_property (GladeWidgetAdaptor * adaptor,
                              GObject * object,
                              const gchar * id,
                              GValue * value)
{
  if (!strcmp (id, "section"))
    {
      g_value_reset (value);
      g_value_set_int (value, glade_gtk_shortcutswindow_get_section (object));
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->get_property (adaptor, object, id, value);
}

void
glade_gtk_shortcutswindow_action_activate (GladeWidgetAdaptor * adaptor,
                                           GObject * object,
                                           const gchar * action_path)
{
  if (strcmp (action_path, "add_section") == 0)
    {
      gint sections;

      sections = glade_gtk_shortcutswindow_get_n_sections (object);
      glade_gtk_shortcutswindow_set_n_sections (object, sections + 1);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->action_activate (adaptor,
                                                         object, action_path);
}

void
glade_gtk_shortcutswindow_add_child (GladeWidgetAdaptor * adaptor,
                                     GObject * object,
                                     GObject * child)
{
  gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));
}

void
glade_gtk_shortcutswindow_remove_child (GladeWidgetAdaptor * adaptor,
                                        GObject * object,
                                        GObject * child)
{
  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
}

void
glade_gtk_shortcutswindow_replace_child (GladeWidgetAdaptor * adaptor,
                                         GObject * container,
                                         GObject * current,
                                         GObject * new_widget)
{
  GWA_GET_CLASS (GTK_TYPE_CONTAINER)->replace_child (adaptor, container, current, new_widget);
}
