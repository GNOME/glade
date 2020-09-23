/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Tristan Van Berkom
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

/**
 * SECTION:glade-widget
 * @Short_Description: An object wrapper for the Glade runtime environment.
 *
 * #GladeWidget is the proxy between the instantiated runtime object and
 * the Glade core metadata. This api will be mostly usefull for its
 * convenience api for getting and setting properties (mostly from the plugin).
 */

#include <string.h>
#include <glib-object.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n-lib.h>
#include "glade.h"
#include "glade-accumulators.h"
#include "glade-project.h"
#include "glade-widget-adaptor.h"
#include "glade-widget-private.h"
#include "glade-marshallers.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-placeholder.h"
#include "glade-signal.h"
#include "glade-popup.h"
#include "glade-editor.h"
#include "glade-app.h"
#include "glade-design-view.h"
#include "glade-widget-action.h"
#include "glade-object-stub.h"

static void         glade_widget_set_adaptor           (GladeWidget           *widget,
							GladeWidgetAdaptor    *adaptor);
static void         glade_widget_set_properties        (GladeWidget           *widget,
							GList                 *properties);
static gboolean     glade_window_is_embedded           (GtkWindow             *window);
static gboolean     glade_widget_embed                 (GladeWidget           *widget);
static void         glade_widget_set_object            (GladeWidget           *gwidget, 
							GObject               *new_object, 
							gboolean               destroy);

enum
{
	ADD_SIGNAL_HANDLER,
	REMOVE_SIGNAL_HANDLER,
	CHANGE_SIGNAL_HANDLER,
	BUTTON_PRESS_EVENT,
	BUTTON_RELEASE_EVENT,
	MOTION_NOTIFY_EVENT,
	SUPPORT_CHANGED,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_ADAPTOR,
	PROP_NAME,
	PROP_INTERNAL,
	PROP_ANARCHIST,
	PROP_PROJECT,
	PROP_PROPERTIES,
	PROP_PARENT,
	PROP_INTERNAL_NAME,
	PROP_TEMPLATE,
	PROP_TEMPLATE_EXACT,
	PROP_REASON,
	PROP_TOPLEVEL_WIDTH,
	PROP_TOPLEVEL_HEIGHT,
	PROP_SUPPORT_WARNING,
	PROP_OBJECT
};

static guint         glade_widget_signals[LAST_SIGNAL] = {0};
static GQuark        glade_widget_name_quark = 0;


G_DEFINE_TYPE (GladeWidget, glade_widget, G_TYPE_INITIALLY_UNOWNED)

/*******************************************************************************
                           GladeWidget class methods
 *******************************************************************************/
static void
glade_widget_set_packing_actions (GladeWidget *widget, GladeWidget *parent)
{
	if (widget->packing_actions)
	{
		g_list_foreach (widget->packing_actions, (GFunc)g_object_unref, NULL);
		g_list_free (widget->packing_actions);
		widget->packing_actions = NULL;
	}
	
	if (parent->adaptor->packing_actions)
		widget->packing_actions = glade_widget_adaptor_pack_actions_new (parent->adaptor);
}

static void
glade_widget_add_child_impl (GladeWidget  *widget,
			     GladeWidget  *child,
			     gboolean      at_mouse)
{
	g_object_ref (child);

	/* Safe to set the parent first... setting it afterwards
	 * creates packing properties, and that is not always
	 * desirable.
	 */
	glade_widget_set_parent (child, widget);

	/* Set packing actions first so we have access from the plugin
	 */
	glade_widget_set_packing_actions (child, widget);

	glade_widget_adaptor_add 
		(widget->adaptor, widget->object, child->object);

	/* XXX FIXME:
	 * We have a fundamental flaw here, we set packing props
	 * after parenting the widget so that we can introspect the
	 * values setup by the runtime widget, in which case the plugin
	 * cannot access its packing properties and set them sensitive
	 * or connect to thier signals etc. maybe its not so important
	 * but its a flaw worthy of note, some kind of double pass api
	 * would be needed to accomadate this.
	 */

	
	/* Setup packing properties here so we can introspect the new
	 * values from the backend.
	 */
	glade_widget_set_packing_properties (child, widget);
}

static void
glade_widget_remove_child_impl (GladeWidget  *widget,
				GladeWidget  *child)
{
	glade_widget_adaptor_remove
		(widget->adaptor, widget->object, child->object);

	child->parent = NULL;

	g_object_unref (child);
}

static void
glade_widget_replace_child_impl (GladeWidget *widget,
				 GObject     *old_object,
				 GObject     *new_object)
{
	GladeWidget *gnew_widget = glade_widget_get_from_gobject (new_object);
	GladeWidget *gold_widget = glade_widget_get_from_gobject (old_object);

	if (gnew_widget)
	{
		g_object_ref (gnew_widget);

		gnew_widget->parent = widget;

		/* Set packing actions first so we have access from the plugin
		 */
		glade_widget_set_packing_actions (gnew_widget, widget);
	}

	if (gold_widget)
	{ 
		g_object_unref (gold_widget);

		if (gold_widget != gnew_widget)
			gold_widget->parent = NULL;
	}

	glade_widget_adaptor_replace_child 
		(widget->adaptor, widget->object,
		 old_object, new_object);

	/* Setup packing properties here so we can introspect the new
	 * values from the backend.
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

	glade_project_update_signal_support_warning (widget, new_signal_handler);
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
	GladeSignal *signal_handler_iter;
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
		signal_handler_iter = g_ptr_array_index (signals, i);
		if (glade_signal_equal (signal_handler_iter, old_signal_handler))
		{
			if (strcmp (old_signal_handler->handler,
				    new_signal_handler->handler) != 0)
			{
				g_free (signal_handler_iter->handler);
				signal_handler_iter->handler =
					g_strdup (new_signal_handler->handler);
			}

			/* Handler */
			if (signal_handler_iter->handler)
				g_free (signal_handler_iter->handler);
			signal_handler_iter->handler =
				g_strdup (new_signal_handler->handler);
			
			/* Object */
			if (signal_handler_iter->userdata)
				g_free (signal_handler_iter->userdata);
			signal_handler_iter->userdata = 
				g_strdup (new_signal_handler->userdata);
			
			signal_handler_iter->after    = new_signal_handler->after;
			signal_handler_iter->swapped  = new_signal_handler->swapped;
			break;
		}
	}
}


static gboolean
glade_widget_button_press_event_impl (GladeWidget    *gwidget,
				      GdkEvent       *base_event)
{
	GtkWidget         *widget;
	GdkEventButton    *event = (GdkEventButton *)base_event;
	gboolean           handled = FALSE;

	/* make sure to grab focus, since we may stop default handlers */
	widget = GTK_WIDGET (glade_widget_get_object (gwidget));
	if (gtk_widget_get_can_focus (widget) && !gtk_widget_has_focus (widget))
		gtk_widget_grab_focus (widget);

	/* if it's already selected don't stop default handlers, e.g. toggle button */
	if (event->button == 1)
	{
		if (event->state & GDK_CONTROL_MASK)
		{
			if (glade_project_is_selected (gwidget->project,
						       gwidget->object))
				glade_app_selection_remove 
					(gwidget->object, TRUE);
			else
				glade_app_selection_add
					(gwidget->object, TRUE);
			handled = TRUE;
		}
		else if (glade_project_is_selected (gwidget->project,
						    gwidget->object) == FALSE)
		{
			glade_util_clear_selection ();
			glade_app_selection_set 
				(gwidget->object, TRUE);

			/* Add selection without interrupting event flow 
			 * when shift is down, this allows better behaviour
			 * for GladeFixed children 
			 */
			handled = !(event->state & GDK_SHIFT_MASK);
		}
	}

	/* Give some kind of access in case of missing right button */
	if (!handled && glade_popup_is_popup_event (event))
       	{
			glade_popup_widget_pop (gwidget, event, TRUE);
			handled = TRUE;
	}

	return handled;
}

static gboolean
glade_widget_event_impl (GladeWidget *gwidget,
			 GdkEvent    *event)
{
	gboolean handled = FALSE;

	g_return_val_if_fail (GLADE_IS_WIDGET (gwidget), FALSE);

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
	default:
		break;
	}

	return handled;
}


/**
 * glade_widget_event:
 * @event: A #GdkEvent
 *
 * Feed an event to be handled on the project GladeWidget
 * hierarchy.
 *
 * Returns: whether the event was handled or not.
 */
