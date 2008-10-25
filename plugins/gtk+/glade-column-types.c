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
 *   Tristan Van Berkom <tvb@gnome.org>
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

static GtkTreeModel *types_model = NULL;

static gint 
find_by_type (GType *a, GType *b)
{
	return *a - *b;
}

static void
column_types_store_populate_enums_flags (GtkTreeStore *store,
					 gboolean      enums)
{
	GtkTreeIter iter, parent_iter;
	GList *types = NULL, *list, *l;
	GList *adaptors = glade_widget_adaptor_list_adaptors ();

	gtk_tree_store_append (store, &parent_iter, NULL);
	gtk_tree_store_set (store, &parent_iter,
			    COLUMN_NAME, enums ? _("Enumerations") : _("Flags"),
			    -1);

	for (list = adaptors; list; list = list->next)
	{
		GladeWidgetAdaptor *adaptor = list->data;
		GladePropertyClass *pclass;

		for (l = adaptor->properties; l; l = l->next)
		{
			pclass = l->data;

			/* special case out a few of these... */
			if (strcmp (g_type_name (pclass->pspec->value_type), "GladeGtkGnomeUIInfo") == 0 ||
			    strcmp (g_type_name (pclass->pspec->value_type), "GladeStock") == 0 ||
			    strcmp (g_type_name (pclass->pspec->value_type), "GladeStockImage") == 0 ||
			    strcmp (g_type_name (pclass->pspec->value_type), "GladeGtkImageType") == 0 ||
			    strcmp (g_type_name (pclass->pspec->value_type), "GladeGtkButtonType") == 0)
				continue;

			if ((enums ? G_TYPE_IS_ENUM (pclass->pspec->value_type) : 
			     G_TYPE_IS_FLAGS (pclass->pspec->value_type)) &&
			    !g_list_find_custom (types, &(pclass->pspec->value_type), 
						 (GCompareFunc)find_by_type))
			{
				types = g_list_prepend (types, 
							g_memdup (&(pclass->pspec->value_type), 
								  sizeof (GType)));

				gtk_tree_store_append (store, &iter, &parent_iter);
				gtk_tree_store_set (store, &iter,
						    COLUMN_NAME, g_type_name (pclass->pspec->value_type),
						    COLUMN_GTYPE, pclass->pspec->value_type,
						    -1);
			}
		}
	}
	g_list_free (adaptors);
	g_list_foreach (types, (GFunc)g_free, NULL);
	g_list_free (types);
}

static void
column_types_store_populate (GtkTreeStore *store)
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
		G_TYPE_OBJECT,
		GDK_TYPE_PIXBUF};
	
	for (i = 0; i < sizeof (types) / sizeof (GType); i++)
	{
		gtk_tree_store_append (store, &iter, NULL);
		gtk_tree_store_set (store, &iter,
				    COLUMN_NAME, g_type_name (types[i]),
				    COLUMN_GTYPE, types[i],
				    -1);
	}

	column_types_store_populate_enums_flags (store, TRUE);
	column_types_store_populate_enums_flags (store, FALSE);
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
glade_column_type_free (GladeColumnType *column)
{
	g_free (column->column_name);
	g_free (column);
}

void
glade_column_list_free (GList *list)
{
	g_list_foreach (list, (GFunc)glade_column_type_free, NULL);
	g_list_free (list);
}

