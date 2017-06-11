/*
 * Copyright (C) 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Chema Celorio <chema@celorio.com>
 *   Tristan Van Berkom <tvb@gnome.org>
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 * SECTION:glade-editor
 * @Short_Description: A Widget to edit a #GladeWidget.
 *
 * This is the Glade Notebook containing all the controls needed to configure a #GladeWidget.
 */

#include <stdio.h>
#include <string.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-widget-adaptor.h"
#include "glade-editor.h"
#include "glade-signal-editor.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-command.h"
#include "glade-debug.h"
#include "glade-marshallers.h"
#include "glade-project.h"
#include "glade-utils.h"
#include "glade-editor-property.h"

static void glade_editor_switch_page (GtkNotebook     *notebook,
				      GtkWidget       *page,
				      guint            page_num,
				      GladeEditor     *editor);

enum
{
  PROP_0,
  PROP_SHOW_INFO,
  PROP_WIDGET,
  PROP_SHOW_CLASS_FIELD,
  PROP_CLASS_FIELD,
  PROP_SHOW_BORDER,
  N_PROPERTIES
};

#define GLADE_EDITOR_PRIVATE(object) (((GladeEditor*)object)->priv)

struct _GladeEditorPrivate
{

  GtkWidget *notebook; /* The notebook widget */

  GladeWidget *loaded_widget; /* A handy pointer to the GladeWidget
			       * that is loaded in the editor. NULL
			       * if no widgets are selected
			       */

  GladeWidgetAdaptor *loaded_adaptor; /* A pointer to the loaded
				       * GladeWidgetAdaptor. Note that we can
				       * have a class loaded without a
				       * loaded_widget. For this reason we
				       * can't use loaded_widget->adaptor.
				       * When a widget is selected we load
				       * this class in the editor first and
				       * then fill the values of the inputs
				       * with the GladeProperty items.
				       * This is usefull for not having
				       * to redraw/container_add the widgets
				       * when we switch from widgets of the
				       * same class
				       */

  GtkWidget *page_widget;
  GtkWidget *page_packing;
  GtkWidget *page_common;
  GtkWidget *page_atk;

  GladeSignalEditor *signal_editor; /* The signal editor packed into vbox_signals
				     */

  GList *editables;     /* A list of GladeEditables. We have a widget
			 * for each GladeWidgetAdaptor and we only load
			 * them on demand
			 */
	
  GtkWidget *packing_page; /* Packing pages are dynamicly created each
			    * selection, this pointer is only to free
			    * the last packing page.
			    */
  
  gboolean loading; /* Use when loading a GladeWidget into the editor
		     * we set this flag so that we can ignore the
		     * "changed" signal of the name entry text since
		     * the name has not really changed, just a new name
		     * was loaded.
		     */

  gulong project_closed_signal_id; /* Unload widget when widget's project closes  */
  gulong project_removed_signal_id; /* Unload widget when its removed from the project. */
  gulong widget_warning_id; /* Update when widget changes warning messages. */
  gulong widget_name_id;    /* Update class field when widget name changes  */

  GtkWidget *class_field; /* The class header */

  GtkWidget *warning;   /* A pointer to an icon we can show in the class
			 * field to publish tooltips for class related
			 * versioning errors.
			 */

  GtkWidget *class_icon; /* An image with the current widget's class icon.  */
  GtkWidget *class_label; /* A label with the current class label. */

  gboolean show_class_field; /* Whether or not to show the class field at the top */
};

G_DEFINE_TYPE_WITH_PRIVATE (GladeEditor, glade_editor, GTK_TYPE_BOX);

static GParamSpec *properties[N_PROPERTIES];

