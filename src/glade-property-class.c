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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "glade.h"
#include "glade-xml-utils.h"
#include "glade-choice.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-parameter.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-gtk.h"

#include <string.h>
#include <stdlib.h>
#include <gmodule.h>

#if 0
typedef struct GladePropertyTypeTable {
	const gchar *xml_tag;
	GladePropertyType glade_type;
	GType *g_type;
};

/* #warning Implement me. */
#endif

GladePropertyType
glade_property_type_str_to_enum (const gchar *str)
{
	if (strcmp (str, GLADE_TAG_STRING) == 0)
		return GLADE_PROPERTY_TYPE_STRING;
	if (strcmp (str, GLADE_TAG_BOOLEAN) == 0)
		return GLADE_PROPERTY_TYPE_BOOLEAN;
	if (strcmp (str, GLADE_TAG_FLOAT) == 0)
		return GLADE_PROPERTY_TYPE_FLOAT;
	if (strcmp (str, GLADE_TAG_INTEGER) == 0)
		return GLADE_PROPERTY_TYPE_INTEGER;
	if (strcmp (str, GLADE_TAG_ENUM) == 0)
		return GLADE_PROPERTY_TYPE_ENUM;
	if (strcmp (str, GLADE_TAG_OTHER_WIDGETS) == 0)
		return GLADE_PROPERTY_TYPE_OTHER_WIDGETS;
	if (strcmp (str, GLADE_TAG_OBJECT) == 0)
		return GLADE_PROPERTY_TYPE_OBJECT;

	g_warning ("Could not determine the property type from *%s*\n", str);

	return GLADE_PROPERTY_TYPE_ERROR;
}

gchar *
glade_property_type_enum_to_string (GladePropertyType type)
{
	switch (type) {
	case GLADE_PROPERTY_TYPE_STRING:
		return GLADE_TAG_STRING;
	case GLADE_PROPERTY_TYPE_BOOLEAN:
		return GLADE_TAG_BOOLEAN;
	case GLADE_PROPERTY_TYPE_FLOAT:
		return GLADE_TAG_FLOAT;
	case GLADE_PROPERTY_TYPE_INTEGER:
		return GLADE_TAG_INTEGER;
	case GLADE_PROPERTY_TYPE_DOUBLE:
		return GLADE_TAG_DOUBLE;
	case GLADE_PROPERTY_TYPE_ENUM:
		return GLADE_TAG_ENUM;
	case GLADE_PROPERTY_TYPE_OTHER_WIDGETS:
		return GLADE_TAG_OTHER_WIDGETS;
	case GLADE_PROPERTY_TYPE_OBJECT:
		return GLADE_TAG_OBJECT;
	case GLADE_PROPERTY_TYPE_ERROR:
		return NULL;
	}

	return NULL;
}

static GladePropertyQuery *
glade_property_query_new (void)
{
	GladePropertyQuery *query;

	query = g_new0 (GladePropertyQuery, 1);
	query->window_title = NULL;
	query->question = NULL;

	return query;
}

	
static GladePropertyQuery *
glade_query_new_from_node (GladeXmlNode *node)
{
	GladePropertyQuery *query;
	
	if (!glade_xml_node_verify (node, GLADE_TAG_QUERY))
		return NULL;

	query = glade_property_query_new ();

	query->window_title = glade_xml_get_value_string_required (node, GLADE_TAG_WINDOW_TITLE, NULL);
	query->question     = glade_xml_get_value_string_required (node, GLADE_TAG_QUESTION, NULL);

	if ((query->window_title == NULL) ||
	    (query->question == NULL))
		return NULL;

	return query;	
}

