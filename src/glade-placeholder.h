/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PLACEHOLDER_H__
#define __GLADE_PLACEHOLDER_H__

G_BEGIN_DECLS

void          glade_placeholder_add (GladeWidgetClass *class,
				     GladeWidget *widget);

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

#if 1
gboolean glade_placeholder_is (GtkWidget *widget);
#endif

void glade_placeholder_remove_all (GtkWidget *widget);

void glade_placeholder_fill_empty (GtkWidget *widget);

G_END_DECLS

#endif /* __GLADE_PLACEHOLDER_H__ */
