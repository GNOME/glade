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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Chema Celorio <chema@celorio.com>
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h> /* for atoi and atof */
#include <string.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-choice.h"
#include "glade-editor.h"
#include "glade-signal-editor.h"
#include "glade-parameter.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-command.h"
#include "glade-debug.h"
#include "glade-menu-editor.h"
#include "glade-project.h"
#include "glade-utils.h"

static void glade_editor_class_init (GladeEditorClass *class);
static void glade_editor_init (GladeEditor *editor);

enum
{
	ADD_SIGNAL,
	LAST_SIGNAL
};


static guint glade_editor_signals[LAST_SIGNAL] = {0};

static GtkNotebookClass *parent_class = NULL;

/* We use this function recursively so we need to declare it */
static gboolean glade_editor_table_append_items (GladeEditorTable *table,
						 GladeWidgetClass *class,
						 GList **list,
						 gboolean common);


static void glade_editor_property_load_flags (GladeEditorProperty *property);

/* marshallers */

static void
glade_editor_marshal_VOID__STRING_ULONG_UINT_STRING (GClosure     *closure,
						     GValue       *return_value,
						     guint         n_param_values,
						     const GValue *param_values,
						     gpointer      invocation_hint,
						     gpointer      marshal_data)
{
	typedef void (*GMarshalFunc_VOID__STRING_ULONG_UINT_STRING) (gpointer     data1,
								     gpointer     arg_1,
								     gulong       arg_2,
								     guint        arg_3,
								     gpointer     arg_4,
								     gpointer     data2);
	GMarshalFunc_VOID__STRING_ULONG_UINT_STRING callback;
	GCClosure *cc = (GCClosure*) closure;
	gpointer data1, data2;

	g_return_if_fail (n_param_values == 5);

	if (G_CCLOSURE_SWAP_DATA (closure))
	{
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	}
	else
	{
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}
	callback = (GMarshalFunc_VOID__STRING_ULONG_UINT_STRING) (marshal_data ? marshal_data : cc->callback);

	callback (data1,
		  (char*) g_value_get_string (param_values + 1),
		  g_value_get_ulong (param_values + 2),
		  g_value_get_uint (param_values + 3),
		  (char*) g_value_get_string (param_values + 4),
		  data2);
}

/**
 * glade_editor_get_type:
 *
 * Creates the typecode for the #GladeEditor object type.
 *
 * Returns: the typecode for the #GladeEditor object type
 */
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

		type = g_type_register_static (GTK_TYPE_NOTEBOOK, "GladeEditor", &info, 0);
	}

	return type;
}

static void
glade_editor_class_init (GladeEditorClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	glade_editor_signals[ADD_SIGNAL] =
		g_signal_new ("add_signal",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeEditorClass, add_signal),
			      NULL, NULL,
			      glade_editor_marshal_VOID__STRING_ULONG_UINT_STRING,
			      G_TYPE_NONE,
			      4,
			      G_TYPE_STRING, G_TYPE_ULONG, G_TYPE_UINT, G_TYPE_STRING);

	class->add_signal = NULL;
}

/**
 * glade_editor_notebook_page:
 * @name:
 * @notebook:
 *
 * TODO: write me
 *
 * Returns:
 */
GtkWidget *
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
	gtk_viewport_set_shadow_type (GTK_VIEWPORT (GTK_BIN (scrolled_window)->child),
				      GTK_SHADOW_NONE);

	label = gtk_label_new (name);
	gtk_notebook_insert_page (GTK_NOTEBOOK (notebook), scrolled_window, label, page++);

	return vbox;
}
	
static void
glade_editor_init (GladeEditor *editor)
{
	editor->tooltips = gtk_tooltips_new ();

	editor->vbox_widget  = glade_editor_notebook_page (_("Widget"), GTK_WIDGET (editor));
	editor->vbox_packing = glade_editor_notebook_page (_("Packing"), GTK_WIDGET (editor));
	editor->vbox_common  = glade_editor_notebook_page (_("Common"), GTK_WIDGET (editor));
	editor->vbox_signals = glade_editor_notebook_page (_("Signals"), GTK_WIDGET (editor));
	editor->widget_tables = NULL;
	editor->loading = FALSE;
}

/**
 * glade_editor_new:
 *
 * Returns: a new #GladeEditor
 */
GladeEditor *
glade_editor_new ()
{
	GladeEditor *editor;

	editor = g_object_new (GLADE_TYPE_EDITOR, NULL);

	gtk_widget_set_size_request (GTK_WIDGET (editor), 350, 450);
	
	return editor;
}

/**
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
	
	if (editor->loading)
		return;

	g_return_if_fail (GTK_IS_WIDGET (editor->loaded_widget->widget));

	widget = editor->loaded_widget;
	new_name = gtk_editable_get_chars (GTK_EDITABLE (editable), 0, -1);
	glade_command_set_name (widget, new_name);
	g_free (new_name);
}

/* ============================ Property Changed ============================ */
static void
glade_editor_property_changed_text_common (GladeProperty *property,
					   const gchar *text)
{
	GValue val = { 0, };

	g_value_init (&val, G_TYPE_STRING);
	g_value_set_string (&val, text);

	glade_command_set_property (property, &val);

	g_value_unset (&val);
}

static void
glade_editor_property_changed_text (GtkWidget *entry,
				    GladeEditorProperty *property)
{
	gchar *text;

	g_return_if_fail (property != NULL);

	if (property->property->loading)
		return;
	
	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);

	glade_editor_property_changed_text_common (property->property, text);

	g_free (text);
}

