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
#include "glade-editor.h"
#include "glade-cursor.h"
#include "glade-catalog.h"
#include "glade-packing.h"
#include "glade-palette.h"
#include "glade-project.h"
#include "glade-project-view.h"
#include "glade-project-window.h"

#include <locale.h>
#include <gmodule.h>
#ifdef G_OS_UNIX
#include <popt.h>

static GList * parse_command_line (poptContext);
#endif

#ifdef G_OS_WIN32
#include <stdlib.h> /* __argc & __argv on the windows build */
#endif

gchar * widget_name = NULL;

#ifdef G_OS_UNIX
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
#endif

static gint
glade_init ()
{
	GladeProjectWindow *project_window;
	GList *catalogs;

	if (!g_module_supported ()) {
		g_warning (_("gmodule support not found. gmodule support is requiered "
			     "for glade to work"));
		return FALSE;
	}

	/* register transformation functions */
	glade_register_transformations ();

	/*
	 * 1. Init the cursors
	 * 2. Create the catalog
	 * 3. Create the project
	 * 4. Create the project window
	 */
	glade_cursor_init ();
	glade_packing_init ();

	catalogs = glade_catalog_load_all ();
	if (catalogs == NULL)
		return FALSE;

	project_window = glade_project_window_new (catalogs);

	return TRUE;
}

int
main (int argc, char *argv[])
{
	GladeProjectWindow *gpw;
	GList *files = NULL;
#ifdef G_OS_UNIX
	poptContext popt_context;
#endif

#ifdef ENABLE_NLS
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, GLADE_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif
						
#ifdef G_OS_UNIX
	popt_context = poptGetContext ("Glade3", argc, (const char **) argv, options, 0);
	files = parse_command_line (popt_context);
	poptFreeContext (popt_context);
#endif

	gtk_init (&argc, &argv);

	if (!glade_init ())
		return -1;

	if (widget_name != NULL) {
		GladeWidgetClass *class;
		class = glade_widget_class_get_by_name (widget_name);
		if (class)
			glade_widget_class_dump_param_specs (class);
		return 0;
	}

	gpw = glade_project_window_get ();
	glade_project_window_show_all (gpw);

	if (files) {
		for (; files != NULL; files = files->next) {
			glade_project_open (files->data);
		}
	} else {
		GladeProject *project;
		project = glade_project_new (TRUE);
		glade_project_window_add_project (gpw, project);
	}
			
	gtk_main ();

	return 0;
}

#ifdef G_OS_UNIX
static GList *
parse_command_line (poptContext pctx)
{
	const gchar **args;
	GList *files = NULL;
	gint i;

	/* I have not figured out how popt works, but we need to call this function to make it work */
	poptGetNextOpt(pctx);
	args = poptGetArgs (pctx);

	for (i = 0; args && args[i]; i++) {
		files = g_list_prepend (files, g_strdup (args[i]));
	}

	files = g_list_reverse (files);

	return files;
}
#endif


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

