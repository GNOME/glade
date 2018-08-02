/*
 * Copyright (C) 2007 Juan Pablo Ugarte.
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
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

/**
 * SECTION:glade-widget-action
 * @Title: GladeWidgetAction
 * @Short_Description: Context menu and toolbar actions.
 *
 * Use #GladeWidgetAction to create custom routines to operate
 * on widgets you add to glade, when running #GladeActionActivateFunc functions
 * you should make sure to use #GladeCommand. 
 */

#include "glade-widget-action.h"
#include "config.h"
#include <glib/gi18n-lib.h>

enum
{
  PROP_0,

  PROP_CLASS,
  PROP_SENSITIVE,
  PROP_VISIBLE,
  N_PROPERTIES
};

struct _GladeWidgetActionPrivate
{
  GWActionClass *klass;     /* The action class */
  GList         *actions;   /* List of actions */
  guint          sensitive : 1; /* If this action is sensitive or not */
  guint          visible   : 1; /* If this action is visible or not */
};

static GParamSpec *properties[N_PROPERTIES];

G_DEFINE_TYPE_WITH_PRIVATE (GladeWidgetAction, glade_widget_action, G_TYPE_OBJECT);

static void
glade_widget_action_init (GladeWidgetAction *object)
{
  object->priv = glade_widget_action_get_instance_private (object);

  object->priv->sensitive = TRUE;
  object->priv->visible   = TRUE;
  object->priv->actions = NULL;
}

static void
glade_widget_action_finalize (GObject *object)
{
  GladeWidgetAction *action = GLADE_WIDGET_ACTION (object);

  if (action->priv->actions)
    {
      g_list_foreach (action->priv->actions, (GFunc) g_object_unref, NULL);
      g_list_free (action->priv->actions);
    }

  G_OBJECT_CLASS (glade_widget_action_parent_class)->finalize (object);
}

static GObject *
glade_widget_action_constructor (GType                  type,
                                 guint                  n_construct_properties,
                                 GObjectConstructParam *construct_properties)
{
  GladeWidgetAction *action;
  GObject *object;
  GList *l;

  object = G_OBJECT_CLASS (glade_widget_action_parent_class)->constructor
      (type, n_construct_properties, construct_properties);

  action = GLADE_WIDGET_ACTION (object);

  if (action->priv->klass == NULL)
    {
      g_warning ("GladeWidgetAction constructed without class property");
      return object;
    }

  for (l = action->priv->klass->actions; l; l = g_list_next (l))
    {
      GWActionClass *action_class = l->data;
      GObject *obj = g_object_new (GLADE_TYPE_WIDGET_ACTION,
                                   "class", action_class,
                                   NULL);

      action->priv->actions = g_list_prepend (action->priv->actions,
                                              GLADE_WIDGET_ACTION (obj));
    }

  action->priv->actions = g_list_reverse (action->priv->actions);

  return object;
}

