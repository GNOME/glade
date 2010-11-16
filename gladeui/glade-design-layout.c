/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
	WIDGET_EVENT,
	LAST_SIGNAL
};

struct _GladeDesignLayoutPrivate
{
	GdkCursor *cursor_resize_bottom;
	GdkCursor *cursor_resize_right;
	GdkCursor *cursor_resize_bottom_right;

	/* state machine */
	Activity        activity;   /* the current activity */
	GtkRequisition *current_size_request;
	gint dx;                    /* child.width - event.pointer.x   */
	gint dy;                    /* child.height - event.pointer.y  */
	gint new_width;             /* user's new requested width */
	gint new_height;            /* user's new requested height */
};

static guint              glade_design_layout_signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE (GladeDesignLayout, glade_design_layout, GTK_TYPE_EVENT_BOX)

static PointerRegion
glade_design_layout_get_pointer_region (GladeDesignLayout *layout, gint x, gint y)
{
	GladeDesignLayoutPrivate  *priv;
	GtkAllocation              child_allocation;
	PointerRegion              region = REGION_INSIDE;

	priv  = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);

	gtk_widget_get_allocation (layout->child, &child_allocation);

	if ((x >= (child_allocation.x + child_allocation.width)) &&
	    (x < (child_allocation.x + child_allocation.width + OUTLINE_WIDTH)))
	{
		if ((y < (child_allocation.y + child_allocation.height - OUTLINE_WIDTH)) &&
		    (y >= child_allocation.y - OUTLINE_WIDTH))
			region = REGION_EAST;

		else if ((y < (child_allocation.y + child_allocation.height)) &&
			 (y >= (child_allocation.y + child_allocation.height - OUTLINE_WIDTH)))
			region = REGION_NORTH_OF_SOUTH_EAST;

		else if ((y < (child_allocation.y + child_allocation.height + OUTLINE_WIDTH)) &&
			 (y >= (child_allocation.y + child_allocation.height)))
			region = REGION_SOUTH_EAST;
	}
	else if ((y >= (child_allocation.y + child_allocation.height)) &&
		 (y < (child_allocation.y + child_allocation.height + OUTLINE_WIDTH)))
	{
		if ((x < (child_allocation.x + child_allocation.width - OUTLINE_WIDTH)) &&
		    (x >= child_allocation.x - OUTLINE_WIDTH))
			region = REGION_SOUTH;

		else if ((x < (child_allocation.x + child_allocation.width)) &&
			 (x >= (child_allocation.x + child_allocation.width - OUTLINE_WIDTH)))
			region = REGION_WEST_OF_SOUTH_EAST;

		else if ((x < (child_allocation.x + child_allocation.width + OUTLINE_WIDTH)) &&
			 (x >= (child_allocation.x + child_allocation.width)))
			region = REGION_SOUTH_EAST;
	}

	if (x < PADDING || y < PADDING ||
	    x >= (child_allocation.x + child_allocation.width + OUTLINE_WIDTH) ||
	    y >= (child_allocation.y + child_allocation.height + OUTLINE_WIDTH))
		region = REGION_OUTSIDE;

	return region;
}

static gboolean
glade_design_layout_leave_notify_event (GtkWidget *widget, GdkEventCrossing *ev)
{
	GtkWidget *child;
	GladeDesignLayoutPrivate *priv;

	if ((child = GLADE_DESIGN_LAYOUT (widget)->child) == NULL)
		return FALSE;

	priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

	if (priv->activity == ACTIVITY_NONE)
		gdk_window_set_cursor (gtk_widget_get_window (widget), NULL);
	
	return FALSE;
}


static void
glade_design_layout_update_child (GladeDesignLayout *layout, 
				  GtkWidget         *child,
				  GtkAllocation     *allocation)
{
	GladeDesignLayoutPrivate *priv;
	GladeWidget *gchild;

	priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);

	/* Update GladeWidget metadata */
	gchild = glade_widget_get_from_gobject (child);
	g_object_set (gchild, 
		      "toplevel-width", allocation->width,
		      "toplevel-height", allocation->height,
		      NULL);

	gtk_widget_size_allocate (child, allocation);
	gtk_widget_queue_resize (GTK_WIDGET (layout));
}





