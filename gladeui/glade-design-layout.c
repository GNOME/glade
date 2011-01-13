/*
 * glade-design-layout.c
 *
 * Copyright (C) 2006-2007 Vincent Geddes
 *
 * Authors:
 *   Vincent Geddes <vgeddes@gnome.org>
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
 *
 */

#include "config.h"

#include "glade.h"
#include "glade-design-layout.h"
#include "glade-accumulators.h"
#include "glade-marshallers.h"

#include <gtk/gtk.h>

#define GLADE_DESIGN_LAYOUT_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object),  \
						 GLADE_TYPE_DESIGN_LAYOUT,               \
						 GladeDesignLayoutPrivate))

#define OUTLINE_WIDTH     4
#define PADDING          12

typedef enum
{
  ACTIVITY_NONE,
  ACTIVITY_RESIZE_WIDTH,
  ACTIVITY_RESIZE_HEIGHT,
  ACTIVITY_RESIZE_WIDTH_AND_HEIGHT
} Activity;

#define ACTIVITY_STR(act)						\
	((act) == ACTIVITY_NONE ? "none" :				\
	 (act) == ACTIVITY_RESIZE_WIDTH ? "resize width" :		\
	 (act) == ACTIVITY_RESIZE_HEIGHT ? "resize height" : "resize width & height")

typedef enum
{
  REGION_INSIDE,
  REGION_EAST,
  REGION_SOUTH,
  REGION_SOUTH_EAST,
  REGION_WEST_OF_SOUTH_EAST,
  REGION_NORTH_OF_SOUTH_EAST,
  REGION_OUTSIDE
} PointerRegion;

enum
{
  DUMMY_SIGNAL,
  LAST_SIGNAL
};

struct _GladeDesignLayoutPrivate
{
  GdkWindow *window, *offscreen_window;

  GdkCursor *cursor_resize_bottom;
  GdkCursor *cursor_resize_right;
  GdkCursor *cursor_resize_bottom_right;

  GladeProject *project;
    
  /* state machine */
  Activity activity;            /* the current activity */
  GtkRequisition *current_size_request;
  gint dx;                      /* child.width - event.pointer.x   */
  gint dy;                      /* child.height - event.pointer.y  */
  gint new_width;               /* user's new requested width */
  gint new_height;              /* user's new requested height */
};

static guint glade_design_layout_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GladeDesignLayout, glade_design_layout, GTK_TYPE_BIN)

static void
ensure_project (GtkWidget * layout)
{
  GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);
  GtkWidget *parent = layout;
    
  if (priv->project) return;

  while ((parent = gtk_widget_get_parent (parent)))
    {
        if (GLADE_IS_DESIGN_VIEW (parent))
        {
            priv->project = glade_design_view_get_project (GLADE_DESIGN_VIEW (parent));
            g_message ("jajajaja");
            return;
        }
    }
}

