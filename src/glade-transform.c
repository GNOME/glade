/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003 Joaquin Cuenca Abela
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
 *   Joaquin Cuenca Abela <e98cuenc@yahoo.com>
 */


#include "glade-transform.h"
#include <glib-object.h>
#include <stdlib.h>

#define DEFINE_TRANSFORM(to_member, c_type, transf_name)				\
static void										\
transform_string_##to_member (const GValue *src_value,					\
                              GValue       *dest_value)					\
{											\
	c_type c_value = transf_name (src_value->data[0].v_pointer);			\
	dest_value->data[0].v_##to_member = c_value;					\
}

DEFINE_TRANSFORM (double, double, atof)
DEFINE_TRANSFORM (float, float, atof)
DEFINE_TRANSFORM (int, int, atoi)
DEFINE_TRANSFORM (uint, guint, atoi)
DEFINE_TRANSFORM (long, long, atol)
DEFINE_TRANSFORM (ulong, gulong, atol)
DEFINE_TRANSFORM (int64, gint64, atol)
DEFINE_TRANSFORM (uint64, guint64, atol)

static void
transform_string_boolean (const GValue *src_value, GValue *dest_value)
{
	const char *str = src_value->data[0].v_pointer;

	if (str && (!g_strcasecmp (str, "true") || atoi (str) != 0))
		dest_value->data[0].v_int = 1;
	else
		dest_value->data[0].v_int = 0;
}

void
glade_register_transformations (void)
{
	g_value_register_transform_func (G_TYPE_STRING, G_TYPE_DOUBLE, transform_string_double);
	g_value_register_transform_func (G_TYPE_STRING, G_TYPE_FLOAT, transform_string_float);
	g_value_register_transform_func (G_TYPE_STRING, G_TYPE_CHAR, transform_string_int);
	g_value_register_transform_func (G_TYPE_STRING, G_TYPE_UCHAR, transform_string_uint);
	g_value_register_transform_func (G_TYPE_STRING, G_TYPE_INT, transform_string_int);
	g_value_register_transform_func (G_TYPE_STRING, G_TYPE_UINT, transform_string_uint);
	g_value_register_transform_func (G_TYPE_STRING, G_TYPE_LONG, transform_string_long);
	g_value_register_transform_func (G_TYPE_STRING, G_TYPE_ULONG, transform_string_ulong);
	g_value_register_transform_func (G_TYPE_STRING, G_TYPE_INT64, transform_string_int64);
	g_value_register_transform_func (G_TYPE_STRING, G_TYPE_UINT64, transform_string_uint64);
	g_value_register_transform_func (G_TYPE_STRING, G_TYPE_BOOLEAN, transform_string_boolean);
}