static void
glade_editor_property_changed_text_view (GtkTextBuffer *buffer,
					 GladeEditorProperty *property)
{
	GtkTextIter start;
	GtkTextIter end;
	gchar *text;

	g_return_if_fail (property != NULL);

	if (property->property->loading)
		return;

	gtk_text_buffer_get_iter_at_offset (buffer, &start, 0);
	gtk_text_buffer_get_iter_at_offset (buffer, &end,
					    gtk_text_buffer_get_char_count (buffer));

	text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	glade_editor_property_changed_text_common (property->property, text);

	g_free (text);
}

static void
glade_editor_property_changed_enum (GtkWidget *menu_item,
				    GladeEditorProperty *property)
{
	GladeChoice *choice;
	GValue val = { 0, };
	GladeProperty *gproperty;

	g_return_if_fail (property != NULL);
	g_return_if_fail (property->property != NULL);

	if (property->property->loading)
		return;

	choice = g_object_get_data (G_OBJECT (menu_item), GLADE_ENUM_DATA_TAG);
	g_return_if_fail (choice != NULL);

	gproperty = property->property;

	g_value_init (&val, gproperty->class->def->g_type);
	g_value_set_enum (&val, choice->value);
	glade_command_set_property (gproperty, &val);

	g_value_unset (&val);
}

static void
glade_editor_property_changed_enabled (GtkWidget *button,
				       GladeEditorProperty *property)
{
	GtkBoxChild *child;
	GtkWidget *hbox;
	GtkWidget *spin;
	GtkWidget *widget1;
	GtkWidget *widget2;
	gboolean state;
	GList *list;

	g_return_if_fail (property != NULL);
	
	if (property->property->loading)
		return;

	/* Ok, this is hackish but i can't think of a better way
	 * since this is a special case i don't think it is necesarry
	 * to add a struct to hold this guys. Chema
	 */
	hbox = property->input;
	list = GTK_BOX (hbox)->children;
	child = (list->data);
	widget1 = child->widget;
	list = list->next;
	child = list->data;
	widget2 = child->widget;
	
	spin   = GTK_IS_SPIN_BUTTON (widget1) ? widget1 : widget2;
	g_return_if_fail (GTK_IS_SPIN_BUTTON (spin));
	
	state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	gtk_widget_set_sensitive (spin, state);
	property = g_object_get_data (G_OBJECT (button), "user_data");
	property->property->enabled = state;
}

static void
glade_editor_property_changed_numeric (GtkWidget *spin,
				       GladeEditorProperty *property)
{
	GladeEditorNumericType numeric_type;
	GValue val = { 0, };

	g_return_if_fail (property != NULL);
	g_return_if_fail (property->property != NULL);

	if (property->property->loading)
		return;

	numeric_type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (spin), "NumericType"));

	switch (numeric_type)
	{
	case GLADE_EDITOR_INTEGER:
		g_value_init (&val, G_TYPE_INT);
		g_value_set_int (&val, gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin)));
		break;
	case GLADE_EDITOR_FLOAT:
		g_value_init (&val, G_TYPE_FLOAT);
		g_value_set_float (&val, (gfloat) gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin)));
		break;
	case GLADE_EDITOR_DOUBLE:
		g_value_init (&val, G_TYPE_DOUBLE);
		g_value_set_double (&val, gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin)));
		break;
	default:
		g_warning ("Invalid numeric_type %i\n", numeric_type);
		return;
	}

	glade_command_set_property (property->property, &val);

	g_value_unset (&val);
}

static void
glade_editor_property_changed_boolean (GtkWidget *button,
				       GladeEditorProperty *property)
{
	const gchar *text[] = { GLADE_TAG_NO, GLADE_TAG_YES };
	GtkWidget *label;
	gboolean state;
	GValue val = { 0, };

	g_return_if_fail (property != NULL);

	if (property->property->loading)
		return;

	state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	label = GTK_BIN (button)->child;
	gtk_label_set_text (GTK_LABEL (label), text [state]);

	g_value_init (&val, G_TYPE_BOOLEAN);
	g_value_set_boolean (&val, state);
	glade_command_set_property (property->property, &val);

	g_value_unset (&val);
}

static void
glade_editor_property_changed_unichar (GtkWidget *entry,
				       GladeEditorProperty *property)
{
	GValue val = { 0, };
	guint len;
	gchar *text;
	gunichar unich;

	g_return_if_fail (property != NULL);

	if (property->property->loading)
		return;

	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	len = g_utf8_strlen (text, -1);
	g_debug (("The lenght of the string is: %d\n", len));
	unich = g_utf8_get_char (text);

	g_value_init (&val, G_TYPE_UINT);
	g_value_set_uint (&val, unich);
	glade_command_set_property (property->property, &val);

	g_value_unset (&val);
	g_free (text);
}

static void glade_editor_property_delete_unichar (GtkEditable *editable,
						  gint start_pos,
						  gint end_pos)
{
	gtk_editable_select_region (editable, 0, -1);
	g_signal_stop_emission_by_name (G_OBJECT (editable), "delete_text");
}

static void
glade_editor_property_insert_unichar (GtkWidget *entry,
				      const gchar *text,
				      gint length,
				      gint *position,
				      gpointer property)
{
	g_signal_handlers_block_by_func (G_OBJECT (entry),
					 G_CALLBACK (glade_editor_property_changed_unichar),
					 property);
	g_signal_handlers_block_by_func (G_OBJECT (entry),
					 G_CALLBACK (glade_editor_property_insert_unichar),
					 property);
	g_signal_handlers_block_by_func (G_OBJECT (entry),
					 G_CALLBACK (glade_editor_property_delete_unichar),
					 property);
	gtk_editable_delete_text (GTK_EDITABLE (entry), 0, -1);
	*position = 0;
	gtk_editable_insert_text (GTK_EDITABLE (entry), text, 1, position);
	g_signal_handlers_unblock_by_func (G_OBJECT (entry),
					   G_CALLBACK (glade_editor_property_changed_unichar),
					   property);
	g_signal_handlers_unblock_by_func (G_OBJECT (entry),
					   G_CALLBACK (glade_editor_property_insert_unichar),
					   property);
	g_signal_handlers_unblock_by_func (G_OBJECT (entry),
					   G_CALLBACK (glade_editor_property_delete_unichar),
					   property);
	g_signal_stop_emission_by_name (G_OBJECT (entry), "insert_text");
	glade_editor_property_changed_unichar (entry, property);
}

