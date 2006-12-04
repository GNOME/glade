/*
 * glade-design-layout.c
 *
 * Copyright (C) 2006 Vincent Geddes
 *
 * Authors:
 *   Vincent Geddes <vgeddes@metroweb.co.za>
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

#include "glade-design-layout.h"
#include "glade.h"

#include <gtk/gtk.h>

#define GLADE_DESIGN_LAYOUT_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object),  \
					        GLADE_TYPE_DESIGN_LAYOUT,               \
					        GladeDesignLayoutPrivate))

#define OUTLINE_WIDTH 4
#define PADDING 12


typedef enum 
{
	GLADE_ACTIVITY_NONE,
	GLADE_ACTIVITY_RESIZE_WIDTH,
	GLADE_ACTIVITY_RESIZE_HEIGHT,
	GLADE_ACTIVITY_RESIZE_WIDTH_AND_HEIGHT

} GladeActivity;

typedef enum 
{
	GLADE_REGION_EAST = 1,
	GLADE_REGION_SOUTH,
	GLADE_REGION_SOUTH_EAST,
	GLADE_REGION_WEST_OF_SOUTH_EAST,
	GLADE_REGION_NORTH_OF_SOUTH_EAST

} GladePointerRegion;

struct _GladeDesignLayoutPrivate
{
	GdkWindow *event_window;
	
	GdkCursor *cursor_resize_bottom;
	GdkCursor *cursor_resize_right;
	GdkCursor *cursor_resize_bottom_right;

	/* state machine */
	GladeActivity  activity;    /* the current activity */
	GtkRequisition *current_size_request;
	int dx;                    /* child.width - event.pointer.x   */
	int dy;                    /* child.height - event.pointer.y  */
	int new_width;             /* user's new requested width */
	int new_height;            /* user's new requested height */
};

static GtkBinClass *parent_class = NULL;


G_DEFINE_TYPE(GladeDesignLayout, glade_design_layout, GTK_TYPE_BIN)

static GladePointerRegion
glade_design_layout_get_pointer_region (GladeDesignLayout *layout, int x, int y)
{
	GladeDesignLayoutPrivate  *priv;
	GtkAllocation             *child_allocation;
	GladePointerRegion         region = 0;

	priv  = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);

	child_allocation = &GTK_BIN (layout)->child->allocation;

	if ((x >= (child_allocation->x + child_allocation->width)) &&
	    (x < (child_allocation->x + child_allocation->width + OUTLINE_WIDTH)))
	{
		if ((y < (child_allocation->y + child_allocation->height - OUTLINE_WIDTH)) &&
		    (y >= child_allocation->y - OUTLINE_WIDTH))
			region = GLADE_REGION_EAST;

		else if ((y < (child_allocation->y + child_allocation->height)) &&
			 (y >= (child_allocation->y + child_allocation->height - OUTLINE_WIDTH)))
			region = GLADE_REGION_NORTH_OF_SOUTH_EAST;

		else if ((y < (child_allocation->y + child_allocation->height + OUTLINE_WIDTH)) &&
			 (y >= (child_allocation->y + child_allocation->height)))
			region = GLADE_REGION_SOUTH_EAST;
	}
	else if ((y >= (child_allocation->y + child_allocation->height)) &&
	    	 (y < (child_allocation->y + child_allocation->height + OUTLINE_WIDTH)))
	{
		if ((x < (child_allocation->x + child_allocation->width - OUTLINE_WIDTH)) &&
		    (x >= child_allocation->x - OUTLINE_WIDTH))
			region = GLADE_REGION_SOUTH;

		else if ((x < (child_allocation->x + child_allocation->width)) &&
			 (x >= (child_allocation->x + child_allocation->width - OUTLINE_WIDTH)))
			region = GLADE_REGION_WEST_OF_SOUTH_EAST;

		else if ((x < (child_allocation->x + child_allocation->width + OUTLINE_WIDTH)) &&
			 (x >= (child_allocation->x + child_allocation->width)))
			region = GLADE_REGION_SOUTH_EAST;	
	}

	return region;
}


