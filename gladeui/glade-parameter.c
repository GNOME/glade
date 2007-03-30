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
 */

#include "config.h"

#include <stdlib.h> /* for atoi and atof */
#include <string.h>

#include "glade.h"
#include "glade-xml-utils.h"
#include "glade-parameter.h"
#include "glade-property-class.h"

/**
 * glade_parameter_get_integer:
 * @parameters: a #GList of #GladeParameters
 * @key: a string containing the parameter name
 * @value: a pointer to a #gint
 *
 * Searches through @parameters looking for a #GladeParameter named @key. If
 * found, it stores a #gint representation of its value into @value.
 */
void
glade_parameter_get_integer (GList *parameters, const gchar *key, gint *value)
{
	GladeParameter *parameter;
	GList *list;

	list = parameters;
	for (; list != NULL; list = list->next) {
		parameter = list->data;
		if (strcmp (key, parameter->key) == 0) {
			*value = g_ascii_strtoll (parameter->value, NULL, 10);
			return;
		}
	}
}

/**
 * glade_parameter_get_float:
 * @parameters: a #GList of #GladeParameters
 * @key: a string containing the parameter name
 * @value: a pointer to a #gfloat
 *
 * Searches through @parameters looking for a #GladeParameter named @key. If
 * found, it stores a #gfloat representation of its value into @value.
 */
void
glade_parameter_get_float (GList *parameters, const gchar *key, gfloat *value)
{
	GladeParameter *parameter;
	GList *list;

	list = parameters;
	for (; list != NULL; list = list->next) {
		parameter = list->data;
		if (strcmp (key, parameter->key) == 0) {
			*value = (float) g_ascii_strtod (parameter->value, NULL);
			return;
		}
	}
}

/**
 * glade_parameter_get_boolean:
 * @parameters: a #GList of #GladeParameters
 * @key: a string containing the parameter name
 * @value: a pointer to a #gboolean
 *
 * Searches through @parameters looking for a #GladeParameter named @key. If
 * found, it stores a #gboolean representation of its value into @value.
 */
void
glade_parameter_get_boolean (GList *parameters, const gchar *key, gboolean *value)
{
	GladeParameter *parameter;
	GList *list;

	list = parameters;
	for (; list != NULL; list = list->next) {
		parameter = list->data;
		if (strcmp (key, parameter->key) == 0) {
			if (strcmp (parameter->value, GLADE_TAG_TRUE) == 0)
				*value = TRUE;
			else if (strcmp (parameter->value, GLADE_TAG_FALSE) == 0)
				*value = FALSE;
			else
				g_warning ("Invalid boolean parameter *%s* (%s/%s)",
					   parameter->value, GLADE_TAG_TRUE, GLADE_TAG_FALSE);
			return;
		}
	}
}

/**
 * glade_parameter_get_string:
 * @parameters: a #GList of #GladeParameters
 * @key: a string containing the parameter name
 * @value: a pointer to an string
 *
 * Searches through @parameters looking for a #GladeParameter named @key. If
 * found, it stores a newly copied string representation of its value into 
 * @value.
 */
void
glade_parameter_get_string (GList *parameters, const gchar *key, gchar **value)
{
	GladeParameter *parameter;
	GList *list;

	list = parameters;
	for (; list != NULL; list = list->next) {
		parameter = list->data;
		if (strcmp (key, parameter->key) == 0) {
			if (*value != NULL)
				g_free (*value);
			*value = g_strdup (parameter->value);
			return;
		}
	}
}

/**
 * glade_parameter_free:
 * @parameter: a #GladeParameter
 *
 * Frees @parameter and its associated memory.
 */
void
glade_parameter_free (GladeParameter *parameter)
{
	if (!parameter)
		return;

	g_free (parameter->key);
	g_free (parameter->value);
	g_free (parameter);
}

/**
 * glade_parameter_new:
 *
 * Returns: a new #GladeParameter
 */
GladeParameter *
glade_parameter_new (void)
{
	GladeParameter *parameter;

	parameter = g_new0 (GladeParameter, 1);

	return parameter;
}

/**
 * glade_parameter_clone:
 * @parameter: a #GladeParameter
 *
 * Returns: a new #GladeParameter cloned from @parameter
 */
GladeParameter *
glade_parameter_clone (GladeParameter *parameter)
{
	GladeParameter *clone;

	if (parameter == NULL)
		return NULL;

	clone = glade_parameter_new ();
	clone->key = g_strdup (parameter->key);
	clone->value = g_strdup (parameter->value);

	return clone;
}

static GladeParameter *
glade_parameter_new_from_node (GladeXmlNode *node)
{
	GladeParameter *parameter;

	if (!glade_xml_node_verify (node, GLADE_TAG_PARAMETER))
		return NULL;
	
	parameter = glade_parameter_new ();
	parameter->key   = glade_xml_get_property_string_required (node, GLADE_TAG_KEY, NULL);
	parameter->value = glade_xml_get_property_string_required (node, GLADE_TAG_VALUE, NULL);

	if (!parameter->key || !parameter->value)
		return NULL;

	return parameter;
}

/**
 * glade_parameter_list_find_by_key:
 * @list: a #GList containing #GladeParameters
 * @key: a string used as a parameter key
 *
 * Searches through @list looking for a node which contains a
 * #GladeParameter matching @key
 * 
 * Returns: the #GList node containing the located #GladeParameter,
 *          or %NULL if none is found
 */
GList *
glade_parameter_list_find_by_key (GList *list, const gchar *key)
{
	GladeParameter *parameter;
	
	for (; list != NULL; list = list->next) {
		parameter = list->data;
		g_return_val_if_fail (parameter->key != NULL, NULL);
		if (strcmp (parameter->key, key) == 0)
			return list;
	}

	return NULL;
}
		
/**
 * glade_parameter_list_new_from_node:
 * @list: a #GList node
 * @node: a #GladeXmlNode
 *
 * TODO: write me
 *
 * Returns:
 */
GList *
glade_parameter_list_new_from_node (GList *list, GladeXmlNode *node)
{
	GladeParameter *parameter;
	GladeXmlNode *child;
	GList *findme;

	if (!glade_xml_node_verify (node, GLADE_TAG_PARAMETERS))
		return NULL;
	child = glade_xml_search_child (node, GLADE_TAG_PARAMETER);
	if (child == NULL)
		return NULL;

	child = glade_xml_node_get_children (node);

	for (; child != NULL; child = glade_xml_node_next (child)) {
		if (!glade_xml_node_verify (child, GLADE_TAG_PARAMETER))
			return NULL;
		
		parameter = glade_parameter_new_from_node (child);
		if (parameter == NULL)
			return NULL;
		/* Is this parameter already there ? just replace
		 * the pointer and free the old one
		 */
		findme = glade_parameter_list_find_by_key (list,
							   parameter->key);
		if (findme) {
			glade_parameter_free (findme->data);
			findme->data = parameter;
			continue;
		}

		list = g_list_prepend (list, parameter);
	}

	list = g_list_reverse (list);
	
	return list;
}
