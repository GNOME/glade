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
#include "glade-menu-editor.h"
#include "glade-project.h"
#include "glade-utils.h"
#include "glade-builtins.h"


static GtkVBoxClass *parent_class = NULL;

static GdkColor *insensitive_colour = NULL;
static GdkColor *normal_colour      = NULL;

static void glade_editor_property_load (GladeEditorProperty *property, GladeWidget *widget);

static void glade_editor_property_load_flags (GladeEditorProperty *property);
static void glade_editor_property_load_text (GladeEditorProperty *property);

static void glade_editor_reset_dialog (GladeEditor *editor);


static void
glade_editor_class_init (GladeEditorClass *class)
{
	parent_class = g_type_class_peek_parent (class);
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
glade_editor_init (GladeEditor *editor)
{
	GtkWidget *hbox, *button;

	editor->notebook     = gtk_notebook_new ();
	editor->vbox_widget  = glade_editor_notebook_page (_("_General"), GTK_WIDGET (editor->notebook));
	editor->vbox_packing = glade_editor_notebook_page (_("_Packing"), GTK_WIDGET (editor->notebook));
	editor->vbox_common  = glade_editor_notebook_page (_("_Common"), GTK_WIDGET (editor->notebook));
	editor->vbox_signals = glade_editor_notebook_page (_("_Signals"), GTK_WIDGET (editor->notebook));
	editor->widget_tables = NULL;
	editor->loading = FALSE;

	gtk_box_pack_start (GTK_BOX (editor), editor->notebook, TRUE, TRUE, 0);

	hbox = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
		
	gtk_box_pack_start (GTK_BOX (editor), hbox, FALSE, FALSE, 0);
		
	button = gtk_button_new_with_mnemonic (_("Set _Defaults..."));
	gtk_container_set_border_width (GTK_CONTAINER (button), 
					GLADE_GENERIC_BORDER_WIDTH);
		
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_editor_on_reset_click), editor);
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

/* ============================ Property Changed ============================ */
static void
glade_editor_property_changed_text_common (GladeProperty *property,
					   const gchar *text,
					   gboolean from_query_dialog)
{
	GValue val = { 0, };

	g_value_init (&val, G_TYPE_STRING);
	g_value_set_string (&val, text);

	if (from_query_dialog == TRUE)
		glade_property_set_value (property, &val);
	else 
		glade_command_set_property (property, &val);

	g_value_unset (&val);
}

static void
glade_editor_property_changed_text (GtkWidget           *entry,
				    GladeEditorProperty *property)
{
	gchar *text;

	g_return_if_fail (property != NULL);

	if (property->property->loading)
		return;
	
	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);

	glade_editor_property_changed_text_common (property->property, text,
						   property->from_query_dialog);

	g_free (text);
}

static gboolean
glade_editor_entry_focus_out (GtkWidget           *entry,
			      GdkEventFocus       *event,
			      GladeEditorProperty *property)
{
	glade_editor_property_changed_text (entry, property);
	return FALSE;
}

static gboolean
glade_editor_text_view_focus_out (GtkTextView         *view,
				  GdkEventFocus       *event,
				  GladeEditorProperty *property)
{
	gchar *text;
	GtkTextBuffer *buffer;
	GtkTextIter    start, end;
	
	g_return_val_if_fail (property != NULL, FALSE);

	if (property->property->loading)
		return FALSE;

	buffer = gtk_text_view_get_buffer (view);

	gtk_text_buffer_get_iter_at_offset (buffer, &start, 0);
	gtk_text_buffer_get_iter_at_offset (buffer, &end,
					    gtk_text_buffer_get_char_count (buffer));

	text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	glade_editor_property_changed_text_common (property->property, text,
						   property->from_query_dialog);

	g_free (text);
	return FALSE;
}

static void
glade_editor_property_changed_enum (GtkWidget *menu_item,
				    GladeEditorProperty *property)
{
	gint   ival;
	GValue val = { 0, };
	GladeProperty *gproperty;

	g_return_if_fail (property != NULL);
	g_return_if_fail (property->property != NULL);

	if (property->property->loading)
		return;

	ival = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (menu_item), GLADE_ENUM_DATA_TAG));

	gproperty = property->property;

	g_value_init (&val, gproperty->class->pspec->value_type);
	g_value_set_enum (&val, ival);

	if (property->from_query_dialog)
		glade_property_set_value (gproperty, &val);
	else
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
	glade_property_set_enabled (property->property, state);
}

