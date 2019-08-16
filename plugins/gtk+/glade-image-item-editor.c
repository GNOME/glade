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

#include "glade-image-item-editor.h"


static void glade_image_item_editor_finalize (GObject *object);

static void glade_image_item_editor_editable_init (GladeEditableInterface *iface);

static void glade_image_item_editor_grab_focus (GtkWidget *widget);

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeImageItemEditor, glade_image_item_editor, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_image_item_editor_editable_init));


static void
glade_image_item_editor_class_init (GladeImageItemEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = glade_image_item_editor_finalize;
  widget_class->grab_focus = glade_image_item_editor_grab_focus;
}

static void
glade_image_item_editor_init (GladeImageItemEditor *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self),
                                  GTK_ORIENTATION_VERTICAL);
}

static GladeWidget *
get_image_widget (GladeWidget *widget)
{
  GtkWidget *image;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (glade_widget_get_object (widget)));
G_GNUC_END_IGNORE_DEPRECATIONS
  return image ? glade_widget_get_from_gobject (image) : NULL;
}

static void
glade_image_item_editor_load (GladeEditable *editable, GladeWidget *widget)
{
  GladeImageItemEditor *item_editor = GLADE_IMAGE_ITEM_EDITOR (editable);
  GladeWidget *image_widget;
  GList *l;
  gboolean use_stock = FALSE;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  /* load the embedded editable... */
  if (item_editor->embed)
    glade_editable_load (GLADE_EDITABLE (item_editor->embed), widget);

  if (item_editor->embed_image)
    {
      /* Finalize safe code here... */
      if (widget && (image_widget = get_image_widget (widget)))
        glade_editable_load (GLADE_EDITABLE (item_editor->embed_image),
                             image_widget);
      else
        glade_editable_load (GLADE_EDITABLE (item_editor->embed_image), NULL);
    }
  for (l = item_editor->properties; l; l = l->next)
    glade_editor_property_load_by_widget (GLADE_EDITOR_PROPERTY (l->data),
                                          widget);

  if (widget)
    {
      glade_widget_property_get (widget, "use-stock", &use_stock);

      gtk_widget_set_sensitive (item_editor->embed_frame, !use_stock);
      gtk_widget_set_sensitive (item_editor->label_frame, !use_stock);

      if (use_stock)
        gtk_toggle_button_set_active
            (GTK_TOGGLE_BUTTON (item_editor->stock_radio), TRUE);
      else
        gtk_toggle_button_set_active
            (GTK_TOGGLE_BUTTON (item_editor->custom_radio), TRUE);
    }
}

static void
glade_image_item_editor_set_show_name (GladeEditable *editable,
                                       gboolean       show_name)
{
  GladeImageItemEditor *item_editor = GLADE_IMAGE_ITEM_EDITOR (editable);

  glade_editable_set_show_name (GLADE_EDITABLE (item_editor->embed), show_name);
}

static void
glade_image_item_editor_editable_init (GladeEditableInterface *iface)
{
  parent_editable_iface = g_type_default_interface_peek (GLADE_TYPE_EDITABLE);

  iface->load = glade_image_item_editor_load;
  iface->set_show_name = glade_image_item_editor_set_show_name;
}

static void
glade_image_item_editor_finalize (GObject *object)
{
  GladeImageItemEditor *item_editor = GLADE_IMAGE_ITEM_EDITOR (object);

  if (item_editor->properties)
    g_list_free (item_editor->properties);
  item_editor->properties = NULL;
  item_editor->embed_image = NULL;
  item_editor->embed = NULL;

  glade_editable_load (GLADE_EDITABLE (object), NULL);

  G_OBJECT_CLASS (glade_image_item_editor_parent_class)->finalize (object);
}

static void
glade_image_item_editor_grab_focus (GtkWidget *widget)
{
  GladeImageItemEditor *item_editor = GLADE_IMAGE_ITEM_EDITOR (widget);

  gtk_widget_grab_focus (item_editor->embed);
}



static void
stock_toggled (GtkWidget *widget, GladeImageItemEditor *item_editor)
{
  GladeProperty *property;
  GladeWidget *image, *loaded;

  loaded = glade_editable_loaded_widget (GLADE_EDITABLE (item_editor));

  if (glade_editable_loading (GLADE_EDITABLE (item_editor)) || !loaded)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (item_editor->stock_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (item_editor));

  glade_command_push_group (_("Setting %s to use a stock item"), glade_widget_get_display_name (loaded));

  property = glade_widget_get_property (loaded, "label");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (loaded, "use-underline");
  glade_command_set_property (property, FALSE);

  /* Delete image... */
  if ((image = get_image_widget (loaded)) != NULL)
    {
      GList list = { 0, };
      list.data = image;
      glade_command_unlock_widget (image);
      glade_command_delete (&list);
      glade_project_selection_set (glade_widget_get_project (loaded), 
                                   glade_widget_get_object (loaded), TRUE);
    }

  property = glade_widget_get_property (loaded, "use-stock");
  glade_command_set_property (property, TRUE);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (item_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (item_editor), loaded);
}

