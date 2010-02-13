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

#include "glade-label-editor.h"


static void glade_label_editor_finalize        (GObject              *object);

static void glade_label_editor_editable_init   (GladeEditableIface *iface);

static void glade_label_editor_grab_focus      (GtkWidget            *widget);


G_DEFINE_TYPE_WITH_CODE (GladeLabelEditor, glade_label_editor, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_label_editor_editable_init));


static void
glade_label_editor_class_init (GladeLabelEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize     = glade_label_editor_finalize;
	widget_class->grab_focus   = glade_label_editor_grab_focus;
}

static void
glade_label_editor_init (GladeLabelEditor *self)
{
	self->appearance_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	self->formatting_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	self->wrap_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
}

static void
project_changed (GladeProject      *project,
		 GladeCommand      *command,
		 gboolean           execute,
		 GladeLabelEditor *label_editor)
{
	if (label_editor->modifying ||
	    !gtk_widget_get_mapped (GTK_WIDGET (label_editor)))
		return;

	/* Reload on all commands */
	glade_editable_load (GLADE_EDITABLE (label_editor), label_editor->loaded_widget);
}


static void
project_finalized (GladeLabelEditor *label_editor,
		   GladeProject       *where_project_was)
{
	label_editor->loaded_widget = NULL;

	glade_editable_load (GLADE_EDITABLE (label_editor), NULL);
}

static void
glade_label_editor_load (GladeEditable *editable,
			  GladeWidget   *widget)
{
	GladeLabelEditor    *label_editor = GLADE_LABEL_EDITOR (editable);
	GList               *l;

	label_editor->loading = TRUE;

	/* Since we watch the project*/
	if (label_editor->loaded_widget)
	{
		/* watch custom-child and use-stock properties here for reloads !!! */

		g_signal_handlers_disconnect_by_func (G_OBJECT (label_editor->loaded_widget->project),
						      G_CALLBACK (project_changed), label_editor);

		/* The widget could die unexpectedly... */
		g_object_weak_unref (G_OBJECT (label_editor->loaded_widget->project),
				     (GWeakNotify)project_finalized,
				     label_editor);
	}

	/* Mark our widget... */
	label_editor->loaded_widget = widget;

	if (label_editor->loaded_widget)
	{
		/* This fires for undo/redo */
		g_signal_connect (G_OBJECT (label_editor->loaded_widget->project), "changed",
				  G_CALLBACK (project_changed), label_editor);

		/* The widget/project could die unexpectedly... */
		g_object_weak_ref (G_OBJECT (label_editor->loaded_widget->project),
				   (GWeakNotify)project_finalized,
				   label_editor);
	}

	/* load the embedded editable... */
	if (label_editor->embed)
		glade_editable_load (GLADE_EDITABLE (label_editor->embed), widget);

	for (l = label_editor->properties; l; l = l->next)
		glade_editor_property_load_by_widget (GLADE_EDITOR_PROPERTY (l->data), widget);

	if (widget)
	{
		GladeLabelContentMode content_mode;
		GladeLabelWrapMode    wrap_mode;
		static PangoAttrList *bold_attr_list = NULL;
		gboolean              use_max_width;

		if (!bold_attr_list)
		{
			PangoAttribute *attr;
			bold_attr_list = pango_attr_list_new ();
			attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
			pango_attr_list_insert (bold_attr_list, attr);
		}

		glade_widget_property_get (widget, "label-content-mode", &content_mode);
		glade_widget_property_get (widget, "label-wrap-mode", &wrap_mode);
		glade_widget_property_get (widget, "use-max-width", &use_max_width);

		switch (content_mode)
		{
		case GLADE_LABEL_MODE_ATTRIBUTES:
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (label_editor->attributes_radio), TRUE);
			break;
		case GLADE_LABEL_MODE_MARKUP:
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (label_editor->markup_radio), TRUE);
			break;
		case GLADE_LABEL_MODE_PATTERN:
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (label_editor->pattern_radio), TRUE);
			break;
		default:
			break;
		}

		if (wrap_mode == GLADE_LABEL_WRAP_FREE)
			gtk_label_set_attributes (GTK_LABEL (label_editor->wrap_free_label), bold_attr_list);
		else
			gtk_label_set_attributes (GTK_LABEL (label_editor->wrap_free_label), NULL);

		switch (wrap_mode)
		{
		case GLADE_LABEL_WRAP_FREE:
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (label_editor->wrap_free_radio), TRUE);
			break;
		case GLADE_LABEL_SINGLE_LINE:
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (label_editor->single_radio), TRUE);
			break;
		case GLADE_LABEL_WRAP_MODE:
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (label_editor->wrap_mode_radio), TRUE);
			break;
		default:
			break;
		}

		if (use_max_width)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (label_editor->max_width_radio), TRUE);
		else
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (label_editor->width_radio), TRUE);

	}
	label_editor->loading = FALSE;
}

