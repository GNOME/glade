/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Juan Pablo Ugarte.
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
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#include <config.h>

#include <gladeui/glade.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include "glade-column-types.h"
#include "glade-model-data.h"

enum
{
	COLUMN_NAME,
	COLUMN_GTYPE,
	COLUMN_COLUMN_NAME,
	N_COLUMNS
};

static GtkTreeModel *types_model;

static GtkListStore *
column_types_store_new ()
{
	return gtk_list_store_new (N_COLUMNS,
				   G_TYPE_STRING,
				   G_TYPE_GTYPE,
				   G_TYPE_STRING);
}

static void
column_types_store_populate (GtkListStore *store)
{
	GtkTreeIter iter;
	gint i;
	GType types[] = {
		G_TYPE_CHAR,
		G_TYPE_UCHAR,
		G_TYPE_BOOLEAN,
		G_TYPE_INT,
		G_TYPE_UINT,
		G_TYPE_LONG,
		G_TYPE_ULONG,
		G_TYPE_INT64,
		G_TYPE_UINT64,
		G_TYPE_FLOAT,
		G_TYPE_DOUBLE,
		G_TYPE_STRING,
		G_TYPE_POINTER,
		G_TYPE_BOXED,
		G_TYPE_PARAM,
		G_TYPE_OBJECT,
		GDK_TYPE_PIXBUF};
	
	for (i = 0; i < sizeof (types) / sizeof (GType); i++)
	{
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    COLUMN_NAME, g_type_name (types[i]),
				    COLUMN_GTYPE, types[i],
				    -1);
	}
}

GList *
glade_column_list_copy (GList *list)
{
	GList *l, *retval = NULL;
	
	for (l = list; l; l = g_list_next (l))
	{
		GladeColumnType *new_data = g_new0(GladeColumnType, 1);
		GladeColumnType *data = l->data;
		
		new_data->type = data->type;
		new_data->column_name = g_strdup (data->column_name);
		
		retval = g_list_prepend (retval, new_data);
	}
	
	return g_list_reverse (retval);
}

void
glade_column_list_free (GList *list)
{
	GList *l;
	
	for (l = list; l; l = g_list_next (l))
	{
		GladeColumnType *data = l->data;
		
		g_free (data->column_name);
		g_free (data);
	}
	
	g_list_free (list);
}

GType
glade_column_type_list_get_type (void)
{
	static GType type_id = 0;

	if (!type_id)
		type_id = g_boxed_type_register_static
			("GladeColumnTypeList", 
			 (GBoxedCopyFunc) glade_column_list_copy,
			 (GBoxedFreeFunc) glade_column_list_free);
	return type_id;
}

/********************** GladeParamSpecColumnTypes  ***********************/

struct _GladeParamSpecColumnTypes
{
	GParamSpec parent_instance;
};

static gint
param_values_cmp (GParamSpec *pspec, const GValue *value1, const GValue *value2)
{
	GList *l1, *l2;
	gint retval;
	
	l1 = g_value_get_boxed (value1);
	l2 = g_value_get_boxed (value2);
	
	if (l1 == NULL && l2 == NULL) return 0;
	
	if (l1 == NULL || l2 == NULL) return l1 - l2;
	
	if ((retval = g_list_length (l1) - g_list_length (l2)))
		return retval;
	else
		return GPOINTER_TO_INT (l1->data) - GPOINTER_TO_INT (l2->data);
}

GType
glade_param_column_types_get_type (void)
{
	static GType accel_type = 0;

	if (accel_type == 0)
	{
		 GParamSpecTypeInfo pspec_info = {
			sizeof (GladeParamSpecColumnTypes),  /* instance_size */
			16,   /* n_preallocs */
			NULL, /* instance_init */
			0,    /* value_type, assigned further down */
			NULL, /* finalize */
			NULL, /* value_set_default */
			NULL, /* value_validate */
			param_values_cmp, /* values_cmp */
		};
		pspec_info.value_type = GLADE_TYPE_COLUMN_TYPE_LIST;

		accel_type = g_param_type_register_static
			("GladeParamSpecColumnTypes", &pspec_info);
		types_model = GTK_TREE_MODEL (column_types_store_new ());
		column_types_store_populate (GTK_LIST_STORE (types_model));
	}
	return accel_type;
}

