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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-popup.h"
#include "glade-editable.h"
#include "glade-property-shell.h"
#include "glade-marshallers.h"

/* GObjectClass */
static void      glade_property_shell_finalize          (GObject       *object);
static void      glade_property_shell_set_real_property (GObject       *object,
                                                         guint          prop_id,
                                                         const GValue  *value,
                                                         GParamSpec    *pspec);
static void      glade_property_shell_get_real_property (GObject       *object,
                                                         guint          prop_id,
                                                         GValue        *value,
                                                         GParamSpec    *pspec);

/* GladeEditableInterface */
static void      glade_property_shell_editable_init     (GladeEditableInterface *iface);

struct _GladePropertyShellPrivate
{
  /* Current State */
  GladeWidgetAdaptor  *adaptor;
  GladeEditorProperty *property_editor;
  gulong               pre_commit_id;
  gulong               post_commit_id;

  /* Properties, used to load the internal editor */
  GType                editor_type;
  gchar               *property_name;
  gchar               *custom_text;
  guint                packing : 1;
  guint                use_command : 1;
  guint                disable_check : 1;
};

enum {
  PROP_0,
  PROP_PROPERTY_NAME,
  PROP_PACKING,
  PROP_USE_COMMAND,
  PROP_EDITOR_TYPE,
  PROP_CUSTOM_TEXT,
  PROP_DISABLE_CHECK
};

enum
{
  PRE_COMMIT,
  POST_COMMIT,
  LAST_SIGNAL
};

static guint glade_property_shell_signals[LAST_SIGNAL] = { 0, };

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladePropertyShell, glade_property_shell, GTK_TYPE_BOX,
                         G_ADD_PRIVATE (GladePropertyShell)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_property_shell_editable_init));

static void
glade_property_shell_init (GladePropertyShell *shell)
{
  shell->priv = glade_property_shell_get_instance_private (shell);

  shell->priv->packing = FALSE;
  shell->priv->use_command = TRUE;
  shell->priv->disable_check = FALSE;
}

