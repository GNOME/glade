/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2001 Ximian Inc.
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
 *   Chema Celorio <chema@ximian.com>
 */

#ifndef __GLADE_PROJECT_VIEW_H__
#define __GLADE_PROJECT_VIEW_H__

#include <gtk/gtkclist.h>
#include "glade-project.h"

G_BEGIN_DECLS

#define GLADE_PROJECT_VIEW_TYPE	         (glade_project_view_get_type ())
#define GLADE_PROJECT_VIEW(obj)          GTK_CHECK_CAST (obj, glade_project_view_get_type (), GladeProjectView)
#define GLADE_PROJECT_VIEW_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, glade_project_view_get_type (), GladeProjectViewClass)
#define GLADE_IS_PROJECT_VIEW(obj)       GTK_CHECK_TYPE (obj, glade_project_view_get_type ())

typedef struct _GladeProjectViewClass  GladeProjectViewClass;

/* A view of a GladeProject. */
   
struct _GladeProjectView
{
	GObject parent;

	GtkWidget *widget;

	GladeProject *project; /* A pointer so that we can get back to the
				* project that we are a view for
				*/
	
	GladeWidget *selected_widget; /* The selected GladeWidget for this view
				       * Selection shuold really be a GList not
				       * a GladeWidget since we should support
				       * multiple selections.
				       */

	gulong add_widget_signal_id; /* We need to know the signal id which we are
				      * connected to so that when the project changes
				      * we stop listening to the old project
				      */
	gulong widget_name_changed_signal_id;

	GtkTreeStore *model; /* Model */

	gboolean is_list; /* If true the view is a list, if false the view
			   * is a tree
			   */
};

struct _GladeProjectViewClass
{
	GtkObjectClass parent_class;

	/* We use this signal so that projects can be advised that
	 * a widget has been selected
	 */
	void   (*item_selected) (GladeProjectView *view,
				 GladeWidget *component);

	void   (*add_item)      (GladeProjectView *view,
				 GladeWidget *widget);
	void   (*widget_name_changed) (GladeProjectView *view,
				       GladeWidget *widget);

	void   (*set_project)   (GladeProjectView *view,
				 GladeProject *project);
};

typedef enum {
	GLADE_PROJECT_VIEW_LIST,
	GLADE_PROJECT_VIEW_TREE,
} GladeProjectViewType;

guint               glade_project_view_get_type (void);
GladeProjectView  * glade_project_view_new (GladeProjectViewType type);

GladeProject* glade_project_view_get_project   (GladeProjectView *project_view);
void	      glade_project_view_set_project   (GladeProjectView *project_view,
						GladeProject	 *project);

GtkWidget * glade_project_view_get_widget (GladeProjectView *view);

void glade_project_view_select_item (GladeProjectView *view, GladeWidget *item);

G_END_DECLS

#endif /* __GLADE_PROJECT_VIEW_H__ */
