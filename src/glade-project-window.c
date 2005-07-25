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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
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

#include "glade.h"
#include "glade-palette.h"
#include "glade-editor.h"
#include "glade-clipboard.h"
#include "glade-clipboard-view.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-project.h"
#include "glade-project-view.h"
#include "glade-project-window.h"
#include "glade-placeholder.h"
#include "glade-command.h"
#include "glade-debug.h"
#include "glade-utils.h"
#include "glade-app.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtkstock.h>


#define CONFIG_RECENT_PROJECTS "Recent Projects"
#define CONFIG_RECENT_PROJECTS_MAX "max_recent_projects"

struct _GladeProjectWindowPriv {
	/* Application widgets */
	GtkWidget *window; /* Main window */
	GtkWidget *main_vbox;

	GtkWidget *statusbar; /* A pointer to the status bar. */
	guint statusbar_menu_context_id; /* The context id of the menu bar */
	guint statusbar_actions_context_id; /* The context id of actions messages */
	

	GtkUIManager *ui;		/* The UIManager */
	GtkActionGroup *actions;	/* All the static actions */
	GtkActionGroup *p_actions;	/* Projects actions */
	GtkActionGroup *rp_actions;	/* Recent projects actions */

	GQueue *recent_projects;	/* A GtkAction queue */
	gint rp_max;			/* Maximun Recent Projects entries */

	GtkWidget *widget_tree;        /* The widget tree window*/
	GtkWindow *palette_window;     /* The window that will contain the palette */
	GtkWindow *editor_window;      /* The window that will contain the editor */
	GtkWidget *toolbar_undo;       /* undo item on the toolbar */
	GtkWidget *toolbar_redo;       /* redo item on the toolbar */
};

const gchar *WINDOW_TITLE = "Glade-3 GUI Builder";
const gint   GLADE_WIDGET_TREE_WIDTH      = 230;
const gint   GLADE_WIDGET_TREE_HEIGHT     = 300;
const gint   GLADE_PALETTE_DEFAULT_HEIGHT = 450;

static gpointer parent_class = NULL;

static void glade_project_window_refresh_undo_redo (GladeProjectWindow *gpw);

static void gpw_project_menuitem_add (GladeProjectWindow *gpw, GladeProject *project);

static void
gpw_refresh_title (GladeProjectWindow *gpw)
{
	GladeProject *active_project;
	gchar *title;

	active_project = glade_app_get_active_project (GLADE_APP (gpw));
	if (active_project)
		title = g_strdup_printf ("%s - %s", WINDOW_TITLE, active_project->name);
	else
		title = g_strdup_printf ("%s", WINDOW_TITLE);

	gtk_window_set_title (GTK_WINDOW (gpw->priv->window), title);
	g_free (title);
}

static void
gpw_select_project_menu (GladeProjectWindow *gpw)
{
	GladeProject *project;
	GList        *projects;
	GtkWidget    *palette_item;

	for (projects = glade_app_get_projects (GLADE_APP (gpw));
	     projects && projects->data; projects = projects->next)
	{
		project      = projects->data;
		palette_item = glade_project_get_menuitem (project);
		
		gtk_check_menu_item_set_draw_as_radio
			(GTK_CHECK_MENU_ITEM (palette_item), TRUE);
		gtk_check_menu_item_set_active 
			(GTK_CHECK_MENU_ITEM (palette_item), 
			 (glade_app_get_active_project 
			  (GLADE_APP (gpw)) == project));
	}
}

static void
gpw_refresh_project_entry (GladeProjectWindow *gpw, GladeProject *project)
{
	/* Remove menuitem and action */
	gtk_ui_manager_remove_ui(gpw->priv->ui,
				 glade_project_get_menuitem_merge_id(project));

	gtk_action_group_remove_action (gpw->priv->p_actions,
					GTK_ACTION (project->action));
	
	g_object_unref (G_OBJECT (project->action));

	/* Create project menuiten and action */
	gpw_project_menuitem_add (gpw, project);
	
	gpw_select_project_menu (gpw);
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

	gtk_action_group_remove_action (gpw->priv->rp_actions, action);
	
	g_queue_remove (gpw->priv->recent_projects, action);
	
	gtk_ui_manager_ensure_update (gpw->priv->ui);
}

