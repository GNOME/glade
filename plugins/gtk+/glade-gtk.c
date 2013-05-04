/*
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2004 - 2008 Tristan Van Berkom, Juan Pablo Ugarte et al.
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
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#include <config.h>

#include "glade-gtk.h"
#include "glade-gtk-frame.h"
#include "glade-about-dialog-editor.h"
#include "glade-accels.h"
#include "glade-activatable-editor.h"
#include "glade-attributes.h"
#include "glade-button-editor.h"
#include "glade-cell-renderer-editor.h"
#include "glade-column-types.h"
#include "glade-entry-editor.h"
#include "glade-fixed.h"
#include "glade-gtk-action-widgets.h"
#include "glade-icon-factory-editor.h"
#include "glade-icon-sources.h"
#include "glade-image-editor.h"
#include "glade-image-item-editor.h"
#include "glade-label-editor.h"
#include "glade-model-data.h"
#include "glade-store-editor.h"
#include "glade-string-list.h"
#include "glade-tool-button-editor.h"
#include "glade-tool-item-group-editor.h"
#include "glade-treeview-editor.h"
#include "glade-window-editor.h"
#include "glade-gtk-dialog.h"
#include "glade-gtk-image.h"
#include "glade-gtk-button.h"
#include "glade-gtk-menu-shell.h"

#include <gladeui/glade-editor-property.h>
#include <gladeui/glade-base-editor.h>
#include <gladeui/glade-xml-utils.h>


#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>


/* This function does absolutely nothing
 * (and is for use in overriding post_create functions).
 */
void
empty (GObject * container, GladeCreateReason reason)
{
}


/* Initialize needed pspec types from here */
void
glade_gtk_init (const gchar * name)
{
}



/* ----------------------------- GtkTextBuffer ------------------------------ */
static void
glade_gtk_entry_buffer_changed (GtkTextBuffer * buffer,
                                GParamSpec * pspec, GladeWidget * gbuffy)
{
  const gchar *text_prop = NULL;
  GladeProperty *prop;
  gchar *text = NULL;

  if (glade_widget_superuser ())
    return;

  g_object_get (buffer, "text", &text, NULL);

  if ((prop = glade_widget_get_property (gbuffy, "text")))
    {
      glade_property_get (prop, &text_prop);

      if (text_prop == NULL || g_strcmp0 (text, text_prop))
        glade_command_set_property (prop, text);
    }
  g_free (text);
}

void
glade_gtk_entry_buffer_post_create (GladeWidgetAdaptor * adaptor,
                                    GObject * object, GladeCreateReason reason)
{
  GladeWidget *gbuffy;

  gbuffy = glade_widget_get_from_gobject (object);

  g_signal_connect (object, "notify::text",
                    G_CALLBACK (glade_gtk_entry_buffer_changed), gbuffy);
}


void
glade_gtk_entry_buffer_set_property (GladeWidgetAdaptor * adaptor,
                                     GObject * object,
                                     const gchar * id, const GValue * value)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);
  GladeProperty *property = glade_widget_get_property (gwidget, id);

  if (!strcmp (id, "text"))
    {
      g_signal_handlers_block_by_func (object, glade_gtk_entry_buffer_changed,
                                       gwidget);

      if (g_value_get_string (value))
        gtk_entry_buffer_set_text (GTK_ENTRY_BUFFER (object),
                                   g_value_get_string (value), -1);
      else
        gtk_entry_buffer_set_text (GTK_ENTRY_BUFFER (object), "", -1);

      g_signal_handlers_unblock_by_func (object, glade_gtk_entry_buffer_changed,
                                         gwidget);

    }
  else if (GPC_VERSION_CHECK
           (glade_property_get_class (property), gtk_major_version, gtk_minor_version + 1))
    GWA_GET_CLASS (G_TYPE_OBJECT)->set_property (adaptor, object, id, value);
}



/* ----------------------------- GtkTextBuffer ------------------------------ */
static void
glade_gtk_text_buffer_changed (GtkTextBuffer * buffer, GladeWidget * gbuffy)
{
  const gchar *text_prop = NULL;
  GladeProperty *prop;
  gchar *text = NULL;

  g_object_get (buffer, "text", &text, NULL);

  if ((prop = glade_widget_get_property (gbuffy, "text")))
    {
      glade_property_get (prop, &text_prop);

      if (g_strcmp0 (text, text_prop))
        glade_command_set_property (prop, text);
    }
  g_free (text);
}

void
glade_gtk_text_buffer_post_create (GladeWidgetAdaptor * adaptor,
                                   GObject * object, GladeCreateReason reason)
{
  GladeWidget *gbuffy;

  gbuffy = glade_widget_get_from_gobject (object);

  g_signal_connect (object, "changed",
                    G_CALLBACK (glade_gtk_text_buffer_changed), gbuffy);
}


void
glade_gtk_text_buffer_set_property (GladeWidgetAdaptor * adaptor,
                                    GObject * object,
                                    const gchar * id, const GValue * value)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);
  GladeProperty *property = glade_widget_get_property (gwidget, id);

  if (!strcmp (id, "text"))
    {
      g_signal_handlers_block_by_func (object, glade_gtk_text_buffer_changed,
                                       gwidget);

      if (g_value_get_string (value))
        gtk_text_buffer_set_text (GTK_TEXT_BUFFER (object),
                                  g_value_get_string (value), -1);
      else
        gtk_text_buffer_set_text (GTK_TEXT_BUFFER (object), "", -1);

      g_signal_handlers_unblock_by_func (object, glade_gtk_text_buffer_changed,
                                         gwidget);

    }
  else if (GPC_VERSION_CHECK
           (glade_property_get_class (property), gtk_major_version, gtk_minor_version + 1))
    GWA_GET_CLASS (G_TYPE_OBJECT)->set_property (adaptor, object, id, value);
}

/* ----------------------------- GtkTextView ------------------------------ */
static gboolean
glade_gtk_text_view_stop_double_click (GtkWidget * widget,
                                       GdkEventButton * event,
                                       gpointer user_data)
{
  /* Return True if the event is double or triple click */
  return (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS);
}

void
glade_gtk_text_view_post_create (GladeWidgetAdaptor * adaptor,
                                 GObject * object, GladeCreateReason reason)
{
  /* This makes gtk_text_view_set_buffer() stop complaing */
  gtk_drag_dest_set (GTK_WIDGET (object), 0, NULL, 0, 0);

  /* Glade hangs when a TextView gets a double click. So we stop them */
  g_signal_connect (object, "button-press-event",
                    G_CALLBACK (glade_gtk_text_view_stop_double_click), NULL);
}

void
glade_gtk_text_view_set_property (GladeWidgetAdaptor * adaptor,
                                  GObject * object,
                                  const gchar * property_name,
                                  const GValue * value)
{
  if (strcmp (property_name, "buffer") == 0)
    {
      if (!g_value_get_object (value))
        return;
    }

  GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor,
                                                    object,
                                                    property_name, value);
}

/* ----------------------------- GtkComboBox ------------------------------ */
void
glade_gtk_combo_box_set_property (GladeWidgetAdaptor * adaptor,
                                  GObject * object,
                                  const gchar * id, const GValue * value)
{
  if (!strcmp (id, "entry-text-column"))
    {
      /* Avoid warnings */
      if (g_value_get_int (value) >= 0)
        GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor,
                                                          object, id, value);
    }
  else if (!strcmp (id, "text-column"))
    {
      if (g_value_get_int (value) >= 0)
        gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (object),
                                             g_value_get_int (value));
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor,
                                                      object, id, value);
}

GList *glade_gtk_cell_layout_get_children (GladeWidgetAdaptor * adaptor,
                                           GObject * container);

GList *
glade_gtk_combo_box_get_children (GladeWidgetAdaptor * adaptor,
                                  GtkComboBox * combo)
{
  GList *list = NULL;

  list = glade_gtk_cell_layout_get_children (adaptor, G_OBJECT (combo));

  /* return the internal entry.
   *
   * FIXME: for recent gtk+ we have no comboboxentry
   * but a "has-entry" property instead
   */
  if (gtk_combo_box_get_has_entry (combo))
    list = g_list_append (list, gtk_bin_get_child (GTK_BIN (combo)));

  return list;
}

/* ----------------------------- GtkComboBoxText ------------------------------ */
#define GLADE_TAG_ITEMS  "items"
#define GLADE_TAG_ITEM   "item"

void
glade_gtk_combo_box_text_post_create (GladeWidgetAdaptor *adaptor,
				      GObject            *object, 
				      GladeCreateReason   reason)
{
  GladeWidget *gwidget;

  /* Chain Up */
  GWA_GET_CLASS (GTK_TYPE_COMBO_BOX)->post_create (adaptor, object, reason);

  /* No editor, no model, no cells on a GtkComboBoxText, just the items. */
  gwidget = glade_widget_get_from_gobject (object);
  glade_widget_set_action_visible (gwidget, "launch_editor", FALSE);
}

GladeEditorProperty *
glade_gtk_combo_box_text_create_eprop (GladeWidgetAdaptor * adaptor,
				       GladePropertyClass * klass, 
				       gboolean use_command)
{
  GladeEditorProperty *eprop;
  GParamSpec          *pspec;

  pspec = glade_property_class_get_pspec (klass);

  if (pspec->value_type == GLADE_TYPE_STRING_LIST)
    {
      eprop = glade_eprop_string_list_new (klass, use_command, TRUE);
    }
  else
    eprop = GWA_GET_CLASS
        (GTK_TYPE_WIDGET)->create_eprop (adaptor, klass, use_command);

  return eprop;
}

gchar *
glade_gtk_combo_box_text_string_from_value (GladeWidgetAdaptor * adaptor,
					    GladePropertyClass * klass,
					    const GValue * value)
{
  GParamSpec          *pspec;

  pspec = glade_property_class_get_pspec (klass);

  if (pspec->value_type == GLADE_TYPE_STRING_LIST)
    {
      GList *list = g_value_get_boxed (value);

      return glade_string_list_to_string (list);
    }
  else
    return GWA_GET_CLASS
        (GTK_TYPE_COMBO_BOX)->string_from_value (adaptor, klass, value);
}

void
glade_gtk_combo_box_text_set_property (GladeWidgetAdaptor * adaptor,
				       GObject * object,
				       const gchar * id, const GValue * value)
{
  if (!strcmp (id, "glade-items"))
    {
      GList *string_list, *l;
      GladeString *string;
      gint active;

      string_list = g_value_get_boxed (value);

      active = gtk_combo_box_get_active (GTK_COMBO_BOX (object));

      /* Update comboboxtext items */
      gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (object));

      for (l = string_list; l; l = l->next)
	{
	  string = l->data;

	  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (object), string->string);
	}

      gtk_combo_box_set_active (GTK_COMBO_BOX (object),
				CLAMP (active, 0, g_list_length (string_list) - 1));
    }
  else
    GWA_GET_CLASS (GTK_TYPE_COMBO_BOX)->set_property (adaptor, object, id, value);
}

static void
glade_gtk_combo_box_text_read_items (GladeWidget * widget, GladeXmlNode * node)
{
  GladeXmlNode *items_node;
  GladeXmlNode *item_node;
  GList        *string_list = NULL;

  if ((items_node =
       glade_xml_search_child (node, GLADE_TAG_ITEMS)) != NULL)
    {

      for (item_node = glade_xml_node_get_children (items_node);
	   item_node; item_node = glade_xml_node_next (item_node))
	{
	  gchar *str, *comment, *context;
	  gboolean translatable;

	  if (!glade_xml_node_verify (item_node, GLADE_TAG_ITEM))
	    continue;

          if ((str = glade_xml_get_content (item_node)) == NULL)
	    continue;

	  context      = glade_xml_get_property_string (item_node, GLADE_TAG_CONTEXT);
	  comment      = glade_xml_get_property_string (item_node, GLADE_TAG_COMMENT);
          translatable = glade_xml_get_property_boolean (item_node, GLADE_TAG_TRANSLATABLE, FALSE);

	  string_list = 
	    glade_string_list_append (string_list,
				      str, comment, context, translatable);

	  g_free (str);
	  g_free (context);
	  g_free (comment);
	}

      glade_widget_property_set (widget, "glade-items", string_list);
      glade_string_list_free (string_list);
    }
}

void
glade_gtk_combo_box_text_read_widget (GladeWidgetAdaptor * adaptor,
				      GladeWidget * widget, GladeXmlNode * node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
	glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (GTK_TYPE_COMBO_BOX)->read_widget (adaptor, widget, node);

  glade_gtk_combo_box_text_read_items (widget, node);
}

static void
glade_gtk_combo_box_text_write_items (GladeWidget * widget,
				      GladeXmlContext * context,
				      GladeXmlNode * node)
{
  GladeXmlNode *item_node;
  GList *string_list = NULL, *l;
  GladeString *string;

  if (!glade_widget_property_get (widget, "glade-items", &string_list) || !string_list)
    return;

  for (l = string_list; l; l = l->next)
    {
      string = l->data;

      item_node = glade_xml_node_new (context, GLADE_TAG_ITEM);
      glade_xml_node_append_child (node, item_node);

      glade_xml_set_content (item_node, string->string);

      if (string->translatable)
        glade_xml_node_set_property_string (item_node,
                                            GLADE_TAG_TRANSLATABLE,
                                            GLADE_XML_TAG_I18N_TRUE);

      if (string->comment)
        glade_xml_node_set_property_string (item_node,
                                            GLADE_TAG_COMMENT,
                                            string->comment);

      if (string->context)
        glade_xml_node_set_property_string (item_node,
                                            GLADE_TAG_CONTEXT,
                                            string->context);
    }
}

void
glade_gtk_combo_box_text_write_widget (GladeWidgetAdaptor * adaptor,
				       GladeWidget * widget,
				       GladeXmlContext * context, GladeXmlNode * node)
{
  GladeXmlNode *attrs_node;

  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
	glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (GTK_TYPE_COMBO_BOX)->write_widget (adaptor, widget, context, node);

  attrs_node = glade_xml_node_new (context, GLADE_TAG_ITEMS);

  glade_gtk_combo_box_text_write_items (widget, context, attrs_node);

  if (!glade_xml_node_get_children (attrs_node))
    glade_xml_node_delete (attrs_node);
  else
    glade_xml_node_append_child (node, attrs_node);

}


/* ----------------------------- GtkSpinButton ------------------------------ */
static void
glade_gtk_spin_button_set_adjustment (GObject * object, const GValue * value)
{
  GObject *adjustment;
  GtkAdjustment *adj;

  g_return_if_fail (GTK_IS_SPIN_BUTTON (object));

  adjustment = g_value_get_object (value);

  if (adjustment && GTK_IS_ADJUSTMENT (adjustment))
    {
      adj = GTK_ADJUSTMENT (adjustment);

      if (gtk_adjustment_get_page_size (adj) > 0)
        {
          GladeWidget *gadj = glade_widget_get_from_gobject (adj);

          /* Silently set any spin-button adjustment page size to 0 */
          glade_widget_property_set (gadj, "page-size", 0.0F);
          gtk_adjustment_set_page_size (adj, 0);
        }

      gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (object), adj);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (object),
                                 gtk_adjustment_get_value (adj));
    }
}

