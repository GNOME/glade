/*
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2006 The GNOME Foundation.
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
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 * SECTION:glade-property
 * @Title: GladeProperty
 * @Short_Description: An interface to properties on the #GladeWidget.
 *
 * Every object property of every #GladeWidget in every #GladeProject has
 * a #GladeProperty to interface with, #GladeProperty provides a means
 * to handle properties in the runtime environment.
 * 
 * A #GladeProperty can be seen as an instance of a #GladePropertyDef, 
 * the #GladePropertyDef describes how a #GladeProperty will function.
 */

#include <stdio.h>
#include <stdlib.h>             /* for atoi and atof */
#include <string.h>

#include <glib/gi18n-lib.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-property.h"
#include "glade-property-def.h"
#include "glade-project.h"
#include "glade-widget-adaptor.h"
#include "glade-debug.h"
#include "glade-app.h"
#include "glade-editor.h"
#include "glade-marshallers.h"

struct _GladePropertyPrivate {

  GladePropertyDef   *def;       /* A pointer to the GladeProperty that this
                                  * setting specifies
                                  */
  GladeWidget        *widget;    /* A pointer to the GladeWidget that this
                                  * GladeProperty is modifying
                                  */

  GladePropertyState  state;     /* Current property state, used by editing widgets.
                                  */
        
  GValue             *value;     /* The value of the property
                                  */

  gchar              *insensitive_tooltip; /* Tooltip to display when in insensitive state
                                            * (used to explain why the property is 
                                            *  insensitive)
                                            */

  gchar              *support_warning; /* Tooltip to display when the property
                                        * has format problems
                                        * (used to explain why the property is 
                                        *  insensitive)
                                        */
  guint               support_disabled : 1; /* Whether this property is disabled due
                                             * to format conflicts
                                             */

  guint               sensitive : 1; /* Whether this property is sensitive (if the
                                      * property is "optional" this takes precedence).
                                      */

  guint               enabled : 1;   /* Enabled is a flag that is used for GladeProperties
                                      * that have the optional flag set to let us know
                                      * if this widget has this setting enabled or
                                      * not. (Like default size, it can be specified or
                                      * unspecified). This flag also sets the state
                                      * of the property->input state for the loaded
                                      * widget.
                                      */

  guint               save_always : 1; /* Used to make a special case exception and always
                                        * save this property regardless of what the default
                                        * value is (used for some special cases like properties
                                        * that are assigned initial values in composite widgets
                                        * or derived widget code).
                                        */

  gint      precision;

  /* Used only for translatable strings. */
  guint     i18n_translatable : 1;
  gchar    *i18n_context;
  gchar    *i18n_comment;
  
  gint      syncing;  /* Avoid recursion while synchronizing object with value */
  gint      sync_tolerance;

  gchar         *bind_source;   /* A pointer to the GladeWidget that this
                                 * GladeProperty is bound to
                                 */

  gchar         *bind_property; /* The name of the property from the source
                                 * that is bound to this property
                                 */

  GBindingFlags  bind_flags;    /* The flags used in g_object_bind_property() to bind
                                 * this property
                                 */
};

G_DEFINE_TYPE_WITH_PRIVATE (GladeProperty, glade_property, G_TYPE_OBJECT)

enum
{
  VALUE_CHANGED,
  TOOLTIP_CHANGED,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_CLASS,
  PROP_ENABLED,
  PROP_SENSITIVE,
  PROP_I18N_TRANSLATABLE,
  PROP_I18N_CONTEXT,
  PROP_I18N_COMMENT,
  PROP_STATE,
  PROP_PRECISION,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];
static guint glade_property_signals[LAST_SIGNAL] = { 0 };

/*******************************************************************************
                           GladeProperty class methods
 *******************************************************************************/
static GladeProperty *
glade_property_dup_impl (GladeProperty *template_prop, GladeWidget *widget)
{
  GladeProperty *property;

  property = g_object_new (GLADE_TYPE_PROPERTY,
                           "class", template_prop->priv->def,
                           "i18n-translatable", template_prop->priv->i18n_translatable, 
                           "i18n-context", template_prop->priv->i18n_context, 
                           "i18n-comment", template_prop->priv->i18n_comment, 
                           NULL);
  property->priv->widget = widget;
  property->priv->value = g_new0 (GValue, 1);

  g_value_init (property->priv->value, template_prop->priv->value->g_type);

  /* Cannot duplicate parentless_widget property */
  if (glade_property_def_parentless_widget (template_prop->priv->def))
    {
      if (!G_IS_PARAM_SPEC_OBJECT (glade_property_def_get_pspec (template_prop->priv->def)))
        g_warning ("Parentless widget property should be of object type");

      g_value_set_object (property->priv->value, NULL);
    }
  else
    g_value_copy (template_prop->priv->value, property->priv->value);

  property->priv->enabled = template_prop->priv->enabled;
  property->priv->state   = template_prop->priv->state;

  glade_property_set_sensitive (property, template_prop->priv->sensitive,
                                template_prop->priv->insensitive_tooltip);

  return property;
}

static gboolean
glade_property_equals_value_impl (GladeProperty *property,
                                  const GValue  *value)
{
  return !glade_property_def_compare (property->priv->def, property->priv->value,
                                        value);
}