#define FLAGS_COLUMN_SETTING		0
#define FLAGS_COLUMN_SYMBOL		1

static void
flag_toggled (GtkCellRendererToggle *cell,
	      gchar                 *path_string,
	      GtkTreeModel          *model)
{
	GtkTreeIter iter;
	gboolean setting;

	gtk_tree_model_get_iter_from_string (model, &iter, path_string);

	gtk_tree_model_get (model, &iter,
			    FLAGS_COLUMN_SETTING, &setting,
			    -1);

	setting = setting ? FALSE : TRUE;

	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    FLAGS_COLUMN_SETTING, setting,
			    -1);
}


static void
glade_editor_property_show_flags_dialog (GtkWidget *entry,
					 GladeEditorProperty *property)
{
	GtkWidget *editor;
	GtkWidget *dialog;
	GtkWidget *scrolled_window;
	GtkListStore *model;
	GtkWidget *tree_view;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GFlagsClass *class;
	gint response_id;
	guint flag_num, value;

	g_return_if_fail (property != NULL);

	editor = gtk_widget_get_toplevel (entry);
	dialog = gtk_dialog_new_with_buttons (_("Set Flags"),
					      GTK_WINDOW (editor),
					      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK,
					      GTK_RESPONSE_OK,
					      NULL);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 300, 400);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_show (scrolled_window);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    scrolled_window, TRUE, TRUE, 0);

	/* Create the treeview using a simple list model with 2 columns. */
	model = gtk_list_store_new (2, G_TYPE_BOOLEAN, G_TYPE_STRING);

	tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);
	gtk_widget_show (tree_view);
	gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);

	column = gtk_tree_view_column_new ();

	renderer = gtk_cell_renderer_toggle_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "active", FLAGS_COLUMN_SETTING,
					     NULL);
	g_signal_connect (renderer, "toggled",
			  G_CALLBACK (flag_toggled), model);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "text", FLAGS_COLUMN_SYMBOL,
					     NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);


	/* Populate the model with the flags. */
	class = g_type_class_ref (G_VALUE_TYPE (property->property->value));
	value = g_value_get_flags (property->property->value);

	/* Step through each of the flags in the class. */
	for (flag_num = 0; flag_num < class->n_values; flag_num++) {
		GtkTreeIter iter;
		guint mask;
		gboolean setting;

		mask = class->values[flag_num].value;
		setting = ((value & mask) == mask) ? TRUE : FALSE;

		/* Add a row to represent the flag. */
		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter,
				    FLAGS_COLUMN_SETTING,
				    setting,
				    FLAGS_COLUMN_SYMBOL,
				    class->values[flag_num].value_name,
				    -1);
	}

	/* Run the dialog. */
	response_id = gtk_dialog_run (GTK_DIALOG (dialog));

	/* If the user selects OK, update the flags property. */
	if (response_id == GTK_RESPONSE_OK) {
		GtkTreeIter iter;
		guint new_value = 0;

		gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter);

		/* Step through each of the flags in the class, checking if
		   the corresponding toggle in the dialog is selected, If it
		   is, OR the flags' mask with the new value. */
		for (flag_num = 0; flag_num < class->n_values; flag_num++) {
			gboolean setting;

			gtk_tree_model_get (GTK_TREE_MODEL (model), &iter,
					    FLAGS_COLUMN_SETTING, &setting,
					    -1);

			if (setting)
				new_value |= class->values[flag_num].value;

			gtk_tree_model_iter_next (GTK_TREE_MODEL (model),
						  &iter);
		}

		/* If the new_value is different from the old value, we need
		   to update the property. */
		if (new_value != value) {
			GValue val = { 0, };

			g_value_init (&val, G_VALUE_TYPE (property->property->value));
			g_value_set_flags (&val, new_value);
			glade_command_set_property (property->property, &val);
			g_value_unset (&val);

			/* Update the entry in the property editor. */
			glade_editor_property_load_flags (property);
		}

	}

	g_type_class_unref (class);

	gtk_widget_destroy (dialog);
}

/* ============================= Create inputs ============================= */
static GtkWidget *
glade_editor_create_input_enum_item (GladeEditorProperty *property,
				     GladeChoice *choice)
{
	GtkWidget *menu_item;
	const gchar *name;

	name = choice->name;
	menu_item = gtk_menu_item_new_with_label (name);

	g_signal_connect (G_OBJECT (menu_item), "activate",
			  G_CALLBACK (glade_editor_property_changed_enum),
			  property);

	g_object_set_data (G_OBJECT (menu_item), GLADE_ENUM_DATA_TAG, choice);

	return menu_item;
}

static GtkWidget *
glade_editor_create_input_enum (GladeEditorProperty *property)
{
	GladeChoice *choice;
	GtkWidget *menu_item;
	GtkWidget *menu;
	GtkWidget *option_menu;
	GList *list;

	g_return_val_if_fail (property != NULL, NULL);

	menu = gtk_menu_new ();
	for (list = property->class->choices; list; list = list->next)
	{
		choice = (GladeChoice *)list->data;
		menu_item = glade_editor_create_input_enum_item (property, choice);
		gtk_widget_show (menu_item);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	}

	option_menu = gtk_option_menu_new ();
	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);

	return option_menu;
}

