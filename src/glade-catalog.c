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

#include "glade.h"
#include "glade-catalog.h"
#include "glade-widget-class.h"

static GladeCatalog *
glade_catalog_new (void)
{
	GladeCatalog *catalog;

	catalog = g_new0 (GladeCatalog, 1);

	catalog->names = NULL;
	catalog->widgets = NULL;

	return catalog;
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

static void
glade_catalog_load_names_from_file (GladeCatalog *catalog, const gchar *file_name)
{
	GladeXmlContext *context;
	GladeXmlNode *root;
	GladeXmlDoc *doc;

	context = glade_xml_context_new_from_path (file_name, NULL, GLADE_TAG_CATALOG);
	if (context == NULL)
		return;
	doc = glade_xml_context_get_doc (context);
	root = glade_xml_doc_get_root (doc);
	catalog->names = glade_catalog_load_names_from_node (context, root);
	glade_xml_context_free (context);
}

static GladeCatalog *
glade_catalog_new_from_file (const gchar *file)
{
	GladeCatalog *catalog;

	catalog = glade_catalog_new ();

	glade_catalog_load_names_from_file (catalog, file);

	return catalog;
}

GladeCatalog *
glade_catalog_load (void)
{
	GladeWidgetClass *class;
	GladeCatalog *catalog;
	GList *list;
	GList *new_list;
	gchar *name;

	catalog = glade_catalog_new_from_file (WIDGETS_DIR "/catalog.xml");
	if (catalog == NULL)
		return NULL;

	list = catalog->names;
	new_list = NULL;
	for (; list != NULL; list = list->next) {
		name = list->data;
		class = glade_widget_class_new_from_name (name);
		if (class == NULL) continue;
		new_list = g_list_prepend (new_list, class);
	}

	catalog->widgets = g_list_reverse (new_list);

	return catalog;
}
		
