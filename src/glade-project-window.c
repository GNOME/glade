/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
 *   Paolo Borelli <pborelli@katamail.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkstock.h>

#include "glade.h"

#include "glade-project-window.h"

#define CONFIG_RECENT_PROJECTS     "Recent Projects"
#define CONFIG_RECENT_PROJECTS_MAX "max_recent_projects"

#define GLADE_ACTION_GROUP_STATIC "GladeStatic"
#define GLADE_ACTION_GROUP_PROJECT "GladeProject"
#define GLADE_ACTION_GROUP_RECENT "GladeRecent"
#define GLADE_ACTION_GROUP_PROJECTS_LIST_MENU "GladeProjectsList"

struct _GladeProjectWindowPriv {
	/* Application widgets */
	GtkWidget *window; /* Main window */
	GtkWidget *main_vbox;

	GtkWidget *statusbar; /* A pointer to the status bar. */
	guint statusbar_menu_context_id; /* The context id of the menu bar */
	guint statusbar_actions_context_id; /* The context id of actions messages */

	GtkUIManager *ui;		/* The UIManager */
	guint projects_list_menu_ui_id; /* Merge id for projects list menu */

	GtkActionGroup *static_actions;	/* All the static actions */
	GtkActionGroup *project_actions;/* All the project actions */
	GtkActionGroup *recent_actions;	/* Recent projects actions */
	GtkActionGroup *projects_list_menu_actions;/* Projects list menu actions */

	GQueue *recent_projects;        /* A GtkAction queue */
	gint    rp_max;                 /* Maximum Recent Projects entries */

	GtkWindow *palette_window;     /* The window that will contain the palette */
	GtkWindow *editor_window;      /* The window that will contain the editor */
	GtkWidget *expand;             /* Expand/Collapse the treeview */
	GtkWidget *collapse;

};


#define      READONLY_INDICATOR             (_("[read-only]"))

const gint   GLADE_WIDGET_TREE_WIDTH      = 230;
const gint   GLADE_WIDGET_TREE_HEIGHT     = 300;
const gint   GLADE_PALETTE_DEFAULT_HEIGHT = 450;

static gpointer parent_class = NULL;

static void gpw_refresh_undo_redo (GladeProjectWindow *gpw);

static void
gpw_refresh_title (GladeProjectWindow *gpw)
{
	GladeProject *active_project;
	gchar *title, *project_name;

	active_project = glade_app_get_project ();
	if (active_project)
	{
		project_name = glade_project_display_name (active_project, TRUE, FALSE, FALSE);
		
		if (active_project->readonly != FALSE)
			title = g_strdup_printf ("%s %s - %s", project_name,
						 READONLY_INDICATOR, g_get_application_name ());
		else
			title = g_strdup_printf ("%s - %s", project_name,
						 g_get_application_name ());
		
		g_free (project_name);
	}
	else
		title = g_strdup_printf ("%s", g_get_application_name ());

	gtk_window_set_title (GTK_WINDOW (gpw->priv->window), title);
	g_free (title);
}

static void
gpw_new_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	glade_project_window_new_project (gpw);
}

static void
gpw_recent_project_delete (GtkAction *action, GladeProjectWindow *gpw)
{
	guint merge_id = GPOINTER_TO_UINT(g_object_get_data (G_OBJECT (action), "merge_id"));
	
	gtk_ui_manager_remove_ui(gpw->priv->ui,	merge_id);
	gtk_action_group_remove_action (gpw->priv->recent_actions, action);
	g_queue_remove (gpw->priv->recent_projects, action);
	gtk_ui_manager_ensure_update (gpw->priv->ui);
}

static void
gpw_recent_project_open_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	gchar *path = (gchar *) g_object_get_data (G_OBJECT (action), "project_path");

	if (path == NULL) return;
	
	if (!glade_app_is_project_loaded (path))
		gpw_recent_project_delete (action, gpw);
	
	glade_project_window_open_project (gpw, path);
}

static gint gpw_rp_cmp (gconstpointer a, gconstpointer b)
{
	/* a is a GtkAction from the queue and b is a gchar* */
	return strcmp (g_object_get_data (G_OBJECT (a), "project_path"), b);
}

static void
gpw_recent_project_add (GladeProjectWindow *gpw, const gchar *project_path)
{
	GtkAction *action;
	gchar *label, *action_name;
	guint merge_id;

	/* Need to check if it's already loaded */
	if (g_queue_find_custom (gpw->priv->recent_projects,
				 project_path, gpw_rp_cmp))
		return;

	label = glade_util_duplicate_underscores (project_path);
	if (!label) return;
	
	action_name = g_strdup_printf ("open[%s]", project_path);
	/* We don't want '/'s in the menu path */
	glade_util_replace (action_name, '/', ' ');
	
	/* Add action */
	action = gtk_action_new (action_name, label, NULL, NULL);
	gtk_action_group_add_action_with_accel (gpw->priv->recent_actions, action, "");
	g_signal_connect (G_OBJECT (action), "activate", (GCallback)gpw_recent_project_open_cb, gpw);
	
	/* Add menuitem */
	merge_id = gtk_ui_manager_new_merge_id (gpw->priv->ui);
	gtk_ui_manager_add_ui (gpw->priv->ui, merge_id,
			      "/MenuBar/FileMenu/Recents", label, action_name,
			      GTK_UI_MANAGER_MENUITEM, TRUE);

	/* Set extra data to action */
	g_object_set_data(G_OBJECT (action), "merge_id", GUINT_TO_POINTER(merge_id));
	g_object_set_data_full (G_OBJECT (action), "project_path", g_strdup (project_path), g_free);

	/* Push action into recent project queue */
	g_queue_push_head (gpw->priv->recent_projects, action);
	
	/* If there is more entries than rp_max, delete the last one.*/
	if (g_queue_get_length(gpw->priv->recent_projects) > gpw->priv->rp_max)
	{
		GtkAction *last_action = (GtkAction *)g_queue_pop_tail(gpw->priv->recent_projects);
		gpw_recent_project_delete (last_action, gpw);
	}
	
	/* Set submenu sensitive */
	gtk_widget_set_sensitive (gtk_ui_manager_get_widget 
				  (gpw->priv->ui, "/MenuBar/FileMenu/Recents"),
				  TRUE);
	g_free (label);
	g_free (action_name);
}

static void
gpw_recent_project_config_load (GladeProjectWindow *gpw)
{
	gchar *filename, key[8];
	gint i;
	
	gpw->priv->rp_max = g_key_file_get_integer (
					glade_app_get_config (),
					CONFIG_RECENT_PROJECTS,
					CONFIG_RECENT_PROJECTS_MAX, NULL);
	
	/* Some default value for recent projects maximum */
	if (gpw->priv->rp_max == 0) gpw->priv->rp_max = 5;
	
	for (i = 0; i < gpw->priv->rp_max; i++)
	{
		g_snprintf(key, 8, "%d", i);
		
		filename = g_key_file_get_string (glade_app_get_config (),
						  CONFIG_RECENT_PROJECTS, key, NULL);
		if (filename)
		{
			if (g_file_test (filename, G_FILE_TEST_EXISTS))
				gpw_recent_project_add (gpw, filename);
		}
		else break;
	}
}

static void
gpw_recent_project_config_save (GladeProjectWindow *gpw)
{
	GKeyFile *config = glade_app_get_config ();
	GList *list;
	gchar key[8];
	gint i = 0;
	
	g_key_file_remove_group (config, CONFIG_RECENT_PROJECTS, NULL);
	
	g_key_file_set_integer (config,
				CONFIG_RECENT_PROJECTS,
				CONFIG_RECENT_PROJECTS_MAX,
				gpw->priv->rp_max);
	
	for (list = gpw->priv->recent_projects->tail; list; list = list->prev, i++)
	{
		gchar *path = g_object_get_data (G_OBJECT (list->data), "project_path");
		
		if( path )
		{
			g_snprintf (key, 8, "%d", i);
			g_key_file_set_string (config, CONFIG_RECENT_PROJECTS,
					       key, path);
		}
	}
}

