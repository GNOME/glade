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

#include "glade-treeview-editor.h"


static void glade_tree_view_editor_finalize        (GObject              *object);

static void glade_tree_view_editor_editable_init   (GladeEditableIface *iface);

static void glade_tree_view_editor_realize         (GtkWidget            *widget);
static void glade_tree_view_editor_grab_focus      (GtkWidget            *widget);


G_DEFINE_TYPE_WITH_CODE (GladeTreeViewEditor, glade_tree_view_editor, GTK_TYPE_HBOX,
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_tree_view_editor_editable_init));


static void
glade_tree_view_editor_class_init (GladeTreeViewEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize     = glade_tree_view_editor_finalize;

	widget_class->realize      = glade_tree_view_editor_realize;
	widget_class->grab_focus   = glade_tree_view_editor_grab_focus;
}

static void
glade_tree_view_editor_init (GladeTreeViewEditor *self)
{
}

static void
project_changed (GladeProject      *project,
		 GladeCommand      *command,
		 gboolean           execute,
		 GladeTreeViewEditor *view_editor)
{
	if (!GTK_WIDGET_MAPPED (view_editor))
		return;

	/* Reload on all commands */
	glade_editable_load (GLADE_EDITABLE (view_editor), view_editor->loaded_widget);
}

static void
project_finalized (GladeTreeViewEditor *view_editor,
		   GladeProject       *where_project_was)
{
	view_editor->loaded_widget = NULL;

	glade_editable_load (GLADE_EDITABLE (view_editor), NULL);
}

static GladeWidget *
get_model_widget (GladeWidget *view)
{

	GtkTreeModel *model = NULL;

	if (GTK_IS_TREE_VIEW (view->object))
		model = gtk_tree_view_get_model (GTK_TREE_VIEW (view->object));
	else if (GTK_IS_ICON_VIEW (view->object))	
		model = gtk_icon_view_get_model (GTK_ICON_VIEW (view->object));
	else if (GTK_IS_COMBO_BOX (view->object))
		model = gtk_combo_box_get_model (GTK_COMBO_BOX (view->object));

	if (model)
		return glade_widget_get_from_gobject (model);
	
	return NULL;
}

static void
glade_tree_view_editor_load (GladeEditable *editable,
			       GladeWidget   *widget)
{
	GladeTreeViewEditor   *view_editor = GLADE_TREE_VIEW_EDITOR (editable);
	GladeWidget           *model_widget;

	/* Since we watch the project*/
	if (view_editor->loaded_widget)
	{
		g_signal_handlers_disconnect_by_func (G_OBJECT (view_editor->loaded_widget->project),
						      G_CALLBACK (project_changed), view_editor);

		/* The widget could die unexpectedly... */
		g_object_weak_unref (G_OBJECT (view_editor->loaded_widget->project),
				     (GWeakNotify)project_finalized,
				     view_editor);
	}

	/* Mark our widget... */
	view_editor->loaded_widget = widget;

	if (view_editor->loaded_widget)
	{
		/* This fires for undo/redo */
		g_signal_connect (G_OBJECT (view_editor->loaded_widget->project), "changed",
				  G_CALLBACK (project_changed), view_editor);

		/* The widget/project could die unexpectedly... */
		g_object_weak_ref (G_OBJECT (view_editor->loaded_widget->project),
				   (GWeakNotify)project_finalized,
				   view_editor);
	}

	/* load the embedded editable... */
	if (view_editor->embed)
		glade_editable_load (GLADE_EDITABLE (view_editor->embed), widget);

	if (view_editor->embed_list_store && view_editor->embed_tree_store) 
	{
		gtk_widget_hide (view_editor->no_model_message);
		gtk_widget_hide (view_editor->embed_list_store);
		gtk_widget_hide (view_editor->embed_tree_store);
		glade_editable_load (GLADE_EDITABLE (view_editor->embed_list_store), NULL);
		glade_editable_load (GLADE_EDITABLE (view_editor->embed_tree_store), NULL);

		/* Finalize safe code here... */
		if (widget && (model_widget = get_model_widget (widget)))
		{	
			if (GTK_IS_LIST_STORE (model_widget->object))
			{
				gtk_widget_show (view_editor->embed_list_store);
				glade_editable_load (GLADE_EDITABLE (view_editor->embed_list_store), model_widget);
			}
			else if (GTK_IS_TREE_STORE (model_widget->object))
			{
				gtk_widget_show (view_editor->embed_tree_store);
				glade_editable_load (GLADE_EDITABLE (view_editor->embed_tree_store), model_widget);
			}
			else
				gtk_widget_show (view_editor->no_model_message);
		}
		else
			gtk_widget_show (view_editor->no_model_message);
	}
}

