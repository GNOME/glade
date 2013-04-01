/*
 * glade-bool-toggle.h
 *
 * Copyright (C) 2013 Juan Pablo Ugarte <juanpablougarte@gmail.com>
   *
 * Authors:
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
   *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "glade-bool-toggle.h"
#include <glib/gi18n-lib.h>

enum
{
  PROP_0,
  PROP_TRUE_STRING,
  PROP_FALSE_STRING
};

struct _GladeBoolTogglePrivate
{
  gchar *true_string;
  gchar *false_string;
};

#define GLADE_BOOL_TOGGLE_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), \
                                               GLADE_TYPE_BOOL_TOGGLE,                \
                                               GladeBoolTogglePrivate))
#define GLADE_BOOL_TOGGLE_PRIVATE(object) (((GladeBoolToggle*)object)->priv)

G_DEFINE_TYPE (GladeBoolToggle, glade_bool_toggle, GTK_TYPE_TOGGLE_BUTTON);


/* translators: This is the string used in boolean property editor toggle button when is active */
#define TRUE_STRING  _("<span color='green'>✓</span>True")

/* translators: This is the string used in boolean property editor toggle button when is not active */
#define FALSE_STRING _("<span color='red'>✗</span>False")

static void
glade_bool_toggle_init (GladeBoolToggle *toggle)
{
  GladeBoolTogglePrivate *priv = GLADE_BOOL_TOGGLE_GET_PRIVATE (toggle);
  GtkWidget *label = gtk_label_new ("");

  toggle->priv = priv;

  priv->true_string = g_strdup (TRUE_STRING);
  priv->false_string = g_strdup (FALSE_STRING);

  gtk_label_set_markup (GTK_LABEL (label), priv->false_string);
  gtk_container_add (GTK_CONTAINER (toggle), label);
  gtk_widget_show (label);
}

static void
glade_bool_toggle_finalize (GObject *object)
{
  G_OBJECT_CLASS (glade_bool_toggle_parent_class)->finalize (object);
}

static gint
layout_get_value (PangoLayout *layout, const gchar *string)
{
  PangoRectangle rect;
  pango_layout_set_markup (layout, string, -1);
  pango_layout_get_extents (layout, NULL, &rect);
  pango_extents_to_pixels (&rect, NULL);
  return rect.width;
}

static void
glade_bool_toggle_get_preferred_width (GtkWidget *widget,
                                       gint      *minimum,
                                       gint      *natural)
{
  GladeBoolTogglePrivate *priv = GLADE_BOOL_TOGGLE_PRIVATE (widget);
  PangoLayout *layout = gtk_widget_create_pango_layout (widget, "");
  GtkWidget *label = gtk_bin_get_child (GTK_BIN (widget));
  gint value, width, true_width, false_width;

  GTK_WIDGET_CLASS (glade_bool_toggle_parent_class)->get_preferred_width (widget, &width, NULL);

  true_width = layout_get_value (layout, priv->true_string);
  false_width = layout_get_value (layout, priv->false_string);

  if (g_strcmp0 (gtk_label_get_label (GTK_LABEL (label)), priv->true_string))
    value = width - false_width;
  else
    value = width - true_width;

  value += MAX (true_width, false_width);

  g_object_unref (layout);

  if (minimum)
    *minimum = value;

  if (natural)
    *natural = value;
}

static void
glade_bool_toggle_toggled (GtkToggleButton *toggle_button)
{
  GladeBoolTogglePrivate *priv = GLADE_BOOL_TOGGLE_PRIVATE (toggle_button);
  GtkWidget *label = gtk_bin_get_child (GTK_BIN (toggle_button));

  if (GTK_IS_LABEL (label))
    {
      if (gtk_toggle_button_get_active (toggle_button))
        gtk_label_set_markup (GTK_LABEL (label), priv->true_string);
      else
        gtk_label_set_markup (GTK_LABEL (label), priv->false_string);
    }
}

static void
glade_bool_toggle_set_property (GObject *object,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
  GladeBoolTogglePrivate *priv = GLADE_BOOL_TOGGLE_PRIVATE (object);
  
  switch (prop_id)
    {
      case PROP_TRUE_STRING:
        g_free (priv->true_string);
        priv->true_string = g_value_dup_string (value);
        glade_bool_toggle_toggled (GTK_TOGGLE_BUTTON (object));
      break;
      case PROP_FALSE_STRING:
        g_free (priv->false_string);
        priv->false_string = g_value_dup_string (value);
        glade_bool_toggle_toggled (GTK_TOGGLE_BUTTON (object));
      break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glade_bool_toggle_get_property (GObject *object,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
  GladeBoolTogglePrivate *priv = GLADE_BOOL_TOGGLE_PRIVATE (object);
  
  switch (prop_id)
    {
      case PROP_TRUE_STRING:
        g_value_set_string (value, priv->true_string);
      break;
      case PROP_FALSE_STRING:
        g_value_set_string (value, priv->false_string);
      break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glade_bool_toggle_class_init (GladeBoolToggleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkToggleButtonClass *toggle_class = GTK_TOGGLE_BUTTON_CLASS (klass);

  object_class->get_property = glade_bool_toggle_get_property;
  object_class->set_property = glade_bool_toggle_set_property;
  object_class->finalize = glade_bool_toggle_finalize;
  widget_class->get_preferred_width = glade_bool_toggle_get_preferred_width;
  toggle_class->toggled = glade_bool_toggle_toggled;

  g_object_class_install_property (object_class, PROP_TRUE_STRING,
                                   g_param_spec_string ("true-string", "True string",
                                                        "String to use when toggled",
                                                        TRUE_STRING,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_FALSE_STRING,
                                   g_param_spec_string ("false-string", "False string",
                                                        "String to use when not toggled",
                                                        FALSE_STRING,
                                                        G_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GladeBoolTogglePrivate));
}

GtkWidget *
_glade_bool_toggle_new (void)
{
  return g_object_new (GLADE_TYPE_BOOL_TOGGLE, NULL);
}
