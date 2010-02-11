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

#include "glade-button-editor.h"


static void glade_button_editor_finalize        (GObject              *object);

static void glade_button_editor_editable_init   (GladeEditableIface *iface);

static void glade_button_editor_grab_focus      (GtkWidget            *widget);


G_DEFINE_TYPE_WITH_CODE (GladeButtonEditor, glade_button_editor, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_button_editor_editable_init));


static void
glade_button_editor_class_init (GladeButtonEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize     = glade_button_editor_finalize;
	widget_class->grab_focus   = glade_button_editor_grab_focus;
}

static void
glade_button_editor_init (GladeButtonEditor *self)
{
}

static void
project_changed (GladeProject      *project,
		 GladeCommand      *command,
		 gboolean           execute,
		 GladeButtonEditor *button_editor)
{
	if (button_editor->modifying ||
	    !gtk_widget_get_mapped (GTK_WIDGET (button_editor)))
		return;

	/* Reload on all commands */
	glade_editable_load (GLADE_EDITABLE (button_editor), button_editor->loaded_widget);
}


static void
project_finalized (GladeButtonEditor *button_editor,
		   GladeProject       *where_project_was)
{
	button_editor->loaded_widget = NULL;

	glade_editable_load (GLADE_EDITABLE (button_editor), NULL);
}

static void
glade_button_editor_load (GladeEditable *editable,
			  GladeWidget   *widget)
{
	GladeButtonEditor *button_editor = GLADE_BUTTON_EDITOR (editable);
	GladeWidget       *gchild = NULL;
	GtkWidget         *child, *button;
	gboolean           use_stock = FALSE, use_appearance = FALSE;
	GList *l;

	button_editor->loading = TRUE;

	/* Since we watch the project*/
	if (button_editor->loaded_widget)
	{
		/* watch custom-child and use-stock properties here for reloads !!! */

		g_signal_handlers_disconnect_by_func (G_OBJECT (button_editor->loaded_widget->project),
						      G_CALLBACK (project_changed), button_editor);

		/* The widget could die unexpectedly... */
		g_object_weak_unref (G_OBJECT (button_editor->loaded_widget->project),
				     (GWeakNotify)project_finalized,
				     button_editor);
	}

	/* Mark our widget... */
	button_editor->loaded_widget = widget;

	if (button_editor->loaded_widget)
	{
		/* This fires for undo/redo */
		g_signal_connect (G_OBJECT (button_editor->loaded_widget->project), "changed",
				  G_CALLBACK (project_changed), button_editor);

		/* The widget/project could die unexpectedly... */
		g_object_weak_ref (G_OBJECT (button_editor->loaded_widget->project),
				   (GWeakNotify)project_finalized,
				   button_editor);
	}

	/* load the embedded editable... */
	if (button_editor->embed)
		glade_editable_load (GLADE_EDITABLE (button_editor->embed), widget);

	for (l = button_editor->properties; l; l = l->next)
		glade_editor_property_load_by_widget (GLADE_EDITOR_PROPERTY (l->data), widget);


	if (widget)
	{
		glade_widget_property_get (widget, "use-action-appearance", &use_appearance);

		button = GTK_WIDGET (widget->object);
		child  = gtk_bin_get_child (GTK_BIN (button));
		if (child)
			gchild = glade_widget_get_from_gobject (child);

		/* Setup radio and sensitivity states */
		if ((gchild && gchild->parent) ||  // a widget is manually inside
		    GLADE_IS_PLACEHOLDER (child)) // placeholder there, custom mode
		{
			/* Custom */
			gtk_widget_set_sensitive (button_editor->standard_frame, FALSE);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_editor->custom_radio), TRUE);
		}
		else
		{
			/* Standard */
			gtk_widget_set_sensitive (button_editor->standard_frame, TRUE);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_editor->standard_radio), TRUE);
			
			glade_widget_property_get (widget, "use-stock", &use_stock);
			
			if (use_stock)
			{
				gtk_widget_set_sensitive (button_editor->stock_frame, TRUE);
				gtk_widget_set_sensitive (button_editor->label_frame, FALSE);
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_editor->stock_radio), TRUE);
			}
			else
			{
				gtk_widget_set_sensitive (button_editor->stock_frame, FALSE);
				gtk_widget_set_sensitive (button_editor->label_frame, TRUE);
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_editor->label_radio), TRUE);
			}
		}

		if (use_appearance)
			gtk_widget_set_sensitive (button_editor->custom_radio, FALSE);
		else
			gtk_widget_set_sensitive (button_editor->custom_radio, TRUE);

	}
	button_editor->loading = FALSE;
}

