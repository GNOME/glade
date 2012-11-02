/*
 * glade-gtk-switch.c
 *
 * Copyright (C) 2011 Juan Pablo Ugarte.
 *
 * Author:
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
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
 */

#include <config.h>
#include "glade-gtk-activatable.h"
#include "glade-activatable-editor.h"

GladeEditable *
glade_gtk_switch_create_editable (GladeWidgetAdaptor *adaptor,
                                  GladeEditorPageType type)
{
  GladeEditable *editable;

  /* Get base editable */
  editable = GWA_GET_CLASS (GTK_TYPE_WIDGET)->create_editable (adaptor, type);

  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_activatable_editor_new (adaptor, editable);

  return editable;
}

void
glade_gtk_switch_post_create (GladeWidgetAdaptor *adaptor,
                              GObject *widget,
                              GladeCreateReason reason)
{
  GladeWidget *gwidget;
  
  if (reason != GLADE_CREATE_LOAD) return;

  g_return_if_fail (GTK_IS_SWITCH (widget));
  gwidget = glade_widget_get_from_gobject (widget);
  g_return_if_fail (GLADE_IS_WIDGET (gwidget));

  g_signal_connect (glade_widget_get_project (gwidget), "parse-finished",
                    G_CALLBACK (glade_gtk_activatable_parse_finished),
                    gwidget);
}

void
glade_gtk_switch_set_property (GladeWidgetAdaptor *adaptor,
                               GObject *object,
                               const gchar *id,
                               const GValue *value)
{
  glade_gtk_activatable_evaluate_property_sensitivity (object, id, value);

  GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id, value);
}
