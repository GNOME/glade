/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_WIDGET_CLASS_H__
#define __GLADE_WIDGET_CLASS_H__

G_BEGIN_DECLS

typedef enum {
	GLADE_TOPLEVEL        = 1 << 2,
	GLADE_ADD_PLACEHOLDER = 1 << 3,
} GladeWidgetClassFlags;

#define GLADE_WIDGET_CLASS(gwc) ((GladeWidgetClass *) gwc)

#define GLADE_WIDGET_FLAGS(gw)           ((GLADE_WIDGET(gw)->class)->flags)
#define GLADE_WIDGET_TOPLEVEL(gw)        ((GLADE_WIDGET_FLAGS(gw) & GLADE_TOPLEVEL) != 0)
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
struct _GladeWidgetClass {

	gchar *name;         /* Name of the widget, for example GtkButton */
	gchar *icon;         /* Name of the icon without the prefix, for example
				"button" */
	GdkPixmap *pixmap;   /* The loaded pixmap for the icon of this widget type */
	GdkBitmap *mask;     /* The mask for the loaded pixmap */
	GdkPixbuf *pixbuf;  /* Temp for glade-project-view-tree. */

	gchar *generic_name; /* Use to generate names of new widgets, for
			      * example "button" so that we generate button1,
			      * button2, buttonX ..
			      */

	gint flags;          /* See GladeWidgetClassFlags above */

	GList *properties;   /* List of GladePropertyClass objects.
			      * [see glade-property.h ] this list contains
			      * properties about the widget that we are going
			      * to modify. Like "title", "label", "rows" .
			      * Each property creates an input in the propety
			      * editor.
			      */

	GList *signals;     /* List of GladeWidgetClassSignal objects */

	void (*placeholder_replace) (GladePlaceholder *placeholder,
				     GladeWidget *widget,
				     GladeWidget *parent);
};

/* GladeWidgetClassSignal contains all the info we need for a given signal, such as
 * the signal name, and maybe more in the future 
 */
struct _GladeWidgetClassSignal {

	gchar *name;         /* Name of the signal, eg clicked */
	gchar *type;         /* Name of the object class that this signal belongs to
			      * eg GtkButton */
};

GladeWidgetClass * glade_widget_class_new_from_name (const gchar *name);

const gchar * glade_widget_class_get_name (GladeWidgetClass *class);
gboolean      glade_widget_class_has_queries (GladeWidgetClass *class);

G_END_DECLS

#endif /* __GLADE_WIDGET_CLASS_H__ */
