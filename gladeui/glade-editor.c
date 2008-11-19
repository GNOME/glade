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
 *   Tristan Van Berkom <tvb@gnome.org>
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 * SECTION:glade-editor
 * @Short_Description: A Widget to edit a #GladeWidget.
 *
 * This is the Glade Notebook containing all the controls needed to configure a #GladeWidget.
 */

#include <stdio.h>
#include <string.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-widget-adaptor.h"
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

enum
{
	PROP_0,
	PROP_SHOW_INFO,
	PROP_WIDGET
};

enum {
	GTK_DOC_SEARCH,
	LAST_SIGNAL
};

static GtkVBoxClass *parent_class = NULL;
static guint         glade_editor_signals[LAST_SIGNAL] = { 0 };

static void glade_editor_reset_dialog (GladeEditor *editor);


void
glade_editor_search_doc_search (GladeEditor *editor,
				const gchar *book,
				const gchar *page,
				const gchar *search)
{
	g_return_if_fail (GLADE_IS_EDITOR (editor));

	g_signal_emit (G_OBJECT (editor),
		       glade_editor_signals[GTK_DOC_SEARCH],
		       0, book, page, search);

}

static void
glade_editor_set_property (GObject      *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
	GladeEditor *editor = GLADE_EDITOR (object);

	switch (prop_id)
	{
	case PROP_SHOW_INFO:
		if (g_value_get_boolean (value))
			glade_editor_show_info (editor);
		else
			glade_editor_hide_info (editor);
		break;
	case PROP_WIDGET:
		glade_editor_load_widget (editor, (GladeWidget *)g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_editor_get_property (GObject    *object,
			   guint       prop_id,
			   GValue     *value,
			   GParamSpec *pspec)
{
	GladeEditor *editor = GLADE_EDITOR (object);

	switch (prop_id)
	{
	case PROP_SHOW_INFO:
		g_value_set_boolean (value, editor->show_info);
		break;
	case PROP_WIDGET:
		g_value_set_object (value, editor->loaded_widget);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}


static void
glade_editor_class_init (GladeEditorClass *klass)
{
	GObjectClass       *object_class;

	parent_class = g_type_class_peek_parent (klass);
	object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = glade_editor_set_property;
	object_class->get_property = glade_editor_get_property;

	klass->gtk_doc_search = NULL;

	/* Properties */
	g_object_class_install_property
		(object_class, PROP_SHOW_INFO,
		 g_param_spec_boolean ("show-info",
				       _("Show info"),
				       _("Whether to show an informational "
					 "button for the loaded widget"),
				       FALSE, G_PARAM_READABLE));

	g_object_class_install_property
		(object_class, PROP_WIDGET,
		 g_param_spec_object ("widget",
				      _("Widget"),
				      _("The currently loaded widget in this editor"),
				      GLADE_TYPE_WIDGET, G_PARAM_READWRITE));

	
	/**
	 * GladeEditor::gtk-doc-search:
	 * @gladeeditor: the #GladeEditor which received the signal.
	 * @arg1: the (#gchar *) book to search or %NULL
	 * @arg2: the (#gchar *) page to search or %NULL
	 * @arg3: the (#gchar *) search string or %NULL
	 *
	 * Emitted when the editor requests that a doc-search be performed.
	 */
	glade_editor_signals[GTK_DOC_SEARCH] =
		g_signal_new ("gtk-doc-search",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeEditorClass,
					       gtk_doc_search),
			      NULL, NULL,
			      glade_marshal_VOID__STRING_STRING_STRING,
			      G_TYPE_NONE, 3, 
			      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
}

static GtkWidget *
glade_editor_notebook_page (GladeEditor *editor, const gchar *name)
{
	GtkWidget     *alignment;
	GtkWidget     *sw;
	GtkWidget     *label_widget;
	GtkWidget     *image;
	static gchar  *path;
	static gint page = 0;

	/* alignment is needed to ensure property labels have some padding on the left */
	alignment = gtk_alignment_new (0.5, 0, 1, 0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 6, 0);

	/* construct tab label widget */
	if (strcmp (name, _("Accessibility")) == 0)
	{
		path = g_build_filename (glade_app_get_pixmaps_dir (), "atk.png", NULL);
		image = gtk_image_new_from_file (path);
		label_widget = gtk_event_box_new ();
		gtk_container_add (GTK_CONTAINER (label_widget), image);
		gtk_widget_show (label_widget);
		gtk_widget_show (image);

		gtk_widget_set_tooltip_text (label_widget, name);
	}
	else
	{
		label_widget = gtk_label_new_with_mnemonic (name);
	}
	
	/* configure page container */
	if (strcmp (name, _("_Signals")) == 0)
	{
		gtk_alignment_set (GTK_ALIGNMENT (alignment), 0.5, 0.5, 1, 1);
		gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 0, 0);

		gtk_container_set_border_width (GTK_CONTAINER (alignment), 6);
		gtk_notebook_insert_page (GTK_NOTEBOOK (editor->notebook), alignment, 
					  label_widget, page++);
	}
	else
	{
		/* wrap the alignment in a scrolled window */
		sw = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
						GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw),
						       GTK_WIDGET (alignment));
		gtk_container_set_border_width (GTK_CONTAINER (sw), 6);

		gtk_notebook_insert_page (GTK_NOTEBOOK (editor->notebook), sw, 
					  label_widget, page++);

	}

	return alignment;
}

static void
glade_editor_on_reset_click (GtkButton *button,
			     GladeEditor *editor)
{
	glade_editor_reset_dialog (editor);
}

static void
glade_editor_on_docs_click (GtkButton *button,
			    GladeEditor *editor)
{
	gchar *book;

	if (editor->loaded_widget)
	{
		g_object_get (editor->loaded_widget->adaptor, "book", &book, NULL);
		glade_editor_search_doc_search (editor, book,
						editor->loaded_widget->adaptor->name,
						NULL);
		g_free (book);
	}
}

static GtkWidget *
glade_editor_create_info_button (GladeEditor *editor)
{
	GtkWidget *button;
	GtkWidget *image;
	
	button = gtk_button_new ();

	image = glade_util_get_devhelp_icon (GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image);

	gtk_container_add (GTK_CONTAINER (button), image);

	gtk_widget_set_tooltip_text (button, _("View documentation for the selected widget"));
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_editor_on_docs_click), editor);

	return button;
}

