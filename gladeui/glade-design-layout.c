/*
 * glade-design-layout.c
 *
 * Copyright (C) 2006-2007 Vincent Geddes
 *                    2011 Juan Pablo Ugarte
 *
 * Authors:
 *   Vincent Geddes <vgeddes@gnome.org>
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
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
 */

#include "config.h"

#include "glade.h"
#include "glade-design-layout.h"
#include "glade-design-private.h"
#include "glade-accumulators.h"
#include "glade-marshallers.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#define GLADE_DESIGN_LAYOUT_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object),  \
						 GLADE_TYPE_DESIGN_LAYOUT,               \
						 GladeDesignLayoutPrivate))

#define OUTLINE_WIDTH     4
#define PADDING           10

#define MARGIN_STEP       8

typedef enum
{
  ACTIVITY_NONE,
  ACTIVITY_RESIZE_WIDTH,
  ACTIVITY_RESIZE_HEIGHT,
  ACTIVITY_RESIZE_WIDTH_AND_HEIGHT,
  ACTIVITY_MARGINS,
  ACTIVITY_MARGINS_VERTICAL, /* These activities are only used to set the cursor */
  ACTIVITY_MARGINS_HORIZONTAL,
  ACTIVITY_MARGINS_TOP_LEFT,
  ACTIVITY_MARGINS_TOP_RIGHT,
  ACTIVITY_MARGINS_BOTTOM_LEFT,
  ACTIVITY_MARGINS_BOTTOM_RIGHT,
  N_ACTIVITY
} Activity;


typedef enum
{
  MARGIN_TOP    = 1 << 0,
  MARGIN_BOTTOM = 1 << 1,
  MARGIN_LEFT   = 1 << 2,
  MARGIN_RIGHT  = 1 << 3
} Margins;

struct _GladeDesignLayoutPrivate
{
  GdkWindow *window, *offscreen_window;

  gint child_offset;
  GdkRectangle east, south, south_east;
  GdkCursor *cursor;            /* Current cursor */
  GdkCursor *cursors[N_ACTIVITY];

  gint current_width, current_height;
  PangoLayout *widget_name;
  gint layout_width;

  /* Colors */
  GdkRGBA frame_color[2];
  GdkRGBA frame_color_active[2];

  /* Margin edit mode */
  GtkWidget *selection;
  gint top, bottom, left, right;
  gint m_dy, m_dx;
  gint max_width, max_height;
  Margins margin;

  /* state machine */
  Activity activity;            /* the current activity */
  gint dx;                      /* child.width - event.pointer.x   */
  gint dy;                      /* child.height - event.pointer.y  */
  gint new_width;               /* user's new requested width */
  gint new_height;              /* user's new requested height */

  /* Properties */
  GladeDesignView *view;
  GladeProject *project;
};

enum
{
  PROP_0,
  PROP_DESIGN_VIEW
};

G_DEFINE_TYPE (GladeDesignLayout, glade_design_layout, GTK_TYPE_BIN)

#define RECTANGLE_POINT_IN(rect,x,y) (x >= rect.x && x <= (rect.x + rect.width) && y >= rect.y && y <= (rect.y + rect.height))

static Margins
gdl_get_margins_from_pointer (GtkWidget *widget, gint x, gint y)
{
  gint xx, yy, top, bottom, left, right;
  GtkAllocation alloc;
  Margins margin = 0;
  GdkRectangle rec;

  gtk_widget_get_allocation (widget, &alloc);
  xx = alloc.x;
  yy = alloc.y;

  top = gtk_widget_get_margin_top (widget);
  bottom = gtk_widget_get_margin_bottom (widget);
  left = gtk_widget_get_margin_left (widget);
  right = gtk_widget_get_margin_right (widget);

  rec.x = xx - left - OUTLINE_WIDTH;
  rec.y = yy - top - OUTLINE_WIDTH;
  rec.width = alloc.width + left + right + (OUTLINE_WIDTH * 2);
  rec.height = alloc.height + top + bottom + (OUTLINE_WIDTH * 2);

  if (RECTANGLE_POINT_IN (rec, x, y))
    {      
      if (y <= yy + OUTLINE_WIDTH) margin |= MARGIN_TOP;
      else if (y >= yy + alloc.height - OUTLINE_WIDTH) margin |= MARGIN_BOTTOM;
      
      if (x <= xx + OUTLINE_WIDTH) margin |= MARGIN_LEFT;
      else if (x >= xx + alloc.width - OUTLINE_WIDTH) margin |= MARGIN_RIGHT;
    }

  return margin;
}

static Activity
gdl_get_activity_from_pointer (GladeDesignLayout *layout, gint x, gint y)
{
  GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);
  
  if (priv->selection)
    {
      priv->margin = gdl_get_margins_from_pointer (priv->selection,
                                                   x - priv->child_offset,
                                                   y - priv->child_offset);
      
      if (priv->margin) return ACTIVITY_MARGINS;
    }
  
  if (RECTANGLE_POINT_IN (priv->south_east, x, y)) return ACTIVITY_RESIZE_WIDTH_AND_HEIGHT;

  if (RECTANGLE_POINT_IN (priv->east, x, y)) return ACTIVITY_RESIZE_WIDTH;

  if (RECTANGLE_POINT_IN (priv->south, x, y)) return ACTIVITY_RESIZE_HEIGHT;

  return ACTIVITY_NONE;
}

static void
gdl_set_cursor (GladeDesignLayoutPrivate *priv, GdkCursor *cursor)
{
  if (cursor != priv->cursor)
    {
      priv->cursor = cursor;
      gdk_window_set_cursor (priv->window, cursor);
    }
}

static Activity
gdl_margin_get_activity (Margins margin)
{
  if (margin & MARGIN_TOP)
    {
      if (margin & MARGIN_LEFT)
        return ACTIVITY_MARGINS_TOP_LEFT;
      else if (margin & MARGIN_RIGHT)
        return ACTIVITY_MARGINS_TOP_RIGHT;
      else
        return ACTIVITY_MARGINS_VERTICAL;
    }
  else if (margin & MARGIN_BOTTOM)
    {
      if (margin & MARGIN_LEFT)
        return ACTIVITY_MARGINS_BOTTOM_LEFT;
      else if (margin & MARGIN_RIGHT)
        return ACTIVITY_MARGINS_BOTTOM_RIGHT;
      else
        return ACTIVITY_MARGINS_VERTICAL;
    }
  else if (margin & MARGIN_LEFT || margin & MARGIN_RIGHT)
    return ACTIVITY_MARGINS_HORIZONTAL;

  return ACTIVITY_NONE;
}