void
glade_gtk_spin_button_set_property (GladeWidgetAdaptor * adaptor,
                                    GObject * object,
                                    const gchar * id, const GValue * value)
{
  if (!strcmp (id, "adjustment"))
    glade_gtk_spin_button_set_adjustment (object, value);
  else
    GWA_GET_CLASS (GTK_TYPE_ENTRY)->set_property (adaptor, object, id, value);
}

/* ------------------------------ GtkAssistant ------------------------------ */
static void
glade_gtk_assistant_append_new_page (GladeWidget * parent,
                                     GladeProject * project,
                                     const gchar * label,
                                     GtkAssistantPageType type)
{
  static GladeWidgetAdaptor *adaptor = NULL;
  GladeWidget *page;

  if (adaptor == NULL)
    adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_LABEL);

  page = glade_widget_adaptor_create_widget (adaptor, FALSE,
                                             "parent", parent,
                                             "project", project, NULL);

  glade_widget_add_child (parent, page, FALSE);

  glade_widget_property_set (page, "label", label);
  glade_widget_pack_property_set (page, "page-type", type);
}

/*
  GtkAssistant is a very weird widget, why is it derived from GtkWindow
  instead of GtkNotebook I do not know!

  If there is no GTK_ASSISTANT_PAGE_CONFIRM, GtkAssistant abort when trying to 
  update its navigation buttons!
*/
static void
glade_gtk_assistant_update_page_type (GtkAssistant * assistant)
{
  gint i, current, pages;
  GtkWidget *page;

  current = gtk_assistant_get_current_page (assistant);
  pages = gtk_assistant_get_n_pages (assistant) - 1;
  if (pages < 0)
    return;

  /* Last Page */
  page = gtk_assistant_get_nth_page (assistant, pages);
  gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONFIRM);

  /* First page */
  page = gtk_assistant_get_nth_page (assistant, 0);
  gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_INTRO);

  /* In betwen pages */
  for (i = 1; i < pages; i++)
    {
      page = gtk_assistant_get_nth_page (assistant, i);
      gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONTENT);
    }

  /* Now we have set page-type in every page, force button update */
  for (i = 0; i <= pages; i++)
    {
      page = gtk_assistant_get_nth_page (assistant, i);
      gtk_assistant_set_page_complete (assistant, page, TRUE);
    }

  if (current >= 0)
    gtk_assistant_set_current_page (assistant, current);
}

static gint
glade_gtk_assistant_get_page (GtkAssistant * assistant, GtkWidget * page)
{
  gint i, pages = gtk_assistant_get_n_pages (assistant);

  for (i = 0; i < pages; i++)
    if (gtk_assistant_get_nth_page (assistant, i) == page)
      return i;

  return -1;
}

static void
glade_gtk_assistant_update_position (GtkAssistant * assistant)
{
  gint i, pages = gtk_assistant_get_n_pages (assistant);

  for (i = 0; i < pages; i++)
    {
      GtkWidget *page = gtk_assistant_get_nth_page (assistant, i);
      GladeWidget *gpage = glade_widget_get_from_gobject (G_OBJECT (page));
      if (gpage)
        glade_widget_pack_property_set (gpage, "position", i);
    }
}

static void
glade_gtk_assistant_parse_finished (GladeProject * project, GObject * object)
{
  GtkAssistant *assistant = GTK_ASSISTANT (object);
  gint pages = gtk_assistant_get_n_pages (assistant);

  if (pages)
    {
      /* also sets pages "complete" and thus allows navigation under glade */
      glade_gtk_assistant_update_page_type (assistant);

      gtk_assistant_set_current_page (assistant, 0);
      glade_widget_property_set (glade_widget_get_from_gobject (object),
                                 "n-pages", pages);
    }
}

GList *
glade_gtk_assistant_get_children (GladeWidgetAdaptor *adaptor,
                                  GObject *container)
{
  GtkAssistant *assist = GTK_ASSISTANT (container);
  gint i, n_pages = gtk_assistant_get_n_pages (assist);
  GList *children = NULL, *parent_children;

  /* Chain up */
  if (GWA_GET_CLASS (GTK_TYPE_WINDOW)->get_children)
    parent_children = GWA_GET_CLASS (GTK_TYPE_WINDOW)->get_children (adaptor, container);
  else
    parent_children = NULL;
  
  for (i = 0; i < n_pages; i++)
    children = g_list_prepend (children, gtk_assistant_get_nth_page (assist, i));

  children = g_list_reverse (children);

  return glade_util_purify_list (g_list_concat (children, parent_children));
}

static void
on_assistant_project_selection_changed (GladeProject *project,
                                        GladeWidget *gassist)
{
  GList *selection = glade_project_selection_get (project);

  if (selection && g_list_next (selection) == NULL)
    {
      GladeWidget *selected = glade_widget_get_from_gobject (selection->data);
      GtkAssistant *assist = GTK_ASSISTANT (glade_widget_get_object (gassist));
      gint pos;

      if (!selected) return;

      if (glade_widget_get_parent (selected) == gassist &&
          glade_widget_property_get (selected, "position", &pos, NULL))
	gtk_assistant_set_current_page (assist, pos);
    }
}

void
glade_gtk_assistant_post_create (GladeWidgetAdaptor * adaptor,
                                 GObject * object, GladeCreateReason reason)
{
  GladeWidget *parent = glade_widget_get_from_gobject (object);
  GladeProject *project = glade_widget_get_project (parent);

  if (reason == GLADE_CREATE_LOAD)
    {
      g_signal_connect (project, "parse-finished",
                        G_CALLBACK (glade_gtk_assistant_parse_finished),
                        object);
    }
  else if (reason == GLADE_CREATE_USER)
    {
      glade_gtk_assistant_append_new_page (parent, project,
                                           _("Introduction page"),
                                           GTK_ASSISTANT_PAGE_INTRO);

      glade_gtk_assistant_append_new_page (parent, project,
                                           _("Content page"),
                                           GTK_ASSISTANT_PAGE_CONTENT);

      glade_gtk_assistant_append_new_page (parent, project,
                                           _("Confirmation page"),
                                           GTK_ASSISTANT_PAGE_CONFIRM);

      gtk_assistant_set_current_page (GTK_ASSISTANT (object), 0);

      glade_widget_property_set (parent, "n-pages", 3);
    }

  if (project)
    g_signal_connect (project, "selection-changed",
		      G_CALLBACK (on_assistant_project_selection_changed),
		      parent);
}

void
glade_gtk_assistant_add_child (GladeWidgetAdaptor * adaptor,
                               GObject * container, GObject * child)
{
  GtkAssistant *assistant = GTK_ASSISTANT (container);
  GtkWidget *widget = GTK_WIDGET (child);

  gtk_assistant_append_page (assistant, widget);
}

static void
assistant_remove_child (GtkAssistant *assistant, GtkWidget *child)
{
  gint i, n = gtk_assistant_get_n_pages (assistant);

  for (i = 0; i < n; i++)
    {
      if (child == gtk_assistant_get_nth_page (assistant, i))
	{
	  gtk_assistant_remove_page (assistant, i);
	  return;
	}
    }
}

void
glade_gtk_assistant_remove_child (GladeWidgetAdaptor * adaptor,
                                  GObject * container, GObject * child)
{
  GladeWidget *gassistant = glade_widget_get_from_gobject (container);
  GtkAssistant *assistant = GTK_ASSISTANT (container);

  assistant_remove_child (assistant, GTK_WIDGET (child));

  glade_widget_property_set (gassistant, "n-pages", 
                             gtk_assistant_get_n_pages (assistant));
}

void
glade_gtk_assistant_replace_child (GladeWidgetAdaptor * adaptor,
                                   GObject * container,
                                   GObject * current, GObject * new_object)
{
  GtkAssistant *assistant = GTK_ASSISTANT (container);
  GtkWidget *page = GTK_WIDGET (new_object), *old_page = GTK_WIDGET (current);
  gint pos = glade_gtk_assistant_get_page (assistant, old_page);
  gboolean set_current = gtk_assistant_get_current_page (assistant) == pos;

  assistant_remove_child (assistant, old_page);

  gtk_assistant_insert_page (assistant, page, pos);
  glade_gtk_assistant_update_page_type (assistant);

  if (set_current)
    gtk_assistant_set_current_page (assistant, pos);
}

gboolean
glade_gtk_assistant_verify_property (GladeWidgetAdaptor * adaptor,
                                     GObject * object,
                                     const gchar * property_name,
                                     const GValue * value)
{
  if (strcmp (property_name, "n-pages") == 0)
    return g_value_get_int (value) >=
        gtk_assistant_get_n_pages (GTK_ASSISTANT (object));

  /* Chain Up */
  if (GWA_GET_CLASS (GTK_TYPE_WINDOW)->verify_property == NULL)
    return TRUE;
  return GWA_GET_CLASS (GTK_TYPE_WINDOW)->verify_property (adaptor,
                                                           object,
                                                           property_name,
                                                           value);
}

void
glade_gtk_assistant_set_property (GladeWidgetAdaptor * adaptor,
                                  GObject * object,
                                  const gchar * property_name,
                                  const GValue * value)
{
  if (strcmp (property_name, "n-pages") == 0)
    {
      GtkAssistant *assistant = GTK_ASSISTANT (object);
      gint size, i;

      for (i = gtk_assistant_get_n_pages (GTK_ASSISTANT (object)),
           size = g_value_get_int (value); i < size; i++)
	{
	  g_message ("aaaa %d %d", i,size);
        gtk_assistant_append_page (assistant, glade_placeholder_new ());
	}

      glade_gtk_assistant_update_page_type (assistant);

      return;
    }

  /* Chain Up */
  GWA_GET_CLASS (GTK_TYPE_WINDOW)->set_property (adaptor,
                                                 object, property_name, value);
}

void
glade_gtk_assistant_get_property (GladeWidgetAdaptor * adaptor,
                                  GObject * object,
                                  const gchar * property_name, GValue * value)
{
  if (strcmp (property_name, "n-pages") == 0)
    {
      g_value_set_int (value,
                       gtk_assistant_get_n_pages (GTK_ASSISTANT (object)));
      return;
    }

  /* Chain Up */
  GWA_GET_CLASS (GTK_TYPE_WINDOW)->get_property (adaptor,
                                                 object, property_name, value);
}

void
glade_gtk_assistant_set_child_property (GladeWidgetAdaptor * adaptor,
                                        GObject * container,
                                        GObject * child,
                                        const gchar * property_name,
                                        const GValue * value)
{
  if (strcmp (property_name, "position") == 0)
    {
      GtkAssistant *assistant = GTK_ASSISTANT (container);
      GtkWidget *widget = GTK_WIDGET (child);
      gint pos;
      gboolean set_current;

      if ((pos = g_value_get_int (value)) < 0)
        return;
      if (pos == glade_gtk_assistant_get_page (assistant, widget))
        return;
      set_current = gtk_assistant_get_current_page (assistant) ==
          glade_gtk_assistant_get_page (assistant, widget);   

      g_object_ref (child);
      assistant_remove_child (assistant, widget);
      gtk_assistant_insert_page (assistant, widget, pos);
      g_object_unref (child);

      if (set_current)
        gtk_assistant_set_current_page (assistant, pos);

      glade_gtk_assistant_update_page_type (assistant);

      glade_gtk_assistant_update_position (assistant);

      return;
    }

  /* Chain Up */
  GWA_GET_CLASS (GTK_TYPE_WINDOW)->child_set_property (adaptor,
                                                       container,
                                                       child,
                                                       property_name, value);
}

void
glade_gtk_assistant_get_child_property (GladeWidgetAdaptor * adaptor,
                                        GObject * container,
                                        GObject * child,
                                        const gchar * property_name,
                                        GValue * value)
{
  if (strcmp (property_name, "position") == 0)
    {
      gint pos;
      pos = glade_gtk_assistant_get_page (GTK_ASSISTANT (container),
                                          GTK_WIDGET (child));
      if (pos >= 0)
        g_value_set_int (value, pos);
      return;
    }

  /* Chain Up */
  GWA_GET_CLASS (GTK_TYPE_WINDOW)->child_get_property (adaptor,
                                                       container,
                                                       child,
                                                       property_name, value);
}

/*--------------------------- GtkRadioButton ---------------------------------*/
void
glade_gtk_radio_button_set_property (GladeWidgetAdaptor * adaptor,
                                     GObject * object,
                                     const gchar * property_name,
                                     const GValue * value)
{
  if (strcmp (property_name, "group") == 0)
    {
      GtkRadioButton *radio = g_value_get_object (value);
      /* g_object_set () on this property produces a bogus warning,
       * so we better use the API GtkRadioButton provides.
       */
      gtk_radio_button_set_group (GTK_RADIO_BUTTON (object),
                                  radio ? gtk_radio_button_get_group (radio) :
                                  NULL);
      return;
    }

  /* Chain Up */
  GWA_GET_CLASS (GTK_TYPE_CHECK_BUTTON)->set_property (adaptor,
                                                       object,
                                                       property_name, value);
}

/*--------------------------- GtkSizeGroup ---------------------------------*/
gboolean
glade_gtk_size_group_depends (GladeWidgetAdaptor * adaptor,
                              GladeWidget * widget, GladeWidget * another)
{
  if (GTK_IS_WIDGET (glade_widget_get_object (another)))
    return TRUE;

  return GWA_GET_CLASS (G_TYPE_OBJECT)->depends (adaptor, widget, another);
}

#define GLADE_TAG_SIZEGROUP_WIDGETS "widgets"
#define GLADE_TAG_SIZEGROUP_WIDGET  "widget"

static void
glade_gtk_size_group_read_widgets (GladeWidget * widget, GladeXmlNode * node)
{
  GladeXmlNode *widgets_node;
  GladeProperty *property;
  gchar *string = NULL;

  if ((widgets_node =
       glade_xml_search_child (node, GLADE_TAG_SIZEGROUP_WIDGETS)) != NULL)
    {
      GladeXmlNode *node;

      for (node = glade_xml_node_get_children (widgets_node);
           node; node = glade_xml_node_next (node))
        {
          gchar *widget_name, *tmp;

          if (!glade_xml_node_verify (node, GLADE_TAG_SIZEGROUP_WIDGET))
            continue;

          widget_name = glade_xml_get_property_string_required
              (node, GLADE_TAG_NAME, NULL);

          if (string == NULL)
            string = widget_name;
          else if (widget_name != NULL)
            {
              tmp =
                  g_strdup_printf ("%s%s%s", string, GPC_OBJECT_DELIMITER,
                                   widget_name);
              string = (g_free (string), tmp);
              g_free (widget_name);
            }
        }
    }


  if (string)
    {
      property = glade_widget_get_property (widget, "widgets");
      g_assert (property);

      /* we must synchronize this directly after loading this project
       * (i.e. lookup the actual objects after they've been parsed and
       * are present).
       */
      g_object_set_data_full (G_OBJECT (property),
                              "glade-loaded-object", string, g_free);
    }
}

