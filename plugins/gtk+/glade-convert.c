/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-convert.c - Project format conversion routines
 *
 * Copyright (C) 2008 Tristan Van Berkom
 *
 * This library is free software; you can redistribute it and/or it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Author(s):
 *      Tristan Van Berkom <tvb@gnome.org>
 *
 */

#include <config.h>

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include "glade-gtk.h"
#include "glade-column-types.h"
#include "glade-model-data.h"
#include "glade-icon-sources.h"
#include "glade-tool-button-editor.h"
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
	GList *toolbuttons;
	GList *menus;
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
 *           GtkWidget:tooltip           *
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
		GladeModelData *data = glade_model_data_new (G_TYPE_STRING, "item text");

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
combo_box_convert_setup (GladeWidget *widget, GladeProjectFormat fmt)
{
	GtkListStore    *store;
	GtkComboBox     *combo = GTK_COMBO_BOX (widget->object);
	GtkCellRenderer *cell;

	if (fmt == GLADE_PROJECT_FORMAT_GTKBUILDER)
	{
		/* Get rid of any custom */
		gtk_combo_box_set_model (combo, NULL);

		/* remove every cell (its only the libglade special case cell that is here) */
		gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo));
	}
	else if (fmt == GLADE_PROJECT_FORMAT_LIBGLADE)
	{
		if (!gtk_combo_box_get_model (GTK_COMBO_BOX (combo)))
		{
			/* Add store for text items */
			store = gtk_list_store_new (1, G_TYPE_STRING);
			gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (store));
			g_object_unref (store);
		}

		/* Add cell renderer for text items */
		cell = gtk_cell_renderer_text_new ();
		gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
		gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell,
						"text", 0, NULL);
	}
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

			combo_box_convert_setup (widget, new_format);

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
	GladeWidgetAdaptor *cell_adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_CELL_RENDERER_TEXT);
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

			column->type_name   = g_strdup ("gchararray");
			column->column_name = g_strdup_printf ("item text");
			columns = g_list_append (columns, column);

			property = glade_widget_get_property (cdata->widget, "model");

			/* Cant cancel a liststore.... */
			widget = glade_command_create (adaptor, NULL, NULL, project);

			data_tree = combos_data_tree_from_items (cdata->items);

			glade_widget_property_set (widget, "columns", columns);
			glade_widget_property_set (widget, "data", data_tree);

			glade_command_set_property (property, widget->object);
			
			glade_column_list_free (columns);
			glade_model_data_tree_free (data_tree);

			/* Add a cell renderer after creating and setting the mode.... */
			widget = glade_command_create (cell_adaptor, cdata->widget, NULL, project);
			glade_widget_property_set (widget, "attr-text", 0);

		}
		else
		{
			combo_box_convert_setup (cdata->widget, new_format);

			property = glade_widget_get_property (cdata->widget, "items");
			glade_command_set_property (property, cdata->items);
		}
		g_strfreev (cdata->items);
		g_free (cdata);
	}

	g_list_free (data->combos);
}

/******************************
 *     ToolButton:icon        *
 ******************************/
