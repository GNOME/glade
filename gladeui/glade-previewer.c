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

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include <gladeui/glade.h>

#include <stdlib.h>
#include <locale.h>
#include <glib/gi18n-lib.h>

#include "glade-preview-tokens.h"

static void
display_help_and_quit (const GOptionEntry * entries)
{
  GOptionContext *context;
  context = g_option_context_new (_("- previews a glade UI definition"));
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_add_group (context, gtk_get_option_group (TRUE));

  g_print ("%s\n", g_option_context_get_help (context, TRUE, NULL));

  g_option_context_free (context);
  exit (1);
}

static void
parse_arguments (int argc, char **argv, gchar ** toplevel_name,
                 gchar ** file_name)
{
  *toplevel_name = NULL;
  *file_name = NULL;
  gboolean listen = FALSE;
  gboolean version = FALSE;
  GError *error = NULL;

  GOptionEntry entries[] = {
    {"filename", 'f', G_OPTION_ARG_NONE,
     G_OPTION_ARG_FILENAME, file_name, _("Name of the file to preview"),
     "FILENAME"},
    {"toplevel", 't', G_OPTION_ARG_NONE,
     G_OPTION_ARG_STRING, toplevel_name, _("Name of the toplevel to preview"),
     "TOPLEVELNAME"},
    {"listen", 'l', 0, G_OPTION_ARG_NONE, &listen, _("Listen standard input"),
     NULL},
    {"version", 'v', 0, G_OPTION_ARG_NONE, &version,
     _("Display previewer version"), NULL},

    {NULL}
  };

  if (!gtk_init_with_args (&argc, &argv, _("- previews a glade UI definition"),
                           entries, NULL, &error))
    {
      g_printerr (_
                  ("%s\nRun '%s --help' to see a full list of available command line "
                   "options.\n"), error->message, argv[0]);
      g_error_free (error);
      exit (1);
    }

  if (version)
    {
      g_print ("glade-previewer " VERSION "\n");
      exit (0);
    }

  if (listen && *file_name != NULL)
    {
      g_printerr (_
                  ("--listen and --filename must not be simultaneously specified.\n"));
      display_help_and_quit (entries);
    }

  if (!listen && *file_name == NULL)
    {
      g_printerr (_("Either --listen or --filename must be specified.\n"));
      display_help_and_quit (entries);
    }
}

static GtkWidget *
get_toplevel (gchar * name, gchar * string, gsize length)
{
  GError *error = NULL;
  GtkWidget *toplevel = NULL;
  GtkWidget *window = NULL;
  GtkBuilder *builder;
  GObject *object;
  GSList *objects;

  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_string (builder, string, length, &error))
    {
      g_printerr (_("Couldn't load builder definition: %s"), error->message);
      g_error_free (error);
      exit (1);
    }

  if (name == NULL)
    {
      objects = gtk_builder_get_objects (builder);

      /* Iterate trough objects and search for a window or widget */
      while (objects != NULL)
        {
          if (GTK_IS_WIDGET (objects->data) && toplevel == NULL)
            {
              toplevel = GTK_WIDGET (objects->data);
              g_object_ref (toplevel);
            }
          if (GTK_IS_WINDOW (objects->data))
            {
              window = GTK_WIDGET (objects->data);
              break;
            }
          objects = objects->next;
        }

      if (window != NULL)
        {
          toplevel = window;
        }
      else if (toplevel == NULL)
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

  g_object_unref (builder);

  return toplevel;
}

static GtkWidget *
preview_widget (gchar * name, gchar * buffer, gsize length)
{
  GtkWidget *widget;

  widget = get_toplevel (name, buffer, length);

  return widget;
}

