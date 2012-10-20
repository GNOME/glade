/*
 * glade-composite-template.c
 *
 * Copyright (C) 2012 Juan Pablo Ugarte
 *
 * Author: Juan Pablo Ugarte <juanpablougarte@gmail.com>
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
 */

#include <config.h>

#include <glib/gi18n-lib.h>
#include "glade-composite-template.h"
#include "glade-app.h"
#include "glade-utils.h"

typedef struct
{
  gboolean right_id;
  GType parent;
  const gchar *type_name;
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

  if (g_strcmp0 (element_name, "template") == 0)
    {
      gint i;

      for (i = 0; attribute_names[i]; i++)
        {
          if (!g_strcmp0 (attribute_names[i], "parent"))
            state->parent = glade_util_get_type_from_name (attribute_values[i], FALSE);
          else if (!g_strcmp0 (attribute_names[i], "class"))
            state->type_name = g_intern_string (attribute_values[i]);
	  else if (!g_strcmp0 (attribute_names[i], "id"))
	    state->right_id = (g_strcmp0 (attribute_values[i], "this") == 0);
        }
    }
}

static gboolean
parse_template (const gchar *template_str, GType *parent, const gchar **type_name)
{
  GMarkupParser parser = { start_element };
  ParseData state = { FALSE, G_TYPE_INVALID, NULL };
  GMarkupParseContext *context;

  context = g_markup_parse_context_new (&parser,
                                        G_MARKUP_TREAT_CDATA_AS_TEXT |
                                        G_MARKUP_PREFIX_ERROR_POSITION,
                                        &state, NULL);

  g_markup_parse_context_parse (context, template_str, -1, NULL);
  g_markup_parse_context_end_parse (context, NULL);
  g_markup_parse_context_free (context);
  
  if (!g_type_is_a (state.parent, GTK_TYPE_CONTAINER))
    {
      g_warning ("Composite templates should derive from GtkContainer");
      return FALSE;
    }
  
  if (parent) *parent = state.parent;
  if (type_name) *type_name = state.type_name;

  return state.right_id;
}

static void
composite_template_derived_init (GTypeInstance *instance, gpointer g_class)
{
}

#if !HAVE_GTK_CONTAINER_CLASS_SET_TEMPLATE_FROM_STRING
static GObject *
composite_template_constructor (GType type,
                                guint n_construct_properties,
                                GObjectConstructParam *construct_properties)
{
  GladeWidgetAdaptor *adaptor;
  GObjectClass *parent_class;
  GObject *obj;

  parent_class = g_type_class_peek (g_type_parent (type));
  obj = parent_class->constructor (type,
                                   n_construct_properties,
                                   construct_properties); 
/*
  adaptor = glade_widget_adaptor_get_by_type (type);


  glade_widget_adaptor_get_*/
  return obj;
}
#endif

static void
composite_template_derived_class_init (gpointer g_class, gpointer class_data)
{
#if !HAVE_GTK_CONTAINER_CLASS_SET_TEMPLATE_FROM_STRING
  GObjectClass *object_class = g_class;
  object_class->constructor = composite_template_constructor;
#endif
}

static inline GType
generate_type (GType parent, const gchar *type_name)
{
  GTypeQuery query;

  g_type_query (parent, &query);

  return g_type_register_static_simple (parent, type_name,
                                        query.class_size,
                                        composite_template_derived_class_init,
                                        query.instance_size,
                                        composite_template_derived_init,
                                        0);
}

/* Public API */

/**
 * glade_composite_template_load_from_string:
 * @template_xml: a #GtkBuilder UI description string
 * 
 * This function will create a new GType from the template UI description defined
 * by @template_xml and its corresponding #GladeWidgetAdator
 * 
 * Returns: A newlly created and registered #GladeWidgetAdptor or NULL if @template_xml is malformed.
 */
