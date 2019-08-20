/*
 * Copyright (C) 2008 Tristan Van Berkom.
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
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#include <config.h>

#include <gladeui/glade.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include "glade-icon-sources.h"

static GList *
icon_set_copy (GList *set)
{
  GList *dup_set = NULL, *l;
  GtkIconSource *source;

  for (l = set; l; l = l->next)
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      source = gtk_icon_source_copy ((GtkIconSource *) l->data);
G_GNUC_END_IGNORE_DEPRECATIONS
      dup_set = g_list_prepend (dup_set, source);
    }
  return g_list_reverse (dup_set);
}


static void
icon_set_free (GList *list)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  g_list_foreach (list, (GFunc) gtk_icon_source_free, NULL);
G_GNUC_END_IGNORE_DEPRECATIONS
  g_list_free (list);
}


GladeIconSources *
glade_icon_sources_new (void)
{
  GladeIconSources *sources = g_new0 (GladeIconSources, 1);

  sources->sources = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            (GDestroyNotify) g_free,
                                            (GDestroyNotify) icon_set_free);
  return sources;
}


static void
icon_sources_dup (gchar *icon_name, GList *set, GladeIconSources *dup)
{
  GList *dup_set = icon_set_copy (set);
  g_hash_table_insert (dup->sources, g_strdup (icon_name), dup_set);
}

GladeIconSources *
glade_icon_sources_copy (GladeIconSources *sources)
{
  if (!sources)
    return NULL;

  GladeIconSources *dup = glade_icon_sources_new ();

  g_hash_table_foreach (sources->sources, (GHFunc) icon_sources_dup, dup);

  return dup;
}

void
glade_icon_sources_free (GladeIconSources *sources)
{
  if (sources)
    {
      g_hash_table_destroy (sources->sources);
      g_free (sources);
    }
}

GType
glade_icon_sources_get_type (void)
{
  static GType type_id = 0;

  if (!type_id)
    type_id = g_boxed_type_register_static
        ("GladeIconSources",
         (GBoxedCopyFunc) glade_icon_sources_copy,
         (GBoxedFreeFunc) glade_icon_sources_free);
  return type_id;
}

/**************************** GladeEditorProperty *****************************/
enum
{
  COLUMN_TEXT,                  /* Used for display/editing purposes */
  COLUMN_TEXT_WEIGHT,           /* Whether the text is bold (icon-name parent rows) */
  COLUMN_TEXT_EDITABLE,         /* parent icon-name displays are not editable */
  COLUMN_ICON_NAME,             /* store the icon name regardless */
  COLUMN_LIST_INDEX,            /* denotes the position in the GList of the actual property value (or -1) */
  COLUMN_DIRECTION_ACTIVE,
  COLUMN_DIRECTION,
  COLUMN_SIZE_ACTIVE,
  COLUMN_SIZE,
  COLUMN_STATE_ACTIVE,
  COLUMN_STATE,
  NUM_COLUMNS
};

struct _GladeEPropIconSources
{
  GladeEditorProperty parent_instance;

  GtkTreeView *view;
  GtkTreeStore *store;
  GtkTreeViewColumn *filename_column;
  GtkWidget *combo;
};

GLADE_MAKE_EPROP (GladeEPropIconSources, glade_eprop_icon_sources, GLADE, EPROP_ICON_SOURCES)

