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


#define GLADE_PROPERY_TABLE_ROW_SPACING 2

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h> /* for atoi and atof */
#include <string.h>

#include "glade.h"

#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-choice.h"
#include "glade-editor.h"
#include "glade-signal-editor.h"
#include "glade-parameter.h"
#include "glade-project-window.h"
#include "glade-property.h"
#include "glade-property-class.h"

static void glade_editor_class_init (GladeEditorClass * klass);
static void glade_editor_init (GladeEditor * editor);

enum
{
	SELECT_ITEM,
	LAST_SIGNAL
};


static guint glade_editor_signals[LAST_SIGNAL] = {0};

static GtkWindowClass *parent_class = NULL;
static void glade_editor_select_item_real (GladeEditor *editor, GladeWidget *widget);


/* We use this function recursively so we need to declare it */
static gboolean glade_editor_table_append_items (GladeEditorTable *table,
						 GladeWidgetClass *class,
						 GList **list,
						 gboolean common);

guint
glade_editor_get_type (void)
{
	static guint editor_type = 0;

	if (!editor_type)
	{
		GtkTypeInfo editor_info =
		{
			"GladeEditor",
			sizeof (GladeEditor),
			sizeof (GladeEditorClass),
			(GtkClassInitFunc) glade_editor_class_init,
			(GtkObjectInitFunc) glade_editor_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		editor_type = gtk_type_unique (gtk_window_get_type (), &editor_info);
	}

	return editor_type;
}


static void
glade_editor_class_init (GladeEditorClass * klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) klass;

	parent_class = gtk_type_class (gtk_window_get_type ());

	glade_editor_signals[SELECT_ITEM] =
		gtk_signal_new ("select_item",
				GTK_RUN_LAST,
				GTK_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (GladeEditorClass, select_item),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	klass->select_item = glade_editor_select_item_real;
}

static gint
glade_editor_delete_event (GladeEditor *editor, gpointer not_used)
{
	gtk_widget_hide (GTK_WIDGET (editor));

	/* Return true so that the editor is not destroyed */
	return TRUE;
}

GtkWidget *
my_notebook_page (const gchar *name, GtkWidget *notebook)
{
	GtkWidget *vbox;
	GtkWidget *label;
	static gint page = 0;

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_set_usize (vbox, -1, 350);
	label = gtk_label_new (name);
	gtk_notebook_insert_page (GTK_NOTEBOOK (notebook), vbox, label, page++);

	return vbox;
}
	
static void
glade_editor_init (GladeEditor *editor)
{
	GtkWidget *notebook;

	editor->tooltips = gtk_tooltips_new ();

	gtk_window_set_title  (GTK_WINDOW (editor), _("Properties"));
	gtk_window_set_policy (GTK_WINDOW (editor), FALSE, TRUE, TRUE);

	notebook = gtk_notebook_new ();
	
	editor->vbox_widget  = my_notebook_page (_("Widget"), notebook);
	editor->vbox_packing = my_notebook_page (_("Packing"), notebook);
	editor->vbox_common  = my_notebook_page (_("Common"), notebook);
	editor->vbox_signals = my_notebook_page (_("Signals"), notebook);
	editor->widget_tables = NULL;
	editor->loading = FALSE;

	gtk_container_add (GTK_CONTAINER (editor), notebook);

	gtk_signal_connect (GTK_OBJECT (editor), "delete_event",
			    GTK_SIGNAL_FUNC (glade_editor_delete_event), NULL);
	
}

static GladeEditor *
glade_editor_new ()
{
	GladeEditor *editor;

	editor = GLADE_EDITOR (gtk_type_new (glade_editor_get_type ()));
	
	return editor;
}

static void
glade_editor_widget_name_changed (GtkWidget *editable, GladeEditor *editor)
{
	GladeWidget *widget;
	gchar *new_name;
	
	g_return_if_fail (GTK_IS_EDITABLE (editable));
	g_return_if_fail (GLADE_IS_EDITOR (editor));
	
	if (editor->loading) return;

	g_return_if_fail (GTK_IS_WIDGET (editor->loaded_widget->widget));

	widget = editor->loaded_widget;
	new_name = gtk_editable_get_chars (GTK_EDITABLE (editable), 0, -1);
	glade_widget_set_name (widget, new_name);
}

static void
glade_editor_table_attach (GtkWidget *table, GtkWidget *child, gint pos, gint row)
{
	gtk_widget_set_usize (child, pos == 0 ? 100 : 120, -1);
	
	gtk_table_attach (GTK_TABLE (table), child,
			  pos, pos+1, row, row +1,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);
}



static GladeEditorTable *
glade_editor_table_new (GladeWidgetClass *class)
{
	GladeEditorTable *table;

	table = g_new0 (GladeEditorTable, 1);

	table->glade_widget_class = class;
	table->table_widget = gtk_table_new (0, 0, FALSE);
	table->properties = NULL;
	table->rows = 0;

	gtk_table_set_row_spacings (GTK_TABLE (table->table_widget),
				    GLADE_PROPERY_TABLE_ROW_SPACING);

	return table;
}











/* ================================ Property Changed ==================================== */
static void
glade_editor_property_changed_text_common (GladeProperty *property, const gchar *text)
{
	glade_property_set_string (property, text);
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

	g_return_if_fail (property != NULL);
	
	if (property->property->loading)
		return;
	
	choice = gtk_object_get_data (GTK_OBJECT (menu_item),
				      GLADE_ENUM_DATA_TAG);
	g_return_if_fail (choice != NULL);
	
	glade_property_set_enum (property->property, choice);
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
	property = gtk_object_get_user_data (GTK_OBJECT (button));
	property->property->enabled = state;
}

static void
glade_editor_property_changed_numeric (GtkWidget *spin,
				       GladeEditorProperty *property)
{
	GladeEditorNumericType numeric_type;
	gdouble double_val;
	gfloat float_val;
	gint integer_val;
	
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->property != NULL);

	if (property->property->loading)
		return;

	numeric_type = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (spin), "NumericType"));

	switch (numeric_type) {
	case GLADE_EDITOR_INTEGER:
		integer_val = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
		glade_property_set_integer (property->property, integer_val);
		break;
	case GLADE_EDITOR_FLOAT:
		float_val = gtk_spin_button_get_value_as_float (GTK_SPIN_BUTTON (spin));
		glade_property_set_float (property->property, float_val);
		break;
	case GLADE_EDITOR_DOUBLE:
		double_val = (gdouble) gtk_spin_button_get_value_as_float (GTK_SPIN_BUTTON (spin));
		glade_property_set_double (property->property, double_val);
		break;
	default:
		g_warning ("Invalid numeric_type %i\n", numeric_type);
	}
}