static void
glade_editor_property_changed_numeric (GtkWidget *spin,
				       GladeEditorProperty *property)
{
	GValue val = { 0, };

	g_return_if_fail (property != NULL);
	g_return_if_fail (property->property != NULL);

	if (property->property->loading)
		return;

	g_value_init (&val, property->class->pspec->value_type);
	
	if (G_IS_PARAM_SPEC_INT(property->class->pspec))
		g_value_set_int (&val, gtk_spin_button_get_value_as_int
				 (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_UINT(property->class->pspec))
		g_value_set_uint (&val, gtk_spin_button_get_value_as_int
				  (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_LONG(property->class->pspec))
		g_value_set_long (&val, (glong)gtk_spin_button_get_value_as_int
				  (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_ULONG(property->class->pspec))
		g_value_set_ulong (&val, (gulong)gtk_spin_button_get_value_as_int
				   (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_INT64(property->class->pspec))
		g_value_set_int64 (&val, (gint64)gtk_spin_button_get_value_as_int
				  (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_UINT64(property->class->pspec))
		g_value_set_uint64 (&val, (guint64)gtk_spin_button_get_value_as_int
				   (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_FLOAT(property->class->pspec))
		g_value_set_float (&val, (gfloat) gtk_spin_button_get_value
				   (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_DOUBLE(property->class->pspec))
		g_value_set_double (&val, gtk_spin_button_get_value
				    (GTK_SPIN_BUTTON (spin)));
	else
		g_warning ("Unsupported type %s\n",
			   g_type_name(G_PARAM_SPEC_TYPE (property->class->pspec)));

	if (property->from_query_dialog)
		glade_property_set_value (property->property, &val);
	else
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

	if (property->from_query_dialog)
		glade_property_set_value (property->property, &val);
	else
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
	gunichar unich = '\0';

	g_return_if_fail (property != NULL);

	if (property->property->loading)
		return;

	if ((text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1)) != NULL)
	{
		len = g_utf8_strlen (text, -1);
		unich = g_utf8_get_char (text);
		g_free (text);
	}

	g_value_init (&val, G_TYPE_UINT);
	g_value_set_uint (&val, unich);

	if (property->from_query_dialog)
		glade_property_set_value (property->property, &val);
	else
		glade_command_set_property (property->property, &val);

	g_value_unset (&val);
}

static void
glade_editor_property_delete_unichar (GtkEditable *editable,
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
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 
					GLADE_GENERIC_BORDER_WIDTH);

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
		gchar *value_name;
		
		mask = class->values[flag_num].value;
		setting = ((value & mask) == mask) ? TRUE : FALSE;
		
		value_name = glade_property_class_get_displayable_value (property->class, 
									 class->values[flag_num].value);

		if (value_name == NULL) value_name = class->values[flag_num].value_name;
		
		/* Add a row to represent the flag. */
		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter,
				    FLAGS_COLUMN_SETTING,
				    setting,
				    FLAGS_COLUMN_SYMBOL,
				    value_name,
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

			if (property->from_query_dialog)
				glade_property_set_value (property->property, &val);
			else
				glade_command_set_property (property->property, &val);

			g_value_unset (&val);
		}

	}

	g_type_class_unref (class);

	gtk_widget_destroy (dialog);
}

static void
glade_editor_property_show_i18n_dialog (GtkWidget           *entry,
					GladeEditorProperty *property)
{
	GtkWidget     *editor;
	GtkWidget     *dialog;
	GtkWidget     *vbox, *hbox;
	GtkWidget     *label;
	GtkWidget     *sw;
	GtkWidget     *text_view, *comment_view;
	GtkTextBuffer *text_buffer, *comment_buffer;
	GtkWidget     *translatable_button, *context_button;
	const gchar   *text;
	gint           res;
	gchar         *str;

	g_return_if_fail (property != NULL);

	editor = gtk_widget_get_toplevel (entry);
	dialog = gtk_dialog_new_with_buttons (_("Edit Text Property"),
					      GTK_WINDOW (editor),
					      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), GLADE_GENERIC_BORDER_WIDTH);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 0);

	/* Text */
	label = gtk_label_new_with_mnemonic (_("_Text:"));
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

	text_view = gtk_text_view_new ();
	gtk_widget_show (text_view);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), text_view);
		
	gtk_container_add (GTK_CONTAINER (sw), text_view);

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));

	text = g_value_get_string (property->property->value);
	if (text)
	{
		gtk_text_buffer_set_text (text_buffer,
					  text,
					  -1);
	}
	
	/* Translatable and context prefix. */
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_widget_show (hbox);

	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	translatable_button = gtk_check_button_new_with_mnemonic (_("T_ranslatable"));
	gtk_widget_show (translatable_button);
	gtk_box_pack_start (GTK_BOX (hbox), translatable_button, FALSE, FALSE, 0);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (translatable_button),
				      glade_property_i18n_get_translatable (property->property));

	context_button = gtk_check_button_new_with_mnemonic (_("Has context _prefix"));
	gtk_widget_show (context_button);
	gtk_box_pack_start (GTK_BOX (hbox), context_button, FALSE, FALSE, 0);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (context_button),
				      glade_property_i18n_get_has_context (property->property));
	
	/* Comments. */
	label = gtk_label_new_with_mnemonic (_("Co_mments for translators:"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (sw);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);

	comment_view = gtk_text_view_new ();
	gtk_widget_show (comment_view);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), comment_view);

	gtk_container_add (GTK_CONTAINER (sw), comment_view);

	comment_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (comment_view));

	text = glade_property_i18n_get_comment (property->property);
	if (text)
	{
		gtk_text_buffer_set_text (comment_buffer,
					  text,
					  -1);
	}
	
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_OK) {
		GtkTextIter start, end;
		
		/* Get the new values. */
		glade_property_i18n_set_translatable (
			property->property,
			gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (translatable_button)));
		glade_property_i18n_set_has_context (
			property->property,
			gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (context_button)));

		/* Text */
		gtk_text_buffer_get_bounds (text_buffer, &start, &end);
		str = gtk_text_buffer_get_text (text_buffer, &start, &end, TRUE);
		if (str[0] == '\0')
		{
			g_free (str);
			str = NULL;
		}
		
		glade_editor_property_changed_text_common (property->property, str,
							   property->from_query_dialog);
		
		g_free (str);
		
		/* Comment */
		gtk_text_buffer_get_bounds (comment_buffer, &start, &end);
		str = gtk_text_buffer_get_text (comment_buffer, &start, &end, TRUE);
		if (str[0] == '\0')
		{
			g_free (str);
			str = NULL;
		}

		glade_property_i18n_set_comment (property->property, str);
		g_free (str);
	}

	gtk_widget_destroy (dialog);
}