gboolean
glade_widget_event (GladeWidget *gwidget,
		    GdkEvent    *event)
{
	gboolean   handled = FALSE;

	/* Lets just avoid some synthetic events (like focus-change) */
	if (((GdkEventAny *)event)->window == NULL) return FALSE;

	handled = GLADE_WIDGET_GET_CLASS (gwidget)->event (gwidget, event);

#if 0
	if (event->type != GDK_EXPOSE)
		g_print ("event widget '%s' handled '%d' event '%d'\n",
			 gwidget->name, handled, event->type);
#endif

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
		if (!glade_property_get_enabled (glade_property) ||
		    pclass == NULL  || /* Unaccounted for in the builder */
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

		/* We only check equality on properties introduced by the same class because
		 * others properties could change its default in a derivated class
		 * so its is better to transfer every property and reset them.
		 */
		if (pspec[i]->owner_type == glade_property->widget->adaptor->type &&
		    g_param_values_cmp (pspec[i],
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
glade_widget_build_object (GladeWidget *widget,
			   GladeWidget *template,
			   GladeCreateReason reason)
{
	GParameter          *params;
	GObject             *object;
	guint                n_params, i;
	
	if (reason == GLADE_CREATE_LOAD)
	{
		object = glade_widget_adaptor_construct_object (widget->adaptor, 0, NULL);
		glade_widget_set_object (widget, object, TRUE);
		return object;
	}

	if (template)
		params = glade_widget_template_params (widget, TRUE, &n_params);
	else
		params = glade_widget_adaptor_default_params (widget->adaptor, TRUE, &n_params);

	/* Create the new object with the correct parameters.
	 */
	object = glade_widget_adaptor_construct_object (widget->adaptor, n_params, params);

	free_params (params, n_params);

	/* Dont destroy toplevels when rebuilding, handle that separately */
	glade_widget_set_object (widget, object, reason != GLADE_CREATE_REBUILD);

	if (template)
		params = glade_widget_template_params (widget, FALSE, &n_params);
	else
		params = glade_widget_adaptor_default_params (widget->adaptor, FALSE, &n_params);

	for (i = 0; i < n_params; i++)
		glade_widget_adaptor_set_property (widget->adaptor, object, params[i].name, &(params[i].value));

	free_params (params, n_params);

	return object;
}

/**
 * glade_widget_dup_properties:
 * @dest_widget: the widget we are copying properties for
 * @template_props: the #GladeProperty list to copy
 * @as_load: whether to behave as if loading the project
 * @copy_parentless: whether to copy reffed widgets at all
 * @exact: whether to copy reffed widgets exactly
 *
 * Copies a list of properties, if @as_load is specified, then
 * properties that are not saved to the glade file are ignored.
 *
 * Returns: A newly allocated #GList of new #GladeProperty objects.
 */
GList *
glade_widget_dup_properties (GladeWidget *dest_widget, GList *template_props, gboolean as_load, 
			     gboolean copy_parentless, gboolean exact)
{
	GList *list, *properties = NULL;

	for (list = template_props; list && list->data; list = list->next)
	{
		GladeProperty *prop = list->data;
		
		if (prop->klass->save == FALSE && as_load)
			continue;


		if (prop->klass->parentless_widget && copy_parentless)
		{
			GObject *object = NULL;
			GladeWidget *parentless;

			glade_property_get (prop, &object);

			prop = glade_property_dup (prop, NULL);

			if (object)
			{
				parentless = glade_widget_get_from_gobject (object);

				parentless = glade_widget_dup (parentless, exact);

				glade_widget_set_project (parentless, dest_widget->project);

				glade_property_set (prop, parentless->object);
			}
		} 
		else 
			prop = glade_property_dup (prop, NULL);


		properties = g_list_prepend (properties, prop);
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

	/* XXX FIXME: currently we arent calling this on packing properties,
	 * but doing so could cause crashes because the hash table is not
	 * managed properly
	 */
	if ((prop = glade_widget_get_property (widget, id_property)) != NULL)
	{
		widget->properties = g_list_remove (widget->properties, prop);
		g_hash_table_remove (widget->props_hash, prop->klass->id);
		g_object_unref (prop);
	}
	else
		g_critical ("Couldnt find property %s on widget %s\n",
			    id_property, widget->name);
}

static void
glade_widget_set_catalog_defaults (GList *list)
{
	GList *l;
	for (l = list; l && l->data; l = l->next)
	{
		GladeProperty *prop  = l->data;
		GladePropertyClass *klass = prop->klass;
		
		if (glade_property_equals_value (prop, klass->orig_def) &&
		     g_param_values_cmp (klass->pspec, klass->orig_def, klass->def))
			glade_property_reset (prop);
	}
}

static void
glade_widget_sync_custom_props (GladeWidget *widget)
{
	GList *l;
	for (l = widget->properties; l && l->data; l = l->next)
	{
		GladeProperty *prop  = GLADE_PROPERTY(l->data);

		if (prop->klass->virt || prop->klass->needs_sync)
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
	GObject          *ret_obj;
	GList            *properties = NULL, *list;

	ret_obj = G_OBJECT_CLASS (glade_widget_parent_class)->constructor
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
								       gwidget,
								       name_base);
				g_free (name_base);
			}
			else
				gwidget->name = name_base;

		}
		else if (gwidget->project)
			gwidget->name = glade_project_new_widget_name
				(gwidget->project, gwidget,
				 gwidget->adaptor->generic_name);
		else
			gwidget->name = 
				g_strdup (gwidget->adaptor->generic_name);
	}

	if (gwidget->construct_template)
	{
		properties = glade_widget_dup_properties
			(gwidget, gwidget->construct_template->properties, FALSE, TRUE, gwidget->construct_exact);
		
		glade_widget_set_properties (gwidget, properties);
	}

	if (gwidget->object == NULL)
		glade_widget_build_object (gwidget,
		                           gwidget->construct_template, 
		                           gwidget->construct_reason);

	/* Copy sync parentless widget props here after a dup
	 */
	if (gwidget->construct_reason == GLADE_CREATE_COPY)
	{
		for (list = gwidget->properties; list; list = list->next)
		{
			GladeProperty *property = list->data;
			if (property->klass->parentless_widget)
				glade_property_sync (property);
		}
	}

	/* Setup width/height */
	gwidget->width  = GWA_DEFAULT_WIDTH (gwidget->adaptor);
	gwidget->height = GWA_DEFAULT_HEIGHT (gwidget->adaptor);

	/* Introspect object properties before passing it to post_create,
	 * but only when its freshly created (depend on glade file at
	 * load time and copying properties at dup time).
	 */
	if (gwidget->construct_reason == GLADE_CREATE_USER)
		for (list = gwidget->properties; list; list = list->next)
			glade_property_load (GLADE_PROPERTY (list->data));
	
	/* We only use catalog defaults when the widget was created by the user! */
	if (gwidget->construct_reason == GLADE_CREATE_USER)
		glade_widget_set_catalog_defaults (gwidget->properties);
	
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
	
	if (GTK_IS_WIDGET (gwidget->object) && !gtk_widget_is_toplevel (GTK_WIDGET (gwidget->object)))
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

#if 0
	/* A good way to check if refcounts are balancing at project close time */
	g_print ("Finalizing widget %s\n", widget->name);
#endif

	g_free (widget->name);
	g_free (widget->internal);
	g_free (widget->support_warning);
	g_hash_table_destroy (widget->signals);

	if (widget->props_hash)
		g_hash_table_destroy (widget->props_hash);
	if (widget->pack_props_hash)
		g_hash_table_destroy (widget->pack_props_hash);

	G_OBJECT_CLASS (glade_widget_parent_class)->finalize(object);
}

static void
reset_object_property (GladeProperty *property,
		       GladeProject  *project)
{
	if (glade_property_class_is_object (property->klass, 
					    project ? glade_project_get_format (project) :
					    GLADE_PROJECT_FORMAT_LIBGLADE))
		glade_property_reset (property);
}

static void
glade_widget_dispose (GObject *object)
{
	GladeWidget *widget = GLADE_WIDGET (object);

	glade_widget_push_superuser ();

	/* Release references by way of object properties... */
	while (widget->prop_refs)
	{
		GladeProperty *property = GLADE_PROPERTY (widget->prop_refs->data);
		glade_property_set (property, NULL);
	}

	if (widget->properties)
		g_list_foreach (widget->properties, (GFunc)reset_object_property, widget->project);

	/* We have to make sure properties release thier references on other widgets first 
	 * hence the reset (for object properties) */
	if (widget->properties)
	{
		g_list_foreach (widget->properties, (GFunc)g_object_unref, NULL);
		g_list_free (widget->properties);
		widget->properties = NULL;
	}

	glade_widget_set_object (widget, NULL, TRUE);
	
	if (widget->packing_properties)
	{
		g_list_foreach (widget->packing_properties, (GFunc)g_object_unref, NULL);
		g_list_free (widget->packing_properties);
		widget->packing_properties = NULL;
	}
	
	if (widget->actions)
	{
		g_list_foreach (widget->actions, (GFunc)g_object_unref, NULL);
		g_list_free (widget->actions);
		widget->actions = NULL;
	}
	
	if (widget->packing_actions)
	{
		g_list_foreach (widget->packing_actions, (GFunc)g_object_unref, NULL);
		g_list_free (widget->packing_actions);
		widget->packing_actions = NULL;
	}

	glade_widget_pop_superuser ();

	G_OBJECT_CLASS (glade_widget_parent_class)->dispose (object);
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
			glade_widget_set_object (widget, g_value_get_object (value), TRUE);
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
	case PROP_TEMPLATE_EXACT:
		widget->construct_exact = g_value_get_boolean (value);
		break;
	case PROP_REASON:
		widget->construct_reason = g_value_get_int (value);
		break;
	case PROP_TOPLEVEL_WIDTH:
		widget->width = g_value_get_int (value);
		break;
	case PROP_TOPLEVEL_HEIGHT:
		widget->height = g_value_get_int (value);
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
	case PROP_TOPLEVEL_WIDTH:
		g_value_set_int (value, widget->width);
		break;
	case PROP_TOPLEVEL_HEIGHT:
		g_value_set_int (value, widget->height);
		break;
	case PROP_SUPPORT_WARNING:
		g_value_set_string (value, widget->support_warning);
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
	widget->signals = g_hash_table_new_full
		(g_str_hash, g_str_equal,
		 (GDestroyNotify) g_free,
		 (GDestroyNotify) free_signals);
	
	/* Initial invalid values */
	widget->width  = -1;
	widget->height = -1;
}

static void
glade_widget_class_init (GladeWidgetClass *klass)
{
	GObjectClass *object_class;

	if (glade_widget_name_quark == 0)
		glade_widget_name_quark = 
			g_quark_from_static_string ("GladeWidgetDataTag");

	object_class = G_OBJECT_CLASS (klass);

	object_class->constructor     = glade_widget_constructor;
	object_class->finalize        = glade_widget_finalize;
	object_class->dispose         = glade_widget_dispose;
	object_class->set_property    = glade_widget_set_real_property;
	object_class->get_property    = glade_widget_get_real_property;

	klass->add_child              = glade_widget_add_child_impl;
	klass->remove_child           = glade_widget_remove_child_impl;
	klass->replace_child          = glade_widget_replace_child_impl;
	klass->event                  = glade_widget_event_impl;

	klass->add_signal_handler     = glade_widget_add_signal_handler_impl;
	klass->remove_signal_handler  = glade_widget_remove_signal_handler_impl;
	klass->change_signal_handler  = glade_widget_change_signal_handler_impl;

	klass->button_press_event     = glade_widget_button_press_event_impl;
	klass->button_release_event   = NULL;
	klass->motion_notify_event    = NULL;

	g_object_class_install_property
		(object_class, PROP_ADAPTOR,
		   g_param_spec_object ("adaptor", _("Adaptor"),
					_("The class adaptor for the associated widget"),
					GLADE_TYPE_WIDGET_ADAPTOR,
					G_PARAM_READWRITE |
					G_PARAM_CONSTRUCT_ONLY));
	
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
		(object_class, PROP_TEMPLATE_EXACT,
		 g_param_spec_boolean ("template-exact", _("Exact Template"),
				       _("Whether we are creating an exact duplicate when using a template"),
				       FALSE, G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property
		(object_class, 	PROP_REASON,
		 g_param_spec_int ("reason", _("Reason"),
				   _("A GladeCreateReason for this creation"),
				   GLADE_CREATE_USER,
				   GLADE_CREATE_REASONS - 1,
				   GLADE_CREATE_USER,
				   G_PARAM_CONSTRUCT_ONLY|G_PARAM_WRITABLE));

	g_object_class_install_property
		(object_class, 	PROP_TOPLEVEL_WIDTH,
		 g_param_spec_int ("toplevel-width", _("Toplevel Width"),
				   _("The width of the widget when toplevel in "
				     "the GladeDesignLayout"),
				   -1,
				   G_MAXINT,
				   -1,
				   G_PARAM_READWRITE));

	g_object_class_install_property
		(object_class, 	PROP_TOPLEVEL_HEIGHT,
		 g_param_spec_int ("toplevel-height", _("Toplevel Height"),
				   _("The height of the widget when toplevel in "
				     "the GladeDesignLayout"),
				   -1,
				   G_MAXINT,
				   -1,
				   G_PARAM_READWRITE));

	g_object_class_install_property
		(object_class, 	PROP_SUPPORT_WARNING,
		 g_param_spec_string ("support-warning", _("Support Warning"),
				      _("A warning string about version mismatches"),
				      NULL, G_PARAM_READABLE));

	/* this property setter depends on adaptor and internal properties to be set
	 * so we install it at the end since in new glib construct properties are
	 * set in instalation order!
	 */
	g_object_class_install_property
		(object_class, PROP_OBJECT,
		 g_param_spec_object ("object", _("Object"),
				      _("The object associated"),
				      G_TYPE_OBJECT,
				      G_PARAM_READWRITE));

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
			      GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);

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
			      GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);


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
			      GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);


	/**
	 * GladeWidget::support-changed:
	 * @gladewidget: the #GladeWidget which received the signal.
	 *
	 * Emitted when property and signal support metadatas and messages
	 * have been updated.
	 */
	glade_widget_signals[SUPPORT_CHANGED] =
		g_signal_new ("support-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);


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
		orig_prop = glade_widget_get_pack_property (template_widget, dup_prop->klass->id);
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
								      child->project,
								      child);
		
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
glade_widget_get_internal_func (GladeWidget  *main_target, 
				GladeWidget  *parent, 
				GladeWidget **parent_ret)
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

		/* Limit the itterations into where the copy routine stared */
		if (gwidget == main_target)
			break;
	}

	return NULL;
}


