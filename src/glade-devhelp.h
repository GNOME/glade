/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __DH_WINDOW_H__
#define __DH_WINDOW_H__

#include <glib-object.h>
#include <gtk/gtkwindow.h>

#define GLADE_TYPE_DHWIDGET            (glade_dh_widget_get_type ())
#define GLADE_DHWIDGET(obj)            (GTK_CHECK_CAST ((obj), GLADE_TYPE_DHWIDGET, GladeDhWidget))
#define GLADE_DHWIDGET_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GLADE_TYPE_DHWIDGET, GladeDhWidgetClass))
#define GLADE_IS_DHWIDGET(obj)         (GTK_CHECK_TYPE ((obj), GLADE_TYPE_DHWIDGET))
#define GLADE_IS_DHWIDGET_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), GLADE_TYPE_DHWIDGET))

typedef struct _GladeDhWidget       GladeDhWidget;
typedef struct _GladeDhWidgetClass  GladeDhWidgetClass;
typedef struct _GladeDhWidgetPriv   GladeDhWidgetPriv;

struct _GladeDhWidget {
        GtkBox     parent;

	GladeDhWidgetPriv *priv;
};

struct _GladeDhWidgetClass {
        GtkVBoxClass    parent_class;
};

GType            glade_dh_widget_get_type     (void) G_GNUC_CONST;

GtkWidget *      glade_dh_widget_new          (void);
GList     *      glade_dh_get_hbuttons        (GladeDhWidget *widget);
void             glade_dh_widget_search       (GladeDhWidget *widget,
					       const gchar   *str);

#endif /* __DH_WINDOW_H__ */
