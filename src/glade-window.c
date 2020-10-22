/*
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2007 Vincent Geddes.
 * Copyright (C) 2012-2018 Juan Pablo Ugarte.
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
 *   Paolo Borelli <pborelli@katamail.com>
 *   Vincent Geddes <vgeddes@gnome.org>
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#include <config.h>

#include "glade-window.h"
#include "glade-resources.h"
#include "glade-preferences.h"
#include "glade-registration.h"
#include "glade-settings.h"
#include "glade-intro.h"

#include <gladeui/glade.h>
#include <gladeui/glade-popup.h>
#include <gladeui/glade-inspector.h>
#include <gladeui/glade-adaptor-chooser.h>
#include <gladeui/gladeui-enum-types.h>

#include <gladeui/glade-project.h>

#include <string.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#define READONLY_INDICATOR (_("[Read Only]"))

#define URL_DEVELOPER_MANUAL "http://library.gnome.org/devel/gladeui/"

#define CONFIG_GROUP_WINDOWS        "Glade Windows"
#define GLADE_WINDOW_DEFAULT_WIDTH  720
#define GLADE_WINDOW_DEFAULT_HEIGHT 540
#define CONFIG_KEY_X                "x"
#define CONFIG_KEY_Y                "y"
#define CONFIG_KEY_WIDTH            "width"
#define CONFIG_KEY_HEIGHT           "height"
#define CONFIG_KEY_MAXIMIZED        "maximized"

#define CONFIG_INTRO_GROUP          "Intro"
#define CONFIG_INTRO_DONE           "intro-done"

#define GLADE_WINDOW_ACTIVE_VIEW(w)  ((GladeDesignView *) gtk_stack_get_visible_child (w->priv->view_stack))
#define GLADE_WINDOW_GET_ACTION(w,a) ((w && ((GladeWindow *)w)->priv->application) ? g_action_map_lookup_action (G_ACTION_MAP (((GladeWindow *)w)->priv->application), a) : NULL)

struct _GladeWindowPrivate
{
  GladeApp *app;
  GtkApplication *application;

  GtkStack *stack;
  GtkStack *view_stack;

  GtkHeaderBar *headerbar;
  GtkWidget *project_switcher ;
  GtkWindow *about_dialog;
  GladeSettings *settings;

  GtkWidget *start_page;
  GtkLabel  *version_label;
  GtkWidget *intro_button;

  GladeAdaptorChooser *adaptor_chooser;
  GtkStack *inspectors_stack;           /* Cached per project inspectors */

  GladeEditor  *editor;                 /* The editor */

  GtkWidget *statusbar;                 /* A pointer to the status bar. */
  guint statusbar_context_id;           /* The context id of general messages */
  guint statusbar_menu_context_id;      /* The context id of the menu bar */
  guint statusbar_actions_context_id;   /* The context id of actions messages */

  GtkRecentManager *recent_manager;
  GtkWidget *recent_menu;

  gchar *default_path;          /* the default path for open/save operations */

  GtkWidget *undo_button;       /* undo/redo button, right click for history */
  GtkWidget *redo_button;

  GtkButtonBox *actions;        /* Actions are added to the headerbar */

  GtkWidget *center_paned;
  GtkWidget *left_paned;
  GtkWidget *open_button_box;   /* gtk_button_box_set_layout() set homogeneous to TRUE, and we do not want that in this case  */
  GtkWidget *save_button_box;

  GtkWidget *registration;      /* Registration and user survey dialog */

  GladeIntro *intro;
  GType new_type;

  GdkRectangle position;
};

static void check_reload_project (GladeWindow *window, GladeProject *project);

G_DEFINE_TYPE_WITH_PRIVATE (GladeWindow, glade_window, GTK_TYPE_WINDOW)

static void
refresh_title (GladeWindow *window)
{
  if (GLADE_WINDOW_ACTIVE_VIEW (window))
    {
      GladeProject *project = glade_design_view_get_project (GLADE_WINDOW_ACTIVE_VIEW (window));
      gchar *title;
      GList *p;

      gtk_header_bar_set_custom_title (window->priv->headerbar, NULL);

      title = glade_project_get_name (project);
      if (glade_project_get_modified (project))
        {
          gchar *old_title = g_steal_pointer (&title);
          title = g_strdup_printf ("*%s", old_title);
          g_free (old_title);
        }

      if (glade_project_get_readonly (project) != FALSE)
        {
          gchar *old_title = g_steal_pointer (&title);
          title = g_strdup_printf ("%s %s", old_title, READONLY_INDICATOR);
        }

      gtk_header_bar_set_title (window->priv->headerbar, title);
      g_free (title);

      if ((p = glade_app_get_projects ()) && g_list_next (p))
        gtk_header_bar_set_custom_title (window->priv->headerbar, window->priv->project_switcher);
      else
        {
          const gchar *path;

          /* Show path */
          if (project && (path = glade_project_get_path (project)))
            {
              gchar *dirname = g_path_get_dirname (path);
              const gchar *home = g_get_home_dir ();

              if (g_str_has_prefix (dirname, home))
                {
                  char *subtitle = &dirname[g_utf8_strlen (home, -1) - 1];
                  subtitle[0] = '~';
                  gtk_header_bar_set_subtitle (window->priv->headerbar, subtitle);
                }
              else
                gtk_header_bar_set_subtitle (window->priv->headerbar, dirname);

              g_free (dirname);
            }
          else
            gtk_header_bar_set_subtitle (window->priv->headerbar, NULL);
        }
    }
  else
    {
      gtk_header_bar_set_custom_title (window->priv->headerbar, NULL);
      gtk_header_bar_set_title (window->priv->headerbar, _("User Interface Designer"));
      gtk_header_bar_set_subtitle (window->priv->headerbar, NULL);
    }
}

static const gchar *
get_default_path (GladeWindow *window)
{
  return window->priv->default_path;
}

static void
update_default_path (GladeWindow *window, const gchar *filename)
{
  gchar *path;

  g_return_if_fail (filename != NULL);

  path = g_path_get_dirname (filename);

  g_free (window->priv->default_path);
  window->priv->default_path = g_strdup (path);

  g_free (path);
}

static void
activate_action (GtkToolButton *toolbutton, GladeWidgetAction *action)
{
  GladeWidget          *widget;
  GladeWidgetActionDef *adef = glade_widget_action_get_def (action);

  if ((widget = g_object_get_data (G_OBJECT (toolbutton), "glade-widget")))
    glade_widget_adaptor_action_activate (glade_widget_get_adaptor (widget),
                                          glade_widget_get_object (widget), 
                                          adef->path);
}

static void
action_notify_sensitive (GObject *gobject, GParamSpec *arg1, GtkWidget *item)
{
  GladeWidgetAction *action = GLADE_WIDGET_ACTION (gobject);
  gtk_widget_set_sensitive (item, glade_widget_action_get_sensitive (action));
}

static void
action_disconnect (gpointer data, GClosure *closure)
{
  g_signal_handlers_disconnect_matched (data, G_SIGNAL_MATCH_FUNC,
                                        0, 0, NULL,
                                        action_notify_sensitive, NULL);
}

static void
clean_actions (GladeWindow *window)
{
  GtkContainer *container = GTK_CONTAINER (window->priv->actions);
  g_autoptr (GList) children = gtk_container_get_children (container);
  GList *l;

  for (l = children; l; l = g_list_next(l))
    gtk_container_remove (container, l->data);
}

static void
add_actions (GladeWindow *window, GladeWidget *widget, GList *actions)
{
  GList *l;

  for (l = actions; l; l = g_list_next (l))
    {
      GladeWidgetAction    *action = l->data;
      GladeWidgetActionDef *adef   = glade_widget_action_get_def (action);
      GtkWidget *button;

      if (!adef->important || !glade_widget_action_get_visible (action))
        continue;

      if (glade_widget_action_get_children (action))
        {
          g_warning ("Trying to add a group action to the toolbar is unsupported");
          continue;
        }

      button = gtk_button_new_from_icon_name ((adef->stock) ? adef->stock : "system-run-symbolic",
                                              GTK_ICON_SIZE_MENU);

      if (adef->label)
        gtk_widget_set_tooltip_text (button, adef->label);

      g_object_set_data (G_OBJECT (button), "glade-widget", widget);

      /* We use destroy_data to keep track of notify::sensitive callbacks
       * on the action object and disconnect them when the toolbar item
       * gets destroyed.
       */
      g_signal_connect_data (button, "clicked",
                             G_CALLBACK (activate_action),
                             action, action_disconnect, 0);

      gtk_widget_set_sensitive (button, glade_widget_action_get_sensitive (action));

      g_signal_connect (action, "notify::sensitive",
                        G_CALLBACK (activate_action), button);

      gtk_widget_show (button);
      gtk_box_pack_start (GTK_BOX (window->priv->actions), button, FALSE, FALSE, 0);
    }
}

static GladeProject *
get_active_project (GladeWindow *window)
{
  if (GLADE_WINDOW_ACTIVE_VIEW (window))
    return glade_design_view_get_project (GLADE_WINDOW_ACTIVE_VIEW (window));

  return NULL;
}

