/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Chema Celorio <chema@celorio.com>
 */

#include <string.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-placeholder.h"
#include "glade-project.h"
#include "glade-project-window.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-popup.h"
#include "glade-placeholder.h"
#include "glade-signal.h"
#include "glade-gtk.h"
#include "glade-packing.h"

#define GLADE_WIDGET_SELECTION_NODE_SIZE 7

/**
 * glade_widget_new_name:
 * @project:
 * @class: 
 * 
 * Allocates a new name for a specific GladeWidgetClass.
 * 
 * Return Value: a newly allocated name for the widget. Caller must g_free it
 **/
gchar *
glade_widget_new_name (GladeProject *project, GladeWidgetClass *class)
{
	gint i = 1;
	gchar *name;

	while (TRUE) {
		name = g_strdup_printf ("%s%i", class->generic_name, i);
		if (glade_project_get_widget_by_name (project, name) == NULL)
			return name;
		g_free (name);
		i++;
	}

}

/**
 * glade_widget_new:
 * @project: The GladeProject this widget belongs to
 * @class: The GladeWidgeClass of the GladeWidget
 * @gtk_widget: The "view" of the GladeWidget
 * @name: The unique name, this is visible name to the user
 * 
 * Allocates a new GladeWidget structure and fills in the required memebers.
 * 
 * Return Value: 
 **/
static GladeWidget *
glade_widget_new (GladeWidgetClass *class, GladeProject *project)
{
	GladeWidget *widget;

	widget = g_new0 (GladeWidget, 1);
	widget->name     = NULL;
	widget->widget   = NULL;
	widget->project  = project;
	widget->class    = class;
	widget->properties = glade_property_list_new_from_widget_class (class, widget);
	widget->parent   = NULL;
	widget->children = NULL;
	widget->selected = FALSE;

	return widget;
}

/**
 * glade_widget_get_from_gtk_widget:
 * @widget: 
 * 
 * Given a GtkWidget, it returns its corresponding GladeWidget
 * 
 * Return Value: a GladeWidget pointer for @widget, NULL if the widget does not
 *               have a GladeWidget counterpart.
 **/
GladeWidget *
glade_widget_get_from_gtk_widget (GtkWidget *widget)
{
	GladeWidget *glade_widget;

	glade_widget = gtk_object_get_data (GTK_OBJECT (widget), GLADE_WIDGET_DATA_TAG);

	return glade_widget;
}

/* A temp data struct that we use when looking for a widget inside a container
 * we need a struct, because the forall can only pass one pointer
 */
typedef struct
{
    gint x;
    gint y;
    GtkWidget *found_child;
}GladeFindInContainerData;


/* #define DEBUG */
static void
glade_widget_find_inside_container (GtkWidget *widget, gpointer data_in)
{
	GladeFindInContainerData *data = data_in;

#ifdef DEBUG
	g_debug ("In find_child_at: %s X:%i Y:%i W:%i H:%i\n"
		 "  so this means that if we are in the %d-%d , %d-%d range. We are ok\n",
		 gtk_widget_get_name (widget),
		 widget->allocation.x, widget->allocation.y,
		 widget->allocation.width, widget->allocation.height,
		 widget->allocation.x, widget->allocation.x + widget->allocation.width,
		 widget->allocation.y, widget->allocation.y + widget->allocation.height);
#endif	

	/* Notebook pages are visible but not mapped if they are not showing. */
	if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget)
	    && (widget->allocation.x <= data->x)
	    && (widget->allocation.y <= data->y)
	    && (widget->allocation.x + widget->allocation.width >= data->x)
	    && (widget->allocation.y + widget->allocation.height >= data->y))
	{
#ifdef DEBUG	
		g_debug ("Found it!\n");
#endif	
		data->found_child = widget;
	}
}

/**
 * glade_widget_get_from_event_widget:
 * @event_widget: 
 * @event: 
 * 
 * Returns the real widget that was "clicked over" for a given event (cooridantes) and a widget
 * For example, when a label is clicked the button press event is triggered for its parent, this
 * function takes the event and the widget that got the event and returns the real GladeWidget that was
 * clicked
 * 
 * Return Value: 
 **/
