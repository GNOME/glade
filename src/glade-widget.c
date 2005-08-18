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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib-object.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n-lib.h>
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
#include "glade-editor.h"
#include "glade-app.h"

static void glade_widget_class_init			(GladeWidgetKlass *klass);
static void glade_widget_init				(GladeWidget *widget);
static void glade_widget_finalize			(GObject *object);
static void glade_widget_dispose			(GObject *object);
static void glade_widget_set_real_property		(GObject *object,
							 guint    prop_id,
							 const GValue *value,
							 GParamSpec *pspec);
static void glade_widget_get_real_property		(GObject *object,
							 guint prop_id,
							 GValue *value,
							 GParamSpec *pspec);
static void glade_widget_set_properties                 (GladeWidget *widget,
							 GList *properties);
static void glade_widget_set_class			(GladeWidget *widget,
							 GladeWidgetClass *klass);
static void glade_widget_real_add_signal_handler	(GladeWidget *widget,
							 GladeSignal *signal_handler);
static void glade_widget_real_remove_signal_handler	(GladeWidget *widget,
							 GladeSignal *signal_handler);
static void glade_widget_real_change_signal_handler	(GladeWidget *widget,
							 GladeSignal *old_signal_handler,
							 GladeSignal *new_signal_handler);
static void glade_widget_set_packing_properties		(GladeWidget *widget,
							 GladeWidget *container);

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
	PROP_OBJECT,
	PROP_CLASS,
	PROP_PROJECT,
	PROP_PROPERTIES,
	PROP_PARENT
};

static guint glade_widget_signals[LAST_SIGNAL] = {0};
static GObjectClass *parent_class = NULL;

/**
 * glade_widget_get_type:
 *
 * Creates the typecode for the #GladeWidget object type.
 *
 * Returns: the typecode for the #GladeWidget object type
 */
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
		(object_class, PROP_OBJECT,
		 g_param_spec_object ("object", _("Object"),
				      _("The object associated"),
				      G_TYPE_OBJECT,
				      G_PARAM_READWRITE |
				      G_PARAM_CONSTRUCT));

	g_object_class_install_property
		(object_class, PROP_CLASS,
		   g_param_spec_pointer ("class", _("Class"),
					 _("The class of the associated"
					   " gtk+ widget"),
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
				       G_PARAM_CONSTRUCT_ONLY));
	
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
			      glade_marshal_VOID__POINTER_POINTER,
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
	widget->widget_class = NULL;
	widget->project = NULL;
	widget->name = NULL;
	widget->internal = NULL;
	widget->object = NULL;
	widget->properties = NULL;
	widget->packing_properties = NULL;
	widget->signals = g_hash_table_new_full
		(g_str_hash, g_str_equal,
		 (GDestroyNotify) g_free,
		 (GDestroyNotify) free_signals);
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

void
glade_widget_debug (GladeWidget *widget)
{
	glade_widget_debug_real (widget, 0);
}

static void
glade_widget_copy_packing_props (GladeWidget *parent,
				 GladeWidget *child,
				 GladeWidget *template)
{
	GList *l;

	glade_widget_set_packing_properties (child, parent);

	for (l = child->packing_properties; l && l->data; l = l->next)
	{
		GladeProperty *dup_prop  = GLADE_PROPERTY(l->data);
		GladeProperty *orig_prop =
			glade_widget_get_property (template, dup_prop->class->id);
		glade_property_set_value (dup_prop, orig_prop->value);
	}
}

