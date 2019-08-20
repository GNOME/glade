
/*
 * Copyright (C) 2006-2016 Juan Pablo Ugarte.
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
 *
 * Authors:
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#include "config.h"

/**
 * SECTION:glade-base-editor
 * @Short_Description: A customisable editor
 *
 * Convenience object to edit containers where placeholders do not make sense, like GtkMenubar.
 */

#include "glade.h"
#include "glade-marshallers.h"
#include "glade-editor-property.h"
#include "glade-base-editor.h"
#include "glade-app.h"
#include "glade-popup.h"
#include "glade-accumulators.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

typedef enum
{
  GLADE_BASE_EDITOR_GTYPE,
  GLADE_BASE_EDITOR_CLASS_NAME,
  GLADE_BASE_EDITOR_TYPES_N_COLUMNS
} GladeBaseEditorChildEnum;

typedef enum
{
  GLADE_BASE_EDITOR_GWIDGET,
  GLADE_BASE_EDITOR_OBJECT,
  GLADE_BASE_EDITOR_TYPE_NAME,
  GLADE_BASE_EDITOR_NAME,
  GLADE_BASE_EDITOR_CHILD_TYPES,
  GLADE_BASE_EDITOR_N_COLUMNS
} GladeBaseEditorEnum;

typedef enum {
  ADD_ROOT = 0,
  ADD_SIBLING,
  ADD_CHILD
} GladeBaseEditorAddMode;

typedef struct
{
  GType parent_type;
  GtkTreeModel *children;
} ChildTypeTab;

typedef struct _GladeBaseEditorPrivate
{
  GladeWidget *gcontainer;      /* The container we are editing */

  /* Editor UI */
  GtkWidget *paned, *table, *treeview, *tip_label;
  GtkWidget *add_button, *delete_button, *help_button;
  GladeSignalEditor *signal_editor;

  GList *child_types;

  GtkTreeModel *model;
  GladeProject *project;

  /* Add button data */
  GType add_type;

  /* Temporal variables */
  GtkTreeIter iter;             /* used in idle functions */
  gint row;

  gboolean updating_treeview;

  guint properties_idle;
} GladeBaseEditorPrivate;

enum
{
  SIGNAL_CHILD_SELECTED,
  SIGNAL_CHANGE_TYPE,
  SIGNAL_GET_DISPLAY_NAME,
  SIGNAL_BUILD_CHILD,
  SIGNAL_DELETE_CHILD,
  SIGNAL_MOVE_CHILD,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_CONTAINER,
  N_PROPERTIES
};

G_DEFINE_TYPE_WITH_PRIVATE (GladeBaseEditor, glade_base_editor, GTK_TYPE_BOX)

static GParamSpec *properties[N_PROPERTIES];
static guint glade_base_editor_signals[LAST_SIGNAL] = { 0 };

static void glade_base_editor_set_container (GladeBaseEditor *editor,
                                             GObject         *container);
static void glade_base_editor_block_callbacks (GladeBaseEditor *editor,
                                               gboolean         block);


static void
reset_child_types (GladeBaseEditor *editor)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  GList *l;
  ChildTypeTab *tab;

  for (l = priv->child_types; l; l = l->next)
    {
      tab = l->data;
      g_object_unref (tab->children);
      g_free (tab);
    }
  g_list_free (priv->child_types);
  priv->child_types = NULL;
}


static gint
sort_type_by_hierarchy (ChildTypeTab *a, ChildTypeTab *b)
{
  if (g_type_is_a (a->parent_type, b->parent_type))
    return -1;

  return 1;
}

static GtkTreeModel *
get_children_model_for_type (GladeBaseEditor *editor, GType type)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);

  GList *l;
  for (l = priv->child_types; l; l = l->next)
    {
      ChildTypeTab *tab = l->data;
      if (g_type_is_a (type, tab->parent_type))
        return tab->children;
    }
  return NULL;
}

static GtkTreeModel *
get_children_model_for_child_type (GladeBaseEditor *editor, GType type)
{
  GList *l;
  GtkTreeModel *model = NULL;
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);

  /* Get deep derived classes first and work up the sorted heirarchy */
  for (l = g_list_last (priv->child_types); model == NULL && l;
       l = l->prev)
    {
      ChildTypeTab *tab = l->data;
      GtkTreeIter iter;
      GType iter_type;

      if (!gtk_tree_model_get_iter_first (tab->children, &iter))
        continue;

      do
        {
          gtk_tree_model_get (tab->children, &iter,
                              GLADE_BASE_EDITOR_GTYPE, &iter_type, -1);

          /* Find exact match types in this case */
          if (iter_type == type)
            model = tab->children;

        }
      while (model == NULL && gtk_tree_model_iter_next (tab->children, &iter));
    }

  return model;
}

static gboolean
glade_base_editor_get_type_info (GladeBaseEditor *e,
                                 GtkTreeIter     *retiter,
                                 GType            child_type,
                                 ...)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GType type;

  model = get_children_model_for_child_type (e, child_type);

  if (!model || gtk_tree_model_get_iter_first (model, &iter) == FALSE)
    return FALSE;

  do
    {
      gtk_tree_model_get (model, &iter, GLADE_BASE_EDITOR_GTYPE, &type, -1);

      if (child_type == type)
        {
          va_list args;
          va_start (args, child_type);
          gtk_tree_model_get_valist (model, &iter, args);
          va_end (args);
          if (retiter)
            *retiter = iter;
          return TRUE;
        }
    }
  while (gtk_tree_model_iter_next (model, &iter));

  return FALSE;
}

static gchar *
glade_base_editor_get_display_name (GladeBaseEditor *editor,
                                    GladeWidget     *gchild)
{
  gchar *retval;
  g_signal_emit (editor,
                 glade_base_editor_signals[SIGNAL_GET_DISPLAY_NAME],
                 0, gchild, &retval);

  return retval;
}

static void
glade_base_editor_fill_store_real (GladeBaseEditor *e,
                                   GladeWidget     *gwidget,
                                   GtkTreeIter     *parent)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (e);
  GList *children, *l;
  GtkTreeIter iter;

  children = glade_widget_get_children (gwidget);

  for (l = children; l; l = l->next)
    {
      GladeWidget *gchild;
      GObject     *child = l->data;
      gchar       *type_name = NULL, *name;

      gchild = glade_widget_get_from_gobject (child);

      /* Have to check parents here for compatibility (could be the parenting menuitem of this menu
       * supports a menuitem...) */
      if (glade_base_editor_get_type_info (e, NULL,
                                           G_OBJECT_TYPE (child),
                                           GLADE_BASE_EDITOR_CLASS_NAME,
                                           &type_name, -1))
        {
          gtk_tree_store_append (GTK_TREE_STORE (priv->model), &iter, parent);

          name = glade_base_editor_get_display_name (e, gchild);

          gtk_tree_store_set (GTK_TREE_STORE (priv->model), &iter,
                              GLADE_BASE_EDITOR_GWIDGET, gchild,
                              GLADE_BASE_EDITOR_OBJECT, child,
                              GLADE_BASE_EDITOR_TYPE_NAME, type_name,
                              GLADE_BASE_EDITOR_NAME, name,
                              GLADE_BASE_EDITOR_CHILD_TYPES,
                              get_children_model_for_child_type (e, G_OBJECT_TYPE (child)),
                              -1);

          glade_base_editor_fill_store_real (e, gchild, &iter);

          g_free (name);
          g_free (type_name);
      }
      else
        glade_base_editor_fill_store_real (e, gchild, parent);
    }

  g_list_free (children);
}

static void
glade_base_editor_fill_store (GladeBaseEditor *e)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (e);

  gtk_tree_store_clear (GTK_TREE_STORE (priv->model));
  gtk_tree_view_set_model (GTK_TREE_VIEW (priv->treeview), NULL);
  glade_base_editor_fill_store_real (e, priv->gcontainer, NULL);
  gtk_tree_view_set_model (GTK_TREE_VIEW (priv->treeview), priv->model);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (priv->treeview));

}

