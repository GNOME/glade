/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Tristan Van Berkom.
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
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#include <config.h>

#include <gladeui/glade.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include "glade-model-data.h"
#include "glade-column-types.h"

GladeModelData *
glade_model_data_new (GType type)
{
	GladeModelData *data = g_new0 (GladeModelData, 1);
	g_value_init (&data->value, type);

	if (type == G_TYPE_STRING)
		data->i18n_translatable = TRUE;

	return data;
}

GladeModelData *
glade_model_data_copy (GladeModelData *data)
{
	if (!data)
		return NULL;

	GladeModelData *dup = g_new0 (GladeModelData, 1);
	
	g_value_init (&dup->value, G_VALUE_TYPE (&data->value));
	g_value_copy (&data->value, &dup->value);

	dup->name              = g_strdup (data->name);

	dup->i18n_translatable = data->i18n_translatable;
	dup->i18n_context      = g_strdup (data->i18n_context);
	dup->i18n_comment      = g_strdup (data->i18n_comment);

	return dup;
}

void
glade_model_data_free (GladeModelData *data)
{
	if (data)
	{
		g_value_unset (&data->value);
	
		g_free (data->name);
		g_free (data->i18n_context);
		g_free (data->i18n_comment);
		g_free (data);
	}
}

GNode *
glade_model_data_tree_copy (GNode *node)
{
	return g_node_copy_deep (node, (GCopyFunc)glade_model_data_copy, NULL);
}

static void
model_data_traverse_free (GNode *node,
			  gpointer data)
{
	glade_model_data_free ((GladeModelData *)node->data);
}

void
glade_model_data_tree_free (GNode *node)
{
	if (node)
	{
		g_node_traverse (node, G_IN_ORDER, G_TRAVERSE_ALL, -1, 
				 (GNodeTraverseFunc)model_data_traverse_free, NULL);
		g_node_destroy (node);
	}
}

void
glade_model_data_insert_column (GNode          *node,
				GType           type,
				gint            nth)
{
	GNode *row, *item;
	GladeModelData *data;

	g_return_if_fail (node != NULL);

	for (row = node->children; row; row = row->next)
	{
		g_return_if_fail (nth >= 0 && nth <= g_node_n_children (row));

		data = glade_model_data_new (type);
		item = g_node_new (data);
		g_node_insert (row, nth, item);
	}
}

void
glade_model_data_remove_column (GNode          *node,
				GType           type,
				gint            nth)
{
	GNode *row, *item;
	GladeModelData *data;

	g_return_if_fail (node != NULL);

	for (row = node->children; row; row = row->next)
	{
		g_return_if_fail (nth >= 0 && nth < g_node_n_children (row));

		item = g_node_nth_child (row, nth);
		data = item->data;

		glade_model_data_free (data);
		g_node_destroy (item);
	}
}

void
glade_model_data_reorder_column (GNode          *node,
				 gint            column,
				 gint            nth)
{
	GNode *row, *item;

	g_return_if_fail (node != NULL);

	for (row = node->children; row; row = row->next)
	{
		g_return_if_fail (nth >= 0 && nth < g_node_n_children (row));

		item = g_node_nth_child (row, column);

		g_node_unlink (item);
		g_node_insert (row, nth, item);
	}
}

GType
glade_model_data_tree_get_type (void)
{
	static GType type_id = 0;

	if (!type_id)
		type_id = g_boxed_type_register_static
			("GladeModelDataTree", 
			 (GBoxedCopyFunc) glade_model_data_tree_copy,
			 (GBoxedFreeFunc) glade_model_data_tree_free);
	return type_id;
}

/********************** GladeParamModelData  ***********************/

struct _GladeParamModelData
{
	GParamSpec parent_instance;
};

static gint
param_values_cmp (GParamSpec *pspec, const GValue *value1, const GValue *value2)
{
	GNode *n1, *n2;
	gint retval;
	
	n1 = g_value_get_boxed (value1);
	n2 = g_value_get_boxed (value2);
	
	if (n1 == NULL && n2 == NULL) return 0;
	
	if (n1 == NULL || n2 == NULL) return n1 - n2;
	
	if ((retval = 
	     g_node_n_nodes (n1, G_TRAVERSE_ALL) - g_node_n_nodes (n2, G_TRAVERSE_ALL)))
		return retval;
	else
		/* XXX We could do alot better here... but right now we are using strings
		 * and ignoring changes somehow... need to fix that.
		 */
		return GPOINTER_TO_INT (n1->data) - GPOINTER_TO_INT (n2->data);
}

