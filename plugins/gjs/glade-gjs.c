/*
 * Copyright (C) 2016 Juan Pablo Ugarte.
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
 
#ifndef STANDALONE_DEBUG
#include <config.h>
#endif

#include <gjs/gjs.h>
#include <gladeui/glade.h>

static gboolean
glade_gjs_setup ()
{
  GjsContext  *context;
  const gchar *path;
  const GList *l;
  GArray *paths;

  paths = g_array_new (TRUE, FALSE, sizeof (gchar *));

  /* GLADE_ENV_MODULE_PATH has priority */
  if ((path = g_getenv (GLADE_ENV_MODULE_PATH)))
    g_array_append_val (paths, path);

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

  return FALSE;
}

void
glade_gjs_init (const gchar *name)
{
  gchar *import_sentence, *cname;
  static gsize init = 0;
  int exit_status = 0;
  GError *error = NULL;

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
  import_sentence = g_strdup_printf ("const %s = imports.%s;", cname, name);

  /* Importing the module will create all the GTypes so that glade can use them at runtime */
  gjs_context_eval (gjs_context_get_current (),
                    import_sentence, -1, NULL,
                    &exit_status,
                    &error);

  if (error)
    {
      g_warning ("GJS module '%s' import failed: '%s' %s", name, import_sentence, error->message);
      g_error_free (error);
    }

  g_free (import_sentence);
}

#ifdef STANDALONE_DEBUG

/* gcc -DSTANDALONE_DEBUG=1 glade-gjs.c -o glade-gjs -O0 -g `pkg-config gladeui-2.0 gobject-2.0 gjs-1.0 --libs --cflags` */
int
main (int argc, char **argv)
{
  GtkWidget *win, *box, *label;

  gtk_init (&argc, &argv);
  glade_init ();

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  glade_gjs_init ("gjsplugin");

  box = g_object_new (g_type_from_name("Gjs_MyBox"), NULL);

  label = gtk_label_new ("This is a regular GtkLabel inside a MyBox GJS object");

  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_container_add (GTK_CONTAINER (win), box);

  gtk_widget_show_all (GTK_WIDGET (win));

  gtk_main ();

  return 0;
}

#endif

