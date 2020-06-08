/*
 * Copyright (C) 2006-2015 Juan Pablo Ugarte.
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

#if PY_MAJOR_VERSION >= 3

#define WCHAR(str) L##str

#define PY_STRING(str) WCHAR(str)
#define STRING_FROM_PYSTR(pstr) PyUnicode_AsUTF8 (pstr)

#else

/* include bytesobject.h to map PyBytes_* to PyString_* */
#include <bytesobject.h>

#define PY_STRING(str) str
#define STRING_FROM_PYSTR(pstr) PyBytes_AsString (pstr)

#endif

static void
python_init (void)
{
  if (Py_IsInitialized ())
    return;

  Py_InitializeEx (0);

  /* if argc is 0 an empty string is prepended to sys.path */
  PySys_SetArgvEx (0, NULL, 0);
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

static gboolean
glade_python_setup ()
{
  GString *command;
  const gchar *module_path;
  const GList *paths;
  gchar **tokens = NULL;

  Py_SetProgramName (PY_STRING (PACKAGE_NAME));

  /* Initialize the Python interpreter */
  python_init ();

  /* Check and init pygobject */

  PyErr_Clear ();
  glade_python_init_pygobject_check (PYGOBJECT_REQUIRED_MAJOR,
                                     PYGOBJECT_REQUIRED_MINOR,
                                     PYGOBJECT_REQUIRED_MICRO);
  if (PyErr_Occurred ())
    {
      PyObject *ptype, *pvalue, *ptraceback, *pstr;
      const char *pvalue_char = "";

      PyErr_Fetch (&ptype, &pvalue, &ptraceback);
      PyErr_NormalizeException (&ptype, &pvalue, &ptraceback);

      if ((pstr = PyObject_Str (pvalue)))
        pvalue_char = STRING_FROM_PYSTR (pstr);

      g_warning ("Unable to load pygobject module >= %d.%d.%d, "
                 "please make sure it is in python's path (sys.path). "
                 "(use PYTHONPATH env variable to specify non default paths)\n%s",
                 PYGOBJECT_REQUIRED_MAJOR, PYGOBJECT_REQUIRED_MINOR,
                 PYGOBJECT_REQUIRED_MICRO, pvalue_char);

      Py_DecRef (ptype);
      Py_DecRef (pvalue);
      Py_DecRef (ptraceback);
      Py_DecRef (pstr);
      PyErr_Clear ();
      Py_Finalize ();

      return TRUE;
    }

  pyg_disable_warning_redirections ();

  /* Generate system path array */
  command = g_string_new ("import sys; sys.path+=[");

  /* GLADE_ENV_MODULE_PATH has priority */
  if ((module_path = g_getenv (GLADE_ENV_MODULE_PATH)))
    {
      tokens = g_strsplit(module_path, ":", -1);
      gint i;

      for (i = 0; tokens[i]; i++)
        g_string_append_printf (command, "'%s', ", tokens[i]);
    }

  /* Append modules directory */
  g_string_append_printf (command, "'%s'", glade_app_get_modules_dir ());
  
  /* Append extra paths (declared in the Preferences) */
  for (paths = glade_catalog_get_extra_paths (); paths; paths = g_list_next (paths))
    g_string_append_printf (command, ", '%s'", (gchar *) paths->data);

  /* Close python statement */
  g_string_append (command, "];\n");

  /* Make sure we load Gtk 3 */
  g_string_append (command, "import gi; gi.require_version('Gtk', '3.0');\n");

  /* Finally run statement in vm */
  PyRun_SimpleString (command->str);

  g_string_free (command, TRUE);
  g_strfreev (tokens);

  return FALSE;
}

void
glade_python_init (const gchar *name)
{
  static gsize init = 0;
  gchar *import_sentence;

  if (g_once_init_enter (&init))
    {
      if (glade_python_setup ())
        return;

      g_once_init_leave (&init, TRUE);
    }

  /* Yeah, we use the catalog name as the library */
  import_sentence = g_strdup_printf ("import %s;", name);

  /* Importing the module will create all the GTypes so that glade can use them at runtime */
  PyRun_SimpleString (import_sentence);
  
  g_free (import_sentence);
}
