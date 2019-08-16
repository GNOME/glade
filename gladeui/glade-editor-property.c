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
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 * SECTION:glade-editor-property
 * @Short_Description: A generic widget to edit a #GladeProperty.
 *
 * The #GladeEditorProperty is a factory that will create the correct
 * control for the #GladePropertyDef it was created for and provides
 * a simple unified api to them.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-editable.h"
#include "glade-editor-property.h"
#include "glade-property-label.h"
#include "glade-property.h"
#include "glade-command.h"
#include "glade-project.h"
#include "glade-popup.h"
#include "glade-builtins.h"
#include "glade-marshallers.h"
#include "glade-displayable-values.h"
#include "glade-named-icon-chooser-dialog.h"
#include "glade-private.h"

enum
{
  PROP_0,
  PROP_PROPERTY_DEFINITION,
  PROP_USE_COMMAND,
  PROP_DISABLE_CHECK,
  PROP_CUSTOM_TEXT
};

enum
{
  CHANGED,
  COMMIT,
  LAST_SIGNAL
};

static GtkTableClass *table_class;
static GladeEditorPropertyClass *editor_property_class;

static guint glade_eprop_signals[LAST_SIGNAL] = { 0, };

#define GLADE_PROPERTY_TABLE_ROW_SPACING 2
#define FLAGS_COLUMN_SETTING             0
#define FLAGS_COLUMN_SYMBOL              1
#define FLAGS_COLUMN_VALUE               2

typedef struct _GladeEditorPropertyPrivate
{
  GladePropertyDef   *property_def;   /* The property definition this GladeEditorProperty was created for */
  GladeProperty      *property;       /* The currently loaded property */

  GtkWidget          *item_label;     /* A GladePropertyLabel, if one was constructed */
  GtkWidget          *input;          /* Input part of property (need to set sensitivity seperately)  */
  GtkWidget          *check;          /* Check button for optional properties. */

  gulong              tooltip_id;     /* signal connection id for tooltip changes        */
  gulong              sensitive_id;   /* signal connection id for sensitivity changes    */
  gulong              changed_id;     /* signal connection id for value changes          */
  gulong              enabled_id;     /* signal connection id for enable/disable changes */

  gchar              *custom_text;    /* Custom text to display in the property label */

  guint               loading : 1;    /* True during glade_editor_property_load calls, this
                                       * is used to avoid feedback from input widgets.
                                       */
  guint               committing : 1; /* True while the editor property itself is applying
                                       * the property with glade_editor_property_commit_no_callback ().
                                       */
  guint               use_command : 1; /* Whether we should use the glade command interface
                                        * or skip directly to GladeProperty interface.
                                        * (used for query dialogs).
                                        */
  guint               changed_blocked : 1; /* Whether the GladeProperty changed signal is currently blocked */

  guint               disable_check : 1; /* Whether to explicitly disable the optional check button */
} GladeEditorPropertyPrivate;

static void glade_editor_property_editable_init (GladeEditableInterface *iface);

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeEditorProperty, glade_editor_property, GTK_TYPE_BOX,
                         G_ADD_PRIVATE (GladeEditorProperty)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_editor_property_editable_init));


/*******************************************************************************
 *                          GladeEditableInterface                             *
 *******************************************************************************/
static void
glade_editor_property_editable_load (GladeEditable   *editable,
                                     GladeWidget     *widget)
{
  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  glade_editor_property_load_by_widget (GLADE_EDITOR_PROPERTY (editable), widget);
}

static void
glade_editor_property_set_show_name (GladeEditable *editable, gboolean show_name)
{
}

static void
glade_editor_property_editable_init (GladeEditableInterface *iface)
{
  parent_editable_iface = g_type_default_interface_peek (GLADE_TYPE_EDITABLE);

  iface->load = glade_editor_property_editable_load;
  iface->set_show_name = glade_editor_property_set_show_name;
}

/*******************************************************************************
                               GladeEditorPropertyClass
 *******************************************************************************/

static void
deepest_child_grab_focus (GtkWidget *widget, gpointer data)
{
  gboolean *focus_set = data;

  if (*focus_set)
    return;
  
  if (GTK_IS_CONTAINER (widget))
    gtk_container_foreach (GTK_CONTAINER (widget),
                           deepest_child_grab_focus,
                           data);

  if (gtk_widget_get_can_focus (widget))
    {
      gtk_widget_grab_focus (widget);
      *focus_set = TRUE;
    }
}

/* declare this forwardly for the finalize routine */
static void glade_editor_property_load_common (GladeEditorProperty *eprop,
                                               GladeProperty       *property);

static void
glade_editor_property_commit_common (GladeEditorProperty *eprop,
                                     GValue              *value)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  if (priv->use_command == FALSE)
    glade_property_set_value (priv->property, value);
  else
    glade_command_set_property_value (priv->property, value);

  /* If the value was denied by a verify function, we'll have to
   * reload the real value.
   */
  if (!glade_property_equals_value (priv->property, value))
    glade_editor_property_load (eprop, priv->property);

  /* Restore input focus. If the property is construct-only, then 
   * glade_widget_rebuild() will be called which means the object will be
   * removed from the project/selection and a new one will be added, which makes
   * the eprop loose its focus.
   * 
   * FIXME: find a better way to set focus?
   * make gtk_widget_grab_focus(priv->input) work?
   */
  if (glade_property_def_get_construct_only (priv->property_def))
    {
      gboolean focus_set = FALSE;
      gtk_container_foreach (GTK_CONTAINER (priv->input),
                             deepest_child_grab_focus,
                             &focus_set);
    }
}

void
glade_editor_property_commit_no_callback (GladeEditorProperty *eprop,
                                          GValue              *value)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  g_return_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop));

  if (priv->committing)
    return;

  g_signal_handler_block (G_OBJECT (priv->property), priv->changed_id);
  priv->changed_blocked = TRUE;

  priv->committing = TRUE;
  glade_editor_property_commit (eprop, value);
  priv->committing = FALSE;

  /* When construct-only properties are set, we are disconnected and re-connected
   * to the GladeWidget while it's rebuilding it's instance, in this case the
   * signal handler is no longer blocked at this point.
   */
  if (priv->changed_blocked)
    g_signal_handler_unblock (G_OBJECT (priv->property), priv->changed_id);
}


void
glade_editor_property_set_custom_text (GladeEditorProperty *eprop,
                                       const gchar         *custom_text)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  g_return_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop));

  if (g_strcmp0 (priv->custom_text, custom_text) != 0)
    {
      g_free (priv->custom_text);
      priv->custom_text = g_strdup (custom_text);

      if (priv->item_label)
        glade_property_label_set_custom_text (GLADE_PROPERTY_LABEL (priv->item_label),
                                              custom_text);

      g_object_notify (G_OBJECT (eprop), "custom-text");
    }
}

const gchar *
glade_editor_property_get_custom_text (GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  g_return_val_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop), NULL);

  return priv->custom_text;
}

void
glade_editor_property_set_disable_check (GladeEditorProperty *eprop,
                                         gboolean             disable_check)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  g_return_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop));

  if (priv->disable_check != disable_check)
    {
      priv->disable_check = disable_check;
      gtk_widget_set_visible (priv->check, !disable_check);
      g_object_notify (G_OBJECT (eprop), "disable-check");
    }
}

gboolean
glade_editor_property_get_disable_check (GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  g_return_val_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop), FALSE);

  return priv->disable_check;
}

/**
 * glade_editor_property_get_item_label:
 * @eprop: a #GladeEditorProperty
 *
 * Returns: (transfer none): the #GladePropertyLabel
 */
GtkWidget *
glade_editor_property_get_item_label  (GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  g_return_val_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop), NULL);

  if (!priv->item_label)
    {
      priv->item_label = glade_property_label_new();

      g_object_ref_sink (priv->item_label);

      if (priv->property)
        glade_property_label_set_property (GLADE_PROPERTY_LABEL (priv->item_label),
                                           priv->property);
    }

  return priv->item_label;
}

/**
 * glade_editor_property_get_property_def:
 * @eprop: a #GladeEditorProperty
 *
 * Returns: (transfer none): the #GladePropertyDef
 */
GladePropertyDef *
glade_editor_property_get_property_def (GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  g_return_val_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop), NULL);

  return priv->property_def;
}

/**
 * glade_editor_property_get_property:
 * @eprop: a #GladeEditorProperty
 *
 * Returns: (transfer none): the #GladeProperty
 */
GladeProperty *
glade_editor_property_get_property (GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  g_return_val_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop), NULL);

  return priv->property;
}

gboolean
glade_editor_property_loading (GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  g_return_val_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop), FALSE);

  return priv->loading;
}

static void
glade_editor_property_tooltip_cb (GladeProperty       *property,
                                  const gchar         *tooltip,
                                  const gchar         *insensitive,
                                  const gchar         *support,
                                  GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  const gchar *choice_tooltip;

  if (glade_property_get_sensitive (property))
    choice_tooltip = tooltip;
  else
    choice_tooltip = insensitive;

  gtk_widget_set_tooltip_text (priv->input, choice_tooltip);
}

static void
glade_editor_property_sensitivity_cb (GladeProperty       *property,
                                      GParamSpec          *pspec,
                                      GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  gboolean property_enabled = glade_property_get_enabled (property);
  gboolean sensitive = glade_property_get_sensitive (priv->property);
  gboolean support_sensitive =
    (glade_property_get_state (priv->property) & GLADE_STATE_SUPPORT_DISABLED) == 0;

  gtk_widget_set_sensitive (priv->input,
                            sensitive && support_sensitive && property_enabled);

  if (priv->check)
    gtk_widget_set_sensitive (priv->check, sensitive && support_sensitive);
}

static void
glade_editor_property_value_changed_cb (GladeProperty       *property,
                                        GValue              *old_value,
                                        GValue              *value,
                                        GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  g_assert (priv->property == property);

  glade_editor_property_load (eprop, priv->property);
}

static void
glade_editor_property_enabled_toggled_cb (GtkWidget *check,
                                          GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  glade_command_set_property_enabled (priv->property,
                                      gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)));
}

static void
glade_editor_property_enabled_cb (GladeProperty       *property,
                                  GParamSpec          *pspec,
                                  GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  gboolean enabled;

  g_assert (priv->property == property);

  if (glade_property_def_optional (priv->property_def))
    {
      enabled = glade_property_get_enabled (property);

      /* sensitive = enabled && support enabled && sensitive */
      if (enabled == FALSE)
        gtk_widget_set_sensitive (priv->input, FALSE);
      else if (glade_property_get_sensitive (property) ||
               (glade_property_get_state (property) & GLADE_STATE_SUPPORT_DISABLED) != 0)
        gtk_widget_set_sensitive (priv->input, TRUE);

      g_signal_handlers_block_by_func (priv->check, glade_editor_property_enabled_toggled_cb, eprop);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->check), enabled);
      g_signal_handlers_unblock_by_func (priv->check, glade_editor_property_enabled_toggled_cb, eprop);
    }
}

static gboolean
glade_editor_property_button_pressed (GtkWidget           *widget,
                                      GdkEventButton      *event,
                                      GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  if (glade_popup_is_popup_event (event))
    {
      glade_popup_property_pop (priv->property, event);
      return TRUE;
    }
  return FALSE;
}


static void
glade_editor_property_constructed (GObject *object)
{
  GladeEditorProperty *eprop;
  GladeEditorPropertyPrivate *priv;

  eprop = GLADE_EDITOR_PROPERTY (object);
  priv = glade_editor_property_get_instance_private (eprop);

  G_OBJECT_CLASS (glade_editor_property_parent_class)->constructed (object);

  /* Create hbox and possibly check button
   */
  if (glade_property_def_optional (priv->property_def))
    {
      priv->check = gtk_check_button_new ();
      gtk_widget_set_focus_on_click (priv->check, FALSE);

      if (!priv->disable_check)
        gtk_widget_show (priv->check);

      gtk_box_pack_start (GTK_BOX (eprop), priv->check, FALSE, FALSE, 0);
      g_signal_connect (G_OBJECT (priv->check), "toggled",
                        G_CALLBACK (glade_editor_property_enabled_toggled_cb),
                        eprop);
    }

  /* Create the class specific input widget and add it */
  priv->input = GLADE_EDITOR_PROPERTY_GET_CLASS (eprop)->create_input (eprop);
  gtk_widget_show (priv->input);

  g_signal_connect (G_OBJECT (priv->input), "button-press-event",
                    G_CALLBACK (glade_editor_property_button_pressed), eprop);

  if (gtk_widget_get_halign (priv->input) != GTK_ALIGN_FILL)
    gtk_box_pack_start (GTK_BOX (eprop), priv->input, FALSE, TRUE, 0);
  else
    gtk_box_pack_start (GTK_BOX (eprop), priv->input, TRUE, TRUE, 0);
}

static void
glade_editor_property_finalize (GObject *object)
{
  GladeEditorProperty *eprop = GLADE_EDITOR_PROPERTY (object);
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  /* detatch from loaded property */
  glade_editor_property_load_common (eprop, NULL);

  g_free (priv->custom_text);

  G_OBJECT_CLASS (table_class)->finalize (object);
}

static void
glade_editor_property_dispose (GObject *object)
{
  GladeEditorProperty *eprop = GLADE_EDITOR_PROPERTY (object);
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  g_clear_object (&priv->item_label);

  G_OBJECT_CLASS (table_class)->dispose (object);
}

