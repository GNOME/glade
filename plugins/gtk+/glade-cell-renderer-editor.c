/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Tristan Van Berkom.
 *
 * This library is free software; you can redistribute it and/or it
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


static void glade_cell_renderer_editor_finalize        (GObject              *object);

static void glade_cell_renderer_editor_editable_init   (GladeEditableIface *iface);

static void glade_cell_renderer_editor_grab_focus      (GtkWidget            *widget);


typedef struct {
	GladeCellRendererEditor  *editor;

	GtkWidget                *attributes_check;
	GladePropertyClass       *pclass;
	GladePropertyClass       *attr_pclass;
	GladePropertyClass       *use_attr_pclass;

	GtkWidget                *use_prop_label;
	GtkWidget                *use_attr_label;
	GtkWidget                *use_prop_eprop;
	GtkWidget                *use_attr_eprop;
} CheckTab;

G_DEFINE_TYPE_WITH_CODE (GladeCellRendererEditor, glade_cell_renderer_editor, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_cell_renderer_editor_editable_init));


static void
glade_cell_renderer_editor_class_init (GladeCellRendererEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize     = glade_cell_renderer_editor_finalize;
	widget_class->grab_focus   = glade_cell_renderer_editor_grab_focus;
}

static void
glade_cell_renderer_editor_init (GladeCellRendererEditor *self)
{
}

static void
project_changed (GladeProject      *project,
		 GladeCommand      *command,
		 gboolean           execute,
		 GladeCellRendererEditor *renderer_editor)
{
	if (renderer_editor->modifying ||
	    !GTK_WIDGET_MAPPED (renderer_editor))
		return;

	/* Reload on all commands */
	glade_editable_load (GLADE_EDITABLE (renderer_editor), renderer_editor->loaded_widget);
}


static void
project_finalized (GladeCellRendererEditor *renderer_editor,
		   GladeProject       *where_project_was)
{
	renderer_editor->loaded_widget = NULL;

	glade_editable_load (GLADE_EDITABLE (renderer_editor), NULL);
}

