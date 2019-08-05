/* glade-signal-def.c
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


#include <config.h>
#include "glade-signal-def.h"
#include "glade-widget-adaptor.h"

struct _GladeSignalDef
{
  GladeWidgetAdaptor *adaptor; /* The adaptor that originated this signal.
                                */
  GSignalQuery        query;

  guint16             version_since_major; /* Version in which this signal was */
  guint16             version_since_minor; /* introduced
                                            */

  const gchar        *name;                /* Name of the signal, eg clicked */
  const gchar        *type;                /* Name of the object class that this signal 
                                            * belongs to eg GtkButton */

  guint deprecated : 1;                    /* True if this signal is deprecated */
};

G_DEFINE_BOXED_TYPE(GladeSignalDef, glade_signal_def, glade_signal_def_clone, glade_signal_def_free)

/**
 * glade_signal_def_new:
 * @adaptor: a #GladeWidgetAdaptor
 * @for_type: a #GType
 * @signal_id: the signal id
 *
 * Creates a new #GladeSignalDef
 *
 * Returns: (transfer full): a new #GladeSignalDef
 */
GladeSignalDef *
glade_signal_def_new  (GladeWidgetAdaptor *adaptor,
                       GType               for_type,
                       guint               signal_id)
{
  GladeSignalDef *def;

  def = g_slice_new0 (GladeSignalDef);

  def->adaptor = adaptor;

  /* Since glib gave us this signal id... it should
   * exist no matter what.
   */
  g_signal_query (signal_id, &(def->query));
  if (def->query.signal_id == 0)
    {
      g_critical ("glade_signal_def_new() called with an invalid signal id");

      glade_signal_def_free (def);
      return NULL;
    }

  def->name = (def->query.signal_name);
  def->type = g_type_name (for_type);

  /* Initialize signal versions & deprecated to adaptor version */
  def->version_since_major = GWA_VERSION_SINCE_MAJOR (adaptor);
  def->version_since_minor = GWA_VERSION_SINCE_MINOR (adaptor);
  def->deprecated          = GWA_DEPRECATED (adaptor);

  return def;
}

/**
 * glade_signal_def_clone:
 * @signal_def: a #GladeSignalDef
 *
 * Clones a #GladeSignalDef
 *
 * Returns: (transfer full): a new copy of @signal_def
 */
GladeSignalDef *
glade_signal_def_clone (GladeSignalDef *signal_def)
{
  GladeSignalDef *clone;

  clone = g_slice_new0 (GladeSignalDef);

  memcpy (clone, signal_def, sizeof (GladeSignalDef));
  return clone;
}

/**
 * glade_signal_def_free:
 * @signal_def: a #GladeSignalDef
 *
 * Frees a #GladeSignalDef
 */
void
glade_signal_def_free (GladeSignalDef *signal_def)
{
  if (signal_def == NULL)
    return;

  g_slice_free (GladeSignalDef, signal_def);
}

void
glade_signal_def_update_from_node (GladeSignalDef *signal_def,
                                   GladeXmlNode     *node,
                                   const gchar      *domain)
{
  g_return_if_fail (signal_def != NULL);
  g_return_if_fail (node != NULL);

  glade_xml_get_property_version (node, GLADE_TAG_VERSION_SINCE,
                                  &signal_def->version_since_major, 
                                  &signal_def->version_since_minor);

  signal_def->deprecated =
    glade_xml_get_property_boolean (node,
                                    GLADE_TAG_DEPRECATED,
                                    signal_def->deprecated);
}

/**
 * glade_signal_def_get_adaptor:
 * @signal_def: a #GladeSignalDef
 *
 * Get #GladeWidgetAdaptor associated with the signal.
 *
 * Returns: (transfer none): a #GladeWidgetAdaptor
 */
GladeWidgetAdaptor *
glade_signal_def_get_adaptor (const GladeSignalDef *signal_def)
{
  g_return_val_if_fail (signal_def != NULL, NULL);

  return signal_def->adaptor;
}

/**
 * glade_signal_def_get_name:
 * @signal_def: a #GladeSignalDef
 *
 * Get the name of the signal.
 *
 * Returns: the name of the signal
 */
const gchar *
glade_signal_def_get_name (const GladeSignalDef *signal_def)
{
  g_return_val_if_fail (signal_def != NULL, NULL);

  return signal_def->name;
}

/**
 * glade_signal_def_get_object_type_name:
 * @signal_def: a #GladeSignalDef
 *
 * Get the name of the object class that this signal belongs to.
 *
 * Returns: the type name of the signal
 */
const gchar *
glade_signal_def_get_object_type_name (const GladeSignalDef *signal_def)
{
  g_return_val_if_fail (signal_def != NULL, NULL);

  return signal_def->type;
}

/**
 * glade_signal_def_get_flags:
 * @signal_def: a #GladeSignalDef
 *
 * Get the combination of GSignalFlags specifying detail of when the default
 * handler is to be invoked.
 *
 * Returns: the #GSignalFlags
 */
GSignalFlags
glade_signal_def_get_flags (const GladeSignalDef *signal_def)
{
  g_return_val_if_fail (signal_def != NULL, 0);

  return signal_def->query.signal_flags;
}

/**
 * glade_signal_def_set_since:
 * @signal_def: a #GladeSignalDef
 * @since_major: the major version.
 * @since_minor: the minor version.
 *
 * Set the major and minor version that introduced this signal.
 */
void
glade_signal_def_set_since (GladeSignalDef *signal_def,
                            guint16           since_major,
                            guint16           since_minor)
{
  g_return_if_fail (signal_def != NULL);

  signal_def->version_since_major = since_major;
  signal_def->version_since_minor = since_minor;
}

/**
 * glade_signal_def_since_major:
 * @signal_def: a #GladeSignalDef
 *
 * Get the major version that introduced this signal.
 *
 * Returns: the major version
 */
guint16
glade_signal_def_since_major (GladeSignalDef *signal_def)
{
  g_return_val_if_fail (signal_def != NULL, 0);

  return signal_def->version_since_major;
}

/**
 * glade_signal_def_since_minor:
 * @signal_def: a #GladeSignalDef
 *
 * Get the minor version that introduced this signal.
 *
 * Returns: the minor version
 */
guint16
glade_signal_def_since_minor (GladeSignalDef *signal_def)
{
  g_return_val_if_fail (signal_def != NULL, 0);

  return signal_def->version_since_minor;
}

/**
 * glade_signal_def_set_deprecated:
 * @signal_def: a #GladeSignalDef
 * @deprecated: %TRUE if the signal is deprecated
 *
 * Set if the signal is deprecated.
 */
void
glade_signal_def_set_deprecated (GladeSignalDef *signal_def,
                                 gboolean          deprecated)
{
  g_return_if_fail (signal_def != NULL);

  signal_def->deprecated = deprecated;
}

/**
 * glade_signal_def_deprecated:
 * @signal_def: a #GladeSignalDef
 *
 * Get if the signal is flagged as deprecated.
 *
 * Returns: %TRUE if the signal is deprecated
 */
gboolean
glade_signal_def_deprecated (GladeSignalDef *signal_def)
{
  g_return_val_if_fail (signal_def != NULL, FALSE);

  return signal_def->deprecated;
}
