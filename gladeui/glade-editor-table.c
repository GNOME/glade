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
#include <glib/gi18n-lib.h>
#include "glade.h"
#include "glade-private.h"
#include "gladeui-enum-types.h"

#include "glade-editor-table.h"


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



static void glade_editor_table_init          (GladeEditorTable * self);
static void glade_editor_table_class_init    (GladeEditorTableClass * klass);

static void glade_editor_table_dispose       (GObject         *object);
static void glade_editor_table_set_property  (GObject         *object,
					      guint            prop_id,
					      const GValue    *value,
					      GParamSpec      *pspec);

static void glade_editor_table_editable_init (GladeEditableIface * iface);
static void glade_editor_table_realize       (GtkWidget * widget);
static void glade_editor_table_grab_focus    (GtkWidget * widget);

static void append_name_field (GladeEditorTable   *table);
static void append_items      (GladeEditorTable   *table,
			       GladeWidgetAdaptor *adaptor,
			       GladeEditorPageType type);


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
  GtkWidget *composite_check; /* A pointer to the composite check button */
  GtkWidget *name_field; /* A box containing the name entry and composite check */

  GList *properties; /* A list of GladeEditorPropery items.
		      * For each row in the gtk_table, there is a
		      * corrsponding GladeEditorProperty struct.
		      */

  GladeEditorPageType type; /* Is this table to be used in the common tab, ?
			     * the general tab, a packing tab or the query popup ?
			     */

  gint rows;

  gboolean show_name;
};

enum {
  PROP_0,
  PROP_PAGE_TYPE,
};

G_DEFINE_TYPE_WITH_CODE (GladeEditorTable, glade_editor_table, GTK_TYPE_GRID,
                         G_ADD_PRIVATE (GladeEditorTable)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_editor_table_editable_init));

