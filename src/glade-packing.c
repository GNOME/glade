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
#include "glade-placeholder.h"

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
				     gint value,
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

/* #warning This is broken, a placeholder should be a subclass of a GladeWidget */
	if (glade_placeholder_is (widget)) {
		table = GTK_TABLE (glade_placeholder_get_parent (widget)->widget);
	} else {
		glade_widget = glade_widget_get_from_gtk_widget (widget);
		g_return_if_fail (GLADE_IS_WIDGET (glade_widget));
		table = GTK_TABLE (glade_widget->parent->widget);
	} 
		
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

static GtkTableChild *
glade_packing_table_get_child (GObject *object)
{
	GtkTableChild *table_child = NULL;
	GladeWidget *glade_widget;
	GtkWidget *widget = GTK_WIDGET (object);
	GtkTable *table;
	GList *list;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), NULL);
	glade_widget = glade_widget_get_from_gtk_widget (widget);
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	table = GTK_TABLE (glade_widget->parent->widget);
	g_return_val_if_fail (GTK_IS_TABLE (table), NULL);

	list = table->children;
	for (; list; list = list->next) {
		table_child = list->data;
		if (table_child->widget == widget)
			break;
	}
	if (list == NULL) {
		g_warning ("Could not find widget %s in table\n",
			   glade_widget_get_name (glade_widget));
		return NULL;
	}

	return table_child;
}				

GtkTableChild *
glade_packing_table_get_child_at (GtkTable *table, gint col, gint row)
{
	GtkTableChild *child;
	GList *list;
	
	g_return_val_if_fail (GTK_IS_TABLE (table), NULL);
	g_return_val_if_fail (col > -1, NULL);
	g_return_val_if_fail (row > -1, NULL);
	g_return_val_if_fail (row <= table->nrows, NULL);
	g_return_val_if_fail (col <= table->ncols, NULL);

	list = table->children;
	for (; list; list = list->next) {
		child = list->data;
		/* Check for spans too */
		if ((child->left_attach == col) &&
		    (child->top_attach  == row))
			return child;
	}

	/* We use this function now to know if a col,row is free or not, so
	 * we should not show a warning */
/*	g_warning ("Could not get widget inside GtkTable at pos %d,%d\n",
		   row, col);*/

	return NULL;
}

static gboolean
glade_packing_table_child_has_span (GtkTableChild *child)
{
	if (((child->left_attach + 1) == child->right_attach) &&
	    ((child->top_attach  + 1) == child->bottom_attach)) {
		return FALSE;
	}

	return FALSE;
}

/* We have several cases
 * _________________________________________
 * |         |        | /  /  /  |  /  /  /|
 * |   A     |   B    |/  /  /  /| /  /  / |
 * |_________|________|__/__/__/_|/__/__/__|
 * |         .        |          | /  /  / |
 * |         C        |    D     |/  /  /  |
 * |_________.________|__________|__/__/__/|
 * |         .        |          .         |
 * |         E        |          F         |
 * |_________.________|__________._________|
 * |         |        .          | /  /  / |
 * |    G    |        H          |/  /  /  |
 * |_________|________.__________|__/__/__/|
 * |                  |          .         |
 * |        I         |                    |
 * |__________________|. . . . . J . . . . |
 * |  /  /  /  /  /  /|          .         |
 * | /  /  /  /  /  / |          .         |
 * |/__/__/__/__/__/__|__________._________|
 * |                  |          .         |
 * |        K         |                    |
 * |__________________|. . . . . N . . . . |
 * |        |         |          .         |
 * |   L    |   M     |          .         |
 * |________|_________|__________._________|
 *
 * // = placeholders
 *
 * 1: A increments by one (or B++ or D++)
 *    We want to switch A with B
 * 2: C increments by one
 *    we want to end up with "D C C /"
 * 3: E increments by one
 *    since size is the same we want to switch E & F
 * 4: G incrementents by one
 *    We want to change the span of H to one less than before
 *    and move H to slot one, move G to slot two, slot 3 & 4 have /'s
 * 5: I increments by one
 *    I am going to go crazy !;-) add a warning to the user about
 *    glade3 beeing too stupid at this point. Implement smarter code later.
 */
