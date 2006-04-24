/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-fixed-manager.c - A proxy object used as an interface to
 *                         handle child placement in fixed type GtkContainer 
 *                         such as GtkFixed and GtkLayout.
 *
 * Copyright (C) 2005 The GNOME Foundation.
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
#include "glade-fixed-manager.h"

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
	CREATE_CHILD,
	CHILD_CREATED,
	ADD_CHILD,
	REMOVE_CHILD,
	CONFIGURE_CHILD,
	CONFIGURE_BEGIN,
	CONFIGURE_END,
	GFM_SIGNALS
};

typedef struct {
	gulong press_id;
	gulong release_id;
	gulong motion_id;
	gulong enter_id;
} GFMSigData;

/* Convenience macros used in pointer events.
 */
#define GFM_CURSOR_TOP(type)                              \
	((type) == GLADE_CURSOR_RESIZE_TOP_RIGHT ||       \
	 (type) == GLADE_CURSOR_RESIZE_TOP_LEFT  ||       \
	 (type) == GLADE_CURSOR_RESIZE_TOP)

#define GFM_CURSOR_BOTTOM(type)                           \
	((type) == GLADE_CURSOR_RESIZE_BOTTOM_RIGHT ||    \
	 (type) == GLADE_CURSOR_RESIZE_BOTTOM_LEFT  ||    \
	 (type) == GLADE_CURSOR_RESIZE_BOTTOM)

#define GFM_CURSOR_RIGHT(type)                            \
	((type) == GLADE_CURSOR_RESIZE_TOP_RIGHT    ||    \
	 (type) == GLADE_CURSOR_RESIZE_BOTTOM_RIGHT ||    \
	 (type) == GLADE_CURSOR_RESIZE_RIGHT)

#define GFM_CURSOR_LEFT(type)                            \
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
static guint         gfm_signals[GFM_SIGNALS];

/*******************************************************************************
                             Static helper routines
 *******************************************************************************/