static void
glade_widget_sync_custom_props (GladeWidget *widget)
{
	GList *l;
	for (l = widget->properties; l && l->data; l = l->next)
	{
		GladeProperty *prop  = GLADE_PROPERTY(l->data);
		if (prop->class->set_function)
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

static void
glade_widget_copy_props (GladeWidget *widget,
			 GladeWidget *template)
{
	GList *l;
	for (l = widget->properties; l && l->data; l = l->next)
	{
		GladeProperty *dup_prop  = GLADE_PROPERTY(l->data);
		GladeProperty *orig_prop =
			glade_widget_get_property (template, dup_prop->class->id);
		glade_property_set_value (dup_prop, orig_prop->value);
	}
}

static GList *
glade_widget_dup_properties (GList *template_props)
{
	GList *list, *properties = NULL;

	for (list = template_props; list && list->data; list = list->next)
	{
		GladeProperty *prop = list->data;
		properties = g_list_prepend (properties, glade_property_dup (prop, NULL));
	}
	return g_list_reverse (properties);
}

static void
glade_widget_fill_all_empty (GladeWidget *widget)
{
	GList *children, *list;

	if ((children =
	     glade_widget_class_container_get_children (widget->widget_class,
							widget->object)) != NULL)
	{
		for (list = children; list && list->data; list = list->next)
		{
			GladeWidget *child_widget =
				glade_widget_get_from_gobject (G_OBJECT (list->data));
			if (child_widget)
				glade_widget_fill_all_empty (child_widget);
		}
	}
	glade_widget_class_container_fill_empty (widget->widget_class, widget->object);
}

/**
 * glade_widget_build_object:
 * @klass: a #GladeWidgetClass
 * @widget: a #GladeWidget
 * 
 * This function creates a new GObject who's parameters are based
 * on the GType of the GladeWidgetClass and its default values, if a
 * GladeWidget is specified, it will be used to apply the values currently in use.
 *
 * Returns: A newly created GObject
 */
static GObject *
glade_widget_build_object (GladeWidgetClass *klass, GladeWidget *widget)
{
	GArray              *params;
	GObjectClass        *oclass;
	GParamSpec         **pspec;
	GladeProperty       *glade_property;
	GladePropertyClass  *glade_property_class;
	GObject             *object;
	guint                n_props, i;

	/* As a slight optimization, we never unref the class
	 */
	oclass = g_type_class_ref (klass->type);
	pspec  = g_object_class_list_properties (oclass, &n_props);
	params = g_array_new (FALSE, FALSE, sizeof (GParameter));

	for (i = 0; i < n_props; i++)
	{
		GParameter parameter = { 0, };

		glade_property_class =
			glade_widget_class_get_property_class (klass,
							       pspec[i]->name);
		if (!glade_property_class)
			/* Ignore properties that are not accounted for
			 * by the GladeWidgetClass
			 */
			continue;
		
		parameter.name = pspec[i]->name; /* No need to dup this */
		g_value_init (&parameter.value, pspec[i]->value_type);

		/* If a widget is specified and has a value set for that
		 * property, then that value will be used (otherwise, we
		 * use the default value)
		 */
		if (widget &&
		    (glade_property =
		     glade_widget_get_property (widget, parameter.name)) != NULL)
		{
			if (g_value_type_compatible (G_VALUE_TYPE (glade_property->value),
						     G_VALUE_TYPE (&parameter.value)))
				g_value_copy (glade_property->value, &parameter.value);
			else
			{
				g_critical ("Type mismatch on %s property of %s",
					    parameter.name, klass->name);
				continue;
			}
		}
		/* If the class has a default, use it.
		 */
		else if (glade_property_class->def != NULL)
		{
			if (g_value_type_compatible (G_VALUE_TYPE (glade_property_class->def),
						     G_VALUE_TYPE (&parameter.value)))
				g_value_copy (glade_property_class->def, &parameter.value);
			else
			{
				g_critical ("Type mismatch on %s property of %s",
					    parameter.name, klass->name);
				continue;
			}
		}
		else
			g_param_value_set_default (pspec[i], &parameter.value);

		g_array_append_val (params, parameter);
	}
	g_free (pspec);


	/* Create the new object with the correct parameters.
	 */
	object = g_object_newv (klass->type, params->len,
				(GParameter *)params->data);

	/* Cleanup parameters
	 */
	for (i = 0; i < params->len; i++)
	{
		GParameter parameter = g_array_index (params, GParameter, i);
		g_value_unset (&parameter.value);
	}
	g_array_free (params, TRUE);

	return object;
}

static void
glade_widget_set_default_packing_properties (GladeWidget *container,
					     GladeWidget *child)
{
	GladeSupportedChild *support;

	support = glade_widget_class_get_child_support (container->widget_class,
							child->widget_class->type);

	if (support) {
		GladePropertyClass *property_class;
		GList              *l;

		for (l = support->properties; l; l = l->next) 
		{
			GladePackingDefault *def;
			GValue             *value;

			property_class = l->data;

			def = glade_widget_class_get_packing_default (child->widget_class, 
								      container->widget_class, 
								      property_class->id);
			
			if (!def)
				continue;

			/* Check value type */
			value = glade_property_class_make_gvalue_from_string (property_class,
									      def->value);

			glade_widget_class_container_set_property (container->widget_class,
								   container->object,
								   child->object,
								   property_class->id,
								   value);

			g_value_unset (value);
			g_free (value);
		}
	}
}

static GladeWidget *
glade_widget_internal_new (const gchar      *name,
			   GladeWidget      *parent,
			   GladeWidgetClass *klass,
			   GladeProject     *project,
			   GladeWidget      *template)
{
	GObject *object;
	GObject *glade_widget;
	GList   *properties = NULL;

	object = glade_widget_build_object(klass, template);
	if (template)
		properties = glade_widget_dup_properties (template->properties);

	glade_widget = g_object_new (GLADE_TYPE_WIDGET,
				     "parent",      parent,
				     "properties",  properties,
				     "class",       klass,
				     "project",     project,
				     "name",        name,
				     "object",      object,
				     NULL);

	return GLADE_WIDGET (glade_widget);
}

/**
 * glade_widget_new:
 * @klass: a #GladeWidgetClass
 * @project: a #GladeProject
 *
 * TODO: write me
 *
 * Returns:
 */
GladeWidget *
glade_widget_new (GladeWidget *parent, GladeWidgetClass *klass, GladeProject *project)
{
	GladeWidget *widget;
	gchar       *widget_name =
		glade_project_new_widget_name
		(GLADE_PROJECT (project), klass->generic_name);

	if ((widget = glade_widget_internal_new
	     (widget_name, parent, klass, project, NULL)) != NULL)
	{
		glade_widget_class_container_fill_empty (klass, widget->object);

		if (widget->query_user)
		{
			GladeEditor *editor = glade_default_app_get_editor ();

			/* If user pressed cancel on query popup. */
			if (!glade_editor_query_dialog (editor, widget))
			{
				g_object_unref (G_OBJECT (widget));
				return NULL;
			}
		}

		/* Properties that have custom set_functions on them need to be
		 * explicitly synchronized.
		 */
		glade_widget_sync_custom_props (widget);
	}
	g_free (widget_name);
	return widget;
}

/**
 * glade_widget_dup_internal:
 * @widget: a #GladeWidget
 *
 * TODO: write me
 *
 * Returns:
 */
static GladeWidget *
glade_widget_dup_internal (GladeWidget *parent, GladeWidget *template)
{
	GladeWidget *gwidget;
	GList       *list, *children;
	
	g_return_val_if_fail (template != NULL && GLADE_IS_WIDGET(template), NULL);
	gwidget = glade_widget_internal_new (glade_widget_get_name(template),
					     parent,
					     template->widget_class,
					     template->project,
					     template);

	if ((children =
	     glade_widget_class_container_get_all_children (template->widget_class,
							    template->object)) != NULL)
	{
		for (list = children; list && list->data; list = list->next)
		{
			GObject     *child = G_OBJECT (list->data);
			GladeWidget *child_gwidget, *child_dup;

			if ((child_gwidget = glade_widget_get_from_gobject (child)) == NULL)
				continue;

			if (child_gwidget->internal == NULL)
			{
				child_dup = glade_widget_dup_internal (gwidget, child_gwidget);

				glade_widget_class_container_add (gwidget->widget_class,
								  gwidget->object,
								  child_dup->object);

				glade_widget_copy_packing_props (gwidget,
								 child_dup,
								 child_gwidget);
			}
			else if (gwidget->widget_class->get_internal_child)
			{
				GObject *internal_object = NULL;

				gwidget->widget_class->get_internal_child
					(gwidget->object,
					 child_gwidget->internal,
					 &internal_object);

				if ((child_dup = glade_widget_get_from_gobject
				     (internal_object)) != NULL)
				{
					glade_widget_copy_props (child_dup,
								 child_gwidget);

					/* If custom properties are still at thier
					 * default value, they need to be synced.
					 */
					glade_widget_sync_custom_props (child_dup);
				}
			}
		}
		g_list_free (children);
	}

	glade_widget_sync_custom_props (gwidget);

	if (GTK_IS_WIDGET (gwidget->object) && !GTK_WIDGET_TOPLEVEL(gwidget->object))
		gtk_widget_show_all (GTK_WIDGET (gwidget->object));
	
	return gwidget;
}

GladeWidget *
glade_widget_dup (GladeWidget *template)
{
	GladeWidget *widget = glade_widget_dup_internal (NULL, template);
	if (widget)
		glade_widget_fill_all_empty (widget);
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
	GObject          *new_object, *old_object;
	GladeWidgetClass *klass = glade_widget->widget_class;

	/* Hold a reference to the old widget while we transport properties
	 * and children from it
	 */
	new_object = glade_widget_build_object(klass, glade_widget);
	old_object = g_object_ref(glade_widget_get_object(glade_widget));

	glade_widget_set_object(glade_widget, new_object);

	glade_widget_class_container_fill_empty (klass, new_object);
	
	/* Custom properties aren't transfered in build_object, since build_object
	 * is only concerned with object creation.
	 */
	glade_widget_sync_custom_props  (glade_widget);

	/* Sync packing.
	 */
	glade_widget_sync_packing_props (glade_widget);
	
	if (g_type_is_a (klass->type, GTK_TYPE_WIDGET))
	{
		/* Must use gtk_widget_destroy here for cases like dialogs and toplevels
		 * (otherwise I'd prefer g_object_unref() )
		 */
		gtk_widget_destroy  (GTK_WIDGET(old_object));
		gtk_widget_show_all (GTK_WIDGET(new_object));
	}
	else
		g_object_unref (old_object);
}


/**
 * glade_widget_new_for_internal_child:
 * @klass:
 * @parent:
 * @internal_widget:
 * @internal_name:
 *
 * TODO: write me
 *
 * Returns:
 */
GladeWidget *
glade_widget_new_for_internal_child (GladeWidgetClass *klass,
				     GladeWidget      *parent,
				     GObject          *internal_object,
				     const gchar      *internal_name)
{
	GladeProject *project      = glade_widget_get_project (parent);
	gchar        *widget_name  = glade_project_new_widget_name (project, klass->generic_name);
	GladeWidget  *widget       = g_object_new (GLADE_TYPE_WIDGET,
						   "parent", parent,
						   "class", klass,
						   "project", project,
						   "name", widget_name,
						   "internal", internal_name,
						   "object", internal_object, NULL);
	g_free (widget_name);
	return widget;
}

static void
glade_widget_finalize (GObject *object)
{
	GladeWidget *widget = GLADE_WIDGET (object);

	g_return_if_fail (GLADE_IS_WIDGET (object));

	g_free (widget->name);
	g_free (widget->internal);
	g_hash_table_destroy (widget->signals);

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
	
	G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void
glade_widget_dispose (GObject *object)
{
	GladeWidget *widget = GLADE_WIDGET (object);

	g_return_if_fail (GLADE_IS_WIDGET (object));

	if (widget->project)
		widget->project =
			(g_object_unref (G_OBJECT (widget->project)), NULL);

	if (widget->object)
		widget->object =
			(g_object_unref (widget->object), NULL);

 	if (G_OBJECT_CLASS(parent_class)->dispose)
		G_OBJECT_CLASS(parent_class)->dispose(object);
}

void
glade_widget_show (GladeWidget *widget)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	/* Position window at saved coordinates or in the center */
	if (GTK_IS_WINDOW (widget->object))
	{
		if (widget->pos_saved)
			gtk_window_move (GTK_WINDOW (widget->object), 
					 widget->save_x, widget->save_y);
		else
			gtk_window_set_position (GTK_WINDOW (widget->object), 
						 GTK_WIN_POS_CENTER);
		gtk_window_present (GTK_WINDOW (widget->object));
	} else {
		gtk_widget_show (GTK_WIDGET (widget->object));
	}
}

void
glade_widget_hide (GladeWidget *widget)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	if (GTK_IS_WINDOW (widget->object))
	{
		/* Save coordinates */
		gtk_window_get_position (GTK_WINDOW (widget->object),
					 &(widget->save_x), &(widget->save_y));
		widget->pos_saved = TRUE;
		gtk_widget_hide (GTK_WIDGET (widget->object));
	}
}


/**
 * glade_widget_add_signal_handler:
 * @widget:
 * @signal_handler:
 *
 * TODO: write me
 */
void
glade_widget_add_signal_handler	(GladeWidget *widget, GladeSignal *signal_handler)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	g_signal_emit (widget, glade_widget_signals[ADD_SIGNAL_HANDLER], 0, signal_handler);
}