static void
glade_editor_table_class_init (GladeEditorTableClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = glade_editor_table_dispose;
  object_class->set_property = glade_editor_table_set_property;

  widget_class->realize = glade_editor_table_realize;
  widget_class->grab_focus = glade_editor_table_grab_focus;

  g_object_class_install_property
      (object_class, PROP_PAGE_TYPE,
       g_param_spec_enum ("page-type", _("Page Type"),
			  _("The editor page type to create this GladeEditorTable for"),
			  GLADE_TYPE_EDITOR_PAGE_TYPE, GLADE_PAGE_GENERAL,
			  G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
glade_editor_table_init (GladeEditorTable *self)
{
  self->priv = glade_editor_table_get_instance_private (self);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (self), 2);
  gtk_grid_set_column_spacing (GTK_GRID (self), 6);

  /* Show name by default */
  self->priv->show_name = TRUE;
}

static void
glade_editor_table_dispose (GObject *object)
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
glade_editor_table_set_property (GObject         *object,
				 guint            prop_id,
				 const GValue    *value,
				 GParamSpec      *pspec)
{
  GladeEditorTable *table = GLADE_EDITOR_TABLE (object);

  switch (prop_id)
    {
    case PROP_PAGE_TYPE:
      table->priv->type = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glade_editor_table_realize (GtkWidget *widget)
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
glade_editor_table_grab_focus (GtkWidget *widget)
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
widget_name_edited (GtkWidget *editable, GladeEditorTable *table)
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

  if (new_name == NULL || new_name[0] == '\0')
    {
      /* If we are explicitly trying to set the widget name to be empty,
       * then we must not allow it there are any active references to
       * the widget which would otherwise break.
       *
       * Otherwise, we need to allocate a new unnamed prefix name for the widget
       */
      if (!glade_widget_has_prop_refs (widget))
	{
	  gchar *unnamed_name = glade_project_new_widget_name (glade_widget_get_project (widget), NULL, GLADE_UNNAMED_PREFIX);
	  glade_command_set_name (widget, unnamed_name);
	  g_free (unnamed_name);
	}
    }
  else if (glade_project_available_widget_name (glade_widget_get_project (widget), 
						widget, new_name))
    glade_command_set_name (widget, new_name);

  g_free (new_name);
}

static void
widget_composite_toggled (GtkToggleButton  *composite_check,
			  GladeEditorTable *table)
{
  GladeProject *project;

  if (table->priv->loaded_widget == NULL)
    {
      g_warning ("Name entry edited with no loaded widget in editor %p!\n",
                 table);
      return;
    }

  project = glade_widget_get_project (table->priv->loaded_widget);

  if (project)
    {
      if (gtk_toggle_button_get_active (composite_check))
	glade_command_set_project_template (project, table->priv->loaded_widget);
      else
	glade_command_set_project_template (project, NULL);
    }
}

static void
widget_name_changed (GladeWidget      *widget,
                     GParamSpec       *pspec,
		     GladeEditorTable *table)
{
  if (!gtk_widget_get_mapped (GTK_WIDGET (table)))
    return;

  if (table->priv->name_entry)
    {
      BLOCK_NAME_ENTRY_CB (table);

      if (glade_widget_has_name (table->priv->loaded_widget))
	gtk_entry_set_text (GTK_ENTRY (table->priv->name_entry), glade_widget_get_name (table->priv->loaded_widget));
      else
	gtk_entry_set_text (GTK_ENTRY (table->priv->name_entry), "");

      UNBLOCK_NAME_ENTRY_CB (table);
    }
}

static void
widget_composite_changed (GladeWidget      *widget,
			  GParamSpec       *pspec,
			  GladeEditorTable *table)
{
  if (!gtk_widget_get_mapped (GTK_WIDGET (table)))
    return;

  if (table->priv->name_label)
    gtk_label_set_text (GTK_LABEL (table->priv->name_label),
			glade_widget_get_is_composite (table->priv->loaded_widget) ?
			_("Class Name:") : _("ID:"));

  if (table->priv->composite_check)
    {
      g_signal_handlers_block_by_func (G_OBJECT (table->priv->composite_check),
				       G_CALLBACK (widget_composite_toggled), table);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (table->priv->composite_check),
				    glade_widget_get_is_composite (table->priv->loaded_widget));
      g_signal_handlers_unblock_by_func (G_OBJECT (table->priv->composite_check),
					 G_CALLBACK (widget_composite_toggled), table);
    }
}

static void
widget_finalized (GladeEditorTable *table, GladeWidget *where_widget_was)
{
  table->priv->loaded_widget = NULL;

  glade_editable_load (GLADE_EDITABLE (table), NULL);
}


static void
glade_editor_table_load (GladeEditable *editable, GladeWidget *widget)
{
  GladeEditorTable *table = GLADE_EDITOR_TABLE (editable);
  GladeEditorProperty *property;
  GList *list;

  /* Setup the table the first time the widget is loaded */
  if (widget && table->priv->adaptor == NULL)
    {
      table->priv->adaptor = glade_widget_get_adaptor (widget);

      if (table->priv->type == GLADE_PAGE_GENERAL)
        append_name_field (table);

      append_items (table, table->priv->adaptor, table->priv->type);
    }

  /* abort mission */
  if (table->priv->loaded_widget == widget)
    return;

  if (table->priv->loaded_widget)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (table->priv->loaded_widget),
                                            G_CALLBACK (widget_name_changed),
                                            table);
      g_signal_handlers_disconnect_by_func (G_OBJECT (table->priv->loaded_widget),
                                            G_CALLBACK (widget_composite_changed),
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

      g_signal_connect (G_OBJECT (table->priv->loaded_widget), "notify::composite",
                        G_CALLBACK (widget_composite_changed), table);

      /* The widget could die unexpectedly... */
      g_object_weak_ref (G_OBJECT (table->priv->loaded_widget),
                         (GWeakNotify) widget_finalized, table);

      if (table->priv->composite_check)
        {
          GObject *object = glade_widget_get_object (table->priv->loaded_widget);
          GladeWidgetAdaptor *adaptor = glade_widget_get_adaptor (table->priv->loaded_widget);

          if (GTK_IS_WIDGET (object) &&
              glade_widget_get_parent (table->priv->loaded_widget) == NULL)
            gtk_widget_show (table->priv->composite_check);
          else
            gtk_widget_hide (table->priv->composite_check);

          gtk_widget_set_sensitive (table->priv->composite_check,
                                    !g_str_has_prefix (glade_widget_adaptor_get_name (adaptor),
                                                       GWA_INSTANTIABLE_PREFIX));
        }

      if (table->priv->name_entry)
        {
          if (glade_widget_has_name (widget))
            gtk_entry_set_text (GTK_ENTRY (table->priv->name_entry), glade_widget_get_name (widget));
          else
            gtk_entry_set_text (GTK_ENTRY (table->priv->name_entry), "");
        }

      if (table->priv->name_label)
        widget_composite_changed (widget, NULL, table);
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
glade_editor_table_set_show_name (GladeEditable *editable, gboolean show_name)
{
  GladeEditorTable *table = GLADE_EDITOR_TABLE (editable);

  if (table->priv->show_name != show_name)
    {
      table->priv->show_name = show_name;

      if (table->priv->name_label)
	{
	  gtk_widget_set_visible (table->priv->name_label, show_name);
	  gtk_widget_set_visible (table->priv->name_field, show_name);
	}
    }
}

static void
glade_editor_table_editable_init (GladeEditableIface *iface)
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
  const gchar *name_a, *name_b;

  pa = glade_property_class_get_pspec (ca);
  pb = glade_property_class_get_pspec (cb);

  name_a = glade_property_class_id (ca);
  name_b = glade_property_class_id (cb);

  /* Special case for the 'name' property, it *always* comes first. */
  if (strcmp (name_a, "name") == 0)
    return -1;
  else if (strcmp (name_b, "name") == 0)
    return 1;
  /* Properties of the same class are sorted in the same level */
  else if (pa->owner_type == pb->owner_type)
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
  /* Group properties by thier class hierarchy */
  else
    {
      if (g_type_is_a (pa->owner_type, pb->owner_type))
        return (glade_property_class_common (ca) || glade_property_class_get_is_packing (ca)) ? 1 : -1;
      else
        return (glade_property_class_common (ca) || glade_property_class_get_is_packing (ca)) ? -1 : 1;
    }
}

static GList *
get_sorted_properties (GladeWidgetAdaptor *adaptor, GladeEditorPageType type)
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
       * invisible (or custom-layout) properties, allow adaptors to filter
       * out properties from the GladeEditorTable using the "custom-layout" attribute.
       */
      if (GLADE_PROPERTY_CLASS_IS_TYPE (klass, type) &&
	  (type == GLADE_PAGE_QUERY || 
	   (!glade_property_class_custom_layout (klass) &&
	    glade_property_class_is_visible (klass))))
        {
          list = g_list_prepend (list, klass);
        }

    }
  return g_list_sort (list, property_class_comp);
}