static void
gpw_refresh_projects_list_item (GladeProjectWindow *gpw, GladeProject *project)
{
	GtkAction *action;
	gchar *project_name;
	gchar *tooltip;
	
	/* Get associated action */
	action = GTK_ACTION (g_object_get_data (G_OBJECT (project), "project-list-action"));

	/* Set action label */
	project_name = glade_project_display_name (project, TRUE, TRUE, TRUE);
	g_object_set (action, "label", project_name, NULL);

	/* Set action tooltip */
	if (project->readonly != FALSE)
		tooltip = g_strdup_printf ("%s %s", project->path, READONLY_INDICATOR);
	else
		tooltip = g_strdup_printf ("%s", project->path);
	
	g_object_set (action, "tooltip", tooltip, NULL);
	
	g_free (tooltip);
	g_free (project_name);
}

static void
gpw_project_notify_handler_cb (GladeProject *project, GParamSpec *spec, GladeProjectWindow *gpw)
{
	GtkAction *action;

	if (strcmp (spec->name, "has-unsaved-changes") == 0)	
	{
		gpw_refresh_title (gpw);
		gpw_refresh_projects_list_item (gpw, project);
	}
	else if (strcmp (spec->name, "read-only") == 0)	
	{
		action = gtk_action_group_get_action (gpw->priv->project_actions, "Save");
		gtk_action_set_sensitive (action,
				  	  !glade_project_get_readonly (project));
	}
	else if (strcmp (spec->name, "has-selection") == 0)	
	{
		action = gtk_action_group_get_action (gpw->priv->project_actions, "Cut");
		gtk_action_set_sensitive (action,
				  	  glade_project_get_has_selection (project));

		action = gtk_action_group_get_action (gpw->priv->project_actions, "Copy");
		gtk_action_set_sensitive (action,
				  	  glade_project_get_has_selection (project));

		action = gtk_action_group_get_action (gpw->priv->project_actions, "Delete");
		gtk_action_set_sensitive (action,
				 	  glade_project_get_has_selection (project));
	}
}

static void
gpw_clipboard_notify_handler_cb (GladeClipboard *clipboard, GParamSpec *spec, GladeProjectWindow *gpw)
{
	GtkAction *action;

	if (strcmp (spec->name, "has-selection") == 0)	
	{
		action = gtk_action_group_get_action (gpw->priv->project_actions, "Paste");
		gtk_action_set_sensitive (action,
				 	  glade_clipboard_get_has_selection (clipboard));
	}
}

static void
gpw_set_sensitivity_according_to_project (GladeProjectWindow *gpw, GladeProject *project)
{
	GtkAction *action;

	action = gtk_action_group_get_action (gpw->priv->project_actions, "Save");
	gtk_action_set_sensitive (action,
			          !glade_project_get_readonly (project));

	action = gtk_action_group_get_action (gpw->priv->project_actions, "Cut");
	gtk_action_set_sensitive (action,
				  glade_project_get_has_selection (project));

	action = gtk_action_group_get_action (gpw->priv->project_actions, "Copy");
	gtk_action_set_sensitive (action,
				  glade_project_get_has_selection (project));

	action = gtk_action_group_get_action (gpw->priv->project_actions, "Paste");
	gtk_action_set_sensitive (action,
				  glade_clipboard_get_has_selection
					(glade_app_get_clipboard ()));

	action = gtk_action_group_get_action (gpw->priv->project_actions, "Delete");
	gtk_action_set_sensitive (action,
				  glade_project_get_has_selection (project));
}

static void
gpw_projects_list_menu_activate_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	GladeProject *project;

	project = GLADE_PROJECT (g_object_get_data (G_OBJECT (action), "project"));
	g_assert (GLADE_IS_PROJECT (project));

	glade_app_set_project (project);

	gpw_set_sensitivity_according_to_project (gpw, project);

	gpw_refresh_title (gpw);
}

static void
gpw_refresh_projects_list_menu (GladeProjectWindow *gpw)
{
	GladeProjectWindowPriv *p = gpw->priv;
	GList *projects, *actions, *l;
	GSList *group = NULL;
	gint i = 0;
	guint id;

	if (p->projects_list_menu_ui_id != 0)
		gtk_ui_manager_remove_ui (p->ui, p->projects_list_menu_ui_id);

	/* Remove all current actions */
	actions = gtk_action_group_list_actions (p->projects_list_menu_actions);
	for (l = actions; l != NULL; l = l->next)
	{
		g_signal_handlers_disconnect_by_func 
			(GTK_ACTION (l->data),
			 G_CALLBACK (gpw_projects_list_menu_activate_cb), gpw);
 		gtk_action_group_remove_action (p->projects_list_menu_actions, GTK_ACTION (l->data));
	}
	g_list_free (actions);

	projects = glade_app_get_projects ();

	id = (projects != NULL) ? gtk_ui_manager_new_merge_id (p->ui) : 0;
	p->projects_list_menu_ui_id = id;

	/* Add an action for each project */
	for (l = projects; l != NULL; l = l->next, i++)
	{
		GladeProject *project;
		GtkRadioAction *action;
		gchar *action_name;
		gchar *project_name;
		gchar *tooltip;

		project = GLADE_PROJECT (l->data);

		action_name = g_strdup_printf ("Project_%d", i);
		project_name = glade_project_display_name (project, TRUE, TRUE, TRUE);
		if (project->readonly != FALSE)
			tooltip = g_strdup_printf ("%s %s",project->path, READONLY_INDICATOR);
		else
			tooltip = g_strdup (project->path);

		action = gtk_radio_action_new (action_name, project_name, tooltip, NULL, i);

		/* Link action and project */
		g_object_set_data (G_OBJECT (project), "project-list-action", action);
		g_object_set_data (G_OBJECT (action), "project", project);

		if (group != NULL)
			gtk_radio_action_set_group (action, group);

		group = gtk_radio_action_get_group (action);

		gtk_action_group_add_action (p->projects_list_menu_actions, GTK_ACTION (action));

		if (project == glade_app_get_project ())
			gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

		g_signal_connect (action, "activate",
				  G_CALLBACK (gpw_projects_list_menu_activate_cb),
				  gpw);

		gtk_ui_manager_add_ui (p->ui, id,
				       "/MenuBar/ProjectMenu/ProjectsListPlaceholder",
				       action_name, action_name,
				       GTK_UI_MANAGER_MENUITEM,
				       FALSE);

		g_object_unref (action);

		g_free (action_name);
		g_free (project_name);
		g_free (tooltip);
	}
}

static gchar *
gpw_get_recent_dir (GladeProjectWindow *gpw)
{
	const gchar *project_path;
	gchar       *dir = NULL;
	
	if (gpw->priv->recent_projects->head)
	{
		GtkAction *action = gpw->priv->recent_projects->head->data;

		project_path = g_object_get_data (G_OBJECT (action), "project_path");

		if (project_path)
			dir = g_path_get_dirname (project_path);
	}

	/* this is wrong on win32 */
	if (!dir)
		dir = g_strdup (g_get_home_dir ());

	return dir;
}

static void
gpw_open_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	GtkWidget           *filechooser;
	gchar               *path = NULL;
	gchar               *dir;

	filechooser = glade_util_file_dialog_new (_("Open..."), GTK_WINDOW (gpw->priv->window),
						   GLADE_FILE_DIALOG_ACTION_OPEN);

	if ((dir = gpw_get_recent_dir (gpw)) != NULL)
	{
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (filechooser), dir);
		g_free (dir);
	}

	if (gtk_dialog_run (GTK_DIALOG(filechooser)) == GTK_RESPONSE_OK)
		path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filechooser));

	gtk_widget_destroy (filechooser);

	if (!path)
		return;
	
	glade_project_window_open_project (gpw, path);
	g_free (path);
}

