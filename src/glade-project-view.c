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
#include "glade-widget-class.h"
#include "glade-project-view.h"

enum
{
	LAST_SIGNAL
};

static GtkObject *parent_class = NULL;

static void glade_project_view_class_init (GladeProjectViewClass * klass);
static void glade_project_view_init (GladeProjectView * project_view);

#define GLADE_PROJECT_VIEW_CLIST_SPACING 3

enum
{
  WIDGET_COLUMN = 0,
  NUM_COLUMNS
};

guint
glade_project_view_get_type (void)
{
	static guint glade_project_view_type = 0;

	if (!glade_project_view_type)
	{
		GtkTypeInfo glade_project_view_info =
		{
			"GladeProjectView",
			sizeof (GladeProjectView),
			sizeof (GladeProjectViewClass),
			(GtkClassInitFunc) glade_project_view_class_init,
			(GtkObjectInitFunc) glade_project_view_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		glade_project_view_type = gtk_type_unique (gtk_object_get_type (),
							   &glade_project_view_info);
	}

	return glade_project_view_type;
}

static GtkTreeIter *
glade_project_view_find_iter (GtkTreeModel *model,
			      GtkTreeIter *iter,
			      GladeWidget *findme)
{
	GladeWidget *widget;
	GtkTreeIter *next;

	next = gtk_tree_iter_copy (iter);
	
	while (TRUE) {
		gtk_tree_model_get (model, next, WIDGET_COLUMN, &widget, -1);
		if (widget == NULL) {
			g_warning ("Could not get the glade widget from the model");
			return NULL;
		}
		if (widget == findme)
			return gtk_tree_iter_copy (next);
		/* Me ? leaking ? nah .... */
#if 1
		if (gtk_tree_model_iter_has_child (model, iter)) {
			GtkTreeIter *child = g_new0 (GtkTreeIter, 1);
			GtkTreeIter *retval = g_new0 (GtkTreeIter, 1);
			gtk_tree_model_iter_children (model, child, iter);
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

	if (!gtk_tree_model_get_first (model, &iter)) {
		g_warning ("Cound not find first widget.");
		return NULL;
	}

	retval = glade_project_view_find_iter (model, &iter, findme);

	if (retval == NULL)
		g_warning ("Could not find iterator for %s\n", findme->name);
	
	return retval;
}


static void
glade_project_view_widget_name_changed (GladeProjectView *view,
					GladeWidget *findme)
{
	GtkTreeModel *model;
	GtkTreeIter *iter;
		
	model = GTK_TREE_MODEL (view->model);

	iter = glade_project_view_find_iter_by_widget (model,
						       findme);

	if (iter) {
		static gboolean warned = FALSE;
		if (!warned)
			g_print ("Update the cell. BUT HOW ??\n");
		warned = TRUE;
	}
}

static void
glade_project_view_populate_model_real (GtkTreeStore *model,
					GList *widgets,
					GtkTreeIter *parent_iter)
{
	GladeWidget *widget;
	GList *list;
	GtkTreeIter iter;
	GtkTreeIter *copy = NULL;
	
	list = g_list_copy (widgets);
	list = g_list_reverse (list);
	for (; list != NULL; list = list->next) {
		widget = list->data;
		gtk_tree_store_append (model, &iter, parent_iter);
		gtk_tree_store_set (model, &iter,
				    WIDGET_COLUMN, widget,
				    -1);
		if (widget->children != NULL) {
			copy = gtk_tree_iter_copy (&iter);
			glade_project_view_populate_model_real (model,
								widget->children,
								copy);
			gtk_tree_iter_free (copy);
		}
	}
}

static void
glade_project_view_populate_model (GtkTreeStore *model,
				   GladeProjectView *view)
{
	GladeProject *project;
	GladeWidget *widget;
	GList *list;
	GList *toplevels = NULL;

	project = view->project;
	if (project == NULL)
		return;

	/* Make a list of only the toplevel widgets */
	list = project->widgets;
	for (; list != NULL; list = list->next) {
		widget = list->data;
		if (GLADE_WIDGET_TOPLEVEL (widget))
			toplevels = g_list_append (toplevels, widget);
	}

	/* add the widgets and recurse */
	glade_project_view_populate_model_real (model, toplevels, NULL);

	g_list_free (toplevels);
}



static void
glade_project_view_add_item (GladeProjectView *view,
			     GladeWidget *widget)
{
	GladeWidgetClass *class;
	GtkTreeStore *model;
	GtkTreeIter iter;
	GtkTreeIter *parent_iter = NULL;

	g_return_if_fail (widget != NULL);

	class = glade_widget_get_class (widget);
	
	if (view->is_list && !GLADE_WIDGET_CLASS_TOPLEVEL (class))
		return;
	
	model = view->model;

	if (widget->parent != NULL)
		parent_iter = glade_project_view_find_iter_by_widget (GTK_TREE_MODEL (model), widget->parent);
	
	gtk_tree_store_append (model, &iter, parent_iter);
	gtk_tree_store_set (model, &iter,
			    WIDGET_COLUMN, widget,
			    -1);

	glade_project_selection_set (view->project, widget, TRUE);
}      


static void
glade_project_view_class_init (GladeProjectViewClass * gpv_class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) gpv_class;

	parent_class = gtk_type_class (gtk_object_get_type ());

#warning FIXME, revisit
	
	gpv_class->add_item      = glade_project_view_add_item;
	gpv_class->set_project   = glade_project_view_set_project;
	gpv_class->widget_name_changed = glade_project_view_widget_name_changed;
}

static void
glade_project_view_cell_function (GtkTreeViewColumn *tree_column,
				  GtkCellRenderer   *cell,
				  GtkTreeModel      *tree_model,
				  GtkTreeIter       *iter,
				  gpointer           data)
{
	GladeWidget *widget;

	gtk_tree_model_get (tree_model, iter, WIDGET_COLUMN, &widget, -1);

	/* The cell exists, but not widget has been asociated with it */
	if (widget == NULL)
		return;

	g_object_set (G_OBJECT (cell),
		      "text", widget->name,
		      "pixbuf", widget->class->pixbuf,
		      NULL);
}

static gboolean
glade_project_view_selection_changed_cb (GtkTreeSelection *selection,
					 GladeProjectView  *view)
{
	GtkTreeModel *model;
	GladeWidget *widget;
	GtkTreeView *tree_view;
	GtkTreeIter iter;

	g_return_val_if_fail (GTK_IS_TREE_SELECTION (selection), FALSE);
	g_return_val_if_fail (GLADE_IS_PROJECT_VIEW (view), FALSE);

	tree_view = gtk_tree_selection_get_tree_view (selection);
	model = gtk_tree_view_get_model (tree_view);

	g_return_val_if_fail (GTK_IS_TREE_SELECTION (selection), FALSE);
	g_return_val_if_fail (GTK_IS_TREE_MODEL (model), FALSE);

	gtk_tree_selection_get_selected (selection, &model, &iter);

	gtk_tree_model_get (model, &iter, WIDGET_COLUMN, &widget, -1);

	/* The cell exists, but not widget has been asociated with it */
	if (widget == NULL)
		return TRUE;

	glade_project_selection_set (view->project, widget, TRUE);

	return TRUE;
}

static GtkWidget *
glade_project_view_create_widget (GladeProjectView *view)
{
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;
	GtkCellRenderer *cell;
	GtkTreeStore *model;
	GtkWidget *widget;
	GtkWidget *scrolled_window;

	model = gtk_tree_store_new_with_types (1, GTK_TYPE_POINTER);
	view->model = model;
	
	glade_project_view_populate_model (model, view);
	widget = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));

	cell = gtk_cell_renderer_text_pixbuf_new ();
	column = gtk_tree_view_column_new_with_attributes ("Widget", cell, NULL);
	gtk_tree_view_column_set_cell_data_func (column, glade_project_view_cell_function, NULL, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);

	gtk_object_unref (GTK_OBJECT (column));
	gtk_object_unref (GTK_OBJECT (cell));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_signal_connect_data (G_OBJECT (selection),
			       "changed", GTK_SIGNAL_FUNC (glade_project_view_selection_changed_cb),
			       view, NULL, FALSE, FALSE);

	gtk_widget_set_usize (widget, 272, 130);
	gtk_widget_show (widget);
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_usize (scrolled_window, 272, 130);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_set_usize (scrolled_window, 272, 130);
	gtk_container_add (GTK_CONTAINER (scrolled_window), widget);
		
	return scrolled_window;
}

