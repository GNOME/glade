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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "glade.h"
#include "glade-palette.h"
#include "glade-editor.h"
#include "glade-clipboard.h"
#include "glade-clipboard-view.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-parameter.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-project.h"
#include "glade-project-view.h"
#include "glade-project-window.h"
#include "glade-command.h"
#include "glade-debug.h"
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkstock.h>

static void gpw_new_cb (void);
static void gpw_open_cb (void);
static void gpw_save_cb (void);
static void gpw_save_as_cb (void);
static void gpw_quit_cb (void);

static void gpw_toggle_palette_cb (void);
static void gpw_toggle_editor_cb (void);
static void gpw_toggle_widget_tree_cb (void);
static void gpw_toggle_clipboard_cb (void);

static void gpw_undo_cb (void);
static void gpw_redo_cb (void);
static void gpw_about_cb (void) {}

static void
gpw_open_cb (void)
{
	glade_project_open ();
}

static void
gpw_delete_cb (void)
{
	GladeProject *project;
	GList *selection;
	GList *free_me;
	GList *list;

	project = glade_project_window_get_project ();
	if (!project) {
		g_warning ("Why is delete sensitive ? it shouldn't not be because "
			   "we don't have a project");
		return;
	}
	
	selection = glade_project_selection_get (project);

	/* We have to be carefull when deleting widgets from the selection
	 * because when we delete each widget, the ->selection pointer changes
	 * by the g_list_remove. Copy the list and free it after we are done
	 */
	list = g_list_copy (selection);
	free_me = list;
	for (; list; list = list->next)
		glade_widget_delete (list->data);
	g_list_free (free_me);

	/* Right now deleting widgets like this is not a problem, cause we
	 * don't support multiple selection. When we do, it would be nice
	 * to have glade_project_selction_freeze the remove all the widgets
	 * and then call glade_project_selection_thaw. This will trigger
	 * only one selection changed signal rather than multiple ones
	 * Chema
	 */
}

static void
gpw_new_cb (void)
{
	GladeProjectWindow *gpw;
	GladeProject *project;

	project = glade_project_new (TRUE);
	gpw = glade_project_window_get ();
	glade_project_window_add_project (gpw, project);
}

static void
gpw_save_cb (void)
{
	GladeProjectWindow *gpw;
	GladeProject *project;

	gpw = glade_project_window_get ();
	project = glade_project_window_get_project ();
	if (glade_project_save (project))
		glade_project_window_refresh_title (gpw);
}

static void
gpw_save_as_cb (void)
{
	GladeProjectWindow *gpw;
	GladeProject *project;

	gpw = glade_project_window_get ();
	project = glade_project_window_get_project ();
	if (glade_project_save_as (project))
		glade_project_window_refresh_title (gpw);
}

static void
gpw_quit_cb (void)
{
	gtk_main_quit ();
}

static void
gpw_copy_cb (void)
{
	GladeProjectWindow *gpw;
	GladeWidget *widget;

	gpw = glade_project_window_get ();
	widget = gpw->active_widget;

	if (widget)
		glade_clipboard_copy (gpw->clipboard, widget);
}

static void
gpw_cut_cb (void)
{
	GladeProjectWindow *gpw;
	GladeWidget *widget;

	gpw = glade_project_window_get ();
	widget = gpw->active_widget;

	if (widget)
		glade_clipboard_cut (gpw->clipboard, widget);
}

static void
gpw_paste_cb (void)
{
	GladeProjectWindow *gpw;

	gpw = glade_project_window_get ();
	glade_clipboard_paste (gpw->clipboard, gpw->active_placeholder);
}

static void
gpw_undo_cb (void)
{
	glade_command_undo ();
}

static void
gpw_redo_cb (void)
{
	glade_command_redo ();
}

static void
gpw_delete_event (GtkWindow *w, gpointer not_used)
{
	gpw_quit_cb ();
}


