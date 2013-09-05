/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

/**
 * SECTION:glade-design-view
 * @Title: GladeDesignView
 * @Short_Description: A widget to embed the workspace.
 *
 * Use this widget to embed the currently active #GtkWindow
 * in a given #GladeProject.
 */
 
#include "config.h"

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

	GtkWidget      *scrolled_window;

	GtkWidget      *progress;
	GtkWidget      *progress_window;
};

static GtkVBoxClass *parent_class = NULL;


G_DEFINE_TYPE(GladeDesignView, glade_design_view, GTK_TYPE_VBOX)


static void
glade_design_view_parse_began (GladeProject    *project,
			       GladeDesignView *view)
{
	gtk_widget_hide (view->priv->scrolled_window);
	gtk_widget_show (view->priv->progress_window);
}

static void
glade_design_view_parse_finished (GladeProject    *project,
				  GladeDesignView *view)
{
	gtk_widget_hide (view->priv->progress_window);
	gtk_widget_show (view->priv->scrolled_window);
}

static void
glade_design_view_load_progress (GladeProject    *project,
				 gint             total,
				 gint             step,
				 GladeDesignView *view)
{
	gchar *path;
	gchar *str;

	path = glade_utils_replace_home_dir_with_tilde (glade_project_get_path (project));
	str  = g_strdup_printf (_("Loading %s: loaded %d of %d objects"), path, step, total);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (view->priv->progress), str);
	g_free (str);
	g_free (path);
				
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (view->priv->progress), step * 1.0 / total);
}

static void
glade_design_view_set_project (GladeDesignView *view, GladeProject *project)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));

	view->priv->project = project;

	g_signal_connect (project, "parse-began", G_CALLBACK (glade_design_view_parse_began), view);
	g_signal_connect (project, "parse-finished", G_CALLBACK (glade_design_view_parse_finished), view);
	g_signal_connect (project, "load-progress", G_CALLBACK (glade_design_view_load_progress), view);

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
	GtkWidget *viewport, *filler, *align;

	view->priv = GLADE_DESIGN_VIEW_GET_PRIVATE (view);

	view->priv->project = NULL;
	view->priv->layout = glade_design_layout_new ();

	view->priv->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view->priv->scrolled_window), 
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (view->priv->scrolled_window), 
					     GTK_SHADOW_NONE);

	viewport = gtk_viewport_new (NULL, NULL);
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER (viewport), view->priv->layout);
	gtk_container_add (GTK_CONTAINER (view->priv->scrolled_window), viewport);

	gtk_widget_show (view->priv->scrolled_window);
	gtk_widget_show (viewport);
	gtk_widget_show (view->priv->layout);

	gtk_box_pack_start (GTK_BOX (view), view->priv->scrolled_window, TRUE, TRUE, 0);

	/* The progress window */
	view->priv->progress_window = gtk_vbox_new (FALSE, 0);
	filler = gtk_label_new (NULL);
	gtk_widget_show (filler);
	gtk_box_pack_start (GTK_BOX (view->priv->progress_window), filler, TRUE, TRUE, 0);

	align = gtk_alignment_new (0.5, 0.5, 0.75, 1.0);
	gtk_widget_show (align);
	gtk_box_pack_start (GTK_BOX (view->priv->progress_window), align, FALSE, TRUE, 0);

	view->priv->progress = gtk_progress_bar_new ();
	gtk_progress_bar_set_ellipsize (GTK_PROGRESS_BAR (view->priv->progress), PANGO_ELLIPSIZE_END);
	gtk_widget_show (view->priv->progress);
	gtk_container_add (GTK_CONTAINER (align), view->priv->progress);

	filler = gtk_label_new (NULL);
	gtk_widget_show (filler);
	gtk_box_pack_start (GTK_BOX (view->priv->progress_window), filler, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (view), view->priv->progress_window, TRUE, TRUE, 0);

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
