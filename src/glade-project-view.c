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
#include "glade-widget-adaptor.h"
#include "glade-project-view.h"
#include "glade-popup.h"
#include "glade-app.h"

#define GLADE_PROJECT_VIEW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object),\
					       GLADE_TYPE_PROJECT_VIEW,              \
			 		       GladeProjectViewPrivate))

enum
{
	ITEM_ACTIVATED,
	LAST_SIGNAL
};
					  
enum
{
	WIDGET_COLUMN = 0,
	N_COLUMNS
};

typedef enum 
{
	CELL_ICON,
	CELL_NAME,
	CELL_MISC
} GPVCellType;

struct _GladeProjectViewPrivate
{
	GtkTreeStore *model; /* The model */

	GtkWidget *tree_view; /* The view */

	GladeProject *project; /* A pointer so that we can get back to the
				* project that we are a view for
				*/

	GList *project_data; /* A list of private data structures of 
			      * project specific data.
			      */
	
	gboolean updating_selection; /* True when we are going to set the
				      * project selection. So that we don't
				      * recurse cause we are also listening
				      * for the project changed selection
				      * signal
				      */
	gboolean updating_treeview; /* Eliminate feedback from the tree-view 
				     * (same as updating_selection)
				     */
};

typedef struct 
{
	GladeProjectView *view;    /* The view */
	GladeProject     *project; /* The GladeProject this describes */

	/* Signal ids for callbacks in this project */
	gulong        add_widget_signal_id;
	gulong        remove_widget_signal_id;
	gulong        widget_name_changed_signal_id;
	gulong        selection_changed_signal_id;
	gulong        project_closed_signal_id;

	GList        *open_leafs;    /* List of open GladeWidget leafs */
	gdouble       hadjust_value; /* View area for this project     */
	gdouble       vadjust_value; /* (i.e. h/v scrollbar values)    */

} GPVProjectData;

typedef struct
{
	GtkTreeView *view;
	GList       *list;
} GPVAccumPair;

static guint glade_project_view_signals[LAST_SIGNAL] = {0};
static GtkScrolledWindow *parent_class = NULL;

static void gpv_project_data_free         (GPVProjectData *pdata);
static gint gpv_find_project              (GPVProjectData *pdata, 
					   GladeProject   *project);
static void gpv_project_data_load_state   (GPVProjectData *pdata);
static void gpv_project_data_save_state   (GPVProjectData *pdata);


G_DEFINE_TYPE(GladeProjectView, glade_project_view, GTK_TYPE_SCROLLED_WINDOW)

/******************************************************************
 *                  GladeProjectViewClass methods                 *
 ******************************************************************/

/*
 * Note that widgets is a list of GObjects, while what we store
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
			    (children = glade_widget_adaptor_get_children
			     (widget->adaptor, widget->object)) != NULL)
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

	project = view->priv->project;
	if (!project)
		return;

	/* Clear it first */
	gtk_tree_store_clear (view->priv->model);

	/* Make a list of only the toplevel widgets */
	for (list = project->objects; list; list = list->next)
	{
		GObject *object      = G_OBJECT (list->data);
		GladeWidget *gwidget = glade_widget_get_from_gobject (object);
		g_assert (gwidget);

		if (gwidget->parent == NULL)
			toplevels = g_list_prepend (toplevels, object);
	}

	/* add the widgets and recurse */
	glade_project_view_populate_model_real (view->priv->model, toplevels,
						NULL, TRUE);

	g_list_free (toplevels);
}

static void
gpw_foreach_add_selection (GtkTreeModel *model, 
			   GtkTreePath  *path,
			   GtkTreeIter  *iter, 
			   gpointer      data)
{
	GladeWidget      *widget;
	gtk_tree_model_get (model, iter, WIDGET_COLUMN, &widget, -1);
	glade_app_selection_add
		(glade_widget_get_object (widget), FALSE);
}

static void
glade_project_view_add_item (GladeProjectView *view,
			     GladeWidget *widget)
{
	GPVProjectData *pdata;
	GList          *list;

	if ((list = 
	     g_list_find_custom (view->priv->project_data, 
				 view->priv->project, 
				 (GCompareFunc)gpv_find_project)) != NULL)
	{
		pdata = list->data;
		gpv_project_data_save_state (pdata);
		gpv_project_data_load_state (pdata);
	}
}      

