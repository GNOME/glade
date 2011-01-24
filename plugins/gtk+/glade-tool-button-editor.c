/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Tristan Van Berkom.
 *
 * This library is free software; you can redistribute it and/or modify it
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

#include "glade-tool-button-editor.h"


static void glade_tool_button_editor_finalize        (GObject              *object);

static void glade_tool_button_editor_editable_init   (GladeEditableIface *iface);

static void glade_tool_button_editor_grab_focus      (GtkWidget            *widget);


G_DEFINE_TYPE_WITH_CODE (GladeToolButtonEditor, glade_tool_button_editor, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_tool_button_editor_editable_init));


static void
glade_tool_button_editor_class_init (GladeToolButtonEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize     = glade_tool_button_editor_finalize;
	widget_class->grab_focus   = glade_tool_button_editor_grab_focus;
}

static void
glade_tool_button_editor_init (GladeToolButtonEditor *self)
{
}

static void
project_changed (GladeProject      *project,
		 GladeCommand      *command,
		 gboolean           execute,
		 GladeToolButtonEditor *button_editor)
{
	if (button_editor->modifying ||
	    !gtk_widget_get_mapped (GTK_WIDGET (button_editor)))
		return;

	/* Reload on all commands */
	glade_editable_load (GLADE_EDITABLE (button_editor), button_editor->loaded_widget);
}


static void
project_finalized (GladeToolButtonEditor *button_editor,
		   GladeProject       *where_project_was)
{
	button_editor->loaded_widget = NULL;

	glade_editable_load (GLADE_EDITABLE (button_editor), NULL);
}

static void
glade_tool_button_editor_load (GladeEditable *editable,
			       GladeWidget   *widget)
{
	GladeToolButtonEditor     *button_editor = GLADE_TOOL_BUTTON_EDITOR (editable);
	gboolean                   custom_label  = FALSE, use_appearance = FALSE;
	GladeToolButtonImageMode   image_mode    = 0;
	GList                     *l;

	button_editor->loading = TRUE;

	/* Since we watch the project*/
	if (button_editor->loaded_widget)
	{
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
		glade_widget_property_get (widget, "image-mode", &image_mode);
		glade_widget_property_get (widget, "custom-label", &custom_label);
		glade_widget_property_get (widget, "use-action-appearance", &use_appearance);

		if (custom_label)
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON (button_editor->custom_label_radio), TRUE);
		else
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON (button_editor->standard_label_radio), TRUE);
		
		switch (image_mode)
		{
		case GLADE_TB_MODE_STOCK:
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_editor->stock_radio), TRUE);
			break;
		case GLADE_TB_MODE_ICON:
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_editor->icon_radio), TRUE);
			break;
		case GLADE_TB_MODE_FILENAME:
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_editor->file_radio), TRUE);
			break;
		case GLADE_TB_MODE_CUSTOM:
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_editor->custom_radio), TRUE);
			break;
		default:
			break;
		}

		if (use_appearance)
		{
			gtk_widget_set_sensitive (button_editor->label_table, FALSE);
			gtk_widget_set_sensitive (button_editor->image_table, FALSE);
		}
		else
		{
			gtk_widget_set_sensitive (button_editor->label_table, TRUE);
			gtk_widget_set_sensitive (button_editor->image_table, TRUE);
		}
	}
	button_editor->loading = FALSE;
}


static void
standard_label_toggled (GtkWidget             *widget,
			GladeToolButtonEditor *button_editor)
{
	GladeProperty     *property;
	GValue             value = { 0, };

	if (button_editor->loading || !button_editor->loaded_widget)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_editor->standard_label_radio)))
		return;

	button_editor->modifying = TRUE;

	glade_command_push_group (_("Setting %s to use standard label text"), button_editor->loaded_widget->name);

	property = glade_widget_get_property (button_editor->loaded_widget, "label-widget");
	glade_command_set_property (property, NULL);

	property = glade_widget_get_property (button_editor->loaded_widget, "label");
	glade_property_get_default (property, &value);
	glade_command_set_property_value (property, &value);
	g_value_unset (&value);
	property = glade_widget_get_property (button_editor->loaded_widget, "custom-label");
	glade_command_set_property (property, FALSE);

	glade_command_pop_group ();

	button_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (button_editor), 
			     button_editor->loaded_widget);
}

