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

/**
 * SECTION:glade-command
 * @Short_Description: An event filter to implement the Undo/Redo stack.
 *
 * The Glade Command api allows us to view user actions as items and execute 
 * and undo those items; each #GladeProject has its own Undo/Redo stack.
 */

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
#include "glade-property-def.h"
#include "glade-debug.h"
#include "glade-placeholder.h"
#include "glade-clipboard.h"
#include "glade-signal.h"
#include "glade-app.h"
#include "glade-name-context.h"

typedef struct _GladeCommandPrivate
{
  GladeProject *project; /* The project this command is created for */

  gchar *description; /* a string describing the command.
                       * It's used in the undo/redo menu entry.
                       */

  gint   group_id;    /* If this is part of a command group, this is
                       * the group id (id is needed only to ensure that
                       * consecutive groups dont get merged).
                       */
} GladeCommandPrivate;

/* Concerning placeholders: we do not hold any reference to placeholders,
 * placeholders that are supplied by the backend are not reffed, placeholders
 * that are created by glade-command are temporarily owned by glade-command
 * untill they are added to a container; in which case it belongs to GTK+
 * and the backend (but I prefer to think of it as the backend).
 */
typedef struct
{
  GladeWidget *widget;
  GladeWidget *parent;
  GList *reffed;
  GladePlaceholder *placeholder;
  gboolean props_recorded;
  GList *pack_props;
  gchar *special_type;
  gulong handler_id;
} CommandData;

/* Group description used for the current group
 */
static gchar *gc_group_description = NULL;

/* Use an id to avoid grouping together consecutive groups
 */
static gint gc_group_id = 1;

/* Groups can be grouped together, push/pop must balance (duh!)
 */
static gint gc_group_depth = 0;


G_DEFINE_TYPE_WITH_PRIVATE (GladeCommand, glade_command, G_TYPE_OBJECT)

static void
glade_command_finalize (GObject *obj)
{
  GladeCommand *cmd = (GladeCommand *) obj;
  GladeCommandPrivate *priv = glade_command_get_instance_private (cmd);

  g_clear_pointer (&priv->description, g_free);

  G_OBJECT_CLASS (glade_command_parent_class)->finalize (obj);
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
glade_command_init (GladeCommand *command)
{
}

static void
glade_command_class_init (GladeCommandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = glade_command_finalize;

  klass->undo = NULL;
  klass->execute = NULL;
  klass->unifies = glade_command_unifies_impl;
  klass->collapse = glade_command_collapse_impl;
}

/* Macros for defining the derived command types */
#define GLADE_MAKE_COMMAND(type, func, upper_type)                   \
G_DECLARE_FINAL_TYPE (type, func, GLADE, upper_type, GladeCommand)   \
G_DEFINE_TYPE (type, func, GLADE_TYPE_COMMAND);                      \
static gboolean                                                      \
func ## _undo (GladeCommand *me);                                    \
static gboolean                                                      \
func ## _execute (GladeCommand *me);                                 \
static void                                                          \
func ## _finalize (GObject *object);                                 \
static gboolean                                                      \
func ## _unifies (GladeCommand *this_cmd, GladeCommand *other_cmd);  \
static void                                                          \
func ## _collapse (GladeCommand *this_cmd, GladeCommand *other_cmd); \
static void                                                          \
func ## _class_init (type ## Class *klass)                           \
{                                                                    \
  GladeCommandClass *command_class = GLADE_COMMAND_CLASS (klass);    \
  GObjectClass* object_class = G_OBJECT_CLASS (klass);               \
  command_class->undo =  func ## _undo;                              \
  command_class->execute =  func ## _execute;                        \
  command_class->unifies =  func ## _unifies;                        \
  command_class->collapse =  func ## _collapse;                      \
  object_class->finalize = func ## _finalize;                        \
}                                                                    \
static void                                                          \
func ## _init (type *self)                                           \
{                                                                    \
}


const gchar *
glade_command_description (GladeCommand *command)
{
  GladeCommandPrivate *priv = glade_command_get_instance_private (command);

  g_return_val_if_fail (GLADE_IS_COMMAND (command), NULL);

  return priv->description;
}

gint
glade_command_group_id (GladeCommand *command)
{
  GladeCommandPrivate *priv = glade_command_get_instance_private (command);

  g_return_val_if_fail (GLADE_IS_COMMAND (command), -1);

  return priv->group_id;
}


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
 * Returns: whether the command was successfully reversed
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
glade_command_unifies (GladeCommand *command, GladeCommand *other)
{
  GladeCommandPrivate *priv = glade_command_get_instance_private (command);
  GladeCommandPrivate *other_priv = glade_command_get_instance_private (other);

  g_return_val_if_fail (command, FALSE);

  /* Cannot unify with a part of a command group.
   * Unify atomic commands only
   */
  if (priv->group_id != 0 || (other && other_priv->group_id != 0))
    return FALSE;

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
glade_command_collapse (GladeCommand *command, GladeCommand *other)
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
  va_list args;

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
    g_critical ("Unbalanced group stack detected in %s\n", G_STRFUNC);
}

gint
glade_command_get_group_depth (void)
{
  return gc_group_depth;
}

static void
glade_command_check_group (GladeCommand *cmd)
{
  GladeCommandPrivate *priv = glade_command_get_instance_private (cmd);

  g_return_if_fail (GLADE_IS_COMMAND (cmd));

  if (gc_group_description)
    {
      priv->description =
          (g_free (priv->description), g_strdup (gc_group_description));
      priv->group_id = gc_group_id;
    }
}


/****************************************************/
/*******  GLADE_COMMAND_PROPERTY_ENABLED      *******/
/****************************************************/
struct _GladeCommandPropertyEnabled
{
  GladeCommand parent;

  GladeProperty *property;
  gboolean old_enabled;
  gboolean new_enabled;
};

/* standard macros */
#define GLADE_TYPE_COMMAND_PROPERTY_ENABLED glade_command_property_enabled_get_type ()
GLADE_MAKE_COMMAND (GladeCommandPropertyEnabled, glade_command_property_enabled, COMMAND_PROPERTY_ENABLED);

static gboolean
glade_command_property_enabled_execute (GladeCommand *cmd)
{
  GladeCommandPropertyEnabled *me = GLADE_COMMAND_PROPERTY_ENABLED (cmd);

  glade_property_set_enabled (me->property, me->new_enabled);

  return TRUE;
}

static gboolean
glade_command_property_enabled_undo (GladeCommand *cmd)
{
  GladeCommandPropertyEnabled *me = GLADE_COMMAND_PROPERTY_ENABLED (cmd);

  glade_property_set_enabled (me->property, me->old_enabled);

  return TRUE;
}

static void
glade_command_property_enabled_finalize (GObject *obj)
{
  GladeCommandPropertyEnabled *me;

  g_return_if_fail (GLADE_IS_COMMAND_PROPERTY_ENABLED (obj));

  me = GLADE_COMMAND_PROPERTY_ENABLED (obj);

  g_object_unref (me->property);
  glade_command_finalize (obj);
}

static gboolean
glade_command_property_enabled_unifies (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
  GladeCommandPropertyEnabled *cmd1;
  GladeCommandPropertyEnabled *cmd2;

  if (!other_cmd)
    {
      if (GLADE_IS_COMMAND_PROPERTY_ENABLED (this_cmd))
        {
          cmd1 = (GladeCommandPropertyEnabled *) this_cmd;

          return (cmd1->old_enabled == cmd1->new_enabled);
        }
      return FALSE;
    }

  if (GLADE_IS_COMMAND_PROPERTY_ENABLED (this_cmd) &&
      GLADE_IS_COMMAND_PROPERTY_ENABLED (other_cmd))
    {
      cmd1 = GLADE_COMMAND_PROPERTY_ENABLED (this_cmd);
      cmd2 = GLADE_COMMAND_PROPERTY_ENABLED (other_cmd);

      return (cmd1->property == cmd2->property);
    }

  return FALSE;
}

static void
glade_command_property_enabled_collapse (GladeCommand *this_cmd,
                                         GladeCommand *other_cmd)
{
  GladeCommandPropertyEnabled *this = GLADE_COMMAND_PROPERTY_ENABLED (this_cmd);
  GladeCommandPrivate *this_priv = glade_command_get_instance_private (this_cmd);
  GladeCommandPropertyEnabled *other = GLADE_COMMAND_PROPERTY_ENABLED (other_cmd);
  GladeWidget *widget;
  GladePropertyDef *pdef;

  this->new_enabled = other->new_enabled;

  widget = glade_property_get_widget (this->property);
  pdef   = glade_property_get_def (this->property);

  g_free (this_priv->description);
  if (this->new_enabled)
    this_priv->description =
      g_strdup_printf (_("Enabling property %s on widget %s"),
                       glade_property_def_get_name (pdef),
                       glade_widget_get_name (widget));
  else
    this_priv->description =
      g_strdup_printf (_("Disabling property %s on widget %s"),
                       glade_property_def_get_name (pdef),
                       glade_widget_get_name (widget));
}

/**
 * glade_command_set_property_enabled:
 * @property: An optional #GladeProperty
 * @enabled: Whether the property should be enabled
 *
 * Enables or disables @property.
 *
 * @property must be an optional property.
 */
void
glade_command_set_property_enabled (GladeProperty *property,
                                    gboolean       enabled)
{
  GladeCommandPropertyEnabled *me;
  GladeCommand *cmd;
  GladeCommandPrivate *cmd_priv;
  GladeWidget *widget;
  GladePropertyDef *pdef;
  gboolean old_enabled;

  /* Sanity checks */
  g_return_if_fail (GLADE_IS_PROPERTY (property));

  widget = glade_property_get_widget (property);
  g_return_if_fail (GLADE_IS_WIDGET (widget));

  /* Only applies to optional properties */
  pdef = glade_property_get_def (property);
  g_return_if_fail (glade_property_def_optional (pdef));

  /* Fetch current state */
  old_enabled = glade_property_get_enabled (property);

  /* Avoid useless command */
  if (old_enabled == enabled)
    return;

  me = g_object_new (GLADE_TYPE_COMMAND_PROPERTY_ENABLED, NULL);
  cmd = GLADE_COMMAND (me);
  cmd_priv = glade_command_get_instance_private (cmd);
  cmd_priv->project = glade_widget_get_project (widget);

  me->property = g_object_ref (property);
  me->new_enabled = enabled;
  me->old_enabled = old_enabled;

  if (enabled)
    cmd_priv->description =
      g_strdup_printf (_("Enabling property %s on widget %s"),
                       glade_property_def_get_name (pdef),
                       glade_widget_get_name (widget));
  else
    cmd_priv->description =
      g_strdup_printf (_("Disabling property %s on widget %s"),
                       glade_property_def_get_name (pdef),
                       glade_widget_get_name (widget));

  glade_command_check_group (GLADE_COMMAND (me));

  if (glade_command_property_enabled_execute (cmd))
    glade_project_push_undo (cmd_priv->project, cmd);
  else
    g_object_unref (G_OBJECT (me));
}

/**************************************************/
/*******     GLADE_COMMAND_SET_PROPERTY     *******/
/**************************************************/

/* create a new GladeCommandSetProperty class.  Objects of this class will
 * encapsulate a "set property" operation */

struct _GladeCommandSetProperty
{
  GladeCommand parent;

  gboolean set_once;
  gboolean undo;
  GList *sdata;
};

/* standard macros */
#define GLADE_TYPE_COMMAND_SET_PROPERTY glade_command_set_property_get_type ()
GLADE_MAKE_COMMAND (GladeCommandSetProperty, glade_command_set_property, COMMAND_SET_PROPERTY);

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
  GladeCommandPrivate *priv = glade_command_get_instance_private (cmd);
  GladeCommandSetProperty *me = (GladeCommandSetProperty *) cmd;
  GList *l;
  gboolean success;
  gboolean retval = FALSE;

  g_return_val_if_fail (me != NULL, FALSE);

  if (me->set_once != FALSE)
    glade_property_push_superuser ();

  for (l = me->sdata; l; l = l->next)
    {
      GValue            new_value = { 0, };
      GladeCommandSetPropData    *sdata = l->data;
      GladePropertyDef *pdef = glade_property_get_def (sdata->property);
      GladeWidget      *widget = glade_property_get_widget (sdata->property);

      g_value_init (&new_value, G_VALUE_TYPE (sdata->new_value));

      if (me->undo)
        g_value_copy (sdata->old_value, &new_value);
      else
        g_value_copy (sdata->new_value, &new_value);

#ifdef GLADE_ENABLE_DEBUG
      if (glade_get_debug_flags () & GLADE_DEBUG_COMMANDS)
        {
          gchar *str =
            glade_widget_adaptor_string_from_value
            (glade_property_def_get_adaptor (pdef), pdef, &new_value);

          g_print ("Setting %s property of %s to %s (sumode: %d)\n",
                   glade_property_def_id (pdef),
                   glade_widget_get_name (widget),
                   str, glade_property_superuser ());

          g_free (str);
        }
#endif

      /* Packing properties need to be refreshed here since
       * they are reset when they get added to containers.
       */
      if (glade_property_def_get_is_packing (pdef))
        {
          GladeProperty *tmp_prop;

          tmp_prop = glade_widget_get_pack_property (widget, glade_property_def_id (pdef));

          if (sdata->property != tmp_prop)
            {
              g_object_unref (sdata->property);
              sdata->property = g_object_ref (tmp_prop);
            }
        }

      /* Make sure the target object has a name for object properties */
      if (glade_property_def_is_object (pdef))
        {
          GObject *pobject = g_value_get_object (&new_value);
          GladeWidget *pwidget;

          if (pobject && (pwidget = glade_widget_get_from_gobject (pobject)))
            glade_widget_ensure_name (pwidget, priv->project, TRUE);
        }

      success = glade_property_set_value (sdata->property, &new_value);
      retval  = retval || success;

      if (!me->set_once && success)
        {
          /* If some verify functions didnt pass on 
           * the first go.. we need to record the actual
           * properties here. XXX should be able to use glade_property_get_value() here
           */
          g_value_copy (glade_property_inline_value (sdata->property), sdata->new_value);
        }

      g_value_unset (&new_value);
    }

  if (me->set_once != FALSE)
    glade_property_pop_superuser ();

  me->set_once = TRUE;
  me->undo = !me->undo;

  return retval;
}

