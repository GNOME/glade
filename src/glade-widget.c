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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Joaquin Cuenca Abela <e98cuenc@yahoo.com>
 *   Chema Celorio <chema@celorio.com>
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib-object.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n-lib.h>
#include "glade.h"
#include "glade-accumulators.h"
#include "glade-project.h"
#include "glade-widget-adaptor.h"
#include "glade-widget.h"
#include "glade-marshallers.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-placeholder.h"
#include "glade-signal.h"
#include "glade-popup.h"
#include "glade-editor.h"
#include "glade-app.h"
#include "glade-design-view.h"



static void         glade_widget_set_adaptor           (GladeWidget           *widget,
							GladeWidgetAdaptor    *adaptor);
static void         glade_widget_set_properties        (GladeWidget           *widget,
							GList                 *properties);
static GParameter  *glade_widget_info_params           (GladeWidgetAdaptor    *adaptor,
							GladeWidgetInfo       *info,
							gboolean               construct,
							guint                 *n_params);
static void         glade_widget_fill_from_widget_info (GladeWidgetInfo       *info,
							GladeWidget           *widget,
							gboolean               apply_props);
static GladeWidget *glade_widget_new_from_widget_info  (GladeWidgetInfo       *info,
							GladeProject          *project,
							GladeWidget           *parent);

static gboolean     glade_window_is_embedded           (GtkWindow *window);
static void         glade_widget_embed                 (GladeWidget *widget);
static void         glade_widget_embed_using_signals   (GladeWidget *widget);

enum
{
	ADD_SIGNAL_HANDLER,
	REMOVE_SIGNAL_HANDLER,
	CHANGE_SIGNAL_HANDLER,
	BUTTON_PRESS_EVENT,
	BUTTON_RELEASE_EVENT,
	MOTION_NOTIFY_EVENT,
	ENTER_NOTIFY_EVENT,
	ACTION_ACTIVATED,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_NAME,
	PROP_INTERNAL,
	PROP_ANARCHIST,
	PROP_OBJECT,
	PROP_ADAPTOR,
	PROP_PROJECT,
	PROP_PROPERTIES,
	PROP_PARENT,
	PROP_INTERNAL_NAME,
	PROP_TEMPLATE,
	PROP_INFO,
	PROP_REASON
};

static guint         glade_widget_signals[LAST_SIGNAL] = {0};
static GObjectClass *parent_class = NULL;

/* Sometimes we need to use the project deep in the loading code,
 * this is just a shortcut way to get the project.
 */
static GladeProject *loading_project = NULL;
static GQuark        glade_widget_name_quark = 0;

/* An optimization to avoid looking up the deepest
 * widget more than once in an event.
 */
static GladeWidget  *deep_event_widget = NULL;

/*******************************************************************************
                           GladeWidget class methods
 *******************************************************************************/
static void
glade_widget_add_child_impl (GladeWidget  *widget,
			     GladeWidget  *child,
			     gboolean      at_mouse)
{
	/* Safe to set the parent first... setting it afterwards
	 * creates packing properties, and that is not always
	 * desirable.
	 */
	glade_widget_set_parent (child, widget);

	glade_widget_adaptor_add 
		(widget->adaptor, widget->object, child->object);

	glade_widget_set_packing_properties (child, widget);
}

static void
glade_widget_remove_child_impl (GladeWidget  *widget,
				GladeWidget  *child)
{
	glade_widget_adaptor_remove
		(widget->adaptor, widget->object, child->object);
}

static void
glade_widget_replace_child_impl (GladeWidget *widget,
				 GObject     *old_object,
				 GObject     *new_object)
{
	GladeWidget *gnew_widget = glade_widget_get_from_gobject (new_object);
	GladeWidget *gold_widget = glade_widget_get_from_gobject (old_object);

	if (gnew_widget) gnew_widget->parent = widget;
	if (gold_widget && gold_widget != gnew_widget)
		gold_widget->parent = NULL;

	/* Setup packing properties here so that they are available on the new
	 * widget for use in the plugin.
	 */
	if (gnew_widget) 
		glade_widget_set_packing_properties (gnew_widget, widget);


	glade_widget_adaptor_replace_child 
		(widget->adaptor, widget->object,
		 old_object, new_object);

	/* Setup packing properties here again so we can introspect the new
	 * values from the plugin.
	 */
	if (gnew_widget) 
		glade_widget_set_packing_properties (gnew_widget, widget);
}

static void
glade_widget_add_signal_handler_impl (GladeWidget *widget, GladeSignal *signal_handler)
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
glade_widget_remove_signal_handler_impl (GladeWidget *widget, GladeSignal *signal_handler)
{
	GPtrArray   *signals;
	GladeSignal *tmp_signal_handler;
	guint        i;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_SIGNAL (signal_handler));

	signals = glade_widget_list_signal_handlers (widget, signal_handler->name);

	/* trying to remove an inexistent signal? */
	g_assert (signals);

	for (i = 0; i < signals->len; i++)
	{
		tmp_signal_handler = g_ptr_array_index (signals, i);
		if (glade_signal_equal (tmp_signal_handler, signal_handler))
		{
			glade_signal_free (tmp_signal_handler);
			g_ptr_array_remove_index (signals, i);
			break;
		}
	}
}

static void
glade_widget_change_signal_handler_impl (GladeWidget *widget,
					 GladeSignal *old_signal_handler,
					 GladeSignal *new_signal_handler)
{
	GPtrArray   *signals;
	GladeSignal *tmp_signal_handler;
	guint        i;
	
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
		{
			if (strcmp (old_signal_handler->handler,
				    new_signal_handler->handler) != 0)
			{
				g_free (tmp_signal_handler->handler);
				tmp_signal_handler->handler =
					g_strdup (new_signal_handler->handler);
			}

			/* Handler */
			if (tmp_signal_handler->handler)
				g_free (tmp_signal_handler->handler);
			tmp_signal_handler->handler =
				g_strdup (new_signal_handler->handler);
			
			/* Object */
			if (tmp_signal_handler->userdata)
				g_free (tmp_signal_handler->userdata);
			tmp_signal_handler->userdata = 
				g_strdup (new_signal_handler->userdata);
			
			tmp_signal_handler->after  = new_signal_handler->after;
			tmp_signal_handler->lookup = new_signal_handler->lookup;
			break;
		}
	}
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
	if (GTK_WIDGET_MAPPED(widget) &&
	    x >= 0 && x < widget->allocation.width && y >= 0 && y < widget->allocation.height)
	{
		if (glade_widget_get_from_gobject (widget))
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
					      glade_widget_find_inside_container, &search);

			data->found = search.found;
		}
	}
}

static GladeWidget *
glade_widget_find_deepest_child_at_position (GtkContainer *toplevel,
					     GtkContainer *container,
					     int top_x, int top_y)
{
	GladeFindInContainerData data;

	data.x = top_x;
	data.y = top_y;
	data.toplevel = GTK_WIDGET (toplevel);
	data.found = NULL;

	gtk_container_forall (container, (GtkCallback)
			      glade_widget_find_inside_container, &data);

	if (data.found && GTK_IS_CONTAINER (data.found))
		return glade_widget_find_deepest_child_at_position
			(toplevel, GTK_CONTAINER (data.found), top_x, top_y);
	else if (data.found)
		return glade_widget_get_from_gobject (data.found);
	else
		return glade_widget_get_from_gobject (container);
}

/*
 * Returns: the real widget that was "clicked over" for a given event 
 * (coordinates) and a widget. 
 * For example, when a label is clicked the button press event is triggered 
 * for its parent, this function takes the event and the widget that got the 
 * event and returns the real #GladeWidget that was clicked
 *
 */
GladeWidget *
glade_widget_retrieve_from_position (GtkWidget *base, int x, int y)
{
	GladeWidget *lookup;
	GtkWidget   *widget;
	gint         top_x;
	gint         top_y;
	
	widget = gtk_widget_get_toplevel (base);
	if (!GTK_WIDGET_TOPLEVEL (widget))
		return NULL;

	gtk_widget_translate_coordinates (base, widget, x, y, &top_x, &top_y);

	lookup = glade_widget_find_deepest_child_at_position
		(GTK_CONTAINER (widget), GTK_CONTAINER (widget), top_x, top_y);

	return lookup;
}

static gboolean
glade_widget_button_press_event_impl (GladeWidget    *gwidget,
				      GdkEvent       *base_event)
{
	GladeWidget       *glade_widget;
	GtkWidget         *event_widget, *widget;
	GdkEventButton    *event = (GdkEventButton *)base_event;
	gboolean           handled = FALSE;

	/* Get event widget and event glade_widget
	 */
	gdk_window_get_user_data (event->window, (gpointer)&event_widget);
	if ((glade_widget = deep_event_widget) == NULL)
		return FALSE;

	/* make sure to grab focus, since we may stop default handlers */
	widget = GTK_WIDGET (glade_widget_get_object (glade_widget));
	if (GTK_WIDGET_CAN_FOCUS (widget) && !GTK_WIDGET_HAS_FOCUS (widget))
		gtk_widget_grab_focus (widget);

	/* if it's already selected don't stop default handlers, e.g. toggle button */
	if (event->button == 1)
	{
		if (event->state & GDK_CONTROL_MASK)
		{
			if (glade_project_is_selected (glade_widget->project,
						       glade_widget->object))
				glade_app_selection_remove 
					(glade_widget->object, TRUE);
			else
				glade_app_selection_add
					(glade_widget->object, TRUE);
			handled = TRUE;
		}
		else if (glade_project_is_selected (glade_widget->project,
						    glade_widget->object) == FALSE)
		{
			glade_util_clear_selection ();
			glade_app_selection_set 
				(glade_widget->object, TRUE);
			handled = TRUE;
		}
	}
	else if (event->button == 3)
	{
		glade_popup_widget_pop (glade_widget, event, TRUE);
		handled = TRUE;
	}

	return handled;
}

static gboolean    
glade_widget_hide_on_delete (GtkWidget *widget,
			     GdkEvent *event,
			     gpointer user_data)
{
	GladeWidget *gwidget =
		glade_widget_get_from_gobject (widget);
	glade_widget_hide (gwidget);
	return TRUE;
}

static gboolean
glade_widget_popup_menu (GtkWidget *widget, gpointer unused_data)
{
	GladeWidget *glade_widget;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	glade_widget = glade_widget_get_from_gobject (widget);
	glade_popup_widget_pop (glade_widget, NULL, TRUE);

	return TRUE;
}

static void
glade_widget_setup_events (GladeWidget *gwidget,
			   GtkWidget   *widget)
{
	gtk_widget_add_events (widget,
			       GDK_POINTER_MOTION_MASK      |
			       GDK_POINTER_MOTION_HINT_MASK |
			       GDK_BUTTON_PRESS_MASK        |
			       GDK_BUTTON_RELEASE_MASK      |
			       GDK_ENTER_NOTIFY_MASK);
	
}

static gboolean
glade_widget_event (GtkWidget   *widget,
		    GdkEvent    *event,
		    GladeWidget *gwidget)
{
	gboolean handled = FALSE;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	switch (event->type) 
	{
	case GDK_BUTTON_PRESS:
		g_signal_emit (gwidget, 
			       glade_widget_signals[BUTTON_PRESS_EVENT], 0, 
			       event, &handled);
		break;
	case GDK_BUTTON_RELEASE:
		g_signal_emit (gwidget, 
			       glade_widget_signals[BUTTON_RELEASE_EVENT], 0, 
			       event, &handled);
		break;
	case GDK_MOTION_NOTIFY:
		g_signal_emit (gwidget, 
			       glade_widget_signals[MOTION_NOTIFY_EVENT], 0, 
			       event, &handled);
		break;
	case GDK_ENTER_NOTIFY:
		g_signal_emit (gwidget, 
			       glade_widget_signals[ENTER_NOTIFY_EVENT], 0, 
			       event, &handled);
		break;
	case GDK_EXPOSE:
	case GDK_CONFIGURE:
		glade_util_queue_draw_nodes (((GdkEventExpose*) event)->window);
		break;
	default:
		break;
	}

	return handled;
}

static gboolean
glade_widget_event_private (GtkWidget   *widget,
			    GdkEvent    *event,
			    GladeWidget *gwidget)
{
	GtkWidget *event_widget;
	gboolean   handled;
	gint       x, y;

	/* Some synthetic events (like focus-change) dont come with a window */
	if (((GdkEventAny *)event)->window == NULL) return FALSE;
	
	/* Get the widget at mouse position before anything else
	 */
	gdk_window_get_user_data (((GdkEventAny *)event)->window, (gpointer)&event_widget);

	/* This manages to happen sometimes... lord knows why */
	if (event_widget == NULL)
		return FALSE;

	gtk_widget_get_pointer (event_widget, &x, &y);
	deep_event_widget = 
		glade_widget_retrieve_from_position (event_widget, x, y);

	/* Check if there are deep fixed widgets without windows
	 * that need to be processed first.
	 */
	if ((handled = glade_util_deep_fixed_event (widget, event, gwidget)) == FALSE)
	{

		/* Run the real class handler now.
		 */
		handled = GLADE_WIDGET_GET_CLASS (gwidget)->event (widget, event, gwidget);

#if 0
		if (event->type != GDK_EXPOSE)
			g_print ("event widget '%s' handled '%d' event '%d'\n",
				 deep_event_widget->name, handled, event->type);
#endif
	}
	return handled;
}

/*******************************************************************************
                      GObjectClass & Object Construction
 *******************************************************************************/

/*
 * This function creates new GObject parameters based on the GType of the 
 * GladeWidgetAdaptor and its default values.
 *
 * If a GladeWidget is specified, it will be used to apply the
 * values currently in use.
 */
