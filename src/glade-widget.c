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
#include "glade-placeholder.h"
#include "glade-signal.h"
#include "glade-gtk.h"
#include "glade-packing.h"
#include "glade-clipboard.h"
#include "glade-command.h"
#include "glade-debug.h"
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

static GList *
glade_widget_property_list_from_class (GladeWidgetClass *class,
				       GladeWidget *widget)
{
	GList *list = NULL;
	GladePropertyClass *property_class;
	GladeProperty *property;
	GList *new_list = NULL;

	for (list = class->properties; list; list = list->next) {
		property_class = list->data;
		property = glade_property_new_from_class (property_class, widget);
		if (property == NULL)
			continue;

		property->widget = widget;

		new_list = g_list_prepend (new_list, property);
	}

	new_list = g_list_reverse (new_list);
	
	return new_list;
}

/**
 * glade_widget_new:
 * @class: The GladeWidgeClass of the GladeWidget
 * 
 * Allocates a new GladeWidget structure and fills in the required memebers.
 * 
 * Return Value: 
 **/
static GladeWidget *
glade_widget_new (GladeWidgetClass *class)
{
	GladeWidget *widget;

	widget = g_new0 (GladeWidget, 1);
	widget->name     = NULL;
	widget->widget   = NULL;
	widget->project  = NULL;
	widget->class    = class;
	widget->properties = glade_widget_property_list_from_class (class, widget);
	widget->parent   = NULL;
	widget->children = NULL;

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
		if (data.found_child) {
			temp = data.found_child;
			if (glade_widget_get_from_gtk_widget (temp)) {
				found = glade_widget_get_from_gtk_widget (temp);
				g_assert (found->widget == data.found_child);
			} else {
				g_debug(("Temp was not a GladeWidget, it was a %s\n",
					 gtk_widget_get_name (temp)));
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
			   gw ? gw->name : "[null]");
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
glade_widget_button_press (GtkWidget *event_widget,
			   GdkEventButton *event,
			   gpointer not_used)
{
	GladeWidget *glade_widget;

	glade_widget = glade_widget_get_from_event_widget (event_widget, event);

	g_debug(("button press for a %s\n", glade_widget->class->name));

	if (!glade_widget) {
		g_warning ("Button press event but the gladewidget was not found\n");
		return FALSE;
	}

	g_debug(("Event button %d\n", event->button));

	if (event->button == 1) {
		/* If this is a selection set, don't change the state of the widget
		 * for example for toggle buttons
		 */
		if (!glade_util_has_nodes (glade_widget->widget))
			g_signal_stop_emission_by_name (G_OBJECT (event_widget),
							"button_press_event");
		glade_project_selection_set (glade_widget->project, glade_widget->widget, TRUE);
	} else if (event->button == 3)
		glade_popup_pop (glade_widget, event);
	else
		g_debug(("Button press not handled yet.\n"));

	g_debug(("The widget found was a %s\n", glade_widget->class->name));

	return FALSE;
}

static gboolean
glade_widget_button_release (GtkWidget *widget, GdkEventButton *event, gpointer not_used)
{
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

	for (list = widget->properties; list; list = list->next) {
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
	  
	g_signal_connect (G_OBJECT (widget), "button_press_event",
			  G_CALLBACK (glade_widget_button_press), NULL);
	g_signal_connect (G_OBJECT (widget), "button_release_event",
			  G_CALLBACK (glade_widget_button_release), NULL);
}

static gboolean
glade_widget_key_press(GtkWidget *event_widget,
		       GdkEventKey *event,
		       gpointer user_data)
{
	GladeWidget *glade_widget = GLADE_WIDGET (user_data);
	
	g_return_val_if_fail (GTK_IS_WIDGET (event_widget), FALSE);
	g_return_val_if_fail (glade_widget->widget == event_widget, FALSE);

	/* We will delete all the selected items */
	if (event->keyval == GDK_Delete)
		glade_util_delete_selection ();

	g_debug(("glade_widget_key_press\n"));
	return TRUE;
}

static void
glade_widget_connect_keyboard_signals (GladeWidget *glade_widget)
{
	GtkWidget *widget = glade_widget->widget;

	if (!GTK_WIDGET_NO_WINDOW(widget)) {
		gtk_widget_set_events (widget, gtk_widget_get_events (widget)
				       | GDK_KEY_PRESS_MASK);
	}

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
	property = g_object_get_data (G_OBJECT (w), GLADE_MODIFY_PROPERTY_DATA);
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

	for (list = class->update_signals; list; list = list->next) {
		g_signal_connect_after (G_OBJECT (widget->widget), list->data,
					G_CALLBACK (glade_widget_property_changed_cb),
					property);
		g_object_set_data (G_OBJECT (widget->widget),
				   GLADE_MODIFY_PROPERTY_DATA, property);
	}
}

static void
glade_widget_connect_edit_signals (GladeWidget *widget)
{
	GladePropertyClass *class;
	GList *list;

	for (list = widget->class->properties; list; list = list->next) {
		class = list->data;
		if (class->update_signals)
			glade_widget_connect_edit_signals_with_class (widget,
								      class);
	}
}

static void
glade_widget_connect_other_signals (GladeWidget *widget)
{
	if (GLADE_WIDGET_IS_TOPLEVEL (widget)) {
		g_signal_connect (G_OBJECT (widget->widget), "delete_event",
				  G_CALLBACK (gtk_widget_hide_on_delete), NULL);
	}
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
	GList *list;
	
	widget->class = NULL;
	widget->project = NULL;
	widget->widget = NULL;
	widget->parent = NULL;

	if (widget->name)
		g_free (widget->name);
	widget->name = NULL;

	list = widget->properties;
	for (; list; list = list->next)
		glade_property_free (list->data);
	widget->properties = NULL;

	list = widget->signals;
	for (; list; list = list->next)
		glade_signal_free (list->data);
	widget->signals = NULL;

	g_free (widget);
}

#if 0
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

gboolean
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
	g_signal_connect_swapped (G_OBJECT (widget), "destroy",
				  G_CALLBACK (glade_widget_free), G_OBJECT (glade_widget));
	g_object_set_data (G_OBJECT (glade_widget->widget), GLADE_WIDGET_DATA_TAG, glade_widget);

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
	
	/* We need to call the post_create_function after the embed of the widget in
	   its parent.  Otherwise, calls to gtk_widget_grab_focus et al. will fail */
	if (class->post_create_function) {
		void (*pcf) (GObject *object);
		pcf = glade_gtk_get_function (class->post_create_function);
		if (!pcf)
			g_warning ("Could not find %s\n", class->post_create_function);
		else
			pcf (G_OBJECT (glade_widget->widget));
	}

	return TRUE;
}

void
glade_widget_connect_signals (GladeWidget *widget)
{
	glade_widget_connect_mouse_signals (widget);
	glade_widget_connect_keyboard_signals (widget);
	glade_widget_connect_edit_signals  (widget);
	glade_widget_connect_other_signals (widget);
}

static GladeWidget *
glade_widget_new_full (GladeWidgetClass *class,
		       GladeProject *project,
		       GladeWidget *parent)
{
	GladeWidget *widget;

	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), NULL);

	widget = glade_widget_new (class);
	widget->name    = glade_widget_new_name (project, class);
	widget->parent  = parent;
	widget->project = project;

	glade_packing_add_properties (widget);
	glade_widget_create_gtk_widget (widget);

	glade_widget_set_contents (widget);
	glade_widget_connect_signals (widget);

	return widget;
}