static void
glade_packing_table_cell_common_set (GObject *object, const GValue *value, gboolean is_x)
{
	GtkTableChild *table_child;
	GtkTableChild *old_table_child;
	GladeWidget *glade_widget;
	GtkWidget *widget;
	GtkTable *table;
	GValue gvalue = { 0, };
	gint table_rows;
	gint table_cols;
	gint current_col;
	gint current_row;
	gint span_col;
	gint span_row;

	gint table_size;
	gint span;
	gint current_pos;
	gint new_pos;

	gint start;
	gint end;
	gint i;

	g_return_if_fail (value != NULL);
	widget = GTK_WIDGET (object);
	g_return_if_fail (GTK_IS_WIDGET (widget));
	glade_widget = glade_widget_get_from_gtk_widget (widget);
	g_return_if_fail (GLADE_IS_WIDGET (glade_widget));
	table = GTK_TABLE (glade_widget->parent->widget);
	g_return_if_fail (GTK_IS_TABLE (table));
	table_child = glade_packing_table_get_child (object);
	g_return_if_fail (table_child != NULL);

	if (glade_packing_table_child_has_span (table_child)) {
		g_warning ("Can't move object that have a span set\n");
		glade_implement_me ();
		return;
	}

	table_rows = table->nrows;
	table_cols = table->ncols;
	current_col = table_child->left_attach;
	current_row = table_child->top_attach;
	span_col = table_child->right_attach  - table_child->left_attach;
	span_row = table_child->bottom_attach - table_child->top_attach;
	new_pos = g_value_get_int (value);

	span        = is_x ? span_col    : span_row;
	table_size  = is_x ? table_cols  : table_rows;
	current_pos = is_x ? current_col : current_row;
	
	if (new_pos == current_pos)
		return;

	if (abs (new_pos - current_pos) != 1) {
		g_print ("I can only handle increments or decrements of 1 for now\n");
		return;
	}
		
	/* Do we need to add a column ? */
	if (new_pos + span > table_size) {
		GladeProperty *property;
		property = glade_property_get_from_id (glade_widget->parent->properties,
						       is_x ? "n-columns" : "n-rows");
		g_return_if_fail (GLADE_IS_PROPERTY (property));
		glade_property_set_integer (property, new_pos + span);
	}

	/* Init gvalue */
	g_value_init (&gvalue, G_TYPE_INT);

	/* Now change the position of the placholders */
	start = is_x ? current_row : current_col;
	end   = is_x ? start + span_row : start + span_col;

	for (i = start; i < end; i ++) {
		/* Get the GtkWidget that is at the new position
		 * verify that span is 1 and switch them (we can be
		 * a lot more smarter if we write more code)
		 */
		if (new_pos < current_pos) {
			old_table_child = glade_packing_table_get_child_at (table,
									    is_x ? new_pos : i,
									    is_x ? i : new_pos);
		} else {
			old_table_child = glade_packing_table_get_child_at (table,
									    is_x ? new_pos + span - 1 : i,
									    is_x ? i : new_pos + span - 1);
		}

		g_return_if_fail (old_table_child != NULL);
		if (!glade_packing_table_child_has_span (old_table_child)) {
 			GtkWidget *old;
			gint x;

			old = old_table_child->widget;
			
			x = (new_pos < current_pos) ? current_pos + span - 1 : new_pos - 1;
			g_value_set_int (&gvalue, x);
			glade_packing_table_set_integer (G_OBJECT (old), &gvalue,
							 is_x ? "left_attach" : "top_attach");
			/* We know is a placeholder so span is 1 */
			g_value_set_int (&gvalue, x + 1);
			glade_packing_table_set_integer (G_OBJECT (old), &gvalue,
							 is_x ? "right_attach": "bottom_attach");
		} else {
			g_print ("Can't handle cell x movements when a widget has a span\n");
		}
	}

	/* Set the position of the widget after the placeholders have been moved
	 * we need to do this after moving the placholders because if we first move
	 * the widget and then the placholders the get_child_at will have 2 widgets
	 * that can return because they will be sharing.
	 */
	g_value_set_int (&gvalue, new_pos + span);
	glade_packing_table_set_integer (G_OBJECT (object), &gvalue,
					 is_x ? "right_attach" : "bottom_attach");
	g_value_set_int (&gvalue, new_pos);
	glade_packing_table_set_integer (G_OBJECT (object), &gvalue,
					 is_x ? "left_attach" : "top_attach");
}
	
static void
glade_packing_table_cell_x_set (GObject *object, const GValue *value)
{
	glade_packing_table_cell_common_set (object, value, TRUE);
}

static void
glade_packing_table_cell_y_set (GObject *object, const GValue *value)
{
	glade_packing_table_cell_common_set (object, value, FALSE);
}				

static void
glade_packing_table_cell_x_get (GObject *object, GValue *value)
{
	GtkTableChild *table_child;

	g_value_reset (value);
	table_child = glade_packing_table_get_child (object);
	g_return_if_fail (table_child != NULL);
	g_value_set_int (value, table_child->left_attach);
}