/* ============================= Create inputs ============================= */
static GtkWidget *
glade_editor_create_input_enum_item (GladeEditorProperty *property,
				     const gchar         *name,
				     gint                 value)
{
	GtkWidget *menu_item;

	menu_item = gtk_menu_item_new_with_label (name);

	g_signal_connect (G_OBJECT (menu_item), "activate",
			  G_CALLBACK (glade_editor_property_changed_enum),
			  property);

	g_object_set_data (G_OBJECT (menu_item), GLADE_ENUM_DATA_TAG, GINT_TO_POINTER(value));

	return menu_item;
}

static GtkWidget *
glade_editor_create_input_stock_item (GladeEditorProperty *property,
				      const gchar         *id,
				      gint                 value)
{
	GtkWidget *menu_item;
	
	menu_item = gtk_image_menu_item_new_from_stock (id, NULL);

	g_signal_connect (G_OBJECT (menu_item), "activate",
			  G_CALLBACK (glade_editor_property_changed_enum),
			  property);

	g_object_set_data (G_OBJECT (menu_item), GLADE_ENUM_DATA_TAG, GINT_TO_POINTER(value));

	return menu_item;
}

static GtkWidget *
glade_editor_create_input_enum (GladeEditorProperty *property,
				gboolean             stock)
{
	GladePropertyClass  *class;
	GtkWidget   *menu_item;
	GtkWidget   *menu;
	GtkWidget   *option_menu;
	GEnumClass  *eclass;
	guint        i;
	
	g_return_val_if_fail (GLADE_IS_EDITOR_PROPERTY (property), NULL);
	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (property->class), NULL);

	class = property->class;
	g_return_val_if_fail ((eclass = g_type_class_ref
			       (class->pspec->value_type)) != NULL, NULL);


	menu = gtk_menu_new ();

	for (i = 0; i < eclass->n_values; i++)
	{
		gchar *value_name = 
			glade_property_class_get_displayable_value
			(class, eclass->values[i].value);
		if (value_name == NULL) value_name = eclass->values[i].value_name;
		
		if (stock && strcmp (eclass->values[i].value_nick, "glade-none"))
			menu_item = glade_editor_create_input_stock_item
				(property, 
				 eclass->values[i].value_nick, 
				 eclass->values[i].value);
		else
			menu_item = glade_editor_create_input_enum_item
				(property, value_name, eclass->values[i].value);

		gtk_widget_show (menu_item);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	}

	option_menu = gtk_option_menu_new ();
	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);

	g_type_class_unref (eclass);

	return option_menu;
}


static GtkWidget *
glade_editor_create_input_flags (GladeEditorProperty *property)
{
	GtkWidget *hbox;
	GtkWidget *entry;
	GtkWidget *button;

	g_return_val_if_fail (GLADE_IS_EDITOR_PROPERTY (property), NULL);

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
	GladePropertyClass  *class;

	g_return_val_if_fail (GLADE_IS_EDITOR_PROPERTY (property), NULL);
	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (property->class), NULL);

	class = property->class;

	if (class->visible_lines < 2) {
		GtkWidget *hbox;
		GtkWidget *entry;
		GtkWidget *button;

		hbox = gtk_hbox_new (FALSE, 0);

		entry = gtk_entry_new ();
		gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0); 

		g_signal_connect (G_OBJECT (entry), "activate",
				  G_CALLBACK (glade_editor_property_changed_text),
				  property);
		
		g_signal_connect (G_OBJECT (entry), "focus-out-event",
				  G_CALLBACK (glade_editor_entry_focus_out),
				  property);

		if (class->translatable) {
			button = gtk_button_new_with_label ("...");
			gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0); 

			g_signal_connect (button, "clicked",
					  G_CALLBACK (glade_editor_property_show_i18n_dialog),
					  property);
		}

		property->text_entry = entry;
		
		return hbox;
	} else {
		GtkWidget  *hbox;
		GtkWidget  *view;
		GtkWidget  *viewport;
		GtkWidget  *swindow;
		GtkWidget  *button;
		
		hbox = gtk_hbox_new (FALSE, 0);
		swindow = gtk_scrolled_window_new (NULL, NULL);
		viewport = gtk_viewport_new (NULL, NULL);
		view = gtk_text_view_new ();

		gtk_text_view_set_editable (GTK_TEXT_VIEW (view), TRUE);

		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
						GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow), GTK_SHADOW_IN);
		
		gtk_container_add (GTK_CONTAINER (viewport), view);
		gtk_container_add (GTK_CONTAINER (swindow), viewport);
		gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (swindow), TRUE, TRUE, 0); 

		g_signal_connect (G_OBJECT (view), "focus-out-event",
				  G_CALLBACK (glade_editor_text_view_focus_out),
				  property);
		
		if (class->translatable) {
			button = gtk_button_new_with_label ("...");
			gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0); 

			g_signal_connect (button, "clicked",
					  G_CALLBACK (glade_editor_property_show_i18n_dialog),
					  property);
		}

		property->text_entry = view;

		return hbox;
	}

	return NULL;
}