static void
gpw_recent_project_open_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	gchar *path = (gchar *) g_object_get_data (G_OBJECT (action), "project_path");

	if (path == NULL) return;
	
	if (!glade_app_is_project_loaded (GLADE_APP (gpw), path))
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
	if (g_queue_find_custom(gpw->priv->recent_projects, project_path, gpw_rp_cmp))
		return;
	
	label = glade_util_duplicate_underscores (project_path);
	if (!label) return;
	
	action_name = g_strdup_printf ("open[%s]", project_path);
	/* We don't want '/'s in the menu path */
	glade_util_replace (action_name, '/', ' ');
	
	/* Add action */
	action = gtk_action_new (action_name, label, NULL, NULL);
	gtk_action_group_add_action_with_accel (gpw->priv->rp_actions, action, "");
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
					glade_app_get_config (GLADE_APP (gpw)),
					CONFIG_RECENT_PROJECTS,
					CONFIG_RECENT_PROJECTS_MAX, NULL);
	
	/* Some default value for recent projects maximum */
	if (gpw->priv->rp_max == 0) gpw->priv->rp_max = 5;
	
	for (i = 0; i < gpw->priv->rp_max; i++)
	{
		g_snprintf(key, 8, "%d", i);
		
		filename = g_key_file_get_string (glade_app_get_config (GLADE_APP (gpw)),
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
	GKeyFile *config = glade_app_get_config (GLADE_APP (gpw));
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
gpw_open_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	GtkWidget           *filechooser;
	gchar               *path = NULL;

	filechooser = glade_util_file_dialog_new (_("Open..."), GTK_WINDOW (gpw->priv->window),
						   GLADE_FILE_DIALOG_ACTION_OPEN);

	if (gtk_dialog_run (GTK_DIALOG(filechooser)) == GTK_RESPONSE_OK)
		path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filechooser));

	gtk_widget_destroy (filechooser);

	if (!path)
		return;

	/* dont allow more than one project with the same name to be
	 * opened simultainiously.
	 */
	if (glade_app_is_project_loaded (GLADE_APP (gpw), path))
	{
		gchar *base_path = g_path_get_basename (path);
		gchar *message = g_strdup_printf (_("A project named %s is already open"), base_path);

		glade_util_ui_warn (gpw->priv->window, message);

		g_free (message);
		g_free (base_path);
	} else {
		glade_project_window_open_project (gpw, path);
	}
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
		glade_app_config_save (GLADE_APP (gpw));
	}
	gtk_widget_destroy (dialog);
}

static void
gpw_save (GladeProjectWindow *gpw, GladeProject *project, const gchar *path)
{
	GError              *error = NULL;
	
	if (!glade_project_save (project, path, &error))
	{
		glade_util_ui_warn (gpw->priv->window, error->message);
		g_error_free (error);
		return;
	}
	
	gpw_recent_project_add (gpw, path);
	gpw_recent_project_config_save (gpw);
	glade_app_config_save (GLADE_APP (gpw));

	gpw_refresh_project_entry (gpw, project);
	gpw_refresh_title (gpw);
	glade_util_flash_message (gpw->priv->statusbar,
				  gpw->priv->statusbar_actions_context_id,
				  _("Project '%s' saved"), project->name);
}

static void
gpw_save_as (GladeProjectWindow *gpw, const gchar *dialog_title)
{
 	GladeProject *project;
 	GtkWidget *filechooser;
	gchar *path = NULL;
	gchar *real_path;
	gchar *ch;
	
	project = glade_app_get_active_project (GLADE_APP (gpw));
	
	filechooser = glade_util_file_dialog_new (dialog_title,
						  GTK_WINDOW (gpw->priv->window),
						  GLADE_FILE_DIALOG_ACTION_SAVE);
	
 	if (gtk_dialog_run (GTK_DIALOG(filechooser)) == GTK_RESPONSE_OK)
		path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filechooser));
	
 	gtk_widget_destroy (filechooser);
 
 	if (!path)
 		return;
	
	ch = strrchr (path, '/');
	if (!strchr (ch, '.')) 
		real_path = g_strconcat (path, ".glade", NULL);
	else 
		real_path = g_strdup (path);
	
	g_free (path);

	/* dont allow more than one project with the same name to be
	 * opened simultainiously (but allow the same project to change paths).
	 */
	if (glade_app_is_project_loaded (GLADE_APP (gpw), real_path) &&
	    glade_util_basenames_match  (project->path, real_path) == FALSE)
	{
		gchar *base_path = g_path_get_basename (path);
		gchar *message = g_strdup_printf (_("A project named %s is already open"), base_path);

		glade_util_ui_warn (gpw->priv->window, message);

		g_free (message);
		g_free (base_path);
		return;
	}

	gpw_save (gpw, project, real_path);

	g_free (real_path);
}

static void
gpw_save_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	GladeProject       *project;
	GError             *error = NULL;
	
	project = glade_app_get_active_project (GLADE_APP (gpw));

	if (project->path != NULL) 
	{
		if (glade_project_save (project, project->path, &error)) 
		{
			glade_util_flash_message 
				(gpw->priv->statusbar,
				 gpw->priv->statusbar_actions_context_id,
				 _("Project '%s' saved"),
				 project->name);
		} 
		else 
		{
			glade_util_ui_warn (gpw->priv->window, error->message);
			g_error_free (error);
		}
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
				glade_util_ui_warn (gpw->priv->window, error->message);
				g_error_free (error);
			}
		}
		else
		{
			GtkWidget *filechooser;
			gchar *path = NULL;

			filechooser = glade_util_file_dialog_new (_("Save ..."), GTK_WINDOW (gpw->priv->window),
								   GLADE_FILE_DIALOG_ACTION_SAVE);

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
	gtk_ui_manager_remove_ui (gpw->priv->ui,
				  glade_project_get_menuitem_merge_id (project));
	
	glade_app_remove_project (GLADE_APP (gpw), project);
	gpw_select_project_menu (gpw);
}

