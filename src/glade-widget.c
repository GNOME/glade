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
#include "glade-parameter.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-popup.h"
#include "glade-signal.h"
#include "glade-clipboard.h"
#include "glade-command.h"
#include "glade-debug.h"
#include "glade-utils.h"
#include <gdk/gdkkeysyms.h>

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
	return glade_project_new_widget_name (project, class->generic_name);
}

 /**
 * Returns a list of GladeProperties from a list of 
 * GladePropertyClass.
 */
static GList *
glade_widget_properties_from_list (GList *list, GladeWidget *widget)
{
	GladePropertyClass *property_class;
	GladeProperty *property;
	GList *new_list = NULL;

	for (; list; list = list->next)
	{
		property_class = list->data;
		property = glade_property_new (property_class, widget);
		if (!property)
			continue;

		new_list = g_list_prepend (new_list, property);
	}

	new_list = g_list_reverse (new_list);

	return new_list;
}

 /**
 * Returns a list of GladeProperties from a list of 
 * GladePropertyClass.
 */
static GList *
glade_widget_create_packing_properties (GladeWidget *container, GladeWidget *widget)
{
	GladePropertyClass *property_class;
	GladeProperty *property;
	GList *list = container->class->child_properties;
	GList *new_list = NULL;
	GList *ancestor_list;

	for (; list; list = list->next)
	{
		property_class = list->data;

		if (!container->class->child_property_applies (container->widget, widget->widget, property_class->id))
			continue;

		property = glade_property_new (property_class, widget);
		if (!property)
			continue;

		new_list = g_list_prepend (new_list, property);
	}

	new_list = g_list_reverse (new_list);
	if (container->widget->parent != NULL)
	{
		ancestor_list = glade_widget_create_packing_properties (glade_widget_get_from_gtk_widget (container->widget->parent),
									widget);
		new_list = g_list_concat (new_list, ancestor_list);
	}

	return new_list;
}

static void
glade_widget_free_signals (gpointer value)
{
	GList *signals = (GList*) value;

	if (signals)
	{
		g_list_foreach (signals, (GFunc) glade_signal_free, NULL);
		g_list_free (signals);
	}
}

/**
 * glade_widget_new:
 * @class: The GladeWidgeClass of the GladeWidget
 * 
 * Allocates a new GladeWidget structure and fills in the required memebers.
 * Note that the GtkWidget member is _not_ set.
 * 
 * Return Value: 
 **/
static GladeWidget *
glade_widget_new (GladeWidgetClass *class, GladeProject *project)
{
	GladeWidget *widget;

	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), NULL);

	widget = g_new0 (GladeWidget, 1);
	widget->project = project;
	widget->name    = glade_widget_new_name (project, class);
	widget->internal = NULL;
	widget->widget   = NULL;
	widget->class    = class;
	widget->properties = glade_widget_properties_from_list (class->properties, widget);
	/* we don't have packing properties until we container add the widget */
	widget->packing_properties = NULL;
	widget->signals  = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free, (GDestroyNotify) glade_widget_free_signals);

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

	glade_widget = g_object_get_data (G_OBJECT (widget), GLADE_WIDGET_DATA_TAG);

	return glade_widget;
}

/**
 * glade_widget_get_parent:
 * @widget
 *
 * Convenience function to retrieve the GladeWidget associated
 * to the parent of @widget.
 * Returns NULL if it is a toplevel.
 **/
GladeWidget *
glade_widget_get_parent (GladeWidget *widget)
{
	GladeWidget *parent = NULL;
	GtkWidget *parent_widget;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

	if (GTK_WIDGET_TOPLEVEL (widget->widget))
		return NULL;

	parent_widget = gtk_widget_get_parent (widget->widget);
	g_return_val_if_fail (parent_widget != NULL, NULL);

	parent = glade_widget_get_from_gtk_widget (parent_widget);
	g_return_val_if_fail (GLADE_IS_WIDGET (parent), NULL);

	return parent;
}

/* A temp data struct that we use when looking for a widget inside a container
 * we need a struct, because the forall can only pass one pointer
 */
typedef struct {
	gint x;
	gint y;
	GtkWidget *found_child;
} GladeFindInContainerData;

static void
glade_widget_find_inside_container (GtkWidget *widget, gpointer data_in)
{
	GladeFindInContainerData *data = data_in;

	g_debug(("In find_child_at: %s X:%i Y:%i W:%i H:%i\n"
		 "  so this means that if we are in the %d-%d , %d-%d range. We are ok\n",
		 gtk_widget_get_name (widget),
		 widget->allocation.x, widget->allocation.y,
		 widget->allocation.width, widget->allocation.height,
		 widget->allocation.x, widget->allocation.x + widget->allocation.width,
		 widget->allocation.y, widget->allocation.y + widget->allocation.height));

	/* Notebook pages are visible but not mapped if they are not showing. */
	/* We should not consider objects that doesn't have the GLADE_WIDGET_DATA_TAG,
	   that way we would only take in account widgets modifiables by the user
	   (for instance, the widgets that are inside a font dialog are not modifiables
	   by the user) */
	if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget)
	    && (widget->allocation.x <= data->x)
	    && (widget->allocation.y <= data->y)
	    && (widget->allocation.x + widget->allocation.width >= data->x)
	    && (widget->allocation.y + widget->allocation.height >= data->y)
	    && g_object_get_data(G_OBJECT (widget), GLADE_WIDGET_DATA_TAG))
	{
		g_debug(("Found it!\n"));
		data->found_child = widget;
	}
}

