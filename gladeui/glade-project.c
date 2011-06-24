/*
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2008 Tristan Van Berkom
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
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#include <config.h>

/**
 * SECTION:glade-project
 * @Short_Description: The Glade document hub and Load/Save interface.
 *
 * This object owns all project objects and is responsable for loading and
 * saving the glade document, you can monitor the project state via this
 * object and its signals.
 */

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

#include "glade-preview.h"

#include "glade-project.h"
#include "glade-command.h"
#include "glade-name-context.h"
#include "glade-object-stub.h"

#define VALID_ITER(project, iter) ((iter)!= NULL && G_IS_OBJECT ((iter)->user_data) && ((GladeProject*)(project))->priv->stamp == (iter)->stamp)

enum
{
  ADD_WIDGET,
  REMOVE_WIDGET,
  WIDGET_NAME_CHANGED,
  SELECTION_CHANGED,
  CLOSE,
  CHANGED,
  PARSE_BEGAN,
  PARSE_FINISHED,
  TARGETS_CHANGED,
  LOAD_PROGRESS,
  WIDGET_VISIBILITY_CHANGED,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_MODIFIED,
  PROP_HAS_SELECTION,
  PROP_PATH,
  PROP_READ_ONLY,
  PROP_ADD_ITEM,
  PROP_POINTER_MODE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

struct _GladeProjectPrivate
{
  gchar *path;                  /* The full canonical path of the glade file for this project */

  gint unsaved_number;          /* A unique number for this project if it is untitled */

  GladeWidgetAdaptor *add_item; /* The next item to add to the project. */

  gint stamp;                   /* A a random int per instance of project used to stamp/check the
                                 * GtkTreeIter->stamps */
  GList *tree;                  /* List of toplevel Objects in this projects */
  GList *objects;               /* List of all objects in this project */

  GList *selection;             /* We need to keep the selection in the project
                                 * because we have multiple projects and when the
                                 * user switchs between them, he will probably
                                 * not want to loose the selection. This is a list
                                 * of #GtkWidget items.
                                 */
  guint selection_changed_id;

  GladeNameContext *widget_names; /* Context for uniqueness of names */


  GList *undo_stack;            /* A stack with the last executed commands */
  GList *prev_redo_item;        /* Points to the item previous to the redo items */

  GList *first_modification;    /* we record the first modification, so that we
                                 * can set "modification" to FALSE when we
                                 * undo this modification
                                 */

  GtkAccelGroup *accel_group;

  gchar *comment;               /* XML comment, Glade will preserve whatever comment was
                                 * in file, so users can delete or change it.
                                 */

  time_t mtime;                 /* last UTC modification time of file, or 0 if it could not be read */

  GHashTable *target_versions_major;    /* target versions by catalog */
  GHashTable *target_versions_minor;    /* target versions by catalog */

  gchar *resource_path;         /* Indicates where to load resources from for this project 
                                 * (full or relative path, null means project directory).
                                 */

  GList *unknown_catalogs; /* List of CatalogInfo catalogs */

  /* Control on the properties dialog to update buttons etc when properties change */
  GtkWidget *prefs_dialog;
  GtkWidget *project_wide_radio;
  GtkWidget *toplevel_contextual_radio;
  GHashTable *target_radios;

  GtkWidget *resource_default_radio;
  GtkWidget *resource_relative_radio;
  GtkWidget *resource_fullpath_radio;
  GtkWidget *relative_path_entry;
  GtkWidget *full_path_button;

  /* Store previews, so we can kill them on close */
  GHashTable *previews;

  /* For the loading progress bars ("load-progress" signal) */
  gint progress_step;
  gint progress_full;

  /* Flags */
  guint load_cancel : 1;
  guint first_modification_is_na : 1;  /* indicates that the first_modification item has been lost */
  guint has_selection : 1;       /* Whether the project has a selection */
  guint readonly : 1;            /* A flag that is set if the project is readonly */
  guint loading : 1;             /* A flags that is set when the project is loading */
  guint modified : 1;            /* A flag that is set when a project has unsaved modifications
                                  * if this flag is not set we don't have to query
                                  * for confirmation after a close or exit is
                                  * requested
                                  */
  guint pointer_mode : 3;        /* The currently effective GladePointerMode */
};

typedef struct 
{
  gchar *catalog;
  gint position;
} CatalogInfo;

GType
glade_pointer_mode_get_type (void)
{
  static GType etype = 0;

  if (etype == 0)
    {
      static const GEnumValue values[] = {
        {GLADE_POINTER_SELECT, "select", "Select-widgets"},
        {GLADE_POINTER_ADD_WIDGET, "add", "Add-widgets"},
        {GLADE_POINTER_DRAG_RESIZE, "drag-resize", "Drag-and-resize-widgets"},
        {GLADE_POINTER_MARGIN_EDIT, "margin-edit", "Edit-widget-margins"},
        {GLADE_POINTER_ALIGN_EDIT, "align-edit", "Edit-widget-alignment"},
        {0, NULL, NULL}
      };
      etype = g_enum_register_static ("GladePointerMode", values);
    }
  return etype;
}

static void glade_project_set_target_version (GladeProject *project,
                                              const gchar *catalog,
                                              guint16 major, guint16 minor);

static void glade_project_target_version_for_adaptor (GladeProject *project,
                                                      GladeWidgetAdaptor *adaptor,
                                                      gint *major,
                                                      gint *minor);

static void glade_project_set_readonly (GladeProject *project,
                                        gboolean readonly);


static gboolean glade_project_verify (GladeProject *project, gboolean saving);
static void     glade_project_verify_properties     (GladeWidget  *widget);
static void     glade_project_verify_project_for_ui (GladeProject *project);

static void glade_project_verify_adaptor (GladeProject *project,
                                          GladeWidgetAdaptor *adaptor,
                                          const gchar *path_name,
                                          GString *string,
                                          gboolean saving,
                                          gboolean forwidget,
                                          GladeSupportMask *mask);

static GtkWidget *glade_project_build_prefs_dialog (GladeProject *project);

static void target_button_clicked (GtkWidget *widget, GladeProject *project);
static void update_prefs_for_resource_path (GladeProject *project);

static void gtk_tree_model_iface_init (GtkTreeModelIface *iface);

static void glade_project_model_get_iter_for_object (GladeProject *project,
                                                     GObject *object,
                                                     GtkTreeIter *iter);

static gint glade_project_count_children (GladeProject *project, 
                                          GladeWidget  *parent);

static guint glade_project_signals[LAST_SIGNAL] = { 0 };

static GladeIDAllocator *unsaved_number_allocator = NULL;

#define GLADE_PROJECT_LARGE_PROJECT 40

G_DEFINE_TYPE_WITH_CODE (GladeProject, glade_project, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
                                                gtk_tree_model_iface_init))


/*******************************************************************
                            GObjectClass
 *******************************************************************/
static GladeIDAllocator *get_unsaved_number_allocator (void)
{
  if (unsaved_number_allocator == NULL)
    unsaved_number_allocator = glade_id_allocator_new ();

  return unsaved_number_allocator;
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
unparent_objects_recurse (GladeWidget *widget)
{
  GladeWidget *child;
  GList *children, *list;

  /* Unparent all children */
  if ((children = glade_widget_get_children (widget)) != NULL)
    {
      for (list = children; list; list = list->next)
        {
          child = glade_widget_get_from_gobject (list->data);

          unparent_objects_recurse (child);

          if (!glade_widget_get_internal (child))
            glade_widget_remove_child (widget, child);
        }
      g_list_free (children);
    }
}

static void
glade_project_dispose (GObject *object)
{
  GladeProject *project = GLADE_PROJECT (object);
  GladeProjectPrivate *priv = project->priv;
  GList *list, *tree;

  /* Emit close signal */
  g_signal_emit (object, glade_project_signals[CLOSE], 0);

  /* Destroy running previews */
  if (priv->previews)
    {
      g_hash_table_destroy (priv->previews);
      priv->previews = NULL;
    }

  if (priv->selection_changed_id > 0)
    priv->selection_changed_id = 
      (g_source_remove (priv->selection_changed_id), 0);

  glade_project_selection_clear (project, TRUE);

  glade_project_list_unref (priv->undo_stack);
  priv->undo_stack = NULL;

  /* Remove objects from the project */
  tree = g_list_copy (priv->tree);
  for (list = tree; list; list = list->next)
    {
      GladeWidget *gwidget = glade_widget_get_from_gobject (list->data);

      unparent_objects_recurse (gwidget);
    }
  g_list_free (tree);

  while (priv->tree)
    glade_project_remove_object (project, priv->tree->data);

  while (priv->objects)
    glade_project_remove_object (project, priv->objects->data);

  g_assert (priv->tree == NULL);
  g_assert (priv->objects == NULL);

  if (priv->unknown_catalogs)
    {
      GList *l;

      for (l = priv->unknown_catalogs; l; l = g_list_next (l))
        {
          CatalogInfo *data = l->data;
          g_free (data->catalog);
          g_free (data);
        }
      
      g_list_free (priv->unknown_catalogs);
      priv->unknown_catalogs = NULL;
    }

  G_OBJECT_CLASS (glade_project_parent_class)->dispose (object);
}

static void
glade_project_finalize (GObject *object)
{
  GladeProject *project = GLADE_PROJECT (object);

  gtk_widget_destroy (project->priv->prefs_dialog);

  g_free (project->priv->path);
  g_free (project->priv->comment);

  if (project->priv->unsaved_number > 0)
    glade_id_allocator_release (get_unsaved_number_allocator (),
                                project->priv->unsaved_number);

  g_hash_table_destroy (project->priv->target_versions_major);
  g_hash_table_destroy (project->priv->target_versions_minor);
  g_hash_table_destroy (project->priv->target_radios);

  glade_name_context_destroy (project->priv->widget_names);

  G_OBJECT_CLASS (glade_project_parent_class)->finalize (object);
}

static void
glade_project_get_property (GObject *object,
                            guint prop_id, GValue *value, GParamSpec *pspec)
{
  GladeProject *project = GLADE_PROJECT (object);

  switch (prop_id)
    {
    case PROP_MODIFIED:
      g_value_set_boolean (value, project->priv->modified);
      break;
    case PROP_HAS_SELECTION:
      g_value_set_boolean (value, project->priv->has_selection);
      break;
    case PROP_PATH:
      g_value_set_string (value, project->priv->path);
      break;
    case PROP_READ_ONLY:
      g_value_set_boolean (value, project->priv->readonly);
      break;
    case PROP_ADD_ITEM:
      g_value_set_object (value, project->priv->add_item);
      break;
    case PROP_POINTER_MODE:
      g_value_set_enum (value, project->priv->pointer_mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * glade_project_set_modified:
 * @project: a #GladeProject
 * @modified: Whether the project should be set as modified or not
 * @modification: The first #GladeCommand which caused the project to have unsaved changes
 *
 * Set's whether a #GladeProject should be flagged as modified or not. This is useful
 * for indicating that a project has unsaved changes. If @modified is #TRUE, then
 * @modification will be recorded as the first change which caused the project to 
 * have unsaved changes. @modified is #FALSE then @modification will be ignored.
 *
 * If @project is already flagged as modified, then calling this method with
 * @modified as #TRUE, will have no effect. Likewise, if @project is unmodified
 * then calling this method with @modified as #FALSE, will have no effect.
 *
 */
static void
glade_project_set_modified (GladeProject *project, gboolean modified)
{
  GladeProjectPrivate *priv;

  g_return_if_fail (GLADE_IS_PROJECT (project));

  priv = project->priv;

  if (priv->modified != modified)
    {
      priv->modified = !priv->modified;

      if (!priv->modified)
        {
          priv->first_modification = project->priv->prev_redo_item;
          priv->first_modification_is_na = FALSE;
        }

      g_object_notify_by_pspec (G_OBJECT (project), properties[PROP_MODIFIED]);
    }
}

/**
 * glade_project_get_modified:
 * @project: a #GladeProject
 *
 * Get's whether the project has been modified since it was last saved.
 *
 * Returns: %TRUE if the project has been modified since it was last saved
 */
gboolean
glade_project_get_modified (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

  return project->priv->modified;
}

void
glade_project_set_pointer_mode (GladeProject *project, GladePointerMode mode)
{
  g_return_if_fail (GLADE_IS_PROJECT (project));

  if (project->priv->pointer_mode != mode)
    {
      project->priv->pointer_mode = mode;

      g_object_notify_by_pspec (G_OBJECT (project), properties[PROP_POINTER_MODE]);
    }
}

GladePointerMode
glade_project_get_pointer_mode (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

  return project->priv->pointer_mode;
}


void
glade_project_set_add_item (GladeProject *project, GladeWidgetAdaptor *adaptor)
{
  GladeProjectPrivate *priv;

  g_return_if_fail (GLADE_IS_PROJECT (project));

  priv = project->priv;

  if (priv->add_item != adaptor)
    {
      priv->add_item = adaptor;

      g_object_notify_by_pspec (G_OBJECT (project), properties[PROP_ADD_ITEM]);
    }
}

GladeWidgetAdaptor *
glade_project_get_add_item (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  return project->priv->add_item;
}

/*******************************************************************
                          GladeProjectClass
 *******************************************************************/
static void
glade_project_walk_back (GladeProject *project)
{
  if (project->priv->prev_redo_item)
    project->priv->prev_redo_item = project->priv->prev_redo_item->prev;
}

static void
glade_project_walk_forward (GladeProject *project)
{
  if (project->priv->prev_redo_item)
    project->priv->prev_redo_item = project->priv->prev_redo_item->next;
  else
    project->priv->prev_redo_item = project->priv->undo_stack;
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
                     glade_project_signals[CHANGED], 0, cmd, FALSE);

      if ((next_cmd = glade_project_next_undo_item (project)) != NULL &&
          (glade_command_group_id (next_cmd) == 0 || 
           glade_command_group_id (next_cmd) != glade_command_group_id (cmd)))
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
                     glade_project_signals[CHANGED], 0, cmd, TRUE);

      if ((next_cmd = glade_project_next_redo_item (project)) != NULL &&
          (glade_command_group_id (next_cmd) == 0 || 
           glade_command_group_id (next_cmd) != glade_command_group_id (cmd)))
        break;
    }
}

static GladeCommand *
glade_project_next_undo_item_impl (GladeProject *project)
{
  GList *l;

  if ((l = project->priv->prev_redo_item) == NULL)
    return NULL;

  return GLADE_COMMAND (l->data);
}

static GladeCommand *
glade_project_next_redo_item_impl (GladeProject *project)
{
  GList *l;

  if ((l = project->priv->prev_redo_item) == NULL)
    return project->priv->undo_stack ?
        GLADE_COMMAND (project->priv->undo_stack->data) : NULL;
  else
    return l->next ? GLADE_COMMAND (l->next->data) : NULL;
}

static GList *
glade_project_free_undo_item (GladeProject *project, GList *item)
{
  g_assert (item->data);

  if (item == project->priv->first_modification)
    project->priv->first_modification_is_na = TRUE;

  g_object_unref (G_OBJECT (item->data));

  return g_list_next (item);
}

static void
glade_project_push_undo_impl (GladeProject *project, GladeCommand *cmd)
{
  GladeProjectPrivate *priv = project->priv;
  GList *tmp_redo_item;

  /* We should now free all the "redo" items */
  tmp_redo_item = g_list_next (priv->prev_redo_item);
  while (tmp_redo_item)
    tmp_redo_item = glade_project_free_undo_item (project, tmp_redo_item);

  if (priv->prev_redo_item)
    {
      g_list_free (g_list_next (priv->prev_redo_item));
      priv->prev_redo_item->next = NULL;
    }
  else
    {
      g_list_free (priv->undo_stack);
      priv->undo_stack = NULL;
    }

  /* Try to unify only if group depth is 0 and the project has not been recently saved */
  if (glade_command_get_group_depth () == 0 &&
      priv->prev_redo_item != NULL &&
      project->priv->prev_redo_item != project->priv->first_modification)
    {
      GladeCommand *cmd1 = priv->prev_redo_item->data;

      if (glade_command_unifies (cmd1, cmd))
        {
          glade_command_collapse (cmd1, cmd);
          g_object_unref (cmd);

          if (glade_command_unifies (cmd1, NULL))
            {
              tmp_redo_item = priv->prev_redo_item;
              glade_project_walk_back (project);
              glade_project_free_undo_item (project, tmp_redo_item);
              priv->undo_stack =
                g_list_delete_link (priv->undo_stack, tmp_redo_item);

              cmd1 = NULL;
            }

          g_signal_emit (G_OBJECT (project),
                         glade_project_signals[CHANGED], 0, cmd1, TRUE);
          return;
        }
    }

  /* and then push the new undo item */
  priv->undo_stack = g_list_append (priv->undo_stack, cmd);

  if (project->priv->prev_redo_item == NULL)
    priv->prev_redo_item = priv->undo_stack;
  else
    priv->prev_redo_item = g_list_next (priv->prev_redo_item);

  g_signal_emit (G_OBJECT (project),
                 glade_project_signals[CHANGED], 0, cmd, TRUE);
}

static void
glade_project_preview_exits (GladePreview *preview, GladeProject *project)
{
  gchar       *pidstr;

  pidstr  = g_strdup_printf ("%d", glade_preview_get_pid (preview));
  preview = g_hash_table_lookup (project->priv->previews, pidstr);

  if (preview)
    g_hash_table_remove (project->priv->previews, pidstr);

  g_free (pidstr);
}

static void
glade_project_destroy_preview (gpointer data)
{
  GladePreview *preview = GLADE_PREVIEW (data);
  GladeWidget  *gwidget;

  gwidget = glade_preview_get_widget (preview);
  g_object_set_data (G_OBJECT (gwidget), "preview", NULL);

  g_signal_handlers_disconnect_by_func (preview,
                                        G_CALLBACK (glade_project_preview_exits),
                                        g_object_get_data (G_OBJECT (preview), "project"));
  g_object_unref (preview);
}

static void
glade_project_changed_impl (GladeProject *project,
                            GladeCommand *command, gboolean forward)
{
  if (!project->priv->loading)
    {
      /* if this command is the first modification to cause the project
       * to have unsaved changes, then we can now flag the project as unmodified
       */
      if (!project->priv->first_modification_is_na &&
          project->priv->prev_redo_item == project->priv->first_modification)
        glade_project_set_modified (project, FALSE);
      else
        glade_project_set_modified (project, TRUE);
    }
}


/*******************************************************************
                          Class Initializers
 *******************************************************************/
static void
glade_project_init (GladeProject *project)
{
  GladeProjectPrivate *priv;
  GList *list;

  project->priv = priv =
      G_TYPE_INSTANCE_GET_PRIVATE ((project), GLADE_TYPE_PROJECT,
                                   GladeProjectPrivate);

  priv->path = NULL;
  priv->readonly = FALSE;
  priv->tree = NULL;
  priv->selection = NULL;
  priv->has_selection = FALSE;
  priv->undo_stack = NULL;
  priv->prev_redo_item = NULL;
  priv->first_modification = NULL;
  priv->first_modification_is_na = FALSE;
  priv->unknown_catalogs = NULL;

  priv->previews = g_hash_table_new_full (g_str_hash, 
                                          g_str_equal,
                                          g_free, 
                                          glade_project_destroy_preview);

  priv->widget_names = glade_name_context_new ();

  priv->accel_group = NULL;

  priv->unsaved_number =
      glade_id_allocator_allocate (get_unsaved_number_allocator ());

  do
    {                           /* Get a random non-zero TreeIter stamper */
      priv->stamp = g_random_int ();
    }
  while (priv->stamp == 0);

  priv->target_versions_major = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       g_free, NULL);
  priv->target_versions_minor = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       g_free, NULL);

  for (list = glade_app_get_catalogs (); list; list = list->next)
    {
      GladeCatalog *catalog = list->data;

      /* Set default target to catalog version */
      glade_project_set_target_version (project,
                                        glade_catalog_get_name (catalog),
                                        glade_catalog_get_major_version
                                        (catalog),
                                        glade_catalog_get_minor_version
                                        (catalog));
    }

  priv->target_radios = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               g_free, NULL);
  priv->prefs_dialog = glade_project_build_prefs_dialog (project);

}