void
glade_gtk_size_group_read_widget (GladeWidgetAdaptor * adaptor,
                                  GladeWidget * widget, GladeXmlNode * node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
	glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->read_widget (adaptor, widget, node);

  glade_gtk_size_group_read_widgets (widget, node);
}


static void
glade_gtk_size_group_write_widgets (GladeWidget * widget,
                                    GladeXmlContext * context,
                                    GladeXmlNode * node)
{
  GladeXmlNode *widgets_node, *widget_node;
  GList *widgets = NULL, *list;
  GladeWidget *awidget;

  widgets_node = glade_xml_node_new (context, GLADE_TAG_SIZEGROUP_WIDGETS);

  if (glade_widget_property_get (widget, "widgets", &widgets))
    {
      for (list = widgets; list; list = list->next)
        {
          awidget = glade_widget_get_from_gobject (list->data);
          widget_node =
              glade_xml_node_new (context, GLADE_TAG_SIZEGROUP_WIDGET);
          glade_xml_node_append_child (widgets_node, widget_node);
          glade_xml_node_set_property_string (widget_node, GLADE_TAG_NAME,
                                              glade_widget_get_name (awidget));
        }
    }

  if (!glade_xml_node_get_children (widgets_node))
    glade_xml_node_delete (widgets_node);
  else
    glade_xml_node_append_child (node, widgets_node);

}


void
glade_gtk_size_group_write_widget (GladeWidgetAdaptor * adaptor,
                                   GladeWidget * widget,
                                   GladeXmlContext * context,
                                   GladeXmlNode * node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
	glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);

  glade_gtk_size_group_write_widgets (widget, context, node);
}


void
glade_gtk_size_group_set_property (GladeWidgetAdaptor * adaptor,
                                   GObject * object,
                                   const gchar * property_name,
                                   const GValue * value)
{
  if (!strcmp (property_name, "widgets"))
    {
      GSList *sg_widgets, *slist;
      GList *widgets, *list;

      /* remove old widgets */
      if ((sg_widgets =
           gtk_size_group_get_widgets (GTK_SIZE_GROUP (object))) != NULL)
        {
          /* copy since we are modifying an internal list */
          sg_widgets = g_slist_copy (sg_widgets);
          for (slist = sg_widgets; slist; slist = slist->next)
            gtk_size_group_remove_widget (GTK_SIZE_GROUP (object),
                                          GTK_WIDGET (slist->data));
          g_slist_free (sg_widgets);
        }

      /* add new widgets */
      if ((widgets = g_value_get_boxed (value)) != NULL)
        {
          for (list = widgets; list; list = list->next)
            gtk_size_group_add_widget (GTK_SIZE_GROUP (object),
                                       GTK_WIDGET (list->data));
        }
    }
  else
    GWA_GET_CLASS (G_TYPE_OBJECT)->set_property (adaptor, object,
                                                 property_name, value);
}

/*--------------------------- GtkIconFactory ---------------------------------*/
#define GLADE_TAG_SOURCES   "sources"
#define GLADE_TAG_SOURCE    "source"

#define GLADE_TAG_STOCK_ID  "stock-id"
#define GLADE_TAG_FILENAME  "filename"
#define GLADE_TAG_DIRECTION "direction"
#define GLADE_TAG_STATE     "state"
#define GLADE_TAG_SIZE      "size"

void
glade_gtk_icon_factory_post_create (GladeWidgetAdaptor * adaptor,
                                    GObject * object, GladeCreateReason reason)
{
  gtk_icon_factory_add_default (GTK_ICON_FACTORY (object));
}

void
glade_gtk_icon_factory_destroy_object (GladeWidgetAdaptor * adaptor,
				       GObject *object)
{
  gtk_icon_factory_remove_default (GTK_ICON_FACTORY (object));

  GWA_GET_CLASS (G_TYPE_OBJECT)->destroy_object (adaptor, object);
}

static void
glade_gtk_icon_factory_read_sources (GladeWidget * widget, GladeXmlNode * node)
{
  GladeIconSources *sources;
  GtkIconSource *source;
  GladeXmlNode *sources_node, *source_node;
  GValue *value;
  GList *list;
  gchar *current_icon_name = NULL;
  GdkPixbuf *pixbuf;

  if ((sources_node = glade_xml_search_child (node, GLADE_TAG_SOURCES)) == NULL)
    return;

  sources = glade_icon_sources_new ();

  /* Here we expect all icon sets to remain together in the list. */
  for (source_node = glade_xml_node_get_children (sources_node); source_node;
       source_node = glade_xml_node_next (source_node))
    {
      gchar *icon_name;
      gchar *str;

      if (!glade_xml_node_verify (source_node, GLADE_TAG_SOURCE))
        continue;

      if (!(icon_name =
            glade_xml_get_property_string_required (source_node,
                                                    GLADE_TAG_STOCK_ID, NULL)))
        continue;

      if (!
          (str =
           glade_xml_get_property_string_required (source_node,
                                                   GLADE_TAG_FILENAME, NULL)))
        {
          g_free (icon_name);
          continue;
        }

      if (!current_icon_name || strcmp (current_icon_name, icon_name) != 0)
        current_icon_name = (g_free (current_icon_name), g_strdup (icon_name));

      source = gtk_icon_source_new ();

      /* Deal with the filename... */
      value = glade_utils_value_from_string (GDK_TYPE_PIXBUF, str, glade_widget_get_project (widget));
      pixbuf = g_value_dup_object (value);
      g_value_unset (value);
      g_free (value);

      gtk_icon_source_set_pixbuf (source, pixbuf);
      g_object_unref (G_OBJECT (pixbuf));
      g_free (str);

      /* Now the attributes... */
      if ((str =
           glade_xml_get_property_string (source_node,
                                          GLADE_TAG_DIRECTION)) != NULL)
        {
          GtkTextDirection direction =
              glade_utils_enum_value_from_string (GTK_TYPE_TEXT_DIRECTION, str);
          gtk_icon_source_set_direction_wildcarded (source, FALSE);
          gtk_icon_source_set_direction (source, direction);
          g_free (str);
        }

      if ((str =
           glade_xml_get_property_string (source_node, GLADE_TAG_SIZE)) != NULL)
        {
          GtkIconSize size =
              glade_utils_enum_value_from_string (GTK_TYPE_ICON_SIZE, str);
          gtk_icon_source_set_size_wildcarded (source, FALSE);
          gtk_icon_source_set_size (source, size);
          g_free (str);
        }

      if ((str =
           glade_xml_get_property_string (source_node,
                                          GLADE_TAG_STATE)) != NULL)
        {
          GtkStateType state =
              glade_utils_enum_value_from_string (GTK_TYPE_STATE_TYPE, str);
          gtk_icon_source_set_state_wildcarded (source, FALSE);
          gtk_icon_source_set_state (source, state);
          g_free (str);
        }

      if ((list =
           g_hash_table_lookup (sources->sources,
                                g_strdup (current_icon_name))) != NULL)
        {
          GList *new_list = g_list_append (list, source);

          /* Warning: if we use g_list_prepend() the returned pointer will be different
           * so we would have to replace the list pointer in the hash table.
           * But before doing that we have to steal the old list pointer otherwise
           * we would have to make a copy then add the new icon to finally replace the hash table
           * value.
           * Anyways if we choose to prepend we would have to reverse the list outside this loop
           * so its better to append.
           */
          if (new_list != list)
            {
              /* current g_list_append() returns the same pointer so this is not needed */
              g_hash_table_steal (sources->sources, current_icon_name);
              g_hash_table_insert (sources->sources,
                                   g_strdup (current_icon_name), new_list);
            }
        }
      else
        {
          list = g_list_append (NULL, source);
          g_hash_table_insert (sources->sources, g_strdup (current_icon_name),
                               list);
        }
    }

  if (g_hash_table_size (sources->sources) > 0)
    glade_widget_property_set (widget, "sources", sources);

  glade_icon_sources_free (sources);
}

void
glade_gtk_icon_factory_read_widget (GladeWidgetAdaptor * adaptor,
                                    GladeWidget * widget, GladeXmlNode * node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
	glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in any normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->read_widget (adaptor, widget, node);

  glade_gtk_icon_factory_read_sources (widget, node);
}

typedef struct
{
  GladeXmlContext *context;
  GladeXmlNode *node;
} SourceWriteTab;

static void
write_icon_sources (gchar * icon_name, GList * sources, SourceWriteTab * tab)
{
  GladeXmlNode *source_node;
  GtkIconSource *source;
  GList *l;
  gchar *string;

  GdkPixbuf *pixbuf;

  for (l = sources; l; l = l->next)
    {
      source = l->data;

      source_node = glade_xml_node_new (tab->context, GLADE_TAG_SOURCE);
      glade_xml_node_append_child (tab->node, source_node);

      glade_xml_node_set_property_string (source_node, GLADE_TAG_STOCK_ID,
                                          icon_name);

      if (!gtk_icon_source_get_direction_wildcarded (source))
        {
          GtkTextDirection direction = gtk_icon_source_get_direction (source);
          string =
              glade_utils_enum_string_from_value (GTK_TYPE_TEXT_DIRECTION,
                                                  direction);
          glade_xml_node_set_property_string (source_node, GLADE_TAG_DIRECTION,
                                              string);
          g_free (string);
        }

      if (!gtk_icon_source_get_size_wildcarded (source))
        {
          GtkIconSize size = gtk_icon_source_get_size (source);
          string =
              glade_utils_enum_string_from_value (GTK_TYPE_ICON_SIZE, size);
          glade_xml_node_set_property_string (source_node, GLADE_TAG_SIZE,
                                              string);
          g_free (string);
        }

      if (!gtk_icon_source_get_state_wildcarded (source))
        {
          GtkStateType state = gtk_icon_source_get_state (source);
          string =
              glade_utils_enum_string_from_value (GTK_TYPE_STATE_TYPE, state);
          glade_xml_node_set_property_string (source_node, GLADE_TAG_STATE,
                                              string);
          g_free (string);
        }

      pixbuf = gtk_icon_source_get_pixbuf (source);
      string = g_object_get_data (G_OBJECT (pixbuf), "GladeFileName");

      glade_xml_node_set_property_string (source_node,
                                          GLADE_TAG_FILENAME, string);
    }
}


static void
glade_gtk_icon_factory_write_sources (GladeWidget * widget,
                                      GladeXmlContext * context,
                                      GladeXmlNode * node)
{
  GladeXmlNode *sources_node;
  GladeIconSources *sources = NULL;
  SourceWriteTab tab;

  glade_widget_property_get (widget, "sources", &sources);
  if (!sources)
    return;

  sources_node = glade_xml_node_new (context, GLADE_TAG_SOURCES);

  tab.context = context;
  tab.node = sources_node;
  g_hash_table_foreach (sources->sources, (GHFunc) write_icon_sources, &tab);

  if (!glade_xml_node_get_children (sources_node))
    glade_xml_node_delete (sources_node);
  else
    glade_xml_node_append_child (node, sources_node);

}


void
glade_gtk_icon_factory_write_widget (GladeWidgetAdaptor * adaptor,
                                     GladeWidget * widget,
                                     GladeXmlContext * context,
                                     GladeXmlNode * node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
	glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and write all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);

  glade_gtk_icon_factory_write_sources (widget, context, node);
}

static void
apply_icon_sources (gchar * icon_name,
                    GList * sources, GtkIconFactory * factory)
{
  GtkIconSource *source;
  GtkIconSet *set;
  GList *l;

  set = gtk_icon_set_new ();

  for (l = sources; l; l = l->next)
    {
      source = gtk_icon_source_copy ((GtkIconSource *) l->data);
      gtk_icon_set_add_source (set, source);
    }

  gtk_icon_factory_add (factory, icon_name, set);
}

static void
glade_gtk_icon_factory_set_sources (GObject * object, const GValue * value)
{
  GladeIconSources *sources = g_value_get_boxed (value);
  if (sources)
    g_hash_table_foreach (sources->sources, (GHFunc) apply_icon_sources,
                          object);
}


void
glade_gtk_icon_factory_set_property (GladeWidgetAdaptor * adaptor,
                                     GObject * object,
                                     const gchar * property_name,
                                     const GValue * value)
{
  if (strcmp (property_name, "sources") == 0)
    {
      glade_gtk_icon_factory_set_sources (object, value);
    }
  else
    /* Chain Up */
    GWA_GET_CLASS (G_TYPE_OBJECT)->set_property (adaptor,
                                                 object, property_name, value);
}

static void
serialize_icon_sources (gchar * icon_name, GList * sources, GString * string)
{
  GList *l;

  for (l = sources; l; l = g_list_next (l))
    {
      GtkIconSource *source = l->data;
      GdkPixbuf *pixbuf;
      gchar *str;

      pixbuf = gtk_icon_source_get_pixbuf (source);
      str = g_object_get_data (G_OBJECT (pixbuf), "GladeFileName");

      g_string_append_printf (string, "%s[%s] ", icon_name, str);

      if (!gtk_icon_source_get_direction_wildcarded (source))
        {
          GtkTextDirection direction = gtk_icon_source_get_direction (source);
          str =
              glade_utils_enum_string_from_value (GTK_TYPE_TEXT_DIRECTION,
                                                  direction);
          g_string_append_printf (string, "dir-%s ", str);
          g_free (str);
        }

      if (!gtk_icon_source_get_size_wildcarded (source))
        {
          GtkIconSize size = gtk_icon_source_get_size (source);
          str = glade_utils_enum_string_from_value (GTK_TYPE_ICON_SIZE, size);
          g_string_append_printf (string, "size-%s ", str);
          g_free (str);
        }

      if (!gtk_icon_source_get_state_wildcarded (source))
        {
          GtkStateType state = gtk_icon_source_get_state (source);
          str = glade_utils_enum_string_from_value (GTK_TYPE_STATE_TYPE, state);
          g_string_append_printf (string, "state-%s ", str);
          g_free (str);
        }

      g_string_append_printf (string, "| ");
    }
}

gchar *
glade_gtk_icon_factory_string_from_value (GladeWidgetAdaptor * adaptor,
                                          GladePropertyClass * klass,
                                          const GValue * value)
{
  GString *string;
  GParamSpec *pspec;

  pspec = glade_property_class_get_pspec (klass);

  if (pspec->value_type == GLADE_TYPE_ICON_SOURCES)
    {
      GladeIconSources *sources = g_value_get_boxed (value);
      if (!sources)
        return g_strdup ("");

      string = g_string_new ("");
      g_hash_table_foreach (sources->sources, (GHFunc) serialize_icon_sources,
                            string);

      return g_string_free (string, FALSE);
    }
  else
    return GWA_GET_CLASS
        (G_TYPE_OBJECT)->string_from_value (adaptor, klass, value);
}