static GParameter *
glade_widget_template_params (GladeWidget      *widget,
			      gboolean          construct,
			      guint            *n_params)
{
	GladeWidgetAdaptor    *klass;
	GArray              *params;
	GObjectClass        *oclass;
	GParamSpec         **pspec;
	GladeProperty       *glade_property;
	GladePropertyClass  *pclass;
	guint                n_props, i;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	g_return_val_if_fail (n_params != NULL, NULL);

	klass = widget->adaptor;
	
	/* As a slight optimization, we never unref the class
	 */
	oclass = g_type_class_ref (klass->type);
	pspec  = g_object_class_list_properties (oclass, &n_props);
	params = g_array_new (FALSE, FALSE, sizeof (GParameter));

	for (i = 0; i < n_props; i++)
	{
		GParameter parameter = { 0, };

		if ((glade_property = 
		     glade_widget_get_property (widget, pspec[i]->name)) == NULL)
			continue;

		pclass = glade_property->klass;
		
		/* Ignore properties based on some criteria
		 */
		if (pclass == NULL  || /* Unaccounted for in the builder */
		    pclass->virt    || /* should not be set before 
					  GladeWidget wrapper exists */
		    pclass->ignore)    /* Catalog explicitly ignores the object */
			continue;

		if (construct &&
		    (pspec[i]->flags & 
		     (G_PARAM_CONSTRUCT|G_PARAM_CONSTRUCT_ONLY)) == 0)
			continue;
		else if (!construct &&
			 (pspec[i]->flags & 
			  (G_PARAM_CONSTRUCT|G_PARAM_CONSTRUCT_ONLY)) != 0)
			continue;

		if (g_value_type_compatible (G_VALUE_TYPE (pclass->def),
					     pspec[i]->value_type) == FALSE)
		{
			g_critical ("Type mismatch on %s property of %s",
				    parameter.name, klass->name);
			continue;
		}

		if (g_param_values_cmp (pspec[i], 
					glade_property->value, 
					pclass->orig_def) == 0)
			continue;


		parameter.name = pspec[i]->name; /* These are not copied/freed */
		g_value_init (&parameter.value, pspec[i]->value_type);
		g_value_copy (glade_property->value, &parameter.value);
		
		g_array_append_val (params, parameter);
	}
	g_free (pspec);

	*n_params = params->len;
	return (GParameter *)g_array_free (params, FALSE);
}

static void
free_params (GParameter *params, guint n_params)
{
	gint i;
	for (i = 0; i < n_params; i++)
		g_value_unset (&(params[i].value));
	g_free (params);

}

static GObject *
glade_widget_build_object (GladeWidgetAdaptor *adaptor, GladeWidget *widget, GladeWidgetInfo *info)
{
	GParameter          *params;
	GObject             *object;
	guint                n_params, i;

	if (widget)
		params = glade_widget_template_params (widget, TRUE, &n_params);
	else if (info)
		params = glade_widget_info_params (adaptor, info, TRUE, &n_params);
	else
		params = glade_widget_adaptor_default_params (adaptor, TRUE, &n_params);

	/* Create the new object with the correct parameters.
	 */
	object = g_object_newv (adaptor->type, n_params, params);

	free_params (params, n_params);

	if (widget)
		params = glade_widget_template_params (widget, FALSE, &n_params);
	else if (info)
		params = glade_widget_info_params (adaptor, info, FALSE, &n_params);
	else
		params = glade_widget_adaptor_default_params (adaptor, FALSE, &n_params);

	for (i = 0; i < n_params; i++)
	{
		g_object_set_property (object, params[i].name, &(params[i].value));
	}

	free_params (params, n_params);

	return object;
}

/**
 * glade_widget_dup_properties:
 * @template_props: the #GladeProperty list to copy
 * @as_load: whether to behave as if loading the project
 *
 * Copies a list of properties, if @as_load is specified, then
 * properties that are not saved to the glade file are ignored.
 *
 * Returns: A newly allocated #GList of new #GladeProperty objects.
 */
GList *
glade_widget_dup_properties (GList *template_props, gboolean as_load)
{
	GList *list, *properties = NULL;

	for (list = template_props; list && list->data; list = list->next)
	{
		GladeProperty *prop = list->data;
		
		if (prop->klass->save == FALSE && as_load)
			continue;

		properties = g_list_prepend (properties, glade_property_dup (prop, NULL));
	}
	return g_list_reverse (properties);
}

/**
 * glade_widget_remove_property:
 * @widget: A #GladeWidget
 * @id_property: the name of the property
 *
 * Removes the #GladeProperty indicated by @id_property
 * from @widget (this is intended for use in the plugin, to
 * remove properties from composite children that dont make
 * sence to allow the user to specify, notably - properties
 * that are proxied through the composite widget's properties or
 * style properties).
 */
void
glade_widget_remove_property (GladeWidget  *widget,
			      const gchar  *id_property)
{
	GladeProperty *prop;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (id_property);

	if ((prop = glade_widget_get_property (widget, id_property)) != NULL)
	{
		widget->properties = g_list_remove (widget->properties, prop);
		g_object_unref (prop);
	}
	else
		g_critical ("Couldnt find property %s on widget %s\n",
			    id_property, widget->name);
}


static void
glade_widget_sync_custom_props (GladeWidget *widget)
{
	GList *l;
	for (l = widget->properties; l && l->data; l = l->next)
	{
		GladeProperty *prop  = GLADE_PROPERTY(l->data);

		/* XXX We need a better option to this hack.
		 *
		 * This used to be based on whether a function was
		 * provided by the backend to treat the said property, now
		 * that function is classwide so we dont know, so currently
		 * we are just syncing all properties for the sake of those
		 * properties.
		 */
		if (!prop->klass->construct_only)
			glade_property_sync (prop);

	}
}

static void
glade_widget_sync_packing_props (GladeWidget *widget)
{
	GList *l;
	for (l = widget->packing_properties; l && l->data; l = l->next) {
		GladeProperty *prop  = GLADE_PROPERTY(l->data);
		glade_property_sync (prop);
	}
}


static GObject *
glade_widget_constructor (GType                  type,
			  guint                  n_construct_properties,
			  GObjectConstructParam *construct_properties)
{
	GladeWidget      *gwidget;
	GObject          *ret_obj, *object;
	GList            *properties = NULL, *list;

	ret_obj = G_OBJECT_CLASS (parent_class)->constructor
		(type, n_construct_properties, construct_properties);

	gwidget = GLADE_WIDGET (ret_obj);

	if (gwidget->name == NULL)
	{
		if (gwidget->internal)
		{
			gchar *name_base = g_strdup_printf ("%s-%s", 
							    gwidget->construct_internal, 
							    gwidget->internal);
			
			if (gwidget->project)
			{
				gwidget->name = 
					glade_project_new_widget_name (gwidget->project, 
								       name_base);
				g_free (name_base);
			}
			else
				gwidget->name = name_base;

		}
		else if (gwidget->project)
			gwidget->name = glade_project_new_widget_name
				(GLADE_PROJECT (gwidget->project), 
				 gwidget->adaptor->generic_name);
		else
			gwidget->name = 
				g_strdup (gwidget->adaptor->generic_name);
	}

	if (gwidget->construct_template)
	{
		properties = glade_widget_dup_properties
			(gwidget->construct_template->properties, FALSE);
		
		glade_widget_set_properties (gwidget, properties);
	}

	if (gwidget->object == NULL)
	{
		object = glade_widget_build_object(gwidget->adaptor, 
						   gwidget->construct_template, 
						   gwidget->construct_info);
		glade_widget_set_object (gwidget, object);
	}

	/* Introspect object properties before passing it to post_create,
	 * but only when its freshly created (depend on glade file at
	 * load time and copying properties at dup time).
	 */
	if (gwidget->internal != NULL &&
	    gwidget->construct_reason == GLADE_CREATE_USER)
		for (list = gwidget->properties; list; list = list->next)
			glade_property_load (GLADE_PROPERTY (list->data));

	/* Only call this once the GladeWidget is completely built
	 * (but before calling custom handlers...)
	 */
	glade_widget_adaptor_post_create (gwidget->adaptor, 
					  gwidget->object,
					  gwidget->construct_reason);

	/* Virtual properties need to be explicitly synchronized.
	 */
	if (gwidget->construct_reason == GLADE_CREATE_USER)
		glade_widget_sync_custom_props (gwidget);

	if (gwidget->parent && gwidget->packing_properties == NULL)
		glade_widget_set_packing_properties (gwidget, gwidget->parent);

	if (GTK_IS_WIDGET (gwidget->object) && !GTK_WIDGET_TOPLEVEL (gwidget->object))
	{
		gwidget->visible = TRUE;
		gtk_widget_show_all (GTK_WIDGET (gwidget->object));
	}
	else if (GTK_IS_WIDGET (gwidget->object) == FALSE)
		gwidget->visible = TRUE;
	
	return ret_obj;
}