static void
project_selection_changed_cb (GladeProject *project, GladeWindow *window)
{
  GladeProject *active_project;
  GladeWidget  *glade_widget = NULL;
  GList        *list;
  gint          num;

  clean_actions (window);

  active_project = get_active_project (window);

  /* This is sometimes called with a NULL project (to make the label
   * insensitive with no projects loaded)
   */
  g_return_if_fail (GLADE_IS_WINDOW (window));

  /* Only update the toolbar & workspace if the selection has changed on
   * the currently active project.
   */
  if (project == active_project)
    {
      list = glade_project_selection_get (project);
      num = g_list_length (list);

      if (num == 1 && !GLADE_IS_PLACEHOLDER (list->data))
        {
          glade_widget = glade_widget_get_from_gobject (G_OBJECT (list->data));

          if (glade_widget_get_actions (glade_widget))
            add_actions (window, glade_widget, glade_widget_get_actions (glade_widget));
        }
    }

  glade_editor_load_widget (window->priv->editor, glade_widget);
}

static void
refresh_stack_title_for_project (GladeWindow *window, GladeProject *project)
{
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (window->priv->view_stack));
  for (l = children; l; l = l->next)
    {
      GtkWidget *view = l->data;

      if (project == glade_design_view_get_project (GLADE_DESIGN_VIEW (view)))
        {
          gchar *name = glade_project_get_name (project);
          gchar *str;

          /* remove extension */
          if ((str = g_utf8_strrchr (name, -1, '.')))
            *str = '\0';

          if (glade_project_get_modified (project))
            str = g_strdup_printf ("*%s", name);
          else
            str = g_strdup (name);

          gtk_container_child_set (GTK_CONTAINER (window->priv->view_stack), view,
                                   "title", str, NULL);
          g_free (name);
          g_free (str);

          break;
        }
    }
  g_list_free (children);
}

static void
project_targets_changed_cb (GladeProject *project, GladeWindow *window)
{
  refresh_stack_title_for_project (window, project);
}

static inline void
actions_set_enabled (GladeWindow *window, const gchar *name, gboolean enabled)
{
  GAction *action;

  if ((action = GLADE_WINDOW_GET_ACTION (window, name)))
    g_simple_action_set_enabled (G_SIMPLE_ACTION (action), enabled);
}

static void
project_actions_set_enabled (GladeWindow *window, gboolean enabled)
{
  actions_set_enabled (window, "close", enabled);
  actions_set_enabled (window, "save", enabled);
  actions_set_enabled (window, "save_as", enabled);
  actions_set_enabled (window, "properties", enabled);
  actions_set_enabled (window, "undo", enabled);
  actions_set_enabled (window, "redo", enabled);
  actions_set_enabled (window, "cut", enabled);
  actions_set_enabled (window, "copy", enabled);
  actions_set_enabled (window, "delete", enabled);
  actions_set_enabled (window, "previous", enabled);
  actions_set_enabled (window, "next", enabled);
}

static void
pointer_mode_actions_set_enabled (GladeWindow *window, gboolean enabled)
{
  actions_set_enabled (window, "select", enabled);
  actions_set_enabled (window, "drag_resize", enabled);
  actions_set_enabled (window, "margin_edit", enabled);
  actions_set_enabled (window, "align_edit", enabled);
}

static void
refresh_undo_redo (GladeWindow *window, GladeProject *project)
{
  GladeCommand *undo = NULL, *redo = NULL;
  GladeWindowPrivate *priv = window->priv;
  const gchar *desc;
  gchar *tooltip;

  if (project != NULL)
    {
      undo = glade_project_next_undo_item (project);
      redo = glade_project_next_redo_item (project);
    }

  /* Refresh Undo */
  actions_set_enabled (window, "undo", undo != NULL);
  desc = undo ? glade_command_description (undo) : _("the last action");
  tooltip = g_strdup_printf (_("Undo: %s"), desc);
  gtk_widget_set_tooltip_text (priv->undo_button, tooltip);
  g_free (tooltip);

  /* Refresh Redo */
  actions_set_enabled (window, "redo", redo != NULL);
  desc = redo ? glade_command_description (redo) : _("the last action");
  tooltip = g_strdup_printf (_("Redo: %s"), desc);
  gtk_widget_set_tooltip_text (priv->redo_button, tooltip);
  g_free (tooltip);
}

static void
cancel_autosave (gpointer data)
{
  guint autosave_id = GPOINTER_TO_UINT (data);

  g_source_remove (autosave_id);
}

static gboolean
autosave_project (gpointer data)
{
  GladeProject *project = (GladeProject *)data;
  GladeWindow *window = GLADE_WINDOW (glade_app_get_window ());
  gchar *display_name;

  display_name = glade_project_get_name (project);

  if (glade_project_autosave (project, NULL))
    glade_util_flash_message (window->priv->statusbar,
                              window->priv->statusbar_actions_context_id,
                              _("Autosaving '%s'"), display_name);
  else
    /* This is problematic, should we be more intrusive and popup a dialog ? */
    glade_util_flash_message (window->priv->statusbar,
                              window->priv->statusbar_actions_context_id,
                              _("Error autosaving '%s'"), display_name);

  g_free (display_name);

  /* This will remove the source id */
  g_object_set_data (G_OBJECT (project), "glade-autosave-id", NULL);
  return FALSE;
}

static void
project_queue_autosave (GladeWindow *window, GladeProject *project)
{
  if (glade_project_get_path (project) != NULL &&
      glade_project_get_modified (project) &&
      glade_settings_autosave (window->priv->settings))
    {
      guint autosave_id =
        g_timeout_add_seconds (glade_settings_autosave_seconds (window->priv->settings),
                               autosave_project, project);

      g_object_set_data_full (G_OBJECT (project), "glade-autosave-id",
                              GUINT_TO_POINTER (autosave_id), cancel_autosave);
    }
  else
      g_object_set_data (G_OBJECT (project), "glade-autosave-id", NULL);
}

static void
project_cancel_autosave (GladeProject *project)
{
  g_object_set_data (G_OBJECT (project), "glade-autosave-id", NULL);
}

static void
project_changed_cb (GladeProject *project,
                    GladeCommand *command,
                    gboolean      execute,
                    GladeWindow  *window)
{
  GladeProject *active_project = get_active_project (window);

  if (project == active_project)
    refresh_undo_redo (window, project);

  project_queue_autosave (window, project);
}

static void
project_notify_handler_cb (GladeProject *project,
                           GParamSpec *spec,
                           GladeWindow *window)
{
  GladeProject *active_project = get_active_project (window);

  if (strcmp (spec->name, "path") == 0)
    {
      refresh_title (window);
      refresh_stack_title_for_project (window, project);
    }
  else if (strcmp (spec->name, "format") == 0)
    {
      refresh_stack_title_for_project (window, project);
    }
  else if (strcmp (spec->name, "modified") == 0)
    {
      refresh_title (window);
      refresh_stack_title_for_project (window, project);
    }
  else if (strcmp (spec->name, "read-only") == 0)
    {
      refresh_stack_title_for_project (window, project);

      actions_set_enabled (window, "save", !glade_project_get_readonly (project));
    }
  else if (strcmp (spec->name, "has-selection") == 0 && (project == active_project))
    {
      gboolean has_selection = glade_project_get_has_selection (project);
      actions_set_enabled (window, "cut", has_selection);
      actions_set_enabled (window, "copy", has_selection);
      actions_set_enabled (window, "delete", has_selection);
    }
}

static void
clipboard_notify_handler_cb (GladeClipboard *clipboard,
                             GParamSpec     *spec,
                             GladeWindow    *window)
{
  if (strcmp (spec->name, "has-selection") == 0)
    {
      actions_set_enabled (window, "paste",
                           glade_clipboard_get_has_selection (clipboard));
    }
}

static void
on_pointer_mode_changed (GladeProject *project,
                         GParamSpec   *pspec,
                         GladeWindow  *window)
{
  GladeProject *active_project = get_active_project (window);

  if (!active_project)
    {
      pointer_mode_actions_set_enabled (window, FALSE);
      return;
    }
  else if (active_project != project)
    return;

  if (glade_project_get_pointer_mode (project) == GLADE_POINTER_ADD_WIDGET)
    return;

  pointer_mode_actions_set_enabled (window, TRUE);
}

static void
set_sensitivity_according_to_project (GladeWindow  *window,
                                      GladeProject *project)
{
  gboolean has_selection = glade_project_get_has_selection (project);

  actions_set_enabled (window, "cut", has_selection);
  actions_set_enabled (window, "copy", has_selection);
  actions_set_enabled (window, "delete", has_selection);

  actions_set_enabled (window, "save",
                       !glade_project_get_readonly (project));

  actions_set_enabled (window, "paste",
                       glade_clipboard_get_has_selection (glade_app_get_clipboard ()));
}

static gchar *
get_uri_from_project_path (const gchar *path)
{
  GError *error = NULL;
  gchar *uri = NULL;

  if (g_path_is_absolute (path))
    uri = g_filename_to_uri (path, NULL, &error);
  else
    {
      gchar *cwd = g_get_current_dir ();
      gchar *fullpath = g_build_filename (cwd, path, NULL);
      uri = g_filename_to_uri (fullpath, NULL, &error);
      g_free (cwd);
      g_free (fullpath);
    }

  if (error)
    {
      g_warning ("Could not convert local path \"%s\" to a uri: %s", path, error->message);
      g_error_free (error);
    }

  return uri;
}

static void
recent_add (GladeWindow *window, const gchar *path)
{
  gchar *uri = get_uri_from_project_path (path);
  GtkRecentData *recent_data;

  if (!uri)
    return;

  recent_data = g_slice_new (GtkRecentData);

  recent_data->display_name = NULL;
  recent_data->description = NULL;
  recent_data->mime_type = "application/x-glade";
  recent_data->app_name = (gchar *) g_get_application_name ();
  recent_data->app_exec = g_strjoin (" ", g_get_prgname (), "%u", NULL);
  recent_data->groups = NULL;
  recent_data->is_private = FALSE;

  gtk_recent_manager_add_full (window->priv->recent_manager, uri, recent_data);

  g_free (uri);
  g_free (recent_data->app_exec);
  g_slice_free (GtkRecentData, recent_data);
}

