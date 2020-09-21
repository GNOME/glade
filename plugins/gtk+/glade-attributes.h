/*
 * glade-attributes.h
 *
 * Copyright (C) 2008 Tristan Van Berkom
 *
 * Author(s):
 *      Tristan Van Berkom <tvb@gnome.org>
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
 */

#ifndef __GLADE_ATTRIBUTES_H__
#define __GLADE_ATTRIBUTES_H__

#include <glib-object.h>
#include <gladeui/glade.h>

G_BEGIN_DECLS


#define GLADE_TYPE_EPROP_ATTRS            (glade_eprop_attrs_get_type())
#define GLADE_TYPE_ATTR_GLIST             (glade_attr_glist_get_type())

/* The GladeParamSpecAttributes is a GList * of GladeAttribute structs */
typedef struct _GladeAttribute             GladeAttribute;

struct _GladeAttribute {
  PangoAttrType   type;   /* The type of pango attribute */

  GValue          value;  /* The corresponding value */

  guint           start;  /* The text offsets where the attributes should apply to */
  guint           end;
};


GType        glade_eprop_attrs_get_type         (void) G_GNUC_CONST;
GType        glade_attr_glist_get_type          (void) G_GNUC_CONST;

GladeAttribute *glade_gtk_attribute_from_string (PangoAttrType    type,
                                                 const gchar     *strval);
gchar       *glade_gtk_string_from_attr         (GladeAttribute  *gattr);
void         glade_attr_list_free               (GList           *attrs);


G_END_DECLS

#endif   /* __GLADE_ATTRIBUTES_H__ */
