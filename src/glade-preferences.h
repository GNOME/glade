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
 *   Tristan Van Berkom <tristan.van.berkom@gmail.com>
 */

#ifndef __GLADE_PREFERENCES_H__
#define __GLADE_PREFERENCES_H__

#include <gtk/gtk.h>
#include "glade-settings.h"

G_BEGIN_DECLS

#define GLADE_TYPE_PREFERENCES                 (glade_preferences_get_type ())
#define GLADE_PREFERENCES(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PREFERENCES, GladePreferences))
#define GLADE_PREFERENCES_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PREFERENCES, GladePreferencesClass))
#define GLADE_IS_PREFERENCES(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PREFERENCES))
#define GLADE_IS_PREFERENCES_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PREFERENCES))
#define GLADE_PREFERENCES_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_PREFERENCES, GladePreferencesClass))

typedef struct _GladePreferences             GladePreferences;
typedef struct _GladePreferencesClass        GladePreferencesClass;
typedef struct _GladePreferencesPrivate      GladePreferencesPrivate;

struct _GladePreferences
{
  /*< private >*/
  GtkDialog dialog;

  GladePreferencesPrivate *priv;
};

struct _GladePreferencesClass
{
  GtkDialogClass parent_class;
};

GType             glade_preferences_get_type         (void) G_GNUC_CONST;
GtkWidget        *glade_preferences_new              (GladeSettings *settings);

G_END_DECLS

#endif /* __GLADE_PREFERENCES_H__ */
