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

#include "glade-project-window.h"

#include <gladeui/glade.h>
#include <gladeui/glade-design-view.h>
#include <gladeui/glade-binding.h>

#include <string.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkstock.h>

#define GLADE_ACTION_GROUP_STATIC "GladeStatic"
#define GLADE_ACTION_GROUP_PROJECT "GladeProject"
#define GLADE_ACTION_GROUP_PROJECTS_LIST_MENU "GladeProjectsList"

#define READONLY_INDICATOR (_("[Read Only]"))

#define GLADE_URL_USER_MANUAL      "http://glade.gnome.org/manual/index.html"
#define GLADE_URL_DEVELOPER_MANUAL "http://glade.gnome.org/docs/index.html"

#define GLADE_PROJECT_WINDOW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object),  \
					         GLADE_TYPE_PROJECT_WINDOW,              \
					         GladeProjectWindowPrivate))

struct _GladeProjectWindowPrivate
{
	/* Application widgets */
	GtkWidget *window; /* Main window */
	GtkWidget *main_vbox;
	
	GtkWidget *notebook;
	GladeDesignView *active_view;
	gint num_tabs;

	GtkWidget *statusbar; /* A pointer to the status bar. */
	guint statusbar_menu_context_id; /* The context id of the menu bar */
	guint statusbar_actions_context_id; /* The context id of actions messages */

	GtkUIManager *ui;		/* The UIManager */
	guint projects_list_menu_ui_id; /* Merge id for projects list menu */

	GtkActionGroup *static_actions;	/* All the static actions */
	GtkActionGroup *project_actions;/* All the project actions */
	GtkActionGroup *projects_list_menu_actions;/* Projects list menu actions */
	
	GtkLabel *label; /* the title of property editor dock */
	
	GtkRecentManager *recent_manager;
	GtkWidget *recent_menu;

	gchar *default_path; /* the default path for open/save operations */
	
	GtkToggleToolButton *selector_button; /* the widget selector button (replaces the one in the palette) */
	
};

static GladeAppClass *parent_class = NULL;

static void gpw_refresh_undo_redo (GladeProjectWindow *gpw);

static void gpw_recent_chooser_item_activated_cb (GtkRecentChooser *chooser, GladeProjectWindow *gpw);


G_DEFINE_TYPE(GladeProjectWindow, glade_project_window, GLADE_TYPE_APP)

static void
about_dialog_activate_link_func (GtkAboutDialog *dialog, const gchar *link, GladeProjectWindow *gpw)
{
	GtkWidget *warning_dialog;
	gboolean retval;
	
	retval = glade_util_url_show (link);
	
	if (!retval)
	{
		warning_dialog = gtk_message_dialog_new (GTK_WINDOW (gpw->priv->window),
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_OK,
						 _("Could not display the URL '%s'"),
						 link);
						 
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (warning_dialog),
							  _("No suitable web browser executable could be found."));
						 	
		gtk_window_set_title (GTK_WINDOW (warning_dialog), "");

		g_signal_connect_swapped (warning_dialog, "response",
					  G_CALLBACK (gtk_widget_destroy),
					  dialog);

		gtk_widget_show (warning_dialog);
	}	

}

/* locates the help file "glade.xml" with respect to current locale */
static gchar*
locate_help_file ()
{
	const gchar* const* locales = g_get_language_names ();

	/* check if user manual has been installed, if not, GLADE_GNOMEHELPDIR is empty */
	if (strlen (GLADE_GNOMEHELPDIR) == 0)
		return NULL;

	for (; *locales; locales++)
	{
		gchar *path;
		
		path = g_build_path (G_DIR_SEPARATOR_S, GLADE_GNOMEHELPDIR, "glade", *locales, "glade.xml", NULL);

		if (g_file_test (path, G_FILE_TEST_EXISTS))
		{
			return (path);
		}
		g_free (path);
	}
	
	return NULL;
}

static gboolean
glade_project_window_help_show (const gchar *link_id)
{
	gchar *file, *url, *command;
	gboolean retval;
	gint exit_status = -1;
	GError *error = NULL; 	

	file = locate_help_file ();
	if (file == NULL)
		return FALSE;

	if (link_id != NULL) {
		url = g_strconcat ("ghelp://", file, "?", link_id, NULL);
	} else {
		url = g_strconcat ("ghelp://", file, NULL);	
	}

	command = g_strconcat ("gnome-open ", url, NULL);
			
	retval = g_spawn_command_line_sync (command,
					    NULL,
					    NULL,
					    &exit_status,
					    &error);

	if (!retval) {
		g_error_free (error);
		error = NULL;
	}		
	
	g_free (command);
	g_free (file);
	g_free (url);
	
	return retval && !exit_status;
}

static void
gpw_refresh_title (GladeProjectWindow *gpw)
{
	GladeProject *project;
	gchar *title, *name = NULL;

	if (gpw->priv->active_view)
	{
		project = glade_design_view_get_project (gpw->priv->active_view);

		name = glade_project_display_name (project, TRUE, FALSE, FALSE);
		
		if (project->readonly != FALSE)
			title = g_strdup_printf ("%s %s", name, READONLY_INDICATOR);
		else
			title = g_strdup_printf ("%s", name);
		
		g_free (name);
	}
	else
	{
		title = g_strdup (_("User Interface Designer"));
	}
	
	gtk_window_set_title (GTK_WINDOW (gpw->priv->window), title);

	g_free (title);
}

static const gchar*
gpw_get_default_path (GladeProjectWindow *gpw)
{
	return gpw->priv->default_path;
}

static void
update_default_path (GladeProjectWindow *gpw, GladeProject *project)
{
	gchar *path;
	
	g_return_if_fail (project->path != NULL);

	path = g_path_get_dirname (project->path);

	g_free (gpw->priv->default_path);
	gpw->priv->default_path = g_strdup (path);

	g_free (path);
}

static gboolean
gpw_window_state_event_cb (GtkWidget *widget,
			   GdkEventWindowState *event,
			   GladeProjectWindow *gpw)
{
	if (event->changed_mask &
	    (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN))
	{
		gboolean show;

		show = !(event->new_window_state &
			(GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN));

		gtk_statusbar_set_has_resize_grip (GTK_STATUSBAR (gpw->priv->statusbar), show);
	}

	return FALSE;
}

