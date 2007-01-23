/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003, 2004 Joaquin Cuenca Abela
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
 * Authors:
 *   Joaquin Cuenca Abela <e98cuenc@yahoo.com>
 */

#include "config.h"

#include <gtk/gtk.h>
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

static void glade_placeholder_class_init     (GladePlaceholderClass   *klass);
static void glade_placeholder_init           (GladePlaceholder        *placeholder);
static void glade_placeholder_finalize       (GObject                 *object);
static void glade_placeholder_realize        (GtkWidget               *widget);
static void glade_placeholder_size_allocate  (GtkWidget               *widget,
					      GtkAllocation           *allocation);
static void glade_placeholder_send_configure (GladePlaceholder        *placeholder);
static gboolean glade_placeholder_expose     (GtkWidget               *widget,
					      GdkEventExpose          *event);
static gboolean glade_placeholder_motion_notify_event (GtkWidget      *widget,
						       GdkEventMotion *event);
static gboolean glade_placeholder_button_press (GtkWidget             *widget,
						GdkEventButton        *event);
static gboolean glade_placeholder_popup_menu (GtkWidget               *widget);


static GtkWidgetClass *parent_class = NULL;

static char *placeholder_xpm[] = {
	/* columns rows colors chars-per-pixel */
	"8 8 2 1",
	"  c #bbbbbb",
	". c #d6d6d6",
	/* pixels */
	" .  .   ",
	".    .  ",
	"      ..",
	"      ..",
	".    .  ",
	" .  .   ",
	"  ..    ",
	"  ..    "
};

/**
 * glade_placeholder_get_type:
 *
 * Creates the typecode for the #GladePlaceholder object type.
 *
 * Returns: the typecode for the #GladePlaceholder object type
 */
GType
glade_placeholder_get_type (void)
{
	static GType placeholder_type = 0;

	if (!placeholder_type)
	{
		static const GTypeInfo placeholder_info =
		{
			sizeof (GladePlaceholderClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) glade_placeholder_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (GladePlaceholder),
			0,		/* n_preallocs */
			(GInstanceInitFunc) glade_placeholder_init,
		};

		placeholder_type = g_type_register_static (GTK_TYPE_WIDGET, "GladePlaceholder",
							   &placeholder_info, 0);
	}

	return placeholder_type;
}

static void
glade_placeholder_class_init (GladePlaceholderClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = glade_placeholder_finalize;
	widget_class->realize = glade_placeholder_realize;
	widget_class->size_allocate = glade_placeholder_size_allocate;
	widget_class->expose_event = glade_placeholder_expose;
	widget_class->motion_notify_event = glade_placeholder_motion_notify_event;
	widget_class->button_press_event = glade_placeholder_button_press;
	widget_class->popup_menu = glade_placeholder_popup_menu;
}

static void
glade_placeholder_init (GladePlaceholder *placeholder)
{
	placeholder->placeholder_pixmap = NULL;

	GTK_WIDGET_SET_FLAGS (GTK_WIDGET (placeholder), GTK_CAN_FOCUS);

	gtk_widget_set_size_request (GTK_WIDGET (placeholder),
				     GLADE_PLACEHOLDER_WIDTH_REQ,
				     GLADE_PLACEHOLDER_HEIGHT_REQ);

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

	/* placeholder->placeholder_pixmap can be NULL if the placeholder is 
         * destroyed before it's realized */
	if (placeholder->placeholder_pixmap)
		g_object_unref (placeholder->placeholder_pixmap);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
glade_placeholder_realize (GtkWidget *widget)
{
	GladePlaceholder *placeholder;
	GdkWindowAttr attributes;
	gint attributes_mask;

	g_return_if_fail (GLADE_IS_PLACEHOLDER (widget));

	placeholder = GLADE_PLACEHOLDER (widget);

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = gtk_widget_get_events (widget) |
				GDK_EXPOSURE_MASK              |
				GDK_BUTTON_PRESS_MASK          |
		                GDK_BUTTON_RELEASE_MASK        |
		                GDK_POINTER_MOTION_MASK;

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
	gdk_window_set_user_data (widget->window, placeholder);

	widget->style = gtk_style_attach (widget->style, widget->window);

	glade_placeholder_send_configure (GLADE_PLACEHOLDER (widget));

	if (!placeholder->placeholder_pixmap)
	{
		placeholder->placeholder_pixmap = gdk_pixmap_colormap_create_from_xpm_d (NULL,
									    gtk_widget_get_colormap (GTK_WIDGET (placeholder)),
									    NULL, NULL, placeholder_xpm);
		g_assert(G_IS_OBJECT(placeholder->placeholder_pixmap));
	}

	gdk_window_set_back_pixmap (GTK_WIDGET (placeholder)->window, placeholder->placeholder_pixmap, FALSE);
}

static void
glade_placeholder_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	g_return_if_fail (GLADE_IS_PLACEHOLDER (widget));
	g_return_if_fail (allocation != NULL);

	widget->allocation = *allocation;

	if (GTK_WIDGET_REALIZED (widget))
	{
		gdk_window_move_resize (widget->window,
					allocation->x, allocation->y,
					allocation->width, allocation->height);

		glade_placeholder_send_configure (GLADE_PLACEHOLDER (widget));
	}
}