static gboolean
glade_base_editor_get_child_selected (GladeBaseEditor *e, GtkTreeIter *iter)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (e);
  GtkTreeSelection *sel =
    gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->treeview));
  return (sel) ? gtk_tree_selection_get_selected (sel, NULL, iter) : FALSE;
}

/* Forward declaration for glade_base_editor_project_widget_name_changed */
static void
glade_base_editor_project_widget_name_changed (GladeProject *project,
                                               GladeWidget *widget,
                                               GladeBaseEditor *editor);


static GladeWidget *
glade_base_editor_delegate_build_child (GladeBaseEditor *editor,
                                        GladeWidget     *parent,
                                        GType            type)
{
  GladeWidget *child = NULL;
  g_signal_emit (editor, glade_base_editor_signals[SIGNAL_BUILD_CHILD],
                 0, parent, type, &child);
  return child;
}

static gboolean
glade_base_editor_delegate_delete_child (GladeBaseEditor *editor,
                                         GladeWidget     *parent,
                                         GladeWidget     *child)
{
  gboolean retval;

  g_signal_emit (editor, glade_base_editor_signals[SIGNAL_DELETE_CHILD],
                 0, parent, child, &retval);

  return retval;
}

static void
glade_base_editor_name_activate (GtkEntry *entry, GladeWidget *gchild)
{
  const gchar *text = gtk_entry_get_text (GTK_ENTRY (entry));
  GladeBaseEditor *editor = g_object_get_data (G_OBJECT (entry), "editor");
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  gchar *new_name = NULL;

  if (text == NULL || text[0] == '\0')
    {
      /* If we are explicitly trying to set the widget name to be empty,
       * then we must not allow it there are any active references to
       * the widget which would otherwise break.
       */
      if (!glade_widget_has_prop_refs (gchild))
        new_name = glade_project_new_widget_name (priv->project, NULL, GLADE_UNNAMED_PREFIX);
    }
  else
    new_name = g_strdup (text);

  if (new_name && new_name[0])
    {
      g_signal_handlers_block_by_func (priv->project,
                                       glade_base_editor_project_widget_name_changed, editor);
      glade_command_set_name (gchild, new_name);
      g_signal_handlers_unblock_by_func (priv->project,
                                         glade_base_editor_project_widget_name_changed, editor);
    }

  g_free (new_name);
}

static void
glade_base_editor_table_attach (GladeBaseEditor *e,
                                GtkWidget *child1,
                                GtkWidget *child2)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (e);
  GtkGrid *table = GTK_GRID (priv->table);
  gint row = priv->row;

  if (child1)
    {
      gtk_grid_attach (table, child1, 0, row, 1, 1);
      gtk_widget_set_hexpand (child1, TRUE);
      gtk_widget_show (child1);
    }

  if (child2)
    {
      gtk_grid_attach (table, child2, 1, row, 1, 1);
      gtk_widget_show (child2);
    }

  priv->row++;
}

static void
glade_base_editor_clear (GladeBaseEditor *editor)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);

  gtk_widget_show (priv->tip_label);
  gtk_container_foreach (GTK_CONTAINER (priv->table),
                         (GtkCallback)gtk_widget_destroy, NULL);
  priv->row = 0;
  gtk_widget_set_sensitive (priv->delete_button, FALSE);
  glade_signal_editor_load_widget (priv->signal_editor, NULL);
}

static void
glade_base_editor_treeview_cursor_changed (GtkTreeView     *treeview,
                                           GladeBaseEditor *editor)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  GtkTreeIter iter;
  GObject *child;
  GladeWidget *gchild;

  g_return_if_fail (GTK_IS_TREE_VIEW (treeview));

  if (!glade_base_editor_get_child_selected (editor, &iter))
    return;

  glade_base_editor_clear (editor);
  gtk_widget_set_sensitive (priv->delete_button, TRUE);

  gtk_tree_model_get (priv->model, &iter,
                      GLADE_BASE_EDITOR_GWIDGET, &gchild,
                      GLADE_BASE_EDITOR_OBJECT, &child, -1);

  g_object_unref (gchild);
  g_object_unref (child);

  /* Emit child-selected signal and let the user add the properties */
  g_signal_emit (editor, glade_base_editor_signals[SIGNAL_CHILD_SELECTED],
                 0, gchild);

  /* Update Signal Editor */
  glade_signal_editor_load_widget (priv->signal_editor, gchild);
}

static gboolean
glade_base_editor_update_properties_idle (gpointer data)
{
  GladeBaseEditor *editor = (GladeBaseEditor *) data;
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);

  glade_base_editor_treeview_cursor_changed
      (GTK_TREE_VIEW (priv->treeview), editor);
  priv->properties_idle = 0;
  return FALSE;
}

/* XXX Can we make it crisper by removing this idle ?? */
static void
glade_base_editor_update_properties (GladeBaseEditor *editor)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);

  g_return_if_fail (GLADE_IS_BASE_EDITOR (editor));

  if (!priv->properties_idle)
    priv->properties_idle =
        g_idle_add (glade_base_editor_update_properties_idle, editor);
}

static void
glade_base_editor_set_cursor (GladeBaseEditor *e, GtkTreeIter *iter)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (e);
  GtkTreePath *path;
  GtkTreeIter real_iter;

  if (iter == NULL && glade_base_editor_get_child_selected (e, &real_iter))
    iter = &real_iter;

  if (iter && (path = gtk_tree_model_get_path (priv->model, iter)))
    {
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->treeview), path, NULL,
                                FALSE);
      gtk_tree_path_free (path);
    }
}

static gboolean
glade_base_editor_find_child_real (GladeBaseEditor *e,
                                   GladeWidget     *gchild,
                                   GtkTreeIter     *iter)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (e);
  GtkTreeModel *model = priv->model;
  GtkTreeIter child_iter;
  GladeWidget *child;

  do
    {
      gtk_tree_model_get (model, iter, GLADE_BASE_EDITOR_GWIDGET, &child, -1);
      g_object_unref (child);

      if (child == gchild)
        return TRUE;

      if (gtk_tree_model_iter_children (model, &child_iter, iter))
        if (glade_base_editor_find_child_real (e, gchild, &child_iter))
          {
            *iter = child_iter;
            return TRUE;
          }
    }
  while (gtk_tree_model_iter_next (model, iter));

  return FALSE;
}

static gboolean
glade_base_editor_find_child (GladeBaseEditor *e,
                              GladeWidget     *child,
                              GtkTreeIter     *iter)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (e);

  if (gtk_tree_model_get_iter_first (priv->model, iter))
    return glade_base_editor_find_child_real (e, child, iter);

  return FALSE;
}

static void
glade_base_editor_select_child (GladeBaseEditor *e, GladeWidget *child)
{
  GtkTreeIter iter;

  if (glade_base_editor_find_child (e, child, &iter))
    glade_base_editor_set_cursor (e, &iter);
}

static void
glade_base_editor_child_change_type (GladeBaseEditor *editor,
                                     GtkTreeIter     *iter,
                                     GType            type)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  GladeWidget *gchild;
  GObject *child;
  gchar *class_name;
  gboolean retval;

  glade_base_editor_block_callbacks (editor, TRUE);

  /* Get old widget data */
  gtk_tree_model_get (priv->model, iter,
                      GLADE_BASE_EDITOR_GWIDGET, &gchild,
                      GLADE_BASE_EDITOR_OBJECT, &child, -1);

  g_object_unref (gchild);
  g_object_unref (child);

  if (type == G_OBJECT_TYPE (child) || 
      !gchild || !glade_widget_get_parent (gchild))
    {
      glade_base_editor_block_callbacks (editor, FALSE);
      return;
    }

  /* Start of glade-command */
  if (glade_base_editor_get_type_info (editor, NULL,
                                       type,
                                       GLADE_BASE_EDITOR_CLASS_NAME,
                                       &class_name, -1))
    {
      glade_command_push_group (_("Setting object type on %s to %s"),
                                glade_widget_get_name (gchild), class_name);
      g_free (class_name);
    }
  else
    {
      glade_base_editor_block_callbacks (editor, FALSE);
      return;
    }

  g_signal_emit (editor,
                 glade_base_editor_signals[SIGNAL_CHANGE_TYPE],
                 0, gchild, type, &retval);

  /* End of glade-command */
  glade_command_pop_group ();

  /* Update properties */
  glade_base_editor_update_properties (editor);

  glade_base_editor_block_callbacks (editor, FALSE);
}