static GladeWidget *
glade_widget_get_from_event_widget (GtkWidget *event_widget, GdkEventButton *event)
{
	GladeFindInContainerData data;
	GladeWidget *found = NULL;
	GtkWidget *temp;
	GdkWindow *window;
	GdkWindow *parent_window;
	gint x, win_x;
	gint y, win_y;
#ifdef DEBUG
	GladeWidget *real_event_glade_widget = glade_widget_get_from_gtk_widget (event_widget);
#endif	
	
	window = event->window;
	x = event->x;
	y = event->y;
	gdk_window_get_position (event_widget->window, &win_x, &win_y);
#ifdef DEBUG	
	g_debug ("Window [%d,%d]\n", win_x, win_y);
	g_debug ("\n\nWe want to find the real widget that was clicked at %d,%d\n", x, y);
	g_debug ("The widget that received the event was \"%s\" a \"%s\" [%d]\n",
		 NULL,
		 gtk_widget_get_name (event_widget),
		 GPOINTER_TO_INT (event_widget));
#endif	

	parent_window = event_widget->parent ? event_widget->parent->window : event_widget->window;
	while (window && window != parent_window) {
		
		gdk_window_get_position (window, &win_x, &win_y);
#ifdef DEBUG	
		g_debug ("	  adding X:%d Y:%d - We now have : %d %d\n",
			 win_x, win_y, x + win_x, y + win_y);
#endif	
		x += win_x;
		y += win_y;
		window = gdk_window_get_parent (window);
	}

	temp = event_widget;
	data.found_child = NULL;
	data.x = x;
	data.y = y;
	found = glade_widget_get_from_gtk_widget (event_widget);

	while (GTK_IS_CONTAINER (temp)) {
#ifdef DEBUG	
		g_debug ("\"%s\" is a container, check inside each child\n",
			 gtk_widget_get_name (temp));
#endif
		data.found_child = NULL;
		gtk_container_forall (GTK_CONTAINER (temp),
				      (GtkCallback) glade_widget_find_inside_container, &data);
		if (data.found_child) {
			temp = data.found_child;
			if (glade_widget_get_from_gtk_widget (temp)) {
				found = glade_widget_get_from_gtk_widget (temp);
				g_assert (found->widget == data.found_child);
			} else {
#ifdef DEBUG	
				g_debug ("Temp was not a GladeWidget, it was a %s\n",
					 gtk_widget_get_name (temp));
#endif	
			}
		} else {
			/* The user clicked on the container itself */
			found = glade_widget_get_from_gtk_widget (temp);
			break;
		}

	}
#ifdef DEBUG	
	if (!found) {
		GladeWidget *gw;
		gw = glade_widget_get_from_gtk_widget (event_widget);
		g_warning ("We could not find the widget %s:%s\n",
			   gtk_widget_get_name (event_widget),
			   gw ? gw->name : "es null");
		return NULL;
	}
#else
	g_return_val_if_fail (found != NULL, NULL);
#endif
	
#ifdef DEBUG
	g_debug ("We found a \"%s\", child at %d,%d\n",
		 gtk_widget_get_name (found->widget), data.x, data.y);
#endif	
	return found;
}

/**
 * glade_widget_button_press:
 * @event_widget: 
 * @event: 
 * @not_used: 
 * 
 * Handle the button press event for every GladeWidget
 * 
 * Return Value: 
 **/
static gboolean
glade_widget_button_press (GtkWidget *event_widget, GdkEventButton *event, gpointer not_used)
{
	GladeWidget *glade_widget;

	glade_widget = glade_widget_get_from_event_widget (event_widget, event);

#ifdef DEBUG
	g_debug ("button press for a %s\n", glade_widget->class->name);
#endif
	if (!glade_widget) {
		g_warning ("Button press event but the gladewidget was not found\n");
		return FALSE;
	}

#ifdef DEBUG	
	g_debug ("Event button %d\n", event->button);
#endif

	if (event->button == 1) {
		/* If this is a selection set, don't change the state of the widget
		 * for exmaple for toggle buttons
		 */
		if (!glade_widget->selected)
			gtk_signal_emit_stop_by_name (GTK_OBJECT (event_widget),
						      "button_press_event");
		glade_project_selection_set (glade_widget, TRUE);
	} else if (event->button == 3)
		glade_popup_pop (glade_widget, event);
#ifdef DEBUG	
	else
		g_debug ("Button press not handled yet.\n");
#endif

#ifdef DEBUG	
	g_debug ("The widget found was a %s\n", glade_widget->class->name);
#endif

	return FALSE;
}
#undef DEBUG

static gboolean
glade_widget_button_release (GtkWidget *widget, GdkEventButton *event, gpointer not_used)
{
#ifdef DEBUG	
	g_debug ("button release\n");
#endif
	return FALSE;
}

