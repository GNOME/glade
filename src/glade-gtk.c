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

/* This struct is used for a temp hack */

#include "glade.h"
#include "glade-property-class.h"

static void
glade_gtk_entry_set_text (GObject *object, const gchar *text)
{
	gtk_entry_set_text (GTK_ENTRY (object), text);
}








/* ================ Temp hack =================== */
typedef struct _GladeGtkFunction GladeGtkFunction;

struct _GladeGtkFunction {
	const gchar *name;
	gpointer function;
};


GladeGtkFunction functions [] = {
	{"glade_gtk_entry_set_text", &glade_gtk_entry_set_text},
	{NULL, NULL},
};
	
gboolean
glade_gtk_get_set_function_hack (GladePropertyClass *class, const gchar *function_name)
{
	gint num;
	gint i;

	num = sizeof (functions) / sizeof (GladeGtkFunction);
	for (i = 0; i < num; i++) {
		if (strcmp (function_name, functions[i].name) == 0)
			break;
	}
	if (i == num) {
		g_warning ("Could not find the function %s for %s\n",
			   function_name, class->name);
		return FALSE;
	}

	class->set_function = functions[i].function;

	return TRUE;
}