static void
glade_project_class_init (GladeProjectClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = glade_project_get_property;
  object_class->finalize = glade_project_finalize;
  object_class->dispose = glade_project_dispose;

  klass->add_object = NULL;
  klass->remove_object = NULL;
  klass->undo = glade_project_undo_impl;
  klass->redo = glade_project_redo_impl;
  klass->next_undo_item = glade_project_next_undo_item_impl;
  klass->next_redo_item = glade_project_next_redo_item_impl;
  klass->push_undo = glade_project_push_undo_impl;

  klass->widget_name_changed = NULL;
  klass->selection_changed = NULL;
  klass->close = NULL;
  klass->changed = glade_project_changed_impl;

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
                    G_TYPE_NONE, 1, GLADE_TYPE_WIDGET);

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
                    G_TYPE_NONE, 1, GLADE_TYPE_WIDGET);


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
                    G_TYPE_NONE, 1, GLADE_TYPE_WIDGET);


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
                    NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);


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
                    NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

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
                    _glade_marshal_VOID__OBJECT_BOOLEAN,
                    G_TYPE_NONE, 2, GLADE_TYPE_COMMAND, G_TYPE_BOOLEAN);

  /**
   * GladeProject::parse-began:
   * @gladeproject: the #GladeProject which received the signal.
   *
   * Emitted when @gladeproject parsing starts.
   */
  glade_project_signals[PARSE_BEGAN] =
      g_signal_new ("parse-began",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_FIRST,
                    0, NULL, NULL,
                    g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

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
                    NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * GladeProject::targets-changed:
   * @gladeproject: the #GladeProject which received the signal.
   *
   * Emitted when @gladeproject target versions change.
   */
  glade_project_signals[TARGETS_CHANGED] =
      g_signal_new ("targets-changed",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_FIRST,
                    0, NULL, NULL,
                    g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * GladeProject::load-progress:
   * @gladeproject: the #GladeProject which received the signal.
   * @objects_total: the total amount of objects to load
   * @objects_loaded: the current amount of loaded objects
   *
   * Emitted while @project is loading.
   */
  glade_project_signals[LOAD_PROGRESS] =
      g_signal_new ("load-progress",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_FIRST,
                    0, NULL, NULL,
                    _glade_marshal_VOID__INT_INT,
                    G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

  /**
   * GladeProject::widget-visibility-changed:
   * @gladeproject: the #GladeProject which received the signal.
   * @widget: the widget that its visibity changed
   * @visible: the current visiblity of the widget
   *
   * Emitted when the visivility of a widget changed
   */
  glade_project_signals[WIDGET_VISIBILITY_CHANGED] =
      g_signal_new ("widget-visibility-changed",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_FIRST,
                    0, NULL, NULL,
                    _glade_marshal_VOID__OBJECT_BOOLEAN,
                    G_TYPE_NONE, 2, GLADE_TYPE_WIDGET, G_TYPE_BOOLEAN);

  properties[PROP_MODIFIED] =
    g_param_spec_boolean ("modified",
                          "Modified",
                          _("Whether project has been modified since it was last saved"),
                          FALSE,
                          G_PARAM_READABLE);

  properties[PROP_HAS_SELECTION] =
    g_param_spec_boolean ("has-selection",
                          _("Has Selection"),
                          _("Whether project has a selection"),
                          FALSE,
                          G_PARAM_READABLE);

  properties[PROP_PATH] =
    g_param_spec_string ("path",
                         _("Path"),
                         _("The filesystem path of the project"),
                         NULL,
                         G_PARAM_READABLE);

  properties[PROP_READ_ONLY] =
    g_param_spec_boolean ("read-only",
                          _("Read Only"),
                          _("Whether project is read-only"),
                          FALSE,
                          G_PARAM_READABLE);

  properties[PROP_ADD_ITEM] = 
    g_param_spec_object ("add-item",
                         _("Add Item"),
                         _("The current item to add to the project"),
                         GLADE_TYPE_WIDGET_ADAPTOR,
                         G_PARAM_READABLE);

  properties[PROP_POINTER_MODE] =
    g_param_spec_enum ("pointer-mode",
                       _("Pointer Mode"),
                       _("The currently effective GladePointerMode"),
                       GLADE_TYPE_POINTER_MODE,
                       GLADE_POINTER_SELECT,
                       G_PARAM_READABLE);
  
  /* Install all properties */
  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  g_type_class_add_private (klass, sizeof (GladeProjectPrivate));
}

/*******************************************************************
                    Loading project code here
 *******************************************************************/

/**
 * glade_project_new:
 *
 * Creates a new #GladeProject.
 *
 * Returns: a new #GladeProject
 */
GladeProject *
glade_project_new (void)
{
  GladeProject *project = g_object_new (GLADE_TYPE_PROJECT, NULL);
  return project;
}

/* Called when finishing loading a glade file to resolve object type properties
 */
static void
glade_project_fix_object_props (GladeProject *project)
{
  GList *l, *ll, *objects;
  GValue *value;
  GladeWidget *gwidget;
  GladeProperty *property;
  gchar *txt;

  objects = g_list_copy (project->priv->objects);
  for (l = objects; l; l = l->next)
    {
      gwidget = glade_widget_get_from_gobject (l->data);

      for (ll = glade_widget_get_properties (gwidget); ll; ll = ll->next)
        {
          GladePropertyClass *klass;

          property = GLADE_PROPERTY (ll->data);
          klass    = glade_property_get_class (property);

          if (glade_property_class_is_object (klass) &&
              (txt = g_object_get_data (G_OBJECT (property),
                                        "glade-loaded-object")) != NULL)
            {
              /* Parse the object list and set the property to it
               * (this magicly works for both objects & object lists)
               */
              value = glade_property_class_make_gvalue_from_string (klass, txt, project);

              glade_property_set_value (property, value);

              g_value_unset (value);
              g_free (value);

              g_object_set_data (G_OBJECT (property),
                                 "glade-loaded-object", NULL);
            }
        }
    }
  g_list_free (objects);
}

static gchar *
glade_project_read_requires_from_comment (GladeXmlNode *comment,
                                          guint16 *major, guint16 *minor)
{
  gint maj, min;
  gchar *value, buffer[256];
  gchar *required_lib = NULL;

  if (!glade_xml_node_is_comment (comment))
    return NULL;

  value = glade_xml_get_content (comment);

  if (value &&
      !strncmp (" interface-requires", value, strlen (" interface-requires")))
    {
      if (sscanf (value, " interface-requires %s %d.%d", buffer, &maj, &min) == 3)
        {
          if (major)
            *major = maj;
          if (minor)
            *minor = min;
          required_lib = g_strdup (buffer);
        }
    }
  g_free (value);

  return required_lib;
}


static gboolean
glade_project_read_requires (GladeProject *project,
                             GladeXmlNode *root_node,
                             const gchar *path, gboolean *has_gtk_dep)
{

  GString *string = g_string_new (NULL);
  GladeXmlNode *node;
  gchar *required_lib = NULL;
  gboolean loadable = TRUE;
  guint16 major, minor;
  gint position = 0;

  for (node = glade_xml_node_get_children_with_comments (root_node);
       node; node = glade_xml_node_next_with_comments (node))
    {
      /* Skip non "requires" tags */
      if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_REQUIRES) ||
            (required_lib =
             glade_project_read_requires_from_comment (node, &major, &minor))))
        continue;

      if (!required_lib)
        {
          required_lib =
              glade_xml_get_property_string_required (node, GLADE_XML_TAG_LIB,
                                                      NULL);
          glade_xml_get_property_version (node, GLADE_XML_TAG_VERSION,
                                          &major, &minor);
        }

      if (!required_lib)
        continue;

      /* Dont mention gtk+ as a required lib in 
       * the generated glade file
       */
      if (!glade_catalog_is_loaded (required_lib))
        {
          CatalogInfo *data = g_new0(CatalogInfo, 1);
          
          data->catalog = required_lib;
          data->position = position;

          /* Keep a list of unknown catalogs to avoid loosing the requirement */
          project->priv->unknown_catalogs = g_list_append (project->priv->unknown_catalogs,
                                                           data);
          /* Also keep the version */
          glade_project_set_target_version (project, required_lib, major, minor);

          if (!loadable)
            g_string_append (string, ", ");

          g_string_append (string, required_lib);
          loadable = FALSE;
        }
      else
        {
          if (has_gtk_dep && strcmp (required_lib, "gtk+") == 0)
            *has_gtk_dep = TRUE;

          glade_project_set_target_version
              (project, required_lib, major, minor);
          g_free (required_lib);
        }

      position++;
    }

  if (!loadable)
    glade_util_ui_message (glade_app_get_window (),
                           GLADE_UI_ERROR, NULL,
                           _("Failed to load %s.\n"
                             "The following required catalogs are unavailable: %s"),
                           path, string->str);
  g_string_free (string, TRUE);
  return loadable;
}

static gchar *
glade_project_read_resource_path_from_comment (GladeXmlNode *comment)
{
  gchar *value, buffer[FILENAME_MAX], *path = NULL;

  if (!glade_xml_node_is_comment (comment))
    return FALSE;

  value = glade_xml_get_content (comment);
  if (value &&
      !strncmp (" interface-local-resource-path", value,
                strlen (" interface-local-resource-path")))
    {
      if (sscanf (value, " interface-local-resource-path %s", buffer) == 1)
        path = g_strdup (buffer);
    }
  g_free (value);

  return path;
}

static void
update_project_for_resource_path (GladeProject *project)
{
  GladeWidget *widget;
  GladeProperty *property;
  GList *l, *list;

  for (l = project->priv->objects; l; l = l->next)
    {

      widget = glade_widget_get_from_gobject (l->data);

      for (list = glade_widget_get_properties (widget); list; list = list->next)
        {
          GladePropertyClass *klass;
          GParamSpec         *pspec;

          property = list->data;
          klass    = glade_property_get_class (property);
          pspec    = glade_property_class_get_pspec (klass);

          /* XXX We should have a "resource" flag on properties that need
           *   to be loaded from the resource path, but that would require
           * that they can serialize both ways (custom properties are only
                                                * required to generate unique strings for value comparisons).
                                                  */
          if (pspec->value_type == GDK_TYPE_PIXBUF)
            {
              GValue *value;
              gchar  *string;

              string = glade_property_make_string (property);
              value  = glade_property_class_make_gvalue_from_string (klass, string, project);

              glade_property_set_value (property, value);

              g_value_unset (value);
              g_free (value);
              g_free (string);
            }
        }
    }
}


/* This function assumes ownership of 'path'. */
static void
glade_project_set_resource_path (GladeProject *project, gchar *path)
{
  g_free (project->priv->resource_path);
  project->priv->resource_path = path;

  update_project_for_resource_path (project);
  update_prefs_for_resource_path (project);
}

static void
glade_project_read_resource_path (GladeProject *project,
                                  GladeXmlNode *root_node)
{
  GladeXmlNode *node;
  gchar *path = NULL;

  for (node = glade_xml_node_get_children_with_comments (root_node);
       node; node = glade_xml_node_next_with_comments (node))
    {
      /* Skip non "requires" tags */
      if ((path = glade_project_read_resource_path_from_comment (node)) != NULL)
        break;
    }

  glade_project_set_resource_path (project, path);
}


static void
glade_project_read_comment (GladeProject *project, GladeXmlDoc *doc)
{
  /* TODO Write me !! Find out how to extract root level comments 
   * with libxml2 !!! 
   */
}


typedef struct
{
  GladeWidget *widget;
  gint major;
  gint minor;
} VersionData;

static void
glade_project_introspect_signal_versions (GladeSignal *signal,
                                          VersionData *data)
{
  GladeSignalClass   *signal_class;
  GladeWidgetAdaptor *adaptor;
  gchar              *catalog = NULL;
  gboolean            is_gtk_adaptor = FALSE;

  signal_class =
    glade_widget_adaptor_get_signal_class (glade_widget_get_adaptor (data->widget), 
                                           glade_signal_get_name (signal));

  /*  unknown signal... can it happen ? */
  if (!signal_class) 
    return;

  adaptor = glade_signal_class_get_adaptor (signal_class);

  /* Check if the signal comes from a GTK+ widget class */
  g_object_get (adaptor, "catalog", &catalog, NULL);
  if (strcmp (catalog, "gtk+") == 0)
    is_gtk_adaptor = TRUE;
  g_free (catalog);

  if (is_gtk_adaptor && 
      !GSC_VERSION_CHECK (signal_class, data->major, data->minor))
    {
      data->major = glade_signal_class_since_major (signal_class);
      data->minor = glade_signal_class_since_minor (signal_class);
    }
}