static GladeWidget *
glade_widget_dup_internal (GladeWidget *main_target,
			   GladeWidget *parent,
			   GladeWidget *template_widget,
			   gboolean     exact)
{
	GladeGetInternalFunc  get_internal;
	GladeWidget *gwidget = NULL, *internal_parent;
	GList       *children;
	GtkWidget   *placeholder;
	gchar       *child_type;
	GList       *l;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (template_widget), NULL);
	g_return_val_if_fail (parent == NULL || GLADE_IS_WIDGET (parent), NULL);

	/* Dont actually duplicate internal widgets, but recurse through them anyway. */
	if (parent && template_widget->internal)
	{
		GObject *internal_object = NULL;

		if ((get_internal = 
		     glade_widget_get_internal_func (main_target, parent, &internal_parent)) != NULL)
		{
			/* We cant use "parent" here, we have to recurse up the hierarchy to find
			 * the "parent" that has `get_internal_child' support (i.e. internal children
			 * may have depth).
			 */
			if ((internal_object = get_internal (internal_parent->adaptor,
							     internal_parent->object, 
							     template_widget->internal)) != NULL)
       			{
				gwidget = glade_widget_get_from_gobject (internal_object);
				g_assert (gwidget);
			}
		}
	}

	/* If either it was not internal, or we failed to lookup the internal child
	* in the copied hierarchy (this can happen when copying an internal vbox from
	* a composite dialog for instance). */
	if (gwidget == NULL)
	{
		gchar *name = g_strdup (template_widget->name);
		gwidget = glade_widget_adaptor_create_widget
			(template_widget->adaptor, FALSE,
			 "name", name,
			 "parent", parent, 
			 "project", template_widget->project,
			 "template", template_widget, 
			 "template-exact", exact,
			 "reason", GLADE_CREATE_COPY, NULL);
		g_free (name);
	}

	/* Copy signals over here regardless of internal or not... */
	if (exact)
		glade_widget_copy_signals (gwidget, template_widget);
	
	if ((children =
	     glade_widget_adaptor_get_children (template_widget->adaptor,
						template_widget->object)) != NULL)
	{
		GList       *list;

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
				child_dup = glade_widget_dup_internal (main_target, gwidget, child_gwidget, exact);

				if (child_dup->internal == NULL)
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
		glade_widget_copy_properties (gwidget, template_widget, TRUE, exact);
	
	if (gwidget->packing_properties == NULL)
		gwidget->packing_properties = 
			glade_widget_dup_properties (gwidget, template_widget->packing_properties, FALSE, FALSE, FALSE);
	
	/* If custom properties are still at thier
	 * default value, they need to be synced.
	 */
	glade_widget_sync_custom_props (gwidget);
	
	/* Some properties may not be synced so we reload them */
	for (l = gwidget->properties; l; l = l->next)
		glade_property_load (GLADE_PROPERTY (l->data));

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
				glade_widget_dup_properties (gchild, gchild->properties, TRUE, FALSE, FALSE);
			
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
					(gchild, gchild->packing_properties, TRUE, FALSE, FALSE);

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
				(NULL, gwidget, &internal_parent);

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
		if (widget->props_hash)
			g_hash_table_destroy (widget->props_hash);

		widget->properties = properties;
		widget->props_hash = g_hash_table_new (g_str_hash, g_str_equal);

		for (list = properties; list; list = list->next)
		{
			property = list->data;
			property->widget = widget;

			g_hash_table_insert (widget->props_hash, property->klass->id, property);
		}
	}
}

static void
glade_widget_set_actions (GladeWidget *widget, GladeWidgetAdaptor *adaptor)
{
	GList *l;
	
	for (l = adaptor->actions; l; l = g_list_next (l))
	{
		GWActionClass *action = l->data;
		GObject *obj = g_object_new (GLADE_TYPE_WIDGET_ACTION,
					     "class", action, NULL);
		
		widget->actions = g_list_prepend (widget->actions,
						  GLADE_WIDGET_ACTION (obj));
	}
	widget->actions = g_list_reverse (widget->actions);
}

static void
glade_widget_set_adaptor (GladeWidget *widget, GladeWidgetAdaptor *adaptor)
{
	GladePropertyClass *property_class;
	GladeProperty *property;
	GList *list, *properties = NULL;

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
							    widget, NULL)) == NULL)
			{
				g_warning ("Failed to create [%s] property",
					   property_class->id);
				continue;
			}
			properties = g_list_prepend (properties, property);
		}
		glade_widget_set_properties (widget, g_list_reverse (properties));
	}
	
	/* Create actions from adaptor */
	glade_widget_set_actions (widget, adaptor);
}

static gboolean
expose_draw_selection (GtkWidget       *widget_gtk,
		       GdkEventExpose  *event,
		       GladeWidget     *gwidget)
{
	glade_util_draw_selection_nodes (event->window);
        return FALSE;
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
		/* Make sure we can recieve the kind of events we're
		 * connecting for 
		 */
		gtk_widget_add_events (widget_gtk,
				       GDK_POINTER_MOTION_MASK      | /* Handle pointer events */
				       GDK_POINTER_MOTION_HINT_MASK | /* for drag/resize and   */
				       GDK_BUTTON_PRESS_MASK        | /* managing selection.   */
				       GDK_BUTTON_RELEASE_MASK);

		g_signal_connect (G_OBJECT (widget_gtk), "event",
				  callback, gwidget);

		g_signal_connect_after (G_OBJECT (widget_gtk), "expose-event",
					G_CALLBACK (expose_draw_selection), gwidget);
		
		
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
				glade_widget_connect_signal_handlers 
					(GTK_WIDGET (list->data), callback, gwidget);
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
		property       = glade_property_new (property_class, widget, NULL);
		packing_props  = g_list_prepend (packing_props, property);

	}
	return g_list_reverse (packing_props);
}

/* Private API */

