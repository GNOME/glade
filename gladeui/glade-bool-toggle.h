/*
 * glade-bool-toggle.h
 *
 * Copyright (C) 2013 Juan Pablo Ugarte <juanpablougarte@gmail.com>
   *
 * Authors:
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _GLADE_BOOL_TOGGLE_H_
#define _GLADE_BOOL_TOGGLE_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_BOOL_TOGGLE             (glade_bool_toggle_get_type ())
#define GLADE_BOOL_TOGGLE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_BOOL_TOGGLE, GladeBoolToggle))
#define GLADE_BOOL_TOGGLE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_BOOL_TOGGLE, GladeBoolToggleClass))
#define GLADE_IS_BOOL_TOGGLE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_BOOL_TOGGLE))
#define GLADE_IS_BOOL_TOGGLE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_BOOL_TOGGLE))
#define GLADE_BOOL_TOGGLE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_BOOL_TOGGLE, GladeBoolToggleClass))

typedef struct _GladeBoolToggle        GladeBoolToggle;
typedef struct _GladeBoolTogglePrivate GladeBoolTogglePrivate;
typedef struct _GladeBoolToggleClass   GladeBoolToggleClass;

struct _GladeBoolToggleClass
{
  GtkToggleButtonClass parent_class;
};

struct _GladeBoolToggle
{
  GtkToggleButton parent_instance;
  GladeBoolTogglePrivate *priv;
};

GType glade_bool_toggle_get_type (void) G_GNUC_CONST;
GtkWidget *_glade_bool_toggle_new (void);

G_END_DECLS

#endif /* _GLADE_BOOL_TOGGLE_H_ */