static void
glade_editor_property_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GladeEditorProperty *eprop = GLADE_EDITOR_PROPERTY (object);
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  switch (prop_id)
    {
      case PROP_PROPERTY_DEFINITION:
        priv->property_def = g_value_get_pointer (value);
        break;
      case PROP_USE_COMMAND:
        priv->use_command = g_value_get_boolean (value);
        break;
      case PROP_DISABLE_CHECK:
        glade_editor_property_set_disable_check (eprop, g_value_get_boolean (value));
        break;
      case PROP_CUSTOM_TEXT:
        glade_editor_property_set_custom_text (eprop, g_value_get_string (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_editor_property_real_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GladeEditorProperty *eprop = GLADE_EDITOR_PROPERTY (object);
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  switch (prop_id)
    {
      case PROP_PROPERTY_DEFINITION:
        g_value_set_pointer (value, priv->property_def);
        break;
      case PROP_USE_COMMAND:
        g_value_set_boolean (value, priv->use_command);
        break;
      case PROP_DISABLE_CHECK:
        g_value_set_boolean (value, priv->disable_check);
        break;
      case PROP_CUSTOM_TEXT:
        g_value_set_string (value, priv->custom_text);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_eprop_property_finalized (GladeEditorProperty *eprop,
                                GladeProperty       *where_property_was)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  priv->tooltip_id = 0;
  priv->sensitive_id = 0;
  priv->changed_id = 0;
  priv->enabled_id = 0;
  priv->property = NULL;

  glade_editor_property_load (eprop, NULL);
}

static void
glade_editor_property_load_common (GladeEditorProperty *eprop,
                                   GladeProperty       *property)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  /* NOTE THIS CODE IS FINALIZE SAFE */

  /* disconnect anything from previously loaded property */
  if (priv->property != property && priv->property != NULL)
    {
      if (priv->tooltip_id > 0)
        g_signal_handler_disconnect (priv->property, priv->tooltip_id);
      if (priv->sensitive_id > 0)
        g_signal_handler_disconnect (priv->property, priv->sensitive_id);
      if (priv->changed_id > 0)
        g_signal_handler_disconnect (priv->property, priv->changed_id);
      if (priv->enabled_id > 0)
        g_signal_handler_disconnect (priv->property, priv->enabled_id);

      priv->tooltip_id = 0;
      priv->sensitive_id = 0;
      priv->changed_id = 0;
      priv->enabled_id = 0;
      priv->changed_blocked = FALSE;

      /* Unref it here */
      g_object_weak_unref (G_OBJECT (priv->property),
                           (GWeakNotify) glade_eprop_property_finalized, eprop);


      /* For a reason I cant quite tell yet, this is the only
       * safe way to nullify the property member of the eprop
       * without leeking signal connections to properties :-/
       */
      if (property == NULL)
        {
          priv->property = NULL;
        }
    }

  /* Connect new stuff, deal with tooltip
   */
  if (priv->property != property && property != NULL)
    {
      GladePropertyDef *pclass = glade_property_get_def (property);

      priv->property = property;

      priv->tooltip_id =
          g_signal_connect (G_OBJECT (priv->property),
                            "tooltip-changed",
                            G_CALLBACK (glade_editor_property_tooltip_cb),
                            eprop);
      priv->sensitive_id =
          g_signal_connect (G_OBJECT (priv->property),
                            "notify::sensitive",
                            G_CALLBACK (glade_editor_property_sensitivity_cb),
                            eprop);
      priv->changed_id =
          g_signal_connect (G_OBJECT (priv->property),
                            "value-changed",
                            G_CALLBACK (glade_editor_property_value_changed_cb),
                            eprop);
      priv->enabled_id =
          g_signal_connect (G_OBJECT (priv->property),
                            "notify::enabled",
                            G_CALLBACK (glade_editor_property_enabled_cb),
                            eprop);

      /* In query dialogs when the user hits cancel, 
       * these babies go away (so better stay protected).
       */
      g_object_weak_ref (G_OBJECT (priv->property),
                         (GWeakNotify) glade_eprop_property_finalized, eprop);

      /* Load initial tooltips
       */
      glade_editor_property_tooltip_cb
        (property, glade_property_def_get_tooltip (pclass),
           glade_propert_get_insensitive_tooltip (property),
           glade_property_get_support_warning (property), eprop);

      /* Load initial enabled state
       */
      glade_editor_property_enabled_cb (property, NULL, eprop);

      /* Load initial sensitive state.
       */
      glade_editor_property_sensitivity_cb (property, NULL, eprop);
    }
}

static void
glade_editor_property_init (GladeEditorProperty *eprop)
{
  gtk_box_set_spacing (GTK_BOX (eprop), 4);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (eprop),
                                  GTK_ORIENTATION_HORIZONTAL);
}

static void
glade_editor_property_class_init (GladeEditorPropertyClass *eprop_class)
{
  GObjectClass *object_class;
  g_return_if_fail (eprop_class != NULL);

  /* Both parent classes assigned here.
   */
  editor_property_class = eprop_class;
  table_class = g_type_class_peek_parent (eprop_class);
  object_class = G_OBJECT_CLASS (eprop_class);

  /* GObjectClass */
  object_class->constructed = glade_editor_property_constructed;
  object_class->finalize = glade_editor_property_finalize;
  object_class->dispose = glade_editor_property_dispose;
  object_class->get_property = glade_editor_property_real_get_property;
  object_class->set_property = glade_editor_property_set_property;

  /* Class methods */
  eprop_class->load = glade_editor_property_load_common;
  eprop_class->commit = glade_editor_property_commit_common;
  eprop_class->create_input = NULL;


  /**
   * GladeEditorProperty::value-changed:
   * @gladeeditorproperty: the #GladeEditorProperty which changed value
   * @arg1: the #GladeProperty that's value changed.
   *
   * Emitted when a contained property changes value
   */
  glade_eprop_signals[CHANGED] =
      g_signal_new ("value-changed",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeEditorPropertyClass, changed),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__OBJECT,
                    G_TYPE_NONE, 1, GLADE_TYPE_PROPERTY);

  /**
   * GladeEditorProperty::commit:
   * @gladeeditorproperty: the #GladeEditorProperty which changed value
   * @arg1: the new #GValue to commit.
   *
   * Emitted when a property's value is committed, can be useful to serialize
   * commands before and after the property's commit command from custom editors.
   */
  glade_eprop_signals[COMMIT] =
      g_signal_new ("commit",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeEditorPropertyClass, commit),
                    NULL, NULL,
                    _glade_marshal_VOID__POINTER,
                    G_TYPE_NONE, 1, G_TYPE_POINTER);

  /* Properties */
  g_object_class_install_property
      (object_class, PROP_PROPERTY_DEFINITION,
       g_param_spec_pointer
       ("property-def", _("Property Definition"),
        _("The GladePropertyDef this GladeEditorProperty was created for"),
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property
      (object_class, PROP_USE_COMMAND,
       g_param_spec_boolean
       ("use-command", _("Use Command"),
        _("Whether we should use the command API for the undo/redo stack"),
        FALSE, G_PARAM_READWRITE));

  g_object_class_install_property
      (object_class, PROP_DISABLE_CHECK,
       g_param_spec_boolean
       ("disable-check", _("Disable Check"),
        _("Whether to explicitly disable the check button"),
        FALSE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property
      (object_class, PROP_CUSTOM_TEXT,
       g_param_spec_string
       ("custom-text", _("Custom Text"),
        _("Custom Text to display in the property label"),
        NULL, G_PARAM_READWRITE));
}

/*******************************************************************************
                        GladeEditorPropertyNumericClass
 *******************************************************************************/
struct _GladeEPropNumeric
{
  GladeEditorProperty parent_instance;

  GtkWidget *spin;
  GBinding *binding;

  gboolean refreshing;
};

GLADE_MAKE_EPROP (GladeEPropNumeric, glade_eprop_numeric, GLADE, EPROP_NUMERIC)

static void
glade_eprop_numeric_finalize (GObject *object)
{
  /* Chain up */
  G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_numeric_load (GladeEditorProperty *eprop, GladeProperty *property)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  gdouble val = 0.0F;
  GladeEPropNumeric *eprop_numeric = GLADE_EPROP_NUMERIC (eprop);
  GParamSpec *pspec;
  GValue *value;

  if (eprop_numeric->refreshing)
    return;
  
  /* Chain up first */
  editor_property_class->load (eprop, property);

  g_clear_object (&eprop_numeric->binding);

  if (property)
    {
      value = glade_property_inline_value (property);
      pspec = glade_property_def_get_pspec (priv->property_def);

      if (G_IS_PARAM_SPEC_INT (pspec))
        val = g_value_get_int (value);
      else if (G_IS_PARAM_SPEC_UINT (pspec))
        val = g_value_get_uint (value);
      else if (G_IS_PARAM_SPEC_LONG (pspec))
        val = g_value_get_long (value);
      else if (G_IS_PARAM_SPEC_ULONG (pspec))
        val = g_value_get_ulong (value);
      else if (G_IS_PARAM_SPEC_INT64 (pspec))
        val = g_value_get_int64 (value);
      else if (G_IS_PARAM_SPEC_UINT64 (pspec))
        val = g_value_get_uint64 (value);
      else if (G_IS_PARAM_SPEC_DOUBLE (pspec))
        val = g_value_get_double (value);
      else if (G_IS_PARAM_SPEC_FLOAT (pspec))
        val = g_value_get_float (value);
      else
        g_warning ("Unsupported type %s\n",
                   g_type_name (G_PARAM_SPEC_TYPE (pspec)));
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (eprop_numeric->spin), val);

      if (G_IS_PARAM_SPEC_FLOAT (pspec) || G_IS_PARAM_SPEC_DOUBLE (pspec))
        eprop_numeric->binding = g_object_bind_property (property,
                                                         "precision",
                                                         eprop_numeric->spin,
                                                         "digits",
                                                         G_BINDING_SYNC_CREATE);
    }
}

#define NEAREST_INT_CAST(x) (((x - floor (x) < ceil (x) - x)) ? floor (x) : ceil (x))

static void
glade_eprop_numeric_value_set (GValue *val, gdouble value)
{
  if (G_VALUE_HOLDS_INT (val))
    g_value_set_int (val, NEAREST_INT_CAST (value));
  else if (G_VALUE_HOLDS_UINT (val))
    g_value_set_uint (val, NEAREST_INT_CAST (value));
  else if (G_VALUE_HOLDS_LONG (val))
    g_value_set_long (val, NEAREST_INT_CAST (value));
  else if (G_VALUE_HOLDS_ULONG (val))
    g_value_set_ulong (val, NEAREST_INT_CAST (value));
  else if (G_VALUE_HOLDS_INT64 (val))
    g_value_set_int64 (val, NEAREST_INT_CAST (value));
  else if (G_VALUE_HOLDS_UINT64 (val))
    g_value_set_uint64 (val, NEAREST_INT_CAST (value));
  else if (G_VALUE_HOLDS_FLOAT (val))
    g_value_set_float (val, value);
  else if (G_VALUE_HOLDS_DOUBLE (val))
    g_value_set_double (val, value);
  else
    g_warning ("Unsupported type %s\n", G_VALUE_TYPE_NAME (val));
}

static void
glade_eprop_numeric_changed (GtkWidget *spin, GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GValue val = { 0, };
  GParamSpec *pspec;

  if (priv->loading)
    return;

  pspec = glade_property_def_get_pspec (priv->property_def);
  g_value_init (&val, pspec->value_type);
  glade_eprop_numeric_value_set (&val, gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin)));

  glade_editor_property_commit_no_callback (eprop, &val);
  g_value_unset (&val);
}

static void
glade_eprop_numeric_force_update (GtkSpinButton       *spin,
                                  GladeEditorProperty *eprop)
{
  GladeProperty *prop = glade_editor_property_get_property (eprop);
  GladePropertyDef *property_def = glade_property_get_def (prop);
  GValue *val, newval = G_VALUE_INIT;
  gdouble value;
  gchar *text;

  text = gtk_editable_get_chars (GTK_EDITABLE (spin), 0, -1);

  /*
   * Skip empty strings, otherwise if 0 is out of range the spin will get a
   * bogus update.
   */
  if (text && *text == '\0')
    return;
  
  val = glade_property_inline_value (prop);

  g_value_init (&newval, G_VALUE_TYPE (val));
  value = g_strtod (text, NULL);
  glade_eprop_numeric_value_set (&newval, value);

  /* 
   * If we unconditionally update the spin button whenever
   * the entry changes we get bogus results (notably, the
   * updating the spin button will insert 0 whenever text
   * is removed, so selecting and inserting text will have
   * an appended 0).
   */                                             
  if (glade_property_def_compare (property_def, val, &newval))
    {
      gdouble min, max;

      gtk_spin_button_get_range (spin, &min, &max);

      if (value < min || value > max)
        {
          /* Special case, if the value is out of range, we force an update to
           * change the value in the spin so the user knows about the range issue.
           */
          gtk_spin_button_update (spin);
        }
      else
        {
          /* Here we commit the new property value but we make sure 
           * glade_eprop_numeric_load() is not called to prevent
           * gtk_spin_button_set_value() changing the the value the
           * user is trying to input.
           */
          GladeEPropNumeric *eprop_numeric = GLADE_EPROP_NUMERIC (eprop);
          eprop_numeric->refreshing = TRUE;
          glade_editor_property_commit_no_callback (eprop, &newval);
          eprop_numeric->refreshing = FALSE;
        }
    }

  g_value_unset (&newval);
  g_free (text);
}

static void
on_spin_digits_notify (GObject *gobject, GParamSpec *pspec, gpointer user_data)
{
  gint digits = gtk_spin_button_get_digits (GTK_SPIN_BUTTON (gobject));
  gdouble step = 1.0 / pow (10, digits);

  gtk_spin_button_set_increments (GTK_SPIN_BUTTON (gobject), step, step * 10);
}

