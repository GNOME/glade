/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 Ximian, Inc.
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
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <glib/gi18n-lib.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-editor.h"
#include "glade-signal-editor.h"
#include "glade-parameter.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-command.h"
#include "glade-debug.h"
#include "glade-marshallers.h"
#include "glade-project.h"
#include "glade-utils.h"
#include "glade-editor-property.h"

static GtkVBoxClass *parent_class = NULL;

static void glade_editor_reset_dialog (GladeEditor *editor);

static void
glade_editor_class_init (GladeEditorClass *class)
{
	parent_class = g_type_class_peek_parent (class);
}

static GtkWidget *
glade_editor_notebook_page (const gchar *name, GtkWidget *notebook)
{
	GtkWidget *vbox;
	GtkWidget *scrolled_window;
	GtkWidget *label;
	static gint page = 0;

	vbox = gtk_vbox_new (FALSE, 0);

	/* wrap the vbox in a scrolled window */
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_window),
					       GTK_WIDGET (vbox));
	gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 
					GLADE_GENERIC_BORDER_WIDTH);
	label = gtk_label_new_with_mnemonic (name);
	gtk_notebook_insert_page (GTK_NOTEBOOK (notebook), scrolled_window, label, page++);

	return vbox;
}

static void
glade_editor_on_reset_click (GtkButton *button,
			     GladeEditor *editor)
{
	glade_editor_reset_dialog (editor);
}

static void
glade_editor_on_launch_click (GtkButton *button,
			     GladeEditor *editor)
{
	glade_widget_launch_editor (editor->loaded_widget);
}

static void
glade_editor_init (GladeEditor *editor)
{
	GtkWidget *hbox, *button;

	editor->notebook     = gtk_notebook_new ();
	editor->vbox_widget  = glade_editor_notebook_page (_("_General"), GTK_WIDGET (editor->notebook));
	editor->vbox_packing = glade_editor_notebook_page (_("_Packing"), GTK_WIDGET (editor->notebook));
	editor->vbox_common  = glade_editor_notebook_page (_("_Common"), GTK_WIDGET (editor->notebook));
	editor->vbox_signals = glade_editor_notebook_page (_("_Signals"), GTK_WIDGET (editor->notebook));
	editor->widget_tables = NULL;
	editor->packing_etable = NULL;
	editor->loading = FALSE;

	gtk_box_pack_start (GTK_BOX (editor), editor->notebook, TRUE, TRUE, 0);

	hbox = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
	gtk_box_pack_start (GTK_BOX (editor), hbox, FALSE, FALSE, 0);

	/* Custom editor button
	 */
	editor->launch_button = gtk_button_new_with_mnemonic (_("_Edit..."));
	gtk_container_set_border_width (GTK_CONTAINER (editor->launch_button), 
					GLADE_GENERIC_BORDER_WIDTH);
	gtk_box_pack_start (GTK_BOX (hbox), editor->launch_button, FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (editor->launch_button), "clicked",
			  G_CALLBACK (glade_editor_on_launch_click), editor);

	/* Reset button
	 */
	button = gtk_button_new_with_mnemonic (_("_Reset..."));
	gtk_container_set_border_width (GTK_CONTAINER (button), 
					GLADE_GENERIC_BORDER_WIDTH);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_editor_on_reset_click), editor);
}

GType
glade_editor_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GladeEditorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) glade_editor_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (GladeEditor),
			0,
			(GInstanceInitFunc) glade_editor_init
		};

		type = g_type_register_static (GTK_TYPE_VBOX, "GladeEditor", &info, 0);
	}

	return type;
}

/**
 * glade_editor_new:
 *
 * Returns: a new #GladeEditor
 */
GladeEditor *
glade_editor_new (void)
{
	GladeEditor *editor;

	editor = g_object_new (GLADE_TYPE_EDITOR, NULL);
	
	return editor;
}

/*
 * We call this function when the user changes the widget name using the
 * entry on the properties editor.
 */
