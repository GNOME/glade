/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PROJECT_WINDOW_H__
#define __GLADE_PROJECT_WINDOW_H__

G_BEGIN_DECLS

#define GLADE_IS_PROJECT_WINDOW(o) (o != NULL)
/* A GladeProjectWindow specifies a loaded glade application.
 * it contains pointers to all the components that make up
 * the running app. This is (well should be) the only global
 * variable in the application.
 */

struct _GladeProjectWindow
{
	GtkWidget *window; /* Main window */
	GtkWidget *main_vbox;

	GtkWidget *statusbar; /* A pointer to the status bar. */
	guint statusbar_menu_context_id; /* The context id of the menu bar */
	guint statusbar_actions_context_id; /* The context id of actions messages */
	
	GtkItemFactory *item_factory; /* A pointer to the Item factory.
				       * We need it to be able to later add
				       * items to the Project submenu
				       */
	GladeProject *project;         /* See glade-project */
	GtkWidget *widget_tree;        /* The widget tree window*/
	GtkWindow *palette_window;     /* The window that will contain the palette */
	GladePalette *palette;         /* See glade-palette */
	GtkWindow *editor_window;     /* The window that will contain the editor */
	GladeEditor *editor;           /* See glade-editor */
	GladeClipboard *clipboard;     /* See glade-clipboard */
	GladeProjectView *active_view; /* See glade-project-view */
	GList *catalogs;               /* See glade-catalog */
	GladeWidgetClass *add_class;   /* The GladeWidgetClass that we are about
					* to add to a container. NULL if no
					* class is to be added. This also has to
					* be in sync with the depressed button
					* in the GladePalette
					*/

	GladeWidget *active_widget;     /* The currently selected widget */
	GladePlaceholder *active_placeholder; /* The currently selected
					       * placeholder
					       */

	GList *views;  /* A list of GladeProjectView item */
	GList *projects; /* The list of Projects */

	guint project_selection_changed_signal;
};


GladeProjectWindow * glade_project_window_new (GList *catalogs);
GladeProjectWindow * glade_project_window_get ();
GladeProject *       glade_project_window_get_project ();
void                 glade_project_window_show_all (GladeProjectWindow *gpw);

void glade_project_window_set_add_class       (GladeProjectWindow *gpw, GladeWidgetClass *class);

void glade_project_window_set_project_cb (GladeProject *project);

void glade_project_window_set_project         (GladeProjectWindow *project_window,
					       GladeProject       *project);
void glade_project_window_add_project         (GladeProjectWindow *gpw,
					       GladeProject *project);

void glade_project_window_refresh_undo_redo   (GladeProjectWindow *gpw);
void glade_project_window_refresh_title       (GladeProjectWindow *gpw);

gboolean glade_project_window_query_properties (GladeWidgetClass *class,
						GladePropertyQueryResult *result);

G_END_DECLS

#endif /* __GLADE_PROJECT_WINDOW_H__ */
