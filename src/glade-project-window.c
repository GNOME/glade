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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkstock.h>

const gchar *WINDOW_TITLE = "Glade-3 GUI Builder";
const gint   GLADE_WIDGET_TREE_WIDTH      = 230;
const gint   GLADE_WIDGET_TREE_HEIGHT     = 300;
const gint   GLADE_PALETTE_DEFAULT_HEIGHT = 450;

static void
gpw_refresh_title (GladeProjectWindow *gpw)
{
	gchar *title;

	if (gpw->active_project)
		title = g_strdup_printf ("%s - %s", WINDOW_TITLE, gpw->active_project->name);
	else
		title = g_strdup_printf ("%s", WINDOW_TITLE);

	gtk_window_set_title (GTK_WINDOW (gpw->window), title);
	g_free (title);
}

static void
gpw_refresh_project_entry (GladeProject *project)
{
	GladeProjectWindow *gpw;
	GtkWidget *item;
	GtkWidget *label;

	gpw = glade_project_window_get ();

	item = gtk_item_factory_get_item (gpw->item_factory, project->entry.path);
	label = gtk_bin_get_child (GTK_BIN (item));

	/* Change the menu item's label */
	gtk_label_set_text (GTK_LABEL (label), project->name);

	/* Update the path entry, for future changes. */
	g_free (project->entry.path);
	project->entry.path = g_strdup_printf ("/Project/%s", project->name);
}

static void
glade_project_window_set_project (GladeProject *project)
{
	GladeProjectWindow *gpw;
	GList *list;

	g_return_if_fail (GLADE_IS_PROJECT (project));

	gpw = glade_project_window_get ();

	if (gpw->active_project == project)
		return;

	if (!g_list_find (gpw->projects, project))
	{
		g_warning ("Could not set project because it could not "
			   " be found in the gpw->project list\n");
		return;
	}

	/* clear the selection in the previous project */
	if (gpw->active_project)
		glade_project_selection_clear (gpw->active_project, FALSE);

	gpw->active_project = project;
	gpw_refresh_title (gpw);

	for (list = gpw->views; list; list = list->next)
	{
		GladeProjectView *view = list->data;
		glade_project_view_set_project (view, project);
	}

	/* trigger the selection changed signal to update the editor */
	glade_project_selection_changed (project);
}

static void
gpw_new_cb (void)
{
	glade_project_window_new_project ();
}

static void
gpw_open_cb (void)
{
	GladeProjectWindow *gpw;
	GtkWidget *filechooser;
	const gchar *path = NULL;

	gpw = glade_project_window_get ();

	filechooser = glade_util_file_dialog_new (_("Open..."), GTK_WINDOW (gpw->window),
						   GLADE_FILE_DIALOG_ACTION_OPEN);

	if (gtk_dialog_run (GTK_DIALOG(filechooser)) == GTK_RESPONSE_OK)
		path = glade_util_file_dialog_get_filename (filechooser);

	gtk_widget_destroy (filechooser);

	if (!path)
		return;

	glade_project_window_open_project (path);
}

static void
gpw_save (GladeProject *project, const gchar *path)
{
	GladeProjectWindow  *gpw   = glade_project_window_get ();
	GError              *error = NULL;

	gpw = glade_project_window_get ();
	
	if (!glade_project_save (project, path, &error))
	{
		glade_util_ui_warn (gpw->window, error->message);
		g_error_free (error);
		return;
	}

	gpw_refresh_project_entry (project);
	gpw_refresh_title (gpw);
	glade_util_flash_message (gpw->statusbar,
				  gpw->statusbar_actions_context_id,
				  _("Project '%s' saved"), project->name);
}

static void
gpw_save_cb (void)
{
	GladeProjectWindow *gpw = glade_project_window_get ();
	GladeProject       *project;
	GtkWidget          *filechooser;
	const gchar        *path  = NULL;
	GError             *error = NULL;

	project = gpw->active_project;

	if (project->path != NULL) 
	{
		if (glade_project_save (project, project->path, &error)) 
		{
			glade_util_flash_message 
				(gpw->statusbar,
				 gpw->statusbar_actions_context_id,
				 _("Project '%s' saved"),
				 project->name);
		} 
		else 
		{
			glade_util_ui_warn (gpw->window, error->message);
			g_error_free (error);
		}
		return;
	}

	/* If instead we dont have a path yet, fire up a file selector */
	filechooser = glade_util_file_dialog_new (_("Save..."), GTK_WINDOW (gpw->window),
						   GLADE_FILE_DIALOG_ACTION_SAVE);

	
	if (gtk_dialog_run (GTK_DIALOG(filechooser)) == GTK_RESPONSE_OK)
		path = glade_util_file_dialog_get_filename (filechooser);

	gtk_widget_destroy (filechooser);

	if (!path)
		return;

	gpw_save (project, path);
}