static void
glade_property_update_prop_refs (GladeProperty *property,
                                 const GValue  *old_value,
                                 const GValue  *new_value)
{
  GladeWidget *gold, *gnew;
  GObject *old_object, *new_object;
  GList *old_list, *new_list, *list, *removed, *added;

  if (GLADE_IS_PARAM_SPEC_OBJECTS (glade_property_def_get_pspec (property->priv->def)))
    {
      /* Make our own copies incase we're walking an
       * unstable list
       */
      old_list = g_value_dup_boxed (old_value);
      new_list = g_value_dup_boxed (new_value);

      /* Diff up the GList */
      removed = glade_util_removed_from_list (old_list, new_list);
      added = glade_util_added_in_list (old_list, new_list);

      /* Adjust the appropriate prop refs */
      for (list = removed; list; list = list->next)
        {
          old_object = list->data;
          gold = glade_widget_get_from_gobject (old_object);
          if (gold != NULL)
            glade_widget_remove_prop_ref (gold, property);
        }
      for (list = added; list; list = list->next)
        {
          new_object = list->data;
          gnew = glade_widget_get_from_gobject (new_object);
          if (gnew != NULL)
            glade_widget_add_prop_ref (gnew, property);
        }

      g_list_free (removed);
      g_list_free (added);
      g_list_free (old_list);
      g_list_free (new_list);
    }
  else
    {
      if ((old_object = g_value_get_object (old_value)) != NULL)
        {
          gold = glade_widget_get_from_gobject (old_object);
          g_return_if_fail (gold != NULL);
          glade_widget_remove_prop_ref (gold, property);
        }

      if ((new_object = g_value_get_object (new_value)) != NULL)
        {
          gnew = glade_widget_get_from_gobject (new_object);
          g_return_if_fail (gnew != NULL);
          glade_widget_add_prop_ref (gnew, property);
        }
    }
}

static gboolean
glade_property_verify (GladeProperty *property, const GValue *value)
{
  gboolean ret = FALSE;
  GladeWidget *parent;

  parent = glade_widget_get_parent (property->priv->widget);

  if (glade_property_def_get_is_packing (property->priv->def) && parent)
    ret =
      glade_widget_adaptor_child_verify_property (glade_widget_get_adaptor (parent),
                                                  glade_widget_get_object (parent),
                                                  glade_widget_get_object (property->priv->widget),
                                                  glade_property_def_id (property->priv->def), 
                                                  value);
  else if (!glade_property_def_get_is_packing (property->priv->def))
    ret = glade_widget_adaptor_verify_property (glade_widget_get_adaptor (property->priv->widget),
                                                glade_widget_get_object (property->priv->widget),
                                                glade_property_def_id (property->priv->def), value);

  return ret;
}

static void
glade_property_fix_state (GladeProperty *property)
{
  property->priv->state = GLADE_STATE_NORMAL;

  /* Properties are 'changed' state if they are not default, or if 
   * they are optional and enabled, optional enabled properties
   * are saved regardless of default value
   */
  if (glade_property_def_optional (property->priv->def))
    {
      if (glade_property_get_enabled (property))
        property->priv->state |= GLADE_STATE_CHANGED;
    }
  else if (!glade_property_original_default (property))
    property->priv->state |= GLADE_STATE_CHANGED;

  if (property->priv->support_warning)
    property->priv->state |= GLADE_STATE_UNSUPPORTED;

  if (property->priv->support_disabled)
    property->priv->state |= GLADE_STATE_SUPPORT_DISABLED;

  g_object_notify_by_pspec (G_OBJECT (property), properties[PROP_STATE]);
}

static void
glade_property_set_precision (GladeProperty *property, gint precision)
{
  property->priv->precision = precision;

  g_object_notify_by_pspec (G_OBJECT (property), properties[PROP_PRECISION]);
}

static gboolean
glade_property_set_value_impl (GladeProperty *property, const GValue *value)
{
  GladeProject *project = property->priv->widget ?
      glade_widget_get_project (property->priv->widget) : NULL;
  gboolean changed = FALSE;
  GValue old_value = { 0, };
  gboolean warn_before, warn_after;

#ifdef GLADE_ENABLE_DEBUG
  if (glade_get_debug_flags () & GLADE_DEBUG_PROPERTIES)
    {
      g_print ("PROPERTY: Setting %s property %s on %s ",
               glade_property_def_get_is_packing (property->priv->def) ? "packing" : "normal",
               glade_property_def_id (property->priv->def),
               property->priv->widget ? glade_widget_get_name (property->priv->widget) : "unknown");

      gchar *str1 =
        glade_widget_adaptor_string_from_value (glade_property_def_get_adaptor (property->priv->def),
                                                property->priv->def, property->priv->value);
      gchar *str2 =
        glade_widget_adaptor_string_from_value (glade_property_def_get_adaptor (property->priv->def),
                                                property->priv->def, value);
      g_print ("from %s to %s\n", str1, str2);
      g_free (str1);
      g_free (str2);
    }
#endif /* GLADE_ENABLE_DEBUG */

  if (!g_value_type_compatible (G_VALUE_TYPE (property->priv->value), G_VALUE_TYPE (value)))
    {
      g_warning ("Trying to assign an incompatible value to property %s\n",
                 glade_property_def_id (property->priv->def));
      return FALSE;
    }

  /* Check if the backend doesnt give us permission to
   * set this value.
   */
  if (glade_property_superuser () == FALSE && property->priv->widget &&
      project && glade_project_is_loading (project) == FALSE &&
      glade_property_verify (property, value) == FALSE)
    {
      return FALSE;
    }

  /* save "changed" state.
   */
  changed = !glade_property_equals_value (property, value);

  /* Add/Remove references from widget ref stacks here
   * (before assigning the value)
   */
  if (property->priv->widget && changed &&
      glade_property_def_is_object (property->priv->def))
    glade_property_update_prop_refs (property, property->priv->value, value);

  /* Check pre-changed warning state */
  warn_before = glade_property_warn_usage (property);

  /* Make a copy of the old value */
  g_value_init (&old_value, G_VALUE_TYPE (property->priv->value));
  g_value_copy (property->priv->value, &old_value);

  /* Assign property first so that; if the object need be
   * rebuilt, it will reflect the new value
   */
  g_value_reset (property->priv->value);
  g_value_copy (value, property->priv->value);

  GLADE_PROPERTY_GET_CLASS (property)->sync (property);

  glade_property_fix_state (property);

  if (changed && property->priv->widget)
    {
      g_signal_emit (G_OBJECT (property),
                     glade_property_signals[VALUE_CHANGED],
                     0, &old_value, property->priv->value);

      glade_project_verify_property (property);

      /* Check post change warning state */
      warn_after = glade_property_warn_usage (property);

      /* Update owning widget's warning state if need be */
      if (property->priv->widget != NULL && warn_before != warn_after)
        glade_widget_verify (property->priv->widget);
    }

  /* Special case parentless widget properties */
  if (glade_property_def_parentless_widget (property->priv->def))
    {
      GladeWidget *gobj;
      GObject *obj;
      
      if ((obj = g_value_get_object (&old_value)) &&
          (gobj = glade_widget_get_from_gobject (obj)))
        glade_widget_show (gobj);

      if ((obj = g_value_get_object (value)) &&
          (gobj = glade_widget_get_from_gobject (obj)))
        glade_widget_hide (gobj);  
    }

  g_value_unset (&old_value);
  return TRUE;
}