GladeEditorProperty *
glade_gtk_icon_factory_create_eprop (GladeWidgetAdaptor * adaptor,
                                     GladePropertyClass * klass,
                                     gboolean use_command)
{
  GladeEditorProperty *eprop;
  GParamSpec          *pspec;

  pspec = glade_property_class_get_pspec (klass);

  if (pspec->value_type == GLADE_TYPE_ICON_SOURCES)
    eprop = g_object_new (GLADE_TYPE_EPROP_ICON_SOURCES,
                          "property-class", klass,
                          "use-command", use_command, NULL);
  else
    eprop = GWA_GET_CLASS
        (G_TYPE_OBJECT)->create_eprop (adaptor, klass, use_command);
  return eprop;
}

GladeEditable *
glade_gtk_icon_factory_create_editable (GladeWidgetAdaptor * adaptor,
                                        GladeEditorPageType type)
{
  GladeEditable *editable;

  /* Get base editable */
  editable = GWA_GET_CLASS (G_TYPE_OBJECT)->create_editable (adaptor, type);

  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_icon_factory_editor_new (adaptor, editable);

  return editable;
}


/*--------------------------- GtkListStore/GtkTreeStore ---------------------------------*/

#define GLADE_TAG_COLUMNS	"columns"
#define GLADE_TAG_COLUMN	"column"
#define GLADE_TAG_TYPE		"type"

#define GLADE_TAG_ROW           "row"
#define GLADE_TAG_DATA          "data"
#define GLADE_TAG_COL           "col"


static gboolean
glade_gtk_cell_layout_has_renderer (GtkCellLayout * layout,
                                    GtkCellRenderer * renderer)
{
  GList *cells = gtk_cell_layout_get_cells (layout);
  gboolean has_renderer;

  has_renderer = (g_list_find (cells, renderer) != NULL);

  g_list_free (cells);

  return has_renderer;
}

static gboolean
glade_gtk_cell_renderer_sync_attributes (GObject * object)
{

  GtkCellLayout *layout;
  GtkCellRenderer *cell;
  GladeWidget *widget;
  GladeWidget *parent;
  GladeWidget *gmodel;
  GladeProperty *property;
  GladePropertyClass *pclass;
  gchar *attr_prop_name;
  GList *l, *column_list = NULL;
  gint columns = 0;
  static gint attr_len = 0;

  if (!attr_len)
    attr_len = strlen ("attr-");

  /* Apply attributes to renderer when bound to a model in runtime */
  widget = glade_widget_get_from_gobject (object);

  parent = glade_widget_get_parent (widget);
  if (parent == NULL)
    return FALSE;

  /* When creating widgets, sometimes the parent is set before parenting happens,
   * here we have to be careful for that..
   */
  layout = GTK_CELL_LAYOUT (glade_widget_get_object (parent));
  cell   = GTK_CELL_RENDERER (object);

  if (!glade_gtk_cell_layout_has_renderer (layout, cell))
    return FALSE;

  if ((gmodel = glade_cell_renderer_get_model (widget)) == NULL)
    return FALSE;

  glade_widget_property_get (gmodel, "columns", &column_list);
  columns = g_list_length (column_list);

  gtk_cell_layout_clear_attributes (layout, cell);

  for (l = glade_widget_get_properties (widget); l; l = l->next)
    {
      property = l->data;
      pclass   = glade_property_get_class (property);

      if (strncmp (glade_property_class_id (pclass), "attr-", attr_len) == 0)
        {
          gint column = g_value_get_int (glade_property_inline_value (property));

          attr_prop_name = (gchar *)&glade_property_class_id (pclass)[attr_len];

          if (column >= 0 && column < columns)
            {
              GladeColumnType *column_type =
                  (GladeColumnType *) g_list_nth_data (column_list, column);
              GType column_gtype = g_type_from_name (column_type->type_name);
	      GParamSpec *pspec = glade_property_class_get_pspec (pclass);

              if (column_gtype &&
                  g_value_type_transformable (column_gtype, pspec->value_type))
                gtk_cell_layout_add_attribute (layout, cell, attr_prop_name, column);
            }
        }
    }

  return FALSE;
}


static gboolean
glade_gtk_cell_layout_sync_attributes (GObject * layout)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (layout);
  GObject *cell;
  GList *children, *l;

  children = glade_widget_get_children (gwidget);
  for (l = children; l; l = l->next)
    {
      cell = l->data;
      if (!GTK_IS_CELL_RENDERER (cell))
        continue;

      glade_gtk_cell_renderer_sync_attributes (cell);
    }
  g_list_free (children);

  return FALSE;
}

static void
glade_gtk_store_set_columns (GObject * object, const GValue * value)
{
  GList *l;
  gint i, n;
  GType *types;

  for (i = 0, l = g_value_get_boxed (value), n = g_list_length (l), types =
       g_new (GType, n); l; l = g_list_next (l), i++)
    {
      GladeColumnType *data = l->data;

      if (g_type_from_name (data->type_name) != G_TYPE_INVALID)
        types[i] = g_type_from_name (data->type_name);
      else
        types[i] = G_TYPE_POINTER;
    }

  if (GTK_IS_LIST_STORE (object))
    gtk_list_store_set_column_types (GTK_LIST_STORE (object), n, types);
  else
    gtk_tree_store_set_column_types (GTK_TREE_STORE (object), n, types);

  g_free (types);
}

static void
glade_gtk_store_set_data (GObject * object, const GValue * value)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);
  GList *columns = NULL;
  GNode *data_tree, *row, *iter;
  gint colnum;
  GtkTreeIter row_iter;
  GladeModelData *data;
  GType column_type;

  if (GTK_IS_LIST_STORE (object))
    gtk_list_store_clear (GTK_LIST_STORE (object));
  else
    gtk_tree_store_clear (GTK_TREE_STORE (object));

  glade_widget_property_get (gwidget, "columns", &columns);
  data_tree = g_value_get_boxed (value);

  /* Nothing to enter without columns defined */
  if (!data_tree || !columns)
    return;

  for (row = data_tree->children; row; row = row->next)
    {
      if (GTK_IS_LIST_STORE (object))
        gtk_list_store_append (GTK_LIST_STORE (object), &row_iter);
      else
        /* (for now no child data... ) */
        gtk_tree_store_append (GTK_TREE_STORE (object), &row_iter, NULL);

      for (colnum = 0, iter = row->children; iter; colnum++, iter = iter->next)
        {
          data = iter->data;

          if (!g_list_nth (columns, colnum))
            break;

          /* Abort if theres a type mismatch, the widget's being rebuilt
           * and a sync will come soon with the right values
           */
          column_type =
              gtk_tree_model_get_column_type (GTK_TREE_MODEL (object), colnum);
          if (G_VALUE_TYPE (&data->value) != column_type)
            continue;

          if (GTK_IS_LIST_STORE (object))
            gtk_list_store_set_value (GTK_LIST_STORE (object),
                                      &row_iter, colnum, &data->value);
          else
            gtk_tree_store_set_value (GTK_TREE_STORE (object),
                                      &row_iter, colnum, &data->value);
        }
    }
}

void
glade_gtk_store_set_property (GladeWidgetAdaptor * adaptor,
                              GObject * object,
                              const gchar * property_name, const GValue * value)
{
  if (strcmp (property_name, "columns") == 0)
    {
      glade_gtk_store_set_columns (object, value);
    }
  else if (strcmp (property_name, "data") == 0)
    {
      glade_gtk_store_set_data (object, value);
    }
  else
    /* Chain Up */
    GWA_GET_CLASS (G_TYPE_OBJECT)->set_property (adaptor,
                                                 object, property_name, value);
}

GladeEditorProperty *
glade_gtk_store_create_eprop (GladeWidgetAdaptor * adaptor,
                              GladePropertyClass * klass, gboolean use_command)
{
  GladeEditorProperty *eprop;
  GParamSpec          *pspec;

  pspec = glade_property_class_get_pspec (klass);

  /* chain up.. */
  if (pspec->value_type == GLADE_TYPE_COLUMN_TYPE_LIST)
    eprop = g_object_new (GLADE_TYPE_EPROP_COLUMN_TYPES,
                          "property-class", klass,
                          "use-command", use_command, NULL);
  else if (pspec->value_type == GLADE_TYPE_MODEL_DATA_TREE)
    eprop = g_object_new (GLADE_TYPE_EPROP_MODEL_DATA,
                          "property-class", klass,
                          "use-command", use_command, NULL);
  else
    eprop = GWA_GET_CLASS
        (G_TYPE_OBJECT)->create_eprop (adaptor, klass, use_command);
  return eprop;
}


static void
glade_gtk_store_columns_changed (GladeProperty * property,
                                 GValue * old_value,
                                 GValue * new_value, GladeWidget * store)
{
  GList *l, *list, *children, *prop_refs;

  /* Reset the attributes for all cell renderers referring to this store */
  prop_refs = glade_widget_list_prop_refs (store);
  for (l = prop_refs; l; l = l->next)
    {
      GladeWidget *referring_widget = glade_property_get_widget (GLADE_PROPERTY (l->data));
      GObject     *referring_object = glade_widget_get_object (referring_widget);

      if (GTK_IS_CELL_LAYOUT (referring_object))
        glade_gtk_cell_layout_sync_attributes (referring_object);
      else if (GTK_IS_TREE_VIEW (referring_object))
        {
          children = glade_widget_get_children (referring_widget);

          for (list = children; list; list = list->next)
            {
              /* Clear the GtkTreeViewColumns... */
              if (GTK_IS_CELL_LAYOUT (list->data))
                glade_gtk_cell_layout_sync_attributes (G_OBJECT (list->data));
            }

          g_list_free (children);
        }
    }
  g_list_free (prop_refs);
}

void
glade_gtk_store_post_create (GladeWidgetAdaptor * adaptor,
                             GObject * object, GladeCreateReason reason)
{
  GladeWidget *gwidget;
  GladeProperty *property;

  if (reason == GLADE_CREATE_REBUILD)
    return;

  gwidget = glade_widget_get_from_gobject (object);
  property = glade_widget_get_property (gwidget, "columns");

  /* Here we watch the value-changed signal on the "columns" property, we need
   * to reset all the Cell Renderer attributes when the underlying "columns" change,
   * the reason we do it from "value-changed" is because GladeWidget prop references
   * are unavailable while rebuilding an object, and the liststore needs to be rebuilt
   * in order to set the columns.
   *
   * This signal will be envoked after applying the new column types to the store
   * and before the views get any signal to update themselves from the changed model,
   * perfect time to reset the attributes.
   */
  g_signal_connect (G_OBJECT (property), "value-changed",
                    G_CALLBACK (glade_gtk_store_columns_changed), gwidget);
}

GladeEditable *
glade_gtk_store_create_editable (GladeWidgetAdaptor * adaptor,
                                 GladeEditorPageType type)
{
  GladeEditable *editable;

  /* Get base editable */
  editable = GWA_GET_CLASS (G_TYPE_OBJECT)->create_editable (adaptor, type);

  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_store_editor_new (adaptor, editable);

  return editable;
}

gchar *
glade_gtk_store_string_from_value (GladeWidgetAdaptor * adaptor,
                                   GladePropertyClass * klass,
                                   const GValue * value)
{
  GString *string;
  GParamSpec *pspec;

  pspec = glade_property_class_get_pspec (klass);

  if (pspec->value_type == GLADE_TYPE_COLUMN_TYPE_LIST)
    {
      GList *l;
      string = g_string_new ("");
      for (l = g_value_get_boxed (value); l; l = g_list_next (l))
        {
          GladeColumnType *data = l->data;
          g_string_append_printf (string,
                                  (g_list_next (l)) ? "%s:%s|" : "%s:%s",
                                  data->type_name, data->column_name);
        }
      return g_string_free (string, FALSE);
    }
  else if (pspec->value_type == GLADE_TYPE_MODEL_DATA_TREE)
    {
      GladeModelData *data;
      GNode *data_tree, *row, *iter;
      gint rownum;
      gchar *str;
      gboolean is_last;

      /* Return a unique string for the backend to compare */
      data_tree = g_value_get_boxed (value);

      if (!data_tree || !data_tree->children)
        return g_strdup ("");

      string = g_string_new ("");
      for (rownum = 0, row = data_tree->children; row;
           rownum++, row = row->next)
        {
          for (iter = row->children; iter; iter = iter->next)
            {
              data = iter->data;

              if (!G_VALUE_TYPE (&data->value) ||
                  G_VALUE_TYPE (&data->value) == G_TYPE_INVALID)
                str = g_strdup ("(virtual)");
              else if (G_VALUE_TYPE (&data->value) != G_TYPE_POINTER)
                str = glade_utils_string_from_value (&data->value);
              else
                str = g_strdup ("(null)");

              is_last = !row->next && !iter->next;
              g_string_append_printf (string, "%s[%d]:%s",
                                      data->name, rownum, str);

              if (data->i18n_translatable)
                g_string_append_printf (string, " translatable");
              if (data->i18n_context)
                g_string_append_printf (string, " i18n-context:%s",
                                        data->i18n_context);
              if (data->i18n_comment)
                g_string_append_printf (string, " i18n-comment:%s",
                                        data->i18n_comment);

              if (!is_last)
                g_string_append_printf (string, "|");

              g_free (str);
            }
        }
      return g_string_free (string, FALSE);
    }
  else
    return GWA_GET_CLASS
        (G_TYPE_OBJECT)->string_from_value (adaptor, klass, value);
}

static void
glade_gtk_store_write_columns (GladeWidget * widget,
                               GladeXmlContext * context, GladeXmlNode * node)
{
  GladeXmlNode *columns_node;
  GladeProperty *prop;
  GList *l;

  prop = glade_widget_get_property (widget, "columns");

  columns_node = glade_xml_node_new (context, GLADE_TAG_COLUMNS);

  for (l = g_value_get_boxed (glade_property_inline_value (prop)); l; l = g_list_next (l))
    {
      GladeColumnType *data = l->data;
      GladeXmlNode *column_node, *comment_node;

      /* Write column names in comments... */
      gchar *comment = g_strdup_printf (" column-name %s ", data->column_name);
      comment_node = glade_xml_node_new_comment (context, comment);
      glade_xml_node_append_child (columns_node, comment_node);
      g_free (comment);

      column_node = glade_xml_node_new (context, GLADE_TAG_COLUMN);
      glade_xml_node_append_child (columns_node, column_node);
      glade_xml_node_set_property_string (column_node, GLADE_TAG_TYPE,
                                          data->type_name);
    }

  if (!glade_xml_node_get_children (columns_node))
    glade_xml_node_delete (columns_node);
  else
    glade_xml_node_append_child (node, columns_node);

}