static void
gpw_save_as_cb (void)
{
	GladeProjectWindow *gpw;
	GladeProject *project;
	GtkWidget *filechooser;
	const gchar *path = NULL;

	gpw = glade_project_window_get ();
	project = gpw->active_project;

	filechooser = glade_util_file_dialog_new (_("Save as ..."), GTK_WINDOW (gpw->window),
						   GLADE_FILE_DIALOG_ACTION_SAVE);

	if (gtk_dialog_run (GTK_DIALOG(filechooser)) == GTK_RESPONSE_OK)
		path = glade_util_file_dialog_get_filename (filechooser);

	gtk_widget_destroy (filechooser);

	if (!path)
		return;

	gpw_save (project, path);
}

static gboolean
gpw_confirm_close_project (GladeProject *project)
{
	GladeProjectWindow *gpw;
	GtkWidget *dialog;
	gboolean close = FALSE;
	char *msg;
	gint ret;
	GError *error = NULL;

	g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

	gpw = glade_project_window_get ();

	msg = g_strdup_printf (_("<span weight=\"bold\" size=\"larger\">Save changes to project \"%s\" before closing?</span>\n\n"
				 "Your changes will be lost if you don't save them.\n"), project->name);

	dialog = gtk_message_dialog_new (GTK_WINDOW (gpw->window),
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
				glade_util_ui_warn (gpw->window, error->message);
				g_error_free (error);
			}
		}
		else
		{
			GtkWidget *filechooser;
			const gchar *path = NULL;

			filechooser = glade_util_file_dialog_new (_("Save ..."), GTK_WINDOW (gpw->window),
								   GLADE_FILE_DIALOG_ACTION_SAVE);

			if (gtk_dialog_run (GTK_DIALOG(filechooser)) == GTK_RESPONSE_OK)
				path = glade_util_file_dialog_get_filename (filechooser);

			gtk_widget_destroy (filechooser);

			if (!path)
				break;

			gpw_save (project, path);

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
	char *item_path;
	GList *list;

	item_path = g_strdup_printf ("/Project/%s", project->name);
	gtk_item_factory_delete_item (gpw->item_factory, item_path);
	g_free (item_path);

	for (list = project->objects; list; list = list->next)
	{
		GObject *object = list->data;
		if (GTK_IS_WIDGET (object) && GTK_WIDGET_TOPLEVEL (object))
			gtk_widget_destroy (GTK_WIDGET(object));
	}

	gpw->projects = g_list_remove (gpw->projects, project);
}


static void
gpw_close_cb (void)
{
	GladeProjectWindow *gpw;
	GladeProject *project;
	gboolean close;
	
	gpw = glade_project_window_get ();
	project = gpw->active_project;

	if (!project)
		return;

	if (project->changed)
	{
		close = gpw_confirm_close_project (project);
			if (!close)
				return;
	}

	do_close (gpw, project);

	/* this is needed to prevent clearing the selection of a closed project 
         */
	gpw->active_project = NULL;

	/* If no more projects */
	if (gpw->projects == NULL)
	{
		GList *list;
		for (list = gpw->views; list; list = list->next)
		{
			GladeProjectView *view;

			view = GLADE_PROJECT_VIEW (list->data);
			glade_project_view_set_project (view, NULL);
		}
		gpw_refresh_title (gpw);
		glade_editor_load_widget (gpw->editor, NULL);
		gtk_widget_set_sensitive (GTK_WIDGET (gpw->palette), FALSE);
		return;
	}

	glade_project_window_set_project (gpw->projects->data);
}

static void
gpw_quit_cb (void)
{
	GladeProjectWindow *gpw;
	GList *list;

	gpw = glade_project_window_get ();

	for (list = gpw->projects; list; list = list->next)
	{
		GladeProject *project = GLADE_PROJECT (list->data);

		if (project->changed)
		{
			gboolean quit = gpw_confirm_close_project (project);
			if (!quit)
				return;
		}
	}

	while (gpw->projects) {
		GladeProject *project = GLADE_PROJECT (gpw->projects->data);
		do_close (gpw, project);
	} 

	gtk_main_quit ();
}

static void
gpw_copy_cb (void)
{
	glade_util_copy_selection ();
}

static void
gpw_cut_cb (void)
{
	glade_util_cut_selection ();
}

static void
gpw_paste_cb (void)
{
	glade_util_paste_clipboard (NULL);
}

static void
gpw_delete_cb (void)
{
	glade_util_delete_selection ();
}

static void
gpw_undo_cb (void)
{
	GladeProjectWindow *gpw;

	gpw = glade_project_window_get ();
	if (!gpw->active_project)
	{
		g_warning ("undo should not be sensitive: we don't have a project");
		return;
	}

	glade_command_undo (gpw->active_project);
	
	glade_editor_refresh (gpw->editor);
	glade_project_window_refresh_undo_redo ();
}