static GtkWidget *
create_recent_chooser_menu (GladeProjectWindow *gpw, GtkRecentManager *manager)
{
	GtkWidget *recent_menu;
	GtkRecentFilter *filter;

	recent_menu = gtk_recent_chooser_menu_new_for_manager (manager);

	gtk_recent_chooser_set_local_only (GTK_RECENT_CHOOSER (recent_menu), TRUE);
	gtk_recent_chooser_set_show_icons (GTK_RECENT_CHOOSER (recent_menu), FALSE);
	gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER (recent_menu), GTK_RECENT_SORT_MRU);
	gtk_recent_chooser_menu_set_show_numbers (GTK_RECENT_CHOOSER_MENU (recent_menu), TRUE);

	filter = gtk_recent_filter_new ();
	gtk_recent_filter_add_application (filter, g_get_application_name());
	gtk_recent_chooser_set_filter (GTK_RECENT_CHOOSER (recent_menu), filter);

	return recent_menu;
}

static void
gpw_window_screen_changed_cb (GtkWidget *widget,
			      GdkScreen *old_screen,
			      GladeProjectWindow *gpw)
{
	GtkWidget *menu_item;
	GdkScreen *screen;

	screen = gtk_widget_get_screen (widget);	

	gpw->priv->recent_manager = gtk_recent_manager_get_for_screen (screen);

	gtk_menu_detach (GTK_MENU (gpw->priv->recent_menu));
	g_object_unref (G_OBJECT (gpw->priv->recent_menu));
	
	gpw->priv->recent_menu = create_recent_chooser_menu (gpw, gpw->priv->recent_manager);

	g_signal_connect (gpw->priv->recent_menu,
			  "item-activated",
			  G_CALLBACK (gpw_recent_chooser_item_activated_cb),
			  gpw);
			  
	menu_item = gtk_ui_manager_get_widget (gpw->priv->ui, "/MenuBar/FileMenu/OpenRecent");
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item), gpw->priv->recent_menu);
}

static void
project_selection_changed_cb (GladeProject *project, GladeProjectWindow *gpw)
{
	GladeWidget *glade_widget = NULL;
	GtkLabel *label;
	GList *list;
	gchar *text;
	gint num;

	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GLADE_IS_PROJECT_WINDOW (gpw));

	label = gpw->priv->label;

	/* Only update the editor if the selection has changed on
	 * the currently active project.
	 */
	if (glade_app_get_editor() &&
	    (project == glade_app_get_project ()))
	{
		list = glade_project_selection_get (project);
		num = g_list_length (list);
		
		if (num == 1 && !GLADE_IS_PLACEHOLDER (list->data))
		{
		
			glade_widget = glade_widget_get_from_gobject (G_OBJECT (list->data));
			
			text = g_strdup_printf ("%s [%s] - Properties",
						glade_widget_get_name (glade_widget),
						G_OBJECT_TYPE_NAME (glade_widget->object));
					
			gtk_label_set_text (label, text);
			
			g_free (text);
		}	
		else
		{
			gtk_label_set_text (label, "Properties");
		}
			
	}
	
}

static GladeDesignView *
glade_project_window_get_active_view (GladeProjectWindow *gpw)
{
	g_return_val_if_fail (GLADE_IS_PROJECT_WINDOW (gpw), NULL);

	return gpw->priv->active_view;
}

static void
gpw_refresh_projects_list_item (GladeProjectWindow *gpw, GladeProject *project)
{
	GtkAction *action;
	gchar *project_name;
	gchar *tooltip = NULL;
	
	/* Get associated action */
	action = GTK_ACTION (g_object_get_data (G_OBJECT (project), "project-list-action"));

	/* Set action label */
	project_name = glade_project_display_name (project, TRUE, FALSE, TRUE);
	g_object_set (action, "label", project_name, NULL);

	/* Set action tooltip */
	if (project->readonly != FALSE && project->path)
		tooltip = g_strdup_printf ("%s %s", project->path, READONLY_INDICATOR);
	else if (project->path)
		tooltip = g_strdup_printf ("%s", project->path);
	
	g_object_set (action, "tooltip", tooltip, NULL);
	
	g_free (tooltip);
	g_free (project_name);
}

static void
gpw_refresh_next_prev_project_sensitivity (GladeProjectWindow *gpw)
{
	GladeDesignView  *view;
	GtkAction *action;
	gint view_number;
	
	view = glade_project_window_get_active_view (gpw);

	if (view != NULL)
	{
		view_number = gtk_notebook_page_num (GTK_NOTEBOOK (gpw->priv->notebook), GTK_WIDGET (view));
		g_return_if_fail (view_number >= 0);
	
		action = gtk_action_group_get_action (gpw->priv->project_actions, "PreviousProject");
		gtk_action_set_sensitive (action, view_number != 0);
	
		action = gtk_action_group_get_action (gpw->priv->project_actions, "NextProject");
		gtk_action_set_sensitive (action, 
				 	 view_number < gtk_notebook_get_n_pages (GTK_NOTEBOOK (gpw->priv->notebook)) - 1);
	}
	else
	{
		action = gtk_action_group_get_action (gpw->priv->project_actions, "PreviousProject");
		gtk_action_set_sensitive (action, FALSE);
		
		action = gtk_action_group_get_action (gpw->priv->project_actions, "NextProject");
		gtk_action_set_sensitive (action, FALSE);
	}
}

static void
gpw_new_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	glade_project_window_new_project (gpw);
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

static GtkWidget*
gpw_construct_dock_item (GladeProjectWindow *gpw, const gchar *title, GtkWidget *child)
{
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *alignment;

	vbox = gtk_vbox_new (FALSE, 0);

	label = gtk_label_new (title);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 2, 5);

	alignment = gtk_alignment_new (0, 0, 1, 1);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 0, 12);
	
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (alignment), label);
	
	gtk_box_pack_start (GTK_BOX (vbox), child, TRUE, TRUE, 0);

	gtk_widget_show (alignment);
	gtk_widget_show (label);
	gtk_widget_show (child);
	gtk_widget_show (vbox);
	
	/* FIXME: naughty */
	g_object_set_data (G_OBJECT (vbox), "dock-label", label);

	return vbox;
}

void
on_palette_toggled (GladePalette *palette, GladeProjectWindow *gpw)
{
	if (glade_palette_get_current_item (palette))
		gtk_toggle_tool_button_set_active (gpw->priv->selector_button, FALSE);	
	else
		gtk_toggle_tool_button_set_active (gpw->priv->selector_button, TRUE);			
}