static void
glade_base_editor_type_changed (GtkComboBox *widget, GladeBaseEditor *e)
{
  GtkTreeIter iter, combo_iter;
  GType type;

  if (!glade_base_editor_get_child_selected (e, &iter))
    return;

  gtk_combo_box_get_active_iter (widget, &combo_iter);

  gtk_tree_model_get (gtk_combo_box_get_model (widget), &combo_iter,
                      GLADE_BASE_EDITOR_GTYPE, &type, -1);

  glade_base_editor_child_change_type (e, &iter, type);
}

static void
glade_base_editor_child_type_edited (GtkCellRendererText *cell,
                                     const gchar         *path_string,
                                     const gchar         *new_text,
                                     GladeBaseEditor     *editor)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  GtkTreeModel *child_class;
  GtkTreePath *path;
  GtkTreeIter iter, combo_iter;
  GType type;
  gchar *type_name = NULL;

  path = gtk_tree_path_new_from_string (path_string);
  gtk_tree_model_get_iter (priv->model, &iter, path);
  gtk_tree_path_free (path);

  gtk_tree_model_get (priv->model, &iter,
                      GLADE_BASE_EDITOR_TYPE_NAME, &type_name,
                      GLADE_BASE_EDITOR_CHILD_TYPES, &child_class, -1);

  if (g_strcmp0 (type_name, new_text) == 0)
    {
      g_free (type_name);
      g_object_unref (child_class);
      return;
    }

  /* Lookup GType */
  if (!gtk_tree_model_get_iter_first (child_class, &combo_iter))
    {
      g_free (type_name);
      g_object_unref (child_class);
      return;
    }

  g_free (type_name);

  do
    {
      gtk_tree_model_get (child_class, &combo_iter,
                          GLADE_BASE_EDITOR_GTYPE, &type,
                          GLADE_BASE_EDITOR_CLASS_NAME, &type_name, -1);

      if (strcmp (type_name, new_text) == 0)
        {
          g_free (type_name);
          break;
        }

      g_free (type_name);
    }
  while (gtk_tree_model_iter_next (child_class, &combo_iter));

  glade_base_editor_child_change_type (editor, &iter, type);
}

static void
glade_base_editor_reorder_children (GladeBaseEditor *editor,
                                    GtkTreeIter     *child)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  GtkTreeModel *model = priv->model;
  GladeWidget *gchild;
  GladeProperty *property;
  GtkTreeIter parent, iter;
  gint position = 0;

  if (gtk_tree_model_iter_parent (model, &parent, child))
    gtk_tree_model_iter_children (model, &iter, &parent);
  else
    gtk_tree_model_get_iter_first (model, &iter);

  do
    {
      gtk_tree_model_get (model, &iter, GLADE_BASE_EDITOR_GWIDGET, &gchild, -1);
      g_object_unref (gchild);

      if ((property = glade_widget_get_property (gchild, "position")) != NULL)
        glade_command_set_property (property, position);
      position++;
    }
  while (gtk_tree_model_iter_next (model, &iter));
}

static void
glade_base_editor_add_child (GladeBaseEditor       *editor,
                             GType                  type, 
                             GladeBaseEditorAddMode add_mode)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  GtkTreeIter iter, new_iter;
  GladeWidget *gparent, *gchild_new;
  gchar *name, *class_name;
  gboolean selected_iter = FALSE;

  glade_base_editor_block_callbacks (editor, TRUE);

  gparent = priv->gcontainer;

  if (add_mode != ADD_ROOT &&
      (selected_iter = glade_base_editor_get_child_selected (editor, &iter)))
    {
      if (add_mode == ADD_CHILD)
        {
          gtk_tree_model_get (priv->model, &iter,
                              GLADE_BASE_EDITOR_GWIDGET, &gparent, -1);
          g_object_unref (gparent);
        }
      else if (add_mode == ADD_SIBLING &&
               gtk_tree_model_iter_parent (priv->model, &new_iter, &iter))
        {
          gtk_tree_model_get (priv->model, &new_iter,
                              GLADE_BASE_EDITOR_GWIDGET, &gparent, -1);
          g_object_unref (gparent);
        }
    }

  if (!glade_base_editor_get_type_info (editor, NULL, type,
                                        GLADE_BASE_EDITOR_CLASS_NAME,
                                        &class_name, -1))
    return;

  glade_command_push_group (_("Add a %s to %s"), class_name,
                            glade_widget_get_name (gparent));

  /* Build Child */
  gchild_new = glade_base_editor_delegate_build_child (editor, gparent, type);

  if (gchild_new == NULL)
    {
      glade_command_pop_group ();
      return;
    }

  if (selected_iter)
    {
      if (add_mode == ADD_CHILD)
        gtk_tree_store_append (GTK_TREE_STORE (priv->model), &new_iter,
                               &iter);
      else
        gtk_tree_store_insert_after (GTK_TREE_STORE (priv->model),
                                     &new_iter, NULL, &iter);
    }
  else
    gtk_tree_store_append (GTK_TREE_STORE (priv->model), &new_iter,
                           NULL);

  name = glade_base_editor_get_display_name (editor, gchild_new);

  gtk_tree_store_set (GTK_TREE_STORE (priv->model), &new_iter,
                      GLADE_BASE_EDITOR_GWIDGET, gchild_new,
                      GLADE_BASE_EDITOR_OBJECT,
                      glade_widget_get_object (gchild_new),
                      GLADE_BASE_EDITOR_TYPE_NAME, class_name,
                      GLADE_BASE_EDITOR_NAME, name,
                      GLADE_BASE_EDITOR_CHILD_TYPES,
                      get_children_model_for_type (editor,
                                                   G_OBJECT_TYPE (glade_widget_get_object (gparent))),
                      -1);

  glade_base_editor_reorder_children (editor, &new_iter);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (priv->treeview));
  glade_base_editor_set_cursor (editor, &new_iter);

  glade_command_pop_group ();

  glade_base_editor_block_callbacks (editor, FALSE);

  g_free (name);
  g_free (class_name);
}


static void
glade_base_editor_add_item_activate (GtkMenuItem     *menuitem,
                                     GladeBaseEditor *e)
{
  GObject *item = G_OBJECT (menuitem);
  GType type = GPOINTER_TO_SIZE (g_object_get_data (item, "object_type"));
  GladeBaseEditorAddMode add_mode =
      GPOINTER_TO_INT (g_object_get_data (item, "object_add_mode"));

  glade_base_editor_add_child (e, type, add_mode);
}

static GtkWidget *
glade_base_editor_popup (GladeBaseEditor *editor, GladeWidget *widget)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  GtkWidget *popup, *item;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GType iter_type;
  gchar *label;
  gchar *class_name;

  if ((model =
       get_children_model_for_child_type (editor,
                                          G_OBJECT_TYPE (glade_widget_get_object (widget)))) == NULL)
    model =
      get_children_model_for_type (editor,
                                   G_OBJECT_TYPE (glade_widget_get_object (priv->gcontainer)));

  g_assert (model);

  popup = gtk_menu_new ();

  if (gtk_tree_model_get_iter_first (model, &iter))
    do
      {
        gtk_tree_model_get (model, &iter,
                            GLADE_BASE_EDITOR_GTYPE, &iter_type,
                            GLADE_BASE_EDITOR_CLASS_NAME, &class_name, -1);

        label = g_strdup_printf (_("Add %s"), class_name);

        item = gtk_menu_item_new_with_label (label);
        gtk_widget_show (item);

        g_object_set_data (G_OBJECT (item), "object_type",
                           GSIZE_TO_POINTER (iter_type));

        g_object_set_data (G_OBJECT (item), "object_add_mode",
                           GINT_TO_POINTER (ADD_SIBLING));

        g_signal_connect (item, "activate",
                          G_CALLBACK (glade_base_editor_add_item_activate),
                          editor);
        gtk_menu_shell_append (GTK_MENU_SHELL (popup), item);

        g_free (label);
        g_free (class_name);

      }
    while (gtk_tree_model_iter_next (model, &iter));


  if ((model =
       get_children_model_for_type (editor, G_OBJECT_TYPE (glade_widget_get_object (widget)))) &&
      gtk_tree_model_get_iter_first (model, &iter))
    do
      {
        gtk_tree_model_get (model, &iter,
                            GLADE_BASE_EDITOR_GTYPE, &iter_type,
                            GLADE_BASE_EDITOR_CLASS_NAME, &class_name, -1);

        label = g_strdup_printf (_("Add child %s"), class_name);

        item = gtk_menu_item_new_with_label (label);
        gtk_widget_show (item);

        g_object_set_data (G_OBJECT (item), "object_type",
                           GSIZE_TO_POINTER (iter_type));

        g_object_set_data (G_OBJECT (item), "object_add_mode",
                           GINT_TO_POINTER (ADD_CHILD));

        g_signal_connect (item, "activate",
                          G_CALLBACK (glade_base_editor_add_item_activate),
                          editor);
        gtk_menu_shell_append (GTK_MENU_SHELL (popup), item);

        g_free (label);
        g_free (class_name);

      }
    while (gtk_tree_model_iter_next (model, &iter));

  return popup;
}

