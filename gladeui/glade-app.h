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
 *   Naba Kumar <naba@gnome.org>
 */
 
#ifndef __GLADE_APP_H__
#define __GLADE_APP_H__

#include <gladeui/glade-editor.h>
#include <gladeui/glade-palette.h>
#include <gladeui/glade-clipboard.h>
#include <gladeui/glade-catalog.h>

G_BEGIN_DECLS

#define GLADE_TYPE_APP glade_app_get_type ()
G_DECLARE_DERIVABLE_TYPE (GladeApp, glade_app, GLADE, APP, GObject)

#define GLADE_ENV_CATALOG_PATH     "GLADE_CATALOG_SEARCH_PATH"
#define GLADE_ENV_MODULE_PATH      "GLADE_MODULE_SEARCH_PATH"
#define GLADE_ENV_TESTING          "GLADE_TESTING"
#define GLADE_ENV_PIXMAP_DIR       "GLADE_PIXMAP_DIR"
#define GLADE_ENV_ICON_THEME_PATH  "GLADE_ICON_THEME_PATH"
#define GLADE_ENV_BUNDLED          "GLADE_BUNDLED"

struct _GladeAppClass
{
  GObjectClass parent_class;

  gpointer padding[6];
};

void               glade_init                     (void);

GladeApp*          glade_app_new                  (void);
GladeApp*          glade_app_get                  (void);
GKeyFile*          glade_app_get_config           (void);
gint               glade_app_config_save          (void);

gboolean           glade_app_do_event             (GdkEvent *event);

gboolean           glade_app_get_catalog_version  (const gchar   *name, 
                                                   gint          *major, 
                                                   gint          *minor);
GList             *glade_app_get_catalogs         (void);
GladeCatalog      *glade_app_get_catalog          (const gchar   *name);
GladeClipboard*    glade_app_get_clipboard        (void);

void               glade_app_add_project          (GladeProject  *project);
void               glade_app_remove_project       (GladeProject  *project);
GList*             glade_app_get_projects         (void);
gboolean           glade_app_is_project_loaded    (const gchar   *project_path);
GladeProject*      glade_app_get_project_by_path  (const gchar   *project_path);

void               glade_app_set_window           (GtkWidget     *window);
GtkWidget*         glade_app_get_window           (void);
 
void               glade_app_set_accel_group      (GtkAccelGroup *accel_group);
GtkAccelGroup     *glade_app_get_accel_group      (void);

void               glade_app_search_docs          (const gchar   *book, 
                                                   const gchar   *page, 
                                                   const gchar   *search);

/* package paths */
const gchar       *glade_app_get_catalogs_dir     (void) G_GNUC_CONST;
const gchar       *glade_app_get_modules_dir      (void) G_GNUC_CONST;
const gchar       *glade_app_get_pixmaps_dir      (void) G_GNUC_CONST;
const gchar       *glade_app_get_locale_dir       (void) G_GNUC_CONST;
const gchar       *glade_app_get_bin_dir          (void) G_GNUC_CONST;
const gchar       *glade_app_get_lib_dir          (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GLADE_APP_H__ */
