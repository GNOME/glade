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

#include <string.h>
#include <stdlib.h>

#include "glade.h"
#include "glade-packing.h"
#include "glade-property-class.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-widget-class.h"
#include "glade-widget.h"

GList *table_properties = NULL;
GList *box_properties = NULL;

/* ---------------------------------- container child properties ------------------------------ */
static void
glade_packing_container_set_flag (GtkContainer *container,
				  GtkWidget *widget,
				  gboolean value,
				  const gchar *prop,
				  guint flag)
{
	GValue gvalue = { 0, };
	guint old;
	guint new;
	
	g_value_init (&gvalue, G_TYPE_UINT);
	gtk_container_child_get_property (container,
					  widget,
					  prop,
					  &gvalue);
	
	old = g_value_get_uint (&gvalue);
	/* Clear the old flag */
	new = old & (~flag);
	/* Set it */
	new |= (value ? flag : 0);
	g_value_set_uint (&gvalue, new);

	gtk_container_child_set_property (container,
					  widget,
					  prop,
					  &gvalue);
}

static void
glade_packing_container_set_integer (GtkContainer *container,
				     GtkWidget *widget,
				     gboolean value,
				     const gchar *property)
{
	GValue gvalue = { 0, };
	
	g_value_init (&gvalue, G_TYPE_UINT);
	g_value_set_uint (&gvalue, value);

	gtk_container_child_set_property (container,
					  widget,
					  property,
					  &gvalue);
}

static void
glade_packing_container_set_boolean (GtkContainer *container,
				     GtkWidget *widget,
				     gboolean value,
				     const gchar *property)
{
	GValue gvalue = { 0, };
	
	g_value_init (&gvalue, G_TYPE_BOOLEAN);
	g_value_set_boolean (&gvalue, value);

	gtk_container_child_set_property (container,
					  widget,
					  property,
					  &gvalue);
}

/* ---------------------------------------- Table ---------------------------*/
static void
glade_packing_table_set_flag (GObject *object, const GValue *value, const gchar *prop, guint flag)
{
	GladeWidget *glade_widget;
	GtkWidget *widget;
	GtkTable *table;
  
	g_return_if_fail (value != NULL);
	widget = GTK_WIDGET (object);
	g_return_if_fail (GTK_IS_WIDGET (widget));
	glade_widget = glade_widget_get_from_gtk_widget (widget);
	g_return_if_fail (GLADE_IS_WIDGET (glade_widget));
	table = GTK_TABLE (glade_widget->parent->widget);

	g_return_if_fail (GTK_IS_TABLE (table));

	glade_packing_container_set_flag (GTK_CONTAINER (table), widget,
					  g_value_get_boolean (value), prop, flag);
}

static void
glade_packing_table_set_integer (GObject *object, const GValue *value, const gchar *prop)
{
	GladeWidget *glade_widget;
	GtkWidget *widget;
	GtkTable *table;
  
	g_return_if_fail (value != NULL);
	widget = GTK_WIDGET (object);
	g_return_if_fail (GTK_IS_WIDGET (widget));
	glade_widget = glade_widget_get_from_gtk_widget (widget);
	g_return_if_fail (GLADE_IS_WIDGET (glade_widget));
	table = GTK_TABLE (glade_widget->parent->widget);

	g_return_if_fail (GTK_IS_TABLE (table));

	glade_packing_container_set_integer (GTK_CONTAINER (table), widget,
					     g_value_get_int (value), prop);
}

static void
glade_packing_table_x_expand_set (GObject *object, const GValue *value)
{
	glade_packing_table_set_flag (object, value, "x_options", GTK_EXPAND);
}

static void
glade_packing_table_y_expand_set (GObject *object, const GValue *value)
{
	glade_packing_table_set_flag (object, value, "y_options", GTK_EXPAND);
}

static void
glade_packing_table_x_shrink_set (GObject *object, const GValue *value)
{
	glade_packing_table_set_flag (object, value, "x_options", GTK_SHRINK);
}

static void
glade_packing_table_y_shrink_set (GObject *object, const GValue *value)
{
	glade_packing_table_set_flag (object, value, "y_options", GTK_SHRINK);
}

static void
glade_packing_table_x_fill_set (GObject *object, const GValue *value)
{
	glade_packing_table_set_flag (object, value, "x_options", GTK_FILL);
}			    

