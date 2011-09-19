/*
 * Copyright (C) 2006-2011 Juan Pablo Ugarte.
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

#include <Python.h>
#include <pygobject.h>

#include <gladeui/glade.h>

static void
python_init (void)
{
  char *argv[1];
  
  if (Py_IsInitialized ())
    return;

  Py_InitializeEx (0);

  argv[0] = g_get_prgname ();

  PySys_SetArgv (1, argv);
}

static void
glade_python_init_pygobject_check (gint req_major, gint req_minor, gint req_micro)
{
  PyObject *gi, *gobject;

  /* import gobject */
  pygobject_init (req_major, req_minor, req_micro);
  if (PyErr_Occurred ())
    {
      g_warning ("Error initializing Python interpreter: could not "
                 "import pygobject");

      return;
    }

  gi = PyImport_ImportModule ("gi");
  if (gi == NULL)
    {
      g_warning ("Error initializing Python interpreter: could not "
                 "import gi");

      return;
    }

  gobject = PyImport_ImportModule ("gi.repository.GObject");
  if (gobject == NULL)
    {
      g_warning ("Error initializing Python interpreter: could not "
                 "import gobject");

      return;
    }
}

static void
glade_python_setup ()
{
  gchar *command;
  const gchar *module_path;

  Py_SetProgramName (PACKAGE_NAME);

  /* Initialize the Python interpreter */
  python_init ();

  /* Check and init pygobject */

  PyErr_Clear ();
  glade_python_init_pygobject_check (PYGOBJECT_REQUIRED_MAJOR,
                                     PYGOBJECT_REQUIRED_MINOR,
                                     PYGOBJECT_REQUIRED_MICRO);
  if (PyErr_Occurred ())
    {
      PyObject *ptype, *pvalue, *ptraceback;
      PyErr_Fetch (&ptype, &pvalue, &ptraceback);
      g_warning ("Unable to load pygobject module >= %d.%d.%d, "
                 "please make sure it is in python's path (sys.path). "
                 "(use PYTHONPATH env variable to specify non default paths)\n%s",
                 PYGOBJECT_REQUIRED_MAJOR, PYGOBJECT_REQUIRED_MINOR,
                 PYGOBJECT_REQUIRED_MICRO, PyString_AsString (pvalue));
      PyErr_Clear ();
      Py_Finalize ();
      return;
    }

  pyg_disable_warning_redirections ();

  /* Set path */
  module_path = g_getenv (GLADE_ENV_MODULE_PATH);
  if (module_path == NULL)
    command = g_strdup_printf ("import sys; sys.path+=['%s'];\n",
                               glade_app_get_modules_dir ());
  else
    command = g_strdup_printf ("import sys; sys.path+=['%s', '%s'];\n",
                               module_path,
                               glade_app_get_modules_dir ());

  PyRun_SimpleString (command);
  g_free (command);
}


void
glade_python_init (const gchar * name)
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

  /* Importing the module will create all the GTypes so that glade can use them at runtime */
  PyRun_SimpleString (import_sentence);
  
  g_free (import_sentence);
}
