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

static void glade_property_class_init (GladePropertyObjectClass * klass);
static void glade_property_init (GladeProperty *property);
static void glade_property_destroy (GtkObject *object);

enum
{
	CHANGED,
	LAST_SIGNAL
};

static guint glade_property_signals[LAST_SIGNAL] = {0};
static GtkObjectClass *parent_class = NULL;

guint
glade_property_get_type (void)
{
	static guint type = 0;
  
	if (!type)
	{
		GtkTypeInfo info =
		{
			"GladeProperty",
			sizeof (GladeProperty),
			sizeof (GladePropertyObjectClass),
			(GtkClassInitFunc) glade_property_class_init,
			(GtkObjectInitFunc) glade_property_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		type = gtk_type_unique (gtk_object_get_type (),
					&info);
	}
	
	return type;
}

static void
glade_property_class_init (GladePropertyObjectClass * klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) klass;

	parent_class = gtk_type_class (gtk_object_get_type ());

	glade_property_signals[CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				GTK_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (GladePropertyObjectClass, changed),
				gtk_marshal_VOID__VOID,
				GTK_TYPE_NONE, 0);
	
	klass->changed = NULL;

	object_class->destroy = glade_property_destroy;
}


static void
glade_property_init (GladeProperty *property)
{

	property->class = NULL;
	property->value = g_new0 (GValue, 1);
	property->enabled = TRUE;
	property->child = NULL;
}

static void
glade_property_destroy (GtkObject *object)
{
	GladeProperty *property;

	property = GLADE_PROPERTY (object);
}

static GladeProperty *
glade_property_new (void)
{
	GladeProperty *property;

	property = GLADE_PROPERTY (gtk_type_new (glade_property_get_type ()));

	return property;
}


/* We are recursing so add the prototype. Don't you love C ? */
static GList * glade_property_list_new_from_list (GList *list, GladeWidget *widget);

GladeProperty *
glade_property_new_from_class (GladePropertyClass *class, GladeWidget *widget)
{
	GladeProperty *property;
	property = glade_property_new ();
	
	property->class = class;

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

static void
glade_property_emit_changed (GladeProperty *property)
{
	gtk_signal_emit (GTK_OBJECT (property),
			 glade_property_signals [CHANGED]);
}

void
glade_property_set_string (GladeProperty *property,
			   const gchar *text)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->value != NULL);
	g_return_if_fail (property->widget != NULL);
	g_return_if_fail (property->widget->widget != NULL);

	if (text == NULL)
		return;

	if (!g_value_get_string (property->value) ||
	    (strcmp (text, g_value_get_string (property->value)) != 0))
		g_value_set_string (property->value, text);

	if (property->enabled) {
		property->loading = TRUE;
		if (property->class->set_function == NULL)
			g_object_set (G_OBJECT (property->widget->widget),
				      property->class->id,
				      g_value_get_string (property->value), NULL);
		else
			(*property->class->set_function) (G_OBJECT (property->widget->widget),
							  property->value);
		property->loading = FALSE;
	}

	glade_property_emit_changed (property);
}

void
glade_property_set_integer (GladeProperty *property, gint val)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->value != NULL);

	g_value_set_int (property->value, val);

	if (property->enabled) {
		property->loading = TRUE;
		if (property->class->set_function == NULL)
			gtk_object_set (GTK_OBJECT (property->widget->widget),
					property->class->id, val, NULL);
		else
			(*property->class->set_function) (G_OBJECT (property->widget->widget),
							  property->value);
		property->loading = FALSE;
	}

	glade_property_emit_changed (property);
}

void
glade_property_set_float (GladeProperty *property, gfloat val)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->value != NULL);

	g_value_set_float (property->value, val);

	if (property->enabled) {
		property->loading = TRUE;
		if (property->class->set_function == NULL)
			gtk_object_set (GTK_OBJECT (property->widget->widget),
					property->class->id, val, NULL);
		else
			(*property->class->set_function) (G_OBJECT (property->widget->widget),
							  property->value);
		property->loading = FALSE;
	}

	glade_property_emit_changed (property);
}

void
glade_property_set_double (GladeProperty *property, gdouble val)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->value != NULL);

	g_value_set_double (property->value, val);

	if (property->enabled) {
		property->loading = TRUE;
		if (property->class->set_function == NULL)
			gtk_object_set (GTK_OBJECT (property->widget->widget),
					property->class->id, val, NULL);
		else
			(*property->class->set_function) (G_OBJECT (property->widget->widget),
							  property->value);
		property->loading = FALSE;
	}

	glade_property_emit_changed (property);

	if (GTK_IS_SPIN_BUTTON (property->widget->widget)) {
		g_debug(("It is spin button\n"));
		g_debug(("The alignement is min :%g max:%g value%g\n",
			 GTK_SPIN_BUTTON (property->widget->widget)->adjustment->lower,
			 GTK_SPIN_BUTTON (property->widget->widget)->adjustment->upper,
			 GTK_SPIN_BUTTON (property->widget->widget)->adjustment->value));
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (property->widget->widget),
					   333.22);
	}
}

