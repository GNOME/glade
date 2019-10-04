/*
 * Copyright (C) 2013 Tristan Van Berkom.
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
 * Authors:
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#ifndef __GLADE_PROJECT_PROPERTIES_H__
#define __GLADE_PROJECT_PROPERTIES_H__

#include <gtk/gtk.h>
#include <gladeui/glade-xml-utils.h>

G_BEGIN_DECLS

#define GLADE_TYPE_PROJECT_PROPERTIES glade_project_properties_get_type ()
G_DECLARE_DERIVABLE_TYPE (GladeProjectProperties, glade_project_properties, GLADE, PROJECT_PROPERTIES, GtkDialog)

struct _GladeProjectPropertiesClass
{
  GtkDialogClass parent_class;
};

GtkWidget        *glade_project_properties_new              (GladeProject *project);

G_END_DECLS

#endif /* __GLADE_PROJECT_PROPERTIES_H__ */
