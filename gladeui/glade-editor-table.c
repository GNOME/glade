/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

static void      glade_editor_table_init               (GladeEditorTable      *self);
static void      glade_editor_table_class_init         (GladeEditorTableClass *klass);
static void      glade_editor_table_dispose            (GObject               *object);
static void      glade_editor_table_editable_init      (GladeEditableIface    *iface);
static void      glade_editor_table_realize            (GtkWidget             *widget);
static void      glade_editor_table_grab_focus         (GtkWidget             *widget);

G_DEFINE_TYPE_WITH_CODE (GladeEditorTable, glade_editor_table, GTK_TYPE_TABLE,
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_editor_table_editable_init));


#define BLOCK_NAME_ENTRY_CB(table)					\
	do { if (table->name_entry)					\
			g_signal_handlers_block_by_func (G_OBJECT (table->name_entry), \
							 G_CALLBACK (widget_name_edited), table); \
	} while (0);

#define UNBLOCK_NAME_ENTRY_CB(table)					\
	do { if (table->name_entry)					\
			g_signal_handlers_unblock_by_func (G_OBJECT (table->name_entry), \
							   G_CALLBACK (widget_name_edited), table); \
	} while (0);


static void
glade_editor_table_class_init (GladeEditorTableClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose      = glade_editor_table_dispose;
	widget_class->realize      = glade_editor_table_realize;
	widget_class->grab_focus   = glade_editor_table_grab_focus;
}

static void
glade_editor_table_init (GladeEditorTable *self)
{
	self->group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
}

static void
glade_editor_table_dispose (GObject *object)
{
	GladeEditorTable *table = GLADE_EDITOR_TABLE (object);

	table->properties =
		(g_list_free (table->properties), NULL);

	/* the entry is finalized anyway, just avoid setting
	 * text in it from _load();
	 */
	table->name_entry = NULL;

	glade_editable_load (GLADE_EDITABLE (table), NULL);

	if (table->group) 
		g_object_unref (table->group);
	table->group = NULL;

	G_OBJECT_CLASS (glade_editor_table_parent_class)->dispose (object);
}


static void
glade_editor_table_realize (GtkWidget *widget)
{
	GladeEditorTable    *table = GLADE_EDITOR_TABLE (widget);
	GList               *list;
	GladeEditorProperty *property;

	GTK_WIDGET_CLASS (glade_editor_table_parent_class)->realize (widget);

	/* Sync up properties, even if widget is NULL */
	for (list = table->properties; list; list = list->next)
	{
		property = list->data;
		glade_editor_property_load_by_widget (property, table->loaded_widget);
	}
}

static void
glade_editor_table_grab_focus (GtkWidget *widget)
{
	GladeEditorTable *editor_table = GLADE_EDITOR_TABLE (widget);
	
	if (editor_table->name_entry && gtk_widget_get_mapped (editor_table->name_entry))
		gtk_widget_grab_focus (editor_table->name_entry);
	else if (editor_table->properties)
		gtk_widget_grab_focus (GTK_WIDGET (editor_table->properties->data));
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

	if (table->loaded_widget == NULL)
	{
		g_warning ("Name entry edited with no loaded widget in editor %p!\n",
			   table);
		return;
	}

	widget = table->loaded_widget;
	new_name = gtk_editable_get_chars (GTK_EDITABLE (editable), 0, -1);

	if (glade_project_available_widget_name (widget->project, widget, new_name))
		glade_command_set_name (widget, new_name);
	g_free (new_name);
}

static void
widget_name_changed (GladeWidget      *widget,
		     GParamSpec       *pspec,
		     GladeEditorTable *table)
{
	if (!gtk_widget_get_mapped (GTK_WIDGET (table)))
		return;

	if (table->name_entry)
	{	
		BLOCK_NAME_ENTRY_CB (table);
		gtk_entry_set_text (GTK_ENTRY (table->name_entry), table->loaded_widget->name);
		UNBLOCK_NAME_ENTRY_CB (table);
	}

}	

static void
widget_finalized (GladeEditorTable *table,
		  GladeWidget      *where_widget_was)
{
	table->loaded_widget = NULL;

	glade_editable_load (GLADE_EDITABLE (table), NULL);
}


static void
glade_editor_table_load (GladeEditable *editable,
			 GladeWidget   *widget)
{
	GladeEditorTable    *table = GLADE_EDITOR_TABLE (editable);
	GladeEditorProperty *property;
	GList               *list;

	/* abort mission */
	if (table->loaded_widget == widget)
		return;

	if (table->loaded_widget)
	{
		g_signal_handlers_disconnect_by_func (G_OBJECT (table->loaded_widget),
						      G_CALLBACK (widget_name_changed), table);

		/* The widget could die unexpectedly... */
		g_object_weak_unref (G_OBJECT (table->loaded_widget),
				     (GWeakNotify)widget_finalized,
				     table);
	}

	table->loaded_widget = widget;

	BLOCK_NAME_ENTRY_CB (table);

	if (table->loaded_widget)
	{
		g_signal_connect (G_OBJECT (table->loaded_widget), "notify::name",
				  G_CALLBACK (widget_name_changed), table);

		/* The widget could die unexpectedly... */
		g_object_weak_ref (G_OBJECT (table->loaded_widget),
				     (GWeakNotify)widget_finalized,
				     table);
		
		if (table->name_entry)
			gtk_entry_set_text (GTK_ENTRY (table->name_entry), widget->name);

	}
	else if (table->name_entry)
		gtk_entry_set_text (GTK_ENTRY (table->name_entry), "");

	UNBLOCK_NAME_ENTRY_CB (table);

	/* Sync up properties, even if widget is NULL */
	for (list = table->properties; list; list = list->next)
	{
		property = list->data;
		glade_editor_property_load_by_widget (property, widget);
	}
}

