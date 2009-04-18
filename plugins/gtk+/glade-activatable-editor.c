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

#include "glade-activatable-editor.h"


static void glade_activatable_editor_finalize        (GObject              *object);

static void glade_activatable_editor_editable_init   (GladeEditableIface *iface);

static void glade_activatable_editor_grab_focus      (GtkWidget            *widget);


G_DEFINE_TYPE_WITH_CODE (GladeActivatableEditor, glade_activatable_editor, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_activatable_editor_editable_init));


static void
glade_activatable_editor_class_init (GladeActivatableEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize     = glade_activatable_editor_finalize;
	widget_class->grab_focus   = glade_activatable_editor_grab_focus;
}

static void
glade_activatable_editor_init (GladeActivatableEditor *self)
{
}

static void
project_changed (GladeProject      *project,
		 GladeCommand      *command,
		 gboolean           execute,
		 GladeActivatableEditor *activatable_editor)
{
	if (activatable_editor->modifying ||
	    !GTK_WIDGET_MAPPED (activatable_editor))
		return;

	/* Reload on all commands */
	glade_editable_load (GLADE_EDITABLE (activatable_editor), activatable_editor->loaded_widget);
}


static void
project_finalized (GladeActivatableEditor *activatable_editor,
		   GladeProject       *where_project_was)
{
	activatable_editor->loaded_widget = NULL;

	glade_editable_load (GLADE_EDITABLE (activatable_editor), NULL);
}

static void
glade_activatable_editor_load (GladeEditable *editable,
			  GladeWidget   *widget)
{
	GladeActivatableEditor    *activatable_editor = GLADE_ACTIVATABLE_EDITOR (editable);
	GList               *l;

	activatable_editor->loading = TRUE;

	/* Since we watch the project*/
	if (activatable_editor->loaded_widget)
	{
		/* watch custom-child and use-stock properties here for reloads !!! */

		g_signal_handlers_disconnect_by_func (G_OBJECT (activatable_editor->loaded_widget->project),
						      G_CALLBACK (project_changed), activatable_editor);

		/* The widget could die unexpectedly... */
		g_object_weak_unref (G_OBJECT (activatable_editor->loaded_widget->project),
				     (GWeakNotify)project_finalized,
				     activatable_editor);
	}

	/* Mark our widget... */
	activatable_editor->loaded_widget = widget;

	if (activatable_editor->loaded_widget)
	{
		/* This fires for undo/redo */
		g_signal_connect (G_OBJECT (activatable_editor->loaded_widget->project), "changed",
				  G_CALLBACK (project_changed), activatable_editor);

		/* The widget/project could die unexpectedly... */
		g_object_weak_ref (G_OBJECT (activatable_editor->loaded_widget->project),
				   (GWeakNotify)project_finalized,
				   activatable_editor);
	}

	/* load the embedded editable... */
	if (activatable_editor->embed)
		glade_editable_load (GLADE_EDITABLE (activatable_editor->embed), widget);

	for (l = activatable_editor->properties; l; l = l->next)
		glade_editor_property_load_by_widget (GLADE_EDITOR_PROPERTY (l->data), widget);

	if (widget)
	{
	}
	activatable_editor->loading = FALSE;
}

static void
glade_activatable_editor_set_show_name (GladeEditable *editable,
				   gboolean       show_name)
{
	GladeActivatableEditor *activatable_editor = GLADE_ACTIVATABLE_EDITOR (editable);

	glade_editable_set_show_name (GLADE_EDITABLE (activatable_editor->embed), show_name);
}

static void
glade_activatable_editor_editable_init (GladeEditableIface *iface)
{
	iface->load = glade_activatable_editor_load;
	iface->set_show_name = glade_activatable_editor_set_show_name;
}

static void
glade_activatable_editor_finalize (GObject *object)
{
	GladeActivatableEditor *activatable_editor = GLADE_ACTIVATABLE_EDITOR (object);

	if (activatable_editor->properties)
		g_list_free (activatable_editor->properties);
	activatable_editor->properties = NULL;
	activatable_editor->embed      = NULL;

	glade_editable_load (GLADE_EDITABLE (object), NULL);

	G_OBJECT_CLASS (glade_activatable_editor_parent_class)->finalize (object);
}

