/*
 * Copyright (C) 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Chema Celorio <chema@celorio.com>
 */

#ifndef __GLADE_UTILS_H__
#define __GLADE_UTILS_H__

#include <glib.h>
#include "glade-project.h"

G_BEGIN_DECLS

#define GLADE_DEVHELP_ICON_NAME           "system-help-symbolic"
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
gboolean          glade_util_ui_message           (GtkWidget *parent, 
                                                   GladeUIMessageType type,
                                                   GtkWidget *widget,
                                                   const gchar *format,
                                                   ...) G_GNUC_PRINTF (4, 5);

void                  glade_util_flash_message        (GtkWidget *statusbar, 
                                                 guint context_id,
                                                 gchar *format,
                                                 ...) G_GNUC_PRINTF (3, 4);
gboolean          glade_util_url_show              (const gchar *url);
GtkWidget        *glade_util_file_dialog_new (const gchar *title,
                                              GladeProject *project,
                                              GtkWindow *parent,
                                              GladeUtilFileDialogType action);

/* Strings */
gint              glade_util_compare_stock_labels (gconstpointer a, gconstpointer b);
void              glade_util_replace (gchar *str, gchar a, gchar b);
gchar            *glade_util_read_prop_name (const gchar *str);
gchar            *glade_util_duplicate_underscores (const gchar *name);




/* GModule stuff */
GType             glade_util_get_type_from_name (const gchar *name, gboolean have_func);
GParamSpec       *glade_utils_get_pspec_from_funcname (const gchar *funcname);
GModule          *glade_util_load_library          (const gchar  *library_name);


/* String/Value utilities */
gint              glade_utils_enum_value_from_string  (GType enum_type, const gchar *strval);
gchar            *glade_utils_enum_string_from_value  (GType enum_type, gint value);
gint              glade_utils_flags_value_from_string (GType enum_type, const gchar *strval);
gchar            *glade_utils_flags_string_from_value (GType enum_type, gint value);
gchar            *glade_utils_flags_string_from_value_displayable (GType flags_type, gint value);
gchar            *glade_utils_enum_string_from_value_displayable (GType flags_type, gint value);
GValue           *glade_utils_value_from_string   (GType               type,
                                                   const gchar        *string,
                                                   GladeProject       *project);
gchar            *glade_utils_string_from_value   (const GValue       *value);
gboolean          glade_utils_boolean_from_string (const gchar *string,
                                                   gboolean *value);

/* Devhelp */
gboolean          glade_util_have_devhelp          (void);
GtkWidget        *glade_util_get_devhelp_icon      (GtkIconSize size);
void              glade_util_search_devhelp        (const gchar *book,
                                                    const gchar *page,
                                                    const gchar *search);

/* Files/Filenames*/
gchar            *glade_utils_replace_home_dir_with_tilde (const gchar *path);
gchar            *glade_util_canonical_path        (const gchar  *path);
time_t            glade_util_get_file_mtime        (const gchar *filename, GError **error);
gboolean          glade_util_file_is_writeable     (const gchar *path);
gchar            *glade_util_filename_to_icon_name (const gchar *value);
gchar            *glade_util_icon_name_to_filename (const gchar *value);

/* Cairo utilities */
void              glade_utils_cairo_draw_line (cairo_t  *cr,
                                               GdkColor *color,
                                               gint      x1,
                                               gint      y1,
                                               gint      x2,
                                               gint      y2);


void              glade_utils_cairo_draw_rectangle (cairo_t *cr,
                                                    GdkColor *color,
                                                    gboolean filled,
                                                    gint x,
                                                    gint y,
                                                    gint width,
                                                    gint height);

/* Lists */
void             glade_util_list_objects_ref       (GList        *list);
GList            *glade_util_purify_list           (GList        *list);
GList            *glade_util_added_in_list         (GList        *old_list,
                                                    GList        *new_list);
GList            *glade_util_removed_from_list     (GList        *old_list,
                                                    GList        *new_list);

/* Other utilities */
GtkListStore     *glade_utils_liststore_from_enum_type  (GType enum_type, gboolean include_empty);
gint              glade_utils_hijack_key_press (GtkWindow          *win, 
                                                GdkEventKey        *event, 
                                                gpointer            user_data);
gboolean          glade_util_check_and_warn_scrollable (GladeWidget        *parent,
                                                        GladeWidgetAdaptor *child_adaptor,
                                                        GtkWidget          *parent_widget);
GList            *glade_util_container_get_all_children (GtkContainer *container);
gint              glade_util_count_placeholders    (GladeWidget *parent);
GtkTreeIter      *glade_util_find_iter_by_widget   (GtkTreeModel *model,
                                                    GladeWidget  *findme,
                                                    gint          column);
GtkWidget        *glade_util_get_placeholder_from_pointer (GtkContainer *container);
gboolean          glade_util_object_is_loading     (GObject *object);

GdkPixbuf        *glade_utils_pointer_mode_render_icon (GladePointerMode mode, GtkIconSize size);

void              glade_utils_get_pointer (GtkWidget *widget,
                                           GdkWindow *window,
                                           GdkDevice *device,
                                           gint      *x,
                                           gint      *y);


void glade_util_remove_scroll_events (GtkWidget *widget);

G_END_DECLS

#endif /* __GLADE_UTILS_H__ */
