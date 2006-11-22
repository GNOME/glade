/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Juan Pablo Ugarte.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <Python.h>
#include <pygobject.h>
#include "glade.h"
#include "glade-binding.h"

static GtkTextBuffer *PythonBuffer = NULL;
static GtkTextMark   *PythonBufferEnd;

static 	PyObject *glade, *glade_dict, *GladeError, *GladeStdout, *GladeStderr;

static PyObject *
glade_python_undo (PyObject *self, PyObject *args)
{
	glade_app_command_undo ();
	
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
glade_python_redo (PyObject *self, PyObject *args)
{
	glade_app_command_redo ();
	
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
glade_python_project_new (PyObject *self, PyObject *args)
{	
	glade_app_add_project (glade_project_new (TRUE));

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
glade_python_project_open (PyObject *self, PyObject *args)
{
	gchar *path;
	
	Py_INCREF(Py_None);
	
	if (PyArg_ParseTuple(args, "s", &path))
	{
		GladeProject *project;
		
		if ((project = glade_app_get_project_by_path (path)))
			glade_app_set_project (project);
		else if ((project = glade_project_open (path)))
			glade_app_add_project (project);
		else
			return Py_None;
	}
	
	return Py_None;
}

static PyObject *
glade_python_project_save (PyObject *self, PyObject *args)
{
	gchar *path;
	
	if (PyArg_ParseTuple(args, "s", &path))
		glade_project_save (glade_app_get_project (), path, NULL);
	
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
glade_python_project_close (PyObject *self, PyObject *args)
{
	gchar *path;
	
	if (PyArg_ParseTuple(args, "s", &path))
	{
		GladeProject *project = glade_app_get_project_by_path (path);
		if (project) glade_app_remove_project (project);
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
glade_python_project_get (PyObject *self, PyObject *args)
{
	GladeProject *project = glade_app_get_project ();
	return Py_BuildValue ("s", (project) ? project->name : "");
}

static PyObject *
glade_python_project_list (PyObject *self, PyObject *args)
{
	GList *p, *projects = glade_app_get_projects ();
	PyObject *list;
	
	list = PyList_New (0);
	
	for (p = projects; p && p->data; p = g_list_next (p))
	{
		GladeProject *project = p->data;
		PyList_Append (list, Py_BuildValue ("s", project->name));
	}
	
	return list;
}

static PyObject *
glade_python_project_set (PyObject *self, PyObject *args)
{
	gchar *path;
	
	if (PyArg_ParseTuple(args, "s", &path))
	{
		GladeProject *project = glade_app_get_project_by_path (path);
		if (project) glade_app_set_project (project);
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
glade_python_widget_new (PyObject *self, PyObject *args)
{
	GladeProject *project = glade_app_get_project ();
	GladeWidgetAdaptor *adaptor; 
	GladeWidget *widget = NULL;
	gchar *class_name, *parent;

	if (project && PyArg_ParseTuple(args, "ss", &class_name, &parent) &&
	    (adaptor = glade_widget_adaptor_get_by_name (class_name)))
		widget = glade_command_create (adaptor,
				glade_project_get_widget_by_name (project, parent),
				NULL, project);
	
	return Py_BuildValue ("s", (widget) ? widget->name : "");
}

static PyObject *
glade_python_widget_delete (PyObject *self, PyObject *args)
{
	GladeProject *project = glade_app_get_project ();
	gchar *name;
	
	if (project && PyArg_ParseTuple(args, "s", &name))
	{
		GladeWidget *widget;
		if ((widget = glade_project_get_widget_by_name (project, name)))
		{
			GList list;
			list.data = widget;
			list.next = list.prev = NULL;
			glade_command_delete (&list);
		}
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
glade_python_widget_set (PyObject *self, PyObject *args)
{
	GladeProject *project = glade_app_get_project ();
	gchar *name, *id_property;
	PyObject *val;
	
	if (project && PyArg_ParseTuple(args, "ssO", &name, &id_property, &val))
	{
		GladeWidget *widget;
		GladeProperty *property;

		if ((widget = glade_project_get_widget_by_name (project, name)) &&
		    (property = glade_widget_get_property (widget, id_property)))
		{
			GValue value = {0,};
			g_value_init (&value, G_VALUE_TYPE (property->value));
			if (pyg_value_from_pyobject (&value, val) == 0)
				glade_command_set_property_value (property, &value);
		}
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
glade_python_widget_get (PyObject *self, PyObject *args)
{
	GladeProject *project = glade_app_get_project ();
	gchar *name, *id_property;
	
	if (project && PyArg_ParseTuple(args, "ss", &name, &id_property))
	{
		GladeWidget *widget;
		GladeProperty *property;

		if ((widget = glade_project_get_widget_by_name (project, name)) &&
		    (property = glade_widget_get_property (widget, id_property)))
			return pyg_value_as_pyobject (property->value, TRUE);
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
glade_python_widget_list (PyObject *self, PyObject *args)
{
	GList *p;
	GladeProject *project = glade_app_get_project ();
	PyObject *list;
	
	list = PyList_New (0);
	
	for (p = project->objects; p && p->data; p = g_list_next (p))
	{
		GladeWidget *widget = glade_widget_get_from_gobject (p->data);
		if (widget)
			PyList_Append (list, Py_BuildValue ("s", glade_widget_get_name (widget)));
	}
	
	return list;
}

extern PyTypeObject PyGladeWidgetAdaptor_Type;

static GHashTable *registered_classes = NULL;

static PyTypeObject *
glade_python_register_class (GType type)
{
	GType parent = g_type_parent (type);
	PyTypeObject *class, *parent_class;
	
	if (parent == 0 || type == GLADE_TYPE_WIDGET_ADAPTOR)
		return &PyGladeWidgetAdaptor_Type;
	
	if (g_hash_table_lookup (registered_classes, GUINT_TO_POINTER (parent)) == NULL)
		parent_class = glade_python_register_class (parent);
	else
		parent_class = pygobject_lookup_class (parent);
	
	class = pygobject_lookup_class (type);
	
	pygobject_register_class (glade_dict, g_type_name (type), type, class,
				  Py_BuildValue("(O)", parent_class));
	pyg_set_object_has_new_constructor (type);

	g_hash_table_insert (registered_classes, GUINT_TO_POINTER (type), class);
	
	return class;
}

static PyObject *
glade_python_get_adaptor_for_type (PyObject *self, PyObject *args)
{
	GladeWidgetAdaptor *adaptor;
	PyObject *class;
	gchar *name;

	if (PyArg_ParseTuple(args, "s", &name) &&
	    (adaptor = glade_widget_adaptor_get_by_name (name)))
	{
		GType type = G_TYPE_FROM_INSTANCE (adaptor);

		if ((class = g_hash_table_lookup (registered_classes, GUINT_TO_POINTER (type))))
			return (PyObject *) class;
		else
			return (PyObject *) glade_python_register_class (type);
	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef GladeMethods[] = {
	{"undo",  glade_python_undo, METH_VARARGS, "Execute undo command."},
	{"redo",  glade_python_redo, METH_VARARGS, "Execute redo command."},
	{"project_new",  glade_python_project_new, METH_VARARGS, "Create a new project."},
	{"project_open",  glade_python_project_open, METH_VARARGS, "Open an existing project."},
	{"project_save",  glade_python_project_save, METH_VARARGS, "Save the current project."},
	{"project_close",  glade_python_project_close, METH_VARARGS, "Close the current project."},
	{"project_get",  glade_python_project_get, METH_VARARGS, "Get the current project."},
	{"project_set",  glade_python_project_set, METH_VARARGS, "Set the current project."},
	{"project_list",  glade_python_project_list, METH_VARARGS, "List all projects."},
	{"widget_new",  glade_python_widget_new, METH_VARARGS, "Create a new GtkWidget."},
	{"widget_delete",  glade_python_widget_delete, METH_VARARGS, "Delete a GtkWidget."},
	{"widget_set",  glade_python_widget_set, METH_VARARGS, "Set a GtkWidget's property."},
	{"widget_get",  glade_python_widget_get, METH_VARARGS, "Get a GtkWidget's property."},
	{"widget_list",  glade_python_widget_list, METH_VARARGS, "List all widgets."},
	{"get_adaptor_for_type",  glade_python_get_adaptor_for_type, METH_VARARGS, "Get the corresponding GladeWidgetAdaptor. Use this function to create your own derived Adaptor."},
	{NULL, NULL, 0, NULL}
};

/* GladeStdout GladeStderr write method */
static PyObject *
glade_python_std_write (PyObject *self, PyObject *args, gboolean stdout)
{
	gchar *string;
	
	if (PythonBuffer && PyArg_ParseTuple(args, "s", &string))
	{
		GtkTextIter iter;
		gtk_text_buffer_get_end_iter (PythonBuffer, &iter);
		if (stdout)
			gtk_text_buffer_insert (PythonBuffer, &iter, string, -1);
		else
			gtk_text_buffer_insert_with_tags_by_name (PythonBuffer, 
				&iter, string, -1, "red_foreground", NULL);
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
glade_python_stdout_write (PyObject *self, PyObject *args)
{
	return glade_python_std_write (self, args, TRUE);
}
static PyObject *
glade_python_stderr_write (PyObject *self, PyObject *args)
{
	return glade_python_std_write (self, args, FALSE);
}

static PyMethodDef GladeStdoutMethods[] = {
    {"write",  glade_python_stdout_write, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}
};

static PyMethodDef GladeStderrMethods[] = {
    {"write",  glade_python_stderr_write, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}
};

/* Bindings */
void
glade_python_binding_finalize (GladeBindingCtrl *ctrl)
{
	if (PythonBuffer)
	{
		Py_Finalize ();
		g_object_unref (PythonBuffer);
	}
}

void 
glade_python_binding_library_load (const gchar *library)
{
	gchar *str = g_strdup_printf ("import %s;", library);
	PyRun_SimpleString (str);
	g_free (str);
}

gint
glade_python_binding_run_script (const gchar *path, gchar **argv)
{
	FILE *fp = fopen (path, "r");
	gint retval, i;
	
	if (fp == NULL) return -1;
	
	PyRun_SimpleString ("sys.argv = [];");
	
	if (argv)
	{
		GString *string = g_string_new ("");
		
		for (i = 0; argv[i]; i++)
		{
			g_string_printf (string, "sys.argv += ['%s'];", argv[i]);
			PyRun_SimpleString (string->str);
		}
		
		g_string_free (string, TRUE);
	}
	
	retval = PyRun_SimpleFile (fp, path);
	fclose (fp);
	
	return retval;
}

static void
glade_python_console_entry_activate (GtkEntry *entry, GtkTextView *textview)
{
	const gchar *command = gtk_entry_get_text (entry);
	GtkTextIter iter;
	
	gtk_text_buffer_get_end_iter (PythonBuffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (PythonBuffer, &iter, ">>> ", -1,
						  "blue_foreground", NULL);
	gtk_text_buffer_get_end_iter (PythonBuffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (PythonBuffer, &iter, command, -1,
						  "blue_foreground", NULL);
	gtk_text_buffer_get_end_iter (PythonBuffer, &iter);
	gtk_text_buffer_insert (PythonBuffer, &iter, "\n", -1);
	
	if (PyRun_SimpleString (command) == 0) gtk_entry_set_text (entry, "");
}

static void
glade_python_buffer_changed (GtkTextBuffer *buffer, GtkTextView *textview)
{
	gtk_text_view_scroll_to_mark (textview, PythonBufferEnd, 0.0, TRUE, 0.0, 1.0);
}

static void 
glade_python_textview_destroy (GtkObject *object, gpointer data)
{
	g_signal_handler_disconnect (PythonBuffer, GPOINTER_TO_UINT (data));
}

GtkWidget *
glade_python_binding_console_new (void)
{
	GtkWidget *vbox, *hbox, *textview, *label, *entry, *sw;
	gulong handler_id;
	
	vbox = gtk_vbox_new (FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (vbox),
					GLADE_GENERIC_BORDER_WIDTH);
	
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
					     GTK_SHADOW_ETCHED_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	
	textview = gtk_text_view_new_with_buffer (PythonBuffer);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (textview), FALSE);
	gtk_container_add (GTK_CONTAINER (sw), textview);
	handler_id = g_signal_connect (PythonBuffer, "changed",
				       G_CALLBACK (glade_python_buffer_changed),
				       GTK_TEXT_VIEW (textview));
	g_signal_connect (textview, "destroy",
			  G_CALLBACK (glade_python_textview_destroy),
			  GUINT_TO_POINTER (handler_id));

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	
	label = gtk_label_new (">>>");
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	
	entry = gtk_entry_new ();
	g_signal_connect (entry, "activate", 
			  G_CALLBACK (glade_python_console_entry_activate),
			  GTK_TEXT_VIEW (textview));
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
	
	gtk_widget_show (textview);
	gtk_widget_show (label);
	gtk_widget_show (entry);
	gtk_widget_show (hbox);
	gtk_widget_show (sw);

	return vbox;
}

/*
  Generated by pygtk-codegen-2.0 
  pygtk-codegen-2.0 -p glade_python_gwa -o glade-python-gwa.override glade-python-gwa.defs > glade-python-gwa.c
*/
void glade_python_gwa_register_classes (PyObject *d);

static void
glade_python_init (void)
{
	char *argv[2] = {"", NULL};

	/* Init interpreter */
	Py_Initialize ();
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

gboolean
glade_binding_init (GladeBindingCtrl *ctrl)
{
	GtkTextIter iter;
	gchar *command;
	
	if (PythonBuffer == NULL)
	{	
		PythonBuffer = gtk_text_buffer_new (NULL);
		gtk_text_buffer_create_tag (PythonBuffer, "blue_foreground",
					    "foreground", "blue", NULL);
		gtk_text_buffer_create_tag (PythonBuffer, "red_foreground",
					    "foreground", "red", NULL);
		gtk_text_buffer_get_end_iter (PythonBuffer, &iter);
		PythonBufferEnd = gtk_text_buffer_create_mark (PythonBuffer, NULL, &iter, FALSE);
	}
	else return FALSE;

    	Py_SetProgramName (PACKAGE_NAME);
	
    	/* Initialize the Python interpreter */
	glade_python_init ();
	
	/* Check and init pygobject >= 2.12.0 */
	PyErr_Clear ();	
	glade_python_init_pygtk_check (PYGTK_REQ_MAYOR, PYGTK_REQ_MINOR, PYGTK_REQ_MICRO);
	if (PyErr_Occurred ())
	{
		g_warning ("Unable to load pygobject module >= %d.%d.%d, "
			   "please make sure it is in python's path (sys.path). "
			   "(use PYTHONPATH env variable to specify non default paths)",
			   PYGTK_REQ_MAYOR, PYGTK_REQ_MINOR, PYGTK_REQ_MICRO);
		PyErr_Clear ();
		Py_Finalize ();
		g_object_unref (PythonBuffer);
		return FALSE;
	}
	
	pyg_disable_warning_redirections ();
	
	/* Create glade object */
	glade = Py_InitModule ("glade", GladeMethods);
	GladeError = PyErr_NewException ("glade.error", NULL, NULL);
	Py_INCREF (GladeError);
	PyModule_AddObject (glade, "error", GladeError);

	/* Create GladeStdout and GladeStderr objects */
	GladeStdout = Py_InitModule ("stdout", GladeStdoutMethods);
	PySys_SetObject("stdout", GladeStdout);

	GladeStderr = Py_InitModule ("stderr", GladeStderrMethods);
	PySys_SetObject("stderr", GladeStderr);
	
	/* Register GladeUI classes (GladeWidgetAdaptor) */
	glade_dict = PyModule_GetDict (glade);
	glade_python_gwa_register_classes (glade_dict);
	
	/* Create registered_classes hash table */
	registered_classes = g_hash_table_new (g_direct_hash, g_direct_equal);

	/* Insert GladeWidgetAdaptor class in the hash table */
	g_hash_table_insert (registered_classes,
			     GUINT_TO_POINTER (GLADE_TYPE_WIDGET_ADAPTOR),
			     &PyGladeWidgetAdaptor_Type);

	/* Import glade module and set path */
	command = g_strdup_printf ("import sys; import glade; sys.path+=['.', '%s'];\n",
				    glade_modules_dir);
	PyRun_SimpleString (command);
	g_free (command);

	gtk_text_buffer_get_end_iter (PythonBuffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name (PythonBuffer, &iter,
					">>> import sys;\n>>> import glade;\n",
					-1, "blue_foreground", NULL);
	
	/* Setup ctrl members */
	ctrl->name = "python";
	ctrl->finalize = glade_python_binding_finalize;
	ctrl->library_load = glade_python_binding_library_load;
	ctrl->run_script = glade_python_binding_run_script;
	ctrl->console_new = glade_python_binding_console_new;
	
	return TRUE;
}
