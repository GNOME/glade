/*
 * Copyright (C) 2013 Tristan Van Berkom.
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
 */
#ifndef _GLADE_GTK_MENU_SHELL_H_
#define _GLADE_GTK_MENU_SHELL_H_

#include <gtk/gtk.h>
#include <gladeui/glade.h>

G_BEGIN_DECLS

G_MODULE_EXPORT
void glade_gtk_menu_shell_launch_editor            (GObject *object, gchar *title);

G_MODULE_EXPORT
gboolean glade_gtk_menu_shell_change_type (GladeBaseEditor *editor,
                                           GladeWidget     *gchild,
                                           GType            type,
                                           gpointer         data);
G_MODULE_EXPORT
GladeWidget *glade_gtk_menu_shell_build_child (GladeBaseEditor *editor,
                                               GladeWidget     *gparent,
                                               GType            type,
                                               gpointer         data);
G_MODULE_EXPORT
gboolean glade_gtk_menu_shell_delete_child (GladeBaseEditor *editor,
                                            GladeWidget     *gparent,
                                            GladeWidget     *gchild,
                                            gpointer         data);
G_MODULE_EXPORT
gboolean glade_gtk_menu_shell_move_child (GladeBaseEditor *editor,
                                          GladeWidget     *gparent,
                                          GladeWidget     *gchild,
                                          gpointer         data);

G_MODULE_EXPORT
gchar *glade_gtk_menu_shell_tool_item_get_display_name (GladeBaseEditor *editor,
                                                        GladeWidget     *gchild,
                                                        gpointer         user_data);

G_MODULE_EXPORT
void glade_gtk_toolbar_child_selected              (GladeBaseEditor *editor,
                                                    GladeWidget     *gchild,
                                                    gpointer         data);
G_MODULE_EXPORT
void glade_gtk_tool_palette_child_selected         (GladeBaseEditor *editor,
                                                    GladeWidget     *gchild,
                                                    gpointer         data);
G_MODULE_EXPORT
void glade_gtk_recent_chooser_menu_child_selected  (GladeBaseEditor *editor,
                                                    GladeWidget     *gchild,
                                                    gpointer         data);
G_MODULE_EXPORT
void glade_gtk_menu_shell_tool_item_child_selected (GladeBaseEditor *editor,
                                                    GladeWidget     *gchild,
                                                    gpointer         data);

G_END_DECLS

#endif  /* _GLADE_GTK_MENU_SHELL_H_ */
