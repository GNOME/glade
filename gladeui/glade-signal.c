/*
 * Copyright (C) 2001 Ximian, Inc.
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
 *
 * Authors:
 *   Chema Celorio <chema@celorio.com>
 *   Paolo Borelli <pborelli@katamail.com>
 */

#include "config.h"

#include <string.h>

#include <glib/gi18n-lib.h>

#include "glade.h"
#include "glade-signal.h"
#include "glade-xml-utils.h"

struct _GladeSignalPrivate
{
  const GladeSignalDef *def;   /* Pointer to the signal definition */
  gchar    *detail;       /* Signal detail */
  gchar    *handler;      /* Handler function eg "gtk_main_quit" */
  gchar    *userdata;     /* User data signal handler argument   */

  gchar    *support_warning;/* Message to inform the user about signals introduced in future versions */

  guint8    after : 1;    /* Connect after TRUE or FALSE         */
  guint8    swapped : 1;  /* Connect swapped TRUE or FALSE (GtkBuilder only) */
};

enum {
  PROP_0,
  PROP_CLASS,
  PROP_DETAIL,
  PROP_HANDLER,
  PROP_USERDATA,
  PROP_SUPPORT_WARNING,
  PROP_AFTER,
  PROP_SWAPPED,
  N_PROPERTIES
};

G_DEFINE_TYPE_WITH_PRIVATE (GladeSignal, glade_signal, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPERTIES];

static void
glade_signal_finalize (GObject *object)
{
  GladeSignal *signal = GLADE_SIGNAL (object);

  g_free (signal->priv->detail);
  g_free (signal->priv->handler);
  g_free (signal->priv->userdata);
  g_free (signal->priv->support_warning);

  G_OBJECT_CLASS (glade_signal_parent_class)->finalize (object);
}

