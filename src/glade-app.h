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

#include "glade-editor.h"
#include "glade-palette.h"
#include "glade-clipboard.h"
#include "glade-project-view.h"

G_BEGIN_DECLS

#define GLADE_CONFIG_FILENAME "glade-3.conf"

#define GLADE_TYPE_APP            (glade_app_get_type())
#define GLADE_APP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_APP, GladeApp))
#define GLADE_APP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_APP, GladeAppClass))
#define GLADE_IS_APP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_APP))
#define GLADE_IS_APP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_APP))
#define GLADE_APP_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_APP, GladeAppClass))

typedef struct _GladeApp GladeApp;
typedef struct _GladeAppClass GladeAppClass;
typedef struct _GladeAppPriv GladeAppPriv;

struct _GladeApp
{
	GObject parent;
	GladeAppPriv *priv;
};

struct _GladeAppClass
{
	GObjectClass parent_class;
	
	/* class methods */
	void   (*  show_properties) (GladeApp* app, gboolean raise);
	void   (*  hide_properties) (GladeApp* app);

	/* signals */
	void   (* update_ui_signal) (GladeApp *app);
};

LIBGLADEUI_API 
GType              glade_app_get_type (void) G_GNUC_CONST;

LIBGLADEUI_API 
GladeApp*          glade_app_new (void);

LIBGLADEUI_API 
void               glade_app_update_ui (GladeApp* app);

LIBGLADEUI_API 
void               glade_app_set_window (GladeApp* app, GtkWidget *window);
LIBGLADEUI_API 
GtkWidget*         glade_app_get_window (GladeApp* app);

LIBGLADEUI_API 
GladeEditor*       glade_app_get_editor (GladeApp* app);
LIBGLADEUI_API 
GladeWidgetClass*  glade_app_get_add_class (GladeApp* app);
LIBGLADEUI_API 
GladeWidgetClass*  glade_app_get_alt_class (GladeApp* app);
LIBGLADEUI_API 
GladePalette*      glade_app_get_palette (GladeApp* app);
LIBGLADEUI_API 
GladeClipboard*    glade_app_get_clipboard (GladeApp* app);
LIBGLADEUI_API 
GtkWidget*         glade_app_get_clipboard_view (GladeApp* app);

LIBGLADEUI_API 
GladeProject*      glade_app_get_active_project (GladeApp* app);
LIBGLADEUI_API 
void               glade_app_set_project (GladeApp *app, GladeProject *project);
LIBGLADEUI_API 
void               glade_app_add_project (GladeApp *app, GladeProject *project);
LIBGLADEUI_API 
void               glade_app_remove_project (GladeApp *app, GladeProject *project);
LIBGLADEUI_API 
GList*             glade_app_get_projects (GladeApp *app);
LIBGLADEUI_API 
GKeyFile*          glade_app_get_config (GladeApp *app);
LIBGLADEUI_API 
gboolean           glade_app_is_project_loaded (GladeApp *app, const gchar *project_path);
LIBGLADEUI_API 
void               glade_app_show_properties (GladeApp* app, gboolean raise);
LIBGLADEUI_API 
void               glade_app_hide_properties (GladeApp* app);

LIBGLADEUI_API 
void               glade_app_add_project_view (GladeApp *app, GladeProjectView *view);

LIBGLADEUI_API 
void               glade_app_command_copy (GladeApp *app);
LIBGLADEUI_API 
void               glade_app_command_cut (GladeApp *app);
LIBGLADEUI_API 
void               glade_app_command_paste (GladeApp *app);
LIBGLADEUI_API 
void               glade_app_command_delete (GladeApp *app);
LIBGLADEUI_API 
void               glade_app_command_undo (GladeApp *app);
LIBGLADEUI_API 
void               glade_app_command_redo (GladeApp *app);

LIBGLADEUI_API 
gint               glade_app_config_save (GladeApp *app);
LIBGLADEUI_API 
void               glade_app_set_transient_parent (GladeApp *app, GtkWindow *parent);
LIBGLADEUI_API 
GtkWindow         *glade_app_get_transient_parent (GladeApp *app);
LIBGLADEUI_API 
void               glade_app_set_accel_group (GladeApp *app, GtkAccelGroup *accel_group);
LIBGLADEUI_API 
void               glade_app_update_instance_count  (GladeApp *app, GladeProject *project);

LIBGLADEUI_API 
GtkWidget 	 *glade_app_undo_button_new (GladeApp *app);
LIBGLADEUI_API 
GtkWidget 	 *glade_app_redo_button_new (GladeApp *app);

/* Default glade application */
LIBGLADEUI_API 
void               glade_default_app_set (GladeApp *app);
LIBGLADEUI_API 
GtkWidget*         glade_default_app_get_window (void);
LIBGLADEUI_API 
GladeEditor*       glade_default_app_get_editor (void);
LIBGLADEUI_API 
GladeWidgetClass*  glade_default_app_get_add_class (void);
LIBGLADEUI_API 
GladeWidgetClass*  glade_default_app_get_alt_class (void);
LIBGLADEUI_API 
GladePalette*      glade_default_app_get_palette (void);
LIBGLADEUI_API 
GladeClipboard*    glade_default_app_get_clipboard (void);
LIBGLADEUI_API 
GladeProject*      glade_default_app_get_active_project (void);
LIBGLADEUI_API 
void               glade_default_app_update_ui (void);
LIBGLADEUI_API 
GList*             glade_default_app_get_selection (void);
LIBGLADEUI_API 
GList*             glade_default_app_get_projects (void);
LIBGLADEUI_API 
void               glade_default_app_show_properties (gboolean raise);
LIBGLADEUI_API 
void               glade_default_app_hide_properties (void);
LIBGLADEUI_API 
void               glade_default_app_set_transient_parent (GtkWindow *parent);
LIBGLADEUI_API 
GtkWindow         *glade_default_app_get_transient_parent (void);

LIBGLADEUI_API 
GtkWidget         *glade_default_app_undo_button_new  (void);
LIBGLADEUI_API 
GtkWidget         *glade_default_app_redo_button_new  (void);


/* GladeCommand interface stuff
 */
LIBGLADEUI_API
void               glade_default_app_command_cut (void);
LIBGLADEUI_API
void               glade_default_app_command_copy (void);
LIBGLADEUI_API
void               glade_default_app_command_paste (void);
LIBGLADEUI_API
void               glade_default_app_command_delete (void);
LIBGLADEUI_API
void               glade_default_app_command_delete_clipboard (void);


/* These handle selection on a global scope and take care
 * of multiple project logic.
 */
LIBGLADEUI_API 
gboolean           glade_default_app_is_selected      (GObject      *object);
LIBGLADEUI_API 
void               glade_default_app_selection_set    (GObject      *object,
						       gboolean      emit_signal);
LIBGLADEUI_API 
void               glade_default_app_selection_add    (GObject      *object,
						       gboolean      emit_signal);
LIBGLADEUI_API 
void               glade_default_app_selection_remove (GObject      *object,
						       gboolean      emit_signal);
LIBGLADEUI_API 
void               glade_default_app_selection_clear  (gboolean      emit_signal);
LIBGLADEUI_API 
void               glade_default_app_selection_changed(void);


G_END_DECLS

#endif /* __GLADE_APP_H__ */
