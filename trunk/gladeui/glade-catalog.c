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

#include <config.h>

#include "glade.h"
#include "glade-catalog.h"
#include "glade-widget-adaptor.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

struct _GladeCatalog
{
	guint16   major_version;    /* The catalog version               */
	guint16   minor_version;

	GList *targetable_versions; /* list of suitable version targets */

	gboolean libglade_supported; /* whether this catalog supports libglade */
	gboolean builder_supported; /* whether this catalog supports gtkbuilder */

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

	gchar *icon_prefix;      /* the prefix for icons */

	GList *widget_groups;    /* List of widget groups (palette)   */
	GList *adaptors;         /* List of widget class adaptors (all)  */

	GladeXmlContext *context;/* Xml context is stored after open
				  * before classes are loaded         */
	
	GModule *module;
	
	gchar *init_function_name;/* Catalog's init function name */
	GladeCatalogInitFunc init_function;

	GladeProjectConvertFunc project_convert_function; /* pointer to module's project converter */
};

struct _GladeWidgetGroup
{
	gchar *name;             /* Group name */
	gchar *title;            /* Group name in the palette */

	gboolean expanded;       /* Whether group is expanded in the palette */

	GList *adaptors;         /* List of class adaptors in the palette    */
};

static void            catalog_load         (GladeCatalog     *catalog);

static GladeCatalog   *catalog_open         (const gchar      *filename);

static GList          *catalog_sort         (GList            *catalogs);

static gboolean        catalog_load_classes (GladeCatalog     *catalog,

					     GladeXmlNode     *widgets_node);
					     
static gboolean        catalog_load_group   (GladeCatalog     *catalog,

					     GladeXmlNode     *group_node);

static void            widget_group_destroy (GladeWidgetGroup *group);

static void            catalog_destroy      (GladeCatalog *catalog);

static void            module_close         (GModule *module);


/* List of catalogs successfully loaded.
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
catalog_allocate (void)
{
	GladeCatalog *catalog;
	
	catalog = g_slice_new0 (GladeCatalog);
	
	catalog->library = NULL;
	catalog->name = NULL;
	catalog->dep_catalog = NULL;      
	catalog->domain = NULL;	
	catalog->book = NULL;
	catalog->icon_prefix = NULL;
	catalog->init_function_name = NULL;
	catalog->module = NULL;

	catalog->context = NULL;
	catalog->adaptors = NULL;
	catalog->widget_groups = NULL;

	catalog->libglade_supported = FALSE;
	catalog->builder_supported = TRUE;
	return catalog;
}

static GladeCatalog *
catalog_open (const gchar *filename)
{
	GladeTargetableVersion *version;
	GladeCatalog    *catalog;
	GladeXmlContext *context;
	GladeXmlDoc     *doc;
	GladeXmlNode    *root;
	gchar           *str;

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

	catalog = catalog_allocate ();
	catalog->context = context;
	catalog->name    = glade_xml_get_property_string (root, GLADE_TAG_NAME);

	if (!catalog->name) 
	{
		g_warning ("Couldn't find required property 'name' in catalog root node");
		catalog_destroy (catalog);
		return NULL;
	}


	glade_xml_get_property_version (root, GLADE_TAG_VERSION, 
					&catalog->major_version,
					&catalog->minor_version);

	/* Make one default suitable target */
	version = g_new (GladeTargetableVersion, 1);
	version->major = catalog->major_version;
	version->minor = catalog->minor_version;

	catalog->targetable_versions = 
		glade_xml_get_property_targetable_versions
		(root, GLADE_TAG_TARGETABLE);

	catalog->targetable_versions = g_list_prepend (catalog->targetable_versions, version);

	catalog->library      = glade_xml_get_property_string (root, GLADE_TAG_LIBRARY);
	catalog->dep_catalog  = glade_xml_get_property_string (root, GLADE_TAG_DEPENDS);
	catalog->domain       = glade_xml_get_property_string (root, GLADE_TAG_DOMAIN);
	catalog->book         = glade_xml_get_property_string (root, GLADE_TAG_BOOK);
	catalog->icon_prefix  = glade_xml_get_property_string (root, GLADE_TAG_ICON_PREFIX);
	catalog->init_function_name = glade_xml_get_value_string (root, GLADE_TAG_INIT_FUNCTION);

	if (!catalog->domain)
		catalog->domain = g_strdup (catalog->library);

	if ((str = glade_xml_get_property_string (root, GLADE_TAG_SUPPORTS)) != NULL)
	{
		gchar **split = g_strsplit (str, ",", 0);
		gint i;

		catalog->builder_supported = FALSE;
		
		for (i = 0; split[i]; i++)
		{
			if (!strcmp (split[i], GLADE_TAG_LIBGLADE))
				catalog->libglade_supported = TRUE;
			else if (!strcmp (split[i], GLADE_TAG_GTKBUILDER))
				catalog->builder_supported = TRUE;
		}
	}
	
	/* catalog->icon_prefix defaults to catalog->name */
	if (!catalog->icon_prefix)
		catalog->icon_prefix = g_strdup (catalog->name);

	if (catalog->init_function_name)
		catalog_get_function (catalog, catalog->init_function_name,
				      (gpointer) &catalog->init_function);
	
	if ((str = glade_xml_get_value_string (root, GLADE_TAG_PROJECT_CONVERT_FUNCTION)) != NULL)
	{
		catalog_get_function (catalog, str, (gpointer) &catalog->project_convert_function);
	}

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

			if (!node || (cat = node->data) == NULL)
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