/* A temp data struct that we use when looking for a widget inside a container
 * we need a struct, because the forall can only pass one pointer
 */
typedef struct {
	gint x;
	gint y;
	gboolean   any;
	GtkWidget *found;
	GtkWidget *toplevel;
} GladeFindInContainerData;

static void
glade_design_layout_find_inside_container (GtkWidget *widget, GladeFindInContainerData *data)
{
	GtkAllocation allocation;
	gint x;
	gint y;

	gtk_widget_translate_coordinates (data->toplevel, widget, data->x, data->y, &x, &y);
	gtk_widget_get_allocation (widget, &allocation);

	if (gtk_widget_get_mapped(widget) &&
	    x >= 0 && x < allocation.width && y >= 0 && y < allocation.height)
	{
		if (glade_widget_get_from_gobject (widget) || data->any)
			data->found = widget;
		else if (GTK_IS_CONTAINER (widget))
		{
			/* Recurse and see if any project objects exist
			 * under this container that is not in the project
			 * (mostly for dialog buttons).
			 */
			GladeFindInContainerData search;
			search.x = data->x;
			search.y = data->y;
			search.toplevel = data->toplevel;
			search.found = NULL;

			gtk_container_forall (GTK_CONTAINER (widget), (GtkCallback)
					      glade_design_layout_find_inside_container, &search);

			data->found = search.found;
		}
	}
}

static GladeWidget *
glade_design_layout_deepest_gwidget_at_position (GtkContainer *toplevel,
						 GtkContainer *container,
						 gint top_x, gint top_y)
{
	GladeFindInContainerData data;
	GladeWidget *ret_widget = NULL;

	data.x = top_x;
	data.y = top_y;
	data.any = FALSE;
	data.toplevel = GTK_WIDGET (toplevel);
	data.found = NULL;

	gtk_container_forall (container, (GtkCallback)
			      glade_design_layout_find_inside_container, &data);

	if (data.found)
	{
		if (GTK_IS_CONTAINER (data.found))
			ret_widget = glade_design_layout_deepest_gwidget_at_position
				(toplevel, GTK_CONTAINER (data.found), top_x, top_y);
		else
			ret_widget = glade_widget_get_from_gobject (data.found);
	}

	if (!ret_widget)
		ret_widget = glade_widget_get_from_gobject (container);

	return ret_widget;
}


static GtkWidget *
glade_design_layout_deepest_widget_at_position (GtkContainer *toplevel,
						GtkContainer *container,
						gint top_x, gint top_y)
{
	GladeFindInContainerData data;
	GtkWidget *ret_widget = NULL;

	data.x = top_x;
	data.y = top_y;
	data.any = TRUE;
	data.toplevel = GTK_WIDGET (toplevel);
	data.found = NULL;

	gtk_container_forall (container, (GtkCallback)
			      glade_design_layout_find_inside_container, &data);

	if (data.found && GTK_IS_CONTAINER (data.found))
		ret_widget = glade_design_layout_deepest_widget_at_position
			(toplevel, GTK_CONTAINER (data.found), top_x, top_y);
	else if (data.found)
		ret_widget = data.found;
	else
		ret_widget = GTK_WIDGET (container);

	return ret_widget;
}