static void
glade_label_editor_set_show_name (GladeEditable *editable,
				   gboolean       show_name)
{
	GladeLabelEditor *label_editor = GLADE_LABEL_EDITOR (editable);

	glade_editable_set_show_name (GLADE_EDITABLE (label_editor->embed), show_name);
}

static void
glade_label_editor_editable_init (GladeEditableIface *iface)
{
	iface->load = glade_label_editor_load;
	iface->set_show_name = glade_label_editor_set_show_name;
}

static void
glade_label_editor_finalize (GObject *object)
{
	GladeLabelEditor *label_editor = GLADE_LABEL_EDITOR (object);

	if (label_editor->properties)
		g_list_free (label_editor->properties);
	label_editor->properties = NULL;
	label_editor->embed      = NULL;

	glade_editable_load (GLADE_EDITABLE (object), NULL);

	g_object_unref (label_editor->appearance_group);
	g_object_unref (label_editor->formatting_group);
	g_object_unref (label_editor->wrap_group);

	G_OBJECT_CLASS (glade_label_editor_parent_class)->finalize (object);
}

static void
glade_label_editor_grab_focus (GtkWidget *widget)
{
	GladeLabelEditor *label_editor = GLADE_LABEL_EDITOR (widget);

	gtk_widget_grab_focus (label_editor->embed);
}


/**********************************************************************
                    label-content-mode radios
 **********************************************************************/
static void
attributes_toggled (GtkWidget        *widget,
		    GladeLabelEditor *label_editor)
{
	GladeProperty     *property;

	if (label_editor->loading || !label_editor->loaded_widget)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (label_editor->attributes_radio)))
		return;

	label_editor->modifying = TRUE;

	glade_command_push_group (_("Setting %s to use an attribute list"), label_editor->loaded_widget->name);

	property = glade_widget_get_property (label_editor->loaded_widget, "use-markup");
	glade_command_set_property (property, FALSE);
	property = glade_widget_get_property (label_editor->loaded_widget, "pattern");
	glade_command_set_property (property, NULL);
	property = glade_widget_get_property (label_editor->loaded_widget, "label-content-mode");
	glade_command_set_property (property, GLADE_LABEL_MODE_ATTRIBUTES);

	glade_command_pop_group ();

	label_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (label_editor), 
			     label_editor->loaded_widget);
}

static void
markup_toggled (GtkWidget        *widget,
		GladeLabelEditor *label_editor)
{
	GladeProperty     *property;

	if (label_editor->loading || !label_editor->loaded_widget)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (label_editor->markup_radio)))
		return;

	label_editor->modifying = TRUE;

	glade_command_push_group (_("Setting %s to use a Pango markup string"), label_editor->loaded_widget->name);

	property = glade_widget_get_property (label_editor->loaded_widget, "pattern");
	glade_command_set_property (property, NULL);
	property = glade_widget_get_property (label_editor->loaded_widget, "glade-attributes");
	glade_command_set_property (property, NULL);

	property = glade_widget_get_property (label_editor->loaded_widget, "use-markup");
	glade_command_set_property (property, TRUE);
	property = glade_widget_get_property (label_editor->loaded_widget, "label-content-mode");
	glade_command_set_property (property, GLADE_LABEL_MODE_MARKUP);

	glade_command_pop_group ();

	label_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (label_editor), 
			     label_editor->loaded_widget);
}

