/*
 * glade-gtk-popover.c - GladeWidgetAdaptor for GtkPopover
 *
 * Copyright (C) 2014 Red Hat, Inc
 *
 * Authors:
 *      Matthias Clasen <mclasen@redhat.com>
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

#include "glade-popover-editor.h"

#define GLADE_TYPE_GTK_POPOVER glade_gtk_popover_get_type ()
G_DECLARE_FINAL_TYPE (GladeGtkPopover, glade_gtk_popover, GLADE, GTK_POPOVER, GtkPopover)

struct _GladeGtkPopover
{
  GtkPopover parent_instance;
};

G_DEFINE_TYPE (GladeGtkPopover, glade_gtk_popover, GTK_TYPE_POPOVER)

static void
glade_gtk_popover_init (GladeGtkPopover *popover)
{
  gtk_popover_set_modal (GTK_POPOVER (popover), FALSE);
  gtk_popover_set_relative_to (GTK_POPOVER (popover), NULL);
}

static void
glade_gtk_popover_map (GtkWidget *widget)
{
  GtkWidgetClass *klass = g_type_class_peek_parent (glade_gtk_popover_parent_class);
  klass->map (widget);
}

static void
glade_gtk_popover_unmap (GtkWidget *widget)
{
  GtkWidgetClass *klass = g_type_class_peek_parent (glade_gtk_popover_parent_class);
  klass->unmap (widget);
}

static gboolean
glade_gtk_popover_button_event (GtkWidget *widget, GdkEventButton *event)
{
  return TRUE;
}

static gint
glade_gtk_popover_key_press_event (GtkWidget *popover, GdkEventKey *event)
{
  if (event->keyval == GDK_KEY_Escape)
    return TRUE;

  return FALSE;
}

static void
glade_gtk_popover_class_init (GladeGtkPopoverClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  /* Ignore some events causing the popover to disappear from the workspace */
  widget_class->key_press_event = glade_gtk_popover_key_press_event;
  widget_class->button_press_event = glade_gtk_popover_button_event;
  widget_class->button_release_event = glade_gtk_popover_button_event;

  /* Make some warning go away */
  widget_class->map = glade_gtk_popover_map;
  widget_class->unmap = glade_gtk_popover_unmap;
}


GObject *
glade_gtk_popover_constructor (GType type,
                               guint n_construct_properties,
                               GObjectConstructParam * construct_properties)
{
  GladeWidgetAdaptor *adaptor;
  GObject *ret_obj;

  ret_obj = GWA_GET_OCLASS (GTK_TYPE_CONTAINER)->constructor
      (type, n_construct_properties, construct_properties);

  adaptor = GLADE_WIDGET_ADAPTOR (ret_obj);

  glade_widget_adaptor_action_remove (adaptor, "add_parent");
  glade_widget_adaptor_action_remove (adaptor, "remove_parent");

  return ret_obj;
}

GladeEditable *
glade_gtk_popover_create_editable (GladeWidgetAdaptor * adaptor,
                                   GladeEditorPageType  type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_popover_editor_new ();
  else
    return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}