static void
glade_widget_finalize (GObject *object)
{
	GladeWidget *widget = GLADE_WIDGET (object);

	g_return_if_fail (GLADE_IS_WIDGET (object));

	g_free (widget->name);
	g_free (widget->internal);
	g_hash_table_destroy (widget->signals);

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
glade_widget_dispose (GObject *object)
{
	GladeWidget *widget = GLADE_WIDGET (object);

	g_return_if_fail (GLADE_IS_WIDGET (object));

	if (GTK_IS_OBJECT (widget->object))
		gtk_object_destroy (GTK_OBJECT (widget->object));
	else 
		g_object_unref (widget->object);

	if (widget->properties)
	{
		g_list_foreach (widget->properties, (GFunc)g_object_unref, NULL);
		g_list_free (widget->properties);
	}
	
	if (widget->packing_properties)
	{
		g_list_foreach (widget->packing_properties, (GFunc)g_object_unref, NULL);
		g_list_free (widget->packing_properties);
	}

 	if (G_OBJECT_CLASS(parent_class)->dispose)
		G_OBJECT_CLASS(parent_class)->dispose(object);
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
	case PROP_ANARCHIST:
		widget->anarchist = g_value_get_boolean (value);
		break;
	case PROP_OBJECT:
		if (g_value_get_object (value))
			glade_widget_set_object (widget, g_value_get_object (value));
		break;
	case PROP_PROJECT:
		glade_widget_set_project (widget, GLADE_PROJECT (g_value_get_object (value)));
		break;
	case PROP_ADAPTOR:
		glade_widget_set_adaptor (widget, GLADE_WIDGET_ADAPTOR
					  (g_value_get_object (value)));
		break;
	case PROP_PROPERTIES:
		glade_widget_set_properties (widget, (GList *)g_value_get_pointer (value));
		break;
	case PROP_PARENT:
		glade_widget_set_parent (widget, GLADE_WIDGET (g_value_get_object (value)));
		break;
	case PROP_INTERNAL_NAME:
		if (g_value_get_string (value))
			widget->construct_internal = g_value_dup_string (value);
		break;
	case PROP_TEMPLATE:
		widget->construct_template = g_value_get_object (value);
		break;
	case PROP_INFO:
		widget->construct_info = g_value_get_pointer (value);
		break;
	case PROP_REASON:
		widget->construct_reason = g_value_get_int (value);
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
	case PROP_ANARCHIST:
		g_value_set_boolean (value, widget->anarchist);
		break;
	case PROP_ADAPTOR:
		g_value_set_object (value, widget->adaptor);
		break;
	case PROP_PROJECT:
		g_value_set_object (value, G_OBJECT (widget->project));
		break;
	case PROP_OBJECT:
		g_value_set_object (value, widget->object);
		break;
	case PROP_PROPERTIES:
		g_value_set_pointer (value, widget->properties);
		break;
	case PROP_PARENT:
		g_value_set_object (value, widget->parent);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
free_signals (gpointer value)
{
	GPtrArray *signals = (GPtrArray*) value;
	guint i;
	guint nb_signals;

	if (signals == NULL)
		return;

	/* g_ptr_array_foreach (signals, (GFunc) glade_signal_free, NULL);
	 * only available in modern versions of Gtk+ */
	nb_signals = signals->len;
	for (i = 0; i < nb_signals; i++)
		glade_signal_free (g_ptr_array_index (signals, i));

	g_ptr_array_free (signals, TRUE);
}

static void
glade_widget_init (GladeWidget *widget)
{
	widget->adaptor = NULL;
	widget->project = NULL;
	widget->name = NULL;
	widget->internal = NULL;
	widget->object = NULL;
	widget->properties = NULL;
	widget->packing_properties = NULL;
	widget->prop_refs = NULL;
	widget->prop_refs_readonly = FALSE;
	widget->signals = g_hash_table_new_full
		(g_str_hash, g_str_equal,
		 (GDestroyNotify) g_free,
		 (GDestroyNotify) free_signals);
}

static void
glade_widget_class_init (GladeWidgetClass *klass)
{
	GObjectClass *object_class;

	if (glade_widget_name_quark == 0)
		glade_widget_name_quark = 
			g_quark_from_static_string ("GladeWidgetDataTag");

	object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->constructor     = glade_widget_constructor;
	object_class->finalize        = glade_widget_finalize;
	object_class->dispose         = glade_widget_dispose;
	object_class->set_property    = glade_widget_set_real_property;
	object_class->get_property    = glade_widget_get_real_property;

	klass->add_child              = glade_widget_add_child_impl;
	klass->remove_child           = glade_widget_remove_child_impl;
	klass->replace_child          = glade_widget_replace_child_impl;
	klass->setup_events           = glade_widget_setup_events;
	klass->event                  = glade_widget_event;

	klass->add_signal_handler     = glade_widget_add_signal_handler_impl;
	klass->remove_signal_handler  = glade_widget_remove_signal_handler_impl;
	klass->change_signal_handler  = glade_widget_change_signal_handler_impl;

	klass->button_press_event     = glade_widget_button_press_event_impl;
	klass->button_release_event   = NULL;
	klass->motion_notify_event    = NULL;
	klass->enter_notify_event     = NULL;
	
	g_object_class_install_property
		(object_class, PROP_NAME,
		 g_param_spec_string ("name", _("Name"),
				      _("The name of the widget"),
				      NULL,
				      G_PARAM_READWRITE |
				      G_PARAM_CONSTRUCT));

	g_object_class_install_property
		(object_class, PROP_INTERNAL,
		 g_param_spec_string ("internal", _("Internal name"),
				      _("The internal name of the widget"),
				      NULL, G_PARAM_READWRITE |
				      G_PARAM_CONSTRUCT));
	
	g_object_class_install_property
		(object_class, PROP_ANARCHIST,
		 g_param_spec_boolean ("anarchist", _("Anarchist"),
				       _("Whether this composite child is "
					 "an ancestral child or an anarchist child"),
				       FALSE, G_PARAM_READWRITE |
				       G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property
		(object_class, PROP_OBJECT,
		 g_param_spec_object ("object", _("Object"),
				      _("The object associated"),
				      G_TYPE_OBJECT,
				      G_PARAM_READWRITE |
				      G_PARAM_CONSTRUCT));

	g_object_class_install_property
		(object_class, PROP_ADAPTOR,
		   g_param_spec_object ("adaptor", _("Adaptor"),
					_("The class adaptor for the associated widget"),
					GLADE_TYPE_WIDGET_ADAPTOR,
					G_PARAM_READWRITE |
					G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property
		(object_class, PROP_PROJECT,
		 g_param_spec_object ("project", _("Project"),
				      _("The glade project that "
					"this widget belongs to"),
				      GLADE_TYPE_PROJECT,
				      G_PARAM_READWRITE |
				      G_PARAM_CONSTRUCT));

	g_object_class_install_property
		(object_class, PROP_PROPERTIES,
		 g_param_spec_pointer ("properties", _("Properties"),
				       _("A list of GladeProperties"),
				       G_PARAM_READWRITE |
				       G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property
		(object_class, 	PROP_PARENT,
		 g_param_spec_object ("parent", _("Parent"),
				      _("A pointer to the parenting GladeWidget"),
				      GLADE_TYPE_WIDGET,
				      G_PARAM_READWRITE |
				      G_PARAM_CONSTRUCT));

	g_object_class_install_property
		(object_class, 	PROP_INTERNAL_NAME,
		 g_param_spec_string ("internal-name", _("Internal Name"),
				      _("A generic name prefix for internal widgets"),
				      NULL, G_PARAM_CONSTRUCT_ONLY|G_PARAM_WRITABLE));

	g_object_class_install_property
		(object_class, 	PROP_TEMPLATE,
		 g_param_spec_object ("template", _("Template"),
				       _("A GladeWidget template to base a new widget on"),
				      GLADE_TYPE_WIDGET,
				      G_PARAM_CONSTRUCT_ONLY|G_PARAM_WRITABLE));

	g_object_class_install_property
		(object_class, 	PROP_INFO,
		 g_param_spec_pointer ("info", _("Info"),
				       _("A GladeWidgetInfo struct to base a new widget on"),
				       G_PARAM_CONSTRUCT_ONLY|G_PARAM_WRITABLE));

	g_object_class_install_property
		(object_class, 	PROP_REASON,
		 g_param_spec_int ("reason", _("Reason"),
				   _("A GladeCreateReason for this creation"),
				   GLADE_CREATE_USER,
				   GLADE_CREATE_REASONS - 1,
				   GLADE_CREATE_USER,
				   G_PARAM_CONSTRUCT_ONLY|G_PARAM_WRITABLE));
	
	/**
	 * GladeWidget::add-signal-handler:
	 * @gladewidget: the #GladeWidget which received the signal.
	 * @arg1: the #GladeSignal that was added to @gladewidget.
	 */
	glade_widget_signals[ADD_SIGNAL_HANDLER] =
		g_signal_new ("add-signal-handler",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeWidgetClass, add_signal_handler),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);

	/**
	 * GladeWidget::remove-signal-handler:
	 * @gladewidget: the #GladeWidget which received the signal.
	 * @arg1: the #GladeSignal that was removed from @gladewidget.
	 */
	glade_widget_signals[REMOVE_SIGNAL_HANDLER] =
		g_signal_new ("remove-signal-handler",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeWidgetClass, remove_signal_handler),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);


	/**
	 * GladeWidget::change-signal-handler:
	 * @gladewidget: the #GladeWidget which received the signal.
	 * @arg1: the old #GladeSignal
	 * @arg2: the new #GladeSignal
	 */
	glade_widget_signals[CHANGE_SIGNAL_HANDLER] =
		g_signal_new ("change-signal-handler",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeWidgetClass, change_signal_handler),
			      NULL, NULL,
			      glade_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_POINTER, G_TYPE_POINTER);


	/**
	 * GladeWidget::button-press-event:
	 * @gladewidget: the #GladeWidget which received the signal.
	 * @arg1: the #GdkEvent
	 */
	glade_widget_signals[BUTTON_PRESS_EVENT] =
		g_signal_new ("button-press-event",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeWidgetClass, button_press_event),
			      glade_boolean_handled_accumulator, NULL,
			      glade_marshal_BOOLEAN__BOXED,
			      G_TYPE_BOOLEAN, 1,
			      GDK_TYPE_EVENT);

	/**
	 * GladeWidget::button-relese-event:
	 * @gladewidget: the #GladeWidget which received the signal.
	 * @arg1: the #GdkEvent
	 */
	glade_widget_signals[BUTTON_RELEASE_EVENT] =
		g_signal_new ("button-release-event",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeWidgetClass, button_release_event),
			      glade_boolean_handled_accumulator, NULL,
			      glade_marshal_BOOLEAN__BOXED,
			      G_TYPE_BOOLEAN, 1,
			      GDK_TYPE_EVENT);


	/**
	 * GladeWidget::motion-notify-event:
	 * @gladewidget: the #GladeWidget which received the signal.
	 * @arg1: the #GdkEvent
	 */
	glade_widget_signals[MOTION_NOTIFY_EVENT] =
		g_signal_new ("motion-notify-event",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeWidgetClass, motion_notify_event),
			      glade_boolean_handled_accumulator, NULL,
			      glade_marshal_BOOLEAN__BOXED,
			      G_TYPE_BOOLEAN, 1,
			      GDK_TYPE_EVENT);


	/**
	 * GladeWidget::enter-notify-event:
	 * @gladewidget: the #GladeWidget which received the signal.
	 * @arg1: the #GdkEvent
	 */
	glade_widget_signals[ENTER_NOTIFY_EVENT] =
		g_signal_new ("enter-notify-event",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeWidgetClass, enter_notify_event),
			      glade_boolean_handled_accumulator, NULL,
			      glade_marshal_BOOLEAN__BOXED,
			      G_TYPE_BOOLEAN, 1,
			      GDK_TYPE_EVENT);

	/**
	 * GladeWidget::action-activated:
	 * @widget: the #GladeWidget which received the signal.
	 * @action_id: the action id (signal detail) or NULL.
	 *
	 * Use this to catch up actions. This signal is proxied from 
	 * GladeWidgetAdaptor's "action-emited" signal default handler.
	 *
	 * Returns TRUE to stop others handlers being invoked.
	 *
	 */
	glade_widget_signals [ACTION_ACTIVATED] =
		g_signal_new ("action-activated",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      G_STRUCT_OFFSET (GladeWidgetClass, action_activated),
			      glade_boolean_handled_accumulator, NULL,
			      glade_marshal_BOOLEAN__STRING,
			      G_TYPE_BOOLEAN, 1, G_TYPE_STRING);
}

GType
glade_widget_get_type (void)
{
	static GType widget_type = 0;

	if (!widget_type)
	{
		static const GTypeInfo widget_info =
		{
			sizeof (GladeWidgetClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) glade_widget_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GladeWidget),
			0,		/* n_preallocs */
			(GInstanceInitFunc) glade_widget_init,
		};

		widget_type = g_type_register_static (G_TYPE_OBJECT, "GladeWidget",
						      &widget_info, 0);
	}

	return widget_type;
}

/*******************************************************************************
                                Static stuff....
 *******************************************************************************/
static void
glade_widget_copy_packing_props (GladeWidget *parent,
				 GladeWidget *child,
				 GladeWidget *template_widget)
{
	GladeProperty *dup_prop, *orig_prop;
	GList         *l;

	g_return_if_fail (child->parent == parent);

	glade_widget_set_packing_properties (child, parent);

	for (l = child->packing_properties; l && l->data; l = l->next)
	{
		dup_prop  = GLADE_PROPERTY(l->data);
		orig_prop =
			glade_widget_get_property (template_widget, dup_prop->klass->id);
		glade_property_set_value (dup_prop, orig_prop->value);
	}
}

static void
glade_widget_set_default_packing_properties (GladeWidget *container,
					     GladeWidget *child)
{
	GladePropertyClass *property_class;
	GList              *l;

	for (l = container->adaptor->packing_props; l; l = l->next) 
	{
		const gchar  *def;
		GValue       *value;

		property_class = l->data;

		if ((def = 
		     glade_widget_adaptor_get_packing_default
		     (child->adaptor, container->adaptor, property_class->id)) == NULL)
			continue;
		
		value = glade_property_class_make_gvalue_from_string (property_class,
								      def,
								      child->project);
		
		glade_widget_child_set_property (container, child,
						 property_class->id, value);
		g_value_unset (value);
		g_free (value);
	}
}

/**
 * When looking for an internal child we have to walk up the hierarchy...
 */
static GObject *
glade_widget_get_internal_child (GladeWidget *parent,
				 const gchar *internal)                
{
	while (parent)
	{
		GladeWidgetAdaptorClass *adaptor_class =
			GLADE_WIDGET_ADAPTOR_GET_CLASS (parent->adaptor);

		if (adaptor_class->get_internal_child)
			return glade_widget_adaptor_get_internal_child
				(parent->adaptor, parent->object, internal);

		parent = glade_widget_get_parent (parent);
	}
	return NULL;
}

static GladeGetInternalFunc
glade_widget_get_internal_func (GladeWidget *parent, GladeWidget **parent_ret)
{
	GladeWidget *gwidget;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (parent), NULL);
	
	for (gwidget = parent; gwidget; gwidget = gwidget->parent)
	{
		GladeWidgetAdaptorClass *adaptor_class =
			GLADE_WIDGET_ADAPTOR_GET_CLASS (gwidget->adaptor);

		if (adaptor_class->get_internal_child)
		{
			if (parent_ret) *parent_ret = gwidget;
			return adaptor_class->get_internal_child;
		}
	}
	g_error ("No internal child search function "
		 "provided for widget class %s (or any parents)",
		 parent->adaptor->name);

	return NULL;
}


static GladeWidget *
glade_widget_dup_internal (GladeWidget *parent, GladeWidget *template_widget)
{
	GladeGetInternalFunc  get_internal;
	GladeWidget *gwidget = NULL, *internal_parent;
	GList       *list, *children;
	GtkWidget   *placeholder;
	gchar       *child_type;
	
	g_return_val_if_fail (template_widget != NULL && GLADE_IS_WIDGET(template_widget), NULL);
	g_return_val_if_fail (parent == NULL || GLADE_IS_WIDGET (parent), NULL);

	/* Dont actually duplicate internal widgets, but recurse through them anyway. */
	if (template_widget->internal)
	{
		GObject *internal_object = NULL;

		if (parent && 
		    (get_internal = 
		     glade_widget_get_internal_func (parent, &internal_parent)) != NULL)
		{
			/* We cant use "parent" here, we have to recurse up the hierarchy to find
			 * the "parent" that has `get_internal_child' support (i.e. internal children
			 * may have depth).
			 */
			internal_object = get_internal (internal_parent->adaptor,
							internal_parent->object, 
							template_widget->internal);
			g_assert (internal_object);
			
			gwidget = glade_widget_get_from_gobject (internal_object);
			g_assert (gwidget);
		}
	}
	else
	{
		gchar *name = glade_project_new_widget_name (template_widget->project, template_widget->name);
		gwidget = glade_widget_adaptor_create_widget
			(template_widget->adaptor, FALSE,
			 "name", name,
			 "parent", parent, 
			 "project", template_widget->project,
			 "template", template_widget, 
			 "reason", GLADE_CREATE_COPY, NULL);
		g_free (name);
	}
 	
	if ((children =
	     glade_widget_adaptor_get_children (template_widget->adaptor,
						template_widget->object)) != NULL)
	{
		for (list = children; list && list->data; list = list->next)
		{
			GObject     *child = G_OBJECT (list->data);
			GladeWidget *child_gwidget, *child_dup;

			child_type = g_object_get_data (child, "special-child-type");

			if ((child_gwidget = glade_widget_get_from_gobject (child)) == NULL)
			{
				/* Bring the placeholders along ...
				 * but not unmarked internal children */
				if (GLADE_IS_PLACEHOLDER (child))
				{
					placeholder = glade_placeholder_new ();

					g_object_set_data_full (G_OBJECT (placeholder),
								"special-child-type",
								g_strdup (child_type),
								g_free);
					
					glade_widget_adaptor_add (gwidget->adaptor,
								  gwidget->object,
								  G_OBJECT (placeholder));
				}
			}
			else
			{
				/* Recurse through every GladeWidget (internal or not) */
				child_dup = glade_widget_dup_internal (gwidget, child_gwidget);

				if (child_gwidget->internal == NULL)
				{
					g_object_set_data_full (child_dup->object,
								"special-child-type",
								g_strdup (child_type),
								g_free);

					glade_widget_add_child (gwidget, child_dup, FALSE);
				}

				/* Internal children that are not heirarchic children
				 * need to avoid copying these packing props (like popup windows
				 * created on behalf of composite widgets).
				 */
				if (glade_widget_adaptor_has_child (gwidget->adaptor,
								    gwidget->object,
								    child_dup->object))
					glade_widget_copy_packing_props (gwidget,
									 child_dup,
									 child_gwidget);
			}
		}
		g_list_free (children);
	}

	if (gwidget->internal)
		glade_widget_copy_properties (gwidget, template_widget);
	
	if (gwidget->packing_properties == NULL)
		gwidget->packing_properties = glade_widget_dup_properties (template_widget->packing_properties , FALSE);
	
	/* If custom properties are still at thier
	 * default value, they need to be synced.
	 */
	glade_widget_sync_custom_props (gwidget);

	/* Special case GtkWindow here and ensure the pasted window
	 * has the same size as the 'Cut' one.
	 */
	if (GTK_IS_WINDOW (gwidget->object))
	{
		gint width, height;
		g_assert (GTK_IS_WINDOW (template_widget->object));

		gtk_window_get_size (GTK_WINDOW (template_widget->object), 
				     &width, &height);
		gtk_window_resize (GTK_WINDOW (gwidget->object),
				   width, height);
	}

	return gwidget;
}


typedef struct {
	GladeWidget *widget;
	GtkWidget   *placeholder;
	GList       *properties;
	
	gchar       *internal_name;
	GList       *internal_list;
} GladeChildExtract;

