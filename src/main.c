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

#include <config.h>

#include "glade.h"
#include "glade-widget-class.h"
#include "glade-editor.h"
#include "glade-cursor.h"
#include "glade-catalog.h"
#include "glade-palette.h"
#include "glade-project.h"
#include "glade-project-view.h"
#include "glade-project-window.h"

#include <gmodule.h>

#ifdef FIX_POPT
static void parse_command_line (poptContext);

static struct poptOption options[] = {
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
	GladeProject *project;
	GladeCatalog *catalog;

	if (!g_module_supported ()) {
		g_warning (_("gmodule support not found. gmodule support is requiered "
			     "for glade to work"));
		return FALSE;
	}
			
	/*
	 * 1. Init the cursors
	 * 2. Create the catalog
	 * 3. Create the project
	 * 4. Create the project window
	 */
	glade_cursor_init ();

	catalog = glade_catalog_load ();
	if (catalog == NULL)
		return FALSE;

	project = glade_project_new ();

	project_window = glade_project_window_new (catalog);
	glade_project_window_add_project (project_window, project);
	gtk_widget_show (project_window->window);
	
	return TRUE;
}

int
main (int argc, char *argv[])
{
#ifdef FIX_POPT	
	poptContext pctx;
#endif	

#if 0	
#if 1
	gnome_init_with_popt_table ("Glade2", VERSION, argc, argv, options, 0, &pctx);
#else
	gnome_program_init ("Glade2", VERSION, &libgnomeui_module_info,
			    argc, argv, NULL);
#endif
#else
	gtk_init (&argc, &argv);
#endif	
	
#if 0
#ifdef ENABLE_NLS
	bindtextdomain (PACKAGE, GLADE_LOCALE_DIR);
	textdomain (PACKAGE);
#endif
#endif	

	if (!glade_init ())
		return -1;

#ifdef FIX_POPT
	parse_command_line (pctx);
#endif	

	gtk_main ();

	return 0;
}


#ifdef FIX_POPT
static void
parse_command_line (poptContext pctx)
{
#if 0  /* The new gnome-libs is crashing here .. */
	const gchar **args;
	gchar *arg_filename = NULL;
	gint i;

	g_print ("Get args. .\n");
	args = poptGetArgs (pctx);

	g_print ("Arg_filename %s\n", arg_filename);
	for (i = 0; args && args[i]; i++) {
		if (arg_filename == NULL)
			arg_filename = (gchar*) args[i];
	}

	g_print ("Arg_filename %s\n", arg_filename);
	
	poptFreeContext (pctx);
#endif	
}
#endif	


gchar * _ (gchar * name) { return name;};
