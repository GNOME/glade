/*
 * glade-named-icon-chooser-widget.c - Named icon chooser widget
 *
 * Copyright (C) 2007 Vincent Geddes
 *
 * Author:  Vincent Geddes <vgeddes@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANNAMED_ICON_CHOOSERILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include "glade-private.h"
#include "glade-named-icon-chooser-dialog.h"
#include "icon-naming-spec.c"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include <errno.h>


#define DEFAULT_SETTING_LIST_STANDARD_ONLY   TRUE

enum
{
  CONTEXTS_ID_COLUMN,
  CONTEXTS_NAME_COLUMN,
  CONTEXTS_TITLE_COLUMN,

  CONTEXTS_N_COLUMS
};

enum
{
  ICONS_CONTEXT_COLUMN,
  ICONS_STANDARD_COLUMN,
  ICONS_NAME_COLUMN,

  ICONS_N_COLUMNS
};

enum
{
  GLADE_NAMED_ICON
};

enum
{
  ICON_ACTIVATED,
  SELECTION_CHANGED,

  LAST_SIGNAL
};

typedef struct _GladeNamedIconChooserDialogPrivate
{
  GtkWidget *icons_view;
  GtkTreeModel *filter_model;   /* filtering model  */
  GtkListStore *icons_store;    /* data store       */
  GtkTreeSelection *selection;

  GtkWidget *contexts_view;
  GtkListStore *contexts_store;

  GtkWidget *entry;
  GtkEntryCompletion *entry_completion;

  GtkWidget *button;            /* list-standard-only checkbutton */

  gint context_id;              /* current icon name context for icon filtering */

  gchar *pending_select_name;   /* an icon name for a pending treeview selection.
                                 * can only select name after model is loaded
                                 * and the widget is mapped */

  GtkIconTheme *icon_theme;     /* the current icon theme */
  guint load_id;                /* id of the idle function for loading data into model */

  gboolean settings_list_standard;      /* whether to list standard icon names only */

  GtkWidget *last_focus_widget;

  gboolean icons_loaded;        /* whether the icons have been loaded into the model */
} GladeNamedIconChooserDialogPrivate;

static GHashTable *standard_icon_quarks = NULL;

static guint dialog_signals[LAST_SIGNAL] = { 0, };

gchar *
glade_named_icon_chooser_dialog_get_icon_name (GladeNamedIconChooserDialog *dialog);

void
glade_named_icon_chooser_dialog_set_icon_name (GladeNamedIconChooserDialog *dialog,
                                               const gchar                 *icon_name);

gboolean
glade_named_icon_chooser_dialog_set_context (GladeNamedIconChooserDialog *dialog,
                                             const gchar                 *context);

gchar *
glade_named_icon_chooser_dialog_get_context (GladeNamedIconChooserDialog *dialog);

static gboolean should_respond (GladeNamedIconChooserDialog *dialog);

static void filter_icons_model (GladeNamedIconChooserDialog *dialog);

static gboolean scan_for_name_func (GtkTreeModel *model,
                                    GtkTreePath  *path,
                                    GtkTreeIter  *iter,
                                    gpointer      data);

static gboolean scan_for_context_func (GtkTreeModel *model,
                                       GtkTreePath  *path,
                                       GtkTreeIter  *iter,
                                       gpointer      data);

static void settings_load (GladeNamedIconChooserDialog *dialog);

static void settings_save (GladeNamedIconChooserDialog *dialog);


G_DEFINE_TYPE_WITH_PRIVATE (GladeNamedIconChooserDialog,
                            glade_named_icon_chooser_dialog,
                            GTK_TYPE_DIALOG);


static void
entry_set_name (GladeNamedIconChooserDialog *dialog, const gchar *name)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  /* Must disable completion before setting text, in order to avoid
   * spurious warnings (possible GTK+ bug).
   */
  gtk_entry_set_completion (GTK_ENTRY (priv->entry), NULL);

  gtk_entry_set_text (GTK_ENTRY (priv->entry), name);

  gtk_entry_set_completion (GTK_ENTRY (priv->entry),
                            priv->entry_completion);
}

static GtkIconTheme *
get_icon_theme_for_widget (GtkWidget *widget)
{
  if (gtk_widget_has_screen (widget))
    return gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));

  return gtk_icon_theme_get_default ();
}

/* validates name according to the icon naming spec (en_US.US_ASCII [a-z1-9_-.])  */
static gboolean
is_well_formed (const gchar * name)
{
  gchar *c = (gchar *) name;
  for (; *c; c++)
    {
      if (g_ascii_isalnum (*c))
        {
          if (g_ascii_isalpha (*c) && !g_ascii_islower (*c))
            return FALSE;
        }
      else if (*c != '_' && *c != '-' && *c != '.')
        {
          return FALSE;
        }
    }
  return TRUE;
}

static void
check_entry_text (GladeNamedIconChooserDialog *dialog,
                  gchar                      **name_ret,
                  gboolean                    *is_wellformed_ret,
                  gboolean                    *is_empty_ret)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  if (strlen (gtk_entry_get_text (GTK_ENTRY (priv->entry))) == 0)
    {
      *name_ret = NULL;
      *is_wellformed_ret = TRUE;
      *is_empty_ret = TRUE;

      return;
    }

  *is_empty_ret = FALSE;

  *is_wellformed_ret =
      is_well_formed (gtk_entry_get_text (GTK_ENTRY (priv->entry)));

  if (*is_wellformed_ret)
    *name_ret = g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->entry)));
  else
    *name_ret = NULL;
}

static void
changed_text_handler (GtkEditable                 *editable,
                      GladeNamedIconChooserDialog *dialog)
{
  g_signal_emit_by_name (dialog, "selection-changed", NULL);
}

