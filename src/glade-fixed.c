/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-fixed.c - A GladeWidget derivative object wrapper designed to
 *                 handle free-form child placement for containers such as 
 *                 GtkFixed and GtkLayout.
 *
 * Copyright (C) 2006 The GNOME Foundation.
 *
 * Author(s):
 *      Tristan Van Berkom <tvb@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include "glade.h"
#include "glade-accumulators.h"
#include "glade-marshallers.h"
#include "glade-popup.h"
#include "glade-fixed.h"

/* properties */
enum {
	PROP_0,
	PROP_X_PROP,
	PROP_Y_PROP,
	PROP_WIDTH_PROP,
	PROP_HEIGHT_PROP
};

/* signals */
enum {
	CONFIGURE_CHILD,
	CONFIGURE_BEGIN,
	CONFIGURE_END,
	FIXED_SIGNALS
};

typedef struct {
	gulong press_id;
	gulong release_id;
	gulong motion_id;
	gulong enter_id;
} GFSigData;

/* Convenience macros used in pointer events.
 */
#define GLADE_FIXED_CURSOR_TOP(type)                      \
	((type) == GLADE_CURSOR_RESIZE_TOP_RIGHT ||       \
	 (type) == GLADE_CURSOR_RESIZE_TOP_LEFT  ||       \
	 (type) == GLADE_CURSOR_RESIZE_TOP)

#define GLADE_FIXED_CURSOR_BOTTOM(type)                   \
	((type) == GLADE_CURSOR_RESIZE_BOTTOM_RIGHT ||    \
	 (type) == GLADE_CURSOR_RESIZE_BOTTOM_LEFT  ||    \
	 (type) == GLADE_CURSOR_RESIZE_BOTTOM)

#define GLADE_FIXED_CURSOR_RIGHT(type)                    \
	((type) == GLADE_CURSOR_RESIZE_TOP_RIGHT    ||    \
	 (type) == GLADE_CURSOR_RESIZE_BOTTOM_RIGHT ||    \
	 (type) == GLADE_CURSOR_RESIZE_RIGHT)

#define GLADE_FIXED_CURSOR_LEFT(type)                    \
	((type) == GLADE_CURSOR_RESIZE_TOP_LEFT    ||    \
	 (type) == GLADE_CURSOR_RESIZE_BOTTOM_LEFT ||    \
	 (type) == GLADE_CURSOR_RESIZE_LEFT)

#define CHILD_WIDTH_MIN    20
#define CHILD_HEIGHT_MIN   20
#define CHILD_WIDTH_DEF    100
#define CHILD_HEIGHT_DEF   80

#define GRAB_BORDER_WIDTH  10
#define GRAB_CORNER_WIDTH  10

static GObjectClass *parent_class;
static guint         glade_fixed_signals[FIXED_SIGNALS];

/*******************************************************************************
                             Static helper routines
 *******************************************************************************/
static GladeCursorType
glade_fixed_get_operation (GtkWidget       *widget,
			   gint             x,
			   gint             y)
{
	GladeCursorType operation = GLADE_CURSOR_DRAG;

	if (x < GRAB_BORDER_WIDTH) {
		if (y < GRAB_BORDER_WIDTH)
			operation = GLADE_CURSOR_RESIZE_TOP_LEFT;
		else if (y > widget->allocation.height - GRAB_BORDER_WIDTH)
			operation = GLADE_CURSOR_RESIZE_BOTTOM_LEFT;
		else
			operation = GLADE_CURSOR_RESIZE_LEFT;
	} else if (x > widget->allocation.width - GRAB_BORDER_WIDTH) {
		if (y < GRAB_BORDER_WIDTH)
			operation = GLADE_CURSOR_RESIZE_TOP_RIGHT;
		else if (y > widget->allocation.height - GRAB_BORDER_WIDTH)
			operation = GLADE_CURSOR_RESIZE_BOTTOM_RIGHT;
		else
			operation = GLADE_CURSOR_RESIZE_RIGHT;
	} else if (y < GRAB_BORDER_WIDTH) {
		if (x < GRAB_BORDER_WIDTH)
			operation = GLADE_CURSOR_RESIZE_TOP_LEFT;
		else if (x > widget->allocation.width - GRAB_BORDER_WIDTH)
			operation = GLADE_CURSOR_RESIZE_TOP_RIGHT;
		else
			operation = GLADE_CURSOR_RESIZE_TOP;
	} else if (y > widget->allocation.height - GRAB_BORDER_WIDTH) {
		if (x < GRAB_BORDER_WIDTH)
			operation = GLADE_CURSOR_RESIZE_BOTTOM_LEFT;
		else if (x > widget->allocation.width - GRAB_BORDER_WIDTH)
			operation = GLADE_CURSOR_RESIZE_BOTTOM_RIGHT;
		else
			operation = GLADE_CURSOR_RESIZE_BOTTOM;
	}
	return operation;
}