static void
gpw_redo_cb (void)
{
	GladeProjectWindow *gpw;

	gpw = glade_project_window_get ();
	if (!gpw->active_project)
	{
		g_warning ("redo should not be sensitive: we don't have a project");
		return;
	}

	glade_command_redo (gpw->active_project);

	glade_editor_refresh (gpw->editor);
	glade_project_window_refresh_undo_redo ();
}

static gboolean
gpw_hide_palette_on_delete (GtkWidget *palette, gpointer not_used,
		GtkItemFactory *item_factory)
{
	GtkWidget *palette_item;

	glade_util_hide_window (GTK_WINDOW (palette));

	palette_item = gtk_item_factory_get_item (item_factory, "<main>/View/Palette");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (palette_item), FALSE);

	/* Return true so that the palette is not destroyed */
	return TRUE;
}

static void
gpw_palette_button_clicked (GladePalette *palette, gpointer not_used)
{
	GladeProjectWindow *gpw;
	GladeWidgetClass *class;

	g_return_if_fail (GLADE_IS_PALETTE (palette));

	gpw = glade_project_window_get ();
	class = palette->current;

	/* class may be NULL if the selector was pressed */
	if (class && g_type_is_a (class->type, GTK_TYPE_WINDOW))
	{
		glade_command_create (class, NULL, NULL, gpw->active_project);
		glade_palette_unselect_widget (gpw->palette);
		gpw->add_class = NULL;
	}
	else if ((gpw->add_class = class) != NULL)
		gpw->alt_class = class;

}

static void
gpw_palette_catalog_changed (GladePalette *palette, gpointer not_used)
{
	GladeProjectWindow *gpw = glade_project_window_get ();

	g_return_if_fail (GLADE_IS_PALETTE (palette));

	glade_palette_unselect_widget (gpw->palette);
	gpw->alt_class = gpw->add_class = NULL;
}


static void
gpw_create_palette (GladeProjectWindow *gpw)
{
	GtkWidget *palette_item;

	g_return_if_fail (gpw != NULL);

	gpw->palette_window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
	gtk_window_set_default_size (GTK_WINDOW (gpw->palette_window), -1,
				     GLADE_PALETTE_DEFAULT_HEIGHT);
	gpw->palette = glade_palette_new (gpw->catalogs);

	gtk_window_set_title (gpw->palette_window, _("Palette"));
	gtk_window_set_type_hint (gpw->palette_window, GDK_WINDOW_TYPE_HINT_UTILITY);
	gtk_window_set_resizable (gpw->palette_window, TRUE);
	gtk_window_move (gpw->palette_window, 0, 250);

	gtk_container_add (GTK_CONTAINER (gpw->palette_window), GTK_WIDGET (gpw->palette));

	/* Delete event, don't destroy it */
	g_signal_connect (G_OBJECT (gpw->palette_window), "delete_event",
			  G_CALLBACK (gpw_hide_palette_on_delete), gpw->item_factory);

	g_signal_connect (G_OBJECT (gpw->palette), "toggled",
			  G_CALLBACK (gpw_palette_button_clicked), NULL);

	g_signal_connect (G_OBJECT (gpw->palette), "catalog-changed",
			  G_CALLBACK (gpw_palette_catalog_changed), NULL);

	palette_item = gtk_item_factory_get_item (gpw->item_factory,
						  "<main>/View/Palette");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (palette_item), TRUE);
}

static void
gpw_show_palette (GladeProjectWindow *gpw)
{
	GtkWidget *palette_item;

	g_return_if_fail (gpw != NULL);

	if (gpw->palette_window == NULL)
		gpw_create_palette (gpw);

	gtk_widget_show_all (GTK_WIDGET (gpw->palette));
	gtk_widget_show (GTK_WIDGET (gpw->palette_window));

	palette_item = gtk_item_factory_get_item (gpw->item_factory,
						  "<main>/View/Palette");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (palette_item), TRUE);
}

static void
gpw_hide_palette (GladeProjectWindow *gpw)
{
	GtkWidget *palette_item;

	g_return_if_fail (gpw != NULL);

	glade_util_hide_window (gpw->palette_window);

	palette_item = gtk_item_factory_get_item (gpw->item_factory,
						  "<main>/View/Palette");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (palette_item), FALSE);
}

static gboolean
gpw_hide_editor_on_delete (GtkWidget *editor, gpointer not_used,
		GtkItemFactory *item_factory)
{
	GtkWidget *editor_item;

	glade_util_hide_window (GTK_WINDOW (editor));

	editor_item = gtk_item_factory_get_item (item_factory, "<main>/View/Property Editor");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (editor_item), FALSE);

	/* Return true so that the editor is not destroyed */
	return TRUE;
}

