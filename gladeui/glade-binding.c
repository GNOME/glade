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

*/

static GHashTable *bindings = NULL;

/**
 * glade_binding_load_all:
 *
 * Loads and initialize every binding plugin.
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
		g_error_free (error);
		return;
	}
	
	if (bindings == NULL)
		bindings = g_hash_table_new (g_str_hash, g_str_equal);
	
	while ((filename = g_dir_read_name (dir)))
	{
		GladeBindingInitFunc init;
		GladeBinding *binding;
		GModule *module;
		gchar *path;
		
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
		g_free (path);
	}
}

static void 
glade_binding_remove (gpointer key, gpointer value, gpointer user_data)
{
	GladeBinding *binding = value;
	
	if (binding->ctrl.finalize) binding->ctrl.finalize (&binding->ctrl);

	g_module_close (binding->module);
	
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
 * Return a newly allocated list of every loaded binding.
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