static void
glade_fixed_save_state (GladeFixed  *fixed,
			GladeWidget *child)
{
	gdk_window_get_pointer (GTK_WIDGET 
				(GLADE_WIDGET (fixed)->object)->window, 
				&fixed->pointer_x_origin, &fixed->pointer_y_origin, NULL);

 	glade_widget_pack_property_get (child, fixed->x_prop, &(fixed->child_x_origin));
 	glade_widget_pack_property_get (child, fixed->y_prop, &(fixed->child_y_origin));
 	glade_widget_property_get (child, fixed->width_prop, &(fixed->child_width_origin));
 	glade_widget_property_get (child, fixed->height_prop, &(fixed->child_height_origin));

	fixed->pointer_x_child_origin = 
		fixed->pointer_x_origin - fixed->child_x_origin;
	fixed->pointer_y_child_origin = 
		fixed->pointer_y_origin - fixed->child_y_origin;
}


static void 
glade_fixed_filter_event (GladeFixed *fixed,
			  gint       *x,
			  gint       *y,
			  gint        left,
			  gint        right,
			  gint        top,
			  gint        bottom)
{
	gint cont_width, cont_height;

	g_return_if_fail (x && y);

	cont_width  = GTK_WIDGET (GLADE_WIDGET (fixed)->object)->allocation.width;
	cont_height = GTK_WIDGET (GLADE_WIDGET (fixed)->object)->allocation.height;

	/* Clip out mouse events that are outside the window.
	 */
	if ((left || fixed->operation == GLADE_CURSOR_DRAG) &&
	    *x - fixed->pointer_x_child_origin < 0)
		*x = fixed->pointer_x_child_origin;
	if ((top || fixed->operation == GLADE_CURSOR_DRAG) && 
	    *y - fixed->pointer_y_child_origin < 0)
		*y = fixed->pointer_y_child_origin;
	
	if ((right || fixed->operation == GLADE_CURSOR_DRAG) && 
	    *x + (fixed->child_width_origin - 
			  fixed->pointer_x_child_origin) > cont_width)
		*x = cont_width - (fixed->child_width_origin - 
				  fixed->pointer_x_child_origin);
	if ((bottom || fixed->operation == GLADE_CURSOR_DRAG) && 
	    *y + (fixed->child_height_origin - 
			   fixed->pointer_y_child_origin) > cont_height)
		*y = cont_height - (fixed->child_height_origin - 
				   fixed->pointer_y_child_origin);
	
	/* Clip out mouse events that mean shrinking to less than 0.
	 */
	if (left && 
	    (*x - fixed->pointer_x_child_origin) > 
	    (fixed->child_x_origin + (fixed->child_width_origin - CHILD_WIDTH_MIN))) {
		*x = (fixed->child_x_origin + (fixed->child_width_origin - CHILD_WIDTH_MIN)) -
			fixed->pointer_x_child_origin;
	    } else if (right &&
		       (*x - fixed->pointer_x_child_origin) < 
		       fixed->child_x_origin - (fixed->child_width_origin + CHILD_WIDTH_MIN)) {
		*x = (fixed->child_x_origin - (fixed->child_width_origin + CHILD_WIDTH_MIN)) +
			fixed->pointer_x_child_origin;
	}


	if (top && 
	    (*y - fixed->pointer_y_child_origin) > 
	    (fixed->child_y_origin + (fixed->child_height_origin - CHILD_HEIGHT_MIN))) {
		*y = (fixed->child_y_origin + (fixed->child_height_origin - CHILD_HEIGHT_MIN)) - 
			fixed->pointer_y_child_origin;
	} else if (bottom &&
		   (*y - fixed->pointer_y_child_origin) < 
		   fixed->child_y_origin - (fixed->child_height_origin + CHILD_HEIGHT_MIN)) {
		*y = (fixed->child_y_origin - (fixed->child_height_origin + CHILD_HEIGHT_MIN)) + 
			fixed->pointer_y_child_origin;
	}
}

