/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2004 Imendio AB
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
 *   Chema Celorio <chema@celorio.com>
 */


#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

#include "glade.h"
#include "glade-catalog.h"
#include "glade-widget-class.h"

typedef void   (*GladeCatalogInitFunc) (void);

struct _GladeCatalog
{
	gchar *library;          /* Library name for backend support  */

	gchar *name;             /* Symbolic catalog name             */

	gchar *dep_catalog;      /* Symbolic name of the catalog that
				  * this catalog depends on           */

	gchar *domain;           /* The domain to be used to translate
				  * strings from this catalog (otherwise this 
				  * defaults to the library name)
				  */
	
	gchar *book;             /* Devhelp search domain
				  */

	GList *widget_groups;    /* List of widget groups (palette)   */
	GList *widget_classes;   /* List of widget classes (all)      */

	GladeXmlContext *context;/* Xml context is stored after open
				  * before classes are loaded         */
	
	GModule *module;
	
	gchar *init_function_name;/* Catalog's init function name */
	GladeCatalogInitFunc init_function;
};

struct _GladeWidgetGroup
{
	gchar *name;             /* Group name                        */
	gchar *title;            /* Group name in the palette             */

	gboolean expanded;       /* Whether group is expanded in the palette */

	GList *widget_classes;   /* List of classes in the palette    */
};

static void            catalog_load         (GladeCatalog     *catalog);
static GladeCatalog   *catalog_open         (const gchar      *filename);
static GList          *catalog_sort         (GList            *catalogs);
static gboolean        catalog_load_classes (GladeCatalog     *catalog,
					     GladeXmlNode     *widgets_node);
static gboolean        catalog_load_group   (GladeCatalog     *catalog,
					     GladeXmlNode     *group_node);

static void            widget_group_free    (GladeWidgetGroup *group);

/* List of catalog names successfully loaded.
 */
static GList *loaded_catalogs = NULL;

static gboolean
catalog_get_function (GladeCatalog *catalog, 
		      const gchar *symbol_name,
		      gpointer *symbol_ptr)
{
	if (catalog->module == NULL)
		catalog->module = glade_util_load_library (catalog->library);
	
	if (catalog->module)
		return g_module_symbol (catalog->module, symbol_name, symbol_ptr);

	return FALSE;
}

static GladeCatalog *
catalog_open (const gchar *filename)
{
	GladeCatalog    *catalog;
	GladeXmlContext *context;
	GladeXmlDoc     *doc;
	GladeXmlNode    *root;

	/* get the context & root node of the catalog file */
	context = glade_xml_context_new_from_path (filename,
						   NULL, 
						   GLADE_TAG_GLADE_CATALOG);
	if (!context) 
	{
		g_warning ("Couldn't open catalog [%s].", filename);
		return NULL;
	}

	doc  = glade_xml_context_get_doc (context);
	root = glade_xml_doc_get_root (doc);

	if (!glade_xml_node_verify (root, GLADE_TAG_GLADE_CATALOG)) 
	{
		g_warning ("Catalog root node is not '%s', skipping %s",
			   GLADE_TAG_GLADE_CATALOG, filename);
		glade_xml_context_free (context);
		return NULL;
	}

	catalog = g_new0 (GladeCatalog, 1);
	catalog->context = context;
	catalog->name    = glade_xml_get_property_string (root, GLADE_TAG_NAME);
	loaded_catalogs = g_list_prepend (loaded_catalogs, g_strdup (catalog->name));

	if (!catalog->name) 
	{
		g_warning ("Couldn't find required property 'name' in catalog root node");
		g_free (catalog);
		glade_xml_context_free (context);
		return NULL;
	}
	
	catalog->library =
		glade_xml_get_property_string (root, GLADE_TAG_LIBRARY);
	catalog->dep_catalog =
		glade_xml_get_property_string (root, GLADE_TAG_DEPENDS);
	catalog->domain =
		glade_xml_get_property_string (root, GLADE_TAG_DOMAIN);
	catalog->book =
		glade_xml_get_property_string (root, GLADE_TAG_BOOK);
	catalog->init_function_name =
		glade_xml_get_value_string (root, GLADE_TAG_INIT_FUNCTION);
	
	if (catalog->init_function_name)
		catalog_get_function (catalog, catalog->init_function_name,
				      (gpointer) &catalog->init_function);

	return catalog;
}


