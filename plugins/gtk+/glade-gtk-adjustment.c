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
glade_gtk_adjustment_write_widget (GladeWidgetAdaptor * adaptor,
                                   GladeWidget * widget,
                                   GladeXmlContext * context,
                                   GladeXmlNode * node)
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
