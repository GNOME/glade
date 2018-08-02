/*
 * glade-gtk-container.c - GladeWidgetAdaptor for GtkContainer
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
#include <string.h>

void
glade_gtk_container_post_create (GladeWidgetAdaptor *adaptor,
                                 GObject            *container,
                                 GladeCreateReason   reason)
{
  GList *children;
  g_return_if_fail (GTK_IS_CONTAINER (container));

  if (reason == GLADE_CREATE_USER)
    {
      if ((children =
           gtk_container_get_children (GTK_CONTAINER (container))) == NULL)
        gtk_container_add (GTK_CONTAINER (container), glade_placeholder_new ());
      else
        g_list_free (children);
    }
}

gboolean
glade_gtk_container_add_verify (GladeWidgetAdaptor *adaptor,
                                GtkWidget          *container,
                                GtkWidget          *child,
                                gboolean            user_feedback)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (container);

  if (GTK_IS_WINDOW (child))
    {
      if (user_feedback)
        glade_util_ui_message (glade_app_get_window (),
                               GLADE_UI_INFO, NULL,
                               _("Cannot add a toplevel window to a container."));

      return FALSE;
    }
  else if (GTK_IS_POPOVER (child))
    {
      if (user_feedback)
        glade_util_ui_message (glade_app_get_window (),
                               GLADE_UI_INFO, NULL,
                               _("Cannot add a popover to a container."));

      return FALSE;
    }
  else if (!GTK_IS_WIDGET (child) ||
           GTK_IS_TOOL_ITEM (child) ||
           GTK_IS_MENU_ITEM (child))
    {
      if (user_feedback)
        glade_util_ui_message (glade_app_get_window (),
                               GLADE_UI_INFO, NULL,
                               _("Widgets of type %s can only have widgets as children."),
                               glade_widget_adaptor_get_title (adaptor));

      return FALSE;
    }
  else if (GWA_USE_PLACEHOLDERS (adaptor) &&
           glade_util_count_placeholders (gwidget) == 0)
    {
      if (user_feedback)
        glade_util_ui_message (glade_app_get_window (),
                               GLADE_UI_INFO, NULL,
                               _("Widgets of type %s need placeholders to add children."),
                               glade_widget_adaptor_get_title (adaptor));

      return FALSE;
    }

  return TRUE;
}

void
glade_gtk_container_replace_child (GladeWidgetAdaptor *adaptor,
                                   GtkWidget          *container,
                                   GtkWidget          *current,
                                   GtkWidget          *new_widget)
{
  GParamSpec **param_spec;
  GladePropertyClass *pclass;
  GValue *value;
  guint nproperties;
  guint i;

  g_return_if_fail (gtk_widget_get_parent (current) == container);

  param_spec = gtk_container_class_list_child_properties
      (G_OBJECT_GET_CLASS (container), &nproperties);
  value = g_malloc0 (sizeof (GValue) * nproperties);

  for (i = 0; i < nproperties; i++)
    {
      g_value_init (&value[i], param_spec[i]->value_type);
      gtk_container_child_get_property
          (GTK_CONTAINER (container), current, param_spec[i]->name, &value[i]);
    }

  gtk_container_remove (GTK_CONTAINER (container), current);
  gtk_container_add (GTK_CONTAINER (container), new_widget);

  for (i = 0; i < nproperties; i++)
    {
      /* If the added widget is a placeholder then we
       * want to keep all the "tranfer-on-paste" properties
       * as default so that it looks fresh (transfer-on-paste
       * properties dont effect the position/slot inside a 
       * contianer)
       */
      if (GLADE_IS_PLACEHOLDER (new_widget))
        {
          pclass = glade_widget_adaptor_get_pack_property_class
              (adaptor, param_spec[i]->name);

          if (pclass && glade_property_class_transfer_on_paste (pclass))
            continue;
        }

      gtk_container_child_set_property
          (GTK_CONTAINER (container), new_widget, param_spec[i]->name,
           &value[i]);
    }

  for (i = 0; i < nproperties; i++)
    g_value_unset (&value[i]);

  g_free (param_spec);
  g_free (value);
}

void
glade_gtk_container_add_child (GladeWidgetAdaptor *adaptor,
                               GtkWidget          *container,
                               GtkWidget          *child)
{
  GtkWidget *container_child = NULL;

  if (GTK_IS_BIN (container))
    container_child = gtk_bin_get_child (GTK_BIN (container));

  /* Get a placeholder out of the way before adding the child if its a GtkBin
   */
  if (GTK_IS_BIN (container) && container_child != NULL &&
      GLADE_IS_PLACEHOLDER (container_child))
    gtk_container_remove (GTK_CONTAINER (container), container_child);

  gtk_container_add (GTK_CONTAINER (container), child);
}

void
glade_gtk_container_remove_child (GladeWidgetAdaptor *adaptor,
                                  GtkWidget          *container,
                                  GtkWidget          *child)
{
  GList *children;
  gtk_container_remove (GTK_CONTAINER (container), child);

  /* If this is the last one, add a placeholder by default.
   */
  if ((children =
       gtk_container_get_children (GTK_CONTAINER (container))) == NULL)
    {
      gtk_container_add (GTK_CONTAINER (container), glade_placeholder_new ());
    }
  else
    g_list_free (children);
}

void
glade_gtk_container_set_child_property (GladeWidgetAdaptor *adaptor,
                                        GObject            *container,
                                        GObject            *child,
                                        const gchar        *property_name,
                                        const GValue       *value)
{
  if (gtk_widget_get_parent (GTK_WIDGET (child)) == GTK_WIDGET (container))
    gtk_container_child_set_property (GTK_CONTAINER (container),
                                      GTK_WIDGET (child), property_name, value);
}

void
glade_gtk_container_get_child_property (GladeWidgetAdaptor *adaptor,
                                        GObject            *container,
                                        GObject            *child,
                                        const gchar        *property_name,
                                        GValue             *value)
{
  if (gtk_widget_get_parent (GTK_WIDGET (child)) == GTK_WIDGET (container))
    gtk_container_child_get_property (GTK_CONTAINER (container),
                                      GTK_WIDGET (child), property_name, value);
}

GList *
glade_gtk_container_get_children (GladeWidgetAdaptor *adaptor,
                                  GObject            *container)
{
  GList *parent_children, *children;

  children = glade_util_container_get_all_children (GTK_CONTAINER (container));
  
  /* Chain up */
  if (GWA_GET_CLASS (GTK_TYPE_WIDGET)->get_children)
    parent_children = GWA_GET_CLASS (GTK_TYPE_WIDGET)->get_children (adaptor, container);
  else
    parent_children = NULL;
  
  return glade_util_purify_list (g_list_concat (children, parent_children));
}

/* This is used in the XML for some derived classes */
GladeEditable *
glade_gtk_container_create_editable (GladeWidgetAdaptor *adaptor,
                                     GladeEditorPageType type)
{
  return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}
