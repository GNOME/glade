/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  Gtk+ User Interface Builder
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GLADE_PROJECT_WINDOW_H__
#define __GLADE_PROJECT_WINDOW_H__

G_BEGIN_DECLS

/* A GladeProjectWindow specifies a loaded glade application.
 * it contains pointers to all the components that make up
 * the running app. This is (well should be) the only global
 * variable in the application.
 */

struct _GladeProjectWindow
{
	GtkWidget *window; /* Main window*/
	GtkWidget *main_vbox;
	GtkWidget *statusbar; /* A pointer to the (not yet implemented)
			       * status bar. We might have to come up with
			       * a glade-statusbar object if it makes sense
			       */
	GtkItemFactory *item_factory; /* A pointer to the Item factory.
				       * We need it to be able to later add
				       * items to the Project submenu
				       */
	GladeProject *project;         /* See glade-project */
	GladePalette *palette;         /* See glade-palette */
	GladeEditor *editor;           /* See glade-editor */
	GladeProjectView *active_view; /* See glade-project-view */
	GladeCatalog *catalog;         /* See glade-catalog */
	GladeWidgetClass *add_class;   /* The GladeWidgetClass that we are about
					* to add to a container. NULL if no
					* class is to be added. This also has to
					* be in sync with the depressed button
					* in the GladePalette
					*/
	
	GList *views;  /* A list of GladeProjectView item */
	GList *projects; /* The list of Projects */

};


GladeProjectWindow * glade_project_window_new (GladeCatalog *catalog);
GladeProjectWindow * glade_project_window_get ();

void glade_project_window_set_add_class       (GladeProjectWindow *gpw, GladeWidgetClass *class);
void glade_project_window_set_project         (GladeProjectWindow *project_window,
					       GladeProject       *project);
void glade_project_window_add_project         (GladeProjectWindow *gpw,
					       GladeProject *project);

gboolean glade_project_window_query_properties (GladeWidgetClass *class,
						GladePropertyQueryResult *result);

G_END_DECLS

#endif /* __GLADE_PROJECT_WINDOW_H__ */