void
on_selector_button_toggled (GtkToggleToolButton *button, GladeProjectWindow *gpw)
{
	if (gtk_toggle_tool_button_get_active (gpw->priv->selector_button))
		glade_palette_deselect_current_item (glade_app_get_palette(), FALSE);
	else if (glade_palette_get_current_item (glade_app_get_palette()) == FALSE)
		gtk_toggle_tool_button_set_active (gpw->priv->selector_button, TRUE);
}

static void
gpw_set_sensitivity_according_to_project (GladeProjectWindow *window, GladeProject *project)
{
	GtkAction *action;

	action = gtk_action_group_get_action (window->priv->project_actions, "Save");
	gtk_action_set_sensitive (action,
			          !glade_project_get_readonly (project));

	action = gtk_action_group_get_action (window->priv->project_actions, "Cut");
	gtk_action_set_sensitive (action,
				  glade_project_get_has_selection (project));

	action = gtk_action_group_get_action (window->priv->project_actions, "Copy");
	gtk_action_set_sensitive (action,
				  glade_project_get_has_selection (project));

	action = gtk_action_group_get_action (window->priv->project_actions, "Paste");
	gtk_action_set_sensitive (action,
				  glade_clipboard_get_has_selection
					(glade_app_get_clipboard ()));

	action = gtk_action_group_get_action (window->priv->project_actions, "Delete");
	gtk_action_set_sensitive (action,
				  glade_project_get_has_selection (project));

	gpw_refresh_next_prev_project_sensitivity (window);

}

static void
gpw_projects_list_menu_activate_cb (GtkAction *action, GladeProjectWindow *window)
{
	gint n;

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)) == FALSE)
		return;

	n = gtk_radio_action_get_current_value (GTK_RADIO_ACTION (action));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (window->priv->notebook), n);
}

static void
gpw_refresh_projects_list_menu (GladeProjectWindow *window)
{
	GladeProjectWindowPrivate *p = window->priv;
	GList *actions, *l;
	GSList *group = NULL;
	gint n, i;
	guint id;

	if (p->projects_list_menu_ui_id != 0)
		gtk_ui_manager_remove_ui (p->ui, p->projects_list_menu_ui_id);

	/* Remove all current actions */
	actions = gtk_action_group_list_actions (p->projects_list_menu_actions);
	for (l = actions; l != NULL; l = l->next)
	{
		g_signal_handlers_disconnect_by_func (GTK_ACTION (l->data),
						      G_CALLBACK (gpw_projects_list_menu_activate_cb), window);
 		gtk_action_group_remove_action (p->projects_list_menu_actions, GTK_ACTION (l->data));
	}
	g_list_free (actions);

	n = gtk_notebook_get_n_pages (GTK_NOTEBOOK (p->notebook));

	id = (n > 0) ? gtk_ui_manager_new_merge_id (p->ui) : 0;

	/* Add an action for each project */
	for (i = 0; i < n; i++)
	{
		GtkWidget *view;
		GladeProject *project;
		GtkRadioAction *action;
		gchar *action_name;
		gchar *project_name;
		gchar *name;
		gchar *tooltip;
		gchar *accel;

		view = gtk_notebook_get_nth_page (GTK_NOTEBOOK (p->notebook), i);
		project = glade_design_view_get_project (GLADE_DESIGN_VIEW (view));


		/* NOTE: the action is associated to the position of the tab in
		 * the notebook not to the tab itself! This is needed to work
		 * around the gtk+ bug #170727: gtk leaves around the accels
		 * of the action. Since the accel depends on the tab position
		 * the problem is worked around, action with the same name always
		 * get the same accel.
		 */
		action_name = g_strdup_printf ("Tab_%d", i);
		project_name = glade_project_display_name (project, TRUE, FALSE, FALSE);
		name = glade_util_duplicate_underscores (project_name);
		tooltip =  g_strdup_printf (_("Activate %s"), project_name);

		/* alt + 1, 2, 3... 0 to switch to the first ten tabs */
		accel = (i < 10) ? g_strdup_printf ("<alt>%d", (i + 1) % 10) : NULL;

		action = gtk_radio_action_new (action_name, 
					       name,
					       tooltip, 
					       NULL,
					       i);

		/* Link action and project */
		g_object_set_data (G_OBJECT (project), "project-list-action", action);
		g_object_set_data (G_OBJECT (action), "project", project);

		if (group != NULL)
			gtk_radio_action_set_group (action, group);

		/* note that group changes each time we add an action, so it must be updated */
		group = gtk_radio_action_get_group (action);

		gtk_action_group_add_action_with_accel (p->projects_list_menu_actions,
							GTK_ACTION (action),
							accel);

		g_signal_connect (action, "activate",
				  G_CALLBACK (gpw_projects_list_menu_activate_cb),
				  window);

		gtk_ui_manager_add_ui (p->ui, id,
				       "/MenuBar/ProjectMenu/ProjectsListPlaceholder",
				       action_name, action_name,
				       GTK_UI_MANAGER_MENUITEM,
				       FALSE);

		if (GLADE_DESIGN_VIEW (view) == p->active_view)
			gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

		g_object_unref (action);

		g_free (action_name);
		g_free (project_name);
		g_free (name);
		g_free (tooltip);
		g_free (accel);
	}

	p->projects_list_menu_ui_id = id;
}


void
gpw_recent_add (GladeProjectWindow *gpw, const gchar *path)
{
	GtkRecentData *recent_data;
	gchar *uri;
	GError *error = NULL;

	uri = g_filename_to_uri (path, NULL, &error);
	if (error)
	{	
		g_warning ("Could not convert uri \"%s\" to a local path: %s", uri, error->message);
		g_error_free (error);
		return;
	}

	recent_data = g_slice_new (GtkRecentData);

	recent_data->display_name   = NULL;
	recent_data->description    = NULL;
	recent_data->mime_type      = "application/x-glade";
	recent_data->app_name       = (gchar *) g_get_application_name ();
	recent_data->app_exec       = g_strjoin (" ", g_get_prgname (), "%u", NULL);
	recent_data->groups         = NULL;
	recent_data->is_private     = FALSE;

	if (!gtk_recent_manager_add_full (gpw->priv->recent_manager,
				          uri,
				          recent_data))
	{
      		g_warning ("Unable to add '%s' to the list of recently used documents", uri);
	}

	g_free (uri);
	g_free (recent_data->app_exec);
	g_slice_free (GtkRecentData, recent_data);

}