GType
glade_param_model_data_get_type (void)
{
	static GType model_data_type = 0;

	if (model_data_type == 0)
	{
		 GParamSpecTypeInfo pspec_info = {
			 sizeof (GladeParamModelData),  /* instance_size */
			 16,   /* n_preallocs */
			 NULL, /* instance_init */
			 0,    /* value_type, assigned further down */
			 NULL, /* finalize */
			 NULL, /* value_set_default */
			 NULL, /* value_validate */
			 param_values_cmp, /* values_cmp */
		 };
		 pspec_info.value_type = GLADE_TYPE_MODEL_DATA_TREE;
		 
		 model_data_type = g_param_type_register_static
			 ("GladeParamModelData", &pspec_info);
	}
	return model_data_type;
}

GParamSpec *
glade_standard_model_data_spec (void)
{
	GladeParamModelData *pspec;

	pspec = g_param_spec_internal (GLADE_TYPE_PARAM_MODEL_DATA,
				       "model-data", _("Model Data"), 
				       _("A datastore property for GtkTreeModel"),
				       G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	return G_PARAM_SPEC (pspec);
}

/**************************** GladeEditorProperty *****************************/
enum {
	COLUMN_ROW = 0, /* row number */
	NUM_COLUMNS
};

typedef struct
{
	GladeEditorProperty parent_instance;

	GtkTreeView  *view;
	GtkListStore *store;
	GtkTreeSelection *selection;
} GladeEPropModelData;

GLADE_MAKE_EPROP (GladeEPropModelData, glade_eprop_model_data)
#define GLADE_EPROP_MODEL_DATA(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_MODEL_DATA, GladeEPropModelData))
#define GLADE_EPROP_MODEL_DATA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_MODEL_DATA, GladeEPropModelDataClass))
#define GLADE_IS_EPROP_MODEL_DATA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_MODEL_DATA))
#define GLADE_IS_EPROP_MODEL_DATA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_MODEL_DATA))
#define GLADE_EPROP_MODEL_DATA_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_MODEL_DATA, GladeEPropModelDataClass))


static void
append_row (GNode *node, GList *columns)
{
	GladeModelData *data;
	GladeColumnType *column;
	GNode *row;
	GList *list;

	g_assert (node && columns);

	row = g_node_new (NULL);
	g_node_append (node, row);

	for (list = columns; list; list = list->next)
       	{
		column = list->data;
		data = glade_model_data_new (column->type);
		g_node_append_data (row, data);
	}
}

static void
clear_view (GladeEditorProperty *eprop)
{
	GladeEPropModelData *eprop_data = GLADE_EPROP_MODEL_DATA (eprop);
	GtkTreeViewColumn   *column;
	
	/* Clear columns ... */
	while ((column = gtk_tree_view_get_column (eprop_data->view, 0)) != NULL)
		gtk_tree_view_remove_column (eprop_data->view, column);

	/* Clear store ... (this will unref the old store) */
	gtk_tree_view_set_model (eprop_data->view, NULL);
	
}

/* User pressed add: append row and commit values  */
static void
glade_eprop_model_data_add_clicked (GtkWidget *button, 
				    GladeEditorProperty *eprop)
{
	GValue value = { 0, };
	GNode *node = NULL;
	GList *columns = NULL;

	glade_property_get (eprop->property, &node);
	glade_widget_property_get (eprop->property->widget, "columns", &columns);

	if (!columns)
		return;

	clear_view (eprop);

	if (!node)
		node = g_node_new (NULL);
	else
		node = glade_model_data_tree_copy (node);

	append_row (node, columns);

	g_value_init (&value, GLADE_TYPE_MODEL_DATA_TREE);
	g_value_take_boxed (&value, node);
	glade_editor_property_commit (eprop, &value);
	g_value_unset (&value);

}

/* User pressed delete: remove selected row and commit values  */
static void
glade_eprop_model_data_delete_clicked (GtkWidget *button, 
				    GladeEditorProperty *eprop)
{
	GtkTreeIter iter;
	GladeEPropModelData *eprop_data = GLADE_EPROP_MODEL_DATA (eprop);
	/* NOTE: This will trigger row-deleted below... */
	if (gtk_tree_selection_get_selected (eprop_data->selection, NULL, &iter))
		gtk_list_store_remove (GTK_LIST_STORE (eprop_data->store), &iter);
}
 
static void
eprop_treeview_row_deleted (GtkTreeModel *tree_model,
			    GtkTreePath  *path,
			    GladeEditorProperty *eprop)
{
	/* User deleted a row: commit values from treeview to property */
#if 0
	GtkTreeIter iter;
	GladeEPropModelData *eprop_data = GLADE_EPROP_MODEL_DATA (eprop);
	GNode *data_tree = NULL, *row;

	glade_property_get (eprop->property, &data_tree);

	if (gtk_tree_selection_get_selected (eprop_types->selection, NULL, &iter))
	{
		
	}

	for (row = data_tree->children; row; row = row->next


	g_value_init (&value, GLADE_TYPE_MODEL_DATA_TREE);
	g_value_take_boxed (&value, node);
	glade_editor_property_commit (eprop, &value);
	g_value_unset (&value);

#endif
}


static void
glade_eprop_model_data_finalize (GObject *object)
{
	/* Chain up */
	GObjectClass *parent_class = g_type_class_peek_parent (G_OBJECT_GET_CLASS (object));

	GladeEPropModelData *eprop_data = GLADE_EPROP_MODEL_DATA (object);

	if (eprop_data->store)
		g_object_unref (G_OBJECT (eprop_data->store));

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GtkListStore *
eprop_model_data_generate_store (GladeEditorProperty *eprop)
{
	GtkListStore   *store = NULL;
	GladeModelData *iter_data, *row_data;
	GNode          *data_tree = NULL, *iter_node, *row_node;
	GArray         *gtypes = g_array_new (FALSE, TRUE, sizeof (GType));
	GtkTreeIter     iter;
	gint            column_num, row_num;
	GType           index_type = G_TYPE_INT;

	glade_property_get (eprop->property, &data_tree);

	if (!data_tree || !data_tree->children || !data_tree->children->children)
		return NULL;

	/* Generate store with tailored column types */
	g_array_append_val (gtypes, index_type);
	for (iter_node = data_tree->children->children; iter_node; iter_node = iter_node->next)
	{
		iter_data = iter_node->data;
		g_array_append_val (gtypes, G_VALUE_TYPE (&iter_data->value));
	}
	store = gtk_list_store_newv (gtypes->len, (GType *)gtypes->data);
	g_array_free (gtypes, TRUE);

	/* Now populate the store with data */
	for (row_num = 0, row_node = data_tree->children; row_node; 
	     row_num++, row_node = row_node->next)
	{
		row_data = row_node->data;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, COLUMN_ROW, row_num, -1);

		for (column_num = NUM_COLUMNS, iter_node = row_node->children; iter_node; 
		     column_num++, iter_node = iter_node->next)
		{
			iter_data = iter_node->data;
			gtk_list_store_set_value (store, &iter, column_num, &iter_data->value);
		}
	}
	return store;
}


static void
value_toggled (GtkCellRendererToggle *cell,
	       gchar                 *path,
	       GladeEditorProperty *eprop)
{
	GladeEPropModelData *eprop_data = GLADE_EPROP_MODEL_DATA (eprop);
	gboolean             active;
	GtkTreeIter          iter;
	gint                 colnum = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column-number"));

	if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (eprop_data->store), &iter, path))
		return;

	gtk_tree_model_get (GTK_TREE_MODEL (eprop_data->store), &iter,
			    NUM_COLUMNS + colnum, &active,
			    -1);

	gtk_list_store_set (eprop_data->store, &iter,
			    NUM_COLUMNS + colnum, !active,
			    -1);
}