static void
recent_remove (GladeWindow * window, const gchar * path)
{
  gchar *uri = get_uri_from_project_path (path);

  if (!uri)
    return;

  gtk_recent_manager_remove_item (window->priv->recent_manager, uri, NULL);

  g_free (uri);
}

/* switch to a project and check if we need to reload it.
 *
 */
static void
switch_to_project (GladeWindow *window, GladeProject *project)
{
  GladeWindowPrivate *priv = window->priv;
  GtkWidget *view;

  view = GTK_WIDGET (glade_design_view_get_from_project (project));
  gtk_stack_set_visible_child (priv->view_stack, view);

  check_reload_project (window, project);
}

static void
on_open_action_activate (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       data)
{
  GladeWindow *window = data;
  GtkWidget *filechooser;
  gchar *path = NULL, *default_path;

  filechooser = glade_util_file_dialog_new (_("Open\342\200\246"), NULL,
                                            GTK_WINDOW (window),
                                            GLADE_FILE_DIALOG_ACTION_OPEN);


  default_path = g_strdup (get_default_path (window));
  if (default_path != NULL)
    {
      gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (filechooser),
                                           default_path);
      g_free (default_path);
    }

  if (gtk_dialog_run (GTK_DIALOG (filechooser)) == GTK_RESPONSE_OK)
    path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filechooser));

  gtk_widget_destroy (filechooser);

  if (!path)
    return;

  glade_window_open_project (window, path);
  g_free (path);
}

static gboolean
check_loading_project_for_save (GladeProject *project)
{
  if (glade_project_is_loading (project))
    {
      gchar *name = glade_project_get_name (project);

      glade_util_ui_message (glade_app_get_window (),
                             GLADE_UI_INFO, NULL,
                             _("Project %s is still loading."), name);
      g_free (name);
      return TRUE;
    }
  return FALSE;
}

static gboolean
do_save (GladeWindow *window, GladeProject *project, const gchar *path)
{
  GError *error = NULL;
  GladeVerifyFlags verify_flags = glade_settings_get_verify_flags (window->priv->settings);
  gchar *display_path = g_strdup (path);

  if (glade_settings_backup (window->priv->settings) &&
      !glade_project_backup (project, path, NULL))
    {
      if (!glade_util_ui_message (GTK_WIDGET (window),
                                  GLADE_UI_ARE_YOU_SURE, NULL,
                                  _("Failed to backup existing file, continue saving?")))
        {
          g_free (display_path);
          return FALSE;
        }
    }

  if (!glade_project_save_verify (project, path, verify_flags, &error))
    {
      if (error)
        {
          /* Reset path so future saves will prompt the file chooser */
          glade_project_reset_path (project);

          glade_util_ui_message (GTK_WIDGET (window), GLADE_UI_ERROR, NULL,
                                 _("Failed to save %s: %s"),
                                 display_path, error->message);
          g_error_free (error);
        }
      g_free (display_path);
      return FALSE;
    }

  /* Cancel any queued autosave when explicitly saving */
  project_cancel_autosave (project);

  g_free (display_path);
  return TRUE;
}

static void
save (GladeWindow *window, GladeProject *project, const gchar *path)
{
  gchar *display_name;
  time_t mtime;
  GtkWidget *dialog;
  GtkWidget *button;
  gint response;

  if (check_loading_project_for_save (project))
    return;

  /* check for external modification to the project file */
  if (glade_project_get_path (project))
    {
      mtime = glade_util_get_file_mtime (glade_project_get_path (project), NULL);

      if (mtime > glade_project_get_file_mtime (project))
        {

          dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                           GTK_DIALOG_MODAL,
                                           GTK_MESSAGE_WARNING,
                                           GTK_BUTTONS_NONE,
                                           _("The file %s has been modified since reading it"),
                                           glade_project_get_path (project));

          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                    _("If you save it, all the external changes could be lost. "
                                                    "Save it anyway?"));

          gtk_window_set_title (GTK_WINDOW (dialog), "");

          button = gtk_button_new_with_mnemonic (_("_Save Anyway"));
          gtk_button_set_image (GTK_BUTTON (button),
                                gtk_image_new_from_icon_name ("document-save",
                                                                      GTK_ICON_SIZE_BUTTON));
          gtk_widget_show (button);

          gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button,
                                        GTK_RESPONSE_ACCEPT);
          gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Don't Save"),
                                 GTK_RESPONSE_REJECT);

          gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_REJECT);

          response = gtk_dialog_run (GTK_DIALOG (dialog));

          gtk_widget_destroy (dialog);

          if (response == GTK_RESPONSE_REJECT)
            return;
      }
    }

  /* Interestingly; we cannot use `path' after glade_project_reset_path
   * because we are getting called with glade_project_get_path (project) as an argument.
   */
  if (!do_save (window, project, path))
    return;

  /* Get display_name here, it could have changed with "Save As..." */
  display_name = glade_project_get_name (project);

  recent_add (window, glade_project_get_path (project));
  update_default_path (window, glade_project_get_path (project));

  /* refresh names */
  refresh_title (window);
  refresh_stack_title_for_project (window, project);

  glade_util_flash_message (window->priv->statusbar,
                            window->priv->statusbar_actions_context_id,
                            _("Project '%s' saved"), display_name);

  g_free (display_name);
}

static gboolean
path_has_extension (const gchar *path)
{
  gchar *basename = g_path_get_basename (path);
  gboolean retval = g_utf8_strrchr (basename, -1, '.') != NULL;
  g_free (basename);
  return retval;
}

static void
save_as (GladeWindow *window)
{
  GladeProject *project, *another_project;
  GtkWidget *filechooser;
  GtkWidget *dialog;
  gchar *path = NULL;
  gchar *project_name;

  project = glade_design_view_get_project (GLADE_WINDOW_ACTIVE_VIEW (window));

  if (project == NULL)
    return;

  if (check_loading_project_for_save (project))
    return;

  filechooser = glade_util_file_dialog_new (_("Save As\342\200\246"), project,
                                            GTK_WINDOW (window),
                                            GLADE_FILE_DIALOG_ACTION_SAVE);

  if (glade_project_get_path (project))
    {
      gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (filechooser),
                                     glade_project_get_path (project));
    }
  else
    {
      gchar *default_path = g_strdup (get_default_path (window));
      if (default_path != NULL)
        {
          gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (filechooser),
                                               default_path);
          g_free (default_path);
        }

      project_name = glade_project_get_name (project);
      gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (filechooser),
                                         project_name);
      g_free (project_name);
    }

  while (gtk_dialog_run (GTK_DIALOG (filechooser)) == GTK_RESPONSE_OK)
    {
      path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filechooser));

      /* Check if selected filename has an extension or not */
      if (!path_has_extension (path))
        {
          gchar *real_path = g_strconcat (path, ".glade", NULL);

          g_free (path);
          path = real_path;

          /* We added .glade extension!,
           * check if file exist to avoid overwriting a file without asking
           */
          if (g_file_test (path, G_FILE_TEST_EXISTS))
            {
              /* Set existing filename and let filechooser ask about overwriting */
              gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (filechooser), path);
              g_free (path);
              path = NULL;
              continue;
            }
        }
      break;
    }

  gtk_widget_destroy (filechooser);

  if (!path)
    return;

  /* checks if selected path is actually writable */
  if (glade_util_file_is_writeable (path) == FALSE)
    {
      dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                       GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_OK,
                                       _("Could not save the file %s"),
                                       path);

      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                _("You do not have the permissions "
                                                  "necessary to save the file."));

      gtk_window_set_title (GTK_WINDOW (dialog), "");

      g_signal_connect_swapped (dialog, "response",
                                G_CALLBACK (gtk_widget_destroy), dialog);

      gtk_widget_show (dialog);
      g_free (path);
      return;
    }

  /* checks if another open project is using selected path */
  if ((another_project = glade_app_get_project_by_path (path)) != NULL)
    {
      if (project != another_project)
        {

          glade_util_ui_message (GTK_WIDGET (window),
                                 GLADE_UI_ERROR, NULL,
                                 _
                                 ("Could not save file %s. Another project with that path is open."),
                                 path);

          g_free (path);
          return;
        }

    }

  save (window, project, path);

  g_free (path);
}

static void
on_save_action_activate (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       data)
{
  GladeWindow *window = data;
  GladeProject *project;

  project = glade_design_view_get_project (GLADE_WINDOW_ACTIVE_VIEW (window));

  if (project == NULL)
    {
      /* Just in case the menu-item or button is not insensitive */
      glade_util_ui_message (GTK_WIDGET (window), GLADE_UI_WARN, NULL,
                             _("No open projects to save"));
      return;
    }

  if (glade_project_get_path (project) != NULL)
    {
      save (window, project, glade_project_get_path (project));
      return;
    }

  /* If instead we dont have a path yet, fire up a file selector */
  save_as (window);
}

static void
on_save_as_action_activate (GSimpleAction *action,
                            GVariant      *parameter,
                            gpointer       data)
{
  GladeWindow *window = data;
  save_as (window);
}