void
gpw_recent_remove (GladeProjectWindow *gpw, const gchar *path)
{
	gchar *uri;
	GError *error = NULL;

	uri = g_filename_to_uri (path, NULL, &error);
	if (error)
	{	
		g_warning ("Could not convert uri \"%s\" to a local path: %s", uri, error->message);
		g_error_free (error);
		return;
	}
	
	gtk_recent_manager_remove_item (gpw->priv->recent_manager, uri, &error);
	if (error)
	{
		g_warning ("Could not remove recent-files uri \"%s\": %s", uri, error->message);	
	}
	
	g_free (uri);
}

static void
gpw_open_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	GtkWidget *filechooser;
	gchar     *path = NULL, *default_path;

	filechooser = glade_util_file_dialog_new (_("Open\342\200\246"), GTK_WINDOW (gpw->priv->window),
						   GLADE_FILE_DIALOG_ACTION_OPEN);


	default_path = g_strdup (gpw_get_default_path (gpw));
	if (default_path != NULL)
	{
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (filechooser), default_path);
		g_free (default_path);
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
gpw_save (GladeProjectWindow *gpw, GladeProject *project, const gchar *path)
{
	GError   *error = NULL;
	gchar    *display_name, *display_path = g_strdup (path);
	time_t    mtime;
	GtkWidget *dialog;
	GtkWidget *button;
	gint       response;

	/* check for external modification to the project file */
	mtime = glade_util_get_file_mtime (project->path, NULL);
	
	if (mtime > glade_project_get_file_mtime (project)) {
	
		dialog = gtk_message_dialog_new (GTK_WINDOW (gpw->priv->window),
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_NONE,
						 _("The file %s has been modified since reading it"),
						 project->path);
						 
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), 				 
							  _("If you save it, all the external changes could be lost. Save it anyway?"));
							  
		gtk_window_set_title (GTK_WINDOW (dialog), "");
		
	        button = gtk_button_new_with_mnemonic (_("_Save Anyway"));
	        gtk_button_set_image (GTK_BUTTON (button),
	        		      gtk_image_new_from_stock (GTK_STOCK_SAVE,
	        		      				GTK_ICON_SIZE_BUTTON));
	        gtk_widget_show (button);

		gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, GTK_RESPONSE_ACCEPT);	        		      			
	        gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Don't Save"), GTK_RESPONSE_REJECT);			 
						 
		gtk_dialog_set_default_response	(GTK_DIALOG (dialog), GTK_RESPONSE_REJECT);
		
		response = gtk_dialog_run (GTK_DIALOG (dialog));
		
		gtk_widget_destroy (dialog);
		
		if (response == GTK_RESPONSE_REJECT)
		{
			g_free (display_path);
			return;
		}
	}
		  
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
	display_name = glade_project_display_name (project, FALSE, FALSE, FALSE);

	gpw_recent_add (gpw, project->path);
	update_default_path (gpw, project);

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
gpw_save_as (GladeProjectWindow *gpw)
{
 	GladeProject *project, *another_project;
 	GtkWidget *filechooser;
	gchar *path = NULL;
	gchar *real_path, *ch;
	
	project = glade_design_view_get_project (gpw->priv->active_view);
	
	if (project == NULL)
	{
		/* Just in case the menu-item is not insensitive */
		glade_util_ui_message (gpw->priv->window, GLADE_UI_WARN, _("No open projects to save"));
		return;
	}

	filechooser = glade_util_file_dialog_new (_("Save As\342\200\246"),
						  GTK_WINDOW (gpw->priv->window),
						  GLADE_FILE_DIALOG_ACTION_SAVE);

	if (project->path)
	{
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (filechooser), project->path);
	}
	else	
	{
		gchar *default_path = g_strdup (gpw_get_default_path (gpw));
		if (default_path != NULL)
		{
			gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (filechooser), default_path);
			g_free (default_path);
		}

		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (filechooser), project->name);
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
	GladeProject *project;

	project = glade_design_view_get_project (gpw->priv->active_view);

	if (project == NULL)
	{
		/* Just in case the menu-item or button is not insensitive */
		glade_util_ui_message (gpw->priv->window, GLADE_UI_WARN, _("No open projects to save"));
		return;
	}

	if (project->path != NULL) 
	{
		gpw_save (gpw, project, project->path);
 		return;
	}

	/* If instead we dont have a path yet, fire up a file selector */
	gpw_save_as (gpw);
}