static void
glade_project_view_remove_item (GladeProjectView *view,
				GladeWidget *widget)
{
	GtkTreeModel       *model;
	GtkTreeIter        *iter;
	
	model = GTK_TREE_MODEL (view->priv->model);

	if ((iter = glade_util_find_iter_by_widget
	     (model, widget, WIDGET_COLUMN)) != NULL)
	{
		gtk_tree_store_remove (view->priv->model, iter);
		gtk_tree_iter_free (iter);
	}
}      

static void
glade_project_view_widget_name_changed (GladeProjectView *view,
					GladeWidget *findme)
{
	GtkTreeModel *model;
	GtkTreeIter  *iter;
	GtkTreePath  *path;

	model = GTK_TREE_MODEL (view->priv->model);

	if ((iter = glade_util_find_iter_by_widget 
	     (model, findme, WIDGET_COLUMN)) != NULL)
	{
		path = gtk_tree_model_get_path (model, iter);
		gtk_tree_model_row_changed (model, path, iter);
		gtk_tree_iter_free (iter);
	}
}

static void
glade_project_view_selection_update (GladeProjectView *view,
				     GladeProject *project)
{
	GladeWidget      *widget;
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter      *iter;
	GList            *list;

	g_return_if_fail (GLADE_IS_PROJECT_VIEW (view));
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (view->priv->project == project);


	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->priv->tree_view));
	model     = GTK_TREE_MODEL (view->priv->model);

	g_return_if_fail (selection != NULL);

	view->priv->updating_treeview = TRUE;

	gtk_tree_selection_unselect_all (selection);

	for (list = glade_project_selection_get (project);
	     list && list->data; list = list->next)
	{	
		if ((widget = glade_widget_get_from_gobject
		     (G_OBJECT (list->data))) != NULL)
		{
			if ((iter = glade_util_find_iter_by_widget 
			     (model, widget, WIDGET_COLUMN)) != NULL)
			{
				gtk_tree_selection_select_iter (selection, iter);
				gtk_tree_iter_free (iter);
			}
		}
	}

	view->priv->updating_treeview = FALSE;
}      

/******************************************************************
 *                    Handled project signals                     *
 ******************************************************************/
static void
glade_project_view_add_widget_cb (GladeProject   *project,
				  GladeWidget    *widget,
				  GPVProjectData *pdata)
{
	if (pdata->project == pdata->view->priv->project)
		GLADE_PROJECT_VIEW_GET_CLASS
			(pdata->view)->add_item (pdata->view, widget);
}

static void
glade_project_view_remove_widget_cb (GladeProject   *project,
				     GladeWidget    *widget,
				     GPVProjectData *pdata)
{
	if (pdata->project == pdata->view->priv->project)
		GLADE_PROJECT_VIEW_GET_CLASS
			(pdata->view)->remove_item (pdata->view, widget);
}

static void
glade_project_view_widget_name_changed_cb (GladeProject   *project,
					   GladeWidget    *widget,
					   GPVProjectData *pdata)
{
	if (pdata->project == pdata->view->priv->project)
		GLADE_PROJECT_VIEW_GET_CLASS
			(pdata->view)->widget_name_changed (pdata->view, widget);
}

static void
glade_project_view_selection_update_cb (GladeProject   *project,
					GPVProjectData *pdata)
{
	/* This view caused this selection change in the project, since we
	 * already know about it we don't need updating. Chema
	 */
	if (pdata->view->priv->updating_selection)
		return;

	if (pdata->project == pdata->view->priv->project)
		GLADE_PROJECT_VIEW_GET_CLASS
			(pdata->view)->selection_update (pdata->view, project);
}

static void
glade_project_view_close_cb (GladeProject   *project,
			     GPVProjectData *pdata)
{
	pdata->view->priv->project_data = 
		g_list_remove (pdata->view->priv->project_data, pdata);

	/* Clear project view if the closing project is current */
	if (pdata->project == pdata->view->priv->project)
		glade_project_view_set_project (pdata->view, NULL);

	gpv_project_data_free (pdata);
}

/******************************************************************
 *              GPVProjectData management functions               *
 ******************************************************************/
static gint
gpv_find_project (GPVProjectData *pdata, 
		  GladeProject   *project)
{
	return (pdata->project != project);
}


gboolean
gpv_accum_expanded (GtkTreeModel  *model,
		    GtkTreePath   *path,
		    GtkTreeIter   *iter,
		    GPVAccumPair  *pair)
{
	GladeWidget *gwidget;
	if (gtk_tree_view_row_expanded (pair->view, path))
	{
		/* Get the widget */
		gtk_tree_model_get (model, iter, 
				    WIDGET_COLUMN, &gwidget, -1);
		pair->list = g_list_prepend (pair->list, gwidget);
	}
	return FALSE;
}