static void
value_text_edited (GtkCellRendererText *cell,
		   const gchar         *path,
		   const gchar         *new_text,
		   GladeEditorProperty *eprop)
{
	GladeEPropModelData *eprop_data = GLADE_EPROP_MODEL_DATA (eprop);
	GtkTreeIter          iter;
	gint                 colnum = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column-number"));
	gint                 row;
	GNode               *data_tree = NULL, *node;
	GValue               value = { 0, };
	GladeModelData      *data;

	if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (eprop_data->store), &iter, path))
		return;


	gtk_tree_model_get (eprop_data->store, &iter,
			    COLUMN_ROW, &row,
			    -1);

	glade_property_get (eprop->property, &data_tree);

	/* if we are editing, then there is data in the datatree */
	g_assert (data_tree);

	data_tree = glade_model_data_tree_copy (data_tree);

	if ((node = g_node_nth_child (data_tree, row)) != NULL)
	{
		if ((node = g_node_nth_child (node, colnum)) != NULL)
		{
			data = node->data;

			g_value_set_string (&(data->value), new_text);
		}
	}

	g_value_init (&value, GLADE_TYPE_MODEL_DATA_TREE);
	g_value_take_boxed (&value, data_tree);
	glade_editor_property_commit (eprop, &value);
	g_value_unset (&value);

	/* No need to update store, it will be reloaded in load() */
