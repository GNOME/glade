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

#include "glade-entry-editor.h"
#include "glade-image-editor.h" // For GladeImageEditMode

static void glade_entry_editor_editable_init (GladeEditableInterface *iface);
static void glade_entry_editor_grab_focus    (GtkWidget          *widget);

/* Callbacks */
static void text_toggled                     (GtkWidget *widget, GladeEntryEditor *entry_editor);
static void buffer_toggled                   (GtkWidget *widget, GladeEntryEditor *entry_editor);
static void primary_stock_toggled            (GtkWidget *widget, GladeEntryEditor *entry_editor);
static void primary_icon_name_toggled        (GtkWidget *widget, GladeEntryEditor *entry_editor);
static void primary_pixbuf_toggled           (GtkWidget *widget, GladeEntryEditor *entry_editor);
static void primary_tooltip_markup_toggled   (GtkWidget *widget, GladeEntryEditor *entry_editor);
static void secondary_stock_toggled          (GtkWidget *widget, GladeEntryEditor *entry_editor);
static void secondary_icon_name_toggled      (GtkWidget *widget, GladeEntryEditor *entry_editor);
static void secondary_pixbuf_toggled         (GtkWidget *widget, GladeEntryEditor *entry_editor);
static void secondary_tooltip_markup_toggled (GtkWidget *widget, GladeEntryEditor *entry_editor);

struct _GladeEntryEditorPrivate
{
  GtkWidget *embed;
  GtkWidget *extension_port;

  GtkWidget *text_radio;
  GtkWidget *buffer_radio;

  GtkWidget *primary_pixbuf_radio;
  GtkWidget *primary_stock_radio;
  GtkWidget *primary_icon_name_radio;
  GtkWidget *primary_tooltip_markup_check;
  GtkWidget *primary_tooltip_notebook;
  GtkWidget *primary_tooltip_editor_notebook;

  GtkWidget *secondary_pixbuf_radio;
  GtkWidget *secondary_stock_radio;
  GtkWidget *secondary_icon_name_radio;
  GtkWidget *secondary_tooltip_markup_check;
  GtkWidget *secondary_tooltip_notebook;
  GtkWidget *secondary_tooltip_editor_notebook;

};

#define TOOLTIP_TEXT_PAGE   0
#define TOOLTIP_MARKUP_PAGE 1

#define ICON_MODE_NAME(primary)       ((primary) ? "primary-icon-mode"    : "secondary-icon-mode")
#define PIXBUF_NAME(primary)          ((primary) ? "primary-icon-pixbuf"  : "secondary-icon-pixbuf")
#define ICON_NAME_NAME(primary)       ((primary) ? "primary-icon-name"    : "secondary-icon-name")
#define STOCK_NAME(primary)           ((primary) ? "primary-icon-stock"   : "secondary-icon-stock")

#define TOOLTIP_MARKUP_NAME(primary)  ((primary) ? "primary-icon-tooltip-markup"  : "secondary-icon-tooltip-markup")
#define TOOLTIP_TEXT_NAME(primary)    ((primary) ? "primary-icon-tooltip-text"    : "secondary-icon-tooltip-text")
#define TOOLTIP_CONTROL_NAME(primary) ((primary) ? "glade-primary-tooltip-markup" : "glade-secondary-tooltip-markup")

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeEntryEditor, glade_entry_editor, GLADE_TYPE_EDITOR_SKELETON,
                         G_ADD_PRIVATE (GladeEntryEditor)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_entry_editor_editable_init));

static void
glade_entry_editor_class_init (GladeEntryEditorClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->grab_focus = glade_entry_editor_grab_focus;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladegtk/glade-entry-editor.ui");

  gtk_widget_class_bind_template_child_internal_private (widget_class, GladeEntryEditor, extension_port);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEntryEditor, embed);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEntryEditor, text_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEntryEditor, buffer_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEntryEditor, primary_stock_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEntryEditor, primary_icon_name_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEntryEditor, primary_pixbuf_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEntryEditor, primary_tooltip_markup_check);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEntryEditor, primary_tooltip_notebook);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEntryEditor, primary_tooltip_editor_notebook);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEntryEditor, secondary_stock_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEntryEditor, secondary_icon_name_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEntryEditor, secondary_pixbuf_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEntryEditor, secondary_tooltip_markup_check);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEntryEditor, secondary_tooltip_notebook);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEntryEditor, secondary_tooltip_editor_notebook);

  gtk_widget_class_bind_template_callback (widget_class, text_toggled);
  gtk_widget_class_bind_template_callback (widget_class, buffer_toggled);
  gtk_widget_class_bind_template_callback (widget_class, primary_stock_toggled);
  gtk_widget_class_bind_template_callback (widget_class, primary_icon_name_toggled);
  gtk_widget_class_bind_template_callback (widget_class, primary_pixbuf_toggled);
  gtk_widget_class_bind_template_callback (widget_class, primary_tooltip_markup_toggled);
  gtk_widget_class_bind_template_callback (widget_class, secondary_stock_toggled);
  gtk_widget_class_bind_template_callback (widget_class, secondary_icon_name_toggled);
  gtk_widget_class_bind_template_callback (widget_class, secondary_pixbuf_toggled);
  gtk_widget_class_bind_template_callback (widget_class, secondary_tooltip_markup_toggled);
}

