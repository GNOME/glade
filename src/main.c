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

gchar *glade_data_dir = GLADE_DATA_DIR;
gchar *glade_pixmaps_dir = PIXMAPS_DIR;
gchar *glade_widgets_dir = WIDGETS_DIR;
gchar *glade_catalogs_dir = CATALOGS_DIR;
#ifdef MODULES_DIR
gchar *glade_modules_dir = MODULES_DIR;
#else
gchar *glade_modules_dir = NULL;
#endif
gchar *glade_locale_dir = LOCALE_DIR;
gchar *glade_icon_dir = ICONS_DIR;

static gchar *widget_name = NULL;
gboolean verbose = FALSE;

#ifdef HAVE_LIBPOPT
static struct poptOption options[] = {
	{ "dump", '\0', POPT_ARG_STRING, &widget_name, 0,
	  N_("dump the properties of a widget. --dump [gtk type] "
	     "where type can be GtkWindow, GtkLabel etc."), NULL },
	{ "verbose", 'v', POPT_ARG_NONE, &verbose, 0,
	  N_("be verbose."), NULL },
#ifndef USE_POPT_DLL
	POPT_AUTOHELP
#else
	/* poptHelpOptions can not be resolved during linking on Win32,
	   get it at runtime. */
	{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, 0, 0,
	  N_("Help options:"), NULL },
#endif
	POPT_TABLEEND
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
		g_warning (_("gmodule support not found. gmodule support is required "
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
	{
		g_warning ("Couldn't load any catalogs");
		return FALSE;
	}

	project_window = glade_project_window_new (catalogs);

	return TRUE;
}

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
	glade_data_dir = g_build_filename (prefix, "share", "glade", NULL);
	glade_pixmaps_dir = g_build_filename (prefix, "lib", PACKAGE "-" VERSION, "pixmaps", NULL);
	glade_widgets_dir = g_build_filename (prefix, "lib", PACKAGE "-" VERSION, "widgets", NULL);
	glade_catalogs_dir = g_build_filename (prefix, "lib", PACKAGE "-" VERSION, "catalogs", NULL);
	glade_modules_dir = g_build_filename (prefix, "lib", "glade", NULL);
	glade_locale_dir = g_build_filename (prefix, "lib", "locale", NULL);
	glade_icon_dir = g_build_filename (prefix, "share", "pixmaps", NULL);
	g_free (prefix);
#endif

#ifdef ENABLE_NLS
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, glade_locale_dir);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	g_set_application_name (_("Glade-3 GUI Builder"));

#ifdef HAVE_LIBPOPT
#  ifdef USE_POPT_DLL
	options[sizeof (options) / sizeof (options[0]) - 2].arg = poptHelpOptions;
#  endif
	popt_context = poptGetContext ("Glade3", argc, (const char **) argv, options, 0);
	files = parse_command_line (popt_context);
	poptFreeContext (popt_context);
#endif

	gtk_init (&argc, &argv);

	/* XXX This is a hack to make up for a bug in GTK+;
	 * gtk_icon_theme_get_default() wont return anything
	 * untill an `ensure_default_icons ();' is provoked
	 * (gtk_icon_factory_lookup_default does this).
	 */
	gtk_icon_factory_lookup_default ("");

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
	g_free (glade_data_dir);
	g_free (glade_pixmaps_dir);
	g_free (glade_widgets_dir);
	g_free (glade_catalogs_dir);
	g_free (glade_modules_dir);
	g_free (glade_locale_dir);
	g_free (glade_icon_dir);
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