static void
gpw_recent_project_clear_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (gpw->priv->window),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 "<big><b>%s</b></big>\n\n%s",
					 _("Are you sure you want to clear the\nlist of recent projects?"),
					 _("If you clear the list of recent projects, they will be\npermanently deleted."));
	
	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_CLEAR, GTK_RESPONSE_OK,
				NULL);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
	{
		/* Remove all Recent Projects items */
		g_queue_foreach (gpw->priv->recent_projects, (GFunc)gpw_recent_project_delete, gpw);
		/* Set submenu insensitive */
		gtk_widget_set_sensitive (gtk_ui_manager_get_widget 
					  (gpw->priv->ui, "/MenuBar/FileMenu/Recents"),
					  FALSE);
		
		/* Save it */
		gpw_recent_project_config_save (gpw);
		glade_app_config_save ();
	}
	gtk_widget_destroy (dialog);
}

static void
gpw_save (GladeProjectWindow *gpw, GladeProject *project, const gchar *path)
{
	GError  *error = NULL;
	gchar   *display_name, *display_path = g_strdup (path);

	/* Interestingly; we cannot use `path' after glade_project_reset_path
	 * because we are getting called with project->path as an argument.
	 */
	if (!glade_project_save (project, path, &error))
	{
		/* Reset path so future saves will prompt the file chooser */
		glade_project_reset_path (project);
		glade_util_ui_message (gpw->priv->window, GLADE_UI_ERROR, 
				       _("Failed to save %s: %s"),
				       display_path, error->message);
		g_error_free (error);
		g_free (display_path);
		return;
	}
	
	glade_app_update_instance_count (project);

	/* Get display_name here, it could have changed with "Save As..." */
	display_name = glade_project_display_name (project, FALSE, FALSE, FALSE),

	gpw_recent_project_add (gpw, project->path);
	gpw_recent_project_config_save (gpw);
	glade_app_config_save ();

	/* refresh names */
	gpw_refresh_title (gpw);
	gpw_refresh_projects_list_item (gpw, project);

	glade_util_flash_message (gpw->priv->statusbar,
				  gpw->priv->statusbar_actions_context_id,
				  _("Project '%s' saved"), display_name);

	g_free (display_path);
	g_free (display_name);
}

static void
gpw_save_as (GladeProjectWindow *gpw, const gchar *dialog_title)
{
 	GladeProject *project, *another_project;
 	GtkWidget *filechooser;
	gchar *path = NULL;
	gchar *real_path, *ch;
	
	if ((project = glade_app_get_project ()) == NULL)
	{
		/* Just incase the menu-item is not insensitive */
		glade_util_ui_message (gpw->priv->window, GLADE_UI_WARN, 
				       _("No open projects to save"));
		return;
	}

	filechooser = glade_util_file_dialog_new (dialog_title,
						  GTK_WINDOW (gpw->priv->window),
						  GLADE_FILE_DIALOG_ACTION_SAVE);

	if (project->path)
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (filechooser), project->path);
	else
	{
		gchar *dir;
		if ((dir = gpw_get_recent_dir (gpw)) != NULL)
		{
			gtk_file_chooser_set_current_folder
				(GTK_FILE_CHOOSER (filechooser), dir);
			g_free (dir);
		}
		
		gtk_file_chooser_set_current_name
			(GTK_FILE_CHOOSER (filechooser), project->name);
	}
	
 	if (gtk_dialog_run (GTK_DIALOG(filechooser)) == GTK_RESPONSE_OK)
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

		glade_util_ui_message (gpw->priv->window, 
				       GLADE_UI_ERROR,
			     	       _("Could not save the file %s. You do not have the permissions necessary to save the file."), 
				       real_path);
		g_free (real_path);
		return;
	}

	/* checks if another open project is using selected path */
	if ((another_project = glade_app_get_project_by_path (real_path)) != NULL)
	{
		if (project != another_project) {

			glade_util_ui_message (gpw->priv->window, 
					       GLADE_UI_ERROR,
				     	       _("Could not save file %s. Another project with that path is open."), 
					       real_path);

			g_free (real_path);
			return;
		}

	}

	gpw_save (gpw, project, real_path);

	g_free (real_path);
}

static void
gpw_save_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	GladeProject       *project;

	if ((project = glade_app_get_project ()) == NULL)
	{
		/* Just incase the menu-item or button is not insensitive */
		glade_util_ui_message (gpw->priv->window, 
				       GLADE_UI_WARN,
				       _("No open projects to save"));
		return;
	}

	if (project->path != NULL) 
	{
		gpw_save (gpw, project, project->path);
 		return;
	}

	/* If instead we dont have a path yet, fire up a file selector */
	gpw_save_as (gpw, _("Save..."));
}

static void
gpw_save_as_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	gpw_save_as (gpw, _("Save as ..."));
}

static gboolean
gpw_confirm_close_project (GladeProjectWindow *gpw, GladeProject *project)
{
	GtkWidget *dialog;
	gboolean close = FALSE;
	char *msg;
	gint ret;
	GError *error = NULL;

	g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

	msg = g_strdup_printf (_("<span weight=\"bold\" size=\"larger\">Save changes to project \"%s\" before closing?</span>\n\n"
				 "Your changes will be lost if you don't save them.\n"), project->name);

	dialog = gtk_message_dialog_new (GTK_WINDOW (gpw->priv->window),
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 msg);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	g_free(msg);

	gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (dialog)->label), TRUE);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				_("_Close without Saving"), GTK_RESPONSE_NO,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_SAVE, GTK_RESPONSE_YES, NULL);

	gtk_dialog_set_default_response	(GTK_DIALOG (dialog), GTK_RESPONSE_YES);

	ret = gtk_dialog_run (GTK_DIALOG (dialog));
	switch (ret) {
	case GTK_RESPONSE_YES:
		/* if YES we save the project: note we cannot use gpw_save_cb
		 * since it saves the current project, while the modified 
                 * project we are saving may be not the current one.
		 */
		if (project->path != NULL)
		{
			if ((close = glade_project_save
			     (project, project->path, &error)) == FALSE)
			{

				glade_util_ui_message
					(gpw->priv->window, GLADE_UI_ERROR, 
					 _("Failed to save %s to %s: %s"),
					 project->name, project->path, error->message);
				g_error_free (error);
			}
		}
		else
		{
			GtkWidget *filechooser;
			gchar *path = NULL;
			gchar *dir;

			filechooser =
				glade_util_file_dialog_new (_("Save ..."),
							    GTK_WINDOW (gpw->priv->window),
							    GLADE_FILE_DIALOG_ACTION_SAVE);

			if ((dir = gpw_get_recent_dir (gpw)) != NULL)
			{
				gtk_file_chooser_set_current_folder
					(GTK_FILE_CHOOSER (filechooser), dir);
				g_free (dir);
			}
			gtk_file_chooser_set_current_name
				(GTK_FILE_CHOOSER (filechooser), project->name);

			
			if (gtk_dialog_run (GTK_DIALOG(filechooser)) == GTK_RESPONSE_OK)
				path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filechooser));

			gtk_widget_destroy (filechooser);

			if (!path)
				break;

			gpw_save (gpw, project, path);

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

	gtk_widget_destroy (dialog);
	return close;
}

static void
do_close (GladeProjectWindow *gpw, GladeProject *project)
{
	GladeProject *active_project;

	g_signal_handlers_disconnect_by_func (G_OBJECT (project),
					      G_CALLBACK (gpw_project_notify_handler_cb),
					      gpw);	

	glade_app_remove_project (project);

	gpw_refresh_projects_list_menu (gpw);

	gpw_refresh_title (gpw);

	active_project = glade_app_get_project ();
	if (active_project != NULL)
		gpw_set_sensitivity_according_to_project (gpw, active_project);
	else
		gtk_action_group_set_sensitive (gpw->priv->project_actions, FALSE);	
}

