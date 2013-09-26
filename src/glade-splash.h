/*
 * glade-splash.c
 * 
 * Copyright (C) 2013 Juan Pablo Ugarte.
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
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#ifndef __GLADE_SPLASH_H__
#define __GLADE_SPLASH_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkWidget *glade_splash_screen_new (void);

void       glade_splash_window_show_immediately (GtkWidget *window);

G_END_DECLS

#endif /* __GLADE_SPLASH_H__ */
