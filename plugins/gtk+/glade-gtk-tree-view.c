/*
 * glade-gtk-tree-view.c - GladeWidgetAdaptor for GtkTreeView
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
 * License along with this program; if not, write to the Free Softwarel
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include <glib/gi18n-lib.h>
#include <gladeui/glade.h>

#include "glade-gtk.h"
#include "glade-gtk-tree-view.h"
#include "glade-gtk-cell-layout.h"

#include "glade-treeview-editor.h"
#include "glade-real-tree-view-editor.h"


GladeEditable *
glade_gtk_treeview_create_editable (GladeWidgetAdaptor *adaptor,
                                    GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    {
      return (GladeEditable *)glade_real_tree_view_editor_new ();
    }

  return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}

gboolean
glade_gtk_treeview_add_verify (GladeWidgetAdaptor *adaptor,
                               GtkWidget          *container,
                               GtkWidget          *child,
                               gboolean            user_feedback)
{
  if (!GTK_IS_TREE_VIEW_COLUMN (child))
    {
      if (user_feedback)
        {
          GladeWidgetAdaptor *cell_adaptor = 
            glade_widget_adaptor_get_by_type (GTK_TYPE_TREE_VIEW_COLUMN);

          glade_util_ui_message (glade_app_get_window (),
                                 GLADE_UI_INFO, NULL,
                                 ONLY_THIS_GOES_IN_THAT_MSG,
                                 glade_widget_adaptor_get_title (cell_adaptor),
                                 glade_widget_adaptor_get_title (adaptor));
        }

      return FALSE;
    }

  return TRUE;
}

void
glade_gtk_treeview_launch_editor (GObject *treeview)
{
  GladeBaseEditor    *editor;
  GtkWidget          *window;

  /* Editor */
  editor = glade_base_editor_new (treeview, NULL,
                                  _("Column"), GTK_TYPE_TREE_VIEW_COLUMN, NULL);

  glade_base_editor_append_types (editor, GTK_TYPE_TREE_VIEW_COLUMN,
                                  _("Text"), GTK_TYPE_CELL_RENDERER_TEXT,
                                  _("Accelerator"),
                                  GTK_TYPE_CELL_RENDERER_ACCEL, _("Combo"),
                                  GTK_TYPE_CELL_RENDERER_COMBO, _("Spin"),
                                  GTK_TYPE_CELL_RENDERER_SPIN, _("Pixbuf"),
                                  GTK_TYPE_CELL_RENDERER_PIXBUF, _("Progress"),
                                  GTK_TYPE_CELL_RENDERER_PROGRESS, _("Toggle"),
                                  GTK_TYPE_CELL_RENDERER_TOGGLE, _("Spinner"),
                                  GTK_TYPE_CELL_RENDERER_SPINNER, NULL);

  g_signal_connect (editor, "get-display-name",
                    G_CALLBACK (glade_gtk_cell_layout_get_display_name), NULL);
  g_signal_connect (editor, "child-selected",
                    G_CALLBACK (glade_gtk_cell_layout_child_selected), NULL);
  g_signal_connect (editor, "move-child",
                    G_CALLBACK (glade_gtk_cell_layout_move_child), NULL);

  gtk_widget_show (GTK_WIDGET (editor));

  window =
      glade_base_editor_pack_new_window (editor, _("Tree View Editor"), NULL);
  gtk_widget_show (window);
}

