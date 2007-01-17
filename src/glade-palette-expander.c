/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-palette-expander.c
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 *
 * Modified for use in glade-3.
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 *      
 */

#include "config.h"

#include "glade-palette-expander.h"

#include <gtk/gtkexpander.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkmarshal.h>
#include <gtk/gtkmain.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkprivate.h>

#define GLADE_PALETTE_EXPANDER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GLADE_TYPE_PALETTE_EXPANDER, GladePaletteExpanderPrivate))

#define DEFAULT_EXPANDER_SIZE 16
#define DEFAULT_EXPANDER_SPACING 2

enum
{
  PROP_0,
  PROP_EXPANDED,
  PROP_LABEL,
  PROP_USE_UNDERLINE,
  PROP_USE_MARKUP,
  PROP_SPACING,
  PROP_LABEL_WIDGET
};

struct _GladePaletteExpanderPrivate
{
  GtkWidget        *label_widget;
  GdkWindow        *event_window;
  gint              spacing;

  GtkExpanderStyle  expander_style;
  guint             animation_timeout;

  guint             expanded : 1;
  guint             use_underline : 1;
  guint             use_markup : 1; 
  guint             button_down : 1;
  guint             prelight : 1;
};

static void glade_palette_expander_class_init (GladePaletteExpanderClass *klass);
static void glade_palette_expander_init       (GladePaletteExpander      *expander);

static void glade_palette_expander_set_property (GObject          *object,
				       guint             prop_id,
				       const GValue     *value,
				       GParamSpec       *pspec);
static void glade_palette_expander_get_property (GObject          *object,
				       guint             prop_id,
				       GValue           *value,
				       GParamSpec       *pspec);

static void glade_palette_expander_destroy (GtkObject *object);

static void     glade_palette_expander_realize        (GtkWidget        *widget);
static void     glade_palette_expander_unrealize      (GtkWidget        *widget);
static void     glade_palette_expander_size_request   (GtkWidget        *widget,
					               GtkRequisition   *requisition);
static void     glade_palette_expander_size_allocate  (GtkWidget        *widget,
					               GtkAllocation    *allocation);
static void     glade_palette_expander_map            (GtkWidget        *widget);
static void     glade_palette_expander_unmap          (GtkWidget        *widget);
static gboolean glade_palette_expander_expose         (GtkWidget        *widget,
					               GdkEventExpose   *event);
static gboolean glade_palette_expander_button_press   (GtkWidget        *widget,
					               GdkEventButton   *event);
static gboolean glade_palette_expander_button_release (GtkWidget        *widget,
					               GdkEventButton   *event);
static gboolean glade_palette_expander_enter_notify   (GtkWidget        *widget,
					               GdkEventCrossing *event);
static gboolean glade_palette_expander_leave_notify   (GtkWidget        *widget,
					               GdkEventCrossing *event);
static gboolean glade_palette_expander_focus          (GtkWidget        *widget,
					               GtkDirectionType  direction);
static void     glade_palette_expander_grab_notify    (GtkWidget        *widget,
					               gboolean          was_grabbed);
static void     glade_palette_expander_state_changed  (GtkWidget        *widget,
					               GtkStateType      previous_state);

static void glade_palette_expander_add    (GtkContainer *container,
				           GtkWidget    *widget);
static void glade_palette_expander_remove (GtkContainer *container,
				           GtkWidget    *widget);
static void glade_palette_expander_forall (GtkContainer *container,
				           gboolean        include_internals,
				           GtkCallback     callback,
				           gpointer        callback_data);

static void glade_palette_expander_activate (GladePaletteExpander *expander);

static void get_expander_bounds (GladePaletteExpander  *expander,
				 GdkRectangle *rect);

static GtkBinClass *parent_class = NULL;

GType
glade_palette_expander_get_type (void)
{
  static GType expander_type = 0;
  
  if (!expander_type)
    {
      static const GTypeInfo expander_info =
      {
	sizeof (GladePaletteExpanderClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) glade_palette_expander_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GladePaletteExpander),
	0,		/* n_preallocs */
	(GInstanceInitFunc) glade_palette_expander_init,
      };
      
      expander_type = g_type_register_static (GTK_TYPE_BIN,
					      "GladePaletteExpander",
					      &expander_info, 0);
    }
  
  return expander_type;
}

