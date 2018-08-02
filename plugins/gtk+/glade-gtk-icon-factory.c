/*
 * glade-gtk-icon-factory.c - GladeWidgetAdaptor for GtkIconFactory
 *
 * Copyright (C) 2013 Tristan Van Berkom
 *
 * Authors:
 *      Tristan Van Berkom <tristan.van.berkom@gmail.com>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include <glib/gi18n-lib.h>
#include <gladeui/glade.h>

#include "glade-icon-sources.h"
#include "glade-icon-factory-editor.h"

#define GLADE_TAG_SOURCES   "sources"
#define GLADE_TAG_SOURCE    "source"

#define GLADE_TAG_STOCK_ID  "stock-id"
#define GLADE_TAG_FILENAME  "filename"
#define GLADE_TAG_DIRECTION "direction"
#define GLADE_TAG_STATE     "state"
#define GLADE_TAG_SIZE      "size"

void
glade_gtk_icon_factory_post_create (GladeWidgetAdaptor *adaptor,
                                    GObject            *object,
                                    GladeCreateReason   reason)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_icon_factory_add_default (GTK_ICON_FACTORY (object));
G_GNUC_END_IGNORE_DEPRECATIONS
}

void
glade_gtk_icon_factory_destroy_object (GladeWidgetAdaptor *adaptor,
                                       GObject            *object)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gtk_icon_factory_remove_default (GTK_ICON_FACTORY (object));
G_GNUC_END_IGNORE_DEPRECATIONS

  GWA_GET_CLASS (G_TYPE_OBJECT)->destroy_object (adaptor, object);
}

static void
glade_gtk_icon_factory_read_sources (GladeWidget *widget, GladeXmlNode *node)
{
  GladeIconSources *sources;
  GtkIconSource *source;
  GladeXmlNode *sources_node, *source_node;
  GValue *value;
  GList *list;
  gchar *current_icon_name = NULL;
  GdkPixbuf *pixbuf;

  if ((sources_node = glade_xml_search_child (node, GLADE_TAG_SOURCES)) == NULL)
    return;

  sources = glade_icon_sources_new ();

  /* Here we expect all icon sets to remain together in the list. */
  for (source_node = glade_xml_node_get_children (sources_node); source_node;
       source_node = glade_xml_node_next (source_node))
    {
      gchar *icon_name;
      gchar *str;

      if (!glade_xml_node_verify (source_node, GLADE_TAG_SOURCE))
        continue;

      if (!(icon_name =
            glade_xml_get_property_string_required (source_node,
                                                    GLADE_TAG_STOCK_ID, NULL)))
        continue;

      if (!
          (str =
           glade_xml_get_property_string_required (source_node,
                                                   GLADE_TAG_FILENAME, NULL)))
        {
          g_free (icon_name);
          continue;
        }

      if (!current_icon_name || strcmp (current_icon_name, icon_name) != 0)
        current_icon_name = (g_free (current_icon_name), g_strdup (icon_name));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      source = gtk_icon_source_new ();
G_GNUC_END_IGNORE_DEPRECATIONS

      /* Deal with the filename... */
      value = glade_utils_value_from_string (GDK_TYPE_PIXBUF, str, glade_widget_get_project (widget));
      pixbuf = g_value_dup_object (value);
      g_value_unset (value);
      g_free (value);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      gtk_icon_source_set_pixbuf (source, pixbuf);
G_GNUC_END_IGNORE_DEPRECATIONS
      g_object_unref (G_OBJECT (pixbuf));
      g_free (str);

      /* Now the attributes... */
      if ((str =
           glade_xml_get_property_string (source_node,
                                          GLADE_TAG_DIRECTION)) != NULL)
        {
          GtkTextDirection direction =
              glade_utils_enum_value_from_string (GTK_TYPE_TEXT_DIRECTION, str);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
          gtk_icon_source_set_direction_wildcarded (source, FALSE);
          gtk_icon_source_set_direction (source, direction);
G_GNUC_END_IGNORE_DEPRECATIONS
          g_free (str);
        }

      if ((str =
           glade_xml_get_property_string (source_node, GLADE_TAG_SIZE)) != NULL)
        {
          GtkIconSize size =
              glade_utils_enum_value_from_string (GTK_TYPE_ICON_SIZE, str);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
          gtk_icon_source_set_size_wildcarded (source, FALSE);
          gtk_icon_source_set_size (source, size);
G_GNUC_END_IGNORE_DEPRECATIONS
          g_free (str);
        }

      if ((str =
           glade_xml_get_property_string (source_node,
                                          GLADE_TAG_STATE)) != NULL)
        {
          GtkStateType state =
              glade_utils_enum_value_from_string (GTK_TYPE_STATE_TYPE, str);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
          gtk_icon_source_set_state_wildcarded (source, FALSE);
          gtk_icon_source_set_state (source, state);
G_GNUC_END_IGNORE_DEPRECATIONS
          g_free (str);
        }

      if ((list =
           g_hash_table_lookup (sources->sources,
                                g_strdup (current_icon_name))) != NULL)
        {
          GList *new_list = g_list_append (list, source);

          /* Warning: if we use g_list_prepend() the returned pointer will be different
           * so we would have to replace the list pointer in the hash table.
           * But before doing that we have to steal the old list pointer otherwise
           * we would have to make a copy then add the new icon to finally replace the hash table
           * value.
           * Anyways if we choose to prepend we would have to reverse the list outside this loop
           * so its better to append.
           */
          if (new_list != list)
            {
              /* current g_list_append() returns the same pointer so this is not needed */
              g_hash_table_steal (sources->sources, current_icon_name);
              g_hash_table_insert (sources->sources,
                                   g_strdup (current_icon_name), new_list);
            }
        }
      else
        {
          list = g_list_append (NULL, source);
          g_hash_table_insert (sources->sources, g_strdup (current_icon_name),
                               list);
        }
    }

  if (g_hash_table_size (sources->sources) > 0)
    glade_widget_property_set (widget, "sources", sources);

  glade_icon_sources_free (sources);
}

