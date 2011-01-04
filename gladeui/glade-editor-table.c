/*
 * Copyright (C) 2008 Tristan Van Berkom.
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
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#include <config.h>
#include <gladeui/glade.h>
#include <glib/gi18n-lib.h>

#include "glade-editor-table.h"

static void glade_editor_table_init          (GladeEditorTable * self);
static void glade_editor_table_class_init    (GladeEditorTableClass * klass);
static void glade_editor_table_dispose       (GObject * object);
static void glade_editor_table_editable_init (GladeEditableIface * iface);
static void glade_editor_table_realize       (GtkWidget * widget);
static void glade_editor_table_grab_focus    (GtkWidget * widget);

struct _GladeEditorTablePrivate
{
  GladeWidgetAdaptor *adaptor; /* The GladeWidgetAdaptor this
				* table was created for.
				*/

  GladeWidget *loaded_widget; /* A pointer to the currently loaded GladeWidget
			       */
	
  GtkWidget *name_label; /* A pointer to the "Name:" label (for show/hide) */
  GtkWidget *name_entry; /* A pointer to the gtk_entry that holds
			  * the name of the widget. This is the
			  * first item _pack'ed to the table_widget.
			  * We have a pointer here because it is an
			  * entry which will not be created from a
			  * GladeProperty but rather from code.
			  */

  GList *properties; /* A list of GladeEditorPropery items.
		      * For each row in the gtk_table, there is a
		      * corrsponding GladeEditorProperty struct.
		      */

  GladeEditorPageType type; /* Is this table to be used in the common tab, ?
			     * the general tab, a packing tab or the query popup ?
			     */

  gint rows;
};

G_DEFINE_TYPE_WITH_CODE (GladeEditorTable, glade_editor_table, GTK_TYPE_GRID,
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_editor_table_editable_init));


#define BLOCK_NAME_ENTRY_CB(table)					\
	do { if (table->priv->name_entry)					\
			g_signal_handlers_block_by_func (G_OBJECT (table->priv->name_entry), \
							 G_CALLBACK (widget_name_edited), table); \
	} while (0);

#define UNBLOCK_NAME_ENTRY_CB(table)					\
	do { if (table->priv->name_entry)					\
			g_signal_handlers_unblock_by_func (G_OBJECT (table->priv->name_entry), \
							   G_CALLBACK (widget_name_edited), table); \
	} while (0);


static void
glade_editor_table_class_init (GladeEditorTableClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = glade_editor_table_dispose;
  widget_class->realize = glade_editor_table_realize;
  widget_class->grab_focus = glade_editor_table_grab_focus;

  g_type_class_add_private (klass, sizeof (GladeEditorTablePrivate));
}

static void
glade_editor_table_init (GladeEditorTable * self)
{
  self->priv = 
    G_TYPE_INSTANCE_GET_PRIVATE ((self),
				 GLADE_TYPE_EDITOR_TABLE,
				 GladeEditorTablePrivate);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (self), 4);
}

static void
glade_editor_table_dispose (GObject * object)
{
  GladeEditorTable *table = GLADE_EDITOR_TABLE (object);

  table->priv->properties = (g_list_free (table->priv->properties), NULL);

  /* the entry is finalized anyway, just avoid setting
   * text in it from _load();
   */
  table->priv->name_entry = NULL;

  glade_editable_load (GLADE_EDITABLE (table), NULL);

  G_OBJECT_CLASS (glade_editor_table_parent_class)->dispose (object);
}


static void
glade_editor_table_realize (GtkWidget * widget)
{
  GladeEditorTable *table = GLADE_EDITOR_TABLE (widget);
  GList *list;
  GladeEditorProperty *property;

  GTK_WIDGET_CLASS (glade_editor_table_parent_class)->realize (widget);

  /* Sync up properties, even if widget is NULL */
  for (list = table->priv->properties; list; list = list->next)
    {
      property = list->data;
      glade_editor_property_load_by_widget (property, table->priv->loaded_widget);
    }
}

static void
glade_editor_table_grab_focus (GtkWidget * widget)
{
  GladeEditorTable *editor_table = GLADE_EDITOR_TABLE (widget);

  if (editor_table->priv->name_entry &&
      gtk_widget_get_mapped (editor_table->priv->name_entry))
    gtk_widget_grab_focus (editor_table->priv->name_entry);
  else if (editor_table->priv->properties)
    gtk_widget_grab_focus (GTK_WIDGET (editor_table->priv->properties->data));
  else
    GTK_WIDGET_CLASS (glade_editor_table_parent_class)->grab_focus (widget);
}