static void
glade_eprop_icon_sources_finalize (GObject *object)
{
  /* Chain up */
  GObjectClass *parent_class =
      g_type_class_peek_parent (G_OBJECT_GET_CLASS (object));
  //GladeEPropIconSources *eprop_sources = GLADE_EPROP_ICON_SOURCES (object);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
populate_store_foreach (const gchar           *icon_name,
                        GList                 *sources,
                        GladeEPropIconSources *eprop_sources)
{
  GtkIconSource *source;
  GtkTreeIter parent_iter, iter;
  GList *l;

  /* Update the comboboxentry's store here... */
  gtk_combo_box_text_insert (GTK_COMBO_BOX_TEXT (eprop_sources->combo), -1, icon_name, icon_name);
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (eprop_sources->combo), icon_name);

  /* Dont set COLUMN_ICON_NAME here */
  gtk_tree_store_append (eprop_sources->store, &parent_iter, NULL);
  gtk_tree_store_set (eprop_sources->store, &parent_iter,
                      COLUMN_TEXT, icon_name,
                      COLUMN_TEXT_EDITABLE, FALSE,
                      COLUMN_TEXT_WEIGHT, PANGO_WEIGHT_BOLD, -1);

  for (l = sources; l; l = l->next)
    {
      GdkPixbuf *pixbuf;
      gchar *str;

      source = l->data;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      pixbuf = gtk_icon_source_get_pixbuf (source);
G_GNUC_END_IGNORE_DEPRECATIONS
      str = g_object_get_data (G_OBJECT (pixbuf), "GladeFileName");

      gtk_tree_store_append (eprop_sources->store, &iter, &parent_iter);
      gtk_tree_store_set (eprop_sources->store, &iter,
                          COLUMN_ICON_NAME, icon_name,
                          COLUMN_LIST_INDEX, g_list_index (sources, source),
                          COLUMN_TEXT, str,
                          COLUMN_TEXT_EDITABLE, TRUE,
                          COLUMN_TEXT_WEIGHT, PANGO_WEIGHT_NORMAL, -1);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      if (!gtk_icon_source_get_direction_wildcarded (source))
        {
          GtkTextDirection direction = gtk_icon_source_get_direction (source);
G_GNUC_END_IGNORE_DEPRECATIONS
          str =
              glade_utils_enum_string_from_value_displayable
              (GTK_TYPE_TEXT_DIRECTION, direction);
          gtk_tree_store_set (eprop_sources->store, &iter,
                              COLUMN_DIRECTION_ACTIVE, TRUE, COLUMN_DIRECTION,
                              str, -1);
          g_free (str);
        }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      if (!gtk_icon_source_get_size_wildcarded (source))
        {
          GtkIconSize size = gtk_icon_source_get_size (source);
G_GNUC_END_IGNORE_DEPRECATIONS
          str =
              glade_utils_enum_string_from_value_displayable
              (GTK_TYPE_ICON_SIZE, size);
          gtk_tree_store_set (eprop_sources->store, &iter, COLUMN_SIZE_ACTIVE,
                              TRUE, COLUMN_SIZE, str, -1);
          g_free (str);
        }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      if (!gtk_icon_source_get_state_wildcarded (source))
        {
          GtkStateType state = gtk_icon_source_get_state (source);
G_GNUC_END_IGNORE_DEPRECATIONS
          str =
              glade_utils_enum_string_from_value_displayable
              (GTK_TYPE_STATE_TYPE, state);
          gtk_tree_store_set (eprop_sources->store, &iter, COLUMN_STATE_ACTIVE,
                              TRUE, COLUMN_STATE, str, -1);
          g_free (str);
        }

      if (!l->next)
        {
          GtkTreePath *path =
              gtk_tree_model_get_path (GTK_TREE_MODEL (eprop_sources->store),
                                       &iter);
          gtk_tree_view_expand_to_path (GTK_TREE_VIEW (eprop_sources->view),
                                        path);
          gtk_tree_path_free (path);
        }
    }
}

static void
icon_sources_populate_store (GladeEPropIconSources *eprop_sources)
{
  GladeIconSources *sources = NULL;
  GladeProperty *property;

  gtk_tree_store_clear (eprop_sources->store);
  gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (eprop_sources->combo));

  property = glade_editor_property_get_property (GLADE_EDITOR_PROPERTY (eprop_sources));
  if (!property)
    return;

  glade_property_get (property, &sources);

  if (sources)
    g_hash_table_foreach (sources->sources, (GHFunc) populate_store_foreach,
                          eprop_sources);

}

static void
glade_eprop_icon_sources_load (GladeEditorProperty *eprop,
                               GladeProperty       *property)
{
  GladeEditorPropertyClass *parent_class =
      g_type_class_peek_parent (GLADE_EDITOR_PROPERTY_GET_CLASS (eprop));
  GladeEPropIconSources *eprop_sources = GLADE_EPROP_ICON_SOURCES (eprop);

  /* Chain up in a clean state... */
  parent_class->load (eprop, property);

  icon_sources_populate_store (eprop_sources);

  gtk_widget_queue_draw (GTK_WIDGET (eprop_sources->view));
}

static gboolean
reload_icon_sources_idle (GladeEditorProperty *eprop)
{
  GladeProperty *property = glade_editor_property_get_property (eprop);

  glade_editor_property_load (eprop, property);
  return FALSE;
}


typedef struct {
  GladeEPropIconSources *eprop;
  GtkTreeRowReference   *row_ref;
} RowEditData;

static gboolean
edit_row_idle (RowEditData *data)
{
  GtkTreePath *new_item_path;

  new_item_path = gtk_tree_row_reference_get_path (data->row_ref);

  gtk_widget_grab_focus (GTK_WIDGET (data->eprop->view));
  gtk_tree_view_expand_to_path (data->eprop->view, new_item_path);
  gtk_tree_view_set_cursor (data->eprop->view, new_item_path,
                            data->eprop->filename_column, TRUE);

  gtk_tree_path_free (new_item_path);
  gtk_tree_row_reference_free (data->row_ref);
  g_slice_free (RowEditData, data);
  return FALSE;
}

