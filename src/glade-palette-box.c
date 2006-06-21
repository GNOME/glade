/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-palette-box.c
 *
 * Copyright (C) 2006 The Gnome Foundation.
 *
 * Authors:
 *   Vincent Geddes <vgeddes@metroweb.co.za>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA.
 *
 */

#include "glade-palette-box.h"


static void glade_palette_box_class_init (GladePaletteBoxClass *klass);

static void glade_palette_box_init (GladePaletteBox *box);

static void glade_palette_box_add (GtkContainer *container, GtkWidget *widget);

static void glade_palette_box_remove (GtkContainer *container, GtkWidget *widget);

static GType glade_palette_box_child_type (GtkContainer *container);

static void glade_palette_box_size_request (GtkWidget *widget, GtkRequisition *requisition);

static void glade_palette_box_size_allocate (GtkWidget *widget, GtkAllocation  *allocation);

static void glade_palette_box_forall (GtkContainer   *container,
					gboolean include_internals,
			        	GtkCallback callback,
			        	gpointer callback_data);

static void glade_palette_box_set_property (GObject *object,
		      		  guint prop_id,
		      		  const GValue *value,
		      		  GParamSpec *pspec);

static void glade_palette_box_get_property (GObject *object,
		      		  guint prop_id,
		      		  GValue *value,
		      		  GParamSpec *pspec);

static void glade_palette_box_set_child_property (GtkContainer    *container,
			    	                  GtkWidget       *child,
			    	                  guint            property_id,
			    	                  const GValue    *value,
			    	                  GParamSpec      *pspec);

static void glade_palette_box_get_child_property (GtkContainer *container,
		       	 	                  GtkWidget    *child,
				                  guint         property_id,
				                  GValue       *value,
				                  GParamSpec   *pspec);




static GtkContainerClass *parent_class = NULL;


enum {
  	PROP_0
};

enum {
	CHILD_PROP_0,
	CHILD_PROP_POSITION
};


GType
glade_palette_box_get_type (void)
{
	static GType type = 0;

	if (type == 0)
	{
		static const GTypeInfo info =
		{
			sizeof (GladePaletteBoxClass),
			NULL,
			NULL,
			(GClassInitFunc) glade_palette_box_class_init,
			NULL,
			NULL,
			sizeof (GladePaletteBox),
			0,
			(GInstanceInitFunc) glade_palette_box_init,
		};

		type = g_type_register_static (GTK_TYPE_CONTAINER,
		                               "GladePaletteBox",
		                               &info, 0);
	}

	return type;
}

static void
glade_palette_box_class_init (GladePaletteBoxClass *class)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class;

	parent_class = g_type_class_peek_parent (class);

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->set_property = glade_palette_box_set_property;
	gobject_class->get_property = glade_palette_box_get_property;

	container_class = GTK_CONTAINER_CLASS (class);
	container_class->add = glade_palette_box_add;
	container_class->remove = glade_palette_box_remove;
	container_class->forall = glade_palette_box_forall;
	container_class->child_type = glade_palette_box_child_type;
	container_class->set_child_property = glade_palette_box_set_child_property;
	container_class->get_child_property = glade_palette_box_get_child_property;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->size_request = glade_palette_box_size_request;
	widget_class->size_allocate = glade_palette_box_size_allocate;

	gtk_container_class_install_child_property (container_class,
						    CHILD_PROP_POSITION,
						    g_param_spec_int ("position", 
						                      "Position", 
						                      "The index of the child in the parent",
						                      -1, G_MAXINT, 0,
						                      G_PARAM_READWRITE));

}

static void
glade_palette_box_init (GladePaletteBox *box)
{
	GTK_WIDGET_SET_FLAGS (box, GTK_NO_WINDOW);
	gtk_widget_set_redraw_on_allocate (GTK_WIDGET (box), FALSE);

	box->children = NULL;
}

