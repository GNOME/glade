/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_CATALOG_H__
#define __GLADE_CATALOG_H__

G_BEGIN_DECLS

/* The GladeCatalog is just a list of widgets. We are going to use
   a different catalog for each group of widgets like gtkbasic
   gnome, gnome-db etc. It is also used to group widgets in the
   palette. */

struct _GladeCatalog
{
	gchar *title;	/* Title for this catalog */
	GList *widget_classes; /* list of widget classes contained on this catalog */
};

void glade_catalog_delete (GladeCatalog *catalog);
GList * glade_catalog_load_all (void);
GList *glade_catalog_get_widgets (void); /* This probally should be in glade-widget-class.c */
const char *glade_catalog_get_title (GladeCatalog *catalog);
GList *glade_catalog_get_widget_classes (GladeCatalog *catalog);

G_END_DECLS

#endif /* __GLADE_CATALOG_H__ */