static void
add_clicked (GtkWidget *button, GladeEPropIconSources *eprop_sources)
{
  /* Remember to set focus on the cell and activate it ! */
  GtkTreeIter *parent_iter = NULL, iter, new_parent_iter;
  GtkTreePath *new_item_path;
  gchar *icon_name;
  gchar *selected_icon_name = NULL;
  gint index;
  RowEditData *edit_data;

  selected_icon_name = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (eprop_sources->combo));

  if (!selected_icon_name)
    return;

  /* Find the right parent iter to add a child to... */
  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (eprop_sources->store), &iter))
    {
      do
        {
          gtk_tree_model_get (GTK_TREE_MODEL (eprop_sources->store), &iter,
                              COLUMN_TEXT, &icon_name, -1);

          if (icon_name && strcmp (icon_name, selected_icon_name) == 0)
            parent_iter = gtk_tree_iter_copy (&iter);

          g_free (icon_name);
        }
      while (parent_iter == NULL &&
             gtk_tree_model_iter_next (GTK_TREE_MODEL (eprop_sources->store), &iter));
    }

  /* check if we're already adding one here... */
  if (parent_iter &&
      gtk_tree_model_iter_children (GTK_TREE_MODEL (eprop_sources->store),
                                    &iter, parent_iter))
    {
      do
        {
          gtk_tree_model_get (GTK_TREE_MODEL (eprop_sources->store), &iter,
                              COLUMN_LIST_INDEX, &index, -1);

          /* Iter is set, expand and return. */
          if (index < 0)
            goto expand_to_path_and_focus;

        }
      while (gtk_tree_model_iter_next (GTK_TREE_MODEL (eprop_sources->store), &iter));
    }

  if (!parent_iter)
    {
      /* Dont set COLUMN_ICON_NAME here */
      gtk_tree_store_append (eprop_sources->store, &new_parent_iter, NULL);
      gtk_tree_store_set (eprop_sources->store, &new_parent_iter,
                          COLUMN_TEXT, selected_icon_name,
                          COLUMN_TEXT_EDITABLE, FALSE,
                          COLUMN_TEXT_WEIGHT, PANGO_WEIGHT_BOLD, -1);
      parent_iter = gtk_tree_iter_copy (&new_parent_iter);
    }

  gtk_tree_store_append (eprop_sources->store, &iter, parent_iter);
  gtk_tree_store_set (eprop_sources->store, &iter,
                      COLUMN_ICON_NAME, selected_icon_name,
                      COLUMN_TEXT_EDITABLE, TRUE,
                      COLUMN_TEXT_WEIGHT, PANGO_WEIGHT_NORMAL,
                      COLUMN_LIST_INDEX, -1, -1);

  /* By now iter is valid. */
expand_to_path_and_focus:
  new_item_path = gtk_tree_model_get_path (GTK_TREE_MODEL (eprop_sources->store), &iter);

  /* Queue an idle to expand to row, GtkTreeView seems to be broken now
   * and will not expand to the second row we just added (instead the parent row get's focus),
   * deferring this to an idle handler fixes the problem.
   */
  edit_data = g_slice_new (RowEditData);
  edit_data->eprop = eprop_sources;
  edit_data->row_ref = gtk_tree_row_reference_new (GTK_TREE_MODEL (eprop_sources->store), new_item_path);
  g_idle_add ((GSourceFunc)edit_row_idle, edit_data);

  g_free (selected_icon_name);
  gtk_tree_path_free (new_item_path);
  gtk_tree_iter_free (parent_iter);
}

static GtkIconSource *
get_icon_source (GladeIconSources *sources,
                 const gchar      *icon_name,
                 gint              index)
{
  GList *source_list = g_hash_table_lookup (sources->sources, icon_name);

  if (source_list)
    {
      if (index < 0)
        return NULL;
      else
        return (GtkIconSource *) g_list_nth_data (source_list, index);
    }
  return NULL;
}

static void
update_icon_sources (GladeEditorProperty *eprop,
                     GladeIconSources    *icon_sources)
{
  GValue value = { 0, };

  g_value_init (&value, GLADE_TYPE_ICON_SOURCES);
  g_value_take_boxed (&value, icon_sources);
  glade_editor_property_commit (eprop, &value);
  g_value_unset (&value);
}

