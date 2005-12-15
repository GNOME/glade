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

/* Generate the GType dynamicly from the gtk stock engine
 */
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

GParamSpec *
glade_standard_int_spec (void)
{
	static GParamSpec *int_spec = NULL;
	if (!int_spec)
		int_spec = g_param_spec_int ("int", _("Integer"), 
				       _("An integer value"),
				       0, G_MAXINT,
				       0, G_PARAM_READWRITE);
	return int_spec;
}

GParamSpec *
glade_standard_string_spec (void)
{
	static GParamSpec *str_spec = NULL;
	if (!str_spec)
		str_spec = g_param_spec_string ("string", _("String"), 
						_("An entry"), "", 
						G_PARAM_READWRITE);
	return str_spec;
}

GParamSpec *
glade_standard_strv_spec (void)
{
	static GParamSpec *strv_spec = NULL;

	if (!strv_spec)
		strv_spec = g_param_spec_boxed ("strv", _("Strv"),
						_("String array"),
						G_TYPE_STRV,
						G_PARAM_READWRITE);

	return strv_spec;
}

GParamSpec *
glade_standard_float_spec (void)
{
	static GParamSpec *float_spec = NULL;
	if (!float_spec)
		float_spec = g_param_spec_float ("float", _("Float"), 
						 _("A floating point entry"),
						 0.0F, G_MAXFLOAT, 0.0F,
						 G_PARAM_READWRITE);
	return float_spec;
}

GParamSpec *
glade_standard_boolean_spec (void)
{
	static GParamSpec *boolean_spec = NULL;
	if (!boolean_spec)
		boolean_spec = g_param_spec_boolean ("boolean", _("Boolean"),
						    _("A boolean value"), FALSE,
						    G_PARAM_READWRITE);
	return boolean_spec;
}
