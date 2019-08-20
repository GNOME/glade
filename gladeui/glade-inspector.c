/*
 * glade-inspector.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2007 Vincent Geddes
 *
 * Authors:
 *   Chema Celorio
 *   Tristan Van Berkom <tvb@gnome.org>
 *   Vincent Geddes <vincent.geddes@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

/**
 * SECTION:glade-inspector
 * @Short_Description: A widget for inspecting objects in a #GladeProject.
 *
 * A #GladeInspector is a widget for inspecting the objects that make up a user interface. 
 *
 * An inspector is created by calling either glade_inspector_new() or glade_inspector_new_with_project(). 
 * The current project been inspected can be changed by calling glade_inspector_set_project().
 */

#include "glade.h"
#include "glade-widget.h"
#include "glade-project.h"
#include "glade-widget-adaptor.h"
#include "glade-inspector.h"
#include "glade-popup.h"
#include "glade-app.h"
#include "glade-dnd.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#if GTK_CHECK_VERSION (2, 21, 8)
#include <gdk/gdkkeysyms-compat.h>
#else
#include <gdk/gdkkeysyms.h>
#endif

static void     search_entry_text_inserted_cb (GtkEntry       *entry,
                                               const gchar    *text,
                                               gint            length,
                                               gint           *position,
                                               GladeInspector *inspector);
static void     search_entry_text_deleted_cb  (GtkEditable    *editable,
                                               gint            start_pos,
                                               gint            end_pos,
                                               GladeInspector *inspector);

enum
{
  PROP_0,
  PROP_PROJECT,
  N_PROPERTIES
};

enum
{
  SELECTION_CHANGED,
  ITEM_ACTIVATED,
  LAST_SIGNAL
};

typedef struct _GladeInspectorPrivate
{
  GtkWidget *view;
  GtkTreeModel *filter;

  GladeProject *project;

  GtkWidget *entry;
  guint idle_complete;
  gboolean search_disabled;
  gchar *completion_text;
  gchar *completion_text_fold;
} GladeInspectorPrivate;

static GParamSpec *properties[N_PROPERTIES];
static guint glade_inspector_signals[LAST_SIGNAL] = { 0 };


static void glade_inspector_dispose (GObject *object);
static void glade_inspector_finalize (GObject *object);
static void add_columns (GtkTreeView *inspector);
static void item_activated_cb (GtkTreeView       *view,
                               GtkTreePath       *path,
                               GtkTreeViewColumn *column,
                               GladeInspector    *inspector);
static void selection_changed_cb (GtkTreeSelection *selection,
                                  GladeInspector   *inspector);
static gint button_press_cb (GtkWidget      *widget,
                             GdkEventButton *event,
                             GladeInspector *inspector);

G_DEFINE_TYPE_WITH_PRIVATE (GladeInspector, glade_inspector, GTK_TYPE_BOX)

