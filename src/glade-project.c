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


#include "glade.h"
#include "glade-project.h"
#include "glade-project-window.h"

static void glade_project_class_init (GladeProjectClass * klass);
static void glade_project_init (GladeProject *project);
static void glade_project_destroy (GtkObject *object);

enum
{
	ADD_WIDGET,
	REMOVE_WIDGET,
	WIDGET_NAME_CHANGED,
	LAST_SIGNAL
};

static guint glade_project_signals[LAST_SIGNAL] = {0};
static GtkObjectClass *parent_class = NULL;

guint
glade_project_get_type (void)
{
	static guint glade_project_type = 0;
  
	if (!glade_project_type)
	{
		GtkTypeInfo glade_project_info =
		{
			"GladeProject",
			sizeof (GladeProject),
			sizeof (GladeProjectClass),
			(GtkClassInitFunc) glade_project_class_init,
			(GtkObjectInitFunc) glade_project_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		glade_project_type = gtk_type_unique (gtk_object_get_type (),
						      &glade_project_info);
	}
	
	return glade_project_type;
}

static void
glade_project_class_init (GladeProjectClass * klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) klass;

	parent_class = gtk_type_class (gtk_object_get_type ());

	glade_project_signals[ADD_WIDGET] =
		gtk_signal_new ("add_widget",
				GTK_RUN_LAST,
				GTK_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (GladeProjectClass, add_widget),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	glade_project_signals[REMOVE_WIDGET] =
		gtk_signal_new ("remove_widget",
				GTK_RUN_LAST,
				GTK_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (GladeProjectClass, remove_widget),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	glade_project_signals[WIDGET_NAME_CHANGED] =
		gtk_signal_new ("widget_name_changed",
				GTK_RUN_LAST,
				GTK_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (GladeProjectClass, widget_name_changed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	
	klass->add_widget = NULL;
	klass->remove_widget = NULL;
	klass->widget_name_changed = NULL;
	
	object_class->destroy = glade_project_destroy;
}


static void
glade_project_init (GladeProject * project)
{
	project->name = NULL;
	project->widgets = NULL;
}

static void
glade_project_destroy (GtkObject *object)
{
	GladeProject *project;

	project = GLADE_PROJECT (object);

}

GladeProject *
glade_project_new (void)
{
	GladeProject *project;
	static gint i = 1;

	project = GLADE_PROJECT (gtk_type_new (glade_project_get_type()));
	project->name = g_strdup_printf ("Untitled %i", i++);
	
	return project;
}

static void
glade_project_set_changed (GladeProject *project, gboolean changed)
{
	project->changed = changed;
}


void
glade_project_add_widget (GladeProject  *project,
			  GladeWidget *widget)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GTK_IS_OBJECT (project));

	project->widgets = g_list_prepend (project->widgets, widget);

	gtk_signal_emit (GTK_OBJECT (project),
			 glade_project_signals [ADD_WIDGET], widget);
	
	glade_project_set_changed (project, TRUE);
}

void
glade_project_widget_name_changed (GladeProject *project,
				   GladeWidget *widget)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GTK_IS_OBJECT (project));

	gtk_signal_emit (GTK_OBJECT (project),
			 glade_project_signals [WIDGET_NAME_CHANGED], widget);
	
	glade_project_set_changed (project, TRUE);
}


GladeProject *
glade_project_get_active (void)
{
	GladeProjectWindow *gpw;
	
	gpw = glade_project_window_get ();

	if (gpw == NULL) {
		g_warning ("Could not get the active project\n");
		return NULL;
	}
		
	return gpw->project;
}