static void
glade_fixed_configure_widget (GladeFixed   *fixed,
			      GladeWidget  *child)
{
	GladeWidget    *gwidget = GLADE_WIDGET (fixed);
	GdkRectangle    new_area;
	gboolean        handled, right, left, top, bottom;
	gint            x, y;

	gdk_window_get_pointer
		(GTK_WIDGET (gwidget->object)->window, &x, &y, NULL);

	/* I think its safe here to skip the glade-property API */
	new_area.x      = GTK_WIDGET (child->object)->allocation.x;
	new_area.y      = GTK_WIDGET (child->object)->allocation.y;
	new_area.width  = GTK_WIDGET (child->object)->allocation.width;
	new_area.height = GTK_WIDGET (child->object)->allocation.height;

	right  = GLADE_FIXED_CURSOR_RIGHT  (fixed->operation);
	left   = GLADE_FIXED_CURSOR_LEFT   (fixed->operation);
	top    = GLADE_FIXED_CURSOR_TOP    (fixed->operation);
	bottom = GLADE_FIXED_CURSOR_BOTTOM (fixed->operation);

	/* Filter out events that make your widget go out of bounds */
	glade_fixed_filter_event (fixed, &x, &y, left, right, top, bottom);

	/* Modify current size.
	 */
	if (fixed->operation == GLADE_CURSOR_DRAG)
	{
		/* Move widget */
		new_area.x = fixed->child_x_origin + 
			x - fixed->pointer_x_origin;
		new_area.y = fixed->child_y_origin + 
			y - fixed->pointer_y_origin;
	} else {

		if (bottom) 
		{
			new_area.height =
				fixed->child_height_origin +
				(y - fixed->pointer_y_origin);
		} else if (top)
		{

			new_area.height =
				fixed->child_height_origin -
				(y - fixed->pointer_y_origin);
			new_area.y =
				fixed->child_y_origin +
				(y - fixed->pointer_y_origin);
		}

		if (right) 
		{
			new_area.width =
				fixed->child_width_origin +
				(x - fixed->pointer_x_origin);
		} else if (left)
		{
			new_area.width =
				fixed->child_width_origin -
				(x - fixed->pointer_x_origin);
			new_area.x =
				fixed->child_x_origin + 
				(x - fixed->pointer_x_origin);
		}
	}

	/* Trim */
	if (new_area.width < CHILD_WIDTH_MIN)
		new_area.width = CHILD_WIDTH_MIN;
	if (new_area.height < CHILD_WIDTH_MIN)
		new_area.height = CHILD_HEIGHT_MIN;

	/* Apply new rectangle to the object */
	g_signal_emit (G_OBJECT (fixed), 
		       glade_fixed_signals[CONFIGURE_CHILD],
		       0, child, &new_area, &handled);
	
	/* Correct glitches when some widgets are draged over others */
	gtk_widget_queue_draw (GTK_WIDGET (GLADE_WIDGET (fixed)->object));
}

static void
glade_fixed_disconnect_child (GladeFixed   *fixed,
			      GladeWidget  *child)
{
	GFSigData *data;

	if ((data = g_object_get_data (G_OBJECT (child), "glade-fixed-signal-data")) != NULL)
	{
		g_signal_handler_disconnect (child->object, data->press_id);
		g_signal_handler_disconnect (child->object, data->release_id);
		g_signal_handler_disconnect (child->object, data->enter_id);
		g_signal_handler_disconnect (child->object, data->motion_id);

		g_object_set_data (G_OBJECT (child), "glade-fixed-signal-data", NULL);
	}
}