static void
glade_cell_renderer_editor_load (GladeEditable *editable,
			       GladeWidget   *widget)
{
	GladeCellRendererEditor  *renderer_editor = GLADE_CELL_RENDERER_EDITOR (editable);
	GList                    *l;

	renderer_editor->loading = TRUE;

	/* Since we watch the project*/
	if (renderer_editor->loaded_widget)
	{
		g_signal_handlers_disconnect_by_func (G_OBJECT (renderer_editor->loaded_widget->project),
						      G_CALLBACK (project_changed), renderer_editor);

		/* The widget could die unexpectedly... */
		g_object_weak_unref (G_OBJECT (renderer_editor->loaded_widget->project),
				     (GWeakNotify)project_finalized,
				     renderer_editor);
	}

	/* Mark our widget... */
	renderer_editor->loaded_widget = widget;

	if (renderer_editor->loaded_widget)
	{
		/* This fires for undo/redo */
		g_signal_connect (G_OBJECT (renderer_editor->loaded_widget->project), "changed",
				  G_CALLBACK (project_changed), renderer_editor);

		/* The widget/project could die unexpectedly... */
		g_object_weak_ref (G_OBJECT (renderer_editor->loaded_widget->project),
				   (GWeakNotify)project_finalized,
				   renderer_editor);
	}

	/* load the embedded editable... */
	if (renderer_editor->embed)
		glade_editable_load (GLADE_EDITABLE (renderer_editor->embed), widget);

	for (l = renderer_editor->properties; l; l = l->next)
		glade_editor_property_load_by_widget (GLADE_EDITOR_PROPERTY (l->data), widget);

	if (widget)
	{
		for (l = renderer_editor->checks; l; l = l->next)
		{
			CheckTab *tab = l->data;
			gboolean use_attr = FALSE;

			glade_widget_property_get (widget, tab->use_attr_pclass->id, &use_attr);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (tab->attributes_check), use_attr);
			

			if (use_attr)
			{
				gtk_widget_show (tab->use_attr_label);
				gtk_widget_show (tab->use_attr_eprop);
				gtk_widget_hide (tab->use_prop_label);
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
	renderer_editor->loading = FALSE;
}

static void
glade_cell_renderer_editor_set_show_name (GladeEditable *editable,
					  gboolean       show_name)
{
	GladeCellRendererEditor *renderer_editor = GLADE_CELL_RENDERER_EDITOR (editable);

	glade_editable_set_show_name (GLADE_EDITABLE (renderer_editor->embed), show_name);
}

static void
glade_cell_renderer_editor_editable_init (GladeEditableIface *iface)
{
	iface->load = glade_cell_renderer_editor_load;
	iface->set_show_name = glade_cell_renderer_editor_set_show_name;
}

static void
glade_cell_renderer_editor_finalize (GObject *object)
{
	GladeCellRendererEditor *renderer_editor = GLADE_CELL_RENDERER_EDITOR (object);

	g_list_foreach (renderer_editor->checks, (GFunc)g_free, NULL);
	g_list_free (renderer_editor->checks);
	g_list_free (renderer_editor->properties);

	renderer_editor->properties  = NULL;
	renderer_editor->checks      = NULL;
	renderer_editor->embed       = NULL;

	glade_editable_load (GLADE_EDITABLE (object), NULL);

	G_OBJECT_CLASS (glade_cell_renderer_editor_parent_class)->finalize (object);
}

static void
glade_cell_renderer_editor_grab_focus (GtkWidget *widget)
{
	GladeCellRendererEditor *renderer_editor = GLADE_CELL_RENDERER_EDITOR (widget);

	gtk_widget_grab_focus (renderer_editor->embed);
}


static void
table_attach (GtkWidget *table, 
	      GtkWidget *child, 
	      gint pos, gint row)
{
	gtk_table_attach (GTK_TABLE (table), child,
			  pos, pos+1, row, row +1,
			  pos ? 0 : GTK_EXPAND | GTK_FILL,
			  GTK_EXPAND | GTK_FILL,
			  3, 1);
}

static void
attributes_toggled (GtkWidget  *widget,
		    CheckTab   *tab)
{
	GladeCellRendererEditor  *renderer_editor = tab->editor;
	GladeProperty            *property;
	GValue                    value = { 0, };

	if (renderer_editor->loading || !renderer_editor->loaded_widget)
		return;

	renderer_editor->modifying = TRUE;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (tab->attributes_check)))
	{

		glade_command_push_group (_("Setting %s to use the %s property as an attribute"), 
					  renderer_editor->loaded_widget->name, tab->pclass->id);
		

		property = glade_widget_get_property (renderer_editor->loaded_widget, tab->pclass->id);
		glade_property_get_default (property, &value);
		glade_command_set_property_value (property, &value);
		g_value_unset (&value);
		
		property = glade_widget_get_property (renderer_editor->loaded_widget, tab->use_attr_pclass->id);
		glade_command_set_property (property, TRUE);
		
		glade_command_pop_group ();
		

	}
	else
	{
		glade_command_push_group (_("Setting %s to use the %s property directly"), 
					  renderer_editor->loaded_widget->name, tab->pclass->id);
		
		
		property = glade_widget_get_property (renderer_editor->loaded_widget, tab->attr_pclass->id);
		glade_property_get_default (property, &value);
		glade_command_set_property_value (property, &value);
		g_value_unset (&value);
		
		property = glade_widget_get_property (renderer_editor->loaded_widget, tab->use_attr_pclass->id);
		glade_command_set_property (property, FALSE);	
		
		glade_command_pop_group ();
	}
	renderer_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (renderer_editor), 
			     renderer_editor->loaded_widget);
}


#define EDITOR_COLUMN_SIZE 150

static void
label_size_request (GtkWidget *widget, GtkRequisition *requisition, 
		    gpointer user_data)
{
	requisition->width = EDITOR_COLUMN_SIZE;
}

static void
label_size_allocate_after (GtkWidget *container, GtkAllocation *allocation,
			   GtkWidget *widget)
{
	GtkWidget *check;
	GtkRequisition req = { -1, -1 };
	gint width = EDITOR_COLUMN_SIZE;
	gint check_width;

	/* Here we have to subtract the check button and the 
	 * remaining padding inside the hbox so that we are
	 * only dealing with the size of the label.
	 * (note the '4' here comes from the hbox spacing).
	 */
	check = g_object_get_data (G_OBJECT (container), "attributes-check");
	g_assert (check);

	gtk_widget_size_request (check, &req);
	check_width = req.width + 4 + 4;

	if (allocation->width > width)
		width = allocation->width;

	gtk_widget_set_size_request (widget, width - check_width, -1);

	gtk_container_check_resize (GTK_CONTAINER (container));

	/* Sometimes labels aren't drawn correctly after resize without this */
	gtk_widget_queue_draw (container);


}


