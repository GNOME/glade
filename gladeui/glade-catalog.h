/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2004 Imendio AB
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
 */

#ifndef __GLADE_CATALOG_H__
#define __GLADE_CATALOG_H__

G_BEGIN_DECLS

#define GLADE_CATALOG(c) ((GladeCatalog *) c)
#define GLADE_IS_CATALOG(c) (c != NULL)

#define GLADE_WIDGET_GROUP(x) ((GladeWidgetGroup *) x)

typedef struct _GladeCatalog       GladeCatalog;
typedef struct _GladeWidgetGroup   GladeWidgetGroup;


LIBGLADEUI_API 
GList *       glade_catalog_load_all                (void);

LIBGLADEUI_API 
const gchar * glade_catalog_get_name                (GladeCatalog     *catalog);

LIBGLADEUI_API 
GList *       glade_catalog_get_widget_groups       (GladeCatalog     *catalog);
LIBGLADEUI_API 
GList *       glade_catalog_get_adaptors            (GladeCatalog     *catalog);

LIBGLADEUI_API 
void          glade_catalog_free                    (GladeCatalog     *catalog);

LIBGLADEUI_API 
const gchar * glade_widget_group_get_name           (GladeWidgetGroup *group);
LIBGLADEUI_API 
const gchar * glade_widget_group_get_title          (GladeWidgetGroup *group);
LIBGLADEUI_API 
gboolean      glade_widget_group_get_expanded       (GladeWidgetGroup *group);
LIBGLADEUI_API 
GList *       glade_widget_group_get_adaptors       (GladeWidgetGroup *group);

LIBGLADEUI_API 
void          glade_widget_group_free               (GladeWidgetGroup *group);

LIBGLADEUI_API 
gboolean      glade_catalog_is_loaded               (const gchar      *name);

LIBGLADEUI_API
void          glade_catalog_modules_close           (void);

G_END_DECLS

#endif /* __GLADE_CATALOG_H__ */
