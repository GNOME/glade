/*
 * Copyright 2019 Collabora Ltd.
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
 *   Corentin Noël <corentin.noel@collabora.com>
 */

#ifndef __GLADE_SETTINGS_H__
#define __GLADE_SETTINGS_H__

#include <gio/gio.h>
#include <gladeui/glade-project.h>

G_BEGIN_DECLS

#define GLADE_TYPE_SETTINGS glade_settings_get_type ()
G_DECLARE_FINAL_TYPE (GladeSettings, glade_settings, GLADE, SETTINGS, GObject)

GladeSettings   *glade_settings_new              (void);
void             glade_settings_save             (GladeSettings *self,
                                                  GKeyFile      *file);
void             glade_settings_load             (GladeSettings *self,
                                                  GKeyFile      *file);

gboolean         glade_settings_backup           (GladeSettings *self);
gboolean         glade_settings_autosave         (GladeSettings *self);
gint             glade_settings_autosave_seconds (GladeSettings *self);
GladeVerifyFlags glade_settings_get_verify_flags (GladeSettings *self);

G_END_DECLS

#endif /* __GLADE_SETTINGS_H__ */
