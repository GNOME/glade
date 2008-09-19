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
#include <gladeui/glade.h>


typedef struct {
	GObject *adjustment;
	GladeProperty *property;
} AdjustmentData;

typedef struct {
	/* List of newly created objects to set */
	GList *adjustments;

} ConvertData;

/*****************************************
 *           Adjustments                 *
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
			widget = glade_command_create (adaptor, NULL, NULL, glade_app_get_project ());

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
 *           Main entry point            *
 *****************************************/
static void
glade_gtk_project_convert_finished (GladeProject *project,
				    ConvertData  *data)
{
	convert_adjustments_finished (project, data);

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

	/* Convert GtkAdjustments ... */
	convert_adjustments (project, new_format, data);

	/* Clean up after the new_format is in effect */
	g_signal_connect (G_OBJECT (project), "convert-finished",
			  G_CALLBACK (glade_gtk_project_convert_finished), data);

	return TRUE;
}
