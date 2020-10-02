/*
 * glade-gtk-notebook.c - GladeWidgetAdaptor for GtkNotebook
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

#include "glade-gtk-notebook.h"
#include "glade-notebook-editor.h"

typedef struct
{
  gint pages;
  gint page;

  GList *children;
  GList *tabs;

  GList *extra_children;
  GList *extra_tabs;
} NotebookChildren;

static gboolean glade_gtk_notebook_setting_position = FALSE;


GladeEditable *
glade_gtk_notebook_create_editable (GladeWidgetAdaptor *adaptor,
                                    GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_notebook_editor_new ();

  return GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}

static gint
notebook_child_compare_func (GtkWidget *widget_a, GtkWidget *widget_b)
{
  GladeWidget *gwidget_a, *gwidget_b;
  gint pos_a = 0, pos_b = 0;

  gwidget_a = glade_widget_get_from_gobject (widget_a);
  gwidget_b = glade_widget_get_from_gobject (widget_b);

  g_assert (gwidget_a && gwidget_b);

  glade_widget_pack_property_get (gwidget_a, "position", &pos_a);
  glade_widget_pack_property_get (gwidget_b, "position", &pos_b);

  return pos_a - pos_b;
}

static gint
notebook_find_child (GtkWidget *check, gpointer cmp_pos_p)
{
  GladeWidget *gcheck;
  gint position = 0, cmp_pos = GPOINTER_TO_INT (cmp_pos_p);

  gcheck = glade_widget_get_from_gobject (check);
  g_assert (gcheck);

  glade_widget_pack_property_get (gcheck, "position", &position);

  return position - cmp_pos;
}

static gint
notebook_search_tab (GtkNotebook *notebook, GtkWidget *tab)
{
  GtkWidget *page;
  gint i;

  for (i = 0; i < gtk_notebook_get_n_pages (notebook); i++)
    {
      page = gtk_notebook_get_nth_page (notebook, i);

      if (tab == gtk_notebook_get_tab_label (notebook, page))
        return i;
    }
  g_critical ("Unable to find tab position in a notebook");
  return -1;
}

static GtkWidget *
notebook_get_filler (NotebookChildren *nchildren, gboolean page)
{
  GtkWidget *widget = NULL;

  if (page && nchildren->extra_children)
    {
      widget = nchildren->extra_children->data;
      nchildren->extra_children =
          g_list_remove (nchildren->extra_children, widget);
      g_assert (widget);
    }
  else if (!page && nchildren->extra_tabs)
    {
      widget = nchildren->extra_tabs->data;
      nchildren->extra_tabs = g_list_remove (nchildren->extra_tabs, widget);
      g_assert (widget);
    }

  if (widget == NULL)
    {
      /* Need explicit reference here */
      widget = glade_placeholder_new ();

      g_object_ref (G_OBJECT (widget));

      if (!page)
        g_object_set_data (G_OBJECT (widget), "special-child-type", "tab");

    }
  return widget;
}

static GtkWidget *
notebook_get_page (NotebookChildren *nchildren, gint position)
{
  GList *node;
  GtkWidget *widget = NULL;

  if ((node = g_list_find_custom
       (nchildren->children,
        GINT_TO_POINTER (position),
        (GCompareFunc) notebook_find_child)) != NULL)
    {
      widget = node->data;
      nchildren->children = g_list_remove (nchildren->children, widget);
    }
  else
    widget = notebook_get_filler (nchildren, TRUE);

  return widget;
}

static GtkWidget *
notebook_get_tab (NotebookChildren *nchildren, gint position)
{
  GList *node;
  GtkWidget *widget = NULL;

  if ((node = g_list_find_custom
       (nchildren->tabs,
        GINT_TO_POINTER (position),
        (GCompareFunc) notebook_find_child)) != NULL)
    {
      widget = node->data;
      nchildren->tabs = g_list_remove (nchildren->tabs, widget);
    }
  else
    widget = notebook_get_filler (nchildren, FALSE);

  return widget;
}