static void
glade_widget_fill_empty (GtkWidget *widget)
{
	GList *children;
	gboolean empty = TRUE;

	if (!GTK_IS_CONTAINER (widget))
		return;
	
	/* fill with placeholders the containers that are inside of this container */
	children = gtk_container_get_children (GTK_CONTAINER (widget));

	/* loop over the children of this container, and fill them with placeholders */
	while (children != NULL) {
		glade_widget_fill_empty (GTK_WIDGET (children->data));
		children = children->next;
		empty = FALSE;
	}

	if (empty) {
		/* retrieve the desired number of placeholders that this widget should hold */
		int nb_children = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "glade_nb_placeholders"));
		int i;

		if (nb_children == 0 && GTK_IS_BIN (widget))
			nb_children = 1;

		for (i = nb_children; i > 0; i--)
			gtk_container_add (GTK_CONTAINER (widget), glade_placeholder_new ());
	}
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
	adjustment = glade_parameter_adjustment_new (property_class->parameters, property_class->def);
	spin = gtk_spin_button_new (adjustment, 1, 0);
	gtk_widget_show (spin);
	gtk_table_attach_defaults (GTK_TABLE (table), spin,
				   1, 2, row, row +1);

	return spin;
}

void
glade_widget_query_properties_set (gpointer key_,
				   gpointer value_,
				   gpointer user_data)
{
	GladePropertyQueryResult *result = user_data;
	GtkWidget *spin = value_;
	const gchar *key = key_;
	gint num;

	num = (gint) gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin));
	glade_property_query_result_set_int (result, key, num);
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
	GtkWidget *dialog;
	GtkWidget *table;
	GtkWidget *vbox;
	GtkWidget *spin = NULL;
	GList *list;
	gint response;
	gint row = 0;

	g_return_val_if_fail (class  != NULL, FALSE);
	g_return_val_if_fail (result != NULL, FALSE);

	dialog = gtk_dialog_new_with_buttons (NULL /* name */,
					      NULL /* parent, FIXME: parent should be the project window */,
					      GTK_DIALOG_MODAL,
					      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					      NULL);
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
				      glade_widget_query_properties_set,
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
 **/