static void
glade_palette_expander_class_init (GladePaletteExpanderClass *klass)
{
  GObjectClass *gobject_class;
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class   = (GObjectClass *) klass;
  object_class    = (GtkObjectClass *) klass;
  widget_class    = (GtkWidgetClass *) klass;
  container_class = (GtkContainerClass *) klass;

  gobject_class->set_property = glade_palette_expander_set_property;
  gobject_class->get_property = glade_palette_expander_get_property;

  object_class->destroy = glade_palette_expander_destroy;

  widget_class->realize              = glade_palette_expander_realize;
  widget_class->unrealize            = glade_palette_expander_unrealize;
  widget_class->size_request         = glade_palette_expander_size_request;
  widget_class->size_allocate        = glade_palette_expander_size_allocate;
  widget_class->map                  = glade_palette_expander_map;
  widget_class->unmap                = glade_palette_expander_unmap;
  widget_class->expose_event         = glade_palette_expander_expose;
  widget_class->button_press_event   = glade_palette_expander_button_press;
  widget_class->button_release_event = glade_palette_expander_button_release;
  widget_class->enter_notify_event   = glade_palette_expander_enter_notify;
  widget_class->leave_notify_event   = glade_palette_expander_leave_notify;
  widget_class->focus                = glade_palette_expander_focus;
  widget_class->grab_notify          = glade_palette_expander_grab_notify;
  widget_class->state_changed        = glade_palette_expander_state_changed;

  container_class->add    = glade_palette_expander_add;
  container_class->remove = glade_palette_expander_remove;
  container_class->forall = glade_palette_expander_forall;

  klass->activate = glade_palette_expander_activate;

  g_type_class_add_private (klass, sizeof (GladePaletteExpanderPrivate));

  g_object_class_install_property (gobject_class,
				   PROP_EXPANDED,
				   g_param_spec_boolean ("expanded",
							 "Expanded",
							 "Whether the expander has been opened to reveal the child widget",
							 FALSE,
							 G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
				   PROP_LABEL,
				   g_param_spec_string ("label",
							"Label",
							"Text of the expander's label",
							NULL,
							G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
				   PROP_USE_UNDERLINE,
				   g_param_spec_boolean ("use-underline",
							 "Use underline",
							 "If set, an underline in the text indicates the next character should be used for the mnemonic accelerator key",
							 FALSE,
							 G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
				   PROP_USE_MARKUP,
				   g_param_spec_boolean ("use-markup",
							 "Use markup",
							 "The text of the label includes XML markup. See pango_parse_markup()",
							 FALSE,
							 G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property (gobject_class,
				   PROP_SPACING,
				   g_param_spec_int ("spacing",
						     "Spacing",
						     "Space to put between the label and the child",
						     0,
						     G_MAXINT,
						     0,
						     G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
				   PROP_LABEL_WIDGET,
				   g_param_spec_object ("label-widget",
							"Label widget",
							"A widget to display in place of the usual expander label",
							GTK_TYPE_WIDGET,
							G_PARAM_READWRITE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("expander-size",
							     "Expander Size",
							     "Size of the expander arrow",
							     0,
							     G_MAXINT,
							     DEFAULT_EXPANDER_SIZE,
							     G_PARAM_READABLE));

  gtk_widget_class_install_style_property (widget_class,
					   g_param_spec_int ("expander-spacing",
							     "Indicator Spacing",
							     "Spacing around expander arrow",
							     0,
							     G_MAXINT,
							     DEFAULT_EXPANDER_SPACING,
							     G_PARAM_READABLE));

  widget_class->activate_signal =
    g_signal_new ("activate",
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GladePaletteExpanderClass, activate),
		  NULL, NULL,
		  gtk_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
}

static void
glade_palette_expander_init (GladePaletteExpander *expander)
{
  GladePaletteExpanderPrivate *priv;

  expander->priv = priv = GLADE_PALETTE_EXPANDER_GET_PRIVATE (expander);

  GTK_WIDGET_SET_FLAGS (expander, GTK_CAN_FOCUS);
  GTK_WIDGET_SET_FLAGS (expander, GTK_NO_WINDOW);

  priv->label_widget = NULL;
  priv->event_window = NULL;
  priv->spacing = 0;

  priv->expander_style = GTK_EXPANDER_COLLAPSED;
  priv->animation_timeout = 0;

  priv->expanded = FALSE;
  priv->use_underline = FALSE;
  priv->use_markup = FALSE;
  priv->button_down = FALSE;
  priv->prelight = FALSE;
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
    case PROP_USE_UNDERLINE:
      glade_palette_expander_set_use_underline (expander, g_value_get_boolean (value));
      break;
    case PROP_USE_MARKUP:
      glade_palette_expander_set_use_markup (expander, g_value_get_boolean (value));
      break;
    case PROP_SPACING:
      glade_palette_expander_set_spacing (expander, g_value_get_int (value));
      break;
    case PROP_LABEL_WIDGET:
      glade_palette_expander_set_label_widget (expander, g_value_get_object (value));
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
  GladePaletteExpanderPrivate *priv = expander->priv;

  switch (prop_id)
    {
    case PROP_EXPANDED:
      g_value_set_boolean (value, priv->expanded);
      break;
    case PROP_LABEL:
      g_value_set_string (value, glade_palette_expander_get_label (expander));
      break;
    case PROP_USE_UNDERLINE:
      g_value_set_boolean (value, priv->use_underline);
      break;
    case PROP_USE_MARKUP:
      g_value_set_boolean (value, priv->use_markup);
      break;
    case PROP_SPACING:
      g_value_set_int (value, priv->spacing);
      break;
    case PROP_LABEL_WIDGET:
      g_value_set_object (value,
			  priv->label_widget ?
			  G_OBJECT (priv->label_widget) : NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glade_palette_expander_destroy (GtkObject *object)
{
  GladePaletteExpanderPrivate *priv = GLADE_PALETTE_EXPANDER (object)->priv;
  
  if (priv->animation_timeout)
    {
      g_source_remove (priv->animation_timeout);
      priv->animation_timeout = 0;
    }
  
  GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
glade_palette_expander_realize (GtkWidget *widget)
{
  GladePaletteExpanderPrivate *priv;
  GdkWindowAttr attributes;
  gint attributes_mask;
  gint border_width;
  GdkRectangle expander_rect;

  priv = GLADE_PALETTE_EXPANDER (widget)->priv;
  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  border_width = GTK_CONTAINER (widget)->border_width;

  get_expander_bounds (GLADE_PALETTE_EXPANDER (widget), &expander_rect);
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x + border_width;
  attributes.y = expander_rect.y;
  attributes.width = MAX (widget->allocation.width - 2 * border_width, 1);
  attributes.height = expander_rect.width;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = gtk_widget_get_events (widget)     |
				GDK_BUTTON_PRESS_MASK        |
				GDK_BUTTON_RELEASE_MASK      |
				GDK_ENTER_NOTIFY_MASK        |
				GDK_LEAVE_NOTIFY_MASK;

  attributes_mask = GDK_WA_X | GDK_WA_Y;

  widget->window = gtk_widget_get_parent_window (widget);
  g_object_ref (widget->window);

  priv->event_window = gdk_window_new (gtk_widget_get_parent_window (widget),
				       &attributes, attributes_mask);
  gdk_window_set_user_data (priv->event_window, widget);

  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void
glade_palette_expander_unrealize (GtkWidget *widget)
{
  GladePaletteExpanderPrivate *priv = GLADE_PALETTE_EXPANDER (widget)->priv;

  if (priv->event_window)
    {
      gdk_window_set_user_data (priv->event_window, NULL);
      gdk_window_destroy (priv->event_window);
      priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
glade_palette_expander_size_request (GtkWidget      *widget,
			   GtkRequisition *requisition)
{
  GladePaletteExpander *expander;
  GtkBin *bin;
  GladePaletteExpanderPrivate *priv;
  gint border_width;
  gint expander_size;
  gint expander_spacing;
  gboolean interior_focus;
  gint focus_width;
  gint focus_pad;

  bin = GTK_BIN (widget);
  expander = GLADE_PALETTE_EXPANDER (widget);
  priv = expander->priv;

  border_width = GTK_CONTAINER (widget)->border_width;

  gtk_widget_style_get (widget,
			"interior-focus", &interior_focus,
			"focus-line-width", &focus_width,
			"focus-padding", &focus_pad,
			"expander-size", &expander_size,
			"expander-spacing", &expander_spacing,
			NULL);

  requisition->width = expander_size + 2 * expander_spacing +
		       2 * focus_width + 2 * focus_pad;
  requisition->height = interior_focus ? (2 * focus_width + 2 * focus_pad) : 0;

  if (priv->label_widget && GTK_WIDGET_VISIBLE (priv->label_widget))
    {
      GtkRequisition label_requisition;

      gtk_widget_size_request (priv->label_widget, &label_requisition);

      requisition->width  += label_requisition.width;
      requisition->height += label_requisition.height;
    }

  requisition->height = MAX (expander_size + 2 * expander_spacing, requisition->height);

  if (!interior_focus)
    requisition->height += 2 * focus_width + 2 * focus_pad;

  if (bin->child && GTK_WIDGET_CHILD_VISIBLE (bin->child))
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
get_expander_bounds (GladePaletteExpander  *expander,
		     GdkRectangle *rect)
{
  GtkWidget *widget;
  GtkBin *bin;
  GladePaletteExpanderPrivate *priv;
  gint border_width;
  gint expander_size;
  gint expander_spacing;
  gboolean interior_focus;
  gint focus_width;
  gint focus_pad;
  gboolean ltr;

  widget = GTK_WIDGET (expander);
  bin = GTK_BIN (expander);
  priv = expander->priv;

  border_width = GTK_CONTAINER (expander)->border_width;

  gtk_widget_style_get (widget,
			"interior-focus", &interior_focus,
			"focus-line-width", &focus_width,
			"focus-padding", &focus_pad,
			"expander-size", &expander_size,
			"expander-spacing", &expander_spacing,
			NULL);

  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;

  rect->x = widget->allocation.x + border_width;
  rect->y = widget->allocation.y + border_width;

  if (ltr)
    rect->x += expander_spacing;
  else
    rect->x += widget->allocation.width - 2 * border_width -
               expander_spacing - expander_size;

  if (priv->label_widget && GTK_WIDGET_VISIBLE (priv->label_widget))
    {
      GtkAllocation label_allocation;

      label_allocation = priv->label_widget->allocation;

      if (expander_size < label_allocation.height)
	rect->y += focus_width + focus_pad + (label_allocation.height - expander_size) / 2;
      else
	rect->y += expander_spacing;
    }
  else
    {
      rect->y += expander_spacing;
    }

  if (!interior_focus)
    {
      if (ltr)
	rect->x += focus_width + focus_pad;
      else
	rect->x -= focus_width + focus_pad;
      rect->y += focus_width + focus_pad;
    }

  rect->width = rect->height = expander_size;
}

static void
glade_palette_expander_size_allocate (GtkWidget     *widget,
			    GtkAllocation *allocation)
{
  GladePaletteExpander *expander;
  GtkBin *bin;
  GladePaletteExpanderPrivate *priv;
  GtkRequisition child_requisition;
  gboolean child_visible = FALSE;
  gint border_width;
  gint expander_size;
  gint expander_spacing;
  gboolean interior_focus;
  gint focus_width;
  gint focus_pad;
  gint label_height;

  expander = GLADE_PALETTE_EXPANDER (widget);
  bin = GTK_BIN (widget);
  priv = expander->priv;

  border_width = GTK_CONTAINER (widget)->border_width;

  gtk_widget_style_get (widget,
			"interior-focus", &interior_focus,
			"focus-line-width", &focus_width,
			"focus-padding", &focus_pad,
			"expander-size", &expander_size,
			"expander-spacing", &expander_spacing,
			NULL);

  child_requisition.width = 0;
  child_requisition.height = 0;
  if (bin->child && GTK_WIDGET_CHILD_VISIBLE (bin->child))
    {
      child_visible = TRUE;
      gtk_widget_get_child_requisition (bin->child, &child_requisition);
    }

  widget->allocation = *allocation;

  if (priv->label_widget && GTK_WIDGET_VISIBLE (priv->label_widget))
    {
      GtkAllocation label_allocation;
      GtkRequisition label_requisition;
      gboolean ltr;

      gtk_widget_get_child_requisition (priv->label_widget, &label_requisition);

      ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;

      if (ltr)
	label_allocation.x = (widget->allocation.x +
                              border_width + focus_width + focus_pad +
                              expander_size + 2 * expander_spacing);
      else
        label_allocation.x = (widget->allocation.x + widget->allocation.width -
                              (label_requisition.width +
                               border_width + focus_width + focus_pad +
                               expander_size + 2 * expander_spacing));

      label_allocation.y = widget->allocation.y + border_width + focus_width + focus_pad;

      label_allocation.width = MIN (label_requisition.width,
				    allocation->width - 2 * border_width -
				    expander_size - 2 * expander_spacing -
				    2 * focus_width - 2 * focus_pad);
      label_allocation.width = MAX (label_allocation.width, 1);

      label_allocation.height = MIN (label_requisition.height,
				     allocation->height - 2 * border_width -
				     2 * focus_width - 2 * focus_pad -
				     (child_visible ? priv->spacing : 0));
      label_allocation.height = MAX (label_allocation.height, 1);

      gtk_widget_size_allocate (priv->label_widget, &label_allocation);

      label_height = label_allocation.height;
    }
  else
    {
      label_height = 0;
    }

  if (GTK_WIDGET_REALIZED (widget))
    {
      GdkRectangle rect;

      get_expander_bounds (expander, &rect);

      gdk_window_move_resize (priv->event_window,
			      allocation->x + border_width, rect.y,
			      MAX (allocation->width - 2 * border_width, 1), rect.width);
    }

  if (child_visible)
    {
      GtkAllocation child_allocation;
      gint top_height;

      top_height = MAX (2 * expander_spacing + expander_size,
			label_height +
			(interior_focus ? 2 * focus_width + 2 * focus_pad : 0));

      child_allocation.x = widget->allocation.x + border_width;
      child_allocation.y = widget->allocation.y + border_width + top_height + priv->spacing;

      if (!interior_focus)
	child_allocation.y += 2 * focus_width + 2 * focus_pad;

      child_allocation.width = MAX (allocation->width - 2 * border_width, 1);

      child_allocation.height = allocation->height - top_height -
				2 * border_width - priv->spacing -
				(!interior_focus ? 2 * focus_width + 2 * focus_pad : 0);
      child_allocation.height = MAX (child_allocation.height, 1);

      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}

static void
glade_palette_expander_map (GtkWidget *widget)
{
  GladePaletteExpanderPrivate *priv = GLADE_PALETTE_EXPANDER (widget)->priv;

  if (priv->label_widget)
    gtk_widget_map (priv->label_widget);

  GTK_WIDGET_CLASS (parent_class)->map (widget);

  if (priv->event_window)
    gdk_window_show (priv->event_window);
}

static void
glade_palette_expander_unmap (GtkWidget *widget)
{
  GladePaletteExpanderPrivate *priv = GLADE_PALETTE_EXPANDER (widget)->priv;

  if (priv->event_window)
    gdk_window_hide (priv->event_window);

  GTK_WIDGET_CLASS (parent_class)->unmap (widget);

  if (priv->label_widget)
    gtk_widget_unmap (priv->label_widget);
}

static void
glade_palette_expander_paint_flat_box (GladePaletteExpander *expander)
{
  GtkWidget *widget;
  GtkContainer *container;
  GladePaletteExpanderPrivate *priv;
  GdkRectangle area;
  gboolean interior_focus;
  int focus_width;
  int focus_pad;
  int expander_size;
  int expander_spacing;

  priv = expander->priv;
  widget = GTK_WIDGET (expander);
  container = GTK_CONTAINER (expander);

  gtk_widget_style_get (widget,
			"interior-focus", &interior_focus,
			"focus-line-width", &focus_width,
			"focus-padding", &focus_pad,
			"expander-size", &expander_size,
			"expander-spacing", &expander_spacing,
			NULL);

  area.x = widget->allocation.x + container->border_width;
  area.y = widget->allocation.y + container->border_width;
  area.width = widget->allocation.width - (2 * container->border_width);

  if (priv->label_widget && GTK_WIDGET_VISIBLE (priv->label_widget))
    area.height = priv->label_widget->allocation.height;
  else
    area.height = 0;

  area.height += interior_focus ? (focus_width + focus_pad) * 2 : 0;
  area.height = MAX (area.height, expander_size + 2 * expander_spacing);
  area.height += !interior_focus ? (focus_width + focus_pad) * 2 : 0;

  gtk_paint_flat_box (widget->style, widget->window,
		      GTK_STATE_ACTIVE,
		      GTK_SHADOW_ETCHED_OUT,
		      &area, widget, "expander",
		      area.x, area.y,
		      area.width, area.height);
}

static void
glade_palette_expander_paint (GladePaletteExpander *expander)
{
  GtkWidget *widget;
  GdkRectangle clip;
  GtkStateType state;

  widget = GTK_WIDGET (expander);

  get_expander_bounds (expander, &clip);

  state = widget->state;
  if (expander->priv->prelight)
    {
      state = GTK_STATE_PRELIGHT;
    }

  glade_palette_expander_paint_flat_box (expander);

  gtk_paint_expander (widget->style,
		      widget->window,
		      state,
		      &clip,
		      widget,
		      "expander",
		      clip.x + clip.width / 2,
		      clip.y + clip.height / 2,
		      expander->priv->expander_style);
}

static void
glade_palette_expander_paint_focus (GladePaletteExpander  *expander,
			  GdkRectangle *area)
{
  GtkWidget *widget;
  GladePaletteExpanderPrivate *priv;
  gint x, y, width, height;
  gboolean interior_focus;
  gint border_width;
  gint focus_width;
  gint focus_pad;
  gint expander_size;
  gint expander_spacing;
  gboolean ltr;

  widget = GTK_WIDGET (expander);
  priv = expander->priv;

  border_width = GTK_CONTAINER (widget)->border_width;

  gtk_widget_style_get (widget,
			"interior-focus", &interior_focus,
			"focus-line-width", &focus_width,
			"focus-padding", &focus_pad,
			"expander-size", &expander_size,
			"expander-spacing", &expander_spacing,
			NULL);

  ltr = gtk_widget_get_direction (widget) != GTK_TEXT_DIR_RTL;
  
  x = widget->allocation.x + border_width;
  y = widget->allocation.y + border_width;

  if (ltr && interior_focus)
    x += expander_spacing * 2 + expander_size;

  width = height = 0;

  if (priv->label_widget && GTK_WIDGET_VISIBLE (priv->label_widget))
    {
      GtkAllocation label_allocation = priv->label_widget->allocation;

      width  = label_allocation.width;
      height = label_allocation.height;
    }

  if (!interior_focus)
    {
      width += expander_size + 2 * expander_spacing;
      height = MAX (height, expander_size + 2 * expander_spacing);
    }
      
  width  += 2 * focus_pad + 2 * focus_width;
  height += 2 * focus_pad + 2 * focus_width;

  gtk_paint_focus (widget->style, widget->window, GTK_WIDGET_STATE (widget),
		   area, widget, "expander",
		   x, y, width, height);
}

static gboolean
glade_palette_expander_expose (GtkWidget      *widget,
		     GdkEventExpose *event)
{
  if (GTK_WIDGET_DRAWABLE (widget))
    {
      GladePaletteExpander *expander = GLADE_PALETTE_EXPANDER (widget);

      glade_palette_expander_paint (expander);

      if (GTK_WIDGET_HAS_FOCUS (expander))
	glade_palette_expander_paint_focus (expander, &event->area);

      GTK_WIDGET_CLASS (parent_class)->expose_event (widget, event);
    }

  return FALSE;
}

static gboolean
glade_palette_expander_button_press (GtkWidget      *widget,
			   GdkEventButton *event)
{
  GladePaletteExpander *expander = GLADE_PALETTE_EXPANDER (widget);

  if (event->button == 1 && event->window == expander->priv->event_window)
    {
      expander->priv->button_down = TRUE;
      return TRUE;
    }

  return FALSE;
}

static gboolean
glade_palette_expander_button_release (GtkWidget      *widget,
			     GdkEventButton *event)
{
  GladePaletteExpander *expander = GLADE_PALETTE_EXPANDER (widget);

  if (event->button == 1 && expander->priv->button_down)
    {
      gtk_widget_activate (widget);
      expander->priv->button_down = FALSE;
      return TRUE;
    }

  return FALSE;
}

static void
glade_palette_expander_grab_notify (GtkWidget *widget,
			  gboolean   was_grabbed)
{
  if (!was_grabbed)
    GLADE_PALETTE_EXPANDER (widget)->priv->button_down = FALSE;
}

static void
glade_palette_expander_state_changed (GtkWidget    *widget,
			    GtkStateType  previous_state)
{
  if (!GTK_WIDGET_IS_SENSITIVE (widget))
    GLADE_PALETTE_EXPANDER (widget)->priv->button_down = FALSE;
}

static void
glade_palette_expander_redraw_expander (GladePaletteExpander *expander)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (expander);

  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_invalidate_rect (widget->window, &widget->allocation, FALSE);
}

static gboolean
glade_palette_expander_enter_notify (GtkWidget        *widget,
			   GdkEventCrossing *event)
{
  GladePaletteExpander *expander = GLADE_PALETTE_EXPANDER (widget);
  GtkWidget *event_widget;

  event_widget = gtk_get_event_widget ((GdkEvent *) event);

  if (event_widget == widget &&
      event->detail != GDK_NOTIFY_INFERIOR)
    {
      expander->priv->prelight = TRUE;

      if (expander->priv->label_widget)
	gtk_widget_set_state (expander->priv->label_widget, GTK_STATE_PRELIGHT);

      glade_palette_expander_redraw_expander (expander);
    }

  return FALSE;
}

static gboolean
glade_palette_expander_leave_notify (GtkWidget        *widget,
			   GdkEventCrossing *event)
{
  GladePaletteExpander *expander = GLADE_PALETTE_EXPANDER (widget);
  GtkWidget *event_widget;

  event_widget = gtk_get_event_widget ((GdkEvent *) event);

  if (event_widget == widget &&
      event->detail != GDK_NOTIFY_INFERIOR)
    {
      expander->priv->prelight = FALSE;

      if (expander->priv->label_widget)
	gtk_widget_set_state (expander->priv->label_widget, GTK_STATE_NORMAL);

      glade_palette_expander_redraw_expander (expander);
    }

  return FALSE;
}

typedef enum
{
  FOCUS_NONE,
  FOCUS_WIDGET,
  FOCUS_LABEL,
  FOCUS_CHILD
} FocusSite;

static gboolean
focus_current_site (GladePaletteExpander      *expander,
		    GtkDirectionType  direction)
{
  GtkWidget *current_focus;

  current_focus = GTK_CONTAINER (expander)->focus_child;

  if (!current_focus)
    return FALSE;

  return gtk_widget_child_focus (current_focus, direction);
}

static gboolean
focus_in_site (GladePaletteExpander      *expander,
	       FocusSite         site,
	       GtkDirectionType  direction)
{
  switch (site)
    {
    case FOCUS_WIDGET:
      gtk_widget_grab_focus (GTK_WIDGET (expander));
      return TRUE;
    case FOCUS_LABEL:
      if (expander->priv->label_widget)
	return gtk_widget_child_focus (expander->priv->label_widget, direction);
      else
	return FALSE;
    case FOCUS_CHILD:
      {
	GtkWidget *child = gtk_bin_get_child (GTK_BIN (expander));

	if (child && GTK_WIDGET_CHILD_VISIBLE (child))
	  return gtk_widget_child_focus (child, direction);
	else
	  return FALSE;
      }
    case FOCUS_NONE:
      break;
    }

  g_assert_not_reached ();
  return FALSE;
}

static FocusSite
get_next_site (GladePaletteExpander      *expander,
	       FocusSite         site,
	       GtkDirectionType  direction)
{
  gboolean ltr;

  ltr = gtk_widget_get_direction (GTK_WIDGET (expander)) != GTK_TEXT_DIR_RTL;

  switch (site)
    {
    case FOCUS_NONE:
      switch (direction)
	{
	case GTK_DIR_TAB_BACKWARD:
	case GTK_DIR_LEFT:
	case GTK_DIR_UP:
	  return FOCUS_CHILD;
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_DOWN:
	case GTK_DIR_RIGHT:
	  return FOCUS_WIDGET;
	}
    case FOCUS_WIDGET:
      switch (direction)
	{
	case GTK_DIR_TAB_BACKWARD:
	case GTK_DIR_UP:
	  return FOCUS_NONE;
	case GTK_DIR_LEFT:
	  return ltr ? FOCUS_NONE : FOCUS_LABEL;
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_DOWN:
	  return FOCUS_LABEL;
	case GTK_DIR_RIGHT:
	  return ltr ? FOCUS_LABEL : FOCUS_NONE;
	  break;
	}
    case FOCUS_LABEL:
      switch (direction)
	{
	case GTK_DIR_TAB_BACKWARD:
	case GTK_DIR_UP:
	  return FOCUS_WIDGET;
	case GTK_DIR_LEFT:
	  return ltr ? FOCUS_WIDGET : FOCUS_CHILD;
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_DOWN:
	  return FOCUS_CHILD;
	case GTK_DIR_RIGHT:
	  return ltr ? FOCUS_CHILD : FOCUS_WIDGET;
	  break;
	}
    case FOCUS_CHILD:
      switch (direction)
	{
	case GTK_DIR_TAB_BACKWARD:
	case GTK_DIR_LEFT:
	case GTK_DIR_UP:
	  return FOCUS_LABEL;
	case GTK_DIR_TAB_FORWARD:
	case GTK_DIR_DOWN:
	case GTK_DIR_RIGHT:
	  return FOCUS_NONE;
	}
    }

  g_assert_not_reached ();
  return FOCUS_NONE;
}

static gboolean
glade_palette_expander_focus (GtkWidget        *widget,
		    GtkDirectionType  direction)
{
  GladePaletteExpander *expander = GLADE_PALETTE_EXPANDER (widget);
  
  if (!focus_current_site (expander, direction))
    {
      GtkWidget *old_focus_child;
      gboolean widget_is_focus;
      FocusSite site = FOCUS_NONE;
      
      widget_is_focus = gtk_widget_is_focus (widget);
      old_focus_child = GTK_CONTAINER (widget)->focus_child;
      
      if (old_focus_child && old_focus_child == expander->priv->label_widget)
	site = FOCUS_LABEL;
      else if (old_focus_child)
	site = FOCUS_CHILD;
      else if (widget_is_focus)
	site = FOCUS_WIDGET;

      while ((site = get_next_site (expander, site, direction)) != FOCUS_NONE)
	{
	  if (focus_in_site (expander, site, direction))
	    return TRUE;
	}

      return FALSE;
    }

  return TRUE;
}

static void
glade_palette_expander_add (GtkContainer *container,
		  GtkWidget    *widget)
{
  GTK_CONTAINER_CLASS (parent_class)->add (container, widget);

  gtk_widget_set_child_visible (widget, GLADE_PALETTE_EXPANDER (container)->priv->expanded);
  gtk_widget_queue_resize (GTK_WIDGET (container));
}

static void
glade_palette_expander_remove (GtkContainer *container,
		     GtkWidget    *widget)
{
  GladePaletteExpander *expander = GLADE_PALETTE_EXPANDER (container);

  if (GLADE_PALETTE_EXPANDER (expander)->priv->label_widget == widget)
    glade_palette_expander_set_label_widget (expander, NULL);
  else
    GTK_CONTAINER_CLASS (parent_class)->remove (container, widget);
}

static void
glade_palette_expander_forall (GtkContainer *container,
		     gboolean      include_internals,
		     GtkCallback   callback,
		     gpointer      callback_data)
{
  GtkBin *bin = GTK_BIN (container);
  GladePaletteExpanderPrivate *priv = GLADE_PALETTE_EXPANDER (container)->priv;

  if (bin->child)
    (* callback) (bin->child, callback_data);

  if (priv->label_widget)
    (* callback) (priv->label_widget, callback_data);
}

static void
glade_palette_expander_activate (GladePaletteExpander *expander)
{
  glade_palette_expander_set_expanded (expander, !expander->priv->expanded);
}


/**
 * glade_palette_expander_new:
 * @label: the text of the label
 * 
 * Creates a new expander using @label as the text of the label.
 * 
 * Return value: a new #GladePaletteExpander widget.
 *
 * Since: 2.4
 **/
GtkWidget *
glade_palette_expander_new (const gchar *label)
{
  return g_object_new (GLADE_TYPE_PALETTE_EXPANDER, "label", label, NULL);
}

/**
 * glade_palette_expander_new_with_mnemonic:
 * @label: the text of the label with an underscore in front of the
 *         mnemonic character
 * 
 * Creates a new expander using @label as the text of the label.
 * If characters in @label are preceded by an underscore, they are underlined.
 * If you need a literal underscore character in a label, use '__' (two 
 * underscores). The first underlined character represents a keyboard 
 * accelerator called a mnemonic.
 * Pressing Alt and that key activates the button.
 * 
 * Return value: a new #GladePaletteExpander widget.
 *
 * Since: 2.4
 **/
GtkWidget *
glade_palette_expander_new_with_mnemonic (const gchar *label)
{
  return g_object_new (GLADE_TYPE_PALETTE_EXPANDER,
		       "label", label,
		       "use-underline", TRUE,
		       NULL);
}

static gboolean
glade_palette_expander_animation_timeout (GladePaletteExpander *expander)
{
  GladePaletteExpanderPrivate *priv = expander->priv;
  GdkRectangle area;
  gboolean finish = FALSE;

  GDK_THREADS_ENTER();

  if (GTK_WIDGET_REALIZED (expander))
    {
      get_expander_bounds (expander, &area);
      gdk_window_invalidate_rect (GTK_WIDGET (expander)->window, &area, TRUE);
    }

  if (priv->expanded)
    {
      if (priv->expander_style == GTK_EXPANDER_COLLAPSED)
	{
	  priv->expander_style = GTK_EXPANDER_SEMI_EXPANDED;
	}
      else
	{
	  priv->expander_style = GTK_EXPANDER_EXPANDED;
	  finish = TRUE;
	}
    }
  else
    {
      if (priv->expander_style == GTK_EXPANDER_EXPANDED)
	{
	  priv->expander_style = GTK_EXPANDER_SEMI_COLLAPSED;
	}
      else
	{
	  priv->expander_style = GTK_EXPANDER_COLLAPSED;
	  finish = TRUE;
	}
    }

  if (finish)
    {
      priv->animation_timeout = 0;
      if (GTK_BIN (expander)->child)
	gtk_widget_set_child_visible (GTK_BIN (expander)->child, priv->expanded);
      gtk_widget_queue_resize (GTK_WIDGET (expander));
    }

  GDK_THREADS_LEAVE();

  return !finish;
}

static void
glade_palette_expander_start_animation (GladePaletteExpander *expander)
{
  GladePaletteExpanderPrivate *priv = expander->priv;

  if (priv->animation_timeout)
    g_source_remove (priv->animation_timeout);

  priv->animation_timeout =
		g_timeout_add (50,
			       (GSourceFunc) glade_palette_expander_animation_timeout,
			       expander);
}

/**
 * glade_palette_expander_set_expanded:
 * @expander: a #GladePaletteExpander
 * @expanded: whether the child widget is revealed
 *
 * Sets the state of the expander. Set to %TRUE, if you want
 * the child widget to be revealed, and %FALSE if you want the
 * child widget to be hidden.
 *
 * Since: 2.4
 **/
void
glade_palette_expander_set_expanded (GladePaletteExpander *expander,
			   gboolean     expanded)
{
  GladePaletteExpanderPrivate *priv;

  g_return_if_fail (GLADE_IS_PALETTE_EXPANDER (expander));

  priv = expander->priv;

  expanded = expanded != FALSE;

  if (priv->expanded != expanded)
    {
      priv->expanded = expanded;

      if (GTK_WIDGET_REALIZED (expander))
	{
	   glade_palette_expander_start_animation (expander);
           
	}
      else 
	{
	  priv->expander_style = expanded ? GTK_EXPANDER_EXPANDED :
					    GTK_EXPANDER_COLLAPSED;

	  if (GTK_BIN (expander)->child)
	    {
	      gtk_widget_set_child_visible (GTK_BIN (expander)->child, priv->expanded);
	      gtk_widget_queue_resize (GTK_WIDGET (expander));
	    }
	}

      g_object_notify (G_OBJECT (expander), "expanded");
    }
}

/**
 * glade_palette_expander_get_expanded:
 * @expander:a #GladePaletteExpander
 *
 * Queries a #GladePaletteExpander and returns its current state. Returns %TRUE
 * if the child widget is revealed.
 *
 * See glade_palette_expander_set_expanded().
 *
 * Return value: the current state of the expander.
 *
 * Since: 2.4
 **/
gboolean
glade_palette_expander_get_expanded (GladePaletteExpander *expander)
{
  g_return_val_if_fail (GLADE_IS_PALETTE_EXPANDER (expander), FALSE);

  return expander->priv->expanded;
}

/**
 * glade_palette_expander_set_spacing:
 * @expander: a #GladePaletteExpander
 * @spacing: distance between the expander and child in pixels.
 *
 * Sets the spacing field of @expander, which is the number of pixels to
 * place between expander and the child.
 *
 * Since: 2.4
 **/
void
glade_palette_expander_set_spacing (GladePaletteExpander *expander,
			  gint         spacing)
{
  g_return_if_fail (GLADE_IS_PALETTE_EXPANDER (expander));
  g_return_if_fail (spacing >= 0);

  if (expander->priv->spacing != spacing)
    {
      expander->priv->spacing = spacing;

      gtk_widget_queue_resize (GTK_WIDGET (expander));

      g_object_notify (G_OBJECT (expander), "spacing");
    }
}

/**
 * glade_palette_expander_get_spacing:
 * @expander: a #GladePaletteExpander
 *
 * Gets the value set by glade_palette_expander_set_spacing().
 *
 * Return value: spacing between the expander and child.
 *
 * Since: 2.4
 **/
gint
glade_palette_expander_get_spacing (GladePaletteExpander *expander)
{
  g_return_val_if_fail (GLADE_IS_PALETTE_EXPANDER (expander), 0);

  return expander->priv->spacing;
}

/**
 * glade_palette_expander_set_label:
 * @expander: a #GladePaletteExpander
 * @label: a string
 *
 * Sets the text of the label of the expander to @label.
 *
 * This will also clear any previously set labels.
 *
 * Since: 2.4
 **/
void
glade_palette_expander_set_label (GladePaletteExpander *expander,
			const gchar *label)
{
  g_return_if_fail (GLADE_IS_PALETTE_EXPANDER (expander));

  if (!label)
    {
      glade_palette_expander_set_label_widget (expander, NULL);
    }
  else
    {
      GtkWidget *child;

      child = gtk_label_new (label);
      gtk_label_set_use_underline (GTK_LABEL (child), expander->priv->use_underline);
      gtk_label_set_use_markup (GTK_LABEL (child), expander->priv->use_markup);
      gtk_widget_show (child);

      glade_palette_expander_set_label_widget (expander, child);
    }

  g_object_notify (G_OBJECT (expander), "label");
}

/**
 * glade_palette_expander_get_label:
 * @expander: a #GladePaletteExpander
 *
 * Fetches the text from the label of the expander, as set by
 * glade_palette_expander_set_label(). If the label text has not
 * been set the return value will be %NULL. This will be the
 * case if you create an empty button with gtk_button_new() to
 * use as a container.
 *
 * Return value: The text of the label widget. This string is owned
 * by the widget and must not be modified or freed.
 *
 * Since: 2.4
 **/
G_CONST_RETURN char *
glade_palette_expander_get_label (GladePaletteExpander *expander)
{
  GladePaletteExpanderPrivate *priv;

  g_return_val_if_fail (GLADE_IS_PALETTE_EXPANDER (expander), NULL);

  priv = expander->priv;

  if (priv->label_widget && GTK_IS_LABEL (priv->label_widget))
    return gtk_label_get_text (GTK_LABEL (priv->label_widget));
  else
    return NULL;
}

/**
 * glade_palette_expander_set_use_underline:
 * @expander: a #GladePaletteExpander
 * @use_underline: %TRUE if underlines in the text indicate mnemonics
 *
 * If true, an underline in the text of the expander label indicates
 * the next character should be used for the mnemonic accelerator key.
 *
 * Since: 2.4
 **/
void
glade_palette_expander_set_use_underline (GladePaletteExpander *expander,
				gboolean     use_underline)
{
  GladePaletteExpanderPrivate *priv;

  g_return_if_fail (GLADE_IS_PALETTE_EXPANDER (expander));

  priv = expander->priv;

  use_underline = use_underline != FALSE;

  if (priv->use_underline != use_underline)
    {
      priv->use_underline = use_underline;

      if (priv->label_widget && GTK_IS_LABEL (priv->label_widget))
	gtk_label_set_use_underline (GTK_LABEL (priv->label_widget), use_underline);

      g_object_notify (G_OBJECT (expander), "use-underline");
    }
}

/**
 * glade_palette_expander_get_use_underline:
 * @expander: a #GladePaletteExpander
 *
 * Returns whether an embedded underline in the expander label indicates a
 * mnemonic. See glade_palette_expander_set_use_underline().
 *
 * Return value: %TRUE if an embedded underline in the expander label
 *               indicates the mnemonic accelerator keys.
 *
 * Since: 2.4
 **/
gboolean
glade_palette_expander_get_use_underline (GladePaletteExpander *expander)
{
  g_return_val_if_fail (GLADE_IS_PALETTE_EXPANDER (expander), FALSE);

  return expander->priv->use_underline;
}

/**
 * glade_palette_expander_set_use_markup:
 * @expander: a #GladePaletteExpander
 * @use_markup: %TRUE if the label's text should be parsed for markup
 *
 * Sets whether the text of the label contains markup in <link
 * linkend="PangoMarkupFormat">Pango's text markup
 * language</link>. See gtk_label_set_markup().
 *
 * Since: 2.4
 **/
void
glade_palette_expander_set_use_markup (GladePaletteExpander *expander,
			     gboolean     use_markup)
{
  GladePaletteExpanderPrivate *priv;

  g_return_if_fail (GLADE_IS_PALETTE_EXPANDER (expander));

  priv = expander->priv;

  use_markup = use_markup != FALSE;

  if (priv->use_markup != use_markup)
    {
      priv->use_markup = use_markup;

      if (priv->label_widget && GTK_IS_LABEL (priv->label_widget))
	gtk_label_set_use_markup (GTK_LABEL (priv->label_widget), use_markup);

      g_object_notify (G_OBJECT (expander), "use-markup");
    }
}

/**
 * glade_palette_expander_get_use_markup:
 * @expander: a #GladePaletteExpander
 *
 * Returns whether the label's text is interpreted as marked up with
 * the <link linkend="PangoMarkupFormat">Pango text markup
 * language</link>. See glade_palette_expander_set_use_markup ().
 *
 * Return value: %TRUE if the label's text will be parsed for markup
 *
 * Since: 2.4
 **/
gboolean
glade_palette_expander_get_use_markup (GladePaletteExpander *expander)
{
  g_return_val_if_fail (GLADE_IS_PALETTE_EXPANDER (expander), FALSE);

  return expander->priv->use_markup;
}

/**
 * glade_palette_expander_set_label_widget:
 * @expander: a #GladePaletteExpander
 * @label_widget: the new label widget
 *
 * Set the label widget for the expander. This is the widget
 * that will appear embedded alongside the expander arrow.
 *
 * Since: 2.4
 **/
void
glade_palette_expander_set_label_widget (GladePaletteExpander *expander,
			       GtkWidget   *label_widget)
{
  GladePaletteExpanderPrivate *priv;

  g_return_if_fail (GLADE_IS_PALETTE_EXPANDER (expander));
  g_return_if_fail (label_widget == NULL || GTK_IS_WIDGET (label_widget));
  g_return_if_fail (label_widget == NULL || label_widget->parent == NULL);

  priv = expander->priv;

  if (priv->label_widget == label_widget)
    return;

  if (priv->label_widget)
    {
      gtk_widget_set_state (priv->label_widget, GTK_STATE_NORMAL);
      gtk_widget_unparent (priv->label_widget);
    }

  priv->label_widget = label_widget;

  if (label_widget)
    {
      priv->label_widget = label_widget;

      gtk_widget_set_parent (label_widget, GTK_WIDGET (expander));

      if (priv->prelight)
	gtk_widget_set_state (label_widget, GTK_STATE_PRELIGHT);
    }

  if (GTK_WIDGET_VISIBLE (expander))
    gtk_widget_queue_resize (GTK_WIDGET (expander));

  g_object_freeze_notify (G_OBJECT (expander));
  g_object_notify (G_OBJECT (expander), "label-widget");
  g_object_notify (G_OBJECT (expander), "label");
  g_object_thaw_notify (G_OBJECT (expander));
}

/**
 * glade_palette_expander_get_label_widget:
 * @expander: a #GladePaletteExpander
 *
 * Retrieves the label widget for the frame. See
 * glade_palette_expander_set_label_widget().
 *
 * Return value: the label widget, or %NULL if there is none.
 * 
 * Since: 2.4
 **/
GtkWidget *
glade_palette_expander_get_label_widget (GladePaletteExpander *expander)
{
  g_return_val_if_fail (GLADE_IS_PALETTE_EXPANDER (expander), NULL);

  return expander->priv->label_widget;
}