static void
glade_fixed_connect_child (GladeFixed   *fixed,
			   GladeWidget  *child)
{
	GFSigData *data;

	if ((data = g_object_get_data (G_OBJECT (child), "glade-fixed-signal-data")) != NULL)
		glade_fixed_disconnect_child (fixed, child);
	
	data = g_new (GFSigData, 1);

	data->press_id =
		g_signal_connect
		(child->object, "button-press-event", G_CALLBACK
		 (GLADE_FIXED_GET_CLASS(fixed)->child_event), fixed);
	data->release_id =
		g_signal_connect
		(child->object, "button-release-event", G_CALLBACK
		 (GLADE_FIXED_GET_CLASS(fixed)->child_event), fixed);
	data->enter_id =
		g_signal_connect
		(child->object, "enter-notify-event", G_CALLBACK
		 (GLADE_FIXED_GET_CLASS(fixed)->child_event), fixed);
	data->motion_id = 
		g_signal_connect
		(child->object, "motion-notify-event", G_CALLBACK
		 (GLADE_FIXED_GET_CLASS(fixed)->child_event), fixed);

	g_object_set_data_full (G_OBJECT (child), "glade-fixed-signal-data", 
				data, g_free);
}


/*******************************************************************************
                               GladeFixedClass
 *******************************************************************************/
static gboolean
glade_fixed_configure_child_impl (GladeFixed   *fixed,
				  GladeWidget  *child,
				  GdkRectangle *rect)
{
 	glade_widget_pack_property_set (child, fixed->x_prop, rect->x);
 	glade_widget_pack_property_set (child, fixed->y_prop, rect->y);
 	glade_widget_property_set (child, fixed->width_prop, rect->width);
 	glade_widget_property_set (child, fixed->height_prop, rect->height);
	return TRUE;
}


static void
glade_fixed_configure_end_impl (GladeFixed  *fixed,
				GladeWidget *child)
{
	GValue x_value = { 0, };
	GValue y_value = { 0, };
	GValue width_value = { 0, };
	GValue height_value = { 0, };
	GValue new_x_value = { 0, };
	GValue new_y_value = { 0, };
	GValue new_width_value = { 0, };
	GValue new_height_value = { 0, };
	GladeProperty *x_prop, *y_prop, *width_prop, *height_prop;

	/* XXX Well... this can be simplified now ... heh */
	x_prop      = glade_widget_get_pack_property (child, fixed->x_prop);
	y_prop      = glade_widget_get_pack_property (child, fixed->y_prop);
	width_prop  = glade_widget_get_property      (child, fixed->width_prop);
	height_prop = glade_widget_get_property      (child, fixed->height_prop);

	g_return_if_fail (GLADE_IS_PROPERTY (x_prop));
	g_return_if_fail (GLADE_IS_PROPERTY (y_prop));
	g_return_if_fail (GLADE_IS_PROPERTY (width_prop));
	g_return_if_fail (GLADE_IS_PROPERTY (height_prop));

	g_value_init (&x_value, G_TYPE_INT);
	g_value_init (&y_value, G_TYPE_INT);
	g_value_init (&width_value, G_TYPE_INT);
	g_value_init (&height_value, G_TYPE_INT);

	glade_property_get_value (x_prop, &new_x_value);
	glade_property_get_value (y_prop, &new_y_value);
	glade_property_get_value (width_prop, &new_width_value);
	glade_property_get_value (height_prop, &new_height_value);

	g_value_set_int (&x_value, fixed->child_x_origin);
	g_value_set_int (&y_value, fixed->child_y_origin);
	g_value_set_int (&width_value, fixed->child_width_origin);
	g_value_set_int (&height_value, fixed->child_height_origin);

	/* whew, all that for this call !
	 */
	glade_command_set_properties (x_prop, &x_value, &new_x_value,
				      y_prop, &y_value, &new_y_value,
				      width_prop, &width_value, &new_width_value,
				      height_prop, &height_value, &new_height_value,
				      NULL);
	g_value_unset (&x_value);
	g_value_unset (&y_value);
	g_value_unset (&width_value);
	g_value_unset (&height_value);
	g_value_unset (&new_x_value);
	g_value_unset (&new_y_value);
	g_value_unset (&new_width_value);
	g_value_unset (&new_height_value);
}