static void
glade_gtk_store_write_data (GladeWidget * widget,
                            GladeXmlContext * context, GladeXmlNode * node)
{
  GladeXmlNode *data_node, *col_node, *row_node;
  GList *columns = NULL;
  GladeModelData *data;
  GNode *data_tree = NULL, *row, *iter;
  gint colnum;

  glade_widget_property_get (widget, "data", &data_tree);
  glade_widget_property_get (widget, "columns", &columns);

  /* XXX log errors about data not fitting columns here when
   * loggin is available
   */
  if (!data_tree || !columns)
    return;

  data_node = glade_xml_node_new (context, GLADE_TAG_DATA);

  for (row = data_tree->children; row; row = row->next)
    {
      row_node = glade_xml_node_new (context, GLADE_TAG_ROW);
      glade_xml_node_append_child (data_node, row_node);

      for (colnum = 0, iter = row->children; iter; colnum++, iter = iter->next)
        {
          gchar *string, *column_number;

          data = iter->data;

          /* Skip non-serializable data */
          if (G_VALUE_TYPE (&data->value) == 0 ||
              G_VALUE_TYPE (&data->value) == G_TYPE_POINTER)
            continue;

          string = glade_utils_string_from_value (&data->value);

          /* XXX Log error: data col j exceeds columns on row i */
          if (!g_list_nth (columns, colnum))
            break;

          column_number = g_strdup_printf ("%d", colnum);

          col_node = glade_xml_node_new (context, GLADE_TAG_COL);
          glade_xml_node_append_child (row_node, col_node);
          glade_xml_node_set_property_string (col_node, GLADE_TAG_ID,
                                              column_number);
          glade_xml_set_content (col_node, string);

          if (data->i18n_translatable)
            glade_xml_node_set_property_string (col_node,
                                                GLADE_TAG_TRANSLATABLE,
                                                GLADE_XML_TAG_I18N_TRUE);
          if (data->i18n_context)
            glade_xml_node_set_property_string (col_node,
                                                GLADE_TAG_CONTEXT,
                                                data->i18n_context);
          if (data->i18n_comment)
            glade_xml_node_set_property_string (col_node,
                                                GLADE_TAG_COMMENT,
                                                data->i18n_comment);


          g_free (column_number);
          g_free (string);
        }
    }

  if (!glade_xml_node_get_children (data_node))
    glade_xml_node_delete (data_node);
  else
    glade_xml_node_append_child (node, data_node);
}


void
glade_gtk_store_write_widget (GladeWidgetAdaptor * adaptor,
                              GladeWidget * widget,
                              GladeXmlContext * context, GladeXmlNode * node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
	glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and write all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);

  glade_gtk_store_write_columns (widget, context, node);
  glade_gtk_store_write_data (widget, context, node);
}

static void
glade_gtk_store_read_columns (GladeWidget * widget, GladeXmlNode * node)
{
  GladeNameContext *context;
  GladeXmlNode *columns_node;
  GladeProperty *property;
  GladeXmlNode *prop;
  GList *types = NULL;
  GValue value = { 0, };
  gchar column_name[256];

  column_name[0] = '\0';
  column_name[255] = '\0';

  if ((columns_node = glade_xml_search_child (node, GLADE_TAG_COLUMNS)) == NULL)
    return;

  context = glade_name_context_new ();

  for (prop = glade_xml_node_get_children_with_comments (columns_node); prop;
       prop = glade_xml_node_next_with_comments (prop))
    {
      GladeColumnType *data;
      gchar *type, *comment_str, buffer[256];

      if (!glade_xml_node_verify_silent (prop, GLADE_TAG_COLUMN) &&
          !glade_xml_node_is_comment (prop))
        continue;

      if (glade_xml_node_is_comment (prop))
        {
          comment_str = glade_xml_get_content (prop);
          if (sscanf (comment_str, " column-name %s", buffer) == 1)
            strncpy (column_name, buffer, 255);

          g_free (comment_str);
          continue;
        }

      type =
          glade_xml_get_property_string_required (prop, GLADE_TAG_TYPE, NULL);

      if (!column_name[0])
	{
	  gchar *cname = g_ascii_strdown (type, -1);

	  data = glade_column_type_new (type, cname);

	  g_free (cname);
	}
      else
	data = glade_column_type_new (type, column_name);

      if (glade_name_context_has_name (context, data->column_name))
        {
          gchar *name =
              glade_name_context_new_name (context, data->column_name);
          g_free (data->column_name);
          data->column_name = name;
        }
      glade_name_context_add_name (context, data->column_name);

      types = g_list_prepend (types, data);
      g_free (type);

      column_name[0] = '\0';
    }

  glade_name_context_destroy (context);

  property = glade_widget_get_property (widget, "columns");
  g_value_init (&value, GLADE_TYPE_COLUMN_TYPE_LIST);
  g_value_take_boxed (&value, g_list_reverse (types));
  glade_property_set_value (property, &value);
  g_value_unset (&value);
}

static void
glade_gtk_store_read_data (GladeWidget * widget, GladeXmlNode * node)
{
  GladeXmlNode *data_node, *row_node, *col_node;
  GNode *data_tree, *row, *item;
  GladeModelData *data;
  GValue *value;
  GList *column_types = NULL;
  GladeColumnType *column_type;
  gint colnum;

  if ((data_node = glade_xml_search_child (node, GLADE_TAG_DATA)) == NULL)
    return;

  /* XXX FIXME: Warn that columns werent there when parsing */
  if (!glade_widget_property_get (widget, "columns", &column_types) ||
      !column_types)
    return;

  /* Create root... */
  data_tree = g_node_new (NULL);

  for (row_node = glade_xml_node_get_children (data_node); row_node;
       row_node = glade_xml_node_next (row_node))
    {
      gchar *value_str;

      if (!glade_xml_node_verify (row_node, GLADE_TAG_ROW))
        continue;

      row = g_node_new (NULL);
      g_node_append (data_tree, row);

      /* XXX FIXME: we are assuming that the columns are listed in order */
      for (colnum = 0, col_node = glade_xml_node_get_children (row_node);
           col_node; col_node = glade_xml_node_next (col_node))
        {
          gint read_column;

          if (!glade_xml_node_verify (col_node, GLADE_TAG_COL))
            continue;

          read_column = glade_xml_get_property_int (col_node, GLADE_TAG_ID, -1);
          if (read_column < 0)
            {
              g_critical ("Parsed negative column id");
              continue;
            }

          /* Catch up for gaps in the list where unserializable types are involved */
          while (colnum < read_column)
            {
              column_type = g_list_nth_data (column_types, colnum);

              data =
                  glade_model_data_new (G_TYPE_INVALID,
                                        column_type->column_name);

              item = g_node_new (data);
              g_node_append (row, item);

              colnum++;
            }

          if (!(column_type = g_list_nth_data (column_types, colnum)))
            /* XXX Log this too... */
            continue;

          /* Ignore unloaded column types for the workspace */
          if (g_type_from_name (column_type->type_name) != G_TYPE_INVALID)
            {
              /* XXX Do we need object properties to somehow work at load time here ??
               * should we be doing this part in "finished" ? ... todo thinkso...
               */
              value_str = glade_xml_get_content (col_node);
              value = glade_utils_value_from_string (g_type_from_name (column_type->type_name), value_str,
						     glade_widget_get_project (widget));
              g_free (value_str);

              data = glade_model_data_new (g_type_from_name (column_type->type_name),
					   column_type->column_name);

              g_value_copy (value, &data->value);
              g_value_unset (value);
              g_free (value);
            }
          else
            {
              data =
                  glade_model_data_new (G_TYPE_INVALID,
                                        column_type->column_name);
            }

          data->i18n_translatable =
              glade_xml_get_property_boolean (col_node, GLADE_TAG_TRANSLATABLE,
                                              FALSE);
          data->i18n_context =
              glade_xml_get_property_string (col_node, GLADE_TAG_CONTEXT);
          data->i18n_comment =
              glade_xml_get_property_string (col_node, GLADE_TAG_COMMENT);

          item = g_node_new (data);
          g_node_append (row, item);

          /* dont increment colnum on invalid xml tags... */
          colnum++;
        }
    }

  if (data_tree->children)
    glade_widget_property_set (widget, "data", data_tree);

  glade_model_data_tree_free (data_tree);
}

void
glade_gtk_store_read_widget (GladeWidgetAdaptor * adaptor,
                             GladeWidget * widget, GladeXmlNode * node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
	glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->read_widget (adaptor, widget, node);

  glade_gtk_store_read_columns (widget, node);

  if (GTK_IS_LIST_STORE (glade_widget_get_object (widget)))
    glade_gtk_store_read_data (widget, node);
}

/*--------------------------- GtkCellRenderer ---------------------------------*/
static void glade_gtk_treeview_launch_editor (GObject * treeview);

void
glade_gtk_cell_renderer_action_activate (GladeWidgetAdaptor * adaptor,
                                         GObject * object,
                                         const gchar * action_path)
{
  if (strcmp (action_path, "launch_editor") == 0)
    {
      GladeWidget *w = glade_widget_get_from_gobject (object);

      while ((w = glade_widget_get_parent (w)))
        {
	  GObject *object = glade_widget_get_object (w);

          if (GTK_IS_TREE_VIEW (object))
            {
              glade_gtk_treeview_launch_editor (object);
              break;
            }
        }
    }
  else
    GWA_GET_CLASS (G_TYPE_OBJECT)->action_activate (adaptor, object, action_path);
}

void
glade_gtk_cell_renderer_deep_post_create (GladeWidgetAdaptor * adaptor,
                                          GObject * object,
                                          GladeCreateReason reason)
{
  GladePropertyClass *pclass;
  GladeProperty *property;
  GladeWidget *widget;
  const GList *l;

  widget = glade_widget_get_from_gobject (object);

  for (l = glade_widget_adaptor_get_properties (adaptor); l; l = l->next)
    {
      pclass = l->data;

      if (strncmp (glade_property_class_id (pclass), "use-attr-", strlen ("use-attr-")) == 0)
        {
          property = glade_widget_get_property (widget, glade_property_class_id (pclass));
          glade_property_sync (property);
        }
    }

  g_idle_add ((GSourceFunc) glade_gtk_cell_renderer_sync_attributes, object);
}

GladeEditorProperty *
glade_gtk_cell_renderer_create_eprop (GladeWidgetAdaptor * adaptor,
                                      GladePropertyClass * klass,
                                      gboolean use_command)
{
  GladeEditorProperty *eprop;

  if (strncmp (glade_property_class_id (klass), "attr-", strlen ("attr-")) == 0)
    eprop = g_object_new (GLADE_TYPE_EPROP_CELL_ATTRIBUTE,
                          "property-class", klass,
                          "use-command", use_command, NULL);
  else
    eprop = GWA_GET_CLASS
        (G_TYPE_OBJECT)->create_eprop (adaptor, klass, use_command);
  return eprop;
}


GladeEditable *
glade_gtk_cell_renderer_create_editable (GladeWidgetAdaptor * adaptor,
                                         GladeEditorPageType type)
{
  GladeEditable *editable;

  /* Get base editable */
  editable = GWA_GET_CLASS (G_TYPE_OBJECT)->create_editable (adaptor, type);

  if (type == GLADE_PAGE_GENERAL || type == GLADE_PAGE_COMMON)
    return (GladeEditable *) glade_cell_renderer_editor_new (adaptor, type,
                                                             editable);

  return editable;
}

static void
glade_gtk_cell_renderer_set_use_attribute (GObject * object,
                                           const gchar * property_name,
                                           const GValue * value)
{
  GladeWidget *widget = glade_widget_get_from_gobject (object);
  gchar *attr_prop_name, *prop_msg, *attr_msg;

  attr_prop_name = g_strdup_printf ("attr-%s", property_name);

  prop_msg = g_strdup_printf (_("%s is set to load %s from the model"),
                              glade_widget_get_name (widget), property_name);
  attr_msg = g_strdup_printf (_("%s is set to manipulate %s directly"),
                              glade_widget_get_name (widget), attr_prop_name);

  glade_widget_property_set_sensitive (widget, property_name, FALSE, prop_msg);
  glade_widget_property_set_sensitive (widget, attr_prop_name, FALSE, attr_msg);

  if (g_value_get_boolean (value))
    glade_widget_property_set_sensitive (widget, attr_prop_name, TRUE, NULL);
  else
    {
      GladeProperty *property =
          glade_widget_get_property (widget, property_name);

      glade_property_set_sensitive (property, TRUE, NULL);
      glade_property_sync (property);
    }

  g_free (prop_msg);
  g_free (attr_msg);
  g_free (attr_prop_name);
}

static GladeProperty *
glade_gtk_cell_renderer_attribute_switch (GladeWidget * gwidget,
                                          const gchar * property_name)
{
  GladeProperty *property;
  gchar *use_attr_name = g_strdup_printf ("use-attr-%s", property_name);

  property = glade_widget_get_property (gwidget, use_attr_name);
  g_free (use_attr_name);

  return property;
}

static gboolean
glade_gtk_cell_renderer_property_enabled (GObject * object,
                                          const gchar * property_name)
{
  GladeProperty *property;
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);
  gboolean use_attr = TRUE;

  if ((property =
       glade_gtk_cell_renderer_attribute_switch (gwidget,
                                                 property_name)) != NULL)
    glade_property_get (property, &use_attr);

  return !use_attr;
}


void
glade_gtk_cell_renderer_set_property (GladeWidgetAdaptor * adaptor,
                                      GObject * object,
                                      const gchar * property_name,
                                      const GValue * value)
{
  static gint use_attr_len = 0;
  static gint attr_len = 0;

  if (!attr_len)
    {
      use_attr_len = strlen ("use-attr-");
      attr_len = strlen ("attr-");
    }

  if (strncmp (property_name, "use-attr-", use_attr_len) == 0)
    glade_gtk_cell_renderer_set_use_attribute (object,
                                               &property_name[use_attr_len],
                                               value);
  else if (strncmp (property_name, "attr-", attr_len) == 0)
    glade_gtk_cell_renderer_sync_attributes (object);
  else if (glade_gtk_cell_renderer_property_enabled (object, property_name))
    /* Chain Up */
    GWA_GET_CLASS (G_TYPE_OBJECT)->set_property (adaptor,
                                                 object, property_name, value);
}

static void
glade_gtk_cell_renderer_write_properties (GladeWidget * widget,
                                          GladeXmlContext * context,
                                          GladeXmlNode * node)
{
  GladeProperty *property, *prop;
  GladePropertyClass *pclass;
  gchar *attr_name;
  GList *l;
  static gint attr_len = 0;

  if (!attr_len)
    attr_len = strlen ("attr-");

  for (l = glade_widget_get_properties (widget); l; l = l->next)
    {
      property = l->data;
      pclass   = glade_property_get_class (property);

      if (strncmp (glade_property_class_id (pclass), "attr-", attr_len) == 0)
        {
          gchar *use_attr_str;
          gboolean use_attr = FALSE;

          use_attr_str = g_strdup_printf ("use-%s", glade_property_class_id (pclass));
          glade_widget_property_get (widget, use_attr_str, &use_attr);

          attr_name = (gchar *)&glade_property_class_id (pclass)[attr_len];
          prop = glade_widget_get_property (widget, attr_name);

          if (!use_attr && prop)
	    {
	      /* Special case to write GtkCellRendererPixbuf:stock-size */
	      if (strcmp (attr_name, "stock-size") == 0)
		glade_gtk_write_icon_size (widget, context, node, "stock-size");
	      else
		glade_property_write (prop, context, node);
	    }

          g_free (use_attr_str);
        }
    }
}

