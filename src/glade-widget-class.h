/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_WIDGET_CLASS_H__
#define __GLADE_WIDGET_CLASS_H__

#include <glib-object.h>
#include <gmodule.h>
#include <gtk/gtkwidget.h>
#include "glade-types.h"

G_BEGIN_DECLS


typedef enum {
	GLADE_TOPLEVEL        = 1 << 2,
	GLADE_ADD_PLACEHOLDER = 1 << 3,
} GladeWidgetClassFlags;

#define GLADE_WIDGET_CLASS(gwc) ((GladeWidgetClass *) gwc)
#define GLADE_IS_WIDGET_CLASS(gwc) (gwc != NULL)

#define GLADE_WIDGET_FLAGS(gw)           ((GLADE_WIDGET(gw)->widget_class)->flags)
#define GLADE_WIDGET_IS_TOPLEVEL(gw)     ((GLADE_WIDGET_FLAGS(gw) & GLADE_TOPLEVEL) != 0)
#define GLADE_WIDGET_ADD_PLACEHOLDER(gw) ((GLADE_WIDGET_FLAGS(gw) & GLADE_ADD_PLACEHOLDER) != 0)

#define GLADE_WIDGET_CLASS_FLAGS(gwc)           ((GLADE_WIDGET_CLASS(gwc)->flags))
#define GLADE_WIDGET_CLASS_TOPLEVEL(gwc)        ((GLADE_WIDGET_CLASS_FLAGS(gwc) & GLADE_TOPLEVEL) != 0)
#define GLADE_WIDGET_CLASS_ADD_PLACEHOLDER(gwc) ((GLADE_WIDGET_CLASS_FLAGS(gwc) & GLADE_ADD_PLACEHOLDER) != 0)

#define GLADE_WIDGET_CLASS_SET_FLAGS(gwc,flag)	  G_STMT_START{ (GLADE_WIDGET_CLASS_FLAGS (gwc) |= (flag)); }G_STMT_END
#define GLADE_WIDGET_CLASS_UNSET_FLAGS(gwc,flag)  G_STMT_START{ (GLADE_WIDGET_CLASS_FLAGS (gwc) &= ~(flag)); }G_STMT_END


/* GladeWidgetClass contains all the information we need regarding an widget
 * type. It is also used to store information that has been loaded to memory
 * for that object like the icon/mask.
 */
struct _GladeWidgetClass
{
	GType type;          /* GType of the widget */

	gchar *name;         /* Name of the widget, for example GtkButton */

	GtkWidget *icon;     /* The GtkImage icon for the widget */

	gchar *generic_name; /* Use to generate names of new widgets, for
			      * example "button" so that we generate button1,
			      * button2, buttonX ..
			      */
	gchar *palette_name; /* Name used in the palette */

	gint flags;          /* See GladeWidgetClassFlags above */

	GList *properties;   /* List of GladePropertyClass objects.
			      * [see glade-property.h ] this list contains
			      * properties about the widget that we are going
			      * to modify. Like "title", "label", "rows" .
			      * Each property creates an input in the propety
			      * editor.
			      */

	GList *signals;     /* List of GladeWidgetClassSignal objects */

	GList *child_properties;   /* List of GladePropertyClass objects
				    * representing child_properties of a
				    * GtkContainer (the list is empty if the
				    * class isn't a container).
				    * Note that the actual GladeProperty
				    * corresponding to each class end up
				    * in the packing_properties list of
				    * each _child_ of the container and thus
				    * are edited when the _child_ is selected.
				    */

	GModule *module;	/* Module with the (optional) special functions
				 * needed for placeholder_replace, post_create_function
				 * and the set & get functions of the properties
				 * of this class.
				 */

	gboolean in_palette;

	/* This method replaces a child widget with another one: it's used to replace
	 * a placeholder with a widget and viceversa.
	 */
	void (*replace_child) (GtkWidget *current,
			       GtkWidget *new,
			       GtkWidget *container);

	/* Executed after widget creation: it takes care of creating the
	 * GladeWidgets associated with internal children. It's also the place
	 * to set sane defaults, e.g. set the size of a window.
	 */
	void (*post_create_function) (GObject *gobject);

	/* Executed before setting the properties of the widget to its initial
	 * value as specified in the xml file */
	void (*pre_create_function) (GObject *gobject);

	/* If the widget is a container, this method takes care of adding the
	 * needed placeholders.
	 */
	void (*fill_empty) (GtkWidget *widget);

	/* Retrieves the the internal child of the given name.
	 */
	void (*get_internal_child) (GtkWidget *parent,
				    const gchar *name,
				    GtkWidget **child);

	/* Is property_class of ancestor applicable to the widget? Usually property_class only
	 * applies to direct children of a given ancestor */
	gboolean (*child_property_applies) (GtkWidget *ancestor,
					    GtkWidget *widget,
					    const char *property_id);
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


GladeWidgetClass *glade_widget_class_new (const char *name, const char *generic_name, const char *palette_name, const char *base_filename, const char *base_library);
GladeWidgetClass *glade_widget_class_new_from_node (GladeXmlNode *node);
void glade_widget_class_free (GladeWidgetClass *widget_class);
GladeWidgetClass *glade_widget_class_get_by_name (const char *name);

const gchar *glade_widget_class_get_name (GladeWidgetClass *class);
GType 	     glade_widget_class_get_type (GladeWidgetClass *class);
gboolean     glade_widget_class_has_queries (GladeWidgetClass *class);

gboolean     glade_widget_class_is (GladeWidgetClass *class, const char *name);

void        glade_widget_class_dump_param_specs (GladeWidgetClass *class);
gboolean    glade_widget_class_has_property (GladeWidgetClass *class, const gchar *name);


G_END_DECLS

#endif /* __GLADE_WIDGET_CLASS_H__ */