static void
glade_editor_widget_name_changed (GtkWidget *editable, GladeEditor *editor)
{
	GladeWidget *widget;
	gchar *new_name;
	
	g_return_if_fail (GTK_IS_EDITABLE (editable));
	g_return_if_fail (GLADE_IS_EDITOR (editor));
	
	if (editor->loading) return;

	widget = editor->loaded_widget;
	new_name = gtk_editable_get_chars (GTK_EDITABLE (editable), 0, -1);
	glade_command_set_name (widget, new_name);
	g_free (new_name);
}


static gboolean
glade_editor_widget_name_focus_out (GtkWidget           *entry,
				    GdkEventFocus       *event,
				    GladeEditor         *editor)
{
	glade_editor_widget_name_changed (entry, editor);
	return FALSE;
}

static void
glade_editor_table_attach (GtkWidget *table, GtkWidget *child, gint pos, gint row)
{
	gtk_table_attach (GTK_TABLE (table), child,
			  pos, pos+1, row, row +1,
			  pos ? GTK_EXPAND | GTK_FILL : GTK_FILL,
			  GTK_EXPAND | GTK_FILL,
			  2, 0);

}

static GladeEditorProperty *
glade_editor_table_append_item (GladeEditorTable *table,
				GladePropertyClass *class,
				gboolean from_query_dialog)
{
	GladeEditorProperty *property;

	property = glade_editor_property_new (class, from_query_dialog == FALSE);

	glade_editor_table_attach (table->table_widget, property->eventbox, 0, table->rows);
	glade_editor_table_attach (table->table_widget, GTK_WIDGET (property), 1, table->rows);

	table->rows++;

	return property;
}

static void
glade_editor_table_append_name_field (GladeEditorTable *table)
{
	GtkWidget *label;
	
	/* Name */
	label = gtk_label_new (_("Name :"));
	gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.0);

	table->name_entry = gtk_entry_new ();

	g_signal_connect (G_OBJECT (table->name_entry), "activate",
			  G_CALLBACK (glade_editor_widget_name_changed),
			  table->editor);
	
	g_signal_connect (G_OBJECT (table->name_entry), "focus-out-event",
			  G_CALLBACK (glade_editor_widget_name_focus_out),
			  table->editor);

	glade_editor_table_attach (table->table_widget, label, 0, table->rows);
	glade_editor_table_attach (table->table_widget, table->name_entry, 1, table->rows);

	table->rows++;
}

static void
glade_editor_table_append_class_field (GladeEditorTable *table)
{
	GtkWidget *label;
	GtkWidget *class_label;

	/* Class */
	label       = gtk_label_new (_("Class :"));
	class_label = gtk_label_new (table->glade_widget_class->name);
	gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.0);
	gtk_misc_set_alignment (GTK_MISC (class_label), 0.0, 0.0);

	glade_editor_table_attach (table->table_widget, label, 0, table->rows);
	glade_editor_table_attach (table->table_widget, class_label, 1, table->rows);

	table->rows++;
}

static gboolean
glade_editor_table_append_items (GladeEditorTable *table,
				 GladeWidgetClass *class,
				 GladeEditorTableType type)
{
	GladeEditorProperty *property;
	GladePropertyClass  *property_class;
	GList *list;

	for (list = class->properties; list != NULL; list = list->next)
	{
		property_class = (GladePropertyClass *) list->data;

		if (!glade_property_class_is_visible (property_class))
			continue;
		if (type == TABLE_TYPE_QUERY && !property_class->query)
			continue;
		else if (type == TABLE_TYPE_COMMON && !property_class->common)
			continue;
		else if (type == TABLE_TYPE_GENERAL && property_class->common)
			continue;

		property = glade_editor_table_append_item (table, property_class, 
							   type == TABLE_TYPE_QUERY);

		table->properties = g_list_prepend (table->properties, property);
	}
	return TRUE;
}

