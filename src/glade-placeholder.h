/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GLADE_PLACEHOLDER_H__
#define __GLADE_PLACEHOLDER_H__


#include <gdk/gdk.h>
#include <gdk/gdkpixmap.h>
#include <gtk/gtkwidget.h>
#include "glade-types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GLADE_TYPE_PLACEHOLDER            (glade_placeholder_get_type ())
#define GLADE_PLACEHOLDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PLACEHOLDER, GladePlaceholder))
#define GLADE_PLACEHOLDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PLACEHOLDER, GladePlaceholderClass))
#define GLADE_IS_PLACEHOLDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PLACEHOLDER))
#define GLADE_IS_PLACEHOLDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PLACEHOLDER))
#define GLADE_PLACEHOLDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_PLACEHOLDER, GladePlaceholderClass))


typedef struct _GladePlaceholder       GladePlaceholder;
typedef struct _GladePlaceholderClass  GladePlaceholderClass;

struct _GladePlaceholder
{
	GtkWidget widget;

	GdkPixmap *placeholder_pixmap;
};

struct _GladePlaceholderClass
{
	GtkWidgetClass parent_class;
};


GType      glade_placeholder_get_type   (void) G_GNUC_CONST;
GtkWidget* glade_placeholder_new   (void);

/* TODO: Remove me */
void	   glade_placeholder_add_methods_to_class (GladeWidgetClass *class);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GLADE_PLACEHOLDER_H__ */
