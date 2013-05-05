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

#include "glade-recent-action-editor.h"

G_DEFINE_TYPE (GladeRecentActionEditor, glade_recent_action_editor, GLADE_TYPE_ACTION_EDITOR)

static void
glade_recent_action_editor_class_init (GladeRecentActionEditorClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-recent-action-editor.ui");
}

static void
glade_recent_action_editor_init (GladeRecentActionEditor * self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
glade_recent_action_editor_new (void)
{
  return g_object_new (GLADE_TYPE_RECENT_ACTION_EDITOR, NULL);
}
