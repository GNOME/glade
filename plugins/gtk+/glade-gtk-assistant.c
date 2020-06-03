/*
 * glade-gtk-assistant.c - GladeWidgetAdaptor for GtkAssistant
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

static void
glade_gtk_assistant_append_new_page (GladeWidget         *parent,
                                     GladeProject        *project,
                                     const gchar         *label,
                                     GtkAssistantPageType type)
{
  static GladeWidgetAdaptor *adaptor = NULL;
  GladeWidget *page;

  if (adaptor == NULL)
    adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_LABEL);

  page = glade_widget_adaptor_create_widget (adaptor, FALSE,
                                             "parent", parent,
                                             "project", project, NULL);

  glade_widget_add_child (parent, page, FALSE);

  glade_widget_property_set (page, "label", label);
  glade_widget_pack_property_set (page, "page-type", type);
}

/*
  GtkAssistant is a very weird widget, why is it derived from GtkWindow
  instead of GtkNotebook I do not know!

  If there is no GTK_ASSISTANT_PAGE_CONFIRM, GtkAssistant abort when trying to 
  update its navigation buttons!
*/
static void
glade_gtk_assistant_update_page_type (GtkAssistant *assistant)
{
  gint i, current, pages;
  GtkWidget *page;

  current = gtk_assistant_get_current_page (assistant);
  pages = gtk_assistant_get_n_pages (assistant) - 1;
  if (pages < 0)
    return;

  /* Last Page */
  page = gtk_assistant_get_nth_page (assistant, pages);
  gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONFIRM);

  /* First page */
  page = gtk_assistant_get_nth_page (assistant, 0);
  gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_INTRO);

  /* In betwen pages */
  for (i = 1; i < pages; i++)
    {
      page = gtk_assistant_get_nth_page (assistant, i);
      gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONTENT);
    }

  /* Now we have set page-type in every page, force button update */
  for (i = 0; i <= pages; i++)
    {
      page = gtk_assistant_get_nth_page (assistant, i);
      gtk_assistant_set_page_complete (assistant, page, TRUE);
    }

  if (current >= 0)
    gtk_assistant_set_current_page (assistant, current);
}

static gint
glade_gtk_assistant_get_page (GtkAssistant *assistant, GtkWidget *page)
{
  gint i, pages = gtk_assistant_get_n_pages (assistant);

  for (i = 0; i < pages; i++)
    if (gtk_assistant_get_nth_page (assistant, i) == page)
      return i;

  return -1;
}

static void
glade_gtk_assistant_update_position (GtkAssistant *assistant)
{
  gint i, pages = gtk_assistant_get_n_pages (assistant);

  for (i = 0; i < pages; i++)
    {
      GtkWidget *page = gtk_assistant_get_nth_page (assistant, i);
      GladeWidget *gpage = glade_widget_get_from_gobject (G_OBJECT (page));
      if (gpage)
        glade_widget_pack_property_set (gpage, "position", i);
    }
}

static void
glade_gtk_assistant_parse_finished (GladeProject *project, GObject *object)
{
  GtkAssistant *assistant = GTK_ASSISTANT (object);
  gint pages = gtk_assistant_get_n_pages (assistant);

  if (pages)
    {
      /* also sets pages "complete" and thus allows navigation under glade */
      glade_gtk_assistant_update_page_type (assistant);

      gtk_assistant_set_current_page (assistant, 0);
      glade_widget_property_set (glade_widget_get_from_gobject (object),
                                 "n-pages", pages);
    }
}

G_MODULE_EXPORT GList *
glade_gtk_assistant_get_children (GladeWidgetAdaptor *adaptor,
                                  GObject            *container)
{
  GtkAssistant *assist = GTK_ASSISTANT (container);
  gint i, n_pages = gtk_assistant_get_n_pages (assist);
  GList *children = NULL, *parent_children;

  /* Chain up */
  if (GWA_GET_CLASS (GTK_TYPE_WINDOW)->get_children)
    parent_children = GWA_GET_CLASS (GTK_TYPE_WINDOW)->get_children (adaptor, container);
  else
    parent_children = NULL;
  
  for (i = 0; i < n_pages; i++)
    children = g_list_prepend (children, gtk_assistant_get_nth_page (assist, i));

  children = g_list_reverse (children);

  return glade_util_purify_list (g_list_concat (children, parent_children));
}

