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

#include <gmodule.h>
#include <popt.h>

static GList * parse_command_line (poptContext);

gchar * widget_name = NULL;

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

static gint
glade_init ()
{
	GladeProjectWindow *project_window;
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
	glade_packing_init ();

	catalog = glade_catalog_load ();
	if (catalog == NULL)
		return FALSE;

	project_window = glade_project_window_new (catalog);

	return TRUE;
}

int
main (int argc, char *argv[])
{
	GladeProjectWindow *gpw;
	GladeProject *project;
	poptContext popt_context;
	GList *files;

#ifdef ENABLE_NLS
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, GLADE_LOCALEDIR);
	textdomain (GETTEXT_PACKAGE);
#if 0
	g_print ("textdomain %s\n", GETTEXT_PACKAGE);
	g_print ("localedir  %s\n", GLADE_LOCALEDIR);
	g_print (_("Translate me\n"));
#endif
#endif
						
	popt_context = poptGetContext ("Glade2", argc, (const char **) argv, options, 0);
	files = parse_command_line (popt_context);
	poptFreeContext (popt_context);

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
			project = glade_project_open_from_file (files->data);
			glade_project_window_add_project (gpw, project);
		}
	} else {
		project = glade_project_new (TRUE);
		glade_project_window_add_project (gpw, project);
	}
			
	gtk_main ();

	return 0;
}


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