GladePropertyClass *
glade_property_class_new (void)
{
	GladePropertyClass *property_class;

	property_class = g_new0 (GladePropertyClass, 1);
	property_class->type = GLADE_PROPERTY_TYPE_ERROR;
	property_class->id = NULL;
#if 0	
	g_print ("New property class %d. Id:%d\n",
		 GPOINTER_TO_INT (property_class), GPOINTER_TO_INT (property_class->id));
#endif
	property_class->name = NULL;
	property_class->tooltip = NULL;
	property_class->parameters = NULL;
	property_class->choices = NULL;
	property_class->optional = FALSE;
	property_class->optional_default = TRUE;
	property_class->common = FALSE;
	property_class->packing = FALSE;
	property_class->get_default = FALSE;
	property_class->query = NULL;
	property_class->set_function = NULL;

	return property_class;
}

#define MY_FREE(foo) if(foo) g_free(foo); foo = NULL
static void
glade_widget_property_class_free (GladePropertyClass *class)
{
	g_return_if_fail (GLADE_IS_PROPERTY_CLASS (class));
	
	MY_FREE (class->name);
	MY_FREE (class->tooltip);
	MY_FREE (class);
}
#undef MY_FREE	

static GladePropertyType
glade_property_class_get_type_from_spec (GParamSpec *spec)
{
	switch (G_PARAM_SPEC_TYPE (spec))
	{
	case G_TYPE_PARAM_INT:
	case G_TYPE_PARAM_UINT:
		return GLADE_PROPERTY_TYPE_INTEGER;
	case G_TYPE_PARAM_FLOAT:
		return GLADE_PROPERTY_TYPE_FLOAT;
	case G_TYPE_PARAM_BOOLEAN:
		return GLADE_PROPERTY_TYPE_BOOLEAN;
	case G_TYPE_PARAM_STRING:
		return GLADE_PROPERTY_TYPE_STRING;
	case G_TYPE_PARAM_ENUM:
		return GLADE_PROPERTY_TYPE_ENUM;
	case G_TYPE_PARAM_DOUBLE:
		return GLADE_PROPERTY_TYPE_DOUBLE;
	case G_TYPE_PARAM_LONG:
		g_warning ("Long not implemented\n");
		break;
	case G_TYPE_PARAM_UCHAR:
		g_warning ("uchar not implemented\n");
		break;
	case G_TYPE_PARAM_OBJECT:
		return GLADE_PROPERTY_TYPE_OBJECT;
	default:
		g_warning ("Could not determine GladePropertyType from spec (%d,%s)",
			   G_PARAM_SPEC_TYPE (spec),
			   G_PARAM_SPEC_TYPE_NAME (spec));
	}

	return GLADE_PROPERTY_TYPE_ERROR;
}

static GladeChoice *
glade_property_class_choice_new_from_value (GEnumValue value)
{
	GladeChoice *choice;

	choice = glade_choice_new ();
	choice->name = g_strdup (value.value_nick);
	choice->id     = g_strdup (value.value_name);
#if 0	
	g_print ("Choice Id is %s\n", choice->id);
#endif
#if 0	
	choice->symbol = g_strdup (value.value_name);
#endif	
	choice->value  = value.value;

	return choice;
}

static GList *
glade_property_class_get_choices_from_spec (GParamSpec *spec)
{
	GladeChoice *choice;
	GEnumClass *class;
	GEnumValue value;
	GList *list = NULL;
	gint num;
	gint i;

	class = G_PARAM_SPEC_ENUM (spec)->enum_class;
	num = class->n_values;
	for (i = 0; i < num; i++) {
		value = class->values[i];
		choice = glade_property_class_choice_new_from_value (value);
		if (choice)
			list = g_list_prepend (list, choice);
	}
	list = g_list_reverse (list);

	return list;
}

#if 0
static gchar *
glade_property_get_default_choice (GParamSpec *spec,
				   GladePropertyClass *class)
{
	GladeChoice *choice = NULL;
	GList *list;
	gint def;

	g_return_val_if_fail (G_IS_PARAM_SPEC_ENUM (spec), NULL);
		
	def = (gint) G_PARAM_SPEC_ENUM (spec)->default_value;
		
	list = class->choices;
	for (; list != NULL; list = list->next) {
		choice = list->data;
		if (choice->value == def)
			break;
	}
	if (list == NULL) {
		g_warning ("Could not find the default value for %s\n", spec->nick);
		if (class->choices == NULL)
			return NULL;
		choice = class->choices->data;
	}

	return g_strdup (choice->symbol);
}
#endif

