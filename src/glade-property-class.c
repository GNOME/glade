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
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-parameter.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-editor.h"
#include "glade-debug.h"

/**
 * glade_property_class_new:
 *
 * Returns: a new #GladePropertyClass
 */
GladePropertyClass *
glade_property_class_new (void)
{
	GladePropertyClass *property_class;

	property_class = g_new0 (GladePropertyClass, 1);
	property_class->pspec = NULL;
	property_class->id = NULL;
	property_class->name = NULL;
	property_class->tooltip = NULL;
	property_class->def = NULL;
	property_class->orig_def = NULL;
	property_class->parameters = NULL;
	property_class->query = FALSE;
	property_class->optional = FALSE;
	property_class->optional_default = TRUE;
	property_class->common = FALSE;
	property_class->packing = FALSE;
	property_class->is_modified = FALSE;
	property_class->is_modified = FALSE;
	property_class->verify_function = NULL;
	property_class->set_function = NULL;
	property_class->get_function = NULL;
	property_class->visible = FALSE;

	return property_class;
}

/**
 * glade_property_class_clone:
 * @property_class: a #GladePropertyClass
 *
 * Returns: a new #GladePropertyClass cloned from @property_class
 */
GladePropertyClass *
glade_property_class_clone (GladePropertyClass *property_class)
{
	GladePropertyClass *clone;

	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (property_class), NULL);

	clone = g_new0 (GladePropertyClass, 1);

	memcpy (clone, property_class, sizeof(GladePropertyClass));

	clone->pspec = property_class->pspec;
	clone->id = g_strdup (clone->id);
	clone->name = g_strdup (clone->name);
	clone->tooltip = g_strdup (clone->tooltip);

	if (G_IS_VALUE (property_class->def))
	{
		clone->def = g_new0 (GValue, 1);
		g_value_init (clone->def, property_class->pspec->value_type);
		g_value_copy (property_class->def, clone->def);
	}

	if (G_IS_VALUE (property_class->orig_def))
	{
		clone->orig_def = g_new0 (GValue, 1);
		g_value_init (clone->orig_def, property_class->pspec->value_type);
		g_value_copy (property_class->orig_def, clone->orig_def);
	}

	if (clone->parameters)
	{
		GList *parameter;

		clone->parameters = g_list_copy (clone->parameters);

		for (parameter = clone->parameters;
		     parameter != NULL;
		     parameter = parameter->next)
			parameter->data =
				glade_parameter_clone ((GladeParameter*) parameter->data);
	}

	return clone;
}

/**
 * glade_property_class_free:
 * @class: a #GladePropertyClass
 *
 * Frees @class and its associated memory.
 */
void
glade_property_class_free (GladePropertyClass *class)
{
	if (class == NULL)
		return;

	g_return_if_fail (GLADE_IS_PROPERTY_CLASS (class));

	g_free (class->id);
	g_free (class->tooltip);
	g_free (class->name);
	if (class->orig_def)
	{
		if (G_VALUE_TYPE (class->orig_def) != 0)
			g_value_unset (class->orig_def);
		g_free (class->orig_def);
	}
	if (class->def)
	{
		if (G_VALUE_TYPE (class->def) != 0)
			g_value_unset (class->def);
		g_free (class->def);
	}
	g_list_foreach (class->parameters, (GFunc) glade_parameter_free, NULL);
	g_list_free (class->parameters);
	g_free (class);
}


static GValue *
glade_property_class_get_default_from_spec (GParamSpec *spec,
					    GladePropertyClass *class)
{
	GValue *value;
	value = g_new0 (GValue, 1);
	g_value_init (value, spec->value_type);
	g_param_value_set_default (spec, value);
	return value;
}


static gchar *
glade_property_class_make_string_from_enum (GType etype, gint eval)
{
	GEnumClass *eclass;
	gchar      *string = NULL;
	guint       i;

	g_return_val_if_fail ((eclass = g_type_class_ref (etype)) != NULL, NULL);
	for (i = 0; i < eclass->n_values; i++)
	{
		if (eval == eclass->values[i].value)
		{
			string = g_strdup (eclass->values[i].value_name);
			break;
		}
	}
	g_type_class_unref (eclass);
	return string;
}

