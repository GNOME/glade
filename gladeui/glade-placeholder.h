/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PLACEHOLDER_H__
#define __GLADE_PLACEHOLDER_H__

#include <gladeui/glade-widget.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_PLACEHOLDER            (glade_placeholder_get_type ())
#define GLADE_PLACEHOLDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PLACEHOLDER, GladePlaceholder))
#define GLADE_PLACEHOLDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PLACEHOLDER, GladePlaceholderClass))
#define GLADE_IS_PLACEHOLDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PLACEHOLDER))
#define GLADE_IS_PLACEHOLDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PLACEHOLDER))
#define GLADE_PLACEHOLDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_PLACEHOLDER, GladePlaceholderClass))

#define GLADE_PLACEHOLDER_WIDTH_REQ    20
#define GLADE_PLACEHOLDER_HEIGHT_REQ   20

typedef struct _GladePlaceholder   GladePlaceholder;
typedef struct _GladePlaceholderClass GladePlaceholderClass;


struct _GladePlaceholder
{
	GtkWidget widget;

	GdkPixmap *placeholder_pixmap;
	
	GList *packing_actions;
};

struct _GladePlaceholderClass
{
	GtkWidgetClass parent_class;
};


GType        glade_placeholder_get_type   (void) G_GNUC_CONST;

GtkWidget   *glade_placeholder_new        (void);

GladeWidget *glade_placeholder_get_parent (GladePlaceholder *placeholder);

G_END_DECLS

#endif /* __GLADE_PLACEHOLDER_H__ */
