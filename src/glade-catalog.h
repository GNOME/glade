#ifndef __GLADE_CATALOG_H__
#define __GLADE_CATALOG_H__

G_BEGIN_DECLS

/* The GladeCatalog is just a list of widgets. We are going to use
   a different catalog for each group of widgets like gtkbasic
   gnome, gnome-db etc. It is also used to group widgets in the
   palette. */

struct _GladeCatalog
{
	GList *names;   /* Contains the list of names that we are going
				  * to try to load. This is basically a memory
				  * representation of catalog.xml
				  */
	GList *widgets; /* Contains the list of GladeWidgetClass objects 
				  * that where successfully loaded from disk
				  */
};

GladeCatalog * glade_catalog_load (void);

G_END_DECLS

#endif /* __GLADE_CATALOG_H__ */
