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

#ifndef __GLADE_PREFERENCES_H__
#define __GLADE_PREFERENCES_H__

#include <gtk/gtk.h>

typedef struct _GladePreferences GladePreferences;

GladePreferences *glade_preferences_new     (GtkBuilder *builder);
void              glade_preferences_destroy (GladePreferences *prefs);

void glade_preferences_config_save (GladePreferences *prefs,
                                    GKeyFile *config);
void glade_preferences_config_load (GladePreferences *prefs,
                                    GKeyFile *config);

/* Callbacks */

void on_preferences_filechooserdialog_response (GtkDialog *dialog,
                                                gint response_id,
                                                GtkComboBoxText *combo);
void on_catalog_path_remove_button_clicked     (GtkButton *button,
                                                GtkComboBoxText *combo);

#endif /* __GLADE_PREFERENCES_H__ */
