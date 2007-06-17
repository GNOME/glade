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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Joaquín Cuenca Abela <e98cuenc@yahoo.com>
 *   Archit Baweja <bighead@users.sourceforge.net>
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include "glade.h"
#include "glade-project.h"
#include "glade-xml-utils.h"
#include "glade-widget.h"
#include "glade-palette.h"
#include "glade-command.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-debug.h"
#include "glade-placeholder.h"
#include "glade-clipboard.h"
#include "glade-signal.h"
#include "glade-app.h"
#include "glade-fixed.h"

/* Concerning placeholders: we do not hold any reference to placeholders,
 * placeholders that are supplied by the backend are not reffed, placeholders
 * that are created by glade-command are temporarily owned by glade-command
 * untill they are added to a container; in which case it belongs to GTK+
 * and the backend (but I prefer to think of it as the backend).
 */
typedef struct {
	GladeWidget      *widget;
	GladeWidget      *parent;
	GladeProject     *project;
	GladePlaceholder *placeholder;
	gboolean          props_recorded;
	GList            *pack_props;
	gchar            *special_type;
	gulong            handler_id;
} CommandData;

static GObjectClass *parent_class = NULL;

/* Group description used for the current group
 */
static gchar        *gc_group_description = NULL;

/* Use an id to avoid grouping together consecutive groups
 */
static gint          gc_group_id          = 1;

/* Groups can be grouped together, push/pop must balance (duh!)
 */
static gint          gc_group_depth       = 0;


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

	if (cmd->description)
		g_free (cmd->description);

        /* Call the base class dtor */
	(* G_OBJECT_CLASS (parent_class)->finalize) (obj);
}

static gboolean
glade_command_unifies_impl (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
	return FALSE;
}

static void
glade_command_collapse_impl (GladeCommand *this_cmd, GladeCommand *other_cmd)
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

	klass->undo            = NULL;
	klass->execute         = NULL;
	klass->unifies         = glade_command_unifies_impl;
	klass->collapse        = glade_command_collapse_impl;
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
func ## _unifies (GladeCommand *this_cmd, GladeCommand *other_cmd);		\
static void								\
func ## _collapse (GladeCommand *this_cmd, GladeCommand *other_cmd);		\
static void								\
func ## _class_init (gpointer parent_tmp, gpointer notused)		\
{									\
	GladeCommandClass *parent = parent_tmp;				\
	GObjectClass* object_class;					\
	object_class = G_OBJECT_CLASS (parent);				\
	parent->undo =  func ## _undo;					\
	parent->execute =  func ## _execute;				\
	parent->unifies =  func ## _unifies;				\
	parent->collapse =  func ## _collapse;				\
	object_class->finalize = func ## _finalize;			\
}									\
typedef struct {							\
	GladeCommandClass cmd;						\
} type ## Class;							\
static MAKE_TYPE(func, type, GLADE_TYPE_COMMAND)


/**
 * glade_command_execute:
 * @command: A #GladeCommand
 *
 * Executes @command
 *
 * Returns: whether the command was successfully executed
 */
gboolean
glade_command_execute (GladeCommand *command)
{
	g_return_val_if_fail (GLADE_IS_COMMAND (command), FALSE);
	return GLADE_COMMAND_GET_CLASS (command)->execute (command);
}


/**
 * glade_command_undo:
 * @command: A #GladeCommand
 *
 * Undo the effects of @command
 *
 * Returns whether the command was successfully reversed
 */
gboolean
glade_command_undo (GladeCommand *command)
{
	g_return_val_if_fail (GLADE_IS_COMMAND (command), FALSE);
	return GLADE_COMMAND_GET_CLASS (command)->undo (command);
}

/**
 * glade_command_unifies:
 * @command: A #GladeCommand
 * @other: another #GladeCommand
 *
 * Checks whether @command and @other can be unified
 * to make one single command.
 *
 * Returns: whether they can be unified.
 */
gboolean
glade_command_unifies (GladeCommand *command,
		       GladeCommand *other)
{
	g_return_val_if_fail (command, FALSE);
	return GLADE_COMMAND_GET_CLASS (command)->unifies (command, other);
}

/**
 * glade_command_collapse:
 * @command: A #GladeCommand
 * @other: another #GladeCommand
 *
 * Merges @other into @command, so that @command now
 * covers both commands and @other can be dispensed with.
 */
void
glade_command_collapse (GladeCommand  *command,
			GladeCommand  *other)
{
	g_return_if_fail (command);
	GLADE_COMMAND_GET_CLASS (command)->collapse (command, other);
}

/**
 * glade_command_push_group:
 * @fmt:         The collective desctiption of the command group.
 *               only the description of the first group on the 
 *               stack is used when embedding groups.
 * @...: args to the format string.
 *
 * Marks the begining of a group.
 */
void
glade_command_push_group (const gchar *fmt, ...)
{
	va_list         args;

	g_return_if_fail (fmt);

	/* Only use the description for the first group.
	 */
	if (gc_group_depth++ == 0)
	{
		va_start (args, fmt);
		gc_group_description = g_strdup_vprintf (fmt, args);
		va_end (args);
	}
}

/**
 * glade_command_pop_group:
 *
 * Mark the end of a command group.
 */
void
glade_command_pop_group (void)
{
	if (gc_group_depth-- == 1)
	{
		gc_group_description = (g_free (gc_group_description), NULL);
		gc_group_id++;
	}

	if (gc_group_depth < 0)
		g_critical ("Unbalanced group stack detected in %s\n",
			    G_GNUC_PRETTY_FUNCTION);
}

static void
glade_command_check_group (GladeCommand *cmd)
{
	g_return_if_fail (GLADE_IS_COMMAND (cmd));
	if (gc_group_description)
	{
		cmd->description = 
			(g_free (cmd->description), g_strdup (gc_group_description));
		cmd->group_id = gc_group_id;
	}
}

/**************************************************/
/*******     GLADE_COMMAND_SET_PROPERTY     *******/
/**************************************************/

/* create a new GladeCommandSetProperty class.  Objects of this class will
 * encapsulate a "set property" operation */