static GtkWidget*
show_widget (GtkWidget *widget)
{
  GtkWidget *window;
  GtkWidget *widget_parent;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  if (!GTK_IS_WINDOW (widget))
    {
      gtk_window_set_title (GTK_WINDOW (window), _("Preview"));

      /* Reparenting snippet */
      g_object_ref (widget);
      widget_parent = gtk_widget_get_parent (widget);
      if (widget_parent != NULL)
        gtk_container_remove (GTK_CONTAINER (widget_parent), widget);
      gtk_container_add (GTK_CONTAINER (window), widget);
      g_object_unref (widget);
    }
  else
    {
      gtk_widget_realize (window);
      gtk_widget_set_parent_window (widget, gtk_widget_get_window (window));
      gtk_container_add (GTK_CONTAINER (window), widget);
    }

  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  return window;
}

static void
preview_file (gchar * toplevel_name, gchar * file_name)
{
  GError *error = NULL;
  gchar *buffer;
  gsize length;

  if (g_file_get_contents (file_name, &buffer, &length, &error))
    {
      GtkWidget *widget = preview_widget (toplevel_name, buffer, length);
      gtk_widget_show_all (show_widget (widget));
      g_free (buffer);
    }
  else
    {
      g_printerr (_("Error: %s.\n"), error->message);
      g_error_free (error);
      exit (1);
    }
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
update_window (GtkWindow *old_window, GtkWidget *new_widget)
{
	GtkWidget *old_window_child;

	if (!old_window) return;

	/* gtk_widget_destroy the *running preview's* window child */
	old_window_child = gtk_bin_get_child (GTK_BIN (old_window));
	if (old_window_child)
		gtk_widget_destroy (old_window_child);

	/* add new widget as child to the old preview's window */
	if (new_widget)
    {
      gtk_widget_set_parent_window (new_widget, gtk_widget_get_window (
                                                            GTK_WIDGET (old_window)));
  		gtk_container_add (GTK_CONTAINER (old_window), new_widget);
    }

  gtk_widget_show_all (GTK_WIDGET (old_window));
}

static gboolean
on_data_incoming (GIOChannel * source, GIOCondition condition, gpointer data)
{
  gchar *buffer;
  gsize length;
  static GtkWidget *preview_window = NULL;
  GtkWidget *new_widget;
  gchar **split_buffer = NULL;

  gchar *toplevel_name = (gchar *) data;

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

  length = strlen (buffer);

  /* if it is the first time this is called */
  if (!preview_window)
  {
    new_widget = preview_widget (toplevel_name, buffer, length);
    preview_window = show_widget (new_widget);
    gtk_widget_show_all (preview_window);
  }
  else
  {
    /* We have an update */
    split_buffer = g_strsplit_set (buffer + UPDATE_TOKEN_SIZE, "\n", 2);

    g_free (buffer);
    if (!split_buffer) return FALSE;

    toplevel_name = split_buffer[0];
    buffer = split_buffer[1];
    length = strlen (buffer);

    new_widget = get_toplevel (toplevel_name, buffer, length);

    update_window (GTK_WINDOW (preview_window), new_widget);
  }

  if (!split_buffer)
  {
    /* This means we're not in an update, we should free the buffer. */
    g_free (buffer);
  }
  else
  {
    /* This means we've had an update, buffer was already freed. */
    g_free (split_buffer[0]);
    g_free (split_buffer[1]);
    g_free (split_buffer);
  }

  return TRUE;
}

static void
start_listener (gchar * toplevel_name)
{
  GIOChannel *input;

#ifdef WINDOWS
  input = g_io_channel_win32_new_fd (fileno (stdin));
#else
  input = g_io_channel_unix_new (fileno (stdin));
#endif

  g_io_add_watch (input, G_IO_IN | G_IO_HUP, on_data_incoming, toplevel_name);
}

int
main (int argc, char **argv)
{
  gchar *file_name;
  gchar *toplevel_name;

#ifdef ENABLE_NLS
  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, glade_app_get_locale_dir ());
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  parse_arguments (argc, argv, &toplevel_name, &file_name);

  gtk_init (&argc, &argv);
  glade_app_get ();

  if (file_name != NULL)
    {
      preview_file (toplevel_name, file_name);
    }
  else
    {
      start_listener (toplevel_name);
    }

  gtk_main ();

  return 0;
}
