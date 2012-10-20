/*
 * glade-composite-template.h
 *
 * Copyright (C) 2012 Juan Pablo Ugarte
 *
 * Author: Juan Pablo Ugarte <juanpablougarte@gmail.com>
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

#ifndef __GLADE_COMPOSITE_TEMPLATE_H__
#define __GLADE_COMPOSITE_TEMPLATE_H__

#include <gladeui/glade-widget.h>

G_BEGIN_DECLS

GladeWidgetAdaptor *glade_composite_template_load_from_string (const gchar *template_xml);

GladeWidgetAdaptor *glade_composite_template_load_from_file (const gchar *path);

void glade_composite_template_load_directory (const gchar *directory);

void glade_composite_template_save_from_widget (GladeWidget *gwidget,
                                                const gchar *template_class,
                                                const gchar *filename,
                                                gboolean     replace);

G_END_DECLS

#endif /* __GLADE_COMPOSITE_TEMPLATE_H__ */
