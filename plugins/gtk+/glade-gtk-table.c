/*
 * glade-gtk-table.c - GladeWidgetAdaptor for GtkTable widget
 *
 * Copyright (C) 2008 Tristan Van Berkom
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

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include "glade-fixed.h"

typedef struct
{
  /* comparable part: */
  GladeWidget *widget;
  gint left_attach;
  gint right_attach;
  gint top_attach;
  gint bottom_attach;
} GladeGtkTableChild;

typedef enum
{
  DIR_UP,
  DIR_DOWN,
  DIR_LEFT,
  DIR_RIGHT
} GladeTableDir;

typedef enum
{
  GROUP_ACTION_INSERT_ROW,
  GROUP_ACTION_INSERT_COLUMN,
  GROUP_ACTION_REMOVE_COLUMN,
  GROUP_ACTION_REMOVE_ROW
} GroupAction;

/* Redefine GTK_TABLE() macro, as GtkTable is deprecated */
#undef GTK_TABLE
#define GTK_TABLE(obj) ((GtkTable *)obj)

static void
glade_gtk_table_get_child_attachments (GtkWidget * table,
                                       GtkWidget * child,
                                       GtkTableChild * tchild)
{
  guint left, right, top, bottom;

  gtk_container_child_get (GTK_CONTAINER (table), child,
                           "left-attach", (guint *) & left,
                           "right-attach", (guint *) & right,
                           "bottom-attach", (guint *) & bottom,
                           "top-attach", (guint *) & top, NULL);

  tchild->widget = child;
  tchild->left_attach = left;
  tchild->right_attach = right;
  tchild->top_attach = top;
  tchild->bottom_attach = bottom;
}

static gboolean
glade_gtk_table_widget_exceeds_bounds (GtkTable * table, gint n_rows,
                                       gint n_cols)
{
  GList *list, *children;
  gboolean ret = FALSE;

  children = gtk_container_get_children (GTK_CONTAINER (table));

  for (list = children; list && list->data; list = list->next)
    {
      GtkTableChild child;

      glade_gtk_table_get_child_attachments (GTK_WIDGET (table),
                                             GTK_WIDGET (list->data), &child);

      if (GLADE_IS_PLACEHOLDER (child.widget) == FALSE &&
          (child.right_attach > n_cols || child.bottom_attach > n_rows))
        {
          ret = TRUE;
          break;
        }
    }

  g_list_free (children);

  return ret;
}

#define TABLE_OCCUPIED(occmap, n_columns, col, row) \
    (occmap)[row * n_columns + col]

static void
glade_gtk_table_build_occupation_maps(GtkTable *table, guint n_columns, guint n_rows,
				      gchar **child_map, gpointer **placeholder_map)
{
    guint i, j;
    GList *list, *children = gtk_container_get_children (GTK_CONTAINER (table));

    *child_map = g_malloc0(n_columns * n_rows * sizeof(gchar));  /* gchar is smaller than gboolean */
    *placeholder_map = g_malloc0(n_columns * n_rows * sizeof(gpointer));

    for (list = children; list && list->data; list = list->next)
    {
	GtkTableChild child;

	glade_gtk_table_get_child_attachments (GTK_WIDGET (table),
					       GTK_WIDGET (list->data), &child);

	if (GLADE_IS_PLACEHOLDER(list->data))
	{
	    /* assumption: placeholders are always attached to exactly 1 cell */
	    TABLE_OCCUPIED(*placeholder_map, n_columns, child.left_attach, child.top_attach) = list->data;
	}
	else
	{
	    for (i = child.left_attach; i < child.right_attach && i < n_columns; i++)
	    {
		for (j = child.top_attach; j < child.bottom_attach && j < n_rows; j++)
		{
		    TABLE_OCCUPIED(*child_map, n_columns, i, j) = 1;
		}
	    }
	}
    }
    g_list_free (children);
}

