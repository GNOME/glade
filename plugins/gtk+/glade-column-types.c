/*
 * Copyright (C) 2008 Juan Pablo Ugarte.
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
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#include <config.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include <gladeui/glade.h>
#include "glade-column-types.h"
#include "glade-model-data.h"

enum
{
  COLUMN_NAME,
  COLUMN_COLUMN_NAME,
  COLUMN_TYPE_EDITABLE,
  COLUMN_NAME_EDITABLE,
  COLUMN_TYPE_FOREGROUND,
  COLUMN_TYPE_STYLE,
  N_COLUMNS
};

static GtkTreeModel *types_model = NULL;

static gint
find_by_type_name (const gchar * a, const gchar * b)
{
  return strcmp (a, b);
}

static void
column_types_store_populate_enums_flags (GtkListStore * store, gboolean enums)
{
  GtkTreeIter iter;
  GList *types = NULL, *list;
  GList *adaptors = glade_widget_adaptor_list_adaptors ();
  const GList *l;

  for (list = adaptors; list; list = list->next)
    {
      GladeWidgetAdaptor *adaptor = list->data;
      GladePropertyDef   *pdef;
      GParamSpec         *pspec;

      for (l = glade_widget_adaptor_get_properties (adaptor); l; l = l->next)
        {
          pdef = l->data;
          pspec  = glade_property_def_get_pspec (pdef);

          /* special case out a few of these... */
          if (strcmp (g_type_name (pspec->value_type),
                      "GladeStock") == 0 ||
              strcmp (g_type_name (pspec->value_type),
                      "GladeStockImage") == 0 ||
              strcmp (g_type_name (pspec->value_type),
                      "GladeGtkImageType") == 0 ||
              strcmp (g_type_name (pspec->value_type),
                      "GladeGtkButtonType") == 0 ||
              strcmp (g_type_name (pspec->value_type),
                      "GladeGnomeDruidPagePosition") == 0 ||
              strcmp (g_type_name (pspec->value_type),
                      "GladeGnomeIconListSelectionMode") == 0 ||
              strcmp (g_type_name (pspec->value_type),
                      "GladeGnomeMessageBoxType") == 0)
            continue;

          if ((enums ? G_TYPE_IS_ENUM (pspec->value_type) :
               G_TYPE_IS_FLAGS (pspec->value_type)) &&
              !g_list_find_custom (types,
                                   g_type_name (pspec->value_type),
                                   (GCompareFunc) find_by_type_name))
            types =
                g_list_prepend (types,
                                g_strdup (g_type_name
                                          (pspec->value_type)));

        }
    }
  g_list_free (adaptors);

  types = g_list_sort (types, (GCompareFunc) find_by_type_name);

  for (l = types; l; l = l->next)
    {
      gchar *type_name = l->data;

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, COLUMN_NAME, type_name, -1);
      g_free (type_name);
    }
  g_list_free (types);
}

static void
column_types_store_populate (GtkListStore * store)
{
  GtkTreeIter iter;
  gint i;
  GType types[] = {
    G_TYPE_CHAR,
    G_TYPE_UCHAR,
    G_TYPE_BOOLEAN,
    G_TYPE_INT,
    G_TYPE_UINT,
    G_TYPE_LONG,
    G_TYPE_ULONG,
    G_TYPE_INT64,
    G_TYPE_UINT64,
    G_TYPE_FLOAT,
    G_TYPE_DOUBLE,
    G_TYPE_STRING,
    G_TYPE_POINTER,
    G_TYPE_OBJECT,
    GDK_TYPE_PIXBUF
  };

  for (i = 0; i < sizeof (types) / sizeof (GType); i++)
    {
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          COLUMN_NAME, g_type_name (types[i]), -1);
    }

  column_types_store_populate_enums_flags (store, TRUE);
  column_types_store_populate_enums_flags (store, FALSE);
}

