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
 */

#include <config.h>

#include "glade-window.h"

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


#define TYPE_GLADE            (glade_get_type())
#define GLADE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_GLADE, Glade))

typedef struct _Glade      Glade;
typedef struct _GladeClass GladeClass;

struct _Glade {
  GtkApplication application;

  GladeWindow *window;
};

struct _GladeClass {
  GtkApplicationClass application_class;
};


static GType  glade_get_type        (void) G_GNUC_CONST;
static void   glade_finalize        (GObject       *object);
static void   glade_open            (GApplication  *application,
				     GFile        **files,
				     gint           n_files,
				     const gchar   *hint);
static void   glade_startup         (GApplication  *application);
static void   glade_activate        (GApplication  *application);

G_DEFINE_TYPE (Glade, glade, GTK_TYPE_APPLICATION)

static gboolean
open_new_project (Glade *glade)
{
  if (!glade_app_get_projects ())
    glade_window_new_project (glade->window);

  return FALSE;
}

static void
glade_init (Glade *glade)
{

}

static void
glade_class_init (GladeClass *glade_class)
{
  GObjectClass      *object_class      = G_OBJECT_CLASS (glade_class);
  GApplicationClass *application_class = G_APPLICATION_CLASS (glade_class);

  object_class->finalize      = glade_finalize;

  application_class->open     = glade_open;
  application_class->startup  = glade_startup;
  application_class->activate = glade_activate;
}

static void
glade_finalize (GObject *object)
{
  G_OBJECT_CLASS (glade_parent_class)->finalize (object);
}

static void
glade_open (GApplication  *application,
	    GFile        **files,
	    gint           n_files,
	    const gchar   *hint)
{
  Glade *glade = GLADE (application);
  gint i;
  gchar *path;

  for (i = 0; i < n_files; i++)
    {
      if ((path = g_file_get_path (files[i])) != NULL)
	{
	  glade_window_open_project (glade->window, path);

	  g_free (path);
	}
    }
}

static void
glade_startup (GApplication *application)
{
  Glade *glade = GLADE (application);

  glade_setup_log_handlers ();

  glade->window = GLADE_WINDOW (glade_window_new ());
  gtk_window_set_application (GTK_WINDOW (glade->window), GTK_APPLICATION (application));

  gtk_widget_show (GTK_WIDGET (glade->window));

  g_idle_add ((GSourceFunc)open_new_project, glade);

  G_APPLICATION_CLASS (glade_parent_class)->startup (application);
}

static void
glade_activate (GApplication *application)
{
}


static Glade *
glade_new (void)
{
  Glade *glade =
    g_object_new (TYPE_GLADE, 
		  "application-id", "org.gnome.Glade",
		  "flags", G_APPLICATION_HANDLES_OPEN,
		  NULL);

  return glade;
}

/* Version argument parsing */
static gboolean version = FALSE;

static GOptionEntry option_entries[] = {
  {"version", '\0', 0, G_OPTION_ARG_NONE, &version,
   N_("Output version information and exit"), NULL},

  {NULL}
};

int
main (int argc, char *argv[])
{
  GOptionContext *option_context;
  GOptionGroup *option_group;
  GError *error = NULL;
  Glade *glade;
  gint status;

  g_type_init ();

  if (!g_thread_supported ())
    g_thread_init (NULL);

#ifdef ENABLE_NLS
  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, glade_app_get_locale_dir ());
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  /* Set up option groups */
  option_context = g_option_context_new (NULL);

  g_option_context_set_summary (option_context,
                                N_("Create or edit user interface designs for GTK+ or GNOME applications."));
  g_option_context_set_translation_domain (option_context, GETTEXT_PACKAGE);

  option_group = g_option_group_new ("glade",
                                     N_("Glade options"),
                                     N_("Glade options"), NULL, NULL);
  g_option_group_add_entries (option_group, option_entries);
  g_option_context_set_main_group (option_context, option_group);
  g_option_group_set_translation_domain (option_group, GETTEXT_PACKAGE);

  /* Parse command line */
  if (!g_option_context_parse (option_context, &argc, &argv, &error))
    {
      g_option_context_free (option_context);

      if (error)
        {
          g_print ("%s\n", error->message);
          g_error_free (error);
        }
      else
        g_print ("An unknown error occurred\n");

      return -1;
    }

  g_option_context_free (option_context);
  option_context = NULL;

  if (version != FALSE)
    {
      /* Print version information and exit */
      g_print ("%s\n", PACKAGE_STRING);
      return 0;
    }

  /* Check for gmodule support */
  if (!g_module_supported ())
    {
      g_warning (_("gmodule support not found. gmodule support is required "
                   "for glade to work"));
      return -1;
    }

  gtk_init (&argc, &argv);

  g_set_application_name (APPLICATION_NAME);
  gtk_window_set_default_icon_name ("glade");

  /* Fire it up */
  glade = glade_new ();
  status = g_application_run (G_APPLICATION (glade), argc, argv);
  g_object_unref (glade);

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
