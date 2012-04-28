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

#include "glade-button-editor.h"


static void glade_button_editor_finalize (GObject * object);

static void glade_button_editor_editable_init (GladeEditableIface * iface);

static void glade_button_editor_grab_focus (GtkWidget * widget);


static GladeEditableIface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeButtonEditor, glade_button_editor, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_button_editor_editable_init));


static void
glade_button_editor_class_init (GladeButtonEditorClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = glade_button_editor_finalize;
  widget_class->grab_focus = glade_button_editor_grab_focus;
}

static void
glade_button_editor_init (GladeButtonEditor * self)
{
}

static void
glade_button_editor_load (GladeEditable * editable, GladeWidget * widget)
{
  GladeButtonEditor *button_editor = GLADE_BUTTON_EDITOR (editable);
  GladeWidget *gchild = NULL;
  GtkWidget *child, *button;
  gboolean use_stock = FALSE, use_appearance = FALSE;
  GList *l;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  /* load the embedded editable... */
  if (button_editor->embed)
    glade_editable_load (GLADE_EDITABLE (button_editor->embed), widget);

  for (l = button_editor->properties; l; l = l->next)
    glade_editor_property_load_by_widget (GLADE_EDITOR_PROPERTY (l->data),
                                          widget);


  if (widget)
    {
      glade_widget_property_get (widget, "use-action-appearance",
                                 &use_appearance);

      button = GTK_WIDGET (glade_widget_get_object (widget));
      child = gtk_bin_get_child (GTK_BIN (button));
      if (child)
        gchild = glade_widget_get_from_gobject (child);

      /* Setup radio and sensitivity states */
      if ((gchild && glade_widget_get_parent (gchild)) || // a widget is manually inside
          GLADE_IS_PLACEHOLDER (child)) // placeholder there, custom mode
        {
          /* Custom */
          gtk_widget_set_sensitive (button_editor->standard_frame, FALSE);
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                        (button_editor->custom_radio), TRUE);
        }
      else
        {
          /* Standard */
          gtk_widget_set_sensitive (button_editor->standard_frame, TRUE);
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                        (button_editor->standard_radio), TRUE);

          glade_widget_property_get (widget, "use-stock", &use_stock);

          if (use_stock)
            {
              gtk_widget_set_sensitive (button_editor->stock_frame, TRUE);
              gtk_widget_set_sensitive (button_editor->label_frame, FALSE);
              gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                            (button_editor->stock_radio), TRUE);
            }
          else
            {
              gtk_widget_set_sensitive (button_editor->stock_frame, FALSE);
              gtk_widget_set_sensitive (button_editor->label_frame, TRUE);
              gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                            (button_editor->label_radio), TRUE);
            }
        }

      if (use_appearance)
        gtk_widget_set_sensitive (button_editor->custom_radio, FALSE);
      else
        gtk_widget_set_sensitive (button_editor->custom_radio, TRUE);

    }
}

static void
glade_button_editor_set_show_name (GladeEditable * editable, gboolean show_name)
{
  GladeButtonEditor *button_editor = GLADE_BUTTON_EDITOR (editable);

  glade_editable_set_show_name (GLADE_EDITABLE (button_editor->embed),
                                show_name);
}

static void
glade_button_editor_editable_init (GladeEditableIface * iface)
{
  parent_editable_iface = g_type_default_interface_peek (GLADE_TYPE_EDITABLE);

  iface->load = glade_button_editor_load;
  iface->set_show_name = glade_button_editor_set_show_name;
}

static void
glade_button_editor_finalize (GObject * object)
{
  GladeButtonEditor *button_editor = GLADE_BUTTON_EDITOR (object);

  if (button_editor->properties)
    g_list_free (button_editor->properties);
  button_editor->properties = NULL;
  button_editor->embed = NULL;

  glade_editable_load (GLADE_EDITABLE (object), NULL);

  G_OBJECT_CLASS (glade_button_editor_parent_class)->finalize (object);
}

static void
glade_button_editor_grab_focus (GtkWidget * widget)
{
  GladeButtonEditor *button_editor = GLADE_BUTTON_EDITOR (widget);

  gtk_widget_grab_focus (button_editor->embed);
}

