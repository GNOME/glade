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

#include <stdio.h>
#include <stdlib.h> /* for atoi and atof */
#include <string.h>

#include "glade.h"
#include "glade-choice.h"
#include "glade-widget.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-parameter.h"
#include "glade-widget-class.h"
#include "glade-debug.h"


GladeProperty *
glade_property_new (GladePropertyClass *class, GladeWidget *widget)
{
	GladeProperty *property;

	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (class), NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

	property = g_new0 (GladeProperty, 1);
	property->class = class;
	property->widget = widget;
	property->value = g_new0 (GValue, 1);
	property->enabled = TRUE;
#if 0
	property->child = NULL;

	if (class->type == GLADE_PROPERTY_TYPE_OBJECT) {
		property->child = glade_widget_new_from_class (class->child,
							       widget->project,
							       widget);
		return property;
	}
#endif

	/* Create an empty default if the class does not specify a default value */
	if (!class->def)
	{
		property->value = glade_property_class_make_gvalue_from_string (class, "");
		return property;
	}

	switch (class->type) {
	case GLADE_PROPERTY_TYPE_DOUBLE:
	case GLADE_PROPERTY_TYPE_INTEGER:
	case GLADE_PROPERTY_TYPE_FLOAT:
		property->enabled = class->optional_default;
		/* Fall thru */
	case GLADE_PROPERTY_TYPE_ENUM:
	case GLADE_PROPERTY_TYPE_FLAGS:
	case GLADE_PROPERTY_TYPE_BOOLEAN:
	case GLADE_PROPERTY_TYPE_STRING:
	case GLADE_PROPERTY_TYPE_UNICHAR:
		g_value_init (property->value, class->def->g_type);
		g_value_copy (class->def, property->value); // glade_property_set (property, class->def);
		break;
	case GLADE_PROPERTY_TYPE_OTHER_WIDGETS:
#if 0	
		value = g_strdup ("");
#endif	
		break;
	case GLADE_PROPERTY_TYPE_OBJECT:
		break;
	case GLADE_PROPERTY_TYPE_ERROR:
		g_warning ("Invalid Glade property type (%d)\n", class->type);
		break;
	}

	return property;
}
	
void
glade_property_free (GladeProperty *property)
{
	property->class = NULL;
	property->widget = NULL;
	if (property->value)
	{
		g_value_unset (property->value);
		g_free (property->value);
		property->value = NULL;
	}

#if 0
	if (property->child)
		g_warning ("Implemenet free property->child\n");
#endif

	g_free (property);
}

/**
 * Generic set function for properties that do not have a
 * custom set_property method. This includes packing properties.
 */
static void
glade_property_set_property (GladeProperty *property, const GValue *value)
{
	if (property->class->packing)
	{
		GladeWidget *parent = glade_widget_get_parent (property->widget);
		GtkContainer *container = GTK_CONTAINER (glade_widget_get_widget (parent));
		GtkWidget *child = glade_widget_get_widget (property->widget);
		gtk_container_child_set_property (container, child, property->class->id, value);
	}
	else
	{
		GObject *gobject = G_OBJECT (glade_widget_get_widget (property->widget));
		g_object_set_property (gobject, property->class->id, value);
	}
}

void
glade_property_set (GladeProperty *property, const GValue *value)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));
	g_return_if_fail (value != NULL);

	if (!g_value_type_compatible (G_VALUE_TYPE (property->value), G_VALUE_TYPE (value)))
	{
		g_warning ("Trying to assing an incompatible value to property %s\n",
			    property->class->id);
		return;
	}

	if (!property->enabled)
		return;

	if (property->loading)
		return;

	property->loading = TRUE;

	/* if there is a custom set_property use it*/
	if (property->class->set_function)
		(*property->class->set_function) (G_OBJECT (property->widget->widget), value);
	else
		glade_property_set_property (property, value);

	g_value_reset (property->value);
	g_value_copy (value, property->value);

	property->loading = FALSE;
}

GladeXmlNode *
glade_property_write (GladeXmlContext *context, GladeProperty *property)
{
	GladeXmlNode *node;
	gchar *tmp;
	gchar *default_str = NULL;

	if (!property->enabled)
		return NULL;

	node = glade_xml_node_new (context, GLADE_XML_TAG_PROPERTY);
	if (!node)
		return NULL;

	/* we should change each '-' by '_' on the name of the property (<property name="...">) */
	tmp = g_strdup (property->class->id);
	if (!tmp)
	{
		glade_xml_node_delete (node);
		return NULL;
	}
	glade_util_replace (tmp, '-', '_');

	/* put the name="..." part on the <property ...> tag */
	glade_xml_node_set_property_string (node, GLADE_XML_TAG_NAME, tmp);

	g_free (tmp);

	/* convert the value of this property to a string, and put it between
	 * the openning and the closing of the property tag */
	tmp = glade_property_class_make_string_from_gvalue (property->class,
							    property->value);
	if (!tmp)
	{
		glade_xml_node_delete (node);
		return NULL;
	}

	if (property->class->def)
	{
		default_str = glade_property_class_make_string_from_gvalue (property->class,
									    property->class->def);
		if (default_str && strcmp (tmp, default_str) == 0)
		{
			g_free (tmp);
			g_free (default_str);
			glade_xml_node_delete (node);
			return NULL;
		}
	}

	glade_xml_set_content (node, tmp);
	g_free (tmp);
	g_free (default_str);

	return node;
}