/* this handler ensures that the user cannot
 * resize a widget below it minimum acceptable size */
static void
child_size_request_handler (GtkWidget         *widget,
			    GtkRequisition    *req,
			    GladeDesignLayout *layout)
{
	GladeDesignLayoutPrivate *priv;
	int new_width, new_height;
	int old_width, old_height;
		
	priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);

	priv->current_size_request->width = req->width;
	priv->current_size_request->height = req->height;

	new_width = widget->allocation.width;
	new_height = widget->allocation.height;

	if (req->width > new_width)
		new_width = req->width;

	if (req->height > new_height)
		new_height = req->height;

	if ((new_width != widget->allocation.width) ||
	    (new_height != widget->allocation.height))
	{
		old_width = widget->requisition.width;
		old_height = widget->requisition.height;

		gtk_widget_set_size_request (widget, new_width, new_height);

		if (old_width > new_width)
			widget->requisition.width = old_width;

		if (old_height > new_height)
			widget->requisition.height = old_height;
	}
	
	gtk_widget_queue_draw (GTK_WIDGET (layout));
	
}

static void
glade_design_layout_handle_event (GladeDesignLayout *layout, GdkEvent* ev)
{
	GladeDesignLayoutPrivate *priv;
	GtkWidget     *widget;
	GtkWidget     *child;
	GtkAllocation  allocation;
	GladeWidget *child_glade_widget;
	GladePointerRegion region;


	gint x, y;
	gint new_width, new_height;
	
	priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);
	widget = GTK_WIDGET (layout);
	child = gtk_bin_get_child (GTK_BIN (layout));

	if (child == NULL)
		return;

	switch (ev->type)
	{
	case GDK_MOTION_NOTIFY:

		if (((GdkEventMotion *)ev)->is_hint)
			gdk_window_get_pointer (priv->event_window, &x, &y, NULL);
		else {
			x = (int) ((GdkEventMotion *)ev)->x;
			y = (int) ((GdkEventMotion *)ev)->y;
		}
		
		child_glade_widget = glade_widget_get_from_gobject (child);
		allocation         = child->allocation;

		if (priv->activity == GLADE_ACTIVITY_RESIZE_WIDTH)
		{
			new_width = x - priv->dx - PADDING - OUTLINE_WIDTH;
			
			if (new_width < priv->current_size_request->width)
				new_width = priv->current_size_request->width;

			/* FIXME: use size_allocate and emit a signal, let the
			 * glade core handle widget properties at a more appropriate 
			 * place, also, it might be a little agressive to change property
			 * values automaticly by default, maybe we should check if
			 * "default width" is enabled and set that, else check if
			 * "width request" is enabled, and set that, or not set any 
			 * properties at all.
			 */
/* 			allocation.width = new_width; */
/* 			gtk_widget_size_allocate (child, &allocation); */
				
			
			glade_widget_property_set_enabled (child_glade_widget, "default-width", TRUE);
			glade_widget_property_set (child_glade_widget, "default-width", new_width, NULL);
			
		}
		else if (priv->activity == GLADE_ACTIVITY_RESIZE_HEIGHT)
		{
			new_height = y - priv->dy - PADDING - OUTLINE_WIDTH;
			
			if (new_height < priv->current_size_request->height)
				new_height = priv->current_size_request->height;
				
/* 			allocation.height = new_height; */
/* 			gtk_widget_size_allocate (child, &allocation); */

			glade_widget_property_set_enabled (child_glade_widget, "default-height", TRUE);
			glade_widget_property_set (child_glade_widget, "default-height", new_height, NULL);
					     
		}
		else if (priv->activity == GLADE_ACTIVITY_RESIZE_WIDTH_AND_HEIGHT)
		{
			new_width =  x - priv->dx - PADDING - OUTLINE_WIDTH;
			new_height = y - priv->dy - PADDING - OUTLINE_WIDTH;
			
			if (new_width < priv->current_size_request->width)
				new_width = priv->current_size_request->width;
			if (new_height < priv->current_size_request->height)
				new_height = priv->current_size_request->height;
				

/* 			allocation.height = new_height; */
/* 			allocation.width  = new_width; */
/* 			gtk_widget_size_allocate (child, &allocation); */

			glade_widget_property_set_enabled (child_glade_widget, "default-width", TRUE);
			glade_widget_property_set_enabled (child_glade_widget, "default-height", TRUE);
			glade_widget_property_set (child_glade_widget, "default-width", new_width, NULL);
			glade_widget_property_set (child_glade_widget, "default-height", new_height, NULL);
								     
		}
		else
		{
			region = glade_design_layout_get_pointer_region (layout, x, y);

			if (region == GLADE_REGION_EAST)
				gdk_window_set_cursor (widget->window, priv->cursor_resize_right);

			else if (region == GLADE_REGION_SOUTH)
				gdk_window_set_cursor (widget->window, priv->cursor_resize_bottom);

			else if (region == GLADE_REGION_SOUTH_EAST ||
				 region == GLADE_REGION_WEST_OF_SOUTH_EAST ||
				 region == GLADE_REGION_NORTH_OF_SOUTH_EAST)
				gdk_window_set_cursor (widget->window, priv->cursor_resize_bottom_right);

			else
				gdk_window_set_cursor (widget->window, NULL);			
		}
		break;

	case GDK_BUTTON_PRESS:

		x = (int) ((GdkEventButton *) ev)->x;
		y = (int) ((GdkEventButton *) ev)->y;

		region = glade_design_layout_get_pointer_region (layout, x, y);

		if (((GdkEventButton *) ev)->button == 1) {

			priv->dx = x - (child->allocation.x + child->allocation.width);
			priv->dy = y - (child->allocation.y + child->allocation.height);
		
			if (region == GLADE_REGION_EAST) {
				priv->activity = GLADE_ACTIVITY_RESIZE_WIDTH;

				gdk_window_set_cursor (widget->window, priv->cursor_resize_right);
			} 
			if (region == GLADE_REGION_SOUTH) {
				priv->activity = GLADE_ACTIVITY_RESIZE_HEIGHT;

				gdk_window_set_cursor (widget->window, priv->cursor_resize_bottom);
			} 
			if (region == GLADE_REGION_SOUTH_EAST) {
				priv->activity = GLADE_ACTIVITY_RESIZE_WIDTH_AND_HEIGHT;

				gdk_window_set_cursor (widget->window, priv->cursor_resize_bottom_right);
			} 
			if (region == GLADE_REGION_WEST_OF_SOUTH_EAST) {
				priv->activity = GLADE_ACTIVITY_RESIZE_WIDTH_AND_HEIGHT;

				gdk_window_set_cursor (widget->window, priv->cursor_resize_bottom_right);
			} 
			if (region == GLADE_REGION_NORTH_OF_SOUTH_EAST) {
				priv->activity = GLADE_ACTIVITY_RESIZE_WIDTH_AND_HEIGHT;

				gdk_window_set_cursor (widget->window, priv->cursor_resize_bottom_right);
			}
		}
		break;

	case GDK_BUTTON_RELEASE:
			
		priv->activity = GLADE_ACTIVITY_NONE;
		gdk_window_set_cursor (widget->window, NULL);

		break;

	case GDK_LEAVE_NOTIFY:

		if (priv->activity == GLADE_ACTIVITY_NONE)
			gdk_window_set_cursor (widget->window, NULL);
		break;

	default:
		break;
	}
}

