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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Chema Celorio <chema@celorio.com>
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h> /* for atoi and atof */
#include <string.h>

#include <glib/gi18n-lib.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-parameter.h"
#include "glade-project.h"
#include "glade-widget-class.h"
#include "glade-debug.h"
#include "glade-app.h"
#include "glade-editor.h"
#include "glade-marshallers.h"

enum
{
	VALUE_CHANGED,
	TOOLTIP_CHANGED,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_ENABLED,
	PROP_SENSITIVE,
	PROP_I18N_TRANSLATABLE,
	PROP_I18N_HAS_CONTEXT,
	PROP_I18N_COMMENT
};

static guint         glade_property_signals[LAST_SIGNAL] = { 0 };
static GObjectClass* parent_class = NULL;

/*******************************************************************************
                           GladeProperty class methods
 *******************************************************************************/
static GladeProperty *
glade_property_dup_impl (GladeProperty *template, GladeWidget *widget)
{
	GladeProperty *property;

	property          = g_object_new (GLADE_TYPE_PROPERTY, 
					  "enabled", template->enabled,
					  "sensitive", template->sensitive,
					  "i18n-translatable", template->i18n_translatable,
					  "i18n-has-context", template->i18n_has_context,
					  "i18n-comment", template->i18n_comment,
					  NULL);
	property->class   = template->class;
	property->widget  = widget;
	property->value   = g_new0 (GValue, 1);

	property->insensitive_tooltip =
		template->insensitive_tooltip ?
		g_strdup (template->insensitive_tooltip) : NULL;

	g_value_init (property->value, template->value->g_type);
	g_value_copy (template->value, property->value);

	property->i18n_translatable = template->i18n_translatable;
	
	return property;
}

void
glade_property_reset_impl (GladeProperty *property)
{
	GLADE_PROPERTY_GET_KLASS (property)->set_value
		(property, property->class->def);
}

gboolean
glade_property_default_impl (GladeProperty *property)
{
	return !g_param_values_cmp (property->class->pspec,
				    property->value,
				    property->class->def);
}

gboolean
glade_property_equals_value_impl (GladeProperty *property,
				  const GValue  *value)
{
	return !g_param_values_cmp (property->class->pspec,
				    property->value, value);
}

/*
 * Generic set function for properties that do not have a
 * custom set_property method. This includes packing properties.
 */
static void
glade_property_set_property (GladeProperty *property, const GValue *value)
{
	if (property->class->packing)
	{
		GladeWidget  *parent = glade_widget_get_parent (property->widget);
		GladeWidget  *child  = property->widget;
		glade_widget_class_container_set_property 
			(parent->widget_class, parent->object, child->object,
			 property->class->id, value);
	}
	else
	{
		GObject *gobject = G_OBJECT (glade_widget_get_object (property->widget));
		g_object_set_property (gobject, property->class->id, value);
	}
}

static void
glade_property_set_value_impl (GladeProperty *property, const GValue *value)
{
	GladeProject *project;
	gboolean      changed = FALSE;

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
	
	/* save "changed" state.
	 */
	changed = g_param_values_cmp (property->class->pspec, 
				      property->value, value) != 0;


	/* Add/Remove references from widget ref stacks here
	 */
	if (changed && glade_property_class_is_object (property->class))
	{
		GladeWidget *gold, *gnew;
		GObject     *old_object, *new_object;

		if ((old_object = g_value_get_object (property->value)) != NULL)
		{
			gold = glade_widget_get_from_gobject (old_object);
			glade_widget_remove_prop_ref (gold, property);
		}

		if ((new_object = g_value_get_object (value)) != NULL)
		{
			gnew = glade_widget_get_from_gobject (new_object);
			glade_widget_add_prop_ref (gnew, property);
		}
	}

	/* Assign property first so that; if the object need be
	 * rebuilt, it will reflect the new value
	 */
	g_value_reset (property->value);
	g_value_copy (value, property->value);

	GLADE_PROPERTY_GET_KLASS (property)->sync (property);

	if (changed)
	{
		if ((project = glade_widget_get_project (property->widget)) != NULL)
			glade_project_changed (project);

		g_signal_emit (G_OBJECT (property),
			       glade_property_signals[VALUE_CHANGED],
			       0, property->value);
	}
}

