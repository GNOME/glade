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

	gtk_table_set_row_spacings (GTK_TABLE (table->table_widget),
				    GLADE_PROPERY_TABLE_ROW_SPACING);

	return table;
}











/* ================================ Property Changed ==================================== */
static void
glade_editor_property_changed_text (GtkWidget *entry,
				    GladeEditorProperty *property)
{
	gchar *text;

	g_return_if_fail (property != NULL);

	if (property->loading)
		return;
	
	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);

	glade_property_changed_text (property->property, text);

	g_free (text);
}


static void
glade_editor_property_changed_choice (GtkWidget *menu_item,
				      GladeEditorProperty *property)
{
	GladeChoice *choice;

	g_return_if_fail (property != NULL);
	
	if (property->loading)
		return;
	
	choice = gtk_object_get_data (GTK_OBJECT (menu_item),
				      GLADE_CHOICE_DATA_TAG);
	g_return_if_fail (choice != NULL);
	
	glade_property_changed_choice (property->property, choice);
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
	
	if (property->loading)
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
glade_editor_property_changed_integer (GtkWidget *spin,
				       GladeEditorProperty *property)
{
	gint val;

	g_return_if_fail (property != NULL);
	
	if (property->loading)
		return;

	val = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
	glade_property_changed_integer (property->property, val);
}

static void
glade_editor_property_changed_float (GtkWidget *spin,
				     GladeEditorProperty *property)
{
	gfloat val;

	g_return_if_fail (property != NULL);
	
	if (property->loading)
		return;

	val = gtk_spin_button_get_value_as_float (GTK_SPIN_BUTTON (spin));
	glade_property_changed_float (property->property, val);
}

static void
glade_editor_property_changed_boolean (GtkWidget *button,
				       GladeEditorProperty *property)
{
	const gchar * text [] = { GLADE_TAG_NO, GLADE_TAG_YES };
	GtkWidget *label;
	gboolean state;

	g_return_if_fail (property != NULL);
	
	if (property->loading)
		return;

	state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	label = GTK_BIN (button)->child;
	gtk_label_set_text (GTK_LABEL (label), text [state]);

	glade_property_changed_boolean (property->property, state);
}












/* ================================ Create inputs ==================================== */
static GtkWidget *
glade_editor_create_input_choice_item (GladeEditorProperty *property,
				       GladeChoice *choice)
{
	GtkWidget *menu_item;
	const gchar *name;

	name = choice->name;
	menu_item = gtk_menu_item_new_with_label (name);

	gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
			    GTK_SIGNAL_FUNC (glade_editor_property_changed_choice), property);

	gtk_object_set_data (GTK_OBJECT (menu_item), GLADE_CHOICE_DATA_TAG, choice);

	return menu_item;
}