static void
delete_clicked (GtkWidget *button, GladeEditorProperty *eprop)
{
  GladeEPropIconSources *eprop_sources = GLADE_EPROP_ICON_SOURCES (eprop);
  GladeProperty *property = glade_editor_property_get_property (eprop);
  GtkTreeIter iter;
  GladeIconSources *icon_sources = NULL;
  GList *list, *sources, *new_list_head;
  gchar *icon_name;
  gint index = 0;

  /* NOTE: This will trigger row-deleted below... */
  if (!gtk_tree_selection_get_selected
      (gtk_tree_view_get_selection (eprop_sources->view), NULL, &iter))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (eprop_sources->store), &iter,
                      COLUMN_ICON_NAME, &icon_name,
                      COLUMN_LIST_INDEX, &index, -1);

  /* Could be the user pressed add and then delete without touching the
   * new item.
   */
  if (index < 0)
    {
      g_idle_add ((GSourceFunc) reload_icon_sources_idle, eprop);
      return;
    }

  glade_property_get (property, &icon_sources);
  if (icon_sources)
    {
      icon_sources = glade_icon_sources_copy (icon_sources);

      if ((sources =
           g_hash_table_lookup (icon_sources->sources, icon_name)) != NULL)
        {
          new_list_head = icon_set_copy (sources);

          list = g_list_nth (new_list_head, index);
          new_list_head = g_list_remove_link (new_list_head, list);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
          gtk_icon_source_free ((GtkIconSource *) list->data);
G_GNUC_END_IGNORE_DEPRECATIONS
          g_list_free (list);

          /* We copied all that above cause this will free the old list */
          g_hash_table_insert (icon_sources->sources, g_strdup (icon_name),
                               new_list_head);

        }
      update_icon_sources (eprop, icon_sources);
    }
  g_free (icon_name);
}

static void
value_filename_edited (GtkCellRendererText *cell,
                       const gchar         *path,
                       const gchar         *new_text,
                       GladeEditorProperty *eprop)
{
  GladeEPropIconSources *eprop_sources = GLADE_EPROP_ICON_SOURCES (eprop);
  GladeProperty *property = glade_editor_property_get_property (eprop);
  GtkTreeIter iter;
  GladeIconSources *icon_sources = NULL;
  GtkIconSource *source;
  gchar *icon_name;
  gint index = -1;
  GValue *value;
  GdkPixbuf *pixbuf;
  GList *source_list;

  if (!new_text || !new_text[0])
    {
      g_idle_add ((GSourceFunc) reload_icon_sources_idle, eprop);
      return;
    }

  if (!gtk_tree_model_get_iter_from_string
      (GTK_TREE_MODEL (eprop_sources->store), &iter, path))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (eprop_sources->store), &iter,
                      COLUMN_ICON_NAME, &icon_name,
                      COLUMN_LIST_INDEX, &index, -1);

  /* get new pixbuf value... */
  value = glade_utils_value_from_string (GDK_TYPE_PIXBUF, new_text,
                                         glade_widget_get_project (glade_property_get_widget (property)));
  pixbuf = g_value_get_object (value);


  glade_property_get (property, &icon_sources);
  if (icon_sources)
    {
      icon_sources = glade_icon_sources_copy (icon_sources);

      if (index >= 0 &&
          (source = get_icon_source (icon_sources, icon_name, index)) != NULL)
        {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
          gtk_icon_source_set_pixbuf (source, pixbuf);
G_GNUC_END_IGNORE_DEPRECATIONS
        }
      else
        {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
          source = gtk_icon_source_new ();
          gtk_icon_source_set_pixbuf (source, pixbuf);
G_GNUC_END_IGNORE_DEPRECATIONS

          if ((source_list = g_hash_table_lookup (icon_sources->sources,
                                                  icon_name)) != NULL)
            {
              source_list = g_list_append (source_list, source);
            }
          else
            {
              source_list = g_list_prepend (NULL, source);
              g_hash_table_insert (icon_sources->sources, g_strdup (icon_name),
                                   source_list);
            }
        }
    }
  else
    {
      icon_sources = glade_icon_sources_new ();
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      source = gtk_icon_source_new ();
      gtk_icon_source_set_pixbuf (source, pixbuf);
G_GNUC_END_IGNORE_DEPRECATIONS

      source_list = g_list_prepend (NULL, source);
      g_hash_table_insert (icon_sources->sources, g_strdup (icon_name),
                           source_list);
    }

  g_value_unset (value);
  g_free (value);

  update_icon_sources (eprop, icon_sources);
}