gchar *
glade_property_class_make_string_from_gvalue (GladePropertyType type,
					      const GValue *value)
{
	gchar *string = NULL;

	switch (type) {
	case GLADE_PROPERTY_TYPE_INTEGER:
		string = g_strdup_printf ("%d", g_value_get_int (value));
		break;
	case GLADE_PROPERTY_TYPE_FLOAT:
		string = g_strdup_printf ("%f", g_value_get_float (value));
		break;
	case GLADE_PROPERTY_TYPE_DOUBLE:
		string = g_strdup_printf ("%g", g_value_get_double (value));
		break;
	case GLADE_PROPERTY_TYPE_BOOLEAN:
		string = g_strdup_printf ("%s", g_value_get_boolean (value) ?
					  GLADE_TAG_TRUE : GLADE_TAG_FALSE);
		break;
	case GLADE_PROPERTY_TYPE_STRING:
		string = g_strdup (g_value_get_string (value));
		break;
	case GLADE_PROPERTY_TYPE_ENUM:
		glade_implement_me ();
		break;
	default:
		g_warning ("Could not make string from gvalue for type %s\n",
			 glade_property_type_enum_to_string (type));
	}

	return string;
}

GValue *
glade_property_class_make_gvalue_from_string (GladePropertyType type,
					      const gchar *string)
{
	GValue *value = g_new0 (GValue, 1);
	
	switch (type) {
	case GLADE_PROPERTY_TYPE_INTEGER:
		g_value_init (value, G_TYPE_INT);
		g_value_set_int (value, atoi (string));
		break;
	case GLADE_PROPERTY_TYPE_FLOAT:
		g_value_init (value, G_TYPE_FLOAT);
		g_value_set_float (value, atof (string));
		break;
	case GLADE_PROPERTY_TYPE_BOOLEAN:
		g_value_init (value, G_TYPE_BOOLEAN);
		if (strcmp (string, GLADE_TAG_TRUE) == 0)
			g_value_set_boolean (value, TRUE);
		else
			g_value_set_boolean (value, FALSE);
		break;
	case GLADE_PROPERTY_TYPE_DOUBLE:
		g_value_init (value, G_TYPE_DOUBLE);
		g_value_set_double (value, atof (string));
		break;
	case GLADE_PROPERTY_TYPE_STRING:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, string);
		break;
	case GLADE_PROPERTY_TYPE_ENUM:
		glade_implement_me ();
#if 0	
		g_value_init (value);
		g_free (value);
		value = NULL;
#endif	
		break;
	case GLADE_PROPERTY_TYPE_OBJECT:
		break;
	default:
		g_warning ("Could not make gvalue from string ->%s<- and type %s\n",
			 string,
			 glade_property_type_enum_to_string (type));
		g_free (value);
		value = NULL;
	}

	return value;
}

