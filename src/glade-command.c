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
#include "glade-command.h"
#include "glade-property.h"
#include "glade-debug.h"
#include "glade-placeholder.h"
#include "glade-clipboard.h"
#include "glade.h"

#define GLADE_COMMAND_TYPE		(glade_command_get_type ())
#define GLADE_COMMAND(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GLADE_COMMAND_TYPE, GladeCommand))
#define GLADE_COMMAND_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST ((k), GLADE_COMMAND_TYPE, GladeCommandClass))
#define IS_GLADE_COMMAND(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_COMMAND_TYPE))
#define IS_GLADE_COMMAND_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_COMMAND_TYPE))
#define CMD_CLASS(o)			GLADE_COMMAND_CLASS (G_OBJECT_GET_CLASS(o))

typedef struct {
	GObject parent;
	
	const gchar *description;
} GladeCommand;

typedef gboolean (* UndoCmd)(GladeCommand *this);
typedef gboolean (* Cmd)(GladeCommand *this);
typedef gboolean (* Unifies)(GladeCommand *this, GladeCommand *other);
typedef void (* Collapse)(GladeCommand *this, GladeCommand *other);

typedef struct {
	GObjectClass parent_class;
	
	UndoCmd undo_cmd;
	Cmd execute_cmd;

	Unifies unifies;
	Collapse collapse;
} GladeCommandClass;

static GObjectClass* parent_class = NULL;