static gboolean
glade_design_layout_motion_notify_event (GtkWidget *widget, GdkEventMotion *ev)
{
	GtkWidget                *child;
	GladeDesignLayoutPrivate *priv;
	GladeWidget              *child_glade_widget;
	PointerRegion        region;
	GtkAllocation             allocation;
	gint                      x, y;
	gint                      new_width, new_height;

	if ((child = GLADE_DESIGN_LAYOUT (widget)->child) == NULL)
		return FALSE;
	
	priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

	gdk_window_get_pointer (gtk_widget_get_window (widget), &x, &y, NULL);
	
	child_glade_widget = glade_widget_get_from_gobject (child);
	gtk_widget_get_allocation (child, &allocation);

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
		new_width =  x - priv->dx - PADDING - OUTLINE_WIDTH;
		new_height = y - priv->dy - PADDING - OUTLINE_WIDTH;
		
		if (new_width < priv->current_size_request->width)
			new_width = priv->current_size_request->width;
		if (new_height < priv->current_size_request->height)
			new_height = priv->current_size_request->height;
		
		
		allocation.height = new_height;
		allocation.width  = new_width;

		glade_design_layout_update_child (GLADE_DESIGN_LAYOUT (widget), 
						  child, &allocation);
	}
	else
	{
		region = glade_design_layout_get_pointer_region (GLADE_DESIGN_LAYOUT (widget), x, y);
		
		if (region == REGION_EAST)
			gdk_window_set_cursor (gtk_widget_get_window (widget), priv->cursor_resize_right);
		
		else if (region == REGION_SOUTH)
			gdk_window_set_cursor (gtk_widget_get_window (widget), priv->cursor_resize_bottom);
		
		else if (region == REGION_SOUTH_EAST ||
			 region == REGION_WEST_OF_SOUTH_EAST ||
			 region == REGION_NORTH_OF_SOUTH_EAST)
			gdk_window_set_cursor (gtk_widget_get_window (widget), priv->cursor_resize_bottom_right);

		else if (region == REGION_OUTSIDE)
			gdk_window_set_cursor (gtk_widget_get_window (widget), NULL);
	}
	
	return FALSE;
}

static gboolean
glade_design_layout_button_press_event (GtkWidget *widget, GdkEventButton *ev)
{
	GtkWidget                *child;
	GtkAllocation             child_allocation;
	PointerRegion             region;
	GladeDesignLayoutPrivate *priv;
	gint                      x, y;

	if ((child = GLADE_DESIGN_LAYOUT (widget)->child) == NULL)
		return FALSE;

	priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

	gdk_window_get_pointer (gtk_widget_get_window (widget), &x, &y, NULL);
	region = glade_design_layout_get_pointer_region (GLADE_DESIGN_LAYOUT (widget), x, y);

	if (((GdkEventButton *) ev)->button == 1) 
	{
		gtk_widget_get_allocation (child, &child_allocation);
		priv->dx = x - (child_allocation.x + child_allocation.width);
		priv->dy = y - (child_allocation.y + child_allocation.height);
		
		if (region == REGION_EAST) 
		{
			priv->activity = ACTIVITY_RESIZE_WIDTH;
			gdk_window_set_cursor (gtk_widget_get_window (widget), priv->cursor_resize_right);
		} 
		if (region == REGION_SOUTH) 
		{
			priv->activity = ACTIVITY_RESIZE_HEIGHT;
			gdk_window_set_cursor (gtk_widget_get_window (widget), priv->cursor_resize_bottom);
		} 
		if (region == REGION_SOUTH_EAST) 
		{
			priv->activity = ACTIVITY_RESIZE_WIDTH_AND_HEIGHT;
			gdk_window_set_cursor (gtk_widget_get_window (widget), priv->cursor_resize_bottom_right);
		} 
		if (region == REGION_WEST_OF_SOUTH_EAST) 
		{
			priv->activity = ACTIVITY_RESIZE_WIDTH_AND_HEIGHT;
			gdk_window_set_cursor (gtk_widget_get_window (widget), priv->cursor_resize_bottom_right);
		} 
		if (region == REGION_NORTH_OF_SOUTH_EAST) 
		{
			priv->activity = ACTIVITY_RESIZE_WIDTH_AND_HEIGHT;
			gdk_window_set_cursor (gtk_widget_get_window (widget), priv->cursor_resize_bottom_right);
		}
	}
		
	return FALSE;
}

static gboolean
glade_design_layout_button_release_event (GtkWidget *widget, GdkEventButton *ev)
{
	GladeDesignLayoutPrivate *priv;
	GtkWidget                *child;

	if ((child = GLADE_DESIGN_LAYOUT (widget)->child) == NULL)
		return FALSE;

	priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

	priv->activity = ACTIVITY_NONE;
	gdk_window_set_cursor (gtk_widget_get_window (widget), NULL);

	return FALSE;
}