void
glade_gtk_cell_renderer_write_widget (GladeWidgetAdaptor * adaptor,
                                      GladeWidget * widget,
                                      GladeXmlContext * context,
                                      GladeXmlNode * node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
	glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* Write our normal properties, then chain up to write any other normal properties,
   * then attributes 
   */
  glade_gtk_cell_renderer_write_properties (widget, context, node);

  GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);
}

static void
glade_gtk_cell_renderer_parse_finished (GladeProject * project,
                                        GladeWidget * widget)
{
  GladeProperty *property;
  GList *l;
  static gint attr_len = 0, use_attr_len = 0;

  /* Set "use-attr-*" everywhere that the object property is non-default 
   *
   * We do this in the finished handler because some properties may be
   * object type properties (which may be anywhere in the glade file).
   */
  if (!attr_len)
    {
      attr_len = strlen ("attr-");
      use_attr_len = strlen ("use-attr-");
    }

  for (l = glade_widget_get_properties (widget); l; l = l->next)
    {
      GladeProperty *switch_prop;
      GladePropertyClass *pclass;

      property = l->data;
      pclass   = glade_property_get_class (property);

      if (strncmp (glade_property_class_id (pclass), "attr-", attr_len) != 0 &&
          strncmp (glade_property_class_id (pclass), "use-attr-", use_attr_len) != 0 &&
          (switch_prop =
           glade_gtk_cell_renderer_attribute_switch (widget,
                                                     glade_property_class_id (pclass))) != NULL)
        {
          if (glade_property_original_default (property))
            glade_property_set (switch_prop, TRUE);
          else
            glade_property_set (switch_prop, FALSE);
        }
    }
}

void
glade_gtk_cell_renderer_read_widget (GladeWidgetAdaptor * adaptor,
                                     GladeWidget * widget, GladeXmlNode * node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
	glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the properties... */
  GWA_GET_CLASS (G_TYPE_OBJECT)->read_widget (adaptor, widget, node);

  g_signal_connect (glade_widget_get_project (widget), "parse-finished",
                    G_CALLBACK (glade_gtk_cell_renderer_parse_finished),
                    widget);
}

/*--------------------------- GtkCellLayout ---------------------------------*/
gboolean
glade_gtk_cell_layout_add_verify (GladeWidgetAdaptor *adaptor,
				  GtkWidget          *container,
				  GtkWidget          *child,
				  gboolean            user_feedback)
{
  if (!GTK_IS_CELL_RENDERER (child))
    {
      if (user_feedback)
	{
	  GladeWidgetAdaptor *cell_adaptor = 
	    glade_widget_adaptor_get_by_type (GTK_TYPE_CELL_RENDERER);

	  glade_util_ui_message (glade_app_get_window (),
				 GLADE_UI_INFO, NULL,
				 ONLY_THIS_GOES_IN_THAT_MSG,
				 glade_widget_adaptor_get_title (cell_adaptor),
				 glade_widget_adaptor_get_title (adaptor));
	}

      return FALSE;
    }

  return TRUE;
}

void
glade_gtk_cell_layout_add_child (GladeWidgetAdaptor * adaptor,
                                 GObject * container, GObject * child)
{
  GladeWidget *gmodel = NULL;
  GladeWidget *grenderer = glade_widget_get_from_gobject (child);

  if (GTK_IS_ICON_VIEW (container) &&
      (gmodel = glade_cell_renderer_get_model (grenderer)) != NULL)
    gtk_icon_view_set_model (GTK_ICON_VIEW (container), NULL);

  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (container),
                              GTK_CELL_RENDERER (child), TRUE);

  if (gmodel)
    gtk_icon_view_set_model (GTK_ICON_VIEW (container),
                             GTK_TREE_MODEL (glade_widget_get_object (gmodel)));

  glade_gtk_cell_renderer_sync_attributes (child);
}

void
glade_gtk_cell_layout_remove_child (GladeWidgetAdaptor * adaptor,
                                    GObject * container, GObject * child)
{
  GtkCellLayout *layout = GTK_CELL_LAYOUT (container);
  GList *l, *children = gtk_cell_layout_get_cells (layout);

  /* Add a reference to every cell except the one we want to remove */
  for (l = children; l; l = g_list_next (l))
    if (l->data != child)
      g_object_ref (l->data);
    else
      l->data = NULL;

  /* remove every cell */
  gtk_cell_layout_clear (layout);

  /* pack others cell renderers */
  for (l = children; l; l = g_list_next (l))
    {
      if (l->data == NULL)
        continue;

      gtk_cell_layout_pack_start (layout, GTK_CELL_RENDERER (l->data), TRUE);

      /* Remove our transient reference */
      g_object_unref (l->data);
    }

  g_list_free (children);
}

GList *
glade_gtk_cell_layout_get_children (GladeWidgetAdaptor * adaptor,
                                    GObject * container)
{
  return gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (container));
}


void
glade_gtk_cell_layout_get_child_property (GladeWidgetAdaptor * adaptor,
                                          GObject * container,
                                          GObject * child,
                                          const gchar * property_name,
                                          GValue * value)
{
  if (strcmp (property_name, "position") == 0)
    {
      GList *cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (container));

      /* We have to fake it, assume we are loading and always return the last item */
      g_value_set_int (value, g_list_length (cells) - 1);

      g_list_free (cells);
    }
  else
    /* Chain Up */
    GWA_GET_CLASS
        (GTK_TYPE_CONTAINER)->child_get_property (adaptor,
                                                  container, child,
                                                  property_name, value);
}

void
glade_gtk_cell_layout_set_child_property (GladeWidgetAdaptor * adaptor,
                                          GObject * container,
                                          GObject * child,
                                          const gchar * property_name,
                                          const GValue * value)
{
  if (strcmp (property_name, "position") == 0)
    {
      /* Need verify on position property !!! XXX */
      gtk_cell_layout_reorder (GTK_CELL_LAYOUT (container),
                               GTK_CELL_RENDERER (child),
                               g_value_get_int (value));
    }
  else
    /* Chain Up */
    GWA_GET_CLASS
        (GTK_TYPE_CONTAINER)->child_set_property (adaptor,
                                                  container, child,
                                                  property_name, value);
}

static void
glade_gtk_cell_renderer_read_attributes (GladeWidget * widget,
                                         GladeXmlNode * node)
{
  GladeXmlNode *attrs_node;
  GladeProperty *attr_prop, *use_attr_prop;
  GladeXmlNode *prop;
  gchar *name, *column_str, *attr_prop_name, *use_attr_name;

  if ((attrs_node =
       glade_xml_search_child (node, GLADE_TAG_ATTRIBUTES)) == NULL)
    return;

  for (prop = glade_xml_node_get_children (attrs_node); prop;
       prop = glade_xml_node_next (prop))
    {

      if (!glade_xml_node_verify_silent (prop, GLADE_TAG_ATTRIBUTE))
        continue;

      name =
          glade_xml_get_property_string_required (prop, GLADE_TAG_NAME, NULL);
      column_str = glade_xml_get_content (prop);
      attr_prop_name = g_strdup_printf ("attr-%s", name);
      use_attr_name = g_strdup_printf ("use-attr-%s", name);

      attr_prop = glade_widget_get_property (widget, attr_prop_name);
      use_attr_prop = glade_widget_get_property (widget, use_attr_name);

      if (attr_prop && use_attr_prop)
        {
          gboolean use_attribute = FALSE;
          glade_property_get (use_attr_prop, &use_attribute);

          if (use_attribute)
            glade_property_set (attr_prop,
                                g_ascii_strtoll (column_str, NULL, 10));
        }

      g_free (name);
      g_free (column_str);
      g_free (attr_prop_name);
      g_free (use_attr_name);

    }
}

void
glade_gtk_cell_layout_read_child (GladeWidgetAdaptor * adaptor,
                                  GladeWidget * widget, GladeXmlNode * node)
{
  GladeXmlNode *widget_node;
  GladeWidget *child_widget;
  gchar *internal_name;

  if (!glade_xml_node_verify (node, GLADE_XML_TAG_CHILD))
    return;

  internal_name =
      glade_xml_get_property_string (node, GLADE_XML_TAG_INTERNAL_CHILD);

  if ((widget_node =
       glade_xml_search_child (node, GLADE_XML_TAG_WIDGET)) != NULL)
    {

      /* Combo box is a special brand of cell-layout, it can also have the internal entry */
      if ((child_widget = glade_widget_read (glade_widget_get_project (widget),
                                             widget, widget_node,
                                             internal_name)) != NULL)
        {
          /* Dont set any packing properties on internal children here,
           * its possible but just not relevant for known celllayouts...
           * i.e. maybe GtkTreeViewColumn will expose the internal button ?
           * but no need for packing properties there either.
           */
          if (!internal_name)
            {
              glade_widget_add_child (widget, child_widget, FALSE);

              glade_gtk_cell_renderer_read_attributes (child_widget, node);

              g_idle_add ((GSourceFunc) glade_gtk_cell_renderer_sync_attributes,
                          glade_widget_get_object (child_widget));
            }
        }
    }
  g_free (internal_name);
}

static void
glade_gtk_cell_renderer_write_attributes (GladeWidget * widget,
                                          GladeXmlContext * context,
                                          GladeXmlNode * node)
{
  GladeProperty *property;
  GladePropertyClass *pclass;
  GladeXmlNode *attrs_node;
  gchar *attr_name;
  GList *l;
  static gint attr_len = 0;

  if (!attr_len)
    attr_len = strlen ("attr-");

  attrs_node = glade_xml_node_new (context, GLADE_TAG_ATTRIBUTES);

  for (l = glade_widget_get_properties (widget); l; l = l->next)
    {
      property = l->data;
      pclass   = glade_property_get_class (property);

      if (strncmp (glade_property_class_id (pclass), "attr-", attr_len) == 0)
        {
          GladeXmlNode *attr_node;
          gchar *column_str, *use_attr_str;
          gboolean use_attr = FALSE;

          use_attr_str = g_strdup_printf ("use-%s", glade_property_class_id (pclass));
          glade_widget_property_get (widget, use_attr_str, &use_attr);

          if (use_attr && g_value_get_int (glade_property_inline_value (property)) >= 0)
            {
              column_str =
                  g_strdup_printf ("%d", g_value_get_int (glade_property_inline_value (property)));
              attr_name = (gchar *)&glade_property_class_id (pclass)[attr_len];

              attr_node = glade_xml_node_new (context, GLADE_TAG_ATTRIBUTE);
              glade_xml_node_append_child (attrs_node, attr_node);
              glade_xml_node_set_property_string (attr_node, GLADE_TAG_NAME,
                                                  attr_name);
              glade_xml_set_content (attr_node, column_str);
              g_free (column_str);
            }
          g_free (use_attr_str);
        }
    }

  if (!glade_xml_node_get_children (attrs_node))
    glade_xml_node_delete (attrs_node);
  else
    glade_xml_node_append_child (node, attrs_node);
}

void
glade_gtk_cell_layout_write_child (GladeWidgetAdaptor * adaptor,
                                   GladeWidget * widget,
                                   GladeXmlContext * context,
                                   GladeXmlNode * node)
{
  GladeXmlNode *child_node;

  child_node = glade_xml_node_new (context, GLADE_XML_TAG_CHILD);
  glade_xml_node_append_child (node, child_node);

  /* ComboBox can have an internal entry */
  if (glade_widget_get_internal (widget))
    glade_xml_node_set_property_string (child_node,
                                        GLADE_XML_TAG_INTERNAL_CHILD,
                                        glade_widget_get_internal (widget));

  /* Write out the widget */
  glade_widget_write (widget, context, child_node);

  glade_gtk_cell_renderer_write_attributes (widget, context, child_node);
}

static gchar *
glade_gtk_cell_layout_get_display_name (GladeBaseEditor * editor,
                                        GladeWidget * gchild,
                                        gpointer user_data)
{
  GObject *child = glade_widget_get_object (gchild);
  gchar *name;

  if (GTK_IS_TREE_VIEW_COLUMN (child))
    glade_widget_property_get (gchild, "title", &name);
  else
    name = (gchar *)glade_widget_get_name (gchild);

  return g_strdup (name);
}

static void
glade_gtk_cell_layout_child_selected (GladeBaseEditor * editor,
                                      GladeWidget * gchild, gpointer data)
{
  GObject *child = glade_widget_get_object (gchild);

  glade_base_editor_add_label (editor, GTK_IS_TREE_VIEW_COLUMN (child) ?
                               _("Tree View Column") : _("Cell Renderer"));

  glade_base_editor_add_default_properties (editor, gchild);

  glade_base_editor_add_label (editor, GTK_IS_TREE_VIEW_COLUMN (child) ?
                               _("Properties") :
                               _("Properties and Attributes"));
  glade_base_editor_add_editable (editor, gchild, GLADE_PAGE_GENERAL);

  if (GTK_IS_CELL_RENDERER (child))
    {
      glade_base_editor_add_label (editor,
                                   _("Common Properties and Attributes"));
      glade_base_editor_add_editable (editor, gchild, GLADE_PAGE_COMMON);
    }
}

static gboolean
glade_gtk_cell_layout_move_child (GladeBaseEditor * editor,
                                  GladeWidget * gparent,
                                  GladeWidget * gchild, gpointer data)
{
  GObject *parent = glade_widget_get_object (gparent);
  GObject *child = glade_widget_get_object (gchild);
  GList list = { 0, };

  if (GTK_IS_TREE_VIEW (parent) && !GTK_IS_TREE_VIEW_COLUMN (child))
    return FALSE;
  if (GTK_IS_CELL_LAYOUT (parent) && !GTK_IS_CELL_RENDERER (child))
    return FALSE;
  if (GTK_IS_CELL_RENDERER (parent))
    return FALSE;

  if (gparent != glade_widget_get_parent (gchild))
    {
      list.data = gchild;
      glade_command_dnd (&list, gparent, NULL);
    }

  return TRUE;
}

