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

#include "glade.h"
#include "glade-packing.h"
#include "glade-property-class.h"
#include "glade-property.h"
#include "glade-widget-class.h"
#include "glade-widget.h"

GList *table_properties = NULL;
GList *box_properties = NULL;

static void
glade_packing_x_expand_set (GObject *object, const gchar *value)
{
	g_print ("X expand set %s\n", value);
}
			    
static void
glade_packing_y_expand_set (GObject *object, const gchar *value)
{
	g_print ("Y expand set %s\n", value);
}

static void
glade_packing_x_shrink_set (GObject *object, const gchar *value)
{
	g_print ("X shrink set %s\n", value);
}
			    
static void
glade_packing_y_shrink_set (GObject *object, const gchar *value)
{
	g_print ("Y shrink set %s\n", value);
}

static void
glade_packing_x_fill_set (GObject *object, const gchar *value)
{
	g_print ("X fill set %s\n", value);
}
			    
static void
glade_packing_y_fill_set (GObject *object, const gchar *value)
{
	g_print ("Y fill set %s\n", value);
}

static void
glade_packing_expand_set (GObject *object, const gchar *value)
{
	GladeWidget *glade_widget;
	GtkBox *box;
	GtkWidget *widget;
	gboolean expand, fill;
	gint padding;
	GtkPackType pack_type;

	widget = GTK_WIDGET (object);
	g_return_if_fail (GTK_IS_WIDGET (widget));
	glade_widget = glade_widget_get_from_gtk_widget (widget);
	g_return_if_fail (GLADE_IS_WIDGET (glade_widget));
	box = GTK_BOX (glade_widget->parent->widget);

	g_return_if_fail (GTK_IS_BOX (box));

	gtk_box_query_child_packing (box, widget,
				     &expand, &fill, &padding, &pack_type);

	expand = (strcmp (value, GLADE_TAG_TRUE) == 0);

	gtk_box_set_child_packing (box, widget,
				   expand, fill, padding, pack_type);
}

static void
glade_packing_fill_set (GObject *object, const gchar *value)
{
	GladeWidget *glade_widget;
	GtkBox *box;
	GtkWidget *widget;
	gboolean expand, fill;
	gint padding;
	GtkPackType pack_type;

	widget = GTK_WIDGET (object);
	g_return_if_fail (GTK_IS_WIDGET (widget));
	glade_widget = glade_widget_get_from_gtk_widget (widget);
	g_return_if_fail (GLADE_IS_WIDGET (glade_widget));
	box = GTK_BOX (glade_widget->parent->widget);

	g_return_if_fail (GTK_IS_BOX (box));

	gtk_box_query_child_packing (box, widget,
				     &expand, &fill, &padding, &pack_type);

	fill = (strcmp (value, GLADE_TAG_TRUE) == 0);

	gtk_box_set_child_packing (box, widget,
				   expand, fill, padding, pack_type);
}

static void
glade_packing_pack_start_set (GObject *object, const gchar *value)
{
	GladeWidget *glade_widget;
	GtkBox *box;
	GtkWidget *widget;
	gboolean expand, fill;
	gint padding;
	GtkPackType pack_type;

	widget = GTK_WIDGET (object);
	g_return_if_fail (GTK_IS_WIDGET (widget));
	glade_widget = glade_widget_get_from_gtk_widget (widget);
	g_return_if_fail (GLADE_IS_WIDGET (glade_widget));
	box = GTK_BOX (glade_widget->parent->widget);

	g_return_if_fail (GTK_IS_BOX (box));

	gtk_box_query_child_packing (box, widget,
				     &expand, &fill, &padding, &pack_type);

	pack_type = (strcmp (value, GLADE_TAG_TRUE) == 0) ? GTK_PACK_START : GTK_PACK_END;

	gtk_box_set_child_packing (box, widget,
				   expand, fill, padding, pack_type);
}


typedef struct _GladePackingProperty GladePackingProperty;
struct _GladePackingProperty
{
	const gchar *name;
	const gchar *id;
	void (*set_function) (GObject *object, const gchar *value);
	GladePropertyType type;
	const gchar *def;
};

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
	if (prop.def)
		class->def = g_strdup (prop.def);

	*list = g_list_prepend (*list, class);
}

GladePackingProperty table_props[] =
{
	{"X Expand", "xexpand", glade_packing_x_expand_set, GLADE_PROPERTY_TYPE_BOOLEAN, NULL},
	{"Y Expand", "yexpand", glade_packing_y_expand_set, GLADE_PROPERTY_TYPE_BOOLEAN, NULL},
	{"X Shrink", "xshrink", glade_packing_x_shrink_set, GLADE_PROPERTY_TYPE_BOOLEAN, NULL},
	{"Y Shrink", "yshrink", glade_packing_y_shrink_set, GLADE_PROPERTY_TYPE_BOOLEAN, NULL},
	{"X Fill",   "xfill",   glade_packing_x_fill_set,   GLADE_PROPERTY_TYPE_BOOLEAN, NULL},
	{"Y Fill",   "yfill",   glade_packing_y_fill_set,   GLADE_PROPERTY_TYPE_BOOLEAN, NULL},
};

GladePackingProperty box_props[] =
{
	{"Expand",     "expand",    glade_packing_expand_set,     GLADE_PROPERTY_TYPE_BOOLEAN, GLADE_TAG_FALSE},
	{"Fill",       "fill",      glade_packing_fill_set,       GLADE_PROPERTY_TYPE_BOOLEAN, GLADE_TAG_FALSE},
	{"Pack Start", "packstart", glade_packing_pack_start_set, GLADE_PROPERTY_TYPE_BOOLEAN, GLADE_TAG_TRUE},
};

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