static gboolean
glade_design_layout_leave_notify_event (GtkWidget *widget, GdkEventCrossing *ev)
{
	GtkWidget *child;

	child = ((GtkBin *) widget)->child;
	
	if (child && GTK_IS_WINDOW (child))
		glade_design_layout_handle_event (GLADE_DESIGN_LAYOUT (widget), (GdkEvent *) ev);

	return FALSE;
}

static gboolean
glade_design_layout_motion_notify_event (GtkWidget *widget, GdkEventMotion *ev)
{
	GtkWidget *child;

	child = ((GtkBin *) widget)->child;
	
	if (child && GTK_IS_WINDOW (child))
		glade_design_layout_handle_event (GLADE_DESIGN_LAYOUT (widget), (GdkEvent *) ev);

	return FALSE;
}

static gboolean
glade_design_layout_button_press_event (GtkWidget *widget, GdkEventButton *ev)
{
	GtkWidget *child;

	child = ((GtkBin *) widget)->child;
	
	if (child && GTK_IS_WINDOW (child))
		glade_design_layout_handle_event (GLADE_DESIGN_LAYOUT (widget), (GdkEvent *) ev);

	return FALSE;
}

static gboolean
glade_design_layout_button_release_event (GtkWidget *widget, GdkEventButton *ev)
{
	GtkWidget *child;

	child = ((GtkBin *) widget)->child;
	
	if (child && GTK_IS_WINDOW (child))
		glade_design_layout_handle_event (GLADE_DESIGN_LAYOUT (widget), (GdkEvent *) ev);

	return FALSE;
}

