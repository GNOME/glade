/*
 * glade-name-context.c
 *
 * Copyright (C) 2008 Tristan Van Berkom.
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
 * Authors:
 *   Tristan Van Berkom <tvb@gnome.org>
 *
 */

#ifndef __GLADE_NAME_CONTEXT_H__
#define __GLADE_NAME_CONTEXT_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GladeNameContext GladeNameContext;

GladeNameContext  *glade_name_context_new                 (void);
void               glade_name_context_destroy             (GladeNameContext *context);

gchar             *glade_name_context_new_name            (GladeNameContext *context,
                                                           const gchar      *base_name);

guint              glade_name_context_n_names             (GladeNameContext *context);

gboolean           glade_name_context_has_name            (GladeNameContext *context,
                                                           const gchar      *name);

gboolean           glade_name_context_add_name            (GladeNameContext *context,
                                                           const gchar      *name);

void               glade_name_context_release_name        (GladeNameContext *context,
                                                           const gchar      *name);

G_END_DECLS

#endif /* __GLADE_NAME_CONTEXT_H__ */