static void
glade_inspector_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GladeInspector *inspector = GLADE_INSPECTOR (object);

  switch (property_id)
    {
      case PROP_PROJECT:
        glade_inspector_set_project (inspector, g_value_get_object (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
glade_inspector_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GladeInspector *inspector = GLADE_INSPECTOR (object);

  switch (property_id)
    {
      case PROP_PROJECT:
        g_value_set_object (value, glade_inspector_get_project (inspector));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
glade_inspector_class_init (GladeInspectorClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = glade_inspector_dispose;
  object_class->finalize = glade_inspector_finalize;
  object_class->set_property = glade_inspector_set_property;
  object_class->get_property = glade_inspector_get_property;

  /**
   * GladeInspector::selection-changed:
   * @inspector: the object which received the signal
   *
   * Emitted when the selection changes in the GladeInspector.
   */
  glade_inspector_signals[SELECTION_CHANGED] =
      g_signal_new ("selection-changed",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeInspectorClass, selection_changed),
                    NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * GladeInspector::item-activated:
   * @inspector: the object which received the signal
   *
   * Emitted when a item is activated in the GladeInspector.
   */
  glade_inspector_signals[ITEM_ACTIVATED] =
      g_signal_new ("item-activated",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeInspectorClass, item_activated),
                    NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  properties[PROP_PROJECT] =
    g_param_spec_object ("project",
                         _("Project"),
                         _("The project being inspected"),
                         GLADE_TYPE_PROJECT,
                         G_PARAM_READABLE | G_PARAM_WRITABLE);
  
  /* Install all properties */
  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static gboolean
glade_inspector_visible_func (GtkTreeModel *model,
                              GtkTreeIter  *parent,
                              gpointer      data)
{
  GladeInspector *inspector = data;
  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);

  GtkTreeIter iter;

  gboolean retval = FALSE;

  if (priv->search_disabled || priv->completion_text == NULL)
    return TRUE;

  if (gtk_tree_model_iter_children (model, &iter, parent))
    {
      do
        {
          retval = glade_inspector_visible_func (model, &iter, data);
        }
      while (gtk_tree_model_iter_next (model, &iter) && !retval);
    }

  if (!retval)
    {
      gchar *widget_name, *haystack;

      gtk_tree_model_get (model, parent, GLADE_PROJECT_MODEL_COLUMN_NAME,
                          &widget_name, -1);

      haystack = g_utf8_casefold (widget_name, -1);

      retval = strstr (haystack, priv->completion_text_fold) != NULL;

      g_free (haystack);
      g_free (widget_name);
    }

  return retval;
}

static void
glade_inspector_refilter (GladeInspector *inspector)
{
  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);

  if (!priv->search_disabled)
    {
      gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->filter));
      gtk_tree_view_expand_all (GTK_TREE_VIEW (priv->view));
    }
}

static void
search_entry_changed_cb (GtkEntry *entry, GladeInspector *inspector)
{
  glade_inspector_refilter (inspector);
}

typedef struct {
  const gchar    *text;
  gchar          *common_text;
  gchar          *first_match;
} CommonMatchData;

static void
reduce_string (gchar *str1,
               const gchar *str2)
{
  gint str1len = strlen (str1);
  gint i;

  for (i = 0; str2[i] != '\0'; i++)
    {

      if (str1[i] != str2[i] || i >= str1len)
        {
          str1[i] = '\0';
          break;
        }
    }

  if (str2[i] == '\0')
    str1[i] = '\0';
}

static gboolean
search_common_matches (GtkTreeModel    *model,
                       GtkTreePath     *path,
                       GtkTreeIter     *iter,
                       CommonMatchData *data)
{
  GladeWidget *gwidget;
  const gchar *name;
  GObject *obj;
  gboolean match;

  gtk_tree_model_get (model, iter, GLADE_PROJECT_MODEL_COLUMN_OBJECT, &obj, -1);
  gwidget = glade_widget_get_from_gobject (obj);

  if (!glade_widget_has_name (gwidget))
    {
      g_object_unref (obj);
      return FALSE;
    }

  name  = glade_widget_get_name (gwidget);
  match = (strncmp (data->text, name, strlen (data->text)) == 0);

  if (match)
    {
      if (!data->first_match)
        data->first_match = g_strdup (name);

      if (data->common_text)
        reduce_string (data->common_text, name);
      else
        data->common_text = g_strdup (name);
    }

  g_object_unref (obj);
  return FALSE;
}

/* Returns the shortest common matching text from all
 * project widget names.
 *
 * If shortest_match is specified, it is given the first
 * full match for the 'search' text
 */
static gchar *
get_partial_match (GladeInspector *inspector,
                   const gchar    *search,
                   gchar         **first_match)
{
  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);
  GtkTreeModel     *model = GTK_TREE_MODEL (priv->project);
  CommonMatchData   data;

  data.text        = search;
  data.common_text = NULL;
  data.first_match = NULL;
  
  gtk_tree_model_foreach (model, (GtkTreeModelForeachFunc)search_common_matches, &data);

  if (first_match)
    *first_match = data.first_match;
  else
    g_free (data.first_match);

  return data.common_text;
}