static void
glade_activatable_editor_grab_focus (GtkWidget *widget)
{
	GladeActivatableEditor *activatable_editor = GLADE_ACTIVATABLE_EDITOR (widget);

	gtk_widget_grab_focus (activatable_editor->embed);
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
reset_property (GladeWidget *gwidget,
		const gchar *property_name)
{
	GladeProperty *property;
	GValue         value = { 0, };

	if ((property = glade_widget_get_property (gwidget, property_name)) != NULL)
	{
		glade_property_get_default (property, &value);
		glade_command_set_property_value (property, &value);
		g_value_unset (&value);
	}
}

static GladeWidget *
get_image_widget (GladeWidget *widget)
{
	GtkWidget *image = NULL;

	if (GTK_IS_IMAGE_MENU_ITEM (widget->object))
		image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (widget->object));
	return image ? glade_widget_get_from_gobject (image) : NULL;
}

static void
reset_properties (GladeWidget *gwidget,
		  GtkAction   *action,
		  gboolean     use_appearance,
		  gboolean     use_appearance_changed)
{
	reset_property (gwidget, "visible");
	reset_property (gwidget, "sensitive");

	if (GTK_IS_MENU_ITEM (gwidget->object))
	{
		if (!use_appearance_changed)
			reset_property (gwidget, "accel-group");

		if (use_appearance)
		{
			GladeWidget *image;
			GladeProperty *property;

			/* Delete image... */
			if ((image = get_image_widget (gwidget)) != NULL)
			{
				GList list = { 0, };
				list.data = image;
				glade_command_unlock_widget (image);
				glade_command_delete (&list);
			}

			property = glade_widget_get_property (gwidget, "label");
			glade_command_set_property (property, NULL);

			reset_property (gwidget, "stock");
			reset_property (gwidget, "use-underline");
			reset_property (gwidget, "use-stock");

		}
		else if (use_appearance_changed)
		{
			reset_property (gwidget, "stock");
			reset_property (gwidget, "use-underline");
			reset_property (gwidget, "use-stock");

			reset_property (gwidget, "label");

		}
	}
	else if (GTK_IS_TOOL_ITEM (gwidget->object))
	{
		reset_property (gwidget, "visible-horizontal");
		reset_property (gwidget, "visible-vertical");
		reset_property (gwidget, "is-important");

		if (use_appearance || use_appearance_changed)
		{
			reset_property (gwidget, "label-widget");
			reset_property (gwidget, "custom-label");
			reset_property (gwidget, "stock-id");
			reset_property (gwidget, "icon-name");
			reset_property (gwidget, "icon");
			reset_property (gwidget, "icon-widget");
			reset_property (gwidget, "image-mode");
		}
	}
	else if (GTK_IS_BUTTON (gwidget->object))
	{
		reset_property (gwidget, "active");


		if (use_appearance)
		{

			GtkWidget *button, *child;
			GladeWidget *gchild = NULL;
			GladeProperty *property;
			
			/* If theres a widget customly inside... command remove it first... */
			button = GTK_WIDGET (gwidget->object);
			child  = GTK_BIN (button)->child;
			if (child)
				gchild = glade_widget_get_from_gobject (child);
			
			if (gchild && gchild->parent == gwidget)
			{
				GList widgets = { 0, };
				widgets.data = gchild;
				glade_command_delete (&widgets);
			}
			
			reset_property (gwidget, "custom-child");
			reset_property (gwidget, "stock");
			//reset_property (gwidget, "use-stock");

			property = glade_widget_get_property (gwidget, "label");
			glade_command_set_property (property, "");

		} else if (use_appearance_changed) {
			reset_property (gwidget, "label");
			reset_property (gwidget, "custom-child");
			reset_property (gwidget, "stock");
			//reset_property (gwidget, "use-stock");
		}
	}
	/* Make sure none of our property resets screw with the current selection,
	 * since we rely on the selection during commit time.
	 */
	glade_project_selection_set (gwidget->project, gwidget->object, TRUE);
}

