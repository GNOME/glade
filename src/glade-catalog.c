/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 Ximian, Inc.
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
#include <glib/gdir.h>

#include "glade.h"
#include "glade-catalog.h"
#include "glade-widget-class.h"

GList *glade_catalog_list = NULL; /* A list of GladeCatalog items */
GList *widget_class_list  = NULL; /* A list of all the GladeWidgetClass objects loaded */

GList *
glade_catalog_get_widgets (void)
{
	return widget_class_list;
}

static GladeCatalog *
glade_catalog_new (void)
{
	GladeCatalog *catalog;

	catalog = g_new0 (GladeCatalog, 1);

	catalog->names = NULL;
	catalog->widgets = NULL;

	return catalog;
}

static void
glade_catalog_delete (GladeCatalog *catalog)
{
	g_return_if_fail (catalog);

	g_free (catalog);
}

static GList *
glade_catalog_load_names_from_node (GladeXmlContext *context, GladeXmlNode *node)
{
	GladeXmlNode *child;
	GList *list;
	gchar *name;

	if (!glade_xml_node_verify (node, GLADE_TAG_CATALOG))
		return NULL;

	list = NULL;
	child = glade_xml_node_get_children (node);
	for (; child != NULL; child = glade_xml_node_next (child)) {
		if (!glade_xml_node_verify (child, GLADE_TAG_GLADE_WIDGET))
			return NULL;
		name = glade_xml_get_content (child);
		if (name == NULL)
			return NULL;
		list = g_list_prepend (list, name);
	}

	list = g_list_reverse (list);
	
	return list;
}

static gboolean
glade_catalog_load_names_from_file (GladeCatalog *catalog, const gchar *file_name)
{
	GladeXmlContext *context;
	GladeXmlNode *root;
	GladeXmlDoc *doc;

	context = glade_xml_context_new_from_path (file_name, NULL, GLADE_TAG_CATALOG);
	if (context == NULL)
		return FALSE;

	doc = glade_xml_context_get_doc (context);
	root = glade_xml_doc_get_root (doc);
	catalog->title = glade_xml_get_property_string_required (root, "Title", NULL);
	catalog->names = glade_catalog_load_names_from_node (context, root);
	glade_xml_context_free (context);

	return TRUE;
}

static GladeCatalog *
glade_catalog_new_from_file (const gchar *file)
{
	GladeCatalog *catalog;

	catalog = glade_catalog_new ();

	if (!glade_catalog_load_names_from_file (catalog, file)) {
		glade_catalog_delete (catalog);
		return NULL;
	}

	return catalog;
}

GladeCatalog *
glade_catalog_load (const gchar *file_name)
{
	GladeWidgetClass *class;
	GladeCatalog *catalog;
	GList *list;
	GList *new_list;
	gchar *name;

	catalog = glade_catalog_new_from_file (file_name);
	if (catalog == NULL)
		return NULL;

	list = catalog->names;
	new_list = NULL;
	for (; list != NULL; list = list->next) {
		name = list->data;
		class = glade_widget_class_new_from_name (name);
		if (class == NULL) continue;
		new_list          = g_list_prepend (new_list, class); 
		widget_class_list = g_list_prepend (widget_class_list, class);
		/* We keep a list per catalog (group) and a general list of
		 * all widgets loaded
		 */
	}

	catalog->widgets = g_list_reverse (new_list);

	glade_catalog_list = g_list_prepend (glade_catalog_list, catalog);
	
	list = catalog->widgets;
	for (; list != NULL; list = list->next) {
		class = list->data;
		glade_widget_class_load_packing_properties (class);
	}


	return catalog;
}

GList *
glade_catalog_load_all (void)
{
	GDir *catalogsdir = NULL;
	GList *catalogs = NULL;
	GladeCatalog *gcatalog = NULL;
	const char *base_filename = NULL;
	char *filename = NULL;

	catalogsdir = g_dir_open (CATALOGS_DIR, 0, NULL);
	if (!catalogsdir) {
		g_warning ("Could not open catalogs from %s\n", CATALOGS_DIR);
		return NULL;
	}
	    
	while ((base_filename = g_dir_read_name (catalogsdir)) != NULL) {
		filename = g_strdup_printf ("%s/%s", CATALOGS_DIR, base_filename);
		gcatalog = glade_catalog_load (filename);

		if (gcatalog) 
			catalogs = g_list_append (catalogs, gcatalog);

		g_free (filename);
	}
	g_dir_close (catalogsdir);

	return catalogs;
}