static void
gpw_close_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	GladeProject *project;
	gboolean close;
	
	project = glade_app_get_project ();

	if (!project)
		return;

	if (project->changed)
	{
		close = gpw_confirm_close_project (gpw, project);
			if (!close)
				return;
	}
	do_close (gpw, project);
}

static void
gpw_quit_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	GList *list;

	for (list = glade_app_get_projects (); list; list = list->next)
	{
		GladeProject *project = GLADE_PROJECT (list->data);

		if (project->changed)
		{
			gboolean quit = gpw_confirm_close_project (gpw, project);
			if (!quit)
				return;
		}
	}

	while (glade_app_get_projects ())
	{
		GladeProject *project = GLADE_PROJECT (glade_app_get_projects ()->data);
		do_close (gpw, project);
	}

	gtk_main_quit ();
}

static void
gpw_copy_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	glade_app_command_copy ();
}

static void
gpw_cut_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	glade_app_command_cut ();
}

static void
gpw_paste_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	glade_app_command_paste (NULL);
}

static void
gpw_delete_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	if (!glade_app_get_project ())
	{
		g_warning ("delete should not be sensitive: we don't have a project");
		return;
	}
	glade_app_command_delete ();
}

static void
gpw_undo_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	if (!glade_app_get_project ())
	{
		g_warning ("undo should not be sensitive: we don't have a project");
		return;
	}
	glade_app_command_undo ();
}

static void
gpw_redo_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	if (!glade_app_get_project ())
	{
		g_warning ("redo should not be sensitive: we don't have a project");
		return;
	}
	glade_app_command_redo ();
}

static gboolean
gpw_hide_palette_on_delete (GtkWidget *palette, gpointer not_used, GtkUIManager *ui)
{
	glade_util_hide_window (GTK_WINDOW (palette));
	return TRUE;
}

static void
gpw_create_palette (GladeProjectWindow *gpw)
{
	g_return_if_fail (gpw != NULL);

	gpw->priv->palette_window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
	gtk_window_set_default_size (GTK_WINDOW (gpw->priv->palette_window), -1,
				     GLADE_PALETTE_DEFAULT_HEIGHT);


	gtk_window_set_title (gpw->priv->palette_window, _("Palette"));
	gtk_window_set_resizable (gpw->priv->palette_window, TRUE);
	gtk_window_move (gpw->priv->palette_window, 0, 250);

	gtk_container_add (GTK_CONTAINER (gpw->priv->palette_window), 
			   GTK_WIDGET (glade_app_get_palette()));

	/* Delete event, don't destroy it */
	g_signal_connect (G_OBJECT (gpw->priv->palette_window), "delete_event",
			  G_CALLBACK (gpw_hide_palette_on_delete), gpw->priv->ui);
}

static void
gpw_show_palette (GladeProjectWindow *gpw, gboolean raise)
{
	g_return_if_fail (gpw != NULL);

	if (gpw->priv->palette_window == NULL)
		gpw_create_palette (gpw);

	gtk_widget_show (GTK_WIDGET (gpw->priv->palette_window));

	if (raise)
		gtk_window_present (gpw->priv->palette_window);
}

static gboolean
gpw_hide_editor_on_delete (GtkWidget *editor, gpointer not_used,
		GtkUIManager *ui)
{
	glade_util_hide_window (GTK_WINDOW (editor));
	return TRUE;
}

static gint
gpw_hijack_editor_key_press (GtkWidget          *editor_win, 
			     GdkEventKey        *event, 
			     GladeProjectWindow *gpw)
{
	if (event->keyval == GDK_Delete || /* Filter Delete from accelerator keys */
	    ((event->state & GDK_CONTROL_MASK) && /* CNTL keys... */
	     ((event->keyval == GDK_c || event->keyval == GDK_C) || /* CNTL-C (copy)  */
	      (event->keyval == GDK_x || event->keyval == GDK_X) || /* CNTL-X (cut)   */
	      (event->keyval == GDK_v || event->keyval == GDK_V)))) /* CNTL-V (paste) */
	{
		return gtk_widget_event (GTK_WINDOW (editor_win)->focus_widget, 
					 (GdkEvent *)event);
	}
	return FALSE;
}

static void
gpw_doc_search_cb (GladeEditor        *editor,
		   const gchar        *book,
		   const gchar        *page,
		   const gchar        *search,
		   GladeProjectWindow *gpw)
{
	glade_util_search_devhelp (book, page, search);
}

static void
gpw_create_editor (GladeProjectWindow *gpw)
{
	g_return_if_fail (gpw != NULL);

	gpw->priv->editor_window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
	gtk_window_set_default_size (GTK_WINDOW (gpw->priv->editor_window), 400, 450);

	gtk_window_set_title  (gpw->priv->editor_window, _("Properties"));
	gtk_window_move (gpw->priv->editor_window, 550, 0);

	gtk_container_set_border_width (GTK_CONTAINER (gpw->priv->editor_window), 6);

	gtk_container_add (GTK_CONTAINER (gpw->priv->editor_window), 
			   GTK_WIDGET (glade_app_get_editor ()));

	/* Delete event, don't destroy it */
	g_signal_connect (G_OBJECT (gpw->priv->editor_window), "delete_event",
			  G_CALLBACK (gpw_hide_editor_on_delete), gpw->priv->ui);

	g_signal_connect (G_OBJECT (gpw->priv->editor_window), "key-press-event",
			  G_CALLBACK (gpw_hijack_editor_key_press), gpw);
}

static void
gpw_show_editor (GladeApp *app, gboolean raise)
{
	GladeProjectWindow *gpw = GLADE_PROJECT_WINDOW (app);

	g_return_if_fail (GLADE_IS_PROJECT_WINDOW (gpw));

	gtk_widget_show (GTK_WIDGET (gpw->priv->editor_window));

	if (raise)
		gtk_window_present (gpw->priv->editor_window);
}

static void
gpw_hide_editor (GladeApp *app)
{
	GladeProjectWindow *gpw = GLADE_PROJECT_WINDOW (app);

	g_return_if_fail (GLADE_IS_PROJECT_WINDOW (gpw));

	glade_util_hide_window (gpw->priv->editor_window);
}

static void 
gpw_expand_treeview (GtkButton *button, GtkTreeView *tree)
{
	gtk_tree_view_expand_all (tree);
	gtk_widget_queue_draw (GTK_WIDGET (tree));
}


static GtkWidget* 
gpw_create_widget_tree_contents (GladeProjectWindow *gpw)
{
	GtkWidget *hbox, *vbox;
	GladeProjectView *view;

	view = glade_project_view_new (GLADE_PROJECT_VIEW_TREE);
	glade_app_add_project_view (view);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (view), GTK_SHADOW_IN);


	gtk_container_set_border_width (GTK_CONTAINER (view), GLADE_GENERIC_BORDER_WIDTH);

	/* Add control buttons to the treeview. */
	hbox     = gtk_hbutton_box_new ();
	vbox     = gtk_vbox_new (FALSE, 0);

	gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_START);

	gpw->priv->expand = gtk_button_new_with_mnemonic (_("E_xpand all"));
	gtk_container_set_border_width (GTK_CONTAINER (gpw->priv->expand), 
					GLADE_GENERIC_BORDER_WIDTH);

	gpw->priv->collapse = gtk_button_new_with_mnemonic (_("_Collapse all"));
	gtk_container_set_border_width (GTK_CONTAINER (gpw->priv->collapse), 
					GLADE_GENERIC_BORDER_WIDTH);

	gtk_box_pack_start (GTK_BOX (hbox), gpw->priv->expand, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), gpw->priv->collapse, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (view), TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (gpw->priv->expand), "clicked",
			  G_CALLBACK (gpw_expand_treeview), 
			  view->tree_view);

	g_signal_connect_swapped (G_OBJECT (gpw->priv->collapse), "clicked",
				  G_CALLBACK (gtk_tree_view_collapse_all), 
				  view->tree_view);

	return vbox;
}