/**
 * glade_widget_remove_signal_handler:
 * @widget:
 * @signal_handler:
 *
 * TODO: write me
 */
void
glade_widget_remove_signal_handler (GladeWidget *widget, GladeSignal *signal_handler)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	g_signal_emit (widget, glade_widget_signals[REMOVE_SIGNAL_HANDLER], 0, signal_handler);
}

/**
 * glade_widget_change_signal_handler:
 * @widget:
 * @old_signal_handler:
 * @new_signal_handler:
 *
 * TODO: write me
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
	case PROP_OBJECT:
		glade_widget_set_object (widget, g_value_get_object (value));
		break;
	case PROP_PROJECT:
		glade_widget_set_project (widget, GLADE_PROJECT (g_value_get_object (value)));
		break;
	case PROP_CLASS:
		glade_widget_set_class (widget, GLADE_WIDGET_CLASS
					(g_value_get_pointer (value)));
		break;
	case PROP_PROPERTIES:
		glade_widget_set_properties (widget, (GList *)g_value_get_pointer (value));
		break;
	case PROP_PARENT:
		glade_widget_set_parent (widget, GLADE_WIDGET (g_value_get_object (value)));
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
 * @widget:
 * @internal:
 *
 * TODO: write me
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
 * TODO: write me
 *
 * Returns: 
 */
