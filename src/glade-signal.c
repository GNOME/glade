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
 *   Paolo Borelli <pborelli@katamail.com>
 */

#include <string.h>

#include "glade.h"
#include "glade-signal.h"
#include "glade-xml-utils.h"

/**
 * glade_signal_new:
 * @name: a name for the signal
 * @handler: a handler function for the signal
 * @after: #gboolean indicating whether this handler should be called after
 *         the default handler
 *
 * Creates a new #GladeSignal with the given parameters.
 *
 * Returns: the new #GladeSignal
 */
GladeSignal *
glade_signal_new (const gchar *name, const gchar *handler, gboolean after)
{
	GladeSignal *signal = g_new0 (GladeSignal, 1);

	signal->name = g_strdup (name);
	signal->handler = g_strdup (handler);
	signal->after = after;

	return signal;
}

/**
 * glade_signal_free:
 * @signal: a #GladeSignal
 *
 * Frees @signal and its associated memory.
 */
void
glade_signal_free (GladeSignal *signal)
{
	g_return_if_fail (GLADE_IS_SIGNAL (signal));

	g_free (signal->name);
	g_free (signal->handler);
	g_free (signal);
}

/**
 * glade_signal_equal:
 * @sig1: a #GladeSignal
 * @sig2: a #GladeSignal
 *
 * Returns: %TRUE if @sig1 and @sig2 have identical attributes, %FALSE otherwise
 */
gboolean
glade_signal_equal (GladeSignal *sig1, GladeSignal *sig2)
{
	gboolean ret = FALSE;

	g_return_val_if_fail (GLADE_IS_SIGNAL (sig1), FALSE);
	g_return_val_if_fail (GLADE_IS_SIGNAL (sig2), FALSE);

	if (!strcmp (sig1->name, sig2->name) &&
	    !strcmp (sig1->handler, sig2->handler) &&
	    sig1->after == sig2->after)
			ret = TRUE;

	return ret;
}

/**
 * glade_signal_clone:
 * @signal: a #GladeSignal
 *
 * Returns: a new #GladeSignal with the same attributes as @signal
 */
GladeSignal *
glade_signal_clone (const GladeSignal *signal)
{
	return glade_signal_new (signal->name, signal->handler, signal->after);
}

/**
 * glade_signal_write:
 * @context: a #GladeXmlContext
 * @signal: a #GladeSignal
 *
 * Returns: a new #GladeXmlNode in @context for @signal
 */
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

/**
 * glade_signal_new_from_node:
 * @node: a #GladeXmlNode
 *
 * Returns: a new #GladeSignal with the attributes defined in @node, %NULL if
 *          there is an error
 */
GladeSignal *
glade_signal_new_from_node (GladeXmlNode *node)
{
	GladeSignal *signal;

	if (!glade_xml_node_verify (node, GLADE_XML_TAG_SIGNAL))
		return NULL;

	signal = g_new0 (GladeSignal, 1);
	signal->name    = glade_xml_get_property_string_required (node, GLADE_XML_TAG_NAME, NULL);
	signal->handler = glade_xml_get_property_string_required (node, GLADE_XML_TAG_HANDLER, NULL);
	signal->after   = glade_xml_get_property_boolean (node, GLADE_XML_TAG_AFTER, FALSE);

	if (!signal->name)
		return NULL;

	return signal;
}