static void
glade_gtk_cell_layout_launch_editor (GObject *layout, gchar *window_name)
{
  GladeWidget        *widget  = glade_widget_get_from_gobject (layout);
  GladeWidgetAdaptor *adaptor = glade_widget_get_adaptor (widget);
  GladeBaseEditor    *editor;
  GladeEditable      *layout_editor;
  GtkWidget          *window;

  layout_editor = glade_widget_adaptor_create_editable (adaptor, GLADE_PAGE_GENERAL);
  layout_editor = (GladeEditable *) glade_tree_view_editor_new (adaptor, layout_editor);

  /* Editor */
  editor = glade_base_editor_new (layout, layout_editor,
                                  _("Text"), GTK_TYPE_CELL_RENDERER_TEXT,
                                  _("Accelerator"), GTK_TYPE_CELL_RENDERER_ACCEL,
                                  _("Combo"), GTK_TYPE_CELL_RENDERER_COMBO,
                                  _("Spin"), GTK_TYPE_CELL_RENDERER_SPIN,
                                  _("Pixbuf"), GTK_TYPE_CELL_RENDERER_PIXBUF,
                                  _("Progress"), GTK_TYPE_CELL_RENDERER_PROGRESS,
                                  _("Toggle"), GTK_TYPE_CELL_RENDERER_TOGGLE,
                                  _("Spinner"), GTK_TYPE_CELL_RENDERER_SPINNER,
                                  NULL);

  g_signal_connect (editor, "get-display-name",
                    G_CALLBACK (glade_gtk_cell_layout_get_display_name), NULL);
  g_signal_connect (editor, "child-selected",
                    G_CALLBACK (glade_gtk_cell_layout_child_selected), NULL);
  g_signal_connect (editor, "move-child",
                    G_CALLBACK (glade_gtk_cell_layout_move_child), NULL);

  gtk_widget_show (GTK_WIDGET (editor));

  window = glade_base_editor_pack_new_window (editor, window_name, NULL);
  gtk_widget_show (window);
}


static void
glade_gtk_cell_layout_launch_editor_action (GObject * object)
{
  GladeWidget *w = glade_widget_get_from_gobject (object);

  do
    {
      GObject *obj = glade_widget_get_object (w);

      if (GTK_IS_TREE_VIEW (obj))
        {
          glade_gtk_treeview_launch_editor (obj);
          break;
        }
      else if (GTK_IS_ICON_VIEW (obj))
        {
          glade_gtk_cell_layout_launch_editor (obj, _("Icon View Editor"));
          break;
        }
      else if (GTK_IS_COMBO_BOX (obj))
        {
          glade_gtk_cell_layout_launch_editor (obj, _("Combo Editor"));
          break;
        }
      else if (GTK_IS_ENTRY_COMPLETION (obj))
        {
          glade_gtk_cell_layout_launch_editor (obj, _("Entry Completion Editor"));
          break;
        }
    }
  while ((w = glade_widget_get_parent (w)));
}

void
glade_gtk_cell_layout_action_activate (GladeWidgetAdaptor * adaptor,
                                       GObject * object,
                                       const gchar * action_path)
{
  if (strcmp (action_path, "launch_editor") == 0)
    glade_gtk_cell_layout_launch_editor_action (object);
  else
    GWA_GET_CLASS (G_TYPE_OBJECT)->action_activate (adaptor,
                                                    object, action_path);
}

void
glade_gtk_cell_layout_action_activate_as_widget (GladeWidgetAdaptor * adaptor,
                                                 GObject * object,
                                                 const gchar * action_path)
{
  if (strcmp (action_path, "launch_editor") == 0)
    glade_gtk_cell_layout_launch_editor_action (object);
  else
    GWA_GET_CLASS (GTK_TYPE_WIDGET)->action_activate (adaptor,
                                                      object, action_path);
}



/*--------------------------- GtkTreeView ---------------------------------*/

gboolean
glade_gtk_treeview_add_verify (GladeWidgetAdaptor *adaptor,
			       GtkWidget          *container,
			       GtkWidget          *child,
			       gboolean            user_feedback)
{
  if (!GTK_IS_TREE_VIEW_COLUMN (child))
    {
      if (user_feedback)
	{
	  GladeWidgetAdaptor *cell_adaptor = 
	    glade_widget_adaptor_get_by_type (GTK_TYPE_TREE_VIEW_COLUMN);

	  glade_util_ui_message (glade_app_get_window (),
				 GLADE_UI_INFO, NULL,
				 ONLY_THIS_GOES_IN_THAT_MSG,
				 glade_widget_adaptor_get_title (cell_adaptor),
				 glade_widget_adaptor_get_title (adaptor));
	}

      return FALSE;
    }

  return TRUE;
}

static void
glade_gtk_treeview_launch_editor (GObject * treeview)
{
  GladeWidget        *widget  = glade_widget_get_from_gobject (treeview);
  GladeWidgetAdaptor *adaptor = glade_widget_get_adaptor (widget);
  GladeBaseEditor    *editor;
  GladeEditable      *treeview_editor;
  GtkWidget          *window;

  treeview_editor = glade_widget_adaptor_create_editable (adaptor, GLADE_PAGE_GENERAL);
  treeview_editor = (GladeEditable *) glade_tree_view_editor_new (adaptor, treeview_editor);

  /* Editor */
  editor = glade_base_editor_new (treeview, treeview_editor,
                                  _("Column"), GTK_TYPE_TREE_VIEW_COLUMN, NULL);

  glade_base_editor_append_types (editor, GTK_TYPE_TREE_VIEW_COLUMN,
                                  _("Text"), GTK_TYPE_CELL_RENDERER_TEXT,
                                  _("Accelerator"),
                                  GTK_TYPE_CELL_RENDERER_ACCEL, _("Combo"),
                                  GTK_TYPE_CELL_RENDERER_COMBO, _("Spin"),
                                  GTK_TYPE_CELL_RENDERER_SPIN, _("Pixbuf"),
                                  GTK_TYPE_CELL_RENDERER_PIXBUF, _("Progress"),
                                  GTK_TYPE_CELL_RENDERER_PROGRESS, _("Toggle"),
                                  GTK_TYPE_CELL_RENDERER_TOGGLE, _("Spinner"),
                                  GTK_TYPE_CELL_RENDERER_SPINNER, NULL);

  g_signal_connect (editor, "get-display-name",
                    G_CALLBACK (glade_gtk_cell_layout_get_display_name), NULL);
  g_signal_connect (editor, "child-selected",
                    G_CALLBACK (glade_gtk_cell_layout_child_selected), NULL);
  g_signal_connect (editor, "move-child",
                    G_CALLBACK (glade_gtk_cell_layout_move_child), NULL);

  gtk_widget_show (GTK_WIDGET (editor));

  window =
      glade_base_editor_pack_new_window (editor, _("Tree View Editor"), NULL);
  gtk_widget_show (window);
}

void
glade_gtk_treeview_action_activate (GladeWidgetAdaptor * adaptor,
                                    GObject * object, const gchar * action_path)
{
  if (strcmp (action_path, "launch_editor") == 0)
    {
      glade_gtk_treeview_launch_editor (object);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->action_activate (adaptor,
                                                         object, action_path);
}

static gint
glade_gtk_treeview_get_column_index (GtkTreeView * view,
                                     GtkTreeViewColumn * column)
{
  GtkTreeViewColumn *iter;
  gint i;

  for (i = 0; (iter = gtk_tree_view_get_column (view, i)) != NULL; i++)
    if (iter == column)
      return i;

  return -1;
}

void
glade_gtk_treeview_get_child_property (GladeWidgetAdaptor * adaptor,
                                       GObject * container,
                                       GObject * child,
                                       const gchar * property_name,
                                       GValue * value)
{
  if (strcmp (property_name, "position") == 0)
    g_value_set_int (value,
                     glade_gtk_treeview_get_column_index (GTK_TREE_VIEW
                                                          (container),
                                                          GTK_TREE_VIEW_COLUMN
                                                          (child)));
  else
    /* Chain Up */
    GWA_GET_CLASS
        (GTK_TYPE_CONTAINER)->child_get_property (adaptor,
                                                  container, child,
                                                  property_name, value);
}

void
glade_gtk_treeview_set_child_property (GladeWidgetAdaptor * adaptor,
                                       GObject * container,
                                       GObject * child,
                                       const gchar * property_name,
                                       const GValue * value)
{
  if (strcmp (property_name, "position") == 0)
    {

      gtk_tree_view_remove_column (GTK_TREE_VIEW (container),
                                   GTK_TREE_VIEW_COLUMN (child));
      gtk_tree_view_insert_column (GTK_TREE_VIEW (container),
                                   GTK_TREE_VIEW_COLUMN (child),
                                   g_value_get_int (value));
    }
  else
    /* Chain Up */
    GWA_GET_CLASS
        (GTK_TYPE_CONTAINER)->child_set_property (adaptor,
                                                  container, child,
                                                  property_name, value);
}

GList *
glade_gtk_treeview_get_children (GladeWidgetAdaptor * adaptor,
                                 GtkTreeView * view)
{
  GList *children;

  children = gtk_tree_view_get_columns (view);
  children = g_list_prepend (children, gtk_tree_view_get_selection (view));

  return children;
}

/* XXX FIXME: We should hide the actual "fixed-height-mode" setting from
 * treeview editors and provide a custom control that sets all its columns
 * to fixed size and then control the column's sensitivity accordingly.
 */
#define INSENSITIVE_COLUMN_SIZING_MSG \
	_("Columns must have a fixed size inside a treeview with fixed height mode set")

void
glade_gtk_treeview_add_child (GladeWidgetAdaptor * adaptor,
                              GObject * container, GObject * child)
{
  GtkTreeView *view = GTK_TREE_VIEW (container);
  GtkTreeViewColumn *column;
  GladeWidget *gcolumn;

  if (!GTK_IS_TREE_VIEW_COLUMN (child))
    return;

  if (gtk_tree_view_get_fixed_height_mode (view))
    {
      gcolumn = glade_widget_get_from_gobject (child);
      glade_widget_property_set (gcolumn, "sizing", GTK_TREE_VIEW_COLUMN_FIXED);
      glade_widget_property_set_sensitive (gcolumn, "sizing", FALSE,
                                           INSENSITIVE_COLUMN_SIZING_MSG);
    }

  column = GTK_TREE_VIEW_COLUMN (child);
  gtk_tree_view_append_column (view, column);

  glade_gtk_cell_layout_sync_attributes (G_OBJECT (column));
}

void
glade_gtk_treeview_remove_child (GladeWidgetAdaptor * adaptor,
                                 GObject * container, GObject * child)
{
  GtkTreeView *view = GTK_TREE_VIEW (container);
  GtkTreeViewColumn *column;

  if (!GTK_IS_TREE_VIEW_COLUMN (child))
    return;

  column = GTK_TREE_VIEW_COLUMN (child);
  gtk_tree_view_remove_column (view, column);
}

void
glade_gtk_treeview_replace_child (GladeWidgetAdaptor * adaptor,
                                  GObject * container,
                                  GObject * current, GObject * new_column)
{
  GtkTreeView *view = GTK_TREE_VIEW (container);
  GList *columns;
  GtkTreeViewColumn *column;
  GladeWidget *gcolumn;
  gint index;

  if (!GTK_IS_TREE_VIEW_COLUMN (current))
    return;

  column = GTK_TREE_VIEW_COLUMN (current);

  columns = gtk_tree_view_get_columns (view);
  index = g_list_index (columns, column);
  g_list_free (columns);

  gtk_tree_view_remove_column (view, column);
  column = GTK_TREE_VIEW_COLUMN (new_column);

  gtk_tree_view_insert_column (view, column, index);

  if (gtk_tree_view_get_fixed_height_mode (view))
    {
      gcolumn = glade_widget_get_from_gobject (column);
      glade_widget_property_set (gcolumn, "sizing", GTK_TREE_VIEW_COLUMN_FIXED);
      glade_widget_property_set_sensitive (gcolumn, "sizing", FALSE,
                                           INSENSITIVE_COLUMN_SIZING_MSG);
    }

  glade_gtk_cell_layout_sync_attributes (G_OBJECT (column));
}

gboolean
glade_gtk_treeview_depends (GladeWidgetAdaptor * adaptor,
                            GladeWidget * widget, GladeWidget * another)
{
  if (GTK_IS_TREE_MODEL (glade_widget_get_object (another)))
    return TRUE;

  return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->depends (adaptor, widget, another);
}

/*--------------------------- GtkAdjustment ---------------------------------*/
void
glade_gtk_adjustment_write_widget (GladeWidgetAdaptor * adaptor,
                                   GladeWidget * widget,
                                   GladeXmlContext * context,
                                   GladeXmlNode * node)
{
  GladeProperty *prop;

  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
	glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* Ensure proper order of adjustment properties by writing them here. */
  prop = glade_widget_get_property (widget, "lower");
  glade_property_write (prop, context, node);

  prop = glade_widget_get_property (widget, "upper");
  glade_property_write (prop, context, node);

  prop = glade_widget_get_property (widget, "value");
  glade_property_write (prop, context, node);

  GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);
}


/*--------------------------- GtkAction ---------------------------------*/
#define ACTION_ACCEL_INSENSITIVE_MSG _("The accelerator can only be set when inside an Action Group.")

void
glade_gtk_action_post_create (GladeWidgetAdaptor * adaptor,
                              GObject * object, GladeCreateReason reason)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);

  if (reason == GLADE_CREATE_REBUILD)
    return;

  if (!gtk_action_get_name (GTK_ACTION (object)))
    glade_widget_property_set (gwidget, "name", "untitled");

  glade_widget_set_action_sensitive (gwidget, "launch_editor", FALSE);
  glade_widget_property_set_sensitive (gwidget, "accelerator", FALSE, 
				       ACTION_ACCEL_INSENSITIVE_MSG);
}

/*--------------------------- GtkActionGroup ---------------------------------*/
gboolean
glade_gtk_action_group_add_verify (GladeWidgetAdaptor *adaptor,
				   GtkWidget          *container,
				   GtkWidget          *child,
				   gboolean            user_feedback)
{
  if (!GTK_IS_ACTION (child))
    {
      if (user_feedback)
	{
	  GladeWidgetAdaptor *action_adaptor = 
	    glade_widget_adaptor_get_by_type (GTK_TYPE_ACTION);

	  glade_util_ui_message (glade_app_get_window (),
				 GLADE_UI_INFO, NULL,
				 ONLY_THIS_GOES_IN_THAT_MSG,
				 glade_widget_adaptor_get_title (action_adaptor),
				 glade_widget_adaptor_get_title (adaptor));
	}

      return FALSE;
    }

  return TRUE;
}

void
glade_gtk_action_group_add_child (GladeWidgetAdaptor * adaptor,
                                  GObject * container, GObject * child)
{
  if (GTK_IS_ACTION (child))
    {
      /* Dont really add/remove actions (because name conflicts inside groups)
       */
      GladeWidget *ggroup = glade_widget_get_from_gobject (container);
      GladeWidget *gaction = glade_widget_get_from_gobject (child);
      GList *actions = g_object_get_data (G_OBJECT (ggroup), "glade-actions");

      actions = g_list_copy (actions);
      actions = g_list_append (actions, child);

      g_object_set_data_full (G_OBJECT (ggroup), "glade-actions", actions,
                              (GDestroyNotify) g_list_free);

      glade_widget_property_set_sensitive (gaction, "accelerator", TRUE, NULL);
      glade_widget_set_action_sensitive (gaction, "launch_editor", TRUE);
    }
}

