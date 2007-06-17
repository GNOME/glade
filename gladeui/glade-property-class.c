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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <gmodule.h>
#include <glib/gi18n-lib.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-parameter.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-editor-property.h"
#include "glade-debug.h"

#define NUMERICAL_STEP_INCREMENT   1
#define FLOATING_STEP_INCREMENT    0.01F
#define NUMERICAL_PAGE_INCREMENT   10
#define NUMERICAL_PAGE_SIZE        1


/* Hardcoded recognized atk actions
 */
typedef struct {
	gchar *prop_name;
	gchar *id;
	gchar *name;
	gchar *tooltip;
} GPCAtkPropertyTab;

static const GPCAtkPropertyTab action_names_table[] = {
	{ "click",   "atk-click",   N_("Click"),   
	  N_("Set the description of the Click atk action") },
	{ "press",   "atk-press",   N_("Press"),   
	  N_("Set the description of the Press atk action") },
	{ "release", "atk-release", N_("Release"), 
	  N_("Set the description of the Release atk action") },
	{ "activate", "atk-activate", N_("Activate"), 
	  N_("Set the description of the Activate atk action") }
};

static const GPCAtkPropertyTab relation_names_table[] = {
	{ "controlled-by", "atk-controlled-by", N_("Controlled By"),
	  N_("Indicates an object controlled by one or more target objects") },

	{ "controlled-for", "atk-controlled-for", N_("Controller For"),
	  N_("Indicates an object is a controller for one or more target objects") },

	{ "labelled-by", "atk-labelled-by", N_("Labelled By"),
	  N_("Indicates an object is labelled by one or more target objects") },

	{ "label-for", "atk-label-for", N_("Label For"),
	  N_("Indicates an object is a label for one or more target objects") },

	{ "member-of", "atk-member-of", N_("Member Of"),
	  N_("Indicates an object is a member of a group of one or more target objects") },

	{ "child-node-of", "atk-child-node-of", N_("Child Node Of"),
	  N_("Indicates an object is a cell in a treetable which is displayed "
	     "because a cell in the same column is expanded and identifies that cell") },

	{ "flows-to", "atk-flows-to", N_("Flows To"),
	  N_("Indicates that the object has content that flows logically to another "
	     "AtkObject in a sequential way (text-flow, for instance).") },

	{ "flows-from", "atk-flows-from", N_("Flows From"),
	  N_("Indicates that the object has content that flows logically from another "
	     "AtkObject in a sequential way, (for instance text-flow)") },

	{ "subwindow-of", "atk-subwindow-of", N_("Subwindow Of"),
	  N_("Indicates a subwindow attached to a component but otherwise has no "
	     "connection in the UI hierarchy to that component") },

	{ "embeds", "atk-embeds", N_("Embeds"),
	  N_("Indicates that the object visually embeds another object's content, "
	     "i.e. this object's content flows around another's content") },

	{ "embedded-by", "atk-embedded-by", N_("Embedded By"),
	  N_("Inverse of 'Embeds', indicates that this object's content "
	     "is visually embedded in another object") },

	{ "popup-for", "atk-popup-for", N_("Popup For"),
	  N_("Indicates that an object is a popup for another object") },

	{ "parent-window-of", "atk-parent-window-of", N_("Parent Window Of"),
	  N_("Indicates that an object is a parent window of another object") }
};


/**
 * glade_property_class_atk_realname:
 * @atk_name: The id of the atk property
 *
 * Translates a GladePropertyClass->id to the name that should be
 * saved into the glade file.
 *
 * Returns: a pointer to a constant string.
 */
G_CONST_RETURN gchar *
glade_property_class_atk_realname (const gchar *atk_name)
{
	gint i;

	g_return_val_if_fail (atk_name != NULL, NULL);

	for (i = 0; i < G_N_ELEMENTS (action_names_table); i++)
		if (!strcmp (action_names_table[i].id, atk_name))
			return action_names_table[i].prop_name;

	for (i = 0; i < G_N_ELEMENTS (relation_names_table); i++)
		if (!strcmp (relation_names_table[i].id, atk_name))
			return relation_names_table[i].prop_name;

	return atk_name;
}

/**
 * glade_property_class_new:
 * @handle: A generic pointer (i.e. a #GladeWidgetClass)
 *
 * Returns: a new #GladePropertyClass
 */
GladePropertyClass *
glade_property_class_new (gpointer handle)
{
	GladePropertyClass *property_class;

	property_class = g_new0 (GladePropertyClass, 1);
	property_class->handle = handle;
	property_class->pspec = NULL;
	property_class->id = NULL;
	property_class->name = NULL;
	property_class->tooltip = NULL;
	property_class->def = NULL;
	property_class->orig_def = NULL;
	property_class->parameters = NULL;
	property_class->displayable_values = NULL;
	property_class->query = FALSE;
	property_class->optional = FALSE;
	property_class->optional_default = FALSE;
	property_class->common = FALSE;
	property_class->packing = FALSE;
	property_class->is_modified = FALSE;
	property_class->visible = TRUE;
	property_class->save = TRUE;
	property_class->save_always = FALSE;
	property_class->ignore = FALSE;
	property_class->resource = FALSE;
	property_class->translatable = FALSE;
	property_class->type = GPC_NORMAL;
	property_class->virt = TRUE;
	property_class->transfer_on_paste = FALSE;
	property_class->weight = -1.0;

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
	
	if (property_class->displayable_values)
	{
		gint i, len;
		GEnumValue val;
		GArray *disp_val;
		
		disp_val = property_class->displayable_values;
		len = disp_val->len;
		
		clone->displayable_values = g_array_new(FALSE, TRUE, sizeof(GEnumValue));
		
		for (i = 0; i < len; i++)
		{
			val.value = g_array_index(disp_val, GEnumValue, i).value;
			val.value_name = g_strdup (g_array_index(disp_val, GEnumValue, i).value_name);
			val.value_nick = g_strdup (g_array_index(disp_val, GEnumValue, i).value_nick);
			
			g_array_append_val(clone->displayable_values, val);
		}
	}
	
	return clone;
}

/**
 * glade_property_class_free:
 * @property_class: a #GladePropertyClass
 *
 * Frees @klass and its associated memory.
 */
