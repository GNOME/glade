/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PLACEHOLDER_H__
#define __GLADE_PLACEHOLDER_H__

G_BEGIN_DECLS

void          glade_placeholder_add (GladeWidgetClass *class,
				     GladeWidget *widget,
				     gint rows, gint columns);

void          glade_placeholder_add_with_result (GladeWidgetClass *class,
						 GladeWidget *widget,
						 GladePropertyQueryResult *result);

GladePlaceholder * glade_placeholder_new (GladeWidget *parent);

GladeWidget * glade_placeholder_get_parent (GladePlaceholder *placeholder);

void glade_placeholder_add_methods_to_class (GladeWidgetClass *class);

void glade_placeholder_replace (GladePlaceholder *placeholder,
				GladeWidget *parent,
				GladeWidget *child);

GladePlaceholder * glade_placeholder_get_from_properties (GladeWidget *parent,
							  GHashTable *properties);

G_END_DECLS

#endif /* __GLADE_PLACEHOLDER_H__ */
