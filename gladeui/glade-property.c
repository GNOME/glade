/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2006 The GNOME Foundation.
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

/**
 * SECTION:glade-property
 * @Title: GladeProperty
 * @Short_Description: An interface to properties on the #GladeWidget.
 *
 * Every object property of every #GladeWidget in every #GladeProject has
 * a #GladeProperty to interface with, #GladeProperty provides a means
 * to handle properties in the runtime environment.
 * 
 * A #GladeProperty can be seen as an instance of a #GladePropertyClass, 
 * the #GladePropertyClass describes how a #GladeProperty will function.
 */

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
#include "glade-widget-adaptor.h"
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
glade_property_dup_impl (GladeProperty *template_prop, GladeWidget *widget)
{
	GladeProperty *property;

	property          = g_object_new (GLADE_TYPE_PROPERTY, 
					  "enabled", template_prop->enabled,
					  "sensitive", template_prop->sensitive,
					  "i18n-translatable", template_prop->i18n_translatable,
					  "i18n-has-context", template_prop->i18n_has_context,
					  "i18n-comment", template_prop->i18n_comment,
					  NULL);
	property->klass   = template_prop->klass;
	property->widget  = widget;
	property->value   = g_new0 (GValue, 1);

	property->insensitive_tooltip =
		template_prop->insensitive_tooltip ?
		g_strdup (template_prop->insensitive_tooltip) : NULL;

	g_value_init (property->value, template_prop->value->g_type);
	g_value_copy (template_prop->value, property->value);
	
	return property;
}

static gboolean
glade_property_equals_value_impl (GladeProperty *property,
				  const GValue  *value)
{
	if (G_IS_PARAM_SPEC_STRING (property->klass->pspec))
	{
		const gchar *prop_str, *value_str;

		/* in string specs; NULL and '\0' are 
		 * treated as equivalent.
		 */
		prop_str = g_value_get_string (property->value);
		value_str = g_value_get_string (value);

		if (prop_str == NULL && value_str && value_str[0] == '\0')
			return TRUE;
		else if (value_str == NULL && prop_str && prop_str[0] == '\0')
			return TRUE;
	}

	return !g_param_values_cmp (property->klass->pspec,
				    property->value, value);
}


static void
glade_property_update_prop_refs (GladeProperty *property, 
				 const GValue  *old_value,
				 const GValue  *new_value)
{
	GladeWidget *gold, *gnew;
	GObject     *old_object, *new_object;
	GList       *old_list, *new_list, *list, *removed, *added;

	if (GLADE_IS_PARAM_SPEC_OBJECTS (property->klass->pspec))
	{
		/* Make our own copies incase we're walking an
		 * unstable list
		 */
		old_list = g_value_dup_boxed (old_value);
		new_list = g_value_dup_boxed (new_value);

		/* Diff up the GList */
		removed = glade_util_removed_from_list (old_list, new_list);
		added   = glade_util_added_in_list     (old_list, new_list);

		/* Adjust the appropriate prop refs */
		for (list = removed; list; list = list->next)
		{
			old_object = list->data;
			gold = glade_widget_get_from_gobject (old_object);
			glade_widget_remove_prop_ref (gold, property);
		}
		for (list = added; list; list = list->next)
		{
			new_object = list->data;
			gnew = glade_widget_get_from_gobject (new_object);
			glade_widget_add_prop_ref (gnew, property);
		}

		g_list_free (removed);
		g_list_free (added);
		g_list_free (old_list);
		g_list_free (new_list);
	}
	else
	{
		if ((old_object = g_value_get_object (old_value)) != NULL)
		{
			gold = glade_widget_get_from_gobject (old_object);
			glade_widget_remove_prop_ref (gold, property);
		}
		
		if ((new_object = g_value_get_object (new_value)) != NULL)
		{
			gnew = glade_widget_get_from_gobject (new_object);
			glade_widget_add_prop_ref (gnew, property);
		}
	}
}

static gboolean
glade_property_verify (GladeProperty *property, const GValue *value)
{
	if (property->klass->packing)
	{
		if (property->widget->parent)
			return glade_widget_adaptor_child_verify_property (property->widget->parent->adaptor,
									   property->widget->parent->object,
									   property->widget->object,
									   property->klass->id,
									   value);
		else
			return FALSE;
	}
	else
	{
		return glade_widget_adaptor_verify_property (property->widget->adaptor, 
							     property->widget->object,
							     property->klass->id,
							     value);
	}
}

