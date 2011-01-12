/*
 * glade-design-layout.c
 *
 * Copyright (C) 2006-2007 Vincent Geddes
 *
 * Authors:
 *   Vincent Geddes <vgeddes@gnome.org>
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
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

enum
{
  DUMMY_SIGNAL,
  LAST_SIGNAL
};

struct _GladeDesignLayoutPrivate
{
  GdkWindow *window, *offscreen_window;

  GList *selection;

  gint child_offset;
  GdkRectangle east, south, south_east;
  GdkCursor *cursors[sizeof (Activity)];

  /* state machine */
  Activity activity;            /* the current activity */
  gint current_width;
  gint current_height;
  gint dx;                      /* child.width - event.pointer.x   */
  gint dy;                      /* child.height - event.pointer.y  */
  gint new_width;               /* user's new requested width */
  gint new_height;              /* user's new requested height */
};

G_DEFINE_TYPE (GladeDesignLayout, glade_design_layout, GTK_TYPE_BIN)

#define RECTANGLE_POINT_IN(rect,x,y) (x >= rect.x && x <= (rect.x + rect.width) && y >= rect.y && y <= (rect.y + rect.height))

static Activity
gdl_get_activity_from_pointer (GladeDesignLayout * layout, gint x, gint y)
{
  GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);

  if (RECTANGLE_POINT_IN (priv->south_east, x, y)) return ACTIVITY_RESIZE_WIDTH_AND_HEIGHT;

  if (RECTANGLE_POINT_IN (priv->east, x, y)) return ACTIVITY_RESIZE_WIDTH;

  if (RECTANGLE_POINT_IN (priv->south, x, y)) return ACTIVITY_RESIZE_HEIGHT;

  return ACTIVITY_NONE;
}

static gboolean
glade_design_layout_leave_notify_event (GtkWidget * widget,
                                        GdkEventCrossing * ev)
{
  GtkWidget *child;
  GladeDesignLayoutPrivate *priv;

  if ((child = gtk_bin_get_child (GTK_BIN (widget))) == NULL ||
      ev->window != gtk_widget_get_window (widget))
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

  gtk_widget_queue_resize (GTK_WIDGET (layout));
}

