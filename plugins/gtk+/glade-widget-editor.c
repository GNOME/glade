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

#include "glade-widget-editor.h"


static void glade_widget_editor_finalize (GObject *object);

static void glade_widget_editor_editable_init (GladeEditableInterface *iface);

static void markup_toggled (GtkWidget * widget, GladeWidgetEditor *widget_editor);
static void custom_tooltip_toggled (GtkWidget *widget, GladeWidgetEditor *widget_editor);

struct _GladeWidgetEditorPrivate
{
  GtkWidget *custom_tooltip_check;
  GtkWidget *tooltip_markup_check;
  GtkWidget *tooltip_label_notebook;
  GtkWidget *tooltip_editor_notebook;

  /* GtkContainer common properties */
  GtkWidget *resize_mode_label;
  GtkWidget *resize_mode_editor;
  GtkWidget *border_width_label;
  GtkWidget *border_width_editor;
};

static GladeEditableInterface *parent_editable_iface;

#define TOOLTIP_TEXT_PAGE   0
#define TOOLTIP_MARKUP_PAGE 1

G_DEFINE_TYPE_WITH_CODE (GladeWidgetEditor, glade_widget_editor, GLADE_TYPE_EDITOR_SKELETON,
                         G_ADD_PRIVATE (GladeWidgetEditor)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_widget_editor_editable_init));

static void
glade_widget_editor_class_init (GladeWidgetEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = glade_widget_editor_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-widget-editor.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GladeWidgetEditor, custom_tooltip_check);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWidgetEditor, tooltip_markup_check);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWidgetEditor, tooltip_label_notebook);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWidgetEditor, tooltip_editor_notebook);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWidgetEditor, resize_mode_label);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWidgetEditor, resize_mode_editor);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWidgetEditor, border_width_label);
  gtk_widget_class_bind_template_child_private (widget_class, GladeWidgetEditor, border_width_editor);

  gtk_widget_class_bind_template_callback (widget_class, markup_toggled);
  gtk_widget_class_bind_template_callback (widget_class, custom_tooltip_toggled);
}

static void
glade_widget_editor_init (GladeWidgetEditor *self)
{
  self->priv = glade_widget_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_widget_editor_load (GladeEditable *editable, GladeWidget *gwidget)
{
  GladeWidgetEditor        *widget_editor = GLADE_WIDGET_EDITOR (editable);
  GladeWidgetEditorPrivate *priv = widget_editor->priv;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, gwidget);

  if (gwidget)
    {
      GtkWidget *widget = (GtkWidget *)glade_widget_get_object (gwidget);
      gboolean is_container = GTK_IS_CONTAINER (widget);
      gboolean tooltip_markup = FALSE;
      gboolean custom_tooltip = FALSE;

      glade_widget_property_get (gwidget, "glade-tooltip-markup", &tooltip_markup);
      glade_widget_property_get (gwidget, "has-tooltip", &custom_tooltip);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->tooltip_markup_check), tooltip_markup);
      gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->tooltip_label_notebook),
                                     tooltip_markup ? TOOLTIP_MARKUP_PAGE : TOOLTIP_TEXT_PAGE);
      gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->tooltip_editor_notebook),
                                     tooltip_markup ? TOOLTIP_MARKUP_PAGE : TOOLTIP_TEXT_PAGE);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->custom_tooltip_check), custom_tooltip);
      gtk_widget_set_sensitive (priv->tooltip_markup_check, !custom_tooltip);
      gtk_widget_set_sensitive (priv->tooltip_label_notebook, !custom_tooltip);
      gtk_widget_set_sensitive (priv->tooltip_editor_notebook, !custom_tooltip);

      /* Set visibility of GtkContainer only properties */
      gtk_widget_set_visible (priv->resize_mode_label,   is_container);
      gtk_widget_set_visible (priv->resize_mode_editor,  is_container);
      gtk_widget_set_visible (priv->border_width_label,  is_container);
      gtk_widget_set_visible (priv->border_width_editor, is_container);
    }
}

static void
glade_widget_editor_editable_init (GladeEditableInterface *iface)
{
  parent_editable_iface = g_type_interface_peek_parent (iface);

  iface->load = glade_widget_editor_load;
}

static void
glade_widget_editor_finalize (GObject *object)
{
  glade_editable_load (GLADE_EDITABLE (object), NULL);

  G_OBJECT_CLASS (glade_widget_editor_parent_class)->finalize (object);
}

