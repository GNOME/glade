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

#include <string.h>
#include <stdlib.h>
#include <gmodule.h>

#include "glade.h"
#include "glade-choice.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-parameter.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-gtk.h"
#include "glade-debug.h"

#if 0
typedef struct GladePropertyTypeTable {
	const gchar *xml_tag;
	GladePropertyType glade_type;
	GType *g_type;
};

/* #warning Implement me. */
#endif

static GladeChoice *
glade_property_class_choice_new_from_value (GEnumValue value)
{
	GladeChoice *choice;

	choice = glade_choice_new ();
	choice->name = g_strdup (value.value_nick);
	choice->id   = g_strdup (value.value_name);

	/*
	  g_debug(("Choice Id is %s\n", choice->id));
	  choice->symbol = g_strdup (value.value_name);
	*/
	
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

static GladeChoice *
glade_property_class_choice_clone (GladeChoice *choice)
{
	GladeChoice *clon;

	clon = glade_choice_new ();
	clon->id = g_strdup (choice->id);
	clon->name = g_strdup (choice->name);
	clon->type = choice->type;
	clon->value = choice->value;

	return clon;
}

static void
glade_property_class_choice_free (GladeChoice *choice)
{
	if (choice == NULL)
		return;

	g_free (choice->name);
	g_free (choice->id);
	g_free (choice);
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

GladePropertyType
glade_property_type_str_to_enum (const gchar *str)
{
	if (strcmp (str, GLADE_TAG_STRING) == 0)
		return GLADE_PROPERTY_TYPE_STRING;
	if (strcmp (str, GLADE_TAG_BOOLEAN) == 0)
		return GLADE_PROPERTY_TYPE_BOOLEAN;
	if (strcmp (str, GLADE_TAG_UNICHAR) == 0)
		return GLADE_PROPERTY_TYPE_UNICHAR;
	if (strcmp (str, GLADE_TAG_FLOAT) == 0)
		return GLADE_PROPERTY_TYPE_FLOAT;
	if (strcmp (str, GLADE_TAG_INTEGER) == 0)
		return GLADE_PROPERTY_TYPE_INTEGER;
	if (strcmp (str, GLADE_TAG_ENUM) == 0)
		return GLADE_PROPERTY_TYPE_ENUM;
	if (strcmp (str, GLADE_TAG_FLAGS) == 0)
		return GLADE_PROPERTY_TYPE_FLAGS;
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
	case GLADE_PROPERTY_TYPE_UNICHAR:
		return GLADE_TAG_UNICHAR;
	case GLADE_PROPERTY_TYPE_FLOAT:
		return GLADE_TAG_FLOAT;
	case GLADE_PROPERTY_TYPE_INTEGER:
		return GLADE_TAG_INTEGER;
	case GLADE_PROPERTY_TYPE_DOUBLE:
		return GLADE_TAG_DOUBLE;
	case GLADE_PROPERTY_TYPE_ENUM:
		return GLADE_TAG_ENUM;
	case GLADE_PROPERTY_TYPE_FLAGS:
		return GLADE_TAG_FLAGS;
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

static GladePropertyQuery *
glade_property_query_clone (GladePropertyQuery *query)
{
	GladePropertyQuery *clon;
	
	if (query == NULL)
		return NULL;

	clon = glade_property_query_new ();
	clon->window_title = g_strdup (query->window_title);
	clon->question = g_strdup (clon->question);

	return clon;
}

static void
glade_property_query_free (GladePropertyQuery *query)
{
	if (query == NULL)
		return;

	g_free (query->window_title);
	g_free (query->question);
	g_free (query);
}

GladePropertyClass *
glade_property_class_new (void)
{
	GladePropertyClass *property_class;

	property_class = g_new0 (GladePropertyClass, 1);
	property_class->type = GLADE_PROPERTY_TYPE_ERROR;
	property_class->id = NULL;

	/*
	  g_debug(("New property class %d. Id:%d\n",
	           GPOINTER_TO_INT (property_class), GPOINTER_TO_INT (property_class->id)));
	*/
	
	property_class->name = NULL;
	property_class->tooltip = NULL;
	property_class->parameters = NULL;
	property_class->choices = NULL;
	property_class->optional = FALSE;
	property_class->optional_default = TRUE;
	property_class->common = FALSE;
	property_class->packing = FALSE;
	property_class->get_default = FALSE;
	property_class->is_modified = FALSE;
	property_class->query = NULL;
	property_class->set_function = NULL;
	property_class->get_function = NULL;

	return property_class;
}

GladePropertyClass *
glade_property_class_clone (GladePropertyClass *property_class)
{
	GladePropertyClass *clon;

	if (property_class == NULL)
		return NULL;

	clon = g_new0 (GladePropertyClass, 1);
	if (clon == NULL)
	{
		g_warning ("Out of memory.");
		return NULL;
	}

	memcpy (clon, property_class, sizeof(GladePropertyClass));
	clon->id = g_strdup (clon->id);
	clon->name = g_strdup (clon->name);
	clon->tooltip = g_strdup (clon->tooltip);

	if (G_IS_VALUE (property_class->def))
	{
		clon->def = g_new0 (GValue, 1);
		g_value_init (clon->def, G_VALUE_TYPE (property_class->def));
		g_value_copy (property_class->def, clon->def);
	}

	if (clon->parameters)
	{
		GList *parameter;

		clon->parameters = g_list_copy (clon->parameters);

		for (parameter = clon->parameters; parameter != NULL; parameter = parameter->next)
			parameter->data = glade_parameter_clone ((GladeParameter*) parameter->data);
	}

	if (clon->choices)
	{
		GList *choice;

		clon->choices = g_list_copy (clon->choices);

		for (choice = clon->choices; choice != NULL; choice = choice->next)
			choice->data = glade_property_class_choice_clone ((GladeChoice*) choice->data);
	}

	clon->query = glade_property_query_clone (clon->query);

	/* ok, wtf? what is the child member for? */
	/* if (clon->child)
		clon->child = glade_widget_class_clone (clon->child); */

	if (clon->update_signals)
	{
		GList *signals;

		clon->update_signals = g_list_copy (clon->update_signals);

		for (signals = clon->update_signals; signals != NULL; signals = signals->next)
			signals->data = g_strdup ((const char *) signals->data);
	}

	return clon;
}

void
glade_property_class_free (GladePropertyClass *class)
{
	if (class == NULL)
		return;

	g_return_if_fail (GLADE_IS_PROPERTY_CLASS (class));
	
	g_free (class->id);
	g_free (class->tooltip);
	g_free (class->name);
	g_free (class->def);
	g_list_foreach (class->parameters, (GFunc) glade_parameter_free, NULL);
	g_list_free (class->parameters);
	g_list_foreach (class->choices, (GFunc) glade_property_class_choice_free, NULL);
	g_list_free (class->choices);
	glade_property_query_free (class->query);
	glade_widget_class_free (class->child);
	g_list_foreach (class->update_signals, (GFunc) g_free, NULL);
	g_list_free (class->update_signals);
	g_free (class);
}

static GladePropertyType
glade_property_class_get_type_from_spec (GParamSpec *spec)
{
	if (G_IS_PARAM_SPEC_INT (spec) || G_IS_PARAM_SPEC_UINT (spec)) {
		return GLADE_PROPERTY_TYPE_INTEGER;
	} else if (G_IS_PARAM_SPEC_FLOAT (spec)) {
		return GLADE_PROPERTY_TYPE_FLOAT;
	} else if (G_IS_PARAM_SPEC_BOOLEAN (spec)) {
		return GLADE_PROPERTY_TYPE_BOOLEAN;
	} else if (G_IS_PARAM_SPEC_UNICHAR (spec)) {
		return GLADE_PROPERTY_TYPE_UNICHAR;
	} else if (G_IS_PARAM_SPEC_STRING (spec)) {
		/* FIXME: We should solve the "name" conflict with a better solution */
		if (!g_ascii_strcasecmp ( spec->name, "name"))
			return GLADE_PROPERTY_TYPE_ERROR;
		else
			return GLADE_PROPERTY_TYPE_STRING;
	} else if (G_IS_PARAM_SPEC_ENUM (spec)) {
		return GLADE_PROPERTY_TYPE_ENUM;
	} else if (G_IS_PARAM_SPEC_FLAGS (spec)) {
		/* FIXME: We should implement the "events" property */
		if (g_ascii_strcasecmp (spec->name, "events") == 0)
			return GLADE_PROPERTY_TYPE_ERROR;
		else
			return GLADE_PROPERTY_TYPE_FLAGS;
	} else if (G_IS_PARAM_SPEC_DOUBLE (spec)) {
		return GLADE_PROPERTY_TYPE_DOUBLE;
	} else if (G_IS_PARAM_SPEC_LONG (spec)) {
		g_warning ("Long not implemented\n");
	} else if (G_IS_PARAM_SPEC_UCHAR (spec)) {
		g_warning ("uchar not implemented\n");
	} else if (G_IS_PARAM_SPEC_OBJECT (spec)) {
		return GLADE_PROPERTY_TYPE_OBJECT;
	} else if (G_IS_PARAM_SPEC_BOXED (spec)) {
		/* FIXME: Implement at least the special case of when the boxed type is GdkColor */
		return GLADE_PROPERTY_TYPE_ERROR;
	} else {
		/* FIXME: We should implement the "events" property */
		if (g_ascii_strcasecmp (spec->name, "user-data")) {
			g_warning ("Could not determine GladePropertyType from spec (%s) (%s)",
				   G_PARAM_SPEC_TYPE_NAME (spec), g_type_name (G_PARAM_SPEC_VALUE_TYPE (spec)));
		}
	}

	return GLADE_PROPERTY_TYPE_ERROR;
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
	case GLADE_PROPERTY_TYPE_FLAGS:
		g_value_init (value, spec->value_type);
		g_value_set_flags (value, G_PARAM_SPEC_FLAGS (spec)->default_value);
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
	case GLADE_PROPERTY_TYPE_UNICHAR:
		g_value_init (value, G_TYPE_UINT);
		g_value_set_uint (value, G_PARAM_SPEC_UNICHAR (spec)->default_value);
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

gchar *
glade_property_class_make_string_from_gvalue (GladePropertyClass *property_class,
					      const GValue *value)
{
	gchar *string = NULL;
	GladePropertyType type = property_class->type;

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
	case GLADE_PROPERTY_TYPE_UNICHAR:
		string = g_malloc (7);
		g_unichar_to_utf8 (g_value_get_uint (value), string);
		*g_utf8_next_char(string) = '\0';
		break;
	case GLADE_PROPERTY_TYPE_STRING:
		string = g_strdup (g_value_get_string (value));
		break;
	case GLADE_PROPERTY_TYPE_ENUM:
		{
			GList *list;
			GladeChoice *choice;

			list = g_list_nth (property_class->choices, g_value_get_enum (value));
			if (list != NULL) {
				choice = (GladeChoice *) list->data;
				string = g_strdup (choice->id);
			} else {
				g_warning ("Could not found choice #%d\n", g_value_get_enum (value));
			}
		}
		break;
	default:
		g_warning ("Could not make string from gvalue for type %s\n",
			 glade_property_type_enum_to_string (type));
	}

	return string;
}

GValue *
glade_property_class_make_gvalue_from_string (GladePropertyClass *property_class,
					      const gchar *string)
{
	GValue *value = g_new0 (GValue, 1);
	GladePropertyType type = property_class->type;
	
	switch (type) {
	case GLADE_PROPERTY_TYPE_INTEGER:
		g_value_init (value, G_TYPE_INT);
		g_value_set_int (value, atoi (string));
		break;
	case GLADE_PROPERTY_TYPE_FLOAT:
		g_value_init (value, G_TYPE_FLOAT);
		g_value_set_float (value, (float) atof (string));
		break;
	case GLADE_PROPERTY_TYPE_BOOLEAN:
		g_value_init (value, G_TYPE_BOOLEAN);
		if (strcmp (string, GLADE_TAG_TRUE) == 0)
			g_value_set_boolean (value, TRUE);
		else
			g_value_set_boolean (value, FALSE);
		break;
	case GLADE_PROPERTY_TYPE_UNICHAR:
		g_value_init (value, G_TYPE_UINT);
		g_value_set_uint (value, g_utf8_get_char (string));
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
		{
			GList *list;
			GladeChoice *choice;
			gint i = 0;
			gboolean found = FALSE;

			list = g_list_first (property_class->choices);
			while (list != NULL && !found) {
				choice = (GladeChoice *) list->data;
				if (!g_ascii_strcasecmp (string, choice->id)) {
					g_value_init (value, property_class->def->g_type);
					g_value_set_enum (value, i);
					found = TRUE;
				}

				list = g_list_next (list);
				i++;
			}
			if (!found) {
				g_warning ("Could not found choice for %s\n", string);
				g_free (value);
				value = NULL;
			}
		}
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

GladePropertyClass *
glade_property_class_new_from_spec (GParamSpec *spec)
{
	GladePropertyClass *property_class;

	property_class = glade_property_class_new ();

	property_class->type = glade_property_class_get_type_from_spec (spec);
	property_class->id = g_strdup (spec->name);
	property_class->name = g_strdup (g_param_spec_get_nick (spec));
	property_class->tooltip = g_strdup (g_param_spec_get_blurb (spec));
	property_class->def = glade_property_class_get_default_from_spec (spec, property_class, NULL);

	g_return_val_if_fail (property_class->name != NULL, NULL);

	switch (property_class->type) {
	case GLADE_PROPERTY_TYPE_ENUM:
		property_class->choices = glade_property_class_get_choices_from_spec (spec);
		break;
	case GLADE_PROPERTY_TYPE_FLAGS:
		break;
	case GLADE_PROPERTY_TYPE_STRING:
		break;
	case GLADE_PROPERTY_TYPE_INTEGER:
	case GLADE_PROPERTY_TYPE_FLOAT:
	case GLADE_PROPERTY_TYPE_DOUBLE:
		property_class->parameters = glade_property_get_parameters_numeric (spec, property_class);
		break;
	case GLADE_PROPERTY_TYPE_BOOLEAN:
		break;
	case GLADE_PROPERTY_TYPE_UNICHAR:
		break;
	case GLADE_PROPERTY_TYPE_OTHER_WIDGETS:
		break;
	case GLADE_PROPERTY_TYPE_OBJECT:
		break;
	case GLADE_PROPERTY_TYPE_ERROR:
		break;
	}

	return property_class;
}

static GValue *
glade_property_class_get_default (GladeXmlNode *node, GladePropertyClass *property_class)
{
	GValue *value;
	gchar *temp;

	temp =	glade_xml_get_property_string (node, GLADE_TAG_DEFAULT);

	if (!temp) {
		/* g_debug(("Temp is NULL, we dont' have a default\n")) */;
		return NULL;
	}

	value = glade_property_class_make_gvalue_from_string (property_class, temp);

	g_free (temp);

	return value;
}

#if 0 // do we still need these 2 ?
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
	case GLADE_PROPERTY_TYPE_FLAGS:
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
	case GLADE_PROPERTY_TYPE_UNICHAR:
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
	class->name    = g_strdup (g_param_spec_get_nick (spec));
	class->tooltip = g_strdup (g_param_spec_get_blurb (spec));
	class->type    = glade_property_class_get_type_from_spec (spec);

	if (class->type == GLADE_PROPERTY_TYPE_ERROR) {
		g_free (class->id);
		g_free (class->name);
		g_free (class->tooltip);
		return FALSE;
	}
	
	if (class->type == GLADE_PROPERTY_TYPE_ENUM)
		class->choices = glade_property_class_get_choices_from_spec (spec);

	/* We want to use the parm spec default only when the xml files do not provide a
	 * default value
	 */
	def = glade_xml_get_property_string (node, GLADE_TAG_DEFAULT);
	if (def) {
		class->def = glade_property_class_get_default (node, class);
		g_free (def);
	} else {
		class->def = glade_property_class_get_default_from_spec (spec, class, node);
	}
	class->parameters = glade_property_class_get_parameters_from_spec (spec, class, node);
	
	return TRUE;
}
#endif

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

static void
glade_property_class_update_from_node (GladeXmlNode *node,
				       GladeWidgetClass *widget_class,
				       GladePropertyClass **property_class)
{
	GladeXmlNode *child;
	GladePropertyClass *pproperty_class;
	gchar *type;
	gchar *id;
	gchar *name;

	g_return_if_fail (glade_xml_node_verify (node, GLADE_TAG_PROPERTY));

	/* If we have a property like ... Disabled="TRUE"> we should
	 * remove this property
	 */
	if ( glade_xml_get_property_boolean (node, GLADE_TAG_DISABLED, FALSE)) {
		if (*property_class != NULL) {
			glade_property_class_free (*property_class);
			*property_class = NULL;
		}

		return;
	}

	id = glade_xml_get_property_string_required (node, GLADE_TAG_ID, widget_class->name);
	if (id == NULL)
		return;

	pproperty_class = *property_class;
	
	/* If we have a property like ... ParamSpec="FALSE"> we should
	 * overwrite the ParamSpec prefs.
	 * If the property_class == NULL we should create it from .xml file
	 */
	if ( !glade_xml_get_property_boolean (node, GLADE_TAG_PARAM_SPEC, TRUE) || pproperty_class == NULL) {
		name = glade_xml_get_property_string_required (node, GLADE_TAG_NAME, widget_class->name);
		if (name == NULL) {
			return;
		}

		pproperty_class = glade_property_class_new ();

		/* Ok, this property should not be loaded with ParamSpec */
		pproperty_class->id    = id;
		pproperty_class->name  = name;
		pproperty_class->tooltip = glade_xml_get_value_string (node, GLADE_TAG_TOOLTIP);

		/* Get the type */
		type = glade_xml_get_value_string_required (node, GLADE_TAG_TYPE, widget_class->name);
		if (type == NULL) {
			glade_property_class_free (pproperty_class);
			pproperty_class = NULL;
			return;
		}
		pproperty_class->type = glade_property_type_str_to_enum (type);
		g_free (type);
		if (pproperty_class->type == GLADE_PROPERTY_TYPE_ERROR) {
			glade_property_class_free (pproperty_class);
			pproperty_class = NULL;
			return;
		}

		/* Get the Parameters */
		child = glade_xml_search_child (node, GLADE_TAG_PARAMETERS);
		if (child != NULL)
			pproperty_class->parameters = glade_parameter_list_new_from_node (NULL, child);
		glade_parameter_get_boolean (pproperty_class->parameters, "Optional", &pproperty_class->optional);

		/* Get the choices */
		if (pproperty_class->type == GLADE_PROPERTY_TYPE_ENUM) {
			GValue *gvalue;
			gchar *type_name;
			GType type;
			gchar *default_string;
			GladeXmlNode *child;
			child = glade_xml_search_child_required (node, GLADE_TAG_ENUMS);
			if (child == NULL) {
				glade_property_class_free (pproperty_class);
				pproperty_class = NULL;
				return;
			}
			
			pproperty_class->choices = glade_choice_list_new_from_node (child);
			type_name = glade_xml_get_property_string_required (child, "EnumType", NULL);
			if (type_name == NULL) {
				glade_property_class_free (pproperty_class);
				pproperty_class = NULL;
				return;
			}
			
			type = g_type_from_name (type_name);
			if (! (type != 0)) {
				glade_property_class_free (pproperty_class);
				pproperty_class = NULL;
				return;
			}
			gvalue = g_new0 (GValue, 1);
			g_value_init (gvalue, type);
			default_string = glade_xml_get_property_string (node, GLADE_TAG_DEFAULT);
			pproperty_class->def = gvalue;
			glade_property_class_free (*property_class);
			*property_class = pproperty_class;
		}
		else {
			/* If the property is an object Load it */
			if (pproperty_class->type == GLADE_PROPERTY_TYPE_OBJECT) {
				child = glade_xml_search_child_required (node, GLADE_TAG_GLADE_WIDGET_CLASS);
				if (child == NULL) {
					glade_property_class_free (pproperty_class);
					pproperty_class = NULL;
					return;
				}
			
				pproperty_class->child = glade_widget_class_new_from_node (child);
			}

			pproperty_class->def = glade_property_class_get_default (node, pproperty_class);
			glade_property_class_free (*property_class);
			*property_class = pproperty_class;
		}
	
	} else {
		GValue *tmp;
		tmp = glade_property_class_get_default (node, pproperty_class);
		if (tmp != NULL) {
			g_free (pproperty_class->def);
			pproperty_class->def = tmp;
		}
	}

	/* we're modifying the default values of this property */
	pproperty_class->is_modified = TRUE;

	/* Get the Query */
	child = glade_xml_search_child (node, GLADE_TAG_QUERY);
	if (child != NULL)
		pproperty_class->query = glade_query_new_from_node (child);

	/* Will this property go in the common tab ? */
	pproperty_class->common   = glade_xml_get_property_boolean (node, GLADE_TAG_COMMON,  FALSE);
	pproperty_class->optional = glade_xml_get_property_boolean (node, GLADE_TAG_OPTIONAL, FALSE);
	if (pproperty_class->optional) {
		pproperty_class->optional_default = glade_xml_get_property_boolean (node,
									GLADE_TAG_OPTIONAL_DEFAULT, FALSE);
	}

	/* Now get the list of signals that we should listen to */
	pproperty_class->update_signals = glade_xml_read_list (node,
							      GLADE_TAG_UPDATE_SIGNALS,
							      GLADE_TAG_SIGNAL_NAME);
	
	/* If this property can't be set with g_object_set, get the workarround
	 * function
	 */
	child = glade_xml_search_child (node, GLADE_TAG_SET_FUNCTION);
	if (child != NULL) {
		gchar * content = glade_xml_get_content (child);
		glade_property_class_get_set_function (pproperty_class, content);
		g_free (content);
	}
	/* If this property can't be get with g_object_get, get the workarround
	 * function
	 */
	child = glade_xml_search_child (node, GLADE_TAG_GET_FUNCTION);
	if (child != NULL) {
		gchar * content = glade_xml_get_content (child);
		glade_property_class_get_get_function (pproperty_class, content);
		g_free (content);
	}

	return;
}

void
glade_property_class_list_add_from_node (GladeXmlNode *node,
					 GladeWidgetClass *widget_class,
					 GList **properties)
{
	GladePropertyClass *property_class = NULL;
	GladeXmlNode *child;
	GList *list_element;
	gchar *buff;

	if (!glade_xml_node_verify (node, GLADE_TAG_PROPERTIES))
		return;
	
	child = glade_xml_node_get_children (node);

	for (; child != NULL; child = glade_xml_node_next (child)) {
		if (!glade_xml_node_verify (child, GLADE_TAG_PROPERTY))
			return;
		
		list_element = g_list_last (*properties);
		buff = glade_xml_get_property_string (child, GLADE_TAG_ID);
		while (list_element != NULL && property_class == NULL) {
			const char *id = ((GladePropertyClass *) list_element->data)->id;
			if (!g_ascii_strcasecmp (id, buff))
				property_class = (GladePropertyClass *) list_element->data;
			else 
				list_element = g_list_previous (list_element);
		}
		
		glade_property_class_update_from_node (child, widget_class, &property_class);
		if (list_element != NULL)
			*properties = g_list_delete_link (*properties, list_element);
		if (property_class != NULL) {
			*properties = g_list_prepend (*properties, property_class);
			property_class = NULL;
		}
		g_free (buff);
	}

	*properties = g_list_reverse (*properties);

}