const gchar *
glade_widget_get_internal (GladeWidget *widget)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	return widget->internal;
}

static void
glade_widget_set_properties (GladeWidget *widget, GList *properties)
{
	GList *list;

	/* "properties" has to be specified before "class" on the g_object_new line.
	 */
	if (widget->properties == NULL)
	{
		widget->properties = properties;
		for (list = properties; list; list = list->next)
		{
			GladeProperty *property = list->data;
			property->widget        = (gpointer)widget;
			if (property->class->query)
			{
				widget->query_user = TRUE;
			}
		}
	}
}

static void
glade_widget_set_class (GladeWidget *widget, GladeWidgetClass *klass)
{
	GladePropertyClass *property_class;
	GladeProperty *property;
	GList *list;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_WIDGET_CLASS (klass));
	/* calling set_class out of the constructor? */
	g_return_if_fail (widget->widget_class == NULL);
	
	widget->widget_class = klass;

	if (!widget->properties)
	{
		for (list = klass->properties; list; list = list->next)
		{
			property_class = GLADE_PROPERTY_CLASS(list->data);
			property = glade_property_new (property_class, 
						       widget, NULL);
			if (!property) {
				g_warning ("Failed to create [%s] property",
					   property_class->id);
				continue;
			}

			widget->properties = g_list_prepend (widget->properties, property);
			if (property_class->query) widget->query_user = TRUE;

		}
		widget->properties = g_list_reverse (widget->properties);
	}
}

/**
 * glade_widget_get_class:
 * @widget: a #GladeWidget
 *
 * Returns: the #GladeWidgetclass of @widget
 */
