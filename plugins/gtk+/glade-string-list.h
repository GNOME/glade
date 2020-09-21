/*
 * glade-string-list.h
 *
 * Copyright (C) 2010 Openismus GmbH
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

#ifndef __GLADE_STRING_LIST_H__
#define __GLADE_STRING_LIST_H__

#include <glib-object.h>
#include <gladeui/glade.h>

G_BEGIN_DECLS


#define GLADE_TYPE_EPROP_STRING_LIST       (glade_eprop_string_list_get_type())
#define GLADE_TYPE_STRING_LIST             (glade_string_list_get_type())

/* The GladeStringList boxed type is a GList * of GladeString structs */
typedef struct _GladeString             GladeString;

struct _GladeString {
  gchar    *string;
  gchar    *comment;
  gchar    *context;
  gchar    *id;
  gboolean  translatable;
};

GType        glade_eprop_string_list_get_type    (void) G_GNUC_CONST;
GType        glade_string_list_get_type          (void) G_GNUC_CONST;

void         glade_string_list_free              (GList         *list);
GList       *glade_string_list_copy              (GList         *list);

GList       *glade_string_list_append            (GList         *list,
                                                  const gchar   *string,
                                                  const gchar   *comment,
                                                  const gchar   *context,
                                                  gboolean       translatable,
                                                  const gchar   *id);

gchar       *glade_string_list_to_string         (GList         *list);

GladeEditorProperty *glade_eprop_string_list_new (GladePropertyDef *pdef,
                                                  gboolean          use_command,
                                                  gboolean          translatable,
                                                  gboolean          with_id);

G_END_DECLS

#endif   /* __GLADE_STRING_LIST_H__ */