/* ensure that only valid text can be inserted into entry */
static void
insert_text_handler (GtkEditable                 *editable,
                     const gchar                 *text,
                     gint                         length,
                     gint                        *position,
                     GladeNamedIconChooserDialog *dialog)
{
  if (is_well_formed (text))
    {

      g_signal_handlers_block_by_func (editable, (gpointer) insert_text_handler,
                                       dialog);

      gtk_editable_insert_text (editable, text, length, position);

      g_signal_handlers_unblock_by_func (editable,
                                         (gpointer) insert_text_handler,
                                         dialog);

    }
  else
    {
      gdk_display_beep (gtk_widget_get_display (GTK_WIDGET (dialog)));
    }

  g_signal_stop_emission_by_name (editable, "insert-text");
}

typedef struct
{
  gchar *name;                  /* the name of the icon or context */

  guint found:1;                /* whether an item matching `name' was found */
  guint do_select:1;            /* select the matched row */
  guint do_cursor:1;            /* put cursor at the matched row */
  guint do_activate:1;          /* activate the matched row */

  GladeNamedIconChooserDialog *dialog;
} ForEachFuncData;

void
glade_named_icon_chooser_dialog_set_icon_name (GladeNamedIconChooserDialog *dialog, 
                                               const gchar                 *name)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  ForEachFuncData *data;
  gboolean located_in_theme;

  g_return_if_fail (GLADE_IS_NAMED_ICON_CHOOSER_DIALOG (dialog));
  g_return_if_fail (gtk_widget_has_screen (GTK_WIDGET (dialog)));

  if (name == NULL)
    {
      gtk_tree_selection_unselect_all (priv->selection);
      entry_set_name (dialog, "");
      return;
    }

  located_in_theme =
      gtk_icon_theme_has_icon (get_icon_theme_for_widget (GTK_WIDGET (dialog)),
                               name);

  if (located_in_theme)
    {

      if (priv->icons_loaded && priv->filter_model)
        {

          data = g_slice_new0 (ForEachFuncData);
          data->name = g_strdup (name);
          data->found = FALSE;
          data->do_activate = FALSE;
          data->do_select = TRUE;
          data->do_cursor = TRUE;
          data->dialog = dialog;

          gtk_tree_model_foreach (priv->filter_model,
                                  scan_for_name_func, data);

          g_free (data->name);
          g_slice_free (ForEachFuncData, data);

        }
      else
        {
          priv->pending_select_name = g_strdup (name);
        }

      /* selecting a treeview row will set the entry text,
       * but we must have this here in case the row has been filtered out
       */
      entry_set_name (dialog, name);

    }
  else if (is_well_formed (name))
    {

      gtk_tree_selection_unselect_all (priv->selection);

      entry_set_name (dialog, name);
    }
  else
    {
      g_warning ("invalid icon name: '%s' is not well formed", name);
    }
}

gboolean
glade_named_icon_chooser_dialog_set_context (GladeNamedIconChooserDialog *dialog,
                                             const gchar                 *name)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  ForEachFuncData *data;

  g_return_val_if_fail (GLADE_IS_NAMED_ICON_CHOOSER_DIALOG (dialog), FALSE);

  data = g_slice_new0 (ForEachFuncData);

  if (name)
    data->name = g_strdup (name);
  else
    data->name = g_strdup ("All Contexts");

  data->found = FALSE;
  data->do_select = TRUE;
  data->do_activate = FALSE;
  data->do_cursor = FALSE;
  data->dialog = dialog;

  gtk_tree_model_foreach (GTK_TREE_MODEL (priv->contexts_store),
                          (GtkTreeModelForeachFunc) scan_for_context_func,
                          data);

  g_free (data->name);
  g_slice_free (ForEachFuncData, data);

  return TRUE;
}

gchar *
glade_named_icon_chooser_dialog_get_context (GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  GtkTreeSelection *sel;
  GtkTreeIter iter;
  gchar *context_name;

  g_return_val_if_fail (GLADE_IS_NAMED_ICON_CHOOSER_DIALOG (dialog), NULL);

  sel =
      gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->contexts_view));

  if (gtk_tree_selection_get_selected (sel, NULL, &iter))
    {

      gtk_tree_model_get (GTK_TREE_MODEL (priv->contexts_store), &iter,
                          CONTEXTS_NAME_COLUMN, &context_name, -1);

      /* if context_name is NULL, then it is the 'all categories' special context */
      return context_name;

    }
  else
    {
      return NULL;
    }
}

static gchar *
get_icon_name_from_selection (GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  GtkTreeIter iter;
  GtkTreeModel *model;
  gchar *name;

  if (!gtk_tree_selection_get_selected (priv->selection, &model, &iter))
    return NULL;

  gtk_tree_model_get (model, &iter, ICONS_NAME_COLUMN, &name, -1);

  return name;
}

gchar *
glade_named_icon_chooser_dialog_get_icon_name (GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  GtkWidget *current_focus;
  gchar *name;

  g_return_val_if_fail (GLADE_IS_NAMED_ICON_CHOOSER_DIALOG (dialog), NULL);

  current_focus = gtk_window_get_focus (GTK_WINDOW (dialog));

  if (current_focus == priv->icons_view)
    {

    view:
      name = get_icon_name_from_selection (dialog);

      if (name == NULL)
        goto entry;

    }
  else if (current_focus == priv->entry)
    {
      gboolean is_wellformed, is_empty;
    entry:
      check_entry_text (dialog, &name, &is_wellformed, &is_empty);

      if (!is_wellformed || is_empty)
        return NULL;

    }
  else if (priv->last_focus_widget == priv->icons_view)
    {
      goto view;
    }
  else if (priv->last_focus_widget == priv->entry)
    {
      goto entry;
    }
  else
    {
      goto view;
    }

  return name;
}

static void
set_busy_cursor (GladeNamedIconChooserDialog *dialog, gboolean busy)
{
  GdkDisplay *display;
  GdkCursor *cursor;

  if (!gtk_widget_get_realized (GTK_WIDGET (dialog)))
    return;

  display = gtk_widget_get_display (GTK_WIDGET (dialog));

  if (busy)
    cursor = gdk_cursor_new_for_display (display, GDK_WATCH);
  else
    cursor = NULL;

  gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (dialog)), cursor);
  gdk_display_flush (display);

  if (cursor)
    g_object_unref (cursor);
}

