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
	project->undo_stack = NULL;
	project->prev_redo_item = NULL;

	/* Setup the /Project menu item */
	project->entry.accelerator = NULL;
	project->entry.callback = glade_project_window_set_project_cb;
	project->entry.callback_action = 0;
	project->entry.item_type = g_strdup ("<Item>");
}

static void
glade_project_destroy (GtkObject *object)
{
	GladeProject *project;

	project = GLADE_PROJECT (object);

}

static GladeProject *
glade_project_check_previously_loaded (const gchar *path)
{
	GladeProjectWindow *gpw;
	GladeProject *project;
	GList *list;

	gpw = glade_project_window_get ();
	list = gpw->projects;

	for (; list; list = list->next) {
		project = GLADE_PROJECT (list->data);

		if (project->path != NULL && !strcmp (project->path, path))
			return project;
	}

	return NULL;
}

static void
glade_project_update_menu_path (GladeProject *project)
{
	g_free (project->entry.path);

	project->entry.path = g_strdup_printf ("/Project/%s", project->name);
}

static void
glade_project_refresh_menu_item (GladeProject *project)
{
	GladeProjectWindow *gpw;
	GtkWidget *label;

	gpw = glade_project_window_get ();
	label = gtk_bin_get_child (GTK_BIN (gtk_item_factory_get_item (gpw->item_factory, project->entry.path)));

	/* Change the menu item's label */
	gtk_label_set_text (GTK_LABEL (label), project->name);

	/* Update the path entry, for future changes. */
	glade_project_update_menu_path (project);
}

GladeProject *
glade_project_new (gboolean untitled)
{
	GladeProject *project;
	static gint i = 1;

	project = GLADE_PROJECT (gtk_type_new (glade_project_get_type ()));

	if (untitled)
		project->name = g_strdup_printf ("Untitled %i", i++);

	glade_project_update_menu_path (project);

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

static void
glade_project_add_widget_real (GladeProject *project,
			       GladeWidget *widget)
{
	GladeWidget *child;
	GList *list;

	widget->project = project;

	/*
	 * Add all the children as well.
	 */
	list = widget->children;
	for (; list; list = list->next) {
		child = list->data;
		glade_project_add_widget_real (project, child);
		child->project = project;
	}
	
	project->widgets = g_list_prepend (project->widgets, widget);

	gtk_signal_emit (GTK_OBJECT (project),
			 glade_project_signals [ADD_WIDGET], widget);
}

void
glade_project_add_widget (GladeProject *project,
			  GladeWidget  *widget)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GTK_IS_OBJECT (project));

	glade_project_add_widget_real (project, widget);
	glade_project_set_changed (project, TRUE);
}

/**
 * Please be careful, this function removes the widget from the project,
 * but it doesn't remove the project reference from the widget.  I.e.
 * widget->project (and it's children ->project) is not set to NULL.
 *
 * We don't want to set it to NULL to the UNDO to work.
 */
