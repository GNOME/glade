/*
 * Copyright (C) 2004 Owen Taylor
 *
 * Authors:
 *   Owen Taylor  <otaylor@redhat.com>
 *
 * Modified by the Glade developers
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
 */

#ifndef __GLADE_ID_ALLOCATOR_H__
#define __GLADE_ID_ALLOCATOR_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GladeIDAllocator GladeIDAllocator;

GladeIDAllocator    *glade_id_allocator_new       (void);

void                 glade_id_allocator_destroy   (GladeIDAllocator *allocator);

guint                glade_id_allocator_allocate  (GladeIDAllocator *allocator);

void                 glade_id_allocator_release   (GladeIDAllocator *allocator,
                                                   guint             id);

G_END_DECLS

#endif /* __GLADE_ID_ALLOCATOR_H__ */