static void
custom_label_toggled (GtkWidget             *widget,
		      GladeToolButtonEditor *button_editor)
{
	GladeProperty     *property;

	if (button_editor->loading || !button_editor->loaded_widget)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_editor->custom_label_radio)))
		return;

	button_editor->modifying = TRUE;

	glade_command_push_group (_("Setting %s to use a custom label widget"), button_editor->loaded_widget->name);

	property = glade_widget_get_property (button_editor->loaded_widget, "label");
	glade_command_set_property (property, NULL);
	property = glade_widget_get_property (button_editor->loaded_widget, "custom-label");
	glade_command_set_property (property, TRUE);

	glade_command_pop_group ();

	button_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (button_editor), 
			     button_editor->loaded_widget);
}

static void
stock_toggled (GtkWidget             *widget,
	       GladeToolButtonEditor *button_editor)
{
	GladeProperty     *property;

	if (button_editor->loading || !button_editor->loaded_widget)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_editor->stock_radio)))
		return;

	button_editor->modifying = TRUE;

	glade_command_push_group (_("Setting %s to use an image from stock"), button_editor->loaded_widget->name);

	property = glade_widget_get_property (button_editor->loaded_widget, "icon-name");
	glade_command_set_property (property, NULL);
	property = glade_widget_get_property (button_editor->loaded_widget, "icon");
	glade_command_set_property (property, NULL);
	property = glade_widget_get_property (button_editor->loaded_widget, "icon-widget");
	glade_command_set_property (property, NULL);
	property = glade_widget_get_property (button_editor->loaded_widget, "image-mode");
	glade_command_set_property (property, GLADE_TB_MODE_STOCK);

	glade_command_pop_group ();

	button_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (button_editor), 
			     button_editor->loaded_widget);
}


static void
icon_toggled (GtkWidget             *widget,
	      GladeToolButtonEditor *button_editor)
{
	GladeProperty     *property;

	if (button_editor->loading || !button_editor->loaded_widget)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_editor->icon_radio)))
		return;

	button_editor->modifying = TRUE;

	glade_command_push_group (_("Setting %s to use an image from the icon theme"), button_editor->loaded_widget->name);

	property = glade_widget_get_property (button_editor->loaded_widget, "stock-id");
	glade_command_set_property (property, NULL);
	property = glade_widget_get_property (button_editor->loaded_widget, "icon");
	glade_command_set_property (property, NULL);
	property = glade_widget_get_property (button_editor->loaded_widget, "icon-widget");
	glade_command_set_property (property, NULL);
	property = glade_widget_get_property (button_editor->loaded_widget, "image-mode");
	glade_command_set_property (property, GLADE_TB_MODE_ICON);

	glade_command_pop_group ();

	button_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (button_editor), 
			     button_editor->loaded_widget);
}

static void
file_toggled (GtkWidget             *widget,
	      GladeToolButtonEditor *button_editor)
{
	GladeProperty     *property;

	if (button_editor->loading || !button_editor->loaded_widget)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_editor->file_radio)))
		return;

	button_editor->modifying = TRUE;

	glade_command_push_group (_("Setting %s to use an image from the icon theme"), button_editor->loaded_widget->name);

	property = glade_widget_get_property (button_editor->loaded_widget, "stock-id");
	glade_command_set_property (property, NULL);
	property = glade_widget_get_property (button_editor->loaded_widget, "icon-name");
	glade_command_set_property (property, NULL);
	property = glade_widget_get_property (button_editor->loaded_widget, "icon-widget");
	glade_command_set_property (property, NULL);
	property = glade_widget_get_property (button_editor->loaded_widget, "image-mode");
	glade_command_set_property (property, GLADE_TB_MODE_FILENAME);

	glade_command_pop_group ();

	button_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (button_editor), 
			     button_editor->loaded_widget);
}

