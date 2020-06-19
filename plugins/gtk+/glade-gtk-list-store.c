/*
 * glade-gtk-list-store.c - GladeWidgetAdaptor for GtkListStore
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

#include "glade-cell-renderer-editor.h"
#include "glade-store-editor.h"
#include "glade-column-types.h"
#include "glade-model-data.h"
#include "glade-gtk-cell-layout.h"

#define GLADE_TAG_COLUMNS       "columns"
#define GLADE_TAG_COLUMN        "column"
#define GLADE_TAG_TYPE          "type"

#define GLADE_TAG_ROW           "row"
#define GLADE_TAG_DATA          "data"
#define GLADE_TAG_COL           "col"

static void
glade_gtk_store_set_columns (GObject *object, const GValue *value)
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
glade_gtk_store_set_data (GObject *object, const GValue *value)
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
glade_gtk_store_set_property (GladeWidgetAdaptor *adaptor,
                              GObject            *object,
                              const gchar        *property_name,
                              const GValue       *value)
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
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (G_TYPE_OBJECT)->set_property (adaptor,
                                                 object, property_name, value);
}

GladeEditorProperty *
glade_gtk_store_create_eprop (GladeWidgetAdaptor *adaptor,
                              GladePropertyDef   *def,
                              gboolean            use_command)
{
  GladeEditorProperty *eprop;
  GParamSpec          *pspec;

  pspec = glade_property_def_get_pspec (def);

  /* chain up.. */
  if (pspec->value_type == GLADE_TYPE_COLUMN_TYPE_LIST)
    eprop = g_object_new (GLADE_TYPE_EPROP_COLUMN_TYPES,
                          "property-def", def,
                          "use-command", use_command, NULL);
  else if (pspec->value_type == GLADE_TYPE_MODEL_DATA_TREE)
    eprop = g_object_new (GLADE_TYPE_EPROP_MODEL_DATA,
                          "property-def", def,
                          "use-command", use_command, NULL);
  else
    eprop = GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS
        (G_TYPE_OBJECT)->create_eprop (adaptor, def, use_command);
  return eprop;
}


static void
glade_gtk_store_columns_changed (GladeProperty *property,
                                 GValue        *old_value,
                                 GValue        *new_value,
                                 GladeWidget   *store)
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
glade_gtk_store_post_create (GladeWidgetAdaptor *adaptor,
                             GObject            *object,
                             GladeCreateReason   reason)
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
glade_gtk_store_create_editable (GladeWidgetAdaptor *adaptor,
                                 GladeEditorPageType type)
{
  GladeEditable *editable;

  /* Get base editable */
  editable = GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (G_TYPE_OBJECT)->create_editable (adaptor, type);

  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_store_editor_new (adaptor, editable);

  return editable;
}

gchar *
glade_gtk_store_string_from_value (GladeWidgetAdaptor *adaptor,
                                   GladePropertyDef   *def,
                                   const GValue       *value)
{
  GString *string;
  GParamSpec *pspec;

  pspec = glade_property_def_get_pspec (def);

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
    return GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS
        (G_TYPE_OBJECT)->string_from_value (adaptor, def, value);
}

static void
glade_gtk_store_write_columns (GladeWidget     *widget,
                               GladeXmlContext *context,
                               GladeXmlNode    *node)
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
glade_gtk_store_write_data (GladeWidget     *widget,
                            GladeXmlContext *context,
                            GladeXmlNode    *node)
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
glade_gtk_store_write_widget (GladeWidgetAdaptor *adaptor,
                              GladeWidget        *widget,
                              GladeXmlContext    *context,
                              GladeXmlNode       *node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and write all the normal properties.. */
  GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);

  glade_gtk_store_write_columns (widget, context, node);
  glade_gtk_store_write_data (widget, context, node);
}

static void
glade_gtk_store_read_columns (GladeWidget *widget, GladeXmlNode *node)
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
      buffer[255] = '\0';

      if (!glade_xml_node_verify_silent (prop, GLADE_TAG_COLUMN) &&
          !glade_xml_node_is_comment (prop))
        continue;

      if (glade_xml_node_is_comment (prop))
        {
          comment_str = glade_xml_get_content (prop);
          if (sscanf (comment_str, " column-name %255s", buffer) == 1)
            strcpy (column_name, buffer);

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
glade_gtk_store_read_data (GladeWidget *widget, GladeXmlNode *node)
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
glade_gtk_store_read_widget (GladeWidgetAdaptor *adaptor,
                             GladeWidget        *widget, 
                             GladeXmlNode       *node)
{
  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS (G_TYPE_OBJECT)->read_widget (adaptor, widget, node);

  glade_gtk_store_read_columns (widget, node);

  if (GTK_IS_LIST_STORE (glade_widget_get_object (widget)))
    glade_gtk_store_read_data (widget, node);
}