static GtkWidget *
glade_eprop_numeric_create_input (GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GladeEPropNumeric *eprop_numeric = GLADE_EPROP_NUMERIC (eprop);
  GtkAdjustment *adjustment;
  GParamSpec *pspec;

  pspec      = glade_property_def_get_pspec (priv->property_def);
  adjustment = glade_property_def_make_adjustment (priv->property_def);
  eprop_numeric->spin = 
    gtk_spin_button_new (adjustment, 0.01,
                         G_IS_PARAM_SPEC_FLOAT (pspec) ||
                         G_IS_PARAM_SPEC_DOUBLE (pspec) ? 2 : 0);
  gtk_widget_set_hexpand (eprop_numeric->spin, TRUE);
  gtk_widget_set_halign (eprop_numeric->spin, GTK_ALIGN_FILL);
  gtk_widget_set_valign (eprop_numeric->spin, GTK_ALIGN_CENTER);

  gtk_entry_set_activates_default (GTK_ENTRY (eprop_numeric->spin), TRUE);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (eprop_numeric->spin), TRUE);
  g_signal_connect (eprop_numeric->spin, "notify::digits",
                    G_CALLBACK (on_spin_digits_notify), NULL);

  glade_util_remove_scroll_events (eprop_numeric->spin);
  gtk_widget_show (eprop_numeric->spin);

  /* Limit the size of the spin if max allowed value is too big */
  if (gtk_adjustment_get_upper (adjustment) > 9999999999999999.0)
    gtk_entry_set_width_chars (GTK_ENTRY (eprop_numeric->spin), 16);

  /* The force update callback is here to ensure that whenever the value
   * is modified, it's committed immediately without requiring entry activation
   * (this avoids lost modifications when modifying a value and navigating away)
   */
  g_signal_connect (G_OBJECT (eprop_numeric->spin), "changed",
                    G_CALLBACK (glade_eprop_numeric_force_update), eprop);

  g_signal_connect (G_OBJECT (eprop_numeric->spin), "value-changed",
                    G_CALLBACK (glade_eprop_numeric_changed), eprop);

  return eprop_numeric->spin;
}

/*******************************************************************************
                        GladeEditorPropertyEnumClass
 *******************************************************************************/
struct _GladeEPropEnum
{
  GladeEditorProperty parent_instance;

  GtkWidget *combo_box;
};

GLADE_MAKE_EPROP (GladeEPropEnum, glade_eprop_enum, GLADE, EPROP_ENUM)

static void
glade_eprop_enum_finalize (GObject *object)
{
  /* Chain up */
  G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_enum_load (GladeEditorProperty *eprop, GladeProperty *property)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GladeEPropEnum *eprop_enum = GLADE_EPROP_ENUM (eprop);
  GParamSpec *pspec;
  GEnumClass *eclass;
  guint i;
  gint value;

  /* Chain up first */
  editor_property_class->load (eprop, property);

  if (property)
    {
      pspec  = glade_property_def_get_pspec (priv->property_def);
      eclass = g_type_class_ref (pspec->value_type);
      value  = g_value_get_enum (glade_property_inline_value (property));

      for (i = 0; i < eclass->n_values; i++)
        if (eclass->values[i].value == value)
          break;

      gtk_combo_box_set_active (GTK_COMBO_BOX (eprop_enum->combo_box),
                                i < eclass->n_values ? i : 0);
      g_type_class_unref (eclass);
    }
}

static void
glade_eprop_enum_changed (GtkWidget *combo_box, GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  gint ival;
  GValue val = { 0, };
  GParamSpec *pspec;
  GtkTreeModel *tree_model;
  GtkTreeIter iter;

  if (priv->loading)
    return;

  tree_model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo_box), &iter);
  gtk_tree_model_get (tree_model, &iter, 1, &ival, -1);

  pspec    = glade_property_def_get_pspec (priv->property_def);

  g_value_init (&val, pspec->value_type);
  g_value_set_enum (&val, ival);

  glade_editor_property_commit_no_callback (eprop, &val);
  g_value_unset (&val);
}

static GtkWidget *
glade_eprop_enum_create_input (GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GladeEPropEnum *eprop_enum = GLADE_EPROP_ENUM (eprop);
  GladePropertyDef *property_def;
  GParamSpec *pspec;
  GEnumClass *eclass;
  GtkListStore *list_store;
  GtkTreeIter iter;
  GtkCellRenderer *cell_renderer;
  guint i;

  property_def  = priv->property_def;
  pspec  = glade_property_def_get_pspec (property_def);
  eclass = g_type_class_ref (pspec->value_type);

  list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);

  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store), &iter);

  for (i = 0; i < eclass->n_values; i++)
    {
      const gchar *value_name;

      if (glade_displayable_value_is_disabled (pspec->value_type,
                                               eclass->values[i].value_nick))
        continue;

      value_name = glade_get_displayable_value (pspec->value_type,
                                                eclass->values[i].value_nick);
      if (value_name == NULL)
        value_name = eclass->values[i].value_nick;

      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (list_store, &iter, 0, value_name, 1,
                          eclass->values[i].value, -1);
    }

  eprop_enum->combo_box = gtk_combo_box_new_with_model (GTK_TREE_MODEL (list_store));
  gtk_widget_set_halign (eprop_enum->combo_box, GTK_ALIGN_FILL);
  gtk_widget_set_valign (eprop_enum->combo_box, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (eprop_enum->combo_box, TRUE);
  
  cell_renderer = gtk_cell_renderer_text_new ();
  g_object_set (cell_renderer,
                "wrap-mode", PANGO_WRAP_WORD,
                "wrap-width", 1,
                "width-chars", 8,
                NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (eprop_enum->combo_box),
                              cell_renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (eprop_enum->combo_box),
                                 cell_renderer, "text", 0);

  g_signal_connect (G_OBJECT (eprop_enum->combo_box), "changed",
                    G_CALLBACK (glade_eprop_enum_changed), eprop);

  glade_util_remove_scroll_events (eprop_enum->combo_box);
  gtk_widget_show_all (eprop_enum->combo_box);

  g_type_class_unref (eclass);

  return eprop_enum->combo_box;
}

/*******************************************************************************
                        GladeEditorPropertyFlagsClass
 *******************************************************************************/
struct _GladeEPropFlags
{
  GladeEditorProperty parent_instance;

  GtkTreeModel *model;
  GtkWidget *entry;
};

GLADE_MAKE_EPROP (GladeEPropFlags, glade_eprop_flags, GLADE, EPROP_FLAGS)

static void
glade_eprop_flags_finalize (GObject *object)
{
  GladeEPropFlags *eprop_flags = GLADE_EPROP_FLAGS (object);

  g_object_unref (G_OBJECT (eprop_flags->model));

  /* Chain up */
  G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_flags_load (GladeEditorProperty *eprop, GladeProperty *property)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GladeEPropFlags *eprop_flags = GLADE_EPROP_FLAGS (eprop);
  GFlagsClass *klass;
  GParamSpec  *pspec;
  guint flag_num, value;
  GString *string = g_string_new (NULL);

  /* Chain up first */
  editor_property_class->load (eprop, property);

  gtk_list_store_clear (GTK_LIST_STORE (eprop_flags->model));

  if (property)
    {
      /* Populate the model with the flags. */
      klass = g_type_class_ref (G_VALUE_TYPE (glade_property_inline_value (property)));
      value = g_value_get_flags (glade_property_inline_value (property));
      pspec = glade_property_def_get_pspec (priv->property_def);

      /* Step through each of the flags in the class. */
      for (flag_num = 0; flag_num < klass->n_values; flag_num++)
        {
          GtkTreeIter iter;
          guint mask;
          gboolean setting;
          const gchar *value_name;

          if (glade_displayable_value_is_disabled (pspec->value_type,
                                                   klass->values[flag_num].value_nick))
            continue;
          
          mask = klass->values[flag_num].value;
          setting = ((value & mask) == mask) ? TRUE : FALSE;

          value_name = glade_get_displayable_value
              (pspec->value_type, klass->values[flag_num].value_nick);

          if (value_name == NULL)
            value_name = klass->values[flag_num].value_name;

          /* Setup string for property label */
          if (setting)
            {
              if (string->len > 0)
                g_string_append (string, " | ");
              g_string_append (string, value_name);
            }

          /* Add a row to represent the flag. */
          gtk_list_store_append (GTK_LIST_STORE (eprop_flags->model), &iter);
          gtk_list_store_set (GTK_LIST_STORE (eprop_flags->model), &iter,
                              FLAGS_COLUMN_SETTING, setting,
                              FLAGS_COLUMN_SYMBOL, value_name,
                              FLAGS_COLUMN_VALUE, mask, -1);

        }
      g_type_class_unref (klass);
    }

  gtk_entry_set_text (GTK_ENTRY (eprop_flags->entry), string->str);

  g_string_free (string, TRUE);
}


static void
flag_toggled_direct (GtkCellRendererToggle *cell,
                     gchar                 *path_string,
                     GladeEditorProperty   *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GtkTreeIter iter;
  guint new_value = 0;
  gboolean selected;
  GValue *gvalue;
  gboolean valid;

  GladeEPropFlags *eprop_flags = GLADE_EPROP_FLAGS (eprop);

  if (!priv->property)
    return;

  gvalue = glade_property_inline_value (priv->property);

  gtk_tree_model_get_iter_from_string (eprop_flags->model, &iter, path_string);

  gtk_tree_model_get (eprop_flags->model, &iter,
                      FLAGS_COLUMN_SETTING, &selected, -1);

  selected = selected ? FALSE : TRUE;

  gtk_list_store_set (GTK_LIST_STORE (eprop_flags->model), &iter,
                      FLAGS_COLUMN_SETTING, selected, -1);

  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (eprop_flags->model), &iter);

  /* Step through each of the flags in the class, checking if
     the corresponding toggle in the dialog is selected, If it
     is, OR the flags' mask with the new value. */
  while (valid)
    {
      gboolean setting;
      guint value;

      gtk_tree_model_get (GTK_TREE_MODEL (eprop_flags->model), &iter,
                          FLAGS_COLUMN_SETTING, &setting,
                          FLAGS_COLUMN_VALUE, &value, -1);

      if (setting) new_value |= value;

      valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (eprop_flags->model), &iter);
    }

  /* If the new_value is different from the old value, we need
     to update the property. */
  if (new_value != g_value_get_flags (gvalue))
    {
      GValue val = { 0, };

      g_value_init (&val, G_VALUE_TYPE (gvalue));
      g_value_set_flags (&val, new_value);

      glade_editor_property_commit_no_callback (eprop, &val);
      g_value_unset (&val);
    }
}

static GtkWidget *
glade_eprop_flags_create_treeview (GladeEditorProperty *eprop)
{
  GtkWidget *scrolled_window;
  GtkWidget *tree_view;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GladeEPropFlags *eprop_flags = GLADE_EPROP_FLAGS (eprop);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
                                       GTK_SHADOW_IN);
  gtk_widget_show (scrolled_window);



  tree_view =
      gtk_tree_view_new_with_model (GTK_TREE_MODEL (eprop_flags->model));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);
  gtk_widget_show (tree_view);
  gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);

  column = gtk_tree_view_column_new ();

  renderer = gtk_cell_renderer_toggle_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "active", FLAGS_COLUMN_SETTING, NULL);

  g_signal_connect (renderer, "toggled",
                    G_CALLBACK (flag_toggled_direct), eprop);


  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", FLAGS_COLUMN_SYMBOL, NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);



  return scrolled_window;
}

static void
glade_eprop_flags_show_dialog (GladeEditorProperty *eprop)
{
  GtkWidget *window = gtk_widget_get_toplevel (GTK_WIDGET (eprop));
  GtkWidget *dialog;
  GtkWidget *view;
  GtkWidget *label;
  GtkWidget *vbox;
  GtkWidget *content_area;

  dialog = gtk_dialog_new_with_buttons (_("Select Fields"),
                                        GTK_WINDOW (window),
                                        GTK_DIALOG_MODAL,
                                        _("_Close"), GTK_RESPONSE_CLOSE,
                                        NULL);

  gtk_window_set_default_size (GTK_WINDOW (dialog), 300, 400);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

  _glade_util_dialog_set_hig (GTK_DIALOG (dialog));
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

  view = glade_eprop_flags_create_treeview (eprop);

  label = gtk_label_new_with_mnemonic (_("_Select individual fields:"));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),
                                 gtk_bin_get_child (GTK_BIN (view)));

  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), view, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (content_area), vbox, TRUE, TRUE, 0);

  gtk_widget_show (label);
  gtk_widget_show (view);
  gtk_widget_show (vbox);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}


static GtkWidget *
glade_eprop_flags_create_input (GladeEditorProperty *eprop)
{
  GladeEPropFlags *eprop_flags = GLADE_EPROP_FLAGS (eprop);

  if (!eprop_flags->model)
    eprop_flags->model = GTK_TREE_MODEL (gtk_list_store_new (3, G_TYPE_BOOLEAN,
                                                             G_TYPE_STRING,
                                                             G_TYPE_UINT));

  eprop_flags->entry = gtk_entry_new ();
  gtk_widget_set_hexpand (eprop_flags->entry, TRUE);

  gtk_editable_set_editable (GTK_EDITABLE (eprop_flags->entry), FALSE);
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (eprop_flags->entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     "gtk-edit");

  g_signal_connect_swapped (eprop_flags->entry, "icon-release",
                            G_CALLBACK (glade_eprop_flags_show_dialog),
                            eprop);

  return eprop_flags->entry;
}

/*******************************************************************************
                        GladeEditorPropertyColorClass
 *******************************************************************************/
struct _GladeEPropColor
{
  GladeEditorProperty parent_instance;

  GtkWidget *cbutton;
  GtkWidget *entry;
};

GLADE_MAKE_EPROP (GladeEPropColor, glade_eprop_color, GLADE, EPROP_COLOR)