static void
value_attribute_toggled (GtkCellRendererToggle *cell_renderer,
                         gchar                 *path,
                         GladeEditorProperty   *eprop)
{
  GladeEPropIconSources *eprop_sources = GLADE_EPROP_ICON_SOURCES (eprop);
  GladeProperty *property = glade_editor_property_get_property (eprop);
  GtkTreeIter iter;
  GladeIconSources *icon_sources = NULL;
  GtkIconSource *source;
  gchar *icon_name;
  gint index, edit_column;
  gboolean edit_column_active = FALSE;

  if (!gtk_tree_model_get_iter_from_string
      (GTK_TREE_MODEL (eprop_sources->store), &iter, path))
    return;

  edit_column =
      GPOINTER_TO_INT (g_object_get_data
                       (G_OBJECT (cell_renderer), "attribute-column"));
  gtk_tree_model_get (GTK_TREE_MODEL (eprop_sources->store), &iter,
                      COLUMN_ICON_NAME, &icon_name, COLUMN_LIST_INDEX, &index,
                      edit_column, &edit_column_active, -1);

  glade_property_get (property, &icon_sources);

  if (icon_sources)
    icon_sources = glade_icon_sources_copy (icon_sources);

  if (icon_sources &&
      (source = get_icon_source (icon_sources, icon_name, index)) != NULL)
    {
      /* Note the reverse meaning of active toggles vs. wildcarded sources... */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      switch (edit_column)
        {
          case COLUMN_DIRECTION_ACTIVE:
            gtk_icon_source_set_direction_wildcarded (source,
                                                      edit_column_active);
            break;
          case COLUMN_SIZE_ACTIVE:
            gtk_icon_source_set_size_wildcarded (source, edit_column_active);
            break;
          case COLUMN_STATE_ACTIVE:
            gtk_icon_source_set_state_wildcarded (source, edit_column_active);
            break;
          default:
            break;
        }
G_GNUC_END_IGNORE_DEPRECATIONS

      update_icon_sources (eprop, icon_sources);
      g_free (icon_name);
      return;
    }

  if (icon_sources)
    glade_icon_sources_free (icon_sources);
  g_free (icon_name);
  return;
}

static void
value_attribute_edited (GtkCellRendererText *cell,
                        const gchar         *path,
                        const gchar         *new_text,
                        GladeEditorProperty *eprop)
{
  GladeEPropIconSources *eprop_sources = GLADE_EPROP_ICON_SOURCES (eprop);
  GladeProperty *property = glade_editor_property_get_property (eprop);
  GtkTreeIter iter;
  GladeIconSources *icon_sources = NULL;
  GtkIconSource *source;
  gchar *icon_name;
  gint index, edit_column;

  if (!new_text || !new_text[0])
    return;

  if (!gtk_tree_model_get_iter_from_string
      (GTK_TREE_MODEL (eprop_sources->store), &iter, path))
    return;

  edit_column =
      GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "attribute-column"));
  gtk_tree_model_get (GTK_TREE_MODEL (eprop_sources->store), &iter,
                      COLUMN_ICON_NAME, &icon_name, COLUMN_LIST_INDEX, &index,
                      -1);

  glade_property_get (property, &icon_sources);

  if (icon_sources)
    icon_sources = glade_icon_sources_copy (icon_sources);

  if (icon_sources &&
      (source = get_icon_source (icon_sources, icon_name, index)) != NULL)
    {
      GtkTextDirection direction;
      GtkIconSize size;
      GtkStateType state;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      switch (edit_column)
        {
          case COLUMN_DIRECTION:
            direction =
                glade_utils_enum_value_from_string (GTK_TYPE_TEXT_DIRECTION,
                                                    new_text);
            gtk_icon_source_set_direction (source, direction);
            break;
          case COLUMN_SIZE:
            size =
                glade_utils_enum_value_from_string (GTK_TYPE_ICON_SIZE,
                                                    new_text);
            gtk_icon_source_set_size (source, size);
            break;
          case COLUMN_STATE:
            state =
                glade_utils_enum_value_from_string (GTK_TYPE_STATE_TYPE,
                                                    new_text);
            gtk_icon_source_set_state (source, state);
            break;
          default:
            break;
        }
G_GNUC_END_IGNORE_DEPRECATIONS

      update_icon_sources (eprop, icon_sources);
      g_free (icon_name);
      return;
    }

  if (icon_sources)
    glade_icon_sources_free (icon_sources);
  g_free (icon_name);
  return;
}

