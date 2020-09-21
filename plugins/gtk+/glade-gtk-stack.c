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
  gint position;

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
                  gtk_container_child_get (GTK_CONTAINER (stack), page, "position", &position, NULL);
                  glade_widget_property_set (gwidget, "page", position);
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
    gtk_stack_add_titled (GTK_STACK (container), glade_placeholder_new (),
                          "page0", "page0");

  g_signal_connect (G_OBJECT (gwidget), "notify::project",
                    G_CALLBACK (glade_gtk_stack_project_changed), NULL);

  glade_gtk_stack_project_changed (gwidget, NULL, NULL);

}

static gchar *
get_unused_name (GtkStack *stack)
{
  gchar *name;
  gint i;

  i = 0;
  while (TRUE)
    {
      name = g_strdup_printf ("page%d", i);
      if (gtk_stack_get_child_by_name (stack, name) == NULL)
        return name;
      g_free (name);
      i++;
    }

  return NULL;
}

static void
update_position_with_command (GtkWidget *widget, gpointer data)
{
  GtkContainer *parent = data;
  GladeWidget *gwidget;
  GladeProperty *property;
  gint position;

  gwidget = glade_widget_get_from_gobject (widget);
  if (!gwidget)
    return;

  property = glade_widget_get_pack_property (gwidget, "position");
  gtk_container_child_get (parent, widget, "position", &position, NULL);
  glade_command_set_property (property, position);
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
      GladeWidget *parent;
      GladeProperty *property;
      gint position;
      gchar *name;
      GtkWidget *new_widget;
      gint pages;

      parent = glade_widget_get_from_gobject (container);
      glade_widget_property_get (parent, "pages", &pages);

      glade_command_push_group (_("Insert placeholder to %s"), glade_widget_get_name (parent));

      gtk_container_child_get (GTK_CONTAINER (container), GTK_WIDGET (object),
                               "position", &position, NULL);

      if (!strcmp (action_path, "insert_page_after"))
        position++;

      name = get_unused_name (GTK_STACK (container));
      new_widget = glade_placeholder_new ();
      gtk_stack_add_titled (GTK_STACK (container), new_widget, name, name);
      gtk_container_child_set (GTK_CONTAINER (container), new_widget,
                               "position", position, NULL);
      gtk_stack_set_visible_child (GTK_STACK (container), new_widget);

      property = glade_widget_get_property (parent, "pages");
      glade_command_set_property (property, pages + 1);

      gtk_container_forall (GTK_CONTAINER (container), update_position_with_command, container);

      property = glade_widget_get_property (parent, "page");
      glade_command_set_property (property, position);

      glade_command_pop_group ();

      g_free (name);
    }
  else if (strcmp (action_path, "remove_page") == 0)
    {
      GladeWidget *parent;
      GladeProperty *property;
      gint pages;
      gint position;

      parent = glade_widget_get_from_gobject (container);
      glade_widget_property_get (parent, "pages", &pages);

      glade_command_push_group (_("Remove placeholder from %s"), glade_widget_get_name (parent));
      g_assert (GLADE_IS_PLACEHOLDER (object));
      gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET (object));

      property = glade_widget_get_property (parent, "pages");
      glade_command_set_property (property, pages - 1);

      gtk_container_forall (GTK_CONTAINER (container), update_position_with_command, container);

      glade_widget_property_get (parent, "page", &position);
      property = glade_widget_get_property (parent, "page");
      glade_command_set_property (property, position);

      glade_command_pop_group ();
    }
  else
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->child_action_activate (adaptor,
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

  return GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
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
gtk_stack_get_n_pages (GtkStack *stack,
                       gboolean  include_placeholders)
{
  ChildData data;

  data.size = 0;
  data.include_placeholders = include_placeholders;
  gtk_container_forall (GTK_CONTAINER (stack), count_child, &data);
  return data.size;
}

static GtkWidget *
gtk_stack_get_nth_child (GtkStack *stack,
                         gint      n)
{
  GList *children;
  GtkWidget *child;

  children = gtk_container_get_children (GTK_CONTAINER (stack));
  child = g_list_nth_data (children, n);
  g_list_free (children);

  return child;
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
glade_gtk_stack_set_n_pages (GObject * object,
                             const GValue * value)
{
  GladeWidget *gbox;
  GtkStack *stack;
  GtkWidget *child;
  gint new_size, i;
  gint old_size;
  gchar *name;
  gint page;

  stack = GTK_STACK (object);

  new_size = g_value_get_int (value);
  old_size = gtk_stack_get_n_pages (stack, TRUE);

  if (old_size == new_size)
    return;

  for (i = old_size; i < new_size; i++)
    {
      name = get_unused_name (stack);
      child = glade_placeholder_new ();
      gtk_stack_add_titled (stack, child, name, name);
      g_free (name);
    }

  for (i = old_size; i > 0; i--)
    {
      if (old_size <= new_size)
        break;
      child = gtk_stack_get_nth_child (stack, i - 1);
      if (GLADE_IS_PLACEHOLDER (child))
        {
          gtk_container_remove (GTK_CONTAINER (stack), child);
          old_size--;
        }
    }

  gtk_container_forall (GTK_CONTAINER (stack), update_position, stack);

  gbox = glade_widget_get_from_gobject (stack);
  glade_widget_property_get (gbox, "page", &page);
  glade_widget_property_set (gbox, "page", page);
}

static void
glade_gtk_stack_set_page (GObject *object,
                          const GValue *value)
{
  gint new_page;
  GList *children;
  GtkWidget *child;

  new_page = g_value_get_int (value);
  children = gtk_container_get_children (GTK_CONTAINER (object));
  child = g_list_nth_data (children, new_page);

  if (child)
    gtk_stack_set_visible_child (GTK_STACK (object), child);

  g_list_free (children);
}

static gint
gtk_stack_get_page (GtkStack *stack)
{
  GtkWidget *child;
  gint page;

  child = gtk_stack_get_visible_child (stack);
  gtk_container_child_get (GTK_CONTAINER (stack), child, "position", &page, NULL);
  return page;
}

void
glade_gtk_stack_set_property (GladeWidgetAdaptor * adaptor,
                              GObject * object,
                              const gchar * id,
                              const GValue * value)
{
  if (!strcmp (id, "pages"))
    glade_gtk_stack_set_n_pages (object, value);
  else if (!strcmp (id, "page"))
    glade_gtk_stack_set_page (object, value);
  else
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id, value);
}

void
glade_gtk_stack_get_property (GladeWidgetAdaptor * adaptor,
                              GObject * object,
                              const gchar * id,
                              GValue * value)
{
  if (!strcmp (id, "pages"))
    {
      g_value_reset (value);
      g_value_set_int (value, gtk_stack_get_n_pages (GTK_STACK (object), TRUE));
    }
  else if (!strcmp (id, "page"))
    {
      g_value_reset (value);
      g_value_set_int (value, gtk_stack_get_page (GTK_STACK (object)));
    }
  else
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->get_property (adaptor, object, id, value);
}

static void
glade_gtk_stack_set_child_position (GObject * container,
                                    GObject * child,
                                    const GValue * value)
{
  static gboolean recursion = FALSE;
  gint new_position, old_position;
  GladeWidget *gbox;
  gint page;

  if (recursion)
    return;

  gtk_container_child_get (GTK_CONTAINER (container), GTK_WIDGET (child), "position", &old_position, NULL);
  new_position = g_value_get_int (value);

  if (old_position == new_position)
    return;

  recursion = TRUE;
  gtk_container_child_set (GTK_CONTAINER (container), GTK_WIDGET (child),
                           "position", new_position,
                           NULL);
  gtk_container_forall (GTK_CONTAINER (container), update_position, container);
  recursion = FALSE;

  gbox = glade_widget_get_from_gobject (container);
  glade_widget_property_get (gbox, "page", &page);
  glade_widget_property_set (gbox, "page", page);
}

void
glade_gtk_stack_set_child_property (GladeWidgetAdaptor * adaptor,
                                    GObject * container,
                                    GObject * child,
                                    const gchar * id,
                                    GValue * value)
{
  if (!strcmp (id, "position"))
    glade_gtk_stack_set_child_position (container, child, value);
  else
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->child_set_property (adaptor, container, child, id, value);
}

static gboolean
glade_gtk_stack_verify_n_pages (GObject * object,
                                const GValue *value)
{
  gint new_size, old_size;

  new_size = g_value_get_int (value);
  old_size = gtk_stack_get_n_pages (GTK_STACK (object), FALSE);

  return old_size <= new_size;
}

static gboolean
glade_gtk_stack_verify_page (GObject *object,
                             const GValue *value)
{
  gint page;
  gint pages;

  page = g_value_get_int (value);
  pages = gtk_stack_get_n_pages (GTK_STACK (object), TRUE);

  return 0 <= page && page < pages;
}

gboolean
glade_gtk_stack_verify_property (GladeWidgetAdaptor * adaptor,
                                 GObject * object,
                                 const gchar * id,
                                 const GValue * value)
{
  if (!strcmp (id, "pages"))
    return glade_gtk_stack_verify_n_pages (object, value);
  else if (!strcmp (id, "page"))
    return glade_gtk_stack_verify_page (object, value);
  else if (GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->verify_property)
    return GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->verify_property (adaptor, object, id, value);

  return TRUE;
}

void
glade_gtk_stack_add_child (GladeWidgetAdaptor * adaptor,
                           GObject * object,
                           GObject * child)
{
  GladeWidget *gbox, *gchild;
  gint pages, page;

  if (!glade_widget_superuser () && !GLADE_IS_PLACEHOLDER (child))
    {
      GList *l, *children;

      children = gtk_container_get_children (GTK_CONTAINER (object));

      for (l = g_list_last (children); l; l = l->prev)
        {
          GtkWidget *widget = l->data;
          if (GLADE_IS_PLACEHOLDER (widget))
            {
              gtk_container_remove (GTK_CONTAINER (object), widget);
              break;
            }
        }
      g_list_free (children);
    }

  gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));

  gchild = glade_widget_get_from_gobject (child);
  if (gchild != NULL)
    glade_widget_set_pack_action_visible (gchild, "remove_page", FALSE);

  gbox = glade_widget_get_from_gobject (object);
  glade_widget_property_get (gbox, "pages", &pages);
  glade_widget_property_set (gbox, "pages", pages);
  glade_widget_property_get (gbox, "page", &page);
  glade_widget_property_set (gbox, "page", page);
}

