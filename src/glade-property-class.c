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


#include "glade.h"
#include "glade-xml-utils.h"
#include "glade-choice.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-parameter.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include <string.h>

GladePropertyType
glade_property_type_str_to_enum (const gchar *str)
{
	if (strcmp (str, GLADE_TAG_TEXT) == 0)
		return GLADE_PROPERTY_TYPE_TEXT;
	if (strcmp (str, GLADE_TAG_BOOLEAN) == 0)
		return GLADE_PROPERTY_TYPE_BOOLEAN;
	if (strcmp (str, GLADE_TAG_FLOAT) == 0)
		return GLADE_PROPERTY_TYPE_FLOAT;
	if (strcmp (str, GLADE_TAG_INTEGER) == 0)
		return GLADE_PROPERTY_TYPE_INTEGER;
	if (strcmp (str, GLADE_TAG_CHOICE) == 0)
		return GLADE_PROPERTY_TYPE_CHOICE;
	if (strcmp (str, GLADE_TAG_OTHER_WIDGETS) == 0)
		return GLADE_PROPERTY_TYPE_OTHER_WIDGETS;

	g_warning ("Could not determine the property type from *%s*\n", str);

	return GLADE_PROPERTY_TYPE_ERROR;
}

const gchar *
glade_property_type_enum_to_str (GladePropertyType type)
{
	switch (type) {
	case GLADE_PROPERTY_TYPE_TEXT:
		return GLADE_TAG_TEXT;
	case GLADE_PROPERTY_TYPE_BOOLEAN:
		return GLADE_TAG_BOOLEAN;
	case GLADE_PROPERTY_TYPE_FLOAT:
		return GLADE_TAG_FLOAT;
	case GLADE_PROPERTY_TYPE_INTEGER:
		return GLADE_TAG_INTEGER;
	case GLADE_PROPERTY_TYPE_CHOICE:
		return GLADE_TAG_CHOICE;
	case GLADE_PROPERTY_TYPE_OTHER_WIDGETS:
		return GLADE_TAG_OTHER_WIDGETS;
	default:
		break;
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
glade_query_new_from_node (xmlNodePtr node)
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

static GladePropertyClass *
glade_property_class_new (void)
{
	GladePropertyClass *property_class;

	property_class = g_new0 (GladePropertyClass, 1);
	property_class->type = GLADE_PROPERTY_TYPE_ERROR;
	property_class->name = NULL;
	property_class->tooltip = NULL;
	property_class->gtk_arg = NULL;
	property_class->parameters = NULL;
	property_class->choices = NULL;
	property_class->optional = FALSE;
	property_class->query = NULL;

	return property_class;
}

static void
glade_property_class_get_specs (GladeWidgetClass *class, GParamSpec ***specs, gint *n_specs)
{
	GObjectClass *object_class;
	GType type;

	type = glade_widget_class_get_type (class);
	g_type_class_ref (type); /* hmm */
	 /* We count on the fact we have an instance, or else we'd have
	  * touse g_type_class_ref ();
	  */
	object_class = g_type_class_peek (type);
	if (object_class == NULL) {
		g_warning ("Class peek failed\n");
		*specs = NULL;
		*n_specs = 0;
		return;
	}

	*specs = g_object_class_list_properties (object_class, n_specs);
}

GParamSpec *
glade_property_class_find_spec (GladeWidgetClass *class, const gchar *name)
{
	GParamSpec **specs = NULL;
	GParamSpec *spec;
	gint n_specs = 0;
	gint i;

	glade_property_class_get_specs (class, &specs, &n_specs);

#if 0
	g_print ("Dumping specs for %s\n\n", class->name);
	for (i = 0; i < n_specs; i++) {
		spec = specs[i];
		g_print ("%02d - %s\n", i, spec->name); 
	}
#endif	
	
	for (i = 0; i < n_specs; i++) {
		spec = specs[i];

		if (!spec || !spec->name) {
			g_warning ("Spec does not have a valid name, or invalid spec");
			return NULL;
		}

		if (strcmp (spec->name, name) == 0)
			return spec;
	}

	return NULL;
}
			

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
		return GLADE_PROPERTY_TYPE_TEXT;
	case G_TYPE_PARAM_ENUM:
		return GLADE_PROPERTY_TYPE_CHOICE;
	case G_TYPE_PARAM_DOUBLE:
		g_warning ("Double not implemented\n");
		break;
	case G_TYPE_PARAM_LONG:
		g_warning ("Long not implemented\n");
		break;
	case G_TYPE_PARAM_UCHAR:
		g_warning ("uchar not implemented\n");
		break;
	default:
		g_warning ("Could not determine GladePropertyType from spec (%d)",
			   G_PARAM_SPEC_TYPE (spec));
	}

	return GLADE_PROPERTY_TYPE_ERROR;
}

static GladeChoice *
glade_property_class_get_choice_from_value (GEnumValue value)
{
	GladeChoice *choice;

	choice = glade_choice_new ();
	choice->name = g_strdup (value.value_nick);
	choice->symbol = g_strdup (value.value_name);
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
		choice = glade_property_class_get_choice_from_value (value);
		if (choice)
			list = g_list_prepend (list, choice);
	}
	list = g_list_reverse (list);

	return list;
}

static GList *
glade_property_get_parameters_boolean (GParamSpec *spec,
				       GladePropertyClass *class)
{
	GladeParameter *parameter;
	gint def;
	
	g_return_val_if_fail (G_IS_PARAM_SPEC_BOOLEAN (spec), NULL);

	def = (gint) G_PARAM_SPEC_BOOLEAN (spec)->default_value;
	
	parameter = glade_parameter_new ();
	parameter->key = g_strdup ("Default");
	parameter->value = def ? g_strdup (GLADE_TAG_TRUE) : g_strdup (GLADE_TAG_FALSE); 

	return g_list_prepend (NULL, parameter);
}