static GtkWidget *
glade_editor_create_input_flags (GladeEditorProperty *property) 
{
	GtkWidget *hbox;
	GtkWidget *entry;
	GtkWidget *button;

	hbox = gtk_hbox_new (FALSE, 0);

	entry = gtk_entry_new ();
	gtk_entry_set_editable (GTK_ENTRY (entry), FALSE);
	gtk_widget_show (entry);
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

	button = gtk_button_new_with_label ("...");
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), button,  FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_editor_property_show_flags_dialog),
			  property);

	return hbox;
}

static GtkWidget *
glade_editor_create_input_text (GladeEditorProperty *property)
{
	GladePropertyClass *class;
	gint lines = 1;

	g_return_val_if_fail (property != NULL, NULL);

	class = property->class;

	glade_parameter_get_integer (class->parameters, GLADE_TAG_VISIBLE_LINES, &lines);

	if (lines < 2) {
		GtkWidget *entry;
		
		entry = gtk_entry_new ();

		g_signal_connect (G_OBJECT (entry), "changed",
				  G_CALLBACK (glade_editor_property_changed_text),
				  property);

		return entry;
	} else {
		GtkTextBuffer *buffer;
		GtkWidget *view;

		view = gtk_text_view_new ();
		gtk_text_view_set_editable (GTK_TEXT_VIEW (view), TRUE);

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

		g_signal_connect_data (G_OBJECT (buffer), "changed",
				       G_CALLBACK (glade_editor_property_changed_text_view),
				       property, NULL, 0);
		
		return view;
	}

	return NULL;
}

static GtkWidget *
glade_editor_create_input_numeric (GladeEditorProperty *property,
				   GladeEditorNumericType numeric_type)
{
	GladePropertyClass *property_class;
	GtkAdjustment *adjustment;
	GtkWidget *spin;

	g_return_val_if_fail (property != NULL, NULL);

	property_class = property->class;

	adjustment = glade_parameter_adjustment_new (property_class);

	spin  = gtk_spin_button_new (adjustment, 4,
				     numeric_type == GLADE_EDITOR_INTEGER ? 0 : 2);

	g_object_set_data (G_OBJECT (spin), "NumericType", GINT_TO_POINTER (numeric_type));
	g_signal_connect (G_OBJECT (spin), "value_changed",
			  G_CALLBACK (glade_editor_property_changed_numeric),
			  property);

	/* Some numeric types are optional, for example the default window size, so
	 * they have a toggle button right next to the spin button. 
	 */
	if (property_class->optional) {
		GtkWidget *check;
		GtkWidget *hbox;
		check = gtk_check_button_new ();
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (hbox), spin,  TRUE, TRUE, 0);

		g_signal_connect (G_OBJECT (check), "toggled",
				  G_CALLBACK (glade_editor_property_changed_enabled),
				  property);
		return hbox;
	}

	return spin;
}

static GtkWidget *
glade_editor_create_input_integer (GladeEditorProperty *property)
{
	return glade_editor_create_input_numeric (property, GLADE_EDITOR_INTEGER);
}

static GtkWidget *
glade_editor_create_input_float (GladeEditorProperty *property)
{
	return glade_editor_create_input_numeric (property, GLADE_EDITOR_FLOAT);
}

static GtkWidget *
glade_editor_create_input_double (GladeEditorProperty *property)
{
	return glade_editor_create_input_numeric (property, GLADE_EDITOR_DOUBLE);
}

static GtkWidget *
glade_editor_create_input_boolean (GladeEditorProperty *property)
{
	GtkWidget *button;

	g_return_val_if_fail (property != NULL, NULL);

	button = gtk_toggle_button_new_with_label (_("No"));

	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (glade_editor_property_changed_boolean),
			  property);

	return button;
}

static GtkWidget *
glade_editor_create_input_unichar (GladeEditorProperty *property)
{
	GtkWidget *entry;

	g_return_val_if_fail (property != NULL, NULL);

	entry = gtk_entry_new ();
	/* it's 2 to prevent spirious beeps... */
	gtk_entry_set_max_length (GTK_ENTRY (entry), 2);
	
	g_signal_connect (G_OBJECT (entry), "changed",
			  G_CALLBACK (glade_editor_property_changed_unichar), property);
	g_signal_connect (G_OBJECT (entry), "insert_text",
			  G_CALLBACK (glade_editor_property_insert_unichar), property);
	g_signal_connect (G_OBJECT (entry), "delete_text",
			  G_CALLBACK (glade_editor_property_delete_unichar), property);

	return entry;
}

static GtkWidget *
glade_editor_create_input_object (GladeEditorProperty *property,
				  GladeEditorTable *table)
{
	g_return_val_if_fail (GLADE_IS_EDITOR_TABLE (table), NULL);
	g_return_val_if_fail (GLADE_IS_EDITOR_PROPERTY (property), NULL);

	glade_editor_table_append_items (table, property->class->child,
					 &property->children, FALSE);

	return NULL;
}

/**
 * glade_editor_create_item_labe:
 * @class:
 *
 * TODO: write me
 *
 * Returns:
 */
GtkWidget *
glade_editor_create_item_label (GladePropertyClass *class)
{
	GtkWidget *eventbox;
	GtkWidget *label;
	gchar *text;

	text = g_strdup_printf ("%s :", class->name);
	label = gtk_label_new (text);
	g_free (text);

	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);

	/* we need to wrap the label in an event box to add tooltips */
	eventbox = gtk_event_box_new ();
	gtk_container_add (GTK_CONTAINER (eventbox), label);
	glade_util_widget_set_tooltip (eventbox, class->tooltip);

	return eventbox;
}

static void
glade_editor_table_attach (GtkWidget *table, GtkWidget *child, gint pos, gint row)
{
	gtk_table_attach_defaults (GTK_TABLE (table), child,
				   pos, pos+1, row, row +1);

}