static void
gpw_close_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	GladeProject *project;
	gboolean close;
	
	project = glade_app_get_active_project (GLADE_APP (gpw));

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

	for (list = glade_app_get_projects (GLADE_APP (gpw)); list; list = list->next)
	{
		GladeProject *project = GLADE_PROJECT (list->data);

		if (project->changed)
		{
			gboolean quit = gpw_confirm_close_project (gpw, project);
			if (!quit)
				return;
		}
	}

	while (glade_app_get_projects (GLADE_APP (gpw))) {
		GladeProject *project = GLADE_PROJECT (glade_app_get_projects (GLADE_APP (gpw))->data);
		do_close (gpw, project);
	}

	gtk_main_quit ();
}

static void
gpw_copy_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	glade_app_command_copy (GLADE_APP (gpw));
}

static void
gpw_cut_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	glade_app_command_cut (GLADE_APP (gpw));
}

static void
gpw_paste_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	glade_app_command_paste (GLADE_APP (gpw));
}

static void
gpw_delete_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	if (!glade_app_get_active_project (GLADE_APP (gpw)))
	{
		g_warning ("delete should not be sensitive: we don't have a project");
		return;
	}
	glade_app_command_delete (GLADE_APP (gpw));
}

static void
gpw_undo_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	if (!glade_app_get_active_project (GLADE_APP (gpw)))
	{
		g_warning ("undo should not be sensitive: we don't have a project");
		return;
	}
	glade_app_command_undo (GLADE_APP (gpw));
}

static void
gpw_redo_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	if (!glade_app_get_active_project (GLADE_APP (gpw)))
	{
		g_warning ("redo should not be sensitive: we don't have a project");
		return;
	}
	glade_app_command_redo (GLADE_APP (gpw));
}

static gboolean
gpw_hide_palette_on_delete (GtkWidget *palette, gpointer not_used, GtkUIManager *ui)
{
	GtkWidget *palette_item;

	glade_util_hide_window (GTK_WINDOW (palette));

	palette_item = gtk_ui_manager_get_widget (ui, "/MenuBar/ViewMenu/Palette");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (palette_item), FALSE);

	/* Return true so that the palette is not destroyed */
	return TRUE;
}

static void
gpw_create_palette (GladeProjectWindow *gpw)
{
	GtkWidget *palette_item;

	g_return_if_fail (gpw != NULL);

	gpw->priv->palette_window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
	gtk_window_set_default_size (GTK_WINDOW (gpw->priv->palette_window), -1,
				     GLADE_PALETTE_DEFAULT_HEIGHT);

	gtk_window_set_title (gpw->priv->palette_window, _("Palette"));
	gtk_window_set_type_hint (gpw->priv->palette_window, GDK_WINDOW_TYPE_HINT_UTILITY);
	gtk_window_set_resizable (gpw->priv->palette_window, TRUE);
	gtk_window_move (gpw->priv->palette_window, 0, 250);

	gtk_container_add (GTK_CONTAINER (gpw->priv->palette_window), GTK_WIDGET (glade_app_get_palette(GLADE_APP (gpw))));

	/* Delete event, don't destroy it */
	g_signal_connect (G_OBJECT (gpw->priv->palette_window), "delete_event",
			  G_CALLBACK (gpw_hide_palette_on_delete), gpw->priv->ui);
	palette_item = gtk_ui_manager_get_widget (gpw->priv->ui, "/MenuBar/ViewMenu/Palette");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (palette_item), TRUE);
}

static void
gpw_show_palette (GladeProjectWindow *gpw)
{
	GtkWidget *palette_item;

	g_return_if_fail (gpw != NULL);

	if (gpw->priv->palette_window == NULL)
		gpw_create_palette (gpw);

	gtk_widget_show (GTK_WIDGET (gpw->priv->palette_window));

	palette_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						  "/MenuBar/ViewMenu/Palette");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (palette_item), TRUE);
}

static void
gpw_hide_palette (GladeProjectWindow *gpw)
{
	GtkWidget *palette_item;

	g_return_if_fail (gpw != NULL);

	glade_util_hide_window (gpw->priv->palette_window);

	palette_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						  "/MenuBar/ViewMenu/Palette");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (palette_item), FALSE);
}

static gboolean
gpw_hide_editor_on_delete (GtkWidget *editor, gpointer not_used,
		GtkUIManager *ui)
{
	GtkWidget *editor_item;

	glade_util_hide_window (GTK_WINDOW (editor));

	editor_item = gtk_ui_manager_get_widget (ui, "/MenuBar/ViewMenu/PropertyEditor");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (editor_item), FALSE);

	/* Return true so that the editor is not destroyed */
	return TRUE;
}