static GtkListStore *
populate_icon_contexts_model (void)
{
  GtkListStore *store;
  GtkTreeIter iter;
  guint i;

  store = gtk_list_store_new (CONTEXTS_N_COLUMS,
                              G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING);


  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
                      CONTEXTS_ID_COLUMN, -1,
                      CONTEXTS_NAME_COLUMN, "All Contexts",
                      CONTEXTS_TITLE_COLUMN, _("All Contexts"), -1);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
                      CONTEXTS_ID_COLUMN, -1,
                      CONTEXTS_NAME_COLUMN, NULL,
                      CONTEXTS_TITLE_COLUMN, NULL, -1);

  for (i = 0; i < G_N_ELEMENTS (standard_contexts); i++)
    {

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          CONTEXTS_ID_COLUMN, i,
                          CONTEXTS_NAME_COLUMN, standard_contexts[i].name,
                          CONTEXTS_TITLE_COLUMN, _(standard_contexts[i].title),
                          -1);
    }

  return store;
}

static void
icons_row_activated_cb (GtkTreeView                 *view,
                        GtkTreePath                 *path,
                        GtkTreeViewColumn           *column,
                        GladeNamedIconChooserDialog *dialog)
{
  g_signal_emit_by_name (dialog, "icon-activated", NULL);
}

static void
icons_selection_changed_cb (GtkTreeSelection * selection,
                            GladeNamedIconChooserDialog * dialog)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *name;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, ICONS_NAME_COLUMN, &name, -1);
      if (name)
        entry_set_name (dialog, name);

      g_free (name);
    }
  else
    {
      /* entry_set_name (dialog, ""); */
    }

  /* we emit "selection-changed" for chooser in insert_text_handler()
   * to avoid emitting the signal twice */
}

static void
contexts_row_activated_cb (GtkTreeView                 *view,
                           GtkTreePath                 *cpath,
                           GtkTreeViewColumn           *column,
                           GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  GtkTreeIter iter;
  GtkTreePath *path;

  if (gtk_tree_model_get_iter_first (priv->filter_model, &iter))
    {

      gtk_tree_selection_select_iter (priv->selection, &iter);

      path = gtk_tree_model_get_path (priv->filter_model, &iter);

      gtk_tree_selection_select_path (priv->selection, path);

      gtk_tree_view_scroll_to_point (GTK_TREE_VIEW (priv->icons_view),
                                     -1, 0);

      gtk_tree_path_free (path);

    }
  gtk_widget_grab_focus (priv->icons_view);
}

static void
contexts_selection_changed_cb (GtkTreeSelection            *selection,
                               GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  GtkTreeIter iter;
  GtkTreeModel *model;
  gboolean retval;
  gint context_id;

  retval = gtk_tree_selection_get_selected (selection, &model, &iter);

  if (retval)
    {

      gtk_tree_model_get (model, &iter, CONTEXTS_ID_COLUMN, &context_id, -1);

      priv->context_id = context_id;

      if (!priv->filter_model)
        return;

      filter_icons_model (dialog);
    }

  entry_set_name (dialog, "");

}

static gboolean
row_separator_func (GtkTreeModel *model, GtkTreeIter *iter, gpointer unused)
{
  gboolean retval;
  gchar *name, *title;

  gtk_tree_model_get (model, iter,
                      CONTEXTS_NAME_COLUMN, &name,
                      CONTEXTS_TITLE_COLUMN, &title, -1);

  retval = !name && !title;

  g_free (name);
  g_free (title);

  return retval;
}

static GtkWidget *
create_contexts_view (GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  GtkTreeView *view;
  GtkTreeViewColumn *column;
  GtkTreePath *path;

  priv->contexts_store = populate_icon_contexts_model ();

  view =
      GTK_TREE_VIEW (gtk_tree_view_new_with_model
                     (GTK_TREE_MODEL (priv->contexts_store)));

  column = gtk_tree_view_column_new_with_attributes (NULL,
                                                     gtk_cell_renderer_text_new
                                                     (), "text",
                                                     CONTEXTS_TITLE_COLUMN,
                                                     NULL);

  gtk_tree_view_append_column (view, column);
  gtk_tree_view_set_headers_visible (view, FALSE);

  gtk_tree_view_set_row_separator_func (view,
                                        (GtkTreeViewRowSeparatorFunc)
                                        row_separator_func, NULL, NULL);

  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (view),
                               GTK_SELECTION_BROWSE);

  path = gtk_tree_path_new_from_indices (0, -1);
  gtk_tree_selection_select_path (gtk_tree_view_get_selection (view), path);
  gtk_tree_path_free (path);

  g_signal_connect (view, "row-activated",
                    G_CALLBACK (contexts_row_activated_cb), dialog);

  g_signal_connect (gtk_tree_view_get_selection (view), "changed",
                    G_CALLBACK (contexts_selection_changed_cb), dialog);

  gtk_widget_show (GTK_WIDGET (view));

  return GTK_WIDGET (view);
}

/* filters the icons model based on the current state */
static void
filter_icons_model (GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);

  set_busy_cursor (dialog, TRUE);

  g_object_ref (priv->filter_model);
  gtk_tree_view_set_model (GTK_TREE_VIEW (priv->icons_view), NULL);
  gtk_entry_completion_set_model (priv->entry_completion, NULL);

  gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER
                                  (priv->filter_model));

  gtk_tree_view_set_model (GTK_TREE_VIEW (priv->icons_view),
                           priv->filter_model);
  gtk_entry_completion_set_model (priv->entry_completion,
                                  GTK_TREE_MODEL (priv->icons_store));
  gtk_entry_completion_set_text_column (priv->entry_completion,
                                        ICONS_NAME_COLUMN);
  g_object_unref (priv->filter_model);

  set_busy_cursor (dialog, FALSE);
}