static GtkWidget *
glade_editor_append_item_real (GladeEditorTable *table,
			       GladeEditorProperty *property)
{
	GtkWidget *label;
	GtkWidget *input = NULL;

	g_return_val_if_fail (GLADE_IS_EDITOR_TABLE (table), NULL);
	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (property->class), NULL);

	switch (property->class->type) {
	case GLADE_PROPERTY_TYPE_BOOLEAN:
		input = glade_editor_create_input_boolean (property);
		break;
	case GLADE_PROPERTY_TYPE_FLOAT:
		input = glade_editor_create_input_float (property);
		break;
	case GLADE_PROPERTY_TYPE_INTEGER:
		input = glade_editor_create_input_integer (property);
		break;
	case GLADE_PROPERTY_TYPE_DOUBLE:
		input = glade_editor_create_input_double (property);
		break;
	case GLADE_PROPERTY_TYPE_STRING:
		input = glade_editor_create_input_text (property);
		break;
	case GLADE_PROPERTY_TYPE_ENUM:
		input = glade_editor_create_input_enum (property);
		break;
	case GLADE_PROPERTY_TYPE_FLAGS:
		input = glade_editor_create_input_flags (property);
		break;
	case GLADE_PROPERTY_TYPE_UNICHAR:
		input = glade_editor_create_input_unichar (property);
		break;
	case GLADE_PROPERTY_TYPE_OTHER_WIDGETS:
		g_warning ("The widget type %s does not have an input implemented\n",
			   property->class->name);
		break;
	case GLADE_PROPERTY_TYPE_OBJECT:
		glade_editor_create_input_object (property, table);
		/* We don't need to add an input for the object, the object
		 * has added its childs already
		 */
		return NULL;
	case GLADE_PROPERTY_TYPE_ERROR:
		return gtk_label_new ("Error !");
	default:
		g_warning ("This type %s does not have an input implemented\n",
			   property->class->name);
		return gtk_label_new ("Fix me !");
	}

	if (input == NULL) {
		g_warning ("Can't create an input widget for type %s\n",
			   property->class->name);
		return gtk_label_new ("Implement me !");
	}

	label = glade_editor_create_item_label (property->class);

	glade_editor_table_attach (table->table_widget, label, 0, table->rows);
	glade_editor_table_attach (table->table_widget, input, 1, table->rows);
	table->rows++;

	return input;
}

static GladeEditorProperty *
glade_editor_table_append_item (GladeEditorTable *table,
				GladePropertyClass *class)
{
	GladeEditorProperty *property;

	property = g_new0 (GladeEditorProperty, 1);

	property->class = class;
	property->children = NULL;

	property->input = glade_editor_append_item_real (table, property);
	property->property = NULL;

	return property;
}