static gchar *
glade_property_class_make_string_from_flags (GType ftype, guint fval)
{
	GFlagsClass *fclass;
	gchar       *string = NULL;
	guint        i;
	g_return_val_if_fail ((fclass = g_type_class_ref (ftype)) != NULL, NULL);

	for (i = 0; i < fclass->n_values; i++)
	{
		if (fval & fclass->values[i].value)
		{
			if (!string)
				string = g_strdup (fclass->values[i].value_name);
			else
			{
				gchar *newstr =
					g_strconcat (string, "|",
						     fclass->values[i].value_name,
						     NULL);
				string = (g_free (string), newstr);
			}
		}
	}
	g_type_class_unref (fclass);
	return string;
}

/**
 * glade_property_class_make_string_from_gvalue:
 * @property_class:
 * @value:
 *
 * TODO: write me
 *
 * Returns:
 */
gchar *
glade_property_class_make_string_from_gvalue (GladePropertyClass *property_class,
					      const GValue *value)
{
	gchar *string = NULL;

	if (G_IS_PARAM_SPEC_ENUM(property_class->pspec))
	{
		gint eval = g_value_get_enum (value);
		string = glade_property_class_make_string_from_enum
			(property_class->pspec->value_type, eval);
	}
	else if (G_IS_PARAM_SPEC_FLAGS(property_class->pspec))
	{
		guint flags = g_value_get_flags (value);
		string = glade_property_class_make_string_from_flags
			(property_class->pspec->value_type, flags);
	}
	else if (G_IS_PARAM_SPEC_INT(property_class->pspec))
		string = g_strdup_printf ("%d", g_value_get_int (value));
	else if (G_IS_PARAM_SPEC_UINT(property_class->pspec))
		string = g_strdup_printf ("%u", g_value_get_uint (value));
	else if (G_IS_PARAM_SPEC_LONG(property_class->pspec))
		string = g_strdup_printf ("%ld", g_value_get_long (value));
	else if (G_IS_PARAM_SPEC_ULONG(property_class->pspec))
		string = g_strdup_printf ("%lu", g_value_get_ulong (value));
	else if (G_IS_PARAM_SPEC_INT64(property_class->pspec))
		string = g_strdup_printf ("%lld", g_value_get_int64 (value));
	else if (G_IS_PARAM_SPEC_UINT64(property_class->pspec))
		string = g_strdup_printf ("%llu", g_value_get_uint64 (value));
	else if (G_IS_PARAM_SPEC_FLOAT(property_class->pspec))
		string = g_strdup_printf ("%f", g_value_get_float (value));
	else if (G_IS_PARAM_SPEC_DOUBLE(property_class->pspec))
		string = g_strdup_printf ("%g", g_value_get_double (value));
	else if (G_IS_PARAM_SPEC_STRING(property_class->pspec))
		string = g_value_dup_string (value);
	else if (G_IS_PARAM_SPEC_CHAR(property_class->pspec))
		string = g_strdup_printf ("%c", g_value_get_char (value));
	else if (G_IS_PARAM_SPEC_UCHAR(property_class->pspec))
		string = g_strdup_printf ("%c", g_value_get_uchar (value));
	else if (G_IS_PARAM_SPEC_UNICHAR(property_class->pspec))
	{
		string = g_malloc (7);
		g_unichar_to_utf8 (g_value_get_uint (value), string);
		*g_utf8_next_char(string) = '\0';
	}
	else if (G_IS_PARAM_SPEC_BOOLEAN(property_class->pspec))
		string = g_strdup_printf ("%s", g_value_get_boolean (value) ?
					  GLADE_TAG_TRUE : GLADE_TAG_FALSE);
	else
		g_critical ("Unsupported pspec type %s",
			    g_type_name(property_class->pspec->value_type));

	return string;
}

/* This is copied exactly from libglade. I've just renamed the function.
 *
 * TODO: Expose function from libglade and use the one in libglade (once
 * Ivan Wongs libglade patch gets in...).
 */
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

/* This is copied exactly from libglade. I've just renamed the function.
 *
 * TODO: Expose function from libglade and use the one in libglade (once
 * Ivan Wongs libglade patch gets in...).
 */