static void
glade_property_get_value_impl (GladeProperty *property, GValue *value)
{
  GParamSpec *pspec;

  pspec = glade_property_def_get_pspec (property->priv->def);

  g_value_init (value, pspec->value_type);
  g_value_copy (property->priv->value, value);
}

static void
glade_property_sync_impl (GladeProperty *property)
{
  GladePropertyPrivate *priv = property->priv;
  GladePropertyDef *def = priv->def;
  const GValue *value;
  const gchar *id;

  /* Heh, here are the many reasons not to
   * sync a property ;-)
   */
  if (/* the class can be NULL during object,
       * construction this is just a temporary state */
       def == NULL ||
       /* explicit "never sync" flag */
       glade_property_def_get_ignore (def) ||
       /* recursion guards */
       priv->syncing >= priv->sync_tolerance ||
       /* No widget owns this property yet */
       priv->widget == NULL)
    return;

  id = glade_property_def_id (def);

  /* Only the properties from widget->properties should affect the runtime widget.
   * (other properties may be used for convenience in the plugin).
   */
  if ((glade_property_def_get_is_packing (def) &&
       !glade_widget_get_pack_property (priv->widget, id))
      || !glade_widget_get_property (priv->widget, id))
    return;

  priv->syncing++;

  /* optional properties that are disabled get the default runtime value */
  value = (priv->enabled) ? priv->value : glade_property_def_get_default (def);

  /* In the case of construct_only, the widget instance must be rebuilt
   * to apply the property
   */
  if (glade_property_def_get_construct_only (def) && priv->syncing == 1)
    {
      /* Virtual properties can be construct only, in which
       * case they are allowed to trigger a rebuild, and in
       * the process are allowed to get "synced" after the
       * instance is rebuilt.
       */
      if (glade_property_def_get_virtual (def))
        priv->sync_tolerance++;

      glade_widget_rebuild (priv->widget);

      if (glade_property_def_get_virtual (def))
        priv->sync_tolerance--;
    }
  else if (glade_property_def_get_is_packing (def))
    glade_widget_child_set_property (glade_widget_get_parent (priv->widget),
                                     priv->widget, id, value);
  else
    glade_widget_object_set_property (priv->widget, id, value);

  priv->syncing--;
}

static void
glade_property_load_impl (GladeProperty *property)
{
  GObject *object;
  GObjectClass *oclass;
  GParamSpec *pspec;

  pspec = glade_property_def_get_pspec (property->priv->def);

  if (property->priv->widget == NULL ||
      glade_property_def_get_virtual (property->priv->def) ||
      glade_property_def_get_is_packing (property->priv->def) ||
      glade_property_def_get_ignore (property->priv->def) ||
      !(pspec->flags & G_PARAM_READABLE) || G_IS_PARAM_SPEC_OBJECT (pspec))
    return;

  object = glade_widget_get_object (property->priv->widget);
  oclass = G_OBJECT_GET_CLASS (object);

  if (g_object_class_find_property (oclass, glade_property_def_id (property->priv->def)))
    glade_widget_object_get_property (property->priv->widget, 
                                      glade_property_def_id (property->priv->def),
                                      property->priv->value);
}

/*******************************************************************************
                      GObjectClass & Object Construction
 *******************************************************************************/