static void
glade_property_shell_class_init (GladePropertyShellClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gobject_class->finalize = glade_property_shell_finalize;
  gobject_class->set_property = glade_property_shell_set_real_property;
  gobject_class->get_property = glade_property_shell_get_real_property;

  g_object_class_install_property
      (gobject_class, PROP_PROPERTY_NAME,
       g_param_spec_string ("property-name", _("Property Name"),
                            _("The property name to use when loading by widget"),
                            NULL, G_PARAM_READWRITE));

  g_object_class_install_property
      (gobject_class, PROP_PACKING,
       g_param_spec_boolean ("packing", _("Packing"),
                             _("Whether the property to load is a packing property or not"),
                             FALSE, G_PARAM_READWRITE));

  g_object_class_install_property
      (gobject_class, PROP_USE_COMMAND,
       g_param_spec_boolean ("use-command", _("Use Command"),
                             _("Whether to use the GladeCommand API when modifying properties"),
                             TRUE, G_PARAM_READWRITE));

  g_object_class_install_property
      (gobject_class, PROP_EDITOR_TYPE,
       g_param_spec_string ("editor-type", _("Editor Property Type Name"),
                            _("Specify the actual editor property type name to use for this shell"),
                            NULL, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property
      (gobject_class, PROP_CUSTOM_TEXT,
       g_param_spec_string ("custom-text", _("Custom Text"),
                            _("Custom Text to display in the property label"),
                            NULL, G_PARAM_READWRITE));
  
  g_object_class_install_property
      (gobject_class, PROP_DISABLE_CHECK,
       g_param_spec_boolean ("disable-check", _("Disable Check"),
                             _("Whether to explicitly disable the check button"),
                             FALSE, G_PARAM_READWRITE));
  
  /**
   * GladePropertyShell::pre-commit:
   * @gladeeditorproperty: the #GladeEditorProperty which changed value
   * @arg1: the new #GValue to commit.
   *
   * Emitted before a property's value is committed, can be useful to serialize
   * commands before a property's commit command from custom editors.
   */
  glade_property_shell_signals[PRE_COMMIT] =
      g_signal_new ("pre-commit",
                    G_TYPE_FROM_CLASS (gobject_class),
                    G_SIGNAL_RUN_LAST,
                    0, NULL, NULL,
                    _glade_marshal_VOID__POINTER,
                    G_TYPE_NONE, 1, G_TYPE_POINTER);

  /**
   * GladePropertyShell::post-commit:
   * @gladeeditorproperty: the #GladeEditorProperty which changed value
   * @arg1: the new #GValue to commit.
   *
   * Emitted after a property's value is committed, can be useful to serialize
   * commands after a property's commit command from custom editors.
   */
  glade_property_shell_signals[POST_COMMIT] =
      g_signal_new ("post-commit",
                    G_TYPE_FROM_CLASS (gobject_class),
                    G_SIGNAL_RUN_LAST,
                    0, NULL, NULL,
                    _glade_marshal_VOID__POINTER,
                    G_TYPE_NONE, 1, G_TYPE_POINTER);
}


/***********************************************************
 *                     GObjectClass                        *
 ***********************************************************/
static void
glade_property_shell_finalize (GObject *object)
{
  GladePropertyShell *shell = GLADE_PROPERTY_SHELL (object);

  g_free (shell->priv->property_name);
  g_free (shell->priv->custom_text);

  G_OBJECT_CLASS (glade_property_shell_parent_class)->finalize (object);
}

static void
glade_property_shell_set_real_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GladePropertyShell *shell = GLADE_PROPERTY_SHELL (object);
  GladePropertyShellPrivate *priv = shell->priv;
  const gchar *type_name = NULL;
  GType type = 0;

  switch (prop_id)
    {
    case PROP_PROPERTY_NAME:
      glade_property_shell_set_property_name (shell, g_value_get_string (value));
      break;
    case PROP_PACKING:
      glade_property_shell_set_packing (shell, g_value_get_boolean (value));
      break;
    case PROP_USE_COMMAND:
      glade_property_shell_set_use_command (shell, g_value_get_boolean (value));
      break;
    case PROP_EDITOR_TYPE:
      type_name = g_value_get_string (value);

      if (type_name)
        type = glade_util_get_type_from_name (type_name, FALSE);

      if (type > 0 && !g_type_is_a (type, GLADE_TYPE_EDITOR_PROPERTY))
        g_warning ("Editor type '%s' is not a GladeEditorProperty", type_name);
      else
        priv->editor_type = type;

      break;
    case PROP_CUSTOM_TEXT:
      glade_property_shell_set_custom_text (shell, g_value_get_string (value));
      break;
    case PROP_DISABLE_CHECK:
      glade_property_shell_set_disable_check (shell, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glade_property_shell_get_real_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GladePropertyShell *shell = GLADE_PROPERTY_SHELL (object);

  switch (prop_id)
    {
    case PROP_PROPERTY_NAME:
      g_value_set_string (value, glade_property_shell_get_property_name (shell));
      break;
    case PROP_PACKING:
      g_value_set_boolean (value, glade_property_shell_get_packing (shell));
      break;
    case PROP_USE_COMMAND:
      g_value_set_boolean (value, glade_property_shell_get_use_command (shell));
      break;
    case PROP_CUSTOM_TEXT:
      g_value_set_string (value, glade_property_shell_get_custom_text (shell));
      break;
    case PROP_DISABLE_CHECK:
      g_value_set_boolean (value, glade_property_shell_get_disable_check (shell));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/*******************************************************************************
 *                          GladeEditableInterface                             *
 *******************************************************************************/
static void
propagate_pre_commit (GladeEditorProperty *property,
                      GValue              *value,
                      GladePropertyShell  *shell)
{
  g_signal_emit (G_OBJECT (shell), glade_property_shell_signals[PRE_COMMIT], 0, value);
}

static void
propagate_post_commit (GladeEditorProperty *property,
                       GValue              *value,
                       GladePropertyShell  *shell)
{
  g_signal_emit (G_OBJECT (shell), glade_property_shell_signals[POST_COMMIT], 0, value);
}

static void
glade_property_shell_set_eprop (GladePropertyShell  *shell,
                                GladeEditorProperty *eprop)
{
  GladePropertyShellPrivate *priv = shell->priv;

  if (priv->property_editor != eprop)
    {
      if (priv->property_editor)
        {
          g_signal_handler_disconnect (priv->property_editor, priv->pre_commit_id);
          g_signal_handler_disconnect (priv->property_editor, priv->post_commit_id);
          priv->pre_commit_id = 0;
          priv->post_commit_id = 0;

          gtk_widget_destroy (GTK_WIDGET (priv->property_editor));
        }

      priv->property_editor = eprop;

      if (priv->property_editor)
        {
          glade_editor_property_set_custom_text (priv->property_editor, priv->custom_text);
          glade_editor_property_set_disable_check (priv->property_editor, priv->disable_check);
            
          priv->pre_commit_id = g_signal_connect (priv->property_editor, "commit",
                                                  G_CALLBACK (propagate_pre_commit), shell);
          priv->post_commit_id = g_signal_connect_after (priv->property_editor, "commit",
                                                         G_CALLBACK (propagate_post_commit), shell);

          gtk_container_add (GTK_CONTAINER (shell), GTK_WIDGET (priv->property_editor));
        }
    }
}

static void
glade_property_shell_load (GladeEditable *editable,
                           GladeWidget   *widget)
{
  GladePropertyShell *shell = GLADE_PROPERTY_SHELL (editable);
  GladePropertyShellPrivate *priv;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  g_return_if_fail (shell->priv->property_name != NULL);

  priv = shell->priv;

  if (widget)
    {
      GladeWidgetAdaptor *adaptor = NULL;

      /* Use the parent adaptor if we're a packing property */
      if (priv->packing)
        {
          GladeWidget *parent = glade_widget_get_parent (widget);

          if (parent)
            adaptor = glade_widget_get_adaptor (parent);
        }
      else
        adaptor = glade_widget_get_adaptor (widget);

      /* Need to rebuild the internal editor */
      if (priv->adaptor != adaptor)
        {
          GladePropertyDef *pdef = NULL;
          GladeEditorProperty *eprop = NULL;

          priv->adaptor = adaptor;

          if (adaptor)
            {
              if (priv->packing)
                pdef = glade_widget_adaptor_get_pack_property_def (priv->adaptor,
                                                                   priv->property_name);
              else
                pdef = glade_widget_adaptor_get_property_def (priv->adaptor,
                                                              priv->property_name);
            }

          /* Be forgiving, allow usage of properties that wont work, so that
           * some editors can include properties for subclasses, and hide
           * those properties if they're not applicable
           */
          if (pdef == NULL)
            {
              priv->property_editor = NULL;
            }
          /* Construct custom editor property if specified */
          else if (g_type_is_a (priv->editor_type, GLADE_TYPE_EDITOR_PROPERTY))
            {
              eprop = g_object_new (priv->editor_type,
                                    "property-def", pdef,
                                    "use-command", priv->use_command,
                                    NULL);
            }
          else
            {
              /* Let the adaptor create one */
              eprop = glade_widget_adaptor_create_eprop_by_name (priv->adaptor,
                                                                 priv->property_name,
                                                                 priv->packing,
                                                                 priv->use_command);
            }

          glade_property_shell_set_eprop (shell, eprop);
        }

      /* If we have an editor for the right adaptor, load it */
      if (priv->property_editor)
        glade_editable_load (GLADE_EDITABLE (priv->property_editor), widget);
    }
  else if (priv->property_editor)
    glade_editable_load (GLADE_EDITABLE (priv->property_editor), NULL);
}

static void
glade_property_shell_set_show_name (GladeEditable *editable, gboolean show_name)
{
}

static void
glade_property_shell_editable_init (GladeEditableInterface *iface)
{
  parent_editable_iface = g_type_default_interface_peek (GLADE_TYPE_EDITABLE);

  iface->load = glade_property_shell_load;
  iface->set_show_name = glade_property_shell_set_show_name;
}

/***********************************************************
 *                            API                          *
 ***********************************************************/
GtkWidget *
glade_property_shell_new (void)
{
  return g_object_new (GLADE_TYPE_PROPERTY_SHELL, NULL);
}

void
glade_property_shell_set_property_name (GladePropertyShell *shell,
                                        const gchar        *property_name)
{
  GladePropertyShellPrivate *priv;

  g_return_if_fail (GLADE_IS_PROPERTY_SHELL (shell));

  priv = shell->priv;

  if (g_strcmp0 (priv->property_name, property_name) != 0)
    {
      g_free (priv->property_name);
      priv->property_name = g_strdup (property_name);

      g_object_notify (G_OBJECT (shell), "property-name");
    }
}

const gchar *
glade_property_shell_get_property_name (GladePropertyShell *shell)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_SHELL (shell), NULL);

  return shell->priv->property_name;
}

void
glade_property_shell_set_custom_text (GladePropertyShell *shell,
                                      const gchar        *custom_text)
{
  GladePropertyShellPrivate *priv;

  g_return_if_fail (GLADE_IS_PROPERTY_SHELL (shell));

  priv = shell->priv;

  if (g_strcmp0 (priv->custom_text, custom_text) != 0)
    {
      g_free (priv->custom_text);
      priv->custom_text = g_strdup (custom_text);

      if (priv->property_editor)
        glade_editor_property_set_custom_text (priv->property_editor, custom_text);

      g_object_notify (G_OBJECT (shell), "custom-text");
    }
}

const gchar *
glade_property_shell_get_custom_text (GladePropertyShell *shell)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_SHELL (shell), NULL);

  return shell->priv->custom_text;
}

void
glade_property_shell_set_packing (GladePropertyShell *shell,
                                  gboolean            packing)
{
  GladePropertyShellPrivate *priv;

  g_return_if_fail (GLADE_IS_PROPERTY_SHELL (shell));

  priv = shell->priv;

  if (priv->packing != packing)
    {
      priv->packing = packing;

      g_object_notify (G_OBJECT (shell), "packing");
    }
}

gboolean
glade_property_shell_get_packing (GladePropertyShell *shell)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_SHELL (shell), FALSE);

  return shell->priv->packing;
}

