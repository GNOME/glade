/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PLACEHOLDER_H__
#define __GLADE_PLACEHOLDER_H__

G_BEGIN_DECLS

void          glade_placeholder_add (GladeWidgetClass *class,
				     GladeWidget *widget,
				     GladePropertyQueryResult *result);

GladeWidget * glade_placeholder_get_parent (GladePlaceholder *placeholder);


/* Hacks */
void          glade_placeholder_replace_box       (GladePlaceholder *place_holder,
						   GladeWidget *widget,
						   GladeWidget *parent);
void          glade_placeholder_replace_table     (GladePlaceholder *placeholder,
						   GladeWidget *widget,
						   GladeWidget *parent);
void          glade_placeholder_replace_container (GladePlaceholder *placeholder,
						   GladeWidget *widget,
						   GladeWidget *parent);

G_END_DECLS

#endif /* __GLADE_PLACEHOLDER_H__ */