static void
custom_tooltip_toggled (GtkWidget         *widget,
                        GladeWidgetEditor *widget_editor)
{
  GladeProperty *property;
  GladeWidget *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (widget_editor));
  gboolean active;

  if (glade_editable_loading (GLADE_EDITABLE (widget_editor)) || !gwidget)
    return;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

  glade_editable_block (GLADE_EDITABLE (widget_editor));

  if (active)
    {
      glade_command_push_group (_("Setting %s to use a custom tooltip"),
                                glade_widget_get_name (gwidget));

      /* clear out some things... */
      property = glade_widget_get_property (gwidget, "tooltip-text");
      glade_command_set_property (property, NULL);

      property = glade_widget_get_property (gwidget, "tooltip-markup");
      glade_command_set_property (property, NULL);

      property = glade_widget_get_property (gwidget, "glade-tooltip-markup");
      glade_command_set_property (property, FALSE);

      property = glade_widget_get_property (gwidget, "has-tooltip");
      glade_command_set_property (property, TRUE);

      glade_command_pop_group ();
    }
  else
    {
      glade_command_push_group (_("Setting %s to use a custom tooltip"),
                                glade_widget_get_name (gwidget));

      /* clear out some things... */
      property = glade_widget_get_property (gwidget, "tooltip-text");
      glade_command_set_property (property, NULL);

      property = glade_widget_get_property (gwidget, "tooltip-markup");
      glade_command_set_property (property, NULL);

      property = glade_widget_get_property (gwidget, "glade-tooltip-markup");
      glade_command_set_property (property, FALSE);

      property = glade_widget_get_property (gwidget, "has-tooltip");
      glade_command_set_property (property, FALSE);

      glade_command_pop_group ();
    }

  glade_editable_unblock (GLADE_EDITABLE (widget_editor));

  /* reload widgets and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (widget_editor), gwidget);
}

static void
transfer_text_property (GladeWidget *gwidget,
                        const gchar *from,
                        const gchar *to)
{
  gchar *value = NULL;
  gchar *comment = NULL, *context = NULL;
  gboolean translatable = FALSE;
  GladeProperty *prop_from;
  GladeProperty *prop_to;

  prop_from = glade_widget_get_property (gwidget, from);
  prop_to   = glade_widget_get_property (gwidget, to);
  g_assert (prop_from);
  g_assert (prop_to);

  glade_property_get (prop_from, &value);
  comment      = (gchar *)glade_property_i18n_get_comment (prop_from);
  context      = (gchar *)glade_property_i18n_get_context (prop_from);
  translatable = glade_property_i18n_get_translatable (prop_from);

  /* Get our own copies */
  value   = g_strdup (value);
  context = g_strdup (context);
  comment = g_strdup (comment);

  /* Set target values */
  glade_command_set_property (prop_to, value);
  glade_command_set_i18n (prop_to, translatable, context, comment);

  /* Clear source values */
  glade_command_set_property (prop_from, NULL);
  glade_command_set_i18n (prop_from, TRUE, NULL, NULL);

  g_free (value);
  g_free (comment);
  g_free (context);
}

static void
markup_toggled (GtkWidget         *widget,
                GladeWidgetEditor *widget_editor)
{
  GladeProperty *property;
  GladeWidget *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (widget_editor));
  gboolean active;


  if (glade_editable_loading (GLADE_EDITABLE (widget_editor)) || !gwidget)
    return;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

  glade_editable_block (GLADE_EDITABLE (widget_editor));

  if (active)
    {
      glade_command_push_group (_("Setting %s to use tooltip markup"),
                                glade_widget_get_name (gwidget));

      transfer_text_property (gwidget, "tooltip-text", "tooltip-markup");

      property = glade_widget_get_property (gwidget, "glade-tooltip-markup");
      glade_command_set_property (property, TRUE);

      glade_command_pop_group ();
    }
  else
    {
      glade_command_push_group (_("Setting %s to not use tooltip markup"),
                                glade_widget_get_name (gwidget));

      transfer_text_property (gwidget, "tooltip-markup", "tooltip-text");

      property = glade_widget_get_property (gwidget, "glade-tooltip-markup");
      glade_command_set_property (property, FALSE);

      glade_command_pop_group ();
    }

  glade_editable_unblock (GLADE_EDITABLE (widget_editor));

  /* reload widgets and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (widget_editor), gwidget);
}

GtkWidget *
glade_widget_editor_new (void)
{
  return g_object_new (GLADE_TYPE_WIDGET_EDITOR, NULL);
}