static void
glade_design_layout_get_preferred_height (GtkWidget *widget,
                                          gint *minimum,
                                          gint *natural)
{
	GladeDesignLayoutPrivate *priv;
	GtkWidget *child;
	GladeWidget *gchild;
	gint child_height = 0;
	guint border_width = 0;

	priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

	*minimum = 0;
	*natural = 0;	

	child = GLADE_DESIGN_LAYOUT (widget)->child;

	if (child && gtk_widget_get_visible (child))
	{
		gchild = glade_widget_get_from_gobject (child);
		g_assert (gchild);

		gtk_widget_get_preferred_height (child, minimum, natural);

		g_object_get (gchild, 
			      "toplevel-height", &child_height,
			      NULL);
			
		child_height = MAX (child_height, *minimum);

		*minimum = MAX (*minimum,
		               2 * PADDING + child_height + 2 * OUTLINE_WIDTH);
		*natural = MAX (*natural,
		               2 * PADDING + child_height + 2 * OUTLINE_WIDTH);
		
	}

	*minimum += border_width * 2;
	*natural += border_width * 2;
}
                                          

static void
glade_design_layout_get_preferred_width (GtkWidget *widget, 
                                         gint *minimum,
                                         gint *natural)
{
	GladeDesignLayoutPrivate *priv;
	GtkWidget *child;
	GladeWidget *gchild;
	gint child_width = 0;
	guint border_width = 0;

	priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

	*minimum = 0;
	*natural = 0;
	
	child = GLADE_DESIGN_LAYOUT (widget)->child;

	if (child && gtk_widget_get_visible (child))
	{
		gchild = glade_widget_get_from_gobject (child);
		g_assert (gchild);

		gtk_widget_get_preferred_width (child, minimum, natural);

		g_object_get (gchild, 
			      "toplevel-width", &child_width,
			      NULL);
			
		child_width = MAX (child_width, *minimum);

		*minimum = MAX (*minimum,
		               2 * PADDING + child_width + 2 * OUTLINE_WIDTH);
		*natural = MAX (*natural,
		               2 * PADDING + child_width + 2 * OUTLINE_WIDTH);
	}

	border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));
	*minimum += border_width * 2;
	*natural += border_width * 2;
}

static void
glade_design_layout_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GladeDesignLayoutPrivate *priv;
	GladeWidget *gchild;
	GtkRequisition child_requisition;
	GtkAllocation child_allocation, widget_allocation;
	GtkWidget *child;
	gint border_width;
	gint child_width = 0;
	gint child_height = 0;

	priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

	gtk_widget_set_allocation (widget, allocation);
	border_width = gtk_container_get_border_width (GTK_CONTAINER (widget));

	child = GLADE_DESIGN_LAYOUT (widget)->child;
	
	if (child && gtk_widget_get_visible (child))
	{
		gchild = glade_widget_get_from_gobject (child);
		g_assert (gchild);

		gtk_widget_get_requisition (child, &child_requisition);

		g_object_get (gchild, 
			      "toplevel-width", &child_width,
			      "toplevel-height", &child_height,
			      NULL);

		child_width = MAX (child_width, child_requisition.width);
		child_height = MAX (child_height, child_requisition.height);

		gtk_widget_get_allocation (widget, &widget_allocation);
		child_allocation.x = widget_allocation.x + border_width + PADDING + OUTLINE_WIDTH;
		child_allocation.y = widget_allocation.y + border_width + PADDING + OUTLINE_WIDTH;

		child_allocation.width = child_width - 2 * border_width;
		child_allocation.height = child_height - 2 * border_width;

		gdk_window_resize (gtk_widget_get_window (child), child_allocation.width, child_allocation.height);
		
		gtk_widget_size_allocate (child, &child_allocation);
		g_message ("size: %d %d", child_allocation.width, child_allocation.height);
	}
}