static void 
glade_palette_box_set_property (GObject *object,
		      		  guint prop_id,
		      		  const GValue *value,
		      		  GParamSpec *pspec)
{
  GladePaletteBox *box;

  box = GLADE_PALETTE_BOX (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
glade_palette_box_get_property (GObject *object,
		      		  guint prop_id,
		      		  GValue *value,
		      		  GParamSpec *pspec)
{
  GladePaletteBox *box;

  box = GLADE_PALETTE_BOX (object);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glade_palette_box_set_child_property (GtkContainer    *container,
			    	      GtkWidget       *child,
			    	      guint            property_id,
			    	      const GValue    *value,
			    	      GParamSpec      *pspec)
{

	switch (property_id)
	{
		case CHILD_PROP_POSITION:
			glade_palette_box_reorder_child (GLADE_PALETTE_BOX (container),
				 		         child,
						         g_value_get_int (value));
			break;
		default:
			GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
			break;
	}
}

static void
glade_palette_box_get_child_property (GtkContainer *container,
		       	 	      GtkWidget    *child,
				      guint         property_id,
				      GValue       *value,
				      GParamSpec   *pspec)
{
	GList *list;
	guint i;

	switch (property_id)
	{
		case CHILD_PROP_POSITION:
			i = 0;
			for (list = GLADE_PALETTE_BOX (container)->children; list; list = list->next)
			{
				GladePaletteBoxChild *child_entry;

				child_entry = list->data;
				if (child_entry->widget == child)
					break;
				i++;
			}
			g_value_set_int (value, list ? i : -1);
			break;
		default:
			GTK_CONTAINER_WARN_INVALID_CHILD_PROPERTY_ID (container, property_id, pspec);
			break;
	}
}

GtkWidget*
glade_palette_box_new (void)
{
	GladePaletteBox *box;

	box = g_object_new (GLADE_TYPE_PALETTE_BOX, NULL);

	return GTK_WIDGET (box);
}

/* do some badass mathematics */
static gint
calculate_children_width_allocation (GtkWidget *widget,
			             GtkAllocation *allocation,
			             GtkRequisition *child_requisition,
				     gint nvis_children)
{
	gint w;  /* container width */
	gint cw; /* children width request */
	gint tmp = 0;

	g_assert (child_requisition->width >= 0);

	w = allocation->width - GTK_CONTAINER (widget)->border_width;
	cw = child_requisition->width;

	if ((nvis_children * cw) < w )
		return cw;

	if ((tmp = w - w % cw) == 0)
		return child_requisition->width;
	else
		return w / (tmp / cw);
}

static void
glade_palette_box_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	GladePaletteBox *box;
	GladePaletteBoxChild *child;
	GtkRequisition child_requisition;
	GList *l;
	gint nvis_children = 0;

	box = GLADE_PALETTE_BOX (widget);

	requisition->width = 0;
	requisition->height = 0;
	child_requisition.width = 0;
	child_requisition.height = 0;

	for (l = box->children; l; l = l->next)
	{
		child = (GladePaletteBoxChild *) (l->data);

		if (GTK_WIDGET_VISIBLE (child->widget))
		{
			GtkRequisition requisition;
			gtk_widget_size_request (child->widget, &requisition);

			child_requisition.width = MAX (child_requisition.width, requisition.width);

			nvis_children += 1;
		}
	}

	if (nvis_children > 0)
	{
		requisition->width += child_requisition.width;
		requisition->height += child_requisition.height;
	}
 
	requisition->width += GTK_CONTAINER (box)->border_width * 2;
	requisition->height += GTK_CONTAINER (box)->border_width * 2;
}

static void
glade_palette_box_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GladePaletteBox *box;
	GladePaletteBoxChild *child;
	GList *l;
	GtkRequisition child_requisition;
	GtkAllocation child_allocation;
	gint nvis_children = 0;
	gint x, y;
	gint rows = 1;
	gint children_width;

	box = GLADE_PALETTE_BOX (widget);
	widget->allocation = *allocation;

	child_requisition.width = 0;
	child_requisition.height = 0;

	/* Retrieve maximum requisition from children */
	for (l = box->children; l; l = l->next)
	{
		child = (GladePaletteBoxChild *) (l->data);

		if (GTK_WIDGET_VISIBLE (child->widget))
		{
			GtkRequisition requisition;

			gtk_widget_get_child_requisition (child->widget, &requisition);
	
			child_requisition.width = MAX (child_requisition.width, requisition.width);
			child_requisition.height = MAX (child_requisition.height, requisition.height);

			nvis_children += 1;
		}
	}

	if (nvis_children <= 0)
		return;

	x = allocation->x + GTK_CONTAINER (box)->border_width;
	y = allocation->y + GTK_CONTAINER (box)->border_width;

	children_width = calculate_children_width_allocation (widget, allocation,
							      &child_requisition,
							      nvis_children);

	/* Allocate real estate to children */
	for (l = box->children; l; l = l->next)
	{
		child = (GladePaletteBoxChild *) (l->data);
		gint horizontal_space_remaining;

		if (GTK_WIDGET_VISIBLE (child->widget))
		{
			child_allocation.x = x;
			child_allocation.y = y;
			child_allocation.width = children_width;
			child_allocation.height = child_requisition.height;

			gtk_widget_size_allocate (child->widget, &child_allocation);

			x += child_allocation.width;

			/* calculate horizontal space remaining */
			horizontal_space_remaining = x
					             - allocation->x 
                                                     + GTK_CONTAINER (box)->border_width
                                                     + children_width;

			/* jump to next row */
			if ((horizontal_space_remaining > allocation->width) && l->next )
			{
				x = allocation->x + GTK_CONTAINER (box)->border_width;
				y += child_allocation.height;
				rows++;
			}
		}
	}

	/* force minimum height */
	gtk_widget_set_size_request (widget, -1, rows * child_allocation.height);
}
				     
static void 
glade_palette_box_add (GtkContainer *container, GtkWidget *widget)
{
	GladePaletteBox *box;
	GladePaletteBoxChild *child;

	g_return_if_fail (GLADE_IS_PALETTE_BOX (container));
	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (widget->parent == NULL);

	box = GLADE_PALETTE_BOX (container);

	child = g_new (GladePaletteBoxChild, 1);
	child->widget = widget;
	box->children = g_list_append (box->children, child);

	gtk_widget_set_parent (widget, GTK_WIDGET (box));

}

static void
glade_palette_box_remove (GtkContainer *container, GtkWidget *widget)
{
	GladePaletteBox *box;
	GladePaletteBoxChild *child;
	GList *children;

	g_return_if_fail (GLADE_IS_PALETTE_BOX (container));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	box = GLADE_PALETTE_BOX (container);

  	children = box->children;
	while (children != NULL)
	{
		child = children->data;
		children = g_list_next (children);

		if (child->widget == widget)
		{
			gboolean was_visible;

			was_visible = GTK_WIDGET_VISIBLE (widget);

			gtk_widget_unparent (widget);

			box->children = g_list_remove (box->children, child);
			g_free (child);

			if (was_visible)
				gtk_widget_queue_resize (GTK_WIDGET (container));
			break;
		}

	}

}

static void
glade_palette_box_forall (GtkContainer *container,
			    gboolean include_internals,
			    GtkCallback callback,
			    gpointer callback_data)
{
	GladePaletteBox *box;
	GladePaletteBoxChild *child;
	GList *children;

	g_return_if_fail (callback != NULL);

	box = GLADE_PALETTE_BOX (container);

	children = box->children;
	while (children != NULL)
	{
		child = children->data;
		children = g_list_next (children);

		(* callback) (child->widget, callback_data);
	}
}

void
glade_palette_box_reorder_child (GladePaletteBox *box,
			         GtkWidget       *child,
			         gint             position)
{
	GList *old_link;
	GList *new_link;
	GladePaletteBoxChild *child_info = NULL;
	gint old_position;

	g_return_if_fail (GLADE_IS_PALETTE_BOX (box));
	g_return_if_fail (GTK_IS_WIDGET (child));

	old_link = box->children;
	old_position = 0;
	while (old_link)
	{
		child_info = old_link->data;
		if (child_info->widget == child)
			break;

		old_link = old_link->next;
		old_position++;
	}

	g_return_if_fail (old_link != NULL);

	if (position == old_position)
		return;

	box->children = g_list_delete_link (box->children, old_link);

	if (position < 0)
		new_link = NULL;
	else
		new_link = g_list_nth (box->children, position);

	box->children = g_list_insert_before (box->children, new_link, child_info);

	gtk_widget_child_notify (child, "position");
	if (GTK_WIDGET_VISIBLE (child) && GTK_WIDGET_VISIBLE (box))
		gtk_widget_queue_resize (child);
}

static GType
glade_palette_box_child_type (GtkContainer *container)
{
  return GTK_TYPE_WIDGET;
}
