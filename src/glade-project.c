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
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-id-allocator.h"
#include "glade-app.h"
#include "glade-marshallers.h"
#include "glade-catalog.h"

#include "glade-project.h"

enum
{
	ADD_WIDGET,
	REMOVE_WIDGET,
	WIDGET_NAME_CHANGED,
	SELECTION_CHANGED,
	CLOSE,
	RESOURCE_ADDED,
	RESOURCE_REMOVED,
	CHANGED,
	PARSE_FINISHED,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_HAS_UNSAVED_CHANGES,
	PROP_HAS_SELECTION,
	PROP_READ_ONLY
};

static guint         glade_project_signals[LAST_SIGNAL] = {0};
static GObjectClass *parent_class = NULL;
static GHashTable   *allocated_untitled_numbers = NULL;

/*******************************************************************
                            GObjectClass
 *******************************************************************/
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
glade_project_release_untitled_number (gint n)
{
	g_return_if_fail (allocated_untitled_numbers != NULL);

	g_hash_table_remove (allocated_untitled_numbers, GINT_TO_POINTER (n));
}

static void
glade_project_dispose (GObject *object)
{
	GladeProject *project = GLADE_PROJECT (object);
	GList        *list;
	GladeWidget  *gwidget;
	
	/* Emit close signal */
	g_signal_emit (object, glade_project_signals [CLOSE], 0);
	
	glade_project_selection_clear (project, TRUE);

	glade_project_list_unref (project->undo_stack);
	project->undo_stack = NULL;

	/* Unparent all widgets in the heirarchy first 
	 * (Since we are bookkeeping exact reference counts, we 
	 * dont want the hierarchy to just get destroyed)
	 */
	for (list = project->objects; list; list = list->next)
	{
		gwidget = glade_widget_get_from_gobject (list->data);

		if (gwidget->parent &&
		    glade_widget_adaptor_has_child (gwidget->parent->adaptor,
						    gwidget->parent->object,
						    gwidget->object))
			glade_widget_remove_child (gwidget->parent, gwidget);
	}

	/* Remove objects from the project */
	for (list = project->objects; list; list = list->next)
	{
		gwidget = glade_widget_get_from_gobject (list->data);

		g_object_unref (G_OBJECT (list->data)); /* Remove the GladeProject reference */
		g_object_unref (G_OBJECT (gwidget));  /* Remove the overall "Glade" reference */
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
	g_free (project->comment);

	if (project->untitled_number > 0)
		glade_project_release_untitled_number (project->untitled_number);

	g_hash_table_destroy (project->widget_names_allocator);
	g_hash_table_destroy (project->widget_old_names);
	g_hash_table_destroy (project->resources);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
glade_project_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
	GladeProject *project = GLADE_PROJECT (object);

	switch (prop_id)
	{
		case PROP_HAS_UNSAVED_CHANGES:
			g_value_set_boolean (value, project->changed);
			break;
		case PROP_HAS_SELECTION:
			g_value_set_boolean (value, project->has_selection);
			break;			
		case PROP_READ_ONLY:
			g_value_set_boolean (value, project->readonly);
			break;		
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;			
	}
}


/*******************************************************************
                          GladeProjectClass
 *******************************************************************/
static void
glade_project_walk_back (GladeProject *project)
{
	if (project->prev_redo_item)
		project->prev_redo_item = project->prev_redo_item->prev;
}

static void
glade_project_walk_forward (GladeProject *project)
{
	if (project->prev_redo_item)
		project->prev_redo_item = project->prev_redo_item->next;
	else
		project->prev_redo_item = project->undo_stack;
}


static void
glade_project_undo_impl (GladeProject *project)
{
	GladeCommand *cmd, *next_cmd;

	while ((cmd = glade_project_next_undo_item (project)) != NULL)
	{
		glade_command_undo (cmd);

		glade_project_walk_back (project);

		g_signal_emit (G_OBJECT (project),
			       glade_project_signals [CHANGED], 
			       0, cmd, FALSE);

		if ((next_cmd = glade_project_next_undo_item (project)) != NULL &&
		    (next_cmd->group_id == 0 || next_cmd->group_id != cmd->group_id))
			break;
	}
}

static void
glade_project_redo_impl (GladeProject *project)
{
	GladeCommand *cmd, *next_cmd;
	
	while ((cmd = glade_project_next_redo_item (project)) != NULL)
	{
		glade_command_execute (cmd);

		glade_project_walk_forward (project);

		g_signal_emit (G_OBJECT (project),
			       glade_project_signals [CHANGED],
			       0, cmd, TRUE);

		if ((next_cmd = glade_project_next_redo_item (project)) != NULL &&
		    (next_cmd->group_id == 0 || next_cmd->group_id != cmd->group_id))
			break;
	}
}

static GladeCommand *
glade_project_next_undo_item_impl (GladeProject *project)
{
	GList *l;

	if ((l = project->prev_redo_item) == NULL)
		return NULL;

	return GLADE_COMMAND (l->data);
}

static GladeCommand *
glade_project_next_redo_item_impl (GladeProject *project)
{
	GList *l;

	if ((l = project->prev_redo_item) == NULL)
		return project->undo_stack ? 
			GLADE_COMMAND (project->undo_stack->data) : NULL;
	else
		return l->next ? GLADE_COMMAND (l->next->data) : NULL;
}

static void
glade_project_push_undo_impl (GladeProject *project, GladeCommand *cmd)
{
	GList* tmp_redo_item;

	/* If there are no "redo" items, and the last "undo" item unifies with
	   us, then we collapse the two items in one and we're done */
	if (project->prev_redo_item != NULL && project->prev_redo_item->next == NULL)
	{
		GladeCommand *cmd1 = project->prev_redo_item->data;
		
		if (glade_command_unifies (cmd1, cmd))
		{
			glade_command_collapse (cmd1, cmd);
			g_object_unref (cmd);

			g_signal_emit (G_OBJECT (project),
				       glade_project_signals [CHANGED],
				       0, cmd1, TRUE);
			return;
		}
	}

	/* We should now free all the "redo" items */
	tmp_redo_item = g_list_next (project->prev_redo_item);
	while (tmp_redo_item)
	{
		g_assert (tmp_redo_item->data);
		g_object_unref (G_OBJECT (tmp_redo_item->data));
		tmp_redo_item = g_list_next (tmp_redo_item);
	}

	if (project->prev_redo_item)
	{
		g_list_free (g_list_next (project->prev_redo_item));
		project->prev_redo_item->next = NULL;
	}
	else
	{
		g_list_free (project->undo_stack);
		project->undo_stack = NULL;
	}

	/* and then push the new undo item */
	project->undo_stack = g_list_append (project->undo_stack, cmd);

	if (project->prev_redo_item == NULL)
		project->prev_redo_item = project->undo_stack;
	else
		project->prev_redo_item = g_list_next (project->prev_redo_item);


	g_signal_emit (G_OBJECT (project),
		       glade_project_signals [CHANGED],
		       0, cmd, TRUE);
}

static void
glade_project_changed_impl (GladeProject *project, 
			    GladeCommand *command,
			    gboolean      forward)
{
	if (!project->changed && !project->loading)
	{
		project->changed = TRUE;
		g_object_notify (G_OBJECT (project), "has-unsaved-changes");
	}

	glade_app_update_ui ();
}

/*******************************************************************
                            Initializers
 *******************************************************************/
static void
glade_project_init (GladeProject *project)
{
	project->path = NULL;
	project->name = NULL;
	project->instance = 0;
	project->untitled_number = 0;
	project->readonly = FALSE;
	project->objects = NULL;
	project->selection = NULL;
	project->has_selection = FALSE;
	project->undo_stack = NULL;
	project->prev_redo_item = NULL;
	project->widget_names_allocator = 
		g_hash_table_new_full (g_str_hash, g_str_equal, g_free, 
				       (GDestroyNotify) glade_id_allocator_free);
	project->widget_old_names = 
		g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) g_free);
	project->tooltips = gtk_tooltips_new ();
	project->accel_group = NULL;

	project->resources = g_hash_table_new_full (g_direct_hash, 
						    g_direct_equal, 
						    NULL, g_free);
}

