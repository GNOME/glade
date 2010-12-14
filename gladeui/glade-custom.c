/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-custom.c - An emulation of a custom widget used to
 *                  support the glade-2 style custom widget.
 *
 * Copyright (C) 2005 The GNOME Foundation.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include "glade-custom.h"

enum
{
	PROP_0,
	PROP_CREATION,
	PROP_STR1,
	PROP_STR2,
	PROP_INT1,
	PROP_INT2
};

static GtkWidgetClass *parent_class = NULL;

/* Ignore properties.
 */
static void
glade_custom_set_property (GObject      *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
}

static void
glade_custom_get_property (GObject    *object,
			   guint       prop_id,
			   GValue     *value,
			   GParamSpec *pspec)
{
}


static void
glade_custom_finalize (GObject *object)
{
	GladeCustom *custom;

	g_return_if_fail (GLADE_IS_CUSTOM (object));
	custom = GLADE_CUSTOM (object);

	/* custom->custom_pixmap can be NULL if the custom
	 * widget is destroyed before it's realized */
	if (custom->custom_pixmap)
		g_object_unref (custom->custom_pixmap);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
glade_custom_send_configure (GladeCustom *custom)
{
	GtkWidget *widget;
	GtkAllocation allocation;
	GdkEvent *event = gdk_event_new (GDK_CONFIGURE);

	widget = GTK_WIDGET (custom);

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

static void
glade_custom_realize (GtkWidget *widget)
{
	GladeCustom *custom;
	GtkAllocation allocation;
	GdkWindow *window;
	GdkWindowAttr attributes;
	gint attributes_mask;

	g_return_if_fail (GLADE_IS_CUSTOM (widget));

	custom = GLADE_CUSTOM (widget);

	gtk_widget_set_realized (widget, TRUE);

	attributes.window_type = GDK_WINDOW_CHILD;
	gtk_widget_get_allocation (widget, &allocation);
	attributes.x = allocation.x;
	attributes.y = allocation.y;
	attributes.width = allocation.width;
	attributes.height = allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual (widget);
	attributes.colormap = gtk_widget_get_colormap (widget);
	attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;

	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);
	gtk_widget_set_window (widget, window);
	gdk_window_set_user_data (window, custom);

	gtk_widget_style_attach (widget);

	glade_custom_send_configure (custom);

	if (!custom->custom_pixmap)
	{
		custom->custom_pixmap = 
			gdk_pixmap_colormap_create_from_xpm_d 
			(NULL, gtk_widget_get_colormap (GTK_WIDGET (custom)),
			 NULL, NULL, custom_xpm);

		g_assert(G_IS_OBJECT(custom->custom_pixmap));
	}
	gdk_window_set_back_pixmap (gtk_widget_get_window (GTK_WIDGET (custom)), custom->custom_pixmap, FALSE);
}

static void
glade_custom_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	g_return_if_fail (GLADE_IS_CUSTOM (widget));
	g_return_if_fail (allocation != NULL);

	gtk_widget_set_allocation (widget, allocation);

	if (gtk_widget_get_realized (widget))
	{
		gdk_window_move_resize (gtk_widget_get_window (widget),
					allocation->x, allocation->y,
					allocation->width, allocation->height);

		glade_custom_send_configure (GLADE_CUSTOM (widget));
	}
}

static gboolean
glade_custom_expose (GtkWidget *widget, GdkEventExpose *event)
{
	GtkStyle *style;
	GdkGC *light_gc;
	GdkGC *dark_gc;
	gint w, h;

	g_return_val_if_fail (GLADE_IS_CUSTOM (widget), FALSE);

	style = gtk_widget_get_style (widget);
	light_gc = style->light_gc[GTK_STATE_NORMAL];
	dark_gc  = style->dark_gc[GTK_STATE_NORMAL];
	gdk_drawable_get_size (event->window, &w, &h);

	gdk_draw_line (event->window, light_gc, 0, 0, w - 1, 0);
	gdk_draw_line (event->window, light_gc, 0, 0, 0, h - 1);
	gdk_draw_line (event->window, dark_gc, 0, h - 1, w - 1, h - 1);
	gdk_draw_line (event->window, dark_gc, w - 1, 0, w - 1, h - 1);

	return FALSE;
}

static void
glade_custom_init (GladeCustom *custom)
{
	custom->custom_pixmap = NULL;

	gtk_widget_set_size_request (GTK_WIDGET (custom),
				     GLADE_CUSTOM_WIDTH_REQ,
				     GLADE_CUSTOM_HEIGHT_REQ);

	gtk_widget_show (GTK_WIDGET (custom));
}

static void
glade_custom_class_init (GladeCustomClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GObjectClass   *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize     = glade_custom_finalize;
	object_class->set_property = glade_custom_set_property;
	object_class->get_property = glade_custom_get_property;

	widget_class->realize       = glade_custom_realize;
	widget_class->size_allocate = glade_custom_size_allocate;
	widget_class->expose_event  = glade_custom_expose;

	g_object_class_install_property 
		(object_class, PROP_CREATION,
		 g_param_spec_string 
		 ("creation-function", _("Creation Function"), 
		  _("The function which creates this widget"),
		  NULL, G_PARAM_READWRITE));

	g_object_class_install_property 
		(object_class, PROP_STR1,
		 g_param_spec_string 
		 ("string1", _("String 1"), 
		  _("The first string argument to pass to the function"),
		  NULL, G_PARAM_READWRITE));

	g_object_class_install_property 
		(object_class, PROP_STR2,
		 g_param_spec_string 
		 ("string2", _("String 2"), 
		  _("The second string argument to pass to the function"),
		  NULL, G_PARAM_READWRITE));

	g_object_class_install_property 
		(object_class, PROP_INT1,
		 g_param_spec_int 
		 ("int1", _("Integer 1"), 
		  _("The first integer argument to pass to the function"),
		  G_MININT, G_MAXINT, 0, G_PARAM_READWRITE));

	g_object_class_install_property 
		(object_class, PROP_INT2,
		 g_param_spec_int 
		 ("int2", _("Integer 2"), 
		  _("The second integer argument to pass to the function"),
		  G_MININT, G_MAXINT, 0, G_PARAM_READWRITE));
}

GType
custom_get_type (void)
{
	static GType custom_type = 0;

	if (!custom_type)
	{
		static const GTypeInfo custom_info = 
		{
			sizeof (GladeCustomClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) glade_custom_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,           /* class_data */
			sizeof (GladeCustom),
			0,              /* n_preallocs */
			(GInstanceInitFunc) glade_custom_init,
		};
		custom_type = 
			g_type_register_static (GTK_TYPE_WIDGET,
						"Custom",
						&custom_info, 0);
	}
	return custom_type;
}
