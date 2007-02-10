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

#include <glib/gi18n-lib.h>

#include "glade.h"
#include "glade-binding.h"
#include <string.h>

/*
GladeBindings are glade's modules to add support for others languages.

The principal objective is to be able to support widgets written in other
language than C.

The bindings modules provides this functionality to the core trought a couple
of obligatory functions.

GladeBindingInitFunc glade_binding_init() to initialize the binding and
GladeBindingLibraryLoadFunc to load a library written in language supported
by the binding.

You can add Scripting capability to the core by providing 
GladeBindingRunScriptFunc and/or GladeBindingConsoleNewFunc

The core will search for script in two different directories,
datadir/package/scripts/ and g_get_user_config_dir()/package/scripts/

The hierarchy in those directories should be binding_name/gtype_name/ *
For example if you want to add a python script to GtkContainers you can do it by
installing the script file in 
g_get_user_config_dir()/glade3/scripts/python/GtkContainer/Delete_Children.py
This will add a context menu item for any GtkContainer called "Delete Children"
that will trigger GtkContainer's widget adaptor "action-activated::Delete_Children" signal.
*/

static GHashTable *bindings = NULL;

static GList*
glade_binding_script_add (GList *list, GladeBinding *binding, gchar *path)
{
	GladeBindingScript *script = g_new0 (GladeBindingScript, 1);
	gchar *tmp;
	
	script->binding = binding;
	script->path = path;
	script->name = g_path_get_basename (path);
	if ((tmp = strrchr (script->name, '.'))) *tmp = '\0';
	return g_list_prepend (list, script);
}

static GList *
glade_binding_get_script_list (GladeBinding *binding, gchar *directory)
{
	const gchar *filename;
	GList *list = NULL;
	GDir *dir;

	if ((dir = g_dir_open (directory, 0, NULL)) == NULL) 
		return NULL;

	while ((filename = g_dir_read_name (dir)))
	{
		gchar *path = g_build_filename (directory, filename, NULL);
		
		if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
			list = glade_binding_script_add (list, binding, path);
		else
			g_free (path);
	}
	
	return list;
}

static void
glade_binding_script_free (gpointer data, gpointer user_data)
{
	GladeBindingScript *script = data;
	if (script->path) g_free (script->path);
	if (script->name) g_free (script->name);
	g_free (script);
}

static void 
glade_binding_classes_destroy (gpointer value)
{
	g_list_foreach (value, glade_binding_script_free, NULL);
	g_list_free (value);
}

static void
glade_binding_script_load (GladeBinding *binding, gchar *rootdir)
{
	const gchar *filename;
	GDir *dir;
	
	if ((dir = g_dir_open (rootdir, 0, NULL)) == NULL) 
		return;

	while ((filename = g_dir_read_name (dir)))
	{
		gchar *key, *path = g_build_filename (rootdir, filename, NULL);

		if (g_file_test (path, G_FILE_TEST_IS_DIR))
		{
			GList *list, *old_list;

			list = glade_binding_get_script_list (binding, path);
			if (g_hash_table_lookup_extended (binding->context_scripts,
							  filename,	
							  (gpointer)&key, 
							  (gpointer)&old_list))
			{
				g_hash_table_steal (binding->context_scripts, key);
				g_free (key);
				list = g_list_concat (old_list, list);
			}

			g_hash_table_insert (binding->context_scripts,
					     g_strdup (filename), list);
		}
		else if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
			binding->scripts = glade_binding_script_add (binding->scripts, binding, path);
		else
			g_free (path);
	}
}

/**
 * glade_binding_load_all:
 *
 * Loads and initialize every binding plugin.
 *
 * Loads scripts for bindings that have GladeBindingRunScriptFunc implemented.
 *
 * This function will search for script in two different directories,
 * datadir/package/scripts/ and g_get_user_config_dir()/package/scripts/
 * 
 * The hierarchy in those directories should be binding_name/gtype_name/ *
 * For example if you want to add a python script to GtkContainers you can do it
 * by installing the script file in g_get_user_config_dir()/glade3/scripts/python/GtkContainer/Delete_Children.py
 * This will add a context menu item for any GtkContainer called "Delete Children"
 * that will trigger GtkContainer's widget adaptor "action-activated::Delete_Children" signal.
 * This signal is proxied by GladeWidget.
 *
 */