typedef struct {
	GladeCommand  parent;
	gboolean      set_once;
	gboolean      undo;
	GList        *sdata;
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

/*
 * Execute the set property command and revert it. IE, after the execution of 
 * this function cmd will point to the undo action
 */
static gboolean
glade_command_set_property_execute (GladeCommand *cmd)
{
	GladeCommandSetProperty* me = (GladeCommandSetProperty*) cmd;
	GList *l;

	g_return_val_if_fail (me != NULL, FALSE);

	if (me->set_once != FALSE)
		glade_property_push_superuser ();

	for (l = me->sdata; l; l = l->next)
	{
		GValue new_value = { 0, };
		GCSetPropData *sdata = l->data;

		g_value_init (&new_value, G_VALUE_TYPE (sdata->new_value));
		
		if (me->undo)
			g_value_copy (sdata->old_value, &new_value);
		else
			g_value_copy (sdata->new_value, &new_value);

#if 0
		{
			gchar *str =
				glade_property_class_make_string_from_gvalue 
				(sdata->property->klass, &new_value);

			g_print ("Setting %s property of %s to %s (sumode: %d)\n",
				 sdata->property->klass->id,
				 sdata->property->widget->name,
				 str, glade_property_superuser ());

			g_free (str);
		}
#endif

		/* Packing properties need to be refreshed here since
		 * they are reset when they get added to containers.
		 */
		if (sdata->property->klass->packing)
		{
			GladeProperty *tmp_prop;

			tmp_prop = glade_widget_get_pack_property
				(sdata->property->widget, sdata->property->klass->id);

			if (sdata->property != tmp_prop)
			{
				g_object_unref (sdata->property);
				sdata->property = g_object_ref (tmp_prop);
				
			}
		}

		glade_property_set_value (sdata->property, &new_value);

		if (!me->set_once)
		{
			/* If some verify functions didnt pass on 
			 * the first go.. we need to record the actual
			 * properties here.
			 */
			g_value_copy (sdata->property->value, 
				      sdata->new_value);
		}


		g_value_unset (&new_value);


	}

	if (me->set_once != FALSE)
		glade_property_pop_superuser ();

	me->set_once = TRUE;
	me->undo     = !me->undo;

	return TRUE;
}

static void
glade_command_set_property_finalize (GObject *obj)
{
	GladeCommandSetProperty *me;
	GList *l;

	me = GLADE_COMMAND_SET_PROPERTY (obj);

	for (l = me->sdata; l; l = l->next)
	{
		GCSetPropData *sdata = l->data;
		
		if (sdata->property)
			g_object_unref (G_OBJECT (sdata->property));

		if (sdata->old_value)
		{
			if (G_VALUE_TYPE (sdata->old_value) != 0)
				g_value_unset (sdata->old_value);
			g_free (sdata->old_value);
		}
		if (G_VALUE_TYPE (sdata->new_value) != 0)
			g_value_unset (sdata->new_value);
		g_free (sdata->new_value);

	}
	glade_command_finalize (obj);
}

static gboolean
glade_command_set_property_unifies (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
       GladeCommandSetProperty *cmd1,   *cmd2;
       GCSetPropData           *pdata1, *pdata2;
       GList                   *list, *l;

       if (GLADE_IS_COMMAND_SET_PROPERTY (this_cmd) && 
	   GLADE_IS_COMMAND_SET_PROPERTY (other_cmd))
       {
               cmd1 = (GladeCommandSetProperty*) this_cmd;
               cmd2 = (GladeCommandSetProperty*) other_cmd;

	       if (g_list_length (cmd1->sdata) != 
		   g_list_length (cmd2->sdata))
		       return FALSE;

	       for (list = cmd1->sdata; list; list = list->next)
	       {
		       pdata1 = list->data;
		       for (l = cmd2->sdata; l; l = l->next)
		       {
			       pdata2 = l->data;

			       if (pdata1->property->widget == pdata2->property->widget &&
				   glade_property_class_match (pdata1->property->klass,
							       pdata2->property->klass))
				       break;
		       }
		       
		       /* If both lists are the same length, and one class type
			* is not found in the other list, then we cant unify.
			*/
		       if (l == NULL)
			       return FALSE;

	       }

	       return TRUE;
       }
       return FALSE;
}

static void
glade_command_set_property_collapse (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
	GladeCommandSetProperty *cmd1,   *cmd2;
	GCSetPropData           *pdata1, *pdata2;
	GList                   *list, *l;
	
	g_return_if_fail (GLADE_IS_COMMAND_SET_PROPERTY (this_cmd) &&
			  GLADE_IS_COMMAND_SET_PROPERTY (other_cmd));
	
	cmd1 = (GladeCommandSetProperty*) this_cmd;
	cmd2 = (GladeCommandSetProperty*) other_cmd;


	for (list = cmd1->sdata; list; list = list->next)
	{
		pdata1 = list->data;
		for (l = cmd2->sdata; l; l = l->next)
		{
			pdata2 = l->data;
			
			if (glade_property_class_match (pdata1->property->klass,
							pdata2->property->klass))
			{
				/* Merge the GCSetPropData structs manually here
				 */
				g_value_copy (pdata2->new_value, pdata1->new_value);
				break;
			}
		}
	}

	/* Set the description
	 */
	g_free (this_cmd->description);
	this_cmd->description = other_cmd->description;
	other_cmd->description = NULL;

	glade_app_update_ui ();
}


#define MAX_UNDO_MENU_ITEM_VALUE_LEN	10
static gchar *
glade_command_set_property_description (GladeCommandSetProperty *me)
{
	GCSetPropData   *sdata;
	gchar *description = NULL;
	gchar *value_name;

	g_assert (me->sdata);

	if (g_list_length (me->sdata) > 1)
		description = g_strdup_printf (_("Setting multiple properties"));
	else 
	{
		sdata = me->sdata->data;
		value_name = glade_property_class_make_string_from_gvalue (sdata->property->klass, 
									   sdata->new_value);
		if (!value_name || strlen (value_name) > MAX_UNDO_MENU_ITEM_VALUE_LEN
		    || strchr (value_name, '_')) {
			description = g_strdup_printf (_("Setting %s of %s"),
						       sdata->property->klass->name,
						       sdata->property->widget->name);
		} else {
			description = g_strdup_printf (_("Setting %s of %s to %s"),
						       sdata->property->klass->name,
						       sdata->property->widget->name, value_name);
		}
		g_free (value_name);
	}

	return description;
}


void
glade_command_set_properties_list (GladeProject *project, GList *props)
{
	GladeCommandSetProperty *me;
	GladeCommand  *cmd;
	GCSetPropData *sdata;
	GList         *list;

	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (props);

	me = (GladeCommandSetProperty*) g_object_new (GLADE_COMMAND_SET_PROPERTY_TYPE, NULL);
	cmd = GLADE_COMMAND (me);

	/* Ref all props */
	for (list = props; list; list = list->next)
	{
		sdata = list->data;
		g_object_ref (G_OBJECT (sdata->property));
	}

	me->sdata        = props;
	cmd->description = glade_command_set_property_description (me);

	glade_command_check_group (GLADE_COMMAND (me));

	/* Push onto undo stack only if it executes successfully. */
	if (glade_command_set_property_execute (GLADE_COMMAND (me)))
		glade_project_push_undo (GLADE_PROJECT (project),
					 GLADE_COMMAND (me));
	else
		/* No leaks on my shift! */
		g_object_unref (G_OBJECT (me));
}


void
glade_command_set_properties (GladeProperty *property, const GValue *old_value, const GValue *new_value, ...)
{
	GCSetPropData *sdata;
	GladeProperty *prop;
	GValue        *ovalue, *nvalue;
	GList         *list = NULL;
	va_list        vl;

	g_return_if_fail (GLADE_IS_PROPERTY (property));

	/* Add first set */
	sdata = g_new (GCSetPropData, 1);
	sdata->property = property;
	sdata->old_value = g_new0 (GValue, 1);
	sdata->new_value = g_new0 (GValue, 1);
	g_value_init (sdata->old_value, G_VALUE_TYPE (old_value));
	g_value_init (sdata->new_value, G_VALUE_TYPE (new_value));
	g_value_copy (old_value, sdata->old_value);
	g_value_copy (new_value, sdata->new_value);
	list = g_list_prepend (list, sdata);

	va_start (vl, new_value);
	while ((prop = va_arg (vl, GladeProperty *)) != NULL)
	{
		ovalue = va_arg (vl, GValue *);
		nvalue = va_arg (vl, GValue *);
		
		g_assert (GLADE_IS_PROPERTY (prop));
		g_assert (G_IS_VALUE (ovalue));
		g_assert (G_IS_VALUE (nvalue));

		sdata = g_new (GCSetPropData, 1);
		sdata->property = g_object_ref (G_OBJECT (prop));
		sdata->old_value = g_new0 (GValue, 1);
		sdata->new_value = g_new0 (GValue, 1);
		g_value_init (sdata->old_value, G_VALUE_TYPE (ovalue));
		g_value_init (sdata->new_value, G_VALUE_TYPE (nvalue));
		g_value_copy (ovalue, sdata->old_value);
		g_value_copy (nvalue, sdata->new_value);
		list = g_list_prepend (list, sdata);
	}	
	va_end (vl);

	glade_command_set_properties_list (property->widget->project, list);
}

void
glade_command_set_property_value (GladeProperty *property, const GValue* pvalue)
{

	/* Dont generate undo/redo items for unchanging property values.
	 */
	if (glade_property_equals_value (property, pvalue))
		return;

	glade_command_set_properties (property, property->value, pvalue, NULL);
}

void
glade_command_set_property (GladeProperty *property, ...)
{
	GValue *value;
	va_list args;
	
	g_return_if_fail (GLADE_IS_PROPERTY (property));

	va_start (args, property);
	value = glade_property_class_make_gvalue_from_vl (property->klass, args);
	va_end (args);
	
	glade_command_set_property_value (property, value);
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

/*
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
glade_command_set_name_unifies (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
	GladeCommandSetName *cmd1;
	GladeCommandSetName *cmd2;

	if (GLADE_IS_COMMAND_SET_NAME (this_cmd) && GLADE_IS_COMMAND_SET_NAME (other_cmd))
	{
		cmd1 = GLADE_COMMAND_SET_NAME (this_cmd);
		cmd2 = GLADE_COMMAND_SET_NAME (other_cmd);

		return (cmd1->widget == cmd2->widget);
	}

	return FALSE;
}

static void
glade_command_set_name_collapse (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
	GladeCommandSetName *nthis = GLADE_COMMAND_SET_NAME (this_cmd);
	GladeCommandSetName *nother = GLADE_COMMAND_SET_NAME (other_cmd);

	g_return_if_fail (GLADE_IS_COMMAND_SET_NAME (this_cmd) && GLADE_IS_COMMAND_SET_NAME (other_cmd));

	g_free (nthis->old_name);
	nthis->old_name = nother->old_name;
	nother->old_name = NULL;

	g_free (this_cmd->description);
	this_cmd->description = g_strdup_printf (_("Renaming %s to %s"), nthis->name, nthis->old_name);

	glade_app_update_ui ();
}

/* this function takes the ownership of name */
void
glade_command_set_name (GladeWidget *widget, const gchar* name)
{
	GladeCommandSetName *me;
	GladeCommand *cmd;

	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (name != NULL);

	/* Dont spam the queue with false name changes.
	 */
	if (!strcmp (widget->name, name))
		return;

	me = g_object_new (GLADE_COMMAND_SET_NAME_TYPE, NULL);
	cmd = GLADE_COMMAND (me);

	me->widget = widget;
	me->name = g_strdup (name);
	me->old_name = g_strdup (widget->name);

	cmd->description = g_strdup_printf (_("Renaming %s to %s"), me->old_name, me->name);

	glade_command_check_group (GLADE_COMMAND (me));

	if (glade_command_set_name_execute (GLADE_COMMAND (me)))
		glade_project_push_undo (GLADE_PROJECT (widget->project), GLADE_COMMAND (me));
	else
		g_object_unref (G_OBJECT (me));
}

/******************************************************************************
 * 
 * add/remove
 * 
 * These canonical commands add/remove a widget list to/from the project.
 * 
 *****************************************************************************/

typedef struct {
	GladeCommand	parent;
	GladeProject	*project;
	GList		*widgets;
	gboolean	add;
	gboolean        from_clipboard;
} GladeCommandAddRemove;


GLADE_MAKE_COMMAND (GladeCommandAddRemove, glade_command_add_remove);
#define GLADE_COMMAND_ADD_REMOVE_TYPE			(glade_command_add_remove_get_type ())
#define GLADE_COMMAND_ADD_REMOVE(o)	  			(G_TYPE_CHECK_INSTANCE_CAST ((o), GLADE_COMMAND_ADD_REMOVE_TYPE, GladeCommandAddRemove))
#define GLADE_COMMAND_ADD_REMOVE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST ((k), GLADE_COMMAND_ADD_REMOVE_TYPE, GladeCommandAddRemoveClass))
#define GLADE_IS_COMMAND_ADD_REMOVE(o)			(G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_COMMAND_ADD_REMOVE_TYPE))
#define GLADE_IS_COMMAND_ADD_REMOVE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_COMMAND_ADD_REMOVE_TYPE))

static void 
glade_command_placeholder_destroyed (GtkObject *object, CommandData *cdata)
{
	if (GTK_OBJECT (cdata->placeholder) == object)
	{
		cdata->placeholder = NULL;
		cdata->handler_id = 0;
	}
}

static void
glade_command_placeholder_connect (CommandData *cdata,
				   GladePlaceholder *placeholder)
{
	g_assert (cdata && cdata->placeholder == NULL);

	/* Something like a no-op with no placeholder */
	if ((cdata->placeholder = placeholder) == NULL)
		return;

	cdata->handler_id = g_signal_connect 
		(placeholder, "destroy",
		 G_CALLBACK (glade_command_placeholder_destroyed), cdata);
}

/**
 * glade_command_add:
 * @widgets: a #Glist
 * @parent: a #GladeWidget
 * @placeholder: a #GladePlaceholder
 * @pasting: whether we are pasting an existing widget or creating a new one.
 *
 * Performs an add command on all widgets in @widgets to @parent, possibly
 * replacing @placeholder (note toplevels dont need a parent; the active project
 * will be used when pasting toplevel objects).
 * Pasted widgets will persist packing properties from thier cut/copy source
 * while newly added widgets will prefer packing defaults.
 *
 */
void
glade_command_add (GList            *widgets, 
		   GladeWidget      *parent, 
		   GladePlaceholder *placeholder,
		   gboolean          pasting)
{
	GladeCommandAddRemove	*me;
	CommandData				*cdata;
	GladeWidget				*widget = NULL;
	GList					*l, *list, *children, *placeholders = NULL;
	GtkWidget				*child;

	g_return_if_fail (widgets && widgets->data);
	g_return_if_fail (parent == NULL || GLADE_IS_WIDGET (parent));

	me = g_object_new (GLADE_COMMAND_ADD_REMOVE_TYPE, NULL);
	me->add = TRUE;
	me->from_clipboard = pasting;

	/* Things can go wrong in this function if the dataset is inacurate,
	 * we make no real attempt here to recover, just g_critical() and
	 * fix the bugs as they pop up.
	 */
	widget = GLADE_WIDGET (widgets->data);
	if (placeholder && GTK_IS_WINDOW (widget->object) == FALSE)
	{
		GladeWidget *some_widget = glade_placeholder_get_parent (placeholder);
		me->project = glade_widget_get_project (some_widget);
	}
	else me->project = glade_app_get_project();

	GLADE_COMMAND (me)->description = 
		g_strdup_printf (_("Add %s"), g_list_length (widgets) == 1 ? 
				 widget->name : _("multiple"));

	for (list = widgets; list && list->data; list = list->next)
	{
		widget = list->data;
		cdata = g_new0 (CommandData, 1);
		if (widget->internal)
			g_critical ("Internal widget in Add");

		/* Widget */
		cdata->widget = g_object_ref (G_OBJECT (widget));
		
		/* Parent */
		if (parent == NULL)
			cdata->parent = glade_widget_get_parent (widget);
		else if (placeholder && GTK_IS_WINDOW (widget->object) == FALSE)
			cdata->parent = glade_placeholder_get_parent (placeholder);
		else if (GTK_IS_WINDOW (widget->object) == FALSE)
			cdata->parent = parent;
		if (cdata->parent == NULL && GTK_IS_WINDOW (widget->object) == FALSE)
			g_critical ("Parentless non GtkWindow widget in Add");

		/* Placeholder */
		if (placeholder != NULL && g_list_length (widgets) == 1)
		{
			glade_command_placeholder_connect (cdata, placeholder);
		}
		else if (cdata->parent && 
			 glade_widget_placeholder_relation (cdata->parent, widget))
		{
			GtkContainer *cont = GTK_CONTAINER (cdata->parent->object);
			
			child = glade_util_get_placeholder_from_pointer (cont);
			if (child && g_list_find (placeholders, child) == NULL)
			{
				placeholders = g_list_append (placeholders, child);
				glade_command_placeholder_connect
						(cdata, GLADE_PLACEHOLDER (child));
			}
			else if ((children = glade_widget_adaptor_get_children
				 (cdata->parent->adaptor, cdata->parent->object)) != NULL)
			{
				for (l = children; l && l->data; l = l->next)
				{
					child = l->data;

					/* Find a placeholder for this child */
					if (GLADE_IS_PLACEHOLDER (child) &&
					    g_list_find (placeholders, child) == NULL)
					{
						placeholders = g_list_append (placeholders, child);
						glade_command_placeholder_connect
							(cdata, GLADE_PLACEHOLDER (child));
						break;
					}
				}
				g_list_free (children);
			}
		}

		/* 
		 * Save a copy of the original project so we can forward that to glade-project, 
		 * who'll copy in any resource files needed by any properties that are getting 
		 * cross-project pasted.
		 */
		cdata->project = cdata->widget->project;

		me->widgets = g_list_prepend (me->widgets, cdata);
	}

	glade_command_check_group (GLADE_COMMAND (me));

	/*
	 * Push it onto the undo stack only on success
	 */
	if (glade_command_add_remove_execute (GLADE_COMMAND (me)))
		glade_project_push_undo (glade_app_get_project(), GLADE_COMMAND (me));
	else
		g_object_unref (G_OBJECT (me));

	if (placeholders) 
		g_list_free (placeholders);
		
} /* end of glade_command_add() */

/**
 * glade_command_remove:
 * @widgets: a #GList of #GladeWidgets
 *
 * Performs a remove command on all widgets in @widgets from @parent.
 */
void
glade_command_remove (GList *widgets)
{
	GladeCommandAddRemove	*me;
	GladeWidget				*widget = NULL;
	CommandData				*cdata;
	GList					*list, *l;

	g_return_if_fail (widgets != NULL);

 	me = g_object_new (GLADE_COMMAND_ADD_REMOVE_TYPE, NULL);
	me->add = FALSE;
	me->from_clipboard = FALSE;

	/* internal children cannot be deleted. Notify the user. */
	for (list = widgets; list && list->data; list = list->next)
	{
		widget = list->data;
		if (widget->internal)
		{
			glade_util_ui_message (glade_app_get_window(),
					       GLADE_UI_WARN,
					       _("You cannot remove a widget internal to a composite widget."));
			return;
		}
	}

	me->project = glade_widget_get_project (widget);

	for (list = widgets; list && list->data; list = list->next)
	{
		widget         = list->data;

		cdata          = g_new0 (CommandData, 1);
		cdata->widget  = g_object_ref (G_OBJECT (widget));
		cdata->parent  = glade_widget_get_parent (widget);
		cdata->project = glade_widget_get_project (widget);

		if (widget->internal)
			g_critical ("Internal widget in Remove");

		if (cdata->parent != NULL &&
		    glade_widget_placeholder_relation 
		    (cdata->parent, cdata->widget))
		{
			glade_command_placeholder_connect 
				(cdata, GLADE_PLACEHOLDER (glade_placeholder_new ()));
		}
		me->widgets = g_list_prepend (me->widgets, cdata);

		/* Dont record props in create_execute (whether or not we actually
		 * record any props here
		 */
		cdata->props_recorded = TRUE; 

		/* Record packing props if not deleted from the clipboard */
		if (me->from_clipboard == FALSE)
		{
			for (l = widget->packing_properties; l; l = l->next)
				cdata->pack_props = 
					g_list_prepend (cdata->pack_props,
							glade_property_dup (GLADE_PROPERTY (l->data),
									    cdata->widget));
		}
	}

	if (g_list_length (widgets) == 1)
		GLADE_COMMAND (me)->description =
			g_strdup_printf (_("Remove %s"), 
					 GLADE_WIDGET (widgets->data)->name);
	else
		GLADE_COMMAND (me)->description =
			g_strdup_printf (_("Remove multiple"));

	g_assert (widget);

	glade_command_check_group (GLADE_COMMAND (me));

	if (glade_command_add_remove_execute (GLADE_COMMAND (me)))
		glade_project_push_undo (GLADE_PROJECT (widget->project), GLADE_COMMAND (me));
	else
		g_object_unref (G_OBJECT (me));
		
} /* end of glade_command_remove() */

static void
glade_command_transfer_props (GladeWidget *gnew, GList *saved_props)
{
	GList *l;

	for (l = saved_props; l; l = l->next)
	{
		GladeProperty *prop, *sprop = l->data;
		
		prop = glade_widget_get_pack_property (gnew, sprop->klass->id);

		if (prop && sprop->klass->transfer_on_paste &&
		    glade_property_class_match (prop->klass, sprop->klass))
			glade_property_set_value (prop, sprop->value);
	}
}

static gboolean
glade_command_add_execute (GladeCommandAddRemove *me)
{
	GladeProject       *active_project = glade_app_get_project ();
	CommandData        *cdata;
	GList              *list, *l, *saved_props;
	gchar              *special_child_type;

	if (me->widgets)
	{
		/* XXX FIXME: Selection here should be specific to the project
		 * related to this command
		 */
		glade_app_selection_clear (FALSE);

		for (list = me->widgets; list && list->data; list = list->next)
		{
			cdata  = list->data;
			saved_props = NULL;

			if (cdata->parent != NULL)
			{
				/* Prepare special-child-type for the paste. */
				if (me->from_clipboard)
				{
					if (cdata->props_recorded == FALSE)
					{
						/* Clear it the first time */
						g_object_set_data (cdata->widget->object,
								   "special-child-type", NULL);
					}
					else
					{
						g_object_set_data_full (cdata->widget->object, 
									"special-child-type",
									g_strdup (cdata->special_type), 
									g_free);
					}
				}
				
				/* Only transfer properties when they are from the clipboard,
				 * otherwise prioritize packing defaults. 
				 */
				if (me->from_clipboard)
				{
					saved_props =
						glade_widget_dup_properties (cdata->widget->packing_properties, FALSE);
					
					glade_widget_set_packing_properties (cdata->widget, cdata->parent);
				}

				/* glade_command_paste ganauntees that if 
				 * there we are pasting to a placeholder, 
				 * there is only one widget.
				 */
				if (cdata->placeholder)
				{
					glade_widget_replace
						(cdata->parent,
						 G_OBJECT (cdata->placeholder),
						 cdata->widget->object);
				}
				else
				{
					glade_widget_add_child (cdata->parent,
								cdata->widget, 
								cdata->props_recorded == FALSE);
				}


				glade_command_transfer_props (cdata->widget, saved_props);
				
				if (saved_props)
				{
					g_list_foreach (saved_props, (GFunc)g_object_unref, NULL);
					g_list_free (saved_props);
				}
				
				/* Now that we've added, apply any packing props if nescisary. */
				for (l = cdata->pack_props; l; l = l->next)
				{
					GValue         value = { 0, };
					GladeProperty *saved_prop = l->data;
					GladeProperty *widget_prop = 
						glade_widget_get_pack_property (cdata->widget,
										saved_prop->klass->id);

					glade_property_get_value (saved_prop, &value);
					glade_property_set_value (widget_prop, &value);
					g_value_unset (&value);
				}
				
				if (cdata->props_recorded == FALSE) 
				{
					/* Save the packing properties after the initial paste.
					 * (this will be the defaults returned by the container
					 * implementation after initially adding them).
					 *
					 * Otherwise this recorded marker was set when cutting
					 */
					g_assert (cdata->pack_props == NULL);
					for (l = cdata->widget->packing_properties; 
					     l; l = l->next)
						cdata->pack_props = 
							g_list_prepend
							(cdata->pack_props,
							 glade_property_dup
							 (GLADE_PROPERTY (l->data),
							  cdata->widget));


					/* Record the special-type here after replacing */
					if ((special_child_type = 
					     g_object_get_data (cdata->widget->object, 
								"special-child-type")) != NULL)
					{
						g_free (cdata->special_type);
						cdata->special_type = g_strdup (special_child_type);
					}

					/* Mark the properties as recorded */
					cdata->props_recorded = TRUE;
				}
				
				
			}

			/* Toplevels get pasted to the active project */
			if (me->from_clipboard && 
			    GTK_WIDGET_TOPLEVEL (cdata->widget->object))
				glade_project_add_object 
					(active_project, cdata->project,
					 cdata->widget->object);
			else
				glade_project_add_object(me->project, cdata->project, cdata->widget->object);
			
			glade_app_selection_add(cdata->widget->object, FALSE);

			glade_widget_show (cdata->widget);
		}
		glade_app_selection_changed ();
	}
	return TRUE;
	
} /* end of glade_command_add_execute() */

static gboolean
glade_command_remove_execute (GladeCommandAddRemove *me)
{
	CommandData      *cdata;
	GList            *list;

	for (list = me->widgets; list && list->data; list = list->next)
	{
		cdata = list->data;

		if (cdata->parent)
		{
			if (cdata->placeholder)
				glade_widget_replace(cdata->parent, cdata->widget->object, G_OBJECT (cdata->placeholder));
			else
				glade_widget_remove_child (cdata->parent, cdata->widget);
		}

		glade_project_remove_object(GLADE_PROJECT (cdata->widget->project), cdata->widget->object);

		glade_widget_hide (cdata->widget);
	}

	return TRUE;
}

/*
 * Execute the cmd and revert it.  Ie, after the execution of this
 * function cmd will point to the undo action
 */
static gboolean
glade_command_add_remove_execute (GladeCommand *cmd)
{
	GladeCommandAddRemove *me = (GladeCommandAddRemove*) cmd;
	gboolean retval;

	if (me->add)
		retval = glade_command_add_execute (me);
	else
		retval = glade_command_remove_execute (me);

	me->add = !me->add;

	return retval;
}

static gboolean
glade_command_add_remove_undo (GladeCommand *cmd)
{
	return glade_command_add_remove_execute (cmd);
}

static void
glade_command_add_remove_finalize (GObject *obj)
{
	GladeCommandAddRemove    *cmd;
	CommandData              *cdata;
	GList                    *list;

	g_return_if_fail (GLADE_IS_COMMAND_ADD_REMOVE (obj));

	cmd = GLADE_COMMAND_ADD_REMOVE (obj);

	for (list = cmd->widgets; list && list->data; list = list->next)
	{
		cdata = list->data;
		
		if (cdata->placeholder)
		{
			if (cdata->handler_id)
				g_signal_handler_disconnect (cdata->placeholder,
							     cdata->handler_id);
			if (GTK_OBJECT_FLOATING (cdata->placeholder))
				gtk_widget_destroy (GTK_WIDGET (cdata->placeholder));
		}

		if (cdata->widget)
			g_object_unref (G_OBJECT (cdata->widget));
	}
	g_list_free (cmd->widgets);
	
	glade_command_finalize (obj);
}

static gboolean
glade_command_add_remove_unifies (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
	return FALSE;
}

static void
glade_command_add_remove_collapse (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
	g_return_if_reached ();
}

/******************************************************************************
 * 
 * clipboard_add/clipboard_remove
 * 
 * These canonical commands add/remove a widget list to/from the clipboard.
 * 
 *****************************************************************************/

typedef struct {
	GladeCommand  parent;
	GList		 *widgets;
	gboolean	  add;
} GladeCommandClipboardAddRemove;


GLADE_MAKE_COMMAND (GladeCommandClipboardAddRemove, glade_command_clipboard_add_remove);
#define GLADE_COMMAND_CLIPBOARD_ADD_REMOVE_TYPE			(glade_command_clipboard_add_remove_get_type ())
#define GLADE_COMMAND_CLIPBOARD_ADD_REMOVE(o)	  		(G_TYPE_CHECK_INSTANCE_CAST ((o), GLADE_COMMAND_CLIPBOARD_ADD_REMOVE_TYPE, GladeCommandClipboardAddRemove))
#define GLADE_COMMAND_CLIPBOARD_ADD_REMOVE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST ((k), GLADE_COMMAND_CLIPBOARD_ADD_REMOVE_TYPE, GladeCommandClipboardAddRemoveClass))
#define GLADE_IS_COMMAND_CLIPBOARD_ADD_REMOVE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_COMMAND_CLIPBOARD_ADD_REMOVE_TYPE))
#define GLADE_IS_COMMAND_CLIPBOARD_ADD_REMOVE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_COMMAND_CLIPBOARD_ADD_REMOVE_TYPE))

void
glade_command_clipboard_add_remove_common (GList *widgets, gboolean add)
{
	GladeCommandClipboardAddRemove	*me;
	GladeWidget						*widget = NULL;
	GList							*list;
	
	g_return_if_fail(widgets && widgets->data);
	
 	me = g_object_new(GLADE_COMMAND_CLIPBOARD_ADD_REMOVE_TYPE, NULL);
	for (list = widgets; list && list->data; list = list->next)
	{
 		widget = g_object_ref(G_OBJECT(list->data));
 		me->widgets = g_list_prepend (me->widgets, widget);
	}
	me->add = add;
	if (add)
	{
		if (g_list_length (widgets) == 1)
			GLADE_COMMAND(me)->description = g_strdup_printf (_("Clipboard add %s"),
									  widget->name);
		else
			GLADE_COMMAND(me)->description =  g_strdup (_("Clipboard add multiple"));
	}
	else
	{
		if (g_list_length (widgets) == 1)
			GLADE_COMMAND(me)->description = g_strdup_printf (_("Clipboard remove %s"),
									  widget->name);
		else
			GLADE_COMMAND(me)->description =  g_strdup (_("Clipboard remove multiple"));
	}	
	
	glade_command_check_group(GLADE_COMMAND(me));
	
	if (glade_command_clipboard_add_remove_execute(GLADE_COMMAND(me)))
	{
		glade_project_push_undo(glade_app_get_project(), GLADE_COMMAND(me));
	}
	else
	{
		g_object_unref(G_OBJECT(me));
	}
}

/**
 * glade_command_clipboard_add:
 * @widgets: a #GList
 *
 * Performs an add command on all widgets in @widgets to the clipboard.
 */
void
glade_command_clipboard_add(GList *widgets)
{
	glade_command_clipboard_add_remove_common(widgets, TRUE);
}

/**
 * glade_command_clipboard_remove:
 * @widgets: a #GList
 *
 * Performs a remove command on all widgets in @widgets from the clipboard.
 */
void
glade_command_clipboard_remove (GList *widgets)
{
	glade_command_clipboard_add_remove_common(widgets, FALSE);
}

static gboolean
glade_command_clipboard_add_execute (GladeCommandClipboardAddRemove *me)
{
	GList *list, *widgets = NULL;

	for (list = me->widgets; list && list->data; list = list->next)
	{
		widgets = g_list_prepend(widgets, list->data);
	}
	if (widgets)
	{
		glade_clipboard_add(glade_app_get_clipboard(), widgets);
		g_list_free(widgets);
	}
	
	return TRUE;
}

static gboolean
glade_command_clipboard_remove_execute (GladeCommandClipboardAddRemove *me)
{
	GList *list, *widgets = NULL;

	for (list = me->widgets; list && list->data; list = list->next)
	{
		widgets = g_list_prepend(widgets, list->data);
	}
	if (widgets)
	{
		glade_clipboard_remove(glade_app_get_clipboard(), widgets);
		g_list_free(widgets);
	}

	return TRUE;
}

static gboolean
glade_command_clipboard_add_remove_execute (GladeCommand *cmd)
{
	GladeCommandClipboardAddRemove *me = (GladeCommandClipboardAddRemove*) cmd;
	gboolean retval;

	if (me->add)
		retval = glade_command_clipboard_add_execute (me);
	else
		retval = glade_command_clipboard_remove_execute (me);

	me->add = !me->add;

	return retval;
}

static gboolean
glade_command_clipboard_add_remove_undo (GladeCommand *cmd)
{
	return glade_command_clipboard_add_remove_execute (cmd);
}

static void
glade_command_clipboard_add_remove_finalize (GObject *obj)
{
	GladeCommandClipboardAddRemove *cmd;
	GList                          *list;

	g_return_if_fail (GLADE_IS_COMMAND_CLIPBOARD_ADD_REMOVE (obj));

	cmd = GLADE_COMMAND_CLIPBOARD_ADD_REMOVE (obj);

	for (list = cmd->widgets; list && list->data; list = list->next)
		if (list->data)
			g_object_unref(G_OBJECT(list->data));
	g_list_free (cmd->widgets);
	
	glade_command_finalize (obj);
}

static gboolean
glade_command_clipboard_add_remove_unifies (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
	return FALSE;
}

static void
glade_command_clipboard_add_remove_collapse (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
	g_return_if_reached ();
}

/******************************************************************************
 * 
 * The following are command aliases.  Their implementations are the actual 
 * glade commands.
 * 
 *****************************************************************************/

/**
 * glade_command_create:
 * @adaptor:		A #GladeWidgetAdaptor
 * @placeholder:	the placeholder which will be substituted by the widget
 * @project:            the project his widget belongs to.
 *
 * Creates a new widget using @adaptor and put in place of the @placeholder
 * in the @project
 *
 * Returns the newly created widget.
 */
GladeWidget*
glade_command_create(GladeWidgetAdaptor *adaptor, GladeWidget *parent, GladePlaceholder *placeholder, GladeProject *project)
{
	GladeWidget *widget;
	GList *widgets = NULL;
	gchar *description;	
	
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);
	if (GWA_IS_TOPLEVEL(adaptor) == FALSE)
		g_return_val_if_fail (GLADE_IS_WIDGET(parent), NULL);
		
	/* attempt to create the widget -- widget may be null, e.g. the user clicked cancel on a query */
	widget = glade_widget_adaptor_create_widget(adaptor, TRUE, "parent", parent, "project", project, NULL);
	if (widget == NULL)
	{
		return NULL;
	}
	widgets = g_list_prepend(widgets, widget);
	description = g_strdup_printf (_("Create %s"), g_list_length (widgets) == 1 ? widget->name : _("multiple"));
	glade_command_push_group(description);
	glade_command_add(widgets, parent, placeholder, FALSE);
	glade_command_pop_group();
	
	if (widgets)
		g_list_free(widgets);
	
	return widget;
}

/**
 * glade_command_delete:
 * @widgets: a #GList of #GladeWidgets
 *
 * Performs a delete command on the list of widgets.
 */
void
glade_command_delete(GList *widgets)
{
	GladeWidget *widget;
	gchar *description;	
	
	g_return_if_fail (widgets != NULL);
	
	widget = widgets->data;
	description = g_strdup_printf (_("Delete %s"), g_list_length (widgets) == 1 ? widget->name : _("multiple"));
	glade_command_push_group(description);
	glade_command_remove(widgets);
	glade_command_pop_group();
}

/**
 * glade_command_cut:
 * @widgets: a #GList of #GladeWidgets
 *
 * Removes the list of widgets and adds them to the clipboard.
 */
void
glade_command_cut(GList *widgets)
{
	GladeWidget *widget;
	GList       *l;
	gchar *description;	
	
	g_return_if_fail (widgets != NULL);

	for (l = widgets; l; l = l->next)
		g_object_set_data (G_OBJECT (l->data), "glade-command-was-cut", GINT_TO_POINTER (TRUE));
		
	widget = widgets->data;
	description = g_strdup_printf (_("Cut %s"), g_list_length (widgets) == 1 ? widget->name : _("multiple"));
	glade_command_push_group(description);
	glade_command_remove(widgets);
	glade_command_clipboard_add(widgets);
	glade_command_pop_group();
}

/**
 * glade_command_copy:
 * @widgets: a #GList of #GladeWidgets
 *
 * Copies the list of widgets and adds them to the clipboard.
 */
void
glade_command_copy(GList *widgets)
{
	GList *list, *copied_widgets = NULL;
	GladeWidget *copied_widget = NULL;
	gchar *description;	
	
	g_return_if_fail (widgets != NULL);
	
	for (list = widgets; list && list->data; list = list->next)
	{
		copied_widget = glade_widget_dup(list->data, FALSE);
		copied_widgets = g_list_prepend(copied_widgets, copied_widget);
	}
	description = g_strdup_printf (_("Copy %s"), g_list_length (widgets) == 1 ? copied_widget->name : _("multiple"));
	glade_command_push_group(description);
	glade_command_clipboard_add(copied_widgets);
	glade_command_pop_group();
	
	if (copied_widgets)
		g_list_free(copied_widgets);
}

/**
 * glade_command_paste:
 * @widgets: a #GList of #GladeWidget
 * @parent: a #GladeWidget
 * @placeholder: a #GladePlaceholder
 *
 * Performs a paste command on all widgets in @widgets to @parent, possibly
 * replacing @placeholder (note toplevels dont need a parent; the active project
 * will be used when pasting toplevel objects).
 */
void
glade_command_paste(GList *widgets, GladeWidget *parent, GladePlaceholder *placeholder)
{
	GList *list, *copied_widgets = NULL;
	GladeWidget *copied_widget = NULL;
	gchar *description;	
	gboolean exact;
	
	g_return_if_fail (widgets != NULL);
	
	for (list = widgets; list && list->data; list = list->next)
	{
		exact = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (list->data), "glade-command-was-cut"));

		copied_widget = glade_widget_dup(list->data, exact);
		copied_widgets = g_list_prepend(copied_widgets, copied_widget);
	}
	description = g_strdup_printf (_("Paste %s"), g_list_length (widgets) == 1 ? copied_widget->name : _("multiple"));
	glade_command_push_group(description);
	glade_command_add(copied_widgets, parent, placeholder, TRUE);
	glade_command_pop_group();
	
	if (copied_widgets)
		g_list_free(copied_widgets);
}

