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

static GList *widget_class_list  = NULL; /* A list of all the GladeWidgetClass objects loaded */

GList *
glade_catalog_get_widgets (void)
{
	return widget_class_list;
}

void
glade_catalog_delete (GladeCatalog *catalog)
{
	GList *list;

	if (catalog == NULL)
		return;

	g_free (catalog->title);

	list = catalog->widget_classes;
	while (list != NULL)
	{
		glade_widget_class_free ((GladeWidgetClass*) list->data);
		list = list->next;
	}

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
	char *catalog_filename = NULL;
	char *base_filename = NULL;
	char *partial_library = NULL;
	char *base_library = NULL;
	GList *last_widget_class = NULL;

	catalog_filename = g_strdup_printf ("%s%c%s", CATALOGS_DIR, G_DIR_SEPARATOR, base_catalog_filename);
	if (catalog_filename == NULL)
	{
		g_critical (_("Not enough memory."));
		goto lblError;
	}

	/* get the context & root node of the catalog file */
	context = glade_xml_context_new_from_path (catalog_filename, NULL, GLADE_TAG_CATALOG);
	if (context == NULL)
	{
		g_warning (_("Impossible to open the catalog [%s]."), catalog_filename);
		goto lblError;
	}

	doc = glade_xml_context_get_doc (context);
	root = glade_xml_doc_get_root (doc);

	/* allocate the catalog */
	catalog = g_new0 (GladeCatalog, 1);
	if (catalog == NULL)
	{
		g_critical (_("Not enough memory."));
		goto lblError;
	}

	last_widget_class = NULL;

	/* read the title of this catalog */
	catalog->title = glade_xml_get_property_string_required (root, "Title", NULL);

	if (!glade_xml_node_verify (root, GLADE_TAG_CATALOG))
	{
		g_warning (_("The root node of [%s] has a name different from %s."), catalog_filename, GLADE_TAG_CATALOG);
		goto lblError;
	}

	/* get the library to be used by this catalog (if any) */
	partial_library = glade_xml_get_property_string (root, "library");
	if (partial_library && *partial_library)
	{
		base_library = g_strdup_printf ("libglade%s", partial_library);
		if (!base_library)
		{
			g_critical (_("Not enough memory."));
			goto lblError;
		}
	}

	/* build all the GladeWidgetClass'es associated with this catalog */
	widget_node = glade_xml_node_get_children (root);
	for (; widget_node != NULL; widget_node = glade_xml_node_next (widget_node))
	{
		char *partial_widget_class_library = NULL;
		char *base_widget_class_library = NULL;
		
		if (!glade_xml_node_verify (widget_node, GLADE_TAG_GLADE_WIDGET))
			continue;

		name = glade_xml_get_property_string_required (widget_node, "name", NULL);
		if (name == NULL)
			continue;

		/* get the specific library to the widget class, if any */
		partial_widget_class_library = glade_xml_get_property_string (widget_node, "library");
		if (partial_widget_class_library && *partial_widget_class_library)
		{
			base_widget_class_library = g_strdup_printf ("libglade%s", partial_widget_class_library);
			if (!base_widget_class_library)
			{
				g_critical (_("Not enough memory."));
				continue;
			}
		}

		generic_name = glade_xml_get_property_string (widget_node, "generic_name");
		base_filename = glade_xml_get_property_string (widget_node, "filename");

		widget_class = glade_widget_class_new (name, generic_name, base_filename,
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
		g_free (base_filename);
		g_free (partial_widget_class_library);
		g_free (base_widget_class_library);
	}

	glade_xml_context_free (context);
	g_free (catalog_filename);
	return catalog;

lblError:
	glade_xml_context_free (context);
	g_free (catalog_filename);
	g_free (catalog);
	g_free (partial_library);
	g_free (base_library);
	return NULL;
}

GList *
glade_catalog_load_all (void)
{
	static const char * const filename = CATALOGS_DIR G_DIR_SEPARATOR_S "glade-palette.xml";
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
	return catalogs;
}

const char *glade_catalog_get_title (GladeCatalog *catalog)
{
	g_return_val_if_fail (catalog != NULL, NULL);
	return catalog->title;
}

GList *glade_catalog_get_widget_classes (GladeCatalog *catalog)
{
	g_return_val_if_fail (catalog != NULL, NULL);
	return catalog->widget_classes;	
}