static gint
glade_property_class_make_enum_from_string (GType type, const char *string)
{
    GEnumClass *eclass;
    GEnumValue *ev;
    gchar *endptr;
    gint ret = 0;

    ret = strtoul(string, &endptr, 0);
    if (endptr != string) /* parsed a number */
	return ret;

    eclass = g_type_class_ref(type);
    ev = g_enum_get_value_by_name(eclass, string);
    if (!ev) ev = g_enum_get_value_by_nick(eclass, string);
    if (ev)  ret = ev->value;

    g_type_class_unref(eclass);

    return ret;
}


/**
 * glade_property_class_make_gvalue_from_string:
 * @property_class:
 * @string:
 *
 * TODO: write me
 *
 * Returns:
 */
GValue *
glade_property_class_make_gvalue_from_string (GladePropertyClass *property_class,
					      const gchar *string)
{
	GValue *value = g_new0 (GValue, 1);

	g_value_init (value, property_class->pspec->value_type);

	if (G_IS_PARAM_SPEC_ENUM(property_class->pspec))
	{
		gint eval = glade_property_class_make_enum_from_string
			(property_class->pspec->value_type, string);
		g_value_set_enum (value, eval);
	}
	else if (G_IS_PARAM_SPEC_FLAGS(property_class->pspec))
	{
		guint flags = glade_property_class_make_flags_from_string
			(property_class->pspec->value_type, string);
		g_value_set_flags (value, flags);
	}
	else if (G_IS_PARAM_SPEC_INT(property_class->pspec))
		g_value_set_int (value, atoi (string));
	else if (G_IS_PARAM_SPEC_UINT(property_class->pspec))
		g_value_set_uint (value, atoi (string));
	else if (G_IS_PARAM_SPEC_LONG(property_class->pspec))
		g_value_set_long (value, strtol (string, NULL, 10));
	else if (G_IS_PARAM_SPEC_ULONG(property_class->pspec))
		g_value_set_ulong (value, strtoul (string, NULL, 10));
	else if (G_IS_PARAM_SPEC_INT64(property_class->pspec))
#ifndef G_OS_WIN32
		g_value_set_int64 (value, strtoll (string, NULL, 10));
#else
		g_value_set_int64 (value, _atoi64 (string));
#endif
	else if (G_IS_PARAM_SPEC_UINT64(property_class->pspec))
		g_value_set_uint64 (value, g_ascii_strtoull (string, NULL, 10));
	else if (G_IS_PARAM_SPEC_FLOAT(property_class->pspec))
		g_value_set_float (value, (float) atof (string));
	else if (G_IS_PARAM_SPEC_DOUBLE(property_class->pspec))
		g_value_set_double (value, (float) atof (string));
	else if (G_IS_PARAM_SPEC_STRING(property_class->pspec))
		g_value_set_string (value, string);
	else if (G_IS_PARAM_SPEC_CHAR(property_class->pspec))
		g_value_set_char (value, string[0]);
	else if (G_IS_PARAM_SPEC_UCHAR(property_class->pspec))
		g_value_set_uchar (value, string[0]);
	else if (G_IS_PARAM_SPEC_UNICHAR(property_class->pspec))
		g_value_set_uint (value, g_utf8_get_char (string));
	else if (G_IS_PARAM_SPEC_BOOLEAN(property_class->pspec))
	{
		if (strcmp (string, GLADE_TAG_TRUE) == 0)
			g_value_set_boolean (value, TRUE);
		else
			g_value_set_boolean (value, FALSE);
	}
	else
		g_critical ("Unsupported pspec type %s",
			    g_type_name(property_class->pspec->value_type));
	return value;
}

/**
 * glade_property_class_new_from_spec:
 * @spec:
 *
 * TODO: write me
 *
 * Returns:
 */
GladePropertyClass *
glade_property_class_new_from_spec (GParamSpec *spec)
{
	GladePropertyClass *property_class;

	property_class = glade_property_class_new ();

	property_class->pspec = spec;

	/* Register only editable properties.
	 */
	if (!glade_editor_editable_property (property_class->pspec))
		goto lblError;
	
	property_class->id   = g_strdup (spec->name);
	property_class->name = g_strdup (g_param_spec_get_nick (spec));
	if (!property_class->id || !property_class->name)
	{
		g_warning ("Failed to create property class from spec");
		goto lblError;
	}

	property_class->tooltip = g_strdup (g_param_spec_get_blurb (spec));
	property_class->def = glade_property_class_get_default_from_spec (spec, property_class);

	return property_class;

  lblError:
	glade_property_class_free (property_class);
	return NULL;
}