static GtkWidget *
glade_editor_create_input_numeric (GladeEditorProperty *property)
{
	GtkAdjustment *adjustment;
	GtkWidget *spin;

	g_return_val_if_fail (GLADE_IS_EDITOR_PROPERTY (property), NULL);
	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (property->class), NULL);
	
	adjustment = glade_property_class_make_adjustment (property->class);
	spin       = gtk_spin_button_new (adjustment, 4,
					  G_IS_PARAM_SPEC_FLOAT (property->class->pspec) ||
					  G_IS_PARAM_SPEC_DOUBLE (property->class->pspec)
					  ? 2 : 0);

	g_signal_connect (G_OBJECT (spin), "value_changed",
			  G_CALLBACK (glade_editor_property_changed_numeric),
			  property);

	/* Some numeric types are optional, for example the default window size, so
	 * they have a toggle button right next to the spin button. 
	 */
	if (property->class->optional) {
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
glade_editor_create_input_boolean (GladeEditorProperty *property)
{
	GtkWidget *button;

	g_return_val_if_fail (GLADE_IS_EDITOR_PROPERTY (property), NULL);

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

	g_return_val_if_fail (GLADE_IS_EDITOR_PROPERTY (property), NULL);

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

/**
 * glade_editor_create_item_label:
 * @class:
 *
 * TODO: write me
 *
 * Returns:
 */
GtkWidget *
glade_editor_create_item_label (GladeEditorProperty *property)
{
	GtkWidget *eventbox;
	gchar     *text;

	g_return_val_if_fail (GLADE_IS_EDITOR_PROPERTY (property), NULL);
	g_return_val_if_fail (property->class != NULL, NULL);

	text = g_strdup_printf ("%s :", property->class->name);
	property->item_label = gtk_label_new (text);
	g_free (text);

	gtk_misc_set_alignment (GTK_MISC (property->item_label), 1.0, 0.0);

	/* we need to wrap the label in an event box to add tooltips */
	eventbox = gtk_event_box_new ();
	gtk_container_add (GTK_CONTAINER (eventbox), property->item_label);

	if (insensitive_colour == NULL)
		insensitive_colour = 
			gdk_color_copy
			(&(GTK_WIDGET (property->item_label)->
			   style->fg[GTK_STATE_INSENSITIVE]));

	if (normal_colour == NULL)
		normal_colour = 
			gdk_color_copy
			(&(GTK_WIDGET (property->item_label)->
			   style->fg[GTK_STATE_NORMAL]));

	return eventbox;
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

static GtkWidget *
glade_editor_append_item_real (GladeEditorTable    *table,
			       GladeEditorProperty *property)
{
	GladePropertyClass  *class;
	GtkWidget           *input = NULL;

	g_return_val_if_fail (GLADE_IS_EDITOR_TABLE (table), NULL);
	g_return_val_if_fail (GLADE_IS_EDITOR_PROPERTY (property), NULL);
	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (property->class), NULL);

	class = property->class;

	if (class->pspec->value_type == GLADE_TYPE_STOCK)
		input = glade_editor_create_input_enum (property, TRUE);
	else if (G_IS_PARAM_SPEC_ENUM(class->pspec))
		input = glade_editor_create_input_enum (property, FALSE);
	else if (G_IS_PARAM_SPEC_FLAGS(class->pspec))
		input = glade_editor_create_input_flags (property);
	else if (G_IS_PARAM_SPEC_BOOLEAN(class->pspec))
		input = glade_editor_create_input_boolean (property);
	else if (G_IS_PARAM_SPEC_INT(class->pspec)    ||
		 G_IS_PARAM_SPEC_UINT(class->pspec)   ||
		 G_IS_PARAM_SPEC_LONG(class->pspec)   ||
		 G_IS_PARAM_SPEC_ULONG(class->pspec)  ||
		 G_IS_PARAM_SPEC_INT64(class->pspec)  ||
		 G_IS_PARAM_SPEC_UINT64(class->pspec) ||
		 G_IS_PARAM_SPEC_FLOAT(class->pspec)  ||
		 G_IS_PARAM_SPEC_DOUBLE(class->pspec))
		input = glade_editor_create_input_numeric (property);
	else if (G_IS_PARAM_SPEC_STRING(class->pspec))
		input = glade_editor_create_input_text (property);
	else if (G_IS_PARAM_SPEC_STRING(class->pspec))
		input = glade_editor_create_input_text (property);
	else if (G_IS_PARAM_SPEC_UNICHAR(class->pspec))
		input = glade_editor_create_input_unichar (property);
	
	if (input == NULL) {
		g_warning ("Can't create an input widget for type %s\n",
			   class->name);
		return gtk_label_new ("Implement me !");
	}

	property->eventbox = glade_editor_create_item_label (property);

	glade_editor_table_attach (table->table_widget, property->eventbox, 0, table->rows);
	glade_editor_table_attach (table->table_widget, input, 1, table->rows);
	table->rows++;

	return input;
}

static GladeEditorProperty *
glade_editor_table_append_item (GladeEditorTable *table,
				GladePropertyClass *class,
				gboolean from_query_dialog)
{
	GladeEditorProperty *property;

	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (class), NULL);
	property = g_new0 (GladeEditorProperty, 1);

	property->children = NULL;
	property->class    = class;
	property->property = NULL;

	/* Set this before creating the widget. */
	property->from_query_dialog = from_query_dialog;
	property->input = glade_editor_append_item_real (table, property);


	return property;
}

static void
glade_editor_table_append_name_field (GladeEditorTable *table)
{
	GtkWidget *gtk_table;
	GtkWidget *label;
	GtkWidget *entry;

	gtk_table = table->table_widget;
	
	/* Name */
	label = gtk_label_new (_("Name :"));
	gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.0);
	entry = gtk_entry_new ();
	table->name_entry = entry;

	g_signal_connect (G_OBJECT (entry), "activate",
			  G_CALLBACK (glade_editor_widget_name_changed),
			  table->editor);
	
	g_signal_connect (G_OBJECT (entry), "focus-out-event",
			  G_CALLBACK (glade_editor_widget_name_focus_out),
			  table->editor);
	
	glade_editor_table_attach (gtk_table, label, 0, table->rows);
	glade_editor_table_attach (gtk_table, entry, 1, table->rows);
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
				 GList **list_,
				 GladeEditorTableType type)
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

		if (!glade_property_class_is_visible (property_class))
			continue;
		if (type == TABLE_TYPE_QUERY && !property_class->query)
			continue;
		else if (type == TABLE_TYPE_COMMON && !property_class->common)
			continue;
		else if (type == TABLE_TYPE_GENERAL && property_class->common)
			continue;

		property = glade_editor_table_append_item (table, property_class, type == TABLE_TYPE_QUERY);
			
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

	menubar = GTK_WIDGET(editor->loaded_widget->object);
	g_return_if_fail (GTK_IS_MENU_BAR (menubar));

	project = GLADE_PROJECT (glade_widget_get_project (editor->loaded_widget));
	g_return_if_fail (project != NULL);

	menu_editor = glade_menu_editor_new (project, GTK_MENU_SHELL (menubar));
	gtk_widget_show (GTK_WIDGET (menu_editor));
}

