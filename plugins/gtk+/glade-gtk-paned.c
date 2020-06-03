/*
 * glade-gtk-paned.c - GladeWidgetAdaptor for GtkPaned
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

G_MODULE_EXPORT void
glade_gtk_paned_post_create (GladeWidgetAdaptor * adaptor,
                             GObject * paned, GladeCreateReason reason)
{
  g_return_if_fail (GTK_IS_PANED (paned));

  if (reason == GLADE_CREATE_USER &&
      gtk_paned_get_child1 (GTK_PANED (paned)) == NULL)
    gtk_paned_add1 (GTK_PANED (paned), glade_placeholder_new ());

  if (reason == GLADE_CREATE_USER &&
      gtk_paned_get_child2 (GTK_PANED (paned)) == NULL)
    gtk_paned_add2 (GTK_PANED (paned), glade_placeholder_new ());
}

G_MODULE_EXPORT void
glade_gtk_paned_add_child (GladeWidgetAdaptor * adaptor,
                           GObject * object, GObject * child)
{
  GtkPaned *paned;
  GtkWidget *child1, *child2;
  gboolean loading;

  g_return_if_fail (GTK_IS_PANED (object));

  paned = GTK_PANED (object);
  loading = glade_util_object_is_loading (object);

  child1 = gtk_paned_get_child1 (paned);
  child2 = gtk_paned_get_child2 (paned);

  if (loading == FALSE)
    {
      /* Remove a placeholder */
      if (child1 && GLADE_IS_PLACEHOLDER (child1))
        {
          gtk_container_remove (GTK_CONTAINER (object), child1);
          child1 = NULL;
        }
      else if (child2 && GLADE_IS_PLACEHOLDER (child2))
        {
          gtk_container_remove (GTK_CONTAINER (object), child2);
          child2 = NULL;
        }
    }

  /* Add the child */
  if (child1 == NULL)
    gtk_paned_add1 (paned, GTK_WIDGET (child));
  else if (child2 == NULL)
    gtk_paned_add2 (paned, GTK_WIDGET (child));

  if (GLADE_IS_PLACEHOLDER (child) == FALSE && loading)
    {
      GladeWidget *gchild = glade_widget_get_from_gobject (child);

      if (gchild && glade_widget_get_packing_properties (gchild))
        {
          if (child1 == NULL)
            glade_widget_pack_property_set (gchild, "first", TRUE);
          else if (child2 == NULL)
            glade_widget_pack_property_set (gchild, "first", FALSE);
        }
    }
}

G_MODULE_EXPORT void
glade_gtk_paned_remove_child (GladeWidgetAdaptor * adaptor,
                              GObject * object, GObject * child)
{
  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));

  glade_gtk_paned_post_create (adaptor, object, GLADE_CREATE_USER);
}

G_MODULE_EXPORT void
glade_gtk_paned_set_child_property (GladeWidgetAdaptor * adaptor,
                                    GObject * container,
                                    GObject * child,
                                    const gchar * property_name,
                                    const GValue * value)
{
  if (strcmp (property_name, "first") == 0)
    {
      GtkPaned *paned = GTK_PANED (container);
      gboolean first = g_value_get_boolean (value);
      GtkWidget *place, *wchild = GTK_WIDGET (child);

      place = (first) ? gtk_paned_get_child1 (paned) :
          gtk_paned_get_child2 (paned);

      if (place && GLADE_IS_PLACEHOLDER (place))
        gtk_container_remove (GTK_CONTAINER (container), place);

      g_object_ref (child);
      gtk_container_remove (GTK_CONTAINER (container), wchild);
      if (first)
        gtk_paned_add1 (paned, wchild);
      else
        gtk_paned_add2 (paned, wchild);
      g_object_unref (child);

      /* Ensure placeholders */
      if (glade_util_object_is_loading (child) == FALSE)
        {
          if ((place = gtk_paned_get_child1 (paned)) == NULL)
            gtk_paned_add1 (paned, glade_placeholder_new ());

          if ((place = gtk_paned_get_child2 (paned)) == NULL)
            gtk_paned_add2 (paned, glade_placeholder_new ());
        }
    }
  else
    /* Chain Up */
    GWA_GET_CLASS
        (GTK_TYPE_CONTAINER)->child_set_property (adaptor,
                                                  container, child,
                                                  property_name, value);
}

G_MODULE_EXPORT void
glade_gtk_paned_get_child_property (GladeWidgetAdaptor * adaptor,
                                    GObject * container,
                                    GObject * child,
                                    const gchar * property_name, GValue * value)
{
  if (strcmp (property_name, "first") == 0)
    g_value_set_boolean (value, GTK_WIDGET (child) ==
                         gtk_paned_get_child1 (GTK_PANED (container)));
  else
    /* Chain Up */
    GWA_GET_CLASS
        (GTK_TYPE_CONTAINER)->child_get_property (adaptor,
                                                  container, child,
                                                  property_name, value);
}