static GtkWidget *
glade_editor_create_reset_button (GladeEditor *editor)
{
	GtkWidget *image;
	GtkWidget *button;

	button = gtk_button_new ();

	image  = gtk_image_new_from_stock ("gtk-clear", GTK_ICON_SIZE_BUTTON);

	gtk_container_add (GTK_CONTAINER (button), image);

	gtk_widget_set_tooltip_text (button, _("Reset widget properties to their defaults"));
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_editor_on_reset_click), editor);

	return button;
}


static void
glade_editor_update_class_warning_cb (GladeWidget  *widget,
				      GParamSpec   *pspec,
				      GladeEditor  *editor)
{
	if (widget->support_warning)
		gtk_widget_show (editor->warning);
	else
 		gtk_widget_hide (editor->warning);

	gtk_widget_set_tooltip_text (editor->warning, widget->support_warning);
}


static void
glade_editor_update_class_field (GladeEditor *editor)
{
	if (editor->loaded_widget)
	{
		GladeWidget *widget = editor->loaded_widget;
		gchar       *text;

		gtk_image_set_from_icon_name (GTK_IMAGE (editor->class_icon),
					      widget->adaptor->icon_name, 
					      GTK_ICON_SIZE_BUTTON);
		gtk_widget_show (editor->class_icon);

		/* translators: referring to the properties of a widget named '%s [%s]' */
		text = g_strdup_printf (_("%s Properties - %s [%s]"),
					widget->adaptor->title,
					widget->adaptor->name,
					widget->name);
		gtk_label_set_text (GTK_LABEL (editor->class_label), text);
		g_free (text);

		glade_editor_update_class_warning_cb (editor->loaded_widget, NULL, editor);
	}
	else
	{
		gtk_widget_hide (editor->warning);
		gtk_label_set_text (GTK_LABEL (editor->class_label), _("Properties"));
	}
}

