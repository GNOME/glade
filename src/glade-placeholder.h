/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PLACEHOLDER_H__
#define __GLADE_PLACEHOLDER_H__

G_BEGIN_DECLS

void          glade_placeholder_add (GladeWidgetClass *class,
				     GladeWidget *widget,
				     GladePropertyQueryResult *result);

GladePlaceholder * glade_placeholder_new (GladeWidget *parent);

GladeWidget * glade_placeholder_get_parent (GladePlaceholder *placeholder);


/* Hacks */
void          glade_placeholder_replace_box       (GtkWidget *current,
						   GtkWidget *new,
						   GtkWidget *container);
void          glade_placeholder_replace_table     (GtkWidget *current,
						   GtkWidget *new,
						   GtkWidget *container);
void          glade_placeholder_replace_container (GtkWidget *current,
						   GtkWidget *new,
						   GtkWidget *container);
void          glade_placeholder_replace_notebook  (GtkWidget *current,
						   GtkWidget *new,
						   GtkWidget *container);


G_END_DECLS

#endif /* __GLADE_PLACEHOLDER_H__ */