GList *
glade_column_list_copy (GList * list)
{
  GList *l, *retval = NULL;

  for (l = list; l; l = g_list_next (l))
    {
      GladeColumnType *data = l->data;
      GladeColumnType *new_data =
          glade_column_type_new (data->type_name, data->column_name);

      retval = g_list_prepend (retval, new_data);
    }

  return g_list_reverse (retval);
}

GladeColumnType *
glade_column_type_new (const gchar * type_name, const gchar * column_name)
{
  GladeColumnType *column = g_slice_new0 (GladeColumnType);

  column->type_name = g_strdup (type_name);
  column->column_name = g_strdup (column_name);

  return column;
}

void
glade_column_type_free (GladeColumnType * column)
{
  g_free (column->type_name);
  g_free (column->column_name);
  g_slice_free (GladeColumnType, column);
}

void
glade_column_list_free (GList * list)
{
  g_list_foreach (list, (GFunc) glade_column_type_free, NULL);
  g_list_free (list);
}

GladeColumnType *
glade_column_list_find_column (GList * list, const gchar * column_name)
{
  GladeColumnType *column = NULL, *iter;
  GList *l;

  for (l = g_list_first (list); l; l = l->next)
    {
      iter = l->data;
      if (strcmp (column_name, iter->column_name) == 0)
        {
          column = iter;
          break;
        }
    }

  return column;
}

GType
glade_column_type_list_get_type (void)
{
  static GType type_id = 0;

  if (!type_id)
    {
      type_id = g_boxed_type_register_static
          ("GladeColumnTypeList",
           (GBoxedCopyFunc) glade_column_list_copy,
           (GBoxedFreeFunc) glade_column_list_free);
    }
  return type_id;
}

/**************************** GladeEditorProperty *****************************/
struct _GladeEPropColumnTypes
{
  GladeEditorProperty parent_instance;

  GtkListStore *store;
  GtkTreeView *view;
  GtkTreeSelection *selection;

  GladeNameContext *context;

  gboolean adding_column;
  gboolean want_focus;
  gboolean setting_cursor;

  GtkTreeViewColumn *name_column;
  GtkTreeViewColumn *type_column;
};

GLADE_MAKE_EPROP (GladeEPropColumnTypes, glade_eprop_column_types, GLADE, EPROP_COLUMN_TYPES)

