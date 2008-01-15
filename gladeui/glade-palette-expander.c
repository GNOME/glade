/*
 * glade-palette-expander.c - A container which can hide its child
 *
 * Copyright (C) 2007 Vincent Geddes
 *
 * Author: Vincent Geddes <vincent.geddes@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <config.h>

#include "glade-palette-expander.h"

#include <gtk/gtkcontainer.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkarrow.h>
#include <gtk/gtkdnd.h>

#define GLADE_PALETTE_EXPANDER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
					       GLADE_TYPE_PALETTE_EXPANDER,      \
					       GladePaletteExpanderPrivate))

enum
{
	PROP_0,
	PROP_EXPANDED,
	PROP_LABEL,
	PROP_SPACING,
	PROP_USE_MARKUP
};

struct _GladePaletteExpanderPrivate
{
	GtkWidget *button;
	GtkWidget *arrow;
	GtkWidget *label;

	gboolean   expanded;
	gboolean   use_markup;
	gboolean   button_down;
	guint      spacing;
	guint      expand_timer_handle;
	
};

static void glade_palette_expander_set_property (GObject      *object,
						 guint         prop_id,
						 const GValue *value,
						 GParamSpec   *pspec);
static void glade_palette_expander_get_property (GObject    *object,
						 guint       prop_id,
						 GValue     *value,
						 GParamSpec *pspec);
static void   glade_palette_expander_size_request  (GtkWidget      *widget,
						    GtkRequisition *requisition);
static void   glade_palette_expander_size_allocate (GtkWidget      *widget,
						    GtkAllocation  *allocation);			     
static void   glade_palette_expander_map           (GtkWidget      *widget);
static void   glade_palette_expander_unmap         (GtkWidget      *widget);
static void   glade_palette_expander_add           (GtkContainer   *container,
						    GtkWidget      *widget);
static void   glade_palette_expander_remove        (GtkContainer   *container,
						    GtkWidget      *widget);	    
static void   glade_palette_expander_forall        (GtkContainer   *container,
						    gboolean        include_internals,
						    GtkCallback     callback,
						    gpointer        callback_data);
static void        button_clicked (GtkButton *button,
				   GladePaletteExpander *expander);
				   

static gboolean    button_drag_motion (GtkWidget        *widget,
				       GdkDragContext   *context,
				       gint              x,
				       gint              y,
				       guint             time,
				       GladePaletteExpander *expander);

static void        button_drag_leave (GtkWidget      *widget,
				      GdkDragContext *context,
				      guint           time,
				      GladePaletteExpander *expander);
		   
static void   glade_palette_expander_activate      (GladePaletteExpander *expander);			       
void          glade_palette_expander_set_expanded  (GladePaletteExpander *expander,
						    gboolean              expanded);
gboolean      glade_palette_expander_get_expanded  (GladePaletteExpander *expander);
void          glade_palette_expander_set_label     (GladePaletteExpander *expander,
						    const gchar          *label);
const gchar  *glade_palette_expander_get_label     (GladePaletteExpander *expander);
void          glade_palette_expander_set_spacing   (GladePaletteExpander *expander,
						    guint                 spacing);
guint         glade_palette_expander_get_spacing   (GladePaletteExpander *expander);


G_DEFINE_TYPE (GladePaletteExpander, glade_palette_expander, GTK_TYPE_BIN);

static void
glade_palette_expander_class_init (GladePaletteExpanderClass *klass)
{
	GObjectClass      *object_class;
	GtkWidgetClass    *widget_class;
	GtkContainerClass *container_class;

	object_class    = G_OBJECT_CLASS (klass);
	widget_class    = GTK_WIDGET_CLASS (klass);
	container_class = GTK_CONTAINER_CLASS (klass);

	object_class->set_property = glade_palette_expander_set_property;
	object_class->get_property = glade_palette_expander_get_property;

	container_class->add    = glade_palette_expander_add;
	container_class->remove = glade_palette_expander_remove;
	container_class->forall = glade_palette_expander_forall;
	
	widget_class->size_request         = glade_palette_expander_size_request;
	widget_class->size_allocate        = glade_palette_expander_size_allocate;
	widget_class->map                  = glade_palette_expander_map;
	widget_class->unmap                = glade_palette_expander_unmap;

	klass->activate = glade_palette_expander_activate;

	g_object_class_install_property (object_class,
					 PROP_EXPANDED,
					 g_param_spec_boolean ("expanded",
							       "Expanded",
							       "Whether the expander has been opened to "
							       "reveal the child widget",
					 		       FALSE,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (object_class,
					 PROP_LABEL,
					 g_param_spec_string ("label",
							      "Label",
							      "Text of the expander's label",
							      NULL,
					 		      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
					 		      
	g_object_class_install_property (object_class,
					 PROP_SPACING,
					 g_param_spec_uint ("spacing",
							    "Spacing",
							    "Space to put between the expander and the child widget",
							    0,
							    G_MAXUINT,
							    0,
					 		    G_PARAM_READWRITE));
					 		    
	g_object_class_install_property (object_class,
					 PROP_USE_MARKUP,
					 g_param_spec_boolean ("use-markup",
							       "Use Markup",
							       "The text of the label includes Pango XML markup"
							       "reveal the child widget",
					 		       FALSE,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
					 		      
	widget_class->activate_signal =
		g_signal_new ("activate",
			  G_TYPE_FROM_CLASS (object_class),
			  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			  G_STRUCT_OFFSET (GladePaletteExpanderClass, activate),
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (GladePaletteExpanderPrivate));
}

static void
glade_palette_expander_init (GladePaletteExpander *expander)
{
	GladePaletteExpanderPrivate *priv;
	GtkWidget *hbox;
	GtkWidget *alignment;

	expander->priv = priv = GLADE_PALETTE_EXPANDER_GET_PRIVATE (expander);
	
	GTK_WIDGET_SET_FLAGS (expander, GTK_NO_WINDOW);
	
	priv->spacing = 0;
	priv->expanded = FALSE;
	priv->use_markup = FALSE;
	priv->button_down = FALSE;
	
	gtk_widget_push_composite_child ();
	
	priv->button = gtk_button_new ();
	gtk_button_set_focus_on_click (GTK_BUTTON (priv->button), FALSE);
	priv->arrow = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE);
	priv->label = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (priv->label), PANGO_ELLIPSIZE_END);
	gtk_misc_set_alignment (GTK_MISC (priv->label), 0.0, 0.5);

	alignment = gtk_alignment_new (0.0, 0.5, 1, 1);

	hbox = gtk_hbox_new (FALSE, 3);
	
	gtk_container_add (GTK_CONTAINER (alignment), hbox);
	gtk_container_add (GTK_CONTAINER (priv->button), alignment);
	gtk_box_pack_start (GTK_BOX (hbox), priv->arrow, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), priv->label, TRUE, TRUE, 0);
	
	gtk_widget_set_parent (priv->button, GTK_WIDGET (expander));
	
	gtk_widget_pop_composite_child ();
			  
	g_signal_connect (priv->button, "clicked",
			  G_CALLBACK (button_clicked),
			  expander);

	g_signal_connect (priv->button, "drag-motion",
			  G_CALLBACK (button_drag_motion),
			  expander);

	g_signal_connect (priv->button, "drag-leave",
			  G_CALLBACK (button_drag_leave),
			  expander);				  

	gtk_widget_show_all (priv->button);
	
	gtk_drag_dest_set (GTK_WIDGET (priv->button), 0, NULL, 0, 0);
	gtk_drag_dest_set_track_motion (GTK_WIDGET (priv->button), TRUE);
}

static void
glade_palette_expander_set_property (GObject      *object,
				     guint         prop_id,
				     const GValue *value,
				     GParamSpec   *pspec)
{
	GladePaletteExpander *expander = GLADE_PALETTE_EXPANDER (object);

	switch (prop_id)
	{
	case PROP_EXPANDED:
		glade_palette_expander_set_expanded (expander, g_value_get_boolean (value));
		break;
	case PROP_LABEL:
		glade_palette_expander_set_label (expander, g_value_get_string (value));
		break;
	case PROP_SPACING:
		glade_palette_expander_set_spacing (expander, g_value_get_int (value));
		break;
	case PROP_USE_MARKUP:
		glade_palette_expander_set_use_markup (expander, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_palette_expander_get_property (GObject    *object,
				     guint       prop_id,
				     GValue     *value,
				     GParamSpec *pspec)
{
	GladePaletteExpander *expander = GLADE_PALETTE_EXPANDER (object);

	switch (prop_id)
	{
	case PROP_EXPANDED:
		g_value_set_boolean (value, glade_palette_expander_get_expanded (expander));
		break;
	case PROP_LABEL:
		g_value_set_string (value, glade_palette_expander_get_label (expander));
		break;
	case PROP_SPACING:
		g_value_set_int (value, glade_palette_expander_get_spacing (expander));
		break;
	case PROP_USE_MARKUP:
		g_value_set_boolean (value, glade_palette_expander_get_use_markup (expander));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_palette_expander_size_request (GtkWidget       *widget,
				     GtkRequisition  *requisition)
{
	GladePaletteExpanderPrivate *priv;
	GladePaletteExpander *expander;
	GtkBin *bin;
	gint border_width;

	bin = GTK_BIN (widget);
	expander = GLADE_PALETTE_EXPANDER (widget);
	priv = expander->priv;

	border_width = GTK_CONTAINER (widget)->border_width;

	requisition->width = 0;
	requisition->height =  0;


	if (GTK_WIDGET_VISIBLE (priv->button))
	{
		GtkRequisition button_requisition;

		gtk_widget_size_request (priv->button, &button_requisition);

		requisition->width  += button_requisition.width;
		requisition->height += button_requisition.height;
	}

	if (bin->child && gtk_widget_get_child_visible (bin->child))
	{
		GtkRequisition child_requisition;

		gtk_widget_size_request (bin->child, &child_requisition);

		requisition->width = MAX (requisition->width, child_requisition.width);
		requisition->height += child_requisition.height + priv->spacing;
	}

	requisition->width  += 2 * border_width;
	requisition->height += 2 * border_width;
}

static void
glade_palette_expander_size_allocate (GtkWidget     *widget,
				      GtkAllocation *allocation)
{
	GladePaletteExpanderPrivate *priv;
	GladePaletteExpander *expander;
	GtkBin *bin;
	GtkRequisition child_requisition;
	gboolean child_visible = FALSE;
	gint border_width;
	gint button_height;

	expander = GLADE_PALETTE_EXPANDER (widget);
	bin = GTK_BIN (widget);
	priv = expander->priv;

	border_width = GTK_CONTAINER (widget)->border_width;

	child_requisition.width = 0;
	child_requisition.height = 0;	
	
	if (bin->child && gtk_widget_get_child_visible (bin->child))
	{
		child_visible = TRUE;
		gtk_widget_get_child_requisition (bin->child, &child_requisition);
	}

	widget->allocation = *allocation;

	if (GTK_WIDGET_VISIBLE (priv->button))
	{
		GtkAllocation button_allocation;
		GtkRequisition button_requisition;

		gtk_widget_get_child_requisition (priv->button, &button_requisition);

		button_allocation.x = widget->allocation.x + border_width;
		button_allocation.y = widget->allocation.y + border_width;

		button_allocation.width = MAX (allocation->width - 2 * border_width, 1);    

		button_allocation.height = MIN (button_requisition.height,
					        allocation->height - 2 * border_width
					        - (child_visible ? priv->spacing : 0));
					         
		button_allocation.height = MAX (button_allocation.height, 1);

		gtk_widget_size_allocate (priv->button, &button_allocation);

		button_height = button_allocation.height;
	}
	else
	{
		button_height = 0;
	}

	if (child_visible)
	{
		GtkAllocation child_allocation;

		child_allocation.x = widget->allocation.x + border_width;
		child_allocation.y = widget->allocation.y + border_width + button_height + priv->spacing;

		child_allocation.width = MAX (allocation->width - 2 * border_width, 1);

		child_allocation.height = allocation->height - button_height -
					  2 * border_width - priv->spacing;
		child_allocation.height = MAX (child_allocation.height, 1);

		gtk_widget_size_allocate (bin->child, &child_allocation);
	}
}

static void
glade_palette_expander_map (GtkWidget *widget)
{
	GladePaletteExpanderPrivate *priv = GLADE_PALETTE_EXPANDER (widget)->priv;

	gtk_widget_map (priv->button);

	GTK_WIDGET_CLASS (glade_palette_expander_parent_class)->map (widget);
}

static void
glade_palette_expander_unmap (GtkWidget *widget)
{
	GladePaletteExpanderPrivate *priv = GLADE_PALETTE_EXPANDER (widget)->priv;

	GTK_WIDGET_CLASS (glade_palette_expander_parent_class)->unmap (widget);

	gtk_widget_unmap (priv->button);
}

static void
glade_palette_expander_add (GtkContainer *container,
			    GtkWidget    *widget)
{
	GTK_CONTAINER_CLASS (glade_palette_expander_parent_class)->add (container, widget);

	gtk_widget_set_child_visible (widget, GLADE_PALETTE_EXPANDER (container)->priv->expanded);
	gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
glade_palette_expander_remove (GtkContainer *container,
			       GtkWidget    *widget)
{
  GladePaletteExpander *expander = GLADE_PALETTE_EXPANDER (container);

	if (expander->priv->button == widget)
	{	
		gtk_widget_unparent (expander->priv->button);
		expander->priv->button = NULL;

		if (GTK_WIDGET_VISIBLE (expander))
			gtk_widget_queue_resize (GTK_WIDGET (expander));

		g_object_notify (G_OBJECT (expander), "label");
	}
	else
	{
    		GTK_CONTAINER_CLASS (glade_palette_expander_parent_class)->remove (container, widget);
	}

}

static void
glade_palette_expander_forall (GtkContainer *container,
			       gboolean      include_internals,
			       GtkCallback   callback,
			       gpointer      callback_data)
{
	GtkBin *bin = GTK_BIN (container);
	GladePaletteExpanderPrivate *priv = GLADE_PALETTE_EXPANDER_GET_PRIVATE (container);
	
	if (bin->child)
		(* callback) (bin->child, callback_data);
	
	if (priv->button)	
		(* callback) (priv->button, callback_data);
}

static gboolean
expand_timeout (gpointer data)
{
	GladePaletteExpander *expander = (GladePaletteExpander *) (data);
	GladePaletteExpanderPrivate *priv = expander->priv;

	GDK_THREADS_ENTER ();

	priv->expand_timer_handle = 0;
	glade_palette_expander_set_expanded (expander, TRUE);

	GDK_THREADS_LEAVE ();

	return FALSE;
}

static gboolean
button_drag_motion (GtkWidget        *widget,
		    GdkDragContext   *context,
		    gint              x,
		    gint              y,
		    guint             time,
		    GladePaletteExpander *expander)
{
	GladePaletteExpanderPrivate *priv = expander->priv;

	if (!priv->expanded && !priv->expand_timer_handle)
	{
		GtkSettings *settings;
		guint timeout;

		settings = gtk_widget_get_settings (widget);
		g_object_get (settings, "gtk-timeout-expand", &timeout, NULL);

		priv->expand_timer_handle = g_timeout_add (timeout,
							   (GSourceFunc) expand_timeout,
							   expander);
	}

  return TRUE;
}

static void
button_drag_leave (GtkWidget      *widget,
		   GdkDragContext *context,
		   guint           time,
		   GladePaletteExpander *expander)
{
	GladePaletteExpanderPrivate *priv = expander->priv;

	if (priv->expand_timer_handle)
	{
		g_source_remove (priv->expand_timer_handle);
		priv->expand_timer_handle = 0;
	}
}

static void
button_clicked (GtkButton            *button,
		GladePaletteExpander *expander)
{
	gtk_widget_activate (GTK_WIDGET (expander));
}

static void
glade_palette_expander_activate (GladePaletteExpander *expander)
{
	glade_palette_expander_set_expanded (expander, !expander->priv->expanded);
}

void
glade_palette_expander_set_expanded (GladePaletteExpander *expander, gboolean expanded)
{
	GladePaletteExpanderPrivate *priv;

	g_return_if_fail (GLADE_IS_PALETTE_EXPANDER (expander));

	priv = expander->priv;

	expanded = expanded != FALSE;

	if (priv->expanded != expanded)
	{
		priv->expanded = expanded;

		if (GTK_BIN (expander)->child)
		{
			gtk_widget_set_child_visible (GTK_BIN (expander)->child, priv->expanded);
			gtk_widget_queue_resize (GTK_WIDGET (expander));
		}

		gtk_arrow_set (GTK_ARROW (priv->arrow),
			       priv->expanded ? GTK_ARROW_DOWN : GTK_ARROW_RIGHT,
			       GTK_SHADOW_NONE);

		g_object_notify (G_OBJECT (expander), "expanded");
	}
}

gboolean
glade_palette_expander_get_expanded (GladePaletteExpander *expander)
{
	g_return_val_if_fail (GLADE_IS_PALETTE_EXPANDER (expander), FALSE);
	
	return expander->priv->expanded;
}

void
glade_palette_expander_set_label (GladePaletteExpander *expander, const gchar *label)
{
	GladePaletteExpanderPrivate *priv;
	g_return_if_fail (GLADE_IS_PALETTE_EXPANDER (expander));

	priv = GLADE_PALETTE_EXPANDER_GET_PRIVATE (expander);

	gtk_label_set_label (GTK_LABEL (priv->label), label);
	g_object_notify (G_OBJECT (expander), "label"); 
}

const gchar *
glade_palette_expander_get_label (GladePaletteExpander *expander)
{
	g_return_val_if_fail (GLADE_IS_PALETTE_EXPANDER (expander), NULL);
	
	return gtk_label_get_label (GTK_LABEL (expander->priv->label));
}

void
glade_palette_expander_set_spacing (GladePaletteExpander *expander,
				    guint        spacing)
{
	g_return_if_fail (GLADE_IS_PALETTE_EXPANDER (expander));

	if (expander->priv->spacing != spacing)
	{
		expander->priv->spacing = spacing;

		gtk_widget_queue_resize (GTK_WIDGET (expander));

		g_object_notify (G_OBJECT (expander), "spacing");
	}
}

guint
glade_palette_expander_get_spacing (GladePaletteExpander *expander)
{
	g_return_val_if_fail (GLADE_IS_PALETTE_EXPANDER (expander), 0);

	return expander->priv->spacing;
}

void
glade_palette_expander_set_use_markup (GladePaletteExpander *expander,
				       gboolean              use_markup)
{
	GladePaletteExpanderPrivate *priv;

	g_return_if_fail (GLADE_IS_PALETTE_EXPANDER (expander));

	priv = expander->priv;

	use_markup = use_markup != FALSE;

	if (priv->use_markup != use_markup)
	{
		priv->use_markup = use_markup;

		gtk_label_set_use_markup (GTK_LABEL (priv->label), use_markup);

		g_object_notify (G_OBJECT (expander), "use-markup");
	}
}

gboolean
glade_palette_expander_get_use_markup (GladePaletteExpander *expander)
{
	g_return_val_if_fail (GLADE_IS_PALETTE_EXPANDER (expander), FALSE);

	return expander->priv->use_markup;
}

GtkWidget *
glade_palette_expander_new (const gchar *label)
{
	return g_object_new (GLADE_TYPE_PALETTE_EXPANDER,
			     "label", label,
			     NULL);
}

