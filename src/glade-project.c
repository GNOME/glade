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
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-xml-utils.h"
#include "glade-widget.h"
#include "glade-placeholder.h"
#include "glade-utils.h"

static void glade_project_class_init (GladeProjectClass *class);
static void glade_project_init (GladeProject *project);

enum
{
	ADD_WIDGET,
	REMOVE_WIDGET,
	WIDGET_NAME_CHANGED,
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static guint glade_project_signals[LAST_SIGNAL] = {0};
static GObjectClass *parent_class = NULL;

GType
glade_project_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GladeProjectClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) glade_project_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (GladeProject),
			0,
			(GInstanceInitFunc) glade_project_init
		};

		type = g_type_register_static (G_TYPE_OBJECT, "GladeProject", &info, 0);
	}

	return type;
}

static void
glade_project_class_init (GladeProjectClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	glade_project_signals[ADD_WIDGET] =
		g_signal_new ("add_widget",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeProjectClass, add_widget),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);

	glade_project_signals[REMOVE_WIDGET] =
		g_signal_new ("remove_widget",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeProjectClass, remove_widget),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);

	glade_project_signals[WIDGET_NAME_CHANGED] =
		g_signal_new ("widget_name_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeProjectClass, widget_name_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);

	glade_project_signals[SELECTION_CHANGED] =
		g_signal_new ("selection_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeProjectClass, selection_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	class->add_widget = NULL;
	class->remove_widget = NULL;
	class->widget_name_changed = NULL;
	class->selection_changed = NULL;
}

static void
glade_project_init (GladeProject *project)
{
	project->name = NULL;
	project->widgets = NULL;
	project->selection = NULL;
	project->undo_stack = NULL;
	project->prev_redo_item = NULL;
}

GladeProject *
glade_project_new (gboolean untitled)
{
	GladeProject *project;
	static gint i = 1;

	project = g_object_new (GLADE_TYPE_PROJECT, NULL);

	if (untitled)
		project->name = g_strdup_printf ("Untitled %i", i++);

	return project;
}

void
glade_project_selection_changed (GladeProject *project)
{
	g_signal_emit (G_OBJECT (project),
		       glade_project_signals [SELECTION_CHANGED],
		       0);
}

static void
glade_project_add_widget_real (GladeProject *project,
			       GtkWidget *widget)
{
	GladeWidget *gwidget;

	/* We don't list placeholders */
	if (GLADE_IS_PLACEHOLDER (widget))
		return;

	/* If it's a container add the children as well */
	if (GTK_IS_CONTAINER (widget)) {
		GList *list;
		GtkWidget *child;

		list = gtk_container_get_children (GTK_CONTAINER (widget));
		for (; list; list = list->next) {
			child = list->data;
			glade_project_add_widget_real (project, child);
		}
	}

	gwidget = glade_widget_get_from_gtk_widget (widget);

	/* The internal widgets (e.g. the label of a GtkButton) are handled
	 * by gtk and don't have an associated GladeWidget: we don't want to
	 * add these to our list. It would be nicer to have a flag to check
	 * (as we do for placeholdres) instead of checking for the associated
	 * GladeWidget, so that we can assert that if a widget is _not_ internal,
	 * it _must_ have a corresponding GladeWidget... Anyway this suffice
	 * for now.
	 */
	if (!gwidget)
		return;

	gwidget->project = project;

	project->widgets = g_list_prepend (project->widgets, widget);
	g_signal_emit (G_OBJECT (project),
		       glade_project_signals [ADD_WIDGET],
		       0,
		       gwidget);
}

/**
 * glade_project_add_widget:
 * @project: the project the widget is added to
 * @widget: the GladeWidget to add
 * @parent: the GladeWidget @widget is reparented to
 *
 * Adds a widget to the project. Parent should be NULL for toplevels.
 */
void
glade_project_add_widget (GladeProject *project,
			  GladeWidget *widget,
			  GladeWidget *parent)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	glade_project_add_widget_real (project, widget->widget);

	/* reparent */
	widget->parent = parent;
	if (parent) {
		g_return_if_fail (GLADE_IS_WIDGET (parent));

		widget->parent = parent;
		parent->children = g_list_append (parent->children, widget);
	}

	project->changed = TRUE;
}

