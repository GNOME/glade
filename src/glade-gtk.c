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

#include <string.h>

#include "glade.h"
#include "glade-property-class.h"

static void
glade_gtk_entry_set_text (GObject *object, const gchar *text)
{
	GtkEntry *entry = GTK_ENTRY (object);

	g_return_if_fail (GTK_IS_ENTRY (entry));
	
	gtk_entry_set_text (GTK_ENTRY (object), text);
}

static void
glade_gtk_option_menu_set_items (GObject *object, const gchar *items)
{
	GtkOptionMenu *option_menu; 
	GtkWidget *menu;
	const gchar *pos = items;
	const gchar *items_end = &items[strlen (items)];

	option_menu = GTK_OPTION_MENU (object);
	g_return_if_fail (GTK_IS_OPTION_MENU (option_menu));
	menu = gtk_option_menu_get_menu (option_menu);
	if (menu != NULL)
		gtk_option_menu_remove_menu (option_menu);
	
	menu = gtk_menu_new ();
	
	while (pos < items_end) {
		GtkWidget *menu_item;
		gchar *item_end = strchr (pos, '\n');
		if (item_end == NULL)
			item_end = (gchar *) items_end;
		*item_end = '\0';
		
		menu_item = gtk_menu_item_new_with_label (pos);
		gtk_widget_show (menu_item);
		gtk_menu_append (GTK_MENU (menu), menu_item);
		
		pos = item_end + 1;
	}
	
	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
}


static void
glade_gtk_progress_bar_set_format (GObject *object, const gchar *format)
{
	GtkProgressBar *bar;

	bar = GTK_PROGRESS_BAR (object);
	g_return_if_fail (GTK_IS_PROGRESS_BAR (bar));
	
	gtk_progress_set_format_string (GTK_PROGRESS (bar), format);
}






/* ================ Temp hack =================== */
typedef struct _GladeGtkFunction GladeGtkFunction;

struct _GladeGtkFunction {
	const gchar *name;
	gpointer function;
};


GladeGtkFunction functions [] = {
	{"glade_gtk_entry_set_text",          &glade_gtk_entry_set_text},
	{"glade_gtk_option_menu_set_items",   &glade_gtk_option_menu_set_items},
	{"glade_gtk_progress_bar_set_format", &glade_gtk_progress_bar_set_format},
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