static void
glade_packing_table_y_fill_set (GObject *object, const GValue *value)
{
	glade_packing_table_set_flag (object, value, "y_options", GTK_FILL);
}

static void
glade_packing_table_padding_h_set (GObject *object, const GValue *value)
{
	glade_packing_table_set_integer (object, value, "x_padding");
}

static void
glade_packing_table_padding_v_set (GObject *object, const GValue *value)
{
	glade_packing_table_set_integer (object, value, "y_padding");
}				


static void
glade_packing_table_cell_x_set (GObject *object, const GValue *value)
{
	glade_implement_me ();
}				

static void
glade_packing_table_cell_y_set (GObject *object, const GValue *value)
{
	glade_implement_me ();
}				

static void
glade_packing_table_span_x_set (GObject *object, const GValue *value)
{
	glade_implement_me ();
}				

static void
glade_packing_table_span_y_set (GObject *object, const GValue *value)
{
	glade_implement_me ();
}				
/* --------------------- box ----------------------------------- */
static void
glade_packing_box_set_boolean (GObject *object, gboolean value, const gchar *property)
{
	GladeWidget *glade_widget;
	GtkWidget *widget;
	GtkBox *box;

	widget = GTK_WIDGET (object);
	g_return_if_fail (GTK_IS_WIDGET (widget));
	glade_widget = glade_widget_get_from_gtk_widget (widget);
	g_return_if_fail (GLADE_IS_WIDGET (glade_widget));
	box = GTK_BOX (glade_widget->parent->widget);

	g_return_if_fail (GTK_IS_BOX (box));

	glade_packing_container_set_boolean (GTK_CONTAINER (box),
					     widget,
					     value,
					     property);
}

static void
glade_packing_box_set_integer (GObject *object, const GValue *value,
			       const gchar *property)
{
	GladeWidget *glade_widget;
	GtkWidget *widget;
	GtkBox *box;

	widget = GTK_WIDGET (object);
	g_return_if_fail (GTK_IS_WIDGET (widget));
	glade_widget = glade_widget_get_from_gtk_widget (widget);
	g_return_if_fail (GLADE_IS_WIDGET (glade_widget));
	box = GTK_BOX (glade_widget->parent->widget);

	g_return_if_fail (GTK_IS_BOX (box));

	glade_packing_container_set_integer (GTK_CONTAINER (box),
					     widget,
					     g_value_get_int (value),
					     property);
}

/**
 * glade_packing_expand_set:
 * @object: 
 * @value: 
 * 
 * Sets the expand property of a widget inside a Gtk[HV]Box
 **/
static void
glade_packing_box_expand_set (GObject *object, const GValue *value)
{
	glade_packing_box_set_boolean (object, g_value_get_boolean (value),
				       "expand");
}

/**
 * glade_packing_fill_set:
 * @object: 
 * @value: 
 * 
 * Sets the fill property of a widget inside a Gtk[VH]Box
 **/
static void
glade_packing_box_fill_set (GObject *object, const GValue *value)
{
	glade_packing_box_set_boolean (object, g_value_get_boolean (value),
				       "fill");
}

/**
 * glade_packing_pack_start_set:
 * @object: 
 * @value: 
 * 
 * Sets the pack_start property for a widget inside a Gtk[HV]Box
 **/
static void
glade_packing_box_pack_start_set (GObject *object, const GValue *value)
{
	/* Reverse value, because pack start is an enum, not a boolean.
	 * Chema
	 */
	glade_packing_box_set_boolean (object, !g_value_get_boolean (value), "pack_type");
}

/**
 * glade_packing_padding_set:
 * @object: 
 * @value: 
 * 
 * Sets the padding for widgets inside a GtkVBox or GtkHBox
 **/
static void
glade_packing_box_padding_set (GObject *object, const GValue *value)
{
	glade_packing_box_set_integer (object, value, "padding");
}