static void
glade_property_refresh (GladeProperty *property)
{
	switch (property->class->type) {
	case GLADE_PROPERTY_TYPE_BOOLEAN:
		glade_property_set_boolean (property,
					    glade_property_get_boolean (property));
		break;
	case GLADE_PROPERTY_TYPE_FLOAT:
		glade_property_set_float (property,
					  glade_property_get_float (property));
		break;
	case GLADE_PROPERTY_TYPE_INTEGER:
		glade_property_set_integer (property,
					    glade_property_get_integer (property));
		break;
	case GLADE_PROPERTY_TYPE_DOUBLE:
		glade_property_set_double (property,
					   glade_property_get_double (property));
		break;
	case GLADE_PROPERTY_TYPE_STRING:
		glade_property_set_string (property,
					   glade_property_get_string (property));
		break;
	case GLADE_PROPERTY_TYPE_ENUM:
		glade_property_set_enum (property,
					 glade_property_get_enum (property));
		break;
	case GLADE_PROPERTY_TYPE_OBJECT:
		g_print ("Set adjustment (refresh) %d\n", GPOINTER_TO_INT (property->child));
#if 1	
#if 0
		glade_widget_set_default_options_real (property->child, packing);
#endif	
		g_print ("Set directly \n");
		gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (property->widget->widget),
						GTK_ADJUSTMENT (property->child));
		g_print ("DONE : Set directly\n");
#else		
		gtk_object_set (GTK_OBJECT (property->widget->widget),
				property->class->id,
				property->child, NULL);
#endif		
		g_print ("Adjustment has been set\n");
		break;
	default:
		g_warning ("Implement set default for this type [%s]\n", property->class->name);
		break;
	}
}

/**
 * glade_widget_set_default_options:
 * @widget: 
 * 
 * Takes care of applying the default values to a newly created widget
 **/
static void
glade_widget_set_default_options_real (GladeWidget *widget, gboolean packing)
{
	GladeProperty *property;
	GList *list;

	list = widget->properties;
	for (; list != NULL; list = list->next) {
		property = list->data;

		if (property->class->packing != packing)
			continue;

		/* For some properties, we get the value from the GtkWidget, for example the
		 * "position" packing property. See g-p-class.h ->get_default. Chema
		 */
		if (property->class->get_default) {
			glade_property_get_from_widget (property);
			continue;
		}

		property->loading = TRUE;
		glade_property_refresh (property);
		property->loading = FALSE;

	}

}

static void
glade_widget_set_default_options (GladeWidget *widget)
{
	glade_widget_set_default_options_real (widget, FALSE);
}

/**
 * glade_widget_set_default_packing_options:
 * @widget: 
 * 
 * We need to have a different funcition for setting packing options
 * because we weed to add the widget to the container before we
 * apply the packing options
 *
 **/
void
glade_widget_set_default_packing_options (GladeWidget *widget)
{
	glade_widget_set_default_options_real (widget, TRUE);
}

static GdkWindow*
glade_widget_get_window (GladeWidget *widget, GtkWidget **paint_widget)
{
	GtkWidget *parent = widget->widget->parent;
	
	if (parent) {
		*paint_widget = parent;
		return parent->window;
	}

	*paint_widget = widget->widget;
	return widget->widget->window;
}

static void
glade_widget_draw_selection_nodes (GladeWidget *glade_widget)
{
	GtkWidget *widget, *paint_widget;
	GdkWindow *window;
	GdkGC *gc;
	gint x, y, w, h;
	gint width, height;

	widget = glade_widget->widget;
	window = glade_widget_get_window (glade_widget, &paint_widget);

	if (widget->parent) {
		x = widget->allocation.x;
		y = widget->allocation.y;
		w = widget->allocation.width;
		h = widget->allocation.height;
	} else {
		x = 0;
		y = 0;
		gdk_window_get_size (window, &w, &h);
	}

	gc = paint_widget->style->black_gc;
	gdk_gc_set_subwindow (gc, GDK_INCLUDE_INFERIORS);

	width = w;
	height = h;
	if (width > GLADE_WIDGET_SELECTION_NODE_SIZE && height > GLADE_WIDGET_SELECTION_NODE_SIZE) {
		gdk_draw_rectangle (window, gc, TRUE,
				    x, y,
				    GLADE_WIDGET_SELECTION_NODE_SIZE, GLADE_WIDGET_SELECTION_NODE_SIZE);
		gdk_draw_rectangle (window, gc, TRUE,
				    x, y + height - GLADE_WIDGET_SELECTION_NODE_SIZE,
				    GLADE_WIDGET_SELECTION_NODE_SIZE, GLADE_WIDGET_SELECTION_NODE_SIZE);
		gdk_draw_rectangle (window, gc, TRUE,
				    x + width - GLADE_WIDGET_SELECTION_NODE_SIZE, y,
				    GLADE_WIDGET_SELECTION_NODE_SIZE, GLADE_WIDGET_SELECTION_NODE_SIZE);
		gdk_draw_rectangle (window, gc, TRUE,
				    x + width - GLADE_WIDGET_SELECTION_NODE_SIZE,
				    y + height - GLADE_WIDGET_SELECTION_NODE_SIZE,
				    GLADE_WIDGET_SELECTION_NODE_SIZE, GLADE_WIDGET_SELECTION_NODE_SIZE);
	}
	gdk_draw_rectangle (window, gc, FALSE,
			    x, y, width - 1, height - 1);
	
	gdk_gc_set_subwindow (gc, GDK_CLIP_BY_CHILDREN);
	
}