static void
inspector_set_completion_text (GladeInspector *inspector, const gchar *text)
{
  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);

  g_free (priv->completion_text);
  priv->completion_text = g_strdup (text);
  priv->completion_text_fold = text ? g_utf8_casefold (text, -1) : NULL;

}

static gboolean
search_complete_idle (GladeInspector *inspector)
{
  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);
  gchar *completed;
  const gchar *str;
  gsize length;

  str = gtk_entry_get_text (GTK_ENTRY (priv->entry));

  completed = get_partial_match (inspector, str, NULL);
  
  inspector_set_completion_text (inspector, str);

  if (completed)
    {
      length = strlen (str);

      g_signal_handlers_block_by_func (priv->entry, search_entry_text_inserted_cb, inspector);
      g_signal_handlers_block_by_func (priv->entry, search_entry_text_deleted_cb, inspector);

      gtk_entry_set_text (GTK_ENTRY (priv->entry), completed);
      gtk_editable_set_position (GTK_EDITABLE (priv->entry), length);
      gtk_editable_select_region (GTK_EDITABLE (priv->entry), length, -1);
      g_free (completed);

      g_signal_handlers_unblock_by_func (priv->entry, search_entry_text_inserted_cb, inspector);
      g_signal_handlers_unblock_by_func (priv->entry, search_entry_text_deleted_cb, inspector);

    }

  priv->idle_complete = 0;

  return FALSE;
}

static void
search_entry_text_inserted_cb (GtkEntry       *entry,
                               const gchar    *text,
                               gint            length,
                               gint           *position,
                               GladeInspector *inspector)
{
  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);

  if (!priv->search_disabled && !priv->idle_complete)
    {
      priv->idle_complete =
          g_idle_add ((GSourceFunc) search_complete_idle, inspector);
    }
}

static void
search_entry_text_deleted_cb (GtkEditable    *editable,
                              gint            start_pos,
                              gint            end_pos,
                              GladeInspector *inspector)
{
  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);

  if (!priv->search_disabled)
    {
      inspector_set_completion_text (inspector, gtk_entry_get_text (GTK_ENTRY (priv->entry)));
      glade_inspector_refilter (inspector);
    }
}

static gboolean
search_entry_key_press_event_cb (GtkEntry       *entry,
                                 GdkEventKey    *event,
                                 GladeInspector *inspector)
{
  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);
  const gchar *str;

  str = gtk_entry_get_text (GTK_ENTRY (priv->entry));

  if (event->keyval == GDK_KEY_Tab)
    {
      /* CNTL-Tab: An escape route to move focus */
      if (event->state & GDK_CONTROL_MASK)
        {
          gtk_widget_grab_focus (priv->view);
        }
      else /* Tab: Move cursor forward and refine the filter to include all text */
        {
          inspector_set_completion_text (inspector, str);

          gtk_editable_set_position (GTK_EDITABLE (entry), -1);
          gtk_editable_select_region (GTK_EDITABLE (entry), -1, -1);

          glade_inspector_refilter (inspector);
        }
      return TRUE;
    }

  /* Enter/Return: Find the first complete match, refine filter to the complete match, and select the match  */
  if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)
    {
      gchar *name, *full_match = NULL;

      if (str && (name = get_partial_match (inspector, str, &full_match)))
        {
          GladeWidget *widget;

          inspector_set_completion_text (inspector, full_match);
          g_free (name);

          g_signal_handlers_block_by_func (priv->entry, search_entry_text_inserted_cb, inspector);
          g_signal_handlers_block_by_func (priv->entry, search_entry_text_deleted_cb, inspector);

          gtk_entry_set_text (GTK_ENTRY (entry), priv->completion_text);

          g_signal_handlers_unblock_by_func (priv->entry, search_entry_text_inserted_cb, inspector);
          g_signal_handlers_unblock_by_func (priv->entry, search_entry_text_deleted_cb, inspector);

          gtk_editable_set_position (GTK_EDITABLE (entry), -1);
          gtk_editable_select_region (GTK_EDITABLE (entry), -1, -1);

          glade_inspector_refilter (inspector);

          widget = glade_project_get_widget_by_name (priv->project, priv->completion_text);
          if (widget)
            glade_project_selection_set (priv->project, glade_widget_get_object (widget), TRUE);
        }
      return TRUE;
    }

  /* Backspace: Attempt to move the cursor backwards and maintain the completed/selected portion */
  if (event->keyval == GDK_KEY_BackSpace)
    {
      if (!priv->search_disabled && !priv->idle_complete && str && str[0])
        {
          /* Now, set the text to the current completion text -1 char, and recomplete */
          if (priv->completion_text && priv->completion_text[0])
            {
              /* If we're not at the position of the length of the completion text, just carry on */
              if (!gtk_editable_get_selection_bounds (GTK_EDITABLE (priv->entry), NULL, NULL))
                return FALSE;

              priv->completion_text[strlen (priv->completion_text) -1] = '\0';

              g_signal_handlers_block_by_func (priv->entry, search_entry_text_inserted_cb, inspector);
              g_signal_handlers_block_by_func (priv->entry, search_entry_text_deleted_cb, inspector);

              gtk_entry_set_text (GTK_ENTRY (priv->entry), priv->completion_text);
              gtk_editable_set_position (GTK_EDITABLE (priv->entry), -1);

              g_signal_handlers_unblock_by_func (priv->entry, search_entry_text_inserted_cb, inspector);
              g_signal_handlers_unblock_by_func (priv->entry, search_entry_text_deleted_cb, inspector);

              priv->idle_complete =
                g_idle_add ((GSourceFunc) search_complete_idle, inspector);

              return TRUE;
            }
        }
    }

  return FALSE;
}

