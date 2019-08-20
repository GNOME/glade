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
 *   Naba Kumar <naba@gnome.org>
 */

#include <config.h>

/**
 * SECTION:glade-app
 * @Short_Description: The central control point of the Glade core.
 *
 * This main control object is where we try to draw the line between
 * what is the Glade core and what is the main application. The main
 * application must derive from the GladeApp object and create an instance
 * to initialize the Glade core.
 */

#include "glade.h"
#include "glade-debug.h"
#include "glade-cursor.h"
#include "glade-catalog.h"
#include "glade-design-view.h"
#include "glade-design-layout.h"
#include "glade-marshallers.h"
#include "glade-accumulators.h"

#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#ifdef MAC_INTEGRATION
#  include <gtkosxapplication.h>
#endif

#define GLADE_CONFIG_FILENAME "glade.conf"

enum
{
  DOC_SEARCH,
  SIGNAL_EDITOR_CREATED,
  WIDGET_ADAPTOR_REGISTERED,
  LAST_SIGNAL
};

typedef struct _GladeAppPrivate GladeAppPrivate;
struct _GladeAppPrivate
{
  GtkWidget *window;

  GladeClipboard *clipboard;    /* See glade-clipboard */
  GList *catalogs;              /* See glade-catalog */

  GList *projects;              /* The list of Projects */

  GKeyFile *config;             /* The configuration file */

  GtkAccelGroup *accel_group;   /* Default acceleration group for this app */
};

static guint glade_app_signals[LAST_SIGNAL] = { 0 };

/* installation paths */
static gchar *catalogs_dir = NULL;
static gchar *modules_dir = NULL;
static gchar *pixmaps_dir = NULL;
static gchar *locale_dir = NULL;
static gchar *bin_dir = NULL;
static gchar *lib_dir = NULL;

static GladeApp *singleton_app = NULL;
static gboolean check_initialised = FALSE;

G_DEFINE_TYPE_WITH_PRIVATE (GladeApp, glade_app, G_TYPE_OBJECT);

/*****************************************************************
 *                    GObjectClass                               *
 *****************************************************************/
static GObject *
glade_app_constructor (GType                  type,
                       guint                  n_construct_properties,
                       GObjectConstructParam *construct_properties)
{
  GObject *object;

  /* singleton */
  if (!singleton_app)
    {
      object = G_OBJECT_CLASS (glade_app_parent_class)->constructor (type,
                                                                     n_construct_properties,
                                                                     construct_properties);
      singleton_app = GLADE_APP (object);
    }
  else
    {
      g_object_ref (singleton_app);
    }

  return G_OBJECT (singleton_app);
}



static void
glade_app_dispose (GObject *app)
{
  GladeAppPrivate *priv = glade_app_get_instance_private ((GladeApp *) app);

  if (priv->clipboard)
    {
      g_object_unref (priv->clipboard);
      priv->clipboard = NULL;
    }
  /* FIXME: Remove projects */

  if (priv->config)
    {
      g_key_file_free (priv->config);
      priv->config = NULL;
    }

  G_OBJECT_CLASS (glade_app_parent_class)->dispose (app);
}

static void
glade_app_finalize (GObject *app)
{
  g_free (catalogs_dir);
  g_free (modules_dir);
  g_free (pixmaps_dir);
  g_free (locale_dir);
  g_free (bin_dir);
  g_free (lib_dir);

  singleton_app = NULL;
  check_initialised = FALSE;

  G_OBJECT_CLASS (glade_app_parent_class)->finalize (app);
}

