/*
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2007 Vincent Geddes.
 * Copyright (C) 2012 Juan Pablo Ugarte.
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
#include "glade-close-button.h"
#include "glade-resources.h"
#include "glade-callbacks.h"
#include "glade-preferences.h"

#include <gladeui/glade.h>
#include <gladeui/glade-popup.h>
#include <gladeui/glade-inspector.h>

#include <gladeui/glade-project.h>

#include <string.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#ifdef MAC_INTEGRATION
#  include <gtkosxapplication.h>
#endif


#define ACTION_GROUP_STATIC             "GladeStatic"
#define ACTION_GROUP_PROJECT            "GladeProject"
#define ACTION_GROUP_PROJECTS_LIST_MENU "GladeProjectsList"

#define READONLY_INDICATOR (_("[Read Only]"))

#define URL_DEVELOPER_MANUAL "http://library.gnome.org/devel/gladeui/"

#define CONFIG_GROUP_WINDOWS        "Glade Windows"
#define GLADE_WINDOW_DEFAULT_WIDTH  720
#define GLADE_WINDOW_DEFAULT_HEIGHT 540
#define CONFIG_KEY_X                "x"
#define CONFIG_KEY_Y                "y"
#define CONFIG_KEY_WIDTH            "width"
#define CONFIG_KEY_HEIGHT           "height"
#define CONFIG_KEY_DETACHED         "detached"
#define CONFIG_KEY_MAXIMIZED        "maximized"
#define CONFIG_KEY_SHOW_TOOLBAR     "show-toolbar"
#define CONFIG_KEY_SHOW_TABS        "show-tabs"
#define CONFIG_KEY_SHOW_STATUS      "show-statusbar"

#define GLADE_WINDOW_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object),  \
					  GLADE_TYPE_WINDOW,                      \
					  GladeWindowPrivate))

enum
{
  DOCK_PALETTE,
  DOCK_INSPECTOR,
  DOCK_EDITOR,
  N_DOCKS
};

typedef struct
{
  GtkWidget *widget;            /* the widget with scrollbars */
  GtkWidget *paned;             /* GtkPaned in the main window containing which part */
  gboolean first_child;         /* whether this widget is packed with gtk_paned_pack1() */
  gboolean detached;            /* whether this widget should be floating */
  gboolean maximized;           /* whether this widget should be maximized */
  char *title;                  /* window title, untranslated */
  char *id;                     /* id to use in config file */
  GdkRectangle window_pos;      /* x and y == G_MININT means unset */
} ToolDock;

struct _GladeWindowPrivate
{
  GladeApp *app;

  GtkWidget *main_vbox;

  GtkWidget *notebook;
  GladeDesignView *active_view;
  gint num_tabs;

  GtkWindow *about_dialog;
  GladePreferences *preferences;
  
  GtkWidget *palettes_notebook;         /* Cached per project palettes */
  GtkWidget *inspectors_notebook;       /* Cached per project inspectors */

  GladeEditor  *editor;                 /* The editor */

  GtkWidget *statusbar;                 /* A pointer to the status bar. */
  guint statusbar_menu_context_id;      /* The context id of the menu bar */
  guint statusbar_actions_context_id;   /* The context id of actions messages */

  GtkAccelGroup *accelgroup;
  
  struct
	{
	  GtkAction *save, *quit;
	  GtkAction *undo, *redo, *cut, *copy, *paste, *delete;
	  GtkAction *previous_project, *next_project;
	  GtkAction *use_small_icons, *icons_and_labels;
	  GtkAction *toolbar_visible, *project_tabs_visible, *statusbar_visible;
	  GtkAction *selector;
	} action;

  GtkActionGroup *project_actions;      /* All the project actions */
  GtkActionGroup *pointer_mode_actions;
  GtkActionGroup *projects_list_menu_actions;   /* Projects list menu actions */

  GtkMenuShell *project_menu;
  
  GtkRecentManager *recent_manager;
  GtkWidget *recent_menu;

  gchar *default_path;          /* the default path for open/save operations */

  GtkToolItem *undo;            /* customized buttons for undo/redo with history */
  GtkToolItem *redo;

  GtkWidget *toolbar;           /* Actions are added to the toolbar */
  gint actions_start;           /* start of action items */

  GtkWidget *center_pane;
  /* paned windows that tools get docked into/out of */
  GtkWidget *left_pane;
  GtkWidget *right_pane;

  GdkRectangle position;
  ToolDock docks[N_DOCKS];
};

static void check_reload_project (GladeWindow * window, GladeProject * project);

G_DEFINE_TYPE (GladeWindow, glade_window, GTK_TYPE_WINDOW)

/* the following functions are taken from gedit-utils.c */
static gchar *
str_middle_truncate (const gchar *string, guint truncate_length)
{
  GString *truncated;
  guint length;
  guint n_chars;
  guint num_left_chars;
  guint right_offset;
  guint delimiter_length;
  const gchar *delimiter = "\342\200\246";

  g_return_val_if_fail (string != NULL, NULL);

  length = strlen (string);

  g_return_val_if_fail (g_utf8_validate (string, length, NULL), NULL);

  /* It doesnt make sense to truncate strings to less than
   * the size of the delimiter plus 2 characters (one on each
   * side)
   */
  delimiter_length = g_utf8_strlen (delimiter, -1);
  if (truncate_length < (delimiter_length + 2))
    {
      return g_strdup (string);
    }

  n_chars = g_utf8_strlen (string, length);

  /* Make sure the string is not already small enough. */
  if (n_chars <= truncate_length)
    {
      return g_strdup (string);
    }

  /* Find the 'middle' where the truncation will occur. */
  num_left_chars = (truncate_length - delimiter_length) / 2;
  right_offset = n_chars - truncate_length + num_left_chars + delimiter_length;

  truncated = g_string_new_len (string,
                                g_utf8_offset_to_pointer (string,
                                                          num_left_chars) -
                                string);
  g_string_append (truncated, delimiter);
  g_string_append (truncated, g_utf8_offset_to_pointer (string, right_offset));

  return g_string_free (truncated, FALSE);
}

/*
 * Doubles underscore to avoid spurious menu accels - taken from gedit-utils.c
 */
static gchar *
escape_underscores (const gchar *text, gssize length)
{
  GString *str;
  const gchar *p;
  const gchar *end;

  g_return_val_if_fail (text != NULL, NULL);

  if (length < 0)
    length = strlen (text);

  str = g_string_sized_new (length);

  p = text;
  end = text + length;

  while (p != end)
    {
      const gchar *next;
      next = g_utf8_next_char (p);

      switch (*p)
        {
          case '_':
            g_string_append (str, "__");
            break;
          default:
            g_string_append_len (str, p, next - p);
            break;
        }

      p = next;
    }

  return g_string_free (str, FALSE);
}

typedef enum
{
  FORMAT_NAME_MARK_UNSAVED = 1 << 0,
  FORMAT_NAME_ESCAPE_UNDERSCORES = 1 << 1,
  FORMAT_NAME_MIDDLE_TRUNCATE = 1 << 2
} FormatNameFlags;

#define MAX_TITLE_LENGTH 100

static gchar *
get_formatted_project_name_for_display (GladeProject *project,
                                        FormatNameFlags format_flags)
{
  gchar *name, *pass1, *pass2, *pass3;

  g_return_val_if_fail (project != NULL, NULL);

  name = glade_project_get_name (project);

  if ((format_flags & FORMAT_NAME_MARK_UNSAVED)
      && glade_project_get_modified (project))
    pass1 = g_strdup_printf ("*%s", name);
  else
    pass1 = g_strdup (name);

  if (format_flags & FORMAT_NAME_ESCAPE_UNDERSCORES)
    pass2 = escape_underscores (pass1, -1);
  else
    pass2 = g_strdup (pass1);

  if (format_flags & FORMAT_NAME_MIDDLE_TRUNCATE)
    pass3 = str_middle_truncate (pass2, MAX_TITLE_LENGTH);
  else
    pass3 = g_strdup (pass2);

  g_free (name);
  g_free (pass1);
  g_free (pass2);

  return pass3;
}


static void
refresh_title (GladeWindow *window)
{
  GladeProject *project;
  gchar *title, *name = NULL;

  if (window->priv->active_view)
    {
      project = glade_design_view_get_project (window->priv->active_view);

      name = get_formatted_project_name_for_display (project,
                                                     FORMAT_NAME_MARK_UNSAVED |
                                                     FORMAT_NAME_MIDDLE_TRUNCATE);

      if (glade_project_get_readonly (project) != FALSE)
        title = g_strdup_printf ("%s %s", name, READONLY_INDICATOR);
      else
        title = g_strdup_printf ("%s", name);

      g_free (name);
    }
  else
    {
      title = g_strdup (_("User Interface Designer"));
    }

  gtk_window_set_title (GTK_WINDOW (window), title);

  g_free (title);
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
  GladeWidget   *widget;
  GWActionClass *aclass = glade_widget_action_get_class (action);

  if ((widget = g_object_get_data (G_OBJECT (toolbutton), "glade-widget")))
    glade_widget_adaptor_action_activate (glade_widget_get_adaptor (widget),
                                          glade_widget_get_object (widget), 
					  aclass->path);
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
  GtkContainer *container = GTK_CONTAINER (window->priv->toolbar);
  GtkToolbar *bar = GTK_TOOLBAR (window->priv->toolbar);
  GtkToolItem *item;

  if (window->priv->actions_start)
    {
      while ((item =
              gtk_toolbar_get_nth_item (bar, window->priv->actions_start)))
        gtk_container_remove (container, GTK_WIDGET (item));
    }
}