static void
catalog_load (GladeCatalog *catalog)
{
	GladeXmlDoc     *doc;
	GladeXmlNode    *root;
	GladeXmlNode    *node;

	g_return_if_fail (catalog->context != NULL);
	
	doc  = glade_xml_context_get_doc (catalog->context);
	root = glade_xml_doc_get_root (doc);
	node = glade_xml_node_get_children (root);

	for (; node; node = glade_xml_node_next (node))
	{
		const gchar *node_name;

		node_name = glade_xml_node_get_name (node);
		if (strcmp (node_name, GLADE_TAG_GLADE_WIDGET_CLASSES) == 0) 
		{
			catalog_load_classes (catalog, node);
		}
		else if (strcmp (node_name, GLADE_TAG_GLADE_WIDGET_GROUP) == 0)
		{
			catalog_load_group (catalog, node);
		}
		else 
			continue;
	}

	catalog->widget_groups = g_list_reverse (catalog->widget_groups);
	catalog->context =
		(glade_xml_context_free (catalog->context), NULL);

	return;
}

static gint
catalog_find_by_name (GladeCatalog *catalog, const gchar *name)
{
	return strcmp (catalog->name, name);
}

static GList *
catalog_sort (GList *catalogs)
{
	GList        *l, *node, *sorted = NULL, *sort;
	GladeCatalog *catalog, *cat;

	/* Add all dependant catalogs to the sorted list first */
	for (l = catalogs; l; l = l->next)
	{
		catalog = l->data;
		sort    = NULL;

		/* itterate ascending through dependancy hierarchy */
		while (catalog->dep_catalog) {
			node = g_list_find_custom
				(catalogs, catalog->dep_catalog,
				 (GCompareFunc)catalog_find_by_name);

			if ((cat = node->data) == NULL)
			{
				g_critical ("Catalog %s depends on catalog %s, not found",
					    catalog->name, catalog->dep_catalog);
				break;
			}			

			/* Prepend to sort list */
			if (g_list_find (sort, cat) == NULL &&
			    g_list_find (sorted, cat) == NULL)
				sort = g_list_prepend (sort, cat);

			catalog = cat;
		}
		sorted = g_list_concat (sorted, sort);
	}
	
	/* Append all independant catalogs after */
	for (l = catalogs; l; l = l->next)
		if (g_list_find (sorted, l->data) == NULL)
			sorted = g_list_append (sorted, l->data);

	g_list_free (catalogs);
	return sorted;
}


static gboolean
catalog_load_classes (GladeCatalog *catalog, GladeXmlNode *widgets_node)
{
	GladeXmlNode *node;

	node = glade_xml_node_get_children (widgets_node);
	for (; node; node = glade_xml_node_next (node)) 
	{
		const gchar      *node_name;
		GladeWidgetClass *widget_class;

		node_name = glade_xml_node_get_name (node);
		if (strcmp (node_name, GLADE_TAG_GLADE_WIDGET_CLASS) != 0) 
			continue;
	
		widget_class = glade_widget_class_new 
			(node, catalog->name, catalog->library, 
			 catalog->domain ? catalog->domain : catalog->library,
			 catalog->book);

		catalog->widget_classes = g_list_prepend (catalog->widget_classes,
							  widget_class);
	}

	return TRUE;
}

static gboolean
catalog_load_group (GladeCatalog *catalog, GladeXmlNode *group_node)
{
	GladeWidgetGroup *group;
	GladeXmlNode     *node;

	group = g_new0 (GladeWidgetGroup, 1);
	
	group->name = glade_xml_get_property_string (group_node, 
						     GLADE_TAG_NAME);

	if (!group->name) 
	{ 
		g_warning ("Required property 'name' not found in group node");
	
		widget_group_free (group);
		return FALSE;
	}
	
	group->title = glade_xml_get_property_string (group_node,
						      GLADE_TAG_TITLE);
	if (!group->title) 
	{ 
		g_warning ("Required property 'title' not found in group node");

		widget_group_free (group);
		return FALSE;	
	}

	/* by default, group is expanded in palette  */
	group->expanded = TRUE;

	/* Translate it */
	group->title = dgettext (catalog->domain ? 
				 catalog->domain : catalog->library, 
				 group->title);

	group->widget_classes = NULL;

	node = glade_xml_node_get_children (group_node);
	for (; node; node = glade_xml_node_next (node)) 
	{
		const gchar      *node_name;
		GladeWidgetClass *widget_class;
		gchar            *name;

		node_name = glade_xml_node_get_name (node);
		
		if (strcmp (node_name, GLADE_TAG_GLADE_WIDGET_CLASS_REF) == 0)
		{
			name = glade_xml_get_property_string (node, GLADE_TAG_NAME);
			if (!name)
			{
				g_warning ("Couldn't find required property on %s",
					   GLADE_TAG_GLADE_WIDGET_CLASS);
				continue;
			}

			widget_class = glade_widget_class_get_by_name (name);
			if (!widget_class) 
			{
				g_warning ("Tried to include undefined widget class '%s' in a widget group", name);
				g_free (name);
				continue;
			}
			g_free (name);

			group->widget_classes = g_list_prepend (group->widget_classes,
								widget_class);

		}
		else if (strcmp (node_name, GLADE_TAG_DEFAULT_PALETTE_STATE) == 0)
		{
			group->expanded = 
				glade_xml_get_property_boolean 
				(node, GLADE_TAG_EXPANDED, group->expanded);
		}
	}

	group->widget_classes = g_list_reverse (group->widget_classes);

	catalog->widget_groups = g_list_prepend (catalog->widget_groups,
						 group);

	return TRUE;
}