/* build package paths at runtime */
static void
build_package_paths (void)
{
  const gchar *path;

  path = g_getenv (GLADE_ENV_PIXMAP_DIR);
  if (path)
    pixmaps_dir = g_strdup (path);

#if defined (G_OS_WIN32) || (defined (MAC_INTEGRATION) && defined (MAC_BUNDLE))
  gchar *prefix;

# ifdef G_OS_WIN32
  prefix = g_win32_get_package_installation_directory_of_module (NULL);

# else // defined (MAC_INTEGRATION) && defined (MAC_BUNDLE)
  prefix = quartz_application_get_resource_path ();

# endif

  if (!pixmaps_dir)
    pixmaps_dir = g_build_filename (prefix, "share", PACKAGE, "pixmaps", NULL);

  catalogs_dir = g_build_filename (prefix, "share", PACKAGE, "catalogs", NULL);
  modules_dir = g_build_filename (prefix, "lib", PACKAGE, "modules", NULL);
  locale_dir = g_build_filename (prefix, "share", "locale", NULL);
  bin_dir = g_build_filename (prefix, "bin", NULL);
  lib_dir = g_build_filename (prefix, "lib", NULL);

  g_free (prefix);
#else
  catalogs_dir = g_strdup (GLADE_CATALOGSDIR);
  modules_dir = g_strdup (GLADE_MODULESDIR);

  if (!pixmaps_dir)
    pixmaps_dir = g_strdup (GLADE_PIXMAPSDIR);
  locale_dir = g_strdup (GLADE_LOCALEDIR);
  bin_dir = g_strdup (GLADE_BINDIR);
  lib_dir = g_strdup (GLADE_LIBDIR);
#endif
}

/* initialization function for libgladeui (not GladeApp) */
static void
glade_init_check (void)
{
  if (check_initialised)
    return;

  glade_init_debug_flags ();

  /* Make sure path accessors work on osx */
  build_package_paths ();

  bindtextdomain (GETTEXT_PACKAGE, locale_dir);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  check_initialised = TRUE;
}

/*****************************************************************
 *                    GladeAppClass                              *
 *****************************************************************/
const gchar *
glade_app_get_catalogs_dir (void)
{
  glade_init_check ();

  return catalogs_dir;
}

const gchar *
glade_app_get_modules_dir (void)
{
  glade_init_check ();

  return modules_dir;
}

const gchar *
glade_app_get_pixmaps_dir (void)
{
  glade_init_check ();

  return pixmaps_dir;
}

const gchar *
glade_app_get_locale_dir (void)
{
  glade_init_check ();

  return locale_dir;
}

const gchar *
glade_app_get_bin_dir (void)
{
  glade_init_check ();

  return bin_dir;
}

const gchar *
glade_app_get_lib_dir (void)
{
  glade_init_check ();

  return lib_dir;
}

static void
pointer_mode_register_icon (const gchar     *icon_name,
                            gint             real_size,
                            GladePointerMode mode,
                            GtkIconSize      size)
{
  GdkPixbuf *pixbuf;

  if ((pixbuf = glade_utils_pointer_mode_render_icon (mode, size)))
    {
      gtk_icon_theme_add_builtin_icon (icon_name, real_size, pixbuf);
      g_object_unref (pixbuf);
    }
}

static void
register_icon (const gchar    *new_icon_name,
               gint            size,
               const gchar    *icon_name,
               const gchar    *file_name)
{
  GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();
  GdkPixbuf *pixbuf;
  GtkIconInfo *info;

  if ((info = gtk_icon_theme_lookup_icon (icon_theme, icon_name, size, 0)))
    {
      pixbuf = gtk_icon_info_load_icon (info, NULL);
    }
  else
    {
      gchar *path = g_build_filename (glade_app_get_pixmaps_dir (), file_name, NULL);
      pixbuf = gdk_pixbuf_new_from_file (path, NULL);
      g_free (path);
    }

  if (pixbuf)
    {
      gtk_icon_theme_add_builtin_icon (new_icon_name, size, pixbuf);
      g_object_unref (pixbuf);
    }
}

/*
 * glade_app_register_icon_names:
 * @size: icon size
 *
 * Register a new icon name for most of GladePointerMode.
 * After calling this function "glade-selector", "glade-drag-resize",
 * "glade-margin-edit" and "glade-align-edit" icon names will be available.
 */ 
static void
glade_app_register_icon_names (GtkIconSize size)
{
  gint w, h, real_size;

  if (gtk_icon_size_lookup (size, &w, &h) == FALSE)
    return;

  real_size = MIN (w, h);

  pointer_mode_register_icon ("glade-selector", real_size, GLADE_POINTER_SELECT, size);
  pointer_mode_register_icon ("glade-drag-resize", real_size, GLADE_POINTER_DRAG_RESIZE, size);
  pointer_mode_register_icon ("glade-margin-edit", real_size, GLADE_POINTER_MARGIN_EDIT, size);
  pointer_mode_register_icon ("glade-align-edit", real_size, GLADE_POINTER_ALIGN_EDIT, size);

  register_icon ("glade-devhelp", real_size,
                 GLADE_DEVHELP_ICON_NAME,
                 GLADE_DEVHELP_FALLBACK_ICON_FILE);
}