static void
glade_editor_table_append_standard_fields (GladeEditorTable *table)
{
	GtkWidget *gtk_table;
	GtkWidget *label;
	GtkWidget *entry;

	gtk_table = table->table_widget;
	
	/* Name */
	label = gtk_label_new (_("Name :"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	entry = gtk_entry_new ();
	table->name_entry = entry;
	g_signal_connect (G_OBJECT (entry), "changed",
			  G_CALLBACK (glade_editor_widget_name_changed),
			  table->editor);

	glade_editor_table_attach (gtk_table, label, 0, table->rows);
	glade_editor_table_attach (gtk_table, entry, 1, table->rows);
	table->rows++;
	
	/* Class */
	label = gtk_label_new (_("Class :"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	entry = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry), table->glade_widget_class->name);
	gtk_editable_set_editable (GTK_EDITABLE (entry), FALSE);

	glade_editor_table_attach (gtk_table, label, 0, table->rows);
	glade_editor_table_attach (gtk_table, entry, 1, table->rows);
	table->rows++;
}

static gboolean
glade_editor_table_append_items (GladeEditorTable *table,
				 GladeWidgetClass *class,
				 GList **list_,
				 gboolean common)
{
	GladeEditorProperty *property;
	GladePropertyClass *property_class;
	GList *list;
	GList *new_list;

	new_list = *list_;

	list = class->properties;

	for (; list != NULL; list = list->next)
	{
		property_class = (GladePropertyClass *) list->data;
		if (!glade_property_class_is_visible (property_class, class))
			continue;
		if (common != property_class->common)
			continue;
		property = glade_editor_table_append_item (table, property_class);
		if (property != NULL)
			new_list = g_list_prepend (new_list, property);
	}

	*list_ = new_list;
	
	return TRUE;
}

static void
glade_editor_on_edit_menu_click (GtkButton *button, GladeEditor *editor)
{
	GtkWidget *menubar;
	GtkWidget *menu_editor;
	GladeProject *project;

	g_return_if_fail (GLADE_IS_EDITOR (editor));
	g_return_if_fail (editor->loaded_widget != NULL);

	menubar = editor->loaded_widget->widget;
	g_return_if_fail (GTK_IS_MENU_BAR (menubar));

	project = editor->loaded_widget->project;
	g_return_if_fail (project != NULL);

	menu_editor = glade_menu_editor_new (project, GTK_MENU_SHELL (menubar));
	gtk_widget_show (GTK_WIDGET (menu_editor));
}

#define GLADE_PROPERY_TABLE_ROW_SPACING 2

static GladeEditorTable *
glade_editor_table_new (void)
{
	GladeEditorTable *table;

	table = g_new0 (GladeEditorTable, 1);

	table->table_widget = gtk_table_new (0, 0, FALSE);
	table->properties = NULL;
	table->rows = 0;

	gtk_table_set_row_spacings (GTK_TABLE (table->table_widget),
				    GLADE_PROPERY_TABLE_ROW_SPACING);

	return table;
}

static GladeEditorTable *
glade_editor_table_create (GladeEditor *editor,
			   GladeWidgetClass *class,
			   gboolean common)
{
	GladeEditorTable *table;

	g_return_val_if_fail (GLADE_IS_EDITOR (editor), NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), NULL);

	table = glade_editor_table_new ();
	table->editor = editor;
	table->glade_widget_class = class;
	table->common = common;
	if (!common)
		glade_editor_table_append_standard_fields (table);
	if (!glade_editor_table_append_items (table, class, &table->properties, common))
		return NULL;

	/* Hack: We don't have currently a way to put a "Edit Menus..." button through the
	 * xml files. */
	if (!common && !strcmp (class->name, "GtkMenuBar")) {
		GtkWidget *edit_menu_button = gtk_button_new_with_label (_("Edit Menus..."));

		g_signal_connect (G_OBJECT (edit_menu_button), "clicked",
				  G_CALLBACK (glade_editor_on_edit_menu_click), editor);
		gtk_table_attach (GTK_TABLE (table->table_widget), edit_menu_button,
				  0, 2, table->rows, table->rows + 1,
				  GTK_EXPAND, 0, 0, 0);
		table->rows++;
	}

	gtk_widget_show_all (table->table_widget);
	editor->widget_tables = g_list_prepend (editor->widget_tables, table);

	return table;
}

static GladeEditorTable *
glade_editor_get_table_from_class (GladeEditor *editor,
				   GladeWidgetClass *class,
				   gboolean common)
{
	GladeEditorTable *table;
	GList *list;

	for (list = editor->widget_tables; list; list = list->next)
	{
		table = list->data;
		if (common != table->common)
			continue;
		if (table->glade_widget_class == class)
			return table;
	}

	table = glade_editor_table_create (editor, class, common);
	g_return_val_if_fail (table != NULL, NULL);

	return table;
}

static void
glade_editor_load_widget_page (GladeEditor *editor, GladeWidgetClass *class)
{
	GladeEditorTable *table;
	GtkContainer *container;
	GList *list;

	/* Remove the old table that was in this container */
	container = GTK_CONTAINER (editor->vbox_widget);
	list = gtk_container_get_children (container);
	for (; list; list = list->next) {
		GtkWidget *widget = list->data;
		g_return_if_fail (GTK_IS_WIDGET (widget));
		gtk_widget_ref (widget);
		gtk_container_remove (container, widget);
	}

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
	GList *list;

	/* Remove the old table that was in this container */
	container = GTK_CONTAINER (editor->vbox_common);
	list = gtk_container_get_children (container);
	for (; list; list = list->next) {
		GtkWidget *widget = list->data;
		g_return_if_fail (GTK_IS_WIDGET (widget));
		gtk_widget_ref (widget);
		gtk_container_remove (container, widget);
	}

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
		editor->signal_editor = glade_signal_editor_new (editor);
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

/* ============================ Load properties ============================ */
static void
glade_editor_property_set_tooltips (GladeEditorProperty *property)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->class != NULL);
	g_return_if_fail (property->input != NULL);

	glade_util_widget_set_tooltip (property->input, property->class->tooltip);

	return;
}

static void
glade_editor_property_load_integer (GladeEditorProperty *property)
{
	GtkWidget *spin = NULL;
	GList *parameters;
	gfloat val;

	g_return_if_fail (property != NULL);
	g_return_if_fail (property->property != NULL);
	g_return_if_fail (property->property->value != NULL);
	g_return_if_fail (property->input != NULL);
	
	parameters = property->property->class->parameters;

	if (property->property->class->optional) {
		GtkBoxChild *child;
		GtkWidget *widget1;
		GtkWidget *widget2;
		GtkWidget *button = NULL;
		GtkWidget *hbox;
		GList *list;

		hbox = property->input;
		list = GTK_BOX (hbox)->children;
		child = list->data;
		widget1 = child->widget;
		list = list->next;
		child = list->data;
		widget2 = child->widget;
		
		spin   = GTK_IS_SPIN_BUTTON (widget1) ? widget1 : widget2;
		button = GTK_IS_SPIN_BUTTON (widget1) ? widget2 : widget1;
		
		g_return_if_fail (GTK_IS_SPIN_BUTTON (spin));
		g_return_if_fail (GTK_IS_CHECK_BUTTON (button));

		gtk_widget_set_sensitive (GTK_WIDGET (spin), property->property->enabled);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
					      property->property->enabled);
		g_object_set_data (G_OBJECT (button), "user_data", property);
	} else {
		spin = property->input;
	}


	if (G_VALUE_TYPE (property->property->value) == G_TYPE_INT)
		val = (gfloat) g_value_get_int (property->property->value);
	else if (G_VALUE_TYPE (property->property->value) == G_TYPE_DOUBLE)
		val = (gfloat) g_value_get_double (property->property->value);
	else
		val = g_value_get_float (property->property->value);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), val);
	g_object_set_data (G_OBJECT (spin), "user_data", property);
}

static void
glade_editor_property_load_float (GladeEditorProperty *property)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->property != NULL);
	g_return_if_fail (property->property->value != NULL);
	g_return_if_fail (property->input != NULL);

	glade_editor_property_load_integer (property);
}

static void
glade_editor_property_load_double (GladeEditorProperty *property)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->property != NULL);
	g_return_if_fail (property->property->value != NULL);
	g_return_if_fail (property->input != NULL);

	glade_editor_property_load_integer (property);
}