static void
glade_editor_property_changed_boolean (GtkWidget *button,
				       GladeEditorProperty *property)
{
	const gchar * text [] = { GLADE_TAG_NO, GLADE_TAG_YES };
	GtkWidget *label;
	gboolean state;

	g_return_if_fail (property != NULL);

	if (property->property->loading)
		return;

	state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	label = GTK_BIN (button)->child;
	gtk_label_set_text (GTK_LABEL (label), text [state]);

	glade_property_set_boolean (property->property, state);
}












/* ================================ Create inputs ==================================== */
static GtkWidget *
glade_editor_create_input_enum_item (GladeEditorProperty *property,
				     GladeChoice *choice)
{
	GtkWidget *menu_item;
	const gchar *name;

	name = choice->name;
	menu_item = gtk_menu_item_new_with_label (name);

	gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
			    GTK_SIGNAL_FUNC (glade_editor_property_changed_enum), property);

	gtk_object_set_data (GTK_OBJECT (menu_item), GLADE_ENUM_DATA_TAG, choice);

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

	list = property->class->choices;
	menu = gtk_menu_new ();
	for (; list != NULL; list = list->next) {
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

		gtk_signal_connect (GTK_OBJECT (entry), "changed",
				    GTK_SIGNAL_FUNC (glade_editor_property_changed_text), property);

		return entry;
	} else {
		GtkTextBuffer *buffer;
		GtkWidget *view;

		view = gtk_text_view_new ();
		gtk_text_view_set_editable (GTK_TEXT_VIEW (view), TRUE);

		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));

		g_signal_connect_data (G_OBJECT (buffer), "changed",
				       GTK_SIGNAL_FUNC (glade_editor_property_changed_text_view),
				       property, NULL, 0);
		
		return view;
	}

	return NULL;
}

