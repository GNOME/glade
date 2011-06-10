/*
 * Copyright (C) 2011 Denis Washington
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
 *   Denis Washington <denisw@online.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "glade-binding.h"
#include "glade-property.h"
#include "glade-project.h"

#define GLADE_BINDING_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object),  \
					   GLADE_TYPE_BINDING,                     \
					   GladeBindingPrivate))

struct _GladeBindingPrivate {

  GladeProperty *target;     /* A pointer to the the binding's target
                              * GladeProperty
                              */

  GladeProperty *source;     /* A pointer to the the binding's source
                              * GladeProperty
                              */

  /* Set by glade_binding_read() for glade_binding_complete() */
  gchar *source_object_name;
  gchar *source_property_name;
};

enum {
  PROP_0,
  PROP_SOURCE,
  PROP_TARGET,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

static void glade_binding_class_init    (GladeBindingClass * klass);
static void glade_binding_init          (GladeBinding      * binding);
static void glade_binding_finalize      (GObject           * object);

G_DEFINE_TYPE (GladeBinding, glade_binding, G_TYPE_OBJECT)

/*******************************************************************************
                      GObjectClass & Object Construction
 *******************************************************************************/
static void
glade_binding_init (GladeBinding *binding)
{
  GladeBindingPrivate *priv;

  priv = GLADE_BINDING_GET_PRIVATE (GLADE_BINDING (binding));
  priv->source = NULL;
  priv->target = NULL;
  binding->priv = priv;
}

static void
glade_binding_finalize (GObject *object)
{
  GladeBindingPrivate *priv = GLADE_BINDING_GET_PRIVATE (GLADE_BINDING (object));

  if (priv->source_object_name)
    g_free (priv->source_object_name);

  if (priv->source_property_name)
    g_free (priv->source_property_name);
}

static void
glade_binding_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GladeBinding *binding = GLADE_BINDING (object);