static GtkItemFactoryEntry menu_items[] =
{
  /* ============ FILE ===================== */
  { "/_File",            NULL,         0,              0, "<Branch>" },
  { "/File/_New",        "<control>N", gpw_new_cb,     0, "<StockItem>", GTK_STOCK_NEW },
  { "/File/_Open",       "<control>O", gpw_open_cb,    0, "<StockItem>", GTK_STOCK_OPEN },
  { "/File/_Save",       "<control>S", gpw_save_cb,    0, "<StockItem>", GTK_STOCK_SAVE },
  { "/File/Save _As...", NULL,         gpw_save_as_cb, 0, "<StockItem>", GTK_STOCK_SAVE },
  { "/File/sep1",        NULL,         NULL,           0, "<Separator>" },
  { "/File/_Quit",       "<control>Q", gpw_quit_cb,    0, "<StockItem>", GTK_STOCK_QUIT },

  /* ============ EDIT ===================== */
  { "/Edit",        NULL, 0, 0, "<Branch>" },
  { "/Edit/_Undo",   "<control>Z", gpw_undo_cb,   0, "<StockItem>", GTK_STOCK_UNDO },
  { "/Edit/_Redo",   "<control>R", gpw_redo_cb,   0, "<StockItem>", GTK_STOCK_REDO },
  { "/Edit/sep1",    NULL,         NULL,          0, "<Separator>" },
  { "/Edit/C_ut",    NULL,         gpw_cut_cb,    0, "<StockItem>", GTK_STOCK_CUT },
  { "/Edit/_Copy",   NULL,         gpw_copy_cb,   0, "<StockItem>", GTK_STOCK_COPY },
  { "/Edit/_Paste",  NULL,         gpw_paste_cb,  0, "<StockItem>", GTK_STOCK_PASTE },
  { "/Edit/_Delete", "Delete",     gpw_delete_cb, 0, "<StockItem>", GTK_STOCK_DELETE },

  /* ============ VIEW ===================== */
  { "/View", NULL, 0, 0, "<Branch>" },
  { "/View/_Palette",         NULL, gpw_toggle_palette_cb,     0, "<ToggleItem>" },
  { "/View/Property _Editor", NULL, gpw_toggle_editor_cb,      0, "<ToggleItem>" },
  { "/View/_Widget Tree",     NULL, gpw_toggle_widget_tree_cb, 0, "<ToggleItem>" },
  { "/View/_Clipboard",       NULL, gpw_toggle_clipboard_cb,   0, "<ToggleItem>" },

  /* ============ PROJECT=================== */
  { "/Project", NULL, 0, 0, "<Branch>" },

  /* ============ HELP ===================== */
  { "/_Help",       NULL, NULL, 0, "<LastBranch>" },
  { "/Help/_About", NULL, gpw_about_cb, 0 },
};


static void
glade_project_window_construct_menu (GladeProjectWindow *gpw)
{
	GtkItemFactory *item_factory;
	GtkAccelGroup *accel_group;

	/* Accelerators */
	accel_group = gtk_accel_group_new ();
	gtk_window_add_accel_group (GTK_WINDOW (gpw->window), accel_group);
      
	/* Item factory */
	item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel_group);
	gpw->item_factory = item_factory;
	gtk_object_ref (GTK_OBJECT (item_factory));
	gtk_object_sink (GTK_OBJECT (item_factory));
	g_object_set_data_full (G_OBJECT (gpw->window),
				"<main>",
				item_factory,
				(GDestroyNotify) g_object_unref);
	
	/* create menu items */
	gtk_item_factory_create_items (item_factory, G_N_ELEMENTS (menu_items),
				       menu_items, gpw->window);
	gtk_box_pack_start_defaults (GTK_BOX (gpw->main_vbox),
				     gtk_item_factory_get_widget (item_factory, "<main>"));
}

static void
glade_project_window_construct_toolbar (GladeProjectWindow *gpw)
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
	gtk_toolbar_insert_stock (GTK_TOOLBAR (toolbar),
				  GTK_STOCK_QUIT,
				  "Quit glade",
				  NULL,
				  G_CALLBACK (gpw_quit_cb),
				  gpw, -1);

	gtk_widget_show (toolbar);
	
	gtk_box_pack_start_defaults (GTK_BOX (gpw->main_vbox),
				     toolbar);
}

static gboolean
gpw_hide_palette_on_delete (GtkWidget *palette, gpointer not_used,
		GtkItemFactory *item_factory)
{
	GtkWidget *palette_item;

	gtk_widget_hide (GTK_WIDGET (palette));

	palette_item = gtk_item_factory_get_item (item_factory, "<main>/View/Palette");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (palette_item), FALSE);

	/* Return true so that the palette is not destroyed */
	return TRUE;
}

