/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_WIDGET_H__
#define __GLADE_WIDGET_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS
 
#define GLADE_TYPE_WIDGET            (glade_widget_get_type ())
#define GLADE_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_WIDGET, GladeWidget))
#define GLADE_WIDGET_KLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_WIDGET, GladeWidgetKlass))
#define GLADE_IS_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_WIDGET))
#define GLADE_IS_WIDGET_KLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_WIDGET))
#define GLADE_WIDGET_GET_KLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_WIDGET, GladeWidgetKlass))

typedef struct _GladeWidget		GladeWidget;
typedef struct _GladeWidgetKlass	GladeWidgetKlass;

struct _GladeWidget
{
	GObject object;

	GladeWidgetClass *widget_class;
	GladeProject *project; /* A pointer to the project that this widget belongs to. */

	char *name; /* The name of the widet. For example window1 or
		     * button2. This is a unique name and is the one
		     * used when loading widget with libglade
		     */

	char *internal; /* If the widget is an internal child of 
			 * another widget this is the name of the 
			 * internal child, otherwise is NULL.
			 * Internal children cannot be deleted.
			 */

	GtkWidget *widget; /* A pointer to the widget that was created.
			    * and is shown as a "view" of the GladeWidget.
			    * This widget is updated as the properties are
			    * modified for the GladeWidget. We should not
			    * destroy this widgets once created, we should
			    * just hide them
			    */

	GList *properties; /* A list of GladeProperty. A GladeProperty is an
			    * instance of a GladePropertyClass. If a
			    * GladePropertyClass for a gtkbutton is label, its
			    * property is "Ok". 
			    */

	GList *packing_properties; /* A list of GladeProperty. Note that these
				    * properties are related to the container
				    * of the widget, thus they change after
				    * pasting the widget to a different
				    * container. Toplevels widget do not have
				    * packing properties.
				    * See also child_properties of 
				    * GladeWidgetClass.
				    */

	GHashTable *signals; /* A table with a GPtrArray of GladeSignals (signal handlers),
			      * indexed by its name */
};

struct _GladeWidgetKlass
{
	GObjectClass parent_class;

	void   (*add_signal_handler)	(GladeWidget *widget,
					 GladeSignal *signal_handler);
	void   (*remove_signal_handler)	(GladeWidget *widget,
					 GladeSignal *signal_handler);
	void   (*change_signal_handler)	(GladeWidget *widget,
					 GladeSignal *old_signal_handler,
					 GladeSignal *new_signal_handler);
};

GType			glade_widget_get_type			(void);
GladeWidget *		glade_widget_new			(GladeWidgetClass *klass, GladeProject *project);
GladeWidget *		glade_widget_new_for_internal_child	(GladeWidgetClass *klass, GladeWidget *parent, GtkWidget *internal_widget, const char *internal_name);

void			glade_widget_set_name			(GladeWidget *widget, const char *name);
void			glade_widget_set_internal		(GladeWidget *widget, const char *internal);
void			glade_widget_set_widget			(GladeWidget *widget, GtkWidget *widget_gtk);
void			glade_widget_set_project		(GladeWidget *widget, GladeProject *project);

const char *		glade_widget_get_name			(GladeWidget *widget);
const char *		glade_widget_get_internal		(GladeWidget *widget);
GladeWidgetClass *	glade_widget_get_class			(GladeWidget *widget);
GladeProject *		glade_widget_get_project		(GladeWidget *widget);
GtkWidget *		glade_widget_get_widget			(GladeWidget *widget);
GladeProperty *		glade_widget_get_property		(GladeWidget *widget, const char *id_property);

void			glade_widget_replace			(GtkWidget *old_widget, GtkWidget *new_widget);

/* widget signals */
void			glade_widget_add_signal_handler		(GladeWidget *widget, GladeSignal *signal_handler);
void			glade_widget_remove_signal_handler	(GladeWidget *widget, GladeSignal *signal_handler);
void			glade_widget_change_signal_handler	(GladeWidget *widget, GladeSignal *old_signal_handler, GladeSignal *new_signal_handler);
GPtrArray *		glade_widget_list_signal_handlers	(GladeWidget *widget, const char *signal_name); /* array of GladeSignal* */

/* serialization */
GladeXmlNode *		glade_widget_write			(GladeWidget *widget, GladeXmlContext *context);
GladeWidget *		glade_widget_read			(GladeProject *project, GladeXmlNode *node);

/* helper functions */
#define			glade_widget_get_from_gtk_widget(w)	g_object_get_data (G_OBJECT (w), "GladeWidgetDataTag")
GladeWidget *		glade_widget_get_parent			(GladeWidget *widget);

G_END_DECLS

#endif __GLADE_WIDGET_H__
