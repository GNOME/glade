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
#include "glade-editor.h"
#include "glade-utils.h"
#include "glade-id-allocator.h"
#include "glade-app.h"

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

/**
 * glade_project_get_type:
 *
 * Creates the typecode for the #GladeProject object type.
 *
 * Returns: the typecode for the #GladeProject object type
 */
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
			      G_STRUCT_OFFSET (GladeProjectClass, add_object),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_POINTER);

	glade_project_signals[REMOVE_WIDGET] =
		g_signal_new ("remove_widget",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeProjectClass, remove_object),
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
	
	class->add_object = NULL;
	class->remove_object = NULL;
	class->widget_name_changed = NULL;
	class->selection_changed = NULL;
}

static void
glade_project_init (GladeProject *project)
{
	project->path = NULL;
	project->name = NULL;
	project->instance = 0;
	project->objects = NULL;
	project->selection = NULL;
	project->undo_stack = NULL;
	project->prev_redo_item = NULL;
	project->widget_names_allocator = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) glade_id_allocator_free);
	project->widget_old_names = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) g_free);
	project->tooltips = gtk_tooltips_new ();
}

/**
 * glade_project_new:
 * @untitled: Whether or not this project is untitled
 *
 * Creates a new #GladeProject. If @untitled is %TRUE, sets the project's
 * name to "Untitled 1" or some such, giving a unique number each time
 * called.
 *
 * Returns: a new #GladeProject
 */
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
	GList *l;
	for (l = original_list; l; l = l->next)
		g_object_unref (G_OBJECT (l->data));

	if (original_list != NULL)
		g_list_free (original_list);
}

static void
glade_project_dispose (GObject *object)
{
	GladeProject *project = GLADE_PROJECT (object);
	GList        *list;
	GladeWidget  *gwidget;

	glade_project_selection_clear (project, TRUE);

	glade_project_list_unref (project->undo_stack);
	project->undo_stack = NULL;

	/* Remove objects from the project */
	for (list = project->objects; list; list = list->next)
	{
		gwidget = glade_widget_get_from_gobject (list->data);

		g_object_unref (G_OBJECT (list->data));
		g_object_unref (G_OBJECT (gwidget));
	}
	project->objects = NULL;

	gtk_object_destroy (GTK_OBJECT (project->tooltips));
	project->tooltips = NULL;

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
glade_project_finalize (GObject *object)
{
	GladeProject *project = GLADE_PROJECT (object);

	g_free (project->name);
	g_free (project->path);

	g_hash_table_destroy (project->widget_names_allocator);
	g_hash_table_destroy (project->widget_old_names);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * glade_project_selection_changed:
 * @project: a #GladeProject
 *
 * Causes @project to emit a "selection_changed" signal.
 */
void
glade_project_selection_changed (GladeProject *project)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_signal_emit (G_OBJECT (project),
		       glade_project_signals [SELECTION_CHANGED],
		       0);
}

static void
glade_project_on_widget_notify (GladeWidget *widget, GParamSpec *arg, GladeProject *project)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (GLADE_IS_PROJECT (project));

	switch (arg->name[0])
	{
	case 'n':
		if (strcmp (arg->name, "name") == 0)
		{
			const char *old_name = g_hash_table_lookup (project->widget_old_names, widget);
			glade_project_widget_name_changed (project, widget, old_name);
			g_hash_table_insert (project->widget_old_names, widget, g_strdup (glade_widget_get_name (widget)));
		}

	case 'p':
		if (strcmp (arg->name, "project") == 0)
			glade_project_remove_object (project, glade_widget_get_object (widget));
	}
}

/**
 * glade_project_add_object:
 * @project: the #GladeProject the widget is added to
 * @object: the #GObject to add
 *
 * Adds an object to the project.
 */
void
glade_project_add_object (GladeProject *project, GObject *object)
{
	GladeWidget          *gwidget;
	GList                *list, *children;
	GtkWindow            *transient_parent;

	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (G_IS_OBJECT      (object));

	/* We don't list placeholders */
	if (GLADE_IS_PLACEHOLDER (object))
		return;

	/* Only widgets accounted for in the catalog or widgets declared
	 * in the plugin with glade_widget_new_for_internal_child () are
	 * usefull in the project.
	 */
	if ((gwidget = glade_widget_get_from_gobject (object)) == NULL)
		return;

	if ((children = glade_widget_class_container_get_all_children
	     (gwidget->widget_class, gwidget->object)) != NULL)
	{
		for (list = children; list && list->data; list = list->next)
			glade_project_add_object (project, G_OBJECT (list->data));
		g_list_free (children);
	}

	glade_widget_set_project (gwidget, (gpointer)project);
	g_hash_table_insert (project->widget_old_names,
			     gwidget, g_strdup (glade_widget_get_name (gwidget)));

	g_signal_connect (G_OBJECT (gwidget), "notify",
			  (GCallback) glade_project_on_widget_notify, project);

	project->objects = g_list_prepend (project->objects, g_object_ref (object));
	project->changed = TRUE;
	g_signal_emit (G_OBJECT (project),
		       glade_project_signals [ADD_WIDGET],
		       0,
		       gwidget);

	if (GTK_IS_WINDOW (object) &&
	    (transient_parent = glade_default_app_get_transient_parent ()) != NULL)
		gtk_window_set_transient_for (GTK_WINDOW (object), transient_parent);
}