static gint
property_class_comp (gconstpointer a, gconstpointer b)
{
	const GladePropertyClass *ca = a, *cb = b;
	
	if (ca->pspec->owner_type == cb->pspec->owner_type)
	{
		gdouble result = ca->weight - cb->weight;
		/* Avoid cast to int */
		if (result < 0.0) return -1;
		else if (result > 0.0) return 1;
		else return 0;
	}
	else
	{
		if (g_type_is_a (ca->pspec->owner_type, cb->pspec->owner_type))
			return (ca->common || ca->packing) ? 1 : -1;
		else
			return (ca->common || ca->packing) ? -1 : 1;
	}
}

static GList *
get_sorted_properties (GladeWidgetAdaptor   *adaptor,
		       GladeEditorPageType   type)
{
	GList *l, *list = NULL;

	for (l = adaptor->properties; l; l = g_list_next (l))
	{
		GladePropertyClass *klass = l->data;

		if (GLADE_PROPERTY_CLASS_IS_TYPE (klass, type) &&
		    (glade_property_class_is_visible (klass)))
		{
			list = g_list_prepend (list, klass);
		}
	}
	return g_list_sort (list, property_class_comp);
}


GtkWidget *
glade_cell_renderer_editor_new (GladeWidgetAdaptor  *adaptor,
				GladeEditorPageType  type,
				GladeEditable       *embed)
{
	GladeCellRendererEditor  *renderer_editor;
	GladeEditorProperty      *eprop;
	GladePropertyClass       *pclass, *attr_pclass, *use_attr_pclass;
	GList                    *list, *sorted;
	GtkWidget                *label, *alignment, *table, *hbox, *separator;
	gchar                    *str;

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (GLADE_IS_EDITABLE (embed), NULL);

	renderer_editor = g_object_new (GLADE_TYPE_CELL_RENDERER_EDITOR, NULL);
	renderer_editor->embed = GTK_WIDGET (embed);

	/* Pack the parent on top... */
	gtk_box_pack_start (GTK_BOX (renderer_editor), GTK_WIDGET (embed), FALSE, FALSE, 0);

	sorted = get_sorted_properties (adaptor, type);

	/* For each normal property, if we have an attr- and use-attr- counterpart, load
	 * a check button property pair into the table...
	 */
	for (list = sorted; list; list = list->next)
	{
		gchar *attr_name;
		gchar *use_attr_name;
		gint   rows = 0;

		pclass = list->data;

		if (pclass->virt)
			continue;

		attr_name       = g_strdup_printf ("attr-%s", pclass->id);
		use_attr_name   = g_strdup_printf ("use-attr-%s", pclass->id);

		attr_pclass     = glade_widget_adaptor_get_property_class (adaptor, attr_name);
		use_attr_pclass = glade_widget_adaptor_get_property_class (adaptor, use_attr_name);

		if (attr_pclass && use_attr_pclass)
		{
			CheckTab *tab = g_new0 (CheckTab, 1);

			tab->editor          = renderer_editor;
			tab->pclass          = pclass;
			tab->attr_pclass     = attr_pclass;
			tab->use_attr_pclass = use_attr_pclass;

			/* Spacing */
			separator = gtk_hbox_new (FALSE, 0);
			gtk_box_pack_start (GTK_BOX (renderer_editor), separator, FALSE, FALSE, 4);


			/* Label appearance... */
			hbox   = gtk_hbox_new (FALSE, 0);
			str    = g_strdup_printf (_("Retrieve <b>%s</b> from model (type %s)"), 
						  pclass->name, g_type_name (pclass->pspec->value_type));
			label  = gtk_label_new (str);
			g_free (str);

			gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
			gtk_label_set_line_wrap (GTK_LABEL(label), TRUE);
			gtk_label_set_line_wrap_mode (GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
			gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

			tab->attributes_check = gtk_check_button_new ();

			gtk_box_pack_start (GTK_BOX (hbox), tab->attributes_check, FALSE, FALSE, 4);
			gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 4);
			gtk_box_pack_start (GTK_BOX (renderer_editor), hbox, FALSE, FALSE, 0);

			/* A Hack so that PANGO_WRAP_WORD_CHAR works nicely */
			g_object_set_data (G_OBJECT (hbox), "attributes-check", tab->attributes_check);
			g_signal_connect (G_OBJECT (hbox), "size-request",
					  G_CALLBACK (label_size_request), NULL);
			g_signal_connect_after (G_OBJECT (hbox), "size-allocate",
						G_CALLBACK (label_size_allocate_after), label);


			alignment = gtk_alignment_new (1.0F, 1.0F, 1.0F, 1.0F);
			gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
 			gtk_box_pack_start (GTK_BOX (renderer_editor), alignment, FALSE, FALSE, 0);

			table = gtk_table_new (0, 0, FALSE);
			gtk_container_add (GTK_CONTAINER (alignment), table);

			/* Edit property */
			eprop           = glade_widget_adaptor_create_eprop (adaptor, pclass, TRUE);
			table_attach (table, eprop->item_label, 0, rows);
			table_attach (table, GTK_WIDGET (eprop), 1, rows++);
			renderer_editor->properties = g_list_prepend (renderer_editor->properties, eprop);

			tab->use_prop_label = eprop->item_label;
			tab->use_prop_eprop = GTK_WIDGET (eprop);

			/* Edit attribute */
			eprop = glade_widget_adaptor_create_eprop (adaptor, attr_pclass, TRUE);
			table_attach (table, eprop->item_label, 0, rows);
			table_attach (table, GTK_WIDGET (eprop), 1, rows++);
			renderer_editor->properties = g_list_prepend (renderer_editor->properties, eprop);

			tab->use_attr_label = eprop->item_label;
			tab->use_attr_eprop = GTK_WIDGET (eprop);

			g_signal_connect (G_OBJECT (tab->attributes_check), "toggled",
					  G_CALLBACK (attributes_toggled), tab);

			renderer_editor->checks = g_list_prepend (renderer_editor->checks, tab);
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
typedef struct
{
	GladeEditorProperty parent_instance;

	GtkTreeModel *columns;

	GtkWidget    *spin;
	GtkWidget    *combo;
} GladeEPropCellAttribute;

GLADE_MAKE_EPROP (GladeEPropCellAttribute, glade_eprop_cell_attribute)
#define GLADE_EPROP_CELL_ATTRIBUTE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_CELL_ATTRIBUTE, GladeEPropCellAttribute))
#define GLADE_EPROP_CELL_ATTRIBUTE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_CELL_ATTRIBUTE, GladeEPropCellAttributeClass))
#define GLADE_IS_EPROP_CELL_ATTRIBUTE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_CELL_ATTRIBUTE))
#define GLADE_IS_EPROP_CELL_ATTRIBUTE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_CELL_ATTRIBUTE))
#define GLADE_EPROP_CELL_ATTRIBUTE_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_CELL_ATTRIBUTE, GladeEPropCellAttributeClass))

static void
glade_eprop_cell_attribute_finalize (GObject *object)
{
	/* Chain up */
	GObjectClass *parent_class = g_type_class_peek_parent (G_OBJECT_GET_CLASS (object));
	//GladeEPropCellAttribute *eprop_attribute = GLADE_EPROP_CELL_ATTRIBUTE (object);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GladeWidget *
glade_cell_renderer_get_model (GladeWidget *renderer)
{
	GladeWidget *model = NULL;

	/* Keep inline with all new cell layouts !!! */
	if (renderer->parent && GTK_IS_TREE_VIEW_COLUMN (renderer->parent->object))
	{
		GladeWidget *column = renderer->parent;

		if (column->parent && GTK_IS_TREE_VIEW (column->parent->object))
		{
			GladeWidget *view = column->parent;
			GtkTreeModel *real_model = NULL;
			glade_widget_property_get (view, "model", &real_model);
			if (real_model)
				model = glade_widget_get_from_gobject (real_model);
		}
	}
	else if (renderer->parent && GTK_IS_ICON_VIEW (renderer->parent->object))
	{
		GladeWidget *view = renderer->parent;
		GtkTreeModel *real_model = NULL;
		glade_widget_property_get (view, "model", &real_model);
		if (real_model)
			model = glade_widget_get_from_gobject (real_model);		
	}
	else if (renderer->parent && GTK_IS_COMBO_BOX (renderer->parent->object))
	{
		GladeWidget *combo = renderer->parent;
		GtkTreeModel *real_model = NULL;
		glade_widget_property_get (combo, "model", &real_model);
		if (real_model)
			model = glade_widget_get_from_gobject (real_model);		
	}

	return model;
}

static void
glade_eprop_cell_attribute_load (GladeEditorProperty *eprop, 
				 GladeProperty       *property)
{
	GladeEditorPropertyClass *parent_class = 
		g_type_class_peek_parent (GLADE_EDITOR_PROPERTY_GET_CLASS (eprop));
	GladeEPropCellAttribute *eprop_attribute = GLADE_EPROP_CELL_ATTRIBUTE (eprop);

	/* Chain up in a clean state... */
	parent_class->load (eprop, property);
	
	if (property)
	{
		GladeWidget  *gmodel;
		GtkListStore *store = GTK_LIST_STORE (eprop_attribute->columns);
		GtkTreeIter   iter;

		gtk_list_store_clear (store);

		/* Generate model and set active iter */
		if ((gmodel = glade_cell_renderer_get_model (property->widget)) != NULL)
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
							      g_type_name (column->type));

				gtk_list_store_append (store, &iter);
				gtk_list_store_set (store, &iter, 0, str, -1);

				g_free (str);
			}

			gtk_combo_box_set_active (GTK_COMBO_BOX (eprop_attribute->combo), 
						  CLAMP (g_value_get_int (property->value) + 1, 
							 0, g_list_length (columns) + 1));

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
					   (gdouble)g_value_get_int (property->value));
	}
}

static void
combo_changed (GtkWidget           *combo,
	       GladeEditorProperty *eprop)
{
	GValue val = { 0, };

	if (eprop->loading) return;

	g_value_init (&val, G_TYPE_INT);
	g_value_set_int (&val, (gint)gtk_combo_box_get_active (GTK_COMBO_BOX (combo)) - 1);
	glade_editor_property_commit (eprop, &val);
	g_value_unset (&val);
}


static void
spin_changed (GtkWidget           *spin,
	      GladeEditorProperty *eprop)
{
	GValue val = { 0, };

	if (eprop->loading) return;

	g_value_init (&val, G_TYPE_INT);
	g_value_set_int (&val, gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin)));
	glade_editor_property_commit (eprop, &val);
	g_value_unset (&val);
}