static void
glade_editor_update_widget_name_cb (GladeWidget  *widget,
				    GParamSpec   *pspec,
				    GladeEditor  *editor)
{
	glade_editor_update_class_field (editor);
}

static GtkWidget *
glade_editor_setup_class_field (GladeEditor *editor)
{
	GtkWidget      *hbox;
	
	hbox = gtk_hbox_new (FALSE, 4);

	editor->class_icon   = gtk_image_new ();
	editor->class_label  = gtk_label_new (NULL);
	editor->warning      = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING, 
							 GTK_ICON_SIZE_MENU);

	gtk_widget_set_no_show_all (editor->warning, TRUE);
	gtk_widget_set_no_show_all (editor->class_icon, TRUE);

	gtk_misc_set_alignment (GTK_MISC (editor->class_label), 0.0, 0.5);
	gtk_label_set_ellipsize (GTK_LABEL (editor->class_label), 
				 PANGO_ELLIPSIZE_END);

	gtk_box_pack_start (GTK_BOX (hbox), editor->class_icon, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), editor->warning, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), editor->class_label, TRUE, TRUE, 0);

	glade_editor_update_class_field (editor);
	gtk_widget_show_all (hbox);

	return hbox;
}

static void
glade_editor_init (GladeEditor *editor)
{
	GtkSizeGroup *size_group;
	GtkWidget *hbox;

	editor->notebook     = gtk_notebook_new ();
	editor->page_widget  = glade_editor_notebook_page (editor, _("_General"));
	editor->page_packing = glade_editor_notebook_page (editor, _("_Packing"));
	editor->page_common  = glade_editor_notebook_page (editor, _("_Common"));
	editor->page_signals = glade_editor_notebook_page (editor, _("_Signals"));
	editor->page_atk     = glade_editor_notebook_page (editor, _("Accessibility"));
	editor->editables    = NULL;
	editor->loading      = FALSE;

	editor->class_field = glade_editor_setup_class_field (editor);

	gtk_container_set_border_width (GTK_CONTAINER (editor->notebook), 0);

	gtk_box_pack_start (GTK_BOX (editor), editor->class_field, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (editor), editor->notebook, TRUE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 0);
	gtk_box_pack_start (GTK_BOX (editor), hbox, FALSE, FALSE, 0);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

	/* Reset button */
	editor->reset_button = glade_editor_create_reset_button (editor);
	gtk_box_pack_end (GTK_BOX (hbox), editor->reset_button, FALSE, FALSE, 0);
	gtk_size_group_add_widget (size_group, editor->reset_button);

	/* Documentation button */
	editor->info_button = glade_editor_create_info_button (editor);
	gtk_box_pack_end (GTK_BOX (hbox), editor->info_button, FALSE, FALSE, 0);
	gtk_size_group_add_widget (size_group, editor->info_button);

	g_object_unref(size_group);

	gtk_widget_show_all (GTK_WIDGET (editor));
	if (editor->show_info)
		gtk_widget_show (editor->info_button);
	else
		gtk_widget_hide (editor->info_button);

	gtk_widget_hide (GTK_WIDGET (editor));
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

	editor = g_object_new (GLADE_TYPE_EDITOR, 
			       "spacing", 6,
			       NULL);
	
	return editor;
}

