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
    {"filename", 'f', G_OPTION_FLAG_FILENAME | G_OPTION_FLAG_OPTIONAL_ARG,
     G_OPTION_ARG_FILENAME, file_name, _("Name of the file to preview"),
     "FILENAME"},
    {"toplevel", 't', G_OPTION_FLAG_OPTIONAL_ARG,
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
                  ("%s\nRun '%s --help' to see a full list of available command line"
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
  GtkWidget *widget_parent;
  GtkWidget *window;

  widget = get_toplevel (name, buffer, length);

  if (GTK_IS_WINDOW (widget))
    {
      window = widget;
    }
  else
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (window), _("Preview"));

      /* Reparenting snippet */
      g_object_ref (widget);
      widget_parent = gtk_widget_get_parent (widget);
      if (widget_parent != NULL)
        gtk_container_remove (GTK_CONTAINER (widget_parent), widget);
      gtk_container_add (GTK_CONTAINER (window), widget);
      g_object_unref (widget);
    }

  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show_all (window);

  return widget;
}

static GIOChannel *
channel_from_stream (gint stream)
{
  GIOChannel *channel;

#ifdef WINDOWS
  channel = g_io_channel_win32_new_fd (stream);
#else
  channel = g_io_channel_unix_new (stream);
#endif

  return channel;
}

static void
preview_file (gchar * toplevel_name, gchar * file_name)
{
  gchar *buffer;
  gsize length;
  GError *error = NULL;
  GIOChannel *input;
  gint stream;

  stream = fileno (fopen (file_name, "r"));
  input = channel_from_stream (stream);

  if (g_io_channel_read_to_end (input, &buffer, &length, &error) !=
      G_IO_STATUS_NORMAL)
    {
      g_printerr (_("Error: %s.\n"), error->message);
      g_error_free (error);
      exit (1);
    }

  preview_widget (toplevel_name, buffer, length);

  g_free (buffer);
  g_io_channel_unref (input);
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
copy_window_properties (GtkWindow *old_window, GtkWindow *new_window)
{
	GParamSpec **properties_list;
	guint i, n_properties;
	gint width, height;
	GValue copy_value = { 0,};

	if (!old_window || !new_window) return;

	properties_list = g_object_class_list_properties
			  (G_OBJECT_GET_CLASS (G_OBJECT (new_window)),
			  &n_properties);

	for (i = 0; i < n_properties; i++)
	{
		if (!((properties_list[i]->flags & G_PARAM_READABLE) &&
		      (properties_list[i]->flags & G_PARAM_WRITABLE) &&
		      (!(properties_list[i]->flags & G_PARAM_CONSTRUCT_ONLY))
		    )) continue;

		/* We won't set some properties */
		if (g_strcmp0 ("parent", properties_list[i]->name) == 0) continue;
		if (g_strcmp0 ("visible", properties_list[i]->name) == 0) continue;

		g_value_init (&copy_value, properties_list[i]->value_type);
		g_object_get_property (G_OBJECT (new_window),
				       properties_list[i]->name,
				       &copy_value);

		g_object_set_property (G_OBJECT (old_window),
				       properties_list[i]->name,
				       &copy_value);

		g_value_unset (&copy_value);
	}

	/* Special code to copy default size */
	if (gtk_window_get_resizable (old_window))
	{
		gtk_window_get_default_size (new_window, &width, &height);
		if ((width > 0) && (height > 0))
			gtk_window_resize (old_window, width, height);
	}

	if (properties_list) g_free (properties_list);
}

static void
update_window (GtkWindow *old_window, GtkWindow *new_window)
{
	GtkWidget *window_child;
	GtkWidget *old_window_child;

	if (!old_window || !new_window) return;

	/* ref the new window child */
	window_child = gtk_bin_get_child (GTK_BIN (new_window));
	if (window_child)
	{
		g_object_ref (window_child);

		/* remove window child */
		gtk_container_remove (GTK_CONTAINER (new_window), window_child);
	}

	/* gtk_widget_destroy the *running preview's* window child */
	old_window_child = gtk_bin_get_child (GTK_BIN (old_window));
	if (old_window_child)
		gtk_widget_destroy (old_window_child);

	/* apply properties from the new window to the
	 * old window (using GObjectapis)*/
	copy_window_properties (old_window, new_window);

	/* add new preview's window child to the old preview's window */
	if (window_child)
	{
		gtk_container_add (GTK_CONTAINER (old_window), window_child);
		/* unref the newly added child (which we added a ref to before) */
		g_object_unref (window_child);
	}
}

static void
update_window_child (GtkWindow *old_window, GtkWidget *new_widget)
{
	GtkWidget *old_window_child;

	if (!old_window) return;

	/* gtk_widget_destroy the *running preview's* window child */
	old_window_child = gtk_bin_get_child (GTK_BIN (old_window));
	if (old_window_child)
		gtk_widget_destroy (old_window_child);

	/* add new widget as child to the old preview's window */
	if (new_widget)
		gtk_container_add (GTK_CONTAINER (old_window), new_widget);
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
    preview_window = preview_widget (toplevel_name, buffer, length);
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

		if (GTK_IS_WINDOW (new_widget))
			update_window (GTK_WINDOW (preview_window),
				       GTK_WINDOW (new_widget));
		else update_window_child (GTK_WINDOW (preview_window), new_widget);
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
  gint stream;

  stream = fileno (stdin);
  input = channel_from_stream (stream);

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