static gint
glade_base_editor_popup_handler (GtkWidget       *treeview,
                                 GdkEventButton  *event,
                                 GladeBaseEditor *e)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (e);
  GtkTreePath *path;
  GtkWidget *popup;

  if (glade_popup_is_popup_event (event))
    {
      if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview),
                                         (gint) event->x, (gint) event->y,
                                         &path, NULL, NULL, NULL))
        {
          GtkTreeIter iter;
          GladeWidget *gwidget;

          gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, NULL,
                                    FALSE);

          gtk_tree_model_get_iter (priv->model, &iter, path);
          gtk_tree_model_get (priv->model, &iter,
                              GLADE_BASE_EDITOR_GWIDGET, &gwidget, -1);


          popup = glade_base_editor_popup (e, gwidget);

          gtk_tree_path_free (path);

          gtk_menu_popup_at_pointer (GTK_MENU (popup), (GdkEvent*) event);
        }
      return TRUE;
    }

  return FALSE;
}


static void
glade_base_editor_add_activate (GtkButton *button, GladeBaseEditor *e)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (e);

  if (priv->add_type)
    glade_base_editor_add_child (e, priv->add_type, ADD_ROOT);
}

static void
glade_base_editor_delete_child (GladeBaseEditor *e)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (e);
  GladeWidget *child, *gparent;
  GtkTreeIter iter, parent;

  if (!glade_base_editor_get_child_selected (e, &iter))
    return;

  gtk_tree_model_get (priv->model, &iter,
                      GLADE_BASE_EDITOR_GWIDGET, &child, -1);

  if (gtk_tree_model_iter_parent (priv->model, &parent, &iter))
    gtk_tree_model_get (priv->model, &parent,
                        GLADE_BASE_EDITOR_GWIDGET, &gparent, -1);
  else
    gparent = priv->gcontainer;

  glade_command_push_group (_("Delete %s child from %s"),
                            glade_widget_get_name (child),
                            glade_widget_get_name (gparent));

  /* Emit delete-child signal */
  glade_base_editor_delegate_delete_child (e, gparent, child);

  glade_command_pop_group ();
}


static gboolean
glade_base_editor_treeview_key_press_event (GtkWidget       *widget,
                                            GdkEventKey     *event,
                                            GladeBaseEditor *e)
{
  if (event->keyval == GDK_KEY_Delete)
    glade_base_editor_delete_child (e);

  return FALSE;
}

static void
glade_base_editor_delete_activate (GtkButton *button, GladeBaseEditor *e)
{
  glade_base_editor_delete_child (e);
}

static gboolean
glade_base_editor_is_child (GladeBaseEditor *e,
                            GladeWidget *gchild,
                            gboolean valid_type)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (e);
  GladeWidget *gcontainer = glade_widget_get_parent (gchild);

  if (!gcontainer)
    return FALSE;

  if (valid_type)
    {
      GObject *child = glade_widget_get_object (gchild);

      if (glade_widget_get_internal (gchild) ||
          glade_base_editor_get_type_info (e, NULL,
                                           G_OBJECT_TYPE (child), -1) == FALSE)
        return FALSE;

      gcontainer = priv->gcontainer;
    }
  else
    {
      GtkTreeIter iter;
      if (glade_base_editor_get_child_selected (e, &iter))
        gtk_tree_model_get (priv->model, &iter,
                            GLADE_BASE_EDITOR_GWIDGET, &gcontainer, -1);
      else
        return FALSE;
    }

  while ((gchild = glade_widget_get_parent (gchild)))
    if (gchild == gcontainer)
      return TRUE;

  return FALSE;
}

static gboolean
glade_base_editor_update_treeview_idle (gpointer data)
{
  GladeBaseEditor *e = data;
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (e);
  GList *selection = glade_project_selection_get (priv->project);

  glade_base_editor_block_callbacks (e, TRUE);

  glade_base_editor_fill_store (e);
  glade_base_editor_clear (e);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (priv->treeview));

  if (selection)
    {
      GladeWidget *widget =
          glade_widget_get_from_gobject (G_OBJECT (selection->data));
      if (glade_base_editor_is_child (e, widget, TRUE))
        glade_base_editor_select_child (e, widget);
    }

  priv->updating_treeview = FALSE;
  glade_base_editor_block_callbacks (e, FALSE);

  return FALSE;
}

static void
glade_base_editor_project_widget_name_changed (GladeProject    *project,
                                               GladeWidget     *widget,
                                               GladeBaseEditor *editor)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  GladeWidget *selected_child;
  GtkTreeIter iter;

  if (glade_base_editor_get_child_selected (editor, &iter))
    {
      gtk_tree_model_get (priv->model, &iter,
                          GLADE_BASE_EDITOR_GWIDGET, &selected_child, -1);
      if (widget == selected_child)
        glade_base_editor_update_properties (editor);

      g_object_unref (G_OBJECT (selected_child));
    }
}

static void
glade_base_editor_project_closed (GladeProject *project, GladeBaseEditor *e)
{
  glade_base_editor_set_container (e, NULL);
}

static void
glade_base_editor_reorder (GladeBaseEditor *editor, GtkTreeIter *iter)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  GladeWidget *gchild, *gparent;
  GtkTreeIter parent_iter;
  gboolean retval;

  glade_command_push_group (_("Reorder %s's children"),
                            glade_widget_get_name (priv->gcontainer));

  gtk_tree_model_get (priv->model, iter, GLADE_BASE_EDITOR_GWIDGET, &gchild, -1);
  g_object_unref (G_OBJECT (gchild));

  if (gtk_tree_model_iter_parent (priv->model, &parent_iter, iter))
    {
      gtk_tree_model_get (priv->model, &parent_iter,
                          GLADE_BASE_EDITOR_GWIDGET, &gparent, -1);
      g_object_unref (G_OBJECT (gparent));
    }
  else
    gparent = priv->gcontainer;

  g_signal_emit (editor, glade_base_editor_signals[SIGNAL_MOVE_CHILD],
                 0, gparent, gchild, &retval);

  if (retval)
    glade_base_editor_reorder_children (editor, iter);
  else
    {
      glade_base_editor_clear (editor);
      glade_base_editor_fill_store (editor);
      glade_base_editor_find_child (editor, gchild, &priv->iter);
    }

  glade_command_pop_group ();
}

static gboolean
glade_base_editor_drag_and_drop_idle (gpointer data)
{
  GladeBaseEditor *e = data;
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (e);

  glade_base_editor_reorder (e, &priv->iter);
  gtk_tree_view_expand_all (GTK_TREE_VIEW (priv->treeview));
  glade_base_editor_set_cursor (e, &priv->iter);
  glade_base_editor_block_callbacks (e, FALSE);

  return FALSE;
}