static void
glade_property_get_value_impl (GladeProperty *property, GValue *value)
{

	g_value_init (value, property->class->pspec->value_type);
	g_value_copy (property->value, value);
}

static void
glade_property_get_default_impl (GladeProperty *property, GValue *value)
{

	g_value_init (value, property->class->pspec->value_type);
	g_value_copy (property->class->def, value);
}

static void
glade_property_sync_impl (GladeProperty *property)
{
	if (property->enabled == FALSE || 
	    property->class->ignore    ||
	    property->syncing)
		return;

	property->syncing = TRUE;

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
			     glade_project_selection_get
			     (property->widget->project)) != NULL &&
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
			glade_project_add_object (property->widget->project, NULL,
						  glade_widget_get_object (property->widget));
			if (reselect)
				glade_project_selection_add
					(property->widget->project,
					 glade_widget_get_object (property->widget), TRUE);
		}
	}
	else
		glade_property_set_property (property, property->value);

	property->syncing = FALSE;
}

static gboolean
glade_property_write_impl (GladeProperty  *property, 
			   GladeInterface *interface,
			   GArray         *props)
{
	GladePropInfo info = { 0 };
	gchar *tmp;
	gchar *default_str = NULL;
	gboolean skip;

	if (!property->class->save || !property->enabled)
		return FALSE;

	/* we should change each '-' by '_' on the name of the property 
         * (<property name="...">) */
	tmp = g_strdup (property->class->id);
	if (!tmp)
		return FALSE;
	glade_util_replace (tmp, '-', '_');

	/* put the name="..." part on the <property ...> tag */
	info.name = alloc_propname(interface, tmp);
	g_free (tmp);

	/* convert the value of this property to a string, and put it between
	 * the opening and the closing of the property tag */
	tmp = glade_property_class_make_string_from_gvalue (property->class,
							    property->value);
	if (!tmp || !tmp[0])
		return FALSE;

	if (property->class->orig_def == NULL)
	{
		/* Skip custom properties that are NULL string types. */
		skip = G_IS_PARAM_SPEC_STRING (property->class->pspec) &&
			!g_value_get_string (property->value);
	}
	else
	{
		/* Skip properties that are default
		 * (by original pspec default) 
		 */
		default_str =
			glade_property_class_make_string_from_gvalue
			(property->class, property->class->orig_def);
		skip = default_str && !strcmp (tmp, default_str);
		g_free (default_str);
	}
	
	if (skip)
	{
		g_free (tmp);
		return FALSE;
	}

	if (property->class->translatable)
	{
		info.translatable = property->i18n_translatable;
		info.has_context  = property->i18n_has_context;
		if (property->i18n_comment)
			info.comment = alloc_string
				(interface, property->i18n_comment);
	}
	
	info.value = alloc_string(interface, tmp);
	g_array_append_val (props, info);
	g_free (tmp);

	return TRUE;
}


static G_CONST_RETURN gchar *
glade_property_get_tooltip_impl (GladeProperty *property)
{
	gchar *tooltip = NULL;
	if (property->sensitive == FALSE)
		tooltip = property->insensitive_tooltip;
	else
		tooltip = property->class->tooltip;
	return tooltip;
}

/*******************************************************************************
                      GObjectClass & Object Construction
 *******************************************************************************/
