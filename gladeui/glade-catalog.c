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
#include "glade-private.h"
#include "glade-tsort.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <glib.h>
#include <glib/gi18n-lib.h>

struct _GladeCatalog
{
  guint16 major_version;        /* The catalog version               */
  guint16 minor_version;

  GList *targetable_versions;   /* list of suitable version targets */

  gchar *library;               /* Library name for backend support  */

  gchar *name;                  /* Symbolic catalog name             */

  gchar *prefix;                /* Catalog path prefix               */
   
  gchar *dep_catalog;           /* Symbolic name of the catalog that
                                 * this catalog depends on           */

  gchar *domain;                /* The domain to be used to translate
                                 * strings from this catalog (otherwise this 
                                 * defaults to the library name)
                                 */

  gchar *book;                  /* Devhelp search domain */

  gchar *icon_prefix;           /* the prefix for icons */

  GList *widget_groups;         /* List of widget groups (palette)   */
  GList *adaptors;              /* List of widget class adaptors (all)  */

  GladeXmlContext *context;     /* Xml context is stored after open
                                 * before classes are loaded         */

  GModule *module;

  gchar *init_function_name;    /* Catalog's init function name */
  GladeCatalogInitFunc init_function;
};

struct _GladeWidgetGroup
{
  gchar *name;                  /* Group name */
  gchar *title;                 /* Group name in the palette */

  gboolean expanded;            /* Whether group is expanded in the palette */

  GList *adaptors;              /* List of class adaptors in the palette    */
};

/* List of catalogs successfully loaded. */
static GList *loaded_catalogs = NULL;

/* Extra paths to load catalogs from */
static GList *catalog_paths = NULL;

static gboolean
catalog_get_function (GladeCatalog *catalog,
                      const gchar  *symbol_name,
                      gpointer     *symbol_ptr)
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
  catalog->prefix = NULL;
  catalog->dep_catalog = NULL;
  catalog->domain = NULL;
  catalog->book = NULL;
  catalog->icon_prefix = NULL;
  catalog->init_function_name = NULL;
  catalog->module = NULL;

  catalog->context = NULL;
  catalog->adaptors = NULL;
  catalog->widget_groups = NULL;

  return catalog;
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
      g_list_free (catalog->adaptors);
    }

  if (catalog->widget_groups)
    {
      g_list_foreach (catalog->widget_groups, (GFunc) widget_group_destroy,
                      NULL);
      g_list_free (catalog->widget_groups);
    }

  g_clear_pointer (&catalog->context, glade_xml_context_free);
  g_slice_free (GladeCatalog, catalog);
}