static gboolean
filter_visible_func (GtkTreeModel                *model,
                     GtkTreeIter                 *iter,
                     GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  gboolean standard;
  gint context_id;

  gtk_tree_model_get (model, iter,
                      ICONS_CONTEXT_COLUMN, &context_id,
                      ICONS_STANDARD_COLUMN, &standard, -1);

  if (priv->context_id == -1)
    return (priv->settings_list_standard) ? TRUE && standard : TRUE;

  if (context_id == priv->context_id)
    return (priv->settings_list_standard) ? TRUE && standard : TRUE;
  else
    return FALSE;
}


static gboolean
search_equal_func (GtkTreeModel                *model,
                   gint                         column,
                   const gchar                 *key,
                   GtkTreeIter                 *iter,
                   GladeNamedIconChooserDialog *dialog)
{
  gchar *name;
  gboolean retval;

  gtk_tree_model_get (model, iter, ICONS_NAME_COLUMN, &name, -1);

  retval = !g_str_has_prefix (name, key);

  g_free (name);

  return retval;

}

static gboolean
scan_for_context_func (GtkTreeModel *model,
                       GtkTreePath  *path,
                       GtkTreeIter  *iter,
                       gpointer      user_data)
{
  ForEachFuncData *data = (ForEachFuncData *) user_data;
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (data->dialog);
  GtkTreeSelection *selection =
      gtk_tree_view_get_selection (GTK_TREE_VIEW
                                   (priv->contexts_view));
  gchar *name = NULL;

  gtk_tree_model_get (model, iter, CONTEXTS_NAME_COLUMN, &name, -1);
  if (!name)
    return FALSE;

  if (strcmp (name, data->name) == 0)
    {

      data->found = TRUE;

      if (data->do_activate)
        gtk_tree_view_row_activated (GTK_TREE_VIEW
                                     (priv->contexts_view), path,
                                     gtk_tree_view_get_column (GTK_TREE_VIEW
                                                               (priv->
                                                                contexts_view),
                                                               0));

      if (data->do_select)
        gtk_tree_selection_select_path (selection, path);
      else
        gtk_tree_selection_unselect_path (selection, path);

      if (data->do_cursor)
        gtk_tree_view_set_cursor (GTK_TREE_VIEW
                                  (priv->contexts_view), path,
                                  NULL, FALSE);

      g_free (name);

      return TRUE;
    }

  g_free (name);

  return FALSE;
}

gboolean
scan_for_name_func (GtkTreeModel *model,
                    GtkTreePath  *path,
                    GtkTreeIter  *iter,
                    gpointer      user_data)
{
  ForEachFuncData *data = (ForEachFuncData *) user_data;
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (data->dialog);
  gchar *name = NULL;

  gtk_tree_model_get (model, iter, ICONS_NAME_COLUMN, &name, -1);
  if (!name)
    return FALSE;

  if (strcmp (name, data->name) == 0)
    {

      data->found = TRUE;

      if (data->do_activate)
        gtk_tree_view_row_activated (GTK_TREE_VIEW
                                     (priv->icons_view), path,
                                     gtk_tree_view_get_column (GTK_TREE_VIEW
                                                               (priv->
                                                                icons_view),
                                                               0));

      if (data->do_select)
        gtk_tree_selection_select_path (priv->selection, path);
      else
        gtk_tree_selection_unselect_path (priv->selection, path);

      if (data->do_cursor)
        gtk_tree_view_set_cursor (GTK_TREE_VIEW
                                  (priv->icons_view), path, NULL,
                                  FALSE);

      g_free (name);

      return TRUE;
    }

  g_free (name);

  return FALSE;
}

static void
centre_selected_row (GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  GList *l;

  g_assert (priv->icons_store != NULL);
  g_assert (priv->selection != NULL);

  l = gtk_tree_selection_get_selected_rows (priv->selection, NULL);

  if (l)
    {
      g_assert (gtk_widget_get_mapped (GTK_WIDGET (dialog)));
      g_assert (gtk_widget_get_visible (GTK_WIDGET (dialog)));

      gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (priv->icons_view),
                                    (GtkTreePath *) l->data,
                                    NULL, TRUE, 0.5, 0.0);

/*                gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->icons_view),
                                            (GtkTreePath *) l->data,
                                            0,
                                            FALSE);
                                         
                gtk_widget_grab_focus (priv->icons_view);
*/
      g_list_free_full (l, (GDestroyNotify) gtk_tree_path_free);
    }
}

static void
select_first_row (GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  GtkTreePath *path;

  if (!priv->filter_model)
    return;

  path = gtk_tree_path_new_from_indices (0, -1);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->icons_view), path,
                            NULL, FALSE);
  gtk_tree_path_free (path);
}

static void
pending_select_name_process (GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  ForEachFuncData *data;

  g_assert (priv->icons_store != NULL);
  g_assert (priv->selection != NULL);

  if (priv->pending_select_name)
    {

      data = g_slice_new0 (ForEachFuncData);

      data->name = priv->pending_select_name;
      data->do_select = TRUE;
      data->do_activate = FALSE;
      data->dialog = dialog;

      gtk_tree_model_foreach (priv->filter_model,
                              scan_for_name_func, data);

      g_free (priv->pending_select_name);
      priv->pending_select_name = NULL;

      g_slice_free (ForEachFuncData, data);

    }
  else
    {
      if (strlen (gtk_entry_get_text (GTK_ENTRY (priv->entry))) == 0)
        {
          select_first_row (dialog);
        }
    }

  centre_selected_row (dialog);
}

static gboolean
is_standard_icon_name (const gchar *icon_name)
{
  GQuark quark;

  quark = g_quark_try_string (icon_name);

  if (quark == 0)
    return FALSE;

  return (g_hash_table_lookup (standard_icon_quarks, GUINT_TO_POINTER (quark))
          != NULL);

}

static void
cleanup_after_load (GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  priv->load_id = 0;

  pending_select_name_process (dialog);

  set_busy_cursor (dialog, FALSE);
}

