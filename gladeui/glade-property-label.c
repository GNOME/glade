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
#include "glade-property-label.h"

/* GObjectClass */
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

struct _GladePropertyLabelPrivate
{
  GladeProperty *property;

  GtkWidget     *warning;
  GtkWidget     *label;

  gulong         tooltip_id;   /* signal connection id for tooltip changes     */
  gulong         state_id;     /* signal connection id for state changes       */
  gulong         sensitive_id; /* signal connection id for sensitivity changes */
  gulong         enabled_id;   /* signal connection id for property enabled changes */
};

enum {
  PROP_0,
  PROP_PROPERTY
};

G_DEFINE_TYPE (GladePropertyLabel, glade_property_label, GTK_TYPE_EVENT_BOX);

static void
glade_property_label_init (GladePropertyLabel *label)
{
  label->priv = 
    G_TYPE_INSTANCE_GET_PRIVATE (label,
				 GLADE_TYPE_PROPERTY_LABEL,
				 GladePropertyLabelPrivate);
  
  gtk_widget_init_template (GTK_WIDGET (label));
}

static void
glade_property_label_class_init (GladePropertyLabelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  
  gobject_class->dispose = glade_property_label_dispose;
  gobject_class->set_property = glade_property_label_set_real_property;
  gobject_class->get_property = glade_property_label_get_real_property;

  widget_class->button_press_event = glade_property_label_button_press;

  /* Install a property, this is actually just a proxy for the internal GtkEntry text */
  g_object_class_install_property (gobject_class,
                                   PROP_PROPERTY,
                                   g_param_spec_string ("property",
							_("Property"),
                                                        _("The GladeProperty to display a label for"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /* Bind to template */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladeui/glade-property-label.ui");
  gtk_widget_class_bind_child (widget_class, GladePropertyLabelPrivate, label);
  gtk_widget_class_bind_child (widget_class, GladePropertyLabelPrivate, warning);

  g_type_class_add_private (gobject_class, sizeof (GladePropertyLabelPrivate));
}


/***********************************************************
 *                     GObjectClass                        *
 ***********************************************************/
static void
glade_property_label_dispose (GObject *object)
{
  glade_property_label_set_property (GLADE_PROPERTY_LABEL (object), NULL);

  G_OBJECT_CLASS (glade_property_label_parent_class)->dispose (object);
}

static void
glade_property_label_set_real_property (GObject         *object,
					guint            prop_id,
					const GValue    *value,
					GParamSpec      *pspec)
{
  GladePropertyLabel *label = GLADE_PROPERTY_LABEL (object);

  switch (prop_id)
    {
    case PROP_PROPERTY:
      glade_property_label_set_property (label, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glade_property_label_get_real_property (GObject         *object,
					guint            prop_id,
					GValue          *value,
					GParamSpec      *pspec)
{
  GladePropertyLabel *label = GLADE_PROPERTY_LABEL (object);

  switch (prop_id)
    {
    case PROP_PROPERTY:
      g_value_set_object (value, glade_property_label_get_property (label));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/***********************************************************
 *                     GtkWidgetClass                      *
 ***********************************************************/
static gint
glade_property_label_button_press (GtkWidget       *widget,
				   GdkEventButton  *event)
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

  gtk_widget_set_sensitive (GTK_WIDGET (label), sensitive);
}

static void
glade_property_label_state_cb (GladeProperty      *property,
			       GParamSpec         *pspec,
			       GladePropertyLabel *label)
{
  GladePropertyLabelPrivate *priv = label->priv;
  GladePropertyClass *pclass;
  gchar *text = NULL;

  if (!priv->property)
    return;

  pclass = glade_property_get_class (priv->property);

  /* refresh label */
  if ((glade_property_get_state (priv->property) & GLADE_STATE_CHANGED) != 0)
    text = g_strdup_printf ("<b>%s:</b>", glade_property_class_get_name (pclass));
  else
    text = g_strdup_printf ("%s:", glade_property_class_get_name (pclass));
  gtk_label_set_markup (GTK_LABEL (priv->label), text);
  g_free (text);

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
glade_property_label_set_property (GladePropertyLabel    *label,
				   GladeProperty         *property)
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
	  GladePropertyClass *pclass = glade_property_get_class (priv->property);

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
	    (property, glade_property_class_get_tooltip (pclass),
	     glade_propert_get_insensitive_tooltip (property),
	     glade_property_get_support_warning (property), label);

	  /* Load initial sensitive state.
	   */
	  glade_property_label_sensitivity_cb (property, NULL, label);

	  /* Load intial label state
	   */
	  glade_property_label_state_cb (property, NULL, label);
	}

      g_object_notify (G_OBJECT (label), "property");
    }
}

GladeProperty *
glade_property_label_get_property (GladePropertyLabel *label)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_LABEL (label), NULL);

  return label->priv->property;
}
