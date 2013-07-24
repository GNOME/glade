/*
 * Copyright (C) 2010 Marco Diego Aurélio Mesquita
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Marco Diego Aurélio Mesquita <marcodiegomesquita@gmail.com>
 */

#include <config.h>

#include <gladeui/glade.h>

#include <stdlib.h>
#include <locale.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "glade-preview-window.h"
#include "glade-preview-tokens.h"

typedef struct
{
  GladePreviewWindow *window;
  gchar *file_name, *toplevel;
} GladePreviewer;

static GtkWidget *
get_toplevel (GtkBuilder *builder, gchar *name)
{
  GtkWidget *toplevel = NULL;
  GObject *object;

  if (name == NULL)
    {
      GSList *objects;

      /* Iterate trough objects and search for a window or widget */
      for (objects = gtk_builder_get_objects (builder); objects; objects = g_slist_next (objects))
        {
          GObject *obj = objects->data;

          if (!GTK_IS_WIDGET (obj) || gtk_widget_get_parent (GTK_WIDGET (obj)))
            continue;

          if (toplevel == NULL)
            toplevel = GTK_WIDGET (obj);
          else if (GTK_IS_WINDOW (obj))
            toplevel = GTK_WIDGET (obj);
        }

      g_slist_free (objects);
      if (toplevel == NULL)
        {
          g_printerr (_("UI definition has no previewable widgets.\n"));
          exit (1);
        }
    }
  else
    {
      object = gtk_builder_get_object (builder, name);

      if (object == NULL)
        {
          g_printerr (_("Object %s not found in UI definition.\n"), name);
          exit (1);
        }

      if (!GTK_IS_WIDGET (object))
        {
          g_printerr (_("Object is not previewable.\n"));
          exit (1);
        }

      toplevel = GTK_WIDGET (object);
    }

  return g_object_ref_sink (toplevel);
}

static GtkWidget *
get_toplevel_from_string (GladePreviewer *app, gchar *name, gchar *string)
{
  GtkBuilder *builder = gtk_builder_new ();
  GError *error = NULL;
  GtkWidget *retval;
  gchar *wd;

  /* We need to change the working directory so builder get a chance to load resources */
  if (app->file_name)
    {
      gchar *dirname = g_path_get_dirname (app->file_name);
      wd = g_get_current_dir ();
      g_chdir (dirname);
      g_free (dirname);
    }
  else
    wd = NULL;

  if (!gtk_builder_add_from_string (builder, string, -1, &error))
    {
      g_printerr (_("Couldn't load builder definition: %s"), error->message);
      g_error_free (error);
      exit (1);
    }

  retval = get_toplevel (builder, name);
  g_object_unref (builder);

  /* restore directory */
  if (wd)
    {
      g_chdir (wd);
      g_free (wd);
    }
  
  return retval;
}

static gchar *
read_buffer (GIOChannel * source)
{
  gchar *buffer;
  gchar *token;
  gchar *tmp;
  GError *error = NULL;

  if (g_io_channel_read_line (source, &token, NULL, NULL, &error) !=
      G_IO_STATUS_NORMAL)
    {
      g_printerr (_("Error: %s.\n"), error->message);
      g_error_free (error);
      exit (1);
    }

  /* Check for quit token */
  if (g_strcmp0 (QUIT_TOKEN, token) == 0)
    {
      g_free (token);
      return NULL;
    }

  /* Loop to load the UI */
  buffer = g_strdup (token);
  do
    {
      g_free (token);
      if (g_io_channel_read_line (source, &token, NULL, NULL, &error) !=
          G_IO_STATUS_NORMAL)
        {
          g_printerr (_("Error: %s.\n"), error->message);
          g_error_free (error);
          exit (1);
        }
      tmp = buffer;
      buffer = g_strconcat (buffer, token, NULL);
      g_free (tmp);
    }
  while (g_strcmp0 ("</interface>\n", token) != 0);
  g_free (token);

  return buffer;
}

static void
glade_previewer_window_set_title (GtkWindow *window, 
                                  gchar *filename,
                                  gchar *toplevel)
{
  gchar *title, *pretty_path = NULL;

  if (filename && g_path_is_absolute (filename))
    {
      gchar *canonical_path = glade_util_canonical_path (filename); 
      filename = pretty_path = glade_utils_replace_home_dir_with_tilde (canonical_path);
      g_free (canonical_path);      
    }

  if (filename)
    {
      if (toplevel)
        title = g_strdup_printf (_("Previewing %s (%s)"), filename, toplevel);
      else
        title = g_strdup_printf (_("Previewing %s"), filename);
    }
  else if (toplevel)
    {
      title = g_strdup_printf (_("Previewing %s"), toplevel);
    }
  else
    {
      gtk_window_set_title (window, _("Glade Preview"));
      return;
    }

  gtk_window_set_title (window, title);
  g_free (pretty_path);
  g_free (title);
}