static void
add_actions (GladeWindow *window, GladeWidget *widget, GList *actions)
{
  GtkToolbar *bar = GTK_TOOLBAR (window->priv->toolbar);
  GtkToolItem *item = gtk_separator_tool_item_new ();
  gint n = 0;
  GList *l;

  gtk_toolbar_insert (bar, item, -1);
  gtk_widget_show (GTK_WIDGET (item));

  if (window->priv->actions_start == 0)
    window->priv->actions_start = gtk_toolbar_get_item_index (bar, item);

  for (l = actions; l; l = g_list_next (l))
    {
      GladeWidgetAction *action = l->data;
      GWActionClass     *aclass = glade_widget_action_get_class (action);

      if (!aclass->important || !glade_widget_action_get_visible (action))
        continue;

      if (glade_widget_action_get_children (action))
        {
          g_warning ("Trying to add a group action to the toolbar is unsupported");
          continue;
        }

      item = gtk_tool_button_new_from_stock ((aclass->stock) ? aclass->stock : "gtk-execute");
      if (aclass->label)
	{
	  gtk_tool_button_set_label (GTK_TOOL_BUTTON (item), aclass->label);
	  gtk_widget_set_tooltip_text (GTK_WIDGET (item), aclass->label);
	}

      g_object_set_data (G_OBJECT (item), "glade-widget", widget);

      /* We use destroy_data to keep track of notify::sensitive callbacks
       * on the action object and disconnect them when the toolbar item
       * gets destroyed.
       */
      g_signal_connect_data (item, "clicked",
                             G_CALLBACK (activate_action),
                             action, action_disconnect, 0);

      gtk_widget_set_sensitive (GTK_WIDGET (item), 
				glade_widget_action_get_sensitive (action));

      g_signal_connect (action, "notify::sensitive",
                        G_CALLBACK (activate_action), GTK_WIDGET (item));

      gtk_toolbar_insert (bar, item, -1);
      gtk_tool_item_set_homogeneous (item, FALSE);
      gtk_widget_show (GTK_WIDGET (item));
      n++;
    }

  if (n == 0)
    clean_actions (window);
}

static GladeProject *
get_active_project (GladeWindow *window)
{
  if (window->priv->active_view)
    return glade_design_view_get_project (window->priv->active_view);

  return NULL;
}

static void
project_selection_changed_cb (GladeProject *project, GladeWindow *window)
{
  GladeProject *active_project;
  GladeWidget  *glade_widget = NULL;
  GList        *list;
  gint          num;

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

          clean_actions (window);
          if (glade_widget_get_actions (glade_widget))
            add_actions (window, glade_widget, glade_widget_get_actions (glade_widget));
        }
    }

  glade_editor_load_widget (window->priv->editor, glade_widget);
}

static GladeDesignView *
get_active_view (GladeWindow *window)
{
  g_return_val_if_fail (GLADE_IS_WINDOW (window), NULL);

  return window->priv->active_view;
}

static gchar *
format_project_list_item_tooltip (GladeProject *project)
{
  gchar *tooltip, *path, *name;

  if (glade_project_get_path (project))
    {
      path =
          glade_utils_replace_home_dir_with_tilde (glade_project_get_path
                                                   (project));

      if (glade_project_get_readonly (project))
        {
          /* translators: referring to the action of activating a file named '%s'.
           *              we also indicate to users that the file may be read-only with
           *              the second '%s' */
          tooltip = g_strdup_printf (_("Activate '%s' %s"),
                                     path, READONLY_INDICATOR);
        }
      else
        {
          /* translators: referring to the action of activating a file named '%s' */
          tooltip = g_strdup_printf (_("Activate '%s'"), path);
        }
      g_free (path);
    }
  else
    {
      name = glade_project_get_name (project);
      /* FIXME add hint for translators */
      tooltip = g_strdup_printf (_("Activate '%s'"), name);
      g_free (name);
    }

  return tooltip;
}

static void
refresh_notebook_tab_for_project (GladeWindow *window, GladeProject *project)
{
  GtkWidget *tab_label, *label, *view, *eventbox;
  GList *children, *l;
  gchar *str;

  children =
      gtk_container_get_children (GTK_CONTAINER (window->priv->notebook));
  for (l = children; l; l = l->next)
    {
      view = l->data;

      if (project == glade_design_view_get_project (GLADE_DESIGN_VIEW (view)))
        {
          gchar *path, *deps;

          tab_label =
              gtk_notebook_get_tab_label (GTK_NOTEBOOK (window->priv->notebook),
                                          view);
          label = g_object_get_data (G_OBJECT (tab_label), "tab-label");
          eventbox = g_object_get_data (G_OBJECT (tab_label), "tab-event-box");

          str = get_formatted_project_name_for_display (project,
                                                        FORMAT_NAME_MARK_UNSAVED |
                                                        FORMAT_NAME_MIDDLE_TRUNCATE);
          gtk_label_set_text (GTK_LABEL (label), str);
          g_free (str);

          if (glade_project_get_path (project))
            path =
                glade_utils_replace_home_dir_with_tilde (glade_project_get_path
                                                         (project));
          else
            path = glade_project_get_name (project);


          deps = glade_project_display_dependencies (project);
          str = g_markup_printf_escaped (" <b>%s</b> %s \n"
                                         " %s \n"
                                         " <b>%s</b> %s ",
                                         _("Name:"), path,
                                         glade_project_get_readonly (project) ?
                                         READONLY_INDICATOR : "",
                                         _("Requires:"), deps);

          gtk_widget_set_tooltip_markup (eventbox, str);

          g_free (path);
          g_free (deps);
          g_free (str);

          break;
        }
    }
  g_list_free (children);
}

static void
project_targets_changed_cb (GladeProject *project, GladeWindow *window)
{
  refresh_notebook_tab_for_project (window, project);
}

static void
change_menu_label (GtkAction *action,
                   const gchar *action_label,
                   const gchar *action_description)
{
  gchar *text, *tmp_text;

  g_return_if_fail (action_label != NULL);

  if (action_description == NULL)
    text = g_strdup (action_label);
  else
    {
      tmp_text = escape_underscores (action_description, -1);
      text = g_strdup_printf ("%s: %s", action_label, tmp_text);
      g_free (tmp_text);
    }

  gtk_action_set_label (action, text);

  g_free (text);
}

static void
refresh_undo_redo (GladeWindow *window, GladeProject *project)
{
  GladeCommand *undo = NULL, *redo = NULL;
  GladeWindowPrivate *priv = window->priv;
  gchar        *tooltip;

  if (project != NULL)
    {
      undo = glade_project_next_undo_item (project);
      redo = glade_project_next_redo_item (project);
    }

  /* Refresh Undo */
  gtk_action_set_sensitive (priv->action.undo, undo != NULL);

  change_menu_label (priv->action.undo, _("_Undo"),
                     undo ? glade_command_description (undo) : NULL);

  tooltip = g_strdup_printf (_("Undo: %s"),
                             undo ? glade_command_description (undo) : _("the last action"));
  g_object_set (priv->action.undo, "tooltip", tooltip, NULL);
  g_free (tooltip);

  /* Refresh Redo */
  gtk_action_set_sensitive (priv->action.redo, redo != NULL);

  change_menu_label (priv->action.redo, _("_Redo"),
                     redo ? glade_command_description (redo) : NULL);

  tooltip = g_strdup_printf (_("Redo: %s"),
                             redo ? glade_command_description (redo) : _("the last action"));
  g_object_set (priv->action.redo, "tooltip", tooltip, NULL);
  g_free (tooltip);

  /* Refresh menus */
  gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (priv->undo),
                                 glade_project_undo_items (project));
  gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (priv->redo),
                                 glade_project_redo_items (project));
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
}

static void
refresh_projects_list_item (GladeWindow *window, GladeProject *project)
{
  GtkAction *action;
  gchar *project_name;
  gchar *tooltip;

  /* Get associated action */
  action =
      GTK_ACTION (g_object_get_data
                  (G_OBJECT (project), "project-list-action"));

  /* Set action label */
  project_name = get_formatted_project_name_for_display (project,
                                                         FORMAT_NAME_MARK_UNSAVED
                                                         |
                                                         FORMAT_NAME_ESCAPE_UNDERSCORES
                                                         |
                                                         FORMAT_NAME_MIDDLE_TRUNCATE);

  g_object_set (action, "label", project_name, NULL);

  /* Set action tooltip */
  tooltip = format_project_list_item_tooltip (project);
  g_object_set (action, "tooltip", tooltip, NULL);

  g_free (tooltip);
  g_free (project_name);
}

static void
refresh_next_prev_project_sensitivity (GladeWindow *window)
{
  GladeDesignView *view = get_active_view (window);
  GladeWindowPrivate *priv = window->priv;

  if (view != NULL)
    {
      gint view_number = gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook),
                                                GTK_WIDGET (view));
      g_return_if_fail (view_number >= 0);

      gtk_action_set_sensitive (priv->action.previous_project, view_number != 0);

      gtk_action_set_sensitive (priv->action.next_project,
                                view_number <
                                gtk_notebook_get_n_pages (GTK_NOTEBOOK
                                                          (priv->notebook)) - 1);
    }
  else
    {
      gtk_action_set_sensitive (priv->action.previous_project, FALSE);
      gtk_action_set_sensitive (priv->action.next_project, FALSE);
    }
}

