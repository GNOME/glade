/*
 * glade-editable.c
 *
 * Copyright (C) 2008 Tristan Van Berkom.
 *
 * Authors:
 *   Tristan Van Berkom <tvb@gnome.org>
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
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include <string.h>
#include <stdlib.h>

#include "glade-project.h"
#include "glade-widget.h"
#include "glade-editable.h"

G_DEFINE_INTERFACE (GladeEditable, glade_editable, GTK_TYPE_WIDGET);

static GQuark glade_editable_project_quark = 0;
static GQuark glade_editable_widget_quark = 0;
static GQuark glade_editable_loading_quark = 0;
static GQuark glade_editable_destroy_quark = 0;

static void
project_changed (GladeProject  *project,
                 GladeCommand  *command,
                 gboolean       execute, 
                 GladeEditable *editable)
{
  GladeWidget *widget;

  widget = g_object_get_qdata (G_OBJECT (editable), glade_editable_widget_quark);

  glade_editable_load (editable, widget);
}

static void
project_closed (GladeProject  *project,
                GladeEditable *editable)
{
  glade_editable_load (editable, NULL);
}

static void
editable_destroyed (GladeEditable *editable)
{
  glade_editable_load (editable, NULL);
}

static void
glade_editable_load_default (GladeEditable  *editable,
                             GladeWidget    *widget)
{
  GladeWidget  *old_widget;
  GladeProject *old_project;

  old_widget  = g_object_get_qdata (G_OBJECT (editable), glade_editable_widget_quark);
  old_project = g_object_get_qdata (G_OBJECT (editable), glade_editable_project_quark);

  if (old_widget != widget)
    {
      if (old_widget)
        {
          g_signal_handlers_disconnect_by_func (old_project, G_CALLBACK (project_changed), editable);
          g_signal_handlers_disconnect_by_func (old_project, G_CALLBACK (project_closed), editable);

          g_object_set_qdata (G_OBJECT (editable), glade_editable_widget_quark, NULL);
          g_object_set_qdata (G_OBJECT (editable), glade_editable_project_quark, NULL);
        }

      if (widget)
        {
          GladeProject *project = glade_widget_get_project (widget);

          g_object_set_qdata (G_OBJECT (editable), glade_editable_widget_quark, widget);
          g_object_set_qdata (G_OBJECT (editable), glade_editable_project_quark, project);

          g_signal_connect (project, "changed", 
                            G_CALLBACK (project_changed), editable);
          g_signal_connect (project, "close", 
                            G_CALLBACK (project_closed), editable);
        }
    }
}

static void
glade_editable_default_init (GladeEditableInterface *iface)
{
  glade_editable_project_quark = g_quark_from_static_string ("glade-editable-project-quark");
  glade_editable_widget_quark  = g_quark_from_static_string ("glade-editable-widget-quark");
  glade_editable_loading_quark = g_quark_from_static_string ("glade-editable-loading-quark");
  glade_editable_destroy_quark = g_quark_from_static_string ("glade-editable-destroy-quark");

  iface->load = glade_editable_load_default;
}

/**
 * glade_editable_load:
 * @editable: A #GladeEditable
 * @widget: the #GladeWidget to load
 *
 * Loads @widget property values into @editable
 * (the editable will watch the widgets properties
 * until its loaded with another widget or %NULL)
 */
void
glade_editable_load (GladeEditable *editable, GladeWidget *widget)
{
  GladeEditableInterface *iface;
  g_return_if_fail (GLADE_IS_EDITABLE (editable));
  g_return_if_fail (widget == NULL || GLADE_IS_WIDGET (widget));

  /* Connect to the destroy signal once, make sure we unload the widget and disconnect
   * to any signals when destroying
   */
  if (!GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (editable), glade_editable_destroy_quark)))
    {
      g_signal_connect (editable, "destroy", G_CALLBACK (editable_destroyed), NULL);
      g_object_set_qdata (G_OBJECT (editable), glade_editable_destroy_quark, GINT_TO_POINTER (TRUE));
    }

  iface = GLADE_EDITABLE_GET_IFACE (editable);

  g_object_set_qdata (G_OBJECT (editable), glade_editable_loading_quark, GINT_TO_POINTER (TRUE));

  if (iface->load)
    iface->load (editable, widget);
  else
    g_critical ("No GladeEditable::load() support on type %s",
                G_OBJECT_TYPE_NAME (editable));

  g_object_set_qdata (G_OBJECT (editable), glade_editable_loading_quark, GINT_TO_POINTER (FALSE));
}

/**
 * glade_editable_set_show_name:
 * @editable: A #GladeEditable
 * @show_name: Whether or not to show the name entry
 *
 * This only applies for the general page in the property
 * editor, editables that embed the #GladeEditorTable implementation
 * for the general page should use this to forward the message
 * to its embedded editable.
 */
void
glade_editable_set_show_name (GladeEditable *editable, gboolean show_name)
{
  GladeEditableInterface *iface;
  g_return_if_fail (GLADE_IS_EDITABLE (editable));

  iface = GLADE_EDITABLE_GET_IFACE (editable);

  if (iface->set_show_name)
    iface->set_show_name (editable, show_name);
}

/**
 * glade_editable_loaded_widget:
 * @editable: A #GladeEditable
 *
 * Returns: (transfer none) (nullable): a #GladeWidget or %NULL if the editable hasn't been loaded
 */
GladeWidget *
glade_editable_loaded_widget (GladeEditable *editable)
{
  return g_object_get_qdata (G_OBJECT (editable), glade_editable_widget_quark);
}

gboolean
glade_editable_loading (GladeEditable *editable)
{
  return GPOINTER_TO_INT (g_object_get_qdata (G_OBJECT (editable), glade_editable_loading_quark));
}

void
glade_editable_block (GladeEditable *editable)
{
  GladeProject *project;

  g_return_if_fail (GLADE_IS_EDITABLE (editable));

  project = g_object_get_qdata (G_OBJECT (editable), glade_editable_project_quark);

  g_return_if_fail (GLADE_IS_PROJECT (project));

  g_signal_handlers_block_by_func (project, G_CALLBACK (project_changed), editable);
}

void
glade_editable_unblock (GladeEditable *editable)
{
  GladeProject *project;

  g_return_if_fail (GLADE_IS_EDITABLE (editable));

  project = g_object_get_qdata (G_OBJECT (editable), glade_editable_project_quark);

  g_return_if_fail (GLADE_IS_PROJECT (project));

  g_signal_handlers_unblock_by_func (project, G_CALLBACK (project_changed), editable);
}