static void
glade_widget_action_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GladeWidgetAction *action = GLADE_WIDGET_ACTION (object);

  g_return_if_fail (GLADE_IS_WIDGET_ACTION (object));

  switch (prop_id)
    {
      case PROP_CLASS:
        action->priv->klass = g_value_get_pointer (value);
        break;
      case PROP_SENSITIVE:
        glade_widget_action_set_sensitive (action, g_value_get_boolean (value));
        break;
      case PROP_VISIBLE:
        glade_widget_action_set_visible (action, g_value_get_boolean (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_widget_action_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GladeWidgetAction *action = GLADE_WIDGET_ACTION (object);

  g_return_if_fail (GLADE_IS_WIDGET_ACTION (object));

  switch (prop_id)
    {
      case PROP_CLASS:
        g_value_set_pointer (value, action->priv->klass);
        break;
      case PROP_SENSITIVE:
        g_value_set_boolean (value, action->priv->sensitive);
        break;
      case PROP_VISIBLE:
        g_value_set_boolean (value, action->priv->visible);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_widget_action_class_init (GladeWidgetActionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor = glade_widget_action_constructor;
  object_class->finalize = glade_widget_action_finalize;
  object_class->set_property = glade_widget_action_set_property;
  object_class->get_property = glade_widget_action_get_property;

  properties[PROP_CLASS] =
    g_param_spec_pointer ("class",
                          _("class"),
                          _("GladeWidgetActionClass structure pointer"),
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);

  properties[PROP_SENSITIVE] =
    g_param_spec_boolean ("sensitive",
                          _("Sensitive"),
                          _("Whether this action is sensitive"),
                          TRUE,
                          G_PARAM_READWRITE);

  properties[PROP_VISIBLE] =
    g_param_spec_boolean ("visible",
                          _("Visible"),
                          _("Whether this action is visible"),
                          TRUE,
                          G_PARAM_READWRITE);

  /* Install all properties */
  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

/**
 * glade_widget_action_set_sensitive:
 * @action: a #GladeWidgetAction
 * @sensitive:
 *
 * Set whether or not this action is sensitive.
 *
 */
void
glade_widget_action_set_sensitive (GladeWidgetAction *action,
                                   gboolean           sensitive)
{
  g_return_if_fail (GLADE_IS_WIDGET_ACTION (action));

  action->priv->sensitive = sensitive;
  g_object_notify_by_pspec (G_OBJECT (action), properties[PROP_SENSITIVE]);
}

gboolean
glade_widget_action_get_sensitive (GladeWidgetAction *action)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ACTION (action), FALSE);

  return action->priv->sensitive;
}

void
glade_widget_action_set_visible (GladeWidgetAction *action,
                                 gboolean           visible)
{
  g_return_if_fail (GLADE_IS_WIDGET_ACTION (action));

  action->priv->visible = visible;
  g_object_notify_by_pspec (G_OBJECT (action), properties[PROP_VISIBLE]);
}

gboolean
glade_widget_action_get_visible (GladeWidgetAction *action)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ACTION (action), FALSE);

  return action->priv->visible;
}

GList *
glade_widget_action_get_children (GladeWidgetAction *action)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ACTION (action), NULL);

  return action->priv->actions;
}

GWActionClass *
glade_widget_action_get_class (GladeWidgetAction *action)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ACTION (action), NULL);

  return action->priv->klass;
}

/***************************************************************
 *                         GWActionClass                       *
 ***************************************************************/
static const gchar *
gwa_action_path_get_id (const gchar *action_path)
{
  const gchar *id;

  if ((id = g_strrstr (action_path, "/")) && id[1] != '\0')
    return &id[1];
  else
    return action_path;
}

/**
 * glade_widget_action_class_new:
 * @path: the action path
 *
 * Returns: a newlly created #GWActionClass for @path.
 */
GWActionClass *
glade_widget_action_class_new (const gchar *path)
{
  GWActionClass *action;

  action = g_slice_new0 (GWActionClass);
  action->path = g_strdup (path);
  action->id   = gwa_action_path_get_id (action->path);

  return action;
}

/**
 * glade_widget_action_class_clone:
 * @action: a GWActionClass
 *
 * Returns: a newlly allocated copy of @action.
 */
GWActionClass *
glade_widget_action_class_clone (GWActionClass *action)
{
  GWActionClass *copy;
  GList *l;

  g_return_val_if_fail (action != NULL, NULL);

  copy = glade_widget_action_class_new (action->path);
  copy->label = g_strdup (action->label);
  copy->stock = g_strdup (action->stock);
  copy->important = action->important;

  for (l = action->actions; l; l = g_list_next (l))
    {
      GWActionClass *child = glade_widget_action_class_clone (l->data);
      copy->actions = g_list_prepend (copy->actions, child);
    }

  copy->actions = g_list_reverse (copy->actions);

  return copy;
}

/**
 * glade_widegt_action_class_free:
 * @action: a GWActionClass
 *
 * Frees a GWActionClass.
 */
void
glade_widget_action_class_free (GWActionClass *action)
{
  if (action->actions)
    g_list_foreach (action->actions, (GFunc) glade_widget_action_class_free,
                    NULL);

  /* Dont free id since it points into path directly */
  g_free (action->path);
  g_free (action->label);
  g_free (action->stock);

  g_slice_free (GWActionClass, action);
}

void
glade_widget_action_class_set_label (GWActionClass *action,
                                     const gchar   *label)
{
  g_free (action->label);
  action->label = g_strdup (label);
}

void
glade_widget_action_class_set_stock (GWActionClass *action,
                                     const gchar   *stock)
{
  g_free (action->stock);
  action->stock = g_strdup (stock);
}

void
glade_widget_action_class_set_important (GWActionClass *action,
                                         gboolean       important)
{
  action->important = important;
}
