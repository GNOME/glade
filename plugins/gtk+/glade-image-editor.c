/*
 * Copyright (C) 2008 Tristan Van Berkom.
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
 * Authors:
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#include <config.h>
#include <gladeui/glade.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#include "glade-image-editor.h"


static void glade_image_editor_finalize (GObject * object);

static void glade_image_editor_editable_init (GladeEditableIface * iface);

static void glade_image_editor_grab_focus (GtkWidget * widget);

static GladeEditableIface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeImageEditor, glade_image_editor, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_image_editor_editable_init));


static void
glade_image_editor_class_init (GladeImageEditorClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = glade_image_editor_finalize;
  widget_class->grab_focus = glade_image_editor_grab_focus;
}

static void
glade_image_editor_init (GladeImageEditor * self)
{
}

static void
glade_image_editor_load (GladeEditable * editable, GladeWidget * widget)
{
  GladeImageEditor *image_editor = GLADE_IMAGE_EDITOR (editable);
  GladeImageEditMode image_mode = 0;
  GList *l;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  /* load the embedded editable... */
  if (image_editor->embed)
    glade_editable_load (GLADE_EDITABLE (image_editor->embed), widget);

  for (l = image_editor->properties; l; l = l->next)
    glade_editor_property_load_by_widget (GLADE_EDITOR_PROPERTY (l->data),
                                          widget);

  if (widget)
    {
      glade_widget_property_get (widget, "image-mode", &image_mode);

      switch (image_mode)
        {
          case GLADE_IMAGE_MODE_STOCK:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (image_editor->stock_radio), TRUE);
            break;
          case GLADE_IMAGE_MODE_ICON:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (image_editor->icon_radio), TRUE);
            break;
          case GLADE_IMAGE_MODE_FILENAME:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (image_editor->file_radio), TRUE);
            break;
          default:
            break;
        }
    }
}

static void
glade_image_editor_set_show_name (GladeEditable * editable, gboolean show_name)
{
  GladeImageEditor *image_editor = GLADE_IMAGE_EDITOR (editable);

  glade_editable_set_show_name (GLADE_EDITABLE (image_editor->embed),
                                show_name);
}

static void
glade_image_editor_editable_init (GladeEditableIface * iface)
{
  parent_editable_iface = g_type_default_interface_peek (GLADE_TYPE_EDITABLE);

  iface->load = glade_image_editor_load;
  iface->set_show_name = glade_image_editor_set_show_name;
}

static void
glade_image_editor_finalize (GObject * object)
{
  GladeImageEditor *image_editor = GLADE_IMAGE_EDITOR (object);

  if (image_editor->properties)
    g_list_free (image_editor->properties);
  image_editor->properties = NULL;
  image_editor->embed = NULL;

  glade_editable_load (GLADE_EDITABLE (object), NULL);

  G_OBJECT_CLASS (glade_image_editor_parent_class)->finalize (object);
}

static void
glade_image_editor_grab_focus (GtkWidget * widget)
{
  GladeImageEditor *image_editor = GLADE_IMAGE_EDITOR (widget);

  gtk_widget_grab_focus (image_editor->embed);
}

static void
table_attach (GtkWidget * table, GtkWidget * child, gint pos, gint row)
{
  gtk_grid_attach (GTK_GRID (table), child, pos, row, 1, 1);

  if (pos)
    gtk_widget_set_hexpand (child, TRUE);
}

static void
set_stock_mode (GladeImageEditor * image_editor)
{
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (image_editor));
  GValue value = { 0, };

  property = glade_widget_get_property (gwidget, "icon-name");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "pixbuf");
  glade_command_set_property (property, NULL);

  property = glade_widget_get_property (gwidget, "stock");
  glade_property_get_default (property, &value);
  glade_command_set_property_value (property, &value);
  g_value_unset (&value);

  property = glade_widget_get_property (gwidget, "image-mode");
  glade_command_set_property (property, GLADE_IMAGE_MODE_STOCK);
}

static void
set_icon_mode (GladeImageEditor * image_editor)
{
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (image_editor));

  property = glade_widget_get_property (gwidget, "stock");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "pixbuf");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "image-mode");
  glade_command_set_property (property, GLADE_IMAGE_MODE_ICON);
}



static void
set_file_mode (GladeImageEditor * image_editor)
{
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (image_editor));

  property = glade_widget_get_property (gwidget, "stock");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "icon-name");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "image-mode");
  glade_command_set_property (property, GLADE_IMAGE_MODE_FILENAME);
}

