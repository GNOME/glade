/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Joaquin Cuenca Abela
 * Copyright (C) 2001, 2002, 2003 Ximian, Inc.
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
 *   Joaquin Cuenca Abela <e98cuenc@yahoo.com>
 *   Chema Celorio <chema@celorio.com>
 */

#include <string.h>
#include <glib-object.h>
#include "glade.h"
#include "glade-project.h"
#include "glade-widget-class.h"
#include "glade-widget.h"
#include "glade-marshallers.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-placeholder.h"
#include "glade-signal.h"
#include "glade-popup.h"
#include <glib.h>
#include <gdk/gdkkeysyms.h>

static void glade_widget_class_init			(GladeWidgetKlass *klass);
static void glade_widget_init				(GladeWidget *widget);
static void glade_widget_finalize			(GObject *object);
static void glade_widget_dispose			(GObject *object);
static void glade_widget_set_real_property		(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void glade_widget_get_real_property		(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void glade_widget_set_class			(GladeWidget *widget, GladeWidgetClass *klass);
static void glade_widget_real_add_signal_handler	(GladeWidget *widget, GladeSignal *signal_handler);
static void glade_widget_real_remove_signal_handler	(GladeWidget *widget, GladeSignal *signal_handler);
static void glade_widget_real_change_signal_handler	(GladeWidget *widget, GladeSignal *old_signal_handler, GladeSignal *new_signal_handler);
static void glade_widget_set_packing_properties		(GladeWidget *widget, GladeWidget *container);

enum
{
	ADD_SIGNAL_HANDLER,
	REMOVE_SIGNAL_HANDLER,
	CHANGE_SIGNAL_HANDLER,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_NAME,
	PROP_INTERNAL,
	PROP_WIDGET,
	PROP_CLASS,
	PROP_PROJECT
};

static guint glade_widget_signals[LAST_SIGNAL] = {0};
static GObjectClass *parent_class = NULL;

GType
glade_widget_get_type (void)
{
	static GType widget_type = 0;

	if (!widget_type)
	{
		static const GTypeInfo widget_info =
		{
			sizeof (GladeWidgetKlass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) glade_widget_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GladeWidget),
			0,		/* n_preallocs */
			(GInstanceInitFunc) glade_widget_init,
		};

		widget_type = g_type_register_static (G_TYPE_OBJECT, "Gladewidget",
						      &widget_info, 0);
	}

	return widget_type;
}

static void
glade_widget_class_init (GladeWidgetKlass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = glade_widget_finalize;
	object_class->dispose = glade_widget_dispose;
	object_class->set_property = glade_widget_set_real_property;
	object_class->get_property = glade_widget_get_real_property;

	klass->add_signal_handler = glade_widget_real_add_signal_handler;
	klass->remove_signal_handler = glade_widget_real_remove_signal_handler;
	klass->change_signal_handler = glade_widget_real_change_signal_handler;

	g_object_class_install_property (object_class, PROP_NAME,
					 g_param_spec_string ("name", _("Name"), _("The name of the widget"),
							      NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class, PROP_INTERNAL,
					 g_param_spec_string ("internal", _("Internal name"), _("The internal name of the widget"),
							      NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class, PROP_WIDGET,
					 g_param_spec_object ("widget", _("Widget"), _("The gtk+ widget associated"),
							      GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	g_object_class_install_property (object_class, PROP_CLASS,
					 g_param_spec_pointer ("class", _("Class"), _("The class of the associated gtk+ widget"),
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_PROJECT,
					 g_param_spec_object ("project", _("Project"), _("The glade project that this widget belongs to"),
							      GLADE_TYPE_PROJECT, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	glade_widget_signals[ADD_SIGNAL_HANDLER] =
		g_signal_new ("add_signal_handler",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeWidgetKlass, add_signal_handler),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);
	glade_widget_signals[REMOVE_SIGNAL_HANDLER] =
		g_signal_new ("remove_signal_handler",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeWidgetKlass, remove_signal_handler),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);
	glade_widget_signals[CHANGE_SIGNAL_HANDLER] =
		g_signal_new ("change_signal_handler",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeWidgetKlass, change_signal_handler),
			      NULL, NULL,
			      glade_cclosure_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_POINTER, G_TYPE_POINTER);
}

static void
free_signals (gpointer value)
{
	GPtrArray *signals = (GPtrArray*) value;
	guint i;
	guint nb_signals;

	if (signals == NULL)
		return;

	/* g_ptr_array_foreach (signals, (GFunc) glade_signal_free, NULL); only available in modern versions of Gtk+ */
	nb_signals = signals->len;
	for (i = 0; i < nb_signals; i++)
		glade_signal_free (g_ptr_array_index (signals, i));

	g_ptr_array_free (signals, TRUE);
}

static void
glade_widget_init (GladeWidget *widget)
{
	widget->widget_class = NULL;
	widget->project = NULL;
	widget->name = NULL;
	widget->internal = NULL;
	widget->widget = NULL;
	widget->properties = NULL;
	widget->packing_properties = NULL;
	widget->signals = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free, (GDestroyNotify) free_signals);
}

GladeWidget *
glade_widget_new (GladeWidgetClass *klass, GladeProject *project)
{
	GObject *widget = g_object_new (klass->type, NULL);
	GObject *glade_widget;

	if (klass->pre_create_function)
		klass->pre_create_function (G_OBJECT (widget));

	glade_widget = g_object_new (GLADE_TYPE_WIDGET,
				     "class", klass,
				     "project", project,
				     "name", glade_project_new_widget_name (project, klass->generic_name),
				     "widget", widget,
				     NULL);

	if (klass->post_create_function)
		klass->post_create_function (G_OBJECT (widget));
	if (klass->fill_empty)
		klass->fill_empty (GTK_WIDGET (widget));

	return (GladeWidget *) glade_widget;
}

GladeWidget *
glade_widget_new_for_internal_child (GladeWidgetClass *klass, GladeWidget *parent, GtkWidget *internal_widget, const char *internal_name)
{
	GladeProject *project = glade_widget_get_project (parent);
	return (GladeWidget *) g_object_new (GLADE_TYPE_WIDGET,
					     "class", klass,
					     "project", project,
					     "name", glade_project_new_widget_name (project, klass->generic_name),
					     "internal", internal_name,
					     "widget", internal_widget, NULL);
}

static void
glade_widget_finalize (GObject *object)
{
	GladeWidget *widget = GLADE_WIDGET (object);

	g_return_if_fail (GLADE_IS_WIDGET (object));

	glade_widget_class_free (widget->widget_class);
	g_free (widget->name);
	g_free (widget->internal);
	g_object_unref (widget->signals);
}

static void
glade_widget_dispose (GObject *object)
{
	GladeWidget *widget = GLADE_WIDGET (object);

	g_return_if_fail (GLADE_IS_WIDGET (object));

	if (widget->project)
		g_object_unref (widget->project);

	if (widget->widget)
		g_object_unref (widget->widget);

	if (widget->properties)
		g_list_foreach (widget->properties, (GFunc) g_object_unref, NULL);
	g_list_free (widget->properties);

	if (widget->packing_properties)
		g_list_foreach (widget->packing_properties, (GFunc) g_object_unref, NULL);
	g_list_free (widget->packing_properties);

	widget->project = NULL;
	widget->widget = NULL;
	widget->properties = NULL;
	widget->packing_properties = NULL;
}

void
glade_widget_add_signal_handler	(GladeWidget *widget, GladeSignal *signal_handler)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	g_signal_emit (widget, glade_widget_signals[ADD_SIGNAL_HANDLER], 0, signal_handler);
}

void
glade_widget_remove_signal_handler (GladeWidget *widget, GladeSignal *signal_handler)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	g_signal_emit (widget, glade_widget_signals[REMOVE_SIGNAL_HANDLER], 0, signal_handler);
}

void
glade_widget_change_signal_handler (GladeWidget *widget, GladeSignal *old_signal_handler, GladeSignal *new_signal_handler)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	g_signal_emit (widget, glade_widget_signals[CHANGE_SIGNAL_HANDLER], 0, old_signal_handler, new_signal_handler);
}

static void
glade_widget_set_real_property (GObject         *object,
				guint            prop_id,
				const GValue    *value,
				GParamSpec      *pspec)
{
	GladeWidget *widget;

	widget = GLADE_WIDGET (object);

	switch (prop_id)
	{
	case PROP_NAME:
		glade_widget_set_name (widget, g_value_get_string (value));
		break;
	case PROP_INTERNAL:
		glade_widget_set_internal (widget, g_value_get_string (value));
		break;
	case PROP_WIDGET:
		glade_widget_set_widget (widget, GTK_WIDGET (g_value_get_object (value)));
		break;
	case PROP_PROJECT:
		glade_widget_set_project (widget, GLADE_PROJECT (g_value_get_object (value)));
		break;
	case PROP_CLASS:
		glade_widget_set_class (widget, GLADE_WIDGET_CLASS (g_value_get_pointer (value)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_widget_get_real_property (GObject         *object,
				guint            prop_id,
				GValue          *value,
				GParamSpec      *pspec)
{
	GladeWidget *widget;

	widget = GLADE_WIDGET (object);

	switch (prop_id)
	{
	case PROP_NAME:
		g_value_set_string (value, widget->name);
		break;
	case PROP_INTERNAL:
		g_value_set_string (value, widget->internal);
		break;
	case PROP_CLASS:
		g_value_set_pointer (value, widget->widget_class);
		break;
	case PROP_PROJECT:
		g_value_set_object (value, widget->project);
		break;
	case PROP_WIDGET:
		g_value_set_object (value, widget->widget);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

void
glade_widget_set_name (GladeWidget *widget, const char *name)
{
	if (widget->name)
		g_free (widget->name);

	widget->name = g_strdup (name);
	g_object_notify (G_OBJECT (widget), "name");
}

const char *
glade_widget_get_name (GladeWidget *widget)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	return widget->name;
}

void
glade_widget_set_internal (GladeWidget *widget, const char *internal)
{
	if (widget->internal)
		g_free (widget->internal);

	widget->internal = g_strdup (internal);
	g_object_notify (G_OBJECT (widget), "internal");
}

const char *
glade_widget_get_internal (GladeWidget *widget)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	return widget->internal;
}

static void
glade_widget_set_class (GladeWidget *widget, GladeWidgetClass *klass)
{
	GladePropertyClass *property_class;
	GladeProperty *property;
	GList *list = klass->properties;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_WIDGET_CLASS (klass));
	/* calling set_class out of the constructor? */
	g_return_if_fail (widget->widget_class == NULL);
	
	// g_object_ref (klass); TODO, GladeWidgetClass is not a GObject
	widget->widget_class = klass;

	for (; list; list = list->next)
	{
		property_class = list->data;
		property = glade_property_new (property_class, widget);
		if (!property)
			continue;

		widget->properties = g_list_prepend (widget->properties, property);
	}

	widget->properties = g_list_reverse (widget->properties);
}

GladeWidgetClass *
glade_widget_get_class (GladeWidget *widget)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	return widget->widget_class;
}

void
glade_widget_set_project (GladeWidget *widget, GladeProject *project)
{
	if (widget->project)
		g_object_unref (widget->project);

	g_object_ref (project);
	widget->project = project;
	g_object_notify (G_OBJECT (widget), "project");
}

GladeProject *
glade_widget_get_project (GladeWidget *widget)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	return widget->project;
}

GladeProperty *
glade_widget_get_property (GladeWidget *widget, const char *id_property)
{
	GList *list;
	GladeProperty *property;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	g_return_val_if_fail (id_property != NULL, NULL);

	list = widget->properties;
	for (; list; list = list->next)
	{
		property = list->data;
		if (strcmp (property->class->id, id_property) == 0)
			return property;
	}

	list = widget->packing_properties;
	for (; list; list = list->next)
	{
		property = list->data;
		if (strcmp (property->class->id, id_property) == 0)
			return property;
	}

	g_warning ("Could not get property %s for widget %s\n",
		   id_property, widget->name);

	return NULL;
}

static gboolean
glade_widget_popup_menu (GtkWidget *widget, gpointer unused_data)
{
	GladeWidget *glade_widget;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	glade_widget = glade_widget_get_from_gtk_widget (widget);
	glade_popup_widget_pop (glade_widget, NULL);

	return TRUE;
}

/* A temp data struct that we use when looking for a widget inside a container
 * we need a struct, because the forall can only pass one pointer
 */
typedef struct {
	gint x;
	gint y;
	GtkWidget *found;
	GtkWidget *toplevel;
} GladeFindInContainerData;

static void
glade_widget_find_inside_container (GtkWidget *widget, GladeFindInContainerData *data)
{
	int x;
	int y;

	gtk_widget_translate_coordinates (data->toplevel, widget, data->x, data->y, &x, &y);
	if (x >= 0 && x < widget->allocation.width && y >= 0 && y < widget->allocation.height &&
	    (glade_widget_get_from_gtk_widget (widget) || GLADE_IS_PLACEHOLDER (widget)))
		data->found = widget;
}

static GladeWidget *
glade_widget_find_deepest_child_at_position (GtkContainer *toplevel, GtkContainer *container, int top_x, int top_y)
{
	GladeFindInContainerData data;
	data.x = top_x;
	data.y = top_y;
	data.toplevel = GTK_WIDGET (toplevel);
	data.found = NULL;

	gtk_container_forall (container, (GtkCallback) glade_widget_find_inside_container, &data);

	if (data.found && GTK_IS_CONTAINER (data.found))
		return glade_widget_find_deepest_child_at_position (toplevel, GTK_CONTAINER (data.found), top_x, top_y);
	else if (data.found)
		return glade_widget_get_from_gtk_widget (data.found);
	else
		return glade_widget_get_from_gtk_widget (container);
}

/**
 * glade_widget_retrieve_from_position:
 * @base: 
 * @x: 
 * @y:
 * 
 * Returns the real widget that was "clicked over" for a given event (coordinates) and a widget
 * For example, when a label is clicked the button press event is triggered for its parent, this
 * function takes the event and the widget that got the event and returns the real GladeWidget that was
 * clicked
 * 
 * Return Value: 
 **/
static GladeWidget *
glade_widget_retrieve_from_position (GtkWidget *base, int x, int y)
{
	GtkWidget *toplevel_widget;
	gint top_x;
	gint top_y;
	
	toplevel_widget = gtk_widget_get_toplevel (base);
	if (!GTK_WIDGET_TOPLEVEL (toplevel_widget))
		return NULL;

	gtk_widget_translate_coordinates (base, toplevel_widget, x, y, &top_x, &top_y);
	return glade_widget_find_deepest_child_at_position (GTK_CONTAINER (toplevel_widget), GTK_CONTAINER (toplevel_widget), top_x, top_y);
}

static gboolean
glade_widget_button_press (GtkWidget *widget,
			   GdkEventButton *event,
			   gpointer unused_data)
{
	double x = event->x;
	double y = event->y;
	GladeWidget *glade_widget;

	glade_widget = glade_widget_retrieve_from_position (widget, (int) (x + 0.5), (int) (y + 0.5));
	widget = glade_widget_get_widget (glade_widget);

	/* make sure to grab focus, since we may stop default handlers */
	if (GTK_WIDGET_CAN_FOCUS (widget) && !GTK_WIDGET_HAS_FOCUS (widget))
		gtk_widget_grab_focus (widget);

	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	if (event->button == 1)
	{
		/* if it's already selected don't stop default handlers, e.g. toggle button */
		if (glade_util_has_nodes (widget))
			return FALSE;

		glade_project_selection_set (glade_widget->project, widget, TRUE);
		return TRUE;
	}
	else if (event->button == 3)
	{
		glade_popup_widget_pop (glade_widget, event);
		return TRUE;
	}

	return FALSE;
}

static gboolean
glade_widget_key_press (GtkWidget *event_widget,
			GdkEventKey *event,
			gpointer unused_data)
{
	GladeWidget *glade_widget;

	g_return_val_if_fail (GTK_IS_WIDGET (event_widget), FALSE);

	glade_widget = glade_widget_get_from_gtk_widget (event_widget);

	/* We will delete all the selected items */
	if (event->keyval == GDK_Delete)
	{
		glade_util_delete_selection (glade_widget_get_project (glade_widget));
		return TRUE;
	}

	return FALSE;
}

static gboolean
glade_widget_event (GtkWidget *widget,
		    GdkEvent *event,
		    gpointer unused_data)
{
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		return glade_widget_button_press (widget,
						  (GdkEventButton*) event,
						  unused_data);
	case GDK_EXPOSE:
		glade_util_queue_draw_nodes (((GdkEventExpose*) event)->window);
		/* FIXME: For GtkFixed & GtkLayout we can draw the grid here.
		   (But don't draw it for internal children). */
		break;
	default:
		break;
	}

	return FALSE;
}

static void
glade_widget_apply_properties (GladeWidget *glade_widget)
{
	GList *properties;
	GtkWidget *widget;
	GObject *object;

	g_return_if_fail (GLADE_IS_WIDGET (glade_widget));
	g_return_if_fail (glade_widget->properties != NULL);

	properties = glade_widget->properties;
	widget = glade_widget->widget;
	object = G_OBJECT (widget);
	g_object_freeze_notify (object);

	while (properties != NULL)
	{
		glade_util_object_set_property (object, GLADE_PROPERTY (properties->data));
		properties = properties->next;
	}

	g_object_thaw_notify (G_OBJECT (widget));
}

static void
glade_widget_retrieve_properties (GladeWidget *glade_widget)
{
	GList *properties;
	GtkWidget *widget;
	GObject *object;

	g_return_if_fail (GLADE_IS_WIDGET (glade_widget));
	g_return_if_fail (glade_widget->properties != NULL);

	properties = glade_widget->properties;
	widget = glade_widget->widget;
	object = G_OBJECT (widget);

	while (properties != NULL)
	{
		GladeProperty *property = GLADE_PROPERTY (properties->data);
		GladePropertyClass *property_class = property->class;

		g_value_reset (property->value);
		g_object_get_property (object, property_class->id, property->value);

		properties = properties->next;
	}

	g_object_thaw_notify (G_OBJECT (widget));
}

/* Connects a signal handler to the 'event' signal for a widget and
   all its children recursively. We need this to draw the selection
   rectangles and to get button press/release events reliably. */
static void
glade_widget_connect_signal_handlers (GtkWidget *widget_gtk, gpointer data)
{
	g_signal_connect (G_OBJECT (widget_gtk), "event",
			  G_CALLBACK (glade_widget_event), NULL);

	/* We also need to get expose events for any children. */
	if (GTK_IS_CONTAINER (widget_gtk)) {
		gtk_container_forall (GTK_CONTAINER (widget_gtk), 
				      glade_widget_connect_signal_handlers,
				      NULL);
	}
}

void
glade_widget_set_widget (GladeWidget *glade_widget, GtkWidget *widget_gtk)
{
	g_return_if_fail (GLADE_IS_WIDGET (glade_widget));
	g_return_if_fail (GTK_IS_WIDGET (widget_gtk));
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (widget_gtk), glade_widget->widget_class->type));

	if (glade_widget->widget)
	{
		g_object_set_data (G_OBJECT (glade_widget->widget), "GladeWidgetDataTag", NULL);
		g_object_unref (glade_widget->widget);
	}

	g_object_ref (widget_gtk);
	glade_widget->widget = widget_gtk;
	g_object_set_data (G_OBJECT (widget_gtk), "GladeWidgetDataTag", glade_widget);

	gtk_widget_add_events (widget_gtk, GDK_BUTTON_PRESS_MASK |
					   GDK_BUTTON_RELEASE_MASK |
					   GDK_KEY_PRESS_MASK);

	if (GTK_WIDGET_TOPLEVEL (widget_gtk))
		g_signal_connect (G_OBJECT (widget_gtk), "delete_event",
				  G_CALLBACK (gtk_widget_hide_on_delete), NULL);

	g_signal_connect (G_OBJECT (widget_gtk), "popup_menu",
			  G_CALLBACK (glade_widget_popup_menu), NULL);
	g_signal_connect (G_OBJECT (widget_gtk), "key_press_event",
			  G_CALLBACK (glade_widget_key_press), NULL);

	glade_widget_connect_signal_handlers (widget_gtk, NULL);

	if (glade_widget->internal == NULL)
	{
		/* we should set the values of the properties of this widget from the
		 * default values that we gather from the class of this widget */
		glade_widget_apply_properties (glade_widget);
	}
	else
	{
		GladeWidget *parent = glade_widget_get_parent (glade_widget);

		/* set packing properties */
		glade_widget_set_packing_properties (glade_widget, parent);
	}

	g_object_notify (G_OBJECT (glade_widget), "widget");
}

