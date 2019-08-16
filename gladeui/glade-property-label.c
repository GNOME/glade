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
#include "glade-property-label.h"

/* GObjectClass */
static void      glade_property_label_finalize          (GObject         *object);
static void      glade_property_label_dispose           (GObject         *object);
static void      glade_property_label_set_real_property (GObject         *object,
                                                         guint            prop_id,
                                                         const GValue    *value,
                                                         GParamSpec      *pspec);
static void      glade_property_label_get_real_property (GObject         *object,
                                                         guint            prop_id,
                                                         GValue          *value,
                                                         GParamSpec      *pspec);

/* GtkWidgetClass */
static gint      glade_property_label_button_press      (GtkWidget       *widget,
                                                         GdkEventButton  *event);

/* GladeEditableInterface */
static void      glade_property_label_editable_init     (GladeEditableInterface *iface);

struct _GladePropertyLabelPrivate
{
  GladeProperty *property;

  GtkWidget     *warning;
  GtkWidget     *label;
  GtkWidget     *box;

  gulong         tooltip_id;   /* signal connection id for tooltip changes     */
  gulong         state_id;     /* signal connection id for state changes       */
  gulong         sensitive_id; /* signal connection id for sensitivity changes */
  gulong         enabled_id;   /* signal connection id for property enabled changes */

  gchar         *property_name; /* The property name to use when loading by GladeWidget */

  guint          packing : 1;
  guint          custom_text : 1;
  guint          custom_tooltip : 1;
  guint          append_colon : 1;
};

enum {
  PROP_0,
  PROP_PROPERTY,
  PROP_PROPERTY_NAME,
  PROP_APPEND_COLON,
  PROP_PACKING,
  PROP_CUSTOM_TEXT,
  PROP_CUSTOM_TOOLTIP,
};

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladePropertyLabel, glade_property_label, GTK_TYPE_EVENT_BOX,
                         G_ADD_PRIVATE (GladePropertyLabel)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_property_label_editable_init));

static void
glade_property_label_init (GladePropertyLabel *label)
{
  label->priv = glade_property_label_get_instance_private (label);

  label->priv->packing = FALSE;
  label->priv->custom_text = FALSE;
  label->priv->custom_tooltip = FALSE;
  label->priv->append_colon = TRUE;
  
  gtk_widget_init_template (GTK_WIDGET (label));
}

static void
glade_property_label_class_init (GladePropertyLabelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  
  gobject_class->finalize = glade_property_label_finalize;
  gobject_class->dispose = glade_property_label_dispose;
  gobject_class->set_property = glade_property_label_set_real_property;
  gobject_class->get_property = glade_property_label_get_real_property;

  widget_class->button_press_event = glade_property_label_button_press;

  g_object_class_install_property
    (gobject_class, PROP_PROPERTY,
     g_param_spec_object ("property", _("Property"),
                          _("The GladeProperty to display a label for"),
                          GLADE_TYPE_PROPERTY, G_PARAM_READWRITE));

  g_object_class_install_property
      (gobject_class, PROP_PROPERTY_NAME,
       g_param_spec_string ("property-name", _("Property Name"),
                            /* To Translators: the property name/id to use to get
                             * the GladeProperty object from the GladeWidget the
                             * property belongs to.
                             */
                            _("The property name to use when loading by widget"),
                            NULL, G_PARAM_READWRITE));

  g_object_class_install_property
      (gobject_class, PROP_APPEND_COLON,
       g_param_spec_boolean ("append-colon", _("Append Colon"),
                             _("Whether to append a colon ':' to the property name"),
                             TRUE, G_PARAM_READWRITE));

  g_object_class_install_property
      (gobject_class, PROP_PACKING,
       g_param_spec_boolean ("packing", _("Packing"),
                             /* To Translators: packing properties or child properties are
                              * properties introduced by GtkContainer and they are not specific
                              * to the container or child widget but to the relation.
                              * For more information see GtkContainer docs.
                              */
                             _("Whether the property to load is a packing property or not"),
                             FALSE, G_PARAM_READWRITE));

  g_object_class_install_property
      (gobject_class, PROP_CUSTOM_TEXT,
       g_param_spec_string ("custom-text", _("Custom Text"),
                            _("Custom text to override the property name"),
                            NULL, G_PARAM_READWRITE));

  g_object_class_install_property
      (gobject_class, PROP_CUSTOM_TOOLTIP,
       g_param_spec_string ("custom-tooltip", _("Custom Tooltip"),
                            _("Custom tooltip to override the property description"),
                            NULL, G_PARAM_READWRITE));

  /* Bind to template */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladeui/glade-property-label.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GladePropertyLabel, box);
  gtk_widget_class_bind_template_child_private (widget_class, GladePropertyLabel, label);
  gtk_widget_class_bind_template_child_private (widget_class, GladePropertyLabel, warning);
}