static void
glade_gtk_table_refresh_placeholders (GtkTable * table)
{
  guint n_columns, n_rows, i, j;
  gchar *child_map;
  gpointer *placeholder_map;

  g_object_get (table, "n-columns", &n_columns, "n-rows", &n_rows, NULL);
  glade_gtk_table_build_occupation_maps (table, n_columns, n_rows,
					 &child_map, &placeholder_map);

  for (i = 0; i < n_columns; i++)
    {
      for (j = 0; j < n_rows; j++)
	{
	  gpointer placeholder = TABLE_OCCUPIED(placeholder_map, n_columns, i, j);

	  if (TABLE_OCCUPIED(child_map, n_columns, i, j))
	    {
	      if (placeholder)
		{
		  gtk_container_remove (GTK_CONTAINER (table), 
					GTK_WIDGET (placeholder));
		}
	    }
	  else
	    {
	      if (!placeholder)
		{
		  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
		  gtk_table_attach_defaults (table, 
					     glade_placeholder_new (), 
					     i, i + 1, j, j + 1);
		  G_GNUC_END_IGNORE_DEPRECATIONS;
		}
	    }
	}
    }
  g_free(child_map);
  g_free(placeholder_map);

  if (gtk_widget_get_realized (GTK_WIDGET (table)))
    gtk_container_check_resize (GTK_CONTAINER (table));
}

static void
gtk_table_children_callback (GtkWidget * widget, gpointer client_data)
{
  GList **children;

  children = (GList **) client_data;
  *children = g_list_prepend (*children, widget);
}

GList *
glade_gtk_table_get_children (GladeWidgetAdaptor * adaptor,
                              GtkContainer * container)
{
  GList *children = NULL;

  gtk_container_forall (container, gtk_table_children_callback, &children);

  /* GtkTable has the children list already reversed */
  return children;
}

void
glade_gtk_table_add_child (GladeWidgetAdaptor * adaptor,
                           GObject * object, GObject * child)
{
  gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));

  glade_gtk_table_refresh_placeholders (GTK_TABLE (object));
}

void
glade_gtk_table_remove_child (GladeWidgetAdaptor * adaptor,
                              GObject * object, GObject * child)
{
  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));

  glade_gtk_table_refresh_placeholders (GTK_TABLE (object));
}

void
glade_gtk_table_replace_child (GladeWidgetAdaptor * adaptor,
                               GtkWidget * container,
                               GtkWidget * current, GtkWidget * new_widget)
{
  /* Chain Up */
  GWA_GET_CLASS
      (GTK_TYPE_CONTAINER)->replace_child (adaptor,
                                           G_OBJECT (container),
                                           G_OBJECT (current),
                                           G_OBJECT (new_widget));

  /* If we are replacing a GladeWidget, we must refresh placeholders
   * because the widget may have spanned multiple rows/columns, we must
   * not do so in the case we are pasting multiple widgets into a table,
   * where destroying placeholders results in default packing properties
   * (since the remaining placeholder templates no longer exist, only the
   * first pasted widget would have proper packing properties).
   */
  if (!GLADE_IS_PLACEHOLDER (new_widget))
    glade_gtk_table_refresh_placeholders (GTK_TABLE (container));

}