static GList *
glade_widget_extract_children (GladeWidget *gwidget)
{
	GladeChildExtract *extract;
	GList             *extract_list = NULL;
	GList             *children, *list;
	
	children = glade_widget_adaptor_get_children
		(gwidget->adaptor, gwidget->object);

	for (list = children; list && list->data; list = list->next)
	{
		GObject     *child   = G_OBJECT(list->data);
		GladeWidget *gchild  = glade_widget_get_from_gobject (child);
#if 0
		g_print ("Extracting %s from %s\n",
			 gchild ? gchild->name : 
			 GLADE_IS_PLACEHOLDER (child) ? "placeholder" : "unknown widget", 
			 gwidget->name);
#endif
		if (gchild && gchild->internal)
		{
			/* Recurse and collect any deep child hierarchies
			 * inside composite widgets.
			 */
			extract = g_new0 (GladeChildExtract, 1);
			extract->internal_name = g_strdup (gchild->internal);
			extract->internal_list = glade_widget_extract_children (gchild);
			extract->properties    = 
				glade_widget_dup_properties (gchild->properties, TRUE);
			
			extract_list = g_list_prepend (extract_list, extract);
		
		}
		else if (gchild || GLADE_IS_PLACEHOLDER (child))
		{
			extract = g_new0 (GladeChildExtract, 1);

			if (gchild)
			{
				extract->widget = g_object_ref (gchild);
				
				/* Make copies of the packing properties
				 */
				extract->properties = 
					glade_widget_dup_properties 
					(gchild->packing_properties, TRUE);

				glade_widget_remove_child (gwidget, gchild);
			}
			else
			{
				/* need to handle placeholders by hand here */
				extract->placeholder = g_object_ref (child);
				glade_widget_adaptor_remove (gwidget->adaptor,
							     gwidget->object, child);
			}
			extract_list =
				g_list_prepend (extract_list, extract);
		}
	}

	if (children)
		g_list_free (children);

	return g_list_reverse (extract_list);
}

static void
glade_widget_insert_children (GladeWidget *gwidget, GList *children)
{
	GladeChildExtract *extract;
	GladeWidget       *gchild;
	GObject           *internal_object;
	GList             *list, *l;
	
	for (list = children; list; list = list->next)
	{
		extract = list->data;
		
		if (extract->internal_name)
		{
			GladeGetInternalFunc   get_internal;
			GladeWidget           *internal_parent;


			/* Recurse and add deep widget hierarchies to internal
			 * widgets.
			 */
			get_internal = glade_widget_get_internal_func
				(gwidget, &internal_parent);

			internal_object = get_internal (internal_parent->adaptor,
							internal_parent->object,
							extract->internal_name);

			gchild = glade_widget_get_from_gobject (internal_object);
			
			/* This will free the list... */
			glade_widget_insert_children (gchild, extract->internal_list);

			/* Set the properties after inserting the children */
			for (l = extract->properties; l; l = l->next)
			{
				GValue         value = { 0, };
				GladeProperty *saved_prop = l->data;
				GladeProperty *widget_prop = 
					glade_widget_get_property (gchild,
								   saved_prop->klass->id);
				
				glade_property_get_value (saved_prop, &value);
				glade_property_set_value (widget_prop, &value);
				g_value_unset (&value);

				/* Free them as we go ... */
				g_object_unref (saved_prop);
			}

			if (extract->properties)
				g_list_free (extract->properties);

			g_free (extract->internal_name);
		}
		else if (extract->widget)
		{
			glade_widget_add_child (gwidget, extract->widget, FALSE);
			g_object_unref (extract->widget);
			
			for (l = extract->properties; l; l = l->next)
			{
				GValue         value = { 0, };
				GladeProperty *saved_prop = l->data;
				GladeProperty *widget_prop = 
					glade_widget_get_pack_property (extract->widget,
									saved_prop->klass->id);
				
				glade_property_get_value (saved_prop, &value);
				glade_property_set_value (widget_prop, &value);
				g_value_unset (&value);

				/* Free them as we go ... */
				g_object_unref (saved_prop);
			}
			if (extract->properties)
				g_list_free (extract->properties);
		}
		else
		{
			glade_widget_adaptor_add (gwidget->adaptor,
						  gwidget->object,
						  G_OBJECT (extract->placeholder));
			g_object_unref (extract->placeholder);
		}
		g_free (extract);
	}
	
	if (children)
		g_list_free (children);
}

static void
glade_widget_set_properties (GladeWidget *widget, GList *properties)
{
	GladeProperty *property;
	GList         *list;

	if (properties)
	{
		if (widget->properties)
		{
			g_list_foreach (widget->properties, (GFunc)g_object_unref, NULL);
			g_list_free (widget->properties);
		}
		widget->properties = properties;
		
		for (list = properties; list; list = list->next)
		{
			property = list->data;

			property->widget = widget;
		}
	}
}

static void
glade_widget_set_adaptor (GladeWidget *widget, GladeWidgetAdaptor *adaptor)
{
	GladePropertyClass *property_class;
	GladeProperty *property;
	GList *list;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	/* calling set_class out of the constructor? */
	g_return_if_fail (widget->adaptor == NULL);
	
	widget->adaptor = adaptor;

	/* If we have no properties; we are not in the process of loading
	 */
	if (!widget->properties)
	{
		for (list = adaptor->properties; list; list = list->next)
		{
			property_class = GLADE_PROPERTY_CLASS(list->data);
			if ((property = glade_property_new (property_class, 
							    widget, NULL, TRUE)) == NULL)
			{
				g_warning ("Failed to create [%s] property",
					   property_class->id);
				continue;
			}

			widget->properties = g_list_prepend (widget->properties, property);
		}
		widget->properties = g_list_reverse (widget->properties);
	}
}

/* Connects a signal handler to the 'event' signal for a widget and
   all its children recursively. We need this to draw the selection
   rectangles and to get button press/release events reliably. */
static void
glade_widget_connect_signal_handlers (GtkWidget   *widget_gtk, 
				      GCallback    callback, 
				      GladeWidget *gwidget)
{
	GList *children, *list;

	/* Check if we've already connected an event handler. */
	if (!g_object_get_data (G_OBJECT (widget_gtk),
				GLADE_TAG_EVENT_HANDLER_CONNECTED)) 
	{
		g_signal_connect (G_OBJECT (widget_gtk), "event",
				  callback, gwidget);

		g_object_set_data (G_OBJECT (widget_gtk),
				   GLADE_TAG_EVENT_HANDLER_CONNECTED,
				   GINT_TO_POINTER (1));
	}

	/* We also need to get expose events for any children.
	 */
	if (GTK_IS_CONTAINER (widget_gtk)) 
	{
		if ((children = 
		     glade_util_container_get_all_children (GTK_CONTAINER
							    (widget_gtk))) != NULL)
		{
			for (list = children; list; list = list->next)
			{
				GLADE_WIDGET_GET_CLASS (gwidget)->setup_events
					(gwidget, GTK_WIDGET (list->data));

				glade_widget_connect_signal_handlers 
					(GTK_WIDGET (list->data), callback, gwidget);
			}
			g_list_free (children);
		}
	}
}

/*
 * Returns a list of GladeProperties from a list for the correct
 * child type for this widget of this container.
 */
static GList *
glade_widget_create_packing_properties (GladeWidget *container, GladeWidget *widget)
{
	GladePropertyClass   *property_class;
	GladeProperty        *property;
	GList                *list, *packing_props = NULL;

	/* XXX TODO: by checking with some GladePropertyClass metadata, decide
	 * which packing properties go on which type of children.
	 */
	for (list = container->adaptor->packing_props; 
	     list && list->data; list = list->next)
	{
		property_class = list->data;
		property       = glade_property_new
			(property_class, widget, NULL, TRUE);
		packing_props  = g_list_prepend (packing_props, property);

	}
	return g_list_reverse (packing_props);
}

/*******************************************************************************
                           GladeInterface Parsing code
 *******************************************************************************/
static gint
glade_widget_set_child_type_from_child_info (GladeChildInfo     *child_info,
					     GladeWidgetAdaptor *parent_adaptor,
					     GObject *child)
{
	guint           i;
	GladePropInfo  *prop_info;
	gchar          *special_child_type;

	g_object_get (parent_adaptor, "special-child-type", &special_child_type, NULL);

	if (!special_child_type)
		return -1;

	for (i = 0; i < child_info->n_properties; ++i)
	{
		prop_info = child_info->properties + i;
		if (!strcmp (prop_info->name, special_child_type))
		{
			g_free (special_child_type);
			g_object_set_data_full (child,
						"special-child-type",
						g_strdup (prop_info->value),
						g_free);
			return i;
		}
	}
	g_free (special_child_type);
	return -1;
}

static gboolean
glade_widget_new_child_from_child_info (GladeChildInfo *info,
					GladeProject   *project,
					GladeWidget    *parent)
{
	GladeWidget    *child;
	GList          *list;

	g_return_val_if_fail (info != NULL, FALSE);
	g_return_val_if_fail (project != NULL, FALSE);

	/* is it a placeholder? */
	if (!info->child)
	{
		GObject *palaceholder = G_OBJECT (glade_placeholder_new ());
		glade_widget_set_child_type_from_child_info 
			(info, parent->adaptor, palaceholder);
		glade_widget_adaptor_add (parent->adaptor,
					  parent->object,
					  palaceholder);
		return TRUE;
	}

	/* is it an internal child? */
	if (info->internal_child)
	{
		GObject *child_object =
			glade_widget_get_internal_child (parent, info->internal_child);

		if (!child_object)
                {
			g_warning ("Failed to locate internal child %s of %s",
				   info->internal_child, glade_widget_get_name (parent));
			return FALSE;
		}

		if ((child = glade_widget_get_from_gobject (child_object)) == NULL)
			g_error ("Unable to get GladeWidget for internal child %s\n",
				 info->internal_child);

		/* Apply internal widget name from here */
		glade_widget_set_name (child, info->child->name);
		glade_widget_fill_from_widget_info (info->child, child, TRUE);
		glade_widget_sync_custom_props (child);
	}
        else
        {
		child = glade_widget_new_from_widget_info (info->child, project, parent);
		if (!child)
			return FALSE;

		child->parent = parent;

		glade_widget_set_child_type_from_child_info 
			(info, parent->adaptor, child->object);
		
		glade_widget_add_child (parent, child, FALSE);

		glade_widget_sync_packing_props (child);
	}

	/* Get the packing properties */
	for (list = child->packing_properties; list; list = list->next)
	{
		GladeProperty *property = list->data;
		glade_property_read (property, property->klass, 
				     loading_project, info, TRUE);
	}
	return TRUE;
}

static void
glade_widget_fill_from_widget_info (GladeWidgetInfo *info,
				    GladeWidget     *widget,
				    gboolean         apply_props)
{
	GladeProperty *property;
	GList         *list;
	guint          i;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (info != NULL);
	
	g_assert (strcmp (info->classname, widget->adaptor->name) == 0);

	/* Children */
	for (i = 0; i < info->n_children; ++i)
	{
		if (!glade_widget_new_child_from_child_info (info->children + i,
							     widget->project, widget))
		{
			g_warning ("Failed to read child of %s",
				   glade_widget_get_name (widget));
			continue;
		}
	}

	/* Signals */
	for (i = 0; i < info->n_signals; ++i)
	{
		GladeSignal *signal;

		signal = glade_signal_new_from_signal_info (info->signals + i);
		if (!signal)
		{
			g_warning ("Failed to read signal");
			continue;
		}
		glade_widget_add_signal_handler (widget, signal);
	}

	/* Properties */
	if (apply_props)
	{
 		for (list = widget->properties; list; list = list->next)
  		{
			property = list->data;
			glade_property_read (property, property->klass, 
					     loading_project, info, TRUE);
		}
	}
}



static GList *
glade_widget_properties_from_widget_info (GladeWidgetAdaptor *klass,
					  GladeWidgetInfo  *info)
{
	GList  *properties = NULL, *list;
	
	for (list = klass->properties; list && list->data; list = list->next)
	{
		GladePropertyClass *pclass = list->data;
		GladeProperty      *property;

		/* If there is a value in the XML, initialize property with it,
		 * otherwise initialize property to default.
		 */
		property = glade_property_new (pclass, NULL, NULL, FALSE);

		glade_property_read (property, property->klass, 
				     loading_project, info, TRUE);

		properties = g_list_prepend (properties, property);
	}

	return g_list_reverse (properties);
}

static GladeWidget *
glade_widget_new_from_widget_info (GladeWidgetInfo *info,
                                   GladeProject    *project,
                                   GladeWidget     *parent)
{
	GladeWidgetAdaptor *adaptor;
	GladeWidget        *widget;
	GList              *properties;
	
	g_return_val_if_fail (info != NULL, NULL);
	g_return_val_if_fail (project != NULL, NULL);

	if ((adaptor = 
	     glade_widget_adaptor_get_by_name (info->classname)) == NULL)
	{
		g_warning ("Widget class %s unknown.", info->classname);
		return NULL;
	}

	properties = glade_widget_properties_from_widget_info (adaptor, info);
	widget = glade_widget_adaptor_create_widget
		(adaptor, FALSE,
		 "name", info->name, 
		 "parent", parent, 
		 "project", project, 
		 "info", info, 
		 "properties", properties, 
		 "reason", GLADE_CREATE_LOAD, NULL);

	/* create the packing_properties list, without setting them */
	if (parent)
		widget->packing_properties =
			glade_widget_create_packing_properties (parent, widget);

	/* Load children first */
	glade_widget_fill_from_widget_info (info, widget, FALSE);

	/* Now sync custom props, things like "size" on GtkBox need
	 * this to be done afterwards.
	 */
	glade_widget_sync_custom_props (widget);

	return widget;
}

static GParameter *
glade_widget_info_params (GladeWidgetAdaptor *adaptor,
			  GladeWidgetInfo    *info,
			  gboolean            construct,
			  guint              *n_params)
{
	GladePropertyClass   *glade_property_class;
	GObjectClass         *oclass;
	GParamSpec          **pspec;
	GArray               *params;
	guint                 i, n_props;
	
	oclass = g_type_class_ref (adaptor->type);
	pspec  = g_object_class_list_properties (oclass, &n_props);
	params = g_array_new (FALSE, FALSE, sizeof (GParameter));

	/* prepare parameters that have glade_property_class->def */
	for (i = 0; i < n_props; i++)
	{
		GParameter  parameter = { 0, };
		GValue     *value;
		
		glade_property_class =
			glade_widget_adaptor_get_property_class (adaptor,
								 pspec[i]->name);
		if (glade_property_class == NULL  ||
		    glade_property_class->virt    ||
		    glade_property_class->ignore)
			continue;

		if (construct &&
		    (pspec[i]->flags & 
		     (G_PARAM_CONSTRUCT|G_PARAM_CONSTRUCT_ONLY)) == 0)
			continue;
		else if (!construct &&
			 (pspec[i]->flags & 
			  (G_PARAM_CONSTRUCT|G_PARAM_CONSTRUCT_ONLY)) != 0)
			continue;


		/* Try filling parameter with value from widget info.
		 */
		if ((value = glade_property_read (NULL, glade_property_class,
						  loading_project, info, FALSE)) != NULL)
		{
			parameter.name = pspec[i]->name;
			g_value_init (&parameter.value, pspec[i]->value_type);
			
			g_value_copy (value, &parameter.value);
			g_value_unset (value);
			g_free (value);

			g_array_append_val (params, parameter);
		}
	}
	g_free(pspec);

	g_type_class_unref (oclass);

	*n_params = params->len;
	return (GParameter *)g_array_free (params, FALSE);
}

