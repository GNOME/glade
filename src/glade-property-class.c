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


static GladePropertyClass *
glade_property_class_new_from_node (xmlNodePtr node)
{
	GladePropertyClass *property_class;
	xmlNodePtr child;
	gchar *type;

	if (!glade_xml_node_verify (node, GLADE_TAG_PROPERTY))
		return NULL;
	
	property_class = glade_property_class_new ();
	property_class->name    = glade_xml_get_value_string_required (node, GLADE_TAG_NAME, NULL);
	property_class->tooltip = glade_xml_get_value_string (node, GLADE_TAG_TOOLTIP);
	property_class->gtk_arg = glade_xml_get_value_string (node, GLADE_TAG_GTKARG);

	/* Get the type */
	type = glade_xml_get_value_string_required (node, GLADE_TAG_TYPE, NULL);
	if (type == NULL)
		return NULL;
	property_class->type = glade_property_type_str_to_enum (type);
	g_free (type);
	if (property_class->type == GLADE_PROPERTY_TYPE_ERROR)
		return NULL;

	/* Get the Parameters */
	child = glade_xml_search_child (node, GLADE_TAG_PARAMETERS);
	if (child != NULL)
		property_class->parameters = glade_parameter_list_new_from_node (child);
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
glade_property_class_list_new_from_node (xmlNodePtr node)
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
		property_class = glade_property_class_new_from_node (child);
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




