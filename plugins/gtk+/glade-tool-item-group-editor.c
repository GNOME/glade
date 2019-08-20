/*
 * Copyright (C) 2010 Openismus GmbH
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
 *      Tristan Van Berkom <tristanvb@openismus.com>
 */

#include <config.h>
#include <gladeui/glade.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#include "glade-tool-item-group-editor.h"


static void glade_tool_item_group_editor_finalize (GObject *object);

static void glade_tool_item_group_editor_editable_init (GladeEditableInterface *iface);

static void glade_tool_item_group_editor_grab_focus (GtkWidget *widget);


static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeToolItemGroupEditor, glade_tool_item_group_editor, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_tool_item_group_editor_editable_init));


static void
glade_tool_item_group_editor_class_init (GladeToolItemGroupEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = glade_tool_item_group_editor_finalize;
  widget_class->grab_focus = glade_tool_item_group_editor_grab_focus;
}

static void
glade_tool_item_group_editor_init (GladeToolItemGroupEditor *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self),
                                  GTK_ORIENTATION_VERTICAL);
}

static void
glade_tool_item_group_editor_load (GladeEditable *editable, GladeWidget *widget)
{
  GladeToolItemGroupEditor *group_editor = GLADE_TOOL_ITEM_GROUP_EDITOR (editable);
  gboolean custom_label = FALSE;
  GList *l;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  /* load the embedded editable... */
  if (group_editor->embed)
    glade_editable_load (GLADE_EDITABLE (group_editor->embed), widget);

  for (l = group_editor->properties; l; l = l->next)
    glade_editor_property_load_by_widget (GLADE_EDITOR_PROPERTY (l->data),
                                          widget);


  if (widget)
    {
      glade_widget_property_get (widget, "custom-label", &custom_label);

      if (custom_label)
        gtk_toggle_button_set_active
            (GTK_TOGGLE_BUTTON (group_editor->label_widget_radio), TRUE);
      else
        gtk_toggle_button_set_active
            (GTK_TOGGLE_BUTTON (group_editor->label_radio), TRUE);
    }
}

static void
glade_tool_item_group_editor_set_show_name (GladeEditable *editable, gboolean show_name)
{
  GladeToolItemGroupEditor *group_editor = GLADE_TOOL_ITEM_GROUP_EDITOR (editable);

  glade_editable_set_show_name (GLADE_EDITABLE (group_editor->embed),
                                show_name);
}

static void
glade_tool_item_group_editor_editable_init (GladeEditableInterface *iface)
{
  parent_editable_iface = g_type_default_interface_peek (GLADE_TYPE_EDITABLE);

  iface->load = glade_tool_item_group_editor_load;
  iface->set_show_name = glade_tool_item_group_editor_set_show_name;
}

static void
glade_tool_item_group_editor_finalize (GObject *object)
{
  GladeToolItemGroupEditor *group_editor = GLADE_TOOL_ITEM_GROUP_EDITOR (object);

  if (group_editor->properties)
    g_list_free (group_editor->properties);
  group_editor->properties = NULL;
  group_editor->embed = NULL;

  glade_editable_load (GLADE_EDITABLE (object), NULL);

  G_OBJECT_CLASS (glade_tool_item_group_editor_parent_class)->finalize (object);
}

static void
glade_tool_item_group_editor_grab_focus (GtkWidget *widget)
{
  GladeToolItemGroupEditor *group_editor = GLADE_TOOL_ITEM_GROUP_EDITOR (widget);

  gtk_widget_grab_focus (group_editor->embed);
}