static void
glade_gtk_table_set_n_common (GObject * object, const GValue * value,
                              gboolean for_rows)
{
  GladeWidget *widget;
  GtkTable *table;
  guint new_size, old_size, n_columns, n_rows;

  table = GTK_TABLE (object);

  g_object_get (table, "n-columns", &n_columns, "n-rows", &n_rows, NULL);

  new_size = g_value_get_uint (value);
  old_size = for_rows ? n_rows : n_columns;

  if (new_size < 1)
    return;

  if (glade_gtk_table_widget_exceeds_bounds
      (table, for_rows ? new_size : n_rows, for_rows ? n_columns : new_size))
    /* Refuse to shrink if it means orphaning widgets */
    return;

  widget = glade_widget_get_from_gobject (GTK_WIDGET (table));
  g_return_if_fail (widget != NULL);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  if (for_rows)
    gtk_table_resize (table, new_size, n_columns);
  else
    gtk_table_resize (table, n_rows, new_size);
  G_GNUC_END_IGNORE_DEPRECATIONS;

  /* Fill table with placeholders */
  glade_gtk_table_refresh_placeholders (table);

  if (new_size < old_size)
    {
      /* Remove from the bottom up */
      GList *list, *children;
      GList *list_to_free = NULL;

      children = gtk_container_get_children (GTK_CONTAINER (table));

      for (list = children; list && list->data; list = list->next)
        {
          GtkTableChild child;
          guint start, end;

          glade_gtk_table_get_child_attachments (GTK_WIDGET (table),
                                                 GTK_WIDGET (list->data),
                                                 &child);

          start = for_rows ? child.top_attach : child.left_attach;
          end = for_rows ? child.bottom_attach : child.right_attach;

          /* We need to completely remove it */
          if (start >= new_size)
            {
              list_to_free = g_list_prepend (list_to_free, child.widget);
              continue;
            }

          /* If the widget spans beyond the new border,
           * we should resize it to fit on the new table */
          if (end > new_size)
            gtk_container_child_set
                (GTK_CONTAINER (table), GTK_WIDGET (child.widget),
                 for_rows ? "bottom_attach" : "right_attach", new_size, NULL);
        }

      g_list_free (children);

      if (list_to_free)
        {
          for (list = g_list_first (list_to_free);
               list && list->data; list = list->next)
            {
              g_object_ref (G_OBJECT (list->data));
              gtk_container_remove (GTK_CONTAINER (table),
                                    GTK_WIDGET (list->data));
              /* This placeholder is no longer valid, force destroy */
              gtk_widget_destroy (GTK_WIDGET (list->data));
            }
          g_list_free (list_to_free);
        }

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      gtk_table_resize (table,
                        for_rows ? new_size : n_rows,
                        for_rows ? n_columns : new_size);
      G_GNUC_END_IGNORE_DEPRECATIONS;
    }
}

void
glade_gtk_table_set_property (GladeWidgetAdaptor * adaptor,
                              GObject * object,
                              const gchar * id, const GValue * value)
{
  if (!strcmp (id, "n-rows"))
    glade_gtk_table_set_n_common (object, value, TRUE);
  else if (!strcmp (id, "n-columns"))
    glade_gtk_table_set_n_common (object, value, FALSE);
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object,
                                                      id, value);
}

static gboolean
glade_gtk_table_verify_n_common (GObject * object, const GValue * value,
                                 gboolean for_rows)
{
  GtkTable *table = GTK_TABLE (object);
  guint n_columns, n_rows, new_size = g_value_get_uint (value);

  g_object_get (table, "n-columns", &n_columns, "n-rows", &n_rows, NULL);

  if (glade_gtk_table_widget_exceeds_bounds
      (table, for_rows ? new_size : n_rows, for_rows ? n_columns : new_size))
    /* Refuse to shrink if it means orphaning widgets */
    return FALSE;

  return TRUE;
}

gboolean
glade_gtk_table_verify_property (GladeWidgetAdaptor * adaptor,
                                 GObject * object,
                                 const gchar * id, const GValue * value)
{
  if (!strcmp (id, "n-rows"))
    return glade_gtk_table_verify_n_common (object, value, TRUE);
  else if (!strcmp (id, "n-columns"))
    return glade_gtk_table_verify_n_common (object, value, FALSE);
  else if (GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property)
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property (adaptor, object,
                                                         id, value);

  return TRUE;
}