static void
glade_editor_property_load_enum (GladeEditorProperty *property)
{
	GladePropertyClass *pclass;
	GladeChoice *choice;
	GList *list;
	gint idx = 0;

	g_return_if_fail (property != NULL);
	g_return_if_fail (property->property != NULL);
	g_return_if_fail (property->property->value != NULL);
	g_return_if_fail (property->input != NULL);
	
	pclass = property->property->class;

	list = pclass->choices;
	for (; list != NULL; list = list->next) {
		gint value = g_value_get_enum (property->property->value);
		choice = (GladeChoice *)list->data;
		if (choice->value == value)
			break;
		idx ++;
	}

	if (list == NULL)
		idx = 0;

	gtk_option_menu_set_history (GTK_OPTION_MENU (property->input), idx);

	list = GTK_MENU_SHELL (GTK_OPTION_MENU (property->input)->menu)->children;
	for (; list != NULL; list = list->next) {
		GtkMenuItem *menu_item = list->data;
		g_object_set_data (G_OBJECT (menu_item), "user_data",  property);
	}
}

static void
glade_editor_property_load_flags (GladeEditorProperty *property)
{
	GtkBoxChild *child;
	GtkWidget *entry;
	GValue tmp_value = { 0, };
	gchar *text;

	g_return_if_fail (property != NULL);
	g_return_if_fail (property->property != NULL);
	g_return_if_fail (property->property->value != NULL);
	g_return_if_fail (property->input != NULL);
	g_return_if_fail (GTK_IS_HBOX (property->input));

	/* The entry should be the first child. */
	child = GTK_BOX (property->input)->children->data;
	entry = child->widget;
	g_return_if_fail (GTK_IS_ENTRY (entry));

	/* Transform the GValue from flags to a string. */
	g_value_init (&tmp_value, G_TYPE_STRING);
	g_value_transform (property->property->value, &tmp_value);
	text = g_strescape (g_value_get_string (&tmp_value), NULL);
	g_value_unset (&tmp_value);

	gtk_entry_set_text (GTK_ENTRY (entry), text);
	g_free (text);
}

static void
glade_editor_property_load_other_widgets (GladeEditorProperty *property)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->property != NULL);
	g_return_if_fail (property->property->value != NULL);
	g_return_if_fail (property->input != NULL);

	g_warning ("Implement me (%s)!\n", G_GNUC_FUNCTION);
}

static void
glade_editor_property_load_boolean (GladeEditorProperty *property)
{
	GtkWidget *label;
	const gchar *text[] = { GLADE_TAG_NO, GLADE_TAG_YES };
	gboolean state;

	g_return_if_fail (property != NULL);
	g_return_if_fail (property->property != NULL);
	g_return_if_fail (property->property->value != NULL);
	g_return_if_fail (property->input != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (property->input));

	state = g_value_get_boolean (property->property->value);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (property->input), state);
	label = GTK_BIN (property->input)->child;
	g_return_if_fail (GTK_IS_LABEL (label));
	gtk_label_set_text (GTK_LABEL (label), text [state]);

	g_object_set_data (G_OBJECT (property->input), "user_data", property);
}

static void
glade_editor_property_load_text (GladeEditorProperty *property)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->property != NULL);
	g_return_if_fail (property->property->value != NULL);
	g_return_if_fail (property->input != NULL);

	if (GTK_IS_EDITABLE (property->input)) {
		GtkEditable *editable = GTK_EDITABLE (property->input);
		gint pos, insert_pos = 0;
		const gchar *text;
		text = g_value_get_string (property->property->value);
		pos = gtk_editable_get_position (editable);
 		gtk_editable_delete_text (editable, 0, -1);
		if (text)
			gtk_editable_insert_text (editable,
						  text,
						  g_utf8_strlen (text, -1),
						  &insert_pos);
		gtk_editable_set_position (editable, pos);
	} else if (GTK_IS_TEXT_VIEW (property->input)) {
		GtkTextBuffer *buffer;
		const gchar *text;

		text = g_value_get_string (property->property->value);
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (property->input));

		gtk_text_buffer_set_text (buffer,
					  text,
					  g_utf8_strlen (text, -1));
	} else {
		g_warning ("Invalid Text Widget type.");
	}

	g_object_set_data (G_OBJECT (property->input), "user_data", property);
}

static void
glade_editor_property_load_unichar (GladeEditorProperty *property)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->property != NULL);
	g_return_if_fail (property->property->value != NULL);
	g_return_if_fail (property->input != NULL);

	if (GTK_IS_EDITABLE (property->input)) {
		GtkEditable *editable = GTK_EDITABLE (property->input);
		gint insert_pos = 0;
		gunichar ch;
		gchar utf8st[6];

		ch = g_value_get_uint (property->property->value);

		g_unichar_to_utf8 (ch, utf8st);
 		gtk_editable_delete_text (editable, 0, -1);
		gtk_editable_insert_text (editable, utf8st, 1, &insert_pos);
	}

	g_object_set_data (G_OBJECT (property->input), "user_data", property);
}

/* We are recursing so we need the prototype beforehand. Don't you love C ? */
static void glade_editor_property_load (GladeEditorProperty *property, GladeWidget *widget);

static void
glade_editor_property_load_object (GladeEditorProperty *property,
				   GladeWidget *widget)
{
	GladeEditorProperty *child;
	GList *list;

	g_return_if_fail (property != NULL);

	list = property->children;
	for (; list != NULL; list = list->next) {
		child = list->data;
		glade_editor_property_load (child, widget);
	}
		
}