static gboolean
glade_fixed_handle_child_event (GladeFixed  *fixed,
				GladeWidget *child,
				GdkEvent    *event)
{
	GladeWidget *gwidget = GLADE_WIDGET (fixed);
	gboolean handled = FALSE;
	gint parent_x, parent_y, child_x, child_y, x, y;
	GladeCursorType operation;

	gdk_window_get_pointer (GTK_WIDGET (gwidget->object)->window, 
				&parent_x, &parent_y, NULL);

 	glade_widget_pack_property_get (child, fixed->x_prop, &child_x);
 	glade_widget_pack_property_get (child, fixed->y_prop, &child_y);

	x = parent_x - child_x;
	y = parent_y - child_y;

	operation = glade_fixed_get_operation (GTK_WIDGET (child->object), x, y);

	switch (event->type)
	{
	case GDK_ENTER_NOTIFY:
	case GDK_MOTION_NOTIFY:
		if (fixed->configuring == NULL)
		{
			/* GTK_NO_WINDOW widgets still have a pointer to the parent window */
			glade_cursor_set (((GdkEventAny *)event)->window, 
					  operation);
		} else if (event->type == GDK_MOTION_NOTIFY) 
		{
			glade_fixed_configure_widget (fixed, child);
			handled = TRUE;
		}
		gdk_window_get_pointer (GTK_WIDGET (child->object)->window, NULL, NULL, NULL);
		break;
	case GDK_BUTTON_PRESS:
		if (((GdkEventButton *)event)->button == 1)
		{
			fixed->configuring = child;
			/* Save widget allocation and pointer pos */
			glade_fixed_save_state (fixed, child);
			
			fixed->operation = operation;
			glade_cursor_set (((GdkEventAny *)event)->window, fixed->operation);

			g_signal_emit (G_OBJECT (fixed),
				       glade_fixed_signals[CONFIGURE_BEGIN],
				       0, child);

			handled = TRUE;
		}
		break;
	case GDK_BUTTON_RELEASE:
		if (((GdkEventButton *)event)->button == 1 && 
		    fixed->configuring)
		{
			// cancel drag stuff
			fixed->operation = operation;
			glade_cursor_set (((GdkEventAny *)event)->window, fixed->operation);

			g_signal_emit (G_OBJECT (fixed),
				       glade_fixed_signals[CONFIGURE_END],
				       0, child);

			fixed->configuring = NULL;
			handled = TRUE;
		}
		break;
	default:
		g_debug ("Unhandled event");
		break;
	}
	return handled;
}

static gint
glade_fixed_child_event (GtkWidget   *widget, 
			 GdkEvent    *event,
			 GladeFixed  *fixed)
{
	GladeWidget *gwidget =
		glade_widget_get_from_gobject (widget);
	return glade_fixed_handle_child_event (fixed, gwidget, event);
}

/*******************************************************************************
                               GladeWidgetClass
 *******************************************************************************/
static void
glade_fixed_add_child_impl (GladeWidget *gwidget_fixed,
			    GladeWidget *child,
			    gboolean     at_mouse)
{
	GladeFixed   *fixed = GLADE_FIXED (gwidget_fixed);
	GdkRectangle  rect;
	gboolean      handled;

	g_return_if_fail (GLADE_IS_FIXED (fixed));
	g_return_if_fail (GLADE_IS_WIDGET (child));

	/* Chain up for the basic parenting */
	GLADE_WIDGET_KLASS (parent_class)->add_child
		(GLADE_WIDGET (fixed), child, at_mouse);

	/* Could be a delagate object that is not a widget or a special
	 * relationship like menushell->menuitem
	 */
	if (!glade_util_gtkcontainer_relation (GLADE_WIDGET (fixed), child))
		return;
	
	gtk_widget_add_events (GTK_WIDGET (child->object),
			       GDK_POINTER_MOTION_MASK      |
			       GDK_POINTER_MOTION_HINT_MASK |
			       GDK_BUTTON_PRESS_MASK        |
			       GDK_BUTTON_RELEASE_MASK      |
			       GDK_ENTER_NOTIFY_MASK);

	glade_fixed_connect_child (fixed, child);

	/* Make sure we can modify these properties */
	glade_widget_pack_property_set_enabled (child, fixed->x_prop, TRUE);
	glade_widget_pack_property_set_enabled (child, fixed->y_prop, TRUE);
	glade_widget_property_set_enabled (child, fixed->width_prop, TRUE);
	glade_widget_property_set_enabled (child, fixed->height_prop, TRUE);

	/* Setup rect and send configure
	 */
	if (fixed->creating)
	{
		rect.x      = fixed->create_x;
		rect.y      = fixed->create_y;
		rect.width  = CHILD_WIDTH_DEF;
		rect.height = CHILD_HEIGHT_DEF;

		g_signal_emit (G_OBJECT (fixed),
			       glade_fixed_signals[CONFIGURE_CHILD],
			       0, child, &rect, &handled);
	} 
	else if (at_mouse)
	{
		rect.x      = fixed->mouse_x;
		rect.y      = fixed->mouse_y;

		glade_widget_property_get (child, fixed->width_prop, &rect.width);
		glade_widget_property_get (child, fixed->height_prop, &rect.height);

		if (rect.width <= 0)
			rect.width  = CHILD_WIDTH_DEF;

		if (rect.height <= 0)
			rect.height = CHILD_HEIGHT_DEF;

		g_signal_emit (G_OBJECT (fixed),
			       glade_fixed_signals[CONFIGURE_CHILD],
			       0, child, &rect, &handled);
	}
	return;
}

