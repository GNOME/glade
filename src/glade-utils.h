/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_UTILS_H__
#define __GLADE_UTILS_H__

G_BEGIN_DECLS


typedef enum   _GladeUtilFileDialogType GladeUtilFileDialogType;

enum _GladeUtilFileDialogType
{
        GLADE_FILE_DIALOG_ACTION_OPEN,
        GLADE_FILE_DIALOG_ACTION_SAVE
};

#define glade_implement_me() g_print ("Implement me : %s %d %s\n", __FILE__, __LINE__, G_GNUC_FUNCTION);


void		glade_util_widget_set_tooltip	(GtkWidget *widget, const gchar *str);
GType		glade_util_get_type_from_name	(const gchar *name);
GParamSpec     *glade_utils_get_pspec_from_funcname (const gchar *funcname);
void		glade_util_ui_warn		(GtkWidget *parent, const gchar *warning);
void		glade_util_flash_message	(GtkWidget *statusbar, 
						 guint context_id,
						 gchar *format, ...);

/* This is a GCompareFunc for comparing the labels of 2 stock items, ignoring
   any '_' characters. It isn't particularly efficient. */
gint		glade_util_compare_stock_labels (gconstpointer a, gconstpointer b);

void		glade_util_hide_window		(GtkWindow *window);
gchar		*glade_util_gtk_combo_func	(gpointer data);
gpointer	glade_util_gtk_combo_find	(GtkCombo *combo);

GtkWidget       *glade_util_file_dialog_new (const gchar *title, GtkWindow *parent, GladeUtilFileDialogType action);
gchar           *glade_util_file_dialog_get_filename (GtkWidget *file_dialog);
void             glade_util_file_dialog_set_filename (GtkWidget *file_dialog, gchar *filename);

void		glade_util_replace (char *str, char a, char b);
char		*glade_util_duplicate_underscores (const char *name);

void		glade_util_delete_selection (GladeProject *project);

void		glade_util_add_selection    (GObject   *object);
void		glade_util_remove_selection (GObject   *object);
gboolean	glade_util_has_selection    (GObject   *object);
void		glade_util_queue_draw_nodes (GdkWindow *window);

GladeWidget	*glade_util_get_parent (GtkWidget *w);
GList           *glade_util_container_get_all_children (GtkContainer *container);
void		glade_util_object_set_property (GObject *object, GladeProperty *property);

GList           *glade_util_uri_list_parse (const gchar* uri_list);


G_END_DECLS

#endif /* __GLADE_UTILS_H__ */