static void
gpw_save_as_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	gpw_save_as (gpw);
}
static gboolean
gpw_confirm_close_project (GladeProjectWindow *gpw, GladeProject *project)
{
	GtkWidget *dialog;
	gboolean close = FALSE;
	gchar *msg;
	gint ret;
	GError *error = NULL;

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
			gchar *default_path;

			filechooser =
				glade_util_file_dialog_new (_("Save\342\200\246"),
							    GTK_WINDOW (gpw->priv->window),
							    GLADE_FILE_DIALOG_ACTION_SAVE);
	
			default_path = g_strdup (gpw_get_default_path (gpw));
			if (default_path != NULL)
			{
				gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (filechooser), default_path);
				g_free (default_path);
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
do_close (GladeProjectWindow *gpw, GladeDesignView *view)
{
	gint n;
	
	n = gtk_notebook_page_num (GTK_NOTEBOOK (gpw->priv->notebook), GTK_WIDGET (view));
	
	g_object_ref (view);
	
	gtk_notebook_remove_page (GTK_NOTEBOOK (gpw->priv->notebook), n);
	
	g_object_unref (view);	
}

static void
gpw_close_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	GladeDesignView *view;
	GladeProject *project;
	gboolean close;
	
	view = gpw->priv->active_view;

	project = glade_design_view_get_project (view);

	if (view == NULL)
		return;

	if (project->changed)
	{
		close = gpw_confirm_close_project (gpw, project);
			if (!close)
				return;
	}
	do_close (gpw, view);
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
		do_close (gpw, glade_design_view_get_from_project (project));
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
gpw_hide_window_on_delete (GtkWidget *window, gpointer not_used, GtkUIManager *ui)
{
	glade_util_hide_window (GTK_WINDOW (window));
	return TRUE;
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
glade_project_window_previous_project_cb (GtkAction *action, GladeProjectWindow *window)
{
	gtk_notebook_prev_page (GTK_NOTEBOOK (window->priv->notebook));
}

static void
glade_project_window_next_project_cb (GtkAction *action, GladeProjectWindow *window)
{
	gtk_notebook_next_page (GTK_NOTEBOOK (window->priv->notebook));
}

static void
gpw_notebook_switch_page_cb (GtkNotebook *notebook,
			     GtkNotebookPage *page,
			     guint page_num,
			     GladeProjectWindow *gpw)
{
	GladeDesignView *view;
	GladeProject *project;
	GtkAction *action;
	gchar *action_name;

	view = GLADE_DESIGN_VIEW (gtk_notebook_get_nth_page (notebook, page_num));

	/* CHECK: I don't know why but it seems notebook_switch_page is called
	two times every time the user change the active tab */
	if (view == gpw->priv->active_view)
		return;

	gpw->priv->active_view = view;
	
	project = glade_design_view_get_project (view);

	/* FIXME: this does not feel good */
	glade_app_set_project (project);

	gpw_refresh_title (gpw);
	gpw_set_sensitivity_according_to_project (gpw, project);
	
	/* activate the corresponding item in the project menu */
	action_name = g_strdup_printf ("Tab_%d", page_num);
	action = gtk_action_group_get_action (gpw->priv->projects_list_menu_actions,
					      action_name);

	/* sometimes the action doesn't exist yet, and the proper action
	 * is set active during the documents list menu creation
	 * CHECK: would it be nicer if active_view was a property and we monitored the notify signal?
	 */
	if (action != NULL)
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

	g_free (action_name);

}

static void
gpw_notebook_tab_added_cb (GtkNotebook *notebook,
			   GladeDesignView *view,
			   guint page_num,
			   GladeProjectWindow *gpw)
{
	GladeProject *project;

	++gpw->priv->num_tabs;
	
	project = glade_design_view_get_project (view);

	g_signal_connect (G_OBJECT (project), "notify::has-unsaved-changes",
			  G_CALLBACK (gpw_project_notify_handler_cb),
			  gpw);	
	g_signal_connect (G_OBJECT (project), "selection-changed",
			  G_CALLBACK (project_selection_changed_cb), gpw);
	g_signal_connect (G_OBJECT (project), "notify::has-selection",
			  G_CALLBACK (gpw_project_notify_handler_cb),
			  gpw);
	g_signal_connect (G_OBJECT (project), "notify::read-only",
			  G_CALLBACK (gpw_project_notify_handler_cb),
			  gpw);

	gpw_set_sensitivity_according_to_project (gpw, project);

	gpw_refresh_projects_list_menu (gpw);

	gpw_refresh_title (gpw);

	if (gpw->priv->num_tabs > 0)
		gtk_action_group_set_sensitive (gpw->priv->project_actions, TRUE);

}

static void
gpw_notebook_tab_removed_cb (GtkNotebook *notebook,
			     GladeDesignView *view,
			     guint page_num,
			     GladeProjectWindow *gpw)
{
	GladeProject *project;

	--gpw->priv->num_tabs;

	if (gpw->priv->num_tabs == 0)
		gpw->priv->active_view = NULL;

	project = glade_design_view_get_project (view);	

	g_signal_handlers_disconnect_by_func (G_OBJECT (project),
					      G_CALLBACK (gpw_project_notify_handler_cb),
					      gpw);
	g_signal_handlers_disconnect_by_func (G_OBJECT (project),
					      G_CALLBACK (project_selection_changed_cb),
					      gpw);

	/* FIXME: this function needs to be preferably called somewhere else */
	glade_app_remove_project (project);

	gpw_refresh_projects_list_menu (gpw);

	gpw_refresh_title (gpw);

	if (gpw->priv->active_view)
		gpw_set_sensitivity_according_to_project (gpw, glade_design_view_get_project (gpw->priv->active_view));
	else
		gtk_action_group_set_sensitive (gpw->priv->project_actions, FALSE);	

}

static void
gpw_recent_chooser_item_activated_cb (GtkRecentChooser *chooser, GladeProjectWindow *gpw)
{
	gchar *uri, *path;
	GError *error = NULL;

	uri = gtk_recent_chooser_get_current_uri (chooser);

	path = g_filename_from_uri (uri, NULL, NULL);
	if (error)
	{
		g_warning ("Could not convert uri \"%s\" to a local path: %s", uri, error->message);
		g_error_free (error);
		return;
	}
	
	if (!glade_project_window_open_project (gpw, path))
	{
		gpw_recent_remove (gpw, path);
	}

	g_free (uri);
	g_free (path);
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
	glade_palette_set_use_small_item_icons (glade_app_get_palette(),
						gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}

static void
gpw_show_clipboard_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	static GtkWidget *view = NULL;
	
	g_return_if_fail (gpw != NULL);

	if (view == NULL)
	{
		view = glade_app_get_clipboard_view ();

		g_signal_connect (view, "delete_event",
				  G_CALLBACK (gpw_hide_window_on_delete),
				  gpw->priv->ui);
		gtk_widget_show_all (view);
	}
	
        gtk_window_present (GTK_WINDOW (view));
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
gpw_show_help_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	GtkWidget *dialog;
	gboolean retval;
	
	retval = glade_project_window_help_show (NULL);
	if (retval)
		return;
	
	/* fallback to displaying online user manual */ 
	retval = glade_util_url_show (GLADE_URL_USER_MANUAL);	

	if (!retval)
	{
		dialog = gtk_message_dialog_new (GTK_WINDOW (gpw->priv->window),
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("Could not display the online user manual"));
						 
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("No suitable web browser executable could be found "
							    "to be executed and to display the URL: %s"),
							    GLADE_URL_USER_MANUAL);
						 	
		gtk_window_set_title (GTK_WINDOW (dialog), "");

		g_signal_connect_swapped (dialog, "response",
					  G_CALLBACK (gtk_widget_destroy),
					  dialog);

		gtk_widget_show (dialog);
		
	}
}

static void 
gpw_show_developer_manual_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	GtkWidget *dialog;
	gboolean retval;
	
	if (glade_util_have_devhelp ())
	{
		glade_util_search_devhelp ("gladeui", NULL, NULL);
		return;	
	}
	
	retval = glade_util_url_show (GLADE_URL_DEVELOPER_MANUAL);	

	if (!retval)
	{
		dialog = gtk_message_dialog_new (GTK_WINDOW (gpw->priv->window),
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("Could not display the online developer reference manual"));
						 
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("No suitable web browser executable could be found "
							    "to be executed and to display the URL: %s"),
							    GLADE_URL_DEVELOPER_MANUAL);
						 	
		gtk_window_set_title (GTK_WINDOW (dialog), "");

		g_signal_connect_swapped (dialog, "response",
					  G_CALLBACK (gtk_widget_destroy),
					  dialog);

		gtk_widget_show (dialog);
	}	
}

