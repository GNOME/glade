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

#include "glade-entry-editor.h"
#include "glade-image-editor.h" // For GladeImageEditMode


static void glade_entry_editor_finalize (GObject * object);

static void glade_entry_editor_editable_init (GladeEditableIface * iface);

static void glade_entry_editor_grab_focus (GtkWidget * widget);

static GladeEditableIface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeEntryEditor, glade_entry_editor, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_entry_editor_editable_init));


static void
glade_entry_editor_class_init (GladeEntryEditorClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = glade_entry_editor_finalize;
  widget_class->grab_focus = glade_entry_editor_grab_focus;
}

static void
glade_entry_editor_init (GladeEntryEditor * self)
{
}

static void
glade_entry_editor_load (GladeEditable * editable, GladeWidget * widget)
{
  GladeEntryEditor *entry_editor = GLADE_ENTRY_EDITOR (editable);
  GladeImageEditMode icon_mode;
  gboolean use_buffer = FALSE;
  GList *l;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  /* load the embedded editable... */
  if (entry_editor->embed)
    glade_editable_load (GLADE_EDITABLE (entry_editor->embed), widget);

  for (l = entry_editor->properties; l; l = l->next)
    glade_editor_property_load_by_widget (GLADE_EDITOR_PROPERTY (l->data),
                                          widget);


  if (widget)
    {
      glade_widget_property_get (widget, "use-entry-buffer", &use_buffer);
      if (use_buffer)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                      (entry_editor->buffer_radio), TRUE);
      else
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                      (entry_editor->text_radio), TRUE);


      glade_widget_property_get (widget, "primary-icon-mode", &icon_mode);
      switch (icon_mode)
        {
          case GLADE_IMAGE_MODE_STOCK:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (entry_editor->primary_stock_radio),
                                          TRUE);
            break;
          case GLADE_IMAGE_MODE_ICON:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (entry_editor->
                                           primary_icon_name_radio), TRUE);
            break;
          case GLADE_IMAGE_MODE_FILENAME:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (entry_editor->primary_pixbuf_radio),
                                          TRUE);
            break;
          default:
            break;
        }

      glade_widget_property_get (widget, "secondary-icon-mode", &icon_mode);
      switch (icon_mode)
        {
          case GLADE_IMAGE_MODE_STOCK:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (entry_editor->secondary_stock_radio),
                                          TRUE);
            break;
          case GLADE_IMAGE_MODE_ICON:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (entry_editor->
                                           secondary_icon_name_radio), TRUE);
            break;
          case GLADE_IMAGE_MODE_FILENAME:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (entry_editor->
                                           secondary_pixbuf_radio), TRUE);
            break;
          default:
            break;
        }
    }
}

static void
glade_entry_editor_set_show_name (GladeEditable * editable, gboolean show_name)
{
  GladeEntryEditor *entry_editor = GLADE_ENTRY_EDITOR (editable);

  glade_editable_set_show_name (GLADE_EDITABLE (entry_editor->embed),
                                show_name);
}

static void
glade_entry_editor_editable_init (GladeEditableIface * iface)
{
  parent_editable_iface = g_type_default_interface_peek (GLADE_TYPE_EDITABLE);

  iface->load = glade_entry_editor_load;
  iface->set_show_name = glade_entry_editor_set_show_name;
}

static void
glade_entry_editor_finalize (GObject * object)
{
  GladeEntryEditor *entry_editor = GLADE_ENTRY_EDITOR (object);

  if (entry_editor->properties)
    g_list_free (entry_editor->properties);
  entry_editor->properties = NULL;
  entry_editor->embed = NULL;

  glade_editable_load (GLADE_EDITABLE (object), NULL);

  G_OBJECT_CLASS (glade_entry_editor_parent_class)->finalize (object);
}

static void
glade_entry_editor_grab_focus (GtkWidget * widget)
{
  GladeEntryEditor *entry_editor = GLADE_ENTRY_EDITOR (widget);

  gtk_widget_grab_focus (entry_editor->embed);
}