static NotebookChildren *
glade_gtk_notebook_extract_children (GtkWidget *notebook)
{
  NotebookChildren *nchildren;
  gchar *special_child_type;
  GList *list, *children =
      glade_util_container_get_all_children (GTK_CONTAINER (notebook));
  GladeWidget *gchild;
  gint position = 0;
  GtkNotebook *nb;

  nb = GTK_NOTEBOOK (notebook);
  nchildren = g_new0 (NotebookChildren, 1);
  nchildren->pages = gtk_notebook_get_n_pages (nb);
  nchildren->page = gtk_notebook_get_current_page (nb);

  /* Ref all the project widgets and build returned list first */
  for (list = children; list; list = list->next)
    {
      if ((gchild = glade_widget_get_from_gobject (list->data)) != NULL)
        {
          special_child_type =
              g_object_get_data (G_OBJECT (list->data), "special-child-type");

          glade_widget_pack_property_get (gchild, "position", &position);

          g_object_ref (G_OBJECT (list->data));

          /* Sort it into the proper struct member
           */
          if (special_child_type == NULL)
            {
              if (g_list_find_custom (nchildren->children,
                                      GINT_TO_POINTER (position),
                                      (GCompareFunc) notebook_find_child))
                nchildren->extra_children =
                    g_list_insert_sorted
                    (nchildren->extra_children, list->data,
                     (GCompareFunc) notebook_child_compare_func);
              else
                nchildren->children =
                    g_list_insert_sorted
                    (nchildren->children, list->data,
                     (GCompareFunc) notebook_child_compare_func);
            }
          else if (!strcmp (special_child_type, "tab"))
            {
              if (g_list_find_custom (nchildren->tabs,
                                      GINT_TO_POINTER (position),
                                      (GCompareFunc) notebook_find_child))
                nchildren->extra_tabs =
                    g_list_insert_sorted
                    (nchildren->extra_tabs, list->data,
                     (GCompareFunc) notebook_child_compare_func);
              else
                nchildren->tabs =
                    g_list_insert_sorted
                    (nchildren->tabs, list->data,
                     (GCompareFunc) notebook_child_compare_func);
            }
        }
    }

  /* Remove all pages, resulting in the unparenting of all widgets including tab-labels.
   */
  while (gtk_notebook_get_n_pages (nb) > 0)
    {
      GtkWidget *page = gtk_notebook_get_nth_page (nb, 0);
      GtkWidget *tab = gtk_notebook_get_tab_label (nb, page);

      if (tab)
        g_object_ref (tab);

      /* Explicitly remove the tab label first */
      gtk_notebook_set_tab_label (nb, page, NULL);

      /* FIXE: we need to unparent here to avoid annoying warning when reparenting */
      if (tab)
        {
          gtk_widget_unparent (tab);
          g_object_unref (tab);
        }

      gtk_notebook_remove_page (nb, 0);
    }

  if (children)
    g_list_free (children);

  return nchildren;
}

static void
glade_gtk_notebook_insert_children (GtkWidget        *notebook,
                                    NotebookChildren *nchildren)
{
  gint i;

  /*********************************************************
   *                     INSERT PAGES                      *
   *********************************************************/
  for (i = 0; i < nchildren->pages; i++)
    {
      GtkWidget *page = notebook_get_page (nchildren, i);
      GtkWidget *tab = notebook_get_tab (nchildren, i);

      gtk_notebook_insert_page (GTK_NOTEBOOK (notebook), page, tab, i);

      g_object_unref (G_OBJECT (page));
      g_object_unref (G_OBJECT (tab));
    }

  /* Stay on the same page */
  gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), nchildren->page);

  /* Free the original lists now */
  if (nchildren->children)
    g_list_free (nchildren->children);

  if (nchildren->tabs)
    g_list_free (nchildren->tabs);

  if (nchildren->children ||
      nchildren->tabs || nchildren->extra_children || nchildren->extra_tabs)
    g_critical ("Unbalanced children when inserting notebook children"
                " (pages: %d tabs: %d extra pages: %d extra tabs %d)",
                g_list_length (nchildren->children),
                g_list_length (nchildren->tabs),
                g_list_length (nchildren->extra_children),
                g_list_length (nchildren->extra_tabs));

  g_free (nchildren);
}

static void
glade_gtk_notebook_switch_page (GtkNotebook *notebook,
                                GtkWidget   *page,
                                guint        page_num,
                                gpointer     user_data)
{
  GladeWidget *gnotebook = glade_widget_get_from_gobject (notebook);

  glade_widget_property_set (gnotebook, "page", page_num);

}