GtkWidget *
glade_widget_get_widget (GladeWidget *widget)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	return widget->widget;
}

static void
glade_widget_real_add_signal_handler (GladeWidget *widget, GladeSignal *signal_handler)
{
	GPtrArray *signals;
	GladeSignal *new_signal_handler;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_SIGNAL (signal_handler));

	signals = glade_widget_list_signal_handlers (widget, signal_handler->name);
	if (!signals)
	{
		signals = g_ptr_array_new ();
		g_hash_table_insert (widget->signals, g_strdup (signal_handler->name), signals);
	}

	new_signal_handler = glade_signal_clone (signal_handler);
	g_ptr_array_add (signals, new_signal_handler);
}

static void
glade_widget_real_remove_signal_handler (GladeWidget *widget, GladeSignal *signal_handler)
{
	GPtrArray *signals;
	GladeSignal *tmp_signal_handler;
	size_t i;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_SIGNAL (signal_handler));

	signals = glade_widget_list_signal_handlers (widget, signal_handler->name);

	/* trying to remove an inexistent signal? */
	g_assert (signals);

	for (i = 0; i < signals->len; i++)
	{
		tmp_signal_handler = g_ptr_array_index (signals, i);
		if (glade_signal_equal (tmp_signal_handler, signal_handler))
			break;
	}

	/* trying to remove an inexistent signal? */
	g_assert(i != signals->len);

	glade_signal_free (tmp_signal_handler);
	g_ptr_array_remove_index (signals, i);
}

