/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_UTILS_H__
#define __GLADE_UTILS_H__

G_BEGIN_DECLS

/* Function is a GNU extension */
#ifndef __GNUC__
#define __FUNCTION__   ""
#endif

#define glade_implement_me() g_print ("Implement me : %s %d %s\n", __FILE__, __LINE__, __FUNCTION__); 

typedef enum
{
	GladeEscCloses,
	GladeEscDestroys
} GladeEscAction;

void     glade_util_widget_set_tooltip	(GtkWidget *widget, const gchar *str);
GType    glade_util_get_type_from_name	(const gchar *name);
void     glade_util_ui_warn		(const gchar *warning);

/* This is a GCompareFunc for comparing the labels of 2 stock items, ignoring
   any '_' characters. It isn't particularly efficient. */
gint     glade_util_compare_stock_labels (gconstpointer a, gconstpointer b);

gpointer glade_util_gtk_combo_find	(GtkCombo *combo);
gint	 glade_util_hide_window_on_delete (GtkWidget *widget, GdkEvent *event, gpointer data);
gint	 glade_util_hide_window	(GtkWidget *widget);
gint	 glade_util_check_key_is_esc	(GtkWidget *widget, GdkEventKey *event, gpointer data);
gchar	*glade_util_gtk_combo_func	(gpointer data);
gpointer glade_util_gtk_combo_find	(GtkCombo *combo);

GtkWidget *glade_util_file_selection_new (const gchar *title, GtkWindow *parent);

G_END_DECLS

#endif /* __GLADE_UTILS_H__ */
