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
	dup->i18n_has_context  = data->i18n_has_context;
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
typedef struct
{
	GladeEditorProperty parent_instance;

	GtkTreeView  *view;
	GtkListStore *store;
} GladeEPropModelData;

GLADE_MAKE_EPROP (GladeEPropModelData, glade_eprop_model_data)
#define GLADE_EPROP_MODEL_DATA(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_MODEL_DATA, GladeEPropModelData))
#define GLADE_EPROP_MODEL_DATA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_MODEL_DATA, GladeEPropModelDataClass))
#define GLADE_IS_EPROP_MODEL_DATA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_MODEL_DATA))
#define GLADE_IS_EPROP_MODEL_DATA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_MODEL_DATA))
#define GLADE_EPROP_MODEL_DATA_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_MODEL_DATA, GladeEPropModelDataClass))


static void
glade_eprop_model_data_add_clicked (GtkWidget *button, 
				    GladeEPropModelData *eprop_types)
{
	/* Add new row with default values */
}

static void
glade_eprop_model_data_delete_clicked (GtkWidget *button, 
					 GladeEPropModelData *eprop_types)
{
	/* User pressed delete: remove selected row and commit values from treeview to property */
	
}
 
static void
eprop_treeview_row_deleted (GtkTreeModel *tree_model,
			    GtkTreePath  *path,
			    GladeEditorProperty *eprop)
{
	/* User deleted a row: commit values from treeview to property */
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
	gint            column_num;

	glade_property_get (eprop->property, &data_tree);


	if (!data_tree || !data_tree->children || !data_tree->children->children)
		return NULL;

	/* Generate store with tailored column types */
	for (iter_node = data_tree->children->children; iter_node; iter_node = iter_node->next)
	{
		iter_data = iter_node->data;
		g_array_append_val (gtypes, G_VALUE_TYPE (&iter_data->value));
	}
	store = gtk_list_store_newv (gtypes->len, (GType *)gtypes->data);
	g_array_free (gtypes, TRUE);

	/* Now populate the store with data */
	for (row_node = data_tree->children; row_node; row_node = row_node->next)
	{
		row_data = row_node->data;

		gtk_list_store_append (store, &iter);

		for (column_num = 0, iter_node = row_node->children; iter_node; 
		     column_num++, iter_node = iter_node->next)
		{
			iter_data = iter_node->data;
			gtk_list_store_set_value (store, &iter, column_num, &iter_data->value);
		}
	}
	return store;
}

static GtkTreeViewColumn *
eprop_model_generate_column (GladeEditorProperty *eprop, 
			     GladeModelData      *data)
{
	GtkTreeViewColumn *column = gtk_tree_view_column_new ();
#if 0
	/* Support enum and flag types, and a hardcoded list of fundamental types */
	if (type == G_TYPE_CHAR)
	else if (type == G_TYPE_UCHAR)
	else if (type == G_TYPE_STRING)
		/* Text renderer single char */
	else if (type == G_TYPE_BOOLEAN)
		/* Check renderer */
	else if (type == G_TYPE_INT)
	else if (type == G_TYPE_UINT)
	else if (type == G_TYPE_LONG)
	else if (type == G_TYPE_ULONG)
	else if (type == G_TYPE_INT64)
	else if (type == G_TYPE_UINT64)
		/* spin renderer with customized adjustments */
	else if (type == G_TYPE_FLOAT)
	else if (type == G_TYPE_DOUBLE)
		/* spin renderer with customized adjustments */
	else if (type == G_TYPE_OBJECT || g_type_is_a (type, G_TYPE_OBJECT))
		/* text renderer and object dialog (or raw text for pixbuf) */
	else if (G_TYPE_IS_ENUM (type))
		/* combobox renderer */
	else if (G_TYPE_IS_FLAGS (type))
#endif
		return NULL;
}

static void
eprop_model_data_generate_columns (GladeEditorProperty *eprop)
{
	GladeEPropModelData *eprop_data = GLADE_EPROP_MODEL_DATA (eprop);
	GladeModelData      *iter_data;
	GtkTreeViewColumn   *column;
	GNode               *data_tree = NULL, *iter_node;

	glade_property_get (eprop->property, &data_tree);
	
	/* Clear columns ... */
	while ((column = gtk_tree_view_get_column (eprop_data->view, 0)) != NULL)
		gtk_tree_view_remove_column (eprop_data->view, column);

	if (!data_tree || !data_tree->children || !data_tree->children->children)
		return;

	/* Append new columns */
	for (iter_node = data_tree->children->children; iter_node; iter_node = iter_node->next)
	{
		iter_data = iter_node->data;

		column = eprop_model_generate_column (eprop, iter_data);
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
	
	/* Chain up first */
	parent_class->load (eprop, property);
	
	/* Recreate and populate store */
	if (eprop_data->store)
		g_object_unref (G_OBJECT (eprop_data->store));
		
	eprop_data->store = eprop_model_data_generate_store (eprop);

	if (eprop_data->store)
		g_signal_connect (eprop_data->store, "row-deleted",
				  G_CALLBACK (eprop_treeview_row_deleted),
				  eprop);

	/* Clear and create new columns with renderers */
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
	gtk_box_pack_start (GTK_BOX (hbox), swin, TRUE, TRUE, 0);

	/* hbox with add/remove row buttons on the right... */
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
	
	button = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON));
	gtk_box_pack_start (GTK_BOX (vbox), button,  FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_eprop_model_data_add_clicked), 
			  eprop_data);
	
	button = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON));
	gtk_box_pack_start (GTK_BOX (vbox), button,  FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_eprop_model_data_delete_clicked), 
			  eprop_data);

	eprop_data->view = (GtkTreeView *)gtk_tree_view_new ();
	
	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (eprop_data->view), TRUE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (eprop_data->view), TRUE);
	gtk_container_add (GTK_CONTAINER (swin), GTK_WIDGET (eprop_data->view));
	
	gtk_widget_show_all (hbox);
	return hbox;
}
