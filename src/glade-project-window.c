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
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-parameter.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-project.h"
#include "glade-project-view.h"
#include "glade-project-window.h"
#include <gdk/gdkkeysyms.h>

static void gpw_new_cb (void);
static void gpw_open_cb (void);
static void gpw_save_cb (void);
static void gpw_save_as_cb (void);
static void gpw_quit_cb (void);

static void gpw_show_palette_cb (void);
static void gpw_show_editor_cb (void);
static void gpw_show_widget_tree_cb (void);
static void gpw_show_clipboard_cb (void) {};

static void gpw_undo_cb (void);
static void gpw_redo_cb (void);
static void gpw_about_cb (void) {};

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
	 * by the g_list_remove. Copy the list and freee it after we are done
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
	GladeProject *project;

	project = glade_project_window_get_project ();
	glade_project_save (project);
}

static void
gpw_save_as_cb (void)
{
	glade_implement_me ();
}

static void
gpw_quit_cb (void)
{
	gtk_main_quit ();
}

static void
gpw_copy_cb (void)
{
	glade_implement_me ();
}

static void
gpw_cut_cb (void)
{
	glade_implement_me ();
}

static void
gpw_paste_cb (void)
{
	glade_implement_me ();
}

static void
gpw_undo_cb (void)
{
	glade_implement_me ();
}

static void
gpw_redo_cb (void)
{
	glade_implement_me ();
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
  { "/Edit/_Delete", NULL,         gpw_delete_cb, 0, "<StockItem>", GTK_STOCK_DELETE },

  /* ============ VIEW ===================== */
  { "/View", NULL, 0, 0, "<Branch>" },
  { "/View/Show _Palette",         NULL, gpw_show_palette_cb,     0, "<Item>" },
  { "/View/Show Property _Editor", NULL, gpw_show_editor_cb,      0, "<Item>" },
  { "/View/Show _Widget Tree",     NULL, gpw_show_widget_tree_cb, 0, "<Item>" },
  { "/View/Show _Clipboard",       NULL, gpw_show_clipboard_cb,   0, "<Item>" },

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
	
	gtk_signal_connect (GTK_OBJECT (app), "delete_event",
			    GTK_SIGNAL_FUNC (gpw_delete_event), NULL);

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
glade_project_window_new (GList *catalog)
{
	GladeProjectWindow *gpw;
	GladeProjectView *view;

	view = glade_project_view_new (GLADE_PROJECT_VIEW_LIST);
	
	gpw = g_new0 (GladeProjectWindow, 1);
	glade_project_window_create (gpw, view);
	gpw->catalog = catalog;
	gpw->views = g_list_prepend (NULL, view);
	gpw->add_class = NULL;

	glade_project_window_set_view (gpw, view);

	glade_project_window = gpw;
	glade_palette_create (gpw);
	glade_editor_create  (gpw);
	
	return gpw;
}

static gboolean 
gpw_key_press_widget_tree_cb (GtkWidget *widget_tree, GdkEventKey *event,
		gpointer not_used)
{
	if (event->keyval == GDK_Escape) {
		gtk_widget_hide (widget_tree);
		return TRUE;
	}
	return FALSE;
}

static gboolean
gpw_delete_widget_tree_cb (GtkWidget *widget_tree, gpointer not_used)
{
	gtk_widget_hide (widget_tree);

	/* return true so that the widget tree is not destroyed */
	return TRUE;
}

static GtkWidget* 
glade_project_window_widget_tree_create (GladeProjectWindow *gpw)
{
	GtkWidget* widget_tree;
	GladeProjectView *view;
	
	widget_tree = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (widget_tree), _("Widget Tree"));
	view = glade_project_view_new (GLADE_PROJECT_VIEW_TREE);
	gtk_container_add (GTK_CONTAINER (widget_tree),
			   glade_project_view_get_widget (view));
	gpw->views = g_list_prepend (gpw->views, view);
	glade_project_view_set_project (view, gpw->project);
	gtk_signal_connect (GTK_OBJECT (widget_tree), "delete_event",
			GTK_SIGNAL_FUNC (gpw_delete_widget_tree_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (widget_tree), "key_press_event",
			GTK_SIGNAL_FUNC (gpw_key_press_widget_tree_cb), NULL);

	return widget_tree;
}

static void 
gpw_show_widget_tree (GladeProjectWindow *gpw) 
{
	if (gpw->widget_tree == NULL) 
		gpw->widget_tree = glade_project_window_widget_tree_create (gpw);
	
	gtk_widget_show_all (gpw->widget_tree);
}

static void 
gpw_show_widget_tree_cb (void) 
{
	GladeProjectWindow *gpw = glade_project_window_get ();

	gpw_show_widget_tree (gpw);
}

static void
gpw_show_palette_cb (void)
{
	GladeProjectWindow *gpw = glade_project_window_get ();

	glade_palette_show (gpw);
}

static void
gpw_show_editor_cb (void)
{
	GladeProjectWindow *gpw = glade_project_window_get ();

	glade_editor_show (gpw);
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
		if (num == 1)
			glade_editor_select_widget (gpw->editor, list->data);
		else
			glade_editor_select_widget (gpw->editor, NULL);
	}

}

void
glade_project_window_set_project (GladeProjectWindow *gpw, GladeProject *project)
{
	GladeProjectView *view;
	GList *list;
	gchar *title;

	if (g_list_find (gpw->projects, project) == NULL) {
		g_warning ("Could not set project because it could not "
			   " be found in the gpw->project list\n");
		return;
	}
	
	gpw->project = project;
	if (project) {
		title = g_strdup_printf ("glade2 : %s", project->name);
		gtk_window_set_title (GTK_WINDOW (gpw->window), title);
		g_free (title);
	} else {
		gtk_window_set_title (GTK_WINDOW (gpw->window), "glade 2");
	}

	list = gpw->views;
	for (; list != NULL; list = list->next) {
		view = list->data;
		glade_project_view_set_project (view, project);
	}

	gpw->project_selection_changed_signal =
		gtk_signal_connect (GTK_OBJECT (project), "selection_changed",
				    GTK_SIGNAL_FUNC (glade_project_window_selection_changed_cb),
				    gpw);
	
	glade_project_selection_changed (project);
}

static void
glade_project_window_set_project_cb (GladeProject *project)
{
	GladeProjectWindow *gpw;

	gpw = glade_project_window_get ();
	glade_project_window_set_project (gpw, project);
}
		
void
glade_project_window_add_project (GladeProjectWindow *gpw, GladeProject *project)
{
	GtkItemFactoryEntry entry;

	g_return_if_fail (GLADE_IS_PROJECT_WINDOW (gpw));
	g_return_if_fail (GLADE_IS_PROJECT (project));
	
	gpw->projects = g_list_prepend (gpw->projects, project);
	
	entry.path = g_strdup_printf ("/Project/%s", project->name);
	entry.accelerator = NULL;
	entry.callback = glade_project_window_set_project_cb;
	entry.callback_action = 0;
	entry.item_type = g_strdup ("<Item>");
	
	gtk_item_factory_create_item (gpw->item_factory,
				      &entry,
				      project,
				      1);

	g_free (entry.path);
	g_free (entry.item_type);

	glade_project_window_set_project (gpw, project);
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
		g_warning ("We can only query integer types for now. FIXME please ;-)");
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
	glade_palette_show (gpw);
	glade_editor_show (gpw);
}