void
glade_property_class_free (GladePropertyClass *property_class)
{
	if (property_class == NULL)
		return;

	g_return_if_fail (GLADE_IS_PROPERTY_CLASS (property_class));

	g_free (property_class->id);
	g_free (property_class->tooltip);
	g_free (property_class->name);
	if (property_class->orig_def)
	{
		if (G_VALUE_TYPE (property_class->orig_def) != 0)
			g_value_unset (property_class->orig_def);
		g_free (property_class->orig_def);
	}
	if (property_class->def)
	{
		if (G_VALUE_TYPE (property_class->def) != 0)
			g_value_unset (property_class->def);
		g_free (property_class->def);
	}
	g_list_foreach (property_class->parameters, (GFunc) glade_parameter_free, NULL);
	g_list_free (property_class->parameters);
	
	if (property_class->displayable_values)
	{
		gint i, len;
		GArray *disp_val;
		
		disp_val = property_class->displayable_values;
		len = disp_val->len;
		
		for (i = 0; i < len; i++)
		{
			gchar *name, *nick;
			
			name = (gchar *) g_array_index (disp_val, GEnumValue, i).value_name;
			if (name) g_free(name);
			
			nick = (gchar *) g_array_index (disp_val, GEnumValue, i).value_nick;
			if (nick) g_free(nick);
		}
		
		g_array_free(disp_val, TRUE);
	}	
	
	g_free (property_class);
}


static GValue *
glade_property_class_get_default_from_spec (GParamSpec *spec)
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
glade_property_class_make_string_from_flags (GladePropertyClass *klass, guint fvals, gboolean displayables)
{
	GFlagsClass *fclass;
	GFlagsValue *fvalue;
	GString *string;
	gchar *retval;
	
	g_return_val_if_fail ((fclass = g_type_class_ref (klass->pspec->value_type)) != NULL, NULL);
	
	string = g_string_new("");
	
	while ((fvalue = g_flags_get_first_value(fclass, fvals)) != NULL)
	{
		const gchar *val_str = NULL;
		
		fvals &= ~fvalue->value;
		
		if (displayables)
			val_str = glade_property_class_get_displayable_value(klass, fvalue->value);
		
		if (string->str[0])
			g_string_append(string, " | ");
		
		g_string_append (string, (val_str) ? val_str : fvalue->value_name);
		
		/* If one of the flags value is 0 this loop become infinite :) */
		if (fvalue->value == 0) break;
	}
	
	retval = string->str;
	
	g_type_class_unref (fclass);
	g_string_free(string, FALSE);
	
	return retval;
}

static gchar *
glade_property_class_make_string_from_object (GladePropertyClass *property_class,
					      GObject            *object)
{
	GladeWidget *gwidget;
	gchar       *string = NULL, *filename;

	if (!object) return NULL;

	if (property_class->pspec->value_type == GDK_TYPE_PIXBUF)
	{
		if ((filename = g_object_get_data (object, "GladeFileName")) != NULL)
			string = g_path_get_basename (filename);
	}
	else if (property_class->pspec->value_type == GTK_TYPE_ADJUSTMENT)
	{
		GtkAdjustment *adj = GTK_ADJUSTMENT (object);
		GString       *str = g_string_sized_new (G_ASCII_DTOSTR_BUF_SIZE * 6 + 6);
		gchar          buff[G_ASCII_DTOSTR_BUF_SIZE];

		g_ascii_dtostr (buff, sizeof (buff), adj->value);
		g_string_append (str, buff);

		g_string_append_c (str, ' ');
		g_ascii_dtostr (buff, sizeof (buff), adj->lower);
		g_string_append (str, buff);

		g_string_append_c (str, ' ');
		g_ascii_dtostr (buff, sizeof (buff), adj->upper);
		g_string_append (str, buff);

		g_string_append_c (str, ' ');
		g_ascii_dtostr (buff, sizeof (buff), adj->step_increment);
		g_string_append (str, buff);

		g_string_append_c (str, ' ');
		g_ascii_dtostr (buff, sizeof (buff), adj->page_increment);
		g_string_append (str, buff);

		g_string_append_c (str, ' ');
		g_ascii_dtostr (buff, sizeof (buff), adj->page_size);
		g_string_append (str, buff);

		string = g_string_free (str, FALSE);
	}
	else if ((gwidget = glade_widget_get_from_gobject (object)) != NULL)
		string = g_strdup (gwidget->name);
	else
		g_critical ("Object type property refers to an object "
			    "outside the project");
	
	return string;
}

static gchar *
glade_property_class_make_string_from_objects (GladePropertyClass *property_class,
					       GList              *objects)
{
	GObject *object;
	GList   *list;
	gchar   *string = NULL, *obj_str, *tmp;

	for (list = objects; list; list = list->next)
	{
		object = list->data;

		obj_str = glade_property_class_make_string_from_object 
			(property_class, object);

		if (string == NULL)
			string = obj_str;
		else if (obj_str != NULL)
		{
			tmp = g_strdup_printf ("%s%s%s", string, GPC_OBJECT_DELIMITER, obj_str);
			string = (g_free (string), tmp);
			g_free (obj_str);
		}
	}
	return string;
}

/* This is not used to save in the glade file... and its a one-way conversion.
 * its only usefull to show the values in the UI.
 */
static gchar *
glade_property_class_make_string_from_accels (GladePropertyClass *property_class,
					      GList              *accels)
{
	GladeAccelInfo *info;
	GString        *string;
	GList          *list;

	string = g_string_new ("");

	for (list = accels; list; list = list->next)
	{
		info = list->data;
		
		if (info->modifiers & GDK_SHIFT_MASK)
			g_string_append (string, "SHIFT-");

		if (info->modifiers & GDK_CONTROL_MASK)
			g_string_append (string, "CNTL-");

		if (info->modifiers & GDK_MOD1_MASK)
			g_string_append (string, "ALT-");

		g_string_append (string, glade_builtin_string_from_key (info->key));

		if (list->next)
			g_string_append (string, ", ");
	}

	return g_string_free (string, FALSE);
}

/**
 * glade_property_class_make_string_from_gvalue:
 * @property_class: A #GladePropertyClass
 * @value: A #GValue
 *
 * Returns: A newly allocated string representation of @value
 */