static PointerRegion
glade_design_layout_get_pointer_region (GladeDesignLayout * layout, gint x, gint y)
{
  GladeDesignLayoutPrivate *priv;
  GtkAllocation child_allocation;
  PointerRegion region = REGION_INSIDE;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);

  gtk_widget_get_allocation (gtk_bin_get_child (GTK_BIN (layout)),
                             &child_allocation);

  gint offset = gtk_container_get_border_width (GTK_CONTAINER (layout)) + PADDING + OUTLINE_WIDTH;
  child_allocation.x += offset;
  child_allocation.y += offset;
    
  if ((x >= (child_allocation.x + child_allocation.width)) &&
      (x < (child_allocation.x + child_allocation.width + OUTLINE_WIDTH)))
    {
      if ((y < (child_allocation.y + child_allocation.height - OUTLINE_WIDTH))
          && (y >= child_allocation.y - OUTLINE_WIDTH))
        region = REGION_EAST;

      else if ((y < (child_allocation.y + child_allocation.height)) &&
               (y >=
                (child_allocation.y + child_allocation.height - OUTLINE_WIDTH)))
        region = REGION_NORTH_OF_SOUTH_EAST;

      else if ((y <
                (child_allocation.y + child_allocation.height + OUTLINE_WIDTH))
               && (y >= (child_allocation.y + child_allocation.height)))
        region = REGION_SOUTH_EAST;
    }
  else if ((y >= (child_allocation.y + child_allocation.height)) &&
           (y < (child_allocation.y + child_allocation.height + OUTLINE_WIDTH)))
    {
      if ((x < (child_allocation.x + child_allocation.width - OUTLINE_WIDTH)) &&
          (x >= child_allocation.x - OUTLINE_WIDTH))
        region = REGION_SOUTH;

      else if ((x < (child_allocation.x + child_allocation.width)) &&
               (x >=
                (child_allocation.x + child_allocation.width - OUTLINE_WIDTH)))
        region = REGION_WEST_OF_SOUTH_EAST;

      else if ((x <
                (child_allocation.x + child_allocation.width + OUTLINE_WIDTH))
               && (x >= (child_allocation.x + child_allocation.width)))
        region = REGION_SOUTH_EAST;
    }

  if (x < PADDING || y < PADDING ||
      x >= (child_allocation.x + child_allocation.width + OUTLINE_WIDTH) ||
      y >= (child_allocation.y + child_allocation.height + OUTLINE_WIDTH))
    region = REGION_OUTSIDE;

  return region;
}

static gboolean
glade_design_layout_leave_notify_event (GtkWidget * widget,
                                        GdkEventCrossing * ev)
{
  GtkWidget *child;
  GladeDesignLayoutPrivate *priv;

  if ((child = gtk_bin_get_child (GTK_BIN (widget))) == NULL)
    return FALSE;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

  if (priv->activity == ACTIVITY_NONE)
    gdk_window_set_cursor (priv->window, NULL);

  return FALSE;
}


static void
glade_design_layout_update_child (GladeDesignLayout * layout,
                                  GtkWidget * child, GtkAllocation * allocation)
{
  GladeDesignLayoutPrivate *priv;
  GladeWidget *gchild;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);

  /* Update GladeWidget metadata */
  gchild = glade_widget_get_from_gobject (child);
  g_object_set (gchild,
                "toplevel-width", allocation->width,
                "toplevel-height", allocation->height, NULL);

  gtk_widget_size_allocate (child, allocation);
  gtk_widget_queue_resize (GTK_WIDGET (layout));
}