static void
glade_editor_table_set_show_name (GladeEditable *editable,
				  gboolean       show_name)
{
	GladeEditorTable *table = GLADE_EDITOR_TABLE (editable);

	if (table->name_label)
	{
		if (show_name)
		{
			gtk_widget_show (table->name_label);
			gtk_widget_show (table->name_entry);
		}
		else
		{
			gtk_widget_hide (table->name_label);
			gtk_widget_hide (table->name_entry);
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
glade_editor_table_attach (GladeEditorTable *table, 
			   GtkWidget *child, 
			   gint pos, gint row)
{
	gtk_table_attach (GTK_TABLE (table), child,
			  pos, pos+1, row, row +1,
			  pos ? 0 : GTK_EXPAND | GTK_FILL,
			  0,
			  3, 1);
	
	if (pos)
		gtk_size_group_add_widget (table->group, child);

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
	GList *l, *list = NULL, *properties;

	properties = (type == GLADE_PAGE_PACKING) ? adaptor->packing_props : adaptor->properties;

	for (l = properties; l; l = g_list_next (l))
	{
		GladePropertyClass *klass = l->data;

		/* Collect properties in our domain, query dialogs are allowed editor 
		 * invisible properties, allow adaptors to filter out properties from
		 * the GladeEditorTable using the "custom-layout" attribute.
		 */
		if ((!klass->custom_layout) && GLADE_PROPERTY_CLASS_IS_TYPE (klass, type) &&
		    (glade_property_class_is_visible (klass) || type == GLADE_PAGE_QUERY))
		{
			list = g_list_prepend (list, klass);
		}
			
	}
	return g_list_sort (list, property_class_comp);
}

static GladeEditorProperty *
append_item (GladeEditorTable *table,
	     GladePropertyClass *klass,
	     gboolean from_query_dialog)
{
	GladeEditorProperty *property;

	if (!(property = glade_widget_adaptor_create_eprop 
	      (GLADE_WIDGET_ADAPTOR (klass->handle), 
	       klass, from_query_dialog == FALSE)))
	{
		g_critical ("Unable to create editor for property '%s' of class '%s'",
			    klass->id, GLADE_WIDGET_ADAPTOR (klass->handle)->name);
		return NULL;
	}

	gtk_widget_show (GTK_WIDGET (property));
	gtk_widget_show_all (property->item_label);

	glade_editor_table_attach (table, property->item_label, 0, table->rows);
	glade_editor_table_attach (table, GTK_WIDGET (property), 1, table->rows);

	table->rows++;

	return property;
}


static void
append_items (GladeEditorTable     *table,
	      GladeWidgetAdaptor   *adaptor,
	      GladeEditorPageType   type)
{
	GladeEditorProperty *property;
	GladePropertyClass  *property_class;
	GList *list, *sorted_list;

	sorted_list = get_sorted_properties (adaptor, type);
	for (list = sorted_list; list != NULL; list = list->next)
	{
		property_class = (GladePropertyClass *) list->data;

		property = append_item (table, property_class, type == GLADE_PAGE_QUERY);
		table->properties = g_list_prepend (table->properties, property);
	}
	g_list_free (sorted_list);

	table->properties = g_list_reverse (table->properties);
}

static void
append_name_field (GladeEditorTable *table)
{
	gchar     *text = _("The Object's name");
	
	/* Name */
	table->name_label = gtk_label_new (_("Name:"));
	gtk_misc_set_alignment (GTK_MISC (table->name_label), 0.0, 0.5);
	gtk_widget_show (table->name_label);
	gtk_widget_set_no_show_all (table->name_label, TRUE);

	table->name_entry = gtk_entry_new ();
	gtk_widget_show (table->name_entry);
	gtk_widget_set_no_show_all (table->name_entry, TRUE);

	gtk_widget_set_tooltip_text (table->name_label, text);
	gtk_widget_set_tooltip_text (table->name_entry, text);

	g_signal_connect (G_OBJECT (table->name_entry), "activate",
			  G_CALLBACK (widget_name_edited), table);
	g_signal_connect (G_OBJECT (table->name_entry), "changed",
			  G_CALLBACK (widget_name_edited), table);

	glade_editor_table_attach (table, table->name_label, 0, table->rows);
	glade_editor_table_attach (table, table->name_entry, 1, table->rows);

	table->rows++;
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
glade_editor_table_new (GladeWidgetAdaptor   *adaptor,
			GladeEditorPageType   type)
{
	GladeEditorTable *table;

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

	table = g_object_new (GLADE_TYPE_EDITOR_TABLE, NULL);
	table->adaptor = adaptor;
	table->type = type;
	
	if (type == GLADE_PAGE_GENERAL)
		append_name_field (table);

	append_items (table, adaptor, type);

	gtk_widget_show (GTK_WIDGET (table));

	return GTK_WIDGET (table);
}