static gboolean
confirm_close_project (GladeWindow *window, GladeProject *project)
{
  GtkWidget *dialog;
  gboolean close = FALSE;
  gchar *msg, *project_name = NULL;
  gint ret;

  project_name = glade_project_get_name (project);

  msg = g_strdup_printf (_("Save changes to project \"%s\" before closing?"),
                         project_name);

  dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                   GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_NONE, "%s", msg);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            "%s", _("Your changes will be lost if you don't save them."));
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          _("Close _without Saving"), GTK_RESPONSE_NO,
                          _("_Cancel"), GTK_RESPONSE_CANCEL,
                          _("_Save"), GTK_RESPONSE_YES,
                          NULL);

#ifdef G_OS_WIN32
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_YES,
                                           GTK_RESPONSE_CANCEL,
                                           GTK_RESPONSE_NO, -1);
#endif

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);

  ret = gtk_dialog_run (GTK_DIALOG (dialog));
  switch (ret)
    {
      case GTK_RESPONSE_YES:
        /* if YES we save the project: note we cannot use save_cb
         * since it saves the current project, while the modified
         * project we are saving may be not the current one.
         */
        if (glade_project_get_path (project) != NULL)
          {
            close = do_save (window, project, glade_project_get_path (project));
          }
        else
          {
            GtkWidget *filechooser;
            gchar *path = NULL;
            gchar *default_path;

            filechooser =
                glade_util_file_dialog_new (_("Save\342\200\246"), project,
                                            GTK_WINDOW (window),
                                            GLADE_FILE_DIALOG_ACTION_SAVE);

            default_path = g_strdup (get_default_path (window));
            if (default_path != NULL)
              {
                gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER
                                                     (filechooser),
                                                     default_path);
                g_free (default_path);
              }

            gtk_file_chooser_set_current_name
                (GTK_FILE_CHOOSER (filechooser), project_name);


            if (gtk_dialog_run (GTK_DIALOG (filechooser)) == GTK_RESPONSE_OK)
              path =
                  gtk_file_chooser_get_filename (GTK_FILE_CHOOSER
                                                 (filechooser));

            gtk_widget_destroy (filechooser);

            if (!path)
              break;

            save (window, project, path);

            g_free (path);

            close = FALSE;
          }
        break;
      case GTK_RESPONSE_NO:
        close = TRUE;
        break;
      case GTK_RESPONSE_CANCEL:
      case GTK_RESPONSE_DELETE_EVENT:
        close = FALSE;
        break;
      default:
        g_assert_not_reached ();
        close = FALSE;
    }

  g_free (msg);
  g_free (project_name);
  gtk_widget_destroy (dialog);

  return close;
}

static void
close_project (GladeWindow *window, GladeProject *project)
{
  GladeDesignView *view = glade_design_view_get_from_project (project);
  GladeWindowPrivate *priv = window->priv;

  /* Cancel any queued autosave activity */
  project_cancel_autosave (project);

  if (glade_project_is_loading (project))
    {
      glade_project_cancel_load (project);
      return;
    }

  g_signal_handlers_disconnect_by_func (project, project_notify_handler_cb, window);
  g_signal_handlers_disconnect_by_func (project, project_selection_changed_cb, window);
  g_signal_handlers_disconnect_by_func (project, project_targets_changed_cb, window);
  g_signal_handlers_disconnect_by_func (project, project_changed_cb, window);
  g_signal_handlers_disconnect_by_func (project, on_pointer_mode_changed, window);

  /* remove inspector first */
  gtk_container_remove (GTK_CONTAINER (priv->inspectors_stack),
                        g_object_get_data (G_OBJECT (view), "glade-window-view-inspector"));

  /* then the main view */
  gtk_container_remove (GTK_CONTAINER (priv->view_stack), GTK_WIDGET (view));

  clean_actions (window);

  /* Refresh the editor and some of the actions */
  project_selection_changed_cb (project, window);

  on_pointer_mode_changed (project, NULL, window);

  glade_app_remove_project (project);

  refresh_title (window);

  if (!glade_app_get_projects ())
    gtk_stack_set_visible_child (priv->stack, priv->start_page);

  if (GLADE_WINDOW_ACTIVE_VIEW (window))
    set_sensitivity_according_to_project (window,
                                          glade_design_view_get_project
                                          (GLADE_WINDOW_ACTIVE_VIEW (window)));
  else
    project_actions_set_enabled (window, FALSE);
}

static void
on_close_action_activate (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       data)
{
  GladeWindow *window = data;
  GladeDesignView *view;
  GladeProject *project;
  gboolean close;

  view = GLADE_WINDOW_ACTIVE_VIEW (window);

  project = glade_design_view_get_project (view);

  if (view == NULL)
    return;

  if (glade_project_get_modified (project))
    {
      close = confirm_close_project (window, project);
      if (!close)
        return;
    }
  close_project (window, project);
}

static void
on_copy_action_activate (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       data)
{
  GladeWindow *window = data;
  GladeProject *project;

  if (!GLADE_WINDOW_ACTIVE_VIEW (window))
    return;

  project = glade_design_view_get_project (GLADE_WINDOW_ACTIVE_VIEW (window));

  glade_project_copy_selection (project);
}

static void
on_cut_action_activate (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       data)
{
  GladeWindow *window = data;
  GladeProject *project;

  if (!GLADE_WINDOW_ACTIVE_VIEW (window))
    return;

  project = glade_design_view_get_project (GLADE_WINDOW_ACTIVE_VIEW (window));

  glade_project_command_cut (project);
}

static void
on_paste_action_activate (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       data)
{
  GladeWindow *window = data;
  GtkWidget *placeholder;
  GladeProject *project;

  if (!GLADE_WINDOW_ACTIVE_VIEW (window))
    return;

  project = glade_design_view_get_project (GLADE_WINDOW_ACTIVE_VIEW (window));
  placeholder = glade_util_get_placeholder_from_pointer (GTK_CONTAINER (window));

  /* If this action is activated with a key binging (ctrl-v) the widget will be
   * pasted over the placeholder below the default pointer.
   */
  glade_project_command_paste (project, placeholder ? GLADE_PLACEHOLDER (placeholder) : NULL);
}

static void
on_delete_action_activate (GSimpleAction *action,
                           GVariant      *parameter,
                           gpointer       data)
{
  GladeWindow *window = data;
  GladeProject *project;

  if (!GLADE_WINDOW_ACTIVE_VIEW (window))
    return;

  project = glade_design_view_get_project (GLADE_WINDOW_ACTIVE_VIEW (window));

  glade_project_command_delete (project);
}

static void
stack_visible_child_next_prev (GladeWindow *window, gboolean next)
{
  GladeDesignView *view;
  GList *children, *node;

  if (!(view = GLADE_WINDOW_ACTIVE_VIEW (window)))
    return;

  children = gtk_container_get_children (GTK_CONTAINER (window->priv->view_stack));

  if ((node = g_list_find (children, view)) &&
      ((next && node->next) || (!next && node->prev)))
    gtk_stack_set_visible_child (window->priv->view_stack,
                                 (next) ? node->next->data : node->prev->data);

  g_list_free (children);
}

static void
on_previous_action_activate (GSimpleAction *action,
                             GVariant      *parameter,
                             gpointer       data)
{
  GladeWindow *window = data;
  stack_visible_child_next_prev (window, FALSE);
}

static void
on_next_action_activate (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       data)
{
  GladeWindow *window = data;
  stack_visible_child_next_prev (window, TRUE);
}

static void
on_properties_action_activate (GSimpleAction *action,
                               GVariant      *parameter,
                               gpointer       data)
{
  GladeWindow *window = data;
  GladeProject *project;

  if (!GLADE_WINDOW_ACTIVE_VIEW (window))
    return;

  project = glade_design_view_get_project (GLADE_WINDOW_ACTIVE_VIEW (window));

  glade_project_properties (project);
}

static void
on_undo_action_activate (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       data)
{
  GladeWindow *window = data;
  GladeProject *active_project = get_active_project (window);

  if (!active_project)
    {
      g_warning ("undo should not be sensitive: we don't have a project");
      return;
    }

  glade_project_undo (active_project);
}

static void
on_redo_action_activate (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       data)
{
  GladeWindow *window = data;
  GladeProject *active_project = get_active_project (window);

  if (!active_project)
    {
      g_warning ("redo should not be sensitive: we don't have a project");
      return;
    }

  glade_project_redo (active_project);
}

static void
doc_search_cb (GladeEditor *editor,
               const gchar *book,
               const gchar *page,
               const gchar *search,
               GladeWindow *window)
{
  glade_util_search_devhelp (book, page, search);
}

static void
on_stack_visible_child_notify (GObject    *gobject,
                               GParamSpec *pspec,
                               GladeWindow *window)
{
  GladeDesignView *view = GLADE_WINDOW_ACTIVE_VIEW (window);
  GladeWindowPrivate *priv = window->priv;

  if (view)
    {
      GladeProject *project = glade_design_view_get_project (view);

      /* switch to the project's inspector */
      gtk_stack_set_visible_child (priv->inspectors_stack,
                                   g_object_get_data (G_OBJECT (view), "glade-window-view-inspector"));

      glade_adaptor_chooser_set_project (priv->adaptor_chooser, project);

      set_sensitivity_according_to_project (window, project);

      refresh_undo_redo (window, project);

      /* Refresh the editor and some of the actions */
      project_selection_changed_cb (project, window);

      on_pointer_mode_changed (project, NULL, window);
    }
}