/**
 * glade_init:
 * 
 * Initialization function for libgladeui (not #GladeApp)
 * It builds paths, bind text domain, and register icons
 */
void
glade_init (void)
{
  static gboolean init = FALSE;

  if (init) return;
  
  glade_init_check ();

  /* Register icons needed by the UI */
  glade_app_register_icon_names (GTK_ICON_SIZE_LARGE_TOOLBAR);
  
  init = TRUE;
}

static void
glade_app_init (GladeApp *app)
{
  static gboolean initialized = FALSE;
  GladeAppPrivate *priv = glade_app_get_instance_private (app);

  singleton_app = app;

  glade_init ();

  if (!initialized)
    {
      GtkIconTheme *default_icon_theme = gtk_icon_theme_get_default ();
      const gchar *path;

      gtk_icon_theme_append_search_path (default_icon_theme, pixmaps_dir);

      /* Handle extra icon theme paths. Needed for tests to work */
      if ((path = g_getenv (GLADE_ENV_ICON_THEME_PATH)))
        {
          gchar **tokens = g_strsplit (path, ":", -1);
          gint i;

          for (i = 0; tokens[i]; i++)
            gtk_icon_theme_append_search_path (default_icon_theme, tokens[i]);

          g_strfreev (tokens);
        }

      glade_cursor_init ();

      initialized = TRUE;
    }

  priv->accel_group = NULL;

  /* Initialize app objects */
  priv->catalogs = (GList *) glade_catalog_load_all ();

  /* Create clipboard */
  priv->clipboard = glade_clipboard_new ();

  /* Load the configuration file */
  priv->config = g_key_file_ref (glade_app_get_config ());
}

static void
glade_app_event_handler (GdkEvent *event, gpointer data)
{
  if (glade_app_do_event (event)) return;

  gtk_main_do_event (event);
}

