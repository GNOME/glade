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

/* TODO : code from glade-signal-editor shuold really be here , not there */

#include <string.h>

#include "glade.h"
#include "glade-signal.h"

GladeSignal *
glade_signal_new (const gchar *name, const gchar *handler, gboolean after)
{
	GladeSignal *signal = g_new0 (GladeSignal, 1);

	signal->name = g_strdup (name);
	signal->handler = g_strdup (handler);
	signal->after = after;

	return signal;
}

void
glade_signal_free (GladeSignal *signal)
{
	g_free (signal->name);
	g_free (signal->handler);
	g_free (signal);
}

GladeXmlNode *
glade_signal_write (GladeXmlContext *context, GladeSignal *signal)
{
	GladeXmlNode *node;
		
	node = glade_xml_node_new (context, GLADE_XML_TAG_SIGNAL);

	glade_xml_node_set_property_string (node, GLADE_XML_TAG_NAME, signal->name);
	glade_xml_node_set_property_string (node, GLADE_XML_TAG_HANDLER, signal->handler);
	glade_xml_node_set_property_boolean (node, GLADE_XML_TAG_AFTER, signal->after);

	return node;
}