static gboolean
glade_fixed_popup_menu (GtkWidget *widget, gpointer unused_data)
{
	GladeWidget *glade_widget;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	glade_widget = glade_widget_get_from_gobject (widget);
	glade_popup_widget_pop (glade_widget, NULL, TRUE);

	return TRUE;
}

static void
glade_fixed_setup_events (GladeWidget *gwidget,
			  GtkWidget   *widget)
{
	gtk_widget_add_events (widget,
			       GDK_POINTER_MOTION_MASK      |
			       GDK_POINTER_MOTION_HINT_MASK |
			       GDK_BUTTON_PRESS_MASK        |
			       GDK_BUTTON_RELEASE_MASK      |
			       GDK_ENTER_NOTIFY_MASK);

	g_signal_connect (G_OBJECT (widget), "popup_menu",
			  G_CALLBACK (glade_fixed_popup_menu), NULL);
}

static gint
glade_fixed_event (GtkWidget   *widget, 
		   GdkEvent    *event,
		   GladeWidget *gwidget_fixed)
{
	GladeFixed       *fixed = GLADE_FIXED (gwidget_fixed);
	GladeWidgetClass *add_class, *alt_class;
	GtkWidget        *event_widget;
	gboolean          handled = FALSE;
	GladeWidget      *glade_fixed_widget, *gwidget, *search;
	gdouble           x, y;

	add_class = glade_app_get_add_class ();
	alt_class = glade_app_get_alt_class ();

	gdk_window_get_pointer (widget->window, NULL, NULL, NULL);

	/* Currently ignoring the return value and acting... even if 
	 * this was a selection click
	 */
	if (GLADE_WIDGET_KLASS (parent_class)->event (widget, event, gwidget_fixed))
		return TRUE;

	gdk_window_get_user_data (((GdkEventAny *)event)->window, (gpointer)&event_widget);

	g_assert (GTK_IS_WIDGET (event_widget));

	glade_fixed_widget = glade_widget_get_from_gobject (widget);
	gdk_event_get_coords (event, &x, &y);
	gwidget    =
		GLADE_WIDGET_GET_KLASS (fixed)->retrieve_from_position
		(event_widget, (int) (x + 0.5), (int) (y + 0.5));

	g_return_val_if_fail (GLADE_IS_WIDGET (gwidget), FALSE);
	g_return_val_if_fail (GLADE_IS_WIDGET (glade_fixed_widget), FALSE);

	/* make sure to grab focus, since we may stop default handlers */
	if (GTK_WIDGET_CAN_FOCUS (widget) && !GTK_WIDGET_HAS_FOCUS (widget))
		gtk_widget_grab_focus (widget);

	switch (event->type)
	{

	case GDK_BUTTON_PRESS:
	case GDK_ENTER_NOTIFY:
	case GDK_MOTION_NOTIFY:
	case GDK_BUTTON_RELEASE:

		/* Get the gwidget that is a direct child of 'fixed' */
		for (search = gwidget; 
		     search && search->parent != GLADE_WIDGET (fixed);
		     search = search->parent);

		if (glade_fixed_widget == gwidget) 
		{
			fixed->mouse_x = ((GdkEventButton *)event)->x;
			fixed->mouse_y = ((GdkEventButton *)event)->y;
		}
	
		if (fixed->configuring)
		{
			return glade_fixed_handle_child_event (fixed, fixed->configuring, event);
		} 
		else if (glade_fixed_widget != gwidget)
		{
			if (search)
				return glade_fixed_handle_child_event (fixed, search, event);
		}
		break;
	default:
		break;
	}

	switch (event->type)
	{
	case GDK_BUTTON_PRESS: // add widget
		if (((GdkEventButton *)event)->button == 1)
		{

			if ((add_class != NULL)        ||
				 ((((GdkEventButton *)event)->state & GDK_SHIFT_MASK) &&
				  alt_class != NULL))
			{
				/* A widget type is selected in the palette.
				 * Add a new widget of that type.
				 */
				fixed->creating = TRUE;
				gdk_window_get_pointer (GTK_WIDGET 
							(GLADE_WIDGET (fixed)->object)->window, 
							&fixed->create_x, &fixed->create_y, NULL);
				glade_command_create
					(add_class ? add_class : alt_class, 
					 GLADE_WIDGET (fixed), NULL, 
					 GLADE_WIDGET (fixed)->project);
				fixed->creating = FALSE;
				
				/* reset the palette */
				glade_palette_unselect_widget 
					(glade_app_get_palette ());
				handled = TRUE;
			}
		}
		break;
	case GDK_ENTER_NOTIFY:
	case GDK_MOTION_NOTIFY:
		if (glade_app_get_add_class ())
			glade_cursor_set (((GdkEventAny *)event)->window, 
					  GLADE_CURSOR_ADD_WIDGET);
		else
			glade_cursor_set (((GdkEventAny *)event)->window, 
					  GLADE_CURSOR_SELECTOR);
		handled = TRUE;
		break;
	default:
		break;
	}
	return handled;
}