#define GLADE_PROPERTY_TABLE_ROW_SPACING 2

static GladeEditorTable *
glade_editor_table_new (void)
{
	GladeEditorTable *table;

	table = g_new0 (GladeEditorTable, 1);

	table->table_widget = gtk_table_new (0, 0, FALSE);
	table->properties = NULL;
	table->rows = 0;

	gtk_table_set_row_spacings (GTK_TABLE (table->table_widget),
				    GLADE_PROPERTY_TABLE_ROW_SPACING);

	g_object_ref (G_OBJECT(table->table_widget));
	
	return table;
}

static GladeEditorTable *
glade_editor_table_create (GladeEditor *editor,
			   GladeWidgetClass *class,
			   GladeEditorTableType type)
{
	GladeEditorTable *table;
	GtkWidget        *hbox, *button;

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

	if (!glade_editor_table_append_items (table, class,
					      &table->properties, type))
		return NULL;

	if (type == TABLE_TYPE_GENERAL)
	{
		
		/* Hack: We don't have currently a way to put a "Edit Menus..." button through the
		 * xml files. */
		if (!strcmp (class->name, "GtkMenuBar")) {
			
			button = gtk_button_new_with_label (_("Edit Menus..."));
			gtk_container_set_border_width (GTK_CONTAINER (button), 
							GLADE_GENERIC_BORDER_WIDTH);
			
			gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);
			
			g_signal_connect (G_OBJECT (button), "clicked",
					  G_CALLBACK (glade_editor_on_edit_menu_click), editor);
			table->rows++;
		}
	}

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

/* ============================ Load properties ============================ */
static void
glade_editor_tooltip_cb (GladeProperty       *property,
			 const gchar         *tooltip,
			 GladeEditorProperty *editor_prop)
{
	glade_util_widget_set_tooltip (editor_prop->input, tooltip);
	glade_util_widget_set_tooltip (editor_prop->eventbox, tooltip);
}

static void
glade_editor_property_set_tooltips (GladeEditorProperty *property)
{
	gchar *tooltip;
	g_return_if_fail (property != NULL);
	g_return_if_fail (GLADE_IS_PROPERTY (property->property));
	g_return_if_fail (property->input != NULL);

	tooltip = (gchar *)glade_property_get_tooltip (property->property);
	glade_util_widget_set_tooltip (property->input, tooltip);
	glade_util_widget_set_tooltip (property->eventbox, tooltip);
 	return;
}

static void
glade_editor_property_load_integer (GladeEditorProperty *property)
{
	GladePropertyClass *class;
	GtkWidget          *spin = NULL;
	gfloat              val  = 0.0F;

	g_return_if_fail (property != NULL);
	g_return_if_fail (GLADE_IS_PROPERTY (property->property));
	g_return_if_fail (property->property->value != NULL);
	g_return_if_fail (property->input != NULL);

	class = property->class;

	if (class->optional) {
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

		gtk_widget_set_sensitive (GTK_WIDGET (spin), 
					  glade_property_get_enabled (property->property));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
					      glade_property_get_enabled (property->property));
		g_object_set_data (G_OBJECT (button), "user_data", property);
	} else {
		spin = property->input;
	}

	if (G_IS_PARAM_SPEC_INT(class->pspec))
		val = (gfloat) g_value_get_int (property->property->value);
	else if (G_IS_PARAM_SPEC_UINT(class->pspec))
		val = (gfloat) g_value_get_uint (property->property->value);		
	else if (G_IS_PARAM_SPEC_LONG(class->pspec))
		val = (gfloat) g_value_get_long (property->property->value);		
	else if (G_IS_PARAM_SPEC_ULONG(class->pspec))
		val = (gfloat) g_value_get_ulong (property->property->value);		
	else if (G_IS_PARAM_SPEC_INT64(class->pspec))
		val = (gfloat) g_value_get_int64 (property->property->value);		
	else if (G_IS_PARAM_SPEC_UINT64(class->pspec))
		val = (gfloat) g_value_get_uint64 (property->property->value);		
	else if (G_IS_PARAM_SPEC_DOUBLE(class->pspec))
		val = (gfloat) g_value_get_double (property->property->value);
	else if (G_IS_PARAM_SPEC_FLOAT(class->pspec))
		val = g_value_get_float (property->property->value);
	else
		g_warning ("Unsupported type %s\n",
			   g_type_name(G_PARAM_SPEC_TYPE (class->pspec)));

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), val);
	g_object_set_data (G_OBJECT (spin), "user_data", property);
}

