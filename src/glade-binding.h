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
 
#ifndef __GLADE_BINDING_H__
#define __GLADE_BINDING_H__

G_BEGIN_DECLS

#define GLADE_BINDING_SCRIPT_DIR "scripts"

typedef struct _GladeBindingCtrl GladeBindingCtrl;

/**
 * GladeBindingInitFunc:
 *
 * This function is called after loading the binding plugin.
 *
 * With interpreted language this would initialize the language's interpreter.
 *
 * Returns: whether or not the plugin was successfully initialized.
 *
 */
typedef gboolean (*GladeBindingInitFunc)        (GladeBindingCtrl *ctrl);

/**
 * GladeBindingFinalizeFunc:
 *
 * This function is called after unloading the binding plugin.
 * It should freed everuthing allocated by the init function.
 *
 */
typedef void     (*GladeBindingFinalizeFunc)    (GladeBindingCtrl *ctrl);

/**
 * GladeBindingLibraryLoadFunc:
 *
 * @str: the library name to load.
 *
 * This function is used to load a library written in the language that is 
 * supported by the binding.
 *
 * For example if I had a custom widget library writen in python, this function
 * will provide the core a way to load "glademywidgets" python module.
 *
 * <glade-catalog name="mywidgets" library="glademywidgets" language="python">
 *
 */
typedef void     (*GladeBindingLibraryLoadFunc) (const gchar *str);

/**
 * GladeBindingRunScriptFunc:
 *
 * @path: the script path.
 *
 * Run the script @path.
 * Define this function for the binding to support running scripts.
 *
 */
typedef gint     (*GladeBindingRunScriptFunc)   (const gchar *path,
						 gchar **argv);

/**
 * GladeBindingConsoleNewFunc:
 *
 * Create a new console widget for this binding.
 * In glade3 this widget will be packed in the console window.
 *
 * Returns: A new GtkWidget.
 *
 */
typedef GtkWidget *(*GladeBindingConsoleNewFunc)  (void);

struct _GladeBindingCtrl {
	gchar *name;   /* the name of the module (ie: python) */
	
	/* Module symbols */
	GladeBindingFinalizeFunc    finalize;
	GladeBindingLibraryLoadFunc library_load;
	GladeBindingRunScriptFunc   run_script;
	GladeBindingConsoleNewFunc  console_new;
};

typedef struct _GladeBinding GladeBinding;
struct _GladeBinding {
	GModule  *module; /* The binding module */
	
	GList *scripts;   /* A list of GladeBindingScript */
	
	GData *context_scripts; /* A list of GladeBindingScript separated by GType */
	
	GladeBindingCtrl ctrl;
};

typedef struct _GladeBindingScript GladeBindingScript ;
struct _GladeBindingScript {
	GladeBinding *binding;
	gchar *name, *path;
};

LIBGLADEUI_API
void          glade_binding_load_all (void);

LIBGLADEUI_API
void          glade_binding_unload_all (void);

LIBGLADEUI_API
GladeBinding *glade_binding_get (const gchar *name);

LIBGLADEUI_API
const gchar  *glade_binding_get_name (GladeBinding *binding);

LIBGLADEUI_API
GList        *glade_binding_get_all ();

LIBGLADEUI_API
void          glade_binding_library_load (GladeBinding *binding,
					  const gchar *library);

LIBGLADEUI_API
gint          glade_binding_run_script (GladeBinding *script, 
					const gchar *path,
					gchar **argv);

LIBGLADEUI_API
GtkWidget    *glade_binding_console_new (GladeBinding *binding);

G_END_DECLS

#endif
