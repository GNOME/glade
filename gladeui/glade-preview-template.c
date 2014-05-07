/*
 * glade-preview-template.c
 *
 * Copyright (C) 2013 Juan Pablo Ugarte
   *
 * Author: Juan Pablo Ugarte <juanpablougarte@gmail.com>
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
 *
 */

#include <config.h>

#include <string.h>
#include "glade-utils.h"
#include "glade-preview-template.h"

typedef struct
{
  GTypeInfo info;
  GString *template_string;
  GBytes *template_data;
  gint count;
} TypeData;

/* We need to call gtk_widget_init_template() in the instance init for the
* template to work.
*/
static void
template_init (GTypeInstance *instance, gpointer g_class)
{  
  gtk_widget_init_template (GTK_WIDGET (instance));
}

/* Need to associate the class with a template */
static void
template_class_init (gpointer g_class, gpointer user_data)
{
  TypeData *data = user_data;
  gtk_widget_class_set_template (g_class, data->template_data);
}

static GQuark type_data_quark = 0;

static GType
template_generate_type (const gchar *name,
                        const gchar *parent_name,
                        GString *template_string)
{
  GType parent_type, retval;
  gchar *real_name = NULL;
  GTypeQuery query;
  TypeData *data;

  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (parent_name != NULL, 0);

  parent_type = glade_util_get_type_from_name (parent_name, FALSE);
  g_return_val_if_fail (parent_type != 0, 0);

  if ((retval = g_type_from_name (name)) &&
      (data = g_type_get_qdata (retval, type_data_quark)))
    {
      /* Type already registered! reuse TypeData
       * 
       * If the template and parent class are the same there is no need to
       * register a new type
       */
      if (g_type_parent (retval) == parent_type &&
          template_string->len == data->template_string->len && 
          g_strcmp0 (template_string->str, data->template_string->str) == 0)
        return retval;

      real_name = g_strdup_printf ("GladePreviewTemplate_%s_%d", name, data->count);
    }
  else
    {
      /* We only allocate a TypeData struct once for each type class */
      data = g_new0 (TypeData, 1);
    }

  g_type_query (parent_type, &query);
  g_return_val_if_fail (query.type != 0, 0);

  /* Free old template string */
  if (data->template_string)
    g_string_free (data->template_string, TRUE);

  /* And old bytes reference to template string */
  if (data->template_data)
    g_bytes_unref (data->template_data);

  /* Take ownership, will be freed next time we want to create this type */
  data->template_string = template_string;

  data->info.class_size = query.class_size;
  data->info.instance_size = query.instance_size;
  data->info.class_init = template_class_init;
  data->info.instance_init = template_init;
  data->info.class_data = data;
  data->template_data = g_bytes_new_static (template_string->str, template_string->len);

  retval = g_type_register_static (parent_type, real_name ? real_name : name, &data->info, 0);

  /* bind TypeData struct with GType */
  if (!data->count)
    g_type_set_qdata (retval, type_data_quark, data);

  data->count++;

  g_free (real_name);

  return retval;
}

typedef struct
{
  gboolean is_template;
  GString *xml;
  gchar *klass, *parent;
  gint indent;
} ParseData;

static void
start_element (GMarkupParseContext  *context,
               const gchar          *element_name,
               const gchar         **attribute_names,
               const gchar         **attribute_values,
               gpointer              user_data,
               GError              **error)
{
  ParseData *state = user_data;
  gboolean is_template = FALSE;
  gint i;

  g_string_append_printf (state->xml, "<%s", element_name);

  if (g_strcmp0 (element_name, "template") == 0)
    state->is_template = is_template = TRUE;

  for (i = 0; attribute_names[i]; i++)
    {
      gchar *escaped_value = g_markup_escape_text (attribute_values[i], -1);

      if (is_template)
        {
          if (!g_strcmp0 (attribute_names[i], "class"))
            {
              TypeData *data;
              GType type;

              state->klass = g_strdup (attribute_values[i]);

              /* If we are in a template then we need to replace the class with
               * the fake name template_generate_type() will use to register
                 * a new class
               */
              if ((type = g_type_from_name (state->klass)) &&
                  (data = g_type_get_qdata (type, type_data_quark)))
                {
                  g_free (escaped_value);
                  escaped_value = g_strdup_printf ("GladePreviewTemplate_%s_%d", state->klass, data->count);
                }
            }
          else if (!g_strcmp0 (attribute_names[i], "parent"))
            state->parent = g_strdup (attribute_values[i]);
        }

      g_string_append_printf (state->xml, " %s=\"%s\"",
                              attribute_names[i], escaped_value);
      g_free (escaped_value);
    }

  g_string_append (state->xml, ">");
}

static void
end_element (GMarkupParseContext *context,
             const gchar         *element_name,
             gpointer             user_data,
             GError             **error)
{
  ParseData *state = user_data;
  g_string_append_printf (state->xml, "</%s>", element_name);
}

static void
text (GMarkupParseContext *context,
      const gchar         *text,
      gsize                text_len,
      gpointer             user_data,
      GError             **error)
{
  gchar *escaped_text = g_markup_escape_text (text, text_len);
  ParseData *state = user_data;

  g_string_append_len (state->xml, escaped_text, -1);

  g_free (escaped_text);
}

static void
passthrough (GMarkupParseContext *context,
             const gchar         *text,
             gsize                text_len,
             gpointer             user_data,
             GError             **error)
{
  ParseData *state = user_data;
  g_string_append_len (state->xml, text, text_len);
  g_string_append_c (state->xml, '\n');
}

GObject *
glade_preview_template_object_new (const gchar *template_data, gsize len)
{
  GMarkupParser parser = { start_element, end_element, text, passthrough};
  ParseData state = { FALSE, NULL};
  GMarkupParseContext *context;
  GObject *object = NULL;

  if (type_data_quark == 0)
    type_data_quark = g_quark_from_string ("glade-preview-type-data");

  if (len == -1)
    len = strlen (template_data);

  /* Preallocate enough for the string plus the new fake template name */
  state.xml = g_string_sized_new (len + 32);

  context = g_markup_parse_context_new (&parser,
                                        G_MARKUP_TREAT_CDATA_AS_TEXT |
                                        G_MARKUP_PREFIX_ERROR_POSITION,
                                        &state, NULL);

  if (g_markup_parse_context_parse (context, template_data, len, NULL) &&
      g_markup_parse_context_end_parse (context, NULL) && 
      state.is_template)
    {
      GType template_type = template_generate_type (state.klass,
                                                    state.parent,
                                                    state.xml);
      if (template_type)
        object = g_object_new (template_type, NULL);
      else
        {
          /* on success template_generate_type() takes ownership of xml */
          g_string_free (state.xml, TRUE);
        }
    }
  else
    g_string_free (state.xml, TRUE);

  g_free (state.klass);
  g_free (state.parent);
  g_markup_parse_context_free (context);

  return object ? g_object_ref_sink (object) : NULL;
}
