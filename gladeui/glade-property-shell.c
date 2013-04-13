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
#include "glade-property-editor.h"
#include "glade-property-shell.h"

/* GObjectClass */
static void      glade_property_shell_finalize          (GObject         *object);
static void      glade_property_shell_set_real_property (GObject         *object,
							 guint            prop_id,
							 const GValue    *value,
							 GParamSpec      *pspec);
static void      glade_property_shell_get_real_property (GObject         *object,
							 guint            prop_id,
							 GValue          *value,
							 GParamSpec      *pspec);

/* GladePropertyEditorInterface */
static void      glade_property_shell_property_editor_init (GladePropertyEditorInterface *iface);

struct _GladePropertyShellPrivate
{
  /* Current State */
  GladeWidgetAdaptor  *adaptor;
  GladeEditorProperty *property_editor;

  /* Properties, used to load the internal editor */
  gchar               *property_name;
  guint                packing : 1;
  guint                use_command : 1;
};

enum {
  PROP_0,
  PROP_PROPERTY_NAME,
  PROP_PACKING,
  PROP_USE_COMMAND,
};

G_DEFINE_TYPE_WITH_CODE (GladePropertyShell, glade_property_shell, GTK_TYPE_BOX,
			 G_IMPLEMENT_INTERFACE (GLADE_TYPE_PROPERTY_EDITOR,
                                                glade_property_shell_property_editor_init));

static void
glade_property_shell_init (GladePropertyShell *shell)
{
  shell->priv = 
    G_TYPE_INSTANCE_GET_PRIVATE (shell,
				 GLADE_TYPE_PROPERTY_SHELL,
				 GladePropertyShellPrivate);

  shell->priv->packing = FALSE;
  shell->priv->use_command = TRUE;
}

static void
glade_property_shell_class_init (GladePropertyShellClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  
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

  g_type_class_add_private (gobject_class, sizeof (GladePropertyShellPrivate));
}


/***********************************************************
 *                     GObjectClass                        *
 ***********************************************************/
static void
glade_property_shell_finalize (GObject *object)
{
  GladePropertyShell *shell = GLADE_PROPERTY_SHELL (object);

  g_free (shell->priv->property_name);

  G_OBJECT_CLASS (glade_property_shell_parent_class)->finalize (object);
}

static void
glade_property_shell_set_real_property (GObject         *object,
					guint            prop_id,
					const GValue    *value,
					GParamSpec      *pspec)
{
  GladePropertyShell *shell = GLADE_PROPERTY_SHELL (object);

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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glade_property_shell_get_real_property (GObject         *object,
					guint            prop_id,
					GValue          *value,
					GParamSpec      *pspec)
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
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/*******************************************************************************
 *                         GladePropertyEditorInterface                        *                               
 *******************************************************************************/
static void
glade_property_shell_property_editor_load (GladePropertyEditor  *editor,
					   GladeWidget          *widget)
{
  GladePropertyShell *shell = GLADE_PROPERTY_SHELL (editor);
  GladePropertyShellPrivate *priv;

  g_return_if_fail (shell->priv->property_name != NULL);

  priv = shell->priv;

  if (widget)
    {
      GladeWidgetAdaptor *adaptor = glade_widget_get_adaptor (widget);

      /* Need to rebuild the internal editor */
      if (priv->adaptor != adaptor)
	{
	  if (priv->property_editor)
	    gtk_widget_destroy (GTK_WIDGET (priv->property_editor));

	  priv->adaptor = adaptor;

	  priv->property_editor = 
	    glade_widget_adaptor_create_eprop_by_name (priv->adaptor,
						       priv->property_name,
						       priv->packing,
						       priv->use_command);

	  if (priv->property_editor)
	    gtk_container_add (GTK_CONTAINER (shell), GTK_WIDGET (priv->property_editor));
	}

      /* If we have an editor for the right adaptor, load it */
      if (priv->property_editor)
	glade_property_editor_load (GLADE_PROPERTY_EDITOR (priv->property_editor), widget);
    }
  else if (priv->property_editor)
    glade_property_editor_load (GLADE_PROPERTY_EDITOR (priv->property_editor), NULL);
}

static void
glade_property_shell_property_editor_init (GladePropertyEditorInterface *iface)
{
  iface->load = glade_property_shell_property_editor_load;
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

  if (g_strcmp0 (priv->property_name, property_name))
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
