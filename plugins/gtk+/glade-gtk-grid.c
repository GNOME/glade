/*
 * glade-gtk-grid.c - GladeWidgetAdaptor for GtkGrid widget
 *
 * Copyright (C) 2011 Openismus GmbH
 *
 * Authors:
 *      Tristan Van Berkom <tristanvb@openismus.com>
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

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include "glade-fixed.h"

typedef struct
{
  gint left_attach;
  gint top_attach;
  gint width;
  gint height;
} GladeGridAttachments;

typedef struct
{
  /* comparable part: */
  GladeWidget *widget;
  gint left_attach;
  gint top_attach;
  gint width;
  gint height;
} GladeGridChild;

typedef enum
{
  DIR_UP,
  DIR_DOWN,
  DIR_LEFT,
  DIR_RIGHT
} GladeGridDir;

static void
glade_gtk_grid_get_child_attachments (GtkWidget            *grid,
				      GtkWidget            *child,
				      GladeGridAttachments *grid_child)
{
  gtk_container_child_get (GTK_CONTAINER (grid), child,
                           "left-attach", &grid_child->left_attach,
                           "width",       &grid_child->width,
                           "top-attach",  &grid_child->top_attach,
                           "height",      &grid_child->height,
			   NULL);
}

static void
glade_gtk_grid_parse_finished (GladeProject *project, GObject *container)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (container);
  GladeGridAttachments attach;
  GList *list, *children;
  gint row = 0, column = 0, n_row = 0, n_column = 0;

  children = gtk_container_get_children (GTK_CONTAINER (container));

  for (list = children; list; list = list->next)
    {
      GtkWidget *widget = list->data;

      if (GLADE_IS_PLACEHOLDER (widget)) continue;

      glade_gtk_grid_get_child_attachments (GTK_WIDGET (container), widget, &attach);

      n_row = attach.top_attach + attach.height;
      n_column = attach.left_attach + attach.width;
      
      if (row < n_row) row = n_row;
      if (column < n_column) column = n_column;
    }

  if (column) glade_widget_property_set (gwidget, "n-columns", column);
  if (row) glade_widget_property_set (gwidget, "n-rows", row);

  g_list_free (children);
}

void
glade_gtk_grid_post_create (GladeWidgetAdaptor * adaptor,
                             GObject * container, GladeCreateReason reason)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (container);

  if (reason == GLADE_CREATE_LOAD)
    g_signal_connect (glade_widget_get_project (gwidget), "parse-finished",
		      G_CALLBACK (glade_gtk_grid_parse_finished),
		      container);
}

static gboolean
glade_gtk_grid_has_child (GtkGrid *grid,
                          GList *children,
                          guint left_attach,
                          guint top_attach)
{
  gboolean ret = FALSE;
  GList *list;

  for (list = children; list && list->data; list = list->next)
    {
      GladeGridAttachments attach;
      GtkWidget *widget = list->data;

      glade_gtk_grid_get_child_attachments (GTK_WIDGET (grid), widget, &attach);

      if (left_attach >= attach.left_attach && left_attach < attach.left_attach + attach.width && 
	  top_attach  >= attach.top_attach  && top_attach  < attach.top_attach  + attach.height)
        {
          ret = TRUE;
          break;
        }
    }

  return ret;
}

static gboolean
glade_gtk_grid_widget_exceeds_bounds (GtkGrid *grid, gint n_rows, gint n_cols)
{
  GList *list, *children;
  gboolean ret = FALSE;

  children = gtk_container_get_children (GTK_CONTAINER (grid));

  for (list = children; list && list->data; list = g_list_next (list))
    {
      GladeGridAttachments attach;
      GtkWidget *widget = list->data;

      if (GLADE_IS_PLACEHOLDER (widget)) continue;
          
      glade_gtk_grid_get_child_attachments (GTK_WIDGET (grid), widget, &attach);

      if (attach.left_attach + attach.width  > n_cols || 
	  attach.top_attach  + attach.height > n_rows)
        {
          ret = TRUE;
          break;
        }
    }

  g_list_free (children);

  return ret;
}