static void
glade_eprop_color_finalize (GObject *object)
{
  /* Chain up */
  G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_color_load (GladeEditorProperty *eprop, GladeProperty *property)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GladeEPropColor *eprop_color = GLADE_EPROP_COLOR (eprop);
  GParamSpec *pspec;
  GdkColor *color;
  PangoColor *pango_color;
  GdkRGBA *rgba;
  gchar *text;

  /* Chain up first */
  editor_property_class->load (eprop, property);

  pspec = glade_property_def_get_pspec (priv->property_def);

  if (property)
    {
      if ((text = glade_property_make_string (property)) != NULL)
        {
          gtk_entry_set_text (GTK_ENTRY (eprop_color->entry), text);
          g_free (text);
        }
      else
        gtk_entry_set_text (GTK_ENTRY (eprop_color->entry), "");

      if (pspec->value_type == GDK_TYPE_COLOR)
        {
          if ((color = g_value_get_boxed (glade_property_inline_value (property))) != NULL)
            {
              GdkRGBA copy;

              copy.red   = color->red   / 65535.0;
              copy.green = color->green / 65535.0;
              copy.blue  = color->blue  / 65535.0;
              copy.alpha = 1.0;

              gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (eprop_color->cbutton), &copy);
            }
          else
            {
              GdkRGBA black = { 0, };

              /* Manually fill it with black for an NULL value.
               */
              if (gdk_rgba_parse (&black, "Black"))
                gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (eprop_color->cbutton), &black);
            }
        }
      else if (pspec->value_type == PANGO_TYPE_COLOR)
        {
          if ((pango_color = g_value_get_boxed (glade_property_inline_value (property))) != NULL)
            {
              GdkRGBA copy;

              copy.red   = pango_color->red   / 65535.0;
              copy.green = pango_color->green / 65535.0;
              copy.blue  = pango_color->blue  / 65535.0;
              copy.alpha = 1.0;

              gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (eprop_color->cbutton), &copy);
            }
          else
            {
              GdkRGBA black = { 0, };

              /* Manually fill it with black for an NULL value.
               */
              if (gdk_rgba_parse (&black, "Black"))
                gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (eprop_color->cbutton), &black);
            }
        }
      else if (pspec->value_type == GDK_TYPE_RGBA)
        {
          if ((rgba = g_value_get_boxed (glade_property_inline_value (property))) != NULL)
              gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (eprop_color->cbutton), rgba);
          else
            {
              GdkRGBA black = { 0, };

              /* Manually fill it with black for an NULL value.
               */
              if (gdk_rgba_parse (&black, "Black"))
                gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (eprop_color->cbutton), &black);
            }
        }
    }
}

static void
glade_eprop_color_changed (GtkWidget *button, GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GdkRGBA rgba = { 0, };
  GValue value = { 0, };
  GParamSpec *pspec;

  if (priv->loading)
    return;

  pspec = glade_property_def_get_pspec (priv->property_def);
  g_value_init (&value, pspec->value_type);

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &rgba);

  if (pspec->value_type == GDK_TYPE_COLOR)
    {
      GdkColor color = { 0, };

      color.red   = (gint16) (rgba.red * 65535);
      color.green = (gint16) (rgba.green * 65535);
      color.blue  = (gint16) (rgba.blue * 65535);

      g_value_set_boxed (&value, &color);
    }
  else if (pspec->value_type == PANGO_TYPE_COLOR)
    {
      PangoColor color = { 0, };

      color.red   = (gint16) (rgba.red * 65535);
      color.green = (gint16) (rgba.green * 65535);
      color.blue  = (gint16) (rgba.blue * 65535);

      g_value_set_boxed (&value, &color);
    }
  else if (pspec->value_type == GDK_TYPE_RGBA)
    g_value_set_boxed (&value, &rgba);

  glade_editor_property_commit (eprop, &value);
  g_value_unset (&value);
}

static GtkWidget *
glade_eprop_color_create_input (GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GladeEPropColor *eprop_color = GLADE_EPROP_COLOR (eprop);
  GtkWidget *hbox;
  GParamSpec *pspec;

  pspec  = glade_property_def_get_pspec (priv->property_def);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_halign (hbox, GTK_ALIGN_START);
  gtk_widget_set_valign (hbox, GTK_ALIGN_CENTER);
  
  eprop_color->entry = gtk_entry_new ();
  gtk_widget_set_hexpand (eprop_color->entry, TRUE);
  gtk_editable_set_editable (GTK_EDITABLE (eprop_color->entry), FALSE);
  gtk_widget_show (eprop_color->entry);
  gtk_box_pack_start (GTK_BOX (hbox), eprop_color->entry, TRUE, TRUE, 0);

  eprop_color->cbutton = gtk_color_button_new ();
  gtk_widget_show (eprop_color->cbutton);
  gtk_box_pack_start (GTK_BOX (hbox), eprop_color->cbutton, FALSE, FALSE, 0);

  if (pspec->value_type == GDK_TYPE_RGBA)
    gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (eprop_color->cbutton), TRUE);
  else
    gtk_color_chooser_set_use_alpha (GTK_COLOR_CHOOSER (eprop_color->cbutton), FALSE);

  g_signal_connect (G_OBJECT (eprop_color->cbutton), "color-set",
                    G_CALLBACK (glade_eprop_color_changed), eprop);

  return hbox;
}

/*******************************************************************************
                        GladeEditorPropertyNamedIconClass
 *******************************************************************************/
struct _GladeEPropNamedIcon
{
  GladeEditorProperty parent_instance;

  GtkWidget *entry;
  gchar *current_context;
};

GLADE_MAKE_EPROP (GladeEPropNamedIcon, glade_eprop_named_icon, GLADE, EPROP_NAMED_ICON)

static void
glade_eprop_named_icon_finalize (GObject *object)
{
  /* Chain up */
  G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_named_icon_load (GladeEditorProperty *eprop,
                             GladeProperty       *property)
{
  GladeEPropNamedIcon *eprop_named_icon = GLADE_EPROP_NAMED_ICON (eprop);
  GtkEntry *entry;
  gchar *text;

  /* Chain up first */
  editor_property_class->load (eprop, property);

  if (property == NULL)
    return;

  entry = GTK_ENTRY (eprop_named_icon->entry);
  text = glade_property_make_string (property);

  gtk_entry_set_text (entry, text ? text : "");

  g_free (text);
}

static void
glade_eprop_named_icon_changed_common (GladeEditorProperty *eprop,
                                       const gchar         *text,
                                       gboolean             use_command)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GValue *val;
  gchar *prop_text;

  val = g_new0 (GValue, 1);

  g_value_init (val, G_TYPE_STRING);

  glade_property_get (priv->property, &prop_text);

  /* Here we try not to modify the project state by not 
   * modifying a null value for an unchanged property.
   */
  if (prop_text == NULL && text && *text == '\0')
    g_value_set_string (val, NULL);
  else if (text == NULL && prop_text && *prop_text == '\0')
    g_value_set_string (val, "");
  else
    g_value_set_string (val, text);

  glade_editor_property_commit (eprop, val);
  g_value_unset (val);
  g_free (val);
}

static void
glade_eprop_named_icon_changed (GtkWidget *entry, GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  gchar *text;

  if (priv->loading)
    return;

  text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
  glade_eprop_named_icon_changed_common (eprop, text, priv->use_command);

  g_free (text);
}

static gboolean
glade_eprop_named_icon_focus_out (GtkWidget *entry,
                                  GdkEventFocus *event,
                                  GladeEditorProperty *eprop)
{
  glade_eprop_named_icon_changed (entry, eprop);
  return FALSE;
}

static void
glade_eprop_named_icon_activate (GtkEntry *entry, GladeEPropNamedIcon *eprop)
{
  glade_eprop_named_icon_changed (GTK_WIDGET (entry),
                                  GLADE_EDITOR_PROPERTY (eprop));
}

static void
chooser_response (GladeNamedIconChooserDialog *dialog,
                  gint                         response_id,
                  GladeEPropNamedIcon         *eprop)
{
  gchar *icon_name;

  switch (response_id)
    {

      case GTK_RESPONSE_OK:

        g_free (eprop->current_context);
        eprop->current_context =
            glade_named_icon_chooser_dialog_get_context (dialog);
        icon_name = glade_named_icon_chooser_dialog_get_icon_name (dialog);

        gtk_entry_set_text (GTK_ENTRY (eprop->entry), icon_name);
        gtk_widget_destroy (GTK_WIDGET (dialog));

        g_free (icon_name);

        glade_eprop_named_icon_changed (eprop->entry,
                                        GLADE_EDITOR_PROPERTY (eprop));

        break;

      case GTK_RESPONSE_CANCEL:

        gtk_widget_destroy (GTK_WIDGET (dialog));
        break;

      case GTK_RESPONSE_HELP:

        break;

      case GTK_RESPONSE_DELETE_EVENT:
        gtk_widget_destroy (GTK_WIDGET (dialog));
    }
}

static void
glade_eprop_named_icon_show_chooser_dialog (GladeEditorProperty *eprop)
{
  GtkWidget *dialog;

  dialog = glade_named_icon_chooser_dialog_new (_("Select Named Icon"),
                                                GTK_WINDOW
                                                (gtk_widget_get_toplevel
                                                 (GTK_WIDGET (eprop))),
                                                _("_Cancel"), GTK_RESPONSE_CANCEL,
                                                _("_OK"), GTK_RESPONSE_OK,
                                                NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  glade_named_icon_chooser_dialog_set_context (GLADE_NAMED_ICON_CHOOSER_DIALOG
                                               (dialog),
                                               GLADE_EPROP_NAMED_ICON (eprop)->
                                               current_context);

  glade_named_icon_chooser_dialog_set_icon_name (GLADE_NAMED_ICON_CHOOSER_DIALOG
                                                 (dialog),
                                                 gtk_entry_get_text (GTK_ENTRY
                                                                     (GLADE_EPROP_NAMED_ICON
                                                                      (eprop)->
                                                                      entry)));


  g_signal_connect (dialog, "response", G_CALLBACK (chooser_response), eprop);

  gtk_widget_show (dialog);

}

static GtkWidget *
glade_eprop_named_icon_create_input (GladeEditorProperty *eprop)
{
  GladeEPropNamedIcon *eprop_named_icon = GLADE_EPROP_NAMED_ICON (eprop);
  
  eprop_named_icon->entry = gtk_entry_new ();
  gtk_widget_set_hexpand (eprop_named_icon->entry, TRUE);
  gtk_widget_set_valign (eprop_named_icon->entry, GTK_ALIGN_CENTER);
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (eprop_named_icon->entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     "gtk-edit");

  eprop_named_icon->current_context = NULL;

  g_signal_connect (G_OBJECT (eprop_named_icon->entry), "activate",
                    G_CALLBACK (glade_eprop_named_icon_activate), eprop);

  g_signal_connect (G_OBJECT (eprop_named_icon->entry), "focus-out-event",
                    G_CALLBACK (glade_eprop_named_icon_focus_out), eprop);

  g_signal_connect_swapped (eprop_named_icon->entry, "icon-release",
                            G_CALLBACK (glade_eprop_named_icon_show_chooser_dialog),
                            eprop);

  return eprop_named_icon->entry;
}



/*******************************************************************************
                        GladeEditorPropertyTextClass
 *******************************************************************************/
struct _GladeEPropText
{
  GladeEditorProperty parent_instance;

  GtkWidget *text_entry;
  GtkTreeModel *store;
};

GLADE_MAKE_EPROP (GladeEPropText, glade_eprop_text, GLADE, EPROP_TEXT)

static void
glade_eprop_text_finalize (GObject *object)
{
  /* Chain up */
  G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static gchar *
text_buffer_get_text (GtkTextBuffer *buffer)
{
  GtkTextIter start, end;
  gchar *retval;
  
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  retval = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

  if (retval && *retval == '\0')
    {
      g_free (retval);
      return NULL;
    }

  return retval;
}

static void
glade_eprop_text_load (GladeEditorProperty *eprop, GladeProperty *property)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GladeEPropText *eprop_text = GLADE_EPROP_TEXT (eprop);
  GParamSpec *pspec;

  /* Chain up first */
  editor_property_class->load (eprop, property);

  if (property == NULL)
    return;

  pspec = glade_property_def_get_pspec (priv->property_def);

  if (GTK_IS_COMBO_BOX (eprop_text->text_entry))
    {
      if (gtk_combo_box_get_has_entry (GTK_COMBO_BOX (eprop_text->text_entry)))
        {
          GtkWidget *entry = gtk_bin_get_child (GTK_BIN (eprop_text->text_entry));
          gchar *text = glade_property_make_string (property);

          gtk_entry_set_text (GTK_ENTRY (entry), text ? text : "");
          g_free (text);
        }
      else
        {
          gchar *text = glade_property_make_string (property);
          gint value = text ?
              glade_utils_enum_value_from_string (GLADE_TYPE_STOCK, text) : 0;

          /* Set active iter... */
          gtk_combo_box_set_active (GTK_COMBO_BOX (eprop_text->text_entry),
                                    value);
          g_free (text);
        }
    }
  else if (GTK_IS_ENTRY (eprop_text->text_entry))
    {
      GtkEntry *entry = GTK_ENTRY (eprop_text->text_entry);
      gchar *text = glade_property_make_string (property);

      gtk_entry_set_text (entry, text ? text : "");
      g_free (text);
    }
  else if (GTK_IS_TEXT_VIEW (eprop_text->text_entry))
    {
      GtkTextBuffer *buffer;
      GType value_array_type;

      /* Deprecated GValueArray */
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      value_array_type = G_TYPE_VALUE_ARRAY;
      G_GNUC_END_IGNORE_DEPRECATIONS;

      buffer =
          gtk_text_view_get_buffer (GTK_TEXT_VIEW (eprop_text->text_entry));

      if (pspec->value_type == G_TYPE_STRV ||
          pspec->value_type == value_array_type)
        {
          GladePropertyDef *pclass = glade_property_get_def (property);
          gchar *text = glade_widget_adaptor_string_from_value
            (glade_property_def_get_adaptor (pclass),
             pclass, glade_property_inline_value (property));
          gchar *old_text = text_buffer_get_text (buffer);

          /* Only update it if necessary, see notes bellow */
          if (g_strcmp0 (text, old_text))
            gtk_text_buffer_set_text (buffer, text ? text : "", -1);

          g_free (text);
        }
      else
        {
          gchar *text = glade_property_make_string (property);
          gchar *old_text = text_buffer_get_text (buffer);

          /* NOTE: GtkTextBuffer does not like to be updated from a "changed"
           * signal callback. It prints a iterator warning and moves the cursor
           * to the end.
           */
          if (g_strcmp0 (text, old_text))
            gtk_text_buffer_set_text (buffer, text ? text : "", -1);

          g_free (old_text);
          g_free (text);
        }
    }
  else
    {
      g_warning ("BUG! Invalid Text Widget type.");
    }
}

static void
glade_eprop_text_changed_common (GladeEditorProperty *eprop,
                                 const gchar         *text,
                                 gboolean             use_command)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GValue *val;
  GParamSpec *pspec;
  gchar *prop_text;
  GType value_array_type;

  /* Deprecated GValueArray */
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  value_array_type = G_TYPE_VALUE_ARRAY;
  G_GNUC_END_IGNORE_DEPRECATIONS;

  pspec = glade_property_def_get_pspec (priv->property_def);

  if (pspec->value_type == G_TYPE_STRV ||
      pspec->value_type == value_array_type ||
      pspec->value_type == GDK_TYPE_PIXBUF ||
      pspec->value_type == G_TYPE_FILE ||
      pspec->value_type == G_TYPE_VARIANT)
    {
      GladeWidget *gwidget = glade_property_get_widget (priv->property);

      val = glade_property_def_make_gvalue_from_string (priv->property_def, 
                                                          text,
                                                          glade_widget_get_project (gwidget));
    }
  else
    {
      val = g_new0 (GValue, 1);

      g_value_init (val, G_TYPE_STRING);

      glade_property_get (priv->property, &prop_text);

      /* Here we try not to modify the project state by not 
       * modifying a null value for an unchanged property.
       */
      if (prop_text == NULL && text && *text == '\0')
        g_value_set_string (val, NULL);
      else if (text == NULL && prop_text && *prop_text == '\0')
        g_value_set_string (val, "");
      else
        g_value_set_string (val, text);
    }

  glade_editor_property_commit_no_callback (eprop, val);
  g_value_unset (val);
  g_free (val);
}