static void
glade_property_set_real_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
	GladeProperty *property = GLADE_PROPERTY (object);

	switch (prop_id)
	{
	case PROP_ENABLED:
		glade_property_set_enabled (property, g_value_get_boolean (value));
		break;
	case PROP_SENSITIVE:
		property->sensitive = g_value_get_boolean (value);
		break;
	case PROP_I18N_TRANSLATABLE:
		glade_property_i18n_set_translatable (property, g_value_get_boolean (value));
		break;
	case PROP_I18N_HAS_CONTEXT:
		glade_property_i18n_set_has_context (property, g_value_get_boolean (value));
		break;
	case PROP_I18N_COMMENT:
		glade_property_i18n_set_comment (property, g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_property_get_real_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
	GladeProperty *property = GLADE_PROPERTY (object);

	switch (prop_id)
	{
	case PROP_ENABLED:
		g_value_set_boolean (value, glade_property_get_enabled (property));
		break;
	case PROP_SENSITIVE:
		g_value_set_boolean (value, glade_property_get_sensitive (property));
		break;
	case PROP_I18N_TRANSLATABLE:
		g_value_set_boolean (value, glade_property_i18n_get_translatable (property));
		break;
	case PROP_I18N_HAS_CONTEXT:
		g_value_set_boolean (value, glade_property_i18n_get_has_context (property));
		break;
	case PROP_I18N_COMMENT:
		g_value_set_string (value, glade_property_i18n_get_comment (property));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_property_finalize (GObject *object)
{
	GladeProperty *property = GLADE_PROPERTY (object);

	if (property->value)
	{
		g_value_unset (property->value);
		g_free (property->value);
	}
	if (property->i18n_comment)
		g_free (property->i18n_comment);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
glade_property_init (GladeProperty *property)
{
	property->enabled   = TRUE;
	property->sensitive = TRUE;
	property->i18n_translatable = TRUE;
	property->i18n_has_context = FALSE;
	property->i18n_comment = NULL;
}

static void
glade_property_klass_init (GladePropertyKlass *prop_class)
{
	GObjectClass       *object_class;
	g_return_if_fail (prop_class != NULL);
	
	parent_class = g_type_class_peek_parent (prop_class);
	object_class = G_OBJECT_CLASS (prop_class);

	/* GObjectClass */
	object_class->set_property = glade_property_set_real_property;
	object_class->get_property = glade_property_get_real_property;
	object_class->finalize  = glade_property_finalize;

	/* Class methods */
	prop_class->dup                   = glade_property_dup_impl;
	prop_class->reset                 = glade_property_reset_impl;
	prop_class->def                   = glade_property_default_impl;
	prop_class->equals_value          = glade_property_equals_value_impl;
	prop_class->set_value             = glade_property_set_value_impl;
	prop_class->get_value             = glade_property_get_value_impl;
	prop_class->get_default           = glade_property_get_default_impl;
	prop_class->sync                  = glade_property_sync_impl;
	prop_class->write                 = glade_property_write_impl;
	prop_class->get_tooltip           = glade_property_get_tooltip_impl;
	prop_class->value_changed         = NULL;
	prop_class->tooltip_changed       = NULL;

	/* Properties */
	g_object_class_install_property 
		(object_class, PROP_ENABLED,
		 g_param_spec_boolean 
		 ("enabled", _("Enabled"), 
		  _("If the property is optional, this is its enabled state"),
		  TRUE, G_PARAM_READWRITE));

	g_object_class_install_property 
		(object_class, PROP_SENSITIVE,
		 g_param_spec_boolean 
		 ("sensitive", _("Sensitive"), 
		  _("This gives backends control to set property sensitivity"),
		  TRUE, G_PARAM_READWRITE));

	g_object_class_install_property 
		(object_class, PROP_I18N_COMMENT,
		 g_param_spec_string 
		 ("i18n-comment", _("Comment"), 
		  _("XXX FIXME: The translators comment ?"),
		  NULL, G_PARAM_READWRITE));

	g_object_class_install_property 
		(object_class, PROP_I18N_TRANSLATABLE,
		 g_param_spec_boolean 
		 ("i18n-translatable", _("Translatable"), 
		  _("Whether this property is translatable or not"),
		  TRUE, G_PARAM_READWRITE));

	g_object_class_install_property 
		(object_class, PROP_I18N_HAS_CONTEXT,
		 g_param_spec_boolean 
		 ("i18n-has-context", _("Has Context"), 
		  _("Whether this property is translatable or not"),
		  TRUE, G_PARAM_READWRITE));

	/* Signals */
	glade_property_signals[VALUE_CHANGED] =
		g_signal_new ("value-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladePropertyKlass,
					       value_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

	glade_property_signals[TOOLTIP_CHANGED] =
		g_signal_new ("tooltip-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladePropertyKlass,
					       tooltip_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);


}


GType
glade_property_get_type (void)
{
	static GType property_type = 0;
	
	if (!property_type)
	{
		static const GTypeInfo property_info = 
		{
			sizeof (GladePropertyKlass), // Klass is our class
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) glade_property_klass_init,
			(GClassFinalizeFunc) NULL,
			NULL,           /* class_data */
			sizeof (GladeProperty),
			0,              /* n_preallocs */
			(GInstanceInitFunc) glade_property_init,
		};
		property_type = 
			g_type_register_static (G_TYPE_OBJECT,
						"GladeProperty",
						&property_info, 0);
	}
	return property_type;
}

/*******************************************************************************
                                     API
 *******************************************************************************/
/**
 * glade_property_new:
 * @class: A #GladeWidgetClass
 * @widget: A #GladeWidget
 * @value: A #GValue
 *
 *
 * Returns: A newly created #GladeProperty based on the
 *          given criteria
 */
GladeProperty *
glade_property_new (GladePropertyClass *class, 
		    GladeWidget        *widget, 
		    GValue             *value)
{
	GladeProperty *property;

	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (class), NULL);

	property            = 
		(GladeProperty *)g_object_new (GLADE_TYPE_PROPERTY, NULL);
	property->class     = class;
	property->widget    = widget;
	property->value     = value;
	
	if (G_IS_PARAM_SPEC_DOUBLE  (class->pspec) ||
	    G_IS_PARAM_SPEC_FLOAT (class->pspec)   ||
	    G_IS_PARAM_SPEC_LONG (class->pspec)    ||
	    G_IS_PARAM_SPEC_ULONG (class->pspec)   ||
	    G_IS_PARAM_SPEC_INT64 (class->pspec)   ||
	    G_IS_PARAM_SPEC_UINT64 (class->pspec)  ||
	    G_IS_PARAM_SPEC_INT (class->pspec)     ||
	    G_IS_PARAM_SPEC_UINT (class->pspec))
		property->enabled = class->optional_default;
	
	if (property->value == NULL)
	{
		g_assert (class->def);
		property->value = g_new0 (GValue, 1);
		g_value_init (property->value, class->def->g_type);
		g_value_copy (class->def, property->value);
	}
	return property;
}

/**
 * glade_property_dup:
 * @template: A #GladeProperty
 * @widget: A #GladeWidget
 *
 * Returns: A newly duplicated property based on the new widget
 */
GladeProperty *
glade_property_dup (GladeProperty *template, GladeWidget *widget)
{
	g_return_val_if_fail (GLADE_IS_PROPERTY (template), NULL);
	return GLADE_PROPERTY_GET_KLASS (template)->dup (template, widget);
}

/**
 * glade_property_reset:
 * @property: A #GladeProperty
 *
 * Resets this property to its default value
 */
void
glade_property_reset (GladeProperty *property)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));
	GLADE_PROPERTY_GET_KLASS (property)->reset (property);
}

