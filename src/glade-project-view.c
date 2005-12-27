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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n-lib.h>
#include "glade.h"
#include "glade-widget.h"
#include "glade-project.h"
#include "glade-widget-class.h"
#include "glade-project-view.h"
#include "glade-popup.h"
#include "glade-app.h"

enum
{
	LAST_SIGNAL
};

typedef enum {
	CELL_ICON,
	CELL_NAME,
	CELL_MISC
} GPVCellType;

static GtkScrolledWindow *parent_class = NULL;

static void glade_project_view_class_init (GladeProjectViewClass *class);
static void glade_project_view_init (GladeProjectView *view);

/**
 * glade_project_view_get_type:
 *
 * Creates the typecode for the #GladeProjectView object type.
 *
 * Returns: the typecode for the #GladeProjectView object type
 */
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

static GList *
glade_project_view_list_child_iters (GtkTreeStore *model,
				     GladeWidget  *parent)
{
	GList *children, *l, *child_iters = NULL;

	if ((children = glade_widget_class_container_get_all_children
	     (parent->widget_class, parent->object)) != NULL)
	{
		for (l = children; l && l->data; l = l->next)
		{
			GObject     *object = l->data;
			GladeWidget *gwidget;
			if ((gwidget = glade_widget_get_from_gobject (object)) != NULL)
			{
				GtkTreeIter *iter;
				if ((iter = glade_util_find_iter_by_widget
				     (GTK_TREE_MODEL (model), gwidget, WIDGET_COLUMN)) != NULL)
				{
					child_iters = g_list_prepend (child_iters, iter);
				}
			}
		}
		g_list_free (children);
	}
	return g_list_reverse (child_iters);
}


static void
glade_project_view_widget_name_changed (GladeProjectView *view,
					GladeWidget *findme)
{
	GtkTreeModel *model;
	GtkTreeIter  *iter;
	GtkTreePath  *path;

	if (view->is_list &&
	    !g_type_is_a (findme->widget_class->type, GTK_TYPE_WINDOW))
		return;

	model = GTK_TREE_MODEL (view->model);

	if ((iter = glade_util_find_iter_by_widget 
	     (model, findme, WIDGET_COLUMN)) != NULL)
	{
		path = gtk_tree_model_get_path (model, iter);
		gtk_tree_model_row_changed (model, path, iter);
		gtk_tree_iter_free (iter);
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
	GList *children, *list;
	GtkTreeIter       iter;

	for (list = widgets; list; list = list->next)
	{
		GladeWidget *widget;

		if ((widget = glade_widget_get_from_gobject (list->data)) != NULL)
		{
			gtk_tree_store_append (model, &iter, parent_iter);
			gtk_tree_store_set    (model, &iter, WIDGET_COLUMN, widget, -1);

			if (add_childs &&
			    (children = glade_widget_class_container_get_all_children
			     (widget->widget_class, widget->object)) != NULL)
			{
				GtkTreeIter *copy = NULL;

				copy = gtk_tree_iter_copy (&iter);
				glade_project_view_populate_model_real
					(model, children, copy, TRUE);
				gtk_tree_iter_free (copy);

				g_list_free (children);
			}
		}
	}
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
	for (list = project->objects; list; list = list->next)
	{
		GObject *object = G_OBJECT (list->data);
		GladeWidget *gwidget = glade_widget_get_from_gobject (object);
		g_assert (gwidget);

		if (gwidget->parent == NULL)
			toplevels = g_list_append (toplevels, object);
	}

	/* add the widgets and recurse */
	glade_project_view_populate_model_real (view->model, toplevels,
						NULL, !view->is_list);

	g_list_free (toplevels);
}

static GtkTreeIter *
gpv_find_preceeding_sibling (GtkTreeModel *model,
			     GtkTreeIter  *parent_iter,
			     GladeWidget  *widget)
{
	GtkTreeIter *retval = NULL, next;
	GladeWidget *w, *sibling = NULL;

	g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);

	if (!parent_iter)
	{
		if (!gtk_tree_model_get_iter_first (model, &next))
			return NULL;
	}
	else
	{
		if (!gtk_tree_model_iter_has_child (model, parent_iter))
			return NULL;
		gtk_tree_model_iter_children (model, &next, parent_iter);
	}

	while (42)
	{
		gtk_tree_model_get (model, &next, WIDGET_COLUMN, &w, -1);
		
		/* If this sibling (w) is less than widget and
		 * the last found sibling is less then this one
		 */
		if ((strcmp (glade_widget_get_name (w),
			     glade_widget_get_name (widget)) < 0) &&
		    (sibling == NULL || 
		     strcmp (glade_widget_get_name (sibling),
			     glade_widget_get_name (w)) < 0))
		{
			sibling = widget;
			if (retval)
				gtk_tree_iter_free (retval);
			retval = gtk_tree_iter_copy (&next);
		}

		if (!gtk_tree_model_iter_next (model, &next))
			break;
	}
	return retval;
}