void
glade_property_shell_set_use_command (GladePropertyShell *shell,
                                      gboolean            use_command)
{
  GladePropertyShellPrivate *priv;

  g_return_if_fail (GLADE_IS_PROPERTY_SHELL (shell));

  priv = shell->priv;

  if (priv->use_command != use_command)
    {
      priv->use_command = use_command;

      g_object_notify (G_OBJECT (shell), "use-command");
    }
}

gboolean
glade_property_shell_get_use_command (GladePropertyShell *shell)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_SHELL (shell), FALSE);

  return shell->priv->use_command;
}

void
glade_property_shell_set_disable_check (GladePropertyShell *shell,
                                        gboolean            disable_check)
{
  GladePropertyShellPrivate *priv;

  g_return_if_fail (GLADE_IS_PROPERTY_SHELL (shell));

  priv = shell->priv;

  if (priv->disable_check != disable_check)
    {
      priv->disable_check = disable_check;

      if (priv->property_editor)
        g_object_set (priv->property_editor, "disable-check", disable_check, NULL);

      g_object_notify (G_OBJECT (shell), "disable-check");
    }
}

gboolean
glade_property_shell_get_disable_check (GladePropertyShell *shell)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_SHELL (shell), FALSE);

  return shell->priv->disable_check;
}