static void
gpw_create_editor (GladeProjectWindow *gpw)
{
	GtkWidget *editor_item;

	g_return_if_fail (gpw != NULL);

	gpw->priv->editor_window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
	gtk_window_set_default_size (GTK_WINDOW (gpw->priv->editor_window), 350, 450);

	gtk_window_set_title  (gpw->priv->editor_window, _("Properties"));
	gtk_window_move (gpw->priv->editor_window, 350, 0);

	gtk_container_add (GTK_CONTAINER (gpw->priv->editor_window), GTK_WIDGET (glade_app_get_editor (GLADE_APP(gpw))));

	/* Delete event, don't destroy it */
	g_signal_connect (G_OBJECT (gpw->priv->editor_window), "delete_event",
			  G_CALLBACK (gpw_hide_editor_on_delete), gpw->priv->ui);

	editor_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						 "/MenuBar/ViewMenu/PropertyEditor");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (editor_item), TRUE);
}

static void
gpw_show_editor (GladeApp *app, gboolean raise)
{
	GladeProjectWindow *gpw = GLADE_PROJECT_WINDOW (app);
	GtkWidget *editor_item;

	g_return_if_fail (GLADE_IS_PROJECT_WINDOW (gpw));

	gtk_widget_show (GTK_WIDGET (gpw->priv->editor_window));

	if (raise)
		gtk_window_present (gpw->priv->editor_window);

	editor_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						 "/MenuBar/ViewMenu/PropertyEditor");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (editor_item), TRUE);
}

static void
gpw_hide_editor (GladeApp *app)
{
	GladeProjectWindow *gpw = GLADE_PROJECT_WINDOW (app);
	GtkWidget *editor_item;

	g_return_if_fail (GLADE_IS_PROJECT_WINDOW (gpw));

	glade_util_hide_window (gpw->priv->editor_window);

	editor_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						 "/MenuBar/ViewMenu/PropertyEditor");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (editor_item), FALSE);
}

static gboolean
gpw_hide_widget_tree_on_delete (GtkWidget *widget_tree, gpointer not_used,
		GtkUIManager *ui)
{
	GtkWidget *widget_tree_item;

	glade_util_hide_window (GTK_WINDOW (widget_tree));

	widget_tree_item = gtk_ui_manager_get_widget (ui,"/MenuBar/ViewMenu/WidgetTree");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget_tree_item), FALSE);

	/* return true so that the widget tree is not destroyed */
	return TRUE;
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
	GtkWidget *hbox, *vbox, *expand, *collapse;
	GladeProjectView *view;

	view = glade_project_view_new (GLADE_PROJECT_VIEW_TREE);
	glade_app_add_project_view (GLADE_APP (gpw), view);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	/* Add control buttons to the treeview. */
	hbox     = gtk_hbox_new (TRUE, 0);
	vbox     = gtk_vbox_new (FALSE, 0);
	expand   = gtk_button_new_with_label (_("Expand all"));
	collapse = gtk_button_new_with_label (_("Collapse all"));
	gtk_box_pack_start (GTK_BOX (hbox), expand, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), collapse, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (view), TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (expand), "clicked",
			  G_CALLBACK (gpw_expand_treeview), 
			  view->tree_view);

	g_signal_connect_swapped (G_OBJECT (collapse), "clicked",
				  G_CALLBACK (gtk_tree_view_collapse_all), 
				  view->tree_view);

	return vbox;
}

static GtkWidget* 
gpw_create_widget_tree (GladeProjectWindow *gpw)
{
	GtkWidget *widget_tree;
	GtkWidget *widget_tree_item;
 
	widget_tree = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (widget_tree),
				     GLADE_WIDGET_TREE_WIDTH,
				     GLADE_WIDGET_TREE_HEIGHT);
 
	gtk_window_set_title (GTK_WINDOW (widget_tree), _("Widget Tree"));

	gtk_container_add (GTK_CONTAINER (widget_tree), 
			   gpw_create_widget_tree_contents (gpw));

	g_signal_connect (G_OBJECT (widget_tree), "delete_event",
			  G_CALLBACK (gpw_hide_widget_tree_on_delete), gpw->priv->ui);

	widget_tree_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						      "/MenuBar/ViewMenu/WidgetTree");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget_tree_item), TRUE);

	return widget_tree;
}

static void 
gpw_show_widget_tree (GladeProjectWindow *gpw) 
{
	GtkWidget *widget_tree_item;

	g_return_if_fail (gpw != NULL);

	if (gpw->priv->widget_tree == NULL)
		gpw->priv->widget_tree = gpw_create_widget_tree (gpw);

	gtk_widget_show_all (gpw->priv->widget_tree);

	widget_tree_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						      "/MenuBar/ViewMenu/WidgetTree");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget_tree_item), TRUE);
}

static void
gpw_hide_widget_tree (GladeProjectWindow *gpw)
{
	GtkWidget *widget_tree_item;

	g_return_if_fail (gpw != NULL);

	glade_util_hide_window (GTK_WINDOW (gpw->priv->widget_tree));

	widget_tree_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						      "/MenuBar/ViewMenu/WidgetTree");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget_tree_item), FALSE);

}

static gboolean
gpw_hide_clipboard_view_on_delete (GtkWidget *clipboard_view, gpointer not_used,
				   GtkUIManager *ui)
{
	GtkWidget *clipboard_item;

	glade_util_hide_window (GTK_WINDOW (clipboard_view));

	clipboard_item = gtk_ui_manager_get_widget (ui, "/MenuBar/ViewMenu/Clipboard");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (clipboard_item), FALSE);

	/* return true so that the clipboard view is not destroyed */
	return TRUE;
}

