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
 *   Paolo Borelli <pborelli@katamail.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n-lib.h>

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
GladeSignal *glade_signal_new (const gchar *name,
			       const gchar *handler,
			       const gchar *userdata,
			       gboolean     after)
{
	GladeSignal *signal = g_new0 (GladeSignal, 1);

	signal->name     = g_strdup (name);
	signal->handler  = g_strdup (handler);
	signal->userdata = userdata ? g_strdup (userdata) : NULL;
	signal->after    = after;

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
	if (signal->userdata) g_free (signal->userdata);
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
	
	if (!strcmp (sig1->name, sig2->name)        &&
	    !strcmp (sig1->handler, sig2->handler)  &&
	    sig1->after  == sig2->after)
	{
		if ((sig1->userdata == NULL && sig2->userdata == NULL) ||
		    (sig1->userdata != NULL && sig2->userdata != NULL  &&
		     !strcmp (sig1->userdata, sig2->userdata)))
			ret = TRUE;
	}

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
	return glade_signal_new (signal->name,
				 signal->handler,
				 signal->userdata,
				 signal->after);

}

/**
 * glade_signal_write:
 * @signal: The #GladeSignal
 * @context: A #GladeXmlContext
 * @node: A #GladeXmlNode
 *
 * Writes @signal to @node
 */
void
glade_signal_write (GladeSignal     *signal,
		    GladeXmlContext *context,
		    GladeXmlNode    *node)
{
	GladeXmlNode *signal_node;
	gchar        *name;

	/*  Should assert GLADE_XML_TAG_WIDGET tag here, but no 
	 * access to project, so not really seriosly needed 
	 */

	name = g_strdup (signal->name);
	glade_util_replace (name, '-', '_');

	/* Now dump the node values... */
	signal_node = glade_xml_node_new (context, GLADE_XML_TAG_SIGNAL);
	glade_xml_node_append_child (node, signal_node);

	glade_xml_node_set_property_string (signal_node, GLADE_XML_TAG_NAME, name);
	glade_xml_node_set_property_string (signal_node, GLADE_XML_TAG_HANDLER, signal->handler);

	if (signal->userdata)
		glade_xml_node_set_property_string (signal_node, 
						    GLADE_XML_TAG_OBJECT, 
						    signal->userdata);

	if (signal->after)
		glade_xml_node_set_property_string (signal_node, 
						    GLADE_XML_TAG_AFTER, 
						    GLADE_XML_TAG_SIGNAL_TRUE);

	g_free (name);
}


/**
 * glade_signal_read:
 * @node: The #GladeXmlNode to read
 *
 * Reads and creates a ner #GladeSignal based on @node
 *
 * Returns: A newly created #GladeSignal
 */
GladeSignal *
glade_signal_read (GladeXmlNode *node)
{
	GladeSignal *signal;
	gchar *name, *handler;

	g_return_val_if_fail (glade_xml_node_verify_silent 
			      (node, GLADE_XML_TAG_SIGNAL), NULL);

	if (!(name = 
	      glade_xml_get_property_string_required (node, GLADE_XML_TAG_NAME, NULL)))
		return NULL;
	glade_util_replace (name, '_', '-');

	if (!(handler = 
	      glade_xml_get_property_string_required (node, GLADE_XML_TAG_HANDLER, NULL)))
	{
		g_free (name);
		return NULL;
	}

	signal = g_new0 (GladeSignal, 1);
	signal->name     = name;
	signal->handler  = handler;
	signal->after    = glade_xml_get_property_boolean (node, GLADE_XML_TAG_AFTER, FALSE);
	signal->userdata = glade_xml_get_property_string (node, GLADE_XML_TAG_OBJECT);

	return signal;
}