static void
on_open_recent_action_item_activated (GtkRecentChooser *chooser,
                                      GladeWindow *window)
{
  gchar *uri, *path;
  GError *error = NULL;

  uri = gtk_recent_chooser_get_current_uri (chooser);

  path = g_filename_from_uri (uri, NULL, NULL);
  if (error)
    {
      g_warning ("Could not convert uri \"%s\" to a local path: %s", uri,
                 error->message);
      g_error_free (error);
      return;
    }

  glade_window_open_project (window, path);

  g_free (uri);
  g_free (path);
}

static void
on_reference_action_activate (GSimpleAction *action,
                              GVariant      *parameter,
                              gpointer       data)
{
  if (glade_util_have_devhelp ())
    {
      glade_util_search_devhelp ("gladeui", NULL, NULL);
      return;
    }

  /* fallback to displaying online developer manual */
  glade_util_url_show (URL_DEVELOPER_MANUAL);
}

static void
on_preferences_action_activate (GSimpleAction *action,
                                GVariant      *parameter,
                                gpointer       data)
{
  GladeWindow *window = data;
  GladeWindowPrivate *priv = window->priv;
  GtkWidget *preferences = glade_preferences_new (priv->settings);

  gtk_window_set_transient_for (GTK_WINDOW (preferences), GTK_WINDOW (window));
  gtk_widget_show (preferences);

  gtk_dialog_run (GTK_DIALOG (preferences));
  gtk_widget_destroy (preferences);
}

static void
on_about_action_activate (GSimpleAction *action,
                          GVariant      *parameter,
                          gpointer       data)
{
  GladeWindow *window = data;
  GladeWindowPrivate *priv = window->priv;

  gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (priv->about_dialog), PACKAGE_VERSION);

  gtk_window_present (priv->about_dialog);
}

enum
{
  TARGET_URI_LIST
};

static GtkTargetEntry drop_types[] = {
  {"text/uri-list", 0, TARGET_URI_LIST}
};

static void
drag_data_received (GtkWidget *widget,
                    GdkDragContext *context,
                    gint x, gint y,
                    GtkSelectionData *selection_data,
                    guint info,
                    guint time,
                    GladeWindow *window)
{
  gchar **uris, **str;
  const guchar *data;

  if (info != TARGET_URI_LIST)
    return;

  data = gtk_selection_data_get_data (selection_data);

  uris = g_uri_list_extract_uris ((gchar *) data);

  for (str = uris; *str; str++)
    {
      GError *error = NULL;
      gchar *path = g_filename_from_uri (*str, NULL, &error);

      if (path)
        {
          glade_window_open_project (window, path);
        }
      else
        {
          g_warning ("Could not convert uri to local path: %s", error->message);

          g_error_free (error);
        }
      g_free (path);
    }
  g_strfreev (uris);
}

static gboolean
delete_event (GtkWindow *w, GdkEvent *event, GladeWindow *window)
{
  GladeWindowPrivate *priv = window->priv;

  g_action_group_activate_action (G_ACTION_GROUP (priv->application), "quit", NULL);

  /* return TRUE to stop other handlers */
  return TRUE;
}

static void
add_project (GladeWindow *window, GladeProject *project, gboolean for_file)
{
  GladeWindowPrivate *priv = window->priv;
  GtkWidget *view, *inspector;

  g_return_if_fail (GLADE_IS_PROJECT (project));

  /* Create a new view for project */
  view = glade_design_view_new (project);

  gtk_stack_set_visible_child (priv->stack, priv->center_paned);

  g_signal_connect (G_OBJECT (project), "notify::modified",
                    G_CALLBACK (project_notify_handler_cb), window);
  g_signal_connect (G_OBJECT (project), "notify::path",
                    G_CALLBACK (project_notify_handler_cb), window);
  g_signal_connect (G_OBJECT (project), "notify::format",
                    G_CALLBACK (project_notify_handler_cb), window);
  g_signal_connect (G_OBJECT (project), "notify::has-selection",
                    G_CALLBACK (project_notify_handler_cb), window);
  g_signal_connect (G_OBJECT (project), "notify::read-only",
                    G_CALLBACK (project_notify_handler_cb), window);
  g_signal_connect (G_OBJECT (project), "notify::pointer-mode",
                    G_CALLBACK (on_pointer_mode_changed), window);
  g_signal_connect (G_OBJECT (project), "selection-changed",
                    G_CALLBACK (project_selection_changed_cb), window);
  g_signal_connect (G_OBJECT (project), "targets-changed",
                    G_CALLBACK (project_targets_changed_cb), window);
  g_signal_connect (G_OBJECT (project), "changed",
                    G_CALLBACK (project_changed_cb), window);

  /* create inspector */
  inspector = glade_inspector_new ();
  g_object_set_data (G_OBJECT (view), "glade-window-view-inspector", inspector);
  glade_inspector_set_project (GLADE_INSPECTOR (inspector), project);
  gtk_container_add (GTK_CONTAINER (priv->inspectors_stack), inspector);
  gtk_widget_show (inspector);

  set_sensitivity_according_to_project (window, project);

  project_actions_set_enabled (window, TRUE);

  /* Pass ownership of the project to the app */
  glade_app_add_project (project);
  g_object_unref (project);

  /* Add view to stack */
  gtk_container_add (GTK_CONTAINER (priv->view_stack), view);
  gtk_widget_show (view);
  gtk_stack_set_visible_child (priv->view_stack, view);

  refresh_stack_title_for_project (window, project);
  refresh_title (window);
}

static void
on_registration_action_activate (GSimpleAction *action,
                                 GVariant      *parameter,
                                 gpointer       data)
{
  GladeWindow *window = data;
  gtk_window_present (GTK_WINDOW (window->priv->registration));
}

static gboolean
on_undo_button_button_press_event (GtkWidget   *widget,
                                   GdkEvent    *event,
                                   GladeWindow *window)
{
  GladeProject *project = get_active_project (window);

  if (project && event->button.button == 3)
    gtk_menu_popup_at_widget (GTK_MENU (glade_project_undo_items (project)),
                              widget,
                              GDK_GRAVITY_NORTH_WEST,
                              GDK_GRAVITY_SOUTH_WEST,
                              event);
  return FALSE;
}

static gboolean
on_redo_button_button_press_event (GtkWidget   *widget,
                                   GdkEvent    *event,
                                   GladeWindow *window)
{
  GladeProject *project = get_active_project (window);

  if (project && event->button.button == 3)
    gtk_menu_popup_at_widget (GTK_MENU (glade_project_redo_items (project)),
                              widget,
                              GDK_GRAVITY_NORTH_WEST,
                              GDK_GRAVITY_SOUTH_WEST,
                              event);
  return FALSE;
}

void
glade_window_new_project (GladeWindow *window)
{
  GladeProject *project;

  g_return_if_fail (GLADE_IS_WINDOW (window));

  project = glade_project_new ();
  if (!project)
    {
      glade_util_ui_message (GTK_WIDGET (window),
                             GLADE_UI_ERROR, NULL,
                             _("Could not create a new project."));
      return;
    }
  add_project (window, project, FALSE);
}

static void
on_new_action_activate (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       data)
{
  glade_window_new_project (data);
}

static gboolean
open_project (GladeWindow *window, const gchar *path)
{
  GladeProject *project;

  project = glade_project_new ();

  add_project (window, project, TRUE);
  update_default_path (window, path);

  if (!glade_project_load_from_file (project, path))
    {
      close_project (window, project);

      recent_remove (window, path);
      return FALSE;
    }

  /* increase project popularity */
  recent_add (window, glade_project_get_path (project));

  return TRUE;
}

static void
check_reload_project (GladeWindow *window, GladeProject *project)
{
  gchar *path;
  GtkWidget *dialog;
  GtkWidget *button;
  gint response;

  /* Reopen the project if it has external modifications.
   * Prompt for permission to reopen.
   */
  if ((glade_util_get_file_mtime (glade_project_get_path (project), NULL)
       <= glade_project_get_file_mtime (project)))
    {
      return;
    }

  if (glade_project_get_modified (project))
    {
      dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                       GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_WARNING,
                                       GTK_BUTTONS_NONE,
                                       _("The project %s has unsaved changes"),
                                       glade_project_get_path (project));

      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                _
                                                ("If you reload it, all unsaved changes "
                                                 "could be lost. Reload it anyway?"));
    }
  else
    {
      dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                       GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_WARNING,
                                       GTK_BUTTONS_NONE,
                                       _
                                       ("The project file %s has been externally modified"),
                                       glade_project_get_path (project));

      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                _
                                                ("Do you want to reload the project?"));

    }

  gtk_window_set_title (GTK_WINDOW (dialog), "");

  button = gtk_button_new_with_mnemonic (_("_Reload"));
  gtk_button_set_image (GTK_BUTTON (button),
                        gtk_image_new_from_icon_name ("view-refresh",
                                                      GTK_ICON_SIZE_BUTTON));
  gtk_widget_show (button);

  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_REJECT);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button,
                                GTK_RESPONSE_ACCEPT);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT);

  response = gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);

  if (response == GTK_RESPONSE_REJECT)
    {
      return;
    }

  /* Reopen */
  path = g_strdup (glade_project_get_path (project));

  close_project (window, project);
  open_project (window, path);
  g_free (path);
}

/**
 * glade_window_open_project:
 * @window: a #GladeWindow
 * @path: the filesystem path of the project
 *
 * Opens a project file. If the project is already open, switch to that
 * project.
 *
 * Returns: #TRUE if the project was opened
 */
