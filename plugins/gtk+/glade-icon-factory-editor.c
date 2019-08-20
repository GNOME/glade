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

#include "glade-icon-factory-editor.h"


static void glade_icon_factory_editor_finalize (GObject *object);

static void glade_icon_factory_editor_editable_init (GladeEditableInterface *
                                                     iface);

static void glade_icon_factory_editor_grab_focus (GtkWidget *widget);

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeIconFactoryEditor, glade_icon_factory_editor,
                         GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_icon_factory_editor_editable_init));


static void
glade_icon_factory_editor_class_init (GladeIconFactoryEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = glade_icon_factory_editor_finalize;
  widget_class->grab_focus = glade_icon_factory_editor_grab_focus;
}

static void
glade_icon_factory_editor_init (GladeIconFactoryEditor *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self),
                                  GTK_ORIENTATION_VERTICAL);
}

static void
glade_icon_factory_editor_load (GladeEditable *editable, GladeWidget *widget)
{
  GladeIconFactoryEditor *factory_editor = GLADE_ICON_FACTORY_EDITOR (editable);
  GList *l;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  /* load the embedded editable... */
  if (factory_editor->embed)
    glade_editable_load (GLADE_EDITABLE (factory_editor->embed), widget);

  for (l = factory_editor->properties; l; l = l->next)
    glade_editor_property_load_by_widget (GLADE_EDITOR_PROPERTY (l->data),
                                          widget);
}

static void
glade_icon_factory_editor_set_show_name (GladeEditable *editable,
                                         gboolean       show_name)
{
  GladeIconFactoryEditor *factory_editor = GLADE_ICON_FACTORY_EDITOR (editable);

  glade_editable_set_show_name (GLADE_EDITABLE (factory_editor->embed),
                                show_name);
}

static void
glade_icon_factory_editor_editable_init (GladeEditableInterface *iface)
{
  parent_editable_iface = g_type_default_interface_peek (GLADE_TYPE_EDITABLE);

  iface->load = glade_icon_factory_editor_load;
  iface->set_show_name = glade_icon_factory_editor_set_show_name;
}

static void
glade_icon_factory_editor_finalize (GObject *object)
{
  GladeIconFactoryEditor *factory_editor = GLADE_ICON_FACTORY_EDITOR (object);

  if (factory_editor->properties)
    g_list_free (factory_editor->properties);
  factory_editor->properties = NULL;
  factory_editor->embed = NULL;

  glade_editable_load (GLADE_EDITABLE (object), NULL);

  G_OBJECT_CLASS (glade_icon_factory_editor_parent_class)->finalize (object);
}

static void
glade_icon_factory_editor_grab_focus (GtkWidget *widget)
{
  GladeIconFactoryEditor *factory_editor = GLADE_ICON_FACTORY_EDITOR (widget);

  gtk_widget_grab_focus (factory_editor->embed);
}


GtkWidget *
glade_icon_factory_editor_new (GladeWidgetAdaptor *adaptor,
                               GladeEditable      *embed)
{
  GladeIconFactoryEditor *factory_editor;
  GladeEditorProperty *eprop;
  GtkWidget *label, *description_label, *vbox;

  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
  g_return_val_if_fail (GLADE_IS_EDITABLE (embed), NULL);

  factory_editor = g_object_new (GLADE_TYPE_ICON_FACTORY_EDITOR, NULL);
  factory_editor->embed = GTK_WIDGET (embed);

  /* Label item in frame label widget on top... */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "sources", FALSE,
                                                 TRUE);
  factory_editor->properties =
      g_list_prepend (factory_editor->properties, eprop);

  label = glade_editor_property_get_item_label (eprop);

  /* Add descriptive label */
  description_label = gtk_label_new (_("First add a stock name in the entry below, "
                                       "then add and define sources for that icon "
                                       "in the treeview."));
  gtk_label_set_line_wrap (GTK_LABEL (description_label), TRUE);
  gtk_label_set_line_wrap_mode (GTK_LABEL (description_label), PANGO_WRAP_WORD);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_set_spacing (GTK_BOX (vbox), 6);

  gtk_container_add (GTK_CONTAINER (vbox), label);
  gtk_container_add (GTK_CONTAINER (vbox), description_label);
  gtk_container_add (GTK_CONTAINER (vbox), GTK_WIDGET (eprop));
  gtk_widget_set_margin_top (vbox, 6);

  /* Pack the parent on top... */
  gtk_container_add (GTK_CONTAINER (factory_editor), GTK_WIDGET (embed));
  gtk_container_add (GTK_CONTAINER (factory_editor), vbox);

  gtk_widget_show_all (GTK_WIDGET (factory_editor));

  return GTK_WIDGET (factory_editor);
}
