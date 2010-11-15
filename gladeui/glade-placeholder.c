/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003, 2004 Joaquin Cuenca Abela
 *
 * Authors:
 *   Joaquin Cuenca Abela <e98cuenc@yahoo.com> 
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
*/

#include "config.h"

/**
 * SECTION:glade-placeholder
 * @Short_Description: A #GtkWidget to fill empty places.
 *
 * Generally in Glade, container widgets are implemented with #GladePlaceholder 
 * children to allow users to 'click' and add thier widgets to a container. 
 * It is the responsability of the plugin writer to create placeholders for 
 * container widgets where appropriate; usually in #GladePostCreateFunc 
 * when the #GladeCreateReason is %GLADE_CREATE_USER.
 */

#include <gtk/gtk.h>
#include "glade-marshallers.h"
#include "glade.h"
#include "glade-placeholder.h"
#include "glade-xml-utils.h"
#include "glade-project.h"
#include "glade-command.h"
#include "glade-palette.h"
#include "glade-popup.h"
#include "glade-cursor.h"
#include "glade-widget.h"
#include "glade-app.h"
#include <math.h>

#define WIDTH_REQUISITION    20
#define HEIGHT_REQUISITION   20

static void      glade_placeholder_finalize       (GObject           *object);

static void      glade_placeholder_realize        (GtkWidget         *widget);

static void      glade_placeholder_size_allocate  (GtkWidget         *widget,
					           GtkAllocation     *allocation);
					      
static void      glade_placeholder_send_configure (GladePlaceholder  *placeholder);

static gboolean  glade_placeholder_draw           (GtkWidget         *widget,
					           cairo_t *cr);
					      
static gboolean  glade_placeholder_motion_notify_event (GtkWidget      *widget,
						        GdkEventMotion *event);
						       
static gboolean  glade_placeholder_button_press        (GtkWidget      *widget,
						        GdkEventButton *event);
						
static gboolean  glade_placeholder_popup_menu          (GtkWidget      *widget);

G_DEFINE_TYPE (GladePlaceholder, glade_placeholder, GTK_TYPE_WIDGET)

static void
glade_placeholder_class_init (GladePlaceholderClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = glade_placeholder_finalize;
	widget_class->realize = glade_placeholder_realize;
	widget_class->size_allocate = glade_placeholder_size_allocate;
	widget_class->draw = glade_placeholder_draw;
	widget_class->motion_notify_event = glade_placeholder_motion_notify_event;
	widget_class->button_press_event = glade_placeholder_button_press;
	widget_class->popup_menu = glade_placeholder_popup_menu;
}

static void
glade_placeholder_notify_parent (GObject    *gobject,
				 GParamSpec *arg1,
				 gpointer    user_data)
{
	GladePlaceholder *placeholder = GLADE_PLACEHOLDER (gobject);
	GladeWidget *parent = glade_placeholder_get_parent (placeholder);
	
	if (placeholder->packing_actions)
	{
		g_list_foreach (placeholder->packing_actions, (GFunc)g_object_unref, NULL);
		g_list_free (placeholder->packing_actions);
		placeholder->packing_actions = NULL;
	}
	
	if (parent && parent->adaptor->packing_actions)
		placeholder->packing_actions = glade_widget_adaptor_pack_actions_new (parent->adaptor);	
}

static void
glade_placeholder_init (GladePlaceholder *placeholder)
{
	placeholder->packing_actions = NULL;

	gtk_widget_set_can_focus (GTK_WIDGET (placeholder), TRUE);

	gtk_widget_set_size_request (GTK_WIDGET (placeholder),
				     WIDTH_REQUISITION,
				     HEIGHT_REQUISITION);

	g_signal_connect (placeholder, "notify::parent",
			  G_CALLBACK (glade_placeholder_notify_parent),
			  NULL);
	
	gtk_widget_show (GTK_WIDGET (placeholder));
}

/**
 * glade_placeholder_new:
 * 
 * Returns: a new #GladePlaceholder cast as a #GtkWidget
 */
GtkWidget *
glade_placeholder_new (void)
{
	return g_object_new (GLADE_TYPE_PLACEHOLDER, NULL);
}