static void
glade_widget_real_change_signal_handler (GladeWidget *widget, GladeSignal *old_signal_handler, GladeSignal *new_signal_handler)
{
	GPtrArray *signals;
	GladeSignal *tmp_signal_handler;
	size_t i;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_SIGNAL (old_signal_handler));
	g_return_if_fail (GLADE_IS_SIGNAL (new_signal_handler));
	g_return_if_fail (strcmp (old_signal_handler->name, new_signal_handler->name) == 0);

	signals = glade_widget_list_signal_handlers (widget, old_signal_handler->name);
	/* trying to remove an inexistent signal? */
	g_assert (signals);

	for (i = 0; i < signals->len; i++)
	{
		tmp_signal_handler = g_ptr_array_index (signals, i);
		if (glade_signal_equal (tmp_signal_handler, old_signal_handler))
			break;
	}

	/* trying to remove an inexistent signal? */
	g_assert (i != signals->len);

	if (strcmp (old_signal_handler->handler, new_signal_handler->handler) != 0)
	{
		g_free (tmp_signal_handler->handler);
		tmp_signal_handler->handler = g_strdup (new_signal_handler->handler);
	}

	tmp_signal_handler->after = new_signal_handler->after;
}

GPtrArray *
glade_widget_list_signal_handlers (GladeWidget *widget, const char *signal_name) /* array of GladeSignal* */
{
	return g_hash_table_lookup (widget->signals, signal_name);
}

