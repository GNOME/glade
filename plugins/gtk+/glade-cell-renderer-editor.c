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

#include "glade-cell-renderer-editor.h"
#include "glade-column-types.h"


static void glade_cell_renderer_editor_finalize (GObject * object);

static void glade_cell_renderer_editor_editable_init (GladeEditableInterface *
                                                      iface);

static void glade_cell_renderer_editor_grab_focus (GtkWidget * widget);


typedef struct
{
  GladeCellRendererEditor *editor;

  GtkWidget *attributes_check;
  GladePropertyDef *pdef;
  GladePropertyDef *attr_pdef;
  GladePropertyDef *use_attr_pdef;

  GtkWidget *use_prop_label;
  GtkWidget *use_attr_label;
  GtkWidget *use_prop_eprop;
  GtkWidget *use_attr_eprop;
} CheckTab;

static GladeEditableInterface *parent_editable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeCellRendererEditor, glade_cell_renderer_editor,
                         GTK_TYPE_BOX,
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_cell_renderer_editor_editable_init));


static void
glade_cell_renderer_editor_class_init (GladeCellRendererEditorClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = glade_cell_renderer_editor_finalize;
  widget_class->grab_focus = glade_cell_renderer_editor_grab_focus;
}

static void
glade_cell_renderer_editor_init (GladeCellRendererEditor * self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self),
                                  GTK_ORIENTATION_VERTICAL);
}

static void
glade_cell_renderer_editor_load (GladeEditable * editable, GladeWidget * widget)
{
  GladeCellRendererEditor *renderer_editor =
      GLADE_CELL_RENDERER_EDITOR (editable);
  GList *l;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  /* load the embedded editable... */
  if (renderer_editor->embed)
    glade_editable_load (GLADE_EDITABLE (renderer_editor->embed), widget);

  for (l = renderer_editor->properties; l; l = l->next)
    glade_editor_property_load_by_widget (GLADE_EDITOR_PROPERTY (l->data),
                                          widget);

  if (widget)
    {
      for (l = renderer_editor->checks; l; l = l->next)
        {
          CheckTab *tab = l->data;
          gboolean use_attr = FALSE;

          glade_widget_property_get (widget, glade_property_def_id (tab->use_attr_pdef), &use_attr);
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tab->attributes_check), use_attr);

          if (use_attr)
            {
              //gtk_widget_show (tab->use_attr_label);
              gtk_widget_show (tab->use_attr_eprop);
              //gtk_widget_hide (tab->use_prop_label);
              gtk_widget_hide (tab->use_prop_eprop);
            }
          else
            {
              gtk_widget_show (tab->use_prop_label);
              gtk_widget_show (tab->use_prop_eprop);
              gtk_widget_hide (tab->use_attr_label);
              gtk_widget_hide (tab->use_attr_eprop);
            }
        }
    }
}

static void
glade_cell_renderer_editor_set_show_name (GladeEditable * editable,
                                          gboolean show_name)
{
  GladeCellRendererEditor *renderer_editor =
      GLADE_CELL_RENDERER_EDITOR (editable);

  glade_editable_set_show_name (GLADE_EDITABLE (renderer_editor->embed),
                                show_name);
}

static void
glade_cell_renderer_editor_editable_init (GladeEditableInterface * iface)
{
  parent_editable_iface = g_type_default_interface_peek (GLADE_TYPE_EDITABLE);

  iface->load = glade_cell_renderer_editor_load;
  iface->set_show_name = glade_cell_renderer_editor_set_show_name;
}

static void
glade_cell_renderer_editor_finalize (GObject * object)
{
  GladeCellRendererEditor *renderer_editor =
      GLADE_CELL_RENDERER_EDITOR (object);

  g_list_foreach (renderer_editor->checks, (GFunc) g_free, NULL);
  g_list_free (renderer_editor->checks);
  g_list_free (renderer_editor->properties);

  renderer_editor->properties = NULL;
  renderer_editor->checks = NULL;
  renderer_editor->embed = NULL;

  glade_editable_load (GLADE_EDITABLE (object), NULL);

  G_OBJECT_CLASS (glade_cell_renderer_editor_parent_class)->finalize (object);
}