static void
gpw_create_editor (GladeProjectWindow *gpw)
{
	GtkWidget *editor_item;

	g_return_if_fail (gpw != NULL);

	if (gpw->editor != NULL)
		return;

	gpw->editor_window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
	gpw->editor = GLADE_EDITOR (glade_editor_new ());
	gpw->editor->project_window = gpw;

	gtk_window_set_title  (gpw->editor_window, _("Properties"));
	gtk_window_move (gpw->editor_window, 350, 0);

	gtk_container_add (GTK_CONTAINER (gpw->editor_window), GTK_WIDGET (gpw->editor));

	/* Delete event, don't destroy it */
	g_signal_connect (G_OBJECT (gpw->editor_window), "delete_event",
			  G_CALLBACK (gpw_hide_editor_on_delete), gpw->item_factory);

	editor_item = gtk_item_factory_get_item (gpw->item_factory,
						 "<main>/View/Property Editor");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (editor_item), TRUE);
}

static void
gpw_show_editor (GladeProjectWindow *gpw)
{
	GtkWidget *editor_item;

	g_return_if_fail (gpw != NULL);

	if (gpw->editor == NULL)
		gpw_create_editor (gpw);
	
	gtk_widget_show_all (GTK_WIDGET (gpw->editor));
	gtk_widget_show (GTK_WIDGET (gpw->editor_window));

	editor_item = gtk_item_factory_get_item (gpw->item_factory,
						 "<main>/View/Property Editor");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (editor_item), TRUE);
}

static void
gpw_hide_editor (GladeProjectWindow *gpw)
{
	GtkWidget *editor_item;

	g_return_if_fail (gpw != NULL);

	glade_util_hide_window (gpw->editor_window);

	editor_item = gtk_item_factory_get_item (gpw->item_factory,
						 "<main>/View/Property Editor");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (editor_item), FALSE);
}

static gboolean
gpw_hide_widget_tree_on_delete (GtkWidget *widget_tree, gpointer not_used,
		GtkItemFactory *item_factory)
{
	GtkWidget *widget_tree_item;

	glade_util_hide_window (GTK_WINDOW (widget_tree));

	widget_tree_item = gtk_item_factory_get_item (item_factory,
						      "<main>/View/Widget Tree");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget_tree_item), FALSE);

	/* return true so that the widget tree is not destroyed */
	return TRUE;
}

static GtkWidget* 
gpw_create_widget_tree (GladeProjectWindow *gpw)
{
	GtkWidget *widget_tree;
	GladeProjectView *view;
	GtkWidget *widget_tree_item;

	widget_tree = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (widget_tree),
				     GLADE_WIDGET_TREE_WIDTH,
				     GLADE_WIDGET_TREE_HEIGHT);
	gtk_window_set_title (GTK_WINDOW (widget_tree), _("Widget Tree"));

	view = glade_project_view_new (GLADE_PROJECT_VIEW_TREE);
	gpw->views = g_list_prepend (gpw->views, view);
	glade_project_view_set_project (view, gpw->active_project);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	gtk_container_add (GTK_CONTAINER (widget_tree), GTK_WIDGET (view));

	g_signal_connect (G_OBJECT (widget_tree), "delete_event",
			  G_CALLBACK (gpw_hide_widget_tree_on_delete), gpw->item_factory);

	widget_tree_item = gtk_item_factory_get_item (gpw->item_factory,
						      "<main>/View/Widget Tree");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget_tree_item), TRUE);

	return widget_tree;
}

static void 
gpw_show_widget_tree (GladeProjectWindow *gpw) 
{
	GtkWidget *widget_tree_item;

	g_return_if_fail (gpw != NULL);

	if (gpw->widget_tree == NULL)
		gpw->widget_tree = gpw_create_widget_tree (gpw);

	gtk_widget_show_all (gpw->widget_tree);

	widget_tree_item = gtk_item_factory_get_item (gpw->item_factory,
						     "<main>/View/Widget Tree");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget_tree_item), TRUE);
}

static void
gpw_hide_widget_tree (GladeProjectWindow *gpw)
{
	GtkWidget *widget_tree_item;

	g_return_if_fail (gpw != NULL);

	glade_util_hide_window (GTK_WINDOW (gpw->widget_tree));

	widget_tree_item = gtk_item_factory_get_item (gpw->item_factory,
						      "<main>/View/Widget Tree");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget_tree_item), FALSE);

}

static void 
gpw_create_clipboard (GladeProjectWindow *gpw)
{
	g_return_if_fail (gpw != NULL);

	if (gpw->clipboard == NULL)
	{
		GladeClipboard *clipboard;

		clipboard = glade_clipboard_new ();
		gpw->clipboard = clipboard;
	}
}

static gboolean
gpw_hide_clipboard_view_on_delete (GtkWidget *clipboard_view, gpointer not_used,
		GtkItemFactory *item_factory)
{
	GtkWidget *clipboard_item;

	glade_util_hide_window (GTK_WINDOW (clipboard_view));

	clipboard_item = gtk_item_factory_get_item (item_factory,
						    "<main>/View/Clipboard");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (clipboard_item), FALSE);

	/* return true so that the clipboard view is not destroyed */
	return TRUE;
}

