/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_UTILS_H__
#define __GLADE_UTILS_H__

G_BEGIN_DECLS

/* Function is a GNU extension */
#ifndef __GNUC__
#define __FUNCTION__   ""
#endif

#define glade_implement_me() g_print ("Implement me : %s %d %s\n", __FILE__, __LINE__, __FUNCTION__); 

gboolean glade_util_path_is_writable (const gchar *full_path);

void glade_util_widget_set_tooltip (GtkWidget *widget, const gchar *str);

G_END_DECLS

#endif /* __GLADE_UTILS_H__ */