static void 
gpw_create_clipboard_view (GladeProjectWindow *gpw)
{
	GtkWidget *view;
	GtkWidget *clipboard_item;
	
	view = glade_app_get_clipboard_view (GLADE_APP (gpw));
	g_signal_connect (G_OBJECT (view), "delete_event",
			  G_CALLBACK (gpw_hide_clipboard_view_on_delete),
			  gpw->priv->ui);
	clipboard_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						    "/MenuBar/ViewMenu/Clipboard");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (clipboard_item), TRUE);
}

static void
gpw_show_clipboard_view (GladeProjectWindow *gpw)
{
	GtkWidget *clipboard_item;
	static gboolean created = FALSE;
	
	g_return_if_fail (gpw != NULL);

	if (!created)
	{
		gpw_create_clipboard_view (gpw);
		created = TRUE;
	}
	
	gtk_widget_show_all (GTK_WIDGET (glade_app_get_clipboard_view (GLADE_APP(gpw))));

	clipboard_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						    "/MenuBar/ViewMenu/Clipboard");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (clipboard_item), TRUE);
}

static void
gpw_hide_clipboard_view (GladeProjectWindow *gpw)
{
	GtkWidget *clipboard_item;

	g_return_if_fail (gpw != NULL);

	glade_util_hide_window (GTK_WINDOW (glade_app_get_clipboard_view (GLADE_APP(gpw))));

	clipboard_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						    "/MenuBar/ViewMenu/Clipboard");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (clipboard_item), FALSE);

}

static void
gpw_toggle_palette_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	GtkWidget *palette_item;
 
	palette_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						  "/MenuBar/ViewMenu/Palette");

	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (palette_item)))
		gpw_show_palette (gpw);
	else
		gpw_hide_palette (gpw);
}

static void
gpw_toggle_editor_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	GtkWidget *editor_item;

	editor_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						 "/MenuBar/ViewMenu/PropertyEditor");

	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (editor_item)))
		gpw_show_editor (GLADE_APP (gpw), FALSE);
	else
		gpw_hide_editor (GLADE_APP (gpw));
}

static void 
gpw_toggle_widget_tree_cb (GtkAction *action, GladeProjectWindow *gpw) 
{
	GtkWidget *widget_tree_item;

	widget_tree_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						      "/MenuBar/ViewMenu/WidgetTree");

	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget_tree_item)))
		gpw_show_widget_tree (gpw);
	else
		gpw_hide_widget_tree (gpw);
}

static void
gpw_toggle_clipboard_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	GtkWidget *clipboard_item;

	clipboard_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						    "/MenuBar/ViewMenu/Clipboard");

	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (clipboard_item)))
		gpw_show_clipboard_view (gpw);
	else
		gpw_hide_clipboard_view (gpw);
}

static void gpw_about_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	GtkWidget *about_dialog;
	GtkWidget *vbox;
	GtkWidget *glade_image;
	GtkWidget *version;
	GtkWidget *description;
	gchar *filename;

	about_dialog = gtk_dialog_new_with_buttons (_("About Glade"), GTK_WINDOW (gpw->priv->window), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						    GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
	gtk_window_set_title (GTK_WINDOW (about_dialog), _("About Glade"));
	gtk_dialog_set_has_separator (GTK_DIALOG (about_dialog), FALSE);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (about_dialog)->vbox), vbox, TRUE, TRUE, 0);

	filename = g_build_filename (glade_icon_dir, "glade-3.png", NULL);
	glade_image = gtk_image_new_from_file (filename);
	g_free (filename);
	gtk_box_pack_start (GTK_BOX (vbox), glade_image, TRUE, TRUE, 0);

	version = gtk_label_new (_("<span size=\"xx-large\"><b>Glade 3.0.0</b></span>"));
	gtk_box_pack_start (GTK_BOX (vbox), version, FALSE, FALSE, 0);
	gtk_label_set_use_markup (GTK_LABEL (version), TRUE);
	gtk_label_set_justify (GTK_LABEL (version), GTK_JUSTIFY_LEFT);

	description = gtk_label_new (_("Glade is a User Interface Builder for GTK+ and GNOME. \nThis version is a rewrite of the Glade 2.0.0 version, original from Damon Chaplin\n\n<small>(C) 2001-2004 Ximian, Inc.\n(C) 2003-2004 Joaquin Cuenca Abela, Paolo Borelli, et al.</small>"));
	gtk_box_pack_start (GTK_BOX (vbox), description, FALSE, FALSE, 4);
	gtk_label_set_use_markup (GTK_LABEL (description), TRUE);
	gtk_label_set_justify (GTK_LABEL (description), GTK_JUSTIFY_FILL);
	gtk_label_set_line_wrap (GTK_LABEL (description), TRUE);

	gtk_widget_show_all (about_dialog);
	gtk_dialog_run (GTK_DIALOG (about_dialog));
	gtk_widget_destroy (about_dialog);
}