  switch (prop_id)
    {
    case PROP_SOURCE:
      g_value_set_object (value, glade_binding_get_source (binding));
      break;
    case PROP_TARGET:
      g_value_set_object (value, glade_binding_get_target (binding));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glade_binding_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GladeBindingPrivate *priv = GLADE_BINDING_GET_PRIVATE (object);

  switch (prop_id)
    {
      case PROP_SOURCE:
        priv->source = g_value_get_object (value);
        break;
      case PROP_TARGET:
        priv->target = g_value_get_object (value);
        g_assert (priv->target != NULL);
        glade_property_set_binding (priv->target, GLADE_BINDING (object));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_binding_class_init (GladeBindingClass *klass)
{
  GObjectClass *object_class;
  g_return_if_fail (klass != NULL);

  object_class = G_OBJECT_CLASS (klass);

  /* GObjectClass */
  object_class->get_property = glade_binding_get_property;
  object_class->set_property = glade_binding_set_property;
  object_class->finalize = glade_binding_finalize;

  /* Properties */
  properties[PROP_TARGET] =
    g_param_spec_object ("target",
                         _("Target"),
                         _("The GladeBinding's target property"),
                         GLADE_TYPE_PROPERTY,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  properties[PROP_SOURCE] =
    g_param_spec_object ("source",
                         _("Source"),
                         _("The GladeBinding's source property"),
                         GLADE_TYPE_PROPERTY,
                         G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /* Private */
  g_type_class_add_private (klass, sizeof (GladeBindingPrivate));
}

/*******************************************************************************
                                     API
 *******************************************************************************/
/**
 * glade_binding_new:
 * @source: The binding source property
 * @target: The binding target property
 *
 * Create a new GladeBinding with the specified @source and
 * @target property.
 *
 * Returns: The newly created #GladeBinding
 */
GladeBinding *
glade_binding_new (GladeProperty *source,
                   GladeProperty *target)
{
  return g_object_new (GLADE_TYPE_BINDING,
                       "source", source,
                       "target", target,
                       NULL);
}

/**
 * glade_binding_get_target:
 * @binding: The #GladeBinding
 *
 * Returns: The binding's target property
 */
GladeProperty *
glade_binding_get_target (GladeBinding *binding)
{
  g_return_val_if_fail (GLADE_IS_BINDING (binding), NULL);
  return GLADE_BINDING_GET_PRIVATE (binding)->target;
}

/**
 * glade_binding_get_target:
 * @binding: The #GladeBinding
 *
 * Returns: The binding's source property
 */
GladeProperty *
glade_binding_get_source (GladeBinding *binding)
{
  g_return_val_if_fail (GLADE_IS_BINDING (binding), NULL);
  return GLADE_BINDING_GET_PRIVATE (binding)->source;
}

/**
 * glade_binding_read:
 * @node: The #GladeXmlNode to read from
 * @widget: The widget to which the target property belongs
 *
 * Read the binding information from @node and create a new
 * #GladeBinding from it, which is returned.
 *
 * Note that binding's source property will only be resolved
 * after the project is completely loaded by calling
 * glade_binding_complete().
 *
 * Returns: The read #GladeBinding
 */
GladeBinding *
glade_binding_read (GladeXmlNode *node,
                    GladeWidget  *widget)
{
  gchar *to, *from, *source;
  GladeProperty *target;
  GladeBinding *binding;
  GladeBindingPrivate *priv;
  
  g_return_val_if_fail (node != NULL, NULL);
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

  if (!glade_xml_node_verify (node, GLADE_XML_TAG_BINDING))
    return NULL;

  to = glade_xml_get_property_string_required (node, GLADE_XML_TAG_TO, NULL);
  from = glade_xml_get_property_string_required (node, GLADE_XML_TAG_FROM, NULL);
  source = glade_xml_get_property_string_required (node, GLADE_XML_TAG_SOURCE, NULL);
  
  if (!to || !from || !source)
    return NULL;

  target = glade_widget_get_property (widget, to);
  binding = glade_binding_new (NULL, target);

  /* We need to wait with resolving the source property until the complete
   * project is loaded, as the object referred to might not have been read
   * in yet.
   */
  priv = GLADE_BINDING_GET_PRIVATE (binding);
  priv->source_object_name = source;
  priv->source_property_name = from;

  g_free (to);
  
  return binding;
}

/**
 * glade_binding_complete:
 * @binding: The #GladeBinding
 * @project: The containing #GladeProject
 *
 * Resolves the source property of a #GladeBinding read from
 * a file with glade_binding_read() by looking it up in the
 * passed #GladeProject (which must be completely loaded).
 */
void
glade_binding_complete (GladeBinding *binding,
                        GladeProject *project)
{
  GladeBindingPrivate *priv;
  gchar *source_obj, *source_prop;
  GladeWidget *widget;
  
  g_return_if_fail (GLADE_IS_BINDING (binding));
  g_return_if_fail (GLADE_IS_PROJECT (project));

  priv = GLADE_BINDING_GET_PRIVATE (binding);
  source_obj = priv->source_object_name; 
  source_prop = priv->source_property_name;

  /* If the binding has no unresolved source property name attached,
   * there is nothing to do.
   */
  if (!source_obj)
    {
      g_assert (source_prop == NULL);
      return;
    }

  widget = glade_project_get_widget_by_name (project, source_obj);
  if (widget)
    {
      GladeProperty *source = glade_widget_get_property (widget, source_prop);
      if (source)
        priv->source = source;
    }

  g_free (source_obj);
  g_free (source_prop);
  priv->source_object_name = NULL;
  priv->source_property_name = NULL;  
}

/**
 * glade_binding_write:
 * @binding: a #GladeBinding
 * @context: A #GladeXmlContext
 * @node: A #GladeXmlNode
 *
 * Write @binding to @node.
 */
void
glade_binding_write (GladeBinding    *binding,
                     GladeXmlContext *context,
                     GladeXmlNode    *node)
{
  GladeXmlNode *binding_node;
  GladeProperty *target_prop, *source_prop;
  const gchar *to, *from, *source;
  GladeWidget *widget;
  
  g_return_if_fail (GLADE_IS_BINDING (binding));
  g_return_if_fail (glade_binding_get_target (binding) != NULL);
  g_return_if_fail (glade_binding_get_source (binding) != NULL);
  g_return_if_fail (node != NULL);
  
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET)))
    return;

  binding_node = glade_xml_node_new (context, GLADE_XML_TAG_BINDING);
  glade_xml_node_append_child (node, binding_node);

  target_prop = glade_binding_get_target (binding);
  source_prop = glade_binding_get_source (binding);  
  to = glade_property_class_id (glade_property_get_class (target_prop));
  from = glade_property_class_id (glade_property_get_class (source_prop));

  widget = glade_property_get_widget (source_prop);
  source = glade_widget_get_name (widget);
  
  glade_xml_node_set_property_string (binding_node,
                                      GLADE_XML_TAG_TO,
                                      to);
  glade_xml_node_set_property_string (binding_node,
                                      GLADE_XML_TAG_FROM,
                                      from);
  glade_xml_node_set_property_string (binding_node,
                                      GLADE_XML_TAG_SOURCE,
                                      source);
}