static void
glade_project_view_add_item (GladeProjectView *view,
			     GladeWidget *widget)
{
	GladeWidgetClass *class;
	GladeWidget  *parent;
	GtkTreeStore *model;
	GtkTreeIter   iter;
	GtkTreeIter  *parent_iter = NULL, *child_iter, *sibling_iter;
	GList        *child_iters, *children, *l;
	
	g_return_if_fail (widget != NULL);

	class = glade_widget_get_class (widget);

	if (view->is_list && !g_type_is_a (class->type, GTK_TYPE_WINDOW))
		return;
	
	model = view->model;

	/* Find parent and preceeding sibling to insert sorted */
	if ((parent = glade_widget_get_parent (widget)) != NULL)
		parent_iter = glade_util_find_iter_by_widget 
			(GTK_TREE_MODEL (model), parent, WIDGET_COLUMN);

	sibling_iter = gpv_find_preceeding_sibling 
		(GTK_TREE_MODEL (model), parent_iter, widget);

	/* Add to the tree sorted and free iterators */
	gtk_tree_store_insert_after (model, &iter, parent_iter, sibling_iter);
	if (parent_iter)
		gtk_tree_iter_free (parent_iter);
	if (sibling_iter)
		gtk_tree_iter_free (sibling_iter);

	gtk_tree_store_set (model, &iter, WIDGET_COLUMN, widget, -1);

	/* Remove all children */
	if ((child_iters = 
	     glade_project_view_list_child_iters (model, widget)) != NULL)
	{
		for (l = child_iters; l && l->data; l = l->next)
		{
			child_iter = l->data;
			gtk_tree_store_remove (model, child_iter);
			gtk_tree_iter_free (child_iter);
		}
		g_list_free (child_iters);
	}

	/* Repopulate these children properly.
	 */
	if (!view->is_list &&
	    (children = glade_widget_class_container_get_all_children
	     (widget->widget_class, widget->object)) != NULL)
	{
		glade_project_view_populate_model_real
			(model, children, &iter, TRUE);
		g_list_free (children);
	}
	
}      

static void
glade_project_view_remove_item (GladeProjectView *view,
				GladeWidget *widget)
{
	GladeWidgetClass *class;
	GtkTreeModel     *model;
	GtkTreeIter      *iter;

	class = glade_widget_get_class (widget);
	
	if (view->is_list && !g_type_is_a (class->type, GTK_TYPE_WINDOW))
		return;
	
	model = GTK_TREE_MODEL (view->model);

	if ((iter = glade_util_find_iter_by_widget
	     (model, widget, WIDGET_COLUMN)) != NULL)
	{
		gtk_tree_store_remove (view->model, iter);
		gtk_tree_iter_free (iter);
	}
}      

/**
 * glade_project_view_selection_update:
 * @view: a #GladeProjectView
 * @widget: a #GladeProject
 * 
 * The project selection has changed, update our state to reflect the changes.
 */
static void
glade_project_view_selection_update (GladeProjectView *view,
				     GladeProject *project)
{
	GladeWidget      *widget;
	GladeWidgetClass *class;
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter      *iter;
	GList            *list;

	g_return_if_fail (GLADE_IS_PROJECT_VIEW (view));
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (view->project == project);


	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->tree_view));
	model     = GTK_TREE_MODEL (view->model);

	g_return_if_fail (selection != NULL);

	view->updating_treeview = TRUE;

	gtk_tree_selection_unselect_all (selection);

	for (list = glade_project_selection_get (project);
	     list && list->data; list = list->next)
	{	
		if ((widget = glade_widget_get_from_gobject
		     (G_OBJECT (list->data))) != NULL)
		{
			class  = glade_widget_get_class (widget);

			if (view->is_list && 
			    !g_type_is_a (class->type, GTK_TYPE_WINDOW))
				continue;

			if ((iter = glade_util_find_iter_by_widget 
			     (model, widget, WIDGET_COLUMN)) != NULL)
			{
				gtk_tree_selection_select_iter (selection, iter);
				gtk_tree_iter_free (iter);
			}
		}
	}

	view->updating_treeview = FALSE;
}      