static gint
glade_widget_expose_event_cb (GtkWidget *widget, GdkEventExpose *event,
			      GladeWidget *glade_widget)
{
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

	g_assert (widget == glade_widget->widget);

	if (glade_widget->selected)
		glade_widget_draw_selection_nodes (glade_widget);

	return FALSE;
}

static void
glade_widget_connect_draw_signals (GladeWidget *glade_widget)
{
	GtkWidget *widget = glade_widget->widget;

	gtk_signal_connect_after (GTK_OBJECT (widget), "expose_event",
				  GTK_SIGNAL_FUNC (glade_widget_expose_event_cb),
				  glade_widget);
}

static void
glade_widget_connect_mouse_signals (GladeWidget *glade_widget)
{
	GtkWidget *widget = glade_widget->widget;

	if (!GTK_WIDGET_NO_WINDOW (widget)) {
		if (!GTK_WIDGET_REALIZED (widget))
			gtk_widget_set_events (widget, gtk_widget_get_events (widget)
					       | GDK_BUTTON_PRESS_MASK
					       | GDK_BUTTON_RELEASE_MASK);
		else {
			GdkEventMask event_mask;

			event_mask = gdk_window_get_events (widget->window);
			gdk_window_set_events (widget->window, event_mask
					       | GDK_BUTTON_PRESS_MASK
					       | GDK_BUTTON_RELEASE_MASK);
		}
	}
	  
	gtk_signal_connect (GTK_OBJECT (widget), "button_press_event",
			    GTK_SIGNAL_FUNC (glade_widget_button_press), NULL);
	gtk_signal_connect (GTK_OBJECT (widget), "button_release_event",
			    GTK_SIGNAL_FUNC (glade_widget_button_release), NULL);
}


/**
 * glade_widget_set_contents:
 * @widget: 
 * 
 * Loads the name of the widget. For example a button will have the
 * "button1" text in it, or a label will have "label4". right after
 * it is created.
 **/
static void
glade_widget_set_contents (GladeWidget *widget)
{
	GladeProperty *property = NULL;
	GladeWidgetClass *class;

	class = widget->class;

	if (glade_widget_class_find_spec (class, "label") != NULL)
		property = glade_property_get_from_id (widget->properties,
						       "label");
	if (glade_widget_class_find_spec (class, "title") != NULL)
		property = glade_property_get_from_id (widget->properties,
						       "title");
	if (property != NULL)
		glade_property_set_string (property, widget->name);
}

static void
glade_widget_property_changed_cb (GtkWidget *w)
{
	GladeProperty *property;	
	GladeWidget *widget;

	widget = glade_widget_get_from_gtk_widget (w);
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	property = gtk_object_get_data (GTK_OBJECT (w),
					GLADE_MODIFY_PROPERTY_DATA);
	g_return_if_fail (GLADE_IS_PROPERTY (property));

	if (property->loading)
		return;

	glade_property_get_from_widget (property);

	glade_property_refresh (property);
}

static void
glade_widget_connect_edit_signals_with_class (GladeWidget *widget,
					      GladePropertyClass *class)
{
	GladeProperty *property;
	GList *list;

	property = glade_widget_get_property_from_class (widget, class);
	
	g_return_if_fail (GLADE_IS_PROPERTY (property));
	
	list = class->update_signals;
	for (; list != NULL; list = list->next) {
		gtk_signal_connect_after (GTK_OBJECT (widget->widget), list->data,
					  GTK_SIGNAL_FUNC (glade_widget_property_changed_cb),
					  property);
		gtk_object_set_data (GTK_OBJECT (widget->widget),
				     GLADE_MODIFY_PROPERTY_DATA, property);
	}
}


static void
glade_widget_connect_edit_signals (GladeWidget *widget)
{
	GladePropertyClass *class;
	GList *list;

	list = widget->class->properties;
	for (; list != NULL; list = list->next) {
		class = list->data;
		if (class->update_signals)
			glade_widget_connect_edit_signals_with_class (widget,
								      class);
	}
}

static gint
glade_widget_toplevel_delete_event_cb (GtkWidget *widget, gpointer not_used)
{
	gtk_widget_hide (widget);

	return TRUE;
}

static void
glade_widget_connect_other_signals (GladeWidget *widget)
{
	if (GLADE_WIDGET_IS_TOPLEVEL (widget)) {
		gtk_signal_connect (GTK_OBJECT (widget->widget), "delete_event",
				    GTK_SIGNAL_FUNC (glade_widget_toplevel_delete_event_cb),
				    NULL);
	}
}

#if 1
/* Sigh.
 * Fix, Fix, fix. Turn this off to see why this is here.
 * Add a gtkwindow and a gtkvbox to reproduce
 * Some werid redraw problems that i can't figure out.
 * Chema
 */