GList *
_glade_widget_peek_prop_refs (GladeWidget      *widget)
{
  return widget->prop_refs;
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
glade_widget_add_to_layout (GladeWidget *widget, GtkWidget *layout)
{
	if (gtk_bin_get_child (GTK_BIN (layout)) != NULL)
		gtk_container_remove (GTK_CONTAINER (layout), gtk_bin_get_child (GTK_BIN (layout)));

	gtk_container_add (GTK_CONTAINER (layout), GTK_WIDGET (widget->object));

	gtk_widget_show_all (GTK_WIDGET (widget->object));
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
	GladeProperty *property;
	GladeProject *project;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	
	/* Position window at saved coordinates or in the center */
	if (GTK_IS_WIDGET (widget->object) && !widget->parent)
	{
		if (GTK_IS_WINDOW (widget->object) && !glade_widget_embed (widget))
		{
			g_warning ("Unable to embed %s\n", widget->name);
			return;
		}

		/* Maybe a property references this widget internally, show that widget instead */
		if ((property = glade_widget_get_parentless_widget_ref (widget)) != NULL)
		{
			/* will never happen, paranoid check to avoid endless recursion. */
			if (property->widget != widget)
				glade_widget_show (property->widget);
			return;
		}

		project = glade_widget_get_project (widget);
		if (!project) return;

		view = glade_design_view_get_from_project (project);
		if (!view) return;

		layout = GTK_WIDGET (glade_design_view_get_layout (view));
		if (!layout) return;
		
		if (gtk_widget_get_realized (layout))
			glade_widget_add_to_layout (widget, layout);
		else
			g_signal_connect_data (G_OBJECT (layout), "map", 
					       G_CALLBACK (glade_widget_add_to_layout), 
					       widget, NULL, G_CONNECT_AFTER | G_CONNECT_SWAPPED);

	} 
	else if (GTK_IS_WIDGET (widget->object))
	{
		GladeWidget *toplevel = glade_widget_get_toplevel (widget);
		if (toplevel != widget)
			glade_widget_show (toplevel);
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
	if (GTK_IS_WIDGET (widget->object))
	{
		GladeDesignView *view;
		GtkWidget *layout;
		GladeProject *project;

		project = glade_widget_get_project (widget);

		if (project &&
		    (view = glade_design_view_get_from_project (project)) != NULL)
		{
			GtkWidget *child;

			layout = GTK_WIDGET (glade_design_view_get_layout (view));
			child = gtk_bin_get_child (GTK_BIN (layout));

			if (child == GTK_WIDGET (widget->object))
				gtk_container_remove (GTK_CONTAINER (layout), child);
		}

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
 * Note: this is used to track properties on other objects that
 *       reffer to this object.
 */
void
glade_widget_add_prop_ref (GladeWidget *widget, GladeProperty *property)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_PROPERTY (property));

	if (!g_list_find (widget->prop_refs, property))
		widget->prop_refs = g_list_prepend (widget->prop_refs, property);

	/* parentless widget reffed widgets are added to thier reffering widgets. 
	 * they cant be in the design view.
	 */
	if (property->klass->parentless_widget)
		glade_widget_hide (widget);
}

/**
 * glade_widget_remove_prop_ref:
 * @widget: A #GladeWidget
 * @property: the #GladeProperty
 *
 * Removes @property from @widget 's list of referenced properties.
 *
 * Note: this is used to track properties on other objects that
 *       reffer to this object.
 */
void
glade_widget_remove_prop_ref (GladeWidget *widget, GladeProperty *property)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_PROPERTY (property));

	widget->prop_refs = g_list_remove (widget->prop_refs, property);
}

GladeProperty *
glade_widget_get_parentless_widget_ref (GladeWidget *widget)
{
	GList         *l;
	GladeProperty *property;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

	for (l = widget->prop_refs; l && l->data; l = l->next)
	{
		property = GLADE_PROPERTY (l->data);

		if (property->klass->parentless_widget)
			/* only one external property can point to this widget */
			return property;
	}
	return NULL;
}


GList *
glade_widget_get_parentless_reffed_widgets (GladeWidget *widget)
{
	GObject       *reffed = NULL;
	GladeProperty *property = NULL;
	GList         *l, *widgets = NULL;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

	for (l = widget->properties; l && l->data; l = l->next)
	{
		property = GLADE_PROPERTY (l->data);
		reffed   = NULL;

		if (property->klass->parentless_widget)
		{
			glade_property_get (property, &reffed);
			if (reffed)
				widgets = g_list_prepend (widgets, glade_widget_get_from_gobject (reffed));
		}
	}
	return g_list_reverse (widgets);
}

static void
glade_widget_accum_signal_foreach (const gchar *key,
				   GPtrArray   *signals,
				   GList      **list)
{
	GladeSignal *signal;
	gint i;

	for (i = 0; i < signals->len; i++)
	{
		signal = (GladeSignal *)signals->pdata[i];
		*list = g_list_append (*list, signal);
	}
}

/**
 * glade_widget_get_signal_list:
 * @widget:   a 'dest' #GladeWidget
 *
 * Compiles a list of #GladeSignal elements
 *
 * Returns: a newly allocated #GList of #GladeSignals, the caller
 * must call g_list_free() to free the list.
 */
GList *
glade_widget_get_signal_list (GladeWidget *widget)
{
	GList *signals = NULL;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

	g_hash_table_foreach (widget->signals,
			      (GHFunc)glade_widget_accum_signal_foreach,
			      &signals);

	return signals;
}

static void
glade_widget_copy_signal_foreach (const gchar *key,
				  GPtrArray   *signals,
				  GladeWidget *dest)
{
	GladeSignal *signal;
	gint i;

	for (i = 0; i < signals->len; i++)
	{
		signal = (GladeSignal *)signals->pdata[i];
		glade_widget_add_signal_handler	(dest, signal);
	}
}

/**
 * glade_widget_copy_signals:
 * @widget:   a 'dest' #GladeWidget
 * @template_widget: a 'src' #GladeWidget
 *
 * Sets signals in @widget based on the values of
 * matching signals in @template_widget
 */
void
glade_widget_copy_signals (GladeWidget *widget,
			   GladeWidget *template_widget)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_WIDGET (template_widget));

	g_hash_table_foreach (template_widget->signals,
			      (GHFunc)glade_widget_copy_signal_foreach,
			      widget);
}

/**
 * glade_widget_copy_properties:
 * @widget:   a 'dest' #GladeWidget
 * @template_widget: a 'src' #GladeWidget
 * @copy_parentless: whether to copy reffed widgets at all
 * @exact: whether to copy reffed widgets exactly
 *
 * Sets properties in @widget based on the values of
 * matching properties in @template_widget
 */
void
glade_widget_copy_properties (GladeWidget *widget,
			      GladeWidget *template_widget,
			      gboolean     copy_parentless,
			      gboolean     exact)
{
	GList *l;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_WIDGET (template_widget));

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
		{
			if (template_prop->klass->parentless_widget && copy_parentless)
			{
				GObject *object = NULL;
				GladeWidget *parentless;

				glade_property_get (template_prop, &object);
				if (object)
				{
					parentless = glade_widget_get_from_gobject (object);
					parentless = glade_widget_dup (parentless, exact);

					glade_widget_set_project (parentless, widget->project);

					glade_property_set (widget_prop, parentless->object);
				}
				else
					glade_property_set (widget_prop, NULL);
			}
			else
				glade_property_set_value (widget_prop, template_prop->value);
		}
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
 * @exact: whether or not to creat an exact duplicate
 *
 * Creates a deep copy of #GladeWidget. if @exact is specified,
 * the widget name is preserved and signals are carried over
 * (this is used to maintain names & signals in Cut/Paste context
 * as opposed to Copy/Paste contexts).
 *
 * Returns: The newly created #GladeWidget
 */
GladeWidget *
glade_widget_dup (GladeWidget *template_widget,
		  gboolean     exact)
{
	GladeWidget *widget;

	g_return_val_if_fail (GLADE_IS_WIDGET (template_widget), NULL);
	
	glade_widget_push_superuser ();
	widget = glade_widget_dup_internal (template_widget, NULL, template_widget, exact);
	glade_widget_pop_superuser ();

	return widget;
}

typedef struct
{
	GladeProperty *property;
	GValue value;
} PropertyData;

/**
 * glade_widget_rebuild:
 * @gwidget: a #GladeWidget
 *
 * Replaces the current widget instance with
 * a new one while preserving all properties children and
 * takes care of reparenting.
 *
 */