/**
 * glade_project_has_object:
 * @project: the #GladeProject the widget is added to
 * @object: the #GObject to search
 *
 * Returns whether this object is in this project.
 */
gboolean
glade_project_has_object (GladeProject *project, GObject *object)
{
	g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);
	g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
	return (g_list_find (project->objects, object)) != NULL;
}

/**
 * glade_project_release_widget_name:
 * @project: a #GladeProject
 * @glade_widget:
 * @widget_name:
 *
 * TODO: Write me
 */
void
glade_project_release_widget_name (GladeProject *project, GladeWidget *glade_widget, const char *widget_name)
{
	const char *first_number = widget_name;
	char *end_number;
	char *base_widget_name;
	GladeIDAllocator *id_allocator;
	gunichar ch;
	int id;

	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GLADE_IS_WIDGET (glade_widget));

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
	g_hash_table_remove (project->widget_old_names, glade_widget);
	g_free (base_widget_name);
}

/**
 * glade_project_remove_object:
 * @project: a #GladeProject
 * @widget: the #GtkWidget to remove
 *
 * Removes @widget from @project.
 *
 * Note that when removing the #GtkWidget from the project we
 * don't change ->project in the associated #GladeWidget; this
 * way UNDO can work.
 */
void
glade_project_remove_object (GladeProject *project, GObject *object)
{
	GladeWidget          *gwidget;
	GList                *link, *list, *children;
	
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (G_IS_OBJECT      (object));

	if (GLADE_IS_PLACEHOLDER (object))
		return;

	if ((gwidget = glade_widget_get_from_gobject (object)) == NULL)
		return;
	
	if ((children = glade_widget_class_container_get_children (gwidget->widget_class,
								   gwidget->object)) != NULL)
	{
		for (list = children; list && list->data; list = list->next)
			glade_project_remove_object (project, G_OBJECT (list->data));
		g_list_free (children);
	}
	
	glade_project_selection_remove (project, object, TRUE);

	if ((link = g_list_find (project->objects, object)) != NULL)
	{
		g_object_unref (object);
		glade_project_release_widget_name (project, gwidget,
						   glade_widget_get_name (gwidget));
		project->objects = g_list_delete_link (project->objects, link);
	}

	project->changed = TRUE;
	g_signal_emit (G_OBJECT (project),
		       glade_project_signals [REMOVE_WIDGET],
		       0,
		       gwidget);
}

/**
 * glade_project_widget_name_changed:
 * @project: a #GladeProject
 * @widget: a #GladeWidget
 * @old_name:
 *
 * TODO: write me
 */
void
glade_project_widget_name_changed (GladeProject *project, GladeWidget *widget, const char *old_name)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	glade_project_release_widget_name (project, widget, old_name);
	
	g_signal_emit (G_OBJECT (project),
		       glade_project_signals [WIDGET_NAME_CHANGED],
		       0,
		       widget);

	project->changed = TRUE;
}

/**
 * glade_project_get_widget_by_name:
 * @project: a #GladeProject
 * @name: The user visible name of the widget we are looking for
 * 
 * Searches through @project looking for a #GladeWidget named @name.
 * 
 * Returns: a pointer to the widget, %NULL if the widget does not exist
 */
GladeWidget *
glade_project_get_widget_by_name (GladeProject *project, const gchar *name)
{
	GList *list;

	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	for (list = project->objects; list; list = list->next) {
		GladeWidget *widget;

		widget = glade_widget_get_from_gobject (list->data);
		g_return_val_if_fail (widget->name != NULL, NULL);
		if (strcmp (widget->name, name) == 0)
			return widget;
	}

	return NULL;
}

/**
 * glade_project_new_widget_name:
 * @project: a #GladeProject
 * @base_name: base name of the widget to create
 *
 * Creates a new name for a widget that doesn't collide with any of the names 
 * already in @project. This name will start with @base_name.
 *
 * Returns: a string containing the new name, %NULL if there is not enough 
 *          memory for this string
 */
