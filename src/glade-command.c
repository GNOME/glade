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
glade_command_unifies_impl (GladeCommand *this, GladeCommand *other)
{
	return FALSE;
}

static void
glade_command_collapse_impl (GladeCommand *this, GladeCommand *other)
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
func ## _unifies (GladeCommand *this, GladeCommand *other);		\
static void								\
func ## _collapse (GladeCommand *this, GladeCommand *other);		\
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
				(sdata->property->class, &new_value);

			g_print ("Setting %s property of %s to %s (sumode: %d)\n",
				 sdata->property->class->id,
				 sdata->property->widget->name,
				 str, glade_property_superuser ());

			g_free (str);
		}
#endif

		/* Packing properties need to be refreshed here since
		 * they are reset when they get added to containers.
		 */
		if (sdata->property->class->packing)
		{
			GladeProperty *tmp_prop;

			tmp_prop = glade_widget_get_pack_property
				(sdata->property->widget, sdata->property->class->id);

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
glade_command_set_property_unifies (GladeCommand *this, GladeCommand *other)
{
       GladeCommandSetProperty *cmd1,   *cmd2;
       GCSetPropData           *pdata1, *pdata2;
       GList                   *list, *l;

       if (GLADE_IS_COMMAND_SET_PROPERTY (this) && 
	   GLADE_IS_COMMAND_SET_PROPERTY (other))
       {
               cmd1 = (GladeCommandSetProperty*) this;
               cmd2 = (GladeCommandSetProperty*) other;

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
				   glade_property_class_match (pdata1->property->class,
							       pdata2->property->class))
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
glade_command_set_property_collapse (GladeCommand *this, GladeCommand *other)
{
	GladeCommandSetProperty *cmd1,   *cmd2;
	GCSetPropData           *pdata1, *pdata2;
	GList                   *list, *l;
	
	g_return_if_fail (GLADE_IS_COMMAND_SET_PROPERTY (this) &&
			  GLADE_IS_COMMAND_SET_PROPERTY (other));
	
	cmd1 = (GladeCommandSetProperty*) this;
	cmd2 = (GladeCommandSetProperty*) other;


	for (list = cmd1->sdata; list; list = list->next)
	{
		pdata1 = list->data;
		for (l = cmd2->sdata; l; l = l->next)
		{
			pdata2 = l->data;
			
			if (glade_property_class_match (pdata1->property->class,
							pdata2->property->class))
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
	g_free (this->description);
	this->description = other->description;
	other->description = NULL;

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
		value_name = glade_property_class_make_string_from_gvalue (sdata->property->class, 
									   sdata->new_value);
		if (!value_name || strlen (value_name) > MAX_UNDO_MENU_ITEM_VALUE_LEN
		    || strchr (value_name, '_')) {
			description = g_strdup_printf (_("Setting %s of %s"),
						       sdata->property->class->name,
						       sdata->property->widget->name);
		} else {
			description = g_strdup_printf (_("Setting %s of %s to %s"),
						       sdata->property->class->name,
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
	value = glade_property_class_make_gvalue_from_vl (property->class, args);
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

/***************************************************
 * CREATE / DELETE
 **************************************************/

typedef struct {
	GladeCommand      parent;
	
	GList            *widgets;
	gboolean          create;
	gboolean          from_clipboard;
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

static gboolean
glade_command_create_execute (GladeCommandCreateDelete *me)
{
	GladeClipboard   *clipboard = glade_app_get_clipboard();
	CommandData      *cdata = NULL;
	GList            *list, *wlist = NULL, *l;

	glade_app_selection_clear (FALSE);

	for (list = me->widgets; list && list->data; list = list->next)
	{
		cdata = list->data;

		if (cdata->parent)
		{
			if (cdata->placeholder)
				glade_widget_replace
					(cdata->parent, 
					 G_OBJECT (cdata->placeholder), 
					 cdata->widget->object);
			else
				glade_widget_add_child (cdata->parent, cdata->widget, FALSE);

			/* Now that we've added, apply any packing props if nescisary. */
			for (l = cdata->pack_props; l; l = l->next)
			{
				GValue         value = { 0, };
				GladeProperty *saved_prop = l->data;
				GladeProperty *widget_prop = 
					glade_widget_get_pack_property (cdata->widget,
									saved_prop->class->id);
				
				glade_property_get_value (saved_prop, &value);
				glade_property_set_value (widget_prop, &value);
				g_value_unset (&value);
			}
				
			if (cdata->props_recorded == FALSE) 
			{
				/* Save the packing properties after the initial creation.
				 * (this will be the defaults returned by the container
				 * implementation after initially adding them).
				 *
				 * Otherwise this recorded marker was set when deleting
				 */
				g_assert (cdata->pack_props == NULL);
				for (l = cdata->widget->packing_properties; l; l = l->next)
					cdata->pack_props = 
						g_list_prepend (cdata->pack_props,
								glade_property_dup (GLADE_PROPERTY (l->data),
										    cdata->widget));


				/* Mark the properties as recorded
				 */
				cdata->props_recorded = TRUE;
			}

		}
		
		if (me->from_clipboard)
		{
			wlist = g_list_prepend (wlist, cdata->widget);
		}
		else
		{
			glade_project_add_object 
				(GLADE_PROJECT (cdata->widget->project),
				 NULL, cdata->widget->object);
			glade_app_selection_add 
				(cdata->widget->object, TRUE);
		}
	}
	
	if (wlist) 
	{
		glade_clipboard_add (clipboard, wlist);
		g_list_free (wlist);
	}

	if (cdata)
		glade_widget_show (cdata->widget);
	
	return TRUE;
}

static gboolean
glade_command_delete_execute (GladeCommandCreateDelete *me)
{
	GladeClipboard   *clipboard = glade_app_get_clipboard();
	CommandData      *cdata;
	GList            *list, *wlist = NULL;

	for (list = me->widgets; list && list->data; list = list->next)
	{
		cdata = list->data;

		if (cdata->parent && me->from_clipboard == FALSE)
		{
			if (cdata->placeholder)
				glade_widget_replace
					(cdata->parent, cdata->widget->object, 
					 G_OBJECT (cdata->placeholder));
			else
				glade_widget_remove_child (cdata->parent, cdata->widget);
		}

		if (me->from_clipboard != FALSE) 
			wlist = g_list_prepend (wlist, cdata->widget);
		else
			glade_project_remove_object 
				(GLADE_PROJECT (cdata->widget->project), cdata->widget->object);

		glade_widget_hide (cdata->widget);
	}

	if (wlist) 
	{
		glade_clipboard_remove (clipboard, wlist);
		g_list_free (wlist);
	}

	return TRUE;
}

/*
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
	CommandData              *cdata;
	GList                    *list;

	g_return_if_fail (GLADE_IS_COMMAND_CREATE_DELETE (obj));

	cmd = GLADE_COMMAND_CREATE_DELETE (obj);

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
 * glade_command_delete:
 * @widgets: a #GList of #GladeWidgets
 *
 * Performs a delete command on the list of widgets
 */
void
glade_command_delete (GList *widgets)
{
	GladeClipboard           *clipboard = glade_app_get_clipboard();
	GladeCommandCreateDelete *me;
	GladeWidget              *widget = NULL;
	CommandData              *cdata;
	GList                    *list, *l;

	g_return_if_fail (widgets != NULL);

 	me = g_object_new (GLADE_COMMAND_CREATE_DELETE_TYPE, NULL);
	me->create         = FALSE;
	me->from_clipboard = 
		(g_list_find (clipboard->selection, widgets->data) != NULL);

	/* internal children cannot be deleted. Notify the user. */
	for (list = widgets; list && list->data; list = list->next)
	{
		widget = list->data;
		if (widget->internal)
		{
			glade_util_ui_message (glade_app_get_window(),
					       GLADE_UI_WARN,
					       _("You cannot delete a widget internal to a composite widget."));
			return;
		}
	}

	for (list = widgets; list && list->data; list = list->next)
	{
		widget         = list->data;

		cdata          = g_new0 (CommandData, 1);
		cdata->widget  = g_object_ref (G_OBJECT (widget));
		cdata->parent  = glade_widget_get_parent (widget);

		if (widget->internal)
			g_critical ("Internal widget in Delete");

		/* !fixed here */
		if (cdata->parent != NULL &&
		    GLADE_IS_FIXED (cdata->parent) == FALSE &&
		    glade_util_gtkcontainer_relation 
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
			g_strdup_printf (_("Delete %s"), 
					 GLADE_WIDGET (widgets->data)->name);
	else
		GLADE_COMMAND (me)->description =
			g_strdup_printf (_("Delete multiple"));

	g_assert (widget);

	glade_command_check_group (GLADE_COMMAND (me));

	if (glade_command_create_delete_execute (GLADE_COMMAND (me)))
		glade_project_push_undo (GLADE_PROJECT (widget->project), 
					 GLADE_COMMAND (me));
	else
		g_object_unref (G_OBJECT (me));
}

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
GladeWidget *
glade_command_create (GladeWidgetAdaptor *adaptor,
		      GladeWidget        *parent,
		      GladePlaceholder   *placeholder,
		      GladeProject       *project)
{
	GladeCommandCreateDelete *me;
	CommandData              *cdata;
	GladeWidget              *widget;

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);
	if (GWA_IS_TOPLEVEL (adaptor) == FALSE)
		g_return_val_if_fail (GLADE_IS_WIDGET (parent), NULL);

 	me            = g_object_new (GLADE_COMMAND_CREATE_DELETE_TYPE, NULL);
	me->create    = TRUE;

	cdata         = g_new0 (CommandData, 1);
	cdata->parent = parent;

	glade_command_placeholder_connect (cdata, placeholder);
	
	me->widgets = g_list_append (me->widgets, cdata);

	widget = glade_widget_adaptor_create_widget (adaptor, TRUE,
						     "parent", parent, 
						     "project", project, 
						     NULL);

	/* widget may be null, e.g. the user clicked cancel on a query */
	if ((cdata->widget = widget) == NULL)
	{
		g_object_unref (G_OBJECT (me));
		return NULL;
	}

	/*  `widget' is created with an initial reference that belongs to "glade-3"
	 *  (and is unreffed by GladeProject at close time); so add our own explicit
	 *  reference here.
	 */
	g_object_ref (G_OBJECT (widget));
	
	GLADE_COMMAND (me)->description  =
		g_strdup_printf (_("Create %s"), 
				 cdata->widget->name);

	glade_command_check_group (GLADE_COMMAND (me));

	if (glade_command_create_delete_execute (GLADE_COMMAND (me)))
		glade_project_push_undo (project, GLADE_COMMAND (me));
	else
		g_object_unref (G_OBJECT (me));

	return widget;
}

static void
glade_command_transfer_props (GladeWidget *gnew, GList *saved_props)
{
	GList *l;

	for (l = saved_props; l; l = l->next)
	{
		GladeProperty *prop, *sprop = l->data;
		
		prop = glade_widget_get_pack_property (gnew, sprop->class->id);

		if (prop && sprop->class->transfer_on_paste &&
		    glade_property_class_match (prop->class, sprop->class))
			glade_property_set_value (prop, sprop->value);
	}
}

typedef enum {
	GLADE_CUT,
	GLADE_COPY,
	GLADE_PASTE
} GladeCutCopyPasteType;

/*
 * Cut/Copy/Paste
 *
 * Following is the code to extend the GladeCommand Undo/Redo system to 
 * Clipboard functions.
 */
typedef struct {
	GladeCommand           parent;
	GladeProject          *project;
	GList                 *widgets;
	GladeCutCopyPasteType  type;
	GladeCutCopyPasteType  original_type;
	gboolean               from_clipboard;
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
	GladeProject       *active_project = glade_app_get_project ();
	CommandData        *cdata;
	GList              *list, *remove = NULL, *l, *saved_props;
	gchar              *special_child_type;

	if (me->widgets)
	{
		glade_app_selection_clear (FALSE);

		for (list = me->widgets; list && list->data; list = list->next)
		{
			cdata  = list->data;
			remove = g_list_prepend (remove, cdata->widget);

			if (cdata->parent != NULL)
			{
				/* Prepare special-child-type for the paste. */
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

				saved_props = glade_widget_dup_properties (cdata->widget->packing_properties, FALSE);

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
										saved_prop->class->id);

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


					/* Mark the properties as recorded
					 */
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
				glade_project_add_object
					(me->project, cdata->project, 
					 cdata->widget->object);
			
			glade_app_selection_add
				(cdata->widget->object, FALSE);

			glade_widget_show (cdata->widget);
		}
		glade_app_selection_changed ();

		if (remove)
		{
			glade_clipboard_remove (glade_app_get_clipboard(), remove);
			g_list_free (remove);
		}
	}
	return TRUE;
}

static gboolean
glade_command_cut_execute (GladeCommandCutCopyPaste *me)
{
	CommandData        *cdata;
	GList              *list, *add = NULL;
	gchar              *special_child_type;

	for (list = me->widgets; list && list->data; list = list->next)
	{
		cdata = list->data;
		add   = g_list_prepend (add, cdata->widget);

		if (me->original_type == GLADE_CUT)
		{
			if ((special_child_type = 
			     g_object_get_data (cdata->widget->object, 
						"special-child-type")) != NULL)
			{
				g_free (cdata->special_type);
				cdata->special_type = g_strdup (special_child_type);
			}
		}
		else
			g_object_set_data (cdata->widget->object,
					   "special-child-type", NULL);

		if (cdata->parent)
		{
			if (cdata->placeholder)
				glade_widget_replace
					(cdata->parent,
					 cdata->widget->object,
					 G_OBJECT (cdata->placeholder));
			else
				glade_widget_remove_child (cdata->parent, cdata->widget);
		}

		g_object_set_data (cdata->widget->object, "special-child-type", NULL);
		
		glade_widget_hide (cdata->widget);
		glade_project_remove_object (GLADE_PROJECT (cdata->widget->project),
					     cdata->widget->object);
		
	}

	if (add)
	{
		glade_clipboard_add (glade_app_get_clipboard(), add);
		g_list_free (add);
	}
	return TRUE;
}

static gboolean
glade_command_copy_execute (GladeCommandCutCopyPaste *me)
{
	GList              *list, *add = NULL;

	for (list = me->widgets; list && list->data; list = list->next)
		add = g_list_prepend (add, ((CommandData *)list->data)->widget);

	if (add)
	{
		glade_clipboard_add (glade_app_get_clipboard(), add);
		g_list_free (add);
	}
	return TRUE;
}

static gboolean
glade_command_copy_undo (GladeCommandCutCopyPaste *me)
{
	GList              *list, *remove = NULL;

	for (list = me->widgets; list && list->data; list = list->next)
		remove = g_list_prepend (remove, ((CommandData *)list->data)->widget);

	if (remove)
	{
		glade_clipboard_remove (glade_app_get_clipboard(), remove);
		g_list_free (remove);
	}
	return TRUE;
}

/*
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
	GladeCommandCutCopyPaste *me = GLADE_COMMAND_CUT_COPY_PASTE (obj);
	CommandData              *cdata;
	GList                    *list;

	for (list = me->widgets; list && list->data; list = list->next)
	{
		cdata = list->data;
		if (cdata->widget)
			g_object_unref (cdata->widget);
		if (cdata->placeholder)
		{
			if (cdata->handler_id)
				g_signal_handler_disconnect (cdata->placeholder,
							     cdata->handler_id);

			if (GTK_OBJECT_FLOATING (cdata->placeholder))
				gtk_widget_destroy (GTK_WIDGET (cdata->placeholder));
		}
		if (cdata->pack_props)
		{
			g_list_foreach (cdata->pack_props, (GFunc)g_object_unref, NULL);
			g_list_free (cdata->pack_props);
		}
		g_free (cdata);
	}
	g_list_free (me->widgets);
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
glade_command_cut_copy_paste_common (GList                 *widgets,
				     GladeWidget           *parent,
				     GladePlaceholder      *placeholder,
				     GladeCutCopyPasteType  type)
{
	GladeCommandCutCopyPaste *me;
	CommandData              *cdata;
	GladeWidget              *widget = NULL;
	GList                    *l, *list, *children, *placeholders = NULL;
	gchar                    *fmt = NULL;
	GtkWidget                *child;

	g_return_if_fail (widgets && widgets->data);
	g_return_if_fail (parent == NULL || GLADE_IS_WIDGET (parent));

	/* Things can go wrong in this function if the dataset is inacurate,
	 * we make no real attempt here to recover, just g_critical() and
	 * fix the bugs as they pop up.
	 */
	fmt = 
		(type == GLADE_CUT) ? _("Cut %s") : 
		(type == GLADE_COPY) ? _("Copy %s") : _("Paste %s");
	widget = GLADE_WIDGET (widgets->data);
	me = (GladeCommandCutCopyPaste *) 
		g_object_new (GLADE_COMMAND_CUT_COPY_PASTE_TYPE, NULL);

	if (type == GLADE_PASTE && placeholder && 
	    GTK_IS_WINDOW (widget->object) == FALSE)
	{
		GladeWidget *some_widget = glade_placeholder_get_parent (placeholder);
		me->project = glade_widget_get_project (some_widget);
	}
	else if (type == GLADE_PASTE)
		me->project = glade_app_get_project();
	else
		me->project = glade_widget_get_project (widget);

	me->type           = type;
	me->original_type  = type;
	me->from_clipboard = (type == GLADE_PASTE);
	GLADE_COMMAND (me)->description = 
		g_strdup_printf (fmt, g_list_length (widgets) == 1 ? 
				 widget->name : _("multiple"));

	for (list = widgets; list && list->data; list = list->next)
	{
		widget             = list->data;

		cdata              = g_new0 (CommandData, 1);

		if (widget->internal)
			g_critical ("Internal widget in Cut/Copy/Paste");

		/* Widget */
		if (type == GLADE_COPY)
		{
			cdata->widget = glade_widget_dup (widget);
			/* Copy or not, we need a reference for GladeCommand, and
			 * a global reference for Glade.
			 */
			cdata->widget = g_object_ref (G_OBJECT (cdata->widget));
		} else
			cdata->widget = g_object_ref (G_OBJECT (widget));
		
		/* Parent */
		if (parent == NULL)
			cdata->parent = glade_widget_get_parent (widget);
		else if (type == GLADE_PASTE && placeholder && 
			 GTK_IS_WINDOW (widget->object) == FALSE)
			cdata->parent = glade_placeholder_get_parent (placeholder);
		else if (GTK_IS_WINDOW (widget->object) == FALSE)
			cdata->parent = parent;

		if (cdata->parent == NULL && type == GLADE_PASTE &&
		    GTK_IS_WINDOW (widget->object) == FALSE)
			g_critical ("Parentless non GtkWindow widget in Paste");

		/* Placeholder */
		if (type == GLADE_CUT)
		{
			if (cdata->parent && GLADE_IS_FIXED (cdata->parent) == FALSE &&
			    glade_util_gtkcontainer_relation
			    (cdata->parent, cdata->widget))
			{
				glade_command_placeholder_connect
					(cdata, GLADE_PLACEHOLDER (glade_placeholder_new ()));
			}
			else if (placeholder != NULL)
				glade_command_placeholder_connect (cdata, placeholder);
		}
		else if (type == GLADE_PASTE && placeholder != NULL &&
			 g_list_length (widgets) == 1)
		{
			glade_command_placeholder_connect (cdata, placeholder);
		}
		else if (type == GLADE_PASTE && cdata->parent &&
			 glade_util_gtkcontainer_relation (cdata->parent, widget))
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

		/* Save a copy of packing properties on cut so we can
		 * re-apply them at undo time.
		 */
		if (type == GLADE_CUT)
		{
			/* We dont want the paste mechanism to overwrite our 
			 * packing props when we undo a CUT command; so we have to 
			 * mark them "recorded" too.
			 */
			cdata->props_recorded = TRUE;
			for (l = cdata->widget->packing_properties; l; l = l->next)
				cdata->pack_props = 
					g_list_prepend (cdata->pack_props,
							glade_property_dup (GLADE_PROPERTY (l->data),
									    cdata->widget));
		}

		/* Save a copy of the original project so we can 
		 * forward that to glade-project, who'll copy in
		 * any resource files needed by any properties that
		 * are getting cross-project pasted.
		 */
		if (type == GLADE_PASTE)
			cdata->project = cdata->widget->project;

		me->widgets = g_list_prepend (me->widgets, cdata);
	}

	glade_command_check_group (GLADE_COMMAND (me));

	/*
	 * Push it onto the undo stack only on success
	 */
	if (glade_command_cut_copy_paste_execute (GLADE_COMMAND (me)))
		glade_project_push_undo 
			(glade_app_get_project(),
			 GLADE_COMMAND (me));
	else
		g_object_unref (G_OBJECT (me));

	if (placeholders) 
		g_list_free (placeholders);
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
glade_command_paste (GList *widgets, GladeWidget *parent, GladePlaceholder *placeholder)
{
	glade_command_cut_copy_paste_common (widgets, parent, placeholder, GLADE_PASTE);
}

/**
 * glade_command_cut:
 * @widgets: a #GList
 *
 * TODO: write me
 */
void
glade_command_cut (GList *widgets)
{
	glade_command_cut_copy_paste_common (widgets, NULL, NULL, GLADE_CUT);
}

/**
 * glade_command_copy:
 * @widgets: a #GList
 *
 * TODO: write me
 */
void
glade_command_copy (GList *widgets)
{
	glade_command_cut_copy_paste_common (widgets, NULL, NULL, GLADE_COPY);
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
				 const GladeSignal *new)
{
	glade_command_add_remove_change_signal 
		(glade_widget, old, new, GLADE_CHANGE);
}
