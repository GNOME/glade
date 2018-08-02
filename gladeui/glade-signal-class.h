/* glade-signal-class.h
 *
 * Copyright (C) 2011 Openismus GmbH
 *
 * Authors:
 *      Tristan Van Berkom <tristanvb@openismus.com>
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

#ifndef _GLADE_SIGNAL_CLASS_H_
#define _GLADE_SIGNAL_CLASS_H_

#include <glib-object.h>
#include <gladeui/glade-xml-utils.h>

G_BEGIN_DECLS

/**
 * GSC_VERSION_CHECK:
 * @klass: A #GladeSignalClass
 * @major_version: The major version to check
 * @minor_version: The minor version to check
 *
 * Evaluates to %TRUE if @klass is available in its owning library version-@major_verion.@minor_version.
 *
 */
#define GSC_VERSION_CHECK(klass, major_version, minor_version)                      \
  ((glade_signal_class_since_major (GLADE_SIGNAL_CLASS (klass)) == major_version) ? \
   (glade_signal_class_since_minor (GLADE_SIGNAL_CLASS (klass)) <= minor_version) : \
   (glade_signal_class_since_major (GLADE_SIGNAL_CLASS (klass)) <= major_version))


#define GLADE_SIGNAL_CLASS(klass) ((GladeSignalClass *)(klass))

typedef struct _GladeSignalClass GladeSignalClass;

GladeSignalClass     *glade_signal_class_new                      (GladeWidgetAdaptor *adaptor,
                                                                   GType               for_type,
                                                                   guint               signal_id);
void                  glade_signal_class_free                     (GladeSignalClass   *signal_class);
void                  glade_signal_class_update_from_node         (GladeSignalClass   *signal_class,
                                                                   GladeXmlNode       *node,
                                                                   const gchar        *domain);

GladeWidgetAdaptor   *glade_signal_class_get_adaptor              (const GladeSignalClass   *signal_class);
G_CONST_RETURN gchar *glade_signal_class_get_name                 (const GladeSignalClass   *signal_class);
G_CONST_RETURN gchar *glade_signal_class_get_type                 (const GladeSignalClass   *signal_class);
GSignalFlags          glade_signal_class_get_flags                (const GladeSignalClass   *signal_class);

void                  glade_signal_class_set_since                (GladeSignalClass   *signal_class,
                                                                   guint16             since_major,
                                                                   guint16             since_minor);
guint16               glade_signal_class_since_major              (GladeSignalClass   *signal_class);
guint16               glade_signal_class_since_minor              (GladeSignalClass   *signal_class);

void                  glade_signal_class_set_deprecated           (GladeSignalClass   *signal_class,
                                                                   gboolean            deprecated);
gboolean              glade_signal_class_deprecated               (GladeSignalClass   *signal_class);


G_END_DECLS

#endif /* _GLADE_SIGNAL_CLASS_H_ */
