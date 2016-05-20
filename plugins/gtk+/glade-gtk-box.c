/*
 * glade-gtk-box.c - GladeWidgetAdaptor for GtkGrid widget
 *
 * Copyright (C) 2013 Openismus GmbH
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
#include "glade-gtk-notebook.h"
#include "glade-box-editor.h"
#include "glade-gtk.h"

static gboolean glade_gtk_box_configure_child (GladeFixed * fixed,
					       GladeWidget * child,
					       GdkRectangle * rect,
					       GtkWidget * box);

static gboolean glade_gtk_box_configure_begin (GladeFixed * fixed,
					       GladeWidget * child,
					       GtkWidget * box);


static gboolean glade_gtk_box_configure_end (GladeFixed * fixed,
					     GladeWidget * child,
					     GtkWidget * box);


GladeEditable *
glade_gtk_box_create_editable (GladeWidgetAdaptor * adaptor,
			       GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_box_editor_new ();

  return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}

static void
glade_gtk_box_parse_finished (GladeProject * project, GladeWidget *gbox)
{
  GObject *box = glade_widget_get_object (gbox);

  glade_widget_property_set (gbox, "use-center-child",
                             gtk_box_get_center_widget (GTK_BOX (box)) != NULL);
}

void
glade_gtk_box_post_create (GladeWidgetAdaptor * adaptor,
                           GObject * container, GladeCreateReason reason)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (container);
  GladeProject *project = glade_widget_get_project (gwidget);

  /* Implement drag in GtkBox but not resize.
   */
  g_object_set (gwidget, "can-resize", FALSE, NULL);

  g_signal_connect (G_OBJECT (gwidget), "configure-child",
                    G_CALLBACK (glade_gtk_box_configure_child), container);

  g_signal_connect (G_OBJECT (gwidget), "configure-begin",
                    G_CALLBACK (glade_gtk_box_configure_begin), container);

  g_signal_connect (G_OBJECT (gwidget), "configure-end",
                    G_CALLBACK (glade_gtk_box_configure_end), container);

  if (reason == GLADE_CREATE_LOAD)
    {
      g_signal_connect_object (project, "parse-finished",
                               G_CALLBACK (glade_gtk_box_parse_finished),
                               gwidget, 0);
    }
}

static gint
sort_box_children (GtkWidget * widget_a, GtkWidget * widget_b, GtkWidget *box)
{
  GladeWidget *gwidget_a, *gwidget_b;
  gint position_a, position_b;

  gwidget_a = glade_widget_get_from_gobject (widget_a);
  gwidget_b = glade_widget_get_from_gobject (widget_b);

  /* Indirect children might be internal children, sort internal children before any other children */
  if (box != gtk_widget_get_parent (widget_a))
    return -1;
  if (box != gtk_widget_get_parent (widget_b))
    return 1;

  if (gtk_box_get_center_widget (GTK_BOX (box)) == widget_a)
    return -1;
  if (gtk_box_get_center_widget (GTK_BOX (box)) == widget_b)
    return -1;

  /* XXX Sometimes the packing "position" property doesnt exist here, why ?
   */
  if (gwidget_a)
    glade_widget_pack_property_get (gwidget_a, "position", &position_a);
  else
    gtk_container_child_get (GTK_CONTAINER (box),
                             widget_a, "position", &position_a, NULL);

  if (gwidget_b)
    glade_widget_pack_property_get (gwidget_b, "position", &position_b);
  else
    gtk_container_child_get (GTK_CONTAINER (box),
                             widget_b, "position", &position_b, NULL);
  return position_a - position_b;
}

GList *
glade_gtk_box_get_children (GladeWidgetAdaptor * adaptor,
                            GObject * container)
{
  GList *children;

  children = GWA_GET_CLASS (GTK_TYPE_CONTAINER)->get_children (adaptor, container);

  return g_list_sort_with_data (children, (GCompareDataFunc) sort_box_children, container);
}

