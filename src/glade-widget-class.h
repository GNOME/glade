/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_WIDGET_CLASS_H__
#define __GLADE_WIDGET_CLASS_H__

#include <glib-object.h>
#include <gmodule.h>
#include <gtk/gtk.h>
#include "glade-xml-utils.h"
#include "glade-property-class.h"

G_BEGIN_DECLS

#define GLADE_WIDGET_CLASS(gwc)           ((GladeWidgetClass *) gwc)
#define GLADE_IS_WIDGET_CLASS(gwc)        (gwc != NULL)
#define GLADE_VALID_CREATE_REASON(reason) (reason >= 0 && reason < GLADE_CREATE_REASONS)

typedef struct _GladeWidgetClass       GladeWidgetClass;
typedef struct _GladeSupportedChild    GladeSupportedChild;
typedef struct _GladeSignalClass       GladeSignalClass;


/**
 * GladeCreateReason:
 * @GLADE_CREATE_USER: Was created at the user's request
 *                     (this is a good time to set any properties
 *                     or add children to the project; like GtkFrame's 
 *                     label for example).
 * @GLADE_CREATE_COPY: Was created as a result of the copy/paste 
 *                     mechanism, at this point you can count on glade
 *                     to follow up with properties and children on 
 *                     its own.
 * @GLADE_CREATE_LOAD: Was created during the load process.
 * @GLADE_CREATE_REBUILD: Was created as a replacement for another project 
 *                        object; this only happens when the user is 
 *                        changing a property that is marked by the type 
 *                        system as G_PARAM_SPEC_CONSTRUCT_ONLY.
 * @GLADE_CREATE_REASONS: Never used.
 *
 * These are the reasons your #GladePostCreateFunc can be called.
 */
typedef enum _GladeCreateReason 
{
	GLADE_CREATE_USER = 0,
	GLADE_CREATE_COPY,
	GLADE_CREATE_LOAD,
	GLADE_CREATE_REBUILD,
	GLADE_CREATE_REASONS
} GladeCreateReason;

/**
 * GladeChildSetPropertyFunc:
 * @container: A #GObject container
 * @child: The #GObject child
 * @property_name: The property name
 * @value: The #GValue
 *
 * Called to set the packing property @property_name to @value
 * on the @child object of @container.
 */
typedef void (* GladeChildSetPropertyFunc)      (GObject            *container,
						 GObject            *child,
						 const gchar        *property_name,
						 const GValue       *value);

/**
 * GladeChildGetPropertyFunc:
 * @container: A #GObject container
 * @child: The #GObject child
 * @property_name: The property name
 * @value: The #GValue
 *
 * Called to get the packing property @property_name
 * on the @child object of @container into @value.
 */
typedef void (* GladeChildGetPropertyFunc)      (GObject            *container,
						 GObject            *child,
						 const gchar        *property_name,
						 GValue             *value);


/**
 * GladeGetChildrenFunc:
 * @container: A #GObject container
 * @Returns: A #GList of #GObject children.
 *
 * A function called to get @containers children.
 */
typedef GList *(* GladeGetChildrenFunc)         (GObject            *container);

/**
 * GladeAddChildFunc:
 * @parent: A #GObject container
 * @child: A #GObject child
 *
 * Called to add @child to @parent.
 */
typedef void   (* GladeAddChildFunc)            (GObject            *parent,
						 GObject            *child);
/**
 * GladeRemoveChildFunc:
 * @parent: A #GObject container
 * @child: A #GObject child
 *
 * Called to remove @child from @parent.
 */
typedef void   (* GladeRemoveChildFunc)         (GObject            *parent,
						 GObject            *child);

/**
 * GladeReplaceChildFunc:
 * @container: A #GObject container
 * @old: The old #GObject child
 * @new: The new #GObject child to take its place
 * 
 * Called to swap placeholders with project objects
 * in containers.
 */
typedef void   (* GladeReplaceChildFunc)        (GObject            *container,  
						 GObject            *old,
						 GObject            *new);

/**
 * GladePostCreateFunc:
 * @object: a #GObject
 * @reason: a #GladeCreateReason
 *
 * This function is called exactly once for any project object
 * instance and can be for any #GladeCreateReason.
 */
typedef void   (* GladePostCreateFunc)          (GObject            *object,
						 GladeCreateReason   reason);

/**
 * GladeGetInternalFunc:
 * @parent: A #GObject composite object
 * @name: A string identifier
 * @child: A return location for a #GObject
 *
 * Called to lookup @child in composite object @parent by @name.
 */
typedef void   (* GladeGetInternalFunc)         (GObject            *parent,
						 const gchar        *name,
						 GObject           **child);


/**
 * GladeEditorLaunchFunc:
 * @object: A #GObject
 *
 * Called to launch a custom editor for @object
 */
typedef void   (* GladeEditorLaunchFunc)        (GObject            *object);


/* GladeWidgetClass contains all the information we need regarding an widget
 * type. It is also used to store information that has been loaded to memory
 * for that object like the icon/mask.
 */
struct _GladeWidgetClass
{
	GType type;          /* GType of the widget */

	gchar *name;         /* Name of the widget, for example GtkButton */

	gchar *catalog;      /* The name of the widget catalog this class
			      * was declared by.
			      */

	gchar *book;         /* Devhelp search namespace
			      */

	GdkPixbuf *large_icon;     /* The 22x22 icon for the widget */

	GdkPixbuf *small_icon;     /* The 16x16 icon for the widget */


	GdkCursor *cursor;         /* a cursor for inserting widgets */


	gboolean fixed;      /* If this is a GtkContainer, use free-form
			      * placement with drag/resize/paste at mouse...
			      */

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