static void
glade_cell_renderer_editor_grab_focus (GtkWidget * widget)
{
  GladeCellRendererEditor *renderer_editor =
      GLADE_CELL_RENDERER_EDITOR (widget);

  gtk_widget_grab_focus (renderer_editor->embed);
}

static void
attributes_toggled (GtkWidget * widget, CheckTab * tab)
{
  GladeCellRendererEditor *renderer_editor = tab->editor;
  GladeProperty *property;
  GladeWidget   *gwidget;
  GValue value = { 0, };

  gwidget = glade_editable_loaded_widget (GLADE_EDITABLE (renderer_editor));

  if (glade_editable_loading (GLADE_EDITABLE (renderer_editor)) || !gwidget)
    return;

  glade_editable_block (GLADE_EDITABLE (renderer_editor));

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (tab->attributes_check)))
    {

      glade_command_push_group (_("Setting %s to use the %s property as an attribute"),
                                glade_widget_get_name (gwidget),
                                glade_property_def_id (tab->pdef));


      property =
          glade_widget_get_property (gwidget, glade_property_def_id (tab->pdef));
      glade_property_get_default (property, &value);
      glade_command_set_property_value (property, &value);
      g_value_unset (&value);

      property =
          glade_widget_get_property (gwidget, glade_property_def_id (tab->use_attr_pdef));
      glade_command_set_property (property, TRUE);

      glade_command_pop_group ();


    }
  else
    {
      glade_command_push_group (_("Setting %s to use the %s property directly"),
                                glade_widget_get_name (gwidget),
                                glade_property_def_id (tab->pdef));

      property =
          glade_widget_get_property (gwidget, glade_property_def_id (tab->attr_pdef));
      glade_property_get_default (property, &value);
      glade_command_set_property_value (property, &value);
      g_value_unset (&value);

      property =
          glade_widget_get_property (gwidget, glade_property_def_id (tab->use_attr_pdef));
      glade_command_set_property (property, FALSE);

      glade_command_pop_group ();
    }

  glade_editable_unblock (GLADE_EDITABLE (renderer_editor));

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (renderer_editor), gwidget);
}

static gint
property_def_comp (gconstpointer a, gconstpointer b)
{
  GladePropertyDef *ca = (GladePropertyDef *)a, *cb = (GladePropertyDef *)b;
  GParamSpec *pa, *pb;

  pa = glade_property_def_get_pspec (ca);
  pb = glade_property_def_get_pspec (cb);

  if (pa->owner_type == pb->owner_type)
    {
      gdouble result = glade_property_def_weight (ca) - glade_property_def_weight (cb);
      /* Avoid cast to int */
      if (result < 0.0)
        return -1;
      else if (result > 0.0)
        return 1;
      else
        return 0;
    }
  else
    {
      if (g_type_is_a (pa->owner_type, pb->owner_type))
        return (glade_property_def_common (ca) || glade_property_def_get_is_packing (ca)) ? 1 : -1;
      else
        return (glade_property_def_common (ca) || glade_property_def_get_is_packing (ca)) ? -1 : 1;
    }
}

static GList *
get_sorted_properties (GladeWidgetAdaptor * adaptor, GladeEditorPageType type)
{
  GList *list = NULL;
  const GList *l;

  for (l = glade_widget_adaptor_get_properties (adaptor); l; l = l->next)
    {
      GladePropertyDef *def = l->data;

      if (GLADE_PROPERTY_DEF_IS_TYPE (def, type) &&
          (glade_property_def_is_visible (def)))
        {
          list = g_list_prepend (list, def);
        }
    }
  return g_list_sort (list, property_def_comp);
}