static void
glade_eprop_text_changed (GtkWidget *entry, GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  gchar *text;

  if (priv->loading)
    return;

  text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
  glade_eprop_text_changed_common (eprop, text, priv->use_command);

  g_free (text);
}

static void
glade_eprop_text_buffer_changed (GtkTextBuffer *buffer,
                                 GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  gchar *text;

  if (priv->loading)
    return;

  text = text_buffer_get_text (buffer);
  glade_eprop_text_changed_common (eprop, text, priv->use_command);
  g_free (text);
}

/**
 * glade_editor_property_show_i18n_dialog:
 * @parent: The parent widget for the dialog.
 * @text: A read/write pointer to the text property
 * @context: A read/write pointer to the translation context
 * @comment: A read/write pointer to the translator comment
 * @translatable: A read/write pointer to the translatable setting]
 *
 * Runs a dialog and updates the provided values.
 *
 * Returns: %TRUE if OK was selected.
 */
gboolean
glade_editor_property_show_i18n_dialog (GtkWidget *parent,
                                        gchar    **text,
                                        gchar    **context,
                                        gchar    **comment,
                                        gboolean  *translatable)
{
  GtkWidget *dialog;
  GtkWidget *vbox, *hbox;
  GtkWidget *label;
  GtkWidget *sw;
  GtkWidget *alignment;
  GtkWidget *text_view, *comment_view, *context_view;
  GtkTextBuffer *text_buffer, *comment_buffer, *context_buffer = NULL;
  GtkWidget *translatable_button;
  GtkWidget *content_area;
  gint res;

  g_return_val_if_fail (text && context && comment && translatable, FALSE);

  dialog = gtk_dialog_new_with_buttons (_("Edit Text"),
                                        parent ?
                                        GTK_WINDOW (gtk_widget_get_toplevel
                                                    (parent)) : NULL,
                                        GTK_DIALOG_MODAL,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_OK"), GTK_RESPONSE_OK, NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  _glade_util_dialog_set_hig (GTK_DIALOG (dialog));
  content_area =  gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
  gtk_widget_show (vbox);

  gtk_box_pack_start (GTK_BOX (content_area), vbox, TRUE, TRUE, 0);

  /* Text */
  label = gtk_label_new_with_mnemonic (_("_Text:"));
  gtk_widget_show (label);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (sw);
  gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
  gtk_widget_set_size_request (sw, 400, 200);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);

  text_view = gtk_text_view_new ();
  gtk_scrollable_set_hscroll_policy (GTK_SCROLLABLE (text_view), GTK_SCROLL_MINIMUM);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view), GTK_WRAP_WORD);
  gtk_widget_show (text_view);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label), text_view);

  gtk_container_add (GTK_CONTAINER (sw), text_view);

  text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));

  if (*text)
    {
      gtk_text_buffer_set_text (text_buffer, *text, -1);
    }

  /* Translatable and context prefix. */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_show (hbox);

  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  /* Translatable */
  translatable_button = gtk_check_button_new_with_mnemonic (_("T_ranslatable"));
  gtk_widget_show (translatable_button);
  gtk_box_pack_start (GTK_BOX (hbox), translatable_button, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (translatable_button),
                                *translatable);
  gtk_widget_set_tooltip_text (translatable_button,
                               _("Whether this property is translatable"));


  /* Context. */
  alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 12, 0, 0, 0);
  gtk_widget_show (alignment);

  label = gtk_label_new_with_mnemonic (_("Conte_xt for translation:"));
  gtk_widget_show (label);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (alignment), label);
  gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text (alignment,
                               _("For short and ambiguous strings: type a word here to differentiate "
                                 "the meaning of this string from the meaning of other occurrences of "
                                 "the same string"));

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (sw);
  gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);

  context_view = gtk_text_view_new ();
  gtk_scrollable_set_hscroll_policy (GTK_SCROLLABLE (context_view), GTK_SCROLL_MINIMUM);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (context_view), GTK_WRAP_WORD);
  gtk_widget_show (context_view);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label), context_view);

  gtk_container_add (GTK_CONTAINER (sw), context_view);

  context_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (context_view));

  if (*context)
    {
      gtk_text_buffer_set_text (context_buffer, *context, -1);
    }

  /* Comments. */
  alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 12, 0, 0, 0);
  gtk_widget_show (alignment);

  label = gtk_label_new_with_mnemonic (_("Co_mments for translators:"));
  gtk_widget_show (label);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (alignment), label);
  gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (sw);
  gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);

  comment_view = gtk_text_view_new ();
  gtk_scrollable_set_hscroll_policy (GTK_SCROLLABLE (comment_view), GTK_SCROLL_MINIMUM);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (comment_view), GTK_WRAP_WORD);
  gtk_widget_show (comment_view);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label), comment_view);

  gtk_container_add (GTK_CONTAINER (sw), comment_view);

  comment_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (comment_view));

  if (*comment)
    {
      gtk_text_buffer_set_text (comment_buffer, *comment, -1);
    }

  res = gtk_dialog_run (GTK_DIALOG (dialog));
  if (res == GTK_RESPONSE_OK)
    {
      g_free ((gpointer) * text);
      g_free ((gpointer) * context);
      g_free ((gpointer) * comment);

      /* Get the new values for translatable */
      *translatable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                                    (translatable_button));

      /* Comment, text and context */
      *comment = text_buffer_get_text (comment_buffer);
      *text = text_buffer_get_text (text_buffer);
      *context = text_buffer_get_text (context_buffer);

      gtk_widget_destroy (dialog);
      return TRUE;
    }

  gtk_widget_destroy (dialog);
  return FALSE;
}

static void
glade_eprop_text_show_i18n_dialog (GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  gchar *text = glade_property_make_string (priv->property);
  gchar *context = g_strdup (glade_property_i18n_get_context (priv->property));
  gchar *comment = g_strdup (glade_property_i18n_get_comment (priv->property));
  gboolean translatable = glade_property_i18n_get_translatable (priv->property);

  if (glade_editor_property_show_i18n_dialog
      (GTK_WIDGET (eprop), &text, &context, &comment, &translatable))
    {
      glade_command_set_i18n (priv->property, translatable, context, comment);
      glade_eprop_text_changed_common (eprop, text, priv->use_command);

      glade_editor_property_load (eprop, priv->property);
    }

  g_free (text);
  g_free (context);
  g_free (comment);
}

gboolean
glade_editor_property_show_resource_dialog (GladeProject *project,
                                            GtkWidget    *parent,
                                            gchar      **filename)
{
  GFile *resource_folder;
  GtkWidget *dialog;
  gchar *folder;

  g_return_val_if_fail (filename != NULL, FALSE);

  *filename = NULL;

  dialog =
      gtk_file_chooser_dialog_new (_
                                   ("Select a file from the project resource directory"),
                                   parent ?
                                   GTK_WINDOW (gtk_widget_get_toplevel (parent))
                                   : NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
                                   _("_Cancel"), GTK_RESPONSE_CANCEL,
                                   _("_Open"), GTK_RESPONSE_OK, NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  _glade_util_dialog_set_hig (GTK_DIALOG (dialog));

  folder = glade_project_resource_fullpath (project, "");
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), folder);
  resource_folder = g_file_new_for_path (folder);
  g_free (folder);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      GFile *file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
      *filename = _glade_util_file_get_relative_path (resource_folder, file);
      g_object_unref (file);
    }

  gtk_widget_destroy (dialog);
  g_object_unref (resource_folder);

  return *filename != NULL;
}

static void
glade_eprop_text_show_resource_dialog (GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GladeWidget  *widget  = glade_property_get_widget (priv->property);
  GladeProject *project = glade_widget_get_project (widget);
  gchar *text = NULL;

  if (glade_editor_property_show_resource_dialog (project, GTK_WIDGET (eprop), &text))
    {
      GParamSpec *pspec = glade_property_def_get_pspec (priv->property_def);

      if (G_PARAM_SPEC_VALUE_TYPE (pspec) == G_TYPE_FILE)
        {
          gchar *path = text;
          text = g_strconcat ("file://", path, NULL);
          g_free (path);
        }

      glade_eprop_text_changed_common (eprop, text, priv->use_command);

      glade_editor_property_load (eprop, priv->property);

      g_free (text);
    }
}

enum
{
  COMBO_COLUMN_TEXT = 0,
  COMBO_COLUMN_PIXBUF,
  COMBO_LAST_COLUMN
};

static GtkListStore *
glade_eprop_text_create_store (GType enum_type)
{
  GtkListStore *store;
  GtkTreeIter iter;
  GEnumClass *eclass;
  guint i;

  eclass = g_type_class_ref (enum_type);

  store = gtk_list_store_new (COMBO_LAST_COLUMN, G_TYPE_STRING, G_TYPE_STRING);

  for (i = 0; i < eclass->n_values; i++)
    {
      const gchar *displayable =
          glade_get_displayable_value (enum_type, eclass->values[i].value_nick);
      if (!displayable)
        displayable = eclass->values[i].value_nick;

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          COMBO_COLUMN_TEXT, displayable,
                          COMBO_COLUMN_PIXBUF, eclass->values[i].value_nick,
                          -1);
    }

  g_type_class_unref (eclass);

  return store;
}

static void
eprop_text_stock_changed (GtkComboBox *combo, GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GladeEPropText *eprop_text = GLADE_EPROP_TEXT (eprop);
  GtkTreeIter iter;
  gchar *text = NULL;
  const gchar *str;

  if (priv->loading)
    return;

  if (gtk_combo_box_get_active_iter (combo, &iter))
    {
      gtk_tree_model_get (GTK_TREE_MODEL (eprop_text->store), &iter,
                          COMBO_COLUMN_PIXBUF, &text, -1);
      glade_eprop_text_changed_common (eprop, text, priv->use_command);
      g_free (text);
    }
  else if (gtk_combo_box_get_has_entry (combo))
    {
      str =
          gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (combo))));
      glade_eprop_text_changed_common (eprop, str, priv->use_command);
    }
}

static gint
get_text_view_height (void)
{
  static gint height = -1;

  if (height < 0)
    {
      GtkWidget *label = gtk_label_new (NULL);
      PangoLayout *layout =
        gtk_widget_create_pango_layout (label, 
                                        "The quick\n"
                                        "brown fox\n"
                                        "jumped over\n"
                                        "the lazy dog");

      pango_layout_get_pixel_size (layout, NULL, &height);

      g_object_unref (layout);
      g_object_ref_sink (label);
      g_object_unref (label);
    }

  return height;
}