static void
related_action_pre_commit (GladeEditorProperty     *property,
			   GValue                  *value,
			   GladeActivatableEditor  *activatable_editor)
{
	GladeWidget *gwidget = activatable_editor->loaded_widget;
	GtkAction   *action = g_value_get_object (value);
	gboolean     use_appearance = FALSE;

	glade_widget_property_get (gwidget, "use-action-appearance", &use_appearance);

	glade_command_push_group (_("Setting %s action"), gwidget->name);

	reset_properties (gwidget, action, use_appearance, FALSE);

}

static void
related_action_post_commit (GladeEditorProperty     *property,
			    GValue                  *value,
			    GladeActivatableEditor  *activatable_editor)
{

	glade_command_pop_group ();
}


static void
use_appearance_pre_commit (GladeEditorProperty     *property,
			   GValue                  *value,
			   GladeActivatableEditor  *activatable_editor)
{
	GladeWidget *gwidget = activatable_editor->loaded_widget;
	GtkAction   *action = NULL;
	gboolean     use_appearance = g_value_get_boolean (value);

	glade_widget_property_get (gwidget, "related-action", &action);

	glade_command_push_group (use_appearance ? 
				  _("Setting %s to use action appearance") :
				  _("Setting %s to not use action appearance"),
				  gwidget->name);

	reset_properties (gwidget, action, use_appearance, TRUE);
}

static void
use_appearance_post_commit (GladeEditorProperty     *property,
			    GValue                  *value,
			    GladeActivatableEditor  *activatable_editor)
{

	glade_command_pop_group ();
}

GtkWidget *
glade_activatable_editor_new (GladeWidgetAdaptor *adaptor,
			      GladeEditable      *embed)
{
	GladeActivatableEditor    *activatable_editor;
	GladeEditorProperty *eprop;
	GtkWidget           *table, *frame, *alignment, *label, *hbox;
	GtkSizeGroup        *group;
	gchar               *str;
	gint                 row = 0;

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (GLADE_IS_EDITABLE (embed), NULL);

	activatable_editor = g_object_new (GLADE_TYPE_ACTIVATABLE_EDITOR, NULL);
	activatable_editor->embed = GTK_WIDGET (embed);

	/* Pack the parent on top... */
	gtk_box_pack_start (GTK_BOX (activatable_editor), GTK_WIDGET (embed), FALSE, FALSE, 0);

	str = g_strdup_printf ("<b>%s</b>", _("Action"));
	label = gtk_label_new (str);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	g_free (str);
	frame = gtk_frame_new (NULL);
	gtk_frame_set_label_widget (GTK_FRAME (frame), label);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (activatable_editor), frame, FALSE, FALSE, 4);

	alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 12, 0);
	gtk_container_add (GTK_CONTAINER (frame), alignment);

	table = gtk_table_new (0, 0, FALSE);
	gtk_container_add (GTK_CONTAINER (alignment), table);

	group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "related-action", FALSE, TRUE);
	table_attach (table, eprop->item_label, 0, row, group);
	table_attach (table, GTK_WIDGET (eprop), 1, row++, group);
	activatable_editor->properties = g_list_prepend (activatable_editor->properties, eprop);

	g_signal_connect (G_OBJECT (eprop), "commit",
			  G_CALLBACK (related_action_pre_commit), activatable_editor);
	g_signal_connect_after (G_OBJECT (eprop), "commit",
				G_CALLBACK (related_action_post_commit), activatable_editor);

	eprop = glade_widget_adaptor_create_eprop_by_name (adaptor, "use-action-appearance", FALSE, TRUE);
	table_attach (table, eprop->item_label, 0, row, group);
	table_attach (table, GTK_WIDGET (eprop), 1, row++, group);
	activatable_editor->properties = g_list_prepend (activatable_editor->properties, eprop);

	gtk_widget_show_all (GTK_WIDGET (activatable_editor));

	g_signal_connect (G_OBJECT (eprop), "commit",
			  G_CALLBACK (use_appearance_pre_commit), activatable_editor);
	g_signal_connect_after (G_OBJECT (eprop), "commit",
			  G_CALLBACK (use_appearance_post_commit), activatable_editor);

	return GTK_WIDGET (activatable_editor);
}