static gboolean
search_entry_focus_in_cb (GtkWidget      *entry,
                          GdkEventFocus  *event,
                          GladeInspector *inspector)
{
  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);

  if (priv->search_disabled)
    priv->search_disabled = FALSE;

  return FALSE;
}

static gboolean
search_entry_focus_out_cb (GtkWidget      *entry,
                           GdkEventFocus  *event,
                           GladeInspector *inspector)
{
  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);

  priv->search_disabled = TRUE;

  inspector_set_completion_text (inspector, NULL);

  gtk_entry_set_text (GTK_ENTRY (priv->entry), "");

  gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (priv->filter));

  return FALSE;
}

static void
glade_inspector_init (GladeInspector *inspector)
{
  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);
  GtkWidget *sw;
  GtkTreeSelection *selection;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (inspector),
                                  GTK_ORIENTATION_VERTICAL);

  priv->project = NULL;

  priv->entry = gtk_entry_new ();

  gtk_entry_set_placeholder_text (GTK_ENTRY (priv->entry), _(" < Search Widgets >"));
  gtk_widget_show (priv->entry);
  gtk_box_pack_start (GTK_BOX (inspector), priv->entry, FALSE, FALSE, 2);

  g_signal_connect (priv->entry, "changed",
                    G_CALLBACK (search_entry_changed_cb), inspector);
  g_signal_connect (priv->entry, "key-press-event",
                    G_CALLBACK (search_entry_key_press_event_cb), inspector);
  g_signal_connect_after (priv->entry, "insert-text",
                          G_CALLBACK (search_entry_text_inserted_cb),
                          inspector);
  g_signal_connect_after (priv->entry, "delete-text",
                          G_CALLBACK (search_entry_text_deleted_cb),
                          inspector);
  g_signal_connect (priv->entry, "focus-in-event",
                    G_CALLBACK (search_entry_focus_in_cb), inspector);
  g_signal_connect (priv->entry, "focus-out-event",
                    G_CALLBACK (search_entry_focus_out_cb), inspector);

  priv->view = gtk_tree_view_new ();
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (priv->view), FALSE);
  gtk_scrollable_set_hscroll_policy (GTK_SCROLLABLE (priv->view), GTK_SCROLL_MINIMUM);
  add_columns (GTK_TREE_VIEW (priv->view));

  /* Set it as a drag source */
  gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (priv->view),
                                          GDK_BUTTON1_MASK,
                                          _glade_dnd_get_target (), 1, GDK_ACTION_MOVE | GDK_ACTION_COPY);

  g_signal_connect (G_OBJECT (priv->view), "row-activated",
                    G_CALLBACK (item_activated_cb), inspector);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (selection_changed_cb), inspector);

  /* Expand All */
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->entry), GTK_ENTRY_ICON_SECONDARY, "go-down");
  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (priv->entry), GTK_ENTRY_ICON_SECONDARY, _("Expand all"));
  g_signal_connect_swapped (priv->entry, "icon-press", G_CALLBACK (gtk_tree_view_expand_all), priv->view);

  /* popup menu */
  g_signal_connect (G_OBJECT (priv->view), "button-press-event",
                    G_CALLBACK (button_press_cb), inspector);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (sw), priv->view);
  gtk_box_pack_start (GTK_BOX (inspector), sw, TRUE, TRUE, 0);

  gtk_widget_show (priv->view);
  gtk_widget_show (sw);
}