static GValue *
glade_property_class_get_default_from_spec (GParamSpec *spec,
					    GladePropertyClass *class,
					    GladeXmlNode *node)
{
	GValue *value;

	value = g_new0 (GValue, 1);

	switch (class->type) {
	case GLADE_PROPERTY_TYPE_ENUM:
		g_value_init (value, spec->value_type);
		g_value_set_enum (value, G_PARAM_SPEC_ENUM (spec)->default_value);
		break;
	case GLADE_PROPERTY_TYPE_STRING:
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, G_PARAM_SPEC_STRING (spec)->default_value);
		break;
	case GLADE_PROPERTY_TYPE_INTEGER:
		g_value_init (value, G_TYPE_INT);
		if (G_IS_PARAM_SPEC_INT (spec))
			g_value_set_int (value, G_PARAM_SPEC_INT (spec)->default_value);
		else
			g_value_set_int (value, G_PARAM_SPEC_UINT (spec)->default_value);
		break;
	case GLADE_PROPERTY_TYPE_FLOAT:
		g_value_init (value, G_TYPE_FLOAT);
		g_value_set_float (value, G_PARAM_SPEC_FLOAT (spec)->default_value);
		break;
	case GLADE_PROPERTY_TYPE_DOUBLE:
		g_value_init (value, G_TYPE_DOUBLE);
		g_value_set_double (value, G_PARAM_SPEC_DOUBLE (spec)->default_value);
		break;
	case GLADE_PROPERTY_TYPE_BOOLEAN:
		g_value_init (value, G_TYPE_BOOLEAN);
		g_value_set_boolean (value, G_PARAM_SPEC_BOOLEAN (spec)->default_value);
		break;
	case GLADE_PROPERTY_TYPE_OTHER_WIDGETS:
		break;
	case GLADE_PROPERTY_TYPE_OBJECT:
		break;
	case GLADE_PROPERTY_TYPE_ERROR:
		break;
	}

	return value;
}

static gchar *
glade_property_get_parameter_numeric_min (GParamSpec *spec)
{
	gchar *value = NULL;
	
	if (G_IS_PARAM_SPEC_INT (spec))
		value = g_strdup_printf ("%d", G_PARAM_SPEC_INT (spec)->minimum);
	else if (G_IS_PARAM_SPEC_UINT (spec))
		value = g_strdup_printf ("%u", G_PARAM_SPEC_UINT (spec)->minimum);
	else if (G_IS_PARAM_SPEC_FLOAT (spec))
		value = g_strdup_printf ("%g", G_PARAM_SPEC_FLOAT (spec)->minimum);
	else if (G_IS_PARAM_SPEC_DOUBLE (spec))
		value = g_strdup_printf ("%g", G_PARAM_SPEC_DOUBLE (spec)->minimum);
	else
		g_warning ("glade_propery_get_parameter_numeric_item invalid ParamSpec (min)\n");

	return value;
}

static gchar *
glade_property_get_parameter_numeric_max (GParamSpec *spec)
{
	gchar *value = NULL;
	
	if (G_IS_PARAM_SPEC_INT (spec))
		value = g_strdup_printf ("%d", G_PARAM_SPEC_INT (spec)->maximum);
	else if (G_IS_PARAM_SPEC_UINT (spec))
		value = g_strdup_printf ("%u", G_PARAM_SPEC_UINT (spec)->maximum);
	else if (G_IS_PARAM_SPEC_FLOAT (spec))
		value = g_strdup_printf ("%g", G_PARAM_SPEC_FLOAT (spec)->maximum);
	else if (G_IS_PARAM_SPEC_DOUBLE (spec))
		value = g_strdup_printf ("%g", G_PARAM_SPEC_DOUBLE (spec)->maximum);
	else 
		g_warning ("glade_propery_get_parameter_numeric_item invalid ParamSpec (max)\n");

	return value;
}


static GList *
glade_property_get_parameters_numeric (GParamSpec *spec,
				       GladePropertyClass *class)
{
	GladeParameter *parameter;
	GList *list = NULL;

	g_return_val_if_fail (G_IS_PARAM_SPEC_INT    (spec) |
			      G_IS_PARAM_SPEC_UINT   (spec) |
			      G_IS_PARAM_SPEC_FLOAT  (spec) |
			      G_IS_PARAM_SPEC_DOUBLE (spec), NULL);

	/* Get the min value */
	parameter = glade_parameter_new ();
	parameter->key = g_strdup ("Min");
	parameter->value = glade_property_get_parameter_numeric_min (spec);
	list = g_list_prepend (list, parameter);
	
	/* Get the max value */
	parameter = glade_parameter_new ();
	parameter->key = g_strdup ("Max");
	parameter->value = glade_property_get_parameter_numeric_max (spec);
	list = g_list_prepend (list, parameter);


	return list;
}