static GladeEditorTable *
glade_editor_table_new (void)
{
	GladeEditorTable *table;

	table = g_new0 (GladeEditorTable, 1);

	table->table_widget = gtk_table_new (0, 0, FALSE);

	g_object_ref (G_OBJECT(table->table_widget));

	return table;
}

static void
glade_editor_table_free (GladeEditorTable *etable)
{
	g_object_unref (G_OBJECT(etable->table_widget));
	g_free (etable);
}

static GladeEditorTable *
glade_editor_table_create (GladeEditor *editor,
			   GladeWidgetClass *class,
			   GladeEditorTableType type)
{
	GladeEditorTable *table;

	g_return_val_if_fail (GLADE_IS_EDITOR (editor), NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), NULL);

	table = glade_editor_table_new ();
	table->editor = editor;
	table->glade_widget_class = class;
	table->type = type;

	if (type == TABLE_TYPE_GENERAL)
	{
		glade_editor_table_append_name_field (table);
		glade_editor_table_append_class_field (table);
	}

	if (!glade_editor_table_append_items (table, class, type))
		return NULL;

	gtk_widget_show_all (table->table_widget);

	return table;
}

static GladeEditorTable *
glade_editor_get_table_from_class (GladeEditor *editor,
				   GladeWidgetClass *class,
				   GladeEditorTableType type)
{
	GladeEditorTable *table;
	GList *list;

	for (list = editor->widget_tables; list; list = list->next)
	{
		table = list->data;
		if (type != table->type)
			continue;
		if (table->glade_widget_class == class)
			return table;
	}

	table = glade_editor_table_create (editor, class, type);
	g_return_val_if_fail (table != NULL, NULL);

	editor->widget_tables = g_list_prepend (editor->widget_tables, table);

	return table;
}

static void
glade_editor_load_widget_page (GladeEditor *editor, GladeWidgetClass *class)
{
	GladeEditorTable *table;
	GtkContainer *container;
	GList *list, *children;

	/* Remove the old table that was in this container */
	container = GTK_CONTAINER (editor->vbox_widget);
	children = gtk_container_get_children (container);
	for (list = children; list; list = list->next) {
		GtkWidget *widget = list->data;
		g_return_if_fail (GTK_IS_WIDGET (widget));
		gtk_widget_ref (widget);
		gtk_container_remove (container, widget);
	}
	g_list_free (children);

	if (!class)
		return;

	table = glade_editor_get_table_from_class (editor, class, FALSE);

	/* Attach the new table */
	gtk_box_pack_start (GTK_BOX (editor->vbox_widget), table->table_widget,
			    FALSE, TRUE, 0);
}

static void
glade_editor_load_common_page (GladeEditor *editor, GladeWidgetClass *class)
{
	GladeEditorTable *table;
	GtkContainer *container;
	GList *list, *children;

	/* Remove the old table that was in this container */
	container = GTK_CONTAINER (editor->vbox_common);
	children = gtk_container_get_children (container);
	for (list = children; list; list = list->next) {
		GtkWidget *widget = list->data;
		g_return_if_fail (GTK_IS_WIDGET (widget));
		gtk_widget_ref (widget);
		gtk_container_remove (container, widget);
	}
	g_list_free (children);

	if (!class)
		return;
	
	table = glade_editor_get_table_from_class (editor, class, TRUE);

	/* Attach the new table */
	gtk_box_pack_start (GTK_BOX (editor->vbox_common), table->table_widget,
			    FALSE, TRUE, 0);
}

/**
 * glade_editor_update_widget_name:
 * @editor: a #GladeEditor
 *
 * TODO: write me
 */
void
glade_editor_update_widget_name (GladeEditor *editor)
{
	GladeEditorTable *table = glade_editor_get_table_from_class (editor, editor->loaded_class, FALSE);

	g_signal_handlers_block_by_func (G_OBJECT (table->name_entry), glade_editor_widget_name_changed, editor);
	gtk_entry_set_text (GTK_ENTRY (table->name_entry), editor->loaded_widget->name);
	g_signal_handlers_unblock_by_func (G_OBJECT (table->name_entry), glade_editor_widget_name_changed, editor);
}