static GtkWidget *
glade_editor_get_editable_by_adaptor (GladeEditor *editor,
				      GladeWidgetAdaptor *adaptor,
				      GladeEditorPageType type)
{
	GtkWidget *editable;
	GList *list;

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

	for (list = editor->editables; list; list = list->next)
	{
		editable = list->data;
		if (type != GPOINTER_TO_INT (g_object_get_data (G_OBJECT (editable), "glade-editor-page-type")))
			continue;
		if (g_object_get_data (G_OBJECT (editable), "glade-widget-adaptor") == adaptor)
			return editable;
	}

	editable = (GtkWidget *)glade_widget_adaptor_create_editable (adaptor, type);
	g_return_val_if_fail (editable != NULL, NULL);

	g_object_set_data (G_OBJECT (editable), "glade-editor-page-type", GINT_TO_POINTER (type));
	g_object_set_data (G_OBJECT (editable), "glade-widget-adaptor", adaptor);

	if (type != GLADE_PAGE_PACKING)
	{
		editor->editables = g_list_prepend (editor->editables, editable);
		g_object_ref_sink (editable);
	}

	return editable;
}

static GtkWidget *
glade_editor_load_editable_in_page (GladeEditor          *editor, 
				    GladeWidgetAdaptor   *adaptor,
				    GladeEditorPageType   type)
{
	GtkContainer  *container = NULL;
	GtkWidget     *scrolled_window, *editable;
	GtkAdjustment *adj;

	/* Remove the old table that was in this container */
	switch (type)
	{
	case GLADE_PAGE_GENERAL:
		container = GTK_CONTAINER (editor->page_widget);
		break;
	case GLADE_PAGE_COMMON:
		container = GTK_CONTAINER (editor->page_common);
		break;
	case GLADE_PAGE_PACKING:
		container = GTK_CONTAINER (editor->page_packing);
		break;
	case GLADE_PAGE_ATK:
		container = GTK_CONTAINER (editor->page_atk);
		break;
	case GLADE_PAGE_QUERY:
	default:
		g_critical ("Unreachable code reached !");
		break;
	}
	
	/* Remove the editable (this will destroy on packing pages) */
	if (GTK_BIN (container)->child)
	{
		gtk_widget_hide (GTK_BIN (container)->child);
		gtk_container_remove (container, GTK_BIN (container)->child);
	}

	if (!adaptor)
		return NULL;

	if ((editable = glade_editor_get_editable_by_adaptor (editor, adaptor, type)) == NULL)
		return NULL;
	
	/* Attach the new page */
	gtk_container_add (GTK_CONTAINER (container), editable);
	gtk_widget_show (editable);

	/* Enable tabbed keynav in the editor */
	scrolled_window = gtk_widget_get_parent (GTK_WIDGET (container));
	scrolled_window = gtk_widget_get_parent (scrolled_window);
	
	/* FIXME: Save pointer to the scrolled window (or just the
	   adjustments) before hand. */
	g_assert (GTK_IS_SCROLLED_WINDOW (scrolled_window));
	
	adj = gtk_scrolled_window_get_vadjustment
		(GTK_SCROLLED_WINDOW (scrolled_window));
	gtk_container_set_focus_vadjustment
		(GTK_CONTAINER (editable), adj);
	
	adj = gtk_scrolled_window_get_hadjustment
		(GTK_SCROLLED_WINDOW (scrolled_window));
	gtk_container_set_focus_hadjustment
		(GTK_CONTAINER (editable), adj);

	return editable;
}

static void
glade_editor_load_signal_page (GladeEditor *editor)
{
	if (editor->signal_editor == NULL) {
		editor->signal_editor = glade_signal_editor_new ((gpointer) editor);
		gtk_container_add (GTK_CONTAINER (editor->page_signals),
				    glade_signal_editor_get_widget (editor->signal_editor));
	}
}

static void
glade_editor_load_widget_class (GladeEditor *editor, GladeWidgetAdaptor *adaptor)
{

	glade_editor_load_editable_in_page (editor, adaptor, GLADE_PAGE_GENERAL);
	glade_editor_load_editable_in_page (editor, adaptor, GLADE_PAGE_COMMON);
	glade_editor_load_editable_in_page (editor, adaptor, GLADE_PAGE_ATK);

	glade_editor_load_signal_page  (editor);
	
	editor->loaded_adaptor = adaptor;
}

static void
glade_editor_close_cb (GladeProject *project,
		       GladeEditor  *editor)
{
	/* project we are viewing was closed,
	 * detatch from editor.
	 */
	glade_editor_load_widget (editor, NULL);
}

