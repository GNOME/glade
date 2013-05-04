/*
 * glade-gtk-combo-box.c - GladeWidgetAdaptor for GtkComboBox
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
glade_gtk_combo_box_set_property (GladeWidgetAdaptor * adaptor,
                                  GObject * object,
                                  const gchar * id, const GValue * value)
{
  if (!strcmp (id, "entry-text-column"))
    {
      /* Avoid warnings */
      if (g_value_get_int (value) >= 0)
        GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor,
                                                          object, id, value);
    }
  else if (!strcmp (id, "text-column"))
    {
      if (g_value_get_int (value) >= 0)
        gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (object),
                                             g_value_get_int (value));
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor,
                                                      object, id, value);
}

GList *glade_gtk_cell_layout_get_children (GladeWidgetAdaptor * adaptor,
                                           GObject * container);

GList *
glade_gtk_combo_box_get_children (GladeWidgetAdaptor * adaptor,
                                  GtkComboBox * combo)
{
  GList *list = NULL;

  list = glade_gtk_cell_layout_get_children (adaptor, G_OBJECT (combo));

  /* return the internal entry.
   *
   * FIXME: for recent gtk+ we have no comboboxentry
   * but a "has-entry" property instead
   */
  if (gtk_combo_box_get_has_entry (combo))
    list = g_list_append (list, gtk_bin_get_child (GTK_BIN (combo)));

  return list;
}