static gboolean
glade_design_layout_leave_notify_event (GtkWidget *widget, GdkEventCrossing *ev)
{
  GtkWidget *child;
  GladeDesignLayoutPrivate *priv;

  if ((child = gtk_bin_get_child (GTK_BIN (widget))) == NULL ||
      ev->window != gtk_widget_get_window (widget))
    return FALSE;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

  if (priv->activity == ACTIVITY_NONE)
    gdl_set_cursor (priv, NULL);

  return FALSE;
}

static void
gdl_update_max_margins (GladeDesignLayoutPrivate *priv,
                        GtkWidget *child,
                        gint width, gint height)
{
  gint top, bottom, left, right;
  GtkRequisition req;
  
  gtk_widget_get_preferred_size (child, &req, NULL);

  top = gtk_widget_get_margin_top (priv->selection);
  bottom = gtk_widget_get_margin_bottom (priv->selection);
  left = gtk_widget_get_margin_left (priv->selection);
  right = gtk_widget_get_margin_right (priv->selection);

  priv->max_width = width - (req.width - left - right);
  priv->max_height = height - (req.height - top - bottom);
}

static void
glade_design_layout_update_child (GladeDesignLayout *layout,
                                  GtkWidget         *child,
                                  GtkAllocation     *allocation)
{
  GladeWidget *gchild;

  /* Update GladeWidget metadata */
  gchild = glade_widget_get_from_gobject (child);
  g_object_set (gchild,
                "toplevel-width", allocation->width,
                "toplevel-height", allocation->height, NULL);

  if (priv->selection)
    gdl_update_max_margins (priv, child, allocation->width, allocation->height);

  gtk_widget_queue_resize (GTK_WIDGET (layout));
}

static gboolean
glade_design_layout_motion_notify_event (GtkWidget *widget, GdkEventMotion *ev)
{
  GladeDesignLayoutPrivate *priv;
  GtkAllocation allocation;
  GtkWidget *child;
  gint x, y;

  if ((child = gtk_bin_get_child (GTK_BIN (widget))) == NULL)
    return FALSE;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

  x = ev->x;
  y = ev->y;

  gtk_widget_get_allocation (child, &allocation);

  allocation.x += priv->child_offset;
  allocation.y += priv->child_offset;

  switch (priv->activity)
    {
    case ACTIVITY_RESIZE_WIDTH:
        allocation.width = MAX (0, x - priv->dx - PADDING - OUTLINE_WIDTH);
      break;
    case ACTIVITY_RESIZE_HEIGHT:
        allocation.height = MAX (0, y - priv->dy - PADDING - OUTLINE_WIDTH);
      break;
    case ACTIVITY_RESIZE_WIDTH_AND_HEIGHT:
        allocation.height = MAX (0, y - priv->dy - PADDING - OUTLINE_WIDTH);
        allocation.width = MAX (0, x - priv->dx - PADDING - OUTLINE_WIDTH);
      break;
    case ACTIVITY_MARGINS:
        {
          gboolean shift = ev->state & GDK_SHIFT_MASK;
          gboolean snap = ev->state & GDK_MOD1_MASK;
          GtkWidget *selection = priv->selection;
          Margins margin = priv->margin;

          if (margin & MARGIN_TOP)
            {
              gint max_height = (shift) ? priv->max_height/2 : priv->max_height -
                gtk_widget_get_margin_bottom (selection);
              gint val = MAX (0, MIN (priv->m_dy - y, max_height));
              
              if (snap) val = (val/MARGIN_STEP)*MARGIN_STEP;
              gtk_widget_set_margin_top (selection, val);
              if (shift) gtk_widget_set_margin_bottom (selection, val);
            }
          else if (margin & MARGIN_BOTTOM)
            {
              gint max_height = (shift) ? priv->max_height/2 : priv->max_height -
                gtk_widget_get_margin_top (selection);
              gint val = MAX (0, MIN (y - priv->m_dy, max_height));
              
              if (snap) val = (val/MARGIN_STEP)*MARGIN_STEP;
              gtk_widget_set_margin_bottom (selection, val);
              if (shift) gtk_widget_set_margin_top (selection, val);
            }

          if (margin & MARGIN_LEFT)
            {
              gint max_width = (shift) ? priv->max_width/2 : priv->max_width -
                gtk_widget_get_margin_right (selection);
              gint val = MAX (0, MIN (priv->m_dx - x, max_width));
              
              if (snap) val = (val/MARGIN_STEP)*MARGIN_STEP;
              gtk_widget_set_margin_left (selection, val);
              if (shift) gtk_widget_set_margin_right (selection, val);
            }
          else if (margin & MARGIN_RIGHT)
            {
              gint max_width = (shift) ? priv->max_width/2 : priv->max_width -
                gtk_widget_get_margin_left (selection);
              gint val = MAX (0, MIN (x - priv->m_dx, max_width));
              
              if (snap) val = (val/MARGIN_STEP)*MARGIN_STEP;
              gtk_widget_set_margin_right (selection, val);
              if (shift) gtk_widget_set_margin_left (selection, val);
            }
        }
      break;
    default:
        {
          Activity activity = gdl_get_activity_from_pointer (GLADE_DESIGN_LAYOUT (widget), x, y);

          if (activity == ACTIVITY_MARGINS)
            activity = gdl_margin_get_activity (priv->margin);

          /* Only set the cursor if changed */
          gdl_set_cursor (priv, priv->cursors[activity]);
          return TRUE;
        }
      break;
    }

  glade_design_layout_update_child (GLADE_DESIGN_LAYOUT (widget), child, &allocation);
  return FALSE;
}

typedef struct
{
  GtkWidget *toplevel;
  gint x;
  gint y;
  GtkWidget *placeholder;
  GladeWidget *gwidget;
} GladeFindInContainerData;