static void
glade_editor_removed_cb (GladeProject *project,
			 GladeWidget  *widget,
			 GladeEditor  *editor)
{
	/* Widget we were viewing was removed from project,
	 * detatch from editor.
	 */
	if (widget == editor->loaded_widget)
		glade_editor_load_widget (editor, NULL);

}


static void
glade_editor_load_editable (GladeEditor         *editor, 
			    GladeWidget         *widget,
			    GladeEditorPageType  type)
{
	GtkWidget *editable;

	/* Use the parenting adaptor for packing pages... so deffer creating the widgets
	 * until load time.
	 */
	if (type == GLADE_PAGE_PACKING && widget->parent)
	{
		GladeWidgetAdaptor *adaptor = widget->parent->adaptor;
		editable = glade_editor_load_editable_in_page (editor, adaptor, GLADE_PAGE_PACKING);
	}
	else
		editable = glade_editor_get_editable_by_adaptor
			(editor, widget->adaptor, type);
	
	g_assert (editable);

	if (!widget) gtk_widget_hide (editable);

	glade_editable_load (GLADE_EDITABLE (editable), widget);

	if (widget) gtk_widget_show (editable);
}

static void
clear_editables (GladeEditor *editor)
{
	GladeEditable *editable;
	GList *l;

	for (l = editor->editables; l; l = l->next)
	{
		editable = l->data;
		glade_editable_load (editable, NULL);
	}
}

static void
glade_editor_load_widget_real (GladeEditor *editor, GladeWidget *widget)
{
	GladeWidgetAdaptor *adaptor;
	GladeProject       *project;
	gchar              *book;

	/* Disconnect from last widget */
	if (editor->loaded_widget != NULL)
	{
		/* better pay a small price now and avoid unseen editables
		 * waking up on project metadata changes.
		 */
		clear_editables (editor);

		project = glade_widget_get_project (editor->loaded_widget);
		g_signal_handler_disconnect (G_OBJECT (project),
					     editor->project_closed_signal_id);
		g_signal_handler_disconnect (G_OBJECT (project),
					     editor->project_removed_signal_id);
		g_signal_handler_disconnect (G_OBJECT (editor->loaded_widget),
					     editor->widget_warning_id);
		g_signal_handler_disconnect (G_OBJECT (editor->loaded_widget),
					     editor->widget_name_id);
	}	

	/* Load the GladeWidgetClass */
	adaptor = widget ? widget->adaptor : NULL;
	if (editor->loaded_adaptor != adaptor || adaptor == NULL)
		glade_editor_load_widget_class (editor, adaptor);

	glade_signal_editor_load_widget (editor->signal_editor, widget);

	/* we are just clearing, we are done */
	if (widget == NULL)
	{
		gtk_widget_set_sensitive (editor->reset_button, FALSE);
		gtk_widget_set_sensitive (editor->info_button, FALSE);

		editor->loaded_widget = NULL;

		g_object_notify (G_OBJECT (editor), "widget");
		return;
	}
	gtk_widget_set_sensitive (editor->reset_button, TRUE);

	g_object_get (editor->loaded_adaptor, "book", &book, NULL);
	gtk_widget_set_sensitive (editor->info_button, book != NULL);
	g_free (book);

	editor->loading = TRUE;

	/* Load each GladeEditorProperty from 'widget' */
	glade_editor_load_editable (editor, widget, GLADE_PAGE_GENERAL);
	glade_editor_load_editable (editor, widget, GLADE_PAGE_COMMON);
	glade_editor_load_editable (editor, widget, GLADE_PAGE_ATK);
	glade_editor_load_editable (editor, widget, GLADE_PAGE_PACKING);

	editor->loaded_widget = widget;
	editor->loading = FALSE;

	/* Update class header */
	glade_editor_update_class_field (editor);

	/* Connect to new widget */
	project = glade_widget_get_project (editor->loaded_widget);
	editor->project_closed_signal_id =
		g_signal_connect (G_OBJECT (project), "close",
				  G_CALLBACK (glade_editor_close_cb), editor);
	editor->project_removed_signal_id =
		g_signal_connect (G_OBJECT (project), "remove-widget",
				  G_CALLBACK (glade_editor_removed_cb), editor);
	editor->widget_warning_id =
		g_signal_connect (G_OBJECT (widget), "notify::support-warning",
				  G_CALLBACK (glade_editor_update_class_warning_cb),
				  editor);
	editor->widget_name_id =
		g_signal_connect (G_OBJECT (widget), "notify::name",
				  G_CALLBACK (glade_editor_update_widget_name_cb),
				  editor);

	gtk_container_check_resize (GTK_CONTAINER (editor));

	g_object_notify (G_OBJECT (editor), "widget");
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

static void
query_dialog_style_set_cb (GtkWidget *dialog,
			   GtkStyle  *previous_style,
			   gpointer   user_data)
{
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), 12);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 12);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area), 0);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->action_area), 6);
}