char *
glade_project_new_widget_name (GladeProject *project, const char *base_name)
{
	gchar *name;
	GladeIDAllocator *id_allocator;
	guint i = 1;
	
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

	while (TRUE)
	{
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

/**
 * glade_project_is_selected:
 * @project: a #GladeProject
 * @object: a #GObject
 *
 * Returns: whether @object is in @project selection
 */
gboolean
glade_project_is_selected (GladeProject *project,
			   GObject      *object)
{
	g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);
	return (g_list_find (project->selection, object)) != NULL;
}

/**
 * glade_project_selection_clear:
 * @project: a #GladeProject
 * @emit_signal: whether or not to emit a signal indication a selection change
 *
 * TODO: write me
 *
 * If @emit_signal is %TRUE, calls glade_project_selection_changed().
 */
void
glade_project_selection_clear (GladeProject *project, gboolean emit_signal)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	if (project->selection == NULL)
		return;

	glade_util_clear_selection ();

	g_list_free (project->selection);
	project->selection = NULL;

	if (emit_signal)
		glade_project_selection_changed (project);
}

/**
 * glade_project_selection_remove:
 * @project: a #GladeProject
 * @widget:
 * @emit_signal: whether or not to emit a signal indication a selection change
 *
 * TODO: write me
 *
 * If @emit_signal is %TRUE, calls glade_project_selection_changed().
 */
void
glade_project_selection_remove (GladeProject *project,
				GObject      *object,
				gboolean      emit_signal)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (G_IS_OBJECT      (object));

	if (glade_project_is_selected (project, object))
	{
		if (GTK_IS_WIDGET (object))
			glade_util_remove_selection (GTK_WIDGET (object));
		project->selection = g_list_remove (project->selection, object);
		if (emit_signal)
			glade_project_selection_changed (project);
	}
}

/**
 * glade_project_selection_add:
 * @project: a #GladeProject
 * @widget:
 * @emit_signal: whether or not to emit a signal indication a selection change
 *
 * TODO: write me
 *
 * If @emit_signal is %TRUE, calls glade_project_selection_changed().
 */
void
glade_project_selection_add (GladeProject *project,
			     GObject      *object,
			     gboolean      emit_signal)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (G_IS_OBJECT      (object));
	g_return_if_fail (g_list_find (project->objects, object) != NULL);

	if (glade_project_is_selected (project, object) == FALSE)
	{
		if (GTK_IS_WIDGET (object))
			glade_util_add_selection (GTK_WIDGET (object));
		project->selection = g_list_prepend (project->selection, object);
		if (emit_signal)
			glade_project_selection_changed (project);
	}
}

/**
 * glade_project_selection_set:
 * @project: a #GladeProject
 * @widget:
 * @emit_signal: whether or not to emit a signal indication a selection change
 *
 * TODO: write me
 *
 * If @emit_signal is %TRUE, calls glade_project_selection_changed().
 */
void
glade_project_selection_set (GladeProject *project,
			     GObject      *object,
			     gboolean      emit_signal)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (G_IS_OBJECT      (object));

	if (g_list_find (project->objects, object) == NULL)
		return;

	if (glade_project_is_selected (project, object) == FALSE ||
	    g_list_length (project->selection) != 1)
	{
		glade_project_selection_clear (project, FALSE);
		glade_project_selection_add (project, object, emit_signal);
	}
}	

/**
 * glade_project_selection_get:
 * @project: a #GladeProject
 *
 * Returns: a #GList containing the #GtkWidget items currently selected in 
 *          @project
 */
GList *
glade_project_selection_get (GladeProject *project)
{
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

	return project->selection;
}

/**
 * glade_project_write:
 * @project: a #GladeProject
 * 
 * Returns: a libglade's GladeInterface representation of the
 *          project and its contents
 */
static GladeInterface *
glade_project_write (const GladeProject *project)
{
	GladeInterface *interface;
	GList *list, *tops = NULL;
	guint i;

	interface = g_new0 (GladeInterface, 1);
	interface->names = g_hash_table_new (g_str_hash, g_str_equal);
	interface->strings = g_hash_table_new_full (g_str_hash,
						    g_str_equal,
						    (GDestroyNotify)g_free,
						    NULL);

        for (i = 0, list = project->objects; list; list = list->next)
	{
		GladeWidget *widget;
		GladeWidgetInfo *info;

		widget = glade_widget_get_from_gobject (list->data);

		/* 
		 * Append toplevel widgets. Each widget then takes
		 * care of appending its children.
		 */
		if (g_type_is_a (widget->widget_class->type, GTK_TYPE_WINDOW))
		{
			info = glade_widget_write (widget, interface);
			if (!info)
				return NULL;

			tops = g_list_prepend (tops, info);
			++i;
		}
	}
	interface->n_toplevels = i;
        interface->toplevels = (GladeWidgetInfo **) g_new (GladeWidgetInfo *, i);
        for (i = 0, list = tops; list; list = list->next, ++i)
            interface->toplevels[i] = list->data;

	g_list_free (tops);

	return interface;
}