static void
offscreen_window_to_parent (GdkWindow     *offscreen_window,
                            double         offscreen_x,
                            double         offscreen_y,
                            double        *parent_x,
                            double        *parent_y,
                            gpointer    data)
{
	*parent_x = offscreen_x;
	*parent_y = offscreen_y;
}

static void
offscreen_window_from_parent (GdkWindow     *window,
                              double         parent_x,
                              double         parent_y,
                              double        *offscreen_x,
                              double        *offscreen_y,
                              gpointer    data)
{
	*offscreen_x = parent_x;
	*offscreen_y = parent_y;
}

static GdkWindow *
pick_offscreen_child (GdkWindow     *offscreen_window,
                      double         widget_x,
                      double         widget_y,
                      GladeDesignLayout *layout)
{
	if (layout->child && gtk_widget_get_visible (layout->child))
	{
		GtkAllocation child_area;
		double x, y;
		
		x = widget_x;
		y = widget_y;
		g_message ("pick_offscreen_child");

		gtk_widget_get_allocation (layout->child, &child_area);
			
		if (x >= 0 && x < child_area.width &&
		    y >= 0 && y < child_area.height)
			return gtk_widget_get_window (layout->child);
	}

	return NULL;
}

static void
child_realize (GtkWidget *widget, GtkWidget *parent)
{
	GdkWindow *window, *new_window;
	GdkWindowAttr attributes;

	window = gtk_widget_get_window (widget);

	attributes.width = gdk_window_get_width (window);
	attributes.height = gdk_window_get_height (window);

	attributes.window_type = GDK_WINDOW_OFFSCREEN;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.event_mask = gdk_window_get_events (window);
	attributes.visual = gtk_widget_get_visual (widget);
	
	new_window = gdk_window_new (NULL, &attributes, GDK_WA_VISUAL);
	gtk_widget_set_window (widget, new_window);

	gdk_window_set_user_data (new_window, parent);

	g_signal_connect (gtk_widget_get_window (parent), "pick-embedded-child",
	                  G_CALLBACK (pick_offscreen_child), GLADE_DESIGN_LAYOUT (parent));
	
	gdk_offscreen_window_set_embedder (gtk_widget_get_window (widget), gtk_widget_get_window (parent));
	g_signal_connect (gtk_widget_get_window (widget), "to-embedder",
	                  G_CALLBACK (offscreen_window_to_parent), NULL);
	g_signal_connect (gtk_widget_get_window (widget), "from-embedder",
	                  G_CALLBACK (offscreen_window_from_parent), NULL);

	gtk_style_set_background (gtk_widget_get_style (widget), new_window, GTK_STATE_NORMAL);
	gdk_window_show (new_window);
}

static void
glade_design_layout_add (GtkContainer *container, GtkWidget *widget)
{
	GladeDesignLayout *layout;
	GtkWidget *parent;

	layout = GLADE_DESIGN_LAYOUT (container);
	parent = GTK_WIDGET (container);

	layout->priv->current_size_request->width = 0;
	layout->priv->current_size_request->height = 0;
	
	g_signal_connect_after (G_OBJECT (widget), "realize", 
			  G_CALLBACK (child_realize),
	                  GTK_WIDGET (container));

//	GTK_CONTAINER_CLASS (glade_design_layout_parent_class)->add (container, widget);
}

static void
glade_design_layout_dispose (GObject *object)
{
	GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (object);

	if (priv->cursor_resize_bottom != NULL) {
		gdk_cursor_unref (priv->cursor_resize_bottom);
		priv->cursor_resize_bottom = NULL;
	}
	if (priv->cursor_resize_right != NULL) {
		gdk_cursor_unref (priv->cursor_resize_right);	
		priv->cursor_resize_right = NULL;
	}
	if (priv->cursor_resize_bottom_right != NULL) {	
		gdk_cursor_unref (priv->cursor_resize_bottom_right);
		priv->cursor_resize_bottom_right = NULL;
	}

	G_OBJECT_CLASS (glade_design_layout_parent_class)->dispose (object);
}

