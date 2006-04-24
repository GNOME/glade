/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-clipboard.c - An object for handling Cut/Copy/Paste.
 *
 * Copyright (C) 2005 The GNOME Foundation.
 *
 * Author(s):
 *      Tristan Van Berkom <tvb@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include "glade-builtins.h"



struct _GladeParamSpecObjects {
	GParamSpec    parent_instance;
	
	GType         type;
};


/************************************************************
 *      Auto-generate the enum type for stock properties    *
 ************************************************************/
GType
glade_standard_stock_get_type (void)
{
	static GType etype = 0;
	if (etype == 0) {

		GtkStockItem  item;
		GSList       *l, *stock_list;
		gchar        *stock_id;
		gint          stock_enum = 0;
		GEnumValue    value;
		GArray       *values = 
			g_array_new (TRUE, TRUE, sizeof (GEnumValue));

		/* We have ownership of the retuened list & the
		 * strings within
		 */
		stock_list = gtk_stock_list_ids ();

		/* Add first "no stock" element */
		value.value_nick = g_strdup ("glade-none"); // Passing ownership here.
		value.value_name = g_strdup ("None");
		value.value      = stock_enum++;
		values = g_array_append_val (values, value);

		for (l = stock_list; l; l = l->next)
		{
			stock_id = l->data;
			if (!gtk_stock_lookup (stock_id, &item))
				continue;

			value.value      = stock_enum++;
			value.value_name = g_strdup (item.label);
			value.value_nick = stock_id; // Passing ownership here.
			values = g_array_append_val (values, value);
		}

		etype = g_enum_register_static ("GladeStock", (GEnumValue *)values->data);

		g_slist_free (stock_list);
	}
	return etype;
}

GParamSpec *
glade_standard_stock_spec (void)
{
	return g_param_spec_enum ("stock", _("Stock"), 
				  _("A builtin stock item"),
				  glade_standard_stock_get_type (),
				  0, G_PARAM_READWRITE);
}


/****************************************************************
 *  A GList boxed type used by GladeParamSpecObjects and        *
 *  GladeParamSpecAccel                                         *
 ****************************************************************/
GType
glade_glist_get_type (void)
{
	static GType type_id = 0;

	if (!type_id)
		type_id = g_boxed_type_register_static
			("GladeGList", 
			 (GBoxedCopyFunc) g_list_copy,
			 (GBoxedFreeFunc) g_list_free);
	return type_id;
}



/****************************************************************
 *  Built-in GladeParamSpecAccel for accelerator properties     *
 ****************************************************************/
#if 0
static void
param_accel_init (GParamSpec *pspec)
{
	GladeParamSpecAccel *ospec = GLADE_PARAM_SPEC_ACCEL (pspec);
	ospec->type = G_TYPE_OBJECT;
}

static void
param_accel_set_default (GParamSpec *pspec,
			   GValue     *value)
{
	if (value->data[0].v_pointer != NULL)
	{
		g_free (value->data[0].v_pointer);
	}
	value->data[0].v_pointer = NULL;
}

static gboolean
param_accel_validate (GParamSpec *pspec,
		      GValue     *value)
{
	GladeParamSpecAccel *ospec = GLADE_PARAM_SPEC_ACCEL (pspec);
	GList               *accels, *list, *toremove = NULL;
	GladeAccelInfo      *info;

	accel = value->data[0].v_pointer;

	for (list = accels; list; list = list->next)
	{
		info = list->data;
		
		/* Is it a valid key ? */
		if (info->key /* XXX */ ||
		    /* Does the modifier contain any unwanted bits ? */
		    info->modifier & GLADE_MODIFIER_MASK ||
		    /* Do we have a signal ? */
		    info->signal == NULL)
			toremove = g_list_prepend (toremove, info);
	}

	for (list = toremove; list; list = list->next)
	{
		object = list->data;
		accels = g_list_remove (accels, object);
	}
	if (toremove) g_list_free (toremove);
 
	value->data[0].v_pointer = accels;

	return toremove != NULL;
}

static gint
param_accel_values_cmp (GParamSpec   *pspec,
			  const GValue *value1,
			  const GValue *value2)
{
  guint8 *p1 = value1->data[0].v_pointer;
  guint8 *p2 = value2->data[0].v_pointer;

  /* not much to compare here, try to at least provide stable lesser/greater result */

  return p1 < p2 ? -1 : p1 > p2;
}

GType
glade_param_accel_get_type (void)
{
	static GType accel_type = 0;

	if (accel_type == 0)
	{
		static /* const */ GParamSpecTypeInfo pspec_info = {
			sizeof (GladeParamSpecAccel),  /* instance_size */
			16,                         /* n_preallocs */
			param_accel_init,         /* instance_init */
			0xdeadbeef,                 /* value_type, assigned further down */
			NULL,                       /* finalize */
			param_accel_set_default,  /* value_set_default */
			param_accel_validate,     /* value_validate */
			param_accel_values_cmp,   /* values_cmp */
		};
		pspec_info.value_type = GLADE_TYPE_GLIST;

		accel_type = g_param_type_register_static
			("GladeParamAccel", &pspec_info);
	}
	return accel_type;
}

GParamSpec *
glade_param_spec_accel (const gchar   *name,
			const gchar   *nick,
			const gchar   *blurb,
			GType          accepted_type,
			GParamFlags    flags)
{
  GladeParamSpecAccel *pspec;

  pspec = g_param_spec_internal (GLADE_TYPE_PARAM_ACCEL,
				 name, nick, blurb, flags);
  
  pspec->type = accepted_type;
  return G_PARAM_SPEC (pspec);
}
#endif