static void
glade_packing_box_position_get (GObject *object, GValue *value)
{
	GladeWidget *glade_widget;
	GtkBoxChild *box_child = NULL;
	GtkWidget *widget;
	GtkBox *box;
	GList *list;
	gint i;

	g_value_reset (value);
	
	widget = GTK_WIDGET (object);
	g_return_if_fail (GTK_IS_WIDGET (widget));
	glade_widget = glade_widget_get_from_gtk_widget (widget);
	g_return_if_fail (GLADE_IS_WIDGET (glade_widget));
	box = GTK_BOX (glade_widget->parent->widget);
	g_return_if_fail (GTK_IS_BOX (box));

	list = box->children;
	for (; list; list = list->next) {
		box_child = list->data;
		if (box_child->widget == widget)
			break;
	}
	if (list == NULL) {
		g_warning ("Could not find the position in the GtkBox");
		return;
	}

	i = g_list_index (box->children, box_child);

	g_value_set_int (value, i);
}

static void
glade_packing_box_position_set (GObject *object, const GValue *value)
{
	GladeWidget *glade_widget;
	GladeWidget *glade_widget_child;
	GtkWidget *widget;
	GList *list;
	GtkBox *box;

	widget = GTK_WIDGET (object);
	g_return_if_fail (GTK_IS_WIDGET (widget));
	glade_widget = glade_widget_get_from_gtk_widget (widget);
	g_return_if_fail (GLADE_IS_WIDGET (glade_widget));
	box = GTK_BOX (glade_widget->parent->widget);

	gtk_box_reorder_child (box, widget, g_value_get_int (value));

	/* This all works fine, but we need to update the position property
	 * of the other children in this box since it has changed.
	 */
	list = box->children;
	for (; list; list = list->next) {
		GladeProperty *property;
		GtkBoxChild *box_child;
		GtkWidget *child;

		box_child = list->data;
		child = box_child->widget;
		glade_widget_child = glade_widget_get_from_gtk_widget (child);

		if (!glade_widget_child) {
			g_warning ("Could not get the GladeWidget to set packing position");
			continue;
		}

		property = glade_property_get_from_id (glade_widget_child->properties,
						       "position");

		/* If we have a placeholder in the Box the property will not be found */
		if (property)
			glade_packing_box_position_get (G_OBJECT (child), property->value);

		/* We should pass a FALSE argument so that this property is not added to the
		 * undo stack
		 * Also we should have a generic way to update a property, here we know is interger
		 * but it shuold be done with a generic fuction
		 */
	}
	
}

typedef struct _GladePackingProperty GladePackingProperty;
struct _GladePackingProperty
{
	const gchar *name;
	const gchar *id;
	void (*set_function) (GObject *object, const GValue *value);
	void (*get_function) (GObject *object, GValue *value);
	GladePropertyType type;
	const gchar *def;
	gboolean get_default; /* see g-property-class.h ->get_default */
};

GladePackingProperty table_props[] =
{
	{"Cell X",    "cell_x",    glade_packing_table_cell_x_set,    NULL,              GLADE_PROPERTY_TYPE_INTEGER, "0", FALSE},
	{"Cell Y",    "cell_y",    glade_packing_table_cell_y_set,    NULL,              GLADE_PROPERTY_TYPE_INTEGER, "0", FALSE},
	{"Col Span",  "col_span",  glade_packing_table_span_x_set,    NULL,              GLADE_PROPERTY_TYPE_INTEGER, "0", FALSE},
	{"Row Span",  "row_span",  glade_packing_table_span_y_set,    NULL,              GLADE_PROPERTY_TYPE_INTEGER, "0", FALSE},
	{"H Padding", "h_padding", glade_packing_table_padding_h_set, NULL,              GLADE_PROPERTY_TYPE_INTEGER, "0", FALSE},
	{"V Padding", "v_padding", glade_packing_table_padding_v_set, NULL,              GLADE_PROPERTY_TYPE_INTEGER, "0", FALSE},

	{"X Expand", "xexpand", glade_packing_table_x_expand_set, NULL,              GLADE_PROPERTY_TYPE_BOOLEAN, NULL, FALSE},
	{"Y Expand", "yexpand", glade_packing_table_y_expand_set, NULL,              GLADE_PROPERTY_TYPE_BOOLEAN, NULL, FALSE},
	{"X Shrink", "xshrink", glade_packing_table_x_shrink_set, NULL,              GLADE_PROPERTY_TYPE_BOOLEAN, NULL, FALSE},
	{"Y Shrink", "yshrink", glade_packing_table_y_shrink_set, NULL,              GLADE_PROPERTY_TYPE_BOOLEAN, NULL, FALSE},
	{"X Fill",   "xfill",   glade_packing_table_x_fill_set,   NULL,              GLADE_PROPERTY_TYPE_BOOLEAN, NULL, FALSE},
	{"Y Fill",   "yfill",   glade_packing_table_y_fill_set,   NULL,              GLADE_PROPERTY_TYPE_BOOLEAN, NULL, FALSE},
};