static void
glade_gtk_grid_refresh_placeholders (GtkGrid *grid)
{
  GladeWidget *widget;
  GtkContainer *container;
  GList *list, *children;
  guint n_columns, n_rows;
  gint i, j;

  widget = glade_widget_get_from_gobject (grid);
  glade_widget_property_get (widget, "n-columns", &n_columns);
  glade_widget_property_get (widget, "n-rows", &n_rows);

  container = GTK_CONTAINER (grid);
  children = gtk_container_get_children (container);

  for (list = children; list && list->data; list = list->next)
    {
      GtkWidget *child = list->data;
      if (GLADE_IS_PLACEHOLDER (child))
        gtk_container_remove (container, child);
    }
  g_list_free (children);

  children = gtk_container_get_children (container);

  for (i = 0; i < n_columns; i++)
    for (j = 0; j < n_rows; j++)
      if (glade_gtk_grid_has_child (grid, children, i, j) == FALSE)
        gtk_grid_attach (grid, glade_placeholder_new (), i, j, 1, 1);

  gtk_container_check_resize (container);
  g_list_free (children);
}

static void
gtk_grid_children_callback (GtkWidget * widget, gpointer client_data)
{
  GList **children;

  children = (GList **) client_data;
  *children = g_list_prepend (*children, widget);
}

GList *
glade_gtk_grid_get_children (GladeWidgetAdaptor * adaptor,
                              GtkContainer * container)
{
  GList *children = NULL;

  g_return_val_if_fail (GTK_IS_GRID (container), NULL);

  gtk_container_forall (container, gtk_grid_children_callback, &children);

  /* GtkGrid has the children list already reversed */
  return children;
}

void
glade_gtk_grid_add_child (GladeWidgetAdaptor * adaptor,
                           GObject * object, GObject * child)
{
  g_return_if_fail (GTK_IS_GRID (object));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));

  glade_gtk_grid_refresh_placeholders (GTK_GRID (object));
}

void
glade_gtk_grid_remove_child (GladeWidgetAdaptor * adaptor,
                              GObject * object, GObject * child)
{
  g_return_if_fail (GTK_IS_GRID (object));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));

  glade_gtk_grid_refresh_placeholders (GTK_GRID (object));
}

void
glade_gtk_grid_replace_child (GladeWidgetAdaptor *adaptor,
                              GObject *container,
                              GObject *current,
                              GObject *new_widget)
{
  g_return_if_fail (GTK_IS_GRID (container));
  g_return_if_fail (GTK_IS_WIDGET (current));
  g_return_if_fail (GTK_IS_WIDGET (new_widget));

  /* Chain Up */
  GWA_GET_CLASS (GTK_TYPE_CONTAINER)->replace_child (adaptor,
                                                     container,
                                                     current,
                                                     new_widget);

  /* If we are replacing a GladeWidget, we must refresh placeholders
   * because the widget may have spanned multiple rows/columns, we must
   * not do so in the case we are pasting multiple widgets into a grid,
   * where destroying placeholders results in default packing properties
   * (since the remaining placeholder templates no longer exist, only the
   * first pasted widget would have proper packing properties).
   */
  if (!GLADE_IS_PLACEHOLDER (new_widget))
    glade_gtk_grid_refresh_placeholders (GTK_GRID (container));
}

static void
glade_gtk_grid_set_n_common (GObject * object, const GValue * value,
                              gboolean for_rows)
{
  GladeWidget *widget;
  GtkGrid *grid;
  guint new_size, n_columns, n_rows;

  grid = GTK_GRID (object);
  widget = glade_widget_get_from_gobject (GTK_WIDGET (grid));

  glade_widget_property_get (widget, "n-columns", &n_columns);
  glade_widget_property_get (widget, "n-rows", &n_rows);

  new_size = g_value_get_uint (value);

  if (new_size < 1)
    return;

  if (glade_gtk_grid_widget_exceeds_bounds
      (grid, for_rows ? new_size : n_rows, for_rows ? n_columns : new_size))
    /* Refuse to shrink if it means orphaning widgets */
    return;

  /* Fill grid with placeholders */
  glade_gtk_grid_refresh_placeholders (grid);
}

void
glade_gtk_grid_set_property (GladeWidgetAdaptor * adaptor,
                              GObject * object,
                              const gchar * id, const GValue * value)
{
  if (!strcmp (id, "n-rows"))
    glade_gtk_grid_set_n_common (object, value, TRUE);
  else if (!strcmp (id, "n-columns"))
    glade_gtk_grid_set_n_common (object, value, FALSE);
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object,
                                                      id, value);
}