static GPVProjectData *
gpv_project_data_new (GladeProjectView *view,
		      GladeProject     *project)
{
	GPVProjectData *pdata = g_new0 (GPVProjectData, 1);

	pdata->view    = view;
	pdata->project = project;

	/* Here we connect to all the signals of the project that interest us */
	pdata->add_widget_signal_id =
		g_signal_connect (G_OBJECT (project), "add_widget",
				  G_CALLBACK (glade_project_view_add_widget_cb),
				  pdata);
	pdata->remove_widget_signal_id =
		g_signal_connect (G_OBJECT (project), "remove_widget",
				  G_CALLBACK (glade_project_view_remove_widget_cb),
				  pdata);
	pdata->widget_name_changed_signal_id =
		g_signal_connect (G_OBJECT (project), "widget_name_changed",
				  G_CALLBACK (glade_project_view_widget_name_changed_cb),
				  pdata);
	pdata->selection_changed_signal_id =
		g_signal_connect (G_OBJECT (project), "selection_changed",
				  G_CALLBACK (glade_project_view_selection_update_cb),
				  pdata);
	pdata->project_closed_signal_id =
		g_signal_connect (G_OBJECT (project), "close",
				  G_CALLBACK (glade_project_view_close_cb),
				  pdata);
	return pdata;
}

static void
gpv_project_data_free (GPVProjectData *pdata)
{
	/* Disconnect from project */
	g_signal_handler_disconnect (G_OBJECT (pdata->project),
				     pdata->add_widget_signal_id);
	g_signal_handler_disconnect (G_OBJECT (pdata->project),
				     pdata->remove_widget_signal_id);
	g_signal_handler_disconnect (G_OBJECT (pdata->project),
				     pdata->widget_name_changed_signal_id);
	g_signal_handler_disconnect (G_OBJECT (pdata->project),
				     pdata->selection_changed_signal_id);
	g_signal_handler_disconnect (G_OBJECT (pdata->project),
				     pdata->project_closed_signal_id);
	
	if (pdata->open_leafs)
		g_list_free (pdata->open_leafs);
	g_free (pdata);
}

static void
gpv_project_data_save_state (GPVProjectData *pdata)
{
	GPVAccumPair pair = { (GtkTreeView *)pdata->view->priv->tree_view, NULL };
	GtkAdjustment *adj;

	if (pdata->open_leafs)
	{
		g_list_free (pdata->open_leafs);
		pdata->open_leafs = NULL;
	}

	/* Save open leafs and scroll position */
	gtk_tree_model_foreach 
		(GTK_TREE_MODEL (pdata->view->priv->model),
		 (GtkTreeModelForeachFunc)gpv_accum_expanded, &pair);
	pdata->open_leafs = g_list_reverse (pair.list);

	adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (pdata->view));
	pdata->hadjust_value = gtk_adjustment_get_value (adj);

	adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (pdata->view));
	pdata->vadjust_value = gtk_adjustment_get_value (adj);

}

static gboolean
gpv_fix_view_area (GPVProjectData *pdata)
{
	GtkAdjustment *adj;

	adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (pdata->view));
	gtk_adjustment_set_value (adj, pdata->hadjust_value);

	adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (pdata->view));
	gtk_adjustment_set_value (adj, pdata->vadjust_value);
	
	return FALSE;
}

static void
gpv_project_data_load_state (GPVProjectData *pdata)
{
	GladeWidget   *gwidget;
	GtkTreePath   *path;
	GtkTreeIter   *iter;
	GList         *list;

	glade_project_view_populate_model (pdata->view);

	/* Open leafs and set scroll position */
	for (list = pdata->open_leafs; list; list = list->next)
	{
		gwidget = list->data;


		if ((iter = glade_util_find_iter_by_widget
		     (GTK_TREE_MODEL (pdata->view->priv->model), 
		      gwidget, WIDGET_COLUMN)) != NULL)
		{
			path = gtk_tree_model_get_path 
				(GTK_TREE_MODEL (pdata->view->priv->model), iter);

			gtk_tree_view_expand_row
				(GTK_TREE_VIEW (pdata->view->priv->tree_view),
				 path, FALSE);

			gtk_tree_path_free (path);
			gtk_tree_iter_free (iter);
		}
	}

	/* Deffer settiong the v/h adjustments till after the treeview
	 * has recieved its proper size allocation.
	 */
	g_idle_add ((GSourceFunc) gpv_fix_view_area, pdata);
}