static gboolean
glade_design_layout_motion_notify_event (GtkWidget * widget,
                                         GdkEventMotion * ev)
{
  GtkWidget *child;
  GladeDesignLayoutPrivate *priv;
  GladeWidget *child_glade_widget;
  PointerRegion region;
  GtkAllocation allocation;
  gint x, y;
  gint new_width, new_height;

  if ((child = gtk_bin_get_child (GTK_BIN (widget))) == NULL ||
      ev->window != gtk_widget_get_window (widget))
    return FALSE;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

  gint offset = gtk_container_get_border_width (GTK_CONTAINER (widget)) + PADDING + OUTLINE_WIDTH;
  x = ev->x;
  y = ev->y;

  child_glade_widget = glade_widget_get_from_gobject (child);
  gtk_widget_get_allocation (child, &allocation);

  allocation.x += offset;
  allocation.y += offset;
    
  if (priv->activity == ACTIVITY_RESIZE_WIDTH)
    {
      new_width = x - priv->dx - PADDING - OUTLINE_WIDTH;

      if (new_width < priv->current_size_request->width)
        new_width = priv->current_size_request->width;

      allocation.width = new_width;

      glade_design_layout_update_child (GLADE_DESIGN_LAYOUT (widget),
                                        child, &allocation);
    }
  else if (priv->activity == ACTIVITY_RESIZE_HEIGHT)
    {
      new_height = y - priv->dy - PADDING - OUTLINE_WIDTH;

      if (new_height < priv->current_size_request->height)
        new_height = priv->current_size_request->height;

      allocation.height = new_height;

      glade_design_layout_update_child (GLADE_DESIGN_LAYOUT (widget),
                                        child, &allocation);
    }
  else if (priv->activity == ACTIVITY_RESIZE_WIDTH_AND_HEIGHT)
    {
      new_width = x - priv->dx - PADDING - OUTLINE_WIDTH;
      new_height = y - priv->dy - PADDING - OUTLINE_WIDTH;

      if (new_width < priv->current_size_request->width)
        new_width = priv->current_size_request->width;
      if (new_height < priv->current_size_request->height)
        new_height = priv->current_size_request->height;


      allocation.height = new_height;
      allocation.width = new_width;

      glade_design_layout_update_child (GLADE_DESIGN_LAYOUT (widget),
                                        child, &allocation);
    }
  else
    {
      region =
          glade_design_layout_get_pointer_region (GLADE_DESIGN_LAYOUT (widget),
                                                  x, y);

      if (region == REGION_EAST)
        gdk_window_set_cursor (priv->window, priv->cursor_resize_right);

      else if (region == REGION_SOUTH)
        gdk_window_set_cursor (priv->window, priv->cursor_resize_bottom);

      else if (region == REGION_SOUTH_EAST ||
               region == REGION_WEST_OF_SOUTH_EAST ||
               region == REGION_NORTH_OF_SOUTH_EAST)
        gdk_window_set_cursor (priv->window,
                               priv->cursor_resize_bottom_right);

      else if (region == REGION_OUTSIDE)
        gdk_window_set_cursor (priv->window, NULL);
    }

  return FALSE;
}

typedef struct
{
  GtkWidget *toplevel;
  gint x;
  gint y;
  GladeWidget *gwidget;
} GladeFindInContainerData;

static void
glade_design_layout_find_inside_container (GtkWidget * widget,
                                           GladeFindInContainerData * data)
{
  GtkAllocation allocation;
  gint x;
  gint y;

  if (data->gwidget || !gtk_widget_get_mapped (widget))
    return;

  gtk_widget_translate_coordinates (data->toplevel, widget, data->x, data->y, &x, &y);

  gtk_widget_get_allocation (widget, &allocation);

  if (x >= 0 && x < allocation.width && y >= 0 && y < allocation.height)
    {
      if (GTK_IS_CONTAINER (widget))
        gtk_container_forall (GTK_CONTAINER (widget), (GtkCallback)
                              glade_design_layout_find_inside_container,
                              data);

      if (!data->gwidget)
        data->gwidget = glade_widget_get_from_gobject (widget);
    }
}

static gboolean
glade_design_layout_button_press_event (GtkWidget * widget, GdkEventButton * ev)
{
  GtkWidget *child;
  GtkAllocation child_allocation;
  PointerRegion region;
  GladeDesignLayoutPrivate *priv;
  gint x, y;

  if ((child = gtk_bin_get_child (GTK_BIN (widget))) == NULL)
    return FALSE;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

  x = ev->x;
  y = ev->y;

  region =
      glade_design_layout_get_pointer_region (GLADE_DESIGN_LAYOUT (widget), x, y);

  if (((GdkEventButton *) ev)->button == 1)
    {
      gtk_widget_get_allocation (child, &child_allocation);
      priv->dx = x - (child_allocation.x + child_allocation.width);
      priv->dy = y - (child_allocation.y + child_allocation.height);

      if (region == REGION_EAST)
        {
          priv->activity = ACTIVITY_RESIZE_WIDTH;
          gdk_window_set_cursor (priv->window, priv->cursor_resize_right);
        }
      if (region == REGION_SOUTH)
        {
          priv->activity = ACTIVITY_RESIZE_HEIGHT;
          gdk_window_set_cursor (priv->window,
                                 priv->cursor_resize_bottom);
        }
      if (region == REGION_SOUTH_EAST)
        {
          priv->activity = ACTIVITY_RESIZE_WIDTH_AND_HEIGHT;
          gdk_window_set_cursor (priv->window,
                                 priv->cursor_resize_bottom_right);
        }
      if (region == REGION_WEST_OF_SOUTH_EAST)
        {
          priv->activity = ACTIVITY_RESIZE_WIDTH_AND_HEIGHT;
          gdk_window_set_cursor (priv->window,
                                 priv->cursor_resize_bottom_right);
        }
      if (region == REGION_NORTH_OF_SOUTH_EAST)
        {
          priv->activity = ACTIVITY_RESIZE_WIDTH_AND_HEIGHT;
          gdk_window_set_cursor (priv->window,
                                 priv->cursor_resize_bottom_right);
        }
    }

  return FALSE;
}