gchar *
glade_property_class_make_string_from_gvalue (GladePropertyClass *property_class,
					      const GValue *value)
{
	gchar    *string = NULL, **strv, str[G_ASCII_DTOSTR_BUF_SIZE];
	GObject  *object;
	GdkColor *color;
	GList    *objects, *accels;

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
			(property_class, flags, FALSE);
	}
	else if (G_IS_PARAM_SPEC_VALUE_ARRAY (property_class->pspec))
	{
		GValueArray *value_array = g_value_get_boxed (value);
		
		if (value_array && value_array->n_values &&
		   G_VALUE_HOLDS (&value_array->values [0], G_TYPE_STRING))
		{
			gint i, n_values = value_array->n_values;
			GString *gstring = g_string_new (NULL);
			
			for (i = 0; i < n_values; i++)
			{
				g_string_append (gstring, g_value_get_string (&value_array->values [i]));
				g_string_append_c (gstring, '\n');
			}
			string = gstring->str;
			g_string_free (gstring, FALSE);
		}
	}
	else if (G_IS_PARAM_SPEC_BOXED(property_class->pspec))
	{
		if (property_class->pspec->value_type == GDK_TYPE_COLOR)
		{
			color  = g_value_get_boxed (value);
			if (color)
				string = g_strdup_printf ("#%04x%04x%04x", 
							  color->red, 
							  color->green,
							  color->blue);
		}
		else if (property_class->pspec->value_type == G_TYPE_STRV)
		{
			strv = g_value_get_boxed (value);
			if (strv) string = g_strjoinv ("\n", strv);
		}
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
		string = g_strdup_printf ("%" G_GINT64_FORMAT, g_value_get_int64 (value));
	else if (G_IS_PARAM_SPEC_UINT64(property_class->pspec))
		string = g_strdup_printf ("%" G_GUINT64_FORMAT, g_value_get_uint64 (value));
	else if (G_IS_PARAM_SPEC_FLOAT(property_class->pspec))
	{
		g_ascii_dtostr (str, sizeof (str), g_value_get_float (value));
		string = g_strdup (str);
	}
	else if (G_IS_PARAM_SPEC_DOUBLE(property_class->pspec))
	{
		g_ascii_dtostr (str, sizeof (str), g_value_get_double (value));
		string = g_strdup (str);
	}
	else if (G_IS_PARAM_SPEC_STRING(property_class->pspec))
	{
		if (property_class->resource && g_value_get_string (value) != NULL)
			string = g_path_get_basename 
				(g_value_get_string (value));
		else
			string = g_value_dup_string (value);
	}
	else if (G_IS_PARAM_SPEC_CHAR(property_class->pspec))
		string = g_strdup_printf ("%c", g_value_get_char (value));
	else if (G_IS_PARAM_SPEC_UCHAR(property_class->pspec))
		string = g_strdup_printf ("%c", g_value_get_uchar (value));
	else if (G_IS_PARAM_SPEC_UNICHAR(property_class->pspec))
	{
                int len;
                string = g_malloc (7);
                len = g_unichar_to_utf8 (g_value_get_uint (value), string);
                string[len] = '\0';
	}
	else if (G_IS_PARAM_SPEC_BOOLEAN(property_class->pspec))
		string = g_strdup_printf ("%s", g_value_get_boolean (value) ?
					  GLADE_TAG_TRUE : GLADE_TAG_FALSE);
	else if (G_IS_PARAM_SPEC_OBJECT(property_class->pspec))
	{
		object = g_value_get_object (value);
		string = glade_property_class_make_string_from_object
			(property_class, object);
	}
	else if (GLADE_IS_PARAM_SPEC_OBJECTS (property_class->pspec))
	{
		objects = g_value_get_boxed (value);
		string = glade_property_class_make_string_from_objects
			(property_class, objects);
	}
	else if (GLADE_IS_PARAM_SPEC_ACCEL (property_class->pspec))
	{
		accels = g_value_get_boxed (value);
		string = glade_property_class_make_string_from_accels 
			(property_class, accels);
	}
	else
		g_critical ("Unsupported pspec type %s",
			    g_type_name(G_PARAM_SPEC_TYPE (property_class->pspec)));

	return string;
}

/* This is copied exactly from libglade. I've just renamed the function.
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

static GObject *
glade_property_class_make_object_from_string (GladePropertyClass *property_class,
					      const gchar        *string,
					      GladeProject       *project)
{
	GObject *object = NULL;
	gchar   *fullpath;

	if (string == NULL) return NULL;
	
	if (property_class->pspec->value_type == GDK_TYPE_PIXBUF && project)
	{
		GdkPixbuf *pixbuf;
		
		fullpath = glade_project_resource_fullpath (project, string);
 
		if ((pixbuf = gdk_pixbuf_new_from_file (fullpath, NULL)) == NULL)
		{
			static GdkPixbuf *icon = NULL;

			if (icon == NULL)
			{
				GtkWidget *widget = gtk_label_new ("");
				icon = gtk_widget_render_icon (widget,
							       GTK_STOCK_MISSING_IMAGE,
							       GTK_ICON_SIZE_MENU, NULL);
				gtk_object_sink (GTK_OBJECT (widget));
			}
			
			pixbuf = gdk_pixbuf_copy (icon);
		}
 
		if (pixbuf)
		{
			object = G_OBJECT (pixbuf);
			g_object_set_data_full (object, "GladeFileName",
						g_strdup (string), g_free);
		}
 
		g_free (fullpath);
	}
	if (property_class->pspec->value_type == GTK_TYPE_ADJUSTMENT)
	{
		gdouble value, lower, upper, step_increment, page_increment, page_size;
                gchar *pstring = (gchar*) string;

                value = g_ascii_strtod (pstring, &pstring);
                lower = g_ascii_strtod (pstring, &pstring);
                upper = g_ascii_strtod (pstring, &pstring);
                step_increment = g_ascii_strtod (pstring, &pstring);
                page_increment = g_ascii_strtod (pstring, &pstring);
                page_size = g_ascii_strtod (pstring, &pstring);

		object = G_OBJECT (gtk_adjustment_new (value, lower, upper, step_increment, page_increment, page_size));
	}
	else
	{
		GladeWidget *gwidget;
		if ((gwidget = glade_project_get_widget_by_name 
			       (project, string)) != NULL)
			object = gwidget->object;
	}
	
	return object;
}

static GList *
glade_property_class_make_objects_from_string (GladePropertyClass *property_class,
					       const gchar        *string,
					       GladeProject       *project)
{
	GList    *objects = NULL;
	GObject  *object;
	gchar   **split;
	guint     i;
	
	if ((split = g_strsplit (string, GPC_OBJECT_DELIMITER, 0)) != NULL)
	{
		for (i = 0; split[i]; i++)
		{
			if ((object = glade_property_class_make_object_from_string
			     (property_class, split[i], project)) != NULL)
				objects = g_list_prepend (objects, object);
		}
		g_strfreev (split);
	}
	return objects;
}

/**
 * glade_property_class_make_gvalue_from_string:
 * @property_class: A #GladePropertyClass
 * @string: a string representation of this property
 * @project: the glade project that the associated property
 *           belongs to.
 *
 * Returns: A #GValue created based on the @property_class
 *          and @string criteria.
 */