	GList *signals;     /* List of GladeSignalClass objects */


	GList *children;    /* List of GladeSupportedChild objects */

        GList *child_packings; /* Default packing property values */

	GModule *module;	/* Module with the (optional) special functions
				 * needed for placeholder_replace, post_create_function
				 * and the set & get functions of the properties
				 * of this class.
				 */
				 
	gboolean toplevel;	/* If this class is toplevel */
	
	/* Executed after widget creation: it takes care of creating the
	 * GladeWidgets associated with internal children. It's also the place
	 * to set sane defaults, e.g. set the size of a window.
	 */
	GladePostCreateFunc           post_create_function;

	/* Retrieves the the internal child of the given name.
	 */
	GladeGetInternalFunc          get_internal_child;

	/* Entry point for custom editors.
	 */
	GladeEditorLaunchFunc         launch_editor;
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
	GladeGetChildrenFunc          get_children;     /* Returns a list of direct children for
							 * this support type.
							 */
	
	GladeChildSetPropertyFunc     set_property; /* Sets/Gets a packing property */
	GladeChildGetPropertyFunc     get_property; /* for this child */
	
	GladeReplaceChildFunc         replace_child;  /* This method replaces a 
						       * child widget with
						       * another one: it's used to
						       * replace a placeholder with
						       * a widget and viceversa.
						       */

	gchar                        *special_child_type; /* Special case code for children that
							   * are special children (like notebook tab 
							   * widgets for example).
							   */
};


/* GladeSignalClass contains all the info we need for a given signal, such as
 * the signal name, and maybe more in the future 
 */
struct _GladeSignalClass
{
	GSignalQuery query;

	const gchar *name;         /* Name of the signal, eg clicked */
	gchar       *type;         /* Name of the object class that this signal belongs to
				    * eg GtkButton */

};

#define glade_widget_class_create_widget(class, query, ...) \
    (glade_widget_class_create_widget_real (query, "class", class, __VA_ARGS__));
 
LIBGLADEUI_API
GladeWidgetClass    *glade_widget_class_new                (GladeXmlNode     *class_node,
							    const gchar      *catname,
							    const gchar      *library,
							    const gchar      *domain,
							    const gchar      *book);
LIBGLADEUI_API 
GladeWidget         *glade_widget_class_create_internal    (GladeWidget      *parent,
							    GObject          *internal_object,
							    const gchar      *internal_name,
							    const gchar      *parent_name,
							    gboolean          anarchist,
							    GladeCreateReason reason);
LIBGLADEUI_API
GladeWidget         *glade_widget_class_create_widget_real (gboolean          query, 
							    const gchar      *first_property,
							    ...);
LIBGLADEUI_API
void                 glade_widget_class_free               (GladeWidgetClass *widget_class);
LIBGLADEUI_API
GladeWidgetClass    *glade_widget_class_get_by_name        (const char       *name);
LIBGLADEUI_API
GladeWidgetClass    *glade_widget_class_get_by_type        (GType             type);
LIBGLADEUI_API
GList               *glade_widget_class_get_derived_types  (GType             type);
LIBGLADEUI_API
GType 	            glade_widget_class_get_type           (GladeWidgetClass *class);
LIBGLADEUI_API
void                 glade_widget_class_dump_param_specs   (GladeWidgetClass *class);
LIBGLADEUI_API
GladePropertyClass  *glade_widget_class_get_property_class (GladeWidgetClass *class,
							    const gchar      *name);
LIBGLADEUI_API
GladeSupportedChild *glade_widget_class_get_child_support  (GladeWidgetClass *class,
							    GType             child_type);
LIBGLADEUI_API
GParameter          *glade_widget_class_default_params             (GladeWidgetClass *class,
								    gboolean          construct,
								    guint            *n_params);
LIBGLADEUI_API
void                 glade_widget_class_container_add              (GladeWidgetClass *class,
								    GObject          *container,
								    GObject          *child);
LIBGLADEUI_API
void                 glade_widget_class_container_remove           (GladeWidgetClass *class,
								    GObject          *container,
								    GObject          *child);
LIBGLADEUI_API
gboolean             glade_widget_class_container_has_child        (GladeWidgetClass *class,
								    GObject          *container,
								    GObject          *child);
LIBGLADEUI_API
GList               *glade_widget_class_container_get_children     (GladeWidgetClass *class,
								    GObject          *container);
LIBGLADEUI_API
void                 glade_widget_class_container_set_property     (GladeWidgetClass *class,
								    GObject      *container,
								    GObject      *child,
								    const gchar  *property_name,
								    const GValue *value);
LIBGLADEUI_API
void                 glade_widget_class_container_get_property     (GladeWidgetClass *class,
								    GObject      *container,
								    GObject      *child,
								    const gchar  *property_name,
								    GValue       *value);
LIBGLADEUI_API
void                 glade_widget_class_container_replace_child    (GladeWidgetClass *class,
								    GObject      *container,
								    GObject      *old,
								    GObject      *new);
LIBGLADEUI_API
gboolean             glade_widget_class_contains_extra             (GladeWidgetClass *class);
LIBGLADEUI_API
gboolean             glade_widget_class_query                      (GladeWidgetClass *class);
LIBGLADEUI_API
GladePackingDefault *glade_widget_class_get_packing_default        (GladeWidgetClass *child_class,
								    GladeWidgetClass *container_class,
								    const gchar *propert_id);

#define glade_widget_class_from_pclass(pclass) \
    ((pclass) ? (GladeWidgetClass *)((GladePropertyClass *)(pclass))->handle : NULL)

G_END_DECLS

#endif /* __GLADE_WIDGET_CLASS_H__ */
