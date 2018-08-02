/*
 * Copyright (C) 2008 Tristan Van Berkom.
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
 *   Tristan Van Berkom <tvb@gnome.org>
 */
#ifndef _GLADE_EDITOR_BUTTON_H_
#define _GLADE_EDITOR_BUTTON_H_

#include <gtk/gtk.h>
#include <gladeui/glade-editable.h>


G_BEGIN_DECLS

#define GLADE_TYPE_EDITOR_TABLE             (glade_editor_table_get_type ())
#define GLADE_EDITOR_TABLE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EDITOR_TABLE, GladeEditorTable))
#define GLADE_EDITOR_TABLE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EDITOR_TABLE, GladeEditorTableClass))
#define GLADE_IS_EDITOR_TABLE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EDITOR_TABLE))
#define GLADE_IS_EDITOR_TABLE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EDITOR_TABLE))
#define GLADE_EDITOR_TABLE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_EDITOR_TABLE, GladeEditorEditorClass))

typedef struct _GladeEditorTable        GladeEditorTable;
typedef struct _GladeEditorTableClass   GladeEditorTableClass;
typedef struct _GladeEditorTablePrivate GladeEditorTablePrivate;

struct _GladeEditorTable
{
  GtkGrid  parent;

  GladeEditorTablePrivate *priv;
};

struct _GladeEditorTableClass
{
  GtkGridClass parent;

  void   (* glade_reserved1)   (void);
  void   (* glade_reserved2)   (void);
  void   (* glade_reserved3)   (void);
  void   (* glade_reserved4)   (void);
};

GType            glade_editor_table_get_type (void);
GtkWidget       *glade_editor_table_new      (GladeWidgetAdaptor   *adaptor,
                                              GladeEditorPageType   type);

G_END_DECLS

#endif  /* _GLADE_EDITOR_TABLE_H_ */
