/*
 * glade-gtk-overlay.c - GladeWidgetAdaptor for GtkOverlay widget
 *
 * Copyright (C) 2013 Juan Pablo Ugarte
 *
 * Authors:
 *      Juan Pablo Ugarte <juanpablougarte@gmail.com>
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

#include "glade-gtk.h"
#include <gladeui/glade.h>
#include <glib/gi18n-lib.h>
#include <string.h>

typedef struct
{
  GtkWidget *bin_child;
  GtkWidget *overlay_child;
} VisibilityData;

static void 
set_children_visibility (GtkWidget *widget, gpointer user_data)
{
  VisibilityData *data = user_data;

  /* We never hide the main child */
  if (widget == data->bin_child)
    return;

  /* We only hide overlay children when one of them is selected */
  if (data->overlay_child)
    gtk_widget_set_visible (widget, widget == data->overlay_child);
  else
    /* otherwhise we show them so we can interact with the container */
    gtk_widget_set_visible (widget, TRUE);
}

static inline GtkWidget *
get_overlay_children (GtkWidget *deep_child, GtkWidget *overlay)
{
  GtkWidget *parent = deep_child;
  
  while (parent)
    {
      if (parent == overlay)
        return deep_child;

      deep_child = parent;
      parent = gtk_widget_get_parent (parent);
    }

  return NULL;
}

static void
on_project_selection_changed (GladeProject *project, GtkWidget *overlay)
{
  VisibilityData data = { gtk_bin_get_child (GTK_BIN (overlay)), NULL };
  GList *l;
  
  for (l = glade_project_selection_get (project); l; l = g_list_next (l))
    {
      GtkWidget *selection;
      
      if (GTK_IS_WIDGET (l->data) && (selection = GTK_WIDGET (l->data)) &&
          overlay != selection)
        data.overlay_child = get_overlay_children (selection, overlay);

      if (data.overlay_child)
        break;
    }

  gtk_container_foreach (GTK_CONTAINER (overlay), set_children_visibility, &data);
}

static void 
on_widget_project_notify (GObject *gobject,
                          GParamSpec *pspec,
                          GladeProject *old_project)
{
  GladeWidget *gwidget = GLADE_WIDGET (gobject);
  GladeProject *project = glade_widget_get_project (gwidget);
  GObject *object = glade_widget_get_object (gwidget);

  if (old_project)
    g_signal_handlers_disconnect_by_func (old_project, on_project_selection_changed, object);

  g_signal_handlers_disconnect_by_func (gwidget, on_widget_project_notify, old_project);
  
  g_signal_connect_object (gwidget, "notify::project",
                           G_CALLBACK (on_widget_project_notify),
                           project, 0);

  if (project)
    g_signal_connect_object (project, "selection-changed",
                             G_CALLBACK (on_project_selection_changed),
                             object, 0);
}

void
glade_gtk_overlay_post_create (GladeWidgetAdaptor *adaptor,
                               GObject            *object,
                               GladeCreateReason   reason)
{
  GladeWidget *widget = glade_widget_get_from_gobject (object);

  if (reason == GLADE_CREATE_USER)
    gtk_container_add (GTK_CONTAINER (object), glade_placeholder_new ());

  on_widget_project_notify (G_OBJECT (widget), NULL, NULL);
}

gboolean
glade_gtk_overlay_add_verify (GladeWidgetAdaptor *adaptor,
                              GtkWidget          *container,
                              GtkWidget          *child,
                              gboolean            user_feedback)
{
  if (!GTK_IS_WIDGET (child))
    {
      if (user_feedback)
        {
          GladeWidgetAdaptor *widget_adaptor = 
            glade_widget_adaptor_get_by_type (GTK_TYPE_WIDGET);

          glade_util_ui_message (glade_app_get_window (),
                                 GLADE_UI_INFO, NULL,
                                 ONLY_THIS_GOES_IN_THAT_MSG,
                                 glade_widget_adaptor_get_title (widget_adaptor),
                                 glade_widget_adaptor_get_title (adaptor));
        }

      return FALSE;
    }

  return TRUE;
}

void
glade_gtk_overlay_add_child (GladeWidgetAdaptor *adaptor,
                             GObject            *object,
                             GObject            *child)
{
  gchar *special_type = g_object_get_data (child, "special-child-type");
  GtkWidget *bin_child;

  if ((special_type && !strcmp (special_type, "overlay")) ||
      ((bin_child = gtk_bin_get_child (GTK_BIN (object))) &&
       !GLADE_IS_PLACEHOLDER (bin_child)))
    {
      g_object_set_data (child, "special-child-type", "overlay");
      gtk_overlay_add_overlay (GTK_OVERLAY (object), GTK_WIDGET (child));
    }
  else
    /* Chain Up */
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (GTK_TYPE_CONTAINER)->add (adaptor, object, child);
}

void
glade_gtk_overlay_remove_child (GladeWidgetAdaptor *adaptor,
                                GObject            *object,
                                GObject            *child)
{
  gchar *special_type = g_object_get_data (child, "special-child-type");

  if (special_type && !strcmp (special_type, "overlay"))
    {
      g_object_set_data (child, "special-child-type", NULL);
      gtk_widget_show (GTK_WIDGET (child));
    }

  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
  
  if (gtk_bin_get_child (GTK_BIN (object)) == NULL)
    gtk_container_add (GTK_CONTAINER (object), glade_placeholder_new ());
}