static void
pattern_toggled (GtkWidget        *widget,
		 GladeLabelEditor *label_editor)
{
	GladeProperty     *property;

	if (label_editor->loading || !label_editor->loaded_widget)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (label_editor->pattern_radio)))
		return;

	label_editor->modifying = TRUE;

	glade_command_push_group (_("Setting %s to use a pattern string"), label_editor->loaded_widget->name);

	property = glade_widget_get_property (label_editor->loaded_widget, "glade-attributes");
	glade_command_set_property (property, NULL);
	property = glade_widget_get_property (label_editor->loaded_widget, "use-markup");
	glade_command_set_property (property, FALSE);
	property = glade_widget_get_property (label_editor->loaded_widget, "label-content-mode");
	glade_command_set_property (property, GLADE_LABEL_MODE_PATTERN);

	glade_command_pop_group ();

	label_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (label_editor), 
			     label_editor->loaded_widget);
}

/**********************************************************************
                    use-max-width radios
 **********************************************************************/

static void
width_toggled (GtkWidget        *widget,
	       GladeLabelEditor *label_editor)
{
	GladeProperty     *property;

	if (label_editor->loading || !label_editor->loaded_widget)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (label_editor->width_radio)))
		return;

	label_editor->modifying = TRUE;

	glade_command_push_group (_("Setting %s to set desired width in characters"), 
				  label_editor->loaded_widget->name);

	property = glade_widget_get_property (label_editor->loaded_widget, "max-width-chars");
	glade_command_set_property (property, -1);
	property = glade_widget_get_property (label_editor->loaded_widget, "use-max-width");
	glade_command_set_property (property, FALSE);

	glade_command_pop_group ();

	label_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (label_editor), 
			     label_editor->loaded_widget);
}

static void
max_width_toggled (GtkWidget        *widget,
		   GladeLabelEditor *label_editor)
{
	GladeProperty     *property;

	if (label_editor->loading || !label_editor->loaded_widget)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (label_editor->max_width_radio)))
		return;

	label_editor->modifying = TRUE;

	glade_command_push_group (_("Setting %s to set maximum width in characters"), 
				  label_editor->loaded_widget->name);

	property = glade_widget_get_property (label_editor->loaded_widget, "width-chars");
	glade_command_set_property (property, -1);
	property = glade_widget_get_property (label_editor->loaded_widget, "use-max-width");
	glade_command_set_property (property, TRUE);

	glade_command_pop_group ();

	label_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (label_editor), 
			     label_editor->loaded_widget);
}

/**********************************************************************
                    label-wrap-mode radios
 **********************************************************************/
static void
wrap_free_toggled (GtkWidget        *widget,
		   GladeLabelEditor *label_editor)
{
	GladeProperty     *property;

	if (label_editor->loading || !label_editor->loaded_widget)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (label_editor->wrap_free_radio)))
		return;

	label_editor->modifying = TRUE;

	glade_command_push_group (_("Setting %s to use normal line wrapping"), label_editor->loaded_widget->name);

	property = glade_widget_get_property (label_editor->loaded_widget, "single-line-mode");
	glade_command_set_property (property, FALSE);
	property = glade_widget_get_property (label_editor->loaded_widget, "wrap-mode");
	glade_command_set_property (property, PANGO_WRAP_WORD);
	property = glade_widget_get_property (label_editor->loaded_widget, "wrap");
	glade_command_set_property (property, FALSE);

	property = glade_widget_get_property (label_editor->loaded_widget, "label-wrap-mode");
	glade_command_set_property (property, GLADE_LABEL_WRAP_FREE);

	glade_command_pop_group ();

	label_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (label_editor), 
			     label_editor->loaded_widget);
}

static void
single_toggled (GtkWidget        *widget,
		GladeLabelEditor *label_editor)
{
	GladeProperty     *property;

	if (label_editor->loading || !label_editor->loaded_widget)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (label_editor->single_radio)))
		return;

	label_editor->modifying = TRUE;

	glade_command_push_group (_("Setting %s to use a single line"), label_editor->loaded_widget->name);

	property = glade_widget_get_property (label_editor->loaded_widget, "wrap-mode");
	glade_command_set_property (property, PANGO_WRAP_WORD);
	property = glade_widget_get_property (label_editor->loaded_widget, "wrap");
	glade_command_set_property (property, FALSE);

	property = glade_widget_get_property (label_editor->loaded_widget, "single-line-mode");
	glade_command_set_property (property, TRUE);
	property = glade_widget_get_property (label_editor->loaded_widget, "label-wrap-mode");
	glade_command_set_property (property, GLADE_LABEL_SINGLE_LINE);

	glade_command_pop_group ();

	label_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (label_editor), 
			     label_editor->loaded_widget);
}