GValue *
glade_property_class_make_gvalue_from_string (GladePropertyClass *property_class,
					      const gchar        *string,
					      GladeProject       *project)
{
	GValue    *value = g_new0 (GValue, 1);
	gchar    **strv, *fullpath;
	GdkColor   color = { 0, };

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
	else if (G_IS_PARAM_SPEC_VALUE_ARRAY (property_class->pspec))
	{
		GValueArray *value_array = g_value_array_new (0);
		GValue str_value = {0, };
		gint i;
			
		g_value_init (&str_value, G_TYPE_STRING);
		strv   = g_strsplit (string, "\n", 0);
						
		for (i = 0; strv[i]; i++)
		{
			g_value_set_static_string (&str_value, strv[i]);
			value_array = g_value_array_append (value_array, &str_value);
		}
		g_value_set_boxed (value, value_array);
		g_strfreev (strv);
	}
	else if (G_IS_PARAM_SPEC_BOXED(property_class->pspec))
	{
		if (property_class->pspec->value_type == GDK_TYPE_COLOR)
		{
			if (gdk_color_parse(string, &color) &&
			    gdk_colormap_alloc_color(gtk_widget_get_default_colormap(),
						     &color, FALSE, TRUE))
				g_value_set_boxed(value, &color);
			else
				g_warning ("could not parse colour name `%s'", string);
		}
		else if (property_class->pspec->value_type == G_TYPE_STRV)
		{
			strv   = g_strsplit (string, "\n", 0);
			g_value_take_boxed (value, strv);
		}
	}
	else if (G_IS_PARAM_SPEC_INT(property_class->pspec))
		g_value_set_int (value, g_ascii_strtoll (string, NULL, 10));
	else if (G_IS_PARAM_SPEC_UINT(property_class->pspec))
		g_value_set_uint (value, g_ascii_strtoull (string, NULL, 10));
	else if (G_IS_PARAM_SPEC_LONG(property_class->pspec))
		g_value_set_long (value, g_ascii_strtoll (string, NULL, 10));
	else if (G_IS_PARAM_SPEC_ULONG(property_class->pspec))
		g_value_set_ulong (value, g_ascii_strtoull (string, NULL, 10));
	else if (G_IS_PARAM_SPEC_INT64(property_class->pspec))
		g_value_set_int64 (value, g_ascii_strtoll (string, NULL, 10));
	else if (G_IS_PARAM_SPEC_UINT64(property_class->pspec))
		g_value_set_uint64 (value, g_ascii_strtoull (string, NULL, 10));
	else if (G_IS_PARAM_SPEC_FLOAT(property_class->pspec))
		g_value_set_float (value, (float) g_ascii_strtod (string, NULL));
	else if (G_IS_PARAM_SPEC_DOUBLE(property_class->pspec))
		g_value_set_double (value, g_ascii_strtod (string, NULL));
	else if (G_IS_PARAM_SPEC_STRING(property_class->pspec))
	{
		/* This can be called when loading defaults from
		 * catalog files... it wont happen and we cant do
		 * anything for it.
		 */
		if (property_class->resource && project)
		{
			fullpath = g_build_filename
				(glade_project_get_path (project), string, NULL);
			g_value_set_string (value, fullpath);
			g_free (fullpath);
		}
		else g_value_set_string (value, string);
	}
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
	else if (G_IS_PARAM_SPEC_OBJECT(property_class->pspec))
	{
		GObject *object = glade_property_class_make_object_from_string
			(property_class, string, project);
		g_value_set_object (value, object);
	}
	else if (GLADE_IS_PARAM_SPEC_OBJECTS (property_class->pspec))
	{
		GList *objects = glade_property_class_make_objects_from_string
			(property_class, string, project);
		g_value_set_boxed (value, objects);
	}
	else
		g_critical ("Unsupported pspec type %s",
			    g_type_name(G_PARAM_SPEC_TYPE (property_class->pspec)));

	return value;
}

/**
 * glade_property_class_make_gvalue_from_vl:
 * @property_class: A #GladePropertyClass
 * @vl: a #va_list holding one argument of the correct type
 *      specified by @property_class
 *
 * Returns: A #GValue created based on the @property_class
 *          and a @vl arg of the correct type.
 */
GValue *
glade_property_class_make_gvalue_from_vl (GladePropertyClass  *klass,
					  va_list              vl)
{
	GValue   *value;

	g_return_val_if_fail (klass != NULL, NULL);

	value = g_new0 (GValue, 1);
	g_value_init (value, klass->pspec->value_type);
	
	if (G_IS_PARAM_SPEC_ENUM(klass->pspec))
		g_value_set_enum (value, va_arg (vl, gint));
	else if (G_IS_PARAM_SPEC_FLAGS(klass->pspec))
		g_value_set_flags (value, va_arg (vl, gint));
	else if (G_IS_PARAM_SPEC_INT(klass->pspec))
		g_value_set_int (value, va_arg (vl, gint));
	else if (G_IS_PARAM_SPEC_UINT(klass->pspec))
		g_value_set_uint (value, va_arg (vl, guint));
	else if (G_IS_PARAM_SPEC_LONG(klass->pspec))
		g_value_set_long (value, va_arg (vl, glong));
	else if (G_IS_PARAM_SPEC_ULONG(klass->pspec))
		g_value_set_ulong (value, va_arg (vl, gulong));
	else if (G_IS_PARAM_SPEC_INT64(klass->pspec))
		g_value_set_int64 (value, va_arg (vl, gint64));
	else if (G_IS_PARAM_SPEC_UINT64(klass->pspec))
		g_value_set_uint64 (value, va_arg (vl, guint64));
	else if (G_IS_PARAM_SPEC_FLOAT(klass->pspec))
		g_value_set_float (value, (gfloat)va_arg (vl, gdouble));
	else if (G_IS_PARAM_SPEC_DOUBLE(klass->pspec))
		g_value_set_double (value, va_arg (vl, gdouble));
	else if (G_IS_PARAM_SPEC_STRING(klass->pspec))
		g_value_set_string (value, va_arg (vl, gchar *));
	else if (G_IS_PARAM_SPEC_CHAR(klass->pspec))
		g_value_set_char (value, (gchar)va_arg (vl, gint));
	else if (G_IS_PARAM_SPEC_UCHAR(klass->pspec))
		g_value_set_uchar (value, (guchar)va_arg (vl, guint));
	else if (G_IS_PARAM_SPEC_UNICHAR(klass->pspec))
		g_value_set_uint (value, va_arg (vl, gunichar));
	else if (G_IS_PARAM_SPEC_BOOLEAN(klass->pspec))
		g_value_set_boolean (value, va_arg (vl, gboolean));
	else if (G_IS_PARAM_SPEC_OBJECT(klass->pspec))
		g_value_set_object (value, va_arg (vl, gpointer));
	else if (G_IS_PARAM_SPEC_BOXED(klass->pspec))
		g_value_set_boxed (value, va_arg (vl, gpointer));
	else if (GLADE_IS_PARAM_SPEC_OBJECTS(klass->pspec))
		g_value_set_boxed (value, va_arg (vl, gpointer));
	else
		g_critical ("Unsupported pspec type %s",
			    g_type_name(G_PARAM_SPEC_TYPE (klass->pspec)));
	
	return value;
}