gboolean
glade_window_open_project (GladeWindow *window, const gchar *path)
{
  GladeProject *project;

  g_return_val_if_fail (GLADE_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (path != NULL, FALSE);

  /* dont allow a project to be opened twice */
  project = glade_app_get_project_by_path (path);
  if (project)
    {
      /* just switch to the project */
      switch_to_project (window, project);
      return TRUE;
    }
  else
    {
      return open_project (window, path);
    }
}

static void
glade_window_dispose (GObject *object)
{
  GladeWindow *window = GLADE_WINDOW (object);

  g_clear_object (&window->priv->app);
  g_clear_object (&window->priv->registration);

  G_OBJECT_CLASS (glade_window_parent_class)->dispose (object);
}

static void
glade_window_finalize (GObject *object)
{
  g_free (GLADE_WINDOW (object)->priv->default_path);

  G_OBJECT_CLASS (glade_window_parent_class)->finalize (object);
}


static gboolean
glade_window_configure_event (GtkWidget *widget, GdkEventConfigure *event)
{
  GladeWindow *window = GLADE_WINDOW (widget);
  gboolean retval;

  gboolean is_maximized;
  GdkWindow *gdk_window = gtk_widget_get_window (GTK_WIDGET (window));
  is_maximized = gdk_window_get_state (gdk_window) & GDK_WINDOW_STATE_MAXIMIZED;

  if (!is_maximized)
    {
      window->priv->position.width = event->width;
      window->priv->position.height = event->height;
    }

  retval =
      GTK_WIDGET_CLASS (glade_window_parent_class)->configure_event (widget,
                                                                     event);

  if (!is_maximized)
    {
      gtk_window_get_position (GTK_WINDOW (widget),
                               &window->priv->position.x,
                               &window->priv->position.y);
    }

  return retval;
}

static void
key_file_set_window_position (GKeyFile *config,
                              GdkRectangle *position,
                              const char *id,
                              gboolean maximized)
{
  char *key_x, *key_y, *key_width, *key_height, *key_maximized;

  key_x = g_strdup_printf ("%s-" CONFIG_KEY_X, id);
  key_y = g_strdup_printf ("%s-" CONFIG_KEY_Y, id);
  key_width = g_strdup_printf ("%s-" CONFIG_KEY_WIDTH, id);
  key_height = g_strdup_printf ("%s-" CONFIG_KEY_HEIGHT, id);
  key_maximized = g_strdup_printf ("%s-" CONFIG_KEY_MAXIMIZED, id);

  if (position->x > G_MININT)
    g_key_file_set_integer (config, CONFIG_GROUP_WINDOWS, key_x, position->x);
  if (position->y > G_MININT)
    g_key_file_set_integer (config, CONFIG_GROUP_WINDOWS, key_y, position->y);

  g_key_file_set_integer (config, CONFIG_GROUP_WINDOWS,
                          key_width, position->width);
  g_key_file_set_integer (config, CONFIG_GROUP_WINDOWS,
                          key_height, position->height);

  g_key_file_set_boolean (config, CONFIG_GROUP_WINDOWS,
                          key_maximized, maximized);


  g_free (key_maximized);
  g_free (key_height);
  g_free (key_width);
  g_free (key_y);
  g_free (key_x);
}

static void
save_windows_config (GladeWindow *window, GKeyFile *config)
{
  GladeWindowPrivate *priv = window->priv;
  GdkWindow *gdk_window;
  gboolean maximized;

  gdk_window = gtk_widget_get_window (GTK_WIDGET (window));
  maximized = gdk_window_get_state (gdk_window) & GDK_WINDOW_STATE_MAXIMIZED;

  key_file_set_window_position (config, &priv->position, "main", maximized);
}

static void
save_paned_position (GKeyFile *config, GtkWidget *paned, const gchar *name)
{
  g_key_file_set_integer (config, name, "position",
                          gtk_paned_get_position (GTK_PANED (paned)));
}

static void
glade_window_config_save (GladeWindow * window)
{
  GKeyFile *config = glade_app_get_config ();

  save_windows_config (window, config);

  /* Save main window paned positions */
  save_paned_position (config, window->priv->center_paned, "center_pane");
  save_paned_position (config, window->priv->left_paned, "left_pane");

  glade_settings_save (window->priv->settings, config);

  glade_app_config_save ();
}

static int
key_file_get_int (GKeyFile *config,
                  const char *group,
                  const char *key,
                  int default_value)
{
  if (g_key_file_has_key (config, group, key, NULL))
    return g_key_file_get_integer (config, group, key, NULL);
  else
    return default_value;
}

static void
key_file_get_window_position (GKeyFile *config,
                              const char *id,
                              GdkRectangle *pos,
                              gboolean *maximized)
{
  char *key_x, *key_y, *key_width, *key_height, *key_maximized;

  key_x = g_strdup_printf ("%s-" CONFIG_KEY_X, id);
  key_y = g_strdup_printf ("%s-" CONFIG_KEY_Y, id);
  key_width = g_strdup_printf ("%s-" CONFIG_KEY_WIDTH, id);
  key_height = g_strdup_printf ("%s-" CONFIG_KEY_HEIGHT, id);
  key_maximized = g_strdup_printf ("%s-" CONFIG_KEY_MAXIMIZED, id);

  pos->x = key_file_get_int (config, CONFIG_GROUP_WINDOWS, key_x, pos->x);
  pos->y = key_file_get_int (config, CONFIG_GROUP_WINDOWS, key_y, pos->y);
  pos->width =
      key_file_get_int (config, CONFIG_GROUP_WINDOWS, key_width, pos->width);
  pos->height =
      key_file_get_int (config, CONFIG_GROUP_WINDOWS, key_height, pos->height);

  if (maximized)
    {
      if (g_key_file_has_key
          (config, CONFIG_GROUP_WINDOWS, key_maximized, NULL))
        *maximized =
            g_key_file_get_boolean (config, CONFIG_GROUP_WINDOWS, key_maximized,
                                    NULL);
      else
        *maximized = FALSE;
    }

  g_free (key_x);
  g_free (key_y);
  g_free (key_width);
  g_free (key_height);
  g_free (key_maximized);
}

static void
load_paned_position (GKeyFile *config,
                     GtkWidget *pane,
                     const gchar *name,
                     gint default_position)
{
  gtk_paned_set_position (GTK_PANED (pane),
                          key_file_get_int (config, name, "position",
                                            default_position));
}

static gboolean
fix_paned_positions_idle (GladeWindow *window)
{
  /* When initially maximized/fullscreened we need to deffer this operation 
   */
  GKeyFile *config = glade_app_get_config ();

  load_paned_position (config, window->priv->left_paned, "left_pane", 200);
  load_paned_position (config, window->priv->center_paned, "center_pane", 400);

  return FALSE;
}

static void
glade_window_set_initial_size (GladeWindow *window, GKeyFile *config)
{
  GdkRectangle position = {
    G_MININT, G_MININT, GLADE_WINDOW_DEFAULT_WIDTH, GLADE_WINDOW_DEFAULT_HEIGHT
  };

  gboolean maximized;

  key_file_get_window_position (config, "main", &position, &maximized);
  if (maximized)
    {
      gtk_window_maximize (GTK_WINDOW (window));
      g_timeout_add (200, (GSourceFunc) fix_paned_positions_idle, window);
    }

  if (position.width <= 0 || position.height <= 0)
    {
      position.width = GLADE_WINDOW_DEFAULT_WIDTH;
      position.height = GLADE_WINDOW_DEFAULT_HEIGHT;
    }

  gtk_window_set_default_size (GTK_WINDOW (window), position.width,
                               position.height);

  if (position.x > G_MININT && position.y > G_MININT)
    gtk_window_move (GTK_WINDOW (window), position.x, position.y);
}

static void
glade_window_config_load (GladeWindow *window)
{
  GKeyFile *config = glade_app_get_config ();

  /* Initial main dimensions */
  glade_window_set_initial_size (window, config);

  /* Paned positions */
  load_paned_position (config, window->priv->left_paned, "left_pane", 200);
  load_paned_position (config, window->priv->center_paned, "center_pane", 400);

  /* Intro button */
  if (g_key_file_get_boolean (config, CONFIG_INTRO_GROUP, CONFIG_INTRO_DONE, FALSE))
    gtk_widget_hide (window->priv->intro_button);
}

static void
on_quit_action_activate (GSimpleAction *action,
                         GVariant      *parameter,
                         gpointer       data)
{
  GladeWindow *window = data;
  GList *list, *projects;

  projects = g_list_copy (glade_app_get_projects ());

  for (list = projects; list; list = list->next)
    {
      GladeProject *project = GLADE_PROJECT (list->data);

      if (glade_project_get_modified (project))
        {
          gboolean quit = confirm_close_project (window, project);
          if (!quit)
            {
              g_list_free (projects);
              return;
            }
        }
    }

  for (list = projects; list; list = list->next)
    {
      GladeProject *project = GLADE_PROJECT (glade_app_get_projects ()->data);
      close_project (window, project);
    }

  glade_window_config_save (window);

  g_list_free (projects);

  g_application_quit (G_APPLICATION (window->priv->application));
}

static void
on_pointer_mode_action_activate (GSimpleAction *simple,
                                 GVariant      *parameter,
                                 gpointer       data)
{
  GladePointerMode mode;

  mode = glade_utils_enum_value_from_string (GLADE_TYPE_POINTER_MODE,
                                             g_variant_get_string (parameter,
                                                                   NULL));
  glade_project_set_pointer_mode (get_active_project (data), mode);
}

static void
on_intro_action_activate (GSimpleAction *action, GVariant *p, gpointer data)
{
  GladeWindow *window = data;
  gtk_widget_show (window->priv->intro_button);
  glade_intro_play (window->priv->intro);
}

static void
glade_window_init (GladeWindow *window)
{
  GladeWindowPrivate *priv;
  GtkStyleContext *ctx;

  window->priv = priv = glade_window_get_instance_private (window);

  priv->default_path = NULL;

  /* This will load extra catalog paths */
  priv->settings = glade_settings_new ();
  glade_settings_load (priv->settings, glade_app_get_config ());

  /* Create GladeApp singleton, this will load all catalogs and load icons */
  priv->app = glade_app_new ();

  gtk_widget_init_template (GTK_WIDGET (window));

  gtk_box_set_homogeneous (GTK_BOX (priv->open_button_box), FALSE);
  gtk_box_set_homogeneous (GTK_BOX (priv->save_button_box), FALSE);

  priv->registration = glade_registration_new ();

  /* Add Gdk backend as a class */
  ctx = gtk_widget_get_style_context (GTK_WIDGET (window));
  gtk_style_context_add_class (ctx, glade_window_get_gdk_backend ());
}

static void
glade_window_action_handler (GladeWindow *window, const gchar *name)
{
  GAction *action;

  if ((action = GLADE_WINDOW_GET_ACTION (window, name)))
    g_action_activate (action, NULL);
}

static void
switch_foreach (GtkWidget *widget, gpointer data)
{
  GtkWidget *parent = gtk_widget_get_parent (widget);
  gint pos;

  gtk_container_child_get (GTK_CONTAINER(parent), widget, "position", &pos, NULL);

  if (pos == GPOINTER_TO_INT (data))
    gtk_stack_set_visible_child (GTK_STACK (parent), widget);
}

static void
glade_window_switch_handler (GladeWindow *window, gint index)
{
  gtk_container_foreach (GTK_CONTAINER (window->priv->view_stack),
                         switch_foreach, GINT_TO_POINTER (index));
}

static gboolean
intro_continue (gpointer intro)
{
  glade_intro_play (intro);
  return G_SOURCE_REMOVE;
}

static void
on_intro_project_add_widget (GladeProject *project,
                             GladeWidget  *widget,
                             GladeWindow  *window)
{
  GladeWidgetAdaptor *adaptor = glade_widget_get_adaptor (widget);

  if (glade_widget_adaptor_get_object_type (adaptor) == window->priv->new_type)
    {
      g_idle_add (intro_continue, window->priv->intro);

      if (window->priv->new_type == GTK_TYPE_BUTTON)
        g_signal_handlers_disconnect_by_func (project, on_intro_project_add_widget, window);
    }
}

static void
on_user_new_action_activate (GSimpleAction *simple,
                             GVariant      *parameter,
                             GladeWindow   *window)
{
  g_signal_connect (get_active_project (window), "add-widget",
                    G_CALLBACK (on_intro_project_add_widget),
                    window);

  glade_intro_play (window->priv->intro);

  g_signal_handlers_disconnect_by_func (simple, on_user_new_action_activate, window);
}

static void
on_intro_show_node (GladeIntro  *intro,
                    const gchar *node,
                    GtkWidget   *widget,
                    GladeWindow *window)
{
  GladeWindowPrivate *priv = window->priv;
  if (!g_strcmp0 (node, "new-project"))
    {
      /* Create two new project to make the project switcher visible */
      g_action_group_activate_action (G_ACTION_GROUP (priv->application), "new", NULL);
      g_action_group_activate_action (G_ACTION_GROUP (priv->application), "new", NULL);
    }
  else if (!g_strcmp0 (node, "add-project"))
    {
      GAction *new_action;

      if ((new_action = GLADE_WINDOW_GET_ACTION (window, "new")))
        g_signal_connect (new_action, "activate",
                          G_CALLBACK (on_user_new_action_activate),
                          window);
    }
  else if (!g_strcmp0 (node, "add-window"))
    {
      window->priv->new_type = GTK_TYPE_WINDOW;
    }
  else if (!g_strcmp0 (node, "add-grid"))
    {
      window->priv->new_type = GTK_TYPE_GRID;
    }
  else if (!g_strcmp0 (node, "add-button"))
    {
      window->priv->new_type = GTK_TYPE_BUTTON;
    }
  else if (!g_strcmp0 (node, "search") ||
           !g_strcmp0 (node, "others"))
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
    }
  else if (!g_strcmp0 (node, "gtk"))
    {
      GList *children;

      if ((children = gtk_container_get_children (GTK_CONTAINER (widget))))
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (children->data), TRUE);

      g_list_free (children);
    }
}