static void
wrap_mode_toggled (GtkWidget        *widget,
		   GladeLabelEditor *label_editor)
{
	GladeProperty     *property;

	if (label_editor->loading || !label_editor->loaded_widget)
		return;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (label_editor->wrap_mode_radio)))
		return;

	label_editor->modifying = TRUE;

	glade_command_push_group (_("Setting %s to use specific Pango word wrapping"),
				  label_editor->loaded_widget->name);

	property = glade_widget_get_property (label_editor->loaded_widget, "single-line-mode");
	glade_command_set_property (property, FALSE);
	property = glade_widget_get_property (label_editor->loaded_widget, "wrap");
	glade_command_set_property (property, TRUE);

	property = glade_widget_get_property (label_editor->loaded_widget, "label-wrap-mode");
	glade_command_set_property (property, GLADE_LABEL_WRAP_MODE);

	glade_command_pop_group ();

	label_editor->modifying = FALSE;

	/* reload buttons and sensitivity and stuff... */
	glade_editable_load (GLADE_EDITABLE (label_editor), 
			     label_editor->loaded_widget);
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

static void
append_label_appearance (GladeLabelEditor   *label_editor,
			 GladeWidgetAdaptor *adaptor)
{
	GladeEditorProperty *eprop, *markup_property;
	GtkWidget           *table, *frame, *alignment, *label, *hbox;
	gchar               *str;

	/* Label appearance... */
	str = g_strdup_printf ("<b>%s</b>", _("Edit label appearance"));
	label = gtk_label_new (str);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	g_free (str);
	frame = gtk_frame_new (NULL);
	gtk_frame_set_label_widget (GTK_FRAME (frame), label);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (label_editor), frame, FALSE, FALSE, 8);

	alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
	gtk_container_add (GTK_CONTAINER (frame), alignment);

	table = gtk_table_new (0, 0, FALSE);
	gtk_container_add (GTK_CONTAINER (alignment), table);

	/* Edit the label itself... */
	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "label", FALSE, TRUE);
	table_attach (table, eprop->item_label, 0, 0, label_editor->appearance_group);
	table_attach (table, GTK_WIDGET (eprop), 1, 0, label_editor->appearance_group);
	label_editor->properties = g_list_prepend (label_editor->properties, eprop);

	/* Edit by attributes... */
	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "glade-attributes", FALSE, TRUE);
	hbox  = gtk_hbox_new (FALSE, 0);
	label_editor->attributes_radio = gtk_radio_button_new (NULL);
	gtk_box_pack_start (GTK_BOX (hbox), label_editor->attributes_radio, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), eprop->item_label, TRUE, TRUE, 2);
	table_attach (table, hbox, 0, 1, label_editor->appearance_group);
	table_attach (table, GTK_WIDGET (eprop), 1, 1, label_editor->appearance_group);
	label_editor->properties = g_list_prepend (label_editor->properties, eprop);

	/* Edit with label as pango markup strings... */
	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "use-markup", FALSE, TRUE);
	hbox  = gtk_hbox_new (FALSE, 0);
	label_editor->markup_radio = gtk_radio_button_new_from_widget
		(GTK_RADIO_BUTTON (label_editor->attributes_radio));
	gtk_box_pack_start (GTK_BOX (hbox), label_editor->markup_radio, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), eprop->item_label, TRUE, TRUE, 2);
	table_attach (table, hbox, 0, 2, label_editor->appearance_group);
	label_editor->properties = g_list_prepend (label_editor->properties, eprop);
	markup_property = eprop; /* Its getting into a hidden row on the bottom... */

	/* Add underscores from pattern string... */
	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "pattern", FALSE, TRUE);
	hbox  = gtk_hbox_new (FALSE, 0);
	label_editor->pattern_radio = gtk_radio_button_new_from_widget
		(GTK_RADIO_BUTTON (label_editor->attributes_radio));
	gtk_box_pack_start (GTK_BOX (hbox), label_editor->pattern_radio, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), eprop->item_label, TRUE, TRUE, 2);
	table_attach (table, hbox, 0, 3, label_editor->appearance_group);
	table_attach (table, GTK_WIDGET (eprop), 1, 3, label_editor->appearance_group);
	label_editor->properties = g_list_prepend (label_editor->properties, eprop);

	/* Add invisible eprops at the end of this table... */
	gtk_widget_set_no_show_all (GTK_WIDGET (markup_property), TRUE);
	gtk_widget_hide (GTK_WIDGET (markup_property));
	table_attach (table, GTK_WIDGET (markup_property), 0, 4, label_editor->appearance_group);
}