static void
glade_editor_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GladeEditor *editor = GLADE_EDITOR (object);

  switch (prop_id)
    {
      case PROP_SHOW_INFO:
        break;
      case PROP_WIDGET:
        glade_editor_load_widget (editor,
                                  GLADE_WIDGET (g_value_get_object (value)));
        break;
      case PROP_SHOW_CLASS_FIELD:
        if (g_value_get_boolean (value))
          glade_editor_show_class_field (editor);
        else
          glade_editor_hide_class_field (editor);
        break;
      case PROP_SHOW_BORDER:
        gtk_notebook_set_show_border (GTK_NOTEBOOK (editor->priv->notebook),
                                      g_value_get_boolean (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_editor_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GladeEditor *editor = GLADE_EDITOR (object);

  switch (prop_id)
    {
      case PROP_SHOW_INFO:
        g_value_set_boolean (value, FALSE);
        break;
      case PROP_WIDGET:
        g_value_set_object (value, editor->priv->loaded_widget);
        break;
      case PROP_SHOW_CLASS_FIELD:
        g_value_set_boolean (value, editor->priv->show_class_field);
        break;
      case PROP_CLASS_FIELD:
        g_value_set_static_string (value, gtk_label_get_label (GTK_LABEL (editor->priv->class_label)));
        break;
      case PROP_SHOW_BORDER:
        g_value_set_boolean (value, gtk_notebook_get_show_border (GTK_NOTEBOOK (editor->priv->notebook)));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_editor_dispose (GObject *object)
{
  GladeEditorPrivate *priv = GLADE_EDITOR_PRIVATE (object);
  GladeEditor *editor = GLADE_EDITOR (object);

  glade_editor_load_widget (editor, NULL);

  /* Unref all the cached pages */
  g_list_foreach (priv->editables, (GFunc) g_object_unref, NULL);
  priv->editables = (g_list_free (priv->editables), NULL);

  G_OBJECT_CLASS (glade_editor_parent_class)->dispose (object);
}

static void
glade_editor_class_init (GladeEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose      = glade_editor_dispose;
  object_class->set_property = glade_editor_set_property;
  object_class->get_property = glade_editor_get_property;

  /* Properties */
  properties[PROP_SHOW_INFO] =
    g_param_spec_boolean ("show-info",
                          _("Show info"),
                          _("Whether to show an informational "
                            "button for the loaded widget"),
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_DEPRECATED);

  properties[PROP_WIDGET] =
    g_param_spec_object ("widget",
                         _("Widget"),
                         _("The currently loaded widget in this editor"),
                         GLADE_TYPE_WIDGET,
                         G_PARAM_READWRITE);

  properties[PROP_SHOW_CLASS_FIELD] =
    g_param_spec_boolean ("show-class-field",
                          _("Show Class Field"),
                          _("Whether to show the class field at the top"),
                          TRUE,
                          G_PARAM_READWRITE);

  properties[PROP_CLASS_FIELD] =
    g_param_spec_string ("class-field",
                         _("Class Field"),
                         _("The class field string"),
                         NULL,
                         G_PARAM_READABLE);
  properties[PROP_SHOW_BORDER] =
    g_param_spec_boolean ("show-boder",
                          _("Show Border"),
                          _("Whether the border should be shown"),
                          TRUE,
                          G_PARAM_READWRITE);
  
  /* Install all properties */
  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /* Bind to template */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladeui/glade-editor.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GladeEditor, notebook);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEditor, page_widget);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEditor, page_packing);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEditor, page_common);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEditor, page_atk);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEditor, class_field);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEditor, class_icon);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEditor, class_label);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEditor, warning);
  gtk_widget_class_bind_template_child_private (widget_class, GladeEditor, signal_editor);

  gtk_widget_class_bind_template_callback (widget_class, glade_editor_switch_page);
}

static void
glade_editor_update_class_warning_cb (GladeWidget *widget,
                                      GParamSpec  *pspec,
                                      GladeEditor *editor)
{
  GladeEditorPrivate *priv = GLADE_EDITOR_PRIVATE (editor);

  if (glade_widget_support_warning (widget))
    gtk_widget_show (priv->warning);
  else
    gtk_widget_hide (priv->warning);

  gtk_widget_set_tooltip_text (priv->warning, glade_widget_support_warning (widget));
}


static void
glade_editor_update_class_field (GladeEditor *editor)
{
  GladeEditorPrivate *priv = GLADE_EDITOR_PRIVATE (editor);

  if (priv->loaded_widget)
    {
      GladeWidget *widget = priv->loaded_widget;
      gchar *text;

      gtk_image_set_from_icon_name (GTK_IMAGE (priv->class_icon),
                                    glade_widget_adaptor_get_icon_name (priv->loaded_adaptor),
                                    GTK_ICON_SIZE_BUTTON);
      gtk_widget_show (priv->class_icon);

      if (glade_widget_has_name (widget))
        {
          /* translators: %s(Class Title) Properties - %s (ClassName) [%s(WidgetName)]
           * example: Window Properties - GtkWindow [window1]
           */
          text = g_strdup_printf (_("%s Properties - %s [%s]"),
                                  glade_widget_adaptor_get_title (priv->loaded_adaptor),
                                  glade_widget_adaptor_get_name (priv->loaded_adaptor), 
		                  glade_widget_get_display_name (widget));
        }
      else
        {
          /* translators: %s(Class Title) Properties - %s (ClassName)
           * example: Window Properties - GtkWindow
           */
          text = g_strdup_printf (_("%s Properties - %s"),
                                  glade_widget_adaptor_get_title (priv->loaded_adaptor),
                                  glade_widget_adaptor_get_name (priv->loaded_adaptor));
	}
		 
      gtk_label_set_text (GTK_LABEL (priv->class_label), text);
      g_free (text);

      glade_editor_update_class_warning_cb (priv->loaded_widget, NULL, editor);
    }
  else
    {
      gtk_widget_hide (priv->class_icon);
      gtk_widget_hide (priv->warning);
      gtk_label_set_text (GTK_LABEL (priv->class_label), _("Properties"));
    }

  g_object_notify_by_pspec (G_OBJECT (editor), properties[PROP_CLASS_FIELD]);
}

static void
glade_editor_update_widget_name_cb (GladeWidget *widget,
                                    GParamSpec  *pspec,
                                    GladeEditor *editor)
{
  glade_editor_update_class_field (editor);
}

