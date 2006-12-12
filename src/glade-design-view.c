/*
 * glade-design-view.c
 *
 * Copyright (C) 2006 Vincent Geddes
 *
 * Authors:
 *   Vincent Geddes <vincent.geddes@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANDESIGN_VIEWILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
 
#include "glade.h"
#include "glade-utils.h"
#include "glade-design-view.h"
#include "glade-design-layout.h"

#include <glib.h>
#include <glib/gi18n.h>

#define GLADE_DESIGN_VIEW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), GLADE_TYPE_DESIGN_VIEW, GladeDesignViewPrivate))

#define GLADE_DESIGN_VIEW_KEY "GLADE_DESIGN_VIEW_KEY"

enum
{
	PROP_0,
	PROP_PROJECT,
};

struct _GladeDesignViewPrivate
{
	GtkWidget      *layout;

	GladeProject   *project;
};

static GtkVBoxClass *parent_class = NULL;


G_DEFINE_TYPE(GladeDesignView, glade_design_view, GTK_TYPE_VBOX)

static void
glade_design_view_set_project (GladeDesignView *view, GladeProject *project)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));

	view->priv->project = project;
	
	g_object_set_data (G_OBJECT (view->priv->project), GLADE_DESIGN_VIEW_KEY, view);

}

static void 
glade_design_view_set_property (GObject *object,
		            guint prop_id,
		      	    const GValue *value,
		            GParamSpec *pspec)
{
	switch (prop_id)
	{
		case PROP_PROJECT:
			glade_design_view_set_project (GLADE_DESIGN_VIEW (object), g_value_get_object (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
    }
}

static void
glade_design_view_get_property (GObject    *object,
			        guint       prop_id,
			        GValue     *value,
			        GParamSpec *pspec)
{
	switch (prop_id)
	{
		case PROP_PROJECT:
			g_value_set_object (value, GLADE_DESIGN_VIEW (object)->priv->project);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;			
	}
}

static void
glade_design_view_init (GladeDesignView *view)
{
	GtkWidget *sw;
	GtkWidget *viewport;

	view->priv = GLADE_DESIGN_VIEW_GET_PRIVATE (view);

	view->priv->project = NULL;
	view->priv->layout = glade_design_layout_new ();

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);

	viewport = gtk_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (viewport), view->priv->layout);
	gtk_container_add (GTK_CONTAINER (sw), viewport);

	gtk_widget_show (sw);
	gtk_widget_show (viewport);
	gtk_widget_show (view->priv->layout);

	gtk_box_pack_start (GTK_BOX (view), sw, TRUE, TRUE, 0);

	gtk_container_set_border_width (GTK_CONTAINER (view), 0);
	
}

static void
glade_design_view_class_init (GladeDesignViewClass *klass)
{
	GObjectClass    *object_class;
	GtkWidgetClass  *widget_class;

	parent_class = g_type_class_peek_parent (klass);
	object_class = G_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	object_class->get_property = glade_design_view_get_property;
	object_class->set_property = glade_design_view_set_property;

	g_object_class_install_property (object_class,
					 PROP_PROJECT,
					 g_param_spec_object ("project",
							      "Project",
							      "The project for this view",
							      GLADE_TYPE_PROJECT,
							      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private (object_class, sizeof (GladeDesignViewPrivate));
}

GladeProject*
glade_design_view_get_project (GladeDesignView *view)
{
	g_return_val_if_fail (GLADE_IS_DESIGN_VIEW (view), NULL);
	
	return view->priv->project;

}

GtkWidget *
glade_design_view_new (GladeProject *project)
{
	GladeDesignView *view;

	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

	view = g_object_new (GLADE_TYPE_DESIGN_VIEW, 
			    "project", project,
			    NULL);

	return GTK_WIDGET (view);
}

GladeDesignView *
glade_design_view_get_from_project (GladeProject *project)
{
	gpointer p;
	
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);
	
	p = g_object_get_data (G_OBJECT (project), GLADE_DESIGN_VIEW_KEY);
	
	return (p != NULL) ? GLADE_DESIGN_VIEW (p) : NULL;

}

GladeDesignLayout *
glade_design_view_get_layout (GladeDesignView *view)
{
	return GLADE_DESIGN_LAYOUT (view->priv->layout);
}
