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
	if (event->button == 1)
		glade_project_selection_set (glade_widget, TRUE);
	else if (event->button == 3)
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
		 * "position" packing property
		 */
		if (property->value && (strcmp (property->value, GLADE_GET_DEFAULT_FROM_WIDGET) == 0)) {
			gchar *temp;
			if (!property->class->get_function) {
				g_warning ("Class %s is get_default_from_widget but no get function specified",
					   property->class->name);
				temp = g_strdup ("Error");
			} else
				temp = (*property->class->get_function) (G_OBJECT (widget->widget));
			g_free (property->value);
			property->value = temp;
			continue;
		}

		property->loading = TRUE;
			
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
		case GLADE_PROPERTY_TYPE_DOUBLE:
			glade_property_changed_double (property,
						       glade_property_get_double (property));
			break;
		case GLADE_PROPERTY_TYPE_TEXT:
			glade_property_changed_text (property,
						     glade_property_get_text (property));
			break;
		case GLADE_PROPERTY_TYPE_CHOICE:
			glade_property_changed_choice (property,
						       glade_property_get_choice (property));
			break;
		case GLADE_PROPERTY_TYPE_OBJECT:
			g_print ("Set adjustment\n");
#if 1
			g_print ("Set directly \n");
			glade_widget_set_default_options_real (property->child, packing);
			gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (property->widget->widget),
							GTK_ADJUSTMENT (property->child));
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

static GladeWidget *
glade_widget_register (GladeProject *project, GladeWidgetClass *class, GtkWidget *gtk_widget, const gchar *name, GladeWidget *parent)
{
	GladeWidget *glade_widget;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

	glade_widget = glade_widget_new (project, class, gtk_widget, name);
	glade_widget->parent = parent;

	glade_packing_add_properties (glade_widget);
	
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
		glade_property_changed_text (property, widget->name);
}

static gchar *
glade_property_get (GladeProperty *property)
{
	gchar *resp;
	
	if (property->class->get_function)
		resp = (*property->class->get_function)
			(G_OBJECT (property->widget->widget));
	else {
		gboolean bool;
		switch (property->class->type) {
		case GLADE_PROPERTY_TYPE_BOOLEAN:
			gtk_object_get (GTK_OBJECT (property->widget->widget),
					property->class->id,
					&bool,
					NULL);
			resp = bool ?
				g_strdup (GLADE_TAG_TRUE) :
				g_strdup (GLADE_TAG_FALSE);
			break;
		default:
			resp = NULL;
			break;
		}
	}

	return resp;
}

static void
glade_widget_property_changed_cb (GtkWidget *w)
{
	GladeProperty *property;	
	GladeWidget *widget;
	gchar *new = NULL;
	
	widget = glade_widget_get_from_gtk_widget (w);
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	property = gtk_object_get_data (GTK_OBJECT (w),
					GLADE_MODIFY_PROPERTY_DATA);
	g_return_if_fail (GLADE_IS_PROPERTY (property));

	if (property->loading)
		return;

	new = glade_property_get (property);

	switch (property->class->type) {
	case GLADE_PROPERTY_TYPE_TEXT:
		glade_property_changed_text (property, new);
		break;
	case GLADE_PROPERTY_TYPE_BOOLEAN:
		if (new && strcmp (new, GLADE_TAG_TRUE) == 0)
			glade_property_changed_boolean (property, TRUE);
		else
			glade_property_changed_boolean (property, FALSE);
		break;
	default:
		break;
	}

	g_free (new);
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

static GladeWidget *
glade_widget_create_gtk_widget (GladeProject *project,
				GladeWidgetClass *class,
				GladeWidget *parent)
{
	GladeWidget *glade_widget;
	GType type;
	GtkWidget *widget;
	gchar *name;


	type = g_type_from_name (class->name);
	if (! g_type_is_a (type, G_TYPE_OBJECT)) {
		gchar *text;
		g_warning ("Unknown type %s read from glade file.", class->name);
 		text = g_strdup_printf ("Error, class_new_widget not implemented [%s]\n", class->name);
		widget = gtk_label_new (text);
		g_free (text);
	} else {
		if (g_type_is_a (type, GTK_TYPE_WIDGET))
			widget = gtk_widget_new (type, NULL);
		else
			widget = g_object_new (type, NULL);
	}

	name = glade_widget_new_name (project, class);
	glade_widget = glade_widget_register (project, class, widget, name, parent);
	g_free (name);

	gtk_object_set_data (GTK_OBJECT (glade_widget->widget), GLADE_WIDGET_DATA_TAG, glade_widget);

	glade_project_add_widget (project, glade_widget);

	/* We can be a GtkObject for example, like an adjustment */
	if (!g_type_is_a (type, GTK_TYPE_WIDGET))
		return glade_widget;
		
	glade_widget_set_contents (glade_widget);
	glade_widget_connect_mouse_signals (glade_widget);
	glade_widget_connect_draw_signals (glade_widget);
	glade_widget_connect_edit_signals (glade_widget);
	glade_widget_connect_other_signals (glade_widget);

	if (GTK_IS_TABLE (glade_widget->widget)) {
		g_print ("Is table\n");
		gtk_table_set_homogeneous (GTK_TABLE (glade_widget->widget),
					   FALSE);
	}
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
 * if needed.
 * 
 * Return Value: A newly creatred GladeWidget, NULL on user cancel or error	
 **/
static GladeWidget *
glade_widget_new_from_class_full (GladeWidgetClass *class, GladeProject *project, GladeWidget *parent)
{
	GladePropertyQueryResult *result = NULL;
	GladeWidget *glade_widget;

	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), NULL);
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);
		
	if (glade_widget_class_has_queries (class)) {
		result = glade_property_query_result_new ();
		if (glade_project_window_query_properties (class, result))
			return NULL;
	}

	glade_widget = glade_widget_create_gtk_widget (project, class, parent);

	/* IF we are a container, add the placeholders */
	if (GLADE_WIDGET_CLASS_ADD_PLACEHOLDER (class))
		glade_placeholder_add (class, glade_widget, result);

	/* ->widget sometimes contains GtkObjects like a GtkAdjustment for example */
	if (GTK_IS_WIDGET (glade_widget->widget))
		gtk_widget_show (glade_widget->widget);
	
	if (result) 
		glade_property_query_result_destroy (result);

	glade_widget_set_default_options (glade_widget);
	
	return glade_widget;
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
			g_warning ("Could not find the GladeProperty to load from -%s-",
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


/* I don't think this flag is beeing used at all, but for now it is queueing
 * redraws. Chema.
 */
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
	gtk_widget_queue_draw (widget->widget);
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
			return NULL;
		glade_xml_append_child (node, child);
	}

	/* Signals */
	list = widget->signals;
	for (; list != NULL; list = list->next) {
		signal = list->data;
		child = glade_signal_write (context, signal);
		if (child == NULL)
			return NULL;
		glade_xml_append_child (node, child);
	}

	/* Children */
	list = widget->children;
	for (; list != NULL; list = list->next) {
		child_widget = list->data;
		child = glade_widget_write (context, child_widget);
		if (child == NULL)
			return NULL;
		child_tag = glade_xml_node_new (context, GLADE_XML_TAG_CHILD);
		glade_xml_append_child (node, child_tag);
		glade_xml_append_child (child_tag, child);

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
				return NULL;
			glade_xml_append_child (packing, packing_property);
		}
		glade_xml_append_child (child_tag, packing);
		/* */
	}

	return node;
}

	
