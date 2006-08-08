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

typedef enum 
{
	GLADE_UI_INFO,
	GLADE_UI_WARN,
	GLADE_UI_ERROR,
	GLADE_UI_ARE_YOU_SURE,
	GLADE_UI_YES_OR_NO
} GladeUIMessageType;


LIBGLADEUI_API
void		glade_util_widget_set_tooltip	(GtkWidget *widget, const gchar *str);
LIBGLADEUI_API
GType		glade_util_get_type_from_name	(const gchar *name);
LIBGLADEUI_API
GParamSpec      *glade_utils_get_pspec_from_funcname (const gchar *funcname);
LIBGLADEUI_API
gboolean         glade_util_ui_message           (GtkWidget *parent, 
						  GladeUIMessageType type,
						  const gchar *format, ...);
LIBGLADEUI_API
void		glade_util_flash_message	(GtkWidget *statusbar, 
						 guint context_id,
						 gchar *format, ...);

/* This is a GCompareFunc for comparing the labels of 2 stock items, ignoring
   any '_' characters. It isn't particularly efficient. */
LIBGLADEUI_API
gint              glade_util_compare_stock_labels (gconstpointer a, gconstpointer b);

LIBGLADEUI_API
void              glade_util_hide_window		(GtkWindow *window);
LIBGLADEUI_API
gchar            *glade_util_gtk_combo_func	(gpointer data);
LIBGLADEUI_API
gpointer          glade_util_gtk_combo_find	(GtkCombo *combo);

LIBGLADEUI_API
GtkWidget        *glade_util_file_dialog_new (const gchar *title,
					      GtkWindow *parent,
					      GladeUtilFileDialogType action);
LIBGLADEUI_API
void              glade_util_replace (gchar *str, gchar a, gchar b);
LIBGLADEUI_API
gchar            *glade_util_read_prop_name (const gchar *str);
LIBGLADEUI_API
gchar            *glade_util_duplicate_underscores (const gchar *name);

LIBGLADEUI_API
void              glade_util_add_selection    (GtkWidget *widget);
LIBGLADEUI_API
void              glade_util_remove_selection (GtkWidget *widget);
LIBGLADEUI_API
gboolean	         glade_util_has_selection    (GtkWidget *widget);
LIBGLADEUI_API
void              glade_util_clear_selection  (void);
LIBGLADEUI_API
GList            *glade_util_get_selection    (void);

LIBGLADEUI_API
void              glade_util_queue_draw_nodes (GdkWindow *window);

LIBGLADEUI_API
GladeWidget      *glade_util_get_parent (GtkWidget *w);
LIBGLADEUI_API
GList            *glade_util_container_get_all_children (GtkContainer *container);

LIBGLADEUI_API
GList            *glade_util_uri_list_parse (const gchar* uri_list);

LIBGLADEUI_API
gboolean          glade_util_gtkcontainer_relation     (GladeWidget *parent,
							GladeWidget *widget);
LIBGLADEUI_API
gboolean          glade_util_any_gtkcontainer_relation (GladeWidget *parent, 
							GList       *widgets);

LIBGLADEUI_API
gboolean          glade_util_widget_pastable       (GladeWidget *child,  
						    GladeWidget *parent);
LIBGLADEUI_API
gint              glade_util_count_placeholders    (GladeWidget *parent);

LIBGLADEUI_API
GtkTreeIter      *glade_util_find_iter_by_widget   (GtkTreeModel *model,
						    GladeWidget  *findme,
						    gint          column);

LIBGLADEUI_API
gboolean          glade_util_basenames_match       (const gchar  *path1,
						    const gchar  *path2);

LIBGLADEUI_API
GList            *glade_util_purify_list           (GList        *list);
LIBGLADEUI_API
GList            *glade_util_added_in_list         (GList        *old,
						    GList        *new);
LIBGLADEUI_API
GList            *glade_util_removed_from_list     (GList        *old,
						    GList        *new);
LIBGLADEUI_API
gchar            *glade_util_canonical_path        (const gchar  *path);

LIBGLADEUI_API
gboolean          glade_util_copy_file             (const gchar  *src_path,
						    const gchar  *dest_path);
LIBGLADEUI_API
gboolean          glade_util_class_implements_interface (GType class_type, 
							 GType iface_type);

LIBGLADEUI_API
GModule          *glade_util_load_library          (const gchar  *library_name);

LIBGLADEUI_API
gboolean          glade_util_file_is_writeable     (const gchar *path);

LIBGLADEUI_API
gboolean          glade_util_have_devhelp          (void);
LIBGLADEUI_API
GtkWidget        *glade_util_get_devhelp_icon      (GtkIconSize size);
LIBGLADEUI_API
void              glade_util_search_devhelp        (const gchar *book,
						    const gchar *page,
						    const gchar *search);
LIBGLADEUI_API
void              glade_util_set_grabed_widget     (GladeWidget *gwidget);

LIBGLADEUI_API
gboolean          glade_util_deep_fixed_event      (GtkWidget   *widget,
						    GdkEvent    *event,
						    GladeWidget *gwidget);

LIBGLADEUI_API
GtkWidget        *glade_util_get_placeholder_from_pointer (GtkContainer *container);

G_END_DECLS

#endif /* __GLADE_UTILS_H__ */