static void
glade_project_introspect_gtk_version (GladeProject *project)
{
  GladeWidget *widget;
  GList *list, *l;
  gint target_major = 2, target_minor = 12;

  for (list = project->priv->objects; list; list = list->next)
    {
      gboolean is_gtk_adaptor = FALSE;
      gchar *catalog = NULL;
      VersionData data = { 0, };
      GList *signals;

      widget = glade_widget_get_from_gobject (list->data);

      /* Check if its a GTK+ widget class */
      g_object_get (glade_widget_get_adaptor (widget), "catalog", &catalog, NULL);
      if (strcmp (catalog, "gtk+") == 0)
        is_gtk_adaptor = TRUE;
      g_free (catalog);

      /* Check widget class version */
      if (is_gtk_adaptor &&
          !GWA_VERSION_CHECK (glade_widget_get_adaptor (widget), target_major, target_minor))
        {
          target_major = GWA_VERSION_SINCE_MAJOR (glade_widget_get_adaptor (widget));
          target_minor = GWA_VERSION_SINCE_MINOR (glade_widget_get_adaptor (widget));
        }

      /* Check all properties */
      for (l = glade_widget_get_properties (widget); l; l = l->next)
        {
          GladeProperty *property = l->data;
          GladePropertyClass *pclass = glade_property_get_class (property);
          GladeWidgetAdaptor *prop_adaptor, *adaptor;
          GParamSpec         *pspec;

          /* Unset properties ofcourse dont count... */
          if (glade_property_original_default (property))
            continue;

          /* Check if this property originates from a GTK+ widget class */
          pspec        = glade_property_class_get_pspec (pclass);
          prop_adaptor = glade_property_class_get_adaptor (pclass);
          adaptor      = glade_widget_adaptor_from_pspec (prop_adaptor, pspec);

          catalog = NULL;
          is_gtk_adaptor = FALSE;
          g_object_get (adaptor, "catalog", &catalog, NULL);
          if (strcmp (catalog, "gtk+") == 0)
            is_gtk_adaptor = TRUE;
          g_free (catalog);

          /* Check GTK+ property class versions */
          if (is_gtk_adaptor &&
              !GPC_VERSION_CHECK (pclass, target_major, target_minor))
            {
              target_major = glade_property_class_since_major (pclass);
              target_minor = glade_property_class_since_minor (pclass);
            }
        }

      /* Check all signal versions here */
      data.widget = widget;
      data.major = target_major;
      data.minor = target_minor;

      signals = glade_widget_get_signal_list (widget);
      g_list_foreach (signals, (GFunc)glade_project_introspect_signal_versions, &data);
      g_list_free (signals);

      if (target_major < data.major)
        target_major = data.major;

      if (target_minor < data.minor)
        target_minor = data.minor;
    }

  glade_project_set_target_version (project, "gtk+", target_major,
                                    target_minor);
}


static gint
glade_project_count_xml_objects (GladeProject *project,
                                 GladeXmlNode *root,
                                 gint count)
{
  GladeXmlNode *node;

  for (node = glade_xml_node_get_children (root);
       node; node = glade_xml_node_next (node))
    {
      if (glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET))
        count = glade_project_count_xml_objects (project, node, ++count);
      else if (glade_xml_node_verify_silent (node, GLADE_XML_TAG_CHILD))
        count = glade_project_count_xml_objects (project, node, count);
    }
  return count;
}

void
glade_project_cancel_load (GladeProject *project)
{
  g_return_if_fail (GLADE_IS_PROJECT (project));

  project->priv->load_cancel = TRUE;
}

gboolean
glade_project_load_cancelled (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

  return project->priv->load_cancel;
}

void
glade_project_push_progress (GladeProject *project)
{
  g_return_if_fail (GLADE_IS_PROJECT (project));

  project->priv->progress_step++;

  g_signal_emit (project, glade_project_signals[LOAD_PROGRESS], 0,
                 project->priv->progress_full, project->priv->progress_step);
}


/* translators: refers to project name '%s' that targets gtk version '%d.%d' */
#define PROJECT_TARGET_DIALOG_TITLE_FMT _("%s targets Gtk+ %d.%d")

static void
glade_project_check_target_version (GladeProject *project)
{
  GladeProjectPrivate *priv;
  GHashTable *unknown_classes;
  gint unknown_objects;
  gint major, minor;
  GtkWidget *dialog;
  GString *text;
  GList *l;

  glade_project_get_target_version (project, "gtk+", &major, &minor);
    
  /* Glade >= 3.10 only targets Gtk 3 */
  if (major >= 3) return;

  priv = project->priv;
  unknown_classes = g_hash_table_new (g_str_hash, g_str_equal);
  unknown_objects = 0;

  for (l = priv->objects; l; l = g_list_next (l))
    {
      if (GLADE_IS_OBJECT_STUB (l->data))
        {
          gchar *type;
          g_object_get (l->data, "object-type", &type, NULL);
          g_hash_table_insert (unknown_classes, type, NULL);
          unknown_objects++;
        }
    }

  if (unknown_objects)
    {
      GList *classes = g_hash_table_get_keys (unknown_classes);

      if (unknown_objects == 1)
        {
          text = g_string_new (_("Specially because there is an object that can not be build with type "));
        }
      else
        {
          text = g_string_new ("");
          g_string_printf (text, _("Specially because there are %d objects that can not be build with types "),
                           unknown_objects);
        }

      for (l = classes; l; l = g_list_next (l))
        {
          if (g_list_previous (l))
            g_string_append (text, (g_list_next (l)) ? ", " : _(" and "));
          
          g_string_append (text, l->data);
        }

      g_list_free (classes);
    }
  else 
    text = NULL;

  dialog = gtk_message_dialog_new (GTK_WINDOW (glade_app_get_window ()),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
                                   PROJECT_TARGET_DIALOG_TITLE_FMT,
                                   glade_project_get_name (project),
                                   major, minor);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("But this version of Glade is for GTK+ 3 only.\n"
                                              "Make sure you can run this project with Glade 3.8 with no deprecated widgets first.\n"
                                              "%s"), (text) ? text->str : "");

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  glade_project_set_target_version (project, "gtk+", 3, 0);

  g_hash_table_destroy (unknown_classes);
  if (text) g_string_free (text, TRUE);
}

static gboolean
glade_project_load_internal (GladeProject *project)
{
  GladeProjectPrivate *priv = project->priv;
  GladeXmlContext *context;
  GladeXmlDoc *doc;
  GladeXmlNode *root;
  GladeXmlNode *node;
  GladeWidget *widget;
  gboolean has_gtk_dep = FALSE;
  gint count;

  priv->selection = NULL;
  priv->objects = NULL;
  priv->loading = TRUE;

  /* get the context & root node of the catalog file */
  if (!(context =
        glade_xml_context_new_from_path (priv->path, NULL, NULL)))
    {
      g_warning ("Couldn't open glade file [%s].", priv->path);
      priv->loading = FALSE;
      return FALSE;
    }

  priv->mtime = glade_util_get_file_mtime (priv->path, NULL);

  doc = glade_xml_context_get_doc (context);
  root = glade_xml_doc_get_root (doc);

  if (!glade_xml_node_verify_silent (root, GLADE_XML_TAG_PROJECT))
    {
      g_warning ("Couldnt recognize GtkBuilder xml, skipping %s",
                 priv->path);
      glade_xml_context_free (context);
      priv->loading = FALSE;
      return FALSE;
    }

  /* Emit "parse-began" signal */
  g_signal_emit (project, glade_project_signals[PARSE_BEGAN], 0);

  /* XXX Need to load project->priv->comment ! */
  glade_project_read_comment (project, doc);

  /* Read requieres, and do not abort load if there are missing catalog since
   * GladeObjectStub is created to keep the original xml for unknown object classes
   */
  glade_project_read_requires (project, root, priv->path, &has_gtk_dep);

  glade_project_read_resource_path (project, root);

  /* Launch a dialog if it's going to take enough time to be
   * worth showing at all */
  count = glade_project_count_xml_objects (project, root, 0);
  priv->progress_full = count;
  priv->progress_step = 0;

  for (node = glade_xml_node_get_children (root);
       node; node = glade_xml_node_next (node))
    {
      /* Skip "requires" tags */
      if (!glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET))
        continue;

      if ((widget = glade_widget_read (project, NULL, node, NULL)) != NULL)
        glade_project_add_object (project, glade_widget_get_object (widget));

      if (priv->load_cancel)
        break;
    }

  /* Finished with the xml context */
  glade_xml_context_free (context);

  if (priv->load_cancel)
    {
      priv->loading = FALSE;
      return FALSE;
    }

  if (!has_gtk_dep)
    glade_project_introspect_gtk_version (project);

  if (glade_util_file_is_writeable (priv->path) == FALSE)
    glade_project_set_readonly (project, TRUE);

  /* Now we have to loop over all the object properties
   * and fix'em all ('cause they probably weren't found)
   */
  glade_project_fix_object_props (project);

  /* Emit "parse-finished" signal */
  g_signal_emit (project, glade_project_signals[PARSE_FINISHED], 0);

  /* Reset project status here too so that you get a clean
   * slate after calling glade_project_open().
   */
  priv->modified = FALSE;
  priv->loading = FALSE;

  /* Update ui with versioning info
   */
  glade_project_verify_project_for_ui (project);

  glade_project_check_target_version (project);
  
  return TRUE;

}

gboolean
glade_project_load_from_file (GladeProject *project, const gchar *path)
{
  gboolean retval;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

  project->priv->path = glade_util_canonical_path (path);
  g_object_notify_by_pspec (G_OBJECT (project), properties[PROP_PATH]);

  retval = glade_project_load_internal (project);

  if (retval)
    {
      gchar *name, *title;

      /* Update prefs dialogs here... */
      name = glade_project_get_name (project);
      title = g_strdup_printf (_("%s document properties"), name);
      gtk_window_set_title (GTK_WINDOW (project->priv->prefs_dialog), title);
      g_free (title);
      g_free (name);
    }
  return retval;
}

/**
 * glade_project_load:
 * @path:
 * 
 * Opens a project at the given path.
 *
 * Returns: a new #GladeProject for the opened project on success, %NULL on 
 *          failure
 */
GladeProject *
glade_project_load (const gchar *path)
{
  GladeProject *project;
  gboolean retval;

  g_return_val_if_fail (path != NULL, NULL);

  project = g_object_new (GLADE_TYPE_PROJECT, NULL);

  project->priv->path = glade_util_canonical_path (path);

  retval = glade_project_load_internal (project);

  if (retval)
    {
      gchar *name, *title;

      /* Update prefs dialogs here... */
      name = glade_project_get_name (project);
      title = g_strdup_printf (_("%s document properties"), name);
      gtk_window_set_title (GTK_WINDOW (project->priv->prefs_dialog), title);
      g_free (title);
      g_free (name);

      return project;
    }
  else
    {
      g_object_unref (project);
      return NULL;
    }
}

/*******************************************************************
                    Writing project code here
 *******************************************************************/

#define GLADE_XML_COMMENT "Generated with "PACKAGE_NAME

static gchar *
glade_project_make_comment ()
{
  time_t now = time (NULL);
  gchar *comment;
  comment = g_strdup_printf (GLADE_XML_COMMENT " " PACKAGE_VERSION " on %s",
                             ctime (&now));
  glade_util_replace (comment, '\n', ' ');

  return comment;
}

static void
glade_project_update_comment (GladeProject *project)
{
  gchar **lines, **l, *comment = NULL;

  /* If project has no comment -> add the new one */
  if (project->priv->comment == NULL)
    {
      project->priv->comment = glade_project_make_comment ();
      return;
    }

  for (lines = l = g_strsplit (project->priv->comment, "\n", 0); *l; l++)
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
      g_free (project->priv->comment);
      project->priv->comment = g_strjoinv ("\n", lines);
    }

  g_strfreev (lines);
}

static void
glade_project_write_required_libs (GladeProject *project,
                                   GladeXmlContext *context,
                                   GladeXmlNode *root)
{
  GladeXmlNode *req_node;
  GList *required, *list;
  gint major, minor;
  gchar *version;

  if ((required = glade_project_required_libs (project)) != NULL)
    {
      for (list = required; list; list = list->next)
        {
          glade_project_get_target_version (project, (gchar *) list->data,
                                            &major, &minor);

          version = g_strdup_printf ("%d.%d", major, minor);

          /* Write the standard requires tag */
          if (GLADE_GTKBUILDER_HAS_VERSIONING (major, minor))
            {
              req_node = glade_xml_node_new (context, GLADE_XML_TAG_REQUIRES);
              glade_xml_node_append_child (root, req_node);
              glade_xml_node_set_property_string (req_node,
                                                  GLADE_XML_TAG_LIB,
                                                  (gchar *) list->data);
            }
          else
            {
              gchar *comment = g_strdup_printf (" interface-requires %s %s ",
                                                (gchar *) list->data, version);
              req_node = glade_xml_node_new_comment (context, comment);
              glade_xml_node_append_child (root, req_node);
              g_free (comment);
            }

          glade_xml_node_set_property_string (req_node, GLADE_XML_TAG_VERSION,
                                              version);
          g_free (version);

        }
      g_list_foreach (required, (GFunc) g_free, NULL);
      g_list_free (required);
    }
}

static void
glade_project_write_resource_path (GladeProject *project,
                                   GladeXmlContext *context,
                                   GladeXmlNode *root)
{
  GladeXmlNode *path_node;
  if (project->priv->resource_path)
    {
      gchar *comment = g_strdup_printf (" interface-local-resource-path %s ",
                                        project->priv->resource_path);
      path_node = glade_xml_node_new_comment (context, comment);
      glade_xml_node_append_child (root, path_node);
      g_free (comment);
    }
}

static gint
sort_project_dependancies (GObject *a, GObject *b)
{
  GladeWidget *ga, *gb;

  ga = glade_widget_get_from_gobject (a);
  gb = glade_widget_get_from_gobject (b);

  if (glade_widget_adaptor_depends (glade_widget_get_adaptor (ga), ga, gb))
    return 1;
  else if (glade_widget_adaptor_depends (glade_widget_get_adaptor (gb), gb, ga))
    return -1;
  else
    return strcmp (glade_widget_get_name (ga), glade_widget_get_name (gb));
}

static GladeXmlContext *
glade_project_write (GladeProject *project)
{
  GladeXmlContext *context;
  GladeXmlDoc *doc;
  GladeXmlNode *root;           /* *comment_node; */
  GList *list;

  doc = glade_xml_doc_new ();
  context = glade_xml_context_new (doc, NULL);
  root = glade_xml_node_new (context, GLADE_XML_TAG_PROJECT);
  glade_xml_doc_set_root (doc, root);

  glade_project_update_comment (project);
  /* comment_node = glade_xml_node_new_comment (context, project->priv->comment); */

  /* XXX Need to append this to the doc ! not the ROOT !
     glade_xml_node_append_child (root, comment_node); */

  glade_project_write_required_libs (project, context, root);

  glade_project_write_resource_path (project, context, root);

  /* Sort the whole thing */
  project->priv->objects =
      g_list_sort (project->priv->objects,
                   (GCompareFunc) sort_project_dependancies);

  for (list = project->priv->objects; list; list = list->next)
    {
      GladeWidget *widget;

      widget = glade_widget_get_from_gobject (list->data);

      /* 
       * Append toplevel widgets. Each widget then takes
       * care of appending its children.
       */
      if (!glade_widget_get_parent (widget))
        glade_widget_write (widget, context, root);
    }

  return context;
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
  GladeXmlContext *context;
  GladeXmlDoc *doc;
  gchar *canonical_path;
  gint ret;

  g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

  if (glade_project_is_loading (project))
    return FALSE;

  if (!glade_project_verify (project, TRUE))
    return FALSE;

  context = glade_project_write (project);
  doc = glade_xml_context_get_doc (context);
  ret = glade_xml_doc_save (doc, path);
  glade_xml_context_destroy (context);

  canonical_path = glade_util_canonical_path (path);
  g_assert (canonical_path);

  if (project->priv->path == NULL ||
      strcmp (canonical_path, project->priv->path))
    {
      gchar *name, *title;

      project->priv->path = (g_free (project->priv->path),
                             g_strdup (canonical_path));
      g_object_notify_by_pspec (G_OBJECT (project), properties[PROP_PATH]);

      /* Update prefs dialogs here... */
      name = glade_project_get_name (project);
      title = g_strdup_printf (_("%s document properties"), name);
      gtk_window_set_title (GTK_WINDOW (project->priv->prefs_dialog), title);
      g_free (title);
      g_free (name);
    }

  glade_project_set_readonly (project,
                              !glade_util_file_is_writeable (project->priv->
                                                             path));

  project->priv->mtime = glade_util_get_file_mtime (project->priv->path, NULL);

  glade_project_set_modified (project, FALSE);

  if (project->priv->unsaved_number > 0)
    {
      glade_id_allocator_release (get_unsaved_number_allocator (),
                                  project->priv->unsaved_number);
      project->priv->unsaved_number = 0;
    }

  g_free (canonical_path);

  return ret > 0;
}