static void
glade_project_class_init (GladeProjectClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->get_property = glade_project_get_property;
	object_class->finalize     = glade_project_finalize;
	object_class->dispose      = glade_project_dispose;
	
	class->add_object          = NULL;
	class->remove_object       = NULL;
	class->undo                = glade_project_undo_impl;
	class->redo                = glade_project_redo_impl;
	class->next_undo_item      = glade_project_next_undo_item_impl;
	class->next_redo_item      = glade_project_next_redo_item_impl;
	class->push_undo           = glade_project_push_undo_impl;

	class->widget_name_changed = NULL;
	class->selection_changed   = NULL;
	class->close               = NULL;
	class->resource_added      = NULL;
	class->resource_removed    = NULL;
	class->changed             = glade_project_changed_impl;
	
	/**
	 * GladeProject::add-widget:
	 * @gladeproject: the #GladeProject which received the signal.
	 * @arg1: the #GladeWidget that was added to @gladeproject.
	 *
	 * Emitted when a widget is added to a project.
	 */
	glade_project_signals[ADD_WIDGET] =
		g_signal_new ("add_widget",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeProjectClass, add_object),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GLADE_TYPE_WIDGET);

	/**
	 * GladeProject::remove-widget:
	 * @gladeproject: the #GladeProject which received the signal.
	 * @arg1: the #GladeWidget that was removed from @gladeproject.
	 * 
	 * Emitted when a widget is removed from a project.
	 */
	glade_project_signals[REMOVE_WIDGET] =
		g_signal_new ("remove_widget",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeProjectClass, remove_object),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GLADE_TYPE_WIDGET);


	/**
	 * GladeProject::widget-name-changed:
	 * @gladeproject: the #GladeProject which received the signal.
	 * @arg1: the #GladeWidget who's name changed.
	 *
	 * Emitted when @gwidget's name changes.
	 */
	glade_project_signals[WIDGET_NAME_CHANGED] =
		g_signal_new ("widget_name_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeProjectClass, widget_name_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      GLADE_TYPE_WIDGET);


	/** 
	 * GladeProject::selection-changed:
	 * @gladeproject: the #GladeProject which received the signal.
	 *
	 * Emitted when @gladeproject selection list changes.
	 */
	glade_project_signals[SELECTION_CHANGED] =
		g_signal_new ("selection_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeProjectClass, selection_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);


	/**
	 * GladeProject::close:
	 * @gladeproject: the #GladeProject which received the signal.
	 *
	 * Emitted when a project is closing (a good time to clean up
	 * any associated resources).
	 */
	glade_project_signals[CLOSE] =
		g_signal_new ("close",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeProjectClass, close),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);


	/** 
	 * GladeProject::resource-added:
	 * @gladeproject: the #GladeProject which received the signal.
	 * @arg1: the file's basename (in the project path).
	 *
	 * Emitted when a resource file required by a #GladeProperty is
	 * added to @gladeproject
	 */
	glade_project_signals[RESOURCE_ADDED] =
		g_signal_new ("resource-added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeProjectClass, resource_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);

	/**
	 * GladeProject::resource-removed:
	 * @gladeproject: the #GladeProject which received the signal.
	 * @arg1: the file's basename
	 *
	 * Emitted when a resource file is removed from @gladeproject
	 */
	glade_project_signals[RESOURCE_REMOVED] =
		g_signal_new ("resource-removed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeProjectClass, resource_removed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);


	/**
	 * GladeProject::changed:
	 * @gladeproject: the #GladeProject which received the signal.
	 * @arg1: the #GladeCommand that was executed
	 * @arg2: whether the command was executed or undone.
	 *
	 * Emitted when a @gladeproject's state changes via a #GladeCommand.
	 */
	glade_project_signals[CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GladeProjectClass, changed),
			      NULL, NULL,
			      glade_marshal_VOID__OBJECT_BOOLEAN,
			      G_TYPE_NONE,
			      2,
			      GLADE_TYPE_COMMAND, G_TYPE_BOOLEAN);

	/**
	 * GladeProject::parse-finished:
	 * @gladeproject: the #GladeProject which received the signal.
	 *
	 * Emitted when @gladeproject parsing has finished.
	 */
	glade_project_signals[PARSE_FINISHED] =
		g_signal_new ("parse-finished",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (GladeProjectClass, parse_finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_object_class_install_property (object_class,
					 PROP_HAS_UNSAVED_CHANGES,
					 g_param_spec_boolean ("has-unsaved-changes",
							       _("Has Unsaved Changes"),
							       _("Whether project has unsaved changes"),
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_HAS_SELECTION,
					 g_param_spec_boolean ("has-selection",
							       _("Has Selection"),
							       _("Whether project has a selection"),
							       FALSE,
							       G_PARAM_READABLE));

	g_object_class_install_property (object_class,
					 PROP_READ_ONLY,
					 g_param_spec_boolean ("read-only",
							       _("Read Only"),
							       _("Whether project is read only or not"),
							       FALSE,
							       G_PARAM_READABLE));

}

/*******************************************************************
                                  API
 *******************************************************************/
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


static gint
glade_project_get_untitled_number (void)
{
	gint i = 1;

	if (allocated_untitled_numbers == NULL)
		allocated_untitled_numbers = g_hash_table_new (NULL, NULL);

	g_return_val_if_fail (allocated_untitled_numbers != NULL, -1);

	while (TRUE)
	{
		if (g_hash_table_lookup (allocated_untitled_numbers, GINT_TO_POINTER (i)) == NULL)
		{
			g_hash_table_insert (allocated_untitled_numbers, 
					     GINT_TO_POINTER (i),
					     GINT_TO_POINTER (i));

			return i;
		}

		++i;
	}
}

static void
glade_project_set_readonly (GladeProject *project, gboolean readonly)
{
	g_assert (GLADE_IS_PROJECT (project));
	
	if (project->readonly != readonly)
	{
		project->readonly = readonly;
		g_object_notify (G_OBJECT (project), "read-only");
	}
}                                                                                               

/**
 * glade_project_get_readonly:
 * @project: a #GladeProject
 *
 * Gets whether the project is read only or not
 *
 * Returns: TRUE if project is read only
 */
gboolean
glade_project_get_readonly (GladeProject *project)
{
	g_assert (project != NULL);

	return project->readonly;
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

	project = g_object_new (GLADE_TYPE_PROJECT, NULL);

	if (untitled)
	{
		project->untitled_number = glade_project_get_untitled_number ();
		project->name = g_strdup_printf ("Untitled %d", project->untitled_number);
	}

	return project;
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


static void
gp_sync_resources (GladeProject *project, 
		   GladeProject *prev_project,
		   GladeWidget  *gwidget,
		   gboolean      remove)
{
	GList          *prop_list, *l;
	GladeProperty  *property;
	gchar          *resource, *full_resource;

	prop_list = g_list_copy (gwidget->properties);
	prop_list = g_list_concat
		(prop_list, g_list_copy (gwidget->packing_properties));

	for (l = prop_list; l; l = l->next)
	{
		property = l->data;
		if (property->class->resource)
		{
			GValue value = { 0, };

			if (remove)
			{
				glade_project_set_resource (project, property, NULL);
				continue;
			}

			glade_property_get_value (property, &value);
			
			if ((resource = glade_property_class_make_string_from_gvalue
			     (property->class, &value)) != NULL)
			{
				full_resource = glade_project_resource_fullpath
					(prev_project ? prev_project : project, resource);
			
				/* Use a full path here so that the current
				 * working directory isnt used.
				 */
				glade_project_set_resource (project, 
							    property,
							    full_resource);
				
				g_free (full_resource);
				g_free (resource);
			}
			g_value_unset (&value);
		}
	}
	g_list_free (prop_list);
}

static void
glade_project_sync_resources_for_widget (GladeProject *project, 
					 GladeProject *prev_project,
					 GladeWidget  *gwidget,
					 gboolean      remove)
{
	GList *children, *l;
	GladeWidget *gchild;

	children = glade_widget_adaptor_get_children
		(gwidget->adaptor, gwidget->object);

	for (l = children; l; l = l->next)
		if ((gchild = 
		     glade_widget_get_from_gobject (l->data)) != NULL)
			glade_project_sync_resources_for_widget 
				(project, prev_project, gchild, remove);
	if (children)
		g_list_free (children);

	gp_sync_resources (project, prev_project, gwidget, remove);
}

/**
 * glade_project_add_object:
 * @project: the #GladeProject the widget is added to
 * @old_project: the #GladeProject the widget was previously in
 *               (or %NULL for the clipboard)
 * @object: the #GObject to add
 *
 * Adds an object to the project.
 */
void
glade_project_add_object (GladeProject *project, 
			  GladeProject *old_project, 
			  GObject      *object)
{
	GladeWidget   *gwidget;
	GList         *list, *children;
	GtkWindow     *transient_parent;
	static gint    reentrancy_count = 0;

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

	/* Code body starts here */
	reentrancy_count++;

	if ((children = glade_widget_adaptor_get_children
	     (gwidget->adaptor, gwidget->object)) != NULL)
	{
		for (list = children; list && list->data; list = list->next)
			glade_project_add_object
				(project, old_project, G_OBJECT (list->data));
		g_list_free (children);
	}

	glade_widget_set_project (gwidget, (gpointer)project);
	g_hash_table_insert (project->widget_old_names,
			     gwidget, g_strdup (glade_widget_get_name (gwidget)));

	g_signal_connect (G_OBJECT (gwidget), "notify",
			  (GCallback) glade_project_on_widget_notify, project);

	project->objects = g_list_prepend (project->objects, g_object_ref (object));
	
	g_signal_emit (G_OBJECT (project),
		       glade_project_signals [ADD_WIDGET],
		       0, gwidget);

	if (GTK_IS_WINDOW (object) &&
	    (transient_parent = glade_app_get_transient_parent ()) != NULL)
		gtk_window_set_transient_for (GTK_WINDOW (object), transient_parent);

	/* Notify widget it was added to the project */
	glade_widget_project_notify (gwidget, project);

	/* Call this once at the end for every recursive call */
	if (--reentrancy_count == 0)
		glade_project_sync_resources_for_widget
			(project, old_project, gwidget, FALSE);
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
 * @object: the #GObject to remove
 *
 * Removes @object from @project.
 *
 * Note that when removing the #GObject from the project we
 * don't change ->project in the associated #GladeWidget; this
 * way UNDO can work.
 */
void
glade_project_remove_object (GladeProject *project, GObject *object)
{
	GladeWidget   *gwidget;
	GList         *link, *list, *children;
	static gint    reentrancy_count = 0;
	
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (G_IS_OBJECT      (object));

	if (GLADE_IS_PLACEHOLDER (object))
		return;

	if ((gwidget = glade_widget_get_from_gobject (object)) == NULL)
		return;

	/* Code body starts here */
	reentrancy_count++;

	/* Notify widget is being removed from the project */
	glade_widget_project_notify (gwidget, NULL);
	
	if ((children = 
	     glade_widget_adaptor_get_children (gwidget->adaptor,
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

	g_signal_emit (G_OBJECT (project),
		       glade_project_signals [REMOVE_WIDGET],
		       0,
		       gwidget);

	/* Call this once at the end for every recursive call */
	if (--reentrancy_count == 0)
		glade_project_sync_resources_for_widget (project, NULL, gwidget, TRUE);
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
	GladeIDAllocator *id_allocator;
	const gchar      *number;
	gchar            *name = NULL, *freeme = NULL;
	guint             i = 1;
	
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

	number = base_name + strlen (base_name);

	while (number > base_name && g_ascii_isdigit (number[-1]))
		--number;

	if (*number)
        {
		freeme = g_strndup (base_name, number - base_name);
		base_name = freeme;
	}
	
	id_allocator = g_hash_table_lookup (project->widget_names_allocator, base_name);

	if (id_allocator == NULL)
	{
		id_allocator = glade_id_allocator_new ();
		g_hash_table_insert (project->widget_names_allocator,
				     g_strdup (base_name), id_allocator);
	}

	while (TRUE)
	{
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

	g_free (freeme);
	return name;
}

void
glade_project_set_has_selection (GladeProject *project, gboolean has_selection)
{
	g_assert (GLADE_IS_PROJECT (project));

	if (project->has_selection != has_selection)
	{
		project->has_selection = has_selection;
		g_object_notify (G_OBJECT (project), "has-selection");
	}
}

/**
 * glade_project_get_has_selection:
 * @project: a #GladeProject
 *
 * Returns: whether @project currently has a selection
 */
gboolean
glade_project_get_has_selection (GladeProject *project)
{
	g_assert (GLADE_IS_PROJECT (project));

	return project->has_selection;
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
 * Clears @project's selection chain
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
	glade_project_set_has_selection (project, FALSE);

	if (emit_signal)
		glade_project_selection_changed (project);
}

/**
 * glade_project_selection_remove:
 * @project: a #GladeProject
 * @object:  a #GObject in @project
 * @emit_signal: whether or not to emit a signal 
 *               indicating a selection change
 *
 * Removes @object from the selection chain of @project
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
		if (project->selection == NULL)
			glade_project_set_has_selection (project, FALSE);
		if (emit_signal)
			glade_project_selection_changed (project);
	}
}

/**
 * glade_project_selection_add:
 * @project: a #GladeProject
 * @object:  a #GObject in @project
 * @emit_signal: whether or not to emit a signal indicating 
 *               a selection change
 *
 * Adds @object to the selection chain of @project
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
		if (project->selection == NULL)
			glade_project_set_has_selection (project, TRUE);
		project->selection = g_list_prepend (project->selection, object);
		if (emit_signal)
			glade_project_selection_changed (project);
	}
}

/**
 * glade_project_selection_set:
 * @project: a #GladeProject
 * @object:  a #GObject in @project
 * @emit_signal: whether or not to emit a signal 
 *               indicating a selection change
 *
 * Set the selection in @project to @object
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

	if (project->selection == NULL)
		glade_project_set_has_selection (project, TRUE);

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

static GList *
glade_project_required_libs (GladeProject *project)
{
	GList       *required = NULL, *l, *ll;
	GladeWidget *gwidget;
	gboolean     listed;

	for (l = project->objects; l; l = l->next)
	{
		gchar *catalog = NULL;

		gwidget = glade_widget_get_from_gobject (l->data);
		g_assert (gwidget);

		g_object_get (gwidget->adaptor, "catalog", &catalog, NULL);

		if (catalog)
		{
			listed = FALSE;
			for (ll = required; ll; ll = ll->next)
				if (!strcmp ((gchar *)ll->data, catalog))
				{
					listed = TRUE;
					break;
				}

			if (!listed)
				required = g_list_prepend (required, catalog);
		}
	}
	return required;
}

#define GLADE_XML_COMMENT "Generated with "PACKAGE_NAME

static gchar *
glade_project_make_comment ()
{
	time_t now = time (NULL);
	gchar *comment;
	comment = g_strdup_printf (GLADE_XML_COMMENT" "PACKAGE_VERSION" on %sby %s@%s",
				   ctime (&now), g_get_user_name (), g_get_host_name ());
	glade_util_replace (comment, '\n', ' ');
	
	return comment;
}

static void
glade_project_update_comment (GladeProject *project)
{
	gchar **lines, **l, *comment = NULL;
	
	/* If project has no comment -> add the new one */
	if (project->comment == NULL)
	{
		project->comment = glade_project_make_comment ();
		return;
	}
	
	for (lines = l = g_strsplit (project->comment, "\n", 0); *l; l++)
	{
		gchar *start;
		
		/* Ignore leading spaces */
		for (start = *l; *start && g_ascii_isspace (*start); start++);
		
		if (g_str_has_prefix (start, GLADE_XML_COMMENT))
		{
			/* This line was generated by glade -> updating... */
			g_free (*l);
			*l = comment = glade_project_make_comment ();
		}
	}
	
	if (comment)
	{
		g_free (project->comment);
		project->comment = g_strjoinv ("\n", lines);
	}
	
	g_strfreev (lines);
}

/**
 * glade_project_write:
 * @project: a #GladeProject
 * 
 * Returns: a libglade's GladeInterface representation of the
 *          project and its contents
 */
static GladeInterface *
glade_project_write (GladeProject *project)
{
	GladeInterface  *interface;
	GList           *required, *list, *tops = NULL;
	gchar          **strv = NULL;
	guint            i;

	interface = glade_parser_interface_new ();

	if ((required = glade_project_required_libs (project)) != NULL)
	{
		strv = g_malloc0 (g_list_length (required) * sizeof (char *));
		for (i = 0, list = required; list; i++, list = list->next)
			strv[i] = list->data; /* list->data is allocated for us */

		interface->n_requires = g_list_length (required);
		interface->requires   = strv;

		g_list_free (required);
	}

	for (i = 0, list = project->objects; list; list = list->next)
	{
		GladeWidget *widget;
		GladeWidgetInfo *info;

		widget = glade_widget_get_from_gobject (list->data);

		/* 
		 * Append toplevel widgets. Each widget then takes
		 * care of appending its children.
		 */
		if (widget->parent == NULL)
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

	glade_project_update_comment (project);
	interface->comment = g_strdup (project->comment);
	
	return interface;
}

static gboolean
loadable_interface (GladeInterface *interface, const gchar *path)
{
	GString *string = g_string_new (NULL);
	gboolean loadable = TRUE;
	guint i;

	/* Check for required plugins here
	 */
	for (i = 0; i < interface->n_requires; i++)
		if (!glade_catalog_is_loaded (interface->requires[i]))
		{
			g_string_append (string, interface->requires[i]);
			loadable = FALSE;
		}

	if (loadable == FALSE)
		glade_util_ui_message (glade_app_get_window(),
				       GLADE_UI_ERROR,
				       _("Failed to load %s.\n"
					 "The following required catalogs are unavailable: %s"),
				       path, string->str);
	g_string_free (string, TRUE);
	return loadable;
}

static GladeProject *
glade_project_new_from_interface (GladeInterface *interface, const gchar *path)
{
	GladeProject *project;
	GladeWidget  *widget;
	guint i;

	g_return_val_if_fail (interface != NULL, NULL);
	g_return_val_if_fail (path != NULL, NULL);

	if (loadable_interface (interface, path) == FALSE)
		return NULL;

	project = glade_project_new (FALSE);

	project->path = glade_util_canonical_path (path);
	
	if (project->name) g_free (project->name);

	project->name = g_path_get_basename (path);
	project->selection = NULL;
	project->objects = NULL;
	project->loading = TRUE;

	/* keep a comment */
	if (interface->comment)
		project->comment = g_strdup (interface->comment);
	
	for (i = 0; i < interface->n_toplevels; ++i)
	{
		widget = glade_widget_read ((gpointer)project, interface->toplevels[i]);
		if (!widget)
		{
			g_warning ("Failed to read a <widget> tag");
			continue;
		}
		glade_project_add_object (project, NULL, widget->object);
	}

	/* Reset project status here too so that you get a clean
	 * slate after calling glade_project_open().
	 */
	project->changed = FALSE;
	project->loading = FALSE;

	/* Emit "parse-finished" signal */
	g_signal_emit (project, glade_project_signals [PARSE_FINISHED], 0);
	
	return project;	
}

static void 
glade_project_fix_object_props (GladeProject *project)
{
	GList         *l, *ll;
	GValue        *value;
	GladeWidget   *gwidget;
	GladeProperty *property;
	gchar         *txt;

	for (l = project->objects; l; l = l->next)
	{
		gwidget = glade_widget_get_from_gobject (l->data);

		for (ll = gwidget->properties; ll; ll = ll->next)
		{
			property = GLADE_PROPERTY (ll->data);

			if (glade_property_class_is_object (property->class) &&
			    (txt = g_object_get_data (G_OBJECT (property), 
						      "glade-loaded-object")) != NULL)
			{
				/* Parse the object list and set the property to it
				 * (this magicly works for both objects & object lists)
				 */
				value = glade_property_class_make_gvalue_from_string
					(property->class, txt, project);
				
				glade_property_set_value (property, value);
				
				g_value_unset (value);
				g_free (value);
				
				g_object_set_data (G_OBJECT (property), 
						   "glade-loaded-object", NULL);
			}
		}
	}
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
	GladeProject *project = NULL;
	GladeInterface *interface;
	
	g_return_val_if_fail (path != NULL, NULL);

	if ((interface = 
	     glade_parser_interface_new_from_file (path, NULL)) != NULL)
	{
		if ((project = 
		     glade_project_new_from_interface (interface, 
						       path)) != NULL)
		{
			/* Now we have to loop over all the object properties
			 * and fix'em all ('cause they probably weren't found)
			 */
			glade_project_fix_object_props (project);
		}
		
		glade_parser_interface_destroy (interface);
	
	}

	if (project)
	{
		if (glade_util_file_is_writeable (project->path) == FALSE)
			glade_project_set_readonly (project, TRUE);

		project->changed = FALSE;
	}

	return project;
}

static void
glade_project_move_resources (GladeProject *project,
			      const gchar  *old_dir,
			      const gchar  *new_dir)
{
	GList *list, *resources;
	gchar *old_name, *new_name;

	if (old_dir == NULL || /* <-- Cant help you :( */
	    new_dir == NULL)   /* <-- Unlikely         */
		return;

	if ((resources = /* Nothing to do here */
	     glade_project_list_resources (project)) == NULL)
		return;
	
	for (list = resources; list; list = list->next)
	{
		old_name = g_build_filename 
			(old_dir, (gchar *)list->data, NULL);
		new_name = g_build_filename 
			(new_dir, (gchar *)list->data, NULL);
		glade_util_copy_file (old_name, new_name);
		g_free (old_name);
		g_free (new_name);
	}
	g_list_free (resources);
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
	gchar          *canonical_path;

	g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);

	interface = glade_project_write (project);
	if (!interface)
	{
		g_warning ("Could not write glade document\n");
		return FALSE;
	}

	ret = glade_parser_interface_dump (interface, path, error);
	glade_parser_interface_destroy (interface);

	canonical_path = glade_util_canonical_path (path);
	g_assert (canonical_path);

	if (project->path == NULL ||
	    strcmp (canonical_path, project->path))
        {
		gchar *old_dir, *new_dir;

		if (project->path)
		{
			old_dir = g_path_get_dirname (project->path);
			new_dir = g_path_get_dirname (canonical_path);

			glade_project_move_resources (project, 
						      old_dir, 
						      new_dir);
			g_free (old_dir);
			g_free (new_dir);
		}
		project->path = (g_free (project->path),
				 g_strdup (canonical_path));

		project->name = (g_free (project->name),
				 g_path_get_basename (project->path));

	}

	glade_project_set_readonly (project, 
				    !glade_util_file_is_writeable (project->path));

	if (project->changed)
	{
		project->changed = FALSE;
		g_object_notify (G_OBJECT (project), "has-unsaved-changes");
	}

	if (project->untitled_number > 0)
	{
		glade_project_release_untitled_number (project->untitled_number);
		project->untitled_number = 0;
        }

	g_free (canonical_path);

	return ret;
}


/**
 * glade_project_undo:
 * @project: a #GladeProject
 * 
 * Undoes a #GladeCommand in this project.
 */
void
glade_project_undo (GladeProject *project)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	GLADE_PROJECT_GET_CLASS (project)->undo (project);
}

/**
 * glade_project_undo:
 * @project: a #GladeProject
 * 
 * Redoes a #GladeCommand in this project.
 */
void
glade_project_redo (GladeProject *project)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	GLADE_PROJECT_GET_CLASS (project)->redo (project);
}

/**
 * glade_project_next_undo_item:
 * @project: a #GladeProject
 * 
 * Gets the next undo item on @project's command stack.
 *
 * Returns: the #GladeCommand
 */
GladeCommand *
glade_project_next_undo_item (GladeProject *project)
{
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);
	return GLADE_PROJECT_GET_CLASS (project)->next_undo_item (project);
}


/**
 * glade_project_next_redo_item:
 * @project: a #GladeProject
 * 
 * Gets the next redo item on @project's command stack.
 *
 * Returns: the #GladeCommand
 */
GladeCommand *
glade_project_next_redo_item (GladeProject *project)
{
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);
	return GLADE_PROJECT_GET_CLASS (project)->next_redo_item (project);
}



/**
 * glade_project_push_undo:
 * @project: a #GladeProject
 * @cmd: the #GladeCommand
 * 
 * Pushes a newly created #GladeCommand onto @projects stack.
 */
void
glade_project_push_undo (GladeProject *project, GladeCommand *cmd)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GLADE_IS_COMMAND (cmd));

	GLADE_PROJECT_GET_CLASS (project)->push_undo (project, cmd);
}

void
glade_project_reset_path (GladeProject *project)
{
	g_return_if_fail (GLADE_IS_PROJECT (project));
	project->path = (g_free (project->path), NULL);
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
 * glade_project_set_accel_group:
 * @project: A #GladeProject
 * @accel_group: The @GtkAccelGroup
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
		{
			if (project->accel_group)
				gtk_window_remove_accel_group (GTK_WINDOW (objects->data), project->accel_group);
			
			gtk_window_add_accel_group (GTK_WINDOW (objects->data), accel_group);
		}

		objects = objects->next;
	}
	
	project->accel_group = accel_group;
}

/**
 * glade_project_resource_fullpath:
 * @project: The #GladeProject.
 * @resource: The resource basename
 *
 * Returns: A newly allocated string holding the 
 *          full path the the project resource.
 */
gchar *
glade_project_resource_fullpath (GladeProject *project,
				 const gchar  *resource)
{
	gchar *fullpath, *project_dir;

	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

	if (project->path == NULL) return g_strdup (resource);
	
	project_dir = g_path_get_dirname (project->path);
	fullpath    = g_build_filename (project_dir, resource, NULL);
	g_free (project_dir);

	return fullpath;
}


static gboolean
find_resource_by_resource (GladeProperty *key,
			   const gchar   *resource,
			   const gchar   *resource_cmp)
{
	g_assert (resource);
	g_assert (resource_cmp);
	return (!strcmp (resource, resource_cmp));
}



/**
 * glade_project_set_resource:
 * @project: A #GladeProject
 * @property: The #GladeProperty this resource is required by
 * @resource: The resource file basename to be found in the same
 *            directory as the glade file.
 *
 * Adds/Modifies/Removes a resource from a project; any project resources
 * will be copied when using "Save As...", when moving widgets across projects
 * and will be copied into the project's directory when selected.
 */
void
glade_project_set_resource (GladeProject  *project, 
			    GladeProperty *property,
			    const gchar   *resource)
{
	gchar *last_resource, *last_resource_dup = NULL, *base_resource = NULL;
	gchar *fullpath, *dirname;

	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GLADE_IS_PROPERTY (property));

	if ((last_resource = 
	     g_hash_table_lookup (project->resources, property)) != NULL)
		last_resource_dup = g_strdup (last_resource);

	/* Get dependable input */
	if (resource && resource[0] != '\0' && strcmp (resource, "."))
		base_resource = g_path_get_basename (resource);
	
	/* If the resource has been removed or the base name has changed
	 * then remove from hash and emit removed.
	 */
	if (last_resource_dup && 
	    (base_resource == NULL || strcmp (last_resource_dup, base_resource)))
	{
		g_hash_table_remove (project->resources, property);

		if (g_hash_table_find (project->resources,
				       (GHRFunc)find_resource_by_resource,
				       last_resource_dup) == NULL)
			g_signal_emit (G_OBJECT (project),
				       glade_project_signals [RESOURCE_REMOVED],
				       0, last_resource_dup);
	}
	
	/* Copy files when importing widgets with resources.
	 */
	if (project->path)
	{
		dirname = g_path_get_dirname (project->path);
		fullpath = g_build_filename (dirname, base_resource, NULL);
	
		if (resource && project->path && 
		    g_file_test (resource, G_FILE_TEST_IS_REGULAR) &&
		    strcmp (fullpath, resource))
		{
			/* FIXME: In the case of copy/pasting widgets
			 * across projects we should ask the user about
			 * copying any resources.
			 */
			glade_util_copy_file (resource, fullpath);
		}
		g_free (fullpath);
		g_free (dirname);
	}

	if (base_resource)
	{

		/* If the resource has been added or the base name has 
		 * changed then emit added.
		 */
		if ((last_resource_dup == NULL || 
		     strcmp (last_resource_dup, base_resource)) &&
		    g_hash_table_find (project->resources,
				       (GHRFunc)find_resource_by_resource,
				       base_resource) == NULL)
			g_signal_emit (G_OBJECT (project),
				       glade_project_signals [RESOURCE_ADDED],
				       0, base_resource);

		g_hash_table_insert (project->resources, property, base_resource);

	}
	g_free (last_resource_dup);
}

static void
list_resources_accum (GladeProperty  *key,
		      gchar          *value,
		      GList         **list)
{
	*list = g_list_prepend (*list, value);
}



/**
 * glade_project_list_resources:
 * @project: A #GladeProject
 *
 * Returns: A newly allocated #GList of file basenames
 *          of resources in this project, note that the
 *          strings are not allocated and are unsafe to
 *          use once the projects state changes.
 *          The returned list should be freed with g_list_free.
 */
GList *
glade_project_list_resources (GladeProject  *project)
{
	GList *list = NULL;
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

	g_hash_table_foreach (project->resources, 
			      (GHFunc)list_resources_accum, &list);
	return list;
}



/**
 * glade_project_display_name:
 * @project: A #GladeProject
 * @unsaved_changes: whether to prepend a '*' to names 
 *                   of projects with unsaved changes
 * @tab_aligned: whether to prepend a tab and align unsaved
 *               names using the tab (for the project menu)
 * @mnemonic: Pass %TRUE here if you are going to use this in
 *            a mnemonic label... this will escape the underscores nicely.
 *
 * Returns: A newly allocated string to uniquely 
 *          describe this open project.
 *       
 */
gchar *
glade_project_display_name (GladeProject *project, 
			    gboolean      unsaved_changes,
			    gboolean      tab_aligned,
			    gboolean      mnemonic)
{
	const gchar *prefix         =
		tab_aligned ? "\t" : "";
	const gchar *unsaved_prefix = unsaved_changes ? 
		(tab_aligned ? "     *\t" : "*") : prefix;

	gchar       *prefixed_name  = NULL;
	gchar       *final_name     = NULL;

	prefixed_name = 
		g_strdup_printf ("%s%s", 
				 project->changed ? unsaved_prefix : prefix,
				 project->name);

	if (project->instance > 0)
	{
		final_name = 
			g_strdup_printf ("%s <%d>", 
					 prefixed_name, project->instance);
		g_free (prefixed_name);
	}
	else
		final_name = prefixed_name;

	if (mnemonic)
	{
		prefixed_name = glade_util_duplicate_underscores (final_name);
		g_free (final_name);
		return prefixed_name;
	}
	
	return final_name;
}

/**
 * glade_project_is_loading:
 * @project: A #GladeProject
 *
 * Returns: Whether the project is being loaded or not.
 *       
 */
gboolean
glade_project_is_loading (GladeProject *project)
{
	g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);
	
	return project->loading;
}
