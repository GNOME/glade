/*
 * glade-string-list.c - Editing support for lists of translatable strings
 *
 * Copyright (C) 2010 Openismus GmbH
 *
 * Author(s):
 *      Tristan Van Berkom <tvb@gnome.org>
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

#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gladeui/glade.h>
#include <glib/gi18n-lib.h>

#include "glade-string-list.h"


/**************************************************************
 *            GladeStringList boxed type stuff here           *
 **************************************************************/
static GladeString *
glade_string_new (const gchar *string,
                  const gchar *comment,
                  const gchar *context,
                  gboolean     translatable,
                  const gchar *id)
{
  GladeString *gstring = g_slice_new0 (GladeString);

  gstring->string       = g_strdup (string);
  gstring->comment      = g_strdup (comment);
  gstring->context      = g_strdup (context);
  gstring->translatable = translatable;
  gstring->id           = g_strdup (id);

  return gstring;
}

static GladeString *
glade_string_copy (GladeString *string)
{
  return glade_string_new (string->string, 
                           string->comment, 
                           string->context, 
                           string->translatable,
                           string->id);
}

static void
glade_string_free (GladeString *string)
{
  g_free (string->string);
  g_free (string->comment);
  g_free (string->context);
  g_free (string->id);
  g_slice_free (GladeString, string);
}

GList *
glade_string_list_append (GList       *list,
                          const gchar *string,
                          const gchar *comment,
                          const gchar *context,
                          gboolean     translatable,
                          const gchar *id)
{
  GladeString *gstring;

  gstring = glade_string_new (string, comment, context, translatable, id);

  return g_list_append (list, gstring);
}

GList *
glade_string_list_copy (GList *string_list)
{
  GList *ret = NULL, *list;
  GladeString *string, *copy;

  for (list = string_list; list; list = list->next)
    {
      string = list->data;

      copy = glade_string_copy (string);

      ret = g_list_prepend (ret, copy);
    }

  return g_list_reverse (ret);
}

void
glade_string_list_free (GList *string_list)
{
  g_list_foreach (string_list, (GFunc)glade_string_free, NULL);
  g_list_free (string_list);
}

GType
glade_string_list_get_type (void)
{
  static GType type_id = 0;

  if (!type_id)
    type_id = g_boxed_type_register_static
        ("GladeStringList",
         (GBoxedCopyFunc) glade_string_list_copy,
         (GBoxedFreeFunc) glade_string_list_free);
  return type_id;
}


gchar *
glade_string_list_to_string (GList *list)
{
  GString *string = g_string_new ("");
  GList *l;

  for (l = list; l; l = l->next)
    {
      GladeString *str = l->data;

      if (l != list)
        g_string_append_c (string, ',');

      g_string_append_printf (string, "%s:%s:%s:%d:%s", 
                              str->string,
                              str->comment ? str->comment : "",
                              str->context ? str->context : "",
                              str->translatable,
                              str->id ? str->id : "");
    }

  return g_string_free (string, FALSE);
}

/**************************************************************
 *              GladeEditorProperty stuff here
 **************************************************************/
struct _GladeEPropStringList
{
  GladeEditorProperty parent_instance;

  GtkTreeModel *model;
  GtkWidget    *view;

  guint  translatable : 1;
  guint  with_id : 1;
  guint  want_focus : 1;

  guint  editing_index;

  guint  changed_id;
  guint  update_id;
  GList *pending_string_list;
};

enum
{
  COLUMN_STRING,
  COLUMN_INDEX,
  COLUMN_DUMMY,
  COLUMN_ID,
  NUM_COLUMNS
};

GLADE_MAKE_EPROP (GladeEPropStringList, glade_eprop_string_list, GLADE, EPROP_STRING_LIST)