static GladeCursorType
gfm_get_operation (GtkWidget       *widget,
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
gfm_save_state (GladeFixedManager *manager,
		GladeWidget       *child)
{
	gdk_window_get_pointer (GTK_WIDGET (manager->container->object)->window, 
				&manager->pointer_x_origin, &manager->pointer_y_origin, NULL);

 	glade_widget_pack_property_get (child, manager->x_prop, &(manager->child_x_origin));
 	glade_widget_pack_property_get (child, manager->y_prop, &(manager->child_y_origin));
 	glade_widget_property_get (child, manager->width_prop, &(manager->child_width_origin));
 	glade_widget_property_get (child, manager->height_prop, &(manager->child_height_origin));

	manager->pointer_x_child_origin = 
		manager->pointer_x_origin - manager->child_x_origin;
	manager->pointer_y_child_origin = 
		manager->pointer_y_origin - manager->child_y_origin;
}


static void 
gfm_filter_event (GladeFixedManager *manager,
		  gint              *x,
		  gint              *y,
		  gint               left,
		  gint               right,
		  gint               top,
		  gint               bottom)
{
	gint cont_width, cont_height;

	g_return_if_fail (x && y);

	cont_width  = GTK_WIDGET (manager->container->object)->allocation.width;
	cont_height = GTK_WIDGET (manager->container->object)->allocation.height;

	/* Clip out mouse events that are outside the window.
	 */
	if ((left || manager->operation == GLADE_CURSOR_DRAG) &&
	    *x - manager->pointer_x_child_origin < 0)
		*x = manager->pointer_x_child_origin;
	if ((top || manager->operation == GLADE_CURSOR_DRAG) && 
	    *y - manager->pointer_y_child_origin < 0)
		*y = manager->pointer_y_child_origin;
	
	if ((right || manager->operation == GLADE_CURSOR_DRAG) && 
	    *x + (manager->child_width_origin - 
			  manager->pointer_x_child_origin) > cont_width)
		*x = cont_width - (manager->child_width_origin - 
				  manager->pointer_x_child_origin);
	if ((bottom || manager->operation == GLADE_CURSOR_DRAG) && 
	    *y + (manager->child_height_origin - 
			   manager->pointer_y_child_origin) > cont_height)
		*y = cont_height - (manager->child_height_origin - 
				   manager->pointer_y_child_origin);
	
	/* Clip out mouse events that mean shrinking to less than 0.
	 */
	if (left && 
	    (*x - manager->pointer_x_child_origin) > 
	    (manager->child_x_origin + (manager->child_width_origin - CHILD_WIDTH_MIN))) {
		*x = (manager->child_x_origin + (manager->child_width_origin - CHILD_WIDTH_MIN)) -
			manager->pointer_x_child_origin;
	    } else if (right &&
		       (*x - manager->pointer_x_child_origin) < 
		       manager->child_x_origin - (manager->child_width_origin + CHILD_WIDTH_MIN)) {
		*x = (manager->child_x_origin - (manager->child_width_origin + CHILD_WIDTH_MIN)) +
			manager->pointer_x_child_origin;
	}


	if (top && 
	    (*y - manager->pointer_y_child_origin) > 
	    (manager->child_y_origin + (manager->child_height_origin - CHILD_HEIGHT_MIN))) {
		*y = (manager->child_y_origin + (manager->child_height_origin - CHILD_HEIGHT_MIN)) - 
			manager->pointer_y_child_origin;
	} else if (bottom &&
		   (*y - manager->pointer_y_child_origin) < 
		   manager->child_y_origin - (manager->child_height_origin + CHILD_HEIGHT_MIN)) {
		*y = (manager->child_y_origin - (manager->child_height_origin + CHILD_HEIGHT_MIN)) + 
			manager->pointer_y_child_origin;
	}
}

static void
gfm_configure_widget (GladeFixedManager *manager,
		      GladeWidget       *child)
{
	GdkRectangle    new_area;
	gboolean        handled, right, left, top, bottom;
	gint            x, y;

	gdk_window_get_pointer (GTK_WIDGET (manager->container->object)->window, &x, &y, NULL);

	/* I think its safe here to skip the glade-property API */
	new_area.x      = GTK_WIDGET (child->object)->allocation.x;
	new_area.y      = GTK_WIDGET (child->object)->allocation.y;
	new_area.width  = GTK_WIDGET (child->object)->allocation.width;
	new_area.height = GTK_WIDGET (child->object)->allocation.height;

	right  = GFM_CURSOR_RIGHT  (manager->operation);
	left   = GFM_CURSOR_LEFT   (manager->operation);
	top    = GFM_CURSOR_TOP    (manager->operation);
	bottom = GFM_CURSOR_BOTTOM (manager->operation);

	/* Filter out events that make your widget go out of bounds */
	gfm_filter_event (manager, &x, &y, left, right, top, bottom);

	/* Modify current size.
	 */
	if (manager->operation == GLADE_CURSOR_DRAG)
	{
		/* Move widget */
		new_area.x = manager->child_x_origin + 
			x - manager->pointer_x_origin;
		new_area.y = manager->child_y_origin + 
			y - manager->pointer_y_origin;
	} else {

		if (bottom) 
		{
			new_area.height =
				manager->child_height_origin +
				(y - manager->pointer_y_origin);
		} else if (top)
		{

			new_area.height =
				manager->child_height_origin -
				(y - manager->pointer_y_origin);
			new_area.y =
				manager->child_y_origin +
				(y - manager->pointer_y_origin);
		}

		if (right) 
		{
			new_area.width =
				manager->child_width_origin +
				(x - manager->pointer_x_origin);
		} else if (left)
		{
			new_area.width =
				manager->child_width_origin -
				(x - manager->pointer_x_origin);
			new_area.x =
				manager->child_x_origin + 
				(x - manager->pointer_x_origin);
		}
	}

	/* Trim */
	if (new_area.width < CHILD_WIDTH_MIN)
		new_area.width = CHILD_WIDTH_MIN;
	if (new_area.height < CHILD_WIDTH_MIN)
		new_area.height = CHILD_HEIGHT_MIN;

	/* Apply new rectangle to the object */
	g_signal_emit (G_OBJECT (manager), 
		       gfm_signals[CONFIGURE_CHILD],
		       0, child, &new_area, &handled);
	
	/* Correct glitches when some widgets are draged over others */
	gtk_widget_queue_draw (GTK_WIDGET (manager->container->object));
}

static void
gfm_disconnect_child (GladeFixedManager *manager,
		      GladeWidget       *child)
{
	GFMSigData *data;

	if ((data = g_object_get_data (G_OBJECT (child), "gfm-signal-data")) != NULL)
	{
		g_signal_handler_disconnect (child->object, data->press_id);
		g_signal_handler_disconnect (child->object, data->release_id);
		g_signal_handler_disconnect (child->object, data->enter_id);
		g_signal_handler_disconnect (child->object, data->motion_id);

		g_object_set_data (G_OBJECT (child), "gfm-signal-data", NULL);
		g_free (data);
	}
}

static void
gfm_connect_child (GladeFixedManager *manager,
		   GladeWidget       *child)
{
	GFMSigData *data;

	if ((data = g_object_get_data (G_OBJECT (child), "gfm-signal-data")) != NULL)
		gfm_disconnect_child (manager, child);
	
	data = g_new (GFMSigData, 1);

	data->press_id =
		g_signal_connect
		(child->object, "button-press-event", G_CALLBACK
		 (GLADE_FIXED_MANAGER_GET_CLASS(manager)->child_event), manager);
	data->release_id =
		g_signal_connect
		(child->object, "button-release-event", G_CALLBACK
		 (GLADE_FIXED_MANAGER_GET_CLASS(manager)->child_event), manager);
	data->enter_id =
		g_signal_connect
		(child->object, "enter-notify-event", G_CALLBACK
		 (GLADE_FIXED_MANAGER_GET_CLASS(manager)->child_event), manager);
	data->motion_id = 
		g_signal_connect
		(child->object, "motion-notify-event", G_CALLBACK
		 (GLADE_FIXED_MANAGER_GET_CLASS(manager)->child_event), manager);

	g_object_set_data (G_OBJECT (child), "gfm-signal-data", data);
}



/*******************************************************************************
                               GladeFixedManagerClass
 *******************************************************************************/
static GladeWidget *
glade_fixed_manager_create_child_impl (GladeFixedManager *manager,
				       GladeWidgetClass  *wclass)
{
	GladeWidget *gwidget =
		glade_widget_new (manager->container,
				  wclass, 
				  manager->container->project, TRUE);

	return gwidget;
}

static gboolean
glade_fixed_manager_add_child_impl (GladeFixedManager *manager,
				    GladeWidget       *child)
{
	glade_widget_class_container_add 
		(manager->container->widget_class,
		 manager->container->object,
		 child->object);
	glade_widget_set_parent (child, manager->container);
	return TRUE;
}

static gboolean
glade_fixed_manager_remove_child_impl (GladeFixedManager *manager,
				       GladeWidget       *child)
{
	glade_widget_class_container_remove
		(manager->container->widget_class,
		 manager->container->object, child->object);
	return TRUE;
}

static gboolean
glade_fixed_manager_configure_child_impl (GladeFixedManager *manager,
					  GladeWidget       *child,
					  GdkRectangle      *rect)
{
 	glade_widget_pack_property_set (child, manager->x_prop, rect->x);
 	glade_widget_pack_property_set (child, manager->y_prop, rect->y);
 	glade_widget_property_set (child, manager->width_prop, rect->width);
 	glade_widget_property_set (child, manager->height_prop, rect->height);
	return TRUE;
}


static void
glade_fixed_manager_configure_end_impl (GladeFixedManager *manager,
					GladeWidget       *child)
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

	x_prop      = glade_widget_get_pack_property (child, manager->x_prop);
	y_prop      = glade_widget_get_pack_property (child, manager->y_prop);
	width_prop  = glade_widget_get_property      (child, manager->width_prop);
	height_prop = glade_widget_get_property      (child, manager->height_prop);

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

	g_value_set_int (&x_value, manager->child_x_origin);
	g_value_set_int (&y_value, manager->child_y_origin);
	g_value_set_int (&width_value, manager->child_width_origin);
	g_value_set_int (&height_value, manager->child_height_origin);

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
gfm_handle_child_event (GladeFixedManager  *manager,
			GladeWidget        *child,
			GdkEvent           *event)
{
	gboolean handled = FALSE;
	gint parent_x, parent_y, child_x, child_y, x, y;
	GladeCursorType operation;

	gdk_window_get_pointer (GTK_WIDGET (manager->container->object)->window, 
				&parent_x, &parent_y, NULL);

 	glade_widget_pack_property_get (child, manager->x_prop, &child_x);
 	glade_widget_pack_property_get (child, manager->y_prop, &child_y);

	x = parent_x - child_x;
	y = parent_y - child_y;

	operation = gfm_get_operation (GTK_WIDGET (child->object), x, y);

	switch (event->type)
	{
	case GDK_ENTER_NOTIFY:
	case GDK_MOTION_NOTIFY:
		if (manager->configuring == NULL)
		{
			/* GTK_NO_WINDOW widgets still have a pointer to the parent window */
			glade_cursor_set (GTK_WIDGET (child->object)->window, 
					  operation);
			handled = TRUE;
		} else if (event->type == GDK_MOTION_NOTIFY) 
		{
			gfm_configure_widget (manager, child);
			handled = TRUE;
		}
		gdk_window_get_pointer (GTK_WIDGET (child->object)->window, NULL, NULL, NULL);
		break;
	case GDK_BUTTON_PRESS:
		if (((GdkEventButton *)event)->button == 1)
		{
			manager->configuring = child;
			/* Save widget allocation and pointer pos */
			gfm_save_state (manager, child);
			
			manager->operation = operation;
			glade_cursor_set (GTK_WIDGET (child->object)->window, manager->operation);

			g_signal_emit (G_OBJECT (manager),
				       gfm_signals[CONFIGURE_BEGIN],
				       0, child);

			handled = TRUE;
		}
		break;
	case GDK_BUTTON_RELEASE:
		if (((GdkEventButton *)event)->button == 1 && 
		    manager->configuring)
		{
			// cancel drag stuff
			manager->operation = operation;
			glade_cursor_set (GTK_WIDGET (child->object)->window, manager->operation);

			g_signal_emit (G_OBJECT (manager),
				       gfm_signals[CONFIGURE_END],
				       0, child);

			manager->configuring = NULL;
			handled = TRUE;
		}
		break;
	default:
		g_debug ("Unhandled event");
		break;
	}
	return handled;
}

/* Handle button-press-event, enter/leave-notify-event, 
 * motion-notify-event
 */
static gint
glade_fixed_manager_event (GtkWidget         *widget, 
			   GdkEvent          *event,
			   GladeFixedManager *manager)
{
	GladeWidgetClass *add_class, *alt_class;
	gboolean          handled = FALSE;
	GladeWidget      *gfm_widget;
	GladeWidget      *gwidget;
	gdouble           x, y;
	

	gdk_window_get_pointer (widget->window, NULL, NULL, NULL);
	gfm_widget = glade_widget_get_from_gobject (widget);
	gdk_event_get_coords (event, &x, &y);
	gwidget    = glade_widget_retrieve_from_position
		(widget, (int) (x + 0.5), (int) (y + 0.5));

	g_return_val_if_fail (GLADE_IS_WIDGET (gwidget), FALSE);
	g_return_val_if_fail (GLADE_IS_WIDGET (gfm_widget), FALSE);

	/* make sure to grab focus, since we may stop default handlers */
	if (GTK_WIDGET_CAN_FOCUS (widget) && !GTK_WIDGET_HAS_FOCUS (widget))
		gtk_widget_grab_focus (widget);


	switch (event->type)
	{

	case GDK_BUTTON_PRESS:
	case GDK_ENTER_NOTIFY:
	case GDK_MOTION_NOTIFY:
	case GDK_BUTTON_RELEASE:

		if (manager->configuring)
			return gfm_handle_child_event (manager, manager->configuring, event);

		/* Get the gwidget that is a direct child of manager->container */
		if (gfm_widget != gwidget)
		{
			while (gwidget != NULL &&
			       gwidget->parent != manager->container)
				gwidget = gwidget->parent;
			if (gwidget)
				return gfm_handle_child_event (manager, gwidget, event);
		}
		break;
	default:
		break;
	}

	switch (event->type)
	{
	case GDK_BUTTON_PRESS: // add widget
		
		add_class = glade_app_get_add_class ();
		alt_class = glade_app_get_alt_class ();

		if (((GdkEventButton *)event)->button == 1)
		{

			if ((add_class != NULL)        ||
				 ((((GdkEventButton *)event)->state & GDK_SHIFT_MASK) &&
				  alt_class != NULL))
			{
				/* A widget type is selected in the palette.
				 * Add a new widget of that type.
				 */
				manager->creating = TRUE;
				gdk_window_get_pointer (GTK_WIDGET (manager->container->object)->window, 
							&manager->create_x, &manager->create_y, NULL);
				glade_command_create
					(add_class ? add_class : alt_class, 
					 manager->container,
					 NULL, manager->container->project);
				manager->creating = FALSE;

				
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
			glade_cursor_set (((GdkEventCrossing *)event)->window, GLADE_CURSOR_ADD_WIDGET);
		else
			glade_cursor_set (((GdkEventCrossing *)event)->window, GLADE_CURSOR_SELECTOR);
		handled = TRUE;
		break;
	default:
		break;
	}
	return handled;
}

static gint
glade_fixed_manager_child_event (GtkWidget         *widget, 
				 GdkEvent          *event,
				 GladeFixedManager *manager)
{
	GladeWidget *gwidget =
		glade_widget_get_from_gobject (widget);
	return gfm_handle_child_event (manager,	gwidget, event);
}


/*******************************************************************************
                                   GObjectClass
 *******************************************************************************/
static void
glade_fixed_manager_finalize (GObject *object)
{
	GladeFixedManager *manager = GLADE_FIXED_MANAGER (object);

	/* A GladeFixedManager should be finalized as a result of its
	 * GtkContainer being destroyed, so we shouldn't need to bother
	 * about disconnecting all the child signals.
	 */
	g_free (manager->x_prop);
	g_free (manager->y_prop);
	g_free (manager->width_prop);
	g_free (manager->height_prop);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
glade_fixed_manager_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
	GladeFixedManager *manager = GLADE_FIXED_MANAGER (object);

	switch (prop_id)
	{
	case PROP_X_PROP:
		g_free (manager->x_prop);
		manager->x_prop = g_value_dup_string (value);
		break;
	case PROP_Y_PROP:
		g_free (manager->y_prop);
		manager->y_prop = g_value_dup_string (value);
		break;
	case PROP_WIDTH_PROP:
		g_free (manager->width_prop);
		manager->width_prop = g_value_dup_string (value);
		break;
	case PROP_HEIGHT_PROP:
		g_free (manager->height_prop);
		manager->height_prop = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_fixed_manager_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
	GladeFixedManager *manager = GLADE_FIXED_MANAGER (object);

	switch (prop_id)
	{
	case PROP_X_PROP:       g_value_set_string (value, manager->x_prop);        break;
	case PROP_Y_PROP:       g_value_set_string (value, manager->y_prop);        break;
	case PROP_WIDTH_PROP:   g_value_set_string (value, manager->width_prop);    break;
	case PROP_HEIGHT_PROP:  g_value_set_string (value, manager->height_prop);   break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_fixed_manager_init (GladeFixedManager *manager)
{
	/* Set defaults */
	manager->x_prop      = g_strdup ("x");
	manager->y_prop      = g_strdup ("y");
	manager->width_prop  = g_strdup ("width");
	manager->height_prop = g_strdup ("height");
}

static void
glade_fixed_manager_class_init (GladeFixedManagerClass *manager_class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (manager_class);
	parent_class = 
		G_OBJECT_CLASS
		(g_type_class_peek_parent (gobject_class));

	gobject_class->finalize         = glade_fixed_manager_finalize;
	gobject_class->set_property     = glade_fixed_manager_set_property;
	gobject_class->get_property     = glade_fixed_manager_get_property;
	
	manager_class->create_child     = glade_fixed_manager_create_child_impl;
	manager_class->child_created    = NULL;
	manager_class->add_child        = glade_fixed_manager_add_child_impl;
	manager_class->remove_child     = glade_fixed_manager_remove_child_impl;
	manager_class->configure_child  = glade_fixed_manager_configure_child_impl;
	manager_class->configure_begin  = NULL;
	manager_class->configure_end    = glade_fixed_manager_configure_end_impl;
	manager_class->event            = glade_fixed_manager_event;
	manager_class->child_event      = glade_fixed_manager_child_event;

	/* Properties */
	g_object_class_install_property 
		(gobject_class, PROP_X_PROP,
		 g_param_spec_string 
		 ("x_prop", _("X position property"),
		  _("The property used to set the X position of a child object"),
		  "x", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property 
		(gobject_class, PROP_Y_PROP,
		 g_param_spec_string 
		 ("y_prop", _("Y position property"),
		  _("The property used to set the Y position of a child object"),
		  "y", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property 
		(gobject_class, PROP_WIDTH_PROP,
		 g_param_spec_string 
		 ("width_prop", _("Width property"),
		  _("The property used to set the width of a child object"),
		  "width-request", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property 
		(gobject_class, PROP_HEIGHT_PROP,
		 g_param_spec_string 
		 ("height_prop", _("Height property"),
		  _("The property used to set the height of a child object"),
		  "height-request", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	/* Signals */
	gfm_signals[CREATE_CHILD]    = 
		g_signal_new ("create-child",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET
			      (GladeFixedManagerClass, create_child),
			      glade_single_object_accumulator, NULL,
			      glade_marshal_OBJECT__POINTER,
			      G_TYPE_OBJECT, 1, G_TYPE_POINTER);

	gfm_signals[CHILD_CREATED]       =
		g_signal_new ("child-created",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET
			      (GladeFixedManagerClass, child_created),
			      NULL, NULL,
			      glade_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_OBJECT);

	gfm_signals[ADD_CHILD]       =
		g_signal_new ("add-child",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET
			      (GladeFixedManagerClass, add_child),
			      glade_boolean_handled_accumulator, NULL,
			      glade_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN, 1, G_TYPE_OBJECT);

	gfm_signals[REMOVE_CHILD]    =
		g_signal_new ("remove-child",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET
			      (GladeFixedManagerClass, remove_child),
			      glade_boolean_handled_accumulator, NULL,
			      glade_marshal_BOOLEAN__OBJECT,
			      G_TYPE_BOOLEAN, 1, G_TYPE_OBJECT);

	gfm_signals[CONFIGURE_CHILD] =
		g_signal_new ("configure-child",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET
			      (GladeFixedManagerClass, configure_child),
			      glade_boolean_handled_accumulator, NULL,
			      glade_marshal_BOOLEAN__OBJECT_POINTER,
			      G_TYPE_BOOLEAN, 2, G_TYPE_OBJECT, G_TYPE_POINTER);

	gfm_signals[CONFIGURE_BEGIN] =
		g_signal_new ("configure-begin",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET
			      (GladeFixedManagerClass, configure_begin),
			      NULL, NULL,
			      glade_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_OBJECT);

	gfm_signals[CONFIGURE_END] =
		g_signal_new ("configure-end",
			      G_TYPE_FROM_CLASS (gobject_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET
			      (GladeFixedManagerClass, configure_end),
			      NULL, NULL,
			      glade_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_OBJECT);

}



/*******************************************************************************
                                      API
*******************************************************************************/

GType
glade_fixed_manager_get_type (void)
{
	static GType fixed_manager_type = 0;
	
	if (!fixed_manager_type)
	{
		static const GTypeInfo fixed_manager_info = 
		{
			sizeof (GladeFixedManagerClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) glade_fixed_manager_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,           /* class_data */
			sizeof (GladeFixedManager),
			0,              /* n_preallocs */
			(GInstanceInitFunc) glade_fixed_manager_init,
		};
		fixed_manager_type = 
			g_type_register_static (G_TYPE_OBJECT,
						"GladeFixedManager",
						&fixed_manager_info, 0);
	}
	return fixed_manager_type;
}

GladeFixedManager *
glade_fixed_manager_new (GladeWidget  *gtkcontainer,
			 const gchar  *x_prop,
			 const gchar  *y_prop,
			 const gchar  *width_prop,
			 const gchar  *height_prop)
{
	GladeFixedManager *manager;

	g_return_val_if_fail (GLADE_IS_WIDGET (gtkcontainer), NULL);
	g_return_val_if_fail (GTK_IS_CONTAINER (gtkcontainer->object), NULL);

	manager = g_object_new (GLADE_TYPE_FIXED_MANAGER, 
				"x_prop", x_prop,
				"y_prop", y_prop,
				"width_prop", width_prop,
				"height_prop", height_prop,
				NULL);

	manager->container    = gtkcontainer;
	gtkcontainer->manager = manager;

	/* Hook up fixed container.
	 */
	gtk_widget_add_events (GTK_WIDGET (gtkcontainer->object),
			       GDK_POINTER_MOTION_MASK      |
			       GDK_POINTER_MOTION_HINT_MASK |
			       GDK_BUTTON_PRESS_MASK        |
			       GDK_BUTTON_RELEASE_MASK      |
			       GDK_ENTER_NOTIFY_MASK);

	g_signal_connect
		(gtkcontainer->object, "button-press-event", G_CALLBACK
		 (GLADE_FIXED_MANAGER_GET_CLASS(manager)->event), manager);
	g_signal_connect
		(gtkcontainer->object, "button-release-event", G_CALLBACK
		 (GLADE_FIXED_MANAGER_GET_CLASS(manager)->event), manager);
	g_signal_connect
		(gtkcontainer->object, "enter-notify-event", G_CALLBACK
		 (GLADE_FIXED_MANAGER_GET_CLASS(manager)->event), manager);
	g_signal_connect
		(gtkcontainer->object, "motion-notify-event", G_CALLBACK
		 (GLADE_FIXED_MANAGER_GET_CLASS(manager)->event), manager);
	
	return manager;
}

GladeWidget *
glade_fixed_manager_create_child (GladeFixedManager *manager,
				  GladeWidgetClass  *wclass)
{
	GladeWidget *gwidget = NULL;
	g_return_val_if_fail (GLADE_IS_FIXED_MANAGER (manager), NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS  (wclass), NULL);

	g_signal_emit (G_OBJECT (manager), 
		       gfm_signals[CREATE_CHILD],
		       0, wclass, &gwidget);

	/* Can be NULL if user cancels in query dialog */
	if (gwidget != NULL)
		g_signal_emit (G_OBJECT (manager), 
			       gfm_signals[CHILD_CREATED],
			       0, gwidget);

	return gwidget;
}

void
glade_fixed_manager_add_child (GladeFixedManager *manager,
			       GladeWidget       *child,
			       gboolean           at_mouse)
{
	GdkRectangle rect;
	gboolean     handled;

	g_return_if_fail (GLADE_IS_FIXED_MANAGER (manager));
	g_return_if_fail (GLADE_IS_WIDGET (child));

	g_signal_emit (G_OBJECT (manager),
		       gfm_signals[ADD_CHILD],
		       0, child, &handled);
	
	if (!glade_util_gtkcontainer_relation (manager->container, child))
		return;
	
	gtk_widget_add_events (GTK_WIDGET (child->object),
			       GDK_POINTER_MOTION_MASK      |
			       GDK_POINTER_MOTION_HINT_MASK |
			       GDK_BUTTON_PRESS_MASK        |
			       GDK_BUTTON_RELEASE_MASK      |
			       GDK_ENTER_NOTIFY_MASK);

	gfm_connect_child (manager, child);

	/* Make sure we can modify these properties */
	glade_widget_pack_property_set_enabled (child, manager->x_prop, TRUE);
	glade_widget_pack_property_set_enabled (child, manager->y_prop, TRUE);
	glade_widget_property_set_enabled (child, manager->width_prop, TRUE);
	glade_widget_property_set_enabled (child, manager->height_prop, TRUE);

	if (manager->creating)
	{
		rect.x      = manager->create_x;
		rect.y      = manager->create_y;
		rect.width  = CHILD_WIDTH_DEF;
		rect.height = CHILD_HEIGHT_DEF;
		
		g_signal_emit (G_OBJECT (manager),
			       gfm_signals[CONFIGURE_CHILD],
			       0, child, &rect, &handled);
	} else if (at_mouse)
	{
		rect.x      = manager->mouse_x;
		rect.y      = manager->mouse_y;

		glade_widget_property_get (child, manager->width_prop, &rect.width);
		glade_widget_property_get (child, manager->height_prop, &rect.height);

		if (rect.width <= 0)
			rect.width  = CHILD_WIDTH_DEF;

		if (rect.height <= 0)
			rect.height = CHILD_HEIGHT_DEF;
		
		g_signal_emit (G_OBJECT (manager),
			       gfm_signals[CONFIGURE_CHILD],
			       0, child, &rect, &handled);
	}

}

void
glade_fixed_manager_remove_child (GladeFixedManager *manager,
				  GladeWidget       *child)
{
	gboolean handled;
	g_return_if_fail (GLADE_IS_FIXED_MANAGER (manager));
	g_return_if_fail (GLADE_IS_WIDGET (child));

	gfm_disconnect_child (manager, child);

	g_signal_emit (G_OBJECT (manager),
		       gfm_signals[REMOVE_CHILD],
		       0, child, &handled);
}

void
glade_fixed_manager_post_mouse (GladeFixedManager *manager,
				gint               x,
				gint               y)
{
	g_return_if_fail (GLADE_IS_FIXED_MANAGER (manager));
	manager->mouse_x = x;
	manager->mouse_y = y;
}