static void
project_notify_handler_cb (GladeProject *project,
                           GParamSpec *spec,
                           GladeWindow *window)
{
  GladeProject *active_project = get_active_project (window);
  GladeWindowPrivate *priv = window->priv;

  if (strcmp (spec->name, "path") == 0)
    {
      refresh_title (window);
      refresh_notebook_tab_for_project (window, project);
    }
  else if (strcmp (spec->name, "format") == 0)
    {
      refresh_notebook_tab_for_project (window, project);
    }
  else if (strcmp (spec->name, "modified") == 0)
    {
      refresh_title (window);
      refresh_projects_list_item (window, project);
      refresh_notebook_tab_for_project (window, project);
    }
  else if (strcmp (spec->name, "read-only") == 0)
    {
      refresh_notebook_tab_for_project (window, project);

      gtk_action_set_sensitive (priv->action.save, !glade_project_get_readonly (project));
    }
  else if (strcmp (spec->name, "has-selection") == 0 && (project == active_project))
    {
      gtk_action_set_sensitive (priv->action.cut,
                                glade_project_get_has_selection (project));
      gtk_action_set_sensitive (priv->action.copy,
                                glade_project_get_has_selection (project));
      gtk_action_set_sensitive (priv->action.delete,
                                glade_project_get_has_selection (project));
    }
}

static void
clipboard_notify_handler_cb (GladeClipboard *clipboard,
                             GParamSpec *spec,
                             GladeWindow * window)
{
  if (strcmp (spec->name, "has-selection") == 0)
    {
      gtk_action_set_sensitive (window->priv->action.paste,
                                glade_clipboard_get_has_selection (clipboard));
    }
}

static void
on_pointer_mode_changed (GladeProject *project,
                         GParamSpec   *pspec,
                         GladeWindow  *window)
{
  GladeProject *active_project = get_active_project (window);
  GladeWindowPrivate *priv = window->priv;
  GladePointerMode mode;
  
  if (!active_project)
    {
      gtk_action_group_set_sensitive (priv->pointer_mode_actions, FALSE);
      return;
    }
  else if (active_project != project)
    return;

  mode = glade_project_get_pointer_mode (project);
  if (mode == GLADE_POINTER_ADD_WIDGET) return;

  gtk_action_group_set_sensitive (priv->pointer_mode_actions, TRUE);
  gtk_radio_action_set_current_value (GTK_RADIO_ACTION (priv->action.selector),
                                      mode);
}

static void
set_sensitivity_according_to_project (GladeWindow  *window,
                                      GladeProject *project)
{
  GladeWindowPrivate *priv = window->priv;

  gtk_action_set_sensitive (priv->action.save, !glade_project_get_readonly (project));

  gtk_action_set_sensitive (priv->action.cut, glade_project_get_has_selection (project));

  gtk_action_set_sensitive (priv->action.copy, glade_project_get_has_selection (project));

  gtk_action_set_sensitive (priv->action.paste,
                            glade_clipboard_get_has_selection
                            (glade_app_get_clipboard ()));

  gtk_action_set_sensitive (priv->action.delete, glade_project_get_has_selection (project));

  refresh_next_prev_project_sensitivity (window);
}

static void
recent_add (GladeWindow *window, const gchar *path)
{
  GtkRecentData *recent_data;
  gchar *uri;
  GError *error = NULL;

  uri = g_filename_to_uri (path, NULL, &error);
  if (error)
    {
      g_warning ("Could not convert local path \"%s\" to a uri: %s", path, error->message);
      g_error_free (error);
      return;
    }

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
  gchar *uri;
  GError *error = NULL;

  uri = g_filename_to_uri (path, NULL, &error);
  if (error)
    {
      g_warning ("Could not convert local path \"%s\" to a uri: %s", path, error->message);
      g_error_free (error);
      return;
    }

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
  guint i, n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook));

  /* increase project popularity */
  recent_add (window, glade_project_get_path (project));
  update_default_path (window, glade_project_get_path (project));

  for (i = 0; i < n; i++)
    {
      GladeProject *project_i;
      GtkWidget *view;

      view = gtk_notebook_get_nth_page (GTK_NOTEBOOK (priv->notebook), i);
      project_i = glade_design_view_get_project (GLADE_DESIGN_VIEW (view));

      if (project == project_i)
        {
          gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), i);
          break;
        }
    }
  check_reload_project (window, project);
}

static void
projects_list_menu_activate_cb (GtkAction *action, GladeWindow *window)
{
  if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) == FALSE)
    return;

  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->priv->notebook),
                                 gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action)));
}

static void
refresh_projects_list_menu (GladeWindow *window)
{
  GladeWindowPrivate *priv = window->priv;
  GList *actions, *l;
  GSList *group = NULL;
  gint n, i;

  /* Remove all current actions */
  actions = gtk_action_group_list_actions (priv->projects_list_menu_actions);
  for (l = actions; l != NULL; l = l->next)
    {
      GtkAction *action = l->data;
      GSList *p, *proxies = gtk_action_get_proxies (action);

      /* Remove MenuItems */
      for (p = proxies; p; p = g_slist_next (p))
        if (GTK_IS_MENU_ITEM (p->data)) gtk_widget_destroy (p->data);

      g_signal_handlers_disconnect_by_func (action,
                                            G_CALLBACK (projects_list_menu_activate_cb),
                                            window);
      gtk_accel_group_disconnect (priv->accelgroup,
                                  gtk_action_get_accel_closure (action));
      gtk_action_group_remove_action (priv->projects_list_menu_actions, action);
    }
  g_list_free (actions);

  n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook));

  /* Add an action for each project */
  for (i = 0; i < n; i++)
    {
      GtkWidget *view, *item;
      GladeProject *project;
      GtkRadioAction *action;
      gchar action_name[32];
      gchar *project_name;
      gchar *tooltip;
      gchar *accel;

      view = gtk_notebook_get_nth_page (GTK_NOTEBOOK (priv->notebook), i);
      project = glade_design_view_get_project (GLADE_DESIGN_VIEW (view));


      /* NOTE: the action is associated to the position of the tab in
       * the notebook not to the tab itself! This is needed to work
       * around the gtk+ bug #170727: gtk leaves around the accels
       * of the action. Since the accel depends on the tab position
       * the problem is worked around, action with the same name always
       * get the same accel.
       */
      g_snprintf (action_name, sizeof (action_name), "Tab_%d", i);
      project_name = get_formatted_project_name_for_display (project,
                                                             FORMAT_NAME_MARK_UNSAVED
                                                             |
                                                             FORMAT_NAME_MIDDLE_TRUNCATE
                                                             |
                                                             FORMAT_NAME_ESCAPE_UNDERSCORES);
      tooltip = format_project_list_item_tooltip (project);

      /* alt + 1, 2, 3... 0 to switch to the first ten tabs */
      accel = (i < 10) ? gtk_accelerator_name (GDK_KEY_0 + ((i + 1) % 10), GDK_MOD1_MASK) : NULL;

      action = gtk_radio_action_new (action_name,
                                     project_name, tooltip, NULL, i);

      /* Link action and project */
      g_object_set_data (G_OBJECT (project), "project-list-action", action);
      g_object_set_data (G_OBJECT (action), "project", project);

      /* note that group changes each time we add an action, so it must be updated */
      gtk_radio_action_set_group (action, group);
      group = gtk_radio_action_get_group (action);

      gtk_action_group_add_action_with_accel (priv->projects_list_menu_actions,
                                              GTK_ACTION (action), accel);
      gtk_accel_group_connect_by_path (priv->accelgroup,
                                       gtk_action_get_accel_path (GTK_ACTION (action)),
                                       gtk_action_get_accel_closure (GTK_ACTION (action)));

      /* Create Menu Item*/
      item = gtk_check_menu_item_new ();
      gtk_menu_shell_append (priv->project_menu, item);
      gtk_activatable_set_related_action (GTK_ACTIVATABLE (item), GTK_ACTION (action));
      gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (item), TRUE);
      gtk_widget_show (item);

      g_signal_connect (action, "activate",
                        G_CALLBACK (projects_list_menu_activate_cb), window);

      if (GLADE_DESIGN_VIEW (view) == priv->active_view)
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

      g_object_unref (action);

      g_free (project_name);
      g_free (tooltip);
      g_free (accel);
    }
}

void
on_open_action_activate (GtkAction *action, GladeWindow *window)
{
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

static void
save (GladeWindow *window, GladeProject *project, const gchar *path)
{
  GError *error = NULL;
  gchar *display_name, *display_path = g_strdup (path);
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
				gtk_image_new_from_stock (GTK_STOCK_SAVE,
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
	    {
	      g_free (display_path);
	      return;
	    }
	}
    }

  /* Interestingly; we cannot use `path' after glade_project_reset_path
   * because we are getting called with glade_project_get_path (project) as an argument.
   */
  if (!glade_project_save (project, path, &error))
    {
      /* Reset path so future saves will prompt the file chooser */
      glade_project_reset_path (project);

      if (error)
        {
          glade_util_ui_message (GTK_WIDGET (window), GLADE_UI_ERROR, NULL,
                                 _("Failed to save %s: %s"),
                                 display_path, error->message);
          g_error_free (error);
        }
      g_free (display_path);
      return;
    }

  /* Get display_name here, it could have changed with "Save As..." */
  display_name = glade_project_get_name (project);

  recent_add (window, glade_project_get_path (project));
  update_default_path (window, glade_project_get_path (project));

  /* refresh names */
  refresh_title (window);
  refresh_projects_list_item (window, project);
  refresh_notebook_tab_for_project (window, project);

  glade_util_flash_message (window->priv->statusbar,
                            window->priv->statusbar_actions_context_id,
                            _("Project '%s' saved"), display_name);

  g_free (display_path);
  g_free (display_name);
}

