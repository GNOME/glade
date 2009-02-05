/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-editable.c
 *
 * Copyright (C) 2008 Tristan Van Berkom.
 *
 * Authors:
 *   Tristan Van Berkom <tvb@gnome.org>
 *
 * This library is free software; you can redistribute it and/or it
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include <string.h>
#include <stdlib.h>


#include "glade-widget.h"
#include "glade-editable.h"


static void
glade_editable_class_init (gpointer g_iface)
{	
	/* */
}

GType
glade_editable_get_type (void)
{
  static GType editable_type = 0;

  if (!editable_type)
    editable_type =
      g_type_register_static_simple (G_TYPE_INTERFACE, "GladeEditable",
				       sizeof (GladeEditableIface),
				       (GClassInitFunc) glade_editable_class_init,
				       0, NULL, (GTypeFlags)0);

  return editable_type;
}

/**
 * glade_editable_load:
 * @editable: A #GladeEditable
 * @widget: the #GladeWidget to load
 *
 * Loads @widget property values into @editable
 * (the editable will watch the widgets properties
 * until its loaded with another widget or %NULL)
 */
void
glade_editable_load (GladeEditable *editable,
		     GladeWidget   *widget)
{
	GladeEditableIface *iface;
	g_return_if_fail (GLADE_IS_EDITABLE (editable));
	g_return_if_fail (widget == NULL || GLADE_IS_WIDGET (widget));

	iface = GLADE_EDITABLE_GET_IFACE (editable);

	if (iface->load)
		iface->load (editable, widget);
	else
		g_critical ("No GladeEditable::load() support on type %s", 
			    G_OBJECT_TYPE_NAME (editable));
}


/**
 * glade_editable_set_show_name:
 * @editable: A #GladeEditable
 * @show_name: Whether or not to show the name entry
 *
 * This only applies for the general page in the property
 * editor, editables that embed the #GladeEditorTable implementation
 * for the general page should use this to forward the message
 * to its embedded editable.
 */
void
glade_editable_set_show_name  (GladeEditable  *editable,
			       gboolean        show_name)
{
	GladeEditableIface *iface;
	g_return_if_fail (GLADE_IS_EDITABLE (editable));

	iface = GLADE_EDITABLE_GET_IFACE (editable);

	if (iface->set_show_name)
		iface->set_show_name (editable, show_name);
}