static void
on_assistant_project_selection_changed (GladeProject *project,
                                        GladeWidget  *gassist)
{
  GList *selection = glade_project_selection_get (project);

  if (selection && g_list_next (selection) == NULL)
    {
      GladeWidget *selected = glade_widget_get_from_gobject (selection->data);
      GtkAssistant *assist = GTK_ASSISTANT (glade_widget_get_object (gassist));
      gint pos;

      if (!selected) return;

      if (glade_widget_get_parent (selected) == gassist &&
          glade_widget_property_get (selected, "position", &pos, NULL))
        gtk_assistant_set_current_page (assist, pos);
    }
}

G_MODULE_EXPORT void
glade_gtk_assistant_post_create (GladeWidgetAdaptor *adaptor,
                                 GObject            *object,
                                 GladeCreateReason   reason)
{
  GladeWidget *parent = glade_widget_get_from_gobject (object);
  GladeProject *project = glade_widget_get_project (parent);

  if (reason == GLADE_CREATE_LOAD)
    {
      g_signal_connect (project, "parse-finished",
                        G_CALLBACK (glade_gtk_assistant_parse_finished),
                        object);
    }
  else if (reason == GLADE_CREATE_USER)
    {
      glade_gtk_assistant_append_new_page (parent, project,
                                           _("Introduction page"),
                                           GTK_ASSISTANT_PAGE_INTRO);

      glade_gtk_assistant_append_new_page (parent, project,
                                           _("Content page"),
                                           GTK_ASSISTANT_PAGE_CONTENT);

      glade_gtk_assistant_append_new_page (parent, project,
                                           _("Confirmation page"),
                                           GTK_ASSISTANT_PAGE_CONFIRM);

      gtk_assistant_set_current_page (GTK_ASSISTANT (object), 0);

      glade_widget_property_set (parent, "n-pages", 3);
    }

  if (project)
    g_signal_connect (project, "selection-changed",
                      G_CALLBACK (on_assistant_project_selection_changed),
                      parent);
}

G_MODULE_EXPORT void
glade_gtk_assistant_add_child (GladeWidgetAdaptor * adaptor,
                               GObject * container, GObject * child)
{
  GtkAssistant *assistant = GTK_ASSISTANT (container);
  GtkWidget *widget = GTK_WIDGET (child);

  gtk_assistant_append_page (assistant, widget);
}

static void
assistant_remove_child (GtkAssistant *assistant, GtkWidget *child)
{
  gint i, n = gtk_assistant_get_n_pages (assistant);

  for (i = 0; i < n; i++)
    {
      if (child == gtk_assistant_get_nth_page (assistant, i))
        {
          gtk_assistant_remove_page (assistant, i);
          return;
        }
    }
}

G_MODULE_EXPORT void
glade_gtk_assistant_remove_child (GladeWidgetAdaptor *adaptor,
                                  GObject            *container,
                                  GObject            *child)
{
  GladeWidget *gassistant = glade_widget_get_from_gobject (container);
  GtkAssistant *assistant = GTK_ASSISTANT (container);

  assistant_remove_child (assistant, GTK_WIDGET (child));

  glade_widget_property_set (gassistant, "n-pages", 
                             gtk_assistant_get_n_pages (assistant));
}

G_MODULE_EXPORT void
glade_gtk_assistant_replace_child (GladeWidgetAdaptor *adaptor,
                                   GObject            *container,
                                   GObject            *current,
                                   GObject            *new_object)
{
  GtkAssistant *assistant = GTK_ASSISTANT (container);
  GtkWidget *page = GTK_WIDGET (new_object), *old_page = GTK_WIDGET (current);
  gint pos = glade_gtk_assistant_get_page (assistant, old_page);
  gboolean set_current = gtk_assistant_get_current_page (assistant) == pos;

  assistant_remove_child (assistant, old_page);

  gtk_assistant_insert_page (assistant, page, pos);
  glade_gtk_assistant_update_page_type (assistant);

  if (set_current)
    gtk_assistant_set_current_page (assistant, pos);
}