static void
save_as (GladeWindow *window)
{
  GladeProject *project, *another_project;
  GtkWidget *filechooser;
  GtkWidget *dialog;
  gchar *path = NULL;
  gchar *real_path, *ch, *project_name;

  project = glade_design_view_get_project (window->priv->active_view);

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

  if (gtk_dialog_run (GTK_DIALOG (filechooser)) == GTK_RESPONSE_OK)
    path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filechooser));

  gtk_widget_destroy (filechooser);

  if (!path)
    return;

  ch = strrchr (path, '.');
  if (!ch || strchr (ch, G_DIR_SEPARATOR))
    real_path = g_strconcat (path, ".glade", NULL);
  else
    real_path = g_strdup (path);

  g_free (path);

  /* checks if selected path is actually writable */
  if (glade_util_file_is_writeable (real_path) == FALSE)
    {
      dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                       GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_OK,
                                       _("Could not save the file %s"),
                                       real_path);

      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						_("You do not have the permissions "
						  "necessary to save the file."));

      gtk_window_set_title (GTK_WINDOW (dialog), "");

      g_signal_connect_swapped (dialog, "response",
                                G_CALLBACK (gtk_widget_destroy), dialog);

      gtk_widget_show (dialog);
      g_free (real_path);
      return;
    }

  /* checks if another open project is using selected path */
  if ((another_project = glade_app_get_project_by_path (real_path)) != NULL)
    {
      if (project != another_project)
        {

          glade_util_ui_message (GTK_WIDGET (window),
                                 GLADE_UI_ERROR, NULL,
                                 _
                                 ("Could not save file %s. Another project with that path is open."),
                                 real_path);

          g_free (real_path);
          return;
        }

    }

  save (window, project, real_path);

  g_free (real_path);
}

void
on_save_action_activate (GtkAction *action, GladeWindow *window)
{
  GladeProject *project;

  project = glade_design_view_get_project (window->priv->active_view);

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

void
on_save_as_action_activate (GtkAction *action, GladeWindow *window)
{
  save_as (window);
}

static gboolean
confirm_close_project (GladeWindow *window, GladeProject *project)
{
  GtkWidget *dialog;
  gboolean close = FALSE;
  gchar *msg, *project_name = NULL;
  gint ret;
  GError *error = NULL;

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
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                          GTK_STOCK_SAVE, GTK_RESPONSE_YES, NULL);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_YES,
                                           GTK_RESPONSE_CANCEL,
                                           GTK_RESPONSE_NO, -1);

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
            if ((close = glade_project_save
                 (project, glade_project_get_path (project), &error)) == FALSE)
              {

                glade_util_ui_message
                    (GTK_WIDGET (window), GLADE_UI_ERROR, NULL,
                     _("Failed to save %s to %s: %s"),
                     project_name, glade_project_get_path (project),
                     error->message);
                g_error_free (error);
              }
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
do_close (GladeWindow *window, GladeProject *project)
{
  GladeDesignView *view;
  gint n;

  view = glade_design_view_get_from_project (project);

  if (glade_project_is_loading (project))
    {
      glade_project_cancel_load (project);
      return;
    }

  n = gtk_notebook_page_num (GTK_NOTEBOOK (window->priv->notebook),
                             GTK_WIDGET (view));

  g_object_ref (view);

  gtk_notebook_remove_page (GTK_NOTEBOOK (window->priv->notebook), n);

  g_object_unref (view);
}

void
on_close_action_activate (GtkAction *action, GladeWindow *window)
{
  GladeDesignView *view;
  GladeProject *project;
  gboolean close;

  view = window->priv->active_view;

  project = glade_design_view_get_project (view);

  if (view == NULL)
    return;

  if (glade_project_get_modified (project))
    {
      close = confirm_close_project (window, project);
      if (!close)
        return;
    }
  do_close (window, project);
}

void
on_copy_action_activate (GtkAction *action, GladeWindow *window)
{
  GladeProject *project;

  if (!window->priv->active_view)
    return;

  project = glade_design_view_get_project (window->priv->active_view);

  glade_project_copy_selection (project);
}

void
on_cut_action_activate (GtkAction *action, GladeWindow *window)
{
  GladeProject *project;

  if (!window->priv->active_view)
    return;

  project = glade_design_view_get_project (window->priv->active_view);

  glade_project_command_cut (project);
}

void
on_paste_action_activate (GtkAction *action, GladeWindow *window)
{
  GtkWidget *placeholder;
  GladeProject *project;

  if (!window->priv->active_view)
    return;

  project = glade_design_view_get_project (window->priv->active_view);
  placeholder = glade_util_get_placeholder_from_pointer (GTK_CONTAINER (window));

  /* If this action is activated with a key binging (ctrl-v) the widget will be 
   * pasted over the placeholder below the default pointer.
   */
  glade_project_command_paste (project, placeholder ? GLADE_PLACEHOLDER (placeholder) : NULL);
}

void
on_delete_action_activate (GtkAction *action, GladeWindow *window)
{
  GladeProject *project;

  if (!window->priv->active_view)
    return;

  project = glade_design_view_get_project (window->priv->active_view);

  glade_project_command_delete (project);
}

void
on_properties_action_activate (GtkAction *action, GladeWindow *window)
{
  GladeProject *project;

  if (!window->priv->active_view)
    return;

  project = glade_design_view_get_project (window->priv->active_view);

  glade_project_properties (project);
}

void
on_undo_action_activate (GtkAction *action, GladeWindow *window)
{
  GladeProject *active_project = get_active_project (window);

  if (!active_project)
    {
      g_warning ("undo should not be sensitive: we don't have a project");
      return;
    }

  glade_project_undo (active_project);
}

void
on_redo_action_activate (GtkAction *action, GladeWindow *window)
{
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

void
on_notebook_switch_page (GtkNotebook *notebook,
                         GtkWidget *page,
                         guint page_num,
                         GladeWindow *window)
{
  GladeWindowPrivate *priv = window->priv;
  GladeDesignView *view;
  GladeProject *project;
  GtkAction *action;
  gchar *action_name;

  view = GLADE_DESIGN_VIEW (gtk_notebook_get_nth_page (notebook, page_num));

  /* CHECK: I don't know why but it seems notebook_switch_page is called
     two times every time the user change the active tab */
  if (view == priv->active_view)
    return;

  window->priv->active_view = view;

  project = glade_design_view_get_project (view);

  refresh_title (window);

  set_sensitivity_according_to_project (window, project);

  /* switch to the project's inspector/palette */
  gtk_notebook_set_current_page (GTK_NOTEBOOK
                                 (priv->inspectors_notebook), page_num);
  gtk_notebook_set_current_page (GTK_NOTEBOOK
                                 (priv->palettes_notebook), page_num);


  /* activate the corresponding item in the project menu */
  action_name = g_strdup_printf ("Tab_%d", page_num);
  action =
      gtk_action_group_get_action (priv->projects_list_menu_actions,
                                   action_name);

  /* sometimes the action doesn't exist yet, and the proper action
   * is set active during the documents list menu creation
   * CHECK: would it be nicer if active_view was a property and we monitored the notify signal?
   */
  if (action != NULL)
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

  g_free (action_name);

  refresh_undo_redo (window, project);

  /* Refresh the editor and some of the actions */
  project_selection_changed_cb (project, window);

  on_pointer_mode_changed (project, NULL, window);
}

static void
project_parse_finished_cb (GladeProject *project, GtkWidget *inspector)
{
  gtk_widget_set_sensitive (inspector, TRUE);

  glade_inspector_set_project (GLADE_INSPECTOR (inspector), project);
}

static void
set_widget_sensitive_on_load (GladeProject *project, GtkWidget *widget)
{
  gtk_widget_set_sensitive (widget, TRUE);
}

void
on_notebook_tab_added (GtkNotebook *notebook,
                       GladeDesignView *view,
                       guint page_num,
                       GladeWindow *window)
{
  GladeWindowPrivate *priv = window->priv;
  GladeProject *project;
  GtkWidget *inspector, *palette;

  ++window->priv->num_tabs;

  project = glade_design_view_get_project (view);

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
  gtk_widget_show (inspector);
  gtk_notebook_append_page (GTK_NOTEBOOK (window->priv->inspectors_notebook),
                            inspector, NULL);

  /* create palette */
  palette = glade_palette_new ();
  gtk_widget_show (palette);
  glade_palette_set_show_selector_button (GLADE_PALETTE (palette), FALSE);
  glade_palette_set_project (GLADE_PALETTE (palette), project);

  glade_palette_set_use_small_item_icons (GLADE_PALETTE (palette),
					  gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (priv->action.use_small_icons)));

  glade_palette_set_item_appearance (GLADE_PALETTE (palette),
				     gtk_radio_action_get_current_value (GTK_RADIO_ACTION (priv->action.icons_and_labels)));

  gtk_notebook_append_page (GTK_NOTEBOOK (window->priv->palettes_notebook),
                            palette, NULL);

  glade_design_view_set_drag_source (view, glade_palette_get_tool_palette (GLADE_PALETTE (palette)));
  
  if (GPOINTER_TO_INT
      (g_object_get_data (G_OBJECT (view), "view-added-while-loading")))
    {
      gtk_widget_set_sensitive (inspector, FALSE);
      gtk_widget_set_sensitive (palette, FALSE);
      g_signal_connect (project, "parse-finished",
                        G_CALLBACK (project_parse_finished_cb), inspector);
      g_signal_connect (project, "parse-finished",
                        G_CALLBACK (set_widget_sensitive_on_load), palette);
    }
  else
    glade_inspector_set_project (GLADE_INSPECTOR (inspector), project);

  set_sensitivity_according_to_project (window, project);

  refresh_projects_list_menu (window);

  refresh_title (window);

  if (window->priv->num_tabs > 0)
    gtk_action_group_set_sensitive (window->priv->project_actions, TRUE);

}

