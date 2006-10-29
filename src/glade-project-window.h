/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PROJECT_WINDOW_H__
#define __GLADE_PROJECT_WINDOW_H__

#include "glade-app.h"

G_BEGIN_DECLS

#define GLADE_TYPE_PROJECT_WINDOW            (glade_project_window_get_type())
#define GLADE_PROJECT_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PROJECT_WINDOW, GladeProjectWindow))
#define GLADE_PROJECT_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PROJECT_WINDOW, GladeProjectWindowClass))
#define GLADE_IS_PROJECT_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PROJECT_WINDOW))
#define GLADE_IS_PROJECT_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PROJECT_WINDOW))
#define GLADE_PROJECT_WINDOW_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_PROJECT_WINDOW, GladeProjectWindowClass))

typedef struct _GladeProjectWindow      GladeProjectWindow;
typedef struct _GladeProjectWindowClass GladeProjectWindowClass;
typedef struct _GladeProjectWindowPrivate  GladeProjectWindowPrivate;

/* A GladeProjectWindow specifies a loaded glade application.
 * it contains pointers to all the components that make up
 * the running app. This is (well should be) the only global
 * variable in the application.
 */
struct _GladeProjectWindow
{
	GladeApp parent;
	
	GladeProjectWindowPrivate *priv;
};

struct _GladeProjectWindowClass
{
	GladeAppClass parent_class;
};

GType                glade_project_window_get_type (void);
GladeProjectWindow * glade_project_window_new (void);
void                 glade_project_window_show_all (GladeProjectWindow *gpw);
void                 glade_project_window_new_project (GladeProjectWindow *gpw);
gboolean             glade_project_window_open_project (GladeProjectWindow *gpw, const gchar *path);
void                 glade_project_window_check_devhelp (GladeProjectWindow *gpw);
G_END_DECLS

#endif /* __GLADE_PROJECT_WINDOW_H__ */