static gboolean
glade_gtk_grid_verify_n_common (GObject * object, const GValue * value,
                                 gboolean for_rows)
{
  GtkGrid *grid = GTK_GRID (object);
  GladeWidget *widget;
  guint n_columns, n_rows, new_size = g_value_get_uint (value);

  widget = glade_widget_get_from_gobject (GTK_WIDGET (grid));
  glade_widget_property_get (widget, "n-columns", &n_columns);
  glade_widget_property_get (widget, "n-rows", &n_rows);

  if (glade_gtk_grid_widget_exceeds_bounds
      (grid, for_rows ? new_size : n_rows, for_rows ? n_columns : new_size))
    /* Refuse to shrink if it means orphaning widgets */
    return FALSE;

  return TRUE;
}

gboolean
glade_gtk_grid_verify_property (GladeWidgetAdaptor * adaptor,
                                 GObject * object,
                                 const gchar * id, const GValue * value)
{
  if (!strcmp (id, "n-rows"))
    return glade_gtk_grid_verify_n_common (object, value, TRUE);
  else if (!strcmp (id, "n-columns"))
    return glade_gtk_grid_verify_n_common (object, value, FALSE);
  else if (GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property)
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property (adaptor, object,
                                                         id, value);

  return TRUE;
}

void
glade_gtk_grid_set_child_property (GladeWidgetAdaptor * adaptor,
                                    GObject * container,
                                    GObject * child,
                                    const gchar * property_name, GValue * value)
{
  g_return_if_fail (GTK_IS_GRID (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (property_name != NULL && value != NULL);

  GWA_GET_CLASS
      (GTK_TYPE_CONTAINER)->child_set_property (adaptor,
                                                container, child,
                                                property_name, value);

  if (strcmp (property_name, "left-attach") == 0 ||
      strcmp (property_name, "top-attach")  == 0 ||
      strcmp (property_name, "width")       == 0 ||
      strcmp (property_name, "height")      == 0)
    {
      /* Refresh placeholders */
      glade_gtk_grid_refresh_placeholders (GTK_GRID (container));
    }
}

static gboolean
glade_gtk_grid_verify_attach_common (GObject     *object,
				     GValue      *value,
				     const gchar *prop,
				     const gchar *parent_prop)
{
  GladeWidget *widget, *parent;
  guint parent_val;
  gint val, prop_val;
  
  widget = glade_widget_get_from_gobject (object);
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), TRUE);
  parent = glade_widget_get_parent (widget);
  g_return_val_if_fail (GLADE_IS_WIDGET (parent), TRUE);

  val = g_value_get_int (value);
  glade_widget_property_get (widget, prop, &prop_val);
  glade_widget_property_get (parent, parent_prop, &parent_val);

  if (val < 0 || (val+prop_val) > parent_val)
    return FALSE;
  
  return TRUE;
}

gboolean
glade_gtk_grid_child_verify_property (GladeWidgetAdaptor *adaptor,
                                      GObject *container,
                                      GObject *child,
                                      const gchar *id,
                                      GValue *value)
{
  if (!strcmp (id, "left-attach"))
    return glade_gtk_grid_verify_attach_common (child, value, "width", "n-columns");
  else if (!strcmp (id, "width"))
    return glade_gtk_grid_verify_attach_common (child, value, "left-attach", "n-columns");
  else if (!strcmp (id, "top-attach"))
    return glade_gtk_grid_verify_attach_common (child, value, "height", "n-rows");
  else if (!strcmp (id, "height"))
    return glade_gtk_grid_verify_attach_common (child, value, "top-attach", "n-rows");
  else if (GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_verify_property)
    GWA_GET_CLASS
        (GTK_TYPE_CONTAINER)->child_verify_property (adaptor,
                                                     container, child,
                                                     id, value);

  return TRUE;
}