static void
glade_placeholder_finalize (GObject *object)
{
	GladePlaceholder *placeholder;

	g_return_if_fail (GLADE_IS_PLACEHOLDER (object));
	placeholder = GLADE_PLACEHOLDER (object);

	if (placeholder->packing_actions)
	{
		g_list_foreach (placeholder->packing_actions, (GFunc)g_object_unref, NULL);
		g_list_free (placeholder->packing_actions);
	}
	
	G_OBJECT_CLASS (glade_placeholder_parent_class)->finalize (object);
}

static void
glade_placeholder_realize (GtkWidget *widget)
{
	GladePlaceholder *placeholder;
	GtkAllocation allocation;
	GdkWindow *window;
	GdkWindowAttr attributes;
	gint attributes_mask;

	g_return_if_fail (GLADE_IS_PLACEHOLDER (widget));

	placeholder = GLADE_PLACEHOLDER (widget);

	gtk_widget_set_realized (widget, TRUE);

	attributes.window_type = GDK_WINDOW_CHILD;
	gtk_widget_get_allocation (widget, &allocation);
	attributes.x = allocation.x;
	attributes.y = allocation.y;
	attributes.width = allocation.width;
	attributes.height = allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.event_mask = 
		gtk_widget_get_events (widget) |
		GDK_EXPOSURE_MASK              |
		GDK_BUTTON_PRESS_MASK          |
		GDK_BUTTON_RELEASE_MASK        |
		GDK_POINTER_MOTION_MASK;

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

	window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
	gtk_widget_set_window (widget, window);
	gdk_window_set_user_data (window, placeholder);

	gtk_widget_style_attach (widget);

	glade_placeholder_send_configure (GLADE_PLACEHOLDER (widget));
}

static void
glade_placeholder_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	g_return_if_fail (GLADE_IS_PLACEHOLDER (widget));
	g_return_if_fail (allocation != NULL);

	gtk_widget_set_allocation (widget, allocation);

	if (gtk_widget_get_realized (widget))
	{
		gdk_window_move_resize (gtk_widget_get_window (widget),
					allocation->x, allocation->y,
					allocation->width, allocation->height);

		glade_placeholder_send_configure (GLADE_PLACEHOLDER (widget));
	}
}

static void
glade_placeholder_send_configure (GladePlaceholder *placeholder)
{
	GtkWidget *widget;
	GtkAllocation allocation;
	GdkEvent *event = gdk_event_new (GDK_CONFIGURE);

	widget = GTK_WIDGET (placeholder);

	event->configure.window = g_object_ref (gtk_widget_get_window (widget));
	event->configure.send_event = TRUE;
	gtk_widget_get_allocation (widget, &allocation);
	event->configure.x = allocation.x;
	event->configure.y = allocation.y;
	event->configure.width = allocation.width;
	event->configure.height = allocation.height;

	gtk_widget_event (widget, event);
	gdk_event_free (event);
}

GladeProject*
glade_placeholder_get_project (GladePlaceholder *placeholder)
{
	GladeWidget *parent;
	parent = glade_placeholder_get_parent (placeholder);
	return parent ? GLADE_PROJECT (parent->project) : NULL;
}

static void
glade_placeholder_draw_background (GtkWidget* widget, cairo_t *cr)
{
	cairo_surface_t *surface;
	cairo_t *cr2;
	const gint width = 10;
	const gint height = 10;

	surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, width, height);
	cr2 = cairo_create (surface);
	cairo_surface_destroy (surface);

	cairo_set_source_rgb (cr2, 0.75, 0.75, 0.75); /* light gray */
	cairo_paint (cr2);

	cairo_set_source_rgb (cr2, 0.5, 0.5, 0.5); /* dark gray */
	cairo_rectangle (cr2, width / 2,  0, width / 2, height / 2);
	cairo_rectangle (cr2, 0, height / 2, width / 2, height / 2);
	cairo_fill (cr2);

	surface = cairo_surface_reference (cairo_get_target (cr2));
	cairo_destroy (cr2);

	cairo_save (cr);
	cairo_set_source_surface (cr, surface, 0, 0);
	cairo_surface_destroy (surface);
	cairo_pattern_set_filter (cairo_get_source (cr), CAIRO_FILTER_NEAREST);
	cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_REPEAT);
	cairo_paint (cr);
}

