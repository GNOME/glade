/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-palette-box.h
 *
 * Copyright (C) 2007 Vincent Geddes.
 *
 * Author: Vincent Geddes <vincent.geddes@gmail.com>
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
 *
 */

#ifndef __GLADE_PALETTE_BOX_H__
#define __GLADE_PALETTE_BOX_H__

#include <gtk/gtkcontainer.h>

G_BEGIN_DECLS

#define GLADE_TYPE_PALETTE_BOX            (glade_palette_box_get_type ())
#define GLADE_PALETTE_BOX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PALETTE_BOX, GladePaletteBox))
#define GLADE_PALETTE_BOX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PALETTE_BOX, GladePaletteBoxClass))
#define GLADE_IS_PALETTE_BOX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PALETTE_BOX))
#define GLADE_IS_PALETTE_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PALETTE_BOX))
#define GLADE_PALETTE_BOX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_PALETTE_BOX, GladePaletteBoxClass))

typedef struct _GladePaletteBox	       GladePaletteBox;
typedef struct _GladePaletteBoxPrivate GladePaletteBoxPrivate;
typedef struct _GladePaletteBoxClass   GladePaletteBoxClass;

struct _GladePaletteBox
{
	/*< private >*/
	GtkContainer parent_instance;

	GladePaletteBoxPrivate *priv;
};

struct _GladePaletteBoxClass
{
	GtkContainerClass parent_class;
};




GType          glade_palette_box_get_type       (void) G_GNUC_CONST;

GtkWidget     *glade_palette_box_new            (void);

void           glade_palette_box_reorder_child  (GladePaletteBox *box,
                                                 GtkWidget       *child,
                                                 gint             position);


G_END_DECLS

#endif /* __GLADE_PALETTE_BOX_H__ */