/**
 * glade_property_default:
 * @property: A #GladeProperty
 *
 * Returns: Whether this property is at its default value
 */
gboolean
glade_property_default (GladeProperty *property)
{
	g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);
	return GLADE_PROPERTY_GET_KLASS (property)->def (property);
}

/**
 * glade_property_equals_value:
 * @property: a #GladeProperty
 * @value: a #GValue
 *
 * Returns: Whether this property is equal to the value provided
 */
gboolean
glade_property_equals_value (GladeProperty      *property, 
			     GValue             *value)
{
	g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);
	return GLADE_PROPERTY_GET_KLASS (property)->equals_value (property, value);
}

/**
 * glade_property_equals_va_list:
 * @property: a #GladeProperty
 * @vl: a va_list
 *
 * Returns: Whether this property is equal to the value provided
 */
gboolean
glade_property_equals_va_list (GladeProperty *property, va_list vl)
{
	GValue   *value;
	gboolean  ret;

	g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);

	value = glade_property_class_make_gvalue_from_vl (property->class, vl);

	ret = GLADE_PROPERTY_GET_KLASS (property)->equals_value (property, value);
	
	g_value_unset (value);
	g_free (value);
	return ret;
}

/**
 * glade_property_equals:
 * @property: a #GladeProperty
 * @...: a provided property value
 *
 * Returns: Whether this property is equal to the value provided
 */