static void
glade_editor_switch_page (GtkNotebook     *notebook,
			  GtkWidget       *page,
			  guint            page_num,
			  GladeEditor     *editor)
{
  GladeEditorPrivate *priv = GLADE_EDITOR_PRIVATE (editor);

  gtk_widget_hide (priv->page_widget);
  gtk_widget_hide (priv->page_packing);
  gtk_widget_hide (priv->page_common);
  gtk_widget_hide (priv->page_atk);

  switch (page_num)
    {
    case 0:
      gtk_widget_show (priv->page_widget);
      break;
    case 1:
      gtk_widget_show (priv->page_packing);
      break;
    case 2:
      gtk_widget_show (priv->page_common);
      break;
    case 4:
      gtk_widget_show (priv->page_atk);
      break;
    }
}

static void
glade_editor_init (GladeEditor *editor)
{
  GladeEditorPrivate *priv;
  gint                icon_height;

  editor->priv = priv = glade_editor_get_instance_private (editor);

  priv->show_class_field = TRUE;

  gtk_widget_init_template (GTK_WIDGET (editor));

  gtk_icon_size_lookup (GTK_ICON_SIZE_BUTTON, NULL, &icon_height);
  gtk_widget_set_size_request (priv->class_label, -1, icon_height + 2);

  glade_editor_update_class_field (editor);
}

static GtkWidget *
glade_editor_get_editable_by_adaptor (GladeEditor        *editor,
                                      GladeWidgetAdaptor *adaptor,
                                      GladeEditorPageType type)
{
  GladeEditorPrivate *priv = GLADE_EDITOR_PRIVATE (editor);
  GtkWidget *editable;
  GList *list;

  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  for (list = priv->editables; list; list = list->next)
    {
      editable = list->data;
      if (type !=
          GPOINTER_TO_INT (g_object_get_data
                           (G_OBJECT (editable), "glade-editor-page-type")))
        continue;
      if (g_object_get_data (G_OBJECT (editable), "glade-widget-adaptor") ==
          adaptor)
        return editable;
    }

  editable = (GtkWidget *) glade_widget_adaptor_create_editable (adaptor, type);
  g_return_val_if_fail (editable != NULL, NULL);

  g_object_set_data (G_OBJECT (editable), "glade-editor-page-type",
                     GINT_TO_POINTER (type));
  g_object_set_data (G_OBJECT (editable), "glade-widget-adaptor", adaptor);

  if (type != GLADE_PAGE_PACKING)
    {
      priv->editables = g_list_prepend (priv->editables, editable);
      g_object_ref_sink (editable);
    }

  return editable;
}

static void
hide_or_remove_visible_child (GtkContainer *container, gboolean remove)
{
  GList *l, *children = gtk_container_get_children (container);
  GtkWidget *widget;

  for (l = children; l; l = l->next)
    {
      widget = l->data;

      if (gtk_widget_get_visible (widget))
	{
	  gtk_widget_hide (widget);

	  if (remove)
	    gtk_container_remove (container, widget);

	  break;
	}
    }
  g_list_free (children);
}

static GtkWidget *
glade_editor_load_editable_in_page (GladeEditor        *editor,
                                    GladeWidgetAdaptor *adaptor,
                                    GladeEditorPageType type)
{
  GladeEditorPrivate *priv = GLADE_EDITOR_PRIVATE (editor);
  GtkContainer *container = NULL;
  GtkWidget *scrolled_window, *editable;
  GtkAdjustment *adj;

  /* Remove the old table that was in this container */
  switch (type)
    {
      case GLADE_PAGE_GENERAL:
        container = GTK_CONTAINER (priv->page_widget);
        break;
      case GLADE_PAGE_COMMON:
        container = GTK_CONTAINER (priv->page_common);
        break;
      case GLADE_PAGE_PACKING:
        container = GTK_CONTAINER (priv->page_packing);
        break;
      case GLADE_PAGE_ATK:
        container = GTK_CONTAINER (priv->page_atk);
        break;
      case GLADE_PAGE_QUERY:
      default:
        g_critical ("Unreachable code reached !");
        break;
    }

  /* Hide the editable (this will destroy on packing pages) */
  hide_or_remove_visible_child (container, type == GLADE_PAGE_PACKING);

  if (!adaptor)
    return NULL;

  if ((editable =
       glade_editor_get_editable_by_adaptor (editor, adaptor, type)) == NULL)
    return NULL;

  /* Attach the new page */
  if (!gtk_widget_get_parent (editable))
    gtk_container_add (GTK_CONTAINER (container), editable);
  gtk_widget_show (editable);

  if ((scrolled_window = 
       gtk_widget_get_ancestor (GTK_WIDGET (container), 
				GTK_TYPE_SCROLLED_WINDOW)) != NULL)
    {
      adj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled_window));
      gtk_container_set_focus_vadjustment (GTK_CONTAINER (editable), adj);

      adj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scrolled_window));
      gtk_container_set_focus_hadjustment (GTK_CONTAINER (editable), adj);
    }

  return editable;
}