static gboolean
gpw_hide_clipboard_view_on_delete (GtkWidget *clipboard_view, gpointer not_used,
				   GtkUIManager *ui)
{
	glade_util_hide_window (GTK_WINDOW (clipboard_view));
	return TRUE;
}

static void 
gpw_create_clipboard_view (GladeProjectWindow *gpw)
{
	GtkWidget *view;
	
	view = glade_app_get_clipboard_view ();

	g_signal_connect (G_OBJECT (view), "delete_event",
			  G_CALLBACK (gpw_hide_clipboard_view_on_delete),
			  gpw->priv->ui);
}

static void
gpw_show_clipboard_view (GladeProjectWindow *gpw)
{
	static gboolean created = FALSE;
	
	g_return_if_fail (gpw != NULL);

	if (!created)
	{
		gpw_create_clipboard_view (gpw);
		created = TRUE;
	}
	
	gtk_widget_show_all (GTK_WIDGET (glade_app_get_clipboard_view ()));
        gtk_window_present (GTK_WINDOW (glade_app_get_clipboard_view ()));
}

static void
gpw_palette_appearance_change_cb (GtkRadioAction *action,
				  GtkRadioAction *current,
				  GladeProjectWindow *gpw)
{
	gint value;

	value = gtk_radio_action_get_current_value (action);

	glade_palette_set_item_appearance (glade_app_get_palette (), value);

}

static void
gpw_palette_toggle_small_icons_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)))
		glade_palette_set_use_small_item_icons (glade_app_get_palette(), TRUE);
	else
		glade_palette_set_use_small_item_icons (glade_app_get_palette(), FALSE);

}

static void
gpw_show_palette_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	gpw_show_palette (gpw, TRUE);
}

static void
gpw_show_editor_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	gpw_show_editor (GLADE_APP (gpw), TRUE);
}

static void
gpw_show_clipboard_cb (GtkAction *action, GladeProjectWindow *gpw)
{
		gpw_show_clipboard_view (gpw);
}

static void
gpw_toggle_editor_help_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	GtkWidget *help_item;

	if (glade_util_have_devhelp() == FALSE)
		return;

	help_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						 "/MenuBar/ViewMenu/PropertyEditorHelp");

	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (help_item)))
		glade_editor_show_context_info (glade_app_get_editor ());
	else
		glade_editor_hide_context_info (glade_app_get_editor ());
}

static void 
gpw_documentation_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	glade_util_search_devhelp ("gladeui", NULL, NULL);
}

static void 
gpw_about_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	gchar *authors[] =
		{ "Chema Celorio <chema@ximian.com>",
		  "Joaquin Cuenca Abela <e98cuenc@yahoo.com>",
		  "Paolo Borelli <pborelli@katamail.com>",
		  "Archit Baweja <bighead@users.sourceforge.net>",
		  "Shane Butler <shane_b@operamail.com>",
		  "Tristan Van Berkom <tvb@gnome.org>",
		  "Ivan Wong <email@ivanwong.info>",
		  "Juan Pablo Ugarte <juanpablougarte@gmail.com>",
		  "Vincent Geddes <vincent.geddes@gmail.com>",
		  NULL };

	gchar *translators =
		_("Fatih Demir <kabalak@gtranslator.org>\n"
		  "Christian Rose <menthos@menthos.com>\n"
		  "Pablo Saratxaga <pablo@mandrakesoft.com>\n"
		  "Duarte Loreto <happyguy_pt@hotmail.com>\n"
		  "Zbigniew Chyla <cyba@gnome.pl>\n"
		  "Hasbullah Bin Pit <sebol@ikhlas.com>\n"
		  "Takeshi AIHANA <aihana@gnome.gr.jp>\n"
		  "Kjartan Maraas <kmaraas@gnome.org>\n"
		  "Carlos Perell Marn <carlos@gnome-db.org>\n"
		  "Valek Filippov <frob@df.ru>\n"
		  "Stanislav Visnovsky <visnovsky@nenya.ms.mff.cuni.cz>\n"
		  "Christophe Merlet <christophe@merlet.net>\n"
		  "Funda Wang <fundawang@linux.net.cn>\n"
		  "Francisco Javier F. Serrador <serrador@cvs.gnome.org>\n"
		  "Adam Weinberger <adamw@gnome.org>\n"
		  "Raphael Higino <raphaelh@cvs.gnome.org>\n"
		  "Clytie Siddall <clytie@riverland.net.au>\n"
		  "Jordi Mas <jmas@softcatala.org>\n"
		  "Vincent van Adrighem <adrighem@gnome.org>\n"
		  "Daniel Nylander <po@danielnylander.se>\n");

	gchar *comments =
		_("Glade is a User Interface Designer for GTK+ and GNOME.\n"
		  "This version is a rewrite of the Glade 2 version, "
		  "originally created by Damon Chaplin\n");

	gchar *license =
		_("This program is free software; you can redistribute it and/or modify\n"
		  "it under the terms of the GNU General Public License as\n"
		  "published by the Free Software Foundation; either version 2 of the\n"
		  "License, or (at your option) any later version.\n\n"
		  "This program is distributed in the hope that it will be useful,\n"
		  "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		  "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		  "GNU General Public License for more details.\n\n"
		  "You should have received a copy of the GNU General Public License\n"
		  "along with this program; if not, write to the Free Software\n"
		  "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, "
		  "MA 02110-1301, USA.");

	gchar *copyright =
		"(C) 2001-2006 Ximian, Inc.\n"
		"(C) 2001-2006 Joaquin Cuenca Abela, Paolo Borelli, et al.\n"
		"(C) 2001-2006 Tristan Van Berkom, Juan Pablo Ugarte, et al.";
	
	gtk_show_about_dialog (GTK_WINDOW (gpw->priv->window),
			       "name",  PACKAGE_NAME,
			       "logo-icon-name", "glade-3",
			       "authors", authors,
			       "translator-credits", translators,
			       "comments", comments,
			       "license", license,
			       "copyright", copyright,
			       "version", PACKAGE_VERSION,
			       NULL);
}

static const gchar *ui_info =
"<ui>\n"
"  <menubar name='MenuBar'>\n"
"    <menu action='FileMenu'>\n"
"      <menuitem action='New'/>\n"
"      <menuitem action='Open'/>\n"
"      <menu action='Recents'>\n"
"        <separator/>\n"
"        <menuitem action='ClearRecents'/>\n"
"      </menu>\n"
"      <separator/>\n"
"      <menuitem action='Save'/>\n"
"      <menuitem action='SaveAs'/>\n"
"      <separator/>\n"
"      <menuitem action='Close'/>\n"
"      <menuitem action='Quit'/>\n"
"    </menu>\n"
"    <menu action='EditMenu'>\n"
"      <menuitem action='Undo'/>\n"
"      <menuitem action='Redo'/>\n"
"      <separator/>\n"
"      <menuitem action='Cut'/>\n"
"      <menuitem action='Copy'/>\n"
"      <menuitem action='Paste'/>\n"
"      <menuitem action='Delete'/>\n"
"    </menu>\n"
"    <menu action='ViewMenu'>\n"
"      <menuitem action='PropertyEditor'/>\n"
"      <menuitem action='Clipboard'/>\n"
"      <menuitem action='Palette'/>\n"
"      <separator/>\n"
"      <menu action='PaletteAppearance'>\n"
"        <menuitem action='IconsAndLabels'/>\n"
"        <menuitem action='IconsOnly'/>\n"
"        <menuitem action='LabelsOnly'/>\n"
"        <separator/>\n"
"        <menuitem action='UseSmallIcons'/>\n"
"      </menu>\n"
"      <menuitem action='PropertyEditorHelp'/>\n"
"    </menu>\n"
"    <menu action='ProjectMenu'>\n"
"      <placeholder name='ProjectsListPlaceholder'/>\n"
"    </menu>\n"
"    <menu action='HelpMenu'>\n"
"      <menuitem action='Manual'/>\n"
"      <separator/>\n"
"      <menuitem action='About'/>\n"
"    </menu>\n"
"  </menubar>\n"
"  <toolbar  name='ToolBar'>"
"    <toolitem action='Open'/>"
"    <toolitem action='Save'/>"
"    <separator/>\n"
"    <toolitem action='Undo'/>"
"    <toolitem action='Redo'/>"
"  </toolbar>"
"</ui>\n";