GtkWidget *
glade_cell_renderer_editor_new (GladeWidgetAdaptor * adaptor,
                                GladeEditorPageType type, GladeEditable * embed)
{
  GladeCellRendererEditor *renderer_editor;
  GladeEditorProperty *eprop;
  GladePropertyDef *pdef, *attr_pdef, *use_attr_pdef;
  GList *list, *sorted;
  GtkWidget *hbox_left, *hbox_right, *grid;
  gchar *str;
  gint row = 0;

  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
  g_return_val_if_fail (GLADE_IS_EDITABLE (embed), NULL);

  renderer_editor = g_object_new (GLADE_TYPE_CELL_RENDERER_EDITOR, NULL);
  renderer_editor->embed = GTK_WIDGET (embed);

  /* Pack the parent on top... */
  gtk_box_pack_start (GTK_BOX (renderer_editor), GTK_WIDGET (embed), FALSE,
                      FALSE, 0);

  /* Next pack in a grid for all the renderers */
  grid = gtk_grid_new ();
  gtk_orientable_set_orientation (GTK_ORIENTABLE (grid),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 4);
  gtk_box_pack_start (GTK_BOX (renderer_editor), grid, FALSE, FALSE, 0);

  sorted = get_sorted_properties (adaptor, type);

  /* For each normal property, if we have an attr- and use-attr- counterpart, load
   * a check button property pair into the table...
   */
  for (list = sorted; list; list = list->next)
    {
      gchar *attr_name;
      gchar *use_attr_name;

      pdef = list->data;

      /* "stock-size" is a normal property, but we virtualize it to use the GtkIconSize enumeration */
      if (glade_property_def_get_virtual (pdef) &&
          strcmp (glade_property_def_id (pdef), "stock-size") != 0)
        continue;

      attr_name = g_strdup_printf ("attr-%s", glade_property_def_id (pdef));
      use_attr_name = g_strdup_printf ("use-attr-%s", glade_property_def_id (pdef));

      attr_pdef =
          glade_widget_adaptor_get_property_def (adaptor, attr_name);
      use_attr_pdef =
          glade_widget_adaptor_get_property_def (adaptor, use_attr_name);

      if (attr_pdef && use_attr_pdef)
        {
          CheckTab *tab = g_new0 (CheckTab, 1);
          GParamSpec *pspec;

          pspec = glade_property_def_get_pspec (pdef);

          tab->editor = renderer_editor;
          tab->pdef = pdef;
          tab->attr_pdef = attr_pdef;
          tab->use_attr_pdef = use_attr_pdef;

          /* Label appearance... */
          hbox_left = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
          hbox_right = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
          gtk_widget_set_hexpand (hbox_right, TRUE);

          tab->attributes_check = gtk_check_button_new ();
          str = g_strdup_printf (_("Retrieve %s from model (type %s)"),
                                 glade_property_def_get_name (pdef),
                                 g_type_name (pspec->value_type));
          gtk_widget_set_tooltip_text (tab->attributes_check, str);
          g_free (str);

          gtk_box_pack_start (GTK_BOX (hbox_left), tab->attributes_check, FALSE,
                              FALSE, 4);

          /* Edit property */
          eprop = glade_widget_adaptor_create_eprop (adaptor, pdef, TRUE);
          gtk_box_pack_start (GTK_BOX (hbox_left), glade_editor_property_get_item_label (eprop), TRUE,
                              TRUE, 4);
          gtk_box_pack_start (GTK_BOX (hbox_right), GTK_WIDGET (eprop), FALSE,
                              FALSE, 4);
          renderer_editor->properties =
              g_list_prepend (renderer_editor->properties, eprop);

          tab->use_prop_label = glade_editor_property_get_item_label (eprop);
          tab->use_prop_eprop = GTK_WIDGET (eprop);

          /* Edit attribute */
          eprop =
              glade_widget_adaptor_create_eprop (adaptor, attr_pdef, TRUE);
          gtk_box_pack_start (GTK_BOX (hbox_right), GTK_WIDGET (eprop), FALSE,
                              FALSE, 4);
          renderer_editor->properties =
              g_list_prepend (renderer_editor->properties, eprop);

          gtk_grid_attach (GTK_GRID (grid), hbox_left, 0, row, 1, 1);
          gtk_grid_attach (GTK_GRID (grid), hbox_right, 1, row++, 1, 1);

          tab->use_attr_label = glade_editor_property_get_item_label (eprop);
          tab->use_attr_eprop = GTK_WIDGET (eprop);

          g_signal_connect (G_OBJECT (tab->attributes_check), "toggled",
                            G_CALLBACK (attributes_toggled), tab);

          renderer_editor->checks =
              g_list_prepend (renderer_editor->checks, tab);
        }
      g_free (attr_name);
      g_free (use_attr_name);
    }
  g_list_free (sorted);

  gtk_widget_show_all (GTK_WIDGET (renderer_editor));

  return GTK_WIDGET (renderer_editor);
}

