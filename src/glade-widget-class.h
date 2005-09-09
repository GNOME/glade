/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_WIDGET_CLASS_H__
#define __GLADE_WIDGET_CLASS_H__

#include <glib-object.h>
#include <gmodule.h>
#include <gtk/gtk.h>
#include "glade-xml-utils.h"
#include "glade-property-class.h"

G_BEGIN_DECLS


#define GLADE_WIDGET_CLASS(gwc) ((GladeWidgetClass *) gwc)
#define GLADE_IS_WIDGET_CLASS(gwc) (gwc != NULL)

typedef struct _GladeWidgetClass       GladeWidgetClass;
typedef struct _GladeSupportedChild    GladeSupportedChild;
typedef struct _GladeWidgetClassSignal GladeWidgetClassSignal;


typedef void (* GladeChildSetPropertyFunc) (GObject      *container,
					    GObject      *child,
					    const gchar  *property_name,
					    const GValue *value);

typedef void (* GladeChildGetPropertyFunc) (GObject      *container,
					    GObject      *child,
					    const gchar  *property_name,
					    GValue       *value);

typedef GList *(* GladeGetChildrenFunc)    (GObject      *container);

typedef void   (* GladeAddChildFunc)       (GObject      *parent,
					    GObject      *child);
typedef void   (* GladeRemoveChildFunc)    (GObject      *parent,
					    GObject      *child);

/* GladeWidgetClass contains all the information we need regarding an widget
 * type. It is also used to store information that has been loaded to memory
 * for that object like the icon/mask.
 */
struct _GladeWidgetClass
{
	GType type;          /* GType of the widget */

	gchar *name;         /* Name of the widget, for example GtkButton */

	GdkPixbuf *icon;     /* The GdkPixbuf icon for the widget */

	gchar *generic_name; /* Use to generate names of new widgets, for
			      * example "button" so that we generate button1,
			      * button2, buttonX ..
			      */

	gchar *palette_name; /* Name used in the palette */

	GList *properties;   /* List of GladePropertyClass objects.
			      * [see glade-property.h ] this list contains
			      * properties about the widget that we are going
			      * to modify. Like "title", "label", "rows" .
			      * Each property creates an input in the propety
			      * editor.
			      */

	GList *signals;     /* List of GladeWidgetClassSignal objects */


	GList *children;    /* List of GladeSupportedChild objects */
        GList *child_packings; /* Private */ 

	GModule *module;	/* Module with the (optional) special functions
				 * needed for placeholder_replace, post_create_function
				 * and the set & get functions of the properties
				 * of this class.
				 */

	gboolean in_palette;

	/* Executed after widget creation: it takes care of creating the
	 * GladeWidgets associated with internal children. It's also the place
	 * to set sane defaults, e.g. set the size of a window.
	 */
	void (*post_create_function) (GObject      *gobject);

	/* Retrieves the the internal child of the given name.
	 */
	void (*get_internal_child)   (GObject      *parent,
				      const gchar  *name,
				      GObject     **child);

	/* Is property_class of ancestor applicable to the widget? Usually property_class only
	 * applies to direct children of a given ancestor */
	gboolean (*child_property_applies) (GtkWidget *ancestor,
					    GtkWidget *widget,
					    const gchar *property_id);
};

struct _GladeSupportedChild
{
	GType        type;         /* This supported child type */

	GList       *properties;   /* List of GladePropertyClass objects representing
				    * child_properties of a container (the list is empty if
				    * this container has no child_properties)
				    * Note that the actual GladeProperty corresponding to
				    * each class end up in the packing_properties list of
				    * each _child_ of the container and thus are edited
				    * when the _child_ is selected.
				    */

	GladeAddChildFunc             add;              /* Adds a new child of this type */
	GladeRemoveChildFunc          remove;           /* Removes a child from the container */
	GladeGetChildrenFunc          get_children;     /* Returns a list of children for this
							 * type, not including internals
							 */
	GladeGetChildrenFunc          get_all_children; /* Returns a list of children of this
							 * type, including internals
							 */
	