static void
glade_base_editor_row_inserted (GtkTreeModel    *model,
                                GtkTreePath     *path,
                                GtkTreeIter     *iter,
                                GladeBaseEditor *e)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (e);
  priv->iter = *iter;
  glade_base_editor_block_callbacks (e, TRUE);
  g_idle_add (glade_base_editor_drag_and_drop_idle, e);
}

static void
glade_base_editor_project_remove_widget (GladeProject *project,
                                         GladeWidget *widget,
                                         GladeBaseEditor *e)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (e);
  if (widget == priv->gcontainer)
    {
      glade_base_editor_set_container (e, NULL);
      return;
    }

  if (glade_base_editor_is_child (e, widget, TRUE))
    {
      GtkTreeIter iter;
      if (glade_base_editor_find_child (e, widget, &iter))
        {
          gtk_tree_store_remove (GTK_TREE_STORE (priv->model), &iter);
          glade_base_editor_clear (e);
        }
    }

  if (glade_widget_get_internal (widget) && 
      glade_base_editor_is_child (e, widget, FALSE))
    glade_base_editor_update_properties (e);
}

static void
glade_base_editor_project_add_widget (GladeProject    *project,
                                      GladeWidget     *widget,
                                      GladeBaseEditor *e)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (e);
  if (priv->updating_treeview)
    return;

  if (glade_base_editor_is_child (e, widget, TRUE))
    {
      priv->updating_treeview = TRUE;
      g_idle_add (glade_base_editor_update_treeview_idle, e);
    }

  if (glade_widget_get_internal (widget) && 
      glade_base_editor_is_child (e, widget, FALSE))
    glade_base_editor_update_properties (e);
}

static gboolean
glade_base_editor_update_display_name (GtkTreeModel *model,
                                       GtkTreePath  *path,
                                       GtkTreeIter  *iter,
                                       gpointer      data)
{
  GladeBaseEditor *editor = data;
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  GladeWidget *gchild;
  gchar *name;

  gtk_tree_model_get (model, iter, GLADE_BASE_EDITOR_GWIDGET, &gchild, -1);

  name = glade_base_editor_get_display_name (editor, gchild);

  gtk_tree_store_set (GTK_TREE_STORE (priv->model), iter,
                      GLADE_BASE_EDITOR_NAME, name, -1);
  g_free (name);
  g_object_unref (G_OBJECT (gchild));

  return FALSE;
}

static void
glade_base_editor_project_changed (GladeProject    *project,
                                   GladeCommand    *command,
                                   gboolean         forward,
                                   GladeBaseEditor *editor)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  gtk_tree_model_foreach (priv->model,
                          glade_base_editor_update_display_name, editor);
}



static void
glade_base_editor_project_disconnect (GladeBaseEditor *editor)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);

  if (priv->project == NULL)
    return;

  g_signal_handlers_disconnect_by_func (priv->project,
                                        glade_base_editor_project_closed,
                                        editor);

  g_signal_handlers_disconnect_by_func (priv->project,
                                        glade_base_editor_project_remove_widget,
                                        editor);

  g_signal_handlers_disconnect_by_func (priv->project,
                                        glade_base_editor_project_add_widget,
                                        editor);

  g_signal_handlers_disconnect_by_func (priv->project,
                                        glade_base_editor_project_widget_name_changed,
                                        editor);

  g_signal_handlers_disconnect_by_func (priv->project,
                                        glade_base_editor_project_changed,
                                        editor);


  if (priv->properties_idle)
    g_source_remove (priv->properties_idle);
  priv->properties_idle = 0;
}

static void
glade_base_editor_set_container (GladeBaseEditor *editor, GObject *container)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);

  glade_base_editor_project_disconnect (editor);

  if (container == NULL)
    {
      reset_child_types (editor);

      priv->gcontainer = NULL;
      priv->project = NULL;
      glade_base_editor_block_callbacks (editor, TRUE);
      glade_base_editor_clear (editor);

      gtk_tree_view_set_model (GTK_TREE_VIEW (priv->treeview), NULL);
      gtk_tree_store_clear (GTK_TREE_STORE (priv->model));
      gtk_tree_view_set_model (GTK_TREE_VIEW (priv->treeview),
                               priv->model);

      gtk_widget_set_sensitive (priv->paned, FALSE);
      glade_base_editor_block_callbacks (editor, FALSE);

      glade_signal_editor_load_widget (priv->signal_editor, NULL);

      g_object_notify_by_pspec (G_OBJECT (editor), properties[PROP_CONTAINER]);
      return;
    }

  gtk_widget_set_sensitive (priv->paned, TRUE);

  priv->gcontainer = glade_widget_get_from_gobject (container);

  priv->project = glade_widget_get_project (priv->gcontainer);

  g_signal_connect (priv->project, "close",
                    G_CALLBACK (glade_base_editor_project_closed), editor);

  g_signal_connect (priv->project, "remove-widget",
                    G_CALLBACK (glade_base_editor_project_remove_widget),
                    editor);

  g_signal_connect (priv->project, "add-widget",
                    G_CALLBACK (glade_base_editor_project_add_widget), editor);

  g_signal_connect (priv->project, "widget-name-changed",
                    G_CALLBACK (glade_base_editor_project_widget_name_changed),
                    editor);

  g_signal_connect (priv->project, "changed",
                    G_CALLBACK (glade_base_editor_project_changed), editor);

  g_object_notify_by_pspec (G_OBJECT (editor), properties[PROP_CONTAINER]);
}

/*************************** GladeBaseEditor Class ****************************/
static void
glade_base_editor_dispose (GObject *object)
{
  GladeBaseEditor *editor = GLADE_BASE_EDITOR (object);
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);

  reset_child_types (editor);

  glade_base_editor_project_disconnect (editor);
  priv->project = NULL;

  G_OBJECT_CLASS (glade_base_editor_parent_class)->dispose (object);
}