static gboolean
glade_placeholder_draw (GtkWidget *widget, cairo_t *cr)
{
	GtkStyle *style;
	GdkColor *light;
	GdkColor *dark;
	gint      w, h;

	g_return_val_if_fail (GLADE_IS_PLACEHOLDER (widget), FALSE);

	style = gtk_widget_get_style (widget);
	light = &style->light[GTK_STATE_NORMAL];
	dark  = &style->dark[GTK_STATE_NORMAL];

	h = gtk_widget_get_allocated_height (widget);
	w = gtk_widget_get_allocated_width (widget);

	glade_placeholder_draw_background (widget, cr);
	
	cairo_set_line_width (cr, 1.0);

	glade_utils_cairo_draw_line (cr, light, 0, 0, w - 1, 0);
	glade_utils_cairo_draw_line (cr, light, 0, 0, 0, h - 1);
	glade_utils_cairo_draw_line (cr, dark, 0, h - 1, w - 1, h - 1);
	glade_utils_cairo_draw_line (cr, dark, w - 1, 0, w - 1, h - 1);

	glade_util_draw_selection_nodes (widget, cr);	

	return FALSE;
}

static gboolean                                                                 
glade_placeholder_motion_notify_event (GtkWidget *widget, GdkEventMotion *event)
{
	GladePointerMode    pointer_mode;
	GladeWidget        *gparent;

	g_return_val_if_fail (GLADE_IS_PLACEHOLDER (widget), FALSE);

	gparent = glade_placeholder_get_parent (GLADE_PLACEHOLDER (widget));
	pointer_mode = glade_app_get_pointer_mode ();

	if (pointer_mode == GLADE_POINTER_SELECT && 
	    /* If we are the child of a widget that is in a GladeFixed, then
	     * we are the means of drag/resize and we dont want to fight for
	     * the cursor (ideally; GladeCursor should somehow deal with such
	     * concurrencies I suppose).
	     */
	    (gparent->parent && 
	     GLADE_IS_FIXED (gparent->parent)) == FALSE) 
                glade_cursor_set (event->window, GLADE_CURSOR_SELECTOR);
	else if (pointer_mode == GLADE_POINTER_ADD_WIDGET)
                glade_cursor_set (event->window, GLADE_CURSOR_ADD_WIDGET);

	return FALSE;
}

static gboolean
glade_placeholder_button_press (GtkWidget *widget, GdkEventButton *event)
{
	GladePlaceholder   *placeholder;
	GladeProject       *project;
	GladeWidgetAdaptor *adaptor;
	GladePalette       *palette;
	gboolean            handled = FALSE;

	g_return_val_if_fail (GLADE_IS_PLACEHOLDER (widget), FALSE);

	adaptor = glade_palette_get_current_item (glade_app_get_palette ());

	palette = glade_app_get_palette ();
	placeholder = GLADE_PLACEHOLDER (widget);
	project = glade_placeholder_get_project (placeholder);

	if (!gtk_widget_has_focus (widget))
		gtk_widget_grab_focus (widget);

	if (event->button == 1 && event->type == GDK_BUTTON_PRESS)
	{
		if (adaptor != NULL)
		{
			GladeWidget *parent = glade_placeholder_get_parent (placeholder);

			if (!glade_util_check_and_warn_scrollable (parent, adaptor, glade_app_get_window()))
			{
				/* A widget type is selected in the palette.
				 * Add a new widget of that type.
				 */
				glade_command_create (adaptor, parent, placeholder, project);
				
				glade_palette_deselect_current_item (glade_app_get_palette(), TRUE);
				
				/* reset the cursor */
				glade_cursor_set (event->window, GLADE_CURSOR_SELECTOR);
			}
			handled = TRUE;
		}
	}

	if (!handled && glade_popup_is_popup_event (event))
	{
		glade_popup_placeholder_pop (placeholder, event);
		handled = TRUE;
	}

	return handled;
}

static gboolean
glade_placeholder_popup_menu (GtkWidget *widget)
{
	g_return_val_if_fail (GLADE_IS_PLACEHOLDER (widget), FALSE);

	glade_popup_placeholder_pop (GLADE_PLACEHOLDER (widget), NULL);

	return TRUE;
}

GladeWidget *
glade_placeholder_get_parent (GladePlaceholder *placeholder)
{
	GtkWidget   *widget;
	GladeWidget *parent = NULL;

	g_return_val_if_fail (GLADE_IS_PLACEHOLDER (placeholder), NULL);

	for (widget  = gtk_widget_get_parent (GTK_WIDGET (placeholder));
	     widget != NULL;
	     widget = gtk_widget_get_parent (widget))
	{
		if ((parent = glade_widget_get_from_gobject (widget)) != NULL)
			break;
	}
	return parent;
}