GParamSpec *
glade_standard_column_types_spec (void)
{
	GladeParamSpecColumnTypes *pspec;

	pspec = g_param_spec_internal (GLADE_TYPE_PARAM_COLUMN_TYPES,
				       "column-types", _("Column Types"), 
				       _("A list of GTypes and thier names"),
				       G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	return G_PARAM_SPEC (pspec);
}

/**************************** GladeEditorProperty *****************************/
typedef struct
{
	GladeEditorProperty parent_instance;

	GtkComboBox *combo;
	GtkListStore *store;
	GtkTreeSelection *selection;
} GladeEPropColumnTypes;

GLADE_MAKE_EPROP (GladeEPropColumnTypes, glade_eprop_column_types)
#define GLADE_EPROP_COLUMN_TYPES(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_COLUMN_TYPES, GladeEPropColumnTypes))
#define GLADE_EPROP_COLUMN_TYPES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_COLUMN_TYPES, GladeEPropColumnTypesClass))
#define GLADE_IS_EPROP_COLUMN_TYPES(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_COLUMN_TYPES))
#define GLADE_IS_EPROP_COLUMN_TYPES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_COLUMN_TYPES))
#define GLADE_EPROP_COLUMN_TYPES_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_COLUMN_TYPES, GladeEPropColumnTypesClass))

static void
glade_eprop_column_types_finalize (GObject *object)
{
	/* Chain up */
	GObjectClass *parent_class = g_type_class_peek_parent (G_OBJECT_GET_CLASS (object));

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
eprop_reload_value (GladeEPropColumnTypes *eprop_types)
{
	GladeEditorProperty *eprop = GLADE_EDITOR_PROPERTY (eprop_types);
	GtkTreeModel *model = GTK_TREE_MODEL (eprop_types->store);
	GValue value = {0, };
	GtkTreeIter iter;
	GList *list = NULL, *l;
	GNode *data_tree = NULL;
	
	if (gtk_tree_model_get_iter_first (model, &iter))
	{
		do
		{
			GladeColumnType *data = g_new0(GladeColumnType, 1);

			gtk_tree_model_get (model, &iter,
					    COLUMN_COLUMN_NAME, &data->column_name,
					    COLUMN_GTYPE, &data->type,
					    -1);
		
			list = g_list_append (list, data);
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	glade_widget_property_get (eprop->property->widget, "data", &data_tree);
	if (data_tree)
	{
		GNode *row, *iter;
		gint colnum;
		data_tree = glade_model_data_tree_copy (data_tree);

		glade_command_push_group (_("Setting columns of %s"), eprop->property->widget->name);

		/* Remove extra columns */
		for (row = data_tree->children; row; row = row->next)
		{

			for (colnum = 0, iter = row->children; iter; 
			     colnum++, iter = iter->next)
			{

			}
		}

		g_value_init (&value, GLADE_TYPE_MODEL_DATA_TREE);
		g_value_take_boxed (&value, data_tree);
		glade_editor_property_commit (eprop, &value);
		g_value_unset (&value);

	}

	g_value_init (&value, GLADE_TYPE_COLUMN_TYPE_LIST);
	g_value_take_boxed (&value, list);
	glade_editor_property_commit (eprop, &value);
	g_value_unset (&value);

	if (data_tree)
	{
		glade_model_data_tree_free (data_tree);
		glade_command_pop_group ();
	}
}

static void
eprop_column_append (GladeEPropColumnTypes *eprop_types,
		     GType type,
		     const gchar *name,
		     const gchar *column_name)
{
	gtk_list_store_insert_with_values (eprop_types->store, NULL, -1,
					   COLUMN_NAME, name ? name : g_type_name (type),
					   COLUMN_GTYPE, type,
					   COLUMN_COLUMN_NAME, column_name,
					   -1);
}

static void
glade_eprop_column_types_load (GladeEditorProperty *eprop, GladeProperty *property)
{
	GladeEditorPropertyClass *parent_class = 
		g_type_class_peek_parent (GLADE_EDITOR_PROPERTY_GET_CLASS (eprop));
	GladeEPropColumnTypes *eprop_types = GLADE_EPROP_COLUMN_TYPES (eprop);
	GList *l, *list;
	
	/* Chain up first */
	parent_class->load (eprop, property);
	
	/* Clear Store */
	gtk_list_store_clear (eprop_types->store);
	/* We could set the combo to the first item */
	
	list = g_value_get_boxed (property->value);
	
	for (l = list; l; l = g_list_next (l))
	{
		GladeColumnType *data = l->data;
		
		eprop_column_append (eprop_types, data->type, NULL, data->column_name);
	}
}

static void
glade_eprop_column_types_add_clicked (GtkWidget *button, 
				      GladeEPropColumnTypes *eprop_types)
{
	GtkTreeIter iter;
	GType type2add;
	gchar *name;
	
	if (gtk_combo_box_get_active_iter (eprop_types->combo, &iter) == FALSE)
		return;
	
	gtk_tree_model_get (types_model, &iter,
			    COLUMN_NAME, &name,
			    COLUMN_GTYPE, &type2add,
			    -1);
	
	eprop_column_append (eprop_types, type2add, name, NULL);
	eprop_reload_value (eprop_types);
}

static void
glade_eprop_column_types_delete_clicked (GtkWidget *button, 
					 GladeEPropColumnTypes *eprop_types)
{
	GtkTreeIter iter;
	if (gtk_tree_selection_get_selected (eprop_types->selection, NULL, &iter))
		gtk_list_store_remove (GTK_LIST_STORE (eprop_types->store), &iter);
}

static gboolean
eprop_reload_value_idle (gpointer data)
{
	eprop_reload_value (GLADE_EPROP_COLUMN_TYPES (data));
	return FALSE;
}

static void
eprop_treeview_row_deleted (GtkTreeModel *tree_model,
			    GtkTreePath  *path,
			    GladeEditorProperty *eprop)
{

	if (eprop->loading) return;
	eprop_reload_value_idle (eprop);
}

static GtkWidget *
glade_eprop_column_types_create_input (GladeEditorProperty *eprop)
{
	GladeEPropColumnTypes *eprop_types = GLADE_EPROP_COLUMN_TYPES (eprop);
	GtkWidget *vbox, *hbox, *button, *swin, *treeview;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *col;

	vbox = gtk_vbox_new (FALSE, 2);
	
	hbox = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	
	eprop_types->combo = GTK_COMBO_BOX (gtk_combo_box_new_with_model (types_model));
	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (eprop_types->combo),
				    cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (eprop_types->combo),
					cell, "text", COLUMN_NAME, NULL);
	gtk_combo_box_set_active (eprop_types->combo, 0);
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (eprop_types->combo), TRUE, TRUE, 0);

	button = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON));
	gtk_box_pack_start (GTK_BOX (hbox), button,  FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_eprop_column_types_add_clicked), 
			  eprop_types);
	
	button = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON));
	gtk_box_pack_start (GTK_BOX (hbox), button,  FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_eprop_column_types_delete_clicked), 
			  eprop_types);
	
	swin = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_box_pack_start (GTK_BOX (vbox), swin, TRUE, TRUE, 0);
	
	eprop_types->store = column_types_store_new ();
	g_signal_connect (eprop_types->store, "row-deleted",
			  G_CALLBACK (eprop_treeview_row_deleted),
			  eprop);
	
	treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (eprop_types->store));
	eprop_types->selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	
	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (treeview), TRUE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
	col = gtk_tree_view_column_new_with_attributes ("Type Name", 
							gtk_cell_renderer_text_new (),
							"text", COLUMN_NAME,
							NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), col);	
	gtk_container_add (GTK_CONTAINER (swin), treeview);
	
	gtk_widget_show_all (vbox);
	return vbox;
}