static gboolean
glade_design_layout_button_release_event (GtkWidget * widget,
                                          GdkEventButton * ev)
{
  GladeDesignLayoutPrivate *priv;
  GtkWidget *child;

  if ((child = gtk_bin_get_child (GTK_BIN (widget))) == NULL)
    return FALSE;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

  priv->activity = ACTIVITY_NONE;
  gdk_window_set_cursor (priv->window, NULL);

  return FALSE;
}

static void
glade_design_layout_get_preferred_height (GtkWidget * widget,
                                          gint * minimum, gint * natural)
{
  GladeDesignLayoutPrivate *priv;
  GtkWidget *child;
  GladeWidget *gchild;
  gint child_height = 0;
  guint border_width = 0;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

  *minimum = 0;
  *natural = 0;

  child = gtk_bin_get_child (GTK_BIN (widget));

  if (child && gtk_widget_get_visible (child))
    {
      gchild = glade_widget_get_from_gobject (child);
      g_assert (gchild);

      gtk_widget_get_preferred_height (child, minimum, natural);

      g_object_get (gchild, "toplevel-height", &child_height, NULL);

      child_height = MAX (child_height, *minimum);

      *minimum = MAX (*minimum, 2 * PADDING + child_height + 2 * OUTLINE_WIDTH);
      *natural = MAX (*natural, 2 * PADDING + child_height + 2 * OUTLINE_WIDTH);

    }

  *minimum += border_width * 2;
  *natural += border_width * 2;
}


static void
glade_design_layout_get_preferred_width (GtkWidget * widget,
                                         gint * minimum, gint * natural)
{
  GladeDesignLayoutPrivate *priv;
  GtkWidget *child;
  GladeWidget *gchild;
  gint child_width = 0;
  guint border_width = 0;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

  *minimum = 0;
  *natural = 0;

  child = gtk_bin_get_child (GTK_BIN (widget));

  if (child && gtk_widget_get_visible (child))
    {
      gchild = glade_widget_get_from_gobject (child);
      g_assert (gchild);

      gtk_widget_get_preferred_width (child, minimum, natural);

      g_object_get (gchild, "toplevel-width", &child_width, NULL);

      child_width = MAX (child_width, *minimum);

      *minimum = MAX (*minimum, 2 * PADDING + child_width + 2 * OUTLINE_WIDTH);
      *natural = MAX (*natural, 2 * PADDING + child_width + 2 * OUTLINE_WIDTH);
    }

  border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
  *minimum += border_width * 2;
  *natural += border_width * 2;
}