void
glade_gtk_treeview_action_activate (GladeWidgetAdaptor *adaptor,
                                    GObject            *object,
                                    const gchar        *action_path)
{
  if (strcmp (action_path, "launch_editor") == 0)
    {
      glade_gtk_treeview_launch_editor (object);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->action_activate (adaptor,
                                                         object, action_path);
}

static gint
glade_gtk_treeview_get_column_index (GtkTreeView * view,
                                     GtkTreeViewColumn * column)
{
  GtkTreeViewColumn *iter;
  gint i;

  for (i = 0; (iter = gtk_tree_view_get_column (view, i)) != NULL; i++)
    if (iter == column)
      return i;

  return -1;
}

void
glade_gtk_treeview_get_child_property (GladeWidgetAdaptor *adaptor,
                                       GObject            *container,
                                       GObject            *child,
                                       const gchar        *property_name,
                                       GValue             *value)
{
  if (strcmp (property_name, "position") == 0)
    g_value_set_int (value,
                     glade_gtk_treeview_get_column_index (GTK_TREE_VIEW
                                                          (container),
                                                          GTK_TREE_VIEW_COLUMN
                                                          (child)));
  else
    /* Chain Up */
    GWA_GET_CLASS
        (GTK_TYPE_CONTAINER)->child_get_property (adaptor,
                                                  container, child,
                                                  property_name, value);
}

void
glade_gtk_treeview_set_child_property (GladeWidgetAdaptor *adaptor,
                                       GObject            *container,
                                       GObject            *child,
                                       const gchar        *property_name,
                                       const GValue       *value)
{
  if (strcmp (property_name, "position") == 0)
    {

      gtk_tree_view_remove_column (GTK_TREE_VIEW (container),
                                   GTK_TREE_VIEW_COLUMN (child));
      gtk_tree_view_insert_column (GTK_TREE_VIEW (container),
                                   GTK_TREE_VIEW_COLUMN (child),
                                   g_value_get_int (value));
    }
  else
    /* Chain Up */
    GWA_GET_CLASS
        (GTK_TYPE_CONTAINER)->child_set_property (adaptor,
                                                  container, child,
                                                  property_name, value);
}

GList *
glade_gtk_treeview_get_children (GladeWidgetAdaptor *adaptor,
                                 GtkTreeView        *view)
{
  GList *children;

  children = gtk_tree_view_get_columns (view);
  children = g_list_prepend (children, gtk_tree_view_get_selection (view));

  return children;
}

/* XXX FIXME: We should hide the actual "fixed-height-mode" setting from
 * treeview editors and provide a custom control that sets all its columns
 * to fixed size and then control the column's sensitivity accordingly.
 */
#define INSENSITIVE_COLUMN_SIZING_MSG \
        _("Columns must have a fixed size inside a treeview with fixed height mode set")

void
glade_gtk_treeview_add_child (GladeWidgetAdaptor * adaptor,
                              GObject * container, GObject * child)
{
  GtkTreeView *view = GTK_TREE_VIEW (container);
  GtkTreeViewColumn *column;
  GladeWidget *gcolumn;

  if (!GTK_IS_TREE_VIEW_COLUMN (child))
    return;

  if (gtk_tree_view_get_fixed_height_mode (view))
    {
      gcolumn = glade_widget_get_from_gobject (child);
      glade_widget_property_set (gcolumn, "sizing", GTK_TREE_VIEW_COLUMN_FIXED);
      glade_widget_property_set_sensitive (gcolumn, "sizing", FALSE,
                                           INSENSITIVE_COLUMN_SIZING_MSG);
    }

  column = GTK_TREE_VIEW_COLUMN (child);
  gtk_tree_view_append_column (view, column);

  glade_gtk_cell_layout_sync_attributes (G_OBJECT (column));
}

void
glade_gtk_treeview_remove_child (GladeWidgetAdaptor *adaptor,
                                 GObject            *container,
                                 GObject            *child)
{
  GtkTreeView *view = GTK_TREE_VIEW (container);
  GtkTreeViewColumn *column;

  if (!GTK_IS_TREE_VIEW_COLUMN (child))
    return;

  column = GTK_TREE_VIEW_COLUMN (child);
  gtk_tree_view_remove_column (view, column);
}

void
glade_gtk_treeview_replace_child (GladeWidgetAdaptor *adaptor,
                                  GObject            *container,
                                  GObject            *current, 
                                  GObject            *new_column)
{
  GtkTreeView *view = GTK_TREE_VIEW (container);
  GList *columns;
  GtkTreeViewColumn *column;
  GladeWidget *gcolumn;
  gint index;

  if (!GTK_IS_TREE_VIEW_COLUMN (current))
    return;

  column = GTK_TREE_VIEW_COLUMN (current);

  columns = gtk_tree_view_get_columns (view);
  index = g_list_index (columns, column);
  g_list_free (columns);

  gtk_tree_view_remove_column (view, column);
  column = GTK_TREE_VIEW_COLUMN (new_column);

  gtk_tree_view_insert_column (view, column, index);

  if (gtk_tree_view_get_fixed_height_mode (view))
    {
      gcolumn = glade_widget_get_from_gobject (column);
      glade_widget_property_set (gcolumn, "sizing", GTK_TREE_VIEW_COLUMN_FIXED);
      glade_widget_property_set_sensitive (gcolumn, "sizing", FALSE,
                                           INSENSITIVE_COLUMN_SIZING_MSG);
    }

  glade_gtk_cell_layout_sync_attributes (G_OBJECT (column));
}

void
glade_gtk_treeview_set_property (GladeWidgetAdaptor *adaptor,
                                 GObject            *object,
                                 const gchar        *id,
                                 const GValue       *value)
{
  GladeWidget *widget = glade_widget_get_from_gobject (object);
  GladeProperty *property = glade_widget_get_property (widget, id);

  if (strcmp (id, "enable-search") == 0)
    {
      if (g_value_get_boolean (value))
        glade_widget_property_set_sensitive (widget, "search-column", TRUE, NULL);
      else
        glade_widget_property_set_sensitive (widget, "search-column", FALSE, _("Search is disabled"));
    }
  else if (strcmp (id, "headers-visible") == 0)
    {
      if (g_value_get_boolean (value))
        glade_widget_property_set_sensitive (widget, "headers-clickable", TRUE, NULL);
      else
        glade_widget_property_set_sensitive (widget, "headers-clickable", FALSE, _("Headers are invisible"));
    }
  else if (strcmp (id, "show-expanders") == 0)
    {
      if (g_value_get_boolean (value))
        glade_widget_property_set_sensitive (widget, "expander-column", TRUE, NULL);
      else
        glade_widget_property_set_sensitive (widget, "expander-column", FALSE, _("Expanders are not shown"));
    }

  if (GPC_VERSION_CHECK (glade_property_get_def (property), gtk_major_version, gtk_minor_version + 1))
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id, value);
}