static GHashTable *modules = NULL;

static GModule *
catalog_load_library (GladeCatalog *catalog)
{
	GModule *module;

	if (modules == NULL)
		modules = g_hash_table_new_full (g_str_hash,
					         g_str_equal,
					         (GDestroyNotify) g_free,
					         (GDestroyNotify) module_close);
		
	if (catalog->library == NULL)
		return NULL;

	if ((module = g_hash_table_lookup (modules, catalog->library)))
		return module;	
	
	if ((module = glade_util_load_library (catalog->library)))
		g_hash_table_insert (modules, g_strdup (catalog->library), module);
	else
		g_warning ("Failed to load external library '%s'",
			   catalog->library);
		
	return module;
}

static gboolean
catalog_load_classes (GladeCatalog *catalog, GladeXmlNode *widgets_node)
{
	GladeXmlNode *node;
	GModule *module = catalog_load_library (catalog);

	node = glade_xml_node_get_children (widgets_node);
	for (; node; node = glade_xml_node_next (node)) 
	{
		const gchar        *node_name, *domain;
		GladeWidgetAdaptor *adaptor;

		node_name = glade_xml_node_get_name (node);
		if (strcmp (node_name, GLADE_TAG_GLADE_WIDGET_CLASS) != 0) 
			continue;
	
		domain = catalog->domain ? catalog->domain : catalog->library;
		
		adaptor = glade_widget_adaptor_from_catalog (catalog, node, module);

		catalog->adaptors = g_list_prepend (catalog->adaptors, adaptor);
	}

	return TRUE;
}

static gboolean
catalog_load_group (GladeCatalog *catalog, GladeXmlNode *group_node)
{
	GladeWidgetGroup *group;
	GladeXmlNode     *node;
        char *title, *translated_title;

	group = g_slice_new0 (GladeWidgetGroup);
	
	group->name = glade_xml_get_property_string (group_node, 
						     GLADE_TAG_NAME);

	if (!group->name) 
	{ 
		g_warning ("Required property 'name' not found in group node");
		widget_group_destroy (group);
		return FALSE;
	}
	
	title = glade_xml_get_property_string (group_node,
					       GLADE_TAG_TITLE);
	if (!title)
	{ 
		g_warning ("Required property 'title' not found in group node");
		widget_group_destroy (group);
		return FALSE;	
	}

	/* by default, group is expanded in palette  */
	group->expanded = TRUE;

	/* Translate it */
	translated_title = dgettext (catalog->domain, 
				     title);
        if (translated_title != title)
        {
                group->title = g_strdup (translated_title);
                g_free (title);
        }
        else
        {
                group->title = title;
        }

	group->adaptors = NULL;

	node = glade_xml_node_get_children (group_node);
	for (; node; node = glade_xml_node_next (node)) 
	{
		const gchar        *node_name;
		GladeWidgetAdaptor *adaptor;
		gchar              *name;

		node_name = glade_xml_node_get_name (node);
		
		if (strcmp (node_name, GLADE_TAG_GLADE_WIDGET_CLASS_REF) == 0)
		{
			if ((name = 
			     glade_xml_get_property_string (node, GLADE_TAG_NAME)) == NULL)
			{
				g_warning ("Couldn't find required property on %s",
					   GLADE_TAG_GLADE_WIDGET_CLASS);
				continue;
			}

			if ((adaptor = glade_widget_adaptor_get_by_name (name)) == NULL)
			{
				g_warning ("Tried to include undefined widget "
					   "class '%s' in a widget group", name);
				g_free (name);
				continue;
			}
			g_free (name);

			group->adaptors = g_list_prepend (group->adaptors, adaptor);

		}
		else if (strcmp (node_name, GLADE_TAG_DEFAULT_PALETTE_STATE) == 0)
		{
			group->expanded = 
				glade_xml_get_property_boolean 
				(node, GLADE_TAG_EXPANDED, group->expanded);
		}
	}

	group->adaptors = g_list_reverse (group->adaptors);

	catalog->widget_groups = g_list_prepend (catalog->widget_groups, group);

	return TRUE;
}