void
glade_gtk_table_set_child_property (GladeWidgetAdaptor * adaptor,
                                    GObject * container,
                                    GObject * child,
                                    const gchar * property_name, GValue * value)
{
  GWA_GET_CLASS
      (GTK_TYPE_CONTAINER)->child_set_property (adaptor,
                                                container, child,
                                                property_name, value);

  if (strcmp (property_name, "bottom-attach") == 0 ||
      strcmp (property_name, "left-attach") == 0 ||
      strcmp (property_name, "right-attach") == 0 ||
      strcmp (property_name, "top-attach") == 0)
    {
      /* Refresh placeholders */
      glade_gtk_table_refresh_placeholders (GTK_TABLE (container));
    }

}

static gboolean
glade_gtk_table_verify_attach_common (GObject * object,
                                      GValue * value,
                                      guint * val,
                                      const gchar * prop,
                                      guint * prop_val,
                                      const gchar * parent_prop,
                                      guint * parent_val)
{
  GladeWidget *widget, *parent;

  widget = glade_widget_get_from_gobject (object);
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), TRUE);
  parent = glade_widget_get_parent (widget);
  g_return_val_if_fail (GLADE_IS_WIDGET (parent), TRUE);

  *val = g_value_get_uint (value);
  glade_widget_property_get (widget, prop, prop_val);
  glade_widget_property_get (parent, parent_prop, parent_val);

  return FALSE;
}

static gboolean
glade_gtk_table_verify_left_top_attach (GObject * object,
                                        GValue * value,
                                        const gchar * prop,
                                        const gchar * parent_prop)
{
  guint val, prop_val, parent_val;

  if (glade_gtk_table_verify_attach_common (object, value, &val,
                                            prop, &prop_val,
                                            parent_prop, &parent_val))
    return FALSE;

  if (val >= parent_val || val >= prop_val)
    return FALSE;

  return TRUE;
}

static gboolean
glade_gtk_table_verify_right_bottom_attach (GObject * object,
                                            GValue * value,
                                            const gchar * prop,
                                            const gchar * parent_prop)
{
  guint val, prop_val, parent_val;

  if (glade_gtk_table_verify_attach_common (object, value, &val,
                                            prop, &prop_val,
                                            parent_prop, &parent_val))
    return FALSE;

  if (val <= prop_val || val > parent_val)
    return FALSE;

  return TRUE;
}

gboolean
glade_gtk_table_child_verify_property (GladeWidgetAdaptor * adaptor,
                                       GObject * container,
                                       GObject * child,
                                       const gchar * id, GValue * value)
{
  if (!strcmp (id, "left-attach"))
    return glade_gtk_table_verify_left_top_attach (child,
                                                   value,
                                                   "right-attach", "n-columns");
  else if (!strcmp (id, "right-attach"))
    return glade_gtk_table_verify_right_bottom_attach (child,
                                                       value,
                                                       "left-attach",
                                                       "n-columns");
  else if (!strcmp (id, "top-attach"))
    return glade_gtk_table_verify_left_top_attach (child,
                                                   value,
                                                   "bottom-attach", "n-rows");
  else if (!strcmp (id, "bottom-attach"))
    return glade_gtk_table_verify_right_bottom_attach (child,
                                                       value,
                                                       "top-attach", "n-rows");
  else if (GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_verify_property)
    GWA_GET_CLASS
        (GTK_TYPE_CONTAINER)->child_verify_property (adaptor,
                                                     container, child,
                                                     id, value);

  return TRUE;
}