static void
glade_inspector_dispose (GObject *object)
{
  GladeInspector *inspector = GLADE_INSPECTOR (object);
  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);

  glade_inspector_set_project (inspector, NULL);

  if (priv->idle_complete)
    {
      g_source_remove (priv->idle_complete);
      priv->idle_complete = 0;
    }

  G_OBJECT_CLASS (glade_inspector_parent_class)->dispose (object);
}

static void
glade_inspector_finalize (GObject *object)
{
  GladeInspector *inspector = GLADE_INSPECTOR (object);
  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);

  g_free (priv->completion_text);
  g_free (priv->completion_text_fold);

  G_OBJECT_CLASS (glade_inspector_parent_class)->finalize (object);
}

static void
project_selection_changed_cb (GladeProject   *project,
                              GladeInspector *inspector)
{
  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);
  GladeWidget *widget;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter *iter;
  GtkTreePath *path, *ancestor_path;
  GList *list;

  g_return_if_fail (GLADE_IS_INSPECTOR (inspector));
  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (priv->project == project);

  g_signal_handlers_block_by_func (gtk_tree_view_get_selection
                                   (GTK_TREE_VIEW (priv->view)),
                                   G_CALLBACK (selection_changed_cb),
                                   inspector);

  selection =
      gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->view));
  g_return_if_fail (selection != NULL);

  model = priv->filter;

  gtk_tree_selection_unselect_all (selection);

  for (list = glade_project_selection_get (project);
       list && list->data; list = list->next)
    {
      if ((widget =
           glade_widget_get_from_gobject (G_OBJECT (list->data))) != NULL)
        {
          if ((iter =
               glade_util_find_iter_by_widget (model, widget,
                                               GLADE_PROJECT_MODEL_COLUMN_OBJECT))
              != NULL)
            {
              path = gtk_tree_model_get_path (model, iter);
              ancestor_path = gtk_tree_path_copy (path);

              /* expand parent node */
              if (gtk_tree_path_up (ancestor_path))
                gtk_tree_view_expand_to_path
                    (GTK_TREE_VIEW (priv->view), ancestor_path);

              gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW
                                            (priv->view), path, NULL,
                                            TRUE, 0.5, 0);

              gtk_tree_selection_select_iter (selection, iter);

              gtk_tree_iter_free (iter);
              gtk_tree_path_free (path);
              gtk_tree_path_free (ancestor_path);
            }
        }
    }

  g_signal_handlers_unblock_by_func (gtk_tree_view_get_selection
                                     (GTK_TREE_VIEW (priv->view)),
                                     G_CALLBACK (selection_changed_cb),
                                     inspector);
}

static void
selection_foreach_func (GtkTreeModel *model,
                        GtkTreePath  *path,
                        GtkTreeIter  *iter,
                        GList       **selection)
{
  GObject *object;

  gtk_tree_model_get (model, iter, GLADE_PROJECT_MODEL_COLUMN_OBJECT, &object,
                      -1);

  if (object)
    {
      *selection = g_list_prepend (*selection, object);
      g_object_unref (object);
    }
}

