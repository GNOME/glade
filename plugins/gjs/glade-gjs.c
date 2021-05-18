/*
 * Copyright (C) 2016 Endless Mobile Inc.
 *               2020 Juan Pablo Ugarte.
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
 * Authors:
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */
 
#include <config.h>
#include <gjs/gjs.h>
#include <gladeui/glade.h>

static gboolean
glade_gjs_setup ()
{
  GjsContext  *context;
  gchar **tokens = NULL;
  const gchar *path;
  const GList *l;
  GArray *paths;

  paths = g_array_new (TRUE, FALSE, sizeof (gchar *));

  /* GLADE_ENV_MODULE_PATH has priority */
  if ((path = g_getenv (GLADE_ENV_MODULE_PATH)))
    {
      tokens = g_strsplit(path, ":", -1);
      gint i;

      for (i = 0; tokens[i]; i++)
        g_array_append_val (paths, tokens[i]);
    }


  /* Append modules directory */
  if ((path = glade_app_get_modules_dir ()))
    g_array_append_val (paths, path);

  /* Append extra paths (declared in the Preferences) */
  for (l = glade_catalog_get_extra_paths (); l; l = g_list_next (l))
    g_array_append_val (paths, l->data);

  /* Create new JS context and set it as default if needed */
  context = gjs_context_new_with_search_path ((gchar **) paths->data);
  if (gjs_context_get_current() != context)
    gjs_context_make_current (context);

  g_object_ref_sink (context);

  g_array_free (paths, TRUE);
  g_strfreev (tokens);

  return FALSE;
}

void
glade_gjs_init (const gchar *name)
{
  gchar *import_sentence, *cname;
  static gsize init = 0;
  int exit_status = 0;
  GError *error = NULL;
  gboolean retval;

  if (g_once_init_enter (&init))
    {
      if (glade_gjs_setup ())
        return;

      g_once_init_leave (&init, TRUE);
    }

  cname = g_strdup (name);
  if (cname[0])
    cname[0] = g_ascii_toupper (cname[0]);

  /* Yeah, we use the catalog name as the library */
  import_sentence = g_strdup_printf ("imports.gi.versions.Gtk = \"3.0\";\nconst %s = imports.%s;", cname, name);

  /* Importing the module will create all the GTypes so that glade can use them at runtime */
  retval = gjs_context_eval (gjs_context_get_current (),
                             import_sentence, -1, "<glade-gjs>",
                             &exit_status,
                             &error);
  if (!retval && error)
    {
      g_warning ("GJS module '%s' import failed: '%s' %s", name, import_sentence, error->message);
      g_error_free (error);
    }

  g_free (import_sentence);
}

