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

struct _GladeBindingCtrl {
	gchar *name;   /* the name of the module (ie: python) */
	
	/* Module symbols */
	GladeBindingFinalizeFunc    finalize;
	GladeBindingLibraryLoadFunc library_load;
};

typedef struct _GladeBinding GladeBinding;
struct _GladeBinding {
	GModule  *module; /* The binding module */
	
	GladeBindingCtrl ctrl;
};

void          glade_binding_load_all (void);

void          glade_binding_unload_all (void);

GladeBinding *glade_binding_get (const gchar *name);

const gchar  *glade_binding_get_name (GladeBinding *binding);

GList        *glade_binding_get_all ();

void          glade_binding_library_load (GladeBinding *binding,
					  const gchar *library);

G_END_DECLS

#endif /* __GLADE_BINDING_H__ */
