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
 */

#include <config.h>
#include "glade-activatable-editor.h"

GladeEditable *
glade_gtk_switch_create_editable (GladeWidgetAdaptor *adaptor,
                                  GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_activatable_editor_new (adaptor, NULL);

  return GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_WIDGET)->create_editable (adaptor, type);
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
}