static void
glade_property_set_real_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GladeProperty *property = GLADE_PROPERTY (object);

  switch (prop_id)
    {
      case PROP_CLASS:
        property->priv->def = g_value_get_pointer (value);
        break;
      case PROP_ENABLED:
        glade_property_set_enabled (property, g_value_get_boolean (value));
        break;
      case PROP_SENSITIVE:
        property->priv->sensitive = g_value_get_boolean (value);
        break;
      case PROP_I18N_TRANSLATABLE:
        glade_property_i18n_set_translatable (property,
                                              g_value_get_boolean (value));
        break;
      case PROP_I18N_CONTEXT:
        glade_property_i18n_set_context (property, g_value_get_string (value));
        break;
      case PROP_I18N_COMMENT:
        glade_property_i18n_set_comment (property, g_value_get_string (value));
        break;
      case PROP_PRECISION:
        glade_property_set_precision (property, g_value_get_int (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_property_get_real_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GladeProperty *property = GLADE_PROPERTY (object);

  switch (prop_id)
    {
      case PROP_CLASS:
        g_value_set_pointer (value, property->priv->def);
        break;
      case PROP_ENABLED:
        g_value_set_boolean (value, glade_property_get_enabled (property));
        break;
      case PROP_SENSITIVE:
        g_value_set_boolean (value, glade_property_get_sensitive (property));
        break;
      case PROP_I18N_TRANSLATABLE:
        g_value_set_boolean (value,
                             glade_property_i18n_get_translatable (property));
        break;
      case PROP_I18N_CONTEXT:
        g_value_set_string (value, glade_property_i18n_get_context (property));
        break;
      case PROP_I18N_COMMENT:
        g_value_set_string (value, glade_property_i18n_get_comment (property));
        break;
      case PROP_STATE:
        g_value_set_int (value, property->priv->state);
        break;
      case PROP_PRECISION:
        g_value_set_int (value, property->priv->precision);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_property_finalize (GObject *object)
{
  GladeProperty *property = GLADE_PROPERTY (object);

  if (property->priv->value)
    {
      g_value_unset (property->priv->value);
      g_free (property->priv->value);
    }
  if (property->priv->i18n_comment)
    g_free (property->priv->i18n_comment);
  if (property->priv->i18n_context)
    g_free (property->priv->i18n_context);
  if (property->priv->support_warning)
    g_free (property->priv->support_warning);
  if (property->priv->insensitive_tooltip)
    g_free (property->priv->insensitive_tooltip);

  G_OBJECT_CLASS (glade_property_parent_class)->finalize (object);
}

static void
glade_property_init (GladeProperty *property)
{
  property->priv = G_TYPE_INSTANCE_GET_PRIVATE (property,
                                                GLADE_TYPE_PROPERTY,
                                                GladePropertyPrivate);

  property->priv->precision = 2;
  property->priv->enabled = TRUE;
  property->priv->sensitive = TRUE;
  property->priv->i18n_translatable = TRUE;
  property->priv->i18n_comment = NULL;
  property->priv->sync_tolerance = 1;
}

static void
glade_property_class_init (GladePropertyClass * prop_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (prop_class);

  /* GObjectClass */
  object_class->set_property = glade_property_set_real_property;
  object_class->get_property = glade_property_get_real_property;
  object_class->finalize = glade_property_finalize;

  /* Class methods */
  prop_class->dup = glade_property_dup_impl;
  prop_class->equals_value = glade_property_equals_value_impl;
  prop_class->set_value = glade_property_set_value_impl;
  prop_class->get_value = glade_property_get_value_impl;
  prop_class->sync = glade_property_sync_impl;
  prop_class->load = glade_property_load_impl;
  prop_class->value_changed = NULL;
  prop_class->tooltip_changed = NULL;

  /* Properties */
  properties[PROP_CLASS] =
    g_param_spec_pointer ("class",
                          _("Class"),
                          _("The GladePropertyDef for this property"),
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

  properties[PROP_ENABLED] =
    g_param_spec_boolean ("enabled",
                          _("Enabled"),
                          _("If the property is optional, this is its enabled state"),
                          TRUE, G_PARAM_READWRITE);

  properties[PROP_SENSITIVE] =
    g_param_spec_boolean ("sensitive",
                          _("Sensitive"),
                          _("This gives backends control to set property sensitivity"),
                          TRUE, G_PARAM_READWRITE);

  properties[PROP_I18N_CONTEXT] =
    g_param_spec_string ("i18n-context",
                         _("Context"),
                         _("Context for translation"),
                         NULL,
                         G_PARAM_READWRITE);

  properties[PROP_I18N_COMMENT] =
    g_param_spec_string ("i18n-comment",
                         _("Comment"),
                         _("Comment for translators"),
                         NULL,
                         G_PARAM_READWRITE);

  properties[PROP_I18N_TRANSLATABLE] =
    g_param_spec_boolean ("i18n-translatable",
                          _("Translatable"),
                          _("Whether this property is translatable"),
                          TRUE,
                          G_PARAM_READWRITE);

  properties[PROP_STATE] =
    g_param_spec_int ("state",
                      _("Visual State"),
                      _("Priority information for the property editor to act on"),
                      GLADE_STATE_NORMAL,
                      G_MAXINT,
                      GLADE_STATE_NORMAL,
                      G_PARAM_READABLE);
  
  properties[PROP_PRECISION] =
    g_param_spec_int ("precision",
                      _("Precision"),
                      _("Where applicable, precision to use on editors"),
                      0,
                      G_MAXINT,
                      2,
                      G_PARAM_READWRITE);

  /* Install all properties */
  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /* Signal */
  glade_property_signals[VALUE_CHANGED] =
      g_signal_new ("value-changed",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladePropertyClass,
                                     value_changed),
                    NULL, NULL,
                    _glade_marshal_VOID__POINTER_POINTER,
                    G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

  glade_property_signals[TOOLTIP_CHANGED] =
      g_signal_new ("tooltip-changed",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladePropertyClass,
                                     tooltip_changed),
                    NULL, NULL,
                    _glade_marshal_VOID__STRING_STRING_STRING,
                    G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_STRING,
                    G_TYPE_STRING);
}

/*******************************************************************************
                                     API
 *******************************************************************************/
/**
 * glade_property_new:
 * @def: A #GladePropertyDef defining this property
 * @widget: The #GladeWidget this property is created for
 * @value: The initial #GValue of the property or %NULL
 *         (the #GladeProperty will assume ownership of @value)
 *
 * Creates a #GladeProperty of type @klass for @widget with @value; if
 * @value is %NULL, then the introspected default value for that property
 * will be used.
 *
 * Returns: The newly created #GladeProperty
 */
GladeProperty *
glade_property_new (GladePropertyDef *def,
                    GladeWidget      *widget,
                    GValue           *value)
{
  GladeProperty *property;

  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (def), NULL);

  property = (GladeProperty *) g_object_new (GLADE_TYPE_PROPERTY, NULL);
  property->priv->def = def;
  property->priv->widget = widget;
  property->priv->value = value;

  if (glade_property_def_optional (def))
    property->priv->enabled = glade_property_def_optional_default (def);

  if (property->priv->value == NULL)
    {
      const GValue *orig_def =
        glade_property_def_get_original_default (def);

      property->priv->value = g_new0 (GValue, 1);
      g_value_init (property->priv->value, orig_def->g_type);
      g_value_copy (orig_def, property->priv->value);
    }

  return property;
}

/**
 * glade_property_dup:
 * @template_prop: A #GladeProperty
 * @widget: A #GladeWidget
 *
 * Returns: (transfer full): A newly duplicated property based on the new widget
 */
GladeProperty *
glade_property_dup (GladeProperty *template_prop, GladeWidget *widget)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY (template_prop), NULL);
  return GLADE_PROPERTY_GET_CLASS (template_prop)->dup (template_prop, widget);
}

static void
glade_property_reset_common (GladeProperty *property, gboolean original)
{
  const GValue *value;

  g_return_if_fail (GLADE_IS_PROPERTY (property));
  
  if (original)
    value = glade_property_def_get_original_default (property->priv->def);
  else
    value = glade_property_def_get_default (property->priv->def);

  GLADE_PROPERTY_GET_CLASS (property)->set_value (property, value);
}

/**
 * glade_property_reset:
 * @property: A #GladeProperty
 *
 * Resets this property to its default value
 */
void
glade_property_reset (GladeProperty *property)
{
  glade_property_reset_common (property, FALSE);
}

/**
 * glade_property_original_reset:
 * @property: A #GladeProperty
 *
 * Resets this property to its original default value
 */
void
glade_property_original_reset (GladeProperty *property)
{
  glade_property_reset_common (property, TRUE);
}

static gboolean
glade_property_default_common (GladeProperty *property, gboolean orig)
{
  const GValue *value;

  g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);

  if (orig)
    value = glade_property_def_get_original_default (property->priv->def);
  else
    value = glade_property_def_get_default (property->priv->def);

  return GLADE_PROPERTY_GET_CLASS (property)->equals_value (property, value);
}

