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


LIBGLADEUI_API void		glade_util_widget_set_tooltip	(GtkWidget *widget, const gchar *str);
LIBGLADEUI_API GType		glade_util_get_type_from_name	(const gchar *name);
LIBGLADEUI_API GParamSpec      *glade_utils_get_pspec_from_funcname (const gchar *funcname);
LIBGLADEUI_API void		glade_util_ui_warn		(GtkWidget *parent, const gchar *warning);
LIBGLADEUI_API void		glade_util_flash_message	(GtkWidget *statusbar, 
								 guint context_id,
								 gchar *format, ...);

/* This is a GCompareFunc for comparing the labels of 2 stock items, ignoring
   any '_' characters. It isn't particularly efficient. */
LIBGLADEUI_API gint              glade_util_compare_stock_labels (gconstpointer a, gconstpointer b);

LIBGLADEUI_API void              glade_util_hide_window		(GtkWindow *window);
LIBGLADEUI_API gchar            *glade_util_gtk_combo_func	(gpointer data);
LIBGLADEUI_API gpointer          glade_util_gtk_combo_find	(GtkCombo *combo);

LIBGLADEUI_API GtkWidget        *glade_util_file_dialog_new (const gchar *title,
							   GtkWindow *parent,
							   GladeUtilFileDialogType action);
LIBGLADEUI_API void              glade_util_replace (char *str, char a, char b);
LIBGLADEUI_API gchar            *glade_util_duplicate_underscores (const char *name);

LIBGLADEUI_API void              glade_util_add_selection    (GtkWidget *widget);
LIBGLADEUI_API void              glade_util_remove_selection (GtkWidget *widget);
LIBGLADEUI_API gboolean	         glade_util_has_selection    (GtkWidget *widget);
LIBGLADEUI_API void              glade_util_clear_selection  (void);
LIBGLADEUI_API GList            *glade_util_get_selection    (void);
LIBGLADEUI_API GladePlaceholder *glade_util_selected_placeholder (void);

LIBGLADEUI_API void              glade_util_queue_draw_nodes (GdkWindow *window);

LIBGLADEUI_API GladeWidget      *glade_util_get_parent (GtkWidget *w);
LIBGLADEUI_API GList            *glade_util_container_get_all_children (GtkContainer *container);

LIBGLADEUI_API GList            *glade_util_uri_list_parse (const gchar* uri_list);

LIBGLADEUI_API gboolean          glade_util_gtkcontainer_relation     (GladeWidget *parent,
								       GladeWidget *widget);
LIBGLADEUI_API gboolean          glade_util_any_gtkcontainer_relation (GladeWidget *parent, 
								       GList       *widgets);

LIBGLADEUI_API gboolean          glade_util_widget_pastable       (GladeWidget *child,  
								   GladeWidget *parent);

LIBGLADEUI_API void              glade_util_paste_clipboard       (GladePlaceholder *placeholder,
								   GladeWidget      *parent);
LIBGLADEUI_API void              glade_util_cut_selection         (void);
LIBGLADEUI_API void              glade_util_copy_selection        (void);
LIBGLADEUI_API void              glade_util_delete_selection      (void);
LIBGLADEUI_API void              glade_util_delete_clipboard      (void);

LIBGLADEUI_API gint              glade_util_count_placeholders    (GladeWidget *parent);

LIBGLADEUI_API GtkTreeIter      *glade_util_find_iter_by_widget   (GtkTreeModel *model,
								   GladeWidget  *findme,
								   gint          column);

LIBGLADEUI_API gboolean          glade_util_basenames_match       (const gchar  *path1,
								   const gchar  *path2);

LIBGLADEUI_API GList            *glade_util_purify_list           (GList        *list);

G_END_DECLS

#endif /* __GLADE_UTILS_H__ */
