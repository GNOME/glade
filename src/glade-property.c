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

#include <stdio.h>
#include <stdlib.h> /* for atoi and atof */
#include <string.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-parameter.h"
#include "glade-project.h"
#include "glade-widget-class.h"
#include "glade-debug.h"

/**
 * glade_property_new:
 * @class:
 * @widget:
 *
 * TODO: write me
 *
 * Returns:
 */
GladeProperty *
glade_property_new (GladePropertyClass *class, GladeWidget *widget, GValue *value)
{
	GladeProperty *property;

	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (class), NULL);

	property          = g_new0 (GladeProperty, 1);
	property->class   = class;
	property->widget  = widget;
	property->value   = value;
	property->enabled = TRUE;

	property->i18n_translatable = FALSE;
	property->i18n_has_context = FALSE;
	property->i18n_comment = NULL;
	
	if (G_IS_PARAM_SPEC_DOUBLE  (class->pspec) ||
	    G_IS_PARAM_SPEC_FLOAT (class->pspec)   ||
	    G_IS_PARAM_SPEC_LONG (class->pspec)    ||
	    G_IS_PARAM_SPEC_ULONG (class->pspec)   ||
	    G_IS_PARAM_SPEC_INT64 (class->pspec)   ||
	    G_IS_PARAM_SPEC_UINT64 (class->pspec)  ||
	    G_IS_PARAM_SPEC_INT (class->pspec)     ||
	    G_IS_PARAM_SPEC_UINT (class->pspec))
		property->enabled = class->optional_default;
	
	/* Create an empty default if the class does not specify a default value */
	if (property->value == NULL)
	{
		if (!class->def)
			property->value =
				glade_property_class_make_gvalue_from_string (class, "");
		else
		{
			property->value = g_new0 (GValue, 1);
			g_value_init (property->value, class->def->g_type);
			g_value_copy (class->def, property->value);
		}
	}
	return property;
}

/**
 * glade_property_dup:
 * @template:
 * @widget:
 *
 * TODO: write me
 *
 * Returns:
 */
GladeProperty *
glade_property_dup (GladeProperty *template, GladeWidget *widget)
{
	GladeProperty *property;

	property = g_new0 (GladeProperty, 1);
	property->class   = template->class;
	property->widget  = widget;
	property->value   = g_new0 (GValue, 1);
	property->enabled = template->enabled;

	g_value_init (property->value, template->value->g_type);
	g_value_copy (template->value, property->value);

	property->i18n_translatable = template->i18n_translatable;
	property->i18n_has_context = template->i18n_has_context;
	property->i18n_comment = g_strdup (template->i18n_comment);
	
	return property;
}

/**
 * glade_property_free:
 * @property: a #GladeProperty
 *
 * Frees @property and its associated memory.
 */
void
glade_property_free (GladeProperty *property)
{
	property->class = NULL;
	property->widget = NULL;
	if (property->value)
	{
		g_value_unset (property->value);
		g_free (property->value);
		property->value = NULL;
	}

	g_free (property->i18n_comment);
	
	g_free (property);
}

/**
 * Generic set function for properties that do not have a
 * custom set_property method. This includes packing properties.
 */
static void
glade_property_set_property (GladeProperty *property, const GValue *value)
{
	if (property->class->packing)
	{
		GladeWidget          *parent = glade_widget_get_parent (property->widget);
		GladeWidget          *child  = property->widget;
		glade_widget_class_container_set_property (parent->widget_class,
							   parent->object, child->object,
							   property->class->id, value);
	}
	else
	{
		GObject *gobject = G_OBJECT (glade_widget_get_object (property->widget));
		g_object_set_property (gobject, property->class->id, value);
	}
}

/**
 * glade_property_set:
 * @property: a #GladeProperty
 * @value: a #GValue
 *
 * TODO: write me
 */
void
glade_property_set (GladeProperty *property, const GValue *value)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));
	g_return_if_fail (value != NULL);
	
	if (!g_value_type_compatible (G_VALUE_TYPE (property->value), G_VALUE_TYPE (value)))
	{
		g_warning ("Trying to assign an incompatible value to property %s\n",
			    property->class->id);
		return;
	}

	if (property->class->verify_function)
	{
		GObject *object = glade_widget_get_object (property->widget);
		if (property->class->verify_function (object, value) == FALSE)
			return;
	}
	
	/* Assign property first so that; if the object need be
	 * rebuilt, it will reflect the new value
	 */
	g_value_reset (property->value);
	g_value_copy (value, property->value);

	glade_property_sync(property);
}

/**
 * glade_property_sync:
 * @property: a #GladeProperty
 *
 * TODO: write me
 */