static GtkActionEntry static_entries[] = {
	{ "FileMenu", NULL, N_("_File") },
	{ "EditMenu", NULL, N_("_Edit") },
	{ "ViewMenu", NULL, N_("_View") },
	{ "ProjectMenu", NULL, N_("_Project") },
	{ "HelpMenu", NULL, N_("_Help") },
	
	/* FileMenu */
	{ "New", GTK_STOCK_NEW, N_("_New"), "<control>N",
	  N_("Create a new project file"), G_CALLBACK (gpw_new_cb) },
	
	{ "Open", GTK_STOCK_OPEN, N_("_Open"),"<control>O",
	  N_("Open a project file"), G_CALLBACK (gpw_open_cb) },
	
	{ "Recents", NULL, N_("Open _Recent"), NULL, NULL },
	
	{ "ClearRecents", GTK_STOCK_CLEAR, N_("Clear Recent Projects"), NULL,
	  NULL, G_CALLBACK (gpw_recent_project_clear_cb) },
	
	{ "Quit", GTK_STOCK_QUIT, N_("_Quit"), "<control>Q",
	  N_("Quit the program"), G_CALLBACK (gpw_quit_cb) },

	/* ViewMenu */
	{ "PaletteAppearance", NULL, "Palette _Appearance" },
	
	/* HelpMenu */
	{ "About", GTK_STOCK_ABOUT, N_("_About"), NULL,
	  N_("Shows the About Dialog"), G_CALLBACK (gpw_about_cb) },

	{ "Manual", NULL, N_("_Documentation"), NULL,
	  N_("Documentation about Glade"), G_CALLBACK (gpw_documentation_cb) }
};
static guint n_static_entries = G_N_ELEMENTS (static_entries);

static GtkActionEntry project_entries[] = {

	/* FileMenu */
	{ "Save", GTK_STOCK_SAVE, N_("_Save"),"<control>S",
	  N_("Save the current project file"), G_CALLBACK (gpw_save_cb) },
	
	{ "SaveAs", GTK_STOCK_SAVE_AS, N_("Save _As..."), NULL,
	  N_("Save the current project file with a different name"), G_CALLBACK (gpw_save_as_cb) },
	
	{ "Close", GTK_STOCK_CLOSE, N_("_Close"), "<control>W",
	  N_("Close the current project file"), G_CALLBACK (gpw_close_cb) },

	/* EditMenu */	
	{ "Undo", GTK_STOCK_UNDO, N_("_Undo"), "<control>Z",
	  N_("Undo the last action"),	G_CALLBACK (gpw_undo_cb) },
	
	{ "Redo", GTK_STOCK_REDO, N_("_Redo"), "<control>R",
	  N_("Redo the last action"),	G_CALLBACK (gpw_redo_cb) },
	
	{ "Cut", GTK_STOCK_CUT, N_("C_ut"), NULL,
	  N_("Cut the selection"), G_CALLBACK (gpw_cut_cb) },
	
	{ "Copy", GTK_STOCK_COPY, N_("_Copy"), NULL,
	  N_("Copy the selection"), G_CALLBACK (gpw_copy_cb) },
	
	{ "Paste", GTK_STOCK_PASTE, N_("_Paste"), NULL,
	  N_("Paste the clipboard"), G_CALLBACK (gpw_paste_cb) },
	
	{ "Delete", GTK_STOCK_DELETE, N_("_Delete"), "Delete",
	  N_("Delete the selection"), G_CALLBACK (gpw_delete_cb) },

	/* ViewMenu */

	{ "PropertyEditor", NULL, N_("Property _Editor"), NULL,
	  N_("Show the property editor"),
	  G_CALLBACK (gpw_show_editor_cb) },

	{ "Palette", NULL, N_("_Palette"), NULL,
	  N_("Show the palette of widgets"),
	  G_CALLBACK (gpw_show_palette_cb) },
	
	{ "Clipboard", NULL, N_("_Clipboard"), NULL,
	  N_("Show the clipboard"),
	  G_CALLBACK (gpw_show_clipboard_cb) }


};
static guint n_project_entries = G_N_ELEMENTS (project_entries);

static GtkToggleActionEntry view_entries[] = {

	{ "UseSmallIcons", NULL, N_("_Use Small Icons"), NULL,
	  N_("Show items using small icons"),
	  G_CALLBACK (gpw_palette_toggle_small_icons_cb), FALSE },

	{ "PropertyEditorHelp", NULL, N_("Context _Help"), NULL,
	  N_("Show or hide contextual help buttons in the editor"),
	  G_CALLBACK (gpw_toggle_editor_help_cb), FALSE }
};
static guint n_view_entries = G_N_ELEMENTS (view_entries);

static GtkRadioActionEntry radio_entries[] = {

	{ "IconsAndLabels", NULL, N_("Text beside icons"), NULL, 
	  N_("Display items as text beside icons"), GLADE_ITEM_ICON_AND_LABEL },

	{ "IconsOnly", NULL, N_("_Icons only"), NULL, 
	  N_("Display items as icons only"), GLADE_ITEM_ICON_ONLY },

	{ "LabelsOnly", NULL, N_("_Text only"), NULL, 
	  N_("Display items as text only"), GLADE_ITEM_LABEL_ONLY },
};
static guint n_radio_entries = G_N_ELEMENTS (radio_entries);

static void
gpw_menu_item_selected_cb (GtkItem *item, GladeProjectWindow *gpw)
{
	GtkAction *action;
	gchar *tooltip;

	action = GTK_ACTION (g_object_get_data (G_OBJECT (item), "action-for-proxy"));

	g_object_get (G_OBJECT (action), "tooltip", &tooltip, NULL);

	if (tooltip != NULL)
		gtk_statusbar_push (GTK_STATUSBAR (gpw->priv->statusbar),
				    gpw->priv->statusbar_menu_context_id, tooltip);

	g_free (tooltip);
}

static void
gpw_menu_item_deselected_cb (GtkItem *item, GladeProjectWindow *gpw)
{
	gtk_statusbar_pop (GTK_STATUSBAR (gpw->priv->statusbar),
			   gpw->priv->statusbar_menu_context_id);
}

static void
gpw_ui_connect_proxy_cb (GtkUIManager *ui,
		         GtkAction    *action,
		         GtkWidget    *proxy,
		         GladeProjectWindow *gpw)
{	
	if (GTK_IS_MENU_ITEM (proxy))
	{	
		g_object_set_data (G_OBJECT (proxy), "action-for-proxy", action);
	
		g_signal_connect(G_OBJECT(proxy), "select", 
				 G_CALLBACK (gpw_menu_item_selected_cb), gpw);
		g_signal_connect(G_OBJECT(proxy), "deselect", 
				 G_CALLBACK (gpw_menu_item_deselected_cb), gpw);
	}
}

static void
gpw_ui_disconnect_proxy_cb (GtkUIManager *manager,
                     	    GtkAction *action,
                            GtkWidget *proxy,
                            GladeProjectWindow *gpw)
{
	if (GTK_IS_MENU_ITEM (proxy))
	{
		g_signal_handlers_disconnect_by_func
                	(proxy, G_CALLBACK (gpw_menu_item_selected_cb), gpw);
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (gpw_menu_item_deselected_cb), gpw);
	}
}