/**
 * Note that when removing the GtkWidget from the project we
 * don't change ->project in the associated GladeWidget, this
 * way UNDO works.
 */
static void
glade_project_remove_widget_real (GladeProject *project,
				  GtkWidget *widget)
{
	if (GLADE_IS_PLACEHOLDER (widget))
		return;

	/* If it's a container remove the children as well */
	if (GTK_IS_CONTAINER (widget)) {
		GList *list;
		GtkWidget *child;

		list = gtk_container_get_children (GTK_CONTAINER (widget));
		for (; list; list = list->next) {
			child = list->data;
			glade_project_remove_widget_real (project, child);
		}
	}

	project->selection = g_list_remove (project->selection, widget);
	glade_project_selection_changed (project);

	project->widgets = g_list_remove (project->widgets, widget);
	g_signal_emit (G_OBJECT (project),
		       glade_project_signals [REMOVE_WIDGET],
		       0,
		       widget);
}

/**
 * glade_project_remove_widget:
 * @widget: the GladeWidget to remove
 *
 * Remove a widget from the project.
 */
void
glade_project_remove_widget (GladeWidget *widget)
{
	GladeWidget *parent;

	g_return_if_fail (GLADE_IS_WIDGET (widget));

	glade_project_remove_widget_real (widget->project, widget->widget);

	/* remove from the parent's children list */
	parent = widget->parent;
	if (parent) {
		g_return_if_fail (GLADE_IS_WIDGET (parent));

		parent->children = g_list_remove (parent->children, widget);
	}

	widget->project->changed = TRUE;
}

void
glade_project_widget_name_changed (GladeProject *project,
				   GladeWidget *widget)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	g_signal_emit (G_OBJECT (project),
		       glade_project_signals [WIDGET_NAME_CHANGED],
		       0,
		       widget);

	project->changed = TRUE;
}

/**
 * glade_project_get_widget_by_name:
 * @project: The project in which to look for
 * @name: The user visible name of the widget we are looking for
 * 
 * Finds a GladeWidget inside a project given its name
 * 
 * Return Value: a pointer to the wiget, NULL if the widget does not exist
 */
GladeWidget *
glade_project_get_widget_by_name (GladeProject *project, const gchar *name)
{
	GList *list;

	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	for (list = project->widgets; list; list = list->next) {
		GladeWidget *widget;

		widget = glade_widget_get_from_gtk_widget (list->data);
		g_return_val_if_fail (widget->name != NULL, NULL);
		if (strcmp (widget->name, name) == 0)
			return widget;
	}

	return NULL;
}

/**
 * glade_project_new_widget_name:
 * @project: The project in which we will insert this widget
 * @base_name: base name of the widget to create
 *
 * Creates a new name for the widget that doesn't collides with
 * any of the names on the project passed as argument.  This name
 * will start with @base_name.
 *
 * Return Value: a string to the new name, NULL if there is not enough memory
 */
char *
glade_project_new_widget_name (GladeProject *project, const char *base_name)
{
	gint i = 1;
	gchar *name;

	while (TRUE) {
		name = g_strdup_printf ("%s%i", base_name, i);
		/* not enough memory? */
		if (name == NULL)
			break;

		/* ok, there is no widget with this name, so return the name */
		if (glade_project_get_widget_by_name (project, name) == NULL)
			return name;

		/* we already have a widget with this name, so free the name and
		 * try again with another number */
		g_free (name);
		i++;
	}

	return NULL;
}

void
glade_project_selection_clear (GladeProject *project, gboolean emit_signal)
{
	GtkWidget *widget;
	GList *list;

	g_return_if_fail (GLADE_IS_PROJECT (project));

	if (project->selection == NULL)
		return;

	for (list = project->selection; list; list = list->next) {
		widget = list->data;
		glade_util_remove_nodes (widget);
	}

	g_list_free (project->selection);
	project->selection = NULL;

	if (emit_signal)
		glade_project_selection_changed (project);
}