static gboolean
icon_sources_query_tooltip (GtkWidget             *widget,
                            gint                   x,
                            gint                   y,
                            gboolean               keyboard_mode,
                            GtkTooltip            *tooltip,
                            GladeEPropIconSources *eprop_sources)
{
  GtkTreePath *path = NULL;
  GtkTreeIter iter;
  GtkTreeViewColumn *column = NULL;
  gint bin_x = x, bin_y = y, col;
  gchar *icon_name = NULL;
  gboolean show_now = FALSE;

  if (keyboard_mode)
    return FALSE;

  gtk_tree_view_convert_widget_to_bin_window_coords (eprop_sources->view,
                                                     x, y, &bin_x, &bin_y);

  if (gtk_tree_view_get_path_at_pos (eprop_sources->view,
                                     bin_x, bin_y, &path, &column, NULL, NULL))
    {
      if (gtk_tree_model_get_iter
          (GTK_TREE_MODEL (eprop_sources->store), &iter, path))
        {
          col =
              GPOINTER_TO_INT (g_object_get_data
                               (G_OBJECT (column), "column-id"));

          gtk_tree_model_get (GTK_TREE_MODEL (eprop_sources->store), &iter,
                              COLUMN_ICON_NAME, &icon_name, -1);

          /* no tooltips on the parent rows */
          if (icon_name)
            {
              gchar *tooltip_text = NULL;
              show_now = TRUE;

              switch (col)
                {
                  case COLUMN_TEXT:
                    tooltip_text =
                        g_strdup_printf (_
                                         ("Enter a filename or a relative or full path for this "
                                          "source of '%s' (Glade will only ever load them in "
                                          "the runtime from your project directory)."),
                                         icon_name);
                    break;
                  case COLUMN_DIRECTION_ACTIVE:
                    tooltip_text =
                        g_strdup_printf (_
                                         ("Set whether you want to specify a text direction "
                                          "for this source of '%s'"),
                                         icon_name);
                    break;
                  case COLUMN_DIRECTION:
                    tooltip_text =
                        g_strdup_printf (_
                                         ("Set the text direction for this source of '%s'"),
                                         icon_name);
                    break;
                  case COLUMN_SIZE_ACTIVE:
                    tooltip_text =
                        g_strdup_printf (_
                                         ("Set whether you want to specify an icon size "
                                          "for this source of '%s'"),
                                         icon_name);
                    break;
                  case COLUMN_SIZE:
                    tooltip_text =
                        g_strdup_printf (_
                                         ("Set the icon size for this source of '%s'"),
                                         icon_name);
                    break;
                  case COLUMN_STATE_ACTIVE:
                    tooltip_text =
                        g_strdup_printf (_
                                         ("Set whether you want to specify a state "
                                          "for this source of '%s'"),
                                         icon_name);
                    break;
                  case COLUMN_STATE:
                    tooltip_text =
                        g_strdup_printf (_
                                         ("Set the state for this source of '%s'"),
                                         icon_name);
                  default:
                    break;

                }

              gtk_tooltip_set_text (tooltip, tooltip_text);
              g_free (tooltip_text);
              g_free (icon_name);


              gtk_tree_view_set_tooltip_cell (eprop_sources->view,
                                              tooltip, path, column, NULL);

            }
        }
      gtk_tree_path_free (path);
    }
  return show_now;
}