/***********************************************************
 *                     GObjectClass                        *
 ***********************************************************/
static void
glade_property_label_finalize (GObject *object)
{
  GladePropertyLabel *label = GLADE_PROPERTY_LABEL (object);

  g_free (label->priv->property_name);

  G_OBJECT_CLASS (glade_property_label_parent_class)->finalize (object);
}

static void
glade_property_label_dispose (GObject *object)
{
  GladePropertyLabel *label = GLADE_PROPERTY_LABEL (object);

  glade_property_label_set_property (label, NULL);

  G_OBJECT_CLASS (glade_property_label_parent_class)->dispose (object);
}

static void
glade_property_label_set_real_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GladePropertyLabel *label = GLADE_PROPERTY_LABEL (object);

  switch (prop_id)
    {
    case PROP_PROPERTY:
      glade_property_label_set_property (label, g_value_get_object (value));
      break;
    case PROP_PROPERTY_NAME:
      glade_property_label_set_property_name (label, g_value_get_string (value));
      break;
    case PROP_APPEND_COLON:
      glade_property_label_set_append_colon (label, g_value_get_boolean (value));
      break;
    case PROP_PACKING:
      glade_property_label_set_packing (label, g_value_get_boolean (value));
      break;
    case PROP_CUSTOM_TEXT:
      glade_property_label_set_custom_text (label, g_value_get_string (value));
      break;
    case PROP_CUSTOM_TOOLTIP:
      glade_property_label_set_custom_tooltip (label, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glade_property_label_get_real_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GladePropertyLabel *label = GLADE_PROPERTY_LABEL (object);

  switch (prop_id)
    {
    case PROP_PROPERTY:
      g_value_set_object (value, glade_property_label_get_property (label));
      break;
    case PROP_PROPERTY_NAME:
      g_value_set_string (value, glade_property_label_get_property_name (label));
      break;
    case PROP_PACKING:
      g_value_set_boolean (value, glade_property_label_get_packing (label));
      break;
    case PROP_APPEND_COLON:
      g_value_set_boolean (value, glade_property_label_get_append_colon (label));
      break;
    case PROP_CUSTOM_TEXT:
      g_value_set_string (value, glade_property_label_get_custom_text (label));
      break;
    case PROP_CUSTOM_TOOLTIP:
      g_value_set_string (value, glade_property_label_get_custom_tooltip (label));
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
glade_property_label_load (GladeEditable *editable,
                           GladeWidget   *widget)
{
  GladePropertyLabel *label = GLADE_PROPERTY_LABEL (editable);
  GladePropertyLabelPrivate *priv;
  GladeProperty *property;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  g_return_if_fail (label->priv->property_name != NULL);

  priv = label->priv;

  if (widget)
    {
      if (priv->packing)
        property = glade_widget_get_pack_property (widget, priv->property_name);
      else
        property = glade_widget_get_property (widget, priv->property_name);

      glade_property_label_set_property (label, property);
    }
  else
    glade_property_label_set_property (label, NULL);
}

static void
glade_property_label_set_show_name (GladeEditable *editable, gboolean show_name)
{
}

static void
glade_property_label_editable_init (GladeEditableInterface *iface)
{
  parent_editable_iface = g_type_default_interface_peek (GLADE_TYPE_EDITABLE);

  iface->load = glade_property_label_load;
  iface->set_show_name = glade_property_label_set_show_name;
}

/***********************************************************
 *                     GtkWidgetClass                      *
 ***********************************************************/
static gint
glade_property_label_button_press (GtkWidget      *widget,
                                   GdkEventButton *event)
{
  GladePropertyLabel        *label = GLADE_PROPERTY_LABEL (widget);
  GladePropertyLabelPrivate *priv = label->priv;

  if (priv->property && glade_popup_is_popup_event (event))
    {
      glade_popup_property_pop (priv->property, event);
      return TRUE;
    }

  return FALSE;
}

/***********************************************************
 *                        Callbacks                        *
 ***********************************************************/
static void
glade_property_label_tooltip_cb (GladeProperty      *property,
                                 const gchar        *tooltip,
                                 const gchar        *insensitive,
                                 const gchar        *support,
                                 GladePropertyLabel *label)
{
  GladePropertyLabelPrivate *priv = label->priv;
  const gchar *choice_tooltip;

  if (glade_property_get_sensitive (property))
    choice_tooltip = tooltip;
  else
    choice_tooltip = insensitive;

  if (!priv->custom_tooltip)
    gtk_widget_set_tooltip_text (priv->label, choice_tooltip);

  gtk_widget_set_tooltip_text (priv->warning, support);
}

static void
glade_property_label_sensitivity_cb (GladeProperty      *property,
                                     GParamSpec         *pspec,
                                     GladePropertyLabel *label)
{
  GladePropertyLabelPrivate *priv = label->priv;
  gboolean sensitive;

  sensitive = glade_property_get_enabled (property);
  sensitive = sensitive && glade_property_get_sensitive (priv->property);
  sensitive = sensitive && (glade_property_get_state (priv->property) & GLADE_STATE_SUPPORT_DISABLED) == 0;

  gtk_widget_set_sensitive (priv->box, sensitive);
}

static PangoAttrList *
get_modified_attribute (void)
{
  static PangoAttrList *attrs = NULL;

  if (!attrs)
    {
      PangoAttribute *attr;

      attrs = pango_attr_list_new ();
      attr  = pango_attr_style_new (PANGO_STYLE_ITALIC);
      pango_attr_list_insert (attrs, attr);
    }

  return attrs;
}

static void
glade_property_label_state_cb (GladeProperty      *property,
                               GParamSpec         *pspec,
                               GladePropertyLabel *label)
{
  GladePropertyLabelPrivate *priv = label->priv;

  if (!priv->property)
    return;

  /* refresh label */
  if ((glade_property_get_state (priv->property) & GLADE_STATE_CHANGED) != 0)
    gtk_label_set_attributes (GTK_LABEL (priv->label), get_modified_attribute());
  else
    gtk_label_set_attributes (GTK_LABEL (priv->label), NULL);

  /* refresh icon */
  if ((glade_property_get_state (priv->property) & GLADE_STATE_UNSUPPORTED) != 0)
    gtk_widget_show (priv->warning);
  else
    gtk_widget_hide (priv->warning);
}

static void
glade_property_label_property_finalized (GladePropertyLabel *label,
                                         GladeProperty *where_property_was)
{
  /* Silent disconnect */
  label->priv->property = NULL;
  label->priv->tooltip_id = 0;
  label->priv->state_id = 0;
  label->priv->sensitive_id = 0;
  label->priv->enabled_id = 0;
}

/***********************************************************
 *                            API                          *
 ***********************************************************/
GtkWidget *
glade_property_label_new (void)
{
  return g_object_new (GLADE_TYPE_PROPERTY_LABEL, NULL);
}

void
glade_property_label_set_property_name (GladePropertyLabel *label,
                                        const gchar        *property_name)
{
  GladePropertyLabelPrivate *priv;

  g_return_if_fail (GLADE_IS_PROPERTY_LABEL (label));

  priv = label->priv;

  if (g_strcmp0 (priv->property_name, property_name))
    {
      g_free (priv->property_name);
      priv->property_name = g_strdup (property_name);

      g_object_notify (G_OBJECT (label), "property-name");
    }
}

const gchar *
glade_property_label_get_property_name (GladePropertyLabel *label)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_LABEL (label), NULL);

  return label->priv->property_name;
}

void
glade_property_label_set_append_colon (GladePropertyLabel *label,
                                       gboolean            append_colon)
{
  GladePropertyLabelPrivate *priv;

  g_return_if_fail (GLADE_IS_PROPERTY_LABEL (label));

  priv = label->priv;

  if (priv->append_colon != append_colon)
    {
      priv->append_colon = append_colon;

      g_object_notify (G_OBJECT (label), "append-colon");
    }
}

gboolean
glade_property_label_get_append_colon (GladePropertyLabel *label)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_LABEL (label), FALSE);

  return label->priv->append_colon;
}