/**
 * glade_property_class_make_gvalue:
 * @klass: A #GladePropertyClass
 * @...: an argument of the correct type specified by @property_class
 *
 * Returns: A #GValue created based on the @property_class
 *          and the provided argument.
 */
GValue *
glade_property_class_make_gvalue (GladePropertyClass  *klass,
				  ...)
{
	GValue  *value;
	va_list  vl;

	g_return_val_if_fail (klass != NULL, NULL);

	va_start (vl, klass);
	value = glade_property_class_make_gvalue_from_vl (klass, vl);
	va_end (vl);

	return value;
}


/**
 * glade_property_class_set_vl_from_gvalue:
 * @klass: A #GladePropertyClass
 * @value: A #GValue to set
 * @vl: a #va_list holding one argument of the correct type
 *      specified by @klass
 * 
 *
 * Sets @vl from @value based on @klass criteria.
 */
void
glade_property_class_set_vl_from_gvalue (GladePropertyClass  *klass,
					 GValue              *value,
					 va_list              vl)
{
	g_return_if_fail (klass != NULL);
	g_return_if_fail (value != NULL);

	/* The argument is a pointer of the specified type, cast the pointer and assign
	 * the value using the proper g_value_get_ variation.
	 */
	if (G_IS_PARAM_SPEC_ENUM(klass->pspec))
		*(gint *)(va_arg (vl, gint *)) = g_value_get_enum (value);
	else if (G_IS_PARAM_SPEC_FLAGS(klass->pspec))
		*(gint *)(va_arg (vl, gint *)) = g_value_get_flags (value);
	else if (G_IS_PARAM_SPEC_INT(klass->pspec))
		*(gint *)(va_arg (vl, gint *)) = g_value_get_int (value);
	else if (G_IS_PARAM_SPEC_UINT(klass->pspec))
		*(guint *)(va_arg (vl, guint *)) = g_value_get_uint (value);
	else if (G_IS_PARAM_SPEC_LONG(klass->pspec))
		*(glong *)(va_arg (vl, glong *)) = g_value_get_long (value);
	else if (G_IS_PARAM_SPEC_ULONG(klass->pspec))
		*(gulong *)(va_arg (vl, gulong *)) = g_value_get_ulong (value);
	else if (G_IS_PARAM_SPEC_INT64(klass->pspec))
		*(gint64 *)(va_arg (vl, gint64 *)) = g_value_get_int64 (value);
	else if (G_IS_PARAM_SPEC_UINT64(klass->pspec))
		*(guint64 *)(va_arg (vl, guint64 *)) = g_value_get_uint64 (value);
	else if (G_IS_PARAM_SPEC_FLOAT(klass->pspec))
		*(gfloat *)(va_arg (vl, gdouble *)) = g_value_get_float (value);
	else if (G_IS_PARAM_SPEC_DOUBLE(klass->pspec))
		*(gdouble *)(va_arg (vl, gdouble *)) = g_value_get_double (value);
	else if (G_IS_PARAM_SPEC_STRING(klass->pspec))
		*(gchar **)(va_arg (vl, gchar *)) = (gchar *)g_value_get_string (value);
	else if (G_IS_PARAM_SPEC_CHAR(klass->pspec))
		*(gchar *)(va_arg (vl, gint *)) = g_value_get_char (value);
	else if (G_IS_PARAM_SPEC_UCHAR(klass->pspec))
		*(guchar *)(va_arg (vl, guint *)) = g_value_get_uchar (value);
	else if (G_IS_PARAM_SPEC_UNICHAR(klass->pspec))
		*(guint *)(va_arg (vl, gunichar *)) = g_value_get_uint (value);
	else if (G_IS_PARAM_SPEC_BOOLEAN(klass->pspec))
		*(gboolean *)(va_arg (vl, gboolean *)) = g_value_get_boolean (value);
	else if (G_IS_PARAM_SPEC_OBJECT(klass->pspec))
		*(gpointer *)(va_arg (vl, gpointer *)) = g_value_get_object (value);
	else if (G_IS_PARAM_SPEC_BOXED(klass->pspec))
		*(gpointer *)(va_arg (vl, gpointer *)) = g_value_get_boxed (value);
	else if (GLADE_IS_PARAM_SPEC_OBJECTS(klass->pspec))
		*(gpointer *)(va_arg (vl, gpointer *)) = g_value_get_boxed (value);
	else
		g_critical ("Unsupported pspec type %s",
			    g_type_name(G_PARAM_SPEC_TYPE (klass->pspec)));
}

/**
 * glade_property_class_get_from_gvalue:
 * @klass: A #GladePropertyClass
 * @value: A #GValue to set
 * @...: a return location of the correct type
 * 
 *
 * Assignes the provided return location to @value
 */
void
glade_property_class_get_from_gvalue (GladePropertyClass  *klass,
				      GValue              *value,
				      ...)
{
	va_list  vl;

	g_return_if_fail (klass != NULL);

	va_start (vl, value);
	glade_property_class_set_vl_from_gvalue (klass, value, vl);
	va_end (vl);
}

/**
 * glade_property_class_list_atk_relations:
 * @handle: A generic pointer (i.e. a #GladeWidgetClass)
 * @owner_type: The #GType of the owning widget class.
 *
 * Returns: a #GList of newly created atk relation #GladePropertyClass.
 */
GList *
glade_property_class_list_atk_relations (gpointer handle,
					 GType    owner_type)
{
	const GPCAtkPropertyTab *relation_tab = NULL;
	GladePropertyClass      *property_class;
	GList                   *list = NULL;
	gint                     i;
	
	/* Loop through our hard-coded table enties */
	for (i = 0; i < G_N_ELEMENTS (relation_names_table); i++)
	{
		relation_tab = &relation_names_table[i];

		property_class                    = glade_property_class_new (handle);
		property_class->pspec             = 
			glade_param_spec_objects (relation_tab->id,
						  _(relation_tab->name),
						  _(relation_tab->tooltip),
						  ATK_TYPE_IMPLEMENTOR,
						  G_PARAM_READWRITE);
		
		property_class->pspec->owner_type = owner_type;
		property_class->id                = g_strdup (relation_tab->id);
		property_class->name              = g_strdup (_(relation_tab->name));
		property_class->tooltip           = g_strdup (_(relation_tab->tooltip));
		property_class->type              = GPC_ATK_RELATION;
		property_class->visible_lines     = 2;
		property_class->ignore            = TRUE;

		property_class->def = 
			glade_property_class_make_gvalue_from_string
			(property_class, "", NULL);
		
		property_class->orig_def = 
			glade_property_class_make_gvalue_from_string
			(property_class, "", NULL);

		list = g_list_prepend (list, property_class);
	}

	return g_list_reverse (list);
}