/**
 * glade_property_default:
 * @property: A #GladeProperty
 *
 * Returns: Whether this property is at its default value
 */
gboolean
glade_property_default (GladeProperty *property)
{
  return glade_property_default_common (property, FALSE);
}

/**
 * glade_property_original_default:
 * @property: A #GladeProperty
 *
 * Returns: Whether this property is at its original default value
 */
gboolean
glade_property_original_default (GladeProperty *property)
{
  return glade_property_default_common (property, TRUE);
}

/**
 * glade_property_equals_value:
 * @property: a #GladeProperty
 * @value: a #GValue
 *
 * Returns: Whether this property is equal to the value provided
 */
gboolean
glade_property_equals_value (GladeProperty *property, const GValue *value)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);
  return GLADE_PROPERTY_GET_CLASS (property)->equals_value (property, value);
}

/**
 * glade_property_equals_va_list:
 * @property: a #GladeProperty
 * @vl: a va_list
 *
 * Returns: Whether this property is equal to the value provided
 */
static gboolean
glade_property_equals_va_list (GladeProperty *property, va_list vl)
{
  GValue *value;
  gboolean ret;

  g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);

  value = glade_property_def_make_gvalue_from_vl (property->priv->def, vl);

  ret = GLADE_PROPERTY_GET_CLASS (property)->equals_value (property, value);

  g_value_unset (value);
  g_free (value);
  return ret;
}

/**
 * glade_property_equals:
 * @property: a #GladeProperty
 * @...: a provided property value
 *
 * Returns: Whether this property is equal to the value provided
 */
gboolean
glade_property_equals (GladeProperty *property, ...)
{
  va_list vl;
  gboolean ret;

  g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);

  va_start (vl, property);
  ret = glade_property_equals_va_list (property, vl);
  va_end (vl);

  return ret;
}

/**
 * glade_property_set_value:
 * @property: a #GladeProperty
 * @value: a #GValue
 *
 * Sets the property's value
 *
 * Returns: Whether the property was successfully set.
 */
gboolean
glade_property_set_value (GladeProperty *property, const GValue *value)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);
  g_return_val_if_fail (value != NULL, FALSE);
  return GLADE_PROPERTY_GET_CLASS (property)->set_value (property, value);
}

/**
 * glade_property_set_va_list:
 * @property: a #GladeProperty
 * @vl: a va_list with value to set
 *
 * Sets the property's value
 */
gboolean
glade_property_set_va_list (GladeProperty *property, va_list vl)
{
  GValue *value;
  gboolean success;

  g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);

  value = glade_property_def_make_gvalue_from_vl (property->priv->def, vl);

  success = GLADE_PROPERTY_GET_CLASS (property)->set_value (property, value);

  g_value_unset (value);
  g_free (value);

  return success;
}

/**
 * glade_property_set:
 * @property: a #GladeProperty
 * @...: the value to set
 *
 * Sets the property's value (in a convenient way)
 */
gboolean
glade_property_set (GladeProperty *property, ...)
{
  va_list vl;
  gboolean success;

  g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);

  va_start (vl, property);
  success = glade_property_set_va_list (property, vl);
  va_end (vl);

  return success;
}

/**
 * glade_property_get_value:
 * @property: a #GladeProperty
 * @value: a #GValue
 *
 * Retrieve the property value
 */
void
glade_property_get_value (GladeProperty *property, GValue *value)
{
  g_return_if_fail (GLADE_IS_PROPERTY (property));
  g_return_if_fail (value != NULL);
  GLADE_PROPERTY_GET_CLASS (property)->get_value (property, value);
}

/**
 * glade_property_get_default:
 * @property: a #GladeProperty
 * @value: a #GValue
 *
 * Retrieve the default property value
 */
void
glade_property_get_default (GladeProperty *property, GValue *value)
{
  GParamSpec *pspec;

  g_return_if_fail (GLADE_IS_PROPERTY (property));
  g_return_if_fail (value != NULL);

  pspec = glade_property_def_get_pspec (property->priv->def);
  g_value_init (value, pspec->value_type);
  g_value_copy (glade_property_def_get_default (property->priv->def), value);
}

/**
 * glade_property_get_va_list:
 * @property: a #GladeProperty
 * @vl: a va_list
 *
 * Retrieve the property value
 */
void
glade_property_get_va_list (GladeProperty *property, va_list vl)
{
  g_return_if_fail (GLADE_IS_PROPERTY (property));
  glade_property_def_set_vl_from_gvalue (property->priv->def, property->priv->value,
                                           vl);
}

/**
 * glade_property_get:
 * @property: a #GladeProperty
 * @...: An address to store the value
 *
 * Retrieve the property value
 */