/******************************************************************
 *                       TreeView callbacks                       *
 ******************************************************************/
static gboolean
glade_project_view_selection_changed_cb (GtkTreeSelection *selection,
					 GladeProjectView  *view)
{
	g_return_val_if_fail (GLADE_IS_PROJECT_VIEW (view), FALSE);

	if (view->priv->project != NULL &&
	    view->priv->updating_treeview == FALSE)
	{
		view->priv->updating_selection = TRUE;

		glade_app_selection_clear (FALSE);
		gtk_tree_selection_selected_foreach
			(selection, gpw_foreach_add_selection, view);
		glade_app_selection_changed ();
		
		view->priv->updating_selection = FALSE;
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
	GtkTreeModel *model;
	GtkTreeIter   iter;
	
	model = gtk_tree_view_get_model (view);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter, WIDGET_COLUMN, &widget, -1);

	glade_widget_show (widget);
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
		if (gtk_tree_model_get_iter (GTK_TREE_MODEL (view->priv->model),
					     &iter, path))
		{
			/* Alright, now we can obtain 
			 * the widget from the iter.
			 */
			gtk_tree_model_get (GTK_TREE_MODEL (view->priv->model), &iter,
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
	GLADE_PROJECT_VIEW_GET_CLASS (view)->selection_update (view, view->priv->project);
}

static void
glade_project_view_cell_function (GtkTreeViewColumn *tree_column,
				  GtkCellRenderer *cell,
				  GtkTreeModel *tree_model,
				  GtkTreeIter *iter,
				  gpointer data)
{
	GPVCellType  type = GPOINTER_TO_INT (data);
	GdkPixbuf   *small_icon = NULL;
	GladeWidget *widget;
	gchar       *text = NULL, *child_type;

	gtk_tree_model_get (tree_model, iter, WIDGET_COLUMN, &widget, -1);

	/* The cell exists, but not widget has been asociated with it */
	if (!GLADE_IS_WIDGET (widget)) return;

	g_return_if_fail (widget->name != NULL);
	g_return_if_fail (widget->adaptor != NULL);
	g_return_if_fail (widget->adaptor->name != NULL);

	g_object_get (widget->adaptor, "small-icon", &small_icon, NULL);
	g_return_if_fail (small_icon != NULL);

	switch (type) 
	{
	case CELL_ICON:
		g_object_set (G_OBJECT (cell), "pixbuf", small_icon, NULL);
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

	g_object_unref (small_icon);
}

static void
glade_project_view_class_init (GladeProjectViewClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	parent_class = g_type_class_peek_parent (class);

	class->add_item             = glade_project_view_add_item;
	class->remove_item          = glade_project_view_remove_item;
	class->widget_name_changed  = glade_project_view_widget_name_changed;
	class->selection_update     = glade_project_view_selection_update;

	/**
	 * GladeProjectView::item-activated:
	 * @view the #GladeProjectView which received the signal.
	 * @widget: the #GladeWidget that was activated.
	 *
	 * Emitted when a item is activated in the GtkTreeView of a GladeProjectView.
	 */
	glade_project_view_signals[ITEM_ACTIVATED] =
		g_signal_new ("item_activated",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeProjectViewClass, item_activated),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GLADE_TYPE_WIDGET);
	
	g_type_class_add_private (object_class, sizeof (GladeProjectViewPrivate));
}

static gboolean
glade_project_view_search_func (GtkTreeModel *model,
				gint column,
				const gchar *key,
				GtkTreeIter *iter,
				gpointer search_data)
{
	GladeWidget *widget;

	gtk_tree_model_get (model, iter, WIDGET_COLUMN, &widget, -1);
	
	if (!widget) return TRUE;
	
	g_return_val_if_fail (widget->name != NULL, TRUE);
	
	return !g_str_has_prefix (widget->name, key);
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
	g_object_set (G_OBJECT (renderer), 
		      "xpad", 6, NULL);
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
	
	/* Set search column */
	gtk_tree_view_set_search_equal_func (view, glade_project_view_search_func, NULL, NULL);
	gtk_tree_view_set_enable_search (view, TRUE);
	gtk_tree_view_set_search_column (view, WIDGET_COLUMN);
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
	GladeProjectViewPrivate *priv;
	GtkTreeSelection *selection;
	
	view->priv = priv = GLADE_PROJECT_VIEW_GET_PRIVATE (view);
	
	priv->project = NULL;

	priv->model = gtk_tree_store_new (N_COLUMNS, G_TYPE_POINTER);
	glade_project_view_populate_model (view);

#if 0
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (view->model),
					      0, GTK_SORT_ASCENDING);

	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (view->model), 0,
					 glade_project_view_sort_func,
					 NULL, NULL);
#endif
 
	priv->tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (priv->model));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->tree_view), FALSE);

	/* the view now holds a reference, we can get rid of our own */
	g_object_unref (G_OBJECT (priv->model));

	glade_project_view_add_columns (GTK_TREE_VIEW (priv->tree_view));

	/* Show/Hide windows on double-click */
	g_signal_connect (G_OBJECT (priv->tree_view), "row-activated",
			  G_CALLBACK (glade_project_view_item_activated_cb), NULL);

	/* Update project selection */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree_view));
	g_signal_connect_data (G_OBJECT (selection), "changed",
			       G_CALLBACK (glade_project_view_selection_changed_cb),
			       view, NULL, 0);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	/* Popup menu */
	g_signal_connect (G_OBJECT (priv->tree_view), "button-press-event",
			  G_CALLBACK (glade_project_view_button_press_cb), view);
	
	/* Refresh selection */
	g_signal_connect (G_OBJECT (priv->tree_view), "row-expanded",
			  G_CALLBACK (glade_project_view_row_cb), view);
	g_signal_connect (G_OBJECT (priv->tree_view), "row-collapsed",
			  G_CALLBACK (glade_project_view_row_cb), view);

	priv->updating_selection = FALSE;
	priv->updating_treeview  = FALSE;
}