void
glade_property_label_set_packing (GladePropertyLabel *label,
                                  gboolean            packing)
{
  GladePropertyLabelPrivate *priv;

  g_return_if_fail (GLADE_IS_PROPERTY_LABEL (label));

  priv = label->priv;

  if (priv->packing != packing)
    {
      priv->packing = packing;

      g_object_notify (G_OBJECT (label), "packing");
    }
}

gboolean
glade_property_label_get_packing (GladePropertyLabel *label)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_LABEL (label), FALSE);

  return label->priv->packing;
}

void
glade_property_label_set_custom_text (GladePropertyLabel *label,
                                      const gchar        *custom_text)
{
  GladePropertyLabelPrivate *priv;
  gboolean changed = FALSE;

  g_return_if_fail (GLADE_IS_PROPERTY_LABEL (label));

  priv = label->priv;

  if (custom_text)
    {
      if (!priv->custom_text)
        changed = TRUE;

      priv->custom_text = TRUE;

      gtk_label_set_markup (GTK_LABEL (priv->label), custom_text);
    }
  else
    {
      if (priv->custom_text)
        changed = TRUE;

      priv->custom_text = FALSE;

      if (priv->property)
          glade_property_label_state_cb (priv->property, NULL, label);
    }

  if (changed)
    g_object_notify (G_OBJECT (label), "custom-text");
}