void
glade_widget_rebuild (GladeWidget *gwidget)
{
	GObject            *new_object, *old_object;
	GladeWidgetAdaptor *adaptor;
	GladeProject       *project = NULL;
	GList              *children;
	gboolean            reselect = FALSE;
	GList              *restore_properties = NULL;
	GList              *save_properties, *l;	
	GladeWidget        *parent = NULL;


	g_return_if_fail (GLADE_IS_WIDGET (gwidget));

	adaptor = gwidget->adaptor;

	if (gwidget->project && 
	    glade_project_has_object (gwidget->project, gwidget->object))
		project = gwidget->project;

	if (gwidget->parent && 
	    glade_widget_adaptor_has_child (gwidget->parent->adaptor,
					    gwidget->parent->object,
					    gwidget->object))
		parent  = gwidget->parent;

	g_object_ref (gwidget);

	/* Extract and keep the child hierarchies aside... */
	children = glade_widget_extract_children (gwidget);

	/* Here we take care removing the widget from the project and
	 * the selection before rebuilding the instance.
	 */
	if (project)
	{
		if (glade_project_is_selected (project, gwidget->object))
		{
			reselect = TRUE;
			glade_project_selection_remove
				(project, gwidget->object, FALSE);
		}
		glade_project_remove_object (project, gwidget->object);
	}

	/* parentless_widget and object properties that reffer to this widget 
	 * should be unset before transfering */
	l = g_list_copy (gwidget->properties);
	save_properties = g_list_copy (gwidget->prop_refs);
	save_properties = g_list_concat (l, save_properties);

	for (l = save_properties; l; l = l->next)
	{
		GladeProperty *property = GLADE_PROPERTY (l->data);
		if (property->widget != gwidget || property->klass->parentless_widget)
		{
			PropertyData *prop_data;

			if (!G_IS_PARAM_SPEC_OBJECT (property->klass->pspec))
				g_warning ("Parentless widget property should be of object type");

			prop_data = g_new0 (PropertyData, 1);
			prop_data->property = property;

			if (property->widget == gwidget)
			{
				g_value_init (&prop_data->value, property->value->g_type);
				g_value_copy (property->value, &prop_data->value);
			}
			restore_properties = g_list_prepend (restore_properties,
							     prop_data);
			glade_property_set (property, NULL);
		}
	}
	g_list_free (save_properties);

	/* Remove old object from parent
	 */
	if (parent)
		glade_widget_remove_child (parent, gwidget);

	/* Hold a reference to the old widget while we transport properties
	 * and children from it
	 */
	old_object = g_object_ref (glade_widget_get_object (gwidget));
	new_object = glade_widget_build_object (gwidget, gwidget, GLADE_CREATE_REBUILD);

	/* Only call this once the object has a proper GladeWidget */
	glade_widget_adaptor_post_create (adaptor, new_object, GLADE_CREATE_REBUILD);

	/* Must call dispose for cases like dialogs and toplevels */
	if (GTK_IS_WINDOW (old_object))
		gtk_widget_destroy (GTK_WIDGET (old_object));
	else
		g_object_unref (old_object);

	/* Reparent any children of the old object to the new object
	 * (this function will consume and free the child list).
	 */
	glade_widget_push_superuser ();
	glade_widget_insert_children (gwidget, children);
	glade_widget_pop_superuser ();

	/* Add new object to parent
	 */
	if (parent)
		glade_widget_add_child (parent, gwidget, FALSE);

	/* Custom properties aren't transfered in build_object, since build_object
	 * is only concerned with object creation.
	 */
	glade_widget_sync_custom_props (gwidget);

	/* Setting parentless_widget and prop_ref properties back */
	for (l = restore_properties; l; l = l->next)
	{
		PropertyData *prop_data = l->data;
		GladeProperty *property = prop_data->property;
		
		if (property->widget == gwidget)
		{
			glade_property_set_value (property, &prop_data->value);
			g_value_unset (&prop_data->value);
		}
		else
		{
			/* restore property references on rebuilt objects */
			glade_property_set (property, gwidget->object);
		}
		g_free (prop_data);
	}
	g_list_free (restore_properties);

	/* Sync packing.
	 */
	if (gwidget->parent)
		glade_widget_sync_packing_props (gwidget);

	/* If the widget was in a project (and maybe the selection), then
	 * restore that stuff.
	 */
	if (project)
	{
		glade_project_add_object (project, NULL, gwidget->object);
		if (reselect)
			glade_project_selection_add (project, gwidget->object, TRUE);
	}

 	/* We shouldnt show if its not already visible */
	if (gwidget->visible)
		glade_widget_show (gwidget);

	g_object_unref (gwidget);
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
	if (widget->name != name) 
	{
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
	if (widget->project != project)
	{
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
	GladeProperty *property;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	g_return_val_if_fail (id_property != NULL, NULL);

	if (widget->props_hash && 
	    (property = g_hash_table_lookup (widget->props_hash, id_property)))
		return property;

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
	GladeProperty *property;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	g_return_val_if_fail (id_property != NULL, NULL);

	if (widget->pack_props_hash && 
	    (property = g_hash_table_lookup (widget->pack_props_hash, id_property)))
		return property;

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
 *
 */
gboolean
glade_widget_property_get (GladeWidget      *widget,
			   const gchar      *id_property,
			   ...)
{
	GladeProperty *property;
	va_list        vl;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (id_property != NULL, FALSE);

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
	g_return_val_if_fail (id_property != NULL, FALSE);

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
	g_return_val_if_fail (id_property != NULL, FALSE);

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
	g_return_val_if_fail (id_property != NULL, FALSE);

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
	g_return_val_if_fail (id_property != NULL, FALSE);

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
	g_return_val_if_fail (id_property != NULL, FALSE);

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
	g_return_val_if_fail (id_property != NULL, FALSE);

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
	g_return_val_if_fail (id_property != NULL, FALSE);

	if ((property = glade_widget_get_pack_property (widget, id_property)) != NULL)
	{
		glade_property_set_enabled (property, enabled);
		return TRUE;
	}
	return FALSE;
}

/**
 * glade_widget_property_set_save_always:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @setting: the setting 
 *
 * Sets whether @id_property in @widget should be special cased
 * to always be saved regardless of its default value.
 * (used for some special cases like properties
 * that are assigned initial values in composite widgets
 * or derived widget code).
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_property_set_save_always (GladeWidget      *widget,
				       const gchar      *id_property,
				       gboolean          setting)
{
	GladeProperty *property;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (id_property != NULL, FALSE);

	if ((property = glade_widget_get_property (widget, id_property)) != NULL)
	{
		glade_property_set_save_always (property, setting);
		return TRUE;
	}
	return FALSE;
}

/**
 * glade_widget_pack_property_set_save_always:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @setting: the setting 
 *
 * Sets whether @id_property in @widget should be special cased
 * to always be saved regardless of its default value.
 * (used for some special cases like properties
 * that are assigned initial values in composite widgets
 * or derived widget code).
 *
 * Returns: whether @id_property was found or not.
 */
gboolean
glade_widget_pack_property_set_save_always (GladeWidget      *widget,
					    const gchar      *id_property,
					    gboolean          setting)
{
	GladeProperty *property;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (id_property != NULL, FALSE);

	if ((property = glade_widget_get_pack_property (widget, id_property)) != NULL)
	{
		glade_property_set_save_always (property, setting);
		return TRUE;
	}
	return FALSE;
}

/**
 * glade_widget_property_string:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @value: the #GValue to print or %NULL
 *
 * Creates a printable string representing @id_property in
 * @widget, if @value is specified it will be used in place
 * of @id_property's real value (this is a convinience
 * function to print/debug properties usually from plugin
 * backends).
 *
 * Returns: A newly allocated string representing @id_property
 */
gchar *
glade_widget_property_string (GladeWidget      *widget,
			      const gchar      *id_property,
			      const GValue     *value)
{
	GladeProperty *property;
	gchar         *ret_string = NULL;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	g_return_val_if_fail (id_property != NULL, NULL);

	if ((property = glade_widget_get_property (widget, id_property)) != NULL)
		ret_string = glade_widget_adaptor_string_from_value
			(GLADE_WIDGET_ADAPTOR (property->klass->handle),
			 property->klass, value ? value : property->value,
			 glade_project_get_format (widget->project));

	return ret_string;
}

/**
 * glade_widget_pack_property_string:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 * @value: the #GValue to print or %NULL
 *
 * Same as glade_widget_property_string() but for packing
 * properties.
 *
 * Returns: A newly allocated string representing @id_property
 */
gchar *
glade_widget_pack_property_string (GladeWidget      *widget,
				   const gchar      *id_property,
				   const GValue     *value)
{
	GladeProperty *property;
	gchar         *ret_string = NULL;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	g_return_val_if_fail (id_property != NULL, NULL);

	if ((property = glade_widget_get_pack_property (widget, id_property)) != NULL)
		ret_string = glade_widget_adaptor_string_from_value
			(GLADE_WIDGET_ADAPTOR (property->klass->handle),
			 property->klass, value ? value : property->value,
			 glade_project_get_format (widget->project));

	return ret_string;
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

static gboolean
glade_widget_property_default_common (GladeWidget *widget,
			       const gchar *id_property, gboolean original)
{
	GladeProperty *property;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

	if ((property = glade_widget_get_property (widget, id_property)) != NULL)
		return (original) ? glade_property_original_default (property) :
			glade_property_default (property);

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
	return glade_widget_property_default_common (widget, id_property, FALSE);
}

/**
 * glade_widget_property_original_default:
 * @widget: a #GladeWidget
 * @id_property: a string naming a #GladeProperty
 *
 * Returns: whether whether @id_property was found and is 
 * currently set to it's original default value.
 */
gboolean
glade_widget_property_original_default (GladeWidget *widget,
			       const gchar *id_property)
{
	return glade_widget_property_default_common (widget, id_property, TRUE);
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
	GList *old_order = NULL;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_WIDGET (child));
	g_return_if_fail (property_name != NULL && value != NULL);


	if (widget->project &&
	    widget->in_project)
		old_order = glade_widget_get_children (widget);

	glade_widget_adaptor_child_set_property (widget->adaptor,
						 widget->object,
						 child->object,
						 property_name, value);

	/* After setting a child property... it's possible the order of children
	 * in the parent has been effected.
	 *
	 * If this is the case then we need to signal the GladeProject that
	 * it's rows have been reordered so that any connected views update
	 * themselves properly.
	 */
	if (widget->project &&
	    widget->in_project)
		glade_project_check_reordered (widget->project, widget, old_order);

	g_list_free (old_order);
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

static gboolean 
glade_widget_event_private (GtkWidget   *widget,
			    GdkEvent    *event,
			    GladeWidget *gwidget)
{
	GtkWidget *layout = widget;

	/* Find the parenting layout container */
	while (layout && !GLADE_IS_DESIGN_LAYOUT (layout))
		layout = gtk_widget_get_parent (layout);

	/* Event outside the logical heirarchy, could be a menuitem
	 * or other such popup window, we'll presume to send it directly
	 * to the GladeWidget that connected here.
	 */
	if (!layout)
		return glade_widget_event (gwidget, event);

	/* Let the parenting GladeDesignLayout decide which GladeWidget to
	 * marshall this event to.
	 */
	if (GLADE_IS_DESIGN_LAYOUT (layout))
		return glade_design_layout_widget_event (GLADE_DESIGN_LAYOUT (layout),
							 gwidget, event);
	else
		return FALSE;
}

static void
glade_widget_set_object (GladeWidget *gwidget, GObject *new_object, gboolean destroy)
{
	GladeWidgetAdaptor *adaptor;
	GObject            *old_object;
	
	g_return_if_fail (GLADE_IS_WIDGET (gwidget));
	adaptor = gwidget->adaptor;
	g_return_if_fail (adaptor);
	g_return_if_fail (new_object == NULL || 
			  g_type_is_a (G_OBJECT_TYPE (new_object), 
			               adaptor->type));

	if (gwidget->object == new_object)
		return;

	old_object      = gwidget->object;
	gwidget->object = new_object;

	if (new_object)
	{
		/* Add internal reference to new widget if its not internal */
		if (gwidget->internal == NULL)
		{
			/* Assume ownership of floating objects */
			if (g_object_is_floating (new_object))
				g_object_ref_sink (new_object);
		}
	
		g_object_set_qdata (G_OBJECT (new_object), glade_widget_name_quark, gwidget);

		if (g_type_is_a (gwidget->adaptor->type, GTK_TYPE_WIDGET))
		{
			/* Disable any built-in DnD
			 */
			gtk_drag_dest_unset (GTK_WIDGET (new_object));
			gtk_drag_source_unset (GTK_WIDGET (new_object));
			
			/* Take care of drawing selection directly on widgets
			 * for the time being
			 */
			glade_widget_connect_signal_handlers
				(GTK_WIDGET(new_object),
				 G_CALLBACK (glade_widget_event_private),
				 gwidget);
		}
	}

	/* Remove internal reference to old widget */
	if (old_object)
	{
		/* From this point on, the GladeWidget is no longer retrievable with
		 * glade_widget_get_from_gobject() */
		g_object_set_qdata (G_OBJECT (old_object), glade_widget_name_quark, NULL);

		if (gwidget->internal == NULL)
		{
#if _YOU_WANT_TO_LOOK_AT_PROJECT_REFCOUNT_BALANCING_
			g_print ("Killing '%s::%s' widget's object with reference count %d\n",
				 gwidget->adaptor->name, gwidget->name ? gwidget->name : "(unknown)",
				 old_object->ref_count);
#endif
			if (GTK_IS_WINDOW (old_object) && destroy)
				gtk_widget_destroy (GTK_WIDGET (old_object));
			else
				g_object_unref (old_object);
			
		}
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
	
	if (parent) glade_widget_set_packing_actions (widget, parent);

	g_object_notify (G_OBJECT (widget), "parent");
}

/**
 * glade_widget_get_children:
 * @widget: A #GladeWidget
 *
 * Fetches any wrapped children of @widget.
 *
 * Returns: The children of widget
 *
 * <note><para>This differs from a direct call to glade_widget_adaptor_get_children() as
 * it only returns children which have an associated GladeWidget. This function will
 * not return any placeholders or internal composite children that have not been 
 * exposed for Glade configuration</para></note>
 */
GList *
glade_widget_get_children (GladeWidget *widget)
{
	GList *adapter_children; 
	GList *real_children = NULL;
	GList *node;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

	adapter_children = glade_widget_adaptor_get_children (glade_widget_get_adaptor (widget),
	                                                      widget->object);
	
	for (node = adapter_children; node != NULL; node = g_list_next (node))
	{
		if (glade_widget_get_from_gobject (node->data))
		{
			real_children = g_list_prepend (real_children, node->data);
		}
	}
	g_list_free (adapter_children);
	
	return g_list_reverse (real_children);
}
	

/**
 * glade_widget_get_toplevel:
 * @widget: A #GladeWidget
 *
 * Returns: The toplevel #GladeWidget in the hierarchy (or @widget)
 */
GladeWidget *
glade_widget_get_toplevel (GladeWidget *widget)
{
	GladeWidget *toplevel = widget;
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

	while (toplevel->parent)
		toplevel = toplevel->parent;

	return toplevel;
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
	GList *list;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_WIDGET (container));

	g_list_foreach (widget->packing_properties, (GFunc)g_object_unref, NULL);
	g_list_free (widget->packing_properties);
	widget->packing_properties = NULL;

	if (widget->pack_props_hash)
		g_hash_table_destroy (widget->pack_props_hash);
	widget->pack_props_hash = NULL;

	/* We have to detect whether this is an anarchist child of a composite
	 * widget or not, in otherwords; whether its really a direct child or
	 * a child of a popup window created on the composite widget's behalf.
	 */
	if (widget->anarchist) return;

	widget->packing_properties = glade_widget_create_packing_properties (container, widget);
	widget->pack_props_hash = g_hash_table_new (g_str_hash, g_str_equal);

	/* update the quick reference hash table */
	for (list = widget->packing_properties;
	     list && list->data;
	     list = list->next)
	{
		GladeProperty *property = list->data;
		g_hash_table_insert (widget->pack_props_hash, property->klass->id, property);
	}

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

/*******************************************************************************
 *                           Xml Parsing code                                  *
 *******************************************************************************/
/* XXX Doc me !*/
void
glade_widget_write_special_child_prop (GladeWidget     *parent, 
				       GObject         *object,
				       GladeXmlContext *context,
				       GladeXmlNode    *node)
{
	GladeXmlNode *prop_node, *packing_node;
	gchar        *buff, *special_child_type;

	buff = g_object_get_data (object, "special-child-type");
	g_object_get (parent->adaptor, "special-child-type", &special_child_type, NULL);

	packing_node = glade_xml_search_child (node, GLADE_XML_TAG_PACKING);

	if (special_child_type && buff)
	{
		switch (glade_project_get_format (parent->project))
		{
		case GLADE_PROJECT_FORMAT_LIBGLADE:	
			prop_node = glade_xml_node_new (context, GLADE_XML_TAG_PROPERTY);
			glade_xml_node_append_child (packing_node, prop_node);
			
			/* Name and value */
			glade_xml_node_set_property_string (prop_node, 
							    GLADE_XML_TAG_NAME, 
							    special_child_type);
			glade_xml_set_content (prop_node, buff);
			break;
		case GLADE_PROJECT_FORMAT_GTKBUILDER:
			glade_xml_node_set_property_string (node, 
							    GLADE_XML_TAG_TYPE, 
							    buff);
			break;
		default:
			g_assert_not_reached ();
		}
		
	}
	g_free (special_child_type);
}

/* XXX Doc me ! */
void
glade_widget_set_child_type_from_node (GladeWidget   *parent,
				       GObject       *child,
				       GladeXmlNode  *node)
{
	GladeXmlNode *packing_node, *prop;
	gchar        *special_child_type, *name, *value;

	if (!glade_xml_node_verify (node, GLADE_XML_TAG_CHILD))
		return;

	g_object_get (parent->adaptor, "special-child-type", &special_child_type, NULL);
	if (!special_child_type)
		return;
 
	switch (glade_project_get_format (parent->project))
	{
	case GLADE_PROJECT_FORMAT_LIBGLADE:	
		if ((packing_node = 
		     glade_xml_search_child (node, GLADE_XML_TAG_PACKING)) != NULL)
		{
			for (prop = glade_xml_node_get_children (packing_node); 
			     prop; prop = glade_xml_node_next (prop))
			{
				if (!(name = 
				      glade_xml_get_property_string_required
				      (prop, GLADE_XML_TAG_NAME, NULL)))
					continue;
			
				if (!(value = glade_xml_get_content (prop)))
				{
					/* XXX should be glade_xml_get_content_required()... */
					g_free (name);
					continue;
				}
			
				if (!strcmp (name, special_child_type))
				{
					g_object_set_data_full (child,
								"special-child-type",
								g_strdup (value),
								g_free);
					g_free (name);
					g_free (value);
					break;
				}
				g_free (name);
				g_free (value);
			}
		}
		break;
	case GLADE_PROJECT_FORMAT_GTKBUILDER:
		/* all child types here are depicted by the "type" property */
		if ((value = 
		     glade_xml_get_property_string (node, GLADE_XML_TAG_TYPE)))
		{
			g_object_set_data_full (child,
						"special-child-type",
						value,
						g_free);
		}
		break;
	default:
		g_assert_not_reached ();
	}
	g_free (special_child_type);
}


/**
 * glade_widget_read_child:
 * @widget: A #GladeWidget
 * @node: a #GladeXmlNode
 *
 * Reads in a child widget from the xml (handles 'child' tag)
 */
void
glade_widget_read_child (GladeWidget  *widget,
			 GladeXmlNode *node)
{
	if (glade_project_load_cancelled (widget->project))
		return;

	glade_widget_adaptor_read_child (widget->adaptor, widget, node);
}

/**
 * glade_widget_read:
 * @project: a #GladeProject
 * @parent: The parent #GladeWidget or %NULL
 * @node: a #GladeXmlNode
 *
 * Returns: a new #GladeWidget for @project, based on @node
 */
GladeWidget *
glade_widget_read (GladeProject *project, 
		   GladeWidget  *parent, 
		   GladeXmlNode *node,
		   const gchar  *internal)
{
	GladeWidgetAdaptor *adaptor;
	GladeWidget  *widget = NULL;
	gchar        *klass, *id;

	if (glade_project_load_cancelled (project))
		return NULL;
	
	if (!glade_xml_node_verify
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (project))))
		return NULL;

	glade_widget_push_superuser ();

	if ((klass = 
	     glade_xml_get_property_string_required
	     (node, GLADE_XML_TAG_CLASS, NULL)) != NULL)
	{
		if ((id = 
		     glade_xml_get_property_string_required
		     (node, GLADE_XML_TAG_ID, NULL)) != NULL)
		{
			adaptor = glade_widget_adaptor_get_by_name (klass);
			/* 
			 * Create GladeWidget instance based on type. 
			 */
			if (adaptor &&
			    G_TYPE_IS_INSTANTIATABLE (adaptor->type) &&
			    G_TYPE_IS_ABSTRACT (adaptor->type) == FALSE)
			{

				// Internal children !!!
				if (internal)
				{
					GObject *child_object =
						glade_widget_get_internal_child 
						(parent, internal);
					
					if (!child_object)
					{
						g_warning ("Failed to locate "
							   "internal child %s of %s",
							   internal,
							   glade_widget_get_name (parent));
						goto out;
					}

					if (!(widget = 
					      glade_widget_get_from_gobject (child_object)))
						g_error ("Unable to get GladeWidget "
							 "for internal child %s\n",
							 internal);

					/* Apply internal widget name from here */
					glade_widget_set_name (widget, id);
				} else { 
					widget = glade_widget_adaptor_create_widget
						(adaptor, FALSE,
						 "name", id, 
						 "parent", parent, 
						 "project", project, 
						 "reason", GLADE_CREATE_LOAD, NULL);
				}

				glade_widget_adaptor_read_widget (adaptor,
								  widget,
								  node);
			}
			else
			{
				GObject *stub = g_object_new (GLADE_TYPE_OBJECT_STUB,
				                              "object-type", klass,
				                              "xml-node", node,
				                              NULL);

				widget = glade_widget_adaptor_create_widget (glade_widget_adaptor_get_by_type (GTK_TYPE_HBOX),
				                                             FALSE,
				                                             "parent", parent,
				                                             "project", project,
				                                             "reason", GLADE_CREATE_LOAD,
				                                             "object", stub,
				                                             "name", id,
				                                             NULL);
			}
			g_free (id);
		}
		g_free (klass);
	}

 out:
	glade_widget_pop_superuser ();

	glade_project_push_progress (project);

	return widget;
}


/**
 * glade_widget_write_child:
 * @widget: A #GladeWidget
 * @child: The child #GladeWidget to write
 * @context: A #GladeXmlContext
 * @node: A #GladeXmlNode
 *
 * Writes out a widget to the xml, takes care
 * of packing properties and special child types.
 */
void
glade_widget_write_child (GladeWidget     *widget,
			  GladeWidget     *child,
			  GladeXmlContext *context,
			  GladeXmlNode    *node)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_WIDGET (child));
	g_return_if_fail (child->parent == widget);

	glade_widget_adaptor_write_child (widget->adaptor,
					  child, context, node);
}


/**
 * glade_widget_write_placeholder:
 * @parent: The parent #GladeWidget
 * @object: A #GladePlaceHolder
 * @context: A #GladeXmlContext
 * @node: A #GladeXmlNode
 *
 * Writes out a placeholder to the xml
 */
void
glade_widget_write_placeholder (GladeWidget     *parent,
				GObject         *object,
				GladeXmlContext *context,
				GladeXmlNode    *node)
{
	GladeXmlNode *child_node, *packing_node, *placeholder_node;

	child_node = glade_xml_node_new (context, GLADE_XML_TAG_CHILD);
	glade_xml_node_append_child (node, child_node);

	placeholder_node = glade_xml_node_new (context, GLADE_XML_TAG_PLACEHOLDER);
	glade_xml_node_append_child (child_node, placeholder_node);

	/* maybe write out special-child-type here */
	packing_node = glade_xml_node_new (context, GLADE_XML_TAG_PACKING);
	glade_xml_node_append_child (child_node, packing_node);

	glade_widget_write_special_child_prop (parent, object,
					       context, child_node);

	if (!glade_xml_node_get_children (packing_node))
	{
		glade_xml_node_remove (packing_node);
		glade_xml_node_delete (packing_node);
	}
}

typedef struct {
	GladeXmlContext    *context;
	GladeXmlNode       *node;
	GladeProjectFormat  fmt;
} WriteSignalsInfo;

static void
glade_widget_adaptor_write_signals (gpointer key, 
				    gpointer value, 
				    gpointer user_data)
{
	WriteSignalsInfo *info;
        GPtrArray *signals;
	guint i;

	info = (WriteSignalsInfo *) user_data;
	signals = (GPtrArray *) value;
	for (i = 0; i < signals->len; i++)
	{
		GladeSignal *signal = g_ptr_array_index (signals, i);
		glade_signal_write (signal,
				    info->fmt,
				    info->context,
				    info->node);
	}
}

void 
glade_widget_write_signals (GladeWidget     *widget,
			    GladeXmlContext *context,
			    GladeXmlNode    *node)
{
 	WriteSignalsInfo info;

	info.context = context;
	info.node = node;
	info.fmt = glade_project_get_format (widget->project);

	g_hash_table_foreach (widget->signals,
			      glade_widget_adaptor_write_signals,
			      &info);

}

/**
 * glade_widget_write:
 * @widget: The #GladeWidget
 * @context: A #GladeXmlContext
 * @node: A #GladeXmlNode
 *
 * Recursively writes out @widget and its children
 * and appends the created #GladeXmlNode to @node.
 */
void
glade_widget_write (GladeWidget     *widget,
		    GladeXmlContext *context,
		    GladeXmlNode    *node)
{
	GladeXmlNode *widget_node;
	GList *l, *list;
	GladeProjectFormat fmt = glade_project_get_format (widget->project);

	/* Check if its an unknown object, and use saved xml if so */
	if (GLADE_IS_OBJECT_STUB (widget->object))
	{
		g_object_get (widget->object, "xml-node", &widget_node, NULL);
		glade_xml_node_append_child (node, widget_node);
		return;
	}

	widget_node = glade_xml_node_new (context, GLADE_XML_TAG_WIDGET (fmt));
	glade_xml_node_append_child (node, widget_node);

	/* Set class and id */
	glade_xml_node_set_property_string (widget_node, 
					    GLADE_XML_TAG_CLASS, 
					    widget->adaptor->name);
	glade_xml_node_set_property_string (widget_node, 
					    GLADE_XML_TAG_ID, 
					    widget->name);

	/* Write out widget content (properties and signals) */
	glade_widget_adaptor_write_widget (widget->adaptor, widget, context, widget_node);
		
	/* Write the signals strictly after all properties and before children
	 * when in builder format
	 */
	if (fmt == GLADE_PROJECT_FORMAT_GTKBUILDER)
		glade_widget_write_signals (widget, context, widget_node);

	/* Write the children */
	if ((list =
	     glade_widget_adaptor_get_children (widget->adaptor, widget->object)) != NULL)
	{
		for (l = list; l; l = l->next)
		{
			GladeWidget *child = glade_widget_get_from_gobject (l->data);

			if (child) 
				glade_widget_write_child (widget, child, context, widget_node);
			else if (GLADE_IS_PLACEHOLDER (l->data))
				glade_widget_write_placeholder (widget, 
								G_OBJECT (l->data),
								context, widget_node);
		}
		g_list_free (list);
	}
}


/**
 * gtk_widget_is_ancestor:
 * @widget: a #GladeWidget
 * @ancestor: another #GladeWidget
 *
 * Determines whether @widget is somewhere inside @ancestor, possibly with
 * intermediate containers.
 *
 * Return value: %TRUE if @ancestor contains @widget as a child,
 *    grandchild, great grandchild, etc.
 **/
gboolean
glade_widget_is_ancestor (GladeWidget      *widget,
			  GladeWidget      *ancestor)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GLADE_IS_WIDGET (ancestor), FALSE);

  while (widget)
    {
      if (widget->parent == ancestor)
	return TRUE;
      widget = widget->parent;
    }

  return FALSE;
}