GladeWidgetClass *
glade_widget_get_class (GladeWidget *widget)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	return widget->widget_class;
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
		if (project)
			g_object_ref (project);
		if (widget->project)
			g_object_unref (widget->project);
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
	GList *list;
	GladeProperty *property;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	g_return_val_if_fail (id_property != NULL, NULL);

	for (list = widget->properties; list; list = list->next) {
		property = list->data;
		if (strcmp (property->class->id, id_property) == 0)
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
	GList *list;
	GladeProperty *property;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	g_return_val_if_fail (id_property != NULL, NULL);


	for (list = widget->packing_properties; list; list = list->next) {
		property = list->data;
		if (strcmp (property->class->id, id_property) == 0)
			return property;
	}
	g_critical ("Unable to find property %s on widget %s of class %s",
		    id_property, widget->name, widget->widget_class->name);
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


static gboolean
glade_widget_popup_menu (GtkWidget *widget, gpointer unused_data)
{
	GladeWidget *glade_widget;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

	glade_widget = glade_widget_get_from_gobject (widget);
	glade_popup_widget_pop (glade_widget, NULL, TRUE);

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
	    (glade_widget_get_from_gobject (widget)) && GTK_WIDGET_MAPPED(widget))
		data->found = widget;
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

/**
 * glade_widget_retrieve_from_position:
 * @base: a #GtkWidget 
 * @x: an int
 * @y: an int
 * 
 * Returns: the real widget that was "clicked over" for a given event 
 * (coordinates) and a widget. 
 * For example, when a label is clicked the button press event is triggered 
 * for its parent, this function takes the event and the widget that got the 
 * event and returns the real #GladeWidget that was clicked
 */
GladeWidget *
glade_widget_retrieve_from_position (GtkWidget *base, int x, int y)
{
	GtkWidget *toplevel_widget;
	gint top_x;
	gint top_y;
	
	toplevel_widget = gtk_widget_get_toplevel (base);
	if (!GTK_WIDGET_TOPLEVEL (toplevel_widget))
		return NULL;

	gtk_widget_translate_coordinates (base, toplevel_widget, x, y, &top_x, &top_y);
	return glade_widget_find_deepest_child_at_position
		(GTK_CONTAINER (toplevel_widget),
		 GTK_CONTAINER (toplevel_widget), top_x, top_y);
}

static gboolean
glade_widget_button_press (GtkWidget *widget,
			   GdkEventButton *event,
			   gpointer unused_data)
{
	GladeWidget       *glade_widget;
	gint               x = (gint) (event->x + 0.5);
	gint               y = (gint) (event->y + 0.5);
	gboolean           handled = FALSE;

	glade_widget = glade_widget_retrieve_from_position (widget, x, y);
	widget = GTK_WIDGET (glade_widget_get_object (glade_widget));

	/* make sure to grab focus, since we may stop default handlers */
	if (GTK_WIDGET_CAN_FOCUS (widget) && !GTK_WIDGET_HAS_FOCUS (widget))
		gtk_widget_grab_focus (widget);

	/* notify manager */
	if (glade_widget->manager != NULL) 
		glade_fixed_manager_post_mouse (glade_widget->manager, x, y);

	/* if it's already selected don't stop default handlers, e.g. toggle button */
	if (event->button == 1)
	{
		if (event->state & GDK_CONTROL_MASK)
		{
			if (glade_project_is_selected (glade_widget->project,
						       glade_widget->object))
				glade_default_app_selection_remove 
					(glade_widget->object, TRUE);
			else
				glade_default_app_selection_add
					(glade_widget->object, TRUE);
			handled = TRUE;
		}
		else if (glade_project_is_selected (glade_widget->project,
						    glade_widget->object) == FALSE)
		{
			glade_util_clear_selection ();
			glade_default_app_selection_set 
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
glade_widget_key_press (GtkWidget *event_widget,
			GdkEventKey *event,
			gpointer unused_data)
{
	GladeWidget *glade_widget;

	g_return_val_if_fail (GTK_IS_WIDGET (event_widget), FALSE);

	glade_widget = glade_widget_get_from_gobject (event_widget);

	/* We will delete all the selected items */
	if (event->keyval == GDK_Delete)
	{
		glade_util_delete_selection ();
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



/* Connects a signal handler to the 'event' signal for a widget and
   all its children recursively. We need this to draw the selection
   rectangles and to get button press/release events reliably. */
static void
glade_widget_connect_signal_handlers (GtkWidget *widget_gtk, gpointer data)
{
	/* Don't connect handlers for placeholders. */
	if (GLADE_IS_PLACEHOLDER (widget_gtk))
		return;
	
	/* Check if we've already connected an event handler. */
	if (!g_object_get_data (G_OBJECT (widget_gtk),
				GLADE_TAG_EVENT_HANDLER_CONNECTED)) {
		g_signal_connect (G_OBJECT (widget_gtk), "event",
				  G_CALLBACK (glade_widget_event), NULL);

		g_object_set_data (G_OBJECT (widget_gtk),
				   GLADE_TAG_EVENT_HANDLER_CONNECTED,
				   GINT_TO_POINTER (1));
	}

	/* We also need to get expose events for any children.
	 * Note that we are only interested in children returned through the
	 * standard GtkContainer interface, as this is stricktly Gtk+ stuff.
	 * (hence the absence of glade_widget_class_container_get_children () )
	 */
	if (GTK_IS_CONTAINER (widget_gtk)) {
		gtk_container_forall (GTK_CONTAINER (widget_gtk), 
				      glade_widget_connect_signal_handlers,
				      NULL);
	}
}

/**
 * glade_widget_transport_children:
 * @gwidget: A #GladeWidget
 * @from_container: A #GObject
 * @to_container: A #GObject
 * 
 * Transports all children from @from_container to @to_container
 *
 */
static void
glade_widget_transport_children (GladeWidget  *gwidget,
				 GObject      *from_container,
				 GObject      *to_container)
{
	GList *list, *children;

	if ((children = glade_widget_class_container_get_children (gwidget->widget_class,
								   from_container)) != NULL)
	{
		for (list = children; list && list->data; list = list->next)
		{
			GObject     *child  = G_OBJECT(list->data);
			GladeWidget *gchild = glade_widget_get_from_gobject (child);
			/* If this widget is a container, all children get a temporary
			 * reference and are moved from the old container, to the new
			 * container and thier child properties are applied.
			 */
			g_object_ref(child);
			glade_widget_class_container_remove (gwidget->widget_class,
							     from_container, child);
			glade_widget_class_container_add    (gwidget->widget_class,
							     to_container, child);
			glade_widget_sync_packing_props     (gchild);

			g_object_unref(child);
		}
		g_list_free (children);
	}
}

/**
 * glade_widget_set_object:
 * @gwidget:
 * @new_widget:
 *
 * TODO: write me
 */
void
glade_widget_set_object (GladeWidget *gwidget, GObject *new_object)
{
	GladeWidgetClass *klass;
	GObject          *old_object;
	
	g_return_if_fail (GLADE_IS_WIDGET (gwidget));
	g_return_if_fail (G_IS_OBJECT     (new_object));
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (new_object),
				       gwidget->widget_class->type));

	klass      = gwidget->widget_class;
	old_object = gwidget->object;
	
	/* Add internal reference to new widget */
	gwidget->object = g_object_ref (G_OBJECT(new_object));
	g_object_set_data (G_OBJECT (new_object), "GladeWidgetDataTag", gwidget);

	if (gwidget->internal == NULL)
	{
		/* Call custom notification of widget creation in plugin */
		if (klass->post_create_function)
			klass->post_create_function (G_OBJECT(new_object));

		if (g_type_is_a (gwidget->widget_class->type, GTK_TYPE_WIDGET))
		{
			/* Take care of events and toolkit signals.
			 */
			gtk_widget_add_events (GTK_WIDGET(new_object),
					       GDK_BUTTON_PRESS_MASK   |
					       GDK_BUTTON_RELEASE_MASK |
					       GDK_KEY_PRESS_MASK);

			if (GTK_WIDGET_TOPLEVEL (new_object))
				g_signal_connect (G_OBJECT (new_object), "delete_event",
						  G_CALLBACK (glade_widget_hide_on_delete), NULL);
			g_signal_connect (G_OBJECT (new_object), "popup_menu",
					  G_CALLBACK (glade_widget_popup_menu), NULL);
			g_signal_connect (G_OBJECT (new_object), "key_press_event",
					  G_CALLBACK (glade_widget_key_press), NULL);

			glade_widget_connect_signal_handlers (GTK_WIDGET(new_object), NULL);
		}
		
		/* Take care of hierarchy here
		 */
		if (old_object)
		{
			if (gwidget->parent)
			{
				glade_widget_class_container_replace_child
					(gwidget->parent->widget_class,
					 gwidget->parent->object,
					 old_object, new_object);
			}
			glade_widget_transport_children (gwidget, old_object, new_object);
		}
	}


	/* Remove internal reference to old widget */
	if (old_object) {
		g_object_set_data (G_OBJECT (old_object), "GladeWidgetDataTag", NULL);
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
glade_widget_real_change_signal_handler (GladeWidget *widget,
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

GPtrArray *
glade_widget_list_signal_handlers (GladeWidget *widget,
				   const gchar *signal_name) /* array of GladeSignal* */
{
	return g_hash_table_lookup (widget->signals, signal_name);
}

GladeWidget *
glade_widget_get_parent (GladeWidget *widget)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	return widget->parent;
}

void
glade_widget_set_parent (GladeWidget *widget,
			 GladeWidget *parent)
{
	GladeWidget *old_parent = widget->parent;
	widget->parent = parent;

	if (widget->object &&
	    (old_parent == NULL || widget->packing_properties == NULL ||
	     old_parent->widget_class->type != parent->widget_class->type))
		glade_widget_set_packing_properties (widget, parent);
	else
		glade_widget_sync_packing_props (widget);
	
	g_object_notify (G_OBJECT (widget), "parent");
}

/**
 * Returns a list of GladeProperties from a list for the correct
 * child type for this widget of this container.
 */
static GList *
glade_widget_create_packing_properties (GladeWidget *container, GladeWidget *widget)
{
	GladeSupportedChild  *support;
	GladePropertyClass   *property_class;
	GladeProperty        *property;
	GList                *list, *packing_props = NULL;
	
	if ((support =
	     glade_widget_class_get_child_support
	     (container->widget_class, widget->widget_class->type)) != NULL)
	{
		for (list = support->properties; list && list->data; list = list->next)
		{
			property_class = list->data;
			property       = glade_property_new (property_class, widget, NULL);
			packing_props  = g_list_prepend (packing_props, property);
		}
	}
	return g_list_reverse (packing_props);
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
	GList                *list;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_WIDGET (container));

	g_list_foreach (widget->packing_properties, (GFunc)g_object_unref, NULL);
	g_list_free (widget->packing_properties);

	glade_widget_set_default_packing_properties (container, widget);
	
	widget->packing_properties = glade_widget_create_packing_properties (container, widget);

	/* update the values of the properties to the ones we get from gtk */
	for (list = widget->packing_properties;
	     list && list->data;
	     list = list->next)
	{
		GladeProperty *property = list->data;
		g_value_reset (property->value);
		glade_widget_class_container_get_property (container->widget_class,
							   container->object,
							   widget->object,
							   property->class->id,
							   property->value);
	}
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
	GladeWidget *gnew_widget = NULL;
	GladeWidget *gold_widget = NULL;

	g_return_if_fail (G_IS_OBJECT (old_object));
	g_return_if_fail (G_IS_OBJECT (new_object));

	gnew_widget = glade_widget_get_from_gobject (new_object);
	gold_widget = glade_widget_get_from_gobject (old_object);

	if (gnew_widget) gnew_widget->parent = parent;
	if (gold_widget) gold_widget->parent = NULL;

	glade_widget_class_container_replace_child 
		(parent->widget_class, parent->object,
		 old_object, new_object);

	if (gnew_widget) 
		glade_widget_set_packing_properties (gnew_widget, parent);
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
	GArray *props, *children;
	GList *list;

	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

	info = g_new0 (GladeWidgetInfo, 1);

	info->classname = alloc_string (interface, widget->widget_class->name);
	info->name = alloc_string (interface, widget->name);

	/* Write the properties */
	list = widget->properties;
	props = g_array_new (FALSE, FALSE,
			     sizeof (GladePropInfo));
	for (; list; list = list->next)
	{
		GladeProperty *property = list->data;

		if (property->class->packing)
			continue;

		glade_property_write (property, interface, props);
	}
	info->properties = (GladePropInfo *) props->data;
	info->n_properties = props->len;
	g_array_free(props, FALSE);

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
	     glade_widget_class_container_get_all_children (widget->widget_class,
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
	gchar                *buff;
	GladeSupportedChild  *support;

	support = glade_widget_class_get_child_support (parent->widget_class, G_OBJECT_TYPE (object));
	buff    = g_object_get_data (object, "special-child-type");

	if (support && support->special_child_type && buff)
	{
		info.name  = alloc_propname (interface, support->special_child_type);
		info.value = alloc_string (interface, buff);
		g_array_append_val (props, info);
		return TRUE;
	}
	return FALSE;
}

gboolean
glade_widget_write_child (GArray      *children, 
			  GladeWidget *parent, 
			  GObject     *object,
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
		info.internal_child = alloc_string(interface, child_widget->internal);

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
			g_assert (property->class->packing == TRUE);
			glade_property_write (property, interface, props);
		}
	}

	info.properties = (GladePropInfo *) props->data;
	info.n_properties = props->len;
	g_array_free(props, FALSE);
	
	g_array_append_val (children, info);

	return TRUE;
}

static GValue *
glade_widget_value_from_prop_info (GladePropInfo    *info,
				   GladeWidgetClass *class)
{
	GladePropertyClass  *pclass;
	GValue              *gvalue = NULL;
	gchar               *id;

	g_return_val_if_fail (info   != NULL, NULL);

	id = g_strdup (info->name);
	glade_util_replace (id, '_', '-');

	if (info->name && info->value &&
	    (pclass = glade_widget_class_get_property_class (class, id)) != NULL)
		gvalue = glade_property_class_make_gvalue_from_string (pclass,
								       info->value);
		
	g_free (id);

	return gvalue;
}

static gboolean
glade_widget_apply_property_from_prop_info (GladePropInfo *info,
					    GladeWidget   *widget,
					    gboolean	   is_packing)
{
	GladeProperty    *property;
	GladeWidgetClass *wclass;
	GValue           *gvalue;
	gchar            *id;

	g_return_val_if_fail (info != NULL, FALSE);
	g_return_val_if_fail (widget != NULL, FALSE);

	id = g_strdup (info->name);

	glade_util_replace (id, '_', '-');
	property = !is_packing ? glade_widget_get_property (widget, id):
				 glade_widget_get_pack_property (widget, id);
	g_free (id);

	if (!property)
	{
		return FALSE;
	}

	wclass = property->class->packing ?
		widget->parent->widget_class :
		widget->widget_class;
	
	if ((gvalue = glade_widget_value_from_prop_info (info, wclass)) != NULL)
	{
		glade_property_set_value (property, gvalue);

		g_value_unset (gvalue);
		g_free (gvalue);
	}
	return TRUE;
}



static gboolean
glade_widget_new_child_from_child_info (GladeChildInfo *info,
					GladeProject   *project,
					GladeWidget    *parent);

static void
glade_widget_fill_from_widget_info (GladeWidgetInfo *info,
				    GladeWidget     *widget,
				    gboolean         apply_props)
{
	guint i;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (info != NULL);

	g_assert (strcmp (info->classname, widget->widget_class->name) == 0);

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
		for (i = 0; i < info->n_properties; ++i)
		{
			if (!glade_widget_apply_property_from_prop_info (info->properties + i,
									 widget, FALSE))
				g_warning ("Failed to apply property");
		}
	}
}

static GValue *
glade_widget_get_property_from_widget_info (GladeWidgetClass  *class,
					    GladeWidgetInfo   *info,
					    const gchar       *name,
					    gboolean          *translatable,
					    gboolean          *has_context,
					    gchar            **comment)
{
	GValue *value = NULL;
	guint   i;
	gchar  *id;

	for (i = 0; i < info->n_properties; ++i)
	{
		GladePropInfo *pinfo = info->properties + i;

		id = g_strdup (pinfo->name);
		glade_util_replace (id, '_', '-');

		if (!strcmp (id, name))
		{
			g_free (id);

			/* FIXME: Waiting for a solution in libglade for i18n
			 * metadata.
			 */
			
			if (translatable)
				*translatable = FALSE; /*pinfo->translatable;*/
			if (has_context)
				*has_context = FALSE; /*pinfo->has_context;*/
			if (comment)
				*comment = NULL; /*g_strdup (pinfo->comment);*/
			
			return glade_widget_value_from_prop_info (pinfo, class);
		}
		g_free (id);
	}
	return value;
}

static void
glade_widget_params_free (GArray *params)
{
	guint i;
	for (i = 0; i < params->len; i++)
	{
		GParameter parameter = g_array_index (params, GParameter, i);
		g_value_unset (&parameter.value);
	}
	g_array_free (params, TRUE);
}

static GList *
glade_widget_properties_from_widget_info (GladeWidgetClass *class,
					  GladeWidgetInfo  *info)
{
	GList  *properties = NULL, *list;
	
	for (list = class->properties; list && list->data; list = list->next)
	{
		GladePropertyClass *pclass = list->data;
		GValue             *value;
		GladeProperty      *property;
		gboolean            translatable;
		gboolean            has_context;
		gchar              *comment = NULL;

		/* If there is a value in the XML, initialize property with it,
		 * otherwise initialize property to default.
		 */
		value = glade_widget_get_property_from_widget_info (class,
								    info,
								    pclass->id,
								    &translatable,
								    &has_context,
								    &comment);
		
		property          = glade_property_new (pclass, NULL, value);
		property->enabled = value ? TRUE : property->enabled;

		if (value) {
			glade_property_i18n_set_translatable (property, translatable);
			glade_property_i18n_set_has_context (property, has_context);
			glade_property_i18n_set_comment (property, comment);
		}

		g_free (comment);

		properties = g_list_prepend (properties, property);
	}
	return g_list_reverse (properties);
}

static GArray *
glade_widget_params_from_widget_info (GladeWidgetClass *widget_class,
				      GladeWidgetInfo  *info)
{
	GladePropertyClass   *glade_property_class;
	GObjectClass         *oclass;
	GParamSpec          **pspec;
	GArray               *params;
	gint                  i, n_props;
	
	oclass = g_type_class_ref (widget_class->type);
	pspec  = g_object_class_list_properties (oclass, &n_props);
	params = g_array_new (FALSE, FALSE, sizeof (GParameter));

	/* prepare parameters that have glade_property_class->def */
	for (i = 0; i < n_props; i++)
	{
		GParameter  parameter = { 0, };
		GValue     *value;
		
		glade_property_class =
		     glade_widget_class_get_property_class (widget_class,
							    pspec[i]->name);
		if (!glade_property_class || glade_property_class->set_function)
			continue;
		
		parameter.name = pspec[i]->name;
		g_value_init (&parameter.value, pspec[i]->value_type);

		/* Try filling parameter with value from widget info.
		 */
		if ((value = glade_widget_get_property_from_widget_info
		     (widget_class, info, parameter.name,
		      NULL, NULL, NULL)) != NULL)
		{
			if (g_value_type_compatible (G_VALUE_TYPE (value),
						     G_VALUE_TYPE (&parameter.value)))
			{
				g_value_copy (value, &parameter.value);
				g_value_unset (value);
				g_free (value);
			}
			else
			{
				g_critical ("Type mismatch on %s property of %s",
					    parameter.name, widget_class->name);
				g_value_unset (value);
				g_free (value);
				continue;
			}
		}
		/* Now try filling the parameter with the default on the GladeWidgetClass.
		 */
		else if (g_value_type_compatible (G_VALUE_TYPE (glade_property_class->def),
						  G_VALUE_TYPE (&parameter.value)))
			g_value_copy (glade_property_class->def, &parameter.value);
		else
		{
			g_critical ("Type mismatch on %s property of %s",
				    parameter.name, widget_class->name);
			continue;
		}

		g_array_append_val (params, parameter);
	}
	g_free(pspec);

	g_type_class_unref (oclass);

	return params;
}

static GladeWidget *
glade_widget_new_from_widget_info (GladeWidgetInfo *info,
                                   GladeProject    *project,
                                   GladeWidget     *parent)
{
	GladeWidgetClass *klass;
	GladeWidget *widget;
	GObject     *object;
	GArray      *params;
	GList       *properties;
	
	g_return_val_if_fail (info != NULL, NULL);
	g_return_val_if_fail (project != NULL, NULL);

	klass = glade_widget_class_get_by_name (info->classname);
	if (!klass)
	{
		g_warning ("Widget class %s unknown.", info->classname);
		return NULL;
	}
	
	params     = glade_widget_params_from_widget_info (klass, info);
	properties = glade_widget_properties_from_widget_info (klass, info);
	
	object = g_object_newv (klass->type, params->len,
				(GParameter *)params->data);

	glade_widget_params_free (params);

	widget = g_object_new (GLADE_TYPE_WIDGET,
			       "parent",     parent,
			       "properties", properties,
			       "class",      klass,
			       "project",    project,
			       "name",       info->name,
			       "object",     object, NULL);

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

	glade_widget_class_container_fill_empty (klass, object);

	if (GTK_IS_WIDGET (object) && !GTK_WIDGET_TOPLEVEL (object))
		gtk_widget_show_all (GTK_WIDGET (object));

	return widget;
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
		if (parent->widget_class->get_internal_child)
		{
			GObject *object;
			parent->widget_class->get_internal_child (parent->object,
								  internal,
								  &object);
			return object;
		}
		parent = glade_widget_get_parent (parent);
	}
	return NULL;
}

static gint
glade_widget_set_child_type_from_child_info (GladeChildInfo *child_info,
					     GladeWidgetClass *parent_class,
					     GObject *child)
{
	guint                i;
	GladePropInfo        *prop_info;
	GladeSupportedChild  *support;

	support = glade_widget_class_get_child_support (parent_class,
							G_OBJECT_TYPE (child));
	if (!support || !support->special_child_type)
		return -1;

	for (i = 0; i < child_info->n_properties; ++i)
	{
		prop_info = child_info->properties + i;
		if (!strcmp (prop_info->name, support->special_child_type))
		{
			g_object_set_data_full (child,
						"special-child-type",
						g_strdup (prop_info->value),
						g_free);
			return i;
		}
	}
	return -1;
}

static gboolean
glade_widget_new_child_from_child_info (GladeChildInfo *info,
					GladeProject *project,
					GladeWidget *parent)
{
	GladeWidget    *child;
        gint            i, special_child_type_index = -1;

	g_return_val_if_fail (info != NULL, FALSE);
	g_return_val_if_fail (project != NULL, FALSE);

	/* is it a placeholder? */
	if (!info->child)
	{
		GObject *palaceholder = G_OBJECT (glade_placeholder_new ());
		glade_widget_set_child_type_from_child_info 
			(info, parent->widget_class, palaceholder);
		glade_widget_class_container_add (parent->widget_class,
				   		  parent->object,
						  palaceholder);
		return TRUE;
	}

	/* is it an internal child? */
	if (info->internal_child)
	{
		GObject *child_object =
			glade_widget_get_internal_child (parent, info->internal_child);
		GladeWidgetClass *child_class;

		if (!child_object)
                {
			g_warning ("Failed to locate internal child %s of %s",
				   info->internal_child, glade_widget_get_name (parent));
			return FALSE;
		}
		child_class = glade_widget_class_get_by_name (G_OBJECT_TYPE_NAME (child_object));
		child       = glade_widget_new_for_internal_child (child_class,
								   parent,
								   child_object,
								   info->internal_child);

		glade_widget_fill_from_widget_info (info->child, child, TRUE);
		glade_widget_sync_custom_props (child);
	}
        else
        {
		child = glade_widget_new_from_widget_info (info->child, project, parent);
		if (!child)
			return FALSE;

		child->parent = parent;

		special_child_type_index =
			glade_widget_set_child_type_from_child_info (info,
								     parent->widget_class,
								     child->object);
		
		if (parent->manager != NULL) 
			glade_fixed_manager_add_child (parent->manager, child, FALSE);
		else
			glade_widget_class_container_add (parent->widget_class,
							  parent->object, child->object);

		glade_widget_sync_packing_props (child);
	}

	/* Get the packing properties */
	for (i = 0; i < info->n_properties; ++i)
	{
		if (special_child_type_index == i)
			continue;
		if (!glade_widget_apply_property_from_prop_info (info->properties + i,
								 child, TRUE))
			g_warning ("Failed to apply packing property");
	}

	return TRUE;
}


/**
 * glade_widget_read:
 * @project: a #GladeProject
 * @info: a #GladeWidgetInfo
 *
 * Returns: a new #GladeWidget in @project, based off the contents of @node
 */
GladeWidget *
glade_widget_read (GladeProject *project, GladeWidgetInfo *info)
{
	GladeWidget *widget;
		
	if ((widget = glade_widget_new_from_widget_info
	     (info, project, NULL)) != NULL)
	{
		if (glade_verbose)
			glade_widget_debug (widget);
	}	
	return widget;
}
