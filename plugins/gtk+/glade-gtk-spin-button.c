/*
 * glade-gtk-spin-button.c - GladeWidgetAdaptor for GtkSpinButton
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

#include "glade-spin-button-editor.h"

GladeEditable *
glade_gtk_spin_button_create_editable (GladeWidgetAdaptor *adaptor,
                                       GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_spin_button_editor_new ();

  return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}

static void
glade_gtk_spin_button_set_adjustment (GObject *object, const GValue *value)
{
  GObject *adjustment;
  GtkAdjustment *adj;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (object));

  adjustment = g_value_get_object (value);

  if (adjustment && GTK_IS_ADJUSTMENT (adjustment))
    {
      adj = GTK_ADJUSTMENT (adjustment);

      if (gtk_adjustment_get_page_size (adj) > 0)
        {
          GladeWidget *gadj = glade_widget_get_from_gobject (adj);

          /* Silently set any spin-button adjustment page size to 0 */
          glade_widget_property_set (gadj, "page-size", 0.0F);
          gtk_adjustment_set_page_size (adj, 0);
        }

      gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (object), adj);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (object),
                                 gtk_adjustment_get_value (adj));
    }
}

void
glade_gtk_spin_button_set_property (GladeWidgetAdaptor *adaptor,
                                    GObject            *object,
                                    const gchar        *id,
                                    const GValue       *value)
{
  if (!strcmp (id, "adjustment"))
    glade_gtk_spin_button_set_adjustment (object, value);
  else
    GWA_GET_CLASS (GTK_TYPE_ENTRY)->set_property (adaptor, object, id, value);
}