static GList *
glade_property_class_get_parameters_from_spec (GParamSpec *spec,
					       GladePropertyClass *class,
					       GladeXmlNode *node)
{
	GList *parameters = NULL;
	GladeXmlNode *child;

	switch (class->type) {
	case GLADE_PROPERTY_TYPE_ENUM:
		break;
	case GLADE_PROPERTY_TYPE_STRING:
		break;
	case GLADE_PROPERTY_TYPE_INTEGER:
	case GLADE_PROPERTY_TYPE_FLOAT:
	case GLADE_PROPERTY_TYPE_DOUBLE:
		parameters = glade_property_get_parameters_numeric (spec, class);
		break;
	case GLADE_PROPERTY_TYPE_BOOLEAN:
		break;
	case GLADE_PROPERTY_TYPE_OTHER_WIDGETS:
		break;
	case GLADE_PROPERTY_TYPE_OBJECT:
		break;
	case GLADE_PROPERTY_TYPE_ERROR:
		break;
	}

	/* Get the parameters that are specified on the glade file,
	 * they can overwrite gtk+ settings. For example the default
	 * size of a GtkWindow is 0,0 which we need to overwrite.
	 */
	child = glade_xml_search_child (node, GLADE_TAG_PARAMETERS);
	if (child != NULL)
		parameters = glade_parameter_list_new_from_node (parameters,
								 child);
	
	return parameters;
}

static GValue *
glade_property_class_get_default (GladeXmlNode *node, GladePropertyType type)
{
	GValue *value;
	gchar *temp;

	temp =	glade_xml_get_property_string (node, GLADE_TAG_DEFAULT);

	if (!temp) {
#if 0	
		g_print ("Temp is NULL, we dont' have a default\n");
#endif	
		return NULL;
	}

	value = glade_property_class_make_gvalue_from_string (type, temp);

	g_free (temp);

	return value;
}


/**
 * glade_property_class_load_from_param_spec:
 * @name: 
 * @class: 
 * @widget_class: 
 * @node: 
 * 
 * Loads the members of @class that we get from gtk's ParamSpec
 * 
 * Return Value: 
 **/
static gboolean
glade_property_class_load_from_param_spec (const gchar *name,
					   GladePropertyClass *class,
					   GladeWidgetClass *widget_class,
					   GladeXmlNode *node)
{
	GParamSpec *spec;
	gchar *def;

	spec = glade_widget_class_find_spec (widget_class, name);

	if (spec == NULL) {
		g_warning ("Could not create a property class from a param spec for *%s* with name *%s*\n",
			   widget_class->name, name);
		return FALSE;
	}

	class->id      = g_strdup (spec->name);
	class->name    = g_strdup (spec->nick);
	class->tooltip = g_strdup (spec->blurb);
	class->type    = glade_property_class_get_type_from_spec (spec);

	if (class->type == GLADE_PROPERTY_TYPE_ERROR) {
		g_warning ("Could not create the \"%s\" property for the \"%s\" class\n",
			   name, widget_class->name);
		return FALSE;
	}
	
	if (class->type == GLADE_PROPERTY_TYPE_ENUM)
		class->choices = glade_property_class_get_choices_from_spec (spec);

	/* We want to use the parm spec default only when the xml files do not provide a
	 * default value
	 */
	def = glade_xml_get_property_string (node, GLADE_TAG_DEFAULT);
	if (def) {
		class->def = glade_property_class_get_default (node, class->type);
		g_free (def);
	} else {
		class->def = glade_property_class_get_default_from_spec (spec, class, node);
	}
	class->parameters = glade_property_class_get_parameters_from_spec (spec, class, node);
	
	return TRUE;
}

static gboolean
glade_property_class_get_get_function (GladePropertyClass *class, const gchar *function_name)
{
	return glade_gtk_get_get_function_hack (class, function_name);
}