static GladeCatalog *
catalog_open (const gchar *filename)
{
  GladeTargetableVersion *version;
  GladeCatalog *catalog;
  GladeXmlContext *context;
  GladeXmlDoc *doc;
  GladeXmlNode *root;
  gchar *name;

  /* get the context & root node of the catalog file */
  context = glade_xml_context_new_from_path (filename,
                                             NULL, GLADE_TAG_GLADE_CATALOG);
  if (!context)
    {
      g_warning ("Couldn't open catalog [%s].", filename);
      return NULL;
    }

  doc = glade_xml_context_get_doc (context);
  root = glade_xml_doc_get_root (doc);

  if (!glade_xml_node_verify (root, GLADE_TAG_GLADE_CATALOG))
    {
      g_warning ("Catalog root node is not '%s', skipping %s",
                 GLADE_TAG_GLADE_CATALOG, filename);
      glade_xml_context_free (context);
      return NULL;
    }

  if (!(name = glade_xml_get_property_string_required (root, GLADE_TAG_NAME, NULL)))
    return NULL;

  catalog = catalog_allocate ();
  catalog->context = context;
  catalog->name = name;
  catalog->prefix = g_path_get_dirname (filename);

  glade_xml_get_property_version (root, GLADE_TAG_VERSION,
                                  &catalog->major_version,
                                  &catalog->minor_version);

  /* Make one default suitable target */
  version = g_new (GladeTargetableVersion, 1);
  version->major = catalog->major_version;
  version->minor = catalog->minor_version;

  catalog->targetable_versions =
      glade_xml_get_property_targetable_versions (root, GLADE_TAG_TARGETABLE);

  catalog->targetable_versions =
      g_list_prepend (catalog->targetable_versions, version);

  catalog->library = glade_xml_get_property_string (root, GLADE_TAG_LIBRARY);
  catalog->dep_catalog =
      glade_xml_get_property_string (root, GLADE_TAG_DEPENDS);
  catalog->domain = glade_xml_get_property_string (root, GLADE_TAG_DOMAIN);
  catalog->book = glade_xml_get_property_string (root, GLADE_TAG_BOOK);
  catalog->icon_prefix =
      glade_xml_get_property_string (root, GLADE_TAG_ICON_PREFIX);
  catalog->init_function_name =
      glade_xml_get_value_string (root, GLADE_TAG_INIT_FUNCTION);

  if (!catalog->domain)
    catalog->domain = g_strdup (catalog->library);

  /* catalog->icon_prefix defaults to catalog->name */
  if (!catalog->icon_prefix)
    catalog->icon_prefix = g_strdup (catalog->name);

  if (catalog->init_function_name)
    {
      if (!catalog_get_function (catalog, catalog->init_function_name,
                                 (gpointer) & catalog->init_function))
        g_warning ("Failed to find and execute catalog '%s' init function '%s'",
                   glade_catalog_get_name (catalog),
                   catalog->init_function_name);
    }

  return catalog;
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
                                     (GDestroyNotify) g_module_close);

  if (catalog->library == NULL)
    return NULL;

  if ((module = g_hash_table_lookup (modules, catalog->library)))
    return module;

  if ((module = glade_util_load_library (catalog->library)))
    g_hash_table_insert (modules, g_strdup (catalog->library), module);
  else
    g_warning ("Failed to load external library '%s' for catalog '%s'",
               catalog->library,
               glade_catalog_get_name (catalog));

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
      const gchar *node_name;
      GladeWidgetAdaptor *adaptor;

      node_name = glade_xml_node_get_name (node);
      if (strcmp (node_name, GLADE_TAG_GLADE_WIDGET_CLASS) != 0)
        continue;

      adaptor = glade_widget_adaptor_from_catalog (catalog, node, module);

      catalog->adaptors = g_list_prepend (catalog->adaptors, adaptor);
    }

  return TRUE;
}

