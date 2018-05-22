/*
 * glade-gtk-adjustment.c - GladeWidgetAdaptor for GtkAdjustment
 *
 * Copyright (C) 2013 Tristan Van Berkom
 *
 * Authors:
 *      Tristan Van Berkom <tristan.van.berkom@gmail.com>
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
 */

#include <config.h>
#include <glib/gi18n-lib.h>
#include <gladeui/glade.h>

void
glade_gtk_adjustment_write_widget (GladeWidgetAdaptor *adaptor,
                                   GladeWidget        *widget,
                                   GladeXmlContext    *context,
                                   GladeXmlNode       *node)
{
  GladeProperty *prop;

  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* Ensure proper order of adjustment properties by writing them here. */
  prop = glade_widget_get_property (widget, "lower");
  glade_property_write (prop, context, node);

  prop = glade_widget_get_property (widget, "upper");
  glade_property_write (prop, context, node);

  prop = glade_widget_get_property (widget, "value");
  glade_property_write (prop, context, node);

  GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);
}

static gint
double_get_precision (double number, gdouble epsilon)
{
  char str[G_ASCII_DTOSTR_BUF_SIZE];
  int i;

  for (i = 0; i <= 20; i++)
    {
      snprintf (str, G_ASCII_DTOSTR_BUF_SIZE, "%.*f", i, number);

      if (ABS (g_ascii_strtod (str, NULL) - number) <= epsilon)
        return i;
    }

  return 20;
}

static gint
get_prop_precision (GladeWidget *widget, gchar *property)
{
  GladeProperty *prop = glade_widget_get_property (widget, property);
  GladePropertyClass *klass = glade_property_get_class (prop);
  GParamSpec *pspec = glade_property_class_get_pspec (klass);
  GValue value = G_VALUE_INIT;

  glade_property_get_value (prop, &value);

  if (G_IS_PARAM_SPEC_FLOAT (pspec))
    return double_get_precision (g_value_get_float (&value),
                                 ((GParamSpecFloat *)pspec)->epsilon);
  else if (G_IS_PARAM_SPEC_DOUBLE (pspec))
    return double_get_precision (g_value_get_double (&value),
                                 ((GParamSpecDouble *)pspec)->epsilon);
  return 0;
}

static gint
get_digits (GladeWidget *widget)
{
  gint digits = 2;

  digits = MAX (digits, get_prop_precision (widget, "value"));
  digits = MAX (digits, get_prop_precision (widget, "lower"));
  digits = MAX (digits, get_prop_precision (widget, "upper"));
  digits = MAX (digits, get_prop_precision (widget, "page-increment"));
  digits = MAX (digits, get_prop_precision (widget, "step-increment"));
  return MAX (digits, get_prop_precision (widget, "page-size"));
}

void
glade_gtk_adjustment_read_widget (GladeWidgetAdaptor *adaptor,
                                  GladeWidget        *widget,
                                  GladeXmlNode       *node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (GTK_TYPE_WIDGET)->read_widget (adaptor, widget, node);

  glade_widget_property_set (widget, "glade-digits", get_digits (widget), NULL);
}

void
glade_gtk_adjustment_set_property (GladeWidgetAdaptor *adaptor,
                                   GObject            *object,
                                   const gchar        *property_name,
                                   const GValue       *value)
{
  if (!strcmp (property_name, "glade-digits"))
    {
      GladeWidget *adjustment = glade_widget_get_from_gobject (object);
      gint digits = g_value_get_int (value);

      g_object_set (glade_widget_get_property (adjustment, "value"),
                    "precision", digits, NULL);
      g_object_set (glade_widget_get_property (adjustment, "lower"),
                    "precision", digits, NULL);
      g_object_set (glade_widget_get_property (adjustment, "upper"),
                    "precision", digits, NULL);
      g_object_set (glade_widget_get_property (adjustment, "page-increment"),
                    "precision", digits, NULL);
      g_object_set (glade_widget_get_property (adjustment, "step-increment"),
                    "precision", digits, NULL);
      g_object_set (glade_widget_get_property (adjustment, "page-size"),
                    "precision", digits, NULL);
    }
  else
    GWA_GET_CLASS (G_TYPE_OBJECT)->set_property (adaptor, object,
                                                 property_name, value);
}


gboolean
glade_gtk_adjustment_verify_property (GladeWidgetAdaptor *adaptor,
                                      GObject            *object,
                                      const gchar        *id,
                                      const GValue       *value)
{
  if (!strcmp (id, "glade-digits"))
    {
      gint digits = get_digits (glade_widget_get_from_gobject (object));
      return g_value_get_int (value) >= digits;
    }

  return TRUE;
}