void
glade_gtk_box_set_child_property (GladeWidgetAdaptor * adaptor,
                                  GObject * container,
                                  GObject * child,
                                  const gchar * property_name, GValue * value)
{
  GladeWidget *gbox, *gchild, *gchild_iter;
  GList *children, *list;
  gboolean is_position;
  gint old_position, iter_position, new_position;
  static gboolean recursion = FALSE;

  g_return_if_fail (GTK_IS_BOX (container));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (property_name != NULL || value != NULL);

  gbox = glade_widget_get_from_gobject (container);
  gchild = glade_widget_get_from_gobject (child);

  g_return_if_fail (GLADE_IS_WIDGET (gbox));

  if (gtk_widget_get_parent (GTK_WIDGET (child)) != GTK_WIDGET (container))
    return;

  /* Get old position */
  if ((is_position = (strcmp (property_name, "position") == 0)) != FALSE)
    {
      gtk_container_child_get (GTK_CONTAINER (container),
                               GTK_WIDGET (child),
                               property_name, &old_position, NULL);


      /* Get the real value */
      new_position = g_value_get_int (value);
    }

  if (is_position && recursion == FALSE)
    {
      children = glade_widget_get_children (gbox);
      children = g_list_sort (children, (GCompareFunc) sort_box_children);

      for (list = children; list; list = list->next)
        {
          gchild_iter = glade_widget_get_from_gobject (list->data);

          if (gchild_iter == gchild)
            {
              gtk_box_reorder_child (GTK_BOX (container),
                                     GTK_WIDGET (child), new_position);
              continue;
            }

          /* Get the old value from glade */
          glade_widget_pack_property_get
              (gchild_iter, "position", &iter_position);

          /* Search for the child at the old position and update it */
          if (iter_position == new_position &&
              glade_property_superuser () == FALSE)
            {
              /* Update glade with the real value */
              recursion = TRUE;
              glade_widget_pack_property_set
                  (gchild_iter, "position", old_position);
              recursion = FALSE;
              continue;
            }
          else
            {
              gtk_box_reorder_child (GTK_BOX (container),
                                     GTK_WIDGET (list->data), iter_position);
            }
        }

      for (list = children; list; list = list->next)
        {
          gchild_iter = glade_widget_get_from_gobject (list->data);

          /* Refresh values yet again */
          glade_widget_pack_property_get
              (gchild_iter, "position", &iter_position);

          gtk_box_reorder_child (GTK_BOX (container),
                                 GTK_WIDGET (list->data), iter_position);

        }

      if (children)
        g_list_free (children);
    }

  /* Chain Up */
  if (!is_position)
    GWA_GET_CLASS
        (GTK_TYPE_CONTAINER)->child_set_property (adaptor,
                                                  container,
                                                  child, property_name, value);

  gtk_container_check_resize (GTK_CONTAINER (container));

}

static gint
glade_gtk_box_get_num_children (GObject *box)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (box));
  gint retval = g_list_length (children);
  if (gtk_box_get_center_widget (GTK_BOX (box)) != NULL)
    retval -= 1;
  g_list_free (children);
  return retval;
}

