/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PACKING_H__
#define __GLADE_PACKING_H__

G_BEGIN_DECLS

typedef struct _GladePackingProperties GladePackingProperties;

struct _GladePackingProperties {
	GladeWidgetClass *container_class;
        GHashTable *properties; /* Contains a gvalue */
};

void glade_packing_init (void);
void glade_packing_add_properties (GladeWidget *widget);
GladePackingProperties * glade_packing_property_get_from_class (GladeWidgetClass *class,
								GladeWidgetClass *container_class);


G_END_DECLS

#endif /* __GLADE_PACKING_H__ */