static void
glade_design_layout_size_allocate (GtkWidget * widget,
                                   GtkAllocation * allocation)
{
  GtkWidget *child;
        
  gtk_widget_set_allocation (widget, allocation);
    
  if (gtk_widget_get_realized (widget))
    gdk_window_move_resize (gtk_widget_get_window (widget),
                            allocation->x,
                            allocation->y,
                            allocation->width,
                            allocation->height);
    
  child = gtk_bin_get_child (GTK_BIN (widget));

  if (child && gtk_widget_get_visible (child))
    {
      gint border_width, child_width = 0, child_height = 0;
      GladeDesignLayoutPrivate *priv;
      GtkAllocation child_allocation;
      GtkRequisition requisition;
      GladeWidget *gchild;
        
      gchild = glade_widget_get_from_gobject (child);        
      g_assert (gchild);

      g_object_get (gchild,
                    "toplevel-width", &child_width,
                    "toplevel-height", &child_height, NULL);

      priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);
      border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
      gtk_widget_get_preferred_size (child, &requisition, NULL);

      child_allocation.x = allocation->x + border_width + PADDING + OUTLINE_WIDTH;
      child_allocation.y = allocation->y +border_width + PADDING + OUTLINE_WIDTH;
      child_allocation.width = MAX (requisition.width, child_width);
      child_allocation.height = MAX (requisition.height, child_height);

      if (gtk_widget_get_realized (widget))
        gdk_window_move_resize (priv->offscreen_window,
                                child_allocation.x,
                                child_allocation.y,
                                child_allocation.width,
                                child_allocation.height);
      child_allocation.x = child_allocation.y = 0;
      gtk_widget_size_allocate (child, &child_allocation);
    }
}

static void
glade_design_layout_add (GtkContainer * container, GtkWidget * widget)
{
  GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (container);
  GladeDesignLayout *layout = GLADE_DESIGN_LAYOUT (container);

  layout->priv->current_size_request->width = 0;
  layout->priv->current_size_request->height = 0;

  /* XXX I guess we need to queue these up until the design-layout actually gets an allocation */
  gtk_widget_set_parent_window (widget, priv->offscreen_window);

  GTK_CONTAINER_CLASS (glade_design_layout_parent_class)->add (container,
                                                               widget);

  gtk_widget_queue_draw (GTK_WIDGET (container));
}

static void
glade_design_layout_remove (GtkContainer * container, GtkWidget * widget)
{
  GTK_CONTAINER_CLASS (glade_design_layout_parent_class)->remove (container, widget);

  gtk_widget_queue_draw (GTK_WIDGET (container));
}

static void
glade_design_layout_dispose (GObject * object)
{
  GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (object);

  if (priv->cursor_resize_bottom != NULL)
    {
      gdk_cursor_unref (priv->cursor_resize_bottom);
      priv->cursor_resize_bottom = NULL;
    }
  if (priv->cursor_resize_right != NULL)
    {
      gdk_cursor_unref (priv->cursor_resize_right);
      priv->cursor_resize_right = NULL;
    }
  if (priv->cursor_resize_bottom_right != NULL)
    {
      gdk_cursor_unref (priv->cursor_resize_bottom_right);
      priv->cursor_resize_bottom_right = NULL;
    }

  G_OBJECT_CLASS (glade_design_layout_parent_class)->dispose (object);
}

static void
glade_design_layout_finalize (GObject * object)
{
  GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (object);

  g_slice_free (GtkRequisition, priv->current_size_request);

  G_OBJECT_CLASS (glade_design_layout_parent_class)->finalize (object);
}


static gboolean
glade_design_layout_damage (GtkWidget *widget, GdkEventExpose *event)
{
  gdk_window_invalidate_rect (gtk_widget_get_window (widget), NULL, TRUE);
  return TRUE;
}

static inline void
draw_frame (GtkWidget * widget, cairo_t * cr, int x, int y, int w, int h)
{
  cairo_save (cr);
  cairo_set_line_width (cr, OUTLINE_WIDTH);
  cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

  gdk_cairo_set_source_color (cr,
                              &gtk_widget_get_style (widget)->
                              bg[GTK_STATE_SELECTED]);

  /* rectangle */
  cairo_rectangle (cr, x, y, w, h);
  cairo_stroke (cr);
  cairo_restore (cr);
#if 0
  /* shadow effect */
  cairo_set_source_rgba (cr, 0, 0, 0, 0.04);

  cairo_move_to (cr, x + OUTLINE_WIDTH, y + h + OUTLINE_WIDTH);
  cairo_rel_line_to (cr, w, 0);
  cairo_rel_line_to (cr, 0, -h);
  cairo_stroke (cr);
#endif
}