static void
glade_packing_table_cell_y_get (GObject *object, GValue *value)
{
	GtkTableChild *table_child;

	g_value_reset (value);
	table_child = glade_packing_table_get_child (object);
	g_return_if_fail (table_child != NULL);
	g_value_set_int (value, table_child->top_attach);
}				

static void
glade_packing_table_span_x_get (GObject *object, GValue *value)
{
	GtkTableChild *table_child;
	
	g_value_reset (value);
	table_child = glade_packing_table_get_child (object);
	g_return_if_fail (table_child != NULL);
	g_value_set_int (value, table_child->right_attach - table_child->left_attach);
}				

static void
glade_packing_table_span_y_get (GObject *object, GValue *value)
{
	GtkTableChild *table_child;

	g_value_reset (value);
	table_child = glade_packing_table_get_child (object);
	g_return_if_fail (table_child != NULL);
	g_value_set_int (value, table_child->bottom_attach - table_child->top_attach);
}


static void
glade_packing_table_span_common_set (GObject *object, const GValue *value, gboolean for_x)
{
	GtkTableChild *table_child;
	GtkTableChild *old_table_child;
	GladeWidget *glade_widget;
	GtkWidget *widget;
	GtkTable *table;
	GValue gvalue = {0,};
	gint table_rows;
	gint table_cols;
	gint current_col;
	gint current_row;
	gint current_pos;
	gint span_col;
	gint span_row;
	gint new_span;
	gint old_span;
	gint table_size;
	
	g_return_if_fail (value != NULL);
	widget = GTK_WIDGET (object);
	g_return_if_fail (GTK_IS_WIDGET (widget));
	glade_widget = glade_widget_get_from_gtk_widget (widget);
	g_return_if_fail (GLADE_IS_WIDGET (glade_widget));
	table = GTK_TABLE (glade_widget->parent->widget);
	g_return_if_fail (GTK_IS_TABLE (table));
	table_child = glade_packing_table_get_child (object);
	g_return_if_fail (table_child != NULL);

	table_rows = table->nrows;
	table_cols = table->ncols;
	current_col = table_child->left_attach;
	current_row = table_child->top_attach;
	span_row = table_child->bottom_attach - table_child->top_attach;
	span_col = table_child->right_attach  - table_child->left_attach;

	table_size  = for_x ? table_cols  : table_rows;
	current_pos = for_x ? current_col : current_row;
	old_span    = for_x ? span_col    : span_row;
	
	new_span = g_value_get_int (value);


#if 0
	g_print ("Changing the span x. Old span value is %d, new span value should be %d\n",
		 span_col, new_span_col);
#endif	

	if (old_span == new_span)
		return;
	if (new_span < 1)
		return;

	/* Do we need to make the table bigger for the new span to "fit" ? */
	if (current_pos + new_span > table_size) {
		GladeProperty *property;
		property = glade_property_get_from_id (glade_widget->parent->properties,
						       for_x ? "n-columns" : "n-rows");
		g_return_if_fail (GLADE_IS_PROPERTY (property));
		glade_property_set_integer (property, current_pos + new_span);
	}

	/* Remove the containers that we are going to use their slot */
	if (new_span > old_span) {
		gint start;
		gint end;
		gint i;

		start = for_x ? current_row : current_col;
		end   = for_x ? current_row + span_row : current_col + span_col;

		for (i = start; i < end; i++) {
			if (for_x)
				old_table_child = glade_packing_table_get_child_at (table, current_pos + new_span - 1, i);
			else
				old_table_child = glade_packing_table_get_child_at (table, i, current_pos + new_span - 1);
			g_return_if_fail (old_table_child != NULL);
			if (!glade_placeholder_is (old_table_child->widget)) {
				g_print ("I can't change the span of a widget if i don't have a placeholder to remove");
				return;
			}
			gtk_container_remove (GTK_CONTAINER (table), old_table_child->widget);
		}
	}


	/* Change the span of the widget */
	g_value_init (&gvalue, G_TYPE_INT);
	g_value_set_int (&gvalue, current_pos + new_span);
	if (for_x)
		glade_packing_table_set_integer (G_OBJECT (object), &gvalue, "right_attach");
	else
		glade_packing_table_set_integer (G_OBJECT (object), &gvalue, "bottom_attach");

	/* Remove the placholders that are sharing a slot with the enlarged widget */
	if (new_span < old_span) {
		gint i;
		gint j;
		gint start;
		gint end;

		start = for_x ? current_row : current_col;
		end   = for_x ? current_row + span_row : current_col + span_col;

		for (i = new_span; i < old_span; i++) {
			for (j = start; j < end; j++ ) {
				GladePlaceholder *placeholder;
				placeholder = glade_placeholder_new (glade_widget->parent);
#if 0
				g_printe ("Add a placeholder at %d,%d,%d,%d\n",
					  i, i + 1,
					  current_col, current_col + 1);
#endif
				if (for_x)
					gtk_table_attach_defaults (table,
								   GTK_WIDGET (placeholder),
								   current_col + i, current_col + i + 1,
								   j, j + 1);
				else
					gtk_table_attach_defaults (table,
								   GTK_WIDGET (placeholder),
								   j, j + 1,
								   current_row + i, current_row + i + 1);
			}
		}

	}

}				