/**
 * glade_command_dnd:
 * @widgets: a #GList of #GladeWidget
 * @parent: a #GladeWidget
 * @placeholder: a #GladePlaceholder
 *
 * Performs a drag-n-drop command, i.e. removes the list of widgets and adds them 
 * to the new parent, possibly replacing @placeholder (note toplevels dont need a 
 * parent; the active project will be used when pasting toplevel objects).
 */
void
glade_command_dnd(GList *widgets, GladeWidget *parent, GladePlaceholder *placeholder)
{
	GladeWidget *widget;
	gchar *description;	
	
	g_return_if_fail (widgets != NULL);
	
	widget = widgets->data;
	description = g_strdup_printf (_("Drag-n-Drop from %s to %s"), parent->name, g_list_length (widgets) == 1 ? widget->name : _("multiple"));
	glade_command_push_group(description);
	glade_command_remove(widgets);
	glade_command_add(widgets, parent, placeholder, TRUE);
	glade_command_pop_group();
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

	g_object_unref (cmd->widget);

	if (cmd->signal)
		glade_signal_free (cmd->signal);
	if (cmd->new_signal)
		glade_signal_free (cmd->new_signal);

	glade_command_finalize (obj);
}

static gboolean
glade_command_add_signal_undo (GladeCommand *this_cmd)
{
	return glade_command_add_signal_execute (this_cmd);
}