/* Secion control radio button callbacks: */
static void
standard_toggled (GtkWidget * widget, GladeButtonEditor * button_editor)
{
  GladeProperty *property;
  GladeWidget *gchild = NULL, *gwidget;
  GtkWidget *child, *button;
  GValue value = { 0, };
  gboolean use_appearance = FALSE;

  gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (button_editor));

  if (glade_editable_loading (GLADE_EDITABLE (button_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (button_editor->standard_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (button_editor));

  glade_command_push_group (_("Setting %s to use standard configuration"),
                            glade_widget_get_name (gwidget));

  /* If theres a widget customly inside... command remove it first... */
  button = GTK_WIDGET (glade_widget_get_object (gwidget));
  child = gtk_bin_get_child (GTK_BIN (button));
  if (child)
    gchild = glade_widget_get_from_gobject (child);

  if (gchild && glade_widget_get_parent (gchild) == gwidget)
    {
      GList widgets = { 0, };
      widgets.data = gchild;
      glade_command_delete (&widgets);
    }

  property =
      glade_widget_get_property (gwidget, "custom-child");
  glade_command_set_property (property, FALSE);

  /* Setup reasonable defaults for button label. */
  property = glade_widget_get_property (gwidget, "stock");
  glade_command_set_property (property, NULL);

  property =
      glade_widget_get_property (gwidget, "use-stock");
  glade_command_set_property (property, FALSE);

  glade_widget_property_get (gwidget,
                             "use-action-appearance", &use_appearance);
  if (!use_appearance)
    {
      property =
          glade_widget_get_property (gwidget, "label");
      glade_property_get_default (property, &value);
      glade_command_set_property_value (property, &value);
      g_value_unset (&value);
    }

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (button_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (button_editor), gwidget);
}

static void
custom_toggled (GtkWidget * widget, GladeButtonEditor * button_editor)
{
  GladeProperty *property;
  GladeWidget *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (button_editor));

  if (glade_editable_loading (GLADE_EDITABLE (button_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (button_editor->custom_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (button_editor));

  glade_command_push_group (_("Setting %s to use a custom child"),
                            glade_widget_get_name (gwidget));

  /* clear out some things... */
  property = glade_widget_get_property (gwidget, "image");
  glade_command_set_property (property, NULL);

  property =
      glade_widget_get_property (gwidget, "use-stock");
  glade_command_set_property (property, FALSE);

  property = glade_widget_get_property (gwidget, "stock");
  glade_command_set_property (property, NULL);

  property = glade_widget_get_property (gwidget, "label");
  glade_command_set_property (property, NULL);

  /* Add a placeholder via the custom-child property... */
  property =
      glade_widget_get_property (gwidget, "custom-child");
  glade_command_set_property (property, TRUE);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (button_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (button_editor), gwidget);
}

static void
stock_toggled (GtkWidget * widget, GladeButtonEditor * button_editor)
{
  GladeProperty *property;
  gboolean use_appearance = FALSE;
  GladeWidget *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (button_editor));

  if (glade_editable_loading (GLADE_EDITABLE (button_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (button_editor->stock_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (button_editor));

  glade_command_push_group (_("Setting %s to use a stock button"),
                            glade_widget_get_name (gwidget));

  /* clear out stuff... */
  property = glade_widget_get_property (gwidget, "image");
  glade_command_set_property (property, NULL);

  glade_widget_property_get (gwidget, "use-action-appearance", &use_appearance);
  if (!use_appearance)
    {
      property =
          glade_widget_get_property (gwidget, "label");
      glade_command_set_property (property, "");
    }

  property =
      glade_widget_get_property (gwidget, "use-stock");
  glade_command_set_property (property, TRUE);

  property = glade_widget_get_property (gwidget, "stock");
  glade_command_set_property (property, NULL);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (button_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (button_editor), gwidget);
}

static void
label_toggled (GtkWidget * widget, GladeButtonEditor * button_editor)
{
  GladeProperty *property;
  GValue value = { 0, };
  gboolean use_appearance = FALSE;
  GladeWidget *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (button_editor));

  if (glade_editable_loading (GLADE_EDITABLE (button_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (button_editor->label_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (button_editor));

  glade_command_push_group (_("Setting %s to use a label and image"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "stock");
  glade_command_set_property (property, NULL);

  property = glade_widget_get_property (gwidget, "use-stock");
  glade_command_set_property (property, FALSE);

  glade_widget_property_get (gwidget, "use-action-appearance", &use_appearance);
  if (!use_appearance)
    {
      property =
          glade_widget_get_property (gwidget, "label");
      glade_property_get_default (property, &value);
      glade_command_set_property_value (property, &value);
      g_value_unset (&value);
    }

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (button_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (button_editor), gwidget);
}

static void
table_attach (GtkWidget * table, GtkWidget * child, gint pos, gint row)
{
  gtk_grid_attach (GTK_GRID (table), child, pos, row, 1, 1);

  if (pos)
    gtk_widget_set_hexpand (child, TRUE);
}

GtkWidget *
glade_button_editor_new (GladeWidgetAdaptor * adaptor, GladeEditable * embed)
{
  GladeButtonEditor *button_editor;
  GladeEditorProperty *eprop;
  GtkWidget *vbox, *table, *frame;

  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
  g_return_val_if_fail (GLADE_IS_EDITABLE (embed), NULL);

  button_editor = g_object_new (GLADE_TYPE_BUTTON_EDITOR, NULL);
  button_editor->embed = GTK_WIDGET (embed);

  button_editor->standard_radio =
      gtk_radio_button_new_with_label (NULL, _("Configure button content"));
  button_editor->custom_radio =
      gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON
                                                   (button_editor->
                                                    standard_radio),
                                                   _
                                                   ("Add custom button content"));

  button_editor->stock_radio =
      gtk_radio_button_new_with_label (NULL, _("Stock button"));
  button_editor->label_radio =
      gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON
                                                   (button_editor->stock_radio),
                                                   _
                                                   ("Label with optional image"));

  g_signal_connect (G_OBJECT (button_editor->standard_radio), "toggled",
                    G_CALLBACK (standard_toggled), button_editor);
  g_signal_connect (G_OBJECT (button_editor->custom_radio), "toggled",
                    G_CALLBACK (custom_toggled), button_editor);
  g_signal_connect (G_OBJECT (button_editor->stock_radio), "toggled",
                    G_CALLBACK (stock_toggled), button_editor);
  g_signal_connect (G_OBJECT (button_editor->label_radio), "toggled",
                    G_CALLBACK (label_toggled), button_editor);

  /* Pack the parent on top... */
  gtk_box_pack_start (GTK_BOX (button_editor), GTK_WIDGET (embed), FALSE, FALSE,
                      0);

  /* Standard frame... */
  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), button_editor->standard_radio);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (button_editor), frame, FALSE, FALSE, 8);

  button_editor->standard_frame = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
  gtk_alignment_set_padding (GTK_ALIGNMENT (button_editor->standard_frame), 6,
                             0, 12, 0);
  gtk_container_add (GTK_CONTAINER (frame), button_editor->standard_frame);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_add (GTK_CONTAINER (button_editor->standard_frame), vbox);

  /* Populate stock frame here... */
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_frame_set_label_widget (GTK_FRAME (frame), button_editor->stock_radio);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 4);

  button_editor->stock_frame = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
  gtk_alignment_set_padding (GTK_ALIGNMENT (button_editor->stock_frame), 6, 0,
                             12, 0);
  gtk_container_add (GTK_CONTAINER (frame), button_editor->stock_frame);

  table = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (table),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (table), 4);
  gtk_container_add (GTK_CONTAINER (button_editor->stock_frame), table);

  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "stock", FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 0);
  table_attach (table, GTK_WIDGET (eprop), 1, 0);
  button_editor->properties = g_list_prepend (button_editor->properties, eprop);

  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "image-position",
                                                 FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 1);
  table_attach (table, GTK_WIDGET (eprop), 1, 1);
  button_editor->properties = g_list_prepend (button_editor->properties, eprop);

  /* Populate label frame here... */
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_frame_set_label_widget (GTK_FRAME (frame), button_editor->label_radio);
  gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 4);

  button_editor->label_frame = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
  gtk_alignment_set_padding (GTK_ALIGNMENT (button_editor->label_frame), 6, 0,
                             12, 0);
  gtk_container_add (GTK_CONTAINER (frame), button_editor->label_frame);

  table = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (table),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (table), 4);
  gtk_container_add (GTK_CONTAINER (button_editor->label_frame), table);

  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "label", FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 0);
  table_attach (table, GTK_WIDGET (eprop), 1, 0);
  button_editor->properties = g_list_prepend (button_editor->properties, eprop);

  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "use-underline",
                                                 FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 1);
  table_attach (table, GTK_WIDGET (eprop), 1, 1);
  button_editor->properties = g_list_prepend (button_editor->properties, eprop);

  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "image", FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 2);
  table_attach (table, GTK_WIDGET (eprop), 1, 2);
  button_editor->properties = g_list_prepend (button_editor->properties, eprop);

  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "image-position",
                                                 FALSE, TRUE);
  table_attach (table, glade_editor_property_get_item_label (eprop), 0, 3);
  table_attach (table, GTK_WIDGET (eprop), 1, 3);
  button_editor->properties = g_list_prepend (button_editor->properties, eprop);

  /* Custom radio button on the bottom */
  gtk_box_pack_start (GTK_BOX (button_editor), button_editor->custom_radio,
                      FALSE, FALSE, 0);

  gtk_widget_show_all (GTK_WIDGET (button_editor));

  return GTK_WIDGET (button_editor);
}