static void
glade_packing_table_span_x_set (GObject *object, const GValue *value)
{
	glade_packing_table_span_common_set (object, value, TRUE);
}

static void
glade_packing_table_span_y_set (GObject *object, const GValue *value)
{
	glade_packing_table_span_common_set (object, value, FALSE);
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
glade_packing_table_padding_h_get (GObject *object, GValue *value)
{
	GtkTableChild *table_child;

	g_value_reset (value);
	table_child = glade_packing_table_get_child (object);
	if (!table_child)
		return;
	g_value_set_int (value, table_child->xpadding);
}

static void
glade_packing_table_padding_v_get (GObject *object, GValue *value)
{
	GtkTableChild *table_child;

	g_value_reset (value);
	table_child = glade_packing_table_get_child (object);
	if (!table_child)
		return;
	g_value_set_int (value, table_child->ypadding);
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
	 * Chema.
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

		/* This is a placeholder */
		if (!glade_widget_child)
			continue;

		property = glade_property_get_from_id (glade_widget_child->properties,
						       "position");

		glade_packing_box_position_get (G_OBJECT (child), property->value);

		/* We should pass a FALSE argument so that this property is not added to the
		 * undo stack
		 * Also we should have a generic way to update a property, here we know is interger
		 * but it should be done with a generic fuction
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
	{"Cell X",    "cell_x",    glade_packing_table_cell_x_set,    glade_packing_table_cell_x_get,    GLADE_PROPERTY_TYPE_INTEGER, "0", TRUE},
	{"Cell Y",    "cell_y",    glade_packing_table_cell_y_set,    glade_packing_table_cell_y_get,    GLADE_PROPERTY_TYPE_INTEGER, "0", TRUE},
	{"Col Span",  "col_span",  glade_packing_table_span_x_set,    glade_packing_table_span_x_get,    GLADE_PROPERTY_TYPE_INTEGER, "0", TRUE},
	{"Row Span",  "row_span",  glade_packing_table_span_y_set,    glade_packing_table_span_y_get,    GLADE_PROPERTY_TYPE_INTEGER, "0", TRUE},
	{"H Padding", "h_padding", glade_packing_table_padding_h_set, glade_packing_table_padding_h_get, GLADE_PROPERTY_TYPE_INTEGER, "0", TRUE},
	{"V Padding", "v_padding", glade_packing_table_padding_v_set, glade_packing_table_padding_v_get, GLADE_PROPERTY_TYPE_INTEGER, "0", TRUE},

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
		class->def = glade_property_class_make_gvalue_from_string (class, prop.def);
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
	GladePackingProperties *packing_properties;
	GladePropertyClass *class;
	GladeProperty *property;

	packing_properties = glade_packing_property_get_from_class (widget->class,
								  widget->parent->class);
	for (; list != NULL; list = list->next) {
		class = list->data;
		property = glade_property_new_from_class (class, widget);
		property->widget = widget;
		widget->properties = g_list_append (widget->properties, property);

		if (packing_properties) {
			GValue *gvalue;
			const gchar *value;
			value = g_hash_table_lookup (packing_properties->properties,
						     property->class->id);
			if (value) {
				gvalue = glade_property_class_make_gvalue_from_string (property->class, value);
				g_free (property->value);
				property->value = gvalue;
			}
		}
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


GladePackingProperties *
glade_packing_property_get_from_class (GladeWidgetClass *class,
				       GladeWidgetClass *container_class)
{
	GladePackingProperties *property = NULL;
	GList *list;

	list = class->packing_properties;
	for (; list; list = list->next) {
		property = list->data;
		if (property->container_class == container_class)
			break;
	}
	if (list == NULL)
		return NULL;

	return property;
}