static void
glade_project_view_class_init (GladeProjectViewClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	class->add_item             = glade_project_view_add_item;
	class->remove_item          = glade_project_view_remove_item;
	class->set_project          = glade_project_view_set_project;
	class->widget_name_changed  = glade_project_view_widget_name_changed;
	class->selection_update     = glade_project_view_selection_update;
}


static void
gpw_foreach_add_selection (GtkTreeModel *model, 
			   GtkTreePath  *path,
			   GtkTreeIter  *iter, 
			   gpointer      data)
{
	GladeWidget      *widget;
	gtk_tree_model_get (model, iter, WIDGET_COLUMN, &widget, -1);
	glade_default_app_selection_add
		(glade_widget_get_object (widget), FALSE);
}


static gboolean
glade_project_view_selection_changed_cb (GtkTreeSelection *selection,
					 GladeProjectView  *view)
{
	g_return_val_if_fail (GLADE_IS_PROJECT_VIEW (view), FALSE);

	if (view->project != NULL &&
	    view->updating_treeview == FALSE)
	{
		view->updating_selection = TRUE;

		glade_default_app_selection_clear (FALSE);
		gtk_tree_selection_selected_foreach
			(selection, gpw_foreach_add_selection, view);
		glade_default_app_selection_changed ();
		
		view->updating_selection = FALSE;
	}
	return TRUE;
}

static void
glade_project_view_item_activated_cb (GtkTreeView *view,
				      GtkTreePath *path, 
				      GtkTreeViewColumn *column,
				      gpointer notused)
{
	GladeWidget  *widget;
	GObject      *object;
	GtkTreeModel *model;
	GtkTreeIter   iter;
	
	model = gtk_tree_view_get_model (view);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, WIDGET_COLUMN, &widget, -1);

	if ((object = glade_widget_get_object (widget)) != NULL &&
	    GTK_IS_WIDGET (object))
	{
		if (GTK_WIDGET_VISIBLE (object))
			glade_widget_hide (widget);
		else
			glade_widget_show (widget);
	}
}

static gint
glade_project_view_button_press_cb (GtkWidget        *widget,
				    GdkEventButton   *event,
				    GladeProjectView *view)
{
	GtkTreeView      *tree_view = GTK_TREE_VIEW (widget);
	GtkTreePath      *path      = NULL;
	gboolean          handled   = FALSE;

	if (event->window == gtk_tree_view_get_bin_window (tree_view) &&
	    gtk_tree_view_get_path_at_pos (tree_view, (gint) event->x, (gint) event->y,
					   &path, NULL, 
					   NULL, NULL) && path != NULL)
	{
		GtkTreeIter  iter;
		GladeWidget *widget = NULL;
		if (gtk_tree_model_get_iter (GTK_TREE_MODEL (view->model),
					     &iter, path))
		{
			/* Alright, now we can obtain 
			 * the widget from the iter.
			 */
			gtk_tree_model_get (GTK_TREE_MODEL (view->model), &iter,
					    WIDGET_COLUMN, &widget, -1);
			if (widget != NULL &&
				    event->button == 3)
			{
				glade_popup_widget_pop (widget, event, FALSE);
				handled = TRUE;
			}
			gtk_tree_path_free (path);
		}
	}
	return handled;
}

static void
glade_project_view_row_cb (GtkTreeView       *tree_view,
			   GtkTreeIter       *iter,
			   GtkTreePath       *path,
			   GladeProjectView  *view)
{
	GLADE_PROJECT_VIEW_GET_CLASS (view)->selection_update (view, view->project);
}

