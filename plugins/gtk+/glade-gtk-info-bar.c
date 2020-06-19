/*
 * glade-gtk-info-bar.c
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
#include <gladeui/glade.h>
#include "glade-gtk-action-widgets.h"

void
glade_gtk_info_bar_read_child (GladeWidgetAdaptor *adaptor,
                               GladeWidget *widget,
                               GladeXmlNode *node)
{
  GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->read_child (adaptor, widget, node);

  node = glade_xml_node_get_parent (node);
  
  glade_gtk_action_widgets_read_child (widget, node, "action_area");
}

void
glade_gtk_info_bar_write_child (GladeWidgetAdaptor *adaptor,
                                GladeWidget *widget,
                                GladeXmlContext *context,
                                GladeXmlNode *node)
{
  GladeWidget *parent = glade_widget_get_parent (widget);

  /* Before writing out the children, force any response id carrying buttons
   * to have a name.
   *
   * This is NOT correct, but is an exception, we force the buttons to have
   * names in a non-undoable way at save time for the purpose of action widgets
   * only.
   */
  if (g_strcmp0 (glade_widget_get_internal (widget), "action_area"))
    glade_gtk_action_widgets_ensure_names (parent, "action_area");

  GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_BOX)->write_child (adaptor, widget, context, node);

  if (g_strcmp0 (glade_widget_get_internal (widget), "action_area"))
    glade_gtk_action_widgets_write_child (parent, context, node, "action_area");
}