static void
chooser_set_model (GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);

  /* filter model */
  priv->filter_model =
      gtk_tree_model_filter_new (GTK_TREE_MODEL (priv->icons_store),
                                 NULL);

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER
                                          (priv->filter_model),
                                          (GtkTreeModelFilterVisibleFunc)
                                          filter_visible_func, dialog, NULL);

  gtk_tree_view_set_model (GTK_TREE_VIEW (priv->icons_view),
                           priv->filter_model);
  g_object_unref (priv->filter_model);

  gtk_entry_completion_set_model (priv->entry_completion,
                                  GTK_TREE_MODEL (priv->icons_store));
  gtk_entry_completion_set_text_column (priv->entry_completion,
                                        ICONS_NAME_COLUMN);

  gtk_tree_view_set_search_column (GTK_TREE_VIEW (priv->icons_view),
                                   ICONS_NAME_COLUMN);

  priv->icons_loaded = TRUE;
}

typedef struct
{
  gchar *name;
  gint context;
} IconData;

static gint
icon_data_compare (IconData *a, IconData *b)
{
  return g_ascii_strcasecmp (a->name, b->name);
}

static gboolean
reload_icons (GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  GtkListStore *store = priv->icons_store;
  GtkTreeIter iter;
  guint i;
  GList *l, *icons = NULL;

  /* retrieve icon names from each context */
  for (i = 0; i < G_N_ELEMENTS (standard_contexts); i++)
    {

      GList *icons_in_context =
          gtk_icon_theme_list_icons (priv->icon_theme,
                                     standard_contexts[i].name);

      for (l = icons_in_context; l; l = l->next)
        {

          IconData *data = g_slice_new (IconData);

          data->name = (gchar *) l->data;
          data->context = i;

          icons = g_list_prepend (icons, data);
        }

      g_list_free (icons_in_context);
    }

  /* sort icon names */
  icons = g_list_sort (icons, (GCompareFunc) icon_data_compare);

  /* put into to model */
  for (l = icons; l; l = l->next)
    {

      IconData *data = (IconData *) l->data;

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          ICONS_CONTEXT_COLUMN, data->context,
                          ICONS_STANDARD_COLUMN,
                          is_standard_icon_name (data->name), ICONS_NAME_COLUMN,
                          data->name, -1);

      g_free (data->name);
      g_slice_free (IconData, data);
    }

  g_list_free (icons);

  chooser_set_model (dialog);

  return FALSE;
}

static void
change_icon_theme (GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  if (priv->icon_theme == NULL)
    priv->icon_theme = get_icon_theme_for_widget (GTK_WIDGET (dialog));

  gtk_tree_view_set_model (GTK_TREE_VIEW (priv->icons_view), NULL);
  gtk_list_store_clear (priv->icons_store);

  set_busy_cursor (dialog, TRUE);

  priv->load_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE + 300,
                                   (GSourceFunc) reload_icons,
                                   dialog,
                                   (GDestroyNotify) cleanup_after_load);

}

static void
glade_named_icon_chooser_dialog_screen_changed (GtkWidget *widget,
                                                GdkScreen *previous_screen)
{
  GladeNamedIconChooserDialog *dialog;

  dialog = GLADE_NAMED_ICON_CHOOSER_DIALOG (widget);

  if (GTK_WIDGET_CLASS (glade_named_icon_chooser_dialog_parent_class)->
      screen_changed)
    GTK_WIDGET_CLASS (glade_named_icon_chooser_dialog_parent_class)->
        screen_changed (widget, previous_screen);

  if (gtk_widget_get_mapped (widget))
    change_icon_theme (dialog);

}

static GtkWidget *
create_icons_view (GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  GtkTreeView *view;
  GtkTreeViewColumn *column;
  GtkCellRenderer *pixbuf_renderer, *text_renderer;

  view = GTK_TREE_VIEW (gtk_tree_view_new ());

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_min_width (column, 56);
  gtk_tree_view_column_set_title (column, NULL);
  pixbuf_renderer = gtk_cell_renderer_pixbuf_new ();

  gtk_tree_view_column_pack_start (column, pixbuf_renderer, TRUE);

  gtk_tree_view_column_set_attributes (column,
                                       pixbuf_renderer,
                                       "icon-name", ICONS_NAME_COLUMN, NULL);

  gtk_tree_view_append_column (view, column);
  g_object_set (pixbuf_renderer,
                "xpad", 2,
                "xalign", 1.0, "stock-size", GTK_ICON_SIZE_MENU, NULL);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, "Name");
  text_renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (text_renderer),
                "ellipsize", PANGO_ELLIPSIZE_END, "yalign", 0.0, NULL);

  gtk_tree_view_column_pack_start (column, text_renderer, TRUE);

  gtk_tree_view_column_set_attributes (column,
                                       text_renderer,
                                       "text", ICONS_NAME_COLUMN, NULL);


  gtk_tree_view_append_column (view, column);
  gtk_tree_view_column_set_expand (column, TRUE);
  gtk_tree_view_column_set_resizable (column, FALSE);

  gtk_tree_view_set_headers_visible (view, FALSE);

  gtk_tree_view_set_enable_search (view, TRUE);
  gtk_tree_view_set_search_equal_func (view,
                                       (GtkTreeViewSearchEqualFunc)
                                       search_equal_func, dialog, NULL);

  g_signal_connect (view, "row-activated",
                    G_CALLBACK (icons_row_activated_cb), dialog);

  g_signal_connect (gtk_tree_view_get_selection (view), "changed",
                    G_CALLBACK (icons_selection_changed_cb), dialog);

  gtk_tree_selection_set_mode (gtk_tree_view_get_selection (view),
                               GTK_SELECTION_BROWSE);

  priv->selection = gtk_tree_view_get_selection (view);

  gtk_tree_view_set_rules_hint (view, TRUE);

  gtk_widget_show (GTK_WIDGET (view));

  return GTK_WIDGET (view);
}

/* sets the 'list-standard' state and refilters the icons model */
static void
button_toggled (GtkToggleButton *button, GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  priv->settings_list_standard = gtk_toggle_button_get_active (button);

  if (priv->filter_model != NULL)
    filter_icons_model (dialog);
}