static void
glade_design_layout_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	GladeDesignLayoutPrivate *priv;
	GtkRequisition child_requisition;
	GtkWidget *child;
	gint child_width = 0;
	gint child_height = 0;

	priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

	requisition->width = 0;
	requisition->height = 0;

	child = GTK_BIN (widget)->child;
	if (child && GTK_WIDGET_VISIBLE (child))
	{
		gtk_widget_size_request (child, &child_requisition);

		if (GTK_IS_WINDOW (child))
			gtk_window_get_default_size (GTK_WINDOW (child), &child_width, &child_height);
			
		child_width = MAX (child_width, child_requisition.width);
		child_height = MAX (child_height, child_requisition.height);  

		requisition->width = MAX (requisition->width,
		                          2 * PADDING + child_width + 2 * OUTLINE_WIDTH);

		requisition->height = MAX (requisition->height,
		                          2 * PADDING + child_height + 2 * OUTLINE_WIDTH);
	}

	requisition->width += GTK_CONTAINER (widget)->border_width * 2;
	requisition->height += GTK_CONTAINER (widget)->border_width * 2;

}

static void
glade_design_layout_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GladeDesignLayoutPrivate *priv;
	GtkRequisition child_requisition;
	GtkAllocation child_allocation;
	GtkWidget *child;
	gint border_width;
	gint child_width = 0;
	gint child_height = 0;

	priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

	widget->allocation = *allocation;
	border_width = GTK_CONTAINER (widget)->border_width;

	if (GTK_WIDGET_REALIZED (widget))
	{
		if (priv->event_window)
			gdk_window_move_resize (priv->event_window,
						allocation->x + border_width,
						allocation->y + border_width,
						allocation->width - 2 * border_width ,
						allocation->height - 2 * border_width);

	}

	child = GTK_BIN (widget)->child;
	if (child && GTK_WIDGET_VISIBLE (child))
	{
		gtk_widget_get_child_requisition (child, &child_requisition);

		if (GTK_IS_WINDOW (child))
		{
			gtk_window_get_default_size (GTK_WINDOW (child), &child_width, &child_height);
			
			child_width = MAX (child_width, priv->current_size_request->width);
			child_height = MAX (child_height, priv->current_size_request->height); 
		}
		else
		{
			child_width = child_requisition.width;
			child_height = child_requisition.height;		
		}	

		child_allocation.x = widget->allocation.x + border_width + PADDING + OUTLINE_WIDTH;
		child_allocation.y = widget->allocation.y + border_width + PADDING + OUTLINE_WIDTH;

		child_allocation.width = child_width - 2 * border_width;
		child_allocation.height = child_height - 2 * border_width;

		gtk_widget_size_allocate (child, &child_allocation);
	}
}

static void
glade_design_layout_map (GtkWidget *widget)
{
	GladeDesignLayoutPrivate *priv;

	priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

	if (priv->event_window)
		gdk_window_show (priv->event_window);

	GTK_WIDGET_CLASS (parent_class)->map (widget);

}