static void
glade_button_editor_set_show_name (GladeEditable *editable,
				   gboolean       show_name)
{
	GladeButtonEditor *button_editor = GLADE_BUTTON_EDITOR (editable);

	glade_editable_set_show_name (GLADE_EDITABLE (button_editor->embed), show_name);
}

static void
glade_button_editor_editable_init (GladeEditableIface *iface)
{
	iface->load = glade_button_editor_load;
	iface->set_show_name = glade_button_editor_set_show_name;
}

static void
glade_button_editor_finalize (GObject *object)
{
	GladeButtonEditor *button_editor = GLADE_BUTTON_EDITOR (object);

	if (button_editor->properties)
		g_list_free (button_editor->properties);
	button_editor->properties = NULL;
	button_editor->embed      = NULL;

	glade_editable_load (GLADE_EDITABLE (object), NULL);

	G_OBJECT_CLASS (glade_button_editor_parent_class)->finalize (object);
}

static void
glade_button_editor_grab_focus (GtkWidget *widget)
{
	GladeButtonEditor *button_editor = GLADE_BUTTON_EDITOR (widget);

	gtk_widget_grab_focus (button_editor->embed);
}

/* Secion control radio button callbacks: */
static void
standard_toggled (GtkWidget         *widget,
		  GladeButtonEditor *button_editor)
{
	GladeProperty     *property;
	GladeWidget       *gchild = NULL;
	GtkWidget         *child, *button;
	GValue             value = { 0, };
	gboolean           use_appearance = FALSE;

	if (button_editor->loading || !button_editor->loaded_widget)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_editor->standard_radio)))
		return;

	button_editor->modifying = TRUE;

	glade_command_push_group (_("Setting %s to use standard configuration"), 
				  button_editor->loaded_widget->name);

	/* If theres a widget customly inside... command remove it first... */
	button = GTK_WIDGET (button_editor->loaded_widget->object);
	child  = gtk_bin_get_child (GTK_BIN (button));
	if (child)
		gchild = glade_widget_get_from_gobject (child);

	if (gchild && gchild->parent == button_editor->loaded_widget)
	{
		GList widgets = { 0, };
		widgets.data = gchild;
		glade_command_delete (&widgets);
	}

	property = glade_widget_get_property (button_editor->loaded_widget, "custom-child");
	glade_command_set_property (property, FALSE);

	/* Setup reasonable defaults for button label. */
	property = glade_widget_get_property (button_editor->loaded_widget, "stock");
	glade_command_set_property (property, NULL);

	property = glade_widget_get_property (button_editor->loaded_widget, "use-stock");
	glade_command_set_property (property, FALSE);

	glade_widget_property_get (button_editor->loaded_widget, "use-action-appearance", &use_appearance);
	if (!use_appearance)
	{
		property = glade_widget_get_property (button_editor->loaded_widget, "label");
		glade_property_get_default (property, &value);
		glade_command_set_property_value (property, &value);
		g_value_unset (&value);
	}

	glade_command_pop_group ();

	button_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (button_editor), 
			     button_editor->loaded_widget);
}

static void
custom_toggled (GtkWidget         *widget,
		GladeButtonEditor *button_editor)
{
	GladeProperty     *property;

	if (button_editor->loading || !button_editor->loaded_widget)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_editor->custom_radio)))
		return;

	button_editor->modifying = TRUE;

	glade_command_push_group (_("Setting %s to use a custom child"), 
				  button_editor->loaded_widget->name);

	/* clear out some things... */
	property = glade_widget_get_property (button_editor->loaded_widget, "image");
	glade_command_set_property (property, NULL);

	property = glade_widget_get_property (button_editor->loaded_widget, "use-stock");
	glade_command_set_property (property, FALSE);

	property = glade_widget_get_property (button_editor->loaded_widget, "stock");
	glade_command_set_property (property, NULL);

	property = glade_widget_get_property (button_editor->loaded_widget, "label");
	glade_command_set_property (property, NULL);

	/* Add a placeholder via the custom-child property... */
	property = glade_widget_get_property (button_editor->loaded_widget, "custom-child");
	glade_command_set_property (property, TRUE);

	glade_command_pop_group ();

	button_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (button_editor), 
			     button_editor->loaded_widget);
}