const gchar *
glade_property_label_get_custom_text (GladePropertyLabel *label)
{
  GladePropertyLabelPrivate *priv;

  g_return_val_if_fail (GLADE_IS_PROPERTY_LABEL (label), NULL);

  priv = label->priv;

  if (priv->custom_text)
    return gtk_label_get_text (GTK_LABEL (priv->label));

  return NULL;
}

void
glade_property_label_set_custom_tooltip (GladePropertyLabel *label,
                                         const gchar        *custom_tooltip)
{
  GladePropertyLabelPrivate *priv;
  gboolean changed = FALSE;

  g_return_if_fail (GLADE_IS_PROPERTY_LABEL (label));

  priv = label->priv;

  if (custom_tooltip)
    {
      if (!priv->custom_tooltip)
        changed = TRUE;

      priv->custom_tooltip = TRUE;

      gtk_widget_set_tooltip_text (GTK_WIDGET (priv->label), custom_tooltip);
    }
  else
    {
      if (priv->custom_tooltip)
        changed = TRUE;

      priv->custom_tooltip = FALSE;

      if (priv->property)
        {
          GladePropertyDef *pdef = glade_property_get_def (priv->property);

          glade_property_label_tooltip_cb
            (priv->property, glade_property_def_get_tooltip (pdef),
             glade_propert_get_insensitive_tooltip (priv->property),
             glade_property_get_support_warning (priv->property), label);
        }
    }

  if (changed)
    g_object_notify (G_OBJECT (label), "custom-tooltip");
}

