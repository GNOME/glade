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

#define GLADE_TYPE_PROJECT_PROPERTIES             (glade_project_properties_get_type ())
#define GLADE_PROJECT_PROPERTIES(obj)             (G_TYPE_CHECK_INSTANCE_CAST \
                                                   ((obj), GLADE_TYPE_PROJECT_PROPERTIES, GladeProjectProperties))
#define GLADE_PROJECT_PROPERTIES_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST \
                                                   ((klass), GLADE_TYPE_PROJECT_PROPERTIES, GladeProjectPropertiesClass))
#define GLADE_IS_PROJECT_PROPERTIES(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PROJECT_PROPERTIES))
#define GLADE_IS_PROJECT_PROPERTIES_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PROJECT_PROPERTIES))
#define GLADE_PROJECT_PROPERTIES_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS \
                                                   ((obj), GLADE_TYPE_PROJECT_PROPERTIES, GladeProjectPropertiesClass))

typedef struct _GladeProjectProperties             GladeProjectProperties;
typedef struct _GladeProjectPropertiesClass        GladeProjectPropertiesClass;
typedef struct _GladeProjectPropertiesPrivate      GladeProjectPropertiesPrivate;

struct _GladeProjectProperties
{
  /*< private >*/
  GtkDialog dialog;

  GladeProjectPropertiesPrivate *priv;
};

struct _GladeProjectPropertiesClass
{
  GtkDialogClass parent_class;
};

GType             glade_project_properties_get_type         (void) G_GNUC_CONST;
GtkWidget        *glade_project_properties_new              (GladeProject *project);

G_END_DECLS

#endif /* __GLADE_PROJECT_PROPERTIES_H__ */