static gboolean
glade_design_layout_draw (GtkWidget * widget, cairo_t * cr)
{
  GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

  if (gtk_cairo_should_draw_window (cr, gtk_widget_get_window (widget)))
    {
      GtkStyle *style;
      GtkWidget *child;
      GdkWindow *window;
      gint border_width;
      gint width, height;

      border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

      window = gtk_widget_get_window (widget);
      style = gtk_widget_get_style (widget);
      width = gtk_widget_get_allocated_width (widget);
      height = gtk_widget_get_allocated_height (widget);

      child = gtk_bin_get_child (GTK_BIN (widget));

      /* draw a white widget background */
      glade_utils_cairo_draw_rectangle (cr,
                                        &style->
                                        base[gtk_widget_get_state (widget)],
                                        TRUE, border_width, border_width,
                                        width - 2 * border_width,
                                        height - 2 * border_width);

      if (child && gtk_widget_get_visible (child))
        {
          GtkAllocation child_allocation;
          gint offset = border_width + PADDING + OUTLINE_WIDTH;

          gtk_widget_get_allocation (child, &child_allocation);

          /* draw frame */
          draw_frame (widget, cr,
                      border_width + PADDING,
                      border_width + PADDING,
                      child_allocation.width + 2 * OUTLINE_WIDTH,
                      child_allocation.height + 2 * OUTLINE_WIDTH);

          gdk_cairo_set_source_window (cr, priv->offscreen_window, offset, offset);
          cairo_rectangle (cr,
                           offset, offset,
                           child_allocation.width,
                           child_allocation.height);
          cairo_fill (cr);

          /* Fixme: this is just a hack to have selection working */
          ensure_project (widget);
          /* Draw selection */
          if (priv->project)
            {
              GList *widgets = glade_project_selection_get (priv->project);

              if (widgets)
                {
                  gtk_widget_get_allocation (widgets->data, &child_allocation);

                  gdk_cairo_set_source_color (cr,
                                              &gtk_widget_get_style (widget)->bg[GTK_STATE_SELECTED]);
                  cairo_rectangle (cr,
                                   child_allocation.x + offset,
                                   child_allocation.y + offset,
                                   child_allocation.width,
                                   child_allocation.height);
                  cairo_clip (cr);
                  cairo_paint_with_alpha (cr, .32);
                }
            }
        }
    }
  else if (gtk_cairo_should_draw_window (cr, priv->offscreen_window))
    {
      GtkWidget *child = gtk_bin_get_child (GTK_BIN (widget));

      gtk_render_background (gtk_widget_get_style_context (widget),
                             cr,
                             0, 0,
                             gdk_window_get_width (priv->offscreen_window),
                             gdk_window_get_height (priv->offscreen_window));

      if (child)
          gtk_container_propagate_draw (GTK_CONTAINER (widget), child, cr);
    }

  return FALSE;
}

static void
to_child (GladeDesignLayout *bin,
          double         widget_x,
          double         widget_y,
          double        *x_out,
          double        *y_out)
{
  gint offset = gtk_container_get_border_width (GTK_CONTAINER (bin)) + PADDING + OUTLINE_WIDTH;
  *x_out = widget_x - offset;
  *y_out = widget_y - offset;
}

static void
to_parent (GladeDesignLayout *bin,
           double         offscreen_x,
           double         offscreen_y,
           double        *x_out,
           double        *y_out)
{
  gint offset = gtk_container_get_border_width (GTK_CONTAINER (bin)) + PADDING + OUTLINE_WIDTH;
  *x_out = offscreen_x + offset;
  *y_out = offscreen_y + offset;
}