static GtkWidget *
glade_editor_create_input_numeric (GladeEditorProperty *property,
							GladeEditorNumericType numeric_type)
{
	GladePropertyClass *class;
	GtkAdjustment *adjustment;
	GtkWidget *spin;

	g_return_val_if_fail (property != NULL, NULL);

	class = property->class;

	adjustment = glade_parameter_adjustment_new (class->parameters, class->def);

	spin  = gtk_spin_button_new (adjustment, 10,
						    numeric_type == GLADE_EDITOR_INTEGER ? 0 : 2);

	gtk_object_set_data (GTK_OBJECT (spin), "NumericType", GINT_TO_POINTER (numeric_type));
	gtk_signal_connect (GTK_OBJECT (spin), "value_changed",
			    GTK_SIGNAL_FUNC (glade_editor_property_changed_numeric),
			    property);

	/* Some numeric types are optional, for example the default window size, so
	 * they have a toggle button right next to the spin button. 
	 */
	if (class->optional) {
		GtkWidget *check;
		GtkWidget *hbox;
		check = gtk_check_button_new ();
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start (GTK_BOX (hbox), check, FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (hbox), spin,  TRUE, TRUE, 0);
		
		gtk_signal_connect (GTK_OBJECT (check), "toggled",
				    GTK_SIGNAL_FUNC (glade_editor_property_changed_enabled),
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
	GladePropertyClass *class;
	GtkWidget *button;

	g_return_val_if_fail (property != NULL, NULL);

	class = property->class;

	button = gtk_toggle_button_new_with_label (_("No"));

	gtk_signal_connect (GTK_OBJECT (button), "toggled",
			    GTK_SIGNAL_FUNC (glade_editor_property_changed_boolean), property);

	return button;
}

static GtkWidget *
glade_editor_create_input_object (GladeEditorProperty *property, GladeEditorTable *table)
{
	g_return_val_if_fail (GLADE_IS_EDITOR_TABLE (table), NULL);
	g_return_val_if_fail (GLADE_IS_EDITOR_PROPERTY (property), NULL);

	glade_editor_table_append_items (table, property->class->child, &property->children, FALSE);

	return NULL;
}

static GtkWidget *
glade_editor_append_item_real (GladeEditorTable *table, GladeEditorProperty *property)
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

	}

	if (input == NULL) {
		g_warning ("Can't create an input widget for type %s\n",
			   property->class->name);
		return gtk_label_new ("Implement me !");
	}


	label = glade_property_class_create_label (property->class);
	
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
	gtk_signal_connect (GTK_OBJECT (entry), "changed",
			    GTK_SIGNAL_FUNC (glade_editor_widget_name_changed),
			    table->editor);

	glade_editor_table_attach (gtk_table, label, 0, table->rows);
	glade_editor_table_attach (gtk_table, entry, 1, table->rows);
	table->rows++;
	
	/* Class */
	label = gtk_label_new (_("Class :"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	entry = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry), table->glade_widget_class->name);
	gtk_entry_set_editable (GTK_ENTRY (entry), FALSE);

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

	for (; list != NULL; list = list->next) {
		property_class = (GladePropertyClass *) list->data;
		if (common != property_class->common)
			continue;
		property = glade_editor_table_append_item (table, property_class);
		if (property != NULL)
			new_list = g_list_prepend (new_list,
						   property);
	}

	*list_ = new_list;
	
	return TRUE;
}

static GladeEditorTable *
glade_editor_table_create (GladeEditor *editor, GladeWidgetClass *class, gboolean common)
{
	GladeEditorTable *table;

	g_return_val_if_fail (GLADE_IS_EDITOR (editor), NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (class), NULL);

	table = glade_editor_table_new (class);
	table->editor = editor;
	table->common = common;
	if (!common)
		glade_editor_table_append_standard_fields (table);
	if (!glade_editor_table_append_items (table, class, &table->properties, common))
		return NULL;
	gtk_widget_show_all (table->table_widget);
	editor->widget_tables = g_list_prepend (editor->widget_tables,
						table);

	return table;
}

static GladeEditorTable *
glade_editor_get_table_from_class (GladeEditor *editor, GladeWidgetClass *class, gboolean common)
{
	GladeEditorTable *table;
	GList *list;

	list = editor->widget_tables;
	for (; list != NULL; list = list->next) {
		table = list->data;
		if (common != table->common)
			continue;
		if (table->glade_widget_class == class)
			return table;
	}

	return NULL;		
}

static void
glade_editor_load_widget_page (GladeEditor *editor, GladeWidgetClass *class)
{
	GladeEditorTable *table;
	GtkContainer *container;
	GList *list;

	/* Remove the old table that was in this container */
	container = GTK_CONTAINER (editor->vbox_widget);
	list = gtk_container_children (container);
	for (; list; list = list->next) {
		GtkWidget *widget = list->data;
		g_return_if_fail (GTK_IS_WIDGET (widget));
		gtk_widget_ref (widget);
		gtk_container_remove (container, widget);
	}

	if (!class)
		return;
	
	table = glade_editor_get_table_from_class (editor, class, FALSE);
	if (table == NULL)
		table = glade_editor_table_create (editor, class, FALSE);

	g_return_if_fail (table != NULL);
	
	/* Attach the new table */
	gtk_box_pack_start (GTK_BOX (editor->vbox_widget), table->table_widget,
			    TRUE, TRUE, 0);
}