static void
glade_eprop_column_types_finalize (GObject * object)
{
  /* Chain up */
  GObjectClass *parent_class =
      g_type_class_peek_parent (G_OBJECT_GET_CLASS (object));

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gint
get_extra_column (GNode * data_tree, GList * columns)
{
  GladeModelData *data;
  GNode *iter;
  gint idx = -1;

  /* extra columns trail at the end so walk backwards... */
  for (iter = g_node_last_child (data_tree->children); iter; iter = iter->prev)
    {
      data = iter->data;

      if (!glade_column_list_find_column (columns, data->name))
        {
          idx = g_node_child_position (data_tree->children, iter);
          break;
        }

    }
  return idx;
}

static void
eprop_column_adjust_rows (GladeEditorProperty * eprop, GList * columns)
{
  GladeColumnType *column;
  GNode *data_tree = NULL;
  GladeProperty *property, *prop = glade_editor_property_get_property (eprop);  
  GladeWidget *widget = glade_property_get_widget (prop);
  GList *list;
  gint idx;

  property = glade_widget_get_property (widget, "data");
  glade_property_get (property, &data_tree);
  if (!data_tree)
    return;
  data_tree = glade_model_data_tree_copy (data_tree);

  /* Add mising columns and reorder... */
  for (list = g_list_last (columns); list; list = list->prev)
    {
      GType data_type;

      column = list->data;

      data_type = g_type_from_name (column->type_name);

      if ((idx =
           glade_model_data_column_index (data_tree, column->column_name)) < 0)
        {
          glade_model_data_insert_column (data_tree,
                                          data_type, column->column_name, 0);

        }
      else
        glade_model_data_reorder_column (data_tree, idx, 0);

    }

  /* Remove trailing obsolete */
  while ((idx = get_extra_column (data_tree, columns)) >= 0)
    glade_model_data_remove_column (data_tree, idx);

  glade_command_set_property (property, data_tree);
  glade_model_data_tree_free (data_tree);
}

static void
eprop_column_append (GladeEditorProperty *eprop,
                     const gchar         *type_name, 
                     const gchar         *column_name)
{
  GladeEPropColumnTypes *eprop_types = GLADE_EPROP_COLUMN_TYPES (eprop);
  GList *columns = NULL;
  GladeColumnType *data;
  GValue value = { 0, };
  GladeProperty *property;

  property = glade_editor_property_get_property (eprop);

  glade_property_get (property, &columns);
  if (columns)
    columns = glade_column_list_copy (columns);

  data = glade_column_type_new (type_name, column_name);

  columns = g_list_append (columns, data);

  eprop_types->adding_column = TRUE;
  glade_command_push_group (_("Setting columns on %s"),
                            glade_widget_get_name (glade_property_get_widget (property)));

  g_value_init (&value, GLADE_TYPE_COLUMN_TYPE_LIST);
  g_value_take_boxed (&value, columns);
  glade_editor_property_commit (eprop, &value);

  eprop_column_adjust_rows (eprop, columns);
  g_value_unset (&value);

  glade_command_pop_group ();

  eprop_types->adding_column = FALSE;
}

static gboolean
eprop_treeview_key_press (GtkWidget           *treeview,
                          GdkEventKey         *event, 
                          GladeEditorProperty *eprop)
{
  /* Remove from list and commit value, dont touch the liststore except in load() */
  GladeEPropColumnTypes *eprop_types = GLADE_EPROP_COLUMN_TYPES (eprop);
  GtkTreeIter iter;
  GList *columns = NULL;
  GladeColumnType *column;
  GValue value = { 0, };
  gchar *column_name;
  GladeProperty *property;

  property = glade_editor_property_get_property (eprop);

  if (event->keyval == GDK_KEY_Delete &&
      gtk_tree_selection_get_selected (eprop_types->selection, NULL, &iter))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (eprop_types->store), &iter,
                          COLUMN_COLUMN_NAME, &column_name, -1);

      /* Cant delete the symbolic "add new" row */
      if (!column_name)
        return TRUE;

      glade_property_get (property, &columns);
      if (columns)
        columns = glade_column_list_copy (columns);
      g_assert (columns);

      /* Find and remove the offending column... */
      column = glade_column_list_find_column (columns, column_name);
      g_assert (column);
      columns = g_list_remove (columns, column);
      glade_column_type_free (column);

      glade_command_push_group (_("Setting columns on %s"),
                                glade_widget_get_name (glade_property_get_widget (property)));

      eprop_types->want_focus = TRUE;

      g_value_init (&value, GLADE_TYPE_COLUMN_TYPE_LIST);
      g_value_take_boxed (&value, columns);
      glade_editor_property_commit (eprop, &value);

      eprop_column_adjust_rows (eprop, columns);
      g_value_unset (&value);
      glade_command_pop_group ();

      g_free (column_name);

      eprop_types->want_focus = FALSE;

    }

  return (event->keyval == GDK_KEY_Delete);
}

