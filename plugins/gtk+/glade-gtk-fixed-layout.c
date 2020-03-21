/*
 * glade-gtk-fixed-layout.c - GladeWidgetAdaptor for GtkFixed and GtkLayout
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

#include "glade-layout-editor.h"

GladeEditable *
glade_gtk_layout_create_editable (GladeWidgetAdaptor *adaptor,
                                  GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    {
      return (GladeEditable *)glade_layout_editor_new ();
    }

  return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}

static void
glade_gtk_fixed_layout_sync_size_requests (GtkWidget *widget)
{
  GList *children, *l;

  if ((children = gtk_container_get_children (GTK_CONTAINER (widget))) != NULL)
    {
      for (l = children; l; l = l->next)
        {
          GtkWidget *child = l->data;
          GladeWidget *gchild = glade_widget_get_from_gobject (child);
          gint width = -1, height = -1;

          if (!gchild)
            continue;

          glade_widget_property_get (gchild, "width-request", &width);
          glade_widget_property_get (gchild, "height-request", &height);

          gtk_widget_set_size_request (child, width, height);

        }
      g_list_free (children);
    }
}

static cairo_pattern_t *
get_fixed_layout_pattern (void)
{
  static cairo_pattern_t *static_pattern = NULL;

  if (!static_pattern)
    {
      gchar *path = g_build_filename (glade_app_get_pixmaps_dir (), "fixed-bg.png", NULL);
      cairo_surface_t *surface = 
        cairo_image_surface_create_from_png (path);

      if (surface)
        {
          static_pattern = cairo_pattern_create_for_surface (surface);
          cairo_pattern_set_extend (static_pattern, CAIRO_EXTEND_REPEAT);
        }
      else 
        g_warning ("Failed to create surface for %s\n", path);

      g_free (path);
    }
  return static_pattern;
}

static void
glade_gtk_fixed_layout_draw (GtkWidget *widget, cairo_t *cr)
{
  GtkAllocation allocation;

  gtk_widget_get_allocation (widget, &allocation);

  cairo_save (cr);

  cairo_rectangle (cr, 0, 0, allocation.width, allocation.height);
  cairo_set_source (cr, get_fixed_layout_pattern ());
  cairo_fill (cr);

  cairo_restore (cr);
}

void
glade_gtk_fixed_layout_post_create (GladeWidgetAdaptor *adaptor,
                                    GObject            *object,
                                    GladeCreateReason   reason)
{
  /* Set a minimum size so you can actually see it if you added to a box */
  gtk_widget_set_size_request (GTK_WIDGET (object), 32, 32);

  gtk_widget_set_has_window (GTK_WIDGET (object), TRUE);
  
  /* Sync up size request at project load time */
  if (reason == GLADE_CREATE_LOAD)
    g_signal_connect_after (object, "realize",
                            G_CALLBACK
                            (glade_gtk_fixed_layout_sync_size_requests), NULL);

  g_signal_connect (object, "draw",
                    G_CALLBACK (glade_gtk_fixed_layout_draw), NULL);
}

void
glade_gtk_fixed_layout_add_child (GladeWidgetAdaptor *adaptor,
                                  GObject            *object,
                                  GObject            *child)
{
  g_return_if_fail (GTK_IS_CONTAINER (object));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));
}

void
glade_gtk_fixed_layout_remove_child (GladeWidgetAdaptor *adaptor,
                                     GObject            *object,
                                     GObject            *child)
{
  g_return_if_fail (GTK_IS_CONTAINER (object));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
}