static void
glade_project_view_cell_function (GtkTreeViewColumn *tree_column,
				  GtkCellRenderer *cell,
				  GtkTreeModel *tree_model,
				  GtkTreeIter *iter,
				  gpointer data)
{
	GPVCellType  type = GPOINTER_TO_INT (data);
	GladeWidget *widget;
	gchar       *text = NULL, *child_type;

	gtk_tree_model_get (tree_model, iter, WIDGET_COLUMN, &widget, -1);

	/* The cell exists, but not widget has been asociated with it */
	if (!widget) return;

	g_return_if_fail (widget->name != NULL);
	g_return_if_fail (widget->widget_class != NULL);
	g_return_if_fail (widget->widget_class->name != NULL);
	g_return_if_fail (widget->widget_class->icon != NULL);

	switch (type) 
	{
	case CELL_ICON:
		g_object_set (G_OBJECT (cell), "pixbuf", 
			      widget->widget_class->icon, NULL);
		break;
	case CELL_NAME:
		g_object_set (G_OBJECT (cell), "text", widget->name, NULL);
		break;
	case CELL_MISC:
		/* special child type / internal child */
		if (glade_widget_get_internal (widget) != NULL)
			text = g_strdup_printf (_("(internal %s)"),  
						glade_widget_get_internal (widget));
		else if ((child_type = g_object_get_data (glade_widget_get_object (widget),
							  "special-child-type")) != NULL)
			text = g_strdup_printf (_("(%s child)"), child_type);

		g_object_set (G_OBJECT (cell), "text", text ? text : " ", NULL);
		if (text) g_free (text);
		break;
	default:
		break;
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
						 GINT_TO_POINTER (CELL_ICON), NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 glade_project_view_cell_function,
						 GINT_TO_POINTER (CELL_NAME), NULL);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), 
		      "style", PANGO_STYLE_ITALIC,
		      "foreground", "Gray", NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, renderer,
						 glade_project_view_cell_function,
						 GINT_TO_POINTER (CELL_MISC), NULL);
	gtk_tree_view_append_column (view, column);
}

static gint
glade_project_view_sort_func (GtkTreeModel *model,
			      GtkTreeIter  *a,
			      GtkTreeIter  *b,
			      gpointer      user_data)
{
	GladeWidget  *widget_a, *widget_b;
	gtk_tree_model_get (model, a, WIDGET_COLUMN, &widget_a, -1);
	gtk_tree_model_get (model, b, WIDGET_COLUMN, &widget_b, -1);
	return strcmp (glade_widget_get_name (widget_a),
		       glade_widget_get_name (widget_b));
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

#if 0
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (view->model),
					      0, GTK_SORT_ASCENDING);

	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (view->model), 0,
					 glade_project_view_sort_func,
					 NULL, NULL);
#endif
 
	view->tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (view->model));

	/* the view now holds a reference, we can get rid of our own */
	g_object_unref (G_OBJECT (view->model));

	glade_project_view_add_columns (GTK_TREE_VIEW (view->tree_view));

	/* Show/Hide windows on double-click */
	g_signal_connect (G_OBJECT (view->tree_view), "row-activated",
			  G_CALLBACK (glade_project_view_item_activated_cb), NULL);

	/* Update project selection */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->tree_view));
	g_signal_connect_data (G_OBJECT (selection), "changed",
			       G_CALLBACK (glade_project_view_selection_changed_cb),
			       view, NULL, 0);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	/* Popup menu */
	g_signal_connect (G_OBJECT (view->tree_view), "button-press-event",
			  G_CALLBACK (glade_project_view_button_press_cb), view);
	
	/* Refresh selection */
	g_signal_connect (G_OBJECT (view->tree_view), "row-expanded",
			  G_CALLBACK (glade_project_view_row_cb), view);
	g_signal_connect (G_OBJECT (view->tree_view), "row-collapsed",
			  G_CALLBACK (glade_project_view_row_cb), view);

	view->updating_selection = FALSE;
	view->updating_treeview  = FALSE;
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
 * @type: a #GladeProjectViewType
 * 
 * Creates a new #GladeProjectView of type @type
 * 
 * Returns: a new #GladeProjectView
 */
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
 * @view: a #GladeProjectView
 * @project: a #GladeProject or %NULL
 *
 * Sets the project of @view to @project. If @project is %NULL, @view will
 * stop being a view of a project.
 */
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

	/* Set to null while we remove all the items from the store, because we 
         * are going to trigger selection_changed signal emisions on the View. 
         * By setting the project to NULL, _selection_changed_cb will just 
         * return.
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

	/* Here we connect to all the signals of the project that interest us */
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
 * @view: a #GladeProjectView
 * 
 * Returns: the #GladeProject @view represents
 */
GladeProject *
glade_project_view_get_project (GladeProjectView *view)
{
	return view->project;
}