	GladeChildSetPropertyFunc     set_property; /* Sets/Gets a packing property */
	GladeChildGetPropertyFunc     get_property; /* for this child */
	
	void      (* fill_empty)     (GObject      *container); /* Used for placeholders in
								 * GtkContainers */

	void      (* replace_child)  (GObject      *container,  /* This method replaces a  */
				      GObject      *old,        /* child widget with */
				      GObject      *new);       /* another one: it's used to
								 * replace a placeholder with
								 * a widget and viceversa.
								 */

	gchar                        *special_child_type; /* Special case code for children that
							   * are special children (like notebook tab 
							   * widgets for example).
							   */
};


/* GladeWidgetClassSignal contains all the info we need for a given signal, such as
 * the signal name, and maybe more in the future 
 */
struct _GladeWidgetClassSignal
{
	gchar *name;         /* Name of the signal, eg clicked */
	gchar *type;         /* Name of the object class that this signal belongs to
			      * eg GtkButton */
};
 
LIBGLADEUI_API GladeWidgetClass    *glade_widget_class_new                (GladeXmlNode     *class_node,
									   const gchar      *library);
LIBGLADEUI_API void                 glade_widget_class_free               (GladeWidgetClass *widget_class);
LIBGLADEUI_API GladeWidgetClass    *glade_widget_class_get_by_name        (const char       *name);
LIBGLADEUI_API GladeWidgetClass    *glade_widget_class_get_by_type        (GType             type);
LIBGLADEUI_API GList               *glade_widget_class_get_derived_types  (GType             type);
LIBGLADEUI_API GType 	            glade_widget_class_get_type           (GladeWidgetClass *class);
LIBGLADEUI_API void                 glade_widget_class_dump_param_specs   (GladeWidgetClass *class);
LIBGLADEUI_API GladePropertyClass  *glade_widget_class_get_property_class (GladeWidgetClass *class,
									   const gchar      *name);
LIBGLADEUI_API GladeSupportedChild *glade_widget_class_get_child_support  (GladeWidgetClass *class,
									   GType             child_type);

LIBGLADEUI_API void                 glade_widget_class_container_add              (GladeWidgetClass *class,
										   GObject          *container,
										   GObject          *child);
LIBGLADEUI_API void                 glade_widget_class_container_remove           (GladeWidgetClass *class,
										   GObject          *container,
										   GObject          *child);
LIBGLADEUI_API gboolean             glade_widget_class_container_has_child        (GladeWidgetClass *class,
										   GObject          *container,
										   GObject          *child);
LIBGLADEUI_API GList               *glade_widget_class_container_get_children     (GladeWidgetClass *class,
										   GObject          *container);
LIBGLADEUI_API GList               *glade_widget_class_container_get_all_children (GladeWidgetClass *class,
										   GObject          *container);
LIBGLADEUI_API void                 glade_widget_class_container_set_property     (GladeWidgetClass *class,
										   GObject      *container,
										   GObject      *child,
										   const gchar  *property_name,
										   const GValue *value);
LIBGLADEUI_API void                 glade_widget_class_container_get_property     (GladeWidgetClass *class,
										   GObject      *container,
										   GObject      *child,
										   const gchar  *property_name,
										   GValue       *value);
LIBGLADEUI_API void                 glade_widget_class_container_fill_empty       (GladeWidgetClass *class,
										   GObject      *container);
LIBGLADEUI_API void                 glade_widget_class_container_replace_child    (GladeWidgetClass *class,
										   GObject      *container,
										   GObject      *old,
										   GObject      *new);
LIBGLADEUI_API gboolean             glade_widget_class_contains_non_widgets       (GladeWidgetClass *class);

LIBGLADEUI_API GladePackingDefault *glade_widget_class_get_packing_default        (GladeWidgetClass *child_class,
										   GladeWidgetClass *container_class,
										   const gchar *propert_id);

G_END_DECLS

#endif /* __GLADE_WIDGET_CLASS_H__ */
