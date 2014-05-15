/*
 * glade-private.h: miscellaneous private API
 * 
 * This is a placeholder for private API, eventually it should be replaced by
 * proper public API or moved to its own private file.
 *
 * Copyright (C) 2013  Juan Pablo Ugarte
 * 
 * Authors:
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
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

#ifndef __GLADE_PRIVATE_H__
#define __GLADE_PRIVATE_H__

#include "glade-widget.h"
#include "glade-project-properties.h"

G_BEGIN_DECLS

/* glade-widget.c */

GList *_glade_widget_peek_prop_refs (GladeWidget *widget);

/* glade-catalog.c */

GladeCatalog *_glade_catalog_get_catalog (const gchar *name);
GList        *_glade_catalog_tsort       (GList *catalogs);


/* glade-project-properties.c */
void
_glade_project_properties_set_license_data (GladeProjectProperties *props,
                                            const gchar *license,
                                            const gchar *name,
                                            const gchar *description,
                                            const gchar *copyright,
                                            const gchar *authors);
void
_glade_project_properties_get_license_data (GladeProjectProperties *props,
                                            gchar **license,
                                            gchar **name,
                                            gchar **description,
                                            gchar **copyright,
                                            gchar **authors);

/* glade-utils.c */

void   _glade_util_dialog_set_hig (GtkDialog *dialog);

gchar *_glade_util_strreplace (gchar *str,
                               gboolean free_str,
                               const gchar *key,
                               const gchar *replacement);

/* glade-xml-utils.c */

/* GladeXml Error handling */
void    _glade_xml_error_reset_last       (void);
gchar  *_glade_xml_error_get_last_message (void);

G_END_DECLS

#endif /* __GLADE_PRIVATE_H__ */