static gint
glade_widget_ugly_hack (gpointer data)
{
	GladeWidget *widget = data;
	
	gtk_widget_queue_resize (widget->widget);
	
	return FALSE;
}
#endif

static gboolean
glade_widget_create_gtk_widget (GladeWidget *glade_widget)
{
	GladeWidgetClass *class;
	GtkWidget *widget;
	GType type;

	class = glade_widget->class;
	type = g_type_from_name (class->name);
	
	if (!g_type_is_a (type, G_TYPE_OBJECT)) {
		gchar *text;
		g_warning ("Unknown type %s read from glade file.", class->name);
 		text = g_strdup_printf ("Error, class_new_widget not implemented [%s]\n", class->name);
		widget = gtk_label_new (text);
		g_free (text);
		return FALSE;
	} else {
		if (g_type_is_a (type, GTK_TYPE_WIDGET))
			widget = gtk_widget_new (type, NULL);
		else
			widget = g_object_new (type, NULL);
	}
	
	glade_widget->widget = widget;

	gtk_object_set_data (GTK_OBJECT (glade_widget->widget), GLADE_WIDGET_DATA_TAG, glade_widget);

	/* Ugly ugly hack. Don't even remind me about it. SEND ME PATCH !! and you'll
	 * gain 100 love points. 
	 * 100ms works for me, but since i don't know what the problem is i'll add a couple
	 * of more times the call for slower systems or systems under heavy workload, no harm
	 * done with an extra queue_resize.
	 * This was not needed for gtk 1.3.5 but needed for 1.3.7.
	 * To reproduce the problem, remove this timeouts and create a gtkwindow
	 * and then a gtkvbox inside it. It will not draw correctly.
	 * Chema
	 */
	if (class->post_create_function) {
		void (*pcf) (GObject *object);
		pcf = glade_gtk_get_function (class->post_create_function);
		if (!pcf)
			g_warning ("Could not find %s\n", class->post_create_function);
		else
			pcf (G_OBJECT (glade_widget->widget));

	}
	
	if (g_type_is_a (type, GTK_TYPE_WIDGET)) {
		gtk_timeout_add ( 100, glade_widget_ugly_hack, glade_widget);
		gtk_timeout_add ( 400, glade_widget_ugly_hack, glade_widget);
		gtk_timeout_add (1000, glade_widget_ugly_hack, glade_widget);
	}
	
	return TRUE;
}

static void
glade_widget_connect_signals (GladeWidget *widget)
{
	/* We can be a GtkObject. For example an adjustment. */
	if (!GTK_IS_WIDGET (widget->widget))
		return;
	
	glade_widget_connect_mouse_signals (widget);
	glade_widget_connect_draw_signals  (widget);
	glade_widget_connect_edit_signals  (widget);
	glade_widget_connect_other_signals (widget);
}

static GladeWidget *
glade_widget_new_full (GladeProject *project,
		       GladeWidgetClass *class,
		       GladeWidget *parent)
{
	GladeWidget *widget;

	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), NULL);

	widget = glade_widget_new (class, project);
	widget->name    = glade_widget_new_name (project, class);
	widget->parent  = parent;

	glade_packing_add_properties (widget);
	glade_widget_create_gtk_widget (widget);
	glade_project_add_widget (project, widget);

	if (parent)
		parent->children = g_list_prepend (parent->children, widget);

	glade_widget_set_contents (widget);
	glade_widget_connect_signals (widget);

	return widget;
}

/**
 * glade_widget_new_from_class_full:
 * @project: 
 * @class:
 * @parent: the parent of the new widget, should be NULL for toplevel widgets
 * 
 * Creates a new GladeWidget from a given class. Takes care of registering
 * the widget in the project, adding it to the views and quering the user
 * if needed.
 * 
 * Return Value: A newly creatred GladeWidget, NULL on user cancel or error	
 **/
static GladeWidget *
glade_widget_new_from_class_full (GladeWidgetClass *class, GladeProject *project, GladeWidget *parent)
{
	GladePropertyQueryResult *result = NULL;
	GladeWidget *widget;

	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), NULL);
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

	if (glade_widget_class_has_queries (class)) {
		result = glade_property_query_result_new ();
		if (glade_project_window_query_properties (class, result))
			return NULL;
	}

	widget = glade_widget_new_full (project, class, parent);

	/* If we are a container, add the placeholders */
	if (GLADE_WIDGET_CLASS_ADD_PLACEHOLDER (class))
		glade_placeholder_add_with_result (class, widget, result);
	
	if (result) 
		glade_property_query_result_destroy (result);

	glade_widget_set_default_options (widget);

	/* ->widget sometimes contains GtkObjects like a GtkAdjustment for example */
	if (GTK_IS_WIDGET (widget->widget))
		gtk_widget_show (widget->widget);

	return widget;
}