static void
on_intro_hide_node (GladeIntro  *intro,
                    const gchar *node,
                    GtkWidget   *widget,
                    GladeWindow *window)
{
  if (!g_strcmp0 (node, "search") ||
      !g_strcmp0 (node, "others"))
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), FALSE);
    }
  else if (!g_strcmp0 (node, "gtk"))
    {
      GList *children;

      if ((children = gtk_container_get_children (GTK_CONTAINER (widget))))
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (children->data), FALSE);

      g_list_free (children);
    }
  else if (!g_strcmp0 (node, "done"))
    {
      gtk_widget_hide (window->priv->intro_button);
      g_key_file_set_boolean (glade_app_get_config (),
                              CONFIG_INTRO_GROUP,
                              CONFIG_INTRO_DONE,
                              TRUE);
    }
  else if (!g_strcmp0 (node, "add-project") ||
           !g_strcmp0 (node, "add-window") ||
           !g_strcmp0 (node, "add-grid") ||
           !g_strcmp0 (node, "add-button"))
    glade_intro_pause (window->priv->intro);
}

#define ADD_NODE(n,w,P,d,t) glade_intro_script_add (window->priv->intro, n, w, t, GLADE_INTRO_##P, d)

static void
glade_window_populate_intro (GladeWindow *window)
{
  ADD_NODE (NULL, "intro-button",  BOTTOM, 5, _("Hello, I will show you what's new in Glade"));
  ADD_NODE (NULL, "headerbar",     BOTTOM, 6, _("The menubar and toolbar were merged in the headerbar"));

  ADD_NODE (NULL, "open-button",   BOTTOM, 3, _("You can open a project"));
  ADD_NODE (NULL, "recent-button", BOTTOM, 2, _("find recently used"));
  ADD_NODE (NULL, "new-button",    BOTTOM, 2, _("or create a new one"));

  ADD_NODE ("new-project", NULL,       NONE, .75, NULL);

  ADD_NODE (NULL, "undo-button",       BOTTOM, 2, _("Undo"));
  ADD_NODE (NULL, "redo-button",       BOTTOM, 2, _("Redo"));
  ADD_NODE (NULL, "project-switcher",  BOTTOM, 3, _("Project switcher"));

  ADD_NODE (NULL, "save-button",       BOTTOM, 4, _("and Save button are directly accessible in the headerbar"));
  ADD_NODE (NULL, "save-as-button",    BOTTOM, 2, _("just like Save As"));
  ADD_NODE (NULL, "properties-button", BOTTOM, 2, _("project properties"));
  ADD_NODE (NULL, "menu-button",       BOTTOM, 3, _("and less commonly used actions"));

  ADD_NODE (NULL, "inspector", CENTER, 3, _("The object inspector took the palette's place"));
  ADD_NODE (NULL, "editor",    CENTER, 3, _("To free up space for the property editor"));

  ADD_NODE (NULL,      "adaptor-chooser",       BOTTOM, 4, _("The palette was replaced with a new object chooser"));
  ADD_NODE ("search",  "adaptor-search-button", RIGHT,  3, _("Where you can search all supported classes"));
  ADD_NODE ("gtk",     "adaptor-gtk-buttonbox", BOTTOM, 2.5, _("investigate GTK+ object groups"));
  ADD_NODE ("others",  "adaptor-others-button", RIGHT,  4, _("and find classes introduced by other libraries"));

  ADD_NODE (NULL, "intro-button", BOTTOM, 6, _("OK, now that we are done with the overview, let's start with the new workflow"));

  ADD_NODE ("add-project", "intro-button", BOTTOM, 4, _("First of all, create a new project"));
  ADD_NODE ("add-window",  "intro-button", BOTTOM, 6, _("OK, now add a GtkWindow using the new widget chooser or by double clicking on the workspace"));
  ADD_NODE (NULL,          "intro-button", BOTTOM, 2, _("Excellent!"));
  ADD_NODE (NULL,          "intro-button", BOTTOM, 5, _("BTW, did you know you can double click on any placeholder to create widgets?"));
  ADD_NODE ("add-grid",    "intro-button", BOTTOM, 3, _("Try adding a grid"));
  ADD_NODE ("add-button",  "intro-button", BOTTOM, 3, _("and a button"));

  ADD_NODE (NULL,   "intro-button",  BOTTOM, 3, _("Quite easy! Isn't it?"));
  ADD_NODE ("done", "intro-button",  BOTTOM, 2, _("Enjoy!"));

  g_signal_connect (window->priv->intro, "show-node",
                    G_CALLBACK (on_intro_show_node),
                    window);
  g_signal_connect (window->priv->intro, "hide-node",
                    G_CALLBACK (on_intro_hide_node),
                    window);
}