/**
 * glade_property_class_is_visible:
 * @property_class:
 * @widget_class:
 *
 * TODO: write me
 *
 * Returns:
 */
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
 * Returns: %TRUE on success. @property_class is set to NULL if the property
 *          has Disabled="TRUE".
 */
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

	/* ...the spec... */
	buff = glade_xml_get_value_string (node, GLADE_TAG_SPEC);
	if (buff)
	{
		class->pspec = glade_utils_get_pspec_from_funcname (buff);
 		g_free (buff);
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
		
	/* Get the default */
	buff = glade_xml_get_property_string (node, GLADE_TAG_DEFAULT);
	if (buff)
	{
		if (class->def)
			class->orig_def = class->def;
		class->def = glade_property_class_make_gvalue_from_string (class, buff);
		g_free (buff);
		if (!class->def)
			return FALSE;
	}

	/* common, optional, etc */
	class->common   = glade_xml_get_property_boolean (node, GLADE_TAG_COMMON,  FALSE);
	class->optional = glade_xml_get_property_boolean (node, GLADE_TAG_OPTIONAL, FALSE);
	class->query    = glade_xml_get_property_boolean (node, GLADE_TAG_QUERY, FALSE);

	if (class->optional)
		class->optional_default =
			glade_xml_get_property_boolean (node, GLADE_TAG_OPTIONAL_DEFAULT, FALSE);

	/* If this property can't be set with g_object_set, get the work around
	 * function
	 */
	/* I use here a g_warning to signal these errors instead of a dialog 
         * box, as if there is one of this kind of errors, there will probably 
         * a lot of them, and we don't want to inflict the user the pain of 
         * plenty of dialog boxes.  Ideally, we should collect these errors, 
         * and show all of them at the end of the load process.
         */
	child = glade_xml_search_child (node, GLADE_TAG_SET_FUNCTION);
	if (child)
	{
		gchar *symbol_name = glade_xml_get_content (child);

		if (!widget_class->module)
			g_warning (_("The property [%s] of the widget's class [%s] "
				     "needs a special \"set\" function, but there is "
				     "no library associated to this widget's class."),
				   class->name, widget_class->name);

		if (!g_module_symbol (widget_class->module, symbol_name, (gpointer *)
				      &class->set_function))
			g_warning (_("Unable to get the \"set\" function [%s] of the "
				     "property [%s] of the widget's class [%s] from "
				     "the module [%s]: %s"),
				   symbol_name, class->name, widget_class->name,
				   g_module_name (widget_class->module), g_module_error ());
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
			g_warning (_("The property [%s] of the widget's class [%s] needs a "
				     "special \"get\" function, but there is no library "
				     "associated to this widget's class."),
				   class->name, widget_class->name);

		if (!g_module_symbol(widget_class->module, symbol_name,
				     (gpointer *) &class->get_function))
			g_warning (_("Unable to get the \"get\" function [%s] of the "
				     "property [%s] of the widget's class [%s] from the "
				     "module [%s]: %s"),
				   symbol_name, class->name, widget_class->name,
				   g_module_name (widget_class->module), g_module_error ());
		g_free (symbol_name);
	}

	child = glade_xml_search_child (node, GLADE_TAG_VERIFY_FUNCTION);
	if (child)
	{
		gchar *symbol_name = glade_xml_get_content (child);

		if (!widget_class->module)
			g_warning (_("The property [%s] of the widget's class [%s] needs a "
				     "special \"get\" function, but there is no library "
				     "associated to this widget's class."),
				   class->name, widget_class->name);

		if (!g_module_symbol(widget_class->module, symbol_name,
				     (gpointer *) &class->verify_function))
			g_warning (_("Unable to get the \"verify\" function [%s] of the "
				     "property [%s] of the widget's class [%s] from the "
				     "module [%s]: %s"),
				   symbol_name, class->name, widget_class->name,
				   g_module_name (widget_class->module), g_module_error ());
		g_free (symbol_name);
	}

	/* notify that we changed the property class */
	class->is_modified = TRUE;

	return TRUE;
}