static void
glade_entry_editor_init (GladeEntryEditor * self)
{
  self->priv = glade_entry_editor_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
glade_entry_editor_load (GladeEditable * editable, GladeWidget * widget)
{
  GladeEntryEditor *entry_editor = GLADE_ENTRY_EDITOR (editable);
  GladeEntryEditorPrivate *priv = entry_editor->priv;
  GladeImageEditMode icon_mode;
  gboolean use_buffer = FALSE;
  gboolean primary_markup = FALSE;
  gboolean secondary_markup = FALSE;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  if (widget)
    {
      glade_widget_property_get (widget, "use-entry-buffer", &use_buffer);
      if (use_buffer)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->buffer_radio), TRUE);
      else
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->text_radio), TRUE);


      glade_widget_property_get (widget, "primary-icon-mode", &icon_mode);
      switch (icon_mode)
        {
          case GLADE_IMAGE_MODE_STOCK:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->primary_stock_radio), TRUE);
            break;
          case GLADE_IMAGE_MODE_ICON:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->primary_icon_name_radio), TRUE);
            break;
          case GLADE_IMAGE_MODE_FILENAME:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->primary_pixbuf_radio), TRUE);
            break;
          default:
            break;
        }

      glade_widget_property_get (widget, "secondary-icon-mode", &icon_mode);
      switch (icon_mode)
        {
          case GLADE_IMAGE_MODE_STOCK:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->secondary_stock_radio), TRUE);
            break;
          case GLADE_IMAGE_MODE_ICON:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->secondary_icon_name_radio), TRUE);
            break;
          case GLADE_IMAGE_MODE_FILENAME:
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->secondary_pixbuf_radio), TRUE);
            break;
          default:
            break;
        }

      /* Fix up notebook pages and check button state for primary / secondary tooltip markup */
      glade_widget_property_get (widget, TOOLTIP_CONTROL_NAME (TRUE), &primary_markup);
      glade_widget_property_get (widget, TOOLTIP_CONTROL_NAME (FALSE), &secondary_markup);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->primary_tooltip_markup_check), primary_markup);
      gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->primary_tooltip_notebook),
                                     primary_markup ? TOOLTIP_MARKUP_PAGE : TOOLTIP_TEXT_PAGE);
      gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->primary_tooltip_editor_notebook),
                                     primary_markup ? TOOLTIP_MARKUP_PAGE : TOOLTIP_TEXT_PAGE);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->secondary_tooltip_markup_check), secondary_markup);
      gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->secondary_tooltip_notebook),
                                     secondary_markup ? TOOLTIP_MARKUP_PAGE : TOOLTIP_TEXT_PAGE);
      gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->secondary_tooltip_editor_notebook),
                                     secondary_markup ? TOOLTIP_MARKUP_PAGE : TOOLTIP_TEXT_PAGE);
    }
}

static void
glade_entry_editor_set_show_name (GladeEditable * editable, gboolean show_name)
{
  GladeEntryEditor *entry_editor = GLADE_ENTRY_EDITOR (editable);

  glade_editable_set_show_name (GLADE_EDITABLE (entry_editor->priv->embed), show_name);
}

static void
glade_entry_editor_editable_init (GladeEditableInterface * iface)
{
  parent_editable_iface = g_type_interface_peek_parent (iface);

  iface->load = glade_entry_editor_load;
  iface->set_show_name = glade_entry_editor_set_show_name;
}

static void
glade_entry_editor_grab_focus (GtkWidget * widget)
{
  GladeEntryEditor *entry_editor = GLADE_ENTRY_EDITOR (widget);

  gtk_widget_grab_focus (entry_editor->priv->embed);
}