void
glade_gtk_box_get_property (GladeWidgetAdaptor * adaptor,
                            GObject * object, const gchar * id, GValue * value)
{
  if (!strcmp (id, "use-center-child"))
    {
      g_value_reset (value);
      g_value_set_boolean (value, gtk_box_get_center_widget (GTK_BOX (object)) != NULL);
    }
  else if (!strcmp (id, "size"))
    {
      g_value_reset (value);
      g_value_set_int (value, glade_gtk_box_get_num_children (object));
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->get_property (adaptor, object, id,
                                                      value);
}

static gint
glade_gtk_box_get_first_blank (GtkBox * box)
{
  GList *child, *children;
  GladeWidget *gwidget;
  gint position;

  children = gtk_container_get_children (GTK_CONTAINER (box));

  for (child = children, position = 0;
       child && child->data; child = child->next, position++)
    {
      GtkWidget *widget = child->data;

      if (widget == gtk_box_get_center_widget (GTK_BOX (box)))
        continue;

      if ((gwidget = glade_widget_get_from_gobject (widget)) != NULL)
        {
          gint gwidget_position = 0;
          GladeProperty *property =
              glade_widget_get_pack_property (gwidget, "position");

	  /* property can be NULL here when project is closing */
	  if (property)
	    gwidget_position = g_value_get_int (glade_property_inline_value (property));

          if (gwidget_position > position)
            break;
        }
    }

  g_list_free (children);

  return position;
}

static void
glade_gtk_box_set_size (GObject * object, const GValue * value)
{
  GtkBox *box;
  GList *child, *children;
  guint new_size, old_size, i;

  box = GTK_BOX (object);
  g_return_if_fail (GTK_IS_BOX (box));

  if (glade_util_object_is_loading (object))
    return;

  children = gtk_container_get_children (GTK_CONTAINER (box));
  children = g_list_remove (children, gtk_box_get_center_widget (GTK_BOX (box)));

  old_size = g_list_length (children);
  new_size = g_value_get_int (value);

  if (old_size == new_size)
    {
      g_list_free (children);
      return;
    }

  /* Ensure placeholders first...
   */
  for (i = 0; i < new_size; i++)
    {
      if (g_list_length (children) < (i + 1))
        {
          GtkWidget *placeholder = glade_placeholder_new ();
          gint blank = glade_gtk_box_get_first_blank (box);

          gtk_container_add (GTK_CONTAINER (box), placeholder);
          gtk_box_reorder_child (box, placeholder, blank);
        }
    }

  /* The box has shrunk. Remove the widgets that are on those slots */
  for (child = g_list_last (children);
       child && old_size > new_size; child = g_list_previous (child))
    {
      GtkWidget *child_widget = child->data;

      /* Refuse to remove any widgets that are either GladeWidget objects
       * or internal to the hierarchic entity (may be a composite widget,
       * not all internal widgets have GladeWidgets).
       */
      if (glade_widget_get_from_gobject (child_widget) ||
          GLADE_IS_PLACEHOLDER (child_widget) == FALSE)
        continue;

      gtk_container_remove (GTK_CONTAINER (box), child_widget);
      old_size--;
    }
  g_list_free (children);

}

void
glade_gtk_box_set_property (GladeWidgetAdaptor * adaptor,
                            GObject * object,
                            const gchar * id, const GValue * value)
{
  if (!strcmp (id, "use-center-child"))
    {
      GtkWidget *child;

      if (g_value_get_boolean (value))
        {
          child = gtk_box_get_center_widget (GTK_BOX (object));
          if (!child)
            child = glade_placeholder_new ();
          g_object_set_data (G_OBJECT (child), "special-child-type", "center");
        }
      else
        child = NULL;
      gtk_box_set_center_widget (GTK_BOX (object), child);
    }

  else if (!strcmp (id, "size"))
    glade_gtk_box_set_size (object, value);
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id,
                                                      value);
}

static gboolean
glade_gtk_box_verify_size (GObject *object, const GValue *value)
{
  GList *child, *children;
  gint old_size, count = 0;
  gint new_size = g_value_get_int (value);
  
  children = gtk_container_get_children (GTK_CONTAINER (object));
  children = g_list_remove (children, gtk_box_get_center_widget (GTK_BOX (object)));
  old_size = g_list_length (children);

  for (child = g_list_last (children);
       child && old_size > new_size;
       child = g_list_previous (child))
    {
      GtkWidget *widget = child->data;
      if (glade_widget_get_from_gobject (widget) != NULL)
        count++;
      else
        old_size--;
    }

  g_list_free (children);

  return count > new_size ? FALSE : new_size >= 0;
}


gboolean
glade_gtk_box_verify_property (GladeWidgetAdaptor * adaptor,
                               GObject * object,
                               const gchar * id, const GValue * value)
{
  if (!strcmp (id, "size"))
    return glade_gtk_box_verify_size (object, value);
  else if (GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property)
    return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property (adaptor, object,
                                                                id, value);

  return TRUE;
}