static void
custom_toggled (GtkWidget             *widget,
		GladeToolButtonEditor *button_editor)
{
	GladeProperty     *property;

	if (button_editor->loading || !button_editor->loaded_widget)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button_editor->custom_radio)))
		return;

	button_editor->modifying = TRUE;

	glade_command_push_group (_("Setting %s to use an image from the icon theme"), button_editor->loaded_widget->name);

	property = glade_widget_get_property (button_editor->loaded_widget, "stock-id");
	glade_command_set_property (property, NULL);
	property = glade_widget_get_property (button_editor->loaded_widget, "icon-name");
	glade_command_set_property (property, NULL);
	property = glade_widget_get_property (button_editor->loaded_widget, "icon");
	glade_command_set_property (property, NULL);
	property = glade_widget_get_property (button_editor->loaded_widget, "image-mode");
	glade_command_set_property (property, GLADE_TB_MODE_CUSTOM);

	glade_command_pop_group ();

	button_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (button_editor), 
			     button_editor->loaded_widget);
}

static void
glade_tool_button_editor_set_show_name (GladeEditable *editable,
					gboolean       show_name)
{
	GladeToolButtonEditor *button_editor = GLADE_TOOL_BUTTON_EDITOR (editable);

	glade_editable_set_show_name (GLADE_EDITABLE (button_editor->embed), show_name);
}

static void
glade_tool_button_editor_editable_init (GladeEditableIface *iface)
{
	iface->load = glade_tool_button_editor_load;
	iface->set_show_name = glade_tool_button_editor_set_show_name;
}

static void
glade_tool_button_editor_finalize (GObject *object)
{
	GladeToolButtonEditor *button_editor = GLADE_TOOL_BUTTON_EDITOR (object);

	if (button_editor->properties)
		g_list_free (button_editor->properties);
	button_editor->properties = NULL;
	button_editor->embed      = NULL;

	glade_editable_load (GLADE_EDITABLE (object), NULL);

	G_OBJECT_CLASS (glade_tool_button_editor_parent_class)->finalize (object);
}