/* 	gtk_list_store_set (eprop_data->store, &iter, */
/* 			    NUM_COLUMNS + colnum, new_text, */
/* 			    -1); */

	/* XXX Set string in data here and commit ! */

}

static GtkTreeViewColumn *
eprop_model_generate_column (GladeEditorProperty *eprop,
			     gint                 colnum,
			     GladeModelData      *data)
{
	GtkTreeViewColumn *column = gtk_tree_view_column_new ();
	GtkCellRenderer   *renderer = NULL;
	GtkAdjustment     *adjustment;
	GtkListStore      *store;
	GType              type = G_VALUE_TYPE (&data->value);

	gtk_tree_view_column_set_title (column, g_type_name (type));

	/* Support enum and flag types, and a hardcoded list of fundamental types */
	if (type == G_TYPE_CHAR ||
	    type == G_TYPE_UCHAR ||
	    type == G_TYPE_STRING)
	{
		/* Text renderer */
		renderer = gtk_cell_renderer_text_new ();
		g_object_set (G_OBJECT (renderer), 
			      "editable", TRUE, 
			      NULL);
		gtk_tree_view_column_pack_start (column, renderer, FALSE);
		gtk_tree_view_column_set_attributes (column, renderer, 
						     "text", NUM_COLUMNS + colnum,
						     NULL);

		if (type == G_TYPE_CHAR ||
		    type == G_TYPE_UCHAR)
		{
			/* XXX restrict to 1 char !! */
		}

		g_signal_connect (G_OBJECT (renderer), "edited",
				  G_CALLBACK (value_text_edited), eprop);

		/* Trigger i18n dialog from here somehow ! */
/* 		g_signal_connect (G_OBJECT (renderer), "editing-started", */
/* 				  G_CALLBACK (value_text_editing_started), eprop); */
	}

		/* Text renderer single char */
	else if (type == G_TYPE_BOOLEAN)
	{
		/* Toggle renderer */
		renderer = gtk_cell_renderer_toggle_new ();
		g_object_set (G_OBJECT (renderer), "activatable", TRUE, NULL);
		gtk_tree_view_column_pack_start (column, renderer, FALSE);
		gtk_tree_view_column_set_attributes (column, renderer, 
						     "active", NUM_COLUMNS + colnum,
						     NULL);
		g_signal_connect (G_OBJECT (renderer), "toggled",
				  G_CALLBACK (value_toggled), eprop);
	}
		/* Check renderer */
	else if (type == G_TYPE_INT ||
		 type == G_TYPE_UINT ||
		 type == G_TYPE_LONG ||
		 type == G_TYPE_ULONG ||
		 type == G_TYPE_INT64 ||
		 type == G_TYPE_UINT64 ||
		 type == G_TYPE_FLOAT ||
		 type == G_TYPE_DOUBLE)
	{
		/* Spin renderer */
		renderer = gtk_cell_renderer_spin_new ();
		adjustment = (GtkAdjustment *)gtk_adjustment_new (0, -G_MAXDOUBLE, G_MAXDOUBLE, 100, 100, 100);
		g_object_set (G_OBJECT (renderer), 
			      "editable", TRUE, 
			      "adjustment", adjustment, 
			      NULL);

		gtk_tree_view_column_pack_start (column, renderer, TRUE);
		gtk_tree_view_column_set_attributes (column, renderer, 
						     "text", NUM_COLUMNS + colnum,
						     NULL);

		if (type == G_TYPE_FLOAT ||
		    type == G_TYPE_DOUBLE)
			g_object_set (G_OBJECT (renderer), "digits", 2, NULL);

		g_signal_connect (G_OBJECT (renderer), "edited",
				  G_CALLBACK (value_text_edited), eprop);
		
	}
	else if (G_TYPE_IS_ENUM (type))
	{
		/* Combo renderer */
		renderer = gtk_cell_renderer_combo_new ();
		store = glade_utils_liststore_from_enum_type (type, FALSE);
		g_object_set (G_OBJECT (renderer), 
			      "editable", TRUE, 
			      "text-column", 0,
			      "has-entry", FALSE,
			      "model", store,
			      NULL);
		gtk_tree_view_column_pack_start (column, renderer, TRUE);
		gtk_tree_view_column_set_attributes (column, renderer, 
						     "text", NUM_COLUMNS + colnum,
						     NULL);
		g_signal_connect (G_OBJECT (renderer), "edited",
				  G_CALLBACK (value_text_edited), eprop);

	}
	else if (G_TYPE_IS_FLAGS (type))
	{
		/* Export a flags dialog from glade-editor-property... */
	}
	else if (type == G_TYPE_OBJECT || g_type_is_a (type, G_TYPE_OBJECT))
	{
		/* text renderer and object dialog (or raw text for pixbuf) */;
	}

	g_object_set_data (G_OBJECT (renderer), "column-number", GINT_TO_POINTER (colnum));

	return column;
}

