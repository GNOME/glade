/*
 * Copyright (C) 2003 Joaquin Cuenca Abela
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
 *   Joaquin Cuenca Abela <e98cuenc@yahoo.com>
 */

#ifndef __GLADE_DEBUG_H__
#define __GLADE_DEBUG_H__

G_BEGIN_DECLS

typedef enum {
  GLADE_DEBUG_REF_COUNTS    = (1 << 0),
  GLADE_DEBUG_WIDGET_EVENTS = (1 << 1),
  GLADE_DEBUG_COMMANDS      = (1 << 2),
  GLADE_DEBUG_PROPERTIES    = (1 << 3),
  GLADE_DEBUG_VERIFY        = (1 << 4)
} GladeDebugFlag;

#ifdef GLADE_ENABLE_DEBUG

#define GLADE_NOTE(type,action)                        \
  G_STMT_START {                                       \
    if (glade_get_debug_flags () & GLADE_DEBUG_##type) \
      { action; };                                     \
  } G_STMT_END

#else /* !GLADE_ENABLE_DEBUG */

#define GLADE_NOTE(type, action)

#endif /* GLADE_ENABLE_DEBUG */

void   glade_init_debug_flags (void);
guint  glade_get_debug_flags  (void);

void   glade_setup_log_handlers (void);

G_END_DECLS

#endif /* __GLADE_DEBUG_H__ */