static void
glade_gtk_grid_child_insert_remove_action (GladeWidgetAdaptor *adaptor, 
					    GObject            *container, 
					    GObject            *object, 
					    const gchar        *group_format, 
					    const gchar        *n_row_col, 
					    const gchar        *attach1,    /* should be smaller (top/left) attachment */
                                            const gchar        *attach2,      /* should be larger (bot/right) attachment */
                                            gboolean            remove, 
					    gboolean            after)
{
  GladeWidget *parent;
  GList *children, *l;
  gint child_pos, size, offset;

  gtk_container_child_get (GTK_CONTAINER (container),
                           GTK_WIDGET (object),
                           attach1, &child_pos, NULL);

  parent = glade_widget_get_from_gobject (container);
  glade_command_push_group (group_format, glade_widget_get_name (parent));

  children = glade_widget_adaptor_get_children (adaptor, container);
  /* Make sure widgets does not get destroyed */
  g_list_foreach (children, (GFunc) g_object_ref, NULL);

  glade_widget_property_get (parent, n_row_col, &size);

  if (remove)
    {
      GList *del = NULL;
      /* Remove children first */
      for (l = children; l; l = g_list_next (l))
        {
          GladeWidget *gchild = glade_widget_get_from_gobject (l->data);
          gint pos1, pos2;

          /* Skip placeholders */
          if (gchild == NULL)
            continue;

          glade_widget_pack_property_get (gchild, attach1, &pos1);
          glade_widget_pack_property_get (gchild, attach2, &pos2);
          pos2 += pos1;
          if ((pos1 + 1 == pos2) && ((after ? pos2 : pos1) == child_pos))
            {
              del = g_list_prepend (del, gchild);
            }
        }
      if (del)
        {
          glade_command_delete (del);
          g_list_free (del);
        }
      offset = -1;
    }
  else
    {
      /* Expand the grid */
      glade_command_set_property (glade_widget_get_property (parent, n_row_col),
                                  size + 1);
      offset = 1;
    }

  /* Reorder children */
  for (l = children; l; l = g_list_next (l))
    {
      GladeWidget *gchild = glade_widget_get_from_gobject (l->data);
      gint pos;

      /* Skip placeholders */
      if (gchild == NULL)
        continue;

      /* if removing, do top/left before bot/right */
      if (remove)
        {
          /* adjust top-left attachment */
          glade_widget_pack_property_get (gchild, attach1, &pos);
          if (pos > child_pos || (after && pos == child_pos))
            {
              glade_command_set_property (glade_widget_get_pack_property
                                          (gchild, attach1), pos + offset);
            }
        }
      /* if inserting, do bot/right before top/left */
      else
        {
          /* adjust top-left attachment */
          glade_widget_pack_property_get (gchild, attach1, &pos);

          if ((after && pos > child_pos) || (!after && pos >= child_pos))
            {
              glade_command_set_property (glade_widget_get_pack_property
                                          (gchild, attach1), pos + offset);
            }
        }
    }

  if (remove)
    {
      /* Shrink the grid */
      glade_command_set_property (glade_widget_get_property (parent, n_row_col),
                                  size - 1);
    }

  g_list_foreach (children, (GFunc) g_object_unref, NULL);
  g_list_free (children);

  glade_command_pop_group ();
}

void
glade_gtk_grid_child_action_activate (GladeWidgetAdaptor * adaptor,
                                       GObject * container,
                                       GObject * object,
                                       const gchar * action_path)
{
  if (strcmp (action_path, "insert_row/after") == 0)
    {
      glade_gtk_grid_child_insert_remove_action (adaptor, container, object,
                                                  _("Insert Row on %s"),
                                                  "n-rows", "top-attach",
                                                  "height", FALSE, TRUE);
    }
  else if (strcmp (action_path, "insert_row/before") == 0)
    {
      glade_gtk_grid_child_insert_remove_action (adaptor, container, object,
                                                  _("Insert Row on %s"),
                                                  "n-rows", "top-attach",
                                                  "height",
                                                  FALSE, FALSE);
    }
  else if (strcmp (action_path, "insert_column/after") == 0)
    {
      glade_gtk_grid_child_insert_remove_action (adaptor, container, object,
                                                  _("Insert Column on %s"),
                                                  "n-columns", "left-attach",
                                                  "width", FALSE, TRUE);
    }
  else if (strcmp (action_path, "insert_column/before") == 0)
    {
      glade_gtk_grid_child_insert_remove_action (adaptor, container, object,
                                                  _("Insert Column on %s"),
                                                  "n-columns", "left-attach",
                                                  "width", FALSE, FALSE);
    }
  else if (strcmp (action_path, "remove_column") == 0)
    {
      glade_gtk_grid_child_insert_remove_action (adaptor, container, object,
                                                  _("Remove Column on %s"),
                                                  "n-columns", "left-attach",
                                                  "width", TRUE, FALSE);
    }
  else if (strcmp (action_path, "remove_row") == 0)
    {
      glade_gtk_grid_child_insert_remove_action (adaptor, container, object,
						 _("Remove Row on %s"),
						 "n-rows", "top-attach",
						 "height", TRUE, FALSE);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_action_activate (adaptor,
                                                               container,
                                                               object,
                                                               action_path);
}