static void
glade_editor_property_load_enum (GladeEditorProperty *property)
{
	GladePropertyClass *pclass;
	GEnumClass         *eclass;
	guint               i;
	gint                value;
	GList              *list;

	
	g_return_if_fail (property != NULL);
	g_return_if_fail (GLADE_IS_PROPERTY (property->property));
	g_return_if_fail (property->property->value != NULL);
	g_return_if_fail (property->input != NULL);
	g_return_if_fail ((eclass = g_type_class_ref
			   (property->class->pspec->value_type)) != NULL);
	
	pclass = property->class;

	value = g_value_get_enum (property->property->value);

	for (i = 0; i < eclass->n_values; i++)
	{
		if (eclass->values[i].value == value)
			break;
	}
	
	gtk_option_menu_set_history (GTK_OPTION_MENU (property->input),
				     i < eclass->n_values ? i : 0);

	list = GTK_MENU_SHELL (GTK_OPTION_MENU (property->input)->menu)->children;
	for (; list != NULL; list = list->next) {
		GtkMenuItem *menu_item = list->data;
		g_object_set_data (G_OBJECT (menu_item), "user_data",  property);
	}

	g_type_class_unref (eclass);
}

static void
glade_editor_property_load_flags (GladeEditorProperty *property)
{
	GtkBoxChild *child;
	GtkWidget *entry;
	gchar *text;

	g_return_if_fail (property != NULL);
	g_return_if_fail (GLADE_IS_PROPERTY (property->property));
	g_return_if_fail (property->property->value != NULL);
	g_return_if_fail (property->input != NULL);
	g_return_if_fail (GTK_IS_HBOX (property->input));

	/* The entry should be the first child. */
	child = GTK_BOX (property->input)->children->data;
	entry = child->widget;
	g_return_if_fail (GTK_IS_ENTRY (entry));

	text = glade_property_class_make_string_from_flags(property->class,
					g_value_get_flags(property->property->value),
					TRUE);
	
	gtk_entry_set_text (GTK_ENTRY (entry), text);
	g_free (text);
}

static void
glade_editor_property_load_boolean (GladeEditorProperty *property)
{
	GtkWidget *label;
	const gchar *text[] = { GLADE_TAG_NO, GLADE_TAG_YES };
	gboolean state;

	g_return_if_fail (property != NULL);
	g_return_if_fail (GLADE_IS_PROPERTY (property->property));
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
	g_return_if_fail (GLADE_IS_PROPERTY (property->property));
	g_return_if_fail (property->property->value != NULL);
	g_return_if_fail (property->input != NULL);

	
	if (GTK_IS_EDITABLE (property->text_entry)) {
		GtkEditable *editable = GTK_EDITABLE (property->text_entry);
		gint pos, insert_pos = 0;
		const gchar *text;
		text = g_value_get_string (property->property->value);
		pos = gtk_editable_get_position (editable);
 		gtk_editable_delete_text (editable, 0, -1);

		gtk_editable_insert_text (editable,
					  text ? text : "",
					  text ? g_utf8_strlen (text, -1) : 0,
					  &insert_pos);

		gtk_editable_set_position (editable, pos);
	} else if (GTK_IS_TEXT_VIEW (property->text_entry)) {
		GtkTextBuffer *buffer;
		const gchar *text;

		text = g_value_get_string (property->property->value);
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (property->text_entry));

		gtk_text_buffer_set_text (buffer,
					  text ? text : "",
					  text ? g_utf8_strlen (text, -1) : 0);

	} else {
		g_warning ("BUG! Invalid Text Widget type.");
	}

	g_object_set_data (G_OBJECT (property->input), "user_data", property);
}

static void
glade_editor_property_load_unichar (GladeEditorProperty *property)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (GLADE_IS_PROPERTY (property->property));
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

static void
glade_editor_sensitivity_cb (GladeProperty       *property,
			     GParamSpec          *pspec,
			     GladeEditorProperty *editor_prop)
{
	gboolean sensitive = glade_property_get_sensitive (editor_prop->property);

	gtk_widget_modify_fg 
		(GTK_WIDGET (editor_prop->item_label), 
		 GTK_STATE_NORMAL, 
		 sensitive ? normal_colour : insensitive_colour);
	gtk_widget_set_sensitive (editor_prop->input, sensitive);
}

static void
glade_editor_prop_value_changed_cb (GladeProperty       *property,
				    GValue              *value,
				    GladeEditorProperty *eprop)
{
	if (eprop->property == property)
		glade_editor_property_load (eprop, eprop->property->widget);
}

static void
glade_editor_prop_enabled_cb (GladeProperty       *property,
			      GParamSpec          *pspec,
			      GladeEditorProperty *eprop)
{
	if (eprop->property == property)
		glade_editor_property_load (eprop, eprop->property->widget);
}

static void
glade_editor_property_disconnect (GladeEditorProperty *property)
{
	/* This is a weak pointer */
	if (property->signal_prop == NULL) return;

	g_object_remove_weak_pointer (G_OBJECT (property->signal_prop),
				      (gpointer *)&property->signal_prop);
	
	if (property->tooltip_id > 0)
		g_signal_handler_disconnect (G_OBJECT (property->signal_prop),
					     property->tooltip_id);
	if (property->sensitive_id > 0)
		g_signal_handler_disconnect (property->signal_prop, 
					     property->sensitive_id);
	if (property->changed_id > 0)
		g_signal_handler_disconnect (property->signal_prop, 
					     property->changed_id);
	if (property->enabled_id > 0)
		g_signal_handler_disconnect (property->signal_prop, 
					     property->enabled_id);
}

