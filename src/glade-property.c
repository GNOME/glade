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
#include <string.h>

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
	property->child = NULL;

	return property;
}


/* We are recursing so add the prototype. Don't you love C ? */
static GList * glade_property_list_new_from_list (GList *list, GladeWidget *widget);

GladeProperty *
glade_property_new_from_class (GladePropertyClass *class, GladeWidget *widget)
{
	GladeProperty *property;
	gchar *value = NULL;
	gfloat float_val;
	gint int_val;
	gchar *string = class->def;

	/* move somewhere else */
	GladeChoice *choice;
	GList *list;


	property = glade_property_new ();
	
	property->class = class;

	/* For some properties, we don't know its value utill we add the widget and
	 * pack it in a container. For example for a "pos" packing property we need
	 * to pack the property first, the query the property to know the value
	 */
	if (string && strcmp (string, GLADE_GET_DEFAULT_FROM_WIDGET) == 0) {
		property->value = g_strdup (string);
		return property;
	}

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
	case GLADE_PROPERTY_TYPE_DOUBLE:
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
			   choice->name, class->id,
			   choice->symbol, string);
		value = g_strdup (choice->symbol);
		break;
	case GLADE_PROPERTY_TYPE_OTHER_WIDGETS:
		value = g_strdup ("");
		break;
	case GLADE_PROPERTY_TYPE_OBJECT:
		value = NULL;
		property->child = glade_widget_new_from_class (class->child,
							       widget);
		break;
	case GLADE_PROPERTY_TYPE_ERROR:
		g_warning ("Invalid Glade property type (%d)\n", class->type);
		break;
	}

	property->value = value;
	
	return property;
}

static GList *
glade_property_list_new_from_list (GList *list, GladeWidget *widget)
{
	GladePropertyClass *property_class;
	GladeProperty *property;
	GList *new_list = NULL;

	for (; list != NULL; list = list->next) {
		property_class = list->data;
		property = glade_property_new_from_class (property_class, widget);
		if (property == NULL)
			continue;

		property->widget = widget;

		new_list = g_list_prepend (new_list, property);
	}

	new_list = g_list_reverse (new_list);
	
	return new_list;
}


GList *
glade_property_list_new_from_widget_class (GladeWidgetClass *class,
					   GladeWidget *widget)
{
	GList *list = NULL;

	list = class->properties;

	return glade_property_list_new_from_list (list, widget);
}


GladeProperty *
glade_property_get_from_id (GList *settings_list, const gchar *id)
{
	GList *list;
	GladeProperty *property;

	g_return_val_if_fail (id != NULL, NULL);
	
	list = settings_list;
	for (; list != NULL; list = list->next) {
		property = list->data;
		g_return_val_if_fail (property, NULL);
		g_return_val_if_fail (property->class, NULL);
		g_return_val_if_fail (property->class->id, NULL);
		if (strcmp (property->class->id, id) == 0)
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

	if (property->class->id == NULL)
		return;

	if (property->class->set_function == NULL)
		gtk_object_set (GTK_OBJECT (property->widget->widget),
				property->class->id,
				property->value, NULL);
	else
		(*property->class->set_function) (G_OBJECT (property->widget->widget),
						  property->value);
}

void
glade_property_changed_integer (GladeProperty *property, gint val)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->value != NULL);

	g_free (property->value);
	property->value = g_strdup_printf ("%i", val);

	if (property->class->set_function == NULL)
		gtk_object_set (GTK_OBJECT (property->widget->widget),
				property->class->id, val, NULL);
	else
		(*property->class->set_function) (G_OBJECT (property->widget->widget),
						  property->value);
}

void
glade_property_changed_float (GladeProperty *property, gfloat val)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->value != NULL);
	
	g_free (property->value);
	property->value = g_strdup_printf ("%g", val);

	if (property->class->set_function == NULL)
		gtk_object_set (GTK_OBJECT (property->widget->widget),
				property->class->id, val, NULL);
	else
		(*property->class->set_function) (G_OBJECT (property->widget->widget),
						  property->value);
	
}

void
glade_property_changed_double (GladeProperty *property, gdouble val)
{
#if 0	
	GValue *gvalue = g_new0 (GValue, 1);
#endif	
	
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->value != NULL);

	g_free (property->value);
	property->value = g_strdup_printf ("%g", val);

#if 0 
	gvalue = g_value_init (gvalue, G_TYPE_DOUBLE);
	g_value_set_double (gvalue, val);
#endif	

#ifdef DEBUG
	g_debug ("Changed double to %g \"%s\" -->%s<-- but using gvalue @%d\n",
		 val,
		 property->value,
		 property->class->gtk_arg,
		 GPOINTER_TO_INT (gvalue));
#endif	

	g_object_set (G_OBJECT (property->widget->widget),
		      property->class->id, val, NULL);

#ifdef DEBUG
	if (GTK_IS_SPIN_BUTTON (property->widget->widget)) {
		g_debug ("It is spin button\n");
		g_debug ("The alignement is min :%g max:%g value%g\n",
			 GTK_SPIN_BUTTON (property->widget->widget)->adjustment->lower,
			 GTK_SPIN_BUTTON (property->widget->widget)->adjustment->upper,
			 GTK_SPIN_BUTTON (property->widget->widget)->adjustment->value);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (property->widget->widget),
					   333.22);
	}
#endif	
#if 0	
	g_object_set (G_OBJECT (property->widget->widget),
		      property->class->gtk_arg,
		      gvalue, NULL);
#endif
}

void
glade_property_changed_boolean (GladeProperty *property, gboolean val)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->value != NULL);
	g_return_if_fail (property->widget != NULL);
	g_return_if_fail (property->widget->widget != NULL);

	g_free (property->value);
	property->value = g_strdup_printf ("%s", val ? GLADE_TAG_TRUE : GLADE_TAG_FALSE);

	if (property->class->set_function == NULL)
		gtk_object_set (GTK_OBJECT (property->widget->widget),
				property->class->id,
				val,
				NULL);
	else
		(*property->class->set_function) (G_OBJECT (property->widget->widget),
						  property->value);
	
}

void
glade_property_changed_choice (GladeProperty *property, GladeChoice *choice)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->value != NULL);
	g_return_if_fail (choice != NULL);

	g_free (property->value);
	property->value = g_strdup_printf ("%s", choice->symbol);

	if (property->class->set_function == NULL)
		gtk_object_set (GTK_OBJECT (property->widget->widget),
				property->class->id, choice->value, NULL);
	else
		(*property->class->set_function) (G_OBJECT (property->widget->widget),
						  property->value);
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
	
gdouble
glade_property_get_double (GladeProperty *property)
{
	g_return_val_if_fail (property != NULL, 0.0);
	g_return_val_if_fail (property->value != NULL, 0.0);
	
	return (gdouble) atof (property->value);
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

GladeXmlNode *
glade_property_write (GladeXmlContext *context, GladeProperty *property)
{
	GladeXmlNode *node;

	node = glade_xml_node_new (context, GLADE_XML_TAG_PROPERTY);
	glade_xml_node_set_property_string (node, GLADE_XML_TAG_NAME, property->class->id);
	glade_xml_set_content (node, property->value);
	
	return node;
}
	
void
glade_property_free (GladeProperty *property)
{
	property->class = NULL;
	property->widget = NULL;
	if (property->value)
		g_free (property->value);
	property->value = NULL;

	if (property->child)
		g_warning ("Implmenet free property->child\n");

	g_free (property);
}