static GtkWidget *
glade_eprop_text_create_input (GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GladeEPropText *eprop_text = GLADE_EPROP_TEXT (eprop);
  GladePropertyDef *property_def;
  GParamSpec *pspec;
  GtkWidget *hbox;
  GType value_array_type;

  /* Deprecated GValueArray */
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  value_array_type = G_TYPE_VALUE_ARRAY;
  G_GNUC_END_IGNORE_DEPRECATIONS;

  property_def = priv->property_def;
  pspec = glade_property_def_get_pspec (property_def);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  if (glade_property_def_stock (property_def) || 
      glade_property_def_stock_icon (property_def))
    {
      GtkCellRenderer *renderer;
      GtkWidget *child;
      GtkWidget *combo = gtk_combo_box_new_with_entry ();

      gtk_widget_set_halign (hbox, GTK_ALIGN_START);
      gtk_widget_set_valign (hbox, GTK_ALIGN_CENTER);
      gtk_widget_set_hexpand (combo, TRUE);
      glade_util_remove_scroll_events (combo);
      
      eprop_text->store = (GtkTreeModel *)
          glade_eprop_text_create_store (glade_property_def_stock (property_def) ? 
                                         GLADE_TYPE_STOCK : GLADE_TYPE_STOCK_IMAGE);

      gtk_combo_box_set_model (GTK_COMBO_BOX (combo),
                               GTK_TREE_MODEL (eprop_text->store));

      /* let the comboboxentry prepend its intrusive cell first... */
      gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (combo),
                                           COMBO_COLUMN_TEXT);

      renderer = gtk_cell_renderer_pixbuf_new ();
      gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, FALSE);
      gtk_cell_layout_reorder (GTK_CELL_LAYOUT (combo), renderer, 0);
      gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
                                      "stock-id", COMBO_COLUMN_PIXBUF, NULL);

      /* Dont allow custom items where an actual GTK+ stock item is expected
       * (i.e. real items come with labels) */
      child = gtk_bin_get_child (GTK_BIN (combo));
      if (glade_property_def_stock (property_def))
        gtk_editable_set_editable (GTK_EDITABLE (child), FALSE);
      else
        gtk_editable_set_editable (GTK_EDITABLE (child), TRUE);

      gtk_widget_show (combo);
      gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
      g_signal_connect (G_OBJECT (combo), "changed",
                        G_CALLBACK (eprop_text_stock_changed), eprop);


      eprop_text->text_entry = combo;
    }
  else if (glade_property_def_multiline (property_def) ||
           pspec->value_type == G_TYPE_STRV ||
           pspec->value_type == value_array_type)
    {
      GtkWidget *swindow;
      GtkTextBuffer *buffer;
      gint min_height = get_text_view_height ();

      swindow = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (swindow), min_height);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_AUTOMATIC);
      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow),
                                           GTK_SHADOW_IN);
      glade_util_remove_scroll_events (swindow);

      eprop_text->text_entry = gtk_text_view_new ();
      gtk_scrollable_set_hscroll_policy (GTK_SCROLLABLE (eprop_text->text_entry), GTK_SCROLL_MINIMUM);
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (eprop_text->text_entry), GTK_WRAP_WORD);
      buffer =
          gtk_text_view_get_buffer (GTK_TEXT_VIEW (eprop_text->text_entry));

      gtk_container_add (GTK_CONTAINER (swindow), eprop_text->text_entry);
      gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (swindow), TRUE, TRUE, 0);

      gtk_widget_show_all (swindow);

      gtk_widget_set_hexpand (swindow, TRUE);

      g_signal_connect (G_OBJECT (buffer), "changed",
                        G_CALLBACK (glade_eprop_text_buffer_changed), eprop);

    }
  else
    {
      eprop_text->text_entry = gtk_entry_new ();
      gtk_widget_set_hexpand (eprop_text->text_entry, TRUE);
      gtk_widget_show (eprop_text->text_entry);

      gtk_box_pack_start (GTK_BOX (hbox), eprop_text->text_entry, TRUE, TRUE, 0);

      g_signal_connect (G_OBJECT (eprop_text->text_entry), "changed",
                        G_CALLBACK (glade_eprop_text_changed), eprop);

      if (pspec->value_type == GDK_TYPE_PIXBUF ||
          pspec->value_type == G_TYPE_FILE)
        {
          gtk_entry_set_icon_from_icon_name (GTK_ENTRY (eprop_text->text_entry),
                                             GTK_ENTRY_ICON_SECONDARY,
                                             "document-open");

          g_signal_connect_swapped (eprop_text->text_entry, "icon-release",
                                    G_CALLBACK (glade_eprop_text_show_resource_dialog),
                                    eprop);
        }
    }

  if (glade_property_def_translatable (property_def))
    {
      if (GTK_IS_ENTRY (eprop_text->text_entry))
        {
          gtk_entry_set_icon_from_icon_name (GTK_ENTRY (eprop_text->text_entry),
                                             GTK_ENTRY_ICON_SECONDARY,
                                             "gtk-edit");
          g_signal_connect_swapped (eprop_text->text_entry, "icon-release",
                                    G_CALLBACK (glade_eprop_text_show_i18n_dialog),
                                    eprop);
        }
      else
        {
          GtkWidget *button = gtk_button_new ();
          gtk_button_set_image (GTK_BUTTON (button),
                                gtk_image_new_from_icon_name ("gtk-edit", GTK_ICON_SIZE_MENU));
          gtk_widget_show (button);
          gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
          g_signal_connect_swapped (button, "clicked",
                                    G_CALLBACK (glade_eprop_text_show_i18n_dialog),
                                    eprop);
        }
    }
  return hbox;
}

/*******************************************************************************
                        GladeEditorPropertyBoolClass
 *******************************************************************************/
struct _GladeEPropBool
{
  GladeEditorProperty parent_instance;

  GtkWidget *button;
};

GLADE_MAKE_EPROP (GladeEPropBool, glade_eprop_bool, GLADE, EPROP_BOOL)

static void
glade_eprop_bool_finalize (GObject *object)
{
  /* Chain up */
  G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_bool_load (GladeEditorProperty *eprop, GladeProperty *property)
{
  /* Chain up first */
  editor_property_class->load (eprop, property);

  if (property)
    {
      GladeEPropBool *eprop_bool = GLADE_EPROP_BOOL (eprop);
      gboolean state = g_value_get_boolean (glade_property_inline_value (property));
      gtk_switch_set_active (GTK_SWITCH (eprop_bool->button), state);
    }
}

static void
glade_eprop_bool_active_notify (GObject             *gobject,
                                GParamSpec          *pspec,
                                GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GValue val = { 0, };

  if (priv->loading)
    return;

  g_value_init (&val, G_TYPE_BOOLEAN);
  g_value_set_boolean (&val, gtk_switch_get_active (GTK_SWITCH (gobject)));

  glade_editor_property_commit_no_callback (eprop, &val);

  g_value_unset (&val);
}

static GtkWidget *
glade_eprop_bool_create_input (GladeEditorProperty *eprop)
{
  GladeEPropBool *eprop_bool = GLADE_EPROP_BOOL (eprop);
  
  eprop_bool->button = gtk_switch_new ();
  gtk_widget_set_halign (eprop_bool->button, GTK_ALIGN_START);
  gtk_widget_set_valign (eprop_bool->button, GTK_ALIGN_CENTER);

  g_signal_connect (eprop_bool->button, "notify::active",
                    G_CALLBACK (glade_eprop_bool_active_notify), eprop);

  return eprop_bool->button;
}

/*******************************************************************************
                        GladeEditorPropertyCheckClass
 *******************************************************************************/
struct _GladeEPropCheck
{
  GladeEditorProperty parent_instance;

  GtkWidget *button;
};

GLADE_MAKE_EPROP (GladeEPropCheck, glade_eprop_check, GLADE, EPROP_CHECK)

static void
glade_eprop_check_finalize (GObject *object)
{
  /* Chain up */
  G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_check_load (GladeEditorProperty *eprop, GladeProperty *property)
{
  /* Chain up first */
  editor_property_class->load (eprop, property);

  if (property)
    {
      GladeEPropCheck *eprop_check = GLADE_EPROP_CHECK (eprop);
      gboolean state = g_value_get_boolean (glade_property_inline_value (property));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (eprop_check->button), state);
    }
}

static void
glade_eprop_check_active_notify (GObject             *gobject,
                                 GParamSpec          *pspec,
                                 GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GValue val = { 0, };

  if (priv->loading)
    return;

  g_value_init (&val, G_TYPE_BOOLEAN);
  g_value_set_boolean (&val, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gobject)));

  glade_editor_property_commit_no_callback (eprop, &val);

  g_value_unset (&val);
}

static GtkWidget *
glade_eprop_check_create_input (GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GladeEPropCheck *eprop_check = GLADE_EPROP_CHECK (eprop);
  GladePropertyDef *pclass;
  GtkWidget *label;

  pclass = priv->property_def;

  /* Add the property label as the check button's child */
  label = glade_editor_property_get_item_label (eprop);

  glade_property_label_set_property_name (GLADE_PROPERTY_LABEL (label),
                                          glade_property_def_id (pclass));
  glade_property_label_set_packing (GLADE_PROPERTY_LABEL (label),
                                    glade_property_def_get_is_packing (pclass));
  glade_property_label_set_append_colon (GLADE_PROPERTY_LABEL (label), FALSE);
  glade_property_label_set_custom_text (GLADE_PROPERTY_LABEL (label),
                                        priv->custom_text);
  gtk_widget_show (label);
  
  eprop_check->button = gtk_check_button_new ();
  gtk_widget_set_focus_on_click (eprop_check->button, FALSE);

  gtk_container_add (GTK_CONTAINER (eprop_check->button), label);

  gtk_widget_set_halign (eprop_check->button, GTK_ALIGN_START);
  gtk_widget_set_valign (eprop_check->button, GTK_ALIGN_CENTER);

  g_signal_connect (eprop_check->button, "notify::active",
                    G_CALLBACK (glade_eprop_check_active_notify), eprop);

  return eprop_check->button;
}

/*******************************************************************************
                        GladeEditorPropertyUnicharClass
 *******************************************************************************/
struct _GladeEPropUnichar
{
  GladeEditorProperty parent_instance;

  GtkWidget *entry;
};

GLADE_MAKE_EPROP (GladeEPropUnichar, glade_eprop_unichar, GLADE, EPROP_UNICHAR)

static void
glade_eprop_unichar_finalize (GObject *object)
{
  /* Chain up */
  G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_unichar_load (GladeEditorProperty *eprop, GladeProperty *property)
{
  GladeEPropUnichar *eprop_unichar = GLADE_EPROP_UNICHAR (eprop);

  /* Chain up first */
  editor_property_class->load (eprop, property);

  if (property && GTK_IS_ENTRY (eprop_unichar->entry))
    {
      GtkEntry *entry = GTK_ENTRY (eprop_unichar->entry);
      gchar utf8st[8];
      gint n;

      if ((n = g_unichar_to_utf8 (g_value_get_uint (glade_property_inline_value (property)), utf8st)))
        {
          utf8st[n] = '\0';
          gtk_entry_set_text (entry, utf8st);
        }
    }
}


static void
glade_eprop_unichar_changed (GtkWidget *entry, GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  const gchar *text;

  if (priv->loading)
    return;

  if ((text = gtk_entry_get_text (GTK_ENTRY (entry))) != NULL)
    {
      gunichar unich = g_utf8_get_char (text);
      GValue val = { 0, };

      g_value_init (&val, G_TYPE_UINT);
      g_value_set_uint (&val, unich);

      glade_editor_property_commit_no_callback (eprop, &val);

      g_value_unset (&val);
    }
}

static void
glade_eprop_unichar_delete (GtkEditable         *editable,
                            gint                 start_pos,
                            gint                 end_pos,
                            GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  if (priv->loading)
    return;
  gtk_editable_select_region (editable, 0, -1);
  g_signal_stop_emission_by_name (G_OBJECT (editable), "delete_text");
}

static void
glade_eprop_unichar_insert (GtkWidget *entry,
                            const gchar *text,
                            gint length,
                            gint *position,
                            GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  if (priv->loading)
    return;
  g_signal_handlers_block_by_func
      (G_OBJECT (entry), G_CALLBACK (glade_eprop_unichar_changed), eprop);
  g_signal_handlers_block_by_func
      (G_OBJECT (entry), G_CALLBACK (glade_eprop_unichar_insert), eprop);
  g_signal_handlers_block_by_func
      (G_OBJECT (entry), G_CALLBACK (glade_eprop_unichar_delete), eprop);

  gtk_editable_delete_text (GTK_EDITABLE (entry), 0, -1);
  *position = 0;
  gtk_editable_insert_text (GTK_EDITABLE (entry), text, 1, position);

  g_signal_handlers_unblock_by_func
      (G_OBJECT (entry), G_CALLBACK (glade_eprop_unichar_changed), eprop);
  g_signal_handlers_unblock_by_func
      (G_OBJECT (entry), G_CALLBACK (glade_eprop_unichar_insert), eprop);
  g_signal_handlers_unblock_by_func
      (G_OBJECT (entry), G_CALLBACK (glade_eprop_unichar_delete), eprop);

  g_signal_stop_emission_by_name (G_OBJECT (entry), "insert_text");

  glade_eprop_unichar_changed (entry, eprop);
}

static GtkWidget *
glade_eprop_unichar_create_input (GladeEditorProperty *eprop)
{
  GladeEPropUnichar *eprop_unichar = GLADE_EPROP_UNICHAR (eprop);

  eprop_unichar->entry = gtk_entry_new ();
  gtk_widget_set_halign (eprop_unichar->entry, GTK_ALIGN_START);
  gtk_widget_set_valign (eprop_unichar->entry, GTK_ALIGN_CENTER);

  /* it's 2 to prevent spirious beeps... */
  gtk_entry_set_max_length (GTK_ENTRY (eprop_unichar->entry), 2);

  g_signal_connect (G_OBJECT (eprop_unichar->entry), "changed",
                    G_CALLBACK (glade_eprop_unichar_changed), eprop);
  g_signal_connect (G_OBJECT (eprop_unichar->entry), "insert_text",
                    G_CALLBACK (glade_eprop_unichar_insert), eprop);
  g_signal_connect (G_OBJECT (eprop_unichar->entry), "delete_text",
                    G_CALLBACK (glade_eprop_unichar_delete), eprop);
  return eprop_unichar->entry;
}

/*******************************************************************************
                        GladeEditorPropertyObjectClass
 *******************************************************************************/
enum
{
  OBJ_COLUMN_WIDGET = 0,
  OBJ_COLUMN_WIDGET_NAME,
  OBJ_COLUMN_WIDGET_CLASS,
  OBJ_COLUMN_SELECTED,
  OBJ_COLUMN_SELECTABLE,
  OBJ_NUM_COLUMNS
};

#define GLADE_RESPONSE_CLEAR  42
#define GLADE_RESPONSE_CREATE 43

struct _GladeEPropObject
{
  GladeEditorProperty parent_instance;

  GtkWidget *entry;
};

GLADE_MAKE_EPROP (GladeEPropObject, glade_eprop_object, GLADE, EPROP_OBJECT)

static void
glade_eprop_object_finalize (GObject *object)
{
  /* Chain up */
  G_OBJECT_CLASS (editor_property_class)->finalize (object);
}


static gchar *
glade_eprop_object_name (const gchar *name,
                         GtkTreeStore *model, GtkTreeIter *parent_iter)
{
  GtkTreePath *path;
  GString *string;
  gint i;

  string = g_string_new (name);

  if (parent_iter)
    {
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), parent_iter);
      for (i = 0; i < gtk_tree_path_get_depth (path); i++)
        g_string_prepend (string, "    ");
    }

  return g_string_free (string, FALSE);
}

static gboolean
search_list (GList * list, gpointer data)
{
  return g_list_find (list, data) != NULL;
}


/*
 * Note that widgets is a list of GtkWidgets, while what we store
 * in the model are the associated GladeWidgets.
 */
