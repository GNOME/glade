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

#include "glade-gtk-cell-layout.h"
#include "glade-combo-box-editor.h"

#define NO_ENTRY_MSG _("This combo box is not configured to have an entry")

GladeEditable *
glade_gtk_combo_box_create_editable (GladeWidgetAdaptor *adaptor,
                                     GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    {
      return (GladeEditable *) glade_combo_box_editor_new ();
    }

  return GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}

void
glade_gtk_combo_box_post_create (GladeWidgetAdaptor *adaptor,
                                 GObject            *object, 
                                 GladeCreateReason   reason)
{
  GladeWidget *widget;

  /* Chain Up */
  GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->post_create (adaptor, object, reason);

  widget = glade_widget_get_from_gobject (object);
  if (gtk_combo_box_get_has_entry (GTK_COMBO_BOX (object)))
    {
      glade_widget_property_set_sensitive (widget, "entry-text-column", TRUE, NULL);
      glade_widget_property_set_sensitive (widget, "has-frame", TRUE, NULL);
    }
  else
    {
      glade_widget_property_set_sensitive (widget, "entry-text-column", FALSE, NO_ENTRY_MSG);
      glade_widget_property_set_sensitive (widget, "has-frame", FALSE, NO_ENTRY_MSG);
    }
}

void
glade_gtk_combo_box_set_property (GladeWidgetAdaptor *adaptor,
                                  GObject            *object,
                                  const gchar        *id,
                                  const GValue       *value)
{
  if (!strcmp (id, "entry-text-column"))
    {
      /* Avoid warnings */
      if (g_value_get_int (value) >= 0)
        GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor,
                                                          object, id, value);
    }
  else if (!strcmp (id, "text-column"))
    {
      if (g_value_get_int (value) >= 0)
        gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (object),
                                             g_value_get_int (value));
    }
  else if (!strcmp (id, "add-tearoffs"))
    {
      GladeWidget *widget = glade_widget_get_from_gobject (object);

      if (g_value_get_boolean (value))
        glade_widget_property_set_sensitive (widget, "tearoff-title", TRUE, NULL);
      else
        glade_widget_property_set_sensitive (widget, "tearoff-title", FALSE,
                                             _("Tearoff menus are disabled"));
    }
  else
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor,
                                                      object, id, value);
}

GList *
glade_gtk_combo_box_get_children (GladeWidgetAdaptor *adaptor,
                                  GtkComboBox        *combo)
{
  GList *list = NULL;

  list = glade_gtk_cell_layout_get_children (adaptor, G_OBJECT (combo));

  if (gtk_combo_box_get_has_entry (combo))
    list = g_list_append (list, gtk_bin_get_child (GTK_BIN (combo)));

  return list;
}
