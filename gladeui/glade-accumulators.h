/*
 * glade-clipboard.h
 *
 * Copyright (C) 2005 The GNOME Foundation.
 *
 * Author(s):
 *      Tristan Van Berkom <tvb@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */

#ifndef __GLADE_ACCUMULATORS_H__
#define __GLADE_ACCUMULATORS_H__

#include <glib-object.h>

G_BEGIN_DECLS

gboolean _glade_single_object_accumulator   (GSignalInvocationHint *ihint,
                                             GValue                *return_accu,
                                             const GValue          *handler_return,
                                             gpointer               dummy);

gboolean _glade_integer_handled_accumulator (GSignalInvocationHint *ihint,
                                             GValue                *return_accu,
                                             const GValue          *handler_return,
                                             gpointer               dummy);
                                            
gboolean _glade_boolean_handled_accumulator (GSignalInvocationHint *ihint,
                                             GValue                *return_accu,
                                             const GValue          *handler_return,
                                             gpointer               dummy);

gboolean _glade_string_accumulator          (GSignalInvocationHint *ihint,
                                             GValue                *return_accu,
                                             const GValue          *handler_return,
                                             gpointer               dummy);

gboolean _glade_strv_handled_accumulator    (GSignalInvocationHint *ihint,
                                             GValue                *return_accu,
                                             const GValue          *handler_return,
                                             gpointer               dummy);

gboolean _glade_stop_emission_accumulator   (GSignalInvocationHint *ihint,
                                             GValue                *return_accu,
                                             const GValue          *handler_return,
                                             gpointer               dummy);
G_END_DECLS

#endif   /* __GLADE_ACCUMULATORS_H__ */