static void
glade_editor_load_signal_page (GladeEditor *editor, GladeWidgetClass *class)
{
	if (editor->signal_editor == NULL) {
		editor->signal_editor = glade_signal_editor_new ((gpointer) editor);
		gtk_box_pack_start (GTK_BOX (editor->vbox_signals),
				    glade_signal_editor_get_widget (editor->signal_editor),
				    TRUE, TRUE, 0);
	}
}

static void
glade_editor_load_widget_class (GladeEditor *editor, GladeWidgetClass *class)
{
	glade_editor_load_widget_page  (editor, class);
	glade_editor_load_common_page  (editor, class);
	glade_editor_load_signal_page  (editor, class);
	
	editor->loaded_class = class;
}

static void
glade_editor_load_packing_page (GladeEditor *editor, GladeWidget *widget)
{
	GladeEditorProperty *editor_property;
	GladeProperty       *property;
	GladeWidget         *parent;
	GList               *list, *children;

	/* Remove the old properties */
	if ((children = gtk_container_get_children
	     (GTK_CONTAINER (editor->vbox_packing))) != NULL)
	{
		for (list = children; list; list = list->next) 
			gtk_container_remove (GTK_CONTAINER (editor->vbox_packing), 
					      GTK_WIDGET (list->data));
		g_list_free (children);
	}

	/* Free the packing editor table */
	if (editor->packing_etable)
		editor->packing_etable =
			(glade_editor_table_free (editor->packing_etable), NULL);

	/* Free the packing editor properties (we gave ownership to 
	 * packing_etable->table_widget, so no need to unref them here).
	 */
	editor->packing_eprops = (g_list_free (editor->packing_eprops), NULL);


	/* if the widget is a toplevel there are no packing properties */
	if (widget == NULL || (parent = glade_widget_get_parent (widget)) == NULL)
		return;

	/* Now add the new properties */
	editor->packing_etable         = glade_editor_table_new ();
	editor->packing_etable->editor = editor;
	editor->packing_etable->type   = TABLE_TYPE_PACKING;

	for (list = widget->packing_properties; list && list->data; list = list->next)
	{
		property               = GLADE_PROPERTY (list->data);
		editor_property        = glade_editor_table_append_item (editor->packing_etable, 
									 property->class, FALSE);
		editor->packing_eprops = g_list_prepend (editor->packing_eprops, editor_property);
		glade_editor_property_load (editor_property, property);
	}

	gtk_widget_show_all (editor->packing_etable->table_widget);

	gtk_box_pack_start (GTK_BOX (editor->vbox_packing), 
			    editor->packing_etable->table_widget,
                            FALSE, TRUE, 0);
}

static void
glade_editor_close_cb (GladeProject *project,
		       GladeEditor  *editor)
{
	/* Detected project we are viewing was closed,
	 * detatch from editor.
	 */
	glade_editor_load_widget (editor, NULL);
}