void
glade_property_set_boolean (GladeProperty *property, gboolean val)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->value != NULL);
	g_return_if_fail (property->widget != NULL);
	g_return_if_fail (property->widget->widget != NULL);

	g_value_set_boolean (property->value, val);

	if (property->enabled) {
		property->loading = TRUE;
		if (property->class->set_function == NULL) {
			if (GTK_IS_TABLE (property->widget->widget)) {
				g_debug(("Is table \n"));
				gtk_widget_queue_resize (GTK_WIDGET (property->widget->widget));
#if 0	
				gtk_table_set_homogeneous (property->widget->widget, val);
#endif	
			} else
				gtk_object_set (GTK_OBJECT (property->widget->widget),
						property->class->id, val, NULL);
		}
		else
			(*property->class->set_function) (G_OBJECT (property->widget->widget),
							  property->value);
		property->loading = FALSE;
	}

	glade_property_emit_changed (property);
}

void
glade_property_set_unichar (GladeProperty *property, gunichar val)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->value != NULL);

	g_value_set_uint (property->value, val);

	if (property->enabled) {
		property->loading = TRUE;
		if (property->class->set_function == NULL)
			gtk_object_set (GTK_OBJECT (property->widget->widget),
					property->class->id, val, NULL);
		else
			(*property->class->set_function) (G_OBJECT (property->widget->widget),
							  property->value);
		property->loading = FALSE;
	}

	glade_property_emit_changed (property);
}

void
glade_property_set_enum (GladeProperty *property, GladeChoice *choice)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->value != NULL);
	g_return_if_fail (choice != NULL);


	g_value_set_enum (property->value, choice->value);

	property->loading = TRUE;
	if (property->class->set_function == NULL)
		gtk_object_set (GTK_OBJECT (property->widget->widget),
				property->class->id, choice->value, NULL);
	else
		(*property->class->set_function) (G_OBJECT (property->widget->widget),
						  property->value);

	property->loading = FALSE;

	glade_property_emit_changed (property);
}
	
void
glade_property_set (GladeProperty *property, const GValue *value)
{
	switch (property->class->type) {
	case GLADE_PROPERTY_TYPE_BOOLEAN:
		glade_property_set_boolean (property,
					    g_value_get_boolean (value));
		break;
	case GLADE_PROPERTY_TYPE_FLOAT:
		glade_property_set_float (property,
					  g_value_get_float (value));
		break;
	case GLADE_PROPERTY_TYPE_INTEGER:
		glade_property_set_integer (property,
					    g_value_get_int (value));
		break;
	case GLADE_PROPERTY_TYPE_DOUBLE:
		glade_property_set_double (property,
					   g_value_get_double (value));
		break;
	case GLADE_PROPERTY_TYPE_UNICHAR:
		glade_property_set_unichar (property,
					    g_value_get_uint (value));
		break;
	case GLADE_PROPERTY_TYPE_STRING:
		glade_property_set_string (property,
					   g_value_get_string (value));
		break;
	case GLADE_PROPERTY_TYPE_ENUM:
		{
			GladeChoice *choice = NULL;
			GList * list = NULL;

			list = property->class->choices;
			for (; list != NULL; list = list->next) {
				choice = list->data;
				if (choice->value == g_value_get_enum (value))
					break;
			}
			if (list == NULL)
				g_warning ("Cant find the GladePropertyChoice "
					   "for the #%d\n", g_value_get_int (value));
			else
				glade_property_set_enum (property, choice);
		}
		break;
	case GLADE_PROPERTY_TYPE_OBJECT:
		glade_implement_me ();
		g_print ("Set adjustment ->%d<-\n", GPOINTER_TO_INT (property->child));
#if 1	
		g_print ("Set directly \n");
#if 0
		glade_widget_set_default_options_real (property->child, packing);
#endif	
		gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (property->widget->widget),
						GTK_ADJUSTMENT (property->child));
#else		
		gtk_object_set (GTK_OBJECT (property->widget->widget),
				property->class->id,
				property->child, NULL);
#endif		
		g_print ("Adjustment has been set\n");
		break;
	default:
		g_warning ("Implement set default for this type [%s]\n", property->class->name);
		break;
	}
	
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
	for (; list != NULL; list = list->next) {
		choice = list->data;
		if (choice->value == value)
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
	gchar *tmp;

	if (!property->enabled)
		return NULL;

	/* create a new node <property ...> */
	node = glade_xml_node_new (context, GLADE_XML_TAG_PROPERTY);
	if (!node)
		return NULL;

	/* we should change each '-' by '_' on the name of the property (<property name="...">) */
	tmp = g_strdup(property->class->id);
	if (tmp == NULL) {
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
	if (tmp == NULL)
	{
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
			gtk_object_get (GTK_OBJECT (property->widget->widget),
					property->class->id,
					&bool,
					NULL);
			g_value_set_boolean (property->value, bool);
			break;
		default:
			glade_implement_me ();
			break;
		}
	}
}
