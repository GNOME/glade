/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Tristan Van Berkom.
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
 *   Tristan Van Berkom <tvb@gnome.org>
 */
#ifndef _GLADE_CELL_RENDERER_BUTTON_H_
#define _GLADE_CELL_RENDERER_BUTTON_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_CELL_RENDERER_BUTTON	            (glade_cell_renderer_button_get_type ())
#define GLADE_CELL_RENDERER_BUTTON(obj)		    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_CELL_RENDERER_BUTTON, GladeCellRendererButton))
#define GLADE_CELL_RENDERER_BUTTON_CLASS(klass)	    (G_TYPE_CHECK_CLASS_CAST \
						     ((klass), GLADE_TYPE_CELL_RENDERER_BUTTON, GladeCellRendererButtonClass))
#define GLADE_IS_CELL_RENDERER_BUTTON(obj)	    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_CELL_RENDERER_BUTTON))
#define GLADE_IS_CELL_RENDERER_BUTTON_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_CELL_RENDERER_BUTTON))
#define GLADE_CELL_RENDERER_BUTTON_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_CELL_RENDERER_BUTTON, GladeCellRendererTextClass))

typedef struct _GladeCellRendererButton        GladeCellRendererButton;
typedef struct _GladeCellRendererButtonClass   GladeCellRendererButtonClass;

struct _GladeCellRendererButton
{
  GtkCellRendererText parent;
};

struct _GladeCellRendererButtonClass
{
  GtkCellRendererTextClass parent;

  void   (*clicked)  (GladeCellRendererButton *, const gchar *);
};

GType              glade_cell_renderer_button_get_type (void);
GtkCellRenderer   *glade_cell_renderer_button_new      (void);

G_END_DECLS

#endif  /* _GLADE_CELL_RENDERER_BUTTON_H_ */