static void
glade_base_editor_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GladeBaseEditor *editor = GLADE_BASE_EDITOR (object);

  switch (prop_id)
    {
      case PROP_CONTAINER:
        glade_base_editor_set_container (editor, g_value_get_object (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_base_editor_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GladeBaseEditor *editor = GLADE_BASE_EDITOR (object);
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);

  switch (prop_id)
    {
      case PROP_CONTAINER:
        g_value_set_object (value, glade_widget_get_object (priv->gcontainer));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}


/* Default handlers */
static gboolean
glade_base_editor_change_type (GladeBaseEditor *editor,
                               GladeWidget     *gchild,
                               GType            type)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  GladeWidget *parent, *gchild_new;
  GList *children, *l;
  GObject *child_new;
  GtkTreeIter iter;
  gchar *name, *class_name;

  parent = glade_widget_get_parent (gchild);

  if (glade_base_editor_get_type_info (editor, NULL, type,
                                       GLADE_BASE_EDITOR_CLASS_NAME,
                                       &class_name, -1) == FALSE)
    return TRUE;

  name = g_strdup (glade_widget_get_name (gchild));
  glade_base_editor_find_child (editor, gchild, &iter);

  /* Delete old widget first, we cant assume the old and new widget can live in 
   * the same parent simultaniously */
  glade_base_editor_delegate_delete_child (editor, parent, gchild);

  /* Create new widget */
  gchild_new = glade_base_editor_delegate_build_child (editor, parent, type);

  child_new = glade_widget_get_object (gchild_new);

  /* Cut and Paste childrens */
  if ((children = glade_widget_get_children (gchild)) != NULL)
    {
      GList *gchildren = NULL;

      l = children;
      while (l)
        {
          GladeWidget *w = glade_widget_get_from_gobject (l->data);

          if (w && !glade_widget_get_internal (w))
            gchildren = g_list_prepend (gchildren, w);

          l = g_list_next (l);
        }

      if (gchildren)
        {
          glade_command_dnd (gchildren, gchild_new, NULL);

          g_list_free (children);
          g_list_free (gchildren);
        }
    }

  /* Copy properties */
  glade_widget_copy_properties (gchild_new, gchild, TRUE, TRUE);

  /* Apply packing properties to the new object 
   * 
   * No need to use GladeCommand here on the newly created widget,
   * they just become the initial state for this object.
   */
  l = glade_widget_get_packing_properties (gchild);
  while (l)
    {
      GladeProperty    *orig_prop = (GladeProperty *) l->data;
      GladePropertyDef *pdef = glade_property_get_def (orig_prop);
      GladeProperty    *dup_prop = glade_widget_get_property (gchild_new, glade_property_def_id (pdef));
      glade_property_set_value (dup_prop, glade_property_inline_value (orig_prop));
      l = g_list_next (l);
    }

  /* Set the name */
  glade_command_set_name (gchild_new, name);

  if (GTK_IS_WIDGET (child_new))
    gtk_widget_show_all (GTK_WIDGET (child_new));

  /* XXX We should update the widget name in the visible tree here too */
  gtk_tree_store_set (GTK_TREE_STORE (priv->model), &iter,
                      GLADE_BASE_EDITOR_GWIDGET, gchild_new,
                      GLADE_BASE_EDITOR_OBJECT, child_new,
                      GLADE_BASE_EDITOR_TYPE_NAME, class_name, -1);
  g_free (class_name);
  g_free (name);

  return TRUE;
}

static gchar *
glade_base_editor_get_display_name_impl (GladeBaseEditor *editor,
                                         GladeWidget     *gchild)
{
  return g_strdup (glade_widget_get_display_name (gchild));
}

static GladeWidget *
glade_base_editor_build_child (GladeBaseEditor *editor,
                               GladeWidget *gparent,
                               GType type)
{
  return glade_command_create (glade_widget_adaptor_get_by_type (type),
                               gparent, NULL,
                               glade_widget_get_project (gparent));
}

static gboolean
glade_base_editor_move_child (GladeBaseEditor *editor,
                              GladeWidget     *gparent,
                              GladeWidget     *gchild)
{
  GList list = { 0, };

  if (gparent != glade_widget_get_parent (gchild))
    {
      list.data = gchild;
      glade_command_dnd (&list, gparent, NULL);
    }

  return TRUE;
}

static gboolean
glade_base_editor_delete_child_impl (GladeBaseEditor *editor,
                                     GladeWidget     *gparent,
                                     GladeWidget     *gchild)
{
  GList list = { 0, };

  list.data = gchild;
  glade_command_delete (&list);

  return TRUE;
}

static void
glade_base_editor_block_callbacks (GladeBaseEditor *editor, gboolean block)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  if (block)
    {
      g_signal_handlers_block_by_func (priv->model, glade_base_editor_row_inserted,
                                       editor);
      if (priv->project)
        {
          g_signal_handlers_block_by_func (priv->project,
                                           glade_base_editor_project_remove_widget,
                                           editor);
          g_signal_handlers_block_by_func (priv->project,
                                           glade_base_editor_project_add_widget,
                                           editor);
          g_signal_handlers_block_by_func (priv->project,
                                           glade_base_editor_project_changed,
                                           editor);
        }
    }
  else
    {
      g_signal_handlers_unblock_by_func (priv->model,
                                         glade_base_editor_row_inserted,
                                         editor);
      if (priv->project)
        {
          g_signal_handlers_unblock_by_func (priv->project,
                                             glade_base_editor_project_remove_widget,
                                             editor);
          g_signal_handlers_unblock_by_func (priv->project,
                                             glade_base_editor_project_add_widget,
                                             editor);
          g_signal_handlers_unblock_by_func (priv->project,
                                             glade_base_editor_project_changed,
                                             editor);
        }
    }
}

static void
glade_base_editor_realize_callback (GtkWidget *widget, gpointer user_data)
{
  GladeBaseEditor *editor = GLADE_BASE_EDITOR (widget);
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);

  glade_base_editor_block_callbacks (editor, TRUE);

  glade_base_editor_fill_store (editor);
  gtk_tree_view_expand_all (GTK_TREE_VIEW (priv->treeview));

  glade_base_editor_block_callbacks (editor, FALSE);
}

static void
glade_base_editor_init (GladeBaseEditor *editor)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  gtk_widget_init_template (GTK_WIDGET (editor));

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Label"), renderer,
                                                     "text",
                                                     GLADE_BASE_EDITOR_NAME,
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview), column);

  renderer = gtk_cell_renderer_combo_new ();
  g_object_set (renderer,
                "has-entry", FALSE,
                "text-column", GLADE_BASE_EDITOR_CLASS_NAME,
                "editable", TRUE, NULL);

  g_signal_connect (G_OBJECT (renderer), "edited",
                    G_CALLBACK (glade_base_editor_child_type_edited), editor);

  column = gtk_tree_view_column_new_with_attributes (_("Type"), renderer,
                                                     "text",
                                                     GLADE_BASE_EDITOR_TYPE_NAME,
                                                     "model",
                                                     GLADE_BASE_EDITOR_CHILD_TYPES,
                                                     NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (priv->treeview), column);
}

static void
glade_base_editor_class_init (GladeBaseEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = glade_base_editor_dispose;
  object_class->set_property = glade_base_editor_set_property;
  object_class->get_property = glade_base_editor_get_property;

  klass->change_type = glade_base_editor_change_type;
  klass->get_display_name = glade_base_editor_get_display_name_impl;
  klass->build_child = glade_base_editor_build_child;
  klass->delete_child = glade_base_editor_delete_child_impl;
  klass->move_child = glade_base_editor_move_child;

  properties[PROP_CONTAINER] =
    g_param_spec_object ("container",
                         _("Container"),
                         _("The container object this editor is currently editing"),
                         G_TYPE_OBJECT,
                         G_PARAM_READWRITE);
  
  /* Install all properties */
  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * GladeBaseEditor::child-selected:
   * @gladebaseeditor: the #GladeBaseEditor which received the signal.
   * @gchild: the selected #GladeWidget.
   *
   * Emited when the user selects a child in the editor's treeview.
   * You can add the relevant child properties here using 
   * glade_base_editor_add_default_properties() and glade_base_editor_add_properties() 
   * You can also add labels with glade_base_editor_add_label to make the
   * editor look pretty.
   */
  glade_base_editor_signals[SIGNAL_CHILD_SELECTED] =
      g_signal_new ("child-selected",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeBaseEditorClass, child_selected),
                    NULL, NULL,
                    _glade_marshal_VOID__OBJECT, G_TYPE_NONE, 1, G_TYPE_OBJECT);

  /**
   * GladeBaseEditor::child-change-type:
   * @gladebaseeditor: the #GladeBaseEditor which received the signal.
   * @child: the #GObject being changed.
   * @type: the new type for @child.
   *
   * Returns: TRUE to stop signal emision.
   */
  glade_base_editor_signals[SIGNAL_CHANGE_TYPE] =
      g_signal_new ("change-type",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeBaseEditorClass, change_type),
                    _glade_boolean_handled_accumulator, NULL, NULL,
                    G_TYPE_BOOLEAN, 2, G_TYPE_OBJECT, G_TYPE_GTYPE);

  /**
   * GladeBaseEditor::get-display-name:
   * @gladebaseeditor: the #GladeBaseEditor which received the signal.
   * @gchild: the child to get display name string to show in @gladebaseeditor
   * treeview.
   *
   * Returns: a newly allocated string.
   */
  glade_base_editor_signals[SIGNAL_GET_DISPLAY_NAME] =
      g_signal_new ("get-display-name",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeBaseEditorClass, get_display_name),
                    _glade_string_accumulator, NULL,
                    _glade_marshal_STRING__OBJECT,
                    G_TYPE_STRING, 1, G_TYPE_OBJECT);

  /**
   * GladeBaseEditor::build-child:
   * @gladebaseeditor: the #GladeBaseEditor which received the signal.
   * @gparent: the parent of the new child
   * @type: the #GType of the child
   *
   * Create a child widget here if something else must be done other than
   * calling glade_command_create() such as creating an intermediate parent.
   *
   * Returns: (transfer full) (nullable): the newly created #GladeWidget or
   * %NULL if child cant be created
   */
  glade_base_editor_signals[SIGNAL_BUILD_CHILD] =
      g_signal_new ("build-child",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeBaseEditorClass, build_child),
                    _glade_stop_emission_accumulator, NULL, NULL,
                    G_TYPE_OBJECT, 2, G_TYPE_OBJECT, G_TYPE_GTYPE);

  /**
   * GladeBaseEditor::delete-child:
   * @gladebaseeditor: the #GladeBaseEditor which received the signal.
   * @gparent: the parent
   * @gchild: the child to delete
   */
  glade_base_editor_signals[SIGNAL_DELETE_CHILD] =
      g_signal_new ("delete-child",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeBaseEditorClass, delete_child),
                    _glade_boolean_handled_accumulator, NULL,
                    _glade_marshal_BOOLEAN__OBJECT_OBJECT,
                    G_TYPE_BOOLEAN, 2, G_TYPE_OBJECT, G_TYPE_OBJECT);

  /**
   * GladeBaseEditor::move-child:
   * @gladebaseeditor: the #GladeBaseEditor which received the signal.
   * @gparent: the new parent of @gchild
   * @gchild: the #GladeWidget to move
   *
   * Move child here if something else must be done other than cut & paste.
   *
   * Returns: wheater child has been sucessfully moved or not.
   */
  glade_base_editor_signals[SIGNAL_MOVE_CHILD] =
      g_signal_new ("move-child",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeBaseEditorClass, move_child),
                    _glade_stop_emission_accumulator, NULL,
                    _glade_marshal_BOOLEAN__OBJECT_OBJECT,
                    G_TYPE_BOOLEAN, 2, G_TYPE_OBJECT, G_TYPE_OBJECT);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladeui/glade-base-editor.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GladeBaseEditor, paned);
  gtk_widget_class_bind_template_child_private (widget_class, GladeBaseEditor, treeview);
  gtk_widget_class_bind_template_child_private (widget_class, GladeBaseEditor, add_button);
  gtk_widget_class_bind_template_child_private (widget_class, GladeBaseEditor, delete_button);
  gtk_widget_class_bind_template_child_private (widget_class, GladeBaseEditor, help_button);
  gtk_widget_class_bind_template_child_private (widget_class, GladeBaseEditor, table);
  gtk_widget_class_bind_template_child_private (widget_class, GladeBaseEditor, signal_editor);
  gtk_widget_class_bind_template_child_private (widget_class, GladeBaseEditor, tip_label);
  gtk_widget_class_bind_template_callback (widget_class, glade_base_editor_realize_callback);
  gtk_widget_class_bind_template_callback (widget_class, glade_base_editor_treeview_cursor_changed);
  gtk_widget_class_bind_template_callback (widget_class, glade_base_editor_popup_handler);
  gtk_widget_class_bind_template_callback (widget_class, glade_base_editor_treeview_key_press_event);
  gtk_widget_class_bind_template_callback (widget_class, glade_base_editor_add_activate);
  gtk_widget_class_bind_template_callback (widget_class, glade_base_editor_delete_activate);
}

