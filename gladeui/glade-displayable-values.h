/*
 * glade-displayable-values.h
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

#ifndef __GLADE_DISAPLAYABLE_VALUES_H__
#define __GLADE_DISAPLAYABLE_VALUES_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

void        glade_register_displayable_value      (GType          type, 
                                                   const gchar   *value, 
                                                   const gchar   *domain,
                                                   const gchar   *string);

void        glade_register_translated_value       (GType          type, 
                                                   const gchar   *value, 
                                                   const gchar   *string);

gboolean    glade_type_has_displayable_values     (GType          type);

const 
gchar      *glade_get_displayable_value           (GType          type, 
                                                   const gchar   *value);

gboolean    glade_displayable_value_is_disabled   (GType          type, 
                                                   const gchar   *value);

void        glade_displayable_value_set_disabled  (GType type,
                                                   const gchar *value,
                                                   gboolean disabled);

const 
gchar      *glade_get_value_from_displayable      (GType          type, 
                                                   const gchar   *displayabe);

G_END_DECLS

#endif /* __GLADE_DISAPLAYABLE_VALUES_H__ */
