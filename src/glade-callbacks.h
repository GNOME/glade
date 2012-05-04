/*
 * Copyright (C) 2012 Juan Pablo Ugarte.
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

#ifndef __GLADE_CALLBACKS_H__
#define __GLADE_CALLBACKS_H__

#include "glade-window.h"
#include <gladeui/glade-design-view.h>

G_BEGIN_DECLS

void on_open_action_activate             (GtkAction *action, GladeWindow *window);
void on_save_action_activate             (GtkAction *action, GladeWindow *window);
void on_save_as_action_activate          (GtkAction *action, GladeWindow *window);
void on_close_action_activate            (GtkAction *action, GladeWindow *window);
void on_copy_action_activate             (GtkAction *action, GladeWindow *window);
void on_cut_action_activate              (GtkAction *action, GladeWindow *window);
void on_paste_action_activate            (GtkAction *action, GladeWindow *window);
void on_delete_action_activate           (GtkAction *action, GladeWindow *window);
void on_properties_action_activate       (GtkAction *action, GladeWindow *window);
void on_undo_action_activate             (GtkAction *action, GladeWindow *window);
void on_redo_action_activate             (GtkAction *action, GladeWindow *window);
void on_quit_action_activate             (GtkAction *action, GladeWindow *window);
void on_about_action_activate            (GtkAction *action, GladeWindow *window);
void on_reference_action_activate        (GtkAction *action, GladeWindow *window);

void on_open_recent_action_item_activated (GtkRecentChooser *chooser,
                                           GladeWindow *window);

void on_use_small_icons_action_toggled      (GtkAction *action, GladeWindow *window);
void on_dock_action_toggled                 (GtkAction *action, GladeWindow *window);
void on_toolbar_visible_action_toggled      (GtkAction *action, GladeWindow *window);
void on_statusbar_visible_action_toggled    (GtkAction *action, GladeWindow *window);
void on_project_tabs_visible_action_toggled (GtkAction *action, GladeWindow *window);

void on_palette_appearance_radioaction_changed (GtkRadioAction *action,
                                                GtkRadioAction *current,
                                                GladeWindow    *window);
void on_selector_radioaction_changed           (GtkRadioAction *action,
                                                GtkRadioAction *current,
                                                GladeWindow *window);

void on_actiongroup_connect_proxy    (GtkActionGroup *action_group,
                                      GtkAction *action,
                                      GtkWidget *proxy,
                                      GladeWindow *window);
void on_actiongroup_disconnect_proxy (GtkActionGroup *action_group,
                                      GtkAction *action,
                                      GtkWidget *proxy,
                                      GladeWindow *window);

void on_notebook_switch_page (GtkNotebook *notebook,
                              GtkWidget *page,
                              guint page_num,
                              GladeWindow *window);
void on_notebook_tab_added   (GtkNotebook *notebook,
                              GladeDesignView *view,
                              guint page_num,
                              GladeWindow *window);
void on_notebook_tab_removed (GtkNotebook     *notebook,
                              GladeDesignView *view,
                              guint            page_num, 
                              GladeWindow     *window);

void on_recent_menu_insert (GtkMenuShell *menu_shell,
                            GtkWidget    *child,
                            gint          position,
                            GladeWindow *window);
void on_recent_menu_remove (GtkContainer *container,
                            GtkWidget *widget,
                            GladeWindow *window);

G_END_DECLS

#endif /* __GLADE_CALLBACKS_H__ */