static void 
gpw_about_cb (GtkAction *action, GladeProjectWindow *gpw)
{
	static const gchar * const authors[] =
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

	static const gchar license[] =
		N_("This program is free software; you can redistribute it and/or modify\n"
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

	static const gchar copyright[] =
		"Copyright \xc2\xa9 2001-2006 Ximian, Inc.\n"
		"Copyright \xc2\xa9 2001-2006 Joaquin Cuenca Abela, Paolo Borelli, et al.\n"
		"Copyright \xc2\xa9 2001-2006 Tristan Van Berkom, Juan Pablo Ugarte, et al.";
	
	gtk_show_about_dialog (GTK_WINDOW (gpw->priv->window),
			       "name", g_get_application_name (),
			       "logo-icon-name", "glade-3",
			       "authors", authors,
			       "translator-credits", _("translator-credits"),
			       "comments", _("A user interface designer for GTK+ and GNOME."),
			       "license", _(license),
			       "copyright", copyright,
			       "version", PACKAGE_VERSION,
			       "website", "http://glade.gnome.org",
			       NULL);
}

static const gchar *ui_info =
"<ui>\n"
"  <menubar name='MenuBar'>\n"
"    <menu action='FileMenu'>\n"
"      <menuitem action='New'/>\n"
"      <menuitem action='Open'/>\n"
"      <menuitem action='OpenRecent'/>\n"
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
"      <menuitem action='Clipboard'/>\n"
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
"      <menuitem action='PreviousProject'/>\n"
"      <menuitem action='NextProject'/>\n"
"      <separator/>\n"
"      <placeholder name='ProjectsListPlaceholder'/>\n"
"    </menu>\n"
"    <menu action='HelpMenu'>\n"
"      <menuitem action='HelpContents'/>\n"
"      <menuitem action='DeveloperReference'/>\n"
"      <separator/>\n"
"      <menuitem action='About'/>\n"
"    </menu>\n"
"  </menubar>\n"
"  <toolbar  name='ToolBar'>"
"    <toolitem action='New'/>"
"    <toolitem action='Open'/>"
"    <toolitem action='Save'/>"
"    <separator/>\n"
"    <toolitem action='Undo'/>"
"    <toolitem action='Redo'/>"
"    <separator/>\n"
"    <toolitem action='Cut'/>"
"    <toolitem action='Copy'/>"
"    <toolitem action='Paste'/>"
"  </toolbar>"
"</ui>\n";

static GtkActionEntry static_entries[] = {
	{ "FileMenu", NULL, N_("_File") },
	{ "EditMenu", NULL, N_("_Edit") },
	{ "ViewMenu", NULL, N_("_View") },
	{ "ProjectMenu", NULL, N_("_Projects") },
	{ "HelpMenu", NULL, N_("_Help") },
	
	/* FileMenu */
	{ "New", GTK_STOCK_NEW, N_("_New"), "<control>N",
	  N_("Create a new project"), G_CALLBACK (gpw_new_cb) },
	
	{ "Open", GTK_STOCK_OPEN, N_("_Open\342\200\246"),"<control>O",
	  N_("Open a project"), G_CALLBACK (gpw_open_cb) },

	{ "OpenRecent", NULL, N_("Open _Recent") },	
	
	{ "Quit", GTK_STOCK_QUIT, N_("_Quit"), "<control>Q",
	  N_("Quit the program"), G_CALLBACK (gpw_quit_cb) },

	/* ViewMenu */
	{ "PaletteAppearance", NULL, N_("Palette _Appearance") },
	
	/* HelpMenu */
	{ "About", GTK_STOCK_ABOUT, N_("_About"), NULL,
	  N_("About this application"), G_CALLBACK (gpw_about_cb) },

	{ "HelpContents", GTK_STOCK_HELP, N_("_Contents"), "F1",
	  N_("Display the user manual"), G_CALLBACK (gpw_show_help_cb) },

	{ "DeveloperReference", NULL, N_("_Developer Reference"), NULL,
	  N_("Display the developer reference manual"), G_CALLBACK (gpw_show_developer_manual_cb) }
};

static guint n_static_entries = G_N_ELEMENTS (static_entries);

static GtkActionEntry project_entries[] = {

	/* FileMenu */
	{ "Save", GTK_STOCK_SAVE, N_("_Save"),"<control>S",
	  N_("Save the current project"), G_CALLBACK (gpw_save_cb) },
	
	{ "SaveAs", GTK_STOCK_SAVE_AS, N_("Save _As\342\200\246"), NULL,
	  N_("Save the current project with a different name"), G_CALLBACK (gpw_save_as_cb) },
	
	{ "Close", GTK_STOCK_CLOSE, N_("_Close"), "<control>W",
	  N_("Close the current project"), G_CALLBACK (gpw_close_cb) },

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
	
	{ "Clipboard", NULL, N_("_Clipboard"), NULL,
	  N_("Show the clipboard"),
	  G_CALLBACK (gpw_show_clipboard_cb) },
	  
	/* ProjectsMenu */
	{ "PreviousProject", NULL, N_("_Previous Project"), "<control>Page_Up",
	  N_("Activate previous project"), G_CALLBACK (glade_project_window_previous_project_cb) },

	{ "NextProject", NULL, N_("_Next Project"), "<control>Page_Down",
	  N_("Activate next project"), G_CALLBACK (glade_project_window_next_project_cb) }


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
	
	gpw->priv->ui = gtk_ui_manager_new ();

	g_signal_connect(G_OBJECT(gpw->priv->ui), "connect-proxy",
			 G_CALLBACK (gpw_ui_connect_proxy_cb), gpw);
	g_signal_connect(G_OBJECT(gpw->priv->ui), "disconnect-proxy",
			 G_CALLBACK (gpw_ui_disconnect_proxy_cb), gpw);

	gtk_ui_manager_insert_action_group (gpw->priv->ui, gpw->priv->static_actions, 0);
	gtk_ui_manager_insert_action_group (gpw->priv->ui, gpw->priv->project_actions, 1);
	gtk_ui_manager_insert_action_group (gpw->priv->ui, gpw->priv->projects_list_menu_actions, 3);
	
	gtk_window_add_accel_group (GTK_WINDOW (gpw->priv->window), 
				  gtk_ui_manager_get_accel_group (gpw->priv->ui));

	if (!gtk_ui_manager_add_ui_from_string (gpw->priv->ui, ui_info, -1, &error))
	{
		g_message ("Building menus failed: %s", error->message);
		g_error_free (error);
	}
	
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
			guint info, guint time, GladeProjectWindow *window)
{
	gchar **uris, **str;
	gchar *data;

	if (info != TARGET_URI_LIST)
		return;

	/* On MS-Windows, it looks like `selection_data->data' is not NULL terminated. */
	data = g_new (gchar, selection_data->length + 1);
	memcpy (data, selection_data->data, selection_data->length);
	data[selection_data->length] = 0;

	uris = g_uri_list_extract_uris (data);

	for (str = uris; *str; str++)
	{
		GError *error = NULL;
		gchar *path = g_filename_from_uri (*str, NULL, &error);

		if (path)
		{
			glade_project_window_open_project (window, path);
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
gpw_delete_event (GtkWindow *w, GdkEvent *event, GladeProjectWindow *gpw)
{
	gpw_quit_cb (NULL, gpw);
	
	/* return TRUE to stop other handlers */
	return TRUE;	
}

static GtkWidget *
create_selector_tool_button (GtkToolbar *toolbar)
{
	GtkToolItem  *button;
	GtkWidget    *image;
	gchar        *image_path;
	
	image_path = g_build_filename (glade_app_get_pixmaps_dir (), "selector.png", NULL);
	image = gtk_image_new_from_file (image_path);
	g_free (image_path);
	
	button = gtk_toggle_tool_button_new ();
	gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (button), TRUE);
	
	gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (button), image);
	gtk_tool_button_set_label (GTK_TOOL_BUTTON (button), _("Select Widgets"));
	
	gtk_tool_item_set_tooltip (GTK_TOOL_ITEM (button),
				   toolbar->tooltips,
				   _("Select widgets in the workspace"),
				   NULL);
	
	gtk_widget_show (GTK_WIDGET (button));
	gtk_widget_show (image);
	
	return GTK_WIDGET (button);
}

static gint
gpw_hijack_editor_key_press (GtkWidget          *win, 
			     GdkEventKey        *event, 
			     GladeProjectWindow *gpw)
{
	if (GTK_WINDOW (win)->focus_widget &&
	    (event->keyval == GDK_Delete || /* Filter Delete from accelerator keys */
	     ((event->state & GDK_CONTROL_MASK) && /* CNTL keys... */
	      ((event->keyval == GDK_c || event->keyval == GDK_C) || /* CNTL-C (copy)  */
	       (event->keyval == GDK_x || event->keyval == GDK_X) || /* CNTL-X (cut)   */
	       (event->keyval == GDK_v || event->keyval == GDK_V))))) /* CNTL-V (paste) */
	{
		return gtk_widget_event (GTK_WINDOW (win)->focus_widget, 
					 (GdkEvent *)event);
	}
	return FALSE;
}

static void
glade_project_window_create (GladeProjectWindow *gpw)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hpaned1;
	GtkWidget *hpaned2;
	GtkWidget *vpaned;
	GtkWidget *menubar;
	GtkWidget *toolbar;
	GladeProjectView *project_view;
	GtkWidget *statusbar;
	GtkWidget *editor_item;
	GtkWidget *palette;
	GtkWidget *editor;
	GtkWidget *dockitem;
	GtkWidget *widget;
	GtkToolItem *sep;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gpw->priv->window = window;
	gtk_window_maximize (GTK_WINDOW (window));
	gtk_window_set_default_size (GTK_WINDOW (window), 720, 540);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (window), vbox);
	gpw->priv->main_vbox = vbox;

	/* menubar */
	menubar = gpw_construct_menu (gpw);
	gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, TRUE, 0);
	gtk_widget_show (menubar);

	/* toolbar */
	toolbar = gtk_ui_manager_get_widget (gpw->priv->ui, "/ToolBar");
	gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, TRUE, 0);
	gtk_widget_show (toolbar);

	/* main contents */
	hpaned1 = gtk_hpaned_new ();
	hpaned2 = gtk_hpaned_new ();
	vpaned = gtk_vpaned_new ();

	gtk_container_set_border_width (GTK_CONTAINER (hpaned1), 2);

	gtk_box_pack_start (GTK_BOX (vbox), hpaned1, TRUE, TRUE, 0);
	gtk_paned_pack1 (GTK_PANED (hpaned1), hpaned2, TRUE, FALSE);
	gtk_paned_pack2 (GTK_PANED (hpaned1), vpaned, FALSE, FALSE);

	/* divider position between design area and editor/tree */
	gtk_paned_set_position (GTK_PANED (hpaned1), 350);
	/* divider position between tree and editor */	
	gtk_paned_set_position (GTK_PANED (vpaned), 150);


	gtk_widget_show_all (hpaned1);
	gtk_widget_show_all (hpaned2);
	gtk_widget_show_all (vpaned);

	/* palette */
	palette = GTK_WIDGET (glade_app_get_palette ());
	glade_palette_set_show_selector_button (GLADE_PALETTE (palette), FALSE);
	dockitem = gpw_construct_dock_item (gpw, _("Palette"), palette);
	gtk_paned_pack1 (GTK_PANED (hpaned2), dockitem, FALSE, FALSE);

	/* notebook */
	gpw->priv->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (gpw->priv->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (gpw->priv->notebook), FALSE);
	gtk_paned_pack2 (GTK_PANED (hpaned2), gpw->priv->notebook, TRUE, FALSE);
	gtk_widget_show (gpw->priv->notebook);

	/* project view */
	project_view = GLADE_PROJECT_VIEW (glade_project_view_new ());
	glade_app_add_project_view (project_view);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (project_view),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (project_view), GTK_SHADOW_IN);
	dockitem = gpw_construct_dock_item (gpw, _("Inspector"), GTK_WIDGET (project_view));
	gtk_paned_pack1 (GTK_PANED (vpaned), dockitem, FALSE, FALSE); 
	gtk_widget_show_all (GTK_WIDGET (project_view));

	/* editor */
	editor = GTK_WIDGET (glade_app_get_editor ());
	dockitem = gpw_construct_dock_item (gpw, _("Properties"), editor);
	gpw->priv->label = GTK_LABEL (g_object_get_data (G_OBJECT (dockitem), "dock-label"));
	gtk_label_set_ellipsize	(GTK_LABEL (gpw->priv->label), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (gpw->priv->label), 0, 0.5);
	gtk_paned_pack2 (GTK_PANED (vpaned), dockitem, TRUE, FALSE);

	/* status bar */
	statusbar = gpw_construct_statusbar (gpw);
	gtk_box_pack_end (GTK_BOX (vbox), statusbar, FALSE, TRUE, 0);
	gtk_widget_show (statusbar);

	gtk_widget_show (vbox);

	/* devhelp */
	editor_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						 "/MenuBar/ViewMenu/PropertyEditorHelp");

	gtk_widget_set_sensitive (editor_item, FALSE);
	
	/* recent files */	
	gpw->priv->recent_manager = gtk_recent_manager_get_for_screen (gtk_widget_get_screen (gpw->priv->window));

	gpw->priv->recent_menu = create_recent_chooser_menu (gpw, gpw->priv->recent_manager);

	g_signal_connect (gpw->priv->recent_menu,
			  "item-activated",
			  G_CALLBACK (gpw_recent_chooser_item_activated_cb),
			  gpw);
			  
	widget = gtk_ui_manager_get_widget (gpw->priv->ui, "/MenuBar/FileMenu/OpenRecent");
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (widget), gpw->priv->recent_menu);		  
	
	/* palette selector button */
	
	gpw->priv->selector_button = GTK_TOGGLE_TOOL_BUTTON (create_selector_tool_button (GTK_TOOLBAR (toolbar)));	
	
	sep = gtk_separator_tool_item_new();
	gtk_widget_show (GTK_WIDGET (sep));
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (sep), -1);
	
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (gpw->priv->selector_button), -1);
	
	g_signal_connect (G_OBJECT (glade_app_get_palette ()), "toggled",
			  G_CALLBACK (on_palette_toggled), gpw);
	g_signal_connect (G_OBJECT (gpw->priv->selector_button), "toggled",
			  G_CALLBACK (on_selector_button_toggled), gpw);
	
	
	/* support for opening a file by dragging onto the project window */
	gtk_drag_dest_set (GTK_WIDGET (window),
			   GTK_DEST_DEFAULT_ALL,
			   drop_types, G_N_ELEMENTS (drop_types),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect (G_OBJECT (window), "drag-data-received",
			  G_CALLBACK (gpw_drag_data_received), gpw);

	g_signal_connect (G_OBJECT (window), "delete_event",
			  G_CALLBACK (gpw_delete_event), gpw);
			  
	/* connect signals notebook */
	g_signal_connect (gpw->priv->notebook,
			  "switch-page",
			  G_CALLBACK (gpw_notebook_switch_page_cb),
			  gpw);
	g_signal_connect (gpw->priv->notebook,
			  "page-added",
			  G_CALLBACK (gpw_notebook_tab_added_cb),
			  gpw);
	g_signal_connect (gpw->priv->notebook,
			  "page-removed",
			  G_CALLBACK (gpw_notebook_tab_removed_cb),
			  gpw);
			  
			  
	/* GtkWindow events */
	g_signal_connect (gpw->priv->window, "screen-changed",
			  G_CALLBACK (gpw_window_screen_changed_cb),
			  gpw);
			  
	g_signal_connect (gpw->priv->window, "window-state-event",
			  G_CALLBACK (gpw_window_state_event_cb),
			  gpw);

	g_signal_connect (G_OBJECT (gpw->priv->window), "key-press-event",
			  G_CALLBACK (gpw_hijack_editor_key_press), gpw);

}