static void
gpw_create_palette (GladeProjectWindow *gpw)
{
	GtkWidget *palette_item;

	g_return_if_fail (gpw != NULL);

	gpw->palette_window = GTK_WINDOW (gtk_window_new (GTK_WINDOW_TOPLEVEL));
	gpw->palette = glade_palette_new (gpw->catalogs);
	gpw->palette->project_window = gpw;

	gtk_window_set_title (gpw->palette_window, _("Palette"));
	gtk_window_set_resizable (gpw->palette_window, FALSE);

	gtk_container_add (GTK_CONTAINER (gpw->palette_window), GTK_WIDGET (gpw->palette));

	/* Delete event, don't destroy it */
	g_signal_connect (G_OBJECT (gpw->palette_window), "delete_event",
			  G_CALLBACK (gpw_hide_palette_on_delete), gpw->item_factory);

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

	gtk_widget_hide (GTK_WIDGET (gpw->palette_window));

	palette_item = gtk_item_factory_get_item (gpw->item_factory,
						  "<main>/View/Palette");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (palette_item), FALSE);
}

static gboolean
gpw_hide_editor_on_delete (GtkWidget *editor, gpointer not_used,
		GtkItemFactory *item_factory)
{
	GtkWidget *editor_item;

	gtk_widget_hide (GTK_WIDGET (editor));

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

	gtk_widget_hide (GTK_WIDGET (gpw->editor_window));

	editor_item = gtk_item_factory_get_item (gpw->item_factory,
						 "<main>/View/Property Editor");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (editor_item), FALSE);
}

static gboolean 
gpw_key_press_widget_tree_cb (GtkWidget *widget_tree, GdkEventKey *event,
		GtkItemFactory *item_factory)
{

	GtkWidget *widget_tree_item;

	if (event->keyval == GDK_Escape) {
		gtk_widget_hide (widget_tree);
		widget_tree_item = gtk_item_factory_get_item (item_factory,
							      "<main>/View/Widget Tree");
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget_tree_item),
						FALSE);

		return TRUE;
	}
	return FALSE;
}

static gboolean
gpw_hide_widget_tree_on_delete (GtkWidget *widget_tree, gpointer not_used,
		GtkItemFactory *item_factory)
{
	GtkWidget *widget_tree_item;

	gtk_widget_hide (widget_tree);

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
	gtk_window_set_title (GTK_WINDOW (widget_tree), _("Widget Tree"));
	view = glade_project_view_new (GLADE_PROJECT_VIEW_TREE);
	gtk_container_add (GTK_CONTAINER (widget_tree),
			   glade_project_view_get_widget (view));
	gpw->views = g_list_prepend (gpw->views, view);
	glade_project_view_set_project (view, gpw->project);
	g_signal_connect (G_OBJECT (widget_tree), "delete_event",
			  G_CALLBACK (gpw_hide_widget_tree_on_delete), gpw->item_factory);
	g_signal_connect (G_OBJECT (widget_tree), "key_press_event",
			  G_CALLBACK (gpw_key_press_widget_tree_cb), gpw->item_factory);

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

	gtk_widget_hide (GTK_WIDGET (gpw->widget_tree));

	widget_tree_item = gtk_item_factory_get_item (gpw->item_factory,
						      "<main>/View/Widget Tree");
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget_tree_item), FALSE);

}

static void 
gpw_create_clipboard (GladeProjectWindow *gpw)
{
	g_return_if_fail (gpw != NULL);

	if (gpw->clipboard == NULL) {
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

	gtk_widget_hide (clipboard_view);

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

	gtk_widget_hide (GTK_WIDGET (gpw->clipboard->view));

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

static GtkWidget *
glade_project_window_create (GladeProjectWindow *gpw, GladeProjectView *view)
{
	GtkWidget *app;
	GtkWidget *vbox;

	g_return_val_if_fail (GLADE_IS_PROJECT_VIEW (view), NULL);

	app = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gpw->window = app;
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (app), vbox);
	gpw->main_vbox = vbox;

	glade_project_window_construct_menu (gpw);
	glade_project_window_construct_toolbar (gpw);
	glade_project_window_refresh_undo_redo (gpw);
	
	g_signal_connect (G_OBJECT (app), "delete_event",
			  G_CALLBACK (gpw_delete_event), NULL);

	return app;
}

GladeProjectWindow * glade_project_window = NULL;

GladeProjectWindow *
glade_project_window_get ()
{
	return glade_project_window;
}

static void
glade_project_window_set_view (GladeProjectWindow *gpw, GladeProjectView *view)
{
	if (gpw->active_view == view)
		return;
	
	gpw->active_view = view;

	gtk_box_pack_start_defaults (GTK_BOX (gpw->main_vbox),
				     glade_project_view_get_widget (view));

}

GladeProjectWindow *
glade_project_window_new (GList *catalogs)
{
	GladeProjectWindow *gpw;
	GladeProjectView *view;

	view = glade_project_view_new (GLADE_PROJECT_VIEW_LIST);
	
	gpw = g_new0 (GladeProjectWindow, 1);
	glade_project_window_create (gpw, view);
	gpw->catalogs = catalogs;
	gpw->views = g_list_prepend (NULL, view);
	gpw->add_class = NULL;
	gpw->active_widget = NULL;
	gpw->active_placeholder = NULL;
	glade_project_window_set_view (gpw, view);

	glade_project_window = gpw;
	gpw_create_palette (gpw);
	gpw_create_editor  (gpw);
	gpw_create_clipboard (gpw);
	
	return gpw;
}

static void
glade_project_window_selection_changed_cb (GladeProject *project,
					   GladeProjectWindow *gpw)
{
	GList *list;
	gint num;

	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GLADE_IS_PROJECT_WINDOW (gpw));

	if (gpw->editor) {
		list = project->selection;
		num = g_list_length (list);
		if (num == 1) {
			glade_editor_select_widget (gpw->editor, list->data);
			gpw->active_widget = list->data;
		} else {
			glade_editor_select_widget (gpw->editor, NULL);
		}
	}
}