GladeWidget *
glade_widget_new_from_class (GladeWidgetClass *class, GladeWidget *parent)
{
	GladeProject *project;

	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET (parent), NULL);
	g_return_val_if_fail (GLADE_IS_PROJECT (parent->project), NULL);

	project = parent->project;

	return glade_widget_new_from_class_full (class, project, parent);
}
	
GladeWidget *
glade_widget_new_toplevel (GladeProject *project, GladeWidgetClass *class)
{
	GladeWidget *widget;
	
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), NULL);

	widget = glade_widget_new_from_class_full (class, project, NULL);
	
	return widget;
}


const gchar *
glade_widget_get_name (GladeWidget *widget)
{
	return widget->name;
}
		
GladeWidgetClass *
glade_widget_get_class (GladeWidget *widget)
{
	return widget->class;
}



/**
 * glade_widget_get_property_from_list:
 * @list: The list of GladeProperty
 * @class: The Class that we are trying to match with GladePropery
 * @silent: True if we shuold warn when a property is not included in the list
 * 
 * Give a list of GladeProperties find the one that has ->class = to @class.
 * This function recurses into child objects if needed.
 * 
 * Return Value: 
 **/
static GladeProperty *
glade_widget_get_property_from_list (GList *list, GladePropertyClass *class, gboolean silent)
{
	GladeProperty *property = NULL;

	if (list == NULL)
		return NULL;

	for (; list != NULL; list = list->next) {
		property = list->data;
		if (property->class == class)
			break;
		if (property->child != NULL) {
			property = glade_widget_get_property_from_list (property->child->properties,
									class, TRUE);
			if (property != NULL)
				break;
		}
	}

	if (list == NULL) {
		if (!silent)
			g_warning ("Could not find the GladeProperty %s:%s",
				   class->id,
				   class->name);
		return NULL;
	}

	return property;
}


/**
 * glade_widget_get_property_from_class:
 * @widget: 
 * @property_class: 
 * 
 * Given a glade Widget, it returns the property that correspons to @property_class
 * 
 * Return Value: 
 **/
GladeProperty *
glade_widget_get_property_from_class (GladeWidget *widget,
				      GladePropertyClass *property_class)
{
	GladeProperty *property;
	GList *list;

	list = widget->properties;

	property = glade_widget_get_property_from_list (list, property_class, FALSE);

	if (!property)
		g_warning ("Could not get property for widget %s of %s class\n",
			   widget->name, widget->class->name);

	return property;
}

/**
 * glade_widget_set_name:
 * @widget: 
 * @name: 
 * 
 * Sets the name of a widget
 **/
void
glade_widget_set_name (GladeWidget *widget, const gchar *name)
{
	if (widget->name)
		g_free (widget->name);
	widget->name = g_strdup (name);

	glade_project_widget_name_changed (widget->project, widget);
}



void
glade_widget_select (GladeWidget *widget)
{
	glade_project_selection_set (widget, TRUE);
}


static void
glade_widget_clear_draw_selection (GladeWidget *widget)
{
	GdkWindow *window;
	
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	
	if (widget->parent)
		window = widget->parent->widget->window;
	else
		window = widget->widget->window;
	
	gdk_window_clear_area (window,
			       widget->widget->allocation.x,
			       widget->widget->allocation.y,
			       widget->widget->allocation.width,
			       widget->widget->allocation.height);
	
	gtk_widget_queue_draw (widget->widget);
}

/**
 * glade_widget_flag_unselected:
 * @widget: 
 * 
 * Flag the widget as unselected
 **/
void
glade_widget_flag_unselected (GladeWidget *widget)
{
	g_return_if_fail (widget->selected);
	
	widget->selected = FALSE;

	glade_widget_clear_draw_selection (widget);
}

/**
 * glade_widget_flag_selected:
 * @widget: 
 * 
 * Flags the widget as selected
 **/
void
glade_widget_flag_selected (GladeWidget *widget)
{
	g_return_if_fail (!widget->selected);

	widget->selected = TRUE;
	gtk_widget_queue_draw (widget->widget);
}

static void
glade_widget_free (GladeWidget *widget)
{
	GList *list;
	
	widget->class = NULL;
	widget->project = NULL;
	widget->name = NULL;
	widget->widget = NULL;
	widget->parent = NULL;

	list = widget->properties;
	for (; list; list = list->next)
		glade_property_free (list->data);
	widget->properties = NULL;

	list = widget->signals;
	for (; list; list = list->next)
		glade_signal_free (list->data);
	widget->signals = NULL;
	
	list = widget->children;
	for (; list; list = list->next)
		glade_widget_free (list->data);
	widget->children = NULL;

	g_free (widget);
}