/**
 * glade_property_class_accel_property:
 * @handle: A generic pointer (i.e. a #GladeWidgetClass)
 * @owner_type: The #GType of the owning widget class.
 *
 * Returns: a newly created #GladePropertyClass for accelerators
 *          of the prescribed @owner_type.
 */
GladePropertyClass *
glade_property_class_accel_property (gpointer handle,
				     GType    owner_type)
{
	GladePropertyClass *property_class;
	GValue             *def_value;

	property_class                    = glade_property_class_new (handle);
	property_class->pspec             = 
		glade_param_spec_accel ("accelerators", _("Accelerators"),
					_("A list of accelerator keys"), 
					owner_type,
					G_PARAM_READWRITE);

	
	property_class->pspec->owner_type = owner_type;
	property_class->id                = g_strdup (g_param_spec_get_name
						      (property_class->pspec));
	property_class->name              = g_strdup (g_param_spec_get_nick
						      (property_class->pspec));
	property_class->tooltip           = g_strdup (g_param_spec_get_blurb
						      (property_class->pspec));

	property_class->type              = GPC_ACCEL_PROPERTY;
	property_class->ignore            = TRUE;
	property_class->common            = TRUE;

	/* Setup default */
	def_value = g_new0 (GValue, 1);
	g_value_init (def_value, GLADE_TYPE_ACCEL_GLIST);
	g_value_set_boxed (def_value, NULL);
	property_class->def = def_value;

	/* Setup original default */
	def_value = g_new0 (GValue, 1);
	g_value_init (def_value, GLADE_TYPE_ACCEL_GLIST);
	g_value_set_boxed (def_value, NULL);
	property_class->orig_def = def_value;

	return property_class;
}


/**
 * glade_property_class_new_from_spec:
 * @handle: A generic pointer (i.e. a #GladeWidgetClass)
 * @spec: A #GParamSpec
 *
 * Returns: a newly created #GladePropertyClass based on @spec
 *          or %NULL if its unsupported.
 */
GladePropertyClass *
glade_property_class_new_from_spec (gpointer     handle,
				    GParamSpec  *spec)
{
	GObjectClass       *gtk_widget_class;
	GladePropertyClass *property_class;

	g_return_val_if_fail (spec != NULL, NULL);
	gtk_widget_class = g_type_class_ref (GTK_TYPE_WIDGET);
	
	/* Only properties that are _new_from_spec() are 
	 * not virtual properties
	 */
	property_class          = glade_property_class_new (handle);
	property_class->virt    = FALSE;
	property_class->pspec   = spec;

	/* We only use the writable properties */
	if ((spec->flags & G_PARAM_WRITABLE) == 0)
		goto failed;

	/* Register only editable properties.
	 */
	if (!glade_editor_property_supported (property_class->pspec))
		goto failed;
	
	property_class->id   = g_strdup (spec->name);
	property_class->name = g_strdup (g_param_spec_get_nick (spec));


	/* If its on the GtkWidgetClass, it goes in "common" 
	 * (unless stipulated otherwise in the xml file)
	 */
	if (g_object_class_find_property (gtk_widget_class, 
					  g_param_spec_get_name (spec)) != NULL)
		property_class->common = TRUE;

	/* Flag the construct only properties */
	if (spec->flags & G_PARAM_CONSTRUCT_ONLY)
		property_class->construct_only = TRUE;

	if (g_type_is_a (spec->owner_type, ATK_TYPE_OBJECT))
	{
		property_class->type    = GPC_ATK_PROPERTY;
		property_class->ignore  = TRUE;

		/* We only use the name and desctription props,
		 * they are both translatable.
		 */
		property_class->translatable = TRUE;
	}

	if (!property_class->id || !property_class->name)
	{
		g_critical ("No name or id for "
			    "glade_property_class_new_from_spec, failed.");
		goto failed;
	}

	property_class->tooltip  = g_strdup (g_param_spec_get_blurb (spec));
	property_class->orig_def = glade_property_class_get_default_from_spec (spec);
	property_class->def      = glade_property_class_get_default_from_spec (spec);

	g_type_class_unref (gtk_widget_class);
	return property_class;

  failed:
	glade_property_class_free (property_class);
	g_type_class_unref (gtk_widget_class);
	return NULL;
}

/**
 * glade_property_class_is_visible:
 * @property_class: A #GladePropertyClass
 *
 *
 * Returns: whether or not to show this property in the editor
 */
gboolean
glade_property_class_is_visible (GladePropertyClass *klass)
{
	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (klass), FALSE);
	return klass->visible;
}

/**
 * glade_property_class_is_object:
 * @property_class: A #GladePropertyClass
 *
 *
 * Returns: whether or not this is an object property 
 * that refers to another object in this project.
 */
gboolean
glade_property_class_is_object (GladePropertyClass  *klass)
{
	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (klass), FALSE);

 	return (GLADE_IS_PARAM_SPEC_OBJECTS (klass->pspec) ||
		(G_IS_PARAM_SPEC_OBJECT(klass->pspec) &&
		 klass->pspec->value_type != GDK_TYPE_PIXBUF &&
		 klass->pspec->value_type != GTK_TYPE_ADJUSTMENT));
}


/**
 * glade_property_class_get_displayable_value:
 * @klass: the property class to search in
 * @value: the value to search
 *
 * Search a displayable values for @value in this property class.
 *
 * Returns: a (gchar *) if a diplayable value was found, otherwise NULL.
 */
const gchar *
glade_property_class_get_displayable_value(GladePropertyClass *klass, gint value)
{
	gint i, len;
	GArray *disp_val = klass->displayable_values;

	if (disp_val == NULL) return NULL;
	
	len = disp_val->len;
	
	for (i = 0; i < len; i++)
		if (g_array_index (disp_val, GEnumValue, i).value == value)
			return g_array_index (disp_val, GEnumValue, i).value_name;

	return NULL;
}

/**
 * gpc_get_displayable_values_from_node:
 * @node: a GLADE_TAG_DISPLAYABLE_VALUES node
 * @values: an array of the values wich node overrides.
 * @n_values: the size of @values
 *
 * Returns: a (GArray *) of GEnumValue of the overridden fields.
 */