static void
custom_toggled (GtkWidget *widget, GladeImageItemEditor *item_editor)
{
  GladeProperty *property;
  GladeWidgetAdaptor *adaptor;
  GladeWidget *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (item_editor));

  if (glade_editable_loading (GLADE_EDITABLE (item_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (item_editor->custom_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (item_editor));

  adaptor = glade_widget_get_adaptor (gwidget);

  glade_command_push_group (_("Setting %s to use a label and image"),
                            glade_widget_get_display_name (gwidget));

  /* First clear stock...  */
  property = glade_widget_get_property (gwidget, "stock");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "use-stock");
  glade_command_set_property (property, FALSE);

  /* Now setup default label and create image... */
  property = glade_widget_get_property (gwidget, "label");
  glade_command_set_property (property, glade_widget_adaptor_get_generic_name (adaptor));
  property = glade_widget_get_property (gwidget, "use-underline");
  glade_command_set_property (property, FALSE);

  /* There shouldnt be an image widget here... */
  if (!get_image_widget (gwidget))
    {
      /* item_editor->loaded_widget may be set to NULL after the create_command. */
      GladeWidget *loaded = gwidget;
      GladeWidget *image;

      property = glade_widget_get_property (loaded, "image");

      image =
          glade_command_create (glade_widget_adaptor_get_by_type
                                (GTK_TYPE_IMAGE), NULL, NULL,
                                glade_widget_get_project (loaded));

      glade_command_set_property (property, glade_widget_get_object (image));

      /* Make sure nobody deletes this... */
      glade_command_lock_widget (loaded, image);

      /* reload widget by selection ;-) */
      glade_project_selection_set (glade_widget_get_project (loaded), 
                                   glade_widget_get_object (loaded), TRUE);
    }
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (item_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (item_editor), gwidget);
}

static void
table_attach (GtkWidget *table, GtkWidget *child, gint pos, gint row)
{
  gtk_grid_attach (GTK_GRID (table), child, pos, row, 1, 1);

  if (pos)
    gtk_widget_set_hexpand (child, TRUE);
}

GtkWidget *
glade_image_item_editor_new (GladeWidgetAdaptor *adaptor,
                             GladeEditable      *embed)
{
  GladeImageItemEditor *item_editor;
  GladeEditorProperty *eprop;
  GtkWidget *label, *alignment, *frame, *main_table, *table, *vbox;
  gchar *str;

  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
  g_return_val_if_fail (GLADE_IS_EDITABLE (embed), NULL);

  item_editor = g_object_new (GLADE_TYPE_IMAGE_ITEM_EDITOR, NULL);
  item_editor->embed = GTK_WIDGET (embed);

  /* Pack the parent on top... */
  gtk_box_pack_start (GTK_BOX (item_editor), GTK_WIDGET (embed), FALSE, FALSE,
                      0);

  /* Put a radio button to control use-stock here on top... */
  main_table = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (main_table),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (main_table), 4);
  gtk_box_pack_start (GTK_BOX (item_editor), main_table, FALSE, FALSE, 8);

  item_editor->stock_radio =
      gtk_radio_button_new_with_label (NULL, _("Stock Item:"));
  table_attach (main_table, item_editor->stock_radio, 0, 0);

  alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
  gtk_grid_attach (GTK_GRID (main_table), alignment, 0, 1, 2, 1);
  gtk_widget_set_hexpand (alignment, TRUE);

  table = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (table),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), table);

  /* The stock item */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "stock", FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 0);
  table_attach (table, GTK_WIDGET (eprop), 1, 0);
  item_editor->properties = g_list_prepend (item_editor->properties, eprop);

  /* An accel group for the item's accelerator */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "accel-group", FALSE,
                                                 TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 1);
  table_attach (table, GTK_WIDGET (eprop), 1, 1);
  item_editor->properties = g_list_prepend (item_editor->properties, eprop);

  /* Now put a radio button in the same table for the custom image editing */
  item_editor->custom_radio = gtk_radio_button_new_with_label_from_widget
      (GTK_RADIO_BUTTON (item_editor->stock_radio),
       _("Custom label and image:"));
  table_attach (main_table, item_editor->custom_radio, 0, 2);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_grid_attach (GTK_GRID (main_table), vbox, 0, 3, 2, 1);
  gtk_widget_set_hexpand (vbox, TRUE);

  /* Label area frame... */
  str = g_strdup_printf ("<b>%s</b>", _("Edit Label"));
  label = gtk_label_new (str);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_free (str);
  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 12);
  item_editor->label_frame = frame;

  alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
  gtk_container_add (GTK_CONTAINER (frame), alignment);

  table = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (table),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), table);

  /* The menu label... */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "label", FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 0);
  table_attach (table, GTK_WIDGET (eprop), 1, 0);
  item_editor->properties = g_list_prepend (item_editor->properties, eprop);

  /* Whether to use-underline... */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "use-underline",
                                                 FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 1);
  table_attach (table, GTK_WIDGET (eprop), 1, 1);
  item_editor->properties = g_list_prepend (item_editor->properties, eprop);

  /* Internal Image area... */
  str = g_strdup_printf ("<b>%s</b>", _("Edit Image"));
  label = gtk_label_new (str);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_free (str);
  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 12);
  item_editor->embed_frame = frame;

  alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
  gtk_container_add (GTK_CONTAINER (frame), alignment);

  item_editor->embed_image = (GtkWidget *) glade_widget_adaptor_create_editable
      (glade_widget_adaptor_get_by_type (GTK_TYPE_IMAGE), GLADE_PAGE_GENERAL);
  glade_editable_set_show_name (GLADE_EDITABLE (item_editor->embed_image),
                                FALSE);
  gtk_container_add (GTK_CONTAINER (alignment), item_editor->embed_image);

  /* Now hook up to our signals... */
  g_signal_connect (G_OBJECT (item_editor->stock_radio), "toggled",
                    G_CALLBACK (stock_toggled), item_editor);
  g_signal_connect (G_OBJECT (item_editor->custom_radio), "toggled",
                    G_CALLBACK (custom_toggled), item_editor);


  gtk_widget_show_all (GTK_WIDGET (item_editor));

  return GTK_WIDGET (item_editor);
}
