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
#include "glade-debug.h"


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
	switch (type)
	{
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

GladePropertyClass *
glade_property_class_new (void)
{
	GladePropertyClass *property_class;

	property_class = g_new0 (GladePropertyClass, 1);
	property_class->type = GLADE_PROPERTY_TYPE_ERROR;
	property_class->id = NULL;
	property_class->name = NULL;
	property_class->tooltip = NULL;
	property_class->parameters = NULL;
	property_class->choices = NULL;
	property_class->enum_type = 0;
	property_class->optional = FALSE;
	property_class->optional_default = TRUE;
	property_class->common = FALSE;
	property_class->packing = FALSE;
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

	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (property_class), NULL);

	clon = g_new0 (GladePropertyClass, 1);

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
			choice->data = glade_choice_clone ((GladeChoice*) choice->data);
	}

	/* ok, wtf? what is the child member for? */
	/* if (clon->child)
		clon->child = glade_widget_class_clone (clon->child); */

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
	if (class->def)
	{
		if (G_VALUE_TYPE (class->def) != 0)
			g_value_unset (class->def);
		g_free (class->def);
	}
	g_list_foreach (class->parameters, (GFunc) glade_parameter_free, NULL);
	g_list_free (class->parameters);
	g_list_foreach (class->choices, (GFunc) glade_choice_free, NULL);
	g_list_free (class->choices);
	glade_widget_class_free (class->child);
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
					    GladePropertyClass *class)
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
	case GLADE_PROPERTY_TYPE_FLAGS:
		{
			GValue tmp_value = { 0, };

			g_value_init (&tmp_value, G_TYPE_STRING);
			g_value_transform (value, &tmp_value);
			string = g_strescape (g_value_get_string (&tmp_value),
					      NULL);
			g_value_unset (&tmp_value);
		}
		break;
	default:
		g_warning ("Could not make string from gvalue for type %s\n",
			 glade_property_type_enum_to_string (type));
	}

	return string;
}

/* This is copied exactly from libglade. I've just renamed the function. */
static guint
glade_property_class_make_flags_from_string (GType type, const char *string)
{
    GFlagsClass *fclass;
    gchar *endptr, *prevptr;
    guint i, j, ret = 0;
    char *flagstr;

    ret = strtoul(string, &endptr, 0);
    if (endptr != string) /* parsed a number */
	return ret;

    fclass = g_type_class_ref(type);


    flagstr = g_strdup (string);
    for (ret = i = j = 0; ; i++) {
	gboolean eos;

	eos = flagstr [i] == '\0';
	
	if (eos || flagstr [i] == '|') {
	    GFlagsValue *fv;
	    const char  *flag;
	    gunichar ch;

	    flag = &flagstr [j];
            endptr = &flagstr [i];

	    if (!eos) {
		flagstr [i++] = '\0';
		j = i;
	    }

            /* trim spaces */
	    for (;;)
	      {
		ch = g_utf8_get_char (flag);
		if (!g_unichar_isspace (ch))
		  break;
		flag = g_utf8_next_char (flag);
	      }

	    while (endptr > flag)
	      {
		prevptr = g_utf8_prev_char (endptr);
		ch = g_utf8_get_char (prevptr);
		if (!g_unichar_isspace (ch))
		  break;
		endptr = prevptr;
	      }

	    if (endptr > flag)
	      {
		*endptr = '\0';
		fv = g_flags_get_value_by_name (fclass, flag);

		if (!fv)
		  fv = g_flags_get_value_by_nick (fclass, flag);

		if (fv)
		  ret |= fv->value;
		else
		  g_warning ("Unknown flag: '%s'", flag);
	      }

	    if (eos)
		break;
	}
    }
    
    g_free (flagstr);

    g_type_class_unref(fclass);

    return ret;
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
					g_value_init (value, property_class->enum_type);
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
	case GLADE_PROPERTY_TYPE_FLAGS:
		{
			guint flags;

			g_value_init (value, property_class->enum_type);
			flags = glade_property_class_make_flags_from_string (property_class->enum_type, string);
			g_value_set_flags (value, flags);
		}
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
	if (property_class->type == GLADE_PROPERTY_TYPE_ERROR)
		/* no warnings, there are properties that we don't edit */
		goto lblError;

	property_class->id = g_strdup (spec->name);
	property_class->name = g_strdup (g_param_spec_get_nick (spec));
	if (!property_class->id || !property_class->name)
	{
		g_warning ("Failed to create property class from spec");
		goto lblError;
	}

	property_class->tooltip = g_strdup (g_param_spec_get_blurb (spec));
	property_class->def = glade_property_class_get_default_from_spec (spec, property_class);

	switch (property_class->type)
	{
	case GLADE_PROPERTY_TYPE_ENUM:
		property_class->choices = glade_choice_list_new_from_spec (spec);
		property_class->enum_type = spec->value_type;
		break;
	case GLADE_PROPERTY_TYPE_FLAGS:
		property_class->enum_type = spec->value_type;
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
//		break;
		goto lblError; /* we don't support this */
	case GLADE_PROPERTY_TYPE_ERROR:
		break;
	}

	return property_class;

lblError:
	glade_property_class_free (property_class);
	return NULL;
}