/* Track project selection to set the notebook pages to display
 * the selected widget.
 */
static void
glade_gtk_notebook_selection_changed (GladeProject *project,
                                      GladeWidget  *gwidget)
{
  GList *list;
  gint i;
  GtkWidget *page, *sel_widget;
  GtkNotebook *notebook = GTK_NOTEBOOK (glade_widget_get_object (gwidget));

  if ((list = glade_project_selection_get (project)) != NULL &&
      g_list_length (list) == 1)
    {
      sel_widget = list->data;

      /* Check if selected widget is inside the notebook */
      if (GTK_IS_WIDGET (sel_widget) &&
          gtk_widget_is_ancestor (sel_widget, GTK_WIDGET (notebook)))
        {
          /* Find and activate the page */
          for (i = 0;
               i < gtk_notebook_get_n_pages (notebook);
               i++)
            {
              page = gtk_notebook_get_nth_page (notebook, i);

              if (sel_widget == page ||
                  gtk_widget_is_ancestor (sel_widget, GTK_WIDGET (page)))
                {
                  glade_widget_property_set (gwidget, "page", i);
                  return;
                }
            }
        }
    }
}

static void
glade_gtk_notebook_project_changed (GladeWidget *gwidget,
                                    GParamSpec  *pspec,
                                    gpointer     userdata)
{
  GladeProject
      * project = glade_widget_get_project (gwidget),
      *old_project =
      g_object_get_data (G_OBJECT (gwidget), "notebook-project-ptr");

  if (old_project)
    g_signal_handlers_disconnect_by_func (G_OBJECT (old_project),
                                          G_CALLBACK
                                          (glade_gtk_notebook_selection_changed),
                                          gwidget);

  if (project)
    g_signal_connect (G_OBJECT (project), "selection-changed",
                      G_CALLBACK (glade_gtk_notebook_selection_changed),
                      gwidget);

  g_object_set_data (G_OBJECT (gwidget), "notebook-project-ptr", project);
}

static void
glade_gtk_notebook_parse_finished (GladeProject *project, GObject *object)
{
  GtkWidget *action;

  action = gtk_notebook_get_action_widget (GTK_NOTEBOOK (object), GTK_PACK_START);
  glade_widget_property_set (glade_widget_get_from_gobject (object),
                             "has-action-start", action != NULL);
  action = gtk_notebook_get_action_widget (GTK_NOTEBOOK (object), GTK_PACK_END);
  glade_widget_property_set (glade_widget_get_from_gobject (object),
                             "has-action-end", action != NULL);
}

void
glade_gtk_notebook_post_create (GladeWidgetAdaptor *adaptor,
                                GObject            *notebook,
                                GladeCreateReason   reason)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (notebook);
  GladeProject *project = glade_widget_get_project (gwidget);

  gtk_notebook_popup_disable (GTK_NOTEBOOK (notebook));

  g_signal_connect (G_OBJECT (gwidget), "notify::project",
                    G_CALLBACK (glade_gtk_notebook_project_changed), NULL);

  glade_gtk_notebook_project_changed (gwidget, NULL, NULL);

  g_signal_connect (G_OBJECT (notebook), "switch-page",
                    G_CALLBACK (glade_gtk_notebook_switch_page), NULL);

  if (project && glade_project_is_loading (project))
    g_signal_connect_object (project, "parse-finished",
                             G_CALLBACK (glade_gtk_notebook_parse_finished),
                             notebook,
                             0);
}

static gint
glade_gtk_notebook_get_first_blank_page (GtkNotebook *notebook)
{
  GladeWidget *gwidget;
  GtkWidget *widget;
  gint position;

  for (position = 0; position < gtk_notebook_get_n_pages (notebook); position++)
    {
      widget = gtk_notebook_get_nth_page (notebook, position);
      if ((gwidget = glade_widget_get_from_gobject (widget)) != NULL)
        {
          GladeProperty *property =
              glade_widget_get_property (gwidget, "position");
          gint gwidget_position = g_value_get_int (glade_property_inline_value (property));

          if ((gwidget_position - position) > 0)
            return position;
        }
    }
  return position;
}