static void
glade_app_class_init (GladeAppClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->constructor = glade_app_constructor;
  object_class->dispose = glade_app_dispose;
  object_class->finalize = glade_app_finalize;

  /**
   * GladeApp::doc-search:
   * @gladeeditor: the #GladeEditor which received the signal.
   * @arg1: the (#gchar *) book to search or %NULL
   * @arg2: the (#gchar *) page to search or %NULL
   * @arg3: the (#gchar *) search string or %NULL
   *
   * Emitted when the glade core requests that a doc-search be performed.
   */
  glade_app_signals[DOC_SEARCH] =
      g_signal_new ("doc-search",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                    _glade_marshal_VOID__STRING_STRING_STRING,
                    G_TYPE_NONE, 3,
                    G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  /**
   * GladeApp::signal-editor-created:
   * @gladeapp: the #GladeApp which received the signal.
   * @signal_editor: the new #GladeSignalEditor.
   *
   * Emitted when a new signal editor created.
   * A tree view is created in the default handler.
   * Connect your handler before the default handler for setting a custom column or renderer
   * and after it for connecting to the tree view signals
   */
  glade_app_signals[SIGNAL_EDITOR_CREATED] =
    g_signal_new ("signal-editor-created",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _glade_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, G_TYPE_OBJECT);  

  /**
   * GladeApp::widget-adaptor-registered:
   * @gladeapp: the #GladeApp which received the signal.
   * @adaptor: the newlly registered #GladeWidgetAdaptor.
   *
   * Emitted when a new widget adaptor is registered.
   */
  glade_app_signals[WIDGET_ADAPTOR_REGISTERED] =
    g_signal_new ("widget-adaptor-registered",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  _glade_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, G_TYPE_OBJECT);

  gdk_event_handler_set (glade_app_event_handler, NULL, NULL);
}

/*****************************************************************
 *                       Public API                              *
 *****************************************************************/

/**
 * glade_app_do_event:
 * @event: the event to process.
 *
 * This function has to be called in an event handler for widget selection to work.
 * See gdk_event_handler_set()
 *
 * Returns: true if the event was handled.
 */ 
gboolean
glade_app_do_event (GdkEvent *event)
{
  GdkWindow *window = event->any.window;
  GtkWidget *layout;
  gpointer widget;

  if (window == NULL) return FALSE;
    
  gdk_window_get_user_data (window, &widget);

  /* As a slight optimization we could replace gtk_widget_get_ancestor()
   * with a custom function that only iterates trought parents with windows.
   */
  if (widget && IS_GLADE_WIDGET_EVENT (event->type) &&
      (layout = gtk_widget_get_ancestor (widget, GLADE_TYPE_DESIGN_LAYOUT)))
    return _glade_design_layout_do_event (GLADE_DESIGN_LAYOUT (layout), event);

  return FALSE;
}

/**
 * glade_app_config_save
 *
 * Saves the #GKeyFile to "g_get_user_config_dir()/GLADE_CONFIG_FILENAME"
 *
 * Returns: 0 on success.
 */
gint
glade_app_config_save ()
{
  GIOChannel *channel;
  GIOStatus status;
  gchar *data = NULL, *filename;
  const gchar *config_dir = g_get_user_config_dir ();
  GError *error = NULL;
  gsize size, written, bytes_written = 0;
  static gboolean error_shown = FALSE;
  GladeApp *app;
  GladeAppPrivate *priv;

  /* If we had any errors; wait untill next session to retry.
   */
  if (error_shown)
    return -1;

  app = glade_app_get ();
  priv = glade_app_get_instance_private (app);

  /* Just in case... try to create the config directory */
  if (g_file_test (config_dir, G_FILE_TEST_IS_DIR) == FALSE)
    {
      if (g_file_test (config_dir, G_FILE_TEST_EXISTS))
        {
          /* Config dir exists but is not a directory */
          glade_util_ui_message
              (glade_app_get_window (),
               GLADE_UI_ERROR, NULL,
               _("Trying to save private data to %s directory "
                 "but it is a regular file.\n"
                 "No private data will be saved in this session"), config_dir);
          error_shown = TRUE;
          return -1;
        }
      else if (g_mkdir (config_dir, S_IRWXU) != 0)
        {
          /* Doesnt exist; failed to create */
          glade_util_ui_message
              (glade_app_get_window (),
               GLADE_UI_ERROR, NULL,
               _("Failed to create directory %s to save private data.\n"
                 "No private data will be saved in this session"), config_dir);
          error_shown = TRUE;
          return -1;
        }
    }

  filename = g_build_filename (config_dir, GLADE_CONFIG_FILENAME, NULL);

  if ((channel = g_io_channel_new_file (filename, "w", &error)) != NULL)
    {
      if ((data =
           g_key_file_to_data (priv->config, &size, &error)) != NULL)
        {

          /* Implement loop here */
          while ((status = g_io_channel_write_chars (channel, data + bytes_written,     /* Offset of write */
                                                     size - bytes_written,      /* Size left to write */
                                                     &written,
                                                     &error)) !=
                 G_IO_STATUS_ERROR && (bytes_written + written) < size)
            bytes_written += written;

          if (status == G_IO_STATUS_ERROR)
            {
              glade_util_ui_message
                  (glade_app_get_window (),
                   GLADE_UI_ERROR, NULL,
                   _("Error writing private data to %s (%s).\n"
                     "No private data will be saved in this session"),
                   filename, error->message);
              error_shown = TRUE;
            }
          g_free (data);
        }
      else
        {
          glade_util_ui_message
              (glade_app_get_window (),
               GLADE_UI_ERROR, NULL,
               _("Error serializing configuration data to save (%s).\n"
                 "No private data will be saved in this session"),
               error->message);
          error_shown = TRUE;
        }
      g_io_channel_shutdown (channel, TRUE, NULL);
      g_io_channel_unref (channel);
    }
  else
    {
      glade_util_ui_message
          (glade_app_get_window (),
           GLADE_UI_ERROR, NULL,
           _("Error opening %s to write private data (%s).\n"
             "No private data will be saved in this session"),
           filename, error->message);
      error_shown = TRUE;
    }
  g_free (filename);

  if (error)
    {
      g_error_free (error);
      return -1;
    }
  return 0;
}

/**
 * glade_app_get:
 *
 * Returns: (transfer none): the #GladeApp
 */
GladeApp *
glade_app_get (void)
{
  if (!singleton_app)
    {
      singleton_app = glade_app_new ();
    }

  return singleton_app;
}

/**
 * glade_app_set_window:
 * @window: (transfer full): a #GtkWidget
 *
 * Set the window of the application
 */
void
glade_app_set_window (GtkWidget *window)
{
  GladeApp *app = glade_app_get ();
  GladeAppPrivate *priv = glade_app_get_instance_private (app);

  priv->window = window;
}

/**
 * glade_app_get_catalog:
 * @name: the name of the catalog
 *
 * Returns: (transfer none) (nullable): a #GladeCatalog or %NULL if none is found
 */
GladeCatalog *
glade_app_get_catalog (const gchar *name)
{
  GladeApp *app = glade_app_get ();
  GladeAppPrivate *priv = glade_app_get_instance_private (app);
  GList *list;
  GladeCatalog *catalog;

  g_return_val_if_fail (name && name[0], NULL);

  for (list = priv->catalogs; list; list = list->next)
    {
      catalog = list->data;
      if (!strcmp (glade_catalog_get_name (catalog), name))
        return catalog;
    }
  return NULL;
}

/**
 * glade_app_get_catalog_version:
 * @name: the name of the #GladeCatalog
 * @major: (out) (optional): the major version
 * @minor: (out) (optional): the minor version
 *
 * Returns: %TRUE if the catalog has been found. It is a programming error
 * to call this function with an unexisting catalog, returns %FALSE in this
 * case and throws a warning.
 */
gboolean
glade_app_get_catalog_version (const gchar *name, gint *major, gint *minor)
{
  GladeCatalog *catalog = glade_app_get_catalog (name);

  g_return_val_if_fail (catalog != NULL, FALSE);

  if (major)
    *major = glade_catalog_get_major_version (catalog);
  if (minor)
    *minor = glade_catalog_get_minor_version (catalog);

  return TRUE;
}

/**
 * glade_app_get_catalogs:
 *
 * Returns: (transfer none) (element-type GladeCatalog): a list of #GladeCatalog
 */
GList *
glade_app_get_catalogs (void)
{
  GladeApp *app = glade_app_get ();
  GladeAppPrivate *priv = glade_app_get_instance_private (app);

  return priv->catalogs;
}

/**
 * glade_app_get_window:
 *
 * Returns: (transfer none): a #GtkWidget
 */
GtkWidget *
glade_app_get_window (void)
{
  GladeApp *app = glade_app_get ();
  GladeAppPrivate *priv = glade_app_get_instance_private (app);

  return priv->window;
}

/**
 * glade_app_get_clipboard:
 *
 * Returns: (transfer none): a #GladeClipboard
 */
GladeClipboard *
glade_app_get_clipboard (void)
{
  GladeApp *app = glade_app_get ();
  GladeAppPrivate *priv = glade_app_get_instance_private (app);

  return priv->clipboard;
}

/**
 * glade_app_get_projects:
 *
 * Returns: (element-type GladeCatalog) (transfer none): a list of #GladeCatalog
 */
GList *
glade_app_get_projects (void)
{
  GladeApp *app = glade_app_get ();
  GladeAppPrivate *priv = glade_app_get_instance_private (app);

  return priv->projects;
}

/**
 * glade_app_get_config:
 *
 * Returns: (transfer full): a #GKeyFile
 */
GKeyFile *
glade_app_get_config (void)
{
  static GKeyFile *config = NULL;

  if (config == NULL)
    {
      gchar *filename = g_build_filename (g_get_user_config_dir (),
                                          GLADE_CONFIG_FILENAME, NULL);
      config = g_key_file_new ();
      g_key_file_load_from_file (config, filename, G_KEY_FILE_NONE, NULL);
      g_free (filename);
    }

  return config;
}

gboolean
glade_app_is_project_loaded (const gchar *project_path)
{
  GladeApp *app;
  GladeAppPrivate *priv;
  GList *list;
  gboolean loaded = FALSE;

  if (project_path == NULL)
    return FALSE;

  app = glade_app_get ();
  priv = glade_app_get_instance_private (app);

  for (list = priv->projects; list; list = list->next)
    {
      GladeProject *cur_project = GLADE_PROJECT (list->data);

      if ((loaded = glade_project_get_path (cur_project) &&
           (strcmp (glade_project_get_path (cur_project), project_path) == 0)))
        break;
    }

  return loaded;
}

/**
 * glade_app_get_project_by_path:
 * @project_path: The path of an open project
 *
 * Finds an open project with @path
 *
 * Returns: (nullable) (transfer none): A #GladeProject, or NULL if no such open project was found
 */
GladeProject *
glade_app_get_project_by_path (const gchar *project_path)
{
  GladeApp *app;
  GladeAppPrivate *priv;
  GList *l;
  gchar *canonical_path;

  if (project_path == NULL)
    return NULL;

  app = glade_app_get ();
  priv = glade_app_get_instance_private (app);

  canonical_path = glade_util_canonical_path (project_path);

  for (l = priv->projects; l; l = l->next)
    {
      GladeProject *project = (GladeProject *) l->data;

      if (glade_project_get_path (project) &&
          strcmp (canonical_path, glade_project_get_path (project)) == 0)
        {
          g_free (canonical_path);
          return project;
        }
    }

  g_free (canonical_path);

  return NULL;
}

/**
 * glade_app_add_project:
 * @project: the project to add to the #GladeApp
 */
void
glade_app_add_project (GladeProject *project)
{
  GladeApp *app;
  GladeAppPrivate *priv;

  g_return_if_fail (GLADE_IS_PROJECT (project));

  app = glade_app_get ();
  priv = glade_app_get_instance_private (app);

  /* If the project was previously loaded, don't re-load */
  if (g_list_find (priv->projects, project) != NULL)
    return;

  /* Take a reference for GladeApp here... */
  priv->projects = g_list_append (priv->projects, g_object_ref (project));
}

/**
 * glade_app_remove_project:
 * @project: the project to remove from the #GladeApp
 */
void
glade_app_remove_project (GladeProject *project)
{
  GladeApp *app;
  GladeAppPrivate *priv;
  g_return_if_fail (GLADE_IS_PROJECT (project));

  app = glade_app_get ();
  priv = glade_app_get_instance_private (app);

  priv->projects = g_list_remove (priv->projects, project);

  /* Its safe to just release the project as the project emits a
   * "close" signal and everyone is responsable for cleaning up at
   * that point.
   */
  g_object_unref (project);
}

/**
 * glade_app_set_accel_group:
 * @accel_group: (transfer full): a #GtkAccelGroup to set
 *
 * Sets @accel_group to app.
 * The acceleration group will made available for editor dialog windows
 * from the plugin backend.
 */
void
glade_app_set_accel_group (GtkAccelGroup *accel_group)
{
  GladeApp *app;
  GladeAppPrivate *priv;

  g_return_if_fail (GTK_IS_ACCEL_GROUP (accel_group));

  app = glade_app_get ();
  priv = glade_app_get_instance_private (app);

  priv->accel_group = accel_group;
}

/**
 * glade_app_get_accel_group:
 *
 * Returns: (transfer none): the #GtkAccelGroup
 */
GtkAccelGroup *
glade_app_get_accel_group (void)
{
  GladeApp *app = glade_app_get ();
  GladeAppPrivate *priv = glade_app_get_instance_private (app);

  return priv->accel_group;
}

/**
 * glade_app_new:
 *
 * Returns: (transfer full): the #GladeApp
 */
GladeApp *
glade_app_new (void)
{
  return g_object_new (GLADE_TYPE_APP, NULL);
}

/**
 * glade_app_search_docs:
 * @book: the name of a book
 * @page: the name of a page
 * @search: the search query
 *
 * Searches for @book, @page and @search in the documentation.
 */
void
glade_app_search_docs (const gchar *book,
                       const gchar *page, 
                       const gchar *search)
{
  GladeApp *app;

  app = glade_app_get ();

  g_signal_emit (G_OBJECT (app), glade_app_signals[DOC_SEARCH], 0, 
                 book, page, search);
}
