/*
 * glade-gtk-stack.c - GladeWidgetAdaptor for GtkStack
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

#include "glade-stack-editor.h"

static void
glade_gtk_stack_selection_changed (GladeProject * project,
                                   GladeWidget * gwidget)
{
  GList *list;
  GtkWidget *page, *sel_widget;
  GtkStack *stack = GTK_STACK (glade_widget_get_object (gwidget));

  if ((list = glade_project_selection_get (project)) != NULL &&
      g_list_length (list) == 1)
    {
      sel_widget = list->data;

      if (GTK_IS_WIDGET (sel_widget) &&
          gtk_widget_is_ancestor (sel_widget, GTK_WIDGET (stack)))
        {
          GList *children, *l;

          children = gtk_container_get_children (GTK_CONTAINER (stack));
          for (l = children; l; l = l->next)
            {
              page = l->data;
              if (sel_widget == page ||
                  gtk_widget_is_ancestor (sel_widget, page))
                {
                  gtk_stack_set_visible_child (stack, page);
                  break;
                }
            }
          g_list_free (children);
        }
    }
}

static void
glade_gtk_stack_project_changed (GladeWidget * gwidget,
                                 GParamSpec * pspec,
                                 gpointer userdata)
{
  GladeProject * project = glade_widget_get_project (gwidget);
  GladeProject * old_project = g_object_get_data (G_OBJECT (gwidget), "stack-project-ptr");

  if (old_project)
    g_signal_handlers_disconnect_by_func (G_OBJECT (old_project),
                                          G_CALLBACK (glade_gtk_stack_selection_changed),
                                          gwidget);

  if (project)
    g_signal_connect (G_OBJECT (project), "selection-changed",
                      G_CALLBACK (glade_gtk_stack_selection_changed),
                      gwidget);

  g_object_set_data (G_OBJECT (gwidget), "stack-project-ptr", project);
}

void
glade_gtk_stack_post_create (GladeWidgetAdaptor *adaptor,
                             GObject            *container,
                             GladeCreateReason   reason)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (container);

  if (reason == GLADE_CREATE_USER)
    gtk_stack_add_named (GTK_STACK (container),
                         glade_placeholder_new (),
                         "page0");

  g_signal_connect (G_OBJECT (gwidget), "notify::project",
                    G_CALLBACK (glade_gtk_stack_project_changed), NULL);

  glade_gtk_stack_project_changed (gwidget, NULL, NULL);

}

void
glade_gtk_stack_child_action_activate (GladeWidgetAdaptor * adaptor,
                                       GObject * container,
                                       GObject * object,
                                       const gchar * action_path)
{
  if (!strcmp (action_path, "insert_page_after") ||
      !strcmp (action_path, "insert_page_before"))
    {
      gint position;
      gchar *name;
      GtkWidget *new_widget;

      gtk_container_child_get (GTK_CONTAINER (container), GTK_WIDGET (object),
                               "position", &position, NULL);

      if (!strcmp (action_path, "insert_page_after"))
        position++;

      name = g_strdup_printf ("page%d", position);
      new_widget = glade_placeholder_new ();

      gtk_stack_add_named (GTK_STACK (container), new_widget, name);
      gtk_stack_set_visible_child (GTK_STACK (container), new_widget);

      g_free (name);
    }
  else if (strcmp (action_path, "remove_page") == 0)
    {
      gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET (object)); 
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_action_activate (adaptor,
                                                               container,
                                                               object,
                                                               action_path);
}

GladeEditable *
glade_gtk_stack_create_editable (GladeWidgetAdaptor * adaptor,
                                 GladeEditorPageType  type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_stack_editor_new ();

  return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}