static GladeWidget *
glade_gtk_notebook_generate_tab (GladeWidget *notebook, gint page_id)
{
  static GladeWidgetAdaptor *wadaptor = NULL;
  gchar *str;
  GladeWidget *glabel;

  if (wadaptor == NULL)
    wadaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_LABEL);

  glabel = glade_widget_adaptor_create_widget (wadaptor, FALSE,
                                               "parent", notebook,
                                               "project",
                                               glade_widget_get_project
                                               (notebook), NULL);

  str = g_strdup_printf ("page %d", page_id);
  glade_widget_property_set (glabel, "label", str);
  g_free (str);

  g_object_set_data (glade_widget_get_object (glabel), "special-child-type", "tab");
  gtk_widget_show (GTK_WIDGET (glade_widget_get_object (glabel)));

  return glabel;
}

static void
glade_gtk_notebook_set_n_pages (GObject *object, const GValue *value)
{
  GladeWidget *widget;
  GtkNotebook *notebook;
  GtkWidget *child_widget;
  gint new_size, i;
  gint old_size;

  notebook = GTK_NOTEBOOK (object);
  g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

  widget = glade_widget_get_from_gobject (GTK_WIDGET (notebook));
  g_return_if_fail (widget != NULL);

  new_size = g_value_get_int (value);
  old_size = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));

  /* Ensure base size of notebook */
  if (glade_widget_superuser () == FALSE)
    {
      for (i = gtk_notebook_get_n_pages (notebook); i < new_size; i++)
        {
          gint position = glade_gtk_notebook_get_first_blank_page (notebook);
          GtkWidget *placeholder = glade_placeholder_new ();
          GladeWidget *gtab;

          gtk_notebook_insert_page (notebook, placeholder, NULL, position);

          /* XXX Ugly hack amongst many, this one only creates project widgets
           * when the 'n-pages' of a notebook is initially set, otherwise it puts
           * placeholders. (this makes the job easier when doing "insert before/after")
           */
          if (old_size == 0 && new_size > 1)
            {
              gtab = glade_gtk_notebook_generate_tab (widget, position + 1);

              /* Must pass through GladeWidget api so that packing props
               * are correctly assigned.
               */
              glade_widget_add_child (widget, gtab, FALSE);
            }
          else
            {
              GtkWidget *tab_placeholder = glade_placeholder_new ();

              g_object_set_data (G_OBJECT (tab_placeholder),
                                 "special-child-type", "tab");

              gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), placeholder,
                                          tab_placeholder);
            }
        }
    }

  /*
   * Thing to remember is that GtkNotebook starts the
   * page numbers from 0, not 1 (C-style). So we need to do
   * old_size-1, where we're referring to "nth" widget.
   */
  while (old_size > new_size)
    {
      /* Get the last page and remove it (project objects have been cleared by
       * the action code already). */
      child_widget = gtk_notebook_get_nth_page (notebook, old_size - 1);

      /* Ok there shouldnt be widget in the content area, that's
       * the placeholder, we should clean up the project widget that
       * we put in the tab here though (this happens in the case where
       * we undo increasing the "pages" property).
       */
      if (glade_widget_get_from_gobject (child_widget))
        g_critical ("Bug in notebook_set_n_pages()");

      gtk_notebook_remove_page (notebook, old_size - 1);

      old_size--;
    }
}

void
glade_gtk_notebook_set_property (GladeWidgetAdaptor *adaptor,
                                 GObject            *object,
                                 const gchar        *id,
                                 const GValue       *value)
{
  if (!strcmp (id, "pages"))
    glade_gtk_notebook_set_n_pages (object, value);
  else if (!strcmp (id, "has-action-start"))
    {
      if (g_value_get_boolean (value))
        {
          GtkWidget *action = gtk_notebook_get_action_widget (GTK_NOTEBOOK (object), GTK_PACK_START);
          if (!action)
            action = glade_placeholder_new ();
          g_object_set_data (G_OBJECT (action), "special-child-type", "action-start");
          gtk_notebook_set_action_widget (GTK_NOTEBOOK (object), action, GTK_PACK_START); 
        }
      else
        gtk_notebook_set_action_widget (GTK_NOTEBOOK (object), NULL, GTK_PACK_START); 
    }
  else if (!strcmp (id, "has-action-end"))
    {
      if (g_value_get_boolean (value))
        {
          GtkWidget *action = gtk_notebook_get_action_widget (GTK_NOTEBOOK (object), GTK_PACK_END);
          if (!action)
            action = glade_placeholder_new ();
          g_object_set_data (G_OBJECT (action), "special-child-type", "action-end");
          gtk_notebook_set_action_widget (GTK_NOTEBOOK (object), action, GTK_PACK_END); 
        }
      else
        gtk_notebook_set_action_widget (GTK_NOTEBOOK (object), NULL, GTK_PACK_END); 
    }
  else
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object,
                                                      id, value);
}