void
glade_gtk_action_group_remove_child (GladeWidgetAdaptor * adaptor,
                                     GObject * container, GObject * child)
{
  if (GTK_IS_ACTION (child))
    {
      /* Dont really add/remove actions (because name conflicts inside groups)
       */
      GladeWidget *ggroup = glade_widget_get_from_gobject (container);
      GladeWidget *gaction = glade_widget_get_from_gobject (child);
      GList *actions = g_object_get_data (G_OBJECT (ggroup), "glade-actions");

      actions = g_list_copy (actions);
      actions = g_list_remove (actions, child);

      g_object_set_data_full (G_OBJECT (ggroup), "glade-actions", actions,
                              (GDestroyNotify) g_list_free);
      
      glade_widget_property_set_sensitive (gaction, "accelerator", FALSE, 
					   ACTION_ACCEL_INSENSITIVE_MSG);
      glade_widget_set_action_sensitive (gaction, "launch_editor", FALSE);
    }
}

void
glade_gtk_action_group_replace_child (GladeWidgetAdaptor * adaptor,
                                      GObject * container,
                                      GObject * current, GObject * new_action)
{
  glade_gtk_action_group_remove_child (adaptor, container, current);
  glade_gtk_action_group_add_child (adaptor, container, new_action);
}

GList *
glade_gtk_action_group_get_children (GladeWidgetAdaptor * adaptor,
                                     GObject * container)
{
  GladeWidget *ggroup = glade_widget_get_from_gobject (container);
  GList *actions = g_object_get_data (G_OBJECT (ggroup), "glade-actions");

  return g_list_copy (actions);
}


void
glade_gtk_action_group_read_child (GladeWidgetAdaptor * adaptor,
                                   GladeWidget * widget, GladeXmlNode * node)
{
  GladeXmlNode *widget_node;
  GladeWidget *child_widget;

  if (!glade_xml_node_verify (node, GLADE_XML_TAG_CHILD))
    return;

  if ((widget_node =
       glade_xml_search_child (node, GLADE_XML_TAG_WIDGET)) != NULL)
    {
      if ((child_widget = glade_widget_read (glade_widget_get_project (widget),
                                             widget, widget_node,
                                             NULL)) != NULL)
        {
          glade_widget_add_child (widget, child_widget, FALSE);

          /* Read in accelerator */
          glade_gtk_read_accels (child_widget, node, FALSE);
        }
    }
}


void
glade_gtk_action_group_write_child (GladeWidgetAdaptor * adaptor,
                                    GladeWidget * widget,
                                    GladeXmlContext * context,
                                    GladeXmlNode * node)
{
  GladeXmlNode *child_node;

  child_node = glade_xml_node_new (context, GLADE_XML_TAG_CHILD);
  glade_xml_node_append_child (node, child_node);

  /* Write out the widget */
  glade_widget_write (widget, context, child_node);

  /* Write accelerator here  */
  glade_gtk_write_accels (widget, context, child_node, FALSE);
}

static void
glade_gtk_action_child_selected (GladeBaseEditor *editor,
				 GladeWidget *gchild,
				 gpointer data)
{
  glade_base_editor_add_label (editor, _("Action"));
	
  glade_base_editor_add_default_properties (editor, gchild);
	
  glade_base_editor_add_label (editor, _("Properties"));
  glade_base_editor_add_editable (editor, gchild, GLADE_PAGE_GENERAL);
}

static gboolean
glade_gtk_action_move_child (GladeBaseEditor *editor,
			     GladeWidget *gparent,
			     GladeWidget *gchild,
			     gpointer data)
{	
  return FALSE;
}

static void
glade_gtk_action_launch_editor (GObject  *action)
{
  GladeWidget        *widget  = glade_widget_get_from_gobject (action);
  GladeWidgetAdaptor *adaptor = glade_widget_get_adaptor (widget);
  GladeBaseEditor    *editor;
  GladeEditable      *action_editor;
  GtkWidget          *window;

  /* Make sure we get the group here */
  widget = glade_widget_get_toplevel (widget);

  action_editor = glade_widget_adaptor_create_editable (adaptor, GLADE_PAGE_GENERAL);

  /* Editor */
  editor = glade_base_editor_new (glade_widget_get_object (widget), action_editor,
				  _("Action"), GTK_TYPE_ACTION,
				  _("Toggle"), GTK_TYPE_TOGGLE_ACTION,
				  _("Radio"), GTK_TYPE_RADIO_ACTION,
				  _("Recent"), GTK_TYPE_RECENT_ACTION,
				  NULL);

  g_signal_connect (editor, "child-selected", G_CALLBACK (glade_gtk_action_child_selected), NULL);
  g_signal_connect (editor, "move-child", G_CALLBACK (glade_gtk_action_move_child), NULL);

  gtk_widget_show (GTK_WIDGET (editor));
	
  window = glade_base_editor_pack_new_window (editor, _("Action Group Editor"), NULL);
  gtk_widget_show (window);
}


void
glade_gtk_action_action_activate (GladeWidgetAdaptor *adaptor,
				  GObject *object,
				  const gchar *action_path)
{
  if (strcmp (action_path, "launch_editor") == 0)
    {
      glade_gtk_action_launch_editor (object);
    }
}


/*--------------------------- GtkTextTagTable ---------------------------------*/
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
glade_gtk_text_tag_table_add_child (GladeWidgetAdaptor * adaptor,
				    GObject * container, GObject * child)
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
glade_gtk_text_tag_table_remove_child (GladeWidgetAdaptor * adaptor,
				       GObject * container, GObject * child)
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
glade_gtk_text_tag_table_replace_child (GladeWidgetAdaptor * adaptor,
					GObject * container,
					GObject * current, GObject * new_tag)
{
  glade_gtk_text_tag_table_remove_child (adaptor, container, current);
  glade_gtk_text_tag_table_add_child (adaptor, container, new_tag);
}

GList *
glade_gtk_text_tag_table_get_children (GladeWidgetAdaptor * adaptor,
				       GObject * container)
{
  GladeWidget *gtable = glade_widget_get_from_gobject (container);
  GList *tags = g_object_get_data (G_OBJECT (gtable), "glade-tags");

  return g_list_copy (tags);
}

static void
glade_gtk_text_tag_table_child_selected (GladeBaseEditor *editor,
					 GladeWidget *gchild,
					 gpointer data)
{
  glade_base_editor_add_label (editor, _("Tag"));
	
  glade_base_editor_add_default_properties (editor, gchild);
	
  glade_base_editor_add_label (editor, _("Properties"));
  glade_base_editor_add_editable (editor, gchild, GLADE_PAGE_GENERAL);
}

static gboolean
glade_gtk_text_tag_table_move_child (GladeBaseEditor *editor,
				     GladeWidget *gparent,
				     GladeWidget *gchild,
				     gpointer data)
{	
  return FALSE;
}

static void
glade_gtk_text_tag_table_launch_editor (GObject  *table)
{
  GladeWidget        *widget  = glade_widget_get_from_gobject (table);
  GladeWidgetAdaptor *adaptor = glade_widget_get_adaptor (widget);
  GladeBaseEditor    *editor;
  GladeEditable      *action_editor;
  GtkWidget          *window;

  action_editor = glade_widget_adaptor_create_editable (adaptor, GLADE_PAGE_GENERAL);

  /* Editor */
  editor = glade_base_editor_new (glade_widget_get_object (widget), action_editor,
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
					  GObject *object,
					  const gchar *action_path)
{
  if (strcmp (action_path, "launch_editor") == 0)
    {
      glade_gtk_text_tag_table_launch_editor (object);
    }
}


/*--------------------------- GtkRecentFilter ---------------------------------*/
#define GLADE_TAG_PATTERNS     "patterns"
#define GLADE_TAG_PATTERN      "pattern"
#define GLADE_TAG_MIME_TYPES   "mime-types"
#define GLADE_TAG_MIME_TYPE    "mime-type"
#define GLADE_TAG_APPLICATIONS "applications"
#define GLADE_TAG_APPLICATION  "application"

typedef enum {
  FILTER_PATTERN,
  FILTER_MIME,
  FILTER_APPLICATION
} FilterType;


static void
glade_gtk_filter_read_strings (GladeWidget  *widget,
			       GladeXmlNode *node,
			       FilterType    type,
			       const gchar  *property_name)
{
  GladeXmlNode *items_node;
  GladeXmlNode *item_node;
  GList        *string_list = NULL;
  const gchar  *string_group_tag;
  const gchar  *string_tag;

  switch (type)
    {
    case FILTER_PATTERN:
      string_group_tag = GLADE_TAG_PATTERNS;
      string_tag       = GLADE_TAG_PATTERN;
      break;
    case FILTER_MIME:
      string_group_tag = GLADE_TAG_MIME_TYPES;
      string_tag       = GLADE_TAG_MIME_TYPE;
      break;
    case FILTER_APPLICATION:
      string_group_tag = GLADE_TAG_APPLICATIONS;
      string_tag       = GLADE_TAG_APPLICATION;
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  if ((items_node =
       glade_xml_search_child (node, string_group_tag)) != NULL)
    {

      for (item_node = glade_xml_node_get_children (items_node);
	   item_node; item_node = glade_xml_node_next (item_node))
	{
	  gchar *str;

	  if (!glade_xml_node_verify (item_node, string_tag))
	    continue;

          if ((str = glade_xml_get_content (item_node)) == NULL)
	    continue;

	  string_list = glade_string_list_append (string_list, str, NULL, NULL, FALSE);
	  g_free (str);
	}

      glade_widget_property_set (widget, property_name, string_list);
      glade_string_list_free (string_list);
    }
}

static void
glade_gtk_filter_write_strings (GladeWidget     *widget,
				GladeXmlContext *context,
				GladeXmlNode    *node,
				FilterType       type,
				const gchar     *property_name)
{
  GladeXmlNode *item_node;
  GList        *string_list = NULL, *l;
  GladeString  *string;
  const gchar  *string_tag;

  switch (type)
    {
    case FILTER_PATTERN:     string_tag = GLADE_TAG_PATTERN;     break;
    case FILTER_MIME:        string_tag = GLADE_TAG_MIME_TYPE;   break;
    case FILTER_APPLICATION: string_tag = GLADE_TAG_APPLICATION; break;
    default:
      g_assert_not_reached ();
      break;
    }

  if (!glade_widget_property_get (widget, property_name, &string_list) || !string_list)
    return;

  for (l = string_list; l; l = l->next)
    {
      string = l->data;

      item_node = glade_xml_node_new (context, string_tag);
      glade_xml_node_append_child (node, item_node);

      glade_xml_set_content (item_node, string->string);
    }
}

GladeEditorProperty *
glade_gtk_recent_file_filter_create_eprop (GladeWidgetAdaptor * adaptor,
					   GladePropertyClass * klass, 
					   gboolean use_command)
{
  GladeEditorProperty *eprop;
  GParamSpec          *pspec;

  pspec = glade_property_class_get_pspec (klass);

  if (pspec->value_type == GLADE_TYPE_STRING_LIST)
    {
      eprop = glade_eprop_string_list_new (klass, use_command, FALSE);
    }
  else
    eprop = GWA_GET_CLASS
        (G_TYPE_OBJECT)->create_eprop (adaptor, klass, use_command);

  return eprop;
}

gchar *
glade_gtk_recent_file_filter_string_from_value (GladeWidgetAdaptor * adaptor,
						GladePropertyClass * klass,
						const GValue * value)
{
  GParamSpec *pspec;

  pspec = glade_property_class_get_pspec (klass);

  if (pspec->value_type == GLADE_TYPE_STRING_LIST)
    {
      GList *list = g_value_get_boxed (value);

      return glade_string_list_to_string (list);
    }
  else
    return GWA_GET_CLASS
        (G_TYPE_OBJECT)->string_from_value (adaptor, klass, value);
}

void
glade_gtk_recent_filter_read_widget (GladeWidgetAdaptor *adaptor,
				     GladeWidget        *widget, 
				     GladeXmlNode       *node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
	glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->read_widget (adaptor, widget, node);

  glade_gtk_filter_read_strings (widget, node, FILTER_MIME, "glade-mime-types");
  glade_gtk_filter_read_strings (widget, node, FILTER_PATTERN, "glade-patterns");
  glade_gtk_filter_read_strings (widget, node, FILTER_APPLICATION, "glade-applications");
}

void
glade_gtk_recent_filter_write_widget (GladeWidgetAdaptor *adaptor,
				      GladeWidget        *widget,
				      GladeXmlContext    *context, 
				      GladeXmlNode       *node)
{
  GladeXmlNode *strings_node;

  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
	glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);

  strings_node = glade_xml_node_new (context, GLADE_TAG_MIME_TYPES);
  glade_gtk_filter_write_strings (widget, context, strings_node, FILTER_MIME, "glade-mime-types");

  if (!glade_xml_node_get_children (strings_node))
    glade_xml_node_delete (strings_node);
  else
    glade_xml_node_append_child (node, strings_node);


  strings_node = glade_xml_node_new (context, GLADE_TAG_PATTERNS);
  glade_gtk_filter_write_strings (widget, context, strings_node, FILTER_PATTERN, "glade-patterns");

  if (!glade_xml_node_get_children (strings_node))
    glade_xml_node_delete (strings_node);
  else
    glade_xml_node_append_child (node, strings_node);

  strings_node = glade_xml_node_new (context, GLADE_TAG_APPLICATIONS);
  glade_gtk_filter_write_strings (widget, context, strings_node, 
				  FILTER_APPLICATION, "glade-applications");

  if (!glade_xml_node_get_children (strings_node))
    glade_xml_node_delete (strings_node);
  else
    glade_xml_node_append_child (node, strings_node);
}

/*--------------------------- GtkFileFilter ---------------------------------*/
void
glade_gtk_file_filter_read_widget (GladeWidgetAdaptor *adaptor,
				   GladeWidget        *widget, 
				   GladeXmlNode       *node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
	glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->read_widget (adaptor, widget, node);

  glade_gtk_filter_read_strings (widget, node, FILTER_MIME, "glade-mime-types");
  glade_gtk_filter_read_strings (widget, node, FILTER_PATTERN, "glade-patterns");
}

void
glade_gtk_file_filter_write_widget (GladeWidgetAdaptor *adaptor,
				    GladeWidget        *widget,
				    GladeXmlContext    *context, 
				    GladeXmlNode       *node)
{
  GladeXmlNode *strings_node;

  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
	glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);

  strings_node = glade_xml_node_new (context, GLADE_TAG_MIME_TYPES);
  glade_gtk_filter_write_strings (widget, context, strings_node, FILTER_MIME, "glade-mime-types");

  if (!glade_xml_node_get_children (strings_node))
    glade_xml_node_delete (strings_node);
  else
    glade_xml_node_append_child (node, strings_node);


  strings_node = glade_xml_node_new (context, GLADE_TAG_PATTERNS);
  glade_gtk_filter_write_strings (widget, context, strings_node, FILTER_PATTERN, "glade-patterns");

  if (!glade_xml_node_get_children (strings_node))
    glade_xml_node_delete (strings_node);
  else
    glade_xml_node_append_child (node, strings_node);
}