gboolean
glade_editor_query_dialog (GladeEditor *editor, GladeWidget *widget)
{
	GtkWidget           *dialog, *editable;
	gchar               *title;
	gint		     answer;
	gboolean	     retval = TRUE;

	title = g_strdup_printf (_("Create a %s"), widget->adaptor->name);

	dialog = gtk_dialog_new_with_buttons (title, NULL,
		 			      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT |
					      GTK_DIALOG_NO_SEPARATOR,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);

	g_free (title);

	gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
						 GTK_RESPONSE_OK,
						 GTK_RESPONSE_CANCEL,
						 -1);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	editable = glade_editor_get_editable_by_adaptor (editor,
							 widget->adaptor,
							 GLADE_PAGE_QUERY);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    editable, FALSE, FALSE, 6);

	glade_editable_load (GLADE_EDITABLE (editable), widget);

	g_signal_connect (dialog, "style-set", 
			  G_CALLBACK (query_dialog_style_set_cb),
			  NULL);

	answer = gtk_dialog_run (GTK_DIALOG (dialog));

	/*
	 * If user cancel's we cancel the whole "create operation" by
	 * return FALSE. glade_widget_new() will see the FALSE, and
	 * take care of canceling the "create" operation.
	 */
	if (answer == GTK_RESPONSE_CANCEL)
		retval = FALSE;

	gtk_container_remove (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), editable);
	
	gtk_widget_destroy (dialog);
	return retval;
}

