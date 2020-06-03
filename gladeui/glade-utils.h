#ifndef __GLADE_UTILS_H__
#define __GLADE_UTILS_H__

#include <glib.h>
#include "glade-project.h"

G_BEGIN_DECLS

#define GLADE_DEVHELP_ICON_NAME           "devhelp"
#define GLADE_DEVHELP_FALLBACK_ICON_FILE  "devhelp.png"

typedef enum _GladeUtilFileDialogType
{
  GLADE_FILE_DIALOG_ACTION_OPEN,
  GLADE_FILE_DIALOG_ACTION_SAVE
} GladeUtilFileDialogType;

typedef enum 
{
  GLADE_UI_INFO,
  GLADE_UI_WARN,
  GLADE_UI_ERROR,
  GLADE_UI_ARE_YOU_SURE,
  GLADE_UI_YES_OR_NO
} GladeUIMessageType;

/* UI interaction */
GLADEUI_EXPORTS
gboolean          glade_util_ui_message           (GtkWidget *parent, 
                                                   GladeUIMessageType type,
                                                   GtkWidget *widget,
                                                   const gchar *format,
                                                   ...) G_GNUC_PRINTF (4, 5);

GLADEUI_EXPORTS
void                  glade_util_flash_message        (GtkWidget *statusbar, 
                                                 guint context_id,
                                                 gchar *format,
                                                 ...) G_GNUC_PRINTF (3, 4);
GLADEUI_EXPORTS
gboolean          glade_util_url_show              (const gchar *url);
GLADEUI_EXPORTS
GtkWidget        *glade_util_file_dialog_new (const gchar *title,
                                              GladeProject *project,
                                              GtkWindow *parent,
                                              GladeUtilFileDialogType action);

/* Strings */
GLADEUI_EXPORTS
gint              glade_util_compare_stock_labels (gconstpointer a, gconstpointer b);
GLADEUI_EXPORTS
void              glade_util_replace (gchar *str, gchar a, gchar b);
GLADEUI_EXPORTS
gchar            *glade_util_read_prop_name (const gchar *str);
GLADEUI_EXPORTS
gchar            *glade_util_duplicate_underscores (const gchar *name);




/* GModule stuff */
GLADEUI_EXPORTS
GType             glade_util_get_type_from_name (const gchar *name, gboolean have_func);
GLADEUI_EXPORTS
GParamSpec       *glade_utils_get_pspec_from_funcname (const gchar *funcname);
GLADEUI_EXPORTS
GModule          *glade_util_load_library          (const gchar  *library_name);


/* String/Value utilities */
GLADEUI_EXPORTS
gint              glade_utils_enum_value_from_string  (GType enum_type, const gchar *strval);
GLADEUI_EXPORTS
gchar            *glade_utils_enum_string_from_value  (GType enum_type, gint value);
GLADEUI_EXPORTS
gint              glade_utils_flags_value_from_string (GType enum_type, const gchar *strval);
GLADEUI_EXPORTS
gchar            *glade_utils_flags_string_from_value (GType enum_type, gint value);
GLADEUI_EXPORTS
gchar            *glade_utils_flags_string_from_value_displayable (GType flags_type, gint value);
GLADEUI_EXPORTS
gchar            *glade_utils_enum_string_from_value_displayable (GType flags_type, gint value);
GLADEUI_EXPORTS
GValue           *glade_utils_value_from_string   (GType               type,
                                                   const gchar        *string,
                                                   GladeProject       *project);
GLADEUI_EXPORTS
gchar            *glade_utils_string_from_value   (const GValue       *value);
GLADEUI_EXPORTS
gboolean          glade_utils_boolean_from_string (const gchar *string,
                                                   gboolean *value);

/* Devhelp */
GLADEUI_EXPORTS
gboolean          glade_util_have_devhelp          (void);
GLADEUI_EXPORTS
GtkWidget        *glade_util_get_devhelp_icon      (GtkIconSize size);
GLADEUI_EXPORTS
void              glade_util_search_devhelp        (const gchar *book,
                                                    const gchar *page,
                                                    const gchar *search);

/* Files/Filenames*/
GLADEUI_EXPORTS
gchar            *glade_utils_replace_home_dir_with_tilde (const gchar *path);
GLADEUI_EXPORTS
gchar            *glade_util_canonical_path        (const gchar  *path);
GLADEUI_EXPORTS
time_t            glade_util_get_file_mtime        (const gchar *filename, GError **error);
GLADEUI_EXPORTS
gboolean          glade_util_file_is_writeable     (const gchar *path);
GLADEUI_EXPORTS
gchar            *glade_util_filename_to_icon_name (const gchar *value);
GLADEUI_EXPORTS
gchar            *glade_util_icon_name_to_filename (const gchar *value);

/* Cairo utilities */
GLADEUI_EXPORTS
void              glade_utils_cairo_draw_line (cairo_t  *cr,
                                               GdkColor *color,
                                               gint      x1,
                                               gint      y1,
                                               gint      x2,
                                               gint      y2);


GLADEUI_EXPORTS
void              glade_utils_cairo_draw_rectangle (cairo_t *cr,
                                                    GdkColor *color,
                                                    gboolean filled,
                                                    gint x,
                                                    gint y,
                                                    gint width,
                                                    gint height);

/* Lists */
GLADEUI_EXPORTS
GList            *glade_util_purify_list           (GList        *list);
GLADEUI_EXPORTS
GList            *glade_util_added_in_list         (GList        *old_list,
                                                    GList        *new_list);
GLADEUI_EXPORTS
GList            *glade_util_removed_from_list     (GList        *old_list,
                                                    GList        *new_list);

/* Other utilities */
GLADEUI_EXPORTS
GtkListStore     *glade_utils_liststore_from_enum_type  (GType enum_type, gboolean include_empty);
GLADEUI_EXPORTS
gint              glade_utils_hijack_key_press (GtkWindow          *win, 
                                                GdkEventKey        *event, 
                                                gpointer            user_data);
GLADEUI_EXPORTS
gboolean          glade_util_check_and_warn_scrollable (GladeWidget        *parent,
                                                        GladeWidgetAdaptor *child_adaptor,
                                                        GtkWidget          *parent_widget);
GLADEUI_EXPORTS
GList            *glade_util_container_get_all_children (GtkContainer *container);
GLADEUI_EXPORTS
gint              glade_util_count_placeholders    (GladeWidget *parent);
GLADEUI_EXPORTS
GtkTreeIter      *glade_util_find_iter_by_widget   (GtkTreeModel *model,
                                                    GladeWidget  *findme,
                                                    gint          column);
GLADEUI_EXPORTS
GtkWidget        *glade_util_get_placeholder_from_pointer (GtkContainer *container);
GLADEUI_EXPORTS
gboolean          glade_util_object_is_loading     (GObject *object);

GLADEUI_EXPORTS
GdkPixbuf        *glade_utils_pointer_mode_render_icon (GladePointerMode mode, GtkIconSize size);

GLADEUI_EXPORTS
void              glade_utils_get_pointer (GtkWidget *widget,
                                           GdkWindow *window,
                                           GdkDevice *device,
                                           gint      *x,
                                           gint      *y);


GLADEUI_EXPORTS
void glade_util_remove_scroll_events (GtkWidget *widget);

G_END_DECLS

#endif /* __GLADE_UTILS_H__ */