static void
glade_tree_view_editor_set_show_name (GladeEditable *editable,
					gboolean       show_name)
{
	GladeTreeViewEditor *view_editor = GLADE_TREE_VIEW_EDITOR (editable);

	glade_editable_set_show_name (GLADE_EDITABLE (view_editor->embed), show_name);
}

static void
glade_tree_view_editor_editable_init (GladeEditableIface *iface)
{
	iface->load = glade_tree_view_editor_load;
	iface->set_show_name = glade_tree_view_editor_set_show_name;
}

static void
glade_tree_view_editor_finalize (GObject *object)
{
	GladeTreeViewEditor *view_editor = GLADE_TREE_VIEW_EDITOR (object);

	view_editor->embed_tree_store = NULL;
	view_editor->embed_list_store = NULL;
	view_editor->embed            = NULL;

	glade_editable_load (GLADE_EDITABLE (object), NULL);

	G_OBJECT_CLASS (glade_tree_view_editor_parent_class)->finalize (object);
}


static void
glade_tree_view_editor_realize (GtkWidget *widget)
{
	GladeTreeViewEditor *view_editor = GLADE_TREE_VIEW_EDITOR (widget);

	GTK_WIDGET_CLASS (glade_tree_view_editor_parent_class)->realize (widget);

	glade_editable_load (GLADE_EDITABLE (view_editor), view_editor->loaded_widget);
}

static void
glade_tree_view_editor_grab_focus (GtkWidget *widget)
{
	GladeTreeViewEditor *view_editor = GLADE_TREE_VIEW_EDITOR (widget);

	gtk_widget_grab_focus (view_editor->embed);
}

GtkWidget *
glade_tree_view_editor_new (GladeWidgetAdaptor *adaptor,
			     GladeEditable      *embed)
{
	GladeTreeViewEditor    *view_editor;
	GtkWidget              *vbox, *separator;
	gchar                  *str;

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (GLADE_IS_EDITABLE (embed), NULL);

	view_editor = g_object_new (GLADE_TYPE_TREE_VIEW_EDITOR, NULL);
	view_editor->embed = GTK_WIDGET (embed);

	/* Pack the parent on the left... */
	gtk_box_pack_start (GTK_BOX (view_editor), GTK_WIDGET (embed), TRUE, TRUE, 8);

	separator = gtk_vseparator_new ();
	gtk_box_pack_start (GTK_BOX (view_editor), separator, FALSE, FALSE, 0);

	/* ...and the vbox with datastore/label on the right */
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (view_editor), vbox, TRUE, TRUE, 8);

	str = g_strdup_printf ("<b>%s</b>", _("Choose a Data Model and define some\n"
					      "columns in the data store first"));
	view_editor->no_model_message = gtk_label_new (str);
	gtk_label_set_use_markup (GTK_LABEL (view_editor->no_model_message), TRUE);
	gtk_label_set_justify (GTK_LABEL (view_editor->no_model_message), GTK_JUSTIFY_CENTER);

	g_free (str);

 	gtk_box_pack_start (GTK_BOX (vbox), view_editor->no_model_message, TRUE, TRUE, 0);

	view_editor->embed_list_store = (GtkWidget *)glade_widget_adaptor_create_editable 
		(glade_widget_adaptor_get_by_type (GTK_TYPE_LIST_STORE), GLADE_PAGE_GENERAL);
	glade_editable_set_show_name (GLADE_EDITABLE (view_editor->embed_list_store), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), view_editor->embed_list_store, TRUE, TRUE, 0);

	view_editor->embed_tree_store = (GtkWidget *)glade_widget_adaptor_create_editable 
		(glade_widget_adaptor_get_by_type (GTK_TYPE_TREE_STORE), GLADE_PAGE_GENERAL);
	glade_editable_set_show_name (GLADE_EDITABLE (view_editor->embed_tree_store), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), view_editor->embed_tree_store, TRUE, TRUE, 0);

	gtk_widget_show_all (GTK_WIDGET (view_editor));

	return GTK_WIDGET (view_editor);
}