static void
text_toggled (GtkWidget * widget, GladeEntryEditor * entry_editor)
{
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  if (glade_editable_loading (GLADE_EDITABLE (entry_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (entry_editor->text_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (entry_editor));

  glade_command_push_group (_("Setting %s to use static text"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "buffer");
  glade_command_set_property (property, NULL);

  property =
      glade_widget_get_property (gwidget,
                                 "use-entry-buffer");
  glade_command_set_property (property, FALSE);

  /* Text will only take effect after setting the property under the hood */
  property = glade_widget_get_property (gwidget, "text");
  glade_command_set_property (property, NULL);

  /* Incase the NULL text didnt change */
  glade_property_sync (property);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (entry_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (entry_editor), gwidget);
}

static void
buffer_toggled (GtkWidget * widget, GladeEntryEditor * entry_editor)
{
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  if (glade_editable_loading (GLADE_EDITABLE (entry_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (entry_editor->buffer_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (entry_editor));

  glade_command_push_group (_("Setting %s to use an external buffer"),
                            glade_widget_get_name (gwidget));

  /* Reset the text while still in static text mode */
  property = glade_widget_get_property (gwidget, "text");
  glade_command_set_property (property, NULL);

  property =
      glade_widget_get_property (gwidget, "use-entry-buffer");
  glade_command_set_property (property, TRUE);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (entry_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (entry_editor), gwidget);
}


#define ICON_MODE_NAME(primary)   ((primary) ? "primary-icon-mode"    : "secondary-icon-mode")
#define PIXBUF_NAME(primary)      ((primary) ? "primary-icon-pixbuf"  : "secondary-icon-pixbuf")
#define ICON_NAME_NAME(primary)   ((primary) ? "primary-icon-name"    : "secondary-icon-name")
#define STOCK_NAME(primary)       ((primary) ? "primary-icon-stock"   : "secondary-icon-stock")

static void
set_stock_mode (GladeEntryEditor * entry_editor, gboolean primary)
{
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));
  GValue value = { 0, };

  property = glade_widget_get_property (gwidget, ICON_NAME_NAME (primary));
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, PIXBUF_NAME (primary));
  glade_command_set_property (property, NULL);

  property = glade_widget_get_property (gwidget, STOCK_NAME (primary));
  glade_property_get_default (property, &value);
  glade_command_set_property_value (property, &value);
  g_value_unset (&value);

  property = glade_widget_get_property (gwidget, ICON_MODE_NAME (primary));
  glade_command_set_property (property, GLADE_IMAGE_MODE_STOCK);
}

static void
set_icon_name_mode (GladeEntryEditor * entry_editor, gboolean primary)
{
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  property = glade_widget_get_property (gwidget, STOCK_NAME (primary));
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, PIXBUF_NAME (primary));
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, ICON_MODE_NAME (primary));
  glade_command_set_property (property, GLADE_IMAGE_MODE_ICON);
}

static void
set_pixbuf_mode (GladeEntryEditor * entry_editor, gboolean primary)
{
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  property = glade_widget_get_property (gwidget, STOCK_NAME (primary));
  glade_command_set_property (property, NULL);

  property = glade_widget_get_property (gwidget, ICON_NAME_NAME (primary));
  glade_command_set_property (property, NULL);

  property = glade_widget_get_property (gwidget, ICON_MODE_NAME (primary));
  glade_command_set_property (property, GLADE_IMAGE_MODE_FILENAME);
}

static void
primary_stock_toggled (GtkWidget * widget, GladeEntryEditor * entry_editor)
{
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  if (glade_editable_loading (GLADE_EDITABLE (entry_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (entry_editor->primary_stock_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (entry_editor));

  glade_command_push_group (_("Setting %s to use a primary icon from stock"),
                            glade_widget_get_name (gwidget));
  set_stock_mode (entry_editor, TRUE);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (entry_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (entry_editor), gwidget);
}


static void
primary_icon_name_toggled (GtkWidget * widget, GladeEntryEditor * entry_editor)
{
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  if (glade_editable_loading (GLADE_EDITABLE (entry_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (entry_editor->primary_icon_name_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (entry_editor));

  glade_command_push_group (_("Setting %s to use a primary icon from the icon theme"),
                            glade_widget_get_name (gwidget));
  set_icon_name_mode (entry_editor, TRUE);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (entry_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (entry_editor), gwidget);
}

static void
primary_pixbuf_toggled (GtkWidget * widget, GladeEntryEditor * entry_editor)
{
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  if (glade_editable_loading (GLADE_EDITABLE (entry_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (entry_editor->primary_pixbuf_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (entry_editor));

  glade_command_push_group (_("Setting %s to use a primary icon from filename"),
                            glade_widget_get_name (gwidget));
  set_pixbuf_mode (entry_editor, TRUE);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (entry_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (entry_editor), gwidget);
}


static void
secondary_stock_toggled (GtkWidget * widget, GladeEntryEditor * entry_editor)
{
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  if (glade_editable_loading (GLADE_EDITABLE (entry_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (entry_editor->secondary_stock_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (entry_editor));

  glade_command_push_group (_("Setting %s to use a secondary icon from stock"),
                            glade_widget_get_name (gwidget));
  set_stock_mode (entry_editor, FALSE);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (entry_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (entry_editor), gwidget);
}


static void
secondary_icon_name_toggled (GtkWidget * widget,
                             GladeEntryEditor * entry_editor)
{
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  if (glade_editable_loading (GLADE_EDITABLE (entry_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (entry_editor->secondary_icon_name_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (entry_editor));

  glade_command_push_group (_("Setting %s to use a secondary icon from the icon theme"),
                            glade_widget_get_name (gwidget));
  set_icon_name_mode (entry_editor, FALSE);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (entry_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (entry_editor), gwidget);
}

static void
secondary_pixbuf_toggled (GtkWidget * widget, GladeEntryEditor * entry_editor)
{
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  if (glade_editable_loading (GLADE_EDITABLE (entry_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (entry_editor->secondary_pixbuf_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (entry_editor));

  glade_command_push_group (_("Setting %s to use a secondary icon from filename"),
                            glade_widget_get_name (gwidget));
  set_pixbuf_mode (entry_editor, FALSE);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (entry_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (entry_editor), gwidget);
}


static void
table_attach (GtkWidget * table, GtkWidget * child, gint pos, gint row)
{
  gtk_grid_attach (GTK_GRID (table), child, pos, row, 1, 1);

  if (pos)
    gtk_widget_set_hexpand (child, TRUE);
}

GtkWidget *
glade_entry_editor_new (GladeWidgetAdaptor * adaptor, GladeEditable * embed)
{
  GladeEntryEditor *entry_editor;
  GladeEditorProperty *eprop;
  GtkWidget *table, *frame, *alignment, *label, *hbox;
  gchar *str;

  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
  g_return_val_if_fail (GLADE_IS_EDITABLE (embed), NULL);

  entry_editor = g_object_new (GLADE_TYPE_ENTRY_EDITOR, NULL);
  entry_editor->embed = GTK_WIDGET (embed);

  /* Pack the parent on top... */
  gtk_box_pack_start (GTK_BOX (entry_editor), GTK_WIDGET (embed), FALSE, FALSE,
                      0);


  /* Text... */
  str = g_strdup_printf ("<b>%s</b>", _("Text"));
  label = gtk_label_new (str);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_free (str);
  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (entry_editor), frame, FALSE, FALSE, 8);

  alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
  gtk_container_add (GTK_CONTAINER (frame), alignment);

  table = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (table),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), table);

  /* Text */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "text", FALSE, TRUE);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  entry_editor->text_radio = gtk_radio_button_new (NULL);
  gtk_box_pack_start (GTK_BOX (hbox), entry_editor->text_radio, FALSE, FALSE,
                      2);
  gtk_box_pack_start (GTK_BOX (hbox), glade_editor_property_get_item_label (eprop), TRUE, TRUE, 2);
  table_attach (table, hbox, 0, 0);
  table_attach (table, GTK_WIDGET (eprop), 1, 0);
  entry_editor->properties = g_list_prepend (entry_editor->properties, eprop);

  /* Buffer */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "buffer", FALSE,
                                                 TRUE);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  entry_editor->buffer_radio = gtk_radio_button_new_from_widget
      (GTK_RADIO_BUTTON (entry_editor->text_radio));
  gtk_box_pack_start (GTK_BOX (hbox), entry_editor->buffer_radio, FALSE, FALSE,
                      2);
  gtk_box_pack_start (GTK_BOX (hbox), glade_editor_property_get_item_label (eprop), TRUE, TRUE, 2);
  table_attach (table, hbox, 0, 1);
  table_attach (table, GTK_WIDGET (eprop), 1, 1);
  entry_editor->properties = g_list_prepend (entry_editor->properties, eprop);

  /* Progress... */
  str = g_strdup_printf ("<b>%s</b>", _("Progress"));
  label = gtk_label_new (str);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_free (str);
  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (entry_editor), frame, FALSE, FALSE, 8);

  alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
  gtk_container_add (GTK_CONTAINER (frame), alignment);

  table = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (table),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), table);

  /* Fraction */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "progress-fraction",
                                                 FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 0);
  table_attach (table, GTK_WIDGET (eprop), 1, 0);
  entry_editor->properties = g_list_prepend (entry_editor->properties, eprop);

  /* Pulse */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "progress-pulse-step",
                                                 FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 1);
  table_attach (table, GTK_WIDGET (eprop), 1, 1);
  entry_editor->properties = g_list_prepend (entry_editor->properties, eprop);

  /* Primary icon... */
  str = g_strdup_printf ("<b>%s</b>", _("Primary icon"));
  label = gtk_label_new (str);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_free (str);
  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (entry_editor), frame, FALSE, FALSE, 8);

  alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
  gtk_container_add (GTK_CONTAINER (frame), alignment);

  table = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (table),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), table);

  /* Pixbuf */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, PIXBUF_NAME (TRUE),
                                                 FALSE, TRUE);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  entry_editor->primary_pixbuf_radio = gtk_radio_button_new (NULL);
  gtk_box_pack_start (GTK_BOX (hbox), entry_editor->primary_pixbuf_radio, FALSE,
                      FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), glade_editor_property_get_item_label (eprop), TRUE, TRUE, 2);
  table_attach (table, hbox, 0, 0);
  table_attach (table, GTK_WIDGET (eprop), 1, 0);
  entry_editor->properties = g_list_prepend (entry_editor->properties, eprop);

  /* Stock */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, STOCK_NAME (TRUE),
                                                 FALSE, TRUE);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  entry_editor->primary_stock_radio = gtk_radio_button_new_from_widget
      (GTK_RADIO_BUTTON (entry_editor->primary_pixbuf_radio));
  gtk_box_pack_start (GTK_BOX (hbox), entry_editor->primary_stock_radio, FALSE,
                      FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), glade_editor_property_get_item_label (eprop), TRUE, TRUE, 2);
  table_attach (table, hbox, 0, 1);
  table_attach (table, GTK_WIDGET (eprop), 1, 1);
  entry_editor->properties = g_list_prepend (entry_editor->properties, eprop);

  /* Icon name */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, ICON_NAME_NAME (TRUE),
                                                 FALSE, TRUE);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  entry_editor->primary_icon_name_radio = gtk_radio_button_new_from_widget
      (GTK_RADIO_BUTTON (entry_editor->primary_pixbuf_radio));
  gtk_box_pack_start (GTK_BOX (hbox), entry_editor->primary_icon_name_radio,
                      FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), glade_editor_property_get_item_label (eprop), TRUE, TRUE, 2);
  table_attach (table, hbox, 0, 2);
  table_attach (table, GTK_WIDGET (eprop), 1, 2);
  entry_editor->properties = g_list_prepend (entry_editor->properties, eprop);

  /* Other primary icon related properties */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor,
                                                 "primary-icon-activatable",
                                                 FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 3);
  table_attach (table, GTK_WIDGET (eprop), 1, 3);
  entry_editor->properties = g_list_prepend (entry_editor->properties, eprop);

  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor,
                                                 "primary-icon-sensitive",
                                                 FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 4);
  table_attach (table, GTK_WIDGET (eprop), 1, 4);
  entry_editor->properties = g_list_prepend (entry_editor->properties, eprop);

  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor,
                                                 "primary-icon-tooltip-text",
                                                 FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 5);
  table_attach (table, GTK_WIDGET (eprop), 1, 5);
  entry_editor->properties = g_list_prepend (entry_editor->properties, eprop);

  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor,
                                                 "primary-icon-tooltip-markup",
                                                 FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 6);
  table_attach (table, GTK_WIDGET (eprop), 1, 6);
  entry_editor->properties = g_list_prepend (entry_editor->properties, eprop);

  /* Secondary icon... */
  str = g_strdup_printf ("<b>%s</b>", _("Secondary icon"));
  label = gtk_label_new (str);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_free (str);
  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (entry_editor), frame, FALSE, FALSE, 8);

  alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
  gtk_container_add (GTK_CONTAINER (frame), alignment);

  table = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (table),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (table), 4);
  gtk_container_add (GTK_CONTAINER (alignment), table);

  /* Pixbuf */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, PIXBUF_NAME (FALSE),
                                                 FALSE, TRUE);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  entry_editor->secondary_pixbuf_radio = gtk_radio_button_new (NULL);
  gtk_box_pack_start (GTK_BOX (hbox), entry_editor->secondary_pixbuf_radio,
                      FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), glade_editor_property_get_item_label (eprop), TRUE, TRUE, 2);
  table_attach (table, hbox, 0, 0);
  table_attach (table, GTK_WIDGET (eprop), 1, 0);
  entry_editor->properties = g_list_prepend (entry_editor->properties, eprop);

  /* Stock */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, STOCK_NAME (FALSE),
                                                 FALSE, TRUE);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  entry_editor->secondary_stock_radio = gtk_radio_button_new_from_widget
      (GTK_RADIO_BUTTON (entry_editor->secondary_pixbuf_radio));
  gtk_box_pack_start (GTK_BOX (hbox), entry_editor->secondary_stock_radio,
                      FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), glade_editor_property_get_item_label (eprop), TRUE, TRUE, 2);
  table_attach (table, hbox, 0, 1);
  table_attach (table, GTK_WIDGET (eprop), 1, 1);
  entry_editor->properties = g_list_prepend (entry_editor->properties, eprop);

  /* Icon name */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor,
                                                 ICON_NAME_NAME (FALSE), FALSE,
                                                 TRUE);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  entry_editor->secondary_icon_name_radio = gtk_radio_button_new_from_widget
      (GTK_RADIO_BUTTON (entry_editor->secondary_pixbuf_radio));
  gtk_box_pack_start (GTK_BOX (hbox), entry_editor->secondary_icon_name_radio,
                      FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), glade_editor_property_get_item_label (eprop), TRUE, TRUE, 2);
  table_attach (table, hbox, 0, 2);
  table_attach (table, GTK_WIDGET (eprop), 1, 2);
  entry_editor->properties = g_list_prepend (entry_editor->properties, eprop);

  /* Other secondary icon related properties */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor,
                                                 "secondary-icon-activatable",
                                                 FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 3);
  table_attach (table, GTK_WIDGET (eprop), 1, 3);
  entry_editor->properties = g_list_prepend (entry_editor->properties, eprop);

  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor,
                                                 "secondary-icon-sensitive",
                                                 FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 4);
  table_attach (table, GTK_WIDGET (eprop), 1, 4);
  entry_editor->properties = g_list_prepend (entry_editor->properties, eprop);

  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor,
                                                 "secondary-icon-tooltip-text",
                                                 FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 5);
  table_attach (table, GTK_WIDGET (eprop), 1, 5);
  entry_editor->properties = g_list_prepend (entry_editor->properties, eprop);

  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor,
                                                 "secondary-icon-tooltip-markup",
                                                 FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 6);
  table_attach (table, GTK_WIDGET (eprop), 1, 6);
  entry_editor->properties = g_list_prepend (entry_editor->properties, eprop);

  gtk_widget_show_all (GTK_WIDGET (entry_editor));


  /* Connect radio button signals... */
  g_signal_connect (G_OBJECT (entry_editor->text_radio), "toggled",
                    G_CALLBACK (text_toggled), entry_editor);
  g_signal_connect (G_OBJECT (entry_editor->buffer_radio), "toggled",
                    G_CALLBACK (buffer_toggled), entry_editor);

  g_signal_connect (G_OBJECT (entry_editor->primary_stock_radio), "toggled",
                    G_CALLBACK (primary_stock_toggled), entry_editor);
  g_signal_connect (G_OBJECT (entry_editor->primary_icon_name_radio), "toggled",
                    G_CALLBACK (primary_icon_name_toggled), entry_editor);
  g_signal_connect (G_OBJECT (entry_editor->primary_pixbuf_radio), "toggled",
                    G_CALLBACK (primary_pixbuf_toggled), entry_editor);

  g_signal_connect (G_OBJECT (entry_editor->secondary_stock_radio), "toggled",
                    G_CALLBACK (secondary_stock_toggled), entry_editor);
  g_signal_connect (G_OBJECT (entry_editor->secondary_icon_name_radio),
                    "toggled", G_CALLBACK (secondary_icon_name_toggled),
                    entry_editor);
  g_signal_connect (G_OBJECT (entry_editor->secondary_pixbuf_radio), "toggled",
                    G_CALLBACK (secondary_pixbuf_toggled), entry_editor);

  return GTK_WIDGET (entry_editor);
}
