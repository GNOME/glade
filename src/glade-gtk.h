/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_GTK_H__
#define __GLADE_GTK_H__

G_BEGIN_DECLS

gboolean glade_gtk_get_set_function_hack (GladePropertyClass *class, const gchar *name);
gboolean glade_gtk_get_get_function_hack (GladePropertyClass *class, const gchar *name);

/* big ugly hack... bad bad Joaquin... */
GtkWidget *glade_gtk_create_menu_editor_button (GladeWidget *item);

/* Remove !! */
gpointer glade_gtk_get_function (const gchar *name);

G_END_DECLS

#endif /* __GLADE_GTK_H__ */