G_MODULE_EXPORT gboolean
glade_gtk_assistant_verify_property (GladeWidgetAdaptor *adaptor,
                                     GObject            *object,
                                     const gchar        *property_name,
                                     const GValue       *value)
{
  if (strcmp (property_name, "n-pages") == 0)
    return g_value_get_int (value) >=
        gtk_assistant_get_n_pages (GTK_ASSISTANT (object));

  /* Chain Up */
  if (GWA_GET_CLASS (GTK_TYPE_WINDOW)->verify_property == NULL)
    return TRUE;
  return GWA_GET_CLASS (GTK_TYPE_WINDOW)->verify_property (adaptor,
                                                           object,
                                                           property_name,
                                                           value);
}

G_MODULE_EXPORT void
glade_gtk_assistant_set_property (GladeWidgetAdaptor *adaptor,
                                  GObject            *object,
                                  const gchar        *property_name,
                                  const GValue       *value)
{
  if (strcmp (property_name, "n-pages") == 0)
    {
      GtkAssistant *assistant = GTK_ASSISTANT (object);
      gint size, i;

      for (i = gtk_assistant_get_n_pages (GTK_ASSISTANT (object)),
           size = g_value_get_int (value); i < size; i++)
        {
          gtk_assistant_append_page (assistant, glade_placeholder_new ());
        }

      glade_gtk_assistant_update_page_type (assistant);

      return;
    }

  /* Chain Up */
  GWA_GET_CLASS (GTK_TYPE_WINDOW)->set_property (adaptor,
                                                 object, property_name, value);
}

G_MODULE_EXPORT void
glade_gtk_assistant_get_property (GladeWidgetAdaptor *adaptor,
                                  GObject            *object,
                                  const gchar        *property_name,
                                  GValue             *value)
{
  if (strcmp (property_name, "n-pages") == 0)
    {
      g_value_set_int (value,
                       gtk_assistant_get_n_pages (GTK_ASSISTANT (object)));
      return;
    }

  /* Chain Up */
  GWA_GET_CLASS (GTK_TYPE_WINDOW)->get_property (adaptor,
                                                 object, property_name, value);
}

G_MODULE_EXPORT void
glade_gtk_assistant_set_child_property (GladeWidgetAdaptor *adaptor,
                                        GObject            *container,
                                        GObject            *child,
                                        const gchar        *property_name,
                                        const GValue       *value)
{
  if (strcmp (property_name, "position") == 0)
    {
      GtkAssistant *assistant = GTK_ASSISTANT (container);
      GtkWidget *widget = GTK_WIDGET (child);
      gint pos;
      gboolean set_current;

      if ((pos = g_value_get_int (value)) < 0)
        return;
      if (pos == glade_gtk_assistant_get_page (assistant, widget))
        return;
      set_current = gtk_assistant_get_current_page (assistant) ==
          glade_gtk_assistant_get_page (assistant, widget);   

      g_object_ref (child);
      assistant_remove_child (assistant, widget);
      gtk_assistant_insert_page (assistant, widget, pos);
      g_object_unref (child);

      if (set_current)
        gtk_assistant_set_current_page (assistant, pos);

      glade_gtk_assistant_update_page_type (assistant);

      glade_gtk_assistant_update_position (assistant);

      return;
    }

  /* Chain Up */
  GWA_GET_CLASS (GTK_TYPE_WINDOW)->child_set_property (adaptor,
                                                       container,
                                                       child,
                                                       property_name, value);
}

G_MODULE_EXPORT void
glade_gtk_assistant_get_child_property (GladeWidgetAdaptor *adaptor,
                                        GObject            *container,
                                        GObject            *child,
                                        const gchar        *property_name,
                                        GValue             *value)
{
  if (strcmp (property_name, "position") == 0)
    {
      gint pos;
      pos = glade_gtk_assistant_get_page (GTK_ASSISTANT (container),
                                          GTK_WIDGET (child));
      if (pos >= 0)
        g_value_set_int (value, pos);
      return;
    }

  /* Chain Up */
  GWA_GET_CLASS (GTK_TYPE_WINDOW)->child_get_property (adaptor,
                                                       container,
                                                       child,
                                                       property_name, value);
}