static void
glade_design_layout_find_inside_container (GtkWidget                *widget,
                                           GladeFindInContainerData *data)
{
  GtkAllocation allocation;
  gint x, y;

  if (data->gwidget || !gtk_widget_get_mapped (widget))
    return;

  gtk_widget_translate_coordinates (data->toplevel, widget, data->x, data->y,
                                    &x, &y);
  gtk_widget_get_allocation (widget, &allocation);

  if (x >= 0 && x < allocation.width && y >= 0 && y < allocation.height)
    {
      if (GLADE_IS_PLACEHOLDER (widget))
        data->placeholder = widget;
      else
        {
          if (GTK_IS_CONTAINER (widget))
            gtk_container_forall (GTK_CONTAINER (widget), (GtkCallback)
                                  glade_design_layout_find_inside_container,
                                  data);

          if (!data->gwidget)
            data->gwidget = glade_widget_get_from_gobject (widget);
        }
    }
}

static gboolean
glade_project_is_toplevel_active (GladeProject *project, GtkWidget *toplevel)
{
  GList *l;

  for (l = glade_project_selection_get (project); l; l = g_list_next (l))
    {
      if (GTK_IS_WIDGET (l->data) && 
	  gtk_widget_is_ancestor (l->data, toplevel)) return TRUE;
    }

  return FALSE;
}

static gboolean
glade_design_layout_button_press_event (GtkWidget *widget, GdkEventButton *ev)
{
  GladeDesignLayoutPrivate *priv;
  GtkWidget *child;
  gint x, y;

  if (ev->button != 1 ||
      (child = gtk_bin_get_child (GTK_BIN (widget))) == NULL)
    return FALSE;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

  x = ev->x;
  y = ev->y;

  if (ev->type == GDK_BUTTON_PRESS)
    {
      GtkAllocation child_allocation;

      priv->activity = gdl_get_activity_from_pointer (GLADE_DESIGN_LAYOUT (widget), x, y);
      
      /* Check if we are in margin edit mode */
      if (priv->selection)
        {
          if (priv->activity == ACTIVITY_NONE)
            {
              priv->selection = NULL;
              gdl_set_cursor (priv, NULL);
              glade_project_set_pointer_mode (priv->project, GLADE_POINTER_SELECT);
              gtk_widget_queue_draw (widget);
              return FALSE;
            }
          else if (priv->activity == ACTIVITY_MARGINS)
            {
              priv->m_dx = x + ((priv->margin & MARGIN_LEFT) ? 
                                gtk_widget_get_margin_left (priv->selection) :
                                  gtk_widget_get_margin_right (priv->selection) * -1);
              priv->m_dy = y + ((priv->margin & MARGIN_TOP) ?
                                gtk_widget_get_margin_top (priv->selection) :
                                  gtk_widget_get_margin_bottom (priv->selection) * -1);
              
              gdl_set_cursor (priv, priv->cursors[gdl_margin_get_activity (priv->margin)]);
              return FALSE;
            }
          else
            gdl_set_cursor (priv, priv->cursors[priv->activity]);
        }

      gtk_widget_get_allocation (child, &child_allocation);

      priv->dx = x - (child_allocation.x + child_allocation.width + priv->child_offset);
      priv->dy = y - (child_allocation.y + child_allocation.height + priv->child_offset);
      
      if (priv->activity != ACTIVITY_NONE &&
          !glade_project_is_toplevel_active (priv->project, child))
        {
          _glade_design_view_freeze (priv->view);
          glade_project_selection_set (priv->project, G_OBJECT (child), TRUE);
          _glade_design_view_thaw (priv->view);
        }
    }

  return FALSE;
}

static gboolean
glade_design_layout_button_release_event (GtkWidget *widget,
                                          GdkEventButton *ev)
{
  GladeDesignLayoutPrivate *priv;
  GtkWidget *child;

  if ((child = gtk_bin_get_child (GTK_BIN (widget))) == NULL)
    return FALSE;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

  /* Check if margins where edited and execute corresponding glade command */
  if (priv->selection && priv->activity == ACTIVITY_MARGINS)
    {
      GladeWidget *gwidget = glade_widget_get_from_gobject (priv->selection);
      gint top, bottom, left, right;
      GladeProperty *property;

      top = gtk_widget_get_margin_top (priv->selection);
      bottom = gtk_widget_get_margin_bottom (priv->selection);
      left = gtk_widget_get_margin_left (priv->selection);
      right = gtk_widget_get_margin_right (priv->selection);

      glade_command_push_group (_("Editing margins of %s"),
                                glade_widget_get_name (gwidget));
      if (priv->top != top)
        {
          if ((property = glade_widget_get_property (gwidget, "margin-top")))
            glade_command_set_property (property, top);
        }
      if (priv->bottom != bottom)
        {
          if ((property = glade_widget_get_property (gwidget, "margin-bottom")))
            glade_command_set_property (property, bottom);
        }
      if (priv->left != left)
        {
          if ((property = glade_widget_get_property (gwidget, "margin-left")))
            glade_command_set_property (property, left);
        }
      if (priv->right != right)
        {
          if ((property = glade_widget_get_property (gwidget, "margin-right")))
            glade_command_set_property (property, right);
        }
      glade_command_pop_group ();
    }
  
  priv->activity = ACTIVITY_NONE;
  gdl_set_cursor (priv, NULL);

  return FALSE;
}

static void
glade_design_layout_get_preferred_height (GtkWidget *widget,
                                          gint *minimum, gint *natural)
{
  GladeDesignLayoutPrivate *priv;
  GtkWidget *child;
  GladeWidget *gchild;
  gint child_height = 0;
  guint border_width = 0;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

  *minimum = 0;

  child = gtk_bin_get_child (GTK_BIN (widget));

  if (child && gtk_widget_get_visible (child))
    {
      GtkRequisition req;
      gint height;

      gchild = glade_widget_get_from_gobject (child);
      g_assert (gchild);

      gtk_widget_get_preferred_size (child, &req, NULL);

      g_object_get (gchild, "toplevel-height", &child_height, NULL);

      child_height = MAX (child_height, req.height);

      if (priv->widget_name)
        pango_layout_get_pixel_size (priv->widget_name, NULL, &height);
      else
        height = PADDING;

      *minimum = MAX (*minimum, PADDING + 2.5*OUTLINE_WIDTH + height + child_height);
    }

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
  *minimum += border_width * 2;
  *natural = *minimum;
}