static gboolean
columns_changed_idle (GladeEditorProperty * eprop)
{
  GladeEPropColumnTypes *eprop_types = GLADE_EPROP_COLUMN_TYPES (eprop);
  GladeColumnType *column;
  GValue value = { 0, };
  GList *new_list = NULL, *columns = NULL, *list;
  GtkTreeIter iter;
  gchar *column_name;
  GladeProperty *property;

  property = glade_editor_property_get_property (eprop);

  glade_property_get (property, &columns);

  /* This can happen when the user performs DnD and there
   * are no columns yet */
  if (!columns)
    return FALSE;

  columns = glade_column_list_copy (columns);

  if (gtk_tree_model_get_iter_first
      (GTK_TREE_MODEL (eprop_types->store), &iter))
    {
      do
        {
          column_name = NULL;
          gtk_tree_model_get (GTK_TREE_MODEL (eprop_types->store), &iter,
                              COLUMN_COLUMN_NAME, &column_name, -1);
          if (!column_name)
            continue;

          column = glade_column_list_find_column (columns, column_name);
          g_assert (column);

          new_list = g_list_prepend (new_list, column);
          g_free (column_name);
        }
      while (gtk_tree_model_iter_next
             (GTK_TREE_MODEL (eprop_types->store), &iter));
    }

  /* any missing columns to free ? */
  for (list = columns; list; list = list->next)
    {
      if (!g_list_find (new_list, list->data))
        glade_column_type_free ((GladeColumnType *) list->data);
    }
  g_list_free (columns);

  glade_command_push_group (_("Setting columns on %s"),
                            glade_widget_get_name (glade_property_get_widget (property)));

  g_value_init (&value, GLADE_TYPE_COLUMN_TYPE_LIST);
  g_value_take_boxed (&value, g_list_reverse (new_list));
  glade_editor_property_commit (eprop, &value);

  eprop_column_adjust_rows (eprop, new_list);
  g_value_unset (&value);
  glade_command_pop_group ();

  return FALSE;
}

static void
eprop_treeview_row_deleted (GtkTreeModel * tree_model,
                            GtkTreePath * path, GladeEditorProperty * eprop)
{
  if (glade_editor_property_loading (eprop))
    return;

  g_idle_add ((GSourceFunc) columns_changed_idle, eprop);
}

static void
eprop_column_add_new (GladeEPropColumnTypes * eprop_types)
{
  gtk_list_store_insert_with_values (eprop_types->store, NULL, -1,
                                     COLUMN_NAME, _("< define a new column >"),
                                     COLUMN_TYPE_EDITABLE, TRUE,
                                     COLUMN_NAME_EDITABLE, FALSE,
                                     COLUMN_TYPE_FOREGROUND, "Gray",
                                     COLUMN_TYPE_STYLE, PANGO_STYLE_ITALIC, -1);
}

static void
eprop_column_load (GladeEPropColumnTypes * eprop_types,
                   const gchar * type_name, const gchar * column_name)
{
  gtk_list_store_insert_with_values (eprop_types->store, NULL, -1,
                                     COLUMN_NAME, type_name,
                                     COLUMN_COLUMN_NAME, column_name,
                                     COLUMN_TYPE_EDITABLE, FALSE,
                                     COLUMN_NAME_EDITABLE, TRUE,
                                     COLUMN_TYPE_FOREGROUND, "Black",
                                     COLUMN_TYPE_STYLE, PANGO_STYLE_NORMAL, -1);
}


static void
eprop_types_focus_cell (GladeEPropColumnTypes * eprop_types, gboolean use_path,
                        gboolean add_cell, gboolean edit_cell)
{
  /* Focus and edit the first column of a newly added row */
  if (eprop_types->store)
    {
      GtkTreePath *new_item_path;
      GtkTreeIter iter;
      gint n_children;
      gint needed_row;

      n_children =
          gtk_tree_model_iter_n_children (GTK_TREE_MODEL (eprop_types->store),
                                          NULL);

      needed_row = n_children - (add_cell ? 1 : 2);

      if (use_path)
        new_item_path = gtk_tree_path_new_from_string
            (g_object_get_data (G_OBJECT (eprop_types), "current-path-str"));
      else if (gtk_tree_model_iter_nth_child
               (GTK_TREE_MODEL (eprop_types->store), &iter, NULL, needed_row))
        new_item_path =
            gtk_tree_model_get_path (GTK_TREE_MODEL (eprop_types->store),
                                     &iter);
      else
        return;

      eprop_types->setting_cursor = TRUE;

      gtk_widget_grab_focus (GTK_WIDGET (eprop_types->view));
      gtk_tree_view_expand_to_path (eprop_types->view, new_item_path);

      gtk_tree_view_set_cursor (eprop_types->view, new_item_path,
                                add_cell ? eprop_types->
                                type_column : eprop_types->name_column,
                                edit_cell);

      eprop_types->setting_cursor = FALSE;

      gtk_tree_path_free (new_item_path);
    }
}