static void
glade_editor_load_widget_class (GladeEditor *editor,
                                GladeWidgetAdaptor *adaptor)
{

  glade_editor_load_editable_in_page (editor, adaptor, GLADE_PAGE_GENERAL);
  glade_editor_load_editable_in_page (editor, adaptor, GLADE_PAGE_COMMON);
  glade_editor_load_editable_in_page (editor, adaptor, GLADE_PAGE_ATK);
  glade_editor_load_editable_in_page (editor, NULL, GLADE_PAGE_PACKING);

  editor->priv->loaded_adaptor = adaptor;
}

static void
glade_editor_close_cb (GladeProject *project, GladeEditor *editor)
{
  /* project we are viewing was closed,
   * detatch from editor.
   */
  glade_editor_load_widget (editor, NULL);
}

static void
glade_editor_removed_cb (GladeProject *project,
                         GladeWidget *widget,
                         GladeEditor *editor)
{
  /* Widget we were viewing was removed from project,
   * detatch from editor.
   */
  if (widget == editor->priv->loaded_widget)
    glade_editor_load_widget (editor, NULL);

}


static void
glade_editor_load_editable (GladeEditor        *editor,
                            GladeWidget        *widget,
                            GladeEditorPageType type)
{
  GtkWidget   *editable;
  GladeWidget *parent = glade_widget_get_parent (widget);

  /* Use the parenting adaptor for packing pages... so deffer creating the widgets
   * until load time.
   */
  if (type == GLADE_PAGE_PACKING)
    {
      GladeWidgetAdaptor *adaptor;

      if (!parent)
	return;

      adaptor = glade_widget_get_adaptor (parent);
      editable =
	glade_editor_load_editable_in_page (editor, adaptor,
					    GLADE_PAGE_PACKING);
    }
  else
    editable = 
      glade_editor_get_editable_by_adaptor (editor, 
					    glade_widget_get_adaptor (widget), 
					    type);

  g_assert (editable);

  if (!widget)
    gtk_widget_hide (editable);

  glade_editable_load (GLADE_EDITABLE (editable), widget);

  if (widget)
    gtk_widget_show (editable);
}

static void
clear_editables (GladeEditor *editor)
{
  GladeEditable *editable;
  GList *l;

  for (l = editor->priv->editables; l; l = l->next)
    {
      editable = l->data;
      glade_editable_load (editable, NULL);
    }
}

static void
glade_editor_load_widget_real (GladeEditor *editor, GladeWidget *widget)
{
  GladeEditorPrivate *priv = GLADE_EDITOR_PRIVATE (editor);
  GladeWidgetAdaptor *adaptor;
  GladeProject *project;

  /* Disconnect from last widget */
  if (priv->loaded_widget != NULL)
    {
      /* better pay a small price now and avoid unseen editables
       * waking up on project metadata changes.
       */
      clear_editables (editor);

      project = glade_widget_get_project (priv->loaded_widget);
      g_signal_handler_disconnect (G_OBJECT (project),
                                   priv->project_closed_signal_id);
      g_signal_handler_disconnect (G_OBJECT (project),
                                   priv->project_removed_signal_id);
      g_signal_handler_disconnect (G_OBJECT (priv->loaded_widget),
                                   priv->widget_warning_id);
      g_signal_handler_disconnect (G_OBJECT (priv->loaded_widget),
                                   priv->widget_name_id);
    }

  /* Load the GladeWidgetClass */
  adaptor = widget ? glade_widget_get_adaptor (widget) : NULL;
  if (priv->loaded_adaptor != adaptor || adaptor == NULL)
    glade_editor_load_widget_class (editor, adaptor);

  glade_signal_editor_load_widget (priv->signal_editor, widget);

  /* we are just clearing, we are done */
  if (widget == NULL)
    {
      priv->loaded_widget = NULL;

      /* Clear class header */
      glade_editor_update_class_field (editor);

      g_object_notify_by_pspec (G_OBJECT (editor), properties[PROP_WIDGET]);

      return;
    }

  priv->loading = TRUE;

  /* Load each GladeEditorProperty from 'widget' */
  glade_editor_load_editable (editor, widget, GLADE_PAGE_GENERAL);
  glade_editor_load_editable (editor, widget, GLADE_PAGE_COMMON);
  glade_editor_load_editable (editor, widget, GLADE_PAGE_ATK);
  glade_editor_load_editable (editor, widget, GLADE_PAGE_PACKING);

  priv->loaded_widget = widget;
  priv->loading = FALSE;

  /* Update class header */
  glade_editor_update_class_field (editor);

  /* Connect to new widget */
  project = glade_widget_get_project (priv->loaded_widget);
  priv->project_closed_signal_id =
      g_signal_connect (G_OBJECT (project), "close",
                        G_CALLBACK (glade_editor_close_cb), editor);
  priv->project_removed_signal_id =
      g_signal_connect (G_OBJECT (project), "remove-widget",
                        G_CALLBACK (glade_editor_removed_cb), editor);
  priv->widget_warning_id =
      g_signal_connect (G_OBJECT (widget), "notify::support-warning",
                        G_CALLBACK (glade_editor_update_class_warning_cb),
                        editor);
  priv->widget_name_id =
      g_signal_connect (G_OBJECT (widget), "notify::name",
                        G_CALLBACK (glade_editor_update_widget_name_cb),
                        editor);

  g_object_notify_by_pspec (G_OBJECT (editor), properties[PROP_WIDGET]);
}