static void
convert_toolbuttons_finished (GladeProject  *project,
			      ConvertData   *data)
{
	GladeProjectFormat  new_format = glade_project_get_format (project);
	GladeWidgetAdaptor *adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_ICON_FACTORY);
	GladeWidget        *icon_factory = NULL;
	GladeIconSources   *icon_sources = NULL;
	GladeProperty      *property;
	TextData           *tdata;
	GtkIconSource      *source;
	GList              *list, *source_list;
	gchar              *filename;
	GValue             *value;
	GdkPixbuf          *pixbuf;

	if (new_format == GLADE_PROJECT_FORMAT_GTKBUILDER)
	{
		/* Generate icon_sources first... */
		for (list = data->toolbuttons; list; list = list->next)
		{
			tdata = list->data;

			filename = g_strdup_printf ("generated-icon-%s", tdata->text);
			glade_util_replace (filename, '.', '-');

			/* get icon source with pixbuf from tdata->text */
			value = glade_utils_value_from_string (GDK_TYPE_PIXBUF, tdata->text,
							       project, tdata->widget);
			pixbuf = g_value_get_object (value);
			source = gtk_icon_source_new ();
			gtk_icon_source_set_pixbuf (source, pixbuf);
			g_value_unset (value);
			g_free (value);
			
			/* Add to the icon source (only one icon set list per icon) */
			if (!icon_sources)
				icon_sources = glade_icon_sources_new ();
			source_list = g_list_append (NULL, source);
			g_hash_table_insert (icon_sources->sources, g_strdup (filename), source_list);
			
			g_free (filename);
		}

		if (icon_sources)
		{
			icon_factory = glade_command_create (adaptor, NULL, NULL, project);

			property = glade_widget_get_property (icon_factory, "sources");
			glade_command_set_property (property, icon_sources);
			glade_icon_sources_free (icon_sources);
		}

		for (list = data->toolbuttons; list; list = list->next)
		{
			tdata = list->data;

			filename = g_strdup_printf ("generated-icon-%s", tdata->text);
			glade_util_replace (filename, '.', '-');

			/* Set edit mode to stock for newly generated icon */
			property = glade_widget_get_property (tdata->widget, "image-mode");
			glade_command_set_property (property, GLADE_TB_MODE_STOCK);

			/* Set stock-id for newly generated icon */
			property = glade_widget_get_property (tdata->widget, "stock-id");
			glade_command_set_property (property, filename);

			g_free (filename);
			g_free (tdata->text);
			g_free (tdata);
		}
	}
	else
	{
		/* Set "icon" property */
		for (list = data->toolbuttons; list; list = list->next)
		{
			tdata = list->data;

			/* Set edit mode to icon for converted icon */
			property = glade_widget_get_property (tdata->widget, "image-mode");
			glade_command_set_property (property, GLADE_TB_MODE_FILENAME);

			value = glade_utils_value_from_string (GDK_TYPE_PIXBUF, 
							       tdata->text, 
							       project, tdata->widget);
			pixbuf = g_value_get_object (value);
			property = glade_widget_get_property (tdata->widget, "icon");
			glade_command_set_property (property, pixbuf);
			g_value_unset (value);
			g_free (value);

			g_free (tdata->text);
			g_free (tdata);
		}
	}

	g_list_free (data->toolbuttons);
}


static gint
find_icon_factory (GObject *object, gpointer blah)
{
	if (GTK_IS_ICON_FACTORY (object))
		return 0;
	return -1;
}

static void
convert_toolbuttons (GladeProject       *project,
		     GladeProjectFormat  new_format,
		     ConvertData        *data)
{
	GladeIconSources *icon_sources = NULL;
	GladeWidget      *widget, *gfactory;
	GladeProperty    *property;
	GtkIconSource    *source;
	const GList      *objects, *element;
	TextData         *tdata;
	GdkPixbuf        *pixbuf;
	gchar            *filename = NULL, *stock_id = NULL;

	for (objects = glade_project_get_objects (project); objects; objects = objects->next)
	{
		widget = glade_widget_get_from_gobject (objects->data);
		if (!GTK_IS_TOOL_BUTTON (widget->object))
			continue;

		if (new_format == GLADE_PROJECT_FORMAT_GTKBUILDER)
		{
			pixbuf = NULL;
			property = glade_widget_get_property (widget, "icon");
			glade_property_get (property, &pixbuf);
		
			if (pixbuf)
			{
				filename = g_object_get_data (G_OBJECT (pixbuf), "GladeFileName");

				tdata = g_new0 (TextData, 1);
				tdata->widget = widget;
				tdata->text = g_strdup (filename);
				data->toolbuttons = g_list_prepend (data->toolbuttons, tdata);

				glade_command_set_property (property, NULL);
			}
		}
		else
		{
			/* If any stock's are provided in the icon factory, convert
			 * them to the "icon" property
			 */
			property = glade_widget_get_property (widget, "stock-id");
			glade_property_get (property, &stock_id);

			if (!stock_id)
				continue;
			
			if ((element = 
			     g_list_find_custom ((GList *)glade_project_get_objects (project), NULL,
						 (GCompareFunc)find_icon_factory)) != NULL)
			{
				gfactory = glade_widget_get_from_gobject (element->data);
				property = glade_widget_get_property (gfactory, "sources");
				glade_property_get (property, &icon_sources);

				if (icon_sources &&
				    (element = g_hash_table_lookup (icon_sources->sources, stock_id)))
				{
					source = element->data;
					pixbuf = gtk_icon_source_get_pixbuf (source);
					filename = g_object_get_data (G_OBJECT (pixbuf), 
								      "GladeFileName");
					if (filename)
					{

						tdata = g_new0 (TextData, 1);
						tdata->widget     = widget;
						tdata->text       = g_strdup (filename);
						data->toolbuttons = g_list_prepend (data->toolbuttons, tdata);
					}
				}
			}
		}
	}
}