static void
selection_changed_cb (GtkTreeSelection *selection, GladeInspector *inspector)
{
  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);
  GList *sel = NULL, *l;

  gtk_tree_selection_selected_foreach (selection,
                                       (GtkTreeSelectionForeachFunc)
                                       selection_foreach_func, &sel);

  /* We dont modify the project selection for a change that
   * leaves us with no selection. 
   *
   * This is typically because the user is changing the name
   * of a widget and the filter is active, the new name causes
   * the row to go out of the model and the selection to be
   * cleared, if we clear the selection we remove the editor
   * that the user is trying to type into.
   */
  if (!sel)
    return;

  g_signal_handlers_block_by_func (priv->project,
                                   G_CALLBACK (project_selection_changed_cb),
                                   inspector);

  glade_project_selection_clear (priv->project, FALSE);
  for (l = sel; l; l = l->next)
    glade_project_selection_add (priv->project, G_OBJECT (l->data), FALSE);
  glade_project_selection_changed (priv->project);
  g_list_free (sel);

  g_signal_handlers_unblock_by_func (priv->project,
                                     G_CALLBACK (project_selection_changed_cb),
                                     inspector);

  g_signal_emit (inspector, glade_inspector_signals[SELECTION_CHANGED], 0);
}

static void
item_activated_cb (GtkTreeView       *view,
                   GtkTreePath       *path,
                   GtkTreeViewColumn *column,
                   GladeInspector    *inspector)
{
  g_signal_emit (inspector, glade_inspector_signals[ITEM_ACTIVATED], 0);
}

static gint
button_press_cb (GtkWidget      *widget,
                 GdkEventButton *event,
                 GladeInspector *inspector)
{
  GtkTreeView *view = GTK_TREE_VIEW (widget);
  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);
  GtkTreePath *path = NULL;
  gboolean handled = FALSE;

  /* Give some kind of access in case of missing right button */
  if (event->window == gtk_tree_view_get_bin_window (view) &&
      glade_popup_is_popup_event (event))
    {
      if (gtk_tree_view_get_path_at_pos (view, (gint) event->x, (gint) event->y,
                                         &path, NULL,
                                         NULL, NULL) && path != NULL)
        {
          GtkTreeIter iter;
          GladeWidget *object = NULL;
          if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->project),
                                       &iter, path))
            {
              /* now we can obtain the widget from the iter.
               */
              gtk_tree_model_get (GTK_TREE_MODEL (priv->project), &iter,
                                  GLADE_PROJECT_MODEL_COLUMN_OBJECT, &object,
                                  -1);

              if (widget != NULL)
                glade_popup_widget_pop (glade_widget_get_from_gobject (object),
                                        event, TRUE);
              else
                glade_popup_simple_pop (priv->project, event);

              handled = TRUE;

              gtk_tree_path_free (path);
            }
        }
      else
        {
          glade_popup_simple_pop (priv->project, event);
          handled = TRUE;
        }
    }
  return handled;
}

static void
glade_inspector_warning_cell_data_func (GtkTreeViewColumn *column,
                                        GtkCellRenderer   *renderer,
                                        GtkTreeModel      *model,
                                        GtkTreeIter       *iter,
                                        gpointer           data)
{
  gchar *warning = NULL;

  gtk_tree_model_get (model, iter,
                      GLADE_PROJECT_MODEL_COLUMN_WARNING, &warning,
                      -1);

  g_object_set (renderer, "visible", warning != NULL, NULL);

  g_free (warning);
}

static void
glade_inspector_name_cell_data_func (GtkTreeViewColumn *column,
                                     GtkCellRenderer   *renderer,
                                     GtkTreeModel      *model,
                                     GtkTreeIter       *iter,
                                     gpointer           data)
{
  GladeWidget *gwidget;
  GObject *obj;

  gtk_tree_model_get (model, iter,
                      GLADE_PROJECT_MODEL_COLUMN_OBJECT, &obj,
                      -1);

  gwidget = glade_widget_get_from_gobject (obj);

  g_object_set (renderer, "text", 
                (glade_widget_has_name (gwidget)) ? 
                  glade_widget_get_display_name (gwidget) : NULL,
                NULL);

  g_object_unref (obj);
}


