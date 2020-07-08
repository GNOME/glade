/*
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2007 Vincent Geddes.
 * Copyright (C) 2012 Juan Pablo Ugarte.
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

#ifndef __GLADE_WINDOW_H__
#define __GLADE_WINDOW_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_WINDOW            (glade_window_get_type())
#define GLADE_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_WINDOW, GladeWindow))
#define GLADE_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_WINDOW, GladeWindowClass))
#define GLADE_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_WINDOW))
#define GLADE_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_WINDOW))
#define GLADE_WINDOW_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_WINDOW, GladeWindowClass))

typedef struct _GladeWindow         GladeWindow;
typedef struct _GladeWindowPrivate  GladeWindowPrivate;
typedef struct _GladeWindowClass    GladeWindowClass;

struct _GladeWindow
{
  GtkWindow parent_instance;
  GladeWindowPrivate *priv;
};

struct _GladeWindowClass
{
  GtkWindowClass parent_class;
};

GType        glade_window_get_type      (void) G_GNUC_CONST;

GtkWidget   *glade_window_new           (void);

void         glade_window_new_project   (GladeWindow *window);

gboolean     glade_window_open_project  (GladeWindow *window,
                                         const gchar *path);

void         glade_window_check_devhelp (GladeWindow *window);

void         glade_window_registration_notify_user (GladeWindow *window);

const gchar *glade_window_get_gdk_backend (void);

G_END_DECLS

#endif /* __GLADE_WINDOW_H__ */