/********************************* Public API *********************************/
/**
 * glade_base_editor_new:
 * @container: a container this new editor will edit.
 * @main_editable: the custom #GladeEditable for @container, or %NULL
 * @...: A NULL terminated list of gchar *, GType
 *
 * Creates a new GladeBaseEditor with @container toplevel
 * support for all the object types indicated in the variable argument list.
 * Argument List:
 *   o The type name
 *   o The GType the editor will support
 *
 * Returns: a new GladeBaseEditor.
 */
GladeBaseEditor *
glade_base_editor_new (GObject *container, GladeEditable *main_editable, ...)
{
  ChildTypeTab *child_type;
  GladeWidget *gcontainer;
  GladeBaseEditor *editor;
  GladeBaseEditorPrivate *priv;
  GtkTreeIter iter;
  GType iter_type;
  gchar *name;
  va_list args;

  gcontainer = glade_widget_get_from_gobject (container);
  g_return_val_if_fail (GLADE_IS_WIDGET (gcontainer), NULL);

  editor = GLADE_BASE_EDITOR (g_object_new (GLADE_TYPE_BASE_EDITOR, NULL));
  priv = glade_base_editor_get_instance_private (editor);

  /* Store */
  priv->model = (GtkTreeModel *) gtk_tree_store_new (GLADE_BASE_EDITOR_N_COLUMNS,
                                                     G_TYPE_OBJECT,
                                                     G_TYPE_OBJECT,
                                                     G_TYPE_STRING,
                                                     G_TYPE_STRING,
                                                     GTK_TYPE_TREE_MODEL);

  gtk_tree_view_set_model (GTK_TREE_VIEW (priv->treeview), priv->model);
  gtk_tree_view_expand_all (GTK_TREE_VIEW (priv->treeview));

  g_signal_connect (priv->model, "row-inserted",
                    G_CALLBACK (glade_base_editor_row_inserted), editor);

  if (main_editable)
    g_warning ("%s main_editable is deprecated, the editor will only show the hierarchy editor", __func__);

  child_type = g_new0 (ChildTypeTab, 1);
  child_type->parent_type = G_OBJECT_TYPE (container);
  child_type->children =
      (GtkTreeModel *) gtk_list_store_new (GLADE_BASE_EDITOR_TYPES_N_COLUMNS,
                                           G_TYPE_GTYPE, G_TYPE_STRING);

  va_start (args, main_editable);
  while ((name = va_arg (args, gchar *)))
    {
      iter_type = va_arg (args, GType);

      gtk_list_store_append (GTK_LIST_STORE (child_type->children), &iter);
      gtk_list_store_set (GTK_LIST_STORE (child_type->children), &iter,
                          GLADE_BASE_EDITOR_GTYPE, iter_type,
                          GLADE_BASE_EDITOR_CLASS_NAME, name, -1);

      if (priv->add_type == 0)
        priv->add_type = iter_type;
    }
  va_end (args);

  priv->child_types = g_list_prepend (priv->child_types, child_type);

  glade_base_editor_set_container (editor, container);

  glade_signal_editor_load_widget (priv->signal_editor, priv->gcontainer);

  return editor;
}



/**
 * glade_base_editor_append_types:
 * @editor: A #GladeBaseEditor
 * @parent_type: the parent type these child types will apply to
 * @...: A NULL terminated list of gchar *, GType
 *
 * Appends support for all the object types indicated in the variable argument list.
 * Argument List:
 *   o The type name
 *   o The GType the editor will support for parents of type @type
 *
 */
void
glade_base_editor_append_types (GladeBaseEditor *editor, GType parent_type, ...)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  ChildTypeTab *child_type;
  GtkTreeIter iter;
  gchar *name;
  va_list args;

  g_return_if_fail (GLADE_IS_BASE_EDITOR (editor));
  g_return_if_fail (get_children_model_for_type (editor, parent_type) == NULL);

  child_type = g_new0 (ChildTypeTab, 1);
  child_type->parent_type = parent_type;
  child_type->children =
      (GtkTreeModel *) gtk_list_store_new (GLADE_BASE_EDITOR_TYPES_N_COLUMNS,
                                           G_TYPE_GTYPE, G_TYPE_STRING);

  va_start (args, parent_type);
  while ((name = va_arg (args, gchar *)))
    {
      gtk_list_store_append (GTK_LIST_STORE (child_type->children), &iter);
      gtk_list_store_set (GTK_LIST_STORE (child_type->children), &iter,
                          GLADE_BASE_EDITOR_GTYPE, va_arg (args, GType),
                          GLADE_BASE_EDITOR_CLASS_NAME, name,
                          -1);
    }
  va_end (args);

  priv->child_types =
      g_list_insert_sorted (priv->child_types, child_type,
                            (GCompareFunc) sort_type_by_hierarchy);
}

/**
 * glade_base_editor_add_default_properties:
 * @editor: a #GladeBaseEditor
 * @gchild: a #GladeWidget
 *
 * Add @gchild name and type property to @editor
 *
 * NOTE: This function is intended to be used in "child-selected" callbacks
 */