/****************************************************************
 *  Built-in GladeParamSpecObjects for object list properties   *
 *  (Used as a pspec to desctibe an AtkRelationSet, but can     *
 *  for any object list property)                               *
 ****************************************************************/
static void
param_objects_init (GParamSpec *pspec)
{
	GladeParamSpecObjects *ospec = GLADE_PARAM_SPEC_OBJECTS (pspec);
	ospec->type = G_TYPE_OBJECT;
}

static void
param_objects_set_default (GParamSpec *pspec,
			   GValue     *value)
{
	if (value->data[0].v_pointer != NULL)
	{
		g_free (value->data[0].v_pointer);
	}
	value->data[0].v_pointer = NULL;
}

static gboolean
param_objects_validate (GParamSpec *pspec,
			GValue     *value)
{
	GladeParamSpecObjects *ospec = GLADE_PARAM_SPEC_OBJECTS (pspec);
	GList                 *objects, *list, *toremove = NULL;
	GObject               *object;

	objects = value->data[0].v_pointer;

	for (list = objects; list; list = list->next)
	{
		object = list->data;

		if (G_TYPE_IS_INTERFACE (ospec->type) &&
		    glade_util_class_implements_interface
		    (G_OBJECT_TYPE (object), ospec->type) == FALSE)
			toremove = g_list_prepend (toremove, object);
		else if (G_TYPE_IS_INTERFACE (ospec->type) == FALSE &&
			 g_type_is_a (G_OBJECT_TYPE (object), 
				      ospec->type) == FALSE)
			toremove = g_list_prepend (toremove, object);
	}

	for (list = toremove; list; list = list->next)
	{
		object = list->data;
		objects = g_list_remove (objects, object);
	}
	if (toremove) g_list_free (toremove);
 
	value->data[0].v_pointer = objects;

	return toremove != NULL;
}

static gint
param_objects_values_cmp (GParamSpec   *pspec,
			  const GValue *value1,
			  const GValue *value2)
{
  guint8 *p1 = value1->data[0].v_pointer;
  guint8 *p2 = value2->data[0].v_pointer;

  /* not much to compare here, try to at least provide stable lesser/greater result */

  return p1 < p2 ? -1 : p1 > p2;
}

GType
glade_param_objects_get_type (void)
{
	static GType objects_type = 0;

	if (objects_type == 0)
	{
		static /* const */ GParamSpecTypeInfo pspec_info = {
			sizeof (GladeParamSpecObjects),  /* instance_size */
			16,                         /* n_preallocs */
			param_objects_init,         /* instance_init */
			0xdeadbeef,                 /* value_type, assigned further down */
			NULL,                       /* finalize */
			param_objects_set_default,  /* value_set_default */
			param_objects_validate,     /* value_validate */
			param_objects_values_cmp,   /* values_cmp */
		};
		pspec_info.value_type = GLADE_TYPE_GLIST;

		objects_type = g_param_type_register_static
			("GladeParamObjects", &pspec_info);
	}
	return objects_type;
}

GParamSpec *
glade_param_spec_objects (const gchar   *name,
			  const gchar   *nick,
			  const gchar   *blurb,
			  GType          accepted_type,
			  GParamFlags    flags)
{
  GladeParamSpecObjects *pspec;

  pspec = g_param_spec_internal (GLADE_TYPE_PARAM_OBJECTS,
				 name, nick, blurb, flags);
  
  pspec->type = accepted_type;
  return G_PARAM_SPEC (pspec);
}

void
glade_param_spec_objects_set_type (GladeParamSpecObjects *pspec,
				   GType                  type)
{
	pspec->type = type;
}

GType
glade_param_spec_objects_get_type  (GladeParamSpecObjects *pspec)
{
	return pspec->type;
}

/* This was developed for the purpose of holding a list
 * of 'targets' in an AtkRelation (we are setting it up
 * as a property)
 */
GParamSpec *
glade_standard_objects_spec (void)
{
	return glade_param_spec_objects ("objects", _("Objects"), 
					 _("A list of objects"),
					 G_TYPE_OBJECT,
					 G_PARAM_READWRITE);
}


/****************************************************************
 *                    Basic types follow                        *
 ****************************************************************/
GParamSpec *
glade_standard_int_spec (void)
{
	return g_param_spec_int ("int", _("Integer"), 
				 _("An integer value"),
				 0, G_MAXINT,
				 0, G_PARAM_READWRITE);
}

GParamSpec *
glade_standard_string_spec (void)
{
	return g_param_spec_string ("string", _("String"), 
				    _("An entry"), "", 
				    G_PARAM_READWRITE);
}

GParamSpec *
glade_standard_strv_spec (void)
{
	return g_param_spec_boxed ("strv", _("Strv"),
				   _("String array"),
				   G_TYPE_STRV,
				   G_PARAM_READWRITE);
}

GParamSpec *
glade_standard_float_spec (void)
{
	return g_param_spec_float ("float", _("Float"), 
				   _("A floating point entry"),
				   0.0F, G_MAXFLOAT, 0.0F,
				   G_PARAM_READWRITE);
}

GParamSpec *
glade_standard_boolean_spec (void)
{
	return g_param_spec_boolean ("boolean", _("Boolean"),
				     _("A boolean value"), FALSE,
				     G_PARAM_READWRITE);
}
