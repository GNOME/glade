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

#include <stdlib.h> /* for atoi and atof */

#include "glade.h"
#include "glade-choice.h"
#include "glade-widget.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-parameter.h"
#include "glade-widget-class.h"

static GladeProperty *
glade_property_new (void)
{
	GladeProperty *property;

	property = g_new0 (GladeProperty, 1);
	property->class = NULL;
	property->value = NULL;
	property->enabled = TRUE;

	return property;
}


static GladeProperty *
glade_property_new_from_string (const gchar *string, GladePropertyClass *class)
{
	GladeProperty *property;
	gchar *value = NULL;
	gfloat float_val;
	gint int_val;

	/* move somewhere else */
	GladeChoice *choice;
	GList *list;

	property = glade_property_new ();
	
	property->class = class;

	switch (class->type) {
	case GLADE_PROPERTY_TYPE_BOOLEAN:
		if (string == NULL)
			value = g_strdup (GLADE_TAG_FALSE);
		else if (strcmp (string, GLADE_TAG_TRUE) == 0)
			value = g_strdup (GLADE_TAG_TRUE);
		else if (strcmp (string, GLADE_TAG_FALSE) == 0)
			value = g_strdup (GLADE_TAG_FALSE);
		else {
			g_warning ("Invalid default tag for boolean %s (%s/%s)",
				   string, GLADE_TAG_TRUE, GLADE_TAG_FALSE);
			value = g_strdup (GLADE_TAG_FALSE);
		}
		break;
	case GLADE_PROPERTY_TYPE_FLOAT:
		if (string)
			float_val = atof (string);
		else
			float_val = 0;
		value = g_strdup_printf ("%g", float_val);
		glade_parameter_get_boolean (class->parameters,
					     "OptionalDefault",
					     &property->enabled);
		break;
	case GLADE_PROPERTY_TYPE_INTEGER:
		if (string)
			int_val = atoi (string);
		else
			int_val = 0;
		value = g_strdup_printf ("%i", int_val);
		glade_parameter_get_boolean (class->parameters,
					     "OptionalDefault",
					     &property->enabled);
		break;
	case GLADE_PROPERTY_TYPE_TEXT:
		if (string)
			value = g_strdup (string);
		else
			value = g_strdup ("");
		break;
	case GLADE_PROPERTY_TYPE_CHOICE:
		list = class->choices;
		if (string != NULL) {
			for (;list != NULL; list = list->next) {
				choice = list->data;
				if (strcmp (choice->symbol, string) == 0)
					break;
			}
			if (list != NULL) {
				value = g_strdup (string);
				break;
			}
		}
		list = class->choices;
		if (list == NULL) {
			g_warning ("class->choices is NULL. This should not happen\n");
			value = g_strdup ("ERROR");
			break;
		}
		choice = list->data;
		g_warning ("Invalid default tag \"%s\" for property \"%s\", setting deafult to %s (%s)",
			   choice->name, class->name,
			   choice->symbol, string);
		value = g_strdup (choice->symbol);
		break;
	case GLADE_PROPERTY_TYPE_OTHER_WIDGETS:
		value = g_strdup ("");
		break;
	case GLADE_PROPERTY_TYPE_ERROR:
	default:
		g_warning ("Invalid Glade property type (%d)\n", class->type);
		break;
	}

	property->value = value;
	
	return property;
}

GList *
glade_property_list_new_from_widget_class (GladeWidgetClass *class,
					   GladeWidget *widget)
{
	GladePropertyClass *property_class;
	GladeProperty *property;
	GList *list = NULL;
	GList *new_list = NULL;
	gchar *def;

	list = class->properties;
	for (; list != NULL; list = list->next) {
		property_class = list->data;
		def = NULL;
		glade_parameter_get_string (property_class->parameters,
					    "Default", &def);
		property = glade_property_new_from_string (def, property_class);
		if (property == NULL)
			continue;

		property->widget = widget;

		if (def)
			g_free (def);

		new_list = g_list_prepend (new_list, property);
	}


	new_list = g_list_reverse (new_list);
	
	return new_list;
}


GladeProperty *
glade_property_get_from_name (GList *settings_list, const gchar *name)
{
	GList *list;
	GladeProperty *property;

	g_return_val_if_fail (name != NULL, NULL);
	
	list = settings_list;
	for (; list != NULL; list = list->next) {
		property = list->data;
		if (strcmp (property->class->name, name) == 0)
			return property;
	}

	return NULL;
}

		