static gboolean
glade_command_add_signal_execute (GladeCommand *this_cmd)
{
	GladeCommandAddSignal *cmd = GLADE_COMMAND_ADD_SIGNAL (this_cmd);
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
glade_command_add_signal_unifies (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
	return FALSE;
}

static void
glade_command_add_signal_collapse (GladeCommand *this_cmd, GladeCommand *other_cmd)
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
	
	glade_command_check_group (GLADE_COMMAND (me));

	if (glade_command_add_signal_execute (cmd))
		glade_project_push_undo (GLADE_PROJECT (glade_widget->project), cmd);
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
				 const GladeSignal *new_signal)
{
	glade_command_add_remove_change_signal 
		(glade_widget, old, new_signal, GLADE_CHANGE);
}

/******************************************************************************
 * 
 * set i18n metadata
 * 
 * This command sets the i18n metadata on a label property.
 * 
 *****************************************************************************/

typedef struct {
	GladeCommand   parent;
	GladeProperty *property;
	gboolean       translatable;
	gboolean       has_context;
	gchar         *comment;
	gboolean       old_translatable;
	gboolean       old_has_context;
	gchar         *old_comment;
} GladeCommandSetI18n;


GLADE_MAKE_COMMAND (GladeCommandSetI18n, glade_command_set_i18n);
#define GLADE_COMMAND_SET_I18N_TYPE			(glade_command_set_i18n_get_type ())
#define GLADE_COMMAND_SET_I18N(o)	  		(G_TYPE_CHECK_INSTANCE_CAST ((o), GLADE_COMMAND_SET_I18N_TYPE, GladeCommandSetI18n))
#define GLADE_COMMAND_SET_I18N_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST ((k), GLADE_COMMAND_SET_I18N_TYPE, GladeCommandSetI18nClass))
#define GLADE_IS_COMMAND_SET_I18N(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GLADE_COMMAND_SET_I18N_TYPE))
#define GLADE_IS_COMMAND_SET_I18N_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GLADE_COMMAND_SET_I18N_TYPE))