static void
glade_editor_property_load (GladeEditorProperty *property, GladeWidget *widget)
{
	GladePropertyClass *class;
	gboolean            sensitive;

	g_return_if_fail (GLADE_IS_EDITOR_PROPERTY (property));
	g_return_if_fail (GLADE_IS_PROPERTY_CLASS (property->class));

	class = property->class;

	/* Load the property */
 	if ((property->property = 
	     glade_widget_get_property (widget, class->id)) == NULL)
 	{
 		g_critical ("Couldnt find property of class %s on widget of class %s\n",
 			    class->id, widget->widget_class->name);
		return;
 	}

	property->property->loading = TRUE;

	if (class->pspec->value_type == GLADE_TYPE_STOCK ||
	    G_IS_PARAM_SPEC_ENUM(class->pspec))
		glade_editor_property_load_enum (property);
	else if (G_IS_PARAM_SPEC_FLAGS(class->pspec))
		glade_editor_property_load_flags (property);
	else if (G_IS_PARAM_SPEC_STRING(class->pspec))
		glade_editor_property_load_text (property);
	else if (G_IS_PARAM_SPEC_BOOLEAN(class->pspec))
		glade_editor_property_load_boolean (property);
	else if (G_IS_PARAM_SPEC_FLOAT(class->pspec)  ||
		 G_IS_PARAM_SPEC_DOUBLE(class->pspec) ||
		 G_IS_PARAM_SPEC_INT(class->pspec)    ||
		 G_IS_PARAM_SPEC_UINT(class->pspec)   ||
		 G_IS_PARAM_SPEC_LONG(class->pspec)   ||
		 G_IS_PARAM_SPEC_ULONG(class->pspec)  ||
		 G_IS_PARAM_SPEC_INT64(class->pspec)  ||
		 G_IS_PARAM_SPEC_UINT64(class->pspec))
		glade_editor_property_load_integer (property);
	else if (G_IS_PARAM_SPEC_UNICHAR(class->pspec))
		glade_editor_property_load_unichar (property);
	else
		g_warning ("%s : type %s not implemented (%s)\n", G_GNUC_FUNCTION,
			   class->name, g_type_name (class->pspec->value_type));


	/* Set insensitive and hook cb here */
	if (property->signal_prop != property->property) {

		glade_editor_property_disconnect (property);

		property->tooltip_id   = 
			g_signal_connect (G_OBJECT (property->property),
					  "tooltip-changed", 
					  G_CALLBACK (glade_editor_tooltip_cb),
					  property);
		property->sensitive_id =
			g_signal_connect (G_OBJECT (property->property),
					  "notify::sensitive", 
					  G_CALLBACK (glade_editor_sensitivity_cb),
					  property);
		property->changed_id =
			g_signal_connect (G_OBJECT (property->property),
					  "value-changed", 
					  G_CALLBACK (glade_editor_prop_value_changed_cb),
					  property);
		property->enabled_id =
			g_signal_connect (G_OBJECT (property->property),
					  "notify::enabled", 
					  G_CALLBACK (glade_editor_prop_enabled_cb),
					  property);


		sensitive = glade_property_get_sensitive (property->property);
		gtk_widget_modify_fg
			(GTK_WIDGET (property->item_label), 
			 GTK_STATE_NORMAL, 
			 sensitive ? normal_colour : insensitive_colour);
		
		gtk_widget_set_sensitive (property->input, sensitive);
		
		glade_editor_property_set_tooltips (property);
		
		property->signal_prop = property->property;

		/* This is a weak pointer because sometimes we want to
		 * disconnect signals from a widget type that's creation
		 * was canceled in the query dialog.
		 */
		g_object_add_weak_pointer (G_OBJECT (property->signal_prop),
					   (gpointer *)&property->signal_prop);
	}

	property->property->loading = FALSE;
}

static void
glade_editor_load_packing_page (GladeEditor *editor, GladeWidget *widget)
{
	static GladeEditorTable *old = NULL;
	static GList *old_props = NULL;

	GladeEditorProperty *editor_property;
	GladeEditorTable    *table;
	GladeProperty       *property;
	GladeWidget         *parent;
	GtkContainer        *container;
	GList               *list, *children;

	/* Remove the old properties */
	container = GTK_CONTAINER (editor->vbox_packing);
	children = gtk_container_get_children (container);
	for (list = children; list; list = list->next) {
		GtkWidget *widget = list->data;
		g_return_if_fail (GTK_IS_WIDGET (widget));
		gtk_container_remove (container, widget);
	}
	g_list_free (children);

	/* Free the old structs */
	if (old)
	{
		g_object_unref (G_OBJECT (old->table_widget));
		g_free (old);
	}
	for (list = old_props; list; list = list->next)
	{
		glade_editor_property_disconnect ((GladeEditorProperty *)list->data);
		g_free (list->data);
	}
	old_props = NULL;
	old = NULL;

	if (!widget)
		return;

	/* if the widget is a toplevel there are no packing properties */
	if ((parent = glade_widget_get_parent (widget)) == NULL)
		return;

	/* Now add the new properties */
	table = glade_editor_table_new ();
	table->editor = editor;
	table->type   = TABLE_TYPE_PACKING;

	for (list = widget->packing_properties; list && list->data; list = list->next)
	{
		property = list->data;
		g_assert (property->class->packing == TRUE);
		editor_property = glade_editor_table_append_item (table, property->class, FALSE);
		old_props       = g_list_prepend (old_props, editor_property);
		glade_editor_property_load (editor_property, widget);
	}

	gtk_widget_show_all (table->table_widget);
	gtk_box_pack_start (GTK_BOX (editor->vbox_packing), table->table_widget,
                            FALSE, TRUE, 0);

	old = table;
}

