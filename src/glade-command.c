/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002 Joaquín Cuenca Abela
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
 *   Joaquín Cuenca Abela <e98cuenc@yahoo.com>
 *   Archit Baweja <bighead@users.sourceforge.net>
 */

#include <gtk/gtk.h>
#include <string.h>
#include "glade-types.h"
#include "glade-project.h"
#include "glade-project-window.h"
#include "glade-xml-utils.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-palette.h"
#include "glade-command.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-debug.h"
#include "glade-placeholder.h"
#include "glade-clipboard.h"
#include "glade-signal.h"
#include "glade.h"


static GObjectClass* parent_class = NULL;

#define MAKE_TYPE(func, type, parent)			\
GType							\
func ## _get_type (void)				\
{							\
	static GType cmd_type = 0;			\
							\
	if (!cmd_type)					\
	{						\
		static const GTypeInfo info =		\
		{					\
			sizeof (type ## Class),		\
			(GBaseInitFunc) NULL,		\
			(GBaseFinalizeFunc) NULL,	\
			(GClassInitFunc) func ## _class_init,	\
			(GClassFinalizeFunc) NULL,	\
			NULL,				\
			sizeof (type),			\
			0,				\
			(GInstanceInitFunc) NULL	\
		};					\
							\
		cmd_type = g_type_register_static (parent, #type, &info, 0);	\
	}						\
							\
	return cmd_type;				\
}							\

static void
glade_command_finalize (GObject *obj)
{
        GladeCommand *cmd = (GladeCommand *) obj;
        g_return_if_fail (cmd != NULL);

        g_free (cmd->description);

        /* Call the base class dtor */
	(* G_OBJECT_CLASS (parent_class)->finalize) (obj);
}

static gboolean
glade_command_unifies (GladeCommand *this, GladeCommand *other)
{
	return FALSE;
}

static void
glade_command_collapse (GladeCommand *this, GladeCommand *other)
{
	g_return_if_reached ();
}

static void
glade_command_class_init (GladeCommandClass *klass)
{
	GObjectClass* object_class;

	object_class = G_OBJECT_CLASS (klass);
	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = glade_command_finalize;

	klass->undo_cmd = NULL;
	klass->execute_cmd = NULL;
	klass->unifies = glade_command_unifies;
	klass->collapse = glade_command_collapse;
}

/* compose the _get_type function for GladeCommand */
MAKE_TYPE (glade_command, GladeCommand, G_TYPE_OBJECT)


#define GLADE_MAKE_COMMAND(type, func)					\
static gboolean								\
func ## _undo (GladeCommand *me);					\
static gboolean								\
func ## _execute (GladeCommand *me);					\
static void								\
func ## _finalize (GObject *object);					\
static gboolean								\
func ## _unifies (GladeCommand *this, GladeCommand *other);		\
static void								\
func ## _collapse (GladeCommand *this, GladeCommand *other);		\
static void								\
func ## _class_init (gpointer parent_tmp, gpointer notused)		\
{									\
	GladeCommandClass *parent = parent_tmp;				\
	GObjectClass* object_class;					\
	object_class = G_OBJECT_CLASS (parent);				\
	parent->undo_cmd =  func ## _undo;				\
	parent->execute_cmd =  func ## _execute;			\
	parent->unifies =  func ## _unifies;				\
	parent->collapse =  func ## _collapse;				\
	object_class->finalize = func ## _finalize;			\
}									\
typedef struct {							\
	GladeCommandClass cmd;						\
} type ## Class;							\
static MAKE_TYPE(func, type, GLADE_TYPE_COMMAND)

/**************************************************/

/**
 * glade_command_undo:
 * @project: a #GladeProject
 *
 * Undoes the last command performed in @project.
 */
void
glade_command_undo (GladeProject *project)
{
	GList* undo_stack;
	GList* prev_redo_item;
	GladeCommand* cmd;
	GladeCommandClass* class;

	g_return_if_fail (GLADE_IS_PROJECT (project));

	undo_stack = project->undo_stack;
	if (undo_stack == NULL)
		return;

	prev_redo_item = project->prev_redo_item;
	if (prev_redo_item == NULL)
		return;

	cmd = GLADE_COMMAND (prev_redo_item->data);
	class = GLADE_COMMAND_GET_CLASS (cmd);
	class->undo_cmd (cmd);

	project->prev_redo_item = prev_redo_item->prev;
}

/**
 * glade_command_redo:
 * @project: a #GladeProject
 *
 * Redoes the last undone command in @project.
 */
void
glade_command_redo (GladeProject *project)
{
	GList* prev_redo_item;
	GladeCommand* cmd;
	GladeCommandClass* class;
	
	g_return_if_fail (GLADE_IS_PROJECT (project));

	if (project->undo_stack == NULL)
		return;

	prev_redo_item = project->prev_redo_item;
	if (prev_redo_item == NULL)
	{
		cmd = GLADE_COMMAND (project->undo_stack->data);
	}
	else
	{
		if (prev_redo_item->next == NULL)
			return;
		
		cmd = GLADE_COMMAND (g_list_next (prev_redo_item)->data);
	}

	g_assert (cmd != NULL);

	class = GLADE_COMMAND_GET_CLASS (cmd);
	class->execute_cmd (cmd);

	project->prev_redo_item = prev_redo_item ? prev_redo_item->next : project->undo_stack;
}

static void
glade_command_push_undo (GladeProject *project, GladeCommand *cmd)
{
	GList* tmp_redo_item;

	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (GLADE_IS_COMMAND (cmd));

	/* If there are no "redo" items, and the last "undo" item unifies with
	   us, then we collapse the two items in one and we're done */
	if (project->prev_redo_item != NULL && project->prev_redo_item->next == NULL)
	{
		GladeCommand* cmd1 = project->prev_redo_item->data;
		GladeCommandClass* klass = GLADE_COMMAND_GET_CLASS (cmd1);
		
		if (klass->unifies (cmd1, cmd))
		{
			klass->collapse (cmd1, cmd);
			g_object_unref (cmd);
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

	glade_project_window_refresh_undo_redo ();
}

/**************************************************/
/*******     GLADE_COMMAND_SET_PROPERTY     *******/
/**************************************************/

/* create a new GladeCommandSetProperty class.  Objects of this class will
 * encapsulate a "set property" operation */
typedef struct {
	GladeCommand parent;

	GladeProperty *property;
	GValue *arg_value;
} GladeCommandSetProperty;

/* standard macros */
GLADE_MAKE_COMMAND (GladeCommandSetProperty, glade_command_set_property);
#define GLADE_COMMAND_SET_PROPERTY_TYPE		(glade_command_set_property_get_type ())
#define GLADE_COMMAND_SET_PROPERTY(o)	  	(G_TYPE_CHECK_INSTANCE_CAST ((o), GLADE_COMMAND_SET_PROPERTY_TYPE, GladeCommandSetProperty))
#define GLADE_COMMAND_SET_PROPERTY_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), GLADE_COMMAND_SET_PROPERTY_TYPE, GladeCommandSetPropertyClass))
#define GLADE_IS_COMMAND_SET_PROPERTY(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_COMMAND_SET_PROPERTY_TYPE))
#define GLADE_IS_COMMAND_SET_PROPERTY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_COMMAND_SET_PROPERTY_TYPE))

/* Undo the last "set property command" */
static gboolean
glade_command_set_property_undo (GladeCommand *cmd)
{
	return glade_command_set_property_execute (cmd);
}

/**
 * Execute the set property command and revert it. IE, after the execution of 
 * this function cmd will point to the undo action
 */
static gboolean
glade_command_set_property_execute (GladeCommand *cmd)
{
	GladeCommandSetProperty* me = (GladeCommandSetProperty*) cmd;
	GValue new_value = { 0, };

	g_return_val_if_fail (me != NULL, FALSE);

	g_value_init (&new_value, G_VALUE_TYPE (me->arg_value));
	g_value_copy (me->arg_value, &new_value);

	/* store the current value for undo */
	g_value_copy (me->property->value, me->arg_value);

	glade_property_set (me->property, &new_value);

	g_value_unset (&new_value);

	return TRUE;
}

static void
glade_command_set_property_finalize (GObject *obj)
{
	GladeCommandSetProperty *me;

	g_return_if_fail (GLADE_IS_COMMAND_SET_PROPERTY (obj));

	me = GLADE_COMMAND_SET_PROPERTY (obj);

	if (G_VALUE_TYPE (me->arg_value) != 0)
		g_value_unset (me->arg_value);
	g_free (me->arg_value);

	glade_command_finalize (obj);
}

static gboolean
glade_command_set_property_unifies (GladeCommand *this, GladeCommand *other)
{
	GladeCommandSetProperty *cmd1;
	GladeCommandSetProperty *cmd2;

	if (GLADE_IS_COMMAND_SET_PROPERTY (this) && GLADE_IS_COMMAND_SET_PROPERTY (other))
	{
		cmd1 = (GladeCommandSetProperty*) this;
		cmd2 = (GladeCommandSetProperty*) other;

		return (cmd1->property == cmd2->property);
	}

	return FALSE;
}

static void
glade_command_set_property_collapse (GladeCommand *this, GladeCommand *other)
{
	g_return_if_fail (GLADE_IS_COMMAND_SET_PROPERTY (this) &&
			  GLADE_IS_COMMAND_SET_PROPERTY (other));

	g_free (this->description);
	this->description = other->description;
	other->description = NULL;

	glade_project_window_refresh_undo_redo ();
}


#define MAX_UNDO_MENU_ITEM_VALUE_LEN	10

void
glade_command_set_property (GladeProperty *property, const GValue* pvalue)
{
	GladeCommandSetProperty *me;
	GladeCommand *cmd;
	GladeWidget *gwidget;
	gchar *value_name;

	g_return_if_fail (GLADE_IS_PROPERTY (property));
	g_return_if_fail (G_IS_VALUE (pvalue));

	me = (GladeCommandSetProperty*) g_object_new (GLADE_COMMAND_SET_PROPERTY_TYPE, NULL);
	cmd = (GladeCommand*) me;

	gwidget = property->widget;
	g_return_if_fail (GLADE_IS_WIDGET (gwidget));

	me->property = property;
	me->arg_value = g_new0 (GValue, 1);
	g_value_init (me->arg_value, G_VALUE_TYPE (pvalue));
	g_value_copy (pvalue, me->arg_value);

	value_name = glade_property_class_make_string_from_gvalue (property->class, pvalue);
	if (!value_name || strlen (value_name) > MAX_UNDO_MENU_ITEM_VALUE_LEN
	    || strchr (value_name, '_')) {
		cmd->description = g_strdup_printf (_("Setting %s of %s"),
						    property->class->name,
						    gwidget->name);

	} else {
		cmd->description = g_strdup_printf (_("Setting %s of %s to %s"),
						    property->class->name,
						    gwidget->name, value_name);
	}

	g_assert (cmd->description);
	g_free (value_name);

	/* Push onto undo stack only if it executes successfully. */
	if (glade_command_set_property_execute (GLADE_COMMAND (me)))
		glade_command_push_undo (gwidget->project, GLADE_COMMAND (me));
	else
		/* No leaks on my shift! */
		g_object_unref (G_OBJECT (me));
}

/**************************************************/
/*******       GLADE_COMMAND_SET_NAME       *******/
/**************************************************/

/* create a new GladeCommandSetName class.  Objects of this class will
 * encapsulate a "set name" operation */
typedef struct {
	GladeCommand parent;

	GladeWidget *widget;
	gchar *old_name;
	gchar *name;
} GladeCommandSetName;

/* standard macros */
GLADE_MAKE_COMMAND (GladeCommandSetName, glade_command_set_name);
#define GLADE_COMMAND_SET_NAME_TYPE		(glade_command_set_name_get_type ())
#define GLADE_COMMAND_SET_NAME(o)	  	(G_TYPE_CHECK_INSTANCE_CAST ((o), GLADE_COMMAND_SET_NAME_TYPE, GladeCommandSetName))
#define GLADE_COMMAND_SET_NAME_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), GLADE_COMMAND_SET_NAME_TYPE, GladeCommandSetNameClass))
#define GLADE_IS_COMMAND_SET_NAME(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_COMMAND_SET_NAME_TYPE))
#define GLADE_IS_COMMAND_SET_NAME_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_COMMAND_SET_NAME_TYPE))

/* Undo the last "set name command" */
static gboolean
glade_command_set_name_undo (GladeCommand *cmd)
{
	return glade_command_set_name_execute (cmd);
}

/**
 * Execute the set name command and revert it.  Ie, after the execution of this
 * function cmd will point to the undo action
 */
static gboolean
glade_command_set_name_execute (GladeCommand *cmd)
{
	GladeCommandSetName* me = GLADE_COMMAND_SET_NAME (cmd);
	char* tmp;

	g_return_val_if_fail (me != NULL, TRUE);
	g_return_val_if_fail (me->widget != NULL, TRUE);
	g_return_val_if_fail (me->name != NULL, TRUE);

	glade_widget_set_name (me->widget, me->name);
	
	tmp = me->old_name;
	me->old_name = me->name;
	me->name = tmp;

	return TRUE;
}

static void
glade_command_set_name_finalize (GObject *obj)
{
	GladeCommandSetName* me;

	g_return_if_fail (GLADE_IS_COMMAND_SET_NAME (obj));

	me = GLADE_COMMAND_SET_NAME (obj);

	g_free (me->old_name);
	g_free (me->name);

	glade_command_finalize (obj);
}

static gboolean
glade_command_set_name_unifies (GladeCommand *this, GladeCommand *other)
{
	GladeCommandSetName *cmd1;
	GladeCommandSetName *cmd2;

	if (GLADE_IS_COMMAND_SET_NAME (this) && GLADE_IS_COMMAND_SET_NAME (other))
	{
		cmd1 = GLADE_COMMAND_SET_NAME (this);
		cmd2 = GLADE_COMMAND_SET_NAME (other);

		return (cmd1->widget == cmd2->widget);
	}

	return FALSE;
}

static void
glade_command_set_name_collapse (GladeCommand *this, GladeCommand *other)
{
	GladeCommandSetName *nthis = GLADE_COMMAND_SET_NAME (this);
	GladeCommandSetName *nother = GLADE_COMMAND_SET_NAME (other);

	g_return_if_fail (GLADE_IS_COMMAND_SET_NAME (this) && GLADE_IS_COMMAND_SET_NAME (other));

	g_free (nthis->old_name);
	nthis->old_name = nother->old_name;
	nother->old_name = NULL;

	g_free (this->description);
	this->description = g_strdup_printf (_("Renaming %s to %s"), nthis->name, nthis->old_name);

	glade_project_window_refresh_undo_redo ();
}

/* this function takes the ownership of name */
void
glade_command_set_name (GladeWidget *widget, const gchar* name)
{
	GladeCommandSetName *me;
	GladeCommand *cmd;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (name != NULL);

	me = g_object_new (GLADE_COMMAND_SET_NAME_TYPE, NULL);
	cmd = GLADE_COMMAND (me);

	me->widget = widget;
	me->name = g_strdup (name);
	me->old_name = g_strdup (widget->name);

	cmd->description = g_strdup_printf (_("Renaming %s to %s"), me->old_name, me->name);
	if (!cmd->description)
		return;

	if (glade_command_set_name_execute (GLADE_COMMAND (me)))
		glade_command_push_undo (widget->project, GLADE_COMMAND (me));
	else
		g_object_unref (G_OBJECT (me));
}

/***************************************************
 * CREATE / DELETE
 **************************************************/

typedef struct {
	GladeCommand parent;

	GladeWidget      *parent_widget;
	GladeWidget      *widget;
	GladePlaceholder *placeholder;
	
	gboolean          create;
} GladeCommandCreateDelete;

GLADE_MAKE_COMMAND (GladeCommandCreateDelete, glade_command_create_delete);
#define GLADE_COMMAND_CREATE_DELETE_TYPE	(glade_command_create_delete_get_type ())
#define GLADE_COMMAND_CREATE_DELETE(o)	  	(G_TYPE_CHECK_INSTANCE_CAST ((o), GLADE_COMMAND_CREATE_DELETE_TYPE, GladeCommandCreateDelete))
#define GLADE_COMMAND_CREATE_DELETE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), GLADE_COMMAND_CREATE_DELETE_TYPE, GladeCommandCreateDeleteClass))
#define GLADE_IS_COMMAND_CREATE_DELETE(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_COMMAND_CREATE_DELETE_TYPE))
#define GLADE_IS_COMMAND_CREATE_DELETE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_COMMAND_CREATE_DELETE_TYPE))

static gboolean
glade_command_create_delete_undo (GladeCommand *cmd)
{
	return glade_command_create_delete_execute (cmd);
}

static gboolean
glade_command_create_execute (GladeCommandCreateDelete *me)
{
	GladeWidget      *widget      = me->widget;
	GladeWidget      *parent      = me->parent_widget;
	GladePlaceholder *placeholder = me->placeholder;

	if (parent)
	{
		if (placeholder)
			glade_widget_replace
				(parent, G_OBJECT (placeholder), widget->object);
		else
		{
			glade_widget_class_container_add (parent->widget_class,
							  parent->object,
							  widget->object);
			glade_widget_set_parent (widget, parent);
		}
	}
	
	glade_project_add_object    (widget->project, widget->object);
	glade_project_selection_set (widget->project, widget->object, TRUE);

	if (GTK_IS_WIDGET (widget->object))
		gtk_widget_show_all (GTK_WIDGET (widget->object));

	return TRUE;
}

static gboolean
glade_command_delete_execute (GladeCommandCreateDelete *me)
{
	GladeWidget *widget = me->widget;
	GladeWidget *parent = me->parent_widget;

	g_return_val_if_fail (widget != NULL, TRUE);

	parent = glade_widget_get_parent (widget);
	if (parent)
	{
		if (me->placeholder)
			glade_widget_replace
				(parent, widget->object, G_OBJECT (me->placeholder));
		else
			glade_widget_class_container_remove (parent->widget_class,
							     parent->object,
							     widget->object);
	}

	if (GTK_IS_WIDGET (widget->object))
		gtk_widget_hide (GTK_WIDGET (widget->object));

	glade_project_remove_object (widget->project, widget->object);

	return TRUE;
}

/**
 * Execute the cmd and revert it.  Ie, after the execution of this
 * function cmd will point to the undo action
 */
static gboolean
glade_command_create_delete_execute (GladeCommand *cmd)
{
	GladeCommandCreateDelete *me = (GladeCommandCreateDelete*) cmd;
	gboolean retval;

	if (me->create)
		retval = glade_command_create_execute (me);
	else
		retval = glade_command_delete_execute (me);

	me->create = !me->create;

	return retval;
}

static void
glade_command_create_delete_finalize (GObject *obj)
{
	GladeCommandCreateDelete *cmd;

	g_return_if_fail (GLADE_IS_COMMAND_CREATE_DELETE (obj));

	cmd = GLADE_COMMAND_CREATE_DELETE (obj);

	if (cmd->placeholder)
		g_object_unref (cmd->placeholder);

	g_object_unref (cmd->widget);
	
	glade_command_finalize (obj);
}

static gboolean
glade_command_create_delete_unifies (GladeCommand *this, GladeCommand *other)
{
	return FALSE;
}

static void
glade_command_create_delete_collapse (GladeCommand *this, GladeCommand *other)
{
	g_return_if_reached ();
}

/**
 * Placeholder arg may be NULL for toplevel widgets
 */
static void
glade_command_create_delete_common (GladeWidget      *widget,
				    GladeWidget      *parent,
				    GladePlaceholder *placeholder,
				    gboolean          create)
{
	GladeCommandCreateDelete *me;
	GladeCommand *cmd;

 	me = g_object_new (GLADE_COMMAND_CREATE_DELETE_TYPE, NULL);
 	cmd = GLADE_COMMAND (me);

 	me->widget        = g_object_ref(widget);
 	me->create        = create;
	me->placeholder   = placeholder;
	me->parent_widget = parent;
	cmd->description  =
		g_strdup_printf (_("%s %s"), create ?
				 "Create" : "Delete", widget->name);

	if (placeholder)
		g_object_ref (G_OBJECT (placeholder));

	if (glade_command_create_delete_execute (GLADE_COMMAND (me)))
		glade_command_push_undo (widget->project, GLADE_COMMAND (me));
	else
		g_object_unref (G_OBJECT (me));
}

/**
 * glade_command_delete:
 * @widget: a #GladeWidget
 *
 * TODO: write me
 */
void
glade_command_delete (GladeWidget *widget)
{
	g_return_if_fail (GLADE_IS_WIDGET (widget));

	/* internal children cannot be deleted. Notify the user. */
	if (widget->internal)
	{
		glade_util_ui_warn (glade_project_window_get()->window, _("You cannot delete a widget internal to a composite widget."));
		return;
	}

	glade_command_create_delete_common (widget, glade_widget_get_parent (widget),
					    NULL, FALSE);
}

/**
 * glade_command_create:
 * @class:		the class of the widget (GtkWindow or GtkButton)
 * @placeholder:	the placeholder which will be substituted by the widget
 * @project:            the project his widget belongs to.
 *
 * Creates a new widget of @class and put in place of the @placeholder
 * in the @project
 */
void
glade_command_create (GladeWidgetClass *class,
		      GladeWidget      *parent,
		      GladePlaceholder *placeholder,
		      GladeProject     *project)
{
	GladeProjectWindow *gpw = glade_project_window_get ();
	GladeWidget        *widget;

	g_return_if_fail (GLADE_IS_WIDGET_CLASS (class));

	/* Need either a parent or a placeholder or a project
	 */
	g_return_if_fail (parent      != NULL ||
			  placeholder != NULL ||
			  GLADE_IS_PROJECT (project));
	g_assert         (gpw != NULL);
	g_return_if_fail (GLADE_IS_PALETTE (gpw->palette));

	if (g_type_is_a (class->type, GTK_TYPE_WINDOW) == FALSE && !parent)
	{
		g_return_if_fail (placeholder != NULL);
		g_return_if_fail ((parent = glade_placeholder_get_parent (placeholder)));
		if (!project) project = parent->project;
	}
	else if (parent) project = parent->project;
	
	g_return_if_fail (project != NULL);

	widget = glade_widget_new (parent, class, project);

	/* widget may be null, e.g. the user clicked cancel on a query */
	if (widget == NULL)
		return;

	glade_command_create_delete_common (widget, parent, placeholder, TRUE);

}

typedef enum {
	GLADE_CUT,
	GLADE_COPY,
	GLADE_PASTE
} GladeCutCopyPasteType;

/**
 * Cut/Copy/Paste
 *
 * Following is the code to extend the GladeCommand Undo/Redo system to 
 * Clipboard functions.
 **/
typedef struct {
	GladeCommand parent;

	GladeWidget            *parent_widget;
	GladeWidget            *widget;
	GladePlaceholder       *placeholder;
	GladeCutCopyPasteType   type;
} GladeCommandCutCopyPaste;


GLADE_MAKE_COMMAND (GladeCommandCutCopyPaste, glade_command_cut_copy_paste);
#define GLADE_COMMAND_CUT_COPY_PASTE_TYPE		(glade_command_cut_copy_paste_get_type ())
#define GLADE_COMMAND_CUT_COPY_PASTE(o)	  	(G_TYPE_CHECK_INSTANCE_CAST ((o), GLADE_COMMAND_CUT_COPY_PASTE_TYPE, GladeCommandCutCopyPaste))
#define GLADE_COMMAND_CUT_COPY_PASTE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), GLADE_COMMAND_CUT_COPY_PASTE_TYPE, GladeCommandCutCopyPasteClass))
#define GLADE_IS_COMMAND_CUT_COPY_PASTE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_COMMAND_CUT_COPY_PASTE_TYPE))
#define GLADE_IS_COMMAND_CUT_COPY_PASTE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_COMMAND_CREATE_DELETE_TYPE))

static gboolean
glade_command_paste_execute (GladeCommandCutCopyPaste *me)
{
	GladeProjectWindow *gpw = glade_project_window_get ();

	g_return_val_if_fail (g_list_find (gpw->clipboard->widgets, me->widget), TRUE);

	if (me->parent_widget != NULL)
	{
		if (me->placeholder)
			glade_widget_replace
				(me->parent_widget,
				 G_OBJECT (me->placeholder),
				 me->widget->object);
		else
		{
			glade_widget_class_container_add
				(me->parent_widget->widget_class,
				 me->parent_widget->object,
				 me->widget->object);
			glade_widget_set_parent (me->widget, me->parent_widget);
		}
	}

	glade_project_add_object    (me->widget->project, me->widget->object);
	glade_project_selection_set (me->widget->project, me->widget->object, TRUE);

	if (GTK_IS_WIDGET (me->widget->object))
		gtk_widget_show_all (GTK_WIDGET (me->widget->object));

	glade_clipboard_remove (gpw->clipboard, me->widget);

	return TRUE;
}

static gboolean
glade_command_cut_execute (GladeCommandCutCopyPaste *me)
{
	GladeProjectWindow *gpw = glade_project_window_get ();

	g_return_val_if_fail (me->widget != NULL, TRUE);

	glade_clipboard_add (gpw->clipboard, me->widget);

	if (me->parent_widget)
	{
		if (me->placeholder)
			glade_widget_replace
				(me->parent_widget,
				 me->widget->object,
				 G_OBJECT (me->placeholder));
		else
			glade_widget_class_container_remove
				(me->parent_widget->widget_class,
				 me->parent_widget->object,
				 me->widget->object);
	}
	
	if (GTK_IS_WIDGET (me->widget->object))
		gtk_widget_hide (GTK_WIDGET (me->widget->object));

	/* This doesn't NULLify the widget->project pointer.
	 */
	glade_project_remove_object (me->widget->project, me->widget->object);

	return TRUE;
}

static gboolean
glade_command_copy_execute (GladeCommandCutCopyPaste *me)
{
	GladeProjectWindow *gpw = glade_project_window_get ();
	glade_clipboard_add (gpw->clipboard, me->widget);
	return TRUE;
}

static gboolean
glade_command_copy_undo (GladeCommandCutCopyPaste *me)
{
	GladeProjectWindow *gpw = glade_project_window_get ();
	glade_clipboard_remove (gpw->clipboard, me->widget);
	return TRUE;
}

/**
 * Execute the cmd and revert it.  Ie, after the execution of this
 * function cmd will point to the undo action
 */
static gboolean
glade_command_cut_copy_paste_execute (GladeCommand *cmd)
{
	GladeCommandCutCopyPaste *me = (GladeCommandCutCopyPaste *) cmd;
	gboolean retval = FALSE;

	switch (me->type)
	{
	case GLADE_CUT:
		retval = glade_command_cut_execute (me);
		break;
	case GLADE_COPY:
		retval = glade_command_copy_execute (me);
		break;
	case GLADE_PASTE:
		retval = glade_command_paste_execute (me);
		break;
	}

	return retval;
}

static gboolean
glade_command_cut_copy_paste_undo (GladeCommand *cmd)
{
	GladeCommandCutCopyPaste *me = (GladeCommandCutCopyPaste *) cmd;
	gboolean retval = FALSE;

	switch (me->type)
	{
	case GLADE_CUT:
		retval = glade_command_paste_execute (me);
		break;
	case GLADE_COPY:
		retval = glade_command_copy_undo (me);
		break;
	case GLADE_PASTE:
		retval = glade_command_cut_execute (me);
		break;
	}
	return retval;
}

static void
glade_command_cut_copy_paste_finalize (GObject *obj)
{
	GladeCommandCutCopyPaste *cmd;

	g_return_if_fail (GLADE_IS_COMMAND_CUT_COPY_PASTE (obj));

	cmd = GLADE_COMMAND_CUT_COPY_PASTE (obj);

	g_object_unref (cmd->widget);
	if (cmd->placeholder)
		g_object_unref (cmd->placeholder);

	glade_command_finalize (obj);
}

static gboolean
glade_command_cut_copy_paste_unifies (GladeCommand *this, GladeCommand *other)
{
	return FALSE;
}

static void
glade_command_cut_copy_paste_collapse (GladeCommand *this, GladeCommand *other)
{
	g_return_if_reached ();
}

static void
glade_command_cut_copy_paste_common (GladeWidget           *widget,
				     GladeWidget           *parent,
				     GladePlaceholder      *placeholder,
				     GladeProject          *project,
				     GladeCutCopyPasteType  type,
				     gboolean               needs_placeholder)
{
	GladeCommandCutCopyPaste *me;
	GladeCommand *cmd;

	me = (GladeCommandCutCopyPaste *) g_object_new (GLADE_COMMAND_CUT_COPY_PASTE_TYPE, NULL);
	cmd = (GladeCommand *) me;

	me->type           = type;
	me->widget         =
		(type == GLADE_COPY) ?
		glade_widget_dup (widget) :
		g_object_ref (widget);
	me->parent_widget  = parent;
	me->placeholder    = g_object_ref (placeholder);
	if (!me->placeholder && needs_placeholder)
		me->placeholder = GLADE_PLACEHOLDER (glade_placeholder_new ());

	switch (type) {
	case GLADE_CUT:
		cmd->description = g_strdup_printf (_("Cut %s"), widget->name);
		break;
	case GLADE_COPY:
		cmd->description = g_strdup_printf (_("Copy %s"), widget->name);
		break;
	case GLADE_PASTE:
		cmd->description = g_strdup_printf (_("Paste %s"), widget->name);
		break;
	}

	/*
	 * Push it onto the undo stack only on success
	 */
	if (glade_command_cut_copy_paste_execute (cmd))
		glade_command_push_undo (project, cmd);
	else
		g_object_unref (G_OBJECT (me));
}

/**
 * glade_command_paste:
 * @widget: a #GladeWidget
 *
 * TODO: write me
 */
void
glade_command_paste (GladeWidget *parent, GladePlaceholder *placeholder)
{
	GladeProjectWindow *gpw;
	GladeWidget *widget;

	gpw = glade_project_window_get ();

	if (!placeholder)
	{
		glade_util_ui_warn (gpw->window, _("Placeholder not selected!"));
		return;
	}

	g_return_if_fail (GLADE_IS_PLACEHOLDER (placeholder));

	widget = gpw->clipboard->curr;
	if (widget == NULL)
		return;

	glade_command_cut_copy_paste_common (widget, parent, placeholder,
					     parent->project, GLADE_PASTE, FALSE);
}

/**
 * glade_command_cut:
 * @widget: a #GladeWidget
 *
 * TODO: write me
 */
void
glade_command_cut (GladeWidget *widget, gboolean needs_placeholder)
{
	GladeProjectWindow *gpw;

	gpw = glade_project_window_get ();

	if (!widget)
	{
		glade_util_ui_warn (gpw->window, _("No widget selected!"));
		return;
	}

	g_return_if_fail (GLADE_IS_WIDGET (widget));

	/* internal children cannot be cut. Notify the user. */
	if (widget->internal)
	{
		glade_util_ui_warn (gpw->window, _("You cannot cut a widget internal to a composite widget."));
		return;
	}

	glade_command_cut_copy_paste_common (widget, glade_widget_get_parent (widget),
					     NULL, widget->project, GLADE_CUT,
					     needs_placeholder);
}

/**
 * glade_command_copy:
 * @widget: a #GladeWidget
 *
 * TODO: write me
 */
void
glade_command_copy (GladeWidget *widget)
{
	GladeProjectWindow *gpw;

	gpw = glade_project_window_get ();

	if (!widget)
	{
		glade_util_ui_warn (gpw->window, _("No widget selected!"));
		return;
	}

	g_return_if_fail (GLADE_IS_WIDGET (widget));

	/* internal children cannot be copied. Notify the user */
	if (widget->internal)
	{
		glade_util_ui_warn (gpw->window, _("You cannot copy a widget internal to a composite widget."));
		return;
	}

	glade_command_cut_copy_paste_common (widget, glade_widget_get_parent (widget),
					     NULL, widget->project, GLADE_COPY, FALSE);
}

/*********************************************************/
/*******     GLADE_COMMAND_ADD_SIGNAL     *******/
/*********************************************************/

/* create a new GladeCommandAddRemoveChangeSignal class.  Objects of this class will
 * encapsulate an "add or remove signal handler" operation */
typedef enum {
	GLADE_ADD,
	GLADE_REMOVE,
	GLADE_CHANGE
} GladeAddType;

typedef struct {
	GladeCommand   parent;

	GladeWidget   *widget;

	GladeSignal   *signal;
	GladeSignal   *new_signal;

	GladeAddType   type;
} GladeCommandAddSignal;

/* standard macros */
GLADE_MAKE_COMMAND (GladeCommandAddSignal, glade_command_add_signal);
#define GLADE_COMMAND_ADD_SIGNAL_TYPE		(glade_command_add_signal_get_type ())
#define GLADE_COMMAND_ADD_SIGNAL(o)	  	(G_TYPE_CHECK_INSTANCE_CAST ((o), GLADE_COMMAND_ADD_SIGNAL_TYPE, GladeCommandAddSignal))
#define GLADE_COMMAND_ADD_SIGNAL_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), GLADE_COMMAND_ADD_SIGNAL_TYPE, GladeCommandAddSignalClass))
#define GLADE_IS_COMMAND_ADD_SIGNAL(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_COMMAND_ADD_SIGNAL_TYPE))
#define GLADE_IS_COMMAND_ADD_SIGNAL_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_COMMAND_ADD_SIGNAL_TYPE))

static void
glade_command_add_signal_finalize (GObject *obj)
{
	GladeCommandAddSignal *cmd = GLADE_COMMAND_ADD_SIGNAL (obj);
	glade_signal_free (cmd->signal);

	g_object_unref (cmd->widget);

	if (cmd->signal)
		glade_signal_free (cmd->signal);
	if (cmd->new_signal)
		glade_signal_free (cmd->new_signal);

	glade_command_finalize (obj);
}

static gboolean
glade_command_add_signal_undo (GladeCommand *this)
{
	return glade_command_add_signal_execute (this);
}

static gboolean
glade_command_add_signal_execute (GladeCommand *this)
{
	GladeCommandAddSignal *cmd = GLADE_COMMAND_ADD_SIGNAL (this);
	GladeSignal           *temp;

	switch (cmd->type)
 	{
	case GLADE_ADD:
		glade_widget_add_signal_handler (cmd->widget, cmd->signal);
		cmd->type = GLADE_REMOVE;
		break;
	case GLADE_REMOVE:
		glade_widget_remove_signal_handler (cmd->widget, cmd->signal);
		cmd->type = GLADE_ADD;
		break;
	case GLADE_CHANGE:
		glade_widget_change_signal_handler (cmd->widget, 
						    cmd->signal, 
						    cmd->new_signal);
		temp            = cmd->signal;
		cmd->signal     = cmd->new_signal;
		cmd->new_signal = temp;
		break;
	default:
		break;
	}
	return TRUE;
}

static gboolean
glade_command_add_signal_unifies (GladeCommand *this, GladeCommand *other)
{
	return FALSE;
}

static void
glade_command_add_signal_collapse (GladeCommand *this, GladeCommand *other)
{
	g_return_if_reached ();
}

static void
glade_command_add_remove_change_signal (GladeWidget       *glade_widget,
					const GladeSignal *signal,
					const GladeSignal *new_signal,
					GladeAddType       type)
{
	GladeCommandAddSignal *me = GLADE_COMMAND_ADD_SIGNAL
		(g_object_new (GLADE_COMMAND_ADD_SIGNAL_TYPE, NULL));
	GladeCommand          *cmd = GLADE_COMMAND (me);

	/* we can only add/remove a signal to a widget that has been wrapped by a GladeWidget */
	g_assert (glade_widget != NULL);
	g_assert (glade_widget->project != NULL);

	me->widget       = g_object_ref(glade_widget);
	me->type         = type;
	me->signal       = glade_signal_clone (signal);
	me->new_signal   = new_signal ? 
		glade_signal_clone (new_signal) : NULL;
	
	cmd->description =
		g_strdup_printf (type == GLADE_ADD ? _("Add signal handler %s") :
				 type == GLADE_REMOVE ? _("Remove signal handler %s") :
				 _("Change signal handler %s"), 
				 signal->handler);
	
	if (glade_command_add_signal_execute (cmd))
		glade_command_push_undo (glade_widget->project, cmd);
	else
		g_object_unref (G_OBJECT (me));
}

/**
 * glade_command_add_signal:
 * @glade_widget: a #GladeWidget
 * @signal: a #GladeSignal
 *
 * TODO: write me
 */
void
glade_command_add_signal (GladeWidget *glade_widget, const GladeSignal *signal)
{
	glade_command_add_remove_change_signal
		(glade_widget, signal, NULL, GLADE_ADD);
}

/**
 * glade_command_remove_signal:
 * @glade_widget: a #GladeWidget
 * @signal: a #GladeSignal
 *
 * TODO: write me
 */
void
glade_command_remove_signal (GladeWidget *glade_widget, const GladeSignal *signal)
{
	glade_command_add_remove_change_signal
		(glade_widget, signal, NULL, GLADE_REMOVE);
}

/**
 * glade_command_change_signal:
 * @glade_widget: a #GladeWidget
 * @old: a #GladeSignal
 * @new: a #GladeSignal
 *
 * TODO: write me
 */
void
glade_command_change_signal	(GladeWidget *glade_widget, 
				 const GladeSignal *old, 
				 const GladeSignal *new)
{
	glade_command_add_remove_change_signal 
		(glade_widget, old, new, GLADE_CHANGE);
}