static void
fix_response_id_on_child (GladeWidget * gbox, GObject * child, gboolean add)
{
  GladeWidget *gchild;
  const gchar *internal_name;

  gchild = glade_widget_get_from_gobject (child);

  /* Fix response id property on child buttons */
  if (gchild && GTK_IS_BUTTON (child))
    {
      if (add && (internal_name = glade_widget_get_internal (gbox)) &&
          !strcmp (internal_name, "action_area"))
	glade_widget_property_set_sensitive (gchild, "response-id", TRUE,
					     NULL);
      else
	glade_widget_property_set_sensitive (gchild, "response-id", FALSE,
					     RESPID_INSENSITIVE_MSG);
    }
}

void
glade_gtk_box_add_child (GladeWidgetAdaptor * adaptor,
                         GObject * object, GObject * child)
{
  GladeWidget *gbox, *gchild;
  gint num_children;
  gchar *special_child_type;

  g_return_if_fail (GTK_IS_BOX (object));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gbox = glade_widget_get_from_gobject (object);

  special_child_type = g_object_get_data (child, "special-child-type");
  if (special_child_type && !strcmp (special_child_type, "center"))
    {
      gtk_box_set_center_widget (GTK_BOX (object), GTK_WIDGET (child));
       return;
    }

  /*
     Try to remove the last placeholder if any, this way GtkBox`s size 
     will not be changed.
   */
  if (glade_widget_superuser () == FALSE && !GLADE_IS_PLACEHOLDER (child))
    {
      GList *l, *children;
      GtkBox *box = GTK_BOX (object);

      children = gtk_container_get_children (GTK_CONTAINER (box));

      for (l = g_list_last (children); l; l = g_list_previous (l))
        {
          GtkWidget *child_widget = l->data;
          if (GLADE_IS_PLACEHOLDER (child_widget))
            {
              gtk_container_remove (GTK_CONTAINER (box), child_widget);
              break;
            }
        }
      g_list_free (children);
    }

  gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));
  num_children = glade_gtk_box_get_num_children (object);
  glade_widget_property_set (gbox, "size", num_children);

  gchild = glade_widget_get_from_gobject (child);

  /* The "Remove Slot" operation only makes sence on placeholders,
   * otherwise its a "Delete" operation on the child widget.
   */
  if (gchild)
    glade_widget_set_pack_action_visible (gchild, "remove_slot", FALSE);

  fix_response_id_on_child (gbox, child, TRUE);
  
  if (glade_widget_superuser ())
    return;
  
  /* Packing props arent around when parenting during a glade_widget_dup() */
  if (gchild && glade_widget_get_packing_properties (gchild))
    glade_widget_pack_property_set (gchild, "position", num_children - 1);
}

void
glade_gtk_box_remove_child (GladeWidgetAdaptor * adaptor,
                            GObject * object, GObject * child)
{
  GladeWidget *gbox;
  gint size;
  gchar *special_child_type;

  g_return_if_fail (GTK_IS_BOX (object));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gbox = glade_widget_get_from_gobject (object);

  special_child_type = g_object_get_data (child, "special-child-type");
  if (special_child_type && !strcmp (special_child_type, "center"))
    {
      GtkWidget *w;

      w = glade_placeholder_new ();
      g_object_set_data (G_OBJECT (w), "special-child-type", "center");
      gtk_box_set_center_widget (GTK_BOX (object), w);
      return;
    }

  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));

  if (glade_widget_superuser () == FALSE)
    {
      glade_widget_property_get (gbox, "size", &size);
      glade_widget_property_set (gbox, "size", size);
    }

  fix_response_id_on_child (gbox, child, FALSE);
}