/**
 * glade_widget_get_from_event_widget:
 * @event_widget: 
 * @event: 
 * 
 * Returns the real widget that was "clicked over" for a given event (coordinates) and a widget
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
	
	window = event->window;
	x = (int) (event->x + 0.5);
	y = (int) (event->y + 0.5);
	gdk_window_get_position (event_widget->window, &win_x, &win_y);

	/*
	g_debug(("Window [%d,%d]\n", win_x, win_y));
	g_debug(("\n\nWe want to find the real widget that was clicked at %d,%d\n", x, y));
	g_debug(("The widget that received the event was \"%s\" a \"%s\" [%d]\n",
		 "",
		 gtk_widget_get_name (event_widget),
		 GPOINTER_TO_INT (event_widget)));
	*/
	
	parent_window = event_widget->parent ? event_widget->parent->window : event_widget->window;
	while (window && window != parent_window) {
		
		gdk_window_get_position (window, &win_x, &win_y);
		/* g_debug(("	  adding X:%d Y:%d - We now have : %d %d\n",
		   win_x, win_y, x + win_x, y + win_y)); */
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
		g_debug(("\"%s\" is a container, check inside each child\n",
			 gtk_widget_get_name (temp)));
		data.found_child = NULL;
		gtk_container_forall (GTK_CONTAINER (temp),
				      (GtkCallback) glade_widget_find_inside_container, &data);
		if (data.found_child)
		{
			GladeWidget *temp_glade_widget;
			temp = data.found_child;
			if ((temp_glade_widget = glade_widget_get_from_gtk_widget (temp)) != NULL)
			{
				found = temp_glade_widget;
				g_assert (found->widget == data.found_child);
			}
			else
			{
				g_debug(("Temp was not a GladeWidget, it was a %s\n",
					 gtk_widget_get_name (temp)));
			}

			data.found_child = NULL;
		}
		else
			/* there are no more children.  found contains the deepest GladeWidget */
			break;

	}
#ifdef DEBUG	
	if (!found)
	{
		g_warning ("We could not find the widget %s\n",
			   gtk_widget_get_name (event_widget));
		return NULL;
	}
#else
	g_return_val_if_fail (found != NULL, NULL);
#endif
	/*	
	g_debug(("We found a \"%s\", child at %d,%d\n",
		 gtk_widget_get_name (found->widget), data.x, data.y));
	*/
	return found;
}

static gboolean
glade_widget_button_press (GtkWidget *widget,
			   GdkEventButton *event,
			   gpointer not_used)
{
	GladeWidget *glade_widget;

	glade_widget = glade_widget_get_from_event_widget (widget, event);
	if (!glade_widget)
	{
		g_warning ("Button press event but the gladewidget was not found\n");
		return FALSE;
	}

	widget = glade_widget->widget;
	/* make sure to grab focus, since we may stop default handlers */
	if (GTK_WIDGET_CAN_FOCUS (widget) && !GTK_WIDGET_HAS_FOCUS (widget))
		gtk_widget_grab_focus (widget);

	if (event->button == 1 && event->type == GDK_BUTTON_PRESS)
	{
		/* if it's already selected don't stop default handlers, e.g. toggle button */
		if (glade_util_has_nodes (widget))
			return FALSE;

		glade_project_selection_set (glade_widget->project, widget, TRUE);

		return TRUE;
	}
	else if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
	{
		glade_popup_widget_pop (glade_widget, event);

		return TRUE;
	}

	return FALSE;
}

static void
glade_widget_connect_mouse_signals (GladeWidget *glade_widget)
{
	GtkWidget *widget = glade_widget->widget;

	if (!GTK_WIDGET_NO_WINDOW (widget))
	{
		gtk_widget_add_events (widget, GDK_BUTTON_PRESS_MASK |
					       GDK_BUTTON_RELEASE_MASK);
	}

	g_signal_connect (G_OBJECT (widget), "button_press_event",
			  G_CALLBACK (glade_widget_button_press), NULL);
}

static gboolean
glade_widget_key_press (GtkWidget *event_widget,
			GdkEventKey *event,
			GladeWidget *glade_widget)
{
	g_return_val_if_fail (GTK_IS_WIDGET (event_widget), FALSE);
	g_return_val_if_fail (GLADE_IS_WIDGET (glade_widget), FALSE);

	/* We will delete all the selected items */
	if (event->keyval == GDK_Delete)
	{
		glade_util_delete_selection (glade_widget->project);
		return TRUE;
	}

	return FALSE;
}