void
glade_widget_delete (GladeWidget *widget)
{
	GladeWidget *parent;

	g_return_if_fail (widget != NULL);

	glade_project_remove_widget (widget);

	parent = widget->parent;
	
	if (parent) {
		GladePlaceholder *placeholder;
		/* Replace the slot it was occuping with a placeholder */
		placeholder = glade_placeholder_new (widget->parent);
		if (widget->parent->class->placeholder_replace)
			widget->parent->class->placeholder_replace (widget->widget, GTK_WIDGET (placeholder), widget->parent->widget);

		/* Remove it from the parent's child list */
		parent->children = g_list_remove (parent->children,
						  widget);
		
	} else {
		gtk_widget_destroy (widget->widget);
	}

	glade_widget_free (widget);
}

void
glade_widget_cut (GladeWidget *widget)
{
	glade_implement_me ();
}

void
glade_widget_copy (GladeWidget *widget)
{
	glade_implement_me ();
}

void
glade_widget_paste (GladeWidget *widget)
{
	glade_implement_me ();
}

/**
 * glade_widget_new_from_class_name:
 * @class_name: 
 * @parent: 
 * 
 * Given a class name, it creates a GladeWidget
 * 
 * Return Value: the newly created GladeWidget, NULL on error
 **/
GladeWidget *
glade_widget_new_from_class_name (const gchar *name,
				  GladeWidget *parent)
{
	GladeWidgetClass *class;
	GladeWidget *widget;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET (parent), NULL);

	class = glade_widget_class_get_by_name (name);
	if (class == NULL)
		return NULL;

	widget = glade_widget_new_from_class (class, parent);

	return widget;
}
	


