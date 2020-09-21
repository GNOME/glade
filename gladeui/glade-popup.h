/*
 * Copyright (C) 2001 Ximian, Inc.
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
 *   Chema Celorio <chema@celorio.com>
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#ifndef __GLADE_POPUP_H__
#define __GLADE_POPUP_H__

G_BEGIN_DECLS

void glade_popup_widget_pop           (GladeWidget *widget,
                                       GdkEventButton *event,
                                       gboolean packing);

void glade_popup_placeholder_pop      (GladePlaceholder *placeholder,
                                       GdkEventButton *event);

void glade_popup_clipboard_pop        (GladeWidget *widget,
                                       GdkEventButton *event);

void glade_popup_palette_pop          (GladePalette       *palette,
                                       GladeWidgetAdaptor *adaptor,
                                       GdkEventButton     *event);

gint glade_popup_action_populate_menu (GtkWidget *menu,
                                       GladeWidget *widget,
                                       GladeWidgetAction *action,
                                       gboolean packing);

void glade_popup_simple_pop           (GladeProject   *project,
                                       GdkEventButton *event);

void glade_popup_property_pop         (GladeProperty  *property,
                                       GdkEventButton *event);


gboolean glade_popup_is_popup_event   (GdkEventButton *event);

G_END_DECLS

#endif /* __GLADE_POPUP_H__ */