GladeWidgetAdaptor *
glade_composite_template_load_from_string (const gchar *template_xml)
{
  const gchar *type_name = NULL;
  GType parent;

  g_return_val_if_fail (template_xml != NULL, NULL);

  if (parse_template (template_xml, &parent, &type_name))
    {
      GladeWidgetAdaptor *adaptor;
      GType template_type;

      /* Generate dummy template type */
      template_type = generate_type (parent, type_name);
      
      /* Create adaptor for template */
      adaptor = glade_widget_adaptor_from_composite_template (template_type,
                                                              template_xml,
                                                              type_name,
                                                              NULL); /* TODO: generate icon name from parent icon plus some emblem */                        
      /* Register adaptor */
      glade_widget_adaptor_register (adaptor);

      return adaptor;
    }
  else
    g_warning ("Could not parse template");

  return NULL;
}

/**
 * glade_composite_template_load_from_file:
 * @path: a filename to load
 * 
 * Loads a composite template from a file.
 * See glade_composite_template_load_from_string() for details.
 * 
 * Returns: A newlly created and registered #GladeWidgetAdaptor or NULL.
 */
GladeWidgetAdaptor *
glade_composite_template_load_from_file (const gchar *path)
{
  GladeWidgetAdaptor *adaptor;
  GError *error = NULL;
  gchar *contents;

  g_return_val_if_fail (path != NULL, NULL);
  
  if (!g_file_get_contents (path, &contents, NULL, &error))
    {
      g_warning ("Could not load template `%s` %s", path, error->message);
      g_error_free (error);
      return NULL;
    }
  
  if ((adaptor = glade_composite_template_load_from_string (contents)))
    g_object_set (adaptor, "template-path", path, NULL);
  
  g_free (contents);

  return adaptor;
}

/**
 * glade_composite_template_load_directory:
 * @directory: a directory path.
 * 
 * Loads every .ui composite template found in @directory
 */
void
glade_composite_template_load_directory (const gchar *directory)
{
  GError *error = NULL;
  const gchar *name;
  GDir *dir;

  g_return_if_fail (directory != NULL);

  if (!(dir = g_dir_open (directory, 0, &error)))
    {
      g_warning ("Could not open directory `%s` %s", directory, error->message);
      g_error_free (error);
      return;
    }

  while ((name = g_dir_read_name (dir)))
    {
      if (g_str_has_suffix (name, ".ui"))
	{
	  gchar *fullname = g_build_filename (directory, name, NULL);
	  glade_composite_template_load_from_file (fullname);
	  g_free (fullname);
	}
    }

  g_dir_close (dir);
}

/**
 * glade_composite_template_save_from_widget:
 * @gwidget: a #GladeWidget
 * @template_class: the name of the new composite template class
 * @filename: a file name to save the template
 * @replace: True if you want to replace @gwidget with a new widget of type @template_class
 * 
 * Saves a copy of @gwidget as a composite template in @filename with @template_class
 * as the class name
 */
void
glade_composite_template_save_from_widget (GladeWidget *gwidget,
                                           const gchar *template_class,
                                           const gchar *filename,
                                           gboolean replace)
{
  GladeProject *project;
  gchar *template_xml;
  GladeWidget *dup;
  
  g_return_if_fail (GLADE_IS_WIDGET (gwidget));
  g_return_if_fail (template_class && filename);
  g_return_if_fail (GTK_IS_CONTAINER (glade_widget_get_object (gwidget)));

  project = glade_project_new ();
  dup = glade_widget_dup (gwidget, TRUE);

  /* make dupped widget a template */
  glade_widget_set_name (dup, "this");
  glade_widget_set_template_class (dup, template_class);
  
  glade_project_add_object (project, glade_widget_get_object (dup));
  template_xml = glade_project_dump_string (project);

  g_file_set_contents (filename, template_xml, -1, NULL);

  if (replace)
    {
      GladeProject *project = glade_widget_get_project (gwidget);
      GladeWidget *parent = glade_widget_get_parent (gwidget);
      GladeWidgetAdaptor *new_adaptor;
      GList widgets = {0, };
      
      /* Create it at run time */
      if ((new_adaptor = glade_composite_template_load_from_string (template_xml)))
        g_object_set (new_adaptor, "template-path", filename, NULL);

      glade_command_push_group (_("Create new composite type %s"), template_class);
      widgets.data = gwidget;
      glade_command_cut (&widgets);
      glade_command_create (new_adaptor, parent, NULL, project);
      glade_command_pop_group ();
    }
  
  g_free (template_xml);
  g_object_unref (project);
}
