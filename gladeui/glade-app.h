/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
#include <gladeui/glade-project-view.h>

G_BEGIN_DECLS

#define GLADE_CONFIG_FILENAME "glade-3.conf"

#define GLADE_TYPE_APP            (glade_app_get_type())
#define GLADE_APP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_APP, GladeApp))
#define GLADE_APP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_APP, GladeAppClass))
#define GLADE_IS_APP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_APP))
#define GLADE_IS_APP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_APP))
#define GLADE_APP_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_APP, GladeAppClass))

typedef struct _GladeApp        GladeApp;
typedef struct _GladeAppPrivate GladeAppPrivate;
typedef struct _GladeAppClass   GladeAppClass;

struct _GladeApp
{
	/*< private >*/
	GObject parent_instance;
	
	GladeAppPrivate *priv;
};

struct _GladeAppClass
{
	GObjectClass parent_class;
	
	/* class methods */
	void   (*  show_properties) (GladeApp* app,
				     gboolean  raise);
	void   (*  hide_properties) (GladeApp* app);

	/* signals */
	void   (* widget_event)     (GladeApp    *app, 
				     GladeWidget *toplevel,
				     GdkEvent    *event);
	void   (* update_ui_signal) (GladeApp    *app);
};

LIBGLADEUI_API 
GType              glade_app_get_type   (void) G_GNUC_CONST;

LIBGLADEUI_API 
GladeApp*          glade_app_get        (void);

LIBGLADEUI_API 
void               glade_app_update_ui  (void);

LIBGLADEUI_API 
gboolean           glade_app_widget_event (GladeWidget *widget, 
					   GdkEvent    *event);
LIBGLADEUI_API 
void               glade_app_set_window (GtkWidget *window);
LIBGLADEUI_API 
GtkWidget*         glade_app_get_window (void);

LIBGLADEUI_API 
GladeEditor*       glade_app_get_editor (void);
LIBGLADEUI_API 
GladePalette*      glade_app_get_palette (void);
LIBGLADEUI_API 
GladeClipboard*    glade_app_get_clipboard (void);
LIBGLADEUI_API 
GtkWidget*         glade_app_get_clipboard_view (void);

LIBGLADEUI_API 
GladeProject*      glade_app_get_project (void);
LIBGLADEUI_API 
void               glade_app_set_project (GladeProject *project);
LIBGLADEUI_API 
void               glade_app_add_project (GladeProject *project);
LIBGLADEUI_API 
void               glade_app_remove_project (GladeProject *project);
LIBGLADEUI_API 
GList*             glade_app_get_projects (void);
LIBGLADEUI_API 
GKeyFile*          glade_app_get_config (void);
LIBGLADEUI_API 
gboolean           glade_app_is_project_loaded (const gchar *project_path);
LIBGLADEUI_API 
GladeProject*      glade_app_get_project_by_path (const gchar *project_path);
LIBGLADEUI_API 
void               glade_app_show_properties (gboolean raise);
LIBGLADEUI_API 
void               glade_app_hide_properties (void);

LIBGLADEUI_API 
void               glade_app_add_project_view (GladeProjectView *view);

LIBGLADEUI_API 
void               glade_app_command_copy (void);
LIBGLADEUI_API 
void               glade_app_command_cut (void);
LIBGLADEUI_API 
void               glade_app_command_paste (GladePlaceholder *placeholder);
LIBGLADEUI_API 
void               glade_app_command_delete (void);
LIBGLADEUI_API
void               glade_app_command_delete_clipboard (void);
LIBGLADEUI_API 
void               glade_app_command_undo (void);
LIBGLADEUI_API 
void               glade_app_command_redo (void);

LIBGLADEUI_API 
gint               glade_app_config_save (void);
LIBGLADEUI_API 
void               glade_app_set_transient_parent (GtkWindow *parent);
LIBGLADEUI_API 
GtkWindow         *glade_app_get_transient_parent (void);
LIBGLADEUI_API 
void               glade_app_set_accel_group (GtkAccelGroup *accel_group);
LIBGLADEUI_API 
void               glade_app_update_instance_count  (GladeProject *project);

LIBGLADEUI_API 
GtkWidget 	 *glade_app_undo_button_new (void);
LIBGLADEUI_API 
GtkWidget 	 *glade_app_redo_button_new (void);

LIBGLADEUI_API 
GList            *glade_app_get_selection (void);


/* These handle selection on a global scope and take care
 * of multiple project logic.
 */
LIBGLADEUI_API 
gboolean           glade_app_is_selected       (GObject  *object);
LIBGLADEUI_API 
void               glade_app_selection_set     (GObject  *object,
					        gboolean  emit_signal);
LIBGLADEUI_API 
void               glade_app_selection_add     (GObject  *object,
					        gboolean  emit_signal);
LIBGLADEUI_API 
void               glade_app_selection_remove  (GObject  *object,
					        gboolean  emit_signal);
LIBGLADEUI_API 
void               glade_app_selection_clear   (gboolean  emit_signal);
LIBGLADEUI_API 
void               glade_app_selection_changed (void);

/* package paths */
LIBGLADEUI_API
const gchar       *glade_app_get_scripts_dir   (void) G_GNUC_CONST;
LIBGLADEUI_API
const gchar       *glade_app_get_catalogs_dir  (void) G_GNUC_CONST;
LIBGLADEUI_API
const gchar       *glade_app_get_modules_dir   (void) G_GNUC_CONST;
LIBGLADEUI_API
const gchar       *glade_app_get_plugins_dir   (void) G_GNUC_CONST;
LIBGLADEUI_API
const gchar       *glade_app_get_pixmaps_dir   (void) G_GNUC_CONST;
LIBGLADEUI_API
const gchar       *glade_app_get_locale_dir    (void) G_GNUC_CONST;
LIBGLADEUI_API
const gchar       *glade_app_get_bindings_dir  (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GLADE_APP_H__ */