/*******************************************************************************
                                     API
 *******************************************************************************/
GladeWidget *
glade_widget_get_from_gobject (gpointer object)
{
	g_return_val_if_fail (G_IS_OBJECT (object), NULL);
	
	return g_object_get_qdata (G_OBJECT (object), glade_widget_name_quark);
}

static void
glade_widget_debug_real (GladeWidget *widget, int indent)
{
	g_print ("%*sGladeWidget at %p\n", indent, "", widget);
	g_print ("%*sname = [%s]\n", indent, "", widget->name ? widget->name : "-");
	g_print ("%*sinternal = [%s]\n", indent, "", widget->internal ? widget->internal : "-");
	g_print ("%*sgobject = %p [%s]\n",
		 indent, "", widget->object, G_OBJECT_TYPE_NAME (widget->object));
	if (GTK_IS_WIDGET (widget->object))
		g_print ("%*sgtkwidget->parent = %p\n", indent, "",
			 gtk_widget_get_parent (GTK_WIDGET(widget->object)));
	if (GTK_IS_CONTAINER (widget->object)) {
		GList *children, *l;
		children = glade_util_container_get_all_children
			(GTK_CONTAINER (widget->object));
		for (l = children; l; l = l->next) {
			GtkWidget *widget_gtk = GTK_WIDGET (l->data);
			GladeWidget *widget = glade_widget_get_from_gobject (widget_gtk);
			if (widget) {
				glade_widget_debug_real (widget, indent + 2);
			} else if (GLADE_IS_PLACEHOLDER (widget_gtk)) {
				g_print ("%*sGtkWidget child %p is a placeholder.\n",
					 indent + 2, "", widget_gtk);
			} else {
				g_print ("%*sGtkWidget child %p [%s] has no glade widget.\n",
					 indent + 2, "",
					 widget_gtk, G_OBJECT_TYPE_NAME (widget_gtk));
			}
		}
		if (!children)
			g_print ("%*shas no children\n", indent, "");
		g_list_free (children);
	} else {
		g_print ("%*snot a container\n", indent, "");
	}
	g_print ("\n");
}

/**
 * glade_widget_debug:
 * @widget: a #GladeWidget
 *
 * Prints some information about a #GladeWidget, currently
 * this is unmaintained.
 */
void
glade_widget_debug (GladeWidget *widget)
{
	glade_widget_debug_real (widget, 0);
}

/**
 * glade_widget_show:
 * @widget: A #GladeWidget
 *
 * Display @widget in it's project's GladeDesignView
 */
void
glade_widget_show (GladeWidget *widget)
{
	GladeDesignView *view;
	GtkWidget *layout;

	g_return_if_fail (GLADE_IS_WIDGET (widget));

	/* Position window at saved coordinates or in the center */
	if (GTK_IS_WINDOW (widget->object))
	{
		if (!glade_window_is_embedded (GTK_WINDOW (widget->object)))
			glade_widget_embed_using_signals (widget);
			
		view = glade_design_view_get_from_project (glade_widget_get_project (widget));
		layout = GTK_WIDGET (glade_design_view_get_layout (view));
		
		if (gtk_bin_get_child (GTK_BIN (layout)) != NULL)
			gtk_container_remove (GTK_CONTAINER (layout), gtk_bin_get_child (GTK_BIN (layout)));
		
		gtk_container_add (GTK_CONTAINER (layout), GTK_WIDGET (widget->object));

		gtk_widget_show_all (GTK_WIDGET (widget->object));

	} else if (GTK_IS_WIDGET (widget->object)) {
		gtk_widget_show_all (GTK_WIDGET (widget->object));
	}
	widget->visible = TRUE;
}

/**
 * glade_widget_hide:
 * @widget: A #GladeWidget
 *
 * Hide @widget
 */
void
glade_widget_hide (GladeWidget *widget)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	if (GTK_IS_WINDOW (widget->object))
	{
		/* Save coordinates */
		gtk_widget_hide (GTK_WIDGET (widget->object));
	}
	widget->visible = FALSE;
}

/**
 * glade_widget_add_prop_ref:
 * @widget: A #GladeWidget
 * @property: the #GladeProperty
 *
 * Adds @property to @widget 's list of referenced properties.
 *
 * Note: this is used to track object reference properties that
 *       go in and out of the project.
 */
void
glade_widget_add_prop_ref (GladeWidget *widget, GladeProperty *property)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_PROPERTY (property));

	if (property && !widget->prop_refs_readonly &&
	    !g_list_find (widget->prop_refs, property))
		widget->prop_refs = g_list_prepend (widget->prop_refs, property);
}

/**
 * glade_widget_remove_prop_ref:
 * @widget: A #GladeWidget
 * @property: the #GladeProperty
 *
 * Removes @property from @widget 's list of referenced properties.
 *
 * Note: this is used to track object reference properties that
 *       go in and out of the project.
 */
void
glade_widget_remove_prop_ref (GladeWidget *widget, GladeProperty *property)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_PROPERTY (property));

	if (!widget->prop_refs_readonly)
		widget->prop_refs = g_list_remove (widget->prop_refs, property);
}

/**
 * glade_widget_project_notify:
 * @widget: A #GladeWidget
 * @project: The #GladeProject (or %NULL)
 *
 * Notifies @widget that it is now in @project.
 *
 * Note that this doesnt really set the project; the project is saved
 * for internal reasons even when the widget is on the clipboard.
 * (also used for property references).
 */
void
glade_widget_project_notify (GladeWidget *widget, GladeProject *project)
{
	GList         *l;
	GladeProperty *property;

	g_return_if_fail (GLADE_IS_WIDGET (widget));

	/* Since glade_property_set() will try to modify list,
	 * we protect it with the 'prop_refs_readonly' flag.
	 */
	widget->prop_refs_readonly = TRUE;
	for (l = widget->prop_refs; l && l->data; l = l->next)
	{
		property = GLADE_PROPERTY (l->data);

		if (project != NULL && 
		    project == property->widget->project)
			glade_property_add_object (property, widget->object);
		else
			glade_property_remove_object (property, widget->object);
	}
	widget->prop_refs_readonly = FALSE;
}

/**
 * glade_widget_copy_properties:
 * @widget:   a 'dest' #GladeWidget
 * @template_widget: a 'src' #GladeWidget
 *
 * Sets properties in @widget based on the values of
 * matching properties in @template_widget
 */
void
glade_widget_copy_properties (GladeWidget *widget,
			      GladeWidget *template_widget)
{
	GList *l;
	for (l = widget->properties; l && l->data; l = l->next)
	{
		GladeProperty *widget_prop  = GLADE_PROPERTY(l->data);
		GladeProperty *template_prop;
	
		/* Check if they share the same class definition, different
		 * properties may have the same name (support for
		 * copying properties across "not-quite" compatible widget
		 * classes, like GtkImageMenuItem --> GtkCheckMenuItem).
		 */
		if ((template_prop =
		     glade_widget_get_property (template_widget, 
						widget_prop->klass->id)) != NULL &&
		    glade_property_class_match (template_prop->klass, widget_prop->klass))
			glade_property_set_value (widget_prop, template_prop->value);
	}
}

/**
 * glade_widget_add_child:
 * @parent: A #GladeWidget
 * @child: the #GladeWidget to add
 * @at_mouse: whether the added widget should be added
 *            at the current mouse position
 *
 * Adds @child to @parent in a generic way for this #GladeWidget parent.
 */
void
glade_widget_add_child (GladeWidget      *parent,
			GladeWidget      *child,
			gboolean          at_mouse)
{
	g_return_if_fail (GLADE_IS_WIDGET (parent));
	g_return_if_fail (GLADE_IS_WIDGET (child));

	GLADE_WIDGET_GET_CLASS (parent)->add_child (parent, child, at_mouse);
}

/**
 * glade_widget_remove_child:
 * @parent: A #GladeWidget
 * @child: the #GladeWidget to add
 *
 * Removes @child from @parent in a generic way for this #GladeWidget parent.
 */
void
glade_widget_remove_child (GladeWidget      *parent,
			   GladeWidget      *child)
{
	g_return_if_fail (GLADE_IS_WIDGET (parent));
	g_return_if_fail (GLADE_IS_WIDGET (child));

	GLADE_WIDGET_GET_CLASS (parent)->remove_child (parent, child);
}

/**
 * glade_widget_dup:
 * @template_widget: a #GladeWidget
 *
 * Creates a deep copy of #GladeWidget.
 *
 * Returns: The newly created #GladeWidget
 */
GladeWidget *
glade_widget_dup (GladeWidget *template_widget)
{
	GladeWidget *widget;

	g_return_val_if_fail (GLADE_IS_WIDGET (template_widget), NULL);
	
	glade_widget_push_superuser ();
	widget = glade_widget_dup_internal (NULL, template_widget);
	glade_widget_pop_superuser ();

	return widget;
}

/**
 * glade_widget_rebuild:
 * @glade_widget: a #GladeWidget
 *
 * Replaces the current widget instance with
 * a new one while preserving all properties children and
 * takes care of reparenting.
 *
 */
void
glade_widget_rebuild (GladeWidget *glade_widget)
{
	GObject            *new_object, *old_object;
	GladeWidgetAdaptor *adaptor;
	GList              *children;
	gboolean            reselect = FALSE, inproject;
	
	g_return_if_fail (GLADE_IS_WIDGET (glade_widget));


	adaptor = glade_widget->adaptor;

	/* Here we take care removing the widget from the project and
	 * the selection before rebuilding the instance.
	 */
	inproject = glade_widget->project ?
		(glade_project_has_object
		 (glade_widget->project, glade_widget->object) ? TRUE : FALSE) : FALSE;

	if (inproject)
	{
		if (glade_project_is_selected (glade_widget->project, 
					       glade_widget->object))
		{
			reselect = TRUE;
			glade_project_selection_remove
				(glade_widget->project, glade_widget->object, FALSE);
		}
		glade_project_remove_object (glade_widget->project, glade_widget->object);
	}

	/* Extract and keep the child hierarchies aside... */
	children = glade_widget_extract_children (glade_widget);

	/* Hold a reference to the old widget while we transport properties
	 * and children from it
	 */
	new_object = glade_widget_build_object(adaptor, glade_widget, NULL);
	old_object = g_object_ref(glade_widget_get_object(glade_widget));

	glade_widget_set_object(glade_widget, new_object);

	/* Only call this once the object has a proper GladeWidget */
	glade_widget_adaptor_post_create (adaptor, new_object, GLADE_CREATE_REBUILD);

	/* Replace old object with new object in parent
	 */
	if (glade_widget->parent)
		glade_widget_replace (glade_widget->parent,
				      old_object, new_object);

	/* Reparent any children of the old object to the new object
	 * (this function will consume and free the child list).
	 */
	glade_widget_push_superuser ();
	glade_widget_insert_children (glade_widget, children);
	glade_widget_pop_superuser ();
		
	/* Custom properties aren't transfered in build_object, since build_object
	 * is only concerned with object creation.
	 */
	glade_widget_sync_custom_props (glade_widget);

	/* Sync packing.
	 */
	glade_widget_sync_packing_props (glade_widget);
	
	if (g_type_is_a (adaptor->type, GTK_TYPE_WIDGET))
	{
		/* Must use gtk_widget_destroy here for cases like dialogs and toplevels
		 * (otherwise I'd prefer g_object_unref() )
		 */
		gtk_widget_destroy  (GTK_WIDGET(old_object));
	}
	else
		g_object_unref (old_object);

	/* If the widget was in a project (and maybe the selection), then
	 * restore that stuff.
	 */
	if (inproject)
	{
		glade_project_add_object (glade_widget->project, NULL,
					  glade_widget->object);
		if (reselect)
			glade_project_selection_add
				(glade_widget->project, glade_widget->object, TRUE);
	}

 	/* We shouldnt show if its not already visible */
	if (glade_widget->visible)
		glade_widget_show (glade_widget);
}

/**
 * glade_widget_add_signal_handler:
 * @widget: A #GladeWidget
 * @signal_handler: The #GladeSignal
 *
 * Adds a signal handler for @widget 
 */
void
glade_widget_add_signal_handler	(GladeWidget *widget, GladeSignal *signal_handler)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	g_signal_emit (widget, glade_widget_signals[ADD_SIGNAL_HANDLER], 0, signal_handler);
}


/**
 * glade_widget_remove_signal_handler:
 * @widget: A #GladeWidget
 * @signal_handler: The #GladeSignal
 *
 * Removes a signal handler from @widget 
 */
void
glade_widget_remove_signal_handler (GladeWidget *widget, GladeSignal *signal_handler)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	g_signal_emit (widget, glade_widget_signals[REMOVE_SIGNAL_HANDLER], 0, signal_handler);
}

/**
 * glade_widget_change_signal_handler:
 * @widget: A #GladeWidget
 * @old_signal_handler: the old #GladeSignal
 * @new_signal_handler: the new #GladeSignal
 *
 * Changes a #GladeSignal on @widget 
 */
void
glade_widget_change_signal_handler (GladeWidget *widget,
				    GladeSignal *old_signal_handler,
				    GladeSignal *new_signal_handler)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	g_signal_emit (widget, glade_widget_signals[CHANGE_SIGNAL_HANDLER], 0,
		       old_signal_handler, new_signal_handler);
}

/**
 * glade_widget_list_signal_handlers:
 * @widget: a #GladeWidget
 * @signal_name: the name of the signal
 *
 * Returns: A #GPtrArray of #GladeSignal for @signal_name
 */
GPtrArray *
glade_widget_list_signal_handlers (GladeWidget *widget,
				   const gchar *signal_name) /* array of GladeSignal* */
{
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	return g_hash_table_lookup (widget->signals, signal_name);
}