static void
combo_popup_down (GtkWidget *combo,
		  GParamSpec *spec,
		  GtkCellRenderer *cell)
{
	gboolean popup_shown = FALSE;

	g_object_get (G_OBJECT (combo), "popup-shown", &popup_shown, NULL);

	g_object_ref (cell);

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo));

	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell,
					"text", 0, NULL);

	g_object_unref (cell);

	if (popup_shown)
		g_object_set (cell, 
			      "ellipsize", PANGO_ELLIPSIZE_NONE,
			      "width", -1, 
			      NULL);
	else
		g_object_set (cell, 
			      "ellipsize", PANGO_ELLIPSIZE_END,
			      "width", 60, 
			      NULL);
}

static GtkWidget *
glade_eprop_cell_attribute_create_input (GladeEditorProperty *eprop)
{
	GladeEPropCellAttribute *eprop_attribute = GLADE_EPROP_CELL_ATTRIBUTE (eprop);
	GtkWidget *hbox;
	GtkAdjustment *adjustment;
	GtkCellRenderer *cell;

	hbox = gtk_hbox_new (FALSE, 2);

	adjustment = glade_property_class_make_adjustment (eprop->klass);
	eprop_attribute->spin = gtk_spin_button_new (adjustment, 1.0, 0);

	eprop_attribute->columns = (GtkTreeModel *)gtk_list_store_new (1, G_TYPE_STRING);
	eprop_attribute->combo   = gtk_combo_box_new_with_model (eprop_attribute->columns);

	/* Add cell renderer */
	cell = gtk_cell_renderer_text_new ();
	g_object_set (cell,
		      "xpad", 0,
		      "xalign", 0.0F,
 		      "ellipsize", PANGO_ELLIPSIZE_END,
		      "width", 60,
		      NULL);

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (eprop_attribute->combo));

	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (eprop_attribute->combo), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (eprop_attribute->combo), cell,
					"text", 0, NULL);

	g_signal_connect (G_OBJECT (eprop_attribute->combo), "notify::popup-shown",
			  G_CALLBACK (combo_popup_down), cell);

 	gtk_box_pack_start (GTK_BOX (hbox), eprop_attribute->spin, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), eprop_attribute->combo, FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (eprop_attribute->combo), "changed",
			  G_CALLBACK (combo_changed), eprop);
	g_signal_connect (G_OBJECT (eprop_attribute->spin), "value-changed",
			  G_CALLBACK (spin_changed), eprop);

	return hbox;
}