static GList *
catalogs_from_path (GList *catalogs, const gchar *path)
{
	GladeCatalog *catalog;
	GDir         *dir;
	GError       *error = NULL;
	const gchar  *filename;

	if ((dir = g_dir_open (path, 0, &error)) != NULL)
	{
		while ((filename = g_dir_read_name (dir)))
		{
			gchar *catalog_filename;
			
			if (!g_str_has_suffix (filename, ".xml")) 
				continue;

			catalog_filename = g_build_filename (path, filename, NULL);
			catalog = catalog_open (catalog_filename);
			g_free (catalog_filename);

			if (catalog)
			{
				/* Verify that we are not loading the same catalog twice !
				 */
				if (!g_list_find_custom (catalogs, catalog->name,
							 (GCompareFunc)catalog_find_by_name))
					catalogs = g_list_prepend (catalogs, catalog);
				else
					catalog_destroy (catalog);
			}
			else
				g_warning ("Unable to open the catalog file %s.\n", filename);
		}
	}
	else
		g_warning ("Failed to open catalog directory '%s': %s", path, error->message);
	

	return catalogs;
}

const GList *
glade_catalog_load_all (void)
{
	GList         *catalogs = NULL, *l;
	GladeCatalog  *catalog;
	const gchar   *search_path;
	gchar        **split;
	gint           i;
	
	/* First load catalogs from user specified directories ... */
	if ((search_path = g_getenv (GLADE_ENV_CATALOG_PATH)) != NULL)
	{
		if ((split = g_strsplit (search_path, ":", 0)) != NULL)
		{
			for (i = 0; split[i] != NULL; i++)
				catalogs = catalogs_from_path (catalogs, split[i]);

			g_strfreev (split);
		}
	}

	/* ... Then load catalogs from standard install directory */
	catalogs = catalogs_from_path (catalogs, glade_app_get_catalogs_dir ());
	
	/* Catalogs need dependancies, most catalogs depend on
	 * the gtk+ catalog, but some custom toolkits may depend
	 * on the gnome catalog for instance.
	 */
	catalogs = catalog_sort (catalogs);
	
	/* After sorting, execute init function and then load */
	for (l = catalogs; l; l = l->next)
	{
		catalog = l->data;
		if (catalog->init_function)
			catalog->init_function (catalog->name);
	}
	
	for (l = catalogs; l; l = l->next)
	{
		catalog = l->data;
		catalog_load (catalog);
	}
	
	loaded_catalogs = catalogs;
	
	return loaded_catalogs;
}

G_CONST_RETURN gchar *
glade_catalog_get_name (GladeCatalog *catalog)
{
	g_return_val_if_fail (GLADE_IS_CATALOG (catalog), NULL);

	return catalog->name;
}

G_CONST_RETURN gchar *
glade_catalog_get_book (GladeCatalog *catalog)
{
	g_return_val_if_fail (GLADE_IS_CATALOG (catalog), NULL);

	return catalog->book;
}

G_CONST_RETURN gchar *
glade_catalog_get_domain (GladeCatalog *catalog)
{
	g_return_val_if_fail (GLADE_IS_CATALOG (catalog), NULL);

	return catalog->domain;
}

G_CONST_RETURN gchar *
glade_catalog_get_icon_prefix (GladeCatalog *catalog)
{
	g_return_val_if_fail (GLADE_IS_CATALOG (catalog), NULL);

	return catalog->icon_prefix;
}

gboolean
glade_catalog_supports_libglade (GladeCatalog *catalog)
{
	g_return_val_if_fail (GLADE_IS_CATALOG (catalog), FALSE);

	return catalog->libglade_supported;
}

