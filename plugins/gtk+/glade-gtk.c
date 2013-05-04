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