static void
glade_project_window_add_project (GladeProjectWindow *gpw, GladeProject *project)
{
	GtkWidget *view;

 	g_return_if_fail (GLADE_IS_PROJECT (project));
 	
 	view = glade_design_view_new (project);	
	gtk_widget_show (view);
	
	glade_app_add_project (project);

	gtk_notebook_append_page (GTK_NOTEBOOK (gpw->priv->notebook), GTK_WIDGET (view), NULL);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (gpw->priv->notebook), -1);	
	
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

gboolean
glade_project_window_open_project (GladeProjectWindow *gpw, const gchar *path)
{
	GladeProject *project;

	g_return_val_if_fail (path != NULL, FALSE);

	/* dont allow more than one project with the same name to be
	 * opened simultainiously.
	 */
	if (glade_app_is_project_loaded ((gchar*)path))
	{
		glade_util_ui_message (gpw->priv->window, GLADE_UI_WARN, 
				       _("%s is already open"), path);
		return TRUE; /* let this pass */
	}
	
	if ((project = glade_project_open (path)) == NULL)
		return FALSE;

	glade_project_window_add_project (gpw, project);
	
	gpw_recent_add (gpw, project->path);
	update_default_path (gpw, project);
	
	return TRUE;
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
        gtk_widget_show (gpw->priv->window);
}