static void
glade_eprop_object_populate_view_real (GtkTreeStore *model,
                                       GtkTreeIter  *parent_iter,
                                       GList        *widgets,
                                       GList        *selected_widgets,
                                       GList        *exception_widgets,
                                       GType         object_type,
                                       gboolean      parentless)
{
  GList *children, *list;
  GtkTreeIter iter;
  gboolean good_type, has_decendant;

  for (list = widgets; list; list = list->next)
    {
      GladeWidget *widget;
      GladeWidgetAdaptor *adaptor;
      const gchar *widget_name;
      if ((widget = glade_widget_get_from_gobject (list->data)) != NULL)
        {
          adaptor = glade_widget_get_adaptor (widget);

          has_decendant = 
            !parentless && glade_widget_has_decendant (widget, object_type);

          good_type = (glade_widget_adaptor_get_object_type (adaptor) == object_type ||
                       g_type_is_a (glade_widget_adaptor_get_object_type (adaptor), object_type));

          widget_name = glade_widget_get_display_name (widget);
          if (parentless)
            good_type = good_type && !GWA_IS_TOPLEVEL (adaptor);

          if (good_type || has_decendant)
            {
              gchar *prop_name = glade_eprop_object_name (widget_name, model, parent_iter);
              gtk_tree_store_append (model, &iter, parent_iter);
              gtk_tree_store_set
                  (model, &iter,
                   OBJ_COLUMN_WIDGET, widget,
                   OBJ_COLUMN_WIDGET_NAME,
                   prop_name,
                   OBJ_COLUMN_WIDGET_CLASS, glade_widget_adaptor_get_title (adaptor),
                   /* Selectable if its a compatible type and
                    * its not itself.
                    */
                   OBJ_COLUMN_SELECTABLE,
                   good_type && !search_list (exception_widgets, widget),
                   OBJ_COLUMN_SELECTED,
                   good_type && search_list (selected_widgets, widget), -1);
              g_free (prop_name);
            }

          if (has_decendant &&
              (children = glade_widget_adaptor_get_children
               (adaptor, glade_widget_get_object (widget))) != NULL)
            {
              GtkTreeIter *copy = NULL;

              copy = gtk_tree_iter_copy (&iter);
              glade_eprop_object_populate_view_real (model, copy, children,
                                                     selected_widgets,
                                                     exception_widgets,
                                                     object_type, parentless);
              gtk_tree_iter_free (copy);

              g_list_free (children);
            }
        }
    }
}

static void
glade_eprop_object_populate_view (GladeProject *project,
                                  GtkTreeView  *view,
                                  GList        *selected,
                                  GList        *exceptions,
                                  GType         object_type,
                                  gboolean      parentless)
{
  GtkTreeStore *model = (GtkTreeStore *) gtk_tree_view_get_model (view);
  GList *list, *toplevels = NULL;

  /* Make a list of only the toplevel widgets */
  for (list = (GList *) glade_project_get_objects (project); list;
       list = list->next)
    {
      GObject *object = G_OBJECT (list->data);
      GladeWidget *gwidget = glade_widget_get_from_gobject (object);
      g_assert (gwidget);

      if (glade_widget_get_parent (gwidget) == NULL)
        toplevels = g_list_append (toplevels, object);
    }

  /* add the widgets and recurse */
  glade_eprop_object_populate_view_real (model, NULL, toplevels, selected,
                                         exceptions, object_type, parentless);
  g_list_free (toplevels);
}

static gboolean
glade_eprop_object_clear_iter (GtkTreeModel *model,
                               GtkTreePath  *path,
                               GtkTreeIter  *iter,
                               gpointer      data)
{
  gtk_tree_store_set (GTK_TREE_STORE (model), iter,
                      OBJ_COLUMN_SELECTED, FALSE, -1);
  return FALSE;
}

static gboolean
glade_eprop_object_selected_widget (GtkTreeModel *model,
                                    GtkTreePath  *path,
                                    GtkTreeIter  *iter,
                                    GladeWidget **ret)
{
  gboolean selected;
  GladeWidget *widget;

  gtk_tree_model_get (model, iter,
                      OBJ_COLUMN_SELECTED, &selected,
                      OBJ_COLUMN_WIDGET, &widget, -1);

  if (selected)
    {
      *ret = widget;
      return TRUE;
    }
  return FALSE;
}

static void
glade_eprop_object_selected (GtkCellRendererToggle *cell,
                             gchar                 *path_str,
                             GtkTreeModel          *model)
{
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  GtkTreeIter iter;
  gboolean enabled, radio;

  radio = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (model), "radio-list"));


  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, OBJ_COLUMN_SELECTED, &enabled, -1);

  /* Clear the rest of the view first
   */
  if (radio)
    gtk_tree_model_foreach (model, glade_eprop_object_clear_iter, NULL);

  gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
                      OBJ_COLUMN_SELECTED, radio ? TRUE : !enabled, -1);

  gtk_tree_path_free (path);
}

static GtkWidget *
glade_eprop_object_view (gboolean radio)
{
  GtkWidget *view_widget;
  GtkTreeModel *model;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  model = (GtkTreeModel *) gtk_tree_store_new (OBJ_NUM_COLUMNS, G_TYPE_OBJECT,  /* The GladeWidget  */
                                               G_TYPE_STRING,   /* The GladeWidget's name */
                                               G_TYPE_STRING,   /* The GladeWidgetAdaptor title */
                                               G_TYPE_BOOLEAN,  /* Whether this row is selected or not */
                                               G_TYPE_BOOLEAN); /* Whether this GladeWidget is 
                                                                 * of an acceptable type and 
                                                                 * therefore can be selected.
                                                                 */

  g_object_set_data (G_OBJECT (model), "radio-list", GINT_TO_POINTER (radio));

  view_widget = gtk_tree_view_new_with_model (model);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view_widget), FALSE);

  /* Pass ownership to the view */
  g_object_unref (G_OBJECT (model));
  g_object_set (G_OBJECT (view_widget), "enable-search", FALSE, NULL);

        /********************* fake invisible column *********************/
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), "editable", FALSE, "visible", FALSE, NULL);

  column = gtk_tree_view_column_new_with_attributes (NULL, renderer, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

  gtk_tree_view_column_set_visible (column, FALSE);
  gtk_tree_view_set_expander_column (GTK_TREE_VIEW (view_widget), column);

        /************************ selected column ************************/
  renderer = gtk_cell_renderer_toggle_new ();
  g_object_set (G_OBJECT (renderer),
                "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
                "activatable", TRUE, "radio", radio, NULL);
  g_signal_connect (renderer, "toggled",
                    G_CALLBACK (glade_eprop_object_selected), model);
  gtk_tree_view_insert_column_with_attributes
      (GTK_TREE_VIEW (view_widget), 0,
       NULL, renderer,
       "visible", OBJ_COLUMN_SELECTABLE,
       "sensitive", OBJ_COLUMN_SELECTABLE, "active", OBJ_COLUMN_SELECTED, NULL);

        /********************* widget name column *********************/
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);
  gtk_tree_view_insert_column_with_attributes
      (GTK_TREE_VIEW (view_widget), 1,
       _("Name"), renderer, "text", OBJ_COLUMN_WIDGET_NAME, NULL);

        /***************** widget class title column ******************/
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer),
                "editable", FALSE,
                "style", PANGO_STYLE_ITALIC, "foreground", "Gray", NULL);
  gtk_tree_view_insert_column_with_attributes
      (GTK_TREE_VIEW (view_widget), 2,
       _("Class"), renderer, "text", OBJ_COLUMN_WIDGET_CLASS, NULL);

  return view_widget;
}


static gchar *
glade_eprop_object_dialog_title (GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  gboolean parentless;
  GParamSpec *pspec;

  parentless = glade_property_def_parentless_widget (priv->property_def);
  pspec = glade_property_def_get_pspec (priv->property_def);

  if (GLADE_IS_PARAM_SPEC_OBJECTS (pspec))
    {
      const gchar *typename = g_type_name (glade_param_spec_objects_get_type (GLADE_PARAM_SPEC_OBJECTS (pspec)));

      if (parentless)
        return g_strdup_printf (_("Choose parentless %s type objects in this project"), typename);
      else
        return g_strdup_printf (_("Choose %s type objects in this project"), typename);
    }
  else
    {
      GladeWidgetAdaptor *adaptor;
      const gchar *title;

      adaptor = glade_widget_adaptor_get_by_type (pspec->value_type);

      if (adaptor != NULL)
        title = glade_widget_adaptor_get_title (adaptor);
      else
        {
          /* Fallback on type name (which would look like "GtkButton"
           * instead of "Button" and maybe not translated).
           */
          title = g_type_name (pspec->value_type);
        }

      if (parentless)
        return g_strdup_printf (_("Choose a parentless %s in this project"), title);
      else
        return g_strdup_printf (_("Choose a %s in this project"), title);
    }
}

gboolean
glade_editor_property_show_object_dialog (GladeProject *project,
                                          const gchar  *title,
                                          GtkWidget    *parent,
                                          GType         object_type,
                                          GladeWidget  *exception,
                                          GladeWidget **object)
{
  GtkWidget *dialog;
  GtkWidget *vbox, *label, *sw;
  GtkWidget *tree_view;
  GtkWidget *content_area;
  GList *selected_list = NULL, *exception_list = NULL;
  gint res;

  g_return_val_if_fail (object != NULL, -1);

  if (!parent)
    parent = glade_app_get_window ();

  dialog = gtk_dialog_new_with_buttons (title,
                                        GTK_WINDOW (parent),
                                        GTK_DIALOG_MODAL,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("C_lear"), GLADE_RESPONSE_CLEAR,
                                        _("_OK"), GTK_RESPONSE_OK, NULL);

  gtk_window_set_default_size (GTK_WINDOW (dialog), 600, 500);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  _glade_util_dialog_set_hig (GTK_DIALOG (dialog));
  content_area =  gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_show (vbox);

  gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

  gtk_box_pack_start (GTK_BOX (content_area), vbox, TRUE, TRUE, 0);

  /* Checklist */
  label = gtk_label_new_with_mnemonic (_("O_bjects:"));
  gtk_widget_show (label);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (sw);
  gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
  gtk_widget_set_size_request (sw, 400, 200);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);


  if (*object)
    selected_list = g_list_prepend (selected_list, *object);

  if (exception)
    exception_list = g_list_prepend (exception_list, exception);

  tree_view = glade_eprop_object_view (TRUE);
  glade_eprop_object_populate_view (project,
                                    GTK_TREE_VIEW (tree_view),
                                    selected_list, exception_list,
                                    object_type, FALSE);
  g_list_free (selected_list);
  g_list_free (exception_list);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  gtk_widget_show (tree_view);
  gtk_container_add (GTK_CONTAINER (sw), tree_view);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label), tree_view);

  /* Run the dialog */
  res = gtk_dialog_run (GTK_DIALOG (dialog));
  if (res == GTK_RESPONSE_OK)
    {
      GladeWidget *selected = NULL;

      gtk_tree_model_foreach
          (gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view)),
           (GtkTreeModelForeachFunc)
           glade_eprop_object_selected_widget, &selected);

      *object = selected;
    }
  else if (res == GLADE_RESPONSE_CLEAR)
    *object = NULL;

  gtk_widget_destroy (dialog);

  return (res == GTK_RESPONSE_OK || res == GLADE_RESPONSE_CLEAR);
}


