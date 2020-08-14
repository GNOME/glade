/*
 * glade-intro.h
 *
 * Copyright (C) 2017 Juan Pablo Ugarte
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

#ifndef _GLADE_INTRO_H_
#define _GLADE_INTRO_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_INTRO (glade_intro_get_type ())
G_DECLARE_FINAL_TYPE (GladeIntro, glade_intro, GLADE, INTRO, GObject)

typedef enum
{
  GLADE_INTRO_STATE_NULL =  0,
  GLADE_INTRO_STATE_PLAYING,
  GLADE_INTRO_STATE_PAUSED
} GladeIntroState;

typedef enum
{
  GLADE_INTRO_NONE = 0,
  GLADE_INTRO_TOP,
  GLADE_INTRO_BOTTOM,
  GLADE_INTRO_RIGHT,
  GLADE_INTRO_LEFT,
  GLADE_INTRO_CENTER
} GladeIntroPosition;

GladeIntro *glade_intro_new (GtkWindow *toplevel);

void        glade_intro_set_toplevel (GladeIntro *intro,
                                      GtkWindow  *toplevel);

void        glade_intro_script_add   (GladeIntro         *intro,
                                      const gchar        *name,
                                      const gchar        *widget,
                                      const gchar        *text,
                                      GladeIntroPosition  position,
                                      gdouble             delay);

void        glade_intro_play  (GladeIntro *intro);
void        glade_intro_pause (GladeIntro *intro);
void        glade_intro_stop  (GladeIntro *intro);


G_END_DECLS

#endif /* _GLADE_INTRO_H_ */