static void
stock_toggled (GtkWidget * widget, GladeImageEditor * image_editor)
{
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (image_editor));

  if (glade_editable_loading (GLADE_EDITABLE (image_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (image_editor->stock_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (image_editor));

  glade_command_push_group (_("Setting %s to use an image from stock"),
                            glade_widget_get_name (gwidget));
  set_stock_mode (image_editor);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (image_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (image_editor), gwidget);
}


static void
icon_toggled (GtkWidget * widget, GladeImageEditor * image_editor)
{
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (image_editor));

  if (glade_editable_loading (GLADE_EDITABLE (image_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (image_editor->icon_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (image_editor));

  glade_command_push_group (_("Setting %s to use an image from the icon theme"),
                            glade_widget_get_name (gwidget));
  set_icon_mode (image_editor);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (image_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (image_editor), gwidget);
}

static void
file_toggled (GtkWidget * widget, GladeImageEditor * image_editor)
{ 
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (image_editor));

  if (glade_editable_loading (GLADE_EDITABLE (image_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (image_editor->file_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (image_editor));

  glade_command_push_group (_("Setting %s to use an image from filename"),
                            glade_widget_get_name (gwidget));
  set_file_mode (image_editor);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (image_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (image_editor), gwidget);
}


GtkWidget *
glade_image_editor_new (GladeWidgetAdaptor * adaptor, GladeEditable * embed)
{
  GladeImageEditor *image_editor;
  GladeEditorProperty *eprop;
  GtkWidget *table, *frame, *alignment, *label, *hbox;
  gchar *str;

  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
  g_return_val_if_fail (GLADE_IS_EDITABLE (embed), NULL);

  image_editor = g_object_new (GLADE_TYPE_IMAGE_EDITOR, NULL);
  image_editor->embed = GTK_WIDGET (embed);

  /* Pack the parent on top... */
  gtk_box_pack_start (GTK_BOX (image_editor), GTK_WIDGET (embed), FALSE, FALSE,
                      0);

  /* Image content frame... */
  str = g_strdup_printf ("<b>%s</b>", _("Edit Image"));
  label = gtk_label_new (str);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_free (str);
  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (image_editor), frame, FALSE, FALSE, 8);

  alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
  gtk_container_add (GTK_CONTAINER (frame), alignment);

  table = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (table),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), table);

  /* Stock image... */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "stock", FALSE, TRUE);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  image_editor->stock_radio = gtk_radio_button_new (NULL);
  gtk_box_pack_start (GTK_BOX (hbox), image_editor->stock_radio, FALSE, FALSE,
                      2);
  gtk_box_pack_start (GTK_BOX (hbox), glade_editor_property_get_item_label (eprop), TRUE, TRUE, 2);
  table_attach (table, hbox, 0, 0);
  table_attach (table, GTK_WIDGET (eprop), 1, 0);
  image_editor->properties = g_list_prepend (image_editor->properties, eprop);

  /* Icon theme image... */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "icon-name", FALSE,
                                                 TRUE);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  image_editor->icon_radio = gtk_radio_button_new_from_widget
      (GTK_RADIO_BUTTON (image_editor->stock_radio));
  gtk_box_pack_start (GTK_BOX (hbox), image_editor->icon_radio, FALSE, FALSE,
                      2);
  gtk_box_pack_start (GTK_BOX (hbox), glade_editor_property_get_item_label (eprop), TRUE, TRUE, 2);
  table_attach (table, hbox, 0, 1);
  table_attach (table, GTK_WIDGET (eprop), 1, 1);
  image_editor->properties = g_list_prepend (image_editor->properties, eprop);

  /* Filename... */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "pixbuf", FALSE,
                                                 TRUE);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  image_editor->file_radio = gtk_radio_button_new_from_widget
      (GTK_RADIO_BUTTON (image_editor->stock_radio));
  gtk_box_pack_start (GTK_BOX (hbox), image_editor->file_radio, FALSE, FALSE,
                      2);
  gtk_box_pack_start (GTK_BOX (hbox), glade_editor_property_get_item_label (eprop), TRUE, TRUE, 2);
  table_attach (table, hbox, 0, 2);
  table_attach (table, GTK_WIDGET (eprop), 1, 2);
  image_editor->properties = g_list_prepend (image_editor->properties, eprop);

  /* Image size frame... */
  str = g_strdup_printf ("<b>%s</b>", _("Set Image Size"));
  label = gtk_label_new (str);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_free (str);
  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (image_editor), frame, FALSE, FALSE, 8);

  alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
  gtk_container_add (GTK_CONTAINER (frame), alignment);

  table = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (table),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), table);

  /* Icon Size... */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "icon-size", FALSE,
                                                 TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 0);
  table_attach (table, GTK_WIDGET (eprop), 1, 0);
  image_editor->properties = g_list_prepend (image_editor->properties, eprop);

  /* Pixel Size... */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "pixel-size", FALSE,
                                                 TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 1);
  table_attach (table, GTK_WIDGET (eprop), 1, 1);
  image_editor->properties = g_list_prepend (image_editor->properties, eprop);

  /* Connect radio button signals... */
  g_signal_connect (G_OBJECT (image_editor->stock_radio), "toggled",
                    G_CALLBACK (stock_toggled), image_editor);
  g_signal_connect (G_OBJECT (image_editor->icon_radio), "toggled",
                    G_CALLBACK (icon_toggled), image_editor);
  g_signal_connect (G_OBJECT (image_editor->file_radio), "toggled",
                    G_CALLBACK (file_toggled), image_editor);

  gtk_widget_show_all (GTK_WIDGET (image_editor));

  return GTK_WIDGET (image_editor);
}