static gboolean
glade_command_set_i18n_execute(GladeCommand *cmd)
{
	GladeCommandSetI18n *me = (GladeCommandSetI18n *)cmd;
	gboolean  temp_translatable;
	gboolean  temp_has_context;
	gchar    *temp_comment;
	
	/* sanity check */
	g_return_val_if_fail (me != NULL, TRUE);
	g_return_val_if_fail (me->property != NULL, TRUE);

	/* set the new values in the property */
	glade_property_i18n_set_translatable(me->property, me->translatable);	
	glade_property_i18n_set_has_context(me->property, me->has_context);
	glade_property_i18n_set_comment(me->property, me->comment);

	/* swap the current values with the old values to prepare for undo */
	temp_translatable = me->translatable;
	temp_has_context = me->has_context;
	temp_comment = me->comment;
	me->translatable = me->old_translatable;
	me->has_context = me->old_has_context;
	me->comment = me->old_comment;
	me->old_translatable = temp_translatable;
	me->old_has_context = temp_has_context;
	me->old_comment = temp_comment;
	
	return TRUE;
}

static gboolean
glade_command_set_i18n_undo(GladeCommand *cmd)
{
	return glade_command_set_i18n_execute(cmd);
}

static void
glade_command_set_i18n_finalize(GObject *obj)
{
	GladeCommandSetI18n	*me;
	
	g_return_if_fail(GLADE_IS_COMMAND_SET_I18N(obj));

	me = GLADE_COMMAND_SET_I18N(obj);
	g_free (me->comment);
	g_free (me->old_comment);
	
	glade_command_finalize(obj);
}