void
glade_property_get (GladeProperty *property, ...)
{
  va_list vl;

  g_return_if_fail (GLADE_IS_PROPERTY (property));

  va_start (vl, property);
  glade_property_get_va_list (property, vl);
  va_end (vl);
}

/**
 * glade_property_sync:
 * @property: a #GladeProperty
 *
 * Synchronize the object with this property
 */
void
glade_property_sync (GladeProperty *property)
{
  g_return_if_fail (GLADE_IS_PROPERTY (property));
  GLADE_PROPERTY_GET_CLASS (property)->sync (property);
}

/**
 * glade_property_load:
 * @property: a #GladeProperty
 *
 * Loads the value of @property from the coresponding object instance
 */
void
glade_property_load (GladeProperty *property)
{
  g_return_if_fail (GLADE_IS_PROPERTY (property));
  GLADE_PROPERTY_GET_CLASS (property)->load (property);
}

/**
 * glade_property_read:
 * @property: a #GladeProperty or #NULL
 * @project: the #GladeProject
 * @node: the #GladeXmlNode to read, will either be a 'widget'
 *        node or a 'child' node for packing properties.
 *
 * Read the value and any attributes for @property from @node, assumes
 * @property is being loaded for @project
 *
 * Note that object values will only be resolved after the project is
 * completely loaded
 */
void
glade_property_read (GladeProperty *property,
                     GladeProject  *project,
                     GladeXmlNode  *prop)
{
  GValue *gvalue = NULL;
  gchar /* *id, *name, */  * value;
  gint translatable = FALSE;
  gchar *comment = NULL, *context = NULL;
  gchar *bind_flags = NULL;

  g_return_if_fail (GLADE_IS_PROPERTY (property));
  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (prop != NULL);

  if (!glade_xml_node_verify (prop, GLADE_XML_TAG_PROPERTY))
    return;

  if (!(value = glade_xml_get_content (prop)))
    return;

  /* If an optional property is specified in the
   * glade file, its enabled
   */
  property->priv->enabled = TRUE;
  
  if (glade_property_def_is_object (property->priv->def))
    {
      /* we must synchronize this directly after loading this project
       * (i.e. lookup the actual objects after they've been parsed and
       * are present).
       */
      g_object_set_data_full (G_OBJECT (property),
                              "glade-loaded-object", g_strdup (value), g_free);
    }
  else
    {
      gvalue = 
        glade_property_def_make_gvalue_from_string (property->priv->def, value, project);

      GLADE_PROPERTY_GET_CLASS (property)->set_value (property, gvalue);

      g_value_unset (gvalue);
      g_free (gvalue);
    }

  translatable =
      glade_xml_get_property_boolean (prop, GLADE_TAG_TRANSLATABLE, FALSE);
  comment = glade_xml_get_property_string (prop, GLADE_TAG_COMMENT);
  context = glade_xml_get_property_string (prop, GLADE_TAG_CONTEXT);

  property->priv->bind_source = glade_xml_get_property_string (prop, GLADE_TAG_BIND_SOURCE);
  property->priv->bind_property = glade_xml_get_property_string (prop, GLADE_TAG_BIND_PROPERTY);
  bind_flags = glade_xml_get_property_string (prop, GLADE_TAG_BIND_FLAGS);
  if (bind_flags)
    property->priv->bind_flags = glade_property_def_make_flags_from_string (G_TYPE_BINDING_FLAGS, bind_flags);

  glade_property_i18n_set_translatable (property, translatable);
  glade_property_i18n_set_comment (property, comment);
  glade_property_i18n_set_context (property, context);

  g_free (comment);
  g_free (context);
  g_free (value);
  g_free (bind_flags);
}


/**
 * glade_property_write:
 * @property: a #GladeProperty
 * @context: A #GladeXmlContext
 * @node: A #GladeXmlNode
 *
 * Write @property to @node
 */
void
glade_property_write (GladeProperty   *property,
                      GladeXmlContext *context,
                      GladeXmlNode    *node)
{
  GladeXmlNode *prop_node;
  gchar *name, *value;
  gboolean save_always;
  gchar *binding_flags = NULL;
  GFlagsClass *flags_class;
  GFlagsValue flags_value;
  guint i;

  g_return_if_fail (GLADE_IS_PROPERTY (property));
  g_return_if_fail (node != NULL);

  /* This code should work the same for <packing>, <widget> and <template> */
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_PACKING) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* There can be a couple of reasons to forcefully save a property */
  save_always = (glade_property_def_save_always (property->priv->def) || property->priv->save_always);
  save_always = save_always || (glade_property_def_optional (property->priv->def) && property->priv->enabled);

  /* Skip properties that are default by original pspec default and that have no bound property
   * (excepting those that specified otherwise).
   */
  if (!save_always && glade_property_original_default (property) && !property->priv->bind_source)
    return;

  /* Escape our string and save with underscores */
  name = g_strdup (glade_property_def_id (property->priv->def));
  glade_util_replace (name, '-', '_');

  /* convert the value of this property to a string */
  if (!(value = glade_widget_adaptor_string_from_value
        (glade_property_def_get_adaptor (property->priv->def), property->priv->def,
         property->priv->value)))
    /* make sure we keep the empty string, also... upcomming
     * funcs that may not like NULL.
     */
    value = g_strdup ("");

  /* Now dump the node values... */
  prop_node = glade_xml_node_new (context, GLADE_XML_TAG_PROPERTY);
  glade_xml_node_append_child (node, prop_node);

  /* Name and value */
  glade_xml_node_set_property_string (prop_node, GLADE_XML_TAG_NAME, name);
  glade_xml_set_content (prop_node, value);

  /* i18n stuff */
  if (glade_property_def_translatable (property->priv->def))
    {
      if (property->priv->i18n_translatable)
        glade_xml_node_set_property_string (prop_node,
                                            GLADE_TAG_TRANSLATABLE,
                                            GLADE_XML_TAG_I18N_TRUE);

      if (property->priv->i18n_context)
        glade_xml_node_set_property_string (prop_node,
                                            GLADE_TAG_CONTEXT,
                                            property->priv->i18n_context);

      if (property->priv->i18n_comment)
        glade_xml_node_set_property_string (prop_node,
                                            GLADE_TAG_COMMENT,
                                            property->priv->i18n_comment);
    }

  if (property->priv->bind_source)
    {
      glade_xml_node_set_property_string (prop_node,
                                          GLADE_TAG_BIND_SOURCE,
                                          property->priv->bind_source);
      if (property->priv->bind_property)
        glade_xml_node_set_property_string (prop_node,
                                            GLADE_TAG_BIND_PROPERTY,
                                            property->priv->bind_property);
      if (property->priv->bind_flags != G_BINDING_DEFAULT)
        {
          flags_class = G_FLAGS_CLASS (g_type_class_ref (G_TYPE_BINDING_FLAGS));
          for (i = 0; i < flags_class->n_values; i++)
            {
              flags_value = flags_class->values[i];
              if (flags_value.value == 0)
                continue;

              if ((flags_value.value & property->priv->bind_flags) != 0)
                {
                  if (binding_flags)
                    {
                      gchar *old_flags = g_steal_pointer (&binding_flags);
                      binding_flags = g_strdup_printf ("%s|%s", old_flags, flags_value.value_nick);
                      g_free (old_flags);
                    }
                  else
                    {
                      binding_flags = g_strdup (flags_value.value_nick);
                    }
                }
            }

          g_type_class_unref (flags_class);
          glade_xml_node_set_property_string (prop_node,
                                              GLADE_TAG_BIND_FLAGS,
                                              binding_flags);
          g_free (binding_flags);
        }
    }
  g_free (name);
  g_free (value);
}