static gboolean
catalog_load_group (GladeCatalog *catalog, GladeXmlNode *group_node)
{
  GladeWidgetGroup *group;
  GladeXmlNode *node;
  char *title, *translated_title;

  group = g_slice_new0 (GladeWidgetGroup);

  group->name = glade_xml_get_property_string (group_node, GLADE_TAG_NAME);

  if (!group->name)
    {
      g_warning ("Required property 'name' not found in group node");
      widget_group_destroy (group);
      return FALSE;
    }

  title = glade_xml_get_property_string (group_node, GLADE_TAG_TITLE);
  if (!title)
    {
      g_warning ("Required property 'title' not found in group node");
      widget_group_destroy (group);
      return FALSE;
    }

  /* by default, group is expanded in palette  */
  group->expanded = TRUE;

  /* Translate it */
  translated_title = dgettext (catalog->domain, title);
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
      const gchar *node_name;
      GladeWidgetAdaptor *adaptor;
      gchar *name;

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

static void
catalog_load (GladeCatalog *catalog)
{
  GladeXmlDoc *doc;
  GladeXmlNode *root;
  GladeXmlNode *node;

  g_return_if_fail (catalog->context != NULL);

  doc = glade_xml_context_get_doc (catalog->context);
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
  g_clear_pointer (&catalog->context, glade_xml_context_free);

  return;
}

static GladeCatalog *
catalog_find_by_name (GList *catalogs, const gchar *name)
{
  if (name)
    {
      GList *l;
      for (l = catalogs; l; l = g_list_next (l))
        {
          GladeCatalog *catalog = l->data;
          if (g_strcmp0 (catalog->name, name) == 0)
            return catalog;
        }
    }
  
  return NULL;
}

static gint
catalog_name_cmp (gconstpointer a, gconstpointer b)
{
  return (a && b) ? g_strcmp0 (GLADE_CATALOG(a)->name, GLADE_CATALOG(b)->name) : 0;
}

static GList *
glade_catalog_tsort (GList *catalogs, gboolean loading)
{
  GList *l, *sorted = NULL;
  GList *deps = NULL;

  /* Sort alphabetically first */
  catalogs = g_list_sort (catalogs, catalog_name_cmp);

  /* Generate dependency graph edges */
  for (l = catalogs; l; l = g_list_next (l))
    {
      GladeCatalog *catalog = l->data, *dep;

      if (!catalog->dep_catalog)
        continue;

      if ((dep = catalog_find_by_name ((loading) ? catalogs : loaded_catalogs,
                                       catalog->dep_catalog)))
        deps = _node_edge_prepend (deps, dep, catalog);
      else
        g_critical ("Catalog %s depends on catalog %s, not found",
                    catalog->name, catalog->dep_catalog);
    }

  sorted = _glade_tsort (&catalogs, &deps);

  if (deps)
    {
      GList *l, *cycles = NULL;
      
      g_warning ("Circular dependency detected loading catalogs, they will be ignored");

      for (l = deps; l; l = g_list_next (l))
        {
          _NodeEdge *edge = l->data;

          g_message ("\t%s depends on %s",
                     GLADE_CATALOG (edge->successor)->name,
                     GLADE_CATALOG (edge->successor)->dep_catalog);
          
          if (loading && !g_list_find (cycles, edge->successor))
            cycles = g_list_prepend (cycles, edge->successor);
        }

      if (cycles)
        g_list_free_full (cycles, (GDestroyNotify) catalog_destroy);

      _node_edge_list_free (deps);
    }

  return sorted;
}

static GList *
catalogs_from_path (GList *catalogs, const gchar *path)
{
  GladeCatalog *catalog;
  GDir *dir;
  GError *error = NULL;
  const gchar *filename;

  /* Silent return if the directory didn't exist */
  if (!g_file_test (path, G_FILE_TEST_IS_DIR))
    return catalogs;

  if ((dir = g_dir_open (path, 0, &error)) != NULL)
    {
      while ((filename = g_dir_read_name (dir)))
        {
          gchar *catalog_filename;

          if (!g_str_has_suffix (filename, ".xml"))
            continue;

          /* Special case, ignore gresource files (which are present
           * while running tests)
           */
          if (g_str_has_suffix (filename, ".gresource.xml"))
            continue;

          /* If we're running in the bundle, don't ever try to load
           * anything except the GTK+ catalog
           */
          if (g_getenv (GLADE_ENV_BUNDLED) != NULL &&
              strcmp (filename, "gtk+.xml") != 0)
            continue;

          catalog_filename = g_build_filename (path, filename, NULL);
          catalog = catalog_open (catalog_filename);
          g_free (catalog_filename);

          if (catalog)
            {
              /* Verify that we are not loading the same catalog twice !
               */
              if (catalog_find_by_name (catalogs, catalog->name))
                catalog_destroy (catalog);
              else
                catalogs = g_list_prepend (catalogs, catalog);
            }
          else
            g_warning ("Unable to open the catalog file %s.\n", filename);
        }

      g_dir_close (dir);
    }
  else
    g_warning ("Failed to open catalog directory '%s': %s", path,
               error->message);


  return catalogs;
}

/**
 * glade_catalog_add_path:
 * @path: the new path containing catalogs
 * 
 * Adds a new path to the list of path to look catalogs for.
 */
void
glade_catalog_add_path (const gchar *path)
{
  g_return_if_fail (path != NULL);

  if (g_list_find_custom (catalog_paths, path, (GCompareFunc) g_strcmp0) == NULL)
    catalog_paths = g_list_append (catalog_paths, g_strdup (path));
}

/**
 * glade_catalog_remove_path:
 * @path: (nullable): the new path containing catalogs or %NULL to remove all of them
 * 
 * Remove path from the list of path to look catalogs for.
 * %NULL to remove all paths.
 */
void
glade_catalog_remove_path (const gchar *path)
{
  GList *l;
  
  if (path == NULL)
    {
      g_list_free_full (catalog_paths, g_free);
      catalog_paths = NULL;
    }
  else if ((l = g_list_find_custom (catalog_paths, path, (GCompareFunc) g_strcmp0)))
    {
      catalog_paths = g_list_remove_link (catalog_paths, l); 
    }
}

/**
 * glade_catalog_get_extra_paths:
 *
 * Returns: (element-type utf8) (transfer none): a list paths added by glade_catalog_add_path()
 */
const GList *
glade_catalog_get_extra_paths (void)
{
  return catalog_paths;
}

/**
 * glade_catalog_load_all:
 * 
 * Loads all available catalogs in the system.
 * First loads catalogs from GLADE_ENV_CATALOG_PATH,
 * then from glade_app_get_catalogs_dir() and finally from paths specified with
 * glade_catalog_add_path()
 *
 * Returns: (element-type GladeCatalog) (transfer none): the list of loaded GladeCatalog *
 */
const GList *
glade_catalog_load_all (void)
{
  GList *catalogs = NULL, *l, *adaptors;
  GladeCatalog *catalog;
  const gchar *search_path;
  gchar **split;
  GString *icon_warning = NULL;
  gint i;


  /* Make sure we don't init the catalogs twice */
  if (loaded_catalogs)
    return loaded_catalogs;

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
  if (g_getenv (GLADE_ENV_TESTING) == NULL)
    catalogs = catalogs_from_path (catalogs, glade_app_get_catalogs_dir ());

  /* And then load catalogs from extra paths */
  for (l = catalog_paths; l; l = g_list_next (l))
    catalogs = catalogs_from_path (catalogs, l->data);
  
  /* Catalogs need dependancies, most catalogs depend on
   * the gtk+ catalog, but some custom toolkits may depend
   * on the gnome catalog for instance.
   */
  catalogs = glade_catalog_tsort (catalogs, TRUE);

  /* After sorting, execute init function and then load */
  for (l = catalogs; l; l = g_list_next (l))
    {
      catalog = l->data;
      if (catalog->init_function)
        catalog->init_function (catalog->name);
    }

  for (l = catalogs; l; l = g_list_next (l))
    {
      catalog = l->data;
      catalog_load (catalog);
    }

  /* Print a summery of widget adaptors missing icons.
   */
  adaptors = glade_widget_adaptor_list_adaptors ();
  for (l = adaptors; l; l = g_list_next (l))
    {
      GladeWidgetAdaptor *adaptor = l->data;

      /* Dont print missing icons in unit tests */
      if (glade_widget_adaptor_get_missing_icon (adaptor) &&
          g_getenv (GLADE_ENV_TESTING) == NULL)
        {
          if (!icon_warning)
            icon_warning = g_string_new ("Glade needs artwork; "
                                         "a default icon will be used for "
                                         "the following classes:");

          g_string_append_printf (icon_warning,
                                  "\n\t%s\tneeds an icon named '%s'",
                                  glade_widget_adaptor_get_name (adaptor), 
                                  glade_widget_adaptor_get_missing_icon (adaptor));
        }
    }

  g_list_free (adaptors);

  if (icon_warning)
    {
      g_message ("%s", icon_warning->str);
      g_string_free (icon_warning, TRUE);
    }

  loaded_catalogs = catalogs;

  return loaded_catalogs;
}

/**
 * glade_catalog_get_name:
 * @catalog: a catalog object
 * 
 * Returns: The symbolic catalog name.
 */
const gchar *
glade_catalog_get_name (GladeCatalog *catalog)
{
  g_return_val_if_fail (GLADE_IS_CATALOG (catalog), NULL);

  return catalog->name;
}

/**
 * glade_catalog_get_prefix:
 * @catalog: a catalog object
 * 
 * Returns: The catalog path prefix.
 */
const gchar *
glade_catalog_get_prefix (GladeCatalog *catalog)
{
  g_return_val_if_fail (GLADE_IS_CATALOG (catalog), NULL);

  return catalog->prefix;
}

/**
 * glade_catalog_get_book:
 * @catalog: a catalog object
 * 
 * Returns: The Devhelp search domain.
 */
const gchar *
glade_catalog_get_book (GladeCatalog *catalog)
{
  g_return_val_if_fail (GLADE_IS_CATALOG (catalog), NULL);

  return catalog->book;
}

/**
 * glade_catalog_get_domain:
 * @catalog: a catalog object
 * 
 * Returns: The domain to be used to translate strings from this catalog
 */
const gchar *
glade_catalog_get_domain (GladeCatalog *catalog)
{
  g_return_val_if_fail (GLADE_IS_CATALOG (catalog), NULL);

  return catalog->domain;
}

/**
 * glade_catalog_get_icon_prefix:
 * @catalog: a catalog object
 * 
 * Returns: The prefix for icons.
 */
const gchar *
glade_catalog_get_icon_prefix (GladeCatalog *catalog)
{
  g_return_val_if_fail (GLADE_IS_CATALOG (catalog), NULL);

  return catalog->icon_prefix;
}

/**
 * glade_catalog_get_major_version:
 * @catalog: a catalog object
 * 
 * Returns: The catalog version
 */
guint16
glade_catalog_get_major_version (GladeCatalog *catalog)
{
  g_return_val_if_fail (GLADE_IS_CATALOG (catalog), 0);

  return catalog->major_version;
}

/**
 * glade_catalog_get_minor_version:
 * @catalog: a catalog object
 * 
 * Returns: The catalog minor version
 */
guint16
glade_catalog_get_minor_version (GladeCatalog *catalog)
{
  g_return_val_if_fail (GLADE_IS_CATALOG (catalog), 0);

  return catalog->minor_version;
}

/**
 * glade_catalog_get_targets:
 * @catalog: a catalog object
 * 
 * Returns: (transfer none) (element-type GladeTargetableVersion): the list of suitable version targets.
 */
GList *
glade_catalog_get_targets (GladeCatalog *catalog)
{
  g_return_val_if_fail (GLADE_IS_CATALOG (catalog), NULL);

  return catalog->targetable_versions;
}

/**
 * glade_catalog_get_widget_groups:
 * @catalog: a catalog object
 * 
 * Returns: (transfer none) (element-type GladeWidgetGroup): the list of widget groups (palette)
 */
GList *
glade_catalog_get_widget_groups (GladeCatalog *catalog)
{
  g_return_val_if_fail (GLADE_IS_CATALOG (catalog), NULL);

  return catalog->widget_groups;
}

/**
 * glade_catalog_get_adaptors:
 * @catalog: a catalog object
 * 
 * Returns: (transfer none) (element-type GladeWidgetAdaptor): the list of widget class adaptors
 */
GList *
glade_catalog_get_adaptors (GladeCatalog *catalog)
{
  g_return_val_if_fail (GLADE_IS_CATALOG (catalog), NULL);

  return catalog->adaptors;
}

/**
 * glade_catalog_is_loaded:
 * @name: a catalog object
 * 
 * Returns: Whether @name is loaded or not
 */
gboolean
glade_catalog_is_loaded (const gchar *name)
{
  g_return_val_if_fail (name != NULL, FALSE);
  g_assert (loaded_catalogs != NULL);
  return catalog_find_by_name (loaded_catalogs, name) != NULL;
}

/**
 * glade_catalog_destroy_all:
 * 
 * Destroy and free all resources related with every loaded catalog.
 */
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

/**
 * glade_widget_group_get_name:
 * @group: a widget group
 * 
 * Returns: the widget group name
 */
const gchar *
glade_widget_group_get_name (GladeWidgetGroup *group)
{
  g_return_val_if_fail (group != NULL, NULL);

  return group->name;
}

/**
 * glade_widget_group_get_title:
 * @group: a widget group
 * 
 * Returns: the widget group name used in the palette
 */
const gchar *
glade_widget_group_get_title (GladeWidgetGroup *group)
{
  g_return_val_if_fail (group != NULL, NULL);

  return group->title;
}

/**
 * glade_widget_group_get_expanded:
 * @group: a widget group
 * 
 * Returns: Whether group is expanded in the palette
 */
gboolean
glade_widget_group_get_expanded (GladeWidgetGroup *group)
{
  g_return_val_if_fail (group != NULL, FALSE);

  return group->expanded;
}

/**
 * glade_widget_group_get_adaptors:
 * @group: a widget group
 * 
 * Returns: (transfer none) (element-type GladeWidgetAdaptor): a list of class adaptors in the palette
 */
const GList *
glade_widget_group_get_adaptors (GladeWidgetGroup *group)
{
  g_return_val_if_fail (group != NULL, NULL);

  return group->adaptors;
}

/* Private API */

GladeCatalog *
_glade_catalog_get_catalog (const gchar *name)
{
  g_return_val_if_fail (name != NULL, NULL);
  g_assert (loaded_catalogs != NULL);

  return catalog_find_by_name (loaded_catalogs, name);
}

GList *
_glade_catalog_tsort (GList *catalogs)
{
  return glade_catalog_tsort (catalogs, FALSE);
}