static void
glade_property_set_value_impl (GladeProperty *property, const GValue *value)
{
	GladeProject *project = property->widget ?
		glade_widget_get_project (property->widget) : NULL;
	gboolean      changed = FALSE;
	GValue old_value = {0,};

#if 0
	{
		gchar *str = glade_widget_adaptor_string_from_value
			     (GLADE_WIDGET_ADAPTOR (property->klass->handle),
			      property->klass, value);
		g_print ("Setting property %s on %s to %s\n",
			 property->klass->id,
			 property->widget ? property->widget->name : "unknown", str);
		g_free (str);
	}
#endif

	if (!g_value_type_compatible (G_VALUE_TYPE (property->value), G_VALUE_TYPE (value)))
	{
		g_warning ("Trying to assign an incompatible value to property %s\n",
			    property->klass->id);
		return;
	}

	/* Check if the backend doesnt give us permission to
	 * set this value.
	 */
	if (glade_property_superuser () == FALSE && property->widget &&
	    project && glade_project_is_loading (project) == FALSE &&
	    glade_property_verify (property, value) == FALSE)
		return;
	
	/* save "changed" state.
	 */
	changed = g_param_values_cmp (property->klass->pspec, 
				      property->value, value) != 0;


	/* Add/Remove references from widget ref stacks here
	 * (before assigning the value)
	 */
	if (property->widget && changed && glade_property_class_is_object (property->klass))
		glade_property_update_prop_refs (property, property->value, value);


	/* Make a copy of the old value */
	g_value_init (&old_value, G_VALUE_TYPE (property->value));
	g_value_copy (property->value, &old_value);
	
	/* Assign property first so that; if the object need be
	 * rebuilt, it will reflect the new value
	 */
	g_value_reset (property->value);
	g_value_copy (value, property->value);

	GLADE_PROPERTY_GET_KLASS (property)->sync (property);

	if (changed && property->widget)
	{
		g_signal_emit (G_OBJECT (property),
			       glade_property_signals[VALUE_CHANGED],
			       0, &old_value, property->value);
	}
	
	g_value_unset (&old_value);
}

static void
glade_property_get_value_impl (GladeProperty *property, GValue *value)
{

	g_value_init (value, property->klass->pspec->value_type);
	g_value_copy (property->value, value);
}

static void
glade_property_get_default_impl (GladeProperty *property, GValue *value)
{

	g_value_init (value, property->klass->pspec->value_type);
	g_value_copy (property->klass->def, value);
}

static void
glade_property_sync_impl (GladeProperty *property)
{

	/* Heh, here are the many reasons not to
	 * sync a property ;-)
	 */
	if (/* the class can be NULL during object,
	     * construction this is just a temporary state */
	    property->klass == NULL       ||
	    /* optional properties that are disabled */
	    property->enabled == FALSE    || 
	    /* explicit "never sync" flag */
	    property->klass->ignore       || 
	    /* avoid recursion */
	    property->syncing             ||
	    /* No widget owns this property yet */
	    property->widget == NULL)
		return;

	property->syncing = TRUE;

	/* In the case of construct_only, the widget instance must be rebuilt
	 * to apply the property
	 */
	if (property->klass->construct_only)
		glade_widget_rebuild (property->widget);
	else if (property->klass->packing)
		glade_widget_child_set_property (glade_widget_get_parent (property->widget),
						 property->widget, 
						 property->klass->id, 
						 property->value);
	else
		glade_widget_object_set_property (property->widget, 
						  property->klass->id, 
						  property->value);

	property->syncing = FALSE;
}

static void
glade_property_load_impl (GladeProperty *property)
{
	GObject      *object;
	GObjectClass *oclass;
	
	if (property->widget == NULL ||
	    property->klass->packing ||
	    property->klass->ignore  ||
	    !(property->klass->pspec->flags & G_PARAM_READABLE))
		return;

	object = glade_widget_get_object (property->widget);
	oclass = G_OBJECT_GET_CLASS (object);
	
	if (g_object_class_find_property (oclass, property->klass->id))
		g_object_get_property (object, property->klass->id, property->value);
}