GladeColumnType *
glade_column_list_find_column (GList *list, const gchar *column_name)
{
	GladeColumnType *column = NULL, *iter;
	GList *l;

	for (l = g_list_first (list); l; l = l->next)
	{
		iter = l->data;
		if (strcmp (column_name, iter->column_name) == 0)
		{
			column = iter;
			break;
		}
	}

	return column;
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
		types_model = (GtkTreeModel *)gtk_tree_store_new (N_COLUMNS,
								  G_TYPE_STRING,
								  G_TYPE_GTYPE,
								  G_TYPE_STRING);

		column_types_store_populate (GTK_TREE_STORE (types_model));
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

	GtkComboBox      *combo;
	GtkListStore     *store;
	GtkTreeView      *view;
	GtkTreeSelection *selection;

	GladeNameContext *context;

	gboolean          adding_column;
	GtkTreeViewColumn *name_column;
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

static gint
get_extra_column (GNode *data_tree, GList *columns)
{
	GladeModelData *data;
	GNode *iter;
	gint idx = -1;

	/* extra columns trail at the end so walk backwards... */
	for (iter = g_node_last_child (data_tree->children); iter; iter = iter->prev)
	{
		data = iter->data;

		if (!glade_column_list_find_column (columns, data->name))
		{
			idx = g_node_child_position (data_tree->children, iter);
			break;
		}

	}
	return idx;
}

static void
eprop_column_adjust_rows (GladeEditorProperty *eprop, GList *columns)
{
	GladeColumnType *column;
	GNode *data_tree = NULL;
	GladeWidget *widget = eprop->property->widget;
	GList *list;
	GladeProperty *property;
	gint idx;

	property = glade_widget_get_property (widget, "data");
	glade_property_get (property, &data_tree);
	if (!data_tree)
		return;
	data_tree = glade_model_data_tree_copy (data_tree);

	/* Add mising columns and reorder... */
	for (list = g_list_last (columns); list; list = list->prev)
	{
		column = list->data;

		if ((idx = glade_model_data_column_index (data_tree, column->column_name)) < 0)
		{
			glade_model_data_insert_column (data_tree,
							column->type,
							column->column_name,
							0);

		}
		else
			glade_model_data_reorder_column (data_tree, idx, 0);

	}

	/* Remove trailing obsolete */
	while ((idx = get_extra_column (data_tree, columns)) >= 0)
		glade_model_data_remove_column (data_tree, idx);

	glade_command_set_property (property, data_tree);
	glade_model_data_tree_free (data_tree);
}


static void
eprop_column_append (GladeEditorProperty *eprop,
		     GType type,
		     const gchar *column_name)
{
	GladeEPropColumnTypes *eprop_types = GLADE_EPROP_COLUMN_TYPES (eprop);
	GList                 *columns = NULL;
	GladeColumnType       *data;
	GValue                 value = { 0, };

	glade_property_get (eprop->property, &columns);
	if (columns)
		columns = glade_column_list_copy (columns);

	data = g_new0 (GladeColumnType, 1);
	data->column_name = g_strdup (column_name);
	data->type = type;

	columns = g_list_append (columns, data);

	eprop_types->adding_column = TRUE;
	glade_command_push_group (_("Setting columns on %s"), 
				  glade_widget_get_name (eprop->property->widget));

	g_value_init (&value, GLADE_TYPE_COLUMN_TYPE_LIST);
	g_value_take_boxed (&value, columns);
	glade_editor_property_commit (eprop, &value);

	eprop_column_adjust_rows (eprop, columns);
	g_value_unset (&value);

	glade_command_pop_group ();

	eprop_types->adding_column = FALSE;
}

static void
glade_eprop_column_types_add_clicked (GtkWidget *button, 
				      GladeEPropColumnTypes *eprop_types)
{
	GtkTreeIter iter;
	GType type2add;
	gchar *type_name, *column_name;
	
	if (gtk_combo_box_get_active_iter (eprop_types->combo, &iter) == FALSE)
		return;
	
	gtk_tree_model_get (types_model, &iter,
			    COLUMN_GTYPE, &type2add,
			    -1);
	
	type_name = g_ascii_strdown (g_type_name (type2add), -1);
	column_name = glade_name_context_new_name (eprop_types->context, type_name);
	eprop_column_append (GLADE_EDITOR_PROPERTY (eprop_types), type2add, column_name);
	g_free (column_name);
	g_free (type_name);
}

static void
glade_eprop_column_types_delete_clicked (GtkWidget *button, 
					 GladeEditorProperty *eprop)
{
	/* Remove from list and commit value, dont touch the liststore except in load() */
	GladeEPropColumnTypes *eprop_types = GLADE_EPROP_COLUMN_TYPES (eprop);
	GtkTreeIter            iter;
	GList                 *columns = NULL;
	GladeColumnType       *column;
	GValue                 value = { 0, };
	gchar                 *column_name;

	if (gtk_tree_selection_get_selected (eprop_types->selection, NULL, &iter))
	{
		gtk_tree_model_get (GTK_TREE_MODEL (eprop_types->store), &iter,
				    COLUMN_COLUMN_NAME, &column_name, -1);
		g_assert (column_name);

		glade_property_get (eprop->property, &columns);
		if (columns)
			columns = glade_column_list_copy (columns);
		g_assert (columns);

		/* Find and remove the offending column... */
		column = glade_column_list_find_column (columns, column_name);
		g_assert (column);
		columns = g_list_remove (columns, column);
		glade_column_type_free (column);

		glade_command_push_group (_("Setting columns on %s"), 
					  glade_widget_get_name (eprop->property->widget));

		g_value_init (&value, GLADE_TYPE_COLUMN_TYPE_LIST);
		g_value_take_boxed (&value, columns);
		glade_editor_property_commit (eprop, &value);

		eprop_column_adjust_rows (eprop, columns);
		g_value_unset (&value);
		glade_command_pop_group ();

		g_free (column_name);
	}
}

static gboolean
columns_changed_idle (GladeEditorProperty *eprop)
{
	GladeEPropColumnTypes *eprop_types = GLADE_EPROP_COLUMN_TYPES (eprop);
	GladeColumnType       *column;
	GValue                 value = { 0, };
	GList                 *new_list = NULL, *columns = NULL, *list;
	GtkTreeIter            iter;
	gchar                 *column_name;

	glade_property_get (eprop->property, &columns);
	g_assert (columns);
	columns = glade_column_list_copy (columns);

	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (eprop_types->store), &iter))
	{
		do 
		{
			column_name = NULL;
			gtk_tree_model_get (GTK_TREE_MODEL (eprop_types->store), &iter,
					    COLUMN_COLUMN_NAME, &column_name, -1);
			g_assert (column_name);

			column = glade_column_list_find_column (columns, column_name);
			g_assert (column);

			new_list = g_list_prepend (new_list, column);
			g_free (column_name);
		} 
		while (gtk_tree_model_iter_next (GTK_TREE_MODEL (eprop_types->store), &iter));
	}

	/* any missing columns to free ? */
	for (list = columns; list; list = list->next)
	{
		if (!g_list_find (new_list, list->data))
			glade_column_type_free ((GladeColumnType *)list->data);
	}
	g_list_free (columns);

	glade_command_push_group (_("Setting columns on %s"), 
				  glade_widget_get_name (eprop->property->widget));

	g_value_init (&value, GLADE_TYPE_COLUMN_TYPE_LIST);
	g_value_take_boxed (&value, g_list_reverse (new_list));
	glade_editor_property_commit (eprop, &value);

	eprop_column_adjust_rows (eprop, new_list);
	g_value_unset (&value);
	glade_command_pop_group ();

	return FALSE;
}

