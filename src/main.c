/*
 * Copyright (C) 2001 Ximian, Inc.
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
 *   Vincent Geddes <vgeddes@gnome.org>
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#include <config.h>

#include "version.h"
#include "glade-window.h"
#include "glade-resources.h"

#include <gladeui/glade.h>
#include <gladeui/glade-app.h>
#include <gladeui/glade-debug.h>

#include <stdlib.h>
#include <locale.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gmodule.h>

#ifdef G_OS_WIN32
#  include <windows.h>
#endif

#define APPLICATION_NAME (_("Glade"))


/* Application arguments */
static gboolean version = FALSE, without_devhelp = FALSE;
static gboolean verbose = FALSE;

static GOptionEntry option_entries[] = {
  {"version", '\0', 0, G_OPTION_ARG_NONE, &version,
   N_("Output version information and exit"), NULL},

  {"without-devhelp", '\0', 0, G_OPTION_ARG_NONE, &without_devhelp,
   N_("Disable Devhelp integration"), NULL},

  {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose, N_("be verbose"), NULL},

  {NULL}
};

static gint
handle_local_options (GApplication *application,
                      GVariantDict *options,
                      gpointer      user_data)
{
  if (version != FALSE)
    {
      /* Print version information and exit */
      if (GLADE_MINOR_VERSION % 2)
        g_print (PACKAGE_NAME" "VCS_VERSION"\n");
      else
        g_print (PACKAGE_STRING"\n");

      return 0;
    }

  return -1;
}

static void
startup (GApplication *application)
{
#ifdef ENABLE_NLS
  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, glade_app_get_locale_dir ());
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  glade_setup_log_handlers ();

  g_set_application_name (APPLICATION_NAME);

  gtk_window_set_default_icon_name ("glade");

}

static void
activate (GApplication *application)

{
  GladeWindow *window = GLADE_WINDOW (glade_window_new ());

  gtk_application_add_window (GTK_APPLICATION (application),
                              GTK_WINDOW (window));

  if (without_devhelp == FALSE)
    glade_window_check_devhelp (window);

  gtk_widget_show (GTK_WIDGET (window));
}

static void
open (GApplication  *application,
      GFile        **files,
      gint           n_files,
      const gchar   *hint)
{
  GtkApplication *app = GTK_APPLICATION (application);
  GTimer *timer = NULL;
  GtkWindow *window;
  gint i;

  if (!(window = gtk_application_get_active_window (app)))
    {
      g_application_activate (application);
      window = gtk_application_get_active_window (app);
    }

  if (verbose) timer = g_timer_new ();

  for (i = 0; i < n_files; i++)
    {
      gchar *path = g_file_get_path (files[i]);

      for (i = 0; files[i]; ++i)
        {
          if (verbose) g_timer_start (timer);

          if (g_file_test (path, G_FILE_TEST_EXISTS) != FALSE)
            glade_window_open_project (GLADE_WINDOW (window), path);
          else
            g_warning (_("Unable to open '%s', the file does not exist.\n"),
                       path);

          if (verbose)
            {
              g_timer_stop (timer);
              g_message ("Loading '%s' took %lf seconds", path,
                         g_timer_elapsed (timer, NULL));
            }
        }

      g_free (path);
    }

  if (verbose) g_timer_destroy (timer);
}

#include "workaround.h"

int
main (int argc, char *argv[])
{
  GtkApplication *app;
  int status;

  /* Check for gmodule support */
  if (!g_module_supported ())
    {
      g_warning (_("gmodule support not found. gmodule support is required "
                   "for glade to work"));
      return -1;
    }

  gtk_init (&argc, &argv);

  /* FIXME:
   *
   * This prevents glade from crashing when loading GJS plugin on X11 backend
   */
#ifdef GDK_WINDOWING_X11
  _init_cairo_xlib_workaround ();
#endif

  app = gtk_application_new ("org.gnome.Glade", G_APPLICATION_HANDLES_OPEN);

  g_application_set_option_context_summary (G_APPLICATION (app),
                                            N_("Create or edit user interface designs for GTK+ or GNOME applications."));
  g_application_add_main_option_entries (G_APPLICATION (app), option_entries);

  g_signal_connect (app, "handle-local-options", G_CALLBACK (handle_local_options), NULL);
  g_signal_connect (app, "startup", G_CALLBACK (startup), NULL);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  g_signal_connect (app, "open", G_CALLBACK (open), NULL);

  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);

  return status;
}

#ifdef G_OS_WIN32

/* In case we build this as a windowed application. */

#ifdef __GNUC__
#  ifndef _stdcall
#    define _stdcall  __attribute__((stdcall))
#  endif
#endif

int _stdcall
WinMain (struct HINSTANCE__ *hInstance,
         struct HINSTANCE__ *hPrevInstance, char *lpszCmdLine, int nCmdShow)
{
  return main (__argc, __argv);
}

#endif
