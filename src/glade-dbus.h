/*
 * glade-dbus.h
 *
 * Copyright (C) 2018 Juan Pablo Ugarte
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
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#ifndef _GLADE_DBUS_H_
#define _GLADE_DBUS_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

void glade_dbus_register               (GApplication  *app);

void glade_dbus_emit_handler_added     (GApplication  *app,
                                        GladeWidget   *widget,
                                        GladeSignal   *signal,
                                        GError       **error);

void glade_dbus_emit_handler_removed   (GApplication  *app,
                                        GladeWidget   *widget,
                                        GladeSignal   *signal,
                                        GError       **error);

void glade_dbus_emit_handler_changed   (GApplication  *app,
                                        GladeWidget   *widget,
                                        GladeSignal   *old_signal,
                                        GladeSignal   *new_signal,
                                        GError       **error);

void glade_dbus_emit_handler_activated (GApplication  *app,
                                        GladeWidget   *widget,
                                        GladeSignal   *signal,
                                        GError       **error);

G_END_DECLS

#endif /* _GLADE_DBUS_H_ */