static GHashTable *
create_standard_icon_quarks (void)
{
  GHashTable *table;
  GQuark quark;
  guint i;

  table = g_hash_table_new (NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (standard_icon_names); i++)
    {

      quark = g_quark_from_static_string (standard_icon_names[i]);

      g_hash_table_insert (table,
                           GUINT_TO_POINTER (quark), GUINT_TO_POINTER (quark));
    }

  return table;
}

static void
glade_named_icon_chooser_dialog_style_set (GtkWidget *widget,
                                           GtkStyle  *previous_style)
{
  if (gtk_widget_has_screen (widget) && gtk_widget_get_mapped (widget))
    change_icon_theme (GLADE_NAMED_ICON_CHOOSER_DIALOG (widget));
}

/* override GtkWidget::show_all since we have internal widgets we wish to keep
 * hidden unless we decide otherwise, like the list-standard-icons-only checkbox.
 */
static void
glade_named_icon_chooser_dialog_show_all (GtkWidget *widget)
{
  gtk_widget_show (widget);
}

/* Handler for GtkWindow::set-focus; this is where we save the last-focused
 * widget on our toplevel.  See glade_named_icon_chooser_dialog_hierarchy_changed()
 */
static void
glade_named_icon_chooser_dialog_set_focus (GtkWindow *window, GtkWidget *focus)
{
  GladeNamedIconChooserDialog *dialog = GLADE_NAMED_ICON_CHOOSER_DIALOG (window);
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);

  GTK_WINDOW_CLASS (glade_named_icon_chooser_dialog_parent_class)->
      set_focus (window, focus);

  priv->last_focus_widget = gtk_window_get_focus (window);
}

static void
glade_named_icon_chooser_dialog_finalize (GObject *object)
{
  GladeNamedIconChooserDialog *dialog =
      GLADE_NAMED_ICON_CHOOSER_DIALOG (object);
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);

  g_clear_pointer (&priv->pending_select_name, g_free);

  G_OBJECT_CLASS (glade_named_icon_chooser_dialog_parent_class)->
    finalize (object);
}

static void
glade_named_icon_chooser_dialog_map (GtkWidget *widget)
{
  GladeNamedIconChooserDialog *dialog =
      GLADE_NAMED_ICON_CHOOSER_DIALOG (widget);
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);

  GTK_WIDGET_CLASS (glade_named_icon_chooser_dialog_parent_class)->map (widget);

  settings_load (dialog);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->button),
                                priv->settings_list_standard);

  gtk_widget_grab_focus (priv->icons_view);
}

static void
glade_named_icon_chooser_dialog_unmap (GtkWidget *widget)
{
  GladeNamedIconChooserDialog *dialog =
      GLADE_NAMED_ICON_CHOOSER_DIALOG (widget);

  settings_save (dialog);

  GTK_WIDGET_CLASS (glade_named_icon_chooser_dialog_parent_class)->
      unmap (widget);
}

/* we load the icons in expose() because we want the widget
 * to be fully painted before loading begins
 */
static gboolean
glade_named_icon_chooser_dialog_draw (GtkWidget *widget, cairo_t *cr)
{
  GladeNamedIconChooserDialog *dialog =
      GLADE_NAMED_ICON_CHOOSER_DIALOG (widget);
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  gboolean retval;

  retval =
      GTK_WIDGET_CLASS (glade_named_icon_chooser_dialog_parent_class)->
      draw (widget, cr);
  if (!priv->icons_loaded)
    {
      change_icon_theme (GLADE_NAMED_ICON_CHOOSER_DIALOG (widget));
      priv->icons_loaded = TRUE;
    }

  return retval;
}

static void
response_cb (GtkDialog *dialog, gint response_id)
{
  /* Act only on response IDs we recognize */
  if (!(response_id == GTK_RESPONSE_ACCEPT
        || response_id == GTK_RESPONSE_OK
        || response_id == GTK_RESPONSE_YES
        || response_id == GTK_RESPONSE_APPLY))
    return;

  if (!should_respond (GLADE_NAMED_ICON_CHOOSER_DIALOG (dialog)))
    {
      g_signal_stop_emission_by_name (dialog, "response");
    }
}

/* we intercept the GladeNamedIconChooser::icon-activated signal and try to
 * make the dialog emit a valid response signal
 */
static void
icon_activated_cb (GladeNamedIconChooserDialog *dialog)
{
  GList *children, *l;

  children =
      gtk_container_get_children (GTK_CONTAINER
                                  (gtk_dialog_get_action_area
                                   (GTK_DIALOG (dialog))));

  for (l = children; l; l = l->next)
    {
      GtkWidget *widget;
      gint response_id;

      widget = GTK_WIDGET (l->data);
      response_id =
          gtk_dialog_get_response_for_widget (GTK_DIALOG (dialog), widget);

      if (response_id == GTK_RESPONSE_ACCEPT ||
          response_id == GTK_RESPONSE_OK ||
          response_id == GTK_RESPONSE_YES || response_id == GTK_RESPONSE_APPLY)
        {
          g_list_free (children);

          gtk_dialog_response (GTK_DIALOG (dialog), response_id);

          return;
        }
    }
  g_list_free (children);
}

/* we intercept the GladeNamedIconChooser::selection-changed signal and try to
 * make the affirmative response button insensitive when the selection is empty 
 */
static void
selection_changed_cb (GladeNamedIconChooserDialog *dialog)
{
  GList *children, *l;
  gchar *icon_name;

  children =
      gtk_container_get_children (GTK_CONTAINER
                                  (gtk_dialog_get_action_area
                                   (GTK_DIALOG (dialog))));

  for (l = children; l; l = l->next)
    {
      GtkWidget *widget;
      gint response_id;

      widget = GTK_WIDGET (l->data);
      response_id =
          gtk_dialog_get_response_for_widget (GTK_DIALOG (dialog), widget);

      if (response_id == GTK_RESPONSE_ACCEPT ||
          response_id == GTK_RESPONSE_OK ||
          response_id == GTK_RESPONSE_YES || response_id == GTK_RESPONSE_APPLY)
        {
          icon_name = glade_named_icon_chooser_dialog_get_icon_name (dialog);

          gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
                                             response_id, icon_name != NULL);
          g_free (icon_name);
          g_list_free (children);
          return;
        }
    }
  g_list_free (children);
}