static gboolean
eprop_types_focus_new (GladeEPropColumnTypes * eprop_types)
{
  eprop_types_focus_cell (eprop_types, FALSE, TRUE, FALSE);
  return FALSE;
}

static gboolean
eprop_types_focus_name (GladeEPropColumnTypes * eprop_types)
{
  eprop_types_focus_cell (eprop_types, FALSE, FALSE, TRUE);
  return FALSE;
}

static gboolean
eprop_types_focus_name_no_edit (GladeEPropColumnTypes * eprop_types)
{
  eprop_types_focus_cell (eprop_types, TRUE, FALSE, FALSE);
  return FALSE;
}

static void
glade_eprop_column_types_load (GladeEditorProperty * eprop,
                               GladeProperty * property)
{
  GladeEditorPropertyClass *parent_class =
      g_type_class_peek_parent (GLADE_EDITOR_PROPERTY_GET_CLASS (eprop));
  GladeEPropColumnTypes *eprop_types = GLADE_EPROP_COLUMN_TYPES (eprop);
  GList *l, *list = NULL;

  /* Chain up first */
  parent_class->load (eprop, property);

  if (eprop_types->context)
    glade_name_context_destroy (eprop_types->context);
  eprop_types->context = NULL;

  if (!property)
    return;

  eprop_types->context = glade_name_context_new ();

  g_signal_handlers_block_by_func (G_OBJECT (eprop_types->store),
                                   eprop_treeview_row_deleted, eprop);

  /* Clear Store */
  gtk_list_store_clear (eprop_types->store);

  glade_property_get (property, &list);

  for (l = list; l; l = g_list_next (l))
    {
      GladeColumnType *data = l->data;

      eprop_column_load (eprop_types, data->type_name, data->column_name);
      glade_name_context_add_name (eprop_types->context, data->column_name);
    }

  eprop_column_add_new (eprop_types);

  if (eprop_types->adding_column && list)
    g_idle_add ((GSourceFunc) eprop_types_focus_name, eprop_types);
  else if (eprop_types->want_focus)
    g_idle_add ((GSourceFunc) eprop_types_focus_new, eprop_types);

  g_signal_handlers_unblock_by_func (G_OBJECT (eprop_types->store),
                                     eprop_treeview_row_deleted, eprop);
}

static void
column_name_edited (GtkCellRendererText * cell,
                    const gchar * path,
                    const gchar * new_column_name, GladeEditorProperty * eprop)
{
  GladeEPropColumnTypes *eprop_types = GLADE_EPROP_COLUMN_TYPES (eprop);
  GtkTreeIter iter;
  gchar *old_column_name = NULL, *column_name;
  GList *columns = NULL;
  GladeColumnType *column;
  GValue value = { 0, };
  GNode *data_tree = NULL;
  GladeProperty *property, *prop;

  prop = glade_editor_property_get_property (eprop);

  if (eprop_types->adding_column)
    return;

  if (!gtk_tree_model_get_iter_from_string
      (GTK_TREE_MODEL (eprop_types->store), &iter, path))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (eprop_types->store), &iter,
                      COLUMN_COLUMN_NAME, &old_column_name, -1);

  if (new_column_name && old_column_name &&
      strcmp (new_column_name, old_column_name) == 0)
    return;

  /* Attempt to rename the column, and commit if successfull... */
  glade_property_get (prop, &columns);
  if (columns)
    columns = glade_column_list_copy (columns);
  g_assert (columns);

  column = glade_column_list_find_column (columns, old_column_name);

  /* Bookkeep the exclusive names... */
  if (!new_column_name || !new_column_name[0] ||
      glade_name_context_has_name (eprop_types->context, new_column_name))
    column_name = glade_name_context_new_name (eprop_types->context,
                                               new_column_name &&
                                               new_column_name[0] ?
                                               new_column_name : "column");
  else
    column_name = g_strdup (new_column_name);

  glade_name_context_add_name (eprop_types->context, column_name);
  glade_name_context_release_name (eprop_types->context, old_column_name);

  /* Set real column name */
  g_free (column->column_name);
  column->column_name = column_name;

  /* The "columns" copy of this string doesnt last long... */
  column_name = g_strdup (column_name);

  glade_command_push_group (_("Setting columns on %s"),
                            glade_widget_get_name (glade_property_get_widget (prop)));

  eprop_types->want_focus = TRUE;

  g_value_init (&value, GLADE_TYPE_COLUMN_TYPE_LIST);
  g_value_take_boxed (&value, columns);
  glade_editor_property_commit (eprop, &value);
  g_value_unset (&value);

  property = glade_widget_get_property (glade_property_get_widget (prop), "data");
  glade_property_get (property, &data_tree);
  if (data_tree)
    {
      data_tree = glade_model_data_tree_copy (data_tree);
      glade_model_data_column_rename (data_tree, old_column_name, column_name);
      glade_command_set_property (property, data_tree);
      glade_model_data_tree_free (data_tree);
    }
  glade_command_pop_group ();

  eprop_types->want_focus = FALSE;

  g_free (old_column_name);
  g_free (column_name);
}