static void
glade_editor_load_common_page (GladeEditor *editor, GladeWidgetClass *class)
{
	GladeEditorTable *table;
	GtkContainer *container;
	GList *list;

	/* Remove the old table that was in this container */
	container = GTK_CONTAINER (editor->vbox_common);
	list = gtk_container_children (container);
	for (; list; list = list->next) {
		GtkWidget *widget = list->data;
		g_return_if_fail (GTK_IS_WIDGET (widget));
		gtk_widget_ref (widget);
		gtk_container_remove (container, widget);
	}

	if (!class)
		return;
	
	table = glade_editor_get_table_from_class (editor, class, TRUE);
	if (table == NULL)
		table = glade_editor_table_create (editor, class, TRUE);

	g_return_if_fail (table != NULL);
	

	/* Attach the new table */
	gtk_box_pack_start (GTK_BOX (editor->vbox_common), table->table_widget,
			    TRUE, TRUE, 0);
}

static void
glade_editor_load_signal_page (GladeEditor *editor, GladeWidgetClass *class)
{
	if (editor->signal_editor == NULL) {
		editor->signal_editor = glade_signal_editor_new ();
		gtk_box_pack_start (GTK_BOX (editor->vbox_signals),
				    glade_signal_editor_get_widget (editor->signal_editor),
				    TRUE, TRUE, 0);
	}
}

static void
glade_editor_load_class (GladeEditor *editor, GladeWidgetClass *class)
{
	glade_editor_load_widget_page  (editor, class);
	glade_editor_load_common_page  (editor, class);
	glade_editor_load_signal_page  (editor, class);

	editor->loaded_class = class;
}

































/* ================================ Load properties ================================== */
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
		gtk_object_set_user_data (GTK_OBJECT (button), property);
	} else {
		spin = property->input;
	}


	if (property->property->value->g_type == G_TYPE_INT)
		val = (gfloat) g_value_get_int (property->property->value);
	else if (property->property->value->g_type == G_TYPE_DOUBLE)
		val = (gfloat) g_value_get_double (property->property->value);
	else
		val = g_value_get_float (property->property->value);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), val);
	gtk_object_set_user_data (GTK_OBJECT (spin), property);
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
		gtk_object_set_user_data (GTK_OBJECT (menu_item), property);
	}
}

static void
glade_editor_property_load_other_widgets (GladeEditorProperty *property)
{
	g_return_if_fail (property != NULL);
	g_return_if_fail (property->property != NULL);
	g_return_if_fail (property->property->value != NULL);
	g_return_if_fail (property->input != NULL);

	g_warning ("Implement me ! (%s)\n", __FUNCTION__);
}

static void
glade_editor_property_load_boolean (GladeEditorProperty *property)
{
	GtkWidget *label;
	const gchar * text [] = { GLADE_TAG_NO, GLADE_TAG_YES };
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
	
	gtk_object_set_user_data (GTK_OBJECT (property->input), property);
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
						  strlen (text),
						  &insert_pos);
		gtk_editable_set_position (editable, pos);
	} else if (GTK_IS_TEXT_VIEW (property->input)) {
		GtkTextBuffer *buffer;
		const gchar *text;

		text = g_value_get_string (property->property->value);
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (property->input));

		/* FIXME !!! Will not work with mulitybyte languajes !!. Chema */
		gtk_text_buffer_set_text (buffer,
					  text,
					  strlen (text));
	} else {
		g_warning ("Invalid Text Widget type.");
	}

	gtk_object_set_user_data (GTK_OBJECT (property->input), property);
}

/* We are recursing so we need the prototype beforehand. Don't you love C ? */
static void glade_editor_property_load (GladeEditorProperty *property, GladeWidget *widget);