static void
glade_named_icon_chooser_dialog_init (GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  GtkWidget *contents;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *sw;
  GtkWidget *label;
  GtkWidget *hpaned;
  GtkWidget *content_area;
  GtkSizeGroup *group;

  priv->filter_model = NULL;
  priv->icons_store = NULL;
  priv->context_id = -1;
  priv->pending_select_name = NULL;
  priv->last_focus_widget = NULL;
  priv->icons_loaded = FALSE;


  gtk_window_set_title (GTK_WINDOW (dialog), _("Named Icon Chooser"));

  gtk_window_set_default_size (GTK_WINDOW (dialog), 610, 480);

  _glade_util_dialog_set_hig (GTK_DIALOG (dialog));
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  /* We do a signal connection here rather than overriding the method in
   * class_init because GtkDialog::response is a RUN_LAST signal.  We want *our*
   * handler to be run *first*, regardless of whether the user installs response
   * handlers of his own.
   */
  g_signal_connect (dialog, "response", G_CALLBACK (response_cb), NULL);

  g_signal_connect (dialog, "icon-activated",
                    G_CALLBACK (icon_activated_cb), NULL);

  g_signal_connect (dialog, "selection-changed",
                    G_CALLBACK (selection_changed_cb), NULL);


  if (standard_icon_quarks == NULL)
    standard_icon_quarks = create_standard_icon_quarks ();

  contents = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (contents), 5);
  gtk_widget_show (contents);

  label = gtk_label_new_with_mnemonic (_("Icon _Name:"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_show (label);

  priv->entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (priv->entry), TRUE);
  gtk_entry_set_width_chars (GTK_ENTRY (priv->entry), 40);
  g_object_set (G_OBJECT (priv->entry), "truncate-multiline", TRUE,
                NULL);
  g_signal_connect (G_OBJECT (priv->entry), "changed",
                    G_CALLBACK (changed_text_handler), dialog);
  g_signal_connect (G_OBJECT (priv->entry), "insert-text",
                    G_CALLBACK (insert_text_handler), dialog);
  gtk_widget_show (priv->entry);

  priv->entry_completion = gtk_entry_completion_new ();
  gtk_entry_set_completion (GTK_ENTRY (priv->entry),
                            priv->entry_completion);
  gtk_entry_completion_set_popup_completion (priv->entry_completion,
                                             FALSE);
  gtk_entry_completion_set_inline_completion (priv->entry_completion,
                                              TRUE);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label), priv->entry);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_show (hbox);

  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), priv->entry, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (contents), hbox, FALSE, FALSE, 6);

  hpaned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_paned_set_position (GTK_PANED (hpaned), 150);
  gtk_widget_show (hpaned);

  priv->contexts_view = create_contexts_view (dialog);
  priv->icons_view = create_icons_view (dialog);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_show (vbox);

  group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

  label = gtk_label_new_with_mnemonic (_("C_ontexts:"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),
                                 priv->contexts_view);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_size_group_add_widget (group, label);
  gtk_widget_show (label);

  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
  gtk_widget_show (sw);

  gtk_container_add (GTK_CONTAINER (sw), priv->contexts_view);
  gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
  gtk_paned_pack1 (GTK_PANED (hpaned), vbox, FALSE, FALSE);


  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_show (vbox);

  label = gtk_label_new_with_mnemonic (_("Icon Na_mes:"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), priv->icons_view);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_size_group_add_widget (group, label);
  gtk_widget_show (label);

  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER,
                                  GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
  gtk_widget_show (sw);

  gtk_container_add (GTK_CONTAINER (sw), priv->icons_view);
  gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
  gtk_paned_pack2 (GTK_PANED (hpaned), vbox, TRUE, FALSE);

  gtk_box_pack_start (GTK_BOX (contents), hpaned, TRUE, TRUE, 0);


  g_object_unref (G_OBJECT (group));

  priv->button =
      gtk_check_button_new_with_mnemonic (_("_List standard icons only"));
  gtk_widget_show (priv->button);

  g_signal_connect (priv->button, "toggled",
                    G_CALLBACK (button_toggled), dialog);

  gtk_box_pack_start (GTK_BOX (contents), priv->button, FALSE, FALSE,
                      0);
  gtk_box_pack_start (GTK_BOX (content_area), contents, TRUE, TRUE, 0);

  /* underlying model */
  priv->icons_store = gtk_list_store_new (ICONS_N_COLUMNS,
                                          G_TYPE_UINT,
                                          G_TYPE_BOOLEAN,
                                          G_TYPE_STRING);
}

static void
glade_named_icon_chooser_dialog_class_init (GladeNamedIconChooserDialogClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkWindowClass *window_class;

  object_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);
  window_class = GTK_WINDOW_CLASS (klass);

  object_class->finalize = glade_named_icon_chooser_dialog_finalize;

  widget_class->map = glade_named_icon_chooser_dialog_map;
  widget_class->unmap = glade_named_icon_chooser_dialog_unmap;
  widget_class->draw = glade_named_icon_chooser_dialog_draw;
  widget_class->show_all = glade_named_icon_chooser_dialog_show_all;
  widget_class->style_set = glade_named_icon_chooser_dialog_style_set;
  widget_class->screen_changed = glade_named_icon_chooser_dialog_screen_changed;

  window_class->set_focus = glade_named_icon_chooser_dialog_set_focus;

  /**
   * GladeNamedIconChooserDialog::icon-activated
   * @chooser: the object which received the signal
   *
   * This signal is emitted when the user "activates" an icon
   * in the named icon chooser.  This can happen by double-clicking on an item
   * in the recently used resources list, or by pressing
   * <keycap>Enter</keycap>.
   */
  dialog_signals[ICON_ACTIVATED] =
      g_signal_new ("icon-activated",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeNamedIconChooserDialogClass,
                                     icon_activated), NULL, NULL,
                    g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * GladeNamedIconChooserDialog::selection-changed
   * @chooser: the object which received the signal
   *
   * This signal is emitted when there is a change in the set of
   * selected icon names.  This can happen when a user
   * modifies the selection with the mouse or the keyboard, or when
   * explicitely calling functions to change the selection.
   */
  dialog_signals[SELECTION_CHANGED] =
      g_signal_new ("selection-changed",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeNamedIconChooserDialogClass,
                                     selection_changed), NULL, NULL,
                    g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static gboolean
