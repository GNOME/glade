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
#ifndef __GLADE_PROPERTY_LABEL_H__
#define __GLADE_PROPERTY_LABEL_H__

#include <gtk/gtk.h>
#include <gladeui/glade-xml-utils.h>
#include <gladeui/glade-property-def.h>
#include <gladeui/glade-property.h>

G_BEGIN_DECLS

#define GLADE_TYPE_PROPERTY_LABEL            (glade_property_label_get_type ())
#define GLADE_PROPERTY_LABEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PROPERTY_LABEL, GladePropertyLabel))
#define GLADE_PROPERTY_LABEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PROPERTY_LABEL, GladePropertyLabelClass))
#define GLADE_IS_PROPERTY_LABEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PROPERTY_LABEL))
#define GLADE_IS_PROPERTY_LABEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PROPERTY_LABEL))
#define GLADE_PROPERTY_LABEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_PROPERTY_LABEL, GladePropertyLabelClass))

typedef struct _GladePropertyLabel             GladePropertyLabel;
typedef struct _GladePropertyLabelClass        GladePropertyLabelClass;
typedef struct _GladePropertyLabelPrivate      GladePropertyLabelPrivate;

struct _GladePropertyLabel
{
  /*< private >*/
  GtkEventBox box;

  GladePropertyLabelPrivate *priv;
};

struct _GladePropertyLabelClass
{
  GtkEventBoxClass parent_class;
};

GLADEUI_EXPORTS
GType          glade_property_label_get_type          (void) G_GNUC_CONST;

GLADEUI_EXPORTS
GtkWidget     *glade_property_label_new               (void);

GLADEUI_EXPORTS
void           glade_property_label_set_property_name (GladePropertyLabel *label,
                                                       const gchar        *property_name);
GLADEUI_EXPORTS
const gchar   *glade_property_label_get_property_name (GladePropertyLabel *label);
GLADEUI_EXPORTS
void           glade_property_label_set_append_colon  (GladePropertyLabel *label,
                                                       gboolean            append_colon);
GLADEUI_EXPORTS
gboolean       glade_property_label_get_append_colon  (GladePropertyLabel *label);
GLADEUI_EXPORTS
void           glade_property_label_set_packing       (GladePropertyLabel *label,
                                                       gboolean            packing);
GLADEUI_EXPORTS
gboolean       glade_property_label_get_packing       (GladePropertyLabel *label);

GLADEUI_EXPORTS
void           glade_property_label_set_custom_text   (GladePropertyLabel *label,
                                                       const gchar        *custom_text);
GLADEUI_EXPORTS
const gchar   *glade_property_label_get_custom_text   (GladePropertyLabel *label);
GLADEUI_EXPORTS
void           glade_property_label_set_custom_tooltip(GladePropertyLabel *label,
                                                       const gchar        *custom_tooltip);
GLADEUI_EXPORTS
const gchar   *glade_property_label_get_custom_tooltip(GladePropertyLabel *label);

GLADEUI_EXPORTS
void           glade_property_label_set_property      (GladePropertyLabel *label,
                                                       GladeProperty      *property);
GLADEUI_EXPORTS
GladeProperty *glade_property_label_get_property      (GladePropertyLabel *label);

G_END_DECLS

#endif /* __GLADE_PROPERTY_LABEL_H__ */
