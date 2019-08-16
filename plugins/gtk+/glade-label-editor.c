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

#include "glade-label-editor.h"

/* GtkWidgetClass */
static void glade_label_editor_grab_focus    (GtkWidget          *widget);

/* GladeEditableInterface */
static void glade_label_editor_load          (GladeEditable      *editable,
                                              GladeWidget        *widget);
static void glade_label_editor_editable_init (GladeEditableInterface *iface);

/* Callbacks */
static void attributes_toggled (GtkWidget *widget, GladeLabelEditor *label_editor);
static void markup_toggled     (GtkWidget *widget, GladeLabelEditor *label_editor);
static void pattern_toggled    (GtkWidget *widget, GladeLabelEditor *label_editor);
static void wrap_free_toggled  (GtkWidget *widget, GladeLabelEditor *label_editor);
static void single_toggled     (GtkWidget *widget, GladeLabelEditor *label_editor);
static void wrap_mode_toggled  (GtkWidget *widget, GladeLabelEditor *label_editor);

struct _GladeLabelEditorPrivate
{
  GtkWidget *embed;

  GtkWidget *attributes_radio;    /* Set pango attributes manually (attributes eprop embedded) */
  GtkWidget *markup_radio;        /* Parse the label as a pango markup string (no showing eprop) */
  GtkWidget *pattern_radio;       /* Use a pattern string to underline portions of the text
                                   * (pattern eprop embedded) */

  /* These control whether to use single-line-mode, wrap & wrap-mode or niether */
  GtkWidget *wrap_free_label; /* Set boldness on this label for a fake property */
  GtkWidget *wrap_free_radio;
  GtkWidget *single_radio;
  GtkWidget *wrap_mode_radio;
};

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeLabelEditor, glade_label_editor, GLADE_TYPE_EDITOR_SKELETON,
                         G_ADD_PRIVATE (GladeLabelEditor)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_label_editor_editable_init));

static void
glade_label_editor_class_init (GladeLabelEditorClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = glade_label_editor_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-label-editor.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GladeLabelEditor, embed);
  gtk_widget_class_bind_template_child_private (widget_class, GladeLabelEditor, attributes_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeLabelEditor, markup_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeLabelEditor, pattern_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeLabelEditor, wrap_free_label);
  gtk_widget_class_bind_template_child_private (widget_class, GladeLabelEditor, wrap_free_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeLabelEditor, single_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeLabelEditor, wrap_mode_radio);

  gtk_widget_class_bind_template_callback (widget_class, attributes_toggled);
  gtk_widget_class_bind_template_callback (widget_class, markup_toggled);
  gtk_widget_class_bind_template_callback (widget_class, pattern_toggled);
  gtk_widget_class_bind_template_callback (widget_class, wrap_free_toggled);
  gtk_widget_class_bind_template_callback (widget_class, single_toggled);
  gtk_widget_class_bind_template_callback (widget_class, wrap_mode_toggled);
}

static void
glade_label_editor_init (GladeLabelEditor *self)
{
  self->priv = glade_label_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_label_editor_load (GladeEditable *editable, GladeWidget *widget)
{
  GladeLabelEditor *label_editor = GLADE_LABEL_EDITOR (editable);
  GladeLabelEditorPrivate *priv = label_editor->priv;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  if (widget)
    {
      GladeLabelContentMode content_mode;
      GladeLabelWrapMode wrap_mode;
      static PangoAttrList *italic_attr_list = NULL;
      gboolean use_max_width;

      if (!italic_attr_list)
        {
          PangoAttribute *attr;
          italic_attr_list = pango_attr_list_new ();
          attr = pango_attr_style_new (PANGO_STYLE_ITALIC);
          pango_attr_list_insert (italic_attr_list, attr);
        }

      glade_widget_property_get (widget, "label-content-mode", &content_mode);
      glade_widget_property_get (widget, "label-wrap-mode", &wrap_mode);
      glade_widget_property_get (widget, "use-max-width", &use_max_width);

      switch (content_mode)
        {
          case GLADE_LABEL_MODE_ATTRIBUTES:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (priv->attributes_radio),
                                          TRUE);
            break;
          case GLADE_LABEL_MODE_MARKUP:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (priv->markup_radio), TRUE);
            break;
          case GLADE_LABEL_MODE_PATTERN:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (priv->pattern_radio), TRUE);
            break;
          default:
            break;
        }

      if (wrap_mode == GLADE_LABEL_WRAP_FREE)
        gtk_label_set_attributes (GTK_LABEL (priv->wrap_free_label),
                                  italic_attr_list);
      else
        gtk_label_set_attributes (GTK_LABEL (priv->wrap_free_label),
                                  NULL);

      switch (wrap_mode)
        {
          case GLADE_LABEL_WRAP_FREE:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (priv->wrap_free_radio),
                                          TRUE);
            break;
          case GLADE_LABEL_SINGLE_LINE:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (priv->single_radio), TRUE);
            break;
          case GLADE_LABEL_WRAP_MODE:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                          (priv->wrap_mode_radio),
                                          TRUE);
            break;
          default:
            break;
        }
    }
}

static void
glade_label_editor_editable_init (GladeEditableInterface *iface)
{
  parent_editable_iface = g_type_interface_peek_parent (iface);

  iface->load = glade_label_editor_load;
}