GladePackingProperty box_props[] =
{
	{"Position",   "position",  glade_packing_box_position_set,   glade_packing_box_position_get, GLADE_PROPERTY_TYPE_INTEGER, NULL, TRUE},
	{"Padding",    "padding",   glade_packing_box_padding_set,    NULL,                           GLADE_PROPERTY_TYPE_INTEGER, "0",  FALSE},
	{"Expand",     "expand",    glade_packing_box_expand_set,     NULL,                           GLADE_PROPERTY_TYPE_BOOLEAN, GLADE_TAG_TRUE, FALSE},
	{"Fill",       "fill",      glade_packing_box_fill_set,       NULL,                           GLADE_PROPERTY_TYPE_BOOLEAN, GLADE_TAG_TRUE, FALSE},
	{"Pack Start", "packstart", glade_packing_box_pack_start_set, NULL,                           GLADE_PROPERTY_TYPE_BOOLEAN, GLADE_TAG_TRUE, FALSE},
};

/**
 * glade_packing_add_property:
 * @list: The list of GladePropertyClass items that we are adding to
 * @prop: a structure containing the data for the GladePropertyClass we are adding
 * 
 * Addss a property class to the list
 **/
static void
glade_packing_add_property (GList **list, GladePackingProperty prop)
{
	GladePropertyClass *class;

	class = glade_property_class_new ();
	class->packing = TRUE;
	class->name    = g_strdup (prop.name);
	class->id      = g_strdup (prop.id);
	class->tooltip = g_strdup ("Implement me");
	class->type    = prop.type;
	class->set_function = prop.set_function;
	class->get_function = prop.get_function;
	if (prop.def)
		class->def = glade_property_class_make_gvalue_from_string (prop.type, prop.def);
	class->get_default = prop.get_default;

	*list = g_list_prepend (*list, class);
}


/**
 * glade_packing_init:
 * @void: 
 * 
 * Initializes the property clases for the different containers. A widget has different packing
 * properties depending on the container it is beeing added to. This function initializes the
 * lists of property classes that we are later going to add (when a widget i created).
 **/
void
glade_packing_init (void)
{
	gint num;
	gint i;

	num = sizeof (table_props) / sizeof (GladePackingProperty);
	for (i = 0; i < num; i++)
		glade_packing_add_property (&table_properties, table_props[i]);
	table_properties = g_list_reverse (table_properties);
	
	num = sizeof (box_props) / sizeof (GladePackingProperty);
	for (i = 0; i < num; i++)
		glade_packing_add_property (&box_properties, box_props[i]);
	box_properties = g_list_reverse (box_properties);
	
}

/**
 * glade_packing_add_properties_from_list:
 * @widget: The widget that we are adding the properites to
 * @list: The list of GladePropertyClass that we want to add to @widget
 * 
 * Adds a set of properites to a widget based on a list of GladePropertyClass.
 **/
/* Should this be in glade_widget ??. Chema */
static void
glade_packing_add_properties_from_list (GladeWidget *widget,
					GList *list)
{
	GladePropertyClass *class;
	GladeProperty *property;

	for (; list != NULL; list = list->next) {
		class = list->data;
		property = glade_property_new_from_class (class, widget);
		property->widget = widget;
		widget->properties = g_list_append (widget->properties, property);
	}

}

/**
 * glade_packing_add_properties:
 * @widget: The widget that we want to add the properties to
 * 
 * Adds the packing properties to a GladeWidget depending on it's container.
 * the packing properties change depending on the container a widget is using.
 * so we can only add them after we container_add it.
 **/
void
glade_packing_add_properties (GladeWidget *widget)
{
	gchar *class;

	
	if (widget->parent == NULL)
		return;
	class = widget->parent->class->name;	

	if (strcmp (class, "GtkTable") == 0)
		glade_packing_add_properties_from_list (widget, table_properties);

	if ((strcmp (class, "GtkHBox") == 0) ||
	    (strcmp (class, "GtkVBox") == 0))
		glade_packing_add_properties_from_list (widget, box_properties);

}
