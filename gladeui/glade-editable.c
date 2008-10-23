/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-name-context.c
 *
 * Copyright (C) 2008 Tristan Van Berkom.
 *
 * Authors:
 *   Tristan Van Berkom <tvb@gnome.org>
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