gboolean
glade_property_equals (GladeProperty *property, ...)
{
	va_list  vl;
	gboolean ret;

	g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);

	va_start (vl, property);
	ret = glade_property_equals_va_list (property, vl);
	va_end (vl);

	return ret;
}

/**
 * glade_property_set_value:
 * @property: a #GladeProperty
 * @value: a #GValue
 *
 * Sets the property's value
 */
void
glade_property_set_value (GladeProperty *property, const GValue *value)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));
	g_return_if_fail (value != NULL);
	GLADE_PROPERTY_GET_KLASS (property)->set_value (property, value);
}

/**
 * glade_property_set_va_list:
 * @property: a #GladeProperty
 * @vl: a va_list with value to set
 *
 * Sets the property's value
 */
void
glade_property_set_va_list (GladeProperty *property, va_list vl)
{
	GValue  *value;

	g_return_if_fail (GLADE_IS_PROPERTY (property));

	value = glade_property_class_make_gvalue_from_vl (property->class, vl);

	GLADE_PROPERTY_GET_KLASS (property)->set_value (property, value);
	
	g_value_unset (value);
	g_free (value);
}

/**
 * glade_property_set:
 * @property: a #GladeProperty
 * @...: the value to set
 *
 * Sets the property's value (in a convenient way)
 */
void
glade_property_set (GladeProperty *property, ...)
{
	va_list  vl;

	g_return_if_fail (GLADE_IS_PROPERTY (property));

	va_start (vl, property);
	glade_property_set_va_list (property, vl);
	va_end (vl);
}

/**
 * glade_property_get_value:
 * @property: a #GladeProperty
 * @value: a #GValue
 *
 * Retrieve the property value
 */
void
glade_property_get_value (GladeProperty *property, GValue *value)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));
	g_return_if_fail (value != NULL);
	GLADE_PROPERTY_GET_KLASS (property)->get_value (property, value);
}

/**
 * glade_property_get_default:
 * @property: a #GladeProperty
 * @value: a #GValue
 *
 * Retrieve the default property value
 */
void
glade_property_get_default (GladeProperty *property, GValue *value)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));
	g_return_if_fail (value != NULL);
	GLADE_PROPERTY_GET_KLASS (property)->get_default (property, value);
}

/**
 * glade_property_get_va_list:
 * @property: a #GladeProperty
 * @vl: a va_list
 *
 * Retrieve the property value
 */
void
glade_property_get_va_list (GladeProperty *property, va_list vl)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));
	glade_property_class_set_vl_from_gvalue (property->class, property->value, vl);
}

/**
 * glade_property_get:
 * @property: a #GladeProperty
 * @...: An address to store the value
 *
 * Retrieve the property value
 */