void
glade_gtk_icon_factory_read_widget (GladeWidgetAdaptor *adaptor,
                                    GladeWidget        *widget,
                                    GladeXmlNode       *node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in any normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->read_widget (adaptor, widget, node);

  glade_gtk_icon_factory_read_sources (widget, node);
}

typedef struct
{
  GladeXmlContext *context;
  GladeXmlNode *node;
} SourceWriteTab;

static void
write_icon_sources (gchar *icon_name, GList *sources, SourceWriteTab *tab)
{
  GladeXmlNode *source_node;
  GtkIconSource *source;
  GList *l;
  gchar *string;

  GdkPixbuf *pixbuf;

  for (l = sources; l; l = l->next)
    {
      source = l->data;

      source_node = glade_xml_node_new (tab->context, GLADE_TAG_SOURCE);
      glade_xml_node_append_child (tab->node, source_node);

      glade_xml_node_set_property_string (source_node, GLADE_TAG_STOCK_ID,
                                          icon_name);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      if (!gtk_icon_source_get_direction_wildcarded (source))
        {
          GtkTextDirection direction = gtk_icon_source_get_direction (source);
G_GNUC_END_IGNORE_DEPRECATIONS
          string =
              glade_utils_enum_string_from_value (GTK_TYPE_TEXT_DIRECTION,
                                                  direction);
          glade_xml_node_set_property_string (source_node, GLADE_TAG_DIRECTION,
                                              string);
          g_free (string);
        }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      if (!gtk_icon_source_get_size_wildcarded (source))
        {
          GtkIconSize size = gtk_icon_source_get_size (source);
G_GNUC_END_IGNORE_DEPRECATIONS
          string =
              glade_utils_enum_string_from_value (GTK_TYPE_ICON_SIZE, size);
          glade_xml_node_set_property_string (source_node, GLADE_TAG_SIZE,
                                              string);
          g_free (string);
        }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      if (!gtk_icon_source_get_state_wildcarded (source))
        {
          GtkStateType state = gtk_icon_source_get_state (source);
G_GNUC_END_IGNORE_DEPRECATIONS
          string =
              glade_utils_enum_string_from_value (GTK_TYPE_STATE_TYPE, state);
          glade_xml_node_set_property_string (source_node, GLADE_TAG_STATE,
                                              string);
          g_free (string);
        }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      pixbuf = gtk_icon_source_get_pixbuf (source);
G_GNUC_END_IGNORE_DEPRECATIONS
      string = g_object_get_data (G_OBJECT (pixbuf), "GladeFileName");

      glade_xml_node_set_property_string (source_node,
                                          GLADE_TAG_FILENAME, string);
    }
}


static void
glade_gtk_icon_factory_write_sources (GladeWidget     *widget,
                                      GladeXmlContext *context,
                                      GladeXmlNode    *node)
{
  GladeXmlNode *sources_node;
  GladeIconSources *sources = NULL;
  SourceWriteTab tab;

  glade_widget_property_get (widget, "sources", &sources);
  if (!sources)
    return;

  sources_node = glade_xml_node_new (context, GLADE_TAG_SOURCES);

  tab.context = context;
  tab.node = sources_node;
  g_hash_table_foreach (sources->sources, (GHFunc) write_icon_sources, &tab);

  if (!glade_xml_node_get_children (sources_node))
    glade_xml_node_delete (sources_node);
  else
    glade_xml_node_append_child (node, sources_node);

}


void
glade_gtk_icon_factory_write_widget (GladeWidgetAdaptor *adaptor,
                                     GladeWidget        *widget,
                                     GladeXmlContext    *context,
                                     GladeXmlNode       *node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and write all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);

  glade_gtk_icon_factory_write_sources (widget, context, node);
}