static void
append_label_formatting (GladeLabelEditor   *label_editor,
			 GladeWidgetAdaptor *adaptor)
{
	GladeEditorProperty *eprop;
	GtkWidget           *table, *frame, *alignment, *label, *hbox;
	gchar               *str;
	gint                 row = 0;

	/* Label formatting... */
	str = g_strdup_printf ("<b>%s</b>", _("Format label"));
	label = gtk_label_new (str);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	g_free (str);
	frame = gtk_frame_new (NULL);
	gtk_frame_set_label_widget (GTK_FRAME (frame), label);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (label_editor), frame, FALSE, FALSE, 0);

	alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
	gtk_container_add (GTK_CONTAINER (frame), alignment);

	table = gtk_table_new (0, 0, FALSE);
	gtk_container_add (GTK_CONTAINER (alignment), table);

	/* ellipsize... */
	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "ellipsize", FALSE, TRUE);
	table_attach (table, eprop->item_label, 0, row, label_editor->formatting_group);
	table_attach (table, GTK_WIDGET (eprop), 1, row++, label_editor->formatting_group);
	label_editor->properties = g_list_prepend (label_editor->properties, eprop);

	/* justify... */
	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "justify", FALSE, TRUE);
	table_attach (table, eprop->item_label, 0, row, label_editor->formatting_group);
	table_attach (table, GTK_WIDGET (eprop), 1, row++, label_editor->formatting_group);
	label_editor->properties = g_list_prepend (label_editor->properties, eprop);

	/* angle... */
	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "angle", FALSE, TRUE);
	table_attach (table, eprop->item_label, 0, row, label_editor->formatting_group);
	table_attach (table, GTK_WIDGET (eprop), 1, row++, label_editor->formatting_group);
	label_editor->properties = g_list_prepend (label_editor->properties, eprop);

	/* width-chars ... */
	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "width-chars", FALSE, TRUE);
	hbox  = gtk_hbox_new (FALSE, 0);
	label_editor->width_radio = gtk_radio_button_new (NULL);
	gtk_box_pack_start (GTK_BOX (hbox), label_editor->width_radio, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), eprop->item_label, TRUE, TRUE, 2);
	table_attach (table, hbox, 0, row, label_editor->formatting_group);
	table_attach (table, GTK_WIDGET (eprop), 1, row++, label_editor->formatting_group);
	label_editor->properties = g_list_prepend (label_editor->properties, eprop);

	/* max-width-chars ... */
	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "max-width-chars", FALSE, TRUE);
	hbox  = gtk_hbox_new (FALSE, 0);
	label_editor->max_width_radio = gtk_radio_button_new_from_widget
		(GTK_RADIO_BUTTON (label_editor->width_radio));
	gtk_box_pack_start (GTK_BOX (hbox), label_editor->max_width_radio, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), eprop->item_label, TRUE, TRUE, 2);
	table_attach (table, hbox, 0, row, label_editor->formatting_group);
	table_attach (table, GTK_WIDGET (eprop), 1, row++, label_editor->formatting_group);
	label_editor->properties = g_list_prepend (label_editor->properties, eprop);
}

