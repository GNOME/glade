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

#include "glade.h"
#include "glade-widget.h"
#include "glade-project.h"
#include "glade-widget-class.h"
#include "glade-project-view.h"

enum
{
	LAST_SIGNAL
};

static GtkScrolledWindow *parent_class = NULL;

static void glade_project_view_class_init (GladeProjectViewClass *class);
static void glade_project_view_init (GladeProjectView *view);

GType
glade_project_view_get_type (void)
{
	static GType type = 0;

	if (!type)
	{
		static const GTypeInfo info =
		{
			sizeof (GladeProjectViewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) glade_project_view_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (GladeProjectView),
			0,
			(GInstanceInitFunc) glade_project_view_init
		};

		type = g_type_register_static (GTK_TYPE_SCROLLED_WINDOW, "GladeProjectView", &info, 0);
	}

	return type;
}

enum
{
	WIDGET_COLUMN = 0,
	N_COLUMNS
};

static GtkTreeIter *
glade_project_view_find_iter (GtkTreeModel *model,
			      GtkTreeIter *iter,
			      GladeWidget *findme)
{
	GladeWidget *widget;
	GtkTreeIter *next;

	next = gtk_tree_iter_copy (iter);
	
	while (TRUE)
	{
		gtk_tree_model_get (model, next, WIDGET_COLUMN, &widget, -1);
		if (widget == NULL) {
			g_warning ("Could not get the glade widget from the model");
			return NULL;
		}
		if (widget == findme)
			return gtk_tree_iter_copy (next);
		/* Me ? leaking ? nah .... */
#if 1
		if (gtk_tree_model_iter_has_child (model, next))
		{
			GtkTreeIter *child = g_new0 (GtkTreeIter, 1);
			GtkTreeIter *retval = NULL;
			gtk_tree_model_iter_children (model, child, next);
			retval = glade_project_view_find_iter (model,
							       child,
							       findme);
			if (retval != NULL)
				return retval;
		}
#endif	
		if (!gtk_tree_model_iter_next (model, next))
			break;
	}

	return NULL;
}

GtkTreeIter *
glade_project_view_find_iter_by_widget (GtkTreeModel *model,
					GladeWidget *findme)
{
	GtkTreeIter iter;
	GtkTreeIter *retval;

	if (!gtk_tree_model_get_iter_root (model, &iter))
	{
		g_warning ("Could not find first widget.");
		return NULL;
	}

	retval = glade_project_view_find_iter (model, &iter, findme);

	if (!retval)
		g_warning ("Could not find iterator for %s\n", findme->name);

	return retval;
}

static void
glade_project_view_widget_name_changed (GladeProjectView *view,
					GladeWidget *findme)
{
	GtkTreeModel *model;
	GtkTreeIter *iter;
	GtkTreePath *path;

	if (view->is_list && !GLADE_WIDGET_IS_TOPLEVEL (findme))
		return;

	model = GTK_TREE_MODEL (view->model);

	iter = glade_project_view_find_iter_by_widget (model, findme);

	if (iter)
	{
		path = gtk_tree_model_get_path (model, iter);
		gtk_tree_model_row_changed (model, path, iter);
	}
}

/**
 * Note that widgets is a list of GtkWidgets, while what we store
 * in the model are the associated GladeWidgets.
 */
static void
glade_project_view_populate_model_real (GtkTreeStore *model,
					GList *widgets,
					GtkTreeIter *parent_iter,
					gboolean add_childs)
{
	GList *list;
	GtkTreeIter iter;

	list = g_list_copy (widgets);
	list = g_list_reverse (list);
	for (; list; list = list->next)
	{
		GladeWidget *widget;

		widget = glade_widget_get_from_gtk_widget (list->data);
		if (widget)
		{
			gtk_tree_store_append (model, &iter, parent_iter);
			gtk_tree_store_set (model, &iter,
					    WIDGET_COLUMN, widget,
					    -1);
		}

		if (add_childs && GTK_IS_CONTAINER (list->data))
		{
			GList *children;
			GtkTreeIter *copy = NULL;

			copy = gtk_tree_iter_copy (&iter);
			children = gtk_container_get_children (GTK_CONTAINER (list->data));
			glade_project_view_populate_model_real (model, children, copy, TRUE);
			gtk_tree_iter_free (copy);
			g_list_free (children);
		}
	}

	g_list_free (list);
}

