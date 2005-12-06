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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Joaquin Cuenca Abela <e98cuenc@yahoo.com>
 */

#include "glade.h"
#include "glade-debug.h"

#ifdef G_OS_UNIX
#include <signal.h>
#endif

#ifndef RETSIGTYPE
#define RETSIGTYPE void
#endif


static void
glade_log_handler (const char *domain,
             GLogLevelFlags level,
             const char *message,
             gpointer data)
{
	static volatile int want_breakpoint = 0;

	g_log_default_handler (domain, level, message, data);
	if (want_breakpoint && ((level & (G_LOG_LEVEL_CRITICAL /* | G_LOG_LEVEL_WARNING */)) != 0))
		G_BREAKPOINT ();
}

static void
glade_set_log_handler (const char *domain)
{
	g_log_set_handler (domain, G_LOG_LEVEL_MASK, glade_log_handler, NULL);
}

/**
 * glade_setup_log_handlers:
 *
 * Sets up a log handler to manage all %G_LOG_LEVEL_MASK errors of domain:
 * GLib, GLib-GObject, Gtk, Gdk, and domainless.
 */
void
glade_setup_log_handlers ()
{
	glade_set_log_handler ("");
	glade_set_log_handler ("GLib");
	glade_set_log_handler ("GLib-GObject");
	glade_set_log_handler ("Gtk");
	glade_set_log_handler ("Gdk");
}

