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

static void glade_notebook_editor_editable_init (GladeEditableInterface * iface);
static void glade_notebook_editor_grab_focus (GtkWidget * widget);
static void has_start_action_changed (GObject *object, GParamSpec *pspec, GladeNotebookEditor *editor);
static void has_end_action_changed (GObject *object, GParamSpec *pspec, GladeNotebookEditor *editor);

struct _GladeNotebookEditorPrivate
{
  GtkWidget *embed;
  GtkWidget *tabs_grid;
  GtkWidget *action_start_editor;
  GtkWidget *action_end_editor;
};

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeNotebookEditor, glade_notebook_editor, GLADE_TYPE_EDITOR_SKELETON,
                         G_ADD_PRIVATE (GladeNotebookEditor)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_notebook_editor_editable_init));

static void
glade_notebook_editor_class_init (GladeNotebookEditorClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = glade_notebook_editor_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-notebook-editor.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GladeNotebookEditor, embed);
  gtk_widget_class_bind_template_child_private (widget_class, GladeNotebookEditor, tabs_grid);
  gtk_widget_class_bind_template_child_private (widget_class, GladeNotebookEditor, action_start_editor);
  gtk_widget_class_bind_template_child_private (widget_class, GladeNotebookEditor, action_end_editor);

  gtk_widget_class_bind_template_callback (widget_class, has_start_action_changed);
  gtk_widget_class_bind_template_callback (widget_class, has_end_action_changed);
}

static void
glade_notebook_editor_init (GladeNotebookEditor * self)
{
  self->priv = glade_notebook_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_notebook_editor_load (GladeEditable * editable, GladeWidget * widget)
{
  GladeNotebookEditor *notebook_editor = GLADE_NOTEBOOK_EDITOR (editable);
  GladeNotebookEditorPrivate *priv = notebook_editor->priv;
  gboolean show_tabs = FALSE;
  gboolean has_start_action = FALSE;
  gboolean has_end_action = FALSE;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  if (widget)
    {
      glade_widget_property_get (widget, "show-tabs", &show_tabs);
      gtk_widget_set_visible (priv->tabs_grid, show_tabs);

      glade_widget_property_get (widget, "has-action-start", &has_start_action);
      gtk_switch_set_active (GTK_SWITCH (priv->action_start_editor), has_start_action);

      glade_widget_property_get (widget, "has-action-end", &has_end_action);
      gtk_switch_set_active (GTK_SWITCH (priv->action_end_editor), has_end_action);
    }
}

static void
glade_notebook_editor_editable_init (GladeEditableInterface * iface)
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

static void
has_action_changed (GladeNotebookEditor *editor, GtkPackType pack_type)
{
  GladeNotebookEditorPrivate *priv = editor->priv;
  GladeWidget *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (editor));
  GtkWidget *notebook;
  GtkWidget *action;
  GladeWidget *gaction = NULL;
  GladeProperty *property;
  gboolean has_action;
  GtkWidget *action_editor;

  if (glade_editable_loading (GLADE_EDITABLE (editor)) || !gwidget)
    return;

  if (pack_type == GTK_PACK_START)
    {
      action_editor = priv->action_start_editor;
      property = glade_widget_get_property (gwidget, "has-action-start");
    }
  else
    {
      action_editor = priv->action_end_editor;
      property = glade_widget_get_property (gwidget, "has-action-end");
    }

  has_action = gtk_switch_get_active (GTK_SWITCH (action_editor));

  notebook = (GtkWidget *)glade_widget_get_object (gwidget);
  action = gtk_notebook_get_action_widget (GTK_NOTEBOOK (notebook), pack_type);

  if (action && !GLADE_IS_PLACEHOLDER (action))
    gaction = glade_widget_get_from_gobject (action);

  glade_editable_block (GLADE_EDITABLE (editor));

  if (has_action && pack_type == GTK_PACK_START)
    glade_command_push_group (_("Setting %s to have a start action"),
                              glade_widget_get_name (gwidget));
  else if (has_action && pack_type == GTK_PACK_END)
    glade_command_push_group (_("Setting %s to have an end action"),
                              glade_widget_get_name (gwidget));
  else if (!has_action && pack_type == GTK_PACK_START)
    glade_command_push_group (_("Setting %s to not have a start action"),
                              glade_widget_get_name (gwidget));
  else
    glade_command_push_group (_("Setting %s to not have an end action"),
                              glade_widget_get_name (gwidget));

  /* If a project widget exists when were disabling center child, it needs
   * to be removed first as a part of the issuing GladeCommand group
   */
  if (gaction)
    {
      GList list;
      list.prev = list.next = NULL;
      list.data = gaction;
      glade_command_delete (&list);
    }

  glade_command_set_property (property, has_action);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (editor));

  glade_editable_load (GLADE_EDITABLE (editor), gwidget);
}

static void
has_start_action_changed (GObject *object, GParamSpec *pspec, GladeNotebookEditor *editor)
{
  has_action_changed (editor, GTK_PACK_START);
}

static void
has_end_action_changed (GObject *object, GParamSpec *pspec, GladeNotebookEditor *editor)
{
  has_action_changed (editor, GTK_PACK_END);
}