static GtkWidget *
glade_editor_create_input_choice (GladeEditorProperty *property)
{
	GladePropertyClass *class;
	GladeChoice *choice;
	GtkWidget *menu_item;
	GtkWidget *menu;
	GtkWidget *option_menu;
	GList *list;

	g_return_val_if_fail (property != NULL, NULL);

	class = property->glade_property_class;

	list = class->choices;
	menu = gtk_menu_new ();
	for (; list != NULL; list = list->next) {
		choice = (GladeChoice *)list->data;
		menu_item = glade_editor_create_input_choice_item (property, choice);
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

	class = property->glade_property_class;

	glade_parameter_get_integer (class->parameters, "VisibleLines", &lines);

	if (lines < 2) {
		GtkWidget *entry;
		
		entry = gtk_entry_new ();

		gtk_signal_connect (GTK_OBJECT (entry), "changed",
				    GTK_SIGNAL_FUNC (glade_editor_property_changed_text), property);

		return entry;
	} else {
#warning FIXME GtkText is not working
#if 0	
		GtkWidget *text;
		GtkWidget *scrolled_window;
		gint line_height;
		gint height;

		scrolled_window = gtk_scrolled_window_new (NULL, NULL);
		text = gtk_text_new (NULL, NULL);
		gtk_text_set_editable ((GtkText *)text, TRUE);

		gtk_container_add (GTK_CONTAINER (scrolled_window), text);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
						GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		line_height = text->style->font->ascent + text->style->font->descent;
		/* We add 8 for the text's border height etc. */
		height = lines * line_height;
		height -= 20;
		/* This is not working */
		gtk_widget_set_usize (text, -1, height);
		
		gtk_signal_connect (GTK_OBJECT (text), "changed",
				    GTK_SIGNAL_FUNC (glade_editor_property_changed_text), property);
		property->input = text;
		
		return scrolled_window;
#endif
		return NULL;
	}

	return NULL;
}

static GtkWidget *
glade_editor_create_input_numeric (GladeEditorProperty *property,
				   gboolean is_integer)
{
	GladePropertyClass *class;
	GtkAdjustment *adjustment;
	GtkWidget *spin;

	g_return_val_if_fail (property != NULL, NULL);

	class = property->glade_property_class;

	adjustment = glade_parameter_adjustment_new (class->parameters);
	
	spin  = gtk_spin_button_new (adjustment, 10, is_integer ? 0 : 2);
	
	if (is_integer)
		gtk_signal_connect (GTK_OBJECT (spin), "changed",
				    GTK_SIGNAL_FUNC (glade_editor_property_changed_integer),
				    property);
	else
		gtk_signal_connect (GTK_OBJECT (spin), "changed",
				    GTK_SIGNAL_FUNC (glade_editor_property_changed_float),
				    property);

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
	return glade_editor_create_input_numeric (property, TRUE);
}

static GtkWidget *
glade_editor_create_input_float (GladeEditorProperty *property)
{
	return glade_editor_create_input_numeric (property, FALSE);
}

static GtkWidget *
glade_editor_create_input_boolean (GladeEditorProperty *property)
{
	GladePropertyClass *class;
	GtkWidget *button;

	g_return_val_if_fail (property != NULL, NULL);

	class = property->glade_property_class;

	button = gtk_toggle_button_new_with_label (_("No"));

	gtk_signal_connect (GTK_OBJECT (button), "toggled",
			    GTK_SIGNAL_FUNC (glade_editor_property_changed_boolean), property);

	return button;
}

GtkWidget *
glade_editor_create_input (GladeEditorProperty *property)
{
	GladePropertyType type;
	GtkWidget *input = NULL;

	g_return_val_if_fail (property != NULL, NULL);

	type = property->glade_property_class->type;
	
	switch (type) {
	case GLADE_PROPERTY_TYPE_BOOLEAN:
		input = glade_editor_create_input_boolean (property);
		break;
	case GLADE_PROPERTY_TYPE_FLOAT:
		input = glade_editor_create_input_float (property);
		break;
	case GLADE_PROPERTY_TYPE_INTEGER:
		input = glade_editor_create_input_integer (property);
		break;
	case GLADE_PROPERTY_TYPE_TEXT:
		input = glade_editor_create_input_text (property);
		break;
	case GLADE_PROPERTY_TYPE_CHOICE:
		input = glade_editor_create_input_choice (property);
		break;
	case GLADE_PROPERTY_TYPE_OTHER_WIDGETS:
		g_warning ("The widget type %d does not have an input implemented\n", type);
		break;
	default:
	}

	if (input == NULL) {
		g_warning ("Can't create an input widget for type %i\n", type);
		return gtk_label_new ("Implement me !");
	}

	return input;
}




















static GladeEditorProperty *
glade_editor_property_new (GladeEditor *editor,
			   GladePropertyClass *property_class)
{
	GladeEditorProperty *editor_property;

	editor_property = g_new0 (GladeEditorProperty, 1);
	
	editor_property->glade_property_class = property_class;
	editor_property->input = glade_editor_create_input (editor_property);
	editor_property->property = NULL;

	return editor_property;
}

static void
glade_editor_create_widget_table_from_class_common (GladeEditor *editor,
						    GladeEditorTable *table,
						    gint *row_)
{
	GtkWidget *gtk_table;
	GtkWidget *label;
	GtkWidget *entry;
	gint row = *row_;

	gtk_table = table->table_widget;
	
	/* Name */
	label = gtk_label_new (_("Name :"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	entry = gtk_entry_new ();
	table->name_entry = entry;
	gtk_signal_connect (GTK_OBJECT (entry), "changed",
			    GTK_SIGNAL_FUNC (glade_editor_widget_name_changed),
			    editor);

	glade_editor_table_attach (gtk_table, label, 0, row);
	glade_editor_table_attach (gtk_table, entry, 1, row);
	
	row++;

	/* Class */
	label = gtk_label_new (_("Class :"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	entry = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry), table->glade_widget_class->name);
	gtk_entry_set_editable (GTK_ENTRY (entry), FALSE);

	glade_editor_table_attach (gtk_table, label, 0, row);
	glade_editor_table_attach (gtk_table, entry, 1, row);
	
	row++;

	*row_ = row;
}

static GladeEditorTable *
glade_editor_create_table_from_class (GladeEditor *editor,
				      GladeWidgetClass *class)
{
	GladePropertyClass *property_class;
	GladeEditorTable *table;
	GtkWidget *gtk_table;
	GtkWidget *label;
	GList *list;
	gint row = 0;

	table = glade_editor_table_new (class);
	gtk_table = table->table_widget;

	/* Add the common fields. Name & Class */
	glade_editor_create_widget_table_from_class_common (editor,
							    table,
							    &row);
	list = class->properties;
	for (; list != NULL; list = list->next) {
		GladeEditorProperty *editor_property;
		
		property_class = (GladePropertyClass *) list->data;
		label = glade_property_class_create_label (property_class);
		if (label == NULL)
			return NULL;
		editor_property = glade_editor_property_new (editor,
							     property_class);
		if (editor_property == NULL)
			return NULL;

		table->properties = g_list_prepend (table->properties,
						    editor_property);
		
		glade_editor_table_attach (gtk_table, label, 0, row);
		glade_editor_table_attach (gtk_table, editor_property->input,
					   1, row);

		row++;
	}

	gtk_widget_show_all (gtk_table);

	editor->widget_tables = g_list_prepend (editor->widget_tables,
						table);
	return table;
}

static GladeEditorTable *
glade_editor_get_table_from_class (GladeEditor *editor, GladeWidgetClass *class)
{
	GladeEditorTable *table;
	GList *list;

	list = editor->widget_tables;
	for (; list != NULL; list = list->next) {
		table = list->data;
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

	table = glade_editor_get_table_from_class (editor, class);
	if (table == NULL)
		table = glade_editor_create_table_from_class (editor, class);

	g_return_if_fail (table != NULL);
	
	/* Remove the old table that was in this container */
	container = GTK_CONTAINER (editor->vbox_widget);
	list = gtk_container_children (container);
	for (; list; list = list->next) {
		GtkWidget *widget = list->data;
		g_return_if_fail (GTK_IS_WIDGET (widget));
		gtk_widget_ref (widget);
		gtk_container_remove (container, widget);
	}

	/* Attach the new table */
	gtk_box_pack_start (GTK_BOX (editor->vbox_widget), table->table_widget,
			    TRUE, TRUE, 0);
}

static void
glade_editor_load_signal_page (GladeEditor *editor, GladeWidgetClass *class)
{

	if (editor->signal_editor == NULL) {
		editor->signal_editor = glade_signal_editor_new ();
		gtk_box_pack_start (GTK_BOX (editor->vbox_signals), glade_signal_editor_get_widget (editor->signal_editor),
				TRUE, TRUE, 0);
	}
}

static void
glade_editor_load_class (GladeEditor *editor, GladeWidgetClass *class)
{
	glade_editor_load_widget_page (editor, class);
	glade_editor_load_signal_page (editor, class);

	editor->loaded_class = class;
}

































/* ================================ Load properties ================================== */
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

	val = atoi (property->property->value);
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
glade_editor_property_load_choice (GladeEditorProperty *property)
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
		choice = (GladeChoice *)list->data;
		if (strcmp (choice->symbol, property->property->value) == 0)
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

	if (strcmp (property->property->value, GLADE_TAG_TRUE) == 0)
		state = TRUE;
	else if (strcmp (property->property->value, GLADE_TAG_FALSE) == 0)
		state = FALSE;
	else {
		g_warning ("Invalid boolean settings %s [%s],%s,%s)\n",
			   property->property->value, property->property->class->name,
			   GLADE_TAG_TRUE, GLADE_TAG_FALSE);
		state = FALSE;
	}

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (property->input), state);
	label = GTK_BIN (property->input)->child;
	g_return_if_fail (GTK_IS_LABEL (label));
	gtk_label_set_text (GTK_LABEL (label), text [state]);
	
	gtk_object_set_user_data (GTK_OBJECT (property->input), property);
}

static void
glade_editor_property_load_text (GladeEditorProperty *property)
{
	gint pos = 0;

	g_return_if_fail (property != NULL);
	g_return_if_fail (property->property != NULL);
	g_return_if_fail (property->property->value != NULL);
	g_return_if_fail (property->input != NULL);
	g_return_if_fail (GTK_IS_EDITABLE (property->input));

	gtk_editable_delete_text (GTK_EDITABLE (property->input), 0, -1);
	gtk_editable_insert_text (GTK_EDITABLE (property->input),
				  property->property->value, strlen (property->property->value), &pos);

	gtk_object_set_user_data (GTK_OBJECT (property->input), property);
}


static void
glade_editor_property_load (GladeEditorProperty *property, GladeWidget *widget)
{
	GladePropertyClass *property_class = property->glade_property_class;

	g_return_if_fail (property != NULL);
	g_return_if_fail (property->glade_property_class != NULL);
			  
	property->loading = TRUE;
	property->property = glade_property_get_from_class (widget, property_class);

	g_return_if_fail (property->property != NULL);
	g_return_if_fail (property->property->class == property->glade_property_class);
	
	switch (property_class->type) {
	case GLADE_PROPERTY_TYPE_TEXT:
		glade_editor_property_load_text (property);
		break;
	case GLADE_PROPERTY_TYPE_BOOLEAN:
		glade_editor_property_load_boolean (property);
		break;
	case GLADE_PROPERTY_TYPE_FLOAT:
		glade_editor_property_load_float (property);
		break;
	case GLADE_PROPERTY_TYPE_INTEGER:
		glade_editor_property_load_integer (property);
		break;
	case GLADE_PROPERTY_TYPE_CHOICE:
		glade_editor_property_load_choice (property);
		break;
	case GLADE_PROPERTY_TYPE_OTHER_WIDGETS:
		glade_editor_property_load_other_widgets (property);
		break;
	default:
		g_warning ("%s : type %i not implemented\n", __FUNCTION__,
			   property_class->type);
	}

	property->loading = FALSE;
}

	
static void
glade_editor_load_item (GladeEditor *editor, GladeWidget *item)
{
	GladeEditorTable *table;
	GladeEditorProperty *property;
	GladeWidgetClass *class;
	GList *list;

	/* Load the GladeWidgetClass */
	class = item->class;
	if (editor->loaded_class != class)
		glade_editor_load_class (editor, class);

	
	editor->loaded_widget = item;

	/* Load each GladeEditorProperty */
	table = glade_editor_get_table_from_class (editor, class);
	list = table->properties;
	for (; list != NULL; list = list->next) {
		property = list->data;
		glade_editor_property_load (property, item);
	}

	glade_signal_editor_load_widget (editor->signal_editor, item);
}














static void
glade_editor_select_item_real (GladeEditor *editor, GladeWidget *widget)
{
	GladeEditorTable *table;
	
	if (editor->loaded_widget == widget)
		return;

	glade_editor_load_item (editor, widget);

	editor->loading = TRUE;
	table = glade_editor_get_table_from_class (editor, widget->class);
	g_return_if_fail (table != NULL);
	gtk_entry_set_text (GTK_ENTRY (table->name_entry), widget->name);
	editor->loading = FALSE;
}

void
glade_editor_select_widget (GladeEditor *editor, GladeWidget *widget)
{
	g_return_if_fail (GLADE_IS_EDITOR (editor));
	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_WIDGET (widget->widget));
	
	gtk_signal_emit (GTK_OBJECT (editor),
			 glade_editor_signals [SELECT_ITEM], widget);
}
	
void
glade_editor_show (GladeProjectWindow *gpw)
{
	g_return_if_fail (gpw != NULL);

	if (gpw->editor == NULL) {
		gpw->editor = GLADE_EDITOR (glade_editor_new ());
		gpw->editor->project_window = gpw;
	}

	gtk_widget_show_all (GTK_WIDGET (gpw->editor));
}

void
glade_editor_create (GladeProjectWindow *gpw)
{
	glade_editor_show (gpw);
}