static void
widget_name_edited (GtkWidget * editable, GladeEditorTable * table)
{
  GladeWidget *widget;
  gchar *new_name;

  g_return_if_fail (GTK_IS_EDITABLE (editable));
  g_return_if_fail (GLADE_IS_EDITOR_TABLE (table));

  if (table->priv->loaded_widget == NULL)
    {
      g_warning ("Name entry edited with no loaded widget in editor %p!\n",
                 table);
      return;
    }

  widget = table->priv->loaded_widget;
  new_name = gtk_editable_get_chars (GTK_EDITABLE (editable), 0, -1);

  if (glade_project_available_widget_name (glade_widget_get_project (widget), 
					   widget, new_name))
    glade_command_set_name (widget, new_name);
  g_free (new_name);
}

static void
widget_name_changed (GladeWidget * widget,
                     GParamSpec * pspec, GladeEditorTable * table)
{
  if (!gtk_widget_get_mapped (GTK_WIDGET (table)))
    return;

  if (table->priv->name_entry)
    {
      BLOCK_NAME_ENTRY_CB (table);
      gtk_entry_set_text (GTK_ENTRY (table->priv->name_entry),
                          glade_widget_get_name (table->priv->loaded_widget));
      UNBLOCK_NAME_ENTRY_CB (table);
    }

}

static void
widget_finalized (GladeEditorTable * table, GladeWidget * where_widget_was)
{
  table->priv->loaded_widget = NULL;

  glade_editable_load (GLADE_EDITABLE (table), NULL);
}


static void
glade_editor_table_load (GladeEditable * editable, GladeWidget * widget)
{
  GladeEditorTable *table = GLADE_EDITOR_TABLE (editable);
  GladeEditorProperty *property;
  GList *list;

  /* abort mission */
  if (table->priv->loaded_widget == widget)
    return;

  if (table->priv->loaded_widget)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (table->priv->loaded_widget),
                                            G_CALLBACK (widget_name_changed),
                                            table);

      /* The widget could die unexpectedly... */
      g_object_weak_unref (G_OBJECT (table->priv->loaded_widget),
                           (GWeakNotify) widget_finalized, table);
    }

  table->priv->loaded_widget = widget;

  BLOCK_NAME_ENTRY_CB (table);

  if (table->priv->loaded_widget)
    {
      g_signal_connect (G_OBJECT (table->priv->loaded_widget), "notify::name",
                        G_CALLBACK (widget_name_changed), table);

      /* The widget could die unexpectedly... */
      g_object_weak_ref (G_OBJECT (table->priv->loaded_widget),
                         (GWeakNotify) widget_finalized, table);

      if (table->priv->name_entry)
        gtk_entry_set_text (GTK_ENTRY (table->priv->name_entry), 
			    glade_widget_get_name (widget));

    }
  else if (table->priv->name_entry)
    gtk_entry_set_text (GTK_ENTRY (table->priv->name_entry), "");

  UNBLOCK_NAME_ENTRY_CB (table);

  /* Sync up properties, even if widget is NULL */
  for (list = table->priv->properties; list; list = list->next)
    {
      property = list->data;
      glade_editor_property_load_by_widget (property, widget);
    }
}

static void
glade_editor_table_set_show_name (GladeEditable * editable, gboolean show_name)
{
  GladeEditorTable *table = GLADE_EDITOR_TABLE (editable);

  if (table->priv->name_label)
    {
      if (show_name)
        {
          gtk_widget_show (table->priv->name_label);
          gtk_widget_show (table->priv->name_entry);
        }
      else
        {
          gtk_widget_hide (table->priv->name_label);
          gtk_widget_hide (table->priv->name_entry);
        }
    }
}

static void
glade_editor_table_editable_init (GladeEditableIface * iface)
{
  iface->load = glade_editor_table_load;
  iface->set_show_name = glade_editor_table_set_show_name;
}

static void
glade_editor_table_attach (GladeEditorTable * table,
                           GtkWidget * child, gint pos, gint row)
{
  gtk_grid_attach (GTK_GRID (table), child, pos, row, 1, 1);

  if (pos)
    gtk_widget_set_hexpand (child, TRUE);
}

static gint
property_class_comp (gconstpointer a, gconstpointer b)
{
  GladePropertyClass *ca = (GladePropertyClass *)a, *cb = (GladePropertyClass *)b;
  GParamSpec *pa, *pb;

  pa = glade_property_class_get_pspec (ca);
  pb = glade_property_class_get_pspec (cb);

  if (pa->owner_type == pb->owner_type)
    {
      gdouble result = glade_property_class_weight (ca) - glade_property_class_weight (cb);
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
        return (glade_property_class_common (ca) || glade_property_class_get_is_packing (ca)) ? 1 : -1;
      else
        return (glade_property_class_common (ca) || glade_property_class_get_is_packing (ca)) ? -1 : 1;
    }
}

