/*
 * glade-editable.h
 *
 * Copyright (C) 2008 Tristan Van Berkom.
 *
 * Authors:
 *   Tristan Van Berkom <tvb@gnome.org>
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
 */

#ifndef __GLADE_EDITABLE_H__
#define __GLADE_EDITABLE_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GLADE_TYPE_EDITABLE glade_editable_get_type ()
G_DECLARE_INTERFACE (GladeEditable, glade_editable, GLADE, EDITABLE, GtkWidget)

typedef enum
{
  GLADE_PAGE_GENERAL,
  GLADE_PAGE_COMMON,
  GLADE_PAGE_PACKING,
  GLADE_PAGE_ATK,
  GLADE_PAGE_QUERY,
  GLADE_PAGE_SIGNAL
} GladeEditorPageType;


struct _GladeEditableInterface
{
  GTypeInterface g_iface;

  /* virtual table */
  void          (* load)          (GladeEditable  *editable,
                                   GladeWidget    *widget);
  void          (* set_show_name) (GladeEditable  *editable,
                                   gboolean        show_name);
};

void         glade_editable_load           (GladeEditable *editable,
                                            GladeWidget   *widget);
void         glade_editable_set_show_name  (GladeEditable *editable,
                                            gboolean       show_name);
GladeWidget *glade_editable_loaded_widget  (GladeEditable *editable);
gboolean     glade_editable_loading        (GladeEditable *editable);

void         glade_editable_block          (GladeEditable *editable);
void         glade_editable_unblock        (GladeEditable *editable);

G_END_DECLS

#endif /* __GLADE_EDITABLE_H__ */