static void
glade_editor_load_widget_real (GladeEditor *editor, GladeWidget *widget)
{
	GladeWidgetClass *class;
	GladeEditorTable *table;
	GladeEditorProperty *property;
	GladeProject *project;
	GList *list;

	/* Disconnect from last widget */
	if (editor->loaded_widget != NULL)
	{
		project = glade_widget_get_project (editor->loaded_widget);
		g_signal_handler_disconnect (G_OBJECT (project),
					     editor->project_closed_signal_id);
	}	
	/* Load the GladeWidgetClass */
	class = widget ? widget->widget_class : NULL;
	if (editor->loaded_class != class || class == NULL)
		glade_editor_load_widget_class (editor, class);

	glade_editor_load_packing_page (editor, widget);
	glade_signal_editor_load_widget (editor->signal_editor, widget);


	if (widget && glade_widget_has_launcher (widget))
		gtk_widget_show (editor->launch_button);
	else
		gtk_widget_hide (editor->launch_button);


	/* we are just clearing, we are done */
	if (widget == NULL)
	{
		editor->loaded_widget = NULL;
		return;
	}

	editor->loading = TRUE;

	/* Load each GladeEditorProperty */
	table = glade_editor_get_table_from_class (editor, class, TABLE_TYPE_GENERAL);
	if (table->name_entry)
		gtk_entry_set_text (GTK_ENTRY (table->name_entry), widget->name);

	for (list = table->properties; list; list = list->next)
	{
		property = list->data;
		glade_editor_property_load_by_widget (property, widget);
	}

	/* Load each GladeEditorProperty for the common tab */
	table = glade_editor_get_table_from_class (editor, class, TABLE_TYPE_COMMON);
	for (list = table->properties; list; list = list->next)
	{
		property = list->data;
		glade_editor_property_load_by_widget (property, widget);
	}

	editor->loaded_widget = widget;
	editor->loading = FALSE;

	/* Connect to new widget */
	project = glade_widget_get_project (editor->loaded_widget);
	editor->project_closed_signal_id =
		g_signal_connect (G_OBJECT (project), "close",
				  G_CALLBACK (glade_editor_close_cb), editor);
}

/**
 * glade_editor_load_widget:
 * @editor: a #GladeEditor
 * @widget: a #GladeWidget
 *
 * Load @widget into @editor. If @widget is %NULL, clear the editor.
 */
void
glade_editor_load_widget (GladeEditor *editor, GladeWidget *widget)
{
	g_return_if_fail (GLADE_IS_EDITOR (editor));
	g_return_if_fail (widget == NULL || GLADE_IS_WIDGET (widget));
	
	if (editor->loaded_widget == widget)
		return;

	glade_editor_load_widget_real (editor, widget);
}

/**
 * glade_editor_refresh:
 * @editor: a #GladeEditor
 *
 * Synchronize @editor with the currently loaded widget.
 */
void
glade_editor_refresh (GladeEditor *editor)
{
	g_return_if_fail (GLADE_IS_EDITOR (editor));
	glade_editor_load_widget_real (editor, editor->loaded_widget);
}

gboolean
glade_editor_query_dialog (GladeEditor *editor, GladeWidget *widget)
{
	GtkWidget           *dialog;
	GladeEditorTable    *table;
	gchar               *title;
	GList               *list;
	GladeEditorProperty *property;
	gint		     answer;
	gboolean	     retval = TRUE;

	title = g_strdup_printf (_("Create a %s"), widget->widget_class->name);

	dialog = gtk_dialog_new_with_buttons
		(title, NULL,
		 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT |
		 GTK_DIALOG_NO_SEPARATOR,
		 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		 GTK_STOCK_OK, GTK_RESPONSE_OK,
		 NULL);
	g_free (title);

	table = glade_editor_get_table_from_class (editor,
						   widget->widget_class,
						   TABLE_TYPE_QUERY);


	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    table->table_widget,
			    TRUE, TRUE, 4);
	for (list = table->properties; list; list = list->next)
	{
		property = list->data;
		glade_editor_property_load_by_widget (property, widget);
	}

	answer = gtk_dialog_run (GTK_DIALOG (dialog));

	/*
	 * If user cancel's we cancel the whole "create operation" by
	 * return FALSE. glade_widget_new() will see the FALSE, and
	 * take care of canceling the "create" operation.
	 */
	if (answer == GTK_RESPONSE_CANCEL)
		retval = FALSE;

	gtk_container_remove (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox),
			      table->table_widget);
	
	gtk_widget_destroy (dialog);
	return retval;
}

enum {
	COLUMN_ENABLED = 0,
	COLUMN_PROP_NAME,
	COLUMN_PROPERTY,
	COLUMN_PARENT,
	COLUMN_CHILD,
	COLUMN_DEFAULT,
	COLUMN_NDEFAULT,
	COLUMN_DEFSTRING,
	NUM_COLUMNS
};