static GList *
get_sorted_properties (GladeWidgetAdaptor * adaptor, GladeEditorPageType type)
{
  const GList *l, *properties;
  GList *list = NULL;

  properties =
    (type == GLADE_PAGE_PACKING) ? 
    glade_widget_adaptor_get_packing_props (adaptor) : 
    glade_widget_adaptor_get_properties (adaptor);

  for (l = properties; l; l = g_list_next (l))
    {
      GladePropertyClass *klass = l->data;

      /* Collect properties in our domain, query dialogs are allowed editor 
       * invisible properties, allow adaptors to filter out properties from
       * the GladeEditorTable using the "custom-layout" attribute.
       */
      if (!glade_property_class_custom_layout (klass) && GLADE_PROPERTY_CLASS_IS_TYPE (klass, type)
          && (glade_property_class_is_visible (klass) || type == GLADE_PAGE_QUERY))
        {
          list = g_list_prepend (list, klass);
        }

    }
  return g_list_sort (list, property_class_comp);
}

static GladeEditorProperty *
append_item (GladeEditorTable * table,
             GladePropertyClass * klass, gboolean from_query_dialog)
{
  GladeEditorProperty *property;

  if (!(property = glade_widget_adaptor_create_eprop
        (glade_property_class_get_adaptor (klass), klass, from_query_dialog == FALSE)))
    {
      g_critical ("Unable to create editor for property '%s' of class '%s'",
                  glade_property_class_id (klass), 
		  glade_widget_adaptor_get_name (glade_property_class_get_adaptor (klass)));
      return NULL;
    }

  gtk_widget_show (GTK_WIDGET (property));
  gtk_widget_show_all (glade_editor_property_get_item_label (property));

  glade_editor_table_attach (table, glade_editor_property_get_item_label (property), 0, table->priv->rows);
  glade_editor_table_attach (table, GTK_WIDGET (property), 1, table->priv->rows);

  table->priv->rows++;

  return property;
}


static void
append_items (GladeEditorTable * table,
              GladeWidgetAdaptor * adaptor, GladeEditorPageType type)
{
  GladeEditorProperty *property;
  GladePropertyClass *property_class;
  GList *list, *sorted_list;

  sorted_list = get_sorted_properties (adaptor, type);
  for (list = sorted_list; list != NULL; list = list->next)
    {
      property_class = (GladePropertyClass *) list->data;

      property = append_item (table, property_class, type == GLADE_PAGE_QUERY);
      table->priv->properties = g_list_prepend (table->priv->properties, property);
    }
  g_list_free (sorted_list);

  table->priv->properties = g_list_reverse (table->priv->properties);
}

static void
append_name_field (GladeEditorTable * table)
{
  gchar *text = _("The Object's name");

  /* Name */
  table->priv->name_label = gtk_label_new (_("Name:"));
  gtk_misc_set_alignment (GTK_MISC (table->priv->name_label), 0.0, 0.5);
  gtk_widget_show (table->priv->name_label);
  gtk_widget_set_no_show_all (table->priv->name_label, TRUE);

  table->priv->name_entry = gtk_entry_new ();
  gtk_widget_show (table->priv->name_entry);
  gtk_widget_set_no_show_all (table->priv->name_entry, TRUE);

  gtk_widget_set_tooltip_text (table->priv->name_label, text);
  gtk_widget_set_tooltip_text (table->priv->name_entry, text);

  g_signal_connect (G_OBJECT (table->priv->name_entry), "activate",
                    G_CALLBACK (widget_name_edited), table);
  g_signal_connect (G_OBJECT (table->priv->name_entry), "changed",
                    G_CALLBACK (widget_name_edited), table);

  glade_editor_table_attach (table, table->priv->name_label, 0, table->priv->rows);
  glade_editor_table_attach (table, table->priv->name_entry, 1, table->priv->rows);

  table->priv->rows++;
}

/**
 * glade_editor_table_new:
 * @adaptor: A #GladeWidgetAdaptor
 * @type: The #GladeEditorPageType
 *
 * Creates a new #GladeEditorTable. 
 *
 * Returns: a new #GladeEditorTable
 *
 */
GtkWidget *
glade_editor_table_new (GladeWidgetAdaptor * adaptor, GladeEditorPageType type)
{
  GladeEditorTable *table;

  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  table = g_object_new (GLADE_TYPE_EDITOR_TABLE, NULL);
  table->priv->adaptor = adaptor;
  table->priv->type = type;

  if (type == GLADE_PAGE_GENERAL)
    append_name_field (table);

  append_items (table, adaptor, type);

  gtk_widget_show (GTK_WIDGET (table));

  return GTK_WIDGET (table);
}
