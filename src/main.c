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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "glade.h"
#include "glade-widget-class.h"
#include "glade-debug.h"
#include "glade-cursor.h"
#include "glade-catalog.h"
#include "glade-project-window.h"
#include "glade-transform.h"

#include <locale.h>
#include <gmodule.h>

#ifdef HAVE_LIBPOPT
#include <popt.h>
#endif

#ifdef G_OS_WIN32
#include <stdlib.h> /* __argc & __argv on the windows build */
#endif

static gchar *widget_name = NULL;

#ifdef HAVE_LIBPOPT
static struct poptOption options[] = {
	{
		"dump",
		'\0',
		POPT_ARG_STRING,
		&widget_name,
		0,
		"Dump the properties of a widget. --dump [gtk type] where type can be GtkWindow, GtkLabel etc.",
		NULL
	},
	{
		NULL,
		'\0',
		0,
		NULL,
		0,
		NULL,
		NULL
	}
};

static GList *
parse_command_line (poptContext pctx)
{
	gint opt;
	const gchar **args;
	GList *files = NULL;
	gint i;

	/* parse options */
	while ((opt = poptGetNextOpt (pctx)) > 0 || opt == POPT_ERROR_BADOPT)
		/* do nothing */ ;

	/* load the args that aren't options as files */
	args = poptGetArgs (pctx);

	for (i = 0; args && args[i]; i++)
		files = g_list_prepend (files, g_strdup (args[i]));

	files = g_list_reverse (files);

	return files;
}
#endif

static gint
glade_init (void)
{
	GladeProjectWindow *project_window;
	GList *catalogs;

	if (!g_module_supported ())
	{
		g_warning (_("gmodule support not found. gmodule support is requiered "
			     "for glade to work"));
		return FALSE;
	}

	/* register transformation functions */
	glade_register_transformations ();

	/*
	 * 1. Init the cursors
	 * 2. Create the catalog
	 * 3. Create the project window
	 */
	glade_cursor_init ();

	catalogs = glade_catalog_load_all ();
	if (!catalogs)
		return FALSE;

	project_window = glade_project_window_new (catalogs);

	return TRUE;
}

#ifdef G_OS_WIN32
char* g_glade_data_dir;
char* g_pixmaps_dir;
char* g_widgets_dir;
char* g_catalogs_dir;
char* g_modules_dir;
char* g_glade_localedir;
char* g_glade_icondir;
#endif

int
main (int argc, char *argv[])
{
	GList *files = NULL;
#ifdef HAVE_LIBPOPT
	poptContext popt_context;
#endif
#ifdef G_OS_WIN32
    gchar *prefix;

    prefix = g_win32_get_package_installation_directory (NULL, NULL);
    g_glade_data_dir = g_build_filename (prefix, "share", "glade", NULL);
    g_pixmaps_dir = g_build_filename (prefix, "lib", PACKAGE "-" VERSION, "pixmaps", NULL);
    g_widgets_dir = g_build_filename (prefix, "lib", PACKAGE "-" VERSION, "widgets", NULL);
    g_catalogs_dir = g_build_filename (prefix, "lib", PACKAGE "-" VERSION, "catalogs", NULL);
    g_modules_dir = g_build_filename (prefix, "lib", "glade", NULL);
    g_glade_localedir = g_build_filename (prefix, "lib", "locale", NULL);
    g_glade_icondir = g_build_filename (prefix, "share", "pixmaps", NULL);
    g_free (prefix);
#endif

#ifdef ENABLE_NLS
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, GLADE_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	g_set_application_name (_("Glade-3 GUI Builder"));

#ifdef HAVE_LIBPOPT
	popt_context = poptGetContext ("Glade3", argc, (const char **) argv, options, 0);
	files = parse_command_line (popt_context);
	poptFreeContext (popt_context);
#endif

	gtk_init (&argc, &argv);

	glade_setup_log_handlers ();

	if (!glade_init ())
		return -1;

	if (widget_name != NULL)
	{
		GladeWidgetClass *class;
		class = glade_widget_class_get_by_name (widget_name);
		if (class)
			glade_widget_class_dump_param_specs (class);
		return 0;
	}

	glade_project_window_show_all ();

	if (files)
		for (; files; files = files->next)
		{
			glade_project_window_open_project (files->data);
		}
	else
		glade_project_window_new_project ();

	gtk_main ();

#ifdef G_OS_WIN32
    g_free (g_glade_data_dir);
    g_free (g_pixmaps_dir);
    g_free (g_widgets_dir);
    g_free (g_catalogs_dir);
    g_free (g_modules_dir);
    g_free (g_glade_localedir);
    g_free (g_glade_icondir);
#endif

	return 0;
}

#ifdef G_OS_WIN32
/* In case we build this as a windowed application */

#ifdef __GNUC__
#define _stdcall  __attribute__((stdcall))
#endif

int _stdcall
WinMain (struct HINSTANCE__ *hInstance,
	 struct HINSTANCE__ *hPrevInstance,
	 char               *lpszCmdLine,
	 int                 nCmdShow)
{
	return main (__argc, __argv);
}
#endif

