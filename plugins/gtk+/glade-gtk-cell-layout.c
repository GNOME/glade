/*
 * glade-gtk-cell-layout.c - GladeWidgetAdaptor for GtkCellLayout
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
#include "glade-gtk-cell-renderer.h"
#include "glade-gtk-tree-view.h"
#include "glade-cell-renderer-editor.h"
#include "glade-treeview-editor.h"

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
                                (gint)g_ascii_strtoll (column_str, NULL, 10));
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

gchar *
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

void
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

gboolean
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

gboolean
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
