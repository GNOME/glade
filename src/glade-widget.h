/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_WIDGET_H__
#define __GLADE_WIDGET_H__

#include "glade-types.h"

G_BEGIN_DECLS


#define GLADE_WIDGET(w) ((GladeWidget *)w)
#define GLADE_IS_WIDGET(w) (w != NULL)

/* A GladeWidget is an instance of a GladeWidgetClass.
 * Each GtkWidget in the project has an associated GladeWidget.
 */
struct _GladeWidget
{
	GladeWidgetClass *class; /* The class of the widget.
				  * [see glade-widget-class.h ]
				  */

	GladeProject *project; /* A pointer to the project that this
				* widget belongs to. Do we really need it ?
				*/

	gchar *name; /* The name of the widet. For example window1 or
		      * button2. This is a unique name and is the one
		      * used when loading widget with libglade
		      */

	gchar *internal; /* If the widget is an internal child of 
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

	GList *signals;      /* A list of GladeSignals */
};


gchar *glade_widget_new_name (GladeProject *project, GladeWidgetClass *class);
void glade_widget_set_contents (GladeWidget *widget);
void glade_widget_connect_signals (GladeWidget *widget);

void glade_widget_set_packing_properties (GladeWidget *widget,
					  GladeWidget *container);

GladeWidget *glade_widget_new_from_class (GladeWidgetClass *class,
					  GladeProject *project);

GladeWidget *glade_widget_new_for_internal_child (GladeWidgetClass *class,
						  GladeWidget *parent,
						  GtkWidget *widget,
						  const gchar *internal);

const gchar *glade_widget_get_name  (GladeWidget *widget);

GladeWidgetClass *glade_widget_get_class (GladeWidget *widget);

GladeProperty *glade_widget_get_property_by_class (GladeWidget *widget,
						   GladePropertyClass *property_class);

GladeProperty *glade_widget_get_property_by_id (GladeWidget *widget,
						const gchar *id,
						gboolean packing);

void glade_widget_set_name (GladeWidget *widget, const gchar *name);

GladeWidget *glade_widget_clone (GladeWidget *widget);

void glade_widget_replace_with_placeholder (GladeWidget *widget,
					    GladePlaceholder *placeholder);

GladeWidget *glade_widget_get_from_gtk_widget (GtkWidget *widget);

GladeWidget *glade_widget_get_parent (GladeWidget *widget);

/* Widget signal*/
GList *glade_widget_find_signal (GladeWidget *widget, GladeSignal *signal);
void glade_widget_add_signal (GladeWidget *widget, GladeSignal *signal);
void glade_widget_remove_signal (GladeWidget *widget, GladeSignal *signal);


/* Xml saving & reading */
GladeXmlNode *glade_widget_write (GladeXmlContext *context, GladeWidget *widget);
GladeWidget *glade_widget_new_from_node (GladeXmlNode *node, GladeProject *project);


G_END_DECLS

#endif /* __GLADE_WIDGET_H__ */
