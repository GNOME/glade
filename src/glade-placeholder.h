/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PLACEHOLDER_H__
#define __GLADE_PLACEHOLDER_H__

#include <gdk/gdk.h>
#include <gdk/gdkpixmap.h>
#include <gtk/gtkwidget.h>
#include "glade-types.h"

G_BEGIN_DECLS


#define GLADE_TYPE_PLACEHOLDER            (glade_placeholder_get_type ())
#define GLADE_PLACEHOLDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PLACEHOLDER, GladePlaceholder))
#define GLADE_PLACEHOLDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PLACEHOLDER, GladePlaceholderClass))
#define GLADE_IS_PLACEHOLDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PLACEHOLDER))
#define GLADE_IS_PLACEHOLDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PLACEHOLDER))
#define GLADE_PLACEHOLDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_PLACEHOLDER, GladePlaceholderClass))


struct _GladePlaceholder
{
	GtkWidget widget;

	GdkPixmap *placeholder_pixmap;
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