static GladeEditorProperty *
append_item (GladeEditorTable   *table,
             GladePropertyClass *klass,
             gboolean            from_query_dialog)
{
  GladeEditorProperty *property;
  GtkWidget *label;

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

  label = glade_editor_property_get_item_label (property);
  gtk_widget_set_hexpand (label, FALSE);

  glade_editor_table_attach (table, label, 0, table->priv->rows);
  glade_editor_table_attach (table, GTK_WIDGET (property), 1, table->priv->rows);

  table->priv->rows++;

  return property;
}

static void
append_items (GladeEditorTable   *table,
              GladeWidgetAdaptor *adaptor,
              GladeEditorPageType type)
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
append_name_field (GladeEditorTable *table)
{
  gchar *text = _("The object's unique identifier");

  /* translators: The unique identifier of an object in the project */
  table->priv->name_label = gtk_label_new (_("ID:"));
  gtk_widget_set_halign (table->priv->name_label, GTK_ALIGN_START);
  gtk_widget_show (table->priv->name_label);
  gtk_widget_set_no_show_all (table->priv->name_label, TRUE);

  table->priv->name_field = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_set_no_show_all (table->priv->name_field, TRUE);
  gtk_widget_show (table->priv->name_field);

  table->priv->composite_check = gtk_check_button_new_with_label (_("Composite"));
  gtk_widget_set_hexpand (table->priv->composite_check, FALSE);
  gtk_widget_set_tooltip_text (table->priv->composite_check, _("Whether this widget is a composite template"));
  gtk_widget_set_no_show_all (table->priv->composite_check, TRUE);

  table->priv->name_entry = gtk_entry_new ();
  gtk_widget_show (table->priv->name_entry);

  gtk_widget_set_tooltip_text (table->priv->name_label, text);
  gtk_widget_set_tooltip_text (table->priv->name_entry, text);

  g_signal_connect (G_OBJECT (table->priv->name_entry), "activate",
                    G_CALLBACK (widget_name_edited), table);
  g_signal_connect (G_OBJECT (table->priv->name_entry), "changed",
                    G_CALLBACK (widget_name_edited), table);
  g_signal_connect (G_OBJECT (table->priv->composite_check), "toggled",
                    G_CALLBACK (widget_composite_toggled), table);

  gtk_box_pack_start (GTK_BOX (table->priv->name_field), table->priv->name_entry, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (table->priv->name_field), table->priv->composite_check, FALSE, FALSE, 0);

  glade_editor_table_attach (table, table->priv->name_label, 0, table->priv->rows);
  glade_editor_table_attach (table, table->priv->name_field, 1, table->priv->rows);

  /* Set initial visiblity */
  gtk_widget_set_visible (table->priv->name_label, table->priv->show_name);
  gtk_widget_set_visible (table->priv->name_field, table->priv->show_name);

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
glade_editor_table_new (GladeWidgetAdaptor *adaptor, GladeEditorPageType type)
{
  GladeEditorTable *table;

  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  table = g_object_new (GLADE_TYPE_EDITOR_TABLE, "page-type", type, NULL);
  table->priv->adaptor = adaptor;

  if (table->priv->type == GLADE_PAGE_GENERAL)
    append_name_field (table);

  append_items (table, table->priv->adaptor, table->priv->type);

  return (GtkWidget *)table;
}