static void
glade_project_view_init (GladeProjectView * view)
{
	view->selected_widget = NULL;
	view->project = NULL;
	view->add_widget_signal_id = 0;

	view->widget = glade_project_view_create_widget (view);
	gtk_widget_show (view->widget);
}

void
glade_project_view_selection_changed (GladeProjectView *view, GladeWidget *item)
{
	glade_project_selection_clear (view->project, FALSE);

	g_print ("Do something\n");
#if 0	
	if (view->selected_widget == item)
		return;

	view->selected_widget = item;

	gtk_signal_emit (GTK_OBJECT (view),
			 glade_project_view_signals [ITEM_SELECTED], item);
#endif	
}


static void
glade_project_view_add_widget_cb (GladeProject *project,
						    GladeWidget *widget,
						    GladeProjectView *view)
{
        GLADE_PROJECT_VIEW_CLASS (GTK_OBJECT_GET_CLASS(view))->add_item (view, widget);
}

static void
glade_project_view_widget_name_changed_cb (GladeProject *project,
								   GladeWidget *widget,
								   GladeProjectView *view)
{
        GLADE_PROJECT_VIEW_CLASS (GTK_OBJECT_GET_CLASS(view))->widget_name_changed (view, widget);
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
	GladeProjectView *view = NULL;

	view = GLADE_PROJECT_VIEW (gtk_type_new (GLADE_PROJECT_VIEW_TYPE));
	if (type == GLADE_PROJECT_VIEW_LIST)
		view->is_list = TRUE;
	else
		view->is_list = FALSE;

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

	if (view->project == project)
		return;

	/* This view stops listening to the old project (if there was one) */
	if (view->project != NULL) {
		gtk_signal_disconnect (GTK_OBJECT (view->project), view->add_widget_signal_id);
		gtk_signal_disconnect (GTK_OBJECT (view->project), view->widget_name_changed_signal_id);
	}

	view->project = project;
	
	model = GTK_TREE_MODEL (view->model);
	
	while (gtk_tree_model_get_first (model, &iter))
		gtk_tree_store_remove (view->model, &iter);

	glade_project_view_populate_model (view->model, view);

	/* Here we connect to all the signals of the project that interests us */
	view->add_widget_signal_id =
		gtk_signal_connect (GTK_OBJECT (project), "add_widget",
						GTK_SIGNAL_FUNC (glade_project_view_add_widget_cb),
						view);
	view->widget_name_changed_signal_id =
		gtk_signal_connect (GTK_OBJECT (project), "widget_name_changed",
						GTK_SIGNAL_FUNC (glade_project_view_widget_name_changed_cb),
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

/**
 * glade_project_view_get_widget:
 * @view: 
 * 
 * Get the GtkWidget of the view
 * 
 * Return Value: the widget of the view
 **/
GtkWidget *
glade_project_view_get_widget (GladeProjectView *view)
{
	return view->widget;
}