/**
 * glade_editor_new:
 *
 * Returns: a new #GladeEditor
 */
GladeEditor *
glade_editor_new (void)
{
  GladeEditor *editor;

  editor = g_object_new (GLADE_TYPE_EDITOR, "spacing", 6, NULL);

  return editor;
}

/**
 * glade_editor_load_widget:
 * @editor: a #GladeEditor
 * @widget: a #GladeWidget
 *
 * Load @widget into @editor. If @widget is %NULL, clear the editor.
 */
void
glade_editor_load_widget (GladeEditor *editor, GladeWidget *widget)
{
  g_return_if_fail (GLADE_IS_EDITOR (editor));
  g_return_if_fail (widget == NULL || GLADE_IS_WIDGET (widget));

  if (editor->priv->loaded_widget == widget)
    return;

  glade_editor_load_widget_real (editor, widget);
}

static void
query_dialog_style_set_cb (GtkWidget *dialog,
                           GtkStyle *previous_style,
                           gpointer user_data)
{
  GtkWidget *content_area, *action_area;

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_container_set_border_width (GTK_CONTAINER (content_area), 12);
  gtk_box_set_spacing (GTK_BOX (content_area), 12);
  action_area = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
  gtk_container_set_border_width (GTK_CONTAINER (action_area), 0);
  gtk_box_set_spacing (GTK_BOX (action_area), 6);
}

static gboolean
query_dialog_delete_event_cb (GtkDialog *dialog,
			      GdkEvent  *event,
			      gpointer   user_data)
{
  gtk_dialog_response (dialog, GTK_RESPONSE_CANCEL);
  return TRUE;
}

gboolean
glade_editor_query_dialog (GladeWidget *widget)
{
  GladeWidgetAdaptor *adaptor;
  GtkWidget *dialog, *editable, *content_area;
  GtkWidget *create;
  gchar *title;
  gint answer;
  gboolean retval = TRUE;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
  
  adaptor = glade_widget_get_adaptor (widget);

  title = g_strdup_printf (_("Create a %s"), glade_widget_adaptor_get_name (adaptor));
  dialog = gtk_dialog_new_with_buttons (title, NULL,
                                        GTK_DIALOG_MODAL |
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        NULL);
  g_free (title);

  create = gtk_button_new_with_mnemonic (_("Crea_te"));
  gtk_widget_show (create);
  gtk_widget_set_can_default (create, TRUE);
  gtk_dialog_add_action_widget (GTK_DIALOG (dialog), create, GTK_RESPONSE_OK);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL, -1);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  editable = (GtkWidget *) glade_widget_adaptor_create_editable (adaptor, GLADE_PAGE_QUERY);
  gtk_widget_show (editable);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_box_pack_start (GTK_BOX (content_area), editable, FALSE, FALSE, 6);

  glade_editable_load (GLADE_EDITABLE (editable), widget);

  g_signal_connect (dialog, "style-set",
                    G_CALLBACK (query_dialog_style_set_cb), NULL);

  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (query_dialog_delete_event_cb), NULL);

  answer = gtk_dialog_run (GTK_DIALOG (dialog));

  /*
   * If user cancel's we cancel the whole "create operation" by
   * return FALSE. glade_widget_new() will see the FALSE, and
   * take care of canceling the "create" operation.
   */
  if (answer == GTK_RESPONSE_CANCEL)
    retval = FALSE;

  gtk_widget_destroy (dialog);
  return retval;
}

enum
{
  COLUMN_ENABLED = 0,
  COLUMN_PROP_NAME,
  COLUMN_PROPERTY,
  COLUMN_WEIGHT,
  COLUMN_CHILD,
  COLUMN_DEFAULT,
  COLUMN_NDEFAULT,
  COLUMN_DEFSTRING,
  NUM_COLUMNS
};


static void
glade_editor_reset_toggled (GtkCellRendererToggle *cell,
                            gchar                 *path_str,
                            GtkTreeModel          *model)
{
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  GtkTreeIter iter;
  gboolean enabled;

  /* get toggled iter */
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter, COLUMN_ENABLED, &enabled, -1);
  gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
                      COLUMN_ENABLED, !enabled, -1);
  gtk_tree_path_free (path);
}