static void
glade_project_remove_widget_real (GladeProject *project,
				  GladeWidget *widget)
{
	GladeWidget *child;
	GList *list;

	list = widget->children;
	for (; list; list = list->next) {
		child = list->data;
		glade_project_remove_widget_real (project, child);
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
				glade_xml_node_append_child (node, child);
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
 * Returns the root node of a newly created xml representation of the project and its contents
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
 * Save a project
 * 
 * Return Value: TRUE on success, FALSE otherwise
 **/
static gboolean
glade_project_save_to_file (GladeProject *project,
			    const gchar *full_path)
{
	GladeXmlContext *context;
	GladeXmlNode *root;
	GladeXmlDoc *xml_doc;
	gboolean ret;

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

static GladeProject *
glade_project_new_from_node (GladeXmlNode *node)
{
	GladeProject *project;
	GladeXmlNode *child;
	GladeWidget *widget;
	
	if (!glade_xml_node_verify  (node, GLADE_XML_TAG_PROJECT))
		return NULL;

	project = glade_project_new (FALSE);
	project->changed = FALSE;
	project->selection = NULL;
	project->widgets = NULL;
	
	child = glade_xml_node_get_children (node);
	for (; child != NULL; child = glade_xml_node_next (child)) {
		if (!glade_xml_node_verify (child, GLADE_XML_TAG_WIDGET))
			return NULL;
		widget = glade_widget_new_from_node (child, project);
		if (widget == NULL)
			return NULL;
	}
	project->widgets = g_list_reverse (project->widgets);
	
	return project;	
}

static GladeProject *
glade_project_open_from_file (const gchar *path)
{
	GladeXmlContext *context;
	GladeXmlDoc *doc;
	GladeProject *project;

	context = glade_xml_context_new_from_path (path, NULL, GLADE_XML_TAG_PROJECT);
	if (context == NULL)
		return NULL;
	doc = glade_xml_context_get_doc (context);
	project = glade_project_new_from_node (glade_xml_doc_get_root (doc));
	glade_xml_context_free (context);

	if (project) {
		project->path = g_strdup_printf ("%s", path);
		g_free (project->name);
		project->name = g_path_get_basename (project->path);
		/* Setup the menu item to be shown in the /Project menu. */
		glade_project_update_menu_path (project);
	}

	return project;
}


/**
 * glade_project_save:
 * @project: 
 * 
 * Save the project, query the user for a project name if necessary
 * 
 * Return Value: TRUE if the project was saved, FALSE if the user cancelled
 *               the operation or an error was encountered while saving
 **/
gboolean
glade_project_save (GladeProject *project)
{
	gchar *backup;

	g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

	backup = project->path;

	if (project->path == NULL) {
		g_free (project->name);
		project->path = glade_project_ui_get_path (_("Save ..."));

		/* If the user hit cancel, restore its previous name and return */
		if (project->path == NULL) {
			project->path = backup;
			return FALSE;
		}

		project->name = g_path_get_basename (project->path);
		g_free (backup);
	}

	if (!glade_project_save_to_file (project, project->path)) {
		glade_util_ui_warn (_("Invalid file name"));
		g_free (project->path);
		project->path = backup;
		return FALSE;
	}

	glade_project_refresh_menu_item (project);

	return TRUE;
}


/**
 * glade_project_save_as:
 * @project: 
 * 
 * Query the user for a new file name and save it.
 * 
 * Return Value: TRUE if the project was saved. FALSE on error.
 **/
gboolean
glade_project_save_as (GladeProject *project)
{
	gchar *backup;

	g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

	/* Keep the previous path */
	backup = project->path;
	project->path = glade_project_ui_get_path (_("Save as ..."));

	/* If the user hit cancel, restore its previous name and return */
	if (project->path == NULL) {
		project->path = backup;
		return FALSE;
	}

	/* On error, warn and restore its previous name and return */
	if (!glade_project_save_to_file (project, project->path)) {
		glade_util_ui_warn (_("Invalid file name"));
		g_free (project->path);
		project->path = backup;
		return FALSE;
	}

	/* Free the backup and return; */
	g_free (project->name);
	project->name = g_path_get_basename (project->path);
	glade_project_refresh_menu_item (project);
	g_free (backup);

	return TRUE;
}	

/**
 * glade_project_open:
 * @path: 
 * 
 * Open a project. If @path is NULL launches a file selector
 * 
 * Return Value: TRUE on success false on error.
 **/
gboolean
glade_project_open (const gchar *path)
{
	GladeProjectWindow *gpw;
	GladeProject *project;
	gchar *file_path = NULL;

	gpw = glade_project_window_get ();

	if (!path)
		file_path = glade_project_ui_get_path (_("Open ..."));
	else
		file_path = g_strdup (path);

	/* If the user hit cancel, return */
	if (!file_path)
		return FALSE;

	/* If the project is previously loaded, don't re-load */
	if ((project = glade_project_check_previously_loaded (path)) != NULL) {
		glade_project_window_set_project (gpw, project);
		g_free (file_path);
		return TRUE;
	}

	project = glade_project_open_from_file (file_path);

	if (!project) {
		glade_util_ui_warn (_("Could not open project."));
		g_free (file_path);
		return FALSE;
	}

	glade_project_window_add_project (gpw, project);
	g_free (file_path);

	return TRUE;
}