void
glade_property_get (GladeProperty *property, ...)
{
	va_list  vl;

	g_return_if_fail (GLADE_IS_PROPERTY (property));

	va_start (vl, property);
	glade_property_get_va_list (property, vl);
	va_end (vl);
}

/**
 * glade_property_sync:
 * @property: a #GladeProperty
 *
 * Synchronize the object with this property
 */
void
glade_property_sync (GladeProperty *property)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));
	GLADE_PROPERTY_GET_KLASS (property)->sync (property);
}

/**
 * glade_property_write:
 * @property: a #GladeProperty
 * @interface: a #GladeInterface
 * @props: a GArray of #GladePropInfo
 *
 * Write this property to the GladeInterface metadata
 */
gboolean
glade_property_write (GladeProperty *property, GladeInterface *interface, GArray *props)
{
	g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);
	g_return_val_if_fail (interface != NULL, FALSE);
	g_return_val_if_fail (props != NULL, FALSE);
	return GLADE_PROPERTY_GET_KLASS (property)->write (property, interface, props);
}


/**
 * glade_property_get_tooltip:
 * @property: a #GladeProperty
 *
 * Returns: The appropriate tooltip for the editor
 */
G_CONST_RETURN gchar *
glade_property_get_tooltip (GladeProperty *property)
{
	g_return_val_if_fail (GLADE_IS_PROPERTY (property), NULL);
	return GLADE_PROPERTY_GET_KLASS (property)->get_tooltip (property);
}

/* Parameters for translatable properties. */
void
glade_property_i18n_set_comment (GladeProperty *property,
				 const gchar   *str)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));
	if (property->i18n_comment)
		g_free (property->i18n_comment);

	property->i18n_comment = g_strdup (str);
	g_object_notify (G_OBJECT (property), "i18n-comment");
}

const gchar *
glade_property_i18n_get_comment (GladeProperty *property)
{
	g_return_val_if_fail (GLADE_IS_PROPERTY (property), NULL);
	return property->i18n_comment;
}

void
glade_property_i18n_set_translatable (GladeProperty *property,
				      gboolean       translatable)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));
	property->i18n_translatable = translatable;
	g_object_notify (G_OBJECT (property), "i18n-translatable");
}

gboolean
glade_property_i18n_get_translatable (GladeProperty *property)
{
	g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);
	return property->i18n_translatable;
}

void
glade_property_i18n_set_has_context (GladeProperty *property,
				     gboolean       has_context)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));
	property->i18n_has_context = has_context;
	g_object_notify (G_OBJECT (property), "i18n-has-context");
}

gboolean
glade_property_i18n_get_has_context (GladeProperty *property)
{
	g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);
	return property->i18n_has_context;
}

void
glade_property_set_sensitive (GladeProperty *property, 
			      gboolean       sensitive,
			      const gchar   *reason)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));

	/* reason is only why we're disableing it */
	if (sensitive == FALSE)
	{
		if (property->insensitive_tooltip)
			g_free (property->insensitive_tooltip);
		property->insensitive_tooltip =
			g_strdup (reason);
	}

	if (property->sensitive != sensitive)
	{
		gchar *tooltip;
		property->sensitive = sensitive;

		tooltip = (gchar *)GLADE_PROPERTY_GET_KLASS
			(property)->get_tooltip (property);

		g_signal_emit (G_OBJECT (property),
			       glade_property_signals[TOOLTIP_CHANGED],
			       0, tooltip);
		
	}
	g_object_notify (G_OBJECT (property), "sensitive");
}

gboolean
glade_property_get_sensitive (GladeProperty *property)
{
	g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);
	return property->sensitive;
}

void
glade_property_set_enabled (GladeProperty *property, 
			    gboolean       enabled)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));
	property->enabled = enabled;
	g_object_notify (G_OBJECT (property), "enabled");
}

gboolean
glade_property_get_enabled (GladeProperty *property)
{
	g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);
	return property->enabled;
}
