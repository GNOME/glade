/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PLACEHOLDER_H__
#define __GLADE_PLACEHOLDER_H__

G_BEGIN_DECLS


GladePlaceholder * glade_placeholder_new ();

GladeWidget * glade_placeholder_get_parent (GladePlaceholder *placeholder);

void glade_placeholder_add_methods_to_class (GladeWidgetClass *class);

void glade_placeholder_replace_with_widget (GladePlaceholder *placeholder,
					    GladeWidget *widget);

/* remember to convert GladePlaceholder to a true GtkWidget, and convert this
 * function to a macro */
gboolean GLADE_IS_PLACEHOLDER (GtkWidget *widget);


G_END_DECLS

#endif /* __GLADE_PLACEHOLDER_H__ */