void
glade_gtk_notebook_get_property (GladeWidgetAdaptor *adaptor,
                                 GObject            *object,
                                 const gchar        *id,
                                 GValue             *value)
{
  if (!strcmp (id, "has-action-start"))
    {
      g_value_reset (value);
      g_value_set_boolean (value, gtk_notebook_get_action_widget (GTK_NOTEBOOK (object), GTK_PACK_START) != NULL);
    }
  else if (!strcmp (id, "has-action-end"))
    {
      g_value_reset (value);
      g_value_set_boolean (value, gtk_notebook_get_action_widget (GTK_NOTEBOOK (object), GTK_PACK_END) != NULL);
    }
  else
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->get_property (adaptor, object, id,
                                                      value);
}

static gboolean
glade_gtk_notebook_verify_n_pages (GObject *object, const GValue *value)
{
  GtkNotebook *notebook = GTK_NOTEBOOK (object);
  GtkWidget *child_widget, *tab_widget;
  gint old_size, new_size = g_value_get_int (value);

  for (old_size = gtk_notebook_get_n_pages (notebook);
       old_size > new_size; old_size--)
    {
      /* Get the last widget. */
      child_widget = gtk_notebook_get_nth_page (notebook, old_size - 1);
      tab_widget = gtk_notebook_get_tab_label (notebook, child_widget);

      /* 
       * If we got it, and its not a placeholder, remove it
       * from project.
       */
      if (glade_widget_get_from_gobject (child_widget) ||
          glade_widget_get_from_gobject (tab_widget))
        return FALSE;
    }
  return TRUE;
}

gboolean
glade_gtk_notebook_verify_property (GladeWidgetAdaptor *adaptor,
                                    GObject            *object,
                                    const gchar        *id,
                                    const GValue       *value)
{
  if (!strcmp (id, "pages"))
    return glade_gtk_notebook_verify_n_pages (object, value);
  else if (GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->verify_property)
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->verify_property (adaptor, object,
                                                         id, value);

  return TRUE;
}

void
glade_gtk_notebook_add_child (GladeWidgetAdaptor *adaptor,
                              GObject            *object,
                              GObject            *child)
{
  GtkNotebook *notebook;
  gint num_page, position = 0;
  GtkWidget *last_page;
  GladeWidget *gwidget;
  gchar *special_child_type;

  notebook = GTK_NOTEBOOK (object);

  num_page = gtk_notebook_get_n_pages (notebook);
  gwidget = glade_widget_get_from_gobject (object);

  special_child_type = g_object_get_data (child, "special-child-type");
  if (special_child_type && !strcmp (special_child_type, "action-start"))
    {
      gtk_notebook_set_action_widget (notebook, GTK_WIDGET (child), GTK_PACK_START);
    }
  else if (special_child_type && !strcmp (special_child_type, "action-end"))
    {
      gtk_notebook_set_action_widget (notebook, GTK_WIDGET (child), GTK_PACK_END);
    }
  else if (glade_widget_superuser ())
    {
      /* Just append pages blindly when loading/dupping */
      special_child_type = g_object_get_data (child, "special-child-type");
      if (special_child_type && !strcmp (special_child_type, "tab"))
        {
          last_page = gtk_notebook_get_nth_page (notebook, num_page - 1);
          gtk_notebook_set_tab_label (notebook, last_page, GTK_WIDGET (child));
        }
      else
        {
          gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));

          glade_widget_property_set (gwidget, "pages", num_page + 1);

          gwidget = glade_widget_get_from_gobject (child);
          if (gwidget && glade_widget_get_packing_properties (gwidget))
            glade_widget_pack_property_set (gwidget, "position", num_page);
        }
    }
  else
    {
      NotebookChildren *nchildren;

      /* Just destroy placeholders */
      if (GLADE_IS_PLACEHOLDER (child))
        gtk_widget_destroy (GTK_WIDGET (child));
      else
        {
          gwidget = glade_widget_get_from_gobject (child);
          g_assert (gwidget);

          glade_widget_pack_property_get (gwidget, "position", &position);

          nchildren =
              glade_gtk_notebook_extract_children (GTK_WIDGET (notebook));

          if (g_object_get_data (child, "special-child-type") != NULL)
            {
              if (g_list_find_custom (nchildren->tabs,
                                      GINT_TO_POINTER (position),
                                      (GCompareFunc) notebook_find_child))
                nchildren->extra_tabs =
                    g_list_insert_sorted
                    (nchildren->extra_tabs, child,
                     (GCompareFunc) notebook_child_compare_func);
              else
                nchildren->tabs =
                    g_list_insert_sorted
                    (nchildren->tabs, child,
                     (GCompareFunc) notebook_child_compare_func);
            }
          else
            {
              if (g_list_find_custom (nchildren->children,
                                      GINT_TO_POINTER (position),
                                      (GCompareFunc) notebook_find_child))
                nchildren->extra_children =
                    g_list_insert_sorted
                    (nchildren->extra_children, child,
                     (GCompareFunc) notebook_child_compare_func);
              else
                nchildren->children =
                    g_list_insert_sorted
                    (nchildren->children, child,
                     (GCompareFunc) notebook_child_compare_func);
            }

          /* Takes an explicit reference when sitting on the list */
          g_object_ref (child);

          glade_gtk_notebook_insert_children (GTK_WIDGET (notebook), nchildren);
        }
    }
}

