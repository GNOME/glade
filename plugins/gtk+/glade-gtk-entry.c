/*
 * glade-gtk-entry.c - GladeWidgetAdaptor for GtkEntry
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

#include "glade-entry-editor.h"
#include "glade-image-editor.h" /* For the icon modes */
#include "glade-gtk.h"

static void
glade_gtk_entry_changed (GtkEditable *editable, GladeWidget *gentry)
{
  const gchar *text, *text_prop;
  GladeProperty *prop;
  gboolean use_buffer;

  if (glade_widget_superuser ())
    return;

  text = gtk_entry_get_text (GTK_ENTRY (editable));

  glade_widget_property_get (gentry, "text", &text_prop);
  glade_widget_property_get (gentry, "use-entry-buffer", &use_buffer);

  if (use_buffer == FALSE && g_strcmp0 (text, text_prop))
    {
      if ((prop = glade_widget_get_property (gentry, "text")))
        glade_command_set_property (prop, text);
    }
}

void
glade_gtk_entry_post_create (GladeWidgetAdaptor *adaptor,
                             GObject            *object,
                             GladeCreateReason   reason)
{
  GladeWidget *gentry;

  g_return_if_fail (GTK_IS_ENTRY (object));
  gentry = glade_widget_get_from_gobject (object);
  g_return_if_fail (GLADE_IS_WIDGET (gentry));

  g_signal_connect (object, "changed",
                    G_CALLBACK (glade_gtk_entry_changed), gentry);
}

GladeEditable *
glade_gtk_entry_create_editable (GladeWidgetAdaptor *adaptor,
                                 GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_entry_editor_new ();
  else
    return GWA_GET_CLASS (GTK_TYPE_WIDGET)->create_editable (adaptor, type);
}

void
glade_gtk_entry_set_property (GladeWidgetAdaptor *adaptor,
                              GObject            *object,
                              const gchar        *id,
                              const GValue       *value)
{
  GladeImageEditMode mode;
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);
  GladeProperty *property = glade_widget_get_property (gwidget, id);

  if (!strcmp (id, "use-entry-buffer"))
    {
      glade_widget_property_set_sensitive (gwidget, "text", FALSE,
                                           NOT_SELECTED_MSG);
      glade_widget_property_set_sensitive (gwidget, "buffer", FALSE,
                                           NOT_SELECTED_MSG);

      if (g_value_get_boolean (value))
        glade_widget_property_set_sensitive (gwidget, "buffer", TRUE, NULL);
      else
        glade_widget_property_set_sensitive (gwidget, "text", TRUE, NULL);
    }
  else if (!strcmp (id, "primary-icon-mode"))
    {
      mode = g_value_get_int (value);

      glade_widget_property_set_sensitive (gwidget, "primary-icon-stock", FALSE,
                                           NOT_SELECTED_MSG);
      glade_widget_property_set_sensitive (gwidget, "primary-icon-name", FALSE,
                                           NOT_SELECTED_MSG);
      glade_widget_property_set_sensitive (gwidget, "primary-icon-pixbuf",
                                           FALSE, NOT_SELECTED_MSG);

      switch (mode)
        {
          case GLADE_IMAGE_MODE_STOCK:
            glade_widget_property_set_sensitive (gwidget, "primary-icon-stock",
                                                 TRUE, NULL);
            break;
          case GLADE_IMAGE_MODE_ICON:
            glade_widget_property_set_sensitive (gwidget, "primary-icon-name",
                                                 TRUE, NULL);
            break;
          case GLADE_IMAGE_MODE_FILENAME:
            glade_widget_property_set_sensitive (gwidget, "primary-icon-pixbuf",
                                                 TRUE, NULL);
            break;
          case GLADE_IMAGE_MODE_RESOURCE:
            /* Doesnt apply for entry icons */
            break;
        }
    }
  else if (!strcmp (id, "secondary-icon-mode"))
    {
      mode = g_value_get_int (value);

      glade_widget_property_set_sensitive (gwidget, "secondary-icon-stock",
                                           FALSE, NOT_SELECTED_MSG);
      glade_widget_property_set_sensitive (gwidget, "secondary-icon-name",
                                           FALSE, NOT_SELECTED_MSG);
      glade_widget_property_set_sensitive (gwidget, "secondary-icon-pixbuf",
                                           FALSE, NOT_SELECTED_MSG);

      switch (mode)
        {
          case GLADE_IMAGE_MODE_STOCK:
            glade_widget_property_set_sensitive (gwidget,
                                                 "secondary-icon-stock", TRUE,
                                                 NULL);
            break;
          case GLADE_IMAGE_MODE_ICON:
            glade_widget_property_set_sensitive (gwidget, "secondary-icon-name",
                                                 TRUE, NULL);
            break;
          case GLADE_IMAGE_MODE_FILENAME:
            glade_widget_property_set_sensitive (gwidget,
                                                 "secondary-icon-pixbuf", TRUE,
                                                 NULL);
            break;
          case GLADE_IMAGE_MODE_RESOURCE:
            /* Doesnt apply for entry icons */
            break;
        }
    }
  else if (!strcmp (id, "primary-icon-tooltip-text") ||
           !strcmp (id, "primary-icon-tooltip-markup"))
    {
      /* Avoid a silly crash in GTK+ */
      if (gtk_entry_get_icon_storage_type (GTK_ENTRY (object),
                                           GTK_ENTRY_ICON_PRIMARY) != GTK_IMAGE_EMPTY)
        GWA_GET_CLASS (GTK_TYPE_WIDGET)->set_property (adaptor, object, id, value);
    }
  else if (!strcmp (id, "secondary-icon-tooltip-text") ||
           !strcmp (id, "secondary-icon-tooltip-markup"))
    {
      /* Avoid a silly crash in GTK+ */
      if (gtk_entry_get_icon_storage_type (GTK_ENTRY (object),
                                           GTK_ENTRY_ICON_SECONDARY) != GTK_IMAGE_EMPTY)
        GWA_GET_CLASS (GTK_TYPE_WIDGET)->set_property (adaptor, object, id, value);
    }
  else if (!strcmp (id, "text"))
    {
      g_signal_handlers_block_by_func (object, glade_gtk_entry_changed,
                                       gwidget);

      if (g_value_get_string (value))
        gtk_entry_set_text (GTK_ENTRY (object), g_value_get_string (value));
      else
        gtk_entry_set_text (GTK_ENTRY (object), "");

      g_signal_handlers_unblock_by_func (object, glade_gtk_entry_changed,
                                         gwidget);
    }
  else if (!strcmp (id, "has-frame"))
    {
      if (g_value_get_boolean (value))
        glade_widget_property_set_sensitive (gwidget, "shadow-type", TRUE, NULL);
      else
        glade_widget_property_set_sensitive (gwidget, "shadow-type", FALSE,
                                             _("This property is only available\n"
                                               "if the entry has a frame"));

      GWA_GET_CLASS (GTK_TYPE_WIDGET)->set_property (adaptor, object, id, value);
    }
  else if (!strcmp (id, "visibility"))
    {
      if (g_value_get_boolean (value))
        glade_widget_property_set_sensitive (gwidget, "invisible-char", FALSE,
                                             _("This property is only available\n"
                                               "if the entry characters are invisible"));
      else
        glade_widget_property_set_sensitive (gwidget, "invisible-char", TRUE, NULL);

      GWA_GET_CLASS (GTK_TYPE_WIDGET)->set_property (adaptor, object, id, value);
    }
  else if (GLADE_PROPERTY_DEF_VERSION_CHECK
           (glade_property_get_def (property), gtk_major_version, gtk_minor_version + 1))
    GWA_GET_CLASS (GTK_TYPE_WIDGET)->set_property (adaptor, object, id, value);
}

