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
#include "glade-placeholder.h"


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
glade_widget_new (GladeProject *project, GladeWidgetClass *class, GtkWidget *gtk_widget, const gchar *name)
{
	GladeWidget *widget;

	widget = g_new0 (GladeWidget, 1);
	widget->name = g_strdup (name);
	widget->widget = gtk_widget;
	widget->project = project;
	widget->class = class;
	widget->properties = glade_property_list_new_from_widget_class (class, widget);
	widget->parent = NULL;
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
static GladeWidget *
glade_widget_get_from_gtk_widget (GtkWidget *widget)
{
	return gtk_object_get_data (GTK_OBJECT (widget), GLADE_WIDGET_DATA_TAG);
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
	g_print ("In find_child_at: %s X:%i Y:%i W:%i H:%i\n"
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
		g_print ("Found it!\n");
#endif	
		data->found_child = widget;
	}
}

/**
 * glade_widget_get_from_event_widget:
 * @event_widget: 
 * @event_glade_widget: 
 * @event: 
 * 
 * Returns the real widget that was "clicked over" for a given event (cooridantes) and a widget
 * For example, when a label is clicked the button press event is triggered for its parent, this
 * function takes the event and the widget that got the event and returns the real widget that was
 * clicked
 * 
 * Return Value: 
 **/
static GladeWidget *
glade_widget_get_from_event_widget (GtkWidget *event_widget, GladeWidget *event_glade_widget, GdkEventButton *event)
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
	g_print ("Window [%d,%d]\n", win_x, win_y);
	g_print ("\n\nWe want to find the real widget that was clicked at %d,%d\n", x, y);
	g_print ("The widget that received the event was \"%s\" a \"%s\" [%d]\n"
		 "The REAL widget is %s\n",
		 glade_widget_get_name (event_glade_widget),
		 gtk_widget_get_name (event_widget),
		 GPOINTER_TO_INT (event_widget),
		 glade_widget_get_name (real_event_glade_widget));
#endif	

	parent_window = event_widget->parent ? event_widget->parent->window : event_widget->window;
	while (window && window != parent_window) {
		gdk_window_get_position (window, &win_x, &win_y);
#ifdef DEBUG	
		g_print ("	  adding X:%d Y:%d - We now have : %d %d\n",
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
		g_print ("\"%s\" is a container, check inside each child\n",
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
				g_print ("Temp was not a GladeWidget, it was a %s\n",
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
	g_print ("We found a \"%s\", child at %d,%d\n",
		 gtk_widget_get_name (found->widget), data.x, data.y);
#endif	
	return found;
}

static gboolean
glade_widget_button_press (GtkWidget *event_widget, GdkEventButton *event, GladeWidget *event_glade_widget)
{
	GladeWidget *glade_widget;

#ifdef DEBUG	
	g_print ("button press for a %s\n", event_glade_widget->class->name);
#endif	

	glade_widget = glade_widget_get_from_event_widget (event_widget, event_glade_widget, event);

	if (glade_widget)
		glade_project_selection_set (glade_widget->project, glade_widget, TRUE);

#ifdef DEBUG	
	g_print ("The widget found was a %s\n", glade_widget->class->name);
#endif	
	
	return TRUE;
}
#undef DEBUG

static gboolean
glade_widget_button_release (GtkWidget *widget, GdkEventButton *event, GladeWidget *glade_widget)
{
#ifdef DEBUG	
	g_print ("button release\n");
#endif
	
	return TRUE;
}


#if 1
/**
 * glade_widget_set_default_options:
 * @widget: 
 * 
 * Takes care of applying the default values to a newly created widget
 **/
static void
glade_widget_set_default_options (GladeWidget *widget)
{
	GladeProperty *property;
	GList *list;

	list = widget->properties;
	for (; list != NULL; list = list->next) {
		property = list->data;
		switch (property->class->type) {
		case GLADE_PROPERTY_TYPE_BOOLEAN:
			glade_property_changed_boolean (property,
							glade_property_get_boolean (property));
			break;
		case GLADE_PROPERTY_TYPE_FLOAT:
			glade_property_changed_float (property,
						      glade_property_get_float (property));
			break;
		case GLADE_PROPERTY_TYPE_INTEGER:
			glade_property_changed_integer (property,
							glade_property_get_integer (property));
			break;
		case GLADE_PROPERTY_TYPE_TEXT:
			glade_property_changed_text (property,
						     glade_property_get_text (property));
			break;
		case GLADE_PROPERTY_TYPE_CHOICE:
			glade_property_changed_choice (property,
						       glade_property_get_choice (property));
			break;
		default:
			g_warning ("Implement set default for this type\n");
			break;
		}
	}
	       
}
#endif

static GladeWidget *
glade_widget_register (GladeProject *project, GladeWidgetClass *class, GtkWidget *gtk_widget, const gchar *name, GladeWidget *parent)
{
	GladeWidget *glade_widget;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

	glade_widget = glade_widget_new (project, class, gtk_widget, name);

	glade_widget->parent = parent;
	if (parent)
		parent->children = g_list_prepend (parent->children, glade_widget);

	return glade_widget;
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

static void
glade_widget_expose_event_cb (GtkWidget *widget, GdkEventExpose *event,
			      GladeWidget *glade_widget)
{
	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_assert (widget == glade_widget->widget);

	if (glade_widget->selected)
		glade_widget_draw_selection_nodes (glade_widget);
}

static void
glade_widget_connect_draw_signals (GladeWidget *glade_widget)
{
	GtkWidget *widget = glade_widget->widget;

	gtk_signal_connect_after (GTK_OBJECT (widget), "expose_event",
				  GTK_SIGNAL_FUNC (glade_widget_expose_event_cb),
				  glade_widget);

	/* We need to turn doble buffering off since we are going
	 * to draw the selection nodes over the widget
	 */
	gtk_widget_set_double_buffered (widget, FALSE);
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
			    GTK_SIGNAL_FUNC (glade_widget_button_press), glade_widget);
	gtk_signal_connect (GTK_OBJECT (widget), "button_release_event",
			    GTK_SIGNAL_FUNC (glade_widget_button_release), glade_widget);
}

static GladeWidget *
glade_widget_create_gtk_widget (GladeProject *project,
				GladeWidgetClass *class,
				GladeWidget *parent)
{
	GladeWidget *glade_widget;
	GType type;
	GtkWidget *widget;
	gchar *name;

	name = glade_widget_new_name (project, class);

	type = g_type_from_name (class->name);
	if (! g_type_is_a (type, GTK_TYPE_WIDGET)) {
		gchar *text;
		g_warning ("Unknown type %s read from glade file.", class->name);
 		text = g_strdup_printf ("Error, class_new_widget not implemented [%s]\n", class->name);
		widget = gtk_label_new (text);
		g_free (text);
	} else {
		widget = gtk_widget_new (type, NULL);
	}

	glade_widget = glade_widget_register (project, class, widget, name, parent);
	
	glade_widget_set_default_options (glade_widget);
	/* We need to be able to get to the GladeWidget * from a GtkWidget * */
	gtk_object_set_data (GTK_OBJECT (glade_widget->widget), GLADE_WIDGET_DATA_TAG, glade_widget);

	
	/* FIXME */
	if ((strcmp (class->name, "GtkLabel") == 0) ||
	    (strcmp (class->name, "GtkButton") == 0)) {
		GladeProperty *property;
		
		property = glade_property_get_from_name (glade_widget->properties,
							 "Label");
		if (property != NULL)
			glade_property_changed_text (property, name);
		else
			g_warning ("Could not set the label to the widget name\n");
	}

	glade_widget_connect_mouse_signals (glade_widget);
	glade_widget_connect_draw_signals (glade_widget);
	
	glade_project_add_widget (project, glade_widget);
	
	g_free (name);
	
	return glade_widget;
}

/**
 * glade_widget_new_from_class:
 * @project: 
 * @class:
 * @parent: the parent of the new widget, should be NULL for toplevel widgets
 * 
 * Creates a new GladeWidget from a given class. Takes care of registering
 * the widget in the project, adding it to the views and quering the user
 * if necesary.
 * 
 * Return Value: A newly creatred GladeWidget, NULL on user cancel or error	
 **/
GladeWidget *
glade_widget_new_from_class (GladeProject *project, GladeWidgetClass *class, GladeWidget *parent)
{
	GladePropertyQueryResult *result = NULL;
	GladeWidget *glade_widget;

	g_return_val_if_fail (project != NULL, NULL);

	if (glade_widget_class_has_queries (class)) {
		result = glade_property_query_result_new ();
		if (glade_project_window_query_properties (class, result))
			return NULL;
	}

	glade_widget = glade_widget_create_gtk_widget (project, class, parent);

	if (GLADE_WIDGET_CLASS_ADD_PLACEHOLDER (class))
		glade_placeholder_add (class, glade_widget, result);

	gtk_widget_show (glade_widget->widget);
	
	if (result) 
		glade_property_query_result_destroy (result);

	return glade_widget;
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
	GladeProperty *property = NULL;
	GList *list;
	
	list = widget->properties;
	for (; list != NULL; list = list->next) {
		property = list->data;
		if (property->class == property_class)
			break;
	}
	if (list == NULL) {
		g_warning ("Could not find the GladeProperty to load\n");
		return NULL;
	}

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
glade_widget_unselect (GladeWidget *widget)
{
	g_return_if_fail (widget->selected);
	
	widget->selected = FALSE;
	gtk_widget_queue_draw (widget->widget);
}

void
glade_widget_select (GladeWidget *widget)
{
	g_return_if_fail (!widget->selected);

	widget->selected = TRUE;
	gtk_widget_queue_draw (widget->widget);
}
