/*
 * glade-gtk-dialog.c - GladeWidgetAdaptor for GtkDialog
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

#include "glade-gtk-action-widgets.h"
#include "glade-gtk-dialog.h"

static void
glade_gtk_stop_emission_POINTER (gpointer instance, gpointer dummy,
                                 gpointer data)
{
  g_signal_stop_emission (instance, GPOINTER_TO_UINT (data), 0);
}

static void
glade_gtk_dialog_stop_offending_signals (GtkWidget * widget)
{
  static gpointer hierarchy = NULL, screen;

  if (hierarchy == NULL)
    {
      hierarchy = GUINT_TO_POINTER (g_signal_lookup ("hierarchy-changed",
                                                     GTK_TYPE_WIDGET));
      screen = GUINT_TO_POINTER (g_signal_lookup ("screen-changed",
                                                  GTK_TYPE_WIDGET));
    }

  g_signal_connect (widget, "hierarchy-changed",
                    G_CALLBACK (glade_gtk_stop_emission_POINTER), hierarchy);
  g_signal_connect (widget, "screen-changed",
                    G_CALLBACK (glade_gtk_stop_emission_POINTER), screen);
}

static void
glade_gtk_file_chooser_forall (GtkWidget * widget, gpointer data)
{
  /* GtkFileChooserWidget packs a GtkFileChooserDefault */
  if (GTK_IS_FILE_CHOOSER_WIDGET (widget))
    gtk_container_forall (GTK_CONTAINER (widget),
                          glade_gtk_file_chooser_default_forall, NULL);
}

G_MODULE_EXPORT void
glade_gtk_dialog_post_create (GladeWidgetAdaptor *adaptor,
                              GObject *object, GladeCreateReason reason)
{
  GladeWidget *widget, *vbox_widget, *actionarea_widget;
  GtkDialog *dialog;

  /* Chain Up first */
  GWA_GET_CLASS (GTK_TYPE_WINDOW)->post_create (adaptor, object, reason);

  g_return_if_fail (GTK_IS_DIALOG (object));

  widget = glade_widget_get_from_gobject (GTK_WIDGET (object));
  if (!widget)
    return;
  
  dialog = GTK_DIALOG (object);
  
  if (reason == GLADE_CREATE_USER)
    {
      /* HIG compliant border-width defaults on dialogs */
      glade_widget_property_set (widget, "border-width", 5);
    }

  vbox_widget = glade_widget_get_from_gobject (gtk_dialog_get_content_area (dialog));
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  actionarea_widget = glade_widget_get_from_gobject (gtk_dialog_get_action_area (dialog));
G_GNUC_END_IGNORE_DEPRECATIONS

  /* We need to stop default emissions of "hierarchy-changed" and 
   * "screen-changed" of GtkFileChooserDefault to avoid an abort()
   * when doing a reparent.
   * GtkFileChooserDialog packs a GtkFileChooserWidget in 
   * his internal vbox.
   */
  if (GTK_IS_FILE_CHOOSER_DIALOG (object))
    gtk_container_forall (GTK_CONTAINER
                          (gtk_dialog_get_content_area (dialog)),
                          glade_gtk_file_chooser_forall, NULL);

  /* These properties are controlled by the GtkDialog style properties:
   * "content-area-border", "button-spacing" and "action-area-border",
   * so we must disable their use.
   */
  glade_widget_remove_property (vbox_widget, "border-width");
  glade_widget_remove_property (actionarea_widget, "border-width");
  glade_widget_remove_property (actionarea_widget, "spacing");

  if (reason == GLADE_CREATE_LOAD || reason == GLADE_CREATE_USER)
    {
      GObject *child;
      gint size;

      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      if (GTK_IS_COLOR_SELECTION_DIALOG (object))
        {
          child = glade_widget_adaptor_get_internal_child (adaptor, object, "color_selection");
          size = 1;
        }
      else if (GTK_IS_FONT_SELECTION_DIALOG (object))
        {
          child = glade_widget_adaptor_get_internal_child (adaptor, object, "font_selection");
          size = 2;
        }
      else
        size = -1;
      G_GNUC_END_IGNORE_DEPRECATIONS;

      /* Set this to a sane value. At load time, if there are any children then
       * size will adjust appropriately (otherwise the default "3" gets
       * set and we end up with extra placeholders).
       */
      if (size > -1)
        glade_widget_property_set (glade_widget_get_from_gobject (child),
                                   "size", size);
    }

  /* Only set these on the original create. */
  if (reason == GLADE_CREATE_USER)
    {
      /* HIG compliant spacing defaults on dialogs */
      glade_widget_property_set (vbox_widget, "spacing", 2);

      if (GTK_IS_ABOUT_DIALOG (object) ||
          GTK_IS_FILE_CHOOSER_DIALOG (object))
        glade_widget_property_set (vbox_widget, "size", 3);
      else
        glade_widget_property_set (vbox_widget, "size", 2);

      glade_widget_property_set (actionarea_widget, "size", 2);
      glade_widget_property_set (actionarea_widget, "layout-style",
                                 GTK_BUTTONBOX_END);
    }
}

G_MODULE_EXPORT void
glade_gtk_dialog_read_child (GladeWidgetAdaptor * adaptor,
                             GladeWidget * widget, GladeXmlNode * node)
{
  GWA_GET_CLASS (GTK_TYPE_CONTAINER)->read_child (adaptor, widget, node);

  node = glade_xml_node_get_parent (node);

  glade_gtk_action_widgets_read_child (widget, node, "action_area");
}

G_MODULE_EXPORT void
glade_gtk_dialog_write_child (GladeWidgetAdaptor * adaptor,
                              GladeWidget * widget,
                              GladeXmlContext * context, GladeXmlNode * node)
{
  GladeWidget *parent = glade_widget_get_parent (widget);

  /* Before writing out the children, force any response id carrying buttons
   * to have a name.
   *
   * This is NOT correct, but is an exception, we force the buttons to have
   * names in a non-undoable way at save time for the purpose of action widgets
   * only.
   */
  glade_gtk_action_widgets_ensure_names (parent, "action_area");
  
  GWA_GET_CLASS (GTK_TYPE_CONTAINER)->write_child (adaptor, widget, context, node);

  if (parent && GTK_IS_DIALOG (glade_widget_get_object (parent)))
    glade_gtk_action_widgets_write_child (parent, context, node, "action_area");
}

/* Shared with file chooser widget */
G_MODULE_EXPORT void
glade_gtk_file_chooser_default_forall (GtkWidget * widget, gpointer data)
{
  /* Since GtkFileChooserDefault is not exposed we check if its a
   * GtkFileChooser
   */
  if (GTK_IS_FILE_CHOOSER (widget))
    {

      /* Finally we can connect to the signals we want to stop its
       * default handler. Since both signals has the same signature
       * we use one callback for both :)
       */
      glade_gtk_dialog_stop_offending_signals (widget);
    }
}