static void
glade_project_window_set_project (GtkAction *action, GladeProject *project)
{
	GladeProjectWindow *gpw = g_object_get_data (G_OBJECT (project), "gpw");
	GtkWidget *item = glade_project_get_menuitem (project);
	
	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)))
	{
		glade_app_set_project (GLADE_APP (gpw), project);

		/* Ensure correct selection
		 */
		gpw_select_project_menu (gpw);
	}
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
"      <menuitem action='Palette'/>\n"
"      <menuitem action='PropertyEditor'/>\n"
"      <menuitem action='WidgetTree'/>\n"
"      <menuitem action='Clipboard'/>\n"
"    </menu>\n"
"    <menu action='ProjectMenu'>\n"
"    </menu>\n"
"    <menu action='HelpMenu'>\n"
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

static GtkActionEntry entries[] = {
	{ "FileMenu", NULL, "_File" },
	{ "EditMenu", NULL, "Edit" },
	{ "ViewMenu", NULL, "View" },
	{ "ProjectMenu", NULL, "Project" },
	{ "HelpMenu", NULL, "_Help" },
	
	/* FileMenu */
	{ "New", GTK_STOCK_NEW, "_New", "<control>N",
	"Create a new project file", G_CALLBACK (gpw_new_cb) },
	
	{ "Open", GTK_STOCK_OPEN, "_Open","<control>O",
	"Open a project file", G_CALLBACK (gpw_open_cb) },
	
	{ "Recents", NULL, "Recent Projects", NULL, NULL },
	
	{ "ClearRecents", GTK_STOCK_CLEAR, "Clear Recent Projects", NULL,
	NULL, G_CALLBACK (gpw_recent_project_clear_cb) },
	
	{ "Save", GTK_STOCK_SAVE, "_Save","<control>S",
	"Save the current project file", G_CALLBACK (gpw_save_cb) },
	
	{ "SaveAs", GTK_STOCK_SAVE_AS, "Save _As...", NULL,
	"Save the current project file with a different name", G_CALLBACK (gpw_save_as_cb) },
	
	{ "Close", GTK_STOCK_CLOSE, "_Close", "<control>W",
	"Close the current project file", G_CALLBACK (gpw_close_cb) },
	
	{ "Quit", GTK_STOCK_QUIT, "_Quit", "<control>Q",
	"Quit the program", G_CALLBACK (gpw_quit_cb) },

	/* EditMenu */	
	{ "Undo", GTK_STOCK_UNDO, "_Undo", "<control>Z",
	"Undo the last action",	G_CALLBACK (gpw_undo_cb) },
	
	{ "Redo", GTK_STOCK_REDO, "_Redo", "<control>R",
	"Redo the last action",	G_CALLBACK (gpw_redo_cb) },
	
	{ "Cut", GTK_STOCK_CUT, "C_ut", NULL,
	"Cut the selection", G_CALLBACK (gpw_cut_cb) },
	
	{ "Copy", GTK_STOCK_COPY, "_Copy", NULL,
	"Copy the selection", G_CALLBACK (gpw_copy_cb) },
	
	{ "Paste", GTK_STOCK_PASTE, "_Paste", NULL,
	"Paste the clipboard", G_CALLBACK (gpw_paste_cb) },
	
	{ "Delete", GTK_STOCK_DELETE, "_Delete", "Delete",
	"Delete the selection", G_CALLBACK (gpw_delete_cb) },
	
	/* HelpMenu */
	{ "About", NULL, "_About", NULL,
	"Shows the About Dialog", G_CALLBACK (gpw_about_cb) }
};
static guint n_entries = G_N_ELEMENTS (entries);

static GtkToggleActionEntry view_entries[] = {
	/* ViewMenu */
	{ "Palette", NULL, "_Palette", NULL,
	"Change the visibility of the palette of widgets",
	G_CALLBACK (gpw_toggle_palette_cb), TRUE },

	{ "PropertyEditor", NULL, "Property _Editor", NULL,
	"Change the visibility of the property editor",
	G_CALLBACK (gpw_toggle_editor_cb), TRUE },

	{ "WidgetTree", NULL, "_Widget Tree", NULL,
	"Change the visibility of the project widget tree",
	G_CALLBACK (gpw_toggle_widget_tree_cb), FALSE },

	{ "Clipboard", NULL, "_Clipboard", NULL,
	"Change the visibility of the clipboard",
	G_CALLBACK (gpw_toggle_clipboard_cb), FALSE }
};
static guint n_view_entries = G_N_ELEMENTS (view_entries);

static void
gpw_item_selected_cb (GtkItem *item, GladeProjectWindow *gpw)
{
	gchar *tip = g_object_get_data(G_OBJECT(item), "tooltip");
	
	if (tip != NULL)
		gtk_statusbar_push (GTK_STATUSBAR (gpw->priv->statusbar),
				    gpw->priv->statusbar_menu_context_id, tip);
}

static void
gpw_item_deselected_cb (GtkItem *item, GladeProjectWindow *gpw)
{
	gchar *tip = g_object_get_data(G_OBJECT(item), "tooltip");
	
	if (tip != NULL)
		gtk_statusbar_pop (GTK_STATUSBAR (gpw->priv->statusbar),
				   gpw->priv->statusbar_menu_context_id);
}

