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
#include "glade-debug.h"

static void glade_property_object_class_init (GladePropertyObjectClass *class);
static void glade_property_init (GladeProperty *property);

enum
{
	CHANGED,
	LAST_SIGNAL
};

static guint glade_property_signals[LAST_SIGNAL] = {0};
static GtkObjectClass *parent_class = NULL;

GType
glade_property_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GladePropertyObjectClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) glade_property_object_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (GladeProperty),
			0,
			(GInstanceInitFunc) glade_property_init
		};

		type = g_type_register_static (G_TYPE_OBJECT, "GladeProperty", &info, 0);
	}

	return type;
}

static void
glade_property_object_class_init (GladePropertyObjectClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	glade_property_signals[CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladePropertyObjectClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	class->changed = NULL;
}

static void
glade_property_init (GladeProperty *property)
{
	property->class = NULL;
	property->value = g_new0 (GValue, 1);
	property->enabled = TRUE;
	property->child = NULL;
}

GladeProperty *
glade_property_new (GladePropertyClass *class, GladeWidget *widget)
{
	GladeProperty *property;

	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (class), NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

	property = g_object_new (GLADE_TYPE_PROPERTY, NULL);
	property->class = class;
	property->widget = widget;

	if (class->type == GLADE_PROPERTY_TYPE_OBJECT) {
		property->child = glade_widget_new_from_class (class->child,
							       widget->project,
							       widget);
		return property;
	}

	/* Create an empty default if the class does not specify a default value */
	if (!class->def) {
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
		g_value_copy (class->def, property->value);
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

GladeProperty *
glade_property_get_from_id (GList *settings_list, const gchar *id)
{
	GList *list;
	GladeProperty *property;

	g_return_val_if_fail (id != NULL, NULL);

	for (list = settings_list; list; list = list->next) {
		property = list->data;
		g_return_val_if_fail (property, NULL);
		g_return_val_if_fail (property->class, NULL);
		g_return_val_if_fail (property->class->id, NULL);
		if (strcmp (property->class->id, id) == 0)
			return property;
	}

	return NULL;
}

static void
glade_property_emit_changed (GladeProperty *property)
{
	g_signal_emit (G_OBJECT (property), glade_property_signals [CHANGED], 0);
}

/**
 * Generic set function for properties that do not have a
 * custom set_property method. This includes packing properties.
 */
static void
glade_property_set_property (GladeProperty *property, const GValue *value)
{
	if (property->class->packing) {
		GladeWidget *parent = glade_widget_get_parent (property->widget);
		GtkContainer *container = GTK_CONTAINER (parent->widget);
		GtkWidget *child = property->widget->widget;
		gtk_container_child_set_property (container, child, property->class->id, value);
	} else {
		GObject *gobject = G_OBJECT (property->widget->widget);
		g_object_set_property (gobject, property->class->id, value);
	}
}

void
glade_property_set (GladeProperty *property, const GValue *value)
{
	g_return_if_fail (GLADE_IS_PROPERTY (property));
	g_return_if_fail (value != NULL);

	if (!g_value_type_compatible (G_VALUE_TYPE (property->value),
				      G_VALUE_TYPE (value))) {
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
	if (property->class->set_function) {
		(*property->class->set_function) (G_OBJECT (property->widget->widget),
						  value);
	} else {
		glade_property_set_property (property, value);
	}

	g_value_reset (property->value);
	g_value_copy (value, property->value);

	property->loading = FALSE;

	glade_property_emit_changed (property);
}

void
glade_property_refresh (GladeProperty *property)
{
	glade_property_set (property, property->value);
}

const gchar *
glade_property_get_string (GladeProperty *property)
{
	g_return_val_if_fail (property != NULL, NULL);
	g_return_val_if_fail (property->value != NULL, NULL);

	return g_value_get_string (property->value);
}
	
gint
glade_property_get_integer (GladeProperty *property)
{
	g_return_val_if_fail (property != NULL, 0);
	g_return_val_if_fail (property->value != NULL, 0);

	return g_value_get_int (property->value);	
}

gfloat
glade_property_get_float (GladeProperty *property)
{
	gfloat resp;
	g_return_val_if_fail (property != NULL, 0.0);
	g_return_val_if_fail (property->value != NULL, 0.0);

	resp = g_value_get_float (property->value);

	return resp;
}
	
gdouble
glade_property_get_double (GladeProperty *property)
{
	g_return_val_if_fail (property != NULL, 0.0);
	g_return_val_if_fail (property->value != NULL, 0.0);
	
	return g_value_get_double (property->value);
}

gboolean
glade_property_get_boolean (GladeProperty *property)
{
	g_return_val_if_fail (property != NULL, FALSE);
	g_return_val_if_fail (property->value != NULL, FALSE);

	return g_value_get_boolean (property->value);
}	
	
gunichar
glade_property_get_unichar (GladeProperty *property)
{
	g_return_val_if_fail (property != NULL, g_utf8_get_char (" "));
	g_return_val_if_fail (property->value != NULL, g_utf8_get_char (" "));
	
	return g_value_get_uint (property->value);
}

GladeChoice *
glade_property_get_enum (GladeProperty *property)
{
	GladeChoice *choice = NULL;
	GList *list;
	gint value;
	
	g_return_val_if_fail (property != NULL, NULL);
	g_return_val_if_fail (property->value != NULL, NULL);

	value = g_value_get_enum (property->value);
	list = property->class->choices;
	for (; list; list = list->next) {
		choice = list->data;
		if (choice->value == value)
			break;
	}
	if (list == NULL)
		g_warning ("Cant find the GladePropertyChoice selected\n");

	return choice;
}

GladeXmlNode *
glade_property_write (GladeXmlContext *context, GladeProperty *property)
{
	GladeXmlNode *node;
	gchar *tmp;

	if (!property->enabled)
		return NULL;

	/* create a new node <property ...> */
	node = glade_xml_node_new (context, GLADE_XML_TAG_PROPERTY);
	if (!node)
		return NULL;

	/* we should change each '-' by '_' on the name of the property (<property name="...">) */
	tmp = g_strdup (property->class->id);
	if (!tmp) {
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

	if (!tmp) {
		glade_xml_node_delete (node);
		return NULL;
	}

	glade_xml_set_content (node, tmp);
	g_free (tmp);

	/* return the created node */
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

void
glade_property_get_from_widget (GladeProperty *property)
{
	gboolean bool = FALSE;
	
	g_value_reset (property->value);

	if (property->class->get_function)
		(*property->class->get_function) (G_OBJECT (property->widget->widget), property->value);
	else {
		switch (property->class->type) {
		case GLADE_PROPERTY_TYPE_BOOLEAN:
			g_object_get (G_OBJECT (property->widget->widget),
				      property->class->id, &bool, NULL);
			g_value_set_boolean (property->value, bool);
			break;
		default:
			glade_implement_me ();
			break;
		}
	}
}