void
glade_gtk_entry_read_widget (GladeWidgetAdaptor *adaptor,
                             GladeWidget        *widget,
                             GladeXmlNode       *node)
{
  GladeProperty *property;

  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (GTK_TYPE_WIDGET)->read_widget (adaptor, widget, node);

  if (!glade_widget_property_original_default (widget, "text"))
    {
      property = glade_widget_get_property (widget, "text");
      glade_widget_property_set (widget, "use-entry-buffer", FALSE);

      glade_property_sync (property);
    }
  else
    {
      gint target_minor, target_major;

      glade_project_get_target_version (glade_widget_get_project (widget), "gtk+", 
                                        &target_major, &target_minor);

      property = glade_widget_get_property (widget, "buffer");

      /* Only default to the buffer setting if the project version supports it. */
      if (GLADE_PROPERTY_DEF_VERSION_CHECK (glade_property_get_def (property), target_major, target_minor))
        {
          glade_widget_property_set (widget, "use-entry-buffer", TRUE);
          glade_property_sync (property);
        }
      else
        glade_widget_property_set (widget, "use-entry-buffer", FALSE);
    }

  if (!glade_widget_property_original_default (widget, "primary-icon-name"))
    {
      property = glade_widget_get_property (widget, "primary-icon-name");
      glade_widget_property_set (widget, "primary-icon-mode",
                                 GLADE_IMAGE_MODE_ICON);
    }
  else if (!glade_widget_property_original_default (widget, "primary-icon-pixbuf"))
    {
      property = glade_widget_get_property (widget, "primary-icon-pixbuf");
      glade_widget_property_set (widget, "primary-icon-mode",
                                 GLADE_IMAGE_MODE_FILENAME);
    }
  else /*  if (glade_widget_property_original_default (widget, "stock") == FALSE) */
    {
      property = glade_widget_get_property (widget, "primary-icon-stock");
      glade_widget_property_set (widget, "primary-icon-mode",
                                 GLADE_IMAGE_MODE_STOCK);
    }

  glade_property_sync (property);

  if (!glade_widget_property_original_default (widget, "secondary-icon-name"))
    {
      property = glade_widget_get_property (widget, "secondary-icon-name");
      glade_widget_property_set (widget, "secondary-icon-mode",
                                 GLADE_IMAGE_MODE_ICON);
    }
  else if (!glade_widget_property_original_default (widget, "secondary-icon-pixbuf"))
    {
      property = glade_widget_get_property (widget, "secondary-icon-pixbuf");
      glade_widget_property_set (widget, "secondary-icon-mode",
                                 GLADE_IMAGE_MODE_FILENAME);
    }
  else /*  if (glade_widget_property_original_default (widget, "stock") == FALSE) */
    {
      property = glade_widget_get_property (widget, "secondary-icon-stock");
      glade_widget_property_set (widget, "secondary-icon-mode",
                                 GLADE_IMAGE_MODE_STOCK);
    }

  glade_property_sync (property);

  if (!glade_widget_property_original_default (widget, "primary-icon-tooltip-markup"))
    glade_widget_property_set (widget, "glade-primary-tooltip-markup", TRUE);

  if (!glade_widget_property_original_default (widget, "secondary-icon-tooltip-markup"))
    glade_widget_property_set (widget, "glade-secondary-tooltip-markup", TRUE);
}