static GtkTreeView *
build_view (GladeEditorProperty *eprop)
{
  GladeEPropIconSources *eprop_sources = GLADE_EPROP_ICON_SOURCES (eprop);
  static GtkListStore *direction_store = NULL, *size_store =
      NULL, *state_store = NULL;
  GtkTreeView *view = (GtkTreeView *) gtk_tree_view_new ();
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  if (!direction_store)
    {
      direction_store =
          glade_utils_liststore_from_enum_type (GTK_TYPE_TEXT_DIRECTION, FALSE);
      size_store =
          glade_utils_liststore_from_enum_type (GTK_TYPE_ICON_SIZE, FALSE);
      state_store =
          glade_utils_liststore_from_enum_type (GTK_TYPE_STATE_TYPE, FALSE);
    }

  /* Filename / icon name column/renderer */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);
  g_signal_connect (G_OBJECT (renderer), "edited",
                    G_CALLBACK (value_filename_edited), eprop);

  eprop_sources->filename_column =
      gtk_tree_view_column_new_with_attributes (_("File Name"), renderer,
                                                "text", COLUMN_TEXT,
                                                "weight", COLUMN_TEXT_WEIGHT,
                                                "editable",
                                                COLUMN_TEXT_EDITABLE, NULL);
  gtk_tree_view_column_set_expand (eprop_sources->filename_column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view),
                               eprop_sources->filename_column);

  g_object_set_data (G_OBJECT (eprop_sources->filename_column), "column-id",
                     GINT_TO_POINTER (COLUMN_TEXT));

        /********************* Size *********************/
  /* Attribute active portion */
  renderer = gtk_cell_renderer_toggle_new ();
  g_object_set (G_OBJECT (renderer), "activatable", TRUE, NULL);
  g_object_set_data (G_OBJECT (renderer), "attribute-column",
                     GINT_TO_POINTER (COLUMN_SIZE_ACTIVE));
  g_signal_connect (G_OBJECT (renderer), "toggled",
                    G_CALLBACK (value_attribute_toggled), eprop);

  column = gtk_tree_view_column_new_with_attributes
      ("dummy", renderer,
       "visible", COLUMN_TEXT_EDITABLE, "active", COLUMN_SIZE_ACTIVE, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);
  g_object_set_data (G_OBJECT (column), "column-id",
                     GINT_TO_POINTER (COLUMN_SIZE_ACTIVE));

  /* Attribute portion */
  renderer = gtk_cell_renderer_combo_new ();
  g_object_set (G_OBJECT (renderer), "editable", TRUE, "has-entry", FALSE,
                "text-column", 0, "model", size_store, NULL);
  g_object_set_data (G_OBJECT (renderer), "attribute-column",
                     GINT_TO_POINTER (COLUMN_SIZE));
  g_signal_connect (G_OBJECT (renderer), "edited",
                    G_CALLBACK (value_attribute_edited), eprop);

  column = gtk_tree_view_column_new_with_attributes
      ("dummy", renderer,
       "visible", COLUMN_TEXT_EDITABLE,
       "editable", COLUMN_SIZE_ACTIVE, "text", COLUMN_SIZE, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);
  g_object_set_data (G_OBJECT (column), "column-id",
                     GINT_TO_POINTER (COLUMN_SIZE));


        /********************* State *********************/
  /* Attribute active portion */
  renderer = gtk_cell_renderer_toggle_new ();
  g_object_set (G_OBJECT (renderer), "activatable", TRUE, NULL);
  g_object_set_data (G_OBJECT (renderer), "attribute-column",
                     GINT_TO_POINTER (COLUMN_STATE_ACTIVE));
  g_signal_connect (G_OBJECT (renderer), "toggled",
                    G_CALLBACK (value_attribute_toggled), eprop);

  column = gtk_tree_view_column_new_with_attributes
      ("dummy", renderer,
       "visible", COLUMN_TEXT_EDITABLE, "active", COLUMN_STATE_ACTIVE, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);
  g_object_set_data (G_OBJECT (column), "column-id",
                     GINT_TO_POINTER (COLUMN_STATE_ACTIVE));

  /* Attribute portion */
  renderer = gtk_cell_renderer_combo_new ();
  g_object_set (G_OBJECT (renderer), "editable", TRUE, "has-entry", FALSE,
                "text-column", 0, "model", state_store, NULL);
  g_object_set_data (G_OBJECT (renderer), "attribute-column",
                     GINT_TO_POINTER (COLUMN_STATE));
  g_signal_connect (G_OBJECT (renderer), "edited",
                    G_CALLBACK (value_attribute_edited), eprop);

  column = gtk_tree_view_column_new_with_attributes
      ("dummy", renderer,
       "visible", COLUMN_TEXT_EDITABLE,
       "editable", COLUMN_STATE_ACTIVE, "text", COLUMN_STATE, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);
  g_object_set_data (G_OBJECT (column), "column-id",
                     GINT_TO_POINTER (COLUMN_STATE));


        /********************* Direction *********************/
  /* Attribute active portion */
  renderer = gtk_cell_renderer_toggle_new ();
  g_object_set (G_OBJECT (renderer), "activatable", TRUE, NULL);
  g_object_set_data (G_OBJECT (renderer), "attribute-column",
                     GINT_TO_POINTER (COLUMN_DIRECTION_ACTIVE));
  g_signal_connect (G_OBJECT (renderer), "toggled",
                    G_CALLBACK (value_attribute_toggled), eprop);

  column = gtk_tree_view_column_new_with_attributes
      ("dummy", renderer,
       "visible", COLUMN_TEXT_EDITABLE,
       "active", COLUMN_DIRECTION_ACTIVE, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);
  g_object_set_data (G_OBJECT (column), "column-id",
                     GINT_TO_POINTER (COLUMN_DIRECTION_ACTIVE));

  /* Attribute portion */
  renderer = gtk_cell_renderer_combo_new ();
  g_object_set (G_OBJECT (renderer), "editable", TRUE, "has-entry", FALSE,
                "text-column", 0, "model", direction_store, NULL);
  g_object_set_data (G_OBJECT (renderer), "attribute-column",
                     GINT_TO_POINTER (COLUMN_DIRECTION));
  g_signal_connect (G_OBJECT (renderer), "edited",
                    G_CALLBACK (value_attribute_edited), eprop);

  column = gtk_tree_view_column_new_with_attributes
      ("dummy", renderer,
       "visible", COLUMN_TEXT_EDITABLE,
       "editable", COLUMN_DIRECTION_ACTIVE, "text", COLUMN_DIRECTION, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);
  g_object_set_data (G_OBJECT (column), "column-id",
                     GINT_TO_POINTER (COLUMN_DIRECTION));


  /* Connect ::query-tooltip here for fancy tooltips... */
  g_object_set (G_OBJECT (view), "has-tooltip", TRUE, NULL);
  g_signal_connect (G_OBJECT (view), "query-tooltip",
                    G_CALLBACK (icon_sources_query_tooltip), eprop);

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);
  gtk_tree_view_set_show_expanders (GTK_TREE_VIEW (view), FALSE);

  return view;
}

