/*
 * glade-template.c:
 *
 * Copyright (C) 2017-2020 Juan Pablo Ugarte.
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

#include "glade-private.h"
#include "glade-utils.h"

#if HAVE_GTK_TEMPLATE_UNSET
extern void
_gtk_widget_class_template_unset_only_for_glade (GtkWidgetClass *widget_class);
#else
#define _gtk_widget_class_template_unset_only_for_glade(void)
#endif

static GHashTable *templates = NULL;

static void
glade_template_instance_init (GTypeInstance *instance, gpointer g_class)
{
  /* Reset class template */
  _gtk_widget_class_template_unset_only_for_glade (GTK_WIDGET_GET_CLASS (instance));
  gtk_widget_class_set_template (GTK_WIDGET_GET_CLASS (instance), 
                                 g_hash_table_lookup (templates,
                                 G_OBJECT_TYPE_NAME (instance)));

  /* Regular template initialization*/
  gtk_widget_init_template ((GtkWidget *)instance);
}

typedef struct {
  gchar *class;
  gchar *parent;
} ParserData;

static void
start_element (GMarkupParseContext *context,
               const gchar         *element_name,
               const gchar        **attribute_names,
               const gchar        **attribute_values,
               gpointer             user_data,
               GError             **error)
{
  ParserData *data = user_data;
  gint i;

  if (!g_str_equal (element_name, "template"))
    return;

  for (i = 0; attribute_names[i]; i++)
    if (g_str_equal (attribute_names[i], "class"))
      data->class = g_strdup (attribute_values[i]);
    else if (g_str_equal (attribute_names[i], "parent"))
      data->parent = g_strdup (attribute_values[i]);
}

gboolean
_glade_template_parse (const gchar *tmpl, gchar **type, gchar **parent)
{
  GMarkupParser parser = { start_element, NULL, };
  GMarkupParseContext *context;
  ParserData data = { 0, };

  context = g_markup_parse_context_new (&parser, 0, &data, NULL);

  g_markup_parse_context_parse (context, tmpl, -1, NULL);

  /*
  while (g_markup_parse_context_parse (context, tmpl, 128, NULL) &&
         (data.class == NULL || data.parent == NULL))
    tmpl += 128;
*/

  g_markup_parse_context_end_parse (context, NULL);

  if (data.class && data.parent)
    {
      *type = data.class;
      *parent = data.parent;

      return TRUE;
    }

  g_free (data.class);
  g_free (data.parent);

  return FALSE;
}

static GType
get_type_from_name (const gchar *name)
{
  g_autofree gchar *func_name = NULL;
  static GModule *allsymbols = NULL;
  GType (*get_type) (void);
  GType type = 0;

  if (g_once_init_enter (&allsymbols))
    {
      GModule *symbols = g_module_open (NULL, 0);
      g_once_init_leave (&allsymbols, symbols);
    }

  if ((type = g_type_from_name (name)) == 0 &&
      ((func_name = _glade_util_compose_get_type_func (name)) != NULL))
    {
      if (g_module_symbol (allsymbols, func_name, (gpointer) & get_type))
        {
          g_assert (get_type);
          type = get_type ();
        }
    }

  return type;
}

gchar *
_glade_template_load (const gchar *filename, gchar **type, gchar **parent)
{
  g_autoptr(GError) error = NULL;
  gchar *template = NULL;
  gsize len = 0;

  g_return_val_if_fail (filename != NULL, NULL);

  g_file_get_contents (filename, &template, &len, &error);

  if (error)
    g_warning ("Error loading template file %s - %s", filename, error->message);

  if (!template || !_glade_template_parse (template, type, parent))
    *type = *parent = NULL;
  else
    {
      GType tmpl_type = get_type_from_name (*type);

      /* Type is already registered, do not try to load it as a template */
      if (tmpl_type != G_TYPE_INVALID)
        {
          g_clear_pointer (type, g_free);
          g_clear_pointer (parent, g_free);
          g_free (template);
          return NULL;
        }

      if (g_once_init_enter (&templates))
        {
          GHashTable *table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                     (GDestroyNotify) g_free,
                                                     (GDestroyNotify) g_bytes_unref);
          g_once_init_leave (&templates, table);
        }

      /* Add template to global hash table */
      g_hash_table_insert (templates,
                           g_strdup (*type),
                           g_bytes_new_take (template, len));
    }

  return template;
}

GType
_glade_template_generate_type (const gchar *type, const gchar *parent)
{
  GType parent_type;
  GTypeQuery query;
  GTypeInfo *info;

  /* make sure the template is loaded */
  g_return_val_if_fail (g_hash_table_lookup (templates, type) != NULL, G_TYPE_INVALID);

  parent_type = glade_util_get_type_from_name (parent, FALSE);
  g_return_val_if_fail (parent_type != 0, 0);

  g_type_query (parent_type, &query);
  g_return_val_if_fail (query.type != 0, 0);

  info = g_new0 (GTypeInfo, 1);
  info->class_size = query.class_size;
  info->instance_size = query.instance_size;
  info->instance_init = glade_template_instance_init;

  return g_type_register_static (parent_type, type, info, 0);
}

const gchar *
_glade_template_lookup (const gchar *type)
{
  GBytes *tmpl = g_hash_table_lookup (templates, type);
  return (tmpl) ? g_bytes_get_data (tmpl, NULL) : "";
}