static void
glade_editor_reset_toggled (GtkCellRendererToggle *cell,
			    gchar                 *path_str,
			    GtkTreeModel          *model)
{
	GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
	GtkTreeIter  iter;
	gboolean     enabled;

	/* get toggled iter */
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    COLUMN_ENABLED,  &enabled, -1);
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			    COLUMN_ENABLED,  !enabled, -1);
	gtk_tree_path_free (path);
}

static GtkWidget *
glade_editor_reset_view (GladeEditor *editor)
{
	GtkWidget         *view_widget;
	GtkTreeModel      *model;
 	GtkCellRenderer   *renderer;
	GtkTreeViewColumn *column;

	model = (GtkTreeModel *)gtk_tree_store_new
		(NUM_COLUMNS,
		 G_TYPE_BOOLEAN,       /* Enabled  value      */
		 G_TYPE_STRING,        /* Property name       */
		 GLADE_TYPE_PROPERTY,  /* The property        */
		 G_TYPE_BOOLEAN,       /* Parent node ?       */
		 G_TYPE_BOOLEAN,       /* Child node ?        */
		 G_TYPE_BOOLEAN,       /* Has default value   */
		 G_TYPE_BOOLEAN,       /* Doesn't have defaut */
		 G_TYPE_STRING);       /* Default string      */

	view_widget = gtk_tree_view_new_with_model (model);
	g_object_set (G_OBJECT (view_widget), "enable-search", FALSE, NULL);
	
	/********************* fake invisible column *********************/
 	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), "editable", FALSE, "visible", FALSE, NULL);

	column = gtk_tree_view_column_new_with_attributes (NULL, renderer, NULL);
 	gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

	gtk_tree_view_column_set_visible (column, FALSE);
	gtk_tree_view_set_expander_column (GTK_TREE_VIEW (view_widget), column);
	
	/************************ enabled column ************************/
 	renderer = gtk_cell_renderer_toggle_new ();
	g_object_set (G_OBJECT (renderer), 
		      "mode",        GTK_CELL_RENDERER_MODE_ACTIVATABLE, 
		      "activatable", TRUE,
		      NULL);
	g_signal_connect (renderer, "toggled",
			  G_CALLBACK (glade_editor_reset_toggled), model);
	gtk_tree_view_insert_column_with_attributes 
		(GTK_TREE_VIEW (view_widget), COLUMN_ENABLED,
		 _("Reset"), renderer, 
		 "sensitive", COLUMN_NDEFAULT,
		 "activatable", COLUMN_NDEFAULT,
		 "active", COLUMN_ENABLED,
		 "visible", COLUMN_CHILD,
		 NULL);

	/********************* property name column *********************/
 	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), 
		      "editable", FALSE, 
		      "weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_tree_view_insert_column_with_attributes
		(GTK_TREE_VIEW (view_widget), COLUMN_PROP_NAME,
		 _("Property"), renderer, 
		 "text",       COLUMN_PROP_NAME, 
		 "weight-set", COLUMN_PARENT,
		 NULL);

	/******************* default indicator column *******************/
 	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), 
		      "editable", FALSE, 
		      "style", PANGO_STYLE_ITALIC,
		      "foreground", "Gray", NULL);
	gtk_tree_view_insert_column_with_attributes
		(GTK_TREE_VIEW (view_widget), COLUMN_DEFSTRING,
		 NULL, renderer, 
		 "text",    COLUMN_DEFSTRING, 
		 "visible", COLUMN_DEFAULT, 
		 NULL);

	return view_widget;
}