/**
 * glade_widget_depends:
 * @widget: a #GladeWidget
 * @other: another #GladeWidget
 *
 * Determines whether @widget is somehow dependent on @other, in
 * which case it should be serialized after @other.
 *
 * A widget is dependent on another widget.
 * It does not take into account for children dependencies.
 *
 * Return value: %TRUE if @widget depends on @other.
 **/
gboolean
glade_widget_depends (GladeWidget      *widget,
		      GladeWidget      *other)
{
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (GLADE_IS_WIDGET (other), FALSE);

  return glade_widget_adaptor_depends (widget->adaptor, widget, other);
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


/**
 * glade_widget_placeholder_relation:
 * @parent: A #GladeWidget
 * @widget: The child #GladeWidget
 *
 * Returns whether placeholders should be used
 * in operations concerning this parent & child.
 *
 * Currently that criteria is whether @parent is a
 * GtkContainer, @widget is a GtkWidget and the parent
 * adaptor has been marked to use placeholders.
 *
 * Returns: whether to use placeholders for this relationship.
 */
gboolean
glade_widget_placeholder_relation (GladeWidget *parent, 
				   GladeWidget *widget)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (parent), FALSE);
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

	return (GTK_IS_CONTAINER (parent->object) &&
		GTK_IS_WIDGET (widget->object) &&
		GWA_USE_PLACEHOLDERS (parent->adaptor));
}