gboolean
glade_property_class_is_visible (GladePropertyClass *property_class, GladeWidgetClass *widget_class)
{
	if (property_class->visible)
		return property_class->visible (widget_class);

	return TRUE;
}

/**
 * glade_property_class_update_from_node:
 * @node: the <property> node
 * @widget_class: the widget class
 * @property_class: a pointer to the property class
 *
 * Updates the @property_class with the contents of the node in the xml
 * file. Only the values found in the xml file are overridden.
 *
 * Return TRUE on success. @property_class is set to NULL if the property
 * has Disabled="TRUE".
 **/
gboolean
glade_property_class_update_from_node (GladeXmlNode *node,
				       GladeWidgetClass *widget_class,
				       GladePropertyClass **property_class)
{
	GladePropertyClass *class;
	gchar *buff;
	char *visible;
	GladeXmlNode *child;

	g_return_val_if_fail (property_class != NULL, FALSE);

	/* for code cleanliness... */
	class = *property_class;

	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (class), FALSE);
	g_return_val_if_fail (glade_xml_node_verify (node, GLADE_TAG_PROPERTY), FALSE);

	/* check the id */
	buff = glade_xml_get_property_string_required (node, GLADE_TAG_ID, NULL);
	if (!buff)
		return FALSE;
	g_free (buff);

	/* If Disabled="TRUE" we set *property_class to NULL, but we return TRUE.
	 * The caller may want to remove this property from its list.
	 */
	if (glade_xml_get_property_boolean (node, GLADE_TAG_DISABLED, FALSE))
	{
		glade_property_class_free (class);
		*property_class = NULL;
		return TRUE;
	}

	visible = glade_xml_get_property_string (node, "Visible");
	if (visible)
	{
		if (!g_module_symbol (widget_class->module, visible, (void **) &class->visible))
			g_warning ("Could not find %s\n", visible);

		g_free (visible);
	}

	/* If needed, update the name... */
	buff = glade_xml_get_property_string (node, GLADE_TAG_NAME);
	if (buff)
	{
		g_free (class->name);
		class->name = buff;
	}

	/* ...the type... */
	buff = glade_xml_get_value_string (node, GLADE_TAG_TYPE);
	if (buff)
	{
		GladePropertyType type;
		type = glade_property_type_str_to_enum (buff);
		g_free (buff);
		if (type == GLADE_PROPERTY_TYPE_ERROR)
			return FALSE;
		class->type = type;
	}

	/* ...and the tooltip */
	buff = glade_xml_get_value_string (node, GLADE_TAG_TOOLTIP);
	if (buff)
	{
		g_free (class->tooltip);
		class->tooltip = buff;
	}

	/* Get the Parameters */
	child = glade_xml_search_child (node, GLADE_TAG_PARAMETERS);
	if (child)
		class->parameters = glade_parameter_list_new_from_node (class->parameters, child);
	glade_parameter_get_boolean (class->parameters, "Optional", &class->optional);

	/* Get the choices */
	do
	{
		if (class->type == GLADE_PROPERTY_TYPE_ENUM)
		{
			gchar *type_name;
			GType type;

			child = glade_xml_search_child (node, GLADE_TAG_ENUMS);
			if (!child)
				break;
			class->choices = glade_choice_list_new_from_node (child);
			type_name = glade_xml_get_property_string (child, "EnumType");
			if (!type_name)
				break;
			type = g_type_from_name (type_name);
			if (!(type != 0))
				break;
			class->enum_type = type;
		}
	}
	while (FALSE);
		