static void
glade_design_layout_get_preferred_width (GtkWidget *widget,
                                         gint *minimum, gint *natural)
{
  GtkWidget *child;
  GladeWidget *gchild;
  gint child_width = 0;
  guint border_width = 0;

  *minimum = 0;

  child = gtk_bin_get_child (GTK_BIN (widget));

  if (child && gtk_widget_get_visible (child))
    {
      GtkRequisition req;
      
      gchild = glade_widget_get_from_gobject (child);
      g_assert (gchild);

      gtk_widget_get_preferred_size (child, &req, NULL);

      g_object_get (gchild, "toplevel-width", &child_width, NULL);

      child_width = MAX (child_width, req.width);

      *minimum = MAX (*minimum, 2*PADDING + 2*OUTLINE_WIDTH + child_width);
    }

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
  *minimum += border_width * 2;
  *natural = *minimum;
}

static void
glade_design_layout_get_preferred_width_for_height (GtkWidget       *widget,
                                                    gint             height,
                                                    gint            *minimum_width,
                                                    gint            *natural_width)
{
  glade_design_layout_get_preferred_width (widget, minimum_width, natural_width);
}

static void
glade_design_layout_get_preferred_height_for_width (GtkWidget       *widget,
                                                    gint             width,
                                                    gint            *minimum_height,
                                                    gint            *natural_height)
{
  glade_design_layout_get_preferred_height (widget, minimum_height, natural_height);
}

static void
update_rectangles (GladeDesignLayoutPrivate *priv, GtkAllocation *alloc)
{
  GdkRectangle *rect = &priv->south_east;
  gint width, height;

  /* Update rectangles used to resize the children */
  priv->east.x = alloc->width + priv->child_offset;
  priv->east.y = priv->child_offset;
  priv->east.height = alloc->height;

  priv->south.x = priv->child_offset;
  priv->south.y = alloc->height + priv->child_offset;
  priv->south.width = alloc->width;
  
  /* Update south east rectangle width */
  pango_layout_get_pixel_size (priv->widget_name, &width, &height);
  priv->layout_width = width + (OUTLINE_WIDTH*2);
  width = MIN (alloc->width, width);

  rect->x = alloc->x + priv->child_offset + alloc->width - width - OUTLINE_WIDTH/2;
  rect->y = alloc->y + priv->child_offset + alloc->height + OUTLINE_WIDTH/2;
  rect->width = width + (OUTLINE_WIDTH*2);
  rect->height = height + OUTLINE_WIDTH;

  /* Update south rectangle width */
  priv->south.width = rect->x - priv->south.x;
}

static void
glade_design_layout_size_allocate (GtkWidget *widget,
                                   GtkAllocation *allocation)
{
  GtkWidget *child;

  gtk_widget_set_allocation (widget, allocation);
    
  if (gtk_widget_get_realized (widget))
  {
    gdk_window_move_resize (gtk_widget_get_window (widget),
                            allocation->x, allocation->y,
                            allocation->width, allocation->height);
  }

  child = gtk_bin_get_child (GTK_BIN (widget));

  if (child && gtk_widget_get_visible (child))
    {
      GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);
      GtkAllocation alloc;
      gint height, offset;

      offset = gtk_container_get_border_width (GTK_CONTAINER (widget)) + PADDING + OUTLINE_WIDTH;
      priv->child_offset = offset;

      if (priv->widget_name)
        pango_layout_get_pixel_size (priv->widget_name, NULL, &height);
      else
        height = PADDING;
      
      alloc.x = alloc.y = 0;
      priv->current_width = alloc.width = allocation->width - (offset * 2);
      priv->current_height = alloc.height = allocation->height - (offset + OUTLINE_WIDTH * 1.5 + height);
      
      if (gtk_widget_get_realized (widget))
        gdk_window_move_resize (priv->offscreen_window,
                                0, 0, alloc.width, alloc.height);

      gtk_widget_size_allocate (child, &alloc);
      update_rectangles (priv, &alloc);
    }
}

static void
on_glade_widget_name_notify (GObject *gobject, GParamSpec *pspec, GladeDesignLayout *layout) 
{
  GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);
  
  pango_layout_set_text (priv->widget_name, glade_widget_get_name (GLADE_WIDGET (gobject)), -1);
  gtk_widget_queue_resize (GTK_WIDGET (layout));
}

static void
glade_design_layout_add (GtkContainer *container, GtkWidget *widget)
{
  GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (container);
  GladeDesignLayout *layout = GLADE_DESIGN_LAYOUT (container);
  GladeWidget *gchild;

  layout->priv->current_width = 0;
  layout->priv->current_height = 0;

  gtk_widget_set_parent_window (widget, priv->offscreen_window);

  GTK_CONTAINER_CLASS (glade_design_layout_parent_class)->add (container,
                                                               widget);

  if ((gchild = glade_widget_get_from_gobject (G_OBJECT (widget))))
    {
      on_glade_widget_name_notify (G_OBJECT (gchild), NULL, layout);
      g_signal_connect (gchild, "notify::name", G_CALLBACK (on_glade_widget_name_notify), layout);
    }
    
  gtk_widget_queue_draw (GTK_WIDGET (container)); 
}

static void
glade_design_layout_remove (GtkContainer *container, GtkWidget *widget)
{
  GladeWidget *gchild;

  if ((gchild = glade_widget_get_from_gobject (G_OBJECT (widget))))
    g_signal_handlers_disconnect_by_func (gchild, on_glade_widget_name_notify,
                                          GLADE_DESIGN_LAYOUT (container));

  GTK_CONTAINER_CLASS (glade_design_layout_parent_class)->remove (container, widget);
  gtk_widget_queue_draw (GTK_WIDGET (container));
}

static gboolean
glade_design_layout_damage (GtkWidget *widget, GdkEventExpose *event)
{
  gdk_window_invalidate_rect (gtk_widget_get_window (widget), NULL, TRUE);
  return TRUE;
}