static void
glade_widget_connect_keyboard_signals (GladeWidget *glade_widget)
{
	GtkWidget *widget = glade_widget->widget;

	if (!GTK_WIDGET_NO_WINDOW(widget))
		gtk_widget_add_events (widget, GDK_KEY_PRESS_MASK);

	g_signal_connect (G_OBJECT (widget), "key_press_event",
			  G_CALLBACK (glade_widget_key_press), glade_widget);
}

/**
 * glade_widget_set_contents:
 * @widget: 
 * 
 * Loads the name of the widget. For example a button will have the
 * "button1" text in it, or a label will have "label4". right after
 * it is created.
 **/
void
glade_widget_set_contents (GladeWidget *widget)
{
	GladeProperty *property = NULL;

	if (glade_widget_class_has_property (widget->class, "label"))
		property = glade_widget_get_property_by_id (widget, "label", FALSE);
	if (glade_widget_class_has_property (widget->class, "title"))
		property = glade_widget_get_property_by_id (widget, "title", FALSE);

	if (property)
	{
		GValue value = { 0, };

		g_value_init (&value, G_TYPE_STRING);
		g_value_set_string (&value, widget->name);
		glade_property_set (property, &value);
		g_value_unset (&value);
	}
}

static gboolean
glade_widget_popup_menu (GtkWidget *widget, GladeWidget *glade_widget)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (glade_widget), FALSE);

	glade_popup_widget_pop (glade_widget, NULL);

	return TRUE;
}

static void
glade_widget_connect_other_signals (GladeWidget *widget)
{
	if (GTK_WIDGET_TOPLEVEL (widget->widget))
	{
		g_signal_connect (G_OBJECT (widget->widget), "delete_event",
				  G_CALLBACK (gtk_widget_hide_on_delete), NULL);
	}

	g_signal_connect (G_OBJECT (widget->widget), "popup_menu",
			  G_CALLBACK (glade_widget_popup_menu), widget);
}

/**
 * Free the GladeWidget associated to a widget. Note that this is
 * connected to the destroy event of the corresponding GtkWidget so
 * it does not need to recurse, since Gtk takes care of destroying
 * all the children.
 * You should not be calling this function explicitely, if needed
 * just destroy widget->widget.
 */
static void
glade_widget_free (GladeWidget *widget)
{
	widget->class = NULL;
	widget->project = NULL;
	widget->widget = NULL;

	g_free (widget->name);
	g_free (widget->internal);

	g_list_foreach(widget->properties, (GFunc) glade_property_free, NULL);
	g_list_free (widget->properties);
	g_list_foreach(widget->packing_properties, (GFunc) glade_property_free, NULL);
	g_list_free (widget->packing_properties);
	g_hash_table_foreach_remove (widget->signals, (GHRFunc) glade_widget_free_signals, NULL);
	g_hash_table_destroy (widget->signals);

	g_free (widget);
}

static GtkWidget *
glade_widget_create_gtk_widget (GladeWidgetClass *class)
{
	GtkWidget *widget;
	GType type;

	type = g_type_from_name (class->name);

	if (!g_type_is_a (type, G_TYPE_OBJECT)) {
		g_warning ("Create GtkWidget for type %s not implemented", class->name);
		return NULL;
	}

	if (g_type_is_a (type, GTK_TYPE_WIDGET))
		widget = gtk_widget_new (type, NULL);
	else
		widget = g_object_new (type, NULL);

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
	/* That hack makes glade segfault if you delete glade_widget before 1 sec has ellapsed
	 * since its creation (glade_widget will point to garbage).  With gtk 1.3.12 everything
	 * seems to be ok without the timeouts, so I will remove it by now.
	 * Cuenca
	 */

	return widget;
}

void
glade_widget_connect_signals (GladeWidget *widget)
{
	glade_widget_connect_mouse_signals (widget);
	glade_widget_connect_keyboard_signals (widget);
	glade_widget_connect_other_signals (widget);
}

/**
 * glade_widget_set_packing_properties:
 * @widget:
 * @container_class:
 *
 * Generates the packing_properties list of the widget, given
 * the class of the container we are adding the widget to.
 * If the widget already has packing_properties, but the container
 * has changed, the current list is freed and replaced.
 */
void
glade_widget_set_packing_properties (GladeWidget *widget,
				     GladeWidget *container)
{
	GList *list;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_WIDGET (container));

	/* toplevels do not have packing properties, so should not be
	 * passed to this function.
	 */
	g_return_if_fail (!GTK_WIDGET_TOPLEVEL (widget->widget));

	g_list_foreach(widget->packing_properties, (GFunc) glade_property_free, NULL);
	g_list_free (widget->packing_properties);
	widget->packing_properties = glade_widget_create_packing_properties (container, widget);

	/* update the values of the properties to the ones we get from gtk */
	for (list = widget->packing_properties; list; list = list->next)
	{
		GladeProperty *property = list->data;

		g_value_reset (property->value);
		if (gtk_container_class_find_child_property (G_OBJECT_GET_CLASS (container->widget), property->class->id))
			gtk_container_child_get_property (GTK_CONTAINER (container->widget),
							  widget->widget,
							  property->class->id,
							  property->value);
	}
}