static void
glade_tool_button_editor_grab_focus (GtkWidget *widget)
{
	GladeToolButtonEditor *button_editor = GLADE_TOOL_BUTTON_EDITOR (widget);

	gtk_widget_grab_focus (button_editor->embed);
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
glade_tool_button_editor_new (GladeWidgetAdaptor *adaptor,
			      GladeEditable      *embed)
{
	GladeToolButtonEditor   *button_editor;
	GladeEditorProperty     *eprop;
	GtkWidget               *label, *alignment, *frame, *table, *hbox;
	GtkSizeGroup            *group;
	gchar                   *str;

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (GLADE_IS_EDITABLE (embed), NULL);

	button_editor = g_object_new (GLADE_TYPE_TOOL_BUTTON_EDITOR, NULL);
	button_editor->embed = GTK_WIDGET (embed);

	/* Pack the parent on top... */
	gtk_box_pack_start (GTK_BOX (button_editor), GTK_WIDGET (embed), FALSE, FALSE, 0);

	/* Label area frame... */
	str = g_strdup_printf ("<b>%s</b>", _("Edit Label"));
	label = gtk_label_new (str);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	g_free (str);
	frame = gtk_frame_new (NULL);
	gtk_frame_set_label_widget (GTK_FRAME (frame), label);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (button_editor), frame, FALSE, FALSE, 12);

	alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
	gtk_container_add (GTK_CONTAINER (frame), alignment);

	button_editor->label_table = table = gtk_table_new (0, 0, FALSE);
	gtk_container_add (GTK_CONTAINER (alignment), table);

	group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	/* Standard label... */
	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "label", FALSE, TRUE);
	hbox  = gtk_hbox_new (FALSE, 0);
	button_editor->standard_label_radio = gtk_radio_button_new (NULL);
	gtk_box_pack_start (GTK_BOX (hbox), button_editor->standard_label_radio, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), eprop->item_label, TRUE, TRUE, 2);
	table_attach (table, hbox, 0, 0, group);
	table_attach (table, GTK_WIDGET (eprop), 1, 0, group);
	button_editor->properties = g_list_prepend (button_editor->properties, eprop);

	/* Custom label... */
	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "label-widget", FALSE, TRUE);
	hbox  = gtk_hbox_new (FALSE, 0);
	button_editor->custom_label_radio = gtk_radio_button_new_from_widget
	  (GTK_RADIO_BUTTON (button_editor->standard_label_radio));
	gtk_box_pack_start (GTK_BOX (hbox), button_editor->custom_label_radio, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), eprop->item_label, TRUE, TRUE, 2);
	table_attach (table, hbox, 0, 1, group);
	table_attach (table, GTK_WIDGET (eprop), 1, 1, group);
	button_editor->properties = g_list_prepend (button_editor->properties, eprop);

	g_object_unref (group);

	/* Image area frame... */
	str = g_strdup_printf ("<b>%s</b>", _("Edit Image"));
	label = gtk_label_new (str);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	g_free (str);
	frame = gtk_frame_new (NULL);
	gtk_frame_set_label_widget (GTK_FRAME (frame), label);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (button_editor), frame, FALSE, FALSE, 8);

	alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
	gtk_container_add (GTK_CONTAINER (frame), alignment);

	button_editor->image_table = table = gtk_table_new (0, 0, FALSE);
	gtk_container_add (GTK_CONTAINER (alignment), table);

	group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	/* Stock image... */
	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "stock-id", FALSE, TRUE);
	hbox  = gtk_hbox_new (FALSE, 0);
	button_editor->stock_radio = gtk_radio_button_new (NULL);
	gtk_box_pack_start (GTK_BOX (hbox), button_editor->stock_radio, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), eprop->item_label, TRUE, TRUE, 2);
	table_attach (table, hbox, 0, 0, group);
	table_attach (table, GTK_WIDGET (eprop), 1, 0, group);
	button_editor->properties = g_list_prepend (button_editor->properties, eprop);

	/* Icon theme image... */
	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "icon-name", FALSE, TRUE);
	hbox  = gtk_hbox_new (FALSE, 0);
	button_editor->icon_radio = gtk_radio_button_new_from_widget
	  (GTK_RADIO_BUTTON (button_editor->stock_radio));
	gtk_box_pack_start (GTK_BOX (hbox), button_editor->icon_radio, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), eprop->item_label, TRUE, TRUE, 2);
	table_attach (table, hbox, 0, 1, group);
	table_attach (table, GTK_WIDGET (eprop), 1, 1, group);
	button_editor->properties = g_list_prepend (button_editor->properties, eprop);

	/* Filename... */
	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "icon", FALSE, TRUE);
	hbox  = gtk_hbox_new (FALSE, 0);
	button_editor->file_radio = gtk_radio_button_new_from_widget 
	  (GTK_RADIO_BUTTON (button_editor->stock_radio));
	gtk_box_pack_start (GTK_BOX (hbox), button_editor->file_radio, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), eprop->item_label, TRUE, TRUE, 2);
	table_attach (table, hbox, 0, 2, group);
	table_attach (table, GTK_WIDGET (eprop), 1, 2, group);
	button_editor->properties = g_list_prepend (button_editor->properties, eprop);

	/* Custom embedded image widget... */
	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "icon-widget", FALSE, TRUE);
	hbox  = gtk_hbox_new (FALSE, 0);
	button_editor->custom_radio = gtk_radio_button_new_from_widget
		(GTK_RADIO_BUTTON (button_editor->stock_radio));
	gtk_box_pack_start (GTK_BOX (hbox), button_editor->custom_radio, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), eprop->item_label, TRUE, TRUE, 2);
	table_attach (table, hbox, 0, 3, group);
	table_attach (table, GTK_WIDGET (eprop), 1, 3, group);
	button_editor->properties = g_list_prepend (button_editor->properties, eprop);

	g_object_unref (group);

	/* Connect radio button signals... */
	g_signal_connect (G_OBJECT (button_editor->standard_label_radio), "toggled",
			  G_CALLBACK (standard_label_toggled), button_editor);
	g_signal_connect (G_OBJECT (button_editor->custom_label_radio), "toggled",
			  G_CALLBACK (custom_label_toggled), button_editor);
	g_signal_connect (G_OBJECT (button_editor->stock_radio), "toggled",
			  G_CALLBACK (stock_toggled), button_editor);
	g_signal_connect (G_OBJECT (button_editor->icon_radio), "toggled",
			  G_CALLBACK (icon_toggled), button_editor);
	g_signal_connect (G_OBJECT (button_editor->file_radio), "toggled",
			  G_CALLBACK (file_toggled), button_editor);
	g_signal_connect (G_OBJECT (button_editor->custom_radio), "toggled",
			  G_CALLBACK (custom_toggled), button_editor);

	gtk_widget_show_all (GTK_WIDGET (button_editor));

	return GTK_WIDGET (button_editor);
}
