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
	GladeProject *active_project;  /* Currently active project (if there is at least one
				        * project; then this is always valid) */
	GtkWidget *widget_tree;        /* The widget tree window*/
	GtkWindow *palette_window;     /* The window that will contain the palette */
	GladePalette *palette;         /* See glade-palette */
	GtkWindow *editor_window;      /* The window that will contain the editor */
	GladeEditor *editor;           /* See glade-editor */
	GladeClipboard *clipboard;     /* See glade-clipboard */
	GladeProjectView *active_view; /* See glade-project-view */
	GList *catalogs;               /* See glade-catalog */
	GtkWidget *toolbar_undo;       /* undo item on the toolbar */
	GtkWidget *toolbar_redo;       /* redo item on the toolbar */

	GladeWidgetClass *add_class;   /* The GladeWidgetClass that we are about
					* to add to a container. NULL if no
					* class is to be added. This also has to
					* be in sync with the depressed button
					* in the GladePalette
					*/

	GladeWidgetClass *alt_class;  /* The alternate class is always the same as
				       * add_class except when add_class is NULL. (This is
				       * so that the user can enter many of the same widget
				       * without having to reselect widgets in the palette).
				       */


	GList *views;    /* A list of GladeProjectView item */
	GList *projects; /* The list of Projects */
};


GladeProjectWindow *glade_project_window_new (GList *catalogs);

GladeProjectWindow *glade_project_window_get (void);

void glade_project_window_show_all (void);

void glade_project_window_new_project (void);

void glade_project_window_open_project (const gchar *path);

void glade_project_window_refresh_undo_redo (void);

GladeProject *glade_project_window_get_active_project (GladeProjectWindow *gpw);

G_END_DECLS

#endif /* __GLADE_PROJECT_WINDOW_H__ */