static GladeWidgetAction *
glade_widget_action_lookup (GList **actions, const gchar *path, gboolean remove)
{
	GList *l;
	
	for (l = *actions; l; l = g_list_next (l))
	{
		GladeWidgetAction *action = l->data;
		
		if (strcmp (action->klass->path, path) == 0)
		{
			if (remove)
			{
				*actions = g_list_remove (*actions, action);
				g_object_unref (action);
				return NULL;
			}
			return action;
		}
		
		if (action->actions &&
		    g_str_has_prefix (path, action->klass->path) &&
		    (action = glade_widget_action_lookup (&action->actions, path, remove)))
			return action;
	}
	
	return NULL;
}

/**
 * glade_widget_get_action:
 * @widget: a #GladeWidget
 * @action_path: a full action path including groups
 *
 * Returns a #GladeWidgetAction object indentified by @action_path.
 *
 * Returns: the action or NULL if not found.
 */
GladeWidgetAction *
glade_widget_get_action (GladeWidget *widget, const gchar *action_path)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	g_return_val_if_fail (action_path != NULL, NULL);
	
	return glade_widget_action_lookup (&widget->actions, action_path, FALSE);
}

/**
 * glade_widget_get_pack_action:
 * @widget: a #GladeWidget
 * @action_path: a full action path including groups
 *
 * Returns a #GladeWidgetAction object indentified by @action_path.
 *
 * Returns: the action or NULL if not found.
 */