static void
column_type_edited (GtkCellRendererText * cell,
                    const gchar * path,
                    const gchar * type_name, GladeEditorProperty * eprop)
{
  GladeEPropColumnTypes *eprop_types = GLADE_EPROP_COLUMN_TYPES (eprop);
  GtkTreeIter iter;
  GladeProperty *property;
  gchar *column_name;

  if (!gtk_tree_model_get_iter_from_string
      (GTK_TREE_MODEL (eprop_types->store), &iter, path))
    return;

  property = glade_editor_property_get_property (eprop);

  if (type_name && type_name[0])
    {
      column_name =
          glade_name_context_new_name (eprop_types->context, type_name);
      eprop_column_append (eprop, type_name, column_name);
      g_free (column_name);
    }
  else
    {
      eprop_types->want_focus = TRUE;
      glade_editor_property_load (eprop, property);
      eprop_types->want_focus = FALSE;
    }
}


static void
types_combo_editing_started (GtkCellRenderer * renderer,
                             GtkCellEditable * editable,
                             gchar * path, GladeEditorProperty * eprop)
{
  GtkEntryCompletion *completion = gtk_entry_completion_new ();

  g_object_set_data_full (G_OBJECT (eprop), "current-path-str", g_strdup (path),
                          g_free);

  gtk_entry_completion_set_model (completion, types_model);
  gtk_entry_completion_set_text_column (completion, 0);
  gtk_entry_completion_set_inline_completion (completion, TRUE);
  gtk_entry_set_completion (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (editable))),
                            completion);
  g_object_unref (G_OBJECT (completion));
}

static void
types_combo_editing_canceled (GtkCellRenderer * renderer,
                              GladeEditorProperty * eprop)
{
  g_idle_add ((GSourceFunc) eprop_types_focus_new, eprop);
}


static void
types_name_editing_started (GtkCellRenderer * renderer,
                            GtkCellEditable * editable,
                            gchar * path_str, GladeEditorProperty * eprop)
{
  g_object_set_data_full (G_OBJECT (eprop), "current-path-str",
                          g_strdup (path_str), g_free);
}

static void
types_name_editing_canceled (GtkCellRenderer * renderer,
                             GladeEditorProperty * eprop)
{
  GladeEPropColumnTypes *eprop_types = GLADE_EPROP_COLUMN_TYPES (eprop);

  if (eprop_types->adding_column || eprop_types->setting_cursor)
    return;

  g_idle_add ((GSourceFunc) eprop_types_focus_name_no_edit, eprop);
}

static GtkWidget *
glade_eprop_column_types_create_input (GladeEditorProperty * eprop)
{
  GladeEPropColumnTypes *eprop_types = GLADE_EPROP_COLUMN_TYPES (eprop);
  GtkWidget *vbox, *swin, *label;
  GtkCellRenderer *cell;
  gchar *string;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);

