/*
 * glade-clipboard.c - An object for handling Cut/Copy/Paste.
 *
 * Copyright (C) 2005 The GNOME Foundation.
 *
 * Author(s):
 *      Tristan Van Berkom <tvb@gnome.org>
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

#ifndef __GLADE_BUILTINS_H__
#define __GLADE_BUILTINS_H__

#include <glib-object.h>
#include <gladeui/glade.h>

G_BEGIN_DECLS

typedef struct _GladeParamSpecObjects   GladeParamSpecObjects;


#define GLADE_TYPE_STOCK               (glade_standard_stock_get_type())
#define GLADE_TYPE_STOCK_IMAGE         (glade_standard_stock_image_get_type())
#define GLADE_TYPE_GLIST               (glade_glist_get_type())
#define GLADE_TYPE_PARAM_OBJECTS       (glade_param_objects_get_type())

#define GLADE_IS_STOCK(pspec) \
        (G_TYPE_CHECK_INSTANCE_TYPE ((pspec), GLADE_TYPE_STOCK))

#define GLADE_IS_STOCK_IMAGE(pspec) \
        (G_TYPE_CHECK_INSTANCE_TYPE ((pspec), GLADE_TYPE_STOCK_IMAGE))

#define GLADE_IS_PARAM_SPEC_OBJECTS(pspec)     \
        (G_TYPE_CHECK_INSTANCE_TYPE ((pspec),  \
         GLADE_TYPE_PARAM_OBJECTS))
#define GLADE_PARAM_SPEC_OBJECTS(pspec)        \
        (G_TYPE_CHECK_INSTANCE_CAST ((pspec),  \
         GLADE_TYPE_PARAM_OBJECTS, GladeParamSpecObjects))

GType        glade_standard_stock_get_type       (void) G_GNUC_CONST;
GType        glade_standard_stock_image_get_type (void) G_GNUC_CONST;
GType        glade_glist_get_type                (void) G_GNUC_CONST;
GType        glade_param_objects_get_type        (void) G_GNUC_CONST;

GParamSpec  *glade_param_spec_objects      (const gchar   *name,
                                            const gchar   *nick,
                                            const gchar   *blurb,
                                            GType          accepted_type,
                                            GParamFlags    flags);

void         glade_param_spec_objects_set_type (GladeParamSpecObjects *pspec,
                                                GType                  type);
GType        glade_param_spec_objects_get_type (GladeParamSpecObjects *pspec);

GParamSpec  *glade_standard_pixbuf_spec      (void);
GParamSpec  *glade_standard_gdkcolor_spec    (void);
GParamSpec  *glade_standard_objects_spec     (void);
GParamSpec  *glade_standard_stock_spec       (void);
GParamSpec  *glade_standard_stock_image_spec (void);
GParamSpec  *glade_standard_int_spec         (void);
GParamSpec  *glade_standard_uint_spec        (void);
GParamSpec  *glade_standard_string_spec      (void);
GParamSpec  *glade_standard_strv_spec        (void);
GParamSpec  *glade_standard_float_spec       (void);
GParamSpec  *glade_standard_boolean_spec     (void);

void         glade_standard_stock_append_prefix (const gchar *prefix);

G_END_DECLS

#endif   /* __GLADE_BUILTINS_H__ */