static G_CONST_RETURN gchar *
glade_property_get_tooltip_impl (GladeProperty *property)
{
	gchar *tooltip = NULL;
	if (property->sensitive == FALSE)
		tooltip = property->insensitive_tooltip;
	else
		tooltip = property->klass->tooltip;
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
	prop_class->equals_value          = glade_property_equals_value_impl;
	prop_class->set_value             = glade_property_set_value_impl;
	prop_class->get_value             = glade_property_get_value_impl;
	prop_class->get_default           = glade_property_get_default_impl;
	prop_class->sync                  = glade_property_sync_impl;
	prop_class->load                  = glade_property_load_impl;
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
		  _("Comment for translators"),
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
		  _("Whether or not the translatable string has a context prefix"),
		  FALSE, G_PARAM_READWRITE));

	/* Signal */
	glade_property_signals[VALUE_CHANGED] =
		g_signal_new ("value-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladePropertyKlass,
					       value_changed),
			      NULL, NULL,
			      glade_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

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
			sizeof (GladePropertyKlass), /* Klass is our class */
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
 * @klass: A #GladePropertyClass defining this property
 * @widget: The #GladeWidget this property is created for
 * @value: The initial #GValue of the property or %NULL
 *         (the #GladeProperty will assume ownership of @value)
 *
 * Creates a #GladeProperty of type @klass for @widget with @value; if
 * @value is %NULL, then the introspected default value for that property
 * will be used.
 *
 * Returns: The newly created #GladeProperty
 */
GladeProperty *
glade_property_new (GladePropertyClass *klass, 
		    GladeWidget        *widget, 
		    GValue             *value)
{
	GladeProperty *property;
	
	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (klass), NULL);

	property            = 
		(GladeProperty *)g_object_new (GLADE_TYPE_PROPERTY, NULL);
	property->klass     = klass;
	property->widget    = widget;
	property->value     = value;
	
	if (klass->optional)
		property->enabled = klass->optional_default;
	
	if (property->value == NULL)
	{
		g_assert (klass->orig_def);
		
		property->value = g_new0 (GValue, 1);
		g_value_init (property->value, klass->orig_def->g_type);
		g_value_copy (klass->orig_def, property->value);
	}
	return property;
}

/**
 * glade_property_dup:
 * @template_prop: A #GladeProperty
 * @widget: A #GladeWidget
 *
 * Returns: A newly duplicated property based on the new widget
 */
GladeProperty *
glade_property_dup (GladeProperty *template_prop, GladeWidget *widget)
{
	g_return_val_if_fail (GLADE_IS_PROPERTY (template_prop), NULL);
	return GLADE_PROPERTY_GET_KLASS (template_prop)->dup (template_prop, widget);
}

static void
glade_property_reset_common (GladeProperty *property, gboolean original)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));

	GLADE_PROPERTY_GET_KLASS (property)->set_value
		(property, (original) ? property->klass->orig_def : property->klass->def);
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
	glade_property_reset_common (property, FALSE);
}

/**
 * glade_property_original_reset:
 * @property: A #GladeProperty
 *
 * Resets this property to its original default value
 */
void
glade_property_original_reset (GladeProperty *property)
{
	glade_property_reset_common (property, TRUE);
}

static gboolean
glade_property_default_common (GladeProperty *property, gboolean orig)
{
	g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);
	return GLADE_PROPERTY_GET_KLASS (property)->equals_value
		(property, (orig)  ? property->klass->orig_def : property->klass->def);
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
	return glade_property_default_common (property, FALSE);
}

/**
 * glade_property_original_default:
 * @property: A #GladeProperty
 *
 * Returns: Whether this property is at its original default value
 */
gboolean
glade_property_original_default (GladeProperty *property)
{
	return glade_property_default_common (property, TRUE);
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
			     const GValue       *value)
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
static gboolean
glade_property_equals_va_list (GladeProperty *property, va_list vl)
{
	GValue   *value;
	gboolean  ret;

	g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);

	value = glade_property_class_make_gvalue_from_vl (property->klass, vl);

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

	value = glade_property_class_make_gvalue_from_vl (property->klass, vl);

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
	glade_property_class_set_vl_from_gvalue (property->klass, property->value, vl);
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
 * glade_property_load:
 * @property: a #GladeProperty
 *
 * Loads the value of @property from the coresponding object instance
 */
void
glade_property_load (GladeProperty *property)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));
	GLADE_PROPERTY_GET_KLASS (property)->load (property);
}

/**
 * glade_property_read:
 * @property: a #GladeProperty or #NULL
 * @project: the #GladeProject
 * @node: the #GladeXmlNode to read, will either be a 'widget'
 *        node or a 'child' node for packing properties.
 *
 * Read the value and any attributes for @property from @node, assumes
 * @property is being loaded for @project
 *
 * Note that object values will only be resolved after the project is
 * completely loaded
 */