static void
text_toggled (GtkWidget * widget, GladeEntryEditor * entry_editor)
{
  GladeEntryEditorPrivate *priv = entry_editor->priv;
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  if (glade_editable_loading (GLADE_EDITABLE (entry_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (priv->text_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (entry_editor));

  glade_command_push_group (_("Setting %s to use static text"),
                            glade_widget_get_name (gwidget));

  property = glade_widget_get_property (gwidget, "buffer");
  glade_command_set_property (property, NULL);

  property = glade_widget_get_property (gwidget, "use-entry-buffer");
  glade_command_set_property (property, FALSE);

  /* Text will only take effect after setting the property under the hood */
  property = glade_widget_get_property (gwidget, "text");
  glade_command_set_property (property, NULL);

  /* Incase the NULL text didnt change */
  glade_property_sync (property);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (entry_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (entry_editor), gwidget);
}

static void
buffer_toggled (GtkWidget * widget, GladeEntryEditor * entry_editor)
{
  GladeEntryEditorPrivate *priv = entry_editor->priv;
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  if (glade_editable_loading (GLADE_EDITABLE (entry_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (priv->buffer_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (entry_editor));

  glade_command_push_group (_("Setting %s to use an external buffer"),
                            glade_widget_get_name (gwidget));

  /* Reset the text while still in static text mode */
  property = glade_widget_get_property (gwidget, "text");
  glade_command_set_property (property, NULL);

  property = glade_widget_get_property (gwidget, "use-entry-buffer");
  glade_command_set_property (property, TRUE);

  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (entry_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (entry_editor), gwidget);
}

static void
set_stock_mode (GladeEntryEditor * entry_editor, gboolean primary)
{
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));
  GValue value = { 0, };

  property = glade_widget_get_property (gwidget, ICON_NAME_NAME (primary));
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, PIXBUF_NAME (primary));
  glade_command_set_property (property, NULL);

  property = glade_widget_get_property (gwidget, STOCK_NAME (primary));
  glade_property_get_default (property, &value);
  glade_command_set_property_value (property, &value);
  g_value_unset (&value);

  property = glade_widget_get_property (gwidget, ICON_MODE_NAME (primary));
  glade_command_set_property (property, GLADE_IMAGE_MODE_STOCK);
}

static void
set_icon_name_mode (GladeEntryEditor * entry_editor, gboolean primary)
{
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  property = glade_widget_get_property (gwidget, STOCK_NAME (primary));
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, PIXBUF_NAME (primary));
  glade_command_set_property (property, NULL);
  property = glade_widget_get_property (gwidget, ICON_MODE_NAME (primary));
  glade_command_set_property (property, GLADE_IMAGE_MODE_ICON);
}

static void
set_pixbuf_mode (GladeEntryEditor * entry_editor, gboolean primary)
{
  GladeProperty *property;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  property = glade_widget_get_property (gwidget, STOCK_NAME (primary));
  glade_command_set_property (property, NULL);

  property = glade_widget_get_property (gwidget, ICON_NAME_NAME (primary));
  glade_command_set_property (property, NULL);

  property = glade_widget_get_property (gwidget, ICON_MODE_NAME (primary));
  glade_command_set_property (property, GLADE_IMAGE_MODE_FILENAME);
}

static void
primary_stock_toggled (GtkWidget * widget, GladeEntryEditor * entry_editor)
{
  GladeEntryEditorPrivate *priv = entry_editor->priv;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  if (glade_editable_loading (GLADE_EDITABLE (entry_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (priv->primary_stock_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (entry_editor));

  glade_command_push_group (_("Setting %s to use a primary icon from stock"),
                            glade_widget_get_name (gwidget));
  set_stock_mode (entry_editor, TRUE);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (entry_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (entry_editor), gwidget);
}

static void
primary_icon_name_toggled (GtkWidget * widget, GladeEntryEditor * entry_editor)
{
  GladeEntryEditorPrivate *priv = entry_editor->priv;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  if (glade_editable_loading (GLADE_EDITABLE (entry_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (priv->primary_icon_name_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (entry_editor));

  glade_command_push_group (_("Setting %s to use a primary icon from the icon theme"),
                            glade_widget_get_name (gwidget));
  set_icon_name_mode (entry_editor, TRUE);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (entry_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (entry_editor), gwidget);
}

static void
primary_pixbuf_toggled (GtkWidget * widget, GladeEntryEditor * entry_editor)
{
  GladeEntryEditorPrivate *priv = entry_editor->priv;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  if (glade_editable_loading (GLADE_EDITABLE (entry_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (priv->primary_pixbuf_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (entry_editor));

  glade_command_push_group (_("Setting %s to use a primary icon from filename"),
                            glade_widget_get_name (gwidget));
  set_pixbuf_mode (entry_editor, TRUE);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (entry_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (entry_editor), gwidget);
}

static void
secondary_stock_toggled (GtkWidget * widget, GladeEntryEditor * entry_editor)
{
  GladeEntryEditorPrivate *priv = entry_editor->priv;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  if (glade_editable_loading (GLADE_EDITABLE (entry_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (priv->secondary_stock_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (entry_editor));

  glade_command_push_group (_("Setting %s to use a secondary icon from stock"),
                            glade_widget_get_name (gwidget));
  set_stock_mode (entry_editor, FALSE);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (entry_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (entry_editor), gwidget);
}

static void
secondary_icon_name_toggled (GtkWidget * widget,
                             GladeEntryEditor * entry_editor)
{
  GladeEntryEditorPrivate *priv = entry_editor->priv;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  if (glade_editable_loading (GLADE_EDITABLE (entry_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (priv->secondary_icon_name_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (entry_editor));

  glade_command_push_group (_("Setting %s to use a secondary icon from the icon theme"),
                            glade_widget_get_name (gwidget));
  set_icon_name_mode (entry_editor, FALSE);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (entry_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (entry_editor), gwidget);
}

static void
secondary_pixbuf_toggled (GtkWidget * widget, GladeEntryEditor * entry_editor)
{
  GladeEntryEditorPrivate *priv = entry_editor->priv;
  GladeWidget   *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));

  if (glade_editable_loading (GLADE_EDITABLE (entry_editor)) || !gwidget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (priv->secondary_pixbuf_radio)))
    return;

  glade_editable_block (GLADE_EDITABLE (entry_editor));

  glade_command_push_group (_("Setting %s to use a secondary icon from filename"),
                            glade_widget_get_name (gwidget));
  set_pixbuf_mode (entry_editor, FALSE);
  glade_command_pop_group ();

  glade_editable_unblock (GLADE_EDITABLE (entry_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (entry_editor), gwidget);
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
toggle_tooltip_markup (GladeEntryEditor *entry_editor,
                       GtkWidget        *widget,
                       gboolean          primary)
{
  GladeProperty *property;
  GladeWidget *gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (entry_editor));
  gboolean active;

  if (glade_editable_loading (GLADE_EDITABLE (entry_editor)) || !gwidget)
    return;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

  glade_editable_block (GLADE_EDITABLE (entry_editor));

  if (active)
    {
      if (primary)
        glade_command_push_group (_("Setting primary icon of %s to use tooltip markup"),
                                  glade_widget_get_name (gwidget));
      else
        glade_command_push_group (_("Setting secondary icon of %s to use tooltip markup"),
                                  glade_widget_get_name (gwidget));

      transfer_text_property (gwidget, TOOLTIP_TEXT_NAME (primary), TOOLTIP_MARKUP_NAME (primary));

      property = glade_widget_get_property (gwidget, TOOLTIP_CONTROL_NAME (primary));
      glade_command_set_property (property, TRUE);

      glade_command_pop_group ();
    }
  else
    {
      if (primary)
        glade_command_push_group (_("Setting primary icon of %s to not use tooltip markup"),
                                  glade_widget_get_name (gwidget));
      else
        glade_command_push_group (_("Setting secondary icon of %s to not use tooltip markup"),
                                  glade_widget_get_name (gwidget));

      transfer_text_property (gwidget, TOOLTIP_MARKUP_NAME (primary), TOOLTIP_TEXT_NAME (primary));

      property = glade_widget_get_property (gwidget, TOOLTIP_CONTROL_NAME (primary));
      glade_command_set_property (property, FALSE);

      glade_command_pop_group ();
    }

  glade_editable_unblock (GLADE_EDITABLE (entry_editor));

  /* reload widgets and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (entry_editor), gwidget);
}

static void
primary_tooltip_markup_toggled (GtkWidget        *widget,
                                GladeEntryEditor *entry_editor)
{
  toggle_tooltip_markup (entry_editor, widget, TRUE);
}

static void
secondary_tooltip_markup_toggled (GtkWidget        *widget,
                                  GladeEntryEditor *entry_editor)
{
  toggle_tooltip_markup (entry_editor, widget, FALSE);
}

GtkWidget *
glade_entry_editor_new (void)
{
  return g_object_new (GLADE_TYPE_ENTRY_EDITOR, NULL);
}

/*************************************
 *     Private Plugin Extensions     *
 *************************************/
void
glade_entry_editor_post_create (GladeWidgetAdaptor *adaptor,
                                GObject            *editor,
                                GladeCreateReason   reason)
{
  GladeEntryEditorPrivate *priv = GLADE_ENTRY_EDITOR (editor)->priv;

  gtk_widget_show (priv->extension_port);
}
