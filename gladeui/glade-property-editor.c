/*
 * glade-property-editor.c
 *
 * Copyright (C) 2013 Tristan Van Berkom.
 *
 * Authors:
 *   Tristan Van Berkom <tvb@gnome.org>
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include <string.h>
#include <stdlib.h>

#include "glade-widget.h"
#include "glade-property-editor.h"

G_DEFINE_INTERFACE (GladePropertyEditor, glade_property_editor, GTK_TYPE_WIDGET);

static void
glade_property_editor_default_init (GladePropertyEditorInterface *iface)
{
}

/**
 * glade_property_editor_load:
 * @editor: A #GladePropertyEditor
 * @widget: the #GladeWidget to load
 *
 * Loads @editor from @widget
 */
void
glade_property_editor_load (GladePropertyEditor *editor,
			    GladeWidget         *widget)
{
  GladePropertyEditorInterface *iface;

  g_return_if_fail (GLADE_IS_PROPERTY_EDITOR (editor));
  g_return_if_fail (widget == NULL || GLADE_IS_WIDGET (widget));

  iface = GLADE_PROPERTY_EDITOR_GET_IFACE (editor);

  if (iface->load)
    iface->load (editor, widget);
  else
    g_critical ("No GladePropertyEditor::load_by_widget() support on type %s",
                G_OBJECT_TYPE_NAME (editor));
}