static void
glade_widget_associate_with_gtk_widget (GladeWidget *glade_widget,
				        GtkWidget *gtk_widget)
{
	g_return_if_fail (GLADE_IS_WIDGET (glade_widget));
	g_return_if_fail (GTK_IS_WIDGET (gtk_widget));

	if (glade_widget->widget) {
		g_warning ("Failed to associate GladeWidget, "
			   "it is already associated with another GtkWidget");
		return;
	}

	glade_widget->widget = gtk_widget;
	g_object_set_data (G_OBJECT (gtk_widget), GLADE_WIDGET_DATA_TAG, glade_widget);

	/* make sure that when the GtkWidget is destroyed the GladeWidget is freed */
	g_signal_connect_swapped (G_OBJECT (gtk_widget), "destroy",
				  G_CALLBACK (glade_widget_free), G_OBJECT (glade_widget));

	glade_widget_connect_signals (glade_widget);
}

static GladeWidget *
glade_widget_new_full (GladeWidgetClass *class,
		       GladeProject *project)
{
	GladeWidget *widget;
	GtkWidget *gtk_widget;

	widget = glade_widget_new (class, project);
	gtk_widget = glade_widget_create_gtk_widget (class);
	if (!widget || !gtk_widget)
		return NULL;

	glade_widget_associate_with_gtk_widget (widget, gtk_widget);

	/* associate internal children and set sane defaults */
	if (class->post_create_function) {
		class->post_create_function (G_OBJECT (gtk_widget));
	}

	return widget;
}

/**
 * glade_widget_new_for_internal_child
 * @class
 * @parent
 * @widget
 * @internal
 * 
 * Creates, fills and associate a GladeWidget to the GtkWidget of
 * internal children.
 **/
GladeWidget *
glade_widget_new_for_internal_child (GladeWidgetClass *class,
				     GladeWidget *parent,
				     GtkWidget *widget,
				     const gchar *internal)
{
	GladeWidget *glade_widget;

	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET (parent), NULL);
	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
	g_return_val_if_fail (internal != NULL, NULL);

	glade_widget = glade_widget_new (class, parent->project);
	if (!glade_widget)
		return NULL;

	glade_widget_associate_with_gtk_widget (glade_widget, widget);
	glade_widget_set_packing_properties (glade_widget, parent);
	glade_widget->internal = g_strdup (internal);

	return glade_widget;
}
/**
 * Temp struct to hold the results of a query.
 * The keys of the hashtable are the GladePropertyClass->id , while the
 * values are the GValues the user sets for the property.
 */
typedef struct {
	GHashTable *hash;
} GladePropertyQueryResult;

static void
gpq_result_free_value (GValue *value)
{
	if (G_VALUE_TYPE (value) != 0)
		g_value_unset (value);
	g_free (value);
}

static GladePropertyQueryResult *
glade_property_query_result_new (void)
{
	GladePropertyQueryResult *result;

	result = g_new0 (GladePropertyQueryResult, 1);
	result->hash = g_hash_table_new_full (g_str_hash,
					      g_str_equal,
					      NULL,
					      (GDestroyNotify) gpq_result_free_value);

	return result;
}

static void
glade_property_query_result_destroy (GladePropertyQueryResult *result)
{
	g_return_if_fail (result != NULL);

	g_hash_table_destroy (result->hash);
	result->hash = NULL;
	
	g_free (result);
}

static void
glade_widget_query_set_result (gpointer _key,
			       gpointer _value,
			       gpointer _data)
{
	gchar *property_id = _key;
	GtkWidget *widget = _value;
	GladePropertyQueryResult *result = _data;

	gint num;
	GValue *value = g_new0 (GValue, 1);

	g_value_init (value, G_TYPE_INT);

	/* TODO: for now we only support quering int properties through a 
	 * spinbutton. In the future we should have a different function for
	 * each GladePropertyClass which requires it.
	 */
	num = (gint) gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget));
	g_value_set_int (value, num);

	g_hash_table_insert (result->hash, property_id, value);
}

static GtkWidget *
glade_widget_append_query (GtkWidget *table,
			   GladePropertyClass *property_class,
			   gint row)
{
	GladePropertyQuery *query;
	GtkAdjustment *adjustment;
	GtkWidget *label;
	GtkWidget *spin;
	gchar *text;

	query = property_class->query;
	
	if (property_class->type != GLADE_PROPERTY_TYPE_INTEGER) {
		g_warning ("We can only query integer types for now. Trying to query %d. FIXME please ;-)", property_class->type);
		return NULL;
	}
	
	/* Label */
	text = g_strdup_printf ("%s :", query->question);
	label = gtk_label_new (text);
	g_free (text);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), label,
				   0, 1, row, row +1);

	/* Spin/Entry */
	adjustment = glade_parameter_adjustment_new (property_class);
	spin = gtk_spin_button_new (adjustment, 1, 0);
	gtk_widget_show (spin);
	gtk_table_attach_defaults (GTK_TABLE (table), spin,
				   1, 2, row, row +1);

	return spin;
}