void
on_notebook_tab_removed (GtkNotebook     *notebook,
                         GladeDesignView *view,
                         guint            page_num, 
                         GladeWindow     *window)
{
  GladeProject *project;

  --window->priv->num_tabs;

  if (window->priv->num_tabs == 0)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (window->priv->editor), FALSE);
      window->priv->active_view = NULL;
    }

  project = glade_design_view_get_project (view);

  g_signal_handlers_disconnect_by_func (G_OBJECT (project),
                                        G_CALLBACK (project_notify_handler_cb),
                                        window);
  g_signal_handlers_disconnect_by_func (G_OBJECT (project),
                                        G_CALLBACK (project_selection_changed_cb), 
                                        window);
  g_signal_handlers_disconnect_by_func (G_OBJECT (project),
                                        G_CALLBACK (project_targets_changed_cb),
                                        window);
  g_signal_handlers_disconnect_by_func (G_OBJECT (project),
                                        G_CALLBACK (project_changed_cb),
                                        window);
  g_signal_handlers_disconnect_by_func (G_OBJECT (project),
                                        G_CALLBACK (on_pointer_mode_changed),
                                        window);

  gtk_notebook_remove_page (GTK_NOTEBOOK (window->priv->inspectors_notebook), page_num);
  gtk_notebook_remove_page (GTK_NOTEBOOK (window->priv->palettes_notebook), page_num);

  clean_actions (window);

  /* Refresh the editor and some of the actions */
  project_selection_changed_cb (project, window);

  on_pointer_mode_changed (project, NULL, window);

  /* FIXME: this function needs to be preferably called somewhere else */
  glade_app_remove_project (project);

  refresh_projects_list_menu (window);

  refresh_title (window);

  if (window->priv->active_view)
    set_sensitivity_according_to_project (window,
                                          glade_design_view_get_project
                                          (window->priv->active_view));
  else
    gtk_action_group_set_sensitive (window->priv->project_actions, FALSE);

}

void
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

void
on_palette_appearance_radioaction_changed (GtkRadioAction *action,
                                           GtkRadioAction *current,
                                           GladeWindow    *window)
{
  GList *children, *l;
  gint value;

  value = gtk_radio_action_get_current_value (current);

  children = gtk_container_get_children (GTK_CONTAINER (window->priv->palettes_notebook));
  for (l = children; l; l = l->next)
    {
      if (GLADE_IS_PALETTE (l->data))
		glade_palette_set_item_appearance (GLADE_PALETTE (l->data), value);
    }
  g_list_free (children);
}

void
on_use_small_icons_action_toggled (GtkAction *action, GladeWindow *window)
{
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (window->priv->palettes_notebook));
  for (l = children; l; l = l->next)
    {
      if (GLADE_IS_PALETTE (l->data))
	glade_palette_set_use_small_item_icons (GLADE_PALETTE (l->data),
						gtk_toggle_action_get_active
						(GTK_TOGGLE_ACTION (action)));
    }
  g_list_free (children);
}

static gboolean
on_dock_deleted (GtkWidget *widget, GdkEvent *event, GtkAction *dock_action)
{
  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (dock_action), TRUE);
  return TRUE;
}

static gboolean
on_dock_resized (GtkWidget *window, GdkEventConfigure *event, ToolDock *dock)
{
  GdkWindow *gdk_window =
      gtk_widget_get_window (GTK_WIDGET
                             (gtk_widget_get_toplevel (dock->widget)));
  dock->maximized =
      gdk_window_get_state (gdk_window) & GDK_WINDOW_STATE_MAXIMIZED;

  if (!dock->maximized)
    {
      dock->window_pos.width = event->width;
      dock->window_pos.height = event->height;

      gtk_window_get_position (GTK_WINDOW (window),
                               &dock->window_pos.x, &dock->window_pos.y);
    }

  return FALSE;
}

void
on_dock_action_toggled (GtkAction *action, GladeWindow *window)
{
  GtkWidget *toplevel, *alignment;
  ToolDock *dock;
  guint dock_type;

  dock_type =
      GPOINTER_TO_UINT (g_object_get_data
                        (G_OBJECT (action), "glade-dock-type"));
  g_return_if_fail (dock_type < N_DOCKS);

  dock = &window->priv->docks[dock_type];

  if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
    {
      toplevel = gtk_widget_get_toplevel (dock->widget);

      g_object_ref (dock->widget);
      gtk_container_remove (GTK_CONTAINER
                            (gtk_bin_get_child (GTK_BIN (toplevel))),
                            dock->widget);

      if (dock->first_child)
        gtk_paned_pack1 (GTK_PANED (dock->paned), dock->widget, FALSE, FALSE);
      else
        gtk_paned_pack2 (GTK_PANED (dock->paned), dock->widget, FALSE, FALSE);
      g_object_unref (dock->widget);

      gtk_widget_show (dock->paned);
      dock->detached = FALSE;

      gtk_widget_destroy (toplevel);
    }
  else
    {
      toplevel = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      /* Add a little padding on top to match the bottom */
      alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
      gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 4, 0, 0, 0);
      gtk_container_add (GTK_CONTAINER (toplevel), alignment);
      gtk_widget_show (alignment);

      gtk_window_set_default_size (GTK_WINDOW (toplevel),
                                   dock->window_pos.width,
                                   dock->window_pos.height);

      if (dock->window_pos.x > G_MININT && dock->window_pos.y > G_MININT)
        gtk_window_move (GTK_WINDOW (toplevel),
                         dock->window_pos.x, dock->window_pos.y);

      gtk_window_set_title (GTK_WINDOW (toplevel), dock->title);
      g_object_ref (dock->widget);
      gtk_container_remove (GTK_CONTAINER (dock->paned), dock->widget);
      gtk_container_add (GTK_CONTAINER (alignment), dock->widget);
      g_object_unref (dock->widget);

      g_signal_connect (G_OBJECT (toplevel), "delete-event",
                        G_CALLBACK (on_dock_deleted), action);
      g_signal_connect (G_OBJECT (toplevel), "configure-event",
                        G_CALLBACK (on_dock_resized), dock);

      if (!gtk_paned_get_child1 (GTK_PANED (dock->paned)) &&
          !gtk_paned_get_child2 (GTK_PANED (dock->paned)))
        gtk_widget_hide (dock->paned);

      gtk_window_add_accel_group (GTK_WINDOW (toplevel), window->priv->accelgroup);

      g_signal_connect (G_OBJECT (toplevel), "key-press-event",
                        G_CALLBACK (glade_utils_hijack_key_press), window);

      dock->detached = TRUE;

      gtk_window_present (GTK_WINDOW (toplevel));
    }
}

void
on_toolbar_visible_action_toggled (GtkAction *action, GladeWindow *window)
{
  if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
    gtk_widget_show (window->priv->toolbar);
  else
    gtk_widget_hide (window->priv->toolbar);
}

void
on_statusbar_visible_action_toggled (GtkAction *action, GladeWindow *window)
{
  if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
    gtk_widget_show (window->priv->statusbar);
  else
    gtk_widget_hide (window->priv->statusbar);
}

void
on_project_tabs_visible_action_toggled (GtkAction *action, GladeWindow *window)
{
  if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->priv->notebook), TRUE);
  else
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (window->priv->notebook), FALSE);
}

void
on_reference_action_activate (GtkAction *action, GladeWindow *window)
{
  if (glade_util_have_devhelp ())
    {
      glade_util_search_devhelp ("gladeui", NULL, NULL);
      return;
    }

  /* fallback to displaying online developer manual */
  glade_util_url_show (URL_DEVELOPER_MANUAL);
}

void
on_about_action_activate (GtkAction *action, GladeWindow *window)
{
  GladeWindowPrivate *priv = GLADE_WINDOW_GET_PRIVATE (window);
  
  gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (priv->about_dialog), PACKAGE_VERSION);
  
  gtk_window_present (priv->about_dialog);
}

static void
menu_item_selected_cb (GtkWidget *item, GladeWindow *window)
{
  gchar *tooltip = gtk_widget_get_tooltip_text (item);

  if (tooltip)
    gtk_statusbar_push (GTK_STATUSBAR (window->priv->statusbar),
                        window->priv->statusbar_menu_context_id, tooltip);

  g_free (tooltip);
}

/* FIXME: GtkItem does not exist anymore? */
static void
menu_item_deselected_cb (gpointer item, GladeWindow * window)
{
  gtk_statusbar_pop (GTK_STATUSBAR (window->priv->statusbar),
                     window->priv->statusbar_menu_context_id);
}

static void
menu_item_connect (GtkWidget *item, GtkAction *action, GladeWindow *window)
{
  if (GTK_IS_MENU_ITEM (item))
    {
      if (action == NULL)
        action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (item));

      if (action)
        {
          gchar *tooltip;
          g_object_get (G_OBJECT (action), "tooltip", &tooltip, NULL);
          gtk_widget_set_tooltip_text (item, tooltip);
          /* yeah dont actually show tooltips, we use the to push them to the status bar */
          gtk_widget_set_has_tooltip (item, FALSE);
          g_free (tooltip);
          
        }

      g_signal_connect (item, "select",
                        G_CALLBACK (menu_item_selected_cb), window);
      g_signal_connect (item, "deselect",
                        G_CALLBACK (menu_item_deselected_cb), window);
    }
}

