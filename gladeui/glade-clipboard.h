/*
 * glade-clipboard.h
 *
 * Copyright (C) 2001 The GNOME Foundation.
 *
 * Author(s):
 *      Archit Baweja <bighead@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */

#ifndef __GLADE_CLIPBOARD_H__
#define __GLADE_CLIPBOARD_H__

#include <gladeui/glade.h>

G_BEGIN_DECLS

#define GLADE_TYPE_CLIPBOARD glade_clipboard_get_type ()
G_DECLARE_DERIVABLE_TYPE (GladeClipboard, glade_clipboard, GLADE, CLIPBOARD, GObject)

struct _GladeClipboardClass
{
  GObjectClass parent_class;

  gpointer padding[4];
};

GladeClipboard *glade_clipboard_new              (void);
void            glade_clipboard_add              (GladeClipboard *clipboard, 
                                                  GList          *widgets);
void            glade_clipboard_clear            (GladeClipboard *clipboard);

gboolean        glade_clipboard_get_has_selection(GladeClipboard *clipboard);
GList          *glade_clipboard_widgets          (GladeClipboard *clipboard);

G_END_DECLS

#endif /* __GLADE_CLIPBOARD_H__ */