static gboolean
on_data_incoming (GIOChannel *source, GIOCondition condition, gpointer data)
{
  GladePreviewer *app = data;
  GtkWidget *new_widget;
  gchar *buffer;

  buffer = read_buffer (source);
  if (buffer == NULL)
    {
      gtk_main_quit ();
      return FALSE;
    }

  if (condition & G_IO_HUP)
    {
      g_printerr (_("Broken pipe!\n"));
      exit (1);
    }

  /* We have an update */
  if (g_str_has_prefix (buffer, UPDATE_TOKEN))
    {
     gchar **split_buffer = g_strsplit_set (buffer + UPDATE_TOKEN_SIZE, "\n", 2);

      if (!split_buffer)
        {
          g_free (buffer);
          return FALSE;
        }

      new_widget = get_toplevel_from_string (app, split_buffer[0], split_buffer[1]);
      glade_previewer_window_set_title (GTK_WINDOW (app->window), app->file_name,
                                        split_buffer[0]);

      g_strfreev (split_buffer);
    }
  else
    {
      new_widget = get_toplevel_from_string (app, app->toplevel, buffer);
      glade_previewer_window_set_title (GTK_WINDOW (app->window), app->file_name, app->toplevel);
    }

  glade_preview_window_set_widget (app->window, new_widget);

  gtk_window_present (GTK_WINDOW (app->window));
  gtk_widget_show (new_widget);
  
  g_free (buffer);
  
  return TRUE;
}

static GladePreviewer *
glade_previewer_new (gchar *filename, gchar *toplevel)
{
  GladePreviewer *app = g_new0 (GladePreviewer, 1);

  app->window = GLADE_PREVIEW_WINDOW (glade_preview_window_new ());
  g_object_ref_sink (app->window);
  
  g_signal_connect (app->window, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
  glade_previewer_window_set_title (GTK_WINDOW (app->window), filename, toplevel);
   
  app->file_name = g_strdup (filename);
  app->toplevel = g_strdup (toplevel);

  return app;
}

static void
glade_previewer_free (GladePreviewer *app)
{
  g_object_unref (app->window);
  g_free (app->file_name);
  g_free (app->toplevel);
  g_free (app);
}

static gboolean listen = FALSE;
static gboolean version = FALSE;
static gchar *file_name = NULL;
static gchar *toplevel_name = NULL;
static gchar *css_file_name = NULL;
static gchar *screenshot_file_name = NULL;

static GOptionEntry option_entries[] =
{
    {"filename", 'f', 0, G_OPTION_ARG_FILENAME, &file_name, N_("Name of the file to preview"), "FILENAME"},
    {"toplevel", 't', 0, G_OPTION_ARG_STRING, &toplevel_name, N_("Name of the toplevel to preview"), "TOPLEVELNAME"},
    {"screenshot", 0, 0, G_OPTION_ARG_FILENAME, &screenshot_file_name, N_("File name to save a screenshot"), NULL},
    {"css", 0, 0, G_OPTION_ARG_FILENAME, &css_file_name, N_("CSS file to use"), NULL},
    {"listen", 'l', 0, G_OPTION_ARG_NONE, &listen, N_("Listen standard input"), NULL},
    {"version", 'v', 0, G_OPTION_ARG_NONE, &version, N_("Display previewer version"), NULL},
    {NULL}
};

int
main (int argc, char **argv)
{
  GladePreviewer *app;
  GOptionContext *context;
  GError *error = NULL;

#ifdef ENABLE_NLS
  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, glade_app_get_locale_dir ());
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  context = g_option_context_new (_("- previews a glade UI definition"));
  g_option_context_add_main_entries (context, option_entries, GETTEXT_PACKAGE);
  g_option_context_add_group (context, gtk_get_option_group (TRUE));
  
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr (_("%s\nRun '%s --help' to see a full list of available command line "
                   "options.\n"), error->message, argv[0]);
      g_error_free (error);
      g_option_context_free (context);
      return 1;
    }

  g_option_context_free (context);

  if (version)
    {
      g_print ("glade-previewer " VERSION "\n");
      return 0;
    }

  if (!listen && !file_name)
    {
      g_printerr (_("Either --listen or --filename must be specified.\n"));
      return 0;
    }

  gtk_init (&argc, &argv);
  glade_app_get ();

  app = glade_previewer_new (file_name, toplevel_name);
  gtk_widget_show (GTK_WIDGET (app->window));
  
  if (css_file_name)
    glade_preview_window_set_css_file (app->window, css_file_name);

  if (listen)
    {
#ifdef WINDOWS
      GIOChannel *input = g_io_channel_win32_new_fd (fileno (stdin));
#else
      GIOChannel *input = g_io_channel_unix_new (fileno (stdin));
#endif

      g_io_add_watch (input, G_IO_IN | G_IO_HUP, on_data_incoming, app);

      gtk_main ();
    }
  else if (app->file_name)
    {
      GtkBuilder *builder = gtk_builder_new ();
      GError *error = NULL;
      GtkWidget *widget;

      /* Use from_file() function gives builder a chance to know where to load resources from */
      if (!gtk_builder_add_from_file (builder, app->file_name, &error))
        {
          g_printerr (_("Couldn't load builder definition: %s"), error->message);
          g_error_free (error);
          return 1;
        }

      widget = get_toplevel (builder, toplevel_name);
      glade_preview_window_set_widget (app->window, widget);
      gtk_widget_show (widget);

      if (screenshot_file_name)
        glade_preview_window_screenshot (app->window, TRUE, screenshot_file_name);
      else
        gtk_main ();

      g_object_unref (builder);
    }

  /* free unused resources */
  g_free (file_name);
  g_free (toplevel_name);
  g_free (css_file_name);
  g_free (screenshot_file_name);
  glade_previewer_free (app);

  return 0;
}