static void
append_label_wrapping (GladeLabelEditor   *label_editor,
		       GladeWidgetAdaptor *adaptor)
{
	GladeEditorProperty *eprop, *single_line_eprop;
	GtkWidget           *table, *frame, *alignment, *label, *hbox;
	gchar               *str;
	gint                 row = 0;

	/* Line Wrapping... */
	str = g_strdup_printf ("<b>%s</b>", _("Text line wrapping"));
	label = gtk_label_new (str);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	g_free (str);
	frame = gtk_frame_new (NULL);
	gtk_frame_set_label_widget (GTK_FRAME (frame), label);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (label_editor), frame, FALSE, FALSE, 0);

	alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
	gtk_container_add (GTK_CONTAINER (frame), alignment);

	table = gtk_table_new (0, 0, FALSE);
	gtk_container_add (GTK_CONTAINER (alignment), table);

	/* Append defaut epropless radio... */
	hbox = gtk_hbox_new (FALSE, 0);
	label_editor->wrap_free_radio = gtk_radio_button_new (NULL);
	label_editor->wrap_free_label = gtk_label_new (_("Text wraps normally"));
	gtk_misc_set_alignment (GTK_MISC (label_editor->wrap_free_label), 0.0F, 0.5F);
	gtk_box_pack_start (GTK_BOX (hbox), label_editor->wrap_free_radio, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), label_editor->wrap_free_label, TRUE, TRUE, 2);
	table_attach (table, hbox, 0, row++, label_editor->wrap_group);

	/* single-line-mode ... */
	single_line_eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "single-line-mode", FALSE, TRUE);
	hbox = gtk_hbox_new (FALSE, 0);
	label_editor->single_radio = gtk_radio_button_new_from_widget
		(GTK_RADIO_BUTTON (label_editor->wrap_free_radio));
	gtk_box_pack_start (GTK_BOX (hbox), label_editor->single_radio, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), single_line_eprop->item_label, TRUE, TRUE, 2);
	table_attach (table, hbox, 0, row++, label_editor->wrap_group);
	label_editor->properties = g_list_prepend (label_editor->properties, single_line_eprop);

	/* wrap-mode ... */
	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "wrap-mode", FALSE, TRUE);
	hbox  = gtk_hbox_new (FALSE, 0);
	label_editor->wrap_mode_radio = gtk_radio_button_new_from_widget
		(GTK_RADIO_BUTTON (label_editor->wrap_free_radio));
	gtk_box_pack_start (GTK_BOX (hbox), label_editor->wrap_mode_radio, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), eprop->item_label, TRUE, TRUE, 2);
	table_attach (table, hbox, 0, row, label_editor->wrap_group);
	table_attach (table, GTK_WIDGET (eprop), 1, row++, label_editor->wrap_group);
	label_editor->properties = g_list_prepend (label_editor->properties, eprop);

	/* Add invisible eprops at the end of this table... */
	gtk_widget_set_no_show_all (GTK_WIDGET (single_line_eprop), TRUE);
	gtk_widget_hide (GTK_WIDGET (single_line_eprop));
	table_attach (table, GTK_WIDGET (single_line_eprop), 0, row, label_editor->wrap_group);
}

GtkWidget *
glade_label_editor_new (GladeWidgetAdaptor *adaptor,
			GladeEditable      *embed)
{
	GladeLabelEditor    *label_editor;

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (GLADE_IS_EDITABLE (embed), NULL);

	label_editor = g_object_new (GLADE_TYPE_LABEL_EDITOR, NULL);
	label_editor->embed = GTK_WIDGET (embed);

	/* Pack the parent on top... */
	gtk_box_pack_start (GTK_BOX (label_editor), GTK_WIDGET (embed), FALSE, FALSE, 0);

	append_label_appearance (label_editor, adaptor);
	append_label_formatting (label_editor, adaptor);
	append_label_wrapping (label_editor, adaptor);

	/* Connect to our radio buttons.... */
	g_signal_connect (G_OBJECT (label_editor->attributes_radio), "toggled",
			  G_CALLBACK (attributes_toggled), label_editor);
	g_signal_connect (G_OBJECT (label_editor->markup_radio), "toggled",
			  G_CALLBACK (markup_toggled), label_editor);
	g_signal_connect (G_OBJECT (label_editor->pattern_radio), "toggled",
			  G_CALLBACK (pattern_toggled), label_editor);

	g_signal_connect (G_OBJECT (label_editor->width_radio), "toggled",
			  G_CALLBACK (width_toggled), label_editor);
	g_signal_connect (G_OBJECT (label_editor->max_width_radio), "toggled",
			  G_CALLBACK (max_width_toggled), label_editor);

	g_signal_connect (G_OBJECT (label_editor->wrap_free_radio), "toggled",
			  G_CALLBACK (wrap_free_toggled), label_editor);
	g_signal_connect (G_OBJECT (label_editor->single_radio), "toggled",
			  G_CALLBACK (single_toggled), label_editor);
	g_signal_connect (G_OBJECT (label_editor->wrap_mode_radio), "toggled",
			  G_CALLBACK (wrap_mode_toggled), label_editor);


	gtk_widget_show_all (GTK_WIDGET (label_editor));

	return GTK_WIDGET (label_editor);
}