void
glade_gtk_notebook_remove_child (GladeWidgetAdaptor *adaptor,
                                 GObject            *object,
                                 GObject            *child)
{
  NotebookChildren *nchildren;
  gchar *special_child_type;

  special_child_type = g_object_get_data (child, "special-child-type");
  if (special_child_type && !strcmp (special_child_type, "action-start"))
    {
      GtkWidget *placeholder = glade_placeholder_new ();
      g_object_set_data (G_OBJECT (placeholder), "special-child-type", "action-start");
      gtk_notebook_set_action_widget (GTK_NOTEBOOK (object), placeholder, GTK_PACK_START);
      return;
    }
  else if (special_child_type && !strcmp (special_child_type, "action-end"))
    {
      GtkWidget *placeholder = glade_placeholder_new ();
      g_object_set_data (G_OBJECT (placeholder), "special-child-type", "action-end");
      gtk_notebook_set_action_widget (GTK_NOTEBOOK (object), placeholder, GTK_PACK_END);
      return;
    }

  nchildren = glade_gtk_notebook_extract_children (GTK_WIDGET (object));

  if (g_list_find (nchildren->children, child))
    {
      nchildren->children = g_list_remove (nchildren->children, child);
      g_object_unref (child);
    }
  else if (g_list_find (nchildren->extra_children, child))
    {
      nchildren->extra_children =
          g_list_remove (nchildren->extra_children, child);
      g_object_unref (child);
    }
  else if (g_list_find (nchildren->tabs, child))
    {
      nchildren->tabs = g_list_remove (nchildren->tabs, child);
      g_object_unref (child);
    }
  else if (g_list_find (nchildren->extra_tabs, child))
    {
      nchildren->extra_tabs = g_list_remove (nchildren->extra_tabs, child);
      g_object_unref (child);
    }

  glade_gtk_notebook_insert_children (GTK_WIDGET (object), nchildren);

}