void
glade_property_read (GladeProperty      *property, 
		     GladeProject       *project,
		     GladeXmlNode       *node)
{
	GladeXmlNode *prop;
	GValue       *gvalue = NULL;
	gchar        *id, *name, *value;

	g_return_if_fail (GLADE_IS_PROPERTY (property));
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (node != NULL);

	/* This code should work the same for <packing> and <widget> */
	if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_PACKING) ||
	      glade_xml_node_verify_silent
	      (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (project)))))
		return;

	for (prop = glade_xml_node_get_children (node); 
	     prop; prop = glade_xml_node_next (prop))
	{

		if (!glade_xml_node_verify_silent (prop, GLADE_XML_TAG_PROPERTY))
			continue;

		if (!(name = glade_xml_get_property_string_required
		      (prop, GLADE_XML_TAG_NAME, NULL)))
			continue;

		/* Make sure we are working with dashes and
		 * not underscores ... 
		 */
		id = glade_util_read_prop_name (name);
		g_free (name);

		if (!strcmp (id, property->klass->id))
		{
			if (!(value = glade_xml_get_content (prop)))
			{
				/* XXX should be glade_xml_get_content_required()... */
				g_free (id);
				break;
			}

			if (property && glade_property_class_is_object (property->klass))
			{
				/* we must synchronize this directly after loading this project
				 * (i.e. lookup the actual objects after they've been parsed and
				 * are present).
				 */
				g_object_set_data_full (G_OBJECT (property), 
							"glade-loaded-object", 
							g_strdup (value), g_free);
			}
			else
			{
				gvalue = glade_property_class_make_gvalue_from_string
					(property->klass, value, project);

				if (property) 
				{
					GLADE_PROPERTY_GET_KLASS
						(property)->set_value (property, gvalue);
				}

				g_value_unset (gvalue);
				g_free (gvalue);

				/* If an optional property is specified in the
				 * glade file, its enabled
				 */
				property->enabled = TRUE;
			}

			if (property)
			{
				gint translatable, has_context;
				gchar *comment;

				translatable = glade_xml_get_property_boolean
					(prop, GLADE_TAG_TRANSLATABLE, FALSE);
				has_context = glade_xml_get_property_boolean
					(prop, GLADE_TAG_HAS_CONTEXT, FALSE);
				comment = glade_xml_get_property_string
					(prop, GLADE_TAG_COMMENT);

				glade_property_i18n_set_translatable
					(property, translatable);
				glade_property_i18n_set_has_context
					(property, has_context);
				glade_property_i18n_set_comment
					(property, comment);

				g_free (comment);
			}

			g_free (value);
			g_free (id);
			break;
		}
		g_free (id);
	}
}


/**
 * glade_property_write:
 * @property: a #GladeProperty
 * @context: A #GladeXmlContext
 * @node: A #GladeXmlNode
 *
 * Write @property to @node
 */
void
glade_property_write (GladeProperty   *property,
		      GladeXmlContext *context,
		      GladeXmlNode    *node)
{
	GladeXmlNode *prop_node;
	GladeProject *project;
	gchar *name, *value, *tmp;

	g_return_if_fail (GLADE_IS_PROPERTY (property));
	g_return_if_fail (node != NULL);

	project = property->widget->project;

	/* This code should work the same for <packing> and <widget> */
	if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_PACKING) ||
	      glade_xml_node_verify_silent
	      (node, GLADE_XML_TAG_WIDGET (glade_project_get_format(project)))))
		return;

	if (!property->klass->save || !property->enabled)
		return;

	g_assert (property->klass->orig_def);
	g_assert (property->klass->def);

	/* Skip properties that are default by original pspec default
	 * (excepting those that specified otherwise).
	 */
	if (!(property->klass->save_always || property->save_always) &&
	    glade_property_equals_value (property, property->klass->orig_def))
		return;

	/* Escape our string and save with underscores */
	name = g_strdup (property->klass->id);
	glade_util_replace (name, '-', '_');

	/* convert the value of this property to a string */
	if (!(value = glade_widget_adaptor_string_from_value
	     (GLADE_WIDGET_ADAPTOR (property->klass->handle),
	      property->klass, property->value)))
		/* make sure we keep the empty string, also... upcomming
		 * funcs that may not like NULL.
		 */
		value = g_strdup ("");
	else
	{
		/* Escape the string so that it will be parsed as it should. */
		tmp = value;
		value = g_markup_escape_text (value, -1);
		g_free (tmp);
	}

	/* Now dump the node values... */
	prop_node = glade_xml_node_new (context, GLADE_XML_TAG_PROPERTY);
	glade_xml_node_append_child (node, prop_node);

	/* Name and value */
	glade_xml_node_set_property_string (prop_node, GLADE_XML_TAG_NAME, name);
	glade_xml_set_content (prop_node, value);

	/* i18n stuff */
	if (property->klass->translatable)
	{
		if (property->i18n_translatable)
			glade_xml_node_set_property_string (prop_node, 
							    GLADE_TAG_TRANSLATABLE, 
							    GLADE_XML_TAG_I18N_TRUE);

		if (property->i18n_has_context)
			glade_xml_node_set_property_string (prop_node, 
							    GLADE_TAG_HAS_CONTEXT, 
							    GLADE_XML_TAG_I18N_TRUE);


		if (property->i18n_comment)
			glade_xml_node_set_property_string (prop_node, 
							    GLADE_TAG_COMMENT, 
							    property->i18n_comment);
	}

	g_free (name);
	g_free (value);
}

