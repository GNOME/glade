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

#define TRUE_STRING _("<span color='green'>✓</span>True")
#define FALSE_STRING _("<span color='red'>✗</span>False")

#include "glade-bool-toggle.h"
#include <glib/gi18n-lib.h>

G_DEFINE_TYPE (GladeBoolToggle, glade_bool_toggle, GTK_TYPE_TOGGLE_BUTTON);

static void
glade_bool_toggle_init (GladeBoolToggle *toggle)
{
  GtkWidget *label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL (label), FALSE_STRING);
  gtk_container_add (GTK_CONTAINER (toggle), label);
  gtk_widget_show (label);
}

static void
glade_bool_toggle_finalize (GObject *object)
{
  G_OBJECT_CLASS (glade_bool_toggle_parent_class)->finalize (object);
}

static gint
layout_get_value (PangoLayout *layout, const gchar *string, gboolean width)
{
  PangoRectangle rect;
  pango_layout_set_markup (layout, string, -1);
  pango_layout_get_extents (layout, NULL, &rect);
  pango_extents_to_pixels (&rect, NULL);
  return width ? rect.width : rect.height;
}

static void
glade_bool_toggle_get_preferred_size (GtkWidget *widget,
                                      gboolean   width,
                                      gint      *minimum,
                                      gint      *natural)
{
  PangoLayout *layout;
  gint value, tmp;

  layout = gtk_widget_create_pango_layout (widget, "");
    
  value = MAX (layout_get_value (layout, TRUE_STRING, width),
               layout_get_value (layout, FALSE_STRING, width));

  /* Add some extra spacing */
  tmp = layout_get_value (layout, (width) ? "-" : "'", TRUE);
  value += (width) ? tmp*2 : tmp;

  g_object_unref (layout);

  if (minimum)
    *minimum = value;

  if (natural)
    *natural = value;
}

static void
glade_bool_toggle_get_preferred_width (GtkWidget *widget,
                                       gint      *minimum,
                                       gint      *natural)
{
  glade_bool_toggle_get_preferred_size (widget, TRUE, minimum, natural);
}

static void
glade_bool_toggle_get_preferred_height (GtkWidget *widget,
                                        gint      *minimum,
                                        gint      *natural)
{
  glade_bool_toggle_get_preferred_size (widget, FALSE, minimum, natural);
}

static void
glade_bool_toggle_toggled (GtkToggleButton *toggle_button)
{
  GtkWidget *label = gtk_bin_get_child (GTK_BIN (toggle_button));

  if (GTK_IS_LABEL (label))
    {
      if (gtk_toggle_button_get_active (toggle_button))
        gtk_label_set_markup (GTK_LABEL (label), TRUE_STRING);
      else
        gtk_label_set_markup (GTK_LABEL (label), FALSE_STRING);
    }
}

static void
glade_bool_toggle_class_init (GladeBoolToggleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkToggleButtonClass *toggle_class = GTK_TOGGLE_BUTTON_CLASS (klass);

  object_class->finalize = glade_bool_toggle_finalize;
  widget_class->get_preferred_width = glade_bool_toggle_get_preferred_width;
  widget_class->get_preferred_height = glade_bool_toggle_get_preferred_height;
  toggle_class->toggled = glade_bool_toggle_toggled;
}

GtkWidget *
_glade_bool_toggle_new (void)
{
  return g_object_new (GLADE_TYPE_BOOL_TOGGLE, NULL);
}