static void
gpw_ui_connect_proxy_cb (GtkUIManager *ui,
		     GtkAction *action,
		     GtkWidget *widget,
		     GladeProjectWindow *gpw)
{
	gchar *tip;
	
	if (!GTK_IS_MENU_ITEM(widget)) return;

	g_object_get(G_OBJECT(action), "tooltip", &tip, NULL);
	if (tip == NULL) return;
	
	g_object_set_data(G_OBJECT(widget), "tooltip", tip);
	
	g_signal_connect(G_OBJECT(widget), "select", (GCallback)gpw_item_selected_cb, gpw);
	g_signal_connect(G_OBJECT(widget), "deselect", (GCallback)gpw_item_deselected_cb, gpw);
}

static GtkWidget *
gpw_construct_menu (GladeProjectWindow *gpw)
{
	GError *error = NULL;
	
	gpw->priv->actions = gtk_action_group_new ("actions");
	gtk_action_group_add_actions (gpw->priv->actions, entries, n_entries, gpw);
	gtk_action_group_add_toggle_actions (gpw->priv->actions, view_entries,
						n_view_entries, gpw);

	gpw->priv->p_actions = gtk_action_group_new ("p_actions");

	gpw->priv->rp_actions = gtk_action_group_new ("rp_actions");
	
	gpw->priv->ui = gtk_ui_manager_new ();
	g_signal_connect(G_OBJECT(gpw->priv->ui), "connect-proxy",
			 (GCallback)gpw_ui_connect_proxy_cb, gpw);

	gtk_ui_manager_insert_action_group (gpw->priv->ui, gpw->priv->actions, 0);
	gtk_ui_manager_insert_action_group (gpw->priv->ui, gpw->priv->p_actions, 1);
	gtk_ui_manager_insert_action_group (gpw->priv->ui, gpw->priv->rp_actions, 2);
	
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
	gchar *filename;

	app = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_move (GTK_WINDOW (app), 0, 0);
	gtk_window_set_default_size (GTK_WINDOW (app), 280, 220);
	filename = g_build_filename (glade_icon_dir, "glade-3.png", NULL);
	gtk_window_set_default_icon_from_file (filename, NULL);
	g_free (filename);
	gpw->priv->window = app;

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (app), vbox);
	gpw->priv->main_vbox = vbox;

	menubar = gpw_construct_menu (gpw);
	toolbar = gtk_ui_manager_get_widget (gpw->priv->ui, "/ToolBar");
	project_view = gpw_create_widget_tree_contents (gpw);
	statusbar = gpw_construct_statusbar (gpw);

	gpw->priv->toolbar_undo = gtk_ui_manager_get_widget (gpw->priv->ui, "/ToolBar/Undo");
	gpw->priv->toolbar_redo = gtk_ui_manager_get_widget (gpw->priv->ui, "/ToolBar/Redo");

	gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), project_view, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (vbox), statusbar, FALSE, TRUE, 0);

	glade_project_window_refresh_undo_redo (gpw);

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
gpw_project_menuitem_add (GladeProjectWindow *gpw, GladeProject *project)
{
	gchar *underscored_name, *action_name;
	guint merge_id;
	
	/* double the underscores in the project name
	 * (or they will just underscore the next character) */
	underscored_name = glade_util_duplicate_underscores (project->name);
	if (!underscored_name)
		return;

	action_name = g_strdup_printf ("select[%s]", project->name);
	
	/* What happen if there is two project with the same name in differents
	 * directories?
	 */
	project->action = gtk_toggle_action_new (action_name, underscored_name,
					"Selects this Project",	NULL);
	gtk_toggle_action_set_active (project->action, TRUE);
	g_signal_connect (G_OBJECT (project->action), "activate",
			  (GCallback) glade_project_window_set_project, project);
	
	gtk_action_group_add_action_with_accel (gpw->priv->p_actions,
						GTK_ACTION (project->action), "");
	
	/* Add menuitem to menu */
	merge_id = gtk_ui_manager_new_merge_id(gpw->priv->ui);
	
	gtk_ui_manager_add_ui(gpw->priv->ui, merge_id,
			      "/MenuBar/ProjectMenu", action_name, action_name,
			      GTK_UI_MANAGER_MENUITEM, FALSE);

	/* Set extra data to action */
	g_object_set_data (G_OBJECT (project->action), "ui", gpw->priv->ui);
	g_object_set_data_full (G_OBJECT (project->action), "menuitem_path",
				g_strdup_printf ("/MenuBar/ProjectMenu/%s", action_name),
				g_free);
	g_object_set_data (G_OBJECT (project->action), "merge_id", GINT_TO_POINTER (merge_id));

	g_free (action_name);
	g_free (underscored_name);
}

static void
glade_project_window_add_project (GladeProjectWindow *gpw, GladeProject *project)
{
 	g_return_if_fail (GLADE_IS_PROJECT (project));
	
	g_object_set_data (G_OBJECT (project), "gpw", gpw);
	
	glade_app_add_project (GLADE_APP (gpw), project);

	gpw_project_menuitem_add (gpw, project);

	/* Ensure correct selection
	 */
	gpw_select_project_menu (gpw);
}

