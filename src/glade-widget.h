/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_WIDGET_H__
#define __GLADE_WIDGET_H__

G_BEGIN_DECLS

#define GLADE_WIDGET(w) ((GladeWidget *)w)
#define GLADE_IS_WIDGET(w) (w != NULL)

/* A GladeWidget is an instance of a GladeWidgetClass. For every widget
 * in the project there is a GladeWidget
 */

struct _GladeWidget {
	
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

	GtkWidget *widget; /* A pointer to the widget that was created.
			    * and is shown as a "view" of the GladeWidget.
			    * This widget is updated as the properties are
			    * modified for the GladeWidget. We should not
			    * destroy this widgets once created, we should
			    * just hide them
			    */
	
	GList * properties; /* A list of GladeProperty. A GladeProperty is an
			     * instance of a GladePropertyClass. If a
			     * GladePropertyClass for a gtkbutton is label, its
			     * property is "Ok". 
			     */

	GList *signals;      /* A list of GladeWidgetSignals */

	/* Tree Structure */
	GladeWidget *parent; /* The parent of this widget, NULL if this is a
			      * toplevel widget.
			      */
	GList *children;     /* A list of GladeWidget childrens of this widget.
			      */

	gboolean selected;
};

/* GladeWidgetSignal is a structure that holds information about a signal a
 * widget wants for handle / listen for. 
 */
struct _GladeWidgetSignal {
	gchar *name;         /* Signal name eg "clicked" */
	gchar *handler;      /* Handler function eg "gtk_main_quit" */
	gboolean after;      /* Connect after TRUE or FALSE */
};


GladeWidget * glade_widget_new_from_class (GladeProject *project,
					   GladeWidgetClass *class,
					   GladeWidget *widget);

const gchar *      glade_widget_get_name  (GladeWidget *widget);
GladeWidgetClass * glade_widget_get_class (GladeWidget *widget);
GladeProperty *    glade_widget_get_property_from_class (GladeWidget *widget,
							 GladePropertyClass *property_class);

void glade_widget_set_name (GladeWidget *widget, const gchar *name);

void glade_widget_unselect (GladeWidget *widget);
void glade_widget_select (GladeWidget *widget);

G_END_DECLS

#endif /* __GLADE_WIDGET_H__ */