static gboolean
glade_property_class_get_set_function (GladePropertyClass *class, const gchar *function_name)
{
	static GModule *allsymbols;

	/* This is not working ... So add a temp hack */
	return glade_gtk_get_set_function_hack (class, function_name);

	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (class), FALSE);
	g_return_val_if_fail (class->set_function == NULL, FALSE);
	g_return_val_if_fail (function_name != NULL, FALSE);
	
	if (!allsymbols)
		allsymbols = g_module_open (NULL, 0);
	
	if (!g_module_symbol (allsymbols, function_name,
			      (gpointer) &class->set_function)) {
		g_warning (_("We could not find the symbol \"%s\" while trying to load \"%s\""),
			   function_name, class->name);
		return FALSE;
	}

	g_assert (class->set_function);

	return TRUE;
}

static GList *
glade_xml_read_list (GladeXmlNode *node, const gchar *list_tag, const gchar *item_tag)
{
	GladeXmlNode *child;
	GList *list = NULL;
	gchar *item;

	child = glade_xml_search_child (node, list_tag);
	if (child == NULL)
		return NULL;

	child = glade_xml_node_get_children (child);

	for (; child != NULL; child = glade_xml_node_next (child)) {
		if (!glade_xml_node_verify (child, item_tag))
			return NULL;
		item = glade_xml_get_content (child);
		if (item != NULL)
			list = g_list_prepend (list, item);
	}

	list = g_list_reverse (list);

	return list;
}

static GladePropertyClass *
glade_property_class_new_from_node (GladeXmlNode *node, GladeWidgetClass *widget_class)
{
	GladePropertyClass *property_class; 
	GladeXmlNode *child;
	gchar *type;
	gchar *id;
	gchar *name;

	if (!glade_xml_node_verify (node, GLADE_TAG_PROPERTY))
		return NULL;

	id = glade_xml_get_property_string_required (node, GLADE_TAG_ID, widget_class->name);
	if (id == NULL)
		return NULL;

	property_class = glade_property_class_new ();

	/* We first load the members that can be present in a property defined by a ParamSpec */
	
	/* Get the Query */
	child = glade_xml_search_child (node, GLADE_TAG_QUERY);
	if (child != NULL)
		property_class->query = glade_query_new_from_node (child);

	
	/* Will this property go in the common tab ? */
	property_class->common   = glade_xml_get_property_boolean (node, GLADE_TAG_COMMON,  FALSE);
	property_class->optional = glade_xml_get_property_boolean (node, GLADE_TAG_OPTIONAL, FALSE);
	if (property_class->optional) {
		property_class->optional_default = glade_xml_get_property_boolean (node, GLADE_TAG_OPTIONAL_DEFAULT, FALSE);
	}

	/* Now get the list of signals that we should listen to */
	property_class->update_signals = glade_xml_read_list (node,
							      GLADE_TAG_UPDATE_SIGNALS,
							      GLADE_TAG_SIGNAL_NAME);

	/* If this property can't be set with g_object_set, get the workarround
	 * function
	 */
	child = glade_xml_search_child (node, GLADE_TAG_SET_FUNCTION);
	if (child != NULL) {
		gchar * content = glade_xml_get_content (child);
		glade_property_class_get_set_function (property_class, content);
		g_free (content);
	}
	/* If this property can't be get with g_object_get, get the workarround
	 * function
	 */
	child = glade_xml_search_child (node, GLADE_TAG_GET_FUNCTION);
	if (child != NULL) {
		gchar * content = glade_xml_get_content (child);
		glade_property_class_get_get_function (property_class, content);
		g_free (content);
	}

	
	/* Should we load this property from the ParamSpec ? 
	 * We can have a property like ... ParamSpec="TRUE"> 
	 * Or a child like <ParamSpec/>, but this will be deprecated
	 */
	if (glade_xml_get_property_boolean (node, GLADE_TAG_PARAM_SPEC, TRUE)) {
		gboolean retval;
		
		retval = glade_property_class_load_from_param_spec (id, property_class, widget_class, node);
		g_free (id);
		if (retval == FALSE) {
			glade_widget_class_dump_param_specs (widget_class);
			glade_widget_property_class_free (property_class);
			property_class = NULL;
		}

		return property_class;
	}

	name = glade_xml_get_property_string_required (node, GLADE_TAG_NAME, widget_class->name);
	if (name == NULL)
		return NULL;

	/* Ok, this property should not be loaded with ParamSpec */
	property_class->id    = id;
	property_class->name  = name;
	property_class->tooltip = glade_xml_get_value_string (node, GLADE_TAG_TOOLTIP);

	/* Get the type */
	type = glade_xml_get_value_string_required (node, GLADE_TAG_TYPE, widget_class->name);
	if (type == NULL)
		return NULL;
	property_class->type = glade_property_type_str_to_enum (type);
	g_free (type);
	if (property_class->type == GLADE_PROPERTY_TYPE_ERROR)
		return NULL;

	/* Get the Parameters */
	child = glade_xml_search_child (node, GLADE_TAG_PARAMETERS);
	if (child != NULL)
		property_class->parameters = glade_parameter_list_new_from_node (NULL, child);
	glade_parameter_get_boolean (property_class->parameters, "Optional", &property_class->optional);

	/* Get the choices */
	if (property_class->type == GLADE_PROPERTY_TYPE_ENUM) {
		GValue *gvalue;
		gchar *type_name;
		GType type;
		gchar *default_string;
		GladeXmlNode *child;
		child = glade_xml_search_child_required (node, GLADE_TAG_ENUMS);
		if (child == NULL)
			return NULL;
		property_class->choices = glade_choice_list_new_from_node (child);
		type_name = glade_xml_get_property_string_required (child, "EnumType", NULL);
		if (type_name == NULL)
			return NULL;
		type = g_type_from_name (type_name);
		g_return_val_if_fail (type != 0, NULL);
		gvalue = g_new0 (GValue, 1);
		g_value_init (gvalue, type);
		default_string = glade_xml_get_property_string (node, GLADE_TAG_DEFAULT);
		property_class->def = gvalue;
		return property_class;
	}

	/* If the property is an object Load it */
	if (property_class->type == GLADE_PROPERTY_TYPE_OBJECT) {
		child = glade_xml_search_child_required (node, GLADE_TAG_GLADE_WIDGET_CLASS);
		if (child == NULL)
			return NULL;
		property_class->child = glade_widget_class_new_from_node (child);
		g_print ("Loaded %s\n", property_class->child->name);
	}

	property_class->def = glade_property_class_get_default (node, property_class->type);
	
	return property_class;
}