static void
glade_project_view_populate_model (GladeProjectView *view)
{
	GladeProject *project;
	GList *list;
	GList *toplevels = NULL;

	project = view->project;
	if (!project)
		return;

	/* Make a list of only the toplevel widgets */
	for (list = project->widgets; list; list = list->next)
	{
		GtkWidget *widget;

		widget = GTK_WIDGET (list->data);
		if (GTK_WIDGET_TOPLEVEL (widget))
			toplevels = g_list_append (toplevels, widget);
	}

	/* add the widgets and recurse */
	glade_project_view_populate_model_real (view->model, toplevels,
						NULL, !view->is_list);

	g_list_free (toplevels);
}

static void
glade_project_view_add_item (GladeProjectView *view,
			     GladeWidget *widget)
{
	GladeWidgetClass *class;
	GladeWidget *parent;
	GtkTreeStore *model;
	GtkTreeIter iter;
	GtkTreeIter *parent_iter = NULL;

	g_return_if_fail (widget != NULL);

	class = glade_widget_get_class (widget);
	
	if (view->is_list && !GLADE_WIDGET_CLASS_TOPLEVEL (class))
		return;
	
	model = view->model;

	parent = glade_widget_get_parent (widget);
	if (parent)
		parent_iter = glade_project_view_find_iter_by_widget (GTK_TREE_MODEL (model),
								      parent);

	gtk_tree_store_append (model, &iter, parent_iter);
	gtk_tree_store_set (model, &iter,
			    WIDGET_COLUMN, widget,
			    -1);

	view->updating_selection = TRUE;
	glade_project_selection_set (widget->project, widget->widget, TRUE);
	view->updating_selection = FALSE;
}      

static void
glade_project_view_remove_item (GladeProjectView *view,
				GladeWidget *widget)
{
	GladeWidgetClass *class;
	GtkTreeModel *model;
	GtkTreeIter *iter;

	class = glade_widget_get_class (widget);
	
	if (view->is_list && !GLADE_WIDGET_CLASS_TOPLEVEL (class))
		return;
	
	model = GTK_TREE_MODEL (view->model);

	iter = glade_project_view_find_iter_by_widget (model, widget);
	gtk_tree_store_remove (view->model, iter);
}      

/**
 * glade_project_view_selection_update:
 * @view: 
 * @widget: 
 * 
 * The project selection has changed, update our state to reflect the changes.
 **/
static void
glade_project_view_selection_update (GladeProjectView *view,
				     GladeProject *project)
{
#if 0
	GtkTreeSelection *selection;
#endif	
	GList *list;

	g_return_if_fail (GLADE_IS_PROJECT_VIEW (view));
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (view->project == project);

	list = glade_project_selection_get (project);

#if 0
	g_ print ("Update dude\n");
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->tree_view));
	
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

	GladeWidgetClass *class;
	GtkTreeModel *model;
	GtkTreeIter *iter;

	class = glade_widget_get_class (widget);
	
	if (view->is_list && !GLADE_WIDGET_CLASS_TOPLEVEL (class))
		return;
	
	model = GTK_TREE_MODEL (view->model);

	iter = glade_project_view_find_iter_by_widget (model,
						       widget);

	if (iter) {
		static gboolean warned = FALSE;
		if (!warned)
			g_print ("Update the cell. BUT HOW ??\n");
		warned = TRUE;
	}

	gtk_tree_store_remove (view->model, iter);
#endif	
}      

static void
glade_project_view_class_init (GladeProjectViewClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	class->add_item	= glade_project_view_add_item;
	class->remove_item = glade_project_view_remove_item;
	class->set_project = glade_project_view_set_project;
	class->widget_name_changed = glade_project_view_widget_name_changed;
	class->selection_update = glade_project_view_selection_update;
}

