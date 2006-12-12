/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_WIDGET_H__
#define __GLADE_WIDGET_H__

#include <glib.h>
#include <glib-object.h>

#include "glade-widget-adaptor.h"
#include "glade-signal.h"
#include "glade-property.h"

G_BEGIN_DECLS
 
#define GLADE_TYPE_WIDGET            (glade_widget_get_type ())
#define GLADE_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_WIDGET, GladeWidget))
#define GLADE_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_WIDGET, GladeWidgetClass))
#define GLADE_IS_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_WIDGET))
#define GLADE_IS_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_WIDGET))
#define GLADE_WIDGET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_WIDGET, GladeWidgetClass))

typedef struct _GladeWidgetClass  GladeWidgetClass;

struct _GladeWidget
{
	GObject parent_instance;

	GladeWidgetAdaptor *adaptor; /* An adaptor class for the object type */

	GladeProject       *project; /* A pointer to the project that this 
					widget currently belongs to. */

	GladeWidget  *parent;  /* A pointer to the parent widget in the hierarchy */
	
	gchar *name; /* The name of the widget. For example window1 or
		      * button2. This is a unique name and is the one
		      * used when loading widget with libglade
		      */
	
	gchar *internal; /* If the widget is an internal child of 
			  * another widget this is the name of the 
			  * internal child, otherwise is NULL.
			  * Internal children cannot be deleted.
			  */
	
	gboolean anarchist; /* Some composite widgets have internal children
			     * that are not part of the same hierarchy; hence 'anarchists',
			     * typicly a popup window or its child (we need to mark
			     * them so we can avoid bookkeeping packing props on them etc.).
			     */

	GObject *object; /* A pointer to the object that was created.
			  * if it is a GtkWidget; it is shown as a "view"
			  * of the GladeWidget. This object is updated as
			  * the properties are modified for the GladeWidget.
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

	gboolean   visible; /* Local copy of widget visibility, we need to keep track of this
			     * since the objects copy may be invalid due to a rebuild.
			     */


	gboolean   prop_refs_readonly; /* Whether this list is currently readonly */
	GList     *prop_refs; /* List of properties in the project who's value are `this object'
			       * (this is used to set/unset those properties when the object is
			       * added/removed from the project).
			       */

	/* Construct parameters: */
	GladeWidget       *construct_template;
	GladeWidgetInfo   *construct_info;
	GladeCreateReason  construct_reason;
	gchar             *construct_internal;
};

struct _GladeWidgetClass
{
	GObjectClass parent_class;

	void         (*add_child)               (GladeWidget *, GladeWidget *, gboolean);
	void         (*remove_child)            (GladeWidget *, GladeWidget *);
	void         (*replace_child)           (GladeWidget *, GObject *, GObject *);

	void         (*add_signal_handler)	(GladeWidget *, GladeSignal *);
	void         (*remove_signal_handler)	(GladeWidget *, GladeSignal *);
	void         (*change_signal_handler)	(GladeWidget *, GladeSignal *, GladeSignal *);
	gboolean     (*action_activated)        (GladeWidget *, const gchar *);
	
	gint         (*button_press_event)      (GladeWidget *, GdkEvent *);
	gint         (*button_release_event)    (GladeWidget *, GdkEvent *);
	gint         (*motion_notify_event)     (GladeWidget *, GdkEvent *);
	gint         (*enter_notify_event)      (GladeWidget *, GdkEvent *);

	void         (*setup_events)            (GladeWidget *, GtkWidget *);
	gboolean     (*event)                   (GtkWidget *, GdkEvent *, GladeWidget *);
};

/*******************************************************************************
                                  General api
 *******************************************************************************/
LIBGLADEUI_API
GType                   glade_widget_get_type		    (void);
LIBGLADEUI_API
GladeWidget            *glade_widget_get_from_gobject       (gpointer          object);
LIBGLADEUI_API
void                    glade_widget_add_child              (GladeWidget      *parent,
							     GladeWidget      *child,
							     gboolean          at_mouse);