void
glade_project_window_set_project (GladeProjectWindow *gpw, GladeProject *project)
{
	GladeProjectView *view;
	GList *list;

	if (g_list_find (gpw->projects, project) == NULL) {
		g_warning ("Could not set project because it could not "
			   " be found in the gpw->project list\n");
		return;
	}
	
	gpw->project = project;
	if (project) {
		glade_project_window_refresh_title (gpw);
	} else {
		gtk_window_set_title (GTK_WINDOW (gpw->window), "glade3");
	}

	list = gpw->views;
	for (; list != NULL; list = list->next) {
		view = list->data;
		glade_project_view_set_project (view, project);
	}

	gpw->project_selection_changed_signal =
		g_signal_connect (G_OBJECT (project), "selection_changed",
				  G_CALLBACK (glade_project_window_selection_changed_cb),
				  gpw);
	
	glade_project_selection_changed (project);
}

void
glade_project_window_set_project_cb (GladeProject *project)
{
	GladeProjectWindow *gpw;

	gpw = glade_project_window_get ();
	glade_project_window_set_project (gpw, project);
}
		
void
glade_project_window_add_project (GladeProjectWindow *gpw, GladeProject *project)
{
	g_return_if_fail (GLADE_IS_PROJECT_WINDOW (gpw));
	g_return_if_fail (GLADE_IS_PROJECT (project));
	
	gpw->projects = g_list_prepend (gpw->projects, project);

	/* Add the project in the /Project menu. */
	gtk_item_factory_create_item (gpw->item_factory, &(project->entry),
				      project, 1);

	glade_project_window_set_project (gpw, project);
}

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
	
	if (prefix == NULL) {
		gtk_label_set_text_with_mnemonic (label, suffix);
                return;
	}

	if (suffix == NULL) {
		suffix = _("Nothing");
		sensitive = FALSE;
	}

	text = g_strconcat (prefix, suffix, NULL);

	gtk_label_set_text_with_mnemonic (label, text);
	gtk_widget_set_sensitive (GTK_WIDGET (bin), sensitive);
	
	g_free (text);
}

void
glade_project_window_refresh_undo_redo (GladeProjectWindow *gpw)
{
	GtkWidget *undo_widget;
	GtkWidget *redo_widget;
	GList *prev_redo_item;
	GList *undo_item;
	GList *redo_item;
	const gchar *undo_description = NULL;
	const gchar *redo_description = NULL;
	GladeProject *project;
	
	g_return_if_fail (GLADE_IS_PROJECT_WINDOW (gpw));
	
	undo_widget = gtk_item_factory_get_item (gpw->item_factory, "<main>/Edit/Undo");
	redo_widget = gtk_item_factory_get_item (gpw->item_factory, "<main>/Edit/Redo");

	project = gpw->project;
	if (project == NULL) {
		undo_item = NULL;
		redo_item = NULL;
	}
	else {
		undo_item = prev_redo_item = project->prev_redo_item;
		redo_item = (prev_redo_item == NULL) ? project->undo_stack : prev_redo_item->next;
	}

	undo_description = glade_command_get_description (undo_item);
	redo_description = glade_command_get_description (redo_item);

	glade_project_window_change_menu_label (gpw, "/Edit/Undo", _("_Undo: "), undo_description);
	glade_project_window_change_menu_label (gpw, "/Edit/Redo", _("_Redo: "), redo_description);
}