/**
 * glade_project_window_new_project:
 *
 * Creates a new #GladeProject and adds it to the #GladeProjectWindow.
 */
void
glade_project_window_new_project (GladeProjectWindow *gpw)
{
	GladeProject *project;

	project = glade_project_new (TRUE);
	if (!project)
	{
		glade_util_ui_warn (gpw->priv->window, _("Could not create a new project."));
		return;
	}
	glade_project_window_add_project (gpw, project);
}

/**
 * glade_project_window_open_project:
 * @path: a string containing a filename
 *
 * Opens the project specified by @path and adds it to the #GladeProjectWindow.
 */
void
glade_project_window_open_project (GladeProjectWindow *gpw, const gchar *path)
{
	GladeProject *project;

	g_return_if_fail (path != NULL);

	/* dont allow more than one project with the same name to be
	 * opened simultainiously.
	 */
	if (glade_app_is_project_loaded (GLADE_APP (gpw), (gchar*)path))
		return;

	project = glade_project_open (path);
	if (!project)
	{
		glade_util_ui_warn (gpw->priv->window, _("Could not open project."));
		return;
	}

	gpw_recent_project_add (gpw, path);
	gpw_recent_project_config_save (gpw);
	glade_app_config_save (GLADE_APP (gpw));

	glade_project_window_add_project (gpw, project);
}

/**
 * glade_project_window_change_menu_label:
 * @gpw:
 * @path:
 * @prefix:
 * @suffix:
 *
 * TODO: write me
 */
void
glade_project_window_change_menu_label (GladeProjectWindow *gpw,
					const gchar *path,
					const gchar *prefix,
					const gchar *suffix)
{
	gboolean sensitive = TRUE;
	GtkBin *bin;
	GtkLabel *label;
	gchar *text;
	
	bin = GTK_BIN (gtk_ui_manager_get_widget (gpw->priv->ui, path));
	label = GTK_LABEL (gtk_bin_get_child (bin));
	
	if (prefix == NULL)
	{
		gtk_label_set_text_with_mnemonic (label, suffix);
                return;
	}

	if (suffix == NULL)
	{
		suffix = _("Nothing");
		sensitive = FALSE;
	}

	text = g_strconcat (prefix, suffix, NULL);

	gtk_label_set_text_with_mnemonic (label, text);
	gtk_widget_set_sensitive (GTK_WIDGET (bin), sensitive);

	g_free (text);
}

/**
 * glade_project_window_refresh_undo_redo:
 *
 * TODO: write me
 */
void
glade_project_window_refresh_undo_redo (GladeProjectWindow *gpw)
{
	GList *prev_redo_item;
	GList *undo_item;
	GList *redo_item;
	const gchar *undo_description = NULL;
	const gchar *redo_description = NULL;
	GladeProject *project;

	project = glade_app_get_active_project (GLADE_APP (gpw));
	if (!project)
	{
		undo_item = NULL;
		redo_item = NULL;
	}
	else
	{
		undo_item = prev_redo_item = project->prev_redo_item;
		redo_item = (prev_redo_item == NULL) ? project->undo_stack : prev_redo_item->next;

		if (undo_item && undo_item->data)
			undo_description = GLADE_COMMAND (undo_item->data)->description;
		if (redo_item && redo_item->data)
			redo_description = GLADE_COMMAND (redo_item->data)->description;
	}

	glade_project_window_change_menu_label (gpw, "/MenuBar/EditMenu/Undo", _("_Undo: "), undo_description);
	glade_project_window_change_menu_label (gpw, "/MenuBar/EditMenu/Redo", _("_Redo: "), redo_description);

	gtk_widget_set_sensitive (gpw->priv->toolbar_undo, undo_description != NULL);
	gtk_widget_set_sensitive (gpw->priv->toolbar_redo, redo_description != NULL);
}

static void
glade_project_window_update_ui (GladeApp *app)
{
	GladeProjectWindow *gpw = GLADE_PROJECT_WINDOW (app);
	gpw_refresh_title (gpw);
	glade_project_window_refresh_undo_redo (gpw);
}

/**
 * glade_project_window_show_all:
 *
 * TODO: write me
 */
void
glade_project_window_show_all (GladeProjectWindow *gpw)
{
	gtk_widget_show_all (gpw->priv->window);
	gpw_show_palette (gpw);
	gpw_show_editor (GLADE_APP (gpw), FALSE);
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

/**
 * glade_project_window_new:
 * @catalogs:
 *
 * TODO: write me
 *
 * Returns:
 */
GladeProjectWindow *
glade_project_window_new (void)
{
	GladeProjectWindow *gpw;
	
	gpw = g_object_new (GLADE_TYPE_PROJECT_WINDOW, NULL);

	glade_project_window_create (gpw);
	gpw_create_palette (gpw);
	gpw_create_editor  (gpw);

	glade_app_set_window (GLADE_APP (gpw), gpw->priv->window);
	return gpw;
}