static gboolean
glade_design_layout_motion_notify_event (GtkWidget * widget,
                                         GdkEventMotion * ev)
{
  GtkWidget *child;
  GladeDesignLayoutPrivate *priv;
  GladeWidget *child_glade_widget;
  GtkAllocation allocation;
  gint x, y, new_width, new_height;

  if ((child = gtk_bin_get_child (GTK_BIN (widget))) == NULL)
    return FALSE;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

  x = ev->x;
  y = ev->y;

  child_glade_widget = glade_widget_get_from_gobject (child);
  gtk_widget_get_allocation (child, &allocation);

  allocation.x += priv->child_offset;
  allocation.y += priv->child_offset;
    
  if (priv->activity == ACTIVITY_RESIZE_WIDTH)
    {
      new_width = x - priv->dx - PADDING - OUTLINE_WIDTH;

      if (new_width < priv->current_width)
        new_width = priv->current_width;

      allocation.width = new_width;
    }
  else if (priv->activity == ACTIVITY_RESIZE_HEIGHT)
    {
      new_height = y - priv->dy - PADDING - OUTLINE_WIDTH;

      if (new_height < priv->current_height)
        new_height = priv->current_height;

      allocation.height = new_height;
    }
  else if (priv->activity == ACTIVITY_RESIZE_WIDTH_AND_HEIGHT)
    {
      new_width = x - priv->dx - PADDING - OUTLINE_WIDTH;
      new_height = y - priv->dy - PADDING - OUTLINE_WIDTH;

      if (new_width < priv->current_width)
        new_width = priv->current_width;
      if (new_height < priv->current_height)
        new_height = priv->current_height;

      allocation.height = new_height;
      allocation.width = new_width;
    }
  else
    {
      Activity activity = gdl_get_activity_from_pointer (GLADE_DESIGN_LAYOUT (widget), x, y);
      gdk_window_set_cursor (priv->window, priv->cursors[activity]);
      return FALSE;
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
glade_design_layout_find_inside_container (GtkWidget * widget,
                                           GladeFindInContainerData * data)
{
  GtkAllocation allocation;
  gint x;
  gint y;

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
glade_design_layout_button_press_event (GtkWidget * widget, GdkEventButton * ev)
{
  GtkWidget *child;
  GtkAllocation child_allocation;
  GladeDesignLayoutPrivate *priv;
  gint x, y;

  if ((child = gtk_bin_get_child (GTK_BIN (widget))) == NULL)
    return FALSE;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

  x = ev->x;
  y = ev->y;


  if (ev->button == 1)
    {
      gtk_widget_get_allocation (child, &child_allocation);
      priv->dx = x - (child_allocation.x + child_allocation.width);
      priv->dy = y - (child_allocation.y + child_allocation.height);

      priv->activity = gdl_get_activity_from_pointer (GLADE_DESIGN_LAYOUT (widget), x, y);
      gdk_window_set_cursor (priv->window, priv->cursors[priv->activity]);
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
  {
    gdk_window_move_resize (gtk_widget_get_window (widget),
                            allocation->x, allocation->y,
                            allocation->width, allocation->height);
  }

  child = gtk_bin_get_child (GTK_BIN (widget));

  if (child && gtk_widget_get_visible (child))
    {
      gint child_width = 0, child_height = 0;
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

      gtk_widget_get_preferred_size (child, &requisition, NULL);

      priv->child_offset = gtk_container_get_border_width (GTK_CONTAINER (widget)) + PADDING + OUTLINE_WIDTH;

      child_allocation.x = child_allocation.y = 0;
      child_allocation.width = MAX (requisition.width, child_width);
      child_allocation.height = MAX (requisition.height, child_height);

      if (gtk_widget_get_realized (widget))
        gdk_window_move_resize (priv->offscreen_window,
                                0, 0,
                                child_allocation.width,
                                child_allocation.height);

      gtk_widget_size_allocate (child, &child_allocation);
    }
}

static void
on_child_size_allocate (GtkWidget *widget, GtkAllocation *allocation, GladeDesignLayout *layout) 
{
  GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);

  /* Update rectangles used to resize the children */
  priv->east.x = allocation->width + priv->child_offset;
  priv->east.y = priv->child_offset;
  priv->east.height = allocation->height;

  priv->south.x = priv->child_offset;
  priv->south.y = allocation->height + priv->child_offset;
  priv->south.width = allocation->width;

  priv->south_east.x = allocation->width;
  priv->south_east.y = allocation->height;
  priv->south_east.width = priv->south_east.height = priv->child_offset * 2;
}

static void
glade_design_layout_add (GtkContainer * container, GtkWidget * widget)
{
  GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (container);
  GladeDesignLayout *layout = GLADE_DESIGN_LAYOUT (container);

  layout->priv->current_width = 0;
  layout->priv->current_height = 0;

  gtk_widget_set_parent_window (widget, priv->offscreen_window);

  GTK_CONTAINER_CLASS (glade_design_layout_parent_class)->add (container,
                                                               widget);

  g_signal_connect (widget, "size-allocate",
                    G_CALLBACK (on_child_size_allocate),
                    GLADE_DESIGN_LAYOUT (container));

  gtk_widget_queue_draw (GTK_WIDGET (container));
}

static void
glade_design_layout_remove (GtkContainer * container, GtkWidget * widget)
{
  g_signal_handlers_disconnect_by_func (widget, on_child_size_allocate,
                                        GLADE_DESIGN_LAYOUT (container));
  GTK_CONTAINER_CLASS (glade_design_layout_parent_class)->remove (container, widget);
  gtk_widget_queue_draw (GTK_WIDGET (container));
}

static void
glade_design_layout_finalize (GObject * object)
{
  /* Free selection list */
  glade_design_layout_selection_set (GLADE_DESIGN_LAYOUT (object), NULL);

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
}

static inline void
draw_selection (cairo_t *cr, GtkWidget *parent, GtkWidget *widget,
                gint offset, gfloat r, gfloat g, gfloat b)
{
  cairo_pattern_t *gradient;
  GtkAllocation alloc;
  gint x, y;

  gtk_widget_get_allocation (widget, &alloc);
  gtk_widget_translate_coordinates (widget, parent, 0, 0, &x, &y);

  x += offset;
  y += offset;

  cairo_rectangle (cr, x, y, alloc.width, alloc.height);
#ifdef USE_LINEAR_GRADIENT
  gradient = cairo_pattern_create_linear (x, y, x+alloc.width, y+alloc.height);
#else
  gradient = cairo_pattern_create_radial (x+alloc.width/2, y+alloc.height/2, MIN (alloc.width, alloc.height)/6,
                                          x+alloc.width/2, y+alloc.height/2, MAX (alloc.width, alloc.height)/2);
#endif
  cairo_pattern_add_color_stop_rgb (gradient, 0, r+.16, g+.16, b+.16);
  cairo_pattern_add_color_stop_rgb (gradient, 1, r, g, b);
  cairo_set_source (cr, gradient);
  cairo_clip (cr);
  cairo_paint_with_alpha (cr, .32);
  cairo_reset_clip (cr);

  cairo_rectangle (cr, x, y, alloc.width, alloc.height);
  cairo_set_source_rgba (cr, r, g, b, .75);
  cairo_stroke (cr);

  cairo_pattern_destroy (gradient);
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
          const GdkColor *color = &gtk_widget_get_style (widget)->bg[GTK_STATE_SELECTED];
          GtkAllocation child_allocation;
          gfloat r, g, b;
          GList *l;

          gtk_widget_get_allocation (child, &child_allocation);

          /* draw frame */
          draw_frame (widget, cr,
                      border_width + PADDING,
                      border_width + PADDING,
                      child_allocation.width + 2 * OUTLINE_WIDTH,
                      child_allocation.height + 2 * OUTLINE_WIDTH);

          /* draw offscreen widgets */
          gdk_cairo_set_source_window (cr, priv->offscreen_window, priv->child_offset, priv->child_offset);
          cairo_rectangle (cr,
                           priv->child_offset, priv->child_offset,
                           child_allocation.width,
                           child_allocation.height);
          cairo_fill (cr);

          /* Draw selection */
          r = color->red/65535.;
          g = color->green/65535.;
          b = color->blue/65535.;
          cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
          cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
          for (l = priv->selection; l; l = g_list_next (l))
            {
              if (child != l->data)
                draw_selection (cr, child, l->data, priv->child_offset, r, g, b);
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

  /* Allocate cursors */
  display = gtk_widget_get_display (widget);
  priv->cursors[ACTIVITY_RESIZE_HEIGHT] = gdk_cursor_new_for_display (display, GDK_BOTTOM_SIDE);
  priv->cursors[ACTIVITY_RESIZE_WIDTH] = gdk_cursor_new_for_display (display, GDK_RIGHT_SIDE);
  priv->cursors[ACTIVITY_RESIZE_WIDTH_AND_HEIGHT] = gdk_cursor_new_for_display (display, GDK_BOTTOM_RIGHT_CORNER);
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
    
  if (priv->cursors[ACTIVITY_RESIZE_HEIGHT])
    {
      gdk_cursor_unref (priv->cursors[ACTIVITY_RESIZE_HEIGHT]);
      priv->cursors[ACTIVITY_RESIZE_HEIGHT] = NULL;
    }
  if (priv->cursors[ACTIVITY_RESIZE_WIDTH])
    {
      gdk_cursor_unref (priv->cursors[ACTIVITY_RESIZE_WIDTH]);
      priv->cursors[ACTIVITY_RESIZE_WIDTH] = NULL;
    }
  if (priv->cursors[ACTIVITY_RESIZE_WIDTH_AND_HEIGHT])
    {
      gdk_cursor_unref (priv->cursors[ACTIVITY_RESIZE_WIDTH_AND_HEIGHT]);
      priv->cursors[ACTIVITY_RESIZE_WIDTH_AND_HEIGHT] = NULL;
    }

  GTK_WIDGET_CLASS (glade_design_layout_parent_class)->unrealize (widget);
}

static void
glade_design_layout_init (GladeDesignLayout * layout)
{
  GladeDesignLayoutPrivate *priv;

  layout->priv = priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);

  priv->activity = ACTIVITY_NONE;

  priv->cursors[ACTIVITY_NONE] = NULL;
  priv->cursors[ACTIVITY_RESIZE_HEIGHT] = NULL;
  priv->cursors[ACTIVITY_RESIZE_WIDTH] = NULL;
  priv->cursors[ACTIVITY_RESIZE_WIDTH_AND_HEIGHT] = NULL;

  priv->new_width = -1;
  priv->new_height = -1;

  priv->selection = NULL;

  /* setup static member of rectangles */
  priv->east.width = PADDING + OUTLINE_WIDTH;
  priv->south.height = PADDING + OUTLINE_WIDTH;
    
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

static void 
on_selected_child_parent_set (GtkWidget *widget,
                              GtkWidget *old_parent,
                              GladeDesignLayout * layout)
{
  GladeDesignLayoutPrivate *priv;
  GladeWidget *layout_gchild, *gtoplevel, *gwidget;
  GtkWidget *child;
    
  if ((child = gtk_bin_get_child (GTK_BIN (layout))) == NULL) return;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);

  layout_gchild = glade_widget_get_from_gobject (G_OBJECT (child));

  if ((gwidget = glade_widget_get_from_gobject (G_OBJECT (widget))) &&
      (gtoplevel = glade_widget_get_toplevel (gwidget)) &&
      gtoplevel != layout_gchild)
    {
      glade_design_layout_selection_set (layout, NULL);
    }
}

/**
 * glade_design_layout_selection_set:
 * @layout: A #GladeDesignLayout
 * @selection: A list of selected widgets.
 *
 * Set the widget selection list or NULL.
 *
 */
void
glade_design_layout_selection_set (GladeDesignLayout * layout, GList *selection)
{
  GladeDesignLayoutPrivate *priv;
  GladeWidget *layout_gchild;
  GtkWidget *child;
  GList *l;

  g_return_if_fail (GLADE_IS_DESIGN_LAYOUT (layout));
    
  if ((child = gtk_bin_get_child (GTK_BIN (layout))) == NULL) return;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);

  /* Disconnect handlers */
  for (l = priv->selection; l; l = g_list_next (l))
    g_signal_handlers_block_by_func (l->data, on_selected_child_parent_set, layout);

  /* Free list */
  g_list_free (priv->selection);
  priv->selection = NULL;

  layout_gchild = glade_widget_get_from_gobject (G_OBJECT (child));

  for (l = selection; l; l = g_list_next (l))
    {
      GladeWidget *gtoplevel, *gwidget;

      if ((gwidget = glade_widget_get_from_gobject (G_OBJECT (l->data))) &&
          (gtoplevel = glade_widget_get_toplevel (gwidget)) &&
          gtoplevel == layout_gchild)
        {
          /* its a descendant, prepend to list */
          priv->selection = g_list_prepend (priv->selection, l->data);

          /* we unset the whole selection list if one of the widgets is 
           * removed or reparented since Glade Project will take care 
           * of update it properly
           */
          g_signal_connect (l->data, "parent-set", G_CALLBACK (on_selected_child_parent_set), layout);
        }
    }

  gtk_widget_queue_draw (GTK_WIDGET (layout));
}

/**
 * glade_design_layout_do_event:
 * @layout: A #GladeDesignLayout
 * @event: an event to process
 *
 * Process events to make widget selection work. This function should be called
 * before the child widget get the event. See gdk_event_handler_set()
 *
 * Returns: true if the event was handled.
 */
gboolean
glade_design_layout_do_event (GladeDesignLayout * layout, GdkEvent * event)
{
  GladeFindInContainerData data = { 0, };
  GladeDesignLayoutPrivate *priv;
  GtkWidget *child;
    
  if ((child = gtk_bin_get_child (GTK_BIN (layout))) == NULL)
    return FALSE;

  priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);

  data.toplevel = GTK_WIDGET (layout);
  gtk_widget_get_pointer (GTK_WIDGET (layout), &data.x, &data.y);

  glade_design_layout_find_inside_container (child, &data);

  /* Try the placeholder first */
  if (data.placeholder && gtk_widget_event (data.placeholder, event)) return TRUE;

  /* Then we try a GladeWidget */
  if (data.gwidget) return glade_widget_event (data.gwidget, event);

  return FALSE;
}