LIBGLADEUI_API
void                    glade_widget_remove_child           (GladeWidget      *parent,
							     GladeWidget      *child);
LIBGLADEUI_API 
GladeWidgetInfo        *glade_widget_write                  (GladeWidget      *widget,
							     GladeInterface   *interface);
LIBGLADEUI_API 
GladeWidget            *glade_widget_read                   (GladeProject     *project,
							     GladeWidgetInfo  *info);
LIBGLADEUI_API 
void                    glade_widget_replace                (GladeWidget      *parent,
							     GObject          *old_object,
							     GObject          *new_object);
LIBGLADEUI_API 
void                    glade_widget_rebuild                (GladeWidget      *glade_widget);
LIBGLADEUI_API 
GladeWidget            *glade_widget_dup                    (GladeWidget      *template_widget);
LIBGLADEUI_API 
void                    glade_widget_copy_properties        (GladeWidget      *widget,
							     GladeWidget      *template_widget);
LIBGLADEUI_API
void                    glade_widget_set_packing_properties (GladeWidget      *widget,
							     GladeWidget      *container);
LIBGLADEUI_API 
GladeProperty          *glade_widget_get_property           (GladeWidget      *widget,
							     const gchar      *id_property);
LIBGLADEUI_API 
GladeProperty          *glade_widget_get_pack_property      (GladeWidget      *widget,
							     const gchar      *id_property);
LIBGLADEUI_API
GList                  *glade_widget_dup_properties         (GList            *template_props,
                                                             gboolean          as_load);
LIBGLADEUI_API
void                    glade_widget_remove_property        (GladeWidget      *widget,
							     const gchar      *id_property);
LIBGLADEUI_API 
void                    glade_widget_show                   (GladeWidget      *widget);
LIBGLADEUI_API 
void                    glade_widget_hide                   (GladeWidget      *widget);
LIBGLADEUI_API 
void                    glade_widget_add_signal_handler     (GladeWidget      *widget,
							     GladeSignal      *signal_handler);
LIBGLADEUI_API 
void                    glade_widget_remove_signal_handler  (GladeWidget      *widget,
							     GladeSignal      *signal_handler);
LIBGLADEUI_API 
void                    glade_widget_change_signal_handler  (GladeWidget      *widget,
							     GladeSignal      *old_signal_handler,
							     GladeSignal      *new_signal_handler);
LIBGLADEUI_API 
GPtrArray *             glade_widget_list_signal_handlers   (GladeWidget      *widget,
							     const gchar      *signal_name);

LIBGLADEUI_API 
gboolean                glade_widget_has_launcher           (GladeWidget      *widget);
LIBGLADEUI_API 
void                    glade_widget_launch_editor          (GladeWidget      *widget);

LIBGLADEUI_API 
gboolean                glade_widget_has_decendant          (GladeWidget      *widget,
							     GType             type);
LIBGLADEUI_API 
GladeWidget            *glade_widget_event_widget           (void);

/*******************************************************************************
                      Project, object property references
 *******************************************************************************/
LIBGLADEUI_API 
void                    glade_widget_project_notify         (GladeWidget      *widget,
							     GladeProject     *project);
LIBGLADEUI_API 
void                    glade_widget_add_prop_ref           (GladeWidget      *widget,
							     GladeProperty    *property);
LIBGLADEUI_API 
void                    glade_widget_remove_prop_ref        (GladeWidget      *widget,
							     GladeProperty    *property);

/*******************************************************************************
            Functions that deal with properties on the runtime object
 *******************************************************************************/
LIBGLADEUI_API
void                    glade_widget_object_set_property    (GladeWidget      *widget,
							     const gchar      *property_name,
							     const GValue     *value);
LIBGLADEUI_API
void                    glade_widget_object_get_property    (GladeWidget      *widget,
							     const gchar      *property_name,
							     GValue           *value);
LIBGLADEUI_API
void                    glade_widget_child_set_property     (GladeWidget      *widget,
							     GladeWidget      *child,
							     const gchar      *property_name,
							     const GValue     *value);