const gchar *
glade_property_label_get_custom_tooltip (GladePropertyLabel *label)
{
  GladePropertyLabelPrivate *priv;

  g_return_val_if_fail (GLADE_IS_PROPERTY_LABEL (label), NULL);

  priv = label->priv;

  if (priv->custom_tooltip)
    return gtk_widget_get_tooltip_text (priv->label);

  return NULL;
}

void
glade_property_label_set_property (GladePropertyLabel *label,
                                   GladeProperty      *property)
{
  GladePropertyLabelPrivate *priv;

  g_return_if_fail (GLADE_IS_PROPERTY_LABEL (label));
  g_return_if_fail (property == NULL || GLADE_IS_PROPERTY (property));

  priv = label->priv;

  if (priv->property != property)
    {

      /* Disconnect last */
      if (priv->property)
        {
          if (priv->tooltip_id > 0)
            g_signal_handler_disconnect (priv->property, priv->tooltip_id);
          if (priv->state_id > 0)
            g_signal_handler_disconnect (priv->property, priv->state_id);
          if (priv->sensitive_id > 0)
            g_signal_handler_disconnect (priv->property, priv->sensitive_id);
          if (priv->enabled_id > 0)
            g_signal_handler_disconnect (priv->property, priv->enabled_id);

          priv->tooltip_id = 0;
          priv->state_id = 0;
          priv->sensitive_id = 0;
          priv->enabled_id = 0;

          g_object_weak_unref (G_OBJECT (priv->property),
                               (GWeakNotify) glade_property_label_property_finalized, label);
        }

      priv->property = property;

      /* Connect new */
      if (priv->property)
        {
          GladePropertyDef *pdef = glade_property_get_def (priv->property);

          priv->tooltip_id =
            g_signal_connect (G_OBJECT (priv->property),
                              "tooltip-changed",
                              G_CALLBACK (glade_property_label_tooltip_cb),
                              label);
          priv->sensitive_id =
            g_signal_connect (G_OBJECT (priv->property),
                              "notify::sensitive",
                              G_CALLBACK (glade_property_label_sensitivity_cb),
                              label);
          priv->state_id =
            g_signal_connect (G_OBJECT (priv->property),
                              "notify::state",
                              G_CALLBACK (glade_property_label_state_cb), label);
          priv->enabled_id =
            g_signal_connect (G_OBJECT (priv->property),
                              "notify::enabled",
                              G_CALLBACK (glade_property_label_sensitivity_cb),
                              label);

          g_object_weak_ref (G_OBJECT (priv->property),
                             (GWeakNotify) glade_property_label_property_finalized, label);

          /* Load initial tooltips
           */
          glade_property_label_tooltip_cb
            (property, glade_property_def_get_tooltip (pdef),
             glade_propert_get_insensitive_tooltip (property),
             glade_property_get_support_warning (property), label);

          /* Load initial sensitive state.
           */
          glade_property_label_sensitivity_cb (property, NULL, label);

          /* Load intial label state
           */
          glade_property_label_state_cb (property, NULL, label);

          if (!priv->custom_text)
            {
              if (priv->append_colon)
                {
                  gchar *text = g_strdup_printf ("%s:", glade_property_def_get_name (pdef));
                  gtk_label_set_text (GTK_LABEL (priv->label), text);
                  g_free (text);
                }
              else
                {
                  gtk_label_set_text (GTK_LABEL (priv->label),
                                      glade_property_def_get_name (pdef));
                }
            }
        }

      g_object_notify (G_OBJECT (label), "property");
    }
}

/**
 * glade_property_label_get_property:
 * @label: a #GladePropertyLabel
 *
 * Returns: (transfer none): A #GladeProperty
 */
GladeProperty *
glade_property_label_get_property (GladePropertyLabel *label)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_LABEL (label), NULL);

  return label->priv->property;
}