/**
 * glade_widget_set_name:
 * @widget: a #GladeWidget
 * @name: a string
 *
 * Sets @widget's name to @name.
 */
void
glade_widget_set_name (GladeWidget *widget, const gchar *name)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	if (widget->name != name) {
		if (widget->name)
			g_free (widget->name);
		widget->name = g_strdup (name);
		g_object_notify (G_OBJECT (widget), "name");
	}
}

/**
 * glade_widget_get_name:
 * @widget: a #GladeWidget
 *
 * Returns: a pointer to @widget's name
 */
const gchar *
glade_widget_get_name (GladeWidget *widget)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	return widget->name;
}

/**
 * glade_widget_set_internal:
 * @widget: A #GladeWidget
 * @internal: The internal name
 *
 * Sets the internal name of @widget to @internal
 */
void
glade_widget_set_internal (GladeWidget *widget, const gchar *internal)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	if (widget->internal != internal) {
		g_free (widget->internal);
		widget->internal = g_strdup (internal);
		g_object_notify (G_OBJECT (widget), "internal");
	}
}

/**
 * glade_widget_get_internal:
 * @widget: a #GladeWidget
 *
 * Returns: the internal name of @widget
 */
G_CONST_RETURN gchar *
glade_widget_get_internal (GladeWidget *widget)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	return widget->internal;
}

/**
 * glade_widget_get_adaptor:
 * @widget: a #GladeWidget
 *
 * Returns: the #GladeWidgetAdaptor of @widget
 */
GladeWidgetAdaptor *
glade_widget_get_adaptor (GladeWidget *widget)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	return widget->adaptor;
}

/**
 * glade_widget_set_project:
 * @widget: a #GladeWidget
 * @project: a #GladeProject
 *
 * Makes @widget belong to @project.
 */
void
glade_widget_set_project (GladeWidget *widget, GladeProject *project)
{
	if (widget->project != project) {
		widget->project = project;
		g_object_notify (G_OBJECT (widget), "project");
	}
}

/**
 * glade_widget_get_project:
 * @widget: a #GladeWidget
 * 
 * Returns: the #GladeProject that @widget belongs to
 */
GladeProject *
glade_widget_get_project (GladeWidget *widget)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	return widget->project;
}

/**
 * glade_widget_get_property:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 *
 * Returns: the #GladeProperty in @widget named @id_property
 */
GladeProperty *
glade_widget_get_property (GladeWidget *widget, const gchar *id_property)
{
	static gchar   id_buffer[GPC_PROPERTY_NAMELEN] = { 0, };
	GList         *list;
	GladeProperty *property;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	g_return_val_if_fail (id_property != NULL, NULL);

	/* "-1" to always leave a trailing '\0' charachter */
	strncpy (id_buffer, id_property, GPC_PROPERTY_NAMELEN - 1);
	glade_util_replace (id_buffer, '_', '-');
		
	for (list = widget->properties; list; list = list->next) {
		property = list->data;
		if (strcmp (property->klass->id, id_buffer) == 0)
			return property;
	}
	return glade_widget_get_pack_property (widget, id_property);
}

/**
 * glade_widget_get_pack_property:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 *
 * Returns: the #GladeProperty in @widget named @id_property
 */
GladeProperty *
glade_widget_get_pack_property (GladeWidget *widget, const gchar *id_property)
{
	static gchar   id_buffer[GPC_PROPERTY_NAMELEN] = { 0, };
	GList         *list;
	GladeProperty *property;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	g_return_val_if_fail (id_property != NULL, NULL);

	/* "-1" to always leave a trailing '\0' charachter */
	strncpy (id_buffer, id_property, GPC_PROPERTY_NAMELEN - 1);
	glade_util_replace (id_buffer, '_', '-');

	for (list = widget->packing_properties; list; list = list->next) {
		property = list->data;
		if (strcmp (property->klass->id, id_buffer) == 0)
			return property;
	}
	return NULL;
}


/**
 * glade_widget_property_get:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @...: The return location for the value of the said #GladeProperty
 *
 * Gets the value of @id_property in @widget
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_property_get (GladeWidget      *widget,
			   const gchar      *id_property,
			   ...)
{
	GladeProperty *property;
	va_list        vl;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

	if ((property = glade_widget_get_property (widget, id_property)) != NULL)
	{
		va_start (vl, id_property);
		glade_property_get_va_list (property, vl);
		va_end (vl);
		return TRUE;
	}
	return FALSE;
}

/**
 * glade_widget_property_set:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @...: A value of the correct type for the said #GladeProperty
 *
 * Sets the value of @id_property in @widget
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_property_set (GladeWidget      *widget,
			   const gchar      *id_property,
			   ...)
{
	GladeProperty *property;
	va_list        vl;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

	if ((property = glade_widget_get_property (widget, id_property)) != NULL)
	{
		va_start (vl, id_property);
		glade_property_set_va_list (property, vl);
		va_end (vl);
		return TRUE;
	}
	return FALSE;
}

/**
 * glade_widget_pack_property_get:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @...: The return location for the value of the said #GladeProperty
 *
 * Gets the value of @id_property in @widget packing properties
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_pack_property_get (GladeWidget      *widget,
				const gchar      *id_property,
				...)
{
	GladeProperty *property;
	va_list        vl;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

	if ((property = glade_widget_get_pack_property (widget, id_property)) != NULL)
	{
		va_start (vl, id_property);
		glade_property_get_va_list (property, vl);
		va_end (vl);
		return TRUE;
	}
	return FALSE;
}

/**
 * glade_widget_pack_property_set:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @...: The return location for the value of the said #GladeProperty
 *
 * Sets the value of @id_property in @widget packing properties
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_pack_property_set (GladeWidget      *widget,
				const gchar      *id_property,
				...)
{
	GladeProperty *property;
	va_list        vl;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

	if ((property = glade_widget_get_pack_property (widget, id_property)) != NULL)
	{
		va_start (vl, id_property);
		glade_property_set_va_list (property, vl);
		va_end (vl);
		return TRUE;
	}
	return FALSE;
}

/**
 * glade_widget_property_set_sensitive:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @sensitive: setting sensitive or insensitive
 * @reason: a description of why the user cant edit this property
 *          which will be used as a tooltip
 *
 * Sets the sensitivity of @id_property in @widget
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_property_set_sensitive (GladeWidget      *widget,
				     const gchar      *id_property,
				     gboolean          sensitive,
				     const gchar      *reason)
{
	GladeProperty *property;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

	if ((property = glade_widget_get_property (widget, id_property)) != NULL)
	{
		glade_property_set_sensitive (property, sensitive, reason);
		return TRUE;
	}
	return FALSE;
}

/**
 * glade_widget_pack_property_set_sensitive:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @sensitive: setting sensitive or insensitive
 * @reason: a description of why the user cant edit this property
 *          which will be used as a tooltip
 *
 * Sets the sensitivity of @id_property in @widget's packing properties.
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_pack_property_set_sensitive (GladeWidget      *widget,
					  const gchar      *id_property,
					  gboolean          sensitive,
					  const gchar      *reason)
{
	GladeProperty *property;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

	if ((property = glade_widget_get_pack_property (widget, id_property)) != NULL)
	{
		glade_property_set_sensitive (property, sensitive, reason);
		return TRUE;
	}
	return FALSE;
}


/**
 * glade_widget_property_set_enabled:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @enabled: setting enabled or disabled
 *
 * Sets the enabled state of @id_property in @widget; this is
 * used for optional properties.
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_property_set_enabled (GladeWidget      *widget,
				   const gchar      *id_property,
				   gboolean          enabled)
{
	GladeProperty *property;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

	if ((property = glade_widget_get_property (widget, id_property)) != NULL)
	{
		glade_property_set_enabled (property, enabled);
		return TRUE;
	}
	return FALSE;
}

/**
 * glade_widget_pack_property_set_enabled:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @enabled: setting enabled or disabled
 *
 * Sets the enabled state of @id_property in @widget's packing 
 * properties; this is used for optional properties.
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_pack_property_set_enabled (GladeWidget      *widget,
					const gchar      *id_property,
					gboolean          enabled)
{
	GladeProperty *property;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

	if ((property = glade_widget_get_pack_property (widget, id_property)) != NULL)
	{
		glade_property_set_enabled (property, enabled);
		return TRUE;
	}
	return FALSE;
}

/**
 * glade_widget_property_reset:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 *
 * Resets @id_property in @widget to it's default value
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_property_reset (GladeWidget   *widget,
			     const gchar   *id_property)
{
	GladeProperty *property;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

	if ((property = glade_widget_get_property (widget, id_property)) != NULL)
	{
		glade_property_reset (property);
		return TRUE;
	}
	return FALSE;
}

/**
 * glade_widget_pack_property_reset:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 *
 * Resets @id_property in @widget's packing properties to it's default value
 *
 * Returns: whether @id_property was found or not.
 */
gboolean      
glade_widget_pack_property_reset (GladeWidget   *widget,
				  const gchar   *id_property)
{
	GladeProperty *property;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

	if ((property = glade_widget_get_pack_property (widget, id_property)) != NULL)
	{
		glade_property_reset (property);
		return TRUE;
	}
	return FALSE;
}

/**
 * glade_widget_property_default:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 *
 * Returns: whether whether @id_property was found and is 
 * currently set to it's default value.
 */
gboolean
glade_widget_property_default (GladeWidget *widget,
			       const gchar *id_property)
{
	GladeProperty *property;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

	if ((property = glade_widget_get_property (widget, id_property)) != NULL)
		return glade_property_default (property);

	return FALSE;
}

/**
 * glade_widget_pack_property_default:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 *
 * Returns: whether whether @id_property was found and is 
 * currently set to it's default value.
 */
gboolean 
glade_widget_pack_property_default (GladeWidget *widget,
				    const gchar *id_property)
{
	GladeProperty *property;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

	if ((property = glade_widget_get_pack_property (widget, id_property)) != NULL)
		return glade_property_default (property);

	return FALSE;
}


/**
 * glade_widget_object_set_property:
 * @widget:        A #GladeWidget
 * @property_name: The property identifier
 * @value:         The #GValue
 *
 * This function applies @value to the property @property_name on
 * the runtime object of @widget.
 */
void
glade_widget_object_set_property (GladeWidget      *widget,
				  const gchar      *property_name,
				  const GValue     *value)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (property_name != NULL && value != NULL);

	glade_widget_adaptor_set_property (widget->adaptor,
					   widget->object,
					   property_name, value);
}


/**
 * glade_widget_object_get_property:
 * @widget:        A #GladeWidget
 * @property_name: The property identifier
 * @value:         The #GValue
 *
 * This function retrieves the value of the property @property_name on
 * the runtime object of @widget and sets it in @value.
 */
void
glade_widget_object_get_property (GladeWidget      *widget,
				  const gchar      *property_name,
				  GValue           *value)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (property_name != NULL && value != NULL);

	glade_widget_adaptor_get_property (widget->adaptor,
					   widget->object,
					   property_name, value);
}

/**
 * glade_widget_child_set_property:
 * @widget:        A #GladeWidget
 * @child:         The #GladeWidget child
 * @property_name: The id of the property
 * @value:         The @GValue
 *
 * Sets @child's packing property identified by @property_name to @value.
 */
void
glade_widget_child_set_property (GladeWidget      *widget,
				 GladeWidget      *child,
				 const gchar      *property_name,
				 const GValue     *value)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_WIDGET (child));
	g_return_if_fail (property_name != NULL && value != NULL);

	glade_widget_adaptor_child_set_property (widget->adaptor,
						 widget->object,
						 child->object,
						 property_name, value);
}

/**
 * glade_widget_child_get_property:
 * @widget:        A #GladeWidget
 * @child:         The #GladeWidget child
 * @property_name: The id of the property
 * @value:         The @GValue
 *
 * Gets @child's packing property identified by @property_name.
 */
void
glade_widget_child_get_property (GladeWidget      *widget,
				 GladeWidget      *child,
				 const gchar      *property_name,
				 GValue           *value)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_WIDGET (child));
	g_return_if_fail (property_name != NULL && value != NULL);

	glade_widget_adaptor_child_get_property (widget->adaptor,
						 widget->object,
						 child->object,
						 property_name, value);

}



/**
 * glade_widget_set_object:
 * @gwidget: A #GladeWidget
 * @new_object: the new #GObject for @gwidget
 *
 * Set the runtime object for this GladeWidget wrapper
 * (this is used deep in the core and is probably unsafe
 * to use elsewhere).
 */
void
glade_widget_set_object (GladeWidget *gwidget, GObject *new_object)
{
	GladeWidgetAdaptor *adaptor;
	GObject            *old_object;
	
	g_return_if_fail (GLADE_IS_WIDGET (gwidget));
	g_return_if_fail (G_IS_OBJECT     (new_object));
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (new_object),
				       gwidget->adaptor->type));

	adaptor    = gwidget->adaptor;
	old_object = gwidget->object;
	
	/* Add internal reference to new widget */
	gwidget->object = g_object_ref (G_OBJECT(new_object));
	g_object_set_qdata (G_OBJECT (new_object), glade_widget_name_quark, gwidget);

	if (g_type_is_a (gwidget->adaptor->type, GTK_TYPE_WIDGET))
	{
		
		/* Make sure dialogs and such have close buttons.
		 */
		if (g_type_is_a (gwidget->adaptor->type, GTK_TYPE_WINDOW))
			gtk_window_set_decorated (GTK_WINDOW (new_object), TRUE);

		/* Disable any built-in DnD
		 */
		gtk_drag_dest_unset (GTK_WIDGET (new_object));
		gtk_drag_source_unset (GTK_WIDGET (new_object));

		/* Take care of events and toolkit signals.
		 */
		GLADE_WIDGET_GET_CLASS (gwidget)->setup_events
			(gwidget, GTK_WIDGET (new_object));

		if (GTK_WIDGET_TOPLEVEL (new_object))
			g_signal_connect (G_OBJECT (new_object), "delete_event",
					  G_CALLBACK (glade_widget_hide_on_delete), NULL);
		
		g_signal_connect (G_OBJECT (new_object), "popup_menu",
				  G_CALLBACK (glade_widget_popup_menu), NULL);
		
		glade_widget_connect_signal_handlers
			(GTK_WIDGET(new_object), 
			 G_CALLBACK (glade_widget_event_private),
			 gwidget);
	}

	/* Remove internal reference to old widget */
	if (old_object) {
		g_object_set_qdata (G_OBJECT (old_object), glade_widget_name_quark, NULL);
		g_object_unref (G_OBJECT (old_object));
	}
	g_object_notify (G_OBJECT (gwidget), "object");
}