static GladeProject *
glade_project_new_from_interface (GladeInterface *interface, const gchar *path)
{
	GladeProject *project;
	GladeWidget *widget;
	guint i;

	g_return_val_if_fail (interface != NULL, NULL);

	project = glade_project_new (FALSE);

	if (g_path_is_absolute (path))
		project->path = g_strdup (path);
	else
		project->path =
			g_build_filename (g_get_current_dir (), path, NULL);
	
	if (project->name) g_free (project->name);

	project->name = g_path_get_basename (path);
	project->selection = NULL;
	project->objects = NULL;

	if (interface->n_requires)
		g_warning ("We currently do not support projects requiring additional libs");

	for (i = 0; i < interface->n_toplevels; ++i)
	{
		widget = glade_widget_read ((gpointer)project, interface->toplevels[i]);
		if (!widget)
		{
			g_warning ("Failed to read a <widget> tag");
			continue;
		}
		glade_project_add_object (project, widget->object);
	}

	project->changed = FALSE;

	return project;	
}

/**
 * glade_project_open:
 * @path:
 * 
 * Opens a project at the given path.
 *
 * Returns: a new #GladeProject for the opened project on success, %NULL on 
 *          failure
 */
GladeProject *
glade_project_open (const gchar *path)
{
	GladeProject *project;
	GladeInterface *interface;
	
	g_return_val_if_fail (path != NULL, NULL);

	interface = glade_parser_parse_file (path, NULL);
	if (!interface)
		return NULL;

	project = glade_project_new_from_interface (interface, path);
	
	glade_interface_destroy (interface);

	return project;
}

/**
 * glade_project_save:
 * @project: a #GladeProject
 * @path: location to save glade file
 * @error: an error from the G_FILE_ERROR domain.
 * 
 * Saves @project to the given path. 
 *
 * Returns: %TRUE on success, %FALSE on failure
 */
gboolean
glade_project_save (GladeProject *project, const gchar *path, GError **error)
{
	GladeInterface *interface;
	gboolean        ret;

	g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

	interface = glade_project_write (project);
	if (!interface)
	{
		g_warning ("Could not write glade document\n");
		return FALSE;
	}

	ret = glade_interface_dump_full (interface, path, error);
	glade_interface_destroy (interface);

	if (path != project->path)
        {
		g_free (project->path);
		project->path = g_strdup_printf ("%s", path);
	}
	g_free (project->name);
	project->name = g_path_get_basename (project->path);

	project->changed = FALSE;

	return ret;
}

void
glade_project_changed (GladeProject *project)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	project->changed = TRUE;
}


/**
 * glade_project_get_tooltips:
 * @project: a #GladeProject
 *
 * Returns: a #GtkTooltips object containing all tooltips for @project
 */
GtkTooltips *
glade_project_get_tooltips (GladeProject *project)
{
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);
	return project->tooltips;
}

/**
 * glade_project_get_menuitem
 */
GtkWidget *
glade_project_get_menuitem (GladeProject *project)
{
	GtkUIManager *ui;
	gchar        *path;

	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

	ui   = g_object_get_data (G_OBJECT (project->action), "ui");
	path = g_object_get_data (G_OBJECT (project->action), "menuitem_path");
	return gtk_ui_manager_get_widget (ui, path);
}

/**
 * glade_project_get_menuitem_merge_id
 */
guint
glade_project_get_menuitem_merge_id (GladeProject *project)
{
	g_return_val_if_fail (GLADE_IS_PROJECT (project), 0);
	return GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (project->action), "merge_id"));
}

/*
 * glade_project_set_accel_group:
 *
 * Set @accel_group to every top level widget in @project.
 */
void
glade_project_set_accel_group (GladeProject *project, GtkAccelGroup *accel_group)
{
	GList *objects;

	g_return_if_fail (GLADE_IS_PROJECT (project) && GTK_IS_ACCEL_GROUP (accel_group));
                
	objects = project->objects;
	while (objects)
	{
		if(GTK_IS_WINDOW (objects->data))
			gtk_window_add_accel_group (GTK_WINDOW (objects->data), accel_group);

		objects = objects->next;
	}
}