/******************************************
 *   GtkImageMenuItem:image,accel-group   *
 ******************************************/
static void
convert_menus_finished (GladeProject  *project,
			ConvertData   *data)
{
	GList *l;
	GladeWidget *widget, *accel_group = NULL;
	GladeProperty *property;

	for (l = data->menus; l; l = l->next)
	{
		widget = l->data;
		property = glade_widget_get_property (widget, "accel-group");

		if (accel_group == NULL)
		{
			GladeWidget *toplevel = glade_widget_get_toplevel (widget);

			accel_group = glade_command_create (glade_widget_adaptor_get_by_type (GTK_TYPE_ACCEL_GROUP),
							    NULL, NULL, project);


			if (GTK_IS_WINDOW (toplevel->object))
			{
				GladeProperty *groups_prop = glade_widget_get_property (toplevel, "accel-groups");
				GList *list = g_list_append (NULL, accel_group->object);
				glade_command_set_property (groups_prop, list);
				g_list_free (list);
			}
		}

		glade_command_set_property (property, accel_group->object);

	}


	g_list_free (data->menus);
}

static GladeWidget *
get_image_widget (GladeWidget *widget)
{
	GtkWidget *image;
	image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (widget->object));
	return image ? glade_widget_get_from_gobject (image) : NULL;
}

static void
convert_menus (GladeProject       *project,
	       GladeProjectFormat  new_format,
	       ConvertData        *data)
{
	GladeWidget   *widget;
	GladeProperty *property;
	const GList   *objects;
	GladeWidget   *gimage;
	gboolean       use_stock;

	for (objects = glade_project_get_objects (project); objects; objects = objects->next)
	{
		widget = glade_widget_get_from_gobject (objects->data);
		if (!GTK_IS_IMAGE_MENU_ITEM (widget->object))
			continue;
		
		glade_widget_property_get (widget, "use-stock", &use_stock);

		/* convert images */
		if ((gimage = get_image_widget (widget)) != NULL)
		{
			GList list = { 0, };

			list.data = gimage;

			glade_command_unlock_widget (gimage);
			glade_command_cut (&list);

			if (new_format == GLADE_PROJECT_FORMAT_GTKBUILDER)
			{
				property = glade_widget_get_property (widget, "image");
				glade_command_paste (&list, NULL, NULL);
				glade_command_set_property (property, gimage->object);
			}
			else
				glade_command_paste (&list, widget, NULL);

			glade_command_lock_widget (widget, gimage);
		}

		if (new_format == GLADE_PROJECT_FORMAT_GTKBUILDER && use_stock)
			data->menus = g_list_prepend (data->menus, widget);
	}

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
	convert_toolbuttons_finished (project, data);
	convert_menus_finished (project, data);

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
	convert_toolbuttons (project, new_format, data);
	convert_menus (project, new_format, data);

	/* Clean up after the new_format is in effect */
	g_signal_connect (G_OBJECT (project), "convert-finished",
			  G_CALLBACK (glade_gtk_project_convert_finished), data);

	return TRUE;
}