/**
 * glade_widget_query_properties:
 * @class: 
 * @result: 
 * 
 * Queries the user for some property values before a GladeWidget creation
 * for example before creating a GtkVBox we want to ask the user the number
 * of columns he wants.
 * 
 * Return Value: FALSE if the query was canceled
 **/
gboolean 
glade_widget_query_properties (GladeWidgetClass *class,
			       GladePropertyQueryResult *result)
{
	GladePropertyClass *property_class;
	GHashTable *hash;
	gchar *title;
	GtkWidget *dialog;
	GtkWidget *table;
	GtkWidget *vbox;
	GtkWidget *spin = NULL;
	GList *list;
	gint response;
	gint row = 0;

	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), FALSE);
	g_return_val_if_fail (result != NULL, FALSE);

	title = g_strdup_printf (_("New %s"), class->name);
	dialog = gtk_dialog_new_with_buttons (title,
					      NULL /* parent, FIXME: parent should be the project window */,
					      GTK_DIALOG_MODAL,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					      NULL);
	g_free (title);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);	
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);	

	vbox = GTK_DIALOG (dialog)->vbox;
	table = gtk_table_new (0, 0, FALSE);
	gtk_widget_show (table);
	gtk_box_pack_start_defaults (GTK_BOX (vbox), table);

	hash = g_hash_table_new (g_str_hash, g_str_equal);

	for (list = class->properties; list; list = list->next) {
		property_class = list->data;
		if (property_class->query) {
			spin = glade_widget_append_query (table, property_class, row++);
			g_hash_table_insert (hash, property_class->id, spin);
		}
	}
	if (spin == NULL) {
		g_hash_table_destroy (hash);
		gtk_widget_destroy (GTK_WIDGET (dialog));
		return FALSE;
	}

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	switch (response) {
	case GTK_RESPONSE_ACCEPT:
		g_hash_table_foreach (hash,
				      glade_widget_query_set_result,
				      result);
		break;
	case GTK_RESPONSE_REJECT:
	case GTK_RESPONSE_DELETE_EVENT:
		gtk_widget_destroy (dialog);
		return FALSE;
	default:
		g_assert_not_reached ();
	}

	g_hash_table_destroy (hash);
	gtk_widget_destroy (dialog);

	return TRUE;
}

/**
 * glade_widget_apply_queried_properties:
 * @widget: widget that will receive the set of queried properties
 * @result: values of the queried properties
 * 
 */
static void
glade_widget_apply_queried_properties (GladeWidget *widget,
				       GladePropertyQueryResult *result)
{
	GList *list;

	for (list = widget->properties; list; list = list->next) {
		GladeProperty *property;
		GladePropertyClass *pclass;
		GValue *value;

		property = GLADE_PROPERTY (list->data);
		pclass = property->class;
		if (pclass->query) {
			value = g_hash_table_lookup (result->hash, pclass->id);
			if (!value) {
				g_warning ("Property value not found in query result");
				continue;
			}
			glade_property_set (property, value);
		}
	}
}

/**
 * glade_widget_new_from_class:
 * @class:
 * @project:
 * @parent: the parent of the new widget, should be NULL for toplevel widgets
 * 
 * Creates a new GladeWidget from a given class. Takes care of registering
 * the widget in the project, adding it to the views and quering the user
 * if needed.
 * 
 * Return Value: A newly creatred GladeWidget, NULL on user cancel or error	
 **/
GladeWidget *
glade_widget_new_from_class (GladeWidgetClass *class,
			     GladeProject *project)
{
	GladePropertyQueryResult *result = NULL;
	GladeWidget *widget;

	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), NULL);
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

	if (glade_widget_class_has_queries (class))
	{
		result = glade_property_query_result_new ();
		if (!glade_widget_query_properties (class, result))
			return NULL;
	}

	widget = glade_widget_new_full (class, project);

	/* If we are a container, add the placeholders */
	if (widget->class->fill_empty)
		widget->class->fill_empty (widget->widget);

	if (result)
	{
		glade_widget_apply_queried_properties (widget, result);
		glade_property_query_result_destroy (result);
	}

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
 * glade_widget_get_property_by_class:
 * @widget: 
 * @property_class: 
 * 
 * Given a glade Widget, it returns the property that correspons to @property_class
 * 
 * Return Value: 
 **/
GladeProperty *
glade_widget_get_property_by_class (GladeWidget *widget,
				    GladePropertyClass *property_class)
{
	GladeProperty *property;
	GList *list;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (property_class), NULL);

	if (property_class->packing)
		list = widget->packing_properties;
	else
		list = widget->properties;

	for (; list; list = list->next) {
		property = list->data;
		if (property->class == property_class)
			return property;
	}

	g_warning ("Could not get property for widget %s of %s class\n",
		   widget->name, property_class->name);
	return NULL;
}

GladeProperty *
glade_widget_get_property_by_id (GladeWidget *widget,
				 const gchar *id,
				 gboolean packing)
{
	GList *list;
	GladeProperty *property;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	if (packing)
		list = widget->packing_properties;
	else
		list = widget->properties;

	for (; list; list = list->next) {
		property = list->data;
		if (strcmp (property->class->id, id) == 0)
			return property;
	}

	g_warning ("Could not get property %s for widget %s\n",
		   id, widget->name);
	return NULL;
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
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (name != NULL);

	if (widget->name)
		g_free (widget->name);
	widget->name = g_strdup (name);

	glade_project_widget_name_changed (widget->project, widget);
}