/**
 * glade_property_add_object:
 * @property: a #GladeProperty
 * @object: The #GObject to add
 *
 * Adds @object to the object list in @property.
 *
 * Note: This function expects @property to be a #GladeParamSpecObjects
 * or #GParamSpecObject type property.
 */
void
glade_property_add_object (GladeProperty *property, GObject *object)
{
  GList *list = NULL, *new_list = NULL;
  GParamSpec *pspec;

  g_return_if_fail (GLADE_IS_PROPERTY (property));
  g_return_if_fail (G_IS_OBJECT (object));

  pspec = glade_property_def_get_pspec (property->priv->def);

  g_return_if_fail (GLADE_IS_PARAM_SPEC_OBJECTS (pspec) ||
                    G_IS_PARAM_SPEC_OBJECT (pspec));

  if (GLADE_IS_PARAM_SPEC_OBJECTS (pspec))
    {
      glade_property_get (property, &list);
      new_list = g_list_copy (list);

      new_list = g_list_append (new_list, object);
      glade_property_set (property, new_list);

      /* ownership of the list is not passed 
       * through glade_property_set() 
       */
      g_list_free (new_list);
    }
  else
    {
      glade_property_set (property, object);
    }
}

/**
 * glade_property_remove_object:
 * @property: a #GladeProperty
 * @object: The #GObject to add
 *
 * Removes @object from the object list in @property.
 *
 * Note: This function expects @property to be a #GladeParamSpecObjects
 * or #GParamSpecObject type property.
 */
void
glade_property_remove_object (GladeProperty *property, GObject *object)
{
  GList *list = NULL, *new_list = NULL;
  GParamSpec *pspec;

  g_return_if_fail (GLADE_IS_PROPERTY (property));
  g_return_if_fail (G_IS_OBJECT (object));

  pspec = glade_property_def_get_pspec (property->priv->def);

  g_return_if_fail (GLADE_IS_PARAM_SPEC_OBJECTS (pspec) ||
                    G_IS_PARAM_SPEC_OBJECT (pspec));

  if (GLADE_IS_PARAM_SPEC_OBJECTS (pspec))
    {
      /* If object isnt in list; list should stay in tact.
       * not bothering to check for now.
       */
      glade_property_get (property, &list);
      new_list = g_list_copy (list);

      new_list = g_list_remove (new_list, object);
      glade_property_set (property, new_list);

      /* ownership of the list is not passed 
       * through glade_property_set() 
       */
      g_list_free (new_list);
    }
  else
    {
      glade_property_set (property, NULL);
    }
}

/**
 * glade_property_get_def:
 * @property: a #GladeProperty
 *
 * Get the #GladePropertyDef this property was created for.
 *
 * Returns: (transfer none): a #GladePropertyDef
 */
GladePropertyDef *
glade_property_get_def (GladeProperty *property)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY (property), NULL);

  return property->priv->def;
}


/* Parameters for translatable properties. */
void
glade_property_i18n_set_comment (GladeProperty *property, const gchar *str)
{
  g_return_if_fail (GLADE_IS_PROPERTY (property));
  if (property->priv->i18n_comment)
    g_free (property->priv->i18n_comment);

  property->priv->i18n_comment = g_strdup (str);
  g_object_notify_by_pspec (G_OBJECT (property), properties[PROP_I18N_COMMENT]);
}

const gchar *
glade_property_i18n_get_comment (GladeProperty * property)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY (property), NULL);
  return property->priv->i18n_comment;
}

void
glade_property_i18n_set_context (GladeProperty *property, const gchar *str)
{
  g_return_if_fail (GLADE_IS_PROPERTY (property));
  if (property->priv->i18n_context)
    g_free (property->priv->i18n_context);

  property->priv->i18n_context = g_strdup (str);
  g_object_notify_by_pspec (G_OBJECT (property), properties[PROP_I18N_CONTEXT]);
}

const gchar *
glade_property_i18n_get_context (GladeProperty *property)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY (property), NULL);
  return property->priv->i18n_context;
}

void
glade_property_i18n_set_translatable (GladeProperty *property,
                                      gboolean      translatable)
{
  g_return_if_fail (GLADE_IS_PROPERTY (property));
  property->priv->i18n_translatable = translatable;
  g_object_notify_by_pspec (G_OBJECT (property), properties[PROP_I18N_TRANSLATABLE]);
}