static GtkWidget *
gpw_construct_menu (GladeProjectWindow *gpw)
{
	GError *error = NULL;
	
	gpw->priv->static_actions = gtk_action_group_new (GLADE_ACTION_GROUP_STATIC);
	gtk_action_group_set_translation_domain (gpw->priv->static_actions, GETTEXT_PACKAGE);

	gtk_action_group_add_actions (gpw->priv->static_actions,
				      static_entries,
				      n_static_entries,
				      gpw);
	gtk_action_group_add_toggle_actions (gpw->priv->static_actions, 
					     view_entries,
					     n_view_entries, 
					     gpw);
	gtk_action_group_add_radio_actions (gpw->priv->static_actions, radio_entries,
					    n_radio_entries, GLADE_ITEM_ICON_ONLY,
					    G_CALLBACK (gpw_palette_appearance_change_cb), gpw);

	gpw->priv->project_actions = gtk_action_group_new (GLADE_ACTION_GROUP_PROJECT);
	gtk_action_group_set_translation_domain (gpw->priv->project_actions, GETTEXT_PACKAGE);

	gtk_action_group_add_actions (gpw->priv->project_actions,
				      project_entries,
				      n_project_entries,
				      gpw);

	gpw->priv->projects_list_menu_actions = 
		gtk_action_group_new (GLADE_ACTION_GROUP_PROJECTS_LIST_MENU);
	gtk_action_group_set_translation_domain (gpw->priv->projects_list_menu_actions, 
						 GETTEXT_PACKAGE);

	gpw->priv->recent_actions = gtk_action_group_new (GLADE_ACTION_GROUP_RECENT);
	gtk_action_group_set_translation_domain (gpw->priv->recent_actions, 
						 GETTEXT_PACKAGE);
	
	gpw->priv->ui = gtk_ui_manager_new ();

	g_signal_connect(G_OBJECT(gpw->priv->ui), "connect-proxy",
			 G_CALLBACK (gpw_ui_connect_proxy_cb), gpw);
	g_signal_connect(G_OBJECT(gpw->priv->ui), "disconnect-proxy",
			 G_CALLBACK (gpw_ui_disconnect_proxy_cb), gpw);

	gtk_ui_manager_insert_action_group (gpw->priv->ui, gpw->priv->static_actions, 0);
	gtk_ui_manager_insert_action_group (gpw->priv->ui, gpw->priv->project_actions, 1);
	gtk_ui_manager_insert_action_group (gpw->priv->ui, gpw->priv->recent_actions, 2);
	gtk_ui_manager_insert_action_group (gpw->priv->ui, gpw->priv->projects_list_menu_actions, 3);
	
	gtk_window_add_accel_group (GTK_WINDOW (gpw->priv->window), 
				  gtk_ui_manager_get_accel_group (gpw->priv->ui));

	if (!gtk_ui_manager_add_ui_from_string (gpw->priv->ui, ui_info, -1, &error))
	{
		g_message ("Building menus failed: %s", error->message);
		g_error_free (error);
	}

	gtk_widget_set_sensitive (gtk_ui_manager_get_widget 
				  (gpw->priv->ui, "/MenuBar/FileMenu/Recents"),
				  FALSE);
	
	return gtk_ui_manager_get_widget (gpw->priv->ui, "/MenuBar");
}

static GtkWidget *
gpw_construct_statusbar (GladeProjectWindow *gpw)
{
	GtkWidget *statusbar;

	statusbar = gtk_statusbar_new ();
	gpw->priv->statusbar = statusbar;
	gpw->priv->statusbar_menu_context_id =
		gtk_statusbar_get_context_id (GTK_STATUSBAR (statusbar), "menu");
	gpw->priv->statusbar_actions_context_id =
		gtk_statusbar_get_context_id (GTK_STATUSBAR (statusbar), "actions");

	return statusbar;
}

enum
{
	TARGET_URI_LIST
};

static GtkTargetEntry drop_types[] =
{
	{"text/uri-list", 0, TARGET_URI_LIST}
};

static void
gpw_drag_data_received (GtkWidget *widget,
			GdkDragContext *context,
			gint x, gint y,
			GtkSelectionData *selection_data,
			guint info, guint time, GladeProjectWindow *gpw)
{
	gchar *uri_list;
	GList *list = NULL;

	if (info != TARGET_URI_LIST)
		return;

	/*
	 * Mmh... it looks like on Windows selection_data->data is not
	 * NULL terminated, so we need to make sure our local copy is.
	 */
	uri_list = g_new (gchar, selection_data->length + 1);
	memcpy (uri_list, selection_data->data, selection_data->length);
	uri_list[selection_data->length] = 0;

	list = glade_util_uri_list_parse (uri_list);
	for (; list; list = list->next)
	{
		if (list->data)
			glade_project_window_open_project (gpw, list->data);

		/* we can now free each string in the list */
		g_free (list->data);
	}

	g_list_free (list);
}

static gboolean
gpw_delete_event (GtkWindow *w, GdkEvent *event, GladeProjectWindow *gpw)
{
	gpw_quit_cb (NULL, gpw);
	
	/* return TRUE to stop other handlers */
	return TRUE;	
}

static void
glade_project_window_create (GladeProjectWindow *gpw)
{
	GtkWidget *app;
	GtkWidget *vbox;
	GtkWidget *menubar;
	GtkWidget *toolbar;
	GtkWidget *project_view;
	GtkWidget *statusbar;
	GtkWidget *editor_item, *docs_item;

	app = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_move (GTK_WINDOW (app), 0, 0);
	gtk_window_set_default_size (GTK_WINDOW (app), 300, 450);

	gpw->priv->window = app;

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (app), vbox);
	gpw->priv->main_vbox = vbox;

	menubar = gpw_construct_menu (gpw);
	toolbar = gtk_ui_manager_get_widget (gpw->priv->ui, "/ToolBar");
	project_view = gpw_create_widget_tree_contents (gpw);
	statusbar = gpw_construct_statusbar (gpw);

	gtk_widget_show (menubar);
	gtk_widget_show (toolbar);
	gtk_widget_show_all (project_view);
	gtk_widget_show (statusbar);

	gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, TRUE, 0);

	editor_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						 "/MenuBar/ViewMenu/PropertyEditorHelp");
	docs_item = gtk_ui_manager_get_widget (gpw->priv->ui,
					       "/MenuBar/HelpMenu/Manual");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (editor_item), FALSE);

	gtk_widget_set_sensitive (editor_item, FALSE);
	gtk_widget_set_sensitive (docs_item, FALSE);

	gtk_box_pack_start (GTK_BOX (vbox), project_view, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (vbox), statusbar, FALSE, TRUE, 0);
		
	gtk_widget_show (vbox);

	gpw_recent_project_config_load (gpw);
	
	/* support for opening a file by dragging onto the project window */
	gtk_drag_dest_set (app,
			   GTK_DEST_DEFAULT_ALL,
			   drop_types, G_N_ELEMENTS (drop_types),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect (G_OBJECT (app), "drag-data-received",
			  G_CALLBACK (gpw_drag_data_received), gpw);

	g_signal_connect (G_OBJECT (app), "delete_event",
			  G_CALLBACK (gpw_delete_event), gpw);
}

static void
glade_project_window_add_project (GladeProjectWindow *gpw, GladeProject *project)
{
 	g_return_if_fail (GLADE_IS_PROJECT (project));
	
	glade_app_add_project (project);

	/* Connect callback handler for notify signals emitted by projects */
	g_signal_connect (G_OBJECT (project), "notify::has-unsaved-changes",
			  G_CALLBACK (gpw_project_notify_handler_cb),
			  gpw);
	g_signal_connect (G_OBJECT (project), "notify::has-selection",
			  G_CALLBACK (gpw_project_notify_handler_cb),
			  gpw);
	g_signal_connect (G_OBJECT (project), "notify::read-only",
			  G_CALLBACK (gpw_project_notify_handler_cb),
			  gpw);

	gpw_set_sensitivity_according_to_project (gpw, project);

	gpw_refresh_projects_list_menu (gpw);

	gpw_refresh_title (gpw);

	if (gtk_action_group_get_sensitive (gpw->priv->project_actions) == FALSE)
		gtk_action_group_set_sensitive (gpw->priv->project_actions, TRUE);
}