GladeWidgetAction *
glade_widget_get_pack_action (GladeWidget *widget, const gchar *action_path)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	g_return_val_if_fail (action_path != NULL, NULL);
	
	return glade_widget_action_lookup (&widget->packing_actions, action_path, FALSE);
}



/**
 * glade_widget_set_action_sensitive:
 * @widget: a #GladeWidget
 * @action_path: a full action path including groups
 * @sensitive: setting sensitive or insensitive
 *
 * Sets the sensitivity of @action_path in @widget
 *
 * Returns: whether @action_path was found or not.
 */
gboolean
glade_widget_set_action_sensitive (GladeWidget *widget,
				   const gchar *action_path,
				   gboolean     sensitive)
{
	GladeWidgetAction *action;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

	if ((action = glade_widget_get_action (widget, action_path)) != NULL)
	{
		glade_widget_action_set_sensitive (action, sensitive);
		return TRUE;
	}
	return FALSE;
}

/**
 * glade_widget_set_pack_action_sensitive:
 * @widget: a #GladeWidget
 * @action_path: a full action path including groups
 * @sensitive: setting sensitive or insensitive
 *
 * Sets the sensitivity of @action_path in @widget
 *
 * Returns: whether @action_path was found or not.
 */
gboolean
glade_widget_set_pack_action_sensitive (GladeWidget *widget,
					const gchar *action_path,
					gboolean     sensitive)
{
	GladeWidgetAction *action;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

	if ((action = glade_widget_get_pack_action (widget, action_path)) != NULL)
	{
		glade_widget_action_set_sensitive (action, sensitive);
		return TRUE;
	}
	return FALSE;
}


/**
 * glade_widget_remove_action:
 * @widget: a #GladeWidget
 * @action_path: a full action path including groups
 *
 * Remove an action.
 */
void
glade_widget_remove_action (GladeWidget *widget, const gchar *action_path)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (action_path != NULL);
	
	glade_widget_action_lookup (&widget->actions, action_path, TRUE);
}

/**
 * glade_widget_remove_pack_action:
 * @widget: a #GladeWidget
 * @action_path: a full action path including groups
 *
 * Remove a packing action.
 */
void
glade_widget_remove_pack_action (GladeWidget *widget, const gchar *action_path)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (action_path != NULL);
	
	glade_widget_action_lookup (&widget->packing_actions, action_path, TRUE);
}

/*******************************************************************************
 *                           Toplevel GladeWidget Embedding                    *
 ******************************************************************************
 *
 * Overrides realize() and size_allocate() by signal connection on GtkWindows.
 *
 * This is high crack code and should be replaced by a more robust implementation
 * in GTK+ proper. 
 *
 */

static GQuark
embedded_window_get_quark ()
{
	static GQuark embedded_window_quark = 0;

	if (embedded_window_quark == 0)
		embedded_window_quark = g_quark_from_string ("GladeEmbedWindow");
	
	return embedded_window_quark;
}

static gboolean
glade_window_is_embedded (GtkWindow *window)
{
	return GPOINTER_TO_INT (g_object_get_qdata ((GObject *) window, embedded_window_get_quark ()));	 
}

static void
embedded_window_realize_handler (GtkWidget *widget)
{
	GtkAllocation allocation;
	GtkStyle *style;
	GdkWindow *window;
	GdkWindowAttr attributes;
	gint attributes_mask;

	gtk_widget_set_realized (widget, TRUE);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);

	gtk_widget_get_allocation (widget, &allocation);
	attributes.x = allocation.x;
	attributes.y = allocation.y;
	attributes.width = allocation.width;
	attributes.height = allocation.height;

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
	window = gtk_widget_get_window (widget);
	if (GDK_IS_WINDOW (window))
	{
		gdk_window_hide (window);
	}

	window = gdk_window_new (gtk_widget_get_parent_window (widget),
				 &attributes, attributes_mask);
	gtk_widget_set_window (widget, window);

	gdk_window_enable_synchronized_configure (window);

	gdk_window_set_user_data (window, GTK_WINDOW (widget));

	gtk_widget_style_attach (widget);
	style = gtk_widget_get_style (widget);
	gtk_style_set_background (style, window, GTK_STATE_NORMAL);
}

static void
embedded_window_size_allocate_handler (GtkWidget *widget)
{
	GtkAllocation allocation;

	if (gtk_widget_get_realized (widget))
	{
		gtk_widget_get_allocation (widget, &allocation);
		gdk_window_move_resize (gtk_widget_get_window (widget),
					allocation.x,
					allocation.y,
					allocation.width,
					allocation.height);
	}
}

/**
 * glade_widget_embed:
 * @window: a #GtkWindow
 *
 * Embeds a window by signal connection method
 */
static gboolean
glade_widget_embed (GladeWidget *gwidget)
{
	GtkWindow *window;
	GtkWidget *widget;
	
	g_return_val_if_fail (GLADE_IS_WIDGET (gwidget), FALSE);
	g_return_val_if_fail (GTK_IS_WINDOW (gwidget->object), FALSE);
	
	window = GTK_WINDOW (gwidget->object);
	widget = GTK_WIDGET (window);
	
	if (glade_window_is_embedded (window)) return TRUE;
	
	if (gtk_widget_get_realized (widget)) gtk_widget_unrealize (widget);

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_TOPLEVEL);
	gtk_container_set_resize_mode (GTK_CONTAINER (window), GTK_RESIZE_PARENT);

	g_signal_connect (window, "realize",
			  G_CALLBACK (embedded_window_realize_handler), NULL);
	g_signal_connect (window, "size-allocate",
			  G_CALLBACK (embedded_window_size_allocate_handler), NULL);

	/* mark window as embedded */
	g_object_set_qdata (G_OBJECT (window), embedded_window_get_quark (),
			    GINT_TO_POINTER (TRUE));
	
	return TRUE;
}



/**
 * glade_widget_create_editor_property:
 * @widget: A #GladeWidget
 * @property: The widget's property
 * @packing: whether @property indicates a packing property or not.
 * @use_command: Whether the undo/redo stack applies here.
 *
 * This is a convenience function to create a GladeEditorProperty corresponding
 * to @property
 *
 * Returns: A newly created and connected GladeEditorProperty
 */
GladeEditorProperty *
glade_widget_create_editor_property (GladeWidget *widget,
				     const gchar *property,
				     gboolean     packing,
				     gboolean     use_command)
{
	GladeEditorProperty *eprop;
	GladeProperty *p;
	
	if (packing)
		p = glade_widget_get_pack_property (widget, property);
	else
		p = glade_widget_get_property (widget, property);

	g_return_val_if_fail (GLADE_IS_PROPERTY (p), NULL);

	eprop = glade_widget_adaptor_create_eprop (widget->adaptor, 
						   p->klass, 
						   use_command);
	glade_editor_property_load (eprop, p);
	
	return eprop;
}

/**
 * glade_widget_generate_path_name:
 * @widget: A #GladeWidget
 *
 * Creates a user friendly name to describe project widgets
 *
 * Returns: A newly allocated string
 */
gchar *
glade_widget_generate_path_name (GladeWidget *widget)
{
	GString     *string;
	GladeWidget *iter;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

	string = g_string_new (widget->name);
	
       	for (iter = widget->parent; iter; iter = iter->parent)
	{
		gchar *str = g_strdup_printf ("%s:", iter->name);
		g_string_prepend (string, str);
		g_free (str);
	}

	return g_string_free (string, FALSE);
}

void
glade_widget_set_support_warning (GladeWidget *widget,
				  const gchar *warning)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	
	if (widget->support_warning)
		g_free (widget->support_warning);
	widget->support_warning = g_strdup (warning);

	g_object_notify (G_OBJECT (widget), "support-warning");
}

/**
 * glade_widget_lock:
 * @widget: A #GladeWidget
 * @locked: The #GladeWidget to lock
 *
 * Sets @locked to be in a locked up state
 * spoken for by @widget, locked widgets cannot
 * be removed from the project until unlocked.
 *
 */
void
glade_widget_lock (GladeWidget      *widget,
		   GladeWidget      *locked)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_WIDGET (locked));
	g_return_if_fail (locked->lock == NULL);

	locked->lock = widget;
	widget->locked_widgets = 
		g_list_prepend (widget->locked_widgets, locked);
}

/**
 * glade_widget_unlock:
 * @widget: A #GladeWidget
 *
 * Unlocks @widget so that it can be removed
 * from the project again
 *
 */
void
glade_widget_unlock (GladeWidget    *widget)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_WIDGET (widget->lock));
	
	widget->lock->locked_widgets = 
		g_list_remove (widget->lock->locked_widgets, widget);
	widget->lock = NULL;
}


/**
 * glade_widget_support_changed:
 * @widget: A #GladeWidget
 *
 * Notifies that support metadata has changed on the widget.
 *
 */
void
glade_widget_support_changed (GladeWidget *widget)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	g_signal_emit (widget, glade_widget_signals[SUPPORT_CHANGED], 0);
}