/**
 * glade_project_preview:
 * @project: a #GladeProject
 * @gwidget: a #GladeWidget
 * 
 * Creates and displays a preview window holding a snapshot of @gwidget's
 * toplevel window in @project. Note that the preview window is only a snapshot
 * of the current state of the project, there is no limit on how many preview
 * snapshots can be taken.
 */
void
glade_project_preview (GladeProject *project, GladeWidget *gwidget)
{
  GladeXmlContext *context;
  gchar *text, *pidstr;
  GladePreview *preview = NULL;

  g_return_if_fail (GLADE_IS_PROJECT (project));

  context = glade_project_write (project);

  text = glade_xml_dump_from_context (context);

  gwidget = glade_widget_get_toplevel (gwidget);
  if (!GTK_IS_WIDGET (glade_widget_get_object (gwidget)))
    return;

  if ((pidstr = g_object_get_data (G_OBJECT (gwidget), "preview")) != NULL)
    preview = g_hash_table_lookup (project->priv->previews, pidstr);

  if (!preview)
    {
      preview = glade_preview_launch (gwidget, text);

      /* Leave project data on the preview */
      g_object_set_data (G_OBJECT (preview), "project", project);

      /* Leave preview data on the widget */
      g_object_set_data_full (G_OBJECT (gwidget),
                              "preview", 
                              g_strdup_printf ("%d", glade_preview_get_pid (preview)),
                              g_free);

      g_signal_connect (preview, "exits",
                        G_CALLBACK (glade_project_preview_exits),
                        project);

      /* Add preview to list of previews */
      g_hash_table_insert (project->priv->previews,
                           g_strdup_printf("%d", glade_preview_get_pid (preview)),
                           preview);
    }
  else
    {
      glade_preview_update (preview, text);
    }

  g_free (text);
}

/*******************************************************************
     Verify code here (versioning, incompatability checks)
 *******************************************************************/

/* translators: refers to a widget in toolkit version '%s %d.%d' and a project targeting toolkit version '%s %d.%d' */
#define WIDGET_VERSION_CONFLICT_MSGFMT _("This widget was introduced in %s %d.%d " \
                                         "while project targets %s %d.%d")

/* translators: refers to a widget '[%s]' introduced in toolkit version '%s %d.%d' */
#define WIDGET_VERSION_CONFLICT_FMT    _("[%s] Object class '%s' was introduced in %s %d.%d\n")

#define WIDGET_DEPRECATED_MSG          _("This widget is deprecated")

/* translators: refers to a widget '[%s]' loaded from toolkit version '%s %d.%d' */
#define WIDGET_DEPRECATED_FMT          _("[%s] Object class '%s' from %s %d.%d is deprecated\n")


/* Defined here for pretty translator comments (bug in intl tools, for some reason
 * you can only comment about the line directly following, forcing you to write
 * ugly messy code with comments in line breaks inside function calls).
 */

/* translators: refers to a property in toolkit version '%s %d.%d' 
 * and a project targeting toolkit version '%s %d.%d' */
#define PROP_VERSION_CONFLICT_MSGFMT   _("This property was introduced in %s %d.%d " \
                                         "while project targets %s %d.%d")

/* translators: refers to a property '%s' of widget '[%s]' in toolkit version '%s %d.%d' */
#define PROP_VERSION_CONFLICT_FMT      _("[%s] Property '%s' of object class '%s' " \
                                         "was introduced in %s %d.%d\n")

/* translators: refers to a property '%s' of widget '[%s]' in toolkit version '%s %d.%d' */
#define PACK_PROP_VERSION_CONFLICT_FMT _("[%s] Packing property '%s' of object class '%s' " \
                                         "was introduced in %s %d.%d\n")

/* translators: refers to a signal '%s' of widget '[%s]' in toolkit version '%s %d.%d' */
#define SIGNAL_VERSION_CONFLICT_FMT    _("[%s] Signal '%s' of object class '%s' " \
                                         "was introduced in %s %d.%d\n")

/* translators: refers to a signal in toolkit version '%s %d.%d' 
 * and a project targeting toolkit version '%s %d.%d' */
#define SIGNAL_VERSION_CONFLICT_MSGFMT _("This signal was introduced in %s %d.%d " \
                                         "while project targets %s %d.%d")


static void
glade_project_verify_property_internal (GladeProject *project,
                                        GladeProperty *property,
                                        const gchar *path_name,
                                        GString *string,
                                        gboolean forwidget)
{
  GladeWidgetAdaptor *adaptor, *prop_adaptor;
  GladePropertyClass *pclass;
  GParamSpec         *pspec;
  gint target_major, target_minor;
  gchar *catalog, *tooltip;

  if (glade_property_original_default (property) && !forwidget)
    return;

  pclass       = glade_property_get_class (property);
  pspec        = glade_property_class_get_pspec (pclass);
  prop_adaptor = glade_property_class_get_adaptor (pclass);
  adaptor      = glade_widget_adaptor_from_pspec (prop_adaptor, pspec);

  g_object_get (adaptor, "catalog", &catalog, NULL);
  glade_project_target_version_for_adaptor (project, adaptor,
                                            &target_major, &target_minor);

  if (!GPC_VERSION_CHECK (pclass, target_major, target_minor))
    {
      if (forwidget)
        {
          tooltip = g_strdup_printf (PROP_VERSION_CONFLICT_MSGFMT,
                                     catalog,
                                     glade_property_class_since_major (pclass),
                                     glade_property_class_since_minor (pclass),
                                     catalog, target_major, target_minor);

          glade_property_set_support_warning (property, FALSE, tooltip);
          g_free (tooltip);
        }
      else
        g_string_append_printf (string,
                                glade_property_class_get_is_packing (pclass) ?
                                PACK_PROP_VERSION_CONFLICT_FMT :
                                  PROP_VERSION_CONFLICT_FMT,
                                  path_name,
                                  glade_property_class_get_name (pclass),
                                  glade_widget_adaptor_get_title (adaptor),
                                  catalog,
                                  glade_property_class_since_major (pclass),
                                  glade_property_class_since_minor (pclass));
    }
  else if (forwidget)
    glade_property_set_support_warning (property, FALSE, NULL);

  g_free (catalog);
}

static void
glade_project_verify_properties_internal (GladeWidget *widget,
                                          const gchar *path_name,
                                          GString *string,
                                          gboolean forwidget)
{
  GList *list;
  GladeProperty *property;

  for (list = glade_widget_get_properties (widget); list; list = list->next)
    {
      property = list->data;
      glade_project_verify_property_internal (glade_widget_get_project (widget), 
                                              property, path_name,
                                              string, forwidget);
    }

  /* Sometimes widgets on the clipboard have packing props with no parent */
  if (glade_widget_get_parent (widget))
    {
      for (list = glade_widget_get_packing_properties (widget); list; list = list->next)
        {
          property = list->data;
          glade_project_verify_property_internal (glade_widget_get_project (widget), 
                                                  property, path_name, string, forwidget);
        }
    }
}

static void
glade_project_verify_signal_internal (GladeWidget *widget,
                                      GladeSignal *signal,
                                      const gchar *path_name,
                                      GString *string,
                                      gboolean forwidget)
{
  GladeSignalClass   *signal_class;
  GladeWidgetAdaptor *adaptor;
  gint                target_major, target_minor;
  gchar              *catalog;

  signal_class =
      glade_widget_adaptor_get_signal_class (glade_widget_get_adaptor (widget),
                                             glade_signal_get_name (signal));

  if (!signal_class)
    return;

  adaptor = glade_signal_class_get_adaptor (signal_class);

  g_object_get (adaptor, "catalog", &catalog, NULL);
  glade_project_target_version_for_adaptor (glade_widget_get_project (widget),
                                            adaptor,
                                            &target_major, &target_minor);

  if (!GSC_VERSION_CHECK (signal_class, target_major, target_minor))
    {
      if (forwidget)
        {
          gchar *warning;

          warning = g_strdup_printf (SIGNAL_VERSION_CONFLICT_MSGFMT,
                                     catalog,
                                     glade_signal_class_since_major (signal_class),
                                     glade_signal_class_since_minor (signal_class),
                                     catalog, target_major, target_minor);
          glade_signal_set_support_warning (signal, warning);
          g_free (warning);
        }
      else
        g_string_append_printf (string,
                                SIGNAL_VERSION_CONFLICT_FMT,
                                path_name,
                                glade_signal_get_name (signal),
                                glade_widget_adaptor_get_title (adaptor),
                                catalog,
                                glade_signal_class_since_major (signal_class),
                                glade_signal_class_since_minor (signal_class));
    }
  else if (forwidget)
    glade_signal_set_support_warning (signal, NULL);

  g_free (catalog);
}

void
glade_project_verify_property (GladeProperty *property)
{
  GladeWidget  *widget;
  GladeProject *project;

  g_return_if_fail (GLADE_IS_PROPERTY (property));

  widget  = glade_property_get_widget (property);
  project = glade_widget_get_project (widget);

  if (project)
    glade_project_verify_property_internal (project, property, NULL, NULL, TRUE);
}

void
glade_project_verify_signal (GladeWidget *widget, GladeSignal *signal)
{
  glade_project_verify_signal_internal (widget, signal, NULL, NULL, TRUE);
}

static void
glade_project_verify_signals (GladeWidget *widget,
                              const gchar *path_name,
                              GString *string,
                              gboolean forwidget)
{
  GladeSignal *signal;
  GList *signals, *list;

  if ((signals = glade_widget_get_signal_list (widget)) != NULL)
    {
      for (list = signals; list; list = list->next)
        {
          signal = list->data;
          glade_project_verify_signal_internal (widget, signal, path_name,
                                                string, forwidget);
        }
      g_list_free (signals);
    }
}


/**
 * glade_project_verify_properties:
 * @widget: A #GladeWidget
 *
 * Synchonizes @widget with user visible information
 * about version compatability and notifies the UI
 * it should update.
 */
static void
glade_project_verify_properties (GladeWidget *widget)
{
  GladeProject *project;

  g_return_if_fail (GLADE_IS_WIDGET (widget));

  project = glade_widget_get_project (widget);
  if (!project || project->priv->loading)
    return;

  glade_project_verify_properties_internal (widget, NULL, NULL, TRUE);
  glade_project_verify_signals (widget, NULL, NULL, TRUE);

  glade_widget_support_changed (widget);
}

static gboolean
glade_project_verify_dialog (GladeProject *project,
                             GString *string,
                             gboolean saving)
{
  GtkWidget *swindow;
  GtkWidget *textview;
  GtkWidget *expander;
  GtkTextBuffer *buffer;
  gchar *name;
  gboolean ret;

  swindow = gtk_scrolled_window_new (NULL, NULL);
  textview = gtk_text_view_new ();
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
  expander = gtk_expander_new (_("Details"));

  gtk_text_buffer_set_text (buffer, string->str, -1);

  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (swindow),
                                         textview);
  gtk_container_add (GTK_CONTAINER (expander), swindow);
  gtk_widget_show_all (expander);

  gtk_widget_set_size_request (swindow, 800, -1);

  name = glade_project_get_name (project);
  ret = glade_util_ui_message (glade_app_get_window (),
                               saving ? GLADE_UI_YES_OR_NO : GLADE_UI_INFO,
                               expander,
                               saving ?
                               _("Project \"%s\" has errors. Save anyway?") :
                               _("Project \"%s\" has deprecated widgets "
                                 "and/or version mismatches."), name);
  g_free (name);

  return ret;
}


static gboolean
glade_project_verify (GladeProject *project, gboolean saving)
{
  GString *string = g_string_new (NULL);
  GList *list;
  gboolean ret = TRUE;
  
  for (list = project->priv->objects; list; list = list->next)
    {
      GladeWidget *widget = glade_widget_get_from_gobject (list->data);
      
      if (GLADE_IS_OBJECT_STUB (list->data))
        {
          gchar *type;
          g_object_get (list->data, "object-type", &type, NULL);
          
          /* translators: refers to an unknown object named '%s' of type '%s' */
          g_string_append_printf (string, _("Unknown object %s with type %s\n"), 
                                  glade_widget_get_name (widget), type);
          g_free (type);
        }
      else
        {
          gchar *path_name = glade_widget_generate_path_name (widget);

          glade_project_verify_adaptor (project, glade_widget_get_adaptor (widget),
                                        path_name, string, saving, FALSE, NULL);
          glade_project_verify_properties_internal (widget, path_name, string,
                                                    FALSE);
          glade_project_verify_signals (widget, path_name, string, FALSE);

          g_free (path_name);
        }
    }

  if (string->len > 0)
    {
      ret = glade_project_verify_dialog (project, string, saving);

      if (!saving)
        ret = FALSE;
    }

  g_string_free (string, TRUE);

  return ret;
}

static void
glade_project_target_version_for_adaptor (GladeProject *project,
                                          GladeWidgetAdaptor *adaptor,
                                          gint *major,
                                          gint *minor)
{
  gchar *catalog = NULL;

  g_object_get (adaptor, "catalog", &catalog, NULL);
  glade_project_get_target_version (project, catalog, major, minor);
  g_free (catalog);
}

static void
glade_project_verify_adaptor (GladeProject *project,
                              GladeWidgetAdaptor *adaptor,
                              const gchar *path_name,
                              GString *string,
                              gboolean saving,
                              gboolean forwidget,
                              GladeSupportMask *mask)
{
  GladeSupportMask support_mask = GLADE_SUPPORT_OK;
  GladeWidgetAdaptor *adaptor_iter;
  gint target_major, target_minor;
  gchar *catalog = NULL;

  for (adaptor_iter = adaptor; adaptor_iter && support_mask == GLADE_SUPPORT_OK;
       adaptor_iter = glade_widget_adaptor_get_parent_adaptor (adaptor_iter))
    {

      g_object_get (adaptor_iter, "catalog", &catalog, NULL);
      glade_project_target_version_for_adaptor (project, adaptor_iter,
                                                &target_major, &target_minor);

      /* Only one versioning message (builder or otherwise)...
       */
      if (!GWA_VERSION_CHECK (adaptor_iter, target_major, target_minor))
        {
          if (forwidget)
            g_string_append_printf (string,
                                    WIDGET_VERSION_CONFLICT_MSGFMT,
                                    catalog,
                                    GWA_VERSION_SINCE_MAJOR (adaptor_iter),
                                    GWA_VERSION_SINCE_MINOR (adaptor_iter),
                                    catalog, target_major, target_minor);
          else
            g_string_append_printf (string,
                                    WIDGET_VERSION_CONFLICT_FMT,
                                    path_name, 
                                    glade_widget_adaptor_get_title (adaptor_iter),
                                    catalog,
                                    GWA_VERSION_SINCE_MAJOR (adaptor_iter),
                                    GWA_VERSION_SINCE_MINOR (adaptor_iter));

          support_mask |= GLADE_SUPPORT_MISMATCH;
        }

      if (!saving && GWA_DEPRECATED (adaptor_iter))
        {
          if (forwidget)
            {
              if (string->len)
                g_string_append (string, "\n");

              g_string_append_printf (string, WIDGET_DEPRECATED_MSG);
            }
          else
            g_string_append_printf (string, WIDGET_DEPRECATED_FMT,
                                    path_name, 
                                    glade_widget_adaptor_get_title (adaptor_iter), 
                                    catalog, target_major, target_minor);

          support_mask |= GLADE_SUPPORT_DEPRECATED;
        }
      g_free (catalog);
    }
  if (mask)
    *mask = support_mask;

}

/**
 * glade_project_verify_widget_adaptor:
 * @project: A #GladeProject
 * @adaptor: the #GladeWidgetAdaptor to verify
 * @mask: a return location for a #GladeSupportMask
 * 
 * Checks the supported state of this widget adaptor
 * and generates a string to show in the UI describing why.
 *
 * Returns: A newly allocated string 
 */
gchar *
glade_project_verify_widget_adaptor (GladeProject *project,
                                     GladeWidgetAdaptor *adaptor,
                                     GladeSupportMask *mask)
{
  GString *string = g_string_new (NULL);
  gchar *ret = NULL;

  glade_project_verify_adaptor (project, adaptor, NULL,
                                string, FALSE, TRUE, mask);

  /* there was a '\0' byte... */
  if (string->len > 0)
    {
      ret = string->str;
      g_string_free (string, FALSE);
    }
  else
    g_string_free (string, TRUE);


  return ret;
}


/**
 * glade_project_verify_project_for_ui:
 * @project: A #GladeProject
 * 
 * Checks the project and updates warning strings in the UI
 */
