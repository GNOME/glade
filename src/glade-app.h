/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Naba Kumar <naba@gnome.org>
 */
#ifndef __GLADE_APP_H__
#define __GLADE_APP_H__

G_BEGIN_DECLS

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
	
	/* signals */
	void (*update_ui_signal) (GladeApp *app);
};

GType glade_app_get_type (void);

GladeApp*          glade_app_new (void);

void               glade_app_update_ui (GladeApp* app);

void               glade_app_set_window (GladeApp* app, GtkWidget *window);
GtkWidget*         glade_app_get_window (GladeApp* app);

GladeEditor*       glade_app_get_editor (GladeApp* app);
GladeWidgetClass*  glade_app_get_add_class (GladeApp* app);
GladeWidgetClass*  glade_app_get_alt_class (GladeApp* app);
GladePalette*      glade_app_get_palette (GladeApp* app);
GladeClipboard*    glade_app_get_clipboard (GladeApp* app);
GtkWidget*         glade_app_get_clipboard_view (GladeApp* app);

GladeProject*      glade_app_get_active_project (GladeApp* app);
void               glade_app_set_project (GladeApp *app, GladeProject *project);
void               glade_app_add_project (GladeApp *app, GladeProject *project);
void               glade_app_remove_project (GladeApp *app, GladeProject *project);
GList*             glade_app_get_projects (GladeApp *app);

void               glade_app_add_project_view (GladeApp *app, GladeProjectView *view);

void               glade_app_command_copy (GladeApp *app);
void               glade_app_command_cut (GladeApp *app);
void               glade_app_command_paste (GladeApp *app);
void               glade_app_command_delete (GladeApp *app);
void               glade_app_command_undo (GladeApp *app);
void               glade_app_command_redo (GladeApp *app);

/* Default glade application */
void               glade_default_app_set (GladeApp *app);
GtkWidget*         glade_default_app_get_window (void);
GladeEditor*       glade_default_app_get_editor (void);
GladeWidgetClass*  glade_default_app_get_add_class (void);
GladeWidgetClass*  glade_default_app_get_alt_class (void);
GladePalette*      glade_default_app_get_palette (void);
GladeClipboard*    glade_default_app_get_clipboard (void);
GladeProject*      glade_default_app_get_active_project (void);
void               glade_default_app_update_ui (void);

G_END_DECLS

#endif /* __GLADE_APP_H__ */