static void
glade_signal_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GladeSignal *signal = GLADE_SIGNAL (object);

  switch (prop_id)
    {
      case PROP_CLASS:
        g_value_set_pointer (value, (gpointer) signal->priv->def);
        break;
      case PROP_DETAIL:
        g_value_set_string (value, signal->priv->detail);
        break;
      case PROP_HANDLER:
        g_value_set_string (value, signal->priv->handler);
        break;
      case PROP_USERDATA:
        g_value_set_string (value, signal->priv->userdata);
        break; 
      case PROP_SUPPORT_WARNING:
        g_value_set_string (value, signal->priv->support_warning);
        break; 
      case PROP_AFTER:
        g_value_set_boolean (value, signal->priv->after);
        break; 
      case PROP_SWAPPED:
        g_value_set_boolean (value, signal->priv->swapped);
        break; 
     default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_signal_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GladeSignal *signal = GLADE_SIGNAL (object);

  switch (prop_id)
    {
      case PROP_CLASS:
        signal->priv->def = g_value_get_pointer (value);
        break;
      case PROP_DETAIL:
        glade_signal_set_detail (signal, g_value_get_string (value));
        break;
      case PROP_HANDLER:
        glade_signal_set_handler (signal, g_value_get_string (value));
        break;
      case PROP_USERDATA:
        glade_signal_set_userdata (signal, g_value_get_string (value));
        break; 
      case PROP_SUPPORT_WARNING:
        glade_signal_set_support_warning (signal, g_value_get_string (value));
        break; 
      case PROP_AFTER:
        glade_signal_set_after (signal, g_value_get_boolean (value));
        break; 
      case PROP_SWAPPED:
        glade_signal_set_swapped (signal, g_value_get_boolean (value));
        break; 
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_signal_init (GladeSignal *signal)
{
  signal->priv = glade_signal_get_instance_private (signal);
}

static void
glade_signal_class_init (GladeSignalClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = glade_signal_set_property;
  object_class->get_property = glade_signal_get_property;
  object_class->finalize     = glade_signal_finalize;

  /* Properties */
  properties[PROP_CLASS] =
    g_param_spec_pointer ("class",
                          _("SignalClass"),
                          _("The signal class of this signal"),
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  properties[PROP_DETAIL] =
    g_param_spec_string ("detail",
                         _("Detail"),
                         _("The detail for this signal"),
                         NULL, G_PARAM_READWRITE);
  
  properties[PROP_HANDLER] =
    g_param_spec_string ("handler",
                         _("Handler"),
                         _("The handler for this signal"),
                         NULL, G_PARAM_READWRITE);

  properties[PROP_USERDATA] =
    g_param_spec_string ("userdata",
                         _("User Data"),
                         _("The user data for this signal"),
                         NULL, G_PARAM_READWRITE);

  properties[PROP_SUPPORT_WARNING] =
    g_param_spec_string ("support-warning",
                         _("Support Warning"),
                         _("The versioning support warning for this signal"),
                         NULL, G_PARAM_READWRITE);

  properties[PROP_AFTER] =
    g_param_spec_boolean ("after",
                          _("After"),
                          _("Whether this signal is run after default handlers"),
                          FALSE, G_PARAM_READWRITE);

  properties[PROP_SWAPPED] =
    g_param_spec_boolean ("swapped",
                          _("Swapped"),
                          _("Whether the user data is swapped with the instance for the handler"),
                          FALSE, G_PARAM_READWRITE);
  
  /* Install all properties */
  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

/**
 * glade_signal_new:
 * @sig_def: a #GladeSignalDef
 * @handler: a handler function for the signal
 * @userdata: the userdata for this signal
 * @after: whether this handler should be called after the default emission phase
 * @swapped: whether the handler's user data should be swapped with the emitter instance.
 *
 * Creates a new #GladeSignal with the given parameters.
 *
 * Returns: the new #GladeSignal
 */
GladeSignal *
glade_signal_new (const GladeSignalDef *sig_def,
                  const gchar          *handler,
                  const gchar          *userdata, 
                  gboolean              after, 
                  gboolean              swapped)
{
  g_return_val_if_fail (sig_def != NULL, NULL);

  return GLADE_SIGNAL (g_object_new (GLADE_TYPE_SIGNAL,
                                     "class", sig_def,
                                     "handler", handler,
                                     "userdata", userdata,
                                     "after", after,
                                     "swapped", swapped,
                                     NULL));
}

/**
 * glade_signal_equal:
 * @sig1: a #GladeSignal
 * @sig2: a #GladeSignal
 *
 * Returns: %TRUE if @sig1 and @sig2 have identical attributes, %FALSE otherwise
 */
gboolean
glade_signal_equal (const GladeSignal *sig1, const GladeSignal *sig2)
{
  gboolean ret = FALSE;

  g_return_val_if_fail (GLADE_IS_SIGNAL (sig1), FALSE);
  g_return_val_if_fail (GLADE_IS_SIGNAL (sig2), FALSE);

  /* Intentionally ignore support_warning */
  if (!g_strcmp0 (glade_signal_get_name (sig1), glade_signal_get_name (sig2)) &&
      !g_strcmp0 (sig1->priv->handler, sig2->priv->handler) &&
      !g_strcmp0 (sig1->priv->detail, sig2->priv->detail) &&
      sig1->priv->after == sig2->priv->after && sig1->priv->swapped == sig2->priv->swapped)
    {
      if ((sig1->priv->userdata == NULL && sig2->priv->userdata == NULL) ||
          (sig1->priv->userdata != NULL && sig2->priv->userdata != NULL &&
           !g_strcmp0 (sig1->priv->userdata, sig2->priv->userdata)))
        ret = TRUE;
    }

  return ret;
}

/**
 * glade_signal_clone:
 * @signal: a #GladeSignal
 *
 * Returns: (transfer full): a new #GladeSignal with the same attributes as @signal
 */
GladeSignal *
glade_signal_clone (const GladeSignal *signal)
{
  GladeSignal *dup;

  g_return_val_if_fail (GLADE_IS_SIGNAL (signal), NULL);

  dup = glade_signal_new (signal->priv->def,
                          signal->priv->handler,
                          signal->priv->userdata, 
                          signal->priv->after, 
                          signal->priv->swapped);

  glade_signal_set_detail (dup, signal->priv->detail);
  glade_signal_set_support_warning (dup, signal->priv->support_warning);

  return dup;
}

/**
 * glade_signal_write:
 * @signal: The #GladeSignal
 * @context: A #GladeXmlContext
 * @node: A #GladeXmlNode
 *
 * Writes @signal to @node
 */
void
glade_signal_write (GladeSignal     *signal,
                    GladeXmlContext *context,
                    GladeXmlNode    *node)
{
  GladeXmlNode *signal_node;
  gchar *name;

  /*  Should assert GLADE_XML_TAG_WIDGET tag here, but no 
   * access to project, so not really seriosly needed 
   */

  if (signal->priv->detail)
    name = g_strdup_printf ("%s::%s",
                            glade_signal_get_name (signal),
                            signal->priv->detail);
  else
    name = g_strdup (glade_signal_get_name (signal));

  /* Now dump the node values... */
  signal_node = glade_xml_node_new (context, GLADE_XML_TAG_SIGNAL);
  glade_xml_node_append_child (node, signal_node);

  glade_xml_node_set_property_string (signal_node, GLADE_XML_TAG_NAME, name);
  glade_xml_node_set_property_string (signal_node, GLADE_XML_TAG_HANDLER,
                                      signal->priv->handler);

  if (signal->priv->userdata)
    glade_xml_node_set_property_string (signal_node,
                                        GLADE_XML_TAG_OBJECT, signal->priv->userdata);

  if (signal->priv->after)
    glade_xml_node_set_property_string (signal_node,
                                        GLADE_XML_TAG_AFTER,
                                        GLADE_XML_TAG_SIGNAL_TRUE);

  /* Always serialize swapped regardless of format (libglade should not complain about this
   * and we prefer to not lose data in conversions).
   */
  glade_xml_node_set_property_string (signal_node,
                                      GLADE_XML_TAG_SWAPPED,
                                      signal->priv->swapped ?
                                      GLADE_XML_TAG_SIGNAL_TRUE :
                                      GLADE_XML_TAG_SIGNAL_FALSE);

  g_free (name);
}


/**
 * glade_signal_read:
 * @node: The #GladeXmlNode to read
 * @adaptor: The #GladeWidgetAdaptor for the widget
 *
 * Reads and creates a ner #GladeSignal based on @node
 *
 * Returns: (transfer full): A newly created #GladeSignal
 */
GladeSignal *
glade_signal_read (GladeXmlNode *node, GladeWidgetAdaptor *adaptor)
{
  GladeSignal *signal = NULL;
  GladeSignalDef *signal_def;
  gchar *name, *handler, *userdata, *detail;

  g_return_val_if_fail (glade_xml_node_verify_silent
                        (node, GLADE_XML_TAG_SIGNAL), NULL);

  if (!(name =
        glade_xml_get_property_string_required (node, GLADE_XML_TAG_NAME,
                                                NULL)))
    return NULL;
  glade_util_replace (name, '_', '-');

  /* Search for a detail, and strip it from the signal name */
  if ((detail = g_strstr_len (name, -1, "::"))) *detail = '\0';
  
  if (!(handler =
        glade_xml_get_property_string_required (node, GLADE_XML_TAG_HANDLER,
                                                NULL)))
    {
      g_free (name);
      return NULL;
    }

  userdata   = glade_xml_get_property_string (node, GLADE_XML_TAG_OBJECT);
  signal_def = glade_widget_adaptor_get_signal_def (adaptor, name);

  if (signal_def)
    {
      signal = glade_signal_new (signal_def,
                                 handler, userdata,
                                 glade_xml_get_property_boolean (node, GLADE_XML_TAG_AFTER, FALSE),
                                 glade_xml_get_property_boolean (node, GLADE_XML_TAG_SWAPPED,
                                                                 userdata != NULL));

      if (detail && detail[2]) glade_signal_set_detail (signal, &detail[2]);
    }
  else
    {
      /* XXX These errors should be collected and reported to the user */
      g_warning ("No signal %s was found for class %s, skipping\n", 
                 name, glade_widget_adaptor_get_name (adaptor));
    }

  g_free (name);
  g_free (handler);
  g_free (userdata);

  return signal;
}

/**
 * glade_signal_get_name:
 * @signal: a #GladeSignal
 *
 * Get the name of the signal from the signal definition.
 *
 * Returns: The name of the signal
 */
const gchar *
glade_signal_get_name (const GladeSignal *signal)
{
  g_return_val_if_fail (GLADE_IS_SIGNAL (signal), NULL);

  return glade_signal_def_get_name (signal->priv->def);
}

/**
 * glade_signal_get_def:
 * @signal: a #GladeSignal
 *
 * Get the signal definition.
 *
 * Returns: (transfer none): the #GladeSignalDef
 */
const GladeSignalDef *
glade_signal_get_def (const GladeSignal *signal)
{
  g_return_val_if_fail (GLADE_IS_SIGNAL (signal), NULL);

  return signal->priv->def;
}

/**
 * glade_signal_set_detail:
 * @signal: a #GladeSignal
 * @detail: the detail of the signal
 *
 * Set the detail of the signal.
 */
void
glade_signal_set_detail (GladeSignal *signal, const gchar *detail)
{
  g_return_if_fail (GLADE_IS_SIGNAL (signal));
  
  if (glade_signal_def_get_flags (signal->priv->def) & G_SIGNAL_DETAILED &&
      g_strcmp0 (signal->priv->detail, detail))
    {
      g_free (signal->priv->detail);
      signal->priv->detail = (detail && g_utf8_strlen (detail, -1)) ? g_strdup (detail) : NULL;
      g_object_notify_by_pspec (G_OBJECT (signal), properties[PROP_DETAIL]);
    }
}

/**
 * glade_signal_get_detail:
 * @signal: a #GladeSignal
 *
 * Get the signal detail.
 *
 * Returns: the detail of the signal
 */
const gchar *
glade_signal_get_detail (const GladeSignal *signal)
{
  g_return_val_if_fail (GLADE_IS_SIGNAL (signal), NULL);

  return signal->priv->detail;
}

/**
 * glade_signal_set_handler:
 * @signal: a #GladeSignal
 * @handler: the handler of the signal
 *
 * Set the handler of the signal.
 */
void
glade_signal_set_handler (GladeSignal *signal, const gchar *handler)
{
  g_return_if_fail (GLADE_IS_SIGNAL (signal));

  if (g_strcmp0 (signal->priv->handler, handler))
    {
      g_free (signal->priv->handler);
      signal->priv->handler =
          handler ? g_strdup (handler) : NULL;

      g_object_notify_by_pspec (G_OBJECT (signal), properties[PROP_HANDLER]);
    }
}

/**
 * glade_signal_get_handler:
 * @signal: a #GladeSignal
 *
 * Get the signal handler.
 *
 * Returns: the handler of the signal
 */
const gchar *
glade_signal_get_handler (const GladeSignal *signal)
{
  g_return_val_if_fail (GLADE_IS_SIGNAL (signal), NULL);

  return signal->priv->handler;
}

/**
 * glade_signal_set_userdata:
 * @signal: a #GladeSignal
 * @userdata: the userdata of the signal
 *
 * Set the userdata of the signal.
 */
void
glade_signal_set_userdata (GladeSignal *signal, const gchar *userdata)
{
  g_return_if_fail (GLADE_IS_SIGNAL (signal));

  if (g_strcmp0 (signal->priv->userdata, userdata))
    {
      g_free (signal->priv->userdata);
      signal->priv->userdata =
          userdata ? g_strdup (userdata) : NULL;

      g_object_notify_by_pspec (G_OBJECT (signal), properties[PROP_USERDATA]);
    }
}

/**
 * glade_signal_get_userdata:
 * @signal: a #GladeSignal
 *
 * Get the userdata handler.
 *
 * Returns: the userdata of the signal
 */
const gchar *
glade_signal_get_userdata (const GladeSignal *signal)
{
  g_return_val_if_fail (GLADE_IS_SIGNAL (signal), NULL);

  return signal->priv->userdata;
}

/**
 * glade_signal_set_after:
 * @signal: a #GladeSignal
 * @after: if the signal handler should be called after the default handler
 *
 * Set whether the handler should be called before or after the default handler
 * of the signal.
 */
void
glade_signal_set_after (GladeSignal *signal, gboolean after)
{
  g_return_if_fail (GLADE_IS_SIGNAL (signal));

  if (signal->priv->after != after)
    {
      signal->priv->after = after;

      g_object_notify_by_pspec (G_OBJECT (signal), properties[PROP_AFTER]);
    }
}

/**
 * glade_signal_get_after:
 * @signal: a #GladeSignal
 *
 * Get whether the handler should be called before or after the default handler
 * of the signal.
 *
 * Returns: Whether the signal handler should be called after the default handler
 */
gboolean
glade_signal_get_after (const GladeSignal *signal)
{
  g_return_val_if_fail (GLADE_IS_SIGNAL (signal), FALSE);

  return signal->priv->after;
}

/**
 * glade_signal_set_swapped:
 * @signal: a #GladeSignal
 * @swapped: if the instance and userdata arguments should be swapped
 *
 * Set whether the instance and userdata arguments should be swapped.
 */
void
glade_signal_set_swapped (GladeSignal *signal, gboolean swapped)
{
  g_return_if_fail (GLADE_IS_SIGNAL (signal));

  if (signal->priv->swapped != swapped)
    {
      signal->priv->swapped = swapped;

      g_object_notify_by_pspec (G_OBJECT (signal), properties[PROP_SWAPPED]);
    }
}

/**
 * glade_signal_get_swapped:
 * @signal: a #GladeSignal
 *
 * Get whether the instance and userdata arguments should be swapped.
 *
 * Returns: whether the instance and userdata arguments should be swapped
 */
gboolean
glade_signal_get_swapped (const GladeSignal *signal)
{
  g_return_val_if_fail (GLADE_IS_SIGNAL (signal), FALSE);

  return signal->priv->swapped;
}

void
glade_signal_set_support_warning (GladeSignal *signal,
                                  const gchar *support_warning)
{
  g_return_if_fail (GLADE_IS_SIGNAL (signal));

  if (g_strcmp0 (signal->priv->support_warning, support_warning))
    {
      g_free (signal->priv->support_warning);
      signal->priv->support_warning =
          support_warning ? g_strdup (support_warning) : NULL;

      g_object_notify_by_pspec (G_OBJECT (signal), properties[PROP_SUPPORT_WARNING]);
    }
}

const gchar *
glade_signal_get_support_warning (const GladeSignal *signal)
{
  g_return_val_if_fail (GLADE_IS_SIGNAL (signal), NULL);

  return signal->priv->support_warning;
}
