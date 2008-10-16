/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-convert.c - Project format conversion routines
 *
 * Copyright (C) 2008 Tristan Van Berkom
 *
 * Author(s):
 *      Tristan Van Berkom <tvb@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#include <config.h>

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "glade-gtk.h"
#include "glade-column-types.h"
#include "glade-model-data.h"
#include <gladeui/glade.h>


typedef struct {
	GObject *adjustment;
	GladeProperty *property;
} AdjustmentData;

typedef struct {
	GladeWidget *widget;
	gchar *text;
} TextData;

typedef struct {
	GladeWidget *widget;
	gchar **items;
} ComboData;

typedef struct {
	/* List of newly created objects to set */
	GList *adjustments;
	GList *textviews;
	GList *tooltips;
	GList *combos;
} ConvertData;

/*****************************************
 *           GtkAdjustments              *
 *****************************************/
static void
convert_adjustments_finished (GladeProject  *project,
			      ConvertData   *data)
{
	GladeProjectFormat  new_format = glade_project_get_format (project);
	GladeWidgetAdaptor *adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_ADJUSTMENT);
	GladeWidget        *widget;
	AdjustmentData     *adata; 
	GList              *list;

	for (list = data->adjustments; list; list = list->next)
	{
		adata = list->data;

		if (new_format == GLADE_PROJECT_FORMAT_GTKBUILDER)
		{
			gdouble value, lower, upper, step_inc, page_inc, page_size;

			g_object_get (adata->adjustment, 
				      "value", &value,
				      "lower", &lower,
				      "upper", &upper,
				      "step-increment", &step_inc,
				      "page-increment", &page_inc,
				      "page-size", &page_size,
				      NULL);

			/* Cant cancel an adjustment.... */
			widget = glade_command_create (adaptor, NULL, NULL, project);

			/* Initial properties on the new adjustment dont need command history */
			glade_widget_property_set (widget, "value", value);
			glade_widget_property_set (widget, "lower", lower);
			glade_widget_property_set (widget, "upper", upper);
			glade_widget_property_set (widget, "step-increment", step_inc);
			glade_widget_property_set (widget, "page-increment", page_inc);
			glade_widget_property_set (widget, "page-size", page_size);

			/* hook up the new adjustment */
			glade_command_set_property (adata->property, widget->object);

			/* destroy the fabricated object */
			gtk_object_destroy (GTK_OBJECT (adata->adjustment));
		} 
		else
		{
			/* Set the adjustment we created directly */
			glade_command_set_property (adata->property, adata->adjustment);
		}

		g_free (adata);
	}

	g_list_free (data->adjustments);
}

static void
convert_adjustment_properties (GList              *properties,
			       GladeProjectFormat  new_format,
			       ConvertData        *data)
{
	GladeWidget   *adj_widget;
	GladeProperty *property;
	GObject       *adjustment;
	GList *list;
	GList *delete = NULL;


	for (list = properties; list; list = list->next)
	{
		property = list->data;
		
		if (property->klass->pspec->value_type == GTK_TYPE_ADJUSTMENT)
		{
			adjustment = NULL;
			glade_property_get (property, &adjustment);
			
			if (adjustment)
			{
				/* Record an adjustment here to restore after the format switch */
				gdouble value, lower, upper, step_inc, page_inc, page_size;
				AdjustmentData *adata = g_new0 (AdjustmentData, 1);
				
				g_object_get (adjustment, 
					      "value", &value,
					      "lower", &lower,
					      "upper", &upper,
					      "step-increment", &step_inc,
					      "page-increment", &page_inc,
					      "page-size", &page_size,
					      NULL);
				
				adata->property = property;
				adata->adjustment = (GObject *)gtk_adjustment_new (value, lower, upper, 
										   step_inc, page_inc, page_size);
				data->adjustments = g_list_prepend (data->adjustments, adata);
				
				/* Delete trailing builder objects from project */
				if (new_format == GLADE_PROJECT_FORMAT_LIBGLADE &&
				    (adj_widget = glade_widget_get_from_gobject (adjustment)))
				{
					if (!g_list_find (delete, adj_widget))
						delete = g_list_prepend (delete, adj_widget);
				}
			}
		}
	}
	
	if (delete)
	{
		glade_command_delete (delete);
		g_list_free (delete);
	}
}

static void
convert_adjustments (GladeProject       *project,
		     GladeProjectFormat  new_format,
		     ConvertData        *data)
{
	GladeWidget   *widget;
	const GList   *objects;

	for (objects = glade_project_get_objects (project); objects; objects = objects->next)
	{
		widget = glade_widget_get_from_gobject (objects->data);

		convert_adjustment_properties (widget->properties, new_format, data);
		convert_adjustment_properties (widget->packing_properties, new_format, data);
	}
}

/*****************************************
 *           TextView:text               *
 *****************************************/