static void
glade_widget_apply_queried_properties (GladeWidget *widget, GladePropertyQueryResult *result)
{
	GList *list;

	list = widget->class->properties;
	for (; list; list = list->next) {
		GladePropertyClass *pclass = list->data;
		if (pclass->query) {
			GladeProperty *property;
			gint temp;
			glade_property_query_result_get_int (result, pclass->id, &temp);
			property = glade_property_get_from_id (widget->properties, pclass->id);
			glade_property_set_integer (property, temp);
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
			     GladeProject *project,
			     GladeWidget *parent)
{
	GladePropertyQueryResult *result = NULL;
	GladeWidget *widget;

	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), NULL);
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

	if (glade_widget_class_has_queries (class)) {
		result = glade_property_query_result_new ();
		if (!glade_widget_query_properties (class, result))
			return NULL;
	}

	widget = glade_widget_new_full (class, project, parent);

	glade_widget_apply_queried_properties (widget, result);

	/* If we are a container, add the placeholders */
	if (g_type_is_a (class->type,  GTK_TYPE_CONTAINER))
		glade_widget_fill_empty (widget->widget);

	if (result) 
		glade_property_query_result_destroy (result);

	glade_widget_set_default_options (widget);

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

	for (; list; list = list->next) {
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

/**
 * glade_widget_clone:
 * @widget: 
 * 
 * Make a copy of a #GladeWidget.
 * 
 * Return Value: the cloned GladeWidget, NULL on error.
 **/
GladeWidget *
glade_widget_clone (GladeWidget *widget)
{
	GladeWidget *clone;

	g_return_val_if_fail (widget != NULL, NULL);

	/*
	 * This should be enough to clone.
	 */
	clone = glade_widget_new (widget->class);
	clone->name = glade_widget_new_name (widget->project, widget->class);
	clone->project = widget->project;
	glade_widget_create_gtk_widget (clone);

	return clone;
}

/**
 * glade_widget_replace_with_placeholder:
 * @widget:
 * @placeholder:
 *
 * Replaces @widget with @placeholder. If @placeholder is NULL a new one is created.
 **/
GladePlaceholder *
glade_widget_replace_with_placeholder (GladeWidget *widget, GladePlaceholder *placeholder)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

	if (placeholder == NULL)
		placeholder = glade_placeholder_new (widget->parent);
	else
		g_return_val_if_fail (GLADE_IS_PLACEHOLDER (placeholder), NULL);

	if (widget->parent->class->placeholder_replace)
		widget->parent->class->placeholder_replace (widget->widget,
							    GTK_WIDGET (placeholder),
							    widget->parent->widget);

	/* Remove it from the parent's child list */
	widget->parent->children = g_list_remove (widget->parent->children, widget);

	/* Return the placeholder, if some one needs it, he can use it. */
	return placeholder;
}

/**
 * glade_widget_find_signal:
 * @widget
 * @signal
 * 
 * Find the element in the signal list of the widget with the same
 * signal as @signal or NULL if not present.
 */
GList *
glade_widget_find_signal (GladeWidget *widget, GladeSignal *signal)
{
	GList *list;

	for (list = widget->signals; list; list = list->next) {
		GladeSignal *tmp = GLADE_SIGNAL (list->data);
		if (glade_signal_compare (tmp, signal))
				return list;
	}

	/* not found... */
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
	GList *found;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_SIGNAL (signal));

	found = glade_widget_find_signal (widget, signal);
	if (found) {
		glade_signal_free (signal);
		return;
	}

	widget->signals = g_list_append (widget->signals, signal);
	return;
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
	GList *found;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_SIGNAL (signal));

	found = glade_widget_find_signal (widget, signal);
	if (found) {
		g_list_remove_link (widget->signals, found);
		glade_signal_free (GLADE_SIGNAL (found->data));
		g_list_free_1 (found);
	}
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
	GtkWidget *gtk_widget;
	GList *list;
	GList *list2;

	g_return_val_if_fail (GLADE_XML_IS_CONTEXT (context), NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

	node = glade_xml_node_new (context, GLADE_XML_TAG_WIDGET);

	glade_xml_node_set_property_string (node, GLADE_XML_TAG_CLASS, widget->class->name);
	glade_xml_node_set_property_string (node, GLADE_XML_TAG_ID, widget->name);

	/* Write the properties */
	list = widget->properties;
	for (; list; list = list->next) {
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
	for (; list; list = list->next) {
		signal = list->data;
		child = glade_signal_write (context, signal);
		if (child == NULL)
			return NULL;
		glade_xml_node_append_child (node, child);
	}

	/* Children */
	if (GTK_IS_CONTAINER (widget->widget))
		list = gtk_container_get_children (GTK_CONTAINER (widget->widget));
	else
		list = NULL;

	for (; list; list = list->next) {
		gtk_widget = GTK_WIDGET (list->data);
		child_widget = glade_widget_get_from_gtk_widget (gtk_widget);
		if (!child_widget && !GLADE_IS_PLACEHOLDER (gtk_widget))
			continue;

		child_tag = glade_xml_node_new (context, GLADE_XML_TAG_CHILD);
		glade_xml_node_append_child (node, child_tag);

		if (child_widget) {
			/* write the widget */
			child = glade_widget_write (context, child_widget);
			if (child == NULL)
				return NULL;

			glade_xml_node_append_child (child_tag, child);

			/* Append the packing properties */
			packing = glade_xml_node_new (context, GLADE_XML_TAG_PACKING);
			list2 = child_widget->properties;
			for (; list2; list2 = list2->next) {
				GladeXmlNode *packing_property;
				property = list2->data;
				if (!property->class->packing) 
					continue;
				packing_property = glade_property_write (context, property);
				if (packing_property == NULL)
					continue;
				glade_xml_node_append_child (packing, packing_property);
				glade_xml_node_append_child (child_tag, packing);
			}
		} else {
			/* a placeholder */
			child = glade_xml_node_new (context, GLADE_XML_TAG_PLACEHOLDER);
			glade_xml_node_append_child (child_tag, child);
		}
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

	id = glade_xml_get_property_string_required (node, GLADE_XML_TAG_NAME, NULL);
	glade_util_replace (id, '_', '-');
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
glade_widget_new_child_from_node (GladeXmlNode *node,
				  GladeProject *project,
				  GladeWidget *parent);

static GladeWidget *
glade_widget_new_from_node_real (GladeXmlNode *node,
				 GladeProject *project,
				 GladeWidget *parent)
{
	GladeWidgetClass *class;
	GladeXmlNode *child;
	GladeWidget *widget;
	GladeSignal *signal;
	gchar *class_name;

	if (!glade_xml_node_verify (node, GLADE_XML_TAG_WIDGET))
		return NULL;

	class_name = glade_xml_get_property_string_required (node, GLADE_XML_TAG_CLASS, NULL);
	if (!class_name)
		return NULL;
	class = glade_widget_class_get_by_name (class_name);
	if (!class)
		return NULL;
	
	widget = glade_widget_new_full (class, project, parent);

	/* Properties */
	child =	glade_xml_node_get_children (node);
	for (; child; child = glade_xml_node_next (child)) {
		if (!glade_xml_node_verify_silent (child, GLADE_XML_TAG_PROPERTY))
			continue;

		if (!glade_widget_apply_property_from_node (child, widget)) {
			return NULL;
		}
	}

	/* Signals */
	child =	glade_xml_node_get_children (node);
	for (; child; child = glade_xml_node_next (child)) {
		if (!glade_xml_node_verify_silent (child, GLADE_XML_TAG_SIGNAL))
			continue;

		signal = glade_signal_new_from_node (child);
		if (signal) {
			glade_widget_add_signal (widget, signal);
		}
	}

	/* Children */
	child =	glade_xml_node_get_children (node);
	for (; child; child = glade_xml_node_next (child)) {
		if (!glade_xml_node_verify_silent (child, GLADE_XML_TAG_CHILD))
			continue;

		if (!glade_widget_new_child_from_node (child, project, widget)) {
			return NULL;
		}
	}

	gtk_widget_show_all (widget->widget);

	return widget;
}

#if 0 /* do we still need these 3 functions ? */

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
			g_hash_table_destroy (hash);
			return NULL;
		}

		id    = glade_xml_get_property_string_required (child, GLADE_XML_TAG_NAME, NULL);
		value = glade_xml_get_content (child);

		if (!value || !id) {
			g_warning ("Invalid property %s:%s\n", value, id);
			g_hash_table_destroy (hash);
			g_free (value);
			g_free (id);
			return NULL;
		}

		glade_util_replace (id, '_', '-');
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

#endif

static gboolean
glade_widget_new_child_from_node (GladeXmlNode *node,
				  GladeProject *project,
				  GladeWidget *parent)
{
	GladeXmlNode *child_node;
	GladeXmlNode *child_properties;
	GladeWidget *child;
	GtkWidget *child_widget;

	if (!glade_xml_node_verify (node, GLADE_XML_TAG_CHILD))
		return FALSE;

	/* is it a placeholder? */
	child_node = glade_xml_search_child (node, GLADE_XML_TAG_PLACEHOLDER);
	if (child_node)
		child_widget = glade_placeholder_new (parent);
	else {
		/* Get and create the widget */
		child_node = glade_xml_search_child_required (node, GLADE_XML_TAG_WIDGET);
		if (!child_node)
			return FALSE;

		child = glade_widget_new_from_node_real (child_node, project, parent);
		if (!child)
			/* not enough memory... and now, how can I signal it and not make the caller believe that
			 * it was a parsing problem? */
			return FALSE;

		child_widget = child->widget;
	}

	gtk_container_add (GTK_CONTAINER (parent->widget), child_widget);

	/* Get the packing properties */
	child_node = glade_xml_search_child (node, GLADE_XML_TAG_PACKING);
	if (child_node) {
		GValue string_value = { 0, };

		g_value_init (&string_value, G_TYPE_STRING);
		child_properties = glade_xml_node_get_children (child_node);

		for (; child_properties != NULL; child_properties = glade_xml_node_next (child_properties)) {
			GParamSpec *param_spec;
			char *id;
			char *value;
			GValue gvalue = { 0, };

			/* we should be on a <property ...> tag */
			if (!glade_xml_node_verify (child_properties, GLADE_XML_TAG_PROPERTY))
				continue;

			/* the tag should have the form <property name="...id...">...value...</property>*/
			id = glade_xml_get_property_string_required (child_properties, GLADE_XML_TAG_NAME, NULL);
			value = glade_xml_get_content (child_properties);

			if (!value || !id) {
				g_warning ("Invalid property %s:%s\n", value, id);
				g_free (value);
				g_free (id);
				continue;
			}

			glade_util_replace (id, '_', '-');
			param_spec = gtk_container_class_find_child_property (G_OBJECT_GET_CLASS (parent->widget), id);
			if (!param_spec) {
				g_warning ("Invalid property [%s] for container [%s]\n", id, parent->name);
				g_free (value);
				g_free (id);
				continue;
			}

			g_value_set_string_take_ownership (&string_value, value);

			g_value_init (&gvalue, G_PARAM_SPEC_VALUE_TYPE (param_spec));
			if (g_value_transform (&string_value, &gvalue)) {
				gtk_container_child_set_property (GTK_CONTAINER (parent->widget), child_widget, id, &gvalue);
				g_value_unset (&gvalue);
			}
			else
				g_warning ("Not able to apply the value [%s] to the type [%s]\n", value, id);
		}

		g_value_unset (&string_value);
	}
	
	glade_widget_fill_empty (parent->widget);

	return TRUE;
}

GladeWidget *
glade_widget_new_from_node (GladeXmlNode *node, GladeProject *project)
{
	return glade_widget_new_from_node_real (node, project, NULL);
}

