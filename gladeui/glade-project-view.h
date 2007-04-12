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

/* A view of a GladeProject */
typedef struct _GladeProjectView         GladeProjectView;
typedef struct _GladeProjectViewClass    GladeProjectViewClass;
typedef struct _GladeProjectViewPrivate  GladeProjectViewPrivate;

struct _GladeProjectView
{
	GtkScrolledWindow sw; /* The parent is a scrolled window */

	GladeProjectViewPrivate *priv;
};

struct _GladeProjectViewClass
{
	GtkScrolledWindowClass parent_class;

	void   (*add_item)            (GladeProjectView *view,
				       GladeWidget *widget);
	void   (*remove_item)         (GladeProjectView *view,
				       GladeWidget *widget);
	void   (*widget_name_changed) (GladeProjectView *view,
				       GladeWidget *widget);				       
	void    (*item_activated)     (GladeProjectView *view,
				       GladeWidget *widget);

	/* Selection update is when the project changes the selection
	 * and we need to update our state, selection changed functions
	 * are the other way arround, the selection in the view changed
	 * and we need to let the project know about it. Chema
	 */
	void   (*selection_update)    (GladeProjectView *view,
				       GladeProject *project);
};


GType             glade_project_view_get_type (void) G_GNUC_CONST;

GtkWidget        *glade_project_view_new (void);

GladeProject     *glade_project_view_get_project (GladeProjectView *view);

void              glade_project_view_set_project (GladeProjectView *view, GladeProject *project);
				     
void              glade_project_view_expand_all (GladeProjectView *view);
				     
void              glade_project_view_collapse_all (GladeProjectView *view);

G_END_DECLS

#endif /* __GLADE_PROJECT_VIEW_H__ */
