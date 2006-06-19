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
GladeSignal *glade_signal_new (const gchar *name,
			       const gchar *handler,
			       const gchar *userdata,
			       gboolean     lookup,
			       gboolean     after)
{
	GladeSignal *signal = g_new0 (GladeSignal, 1);

	signal->name     = g_strdup (name);
	signal->handler  = g_strdup (handler);
	signal->userdata = userdata ? g_strdup (userdata) : NULL;
	signal->lookup   = lookup;
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
	    sig1->after  == sig2->after             &&
	    sig1->lookup == sig2->lookup)
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
				 signal->lookup,
				 signal->after);
}

/**
 * glade_signal_write:
 * @info:
 * @signal:
 * @interface:
 *
 * Returns: TRUE if succeed
 */
gboolean
glade_signal_write (GladeSignalInfo *info, GladeSignal *signal,
		     GladeInterface *interface)
{
	info->name    = glade_xml_alloc_string(interface, signal->name);
	glade_util_replace (info->name, '-', '_');
	info->handler = glade_xml_alloc_string(interface, signal->handler);
	info->object  =
		signal->userdata ?
		glade_xml_alloc_string(interface, signal->userdata) : NULL;
	info->after   = signal->after;
	info->lookup  = signal->lookup;
	return TRUE;
}

/*
 * Returns a new GladeSignal with the attributes defined in node
 */
GladeSignal *glade_signal_new_from_signal_info (GladeSignalInfo *info)
{
	GladeSignal *signal;

	g_return_val_if_fail (info != NULL, NULL);

	signal = g_new0 (GladeSignal, 1);
	signal->name     = g_strdup (info->name);
	glade_util_replace (signal->name, '_', '-');
	signal->handler  = g_strdup (info->handler);
	signal->after    = info->after;
	signal->userdata = g_strdup (info->object);

	if (!signal->name)
		return NULL;

	return signal;
}
