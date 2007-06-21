/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 The GNOME Foundation.
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
*/

#ifndef __GLADE_PROJECT_WINDOW_H__
#define __GLADE_PROJECT_WINDOW_H__

#include <gladeui/glade-app.h>

G_BEGIN_DECLS

#define GLADE_TYPE_PROJECT_WINDOW            (glade_project_window_get_type())
#define GLADE_PROJECT_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PROJECT_WINDOW, GladeProjectWindow))
#define GLADE_PROJECT_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PROJECT_WINDOW, GladeProjectWindowClass))
#define GLADE_IS_PROJECT_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PROJECT_WINDOW))
#define GLADE_IS_PROJECT_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PROJECT_WINDOW))
#define GLADE_PROJECT_WINDOW_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_PROJECT_WINDOW, GladeProjectWindowClass))

typedef struct _GladeProjectWindow         GladeProjectWindow;
typedef struct _GladeProjectWindowClass    GladeProjectWindowClass;
typedef struct _GladeProjectWindowPrivate  GladeProjectWindowPrivate;

struct _GladeProjectWindow
{
	GladeApp parent_instance;
	
	GladeProjectWindowPrivate *priv;
};

struct _GladeProjectWindowClass
{
	GladeAppClass parent_class;
};

GType                glade_project_window_get_type      (void) G_GNUC_CONST;

GladeProjectWindow  *glade_project_window_new           (void);

void                 glade_project_window_show_all      (GladeProjectWindow *gpw);

void                 glade_project_window_new_project   (GladeProjectWindow *gpw);

gboolean             glade_project_window_open_project  (GladeProjectWindow *gpw,
							 const gchar        *path);

void                 glade_project_window_check_devhelp (GladeProjectWindow *gpw);

G_END_DECLS

#endif /* __GLADE_PROJECT_WINDOW_H__ */
