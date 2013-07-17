/*
 * glade-dnd.h
 *
 * Copyright (C) 2013  Juan Pablo Ugarte
 * 
 * Authors:
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __GLADE_DND_H__
#define __GLADE_DND_H__

#define GLADE_DND_INFO_DATA 96323
#define GLADE_DND_TARGET_DATA "glade/x-drag-data"

#include "glade-drag.h"

G_BEGIN_DECLS

GtkTargetEntry *_glade_dnd_get_target      (void);

void            _glade_dnd_dest_set        (GtkWidget *target);

void            _glade_dnd_set_data        (GtkSelectionData *selection,
                                            GObject          *data);

GObject        *_glade_dnd_get_data        (GdkDragContext   *context,
                                            GtkSelectionData *selection,
                                            guint             info);

void            _glade_dnd_set_icon_widget (GdkDragContext *context,
                                            const gchar    *icon_name,
                                            const gchar    *description);

G_END_DECLS

#endif /* __GLADE_DND_H__ */