void
glade_project_selection_remove (GladeProject *project,
				GtkWidget *widget,
				gboolean emit_signal)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));

	if (!glade_util_has_nodes (widget))
		return;

	glade_util_remove_nodes (widget);

	if (project) {
		project->selection = g_list_remove (project->selection, widget);

		if (emit_signal)
			glade_project_selection_changed (project);
	}
}

void
glade_project_selection_add (GladeProject *project,
			     GtkWidget *widget,
			     gboolean emit_signal)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));

	if (glade_util_has_nodes (widget))
		return;

	glade_util_add_nodes (widget);

	if (project) {
		project->selection = g_list_prepend (project->selection, widget);

		if (emit_signal)
			glade_project_selection_changed (project);
	}
}

void
glade_project_selection_set (GladeProject *project,
			     GtkWidget *widget,
			     gboolean emit_signal)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));

	if (glade_util_has_nodes (widget))
		return;
	    
	glade_project_selection_clear (project, FALSE);
	glade_project_selection_add (project, widget, emit_signal);
}	

GList *
glade_project_selection_get (GladeProject *project)
{
	return project->selection;
}

/**
 * glade_project_write:
 * @project: 
 * 
 * Returns the root node of a newly created xml representation of the
 * project and its contents.
 */
static GladeXmlNode *
glade_project_write (GladeXmlContext *context, const GladeProject *project)
{
	GladeXmlNode *node;
	GList *list;

	node = glade_xml_node_new (context, GLADE_XML_TAG_PROJECT);
	if (!node)
		return NULL;

	for (list = project->widgets; list; list = list->next) {
		GladeWidget *widget;
		GladeXmlNode *child;

		widget = glade_widget_get_from_gtk_widget (list->data);

		/* 
		 * Append toplevel widgets. Each widget then takes
		 * care of appending its children.
		 */
		if (GLADE_WIDGET_IS_TOPLEVEL (widget)) {
			child = glade_widget_write (context, widget);
			if (!child)
				return NULL;

			glade_xml_node_append_child (node, child);
		}
	}

	return node;
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
	for (; child; child = glade_xml_node_next (child)) {
		if (!glade_xml_node_verify (child, GLADE_XML_TAG_WIDGET))
			return NULL;
		widget = glade_widget_new_from_node (child, project);
		if (!widget)
			return NULL;
		project->widgets = g_list_append (project->widgets, widget->widget);
	}
	project->widgets = g_list_reverse (project->widgets);

	return project;	
}

/**
 * glade_project_open:
 * @path: 
 * 
 * Open a project at the given path.
 * On success returns the opened project else NULL.
 */
GladeProject *
glade_project_open (const gchar *path)
{
	GladeXmlContext *context;
	GladeXmlDoc *doc;
	GladeProject *project;

	context = glade_xml_context_new_from_path (path, NULL, GLADE_XML_TAG_PROJECT);
	if (!context)
		return NULL;
	doc = glade_xml_context_get_doc (context);
	project = glade_project_new_from_node (glade_xml_doc_get_root (doc));
	glade_xml_context_free (context);

	if (project) {
		project->path = g_strdup_printf ("%s", path);
		g_free (project->name);
		project->name = g_path_get_basename (project->path);
		project->changed = FALSE;
	}

	return project;
}

/**
 * glade_project_save:
 * @project:
 * @path 
 * 
 * Save the project to the given path. Returns TRUE on success.
 */
gboolean
glade_project_save (GladeProject *project, const gchar *path)
{
	GladeXmlContext *context;
	GladeXmlNode *root;
	GladeXmlDoc *xml_doc;
	gboolean ret;

	xml_doc = glade_xml_doc_new ();
	if (!xml_doc) {
		g_warning ("Could not create xml document\n");
		return FALSE;
	}

	context = glade_xml_context_new (xml_doc, NULL);
	root = glade_project_write (context, project);
	glade_xml_context_destroy (context);
	if (!root)
		return FALSE;

	glade_xml_doc_set_root (xml_doc, root);
	ret = glade_xml_doc_save (xml_doc, path);
	glade_xml_doc_free (xml_doc);

	if (ret < 0)
		return FALSE;

	g_free (project->path);
	project->path = g_strdup_printf ("%s", path);
	g_free (project->name);
	project->name = g_path_get_basename (project->path);

	project->changed = FALSE;

	return TRUE;
}