static inline void
draw_frame (cairo_t *cr, GladeDesignLayoutPrivate *priv, gboolean selected,
            int x, int y, int w, int h)
{
  cairo_save (cr);

  cairo_set_line_width (cr, OUTLINE_WIDTH);

  cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

  gdk_cairo_set_source_rgba (cr, (selected) ? &priv->frame_color_active[0] :
                             &priv->frame_color[0]);

  /* rectangle */
  cairo_rectangle (cr, x, y, w, h);
  cairo_stroke (cr);

  if (priv->widget_name)
    {
      GdkRGBA *color = (selected) ? &priv->frame_color_active[1] : &priv->frame_color[1];
      GdkRectangle *rect = &priv->south_east;
      cairo_pattern_t *pattern;
      gdouble xx, yy;
      
      xx = rect->x + rect->width;
      yy = rect->y + rect->height;

      /* Draw tab */
      cairo_move_to (cr, rect->x, rect->y);
      cairo_line_to (cr, xx, rect->y);
      cairo_line_to (cr, xx, yy-8);
      cairo_curve_to (cr, xx, yy, xx, yy, xx-8, yy);
      cairo_line_to (cr, rect->x+8, yy);
      cairo_curve_to (cr, rect->x, yy, rect->x, yy, rect->x, yy-8);
      cairo_close_path (cr);
      cairo_fill (cr);

      /* Draw widget name */
      if (rect->width < priv->layout_width)
        {
          gdouble r = color->red, g = color->green, b = color->blue;
          
          pattern = cairo_pattern_create_linear (xx-16-OUTLINE_WIDTH, 0,
                                                 xx-OUTLINE_WIDTH, 0);
          cairo_pattern_add_color_stop_rgba (pattern, 0, r, g, b, 1);
          cairo_pattern_add_color_stop_rgba (pattern, 1, r, g, b, 0);
          cairo_set_source (cr, pattern);
        }
      else
        {
          pattern = NULL;
          gdk_cairo_set_source_rgba (cr, color);
        }

      cairo_move_to (cr, rect->x + OUTLINE_WIDTH, rect->y + OUTLINE_WIDTH);
      pango_cairo_show_layout (cr, priv->widget_name);

      if (pattern) cairo_pattern_destroy (pattern);
    }

  cairo_restore (cr);
}

static void
draw_margin_selection (cairo_t *cr,
                       gint x1, gint x2, gint x3, gint x4, 
                       gint y1, gint y2, gint y3, gint y4,
                       gdouble r, gdouble g, gdouble b,
                       gint x5, gint y5)
{
  cairo_pattern_t *gradient = cairo_pattern_create_linear (x1, y1, x5, y5);

  cairo_pattern_add_color_stop_rgba (gradient, 0, r+.24, g+.24, b+.24, .08);
  cairo_pattern_add_color_stop_rgba (gradient, 1, r, g, b, .16);
  
  cairo_set_source (cr, gradient);
  
  cairo_move_to (cr, x1, y1);
  cairo_line_to (cr, x2, y2);
  cairo_line_to (cr, x3, y3);
  cairo_line_to (cr, x4, y4);
  cairo_close_path (cr);
  cairo_fill (cr);

  cairo_pattern_destroy (gradient);
}

static inline void
draw_selection (cairo_t *cr,
                GtkWidget *parent,
                GtkWidget *widget,
                GdkRGBA *color)
{
  gint x, y, w, h, xw, yh, y_top, yh_bottom, x_left, xw_right;
  gint top, bottom, left, right;
  cairo_pattern_t *gradient;
  gdouble r, g, b, cx, cy;
  GtkAllocation alloc;

  gtk_widget_get_allocation (widget, &alloc);

  if (alloc.x < 0 || alloc.y < 0) return;
  
  r = color->red; g = color->green; b = color->blue;
  gtk_widget_translate_coordinates (widget, parent, 0, 0, &x, &y);

  w = alloc.width;
  h = alloc.height;
  xw = x + w;
  yh = y + h;

  top = gtk_widget_get_margin_top (widget);
  bottom = gtk_widget_get_margin_bottom (widget);
  left = gtk_widget_get_margin_left (widget);
  right = gtk_widget_get_margin_right (widget);
  
  y_top = y - top;
  yh_bottom = yh + bottom;
  x_left = x - left;
  xw_right = xw + right;
  
  /* Draw widget area overlay */
  cx = x + w/2;
  cy = y + h/2;
  gradient = cairo_pattern_create_radial (cx, cy, MIN (w, h)/6, cx, cy, MAX (w, h)/2);
  cairo_pattern_add_color_stop_rgba (gradient, 0, r+.24, g+.24, b+.24, .16);
  cairo_pattern_add_color_stop_rgba (gradient, 1, r, g, b, .28);
  cairo_set_source (cr, gradient);

  cairo_rectangle (cr, x, y, w, h);
  cairo_fill (cr);

  cairo_pattern_destroy (gradient);

  /* Draw margins overlays */
  if (top)
    draw_margin_selection (cr, x, xw, xw_right, x_left, y, y, y_top, y_top,
                           r, g, b, x, y_top);

  if (bottom)
    draw_margin_selection (cr, x, xw, xw_right, x_left, yh, yh, yh_bottom, yh_bottom,
                           r, g, b, x, yh_bottom);

  if (left)
    draw_margin_selection (cr, x, x, x_left, x_left, y, yh, yh_bottom, y_top,
                           r, g, b, x_left, y);

  if (right)
    draw_margin_selection (cr, xw, xw, xw_right, xw_right, y, yh, yh_bottom, y_top,
                           r, g, b, xw_right, y);

  /* Draw Boxes */
  cairo_set_source_rgba (cr, r, g, b, .75);
  if (top || bottom || left || right)
    {
      gdouble dashes[] = { 4.0, 4.0 };
      cairo_rectangle (cr,
                       x - left,
                       y - top,
                       w + left + right,
                       h + top + bottom);
      cairo_stroke (cr);
      cairo_set_dash (cr, dashes, 2, 0);
    }

  /* Draw Widget allocation box */
  cairo_rectangle (cr, x, y, w, h);
  cairo_stroke (cr);

  cairo_set_dash (cr, NULL, 0, 0);
}

static void 
draw_node (cairo_t *cr, gint x, gint y, gint radius, GdkRGBA *c1, GdkRGBA *c2)
{
  gdk_cairo_set_source_rgba (cr, c2);
  cairo_new_sub_path (cr);
  cairo_arc (cr, x, y, radius, 0, 2*G_PI);
  cairo_stroke_preserve (cr);

  gdk_cairo_set_source_rgba (cr, c1);
  cairo_fill (cr);
}