static void
apply_icon_sources (gchar *icon_name, GList *sources, GtkIconFactory *factory)
{
  GtkIconSource *source;
  GtkIconSet *set;
  GList *l;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  set = gtk_icon_set_new ();

  for (l = sources; l; l = l->next)
    {
      source = gtk_icon_source_copy ((GtkIconSource *) l->data);
      gtk_icon_set_add_source (set, source);
    }

  gtk_icon_factory_add (factory, icon_name, set);
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
glade_gtk_icon_factory_set_sources (GObject *object, const GValue *value)
{
  GladeIconSources *sources = g_value_get_boxed (value);
  if (sources)
    g_hash_table_foreach (sources->sources, (GHFunc) apply_icon_sources,
                          object);
}


void
glade_gtk_icon_factory_set_property (GladeWidgetAdaptor *adaptor,
                                     GObject            *object,
                                     const gchar        *property_name,
                                     const GValue       *value)
{
  if (strcmp (property_name, "sources") == 0)
    {
      glade_gtk_icon_factory_set_sources (object, value);
    }
  else
    /* Chain Up */
    GWA_GET_CLASS (G_TYPE_OBJECT)->set_property (adaptor,
                                                 object, property_name, value);
}

static void
serialize_icon_sources (gchar *icon_name, GList *sources, GString *string)
{
  GList *l;

  for (l = sources; l; l = g_list_next (l))
    {
      GtkIconSource *source = l->data;
      GdkPixbuf *pixbuf;
      gchar *str;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      pixbuf = gtk_icon_source_get_pixbuf (source);
G_GNUC_END_IGNORE_DEPRECATIONS
      str = g_object_get_data (G_OBJECT (pixbuf), "GladeFileName");

      g_string_append_printf (string, "%s[%s] ", icon_name, str);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      if (!gtk_icon_source_get_direction_wildcarded (source))
        {
          GtkTextDirection direction = gtk_icon_source_get_direction (source);
G_GNUC_END_IGNORE_DEPRECATIONS
          str =
              glade_utils_enum_string_from_value (GTK_TYPE_TEXT_DIRECTION,
                                                  direction);
          g_string_append_printf (string, "dir-%s ", str);
          g_free (str);
        }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      if (!gtk_icon_source_get_size_wildcarded (source))
        {
          GtkIconSize size = gtk_icon_source_get_size (source);
G_GNUC_END_IGNORE_DEPRECATIONS
          str = glade_utils_enum_string_from_value (GTK_TYPE_ICON_SIZE, size);
          g_string_append_printf (string, "size-%s ", str);
          g_free (str);
        }

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      if (!gtk_icon_source_get_state_wildcarded (source))
        {
          GtkStateType state = gtk_icon_source_get_state (source);
G_GNUC_END_IGNORE_DEPRECATIONS
          str = glade_utils_enum_string_from_value (GTK_TYPE_STATE_TYPE, state);
          g_string_append_printf (string, "state-%s ", str);
          g_free (str);
        }

      g_string_append_printf (string, "| ");
    }
}

gchar *
glade_gtk_icon_factory_string_from_value (GladeWidgetAdaptor *adaptor,
                                          GladePropertyClass *klass,
                                          const GValue       *value)
{
  GString *string;
  GParamSpec *pspec;

  pspec = glade_property_class_get_pspec (klass);

  if (pspec->value_type == GLADE_TYPE_ICON_SOURCES)
    {
      GladeIconSources *sources = g_value_get_boxed (value);
      if (!sources)
        return g_strdup ("");

      string = g_string_new ("");
      g_hash_table_foreach (sources->sources, (GHFunc) serialize_icon_sources,
                            string);

      return g_string_free (string, FALSE);
    }
  else
    return GWA_GET_CLASS
        (G_TYPE_OBJECT)->string_from_value (adaptor, klass, value);
}


GladeEditorProperty *
glade_gtk_icon_factory_create_eprop (GladeWidgetAdaptor *adaptor,
                                     GladePropertyClass *klass,
                                     gboolean            use_command)
{
  GladeEditorProperty *eprop;
  GParamSpec          *pspec;

  pspec = glade_property_class_get_pspec (klass);

  if (pspec->value_type == GLADE_TYPE_ICON_SOURCES)
    eprop = g_object_new (GLADE_TYPE_EPROP_ICON_SOURCES,
                          "property-class", klass,
                          "use-command", use_command, NULL);
  else
    eprop = GWA_GET_CLASS
        (G_TYPE_OBJECT)->create_eprop (adaptor, klass, use_command);
  return eprop;
}

GladeEditable *
glade_gtk_icon_factory_create_editable (GladeWidgetAdaptor *adaptor,
                                        GladeEditorPageType type)
{
  GladeEditable *editable;

  /* Get base editable */
  editable = GWA_GET_CLASS (G_TYPE_OBJECT)->create_editable (adaptor, type);

  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_icon_factory_editor_new (adaptor, editable);

  return editable;
}
