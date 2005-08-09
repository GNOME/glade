/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_FIXED_MANAGER_H__
#define __GLADE_FIXED_MANAGER_H__

#include <glib-object.h>
#include <gdk/gdk.h>

#include "glade-widget.h"
#include "glade-cursor.h"

G_BEGIN_DECLS

#define GLADE_TYPE_FIXED_MANAGER            (glade_fixed_manager_get_type())
#define GLADE_FIXED_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_FIXED_MANAGER, GladeFixedManager))
#define GLADE_FIXED_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_FIXED_MANAGER, GladeFixedManagerClass))
#define GLADE_IS_FIXED_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_FIXED_MANAGER))
#define GLADE_IS_FIXED_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_FIXED_MANAGER))
#define GLADE_FIXED_MANAGER_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_FIXED_MANAGER, GladeFixedManagerClass))


typedef struct _GladeFixedManager        GladeFixedManager;
typedef struct _GladeFixedManagerClass   GladeFixedManagerClass;

struct _GladeFixedManager {
	GObject           parent_instance;

	GladeWidget      *container;

	gchar            *x_prop;                // packing property names (on child widgets) used 
	gchar            *y_prop;                // to obtain & configure widget coordinates
	gchar            *width_prop;            // property names (on child widgets) used to obtain
	gchar            *height_prop;           // & configure widget dimentions.

	gint              pointer_x_origin;
	gint              pointer_y_origin;
	gint              pointer_x_child_origin;
	gint              pointer_y_child_origin;
	gint              child_x_origin;
	gint              child_y_origin;
	gint              child_width_origin;
	gint              child_height_origin;

	GladeWidget      *configuring;
	GladeCursorType   operation;
	gboolean          creating;
	gint              create_x;
	gint              create_y;
	gint              mouse_x;
	gint              mouse_y;
};

struct _GladeFixedManagerClass {
	GObjectClass   parent_class;

	GladeWidget *(* create_child)    (GladeFixedManager *, GladeWidgetClass *);
	void         (* child_created)   (GladeFixedManager *, GladeWidget *);
	gboolean     (* add_child)       (GladeFixedManager *, GladeWidget *);
	gboolean     (* remove_child)    (GladeFixedManager *, GladeWidget *);
	gboolean     (* configure_child) (GladeFixedManager *, GladeWidget *, GdkRectangle *);
	void         (* configure_begin) (GladeFixedManager *, GladeWidget *);
	void         (* configure_end)   (GladeFixedManager *, GladeWidget *);

	/* Signal handler for `GTK_CONTAINER (manager->container->object)' and
	 * child widgets
	 */
	gint         (* event)           (GtkWidget *, GdkEvent *, GladeFixedManager *);
	gint         (* child_event)     (GtkWidget *, GdkEvent *, GladeFixedManager *);

};

LIBGLADEUI_API GType              glade_fixed_manager_get_type     (void);
LIBGLADEUI_API GladeFixedManager *glade_fixed_manager_new          (GladeWidget       *gtkcontainer,
								    const gchar       *x_prop,
								    const gchar       *y_prop,
								    const gchar       *width_prop,
								    const gchar       *height_prop);
LIBGLADEUI_API GladeWidget       *glade_fixed_manager_create_child (GladeFixedManager *manager,
								    GladeWidgetClass  *wclass);
LIBGLADEUI_API void               glade_fixed_manager_add_child    (GladeFixedManager *manager,
								    GladeWidget       *child,
								    gboolean           at_mouse);
LIBGLADEUI_API void               glade_fixed_manager_remove_child (GladeFixedManager *manager,
								    GladeWidget       *child);
LIBGLADEUI_API void               glade_fixed_manager_post_mouse   (GladeFixedManager *manager,
								    gint               x,
								    gint               y);

G_END_DECLS

#endif   /* __GLADE_FIXED_MANAGER_H__ */