static void
eprop_treeview_row_deleted (GtkTreeModel *tree_model,
			    GtkTreePath  *path,
			    GladeEditorProperty *eprop)
{
	if (eprop->loading) return;

	g_idle_add ((GSourceFunc)columns_changed_idle, eprop);
}

static void
eprop_column_load (GladeEPropColumnTypes *eprop_types,
		   GType type,
		   const gchar *column_name)
{
	gtk_list_store_insert_with_values (eprop_types->store, NULL, -1,
					   COLUMN_NAME, g_type_name (type),
					   COLUMN_GTYPE, type,
					   COLUMN_COLUMN_NAME, column_name,
					   -1);
}

static gboolean
eprop_types_focus_idle (GladeEPropColumnTypes *eprop_types)
{
	/* Focus and edit the first column of a newly added row */
	if (eprop_types->store)
	{
		GtkTreePath *new_item_path;
		GtkTreeIter  iter, *last_iter = NULL;

		if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (eprop_types->store), &iter))
		{
			do {
				if (last_iter)
					gtk_tree_iter_free (last_iter);
				last_iter = gtk_tree_iter_copy (&iter);
			} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (eprop_types->store), &iter));

			new_item_path = gtk_tree_model_get_path (GTK_TREE_MODEL (eprop_types->store), last_iter);

			gtk_widget_grab_focus (GTK_WIDGET (eprop_types->view));
			gtk_tree_view_expand_to_path (eprop_types->view, new_item_path);
			gtk_tree_view_set_cursor (eprop_types->view, new_item_path,
						  eprop_types->name_column, TRUE);
			
			gtk_tree_path_free (new_item_path);
			gtk_tree_iter_free (last_iter);
		}
	}
	return FALSE;
}

static void
glade_eprop_column_types_load (GladeEditorProperty *eprop, GladeProperty *property)
{
	GladeEditorPropertyClass *parent_class = 
		g_type_class_peek_parent (GLADE_EDITOR_PROPERTY_GET_CLASS (eprop));
	GladeEPropColumnTypes *eprop_types = GLADE_EPROP_COLUMN_TYPES (eprop);
	GList *l, *list = NULL;
	
	/* Chain up first */
	parent_class->load (eprop, property);
	
	if (eprop_types->context)
		glade_name_context_destroy (eprop_types->context);
	eprop_types->context = NULL;

	if (!property) return;

	eprop_types->context = glade_name_context_new ();

	g_signal_handlers_block_by_func (G_OBJECT (eprop_types->store), 
					 eprop_treeview_row_deleted, eprop);

	/* Clear Store */
	gtk_list_store_clear (eprop_types->store);

	glade_property_get (property, &list);
	
	for (l = list; l; l = g_list_next (l))
	{
		GladeColumnType *data = l->data;
		
		eprop_column_load (eprop_types, data->type, data->column_name);
		glade_name_context_add_name (eprop_types->context, data->column_name);
	}

	if (eprop_types->adding_column && list)
		g_idle_add ((GSourceFunc)eprop_types_focus_idle, eprop);

	g_signal_handlers_unblock_by_func (G_OBJECT (eprop_types->store), 
					   eprop_treeview_row_deleted, eprop);
}