static void
widget_group_free (GladeWidgetGroup *group)
{
	g_return_if_fail (group != NULL);
	
	g_free (group->name);
	g_free (group->title);

	/* The actual widget classes will be free elsewhere */
	g_list_free (group->widget_classes);
}
	
GList *
glade_catalog_load_all (void)
{
	GDir         *dir;
	GError       *error = NULL;
	const gchar  *filename;
	GList        *catalogs, *l;
	GladeCatalog *catalog;
	
	/* Read all files in catalog dir */
	dir = g_dir_open (glade_catalogs_dir, 0, &error);
	if (!dir) 
	{
		g_warning ("Failed to open catalog directory: %s",
			   error->message);
		return NULL;
	}

	/* Catalogs need dependancies, most catalogs depend on
	 * the gtk+ catalog, but some custom toolkits may depend
	 * on the gnome catalog for instance.
	 */
	catalogs = NULL;
	while ((filename = g_dir_read_name (dir)))
	{
		gchar *catalog_filename;

		if (!g_str_has_suffix (filename, ".xml")) 
			continue;

		catalog_filename = g_build_filename (glade_catalogs_dir,
						     filename, NULL);
		catalog = catalog_open (catalog_filename);
		g_free (catalog_filename);

		if (catalog) 
			catalogs = g_list_prepend (catalogs, catalog);
		else
			g_warning ("Unable to open the catalog file %s.\n", 
				   filename);
	}

	/* After sorting, execute init function and then load */
	catalogs = catalog_sort (catalogs);
	
	for (l = catalogs; l; l = l->next)
	{
		catalog = l->data;
		if (catalog->init_function) catalog->init_function ();
	}
	
	for (l = catalogs; l; l = l->next)
	{
		catalog = l->data;
		catalog_load (catalog);
	}
	
	g_dir_close (dir);
	return catalogs;
}

const gchar *
glade_catalog_get_name (GladeCatalog *catalog)
{
	g_return_val_if_fail (catalog != NULL, NULL);

	return catalog->name;
}

GList *
glade_catalog_get_widget_groups (GladeCatalog *catalog)
{
	g_return_val_if_fail (catalog != NULL, NULL);

	return catalog->widget_groups;	
}

GList *
glade_catalog_get_widget_classes (GladeCatalog *catalog)
{
	g_return_val_if_fail (catalog != NULL, NULL);

	return catalog->widget_classes;	
}

void
glade_catalog_free (GladeCatalog *catalog)
{
	GList *list;

	if (catalog == NULL)
		return;

	g_free (catalog->name);
	if (catalog->book)
		g_free (catalog->book);
	
	for (list = catalog->widget_classes; list; list = list->next)
		glade_widget_class_free (GLADE_WIDGET_CLASS (list->data));
	
	g_list_free (catalog->widget_classes);

	for (list = catalog->widget_groups; list; list = list->next) 
		widget_group_free (GLADE_WIDGET_GROUP (list->data));
				   
	g_list_free (catalog->widget_groups);
				   
	
	g_free (catalog);
}
const gchar *
glade_widget_group_get_name (GladeWidgetGroup *group)
{
	g_return_val_if_fail (group != NULL, NULL);

	return group->name;
}

const gchar *
glade_widget_group_get_title (GladeWidgetGroup *group)
{
	g_return_val_if_fail (group != NULL, NULL);

	return group->title;
}

gboolean
glade_widget_group_get_expanded (GladeWidgetGroup *group)
{
	g_return_val_if_fail (group != NULL, FALSE);

	return group->expanded;
}

GList *
glade_widget_group_get_widget_classes (GladeWidgetGroup *group)
{
	g_return_val_if_fail (group != NULL, NULL);

	return group->widget_classes;
}

gboolean
glade_catalog_is_loaded (const gchar *name)
{
	GList *l;
	g_return_val_if_fail (name != NULL, FALSE);
	for (l = loaded_catalogs; l; l = l->next)
		if (!strcmp (name, (gchar *)l->data))
			return TRUE;
	return FALSE;
}