GList *
glade_property_class_list_new_from_node (GladeXmlNode *node, GladeWidgetClass *class)
{
	GladePropertyClass *property_class;
	GladeXmlNode *child;
	GList *list;

	if (!glade_xml_node_verify (node, GLADE_TAG_PROPERTIES))
		return NULL;

	list = NULL;
	child = glade_xml_node_get_children (node);

	for (; child != NULL; child = glade_xml_node_next (child)) {
		if (!glade_xml_node_verify (child, GLADE_TAG_PROPERTY))
			return NULL;
		property_class = glade_property_class_new_from_node (child, class);
		if (property_class != NULL)
			list = g_list_prepend (list, property_class);
	}

	list = g_list_reverse (list);

	return list;
}















































/**
 * glade_property_class_create_label:
 * @class: The PropertyClass to create the name from
 * 
 * Creates a GtkLabel widget containing the name of the property.
 * This funcion is used by the property editor to create the label
 * of the property. 
 * 
 * Return Value: a GtkLabel
 **/
GtkWidget *
glade_property_class_create_label (GladePropertyClass *class)
{
	GtkWidget *label;
	gchar *text;

	text = g_strdup_printf ("%s :", class->name);
	label = gtk_label_new (text);
	g_free (text);

	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);

#warning This is not working
	glade_util_widget_set_tooltip (label, class->tooltip);
	
	return label;
}