static void 
gpw_create_clipboard_view (GladeProjectWindow *gpw)
{
	GtkWidget *view;
	GtkWidget *clipboard_item;

	gpw_create_clipboard (gpw);
	
	view = glade_clipboard_view_new (gpw->clipboard);
	gtk_window_set_title (GTK_WINDOW (view), _("Clipboard"));
	g_signal_connect (G_OBJECT (view), "delete_event",
			  G_CALLBACK (gpw_hide_clipboard_view_on_delete),
			  gpw->item_factory);
	gpw->clipboard->view = view;

	clipboard_item = gtk_item_factory_get_item (gpw->item_factory,
						    "<main>/View/Clipboard");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (clipboard_item), TRUE);
}

static void
gpw_show_clipboard_view (GladeProjectWindow *gpw)
{
	GtkWidget *clipboard_item;

	g_return_if_fail (gpw != NULL);

	if (gpw->clipboard == NULL)
		gpw_create_clipboard (gpw);

	if (gpw->clipboard->view == NULL)
		gpw_create_clipboard_view (gpw);
	
	gtk_widget_show_all (GTK_WIDGET (gpw->clipboard->view));

	clipboard_item = gtk_item_factory_get_item (gpw->item_factory,
						    "<main>/View/Clipboard");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (clipboard_item), TRUE);
}

static void
gpw_hide_clipboard_view (GladeProjectWindow *gpw)
{
	GtkWidget *clipboard_item;

	g_return_if_fail (gpw != NULL);

	glade_util_hide_window (GTK_WINDOW (gpw->clipboard->view));

	clipboard_item = gtk_item_factory_get_item (gpw->item_factory,
						    "<main>/View/Clipboard");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (clipboard_item), FALSE);

}

static void
gpw_toggle_palette_cb (void)
{
	GtkWidget *palette_item;

 	GladeProjectWindow *gpw = glade_project_window_get ();
 
	palette_item = gtk_item_factory_get_item (gpw->item_factory,
						  "<main>/View/Palette");

	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (palette_item)))
		gpw_show_palette (gpw);
	else
		gpw_hide_palette (gpw);
}

static void
gpw_toggle_editor_cb (void)
{
	GtkWidget *editor_item;

	GladeProjectWindow *gpw = glade_project_window_get ();

	editor_item = gtk_item_factory_get_item (gpw->item_factory,
						 "<main>/View/Property Editor");

	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (editor_item)))
		gpw_show_editor (gpw);
	else
		gpw_hide_editor (gpw);
}

static void 
gpw_toggle_widget_tree_cb (void) 
{
	GtkWidget *widget_tree_item;

	GladeProjectWindow *gpw = glade_project_window_get ();

	widget_tree_item = gtk_item_factory_get_item (gpw->item_factory,
						      "<main>/View/Widget Tree");

	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (widget_tree_item)))
		gpw_show_widget_tree (gpw);
	else
		gpw_hide_widget_tree (gpw);
}

static void
gpw_toggle_clipboard_cb (void)
{
	GtkWidget *clipboard_item;

	GladeProjectWindow *gpw = glade_project_window_get ();

	clipboard_item = gtk_item_factory_get_item (gpw->item_factory,
						    "<main>/View/Clipboard");

	if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (clipboard_item)))
		gpw_show_clipboard_view (gpw);
	else
		gpw_hide_clipboard_view (gpw);
}

