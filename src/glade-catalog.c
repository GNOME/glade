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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
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

#include "glade.h"
#include "glade-catalog.h"
#include "glade-widget-class.h"

#define GLADE_TAG_CATALOG "GladeCatalog"
#define GLADE_TAG_WIDGETS "GladeWidgets"
#define GLADE_TAG_WIDGET_GROUP "GladeWidgetGroup"

struct _GladeCatalog
{
	gchar *library;

	gchar *name;

	GList *widget_groups;
	GList *widget_classes;
};

struct _GladeWidgetGroup {
	gchar *name;
	gchar *title;
	GList *widget_classes;
};

static GladeCatalog *  catalog_load         (const char       *filename);
static gboolean        catalog_load_widgets (GladeCatalog     *catalog,
					     GladeXmlNode     *widgets_node);
static gboolean        catalog_load_group   (GladeCatalog     *catalog,
					     GladeXmlNode     *group_node);

void                   widget_group_free    (GladeWidgetGroup *group);


static GladeCatalog *
catalog_load (const char *filename)
{
	GladeCatalog    *catalog;
	GladeXmlContext *context;
	GladeXmlDoc     *doc;
	GladeXmlNode    *root;
	GladeXmlNode    *node;

	g_print ("Loading catalog: %s\n", filename);
	/* get the context & root node of the catalog file */
	context = glade_xml_context_new_from_path (filename,
						   NULL, GLADE_TAG_CATALOG);
	if (!context) 
	{
		g_warning ("Couldn't load catalog [%s].", filename);
		return NULL;
	}

	doc  = glade_xml_context_get_doc (context);
	root = glade_xml_doc_get_root (doc);

	if (!glade_xml_node_verify (root, GLADE_TAG_CATALOG)) 
	{
		g_warning ("Catalog root node is not '%s', skipping %s",
			   GLADE_TAG_CATALOG, filename);
		glade_xml_context_free (context);
		return NULL;
	}

	catalog = g_new0 (GladeCatalog, 1);

	catalog->name = glade_xml_get_property_string (root, "name");
	if (!catalog->name) 
	{
		g_warning ("Couldn't find required property 'name' in catalog root node");
		g_free (catalog);
		glade_xml_context_free (context);
		return NULL;
	}
	
	catalog->library = glade_xml_get_property_string (root, "library");

	node = glade_xml_node_get_children (root);
	for (; node; node = glade_xml_node_next (node))
	{
		const gchar *node_name;

		node_name = glade_xml_node_get_name (node);
		if (strcmp (node_name, GLADE_TAG_WIDGETS) == 0) 
		{
			catalog_load_widgets (catalog, node);
		}
		else if (strcmp (node_name, GLADE_TAG_WIDGET_GROUP) == 0)
		{
			catalog_load_group (catalog, node);
		}
		else 
			continue;
	}

	catalog->widget_groups = g_list_reverse (catalog->widget_groups);

	glade_xml_context_free (context);

	return catalog;
}

static gboolean
catalog_load_widgets (GladeCatalog *catalog, GladeXmlNode *widgets_node)
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
	
		widget_class = glade_widget_class_new_from_node (node, 
								 catalog->library);

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
	
	group->name = glade_xml_get_property_string (group_node, "name");
	if (!group->name) 
	{ 
		g_warning ("Required property 'name' not found in group node");
	
		widget_group_free (group);
		return FALSE;
	}
	
	group->title = glade_xml_get_property_string (group_node, "title");
	if (!group->title) 
	{ 
		g_warning ("Required property 'title' not found in group node");

		widget_group_free (group);
		return FALSE;	
	}

	g_print ("Loading widget group: %s\n", group->title);

	group->widget_classes = NULL;

	node = glade_xml_node_get_children (group_node);
	for (; node; node = glade_xml_node_next (node)) 
	{
		const gchar      *node_name;
		GladeWidgetClass *widget_class;
		gchar            *name;

		node_name = glade_xml_node_get_name (node);
		
		if (strcmp (node_name, GLADE_TAG_GLADE_WIDGET_CLASS) != 0) 
			continue;

		name = glade_xml_get_property_string (node, "name");
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

	group->widget_classes = g_list_reverse (group->widget_classes);

	catalog->widget_groups = g_list_prepend (catalog->widget_groups,
						 group);

	return TRUE;
}

void
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
	GError       *error;
	const gchar  *filename;
	GList        *catalogs;
	GladeCatalog *catalog;
	
	/* Read all files in catalog dir */
	dir = g_dir_open (glade_catalogs_dir, 0, &error);
	if (!dir) 
	{
		g_warning ("Failed to open catalog directory: %s",
			   error->message);
		return NULL;
	}

	catalogs = NULL;
	while ((filename = g_dir_read_name (dir)))
	{
		gchar *catalog_filename;

		if (!g_str_has_suffix (filename, ".catalog")) 
			continue;

		catalog_filename = g_build_filename (glade_catalogs_dir,
						     filename, NULL);
		catalog = catalog_load (catalog_filename);
		g_free (catalog_filename);

		if (catalog) 
			catalogs = g_list_append (catalogs, catalog);
		else
			g_warning ("Unable to open the catalog file %s.\n", 
				   filename);
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

GList *
glade_widget_group_get_widget_classes (GladeWidgetGroup *group)
{
	g_return_val_if_fail (group != NULL, NULL);

	return group->widget_classes;
}