void
glade_gtk_notebook_replace_child (GladeWidgetAdaptor *adaptor,
                                  GtkWidget          *container,
                                  GtkWidget          *current,
                                  GtkWidget          *new_widget)
{
  GtkNotebook *notebook;
  GladeWidget *gcurrent, *gnew;
  gint position = 0;
  gchar *special_child_type;

  notebook = GTK_NOTEBOOK (container);

  special_child_type = g_object_get_data (G_OBJECT (current), "special-child-type");
  g_object_set_data (G_OBJECT (new_widget), "special-child-type", special_child_type);
  if (!g_strcmp0 (special_child_type, "action-start"))
    {
      gtk_notebook_set_action_widget (notebook, GTK_WIDGET (new_widget), GTK_PACK_START);
      return;
    }
  else if (!g_strcmp0 (special_child_type, "action-end"))
    {
      gtk_notebook_set_action_widget (notebook, GTK_WIDGET (new_widget), GTK_PACK_END);
      return;
    }

  if ((gcurrent = glade_widget_get_from_gobject (current)) != NULL)
    glade_widget_pack_property_get (gcurrent, "position", &position);
  else
    {
      if ((position = gtk_notebook_page_num (notebook, current)) < 0)
        {
          position = notebook_search_tab (notebook, current);
          g_assert (position >= 0);
        }
    }

  glade_gtk_notebook_remove_child (adaptor,
                                   G_OBJECT (container), G_OBJECT (current));

  if (GLADE_IS_PLACEHOLDER (new_widget) == FALSE)
    {
      gnew = glade_widget_get_from_gobject (new_widget);

      glade_gtk_notebook_add_child (adaptor,
                                    G_OBJECT (container),
                                    G_OBJECT (new_widget));

      if (glade_widget_pack_property_set (gnew, "position", position) == FALSE)
        g_critical ("No position property found on new widget");
    }
  else
    gtk_widget_destroy (GTK_WIDGET (new_widget));
}

gboolean
glade_gtk_notebook_child_verify_property (GladeWidgetAdaptor *adaptor,
                                          GObject            *container,
                                          GObject            *child,
                                          const gchar        *id,
                                          GValue             *value)
{
  if (!strcmp (id, "position"))
    return g_value_get_int (value) >= 0 &&
        g_value_get_int (value) <
        gtk_notebook_get_n_pages (GTK_NOTEBOOK (container));
  else if (GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->child_verify_property)
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS
        (GTK_TYPE_CONTAINER)->child_verify_property (adaptor,
                                                     container, child,
                                                     id, value);

  return TRUE;
}

void
glade_gtk_notebook_set_child_property (GladeWidgetAdaptor *adaptor,
                                       GObject            *container,
                                       GObject            *child,
                                       const gchar        *property_name,
                                       const GValue       *value)
{
  NotebookChildren *nchildren;

  if (strcmp (property_name, "position") == 0)
    {
      /* If we are setting this internally, avoid feedback. */
      if (glade_gtk_notebook_setting_position || glade_widget_superuser ())
        return;

      /* Just rebuild the notebook, property values are already set at this point */
      nchildren = glade_gtk_notebook_extract_children (GTK_WIDGET (container));
      glade_gtk_notebook_insert_children (GTK_WIDGET (container), nchildren);
    }
  /* packing properties are unsupported on tabs ... except "position" */
  else if (g_object_get_data (child, "special-child-type") == NULL)
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS
        (GTK_TYPE_CONTAINER)->child_set_property (adaptor,
                                                  container, child,
                                                  property_name, value);
}

void
glade_gtk_notebook_get_child_property (GladeWidgetAdaptor *adaptor,
                                       GObject            *container,
                                       GObject            *child,
                                       const gchar        *property_name,
                                       GValue             *value)
{
  gint position;

  if (strcmp (property_name, "position") == 0)
    {
      if (g_strcmp0 (g_object_get_data (child, "special-child-type"), "tab") == 0)
        {
          if ((position = notebook_search_tab (GTK_NOTEBOOK (container),
                                               GTK_WIDGET (child))) >= 0)
            g_value_set_int (value, position);
          else
            g_value_set_int (value, 0);
        }
      else if (g_object_get_data (child, "special-child-type") != NULL)
        {
          g_value_set_int (value, 0);
        }
      else
        gtk_container_child_get_property (GTK_CONTAINER (container),
                                          GTK_WIDGET (child),
                                          property_name, value);
    }
  /* packing properties are unsupported on tabs ... except "position" */
  else if (g_object_get_data (child, "special-child-type") == NULL)
    gtk_container_child_get_property (GTK_CONTAINER (container),
                                      GTK_WIDGET (child), property_name, value);
}

void
glade_gtk_notebook_child_action_activate (GladeWidgetAdaptor *adaptor,
                                          GObject            *container,
                                          GObject            *object,
                                          const gchar        *action_path)
{
  if (strcmp (action_path, "insert_page_after") == 0)
    {
      glade_gtk_box_notebook_child_insert_remove_action (adaptor, container,
                                                         object, FALSE, TRUE);
    }
  else if (strcmp (action_path, "insert_page_before") == 0)
    {
      glade_gtk_box_notebook_child_insert_remove_action (adaptor, container,
                                                         object, FALSE, FALSE);
    }
  else if (strcmp (action_path, "remove_page") == 0)
    {
      glade_gtk_box_notebook_child_insert_remove_action (adaptor, container,
                                                         object, TRUE, TRUE);
    }
  else
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->child_action_activate (adaptor,
                                                               container,
                                                               object,
                                                               action_path);
}