static void gpw_about_cb (void)
{
	GladeProjectWindow *gpw;
	GtkWidget *about_dialog;
	GtkWidget *vbox;
	GtkWidget *glade_image;
	GtkWidget *version;
	GtkWidget *description;
	gchar *filename;

	gpw = glade_project_window_get ();
	about_dialog = gtk_dialog_new_with_buttons (_("About Glade"), GTK_WINDOW (gpw->window), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
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


static GtkItemFactoryEntry menu_items[] =
{
  /* ============ FILE ============ */
  { "/_File", NULL, NULL, 0, "<Branch>" },
  { "/File/_New",        "<control>N",        gpw_new_cb,     1, "<StockItem>", GTK_STOCK_NEW },
  { "/File/_Open",       "<control>O",        gpw_open_cb,    2, "<StockItem>", GTK_STOCK_OPEN },
  { "/File/sep1",        NULL,                NULL,           0, "<Separator>" },
  { "/File/_Save",       "<control>S",        gpw_save_cb,    3, "<StockItem>", GTK_STOCK_SAVE },
  { "/File/Save _As...", "<control><shift>S", gpw_save_as_cb, 4, "<StockItem>", GTK_STOCK_SAVE_AS },
  { "/File/sep2",        NULL,                NULL,           0, "<Separator>" },
  { "/File/_Close",      "<control>W",        gpw_close_cb,   5, "<StockItem>", GTK_STOCK_CLOSE },
  { "/File/_Quit",       "<control>Q",        gpw_quit_cb,    6, "<StockItem>", GTK_STOCK_QUIT },

  /* ============ EDIT ============ */
  { "/Edit", NULL, NULL, 0, "<Branch>" },
  { "/Edit/_Undo",   "<control>Z", gpw_undo_cb,    7, "<StockItem>", GTK_STOCK_UNDO },
  { "/Edit/_Redo",   "<control>R", gpw_redo_cb,    8, "<StockItem>", GTK_STOCK_REDO },
  { "/Edit/sep1",    NULL,         NULL,           0, "<Separator>" },
  { "/Edit/C_ut",    NULL,         gpw_cut_cb,     9, "<StockItem>", GTK_STOCK_CUT },
  { "/Edit/_Copy",   NULL,         gpw_copy_cb,   10, "<StockItem>", GTK_STOCK_COPY },
  { "/Edit/_Paste",  NULL,         gpw_paste_cb,  11, "<StockItem>", GTK_STOCK_PASTE },
  { "/Edit/_Delete", "Delete",     gpw_delete_cb, 12, "<StockItem>", GTK_STOCK_DELETE },

  /* ============ VIEW ============ */
  { "/View", NULL, NULL, 0, "<Branch>" },
  { "/View/_Palette",         NULL, gpw_toggle_palette_cb,     13, "<ToggleItem>" },
  { "/View/Property _Editor", NULL, gpw_toggle_editor_cb,      14, "<ToggleItem>" },
  { "/View/_Widget Tree",     NULL, gpw_toggle_widget_tree_cb, 15, "<ToggleItem>" },
  { "/View/_Clipboard",       NULL, gpw_toggle_clipboard_cb,   16, "<ToggleItem>" },

  /* =========== PROJECT ========== */
  { "/Project", NULL, NULL, 0, "<Branch>" },

  /* ============ HELP ============ */
  { "/_Help",       NULL, NULL, 0, "<Branch>" },
  { "/Help/_About", NULL, gpw_about_cb, 17 },
};

/*
 * Note! The action number in menu_items MUST match the position in this array
 */
static const gchar *menu_tips[] =
{
	NULL,					/* action 0 (branches and separators) */
	N_("Create a new project file"),	/* action 1 (New)  */
	N_("Open a project file"),		/* action 2 (Open) */
	N_("Save the current project file"),	/* action 3 (Save) */
	N_("Save the current project file with a different name"),	/* action 4 (Save as) */
	N_("Close the current project file"),	/* action 5 (Close) */
	N_("Quit the program"),			/* action 6 (Quit) */

	N_("Undo the last action"),		/* action 7  (Undo) */ 
	N_("Redo the last action"),		/* action 8  (Redo) */
	N_("Cut the selection"),		/* action 9  (Cut)  */
	N_("Copy the selection"),		/* action 10 (Copy) */
	N_("Paste the clipboard"),		/* action 11 (Paste) */
	N_("Delete the selection"),		/* action 12 (Delete) */

	N_("Change the visibility of the palette of widgets"),	/* action 13 (Palette) */
	N_("Change the visibility of the property editor"),	/* action 14 (Editor) */
	N_("Change the visibility of the project widget tree"),	/* action 15 (Tree) */
	N_("Change the visibility of the clipboard"),		/* action 16 (Clipboard) */

	N_("About this application"),		/* action 17 (About) */
};

static void
gpw_push_statusbar_hint (GtkWidget* menuitem, const char *tip)
{
	GladeProjectWindow *gpw;

	gpw = glade_project_window_get ();
	gtk_statusbar_push (GTK_STATUSBAR (gpw->statusbar), gpw->statusbar_menu_context_id, tip);
}

static void
gpw_pop_statusbar_hint (GtkWidget* menuitem, gpointer not_used)
{
	GladeProjectWindow *gpw;

	gpw = glade_project_window_get ();
	gtk_statusbar_pop (GTK_STATUSBAR (gpw->statusbar), gpw->statusbar_menu_context_id);
}

static GtkWidget *
gpw_construct_menu (GladeProjectWindow *gpw)
{
	GtkItemFactory *item_factory;
	GtkAccelGroup *accel_group;
	unsigned int i;

	/* Accelerators */
	accel_group = gtk_accel_group_new ();
	gtk_window_add_accel_group (GTK_WINDOW (gpw->window), accel_group);
      
	/* Item factory */
	item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel_group);
	gpw->item_factory = item_factory;
	g_object_ref (G_OBJECT (item_factory));
	gtk_object_sink (GTK_OBJECT (item_factory));
	g_object_set_data_full (G_OBJECT (gpw->window),
				"<main>",
				item_factory,
				(GDestroyNotify) g_object_unref);
	
	/* create menu items */
	gtk_item_factory_create_items (item_factory, G_N_ELEMENTS (menu_items),
				       menu_items, gpw->window);

	/* set the tooltips */
	for (i = 1; i < G_N_ELEMENTS (menu_tips); i++)
	{
		GtkWidget *item;

		item = gtk_item_factory_get_widget_by_action (item_factory, i);
		g_signal_connect (G_OBJECT (item), "select",
				  G_CALLBACK (gpw_push_statusbar_hint),
				  (void*) menu_tips[i]);
		g_signal_connect (G_OBJECT (item), "deselect",
				  G_CALLBACK (gpw_pop_statusbar_hint),
				  NULL);
	}

	return gtk_item_factory_get_widget (item_factory, "<main>");
}

static GtkWidget *
gpw_construct_toolbar (GladeProjectWindow *gpw)
{
	GtkWidget *toolbar;

	toolbar = gtk_toolbar_new ();
	
	gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
				  GTK_STOCK_OPEN,
				  "Open project",
				  NULL,
				  G_CALLBACK (gpw_open_cb),
				  gpw, -1);
	gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
				  GTK_STOCK_SAVE,
				  "Save the current project",
				  NULL,
				  G_CALLBACK (gpw_save_cb),
				  gpw, -1);
	gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));
	gpw->toolbar_undo = gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
						      GTK_STOCK_UNDO,
						      "Undo the last action",
						      NULL,
						      G_CALLBACK (gpw_undo_cb),
						      gpw, -1);
	gpw->toolbar_redo = gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
						      GTK_STOCK_REDO,
						      "Redo the last action",
						      NULL,
						      G_CALLBACK (gpw_redo_cb),
						      gpw, -1);

	return toolbar;	
}