static void
glade_editor_property_load (GladeEditorProperty *property, GladeWidget *widget)
{
	GladePropertyClass *class = property->class;

	g_return_if_fail (GLADE_IS_EDITOR_PROPERTY (property));
	g_return_if_fail (GLADE_IS_PROPERTY_CLASS (property->class));

	if (class->type == GLADE_PROPERTY_TYPE_OBJECT) {
		glade_editor_property_load_object (property, widget);
		return;
	}

	property->property = glade_widget_get_property (widget, class->id);

	g_return_if_fail (property->property != NULL);
	g_return_if_fail (property->property->class == property->class);

	property->property->loading = TRUE;

	switch (class->type) {
	case GLADE_PROPERTY_TYPE_STRING:
		glade_editor_property_load_text (property);
		break;
	case GLADE_PROPERTY_TYPE_BOOLEAN:
		glade_editor_property_load_boolean (property);
		break;
	case GLADE_PROPERTY_TYPE_FLOAT:
		glade_editor_property_load_float (property);
		break;
	case GLADE_PROPERTY_TYPE_DOUBLE:
		glade_editor_property_load_double (property);
		break;
	case GLADE_PROPERTY_TYPE_INTEGER:
		glade_editor_property_load_integer (property);
		break;
	case GLADE_PROPERTY_TYPE_ENUM:
		glade_editor_property_load_enum (property);
		break;
	case GLADE_PROPERTY_TYPE_FLAGS:
		glade_editor_property_load_flags (property);
		break;
	case GLADE_PROPERTY_TYPE_UNICHAR:
		glade_editor_property_load_unichar (property);
		break;
	case GLADE_PROPERTY_TYPE_OTHER_WIDGETS:
		glade_editor_property_load_other_widgets (property);
		break;
	case GLADE_PROPERTY_TYPE_ERROR:
		g_warning ("%s : type %i not implemented\n", G_GNUC_FUNCTION,
			   class->type);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	glade_editor_property_set_tooltips (property);

	property->property->loading = FALSE;
}

static void
glade_editor_load_packing_page (GladeEditor *editor, GladeWidget *widget)
{
	static GladeEditorTable *old = NULL;
	static GList *old_props = NULL;
	GladeEditorProperty *editor_property;
	GladeEditorTable *table;
	GladePropertyClass *property_class;
	GladeWidget *parent;
	GladeWidget *ancestor;
	GtkContainer *container;
	GList *list;

	/* Remove the old properties */
	container = GTK_CONTAINER (editor->vbox_packing);
	list = gtk_container_get_children (container);
	for (; list; list = list->next)
	{
		GtkWidget *widget = list->data;
		g_return_if_fail (GTK_IS_WIDGET (widget));
		gtk_container_remove (container, widget);
	}

	/* Free the old structs */
	if (old)
		g_free (old);
	for (list = old_props; list; list = list->next)
		g_free (list->data);
	old_props = NULL;
	old = NULL;

	if (!widget)
		return;

	/* if the widget is a toplevel there are no packing properties */
	parent = glade_widget_get_parent (widget);
	if (!parent)
		return;

	/* Now add the new properties */
	table = glade_editor_table_new ();
	table->editor = editor;
	table->common = FALSE;
	table->packing = TRUE;

	for (ancestor = parent; ancestor != NULL; ancestor = glade_widget_get_parent (ancestor))
	{
 		for (list = ancestor->widget_class->child_properties; list; list = list->next)
		{
			property_class = list->data; 
			if (!ancestor->widget_class->child_property_applies (ancestor->widget, widget->widget, property_class->id))
				continue;

			g_assert (property_class->packing == TRUE);
			editor_property = glade_editor_table_append_item (table, property_class);
			old_props = g_list_prepend (old_props, editor_property);
			glade_editor_property_load (editor_property, widget);
		}
	}

	gtk_widget_show_all (table->table_widget);
	gtk_box_pack_start (GTK_BOX (editor->vbox_packing), table->table_widget,
                            FALSE, TRUE, 0);

	old = table;
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
	GladeWidgetClass *class;
	GladeEditorTable *table;
	GladeEditorProperty *property;
	GList *list;

	g_return_if_fail (GLADE_IS_EDITOR (editor));
	g_return_if_fail (widget == NULL || GLADE_IS_WIDGET (widget));

	if (editor->loaded_widget == widget)
		return;

	/* Load the GladeWidgetClass */
	class = widget ? widget->widget_class : NULL;
	if (editor->loaded_class != class)
		glade_editor_load_widget_class (editor, class);

	glade_editor_load_packing_page (editor, widget);
	glade_signal_editor_load_widget (editor->signal_editor, widget);

	/* we are just clearing, we are done */
	if (widget == NULL)
	{
		editor->loaded_widget = NULL;
		return;
	}

	editor->loading = TRUE;

	/* Load each GladeEditorProperty */
	table = glade_editor_get_table_from_class (editor, class, FALSE);
	gtk_entry_set_text (GTK_ENTRY (table->name_entry), widget->name);

	for (list = table->properties; list; list = list->next)
	{
		property = list->data;
		glade_editor_property_load (property, widget);
	}

	/* Load each GladeEditorProperty for the common tab */
	table = glade_editor_get_table_from_class (editor, class, TRUE);
	for (list = table->properties; list; list = list->next)
	{
		property = list->data;
		glade_editor_property_load (property, widget);
	}

	editor->loaded_widget = widget;
	editor->loading = FALSE;
}

/**
 * glade_editor_add_signal:
 * @editor: a #GladeEditor
 * @signal_id:
 * @callback_name:
 *
 * TODO: write me
 */
void
glade_editor_add_signal (GladeEditor *editor,
			 guint signal_id,
			 const char *callback_name)
{
	const char *widget_name;
	const char *signal_name;
	GType widget_type;

	g_return_if_fail (GLADE_IS_EDITOR (editor));
	g_return_if_fail (callback_name != NULL);
	g_return_if_fail (*callback_name != 0);

	signal_name = g_signal_name (signal_id);
	widget_name = glade_widget_get_name (editor->loaded_widget);
	widget_type = glade_widget_class_get_type (glade_widget_get_class (editor->loaded_widget));

	g_signal_emit (G_OBJECT (editor),
		       glade_editor_signals [ADD_SIGNAL], 0,
		       widget_name, widget_type, signal_id, callback_name);
}