/**
 * glade_property_add_object:
 * @property: a #GladeProperty
 * @object: The #GObject to add
 *
 * Adds @object to the object list in @property.
 *
 * Note: This function expects @property to be a #GladeParamSpecObjects
 * or #GParamSpecObject type property.
 */
void
glade_property_add_object (GladeProperty  *property,
			   GObject        *object)
{
	GList *list = NULL, *new_list = NULL;

	g_return_if_fail (GLADE_IS_PROPERTY (property));
	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (GLADE_IS_PARAM_SPEC_OBJECTS (property->klass->pspec) ||
			  G_IS_PARAM_SPEC_OBJECT (property->klass->pspec));

	if (GLADE_IS_PARAM_SPEC_OBJECTS (property->klass->pspec))
	{
		glade_property_get (property, &list);
		new_list = g_list_copy (list);

		new_list = g_list_append (new_list, object);
		glade_property_set (property, new_list);

		/* ownership of the list is not passed 
		 * through glade_property_set() 
		 */
		g_list_free (new_list);
	}
	else
	{
		glade_property_set (property, object);
	}
}

/**
 * glade_property_remove_object:
 * @property: a #GladeProperty
 * @object: The #GObject to add
 *
 * Removes @object from the object list in @property.
 *
 * Note: This function expects @property to be a #GladeParamSpecObjects
 * or #GParamSpecObject type property.
 */
void
glade_property_remove_object (GladeProperty  *property,
			      GObject        *object)
{
	GList *list = NULL, *new_list = NULL;

	g_return_if_fail (GLADE_IS_PROPERTY (property));
	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (GLADE_IS_PARAM_SPEC_OBJECTS (property->klass->pspec) ||
			  G_IS_PARAM_SPEC_OBJECT (property->klass->pspec));

	if (GLADE_IS_PARAM_SPEC_OBJECTS (property->klass->pspec))
	{
		/* If object isnt in list; list should stay in tact.
		 * not bothering to check for now.
		 */
		glade_property_get (property, &list);
		new_list = g_list_copy (list);

		new_list = g_list_remove (new_list, object);
		glade_property_set (property, new_list);

		/* ownership of the list is not passed 
		 * through glade_property_set() 
		 */
		g_list_free (new_list);
	}
	else
	{
		glade_property_set (property, object);
	}

	glade_property_class_get_from_gvalue (property->klass,
					      property->value,
					      &list);

	glade_property_set (property, list);
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

/**
 * glade_property_set_save_always:
 * @property: A #GladeProperty
 * @setting: the value to set
 *
 * Sets whether this property should be special cased
 * to always be saved regardless of its default value.
 * (used for some special cases like properties
 * that are assigned initial values in composite widgets
 * or derived widget code).
 */
void
glade_property_set_save_always (GladeProperty      *property,
				gboolean            setting)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));

	property->save_always = setting;
}

/**
 * glade_property_get_save_always:
 * @property: A #GladeProperty
 *
 * Returns: whether this property is special cased
 * to always be saved regardless of its default value.
 */
gboolean
glade_property_get_save_always (GladeProperty      *property)
{
	g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);

	return property->save_always;
}

void
glade_property_set_enabled (GladeProperty *property, 
			    gboolean       enabled)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));

	property->enabled = enabled;
	if (enabled)
		glade_property_sync (property);

	g_object_notify (G_OBJECT (property), "enabled");
}

gboolean
glade_property_get_enabled (GladeProperty *property)
{
	g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);
	return property->enabled;
}


static gint glade_property_su_stack = 0;

void
glade_property_push_superuser (void)
{
	glade_property_su_stack++;
}

void
glade_property_pop_superuser (void)
{
	if (--glade_property_su_stack < 0)
	{
		g_critical ("Bug: property super user stack is corrupt.\n");
	}
}

gboolean
glade_property_superuser (void)
{
	return glade_property_su_stack > 0;
}