static GList *
glade_property_get_parameters_integer (GParamSpec *spec,
				       GladePropertyClass *class)
{
	GladeParameter *parameter;
	gint def;
	
	g_return_val_if_fail (G_IS_PARAM_SPEC_INT  (spec) |
			      G_IS_PARAM_SPEC_UINT (spec), NULL);

	if (G_IS_PARAM_SPEC_INT (spec))
		def = (gint) G_PARAM_SPEC_INT (spec)->default_value;
	else
		def = (gint) G_PARAM_SPEC_UINT (spec)->default_value;
	
	parameter = glade_parameter_new ();
	parameter->key = g_strdup ("Default");
	parameter->value = g_strdup_printf ("%i", def);

	return g_list_prepend (NULL, parameter);
}

static GList *
glade_property_get_parameters_choice (GParamSpec *spec,
				      GladePropertyClass *class)
{
	GladeParameter *parameter;
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

	parameter = glade_parameter_new ();
	parameter->key = g_strdup ("Default");
	parameter->value = g_strdup (choice->symbol);

	/* The "list" pointer is now used for something else */
	list = g_list_prepend (NULL, parameter);
	
	return list;
}
	
static GList *
glade_property_class_get_parameters_from_spec (GParamSpec *spec,
					       GladePropertyClass *class,
					       xmlNodePtr node)
{
	GList *parameters = NULL;
	xmlNodePtr child;

	switch (class->type) {
	case GLADE_PROPERTY_TYPE_CHOICE:
		parameters = glade_property_get_parameters_choice (spec,
								  class);
		break;
	case GLADE_PROPERTY_TYPE_TEXT:
		break;
	case GLADE_PROPERTY_TYPE_INTEGER:
		parameters = glade_property_get_parameters_integer (spec,
								    class);
		break;
	case GLADE_PROPERTY_TYPE_FLOAT:
		break;
	case GLADE_PROPERTY_TYPE_BOOLEAN:
		parameters = glade_property_get_parameters_boolean (spec,
								    class);
		break;
	default:
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


static GladePropertyClass *
glade_property_class_new_from_param_spec (const gchar *name,
					  GladeWidgetClass *widget_class,
					  xmlNodePtr node)
{
	GladePropertyClass *class;
	GParamSpec *spec;

	spec = glade_property_class_find_spec (widget_class, name);

	if (spec == NULL) {
		g_warning ("Could not create a property class from a param spec for *%s* with name *%s*\n",
			   widget_class->name, name);
		return NULL;
	}

	class = glade_property_class_new ();
	class->name    = g_strdup (spec->nick);
	class->tooltip = g_strdup (spec->blurb);
	class->gtk_arg = g_strdup (name);
	class->type    = glade_property_class_get_type_from_spec (spec);

	if (class->type == GLADE_PROPERTY_TYPE_CHOICE)
		class->choices = glade_property_class_get_choices_from_spec (spec);

	class->parameters = glade_property_class_get_parameters_from_spec (spec, class, node);
	
	return class;
}
				  

static GladePropertyClass *
glade_property_class_new_from_node (xmlNodePtr node, GladeWidgetClass *widget_class)
{
	GladePropertyClass *property_class; 
	xmlNodePtr child;
	gchar *type;
	gchar *name;

	if (!glade_xml_node_verify (node, GLADE_TAG_PROPERTY))
		return NULL;

	name = glade_xml_get_value_string_required (node, GLADE_TAG_NAME, NULL);

	if (name == NULL)
		return NULL;

	/* Should we load this property from the ParamSpec ? */
	child = glade_xml_search_child (node, GLADE_TAG_PARAM_SPEC);
	if (child) {
		property_class = glade_property_class_new_from_param_spec (name, widget_class, node);
		g_free (name);
		return property_class;
	}

	property_class = glade_property_class_new ();
	property_class->name    = name;
	
	property_class->tooltip = glade_xml_get_value_string (node, GLADE_TAG_TOOLTIP);
	property_class->gtk_arg = glade_xml_get_value_string (node, GLADE_TAG_GTKARG);

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

	/* Get the Query */
	child = glade_xml_search_child (node, GLADE_TAG_QUERY);
	if (child != NULL)
		property_class->query = glade_query_new_from_node (child);
	
	/* Get the choices */
	if (property_class->type == GLADE_PROPERTY_TYPE_CHOICE) {
		child = glade_xml_search_child_required (node, GLADE_TAG_CHOICES);
		if (child == NULL)
			return NULL;
		property_class->choices = glade_choice_list_new_from_node (child);
	}

	
	return property_class;
}


GList *
glade_property_class_list_new_from_node (xmlNodePtr node, GladeWidgetClass *class)
{
	GladePropertyClass *property_class;
	xmlNodePtr child;
	GList *list;

	if (!glade_xml_node_verify (node, GLADE_TAG_PROPERTIES))
		return NULL;

	list = NULL;
	child = node->children;

	while (child != NULL) {
		skip_text (child);
		if (!glade_xml_node_verify (child, GLADE_TAG_PROPERTY))
			return NULL;
		property_class = glade_property_class_new_from_node (child, class);
		if (property_class == NULL)
			return NULL;
		list = g_list_prepend (list, property_class);
		child = child->next;
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
	
	return label;
}