void
glade_binding_load_all (void)
{
	const gchar *filename;
	GError *error = NULL;
	GDir *dir;

	/* Read all files in bindings dir */
	if ((dir = g_dir_open (glade_app_get_bindings_dir (), 0, &error)) == NULL) 
	{
		g_warning ("Failed to open bindings directory: %s", error->message);
		return;
	}
	
	if (bindings == NULL)
		bindings = g_hash_table_new (g_str_hash, g_str_equal);
	
	while ((filename = g_dir_read_name (dir)))
	{
		GladeBindingInitFunc init;
		GladeBinding *binding;
		GModule *module;
		gchar *path, *rootdir;
		
		if (g_str_has_suffix (filename, G_MODULE_SUFFIX) == FALSE)
			continue;
		
		path = g_build_filename (glade_app_get_bindings_dir (), filename, NULL);
		module = g_module_open (path, G_MODULE_BIND_LAZY);
		
		if (module == NULL) continue;
	
		binding = g_new0 (GladeBinding, 1);
		binding->module = module;
		
		if ((g_module_symbol (module, "glade_binding_init", (gpointer)&init) &&
		    init (&binding->ctrl) && binding->ctrl.name) == FALSE)
		{
			g_warning ("Unable to load GladeBinding module '%s'", path);
			g_module_close (module);
			g_free (binding);
			g_free (path);
			continue;
		}
			
		g_hash_table_insert (bindings, binding->ctrl.name, binding);
		
		/* Load Scripts */
		binding->context_scripts = g_hash_table_new_full (g_str_hash,
								  g_str_equal,
								  g_free,
								  glade_binding_classes_destroy);

		/* datadir/package/scripts/ */
		rootdir = g_build_filename (glade_app_get_scripts_dir (), binding->ctrl.name, NULL);
		glade_binding_script_load (binding, rootdir);
		g_free (rootdir);
	
		/* g_get_user_config_dir()/package/scripts/ */
		rootdir = g_build_filename (g_get_user_config_dir (),
					    PACKAGE_NAME, GLADE_BINDING_SCRIPT_DIR,
					    binding->ctrl.name, NULL);
		glade_binding_script_load (binding, rootdir);
		g_free (rootdir);
		g_free (path);
	}
}

static void 
glade_binding_remove (gpointer key, gpointer value, gpointer user_data)
{
	GladeBinding *binding = value;
	
	if (binding->ctrl.finalize) binding->ctrl.finalize (&binding->ctrl);

	g_module_close (binding->module);
	
	if (binding->scripts)
		glade_binding_classes_destroy (binding->scripts);
	
	if (binding->context_scripts)
		g_hash_table_remove_all (binding->context_scripts);
	
	g_free (binding);
}

/**
 * glade_binding_unload_all:
 *
 * Finalize and unload every binding plugin.
 *
 */
void
glade_binding_unload_all (void)
{
	if (bindings == NULL) return;
		
	g_hash_table_foreach (bindings, glade_binding_remove, NULL);
	g_hash_table_destroy (bindings);
	bindings = NULL;
}

/**
 * glade_binding_get:
 *
 * @name: the binding name (ie: python)
 *
 * Returns the loaded binding.
 *
 */
GladeBinding *
glade_binding_get (const gchar *name)
{
	g_return_val_if_fail (name != NULL, NULL);
	return g_hash_table_lookup (bindings, name);	
}

/**
 * glade_binding_get_name:
 *
 * @binding: the binding
 *
 * Returns the binding name.
 *
 */
const gchar *
glade_binding_get_name (GladeBinding *binding)
{
	g_return_val_if_fail (binding != NULL, NULL);
	return binding->ctrl.name;
}

static void
glade_binding_get_all_foreach (gpointer key, gpointer value, gpointer user_data)
{
	GList **l = user_data;
	*l = g_list_prepend (*l, value);
}

/**
 * glade_binding_get_all:
 *
 * Return a newly alocated list of every laoded binding.
 *
 */
GList *
glade_binding_get_all (void)
{
	GList *l = NULL;

	if (bindings)
		g_hash_table_foreach (bindings, glade_binding_get_all_foreach, &l);
	
	return g_list_reverse (l);
}

/**
 * glade_binding_library_load:
 *
 * @binding: a GladeBinding.
 * @library: the library name.
 *
 * Loads the library name.
 *
 */
void
glade_binding_library_load (GladeBinding *binding, const gchar *library)
{
	g_return_if_fail (binding != NULL && library != NULL);
	if (binding->ctrl.library_load)
		binding->ctrl.library_load (library);
}

/**
 * glade_binding_run_script:
 *
 * @binding: a GladeBinding.
 * @path: the library init function name.
 *
 * Run a script file.
 *
 */
gint
glade_binding_run_script (GladeBinding *binding,
			  const gchar *path,
			  gchar **argv)
{
	g_return_val_if_fail (binding != NULL && path != NULL, -1);
	if (binding->ctrl.run_script)
		return binding->ctrl.run_script (path, argv);
	else
		return -1;
}