static gboolean
glade_project_view_selection_changed_cb (GtkTreeSelection *selection,
					 GladeProjectView  *view)
{
	GtkTreeModel *model;
	GladeWidget *widget;
	GtkTreeIter iter;

	if (!view->project)
		return TRUE;

	g_return_val_if_fail (GLADE_IS_PROJECT_VIEW (view), FALSE);

	model = GTK_TREE_MODEL (view->model);

	/* There are no cells selected */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return TRUE;

	gtk_tree_model_get (model, &iter, WIDGET_COLUMN, &widget, -1);

	/* The cell exists, but not widget has been asociated with it */
	if (widget == NULL)
		return TRUE;

	view->updating_selection = TRUE;
	glade_project_selection_set (widget->project, widget->widget, TRUE);
	view->updating_selection = FALSE;

	return TRUE;
}

static void
glade_project_view_item_activated_cb (GtkTreeView *view,
				      GtkTreePath *path, 
				      GtkTreeViewColumn *column,
				      gpointer notused)
{
	GladeWidget *widget;
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model (view);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, WIDGET_COLUMN, &widget, -1);

	if (GTK_IS_WINDOW (widget->widget))
		gtk_window_present (GTK_WINDOW (widget->widget));

	gtk_widget_show (widget->widget);
}

static void
glade_project_view_cell_function (GtkTreeViewColumn *tree_column,
				  GtkCellRenderer *cell,
				  GtkTreeModel *tree_model,
				  GtkTreeIter *iter,
				  gpointer data)
{
	gboolean is_icon = GPOINTER_TO_INT (data);
	GladeWidget *widget;

	gtk_tree_model_get (tree_model, iter, WIDGET_COLUMN, &widget, -1);

	/* The cell exists, but not widget has been asociated with it */
	if (!widget)
		return;

	g_return_if_fail (widget->name != NULL);
	g_return_if_fail (widget->class != NULL);
	g_return_if_fail (GPOINTER_TO_INT (widget->class) > 5000);
	g_return_if_fail (widget->class->name != NULL);
	g_return_if_fail (widget->class->icon != NULL);

	if (is_icon)
	{
		if (gtk_image_get_storage_type GTK_IMAGE (widget->class->icon) != GTK_IMAGE_PIXBUF)
			return;

		g_object_set (G_OBJECT (cell), "pixbuf",
			      gtk_image_get_pixbuf (GTK_IMAGE (widget->class->icon)), NULL);
	}
	else
	{
		g_object_set (G_OBJECT (cell), "text", widget->name, NULL);
	}
}

static void
glade_project_view_add_columns (GtkTreeView *view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Widget"));

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 glade_project_view_cell_function,
						 GINT_TO_POINTER (1), NULL);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 glade_project_view_cell_function,
						 GINT_TO_POINTER (0), NULL);

	gtk_tree_view_append_column (view, column);
}

static void
glade_project_view_init (GladeProjectView *view)
{
	GtkTreeSelection *selection;

	view->selected_widget = NULL;
	view->project = NULL;
	view->add_widget_signal_id = 0;

	view->model = gtk_tree_store_new (N_COLUMNS, G_TYPE_POINTER);
	glade_project_view_populate_model (view);

	view->tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (view->model));

	/* the view now holds a reference, we can get rid of our own */
	g_object_unref (G_OBJECT (view->model));

	glade_project_view_add_columns (GTK_TREE_VIEW (view->tree_view));

	g_signal_connect (G_OBJECT (view->tree_view), "row-activated",
			  G_CALLBACK (glade_project_view_item_activated_cb), NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->tree_view));
	g_signal_connect_data (G_OBJECT (selection), "changed",
			       G_CALLBACK (glade_project_view_selection_changed_cb),
			       view, NULL, 0);

	view->updating_selection = FALSE;
}

static void
glade_project_view_add_widget_cb (GladeProject *project,
				  GladeWidget *widget,
				  GladeProjectView *view)
{
	GLADE_PROJECT_VIEW_GET_CLASS (view)->add_item (view, widget);
}