static GtkWidget *
gpw_construct_project_view (GladeProjectWindow *gpw)
{
	GladeProjectView *view;

	view = glade_project_view_new (GLADE_PROJECT_VIEW_LIST);
	gpw->views = g_list_prepend (NULL, view);
	gpw->active_view = view;

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	return GTK_WIDGET (view);
}

static GtkWidget *
gpw_construct_statusbar (GladeProjectWindow *gpw)
{
	GtkWidget *statusbar;

	statusbar = gtk_statusbar_new ();
	gpw->statusbar = statusbar;
	gpw->statusbar_menu_context_id =
		gtk_statusbar_get_context_id (GTK_STATUSBAR (statusbar), "menu");
	gpw->statusbar_actions_context_id =
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
			guint info, guint time, gpointer data)
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
			glade_project_window_open_project (list->data);

		/* we can now free each string in the list */
		g_free (list->data);
	}

	g_list_free (list);
}

static gboolean
gpw_delete_event (GtkWindow *w, gpointer not_used)
{
	gpw_quit_cb ();
	
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
	gpw->window = app;

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (app), vbox);
	gpw->main_vbox = vbox;

	menubar = gpw_construct_menu (gpw);
	toolbar = gpw_construct_toolbar (gpw);
	project_view = gpw_construct_project_view (gpw);
	statusbar = gpw_construct_statusbar (gpw);

	gtk_box_pack_start (GTK_BOX (vbox), menubar, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), toolbar, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), project_view, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (vbox), statusbar, FALSE, TRUE, 0);

	glade_project_window_refresh_undo_redo ();

	/* support for opening a file by dragging onto the project window */
	gtk_drag_dest_set (app,
			   GTK_DEST_DEFAULT_ALL,
			   drop_types, G_N_ELEMENTS (drop_types),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect (G_OBJECT (app), "drag-data-received",
			  G_CALLBACK (gpw_drag_data_received), NULL);

	g_signal_connect (G_OBJECT (app), "delete_event",
			  G_CALLBACK (gpw_delete_event), NULL);
}

static GladeProjectWindow *glade_project_window = NULL;

/**
 * glade_project_window_get:
 *
 * Returns: the #GladeProjectWindow
 */
GladeProjectWindow *
glade_project_window_get (void)
{
	return glade_project_window;
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
glade_project_window_new (GList *catalogs)
{
	GladeProjectWindow *gpw;
	
	gpw = g_new0 (GladeProjectWindow, 1);
	gpw->catalogs = catalogs;
	gpw->add_class = NULL;
	gpw->alt_class = NULL;

	glade_project_window = gpw;

	glade_project_window_create (gpw);
	gpw_create_palette (gpw);
	gpw_create_editor  (gpw);
	gpw_create_clipboard (gpw);

	return gpw;
}

static void
gpw_widget_name_changed_cb (GladeProject *project,
			    GladeWidget *widget,
			    GladeEditor *editor)
{
	glade_editor_update_widget_name (editor);
}

static void
gpw_project_selection_changed_cb (GladeProject *project,
				  GladeProjectWindow *gpw)
{
	GList *list;
	gint num;

	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GLADE_IS_PROJECT_WINDOW (gpw));

	if (gpw->active_project != project)
	{
		glade_project_window_set_project (project);
		return;
	}

	if (gpw->editor)
	{
		list = glade_project_selection_get (project);
		num = g_list_length (list);
		if (num == 1 && !GLADE_IS_PLACEHOLDER (list->data))
			glade_editor_load_widget
				(gpw->editor,
				 glade_widget_get_from_gobject (list->data));
		else
			glade_editor_load_widget (gpw->editor, NULL);
	}
}