#if 0
	/* If the property is an object load it */
	if (class->type == GLADE_PROPERTY_TYPE_OBJECT) {
		child = glade_xml_search_child (node, GLADE_TAG_GLADE_WIDGET_CLASS);
		if (child)
			class->child = glade_widget_class_new_from_node (child);
	}
#endif
	/* Get the default */
	buff = glade_xml_get_property_string (node, GLADE_TAG_DEFAULT);
	if (buff)
	{
		if (class->def)
		{
			if (G_VALUE_TYPE (class->def) != 0)
				g_value_unset (class->def);
			g_free (class->def);
		}
		class->def = glade_property_class_make_gvalue_from_string (class, buff);
		if (!class->def)
			return FALSE;
	}

	/* common, optional, etc */
	class->common   = glade_xml_get_property_boolean (node, GLADE_TAG_COMMON,  FALSE);
	class->optional = glade_xml_get_property_boolean (node, GLADE_TAG_OPTIONAL, FALSE);
	if (class->optional)
		class->optional_default = glade_xml_get_property_boolean (node, GLADE_TAG_OPTIONAL_DEFAULT, FALSE);

	/* If this property can't be set with g_object_set, get the work around
	 * function
	 */
	/* I use here a g_warning to signal these errors instead of a dialog box, as if there is one
	 * of this kind of errors, there will probably a lot of them, and we don't want to inflict
	 * the user the pain of plenty of dialog boxes.  Ideally, we should collect these errors,
	 * and show all of them at the end of the load processus. */
	child = glade_xml_search_child (node, GLADE_TAG_SET_FUNCTION);
	if (child)
	{
		gchar *symbol_name = glade_xml_get_content (child);

		if (!widget_class->module)
			g_warning (_("The property [%s] of the widget's class [%s] needs a special \"set\" function, but there is no library associated to this widget's class."),
				   class->name, widget_class->name);

		if (!g_module_symbol (widget_class->module, symbol_name, (gpointer *) &class->set_function))
			g_warning (_("Unable to get the \"set\" function [%s] of the property [%s] of the widget's class [%s] from the module [%s]: %s"),
				   symbol_name, class->name, widget_class->name, g_module_name (widget_class->module), g_module_error ());

		g_free (symbol_name);
	}

	/* If this property can't be get with g_object_get, get the work around
	 * function
	 */
	child = glade_xml_search_child (node, GLADE_TAG_GET_FUNCTION);
	if (child)
	{
		gchar *symbol_name = glade_xml_get_content (child);

		if (!widget_class->module)
			g_warning (_("The property [%s] of the widget's class [%s] needs a special \"get\" function, but there is no library associated to this widget's class."),
				   class->name, widget_class->name);

		if (!g_module_symbol(widget_class->module, symbol_name, (gpointer *) &class->get_function))
			g_warning (_("Unable to get the \"get\" function [%s] of the property [%s] of the widget's class [%s] from the module [%s]: %s"),
				   symbol_name, class->name, widget_class->name, g_module_name (widget_class->module), g_module_error ());

		g_free (symbol_name);
	}

	/* notify that we changed the property class */
	class->is_modified = TRUE;

	return TRUE;
}