GladeXmlNode *
glade_widget_write (GladeXmlContext *context, GladeWidget *widget)
{
	GladeProperty *property;
	GladeXmlNode *node;
	GladeXmlNode *child;     /* This is the <widget name="foo" ..> tag */
	GladeXmlNode *child_tag; /* This is the <child> tag */
	GladeXmlNode *packing;
	GladeWidget *child_widget;
	GladeSignal *signal;
	GList *list;
	GList *list2;

	g_return_val_if_fail (GLADE_XML_IS_CONTEXT (context), NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

	node = glade_xml_node_new (context, GLADE_XML_TAG_WIDGET);

	glade_xml_node_set_property_string (node, GLADE_XML_TAG_CLASS, widget->class->name);
	glade_xml_node_set_property_string (node, GLADE_XML_TAG_ID, widget->name);

	/* Write the properties */
	list = widget->properties;
	for (; list != NULL; list = list->next) {
		property = list->data;
		if (property->class->packing)
			continue;
		child = glade_property_write (context, property);
		if (child == NULL)
			continue;
		glade_xml_node_append_child (node, child);
	}

	/* Signals */
	list = widget->signals;
	for (; list != NULL; list = list->next) {
		signal = list->data;
		child = glade_signal_write (context, signal);
		if (child == NULL)
			return NULL;
		glade_xml_node_append_child (node, child);
	}

	/* Children */
	list = widget->children;
	for (; list != NULL; list = list->next) {
		child_widget = list->data;
		child_tag = glade_xml_node_new (context, GLADE_XML_TAG_CHILD);
		glade_xml_node_append_child (node, child_tag);

		/* write the widget */
		child = glade_widget_write (context, child_widget);
		if (child == NULL)
			return NULL;
		glade_xml_node_append_child (child_tag, child);
		
		/* Append the packing properties */
		packing = glade_xml_node_new (context, GLADE_XML_TAG_PACKING);
		list2 = child_widget->properties;
		for (; list2 != NULL; list2 = list2->next) {
			GladeXmlNode *packing_property;
			property = list2->data;
			if (!property->class->packing) 
				continue;
			packing_property = glade_property_write (context, property);
			if (packing_property == NULL)
				continue;
			glade_xml_node_append_child (packing, packing_property);
		}
		glade_xml_node_append_child (child_tag, packing);
		/* */
		

	}

	return node;
}

static gboolean
glade_widget_apply_property_from_node (GladeXmlNode *node, GladeWidget *widget)
{
	GladeProperty *property;
	GValue *gvalue;
	gchar *value;
	gchar *id;

	id    = glade_xml_get_property_string_required (node, GLADE_XML_TAG_NAME, NULL);
	value = glade_xml_get_content (node);

	if (!value || !id)
		return FALSE;

	property = glade_property_get_from_id (widget->properties,
					       id);
	if (property == NULL) {
		g_warning ("Could not apply property from node. Id :%s\n",
			   id);
		return FALSE;
	}

	gvalue = glade_property_class_make_gvalue_from_string (property->class,
							       value);

	glade_property_set (property, gvalue);
		
	g_free (id);
	g_free (value);
	g_free (gvalue);

	return TRUE;
}

static gboolean
glade_widget_new_child_from_node (GladeXmlNode *node, GladeProject *project, GladeWidget *parent);

static GladeWidget *
glade_widget_new_from_node_real (GladeXmlNode *node, GladeProject *project, GladeWidget *parent)
{
	GladeWidgetClass *class;
	GladeXmlNode *child;
	GladeWidget *widget;
	gchar *class_name;

	if (!glade_xml_node_verify (node, GLADE_XML_TAG_WIDGET))
		return NULL;

	class_name = glade_xml_get_property_string_required (node, GLADE_XML_TAG_CLASS, NULL);
	if (!class_name)
		return NULL;
	class = glade_widget_class_get_by_name (class_name);
	if (!class)
		return NULL;
	
	widget = glade_widget_new_full (project, class,	parent);

	child =	glade_xml_node_get_children (node);
	for (; child != NULL; child = glade_xml_node_next (child)) {
		if (!glade_xml_node_verify_silent (child, GLADE_XML_TAG_PROPERTY))
			continue;

		if (!glade_widget_apply_property_from_node (child, widget)) {
			return NULL;
		}
	}

	/* If we are a container, add the placeholders */
	if (GLADE_WIDGET_CLASS_ADD_PLACEHOLDER (class))
		glade_placeholder_add (class, widget);
		
	child =	glade_xml_node_get_children (node);
	for (; child != NULL; child = glade_xml_node_next (child)) {
		if (!glade_xml_node_verify_silent (child, GLADE_XML_TAG_CHILD))
			continue;
		
		if (!glade_widget_new_child_from_node (child, project, widget)) {
			return NULL;
		}
	}

	gtk_widget_show_all (widget->widget);

	return widget;
}

static GHashTable *
glade_widget_properties_hash_from_node (GladeXmlNode *node)
{
	GladeXmlNode *child;
	GHashTable *hash;
	gchar *id;
	gchar *value;

	if (!glade_xml_node_verify (node, GLADE_XML_TAG_PACKING))
		return NULL;

	hash = g_hash_table_new_full (g_str_hash,
				      g_str_equal,
				      g_free,
				      g_free);
	
	child = glade_xml_node_get_children (node);
	for (; child != NULL; child = glade_xml_node_next (child)) {
		if (!glade_xml_node_verify (child, GLADE_XML_TAG_PROPERTY)) {
			return NULL;
		}

		id    = glade_xml_get_property_string_required (child, GLADE_XML_TAG_NAME, NULL);
		value = glade_xml_get_content (child);

		if (!value || !id) {
			g_warning ("Invalid property %s:%s\n", value, id);
			return NULL;
		}

		g_hash_table_insert (hash, id, value);
	}
	
	return hash;
}

static void
glade_widget_apply_property_from_hash_item (gpointer key, gpointer val, gpointer data)
{
	GladeProperty *property;
	GladeWidget *widget = data;
	GValue *gvalue;
	const gchar *id = key;
	const gchar *value = val;

	property = glade_property_get_from_id (widget->properties, id);
	g_assert (property);

	gvalue = glade_property_class_make_gvalue_from_string (property->class,
							       value);

	glade_property_set (property, gvalue);
}
	
static void
glade_widget_apply_properties_from_hash (GladeWidget *widget, GHashTable *properties)
{
	g_hash_table_foreach (properties, glade_widget_apply_property_from_hash_item, widget);
}

static gboolean
glade_widget_new_child_from_node (GladeXmlNode *node, GladeProject *project, GladeWidget *parent)
{
	GladeXmlNode *child_node;
	GladeWidget *child;
	GtkWidget *placeholder;
	GHashTable *packing_properties;

	if (!glade_xml_node_verify (node, GLADE_XML_TAG_CHILD))
		return FALSE;

	/* Get the packing properties */
	child_node = glade_xml_search_child_required (node, GLADE_XML_TAG_PACKING);
	if (!child_node)
		return FALSE;
	packing_properties = glade_widget_properties_hash_from_node (child_node);
	if (packing_properties == NULL)
		return FALSE;

	/* Get and create the widget */
	child_node = glade_xml_search_child_required (node, GLADE_XML_TAG_WIDGET);
	if (!child_node)
		return FALSE;
	child = glade_widget_new_from_node_real (child_node, project, parent);
	g_assert (child);

	/* Get the placeholder and replace it with the widget */
	placeholder = glade_placeholder_get_from_properties (parent, packing_properties);
	if (placeholder) {
		glade_placeholder_replace (placeholder, parent, child);
	} else {
		gtk_container_add (GTK_CONTAINER (parent->widget), child->widget);
	}

	/* Apply the properties and free the hash that contains them */
	glade_widget_apply_properties_from_hash (child, packing_properties);
	g_hash_table_destroy (packing_properties);
	
	return TRUE;
}

GladeWidget *
glade_widget_new_from_node (GladeXmlNode *node, GladeProject *project)
{
	return glade_widget_new_from_node_real (node, project, NULL);
}