static void
glade_project_window_finalize (GObject *object)
{
	g_free (GLADE_PROJECT_WINDOW (object)->priv->default_path);

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
	
	g_type_class_add_private (object_class, sizeof (GladeProjectWindowPrivate));
}

static void
glade_project_window_init (GladeProjectWindow *gpw)
{
	gpw->priv = GLADE_PROJECT_WINDOW_GET_PRIVATE (gpw);
	
	gpw->priv->label = NULL;
	gpw->priv->default_path = NULL;
	
	gtk_about_dialog_set_url_hook ((GtkAboutDialogActivateLinkFunc) about_dialog_activate_link_func, gpw, NULL);
	
}

GladeProjectWindow *
glade_project_window_new (void)
{
	GladeProjectWindow *gpw;
	GtkAccelGroup *accel_group;
	
	gpw = g_object_new (GLADE_TYPE_PROJECT_WINDOW, NULL);

	glade_project_window_create (gpw);

	glade_app_set_window (gpw->priv->window);

	accel_group = gtk_ui_manager_get_accel_group(gpw->priv->ui);
	glade_app_set_accel_group (accel_group);

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
	GtkWidget *editor_item;
	
	editor_item = gtk_ui_manager_get_widget (gpw->priv->ui,
						 "/MenuBar/ViewMenu/PropertyEditorHelp");	

	if (glade_util_have_devhelp ())
	{
		GladeEditor *editor = glade_app_get_editor ();
		glade_editor_show_info (editor);
		glade_editor_hide_context_info (editor);
		
		g_signal_handlers_disconnect_by_func (editor, gpw_doc_search_cb, gpw);
		
		g_signal_connect (editor, "gtk-doc-search",
				  G_CALLBACK (gpw_doc_search_cb), gpw);
		
		gtk_widget_set_sensitive (editor_item, TRUE);
	}
	else
	{
		gtk_widget_set_sensitive (editor_item, FALSE);
	}
}
