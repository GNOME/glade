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
#include <stdlib.h>

#include "glade.h"
#include "glade-project.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-xml-utils.h"
#include "glade-widget.h"
#include "glade-placeholder.h"
#include "glade-utils.h"
#include "glade-id-allocator.h"

static void glade_project_class_init (GladeProjectClass *class);
static void glade_project_init (GladeProject *project);
static void glade_project_finalize (GObject *object);
static void glade_project_dispose (GObject *object);

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

	if (!type)
	{
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

	object_class->finalize = glade_project_finalize;
	object_class->dispose = glade_project_dispose;
	
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
	project->widget_names_allocator = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) glade_id_allocator_free);
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

static void
glade_project_list_unref (GList *original_list)
{
	GList *l = original_list;
	while (l != NULL)
	{
		g_object_unref (G_OBJECT (l->data));
		l = l->next;
	}

	l = original_list;
	if (l != NULL)
		g_list_free (l);
}

static void
glade_project_dispose (GObject *object)
{
	GladeProject *project = GLADE_PROJECT (object);

	glade_project_list_unref (project->undo_stack);
	glade_project_list_unref (project->prev_redo_item);
	glade_project_list_unref (project->widgets);

	project->undo_stack = NULL;
	project->prev_redo_item = NULL;
	project->widgets = NULL;
	
	
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
glade_project_finalize (GObject *object)
{
	GladeProject *project = GLADE_PROJECT (object);

	g_free (project->name);
	g_free (project->path);
	
	g_hash_table_destroy (project->widget_names_allocator);
	
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
glade_project_selection_changed (GladeProject *project)
{
	g_signal_emit (G_OBJECT (project),
		       glade_project_signals [SELECTION_CHANGED],
		       0);
}

/**
 * glade_project_add_widget:
 * @project: the project the widget is added to
 * @widget: the GtkWidget to add
 *
 * Adds a widget to the project.
 */
void
glade_project_add_widget (GladeProject *project, GtkWidget *widget)
{
	GladeWidget *gwidget;

	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GTK_IS_WIDGET (widget));

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
			glade_project_add_widget (project, child);
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
	g_object_ref (widget);
	project->changed = TRUE;
	g_signal_emit (G_OBJECT (project),
		       glade_project_signals [ADD_WIDGET],
		       0,
		       gwidget);
}

void
glade_project_release_widget_name (GladeProject *project, const char *widget_name)
{
	const char *first_number = widget_name;
	char *end_number;
	char *base_widget_name;
	GladeIDAllocator *id_allocator;
	gunichar ch;
	int id;

	do
	{
		ch = g_utf8_get_char (first_number);

		if (ch == 0 || g_unichar_isdigit (ch))
			break;

		first_number = g_utf8_next_char (first_number);
	}
	while (TRUE);

	if (ch == 0)
		return;

	base_widget_name = g_strdup (widget_name);
	*(base_widget_name + (first_number - widget_name)) = 0;
	
	id_allocator = g_hash_table_lookup (project->widget_names_allocator, base_widget_name);
	if (id_allocator == NULL)
		goto lblend;

	id = (int) strtol (first_number, &end_number, 10);
	if (*end_number != 0)
		goto lblend;

	glade_id_allocator_release (id_allocator, id);

 lblend:
	g_free (base_widget_name);
}

/**
 * glade_project_remove_widget:
 * @project: the project the widget is removed from
 * @widget: the GtkWidget to remove
 *
 * Remove a widget from the project.
 * Note that when removing the GtkWidget from the project we
 * don't change ->project in the associated GladeWidget, this
 * way UNDO works.
 */
void
glade_project_remove_widget (GladeProject *project, GtkWidget *widget)
{
	GladeWidget *gwidget;
	GList *widget_l;
	
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	if (GLADE_IS_PLACEHOLDER (widget))
		return;

	/* If it's a container remove the children as well */
	if (GTK_IS_CONTAINER (widget)) {
		GList *list;
		GtkWidget *child;

		list = gtk_container_get_children (GTK_CONTAINER (widget));
		for (; list; list = list->next) {
			child = list->data;
			glade_project_remove_widget (project, child);
		}
	}

	gwidget = glade_widget_get_from_gtk_widget (widget);
	if (!gwidget)
		return;

	project->selection = g_list_remove (project->selection, widget);
	glade_project_selection_changed (project);

	widget_l = g_list_find (project->widgets, widget);
	if (widget_l != NULL)
	{
		g_object_unref (widget);
		glade_project_release_widget_name (project, gwidget->name);
		project->widgets = g_list_delete_link (project->widgets, widget_l);
	}

	project->changed = TRUE;
	g_signal_emit (G_OBJECT (project),
		       glade_project_signals [REMOVE_WIDGET],
		       0,
		       gwidget);
}

void
glade_project_widget_name_changed (GladeProject *project, GladeWidget *widget, const char *old_name)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	glade_project_release_widget_name (project, old_name);
	
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
	gchar *name;
	GladeIDAllocator *id_allocator;
	guint i = 1;
	
	while (TRUE) {
		id_allocator = g_hash_table_lookup (project->widget_names_allocator, base_name);

		if (id_allocator == NULL)
		{
			id_allocator = glade_id_allocator_new ();
			g_hash_table_insert (project->widget_names_allocator, g_strdup (base_name), id_allocator);
		}

		i = glade_id_allocator_alloc (id_allocator);
		name = g_strdup_printf ("%s%u", base_name, i);

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

	for (list = project->selection; list; list = list->next)
	{
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
	g_return_if_fail (GTK_IS_WIDGET (widget));

	if (!glade_util_has_nodes (widget))
		return;

	glade_util_remove_nodes (widget);

	if (project)
	{
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
	g_return_if_fail (GTK_IS_WIDGET (widget));

	if (glade_util_has_nodes (widget))
		return;

	glade_util_add_nodes (widget);

	if (project)
	{
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
	g_return_if_fail (GTK_IS_WIDGET (widget));

	if (glade_util_has_nodes (widget))
		return;
	    
	glade_project_selection_clear (project, FALSE);
	glade_project_selection_add (project, widget, emit_signal);
}	

GList *
glade_project_selection_get (GladeProject *project)
{
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

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
		if (!glade_xml_node_verify_silent (child, GLADE_XML_TAG_REQUIRES))
			continue;
		g_warning ("We currently do not support projects requiring additional libs");
	}

	child = glade_xml_node_get_children (node);
	for (; child; child = glade_xml_node_next (child)) {
		if (!glade_xml_node_verify_silent (child, GLADE_XML_TAG_WIDGET))
			continue;
		widget = glade_widget_new_from_node (child, project);
		if (!widget) {
			g_warning ("Failed to read a <wideget> tag");
			continue;
		}
		project->widgets = g_list_append (project->widgets, widget->widget);
		g_object_ref (widget->widget);
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
	if (!xml_doc)
	{
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