static void
glade_editor_load_widget_real (GladeEditor *editor, GladeWidget *widget)
{
	GladeWidgetClass *class;
	GladeEditorTable *table;
	GladeEditorProperty *property;
	GList *list;

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
	table = glade_editor_get_table_from_class (editor, class, TABLE_TYPE_GENERAL);
	if (table->name_entry)
		gtk_entry_set_text (GTK_ENTRY (table->name_entry), widget->name);

	for (list = table->properties; list; list = list->next)
	{
		property = list->data;
		glade_editor_property_load (property, widget);
	}

	/* Load each GladeEditorProperty for the common tab */
	table = glade_editor_get_table_from_class (editor, class, TABLE_TYPE_COMMON);
	for (list = table->properties; list; list = list->next)
	{
		property = list->data;
		glade_editor_property_load (property, widget);
	}

	editor->loaded_widget = widget;
	editor->loading = FALSE;
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
	if (editor->loaded_widget)
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
		glade_editor_property_load (property, widget);
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


gboolean
glade_editor_editable_property (GParamSpec  *pspec)
{
	g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
	return 
		(G_IS_PARAM_SPEC_ENUM(pspec)    ||
		 G_IS_PARAM_SPEC_FLAGS(pspec)   ||
		 G_IS_PARAM_SPEC_STRING(pspec)  ||
		 G_IS_PARAM_SPEC_BOOLEAN(pspec) ||
		 G_IS_PARAM_SPEC_FLOAT(pspec)   ||
		 G_IS_PARAM_SPEC_DOUBLE(pspec)  ||
		 G_IS_PARAM_SPEC_INT(pspec)     ||
		 G_IS_PARAM_SPEC_UINT(pspec)    ||
		 G_IS_PARAM_SPEC_LONG(pspec)    ||
		 G_IS_PARAM_SPEC_ULONG(pspec)   ||
		 G_IS_PARAM_SPEC_INT64(pspec)   ||
		 G_IS_PARAM_SPEC_UINT64(pspec)  ||
		 G_IS_PARAM_SPEC_UNICHAR(pspec)) ? TRUE : FALSE;
}


enum {
	COLUMN_ENABLED = 0,
	COLUMN_PROP_NAME,
	COLUMN_PROPERTY,
	COLUMN_PARENT,
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

	model = (GtkTreeModel *)gtk_tree_store_new (NUM_COLUMNS,
						    G_TYPE_BOOLEAN,       /* Enabled  value      */
						    G_TYPE_STRING,        /* Property name       */
						    GLADE_TYPE_PROPERTY,  /* The property        */
						    G_TYPE_BOOLEAN,       /* Parent node ?       */
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
	GtkTreeIter    property_iter, general_iter, packing_iter, common_iter, *iter;
	GList         *list;
	GladeProperty *property;
	gboolean       def;
	
	g_return_if_fail (editor->loaded_widget != NULL);

	gtk_tree_store_append (model, &general_iter, NULL);
	gtk_tree_store_set    (model, &general_iter,
			       COLUMN_PROP_NAME, _("General"),
			       COLUMN_PROPERTY,  NULL,
			       COLUMN_PARENT,    TRUE,
			       COLUMN_DEFAULT,   FALSE,
			       COLUMN_NDEFAULT,  FALSE,
			       -1);

	gtk_tree_store_append (model, &common_iter,  NULL);
	gtk_tree_store_set    (model, &common_iter,
			       COLUMN_PROP_NAME, _("Common"),
			       COLUMN_PROPERTY,  NULL,
			       COLUMN_PARENT,    TRUE,
			       COLUMN_DEFAULT,   FALSE,
			       COLUMN_NDEFAULT,  FALSE,
			       -1);

	/* General & Common */
	for (list = editor->loaded_widget->properties; list; list = list->next)
	{
		property = list->data;
		
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
				       COLUMN_DEFAULT,   def,
				       COLUMN_NDEFAULT,  !def,
				       COLUMN_DEFSTRING, _("(default)"),
				       -1);
	}

	/* Packing */
	if (editor->loaded_widget->packing_properties)
	{
		gtk_tree_store_append (model, &packing_iter, NULL);
		gtk_tree_store_set    (model, &packing_iter,
				       COLUMN_PROP_NAME, _("Packing"),
				       COLUMN_PROPERTY,  NULL,
				       COLUMN_PARENT,    TRUE,
				       COLUMN_DEFAULT,   FALSE,
				       COLUMN_NDEFAULT,  FALSE,
				       -1);
		
		for (list = editor->loaded_widget->packing_properties; list; list = list->next)
		{
			property = list->data;
			
			def = glade_property_default (property);

			gtk_tree_store_append (model, &property_iter, &packing_iter);
			gtk_tree_store_set    (model, &property_iter,
					       COLUMN_ENABLED,   FALSE,
					       COLUMN_PROP_NAME, property->class->name,
					       COLUMN_PROPERTY,  property,
					       COLUMN_PARENT,    FALSE,
					       COLUMN_DEFAULT,   def,
					       COLUMN_NDEFAULT,  !def,
					       COLUMN_DEFSTRING, _("(default)"),
					       -1);
		}
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
	gtk_tree_store_set (GTK_TREE_STORE (model), iter, COLUMN_ENABLED, select, -1);
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