static void
glade_editor_property_load_object (GladeEditorProperty *property, GladeWidget *widget)
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
	
	property->property = glade_widget_get_property_from_class (widget, class);

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
	case GLADE_PROPERTY_TYPE_OTHER_WIDGETS:
		glade_editor_property_load_other_widgets (property);
		break;
	case GLADE_PROPERTY_TYPE_ERROR:
		g_warning ("%s : type %i not implemented\n", __FUNCTION__,
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
glade_editor_table_free (GladeEditorTable *me)
{
	if (me)
		g_free (me);
}

static void
glade_editor_load_packing_page (GladeEditor *editor, GladeWidget *widget)
{
	static GladeEditorTable *old = NULL;
	static GList *old_props = NULL;
	GladeEditorProperty *editor_property;
	GladeEditorTable *table;
	GladeProperty *property;
	GtkContainer *container;
	GList *list;

	/* Remove the old properties */
	container = GTK_CONTAINER (editor->vbox_packing);
	list = gtk_container_children (container);
	for (; list; list = list->next) {
		GtkWidget *widget = list->data;
		g_return_if_fail (GTK_IS_WIDGET (widget));
		gtk_container_remove (container, widget);
	}

	/* Free the old structs */
	if (old)
		glade_editor_table_free (old);
	list = old_props;
	for (; list; list = list->next)
		g_free (list->data);
	old_props = NULL;
	old = NULL;

	if (!widget)
		return;

	/* Now add the new properties */
	table = glade_editor_table_new (widget->class);
	table->editor = editor;
	table->common = FALSE;
	table->packing = TRUE;
	
	list = widget->properties;
	for (; list; list = list->next) {
		property = list->data; 
		if (property->class->packing) {
			editor_property = glade_editor_table_append_item (table, property->class);
			old_props = g_list_prepend (old_props, editor_property);
			glade_editor_property_load (editor_property, widget);
		}
	}

	gtk_widget_show_all (table->table_widget);
	gtk_container_add (container, table->table_widget);

	old = table;
}

static void
glade_editor_property_changed_cb (GladeProperty *property,
				  GladeEditorProperty *editor_property)
{
	g_return_if_fail (property == editor_property->property);
	
	glade_editor_property_load (editor_property, property->widget);
}

static void
glade_editor_property_connect_signals (GladeEditorProperty *editor_property,
				       GladeWidget *widget)
{
	GladeProperty *property = editor_property->property;

	gtk_signal_connect (GTK_OBJECT (property), "changed",
			    GTK_SIGNAL_FUNC (glade_editor_property_changed_cb),
			    editor_property);
}

static void
glade_editor_load_item (GladeEditor *editor, GladeWidget *item)
{
	GladeEditorProperty *property;
	GladeWidgetClass *class;
	GladeEditorTable *table;
	GList *list;

	editor->loaded_widget = item;

	/* Load the GladeWidgetClass */
	class = item ? item->class : NULL;
	if (editor->loaded_class != class)
		glade_editor_load_class (editor, class);
	
	glade_editor_load_packing_page (editor, item);
	  
	glade_signal_editor_load_widget (editor->signal_editor, item);

	if (!item)
		return;
	
	/* Load each GladeEditorProperty */
	table = glade_editor_get_table_from_class (editor, class, FALSE);
	list = table->properties;
	for (; list != NULL; list = list->next) {
		property = list->data;
		glade_editor_property_load (property, item);
		
		if (property->class->type == GLADE_PROPERTY_TYPE_OBJECT)
			continue;
		
		glade_editor_property_connect_signals (property, item);
	}
	
	/* Load each GladeEditorProperty for the common tab*/
	table = glade_editor_get_table_from_class (editor, class, TRUE);
	list = table->properties;
	for (; list != NULL; list = list->next) {
		property = list->data;
		glade_editor_property_load (property, item);
	}

}














/**
 * glade_editor_select_item_real:
 * @editor: 
 * @widget: the widget to load into the editor. Can be NULL if we want
 *          to clear the editor. 
 * 
 * Select an item and load it into the editor
 **/
static void
glade_editor_select_item_real (GladeEditor *editor, GladeWidget *widget)
{
	GladeEditorTable *table;

	if (editor->loaded_widget == widget)
		return;

	glade_editor_load_item (editor, widget);

	if (!widget)
		return;
			
	editor->loading = TRUE;
	table = glade_editor_get_table_from_class (editor, widget->class, FALSE);
	g_return_if_fail (table != NULL);
	gtk_entry_set_text (GTK_ENTRY (table->name_entry), widget->name);
	editor->loading = FALSE;
}

void
glade_editor_select_widget (GladeEditor *editor, GladeWidget *widget)
{
	g_return_if_fail (GLADE_IS_EDITOR (editor));

	gtk_signal_emit (GTK_OBJECT (editor),
			 glade_editor_signals [SELECT_ITEM], widget);
}
	
void
glade_editor_show (GladeProjectWindow *gpw)
{
	g_return_if_fail (gpw != NULL);

	if (gpw->editor == NULL)
		glade_editor_create (gpw);
	
	gtk_widget_show_all (GTK_WIDGET (gpw->editor));
}

void
glade_editor_create (GladeProjectWindow *gpw)
{
	if (gpw->editor == NULL) {
		gpw->editor = GLADE_EDITOR (glade_editor_new ());
		gpw->editor->project_window = gpw;
	}
}