static void
glade_label_editor_grab_focus (GtkWidget *widget)
{
  GladeLabelEditor        *label_editor = GLADE_LABEL_EDITOR (widget);
  GladeLabelEditorPrivate *priv = label_editor->priv;

  gtk_widget_grab_focus (priv->embed);
}

/**********************************************************************
                    label-content-mode radios
 **********************************************************************/
static void
attributes_toggled (GtkWidget *widget, GladeLabelEditor *label_editor)
{
  GladeLabelEditorPrivate *priv = label_editor->priv;
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (label_editor));

  if (glade_editable_loading (GLADE_EDITABLE (label_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (priv->attributes_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (label_editor));

  glade_command_push_group (_("Setting %s to use an attribute list"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "use-markup");
  glade_command_set_property (property, FALSE);
  property = glade_widget_get_property (gwidget, "pattern");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "label-content-mode");
  glade_command_set_property (property, GLADE_LABEL_MODE_ATTRIBUTES);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (label_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (label_editor), gwidget);
}

static void
markup_toggled (GtkWidget *widget, GladeLabelEditor *label_editor)
{
  GladeLabelEditorPrivate *priv = label_editor->priv;
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (label_editor));

  if (glade_editable_loading (GLADE_EDITABLE (label_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (priv->markup_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (label_editor));

  glade_command_push_group (_("Setting %s to use a Pango markup string"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "pattern");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "glade-attributes");
  glade_command_set_property (property, NULL);

  property = glade_widget_get_property (gwidget, "use-markup");
  glade_command_set_property (property, TRUE);
  property = glade_widget_get_property (gwidget, "label-content-mode");
  glade_command_set_property (property, GLADE_LABEL_MODE_MARKUP);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (label_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (label_editor), gwidget);
}

static void
pattern_toggled (GtkWidget *widget, GladeLabelEditor *label_editor)
{
  GladeLabelEditorPrivate *priv = label_editor->priv;
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (label_editor));

  if (glade_editable_loading (GLADE_EDITABLE (label_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (priv->pattern_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (label_editor));

  glade_command_push_group (_("Setting %s to use a pattern string"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "glade-attributes");
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, "use-markup");
  glade_command_set_property (property, FALSE);
  property = glade_widget_get_property (gwidget, "label-content-mode");
  glade_command_set_property (property, GLADE_LABEL_MODE_PATTERN);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (label_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (label_editor), gwidget);
}

/**********************************************************************
                    label-wrap-mode radios
 **********************************************************************/
static void
wrap_free_toggled (GtkWidget *widget, GladeLabelEditor *label_editor)
{
  GladeLabelEditorPrivate *priv = label_editor->priv;
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (label_editor));

  if (glade_editable_loading (GLADE_EDITABLE (label_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (priv->wrap_free_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (label_editor));

  glade_command_push_group (_("Setting %s to use normal line wrapping"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "single-line-mode");
  glade_command_set_property (property, FALSE);
  property = glade_widget_get_property (gwidget, "wrap-mode");
  glade_command_set_property (property, PANGO_WRAP_WORD);
  property = glade_widget_get_property (gwidget, "wrap");
  glade_command_set_property (property, FALSE);

  property = glade_widget_get_property (gwidget, "label-wrap-mode");
  glade_command_set_property (property, GLADE_LABEL_WRAP_FREE);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (label_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (label_editor), gwidget);
}

static void
single_toggled (GtkWidget *widget, GladeLabelEditor *label_editor)
{
  GladeLabelEditorPrivate *priv = label_editor->priv;
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (label_editor));

  if (glade_editable_loading (GLADE_EDITABLE (label_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (priv->single_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (label_editor));

  glade_command_push_group (_("Setting %s to use a single line"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "wrap-mode");
  glade_command_set_property (property, PANGO_WRAP_WORD);
  property = glade_widget_get_property (gwidget, "wrap");
  glade_command_set_property (property, FALSE);

  property = glade_widget_get_property (gwidget, "single-line-mode");
  glade_command_set_property (property, TRUE);
  property = glade_widget_get_property (gwidget, "label-wrap-mode");
  glade_command_set_property (property, GLADE_LABEL_SINGLE_LINE);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (label_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (label_editor), gwidget);
}

static void
wrap_mode_toggled (GtkWidget *widget, GladeLabelEditor *label_editor)
{
  GladeLabelEditorPrivate *priv = label_editor->priv;
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (label_editor));

  if (glade_editable_loading (GLADE_EDITABLE (label_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (priv->wrap_mode_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (label_editor));

  glade_command_push_group (_("Setting %s to use specific Pango word wrapping"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "single-line-mode");
  glade_command_set_property (property, FALSE);
  property = glade_widget_get_property (gwidget, "wrap");
  glade_command_set_property (property, TRUE);

  property = glade_widget_get_property (gwidget, "label-wrap-mode");
  glade_command_set_property (property, GLADE_LABEL_WRAP_MODE);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (label_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (label_editor), gwidget);
}

GtkWidget *
glade_label_editor_new (void)
{
  return g_object_new (GLADE_TYPE_LABEL_EDITOR, NULL);
}