static gboolean
glade_command_set_i18n_unifies (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
	GladeCommandSetI18n *cmd1;
	GladeCommandSetI18n *cmd2;

	if (GLADE_IS_COMMAND_SET_I18N (this_cmd) && GLADE_IS_COMMAND_SET_I18N (other_cmd))
	{
		cmd1 = GLADE_COMMAND_SET_I18N (this_cmd);
		cmd2 = GLADE_COMMAND_SET_I18N (other_cmd);

		return (cmd1->property == cmd2->property);
	}

	return FALSE;
}

static void
glade_command_set_i18n_collapse (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
	/* this command is the one that will be used for an undo of the sequence of like commands */
	GladeCommandSetI18n *this = GLADE_COMMAND_SET_I18N (this_cmd);
	
	/* the other command contains the values that will be used for a redo */
	GladeCommandSetI18n *other = GLADE_COMMAND_SET_I18N (other_cmd);

	g_return_if_fail (GLADE_IS_COMMAND_SET_I18N (this_cmd) && GLADE_IS_COMMAND_SET_I18N (other_cmd));

	/* adjust this command to contain, as its old values, the other command's current values */
	this->old_translatable = other->old_translatable;
	this->old_has_context = other->old_has_context;
	g_free (this->old_comment);
	this->old_comment = other->old_comment;
	other->old_comment = NULL;

	glade_app_update_ui ();
}