void
glade_gtk_box_replace_child (GladeWidgetAdaptor * adaptor,
                             GObject * container,
                             GObject * current, GObject * new_widget)
{
  GladeWidget *gchild;
  GladeWidget *gbox;

  gchar *special_child_type;

  special_child_type =
      g_object_get_data (G_OBJECT (current), "special-child-type");

  if (special_child_type && !strcmp (special_child_type, "center"))
    {
      g_object_set_data (G_OBJECT (new_widget), "special-child-type", "center");
      gtk_box_set_center_widget (GTK_BOX (container), GTK_WIDGET (new_widget));
      return;
    }

  g_object_ref (G_OBJECT (current));

  GWA_GET_CLASS (GTK_TYPE_CONTAINER)->replace_child (adaptor,
                                                     container,
                                                     current, new_widget);
  gbox = glade_widget_get_from_gobject (container);

  if ((gchild = glade_widget_get_from_gobject (new_widget)) != NULL)
    /* The "Remove Slot" operation only makes sence on placeholders,
     * otherwise its a "Delete" operation on the child widget.
     */
    glade_widget_set_pack_action_visible (gchild, "remove_slot", FALSE);

  fix_response_id_on_child (gbox, current, FALSE);
  fix_response_id_on_child (gbox, new_widget, TRUE);

  g_object_unref (G_OBJECT (current));
}