static inline void
draw_selection_nodes (cairo_t *cr,
                      GtkWidget *parent,
                      GtkWidget *widget,
                      GdkRGBA *color1,
                      GdkRGBA *color2)
{
  gint x1, x2, x3, y1, y2, y3;
  GtkAllocation alloc;
  gint x, y, w, h;

  gtk_widget_get_allocation (widget, &alloc);
  w = alloc.width;
  h = alloc.height;
  
  if (x < 0 || y < 0) return;
  
  gtk_widget_translate_coordinates (widget, parent, 0, 0, &x, &y);
  
  /* Draw nodes */
  x1 = x - gtk_widget_get_margin_left (widget);
  x2 = x + w/2;
  x3 = x + w + gtk_widget_get_margin_right (widget);
  y1 = y - gtk_widget_get_margin_top (widget);
  y2 = y + h/2;
  y3 = y + h + gtk_widget_get_margin_bottom (widget);

  cairo_set_line_width (cr, OUTLINE_WIDTH);
  draw_node (cr, x2, y1, OUTLINE_WIDTH, color1, color2);
  draw_node (cr, x2, y3, OUTLINE_WIDTH, color1, color2);
  draw_node (cr, x1, y2, OUTLINE_WIDTH, color1, color2);
  draw_node (cr, x3, y2, OUTLINE_WIDTH, color1, color2);
}

static gboolean
glade_design_layout_draw (GtkWidget *widget, cairo_t *cr)
{
  GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);
  GdkWindow *window = gtk_widget_get_window (widget);

  if (gtk_cairo_should_draw_window (cr, window))
    {
      GtkWidget *child;

      if ((child = gtk_bin_get_child (GTK_BIN (widget))) &&
          gtk_widget_get_visible (child))
        {
          gint border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
          gboolean selected = FALSE;
          GList *l;
          
          /* draw offscreen widgets */
          gdk_cairo_set_source_window (cr, priv->offscreen_window,
                                       priv->child_offset, priv->child_offset);
          cairo_rectangle (cr, priv->child_offset, priv->child_offset,
                           priv->current_width, priv->current_height);
          cairo_fill (cr);

          /* Draw selection */
          cairo_set_line_width (cr, OUTLINE_WIDTH/2);
          cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
          cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
          for (l = glade_project_selection_get (priv->project); l; l = g_list_next (l))
            {
              GtkWidget *selection = l->data;
              
              /* Dont draw selection on toplevels */
              if (child != selection)
                {
                  if (GTK_IS_WIDGET (selection) && 
		      gtk_widget_is_ancestor (selection, child))
                  {
                    draw_selection (cr, widget, selection, &priv->frame_color_active[0]);
                    selected = TRUE;
                  }
                }
              else
                selected = TRUE;
            }

          /* draw frame */
          draw_frame (cr, priv, selected,
                      border_width + PADDING,
                      border_width + PADDING,
                      priv->current_width + 2 * OUTLINE_WIDTH,
                      priv->current_height + 2 * OUTLINE_WIDTH);

          /* Draw selection nodes if we are in margins edit mode */
          if (priv->selection)
            draw_selection_nodes (cr, widget, priv->selection,
                                  &priv->frame_color_active[0],
                                  &priv->frame_color_active[1]);
        }
    }
  else if (gtk_cairo_should_draw_window (cr, priv->offscreen_window))
    {
      GtkWidget *child = gtk_bin_get_child (GTK_BIN (widget));

      gtk_render_background (gtk_widget_get_style_context (child),
                             cr,
                             0, 0,
                             gdk_window_get_width (priv->offscreen_window),
                             gdk_window_get_height (priv->offscreen_window));

      if (child)
          gtk_container_propagate_draw (GTK_CONTAINER (widget), child, cr);
    }

  return FALSE;
}

static inline void
to_child (GladeDesignLayout *bin,
          double         widget_x,
          double         widget_y,
          double        *x_out,
          double        *y_out)
{
  GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (bin);
  *x_out = widget_x - priv->child_offset;
  *y_out = widget_y - priv->child_offset;
}

static inline void
to_parent (GladeDesignLayout *bin,
           double         offscreen_x,
           double         offscreen_y,
           double        *x_out,
           double        *y_out)
{
  GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (bin);
  *x_out = offscreen_x + priv->child_offset;
  *y_out = offscreen_y + priv->child_offset;
}

static GdkWindow *
pick_offscreen_child (GdkWindow     *offscreen_window,
                      double         widget_x,
                      double         widget_y,
                      GladeDesignLayout *bin)
{
  GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (bin);
  GtkWidget *child = gtk_bin_get_child (GTK_BIN (bin));

  if (child && gtk_widget_get_visible (child))
    {
      GtkAllocation child_area;
      double x, y;

      to_child (bin, widget_x, widget_y, &x, &y);
        
      gtk_widget_get_allocation (child, &child_area);

      if (x >= 0 && x < child_area.width && y >= 0 && y < child_area.height)
        return (priv->selection) ? NULL : priv->offscreen_window;
    }

  return NULL;
}

static void
offscreen_window_to_parent (GdkWindow     *offscreen_window,
                            double         offscreen_x,
                            double         offscreen_y,
                            double        *parent_x,
                            double        *parent_y,
                            GladeDesignLayout *bin)
{
  to_parent (bin, offscreen_x, offscreen_y, parent_x, parent_y);
}

static void
offscreen_window_from_parent (GdkWindow     *window,
                              double         parent_x,
                              double         parent_y,
                              double        *offscreen_x,
                              double        *offscreen_y,
                              GladeDesignLayout *bin)
{
  to_child (bin, parent_x, parent_y, offscreen_x, offscreen_y);
}