void
glade_property_sync (GladeProperty *property)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));

	if (!property->enabled)
		return;

	if (property->loading)
		return;

	property->loading = TRUE;

	if (property->class->set_function)
		/* if there is a custom set_property, use it */
		(*property->class->set_function)
			(glade_widget_get_object (property->widget), property->value);
	else if (property->class->construct_only)
	{
		/* In the case of construct_only, the widget must be rebuilt, here we
		 * take care of updating the project if the widget is in a project and
		 * updating the selection if the widget was selected.
		 */
		GList    *selection;
		gboolean  reselect  = FALSE;
		gboolean  inproject =
			property->widget->project ?
			(glade_project_get_widget_by_name
			 (property->widget->project,
			  property->widget->name) ? TRUE : FALSE) : FALSE;

		
		if (inproject)
		{
			if ((selection =
			     glade_project_selection_get (property->widget->project)) != NULL &&
			    g_list_find(selection,
					glade_widget_get_object (property->widget)) != NULL)
			{
				reselect = TRUE;
				glade_project_selection_remove
					(property->widget->project,
					 glade_widget_get_object (property->widget), FALSE);
			}
			glade_project_remove_object (property->widget->project,
						     glade_widget_get_object (property->widget));
		}

		glade_widget_rebuild (property->widget);

		if (inproject)
		{
			glade_project_add_object (property->widget->project,
						  glade_widget_get_object (property->widget));
			if (reselect)
				glade_project_selection_add
					(property->widget->project,
					 glade_widget_get_object (property->widget), TRUE);
		}
	}
	else
		glade_property_set_property (property, property->value);

	property->loading = FALSE;
}

/**
 * glade_property_write:
 * @props: a GArray of #GladePropInfo
 * @property: a #GladeProperty
 * @interface: a #GladeInterface
 *
 * TODO: write me
 *
 * Returns:
 */
gboolean
glade_property_write (GArray *props, GladeProperty *property, GladeInterface *interface)
{
	GladePropInfo info = { 0 };
	gchar *tmp;
	gchar *default_str = NULL;
	gboolean skip;

	if (!property->enabled)
		return FALSE;

	if (!glade_property_class_is_visible (property->class,
					      glade_widget_get_class (property->widget)))
		return FALSE;

	/* we should change each '-' by '_' on the name of the property 
         * (<property name="...">) */
	tmp = g_strdup (property->class->id);
	if (!tmp)
		return FALSE;
	glade_util_replace (tmp, '-', '_');

	/* put the name="..." part on the <property ...> tag */
	info.name = alloc_propname(interface, tmp);
 	/*if (state->translate_prop && state->content->str[0] != '\0') {
	    if (state->context_prop) 
		info.value = alloc_string(state->interface,
					  g_strip_context(state->content->str,
							   dgettext(state->domain, state->content->str)));
	    else
		info.value = alloc_string(state->interface,
					  dgettext(state->domain, state->content->str));
 	} else {
 	    info.value = alloc_string(state->interface, state->content->str);
  	}*/
	g_free (tmp);

	/* convert the value of this property to a string, and put it between
	 * the opening and the closing of the property tag */
	tmp = glade_property_class_make_string_from_gvalue (property->class,
							    property->value);
	if (!tmp)
		return FALSE;

	if (property->class->def)
	{
		if (property->class->orig_def == NULL)
		{
			default_str =
				glade_property_class_make_string_from_gvalue (property->class,
									      property->class->def);
			skip = default_str && !strcmp (tmp, default_str);
			g_free (default_str);
		}
		else
		{
			skip = G_IS_PARAM_SPEC_STRING (property->class->pspec) &&
			       tmp[0] == '\0' &&
			       !g_value_get_string (property->class->orig_def);
		}

		if (skip)
		{
			g_free (tmp);
			return FALSE;
		}
	}

	if (property->class->translatable)
	{
		/* FIXME: implement writing the i18n metadata. */
	}
	
	info.value = alloc_string(interface, tmp);
	g_array_append_val (props, info);
	g_free (tmp);

	return TRUE;
}

/* Parameters for translatable properties. */

void
glade_property_i18n_set_comment (GladeProperty *property,
				 const gchar    *str)
{
	if (property->i18n_comment)
		g_free (property->i18n_comment);

	property->i18n_comment = g_strdup (str);
}

const gchar *
glade_property_i18n_get_comment (GladeProperty *property)
{
	return property->i18n_comment;
}

void
glade_property_i18n_set_translatable (GladeProperty *property,
				      gboolean       translatable)
{
	property->i18n_translatable = translatable;
}

gboolean
glade_property_i18n_get_translatable (GladeProperty *property)
{
	return property->i18n_translatable;
}

void
glade_property_i18n_set_has_context (GladeProperty *property,
				     gboolean       has_context)
{
	property->i18n_has_context = has_context;
}

gboolean
glade_property_i18n_get_has_context (GladeProperty *property)
{
	return property->i18n_has_context;
}