void
glade_gtk_box_child_action_activate (GladeWidgetAdaptor * adaptor,
                                     GObject * container,
                                     GObject * object,
                                     const gchar * action_path)
{
  if (strcmp (action_path, "insert_after") == 0)
    {
      glade_gtk_box_notebook_child_insert_remove_action (adaptor, container,
                                                         object, "size",
                                                         _
                                                         ("Insert placeholder to %s"),
                                                         FALSE, TRUE);
    }
  else if (strcmp (action_path, "insert_before") == 0)
    {
      glade_gtk_box_notebook_child_insert_remove_action (adaptor, container,
                                                         object, "size",
                                                         _
                                                         ("Insert placeholder to %s"),
                                                         FALSE, FALSE);
    }
  else if (strcmp (action_path, "remove_slot") == 0)
    {
      glade_gtk_box_notebook_child_insert_remove_action (adaptor, container,
                                                         object, "size",
                                                         _
                                                         ("Remove placeholder from %s"),
                                                         TRUE, FALSE);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_action_activate (adaptor,
                                                               container,
                                                               object,
                                                               action_path);
}


/**********************************************************
 *             GladeFixed drag implementation             *
 **********************************************************/
typedef struct
{
  GtkWidget *widget;
  gint position;
} GladeGtkBoxChild;

static GList *glade_gtk_box_original_positions = NULL;

static gboolean
glade_gtk_box_configure_child (GladeFixed * fixed,
                               GladeWidget * child,
                               GdkRectangle * rect, GtkWidget * box)
{
  GList *list, *children;
  GtkWidget *bchild;
  GtkAllocation allocation, bchild_allocation;
  gint point, trans_point, span,
      iter_span, position, old_position, offset, orig_offset;
  gboolean found = FALSE;

  gtk_widget_get_allocation (GTK_WIDGET (glade_widget_get_object (child)), &allocation);

  if (gtk_orientable_get_orientation (GTK_ORIENTABLE (box)) == GTK_ORIENTATION_HORIZONTAL)
    {
      point = fixed->mouse_x;
      span = allocation.width;
      offset = rect->x;
      orig_offset = fixed->child_x_origin;
    }
  else
    {
      point = fixed->mouse_y;
      span = allocation.height;
      offset = rect->y;
      orig_offset = fixed->child_y_origin;
    }

  glade_widget_pack_property_get (child, "position", &old_position);

  children = gtk_container_get_children (GTK_CONTAINER (box));

  for (list = children; list; list = list->next)
    {
      bchild = list->data;

      if (bchild == GTK_WIDGET (glade_widget_get_object (child)))
        continue;

      /* Find the widget in the box where the center of
       * this rectangle fits... and set the position to that
       * position.
       */

      gtk_widget_get_allocation (GTK_WIDGET (bchild), &bchild_allocation);
      if (gtk_orientable_get_orientation (GTK_ORIENTABLE (box)) == GTK_ORIENTATION_HORIZONTAL)
        {
          gtk_widget_translate_coordinates
              (GTK_WIDGET (box), bchild, point, 0, &trans_point, NULL);

          iter_span = bchild_allocation.width;
        }
      else
        {
          gtk_widget_translate_coordinates
              (GTK_WIDGET (box), bchild, 0, point, NULL, &trans_point);
          iter_span = bchild_allocation.height;
        }

#if 0
      gtk_container_child_get (GTK_CONTAINER (box),
                               bchild, "position", &position, NULL);
      g_print ("widget: %p pos %d, point %d, trans_point %d, iter_span %d\n",
               bchild, position, point, trans_point, iter_span);
#endif

      if (iter_span <= span)
        {
          found = trans_point >= 0 && trans_point < iter_span;
        }
      else
        {
          if (offset > orig_offset)
            found = trans_point >= iter_span - span && trans_point < iter_span;
          else if (offset < orig_offset)
            found = trans_point >= 0 && trans_point < span;
        }

      if (found)
        {
          gtk_container_child_get (GTK_CONTAINER (box),
                                   bchild, "position", &position, NULL);

#if 0
          g_print ("setting position of %s from %d to %d, "
                   "(point %d iter_span %d)\n",
                   glade_widget_get_name (child), old_position, position, trans_point, iter_span);
#endif

          glade_widget_pack_property_set (child, "position", position);

          break;
        }

    }

  g_list_free (children);

  return TRUE;
}

static gboolean
glade_gtk_box_configure_begin (GladeFixed * fixed,
                               GladeWidget * child, GtkWidget * box)
{
  GList *list, *children;
  GtkWidget *bchild;

  g_assert (glade_gtk_box_original_positions == NULL);

  children = gtk_container_get_children (GTK_CONTAINER (box));

  for (list = children; list; list = list->next)
    {
      GladeGtkBoxChild *gbchild;
      GladeWidget *gchild;

      bchild = list->data;
      if ((gchild = glade_widget_get_from_gobject (bchild)) == NULL)
        continue;

      gbchild = g_new0 (GladeGtkBoxChild, 1);
      gbchild->widget = bchild;
      glade_widget_pack_property_get (gchild, "position", &gbchild->position);

      glade_gtk_box_original_positions =
          g_list_prepend (glade_gtk_box_original_positions, gbchild);
    }

  g_list_free (children);

  return TRUE;
}

static gboolean
glade_gtk_box_configure_end (GladeFixed * fixed,
                             GladeWidget * child, GtkWidget * box)
{
  GList *list, *l, *children;
  GList *prop_list = NULL;

  children = gtk_container_get_children (GTK_CONTAINER (box));

  for (list = children; list; list = list->next)
    {
      GtkWidget *bchild = list->data;

      for (l = glade_gtk_box_original_positions; l; l = l->next)
        {
          GladeGtkBoxChild *gbchild = l->data;
          GladeWidget *gchild = glade_widget_get_from_gobject (gbchild->widget);


          if (bchild == gbchild->widget)
            {
              GCSetPropData *prop_data = g_new0 (GCSetPropData, 1);
              prop_data->property =
                  glade_widget_get_pack_property (gchild, "position");

              prop_data->old_value = g_new0 (GValue, 1);
              prop_data->new_value = g_new0 (GValue, 1);

              glade_property_get_value (prop_data->property,
                                        prop_data->new_value);

              g_value_init (prop_data->old_value, G_TYPE_INT);
              g_value_set_int (prop_data->old_value, gbchild->position);

              prop_list = g_list_prepend (prop_list, prop_data);
              break;
            }
        }
    }

  g_list_free (children);

  glade_command_push_group (_("Ordering children of %s"),
                            glade_widget_get_name (GLADE_WIDGET (fixed)));
  glade_property_push_superuser ();
  if (prop_list)
    glade_command_set_properties_list (glade_widget_get_project (GLADE_WIDGET (fixed)),
                                       prop_list);
  glade_property_pop_superuser ();
  glade_command_pop_group ();

  for (l = glade_gtk_box_original_positions; l; l = l->next)
    g_free (l->data);

  glade_gtk_box_original_positions =
      (g_list_free (glade_gtk_box_original_positions), NULL);


  return TRUE;
}