static void
glade_design_layout_unmap (GtkWidget *widget)
{
	GladeDesignLayoutPrivate *priv;	

	priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

	GTK_WIDGET_CLASS (parent_class)->unmap (widget);

	if (priv->event_window)
		gdk_window_hide (priv->event_window);
}

static void
glade_design_layout_realize (GtkWidget *widget)
{
	GladeDesignLayoutPrivate *priv;
	GdkWindowAttr attributes;
	gint attributes_mask;
	gint border_width;

	priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	border_width = GTK_CONTAINER (widget)->border_width;

	attributes.x = widget->allocation.x + border_width;
	attributes.y = widget->allocation.y + border_width;
	attributes.width = widget->allocation.width - 2 * border_width;
	attributes.height = widget->allocation.height - 2 * border_width;

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.wclass = GDK_INPUT_ONLY;
	attributes.event_mask = gtk_widget_get_events (widget)
			        | GDK_POINTER_MOTION_MASK
			        | GDK_POINTER_MOTION_HINT_MASK
			        | GDK_BUTTON_PRESS_MASK
		                | GDK_BUTTON_RELEASE_MASK
			        | GDK_EXPOSURE_MASK
			        | GDK_ENTER_NOTIFY_MASK
			        | GDK_LEAVE_NOTIFY_MASK;

	attributes_mask = GDK_WA_X | GDK_WA_Y;

	widget->window = gtk_widget_get_parent_window (widget);
	g_object_ref (widget->window);

	priv->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
					     &attributes, attributes_mask);
	gdk_window_set_user_data (priv->event_window, widget);

	widget->style = gtk_style_attach (widget->style, widget->window);
	gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
glade_design_layout_unrealize (GtkWidget *widget)
{
	GladeDesignLayoutPrivate *priv;

	priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (widget);

	if (priv->event_window)
	{
		gdk_window_set_user_data (priv->event_window, NULL);
		gdk_window_destroy (priv->event_window);
		priv->event_window = NULL;
	}

	GTK_WIDGET_CLASS (parent_class)->unrealize (widget);

}

static void
glade_design_layout_add (GtkContainer *container, GtkWidget *widget)
{
	GladeDesignLayout *layout;

	layout = GLADE_DESIGN_LAYOUT (container);

	layout->priv->current_size_request->width = 0;
	layout->priv->current_size_request->height = 0;

	g_signal_connect (G_OBJECT (widget), "size-request", 
			  G_CALLBACK (child_size_request_handler),
			  container);

	GTK_CONTAINER_CLASS (parent_class)->add (container, widget);
}