static void
glade_eprop_object_show_dialog (GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GtkWidget *dialog, *parent;
  GtkWidget *vbox, *label, *sw;
  GtkWidget *tree_view;
  GtkWidget *content_area;
  GladeProject *project;
  GladeWidget  *widget;
  GParamSpec *pspec;
  gchar *title = glade_eprop_object_dialog_title (eprop);
  gint res;
  GladeWidgetAdaptor *create_adaptor = NULL;
  GList *selected_list = NULL, *exception_list = NULL;

  widget  = glade_property_get_widget (priv->property);
  project = glade_widget_get_project (widget);
  parent  = gtk_widget_get_toplevel (GTK_WIDGET (eprop));
  pspec   = glade_property_def_get_pspec (priv->property_def);

  if (glade_property_def_create_type (priv->property_def))
    create_adaptor =
      glade_widget_adaptor_get_by_name (glade_property_def_create_type (priv->property_def));
  if (!create_adaptor &&
      G_TYPE_IS_INSTANTIATABLE (pspec->value_type) && !G_TYPE_IS_ABSTRACT (pspec->value_type))
    create_adaptor = glade_widget_adaptor_get_by_type (pspec->value_type);

  if (create_adaptor)
    {
      dialog = gtk_dialog_new_with_buttons (title,
                                            GTK_WINDOW (parent),
                                            GTK_DIALOG_MODAL,
                                            _("_Cancel"), GTK_RESPONSE_CANCEL,
                                            _("C_lear"), GLADE_RESPONSE_CLEAR,
                                            _("_New"), GLADE_RESPONSE_CREATE,
                                            _("_OK"), GTK_RESPONSE_OK, NULL);
      g_free (title);
    }
  else
    {
      dialog = gtk_dialog_new_with_buttons (title,
                                            GTK_WINDOW (parent),
                                            GTK_DIALOG_MODAL,
                                            _("_Cancel"), GTK_RESPONSE_CANCEL,
                                            _("C_lear"), GLADE_RESPONSE_CLEAR,
                                            _("_OK"), GTK_RESPONSE_OK, NULL);
      g_free (title);
    }

  gtk_window_set_default_size (GTK_WINDOW (dialog), 600, 500);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  _glade_util_dialog_set_hig (GTK_DIALOG (dialog));  
  content_area =  gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_show (vbox);

  gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

  gtk_box_pack_start (GTK_BOX (content_area), vbox, TRUE, TRUE, 0);

  /* Checklist */
  label = gtk_label_new_with_mnemonic (_("O_bjects:"));
  gtk_widget_show (label);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (sw);
  gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
  gtk_widget_set_size_request (sw, 400, 200);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);


  exception_list = g_list_prepend (exception_list, widget);
  if (g_value_get_object (glade_property_inline_value (priv->property)))
    selected_list = g_list_prepend (selected_list,
                                    glade_widget_get_from_gobject
                                    (g_value_get_object
                                     (glade_property_inline_value (priv->property))));

  tree_view = glade_eprop_object_view (TRUE);
  glade_eprop_object_populate_view (project, GTK_TREE_VIEW (tree_view),
                                    selected_list, exception_list,
                                    pspec->value_type,
                                    glade_property_def_parentless_widget (priv->property_def));
  g_list_free (selected_list);
  g_list_free (exception_list);


  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  gtk_widget_show (tree_view);
  gtk_container_add (GTK_CONTAINER (sw), tree_view);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label), tree_view);


  /* Run the dialog */
  res = gtk_dialog_run (GTK_DIALOG (dialog));
  if (res == GTK_RESPONSE_OK)
    {
      GladeWidget *selected = NULL;

      gtk_tree_model_foreach
          (gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view)),
           (GtkTreeModelForeachFunc)
           glade_eprop_object_selected_widget, &selected);

      if (selected)
        {
          GValue *value;
          GObject *new_object, *old_object = NULL;
          GladeWidget *new_widget;

          glade_project_selection_set (project, 
                                       glade_widget_get_object (widget),
                                       TRUE);

          value = glade_property_def_make_gvalue_from_string
            (priv->property_def, glade_widget_get_name (selected), project);

          glade_property_get (priv->property, &old_object);
          new_object = g_value_get_object (value);
          new_widget = glade_widget_get_from_gobject (new_object);

          glade_command_push_group (_("Setting %s of %s to %s"),
                                    glade_property_def_get_name (priv->property_def),
                                    glade_widget_get_name (widget), 
                                    glade_widget_get_name (new_widget));

          /* Unparent the widget so we can reuse it for this property */
          if (glade_property_def_parentless_widget (priv->property_def))
            {
              GladeProperty *old_ref;

              if (!G_IS_PARAM_SPEC_OBJECT (pspec))
                g_warning ("Parentless widget property should be of object type");
              else if (new_object && old_object != new_object)
                {
                  /* Steal parentless reference widget references, basically some references
                   * can only be referenced by one property, here we clear it if such a reference
                   * exists for the target object
                   */
                  if ((old_ref = glade_widget_get_parentless_widget_ref (new_widget)))
                    glade_command_set_property (old_ref, NULL);
                }
            }

          /* Ensure that the object we will now refer to has an ID, the NULL
           * check is just paranoia, it *always* has a name.
           *
           * To refer to a widget, it needs to have a name.
           */
          glade_widget_ensure_name (new_widget, project, TRUE);

          glade_editor_property_commit (eprop, value);
          glade_command_pop_group ();

          g_value_unset (value);
          g_free (value);
        }
    }
  else if (res == GLADE_RESPONSE_CREATE)
    {
      GladeWidget *new_widget;

      /* translators: Creating 'a widget' for 'a property' of 'a widget' */
      glade_command_push_group (_("Creating %s for %s of %s"),
                                glade_widget_adaptor_get_display_name (create_adaptor),
                                glade_property_def_get_name (priv->property_def),
                                glade_widget_get_name (widget));

      /* Dont bother if the user canceled the widget */
      if ((new_widget =
           glade_command_create (create_adaptor, NULL, NULL, project)) != NULL)
        {
          GValue *value;

          glade_project_selection_set (project, glade_widget_get_object (widget), TRUE);

          /* Give the newly created object a name */
          glade_widget_ensure_name (new_widget, project, TRUE);

          value = g_new0 (GValue, 1);
          g_value_init (value, pspec->value_type);
          g_value_set_object (value, glade_widget_get_object (new_widget));

          glade_editor_property_commit (eprop, value);

          g_value_unset (value);
          g_free (value);
        }

      glade_command_pop_group ();
    }
  else if (res == GLADE_RESPONSE_CLEAR)
    {
      GValue *value = 
        glade_property_def_make_gvalue_from_string (priv->property_def, NULL, project);

      glade_editor_property_commit (eprop, value);

      g_value_unset (value);
      g_free (value);
    }

  gtk_widget_destroy (dialog);
}


static void
glade_eprop_object_load (GladeEditorProperty *eprop, GladeProperty *property)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GladeEPropObject *eprop_object = GLADE_EPROP_OBJECT (eprop);
  gchar *obj_name;

  /* Chain up first */
  editor_property_class->load (eprop, property);

  if (property == NULL)
    return;

  if ((obj_name = glade_widget_adaptor_string_from_value
       (glade_property_def_get_adaptor (priv->property_def),
        priv->property_def, glade_property_inline_value (property))) != NULL)
    {
      gtk_entry_set_text (GTK_ENTRY (eprop_object->entry), obj_name);
      g_free (obj_name);
    }
  else
    gtk_entry_set_text (GTK_ENTRY (eprop_object->entry), "");

}

static GtkWidget *
glade_eprop_object_create_input (GladeEditorProperty *eprop)
{
  GladeEPropObject *eprop_object = GLADE_EPROP_OBJECT (eprop);

  eprop_object->entry = gtk_entry_new ();
  gtk_widget_set_hexpand (eprop_object->entry, TRUE);
  gtk_widget_set_valign (eprop_object->entry, GTK_ALIGN_CENTER);
  gtk_editable_set_editable (GTK_EDITABLE (eprop_object->entry), FALSE);
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (eprop_object->entry), 
                                     GTK_ENTRY_ICON_SECONDARY,
                                     "gtk-edit");
  g_signal_connect_swapped (eprop_object->entry, "icon-release",
                            G_CALLBACK (glade_eprop_object_show_dialog), eprop);
  
  return eprop_object->entry;
}


/*******************************************************************************
                        GladeEditorPropertyObjectsClass
 *******************************************************************************/

struct _GladeEPropObjects
{
  GladeEditorProperty parent_instance;

  GtkWidget *entry;
};

GLADE_MAKE_EPROP (GladeEPropObjects, glade_eprop_objects, GLADE, EPROP_OBJECTS)

static void
glade_eprop_objects_finalize (GObject *object)
{
  /* Chain up */
  G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_objects_load (GladeEditorProperty *eprop, GladeProperty *property)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GladeEPropObjects *eprop_objects = GLADE_EPROP_OBJECTS (eprop);
  gchar *obj_name;

  /* Chain up first */
  editor_property_class->load (eprop, property);

  if (property == NULL)
    return;

  if ((obj_name = glade_widget_adaptor_string_from_value
       (glade_property_def_get_adaptor (priv->property_def),
        priv->property_def, glade_property_inline_value (property))) != NULL)
    {
      gtk_entry_set_text (GTK_ENTRY (eprop_objects->entry), obj_name);
      g_free (obj_name);
    }
  else
    gtk_entry_set_text (GTK_ENTRY (eprop_objects->entry), "");

}

static gboolean
glade_eprop_objects_selected_widget (GtkTreeModel *model,
                                     GtkTreePath  *path,
                                     GtkTreeIter  *iter,
                                     GList       **ret)
{
  gboolean selected;
  GladeWidget *widget;

  gtk_tree_model_get (model, iter,
                      OBJ_COLUMN_SELECTED, &selected,
                      OBJ_COLUMN_WIDGET, &widget, -1);


  if (selected)
    {
      *ret = g_list_append (*ret, glade_widget_get_object (widget));
      g_object_unref (widget);
    }

  return FALSE;
}

static void
glade_eprop_objects_show_dialog (GladeEditorProperty *eprop)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GtkWidget *dialog, *parent;
  GtkWidget *vbox, *label, *sw;
  GtkWidget *tree_view;
  GladeWidget *widget;
  GladeProject *project;
  GParamSpec   *pspec;
  gchar *title = glade_eprop_object_dialog_title (eprop);
  gint res;
  GList *selected_list = NULL, *exception_list = NULL, *selected_objects = NULL, *l;

  /* It's improbable but possible the editor is visible with no
   * property selected, in this case avoid crashes */
  if (!priv->property)
    return;

  widget  = glade_property_get_widget (priv->property);
  project = glade_widget_get_project (widget);
  parent  = gtk_widget_get_toplevel (GTK_WIDGET (eprop));
  pspec   = glade_property_def_get_pspec (priv->property_def);

  dialog = gtk_dialog_new_with_buttons (title,
                                        GTK_WINDOW (parent),
                                        GTK_DIALOG_MODAL |
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        _("C_lear"), GLADE_RESPONSE_CLEAR,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_OK"), GTK_RESPONSE_OK, NULL);
  g_free (title);

  gtk_window_set_default_size (GTK_WINDOW (dialog), 600, 500);

  _glade_util_dialog_set_hig (GTK_DIALOG (dialog));
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_show (vbox);

  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);

  gtk_box_pack_start (GTK_BOX
                      (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), vbox,
                      TRUE, TRUE, 0);

  /* Checklist */
  label = gtk_label_new (_("Objects:"));
  gtk_widget_show (label);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (sw);
  gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
  gtk_widget_set_size_request (sw, 400, 200);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);

  tree_view = glade_eprop_object_view (FALSE);

  /* Dont allow selecting the widget owning this property (perhaps this is wrong) */
  exception_list = g_list_prepend (exception_list, widget);

  /* Build the list of already selected objects */
  glade_property_get (priv->property, &selected_objects);
  for (l = selected_objects; l; l = l->next)
    selected_list = g_list_prepend (selected_list, glade_widget_get_from_gobject (l->data));

  glade_eprop_object_populate_view (project, GTK_TREE_VIEW (tree_view),
                                    selected_list, exception_list,
                                    glade_param_spec_objects_get_type (GLADE_PARAM_SPEC_OBJECTS (pspec)),
                                    glade_property_def_parentless_widget (priv->property_def));
  g_list_free (selected_list);
  g_list_free (exception_list);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  gtk_widget_show (tree_view);
  gtk_container_add (GTK_CONTAINER (sw), tree_view);

  /* Run the dialog */
  res = gtk_dialog_run (GTK_DIALOG (dialog));
  if (res == GTK_RESPONSE_OK)
    {
      GValue *value;
      GList *selected = NULL, *l;

      gtk_tree_model_foreach
          (gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view)),
           (GtkTreeModelForeachFunc)
           glade_eprop_objects_selected_widget, &selected);

      if (selected)
        {
          glade_command_push_group (_("Setting %s of %s"),
                                    glade_property_def_get_name (priv->property_def),
                                    glade_widget_get_name (widget));

          /* Make sure the selected widgets have names now
           */
          for (l = selected; l; l = l->next)
            {
              GObject *object = l->data;
              GladeWidget *selected_widget = glade_widget_get_from_gobject (object);

              glade_widget_ensure_name (selected_widget, project, TRUE);
            }
        }

      value = glade_property_def_make_gvalue (priv->property_def, selected);
      glade_editor_property_commit (eprop, value);

      if (selected)
        glade_command_pop_group ();

      g_value_unset (value);
      g_free (value);
    }
  else if (res == GLADE_RESPONSE_CLEAR)
    {
      GValue *value = glade_property_def_make_gvalue (priv->property_def, NULL);

      glade_editor_property_commit (eprop, value);

      g_value_unset (value);
      g_free (value);
    }
  gtk_widget_destroy (dialog);
}

static GtkWidget *
glade_eprop_objects_create_input (GladeEditorProperty *eprop)
{
  GladeEPropObjects *eprop_objects = GLADE_EPROP_OBJECTS (eprop);

  eprop_objects->entry = gtk_entry_new ();
  gtk_widget_set_hexpand (eprop_objects->entry, TRUE);
  gtk_widget_set_valign (eprop_objects->entry, GTK_ALIGN_CENTER);
  gtk_editable_set_editable (GTK_EDITABLE (eprop_objects->entry), FALSE);
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (eprop_objects->entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     "gtk-edit");
  g_signal_connect_swapped (eprop_objects->entry, "icon-release",
                            G_CALLBACK (glade_eprop_objects_show_dialog), eprop);
  
  return eprop_objects->entry;
}

/*******************************************************************************
                                     API
 *******************************************************************************/
/**
 * glade_editor_property_commit:
 * @eprop: A #GladeEditorProperty
 * @value: The #GValue to commit
 *
 * Commits @value to the property currently being edited by @eprop.
 *
 */
void
glade_editor_property_commit (GladeEditorProperty *eprop, GValue *value)
{
  g_return_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop));
  g_return_if_fail (G_IS_VALUE (value));

  g_signal_emit (G_OBJECT (eprop), glade_eprop_signals[COMMIT], 0, value);
}

/**
 * glade_editor_property_load:
 * @eprop: A #GladeEditorProperty
 * @property: A #GladeProperty
 *
 * Loads @property values into @eprop and connects.
 * (the editor property will watch the property's value
 * until its loaded with another property or %NULL)
 */
void
glade_editor_property_load (GladeEditorProperty *eprop,
                            GladeProperty       *property)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);

  g_return_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop));
  g_return_if_fail (property == NULL || GLADE_IS_PROPERTY (property));

  priv->loading = TRUE;
  GLADE_EDITOR_PROPERTY_GET_CLASS (eprop)->load (eprop, property);
  priv->loading = FALSE;
}


/**
 * glade_editor_property_load_by_widget:
 * @eprop: A #GladeEditorProperty
 * @widget: A #GladeWidget
 *
 * Convenience function to load the appropriate #GladeProperty into
 * @eprop from @widget
 */
void
glade_editor_property_load_by_widget (GladeEditorProperty *eprop,
                                      GladeWidget         *widget)
{
  GladeEditorPropertyPrivate *priv = glade_editor_property_get_instance_private (eprop);
  GladeProperty *property = NULL;

  g_return_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop));
  g_return_if_fail (widget == NULL || GLADE_IS_WIDGET (widget));

  if (widget)
    {
      /* properties are allowed to be missing on some internal widgets */
      if (glade_property_def_get_is_packing (priv->property_def))
        property = glade_widget_get_pack_property (widget, glade_property_def_id (priv->property_def));
      else
        property = glade_widget_get_property (widget, glade_property_def_id (priv->property_def));

      glade_editor_property_load (eprop, property);

      if (priv->item_label)
        glade_property_label_set_property (GLADE_PROPERTY_LABEL (priv->item_label), property);

      if (property)
        {
          g_assert (priv->property_def == glade_property_get_def (property));

          gtk_widget_show (GTK_WIDGET (eprop));

          if (priv->item_label)
            gtk_widget_show (priv->item_label);
        }
      else
        {
          gtk_widget_hide (GTK_WIDGET (eprop));

          if (priv->item_label)
            gtk_widget_hide (priv->item_label);
        }
    }
  else
    glade_editor_property_load (eprop, NULL);
}
