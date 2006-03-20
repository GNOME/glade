/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PROJECT_VIEW_H__
#define __GLADE_PROJECT_VIEW_H__

G_BEGIN_DECLS


#define GLADE_TYPE_PROJECT_VIEW            (glade_project_view_get_type ())
#define GLADE_PROJECT_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PROJECT_VIEW, GladeProjectView))
#define GLADE_PROJECT_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PROJECT_VIEW, GladeProjectViewClass))
#define GLADE_IS_PROJECT_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PROJECT_VIEW))
#define GLADE_IS_PROJECT_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PROJECT_VIEW))
#define GLADE_PROJECT_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_PROJECT_VIEW, GladeProjectViewClass))


/* A view of a GladeProject. */
typedef struct _GladeProjectView       GladeProjectView;
typedef struct _GladeProjectViewClass  GladeProjectViewClass;

struct _GladeProjectView
{
	GtkScrolledWindow scrolled_window; /* The project view is a scrolled
					    * window containing a tree view
					    * of the project
					    */

	GtkTreeStore *model; /* The model */

	GtkWidget *tree_view; /* The view */

	gboolean is_list; /* If true the view is a list, if false the view
			   * is a tree
			   */

	GladeProject *project; /* A pointer so that we can get back to the
				* project that we are a view for
				*/

	GList *project_data; /* A list of private data structures of 
			      * project specific data.
			      */
	
	gboolean updating_selection; /* True when we are going to set the
				      * project selection. So that we don't
				      * recurse cause we are also listening
				      * for the project changed selection
				      * signal
				      */
	gboolean updating_treeview; /* Eliminate feedback from the tree-view 
				     * (same as updating_selection)
				     */
};

struct _GladeProjectViewClass
{
	GtkScrolledWindowClass parent_class;

	void   (*add_item)      (GladeProjectView *view,
				 GladeWidget *widget);
	void   (*remove_item)   (GladeProjectView *view,
				 GladeWidget *widget);
	void   (*widget_name_changed) (GladeProjectView *view,
				       GladeWidget *widget);

	/* Selection update is when the project changes the selection
	 * and we need to update our state, selection changed functions
	 * are the other way arround, the selection in the view changed
	 * and we need to let the project know about it. Chema
	 */
	void   (*selection_update) (GladeProjectView *view,
				    GladeProject *project);
};

/**
 * GladeProjectViewType:
 * @GLADE_PROJECT_VIEW_LIST: View only toplevels as a flat list
 * @GLADE_PROJECT_VIEW_TREE: View as the entire project tree
 */
typedef enum {
	GLADE_PROJECT_VIEW_LIST,
	GLADE_PROJECT_VIEW_TREE,
} GladeProjectViewType;


LIBGLADEUI_API
GType glade_project_view_get_type (void);

LIBGLADEUI_API
GladeProjectView *glade_project_view_new (GladeProjectViewType type);

LIBGLADEUI_API
GladeProject *glade_project_view_get_project (GladeProjectView *view);

LIBGLADEUI_API
void glade_project_view_set_project (GladeProjectView *view,
				     GladeProject *project);

G_END_DECLS

#endif /* __GLADE_PROJECT_VIEW_H__ */
