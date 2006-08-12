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
 *   Vincent Geddes <vgeddes@metroweb.co.za>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include <locale.h>
#include <gmodule.h>

#include <stdlib.h> /* defines __argc & __argv on the windows build */

#include "glade.h"
#include "glade-project-window.h"
#include "glade-debug.h"

#define APPLICATION_NAME (_("Glade 3"))


/* Application arguments */
static gboolean version = FALSE, without_devhelp = FALSE;
static gchar **files = NULL;

static GOptionEntry option_entries[] = 
{
  { "version", '\0', 0, G_OPTION_ARG_NONE, &version, "output version information and exit", NULL },
  { "without-devhelp", '\0', 0, G_OPTION_ARG_NONE, &without_devhelp, "disable DevHelp check", NULL },
  { G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &files, "", "" },
  { NULL }
};

/* Debugging arguments */
static gchar *widget_name = NULL;
static gboolean verbose = FALSE;

static GOptionEntry debug_option_entries[] = 
{
  { "dump", 'd', 0, G_OPTION_ARG_STRING, &widget_name, "dump the properties of a widget", "GTKWIDGET" },
  { "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, "be verbose", NULL },
  { NULL }
};

int
main (int argc, char *argv[])
{
	GladeProjectWindow *project_window;
	GOptionContext *option_context;
	GOptionGroup *option_group;
	GError *error = NULL;

#ifdef ENABLE_NLS
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, glade_locale_dir);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	/* Set up option groups */
	option_context = g_option_context_new ("[FILE...]");

	option_group = g_option_group_new ("glade",
					   N_("Glade"),
					   N_("Glade options"),
					   NULL, NULL);
	g_option_group_add_entries (option_group, option_entries);
	g_option_context_set_main_group (option_context, option_group);

	option_group = g_option_group_new ("debug",
					   "Glade debug options",
					   "Show debug options",
					   NULL, NULL);
	g_option_group_add_entries (option_group, debug_option_entries);
	g_option_context_add_group (option_context, option_group);

	/* Add Gtk option group */
	g_option_context_add_group (option_context, gtk_get_option_group (FALSE));

	/* Parse command line */
	if (!g_option_context_parse (option_context, &argc, &argv, &error))
	{
		g_option_context_free (option_context);
		
		if (error)
		{
			g_print ("%s\n", error->message);
			g_error_free (error);
		}
		else
			g_print ("An unknown error occurred\n");

		return -1;
	}
	
	g_option_context_free (option_context);
	option_context = NULL;

	if (version != FALSE)
	{
		/* Print version information and exit */
		g_print ("%s\n", PACKAGE_STRING);
		return 0;
	}

	/* Pass NULL here since we parsed the gtk+ args already...
	 * from this point on we need a DISPLAY variable to be set.
	 */
	gtk_init (NULL, NULL);
	
	/* Check for gmodule support */
	if (!g_module_supported ())
	{
		g_warning (_("gmodule support not found. gmodule support is required "
			     "for glade to work"));
		return -1;
	}

	g_set_application_name (APPLICATION_NAME);
	gtk_window_set_default_icon_name ("glade-3");
	
	glade_setup_log_handlers ();
	
	
	project_window = glade_project_window_new ();
	
	if (without_devhelp == FALSE)
		glade_project_window_check_devhelp (project_window);
	
	if (widget_name != NULL)
	{
		GladeWidgetClass *class;
		class = glade_widget_class_get_by_name (widget_name);
		if (class)
			glade_widget_class_dump_param_specs (class);
		return 0;
	}
	
	/* load files specified on commandline */
	if (files != NULL)
	{
		guint i;
		
		for (i=0; files[i] ; ++i)
		{
			if (g_file_test (files[i], G_FILE_TEST_EXISTS) != FALSE)
				glade_project_window_open_project (project_window, files[i]);
			else
				g_warning (_("Unable to open '%s', the file does not exist.\n"), files[i]);
		}
		g_strfreev (files);
	}

	if (glade_app_get_project () == NULL)
		glade_project_window_new_project (project_window);

	glade_project_window_show_all (project_window);

	gtk_main ();

	/* destroy GladeApp */
	g_object_unref (project_window);

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