static GArray *
gpc_get_displayable_values_from_node (GladeXmlNode *node, 
				      GEnumValue   *values, 
				      gint          n_values,
				      const gchar  *domain)
{
	GArray *array;
	GladeXmlNode *child;
	
	if ((child = glade_xml_search_child (node, GLADE_TAG_VALUE)) == NULL)
		return NULL;
	
	array = g_array_new (FALSE, TRUE, sizeof(GEnumValue));

	child = glade_xml_node_get_children (node);
	while (child != NULL)
	{
		gint i;
		gchar *id, *name, *nick;
		GEnumValue val;
		
		id = glade_xml_get_property_string_required (child, GLADE_TAG_ID, NULL);
		name = glade_xml_get_property_string (child, GLADE_TAG_NAME);
		nick = glade_xml_get_property_string (child, GLADE_TAG_NICK);

		for(i=0; i < n_values; i++)
		{
			if(strcmp (id, values[i].value_name) == 0)
			{
				val=values[i];
				
				/* Tedious memory handleing; if gettext doesn't return
				 * a translation, dont free the untranslated string.
				 */
				if(name) 
				{
					val.value_name = dgettext (domain, name);
					if (name != val.value_name)
						g_free (name);
				}
				if(nick)
				{
					val.value_nick = dgettext (domain, nick);
					if (nick != val.value_nick)
						g_free (nick);
				}

				g_array_append_val(array, val);
				break;
			}
		}
		g_free(id);
		
		child = glade_xml_node_next (child);
	}
	
	return array;
}

/**
 * glade_property_class_make_adjustment:
 * @property_class: a pointer to the property class
 *
 * Creates and appropriate GtkAdjustment for use in the editor
 *
 * Returns: An appropriate #GtkAdjustment for use in the Property editor
 */
GtkAdjustment *
glade_property_class_make_adjustment (GladePropertyClass *property_class)
{
	gdouble        min = 0, max = 0, def = 0;
	gboolean       float_range = FALSE;

	g_return_val_if_fail (property_class        != NULL, NULL);
	g_return_val_if_fail (property_class->pspec != NULL, NULL);

	if (G_IS_PARAM_SPEC_INT(property_class->pspec))
	{
		min = (gdouble)((GParamSpecInt *) property_class->pspec)->minimum;
		max = (gdouble)((GParamSpecInt *) property_class->pspec)->maximum;
		def = (gdouble)((GParamSpecInt *) property_class->pspec)->default_value;
	} else if (G_IS_PARAM_SPEC_UINT(property_class->pspec))
	{
		min = (gdouble)((GParamSpecUInt *) property_class->pspec)->minimum;
		max = (gdouble)((GParamSpecUInt *) property_class->pspec)->maximum;
		def = (gdouble)((GParamSpecUInt *) property_class->pspec)->default_value;
	} else if (G_IS_PARAM_SPEC_LONG(property_class->pspec))
	{
		min = (gdouble)((GParamSpecLong *) property_class->pspec)->minimum;
		max = (gdouble)((GParamSpecLong *) property_class->pspec)->maximum;
		def = (gdouble)((GParamSpecLong *) property_class->pspec)->default_value;
	} else if (G_IS_PARAM_SPEC_ULONG(property_class->pspec))
	{
		min = (gdouble)((GParamSpecULong *) property_class->pspec)->minimum;
		max = (gdouble)((GParamSpecULong *) property_class->pspec)->maximum;
		def = (gdouble)((GParamSpecULong *) property_class->pspec)->default_value;
	} else if (G_IS_PARAM_SPEC_INT64(property_class->pspec))
	{
		min = (gdouble)((GParamSpecInt64 *) property_class->pspec)->minimum;
		max = (gdouble)((GParamSpecInt64 *) property_class->pspec)->maximum;
		def = (gdouble)((GParamSpecInt64 *) property_class->pspec)->default_value;
	} else if (G_IS_PARAM_SPEC_UINT64(property_class->pspec))
	{
		min = (gdouble)((GParamSpecUInt64 *) property_class->pspec)->minimum;
		max = (gdouble)((GParamSpecUInt64 *) property_class->pspec)->maximum;
		def = (gdouble)((GParamSpecUInt64 *) property_class->pspec)->default_value;
	} else if (G_IS_PARAM_SPEC_FLOAT(property_class->pspec))
	{
		float_range = TRUE;
		min = (gdouble)((GParamSpecFloat *) property_class->pspec)->minimum;
		max = (gdouble)((GParamSpecFloat *) property_class->pspec)->maximum;
		def = (gdouble)((GParamSpecFloat *) property_class->pspec)->default_value;
	} else if (G_IS_PARAM_SPEC_DOUBLE(property_class->pspec))
	{
		float_range = TRUE;
		min = (gdouble)((GParamSpecDouble *) property_class->pspec)->minimum;
		max = (gdouble)((GParamSpecDouble *) property_class->pspec)->maximum;
		def = (gdouble)((GParamSpecDouble *) property_class->pspec)->default_value;
	} else
	{
		g_critical ("Can't make adjustment for pspec type %s",
			    g_type_name(G_PARAM_SPEC_TYPE (property_class->pspec)));
	}

	return (GtkAdjustment *)gtk_adjustment_new (def, min, max,
						    float_range ?
						    FLOATING_STEP_INCREMENT :
						    NUMERICAL_STEP_INCREMENT,
						    NUMERICAL_PAGE_INCREMENT,
						    NUMERICAL_PAGE_SIZE);
}

/**
 * glade_property_class_update_from_node:
 * @node: the property node
 * @module: a #GModule to lookup symbols from the plugin
 * @object_type: the #GType of the owning object
 * @property_class: a pointer to the property class
 * @domain: the domain to translate catalog strings from
 *
 * Updates the @property_class with the contents of the node in the xml
 * file. Only the values found in the xml file are overridden.
 *
 * Returns: %TRUE on success. @property_class is set to NULL if the property
 *          has Disabled="TRUE".
 */