/**
 * glade_widget_clone:
 * @widget: 
 * 
 * Make a copy of a #GladeWidget.
 * You have to set name, project and parent when adding the clone
 * to a project.
 *
 * Return Value: the cloned GladeWidget, NULL on error.
 */
GladeWidget *
glade_widget_clone (GladeWidget *widget)
{
	GladeWidget *clone;

	g_return_val_if_fail (widget != NULL, NULL);

	/* This should be enough to clone. */
	clone = glade_widget_new_full (widget->class, widget->project);

	return clone;
}

/**
 * glade_widget_replace_with_placeholder:
 * @widget:
 * @placeholder:
 *
 * Replaces @widget with @placeholder.
 */
void
glade_widget_replace_with_placeholder (GladeWidget *widget,
				       GladePlaceholder *placeholder)
{
	GladeWidget *parent;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_PLACEHOLDER (placeholder));

	parent = glade_widget_get_parent (widget);

	if (parent->class->replace_child)
		parent->class->replace_child (widget->widget,
					      GTK_WIDGET (placeholder),
					      parent->widget);
}

GArray *
glade_widget_find_signals_by_name (GladeWidget *widget, const char *name)
{
	return g_hash_table_lookup (widget->signals, name);
}

/**
 * glade_widget_find_signal:
 * @widget
 * @signal
 * 
 * Find the element in the signal list of the widget with the same
 * signal as @signal or NULL if not present.
 */
GladeSignal *
glade_widget_find_signal (GladeWidget *widget, GladeSignal *signal)
{
	size_t i;
	GArray *signals = glade_widget_find_signals_by_name (widget, signal->name);

	if (!signals)
		return NULL;

	for (i = 0; i < signals->len; i++)
	{
		GladeSignal *ret_signal = (GladeSignal*) g_array_index (signals, GladeSignal*, i);
		if (glade_signal_compare (ret_signal, signal))
		{
			g_debug (("glade_widget_find_signal (%s, %s, %s) found\n", signal->name, signal->handler, signal->after ? "TRUE" : "FALSE"));
			return ret_signal;
		}
	}

	/* not found... */
	g_debug (("glade_widget_find_signal (%s, %s, %s) not found\n", signal->name, signal->handler, signal->after ? "TRUE" : "FALSE"));
	return NULL;
}

/**
 * glade_widget_add_signal:
 * @widget
 * @signal
 * 
 * Add @signal to the widget's signal list.
 **/
void
glade_widget_add_signal (GladeWidget *widget, GladeSignal *signal)
{

	GArray *signals;
	GladeSignal *new_signal;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_SIGNAL (signal));

	signals = glade_widget_find_signals_by_name (widget, signal->name);
	if (!signals)
	{
		signals = g_array_new (FALSE, FALSE, sizeof (GladeSignal*));
		g_hash_table_insert (widget->signals, g_strdup (signal->name), signals);
	}

	new_signal = glade_signal_copy (signal);
	g_array_append_val (signals, new_signal);
}

/**
 * glade_widget_remove_signal:
 * @widget
 * @signal
 * 
 * Remove @signal from the widget's signal list.
 **/
void
glade_widget_remove_signal (GladeWidget *widget, GladeSignal *signal)
{
	GArray *signals;
	GladeSignal *tmp_signal;
	size_t i;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_SIGNAL (signal));

	signals = glade_widget_find_signals_by_name (widget, signal->name);
	if (!signals)
	{
		/* trying to remove an inexistent signal? */
		g_assert (FALSE);
		return;
	}

	for (i = 0; i < signals->len; i++)
	{
		tmp_signal = g_array_index (signals, GladeSignal*, i);
		if (glade_signal_compare (tmp_signal, signal))
			break;
	}

	if (i == signals->len)
	{
		/* trying to remove an inexistent signal? */
		g_assert (FALSE);
		return;
	}

	glade_signal_free (tmp_signal);
	g_array_remove_index (signals, i);
}

static GladeXmlNode *
glade_widget_write_child (GladeXmlContext *context, GtkWidget *gtk_widget);

typedef struct _WriteSignalsContext
{
	GladeXmlContext *context;
	GladeXmlNode *node;
} WriteSignalsContext;

static void
glade_widget_write_signals (gpointer key, gpointer value, gpointer user_data)
{
	WriteSignalsContext *write_signals_context = (WriteSignalsContext *) user_data;
	GladeXmlNode *child;
	size_t i;

	GArray *signals = (GArray *) value;
	for (i = 0; i < signals->len; i++)
	{
		GladeSignal *signal = g_array_index (signals, GladeSignal*, i);
		child = glade_signal_write (write_signals_context->context, signal);
		if (!child)
			continue;

		glade_xml_node_append_child (write_signals_context->node, child);
	}
}

