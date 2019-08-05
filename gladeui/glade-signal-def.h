/* glade-signal-def.h
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

#ifndef _GLADE_SIGNAL_DEF_H_
#define _GLADE_SIGNAL_DEF_H_

#include <glib-object.h>
#include <gladeui/glade-xml-utils.h>

G_BEGIN_DECLS

/**
 * GSC_VERSION_CHECK:
 * @klass: A #GladeSignalDef
 * @major_version: The major version to check
 * @minor_version: The minor version to check
 *
 * Evaluates to %TRUE if @klass is available in its owning library version-@major_verion.@minor_version.
 *
 */
#define GSC_VERSION_CHECK(klass, major_version, minor_version)                      \
  ((glade_signal_def_since_major (GLADE_SIGNAL_DEF (klass)) == major_version) ? \
   (glade_signal_def_since_minor (GLADE_SIGNAL_DEF (klass)) <= minor_version) : \
   (glade_signal_def_since_major (GLADE_SIGNAL_DEF (klass)) <= major_version))


#define GLADE_SIGNAL_DEF(klass) ((GladeSignalDef *)(klass))

#define GLADE_TYPE_SIGNAL_DEF glade_signal_def_get_type ()

typedef struct _GladeSignalDef GladeSignalDef;

GType                 glade_signal_def_get_type                 (void) G_GNUC_CONST;
GladeSignalDef       *glade_signal_def_new                      (GladeWidgetAdaptor   *adaptor,
                                                                 GType                 for_type,
                                                                 guint                 signal_id);
GladeSignalDef       *glade_signal_def_clone                    (GladeSignalDef       *signal_def);
void                  glade_signal_def_free                     (GladeSignalDef       *signal_def);
void                  glade_signal_def_update_from_node         (GladeSignalDef       *signal_def,
                                                                 GladeXmlNode         *node,
                                                                 const gchar          *domain);

GladeWidgetAdaptor   *glade_signal_def_get_adaptor              (const GladeSignalDef *signal_def);
const gchar *glade_signal_def_get_name                 (const GladeSignalDef *signal_def);
const gchar *glade_signal_def_get_object_type_name     (const GladeSignalDef *signal_def);
GSignalFlags          glade_signal_def_get_flags                (const GladeSignalDef *signal_def);

void                  glade_signal_def_set_since                (GladeSignalDef       *signal_def,
                                                                 guint16               since_major,
                                                                 guint16               since_minor);
guint16               glade_signal_def_since_major              (GladeSignalDef       *signal_def);
guint16               glade_signal_def_since_minor              (GladeSignalDef       *signal_def);

void                  glade_signal_def_set_deprecated           (GladeSignalDef       *signal_def,
                                                                 gboolean              deprecated);
gboolean              glade_signal_def_deprecated               (GladeSignalDef       *signal_def);


G_END_DECLS

#endif /* _GLADE_SIGNAL_DEF_H_ */
