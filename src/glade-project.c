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

#include <string.h>

#include "glade.h"
#include "glade-project.h"
#include "glade-project-ui.h"
#include "glade-project-window.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-xml-utils.h"
#include "glade-widget.h"

static void glade_project_class_init (GladeProjectClass * klass);
static void glade_project_init (GladeProject *project);
static void glade_project_destroy (GtkObject *object);

enum
{
	ADD_WIDGET,
	REMOVE_WIDGET,
	WIDGET_NAME_CHANGED,
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static guint glade_project_signals[LAST_SIGNAL] = {0};
static GtkObjectClass *parent_class = NULL;

guint
glade_project_get_type (void)
{
	static guint glade_project_type = 0;
  
	if (!glade_project_type)
	{
		GtkTypeInfo glade_project_info =
		{
			"GladeProject",
			sizeof (GladeProject),
			sizeof (GladeProjectClass),
			(GtkClassInitFunc) glade_project_class_init,
			(GtkObjectInitFunc) glade_project_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		glade_project_type = gtk_type_unique (gtk_object_get_type (),
						      &glade_project_info);
	}
	
	return glade_project_type;
}

static void
glade_project_class_init (GladeProjectClass * klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) klass;

	parent_class = gtk_type_class (gtk_object_get_type ());

	glade_project_signals[ADD_WIDGET] =
		gtk_signal_new ("add_widget",
				GTK_RUN_LAST,
				GTK_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (GladeProjectClass, add_widget),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	glade_project_signals[REMOVE_WIDGET] =
		gtk_signal_new ("remove_widget",
				GTK_RUN_LAST,
				GTK_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (GladeProjectClass, remove_widget),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	glade_project_signals[WIDGET_NAME_CHANGED] =
		gtk_signal_new ("widget_name_changed",
				GTK_RUN_LAST,
				GTK_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (GladeProjectClass, widget_name_changed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	glade_project_signals[SELECTION_CHANGED] =
		gtk_signal_new ("selection_changed",
				GTK_RUN_LAST,
				GTK_CLASS_TYPE (object_class),
				GTK_SIGNAL_OFFSET (GladeProjectClass, selection_changed),
				gtk_marshal_VOID__VOID,
				GTK_TYPE_NONE, 0);
	
	klass->add_widget = NULL;
	klass->remove_widget = NULL;
	klass->widget_name_changed = NULL;
	klass->selection_changed = NULL;
	
	object_class->destroy = glade_project_destroy;
}


static void
glade_project_init (GladeProject * project)
{
	project->name = NULL;
	project->widgets = NULL;
	project->selection = NULL;
}

static void
glade_project_destroy (GtkObject *object)
{
	GladeProject *project;

	project = GLADE_PROJECT (object);

}

GladeProject *
glade_project_new (void)
{
	GladeProject *project;
	static gint i = 1;

	project = GLADE_PROJECT (gtk_type_new (glade_project_get_type ()));
	project->name = g_strdup_printf ("Untitled %i", i++);
	
	return project;
}

static void
glade_project_set_changed (GladeProject *project, gboolean changed)
{
	project->changed = changed;
}

void
glade_project_selection_changed (GladeProject *project)
{
	gtk_signal_emit (GTK_OBJECT (project),
			 glade_project_signals [SELECTION_CHANGED]);
}


void
glade_project_add_widget (GladeProject  *project,
			  GladeWidget *widget)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GTK_IS_OBJECT (project));

	project->widgets = g_list_prepend (project->widgets, widget);

	gtk_signal_emit (GTK_OBJECT (project),
			 glade_project_signals [ADD_WIDGET], widget);
	
	glade_project_set_changed (project, TRUE);
}

static void
glade_project_remove_widget_real (GladeProject *project,
				  GladeWidget *widget)
{
	GladeWidget *child;
	GList *list;

	list = widget->children;
	for (; list; list = list->next) {
		child = list->data;
		glade_project_remove_widget_real (project,
						  child);
	}
	
	project->selection = g_list_remove (project->selection, widget);
	project->widgets   = g_list_remove (project->widgets, widget);

	gtk_signal_emit (GTK_OBJECT (project),
			 glade_project_signals [REMOVE_WIDGET], widget);
}
			     
void
glade_project_remove_widget (GladeWidget *widget)
{
	GladeProject *project;

	g_return_if_fail (widget != NULL);

	project = widget->project;
	
	glade_project_remove_widget_real (project, widget);

	glade_project_selection_changed (project);
	glade_project_set_changed (project, TRUE);
}
	
void
glade_project_widget_name_changed (GladeProject *project,
				   GladeWidget *widget)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GTK_IS_OBJECT (project));

	gtk_signal_emit (GTK_OBJECT (project),
			 glade_project_signals [WIDGET_NAME_CHANGED], widget);
	
	glade_project_set_changed (project, TRUE);
}


GladeProject *
glade_project_get_active (void)
{
	GladeProjectWindow *gpw;
	
	gpw = glade_project_window_get ();

	if (gpw == NULL) {
		g_warning ("Could not get the active project\n");
		return NULL;
	}
		
	return gpw->project;
}

/**
 * glade_widget_get_by_name:
 * @project: The project in which to look for
 * @name: The user visible name of the widget we are looking for
 * 
 * Finds a GladeWidget inside a project given its name
 * 
 * Return Value: a pointer to the wiget, NULL if the widget does not exist
 **/
GladeWidget *
glade_project_get_widget_by_name (GladeProject *project, const gchar *name)
{
	GladeWidget *widget;
	GList *list;

	g_return_val_if_fail (name != NULL, NULL);
	
	list = project->widgets;
	
	for (; list != NULL; list = list->next) {
		widget = list->data;
		g_return_val_if_fail (widget->name != NULL, NULL);
		if (strcmp (widget->name, name) == 0)
			return widget;
	}

	return NULL;
}














void
glade_project_selection_clear (GladeProject *project, gboolean emit_signal)
{
	GladeWidget *widget;
	GList *list;
	
	g_return_if_fail (GLADE_IS_PROJECT (project));

	if (project->selection == NULL)
		return;

	list = project->selection;
	for (; list != NULL; list = list->next) {
		widget = list->data;
		glade_widget_flag_unselected (widget);
	}
	
	g_list_free (project->selection);
	project->selection = NULL;

	if (emit_signal)
		glade_project_selection_changed (project);
}
	
void
glade_project_selection_remove (GladeWidget *widget,
				gboolean emit_signal)
{
	GladeProject *project;
	
	g_return_if_fail (GLADE_IS_WIDGET  (widget));
	project = widget->project;
	g_return_if_fail (GLADE_IS_PROJECT (project));
	
	if (!widget->selected)
		return;

	glade_widget_flag_unselected (widget);
		
	project->selection = g_list_remove (project->selection, widget);

	if (emit_signal)
		glade_project_selection_changed (project);
}

void
glade_project_selection_add (GladeWidget *widget,
			     gboolean emit_signal)
{
	GladeProject *project;

	g_return_if_fail (GLADE_IS_WIDGET  (widget));
	project = widget->project;
	g_return_if_fail (GLADE_IS_PROJECT (project));

	if (widget->selected)
		return;
	
	project->selection = g_list_prepend (project->selection, widget);
	glade_widget_flag_selected (widget);

	if (emit_signal)
		glade_project_selection_changed (project);
}

void
glade_project_selection_set (GladeWidget *widget,
			     gboolean emit_signal)
{
	GladeProject *project;
	GList *list;
	g_return_if_fail (GLADE_IS_WIDGET  (widget));

	project = widget->project;
	g_return_if_fail (GLADE_IS_PROJECT (project));
		
	list = project->selection;
	/* Check if the selection is different than what we have */
	if ((list) && (list->next == NULL) && (list->data == widget))
	    return;
	    
	glade_project_selection_clear (project, FALSE);
	glade_project_selection_add   (widget, emit_signal);
}	

GList *
glade_project_selection_get (GladeProject *project)
{
	return project->selection;
}


/**
 * glade_project_write_widgets:
 * @context: 
 * @node: 
 * 
 * Give a project it appends to @node all the toplevel widgets. Each widget is responsible
 * for appending it's childrens
 * 
 * Return Value: FALSE on error, TRUE otherwise
 **/
static gboolean
glade_project_write_widgets (const GladeProject *project, GladeXmlContext *context, GladeXmlNode *node)
{
	GladeXmlNode *child;
	GladeWidget *widget;
	GList *list;

	list = project->widgets;
	for (; list != NULL; list = list->next) {
		widget = list->data;
		if (GLADE_WIDGET_IS_TOPLEVEL (widget)) {
			child = glade_widget_write (context, widget);
			if (child != NULL)
				glade_xml_append_child (node, child);
			else
				return FALSE;
		}
	}

	return TRUE;
}

/**
 * glade_project_write:
 * @project: 
 * 
 * Retrns the root node of a newly created xml representation of the project and its contents
 * 
 * Return Value: 
 **/
static GladeXmlNode *
glade_project_write (GladeXmlContext *context, const GladeProject *project)
{
	GladeXmlNode *node;

	node = glade_xml_node_new (context, GLADE_XML_TAG_PROJECT);
	if (node == NULL)
		return NULL;

	if (!glade_project_write_widgets (project, context, node))
		return NULL;

	return node;
}

/**
 * glade_project_save_to_file:
 * @project: 
 * @file_name: 
 * 
 * Save a proejct
 * 
 * Return Value: TRUE on success, FALSE otherwise
 **/
static gboolean
glade_project_save_to_file (GladeProject *project,
			    const gchar * full_path)
{
	GladeXmlContext *context;
	GladeXmlNode *root;
	GladeXmlDoc *xml_doc;
	gboolean ret;

	if (!glade_util_path_is_writable (full_path))
		return FALSE;

	xml_doc = glade_xml_doc_new ();
	if (xml_doc == NULL) {
		g_warning ("Could not create xml document\n");
		return FALSE;
	}
	context = glade_xml_context_new (xml_doc, NULL);
	root = glade_project_write (context, project);
	glade_xml_context_destroy (context);
	if (root == NULL)
		return FALSE;

	glade_xml_doc_set_root (xml_doc, root);
	ret = glade_xml_doc_save (xml_doc, full_path);
	glade_xml_doc_free (xml_doc);

	if (ret < 0)
		return FALSE;

	return TRUE;
}


/**
 * glade_project_save:
 * @project: 
 * 
 * Save the project, query the user for a proeject name if necessary
 * 
 * Return Value: TRUE if the project was saved, FALSE if the user cancelled
 *               the operation or an error was encountered while saving
 **/
gboolean
glade_project_save (GladeProject *project)
{
	g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

	if (project->path == NULL)
		project->path = glade_project_ui_save_get_name (project);

	if (!glade_project_save_to_file (project, project->path)) {
		glade_project_ui_warn (project, _("Invalid file name"));
		return FALSE;
	}

	return TRUE;
}
	