GladeWidget *
glade_widget_get_parent (GladeWidget *glade_widget)
{
	GladeWidget *parent = NULL;
	GtkWidget *parent_widget;

	g_return_val_if_fail (GLADE_IS_WIDGET (glade_widget), NULL);

	if (GTK_WIDGET_TOPLEVEL (glade_widget_get_widget (glade_widget)))
		return NULL;

	parent_widget = gtk_widget_get_parent (glade_widget_get_widget (glade_widget));
	parent = glade_widget_get_from_gtk_widget (parent_widget);

	return parent;
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
	GladeWidgetClass *container_class = glade_widget_get_class (container);
	GladeWidget *parent;
	GList *list = container_class->child_properties;
	GList *new_list = NULL;
	GList *ancestor_list;

	for (; list; list = list->next)
	{
		property_class = list->data;

		if (!container_class->child_property_applies (container->widget, widget->widget, property_class->id))
			continue;

		property = glade_property_new (property_class, widget);
		if (!property)
			continue;

		new_list = g_list_prepend (new_list, property);
	}

	new_list = g_list_reverse (new_list);
	parent = glade_widget_get_parent (container);
	if (parent != NULL)
	{
		ancestor_list = glade_widget_create_packing_properties (parent, widget);
		new_list = g_list_concat (new_list, ancestor_list);
	}

	return new_list;
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
static void
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

	g_list_foreach (widget->packing_properties, (GFunc) glade_property_free, NULL);
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

void
glade_widget_replace (GtkWidget *old_widget, GtkWidget *new_widget)
{
	GladeWidget *parent = NULL;
	GladeWidget *gnew_widget = NULL;
	GladeWidget *gold_widget = NULL;
	GtkWidget *real_new_widget = new_widget;
	GtkWidget *real_old_widget = old_widget;

	g_return_if_fail (GTK_IS_WIDGET (old_widget));
	g_return_if_fail (GTK_IS_WIDGET (new_widget));

	gnew_widget = glade_widget_get_from_gtk_widget (new_widget);
	gold_widget = glade_widget_get_from_gtk_widget (old_widget);

	if (gnew_widget)
		real_new_widget = glade_widget_get_widget (gnew_widget);
	if (gold_widget)
		real_old_widget = glade_widget_get_widget (gold_widget);

	parent = glade_util_get_parent (old_widget);
	g_assert (parent);

	if (parent->widget_class->replace_child)
	{
		parent->widget_class->replace_child (real_old_widget, real_new_widget, glade_widget_get_widget (parent));
		if (gnew_widget)
			glade_widget_set_packing_properties (gnew_widget, parent);
	}
	else
		g_warning ("Could not replace a placeholder because a replace "
			   " function has not been implemented for \"%s\"\n",
			   parent->widget_class->name);
}

/* XML Serialization */
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

	GPtrArray *signals = (GPtrArray *) value;
	for (i = 0; i < signals->len; i++)
	{
		GladeSignal *signal = g_ptr_array_index (signals, i);
		child = glade_signal_write (write_signals_context->context, signal);
		if (!child)
			continue;

		glade_xml_node_append_child (write_signals_context->node, child);
	}
}