static GdkWindow *
pick_offscreen_child (GdkWindow     *offscreen_window,
                      double         widget_x,
                      double         widget_y,
                      GladeDesignLayout *bin)
{
  GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (bin);
  GtkWidget *child = gtk_bin_get_child (GTK_BIN (bin));
/*
  if (priv->project)
    {
      GList *widgets = glade_project_selection_get (priv->project);
      if (widgets && GTK_IS_WIDGET (widgets->data))
            child = GTK_WIDGET (widgets->data);
    }
*/
  if (child && gtk_widget_get_visible (child))
    {
      GtkAllocation child_area;
      double x, y;

      to_child (bin, widget_x, widget_y, &x, &y);
        
      gtk_widget_get_allocation (child, &child_area);

      if (x >= 0 && x < child_area.width && y >= 0 && y < child_area.height)
        return priv->offscreen_window;
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
}

static void
glade_design_layout_unrealize (GtkWidget * widget)
{
  GladeDesignLayoutPrivate *priv;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

  if (priv->offscreen_window)
    {
      gdk_window_set_user_data (priv->offscreen_window, NULL);
      gdk_window_destroy (priv->offscreen_window);
      priv->offscreen_window = NULL;
    }
    
  GTK_WIDGET_CLASS (glade_design_layout_parent_class)->unrealize (widget);

}

static void
glade_design_layout_init (GladeDesignLayout * layout)
{
  GladeDesignLayoutPrivate *priv;

  layout->priv = priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);

  priv->activity = ACTIVITY_NONE;

  priv->current_size_request = g_slice_new0 (GtkRequisition);

  priv->cursor_resize_bottom = gdk_cursor_new (GDK_BOTTOM_SIDE);
  priv->cursor_resize_right = gdk_cursor_new (GDK_RIGHT_SIDE);
  priv->cursor_resize_bottom_right = gdk_cursor_new (GDK_BOTTOM_RIGHT_CORNER);

  priv->new_width = -1;
  priv->new_height = -1;

  priv->project = NULL;
    
  gtk_widget_set_has_window (GTK_WIDGET (layout), TRUE);
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

  object_class->dispose = glade_design_layout_dispose;
  object_class->finalize = glade_design_layout_finalize;

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
  widget_class->size_allocate = glade_design_layout_size_allocate;

  g_signal_override_class_closure (g_signal_lookup ("damage-event", GTK_TYPE_WIDGET),
                                   GLADE_TYPE_DESIGN_LAYOUT,
                                   g_cclosure_new (G_CALLBACK (glade_design_layout_damage),
                                                   NULL, NULL));

  g_type_class_add_private (object_class, sizeof (GladeDesignLayoutPrivate));
}

/* Public API */

GtkWidget *
glade_design_layout_new (void)
{
  return g_object_new (GLADE_TYPE_DESIGN_LAYOUT, NULL);
}

/**
 * glade_design_layout_do_event:
 * @layout:        A #GladeDesignLayout
 * @event:         the #GdkEvent
 *
 * This is called internally by a #GladeWidget recieving an event,
 * it will marshall the event to the proper #GladeWidget according
 * to its position in @layout.
 *
 * Returns: Whether or not the event was handled by the retrieved #GladeWidget
 */
gboolean
glade_design_layout_do_event (GladeDesignLayout * layout, GdkEvent * event)
{
  GladeFindInContainerData data = { 0, };
  GtkWidget *child;

  if ((child = gtk_bin_get_child (GTK_BIN (layout))) == NULL)
    return FALSE;

  data.toplevel = GTK_WIDGET (layout);
  gtk_widget_get_pointer (GTK_WIDGET (layout), &data.x, &data.y);

  glade_design_layout_find_inside_container (child, &data);

  /* Then we try a GladeWidget */
  if (data.gwidget)
      return glade_widget_event (data.gwidget, event);

  return FALSE;
}