gboolean
glade_property_i18n_get_translatable (GladeProperty *property)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);
  return property->priv->i18n_translatable;
}

void
glade_property_set_sensitive (GladeProperty *property,
                              gboolean       sensitive,
                              const gchar   *reason)
{
  g_return_if_fail (GLADE_IS_PROPERTY (property));

  /* reason is only why we're disableing it */
  if (sensitive == FALSE)
    {
      if (property->priv->insensitive_tooltip)
        g_free (property->priv->insensitive_tooltip);
      property->priv->insensitive_tooltip = g_strdup (reason);
    }

  if (property->priv->sensitive != sensitive)
    {
      property->priv->sensitive = sensitive;

      /* Clear it */
      if (sensitive)
        property->priv->insensitive_tooltip =
            (g_free (property->priv->insensitive_tooltip), NULL);

      g_signal_emit (G_OBJECT (property),
                     glade_property_signals[TOOLTIP_CHANGED],
                     0,
                     glade_property_def_get_tooltip (property->priv->def),
                     property->priv->insensitive_tooltip, 
                     property->priv->support_warning);
    }
  g_object_notify_by_pspec (G_OBJECT (property), properties[PROP_SENSITIVE]);
}

const gchar *
glade_propert_get_insensitive_tooltip (GladeProperty *property)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY (property), NULL);

  return property->priv->insensitive_tooltip;
}

gboolean
glade_property_get_sensitive (GladeProperty *property)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);
  return property->priv->sensitive;
}

void
glade_property_set_support_warning (GladeProperty *property,
                                    gboolean       disable,
                                    const gchar   *reason)
{
  gboolean warn_before, warn_after;

  g_return_if_fail (GLADE_IS_PROPERTY (property));

  /* Check pre-changed warning state */
  warn_before = glade_property_warn_usage (property);

  if (property->priv->support_warning)
    g_free (property->priv->support_warning);
  property->priv->support_warning = g_strdup (reason);

  property->priv->support_disabled = disable;

  g_signal_emit (G_OBJECT (property),
                 glade_property_signals[TOOLTIP_CHANGED],
                 0,
                 glade_property_def_get_tooltip (property->priv->def),
                 property->priv->insensitive_tooltip, 
                 property->priv->support_warning);

  glade_property_fix_state (property);

  /* Check post-changed warning state */
  warn_after = glade_property_warn_usage (property);

  /* Update owning widget's warning state if need be */
  if (property->priv->widget != NULL && warn_before != warn_after)
    glade_widget_verify (property->priv->widget);
}

const gchar *
glade_property_get_support_warning (GladeProperty *property)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY (property), NULL);

  return property->priv->support_warning;
}

gboolean
glade_property_warn_usage (GladeProperty *property)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);

  if (!property->priv->support_warning)
    return FALSE;

  return ((property->priv->state & GLADE_STATE_CHANGED) != 0);
}

/**
 * glade_property_set_save_always:
 * @property: A #GladeProperty
 * @setting: the value to set
 *
 * Sets whether this property should be special cased
 * to always be saved regardless of its default value.
 * (used for some special cases like properties
 * that are assigned initial values in composite widgets
 * or derived widget code).
 */
void
glade_property_set_save_always (GladeProperty *property, gboolean setting)
{
  g_return_if_fail (GLADE_IS_PROPERTY (property));

  property->priv->save_always = setting;
}

/**
 * glade_property_get_save_always:
 * @property: A #GladeProperty
 *
 * Returns: whether this property is special cased
 * to always be saved regardless of its default value.
 */
gboolean
glade_property_get_save_always (GladeProperty *property)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);

  return property->priv->save_always;
}

void
glade_property_set_enabled (GladeProperty *property, gboolean enabled)
{
  gboolean warn_before, warn_after;

  g_return_if_fail (GLADE_IS_PROPERTY (property));

  /* Check pre-changed warning state */
  warn_before = glade_property_warn_usage (property);

  property->priv->enabled = enabled;
  glade_property_sync (property);

  glade_property_fix_state (property);

  /* Check post-changed warning state */
  warn_after = glade_property_warn_usage (property);

  /* Update owning widget's warning state if need be */
  if (property->priv->widget != NULL && warn_before != warn_after)
    glade_widget_verify (property->priv->widget);

  g_object_notify_by_pspec (G_OBJECT (property), properties[PROP_ENABLED]);
}

gboolean
glade_property_get_enabled (GladeProperty * property)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY (property), FALSE);
  return property->priv->enabled;
}

gchar *
glade_property_make_string (GladeProperty *property)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY (property), NULL);

  return glade_property_def_make_string_from_gvalue (property->priv->def, 
                                                       property->priv->value);
}

/**
 * glade_property_set_widget:
 * @property: A #GladeProperty
 * @widget: (transfer full): a #GladeWidget
 */
void
glade_property_set_widget (GladeProperty *property,
                           GladeWidget   *widget)
{
  g_return_if_fail (GLADE_IS_PROPERTY (property));

  property->priv->widget = widget;
}

/**
 * glade_property_get_widget:
 * @property: A #GladeProperty
 *
 * Returns: (transfer none): a #GladeWidget
 */
GladeWidget *
glade_property_get_widget (GladeProperty *property)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY (property), NULL);

  return property->priv->widget;
}

GValue *
glade_property_inline_value (GladeProperty *property)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY (property), NULL);

  return property->priv->value;
}

GladePropertyState
glade_property_get_state (GladeProperty *property)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY (property), 0);

  return property->priv->state;
}


static gint glade_property_su_stack = 0;

void
glade_property_push_superuser (void)
{
  glade_property_su_stack++;
}

void
glade_property_pop_superuser (void)
{
  if (--glade_property_su_stack < 0)
    {
      g_critical ("Bug: property super user stack is corrupt.\n");
    }
}

gboolean
glade_property_superuser (void)
{
  return glade_property_su_stack > 0;
}
