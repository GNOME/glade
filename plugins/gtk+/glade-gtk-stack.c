/*
 * glade-gtk-stack.c - GladeWidgetAdaptor for GtkStack
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

void
glade_gtk_stack_post_create (GladeWidgetAdaptor *adaptor,
                             GObject            *container,
                             GladeCreateReason   reason)
{
  if (reason == GLADE_CREATE_USER)
    {
      gtk_stack_add_named (GTK_STACK (container),
                           glade_placeholder_new (),
                           "page0");
    }
}

void
glade_gtk_stack_child_action_activate (GladeWidgetAdaptor * adaptor,
                                       GObject * container,
                                       GObject * object,
                                       const gchar * action_path)
{
  if (!strcmp (action_path, "insert_page_after") ||
      !strcmp (action_path, "insert_page_before"))
    {
      gint position;
      gchar *name;
      GtkWidget *new_widget;

      gtk_container_child_get (GTK_CONTAINER (container), GTK_WIDGET (object),
                               "position", &position, NULL);

      if (!strcmp (action_path, "insert_page_after"))
        position++;

      name = g_strdup_printf ("page%d", position);
      new_widget = glade_placeholder_new ();

      gtk_stack_add_named (GTK_STACK (container), new_widget, name);
      gtk_stack_set_visible_child (GTK_STACK (container), new_widget);

      g_free (name);
    }
  else if (strcmp (action_path, "remove_page") == 0)
    {
      gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET (object)); 
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_action_activate (adaptor,
                                                               container,
                                                               object,
                                                               action_path);
}