static void
on_application_notify (GObject *gobject, GParamSpec *pspec)
{
  static GActionEntry actions[] = {
    { "open",         on_open_action_activate, NULL, NULL, NULL },
    { "new",          on_new_action_activate, NULL, NULL, NULL },
    { "registration", on_registration_action_activate, NULL, NULL, NULL },
    { "intro",        on_intro_action_activate, NULL, NULL, NULL },
    { "reference",    on_reference_action_activate, NULL, NULL, NULL },
    { "preferences",  on_preferences_action_activate, NULL, NULL, NULL },
    { "about",        on_about_action_activate, NULL, NULL, NULL },
    { "quit",         on_quit_action_activate, NULL, NULL, NULL },
    /* Project actions */
    { "close",        on_close_action_activate, NULL, NULL, NULL },
    { "save",         on_save_action_activate, NULL, NULL, NULL },
    { "save_as",      on_save_as_action_activate, NULL, NULL, NULL },
    { "properties",   on_properties_action_activate, NULL, NULL, NULL },
    { "undo",         on_undo_action_activate, NULL, NULL, NULL },
    { "redo",         on_redo_action_activate, NULL, NULL, NULL },
    { "cut",          on_cut_action_activate, NULL, NULL, NULL },
    { "copy",         on_copy_action_activate, NULL, NULL, NULL },
    { "paste",        on_paste_action_activate, NULL, NULL, NULL },
    { "delete",       on_delete_action_activate, NULL, NULL, NULL },
    { "previous",     on_previous_action_activate, NULL, NULL, NULL },
    { "next",         on_next_action_activate, NULL, NULL, NULL },
    { "pointer-mode", on_pointer_mode_action_activate, "s", NULL, NULL },
  };
  GladeWindowPrivate * priv = GLADE_WINDOW (gobject)->priv;

  priv->application = gtk_window_get_application (GTK_WINDOW (gobject));

  g_action_map_add_action_entries (G_ACTION_MAP (priv->application),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   gobject);
  gtk_widget_insert_action_group (GTK_WIDGET (gobject), "app",
                                  G_ACTION_GROUP (priv->application));

  project_actions_set_enabled (GLADE_WINDOW (gobject), FALSE);
}

static void
glade_window_constructed (GObject *object)
{
  GladeWindow *window = GLADE_WINDOW (object);
  GladeWindowPrivate *priv = window->priv;
  gchar *version;

  /* Chain up... */
  G_OBJECT_CLASS (glade_window_parent_class)->constructed (object);

  g_signal_connect (object, "notify::application", G_CALLBACK (on_application_notify), NULL);

  /* Init Glade version */
  version = g_strdup_printf ("%d.%d.%d", GLADE_MAJOR_VERSION, GLADE_MINOR_VERSION, GLADE_MICRO_VERSION);
  gtk_label_set_text (priv->version_label, version);
  g_free (version);

  /* recent files */
  priv->recent_manager = gtk_recent_manager_get_default ();

  /* status bar */
  priv->statusbar_context_id = gtk_statusbar_get_context_id (GTK_STATUSBAR (priv->statusbar), "general");
  priv->statusbar_menu_context_id = gtk_statusbar_get_context_id (GTK_STATUSBAR (priv->statusbar), "menu");
  priv->statusbar_actions_context_id = gtk_statusbar_get_context_id (GTK_STATUSBAR (priv->statusbar), "actions");

  /* support for opening a file by dragging onto the project window */
  gtk_drag_dest_set (GTK_WIDGET (window),
                     GTK_DEST_DEFAULT_ALL,
                     drop_types, G_N_ELEMENTS (drop_types),
                     GDK_ACTION_COPY | GDK_ACTION_MOVE);

  g_signal_connect (G_OBJECT (window), "drag-data-received",
                    G_CALLBACK (drag_data_received), window);

  g_signal_connect (G_OBJECT (window), "delete-event",
                    G_CALLBACK (delete_event), window);

  /* GtkWindow events */
  g_signal_connect (G_OBJECT (window), "key-press-event",
                    G_CALLBACK (glade_utils_hijack_key_press), window);

  glade_app_set_window (GTK_WIDGET (window));

  /* Load configuration */
  glade_window_config_load (window);

  /* Clipboard signals */
  g_signal_connect (G_OBJECT (glade_app_get_clipboard ()),
                    "notify::has-selection",
                    G_CALLBACK (clipboard_notify_handler_cb), window);

  priv->intro = glade_intro_new (GTK_WINDOW (window));
  glade_window_populate_intro (window);

  refresh_title (window);
}

#define DEFINE_ACTION_SIGNAL(klass, name, handler,...) \
  g_signal_new_class_handler (name, \
                              G_TYPE_FROM_CLASS (klass), \
                              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION, \
                              G_CALLBACK (handler), \
                              NULL, NULL, NULL, \
                              G_TYPE_NONE, __VA_ARGS__)

static void
glade_window_class_init (GladeWindowClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkCssProvider *provider;

  object_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = glade_window_constructed;
  object_class->dispose = glade_window_dispose;
  object_class->finalize = glade_window_finalize;

  widget_class->configure_event = glade_window_configure_event;

  DEFINE_ACTION_SIGNAL (klass, "glade-action", glade_window_action_handler, 1, G_TYPE_STRING);
  DEFINE_ACTION_SIGNAL (klass, "glade-switch", glade_window_switch_handler, 1, G_TYPE_INT);

  gtk_widget_class_set_css_name (widget_class, "GladeWindow");

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/glade/glade-window.css");
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/glade/glade.glade");

  /* Internal children */
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, adaptor_chooser);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, headerbar);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, project_switcher);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, about_dialog);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, start_page);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, version_label);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, intro_button);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, center_paned);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, left_paned);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, open_button_box);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, save_button_box);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, stack);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, view_stack);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, inspectors_stack);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, editor);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, statusbar);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, actions);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, undo_button);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWindow, redo_button);

  /* Callbacks */
  gtk_widget_class_bind_template_callback (widget_class, on_stack_visible_child_notify);
  gtk_widget_class_bind_template_callback (widget_class, on_open_recent_action_item_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_undo_button_button_press_event);
  gtk_widget_class_bind_template_callback (widget_class, on_redo_button_button_press_event);
}


GtkWidget *
glade_window_new (void)
{
  return g_object_new (GLADE_TYPE_WINDOW, NULL);
}

void
glade_window_check_devhelp (GladeWindow *window)
{
  g_return_if_fail (GLADE_IS_WINDOW (window));

  if (glade_util_have_devhelp ())
    g_signal_connect (glade_app_get (), "doc-search", G_CALLBACK (doc_search_cb), window);
}

void
glade_window_registration_notify_user (GladeWindow *window)
{
  gboolean skip_reminder, completed;
  GladeWindowPrivate *priv;

  g_return_if_fail (GLADE_IS_WINDOW (window));
  priv = window->priv;

  if (!g_tls_backend_supports_tls (g_tls_backend_get_default ()))
    {
      g_message ("No TLS support in GIO, Registration & User Survey disabled. (missing glib-networking package)");
      actions_set_enabled (window, "registration", FALSE);
      return;
    }

  g_object_get (priv->registration,
                "completed", &completed,
                "skip-reminder", &skip_reminder,
                NULL);

  if (!completed && !skip_reminder)
    {
      GtkWidget *dialog, *check;

      dialog = gtk_message_dialog_new (GTK_WINDOW (glade_app_get_window ()),
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_QUESTION,
                                       GTK_BUTTONS_YES_NO,
                                       "%s",
                                       /* translators: Primary message of a dialog used to notify the user about the survey */
                                       _("We are conducting a user survey\n would you like to take it now?"));

      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s",
                                                /* translators: Secondary text of a dialog used to notify the user about the survey */
                                                _("If not, you can always find it in the Help menu."));

      check = gtk_check_button_new_with_mnemonic (_("_Do not show this dialog again"));
      gtk_box_pack_end (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                        check, FALSE, FALSE, 0);
      gtk_widget_set_halign (check, GTK_ALIGN_START);
      gtk_widget_set_margin_start (check, 6);
      gtk_widget_show (check);

      if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES)
          gtk_window_present (GTK_WINDOW (priv->registration));

      if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)))
        {
          g_object_set (priv->registration, "skip-reminder", TRUE, NULL);
          glade_app_config_save ();
        }

      gtk_widget_destroy (dialog);
    }
  else if (!completed)
    glade_util_flash_message (priv->statusbar, priv->statusbar_context_id, "%s",
                              /* translators: Text to show in the statusbar if the user did not completed the survey and choose not to show the notification dialog again */
                              _("Go to Help -> Registration & User Survey and complete our survey!"));
}

#ifdef GDK_WINDOWING_X11
#include "gdk/gdkx.h"
#endif

#ifdef GDK_WINDOWING_QUARTZ
#include "gdk/gdkquartz.h"
#endif

#ifdef GDK_WINDOWING_WIN32
#include "gdk/gdkwin32.h"
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include "gdk/gdkwayland.h"
#endif

#ifdef GDK_WINDOWING_BROADWAY
#include "gdk/gdkbroadway.h"
#endif

const gchar *
glade_window_get_gdk_backend ()
{
  GdkDisplay *display = gdk_display_get_default ();

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (display))
    return "X11";
  else
#endif

#ifdef GDK_WINDOWING_QUARTZ
  if (GDK_IS_QUARTZ_DISPLAY (display))
    return "Quartz";
  else
#endif

#ifdef GDK_WINDOWING_WIN32
  if (GDK_IS_WIN32_DISPLAY (display))
    return "Win32";
  else
#endif

#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (display))
    return "Wayland";
  else
#endif

#ifdef GDK_WINDOWING_BROADWAY
  if (GDK_IS_BROADWAY_DISPLAY (display))
    return "Broadway";
  else
#endif
  {
    return "Unknown";
  }
}