static void
glade_project_view_remove_widget_cb (GladeProject *project,
				     GladeWidget *widget,
				     GladeProjectView *view)
{
	GLADE_PROJECT_VIEW_GET_CLASS (view)->remove_item (view, widget);
}

static void
glade_project_view_widget_name_changed_cb (GladeProject *project,
					   GladeWidget *widget,
					   GladeProjectView *view)
{
	GLADE_PROJECT_VIEW_GET_CLASS (view)->widget_name_changed (view, widget);
}

static void
glade_project_view_selection_update_cb (GladeProject *project,
					GladeProjectView *view)
{
	/* This view caused this selection change in the project, since we
	 * already know about it we don't need updating. Chema
	 */
	if (view->updating_selection)
		return;

	GLADE_PROJECT_VIEW_GET_CLASS (view)->selection_update (view, project);
}

/**
 * glade_project_view_new:
 * @type: the type of the view. It can be list or tree.
 * 
 * Creates a new project view and ready to be used
 * 
 * Return Value: a newly created project view
 **/
GladeProjectView *
glade_project_view_new (GladeProjectViewType type)
{
	GladeProjectView *view = g_object_new (GLADE_TYPE_PROJECT_VIEW, NULL);

	if (type == GLADE_PROJECT_VIEW_LIST)
	{
		view->is_list = TRUE;

		/* in the main window list we don't want the header */
		gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view->tree_view), FALSE);
	}
	else
		view->is_list = FALSE;

	gtk_container_add (GTK_CONTAINER (view), view->tree_view);

	return view;
}

/**
 * glade_project_view_set_project:
 * @view: The view we are setting the new project for
 * @project: The project the view will represent, this can be NULL for stop beeing
 *           a view of a project.
 *
 * Sets the project of a view. 
 **/
void
glade_project_view_set_project (GladeProjectView *view,
				GladeProject	 *project)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	g_return_if_fail (GLADE_IS_PROJECT_VIEW (view));

	if (view->project == project)
		return;

	/* This view stops listening to the old project (if there was one) */
	if (view->project != NULL)
	{
		g_signal_handler_disconnect (G_OBJECT (view->project),
					     view->add_widget_signal_id);
		g_signal_handler_disconnect (G_OBJECT (view->project),
					     view->remove_widget_signal_id);
		g_signal_handler_disconnect (G_OBJECT (view->project),
					     view->widget_name_changed_signal_id);
		g_signal_handler_disconnect (G_OBJECT (view->project),
					     view->selection_changed_signal_id);
	}

	model = GTK_TREE_MODEL (view->model);

	/* Set to null while we remove all the items from the store, because we are going to trigger
	 * selection_changed signal emisions on the View. By setting the project to NULL, _selection_changed_cb
	 * will just return. Chema
	 */	 
	view->project = NULL;

	while (gtk_tree_model_get_iter_root (model, &iter))
		gtk_tree_store_remove (view->model, &iter);

	/* if we were passed project == NULL, we are done */
	if (project == NULL)
		return;

	g_return_if_fail (GLADE_IS_PROJECT (project));

	view->project = project;

	glade_project_view_populate_model (view);

	/* Here we connect to all the signals of the project that interests us */
	view->add_widget_signal_id =
		g_signal_connect (G_OBJECT (project), "add_widget",
				  G_CALLBACK (glade_project_view_add_widget_cb),
				  view);
	view->remove_widget_signal_id =
		g_signal_connect (G_OBJECT (project), "remove_widget",
				  G_CALLBACK (glade_project_view_remove_widget_cb),
				  view);
	view->widget_name_changed_signal_id =
		g_signal_connect (G_OBJECT (project), "widget_name_changed",
				  G_CALLBACK (glade_project_view_widget_name_changed_cb),
				  view);
	view->selection_changed_signal_id =
		g_signal_connect (G_OBJECT (project), "selection_changed",
				  G_CALLBACK (glade_project_view_selection_update_cb),
				  view);
}

/**
 * glade_project_view_get_project:
 * @view: 
 * 
 * Get's the project of a view
 * 
 * Return Value: the project this view is representing
 **/
GladeProject *
glade_project_view_get_project (GladeProjectView *view)
{
	return view->project;
}