void
glade_project_window_new_project (GladeProjectWindow *gpw)
{
	GladeProject *project;

	project = glade_project_new (TRUE);
	if (!project)
	{
		glade_util_ui_message (gpw->priv->window, 
				       GLADE_UI_ERROR,
				       _("Could not create a new project."));
		return;
	}
	glade_project_window_add_project (gpw, project);
}

/*
 * Opens the project specified by path and adds it to the GladeProjectWindow.
 */
void
glade_project_window_open_project (GladeProjectWindow *gpw, const gchar *path)
{
	GladeProject *project;

	g_return_if_fail (path != NULL);

	/* dont allow more than one project with the same name to be
	 * opened simultainiously.
	 */
	if (glade_app_is_project_loaded ((gchar*)path))
	{
		glade_util_ui_message (gpw->priv->window, GLADE_UI_WARN, 
				       _("%s is already open"), path);
		return;
	}
	
	if ((project = glade_project_open (path)) == NULL)
		return;

	gpw_recent_project_add (gpw, project->path);
	gpw_recent_project_config_save (gpw);
	glade_app_config_save ();

	glade_project_window_add_project (gpw, project);
}

static void
glade_project_window_change_menu_label (GladeProjectWindow *gpw,
					const gchar *path,
					const gchar *action_label,
					const gchar *action_description)
{
	GtkBin *bin;
	GtkLabel *label;
	gchar *text;

	g_assert (GLADE_IS_PROJECT_WINDOW (gpw));
	g_return_if_fail (path != NULL);
	g_return_if_fail (action_label != NULL);

	bin = GTK_BIN (gtk_ui_manager_get_widget (gpw->priv->ui, path));
	label = GTK_LABEL (gtk_bin_get_child (bin));

	if (action_description == NULL)
		text = g_strdup (action_label);
	else
		text = g_strdup_printf ("%s: %s", action_label, action_description);
	
	gtk_label_set_text_with_mnemonic (label, text);

	g_free (text);
}

static void
gpw_refresh_undo_redo (GladeProjectWindow *gpw)
{
	GladeCommand *undo = NULL, *redo = NULL;
	GladeProject *project;
	GtkAction    *action;
	gchar        *tooltip;

	project = glade_app_get_project ();

	if (project != NULL)
	{
		undo = glade_project_next_undo_item (project);
		redo = glade_project_next_redo_item (project);
	}

	/* Refresh Undo */
	action = gtk_action_group_get_action (gpw->priv->project_actions, "Undo");
	gtk_action_set_sensitive (action, undo != NULL);

	glade_project_window_change_menu_label
		(gpw, "/MenuBar/EditMenu/Undo", _("_Undo"), undo ? undo->description : NULL);

	tooltip = g_strdup_printf (_("Undo: %s"), undo ? undo->description : _("the last action"));
	g_object_set (action, "tooltip", tooltip, NULL);
	g_free (tooltip);
	
	/* Refresh Redo */
	action = gtk_action_group_get_action (gpw->priv->project_actions, "Redo");
	gtk_action_set_sensitive (action, redo != NULL);

	glade_project_window_change_menu_label
		(gpw, "/MenuBar/EditMenu/Redo", _("_Redo"), redo ? redo->description : NULL);

	tooltip = g_strdup_printf (_("Redo: %s"), redo ? redo->description : _("the last action"));
	g_object_set (action, "tooltip", tooltip, NULL);
	g_free (tooltip);
}

static void
glade_project_window_update_ui (GladeApp *app)
{
	GladeProjectWindow *gpw = GLADE_PROJECT_WINDOW (app);
	
	/* Chain Up */
	GLADE_APP_CLASS (parent_class)->update_ui_signal (app);
	
	gpw_refresh_undo_redo (gpw);
}

void
glade_project_window_show_all (GladeProjectWindow *gpw)
{
	gpw_show_palette (gpw, FALSE);
	gpw_show_editor (GLADE_APP (gpw), FALSE);
        gtk_widget_show (gpw->priv->window);
}

static void
glade_project_window_finalize (GObject *object)
{
	GladeProjectWindow *gpw = GLADE_PROJECT_WINDOW (object);

	g_queue_foreach (gpw->priv->recent_projects, (GFunc)g_object_unref, NULL);
	g_queue_free (gpw->priv->recent_projects);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
glade_project_window_class_init (GladeProjectWindowClass * klass)
{
	GObjectClass *object_class;
	GladeAppClass *app_class;
	g_return_if_fail (klass != NULL);


	
	parent_class = g_type_class_peek_parent (klass);
	object_class = G_OBJECT_CLASS  (klass);
	app_class    = GLADE_APP_CLASS (klass);

	object_class->finalize = glade_project_window_finalize;

	app_class->update_ui_signal = glade_project_window_update_ui;
	app_class->show_properties  = gpw_show_editor;
	app_class->hide_properties  = gpw_hide_editor;
}

static void
glade_project_window_init (GladeProjectWindow *gpw)
{
	gpw->priv = g_new0 (GladeProjectWindowPriv, 1);

	/* initialize widgets */
	gpw->priv->recent_projects = g_queue_new ();
}

GType
glade_project_window_get_type ()
{
	static GType obj_type = 0;
	
	if (!obj_type)
	{
		static const GTypeInfo obj_info = 
		{
			sizeof (GladeProjectWindowClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) glade_project_window_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,           /* class_data */
			sizeof (GladeProjectWindow),
			0,              /* n_preallocs */
			(GInstanceInitFunc) glade_project_window_init,
			NULL            /* value_table */
		};
		obj_type = g_type_register_static (GLADE_TYPE_APP,
		                                   "GladeProjectWindow", &obj_info, 0);
	}
	return obj_type;
}

GladeProjectWindow *
glade_project_window_new (void)
{
	GladeProjectWindow *gpw;
	GtkAccelGroup *accel_group;
	
	gpw = g_object_new (GLADE_TYPE_PROJECT_WINDOW, NULL);

	glade_project_window_create (gpw);
	gpw_create_palette (gpw);
	gpw_create_editor  (gpw);

	glade_app_set_window (gpw->priv->window);

	accel_group = gtk_ui_manager_get_accel_group(gpw->priv->ui);
	glade_app_set_accel_group (accel_group);

	gtk_window_add_accel_group(gpw->priv->palette_window, accel_group);
	gtk_window_add_accel_group(gpw->priv->editor_window, accel_group);
	gtk_window_add_accel_group(GTK_WINDOW (glade_app_get_clipboard_view ()), accel_group);

	/* Connect callback handler for notify signals emitted by clipboard */
	g_signal_connect (G_OBJECT (glade_app_get_clipboard ()), "notify::has-selection",
			  G_CALLBACK (gpw_clipboard_notify_handler_cb),
			  gpw);
	return gpw;
}

void
glade_project_window_check_devhelp (GladeProjectWindow *gpw)
{
	GtkWidget *editor_item, *docs_item;
	
	editor_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						 "/MenuBar/ViewMenu/PropertyEditorHelp");
	docs_item = gtk_ui_manager_get_widget (gpw->priv->ui,
					       "/MenuBar/HelpMenu/Manual");

	if (glade_util_have_devhelp ())
	{
		GladeEditor *editor = glade_app_get_editor ();
		glade_editor_show_info (editor);
		glade_editor_hide_context_info (editor);
		
		g_signal_handlers_disconnect_by_func (editor, gpw_doc_search_cb, gpw);
		
		g_signal_connect (editor, "gtk-doc-search",
				  G_CALLBACK (gpw_doc_search_cb), gpw);
		
		gtk_widget_set_sensitive (editor_item, TRUE);
		gtk_widget_set_sensitive (docs_item, TRUE);
	}
	else
	{
		gtk_widget_set_sensitive (editor_item, FALSE);
		gtk_widget_set_sensitive (docs_item, FALSE);
	}
}
