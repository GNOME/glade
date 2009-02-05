/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006-2008 Juan Pablo Ugarte.
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
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#include <config.h>

#include <gladeui/glade.h>

#include <Python.h>
#include <pygobject.h>

static void
python_init (void)
{
	char *argv[1];
	
	if (Py_IsInitialized ()) return;

	Py_InitializeEx (0);

	argv[0] = g_get_prgname ();
	
	PySys_SetArgv (1, argv);
}

static void
glade_python_init_pygtk_check (gint req_major, gint req_minor, gint req_micro)
{
	PyObject *gobject, *mdict, *version;
	int found_major, found_minor, found_micro;
	
	init_pygobject();
	
	gobject = PyImport_ImportModule("gobject");
	mdict = PyModule_GetDict(gobject);
        version = PyDict_GetItemString(mdict, "pygtk_version");
	if (!version)
	{
		PyErr_SetString(PyExc_ImportError, "PyGObject version too old");
		return;
	}
	if (!PyArg_ParseTuple(version, "iii", &found_major, &found_minor, &found_micro))
		return;
	if (req_major != found_major || req_minor >  found_minor ||
	    (req_minor == found_minor && req_micro > found_micro))
	{
		PyErr_Format(PyExc_ImportError, 
                     "PyGObject version mismatch, %d.%d.%d is required, "
                     "found %d.%d.%d.", req_major, req_minor, req_micro,
                     found_major, found_minor, found_micro);
		return;
	}
}

static void
glade_python_setup ()
{
	gchar *command;
	
    	Py_SetProgramName (PACKAGE_NAME);
	
	/* Initialize the Python interpreter */
	python_init ();
	
	/* Check and init pygobject >= 2.12.0 */
	PyErr_Clear ();	
	glade_python_init_pygtk_check (PYGTK_REQUIRED_MAJOR, PYGTK_REQUIRED_MINOR, PYGTK_REQUIRED_MICRO);
	if (PyErr_Occurred ())
	{
		PyObject *ptype, *pvalue, *ptraceback;
		PyErr_Fetch(&ptype, &pvalue, &ptraceback);
		g_warning ("Unable to load pygobject module >= %d.%d.%d, "
			   "please make sure it is in python's path (sys.path). "
			   "(use PYTHONPATH env variable to specify non default paths)\n%s",
			   PYGTK_REQUIRED_MAJOR, PYGTK_REQUIRED_MINOR, PYGTK_REQUIRED_MICRO,
			   PyString_AsString (pvalue));
		PyErr_Clear ();
		Py_Finalize ();
		return;
	}
	
	pyg_disable_warning_redirections ();

	/* Set path */
	command = g_strdup_printf ("import sys; sys.path+=['.', '%s', '%s'];\n",
				   g_getenv (GLADE_ENV_CATALOG_PATH),
				   glade_app_get_modules_dir ());
	PyRun_SimpleString (command);
	g_free (command);
}


void
glade_python_init (const gchar *name)
{
	static gboolean init = TRUE;
	gchar *import_sentence;
	
	if (init)
	{
		glade_python_setup ();
		init = FALSE;
	}
	
	/* Yeah, we use the catalog name as the library */
	import_sentence = g_strdup_printf ("import %s;", name);
	PyRun_SimpleString (import_sentence);
	g_free (import_sentence);
}