void
glade_project_window_refresh_title (GladeProjectWindow *gpw)
{
	gchar *title;
	title = g_strdup_printf ("glade3 : %s", gpw->project->name);
	gtk_window_set_title (GTK_WINDOW (gpw->window), title);
	g_free (title);
}

void
glade_project_window_set_add_class (GladeProjectWindow *gpw, GladeWidgetClass *class)
{
	gpw->add_class = class;

	if (!class && gpw->palette)
		glade_palette_clear (gpw);
	
}

static GtkWidget *
glade_project_append_query (GtkWidget *table, GladePropertyClass *property_class, gint row)
{
	GladePropertyQuery *query;
	GtkAdjustment *adjustment;
	GtkWidget *label;
	GtkWidget *spin;
	gchar *text;

	query = property_class->query;
	
	if (property_class->type != GLADE_PROPERTY_TYPE_INTEGER) {
		g_warning ("We can only query integer types for now. Trying to query %d. FIXME please ;-)", property_class->type);
		return NULL;
	}
	
	/* Label */
	text = g_strdup_printf ("%s :", query->question);
	label = gtk_label_new (text);
	g_free (text);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), label,
				   0, 1, row, row +1);

	/* Spin/Entry */
	adjustment = glade_parameter_adjustment_new (property_class->parameters, property_class->def);
	spin = gtk_spin_button_new (adjustment, 1, 0);
	gtk_widget_show (spin);
	gtk_table_attach_defaults (GTK_TABLE (table), spin,
				   1, 2, row, row +1);

	return spin;
}

void
glade_project_window_query_properties_set (gpointer key_,
					   gpointer value_,
					   gpointer user_data)
{
	GladePropertyQueryResult *result = user_data;
	GtkWidget *spin = value_;
	const gchar *key = key_;
	gint num;

	num = (gint) gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin));
	glade_property_query_result_set_int (result, key, num);
}

/**
 * glade_project_window_query_properties:
 * @class: 
 * @result: 
 * 
 * Queries the user for some property values before a GladeWidget creation
 * for example before creating a GtkVBox we want to ask the user the number
 * of columns he wants.
 * 
 * Return Value: FALSE if the query was canceled
 **/
gboolean 
glade_project_window_query_properties (GladeWidgetClass *class,
				       GladePropertyQueryResult *result)
{
	GladePropertyClass *property_class;
	GHashTable *hash;
	GtkWidget *dialog;
	GtkWidget *table;
	GtkWidget *vbox;
	GtkWidget *spin = NULL;
	GList *list;
	gint response;
	gint row = 0;

	g_return_val_if_fail (class  != NULL, FALSE);
	g_return_val_if_fail (result != NULL, FALSE);

	dialog = gtk_dialog_new_with_buttons (NULL /* name */,
					      NULL /* parent, FIXME: parent should be the project window */,
					      GTK_DIALOG_MODAL,
					      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
					      NULL);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);	
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);	

	vbox = GTK_DIALOG (dialog)->vbox;
	table = gtk_table_new (0, 0, FALSE);
	gtk_widget_show (table);
	gtk_box_pack_start_defaults (GTK_BOX (vbox), table);
	
	hash = g_hash_table_new (g_str_hash, g_str_equal);

	list = class->properties;
	for (; list != NULL; list = list->next) {
		property_class = list->data;
		if (property_class->query) {
			spin = glade_project_append_query (table, property_class, row++);
			g_hash_table_insert (hash, property_class->id, spin);
		}
	}
	if (spin == NULL)
		return TRUE;

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	switch (response) {
	case GTK_RESPONSE_ACCEPT:
		g_hash_table_foreach (hash,
				      glade_project_window_query_properties_set,
				      result);
		break;
	case GTK_RESPONSE_REJECT:
		gtk_widget_destroy (dialog);
		return TRUE;
	default:
		g_warning ("Dunno what to do, unexpected GtkResponse");
	}

	g_hash_table_destroy (hash);
	gtk_widget_destroy (dialog);
	
	return FALSE;
}

GladeProject *
glade_project_window_get_project (void)
{
	GladeProjectWindow *gpw;

	gpw = glade_project_window_get ();
	g_return_val_if_fail (GLADE_IS_PROJECT_WINDOW (gpw), NULL);

	return gpw->project;
}

void
glade_project_window_show_all (GladeProjectWindow *gpw)
{
	gtk_widget_show_all (gpw->window);
	gpw_show_palette (gpw);
	gpw_show_editor (gpw);
}