static void
glade_design_layout_realize (GtkWidget * widget)
{
  GladeDesignLayoutPrivate *priv;
  GtkStyleContext *context;
  GdkWindowAttr attributes;
  GtkWidget *child;
  gint attributes_mask, border_width;
  GtkAllocation allocation;
  GdkDisplay *display;
    
  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

  gtk_widget_set_realized (widget, TRUE);
    
  gtk_widget_get_allocation (widget, &allocation);
  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

  attributes.x = allocation.x + border_width;
  attributes.y = allocation.y + border_width;
  attributes.width = allocation.width - 2 * border_width;
  attributes.height = allocation.height - 2 * border_width;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = gtk_widget_get_events (widget)
                        | GDK_EXPOSURE_MASK
                        | GDK_POINTER_MOTION_MASK
                        | GDK_BUTTON_PRESS_MASK
                        | GDK_BUTTON_RELEASE_MASK
                        | GDK_SCROLL_MASK
                        | GDK_ENTER_NOTIFY_MASK
                        | GDK_LEAVE_NOTIFY_MASK;

  attributes.visual = gtk_widget_get_visual (widget);
  attributes.wclass = GDK_INPUT_OUTPUT;

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  priv->window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                 &attributes, attributes_mask);
  gtk_widget_set_window (widget, priv->window);
  gdk_window_set_user_data (priv->window, widget);

  g_signal_connect (priv->window, "pick-embedded-child",
                    G_CALLBACK (pick_offscreen_child), widget);

  /* Offscreen window */
  child = gtk_bin_get_child (GTK_BIN (widget));
  attributes.window_type = GDK_WINDOW_OFFSCREEN;
  attributes.x = attributes.y = 0;

  if (child && gtk_widget_get_visible (child))
    {
      GtkAllocation alloc;

      gtk_widget_get_allocation (child, &alloc);
      attributes.width = alloc.width;
      attributes.height = alloc.height;
    }
    else
      attributes.width = attributes.height = 0;

  priv->offscreen_window = gdk_window_new (gtk_widget_get_root_window (widget),
                                           &attributes, attributes_mask);
  gdk_window_set_user_data (priv->offscreen_window, widget);
  
  if (child) gtk_widget_set_parent_window (child, priv->offscreen_window);
  gdk_offscreen_window_set_embedder (priv->offscreen_window, priv->window);

  g_signal_connect (priv->offscreen_window, "to-embedder",
                    G_CALLBACK (offscreen_window_to_parent), widget);
  g_signal_connect (priv->offscreen_window, "from-embedder",
                    G_CALLBACK (offscreen_window_from_parent), widget);

  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_background (context, priv->window);
  gtk_style_context_set_background (context, priv->offscreen_window);
  gdk_window_show (priv->offscreen_window);

  gdk_window_set_cursor (priv->window, NULL);
  gdk_window_set_cursor (priv->offscreen_window, NULL);
  
  /* Allocate cursors */
  display = gtk_widget_get_display (widget);
  priv->cursors[ACTIVITY_RESIZE_HEIGHT] = gdk_cursor_new_for_display (display, GDK_BOTTOM_SIDE);
  priv->cursors[ACTIVITY_RESIZE_WIDTH] = gdk_cursor_new_for_display (display, GDK_RIGHT_SIDE);
  priv->cursors[ACTIVITY_RESIZE_WIDTH_AND_HEIGHT] = gdk_cursor_new_for_display (display, GDK_BOTTOM_RIGHT_CORNER);

  priv->cursors[ACTIVITY_MARGINS_VERTICAL] = gdk_cursor_new_for_display (display, GDK_SB_V_DOUBLE_ARROW);
  priv->cursors[ACTIVITY_MARGINS_HORIZONTAL] = gdk_cursor_new_for_display (display, GDK_SB_H_DOUBLE_ARROW);
  priv->cursors[ACTIVITY_MARGINS_TOP_LEFT] = gdk_cursor_new_for_display (display, GDK_TOP_LEFT_CORNER);
  priv->cursors[ACTIVITY_MARGINS_TOP_RIGHT] = gdk_cursor_new_for_display (display, GDK_TOP_RIGHT_CORNER);
  priv->cursors[ACTIVITY_MARGINS_BOTTOM_LEFT] = gdk_cursor_new_for_display (display, GDK_BOTTOM_LEFT_CORNER);
  priv->cursors[ACTIVITY_MARGINS_BOTTOM_RIGHT] = gdk_cursor_ref (priv->cursors[ACTIVITY_RESIZE_WIDTH_AND_HEIGHT]);
  
  priv->widget_name = pango_layout_new (gtk_widget_get_pango_context (widget));
}

static void
glade_design_layout_unrealize (GtkWidget * widget)
{
  GladeDesignLayoutPrivate *priv;
  gint i;
  
  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

  if (priv->offscreen_window)
    {
      gdk_window_set_user_data (priv->offscreen_window, NULL);
      gdk_window_destroy (priv->offscreen_window);
      priv->offscreen_window = NULL;
    }

  /* Free cursors */
  for (i = 0; i < N_ACTIVITY; i++)
    {
      if (priv->cursors[i])
        {
          gdk_cursor_unref (priv->cursors[i]);
          priv->cursors[i] = NULL;
        }
    }

  priv->cursor = NULL;

  if (priv->widget_name)
    {
      g_object_unref (priv->widget_name);
      priv->widget_name = NULL;
    }
  
  GTK_WIDGET_CLASS (glade_design_layout_parent_class)->unrealize (widget);
}

static void
glade_design_layout_style_updated (GtkWidget *widget)
{
  GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);
  GtkStyleContext *context = gtk_widget_get_style_context (widget);
  GdkRGBA bg_color;

  gtk_style_context_save (context);
  
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);

  gtk_style_context_get_background_color (context,
                                          GTK_STATE_FLAG_NORMAL,
                                          &bg_color);

  gtk_style_context_get_background_color (context, GTK_STATE_FLAG_SELECTED,
                                          &priv->frame_color[0]);
  gtk_style_context_get_color (context, GTK_STATE_FLAG_SELECTED,
                               &priv->frame_color[1]);
  
  gtk_style_context_get_background_color (context,
                                          GTK_STATE_FLAG_SELECTED |
                                          GTK_STATE_FLAG_FOCUSED,
                                          &priv->frame_color_active[0]);
  gtk_style_context_get_color (context,
                               GTK_STATE_FLAG_SELECTED |
                               GTK_STATE_FLAG_FOCUSED,
                               &priv->frame_color_active[1]);

  gtk_style_context_restore (context);
  
  gtk_widget_override_background_color (widget, GTK_STATE_FLAG_NORMAL, &bg_color);
}

static void
glade_design_layout_init (GladeDesignLayout *layout)
{
  GladeDesignLayoutPrivate *priv;
  gint i;
  
  layout->priv = priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);

  priv->activity = ACTIVITY_NONE;

  for (i = 0; i < N_ACTIVITY; i++) priv->cursors[i] = NULL;

  priv->new_width = -1;
  priv->new_height = -1;

  /* setup static member of rectangles */
  priv->east.width = PADDING + OUTLINE_WIDTH;
  priv->south.height = PADDING + OUTLINE_WIDTH;

  gtk_widget_set_has_window (GTK_WIDGET (layout), TRUE);
}