/***************************************************************************
 *                             Editor Property                             *
 ***************************************************************************/
struct _GladeEPropCellAttribute
{
  GladeEditorProperty parent_instance;

  GtkTreeModel *columns;

  GtkWidget *spin;
  GtkWidget *combo;
};

GLADE_MAKE_EPROP (GladeEPropCellAttribute, glade_eprop_cell_attribute, GLADE, EPROP_CELL_ATTRIBUTE)

static void
glade_eprop_cell_attribute_finalize (GObject *object)
{
  /* Chain up */
  GObjectClass *parent_class =
      g_type_class_peek_parent (G_OBJECT_GET_CLASS (object));
  //GladeEPropCellAttribute *eprop_attribute = GLADE_EPROP_CELL_ATTRIBUTE (object);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GladeWidget *
glade_cell_renderer_parent_get_model (GladeWidget *widget)
{
  GtkTreeModel *model = NULL;
  
  glade_widget_property_get (widget, "model", &model);

  do
    {
      if (GTK_IS_TREE_MODEL_SORT (model))
        model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (model));
      else if (GTK_IS_TREE_MODEL_FILTER (model))
        model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
      else
        break;
    } while (model);

  if (model)
    return glade_widget_get_from_gobject (model);

  return NULL;
}

GladeWidget *
glade_cell_renderer_get_model (GladeWidget * renderer)
{
  GladeWidget *gparent;
  GObject *parent;

  if ((gparent = glade_widget_get_parent (renderer)) == NULL)
    return NULL;

  parent = glade_widget_get_object (gparent);
  
  /* Keep inline with all new cell layouts !!! */
  if (GTK_IS_TREE_VIEW_COLUMN (parent))
    {
      GladeWidget *treeview = glade_widget_get_parent (gparent);

      if (treeview && GTK_IS_TREE_VIEW (glade_widget_get_object (treeview)))
        return glade_cell_renderer_parent_get_model (treeview);
    }
  else if (GTK_IS_ICON_VIEW (parent) || GTK_IS_COMBO_BOX (parent) ||
           GTK_IS_ENTRY_COMPLETION (parent))
    return glade_cell_renderer_parent_get_model (gparent);

  return NULL;
}