static void
convert_textviews_finished (GladeProject  *project,
			    ConvertData   *data)
{
	GladeProjectFormat  new_format = glade_project_get_format (project);
	GladeWidgetAdaptor *adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_TEXT_BUFFER);
	GladeProperty *property;
	GladeWidget *widget;
	TextData  *tdata;
	GList *list;

	for (list = data->textviews; list; list = list->next)
	{
		tdata = list->data;

		if (new_format == GLADE_PROJECT_FORMAT_GTKBUILDER)
		{
			property = glade_widget_get_property (tdata->widget, "buffer");

			/* Cant cancel a textbuffer.... */
			widget = glade_command_create (adaptor, NULL, NULL, project);

			glade_command_set_property (property, widget->object);

			property = glade_widget_get_property (widget, "text");
			glade_property_set (property, tdata->text);
		}
		else
		{
			property = glade_widget_get_property (tdata->widget, "text");
			glade_command_set_property (property, tdata->text);
		}
		g_free (tdata->text);
		g_free (tdata);
	}

	g_list_free (data->textviews);
}

static void
convert_textviews (GladeProject       *project,
		   GladeProjectFormat  new_format,
		   ConvertData        *data)
{
	GladeWidget   *widget, *gbuffer;
	GladeProperty *property;
	TextData  *tdata;
	GtkTextBuffer *buffer;
	const GList   *objects;
	gchar         *text;

	for (objects = glade_project_get_objects (project); objects; objects = objects->next)
	{
		widget = glade_widget_get_from_gobject (objects->data);
		if (!GTK_IS_TEXT_VIEW (widget->object))
			continue;

		if (new_format == GLADE_PROJECT_FORMAT_GTKBUILDER)
		{
			text = NULL;
			property = glade_widget_get_property (widget, "text");
			glade_property_get (property, &text);
		
			if (text)
			{
				tdata = g_new0 (TextData, 1);
				tdata->widget = widget;
				tdata->text = g_strdup (text);
				data->textviews = g_list_prepend (data->textviews, tdata);

				glade_command_set_property (property, NULL);
			}
		}
		else
		{
			text = NULL;
			gbuffer = NULL;
			buffer = NULL;
			property = glade_widget_get_property (widget, "buffer");
			glade_property_get (property, &buffer);

			if (buffer && (gbuffer = glade_widget_get_from_gobject (buffer)))
				glade_widget_property_get (gbuffer, "text", &text);

			if (text)
			{
				GList delete = { 0, };
				delete.data = gbuffer;
				
				tdata = g_new0 (TextData, 1);
				tdata->widget = widget;
				tdata->text = g_strdup (text);
				data->textviews = g_list_prepend (data->textviews, tdata);

				/* This will take care of unsetting the buffer property as well */
				glade_command_delete (&delete);
			}
		}
	}
}

/*****************************************
 *           GtkWidget:tooltips          *
 *****************************************/
static void
convert_tooltips_finished (GladeProject  *project,
			   ConvertData   *data)
{
	GladeProjectFormat  new_format = glade_project_get_format (project);
	GladeProperty *property;
	GList *list;
	TextData *tdata;

	for (list = data->tooltips; list; list = list->next)
	{
		tdata = list->data;
		
		if (new_format == GLADE_PROJECT_FORMAT_GTKBUILDER)
			property = glade_widget_get_property (tdata->widget, "tooltip-text");
		else	
			property = glade_widget_get_property (tdata->widget, "tooltip");

		glade_command_set_property (property, tdata->text);

		g_free (tdata->text);
		g_free (tdata);
	}
	
	g_list_free (data->tooltips);
}

static void
convert_tooltips (GladeProject       *project,
		  GladeProjectFormat  new_format,
		  ConvertData        *data)
{
	GladeWidget   *widget;
	GladeProperty *property;
	TextData      *tdata;
	const GList   *objects;
	gchar         *text;

	for (objects = glade_project_get_objects (project); objects; objects = objects->next)
	{
		widget = glade_widget_get_from_gobject (objects->data);
		if (!GTK_IS_WIDGET (widget->object))
			continue;

		if (new_format == GLADE_PROJECT_FORMAT_GTKBUILDER)
			property = glade_widget_get_property (widget, "tooltip");
		else	
			property = glade_widget_get_property (widget, "tooltip-text");


		text = NULL;
		glade_property_get (property, &text);
		if (text)
		{
			tdata = g_new0 (TextData, 1);
			tdata->widget = widget;
			tdata->text = g_strdup (text);
			data->tooltips = g_list_prepend (data->tooltips, tdata);

			glade_command_set_property (property, NULL);
		}
	}
}

/*****************************************
 *           Combo:items                 *
 *****************************************/
static GNode *
combos_data_tree_from_items (gchar **items)
{
	GNode *row, *data_tree;
	gint i;

	if (!items)
		return NULL;

	data_tree = g_node_new (NULL);

	for (i = 0; items[i]; i++)
	{
		GladeModelData *data = glade_model_data_new (G_TYPE_STRING, "item");

		g_value_set_string (&data->value, items[i]);

		row = g_node_new (NULL);
		g_node_append (data_tree, row);
		g_node_append_data (row, data);
	}
	return data_tree;
}