should_respond (GladeNamedIconChooserDialog *dialog)
{
  gchar *icon_name;

  /* is there an icon selected? */
  icon_name = glade_named_icon_chooser_dialog_get_icon_name (dialog);
  if (!icon_name)
    return FALSE;

  g_free (icon_name);
  return TRUE;
}

/* get's the name of the configuration file */
static gchar *
get_config_filename (void)
{
  return g_build_filename (g_get_user_config_dir (), "gladeui", "config", NULL);
}

/* get's the name of the directory that contains the config file */
static char *
get_config_dirname (void)
{
  return g_build_filename (g_get_user_config_dir (), "gladeui", NULL);
}

/* loads the configuration settings */
static void
settings_load (GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  GKeyFile *keyfile;
  gboolean success, boolean_value;
  gchar *filename;
  GError *error = NULL;

  keyfile = g_key_file_new ();

  filename = get_config_filename ();
  success = g_key_file_load_from_file (keyfile,
                                       filename, G_KEY_FILE_NONE, &error);
  g_free (filename);

  if (!success)
    {

      priv->settings_list_standard = DEFAULT_SETTING_LIST_STANDARD_ONLY;

      g_clear_error (&error);
      g_key_file_free (keyfile);
      return;
    }


  boolean_value = g_key_file_get_boolean (keyfile,
                                          "Named Icon Chooser",
                                          "ListStandardOnly", &error);
  if (error)
    {
      priv->settings_list_standard = DEFAULT_SETTING_LIST_STANDARD_ONLY;
      g_clear_error (&error);
    }
  else
    {
      priv->settings_list_standard = boolean_value;
    }

  g_key_file_free (keyfile);
}

/* creates a GKeyFile based on the current settings */
static GKeyFile *
settings_to_keyfile (GladeNamedIconChooserDialog *dialog)
{
  GladeNamedIconChooserDialogPrivate *priv = glade_named_icon_chooser_dialog_get_instance_private (dialog);
  GKeyFile *keyfile;
  gchar *filename;

  keyfile = g_key_file_new ();

  filename = get_config_filename ();
  g_key_file_load_from_file (keyfile,
                             get_config_filename (),
                             G_KEY_FILE_NONE, NULL);
  g_free (filename);

  g_key_file_set_boolean (keyfile,
                          "Named Icon Chooser",
                          "ListStandardOnly",
                          priv->settings_list_standard);

  return keyfile;
}

/* serializes the the current configuration to the config file */
static void
settings_save (GladeNamedIconChooserDialog *dialog)
{
  GKeyFile *keyfile;
  gchar *contents;
  gsize contents_length;
  gchar *filename = NULL, *dirname = NULL;
  GError *error = NULL;

  keyfile = settings_to_keyfile (dialog);

  contents = g_key_file_to_data (keyfile, &contents_length, &error);

  if (error)
    goto out;

  filename = get_config_filename ();

  if (!g_file_set_contents (filename, contents, contents_length, NULL))
    {
      gchar *dirname;
      gint saved_errno;

      dirname = get_config_dirname ();
      if (g_mkdir_with_parents (dirname, 0700) != 0)    /* 0700 per the XDG basedir spec */
        {

          saved_errno = errno;
          g_set_error (&error,
                       G_FILE_ERROR,
                       g_file_error_from_errno (saved_errno),
                       _("Could not create directory: %s"), dirname);
          goto out;
        }

      if (!g_file_set_contents (filename, contents, contents_length, &error))
        {
          goto out;
        }
    }

out:

  g_free (contents);
  g_free (dirname);
  g_free (filename);
  g_clear_error (&error);
  g_key_file_free (keyfile);
}

static GtkWidget *
glade_named_icon_chooser_dialog_new_valist (const gchar *title,
                                            GtkWindow   *parent,
                                            const gchar *first_button_text,
                                            va_list      varargs)
{
  GtkWidget *result;
  const char *button_text = first_button_text;
  gint response_id;

  result = g_object_new (GLADE_TYPE_NAMED_ICON_CHOOSER_DIALOG,
                         "title", title, "transient-for", parent, NULL);

  while (button_text)
    {
      response_id = va_arg (varargs, gint);
      gtk_dialog_add_button (GTK_DIALOG (result), button_text, response_id);
      button_text = va_arg (varargs, const gchar *);
    }

  return result;
}

/**
 * glade_named_icon_chooser_dialog_new:
 * @title: (nullable): Title of the dialog, or %NULL
 * @parent: (nullable): Transient parent of the dialog, or %NULL,
 * @first_button_text: (nullable): stock ID or text to go in the first button, or %NULL
 * @...: response ID for the first button, then additional (button, id)
 *   pairs, ending with %NULL
 *
 * Creates a new #GladeNamedIconChooserDialog.  This function is analogous to
 * gtk_dialog_new_with_buttons().
 *
 * Return value: a new #GladeNamedIconChooserDialog
 */
GtkWidget *
glade_named_icon_chooser_dialog_new (const gchar *title,
                                     GtkWindow   *parent,
                                     const gchar *first_button_text,
                                     ...)
{
  GtkWidget *result;
  va_list varargs;

  va_start (varargs, first_button_text);
  result = glade_named_icon_chooser_dialog_new_valist (title,
                                                       parent,
                                                       first_button_text,
                                                       varargs);
  va_end (varargs);

  return result;
}