static void
icon_name_entry_activated (GtkEntry *entry, GladeEPropIconSources *eprop_sources)
{
  const gchar *text = gtk_entry_get_text (entry);
  GladeProperty *property;
  GladeIconSources *sources = NULL;

  if (!text || !text[0])
    return;

  property = glade_editor_property_get_property (GLADE_EDITOR_PROPERTY (eprop_sources));
  if (!property)
    return;

  glade_property_get (property, &sources);
  if (sources == NULL || g_hash_table_lookup (sources->sources, text) == NULL)
    {
      /* Add the new source if it doesnt already exist */
      gtk_combo_box_text_insert (GTK_COMBO_BOX_TEXT (eprop_sources->combo), -1, text, text);
    }

  /* Set the active id whether it existed or not */
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (eprop_sources->combo), text);
}

static GtkWidget *
glade_eprop_icon_sources_create_input (GladeEditorProperty * eprop)
{
  GladeEPropIconSources *eprop_sources = GLADE_EPROP_ICON_SOURCES (eprop);
  GtkWidget *vbox, *hbox, *button, *swin;

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

  /* hbox with comboboxentry add/remove source buttons on the right... */
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  eprop_sources->combo = gtk_combo_box_text_new_with_entry ();
  g_signal_connect (G_OBJECT
                    (gtk_bin_get_child (GTK_BIN (eprop_sources->combo))),
                    "activate", G_CALLBACK (icon_name_entry_activated), eprop);

  gtk_box_pack_start (GTK_BOX (hbox), eprop_sources->combo, TRUE, TRUE, 0);
  button = gtk_button_new ();
  gtk_container_set_border_width (GTK_CONTAINER (button), 2);
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("list-add-symbolic",
                                                      GTK_ICON_SIZE_BUTTON));
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (add_clicked), eprop_sources);

  button = gtk_button_new ();
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("list-remove-symbolic",
                                                      GTK_ICON_SIZE_BUTTON));
  gtk_container_set_border_width (GTK_CONTAINER (button), 2);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (delete_clicked), eprop_sources);

  /* Pack treeview/swindow on the left... */
  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin),
                                       GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_box_pack_start (GTK_BOX (vbox), swin, TRUE, TRUE, 0);

  eprop_sources->view = build_view (eprop);
  gtk_container_add (GTK_CONTAINER (swin), GTK_WIDGET (eprop_sources->view));

  g_object_set (G_OBJECT (vbox), "height-request", 350, NULL);

  eprop_sources->store = gtk_tree_store_new (NUM_COLUMNS, G_TYPE_STRING,        // COLUMN_TEXT
                                             G_TYPE_INT,        // COLUMN_TEXT_WEIGHT
                                             G_TYPE_BOOLEAN,    // COLUMN_TEXT_EDITABLE
                                             G_TYPE_STRING,     // COLUMN_ICON_NAME
                                             G_TYPE_INT,        // COLUMN_LIST_INDEX
                                             G_TYPE_BOOLEAN,    // COLUMN_DIRECTION_ACTIVE
                                             G_TYPE_STRING,     // COLUMN_DIRECTION
                                             G_TYPE_BOOLEAN,    // COLUMN_SIZE_ACTIVE
                                             G_TYPE_STRING,     // COLUMN_SIZE
                                             G_TYPE_BOOLEAN,    // COLUMN_STATE_ACTIVE,
                                             G_TYPE_STRING);    // COLUMN_STATE

  gtk_tree_view_set_model (eprop_sources->view,
                           GTK_TREE_MODEL (eprop_sources->store));
  g_object_unref (G_OBJECT (eprop_sources->store));     // <-- pass ownership here

  gtk_widget_show_all (vbox);
  return vbox;
}