static gchar **
combos_items_from_data_tree (GNode *data_tree)
{
	GNode          *row, *item;
	GPtrArray      *array = g_ptr_array_new ();
	GladeModelData *data;
	gchar          *string;
	
	for (row = data_tree->children; row; row = row->next)
	{
		for (item = row->children; item; item = item->next)
		{
			data = item->data;
			if (G_VALUE_TYPE (&data->value) == G_TYPE_STRING)
			{
				string = g_value_dup_string (&data->value);
				g_ptr_array_add (array, string);
				break;
			}
		}
	}

	if (array->len == 0)
		return NULL;

	g_ptr_array_add (array, NULL);

	return (gchar **)g_ptr_array_free (array, FALSE);
}

static void
convert_combos (GladeProject       *project,
		GladeProjectFormat  new_format,
		ConvertData        *data)
{
	GladeWidget   *widget, *gmodel;
	GladeProperty *property;
	ComboData     *cdata;
	GObject       *model;
	const GList   *objects;
	GNode         *data_tree;
	gchar        **items;

	for (objects = glade_project_get_objects (project); objects; objects = objects->next)
	{
		widget = glade_widget_get_from_gobject (objects->data);
		if (!GTK_IS_COMBO_BOX (widget->object))
			continue;

		if (new_format == GLADE_PROJECT_FORMAT_GTKBUILDER)
		{
			items = NULL;
			property = glade_widget_get_property (widget, "items");
			glade_property_get (property, &items);
		
			if (items)
			{
				cdata = g_new0 (ComboData, 1);
				cdata->widget = widget;
				cdata->items = g_strdupv (items);
				data->combos = g_list_prepend (data->combos, cdata);

				glade_command_set_property (property, NULL);
			}
		}
		else
		{
			items = NULL;
			data_tree = NULL;
			gmodel = NULL;
			model = NULL;
			property = glade_widget_get_property (widget, "model");
			glade_property_get (property, &model);

			if (model && (gmodel = glade_widget_get_from_gobject (model)))
				glade_widget_property_get (gmodel, "data", &data_tree);

			if (data_tree)
				items = combos_items_from_data_tree (data_tree);

			if (items)
			{
				GList delete = { 0, };
				delete.data = gmodel;
				
				cdata = g_new0 (ComboData, 1);
				cdata->widget = widget;
				cdata->items = items;
				data->combos = g_list_prepend (data->combos, cdata);

				/* This will take care of unsetting the buffer property as well */
				glade_command_delete (&delete);
			}
		}
	}
}

static void
convert_combos_finished (GladeProject  *project,
			 ConvertData   *data)
{
	GladeProjectFormat  new_format = glade_project_get_format (project);
	GladeWidgetAdaptor *adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_LIST_STORE);
	GladeProperty *property;
	GladeWidget *widget;
	ComboData  *cdata;
	GNode *data_tree;
	GList *list;

	for (list = data->combos; list; list = list->next)
	{
		cdata = list->data;

		if (new_format == GLADE_PROJECT_FORMAT_GTKBUILDER)
		{
			GList *columns = NULL;
			GladeColumnType *column = g_new0 (GladeColumnType, 1);
			column->type = G_TYPE_STRING;
			columns = g_list_append (columns, column);

			property = glade_widget_get_property (cdata->widget, "model");

			/* Cant cancel a liststore.... */
			widget = glade_command_create (adaptor, NULL, NULL, project);

			data_tree = combos_data_tree_from_items (cdata->items);

			glade_widget_property_set (widget, "columns", columns);
			glade_widget_property_set (widget, "data", data_tree);

			glade_command_set_property (property, widget->object);
			
			glade_column_list_free (columns);
		}
		else
		{
			property = glade_widget_get_property (cdata->widget, "items");
			glade_command_set_property (property, cdata->items);
		}
		g_strfreev (cdata->items);
		g_free (cdata);
	}

	g_list_free (data->combos);
}


/*****************************************
 *           Main entry point            *
 *****************************************/
static void
glade_gtk_project_convert_finished (GladeProject *project,
				    ConvertData  *data)
{
	convert_adjustments_finished (project, data);
	convert_textviews_finished (project, data);
	convert_tooltips_finished (project, data);
	convert_combos_finished (project, data);

	/* Once per conversion */
	g_signal_handlers_disconnect_by_func (G_OBJECT (project),
					      G_CALLBACK (glade_gtk_project_convert_finished), data);

	g_free (data);
}

gboolean
glade_gtk_project_convert (GladeProject *project,
			   GladeProjectFormat new_format)
{
	ConvertData  *data = g_new0 (ConvertData, 1);

	convert_adjustments (project, new_format, data);
	convert_textviews (project, new_format, data);
	convert_tooltips (project, new_format, data);
	convert_combos (project, new_format, data);

	/* Clean up after the new_format is in effect */
	g_signal_connect (G_OBJECT (project), "convert-finished",
			  G_CALLBACK (glade_gtk_project_convert_finished), data);

	return TRUE;
}