static void
glade_editor_populate_reset_view (GladeEditor *editor,
				  GtkTreeView *tree_view)
{
	GtkTreeStore  *model = (GtkTreeStore *)gtk_tree_view_get_model (tree_view);
	GtkTreeIter    property_iter, general_iter, common_iter, *iter;
	GList         *list;
	GladeProperty *property;
	gboolean       def;
	
	g_return_if_fail (editor->loaded_widget != NULL);

	gtk_tree_store_append (model, &general_iter, NULL);
	gtk_tree_store_set    (model, &general_iter,
			       COLUMN_PROP_NAME, _("General"),
			       COLUMN_PROPERTY,  NULL,
			       COLUMN_PARENT,    TRUE,
			       COLUMN_CHILD,     FALSE,
			       COLUMN_DEFAULT,   FALSE,
			       COLUMN_NDEFAULT,  FALSE,
			       -1);

	gtk_tree_store_append (model, &common_iter,  NULL);
	gtk_tree_store_set    (model, &common_iter,
			       COLUMN_PROP_NAME, _("Common"),
			       COLUMN_PROPERTY,  NULL,
			       COLUMN_PARENT,    TRUE,
			       COLUMN_CHILD,     FALSE,
			       COLUMN_DEFAULT,   FALSE,
			       COLUMN_NDEFAULT,  FALSE,
			       -1);

	/* General & Common */
	for (list = editor->loaded_widget->properties; list; list = list->next)
	{
		property = list->data;

		if (glade_property_class_is_visible (property->class) == FALSE)
			continue;
		
		if (property->class->common)
			iter = &common_iter;
		else
			iter = &general_iter;

	        def = glade_property_default (property);

		gtk_tree_store_append (model, &property_iter, iter);
		gtk_tree_store_set    (model, &property_iter,
				       COLUMN_ENABLED,   !def,
				       COLUMN_PROP_NAME, property->class->name,
				       COLUMN_PROPERTY,  property,
				       COLUMN_PARENT,    FALSE,
				       COLUMN_CHILD,     TRUE,
				       COLUMN_DEFAULT,   def,
				       COLUMN_NDEFAULT,  !def,
				       COLUMN_DEFSTRING, _("(default)"),
				       -1);
	}
}

static gboolean
glade_editor_reset_selection_changed_cb (GtkTreeSelection *selection,
					 GtkTextView      *desc_view)
{
	GtkTreeIter iter;
	GladeProperty *property = NULL;
	GtkTreeModel  *model = NULL;
	GtkTextBuffer *text_buffer;

	const gchar *message = 
		_("Select the properties that you want to reset to their default values");

	/* Find selected data and show property blurb here */
	if (gtk_tree_selection_get_selected (selection, &model, &iter))
	{
		text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (desc_view));
		gtk_tree_model_get (model, &iter, COLUMN_PROPERTY, &property, -1);
		gtk_text_buffer_set_text (text_buffer,
					  property ? glade_property_get_tooltip (property) : message,
					  -1);
		if (property)
			g_object_unref (G_OBJECT (property));
	}
	return TRUE;
}

static gboolean
glade_editor_reset_foreach_selection (GtkTreeModel *model,
				      GtkTreePath  *path,
				      GtkTreeIter  *iter,
				      gboolean      select)
{
	gboolean def;

	gtk_tree_model_get (model, iter, COLUMN_DEFAULT, &def, -1);
	/* Dont modify rows that are already default */
	if (def == FALSE)
		gtk_tree_store_set (GTK_TREE_STORE (model), iter,
				    COLUMN_ENABLED, select, -1);
	return FALSE;
}


static void
glade_editor_reset_select_all_clicked (GtkButton   *button,
				       GtkTreeView *tree_view)
{
	GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
	gtk_tree_model_foreach (model, (GtkTreeModelForeachFunc)
				glade_editor_reset_foreach_selection, 
				GINT_TO_POINTER (TRUE));
}

static void
glade_editor_reset_unselect_all_clicked (GtkButton   *button,
				       GtkTreeView *tree_view)
{
	GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
	gtk_tree_model_foreach (model, (GtkTreeModelForeachFunc)
				glade_editor_reset_foreach_selection, 
				GINT_TO_POINTER (FALSE));
}