gboolean
glade_property_class_update_from_node (GladeXmlNode        *node,
				       GModule             *module,
				       GType                object_type,
				       GladePropertyClass **property_class,
				       const gchar         *domain)
{
	GladePropertyClass *klass;
	gchar *buff;
	GladeXmlNode *child;

	g_return_val_if_fail (property_class != NULL, FALSE);

	/* for code cleanliness... */
	klass = *property_class;

	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (klass), FALSE);
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
		glade_property_class_free (klass);
		*property_class = NULL;
		return TRUE;
	}

	/* ...the spec... */
	buff = glade_xml_get_value_string (node, GLADE_TAG_SPEC);
	if (buff)
	{
		/* ... get the tooltip from the pspec ... */
		if ((klass->pspec = glade_utils_get_pspec_from_funcname (buff)) != NULL)
		{
			/* Make sure we can tell properties apart by there 
			 * owning class.
			 */
			klass->pspec->owner_type = object_type;

			if (klass->tooltip) g_free (klass->tooltip);
			if (klass->name)    g_free (klass->name);
			
			klass->tooltip = g_strdup (g_param_spec_get_blurb (klass->pspec));
			klass->name    = g_strdup (g_param_spec_get_nick (klass->pspec));
			
			if (klass->pspec->flags & G_PARAM_CONSTRUCT_ONLY)
				klass->construct_only = TRUE;

			if (klass->def) {
				g_value_unset (klass->def);
				g_free (klass->def);
			}
			klass->def = glade_property_class_get_default_from_spec (klass->pspec);
			
			if (klass->orig_def == NULL)
				klass->orig_def =
					glade_property_class_get_default_from_spec (klass->pspec);

		}

 		g_free (buff);
	}
	else if (!klass->pspec) 
	{
		/* If catalog file didn't specify a pspec function
		 * and this property isn't found by introspection
		 * we simply handle it as a property that has been
		 * disabled.
		 */
		glade_property_class_free (klass);
		*property_class = NULL;
		return TRUE;
	}

	/* Get the default */
	if ((buff = glade_xml_get_property_string (node, GLADE_TAG_DEFAULT)) != NULL)
	{
		if (klass->def) {
			g_value_unset (klass->def);
			g_free (klass->def);
		}
		klass->def = glade_property_class_make_gvalue_from_string (klass, buff, NULL);
		g_free (buff);
	}

	/* If needed, update the name... */
	if ((buff = glade_xml_get_property_string (node, GLADE_TAG_NAME)) != NULL)
	{
		g_free (klass->name);
		klass->name = g_strdup (dgettext (domain, buff));
	}
	
	/* ...and the tooltip */
	if ((buff = glade_xml_get_value_string (node, GLADE_TAG_TOOLTIP)) != NULL)
	{
		g_free (klass->tooltip);
		klass->tooltip = g_strdup (dgettext (domain, buff));
	}

	/* If this property's value is an enumeration then we try to get the displayable values */
	if (G_IS_PARAM_SPEC_ENUM(klass->pspec))
	{
		GEnumClass  *eclass = g_type_class_ref(klass->pspec->value_type);
		
		child = glade_xml_search_child (node, GLADE_TAG_DISPLAYABLE_VALUES);
		if (child)
			klass->displayable_values = gpc_get_displayable_values_from_node
				(child, eclass->values, eclass->n_values, domain);
		
		g_type_class_unref(eclass);
	}
	
	/* the same way if it is a Flags property */
	if (G_IS_PARAM_SPEC_FLAGS(klass->pspec))
	{
		GFlagsClass  *fclass = g_type_class_ref(klass->pspec->value_type);
		
		child = glade_xml_search_child (node, GLADE_TAG_DISPLAYABLE_VALUES);
		if (child)
			klass->displayable_values = gpc_get_displayable_values_from_node
				(child, (GEnumValue*)fclass->values, fclass->n_values, domain);
		
		g_type_class_unref(fclass);
	}

	/* Visible lines */
	glade_xml_get_value_int (node, GLADE_TAG_VISIBLE_LINES,  &klass->visible_lines);

	/* Get the Parameters */
	if ((child = glade_xml_search_child (node, GLADE_TAG_PARAMETERS)) != NULL)
		klass->parameters = glade_parameter_list_new_from_node (klass->parameters, child);
		
	/* Whether or not the property is translatable. This is only used for
	 * string properties.
	 */
	klass->translatable = glade_xml_get_property_boolean (node, GLADE_TAG_TRANSLATABLE, 
							      klass->translatable);

	/* common, optional, etc */
	klass->common   = glade_xml_get_property_boolean (node, GLADE_TAG_COMMON,   klass->common);
	klass->optional = glade_xml_get_property_boolean (node, GLADE_TAG_OPTIONAL, klass->optional);
	klass->query    = glade_xml_get_property_boolean (node, GLADE_TAG_QUERY,    klass->query);
	klass->save     = glade_xml_get_property_boolean (node, GLADE_TAG_SAVE,     klass->save);
	klass->visible  = glade_xml_get_property_boolean (node, GLADE_TAG_VISIBLE,  klass->visible);
	klass->ignore   = glade_xml_get_property_boolean (node, GLADE_TAG_IGNORE,   klass->ignore);
	klass->resource = glade_xml_get_property_boolean (node, GLADE_TAG_RESOURCE, klass->resource);
	klass->weight   = glade_xml_get_property_double  (node, GLADE_TAG_WEIGHT,   klass->weight);
	klass->transfer_on_paste = glade_xml_get_property_boolean (node, GLADE_TAG_TRANSFER_ON_PASTE, klass->transfer_on_paste);
	klass->save_always = glade_xml_get_property_boolean (node, GLADE_TAG_SAVE_ALWAYS, klass->save_always);
	
	/* A sprinkle of hard-code to get atk properties working right
	 */
	if (glade_xml_get_property_boolean (node, GLADE_TAG_ATK_ACTION, FALSE))
		klass->type = GPC_ATK_ACTION;
	else if (glade_xml_get_property_boolean (node, GLADE_TAG_ATK_PROPERTY, FALSE))
	{
		if (GLADE_IS_PARAM_SPEC_OBJECTS (klass->pspec))
			klass->type = GPC_ATK_RELATION;
		else
			klass->type = GPC_ATK_PROPERTY;
	}

	/* Special case accelerators here.
	 */
	if (GLADE_IS_PARAM_SPEC_ACCEL (klass->pspec))
		klass->type = GPC_ACCEL_PROPERTY;

	/* Special case pixbuf here.
	 */
	if (klass->pspec->value_type == GDK_TYPE_PIXBUF)
		klass->resource = TRUE;
	
	if (klass->optional)
		klass->optional_default =
			glade_xml_get_property_boolean (node, GLADE_TAG_OPTIONAL_DEFAULT, 
							klass->optional_default);

	/* notify that we changed the property class */
	klass->is_modified = TRUE;

	return TRUE;
}



/**
 * glade_property_class_match:
 * @klass: a #GladePropertyClass
 * @comp: a #GladePropertyClass
 *
 * Returns: whether @klass and @comp are a match or not
 *          (properties in seperate decendant heirarchies that
 *           have the same name are not matches).
 */
gboolean
glade_property_class_match (GladePropertyClass *klass,
			    GladePropertyClass *comp)
{
	g_return_val_if_fail (klass != NULL, FALSE);
	g_return_val_if_fail (comp != NULL, FALSE);

	return (strcmp (klass->id, comp->id) == 0 &&
		klass->packing           == comp->packing &&
		klass->pspec->owner_type == comp->pspec->owner_type);
}


/**
 * glade_property_class_void_value:
 * @klass: a #GladePropertyClass
 *
 * Returns: Whether @value for this @klass is voided; a voided value
 *          can be a %NULL value for boxed or object type param specs.
 */
gboolean
glade_property_class_void_value (GladePropertyClass *klass,
				 GValue             *value)
{
	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (klass), FALSE);
	
	if (G_IS_PARAM_SPEC_OBJECT (klass->pspec) &&
	    g_value_get_object (value) == NULL)
		return TRUE;
	else if (G_IS_PARAM_SPEC_BOXED (klass->pspec) &&
		 g_value_get_boxed (value) == NULL)
		return TRUE;

	return FALSE;
}