/**
 * glade_widget_get_object:
 * @widget: a #GladeWidget
 *
 * Returns: the #GObject associated with @widget
 */
GObject *
glade_widget_get_object (GladeWidget *widget)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	return widget->object;
}

/**
 * glade_widget_get_parent:
 * @widget: A #GladeWidget
 *
 * Returns: The parenting #GladeWidget
 */
GladeWidget *
glade_widget_get_parent (GladeWidget *widget)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	return widget->parent;
}

/**
 * glade_widget_set_parent:
 * @widget: A #GladeWidget
 * @parent: the parenting #GladeWidget (or %NULL)
 *
 * sets the parenting #GladeWidget
 */
void
glade_widget_set_parent (GladeWidget *widget,
			 GladeWidget *parent)
{
	GladeWidget *old_parent;

	g_return_if_fail (GLADE_IS_WIDGET (widget));

	old_parent     = widget->parent;
	widget->parent = parent;

	/* Set packing props only if the object is actually parented by 'parent'
	 * (a subsequent call should come from glade_command after parenting).
	 */
	if (widget->object && parent != NULL &&
	    glade_widget_adaptor_has_child 
	    (parent->adaptor, parent->object, widget->object))
	{
		if (old_parent == NULL || widget->packing_properties == NULL ||
		    old_parent->adaptor->type != parent->adaptor->type)
			glade_widget_set_packing_properties (widget, parent);
		else
			glade_widget_sync_packing_props (widget);
	}
	    
	g_object_notify (G_OBJECT (widget), "parent");
}

/**
 * glade_widget_set_packing_properties:
 * @widget:     A #GladeWidget
 * @container:  The parent #GladeWidget
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
	GList                *list;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_WIDGET (container));

	g_list_foreach (widget->packing_properties, (GFunc)g_object_unref, NULL);
	g_list_free (widget->packing_properties);
	widget->packing_properties = NULL;

	/* We have to detect whether this is an anarchist child of a composite
	 * widget or not, in otherwords; whether its really a direct child or
	 * a child of a popup window created on the composite widget's behalf.
	 */
	if (widget->anarchist) return;

	widget->packing_properties = glade_widget_create_packing_properties (container, widget);

	/* Dont introspect on properties that are not parented yet.
	 */
	if (glade_widget_adaptor_has_child (container->adaptor,
					    container->object,
					    widget->object))
	{
		glade_widget_set_default_packing_properties (container, widget);

		/* update the values of the properties to the ones we get from gtk */
		for (list = widget->packing_properties;
		     list && list->data;
		     list = list->next)
		{
			GladeProperty *property = list->data;
			g_value_reset (property->value);
			glade_widget_child_get_property 
				(container,
				 widget, property->klass->id, property->value);
		}
	}
}

/**
 * glade_widget_has_decendant:
 * @widget: a #GladeWidget
 * @type: a  #GType
 * 
 * Returns: whether this GladeWidget has any decendants of type @type
 *          or any decendants that implement the @type interface
 */
gboolean
glade_widget_has_decendant (GladeWidget *widget, GType type)
{
	GladeWidget   *child;
	GList         *children, *l;
	gboolean       found = FALSE;

	if (G_TYPE_IS_INTERFACE (type) &&
	    glade_util_class_implements_interface
	    (widget->adaptor->type, type))
		return TRUE;
	else if (G_TYPE_IS_INTERFACE (type) == FALSE &&
		 g_type_is_a (widget->adaptor->type, type))
		return TRUE;

	if ((children = glade_widget_adaptor_get_children
	     (widget->adaptor, widget->object)) != NULL)
	{
		for (l = children; l; l = l->next)
			if ((child = glade_widget_get_from_gobject (l->data)) != NULL &&
			    (found = glade_widget_has_decendant (child, type)))
				break;
		g_list_free (children);
	}	
	return found;
}


/**
 * glade_widget_event_widget:
 *
 * During events, this function returns the deepest
 * project widget at moust position, or %NULL if it is
 * not a mouse event.
 *
 * Handle with care, you must be in an event for 
 * the return value to be meaningfull
 *
 * Returns a #GladeWidget
 */
GladeWidget *
glade_widget_event_widget (void)
{
	return deep_event_widget;
}


/**
 * glade_widget_replace:
 * @old_object: a #GObject
 * @new_object: a #GObject
 * 
 * Replaces a GObject with another GObject inside a GObject which
 * behaves as a container.
 *
 * Note that both GObjects must be owned by a GladeWidget.
 */
void
glade_widget_replace (GladeWidget *parent, GObject *old_object, GObject *new_object)
{
	g_return_if_fail (G_IS_OBJECT (old_object));
	g_return_if_fail (G_IS_OBJECT (new_object));

	GLADE_WIDGET_GET_CLASS (parent)->replace_child (parent, old_object, new_object);
}

/* XML Serialization */
static gboolean
glade_widget_write_child (GArray *children, GladeWidget *parent, GObject *object, GladeInterface *interface);

typedef struct _WriteSignalsContext
{
	GladeInterface *interface;
	GArray *signals;
} WriteSignalsContext;

static void
glade_widget_write_signals (gpointer key, gpointer value, gpointer user_data)
{
	WriteSignalsContext *write_signals_context;
        GPtrArray *signals;
	guint i;

	write_signals_context = (WriteSignalsContext *) user_data;
	signals = (GPtrArray *) value;
	for (i = 0; i < signals->len; i++)
	{
		GladeSignal *signal = g_ptr_array_index (signals, i);
		GladeSignalInfo signalinfo;

		glade_signal_write (&signalinfo, signal,
				    write_signals_context->interface);
		g_array_append_val (write_signals_context->signals, 
				    signalinfo);
	}
}

/**
 * glade_widget_write:
 * @widget: a #GladeWidget
 * @interface: a #GladeInterface
 *
 * TODO: write me
 *
 * Returns: 
 */
GladeWidgetInfo*
glade_widget_write (GladeWidget *widget, GladeInterface *interface)
{
	WriteSignalsContext write_signals_context;
	GladeWidgetInfo *info;
	GArray *props, *atk_props, *atk_actions, *atk_relations, *accels, *children;
	GList *list;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

	info = g_new0 (GladeWidgetInfo, 1);

	info->classname = glade_xml_alloc_string (interface, widget->adaptor->name);
	info->name      = glade_xml_alloc_string (interface, widget->name);

	/* Write the properties */
	props         = g_array_new (FALSE, FALSE, sizeof (GladePropInfo));
	atk_props     = g_array_new (FALSE, FALSE, sizeof (GladePropInfo));
	atk_relations = g_array_new (FALSE, FALSE, sizeof (GladeAtkRelationInfo));
	atk_actions   = g_array_new (FALSE, FALSE, sizeof (GladeAtkActionInfo));
	accels        = g_array_new (FALSE, FALSE, sizeof (GladeAccelInfo));

	for (list = widget->properties; list; list = list->next)
	{
		GladeProperty *property = list->data;

		/* This should never happen */
		if (property->klass->packing)
			continue;

		switch (property->klass->type)
		{
		case GPC_NORMAL:
			glade_property_write (property, interface, props);
			break;
		case GPC_ATK_PROPERTY:
			glade_property_write (property, interface, atk_props);
			break;
		case GPC_ATK_RELATION:
			glade_property_write (property, interface, atk_relations);
			break;
		case GPC_ATK_ACTION:
			glade_property_write (property, interface, atk_actions);
			break;
		case GPC_ACCEL_PROPERTY:
			glade_property_write (property, interface, accels);
			break;
		default:
			break;
		}
	}

	/* Properties */
	info->properties = (GladePropInfo *) props->data;
	info->n_properties = props->len;
	g_array_free(props, FALSE);

	/* Atk Properties */
	info->atk_props = (GladePropInfo *) atk_props->data;
	info->n_atk_props = atk_props->len;
	g_array_free(atk_props, FALSE);

	/* Atk Relations */
	info->relations = (GladeAtkRelationInfo *) atk_relations->data;
	info->n_relations = atk_relations->len;
	g_array_free(atk_relations, FALSE);

	/* Atk Actions */
	info->atk_actions = (GladeAtkActionInfo *) atk_actions->data;
	info->n_atk_actions = atk_actions->len;
	g_array_free(atk_actions, FALSE);

	/* Accels */
	info->accels = (GladeAccelInfo *) accels->data;
	info->n_accels = accels->len;
	g_array_free(accels, FALSE);

	/* Signals */
	write_signals_context.interface = interface;
	write_signals_context.signals = g_array_new (FALSE, FALSE,
	                                              sizeof (GladeSignalInfo));
	g_hash_table_foreach (widget->signals,
			      glade_widget_write_signals,
			      &write_signals_context);
	info->signals = (GladeSignalInfo *)
				write_signals_context.signals->data;
	info->n_signals = write_signals_context.signals->len;
	g_array_free (write_signals_context.signals, FALSE);

	/* Children */
	if ((list =
	     glade_widget_adaptor_get_children (widget->adaptor,
						widget->object)) != NULL)
	{
		children = g_array_new (FALSE, FALSE, sizeof (GladeChildInfo));
		while (list && list->data)
		{
			GObject *child = list->data;
			glade_widget_write_child (children, widget, child, interface);
			list = list->next;
		}
		info->children   = (GladeChildInfo *) children->data;
		info->n_children = children->len;
		
		g_array_free (children, FALSE);
		g_list_free (list);
	}
	g_hash_table_insert(interface->names, info->name, info);

	return info;
}


static gboolean
glade_widget_write_special_child_prop (GArray *props, 
				       GladeWidget *parent, 
				       GObject *object, 
				       GladeInterface *interface)
{
	GladePropInfo         info = { 0 };
	gchar                *buff, *special_child_type;

	buff = g_object_get_data (object, "special-child-type");

	g_object_get (parent->adaptor, "special-child-type", &special_child_type, NULL);

	if (special_child_type && buff)
	{
		info.name  = glade_xml_alloc_propname (interface, 
						       special_child_type);
		info.value = glade_xml_alloc_string (interface, buff);
		g_array_append_val (props, info);

		g_free (special_child_type);
		return TRUE;
	}
	g_free (special_child_type);
	return FALSE;
}

gboolean
glade_widget_write_child (GArray         *children, 
			  GladeWidget    *parent, 
			  GObject        *object,
			  GladeInterface *interface)
{
	GladeChildInfo info = { 0 };
	GladeWidget *child_widget;
	GList *list;
	GArray *props;

	if (GLADE_IS_PLACEHOLDER (object))
	{
		props = g_array_new (FALSE, FALSE,
				     sizeof (GladePropInfo));
		/* Here we have to add the "special-child-type" packing property */
		glade_widget_write_special_child_prop (props, parent, 
						       object, interface);

		info.properties = (GladePropInfo *) props->data;
		info.n_properties = props->len;
		g_array_free(props, FALSE);

		g_array_append_val (children, info);

		return TRUE;
	}

	child_widget = glade_widget_get_from_gobject (object);
	if (!child_widget)
		return FALSE;
	
	if (child_widget->internal)
		info.internal_child = glade_xml_alloc_string(interface, child_widget->internal);

	info.child = glade_widget_write (child_widget, interface);
	if (!info.child)
	{
		g_warning ("Failed to write child widget");
		return FALSE;
	}

	/* Append the packing properties */
	props = g_array_new (FALSE, FALSE, sizeof (GladePropInfo));
	
	/* Here we have to add the "special-child-type" packing property */
	glade_widget_write_special_child_prop (props, parent, 
					       child_widget->object,
					       interface);

	if (child_widget->packing_properties != NULL) 
	{
		for (list = child_widget->packing_properties;
		     list; list = list->next) 
		{
			GladeProperty *property;

			property = list->data;
			g_assert (property->klass->packing != FALSE);
			glade_property_write (property, interface, props);
		}
	}

	info.properties = (GladePropInfo *) props->data;
	info.n_properties = props->len;
	g_array_free(props, FALSE);
	
	g_array_append_val (children, info);

	return TRUE;
}

/**
 * glade_widget_read:
 * @project: a #GladeProject
 * @info: a #GladeWidgetInfo
 *
 * Returns: a new #GladeWidget for @project, based on @info
 */
GladeWidget *
glade_widget_read (GladeProject *project, GladeWidgetInfo *info)
{
	GladeWidget *widget;

	glade_widget_push_superuser ();
	loading_project = project;
		
	if ((widget = glade_widget_new_from_widget_info
	     (info, project, NULL)) != NULL)
	{
		if (glade_verbose)
			glade_widget_debug (widget);
	}	

	loading_project = NULL;
	glade_widget_pop_superuser ();

	return widget;
}

/**
 * glade_widget_get_launcher:
 * @widget: a #GladeWidget
 *
 * Returns: whether there is a #GladeEditorLaunchFunc provided by
 *          the backend for @widget (or any of its parents)
 */
gboolean
glade_widget_has_launcher (GladeWidget *widget)
{
	GladeWidget *parent;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
	parent = widget;
	do
	{
		GladeWidgetAdaptorClass *adaptor_class =
			GLADE_WIDGET_ADAPTOR_GET_CLASS (parent->adaptor);

		if (adaptor_class->launch_editor != NULL)
			return TRUE;
	} while ((parent = parent->parent) != NULL);
	return FALSE;
}

/**
 * glade_widget_launch_editor:
 * @widget: a #GladeWidget
 *
 * Launches a custom editor from the backend for thie widget.
 */
void
glade_widget_launch_editor (GladeWidget *widget)
{
	GladeWidget *parent;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	parent = widget;
	do
	{
		GladeWidgetAdaptorClass *adaptor_class =
			GLADE_WIDGET_ADAPTOR_GET_CLASS (parent->adaptor);

		if (adaptor_class->launch_editor != NULL)
		{
			glade_widget_adaptor_launch_editor (parent->adaptor,
							    parent->object);
			break;
		}
	} while ((parent = parent->parent) != NULL);
}


static gint glade_widget_su_stack = 0;