static void
label_toggled (GtkWidget *widget, GladeToolItemGroupEditor *group_editor)
{
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (group_editor));
  GValue value = { 0, };

  if (glade_editable_loading (GLADE_EDITABLE (group_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (group_editor->label_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (group_editor));

  glade_command_push_group (_("Setting %s to use standard label text"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "label-widget");
  glade_command_set_property (property, NULL);

  property = glade_widget_get_property (gwidget, "label");
  glade_property_get_default (property, &value);
  glade_command_set_property_value (property, &value);
  g_value_unset (&value);
  property = glade_widget_get_property (gwidget, "custom-label");
  glade_command_set_property (property, FALSE);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (group_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (group_editor), gwidget);
}

static void
label_widget_toggled (GtkWidget *widget, GladeToolItemGroupEditor *group_editor)
{
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (group_editor));

  if (glade_editable_loading (GLADE_EDITABLE (group_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (group_editor->label_widget_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (group_editor));

  glade_command_push_group (_("Setting %s to use a custom label widget"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "label");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "custom-label");
  glade_command_set_property (property, TRUE);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (group_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (group_editor), gwidget);
}

static void
table_attach (GtkWidget *table, GtkWidget *child, gint pos, gint row)
{
  gtk_grid_attach (GTK_GRID (table), child, pos, row, 1, 1);

  if (pos)
    gtk_widget_set_hexpand (child, TRUE);
}

GtkWidget *
glade_tool_item_group_editor_new (GladeWidgetAdaptor *adaptor, 
                                  GladeEditable      *embed)
{
  GladeToolItemGroupEditor *group_editor;
  GladeEditorProperty *eprop;
  GtkWidget *table, *frame, *label, *hbox;
  gchar *str;
  gint row = 0;

  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
  g_return_val_if_fail (GLADE_IS_EDITABLE (embed), NULL);

  group_editor = g_object_new (GLADE_TYPE_TOOL_ITEM_GROUP_EDITOR, NULL);
  group_editor->embed = GTK_WIDGET (embed);

  /* Pack the parent on top... */
  gtk_box_pack_start (GTK_BOX (group_editor), GTK_WIDGET (embed), FALSE, FALSE, 0);

  str = g_strdup_printf ("<b>%s</b>", _("Group Header"));
  label = gtk_label_new (str);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_free (str);
  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (group_editor), frame, FALSE, FALSE, 0);

  table = gtk_grid_new ();
  gtk_widget_set_margin_top (table, 6);
  gtk_widget_set_margin_start (table, 12);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (table),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (table), 4);
  gtk_container_add (GTK_CONTAINER (frame), table);

  /* label */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "label", FALSE,
                                                 TRUE);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  group_editor->label_radio = gtk_radio_button_new (NULL);
  gtk_box_pack_start (GTK_BOX (hbox), group_editor->label_radio, FALSE, FALSE,
                      2);
  gtk_box_pack_start (GTK_BOX (hbox), glade_editor_property_get_item_label (eprop), TRUE, TRUE, 2);
  table_attach (table, hbox, 0, row);
  table_attach (table, GTK_WIDGET (eprop), 1, row++);
  group_editor->properties = g_list_prepend (group_editor->properties, eprop);

  /* label-widget ... */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "label-widget",
                                                 FALSE, TRUE);
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  group_editor->label_widget_radio = gtk_radio_button_new_from_widget
      (GTK_RADIO_BUTTON (group_editor->label_radio));
  gtk_box_pack_start (GTK_BOX (hbox), group_editor->label_widget_radio, FALSE,
                      FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), glade_editor_property_get_item_label (eprop), TRUE, TRUE, 2);
  table_attach (table, hbox, 0, row);
  table_attach (table, GTK_WIDGET (eprop), 1, row++);
  group_editor->properties = g_list_prepend (group_editor->properties, eprop);

  g_signal_connect (G_OBJECT (group_editor->label_widget_radio), "toggled",
                    G_CALLBACK (label_widget_toggled), group_editor);
  g_signal_connect (G_OBJECT (group_editor->label_radio), "toggled",
                    G_CALLBACK (label_toggled), group_editor);

  gtk_widget_show_all (GTK_WIDGET (group_editor));

  return GTK_WIDGET (group_editor);
}