static void
glade_placeholder_send_configure (GladePlaceholder *placeholder)
{
	GtkWidget *widget;
	GdkEvent *event = gdk_event_new (GDK_CONFIGURE);

	widget = GTK_WIDGET (placeholder);

	event->configure.window = g_object_ref (widget->window);
	event->configure.send_event = TRUE;
	event->configure.x = widget->allocation.x;
	event->configure.y = widget->allocation.y;
	event->configure.width = widget->allocation.width;
	event->configure.height = widget->allocation.height;

	gtk_widget_event (widget, event);
	gdk_event_free (event);
}

static GladeProject*
glade_placeholder_get_project (GladePlaceholder *placeholder)
{
	GladeWidget *parent;
	parent = glade_placeholder_get_parent (placeholder);
	return parent ? GLADE_PROJECT (parent->project) : NULL;
}

static gboolean
glade_placeholder_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GdkGC *light_gc;
	GdkGC *dark_gc;
	gint w, h;

	g_return_val_if_fail (GLADE_IS_PLACEHOLDER (widget), FALSE);
	
	light_gc = widget->style->light_gc[GTK_STATE_NORMAL];
	dark_gc  = widget->style->dark_gc[GTK_STATE_NORMAL];
	gdk_drawable_get_size (event->window, &w, &h);

	gdk_draw_line (event->window, light_gc, 0, 0, w - 1, 0);
	gdk_draw_line (event->window, light_gc, 0, 0, 0, h - 1);
	gdk_draw_line (event->window, dark_gc, 0, h - 1, w - 1, h - 1);
	gdk_draw_line (event->window, dark_gc, w - 1, 0, w - 1, h - 1);

	glade_util_queue_draw_nodes (event->window);

	return FALSE;
}

static gboolean                                                                 
glade_placeholder_motion_notify_event (GtkWidget *widget, GdkEventMotion *event)
{
	GladeWidgetAdaptor *adaptor;
	GladeWidget        *gparent;

	g_return_val_if_fail (GLADE_IS_PLACEHOLDER (widget), FALSE);

	gparent = glade_placeholder_get_parent (GLADE_PLACEHOLDER (widget));
	adaptor = glade_palette_get_current_item (glade_app_get_palette ());

	if (adaptor == NULL && 
	    /* If we are the child of a widget that is in a GladeFixed, then
	     * we are the means of drag/resize and we dont want to fight for
	     * the cursor (ideally; GladeCursor should somehow deal with such
	     * concurrencies I suppose).
	     */
	    (gparent->parent && 
	     GLADE_IS_FIXED (gparent->parent)) == FALSE)
                glade_cursor_set (event->window, GLADE_CURSOR_SELECTOR);
	else if (adaptor)
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

	if (!GTK_WIDGET_HAS_FOCUS (widget))
		gtk_widget_grab_focus (widget);

	if (event->button == 1 && event->type == GDK_BUTTON_PRESS)
	{
		if (adaptor != NULL)
		{
			/* A widget type is selected in the palette.
			 * Add a new widget of that type.
			 */
			glade_command_create
				(adaptor, 
				 glade_placeholder_get_parent (placeholder),
				 placeholder, project);

			glade_palette_deselect_current_item (glade_app_get_palette(), TRUE);

			/* reset the cursor */
			glade_cursor_set (event->window, GLADE_CURSOR_SELECTOR);

			handled = TRUE;
		}
	}
	else if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
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