#define MAKE_TYPE(func, type, parent)			\
guint							\
func ## _get_type (void)				\
{							\
	static guint command_type = 0;			\
							\
	if (!command_type)				\
	{						\
		static const GTypeInfo command_info =	\
		{					\
			sizeof (type ## Class),		\
			NULL,				\
			NULL,				\
			func ## _class_init,		\
			NULL,				\
			NULL,				\
			sizeof (type),			\
			0,				\
			NULL,				\
			NULL				\
		};					\
							\
		command_type = g_type_register_static (parent, #type,		\
						       &command_info, 0);	\
	}						\
							\
	return command_type;				\
}							\

static void
glade_command_finalize (GObject *obj)
{
        GladeCommand *cmd = (GladeCommand *) obj;
        g_return_if_fail (cmd != NULL);

        /* The const was to avoid accidental changes elsewhere */
        g_free ((gchar *) cmd->description);

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
glade_command_class_init (gpointer klass_p, gpointer notused)
{
	GladeCommandClass* klass = klass_p;
	GObjectClass* object_class;

	object_class = (GObjectClass*) klass;
	parent_class = (GObjectClass*) g_type_class_peek_parent (klass);
	
	object_class->finalize = glade_command_finalize;

	klass->undo_cmd = NULL;
	klass->execute_cmd = NULL;
	klass->unifies = glade_command_unifies;
	klass->collapse = glade_command_collapse;
}

static MAKE_TYPE(glade_command, GladeCommand, G_TYPE_OBJECT)

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
	object_class = (GObjectClass*) parent;				\
	parent->undo_cmd = (UndoCmd)& func ## _undo;			\
	parent->execute_cmd = (Cmd)& func ## _execute;			\
	parent->unifies = (Unifies)& func ## _unifies;			\
	parent->collapse = (Collapse)& func ## _collapse;		\
	object_class->finalize = func ## _finalize;			\
}									\
typedef struct {							\
	GladeCommandClass cmd;						\
} type ## Class;							\
static MAKE_TYPE(func, type, GLADE_COMMAND_TYPE)

/**************************************************/

static void
update_gui (void)
{
	glade_project_window_refresh_undo_redo (glade_project_window_get ());
}

void
glade_command_undo (void)
{
	GladeProject* project;
	GList* undo_stack;
	GList* prev_redo_item;
	GladeCommand* cmd;
	GladeCommandClass* class;
	
	project = glade_project_window_get_project ();
	g_assert (project != NULL);

	undo_stack = project->undo_stack;
	if (undo_stack == NULL)
		return;

	prev_redo_item = project->prev_redo_item;
	if (prev_redo_item == NULL)
		return;

	cmd = GLADE_COMMAND (prev_redo_item->data);
	class = CMD_CLASS (cmd);
	class->undo_cmd (cmd);

	project->prev_redo_item = prev_redo_item->prev;
	update_gui ();
}

void
glade_command_redo (void)
{
	GladeProject* project;
	GList* prev_redo_item;
	GladeCommand* cmd;
	GladeCommandClass* class;
	
	project = glade_project_window_get_project ();
	g_assert (project != NULL);

	if (project->undo_stack == NULL)
		return;

	prev_redo_item = project->prev_redo_item;
	if (prev_redo_item == NULL)
		cmd = GLADE_COMMAND (project->undo_stack->data);
	else {
		if (prev_redo_item->next == NULL)
			return;
		
		cmd = GLADE_COMMAND (g_list_next (prev_redo_item)->data);
	}

	g_assert (cmd != NULL);
	
	class = CMD_CLASS (cmd);
	class->execute_cmd (cmd);

	project->prev_redo_item = prev_redo_item ? prev_redo_item->next : project->undo_stack;
	update_gui ();
}

const gchar*
glade_command_get_description (GList *l)
{
	if (l && l->data)
		return GLADE_COMMAND (l->data)->description;
	else
		return NULL;
}

static void
glade_command_push_undo (GladeProject *project, GladeCommand *cmd)
{
	GList* tmp_redo_item;
	
	if (project == NULL) {
		return;
	}

	g_assert (cmd != NULL);

	/* If there are no "redo" items, and the last "undo" item unifies with
	   us, then we collapse the two items in one and we're done */
	if (project->prev_redo_item != NULL &&
	    project->prev_redo_item->next == NULL) {
		GladeCommand* cmd1 = project->prev_redo_item->data;
		GladeCommandClass* klass = CMD_CLASS(cmd1);
		
		if ((* klass->unifies) (cmd1, cmd)) {
			g_debug(("Command unifies.\n"));
			(* klass->collapse) (cmd1, cmd);
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

	if (project->prev_redo_item) {
		g_list_free (g_list_next (project->prev_redo_item));
		project->prev_redo_item->next = NULL;
	}
	else {
		g_list_free (project->undo_stack);
		project->undo_stack = NULL;
	}

	/* and then push the new undo item */
	project->undo_stack = g_list_append (project->undo_stack, cmd);

	if (project->prev_redo_item == NULL)
		project->prev_redo_item = project->undo_stack;
	else
		project->prev_redo_item = g_list_next (project->prev_redo_item);

	update_gui ();
}

/**************************************************/
/*******     GLADE_COMMAND_SET_PROPERTY     *******/
/**************************************************/

/* create a new GladeCommandSetProperty class.  Objects of this class will
 * encapsulate a "set property" operation */
typedef struct {
	GladeCommand parent;

	GObject *obj;
	const gchar *arg_name;
	GValue *arg_value;
} GladeCommandSetProperty;

/* standard macros */
GLADE_MAKE_COMMAND (GladeCommandSetProperty, glade_command_set_property);
#define GLADE_COMMAND_SET_PROPERTY_TYPE		(glade_command_set_property_get_type ())
#define GLADE_COMMAND_SET_PROPERTY(o)	  	(G_TYPE_CHECK_INSTANCE_CAST ((o), GLADE_COMMAND_SET_PROPERTY_TYPE, GladeCommandSetProperty))
#define GLADE_COMMAND_SET_PROPERTY_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), GLADE_COMMAND_SET_PROPERTY_TYPE, GladeCommandSetPropertyClass))
#define IS_GLADE_COMMAND_SET_PROPERTY(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_COMMAND_SET_PROPERTY_TYPE))
#define IS_GLADE_COMMAND_SET_PROPERTY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_COMMAND_SET_PROPERTY_TYPE))

/* Undo the last "set property command" */
static gboolean
glade_command_set_property_undo (GladeCommand *cmd)
{
	return glade_command_set_property_execute (cmd);
}

/**
 * Execute the set property command and revert it.  Ie, after the execution of this
 * function cmd will point to the undo action
 */
static gboolean
glade_command_set_property_execute (GladeCommand *cmd)
{
	GladeProperty* property;
	GladeWidget* gwidget;
	GladeCommandSetProperty* me = (GladeCommandSetProperty*) cmd;
	GValue* new_value;

	g_return_val_if_fail (me != NULL, TRUE);

	new_value = g_new0 (GValue, 1);
	g_value_init (new_value, G_VALUE_TYPE (me->arg_value));
	g_value_copy (me->arg_value, new_value);

	/* TODO: Change the "GObject *obj" of GladeCommandSetProperty to "GtkWidget *widget" */
	gwidget = glade_widget_get_from_gtk_widget (GTK_WIDGET (me->obj));
	property = glade_property_get_from_id (gwidget->properties, me->arg_name);

	g_value_copy (property->value, me->arg_value);
	glade_property_set (property, new_value);
	
	return FALSE;
}

/* finalize the set property object.  This object doesn't allocates */
static void
glade_command_set_property_finalize (GObject *obj)
{
	glade_command_finalize (obj);
}

static gboolean
glade_command_set_property_unifies (GladeCommand *this, GladeCommand *other)
{
	GladeCommandSetProperty *cmd1;
	GladeCommandSetProperty *cmd2;

	if (IS_GLADE_COMMAND_SET_PROPERTY (this) && IS_GLADE_COMMAND_SET_PROPERTY (other)) {
		cmd1 = (GladeCommandSetProperty*) this;
		cmd2 = (GladeCommandSetProperty*) other;

		return (cmd1->obj == cmd2->obj && strcmp (cmd1->arg_name, cmd2->arg_name) == 0);
	}

	return FALSE;
}

static void
glade_command_set_property_collapse (GladeCommand *this, GladeCommand *other)
{
	g_return_if_fail (IS_GLADE_COMMAND_SET_PROPERTY (this) && IS_GLADE_COMMAND_SET_PROPERTY (other));
	g_free ((gchar*) this->description);
	this->description = other->description;
	other->description = NULL;

	update_gui ();
}

static gchar*
gvalue_to_string (const GValue* pvalue)
{
	gchar* retval;
	
	switch (G_VALUE_TYPE (pvalue)) {
	case G_TYPE_UINT:
		/* TODO: What if this UINT doesn't represents a unichar?? */
		retval = g_new0 (gchar, 7);
		g_unichar_to_utf8 (g_value_get_uint (pvalue), retval);
		*g_utf8_next_char(retval) = '\0';
		break;
	case G_TYPE_INT:
		retval = g_strdup_printf ("%d", g_value_get_int (pvalue));
		break;
	case G_TYPE_FLOAT:
		retval = g_strdup_printf ("%f", g_value_get_float (pvalue));
		break;
	case G_TYPE_DOUBLE:
		retval = g_strdup_printf ("%lf", g_value_get_double (pvalue));
		break;
	case G_TYPE_BOOLEAN:
		retval = g_strdup_printf ("%s", g_value_get_boolean (pvalue) ? _("true") : _("false"));
		break;
	case G_TYPE_ENUM:
		retval = g_strdup_printf ("%d", g_value_get_enum (pvalue));
		break;
	case G_TYPE_STRING:
		retval = g_strdup_printf ("%s", g_value_get_string (pvalue));
		break;
	default:
		retval = g_strdup ("FIXME!");
	}

	return retval;
}

void
glade_command_set_property (GObject *obj, const gchar* name, const GValue* pvalue)
{
	GladeCommandSetProperty *me;
	GladeCommand *cmd;
	GladeProject *project;
	GladeWidget *gwidget;
	gchar *value_name;
	
	me = (GladeCommandSetProperty*) g_object_new (GLADE_COMMAND_SET_PROPERTY_TYPE, NULL);
	cmd = (GladeCommand*) me;
	
	project = glade_project_window_get_project ();
	gwidget = glade_widget_get_from_gtk_widget (GTK_WIDGET (obj));
	g_assert (gwidget);
	
	me->obj = obj;
	me->arg_name = name;
	me->arg_value = g_new0 (GValue, 1);
	g_value_init (me->arg_value, G_VALUE_TYPE (pvalue));
	g_value_copy (pvalue, me->arg_value);

	value_name = gvalue_to_string(pvalue);
	cmd->description = g_strdup_printf (_("Setting %s of %s to %s"), name, gwidget->name, value_name);
	g_assert (cmd->description);
	g_free (value_name);
	
	g_debug(("Pushing: %s\n", cmd->description));

	glade_command_set_property_execute (GLADE_COMMAND (me));
	glade_command_push_undo(project, GLADE_COMMAND (me));
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
#define IS_GLADE_COMMAND_SET_NAME(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_COMMAND_SET_NAME_TYPE))
#define IS_GLADE_COMMAND_SET_NAME_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_COMMAND_SET_NAME_TYPE))

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
	GladeCommandSetName* me = (GladeCommandSetName*) cmd;
	char* tmp;

	g_return_val_if_fail (me != NULL, TRUE);
	g_return_val_if_fail (me->widget != NULL, TRUE);
	g_return_val_if_fail (me->name != NULL, TRUE);

	glade_widget_set_name (me->widget, me->name);
	tmp = me->old_name;
	me->old_name = me->name;
	me->name = tmp;

	return FALSE;
}

/* finalize the set property object. */
static void
glade_command_set_name_finalize (GObject *obj)
{
	GladeCommandSetName* me = GLADE_COMMAND_SET_NAME (obj);
	g_return_if_fail (me != NULL);
	g_free (me->old_name);
	g_free (me->name);
	glade_command_finalize (obj);
}

static gboolean
glade_command_set_name_unifies (GladeCommand *this, GladeCommand *other)
{
	GladeCommandSetName *cmd1;
	GladeCommandSetName *cmd2;

	if (IS_GLADE_COMMAND_SET_NAME (this) && IS_GLADE_COMMAND_SET_NAME (other)) {
		cmd1 = (GladeCommandSetName*) this;
		cmd2 = (GladeCommandSetName*) other;

		return (cmd1->widget == cmd2->widget);
	}

	return FALSE;
}

static void
glade_command_set_name_collapse (GladeCommand *this, GladeCommand *other)
{
	GladeCommandSetName *nthis = GLADE_COMMAND_SET_NAME (this);
	GladeCommandSetName *nother = GLADE_COMMAND_SET_NAME (other);

	g_return_if_fail (IS_GLADE_COMMAND_SET_NAME (this) && IS_GLADE_COMMAND_SET_NAME (other));

	g_free (nthis->old_name);
	nthis->old_name = nother->old_name;
	nother->old_name = NULL;

	g_free ((gchar *) this->description);
	this->description = g_strdup_printf (_("Renaming %s to %s"), nthis->name, nthis->old_name);

	update_gui ();
}

/* this function takes the ownership of name */
void
glade_command_set_name (GladeWidget *widget, const gchar* name)
{
	GladeCommandSetName *me;
	GladeCommand *cmd;
	GladeProject *project;
	
	me = (GladeCommandSetName*) g_object_new (GLADE_COMMAND_SET_NAME_TYPE, NULL);
	cmd = (GladeCommand*) me;
	
	project = glade_project_window_get_project ();
	
	me->widget = widget;
	me->name = g_strdup (name);
	me->old_name = g_strdup (widget->name);

	cmd->description = g_strdup_printf (_("Renaming %s to %s"), me->old_name, me->name);
	if (!cmd->description)
		return;

	g_debug(("Pushing: %s\n", cmd->description));

	glade_command_set_name_execute (GLADE_COMMAND (me));
	glade_command_push_undo(project, GLADE_COMMAND (me));
}

/***************************************************
 * CREATE / DELETE
 * Don't be confused by the names:
 *   * "create" adds a GladeWidget to the project
 *   * "delete" removes a GladeWidget from the project
 **************************************************/

typedef struct {
	GladeCommand parent;

	GladeWidget *widget;
	GladePlaceholder *placeholder;
	gboolean create;
} GladeCommandCreateDelete;

GLADE_MAKE_COMMAND (GladeCommandCreateDelete, glade_command_create_delete);
#define GLADE_COMMAND_CREATE_DELETE_TYPE		(glade_command_create_delete_get_type ())
#define GLADE_COMMAND_CREATE_DELETE(o)	  	(G_TYPE_CHECK_INSTANCE_CAST ((o), GLADE_COMMAND_CREATE_DELETE_TYPE, GladeCommandCreateDelete))
#define GLADE_COMMAND_CREATE_DELETE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), GLADE_COMMAND_CREATE_DELETE_TYPE, GladeCommandCreateDeleteClass))
#define IS_GLADE_COMMAND_CREATE_DELETE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_COMMAND_CREATE_DELETE_TYPE))
#define IS_GLADE_COMMAND_CREATE_DELETE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_COMMAND_CREATE_DELETE_TYPE))

static gboolean
glade_command_create_delete_undo (GladeCommand *cmd)
{
	return glade_command_create_delete_execute (cmd);
}

static gboolean
glade_command_create_execute (GladeCommandCreateDelete *me)
{
	GladeWidget *widget = me->widget;
	GladeWidget *parent = widget->parent;
	
	if (me->placeholder) {
		parent = glade_placeholder_get_parent (me->placeholder);

		if (me->widget->parent->class->placeholder_replace)
			me->widget->parent->class->placeholder_replace (GTK_WIDGET (me->placeholder), widget->widget, widget->parent->widget);

		me->placeholder = NULL;
	}

	if (parent)
		parent->children = g_list_prepend (parent->children, widget);
	
	glade_project_selection_clear (widget->project, FALSE);
	glade_project_add_widget (widget->project, widget);

	if (GTK_IS_WIDGET (widget->widget)) {
		glade_project_selection_add (widget, TRUE);
		gtk_widget_show (widget->widget);
	}

	return FALSE;
}

static gboolean
glade_command_delete_execute (GladeCommandCreateDelete *me)
{
	GladeWidget *widget = me->widget;
	GladeWidget *parent;

	g_return_val_if_fail (widget != NULL, TRUE);

	glade_project_selection_remove (widget, TRUE);
	glade_project_remove_widget (widget);

	parent = widget->parent;

	if (parent) {
		gtk_widget_ref (widget->widget);
		me->placeholder = glade_widget_replace_with_placeholder (widget);
	} else {
		me->placeholder = NULL;
	}

	gtk_widget_hide (widget->widget);

	return FALSE;
}

/**
 * Execute the cmd and revert it.  Ie, after the execution of this
 * function cmd will point to the undo action
 */
static gboolean
glade_command_create_delete_execute (GladeCommand *cmd)
{
	GladeCommandCreateDelete* me = (GladeCommandCreateDelete*) cmd;
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
	GladeCommandCreateDelete *cmd = GLADE_COMMAND_CREATE_DELETE (obj);
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

static void
glade_command_create_delete_common (GladeWidget *widget, gboolean create)
{
	GladeCommandCreateDelete *me;
	GladeCommand *cmd;
	GladeProject *project;
	
	me = (GladeCommandCreateDelete*) g_object_new (GLADE_COMMAND_CREATE_DELETE_TYPE, NULL);
	cmd = (GladeCommand*) me;
	
	project = glade_project_window_get_project ();
	
	me->widget = widget;
	me->create = create;
	me->placeholder = NULL;
	cmd->description = g_strdup_printf (_("%s %s"), create ? "Create" : "Delete", widget->name);
	
	g_debug(("Pushing: %s\n", cmd->description));

	glade_command_create_delete_execute (GLADE_COMMAND (me));
	glade_command_push_undo(project, GLADE_COMMAND (me));
}

void
glade_command_delete (GladeWidget *widget)
{
	glade_command_create_delete_common (widget, FALSE);
}

void
glade_command_create (GladeWidget *widget)
{
	glade_command_create_delete_common (widget, TRUE);
}

/**
 * Cut/Paste
 *
 * Following is the code to extend the GladeCommand Undo/Redo system to 
 * Clipboard functions.
 **/
typedef struct {
	GladeCommand parent;

	GladeClipboard *clipboard;
	GladeWidget *widget;
	GladePlaceholder *placeholder;
	gboolean cut;
} GladeCommandCutPaste;


GLADE_MAKE_COMMAND (GladeCommandCutPaste, glade_command_cut_paste);
#define GLADE_COMMAND_CUT_PASTE_TYPE		(glade_command_cut_paste_get_type ())
#define GLADE_COMMAND_CUT_PASTE(o)	  	(G_TYPE_CHECK_INSTANCE_CAST ((o), GLADE_COMMAND_CUT_PASTE_TYPE, GladeCommandCutPaste))
#define GLADE_COMMAND_CUT_PASTE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), GLADE_COMMAND_CUT_PASTE_TYPE, GladeCommandCutPasteClass))
#define IS_GLADE_COMMAND_CUT_PASTE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_COMMAND_CUT_PASTE_TYPE))
#define IS_GLADE_COMMAND_CUT_PASTE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_COMMAND_CREATE_DELETE_TYPE))

static gboolean
glade_command_cut_paste_undo (GladeCommand *cmd)
{
	return glade_command_cut_paste_execute (cmd);
}

static gboolean
glade_command_paste_execute (GladeCommandCutPaste *me)
{
	glade_clipboard_paste (me->clipboard, me->placeholder);

	me->placeholder = NULL;

	return TRUE;
}

static gboolean
glade_command_cut_execute (GladeCommandCutPaste *me)
{
	me->placeholder = glade_clipboard_cut (me->clipboard, me->widget);

	return TRUE;
}

/**
 * Execute the cmd and revert it.  Ie, after the execution of this
 * function cmd will point to the undo action
 */
static gboolean
glade_command_cut_paste_execute (GladeCommand *cmd)
{
	GladeCommandCutPaste *me = (GladeCommandCutPaste *) cmd;
	gboolean retval;
	
	if (me->cut)
		retval = glade_command_cut_execute (me);
	else
		retval = glade_command_paste_execute (me);

	me->cut = !me->cut;
	return retval;
}

static void
glade_command_cut_paste_finalize (GObject *obj)
{
	GladeCommandCutPaste *cmd = GLADE_COMMAND_CUT_PASTE (obj);
	g_object_unref (cmd->widget);
        glade_command_finalize (obj);
}

static gboolean
glade_command_cut_paste_unifies (GladeCommand *this, GladeCommand *other)
{
	return FALSE;
}

static void
glade_command_cut_paste_collapse (GladeCommand *this, GladeCommand *other)
{
	g_return_if_reached ();
}

static void
glade_command_cut_paste_common (GladeWidget *widget,
				GladePlaceholder *placeholder,
				gboolean cut)
{
	GladeCommandCutPaste *me;
	GladeCommand *cmd;
	GladeProject *project;
	GladeProjectWindow *gpw;

	me = (GladeCommandCutPaste *) g_object_new (GLADE_COMMAND_CUT_PASTE_TYPE, NULL);
	cmd = (GladeCommand *) me;
	
	project = glade_project_window_get_project ();
	gpw = glade_project_window_get ();

	me->cut = cut;
	me->widget = widget;
	me->placeholder = placeholder;
	me->clipboard = gpw->clipboard;
	
	cmd->description = g_strdup_printf (_("%s %s"), cut ? "Cut" : "Paste", widget->name);
	
	g_debug(("Pushing: %s\n", cmd->description));

	/*
	 * Push it onto the undo stack only on success
	 */
	if (glade_command_cut_paste_execute (GLADE_COMMAND (me)))
		glade_command_push_undo (project, GLADE_COMMAND (me));
}

void
glade_command_paste (GladeWidget *widget, GladePlaceholder *placeholder)
{
	glade_command_cut_paste_common (widget, placeholder, FALSE);
}

void
glade_command_cut (GladeWidget *widget)
{
	glade_command_cut_paste_common (widget, NULL, TRUE);
}
