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

#ifndef __GLADE_PROJECT_H__
#define __GLADE_PROJECT_H__

G_BEGIN_DECLS

#define GLADE_PROJECT(obj)          GTK_CHECK_CAST (obj, glade_project_get_type (), GladeProject)
#define GLADE_PROJECT_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, glade_project_get_type (), GladeProjectClass)
#define GLADE_IS_PROJECT(obj)       GTK_CHECK_TYPE (obj, glade_project_get_type ())

typedef struct _GladeProjectClass  GladeProjectClass;

/* A GladeProject is well... a project nothing more nothing less. It is the
 * memory representation of a glade file */
struct _GladeProject
{
	GtkObject object; /* We emit have signals so we are a GtkObject */

	gchar *name;         /* The name of the project like network-conf */
	gchar *xml_filename; /* The name of the xml file on this (w/o the path)*/
	gchar *directory;    /* The path this project resides in. The directory 
			      * plus the xml_filename make the full path.
			      */

	gboolean changed;    /* A flag that is set when a project has changes
			      * if this flag is not set we don't have to query
			      * for confirmation after a close or exit is
			      * requested
			      */

	GList *widgets; /* A list of GladeWidgets that make up this project.
			 * The widgets are stored in no particular order.
			 */
};

struct _GladeProjectClass
{
	GtkObjectClass parent_class;

	void   (*add_widget)          (GladeProject   *project,
				       GladeWidget    *widget);
	void   (*remove_widget)       (GladeProject   *project,
				       GladeWidget    *widget);
	void   (*widget_name_changed) (GladeProject   *project,
				       GladeWidget    *widget);
};


guint glade_project_get_type (void);
GladeProject * glade_project_get_active (void);
GladeProject * glade_project_new (void);

void glade_project_add_widget (GladeProject  *project,
			       GladeWidget *glade_widget);

void glade_project_widget_name_changed (GladeProject *project,
					GladeWidget *widget);

G_END_DECLS

#endif /* __GLADE_PROJECT_H__ */
