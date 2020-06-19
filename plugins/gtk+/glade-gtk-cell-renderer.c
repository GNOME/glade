/*
 * glade-gtk-cell-renderer.c - GladeWidgetAdaptor for GtkCellRenderer
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

#include "glade-gtk-image.h" /* For GtkIconSize serialization */
#include "glade-gtk-tree-view.h"
#include "glade-gtk-cell-renderer.h"
#include "glade-cell-renderer-editor.h"
#include "glade-column-types.h"

void
glade_gtk_cell_renderer_action_activate (GladeWidgetAdaptor *adaptor,
                                         GObject            *object,
                                         const gchar        *action_path)
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
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (G_TYPE_OBJECT)->action_activate (adaptor, object, action_path);
}

void
glade_gtk_cell_renderer_deep_post_create (GladeWidgetAdaptor *adaptor,
                                          GObject            *object,
                                          GladeCreateReason   reason)
{
  GladePropertyDef *pdef;
  GladeProperty *property;
  GladeWidget *widget;
  const GList *l;

  widget = glade_widget_get_from_gobject (object);

  for (l = glade_widget_adaptor_get_properties (adaptor); l; l = l->next)
    {
      pdef = l->data;

      if (strncmp (glade_property_def_id (pdef), "use-attr-", strlen ("use-attr-")) == 0)
        {
          property = glade_widget_get_property (widget, glade_property_def_id (pdef));
          glade_property_sync (property);
        }
    }

  g_idle_add ((GSourceFunc) glade_gtk_cell_renderer_sync_attributes, object);
}

GladeEditorProperty *
glade_gtk_cell_renderer_create_eprop (GladeWidgetAdaptor *adaptor,
                                      GladePropertyDef   *def,
                                      gboolean            use_command)
{
  GladeEditorProperty *eprop;

  if (strncmp (glade_property_def_id (def), "attr-", strlen ("attr-")) == 0)
    eprop = g_object_new (GLADE_TYPE_EPROP_CELL_ATTRIBUTE,
                          "property-def", def,
                          "use-command", use_command, NULL);
  else
    eprop = GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS
        (G_TYPE_OBJECT)->create_eprop (adaptor, def, use_command);
  return eprop;
}


GladeEditable *
glade_gtk_cell_renderer_create_editable (GladeWidgetAdaptor *adaptor,
                                         GladeEditorPageType type)
{
  GladeEditable *editable;

  /* Get base editable */
  editable = GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (G_TYPE_OBJECT)->create_editable (adaptor, type);

  if (type == GLADE_PAGE_GENERAL || type == GLADE_PAGE_COMMON)
    return (GladeEditable *) glade_cell_renderer_editor_new (adaptor, type,
                                                             editable);

  return editable;
}

static void
glade_gtk_cell_renderer_set_use_attribute (GObject      *object,
                                           const gchar  *property_name,
                                           const GValue *value)
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
glade_gtk_cell_renderer_attribute_switch (GladeWidget *gwidget,
                                          const gchar *property_name)
{
  GladeProperty *property;
  gchar *use_attr_name = g_strdup_printf ("use-attr-%s", property_name);

  property = glade_widget_get_property (gwidget, use_attr_name);
  g_free (use_attr_name);

  return property;
}

static gboolean
glade_gtk_cell_renderer_property_enabled (GObject     *object,
                                          const gchar *property_name)
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
glade_gtk_cell_renderer_set_property (GladeWidgetAdaptor *adaptor,
                                      GObject            *object,
                                      const gchar        *property_name,
                                      const GValue       *value)
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
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (G_TYPE_OBJECT)->set_property (adaptor,
                                                 object, property_name, value);
}

static void
glade_gtk_cell_renderer_write_properties (GladeWidget     *widget,
                                          GladeXmlContext *context,
                                          GladeXmlNode    *node)
{
  GladeProperty *property, *prop;
  GladePropertyDef *pdef;
  gchar *attr_name;
  GList *l;
  static gint attr_len = 0;

  if (!attr_len)
    attr_len = strlen ("attr-");

  for (l = glade_widget_get_properties (widget); l; l = l->next)
    {
      property = l->data;
      pdef   = glade_property_get_def (property);

      if (strncmp (glade_property_def_id (pdef), "attr-", attr_len) == 0)
        {
          gchar *use_attr_str;
          gboolean use_attr = FALSE;

          use_attr_str = g_strdup_printf ("use-%s", glade_property_def_id (pdef));
          glade_widget_property_get (widget, use_attr_str, &use_attr);

          attr_name = (gchar *)&glade_property_def_id (pdef)[attr_len];
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
glade_gtk_cell_renderer_write_widget (GladeWidgetAdaptor *adaptor,
                                      GladeWidget        *widget,
                                      GladeXmlContext    *context,
                                      GladeXmlNode       *node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* Write our normal properties, then chain up to write any other normal properties,
   * then attributes 
   */
  glade_gtk_cell_renderer_write_properties (widget, context, node);

  GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);
}

static void
glade_gtk_cell_renderer_parse_finished (GladeProject *project,
                                        GladeWidget  *widget)
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
      GladePropertyDef *pdef;

      property = l->data;
      pdef     = glade_property_get_def (property);

      if (strncmp (glade_property_def_id (pdef), "attr-", attr_len) != 0 &&
          strncmp (glade_property_def_id (pdef), "use-attr-", use_attr_len) != 0 &&
          (switch_prop =
           glade_gtk_cell_renderer_attribute_switch (widget,
                                                     glade_property_def_id (pdef))) != NULL)
        {
          if (glade_property_original_default (property))
            glade_property_set (switch_prop, TRUE);
          else
            glade_property_set (switch_prop, FALSE);
        }
    }
}

void
glade_gtk_cell_renderer_read_widget (GladeWidgetAdaptor *adaptor,
                                     GladeWidget        *widget,
                                     GladeXmlNode       *node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the properties... */
  GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (G_TYPE_OBJECT)->read_widget (adaptor, widget, node);

  g_signal_connect (glade_widget_get_project (widget), "parse-finished",
                    G_CALLBACK (glade_gtk_cell_renderer_parse_finished),
                    widget);
}

static gboolean
glade_gtk_cell_layout_has_renderer (GtkCellLayout   *layout,
                                    GtkCellRenderer *renderer)
{
  GList *cells = gtk_cell_layout_get_cells (layout);
  gboolean has_renderer;

  has_renderer = (g_list_find (cells, renderer) != NULL);

  g_list_free (cells);

  return has_renderer;
}

gboolean
glade_gtk_cell_renderer_sync_attributes (GObject *object)
{

  GtkCellLayout *layout;
  GtkCellRenderer *cell;
  GladeWidget *widget;
  GladeWidget *parent;
  GladeWidget *gmodel;
  GladeProperty *property;
  GladePropertyDef *pdef;
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
      pdef     = glade_property_get_def (property);

      if (strncmp (glade_property_def_id (pdef), "attr-", attr_len) == 0)
        {
          gint column = g_value_get_int (glade_property_inline_value (property));

          attr_prop_name = (gchar *)&glade_property_def_id (pdef)[attr_len];

          if (column >= 0 && column < columns)
            {
              GladeColumnType *column_type =
                  (GladeColumnType *) g_list_nth_data (column_list, column);
              GType column_gtype = g_type_from_name (column_type->type_name);
              GParamSpec *pspec = glade_property_def_get_pspec (pdef);

              if (column_gtype &&
                  g_value_type_transformable (column_gtype, pspec->value_type))
                gtk_cell_layout_add_attribute (layout, cell, attr_prop_name, column);
            }
        }
    }

  return FALSE;
}