static void
glade_design_layout_finalize (GObject *object)
{
	GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (object);

	g_slice_free (GtkRequisition, priv->current_size_request);
	
	G_OBJECT_CLASS (glade_design_layout_parent_class)->finalize (object);
}

static inline void
draw_frame (GtkWidget *widget,
	    cairo_t   *cr,
	    int x, int y, int w, int h)
{	
	cairo_set_line_width (cr, OUTLINE_WIDTH);
	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);

	gdk_cairo_set_source_color (cr, &gtk_widget_get_style (widget)->bg[GTK_STATE_SELECTED]);

	/* rectangle */
	cairo_move_to (cr, x, y);
	cairo_rel_line_to (cr, 0, h);
	cairo_rel_line_to (cr, w, 0);
	cairo_rel_line_to (cr, 0, -h);
	cairo_close_path (cr);	
	cairo_stroke (cr);
	
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
glade_design_layout_draw (GtkWidget *widget, cairo_t *cr)
{
	GladeDesignLayout *layout = GLADE_DESIGN_LAYOUT (widget);

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
		
		child = layout->child;

		/* draw a white widget background */
		glade_utils_cairo_draw_rectangle (cr,
		                                  &style->base [gtk_widget_get_state (widget)],
		                                  TRUE,
		                                  border_width,
		                                  border_width,
		                                  width - 2 * border_width,
		                                  height - 2 * border_width);

		if (child && gtk_widget_get_visible (child))
		{
			gint x, y, w, h;
			cairo_surface_t *surface = 
				gdk_offscreen_window_get_surface (gtk_widget_get_window (child));
			
			w = gdk_window_get_width (gtk_widget_get_window (child));
			h = gdk_window_get_height (gtk_widget_get_window (child));

			x = border_width;
			y = border_width;

			g_message ("aaaa %d %d %d %d ", x, y, w, h);

			cairo_set_source_surface (cr, surface, 0, 0);
			cairo_rectangle (cr, x + OUTLINE_WIDTH/2, y + OUTLINE_WIDTH/2, w, h);
			cairo_fill (cr);

			w += border_width + OUTLINE_WIDTH;
			h += border_width + OUTLINE_WIDTH;
			
			/* draw frame */
			draw_frame (widget, cr, x, y, w, h);
		}
	}

	return FALSE;
}

/**
 * glade_design_layout_widget_event:
 * @layout:        A #GladeDesignLayout
 * @event_gwidget: the #GladeWidget that recieved event notification
 * @event:         the #GdkEvent
 *
 * This is called internally by a #GladeWidget recieving an event,
 * it will marshall the event to the proper #GladeWidget according
 * to its position in @layout.
 *
 * Returns: Whether or not the event was handled by the retrieved #GladeWidget
 */
gboolean
glade_design_layout_widget_event (GladeDesignLayout *layout,
				  GladeWidget       *event_gwidget,
				  GdkEvent          *event)
{
	GladeWidget *gwidget;
	GtkWidget   *child;
	gboolean     retval;
	gint         x, y;
	
	gtk_widget_get_pointer (GTK_WIDGET (layout), &x, &y);
	gwidget = glade_design_layout_deepest_gwidget_at_position
		(GTK_CONTAINER (layout->child), GTK_CONTAINER (layout->child), x, y);

	child = glade_design_layout_deepest_widget_at_position 
		(GTK_CONTAINER (layout->child), GTK_CONTAINER (layout->child), x, y);

	/* First try a placeholder */
	if (GLADE_IS_PLACEHOLDER (child) && event->type != GDK_EXPOSE)
	{
		retval = gtk_widget_event (child, event);

		if (retval)
			return retval;
	}

	/* Then we try a GladeWidget */
	if (gwidget) 
	{
		g_signal_emit_by_name (layout, "widget-event",
				       gwidget, event, &retval);

		if (retval)
			return retval;
	}

	return FALSE;
}

static gboolean
glade_design_layout_widget_event_impl (GladeProject *project,
				       GladeWidget *gwidget,
				       GdkEvent *event)
{

	if (glade_widget_event (gwidget, event))
		return GLADE_WIDGET_EVENT_STOP_EMISSION |
			GLADE_WIDGET_EVENT_RETURN_TRUE;
	else
		return 0;
}