static void
stock_toggled (GtkWidget         *widget,
	       GladeButtonEditor *button_editor)
{
	GladeProperty     *property;
	gboolean           use_appearance = FALSE;

	if (button_editor->loading || !button_editor->loaded_widget)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_editor->stock_radio)))
		return;

	button_editor->modifying = TRUE;

	glade_command_push_group (_("Setting %s to use a stock button"), button_editor->loaded_widget->name);

	/* clear out stuff... */
	property = glade_widget_get_property (button_editor->loaded_widget, "image");
	glade_command_set_property (property, NULL);

	glade_widget_property_get (button_editor->loaded_widget, "use-action-appearance", &use_appearance);
	if (!use_appearance)
	{
		property = glade_widget_get_property (button_editor->loaded_widget, "label");
		glade_command_set_property (property, "");
	}

	property = glade_widget_get_property (button_editor->loaded_widget, "use-stock");
	glade_command_set_property (property, TRUE);

	property = glade_widget_get_property (button_editor->loaded_widget, "stock");
	glade_command_set_property (property, NULL);

	glade_command_pop_group ();

	button_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (button_editor), 
			     button_editor->loaded_widget);
}

static void
label_toggled (GtkWidget         *widget,
	       GladeButtonEditor *button_editor)
{
	GladeProperty     *property;
	GValue             value = { 0, };
	gboolean           use_appearance = FALSE;

	if (button_editor->loading || !button_editor->loaded_widget)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_editor->label_radio)))
		return;

	button_editor->modifying = TRUE;

	glade_command_push_group (_("Setting %s to use a label and image"), button_editor->loaded_widget->name);

	property = glade_widget_get_property (button_editor->loaded_widget, "stock");
	glade_command_set_property (property, NULL);

	property = glade_widget_get_property (button_editor->loaded_widget, "use-stock");
	glade_command_set_property (property, FALSE);

	glade_widget_property_get (button_editor->loaded_widget, "use-action-appearance", &use_appearance);
	if (!use_appearance)
	{
		property = glade_widget_get_property (button_editor->loaded_widget, "label");
		glade_property_get_default (property, &value);
		glade_command_set_property_value (property, &value);
		g_value_unset (&value);
	}

	glade_command_pop_group ();

	button_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (button_editor), 
			     button_editor->loaded_widget);
}


static void
table_attach (GtkWidget *table, 
	      GtkWidget *child, 
	      gint pos, gint row,
	      GtkSizeGroup *group)
{
	gtk_table_attach (GTK_TABLE (table), child,
			  pos, pos+1, row, row +1,
			  pos ? 0 : GTK_EXPAND | GTK_FILL,
			  GTK_EXPAND | GTK_FILL,
			  3, 1);

	if (pos)
		gtk_size_group_add_widget (group, child);
}