/*******************************************************************************
                                   GObjectClass
 *******************************************************************************/
static GObject *
glade_fixed_constructor (GType                  type,
			 guint                  n_construct_properties,
			 GObjectConstructParam *construct_properties)
{
	GObject          *obj;
	GladeWidget      *gwidget;

	obj = G_OBJECT_CLASS (parent_class)->constructor
		(type, n_construct_properties, construct_properties);

	gwidget = GLADE_WIDGET (obj);

	/* This is needed at least to set a backing pixmaps
	 * and handle events more consistantly (lets hope all
	 * free-form containers support being created this way).
	 */
	GTK_WIDGET_UNSET_FLAGS(gwidget->object, GTK_NO_WINDOW);

	return obj;
}

static void
glade_fixed_finalize (GObject *object)
{
	GladeFixed *fixed = GLADE_FIXED (object);

	/* A GladeFixed should be finalized as a result of its
	 * GtkContainer being destroyed, so we shouldn't need to bother
	 * about disconnecting all the child signals.
	 */
	g_free (fixed->x_prop);
	g_free (fixed->y_prop);
	g_free (fixed->width_prop);
	g_free (fixed->height_prop);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
glade_fixed_set_property (GObject      *object,
			  guint         prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
	GladeFixed *fixed = GLADE_FIXED (object);

	switch (prop_id)
	{
	case PROP_X_PROP:
		g_free (fixed->x_prop);
		fixed->x_prop = g_value_dup_string (value);
		break;
	case PROP_Y_PROP:
		g_free (fixed->y_prop);
		fixed->y_prop = g_value_dup_string (value);
		break;
	case PROP_WIDTH_PROP:
		g_free (fixed->width_prop);
		fixed->width_prop = g_value_dup_string (value);
		break;
	case PROP_HEIGHT_PROP:
		g_free (fixed->height_prop);
		fixed->height_prop = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_fixed_get_property (GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
	GladeFixed *fixed = GLADE_FIXED (object);

	switch (prop_id)
	{
	case PROP_X_PROP:       g_value_set_string (value, fixed->x_prop);        break;
	case PROP_Y_PROP:       g_value_set_string (value, fixed->y_prop);        break;
	case PROP_WIDTH_PROP:   g_value_set_string (value, fixed->width_prop);    break;
	case PROP_HEIGHT_PROP:  g_value_set_string (value, fixed->height_prop);   break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_fixed_init (GladeFixed *fixed)
{
	/* Set defaults */
	fixed->x_prop      = g_strdup ("x");
	fixed->y_prop      = g_strdup ("y");
	fixed->width_prop  = g_strdup ("width");
	fixed->height_prop = g_strdup ("height");
}

static void
glade_fixed_class_init (GladeFixedClass *fixed_class)
{
	GObjectClass     *gobject_class = G_OBJECT_CLASS (fixed_class);
	GladeWidgetKlass *gwidget_class = GLADE_WIDGET_KLASS (fixed_class);

	parent_class = 
		G_OBJECT_CLASS
		(g_type_class_peek_parent (gobject_class));

	gobject_class->constructor      = glade_fixed_constructor;
	gobject_class->finalize         = glade_fixed_finalize;
	gobject_class->set_property     = glade_fixed_set_property;
	gobject_class->get_property     = glade_fixed_get_property;
	
	gwidget_class->setup_events     = glade_fixed_setup_events;
	gwidget_class->event            = glade_fixed_event;
	gwidget_class->add_child        = glade_fixed_add_child_impl;

	fixed_class->configure_child  = glade_fixed_configure_child_impl;
	fixed_class->configure_begin  = NULL;
	fixed_class->configure_end    = glade_fixed_configure_end_impl;
	fixed_class->child_event      = glade_fixed_child_event;

	/* Properties */
	g_object_class_install_property 
		(gobject_class, PROP_X_PROP,
		 g_param_spec_string 
		 ("x_prop", _("X position property"),
		  _("The property used to set the X position of a child object"),
		  "x", G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property 
		(gobject_class, PROP_Y_PROP,
		 g_param_spec_string 
		 ("y_prop", _("Y position property"),
		  _("The property used to set the Y position of a child object"),
		  "y", G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property 
		(gobject_class, PROP_WIDTH_PROP,
		 g_param_spec_string 
		 ("width_prop", _("Width property"),
		  _("The property used to set the width of a child object"),
		  "width-request", G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property 
		(gobject_class, PROP_HEIGHT_PROP,
		 g_param_spec_string 
		 ("height_prop", _("Height property"),
		  _("The property used to set the height of a child object"),
		  "height-request", G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/**
	 * GladeFixed::configure-child:
	 * @gladewidget: the #GladeFixed which received the signal.
	 * @arg1: the child #GladeWidget
	 * @arg2: a pointer to a #GdkRectange describing the new size.
	 *
	 * Delegates the Drag/Resize job.
	 *
	 * Returns: %TRUE means you have handled the event and cancels the
	 *          default handler from being triggered.
	 */
	glade_fixed_signals[CONFIGURE_CHILD] =
		g_signal_new ("configure-child",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET
			      (GladeFixedClass, configure_child),
			      glade_boolean_handled_accumulator, NULL,
			      glade_marshal_BOOLEAN__OBJECT_POINTER,
			      G_TYPE_BOOLEAN, 2, G_TYPE_OBJECT, G_TYPE_POINTER);

	/**
	 * GladeFixed::configure-begin:
	 * @gladewidget: the #GladeFixed which received the signal.
	 * @arg1: the child #GladeWidget
	 *
	 * Signals the beginning of a Drag/Resize
	 */
	glade_fixed_signals[CONFIGURE_BEGIN] =
		g_signal_new ("configure-begin",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET
			      (GladeFixedClass, configure_begin),
			      NULL, NULL,
			      glade_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_OBJECT);

	/**
	 * GladeFixed::configure-end:
	 * @gladewidget: the #GladeFixed which received the signal.
	 * @arg1: the child #GladeWidget
	 *
	 * Signals the end of a Drag/Resize
	 */
	glade_fixed_signals[CONFIGURE_END] =
		g_signal_new ("configure-end",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET
			      (GladeFixedClass, configure_end),
			      NULL, NULL,
			      glade_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_OBJECT);
}


/*******************************************************************************
                                      API
*******************************************************************************/

GType
glade_fixed_get_type (void)
{
	static GType fixed_type = 0;
	
	if (!fixed_type)
	{
		static const GTypeInfo fixed_info = 
		{
			sizeof (GladeFixedClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) glade_fixed_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,           /* class_data */
			sizeof (GladeFixed),
			0,              /* n_preallocs */
			(GInstanceInitFunc) glade_fixed_init,
		};
		fixed_type = 
			g_type_register_static (GLADE_TYPE_WIDGET,
						"GladeFixed",
						&fixed_info, 0);
	}
	return fixed_type;
}