static void
column_name_edited (GtkCellRendererText *cell,
		    const gchar         *path,
		    const gchar         *new_column_name,
		    GladeEditorProperty *eprop)
{
	GladeEPropColumnTypes *eprop_types = GLADE_EPROP_COLUMN_TYPES (eprop);
	GtkTreeIter            iter;
	gchar                 *old_column_name = NULL, *column_name;
	GList                 *columns = NULL;
	GladeColumnType       *column;
	GValue                 value = { 0, };
	GNode                 *data_tree = NULL;
	GladeProperty         *property;

	if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (eprop_types->store), &iter, path))
		return;

	gtk_tree_model_get (GTK_TREE_MODEL (eprop_types->store), &iter, COLUMN_COLUMN_NAME, &old_column_name, -1);

	/* Attempt to rename the column, and commit if successfull... */
	glade_property_get (eprop->property, &columns);
	if (columns)
		columns = glade_column_list_copy (columns);
	g_assert (columns);

	column = glade_column_list_find_column (columns, old_column_name);

	/* Bookkeep the exclusive names... */
	if (glade_name_context_has_name (eprop_types->context, new_column_name))
		column_name = glade_name_context_new_name (eprop_types->context, new_column_name);
	else
		column_name = g_strdup (new_column_name);
	
	glade_name_context_add_name (eprop_types->context, column_name);
	glade_name_context_release_name (eprop_types->context, old_column_name);
	
	/* Set real column name */
	g_free (column->column_name);
	column->column_name = column_name;
	
	/* The "columns" copy of this string doesnt last long... */
	column_name = g_strdup (column_name);

	glade_command_push_group (_("Setting columns on %s"), 
				  glade_widget_get_name (eprop->property->widget));


	g_value_init (&value, GLADE_TYPE_COLUMN_TYPE_LIST);
	g_value_take_boxed (&value, columns);
	glade_editor_property_commit (eprop, &value);
	g_value_unset (&value);

	property = glade_widget_get_property (eprop->property->widget, "data");
	glade_property_get (property, &data_tree);
	if (data_tree)
	{
		data_tree = glade_model_data_tree_copy (data_tree);
		glade_model_data_column_rename (data_tree, old_column_name, column_name);
		glade_command_set_property (property, data_tree);
		glade_model_data_tree_free (data_tree);
	}
	glade_command_pop_group ();

	g_free (old_column_name);
	g_free (column_name);
}

static GtkWidget *
glade_eprop_column_types_create_input (GladeEditorProperty *eprop)
{
	GladeEPropColumnTypes *eprop_types = GLADE_EPROP_COLUMN_TYPES (eprop);
	GtkWidget *vbox, *hbox, *button, *swin, *label;
	GtkCellRenderer *cell;
	GtkTreeViewColumn *col;
	gchar *string;

	vbox = gtk_vbox_new (FALSE, 2);
	
	hbox = gtk_hbox_new (FALSE, 4);


	string = g_strdup_printf ("<b>%s</b>", _("Add and remove columns:"));
	label = gtk_label_new (string);
	g_free (string);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 2, 4);
	gtk_box_pack_start (GTK_BOX (vbox), label,  FALSE, TRUE, 0);

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
	
	eprop_types->store = gtk_list_store_new (N_COLUMNS,
						 G_TYPE_STRING,
						 G_TYPE_GTYPE,
						 G_TYPE_STRING);

	g_signal_connect (eprop_types->store, "row-deleted",
			  G_CALLBACK (eprop_treeview_row_deleted),
			  eprop);
	
	eprop_types->view = (GtkTreeView *)gtk_tree_view_new_with_model (GTK_TREE_MODEL (eprop_types->store));
	eprop_types->selection = gtk_tree_view_get_selection (eprop_types->view);

	
	gtk_tree_view_set_reorderable (eprop_types->view, TRUE);
	//gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);

	/* type column */
	col = gtk_tree_view_column_new_with_attributes ("Column type", 
							gtk_cell_renderer_text_new (),
							"text", COLUMN_NAME,
							NULL);
	gtk_tree_view_append_column (eprop_types->view, col);	


	/* name column */
	cell = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (cell), "editable", TRUE, NULL);

	g_signal_connect (G_OBJECT (cell), "edited",
			  G_CALLBACK (column_name_edited), eprop);
	
	eprop_types->name_column = 
		gtk_tree_view_column_new_with_attributes ("Column name", 
							  cell,
							  "text", COLUMN_COLUMN_NAME,
							  NULL);
	gtk_tree_view_append_column (eprop_types->view, eprop_types->name_column);	
	gtk_container_add (GTK_CONTAINER (swin), GTK_WIDGET (eprop_types->view));
	
	g_object_set (G_OBJECT (vbox), "height-request", 200, NULL);

	gtk_widget_show_all (vbox);
	return vbox;
}