LIBGLADEUI_API
void                    glade_widget_child_get_property     (GladeWidget      *widget,
							     GladeWidget      *child,
							     const gchar      *property_name,
							     GValue           *value);

/*******************************************************************************
                   GladeProperty api convenience wrappers
 *******************************************************************************/
LIBGLADEUI_API 
gboolean                glade_widget_property_get           (GladeWidget      *widget,
							     const gchar      *id_property,
							     ...);
LIBGLADEUI_API 
gboolean                glade_widget_property_set           (GladeWidget      *widget,
							     const gchar      *id_property,
							     ...);
LIBGLADEUI_API 
gboolean                glade_widget_pack_property_get      (GladeWidget      *widget,
							     const gchar      *id_property,
							     ...);
LIBGLADEUI_API 
gboolean                glade_widget_pack_property_set      (GladeWidget      *widget,
							     const gchar      *id_property,
							     ...);
LIBGLADEUI_API 
gboolean                glade_widget_property_reset         (GladeWidget      *widget,
							     const gchar      *id_property);
LIBGLADEUI_API 
gboolean                glade_widget_pack_property_reset    (GladeWidget      *widget,
							     const gchar      *id_property);
LIBGLADEUI_API 
gboolean                glade_widget_property_default       (GladeWidget      *widget,
							     const gchar      *id_property);
LIBGLADEUI_API 
gboolean                glade_widget_pack_property_default  (GladeWidget      *widget,
							     const gchar      *id_property);
LIBGLADEUI_API 
gboolean                glade_widget_property_set_sensitive (GladeWidget      *widget,
							     const gchar      *id_property,
							     gboolean          sensitive,
							     const gchar      *reason);
LIBGLADEUI_API 
gboolean                glade_widget_pack_property_set_sensitive (GladeWidget      *widget,
								  const gchar      *id_property,
								  gboolean          sensitive,
								  const gchar      *reason);
LIBGLADEUI_API 
gboolean                glade_widget_property_set_enabled   (GladeWidget      *widget,
							     const gchar      *id_property,
							     gboolean          enabled);
LIBGLADEUI_API 
gboolean                glade_widget_pack_property_set_enabled (GladeWidget      *widget,
								const gchar      *id_property,
								gboolean          enabled);

/*******************************************************************************
                                  Accessors
 *******************************************************************************/
LIBGLADEUI_API
void                    glade_widget_set_name		    (GladeWidget      *widget,
							     const gchar      *name);
LIBGLADEUI_API 
G_CONST_RETURN gchar   *glade_widget_get_name               (GladeWidget      *widget);
LIBGLADEUI_API
void                    glade_widget_set_internal	    (GladeWidget      *widget,
							     const gchar      *internal);
LIBGLADEUI_API 
G_CONST_RETURN gchar   *glade_widget_get_internal           (GladeWidget      *widget);
LIBGLADEUI_API
void                    glade_widget_set_object		    (GladeWidget      *gwidget,
							     GObject          *new_object);
LIBGLADEUI_API 
GObject                *glade_widget_get_object             (GladeWidget      *widget);
LIBGLADEUI_API
void                    glade_widget_set_project	    (GladeWidget      *widget,
							     GladeProject     *project);
LIBGLADEUI_API 
GladeProject           *glade_widget_get_project            (GladeWidget      *widget);
LIBGLADEUI_API 
GladeWidgetAdaptor     *glade_widget_get_adaptor            (GladeWidget      *widget);
LIBGLADEUI_API 
GladeWidget            *glade_widget_get_parent             (GladeWidget      *widget);
LIBGLADEUI_API 
void                    glade_widget_set_parent             (GladeWidget      *widget,
							     GladeWidget      *parent);
LIBGLADEUI_API 
gboolean                glade_widget_superuser              (void);
LIBGLADEUI_API 
void                    glade_widget_push_superuser         (void);
LIBGLADEUI_API 
void                    glade_widget_pop_superuser          (void);

G_END_DECLS

#endif /* __GLADE_WIDGET_H__ */
