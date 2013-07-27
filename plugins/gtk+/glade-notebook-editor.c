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
#include <gdk/gdkkeysyms.h>

#include "glade-notebook-editor.h"

static void glade_notebook_editor_editable_init (GladeEditableIface * iface);
static void glade_notebook_editor_grab_focus (GtkWidget * widget);

struct _GladeNotebookEditorPrivate
{
  GtkWidget *embed;
  GtkWidget *tabs_grid;
};

static GladeEditableIface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeNotebookEditor, glade_notebook_editor, GLADE_TYPE_EDITOR_SKELETON,
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_notebook_editor_editable_init));

static void
glade_notebook_editor_class_init (GladeNotebookEditorClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = glade_notebook_editor_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-notebook-editor.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GladeNotebookEditor, embed);
  gtk_widget_class_bind_template_child_private (widget_class, GladeNotebookEditor, tabs_grid);

  g_type_class_add_private (object_class, sizeof (GladeNotebookEditorPrivate));  
}

static void
glade_notebook_editor_init (GladeNotebookEditor * self)
{
  self->priv = 
    G_TYPE_INSTANCE_GET_PRIVATE (self,
				 GLADE_TYPE_NOTEBOOK_EDITOR,
				 GladeNotebookEditorPrivate);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_notebook_editor_load (GladeEditable * editable, GladeWidget * widget)
{
  GladeNotebookEditor *notebook_editor = GLADE_NOTEBOOK_EDITOR (editable);
  GladeNotebookEditorPrivate *priv = notebook_editor->priv;
  gboolean show_tabs = FALSE;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  if (widget)
    {
      glade_widget_property_get (widget, "show-tabs", &show_tabs);

      gtk_widget_set_visible (priv->tabs_grid, show_tabs);
    }
}

static void
glade_notebook_editor_editable_init (GladeEditableIface * iface)
{
  parent_editable_iface = g_type_interface_peek_parent (iface);

  iface->load = glade_notebook_editor_load;
}

static void
glade_notebook_editor_grab_focus (GtkWidget * widget)
{
  GladeNotebookEditor *notebook_editor = GLADE_NOTEBOOK_EDITOR (widget);

  gtk_widget_grab_focus (notebook_editor->priv->embed);
}

GtkWidget *
glade_notebook_editor_new (void)
{
  return g_object_new (GLADE_TYPE_NOTEBOOK_EDITOR, NULL);
}