static GtkWidget *
glade_editor_reset_view (void)
{
  GtkWidget *view_widget;
  GtkTreeModel *model;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  model = (GtkTreeModel *) gtk_tree_store_new (NUM_COLUMNS, G_TYPE_BOOLEAN,     /* Enabled  value      */
                                               G_TYPE_STRING,   /* Property name       */
                                               GLADE_TYPE_PROPERTY,     /* The property        */
                                               G_TYPE_INT,      /* Parent node ?       */
                                               G_TYPE_BOOLEAN,  /* Child node ?        */
                                               G_TYPE_BOOLEAN,  /* Has default value   */
                                               G_TYPE_BOOLEAN,  /* Doesn't have defaut */
                                               G_TYPE_STRING);  /* Default string      */

  view_widget = gtk_tree_view_new_with_model (model);
  g_object_set (G_OBJECT (view_widget), "enable-search", FALSE, NULL);

  /********************* fake invisible column *********************/
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), "editable", FALSE, "visible", FALSE, NULL);

  column = gtk_tree_view_column_new_with_attributes (NULL, renderer, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

  gtk_tree_view_column_set_visible (column, FALSE);
  gtk_tree_view_set_expander_column (GTK_TREE_VIEW (view_widget), column);

  /************************ enabled column ************************/
  renderer = gtk_cell_renderer_toggle_new ();
  g_object_set (G_OBJECT (renderer),
                "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
                "activatable", TRUE, NULL);
  g_signal_connect (renderer, "toggled",
                    G_CALLBACK (glade_editor_reset_toggled), model);
  gtk_tree_view_insert_column_with_attributes
      (GTK_TREE_VIEW (view_widget), COLUMN_ENABLED,
       _("Reset"), renderer,
       "sensitive", COLUMN_NDEFAULT,
       "activatable", COLUMN_NDEFAULT,
       "active", COLUMN_ENABLED, "visible", COLUMN_CHILD, NULL);

  /********************* property name column *********************/
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);

  gtk_tree_view_insert_column_with_attributes
      (GTK_TREE_VIEW (view_widget), COLUMN_PROP_NAME,
       _("Property"), renderer,
       "text", COLUMN_PROP_NAME, "weight", COLUMN_WEIGHT, NULL);

  /******************* default indicator column *******************/
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer),
                "editable", FALSE,
                "style", PANGO_STYLE_ITALIC, "foreground", "Gray", NULL);

  gtk_tree_view_insert_column_with_attributes
      (GTK_TREE_VIEW (view_widget), COLUMN_DEFSTRING,
       NULL, renderer,
       "text", COLUMN_DEFSTRING, "visible", COLUMN_DEFAULT, NULL);

  return view_widget;
}

static void
glade_editor_populate_reset_view (GladeWidget *widget, GtkTreeView *tree_view)
{
  GtkTreeStore *model = GTK_TREE_STORE (gtk_tree_view_get_model (tree_view));
  GtkTreeIter property_iter, general_iter, common_iter, atk_iter, *iter;
  GList *list;
  GladeProperty *property;
  GladePropertyClass *pclass;
  gboolean def;

  g_return_if_fail (widget != NULL);

  gtk_tree_store_append (model, &general_iter, NULL);
  gtk_tree_store_set (model, &general_iter,
                      COLUMN_PROP_NAME, _("General"),
                      COLUMN_PROPERTY, NULL,
                      COLUMN_WEIGHT, PANGO_WEIGHT_BOLD,
                      COLUMN_CHILD, FALSE,
                      COLUMN_DEFAULT, FALSE, COLUMN_NDEFAULT, FALSE, -1);

  gtk_tree_store_append (model, &common_iter, NULL);
  gtk_tree_store_set (model, &common_iter,
                      COLUMN_PROP_NAME, _("Common"),
                      COLUMN_PROPERTY, NULL,
                      COLUMN_WEIGHT, PANGO_WEIGHT_BOLD,
                      COLUMN_CHILD, FALSE,
                      COLUMN_DEFAULT, FALSE, COLUMN_NDEFAULT, FALSE, -1);

  gtk_tree_store_append (model, &atk_iter, NULL);
  gtk_tree_store_set (model, &atk_iter,
                      COLUMN_PROP_NAME, _("Accessibility"),
                      COLUMN_PROPERTY, NULL,
                      COLUMN_WEIGHT, PANGO_WEIGHT_BOLD,
                      COLUMN_CHILD, FALSE,
                      COLUMN_DEFAULT, FALSE, COLUMN_NDEFAULT, FALSE, -1);

  /* General & Common */
  for (list = glade_widget_get_properties (widget); list; list = list->next)
    {
      property = list->data;
      pclass   = glade_property_get_class (property);

      if (glade_property_class_is_visible (pclass) == FALSE)
        continue;

      if (glade_property_class_atk (pclass))
        iter = &atk_iter;
      else if (glade_property_class_common (pclass))
        iter = &common_iter;
      else
        iter = &general_iter;

      def = glade_property_default (property);

      gtk_tree_store_append (model, &property_iter, iter);
      gtk_tree_store_set (model, &property_iter,
                          COLUMN_ENABLED, !def,
                          COLUMN_PROP_NAME, glade_property_class_get_name (pclass),
                          COLUMN_PROPERTY, property,
                          COLUMN_WEIGHT, PANGO_WEIGHT_NORMAL,
                          COLUMN_CHILD, TRUE,
                          COLUMN_DEFAULT, def,
                          COLUMN_NDEFAULT, !def,
                          COLUMN_DEFSTRING, _("(default)"), -1);
    }
}