void
glade_property_changed_text (GladeProperty *property,
			     const gchar *text)
{
	gchar *temp;
	
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->value != NULL);
	g_return_if_fail (property->widget != NULL);
	g_return_if_fail (property->widget->widget != NULL);
	
	temp = property->value;
	property->value = g_strdup (text);
	g_free (temp);

	if (property->class->gtk_arg == NULL) {
		g_print ("I don't have a gtk arg for %s\n", property->class->name);
		return;
	}
	
	gtk_object_set (GTK_OBJECT (property->widget->widget),
			property->class->gtk_arg, property->value, NULL);
}

void
glade_property_changed_integer (GladeProperty *property, gint val)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->value != NULL);

	g_free (property->value);
	property->value = g_strdup_printf ("%i", val);

	g_print ("Setting the value to %s\n", property->value);
	gtk_object_set (GTK_OBJECT (property->widget->widget),
			property->class->gtk_arg, property->value, NULL);
}

void
glade_property_changed_float (GladeProperty *property, gfloat val)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->value != NULL);

	g_free (property->value);
	property->value = g_strdup_printf ("%g", val);
}

void
glade_property_changed_boolean (GladeProperty *property, gboolean val)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->value != NULL);

	g_free (property->value);
	property->value = g_strdup_printf ("%s", val ? GLADE_TAG_TRUE : GLADE_TAG_FALSE);

	gtk_object_set (GTK_OBJECT (property->widget->widget),
			property->class->gtk_arg, val, NULL);
}

void
glade_property_changed_choice (GladeProperty *property, GladeChoice *choice)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->value != NULL);
	g_return_if_fail (choice != NULL);

	g_free (property->value);
	property->value = g_strdup_printf ("%s", choice->symbol);
}
	


const gchar *
glade_property_get_text (GladeProperty *property)
{
	g_return_val_if_fail (property != NULL, NULL);
	g_return_val_if_fail (property->value != NULL, NULL);

	return property->value;
}
	
gint
glade_property_get_integer (GladeProperty *property)
{
	g_return_val_if_fail (property != NULL, 0);
	g_return_val_if_fail (property->value != NULL, 0);
	
	return atoi (property->value);	
}

gfloat
glade_property_get_float (GladeProperty *property)
{
	g_return_val_if_fail (property != NULL, 0.0);
	g_return_val_if_fail (property->value != NULL, 0.0);
	
	return atof (property->value);
}
	

gboolean
glade_property_get_boolean (GladeProperty *property)
{
	g_return_val_if_fail (property != NULL, FALSE);
	g_return_val_if_fail (property->value != NULL, FALSE);

	return (strcmp (property->value, GLADE_TAG_TRUE) == 0);
}	
	
GladeChoice *
glade_property_get_choice (GladeProperty *property)
{
	GladeChoice *choice = NULL;
	GList *list;
	
	g_return_val_if_fail (property != NULL, NULL);
	g_return_val_if_fail (property->value != NULL, NULL);

	list = property->class->choices;
	for (; list != NULL; list = list->next) {
		choice = list->data;
		if (strcmp (choice->symbol, property->value) == 0)
			break;
	}
	if (list == NULL)
		g_warning ("Cant find the GladePropertyChoice selected\n");

	return choice;
}

GladeProperty *
glade_property_get_from_class (GladeWidget *widget,
			       GladePropertyClass *class)
{
	GladeProperty *property;
	GList *list;

	list = widget->properties;
	for (; list != NULL; list = list->next) {
		property = list->data;
		if (property->class == class)
			return property;
	}

	g_warning ("Could not find property from class\n");

	return NULL;
}


void
glade_property_query_result_set_int (GladePropertyQueryResult *result,
				     const gchar *key,
				     gint value)
{
	g_return_if_fail (result != NULL);
	g_return_if_fail (key != NULL);

	g_hash_table_insert (result->hash, (gchar *)key,
			     GINT_TO_POINTER (value));

}

void
glade_property_query_result_get_int (GladePropertyQueryResult *result,
				     const gchar *key,
				     gint *return_value)
{
	g_return_if_fail (result != NULL);
	g_return_if_fail (key != NULL);

	*return_value = GPOINTER_TO_INT (g_hash_table_lookup (result->hash, key));
}

GladePropertyQueryResult *
glade_property_query_result_new (void)
{
	GladePropertyQueryResult *result;

	result = g_new0 (GladePropertyQueryResult, 1);
	result->hash = g_hash_table_new (g_str_hash, g_str_equal);

	return result;
}

void
glade_property_query_result_destroy (GladePropertyQueryResult *result)
{
	g_return_if_fail (result != NULL);

	g_hash_table_destroy (result->hash);
	result->hash = NULL;
	
	g_free (result);
}