/**
 * glade_command_set_i18n:
 * @property: a #GladeProperty
 * @translatable: a #gboolean
 * @has_context: a #gboolean
 * @comment: a #const gchar *
 *
 * Sets the i18n data on the property.
 */
void
glade_command_set_i18n (GladeProperty *property, gboolean translatable, gboolean has_context, const gchar *comment)
{
	GladeCommandSetI18n *me;
	
	g_return_if_fail(property);
	
	/* check that something changed before continuing with the command */
	if (translatable == property->i18n_translatable &&
		has_context == property->i18n_has_context   &&
		((comment == NULL && property->i18n_comment == NULL) ||
		 (comment && property->i18n_comment && !strcmp(property->i18n_comment, comment))))
		return;

	/* load up the command */
 	me = g_object_new(GLADE_COMMAND_SET_I18N_TYPE, NULL);
	me->property = property;
	me->translatable = translatable;
	me->has_context = has_context;
	me->comment = g_strdup(comment);
	me->old_translatable = property->i18n_translatable;
	me->old_has_context = property->i18n_has_context;
	me->old_comment = g_strdup(property->i18n_comment);
	GLADE_COMMAND(me)->description = g_strdup_printf(_("Setting i18n metadata"));;
	
	glade_command_check_group(GLADE_COMMAND(me));
	
	/* execute the command and push it on the stack if successful */
	if (glade_command_set_i18n_execute(GLADE_COMMAND(me)))
	{
		glade_project_push_undo(glade_app_get_project(), GLADE_COMMAND(me));
	}
	else
	{
		g_object_unref(G_OBJECT(me));
	}
}