static void
glade_project_verify_project_for_ui (GladeProject *project)
{
  GList *list;
  GladeWidget *widget;
  gchar *warning;

  /* Sync displayable info here */
  for (list = project->priv->objects; list; list = list->next)
    {
      widget = glade_widget_get_from_gobject (list->data);

      warning =
          glade_project_verify_widget_adaptor (project, glade_widget_get_adaptor (widget), NULL);
      glade_widget_set_support_warning (widget, warning);

      if (warning)
        g_free (warning);

      glade_project_verify_properties (widget);
    }
}

/**
 * glade_project_get_widget_by_name:
 * @project: a #GladeProject
 * @ancestor: The toplevel project object to search under
 * @name: The user visible name of the widget we are looking for
 * 
 * Searches under @ancestor in @project looking for a #GladeWidget named @name.
 * 
 * Returns: a pointer to the widget, %NULL if the widget does not exist
 */
GladeWidget *
glade_project_get_widget_by_name (GladeProject *project,
                                  const gchar  *name)
{
  GList *list;

  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  for (list = project->priv->objects; list; list = list->next)
    {
      GladeWidget *widget;

      widget = glade_widget_get_from_gobject (list->data);

      if (strcmp (glade_widget_get_name (widget), name) == 0)
        return widget;
    }

  return NULL;
}

static void
glade_project_release_widget_name (GladeProject *project,
                                   GladeWidget *gwidget,
                                   const char *widget_name)
{
  glade_name_context_release_name (project->priv->widget_names, widget_name);
}

/**
 * glade_project_available_widget_name:
 * @project: a #GladeProject
 * @widget: the #GladeWidget intended to recieve a new name
 * @name: base name of the widget to create
 *
 * Checks whether @name is an appropriate name for @widget.
 *
 * Returns: whether the name is available or not.
 */
gboolean
glade_project_available_widget_name (GladeProject *project,
                                     GladeWidget *widget,
                                     const gchar *name)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

  if (!name || !name[0])
    return FALSE;

  return !glade_name_context_has_name (project->priv->widget_names, name);
}

static void
glade_project_reserve_widget_name (GladeProject *project,
                                   GladeWidget *gwidget,
                                   const char *widget_name)
{
  if (!glade_project_available_widget_name (project, gwidget, widget_name))
    {
      g_warning ("BUG: widget '%s' attempting to reserve an unavailable widget name '%s' !",
                 glade_widget_get_name (gwidget), widget_name);
      return;
    }

  /* Add to name context */
  glade_name_context_add_name (project->priv->widget_names, widget_name);
}

/**
 * glade_project_new_widget_name:
 * @project: a #GladeProject
 * @widget: the #GladeWidget intended to recieve a new name
 * @base_name: base name of the widget to create
 *
 * Creates a new name for a widget that doesn't collide with any of the names 
 * already in @project. This name will start with @base_name.
 *
 * Returns: a string containing the new name, %NULL if there is not enough 
 *          memory for this string
 */
gchar *
glade_project_new_widget_name (GladeProject *project,
                               GladeWidget *widget,
                               const gchar *base_name)
{
  gchar *name;

  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (base_name && base_name[0], NULL);

  name = glade_name_context_new_name (project->priv->widget_names, base_name);

  return name;
}

/**
 * glade_project_set_widget_name:
 * @project: a #GladeProject
 * @widget: the #GladeWidget to set a name on
 * @name: the name to set.
 *
 * Sets @name on @widget in @project, if @name is not
 * available then a new name will be used.
 */
void
glade_project_set_widget_name (GladeProject *project,
                               GladeWidget *widget,
                               const gchar *name)
{
  gchar *new_name;
  GtkTreeIter iter;
  GtkTreePath *path;

  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (name && name[0]);

  if (strcmp (name, glade_widget_get_name (widget)) == 0)
    return;

  /* Police the widget name */
  if (!glade_project_available_widget_name (project, widget, name))
    new_name = glade_project_new_widget_name (project, widget, name);
  else
    new_name = g_strdup (name);

  glade_project_reserve_widget_name (project, widget, new_name);

  /* Release old name and set new widget name */
  glade_project_release_widget_name (project, widget, glade_widget_get_name (widget));
  glade_widget_set_name (widget, new_name);

  g_signal_emit (G_OBJECT (project),
                 glade_project_signals[WIDGET_NAME_CHANGED], 0, widget);

  g_free (new_name);

  /* Notify views about the iter change */
  glade_project_model_get_iter_for_object (project, glade_widget_get_object (widget), &iter);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (project), &iter);
  gtk_tree_model_row_changed (GTK_TREE_MODEL (project), path, &iter);
  gtk_tree_path_free (path);
}

static void
glade_project_notify_row_has_child (GladeProject *project, GladeWidget *gwidget)
{
  GladeWidget *parent;
  gint         siblings;

  parent = glade_widget_get_parent (gwidget);

  if (parent)
    {
      siblings = glade_project_count_children (project, parent);

      if (siblings == 1)
        {
          GtkTreePath *path;
          GtkTreeIter  iter;

          glade_project_model_get_iter_for_object (project, glade_widget_get_object (parent), &iter);

          path = gtk_tree_model_get_path (GTK_TREE_MODEL (project), &iter);
          gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (project), path, &iter);
          gtk_tree_path_free (path);
        }
    }
}

static void
glade_project_notify_row_inserted (GladeProject *project, GladeWidget *gwidget)
{
  GtkTreeIter iter;
  GtkTreePath *path;

  /* The state of old iters go invalid and then the new iter is valid
   * until the next change */
  project->priv->stamp++;

  glade_project_model_get_iter_for_object (project, glade_widget_get_object (gwidget), &iter);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (project), &iter);
  gtk_tree_model_row_inserted (GTK_TREE_MODEL (project), path, &iter);
  gtk_tree_path_free (path);

  glade_project_notify_row_has_child (project, gwidget);
}

static void
glade_project_notify_row_deleted (GladeProject *project, GladeWidget *gwidget)
{
  GtkTreeIter iter;
  GtkTreePath *path;

  glade_project_model_get_iter_for_object (project, glade_widget_get_object (gwidget), &iter);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (project), &iter);

  gtk_tree_model_row_deleted (GTK_TREE_MODEL (project), path);
  gtk_tree_path_free (path);

  project->priv->stamp++;
}

void
glade_project_check_reordered (GladeProject *project,
                               GladeWidget  *parent,
                               GList        *old_order)
{
  GList       *new_order, *l, *ll;
  gint        *order, n_children, i;
  GtkTreeIter  iter;
  GtkTreePath *path;

  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (GLADE_IS_WIDGET (parent));
  g_return_if_fail (glade_project_has_object (project,
                                              glade_widget_get_object (parent)));

  new_order = glade_widget_get_children (parent);

  /* Check if the list changed */
  for (l = old_order, ll = new_order; 
       l && ll; 
       l = l->next, ll = ll->next)
    {
      if (l->data != ll->data)
        break;
    }

  if (l || ll)
    {
      n_children = glade_project_count_children (project, parent);
      order = g_new (gint, n_children);

      for (i = 0, l = new_order; l; l = l->next)
        {
          GObject *obj = l->data;

          if (glade_project_has_object (project, obj))
            {
              GList *node = g_list_find (old_order, obj);

              g_assert (node);

              order[i] = g_list_position (old_order, node);

              i++;
            }
        }

      /* Signal that the rows were reordered */
      glade_project_model_get_iter_for_object (project, glade_widget_get_object (parent), &iter);
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (project), &iter);
      gtk_tree_model_rows_reordered (GTK_TREE_MODEL (project), path, &iter, order);
      gtk_tree_path_free (path);

      g_free (order);
    }

  g_list_free (new_order);
}

static void
draw_tip (cairo_t *cr)
{
  cairo_line_to (cr, 2, 8);
  cairo_line_to (cr, 2, 4);
  cairo_line_to (cr, 0, 4);
  cairo_line_to (cr, 0, 3);
  cairo_line_to (cr, 3, 0);
  cairo_line_to (cr, 6, 3);
  cairo_line_to (cr, 6, 4);
  cairo_line_to (cr, 4, 4);

  cairo_translate (cr, 12, 6);
  cairo_rotate (cr, G_PI_2);
}

static void
draw_tips (cairo_t *cr)
{
  cairo_move_to (cr, 2, 8);
  draw_tip (cr); draw_tip (cr); draw_tip (cr); draw_tip (cr);
  cairo_close_path (cr);
}

static void
draw_pointer (cairo_t *cr)
{
  cairo_line_to (cr, 8, 3);
  cairo_line_to (cr, 19, 14);
  cairo_line_to (cr, 13.75, 14);
  cairo_line_to (cr, 16.5, 19);
  cairo_line_to (cr, 14, 21);
  cairo_line_to (cr, 11, 16);
  cairo_line_to (cr, 7, 19);
  cairo_line_to (cr, 7, 3);
  cairo_line_to (cr, 8, 3);
}

/* Needed for private draw functions! */
#include "glade-design-private.h"

/**
 * glade_project_pointer_mode_render_icon:
 * @mode: the #GladePointerMode to render as icon
 * @size: icon size
 *
 * Render an icon representing the pointer mode.
 * Best view with sizes bigger than GTK_ICON_SIZE_LARGE_TOOLBAR.
 */ 
GdkPixbuf *
glade_project_pointer_mode_render_icon (GladePointerMode mode, GtkIconSize size)
{
  GtkStyleContext *ctx = gtk_style_context_new ();
  GdkRGBA c1, c2, c3, fg, bg;
  cairo_surface_t *surface;
  GtkWidgetPath *path;
  gint width, height;
  GdkPixbuf *pix;
  cairo_t *cr;

  if (gtk_icon_size_lookup (size, &width, &height) == FALSE) return NULL;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create (surface);
  cairo_scale (cr, width/24.0, height/24.0);

  /* Get Style context */
  path = gtk_widget_path_new ();
  gtk_widget_path_append_type (path, GTK_TYPE_WIDGET);
  gtk_style_context_set_path (ctx, path);
  gtk_widget_path_free (path);

  /* Now get colors */
  gtk_style_context_get_color (ctx, GTK_STATE_FLAG_NORMAL, &fg);
  gtk_style_context_get_background_color (ctx, GTK_STATE_FLAG_NORMAL, &bg);
  
  gtk_style_context_add_class (ctx, GTK_STYLE_CLASS_VIEW);
  gtk_style_context_get_background_color (ctx, GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_FOCUSED, &c1);
  gtk_style_context_get_color (ctx, GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_FOCUSED, &c2);
  gtk_style_context_get_background_color (ctx, GTK_STATE_FLAG_SELECTED, &c3);

  g_object_unref (ctx);

  /* Clear surface */
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_fill(cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  switch (mode)
    {
      case GLADE_POINTER_SELECT:
      case GLADE_POINTER_ADD_WIDGET:
        cairo_set_line_width (cr, 1);
        cairo_translate (cr, 1.5, 1.5);
        draw_pointer (cr);
        fg.alpha = .16;
        gdk_cairo_set_source_rgba (cr, &fg);
        cairo_stroke (cr);

        cairo_translate (cr, -1, -1);
        draw_pointer (cr);
        gdk_cairo_set_source_rgba (cr, &c2);
        cairo_fill_preserve (cr);
      
        fg.alpha = .64;
        gdk_cairo_set_source_rgba (cr, &fg);
        cairo_stroke (cr);
      break;
      case GLADE_POINTER_DRAG_RESIZE:
        cairo_set_line_width (cr, 1);
        cairo_translate (cr, 10.5, 3.5);
        
        draw_tips (cr);

        fg.alpha = .16;
        gdk_cairo_set_source_rgba (cr, &fg);
        cairo_stroke (cr);

        cairo_translate (cr, -1, -1);
        draw_tips (cr);
        
        gdk_cairo_set_source_rgba (cr, &c2);
        cairo_fill_preserve (cr);
        
        c1.red = MAX (0, c1.red - .1);
        c1.green = MAX (0, c1.green - .1);
        c1.blue = MAX (0, c1.blue - .1);
        gdk_cairo_set_source_rgba (cr, &c1);
        cairo_stroke (cr);
      break;
      case GLADE_POINTER_MARGIN_EDIT:
        {
          gdk_cairo_set_source_rgba (cr, &bg);
          cairo_rectangle (cr, 4, 4, 18, 18);
          cairo_fill (cr);

          c1.alpha = .1;
          gdk_cairo_set_source_rgba (cr, &c1);
          cairo_rectangle (cr, 6, 6, 16, 16);
          cairo_fill (cr);

          cairo_set_line_width (cr, 1);
          fg.alpha = .32;
          gdk_cairo_set_source_rgba (cr, &fg);
          cairo_move_to (cr, 16.5, 22);
          cairo_line_to (cr, 16.5, 16.5);
          cairo_line_to (cr, 22, 16.5);
          cairo_stroke (cr);

          c1.alpha = .16;
          gdk_cairo_set_source_rgba (cr, &c1);
          cairo_rectangle (cr, 16, 16, 6, 6);
          cairo_fill (cr);

          cairo_set_line_width (cr, 2);
          c1.alpha = .75;
          gdk_cairo_set_source_rgba (cr, &c1);
          cairo_move_to (cr, 6, 22);
          cairo_line_to (cr, 6, 6);
          cairo_line_to (cr, 22, 6);
          cairo_stroke (cr);

          c1.alpha = 1;
          cairo_scale (cr, .75, .75);
          cairo_set_line_width (cr, 4);
          _glade_design_layout_draw_node (cr, 16*1.25, 6*1.25, &c1, &c2);
          _glade_design_layout_draw_node (cr, 6*1.25, 16*1.25, &c1, &c2);
        }
      break;
      case GLADE_POINTER_ALIGN_EDIT:
        cairo_scale (cr, 1.5, 1.5);
        cairo_rotate (cr, 45*(G_PI/180));
        cairo_translate (cr, 11, 2);
        _glade_design_layout_draw_pushpin (cr, 2.5, &c1, &c2, &c2, &fg);
      break;
      default:
      break;
    }

  pix = gdk_pixbuf_get_from_surface (surface, 0, 0,
                                     cairo_image_surface_get_width (surface),
                                     cairo_image_surface_get_height (surface));

  cairo_surface_destroy (surface);
  cairo_destroy (cr);

  return pix;
}

static inline gboolean
glade_project_has_gwidget (GladeProject *project, GladeWidget *gwidget)
{
  return (glade_widget_get_project (gwidget) == project && 
           glade_widget_in_project (gwidget));
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
  GladeProjectPrivate *priv;
  GladeWidget *gwidget;
  GList *list, *children;
  const gchar *name;

  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (G_IS_OBJECT (object));

  /* We don't list placeholders */
  if (GLADE_IS_PLACEHOLDER (object))
    return;

  /* Only widgets accounted for in the catalog or widgets declared
   * in the plugin with glade_widget_new_for_internal_child () are
   * usefull in the project.
   */
  if ((gwidget = glade_widget_get_from_gobject (object)) == NULL)
    return;

  if (glade_project_has_gwidget (project, gwidget))
    {
      /* FIXME: It's possible we need to notify the model iface if this 
       * happens to make sure the hierarchy is the same, I dont know, this 
       * happens when message dialogs with children are rebuilt but the 
       * hierarchy still looks good afterwards. */
      return;
    }

  priv = project->priv;
  
  name = glade_widget_get_name (gwidget);
  /* Make sure we have an exclusive name first... */
  if (!glade_project_available_widget_name (project, gwidget, name))
    {
      gchar *new_name = glade_project_new_widget_name (project, gwidget, name);

      /* XXX Collect these errors and make a report at startup time */
      if (priv->loading)
        g_warning ("Loading object '%s' with name conflict, renaming to '%s'",
                   name, new_name);

      glade_widget_set_name (gwidget, new_name);
      name = glade_widget_get_name (gwidget);
      
      g_free (new_name);
    }

  glade_project_reserve_widget_name (project, gwidget, name);

  glade_widget_set_project (gwidget, (gpointer) project);
  glade_widget_set_in_project (gwidget, TRUE);
  g_object_ref_sink (gwidget);

  /* Be sure to update the lists before emitting signals */
  if (glade_widget_get_parent (gwidget) == NULL)
    priv->tree = g_list_append (priv->tree, object);

  priv->objects = g_list_prepend (priv->objects, object);

  glade_project_notify_row_inserted (project, gwidget);

  /* NOTE: Sensitive ordering here, we need to recurse after updating
   * the tree model listeners (and update those listeners after our
   * internal lists have been resolved), otherwise children are added
   * before the parents (and the views dont like that).
   */
  if ((children = glade_widget_get_children (gwidget)) != NULL)
    {
      for (list = children; list && list->data; list = list->next)
        glade_project_add_object (project, G_OBJECT (list->data));
      g_list_free (children);
    }

  /* Update user visible compatibility info */
  glade_project_verify_properties (gwidget);

  g_signal_emit (G_OBJECT (project),
                 glade_project_signals[ADD_WIDGET], 0, gwidget);
}

/**
 * glade_project_has_object:
 * @project: the #GladeProject the widget is added to
 * @object: the #GObject to search
 *
 * Returns: whether this object is in this project.
 */
gboolean
glade_project_has_object (GladeProject *project, GObject *object)
{
  GladeWidget *gwidget;

  g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);
  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);

  gwidget = glade_widget_get_from_gobject (object);

  g_return_val_if_fail (GLADE_IS_WIDGET (gwidget), FALSE);

  return glade_project_has_gwidget (project, gwidget);
}

