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
#include "glade-xml-utils.h"
#include "glade-parameter.h"

void
glade_parameter_get_integer (GList *parameters, const gchar *key, gint *value)
{
	GladeParameter *parameter;
	GList *list;

	list = parameters;
	for (; list != NULL; list = list->next) {
		parameter = list->data;
		if (strcmp (key, parameter->key) == 0) {
			*value = atoi (parameter->value);
			return;
		}
	}
}

void
glade_parameter_get_float (GList *parameters, const gchar *key, gfloat *value)
{
	GladeParameter *parameter;
	GList *list;

	list = parameters;
	for (; list != NULL; list = list->next) {
		parameter = list->data;
		if (strcmp (key, parameter->key) == 0) {
			*value = atof (parameter->value);
			return;
		}
	}
}


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

static void
glade_parameter_free (GladeParameter *parameter)
{
	g_return_if_fail (parameter->key);
	g_return_if_fail (parameter->value);

	g_free (parameter->key);
	g_free (parameter->value);
	parameter->key = NULL;
	parameter->value = NULL;
	g_free (parameter);
}

GladeParameter *
glade_parameter_new (void)
{
	GladeParameter *parameter;

	parameter = g_new0 (GladeParameter, 1);

	return parameter;
}

static GladeParameter *
glade_parameter_new_from_node (xmlNodePtr node)
{
	GladeParameter *parameter;

	if (!glade_xml_node_verify (node, GLADE_TAG_PARAMETER))
		return NULL;
	
	parameter = glade_parameter_new ();
	parameter->key   = glade_xml_get_value_string_required (node, GLADE_TAG_KEY, NULL);
	parameter->value = glade_xml_get_value_string_required (node, GLADE_TAG_VALUE, NULL);

	if (!parameter->key || !parameter->value)
		return NULL;

	return parameter;
}

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
		

GList *
glade_parameter_list_new_from_node (GList *list, xmlNodePtr node)
{
	GladeParameter *parameter;
	xmlNodePtr child;
	GList *findme;

	if (!glade_xml_node_verify (node, GLADE_TAG_PARAMETERS))
		return NULL;
	child = glade_xml_search_child (node, GLADE_TAG_PARAMETER);
	if (child == NULL)
		return NULL;

	child = node->children;

	while (child != NULL) {
		skip_text (child);
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
			child = child->next;
		}

		list = g_list_prepend (list, parameter);
		child = child->next;
	}

	list = g_list_reverse (list);
	
	return list;
}
	

/**
 * glade_parameter_adjustment_new:
 * @parameter: 
 * 
 * Creates a GtkAdjustment from a list of parameters.
 * 
 * Return Value: A newly created GtkAdjustment
 **/
GtkAdjustment *
glade_parameter_adjustment_new (GList *parameters)
{
	GtkAdjustment *adjustment;
	gfloat value = 1;
	gfloat lower = 0;
	gfloat upper = 999999;

	gfloat step_increment = 1;
	gfloat page_increment = 265;
	gfloat climb_rate = 1;

	glade_parameter_get_float (parameters, "Default", &value);
	glade_parameter_get_float (parameters, "Min",     &lower);
	glade_parameter_get_float (parameters, "Max",     &upper);
	glade_parameter_get_float (parameters, "StepIncrement", &step_increment);
	glade_parameter_get_float (parameters, "PageIncrement", &page_increment);
	glade_parameter_get_float (parameters, "ClimbRate", &climb_rate);

	adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (value,
							 lower,
							 upper,
							 step_increment,
							 page_increment,
							 climb_rate));
	return adjustment;
}