static void
eprop_model_data_generate_columns (GladeEditorProperty *eprop)
{
	GladeEPropModelData *eprop_data = GLADE_EPROP_MODEL_DATA (eprop);
	GladeModelData      *iter_data;
	GtkTreeViewColumn   *column;
	GNode               *data_tree = NULL, *iter_node;
	gint                 colnum;

	glade_property_get (eprop->property, &data_tree);
	
	if (!data_tree || !data_tree->children || !data_tree->children->children)
		return;

	/* Append new columns */
	for (colnum = 0, iter_node = data_tree->children->children; iter_node; 
	     colnum++, iter_node = iter_node->next)
	{
		iter_data = iter_node->data;

		column = eprop_model_generate_column (eprop, colnum, iter_data);
		gtk_tree_view_append_column (eprop_data->view, column);
	}
}

static void
glade_eprop_model_data_load (GladeEditorProperty *eprop, 
			     GladeProperty       *property)
{
	GladeEditorPropertyClass *parent_class = 
		g_type_class_peek_parent (GLADE_EDITOR_PROPERTY_GET_CLASS (eprop));
	GladeEPropModelData *eprop_data = GLADE_EPROP_MODEL_DATA (eprop);

	clear_view (eprop);

	/* Chain up in a clean state... */
	parent_class->load (eprop, property);

	if ((eprop_data->store = eprop_model_data_generate_store (eprop)) != NULL)
	{
		eprop_data->selection = gtk_tree_view_get_selection (eprop_data->view);

		/* Pass ownership of the store to the view... */
		gtk_tree_view_set_model (eprop_data->view, GTK_TREE_MODEL (eprop_data->store));
		g_object_unref (G_OBJECT (eprop_data->store));
		
		g_signal_connect (eprop_data->store, "row-deleted",
				  G_CALLBACK (eprop_treeview_row_deleted),
				  eprop);
	}

	/* Create new columns with renderers */
	eprop_model_data_generate_columns (eprop);
}

static GtkWidget *
glade_eprop_model_data_create_input (GladeEditorProperty *eprop)
{
	GladeEPropModelData *eprop_data = GLADE_EPROP_MODEL_DATA (eprop);
	GtkWidget *vbox, *hbox, *button, *swin;

	vbox = gtk_vbox_new (FALSE, 2);
	
	hbox = gtk_hbox_new (FALSE, 4);

	/* Pack treeview/swindow on the left... */
	swin = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_box_pack_start (GTK_BOX (vbox), swin, TRUE, TRUE, 0);

	/* hbox with add/remove row buttons on the right... */
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	
	button = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON));
	gtk_box_pack_start (GTK_BOX (hbox), button,  FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_eprop_model_data_add_clicked), 
			  eprop_data);
	
	button = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON));
	gtk_box_pack_start (GTK_BOX (hbox), button,  FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_eprop_model_data_delete_clicked), 
			  eprop_data);

	eprop_data->view = (GtkTreeView *)gtk_tree_view_new ();
	
	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (eprop_data->view), TRUE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (eprop_data->view), TRUE);
	gtk_container_add (GTK_CONTAINER (swin), GTK_WIDGET (eprop_data->view));
	
	gtk_widget_show_all (vbox);
	return vbox;
}