static void
glade_inspector_detail_cell_data_func (GtkTreeViewColumn *column,
                                       GtkCellRenderer   *renderer,
                                       GtkTreeModel      *model,
                                       GtkTreeIter       *iter,
                                       gpointer           data)
{
  gchar *type_name = NULL, *detail = NULL;

  gtk_tree_model_get (model, iter,
                      GLADE_PROJECT_MODEL_COLUMN_TYPE_NAME, &type_name,
                      GLADE_PROJECT_MODEL_COLUMN_MISC, &detail,
                      -1);

  if (detail)
    {
      gchar *final = g_strconcat (type_name, "  ", detail, NULL);

      g_object_set (renderer, "text", final, NULL);

      g_free (final);
    }
  else
      g_object_set (renderer, "text", type_name, NULL);

  g_free (type_name);
  g_free (detail);
}




static void
add_columns (GtkTreeView *view)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkCellAreaBox *box;

  /* Use a GtkCellArea box to set the alignments manually */
  box = (GtkCellAreaBox *)gtk_cell_area_box_new ();

  column = gtk_tree_view_column_new_with_area (GTK_CELL_AREA (box));
  gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
  gtk_cell_area_box_set_spacing (GTK_CELL_AREA_BOX (box), 2);

  gtk_tree_view_set_tooltip_column (view, GLADE_PROJECT_MODEL_COLUMN_WARNING);

  /* Padding */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), "width", 4, NULL);
  gtk_cell_area_box_pack_start (box, renderer, FALSE, FALSE, FALSE);

  /* Warning cell */
  renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (renderer,
                "stock-id", "gtk-dialog-warning",
                "xpad", 2,
                NULL);
  gtk_cell_area_box_pack_start (box, renderer, FALSE, FALSE, FALSE);
  gtk_tree_view_column_set_cell_data_func (column, renderer,
                                           glade_inspector_warning_cell_data_func,
                                           NULL, NULL);

  /* Class Icon */
  renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (renderer,
                "xpad", 2,
                NULL);
  gtk_cell_area_box_pack_start (box, renderer, FALSE, FALSE, FALSE);
  gtk_tree_view_column_set_attributes (column,
                                       renderer,
                                       "icon_name",
                                       GLADE_PROJECT_MODEL_COLUMN_ICON_NAME,
                                       NULL);

  /* Widget Name */
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_area_box_pack_start (box, renderer, FALSE, FALSE, FALSE);
  gtk_tree_view_column_set_attributes (column,
                                       renderer,
                                       "text", GLADE_PROJECT_MODEL_COLUMN_NAME,
                                       NULL);
  gtk_tree_view_column_set_cell_data_func (column, renderer,
                                           glade_inspector_name_cell_data_func,
                                           NULL, NULL);

  /* Padding */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), "width", 8, NULL);
  gtk_cell_area_box_pack_start (box, renderer, FALSE, FALSE, FALSE);

  /* Class name & detail */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer),
                "style", PANGO_STYLE_ITALIC,
                "foreground", "Gray",
                "ellipsize", PANGO_ELLIPSIZE_END,
                NULL);

  gtk_cell_area_box_pack_start (box, renderer, FALSE, FALSE, FALSE);
  gtk_tree_view_column_set_cell_data_func (column, renderer,
                                           glade_inspector_detail_cell_data_func,
                                           NULL, NULL);

  gtk_tree_view_append_column (view, column);
  gtk_tree_view_set_headers_visible (view, FALSE);
}

static void
disconnect_project_signals (GladeInspector *inspector, GladeProject *project)
{
  g_signal_handlers_disconnect_by_func (G_OBJECT (project),
                                        G_CALLBACK
                                        (project_selection_changed_cb),
                                        inspector);
}

static void
connect_project_signals (GladeInspector *inspector, GladeProject *project)
{
  g_signal_connect (G_OBJECT (project), "selection-changed",
                    G_CALLBACK (project_selection_changed_cb), inspector);
}

/**
 * glade_inspector_set_project:
 * @inspector: a #GladeInspector
 * @project: a #GladeProject
 *
 * Sets the current project of @inspector to @project. To unset the current
 * project, pass %NULL for @project.
 */