static void
menu_item_disconnect (GtkWidget *item, GladeWindow *window)
{
  if (GTK_IS_MENU_ITEM (item))
    {
      g_signal_handlers_disconnect_by_func
          (item, G_CALLBACK (menu_item_selected_cb), window);
      g_signal_handlers_disconnect_by_func
          (item, G_CALLBACK (menu_item_deselected_cb), window);
    }
}

void
on_actiongroup_connect_proxy (GtkActionGroup *action_group,
                              GtkAction *action,
                              GtkWidget *proxy,
                              GladeWindow *window)
{
  menu_item_connect (proxy, action, window);
}

void
on_actiongroup_disconnect_proxy (GtkActionGroup *action_group,
                                 GtkAction *action,
                                 GtkWidget *proxy,
                                 GladeWindow *window)
{
  menu_item_disconnect (proxy, window);
}

void
on_recent_menu_insert (GtkMenuShell *menu_shell,
                       GtkWidget    *child,
                       gint          position,
                       GladeWindow *window)
{
  menu_item_connect (child, NULL, window);
}

void
on_recent_menu_remove (GtkContainer *container,
                       GtkWidget *widget,
                       GladeWindow *window)
{
  menu_item_disconnect (widget, window);
}

static void
recent_menu_setup_callbacks (GtkWidget *menu, GladeWindow *window)
{
  GList *l, *list = gtk_container_get_children (GTK_CONTAINER (menu));

  for (l = list; l; l = g_list_next (l))
    menu_item_connect (l->data, NULL, window);

  g_list_free (list);
}

static void
action_group_setup_callbacks (GtkActionGroup *action_group,
                              GtkAccelGroup *accel_group,
                              GladeWindow *window)
{
  GList *l, *list = gtk_action_group_list_actions (action_group);

  for (l = list; l; l = g_list_next (l))
    {
      GtkAction *action = l->data;
      GSList *p, *proxies = gtk_action_get_proxies (action);
      gboolean is_recent = GTK_IS_RECENT_ACTION (action);

      /* Workaround for gtk+ bug #671786 */
      gtk_accel_group_connect_by_path (accel_group,
                                       gtk_action_get_accel_path (action),
                                       gtk_action_get_accel_closure (action));
      
      for (p = proxies; p; p = g_slist_next (p))
        {
          GtkWidget *submenu, *proxy = p->data;

          gtk_activatable_sync_action_properties (GTK_ACTIVATABLE (proxy),
                                                  action);

          menu_item_connect (proxy, action, window);

          if (is_recent && GTK_IS_MENU_ITEM (proxy) &&
              (submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (proxy))))
            recent_menu_setup_callbacks (submenu, window);
        }
    }

  g_list_free (list);
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
  gtk_action_activate (window->priv->action.quit);

  /* return TRUE to stop other handlers */
  return TRUE;
}

void
on_selector_radioaction_changed (GtkRadioAction *action,
                                 GtkRadioAction *current,
                                 GladeWindow *window) 
{
  glade_project_set_pointer_mode (get_active_project (window),
                                  gtk_radio_action_get_current_value (current));
}

static void
tab_close_button_clicked_cb (GtkWidget *close_button, GladeProject *project)
{
  GladeWindow *window = GLADE_WINDOW (glade_app_get_window ());
  gboolean close;

  if (glade_project_get_modified (project))
    {
      close = confirm_close_project (window, project);
      if (!close)
        return;
    }
  do_close (window, project);
}

static void
project_load_progress_cb (GladeProject *project,
                          gint total, gint step,
                          GtkProgressBar *progress)
{
  gint fraction = (step * 1.0 / total) * 100;
  gchar *str, *name;

  if (fraction/100.0 == gtk_progress_bar_get_fraction (progress))
    return;
  
  name = glade_project_get_name (project);
  str = g_strdup_printf ("%s (%d%%)", name, fraction);

  gtk_progress_bar_set_text (progress, str);
  g_free (name);
  g_free (str);

  gtk_progress_bar_set_fraction (progress, fraction/100.0);

  while (gtk_events_pending ()) gtk_main_iteration ();
}

static void
project_load_finished_cb (GladeProject *project, GtkWidget *tab_label)
{
  GtkWidget *progress, *label;

  progress = g_object_get_data (G_OBJECT (tab_label), "tab-progress");
  label = g_object_get_data (G_OBJECT (tab_label), "tab-label");

  gtk_widget_hide (progress);
  gtk_widget_show (label);
}

static GtkWidget *
create_notebook_tab (GladeWindow *window,
                     GladeProject *project,
                     gboolean for_file)
{
  GtkWidget *tab_label, *ebox, *hbox, *close_button, *label, *dummy_label;
  GtkWidget *progress;

  tab_label = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);

  ebox = gtk_event_box_new ();
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (ebox), FALSE);
  gtk_box_pack_start (GTK_BOX (tab_label), ebox, TRUE, TRUE, 0);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_container_add (GTK_CONTAINER (ebox), hbox);

  close_button = glade_close_button_new ();
  gtk_widget_set_tooltip_text (close_button, _("Close document"));
  gtk_box_pack_start (GTK_BOX (tab_label), close_button, FALSE, FALSE, 0);

  g_signal_connect (close_button,
                    "clicked",
                    G_CALLBACK (tab_close_button_clicked_cb), project);

  label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 0, 0);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  progress = gtk_progress_bar_new ();
  gtk_progress_bar_set_show_text (GTK_PROGRESS_BAR (progress), TRUE);
  gtk_widget_add_events (progress,
                         GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
  gtk_widget_set_name (progress, "glade-tab-label-progress");
  gtk_box_pack_start (GTK_BOX (hbox), progress, FALSE, FALSE, 0);
  g_signal_connect (project, "load-progress",
                    G_CALLBACK (project_load_progress_cb), progress);

  dummy_label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), dummy_label, TRUE, TRUE, 0);

  gtk_widget_show (ebox);
  gtk_widget_show (hbox);
  gtk_widget_show (close_button);
  gtk_widget_show (dummy_label);

  if (for_file)
    {
      gtk_widget_show (progress);

      g_signal_connect (project, "parse-finished",
                        G_CALLBACK (project_load_finished_cb), tab_label);
    }
  else
    gtk_widget_show (label);

  g_object_set_data (G_OBJECT (tab_label), "tab-progress", progress);
  g_object_set_data (G_OBJECT (tab_label), "tab-event-box", ebox);
  g_object_set_data (G_OBJECT (tab_label), "tab-label", label);

  return tab_label;
}

static void
add_project (GladeWindow *window, GladeProject *project, gboolean for_file)
{
  GtkWidget *view, *label;

  g_return_if_fail (GLADE_IS_PROJECT (project));

  view = glade_design_view_new (project);
  gtk_widget_show (view);

  g_object_set_data (G_OBJECT (view), "view-added-while-loading",
                     GINT_TO_POINTER (for_file));

  /* Pass ownership of the project to the app */
  glade_app_add_project (project);
  g_object_unref (project);

  /* Custom notebook tab label (will be refreshed later) */
  label = create_notebook_tab (window, project, for_file);

  gtk_notebook_append_page (GTK_NOTEBOOK (window->priv->notebook),
                            GTK_WIDGET (view), label);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (window->priv->notebook), -1);

  refresh_notebook_tab_for_project (window, project);

  gtk_widget_set_sensitive (GTK_WIDGET (window->priv->editor), TRUE);
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

static gboolean
open_project (GladeWindow *window, const gchar *path)
{
  GladeProject *project;

  project = glade_project_new ();

  add_project (window, project, TRUE);
  update_default_path (window, path);

  if (!glade_project_load_from_file (project, path))
    {
      do_close (window, project);

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
                        gtk_image_new_from_stock (GTK_STOCK_REFRESH,
                                                  GTK_ICON_SIZE_BUTTON));
  gtk_widget_show (button);

  gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL,
                         GTK_RESPONSE_REJECT);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button,
                                GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_ACCEPT,
                                           GTK_RESPONSE_REJECT, -1);


  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_REJECT);

  response = gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);

  if (response == GTK_RESPONSE_REJECT)
    {
      return;
    }

  /* Reopen */
  path = g_strdup (glade_project_get_path (project));

  do_close (window, project);
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

  if (window->priv->app)
    {
      g_object_unref (window->priv->app);
      window->priv->app = NULL;
    }

  G_OBJECT_CLASS (glade_window_parent_class)->dispose (object);
}