void
glade_project_widget_changed (GladeProject *project, GladeWidget *gwidget)
{
  GObject     *object;
  GtkTreeIter  iter;
  GtkTreePath *path;

  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (GLADE_IS_WIDGET (gwidget));

  object = glade_widget_get_object (gwidget);
  g_return_if_fail (glade_project_has_object (project, object));

  glade_project_model_get_iter_for_object (project, object, &iter);
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (project), &iter);
  gtk_tree_model_row_changed (GTK_TREE_MODEL (project), path, &iter);
  gtk_tree_path_free (path);
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
  GladeWidget *gwidget;
  GList *list, *children;
  gchar *preview_pid;

  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (G_IS_OBJECT (object));

  if (!glade_project_has_object (project, object))
    return;

  if (GLADE_IS_PLACEHOLDER (object))
    return;

  if ((gwidget = glade_widget_get_from_gobject (object)) == NULL)
    return;

  /* Recurse and remove deepest children first */
  if ((children = glade_widget_get_children (gwidget)) != NULL)
    {
      for (list = children; list && list->data; list = list->next)
        glade_project_remove_object (project, G_OBJECT (list->data));
      g_list_free (children);
    }

  /* Notify views that the row is being deleted *before* deleting it */
  glade_project_notify_row_deleted (project, gwidget);

  /* Remove selection and release name from the name context */
  glade_project_selection_remove (project, object, TRUE);
  glade_project_release_widget_name (project, gwidget,
                                     glade_widget_get_name (gwidget));

  g_signal_emit (G_OBJECT (project),
                 glade_project_signals[REMOVE_WIDGET], 0, gwidget);

  /* Update internal data structure (remove from lists) */
  project->priv->tree = g_list_remove (project->priv->tree, object);
  project->priv->objects = g_list_remove (project->priv->objects, object);

  if ((preview_pid = g_object_get_data (G_OBJECT (gwidget), "preview")))
    g_hash_table_remove (project->priv->previews, preview_pid);
  
  /* Unset the project pointer on the GladeWidget */
  glade_widget_set_project (gwidget, NULL);
  glade_widget_set_in_project (gwidget, FALSE);

  glade_project_notify_row_has_child (project, gwidget);
  g_object_unref (gwidget);
}

/*******************************************************************
                        Remaining stubs and api
 *******************************************************************/
static void
glade_project_set_target_version (GladeProject *project,
                                  const gchar *catalog,
                                  guint16 major,
                                  guint16 minor)
{
  GladeTargetableVersion *version;
  GSList *radios, *list;
  GtkWidget *radio;

  g_hash_table_insert (project->priv->target_versions_major,
                       g_strdup (catalog), GINT_TO_POINTER ((int) major));
  g_hash_table_insert (project->priv->target_versions_minor,
                       g_strdup (catalog), GINT_TO_POINTER ((int) minor));

  /* Update prefs dialog from here... */
  if (project->priv->target_radios &&
      (radios =
       g_hash_table_lookup (project->priv->target_radios, catalog)) != NULL)
    {
      for (list = radios; list; list = list->next)
        g_signal_handlers_block_by_func (G_OBJECT (list->data),
                                         G_CALLBACK (target_button_clicked),
                                         project);

      for (list = radios; list; list = list->next)
        {
          radio = list->data;

          version = g_object_get_data (G_OBJECT (radio), "version");
          if (version->major == major && version->minor == minor)
            {
              gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
              break;
            }
        }

      for (list = radios; list; list = list->next)
        g_signal_handlers_unblock_by_func (G_OBJECT (list->data),
                                           G_CALLBACK (target_button_clicked),
                                           project);
    }

  glade_project_verify_project_for_ui (project);

  g_signal_emit (project, glade_project_signals[TARGETS_CHANGED], 0);
}

static void
glade_project_set_readonly (GladeProject *project, gboolean readonly)
{
  g_assert (GLADE_IS_PROJECT (project));

  if (project->priv->readonly != readonly)
    {
      project->priv->readonly = readonly;
      g_object_notify_by_pspec (G_OBJECT (project), properties[PROP_READ_ONLY]);
    }
}


/**
 * glade_project_get_target_version:
 * @project: a #GladeProject
 * @catalog: the name of the catalog @project includes
 * @major: the return location for the target major version
 * @minor: the return location for the target minor version
 *
 * Fetches the target version of the @project for @catalog.
 *
 */
void
glade_project_get_target_version (GladeProject *project,
                                  const gchar *catalog,
                                  gint *major,
                                  gint *minor)
{
  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (catalog && catalog[0]);
  g_return_if_fail (major && minor);

  *major = GPOINTER_TO_INT
      (g_hash_table_lookup (project->priv->target_versions_major, catalog));
  *minor = GPOINTER_TO_INT
      (g_hash_table_lookup (project->priv->target_versions_minor, catalog));
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
  g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

  return project->priv->readonly;
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
                 glade_project_signals[SELECTION_CHANGED], 0);

  /* Cancel any idle we have */
  if (project->priv->selection_changed_id > 0)
    project->priv->selection_changed_id = 
      (g_source_remove (project->priv->selection_changed_id), 0);
}

static gboolean
selection_change_idle (GladeProject *project)
{
  project->priv->selection_changed_id = 0;
  glade_project_selection_changed (project);
  return FALSE;
}

void
glade_project_queue_selection_changed (GladeProject *project)
{
  g_return_if_fail (GLADE_IS_PROJECT (project));

  if (project->priv->selection_changed_id == 0)
    project->priv->selection_changed_id = 
      g_idle_add ((GSourceFunc) selection_change_idle, project);
}

static void
glade_project_set_has_selection (GladeProject *project, gboolean has_selection)
{
  g_assert (GLADE_IS_PROJECT (project));

  if (project->priv->has_selection != has_selection)
    {
      project->priv->has_selection = has_selection;
      g_object_notify_by_pspec (G_OBJECT (project), properties[PROP_HAS_SELECTION]);
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

  return project->priv->has_selection;
}

/**
 * glade_project_is_selected:
 * @project: a #GladeProject
 * @object: a #GObject
 *
 * Returns: whether @object is in @project selection
 */
gboolean
glade_project_is_selected (GladeProject *project, GObject *object)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);
  return (g_list_find (project->priv->selection, object)) != NULL;
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
  GList *l;

  g_return_if_fail (GLADE_IS_PROJECT (project));

  if (project->priv->selection == NULL)
    return;

  for (l = project->priv->selection; l; l = l->next)
    {
      if (GTK_IS_WIDGET (l->data))
        gtk_widget_queue_draw (GTK_WIDGET (l->data));
    }

  g_list_free (project->priv->selection);
  project->priv->selection = NULL;
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
                                GObject *object,
                                gboolean emit_signal)
{
  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (G_IS_OBJECT (object));

  if (glade_project_is_selected (project, object))
    {
      project->priv->selection =
          g_list_remove (project->priv->selection, object);
      if (project->priv->selection == NULL)
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
                             GObject *object,
                             gboolean emit_signal)
{
  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (glade_project_has_object (project, object));

  if (glade_project_is_selected (project, object) == FALSE)
    {
      gboolean toggle_has_selection = (project->priv->selection == NULL);

      if (GTK_IS_WIDGET (object))
        gtk_widget_queue_draw (GTK_WIDGET (object));

      project->priv->selection =
        g_list_prepend (project->priv->selection, object);

      if (toggle_has_selection)
        glade_project_set_has_selection (project, TRUE);

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
                             GObject *object,
                             gboolean emit_signal)
{
  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (glade_project_has_object (project, object));

  if (glade_project_is_selected (project, object) == FALSE ||
      g_list_length (project->priv->selection) != 1)
    {
      glade_project_selection_clear (project, FALSE);
      glade_project_selection_add (project, object, emit_signal);
    }
}

/**
 * glade_project_selection_get:
 * @project: a #GladeProject
 *
 * Returns: a #GList containing the #GtkWidget items currently selected in @project
 */
GList *
glade_project_selection_get (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  return project->priv->selection;
}

/**
 * glade_project_required_libs:
 * @project: a #GladeProject
 *
 * Returns: a #GList of allocated strings which are the names
 * of the required catalogs for this project
 */
GList *
glade_project_required_libs (GladeProject *project)
{
  GList *required = NULL, *l, *ll;
  GladeWidget *gwidget;
  gboolean listed;

  for (l = project->priv->objects; l; l = l->next)
    {
      gchar *catalog = NULL;

      gwidget = glade_widget_get_from_gobject (l->data);
      g_assert (gwidget);

      g_object_get (glade_widget_get_adaptor (gwidget), "catalog", &catalog, NULL);

      if (catalog)
        {
          listed = FALSE;
          for (ll = required; ll; ll = ll->next)
            if (!strcmp ((gchar *) ll->data, catalog))
              {
                listed = TRUE;
                break;
              }

          if (!listed)
            required = g_list_prepend (required, catalog);
        }
    }

  /* Assume GTK+ here */
  if (!required)
    required = g_list_prepend (required, g_strdup ("gtk+"));

  required = g_list_reverse (required);
    
  for (l = project->priv->unknown_catalogs; l; l = g_list_next (l))
    {
      CatalogInfo *data = l->data;
      /* Keep position to make sure we do not create a diff when saving */
      required = g_list_insert (required, g_strdup (data->catalog), data->position);
    }
  
  return required;
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

static GList *
walk_command (GList *list, gboolean forward)
{
  GladeCommand *cmd = list->data;
  GladeCommand *next_cmd;

  if (forward)
    list = list->next;
  else
    list = list->prev;

  next_cmd = list ? list->data : NULL;

  while (list && 
   glade_command_group_id (next_cmd) != 0 && 
   glade_command_group_id (next_cmd) == glade_command_group_id (cmd))
    {
      if (forward)
        list = list->next;
      else
        list = list->prev;

      if (list)
        next_cmd = list->data;
    }

  return list;
}

static void
undo_item_activated (GtkMenuItem *item, GladeProject *project)
{
  gint index, next_index;

  GladeCommand *cmd = g_object_get_data (G_OBJECT (item), "command-data");
  GladeCommand *next_cmd;

  index = g_list_index (project->priv->undo_stack, cmd);

  do
    {
      next_cmd = glade_project_next_undo_item (project);
      next_index = g_list_index (project->priv->undo_stack, next_cmd);

      glade_project_undo (project);

    }
  while (next_index > index);
}

static void
redo_item_activated (GtkMenuItem *item, GladeProject *project)
{
  gint index, next_index;

  GladeCommand *cmd = g_object_get_data (G_OBJECT (item), "command-data");
  GladeCommand *next_cmd;

  index = g_list_index (project->priv->undo_stack, cmd);

  do
    {
      next_cmd = glade_project_next_redo_item (project);
      next_index = g_list_index (project->priv->undo_stack, next_cmd);

      glade_project_redo (project);

    }
  while (next_index < index);
}


/**
 * glade_project_undo_items:
 * @project: A #GladeProject
 *
 * Creates a menu of the undo items in the project stack
 *
 * Returns: A newly created menu
 */
GtkWidget *
glade_project_undo_items (GladeProject *project)
{
  GtkWidget *menu = NULL;
  GtkWidget *item;
  GladeCommand *cmd;
  GList *l;

  g_return_val_if_fail (project != NULL, NULL);

  for (l = project->priv->prev_redo_item; l; l = walk_command (l, FALSE))
    {
      cmd = l->data;

      if (!menu)
        menu = gtk_menu_new ();

      item = gtk_menu_item_new_with_label (glade_command_description (cmd));
      gtk_widget_show (item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), GTK_WIDGET (item));
      g_object_set_data (G_OBJECT (item), "command-data", cmd);

      g_signal_connect (G_OBJECT (item), "activate",
                        G_CALLBACK (undo_item_activated), project);

    }

  return menu;
}

/**
 * glade_project_redo_items:
 * @project: A #GladeProject
 *
 * Creates a menu of the undo items in the project stack
 *
 * Returns: A newly created menu
 */
GtkWidget *
glade_project_redo_items (GladeProject *project)
{
  GtkWidget *menu = NULL;
  GtkWidget *item;
  GladeCommand *cmd;
  GList *l;

  g_return_val_if_fail (project != NULL, NULL);

  for (l = project->priv->prev_redo_item ?
       project->priv->prev_redo_item->next :
       project->priv->undo_stack; l; l = walk_command (l, TRUE))
    {
      cmd = l->data;

      if (!menu)
        menu = gtk_menu_new ();

      item = gtk_menu_item_new_with_label (glade_command_description (cmd));
      gtk_widget_show (item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), GTK_WIDGET (item));
      g_object_set_data (G_OBJECT (item), "command-data", cmd);

      g_signal_connect (G_OBJECT (item), "activate",
                        G_CALLBACK (redo_item_activated), project);

    }

  return menu;
}

void
glade_project_reset_path (GladeProject *project)
{
  g_return_if_fail (GLADE_IS_PROJECT (project));
  project->priv->path = (g_free (project->priv->path), NULL);
}

/**
 * glade_project_resource_fullpath:
 * @project: The #GladeProject.
 * @resource: The resource basename
 *
 * Project resource strings may be relative or fullpaths, but glade
 * always expects a copy in the glade file directory, this function
 * is used to make a local path to the file.
 *
 * Returns: A newly allocated string holding the 
 *          local path the the project resource.
 */
gchar *
glade_project_resource_fullpath (GladeProject *project, const gchar *resource)
{
  gchar *fullpath, *project_dir = NULL, *basename;

  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  basename = g_path_get_basename (resource);

  if (project->priv->path == NULL)
    project_dir = g_get_current_dir ();
  else
    project_dir = g_path_get_dirname (project->priv->path);

  if (project->priv->resource_path)
    {
      if (g_path_is_absolute (project->priv->resource_path))
        fullpath =
            g_build_filename (project->priv->resource_path, basename, NULL);
      else
        fullpath =
            g_build_filename (project_dir, project->priv->resource_path,
                              basename, NULL);
    }
  else
    fullpath = g_build_filename (project_dir, basename, NULL);

  g_free (project_dir);
  g_free (basename);

  return fullpath;
}

/**
 * glade_project_widget_visibility_changed:
 * @project: The #GladeProject.
 * @widget: The widget which visibility changed
 * @visible: widget visibility value
 *
 * Emmits  GladeProject::widget-visibility-changed signal
 *
 */
void
glade_project_widget_visibility_changed (GladeProject  *project,
                                         GladeWidget   *widget,
                                         gboolean       visible)
{
  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (project == glade_widget_get_project (widget));

  g_signal_emit (project, glade_project_signals[WIDGET_VISIBILITY_CHANGED], 0,
                 widget, visible);
}

const gchar *
glade_project_get_path (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  return project->priv->path;
}

gchar *
glade_project_get_name (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  if (project->priv->path)
    return g_filename_display_basename (project->priv->path);
  else
    return g_strdup_printf (_("Unsaved %i"), project->priv->unsaved_number);
}

/**
 * glade_project_is_loading:
 * @project: A #GladeProject
 *
 * Returns: Whether the project is being loaded or not
 *       
 */
gboolean
glade_project_is_loading (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

  return project->priv->loading;
}

time_t
glade_project_get_file_mtime (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), 0);

  return project->priv->mtime;
}

/**
 * glade_projects_get_objects:
 * @project: a GladeProject
 *
 * Returns: List of all objects in this project
 */
const GList *
glade_project_get_objects (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  return project->priv->objects;
}

static void
target_button_clicked (GtkWidget *widget, GladeProject *project)
{
  GladeTargetableVersion *version =
      g_object_get_data (G_OBJECT (widget), "version");
  gchar *catalog = g_object_get_data (G_OBJECT (widget), "catalog");

  glade_project_set_target_version (project,
                                    catalog, version->major, version->minor);
}

