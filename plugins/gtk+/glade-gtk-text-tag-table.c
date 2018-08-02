/*
 * glade-gtk-text-tag-table.c - GladeWidgetAdaptor for GtkTextTagTable
 *
 * Copyright (C) 2013 Tristan Van Berkom
 *
 * Authors:
 *      Tristan Van Berkom <tristan.van.berkom@gmail.com>
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
 */

#include <config.h>
#include <glib/gi18n-lib.h>
#include <gladeui/glade.h>

#include "glade-gtk.h"

gboolean
glade_gtk_text_tag_table_add_verify (GladeWidgetAdaptor *adaptor,
                                     GtkWidget          *container,
                                     GtkWidget          *child,
                                     gboolean            user_feedback)
{
  if (!GTK_IS_TEXT_TAG (child))
    {
      if (user_feedback)
        {
          GladeWidgetAdaptor *tag_adaptor = 
            glade_widget_adaptor_get_by_type (GTK_TYPE_TEXT_TAG);

          glade_util_ui_message (glade_app_get_window (),
                                 GLADE_UI_INFO, NULL,
                                 ONLY_THIS_GOES_IN_THAT_MSG,
                                 glade_widget_adaptor_get_title (tag_adaptor),
                                 glade_widget_adaptor_get_title (adaptor));
        }

      return FALSE;
    }

  return TRUE;
}

void
glade_gtk_text_tag_table_add_child (GladeWidgetAdaptor *adaptor,
                                    GObject            *container, 
                                    GObject            *child)
{
  if (GTK_IS_TEXT_TAG (child))
    {
      /* Dont really add/remove tags (because name conflicts inside tables)
       */
      GladeWidget *gtable = glade_widget_get_from_gobject (container);
      GList *tags = g_object_get_data (G_OBJECT (gtable), "glade-tags");

      tags = g_list_copy (tags);
      tags = g_list_append (tags, child);

      g_object_set_data (child, "special-child-type", "tag");

      g_object_set_data_full (G_OBJECT (gtable), "glade-tags", tags,
                              (GDestroyNotify) g_list_free);
    }
}

void
glade_gtk_text_tag_table_remove_child (GladeWidgetAdaptor *adaptor,
                                       GObject            *container,
                                       GObject            *child)
{
  if (GTK_IS_TEXT_TAG (child))
    {
      /* Dont really add/remove actions (because name conflicts inside groups)
       */
      GladeWidget *gtable = glade_widget_get_from_gobject (container);
      GList *tags = g_object_get_data (G_OBJECT (gtable), "glade-tags");

      tags = g_list_copy (tags);
      tags = g_list_remove (tags, child);

      g_object_set_data (child, "special-child-type", NULL);

      g_object_set_data_full (G_OBJECT (gtable), "glade-tags", tags,
                              (GDestroyNotify) g_list_free);
    }
}

void
glade_gtk_text_tag_table_replace_child (GladeWidgetAdaptor *adaptor,
                                        GObject            *container,
                                        GObject            *current,
                                        GObject            *new_tag)
{
  glade_gtk_text_tag_table_remove_child (adaptor, container, current);
  glade_gtk_text_tag_table_add_child (adaptor, container, new_tag);
}

GList *
glade_gtk_text_tag_table_get_children (GladeWidgetAdaptor *adaptor,
                                       GObject            *container)
{
  GladeWidget *gtable = glade_widget_get_from_gobject (container);
  GList *tags = g_object_get_data (G_OBJECT (gtable), "glade-tags");

  return g_list_copy (tags);
}

static void
glade_gtk_text_tag_table_child_selected (GladeBaseEditor *editor,
                                         GladeWidget     *gchild,
                                         gpointer         data)
{
  glade_base_editor_add_label (editor, _("Tag"));

  glade_base_editor_add_default_properties (editor, gchild);

  glade_base_editor_add_label (editor, _("Properties"));
  glade_base_editor_add_editable (editor, gchild, GLADE_PAGE_GENERAL);
}

static gboolean
glade_gtk_text_tag_table_move_child (GladeBaseEditor *editor,
                                     GladeWidget     *gparent,
                                     GladeWidget     *gchild,
                                     gpointer         data)
{
  return FALSE;
}

static void
glade_gtk_text_tag_table_launch_editor (GObject  *table)
{
  GladeWidget        *widget  = glade_widget_get_from_gobject (table);
  GladeBaseEditor    *editor;
  GtkWidget          *window;

  /* Editor */
  editor = glade_base_editor_new (glade_widget_get_object (widget), NULL,
                                  _("Tag"), GTK_TYPE_TEXT_TAG,
                                  NULL);

  g_signal_connect (editor, "child-selected", G_CALLBACK (glade_gtk_text_tag_table_child_selected), NULL);
  g_signal_connect (editor, "move-child", G_CALLBACK (glade_gtk_text_tag_table_move_child), NULL);

  gtk_widget_show (GTK_WIDGET (editor));

  window = glade_base_editor_pack_new_window (editor, _("Text Tag Table Editor"), NULL);
  gtk_widget_show (window);
}

void
glade_gtk_text_tag_table_action_activate (GladeWidgetAdaptor *adaptor,
                                          GObject            *object,
                                          const gchar        *action_path)
{
  if (strcmp (action_path, "launch_editor") == 0)
    {
      glade_gtk_text_tag_table_launch_editor (object);
    }
}
