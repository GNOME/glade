/*
 * glade-close-button.h
 * This file was taken from gedit
 *
 * Copyright (C) 2010 - Paolo Borelli
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GLADE_CLOSE_BUTTON_H__
#define __GLADE_CLOSE_BUTTON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_CLOSE_BUTTON			(glade_close_button_get_type ())
#define GLADE_CLOSE_BUTTON(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_CLOSE_BUTTON, GladeCloseButton))
#define GLADE_CLOSE_BUTTON_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_CLOSE_BUTTON, GladeCloseButton const))
#define GLADE_CLOSE_BUTTON_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_CLOSE_BUTTON, GladeCloseButtonClass))
#define GLADE_IS_CLOSE_BUTTON(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_CLOSE_BUTTON))
#define GLADE_IS_CLOSE_BUTTON_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_CLOSE_BUTTON))
#define GLADE_CLOSE_BUTTON_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_CLOSE_BUTTON, GladeCloseButtonClass))

typedef struct _GladeCloseButton	GladeCloseButton;
typedef struct _GladeCloseButtonClass	GladeCloseButtonClass;
typedef struct _GladeCloseButtonPrivate	GladeCloseButtonPrivate;

struct _GladeCloseButton {
	GtkButton parent;
};

struct _GladeCloseButtonClass {
	GtkButtonClass parent_class;
};

GType		  glade_close_button_get_type (void) G_GNUC_CONST;

GtkWidget	 *glade_close_button_new (void);

G_END_DECLS

#endif /* __GLADE_CLOSE_BUTTON_H__ */