static void
verify_clicked (GtkWidget *button, GladeProject *project)
{
  if (glade_project_verify (project, FALSE))
    {
      gchar *name = glade_project_get_name (project);
      glade_util_ui_message (glade_app_get_window (),
                             GLADE_UI_INFO, NULL,
                             _("Project %s has no deprecated widgets "
                               "or version mismatches."), name);
      g_free (name);
    }
}

static void
resource_default_toggled (GtkWidget *widget, GladeProject *project)
{
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    return;

  glade_project_set_resource_path (project, NULL);
  gtk_widget_set_sensitive (project->priv->relative_path_entry, FALSE);
  gtk_widget_set_sensitive (project->priv->full_path_button, FALSE);
}


static void
resource_relative_toggled (GtkWidget *widget, GladeProject *project)
{
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    return;

  gtk_widget_set_sensitive (project->priv->relative_path_entry, TRUE);
  gtk_widget_set_sensitive (project->priv->full_path_button, FALSE);
}

static void
resource_fullpath_toggled (GtkWidget *widget, GladeProject *project)
{
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    return;

  gtk_widget_set_sensitive (project->priv->relative_path_entry, FALSE);
  gtk_widget_set_sensitive (project->priv->full_path_button, TRUE);
}

static void
resource_path_activated (GtkEntry *entry, GladeProject *project)
{
  const gchar *text = gtk_entry_get_text (entry);

  glade_project_set_resource_path (project, text ? g_strdup (text) : NULL);
}


static void
resource_full_path_set (GtkFileChooserButton *button, GladeProject *project)
{
  gchar *text = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (button));

  glade_project_set_resource_path (project, text);
}

static void
update_prefs_for_resource_path (GladeProject *project)
{
  gtk_widget_set_sensitive (project->priv->full_path_button, FALSE);
  gtk_widget_set_sensitive (project->priv->relative_path_entry, FALSE);


  g_signal_handlers_block_by_func (project->priv->resource_default_radio,
                                   G_CALLBACK (resource_default_toggled),
                                   project);
  g_signal_handlers_block_by_func (project->priv->resource_relative_radio,
                                   G_CALLBACK (resource_relative_toggled),
                                   project);
  g_signal_handlers_block_by_func (project->priv->resource_fullpath_radio,
                                   G_CALLBACK (resource_fullpath_toggled),
                                   project);
  g_signal_handlers_block_by_func (project->priv->relative_path_entry,
                                   G_CALLBACK (resource_path_activated),
                                   project);

  if (project->priv->resource_path == NULL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                  (project->priv->resource_default_radio),
                                  TRUE);
  else if (g_path_is_absolute (project->priv->resource_path))
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                    (project->priv->resource_fullpath_radio),
                                    TRUE);
      gtk_widget_set_sensitive (project->priv->full_path_button, TRUE);
    }
  else
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                    (project->priv->resource_relative_radio),
                                    TRUE);
      gtk_widget_set_sensitive (project->priv->relative_path_entry, TRUE);
    }

  gtk_entry_set_text (GTK_ENTRY (project->priv->relative_path_entry),
                      project->priv->resource_path ? project->priv->
                      resource_path : "");

  g_signal_handlers_unblock_by_func (project->priv->resource_default_radio,
                                     G_CALLBACK (resource_default_toggled),
                                     project);
  g_signal_handlers_unblock_by_func (project->priv->resource_relative_radio,
                                     G_CALLBACK (resource_relative_toggled),
                                     project);
  g_signal_handlers_unblock_by_func (project->priv->resource_fullpath_radio,
                                     G_CALLBACK (resource_fullpath_toggled),
                                     project);
  g_signal_handlers_unblock_by_func (project->priv->relative_path_entry,
                                     G_CALLBACK (resource_path_activated),
                                     project);
}


static GtkWidget *
glade_project_build_prefs_box (GladeProject *project)
{
  GtkWidget *main_box, *button;
  GtkWidget *vbox, *hbox, *frame;
  GtkWidget *target_radio, *active_radio;
  GtkWidget *label, *alignment;
  GList *list, *targets;
  gchar *string;
  GtkWidget *main_frame, *main_alignment;
  GtkSizeGroup *sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  main_frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (main_frame), GTK_SHADOW_NONE);
  main_alignment = gtk_alignment_new (0.5F, 0.5F, 0.8F, 0.8F);
  main_box = gtk_vbox_new (FALSE, 0);

  gtk_alignment_set_padding (GTK_ALIGNMENT (main_alignment), 0, 0, 4, 0);

  gtk_container_add (GTK_CONTAINER (main_alignment), main_box);
  gtk_container_add (GTK_CONTAINER (main_frame), main_alignment);

  /* Resource path */
  string =
      g_strdup_printf ("<b>%s</b>", _("Image resources are loaded locally:"));
  frame = gtk_frame_new (NULL);
  vbox = gtk_vbox_new (FALSE, 0);
  alignment = gtk_alignment_new (0.5F, 0.5F, 0.8F, 0.8F);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 8, 0, 12, 0);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  label = gtk_label_new (string);
  g_free (string);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

  gtk_box_pack_start (GTK_BOX (main_box), frame, TRUE, TRUE, 2);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_container_add (GTK_CONTAINER (frame), alignment);
  gtk_container_add (GTK_CONTAINER (alignment), vbox);

  /* Project directory... */
  project->priv->resource_default_radio =
      gtk_radio_button_new_with_label (NULL, _("From the project directory"));
  gtk_box_pack_start (GTK_BOX (vbox), project->priv->resource_default_radio,
                      FALSE, FALSE, 0);
  gtk_size_group_add_widget (sizegroup, project->priv->resource_default_radio);

  /* Project relative directory... */
  hbox = gtk_hbox_new (FALSE, 0);
  project->priv->resource_relative_radio =
      gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON
                                                   (project->priv->
                                                    resource_default_radio),
                                                   _("From a project relative directory"));

  gtk_box_pack_start (GTK_BOX (hbox), project->priv->resource_relative_radio,
                      TRUE, TRUE, 0);
  project->priv->relative_path_entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), project->priv->relative_path_entry, FALSE,
                      TRUE, 2);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_size_group_add_widget (sizegroup, hbox);


  /* fullpath directory... */
  hbox = gtk_hbox_new (FALSE, 0);
  project->priv->resource_fullpath_radio =
      gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON
                                                   (project->priv->resource_default_radio),
                                                   _("From this directory"));
  gtk_box_pack_start (GTK_BOX (hbox), project->priv->resource_fullpath_radio,
                      TRUE, TRUE, 0);

  project->priv->full_path_button =
      gtk_file_chooser_button_new (_("Choose a path to load image resources"),
                                   GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
  gtk_box_pack_start (GTK_BOX (hbox), project->priv->full_path_button, FALSE,
                      TRUE, 2);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_size_group_add_widget (sizegroup, hbox);

  /* Pass ownership to the widgets in the group */
  g_object_unref (sizegroup);

  update_prefs_for_resource_path (project);

  g_signal_connect (G_OBJECT (project->priv->resource_default_radio), "toggled",
                    G_CALLBACK (resource_default_toggled), project);
  g_signal_connect (G_OBJECT (project->priv->resource_relative_radio),
                    "toggled", G_CALLBACK (resource_relative_toggled), project);
  g_signal_connect (G_OBJECT (project->priv->resource_fullpath_radio),
                    "toggled", G_CALLBACK (resource_fullpath_toggled), project);

  g_signal_connect (G_OBJECT (project->priv->relative_path_entry), "activate",
                    G_CALLBACK (resource_path_activated), project);
  g_signal_connect (G_OBJECT (project->priv->full_path_button), "file-set",
                    G_CALLBACK (resource_full_path_set), project);

  /* Target versions */
  string = g_strdup_printf ("<b>%s</b>", _("Toolkit versions required:"));
  frame = gtk_frame_new (NULL);
  vbox = gtk_vbox_new (FALSE, 0);
  alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);

  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 8, 0, 12, 0);

  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);

  label = gtk_label_new (string);
  g_free (string);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_container_add (GTK_CONTAINER (alignment), vbox);
  gtk_container_add (GTK_CONTAINER (frame), alignment);

  gtk_box_pack_start (GTK_BOX (main_box), frame, TRUE, TRUE, 6);

  /* Add stuff to vbox */
  for (list = glade_app_get_catalogs (); list; list = list->next)
    {
      GladeCatalog *catalog = list->data;
      gint minor, major;

      /* Skip if theres only one option */
      if (g_list_length (glade_catalog_get_targets (catalog)) <= 1)
        continue;

      glade_project_get_target_version (project,
                                        glade_catalog_get_name (catalog),
                                        &major, &minor);

      /* Special case to mark GTK+ in upper case */
      if (strcmp (glade_catalog_get_name (catalog), "gtk+") == 0)
        label = gtk_label_new ("GTK+");
      else
        label = gtk_label_new (glade_catalog_get_name (catalog));
      gtk_misc_set_alignment (GTK_MISC (label), 0.0F, 0.5F);

      gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 2);
      hbox = gtk_hbox_new (FALSE, 0);

      active_radio = NULL;
      target_radio = NULL;

      for (targets = glade_catalog_get_targets (catalog);
           targets; targets = targets->next)
        {
          GladeTargetableVersion *version = targets->data;
          gchar *name = g_strdup_printf ("%d.%d",
                                         version->major,
                                         version->minor);

          if (!target_radio)
            target_radio = gtk_radio_button_new_with_label (NULL, name);
          else
            target_radio =
                gtk_radio_button_new_with_label_from_widget
                (GTK_RADIO_BUTTON (target_radio), name);
          g_free (name);

          g_signal_connect (G_OBJECT (target_radio), "clicked",
                            G_CALLBACK (target_button_clicked), project);

          g_object_set_data (G_OBJECT (target_radio), "version", version);
          g_object_set_data (G_OBJECT (target_radio), "catalog",
                             (gchar *) glade_catalog_get_name (catalog));

          gtk_box_pack_end (GTK_BOX (hbox), target_radio, TRUE, TRUE, 2);

          if (major == version->major && minor == version->minor)
            active_radio = target_radio;

        }

      if (active_radio)
        {
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (active_radio), TRUE);
          g_hash_table_insert (project->priv->target_radios,
                               g_strdup (glade_catalog_get_name (catalog)),
                               gtk_radio_button_get_group (GTK_RADIO_BUTTON
                                                           (active_radio)));
        }
      else
        g_warning ("Corrupt catalog versions");

      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 2);
    }

  /* Run verify */
  hbox = gtk_hbox_new (FALSE, 2);
  button = gtk_button_new_from_stock (GTK_STOCK_EXECUTE);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (verify_clicked), project);

  label = gtk_label_new (_("Verify versions and deprecations:"));

  gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 4);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 4);

  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 4);

  gtk_widget_show_all (main_frame);

  return main_frame;
}

static GtkWidget *
glade_project_build_prefs_dialog (GladeProject *project)
{
  GtkWidget *widget, *dialog;
  gchar *title, *name;

  name = glade_project_get_name (project);
  title = g_strdup_printf (_("%s document properties"), name);

  dialog = gtk_dialog_new_with_buttons (title,
                                        GTK_WINDOW (glade_app_get_window ()),
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_STOCK_CLOSE,
                                        GTK_RESPONSE_ACCEPT, NULL);
  g_free (title);
  g_free (name);

  widget = glade_project_build_prefs_box (project);
  gtk_box_pack_end (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                    widget, TRUE, TRUE, 2);

  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX
                       (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 2);

  /* HIG spacings */
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 2); /* 2 * 5 + 2 = 12 */
  gtk_container_set_border_width (GTK_CONTAINER
                                  (gtk_dialog_get_action_area
                                   (GTK_DIALOG (dialog))), 5);
  gtk_box_set_spacing (GTK_BOX
                       (gtk_dialog_get_action_area (GTK_DIALOG (dialog))), 6);


  /* Were explicitly destroying it anyway */
  g_signal_connect (G_OBJECT (dialog), "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  /* Only one action, used to "close" the dialog */
  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (gtk_widget_hide), NULL);

  return dialog;
}

/**
 * glade_project_properties:
 * @project: A #GladeProject
 *
 * Runs a document properties dialog for @project.
 */
void
glade_project_properties (GladeProject *project)
{
  g_return_if_fail (GLADE_IS_PROJECT (project));

  gtk_window_present (GTK_WINDOW (project->priv->prefs_dialog));
}


gchar *
glade_project_display_dependencies (GladeProject *project)
{
  GList *catalogs, *l;
  GString *string;

  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  string = g_string_new ("");

  catalogs = glade_project_required_libs (project);
  for (l = catalogs; l; l = l->next)
    {
      gchar *catalog = l->data;
      gint major = 0, minor = 0;

      glade_project_get_target_version (project, catalog, &major, &minor);

      if (l != catalogs)
        g_string_append (string, ", ");

      /* Capitalize GTK+ */
      if (strcmp (catalog, "gtk+") == 0)
        g_string_append_printf (string, "GTK+ >= %d.%d", major, minor);
      else if (major && minor)
        g_string_append_printf (string, "%s >= %d.%d", catalog, major, minor);
      else
        g_string_append_printf (string, "%s", catalog);

      g_free (catalog);
    }
  g_list_free (catalogs);

  return g_string_free (string, FALSE);
}

/**
 * glade_project_toplevels:
 * @project: a #GladeProject
 *
 * Returns: a #GList containing the #GtkWidget toplevel items in @project
 */
GList *
glade_project_toplevels (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  return project->priv->tree;
}

/* GtkTreeModel implementation */

static GObject *
glade_project_nth_child (GladeProject *project,
                         GladeWidget  *parent, 
                         gint          nth)
{
  GList   *children, *list;
  GObject *child = NULL;
  gint     i;

  children = glade_widget_get_children (parent);

  for (list = children, i = 0; list; list = list->next)
    {
      child = list->data;

      if (!glade_project_has_object (project, child))
         continue;      

      if (i == nth)
        break;

      child = NULL;
      i++;
    }

  g_list_free (children);

  return child;
}

static gint
glade_project_child_position (GladeProject *project,
                              GladeWidget  *parent, 
                              GObject      *child)
{
  GList   *children, *list;
  GObject *iter;
  gint     i, position = -1;

  children = glade_widget_get_children (parent);

  for (list = children, i = 0; list; list = list->next)
    {
      iter = list->data;

      if (!glade_project_has_object (project, iter))
        continue;

      if (iter == child)
        {
          position = i;
          break;
        }
      i++;
    }

  g_list_free (children);

  return position;
}

static gint
glade_project_count_children (GladeProject *project, GladeWidget *parent)
{
  GList   *children, *list;
  GObject *iter;
  gint     i;

  children = glade_widget_get_children (parent);

  for (list = children, i = 0; list; list = list->next)
    {
      iter = list->data;

      if (!glade_project_has_object (project, iter))
        continue;
      i++;
    }

  g_list_free (children);

  return i;
}

static void
glade_project_model_get_iter_for_object (GladeProject *project,
                                         GObject *object,
                                         GtkTreeIter *iter)
{
  g_assert (object);

  iter->stamp = project->priv->stamp;
  iter->user_data = object;
}

static GtkTreeModelFlags
glade_project_model_get_flags (GtkTreeModel *model)
{
  return 0;
}

static gint
glade_project_model_get_n_columns (GtkTreeModel *model)
{
  return GLADE_PROJECT_MODEL_N_COLUMNS;
}

static GType
glade_project_model_get_column_type (GtkTreeModel *model, gint column)
{
  switch (column)
    {
      case GLADE_PROJECT_MODEL_COLUMN_ICON_NAME:
        return G_TYPE_STRING;
      case GLADE_PROJECT_MODEL_COLUMN_NAME:
        return G_TYPE_STRING;
      case GLADE_PROJECT_MODEL_COLUMN_TYPE_NAME:
        return G_TYPE_STRING;
      case GLADE_PROJECT_MODEL_COLUMN_OBJECT:
        return G_TYPE_OBJECT;
      case GLADE_PROJECT_MODEL_COLUMN_MISC:
        return G_TYPE_STRING;
      default:
        g_assert_not_reached ();
        return G_TYPE_NONE;
    }
}

GladeWidget *debug_widget;