static void
glade_window_finalize (GObject *object)
{
  guint i;

  g_free (GLADE_WINDOW (object)->priv->default_path);

  for (i = 0; i < N_DOCKS; i++)
    {
      ToolDock *dock = &GLADE_WINDOW (object)->priv->docks[i];
      g_free (dock->title);
      g_free (dock->id);
    }

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
                              gboolean detached,
                              gboolean save_detached,
                              gboolean maximized)
{
  char *key_x, *key_y, *key_width, *key_height, *key_detached, *key_maximized;

  key_x = g_strdup_printf ("%s-" CONFIG_KEY_X, id);
  key_y = g_strdup_printf ("%s-" CONFIG_KEY_Y, id);
  key_width = g_strdup_printf ("%s-" CONFIG_KEY_WIDTH, id);
  key_height = g_strdup_printf ("%s-" CONFIG_KEY_HEIGHT, id);
  key_detached = g_strdup_printf ("%s-" CONFIG_KEY_DETACHED, id);
  key_maximized = g_strdup_printf ("%s-" CONFIG_KEY_MAXIMIZED, id);

  /* we do not want to save position of docks which
   * were never detached */
  if (position->x > G_MININT)
    g_key_file_set_integer (config, CONFIG_GROUP_WINDOWS, key_x, position->x);
  if (position->y > G_MININT)
    g_key_file_set_integer (config, CONFIG_GROUP_WINDOWS, key_y, position->y);

  g_key_file_set_integer (config, CONFIG_GROUP_WINDOWS,
                          key_width, position->width);
  g_key_file_set_integer (config, CONFIG_GROUP_WINDOWS,
                          key_height, position->height);

  if (save_detached)
    g_key_file_set_boolean (config, CONFIG_GROUP_WINDOWS,
                            key_detached, detached);

  g_key_file_set_boolean (config, CONFIG_GROUP_WINDOWS,
                          key_maximized, maximized);


  g_free (key_maximized);
  g_free (key_detached);
  g_free (key_height);
  g_free (key_width);
  g_free (key_y);
  g_free (key_x);
}

static void
save_windows_config (GladeWindow *window, GKeyFile *config)
{
  guint i;
  GdkWindow *gdk_window;
  gboolean maximized;

  for (i = 0; i < N_DOCKS; ++i)
    {
      ToolDock *dock = &window->priv->docks[i];
      key_file_set_window_position (config, &dock->window_pos, dock->id,
                                    dock->detached, TRUE, dock->maximized);
    }

  gdk_window = gtk_widget_get_window (GTK_WIDGET (window));
  maximized = gdk_window_get_state (gdk_window) & GDK_WINDOW_STATE_MAXIMIZED;

  key_file_set_window_position (config, &window->priv->position,
                                "main", FALSE, FALSE, maximized);

  g_key_file_set_boolean (config,
                          CONFIG_GROUP_WINDOWS,
                          CONFIG_KEY_SHOW_TOOLBAR,
                          gtk_widget_get_visible (window->priv->toolbar));

  g_key_file_set_boolean (config,
                          CONFIG_GROUP_WINDOWS,
                          CONFIG_KEY_SHOW_STATUS,
                          gtk_widget_get_visible (window->priv->statusbar));

  g_key_file_set_boolean (config,
                          CONFIG_GROUP_WINDOWS,
                          CONFIG_KEY_SHOW_TABS,
                          gtk_notebook_get_show_tabs (GTK_NOTEBOOK
                                                      (window->priv->
                                                       notebook)));
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
  save_paned_position (config, window->priv->center_pane, "center_pane");
  save_paned_position (config, window->priv->left_pane, "left_pane");
  save_paned_position (config, window->priv->right_pane, "right_pane");

  glade_preferences_config_save (window->priv->preferences, config);

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
                              gboolean *detached,
                              gboolean *maximized)
{
  char *key_x, *key_y, *key_width, *key_height, *key_detached, *key_maximized;

  key_x = g_strdup_printf ("%s-" CONFIG_KEY_X, id);
  key_y = g_strdup_printf ("%s-" CONFIG_KEY_Y, id);
  key_width = g_strdup_printf ("%s-" CONFIG_KEY_WIDTH, id);
  key_height = g_strdup_printf ("%s-" CONFIG_KEY_HEIGHT, id);
  key_detached = g_strdup_printf ("%s-" CONFIG_KEY_DETACHED, id);
  key_maximized = g_strdup_printf ("%s-" CONFIG_KEY_MAXIMIZED, id);

  pos->x = key_file_get_int (config, CONFIG_GROUP_WINDOWS, key_x, pos->x);
  pos->y = key_file_get_int (config, CONFIG_GROUP_WINDOWS, key_y, pos->y);
  pos->width =
      key_file_get_int (config, CONFIG_GROUP_WINDOWS, key_width, pos->width);
  pos->height =
      key_file_get_int (config, CONFIG_GROUP_WINDOWS, key_height, pos->height);

  if (detached)
    {
      if (g_key_file_has_key (config, CONFIG_GROUP_WINDOWS, key_detached, NULL))
        *detached =
            g_key_file_get_boolean (config, CONFIG_GROUP_WINDOWS, key_detached,
                                    NULL);
      else
        *detached = FALSE;
    }

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
  g_free (key_detached);
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

  load_paned_position (config, window->priv->left_pane, "left_pane", 200);
  load_paned_position (config, window->priv->center_pane, "center_pane", 400);
  load_paned_position (config, window->priv->right_pane, "right_pane", 220);

  return FALSE;
}

static void
glade_window_set_initial_size (GladeWindow *window, GKeyFile *config)
{
  GdkRectangle position = {
    G_MININT, G_MININT, GLADE_WINDOW_DEFAULT_WIDTH, GLADE_WINDOW_DEFAULT_HEIGHT
  };

  gboolean maximized;

  key_file_get_window_position (config, "main", &position, NULL, &maximized);
  if (maximized)
    {
      gtk_window_maximize (GTK_WINDOW (window));
      g_idle_add ((GSourceFunc) fix_paned_positions_idle, window);
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
  gboolean show_toolbar, show_tabs, show_status;
  GladeWindowPrivate *priv = window->priv;
  GError *error = NULL;

  /* Initial main dimensions */
  glade_window_set_initial_size (window, config);

  /* toolbar and tabs */
  if ((show_toolbar =
       g_key_file_get_boolean (config, CONFIG_GROUP_WINDOWS,
                               CONFIG_KEY_SHOW_TOOLBAR, &error)) == FALSE &&
      error != NULL)
    {
      show_toolbar = TRUE;
      error = (g_error_free (error), NULL);
    }

  if ((show_tabs =
       g_key_file_get_boolean (config, CONFIG_GROUP_WINDOWS,
                               CONFIG_KEY_SHOW_TABS, &error)) == FALSE &&
      error != NULL)
    {
      show_tabs = TRUE;
      error = (g_error_free (error), NULL);
    }

  if ((show_status =
       g_key_file_get_boolean (config, CONFIG_GROUP_WINDOWS,
                               CONFIG_KEY_SHOW_STATUS, &error)) == FALSE &&
      error != NULL)
    {
      show_status = TRUE;
      error = (g_error_free (error), NULL);
    }

  if (show_toolbar)
    gtk_widget_show (priv->toolbar);
  else
    gtk_widget_hide (priv->toolbar);

  if (show_status)
    gtk_widget_show (priv->statusbar);
  else
    gtk_widget_hide (priv->statusbar);

  gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), show_tabs);

  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (priv->action.toolbar_visible), show_toolbar);

  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (priv->action.project_tabs_visible), show_tabs);

  gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (priv->action.statusbar_visible), show_tabs);

  /* Paned positions */
  load_paned_position (config, window->priv->left_pane, "left_pane", 200);
  load_paned_position (config, window->priv->center_pane, "center_pane", 400);
  load_paned_position (config, window->priv->right_pane, "right_pane", 220);

  glade_preferences_config_load (window->priv->preferences, config);
}

static gboolean
raise_window_idle (GtkWindow *window)
{
  gtk_window_present (window);
  return FALSE;
}

static void
show_dock_first_time (GladeWindow *window, guint dock_type, GtkAction *action)
{
  GKeyFile *config;
  int detached = -1;
  gboolean maximized;
  ToolDock *dock;

  g_object_set_data (G_OBJECT (action), "glade-dock-type",
                     GUINT_TO_POINTER (dock_type));

  dock = &window->priv->docks[dock_type];
  config = glade_app_get_config ();

  key_file_get_window_position (config, dock->id, &dock->window_pos, &detached,
                                &maximized);

  if (detached == 1)
    {
      gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);

      g_idle_add ((GSourceFunc)raise_window_idle, gtk_widget_get_toplevel (dock->widget));
    }

  if (maximized)
    gtk_window_maximize (GTK_WINDOW (gtk_widget_get_toplevel (dock->widget)));
}

static void
setup_dock (ToolDock *dock,
            GtkWidget *dock_widget,
            guint default_width,
            guint default_height,
            const char *window_title,
            const char *id,
            GtkWidget *paned,
            gboolean first_child)
{
  dock->widget = dock_widget;
  dock->window_pos.x = dock->window_pos.y = G_MININT;
  dock->window_pos.width = default_width;
  dock->window_pos.height = default_height;
  dock->title = g_strdup (window_title);
  dock->id = g_strdup (id);
  dock->paned = paned;
  dock->first_child = first_child;
  dock->detached = FALSE;
  dock->maximized = FALSE;
}

void
on_quit_action_activate (GtkAction *action, GladeWindow *window)
{
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
      do_close (window, project);
    }

  glade_window_config_save (window);

  g_list_free (projects);

  gtk_main_quit ();
}


static void
glade_window_init (GladeWindow *window)
{
  GladeWindowPrivate *priv;

  window->priv = priv = GLADE_WINDOW_GET_PRIVATE (window);

  priv->default_path = NULL;

  /* We need this for the icons to be available */
  glade_init ();
}

#define GET_OBJECT(b,c,o) c(gtk_builder_get_object(b,o)) /*;g_warn_if_fail(gtk_builder_get_object(b,o))*/