void
glade_base_editor_add_default_properties (GladeBaseEditor *editor,
                                          GladeWidget     *gchild)
{
  GtkTreeIter combo_iter;
  GtkWidget *label, *entry;
  GtkTreeModel *child_class;
  GtkCellRenderer *renderer;
  GObject *child;

  g_return_if_fail (GLADE_IS_BASE_EDITOR (editor));
  g_return_if_fail (GLADE_IS_WIDGET (gchild));
  g_return_if_fail (GLADE_IS_WIDGET (glade_widget_get_parent (gchild)));

  child = glade_widget_get_object (gchild);

  child_class =
      get_children_model_for_child_type (editor, G_OBJECT_TYPE (child));

  /* Name */
  label = gtk_label_new (_("ID:"));
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_widget_set_valign (label, GTK_ALIGN_START);

  entry = gtk_entry_new ();
  if (glade_widget_has_name (gchild))
    gtk_entry_set_text (GTK_ENTRY (entry), glade_widget_get_name (gchild));
  else
    gtk_entry_set_text (GTK_ENTRY (entry), "");

  g_object_set_data (G_OBJECT (entry), "editor", editor);
  g_signal_connect (entry, "activate",
                    G_CALLBACK (glade_base_editor_name_activate), gchild);
  g_signal_connect (entry, "changed",
                    G_CALLBACK (glade_base_editor_name_activate), gchild);
  glade_base_editor_table_attach (editor, label, entry);

  if (child_class && gtk_tree_model_iter_n_children (child_class, NULL) > 1)
    {
      /* Type */
      label = gtk_label_new (_("Type:"));
      gtk_widget_set_halign (label, GTK_ALIGN_END);
      gtk_widget_set_valign (label, GTK_ALIGN_START);

      entry = gtk_combo_box_new ();
      gtk_combo_box_set_model (GTK_COMBO_BOX (entry), child_class);

      renderer = gtk_cell_renderer_text_new ();
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (entry), renderer, FALSE);
      gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (entry), renderer, "text",
                                      GLADE_BASE_EDITOR_CLASS_NAME, NULL);

      if (glade_base_editor_get_type_info (editor, &combo_iter,
                                           G_OBJECT_TYPE (child), -1))
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (entry), &combo_iter);

      g_signal_connect (entry, "changed",
                        G_CALLBACK (glade_base_editor_type_changed), editor);
      glade_base_editor_table_attach (editor, label, entry);
    }
}

/**
 * glade_base_editor_add_properties:
 * @editor: a #GladeBaseEditor
 * @gchild: a #GladeWidget
 * @packing: whether we are adding packing properties or not
 * @...: A NULL terminated list of properties names.
 *
 * Add @gchild properties to @editor
 *
 * NOTE: This function is intended to be used in "child-selected" callbacks
 */
void
glade_base_editor_add_properties (GladeBaseEditor *editor,
                                  GladeWidget     *gchild,
                                  gboolean         packing,
                                  ...)
{
  GladeEditorProperty *eprop;
  va_list args;
  gchar *property;

  g_return_if_fail (GLADE_IS_BASE_EDITOR (editor));
  g_return_if_fail (GLADE_IS_WIDGET (gchild));

  va_start (args, packing);
  property = va_arg (args, gchar *);

  while (property)
    {
      eprop =
          glade_widget_create_editor_property (gchild, property, packing, TRUE);
      if (eprop)
        glade_base_editor_table_attach (editor,
                                        glade_editor_property_get_item_label (eprop),
                                        GTK_WIDGET (eprop));
      property = va_arg (args, gchar *);
    }
  va_end (args);
}


/**
 * glade_base_editor_add_editable:
 * @editor: a #GladeBaseEditor
 * @gchild: the #GladeWidget
 * @page: the #GladeEditorPageType of the desired page for @gchild
 *
 * Add @gchild editor of type @page to the base editor
 *
 * NOTE: This function is intended to be used in "child-selected" callbacks
 */
void
glade_base_editor_add_editable (GladeBaseEditor    *editor,
                                GladeWidget        *gchild,
                                GladeEditorPageType page)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  GladeEditable *editable;
  gint row;

  g_return_if_fail (GLADE_IS_BASE_EDITOR (editor));
  g_return_if_fail (GLADE_IS_WIDGET (gchild));

  editable = glade_widget_adaptor_create_editable (glade_widget_get_adaptor (gchild), page);
  glade_editable_set_show_name (editable, FALSE);
  glade_editable_load (editable, gchild);
  gtk_widget_show (GTK_WIDGET (editable));

  row = priv->row;

  gtk_grid_attach (GTK_GRID (priv->table), GTK_WIDGET (editable), 0,
                   row, 2, 1);
  gtk_widget_set_hexpand (GTK_WIDGET (editable), TRUE);

  priv->row++;

  gtk_widget_hide (priv->tip_label);
}



/**
 * glade_base_editor_add_label:
 * @editor: a #GladeBaseEditor
 * @str: the label string
 *
 * Adds a new label to @editor
 *
 * NOTE: This function is intended to be used in "child-selected" callbacks
 */
void
glade_base_editor_add_label (GladeBaseEditor *editor, gchar *str)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  GtkWidget *label;
  gchar *markup;
  gint row;

  g_return_if_fail (GLADE_IS_BASE_EDITOR (editor));
  g_return_if_fail (str != NULL);

  label = gtk_label_new (NULL);
  markup = g_strdup_printf ("<span rise=\"-20000\"><b>%s</b></span>", str);
  row = priv->row;

  gtk_label_set_markup (GTK_LABEL (label), markup);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_START);
  gtk_widget_set_margin_top (label, 6);
  gtk_widget_set_margin_bottom (label, 6);

  gtk_grid_attach (GTK_GRID (priv->table), label, 0, row, 2, 1);
  gtk_widget_show (label);
  priv->row++;

  gtk_widget_hide (priv->tip_label);
  g_free (markup);
}

/**
 * glade_base_editor_set_show_signal_editor:
 * @editor: a #GladeBaseEditor
 * @val: whether to show the signal editor
 *
 * Shows/hide @editor 's signal editor
 */
void
glade_base_editor_set_show_signal_editor (GladeBaseEditor *editor, gboolean val)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);

  g_return_if_fail (GLADE_IS_BASE_EDITOR (editor));

  if (val)
    gtk_widget_show (GTK_WIDGET (priv->signal_editor));
  else
    gtk_widget_hide (GTK_WIDGET (priv->signal_editor));
}

/* Convenience functions */

static void
glade_base_editor_help (GtkButton *button, gchar *markup)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (button))),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO, GTK_BUTTONS_OK, " ");

  gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), markup);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

/**
 * glade_base_editor_pack_new_window:
 * @editor: a #GladeBaseEditor
 * @title: the window title
 * @help_markup: the help text
 *
 * This convenience function create a new dialog window and packs @editor in it.
 *
 * Returns: (transfer full): the newly created window
 */
GtkWidget *
glade_base_editor_pack_new_window (GladeBaseEditor *editor,
                                   gchar           *title,
                                   gchar           *help_markup)
{
  GladeBaseEditorPrivate *priv = glade_base_editor_get_instance_private (editor);
  GtkWidget *window, *headerbar;
  gchar *msg;

  g_return_val_if_fail (GLADE_IS_BASE_EDITOR (editor), NULL);

  /* Window */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  headerbar = gtk_header_bar_new ();
  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (headerbar), TRUE);
  gtk_window_set_titlebar (GTK_WINDOW (window), headerbar);
  gtk_widget_show (headerbar);

  if (title)
    {
      const gchar *name = glade_widget_get_display_name (priv->gcontainer);

      gtk_header_bar_set_title (GTK_HEADER_BAR (headerbar), title);
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (headerbar), name);
    }

  g_signal_connect_swapped (G_OBJECT (editor), "notify::container",
                            G_CALLBACK (gtk_widget_destroy), window);

  msg = help_markup ? help_markup :
        _("<big><b>Tips:</b></big>\n"
          "  * Right-click over the treeview to add items.\n"
          "  * Press Delete to remove the selected item.\n"
          "  * Drag &amp; Drop to reorder.\n"
          "  * Type column is editable.");

  gtk_label_set_markup (GTK_LABEL (priv->tip_label), msg);
  g_signal_connect (priv->help_button, "clicked",
                    G_CALLBACK (glade_base_editor_help),
                    msg);

  gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (editor));
  gtk_widget_show_all (GTK_WIDGET (editor));

  gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);

  return window;
}