static gboolean
glade_editor_reset_selection_changed_cb (GtkTreeSelection *selection,
                                         GtkTextView      *desc_view)
{
  GtkTreeIter iter;
  GladeProperty *property = NULL;
  GtkTreeModel *model = NULL;
  GtkTextBuffer *text_buffer;
  GladePropertyClass *pclass = NULL;

  const gchar *message =
      _("Select the properties that you want to reset to their default values");

  /* Find selected data and show property blurb here */
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (desc_view));
      gtk_tree_model_get (model, &iter, COLUMN_PROPERTY, &property, -1);

      if (property)
	pclass = glade_property_get_class (property);

      gtk_text_buffer_set_text (text_buffer,
                                pclass ? glade_property_class_get_tooltip (pclass) : message,
                                -1);
      if (property)
        g_object_unref (G_OBJECT (property));
    }
  return TRUE;
}

static gboolean
glade_editor_reset_foreach_selection (GtkTreeModel *model,
                                      GtkTreePath  *path,
                                      GtkTreeIter  *iter,
                                      gboolean      select)
{
  gboolean def;

  gtk_tree_model_get (model, iter, COLUMN_DEFAULT, &def, -1);
  /* Dont modify rows that are already default */
  if (def == FALSE)
    gtk_tree_store_set (GTK_TREE_STORE (model), iter,
                        COLUMN_ENABLED, select, -1);
  return FALSE;
}


static void
glade_editor_reset_select_all_clicked (GtkButton   *button,
                                       GtkTreeView *tree_view)
{
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
  gtk_tree_model_foreach (model, (GtkTreeModelForeachFunc)
                          glade_editor_reset_foreach_selection,
                          GINT_TO_POINTER (TRUE));
}

static void
glade_editor_reset_unselect_all_clicked (GtkButton   *button,
                                         GtkTreeView *tree_view)
{
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
  gtk_tree_model_foreach (model, (GtkTreeModelForeachFunc)
                          glade_editor_reset_foreach_selection,
                          GINT_TO_POINTER (FALSE));
}

static gboolean
glade_editor_reset_accumulate_selected_props (GtkTreeModel *model,
                                              GtkTreePath  *path,
                                              GtkTreeIter  *iter,
                                              GList       **accum)
{
  GladeProperty *property;
  gboolean enabled, def;

  gtk_tree_model_get (model, iter,
                      COLUMN_PROPERTY, &property,
                      COLUMN_ENABLED, &enabled, COLUMN_DEFAULT, &def, -1);

  if (property && enabled && !def)
    *accum = g_list_prepend (*accum, property);


  if (property)
    g_object_unref (G_OBJECT (property));

  return FALSE;
}

static GList *
glade_editor_reset_get_selected_props (GtkTreeModel *model)
{
  GList *ret = NULL;

  gtk_tree_model_foreach (model, (GtkTreeModelForeachFunc)
                          glade_editor_reset_accumulate_selected_props, &ret);

  return ret;
}

static void
glade_editor_reset_properties (GList *props)
{
  GList *list, *sdata_list = NULL;
  GCSetPropData *sdata;
  GladeProperty *prop;
  GladeWidget   *widget;
  GladeProject *project = NULL;

  for (list = props; list; list = list->next)
    {
      prop    = list->data;
      widget  = glade_property_get_widget (prop);
      project = glade_widget_get_project (widget);

      sdata = g_new (GCSetPropData, 1);
      sdata->property = prop;

      sdata->old_value = g_new0 (GValue, 1);
      sdata->new_value = g_new0 (GValue, 1);

      glade_property_get_value (prop, sdata->old_value);
      glade_property_get_default (prop, sdata->new_value);

      sdata_list = g_list_prepend (sdata_list, sdata);
    }

  if (project)
    /* GladeCommand takes ownership of allocated list, ugly but practicle */
    glade_command_set_properties_list (project, sdata_list);

}

