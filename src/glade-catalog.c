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

#define GLADE_TAG_PALETTE "GladePalette"

/**
 * glade_catalog_delete:
 * @catalog:
 * 
 * Frees @catalog and its associated memory.
 */
void
glade_catalog_delete (GladeCatalog *catalog)
{
	GList *list;

	if (catalog == NULL)
		return;

	g_free (catalog->title);
	for (list = catalog->widget_classes; list; list = list->next)
		glade_widget_class_free (GLADE_WIDGET_CLASS (list->data));
	g_list_free (catalog->widget_classes);
	g_free (catalog);
}

static GladeCatalog *
glade_catalog_load (const char *base_catalog_filename)
{
	GladeWidgetClass *widget_class = NULL;
	GladeXmlContext *context = NULL;
	GladeXmlNode *root = NULL;
	GladeXmlNode *widget_node = NULL;
	GladeXmlDoc *doc = NULL;
	GladeCatalog *catalog = NULL;
	char *name = NULL;
	char *generic_name = NULL;
	char *palette_name = NULL;
	char *catalog_filename = NULL;
	char *base_filename = NULL;
	char *base_library = NULL;
	GList *last_widget_class = NULL;

	catalog_filename = g_strdup_printf ("%s%c%s", glade_catalogs_dir, G_DIR_SEPARATOR, base_catalog_filename);
	if (catalog_filename == NULL)
	{
		g_critical (_("Not enough memory."));
		goto lblError;
	}

	/* get the context & root node of the catalog file */
	context = glade_xml_context_new_from_path (catalog_filename, NULL, GLADE_TAG_CATALOG);
	if (!context)
	{
		g_warning (_("Impossible to open the catalog [%s]."), catalog_filename);
		goto lblError;
	}

	doc = glade_xml_context_get_doc (context);
	root = glade_xml_doc_get_root (doc);

	catalog = g_new0 (GladeCatalog, 1);

	last_widget_class = NULL;

	/* read the title of this catalog */
	catalog->title = glade_xml_get_property_string_required (root, "Title", catalog_filename);

	if (!glade_xml_node_verify (root, GLADE_TAG_CATALOG))
	{
		g_warning (_("The root node of [%s] has a name different from %s."), catalog_filename, GLADE_TAG_CATALOG);
		goto lblError;
	}

	/* get the library to be used by this catalog (if any) */
	base_library = glade_xml_get_property_string (root, "library");

	/* build all the GladeWidgetClass'es associated with this catalog */
	widget_node = glade_xml_node_get_children (root);
	for (; widget_node; widget_node = glade_xml_node_next (widget_node))
	{
		char *base_widget_class_library = NULL;

		if (!glade_xml_node_verify (widget_node, GLADE_TAG_GLADE_WIDGET))
			continue;

		name = glade_xml_get_property_string_required (widget_node, "name", catalog_filename);
		if (!name)
			continue;

		/* get the specific library to the widget class, if any */
		base_widget_class_library = glade_xml_get_property_string (widget_node, "library");

		generic_name = glade_xml_get_property_string (widget_node, "generic_name");
		palette_name = NULL;
		if (generic_name != NULL)
			palette_name = glade_xml_get_property_string_required (widget_node, "palette_name", name);
		
		base_filename = glade_xml_get_property_string (widget_node, "filename");

		widget_class = glade_widget_class_new (name, generic_name, palette_name, base_filename,
						       base_widget_class_library ? base_widget_class_library : base_library);
		if (widget_class)
		{
			last_widget_class = g_list_append (last_widget_class, widget_class);

			if (last_widget_class->next != NULL)
				last_widget_class = last_widget_class->next;
			else
				catalog->widget_classes = last_widget_class;
		}

		g_free (name);
		g_free (generic_name);
		g_free (palette_name);
		g_free (base_filename);
		g_free (base_widget_class_library);
	}

	glade_xml_context_free (context);
	g_free (catalog_filename);
	g_free (base_library);
	return catalog;

lblError:
	glade_xml_context_free (context);
	g_free (catalog_filename);
	g_free (catalog);
	g_free (base_library);
	return NULL;
}

/**
 * glade_catalog_load_all:
 *
 * TODO: write me
 *
 * Returns:
 */
GList *
glade_catalog_load_all (void)
{
	gchar *filename = g_build_filename (glade_catalogs_dir, "glade-palette.xml", NULL);
	GladeXmlContext *context;
	GladeXmlNode *root;
	GladeXmlNode *xml_catalogs;
	GladeXmlDoc *doc;
	GList *catalogs = NULL;
	GladeCatalog *catalog;

	context = glade_xml_context_new_from_path (filename, NULL, GLADE_TAG_PALETTE);
	if (context == NULL)
	{
		g_critical ("Unable to open %s.\n", filename);
		return NULL;
	}

	doc = glade_xml_context_get_doc (context);
	root = glade_xml_doc_get_root (doc);
	xml_catalogs = glade_xml_node_get_children (root);
	while (xml_catalogs != NULL) {
		char *name = glade_xml_get_property_string_required (xml_catalogs, "filename", NULL);

		if (!strcmp(glade_xml_node_get_name (xml_catalogs), GLADE_TAG_CATALOG))
		{
			catalog = glade_catalog_load (name);
			if (catalog) 
				catalogs = g_list_append (catalogs, catalog);
			else
				g_warning ("Unable to open the catalog %s.\n", name);
		}
		else
			g_warning ("The palette file \"glade-palette.xml\" has "
				    "a node with name %s instead of " GLADE_TAG_CATALOG "\n", name);

		xml_catalogs = glade_xml_node_next (xml_catalogs);
		g_free (name);
	}

	glade_xml_context_free (context);
	g_free (filename);
	return catalogs;
}

/**
 * glade_catalog_get_title:
 * @catalog: a #GladeCatalog
 *
 * Returns: a pointer to the title of @catalog
 */
const char *
glade_catalog_get_title (GladeCatalog *catalog)
{
	g_return_val_if_fail (catalog != NULL, NULL);
	return catalog->title;
}

/**
 * glade_catalog_get_widget_classes:
 * @catalog: a #GladeCatalog
 *
 * Returns: a #GList containing the widget classes in @catalog
 */
GList *
glade_catalog_get_widget_classes (GladeCatalog *catalog)
{
	g_return_val_if_fail (catalog != NULL, NULL);
	return catalog->widget_classes;	
}