static void
glade_design_layout_set_property (GObject *object,
                                  guint prop_id,
                                  const GValue *value,
                                  GParamSpec *pspec)
{
  switch (prop_id)
    {
      case PROP_DESIGN_VIEW:
        {
          GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (object);
          priv->view = GLADE_DESIGN_VIEW (g_value_get_object (value));
          priv->project = glade_design_view_get_project (priv->view);
        }
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_design_layout_get_property (GObject * object,
                                  guint prop_id,
                                  GValue * value,
                                  GParamSpec * pspec)
{
  switch (prop_id)
    {
      case PROP_DESIGN_VIEW:
        g_value_set_object (value, GLADE_DESIGN_LAYOUT_GET_PRIVATE (object)->view);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
on_project_selection_changed (GladeProject *project, GladeDesignLayout *layout)
{
  layout->priv->selection = NULL;
  glade_project_set_pointer_mode (layout->priv->project, GLADE_POINTER_SELECT);
  gtk_widget_queue_draw (GTK_WIDGET (layout));
}

static GObject *
glade_design_layout_constructor (GType                  type,
                                 guint                  n_construct_params,
                                 GObjectConstructParam *construct_params)
{
  GladeDesignLayoutPrivate *priv;
  GObject *object;
    
  object = G_OBJECT_CLASS (glade_design_layout_parent_class)->constructor (type,
                                                                           n_construct_params,
                                                                           construct_params);

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (object);

  g_signal_connect (priv->project,
                    "selection-changed",
                    G_CALLBACK (on_project_selection_changed),
                    GLADE_DESIGN_LAYOUT (object));
                    
  return object;
}

static void
glade_design_layout_finalize (GObject *object)
{
  GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (object);

  g_signal_handlers_disconnect_by_func (priv->project,
                                        on_project_selection_changed,
                                        GLADE_DESIGN_LAYOUT (object));
  
  G_OBJECT_CLASS (glade_design_layout_parent_class)->finalize (object);
}

static void
glade_design_layout_class_init (GladeDesignLayoutClass * klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);
  container_class = GTK_CONTAINER_CLASS (klass);

  object_class->constructor = glade_design_layout_constructor;
  object_class->finalize = glade_design_layout_finalize;
  object_class->set_property = glade_design_layout_set_property;
  object_class->get_property = glade_design_layout_get_property;
  
  container_class->add = glade_design_layout_add;
  container_class->remove = glade_design_layout_remove;

  widget_class->realize = glade_design_layout_realize;
  widget_class->unrealize = glade_design_layout_unrealize;
  widget_class->motion_notify_event = glade_design_layout_motion_notify_event;
  widget_class->leave_notify_event = glade_design_layout_leave_notify_event;
  widget_class->button_press_event = glade_design_layout_button_press_event;
  widget_class->button_release_event = glade_design_layout_button_release_event;
  widget_class->draw = glade_design_layout_draw;
  widget_class->get_preferred_height = glade_design_layout_get_preferred_height;
  widget_class->get_preferred_width = glade_design_layout_get_preferred_width;
  widget_class->get_preferred_width_for_height = glade_design_layout_get_preferred_width_for_height;
  widget_class->get_preferred_height_for_width = glade_design_layout_get_preferred_height_for_width;
  widget_class->size_allocate = glade_design_layout_size_allocate;
  widget_class->style_updated = glade_design_layout_style_updated;

  g_object_class_install_property (object_class, PROP_DESIGN_VIEW,
                                   g_param_spec_object ("design-view", _("Design View"),
                                                        _("The GladeDesignView that contains this layout"),
                                                        GLADE_TYPE_DESIGN_VIEW,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  
  g_signal_override_class_closure (g_signal_lookup ("damage-event", GTK_TYPE_WIDGET),
                                   GLADE_TYPE_DESIGN_LAYOUT,
                                   g_cclosure_new (G_CALLBACK (glade_design_layout_damage),
                                                   NULL, NULL));

  g_type_class_add_private (object_class, sizeof (GladeDesignLayoutPrivate));
}

/* Internal API */

GtkWidget *
_glade_design_layout_new (GladeDesignView *view)
{
  return g_object_new (GLADE_TYPE_DESIGN_LAYOUT, "design-view", view, NULL);
}

/*
 * _glade_design_layout_do_event:
 * @layout: A #GladeDesignLayout
 * @event: an event to process
 *
 * Process events to make widget selection work. This function should be called
 * before the child widget get the event. See gdk_event_handler_set()
 *
 * Returns: true if the event was handled.
 */
gboolean
_glade_design_layout_do_event (GladeDesignLayout *layout, GdkEvent *event)
{
  GladeFindInContainerData data = { 0, };
  GladeDesignLayoutPrivate *priv;
  GtkWidget *child;
  gboolean retval;
  GList *l;
  
  if ((child = gtk_bin_get_child (GTK_BIN (layout))) == NULL)
    return FALSE;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);

  data.toplevel = GTK_WIDGET (child);
  gtk_widget_get_pointer (GTK_WIDGET (child), &data.x, &data.y);

  /* Check if we want to enter in margin edit mode */
  if (event->type == GDK_BUTTON_PRESS &&
      (l = glade_project_selection_get (priv->project)) &&
      g_list_next (l) == NULL && GTK_IS_WIDGET (l->data) && 
      gtk_widget_is_ancestor (l->data, child))
    {
      if (gdl_get_margins_from_pointer (l->data, data.x, data.y))
        {
          if (priv->selection == NULL)
            {
              priv->selection = l->data;

              /* Save initital margins to know which one where edited */
              priv->top = gtk_widget_get_margin_top (priv->selection);
              priv->bottom = gtk_widget_get_margin_bottom (priv->selection);
              priv->left = gtk_widget_get_margin_left (priv->selection);
              priv->right = gtk_widget_get_margin_right (priv->selection);

              gdl_update_max_margins (priv, child,
                                      gtk_widget_get_allocated_width (child),
                                      gtk_widget_get_allocated_height (child));

              glade_project_set_pointer_mode (priv->project, GLADE_POINTER_MARGIN_MODE);
              gtk_widget_queue_draw (GTK_WIDGET (layout));
              return TRUE;
            }
          return FALSE;
        }
    }

  _glade_design_view_freeze (priv->view);
  
  glade_design_layout_find_inside_container (child, &data);
  
  /* Try the placeholder first */
  if (data.placeholder && gtk_widget_event (data.placeholder, event)) 
    retval = TRUE;
  else if (data.gwidget) /* Then we try a GladeWidget */
    retval = glade_widget_event (data.gwidget, event);
  else
    retval = FALSE;

  _glade_design_view_thaw (priv->view);

  return retval;
}
