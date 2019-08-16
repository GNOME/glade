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

#include "glade-store-editor.h"


static void glade_store_editor_finalize (GObject *object);

static void glade_store_editor_editable_init (GladeEditableInterface *iface);

static void glade_store_editor_grab_focus (GtkWidget *widget);


static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeStoreEditor, glade_store_editor, GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_store_editor_editable_init));


static void
glade_store_editor_class_init (GladeStoreEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = glade_store_editor_finalize;
  widget_class->grab_focus = glade_store_editor_grab_focus;
}

static void
glade_store_editor_init (GladeStoreEditor *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self),
                                  GTK_ORIENTATION_VERTICAL);
}

static void
glade_store_editor_load (GladeEditable *editable, GladeWidget *widget)
{
  GladeStoreEditor *store_editor = GLADE_STORE_EDITOR (editable);
  GList *l;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  /* load the embedded editable... */
  if (store_editor->embed)
    glade_editable_load (GLADE_EDITABLE (store_editor->embed), widget);

  for (l = store_editor->properties; l; l = l->next)
    glade_editor_property_load_by_widget (GLADE_EDITOR_PROPERTY (l->data),
                                          widget);
}

static void
glade_store_editor_set_show_name (GladeEditable *editable, gboolean show_name)
{
  GladeStoreEditor *store_editor = GLADE_STORE_EDITOR (editable);

  glade_editable_set_show_name (GLADE_EDITABLE (store_editor->embed),
                                show_name);
}

static void
glade_store_editor_editable_init (GladeEditableInterface *iface)
{
  parent_editable_iface = g_type_default_interface_peek (GLADE_TYPE_EDITABLE);

  iface->load = glade_store_editor_load;
  iface->set_show_name = glade_store_editor_set_show_name;
}

static void
glade_store_editor_finalize (GObject *object)
{
  GladeStoreEditor *store_editor = GLADE_STORE_EDITOR (object);

  if (store_editor->properties)
    g_list_free (store_editor->properties);
  store_editor->properties = NULL;
  store_editor->embed = NULL;

  glade_editable_load (GLADE_EDITABLE (object), NULL);

  G_OBJECT_CLASS (glade_store_editor_parent_class)->finalize (object);
}

static void
glade_store_editor_grab_focus (GtkWidget *widget)
{
  GladeStoreEditor *store_editor = GLADE_STORE_EDITOR (widget);

  gtk_widget_grab_focus (store_editor->embed);
}

GtkWidget *
glade_store_editor_new (GladeWidgetAdaptor *adaptor, GladeEditable *embed)
{
  GladeStoreEditor *store_editor;
  GladeEditorProperty *eprop;
  GtkWidget *frame, *alignment, *label, *vbox;

  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
  g_return_val_if_fail (GLADE_IS_EDITABLE (embed), NULL);

  /* Pack the parent on top... */
  store_editor = g_object_new (GLADE_TYPE_STORE_EDITOR, NULL);
  store_editor->embed = GTK_WIDGET (embed);
  gtk_box_pack_start (GTK_BOX (store_editor), GTK_WIDGET (embed), FALSE, FALSE,
                      0);

  /* -------------- The columns area here -------------- */
  /* Label item in frame label widget on top.. */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "columns", FALSE,
                                                 TRUE);
  store_editor->properties = g_list_prepend (store_editor->properties, eprop);
  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), glade_editor_property_get_item_label (eprop));
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (store_editor), frame, FALSE, FALSE, 12);

  /* Alignment/Vbox in frame... */
  alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
  gtk_container_add (GTK_CONTAINER (frame), alignment);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (alignment), vbox);

  /* Add descriptive label */
  label = gtk_label_new (_("Define columns for your liststore; "
                           "giving them meaningful names will help you to retrieve "
                           "them when setting cell renderer attributes (press the "
                           "Delete key to remove the selected column)"));
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_line_wrap_mode (GTK_LABEL (label), PANGO_WRAP_WORD);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (eprop), FALSE, FALSE, 4);


  if (glade_widget_adaptor_get_object_type (adaptor) == GTK_TYPE_LIST_STORE ||
      g_type_is_a (glade_widget_adaptor_get_object_type (adaptor), GTK_TYPE_LIST_STORE))
    {
      /* -------------- The data area here -------------- */
      /* Label item in frame label widget on top.. */
      eprop =
          glade_widget_adaptor_create_eprop_by_name (adaptor, "data", FALSE,
                                                     TRUE);
      store_editor->properties =
          g_list_prepend (store_editor->properties, eprop);
      frame = gtk_frame_new (NULL);
      gtk_frame_set_label_widget (GTK_FRAME (frame), glade_editor_property_get_item_label (eprop));
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
      gtk_box_pack_start (GTK_BOX (store_editor), frame, FALSE, FALSE, 12);

      /* Alignment/Vbox in frame... */
      alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
      gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
      gtk_container_add (GTK_CONTAINER (frame), alignment);
      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (alignment), vbox);

      /* Add descriptive label */
      label =
          gtk_label_new (_
                         ("Add remove and edit rows of data (you can optionally use Ctrl+N to add "
                          "new rows and the Delete key to remove the selected row)"));
      gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
      gtk_label_set_line_wrap_mode (GTK_LABEL (label), PANGO_WRAP_WORD);
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 6);
      gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (eprop), FALSE, FALSE, 4);
    }

  gtk_widget_show_all (GTK_WIDGET (store_editor));

  return GTK_WIDGET (store_editor);
}