/* Shared with glade-gtk-box.c */
void
glade_gtk_box_notebook_child_insert_remove_action (GladeWidgetAdaptor *adaptor,
                                                   GObject            *container,
                                                   GObject            *object,
                                                   gboolean            remove,
                                                   gboolean            after)
{
  gboolean is_notebook = GTK_IS_NOTEBOOK (container);
  const gchar *size_prop = (is_notebook) ? "pages" : "size";
  GladeWidget *parent;
  GList *children, *l;
  gint child_pos, size, offset;

  if (is_notebook && g_object_get_data (object, "special-child-type"))
    /* Its a Tab! */
    child_pos = notebook_search_tab (GTK_NOTEBOOK (container),
                                     GTK_WIDGET (object));
  else
    gtk_container_child_get (GTK_CONTAINER (container),
                             GTK_WIDGET (object), "position", &child_pos, NULL);

  parent = glade_widget_get_from_gobject (container);
  if (is_notebook)
    {
      if (remove)
        glade_command_push_group (_("Remove page from %s"), glade_widget_get_name (parent));
      else
        glade_command_push_group (_("Insert page on %s"), glade_widget_get_name (parent));
    }
  else
    {
      if (remove)
        glade_command_push_group (_("Remove placeholder from %s"), glade_widget_get_name (parent));
      else
        glade_command_push_group (_("Insert placeholder to %s"), glade_widget_get_name (parent));
    }

  /* Make sure widgets does not get destroyed */
  children = glade_widget_adaptor_get_children (adaptor, container);
  glade_util_list_objects_ref (children);

  glade_widget_property_get (parent, size_prop, &size);

  if (remove)
    {
      GList *del = NULL;
      offset = -1;
      /* Remove children first */
      for (l = children; l; l = g_list_next (l))
        {
          GladeWidget *gchild = glade_widget_get_from_gobject (l->data);
          gint pos;

          /* Skip placeholders */
          if (gchild == NULL)
            continue;

          glade_widget_pack_property_get (gchild, "position", &pos);
          if (pos == child_pos)
            del = g_list_prepend (del, gchild);
        }
      if (del)
        {
          glade_command_delete (del);
          g_list_free (del);
        }
    }
  else
    {
      /* Expand container */
      glade_command_set_property (glade_widget_get_property (parent, size_prop),
                                  size + 1);
      offset = 1;
    }

  /* Reoder children (fix the position property tracking widget positions) */
  for (l = g_list_last (children); l; l = g_list_previous (l))
    {
      GladeWidget *gchild = glade_widget_get_from_gobject (l->data);
      gint pos;

      /* Skip placeholders */
      if (gchild == NULL)
        continue;

      glade_widget_pack_property_get (gchild, "position", &pos);
      if ((after) ? pos > child_pos : pos >= child_pos)
        glade_command_set_property (glade_widget_get_pack_property
                                    (gchild, "position"), pos + offset);
    }

  if (remove)
    {
      /* Shrink container */
      glade_command_set_property (glade_widget_get_property (parent, size_prop),
                                  size - 1);
    }
  /* If it's a notebook we need to create an undoable tab now */
  else if (GTK_IS_NOTEBOOK (container))
    {
      gint new_pos = after ? child_pos + 1 : child_pos;
      GtkWidget *new_page;
      GtkWidget *tab_placeholder;
      GladeWidget *gtab;
      GList list = { 0, };

      new_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (container), new_pos);

      /* Deleting the project widget gives us a real placeholder now */
      new_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (container), new_pos);
      tab_placeholder =
          gtk_notebook_get_tab_label (GTK_NOTEBOOK (container), new_page);
      gtab = glade_gtk_notebook_generate_tab (parent, new_pos + 1);
      list.data = gtab;

      glade_command_paste (&list, parent, GLADE_PLACEHOLDER (tab_placeholder),
                           glade_widget_get_project (parent));
    }

  g_list_free_full (children, g_object_unref);
  glade_command_pop_group ();
}