/**
 * glade_project_view_new:
 * 
 * Creates a new #GladeProjectView
 * 
 * Returns: a new #GladeProjectView
 */
GtkWidget *
glade_project_view_new (void)
{
	GladeProjectView *view;
	
	view = g_object_new (GLADE_TYPE_PROJECT_VIEW, 
			     "hscrollbar-policy", GTK_POLICY_AUTOMATIC,
			     "vscrollbar-policy", GTK_POLICY_AUTOMATIC,
			     "shadow-type", GTK_SHADOW_IN,
			     NULL);

 	gtk_container_add (GTK_CONTAINER (view), view->priv->tree_view);
	
	return GTK_WIDGET (view);
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
	GPVProjectData *pdata;
	GList          *list;

	g_return_if_fail (GLADE_IS_PROJECT_VIEW (view));

	/* Save ourself the trouble */
	if (view->priv->project == project)
		return;

	/* Save old project state
	 */
	if (view->priv->project != NULL &&
	    (list = g_list_find_custom (view->priv->project_data, 
					view->priv->project, 
					(GCompareFunc)gpv_find_project)) != NULL)
	{
		pdata = list->data;
		gpv_project_data_save_state (pdata);
	}

	/* Make sure to clear the store
	 */
	gtk_tree_store_clear (view->priv->model);

	/* Set the current project and proceed */
	view->priv->project = project;

	/* Load new project state
	 */
	if (project != NULL)
	{
		/* Create a new project record if we dont already have one.
		 */
		if ((list = 
		     g_list_find_custom (view->priv->project_data, 
					 project, 
					 (GCompareFunc)gpv_find_project)) == NULL)
		{
			pdata = gpv_project_data_new (view, project);
			view->priv->project_data = 
				g_list_prepend (view->priv->project_data, pdata);
		}
		else
		{
			pdata = list->data;
		}	
		gpv_project_data_load_state (pdata);
	}

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
	g_return_val_if_fail (GLADE_IS_PROJECT_VIEW (view), NULL);

	return view->priv->project;
}

/**
 * glade_project_view_expand_all:
 * @view: a #GladeProjectView
 * 
 * Expands all rows in the project view.
 */
void
glade_project_view_expand_all (GladeProjectView *view)
{
	g_return_if_fail (GLADE_IS_PROJECT_VIEW (view));

	gtk_tree_view_expand_all (GTK_TREE_VIEW (view->priv->tree_view));
}

/**
 * glade_project_view_collapse_all:
 * @view: a #GladeProjectView
 * 
 * Collapses all rows in the project view.
 */
void
glade_project_view_collapse_all (GladeProjectView *view)
{
	g_return_if_fail (GLADE_IS_PROJECT_VIEW (view));

	gtk_tree_view_collapse_all (GTK_TREE_VIEW (view->priv->tree_view));
}