static void
glade_gtk_table_child_insert_remove_action (GladeWidgetAdaptor *adaptor, 
					    GObject            *container, 
					    GObject            *object, 
					    GroupAction         group_action,
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
                           after ? attach2 : attach1, &child_pos, NULL);

  parent = glade_widget_get_from_gobject (container);
  switch (group_action)
    {
      case GROUP_ACTION_INSERT_ROW:
        glade_command_push_group (_("Insert Row on %s"), glade_widget_get_name (parent));
        break;
      case GROUP_ACTION_INSERT_COLUMN:
        glade_command_push_group (_("Insert Column on %s"), glade_widget_get_name (parent));
        break;
      case GROUP_ACTION_REMOVE_COLUMN:
        glade_command_push_group (_("Remove Column on %s"), glade_widget_get_name (parent));
        break;
      case GROUP_ACTION_REMOVE_ROW:
        glade_command_push_group (_("Remove Row on %s"), glade_widget_get_name (parent));
        break;
      default:
        g_assert_not_reached ();
    }

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
      /* Expand the table */
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

          /* adjust bottom-right attachment */
          glade_widget_pack_property_get (gchild, attach2, &pos);
          if (pos > child_pos || (after && pos == child_pos))
            {
              glade_command_set_property (glade_widget_get_pack_property
                                          (gchild, attach2), pos + offset);
            }

        }
      /* if inserting, do bot/right before top/left */
      else
        {
          /* adjust bottom-right attachment */
          glade_widget_pack_property_get (gchild, attach2, &pos);
          if (pos > child_pos)
            {
              glade_command_set_property (glade_widget_get_pack_property
                                          (gchild, attach2), pos + offset);
            }

          /* adjust top-left attachment */
          glade_widget_pack_property_get (gchild, attach1, &pos);
          if (pos >= child_pos)
            {
              glade_command_set_property (glade_widget_get_pack_property
                                          (gchild, attach1), pos + offset);
            }
        }
    }

  if (remove)
    {
      /* Shrink the table */
      glade_command_set_property (glade_widget_get_property (parent, n_row_col),
                                  size - 1);
    }

  g_list_foreach (children, (GFunc) g_object_unref, NULL);
  g_list_free (children);

  glade_command_pop_group ();
}

void
glade_gtk_table_child_action_activate (GladeWidgetAdaptor * adaptor,
                                       GObject * container,
                                       GObject * object,
                                       const gchar * action_path)
{
  if (strcmp (action_path, "insert_row/after") == 0)
    {
      glade_gtk_table_child_insert_remove_action (adaptor, container, object,
                                                  GROUP_ACTION_INSERT_ROW,
                                                  "n-rows", "top-attach",
                                                  "bottom-attach", FALSE, TRUE);
    }
  else if (strcmp (action_path, "insert_row/before") == 0)
    {
      glade_gtk_table_child_insert_remove_action (adaptor, container, object,
                                                  GROUP_ACTION_INSERT_ROW,
                                                  "n-rows", "top-attach",
                                                  "bottom-attach",
                                                  FALSE, FALSE);
    }
  else if (strcmp (action_path, "insert_column/after") == 0)
    {
      glade_gtk_table_child_insert_remove_action (adaptor, container, object,
                                                  GROUP_ACTION_INSERT_COLUMN,
                                                  "n-columns", "left-attach",
                                                  "right-attach", FALSE, TRUE);
    }
  else if (strcmp (action_path, "insert_column/before") == 0)
    {
      glade_gtk_table_child_insert_remove_action (adaptor, container, object,
                                                  GROUP_ACTION_INSERT_COLUMN,
                                                  "n-columns", "left-attach",
                                                  "right-attach", FALSE, FALSE);
    }
  else if (strcmp (action_path, "remove_column") == 0)
    {
      glade_gtk_table_child_insert_remove_action (adaptor, container, object,
                                                  GROUP_ACTION_REMOVE_COLUMN,
                                                  "n-columns", "left-attach",
                                                  "right-attach", TRUE, FALSE);
    }
  else if (strcmp (action_path, "remove_row") == 0)
    {
      glade_gtk_table_child_insert_remove_action (adaptor, container, object,
                                                  GROUP_ACTION_REMOVE_ROW,
                                                  "n-rows", "top-attach",
                                                  "bottom-attach", TRUE, FALSE);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_action_activate (adaptor,
                                                               container,
                                                               object,
                                                               action_path);
}