GladeXmlNode *
glade_widget_write (GladeWidget *widget, GladeXmlContext *context)
{
	GladeXmlNode *node;
	GladeXmlNode *child;
	WriteSignalsContext write_signals_context;
	GList *list;

	g_return_val_if_fail (GLADE_XML_IS_CONTEXT (context), NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

	node = glade_xml_node_new (context, GLADE_XML_TAG_WIDGET);

	glade_xml_node_set_property_string (node, GLADE_XML_TAG_CLASS, widget->widget_class->name);
	glade_xml_node_set_property_string (node, GLADE_XML_TAG_ID, widget->name);

	/* Write the properties */
	list = widget->properties;
	for (; list; list = list->next) {
		GladeProperty *property = list->data;
		if (property->class->packing)
			continue;

		child = glade_property_write (context, property);
		if (!child)
			continue;

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

	child = glade_widget_write (child_widget, context);
	if (!child)
	{
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
	property = glade_widget_get_property (widget, id);
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

	g_assert (strcmp (class_name, widget->widget_class->name) == 0);
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
		glade_widget_add_signal_handler (widget, signal);
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

static GladeWidget *
glade_widget_new_from_node_real (GladeXmlNode *node,
				 GladeProject *project,
				 GladeWidget *parent)
{
	GladeWidgetClass *klass;
	GladeWidget *widget;
	const gchar *class_name;
	GObject *widget_gtk;

	if (!glade_xml_node_verify (node, GLADE_XML_TAG_WIDGET))
		return NULL;

	class_name = glade_xml_get_property_string_required (node, GLADE_XML_TAG_CLASS, NULL);
	if (!class_name)
		return NULL;

	klass = glade_widget_class_get_by_name (class_name);
	if (!klass)
		return NULL;

	widget_gtk = g_object_new (klass->type, NULL);

	if (klass->pre_create_function)
		klass->pre_create_function (widget_gtk);

	widget = g_object_new (GLADE_TYPE_WIDGET, "class", klass, "project", project,
			       "name", glade_project_new_widget_name (project, klass->generic_name),
			       "widget", widget_gtk, NULL);

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
	while (ancestor)
	{
		if (ancestor->widget_class->get_internal_child)
		{
			GtkWidget *widget;
			ancestor->widget_class->get_internal_child (ancestor->widget, internal, &widget);
			if (widget)
			{
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
glade_widget_read (GladeProject *project, GladeXmlNode *node)
{
	return glade_widget_new_from_node_real (node, project, NULL);
}