gboolean
glade_catalog_supports_gtkbuilder (GladeCatalog *catalog)
{
	g_return_val_if_fail (GLADE_IS_CATALOG (catalog), FALSE);

	return catalog->builder_supported;
}


guint16
glade_catalog_get_major_version (GladeCatalog *catalog)
{
	g_return_val_if_fail (GLADE_IS_CATALOG (catalog), 0);

	return catalog->major_version;
}

guint16
glade_catalog_get_minor_version (GladeCatalog *catalog)
{
	g_return_val_if_fail (GLADE_IS_CATALOG (catalog), 0);

	return catalog->minor_version;
}


GList *
glade_catalog_get_targets (GladeCatalog *catalog)
{
	g_return_val_if_fail (GLADE_IS_CATALOG (catalog), NULL);

	return catalog->targetable_versions;
}

GList *
glade_catalog_get_widget_groups (GladeCatalog *catalog)
{
	g_return_val_if_fail (GLADE_IS_CATALOG (catalog), NULL);

	return catalog->widget_groups;	
}

GList *
glade_catalog_get_adaptors (GladeCatalog *catalog)
{
	g_return_val_if_fail (GLADE_IS_CATALOG (catalog), NULL);

	return catalog->adaptors;	
}

gboolean
glade_catalog_is_loaded (const gchar *name)
{
	GList *l;
	
	g_return_val_if_fail (name != NULL, FALSE);
	g_assert (loaded_catalogs != NULL);
	
	for (l = loaded_catalogs; l; l = l->next)
	{
		GladeCatalog *catalog = GLADE_CATALOG (l->data);
		if (strcmp (catalog->name, name) == 0)
			return TRUE;
	}
	
	return FALSE;
}

static void
catalog_destroy (GladeCatalog *catalog)
{
	g_return_if_fail (GLADE_IS_CATALOG (catalog));

	g_free (catalog->name);
	g_free (catalog->library);	
	g_free (catalog->dep_catalog);      
	g_free (catalog->domain);           	
	g_free (catalog->book);
	g_free (catalog->icon_prefix);
	g_free (catalog->init_function_name);

	if (catalog->adaptors)
	{
		/* TODO: free adaptors */
		g_list_free (catalog->adaptors);
	}

	if (catalog->widget_groups)
	{
		g_list_foreach (catalog->widget_groups, (GFunc) widget_group_destroy, NULL);
		g_list_free (catalog->widget_groups);
	}

	if (catalog->context)
		glade_xml_context_free (catalog->context);

	g_slice_free (GladeCatalog, catalog);
}

static void 
module_close (GModule *module)
{
	g_module_close (module);
}

void
glade_catalog_destroy_all (void)
{	
	/* close catalogs */
	if (loaded_catalogs)
	{
		GList *l;
		for (l = loaded_catalogs; l; l = l->next)
			catalog_destroy (GLADE_CATALOG (l->data));
		g_list_free (loaded_catalogs);
		loaded_catalogs = NULL;
	}
	
	/* close plugin modules */
	if (modules)
	{
		g_hash_table_destroy (modules);
		modules = NULL;
	}
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

const GList *
glade_widget_group_get_adaptors (GladeWidgetGroup *group)
{
	g_return_val_if_fail (group != NULL, NULL);

	return group->adaptors;
}

static void
widget_group_destroy (GladeWidgetGroup *group)
{
	g_return_if_fail (GLADE_IS_WIDGET_GROUP (group));
	
	g_free (group->name);
	g_free (group->title);
	g_list_free (group->adaptors);

	g_slice_free (GladeWidgetGroup, group);
}
	


/**
 * glade_catalog_convert_project:
 * @catalog: A #GladeCatalog
 * @project: The #GladeProject to convert
 * @new_format: The format to convert @project to
 *
 * Do any data changes needed to the project for the new
 * format in an undoable way.
 *
 * Returns: FALSE if any errors occurred during the conversion.
 */
gboolean
glade_catalog_convert_project (GladeCatalog     *catalog,
			       GladeProject     *project,
			       GladeProjectFormat  new_format)
{
	g_return_val_if_fail (GLADE_IS_CATALOG (catalog), FALSE);
	g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

	if (catalog->project_convert_function)
		return catalog->project_convert_function (project, new_format);

	return TRUE;
}