GladeXmlNode *
glade_widget_write (GladeXmlContext *context, GladeWidget *widget)
{
	GladeXmlNode *node;
	GladeXmlNode *child;
	WriteSignalsContext write_signals_context;
	GList *list;

	g_return_val_if_fail (GLADE_XML_IS_CONTEXT (context), NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

	node = glade_xml_node_new (context, GLADE_XML_TAG_WIDGET);

	glade_xml_node_set_property_string (node, GLADE_XML_TAG_CLASS, widget->class->name);
	glade_xml_node_set_property_string (node, GLADE_XML_TAG_ID, widget->name);

	/* Write the properties */
	list = widget->properties;
	for (; list; list = list->next) {
		GladeProperty *property = list->data;
		if (property->class->packing)
			continue;
		child = glade_property_write (context, property);
		if (!child) {
			continue;
		}
		glade_xml_node_append_child (node, child);
	}

	/* Signals */
	write_signals_context.node = node;
	write_signals_context.context = context;
	g_hash_table_foreach (widget->signals, glade_widget_write_signals, &write_signals_context);

	/* Children */
	if (GTK_IS_CONTAINER (widget->widget)) {
		list = glade_util_container_get_all_children (GTK_CONTAINER (widget->widget));
		for (; list; list = list->next) {
			GtkWidget *child_widget;
			child_widget = GTK_WIDGET (list->data);
			child = glade_widget_write_child (context, child_widget);
			if (!child) {
				continue;
	 		}
			glade_xml_node_append_child (node, child);
	 	}
	}

	return node;
}

static GladeXmlNode *
glade_widget_write_child (GladeXmlContext *context, GtkWidget *gtk_widget)
{
	GladeWidget *child_widget;
	GladeXmlNode *child_tag; /* this is the <child> tag */
	GladeXmlNode *child;     /* this is the <widget> under <child> */
	GladeXmlNode *packing;
	GList *list;

	child_tag = glade_xml_node_new (context, GLADE_XML_TAG_CHILD);

	if (GLADE_IS_PLACEHOLDER (gtk_widget)) {
		child = glade_xml_node_new (context, GLADE_XML_TAG_PLACEHOLDER);
		glade_xml_node_append_child (child_tag, child);

		return child_tag;
	}

	child_widget = glade_widget_get_from_gtk_widget (gtk_widget);
	if (!child_widget)
		return NULL;

	if (child_widget->internal)
		glade_xml_node_set_property_string (child_tag, GLADE_XML_TAG_INTERNAL_CHILD, child_widget->internal);

	child = glade_widget_write (context, child_widget);
		if (!child) {
			g_warning ("Failed to write child widget");
			return NULL;
		}
	glade_xml_node_append_child (child_tag, child);

	/* Append the packing properties */
	packing = glade_xml_node_new (context, GLADE_XML_TAG_PACKING);
	glade_xml_node_append_child (child_tag, packing);
	list = child_widget->packing_properties;
	for (; list; list = list->next) {
		GladeProperty *property;
		GladeXmlNode *packing_property;
		property = list->data;
		g_assert (property->class->packing == TRUE);
		packing_property = glade_property_write (context, property);
		if (!packing_property)
			continue;
		glade_xml_node_append_child (packing, packing_property);
	}

	return child_tag;
}

static gboolean
glade_widget_apply_property_from_node (GladeXmlNode *node,
				       GladeWidget *widget,
				       gboolean packing)
{
	GladeProperty *property;
	GValue *gvalue;
	gchar *value;
	gchar *id;

	id = glade_xml_get_property_string_required (node, GLADE_XML_TAG_NAME, NULL);
	value = glade_xml_get_content (node);
	if (!value || !id)
		return FALSE;

	glade_util_replace (id, '_', '-');
	property = glade_widget_get_property_by_id (widget, id, packing);
	if (!property)
		return FALSE;

	gvalue = glade_property_class_make_gvalue_from_string (property->class,
							       value);
	glade_property_set (property, gvalue);
		
	g_free (id);
	g_free (value);
	g_value_unset (gvalue);
	g_free (gvalue);

	return TRUE;
}

static gboolean
glade_widget_new_child_from_node (GladeXmlNode *node,
				  GladeProject *project,
				  GladeWidget *parent);

static void
glade_widget_fill_from_node (GladeXmlNode *node, GladeWidget *widget)
{
	GladeXmlNode *child;
	const gchar *class_name;
	const gchar *widget_name;

	g_return_if_fail (GLADE_IS_WIDGET (widget));

	if (!glade_xml_node_verify (node, GLADE_XML_TAG_WIDGET))
		return;

	class_name = glade_xml_get_property_string_required (node, GLADE_XML_TAG_CLASS, NULL);
	widget_name = glade_xml_get_property_string_required (node, GLADE_XML_TAG_ID, NULL);
	if (!class_name || !widget_name)
		return;

	g_assert (strcmp (class_name, widget->class->name) == 0);
	glade_widget_set_name (widget, widget_name);

	/* Children */
	child =	glade_xml_node_get_children (node);
	for (; child; child = glade_xml_node_next (child)) {
		if (!glade_xml_node_verify_silent (child, GLADE_XML_TAG_CHILD))
			continue;

		if (!glade_widget_new_child_from_node (child, widget->project, widget)) {
			g_warning ("Failed to read child");
			continue;
		}
	}

	/* Signals */
	child =	glade_xml_node_get_children (node);
	for (; child; child = glade_xml_node_next (child)) {
		GladeSignal *signal;

		if (!glade_xml_node_verify_silent (child, GLADE_XML_TAG_SIGNAL))
			continue;

		signal = glade_signal_new_from_node (child);
		if (!signal) {
			g_warning ("Failed to read signal");
			continue;
		}
		glade_widget_add_signal (widget, signal);
	}

	/* Properties */
	child =	glade_xml_node_get_children (node);
	for (; child; child = glade_xml_node_next (child)) {
		if (!glade_xml_node_verify_silent (child, GLADE_XML_TAG_PROPERTY))
			continue;

		if (!glade_widget_apply_property_from_node (child, widget, FALSE)) {
			g_warning ("Failed to apply property");
			continue;
		}
	}
}

GladeWidget *
glade_widget_new_from_node_real (GladeXmlNode *node,
				 GladeProject *project,
				 GladeWidget *parent)
{
	GladeWidgetClass *class;
	GladeWidget *widget;
	const gchar *class_name;

	if (!glade_xml_node_verify (node, GLADE_XML_TAG_WIDGET))
		return NULL;

	class_name = glade_xml_get_property_string_required (node, GLADE_XML_TAG_CLASS, NULL);
	if (!class_name)
		return NULL;

	class = glade_widget_class_get_by_name (class_name);
	if (!class)
		return NULL;

	widget = glade_widget_new_full (class, project);
	if (!widget)
		return NULL;

	/* create the packing_properties list, without setting them */
	if (parent)
		widget->packing_properties = glade_widget_create_packing_properties (parent, widget);

	glade_widget_fill_from_node (node, widget);

	gtk_widget_show_all (widget->widget);

	return widget;
}

/**
 * When looking for an internal child we have to walk up the hierarchy...
 */
static GladeWidget *
glade_widget_get_internal_child (GladeWidget *parent,
				 const gchar *internal)
{
	GladeWidget *ancestor;
	GladeWidget *child = NULL;

	ancestor = parent;
	while (ancestor) {
		if (ancestor->class->get_internal_child) {
			GtkWidget *widget;
			ancestor->class->get_internal_child (ancestor->widget, internal, &widget);
			if (widget) {
				child = glade_widget_get_from_gtk_widget (widget);
				if (child)
					break;
			}
		}
		ancestor = glade_widget_get_parent (ancestor);
	}

	return child;
}

static gboolean
glade_widget_new_child_from_node (GladeXmlNode *node,
				  GladeProject *project,
				  GladeWidget *parent)
{
	gchar *internalchild;
	GladeXmlNode *child_node;
	GladeWidget *child;

	if (!glade_xml_node_verify (node, GLADE_XML_TAG_CHILD))
		return FALSE;

	/* is it a placeholder? */
	child_node = glade_xml_search_child (node, GLADE_XML_TAG_PLACEHOLDER);
	if (child_node)
	{
		gtk_container_add (GTK_CONTAINER (parent->widget), glade_placeholder_new ());
		return TRUE;
	}

	/* then it must be a widget */
	child_node = glade_xml_search_child_required (node, GLADE_XML_TAG_WIDGET);
	if (!child_node)
		return FALSE;

	/* is it an internal child? */
	internalchild = glade_xml_get_property_string (node, GLADE_XML_TAG_INTERNAL_CHILD);
	if (internalchild)
	{
		child = glade_widget_get_internal_child (parent, internalchild);
		if (!child)
		{
			g_warning ("Failed to get internal child %s", internalchild);
			g_free (internalchild);
			return FALSE;
		}
		glade_widget_fill_from_node (child_node, child);
	}
	else
	{
		child = glade_widget_new_from_node_real (child_node, project, parent);
		if (!child)
			/* not enough memory... and now, how can I signal it
			 * and not make the caller believe that it was a parsing
			 * problem?
			 */
			return FALSE;

		gtk_container_add (GTK_CONTAINER (parent->widget), child->widget);
	}

	/* Get the packing properties */
	child_node = glade_xml_search_child (node, GLADE_XML_TAG_PACKING);
	if (child_node)
	{
		GladeXmlNode *property_node;

		property_node = glade_xml_node_get_children (child_node);
		for (; property_node; property_node = glade_xml_node_next (property_node))
		{
			/* we should be on a <property ...> tag */
			if (!glade_xml_node_verify (property_node, GLADE_XML_TAG_PROPERTY))
				continue;

			if (!glade_widget_apply_property_from_node (property_node, child, TRUE)) {
				g_warning ("Failed to apply packing property");
				continue;
			}
		}
	}

	return TRUE;
}

GladeWidget *
glade_widget_new_from_node (GladeXmlNode *node, GladeProject *project)
{
	return glade_widget_new_from_node_real (node, project, NULL);
}