static void
glade_eprop_cell_attribute_load (GladeEditorProperty * eprop,
                                 GladeProperty * property)
{
  GladeEditorPropertyClass *parent_class =
      g_type_class_peek_parent (GLADE_EDITOR_PROPERTY_GET_CLASS (eprop));
  GladeEPropCellAttribute *eprop_attribute = GLADE_EPROP_CELL_ATTRIBUTE (eprop);

  /* Chain up in a clean state... */
  parent_class->load (eprop, property);

  if (property)
    {
      GladeWidget *gmodel;
      GtkListStore *store = GTK_LIST_STORE (eprop_attribute->columns);
      GtkTreeIter iter;

      gtk_list_store_clear (store);

      /* Generate model and set active iter */
      if ((gmodel = glade_cell_renderer_get_model (glade_property_get_widget (property))) != NULL)
        {
          GList *columns = NULL, *l;

          glade_widget_property_get (gmodel, "columns", &columns);

          gtk_list_store_append (store, &iter);
          /* translators: the adjective not the verb */
          gtk_list_store_set (store, &iter, 0, _("unset"), -1);

          for (l = columns; l; l = l->next)
            {
              GladeColumnType *column = l->data;
              gchar *str = g_strdup_printf ("%s - %s", column->column_name,
                                            column->type_name);

              gtk_list_store_append (store, &iter);
              gtk_list_store_set (store, &iter, 0, str, -1);

              g_free (str);
            }

          gtk_combo_box_set_active (GTK_COMBO_BOX (eprop_attribute->combo),
                                    CLAMP (g_value_get_int (glade_property_inline_value (property)) +
                                           1, 0, g_list_length (columns) + 1));

          gtk_widget_set_sensitive (eprop_attribute->combo, TRUE);
        }
      else
        {
          gtk_list_store_append (store, &iter);
          gtk_list_store_set (store, &iter, 0, _("no model"), -1);
          gtk_combo_box_set_active (GTK_COMBO_BOX (eprop_attribute->combo), 0);
          gtk_widget_set_sensitive (eprop_attribute->combo, FALSE);
        }

      gtk_spin_button_set_value (GTK_SPIN_BUTTON (eprop_attribute->spin),
                                 (gdouble) g_value_get_int (glade_property_inline_value (property)));
    }
}

static void
combo_changed (GtkWidget * combo, GladeEditorProperty * eprop)
{
  GValue val = { 0, };

  if (glade_editor_property_loading (eprop))
    return;

  g_value_init (&val, G_TYPE_INT);
  g_value_set_int (&val,
                   (gint) gtk_combo_box_get_active (GTK_COMBO_BOX (combo)) - 1);
  glade_editor_property_commit (eprop, &val);
  g_value_unset (&val);
}


static void
spin_changed (GtkWidget * spin, GladeEditorProperty * eprop)
{
  GValue val = { 0, };

  if (glade_editor_property_loading (eprop))
    return;

  g_value_init (&val, G_TYPE_INT);
  g_value_set_int (&val, gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin)));
  glade_editor_property_commit (eprop, &val);
  g_value_unset (&val);
}

static GtkWidget *
glade_eprop_cell_attribute_create_input (GladeEditorProperty * eprop)
{
  GladeEPropCellAttribute *eprop_attribute = GLADE_EPROP_CELL_ATTRIBUTE (eprop);
  GtkWidget *hbox;
  GtkAdjustment *adjustment;
  GtkCellRenderer *cell;

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

  adjustment = glade_property_def_make_adjustment (glade_editor_property_get_property_def (eprop));
  eprop_attribute->spin = gtk_spin_button_new (adjustment, 1.0, 0);

  eprop_attribute->columns =
      (GtkTreeModel *) gtk_list_store_new (1, G_TYPE_STRING);
  eprop_attribute->combo =
      gtk_combo_box_new_with_model (eprop_attribute->columns);

  gtk_combo_box_set_popup_fixed_width (GTK_COMBO_BOX (eprop_attribute->combo),
                                       FALSE);

  /* Add cell renderer */
  cell = gtk_cell_renderer_text_new ();
  g_object_set (cell,
                "xpad", 0,
                "xalign", 0.0F,
                "ellipsize", PANGO_ELLIPSIZE_END, "width-chars", 10, NULL);

  gtk_cell_layout_clear (GTK_CELL_LAYOUT (eprop_attribute->combo));

  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (eprop_attribute->combo), cell,
                              TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (eprop_attribute->combo),
                                  cell, "text", 0, NULL);

  gtk_box_pack_start (GTK_BOX (hbox), eprop_attribute->spin, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), eprop_attribute->combo, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (eprop_attribute->combo), "changed",
                    G_CALLBACK (combo_changed), eprop);
  g_signal_connect (G_OBJECT (eprop_attribute->spin), "value-changed",
                    G_CALLBACK (spin_changed), eprop);

  return hbox;
}