GtkWidget *
glade_button_editor_new (GladeWidgetAdaptor *adaptor,
			 GladeEditable      *embed)
{
	GladeButtonEditor   *button_editor;
	GladeEditorProperty *eprop;
	GtkWidget           *vbox, *table, *frame;
	GtkSizeGroup        *group;

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (GLADE_IS_EDITABLE (embed), NULL);

	button_editor = g_object_new (GLADE_TYPE_BUTTON_EDITOR, NULL);
	button_editor->embed = GTK_WIDGET (embed);

	button_editor->standard_radio = gtk_radio_button_new_with_label (NULL, _("Configure button content"));
	button_editor->custom_radio   = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON (button_editor->standard_radio), _("Add custom button content"));

	button_editor->stock_radio = gtk_radio_button_new_with_label (NULL, _("Stock button"));
	button_editor->label_radio = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON (button_editor->stock_radio), _("Label with optional image"));

	g_signal_connect (G_OBJECT (button_editor->standard_radio), "toggled",
			  G_CALLBACK (standard_toggled), button_editor);
	g_signal_connect (G_OBJECT (button_editor->custom_radio), "toggled",
			  G_CALLBACK (custom_toggled), button_editor);
	g_signal_connect (G_OBJECT (button_editor->stock_radio), "toggled",
			  G_CALLBACK (stock_toggled), button_editor);
	g_signal_connect (G_OBJECT (button_editor->label_radio), "toggled",
			  G_CALLBACK (label_toggled), button_editor);

	/* Pack the parent on top... */
	gtk_box_pack_start (GTK_BOX (button_editor), GTK_WIDGET (embed), FALSE, FALSE, 0);

	/* Standard frame... */
	frame = gtk_frame_new (NULL);
	gtk_frame_set_label_widget (GTK_FRAME (frame), button_editor->standard_radio);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (button_editor), frame, FALSE, FALSE, 8);

	button_editor->standard_frame = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
	gtk_alignment_set_padding (GTK_ALIGNMENT (button_editor->standard_frame), 6, 0, 12, 0);
	gtk_container_add (GTK_CONTAINER (frame), button_editor->standard_frame);

	vbox = gtk_vbox_new (FALSE, 8);
	gtk_container_add (GTK_CONTAINER (button_editor->standard_frame), vbox);

	/* Populate stock frame here... */
	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_frame_set_label_widget (GTK_FRAME (frame), button_editor->stock_radio);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 4);

	button_editor->stock_frame = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
	gtk_alignment_set_padding (GTK_ALIGNMENT (button_editor->stock_frame), 6, 0, 12, 0);
	gtk_container_add (GTK_CONTAINER (frame), button_editor->stock_frame);

	table = gtk_table_new (0, 0, FALSE);
	group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_container_add (GTK_CONTAINER (button_editor->stock_frame), table);

	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "stock", FALSE, TRUE);
	table_attach (table, eprop->item_label, 0, 0, group);
	table_attach (table, GTK_WIDGET (eprop), 1, 0, group);
	button_editor->properties = g_list_prepend (button_editor->properties, eprop);

	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "image-position", FALSE, TRUE);
	table_attach (table, eprop->item_label, 0, 1, group);
	table_attach (table, GTK_WIDGET (eprop), 1, 1, group);
	button_editor->properties = g_list_prepend (button_editor->properties, eprop);

	g_object_unref (group);

	/* Populate label frame here... */
	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_frame_set_label_widget (GTK_FRAME (frame), button_editor->label_radio);
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 4);

	button_editor->label_frame = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
	gtk_alignment_set_padding (GTK_ALIGNMENT (button_editor->label_frame), 6, 0, 12, 0);
	gtk_container_add (GTK_CONTAINER (frame), button_editor->label_frame);

	table = gtk_table_new (0, 0, FALSE);
	group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_container_add (GTK_CONTAINER (button_editor->label_frame), table);

	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "label", FALSE, TRUE);
	table_attach (table, eprop->item_label, 0, 0, group);
	table_attach (table, GTK_WIDGET (eprop), 1, 0, group);
	button_editor->properties = g_list_prepend (button_editor->properties, eprop);

	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "use-underline", FALSE, TRUE);
	table_attach (table, eprop->item_label, 0, 1, group);
	table_attach (table, GTK_WIDGET (eprop), 1, 1, group);
	button_editor->properties = g_list_prepend (button_editor->properties, eprop);

	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "image", FALSE, TRUE);
	table_attach (table, eprop->item_label, 0, 2, group);
	table_attach (table, GTK_WIDGET (eprop), 1, 2, group);
	button_editor->properties = g_list_prepend (button_editor->properties, eprop);

	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "image-position", FALSE, TRUE);
	table_attach (table, eprop->item_label, 0, 3, group);
	table_attach (table, GTK_WIDGET (eprop), 1, 3, group);
	button_editor->properties = g_list_prepend (button_editor->properties, eprop);

	g_object_unref (group);
	
	/* Custom radio button on the bottom */
	gtk_box_pack_start (GTK_BOX (button_editor), button_editor->custom_radio, FALSE, FALSE, 0);

	gtk_widget_show_all (GTK_WIDGET (button_editor));

	return GTK_WIDGET (button_editor);
}