/**
 * glade_widget_superuser:
 *
 * Checks if we are in superuser mode.
 *
 * Superuser mode is when we are
 *   - Loading a project
 *   - Dupping a widget recursively
 *   - Rebuilding an instance for a construct-only property
 *
 * In these cases, we must act like a load, this should be checked
 * from the plugin when implementing containers, when undo/redo comes
 * around, the plugin is responsable for maintaining the same container
 * size when widgets are added/removed.
 */
gboolean
glade_widget_superuser (void)
{
	return glade_widget_su_stack > 0;
}

/**
 * glade_widget_push_superuser:
 *
 * Sets superuser mode
 */
void
glade_widget_push_superuser (void)
{
	glade_property_push_superuser ();
	glade_widget_su_stack++;
}


/**
 * glade_widget_pop_superuser:
 *
 * Unsets superuser mode
 */
void
glade_widget_pop_superuser (void)
{
	if (--glade_widget_su_stack < 0)
	{
		g_critical ("Bug: widget super user stack is corrupt.\n");
	}
	glade_property_pop_superuser ();
}


/*******************************************************************************
                           Toplevel GladeWidget Embedding 
 *******************************************************************************
 *
 * This code has two implemententations, one that stores original class handlers in
 * a hashtable, and another that uses signal connection to emulate vfuncs.
 * 
 * Signal Connection:  glade_widget_embed_using_signals ()
 *
 * Hashtable:  glade_widget_embed ()
 *
 */


typedef struct
{	
	/* pointers to the original class handlers */ 
	void     (* realize)       (GtkWidget *widget);
	void     (* map)           (GtkWidget *widget);
	void     (* unmap)         (GtkWidget *widget);
	void     (* show)          (GtkWidget *widget);
	void     (* hide)          (GtkWidget *widget);
	void     (* size_allocate) (GtkWidget *widget, GtkAllocation *allocation);

} GladeOriginalHandlers;

static GHashTable *original_handlers_table = NULL;

static GQuark glade_embedded_window_quark = 0;


static gboolean
glade_window_is_embedded (GtkWindow *window)
{
	return GPOINTER_TO_INT (g_object_get_qdata ((GObject *) window, glade_embedded_window_quark));	 
}

static GtkBinClass*
get_bin_class_ancestor (gpointer descendant_class)
{
	gpointer parent_class;
	gpointer klass;

	klass = descendant_class;

	if (!GTK_IS_BIN_CLASS (klass))
		return NULL;

	while (GTK_IS_BIN_CLASS (parent_class = g_type_class_peek_parent (klass)))
	{
		klass = parent_class;
		parent_class = g_type_class_peek_parent (klass);

	}

	return GTK_BIN_CLASS (klass);
}

static GladeOriginalHandlers*
obtain_handlers (GType instance_type)
{
	GladeOriginalHandlers *handlers = NULL;
	GType type = instance_type;

	while (!handlers && type != 0)
	{
		handlers = (GladeOriginalHandlers *) g_hash_table_lookup (original_handlers_table, &type);

		type = g_type_parent (type);
	}

	return handlers;
}

static void
embedded_window_map (GtkWidget *widget)
{
	GtkBinClass *bin_class;

	bin_class = get_bin_class_ancestor (GTK_WIDGET_GET_CLASS (widget));

	GTK_WIDGET_CLASS (bin_class)->map (widget);
}

static void
embedded_window_unmap (GtkWidget *widget)
{
	GtkBinClass *bin_class;

	bin_class = get_bin_class_ancestor (GTK_WIDGET_GET_CLASS (widget));

	GTK_WIDGET_CLASS (bin_class)->unmap (widget);
}

static void
embedded_window_show (GtkWidget *widget)
{
	GtkBinClass *bin_class;

	bin_class = get_bin_class_ancestor (GTK_WIDGET_GET_CLASS (widget));

	GTK_WIDGET_CLASS (bin_class)->show (widget);
}

static void
embedded_window_hide (GtkWidget *widget)
{
	GtkBinClass *bin_class;

	bin_class = get_bin_class_ancestor (GTK_WIDGET_GET_CLASS (widget));

	GTK_WIDGET_CLASS (bin_class)->hide (widget);
}

static void
embedded_window_realize (GtkWidget *widget)
{
	GtkWindow *window;
	GdkWindowAttr attributes;
	gint attributes_mask;

	window = GTK_WINDOW (widget);

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);

	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;

	attributes.event_mask = gtk_widget_get_events (widget) |
				GDK_EXPOSURE_MASK              |
                                GDK_FOCUS_CHANGE_MASK          |
			        GDK_KEY_PRESS_MASK             |
			        GDK_KEY_RELEASE_MASK           |
			        GDK_ENTER_NOTIFY_MASK          |
			        GDK_LEAVE_NOTIFY_MASK          |
				GDK_STRUCTURE_MASK;

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
					 &attributes, attributes_mask);

	gdk_window_enable_synchronized_configure (widget->window);

	gdk_window_set_user_data (widget->window, widget);

	widget->style = gtk_style_attach (widget->style, widget->window);
	gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
embedded_window_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GtkWindow *window;
	GtkAllocation child_allocation;

	window = GTK_WINDOW (widget);
	widget->allocation = *allocation;

	if (GTK_WIDGET_REALIZED (widget))
		gdk_window_move_resize (widget->window, allocation->x, 
							allocation->y,
		                                        allocation->width, 
		                                        allocation->height);

	if (window->bin.child && GTK_WIDGET_VISIBLE (window->bin.child))
	{
		child_allocation.x = GTK_CONTAINER (widget)->border_width;
		child_allocation.y = GTK_CONTAINER (widget)->border_width;
		child_allocation.width = MAX (1, allocation->width - GTK_CONTAINER (widget)->border_width * 2);
		child_allocation.height = MAX (1, allocation->height - GTK_CONTAINER (widget)->border_width * 2);

		gtk_widget_size_allocate (window->bin.child, &child_allocation);
	}
}

static void
embedded_window_realize_proxy (GtkWidget *widget)
{
	GladeOriginalHandlers *original_handlers;

	if (!glade_window_is_embedded ((GtkWindow *) widget))
	{
		original_handlers = obtain_handlers (G_OBJECT_TYPE (widget));

		g_assert (original_handlers != NULL);

		original_handlers->realize (widget);
	}
	else
	{
		embedded_window_realize (widget);
	}
}

static void
embedded_window_size_allocate_proxy (GtkWidget *widget, GtkAllocation *allocation)
{
	GladeOriginalHandlers *original_handlers;

	if (!glade_window_is_embedded ((GtkWindow *) widget))
	{
		original_handlers = obtain_handlers (G_OBJECT_TYPE (widget));

		g_assert (original_handlers != NULL);

		original_handlers->size_allocate (widget, allocation);
	}
	else
	{
		embedded_window_size_allocate (widget, allocation);
	}
}

static void
embedded_window_map_proxy (GtkWidget *widget)
{
	GladeOriginalHandlers *original_handlers;

	if (!glade_window_is_embedded ((GtkWindow *) widget))
	{
		original_handlers = obtain_handlers (G_OBJECT_TYPE (widget));

		g_assert (original_handlers != NULL);

		original_handlers->map (widget);
	}
	else
	{
		embedded_window_map (widget);
	}
}

static void
embedded_window_unmap_proxy (GtkWidget *widget)
{
	GladeOriginalHandlers *original_handlers;

	if (!glade_window_is_embedded ((GtkWindow *) widget))
	{
		original_handlers = obtain_handlers (G_OBJECT_TYPE (widget));

		g_assert (original_handlers != NULL);

		original_handlers->unmap (widget);
	}
	else
	{
		embedded_window_unmap (widget);
	}
}


static void
embedded_window_show_proxy (GtkWidget *widget)
{
	GladeOriginalHandlers *original_handlers;

	if (!glade_window_is_embedded ((GtkWindow *) widget))
	{
		original_handlers = obtain_handlers (G_OBJECT_TYPE (widget));

		g_assert (original_handlers != NULL);

		original_handlers->show (widget);
	}
	else
	{
		embedded_window_show (widget);
	}
}

static void
embedded_window_hide_proxy (GtkWidget *widget)
{
	GladeOriginalHandlers *original_handlers;

	if (!glade_window_is_embedded ((GtkWindow *) widget))
	{
		original_handlers = obtain_handlers (G_OBJECT_TYPE (widget));

		g_assert (original_handlers != NULL);

		original_handlers->hide (widget);
	}
	else
	{
		embedded_window_hide (widget);
	}
}

static guint
glade_gtype_hash (const GType *v)
{
	return *v % G_MAXUINT;
}

static gboolean
glade_gtype_equal (const GType *v1, const GType *v2)
{
	return *v1 == *v2;
}

static void
embedded_data_ensure (void)
{

	if (original_handlers_table == NULL)
	{
		original_handlers_table = g_hash_table_new_full ((GHashFunc)      glade_gtype_hash, 
								 (GEqualFunc)     glade_gtype_equal, 
							         (GDestroyNotify) g_free, 
							         (GDestroyNotify) g_free);
	}
	
	if (glade_embedded_window_quark == 0)
		glade_embedded_window_quark = g_quark_from_string ("GladeEmbedWindow");
}

/**
 * glade_widget_window:
 * @window: a #GtkWindow
 *
 * Embeds a window by overriding class-wide methods
 */
static void
glade_widget_embed (GladeWidget *widget)
{
	GladeOriginalHandlers *original_handlers;
	GtkWindow *window;
	GType *key;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GTK_IS_WINDOW (widget->object));
	
	window = GTK_WINDOW (widget->object);

	/* do some validity checks */
	if (glade_window_is_embedded (window) || GTK_WIDGET_REALIZED (GTK_WIDGET (window))) {
		g_critical ("Cannot embed a window that is already realized or embedded");
		return;
	}

	embedded_data_ensure ();

	GTK_WIDGET_UNSET_FLAGS (GTK_WIDGET (window), GTK_TOPLEVEL);
	gtk_container_set_resize_mode (GTK_CONTAINER (window), GTK_RESIZE_PARENT);

	/* create key (GType) for the hashtable */
	key = (GType *) g_malloc (sizeof (GType));
	*key = G_OBJECT_TYPE (window);

	/* see if the class handlers are already in the hashtable */
	original_handlers = (GladeOriginalHandlers *) g_hash_table_lookup (original_handlers_table, key);

	if (original_handlers == NULL)
	{
		/* now we override the class handlers, and store the overridden handlers in the hashtable */ 
		original_handlers = g_new0 (GladeOriginalHandlers, 1);

		original_handlers->realize       = GTK_WIDGET_GET_CLASS (window)->realize;
		original_handlers->map           = GTK_WIDGET_GET_CLASS (window)->map;
		original_handlers->unmap         = GTK_WIDGET_GET_CLASS (window)->unmap;
		original_handlers->show          = GTK_WIDGET_GET_CLASS (window)->show;
		original_handlers->hide          = GTK_WIDGET_GET_CLASS (window)->hide;
		original_handlers->size_allocate = GTK_WIDGET_GET_CLASS (window)->size_allocate;

		GTK_WIDGET_GET_CLASS (window)->realize       = embedded_window_realize_proxy;
		GTK_WIDGET_GET_CLASS (window)->map           = embedded_window_map_proxy;
		GTK_WIDGET_GET_CLASS (window)->unmap         = embedded_window_unmap_proxy;
		GTK_WIDGET_GET_CLASS (window)->show          = embedded_window_show_proxy;
		GTK_WIDGET_GET_CLASS (window)->hide          = embedded_window_hide_proxy;
		GTK_WIDGET_GET_CLASS (window)->size_allocate = embedded_window_size_allocate_proxy;

		g_hash_table_insert (original_handlers_table, key, original_handlers);
	} 

	/* mark window as embedded */
	g_object_set_qdata (G_OBJECT (window), 
			   glade_embedded_window_quark, GINT_TO_POINTER (TRUE));
}


/* signal connection method */

static void
embedded_window_realize_handler (GtkWidget *widget)
{
	GtkWindow *window;
	GdkWindowAttr attributes;
	gint attributes_mask;

	window = GTK_WINDOW (widget);

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);

	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;

	attributes.event_mask = gtk_widget_get_events (widget) |
				GDK_EXPOSURE_MASK              |
                                GDK_FOCUS_CHANGE_MASK          |
			        GDK_KEY_PRESS_MASK             |
			        GDK_KEY_RELEASE_MASK           |
			        GDK_ENTER_NOTIFY_MASK          |
			        GDK_LEAVE_NOTIFY_MASK          |
				GDK_STRUCTURE_MASK;

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	/* destroy the previously created window */
	if (GDK_IS_WINDOW (widget->window))
	{
		gdk_window_hide (widget->window);
	}

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
					 &attributes, attributes_mask);

	gdk_window_enable_synchronized_configure (widget->window);

	gdk_window_set_user_data (widget->window, window);

	widget->style = gtk_style_attach (widget->style, widget->window);
	gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

}

static void
embedded_window_size_allocate_handler (GtkWidget *widget)
{
	if (GTK_WIDGET_REALIZED (widget))
	{
		gdk_window_move_resize (widget->window,
					widget->allocation.x,
					widget->allocation.y,
					widget->allocation.width,
					widget->allocation.height);
	}
}

/**
 * glade_widget_embed_using_signals:
 * @window: a #GtkWindow
 *
 * Embeds a window by using signal connection method
 */
static void
glade_widget_embed_using_signals (GladeWidget *widget)
{
	GtkWindow *window;
	
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GTK_IS_WINDOW (widget->object));
	
	window = GTK_WINDOW (widget->object);
	
	if (glade_window_is_embedded (window) || GTK_WIDGET_REALIZED (GTK_WIDGET (window))) {
		g_critical ("Cannot embed a window that is already realized or embedded");
		return;
	}

	GTK_WIDGET_UNSET_FLAGS (GTK_WIDGET (window), GTK_TOPLEVEL);
	gtk_container_set_resize_mode (GTK_CONTAINER (window), GTK_RESIZE_PARENT);

	g_signal_connect (G_OBJECT (window), "realize",
			  G_CALLBACK (embedded_window_realize_handler), NULL);
	g_signal_connect (G_OBJECT (window), "size-allocate",
			  G_CALLBACK (embedded_window_size_allocate_handler), NULL);

	/* mark window as embedded */
	if (glade_embedded_window_quark == 0)
		glade_embedded_window_quark = g_quark_from_string ("GladeEmbedWindow");

	g_object_set_qdata (G_OBJECT (widget), 
			   glade_embedded_window_quark, GINT_TO_POINTER (TRUE));
}
