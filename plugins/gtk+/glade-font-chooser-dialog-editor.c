/*
 * Copyright (C) 2013 Tristan Van Berkom.
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

#include "glade-font-chooser-dialog-editor.h"

G_DEFINE_TYPE (GladeFontChooserDialogEditor, glade_font_chooser_dialog_editor, GLADE_TYPE_WINDOW_EDITOR)

static void
glade_font_chooser_dialog_editor_class_init (GladeFontChooserDialogEditorClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-font-chooser-dialog-editor.ui");
}

static void
glade_font_chooser_dialog_editor_init (GladeFontChooserDialogEditor * self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

G_MODULE_EXPORT GtkWidget *
glade_font_chooser_dialog_editor_new (void)
{
  return g_object_new (GLADE_TYPE_FONT_CHOOSER_DIALOG_EDITOR, NULL);
}