static void
glade_eprop_string_list_finalize (GObject *object)
{
  GladeEPropStringList *eprop_string_list = GLADE_EPROP_STRING_LIST (object);
  GObjectClass *parent_class =
      g_type_class_peek_parent (G_OBJECT_GET_CLASS (object));

  if (eprop_string_list->update_id)
    {
      g_source_remove (eprop_string_list->update_id);
      eprop_string_list->update_id = 0;
    }

  if (eprop_string_list->changed_id)
    {
      g_source_remove (eprop_string_list->changed_id);
      eprop_string_list->changed_id = 0;
    }

  if (eprop_string_list->pending_string_list)
    {
      glade_string_list_free (eprop_string_list->pending_string_list);
      eprop_string_list->pending_string_list = NULL;
    }

  /* Chain up */
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
update_string_list_idle (GladeEditorProperty *eprop)
{
  GladeEPropStringList *eprop_string_list = GLADE_EPROP_STRING_LIST (eprop);
  GValue value = { 0, };

  eprop_string_list->want_focus = TRUE;

  g_value_init (&value, GLADE_TYPE_STRING_LIST);
  g_value_take_boxed (&value, eprop_string_list->pending_string_list);
  glade_editor_property_commit (eprop, &value);
  g_value_unset (&value);

  eprop_string_list->want_focus = FALSE;

  eprop_string_list->pending_string_list = NULL;
  eprop_string_list->update_id = 0;
    
  return FALSE;
}

static gboolean
data_changed_idle (GladeEditorProperty *eprop)
{
  GladeEPropStringList *eprop_string_list = GLADE_EPROP_STRING_LIST (eprop);
  GladeProperty        *property = glade_editor_property_get_property (eprop);
  GladeString          *string, *copy;
  GList                *string_list = NULL;
  GList                *new_list = NULL;
  GtkTreeIter           iter;
  guint                 index;

  /* Create a new list based on the order and contents
   * of the model state */
  glade_property_get (property, &string_list);

  if (gtk_tree_model_get_iter_first (eprop_string_list->model, &iter))
    {
      do
        {
          gtk_tree_model_get (eprop_string_list->model, &iter,
                              COLUMN_INDEX, &index, -1);

          if ((string = g_list_nth_data (string_list, index)) != NULL)
            {
              copy = glade_string_copy (string);
              new_list = g_list_prepend (new_list, copy);
            }
        }
      while (gtk_tree_model_iter_next (eprop_string_list->model, &iter));
    }

  new_list = g_list_reverse (new_list);

  if (eprop_string_list->pending_string_list)
    glade_string_list_free (eprop_string_list->pending_string_list);
  eprop_string_list->pending_string_list = new_list;

  /* We're already in an idle, just call it directly here */
  update_string_list_idle (eprop);

  eprop_string_list->changed_id = 0;
  return FALSE;
}

static void
row_deleted (GtkTreeModel        *tree_model,
             GtkTreePath         *path,
             GladeEditorProperty *eprop)
{
  GladeEPropStringList *eprop_string_list = GLADE_EPROP_STRING_LIST (eprop);

  if (glade_editor_property_loading (eprop))
    return;

  eprop_string_list->editing_index = 0;

  if (eprop_string_list->changed_id == 0)
    eprop_string_list->changed_id = 
      g_idle_add ((GSourceFunc) data_changed_idle, eprop);
}

static void
glade_eprop_string_list_load (GladeEditorProperty *eprop, GladeProperty *property)
{
  GladeEPropStringList *eprop_string_list = GLADE_EPROP_STRING_LIST (eprop);
  GladeEditorPropertyClass *parent_class =
      g_type_class_peek_parent (G_OBJECT_GET_CLASS (eprop));
  GList *string_list, *list;
  GtkTreeIter iter;
  guint i;

  g_signal_handlers_block_by_func (eprop_string_list->model, row_deleted, eprop);
  gtk_list_store_clear (GTK_LIST_STORE (eprop_string_list->model));
  g_signal_handlers_unblock_by_func (eprop_string_list->model, row_deleted, eprop);

  parent_class->load (eprop, property);

  if (!property)
    return;

  glade_property_get (property, &string_list);

  for (list = string_list, i = 0; list; list = list->next, i++)
    {
      GladeString *string = list->data;

      gtk_list_store_append (GTK_LIST_STORE (eprop_string_list->model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (eprop_string_list->model), &iter,
                          COLUMN_STRING, string->string,
                          COLUMN_INDEX, i,
                          COLUMN_DUMMY, FALSE,
                          COLUMN_ID, string->id,
                          -1);
    }

  gtk_list_store_append (GTK_LIST_STORE (eprop_string_list->model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (eprop_string_list->model), &iter,
                      COLUMN_STRING, _("<Type Here>"),
                      COLUMN_INDEX, i,
                      COLUMN_DUMMY, TRUE,
                      COLUMN_ID, NULL,
                      -1);

  if (eprop_string_list->want_focus)
    {
      GtkTreePath *path = gtk_tree_path_new_from_indices (eprop_string_list->editing_index, -1);
      GtkTreeViewColumn *column = gtk_tree_view_get_column (GTK_TREE_VIEW (eprop_string_list->view), 0);

      gtk_widget_grab_focus (eprop_string_list->view);
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (eprop_string_list->view), path, column, FALSE);

      gtk_tree_path_free (path);
    }
}

static void
string_edited (GtkCellRendererText *renderer,
               gchar               *path,
               gchar               *new_text,
               GladeEditorProperty *eprop)
{
  GladeEPropStringList *eprop_string_list = GLADE_EPROP_STRING_LIST (eprop);
  GtkTreePath          *tree_path = gtk_tree_path_new_from_string (path);
  GtkTreeIter           iter;
  gboolean              dummy;
  guint                 index;
  GladeProperty        *property = glade_editor_property_get_property (eprop);
  GList                *string_list = NULL;

  gtk_tree_model_get_iter (eprop_string_list->model, &iter, tree_path);
  gtk_tree_model_get (eprop_string_list->model, &iter,
                      COLUMN_INDEX, &index,
                      COLUMN_DUMMY, &dummy,
                      -1);

  glade_property_get (property, &string_list);

  if (string_list)
    string_list = glade_string_list_copy (string_list);

  if (dummy)
    {
      if (new_text && new_text[0] && strcmp (new_text, _("<Type Here>")) != 0)
        string_list = 
          glade_string_list_append (string_list,
                                    new_text, NULL, NULL, 
                                    eprop_string_list->translatable,
                                    NULL);
    }
  else if (new_text && new_text[0])
    {
      GladeString *string = 
        g_list_nth_data (string_list, index);

      g_free (string->string);
      string->string = g_strdup (new_text);
    }
  else
    {
      GList *node = g_list_nth (string_list, index);
      glade_string_free (node->data);
      string_list = 
        g_list_delete_link (string_list, node);
    }

  eprop_string_list->editing_index = index;

  if (eprop_string_list->pending_string_list)
    glade_string_list_free (eprop_string_list->pending_string_list);
  eprop_string_list->pending_string_list = string_list;

  if (eprop_string_list->update_id == 0)
    eprop_string_list->update_id = 
      g_idle_add ((GSourceFunc) update_string_list_idle, eprop);

  gtk_tree_path_free (tree_path);
}

static void
id_edited (GtkCellRendererText *renderer,
           gchar               *path,
           gchar               *new_text,
           GladeEditorProperty *eprop)
{
  GladeEPropStringList *eprop_string_list = GLADE_EPROP_STRING_LIST (eprop);
  GtkTreePath          *tree_path = gtk_tree_path_new_from_string (path);
  GtkTreeIter           iter;
  guint                 index;
  GladeProperty        *property = glade_editor_property_get_property (eprop);
  GList                *string_list = NULL;
  GladeString          *string;

  gtk_tree_model_get_iter (eprop_string_list->model, &iter, tree_path);
  gtk_tree_model_get (eprop_string_list->model, &iter,
                      COLUMN_INDEX, &index,
                      -1);

  glade_property_get (property, &string_list);

  if (string_list)
    string_list = glade_string_list_copy (string_list);

  string = g_list_nth_data (string_list, index);

  g_free (string->id);

  if (new_text && new_text[0])
    string->id = g_strdup (new_text);
  else
    string->id = NULL;

  eprop_string_list->editing_index = index;

  if (eprop_string_list->pending_string_list)
    glade_string_list_free (eprop_string_list->pending_string_list);
  eprop_string_list->pending_string_list = string_list;

  if (eprop_string_list->update_id == 0)
    eprop_string_list->update_id = 
      g_idle_add ((GSourceFunc) update_string_list_idle, eprop);

  gtk_tree_path_free (tree_path);
}

static void
i18n_icon_activate (GtkCellRenderer     *renderer,
                    const gchar         *path,
                    GladeEditorProperty *eprop)
{
  GladeEPropStringList *eprop_string_list = GLADE_EPROP_STRING_LIST (eprop);
  GtkTreePath          *tree_path = gtk_tree_path_new_from_string (path);
  GtkTreeIter           iter;
  guint                 index;
  GladeProperty        *property = glade_editor_property_get_property (eprop);
  GList                *string_list = NULL;
  GladeString          *string;

  gtk_tree_model_get_iter (eprop_string_list->model, &iter, tree_path);
  gtk_tree_model_get (eprop_string_list->model, &iter,
                      COLUMN_INDEX, &index,
                      -1);

  glade_property_get (property, &string_list);
  string_list = glade_string_list_copy (string_list);

  string = g_list_nth_data (string_list, index);

  if (glade_editor_property_show_i18n_dialog (NULL,
                                              &string->string,
                                              &string->context,
                                              &string->comment,
                                              &string->translatable))
    {
      eprop_string_list->editing_index = index;

      if (eprop_string_list->pending_string_list)
        glade_string_list_free (eprop_string_list->pending_string_list);
      eprop_string_list->pending_string_list = string_list;

      if (eprop_string_list->update_id == 0)
        eprop_string_list->update_id = 
          g_idle_add ((GSourceFunc) update_string_list_idle, eprop);
    }
  else
    glade_string_list_free (string_list);

  gtk_tree_path_free (tree_path);
}

static void
cell_data_func (GtkTreeViewColumn   *column,
                GtkCellRenderer     *renderer,
                GtkTreeModel        *model,
                GtkTreeIter         *iter,
                GladeEditorProperty *eprop)
{
  GladeEPropStringList *eprop_string_list = GLADE_EPROP_STRING_LIST (eprop);
  gboolean dummy;
  GdkRGBA  color;

  gtk_tree_model_get (model, iter, COLUMN_DUMMY, &dummy, -1);

  if (GTK_IS_CELL_RENDERER_TEXT (renderer))
    {
      GtkStyleContext* context = gtk_widget_get_style_context (eprop_string_list->view);

      if (dummy)
        {
          gtk_style_context_save (context);
          gtk_style_context_set_state (context, gtk_style_context_get_state (context) | GTK_STATE_FLAG_INSENSITIVE);
          gtk_style_context_get_color (context, gtk_style_context_get_state (context), &color);
          gtk_style_context_restore (context);
          g_object_set (renderer, 
                        "style", PANGO_STYLE_ITALIC,
                        "foreground-rgba", &color,
                        NULL);
        }
      else
        {
          gtk_style_context_get_color (context, gtk_style_context_get_state (context), &color);
          g_object_set (renderer,
                        "style", PANGO_STYLE_NORMAL,
                        "foreground-rgba", &color,
                        NULL);
        }
    }
  else if (GLADE_IS_CELL_RENDERER_ICON (renderer))
    g_object_set (renderer, "visible", !dummy && eprop_string_list->translatable, NULL);
}

static void
id_cell_data_func (GtkTreeViewColumn   *column,
                   GtkCellRenderer     *renderer,
                   GtkTreeModel        *model,
                   GtkTreeIter         *iter,
                   GladeEditorProperty *eprop)
{
  GladeEPropStringList *eprop_string_list = GLADE_EPROP_STRING_LIST (eprop);

  if (eprop_string_list->with_id)
    {
      GtkStyleContext* context = gtk_widget_get_style_context (eprop_string_list->view);
      GdkRGBA  color;
      guint index;
      gboolean dummy;
      gchar *id = NULL;

      gtk_tree_model_get (eprop_string_list->model, iter,
                          COLUMN_INDEX, &index,
                          COLUMN_DUMMY, &dummy,
                          COLUMN_ID, &id,
                          -1);

      /* Dummy, no data yet */
      if (dummy)
        {
          g_object_set (renderer,
                        "editable", FALSE,
                        "text", NULL,
                        NULL);
        }
      /* Not dummy, and id already set */
      else if (id)
        {
          gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &color);
          g_object_set (renderer,
                        "style", PANGO_STYLE_NORMAL,
                        "foreground-rgba", &color,
                        "editable", TRUE,
                        "text", id,
                        NULL);
        }
      /* Not dummy, but no id yet */
      else
        {
          gtk_style_context_get_color (context, GTK_STATE_FLAG_INSENSITIVE, &color);
          g_object_set (renderer, 
                        "style", PANGO_STYLE_ITALIC,
                        "foreground-rgba", &color,
                        "editable", TRUE,
                        "text", _("<Enter ID>"),
                        NULL);
        }

      g_free (id);
    }
  else
    g_object_set (renderer, "visible", FALSE, NULL);

}

static gboolean
treeview_key_press (GtkWidget           *treeview,
                    GdkEventKey         *event, 
                    GladeEditorProperty *eprop)
{

  /* Delete rows from the store, this will trigger "row-deleted" which will
   * handle the property update in an idle handler */
  if (event->keyval == GDK_KEY_Delete)
    {
      GladeEPropStringList *eprop_string_list = GLADE_EPROP_STRING_LIST (eprop);
      GtkTreeSelection *selection;
      GtkTreeIter iter;
      GList *selected_rows, *l;

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

      if ((selected_rows = 
           gtk_tree_selection_get_selected_rows (selection, NULL)) != NULL)
        {
          for (l = selected_rows; l; l = l->next)
            {
              GtkTreePath *path = l->data;

              if (gtk_tree_model_get_iter (eprop_string_list->model, &iter, path))
                gtk_list_store_remove (GTK_LIST_STORE (eprop_string_list->model), &iter);
            }

          g_list_foreach (selected_rows, (GFunc)gtk_tree_path_free, NULL);
          g_list_free (selected_rows);
        }
      return TRUE;
    }

  return FALSE;
}

static gint
get_tree_view_height (void)
{
  static gint height = -1;

  if (height < 0)
    {
      GtkWidget *label = gtk_label_new (NULL);
      PangoLayout *layout =
        gtk_widget_create_pango_layout (label, 
                                        "The quick\n"
                                        "brown fox\n"
                                        "jumped\n"
                                        "over\n"
                                        "the lazy\n"
                                        "dog");

      pango_layout_get_pixel_size (layout, NULL, &height);

      g_object_unref (layout);
      g_object_ref_sink (label);
      g_object_unref (label);
    }

  return height;
}

static GtkWidget *
glade_eprop_string_list_create_input (GladeEditorProperty *eprop)
{
  GladeEPropStringList *eprop_string_list = GLADE_EPROP_STRING_LIST (eprop);
  GtkTreeViewColumn    *column;
  GtkCellRenderer      *renderer;
  GtkWidget            *swindow;

  eprop_string_list->view = gtk_tree_view_new ();
  column                  = gtk_tree_view_column_new ();

  /* Text renderer */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), 
                "editable", TRUE,
                "ellipsize", PANGO_ELLIPSIZE_END,
                NULL);
  g_signal_connect (G_OBJECT (renderer), "edited", G_CALLBACK (string_edited), eprop);

  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer, "text", COLUMN_STRING, NULL);
  gtk_tree_view_column_set_cell_data_func (column, renderer,
                                           (GtkTreeCellDataFunc)cell_data_func,
                                           eprop, NULL);

  /* "id" renderer */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), 
                "editable", TRUE,
                "ellipsize", PANGO_ELLIPSIZE_END,
                NULL);
  g_signal_connect (G_OBJECT (renderer), "edited", G_CALLBACK (id_edited), eprop);

  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func (column, renderer,
                                           (GtkTreeCellDataFunc)id_cell_data_func,
                                           eprop, NULL);

  /* i18n icon renderer */
  renderer = glade_cell_renderer_icon_new ();
  g_object_set (G_OBJECT (renderer), "icon-name", "gtk-edit", NULL);
  g_signal_connect (G_OBJECT (renderer), "activate",
                    G_CALLBACK (i18n_icon_activate), eprop);

  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_cell_data_func (column, renderer,
                                           (GtkTreeCellDataFunc)cell_data_func,
                                           eprop, NULL);

  eprop_string_list->model = (GtkTreeModel *)gtk_list_store_new (NUM_COLUMNS,
                                                                 G_TYPE_STRING,
                                                                 G_TYPE_UINT,
                                                                 G_TYPE_BOOLEAN,
                                                                 G_TYPE_STRING);

  g_signal_connect (G_OBJECT (eprop_string_list->model), "row-deleted",
                    G_CALLBACK (row_deleted), eprop);

  gtk_tree_view_append_column (GTK_TREE_VIEW (eprop_string_list->view), column);

  gtk_tree_view_set_model (GTK_TREE_VIEW (eprop_string_list->view),
                           eprop_string_list->model);

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (eprop_string_list->view), FALSE);
  gtk_tree_view_set_reorderable (GTK_TREE_VIEW (eprop_string_list->view), TRUE);

  g_signal_connect (eprop_string_list->view, "key-press-event",
                    G_CALLBACK (treeview_key_press), eprop);

  swindow = gtk_scrolled_window_new (NULL, NULL);

  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (swindow), get_tree_view_height ());
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (swindow), eprop_string_list->view);

  gtk_widget_set_hexpand (swindow, TRUE);

  gtk_widget_show (eprop_string_list->view);
  gtk_widget_show (swindow);

  return swindow;
}

GladeEditorProperty *
glade_eprop_string_list_new (GladePropertyDef   *pdef,
                             gboolean            use_command,
                             gboolean            translatable,
                             gboolean            with_id)
{
  GladeEditorProperty *eprop = 
    g_object_new (GLADE_TYPE_EPROP_STRING_LIST, 
                  "property-def", pdef,
                  "use-command", use_command,
                  NULL);

  GladeEPropStringList *eprop_string_list = GLADE_EPROP_STRING_LIST (eprop);

  eprop_string_list->translatable = translatable;
  eprop_string_list->with_id = with_id;

  return eprop;
}