static void
glade_design_layout_init (GladeDesignLayout *layout)
{
	GladeDesignLayoutPrivate *priv;

	layout->priv = priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);

	gtk_widget_set_has_window (GTK_WIDGET (layout), FALSE);

	priv->activity = ACTIVITY_NONE;

	priv->current_size_request = g_slice_new0 (GtkRequisition);

	priv->cursor_resize_bottom       = gdk_cursor_new (GDK_BOTTOM_SIDE);
	priv->cursor_resize_right        = gdk_cursor_new (GDK_RIGHT_SIDE);
	priv->cursor_resize_bottom_right = gdk_cursor_new (GDK_BOTTOM_RIGHT_CORNER);

	priv->new_width = -1;
	priv->new_height = -1;
}

static gboolean
glade_design_layout_damage (GtkWidget *widget, GdkEventExpose *event)
{

	gdk_window_invalidate_rect (gtk_widget_get_window (widget), NULL, FALSE);

	return TRUE;
}

static void
glade_design_layout_class_init (GladeDesignLayoutClass *klass)
{
	GObjectClass       *object_class;
	GtkWidgetClass     *widget_class;
	GtkContainerClass  *container_class;

	object_class = G_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	container_class = GTK_CONTAINER_CLASS (klass);

	object_class->dispose               = glade_design_layout_dispose;	
	object_class->finalize              = glade_design_layout_finalize;
	
	container_class->add                = glade_design_layout_add;

	widget_class->motion_notify_event   = glade_design_layout_motion_notify_event;
	widget_class->leave_notify_event    = glade_design_layout_leave_notify_event;
	widget_class->button_press_event    = glade_design_layout_button_press_event;
	widget_class->button_release_event  = glade_design_layout_button_release_event;
	widget_class->draw                  = glade_design_layout_draw;
	widget_class->get_preferred_height  = glade_design_layout_get_preferred_height;
	widget_class->get_preferred_width   = glade_design_layout_get_preferred_width;
	widget_class->size_allocate         = glade_design_layout_size_allocate;

	klass->widget_event          = glade_design_layout_widget_event_impl;


	g_signal_override_class_closure (g_signal_lookup ("damage-event", GTK_TYPE_WIDGET),
	                                 GLADE_TYPE_DESIGN_LAYOUT,
	                                 g_cclosure_new (G_CALLBACK (glade_design_layout_damage),
	                                                 NULL, NULL));
	
	/**
	 * GladeDesignLayout::widget-event:
	 * @glade_design_layout: the #GladeDesignLayout containing the #GladeWidget.
	 * @signal_editor: the #GladeWidget which received the signal.
	 * @event: the #GdkEvent
	 *
	 * Emitted when a widget event received.
	 * Connect before the default handler to block glade logic,
	 * such as selecting or resizing, by returning #GLADE_WIDGET_EVENT_STOP_EMISSION.
	 * If you want to handle the event after it passed the glade logic,
	 * but before it reaches the widget, you should connect after the default handler.
	 *
	 * Returns: #GLADE_WIDGET_EVENT_STOP_EMISSION flag to abort futher emission of the signal.
	 * #GLADE_WIDGET_EVENT_RETURN_TRUE flag in the last handler
	 * to stop propagating of the event to the appropriate widget.
	 */
	glade_design_layout_signals[WIDGET_EVENT] =
		g_signal_new ("widget-event",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeDesignLayoutClass, widget_event),
			      glade_integer_handled_accumulator, NULL,
			      glade_marshal_INT__OBJECT_BOXED,
			      G_TYPE_INT,
			      2,
			      GLADE_TYPE_WIDGET,
			      GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

	g_type_class_add_private (object_class, sizeof (GladeDesignLayoutPrivate));
}

GtkWidget*
glade_design_layout_new (void)
{
	return g_object_new (GLADE_TYPE_DESIGN_LAYOUT, NULL);
}