void
glade_inspector_set_project (GladeInspector *inspector, GladeProject *project)
{
  g_return_if_fail (GLADE_IS_INSPECTOR (inspector));
  g_return_if_fail (GLADE_IS_PROJECT (project) || project == NULL);

  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);

  if (priv->project == project)
    return;

  if (priv->project)
    {
      disconnect_project_signals (inspector, priv->project);

      /* Release our filter which releases the project */
      gtk_tree_view_set_model (GTK_TREE_VIEW (priv->view), NULL);
      priv->filter = NULL;
      priv->project = NULL;
    }

  if (project)
    {
      priv->project = project;

      /* The filter holds our reference to 'project' */
      priv->filter =
          gtk_tree_model_filter_new (GTK_TREE_MODEL (priv->project), NULL);

      gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER
                                              (priv->filter),
                                              (GtkTreeModelFilterVisibleFunc)
                                              glade_inspector_visible_func,
                                              inspector, NULL);

      gtk_tree_view_set_model (GTK_TREE_VIEW (priv->view), priv->filter);
      g_object_unref (priv->filter);    /* pass ownership of the filter to the model */

      connect_project_signals (inspector, project);
    }

  g_object_notify_by_pspec (G_OBJECT (inspector), properties[PROP_PROJECT]);
}

/**
 * glade_inspector_get_project:
 * @inspector: a #GladeInspector
 * 
 * Note that the method does not ref the returned #GladeProject.
 *
 * Returns: (transfer none): A #GladeProject
 */
GladeProject *
glade_inspector_get_project (GladeInspector *inspector)
{
  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);

  g_return_val_if_fail (GLADE_IS_INSPECTOR (inspector), NULL);

  return priv->project;
}

/**
 * glade_inspector_get_selected_items:
 * @inspector: a #GladeInspector
 * 
 * Returns the selected items in the inspector. 
 *
 * Returns: (transfer container) (element-type GladeWidget): A #GList of #GladeWidget
 */
GList *
glade_inspector_get_selected_items (GladeInspector *inspector)
{
  GtkTreeSelection *selection;
  GList *items = NULL, *paths;
  GladeInspectorPrivate *priv = glade_inspector_get_instance_private (inspector);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->view));

  for (paths = gtk_tree_selection_get_selected_rows (selection, NULL);
       paths != NULL; paths = g_list_next (paths->next))
    {
      GtkTreeIter filter_iter;
      GtkTreeIter iter;
      GtkTreePath *path = (GtkTreePath *) paths->data;
      GObject *object = NULL;

      gtk_tree_model_get_iter (priv->filter, &filter_iter, path);
      gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER
                                                        (priv->filter), &iter,
                                                        &filter_iter);
      gtk_tree_model_get (GTK_TREE_MODEL (priv->project), &iter,
                          GLADE_PROJECT_MODEL_COLUMN_OBJECT, &object, -1);

      g_object_unref (object);
      items = g_list_prepend (items, glade_widget_get_from_gobject (object));
    }

  g_list_foreach (paths, (GFunc) gtk_tree_path_free, NULL);
  g_list_free (paths);

  return items;
}

/**
 * glade_inspector_new:
 * 
 * Creates a new #GladeInspector
 * 
 * Returns: (transfer full): a new #GladeInspector
 */
GtkWidget *
glade_inspector_new (void)
{
  return g_object_new (GLADE_TYPE_INSPECTOR, NULL);
}

/**
 * glade_inspector_new_with_project:
 * @project: a #GladeProject
 *
 * Creates a new #GladeInspector with @project
 * 
 * Returns: (transfer full): a new #GladeInspector
 */
GtkWidget *
glade_inspector_new_with_project (GladeProject *project)
{
  GladeInspector *inspector;
  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  inspector = g_object_new (GLADE_TYPE_INSPECTOR, "project", project, NULL);

  /* Make sure we expended to the right path */
  project_selection_changed_cb (project, inspector);

  return GTK_WIDGET (inspector);
}