static gboolean
glade_project_model_get_iter (GtkTreeModel *model,
                              GtkTreeIter *iter,
                              GtkTreePath *path)
{
  GladeProject *project = GLADE_PROJECT (model);
  gint         *indices = gtk_tree_path_get_indices (path);
  gint          depth = gtk_tree_path_get_depth (path);
  GladeWidget  *widget;
  GObject      *object;
  gint          i;
  GList        *parent;

  if ((parent = g_list_nth (project->priv->tree, indices[0])) != NULL)
    {
      object = parent->data;
      widget = glade_widget_get_from_gobject (object);
    }
  else
    {
      iter->stamp = 0;
      iter->user_data = NULL;
      return FALSE;
    }

  debug_widget = widget;
  for (i = 1; i < depth; i++)
    {
      object = glade_project_nth_child (project, widget, indices[i]);

      if (!object)
        {
          iter->stamp = 0;
          iter->user_data = NULL;
          return FALSE;
        }

      widget = glade_widget_get_from_gobject (object);
      debug_widget = widget;
    }

  if (object)
    {
      glade_project_model_get_iter_for_object (project, object, iter);
      return TRUE;
    }
  else
    {
      iter->stamp = 0;
      iter->user_data = NULL;
      return FALSE;
    }
}

static GtkTreePath *
glade_project_model_get_path (GtkTreeModel *model, GtkTreeIter *iter)
{
  GladeProject *project = GLADE_PROJECT (model);
  GtkTreePath  *path;
  GObject      *object;
  GladeWidget  *widget;
  GladeWidget  *toplevel;
  GladeWidget  *parent;
  GList        *top;

  g_return_val_if_fail (VALID_ITER (project, iter), NULL);

  object   = iter->user_data;
  widget   = glade_widget_get_from_gobject (object);
  toplevel = glade_widget_get_toplevel (widget);
  parent   = widget;

  path = gtk_tree_path_new ();

  while ((parent = glade_widget_get_parent (widget)) != NULL)
    {
      gint position;

      if ((position = glade_project_child_position (project, parent, 
                                                    glade_widget_get_object (widget))) < 0)
        gtk_tree_path_prepend_index (path, 0);
      else
        gtk_tree_path_prepend_index (path, position);

      widget = parent;
    }

  /* Get the index for the top-level list */
  top = g_list_find (project->priv->tree, glade_widget_get_object (toplevel));

  /* While the project is disposing widgets are unparented and sometimes no longer in the tree */
  if (top)
    gtk_tree_path_prepend_index (path, g_list_position (project->priv->tree, top));
  else
    gtk_tree_path_prepend_index (path, 0);
  
  return path;
}

static void
glade_project_model_get_value (GtkTreeModel *model,
                               GtkTreeIter *iter,
                               gint column,
                               GValue *value)
{
  GObject *object;
  GladeWidget *widget;
  GladeProperty *ref_prop;
  gchar *str = NULL, *child_type;

  g_return_if_fail (VALID_ITER (model, iter));

  object = iter->user_data;
  widget = glade_widget_get_from_gobject (object);

  value = g_value_init (value,
                        glade_project_model_get_column_type (model, column));

  switch (column)
    {
      case GLADE_PROJECT_MODEL_COLUMN_ICON_NAME:
        g_object_get (glade_widget_get_adaptor (widget), "icon-name", &str, NULL);
        g_value_take_string (value, str);
        break;
      case GLADE_PROJECT_MODEL_COLUMN_NAME:
        g_value_set_string (value, glade_widget_get_name (widget));
        break;
      case GLADE_PROJECT_MODEL_COLUMN_TYPE_NAME:
        g_value_set_static_string (value, G_OBJECT_TYPE_NAME (object));
        break;
      case GLADE_PROJECT_MODEL_COLUMN_OBJECT:
        g_value_set_object (value, object);
        break;
      case GLADE_PROJECT_MODEL_COLUMN_MISC:
        /* special child type / internal child */
        if (glade_widget_get_internal (widget) != NULL)
          str = g_strdup_printf (_("(internal %s)"),
                                 glade_widget_get_internal (widget));
        else if ((child_type =
                  g_object_get_data (glade_widget_get_object (widget),
                                     "special-child-type")) != NULL)
          str = g_strdup_printf (_("(%s child)"), child_type);
        else if ((ref_prop = 
                  glade_widget_get_parentless_widget_ref (widget)) != NULL)
        {
          GladePropertyClass *pclass     = glade_property_get_class (ref_prop);
          GladeWidget        *ref_widget = glade_property_get_widget (ref_prop);

          /* translators: refers to a property named '%s' of widget '%s' */
          str = g_strdup_printf (_("(%s of %s)"), 
                                 glade_property_class_get_name (pclass),
                                 glade_widget_get_name (ref_widget));
        }

        g_value_take_string (value, str);
        break;
      default:
        g_assert_not_reached ();
    }
}

static gboolean
glade_project_model_iter_next (GtkTreeModel *model, GtkTreeIter *iter)
{
  GladeProject *project = GLADE_PROJECT (model);
  GObject *object = iter->user_data;
  GladeWidget *widget;
  GladeWidget *parent;
  GList *children;
  GList *child;
  GList *next;
  gboolean retval = FALSE;

  g_return_val_if_fail (VALID_ITER (project, iter), FALSE);

  widget = glade_widget_get_from_gobject (object);
  parent = glade_widget_get_parent (widget);

  if (parent)
    {
      children = glade_widget_get_children (parent);
    }
  else
    {
      children = project->priv->tree;
    }

  child = g_list_find (children, object);
  if (child)
    {
      /* Get the next child that is actually in the project */
      for (next = child->next; next; next = next->next)
        {
          GObject *object = next->data;

          if (glade_project_has_object (project, object))
            break;
        }

      if (next)
        {
          glade_project_model_get_iter_for_object (project, next->data, iter);
          retval = TRUE;
        }
    }
  if (children != project->priv->tree)
    g_list_free (children);

  return retval;
}

static gboolean
glade_project_model_iter_has_child (GtkTreeModel *model, GtkTreeIter *iter)
{
  GladeProject *project = GLADE_PROJECT (model);
  GladeWidget  *widget;

  g_return_val_if_fail (VALID_ITER (model, iter), FALSE);

  widget = glade_widget_get_from_gobject (iter->user_data);

  if (glade_project_count_children (project, widget) > 0)
    return TRUE;

  return FALSE;
}

static gint
glade_project_model_iter_n_children (GtkTreeModel *model, GtkTreeIter *iter)
{
  GladeProject *project = GLADE_PROJECT (model);

  g_return_val_if_fail (iter == NULL || VALID_ITER (project, iter), 0);

  if (iter)
    {
      GladeWidget *widget = glade_widget_get_from_gobject (iter->user_data);

      return glade_project_count_children (project, widget);
    }

  return g_list_length (project->priv->tree);
}

static gboolean
glade_project_model_iter_nth_child (GtkTreeModel *model,
                                    GtkTreeIter *iter,
                                    GtkTreeIter *parent,
                                    gint nth)
{
  GladeProject *project = GLADE_PROJECT (model);
  GObject      *object = NULL;

  g_return_val_if_fail (parent == NULL || VALID_ITER (project, parent), FALSE);

  if (parent != NULL)
    {
      GObject     *obj    = parent->user_data;
      GladeWidget *widget = glade_widget_get_from_gobject (obj);

      object = glade_project_nth_child (project, widget, nth);
    }
  else if (project->priv->tree)
    {
      GList *child = g_list_nth (project->priv->tree, nth);

      if (child)
        object = child->data;
    }

  if (object)
    {
      glade_project_model_get_iter_for_object (project, object, iter);
      return TRUE;
    }

  iter->stamp     = 0;
  iter->user_data = NULL;

  return FALSE;
}

static gboolean
glade_project_model_iter_children (GtkTreeModel *model,
                                   GtkTreeIter *iter,
                                   GtkTreeIter *parent)
{
  GladeProject *project = GLADE_PROJECT (model);
  GObject      *object  = NULL;

  g_return_val_if_fail (parent == NULL || VALID_ITER (project, parent), FALSE);

  if (parent)
    {
      GladeWidget *widget = glade_widget_get_from_gobject (parent->user_data);

      object = glade_project_nth_child (project, widget, 0);
    }
  else if (project->priv->tree)
    object = project->priv->tree->data;

  if (object)
    {
      glade_project_model_get_iter_for_object (project, object, iter);
      return TRUE;
    }

  iter->stamp = 0;
  iter->user_data = NULL;
  return FALSE;
}

static gboolean
glade_project_model_iter_parent (GtkTreeModel *model,
                                 GtkTreeIter *iter,
                                 GtkTreeIter *child)
{
  GladeProject *project = GLADE_PROJECT (model);
  GladeWidget *widget;
  GladeWidget *parent;

  g_return_val_if_fail (VALID_ITER (project, child), FALSE);

  widget = glade_widget_get_from_gobject (child->user_data);
  parent = glade_widget_get_parent (widget);

  if (parent && 
      glade_project_has_object (project, glade_widget_get_object (parent)))
    {
      glade_project_model_get_iter_for_object (project,
                                               glade_widget_get_object (parent),
                                               iter);
      return TRUE;
    }

  iter->stamp = 0;
  iter->user_data = NULL;

  return FALSE;
}

static void
gtk_tree_model_iface_init (GtkTreeModelIface *iface)
{
  iface->get_flags = glade_project_model_get_flags;
  iface->get_column_type = glade_project_model_get_column_type;
  iface->get_n_columns = glade_project_model_get_n_columns;
  iface->get_iter = glade_project_model_get_iter;
  iface->get_path = glade_project_model_get_path;
  iface->get_value = glade_project_model_get_value;
  iface->iter_next = glade_project_model_iter_next;
  iface->iter_children = glade_project_model_iter_children;
  iface->iter_has_child = glade_project_model_iter_has_child;
  iface->iter_n_children = glade_project_model_iter_n_children;
  iface->iter_nth_child = glade_project_model_iter_nth_child;
  iface->iter_parent = glade_project_model_iter_parent;
}


/*************************************************
 *                Command Central                *
 *************************************************/
void
glade_project_copy_selection (GladeProject *project)
{
  GList *widgets = NULL, *list;
  gboolean has_unknown = FALSE;

  g_return_if_fail (GLADE_IS_PROJECT (project));

  if (glade_project_is_loading (project))
    return;

  if (!project->priv->selection)
    {
      glade_util_ui_message (glade_app_get_window (),
                             GLADE_UI_INFO, NULL, _("No widget selected."));
      return;
    }

  for (list = project->priv->selection; list && list->data; list = list->next)
    {
      if (GLADE_IS_OBJECT_STUB (list->data))
        has_unknown = TRUE;
      else
        {
          GladeWidget *widget = glade_widget_get_from_gobject (list->data);
          widgets = g_list_prepend (widgets, glade_widget_dup (widget, FALSE));
        }
    }

  if (has_unknown)
    glade_util_ui_message (glade_app_get_window (),
                           GLADE_UI_INFO, NULL, _("Unknown widgets ignored."));
  
  glade_clipboard_add (glade_app_get_clipboard (), widgets);
  g_list_free (widgets);
}

void
glade_project_command_cut (GladeProject *project)
{
  GList *widgets = NULL, *list;
  gboolean has_unknown = FALSE;
  gboolean failed = FALSE;

  g_return_if_fail (GLADE_IS_PROJECT (project));

  if (glade_project_is_loading (project))
    return;

  for (list = project->priv->selection; list && list->data; list = list->next)
    {
      if (GLADE_IS_OBJECT_STUB (list->data))
        has_unknown = TRUE;
      else
        {
          GladeWidget *widget = glade_widget_get_from_gobject (list->data);
          widgets = g_list_prepend (widgets, widget);
        }
    }
  
  if (failed == FALSE && widgets != NULL)
    glade_command_cut (widgets);
  else if (has_unknown)
    glade_util_ui_message (glade_app_get_window (),
                           GLADE_UI_INFO, NULL, _("Unknown widgets ignored."));
  else if (widgets == NULL)
    glade_util_ui_message (glade_app_get_window (),
                           GLADE_UI_INFO, NULL, _("No widget selected."));

  if (widgets)
    g_list_free (widgets);
}

void
glade_project_command_paste (GladeProject *project,
                             GladePlaceholder *placeholder)
{
  GladeClipboard *clipboard;
  GList *list;
  GladeWidget *widget = NULL, *parent;
  gint placeholder_relations = 0;

  g_return_if_fail (GLADE_IS_PROJECT (project));

  if (glade_project_is_loading (project))
    return;

  if (placeholder)
    {
      if (glade_placeholder_get_project (placeholder) == NULL ||
          glade_project_is_loading (glade_placeholder_get_project (placeholder)))
        return;
    }

  list      = project->priv->selection;
  clipboard = glade_app_get_clipboard ();

  /* If there is a selection, paste in to the selected widget, otherwise
   * paste into the placeholder's parent, or at the toplevel
   */
  parent = list ? glade_widget_get_from_gobject (list->data) :
      (placeholder) ? glade_placeholder_get_parent (placeholder) : NULL;

  widget = glade_clipboard_widgets (clipboard) ? glade_clipboard_widgets (clipboard)->data : NULL;

  /* Ignore parent argument if we are pasting a toplevel
   */
  if (g_list_length (glade_clipboard_widgets (clipboard)) == 1 &&
      widget && GWA_IS_TOPLEVEL (glade_widget_get_adaptor (widget)))
    parent = NULL;

  /* Check if parent is actually a container of any sort */
  if (parent && !glade_widget_adaptor_is_container (glade_widget_get_adaptor (parent)))
    {
      glade_util_ui_message (glade_app_get_window (),
                             GLADE_UI_INFO, NULL,
                             _("Unable to paste to the selected parent"));
      return;
    }

  /* Check if selection is good */
  if (project->priv->selection)
    {
      if (g_list_length (project->priv->selection) != 1)
        {
          glade_util_ui_message (glade_app_get_window (),
                                 GLADE_UI_INFO, NULL,
                                 _("Unable to paste to multiple widgets"));

          return;
        }
    }

  /* Abort operation when adding a non scrollable widget to any kind of GtkScrolledWindow. */
  if (parent && widget &&
      glade_util_check_and_warn_scrollable (parent, glade_widget_get_adaptor (widget),
                                            glade_app_get_window ()))
    return;

  /* Check if we have anything to paste */
  if (g_list_length (glade_clipboard_widgets (clipboard)) == 0)
    {
      glade_util_ui_message (glade_app_get_window (), GLADE_UI_INFO, NULL,
                             _("No widget on the clipboard"));

      return;
    }

  /* Check that the underlying adaptor allows the paste */
  if (parent)
    {
      for (list = glade_clipboard_widgets (clipboard); list && list->data; list = list->next)
        {
          widget = list->data;

          if (!glade_widget_add_verify (parent, widget, TRUE))
            return;
        }
    }


  /* Check that we have compatible heirarchies */
  for (list = glade_clipboard_widgets (clipboard); list && list->data; list = list->next)
    {
      widget = list->data;

      if (!GWA_IS_TOPLEVEL (glade_widget_get_adaptor (widget)) && parent)
        {
          /* Count placeholder relations
           */
          if (glade_widget_placeholder_relation (parent, widget))
            placeholder_relations++;
        }
    }

  g_assert (widget);

  /* A GladeWidget that doesnt use placeholders can only paste one
   * at a time
   *
   * XXX: Not sure if this has to be true.
   */
  if (GTK_IS_WIDGET (glade_widget_get_object (widget)) &&
      parent && !GWA_USE_PLACEHOLDERS (glade_widget_get_adaptor (parent)) &&
      g_list_length (glade_clipboard_widgets (clipboard)) != 1)
    {
      glade_util_ui_message (glade_app_get_window (),
                             GLADE_UI_INFO, NULL,
                             _("Only one widget can be pasted at a "
                               "time to this container"));
      return;
    }

  /* Check that enough placeholders are available */
  if (parent &&
      GWA_USE_PLACEHOLDERS (glade_widget_get_adaptor (parent)) &&
      glade_util_count_placeholders (parent) < placeholder_relations)
    {
      glade_util_ui_message (glade_app_get_window (),
                             GLADE_UI_INFO, NULL,
                             _("Insufficient amount of placeholders in "
                               "target container"));
      return;
    }

  glade_command_paste (glade_clipboard_widgets (clipboard), parent, placeholder, project);
}

void
glade_project_command_delete (GladeProject *project)
{
  GList *widgets = NULL, *list;
  GladeWidget *widget;
  gboolean failed = FALSE;

  g_return_if_fail (GLADE_IS_PROJECT (project));

  if (glade_project_is_loading (project))
    return;

  for (list = project->priv->selection; list && list->data; list = list->next)
    {
      widget  = glade_widget_get_from_gobject (list->data);
      widgets = g_list_prepend (widgets, widget);
    }

  if (failed == FALSE && widgets != NULL)
    glade_command_delete (widgets);
  else if (widgets == NULL)
    glade_util_ui_message (glade_app_get_window (),
                           GLADE_UI_INFO, NULL, _("No widget selected."));

  if (widgets)
    g_list_free (widgets);
}