static void
glade_project_window_add_project (GladeProject *project)
{
	GladeProjectWindow *gpw;
	GList *list;
	char *underscored_name;

 	g_return_if_fail (GLADE_IS_PROJECT (project));

	gpw = glade_project_window_get ();

	/* If the project was previously loaded, don't re-load */
	for (list = gpw->projects; list; list = list->next)
	{
		GladeProject *cur_project = GLADE_PROJECT (list->data);

		if (cur_project->path && project->path)
		{
			if (!strcmp (cur_project->path, project->path))
			{
				glade_project_window_set_project (cur_project);
				return;
			}
		}
	}

 	gpw->projects = g_list_prepend (gpw->projects, project);

	/* double the underscores in the project name
	 * (or they will just underscore the next character) */
	underscored_name = glade_util_duplicate_underscores (project->name);
	if (!underscored_name)
		return;

 	/* Add the project in the /Project menu. */
	project->entry.path = g_strdup_printf ("/Project/%s", underscored_name);
	g_free (underscored_name);
	project->entry.accelerator = NULL;
	project->entry.callback = (GtkItemFactoryCallback)glade_project_window_set_project;
	project->entry.callback_action = 0;
	project->entry.item_type = g_strdup ("<Item>");

	gtk_item_factory_create_item (gpw->item_factory, &(project->entry), project, 1);

	/* connect to the project signals so that the editor can be updated */
	g_signal_connect (G_OBJECT (project), "widget_name_changed",
			  G_CALLBACK (gpw_widget_name_changed_cb), gpw->editor);
	g_signal_connect (G_OBJECT (project), "selection_changed",
			  G_CALLBACK (gpw_project_selection_changed_cb), gpw);

	/* make sure the palette is sensitive */
	gtk_widget_set_sensitive (GTK_WIDGET (gpw->palette), TRUE);

	glade_project_window_set_project (project);
}

/**
 * glade_project_window_new_project:
 *
 * Creates a new #GladeProject and adds it to the #GladeProjectWindow.
 */
void
glade_project_window_new_project (void)
{
	GladeProject *project;

	project = glade_project_new (TRUE);
	if (!project)
	{
		GladeProjectWindow *gpw;

		gpw = glade_project_window_get ();
		glade_util_ui_warn (gpw->window, _("Could not create a new project."));

		return;
	}

	glade_project_window_add_project (project);
}

/**
 * glade_project_window_open_project:
 * @path: a string containing a filename
 *
 * Opens the project specified by @path and adds it to the #GladeProjectWindow.
 */
void
glade_project_window_open_project (const gchar *path)
{
	GladeProject *project;

	g_return_if_fail (path != NULL);

	project = glade_project_open (path);
	if (!project)
	{
		GladeProjectWindow *gpw;

		gpw = glade_project_window_get ();
		glade_util_ui_warn (gpw->window, _("Could not open project."));

		return;
	}

	glade_project_window_add_project (project);
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
	
	bin = GTK_BIN (gtk_item_factory_get_item (gpw->item_factory, path));
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
glade_project_window_refresh_undo_redo (void)
{
	GladeProjectWindow *gpw;
	GList *prev_redo_item;
	GList *undo_item;
	GList *redo_item;
	const gchar *undo_description = NULL;
	const gchar *redo_description = NULL;
	GladeProject *project;

	gpw = glade_project_window_get ();

	project = gpw->active_project;
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

	glade_project_window_change_menu_label (gpw, "/Edit/Undo", _("_Undo: "), undo_description);
	glade_project_window_change_menu_label (gpw, "/Edit/Redo", _("_Redo: "), redo_description);

	gtk_widget_set_sensitive (gpw->toolbar_undo, undo_description != NULL);
	gtk_widget_set_sensitive (gpw->toolbar_redo, redo_description != NULL);
}

/**
 * glade_project_window_show_all:
 *
 * TODO: write me
 */
void
glade_project_window_show_all (void)
{
	GladeProjectWindow *gpw;

	gpw = glade_project_window_get ();

	gtk_widget_show_all (gpw->window);
	gpw_show_palette (gpw);
	gpw_show_editor (gpw);
}

GladeProject *
glade_project_window_get_active_project (GladeProjectWindow *gpw)
{
	return gpw->active_project;
}