enum {
	COLUMN_ENABLED = 0,
	COLUMN_PROP_NAME,
	COLUMN_PROPERTY,
	COLUMN_WEIGHT,
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
		 G_TYPE_INT,           /* Parent node ?       */
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
		      NULL);

	gtk_tree_view_insert_column_with_attributes
		(GTK_TREE_VIEW (view_widget), COLUMN_PROP_NAME,
		 _("Property"), renderer, 
		 "text",       COLUMN_PROP_NAME, 
		 "weight",     COLUMN_WEIGHT,
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
	GtkTreeIter    property_iter, general_iter, common_iter, atk_iter, *iter;
	GList         *list;
	GladeProperty *property;
	gboolean       def;
	
	g_return_if_fail (editor->loaded_widget != NULL);

	gtk_tree_store_append (model, &general_iter, NULL);
	gtk_tree_store_set    (model, &general_iter,
			       COLUMN_PROP_NAME, _("General"),
			       COLUMN_PROPERTY,  NULL,
			       COLUMN_WEIGHT,    PANGO_WEIGHT_BOLD,
			       COLUMN_CHILD,     FALSE,
			       COLUMN_DEFAULT,   FALSE,
			       COLUMN_NDEFAULT,  FALSE,
			       -1);

	gtk_tree_store_append (model, &common_iter,  NULL);
	gtk_tree_store_set    (model, &common_iter,
			       COLUMN_PROP_NAME, _("Common"),
			       COLUMN_PROPERTY,  NULL,
			       COLUMN_WEIGHT,    PANGO_WEIGHT_BOLD,
			       COLUMN_CHILD,     FALSE,
			       COLUMN_DEFAULT,   FALSE,
			       COLUMN_NDEFAULT,  FALSE,
			       -1);

	gtk_tree_store_append (model, &atk_iter,  NULL);
	gtk_tree_store_set    (model, &atk_iter,
			       COLUMN_PROP_NAME, _("Accessibility"),
			       COLUMN_PROPERTY,  NULL,
			       COLUMN_WEIGHT,    PANGO_WEIGHT_BOLD,
			       COLUMN_CHILD,     FALSE,
			       COLUMN_DEFAULT,   FALSE,
			       COLUMN_NDEFAULT,  FALSE,
			       -1);

	/* General & Common */
	for (list = editor->loaded_widget->properties; list; list = list->next)
	{
		property = list->data;

		if (glade_property_class_is_visible (property->klass) == FALSE)
			continue;
		
		if (property->klass->atk)
			iter = &atk_iter;
		else if (property->klass->common)
			iter = &common_iter;
		else
			iter = &general_iter;

	        def = glade_property_default (property);

		gtk_tree_store_append (model, &property_iter, iter);
		gtk_tree_store_set    (model, &property_iter,
				       COLUMN_ENABLED,   !def,
				       COLUMN_PROP_NAME, property->klass->name,
				       COLUMN_PROPERTY,  property,
				       COLUMN_WEIGHT,    PANGO_WEIGHT_NORMAL,
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
					  property ? property->klass->tooltip : message,
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

	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);

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
	gtk_container_set_border_width (GTK_CONTAINER (button), 6);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_editor_reset_select_all_clicked), tree_view);

	button = gtk_button_new_with_mnemonic (_("_Unselect All"));
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (button), 6);
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

void
glade_editor_show_info (GladeEditor *editor)
{
	g_return_if_fail (GLADE_IS_EDITOR (editor));

	if (editor->show_info != TRUE)
	{
		editor->show_info = TRUE;
		gtk_widget_show (editor->info_button);

		g_object_notify (G_OBJECT (editor), "show-info");
	}
}

void
glade_editor_hide_info (GladeEditor *editor)
{
	g_return_if_fail (GLADE_IS_EDITOR (editor));

	if (editor->show_info != FALSE)
	{
		editor->show_info = FALSE;
		gtk_widget_hide (editor->info_button);

		g_object_notify (G_OBJECT (editor), "show-info");
	}
}

/**
 * glade_editor_dialog_for_widget:
 * @widget: a #GladeWidget
 *
 * This convenience function creates a new dialog window to edit @widget
 * specifically.
 *
 * Returns: the newly created dialog window
 */ 
GtkWidget *
glade_editor_dialog_for_widget (GladeWidget *widget)
{
	GtkWidget *window, *editor;
	gchar *title, *prj_name;

	
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
	
	/* Window */
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DIALOG);

	prj_name = glade_project_get_name (widget->project);
	title = g_strdup_printf ("%s - %s Properties", prj_name, 
				 glade_widget_get_name (widget));
	gtk_window_set_title (GTK_WINDOW (window), title);
	g_free (title);
	g_free (prj_name);
	

	if (glade_app_get_accel_group ())
	{
		gtk_window_add_accel_group (GTK_WINDOW (window), 
					    glade_app_get_accel_group ());
		g_signal_connect (G_OBJECT (window), "key-press-event",
				  G_CALLBACK (glade_utils_hijack_key_press), NULL);
	}

	editor = g_object_new (GLADE_TYPE_EDITOR, 
			       "spacing", 6,
			       NULL);
	glade_editor_load_widget (GLADE_EDITOR (editor), widget);


	g_signal_connect_swapped (G_OBJECT (editor), "notify::widget",
				  G_CALLBACK (gtk_widget_destroy), window);


	gtk_container_set_border_width (GTK_CONTAINER (editor), 6);
	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (editor));
	
	gtk_window_set_default_size (GTK_WINDOW (window), 400, 480);

	gtk_widget_show_all (editor);
	return window;
}