void
glade_gtk_stack_remove_child (GladeWidgetAdaptor * adaptor,
                              GObject * object,
                              GObject * child)
{
  GladeWidget *gbox;
  gint pages, page;

  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));

  gbox = glade_widget_get_from_gobject (object);
  glade_widget_property_get (gbox, "pages", &pages);
  glade_widget_property_set (gbox, "pages", pages);
  glade_widget_property_get (gbox, "page", &page);
  glade_widget_property_set (gbox, "page", page);
}

void
glade_gtk_stack_replace_child (GladeWidgetAdaptor * adaptor,
                               GObject * container,
                               GObject * current,
                               GObject * new_widget)
{
  GladeWidget *gchild;
  GladeWidget *gbox;
  gint pages, page;

  GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->replace_child (adaptor,
                                                     container,
                                                     current,
                                                     new_widget);

  gbox = glade_widget_get_from_gobject (container);

  gchild = glade_widget_get_from_gobject (new_widget);
  if (gchild != NULL)
    glade_widget_set_pack_action_visible (gchild, "remove_page", FALSE);

  /* NOTE: make sure to sync this at the end because new_widget could be
   * a placeholder and syncing these properties could destroy it.
   */
  glade_widget_property_get (gbox, "pages", &pages);
  glade_widget_property_set (gbox, "pages", pages);
  glade_widget_property_get (gbox, "page", &page);
  glade_widget_property_set (gbox, "page", page);
}
