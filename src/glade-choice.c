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
#include "glade-xml-utils.h"

#include "glade-choice.h"

GladeChoice *
glade_choice_new (void)
{
	GladeChoice *choice;

	choice = g_new0 (GladeChoice, 1);

	return choice;
}


/* This should go outside and it should be generated from ./make_enums.pl taking enums.txt
 * as input. Chema */
/* I think gtk 2.0 already has a way of doing this .. Chema */
static gint
glade_enum_from_string (const gchar *string)
{
	g_return_val_if_fail (string != NULL, -1);

	if (strcmp (string, "-") == 0)
		return 0;
	if (strcmp (string, "GTK_JUSTIFY_LEFT") == 0)
		return GTK_JUSTIFY_LEFT;
	if (strcmp (string, "GTK_JUSTIFY_RIGHT") == 0)
		return GTK_JUSTIFY_RIGHT;
	if (strcmp (string, "GTK_JUSTIFY_CENTER") == 0)
		return GTK_JUSTIFY_CENTER;
	if (strcmp (string, "GTK_JUSTIFY_FILL") == 0)
		return GTK_JUSTIFY_FILL;
	if (strcmp (string, "GTK_WINDOW_TOPLEVEL") == 0)
		return GTK_WINDOW_TOPLEVEL;
	if (strcmp (string, "GTK_WINDOW_POPUP") == 0)
		return GTK_WINDOW_POPUP;
	if (strcmp (string, "GTK_WIN_POS_NONE") == 0)
		return GTK_WIN_POS_NONE;
	if (strcmp (string, "GTK_WIN_POS_CENTER") == 0)
		return GTK_WIN_POS_CENTER;
	if (strcmp (string, "GTK_WIN_POS_MOUSE") == 0)
		return GTK_WIN_POS_MOUSE;
	if (strcmp (string, "GTK_RELIEF_NORMAL") == 0)
		return GTK_RELIEF_NORMAL;
	if (strcmp (string, "GTK_RELIEF_HALF") == 0)
		return GTK_RELIEF_HALF;
	if (strcmp (string, "GTK_RELIEF_NONE") == 0)
		return GTK_RELIEF_NONE;

	return -1;
}

static gchar *
glade_string_from_string (const gchar *string)
{
	g_return_val_if_fail (string != NULL, NULL);
	
	if (strcmp (string, "-") == 0)
		return "None";
#if 0  /* Add the gtk stock buttons list here. Gtk takes care of stock
	* items now
	*/
	if (strcmp (string, "GNOME_STOCK_BUTTON_OK") == 0)
		return GNOME_STOCK_BUTTON_OK;
	if (strcmp (string, "GNOME_STOCK_BUTTON_CANCEL") == 0)
		return GNOME_STOCK_BUTTON_CANCEL;
	if (strcmp (string, "GNOME_STOCK_BUTTON_YES") == 0)
		return GNOME_STOCK_BUTTON_YES;
	if (strcmp (string, "GNOME_STOCK_BUTTON_NO") == 0)
		return GNOME_STOCK_BUTTON_NO;
	if (strcmp (string, "GNOME_STOCK_BUTTON_CLOSE") == 0)
		return GNOME_STOCK_BUTTON_CLOSE;
	if (strcmp (string, "GNOME_STOCK_BUTTON_APPLY") == 0)
		return GNOME_STOCK_BUTTON_APPLY;
	if (strcmp (string, "GNOME_STOCK_BUTTON_HELP") == 0)
		return GNOME_STOCK_BUTTON_HELP;
	if (strcmp (string, "GNOME_STOCK_BUTTON_NEXT") == 0)
		return GNOME_STOCK_BUTTON_NEXT;
	if (strcmp (string, "GNOME_STOCK_BUTTON_PREV") == 0)
		return GNOME_STOCK_BUTTON_PREV;
	if (strcmp (string, "GNOME_STOCK_BUTTON_UP") == 0)
		return GNOME_STOCK_BUTTON_UP;
	if (strcmp (string, "GNOME_STOCK_BUTTON_DOWN") == 0)
		return GNOME_STOCK_BUTTON_DOWN;
	if (strcmp (string, "GNOME_STOCK_BUTTON_FONT") == 0)
		return GNOME_STOCK_BUTTON_FONT;
#endif	

	g_warning ("Could not determine string from string (%s)\n",
		   string);

	return NULL;
}

static GladeChoice *
glade_choice_new_from_node (GladeXmlNode *node)
{
	GladeChoice *choice;

	if (!glade_xml_node_verify (node, GLADE_TAG_CHOICE))
		return NULL;
	
	choice = glade_choice_new ();
	choice->name   = glade_xml_get_value_string_required (node, GLADE_TAG_NAME, NULL);
	choice->symbol = glade_xml_get_value_string_required (node, GLADE_TAG_SYMBOL, NULL);
	choice->value  = glade_enum_from_string (choice->symbol);
	if (choice->value == -1)
		choice->string = glade_string_from_string (choice->symbol);

	if ((choice->value == -1 && choice->string == NULL) || !choice->name) {
		g_warning ("Could not create Choice from node");
		return NULL;
	}

	return choice;
}

GList *
glade_choice_list_new_from_node (GladeXmlNode *node)
{
	GladeXmlNode *child;
	GladeChoice *choice;
	GList *list;

	if (!glade_xml_node_verify (node, GLADE_TAG_CHOICES))
		return NULL;

	list = NULL;
	child = glade_xml_node_get_children (node);

	for (; child != NULL; child = glade_xml_node_next (child)) {
		if (!glade_xml_node_verify (child, GLADE_TAG_CHOICE))
			return NULL;
		choice = glade_choice_new_from_node (child);
		if (choice == NULL)
			return NULL;
		list = g_list_prepend (list, choice);
	}

	list = g_list_reverse (list);

	return list;
}