static gboolean
glade_editor_reset_accumulate_selected_props  (GtkTreeModel  *model,
					       GtkTreePath   *path,
					       GtkTreeIter   *iter,
					       GList        **accum)
{
	GladeProperty *property;
	gboolean       enabled, def;

	gtk_tree_model_get (model, iter, 
			    COLUMN_PROPERTY, &property, 
			    COLUMN_ENABLED, &enabled,
			    COLUMN_DEFAULT, &def,
			    -1);

	if (property && enabled && !def)
		*accum = g_list_prepend (*accum, property);


	if (property)
		g_object_unref (G_OBJECT (property));

	return FALSE;
}

static GList *
glade_editor_reset_get_selected_props (GtkTreeModel *model)
{
	GList *ret = NULL;

	gtk_tree_model_foreach (model, (GtkTreeModelForeachFunc)
				glade_editor_reset_accumulate_selected_props, &ret);

	return ret;
}

static void
glade_editor_reset_properties (GList *props)
{
	GList         *list, *sdata_list = NULL;
	GCSetPropData *sdata;
	GladeProperty *prop;
	GladeProject  *project = NULL;

	for (list = props; list; list = list->next)
	{
		prop = list->data;

		project = glade_widget_get_project (prop->widget);

		sdata = g_new (GCSetPropData, 1);
		sdata->property = prop;

		sdata->old_value = g_new0 (GValue, 1);
		sdata->new_value = g_new0 (GValue, 1);

		glade_property_get_value   (prop, sdata->old_value);
		glade_property_get_default (prop, sdata->new_value);

		sdata_list = g_list_prepend (sdata_list, sdata);
	}

	if (project)
		/* GladeCommand takes ownership of allocated list, ugly but practicle */
		glade_command_set_properties_list (project, sdata_list);

}

static void
glade_editor_reset_dialog (GladeEditor *editor)
{
	GtkTreeSelection *selection;
	GtkWidget     *dialog, *parent;
	GtkWidget     *vbox, *hbox, *label, *sw, *button;
	GtkWidget     *tree_view, *description_view;
	gint           res;
	GList         *list;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (editor));
	dialog = gtk_dialog_new_with_buttons (_("Reset Widget Properties"),
					      GTK_WINDOW (parent),
					      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), GLADE_GENERIC_BORDER_WIDTH);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 0);

	/* Checklist */
	label = gtk_label_new_with_mnemonic (_("_Properties:"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (sw);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	gtk_widget_set_size_request (sw, 400, 200);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);


	tree_view = glade_editor_reset_view (editor);
	if (editor->loaded_widget)
		glade_editor_populate_reset_view (editor, GTK_TREE_VIEW (tree_view));
	gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

	gtk_widget_show (tree_view);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), tree_view);
	gtk_container_add (GTK_CONTAINER (sw), tree_view);

	/* Select all / Unselect all */
	hbox = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
	
	button = gtk_button_new_with_mnemonic (_("_Select All"));
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (button), GLADE_GENERIC_BORDER_WIDTH);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_editor_reset_select_all_clicked), tree_view);

	button = gtk_button_new_with_mnemonic (_("_Unselect All"));
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (button), GLADE_GENERIC_BORDER_WIDTH);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_editor_reset_unselect_all_clicked), tree_view);

	
	/* Description */
	label = gtk_label_new_with_mnemonic (_("Property _Description:"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (sw);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	gtk_widget_set_size_request (sw, 400, 80);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);

	description_view = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (description_view), FALSE);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (description_view), GTK_WRAP_WORD);

	gtk_widget_show (description_view);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), description_view);
	gtk_container_add (GTK_CONTAINER (sw), description_view);

	/* Update description */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	g_signal_connect (G_OBJECT (selection), "changed",
			  G_CALLBACK (glade_editor_reset_selection_changed_cb),
			  description_view);

	

	/* Run the dialog */
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_OK) {

		/* get all selected properties and reset properties through glade_command */
		if ((list = glade_editor_reset_get_selected_props 
		     (gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view)))) != NULL)
		{
			glade_editor_reset_properties (list);
			g_list_free (list);
		}
	}
	gtk_widget_destroy (dialog);
}