/*  hbox = gtk_hbox_new (FALSE, 4); */

  if (!types_model)
    {
      /* We make sure to do this after all the adaptors are parsed 
       * because we load the enums/flags from the adaptors
       */
      types_model = (GtkTreeModel *) gtk_list_store_new (1, G_TYPE_STRING);

      column_types_store_populate (GTK_LIST_STORE (types_model));
    }


  string = g_strdup_printf ("<b>%s</b>", _("Add and remove columns:"));
  label = gtk_label_new (string);
  g_free (string);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);

  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin),
                                       GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_box_pack_start (GTK_BOX (vbox), swin, TRUE, TRUE, 0);

  eprop_types->store = gtk_list_store_new (N_COLUMNS,
                                           G_TYPE_STRING,
                                           G_TYPE_STRING,
                                           G_TYPE_BOOLEAN,
                                           G_TYPE_BOOLEAN,
                                           G_TYPE_STRING, PANGO_TYPE_STYLE);

  g_signal_connect (eprop_types->store, "row-deleted",
                    G_CALLBACK (eprop_treeview_row_deleted), eprop);

  eprop_types->view =
      (GtkTreeView *)
      gtk_tree_view_new_with_model (GTK_TREE_MODEL (eprop_types->store));
  eprop_types->selection = gtk_tree_view_get_selection (eprop_types->view);

  gtk_tree_view_set_reorderable (eprop_types->view, TRUE);
  gtk_tree_view_set_enable_search (eprop_types->view, FALSE);
  //gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);

  g_signal_connect (eprop_types->view, "key-press-event",
                    G_CALLBACK (eprop_treeview_key_press), eprop);


  /* type column */
  cell = gtk_cell_renderer_combo_new ();
  g_object_set (G_OBJECT (cell),
                "text-column", COLUMN_NAME, "model", types_model, NULL);

  g_signal_connect (G_OBJECT (cell), "editing-started",
                    G_CALLBACK (types_combo_editing_started), eprop);

  g_signal_connect (G_OBJECT (cell), "editing-canceled",
                    G_CALLBACK (types_combo_editing_canceled), eprop);

  g_signal_connect (G_OBJECT (cell), "edited",
                    G_CALLBACK (column_type_edited), eprop);

  eprop_types->type_column =
      gtk_tree_view_column_new_with_attributes (_("Column type"), cell,
                                                "foreground",
                                                COLUMN_TYPE_FOREGROUND, "style",
                                                COLUMN_TYPE_STYLE, "editable",
                                                COLUMN_TYPE_EDITABLE, "text",
                                                COLUMN_NAME, NULL);

  gtk_tree_view_column_set_expand (eprop_types->type_column, TRUE);
  gtk_tree_view_append_column (eprop_types->view, eprop_types->type_column);

  /* name column */
  cell = gtk_cell_renderer_text_new ();
  g_signal_connect (G_OBJECT (cell), "edited",
                    G_CALLBACK (column_name_edited), eprop);

  g_signal_connect (G_OBJECT (cell), "editing-started",
                    G_CALLBACK (types_name_editing_started), eprop);

  g_signal_connect (G_OBJECT (cell), "editing-canceled",
                    G_CALLBACK (types_name_editing_canceled), eprop);

  eprop_types->name_column =
      gtk_tree_view_column_new_with_attributes (_("Column name"), cell,
                                                "editable",
                                                COLUMN_NAME_EDITABLE, "text",
                                                COLUMN_COLUMN_NAME, NULL);

  gtk_tree_view_column_set_expand (eprop_types->name_column, TRUE);

  gtk_tree_view_append_column (eprop_types->view, eprop_types->name_column);
  gtk_container_add (GTK_CONTAINER (swin), GTK_WIDGET (eprop_types->view));

  g_object_set (G_OBJECT (vbox), "height-request", 200, NULL);

  gtk_widget_show_all (vbox);
  return vbox;
}