static void
glade_window_constructed (GObject *object)
{
  GladeWindowPrivate *priv = GLADE_WINDOW_GET_PRIVATE (object);
  GtkBuilder *builder;  
  GladeWindow *window;
  GError *error = NULL;
  GtkWidget *vbox;
  GtkActionGroup *group;

  /* Chain up... */
  G_OBJECT_CLASS (glade_window_parent_class)->constructed (object);
  
  /* Build UI */
  builder = gtk_builder_new ();
  if (gtk_builder_add_from_resource (builder, "/org/gnome/glade/glade.glade", &error) == 0)
    {
      g_warning ("gtk_builder_add_from_resource() failed %s", (error) ? error->message : "");
      return;
    }
  
  window = GLADE_WINDOW (object);

  priv->projects_list_menu_actions = GET_OBJECT (builder, GTK_ACTION_GROUP, "project_list_actiongroup");
  g_object_ref_sink (priv->projects_list_menu_actions);

  /* recent files */
  priv->recent_manager = gtk_recent_manager_get_default ();
    {
      GtkRecentFilter *filter = gtk_recent_filter_new ();
      gtk_recent_filter_add_application (filter, g_get_application_name ());
      gtk_recent_chooser_set_filter (GET_OBJECT (builder, GTK_RECENT_CHOOSER, "open_recent_action"),
                                     filter);
    }

  /* Init preferences */
  priv->preferences = glade_preferences_new (builder);
  
  /* Fetch pointers */
  vbox = GET_OBJECT (builder, GTK_WIDGET, "main_box");
  priv->about_dialog   = GET_OBJECT (builder, GTK_WINDOW, "about_dialog");

  priv->center_pane = GET_OBJECT (builder, GTK_WIDGET, "center_paned");
  priv->left_pane = GET_OBJECT (builder, GTK_WIDGET, "left_paned");
  priv->right_pane = GET_OBJECT (builder, GTK_WIDGET, "right_paned");

  priv->notebook = GET_OBJECT (builder, GTK_WIDGET, "notebook");
  priv->palettes_notebook = GET_OBJECT (builder, GTK_WIDGET, "palettes_notebook");
  priv->inspectors_notebook = GET_OBJECT (builder, GTK_WIDGET, "inspectors_notebook");
  priv->editor = GET_OBJECT (builder, GLADE_EDITOR, "editor");
  priv->statusbar = GET_OBJECT (builder, GTK_WIDGET, "statusbar");
  priv->toolbar = GET_OBJECT (builder, GTK_WIDGET, "toolbar");
  priv->project_menu = GET_OBJECT (builder, GTK_MENU_SHELL, "project_menu");

  priv->undo = GET_OBJECT (builder, GTK_TOOL_ITEM, "undo_toolbutton");
  priv->redo = GET_OBJECT (builder, GTK_TOOL_ITEM, "redo_toolbutton");

  priv->accelgroup = GTK_ACCEL_GROUP (gtk_builder_get_object (builder, "accelgroup"));
  gtk_window_add_accel_group (GTK_WINDOW (window), priv->accelgroup);

  /* Action groups */
  priv->project_actions = GET_OBJECT (builder, GTK_ACTION_GROUP, "project_actiongroup");
  action_group_setup_callbacks (priv->project_actions, priv->accelgroup, window);
  g_object_ref_sink (priv->project_actions);

  priv->pointer_mode_actions = GET_OBJECT (builder, GTK_ACTION_GROUP, "pointer_mode_actiongroup");
  action_group_setup_callbacks (priv->pointer_mode_actions, priv->accelgroup, window);
  g_object_ref_sink (priv->pointer_mode_actions);
  
  group = GET_OBJECT (builder, GTK_ACTION_GROUP, "static_actiongroup");
  action_group_setup_callbacks (group, priv->accelgroup, window);
  g_object_ref_sink (group);
  
  group = GET_OBJECT (builder, GTK_ACTION_GROUP, "view_actiongroup");
  action_group_setup_callbacks (group, priv->accelgroup, window);
  g_object_ref_sink (group);

  /* Actions */
  priv->action.save = GET_OBJECT (builder, GTK_ACTION, "save_action");
  priv->action.quit = GET_OBJECT (builder, GTK_ACTION, "quit_action");
  priv->action.undo = GET_OBJECT (builder, GTK_ACTION, "undo_action");
  priv->action.redo = GET_OBJECT (builder, GTK_ACTION, "redo_action");
  priv->action.cut = GET_OBJECT (builder, GTK_ACTION, "cut_action");
  priv->action.copy = GET_OBJECT (builder, GTK_ACTION, "copy_action");
  priv->action.paste = GET_OBJECT (builder, GTK_ACTION, "paste_action");
  priv->action.delete = GET_OBJECT (builder, GTK_ACTION, "delete_action");
  priv->action.previous_project = GET_OBJECT (builder, GTK_ACTION, "previous_project_action");
  priv->action.next_project = GET_OBJECT (builder, GTK_ACTION, "next_project_action");
  priv->action.use_small_icons = GET_OBJECT (builder, GTK_ACTION, "use_small_icons_action");
  priv->action.icons_and_labels = GET_OBJECT (builder, GTK_ACTION, "icons_and_labels_radioaction");
  priv->action.toolbar_visible = GET_OBJECT (builder, GTK_ACTION, "toolbar_visible_action");
  priv->action.project_tabs_visible = GET_OBJECT (builder, GTK_ACTION, "project_tabs_visible_action");
  priv->action.statusbar_visible = GET_OBJECT (builder, GTK_ACTION, "statusbar_visible_action");
  priv->action.selector = GET_OBJECT (builder, GTK_ACTION, "selector_radioaction");
  
  gtk_container_add (GTK_CONTAINER (window), vbox);
  
  /* Setup Docks */
  setup_dock (&priv->docks[DOCK_PALETTE], priv->palettes_notebook, 200, 540,
              _("Palette"), "palette", priv->left_pane, TRUE);
  setup_dock (&priv->docks[DOCK_INSPECTOR], priv->inspectors_notebook, 300, 540,
              _("Inspector"), "inspector", priv->right_pane, TRUE);
  setup_dock (&priv->docks[DOCK_EDITOR], GTK_WIDGET (priv->editor), 500, 700,
              _("Properties"), "properties", priv->right_pane, FALSE);
  show_dock_first_time (window, DOCK_PALETTE, GET_OBJECT (builder, GTK_ACTION, "dock_palette_action"));
  show_dock_first_time (window, DOCK_INSPECTOR, GET_OBJECT (builder, GTK_ACTION, "dock_inspector_action"));
  show_dock_first_time (window, DOCK_EDITOR, GET_OBJECT (builder, GTK_ACTION, "dock_editor_action"));

  /* status bar */
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

  gtk_builder_connect_signals (builder, window);

  /* Load configuration, we need the list of extra catalog paths before creating
   * the GladeApp
   */
  glade_window_config_load (window);
  
  /* Create GladeApp singleton, this will load all catalogs */
  priv->app = glade_app_new ();
  glade_app_set_window (GTK_WIDGET (window));

  /* Clipboard signals */
  g_signal_connect (G_OBJECT (glade_app_get_clipboard ()),
                    "notify::has-selection",
                    G_CALLBACK (clipboard_notify_handler_cb), window);
  
#ifdef MAC_INTEGRATION
	{
	  /* Fix up the menubar for MacOSX Quartz builds */
	  GtkWidget *menubar = GET_OBJECT (builder, GTK_WIDGET, "menubar");
	  GtkOSXApplication *theApp = g_object_new(GTK_TYPE_OSX_APPLICATION, NULL);
          GtkWidget *sep, *widget;

	  gtk_widget_hide (menubar);
	  gtk_osxapplication_set_menu_bar(theApp, GTK_MENU_SHELL(menubar));
	  widget = GET_OBJECT (builder, GTK_WIDGET, "quit_menuitem");
	  gtk_widget_hide (widget);
	  widget = GET_OBJECT (builder, GTK_WIDGET, "about_menuitem");
	  gtk_osxapplication_insert_app_menu_item (theApp, widget, 0);
	  sep = gtk_separator_menu_item_new();
	  g_object_ref(sep);
	  gtk_osxapplication_insert_app_menu_item (theApp, sep, 1);

	  widget = GET_OBJECT (builder, GTK_WIDGET, "properties_menuitem");
	  gtk_osxapplication_insert_app_menu_item  (theApp, widget, 2);
	  sep = gtk_separator_menu_item_new();
	  g_object_ref(sep);
	  gtk_osxapplication_insert_app_menu_item (theApp, sep, 3);

	  widget = GET_OBJECT (builder, GTK_WIDGET, "help_menuitem");
	  gtk_osxapplication_set_help_menu(theApp, GTK_MENU_ITEM(widget));

	  g_signal_connect(theApp, "NSApplicationWillTerminate",
	                   G_CALLBACK(on_quit_action_activate), window);

	  gtk_osxapplication_ready(theApp);
	}
#endif
  g_object_unref (builder);
}

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

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider,
                                   "GtkProgressBar#glade-tab-label-progress {\n"
                                   "   -GtkProgressBar-min-horizontal-bar-width : 1;\n"
                                   "   -GtkProgressBar-min-horizontal-bar-height : 1;\n"
                                   "   -GtkProgressBar-xspacing : 4;\n"
                                   "   -GtkProgressBar-yspacing : 0;\n"
                                   "   padding : 0;\n" " }", -1, NULL);

  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  g_type_class_add_private (klass, sizeof (GladeWindowPrivate));
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
    {
      glade_editor_show_info (window->priv->editor);

      g_signal_connect (glade_app_get (), "doc-search",
                        G_CALLBACK (doc_search_cb), window);

    }
}