static void
glade_command_set_property_finalize (GObject *obj)
{
  GladeCommandSetProperty *me;
  GList *l;

  me = GLADE_COMMAND_SET_PROPERTY (obj);

  for (l = me->sdata; l; l = l->next)
    {
      GladeCommandSetPropData *sdata = l->data;

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
glade_command_set_property_unifies (GladeCommand *this_cmd,
                                    GladeCommand *other_cmd)
{
  GladeCommandSetProperty *cmd1, *cmd2;
  GladePropertyDef *pdef1, *pdef2;
  GladeCommandSetPropData *pdata1, *pdata2;
  GladeWidget *widget1, *widget2;
  GList *list, *l;

  if (!other_cmd)
    {
      if (GLADE_IS_COMMAND_SET_PROPERTY (this_cmd))
        {
          cmd1 = (GladeCommandSetProperty *) this_cmd;

          for (list = cmd1->sdata; list; list = list->next)
            {
              pdata1  = list->data;
              pdef1 = glade_property_get_def (pdata1->property);

              if (glade_property_def_compare (pdef1,
                                              pdata1->old_value,
                                              pdata1->new_value))
                return FALSE;
            }
          return TRUE;

        }
      return FALSE;
    }

  if (GLADE_IS_COMMAND_SET_PROPERTY (this_cmd) &&
      GLADE_IS_COMMAND_SET_PROPERTY (other_cmd))
    {
      cmd1 = (GladeCommandSetProperty *) this_cmd;
      cmd2 = (GladeCommandSetProperty *) other_cmd;

      if (g_list_length (cmd1->sdata) != g_list_length (cmd2->sdata))
        return FALSE;

      for (list = cmd1->sdata; list; list = list->next)
        {
          pdata1  = list->data;
          pdef1 = glade_property_get_def (pdata1->property);
          widget1 = glade_property_get_widget (pdata1->property);

          for (l = cmd2->sdata; l; l = l->next)
            {
              pdata2  = l->data;
              pdef2 = glade_property_get_def (pdata2->property);
              widget2 = glade_property_get_widget (pdata2->property);

              if (widget1 == widget2 &&
                  glade_property_def_match (pdef1, pdef2))
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
glade_command_set_property_collapse (GladeCommand *this_cmd,
                                     GladeCommand *other_cmd)
{
  GladeCommandSetProperty *cmd1, *cmd2;
  GladeCommandPrivate *this_priv = glade_command_get_instance_private (this_cmd);
  GladeCommandPrivate *other_priv = glade_command_get_instance_private (other_cmd);
  GladeCommandSetPropData *pdata1, *pdata2;
  GladePropertyDef *pdef1, *pdef2;
  GList *list, *l;

  g_return_if_fail (GLADE_IS_COMMAND_SET_PROPERTY (this_cmd) &&
                    GLADE_IS_COMMAND_SET_PROPERTY (other_cmd));

  cmd1 = (GladeCommandSetProperty *) this_cmd;
  cmd2 = (GladeCommandSetProperty *) other_cmd;


  for (list = cmd1->sdata; list; list = list->next)
    {
      pdata1  = list->data;
      pdef1 = glade_property_get_def (pdata1->property);

      for (l = cmd2->sdata; l; l = l->next)
        {
          pdata2  = l->data;
          pdef2 = glade_property_get_def (pdata2->property);

          if (glade_property_def_match (pdef1, pdef2))
            {
              /* Merge the GladeCommandSetPropData structs manually here
               */
              g_value_copy (pdata2->new_value, pdata1->new_value);
              break;
            }
        }
    }

  /* Set the description
   */
  g_free (this_priv->description);
  this_priv->description = other_priv->description;
  other_priv->description = NULL;
}


#define MAX_UNDO_MENU_ITEM_VALUE_LEN 10
static gchar *
glade_command_set_property_description (GladeCommandSetProperty *me)
{
  GladeCommandSetPropData *sdata;
  gchar *description = NULL;
  gchar *value_name;
  GladePropertyDef *pdef;
  GladeWidget *widget;

  g_assert (me->sdata);

  if (g_list_length (me->sdata) > 1)
    description = g_strdup_printf (_("Setting multiple properties"));
  else
    {
      sdata  = me->sdata->data;
      pdef = glade_property_get_def (sdata->property);
      widget = glade_property_get_widget (sdata->property);
      value_name = glade_widget_adaptor_string_from_value
        (glade_property_def_get_adaptor (pdef), pdef, sdata->new_value);

      if (!value_name || strlen (value_name) > MAX_UNDO_MENU_ITEM_VALUE_LEN
          || strchr (value_name, '_'))
        {
          description = g_strdup_printf (_("Setting %s of %s"),
                                         glade_property_def_get_name (pdef),
                                         glade_widget_get_name (widget));
        }
      else
        {
          description = g_strdup_printf (_("Setting %s of %s to %s"),
                                         glade_property_def_get_name (pdef),
                                         glade_widget_get_name (widget),
                                         value_name);
        }
      g_free (value_name);
    }

  return description;
}

/**
 * glade_command_set_properties_list:
 * @project: a #GladeProject
 * @props: (element-type GladeProperty): List of #GladeProperty
 */
void
glade_command_set_properties_list (GladeProject *project, GList *props)
{
  GladeCommandSetProperty *me;
  GladeCommand *cmd;
  GladeCommandPrivate *priv;
  GladeCommandSetPropData *sdata;
  GList *list;
  gboolean success;
  gboolean multiple;

  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (props);

  me = (GladeCommandSetProperty *)
      g_object_new (GLADE_TYPE_COMMAND_SET_PROPERTY, NULL);
  cmd = GLADE_COMMAND (me);
  priv = glade_command_get_instance_private (cmd);
  priv->project = project;

  /* Ref all props */
  for (list = props; list; list = list->next)
    {
      sdata = list->data;
      g_object_ref (G_OBJECT (sdata->property));
    }

  me->sdata = props;
  priv->description = glade_command_set_property_description (me);

  multiple = g_list_length (me->sdata) > 1;
  if (multiple)
    glade_command_push_group ("%s", priv->description);

  glade_command_check_group (cmd);

  /* Push onto undo stack only if it executes successfully. */
  success = glade_command_set_property_execute (cmd);

  if (success)
    glade_project_push_undo (priv->project, cmd);
  else
    g_object_unref (G_OBJECT (me));

  if (multiple)
    glade_command_pop_group ();
}


void
glade_command_set_properties (GladeProperty *property,
                              const GValue *old_value,
                              const GValue *new_value,
                              ...)
{
  GladeCommandSetPropData *sdata;
  GladeProperty *prop;
  GladeWidget   *widget;
  GladeProject  *project;
  GValue *ovalue, *nvalue;
  GList *list = NULL;
  va_list vl;

  g_return_if_fail (GLADE_IS_PROPERTY (property));

  /* Add first set */
  sdata = g_new (GladeCommandSetPropData, 1);
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

      sdata = g_new (GladeCommandSetPropData, 1);
      sdata->property = g_object_ref (GLADE_PROPERTY (prop));
      sdata->old_value = g_new0 (GValue, 1);
      sdata->new_value = g_new0 (GValue, 1);
      g_value_init (sdata->old_value, G_VALUE_TYPE (ovalue));
      g_value_init (sdata->new_value, G_VALUE_TYPE (nvalue));
      g_value_copy (ovalue, sdata->old_value);
      g_value_copy (nvalue, sdata->new_value);
      list = g_list_prepend (list, sdata);
    }
  va_end (vl);

  widget  = glade_property_get_widget (property);
  project = glade_widget_get_project (widget);
  glade_command_set_properties_list (project, list);
}

void
glade_command_set_property_value (GladeProperty *property, const GValue *pvalue)
{

  /* Dont generate undo/redo items for unchanging property values.
   */
  if (glade_property_equals_value (property, pvalue))
    return;

  glade_command_set_properties (property, glade_property_inline_value (property), pvalue, NULL);
}

void
glade_command_set_property (GladeProperty * property, ...)
{
  GValue *value;
  va_list args;

  g_return_if_fail (GLADE_IS_PROPERTY (property));

  va_start (args, property);
  value = glade_property_def_make_gvalue_from_vl (glade_property_get_def (property), args);
  va_end (args);

  glade_command_set_property_value (property, value);
}

/**************************************************/
/*******       GLADE_COMMAND_SET_NAME       *******/
/**************************************************/

/* create a new GladeCommandSetName class.  Objects of this class will
 * encapsulate a "set name" operation */
struct _GladeCommandSetName
{
  GladeCommand parent;

  GladeWidget *widget;
  gchar *old_name;
  gchar *name;
};

/* standard macros */
#define GLADE_TYPE_COMMAND_SET_NAME glade_command_set_name_get_type ()
GLADE_MAKE_COMMAND (GladeCommandSetName, glade_command_set_name, COMMAND_SET_NAME);

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
  GladeCommandPrivate *priv = glade_command_get_instance_private (cmd);
  GladeCommandSetName *me = GLADE_COMMAND_SET_NAME (cmd);
  char *tmp;

  g_return_val_if_fail (me != NULL, TRUE);
  g_return_val_if_fail (me->widget != NULL, TRUE);
  g_return_val_if_fail (me->name != NULL, TRUE);

  glade_project_set_widget_name (priv->project, me->widget, me->name);

  tmp = me->old_name;
  me->old_name = me->name;
  me->name = tmp;

  return TRUE;
}

static void
glade_command_set_name_finalize (GObject *obj)
{
  GladeCommandSetName *me;

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

  if (!other_cmd)
    {
      if (GLADE_IS_COMMAND_SET_NAME (this_cmd))
        {
          cmd1 = (GladeCommandSetName *) this_cmd;

          return (g_strcmp0 (cmd1->old_name, cmd1->name) == 0);
        }
      return FALSE;
    }

  if (GLADE_IS_COMMAND_SET_NAME (this_cmd) &&
      GLADE_IS_COMMAND_SET_NAME (other_cmd))
    {
      cmd1 = GLADE_COMMAND_SET_NAME (this_cmd);
      cmd2 = GLADE_COMMAND_SET_NAME (other_cmd);

      return (cmd1->widget == cmd2->widget);
    }

  return FALSE;
}

static void
glade_command_set_name_collapse (GladeCommand *this_cmd,
                                 GladeCommand *other_cmd)
{
  GladeCommandPrivate *this_priv = glade_command_get_instance_private (this_cmd);
  GladeCommandSetName *nthis = GLADE_COMMAND_SET_NAME (this_cmd);
  GladeCommandSetName *nother = GLADE_COMMAND_SET_NAME (other_cmd);

  g_return_if_fail (GLADE_IS_COMMAND_SET_NAME (this_cmd) &&
                    GLADE_IS_COMMAND_SET_NAME (other_cmd));

  g_free (nthis->old_name);
  nthis->old_name = nother->old_name;
  nother->old_name = NULL;

  g_free (this_priv->description);
  this_priv->description =
      g_strdup_printf (_("Renaming %s to %s"), nthis->name, nthis->old_name);
}

/* this function takes the ownership of name */
void
glade_command_set_name (GladeWidget *widget, const gchar *name)
{
  GladeCommandSetName *me;
  GladeCommand *cmd;
  GladeCommandPrivate *priv;

  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (name && name[0]);

  /* Dont spam the queue with false name changes.
   */
  if (!strcmp (glade_widget_get_name (widget), name))
    return;

  me = g_object_new (GLADE_TYPE_COMMAND_SET_NAME, NULL);
  cmd = GLADE_COMMAND (me);
  priv = glade_command_get_instance_private (cmd);
  priv->project = glade_widget_get_project (widget);

  me->widget = widget;
  me->name = g_strdup (name);
  me->old_name = g_strdup (glade_widget_get_name (widget));

  priv->description =
      g_strdup_printf (_("Renaming %s to %s"), me->old_name, me->name);

  glade_command_check_group (cmd);

  if (glade_command_set_name_execute (cmd))
    glade_project_push_undo (priv->project, cmd);
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

struct _GladeCommandAddRemove
{
  GladeCommand parent;

  GList *widgets;
  gboolean add;
  gboolean from_clipboard;
};

#define GLADE_TYPE_COMMAND_ADD_REMOVE glade_command_add_remove_get_type ()
GLADE_MAKE_COMMAND (GladeCommandAddRemove, glade_command_add_remove, COMMAND_ADD_REMOVE);

static void
glade_command_placeholder_destroyed (GtkWidget *object, CommandData *cdata)
{
  if (GTK_WIDGET (cdata->placeholder) == object)
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
 * get_all_parentless_reffed_widgetst:
 *
 * @props (element-type GladeWidget) : List of #GladeWidget
 * @return (element-type GladeWidget) : List of #GladeWidget
 */
static GList *
get_all_parentless_reffed_widgets (GList *reffed, GladeWidget *widget)
{
  GList *children, *l, *list;
  GladeWidget *child;

  if ((list = glade_widget_get_parentless_reffed_widgets (widget)) != NULL)
    reffed = g_list_concat (reffed, list);

  children = glade_widget_get_children (widget);

  for (l = children; l; l = l->next)
    {
      child  = glade_widget_get_from_gobject (l->data);
      reffed = get_all_parentless_reffed_widgets (reffed, child);
    }

  g_list_free (children);

  return reffed;
}

/**
 * glade_command_add:
 * @widgets: (element-type GladeWidget): a #GList
 * @parent: a #GladeWidget
 * @placeholder: a #GladePlaceholder
 * @project: a #GladeProject
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
                   GladeProject     *project,
                   gboolean          pasting)
{
  GladeCommandAddRemove *me;
  GladeCommand *cmd;
  GladeCommandPrivate *priv;
  CommandData *cdata;
  GladeWidget *widget = NULL;
  GladeWidgetAdaptor *adaptor;
  GList *l, *list, *children, *placeholders = NULL;
  GtkWidget *child;

  g_return_if_fail (widgets && widgets->data);
  g_return_if_fail (parent == NULL || GLADE_IS_WIDGET (parent));

  me = g_object_new (GLADE_TYPE_COMMAND_ADD_REMOVE, NULL);
  cmd = GLADE_COMMAND (me);
  priv = glade_command_get_instance_private (cmd);
  me->add = TRUE;
  me->from_clipboard = pasting;

  /* Things can go wrong in this function if the dataset is inacurate,
   * we make no real attempt here to recover, just g_critical() and
   * fix the bugs as they pop up.
   */
  widget  = GLADE_WIDGET (widgets->data);
  adaptor = glade_widget_get_adaptor (widget);

  if (placeholder && GWA_IS_TOPLEVEL (adaptor) == FALSE)
    priv->project = glade_placeholder_get_project (placeholder);
  else
    priv->project = project;

  priv->description =
      g_strdup_printf (_("Add %s"), g_list_length (widgets) == 1 ?
                       glade_widget_get_name (widget) : _("multiple"));

  for (list = widgets; list && list->data; list = list->next)
    {
      widget = list->data;
      cdata = g_new0 (CommandData, 1);
      if (glade_widget_get_internal (widget))
        g_critical ("Internal widget in Add");

      adaptor = glade_widget_get_adaptor (widget);

      /* Widget */
      cdata->widget = g_object_ref (GLADE_WIDGET (widget));

      /* Parentless ref */
      if ((cdata->reffed =
           get_all_parentless_reffed_widgets (cdata->reffed, widget)) != NULL)
        g_list_foreach (cdata->reffed, (GFunc) g_object_ref, NULL);

      /* Parent */
      if (parent == NULL)
        cdata->parent = glade_widget_get_parent (widget);
      else if (placeholder && GWA_IS_TOPLEVEL (adaptor) == FALSE)
        cdata->parent = glade_placeholder_get_parent (placeholder);
      else
        cdata->parent = parent;

      /* Placeholder */
      if (placeholder != NULL && g_list_length (widgets) == 1)
        {
          glade_command_placeholder_connect (cdata, placeholder);
        }
      else if (cdata->parent &&
               glade_widget_placeholder_relation (cdata->parent, widget))
        {
          if ((children =
               glade_widget_adaptor_get_children (glade_widget_get_adaptor (cdata->parent), 
                                                  glade_widget_get_object (cdata->parent))) != NULL)
            {
              for (l = children; l && l->data; l = l->next)
                {
                  child = l->data;

                  /* Find a placeholder for this child, ignore special child types */
                  if (GLADE_IS_PLACEHOLDER (child) &&
                      g_object_get_data (G_OBJECT (child), "special-child-type") == NULL &&
                      g_list_find (placeholders, child) == NULL)
                    {
                      placeholders = g_list_append (placeholders, child);
                      glade_command_placeholder_connect (cdata, GLADE_PLACEHOLDER (child));
                      break;
                    }
                }
              g_list_free (children);
            }
        }

      me->widgets = g_list_prepend (me->widgets, cdata);
    }

  glade_command_check_group (cmd);

  /*
   * Push it onto the undo stack only on success
   */
  if (glade_command_add_remove_execute (cmd))
    glade_project_push_undo (priv->project, cmd);
  else
    g_object_unref (G_OBJECT (me));

  if (placeholders)
    g_list_free (placeholders);

}

static void
glade_command_delete_prop_refs (GladeWidget *widget)
{
  GladeProperty *property;
  GList         *refs, *l;

  refs = glade_widget_list_prop_refs (widget);
  for (l = refs; l; l = l->next)
    {
      property = l->data;
      glade_command_set_property (property, NULL);
    }
  g_list_free (refs);
}

static void glade_command_remove (GList *widgets);

static void
glade_command_remove_locked (GladeWidget *widget, GList *reffed)
{
  GList list = { 0, }, *widgets, *l;
  GladeWidget *locked;

  widgets = glade_widget_list_locked_widgets (widget);

  for (l = widgets; l; l = l->next)
    {
      locked = l->data;
      list.data = locked;

      if (g_list_find (reffed, locked))
        continue;

      glade_command_unlock_widget (locked);
      glade_command_remove (&list);
    }

  g_list_free (widgets);
}

/**
 * glade_command_remove:
 * @widgets: (element-type GladeWidget): a #GList of #GladeWidgets
 * @return_placeholders: whether or not to return a list of placehodlers
 *
 * Performs a remove command on all widgets in @widgets from @parent.
 */
static void
glade_command_remove (GList *widgets)
{
  GladeCommandAddRemove *me;
  GladeCommand *cmd;
  GladeCommandPrivate *priv;
  GladeWidget *widget = NULL;
  GladeWidget *lock;
  CommandData *cdata;
  GtkWidget *placeholder;
  GList *list, *l;

  g_return_if_fail (widgets != NULL);

  /* internal children cannot be deleted. Notify the user. */
  for (list = widgets; list && list->data; list = list->next)
    {
      widget = list->data;
      lock   = glade_widget_get_locker (widget);

      if (glade_widget_get_internal (widget))
        {
          glade_util_ui_message (glade_app_get_window (),
                                 GLADE_UI_WARN, NULL,
                                 _
                                 ("You cannot remove a widget internal to a composite widget."));
          return;
        }
      else if (lock)
        {
          glade_util_ui_message (glade_app_get_window (),
                                 GLADE_UI_WARN, NULL,
                                 _("%s is locked by %s, edit %s first."),
                                 glade_widget_get_name (widget), 
                                 glade_widget_get_name (lock),
                                 glade_widget_get_name (lock));
          return;
        }
    }

  me = g_object_new (GLADE_TYPE_COMMAND_ADD_REMOVE, NULL);
  me->add = FALSE;
  me->from_clipboard = FALSE;
  cmd = GLADE_COMMAND (me);
  priv = glade_command_get_instance_private (cmd);

  priv->project = glade_widget_get_project (widget);
  priv->description = g_strdup ("dummy");

  if (g_list_length (widgets) == 1)
    glade_command_push_group (_("Remove %s"),
                              glade_widget_get_name (GLADE_WIDGET (widgets->data)));
  else
    glade_command_push_group (_("Remove multiple"));

  for (list = widgets; list && list->data; list = list->next)
    {
      widget = list->data;

      cdata = g_new0 (CommandData, 1);
      cdata->widget = g_object_ref (GLADE_WIDGET (widget));
      cdata->parent = glade_widget_get_parent (widget);

      if ((cdata->reffed =
           get_all_parentless_reffed_widgets (cdata->reffed, widget)) != NULL)
        g_list_foreach (cdata->reffed, (GFunc) g_object_ref, NULL);

      /* If we're removing the template widget, then we need to unset it as template */
      if (glade_project_get_template (priv->project) == widget)
        glade_command_set_project_template (priv->project, NULL);

      /* Undoably unset any object properties that may point to the removed object */
      glade_command_delete_prop_refs (widget);

      /* Undoably unlock and remove any widgets locked by this widget */
      glade_command_remove_locked (widget, cdata->reffed);

      if (cdata->parent != NULL &&
          glade_widget_placeholder_relation (cdata->parent, cdata->widget))
        {
          placeholder = glade_placeholder_new ();
          glade_command_placeholder_connect
              (cdata, GLADE_PLACEHOLDER (placeholder));
        }
      me->widgets = g_list_prepend (me->widgets, cdata);

      /* Record packing props if not deleted from the clipboard */
      if (me->from_clipboard == FALSE)
        {
          for (l = glade_widget_get_packing_properties (widget); l; l = l->next)
            cdata->pack_props =
              g_list_prepend (cdata->pack_props,
                              glade_property_dup (GLADE_PROPERTY (l->data),
                                                  cdata->widget));
        }
    }

  g_assert (widget);

  glade_command_check_group (cmd);

  if (glade_command_add_remove_execute (cmd))
    glade_project_push_undo (priv->project, cmd);
  else
    g_object_unref (G_OBJECT (me));

  glade_command_pop_group ();
}                               /* end of glade_command_remove() */

static void
glade_command_transfer_props (GladeWidget *gnew, GList *saved_props)
{
  GList *l;

  for (l = saved_props; l; l = l->next)
    {
      GladeProperty *prop, *sprop = l->data;
      GladePropertyDef *pdef = glade_property_get_def (sprop);

      prop = glade_widget_get_pack_property (gnew, glade_property_def_id (pdef));

      if (prop && glade_property_def_transfer_on_paste (pdef) &&
          glade_property_def_match (glade_property_get_def (prop), pdef))
        glade_property_set_value (prop, glade_property_inline_value (sprop));
    }
}

static gboolean
glade_command_add_execute (GladeCommandAddRemove *me)
{
  GladeCommandPrivate *priv = glade_command_get_instance_private ((GladeCommand *) me);
  CommandData *cdata;
  GList *list, *l, *saved_props;
  gchar *special_child_type;

  if (me->widgets)
    {
      glade_project_selection_clear (priv->project, FALSE);

      for (list = me->widgets; list && list->data; list = list->next)
        {
          cdata = list->data;
          saved_props = NULL;

          GLADE_NOTE (COMMANDS,
                      g_print ("Adding widget '%s' to parent '%s' "
                               "(from clipboard: %s, props recorded: %s, have placeholder: %s, child_type: %s)\n",
                               glade_widget_get_name (cdata->widget),
                               cdata->parent ? glade_widget_get_name (cdata->parent) : "(none)",
                               me->from_clipboard ? "yes" : "no",
                               cdata->props_recorded ? "yes" : "no",
                               cdata->placeholder ? "yes" : "no",
                               cdata->special_type));

          if (cdata->parent != NULL)
            {
              /* Prepare special-child-type for the paste. */
              if (me->from_clipboard)
                {
                  /* Only transfer properties when they are from the clipboard,
                   * otherwise prioritize packing defaults. 
                   */
                  saved_props =
                      glade_widget_dup_properties (cdata->widget,
                                                   glade_widget_get_packing_properties (cdata->widget), 
                                                   FALSE, FALSE, FALSE);

                  glade_widget_set_packing_properties (cdata->widget, cdata->parent);
                }

              /* Clear it the first time around, ensure we record it after adding */
              if (cdata->props_recorded == FALSE)
                g_object_set_data (glade_widget_get_object (cdata->widget),
                                   "special-child-type", NULL);
              else
                g_object_set_data_full (glade_widget_get_object (cdata->widget),
                                        "special-child-type",
                                        g_strdup (cdata->special_type),
                                        g_free);

              /* glade_command_paste ganauntees that if 
               * there we are pasting to a placeholder, 
               * there is only one widget.
               */
              if (cdata->placeholder)
                glade_widget_replace (cdata->parent,
                                      G_OBJECT (cdata->placeholder), 
                                      glade_widget_get_object (cdata->widget));
              else
                glade_widget_add_child (cdata->parent,
                                        cdata->widget,
                                        cdata->props_recorded == FALSE);

              glade_command_transfer_props (cdata->widget, saved_props);

              if (saved_props)
                {
                  g_list_foreach (saved_props, (GFunc) g_object_unref, NULL);
                  g_list_free (saved_props);
                }

              /* Now that we've added, apply any packing props if nescisary. */
              for (l = cdata->pack_props; l; l = l->next)
                {
                  GValue            value = { 0, };
                  GladeProperty    *saved_prop = l->data;
                  GladePropertyDef *pdef = glade_property_get_def (saved_prop);
                  GladeProperty    *widget_prop =
                    glade_widget_get_pack_property (cdata->widget, glade_property_def_id (pdef));

                  glade_property_get_value (saved_prop, &value);
                  glade_property_set_value (widget_prop, &value);
                  glade_property_sync (widget_prop);
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
                  for (l = glade_widget_get_packing_properties (cdata->widget); l; l = l->next)
                    cdata->pack_props = 
                      g_list_prepend (cdata->pack_props,
                                      glade_property_dup (GLADE_PROPERTY (l->data), 
                                                          cdata->widget));

                  /* Record the special-type here after replacing */
                  if ((special_child_type =
                       g_object_get_data (glade_widget_get_object (cdata->widget),
                                          "special-child-type")) != NULL)
                    {
                      g_free (cdata->special_type);
                      cdata->special_type = g_strdup (special_child_type);
                    }

                  GLADE_NOTE (COMMANDS,
                              g_print ("Recorded properties for adding widget '%s' to parent '%s' (special child: %s)\n",
                                       glade_widget_get_name (cdata->widget),
                                       cdata->parent ? glade_widget_get_name (cdata->parent) : "(none)",
                                       cdata->special_type));

                  /* Mark the properties as recorded */
                  cdata->props_recorded = TRUE;
                }
            }

          glade_project_add_object (priv->project,
                                    glade_widget_get_object (cdata->widget));

          for (l = cdata->reffed; l; l = l->next)
            {
              GladeWidget *reffed = l->data;
              glade_project_add_object (priv->project,
                                        glade_widget_get_object (reffed));
            }

          glade_project_selection_add (priv->project, 
                                       glade_widget_get_object (cdata->widget), FALSE);

          glade_widget_show (cdata->widget);
        }

      glade_project_queue_selection_changed (priv->project);
    }
  return TRUE;
}                               /* end of glade_command_add_execute() */

static gboolean
glade_command_remove_execute (GladeCommandAddRemove *me)
{
  GladeCommandPrivate *priv = glade_command_get_instance_private ((GladeCommand *) me);
  CommandData *cdata;
  GladeWidget *reffed;
  GList *list, *l;
  gchar *special_child_type;

  for (list = me->widgets; list && list->data; list = list->next)
    {
      cdata = list->data;

      GLADE_NOTE (COMMANDS,
                  g_print ("Removing widget '%s' from parent '%s' "
                           "(from clipboard: %s, props recorded: %s, have placeholder: %s, child_type: %s)\n",
                           glade_widget_get_name (cdata->widget),
                           cdata->parent ? glade_widget_get_name (cdata->parent) : "(none)",
                           me->from_clipboard ? "yes" : "no",
                           cdata->props_recorded ? "yes" : "no",
                           cdata->placeholder ? "yes" : "no",
                           cdata->special_type));

      glade_widget_hide (cdata->widget);

      if (cdata->props_recorded == FALSE)
        {
          /* Record the special-type here after replacing */
          if ((special_child_type =
               g_object_get_data (glade_widget_get_object (cdata->widget),
                                  "special-child-type")) != NULL)
            {
              g_free (cdata->special_type);
              cdata->special_type = g_strdup (special_child_type);
            }

          GLADE_NOTE (COMMANDS,
                      g_print ("Recorded properties for removing widget '%s' from parent '%s' (special child: %s)\n",
                               glade_widget_get_name (cdata->widget),
                               cdata->parent ? glade_widget_get_name (cdata->parent) : "(none)",
                               cdata->special_type));

          /* Mark the properties as recorded */
          cdata->props_recorded = TRUE;
        }

      glade_project_remove_object (priv->project,
                                   glade_widget_get_object (cdata->widget));

      for (l = cdata->reffed; l; l = l->next)
        {
          reffed = l->data;
          glade_project_remove_object (priv->project,
                                       glade_widget_get_object (reffed));
        }

      if (cdata->parent)
        {
          if (cdata->placeholder)
            glade_widget_replace (cdata->parent, glade_widget_get_object (cdata->widget),
                                  G_OBJECT (cdata->placeholder));
          else
            glade_widget_remove_child (cdata->parent, cdata->widget);
        }
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
  GladeCommandAddRemove *me = (GladeCommandAddRemove *) cmd;
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
  GladeCommandAddRemove *cmd;
  CommandData *cdata;
  GList *list;

  g_return_if_fail (GLADE_IS_COMMAND_ADD_REMOVE (obj));

  cmd = GLADE_COMMAND_ADD_REMOVE (obj);

  for (list = cmd->widgets; list && list->data; list = list->next)
    {
      cdata = list->data;

      if (cdata->placeholder)
        {
          if (cdata->handler_id)
            g_signal_handler_disconnect (cdata->placeholder, cdata->handler_id);
          if (g_object_is_floating (G_OBJECT (cdata->placeholder)))
            gtk_widget_destroy (GTK_WIDGET (cdata->placeholder));
        }

      if (cdata->widget)
        g_object_unref (G_OBJECT (cdata->widget));

      g_list_foreach (cdata->reffed, (GFunc) g_object_unref, NULL);
      g_list_free (cdata->reffed);
    }
  g_list_free (cmd->widgets);

  glade_command_finalize (obj);
}

static gboolean
glade_command_add_remove_unifies (GladeCommand *this_cmd,
                                  GladeCommand *other_cmd)
{
  return FALSE;
}

static void
glade_command_add_remove_collapse (GladeCommand *this_cmd,
                                   GladeCommand *other_cmd)
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
 * @adaptor: A #GladeWidgetAdaptor
 * @parent: (nullable): the parent #GladeWidget to add the new widget to.
 * @placeholder: (nullable): the placeholder which will be substituted by the widget
 * @project: the project his widget belongs to.
 *
 * Creates a new widget using @adaptor and put in place of the @placeholder
 * in the @project
 *
 * Returns: (transfer full): the newly created widget.
 */
GladeWidget *
glade_command_create (GladeWidgetAdaptor *adaptor,
                      GladeWidget *parent,
                      GladePlaceholder *placeholder,
                      GladeProject *project)
{
  GladeWidget *widget;
  GList *widgets = NULL;

  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  /* attempt to create the widget -- widget may be null, e.g. the user clicked cancel on a query */
  widget = glade_widget_adaptor_create_widget (adaptor, TRUE,
                                               "parent", parent,
                                               "project", project, NULL);
  if (widget == NULL)
    {
      return NULL;
    }

  if (parent && !glade_widget_add_verify (parent, widget, TRUE))
    {
      g_object_ref_sink (widget);
      g_object_unref (widget);
      return NULL;
    }

  widgets = g_list_prepend (widgets, widget);
  glade_command_push_group (_("Create %s"), glade_widget_get_name (widget));
  glade_command_add (widgets, parent, placeholder, project, FALSE);
  glade_command_pop_group ();

  g_list_free (widgets);

  /* Make selection change immediately when a widget is created */
  glade_project_selection_changed (project);

  return widget;
}

/**
 * glade_command_delete:
 * @widgets: (element-type GladeWidget): a #GList of #GladeWidgets
 *
 * Performs a delete command on the list of widgets.
 */
void
glade_command_delete (GList *widgets)
{
  GladeWidget *widget;

  g_return_if_fail (widgets != NULL);

  widget = widgets->data;
  glade_command_push_group (_("Delete %s"),
                            g_list_length (widgets) == 1 ? 
                            glade_widget_get_name (widget) : _("multiple"));
  glade_command_remove (widgets);
  glade_command_pop_group ();
}

/**
 * glade_command_cut:
 * @widgets: (element-type GladeWidget): a #GList of #GladeWidgets
 *
 * Removes the list of widgets and adds them to the clipboard.
 */
void
glade_command_cut (GList *widgets)
{
  GladeWidget *widget;
  GList *l;

  g_return_if_fail (widgets != NULL);

  for (l = widgets; l; l = l->next)
    g_object_set_data (G_OBJECT (l->data), "glade-command-was-cut",
                       GINT_TO_POINTER (TRUE));

  widget = widgets->data;
  glade_command_push_group (_("Cut %s"),
                            g_list_length (widgets) == 1 ? 
                            glade_widget_get_name (widget) : _("multiple"));
  glade_command_remove (widgets);
  glade_command_pop_group ();

  glade_clipboard_add (glade_app_get_clipboard (), widgets);
}

#if 0
static void
glade_command_break_references_for_widget (GladeWidget *widget, GList *widgets)
{
  GList *l, *children;

  for (l = widget->properties; l; l = l->next)
    {
      property = l->data;

      if (glade_property_def_is_object (property->klass) &&
          property->klass->parentless_widget == FALSE)
        {
          GList *obj_list;
          GObject *reffed_obj = NULL;
          GladeWidget *reffed_widget;

          if (GLADE_IS_PARAM_SPEC_OBJECTS (klass->pspec))
            {
              glade_property_get (property, &obj_list);

            }
          else
            {
              glade_property_get (property, &reffed_obj);
            }
        }
    }

  children = glade_widget_adaptor_get_children (widget->adaptor,
                                                widget->object);

  for (l = children; l; l = l->next)
    {
      if ((child = glade_widget_get_from_gobject (l->data)) != NULL)
        glade_command_break_references_for_widget (child, widgets);
    }

  g_list_free (children);
}

static void
glade_command_break_references (GladeProject *project, GList *widgets)
{
  GList *list;
  GladeWidget *widget;

  for (list = widgets; list && list->data; list = list->next)
    {
      widget = l->data;

      if (project == widget->project)
        continue;

      glade_command_break_references_for_widget (widget, widgets);
    }


}
#endif

/**
 * glade_command_paste:
 * @widgets: (element-type GladeWidget): a #GList of #GladeWidget
 * @parent: (allow-none): a #GladeWidget
 * @placeholder: (allow-none): a #GladePlaceholder
 * @project: a #GladeProject
 *
 * Performs a paste command on all widgets in @widgets to @parent, possibly
 * replacing @placeholder (note toplevels dont need a parent; the active project
 * will be used when pasting toplevel objects).
 */
void
glade_command_paste (GList *widgets,
                     GladeWidget *parent,
                     GladePlaceholder *placeholder,
                     GladeProject *project)
{
  GList *list, *copied_widgets = NULL;
  GladeWidget *copied_widget = NULL;
  gboolean exact;

  g_return_if_fail (widgets != NULL);

  for (list = widgets; list && list->data; list = list->next)
    {
      exact =
          GPOINTER_TO_INT (g_object_get_data
                           (G_OBJECT (list->data), "glade-command-was-cut"));

      copied_widget = glade_widget_dup (list->data, exact);
      copied_widgets = g_list_prepend (copied_widgets, copied_widget);
    }

  glade_command_push_group (_("Paste %s"),
                            g_list_length (widgets) == 1 ? 
                            glade_widget_get_name (copied_widget) : _("multiple"));

  glade_command_add (copied_widgets, parent, placeholder, project, TRUE);
  glade_command_pop_group ();

  if (copied_widgets)
    g_list_free (copied_widgets);
}

/**
 * glade_command_dnd:
 * @widgets: (element-type GladeWidget): a #GList of #GladeWidget
 * @parent: (allow-none): a #GladeWidget
 * @placeholder: (allow-none): a #GladePlaceholder
 *
 * Performs a drag-n-drop command, i.e. removes the list of widgets and adds them 
 * to the new parent, possibly replacing @placeholder (note toplevels dont need a 
 * parent; the active project will be used when pasting toplevel objects).
 */
void
glade_command_dnd (GList *widgets,
                   GladeWidget *parent,
                   GladePlaceholder *placeholder)
{
  GladeWidget *widget;
  GladeProject *project;

  g_return_if_fail (widgets != NULL);

  widget = widgets->data;
  
  if (parent)
    project = glade_widget_get_project (parent);
  else if (placeholder)
    project = glade_placeholder_get_project (placeholder);
  else
    project = glade_widget_get_project (widget);

  g_return_if_fail (project);
  
  glade_command_push_group (_("Drag %s and Drop to %s"),
                            g_list_length (widgets) == 1 ? 
                            glade_widget_get_name (widget) : _("multiple"),
                            parent ? glade_widget_get_name (parent) : _("root"));
  glade_command_remove (widgets);
  glade_command_add (widgets, parent, placeholder, project, TRUE);
  glade_command_pop_group ();
}

/*********************************************************/
/*******     GLADE_COMMAND_ADD_SIGNAL     *******/
/*********************************************************/

/* create a new GladeCommandAddRemoveChangeSignal class.  Objects of this class will
 * encapsulate an "add or remove signal handler" operation */
typedef enum
{
  GLADE_ADD,
  GLADE_REMOVE,
  GLADE_CHANGE
} GladeAddType;

struct _GladeCommandAddSignal
{
  GladeCommand parent;

  GladeWidget *widget;

  GladeSignal *signal;
  GladeSignal *new_signal;

  GladeAddType type;
};

/* standard macros */
#define GLADE_TYPE_COMMAND_ADD_SIGNAL glade_command_add_signal_get_type ()
GLADE_MAKE_COMMAND (GladeCommandAddSignal, glade_command_add_signal, COMMAND_ADD_SIGNAL);

static void
glade_command_add_signal_finalize (GObject *obj)
{
  GladeCommandAddSignal *cmd = GLADE_COMMAND_ADD_SIGNAL (obj);

  g_object_unref (cmd->widget);

  if (cmd->signal)
    g_object_unref (cmd->signal);
  if (cmd->new_signal)
    g_object_unref (cmd->new_signal);

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
  GladeSignal *temp;

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
                                            cmd->signal, cmd->new_signal);
        temp = cmd->signal;
        cmd->signal = cmd->new_signal;
        cmd->new_signal = temp;
        break;
      default:
        break;
    }
  return TRUE;
}

static gboolean
glade_command_add_signal_unifies (GladeCommand *this_cmd,
                                  GladeCommand *other_cmd)
{
  return FALSE;
}

static void
glade_command_add_signal_collapse (GladeCommand *this_cmd,
                                   GladeCommand *other_cmd)
{
  g_return_if_reached ();
}

static void
glade_command_add_remove_change_signal (GladeWidget *glade_widget,
                                        const GladeSignal *signal,
                                        const GladeSignal *new_signal,
                                        GladeAddType type)
{
  GladeCommandAddSignal *me = GLADE_COMMAND_ADD_SIGNAL
      (g_object_new (GLADE_TYPE_COMMAND_ADD_SIGNAL, NULL));
  GladeCommand *cmd = GLADE_COMMAND (me);
  GladeCommandPrivate *priv = glade_command_get_instance_private (cmd);

  /* we can only add/remove a signal to a widget that has been wrapped by a GladeWidget */
  g_assert (glade_widget != NULL);
  g_assert (glade_widget_get_project (glade_widget) != NULL);

  me->widget = g_object_ref (glade_widget);
  me->type = type;
  me->signal = glade_signal_clone (signal);
  me->new_signal = new_signal ? glade_signal_clone (new_signal) : NULL;

  priv->project = glade_widget_get_project (glade_widget);
  priv->description =
      g_strdup_printf (type == GLADE_ADD ? _("Add signal handler %s") :
                       type == GLADE_REMOVE ? _("Remove signal handler %s") :
                       _("Change signal handler %s"), 
                       glade_signal_get_handler ((GladeSignal *)signal));

  glade_command_check_group (cmd);

  if (glade_command_add_signal_execute (cmd))
    glade_project_push_undo (priv->project, cmd);
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
glade_command_remove_signal (GladeWidget *glade_widget,
                             const GladeSignal *signal)
{
  glade_command_add_remove_change_signal
      (glade_widget, signal, NULL, GLADE_REMOVE);
}

/**
 * glade_command_change_signal:
 * @glade_widget: a #GladeWidget
 * @old_signal: a #GladeSignal
 * @new_signal: a #GladeSignal
 *
 * TODO: write me
 */
void
glade_command_change_signal (GladeWidget *glade_widget,
                             const GladeSignal *old_signal,
                             const GladeSignal *new_signal)
{
  glade_command_add_remove_change_signal
      (glade_widget, old_signal, new_signal, GLADE_CHANGE);
}

/******************************************************************************
 * 
 * set i18n metadata
 * 
 * This command sets the i18n metadata on a label property.
 * 
 *****************************************************************************/

struct _GladeCommandSetI18n
{
  GladeCommand parent;

  GladeProperty *property;
  gboolean translatable;
  gchar *context;
  gchar *comment;
  gboolean old_translatable;
  gchar *old_context;
  gchar *old_comment;
};

#define GLADE_TYPE_COMMAND_SET_I18N glade_command_set_i18n_get_type ()
GLADE_MAKE_COMMAND (GladeCommandSetI18n, glade_command_set_i18n, COMMAND_SET_I18N);

static gboolean
glade_command_set_i18n_execute (GladeCommand *cmd)
{
  GladeCommandSetI18n *me = (GladeCommandSetI18n *) cmd;
  gboolean temp_translatable;
  gchar *temp_context;
  gchar *temp_comment;

  /* sanity check */
  g_return_val_if_fail (me != NULL, TRUE);
  g_return_val_if_fail (me->property != NULL, TRUE);

  /* set the new values in the property */
  glade_property_i18n_set_translatable (me->property, me->translatable);
  glade_property_i18n_set_context (me->property, me->context);
  glade_property_i18n_set_comment (me->property, me->comment);

  /* swap the current values with the old values to prepare for undo */
  temp_translatable = me->translatable;
  temp_context = me->context;
  temp_comment = me->comment;
  me->translatable = me->old_translatable;
  me->context = me->old_context;
  me->comment = me->old_comment;
  me->old_translatable = temp_translatable;
  me->old_context = temp_context;
  me->old_comment = temp_comment;

  return TRUE;
}

static gboolean
glade_command_set_i18n_undo (GladeCommand *cmd)
{
  return glade_command_set_i18n_execute (cmd);
}

static void
glade_command_set_i18n_finalize (GObject *obj)
{
  GladeCommandSetI18n *me;

  g_return_if_fail (GLADE_IS_COMMAND_SET_I18N (obj));

  me = GLADE_COMMAND_SET_I18N (obj);
  g_free (me->context);
  g_free (me->comment);
  g_free (me->old_context);
  g_free (me->old_comment);

  glade_command_finalize (obj);
}

static gboolean
glade_command_set_i18n_unifies (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
  GladeCommandSetI18n *cmd1;
  GladeCommandSetI18n *cmd2;

  if (GLADE_IS_COMMAND_SET_I18N (this_cmd) &&
      GLADE_IS_COMMAND_SET_I18N (other_cmd))
    {
      cmd1 = GLADE_COMMAND_SET_I18N (this_cmd);
      cmd2 = GLADE_COMMAND_SET_I18N (other_cmd);

      return (cmd1->property == cmd2->property);
    }

  return FALSE;
}

static void
glade_command_set_i18n_collapse (GladeCommand *this_cmd,
                                 GladeCommand *other_cmd)
{
  /* this command is the one that will be used for an undo of the sequence of like commands */
  GladeCommandSetI18n *this = GLADE_COMMAND_SET_I18N (this_cmd);

  /* the other command contains the values that will be used for a redo */
  GladeCommandSetI18n *other = GLADE_COMMAND_SET_I18N (other_cmd);

  g_return_if_fail (GLADE_IS_COMMAND_SET_I18N (this_cmd) &&
                    GLADE_IS_COMMAND_SET_I18N (other_cmd));

  /* adjust this command to contain, as its old values, the other command's current values */
  this->old_translatable = other->old_translatable;
  g_free (this->old_context);
  g_free (this->old_comment);
  this->old_context = other->old_context;
  this->old_comment = other->old_comment;
  other->old_context = NULL;
  other->old_comment = NULL;
}

/**
 * glade_command_set_i18n:
 * @property: a #GladeProperty
 * @translatable: a #gboolean
 * @context: a #const gchar *
 * @comment: a #const gchar *
 *
 * Sets the i18n data on the property.
 */
void
glade_command_set_i18n (GladeProperty *property,
                        gboolean translatable,
                        const gchar *context,
                        const gchar *comment)
{
  GladeCommandSetI18n *me;
  GladeCommand *cmd;
  GladeCommandPrivate *priv;

  g_return_if_fail (property);

  /* check that something changed before continuing with the command */
  if (translatable == glade_property_i18n_get_translatable (property) &&
      !g_strcmp0 (glade_property_i18n_get_context (property), context) &&
      !g_strcmp0 (glade_property_i18n_get_comment (property), comment))
    return;

  /* load up the command */
  me = g_object_new (GLADE_TYPE_COMMAND_SET_I18N, NULL);
  me->property = property;
  me->translatable = translatable;
  me->context = g_strdup (context);
  me->comment = g_strdup (comment);
  me->old_translatable = glade_property_i18n_get_translatable (property);
  me->old_context = g_strdup (glade_property_i18n_get_context (property));
  me->old_comment = g_strdup (glade_property_i18n_get_comment (property));

  cmd = GLADE_COMMAND (me);
  priv = glade_command_get_instance_private (cmd);
  priv->project = glade_widget_get_project (glade_property_get_widget (property));
  priv->description = g_strdup_printf (_("Setting i18n metadata"));

  glade_command_check_group (cmd);

  /* execute the command and push it on the stack if successful */
  if (glade_command_set_i18n_execute (cmd))
    glade_project_push_undo (priv->project, cmd);
  else
    g_object_unref (G_OBJECT (me));
}

/******************************************************************************
 * 
 * This command sets protection warnings on widgets
 * 
 *****************************************************************************/

struct _GladeCommandLock
{
  GladeCommand parent;

  GladeWidget *widget;
  GladeWidget *locked;
  gboolean locking;
};

#define GLADE_TYPE_COMMAND_LOCK glade_command_lock_get_type ()
GLADE_MAKE_COMMAND (GladeCommandLock, glade_command_lock, COMMAND_LOCK);

static gboolean
glade_command_lock_execute (GladeCommand *cmd)
{
  GladeCommandLock *me = (GladeCommandLock *) cmd;

  /* set the new policy */
  if (me->locking)
    glade_widget_lock (me->widget, me->locked);
  else
    glade_widget_unlock (me->locked);

  /* swap the current values with the old values to prepare for undo */
  me->locking = !me->locking;

  return TRUE;
}

static gboolean
glade_command_lock_undo (GladeCommand *cmd)
{
  return glade_command_lock_execute (cmd);
}

static void
glade_command_lock_finalize (GObject *obj)
{
  GladeCommandLock *me = (GladeCommandLock *) obj;

  g_object_unref (me->widget);
  g_object_unref (me->locked);

  glade_command_finalize (obj);
}

static gboolean
glade_command_lock_unifies (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
/* GladeCommandLock *cmd1; */
/* GladeCommandLock *cmd2; */
  /* No point here, this command undoubtedly always runs in groups */
  return FALSE;
}

static void
glade_command_lock_collapse (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
  /* this command is the one that will be used for an undo of the sequence of like commands */
  //GladeCommandLock *this = GLADE_COMMAND_LOCK (this_cmd);

  /* the other command contains the values that will be used for a redo */
  //GladeCommandLock *other = GLADE_COMMAND_LOCK (other_cmd);

  g_return_if_fail (GLADE_IS_COMMAND_LOCK (this_cmd) &&
                    GLADE_IS_COMMAND_LOCK (other_cmd));

  /* no unify/collapse */
}

/**
 * glade_command_lock_widget:
 * @widget: A #GladeWidget
 * @locked: The #GladeWidget to lock
 *
 * Sets @locked to be in a locked up state
 * spoken for by @widget, locked widgets cannot
 * be removed from the project until unlocked.
 */
void
glade_command_lock_widget (GladeWidget *widget, GladeWidget *locked)
{
  GladeCommandLock *me;
  GladeCommand *cmd;
  GladeCommandPrivate *priv;

  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (GLADE_IS_WIDGET (locked));
  g_return_if_fail (glade_widget_get_locker (locked) == NULL);

  /* load up the command */
  me = g_object_new (GLADE_TYPE_COMMAND_LOCK, NULL);
  me->widget = g_object_ref (widget);
  me->locked = g_object_ref (locked);
  me->locking = TRUE;

  cmd = GLADE_COMMAND (me);
  priv = glade_command_get_instance_private (cmd);
  priv->project = glade_widget_get_project (widget);
  priv->description =
    g_strdup_printf (_("Locking %s by widget %s"), 
                     glade_widget_get_name (locked),
                     glade_widget_get_name (widget));

  glade_command_check_group (cmd);

  /* execute the command and push it on the stack if successful 
   * this sets the actual policy
   */
  if (glade_command_lock_execute (cmd))
    glade_project_push_undo (priv->project, cmd);
  else
    g_object_unref (G_OBJECT (me));

}


/**
 * glade_command_unlock_widget:
 * @widget: A #GladeWidget
 *
 * Unlocks @widget so that it can be removed
 * from the project again
 *
 */
void
glade_command_unlock_widget (GladeWidget *widget)
{
  GladeCommandLock *me;
  GladeCommand *cmd;
  GladeCommandPrivate *priv;

  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (GLADE_IS_WIDGET (glade_widget_get_locker (widget)));

  /* load up the command */
  me = g_object_new (GLADE_TYPE_COMMAND_LOCK, NULL);
  me->widget = g_object_ref (glade_widget_get_locker (widget));
  me->locked = g_object_ref (widget);
  me->locking = FALSE;

  cmd = GLADE_COMMAND (me);
  priv = glade_command_get_instance_private (cmd);
  priv->project = glade_widget_get_project (widget);
  priv->description =
    g_strdup_printf (_("Unlocking %s"), glade_widget_get_name (widget));

  glade_command_check_group (cmd);

  /* execute the command and push it on the stack if successful 
   * this sets the actual policy
   */
  if (glade_command_lock_execute (cmd))
    glade_project_push_undo (priv->project, cmd);
  else
    g_object_unref (G_OBJECT (me));

}


/******************************************************************************
 * 
 * This command sets the target version of a GladeProject
 * 
 *****************************************************************************/
struct _GladeCommandTarget
{
  GladeCommand parent;

  gchar       *catalog;
  gint         old_major;
  gint         old_minor;
  gint         new_major;
  gint         new_minor;
};

#define GLADE_TYPE_COMMAND_TARGET glade_command_target_get_type ()
GLADE_MAKE_COMMAND (GladeCommandTarget, glade_command_target, COMMAND_TARGET);

static gboolean
glade_command_target_execute (GladeCommand *cmd)
{
  GladeCommandPrivate *priv = glade_command_get_instance_private (cmd);
  GladeCommandTarget *me = (GladeCommandTarget *) cmd;

  glade_project_set_target_version (priv->project,
                                    me->catalog,
                                    me->new_major,
                                    me->new_minor);

  return TRUE;
}

static gboolean
glade_command_target_undo (GladeCommand *cmd)
{
  GladeCommandPrivate *priv = glade_command_get_instance_private (cmd);
  GladeCommandTarget *me = (GladeCommandTarget *) cmd;

  glade_project_set_target_version (priv->project,
                                    me->catalog,
                                    me->old_major,
                                    me->old_minor);

  return TRUE;
}

static void
glade_command_target_finalize (GObject *obj)
{
  GladeCommandTarget *me = (GladeCommandTarget *) obj;

  g_free (me->catalog);

  glade_command_finalize (obj);
}

static gboolean
glade_command_target_unifies (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
  GladeCommandTarget *me;

  /* Do we unify with self ? */
  if (!other_cmd)
    {
      if (GLADE_IS_COMMAND_TARGET (this_cmd))
        {
          me = (GladeCommandTarget *) this_cmd;

          return (me->old_major == me->new_major &&
                  me->old_minor == me->new_minor);
        }
      return FALSE;
    }

  if (GLADE_IS_COMMAND_TARGET (this_cmd) &&
      GLADE_IS_COMMAND_TARGET (other_cmd))
    {
      GladeCommandTarget *other;

      me = (GladeCommandTarget *) this_cmd;
      other = (GladeCommandTarget *) other_cmd;

      return g_strcmp0 (me->catalog, other->catalog) == 0;
    }

  return FALSE;
}

static void
glade_command_target_collapse (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
  GladeCommandPrivate *this_priv = glade_command_get_instance_private (this_cmd);
  GladeCommandTarget *this;
  GladeCommandTarget *other;

  g_return_if_fail (GLADE_IS_COMMAND_TARGET (this_cmd) &&
                    GLADE_IS_COMMAND_TARGET (other_cmd));

  this = GLADE_COMMAND_TARGET (this_cmd);
  other = GLADE_COMMAND_TARGET (other_cmd);

  this->new_major = other->new_major;
  this->new_minor = other->new_minor;

  g_free (this_priv->description);
  this_priv->description =
    g_strdup_printf (_("Setting target version of '%s' to %d.%d"), 
                     this->catalog, this->new_major, this->new_minor);

}

/**
 * glade_command_set_project_target:
 * @project: A #GladeProject
 * @catalog: The name of the catalog to set the project's target for
 * @major: The new major version of @catalog to target
 * @minor: The new minor version of @catalog to target
 *
 * Sets the target of @catalog to @major.@minor in @project.
 */
void
glade_command_set_project_target  (GladeProject *project,
                                   const gchar  *catalog,
                                   gint          major,
                                   gint          minor)
{
  GladeCommandTarget *me;
  GladeCommand *cmd;
  GladeCommandPrivate *priv;
  gint old_major = 0;
  gint old_minor = 0;

  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (catalog && catalog[0]);
  g_return_if_fail (major >= 0);
  g_return_if_fail (minor >= 0);

  /* load up the command */
  me = g_object_new (GLADE_TYPE_COMMAND_TARGET, NULL);
  me->catalog = g_strdup (catalog);
  
  cmd = GLADE_COMMAND (me);
  priv = glade_command_get_instance_private (cmd);
  priv->project = project;

  glade_project_get_target_version (project, me->catalog, &old_major, &old_minor);

  me->new_major = major;
  me->new_minor = minor;
  me->old_major = old_major;
  me->old_minor = old_minor;

  priv->description =
    g_strdup_printf (_("Setting target version of '%s' to %d.%d"), 
                     me->catalog, me->new_major, me->new_minor);

  glade_command_check_group (cmd);

  /* execute the command and push it on the stack if successful 
   * this sets the actual policy
   */
  if (glade_command_target_execute (cmd))
    glade_project_push_undo (priv->project, cmd);
  else
    g_object_unref (G_OBJECT (me));
}

/******************************************************************************
 * 
 * This command sets can set different properties of a GladeProject
 * 
 *****************************************************************************/
typedef gchar *(*DescriptionNewFunc) (GladeCommand *);

struct _GladeCommandProperty
{
  GladeCommand parent;

  const gchar *property_id;            /* Intern string */
  DescriptionNewFunc description_new;  /* Used to update command description */
  GValue old_value;
  GValue new_value;
};

#define GLADE_TYPE_COMMAND_PROPERTY glade_command_property_get_type ()
GLADE_MAKE_COMMAND (GladeCommandProperty, glade_command_property, COMMAND_PROPERTY);

/* Return true if a == b, this could be exported in glade_utils */
static gboolean
glade_command_property_compare (GValue *a, GValue *b)
{
  if (G_VALUE_TYPE (a) != G_VALUE_TYPE (b))
    {
      g_warning ("Comparing a %s with a %s type is not supported",
                 G_VALUE_TYPE_NAME (a), G_VALUE_TYPE_NAME (b));
      return FALSE;
    }

  if (G_VALUE_HOLDS_STRING (a))
    return g_strcmp0 (g_value_get_string (a), g_value_get_string (b)) == 0;
  else if (G_VALUE_HOLDS_OBJECT (a))
    return g_value_get_object (a) == g_value_get_object (b);
  else if (G_VALUE_HOLDS_BOOLEAN (a))
    return g_value_get_boolean (a) == g_value_get_boolean (b);
  else if (G_VALUE_HOLDS_CHAR (a))
    return g_value_get_schar (a) == g_value_get_schar (b);
  else if (G_VALUE_HOLDS_DOUBLE (a))
    return g_value_get_double (a) == g_value_get_double (b);
  else if (G_VALUE_HOLDS_ENUM (a))
    return g_value_get_enum (a) == g_value_get_enum (b);
  else if (G_VALUE_HOLDS_FLAGS (a))
    return g_value_get_flags (a) == g_value_get_flags (b);
  else if (G_VALUE_HOLDS_FLOAT (a))
    return g_value_get_float (a) == g_value_get_float (b);
  else if (G_VALUE_HOLDS_GTYPE (a))
    return g_value_get_gtype (a) == g_value_get_gtype (b);
  else if (G_VALUE_HOLDS_INT (a))
    return g_value_get_int (a) == g_value_get_int (b);
  else if (G_VALUE_HOLDS_INT64 (a))
    return g_value_get_int64 (a) == g_value_get_int64 (b);
  else if (G_VALUE_HOLDS_LONG (a))
    return g_value_get_long (a) == g_value_get_long (b);
  else if (G_VALUE_HOLDS_POINTER (a))
    return g_value_get_pointer (a) == g_value_get_pointer (b);
  else if (G_VALUE_HOLDS_UCHAR (a))
    return g_value_get_uchar (a) == g_value_get_uchar (b);
  else if (G_VALUE_HOLDS_UINT (a))
    return g_value_get_uint (a) == g_value_get_uint (b);
  else if (G_VALUE_HOLDS_UINT64 (a))
    return g_value_get_uint64 (a) == g_value_get_uint64 (b);
  else if (G_VALUE_HOLDS_ULONG (a))
    return g_value_get_ulong (a) == g_value_get_ulong (b);

  g_warning ("%s type not supported", G_VALUE_TYPE_NAME (a));
  return FALSE;
}   

static gboolean
glade_command_property_execute (GladeCommand *cmd)
{
  GladeCommandPrivate *priv = glade_command_get_instance_private (cmd);
  GladeCommandProperty *me = (GladeCommandProperty *) cmd;

  g_object_set_property (G_OBJECT (priv->project), me->property_id, &me->new_value);

  return TRUE;
}

static gboolean
glade_command_property_undo (GladeCommand *cmd)
{
  GladeCommandPrivate *priv = glade_command_get_instance_private (cmd);
  GladeCommandProperty *me = (GladeCommandProperty *) cmd;

  g_object_set_property (G_OBJECT (priv->project), me->property_id, &me->old_value);

  return TRUE;
}

static void
glade_command_property_finalize (GObject *obj)
{
  GladeCommandProperty *me = (GladeCommandProperty *) obj;

  /* NOTE: we do not free me->property_id because it is an intern string */
  g_value_unset (&me->new_value);
  g_value_unset (&me->old_value);

  glade_command_finalize (obj);
}

static gboolean
glade_command_property_unifies (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
  /* Do we unify with self ? */
  if (!other_cmd)
    {
      if (GLADE_IS_COMMAND_PROPERTY (this_cmd))
        {
          GladeCommandProperty *me = (GladeCommandProperty *) this_cmd;
          return glade_command_property_compare (&me->new_value, &me->old_value);
        }
      else
        return FALSE;
    }

  if (GLADE_IS_COMMAND_PROPERTY (this_cmd) && GLADE_IS_COMMAND_PROPERTY (other_cmd))
    {
      GladeCommandProperty *this = (GladeCommandProperty *) this_cmd;
      GladeCommandProperty *other = (GladeCommandProperty *) other_cmd;

      /* Intern strings can be compared by comparind the pointers */
      return this->property_id == other->property_id;
    }

  return FALSE;
}

static void
glade_command_property_update_description (GladeCommand *cmd)
{
  GladeCommandPrivate *priv = glade_command_get_instance_private (cmd);
  GladeCommandProperty *me = (GladeCommandProperty *) cmd;

  g_free (priv->description);
  
  if (me->description_new)
    priv->description = me->description_new (cmd);
  else
    priv->description = g_strdup_printf (_("Setting project's %s property"),
                                         me->property_id);
}

static void
glade_command_property_collapse (GladeCommand *this_cmd, GladeCommand *other_cmd)
{
  GladeCommandProperty *this;
  GladeCommandProperty *other;

  g_return_if_fail (GLADE_IS_COMMAND_PROPERTY (this_cmd) &&
                    GLADE_IS_COMMAND_PROPERTY (other_cmd));

  this = GLADE_COMMAND_PROPERTY (this_cmd);
  other = GLADE_COMMAND_PROPERTY (other_cmd);

  g_return_if_fail (this->property_id == other->property_id);

  g_value_copy (&other->new_value, &this->new_value);

  glade_command_property_update_description (this_cmd);
}

/**
 * glade_command_set_project_property:
 * @project: A #GladeProject
 * @description_new: function to create the command description.
 * @property_id: property this command should use
 * @new_value: the value to set @property_id
 *
 * Sets @new_value as the @property_id property for @project.
 */
static void
glade_command_set_project_property (GladeProject       *project,
                                    DescriptionNewFunc  description_new,
                                    const gchar        *property_id,
                                    GValue             *new_value)
{
  GladeCommandProperty *me;
  GladeCommand *cmd;
  GladeCommandPrivate *priv;
  GValue old_value = G_VALUE_INIT;

  g_value_init (&old_value, G_VALUE_TYPE (new_value));
  g_object_get_property (G_OBJECT (project), property_id, &old_value);
  
  if (glade_command_property_compare (&old_value, new_value))
    {
      g_value_unset (&old_value);
      return;
    }

  me = g_object_new (GLADE_TYPE_COMMAND_PROPERTY, NULL);
  cmd = GLADE_COMMAND (me);
  priv = glade_command_get_instance_private (cmd);
  priv->project = project;

  me->description_new = description_new;
  me->property_id = g_intern_static_string (property_id);

  /* move the old value to the comand struct */
  me->old_value = old_value;

  /* set new value */
  g_value_init (&me->new_value, G_VALUE_TYPE (new_value));
  g_value_copy (new_value, &me->new_value);

  glade_command_property_update_description (cmd);
    
  glade_command_check_group (cmd);

  /* execute the command and push it on the stack if successful 
   * this sets the actual policy
   */
  if (glade_command_property_execute (cmd))
    glade_project_push_undo (priv->project, cmd);
  else
    g_object_unref (G_OBJECT (me));
}

/**
 * glade_command_set_project_license:
 * @project: A #GladeProject
 * @license: License of @project
 *
 * Sets the license agreement for @project. It will be saved in the xml as comment.
 */
void
glade_command_set_project_license (GladeProject *project, const gchar *license)
{
  GValue new_value = G_VALUE_INIT;
  
  g_return_if_fail (GLADE_IS_PROJECT (project));
  
  g_value_init (&new_value, G_TYPE_STRING);
  g_value_set_string (&new_value, license);
  
  glade_command_set_project_property (project, NULL, "license", &new_value);

  g_value_unset (&new_value);
}

static gchar *
gcp_resource_path_description_new (GladeCommand *cmd)
{
  GladeCommandProperty *me = (GladeCommandProperty *) cmd;

  return g_strdup_printf (_("Setting resource path to '%s'"),
                          g_value_get_string (&me->new_value));
}

/**
 * glade_command_set_project_resource_path:
 * @project: A #GladeProject
 * @path: path to load resources from. 
 *
 * Sets a resource path @project.
 */
void
glade_command_set_project_resource_path (GladeProject *project, const gchar *path)
{
  GValue new_value = G_VALUE_INIT;
  
  g_return_if_fail (GLADE_IS_PROJECT (project));
  
  g_value_init (&new_value, G_TYPE_STRING);
  g_value_set_string (&new_value, path);
  
  glade_command_set_project_property (project, gcp_resource_path_description_new,
                                      "resource-path", &new_value);
  g_value_unset (&new_value);
}

static gchar *
gcp_domain_description_new (GladeCommand *cmd)
{
  GladeCommandProperty *me = (GladeCommandProperty *) cmd;

  return g_strdup_printf (_("Setting translation domain to '%s'"),
                          g_value_get_string (&me->new_value));
}

/**
 * glade_command_set_project_domain:
 * @project: A #GladeProject
 * @domain: The translation domain for @project
 *
 * Sets @domain as the translation domain for @project.
 */
void
glade_command_set_project_domain  (GladeProject *project,
                                   const gchar  *domain)
{
  GValue new_value = G_VALUE_INIT;
  
  g_return_if_fail (GLADE_IS_PROJECT (project));
  
  g_value_init (&new_value, G_TYPE_STRING);
  g_value_set_string (&new_value, domain);
  
  glade_command_set_project_property (project, gcp_domain_description_new,
                                      "translation-domain", &new_value);
  g_value_unset (&new_value);
}

static gchar *
gcp_template_description_new (GladeCommand *cmd)
{
  GladeCommandProperty *me = (GladeCommandProperty *) cmd;
  GObject *new_template = g_value_get_object (&me->new_value);
  GObject *old_template = g_value_get_object (&me->old_value);

  if (new_template == NULL && old_template != NULL)
    return g_strdup_printf (_("Unsetting widget '%s' as template"),
                            glade_widget_get_name (GLADE_WIDGET (old_template)));
  else if (new_template != NULL)
    return g_strdup_printf (_("Setting widget '%s' as template"),
                            glade_widget_get_name (GLADE_WIDGET (new_template)));
  else
    return g_strdup (_("Unsetting template"));
}

/**
 * glade_command_set_project_template:
 * @project: A #GladeProject
 * @widget: The #GladeWidget to make template
 *
 * Sets @widget to be the template widget in @project.
 */
void
glade_command_set_project_template (GladeProject *project,
                                    GladeWidget  *widget)
{
  GValue new_value = G_VALUE_INIT;
  
  g_return_if_fail (GLADE_IS_PROJECT (project));
  
  g_value_init (&new_value, G_TYPE_OBJECT);
  g_value_set_object (&new_value, widget);
  
  glade_command_set_project_property (project, gcp_template_description_new,
                                      "template", &new_value);
  g_value_unset (&new_value);
}