void
glade_editor_reset_dialog_run (GtkWidget *parent, GladeWidget *gwidget)
{
  GtkTreeSelection *selection;
  GtkWidget *dialog;
  GtkWidget *vbox, *hbox, *label, *sw, *button;
  GtkWidget *tree_view, *description_view;
  gint res;
  GList *list;

  dialog = gtk_dialog_new_with_buttons (_("Reset Widget Properties"),
                                        parent ? GTK_WINDOW (parent) : NULL,
                                        GTK_DIALOG_MODAL |
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_OK"), GTK_RESPONSE_OK, NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_show (vbox);

  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);

  gtk_box_pack_start (GTK_BOX
                      (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), vbox,
                      TRUE, TRUE, 0);

  /* Checklist */
  label = gtk_label_new_with_mnemonic (_("_Properties:"));
  gtk_widget_show (label);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (sw);
  gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
  gtk_widget_set_size_request (sw, 400, 200);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);


  tree_view = glade_editor_reset_view ();
  if (gwidget)
    glade_editor_populate_reset_view (gwidget, GTK_TREE_VIEW (tree_view));
  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  gtk_widget_show (tree_view);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), tree_view);
  gtk_container_add (GTK_CONTAINER (sw), tree_view);

  /* Select all / Unselect all */
  hbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

  button = gtk_button_new_with_mnemonic (_("_Select All"));
  gtk_widget_show (button);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (button), 6);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (glade_editor_reset_select_all_clicked),
                    tree_view);

  button = gtk_button_new_with_mnemonic (_("_Unselect All"));
  gtk_widget_show (button);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (button), 6);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (glade_editor_reset_unselect_all_clicked),
                    tree_view);


  /* Description */
  label = gtk_label_new_with_mnemonic (_("Property _Description:"));
  gtk_widget_show (label);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (sw);
  gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
  gtk_widget_set_size_request (sw, 400, 80);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);

  description_view = gtk_text_view_new ();
  gtk_text_view_set_editable (GTK_TEXT_VIEW (description_view), FALSE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (description_view), GTK_WRAP_WORD);

  gtk_widget_show (description_view);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), description_view);
  gtk_container_add (GTK_CONTAINER (sw), description_view);

  /* Update description */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
  g_signal_connect (G_OBJECT (selection), "changed",
                    G_CALLBACK (glade_editor_reset_selection_changed_cb),
                    description_view);



  /* Run the dialog */
  res = gtk_dialog_run (GTK_DIALOG (dialog));
  if (res == GTK_RESPONSE_OK)
    {

      /* get all selected properties and reset properties through glade_command */
      if ((list = glade_editor_reset_get_selected_props
           (gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view)))) != NULL)
        {
          glade_editor_reset_properties (list);
          g_list_free (list);
        }
    }
  gtk_widget_destroy (dialog);
}

void
glade_editor_show_info (GladeEditor *editor)
{
  g_warning ("%s function is deprecated and does nothing", __func__);
}

void
glade_editor_hide_info (GladeEditor *editor)
{
  g_warning ("%s function is deprecated and does nothing", __func__);
}

void
glade_editor_show_class_field (GladeEditor *editor)
{
  GladeEditorPrivate *priv;

  g_return_if_fail (GLADE_IS_EDITOR (editor));

  priv = GLADE_EDITOR_PRIVATE (editor);

  if (priv->show_class_field != TRUE)
    {
      priv->show_class_field = TRUE;
      gtk_widget_show (priv->class_field);

      g_object_notify_by_pspec (G_OBJECT (editor), properties[PROP_SHOW_CLASS_FIELD]);
    }
}

void
glade_editor_hide_class_field (GladeEditor *editor)
{
  GladeEditorPrivate *priv;

  g_return_if_fail (GLADE_IS_EDITOR (editor));

  priv = GLADE_EDITOR_PRIVATE (editor);

  if (priv->show_class_field != FALSE)
    {
      priv->show_class_field = FALSE;
      gtk_widget_hide (priv->class_field);

      g_object_notify_by_pspec (G_OBJECT (editor), properties[PROP_SHOW_CLASS_FIELD]);
    }
}

static void
editor_widget_name_changed (GladeWidget *widget,
			    GParamSpec  *pspec,
			    GtkWindow   *window)
{
  gchar *title, *prj_name;

  prj_name = glade_project_get_name (glade_widget_get_project (widget));
  /* Translators: first %s is the project name, second is a widget name */
  title = g_strdup_printf (_("%s - %s Properties"), prj_name,
                           glade_widget_get_display_name (widget));
  gtk_window_set_title (window, title);
  g_free (title);
  g_free (prj_name);
}

/**
 * glade_editor_dialog_for_widget:
 * @widget: a #GladeWidget
 *
 * This convenience function creates a new dialog window to edit @widget
 * specifically.
 *
 * Returns: the newly created dialog window
 */
GtkWidget *
glade_editor_dialog_for_widget (GladeWidget *widget)
{
  GtkWidget *window, *editor;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

  /* Window */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_UTILITY);

  /* Keep the title up to date */
  editor_widget_name_changed (widget, NULL, GTK_WINDOW (window));
  g_signal_connect_object (G_OBJECT (widget), "notify::name",
			   G_CALLBACK (editor_widget_name_changed), window, 0);

  if (glade_app_get_accel_group ())
    {
      gtk_window_add_accel_group (GTK_WINDOW (window),
                                  glade_app_get_accel_group ());
      g_signal_connect (G_OBJECT (window), "key-press-event",
                        G_CALLBACK (glade_utils_hijack_key_press), NULL);
    }

  editor = (GtkWidget *)glade_editor_new ();
  glade_editor_load_widget (GLADE_EDITOR (editor), widget);

  g_signal_connect_swapped (G_OBJECT (editor), "notify::widget",
                            G_CALLBACK (gtk_widget_destroy), window);

  gtk_container_set_border_width (GTK_CONTAINER (editor), 6);
  gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (editor));

  gtk_window_set_default_size (GTK_WINDOW (window), 400, 480);

  gtk_widget_show (editor);

  return window;
}