static void
glade_design_layout_remove (GtkContainer *container, GtkWidget *widget)
{
	g_signal_handlers_disconnect_by_func(G_OBJECT (widget), 
					     G_CALLBACK (child_size_request_handler), 
					     container);	

	GTK_CONTAINER_CLASS (parent_class)->remove (container, widget);
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

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
glade_design_layout_finalize (GObject *object)
{
	GladeDesignLayoutPrivate *priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (object);

	g_free (priv->current_size_request);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* creates a gc to draw a nice border around the child */
GdkGC*
create_outline_gc (GtkWidget *widget)
{
	GdkGC *gc;
	GdkGCValues values;

	gc = gdk_gc_new (widget->window);
	
	/* we want the light_gc values as a start */
	gdk_gc_copy (gc, widget->style->light_gc[GTK_STATE_SELECTED]);

	values.line_width = OUTLINE_WIDTH;
	gdk_gc_set_values (gc, &values, GDK_GC_LINE_WIDTH);

	return gc;
}


static gboolean
glade_design_layout_expose_event (GtkWidget *widget, GdkEventExpose *ev)
{
	GladeDesignLayout *layout;
	GdkGC *gc;
	GtkWidget *child;
	int x, y, w, h;
	int border_width;

	layout = GLADE_DESIGN_LAYOUT (widget);

	border_width = GTK_CONTAINER (widget)->border_width;

	/* draw a white widget background */
	gdk_draw_rectangle (widget->window,
			    widget->style->base_gc [GTK_WIDGET_STATE (widget)],
			    TRUE,
			    widget->allocation.x + border_width, 
			    widget->allocation.y + border_width,
			    widget->allocation.width - 2 * border_width, 
			    widget->allocation.height - 2 * border_width);

	child = GTK_BIN (widget)->child;

	if (child && GTK_WIDGET_VISIBLE (child))
	{
		x = child->allocation.x - OUTLINE_WIDTH / 2;
		y = child->allocation.y - OUTLINE_WIDTH / 2;
		w = child->allocation.width + OUTLINE_WIDTH;
		h = child->allocation.height + OUTLINE_WIDTH;

		gc = create_outline_gc (widget);

		/* draw outline around child */
		gdk_draw_rectangle (widget->window,
				    gc,
				    FALSE,
				    x, y, w, h);

		/* draw a filled rectangle in case child does not draw 
		 * it's own background (a GTK_WIDGET_NO_WINDOW child). */
		gdk_draw_rectangle (widget->window,
				    widget->style->fg_gc[GTK_STATE_NORMAL],
				    TRUE,
				    x + OUTLINE_WIDTH / 2, y + OUTLINE_WIDTH / 2, 
				    w - OUTLINE_WIDTH, h - OUTLINE_WIDTH);

		g_object_unref (gc);
	}	

	return GTK_WIDGET_CLASS (parent_class)->expose_event (widget, ev);
}

static void
glade_design_layout_init (GladeDesignLayout *layout)
{
	GladeDesignLayoutPrivate *priv;

	layout->priv = priv = GLADE_DESIGN_LAYOUT_GET_PRIVATE (layout);

	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (layout), GTK_NO_WINDOW);

	priv->event_window = NULL;
	priv->activity = GLADE_ACTIVITY_NONE;

	priv->current_size_request = g_new0 (GtkRequisition, 1);

	priv->cursor_resize_bottom       = gdk_cursor_new (GDK_BOTTOM_SIDE);
	priv->cursor_resize_right        = gdk_cursor_new (GDK_RIGHT_SIDE);
	priv->cursor_resize_bottom_right = gdk_cursor_new (GDK_BOTTOM_RIGHT_CORNER);

	priv->new_width = -1;
	priv->new_height = -1;

}

static void
glade_design_layout_class_init (GladeDesignLayoutClass *class)
{
	GObjectClass       *object_class;
	GtkWidgetClass     *widget_class;
	GtkContainerClass  *container_class;

	object_class = G_OBJECT_CLASS (class);
	parent_class = g_type_class_peek_parent (class);
	widget_class = GTK_WIDGET_CLASS (class);
	container_class = GTK_CONTAINER_CLASS (class);

	object_class->dispose               = glade_design_layout_dispose;	
	object_class->finalize              = glade_design_layout_finalize;
	
	container_class->add                = glade_design_layout_add;
	container_class->remove             = glade_design_layout_remove;
	
	widget_class->motion_notify_event   = glade_design_layout_motion_notify_event;
	widget_class->leave_notify_event    = glade_design_layout_leave_notify_event;
	widget_class->button_press_event    = glade_design_layout_button_press_event;
	widget_class->button_release_event  = glade_design_layout_button_release_event;
	widget_class->realize               = glade_design_layout_realize;
	widget_class->unrealize             = glade_design_layout_unrealize;
	widget_class->map                   = glade_design_layout_map;
	widget_class->unmap                 = glade_design_layout_unmap;
	widget_class->expose_event          = glade_design_layout_expose_event;
	widget_class->size_request          = glade_design_layout_size_request;
	widget_class->size_allocate         = glade_design_layout_size_allocate;

	g_type_class_add_private (object_class, sizeof (GladeDesignLayoutPrivate));

}

GtkWidget*
glade_design_layout_new (void)
{
	GladeDesignLayout *layout;

	layout = g_object_new (GLADE_TYPE_DESIGN_LAYOUT, NULL);

	return GTK_WIDGET (layout);
}
