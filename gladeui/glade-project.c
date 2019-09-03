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
#include "gladeui-enum-types.h"
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
#include "glade-project-properties.h"
#include "glade-dnd.h"
#include "glade-private.h"
#include "glade-tsort.h"

static void     glade_project_target_version_for_adaptor
                                                    (GladeProject       *project,
                                                     GladeWidgetAdaptor *adaptor,
                                                     gint               *major,
                                                     gint               *minor);
static void     glade_project_verify_properties     (GladeWidget        *widget);
static void     glade_project_verify_project_for_ui (GladeProject       *project);
static void     glade_project_verify_adaptor        (GladeProject       *project,
                                                     GladeWidgetAdaptor *adaptor,
                                                     const gchar        *path_name,
                                                     GString            *string,
                                                     GladeVerifyFlags    flags,
                                                     gboolean            forwidget,
                                                     GladeSupportMask   *mask);
static void     glade_project_set_readonly          (GladeProject       *project,
                                                     gboolean            readonly);
static void     glade_project_set_modified          (GladeProject       *project,
                                                     gboolean            modified);

static void     glade_project_model_iface_init      (GtkTreeModelIface  *iface);

static void     glade_project_drag_source_init      (GtkTreeDragSourceIface *iface);

struct _GladeProjectPrivate
{
  gchar *path;                  /* The full canonical path of the glade file for this project */

  gchar *translation_domain;    /* The project translation domain */

  gint unsaved_number;          /* A unique number for this project if it is untitled */

  GladeWidgetAdaptor *add_item; /* The next item to add to the project. */

  GList *tree;                  /* List of toplevel Objects in this projects */
  GList *objects;               /* List of all objects in this project */
  GtkTreeModel *model;          /* GtkTreeStore used as proxy model */

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

  GladeWidget *template;        /* The template widget */

  gchar *license;               /* License for this project (will be saved as a comment) */

  GList *comments;              /* XML comments, Glade will preserve whatever comment was
                                 * in file before the root element, so users can delete or change it.
                                 */

  time_t mtime;                 /* last UTC modification time of file, or 0 if it could not be read */

  GHashTable *target_versions_major;    /* target versions by catalog */
  GHashTable *target_versions_minor;    /* target versions by catalog */

  gchar *resource_path;         /* Indicates where to load resources from for this project 
                                 * (full or relative path, null means project directory).
                                 */

  gchar *css_provider_path;     /* The custom css to use for this project */
  GtkCssProvider *css_provider;
  GFileMonitor *css_monitor;

  GList *unknown_catalogs; /* List of CatalogInfo catalogs */

  GtkWidget *prefs_dialog;

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
  guint writing_preview : 1;     /* During serialization, if we are serializing for a preview */
  guint pointer_mode : 3;        /* The currently effective GladePointerMode */
};

typedef struct 
{
  gchar *catalog;
  gint position;
} CatalogInfo;


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
  ADD_SIGNAL_HANDLER,
  REMOVE_SIGNAL_HANDLER,
  CHANGE_SIGNAL_HANDLER,
  ACTIVATE_SIGNAL_HANDLER,
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
  PROP_TRANSLATION_DOMAIN,
  PROP_TEMPLATE,
  PROP_RESOURCE_PATH,
  PROP_LICENSE,
  PROP_CSS_PROVIDER_PATH,
  N_PROPERTIES
};

static GParamSpec       *glade_project_props[N_PROPERTIES];
static guint             glade_project_signals[LAST_SIGNAL] = { 0 };
static GladeIDAllocator *unsaved_number_allocator = NULL;


#define GLADE_XML_COMMENT "Generated with "PACKAGE_NAME
#define GLADE_PROJECT_LARGE_PROJECT 40

#define VALID_ITER(project, iter) \
  ((iter)!= NULL && G_IS_OBJECT ((iter)->user_data) && \
   ((GladeProject*)(project))->priv->stamp == (iter)->stamp)

G_DEFINE_TYPE_WITH_CODE (GladeProject, glade_project, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GladeProject)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
                                                glade_project_model_iface_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
                                                glade_project_drag_source_init))


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

  g_clear_object (&priv->css_provider);
  g_clear_object (&priv->css_monitor);
  
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

  g_object_unref (priv->model);
  
  G_OBJECT_CLASS (glade_project_parent_class)->dispose (object);
}

static void
glade_project_finalize (GObject *object)
{
  GladeProject *project = GLADE_PROJECT (object);
  GladeProjectPrivate *priv = project->priv;

  gtk_widget_destroy (priv->prefs_dialog);

  g_free (priv->path);
  g_free (priv->license);
  g_free (priv->css_provider_path);

  if (priv->comments)
    {
      g_list_foreach (priv->comments, (GFunc) g_free, NULL);
      g_list_free (priv->comments);
    }

  if (priv->unsaved_number > 0)
    glade_id_allocator_release (get_unsaved_number_allocator (),
                                priv->unsaved_number);

  g_hash_table_destroy (priv->target_versions_major);
  g_hash_table_destroy (priv->target_versions_minor);

  glade_name_context_destroy (priv->widget_names);

  G_OBJECT_CLASS (glade_project_parent_class)->finalize (object);
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
    case PROP_TRANSLATION_DOMAIN:
      g_value_set_string (value, project->priv->translation_domain);
      break;
    case PROP_TEMPLATE:
      g_value_set_object (value, project->priv->template);
      break;
    case PROP_RESOURCE_PATH:
      g_value_set_string (value, project->priv->resource_path);
      break;
    case PROP_LICENSE:
      g_value_set_string (value, project->priv->license);
      break;
    case PROP_CSS_PROVIDER_PATH:
      g_value_set_string (value, project->priv->css_provider_path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glade_project_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_TRANSLATION_DOMAIN:
      glade_project_set_translation_domain (GLADE_PROJECT (object),
                                            g_value_get_string (value));
      break;
    case PROP_TEMPLATE:
      glade_project_set_template (GLADE_PROJECT (object),
                                  g_value_get_object (value));
      break;
    case PROP_RESOURCE_PATH:
      glade_project_set_resource_path (GLADE_PROJECT (object),
                                       g_value_get_string (value));
      break;
    case PROP_LICENSE:
      glade_project_set_license (GLADE_PROJECT (object),
                                 g_value_get_string (value));
      break;
    case PROP_CSS_PROVIDER_PATH:
      glade_project_set_css_provider_path (GLADE_PROJECT (object),
                                           g_value_get_string (value));
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

static inline gchar *
glade_preview_get_pid_as_str (GladePreview *preview)
{
#ifdef G_OS_WIN32
  return g_strdup_printf ("%p", glade_preview_get_pid (preview));
#else
  return g_strdup_printf ("%d", glade_preview_get_pid (preview));
#endif
}

static void
glade_project_preview_exits (GladePreview *preview, GladeProject *project)
{
  gchar       *pidstr;

  pidstr  = glade_preview_get_pid_as_str (preview);
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
                            GladeCommand *command,
                            gboolean      forward)
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

static void 
glade_project_set_css_provider_forall (GtkWidget *widget, gpointer data)
{
  if (GLADE_IS_PLACEHOLDER (widget) || GLADE_IS_OBJECT_STUB (widget))
    return;

  gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
                                  GTK_STYLE_PROVIDER (data),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  
  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget), glade_project_set_css_provider_forall, data);
}

static void
glade_project_add_object_impl (GladeProject *project, GladeWidget *gwidget)
{
  GladeProjectPrivate *priv = project->priv;
  GObject *widget = glade_widget_get_object (gwidget);
  
  if (!priv->css_provider || !GTK_IS_WIDGET (widget))
    return;

  glade_project_set_css_provider_forall (GTK_WIDGET (widget), priv->css_provider);
}

/*******************************************************************
                          Class Initializers
 *******************************************************************/
static void
glade_project_init (GladeProject *project)
{
  GladeProjectPrivate *priv;
  GList *list;

  project->priv = priv = glade_project_get_instance_private (project);

  priv->path = NULL;
  priv->model = GTK_TREE_MODEL (gtk_tree_store_new (1, G_TYPE_OBJECT));

  g_signal_connect_swapped (priv->model, "row-changed",
                            G_CALLBACK (gtk_tree_model_row_changed),
                            project);
  g_signal_connect_swapped (priv->model, "row-inserted",
                            G_CALLBACK (gtk_tree_model_row_inserted),
                            project);
  g_signal_connect_swapped (priv->model, "row-has-child-toggled",
                            G_CALLBACK (gtk_tree_model_row_has_child_toggled),
                            project);
  g_signal_connect_swapped (priv->model, "row-deleted",
                            G_CALLBACK (gtk_tree_model_row_deleted),
                            project);
  g_signal_connect_swapped (priv->model, "rows-reordered",
                            G_CALLBACK (gtk_tree_model_rows_reordered),
                            project);
  
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

  priv->unsaved_number =
      glade_id_allocator_allocate (get_unsaved_number_allocator ());

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

  priv->prefs_dialog = glade_project_properties_new (project);
}

static void
glade_project_class_init (GladeProjectClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = glade_project_get_property;
  object_class->set_property = glade_project_set_property;
  object_class->finalize = glade_project_finalize;
  object_class->dispose = glade_project_dispose;

  klass->add_object = glade_project_add_object_impl;
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

  /**
   * GladeProject::add-signal-handler:
   * @gladeproject: the #GladeProject which received the signal.
   * @gladewidget: the #GladeWidget
   * @signal: the #GladeSignal that was added to @gladewidget.
   */
  glade_project_signals[ADD_SIGNAL_HANDLER] =
      g_signal_new ("add-signal-handler",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL,
                    _glade_marshal_VOID__OBJECT_OBJECT,
                    G_TYPE_NONE,
                    2,
                    GLADE_TYPE_WIDGET,
                    GLADE_TYPE_SIGNAL);

  /**
   * GladeProject::remove-signal-handler:
   * @gladeproject: the #GladeProject which received the signal.
   * @gladewidget: the #GladeWidget
   * @signal: the #GladeSignal that was removed from @gladewidget.
   */
  glade_project_signals[REMOVE_SIGNAL_HANDLER] =
      g_signal_new ("remove-signal-handler",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    0,
                     NULL, NULL,
                    _glade_marshal_VOID__OBJECT_OBJECT,
                    G_TYPE_NONE,
                    2,
                    GLADE_TYPE_WIDGET,
                    GLADE_TYPE_SIGNAL);

  /**
   * GladeProject::change-signal-handler:
   * @gladeproject: the #GladeProject which received the signal.
   * @gladewidget: the #GladeWidget
   * @old_signal: the old #GladeSignal that changed
   * @new_signal: the new #GladeSignal
   */
  glade_project_signals[CHANGE_SIGNAL_HANDLER] =
      g_signal_new ("change-signal-handler",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    0,
                     NULL, NULL,
                    _glade_marshal_VOID__OBJECT_OBJECT_OBJECT,
                    G_TYPE_NONE,
                    3,
                    GLADE_TYPE_WIDGET,
                    GLADE_TYPE_SIGNAL,
                    GLADE_TYPE_SIGNAL);

  /**
   * GladeProject::activate-signal-handler:
   * @gladeproject: the #GladeProject which received the signal.
   * @gladewidget: the #GladeWidget
   * @signal: the #GladeSignal that was activated
   */
  glade_project_signals[ACTIVATE_SIGNAL_HANDLER] =
      g_signal_new ("activate-signal-handler",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    0,
                     NULL, NULL,
                    _glade_marshal_VOID__OBJECT_OBJECT,
                    G_TYPE_NONE,
                    2,
                    GLADE_TYPE_WIDGET,
                    GLADE_TYPE_SIGNAL);

  glade_project_props[PROP_MODIFIED] =
    g_param_spec_boolean ("modified",
                          "Modified",
                          _("Whether project has been modified since it was last saved"),
                          FALSE,
                          G_PARAM_READABLE);

  glade_project_props[PROP_HAS_SELECTION] =
    g_param_spec_boolean ("has-selection",
                          _("Has Selection"),
                          _("Whether project has a selection"),
                          FALSE,
                          G_PARAM_READABLE);

  glade_project_props[PROP_PATH] =
    g_param_spec_string ("path",
                         _("Path"),
                         _("The filesystem path of the project"),
                         NULL,
                         G_PARAM_READABLE);

  glade_project_props[PROP_READ_ONLY] =
    g_param_spec_boolean ("read-only",
                          _("Read Only"),
                          _("Whether project is read-only"),
                          FALSE,
                          G_PARAM_READABLE);

  glade_project_props[PROP_ADD_ITEM] = 
    g_param_spec_object ("add-item",
                         _("Add Item"),
                         _("The current item to add to the project"),
                         GLADE_TYPE_WIDGET_ADAPTOR,
                         G_PARAM_READABLE);

  glade_project_props[PROP_POINTER_MODE] =
    g_param_spec_enum ("pointer-mode",
                       _("Pointer Mode"),
                       _("The currently effective GladePointerMode"),
                       GLADE_TYPE_POINTER_MODE,
                       GLADE_POINTER_SELECT,
                       G_PARAM_READABLE);

  glade_project_props[PROP_TRANSLATION_DOMAIN] =
    g_param_spec_string ("translation-domain",
                         _("Translation Domain"),
                         _("The project translation domain"),
                         NULL,
                         G_PARAM_READWRITE);
  
  glade_project_props[PROP_TEMPLATE] = 
    g_param_spec_object ("template",
                         _("Template"),
                         _("The project's template widget, if any"),
                         GLADE_TYPE_WIDGET,
                         G_PARAM_READWRITE);

  glade_project_props[PROP_RESOURCE_PATH] =
    g_param_spec_string ("resource-path",
                         _("Resource Path"),
                         _("Path to load images and resources in Glade's runtime"),
                         NULL,
                         G_PARAM_READWRITE);
  
  glade_project_props[PROP_LICENSE] =
    g_param_spec_string ("license",
                         _("License"),
                         _("License for this project, it will be added as a document level comment."),
                         NULL,
                         G_PARAM_READWRITE);

  glade_project_props[PROP_CSS_PROVIDER_PATH] =
    g_param_spec_string ("css-provider-path",
                         _("CSS Provider Path"),
                         _("Path to use as the custom CSS provider for this project."),
                         NULL,
                         G_PARAM_READWRITE);

  /* Install all properties */
  g_object_class_install_properties (object_class, N_PROPERTIES, glade_project_props);
}

/******************************************************************
 *                    GtkTreeModelIface                           *
 ******************************************************************/
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
      case GLADE_PROJECT_MODEL_COLUMN_WARNING:
        return G_TYPE_STRING;
      default:
        g_assert_not_reached ();
        return G_TYPE_NONE;
    }
}

static gboolean
glade_project_model_get_iter (GtkTreeModel *model,
                              GtkTreeIter  *iter,
                              GtkTreePath  *path)
{
  return gtk_tree_model_get_iter (GLADE_PROJECT (model)->priv->model, iter, path);
}

static GtkTreePath *
glade_project_model_get_path (GtkTreeModel *model, GtkTreeIter *iter)
{
  return gtk_tree_model_get_path (GLADE_PROJECT (model)->priv->model, iter);
}

static void
glade_project_model_get_value (GtkTreeModel *model,
                               GtkTreeIter  *iter,
                               gint          column,
                               GValue       *value)
{
  GladeWidget *widget;

  gtk_tree_model_get (GLADE_PROJECT (model)->priv->model, iter, 0, &widget, -1);

  value = g_value_init (value,
                        glade_project_model_get_column_type (model, column));

  switch (column)
    {
      case GLADE_PROJECT_MODEL_COLUMN_ICON_NAME:
        g_value_set_string (value, glade_widget_adaptor_get_icon_name (glade_widget_get_adaptor (widget)));
        break;
      case GLADE_PROJECT_MODEL_COLUMN_NAME:
        g_value_set_string (value, glade_widget_get_name (widget));
        break;
      case GLADE_PROJECT_MODEL_COLUMN_TYPE_NAME:
        {
          GladeWidgetAdaptor *adaptor = glade_widget_get_adaptor (widget);
          g_value_set_static_string (value, glade_widget_adaptor_get_display_name (adaptor));
          break;
        }
      case GLADE_PROJECT_MODEL_COLUMN_OBJECT:
        g_value_set_object (value, glade_widget_get_object (widget));
        break;
      case GLADE_PROJECT_MODEL_COLUMN_MISC:
        {
          gchar *str = NULL, *child_type;
          GladeProperty *ref_prop;

          /* special child type / internal child */
          if (glade_widget_get_internal (widget) != NULL)
            str = g_strdup_printf (_("(internal %s)"),
                                   glade_widget_get_internal (widget));
          else if ((child_type =
                    g_object_get_data (glade_widget_get_object (widget),
                                       "special-child-type")) != NULL)
            str = g_strdup_printf (_("(%s child)"), child_type);
          else if (glade_widget_get_is_composite (widget))
            str = g_strdup_printf (_("(template)"));
          else if ((ref_prop = 
                    glade_widget_get_parentless_widget_ref (widget)) != NULL)
            {
              GladePropertyDef *pdef       = glade_property_get_def (ref_prop);
              GladeWidget      *ref_widget = glade_property_get_widget (ref_prop);

              /* translators: refers to a property named '%s' of widget '%s' */
              str = g_strdup_printf (_("(%s of %s)"), 
                                     glade_property_def_get_name (pdef),
                                     glade_widget_get_name (ref_widget));
            }

          g_value_take_string (value, str);
        }
        break;
      case GLADE_PROJECT_MODEL_COLUMN_WARNING:
        g_value_set_string (value, glade_widget_support_warning (widget));
        break;

      default:
        g_assert_not_reached ();
    }
}

static gboolean
glade_project_model_iter_next (GtkTreeModel *model, GtkTreeIter *iter)
{
  return gtk_tree_model_iter_next (GLADE_PROJECT (model)->priv->model, iter);
}

static gboolean
glade_project_model_iter_has_child (GtkTreeModel *model, GtkTreeIter *iter)
{
  return gtk_tree_model_iter_has_child (GLADE_PROJECT (model)->priv->model, iter);
}

static gint
glade_project_model_iter_n_children (GtkTreeModel *model, GtkTreeIter *iter)
{
  return gtk_tree_model_iter_n_children (GLADE_PROJECT (model)->priv->model, iter);
}

static gboolean
glade_project_model_iter_nth_child (GtkTreeModel *model,
                                    GtkTreeIter  *iter,
                                    GtkTreeIter  *parent,
                                    gint          nth)
{
  return gtk_tree_model_iter_nth_child (GLADE_PROJECT (model)->priv->model,
                                        iter, parent, nth);
}

static gboolean
glade_project_model_iter_children (GtkTreeModel *model,
                                   GtkTreeIter  *iter,
                                   GtkTreeIter  *parent)
{
  return gtk_tree_model_iter_children (GLADE_PROJECT (model)->priv->model,
                                       iter, parent);
}

static gboolean
glade_project_model_iter_parent (GtkTreeModel *model,
                                 GtkTreeIter  *iter,
                                 GtkTreeIter  *child)
{
  return gtk_tree_model_iter_parent (GLADE_PROJECT (model)->priv->model,
                                     iter, child);
}

static void
glade_project_model_ref_node (GtkTreeModel *model, GtkTreeIter *iter)
{
  gtk_tree_model_ref_node (GLADE_PROJECT (model)->priv->model, iter);
}

static void
glade_project_model_unref_node (GtkTreeModel *model, GtkTreeIter *iter)
{
  gtk_tree_model_unref_node (GLADE_PROJECT (model)->priv->model, iter);
}


static void
glade_project_model_iface_init (GtkTreeModelIface *iface)
{
  iface->get_flags = glade_project_model_get_flags;
  iface->get_n_columns = glade_project_model_get_n_columns;
  iface->get_column_type = glade_project_model_get_column_type;
  iface->get_iter = glade_project_model_get_iter;
  iface->get_path = glade_project_model_get_path;
  iface->get_value = glade_project_model_get_value;
  iface->iter_next = glade_project_model_iter_next;
  iface->iter_children = glade_project_model_iter_children;
  iface->iter_has_child = glade_project_model_iter_has_child;
  iface->iter_n_children = glade_project_model_iter_n_children;
  iface->iter_nth_child = glade_project_model_iter_nth_child;
  iface->iter_parent = glade_project_model_iter_parent;
  iface->ref_node = glade_project_model_ref_node;
  iface->unref_node = glade_project_model_unref_node;
}

static gboolean 
glade_project_row_draggable (GtkTreeDragSource *drag_source, GtkTreePath *path)
{
  return TRUE;
}
  
static gboolean
glade_project_drag_data_delete (GtkTreeDragSource *drag_source, GtkTreePath *path)
{
  return FALSE;
}

static gboolean
glade_project_drag_data_get (GtkTreeDragSource *drag_source,
                             GtkTreePath       *path,
                             GtkSelectionData  *selection_data)
{
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (drag_source), &iter, path))
    {
      GObject *object;

      gtk_tree_model_get (GTK_TREE_MODEL (drag_source), &iter,
                          GLADE_PROJECT_MODEL_COLUMN_OBJECT, &object,
                          -1);

      _glade_dnd_set_data (selection_data, object);
      return TRUE;
    }

  return FALSE;
}

static void
glade_project_drag_source_init (GtkTreeDragSourceIface *iface)
{
  iface->row_draggable = glade_project_row_draggable;
  iface->drag_data_delete = glade_project_drag_data_delete;
  iface->drag_data_get = glade_project_drag_data_get;
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
          GladePropertyDef *def;

          property = GLADE_PROPERTY (ll->data);
          def      = glade_property_get_def (property);

          if (glade_property_def_is_object (def) &&
              (txt = g_object_get_data (G_OBJECT (property),
                                        "glade-loaded-object")) != NULL)
            {
              /* Parse the object list and set the property to it
               * (this magicly works for both objects & object lists)
               */
              value = glade_property_def_make_gvalue_from_string (def, txt, project);

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

static void
glade_project_fix_template (GladeProject *project)
{
  GList *l;
  gboolean composite = FALSE;

  for (l = project->priv->tree; l; l = l->next)
    {
      GObject     *obj = l->data;
      GladeWidget *gwidget;

      gwidget   = glade_widget_get_from_gobject (obj);
      composite = glade_widget_get_is_composite (gwidget);

      if (composite)
        {
          glade_project_set_template (project, gwidget);
          break;
        }
    }
}

static gchar *
gp_comment_strip_property (gchar *value, gchar *property)
{
  if (g_str_has_prefix (value, property))
    {
      gchar *start = value + strlen (property);

      if (*start == ' ')
        start++;

      memmove (value, start, strlen (start) + 1);
      return value;
    }

  return NULL;
}

static gchar *
gp_comment_get_content (GladeXmlNode *comment)
{
  gchar *value;

  if (glade_xml_node_is_comment (comment) &&
      (value = glade_xml_get_content (comment)))
    {
      gchar *compressed;
      
      /* Replace NON-BREAKING HYPHEN with regular HYPHEN */
      value = _glade_util_strreplace (g_strstrip (value), TRUE, "\\‑\\‑", "--");
      compressed = g_strcompress (value);
      g_free (value);
      return compressed;
    }

  return NULL;
}

static gchar *
glade_project_read_requires_from_comment (GladeXmlNode *comment,
                                          guint16      *major,
                                          guint16      *minor)
{
  gchar *value, *requires, *required_lib = NULL;
  
  if ((value = gp_comment_get_content (comment)) &&
      (requires = gp_comment_strip_property (value, "interface-requires")))
    {
      gchar buffer[128];
      gint maj, min;
      
      if (sscanf (requires, "%128s %d.%d", buffer, &maj, &min) == 3)
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
                             const gchar  *path,
                             gboolean     *has_gtk_dep)
{

  GString *string = g_string_new (NULL);
  GladeXmlNode *node;
  gboolean loadable = TRUE;
  guint16 major, minor;
  gint position = 0;

  for (node = glade_xml_node_get_children_with_comments (root_node);
       node; node = glade_xml_node_next_with_comments (node))
    {
      gchar *required_lib = NULL;

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
          GladePropertyDef *def;
          GParamSpec       *pspec;

          property = list->data;
          def      = glade_property_get_def (property);
          pspec    = glade_property_def_get_pspec (def);

          /* XXX We should have a "resource" flag on properties that need
           * to be loaded from the resource path, but that would require
           * that they can serialize both ways (custom properties are only
           * required to generate unique strings for value comparisons).
           */
          if (pspec->value_type == GDK_TYPE_PIXBUF ||
              pspec->value_type == G_TYPE_FILE)
            {
              GValue *value;
              gchar  *string;

              string = glade_property_make_string (property);
              value  = glade_property_def_make_gvalue_from_string (def, string, project);

              glade_property_set_value (property, value);

              g_value_unset (value);
              g_free (value);
              g_free (string);
            }
        }
    }
}

void
glade_project_set_resource_path (GladeProject *project, const gchar *path)
{
  g_return_if_fail (GLADE_IS_PROJECT (project));

  if (g_strcmp0 (project->priv->resource_path, path) != 0)
    {
      g_free (project->priv->resource_path);
      project->priv->resource_path = g_strdup (path);

      update_project_for_resource_path (project);

      g_object_notify_by_pspec (G_OBJECT (project), glade_project_props[PROP_RESOURCE_PATH]);
    }
}

const gchar *
glade_project_get_resource_path (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  return project->priv->resource_path;
}

void
glade_project_set_license (GladeProject *project, const gchar *license)
{
  GladeProjectPrivate *priv;
  
  g_return_if_fail (GLADE_IS_PROJECT (project));
  priv = project->priv;

  if ((!license && priv->license) ||
      (license && g_strcmp0 (priv->license, license) != 0))
    {
      g_free (priv->license);
      priv->license = g_strdup (license);
      g_object_notify_by_pspec (G_OBJECT (project), glade_project_props[PROP_LICENSE]);
    }
}

const gchar *
glade_project_get_license (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  return project->priv->license;
}

static void
glade_project_read_comment_properties (GladeProject *project,
                                       GladeXmlNode *root_node)
{
  GladeProjectPrivate *priv = project->priv;
  gchar *license, *name, *description, *copyright, *authors;
  GladeXmlNode *node;

  license = name = description = copyright = authors = NULL;
  
  for (node = glade_xml_node_get_children_with_comments (root_node);
       node; node = glade_xml_node_next_with_comments (node))
    {
      gchar *val;

      if (!(val = gp_comment_get_content (node)))
        continue;
      
      if (gp_comment_strip_property (val, "interface-local-resource-path"))
        glade_project_set_resource_path (project, val);
      else if (gp_comment_strip_property (val, "interface-css-provider-path"))
        {
          if (g_path_is_absolute (val))
            glade_project_set_css_provider_path (project, val);
          else
            {
              gchar *dirname = g_path_get_dirname (priv->path);
              gchar *full_path = g_build_filename (dirname, val, NULL);

              glade_project_set_css_provider_path (project, full_path);

              g_free (dirname);
              g_free (full_path);
            }
        }
      else if (!license && (license = gp_comment_strip_property (val, "interface-license-type")))
        continue;
      else if (!name && (name = gp_comment_strip_property (val, "interface-name")))
        continue;
      else if (!description && (description = gp_comment_strip_property (val, "interface-description")))
        continue;
      else if (!copyright && (copyright = gp_comment_strip_property (val, "interface-copyright")))
        continue;
      else if (!authors && (authors = gp_comment_strip_property (val, "interface-authors")))
        continue;

      g_free (val);
    }

  _glade_project_properties_set_license_data (GLADE_PROJECT_PROPERTIES (priv->prefs_dialog),
                                              license,
                                              name,
                                              description,
                                              copyright,
                                              authors);
  g_free (license);
  g_free (name);
  g_free (description);
  g_free (copyright);
  g_free (authors);
}

static inline void
glade_project_read_comments (GladeProject *project, GladeXmlNode *root)
{
  GladeProjectPrivate *priv = project->priv;
  GladeXmlNode *node;

  /* We only support comments before the root element */
  for (node = glade_xml_node_prev_with_comments (root); node;
       node = glade_xml_node_prev_with_comments (node))
    {     
      if (glade_xml_node_is_comment (node))
        {
          gchar *start, *comment = glade_xml_get_content (node);

          /* Ignore leading spaces */
          for (start = comment; *start && g_ascii_isspace (*start); start++);

          /* Do not load generated with glade comment! */
          if (g_str_has_prefix (start, GLADE_XML_COMMENT))
            {
              gchar *new_line = g_strstr_len (start, -1, "\n");

              if (new_line)
                glade_project_set_license (project, g_strstrip (new_line));

              g_free (comment);
              continue;
            }
          
          /* Since we are reading in backwards order,
           * prepending gives us the right order
           */
          priv->comments = g_list_prepend (priv->comments, comment);
        }
    }
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
  GladeSignalDef     *signal_def;
  GladeWidgetAdaptor *adaptor;
  gchar              *catalog = NULL;
  gboolean            is_gtk_adaptor = FALSE;

  signal_def =
    glade_widget_adaptor_get_signal_def (glade_widget_get_adaptor (data->widget), 
                                         glade_signal_get_name (signal));

  /*  unknown signal... can it happen ? */
  if (!signal_def) 
    return;

  adaptor = glade_signal_def_get_adaptor (signal_def);

  /* Check if the signal comes from a GTK+ widget class */
  g_object_get (adaptor, "catalog", &catalog, NULL);
  if (strcmp (catalog, "gtk+") == 0)
    is_gtk_adaptor = TRUE;
  g_free (catalog);

  if (is_gtk_adaptor && 
      !GSC_VERSION_CHECK (signal_def, data->major, data->minor))
    {
      data->major = glade_signal_def_since_major (signal_def);
      data->minor = glade_signal_def_since_minor (signal_def);
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
          GladePropertyDef   *pdef = glade_property_get_def (property);
          GladeWidgetAdaptor *prop_adaptor, *adaptor;
          GParamSpec         *pspec;

          /* Unset properties ofcourse dont count... */
          if (glade_property_original_default (property))
            continue;

          /* Check if this property originates from a GTK+ widget class */
          pspec        = glade_property_def_get_pspec (pdef);
          prop_adaptor = glade_property_def_get_adaptor (pdef);
          adaptor      = glade_widget_adaptor_from_pspec (prop_adaptor, pspec);

          catalog = NULL;
          is_gtk_adaptor = FALSE;
          g_object_get (adaptor, "catalog", &catalog, NULL);
          if (strcmp (catalog, "gtk+") == 0)
            is_gtk_adaptor = TRUE;
          g_free (catalog);

          /* Check GTK+ property class versions */
          if (is_gtk_adaptor &&
              !GLADE_PROPERTY_DEF_VERSION_CHECK (pdef, target_major, target_minor))
            {
              target_major = glade_property_def_since_major (pdef);
              target_minor = glade_property_def_since_minor (pdef);
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
                                 gint          count)
{
  GladeXmlNode *node;

  for (node = glade_xml_node_get_children (root);
       node; node = glade_xml_node_next (node))
    {
      if (glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
          glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE))
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
  GString *text = NULL;
  GList *l;
  gchar *project_name;

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
      GString *missing_types = g_string_new (NULL);
      text = g_string_new (NULL);

      for (l = classes; l; l = g_list_next (l))
        {
          if (g_list_previous (l)) {
            g_string_append_printf (missing_types, _(", %s"), (const gchar *)l->data);
          } else
            g_string_append (missing_types, l->data);
        }

      g_list_free (classes);

      g_string_printf (text,
                       ngettext ("Especially because there is %d object that can not be built with type: %s",
                                 "Especially because there are %d objects that can not be built with types: %s", unknown_objects),
                       unknown_objects,
                       missing_types->str);

      g_string_free (missing_types, TRUE);
    }

  project_name = glade_project_get_name (project);
  dialog = gtk_message_dialog_new (GTK_WINDOW (glade_app_get_window ()),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
                                   PROJECT_TARGET_DIALOG_TITLE_FMT,
                                   project_name,
                                   major, minor);
  g_free (project_name);

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

static gchar *
glade_project_autosave_name (const gchar *path)
{
  gchar *basename, *dirname, *autoname;
  gchar *autosave_name;

  basename = g_path_get_basename (path);
  dirname = g_path_get_dirname (path);

  autoname = g_strdup_printf ("#%s#", basename);
  autosave_name = g_build_filename (dirname, autoname, NULL);

  g_free (basename);
  g_free (dirname);
  g_free (autoname);

  return autosave_name;
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
  gchar *domain;
  gint count;
  gchar *autosave_path;
  time_t mtime, autosave_mtime;
  gchar *load_path = NULL;

  /* Check if an autosave is more recent then the specified file */
  autosave_path = glade_project_autosave_name (priv->path);
  autosave_mtime = glade_util_get_file_mtime (autosave_path, NULL);
  mtime = glade_util_get_file_mtime (priv->path, NULL);

  if (autosave_mtime > mtime)
    {
      gchar *display_name;

      display_name = glade_project_get_name (project);

      if (glade_util_ui_message (glade_app_get_window (),
                                 GLADE_UI_YES_OR_NO, NULL,
                                 _("An automatically saved version of `%s' is more recent.\n\n"
                                   "Would you like to load the autosave version instead?"),
                                 display_name))
        {
          mtime = autosave_mtime;
          load_path = g_strdup (autosave_path);
        }
      g_free (display_name);
    }

  g_free (autosave_path);

  priv->selection = NULL;
  priv->objects = NULL;
  priv->loading = TRUE;

  _glade_xml_error_reset_last ();

  /* get the context & root node of the catalog file */
  if (!(context =
        glade_xml_context_new_from_path (load_path ? load_path : priv->path, NULL, NULL)))
    {
      gchar *message = _glade_xml_error_get_last_message ();

      if (message)
        {
          gchar *escaped = g_markup_escape_text (message, -1);
          glade_util_ui_message (glade_app_get_window (), GLADE_UI_ERROR, NULL, "%s", escaped);
          g_free (escaped);
          g_free (message);
        }
      else
        glade_util_ui_message (glade_app_get_window (), GLADE_UI_ERROR, NULL,
                               "Couldn't open glade file [%s].",
                               load_path ? load_path : priv->path);

      g_free (load_path);
      priv->loading = FALSE;
      return FALSE;
    }

  priv->mtime = mtime;

  doc = glade_xml_context_get_doc (context);
  root = glade_xml_doc_get_root (doc);

  if (!glade_xml_node_verify_silent (root, GLADE_XML_TAG_PROJECT))
    {
      g_warning ("Couldnt recognize GtkBuilder xml, skipping %s",
                 load_path ? load_path : priv->path);
      glade_xml_context_free (context);
      g_free (load_path);
      priv->loading = FALSE;
      return FALSE;
    }

  /* Emit "parse-began" signal */
  g_signal_emit (project, glade_project_signals[PARSE_BEGAN], 0);

  if ((domain = glade_xml_get_property_string (root, GLADE_TAG_DOMAIN)))
    {
      glade_project_set_translation_domain (project, domain);
      g_free (domain);
    }

  glade_project_read_comments (project, root);

  /* Read requieres, and do not abort load if there are missing catalog since
   * GladeObjectStub is created to keep the original xml for unknown object classes
   */
  glade_project_read_requires (project, root, load_path ? load_path : priv->path, &has_gtk_dep);

  /* Read the rest of properties saved as comments */
  glade_project_read_comment_properties (project, root);

  /* Launch a dialog if it's going to take enough time to be
   * worth showing at all */
  count = glade_project_count_xml_objects (project, root, 0);
  priv->progress_full = count;
  priv->progress_step = 0;

  for (node = glade_xml_node_get_children (root);
       node; node = glade_xml_node_next (node))
    {
      /* Skip "requires" tags */
      if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
            glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
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
      g_free (load_path);
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

  glade_project_fix_template (project);

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

static void
glade_project_update_properties_title (GladeProject *project)
{
  gchar *name, *title;

  /* Update prefs dialogs here... */
  name = glade_project_get_name (project);
  title = g_strdup_printf (_("%s document properties"), name);
  gtk_window_set_title (GTK_WINDOW (project->priv->prefs_dialog), title);
  g_free (title);
  g_free (name); 
}

gboolean
glade_project_load_from_file (GladeProject *project, const gchar *path)
{
  gboolean retval;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

  project->priv->path = glade_util_canonical_path (path);
  g_object_notify_by_pspec (G_OBJECT (project), glade_project_props[PROP_PATH]);

  if ((retval = glade_project_load_internal (project)))
    glade_project_update_properties_title (project);

  return retval;
}

/**
 * glade_project_load:
 * @path: the path of the project to load
 * 
 * Opens a project at the given path.
 *
 * Returns: (transfer full) (nullable): a new #GladeProject for the opened project on success,
 *          %NULL on failure
 */
GladeProject *
glade_project_load (const gchar *path)
{
  GladeProject *project;

  g_return_val_if_fail (path != NULL, NULL);

  project = g_object_new (GLADE_TYPE_PROJECT, NULL);

  project->priv->path = glade_util_canonical_path (path);

  if (glade_project_load_internal (project))
    {
      glade_project_update_properties_title (project);
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
#define GLADE_PROJECT_COMMENT " "GLADE_XML_COMMENT" "PACKAGE_VERSION" "

static void
glade_project_write_comment_property (GladeProject    *project,
                                      GladeXmlContext *context,
                                      GladeXmlNode    *root,
                                      const gchar     *property,
                                      gchar           *value)
{
  gchar *comment, *escaped;
  GladeXmlNode *path_node;

  if (!value || *value == '\0')
    return;

  /* The string "--" (double hyphen) is not allowed in xml comments, so we replace
   * the regular HYPHEN with a NON-BREAKING HYPHEN which look the same but have
   * a different encoding.
   */
  escaped = _glade_util_strreplace (g_strescape (value, "‑"), TRUE, "--", "\\‑\\‑");
  
  comment = g_strconcat (" ", property, " ", escaped, " ", NULL);
  path_node = glade_xml_node_new_comment (context, comment);
  glade_xml_node_append_child (root, path_node);

  g_free (escaped);
  g_free (comment);
}

static void
glade_project_write_required_libs (GladeProject    *project,
                                   GladeXmlContext *context,
                                   GladeXmlNode    *root)
{
  gboolean supports_require_tag;
  GladeXmlNode *req_node;
  GList *required, *list;
  gint major, minor;

  glade_project_get_target_version (project, "gtk+", &major, &minor);
  supports_require_tag = GLADE_GTKBUILDER_HAS_VERSIONING (major, minor);

  if ((required = glade_project_required_libs (project)) != NULL)
    {
      gchar version[16];

      for (list = required; list; list = list->next)
        {
          gchar *library = list->data;

          glade_project_get_target_version (project, library, &major, &minor);

          g_snprintf (version, sizeof (version), "%d.%d", major, minor);

          /* Write the standard requires tag */
          if (supports_require_tag)
            {
              req_node = glade_xml_node_new (context, GLADE_XML_TAG_REQUIRES);
              glade_xml_node_set_property_string (req_node,
                                                  GLADE_XML_TAG_LIB,
                                                  library);
              glade_xml_node_set_property_string (req_node,
                                                  GLADE_XML_TAG_VERSION,
                                                  version);
              glade_xml_node_append_child (root, req_node);
            }
          else
            {
              gchar *value = g_strconcat (library, " ", version, NULL);
              glade_project_write_comment_property (project, context, root,
                                                    "interface-requires",
                                                    value);
              g_free (value);
            }
        }
      g_list_free_full (required, g_free);
    }
}

static void
glade_project_write_resource_path (GladeProject    *project,
                                   GladeXmlContext *context,
                                   GladeXmlNode    *root)
{
  glade_project_write_comment_property (project, context, root,
                                        "interface-local-resource-path",
                                        project->priv->resource_path);
}

static void
glade_project_write_css_provider_path (GladeProject    *project,
                                       GladeXmlContext *context,
                                       GladeXmlNode    *root)
{
  GladeProjectPrivate *priv = project->priv;
  gchar *dirname;

  if (priv->css_provider_path && priv->path &&
      (dirname = g_path_get_dirname (priv->path)))
    {
      GFile *project_path = g_file_new_for_path (dirname);
      GFile *file_path = g_file_new_for_path (priv->css_provider_path);
      gchar *css_provider_path;

      css_provider_path = g_file_get_relative_path (project_path, file_path);

      if (css_provider_path)
        glade_project_write_comment_property (project, context, root,
                                              "interface-css-provider-path",
                                              css_provider_path);
      else
        g_warning ("g_file_get_relative_path () return NULL");

      g_object_unref (project_path);
      g_object_unref (file_path);
      g_free (css_provider_path);
      g_free (dirname);
    }
}

static void
glade_project_write_license_data (GladeProject    *project,
                                  GladeXmlContext *context,
                                  GladeXmlNode    *root)
{
  gchar *license, *name, *description, *copyright, *authors;
  
  _glade_project_properties_get_license_data (GLADE_PROJECT_PROPERTIES (project->priv->prefs_dialog),
                                              &license,
                                              &name,
                                              &description,
                                              &copyright,
                                              &authors);

  if (!license)
    return;
  
  glade_project_write_comment_property (project, context, root,
                                        "interface-license-type",
                                        license);
  glade_project_write_comment_property (project, context, root,
                                        "interface-name",
                                        name);
  glade_project_write_comment_property (project, context, root,
                                        "interface-description",
                                        description);
  glade_project_write_comment_property (project, context, root,
                                        "interface-copyright",
                                        copyright);
  glade_project_write_comment_property (project, context, root,
                                        "interface-authors",
                                        authors);

  g_free (license);
  g_free (name);
  g_free (description);
  g_free (copyright);
  g_free (authors);
}

static gint
glade_widgets_name_cmp (gconstpointer ga, gconstpointer gb)
{
  return g_strcmp0 (glade_widget_get_name ((gpointer)ga),
                    glade_widget_get_name ((gpointer)gb));
}

static gint
glade_node_edge_name_cmp (gconstpointer ga, gconstpointer gb)
{
  _NodeEdge *na = (gpointer)ga, *nb = (gpointer)gb;
  return g_strcmp0 (glade_widget_get_name (nb->successor),
                    glade_widget_get_name (na->successor));
}

static inline gboolean
glade_project_widget_hard_depends (GladeWidget *widget, GladeWidget *another)
{
  GList *l;

  for (l = _glade_widget_peek_prop_refs (another); l; l = g_list_next (l))
    {
      GladePropertyDef *def;
      
      /* If one of the properties that reference @another is
       * owned by @widget then @widget depends on @another
       */
      if (glade_property_get_widget (l->data) == widget &&
          (def = glade_property_get_def (l->data)) &&
          glade_property_def_get_construct_only (def))
        return TRUE;
    }

  return FALSE;
}

static GList *
glade_project_get_graph_deps (GladeProject *project)
{
  GladeProjectPrivate *priv = project->priv;
  GList *l, *edges = NULL;
  
  /* Create edges of the directed graph */
  for (l = priv->objects; l; l = g_list_next (l))
    {
      GladeWidget *predecessor = glade_widget_get_from_gobject (l->data);
      GladeWidget *predecessor_top;
      GList *ll;

      predecessor_top = glade_widget_get_toplevel (predecessor);

      /* Adds dependencies based on properties refs */
      for (ll = _glade_widget_peek_prop_refs (predecessor); ll; ll = g_list_next (ll))
        {
          GladeWidget *successor = glade_property_get_widget (ll->data);
          GladeWidget *successor_top;

          /* Ignore widgets that are not part of this project. (ie removed ones) */
          if (glade_widget_get_project (successor) != project)
            continue;

          successor_top = glade_widget_get_toplevel (successor);

          /* Ignore objects within the same toplevel */
          if (predecessor_top != successor_top)
            edges = _node_edge_prepend (edges, predecessor_top, successor_top);
        }
    }

  return edges;
}

static GList *
glade_project_get_nodes_from_edges (GladeProject *project, GList *edges)
{
  GList *l, *hard_edges = NULL;
  GList *cycles = NULL;

  /* Collect widgets with circular dependencies */
  for (l = edges; l; l = g_list_next (l))
    {
      _NodeEdge *edge = l->data;

      if (glade_widget_get_parent (edge->successor) == edge->predecessor ||
          glade_project_widget_hard_depends (edge->predecessor, edge->successor))
        hard_edges = _node_edge_prepend (hard_edges, edge->predecessor, edge->successor);

      /* Just toplevels */
      if (glade_widget_get_parent (edge->successor))
        continue;

      if (!g_list_find (cycles, edge->successor))
        cycles = g_list_prepend (cycles, edge->successor);
    }

  /* Sort them alphabetically */
  cycles = g_list_sort (cycles, glade_widgets_name_cmp);

  if (!hard_edges)
    return cycles;

  /* Sort them by hard deps */
  cycles = _glade_tsort (&cycles, &hard_edges);
  
  if (hard_edges)
    {
      GList *l, *hard_cycles = NULL;

      /* Collect widgets with hard circular dependencies */
      for (l = hard_edges; l; l = g_list_next (l))
        {
          _NodeEdge *edge = l->data;

          /* Just toplevels */
          if (glade_widget_get_parent (edge->successor))
            continue;

          if (!g_list_find (hard_cycles, edge->successor))
            hard_cycles = g_list_prepend (hard_cycles, edge->successor);
        }

      /* And append to the end of the cycles list */
      cycles = g_list_concat (cycles, g_list_sort (hard_cycles, glade_widgets_name_cmp));

      /* Opps! there is at least one hard circular dependency,
       * GtkBuilder will fail to set one of the properties that create the cycle
       */

      _node_edge_list_free (hard_edges);
    }

  return cycles;
}

static GList *
glade_project_add_hardcoded_dependencies (GList *edges, GladeProject *project)
{
  GList *l, *toplevels = project->priv->tree;

  /* Iterate over every toplevel */
  for (l = toplevels; l; l = g_list_next (l))
    {
      GObject *predecessor = l->data;

      /* Looking for a GtkIconFactory */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      if (GTK_IS_ICON_FACTORY (predecessor))
        {
          GladeWidget *predecessor_top = glade_widget_get_from_gobject (predecessor);
          GList *ll;

          /* add dependency on the GtkIconFactory on every toplevel */
          for (ll = toplevels; ll; ll = g_list_next (ll))
            {          
              GObject *successor = ll->data;

              /* except for GtkIconFactory */
              if (!GTK_IS_ICON_FACTORY (successor))
                edges = _node_edge_prepend (edges, predecessor_top,
                                            glade_widget_get_from_gobject (successor));
            }
        }
G_GNUC_END_IGNORE_DEPRECATIONS
    }

  return edges;
}

static GList *
glade_project_get_ordered_toplevels (GladeProject *project)
{
  GladeProjectPrivate *priv = project->priv;
  GList *l, *sorted_tree, *tree = NULL;
  GList *edges;

  /* Create list of toplevels GladeWidgets */
  for (l = priv->tree; l; l = g_list_next (l))
    tree = g_list_prepend (tree, glade_widget_get_from_gobject (l->data));

  /* Get dependency graph */
  edges = glade_project_get_graph_deps (project);

  /* Added hardcoded dependencies */
  edges = glade_project_add_hardcoded_dependencies (edges, project);
    
  /* Sort toplevels alphabetically */
  tree = g_list_sort (tree, glade_widgets_name_cmp);

  /* Sort dep graph alphabetically using successor name.
   * _glade_tsort() is a stable sort algorithm so, output of nodes without
   * dependency depends on the input order
   */
  edges = g_list_sort (edges, glade_node_edge_name_cmp);
  
  /* Sort tree */
  sorted_tree = _glade_tsort (&tree, &edges);

  if (edges)
    {
      GList *cycles = glade_project_get_nodes_from_edges (project, edges);
      
      /* And append to the end of the sorted list */
      sorted_tree = g_list_concat (sorted_tree, cycles);

      _node_edge_list_free (edges);
    }

  /* No need to free tree as tsort will consume the list */
  return sorted_tree;
}

static inline void
glade_project_write_comments (GladeProject *project,
                              GladeXmlDoc  *doc,
                              GladeXmlNode *root)
{
  GladeProjectPrivate *priv = project->priv;
  GladeXmlNode *comment_node, *node;
  GList *l;

  if (priv->license)
    {
      /* Replace regular HYPHEN with NON-BREAKING HYPHEN */
      gchar *license = _glade_util_strreplace (priv->license, FALSE, "--", "‑‑");
      gchar *comment = g_strdup_printf (GLADE_PROJECT_COMMENT"\n\n%s\n\n", license);
      g_free (license);
      comment_node = glade_xml_doc_new_comment (doc, comment);
      g_free (comment);
    }
  else
    comment_node = glade_xml_doc_new_comment (doc, GLADE_PROJECT_COMMENT);

  comment_node = glade_xml_node_add_prev_sibling (root, comment_node);
  
  for (l = priv->comments; l; l = g_list_next (l))
    {
      node = glade_xml_doc_new_comment (doc, l->data);
      comment_node = glade_xml_node_add_next_sibling (comment_node, node);
    }
}

static GladeXmlContext *
glade_project_write (GladeProject *project)
{
  GladeProjectPrivate *priv = project->priv;
  GladeXmlContext *context;
  GladeXmlDoc *doc;
  GladeXmlNode *root;
  GList *list;
  GList *toplevels;

  doc = glade_xml_doc_new ();
  context = glade_xml_context_new (doc, NULL);
  root = glade_xml_node_new (context, GLADE_XML_TAG_PROJECT);
  glade_xml_doc_set_root (doc, root);

  if (priv->translation_domain)
    glade_xml_node_set_property_string (root, GLADE_TAG_DOMAIN, priv->translation_domain);
  
  glade_project_write_comments (project, doc, root);

  glade_project_write_required_libs (project, context, root);

  glade_project_write_resource_path (project, context, root);

  glade_project_write_css_provider_path (project, context, root);

  glade_project_write_license_data (project, context, root);

  /* Get sorted toplevels */
  toplevels = glade_project_get_ordered_toplevels (project);

  for (list = toplevels; list; list = g_list_next (list))
    {
      GladeWidget *widget = list->data;

      /* 
       * Append toplevel widgets. Each widget then takes
       * care of appending its children.
       */
      if (!glade_widget_get_parent (widget))
        glade_widget_write (widget, context, root);
      else
        g_warning ("Tried to save a non toplevel object '%s' at xml root",
                   glade_widget_get_name (widget));
    }

  g_list_free (toplevels);

  return context;
}

/**
 * glade_project_backup:
 * @project: a #GladeProject
 * @path: location to save glade file
 * @error: an error from the G_FILE_ERROR domain.
 *
 * Backup the last file which @project has saved to
 * or was loaded from.
 *
 * If @path is not the same as the current project
 * path, then the current project path will be
 * backed up under the new location.
 *
 * If this the first save, and no persisted file
 * exists, then %TRUE is returned and no backup is made.
 *
 * Returns: %TRUE on success, %FALSE on failure
 */
gboolean
glade_project_backup (GladeProject *project, const gchar *path, GError **error)
{
  gchar *canonical_path;
  gchar *destination_path;
  gchar *content = NULL;
  gsize  length = 0;
  gboolean success;

  g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

  if (project->priv->path == NULL)
    return TRUE;

  canonical_path = glade_util_canonical_path (path);
  destination_path = g_strconcat (canonical_path, "~", NULL);
  g_free (canonical_path);

  success = g_file_get_contents (project->priv->path, &content, &length, error);
  if (success)
    {
      success = g_file_set_contents (destination_path, content, length, error);
      g_free (content);
    }

  g_free (destination_path);

  return success;
}

/**
 * glade_project_autosave:
 * @project: a #GladeProject
 * @error: an error from the G_FILE_ERROR domain.
 * 
 * Saves an autosave snapshot of @project to it's currently set path
 *
 * If the project was never saved, nothing is done and %TRUE is returned.
 *
 * Returns: %TRUE on success, %FALSE on failure
 */
gboolean
glade_project_autosave (GladeProject *project, GError **error)
{

  GladeXmlContext *context;
  GladeXmlDoc *doc;
  gchar *autosave_path;
  gint ret;

  g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

  if (project->priv->path == NULL)
    return TRUE;

  autosave_path = glade_project_autosave_name (project->priv->path);

  context = glade_project_write (project);
  doc = glade_xml_context_get_doc (context);
  ret = glade_xml_doc_save (doc, autosave_path);
  glade_xml_context_free (context);

  g_free (autosave_path);

  return ret > 0;
}

static inline void
update_project_resource_path (GladeProject *project, gchar *path)
{
  GFile *new_resource_path;
  GList *l;

  new_resource_path = g_file_new_for_path (path);

  for (l = project->priv->objects; l; l = l->next)
    {
      GladeWidget *widget = glade_widget_get_from_gobject (l->data);
      GList *list;

      for (list = glade_widget_get_properties (widget); list; list = list->next)
        {
          GladeProperty    *property = list->data;
          GladePropertyDef *def      = glade_property_get_def (property);
          GParamSpec       *pspec    = glade_property_def_get_pspec (def);

          if (pspec->value_type == GDK_TYPE_PIXBUF)
            {
              gchar *fullpath, *relpath;
              const gchar *filename;
              GFile *fullpath_file;
              GObject *pixbuf;

              glade_property_get (property, &pixbuf);
              if (pixbuf == NULL)
                continue;

              filename = g_object_get_data (pixbuf, "GladeFileName");
              fullpath = glade_project_resource_fullpath (project, filename);
              fullpath_file = g_file_new_for_path (fullpath);
              relpath = _glade_util_file_get_relative_path (new_resource_path,
                                                            fullpath_file);
              g_object_set_data_full (pixbuf, "GladeFileName", relpath, g_free);

              g_object_unref (fullpath_file);
              g_free (fullpath);
            }
        }
    }

  g_object_unref (new_resource_path);
}

static inline void
sync_project_resource_path (GladeProject *project)
{
  GList *l;

  for (l = glade_project_selection_get (project); l; l = l->next)
    {
      GladeWidget *widget = glade_widget_get_from_gobject (l->data);
      GList *list;

      for (list = glade_widget_get_properties (widget); list; list = list->next)
        {
          GladeProperty    *property = list->data;
          GladePropertyDef *def      = glade_property_get_def (property);
          GParamSpec       *pspec    = glade_property_def_get_pspec (def);

          if (pspec->value_type == GDK_TYPE_PIXBUF)
            {
              const gchar *filename;
              GObject *pixbuf;
              GValue *value;

              glade_property_get (property, &pixbuf);
              if (pixbuf == NULL)
                continue;

              filename = g_object_get_data (pixbuf, "GladeFileName");
              value = glade_property_def_make_gvalue_from_string (def,
                                                                  filename,
                                                                  project);
              glade_property_set_value (property, value);
              g_value_unset (value);
              g_free (value);
            }
        }
    }
}

/**
 * glade_project_save_verify:
 * @project: a #GladeProject
 * @path: location to save glade file
 * @flags: the #GladeVerifyFlags to warn about
 * @error: an error from the %G_FILE_ERROR domain.
 * 
 * Saves @project to the given path. 
 *
 * Returns: %TRUE on success, %FALSE on failure
 */
gboolean
glade_project_save_verify (GladeProject      *project,
                           const gchar       *path,
                           GladeVerifyFlags   flags,
                           GError           **error)
{
  GladeXmlContext *context;
  GladeXmlDoc *doc;
  gchar *canonical_path;
  gint ret;
  gchar *autosave_path;

  g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

  if (glade_project_is_loading (project))
    return FALSE;

  if (!glade_project_verify (project, TRUE, flags))
    return FALSE;

  /* Delete any autosaves at this point, if they exist */
  if (project->priv->path)
    {
      autosave_path = glade_project_autosave_name (project->priv->path);
      g_unlink (autosave_path);
      g_free (autosave_path);
    }

  if (!project->priv->resource_path)
    {
      /* Fix pixbuf paths: Since there is no resource_path, images are relative
       * to path or CWD so they need to be updated to be relative to @path
       */
      gchar *dirname = g_path_get_dirname (path);
      update_project_resource_path (project, dirname);
      g_free (dirname);
    }

  /* Save the project */
  context = glade_project_write (project);
  doc = glade_xml_context_get_doc (context);
  ret = glade_xml_doc_save (doc, path);
  glade_xml_context_free (context);

  canonical_path = glade_util_canonical_path (path);
  g_assert (canonical_path);

  if (project->priv->path == NULL ||
      strcmp (canonical_path, project->priv->path))
    {
      project->priv->path = (g_free (project->priv->path),
                             g_strdup (canonical_path));
      g_object_notify_by_pspec (G_OBJECT (project), glade_project_props[PROP_PATH]);

      glade_project_update_properties_title (project);

      /* Sync selected objects pixbuf properties */
      sync_project_resource_path (project);
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
 * glade_project_save:
 * @project: a #GladeProject
 * @path: location to save glade file
 * @error: an error from the %G_FILE_ERROR domain.
 * 
 * Saves @project to the given path. 
 *
 * Returns: %TRUE on success, %FALSE on failure
 */
gboolean
glade_project_save (GladeProject *project, const gchar *path, GError **error)
{
  return glade_project_save_verify (project, path,
                                    GLADE_VERIFY_VERSIONS |
                                    GLADE_VERIFY_UNRECOGNIZED,
                                    error);
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

  project->priv->writing_preview = TRUE;
  context = glade_project_write (project);
  project->priv->writing_preview = FALSE;

  text = glade_xml_dump_from_context (context);
  glade_xml_context_free (context);

  gwidget = glade_widget_get_toplevel (gwidget);
  if (!GTK_IS_WIDGET (glade_widget_get_object (gwidget)))
    return;

  if ((pidstr = g_object_get_data (G_OBJECT (gwidget), "preview")) != NULL)
    preview = g_hash_table_lookup (project->priv->previews, pidstr);

  if (!preview)
    {
      /* If the previewer program is somehow missing, this can return NULL */
      preview = glade_preview_launch (gwidget, text);
      g_return_if_fail (GLADE_IS_PREVIEW (preview));

      /* Leave project data on the preview */
      g_object_set_data (G_OBJECT (preview), "project", project);

      /* Leave preview data on the widget */
      g_object_set_data_full (G_OBJECT (gwidget),
                              "preview", 
                              glade_preview_get_pid_as_str (preview),
                              g_free);

      g_signal_connect (preview, "exits",
                        G_CALLBACK (glade_project_preview_exits),
                        project);

      /* Add preview to list of previews */
      g_hash_table_insert (project->priv->previews,
                           glade_preview_get_pid_as_str (preview),
                           preview);
    }
  else
    {
      glade_preview_update (preview, text);
    }

  g_free (text);
}

gboolean
glade_project_writing_preview (GladeProject       *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

  return project->priv->writing_preview;
}

/*******************************************************************
     Verify code here (versioning, incompatability checks)
 *******************************************************************/

/* Defined here for pretty translator comments (bug in intl tools, for some reason
 * you can only comment about the line directly following, forcing you to write
 * ugly messy code with comments in line breaks inside function calls).
 */

/* translators: refers to a widget in toolkit version '%s %d.%d' and a project targeting toolkit version '%s %d.%d' */
#define WIDGET_VERSION_CONFLICT_MSGFMT _("This widget was introduced in %s %d.%d " \
                                         "while project targets %s %d.%d")

/* translators: refers to a widget '[%s]' introduced in toolkit version '%s %d.%d' */
#define WIDGET_VERSION_CONFLICT_FMT    _("[%s] Object class '<b>%s</b>' was introduced in %s %d.%d\n")

#define WIDGET_DEPRECATED_MSG          _("This widget is deprecated")

/* translators: refers to a widget '[%s]' loaded from toolkit version '%s %d.%d' */
#define WIDGET_DEPRECATED_FMT          _("[%s] Object class '<b>%s</b>' from %s %d.%d is deprecated\n")


/* translators: refers to a property in toolkit version '%s %d.%d' 
 * and a project targeting toolkit version '%s %d.%d' */
#define PROP_VERSION_CONFLICT_MSGFMT   _("This property was introduced in %s %d.%d " \
                                         "while project targets %s %d.%d")

/* translators: refers to a property '%s' of widget '[%s]' in toolkit version '%s %d.%d' */
#define PROP_VERSION_CONFLICT_FMT      _("[%s] Property '<b>%s</b>' of object class '<b>%s</b>' " \
                                         "was introduced in %s %d.%d\n")

/* translators: refers to a property '%s' of widget '[%s]' in toolkit version '%s %d.%d' */
#define PACK_PROP_VERSION_CONFLICT_FMT _("[%s] Packing property '<b>%s</b>' of object class '<b>%s</b>' " \
                                         "was introduced in %s %d.%d\n")

#define PROP_DEPRECATED_MSG            _("This property is deprecated")

/* translators: refers to a property '%s' of widget '[%s]' */
#define PROP_DEPRECATED_FMT            _("[%s] Property '<b>%s</b>' of object class '<b>%s</b>' is deprecated\n")

/* translators: refers to a signal in toolkit version '%s %d.%d' 
 * and a project targeting toolkit version '%s %d.%d' */
#define SIGNAL_VERSION_CONFLICT_MSGFMT _("This signal was introduced in %s %d.%d " \
                                         "while project targets %s %d.%d")

/* translators: refers to a signal '%s' of widget '[%s]' in toolkit version '%s %d.%d' */
#define SIGNAL_VERSION_CONFLICT_FMT    _("[%s] Signal '<b>%s</b>' of object class '<b>%s</b>' " \
                                         "was introduced in %s %d.%d\n")

#define SIGNAL_DEPRECATED_MSG          _("This signal is deprecated")

/* translators: refers to a signal '%s' of widget '[%s]' */
#define SIGNAL_DEPRECATED_FMT          _("[%s] Signal '<b>%s</b>' of object class '<b>%s</b>' is deprecated\n")


static void
glade_project_verify_property_internal (GladeProject    *project,
                                        GladeProperty   *property,
                                        const gchar     *path_name,
                                        GString         *string,
                                        gboolean         forwidget,
                                        GladeVerifyFlags flags)
{
  GladeWidgetAdaptor *adaptor, *prop_adaptor;
  GladePropertyDef   *pdef;
  GParamSpec         *pspec;
  gint target_major, target_minor;
  gchar *catalog, *tooltip;

  /* For verification lists, we're only interested in verifying the 'used' state of properties.
   *
   * For the UI on the other hand, we want to show warnings on unset properties until they
   * are set.
   */
  if (!forwidget && (glade_property_get_state (property) & GLADE_STATE_CHANGED) == 0)
    return;

  pdef         = glade_property_get_def (property);
  pspec        = glade_property_def_get_pspec (pdef);
  prop_adaptor = glade_property_def_get_adaptor (pdef);
  adaptor      = glade_widget_adaptor_from_pspec (prop_adaptor, pspec);

  g_object_get (adaptor, "catalog", &catalog, NULL);
  glade_project_target_version_for_adaptor (project, adaptor,
                                            &target_major, &target_minor);

  if ((flags & GLADE_VERIFY_VERSIONS) != 0 &&
      !GLADE_PROPERTY_DEF_VERSION_CHECK (pdef, target_major, target_minor))
    {
      GLADE_NOTE (VERIFY, g_print ("VERIFY: Property '%s' of adaptor %s not available in version %d.%d\n",
                                   glade_property_def_id (pdef),
                                   glade_widget_adaptor_get_name (adaptor),
                                   target_major, target_minor));

      if (forwidget)
        {
          tooltip = g_strdup_printf (PROP_VERSION_CONFLICT_MSGFMT,
                                     catalog,
                                     glade_property_def_since_major (pdef),
                                     glade_property_def_since_minor (pdef),
                                     catalog, target_major, target_minor);

          glade_property_set_support_warning (property, FALSE, tooltip);
          g_free (tooltip);
        }
      else
        g_string_append_printf (string,
                                glade_property_def_get_is_packing (pdef) ?
                                PACK_PROP_VERSION_CONFLICT_FMT :
                                  PROP_VERSION_CONFLICT_FMT,
                                  path_name,
                                  glade_property_def_get_name (pdef),
                                  glade_widget_adaptor_get_title (adaptor),
                                  catalog,
                                  glade_property_def_since_major (pdef),
                                  glade_property_def_since_minor (pdef));
    }
  else if ((flags & GLADE_VERIFY_DEPRECATIONS) != 0 &&
           glade_property_def_deprecated (pdef))
    {
      GLADE_NOTE (VERIFY, g_print ("VERIFY: Property '%s' of adaptor %s is deprecated\n",
                                   glade_property_def_id (pdef),
                                   glade_widget_adaptor_get_name (adaptor)));

      if (forwidget)
        glade_property_set_support_warning (property, FALSE, PROP_DEPRECATED_MSG);
      else
        g_string_append_printf (string,
                                PROP_DEPRECATED_FMT,
                                path_name,
                                glade_property_def_get_name (pdef),
                                glade_widget_adaptor_get_title (adaptor));
    }
  else if (forwidget)
    glade_property_set_support_warning (property, FALSE, NULL);

  g_free (catalog);
}

static void
glade_project_verify_properties_internal (GladeWidget     *widget,
                                          const gchar     *path_name,
                                          GString         *string,
                                          gboolean         forwidget,
                                          GladeVerifyFlags flags)
{
  GList *list;
  GladeProperty *property;

  for (list = glade_widget_get_properties (widget); list; list = list->next)
    {
      property = list->data;
      glade_project_verify_property_internal (glade_widget_get_project (widget), 
                                              property, path_name,
                                              string, forwidget, flags);
    }

  /* Sometimes widgets on the clipboard have packing props with no parent */
  if (glade_widget_get_parent (widget))
    {
      for (list = glade_widget_get_packing_properties (widget); list; list = list->next)
        {
          property = list->data;
          glade_project_verify_property_internal (glade_widget_get_project (widget), 
                                                  property, path_name, string, forwidget, flags);
        }
    }
}

static void
glade_project_verify_signal_internal (GladeWidget     *widget,
                                      GladeSignal     *signal,
                                      const gchar     *path_name,
                                      GString         *string,
                                      gboolean         forwidget,
                                      GladeVerifyFlags flags)
{
  GladeSignalDef     *signal_def;
  GladeWidgetAdaptor *adaptor;
  gint                target_major, target_minor;
  gchar              *catalog;
  GladeProject       *project;

  signal_def =
      glade_widget_adaptor_get_signal_def (glade_widget_get_adaptor (widget),
                                           glade_signal_get_name (signal));

  if (!signal_def)
    return;

  adaptor = glade_signal_def_get_adaptor (signal_def);
  project = glade_widget_get_project (widget);

  if (!project)
    return;

  g_object_get (adaptor, "catalog", &catalog, NULL);
  glade_project_target_version_for_adaptor (project, adaptor, 
                                            &target_major, &target_minor);

  if ((flags & GLADE_VERIFY_VERSIONS) != 0 &&
      !GSC_VERSION_CHECK (signal_def, target_major, target_minor))
    {
      GLADE_NOTE (VERIFY, g_print ("VERIFY: Signal '%s' of adaptor %s not avalable in version %d.%d\n",
                                   glade_signal_get_name (signal),
                                   glade_widget_adaptor_get_name (adaptor),
                                   target_major, target_minor));

      if (forwidget)
        {
          gchar *warning;

          warning = g_strdup_printf (SIGNAL_VERSION_CONFLICT_MSGFMT,
                                     catalog,
                                     glade_signal_def_since_major (signal_def),
                                     glade_signal_def_since_minor (signal_def),
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
                                glade_signal_def_since_major (signal_def),
                                glade_signal_def_since_minor (signal_def));
    }
  else if ((flags & GLADE_VERIFY_DEPRECATIONS) != 0 &&
           glade_signal_def_deprecated (signal_def))
    {
      GLADE_NOTE (VERIFY, g_print ("VERIFY: Signal '%s' of adaptor %s is deprecated\n",
                                   glade_signal_get_name (signal),
                                   glade_widget_adaptor_get_name (adaptor)));

      if (forwidget)
        glade_signal_set_support_warning (signal, SIGNAL_DEPRECATED_MSG);
      else
        g_string_append_printf (string,
                                SIGNAL_DEPRECATED_FMT,
                                path_name,
                                glade_signal_get_name (signal),
                                glade_widget_adaptor_get_title (adaptor));
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
    glade_project_verify_property_internal (project, property, NULL, NULL, TRUE,
                                            GLADE_VERIFY_VERSIONS     |
                                            GLADE_VERIFY_DEPRECATIONS |
                                            GLADE_VERIFY_UNRECOGNIZED);
}

void
glade_project_verify_signal (GladeWidget *widget, GladeSignal *signal)
{
  glade_project_verify_signal_internal (widget, signal, NULL, NULL, TRUE,
                                        GLADE_VERIFY_VERSIONS     |
                                        GLADE_VERIFY_DEPRECATIONS |
                                        GLADE_VERIFY_UNRECOGNIZED);
}

static void
glade_project_verify_signals (GladeWidget     *widget,
                              const gchar     *path_name,
                              GString         *string,
                              gboolean         forwidget,
                              GladeVerifyFlags flags)
{
  GladeSignal *signal;
  GList *signals, *list;

  if ((signals = glade_widget_get_signal_list (widget)) != NULL)
    {
      for (list = signals; list; list = list->next)
        {
          signal = list->data;
          glade_project_verify_signal_internal (widget, signal, path_name,
                                                string, forwidget, flags);
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

  glade_project_verify_properties_internal (widget, NULL, NULL, TRUE,
                                            GLADE_VERIFY_VERSIONS     |
                                            GLADE_VERIFY_DEPRECATIONS |
                                            GLADE_VERIFY_UNRECOGNIZED);
  glade_project_verify_signals (widget, NULL, NULL, TRUE,
                                GLADE_VERIFY_VERSIONS     |
                                GLADE_VERIFY_DEPRECATIONS |
                                GLADE_VERIFY_UNRECOGNIZED);

  glade_widget_support_changed (widget);
}

static gboolean
glade_project_verify_dialog (GladeProject *project,
                             GString      *string,
                             gboolean      saving)
{
  GtkWidget *swindow;
  GtkWidget *textview;
  GtkWidget *expander;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  gchar *name;
  gboolean ret;

  swindow = gtk_scrolled_window_new (NULL, NULL);
  textview = gtk_text_view_new ();
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
  expander = gtk_expander_new (_("Details"));

  gtk_text_buffer_get_start_iter (buffer, &iter);
  gtk_text_buffer_insert_markup (buffer, &iter, string->str, -1);
  gtk_widget_set_vexpand (swindow, TRUE);

  gtk_container_add (GTK_CONTAINER (swindow), textview);
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


gboolean
glade_project_verify (GladeProject    *project,
                      gboolean         saving,
                      GladeVerifyFlags flags)
{
  GString *string = g_string_new (NULL);
  GList *list;
  gboolean ret = TRUE;

  GLADE_NOTE (VERIFY, g_print ("VERIFY: glade_project_verify() start\n"));

  if (project->priv->template)
    {
      gint major, minor;
      glade_project_get_target_version (project, "gtk+", &major, &minor);

      if (major == 3 && minor < 10)
        g_string_append_printf (string, _("Object %s is a class template but this is not supported in gtk+ %d.%d"),
                                glade_widget_get_name (project->priv->template),
                                major, minor); 
    }
  
  for (list = project->priv->objects; list; list = list->next)
    {
      GladeWidget *widget = glade_widget_get_from_gobject (list->data);
      
      if ((flags & GLADE_VERIFY_UNRECOGNIZED) != 0 &&
          GLADE_IS_OBJECT_STUB (list->data))
        {
          gchar *type;
          g_object_get (list->data, "object-type", &type, NULL);
          
          g_string_append_printf (string, _("Object %s has unrecognized type %s\n"), 
                                  glade_widget_get_name (widget), type);
          g_free (type);
        }
      else
        {
          gchar *path_name = glade_widget_generate_path_name (widget);

          glade_project_verify_adaptor (project, glade_widget_get_adaptor (widget),
                                        path_name, string, flags, FALSE, NULL);
          glade_project_verify_properties_internal (widget, path_name, string, FALSE, flags);
          glade_project_verify_signals (widget, path_name, string, FALSE, flags);

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

  GLADE_NOTE (VERIFY, g_print ("VERIFY: glade_project_verify() end\n"));

  return ret;
}

static void
glade_project_target_version_for_adaptor (GladeProject       *project,
                                          GladeWidgetAdaptor *adaptor,
                                          gint               *major,
                                          gint               *minor)
{
  gchar *catalog = NULL;

  g_object_get (adaptor, "catalog", &catalog, NULL);
  glade_project_get_target_version (project, catalog, major, minor);
  g_free (catalog);
}

static void
glade_project_verify_adaptor (GladeProject       *project,
                              GladeWidgetAdaptor *adaptor,
                              const gchar        *path_name,
                              GString            *string,
                              GladeVerifyFlags   flags,
                              gboolean           forwidget,
                              GladeSupportMask   *mask)
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
      if ((flags & GLADE_VERIFY_VERSIONS) != 0 &&
          !GWA_VERSION_CHECK (adaptor_iter, target_major, target_minor))
        {
          GLADE_NOTE (VERIFY, g_print ("VERIFY: Adaptor '%s' not available in version %d.%d\n",
                                       glade_widget_adaptor_get_name (adaptor_iter),
                                       target_major, target_minor));

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

      if ((flags & GLADE_VERIFY_DEPRECATIONS) != 0 &&
          (GWA_DEPRECATED (adaptor_iter) ||
           GWA_DEPRECATED_SINCE_CHECK (adaptor_iter, target_major, target_minor)))
        {
          GLADE_NOTE (VERIFY, g_print ("VERIFY: Adaptor '%s' is deprecated\n",
                                       glade_widget_adaptor_get_name (adaptor_iter)));

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
glade_project_verify_widget_adaptor (GladeProject       *project,
                                     GladeWidgetAdaptor *adaptor,
                                     GladeSupportMask   *mask)
{
  GString *string = g_string_new (NULL);
  gchar *ret = NULL;

  glade_project_verify_adaptor (project, adaptor, NULL,
                                string,
                                GLADE_VERIFY_VERSIONS     |
                                GLADE_VERIFY_DEPRECATIONS |
                                GLADE_VERIFY_UNRECOGNIZED,
                                TRUE, mask);

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

  /* Sync displayable info here */
  for (list = project->priv->objects; list; list = list->next)
    {
      widget = glade_widget_get_from_gobject (list->data);

      /* Update the support warnings for widget's properties */
      glade_project_verify_properties (widget);

      /* Update the support warning for widget */
      glade_widget_verify (widget);
    }
}

/**
 * glade_project_get_widget_by_name:
 * @project: a #GladeProject
 * @name: The user visible name of the widget we are looking for
 * 
 * Searches under @ancestor in @project looking for a #GladeWidget named @name.
 * 
 * Returns: (transfer none) (nullable): a pointer to the widget,
 * %NULL if the widget does not exist
 */
GladeWidget *
glade_project_get_widget_by_name (GladeProject *project, const gchar *name)
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
                                   GladeWidget  *gwidget,
                                   const char   *widget_name)
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
                                     GladeWidget  *widget,
                                     const gchar  *name)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

  if (!name || !name[0])
    return FALSE;

  return !glade_name_context_has_name (project->priv->widget_names, name);
}

static void
glade_project_reserve_widget_name (GladeProject *project,
                                   GladeWidget  *gwidget,
                                   const char   *widget_name)
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
 * @widget: the #GladeWidget intended to recieve a new name, or %NULL
 * @base_name: base name of the widget to create
 *
 * Creates a new name for a widget that doesn't collide with any of the names 
 * already in @project. This name will start with @base_name.
 *
 * Note the @widget parameter is ignored and preserved only for historical reasons.
 *
 * Returns: a string containing the new name, %NULL if there is not enough 
 *          memory for this string
 */
gchar *
glade_project_new_widget_name (GladeProject *project,
                               GladeWidget  *widget,
                               const gchar  *base_name)
{
  gchar *name;

  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);
  g_return_val_if_fail (base_name && base_name[0], NULL);

  name = glade_name_context_new_name (project->priv->widget_names, base_name);

  return name;
}

static gboolean
glade_project_get_iter_for_object (GladeProject *project,
                                   GladeWidget  *widget,
                                   GtkTreeIter  *iter)
{
  GtkTreeModel *model = project->priv->model;
  GladeWidget *widget_iter = widget;
  GList *parent_node, *hierarchy = NULL;
  gboolean iter_valid;

  g_return_val_if_fail (widget, FALSE);
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

  if (!(iter_valid = gtk_tree_model_get_iter_first (model, iter)))
    return FALSE;

  /* Generate widget hierarchy list */
  while ((widget_iter = glade_widget_get_parent (widget_iter)))
    hierarchy = g_list_prepend (hierarchy, widget_iter);

  parent_node = hierarchy;

  do
    {
      gtk_tree_model_get (model, iter, 0, &widget_iter, -1);
      
      if (widget_iter == widget)
        {
          /* Object found */
          g_list_free (hierarchy);
          return TRUE;
        }
      else if (parent_node && widget_iter == parent_node->data)
        {
          GtkTreeIter child_iter;

          if (gtk_tree_model_iter_children (model, &child_iter, iter))
            {
              /* Parent found, adn child iter updated, continue looking */
              parent_node = g_list_next (parent_node);
              *iter = child_iter;
              continue;
            }
          else
            {
              /* Parent with no children? */
              g_warning ("Discrepancy found in TreeModel data proxy. "
                         "Can not get children iter for widget %s", 
                         glade_widget_get_name (widget_iter));
              break;
            }
        }
      
      iter_valid = gtk_tree_model_iter_next (model, iter);

    } while (iter_valid);

  g_list_free (hierarchy);
  return FALSE;
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
                               GladeWidget  *widget,
                               const gchar  *name)
{
  gchar *new_name;

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
  glade_project_widget_changed (project, widget);
}

/**
 * glade_project_check_reordered:
 * @project: a #GladeProject
 * @parent: the parent #GladeWidget
 * @old_order: (element-type GObject): the old order to compare with
 *
 */
void
glade_project_check_reordered (GladeProject *project,
                               GladeWidget  *parent,
                               GList        *old_order)
{
  GList *new_order, *l, *ll;

  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (GLADE_IS_WIDGET (parent));
  g_return_if_fail (glade_project_has_object (project,
                                              glade_widget_get_object (parent)));

  new_order = glade_widget_get_children (parent);

  /* Check if the list changed */
  for (l = old_order, ll = new_order; 
       l && ll; 
       l = g_list_next (l), ll = g_list_next (ll))
    {
      if (l->data != ll->data)
        break;
    }

  if (l || ll)
    {
      gint *order = g_new0 (gint, g_list_length (new_order));
      GtkTreeIter iter;
      gint i;

      for (i = 0, l = new_order; l; l = g_list_next (l))
        {
          GObject *obj = l->data;
          GList *node = g_list_find (old_order, obj);

          g_assert (node);

          order[i] = g_list_position (old_order, node);
          i++;
        }

      /* Signal that the rows were reordered */
      glade_project_get_iter_for_object (project, parent, &iter);
      gtk_tree_store_reorder (GTK_TREE_STORE (project->priv->model), &iter, order);

      g_free (order);
    }

  g_list_free (new_order);
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
  GtkTreeIter iter, *parent = NULL;

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
  else if (glade_project_get_iter_for_object (project,
                                              glade_widget_get_parent (gwidget),
                                              &iter))
    {
      parent = &iter;
    }

  priv->objects = g_list_prepend (priv->objects, object);
  gtk_tree_store_insert_with_values (GTK_TREE_STORE (priv->model), NULL, parent, -1,
                                     0, gwidget, -1);

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
  GtkTreeIter  iter;
  GtkTreePath *path;

  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (GLADE_IS_WIDGET (gwidget));
  g_return_if_fail (glade_project_has_gwidget (project, gwidget));

  glade_project_get_iter_for_object (project, gwidget, &iter);
  path = gtk_tree_model_get_path (project->priv->model, &iter);
  gtk_tree_model_row_changed (project->priv->model, path, &iter);
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
  GtkTreeIter iter;

  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (G_IS_OBJECT (object));

  if (GLADE_IS_PLACEHOLDER (object))
    return;

  if ((gwidget = glade_widget_get_from_gobject (object)) == NULL)
    {
      if (g_list_find (project->priv->objects, object))
        {
          project->priv->tree = g_list_remove_all (project->priv->tree, object);
          project->priv->objects = g_list_remove_all (project->priv->objects, object);
          project->priv->selection = g_list_remove_all (project->priv->selection, object);
          g_warning ("Internal data model error, removing object %p %s without a GladeWidget wrapper",
                     object, G_OBJECT_TYPE_NAME (object));
        }
      return;
    }

  if (!glade_project_has_object (project, object))
    return;
  
  /* Recurse and remove deepest children first */
  if ((children = glade_widget_get_children (gwidget)) != NULL)
    {
      for (list = children; list && list->data; list = list->next)
        glade_project_remove_object (project, G_OBJECT (list->data));
      g_list_free (children);
    }

  /* Remove selection and release name from the name context */
  glade_project_selection_remove (project, object, TRUE);
  glade_project_release_widget_name (project, gwidget,
                                     glade_widget_get_name (gwidget));

  g_signal_emit (G_OBJECT (project),
                 glade_project_signals[REMOVE_WIDGET], 0, gwidget);

  /* Update internal data structure (remove from lists) */
  project->priv->tree = g_list_remove (project->priv->tree, object);
  project->priv->objects = g_list_remove (project->priv->objects, object);
  
  if (glade_project_get_iter_for_object (project, gwidget, &iter))
    gtk_tree_store_remove (GTK_TREE_STORE (project->priv->model), &iter);
  else
    g_warning ("Internal data model error, object %p %s not found in tree model",
               object, G_OBJECT_TYPE_NAME (object));
  
  if ((preview_pid = g_object_get_data (G_OBJECT (gwidget), "preview")))
    g_hash_table_remove (project->priv->previews, preview_pid);
  
  /* Unset the project pointer on the GladeWidget */
  glade_widget_set_project (gwidget, NULL);
  glade_widget_set_in_project (gwidget, FALSE);
  g_object_unref (gwidget);
}

/*******************************************************************
 *                          Other API                              *
 *******************************************************************/
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

      g_object_notify_by_pspec (G_OBJECT (project), glade_project_props[PROP_MODIFIED]);
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

      g_object_notify_by_pspec (G_OBJECT (project), glade_project_props[PROP_POINTER_MODE]);
    }
}

GladePointerMode
glade_project_get_pointer_mode (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

  return project->priv->pointer_mode;
}

void
glade_project_set_template (GladeProject *project, GladeWidget *widget)
{
  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (widget == NULL || GLADE_IS_WIDGET (widget));

  if (widget)
    {
      GObject *object = glade_widget_get_object (widget);

      g_return_if_fail (GTK_IS_WIDGET (object));
      g_return_if_fail (glade_widget_get_parent (widget) == NULL);
      g_return_if_fail (glade_widget_get_project (widget) == project);
    }

  /* Let's not add any strong reference here, we already own the widget */
  if (project->priv->template != widget)
    {
      if (project->priv->template)
        glade_widget_set_is_composite (project->priv->template, FALSE);

      project->priv->template = widget;

      if (project->priv->template)
        glade_widget_set_is_composite (project->priv->template, TRUE);

      glade_project_verify_project_for_ui (project);

      g_object_notify_by_pspec (G_OBJECT (project), glade_project_props[PROP_TEMPLATE]);
    }
}

/**
 * glade_project_get_template:
 * @project: a #GladeProject
 * 
 * Returns: (transfer none): a #GladeWidget
 */
GladeWidget *
glade_project_get_template (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), FALSE);

  return project->priv->template;
}

/**
 * glade_project_set_add_item:
 * @project: a #GladeProject
 * @adaptor: (transfer full): a #GladeWidgetAdaptor
 */
void
glade_project_set_add_item (GladeProject *project, GladeWidgetAdaptor *adaptor)
{
  GladeProjectPrivate *priv;

  g_return_if_fail (GLADE_IS_PROJECT (project));

  priv = project->priv;

  if (priv->add_item != adaptor)
    {
      priv->add_item = adaptor;

      g_object_notify_by_pspec (G_OBJECT (project), glade_project_props[PROP_ADD_ITEM]);
    }
}

/**
 * glade_project_get_add_item:
 * @project: a #GladeProject
 * 
 * Returns: (transfer none): a #GladeWidgetAdaptor
 */
GladeWidgetAdaptor *
glade_project_get_add_item (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  return project->priv->add_item;
}

void
glade_project_set_target_version (GladeProject *project,
                                  const gchar  *catalog,
                                  gint          major,
                                  gint          minor)
{
  g_return_if_fail (GLADE_IS_PROJECT (project));
  g_return_if_fail (catalog && catalog[0]);
  g_return_if_fail (major >= 0);
  g_return_if_fail (minor >= 0);

  g_hash_table_insert (project->priv->target_versions_major,
                       g_strdup (catalog), GINT_TO_POINTER ((int) major));
  g_hash_table_insert (project->priv->target_versions_minor,
                       g_strdup (catalog), GINT_TO_POINTER ((int) minor));

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
      g_object_notify_by_pspec (G_OBJECT (project), glade_project_props[PROP_READ_ONLY]);
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
                                  const gchar  *catalog,
                                  gint         *major,
                                  gint         *minor)
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
      g_object_notify_by_pspec (G_OBJECT (project), glade_project_props[PROP_HAS_SELECTION]);
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
                                GObject      *object,
                                gboolean      emit_signal)
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
                             GObject      *object,
                             gboolean      emit_signal)
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
                             GObject      *object,
                             gboolean      emit_signal)
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
 * Returns: (transfer none) (element-type GtkWidget): a #GList containing
 * the #GtkWidget items currently selected in @project
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
 * Returns: (transfer full) (element-type utf8): a #GList of allocated strings
 * which are the names of the required catalogs for this project
 */
GList *
glade_project_required_libs (GladeProject *project)
{
  GList *l, *required = NULL;

  /* Assume GTK+ catalog here */
  required = g_list_prepend (required, _glade_catalog_get_catalog ("gtk+"));
    
  for (l = project->priv->objects; l; l = g_list_next (l))
    {
      GladeWidget *gwidget = glade_widget_get_from_gobject (l->data);
      GladeCatalog *catalog;
      gchar *name = NULL;

      g_assert (gwidget);

      g_object_get (glade_widget_get_adaptor (gwidget), "catalog", &name, NULL);

      if ((catalog = _glade_catalog_get_catalog (name)))
        {
          if (!g_list_find (required, catalog))
            required = g_list_prepend (required, catalog);
        }

      g_free (name);
    }

  /* Sort by dependency */
  required = _glade_catalog_tsort (required);

  /* Convert list of GladeCatalog to list of names */
  for (l = required; l; l = g_list_next (l))
    l->data = g_strdup (glade_catalog_get_name (l->data));
  
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
 * glade_project_redo:
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
 * Returns: (transfer none): the #GladeCommand
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
 * Returns: (transfer none): the #GladeCommand
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
 * Returns: (transfer full): A newly created menu
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
 * Returns: (transfer full): A newly created menu
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
 * Project resource strings are always relative, this function tranforms a
 * path relative to project to a full path.
 *
 * Returns: A newly allocated string holding the
 *          full path to the resource.
 */
gchar *
glade_project_resource_fullpath (GladeProject *project, const gchar *resource)
{
  gchar *fullpath, *project_dir = NULL;

  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  if (project->priv->path == NULL)
    project_dir = g_get_current_dir ();
  else
    project_dir = g_path_get_dirname (project->priv->path);

  if (project->priv->resource_path)
    {
      if (g_path_is_absolute (project->priv->resource_path))
        fullpath =
            g_build_filename (project->priv->resource_path, resource, NULL);
      else
        fullpath =
            g_build_filename (project_dir, project->priv->resource_path,
                              resource, NULL);
    }
  else
    fullpath = g_build_filename (project_dir, resource, NULL);

  g_free (project_dir);
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
 * glade_project_get_objects:
 * @project: a GladeProject
 *
 * Returns: (transfer none) (element-type GObject): List of all objects in this project
 */
const GList *
glade_project_get_objects (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  return project->priv->objects;
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
 * Returns: (transfer none) (element-type GtkWidget): a #GList containing
 * the #GtkWidget toplevel items in @project
 */
GList *
glade_project_toplevels (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  return project->priv->tree;
}

/**
 * glade_project_set_translation_domain:
 * @project: a #GladeProject
 * @domain: the translation domain
 *
 * Set the project translation domain.
 */
void
glade_project_set_translation_domain (GladeProject *project, const gchar *domain)
{
  GladeProjectPrivate *priv;

  g_return_if_fail (GLADE_IS_PROJECT (project));

  priv = project->priv;

  if (g_strcmp0 (priv->translation_domain, domain))
    {
      g_free (priv->translation_domain);
      priv->translation_domain = g_strdup (domain);

      g_object_notify_by_pspec (G_OBJECT (project),
                                glade_project_props[PROP_TRANSLATION_DOMAIN]);
    }
}

/**
 * glade_project_get_translation_domain:
 * @project: a #GladeProject
 *
 * Returns: the translation domain
 */
const gchar *
glade_project_get_translation_domain (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  return project->priv->translation_domain;
}

static void 
glade_project_css_provider_remove_forall (GtkWidget *widget, gpointer data)
{
  gtk_style_context_remove_provider (gtk_widget_get_style_context (widget),
                                     GTK_STYLE_PROVIDER (data));
  
  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget), glade_project_css_provider_remove_forall, data);
}

static inline void
glade_project_css_provider_refresh (GladeProject *project, gboolean remove)
{
  GladeProjectPrivate *priv = project->priv;
  GtkCssProvider *provider = priv->css_provider;
  const GList *l;

  for (l = priv->tree; l; l = g_list_next (l))
    {
      GObject *object = l->data;

      if (!GTK_IS_WIDGET (object) || GLADE_IS_OBJECT_STUB (object))
        continue;

      if (remove)
        glade_project_css_provider_remove_forall (GTK_WIDGET (object), provider);
      else
        glade_project_set_css_provider_forall (GTK_WIDGET (object), provider);
    }
}

static void 
on_css_monitor_changed (GFileMonitor     *monitor,
                        GFile            *file,
                        GFile            *other_file,
                        GFileMonitorEvent event_type,
                        GladeProject     *project)
{
  GError *error = NULL;

  gtk_css_provider_load_from_file (project->priv->css_provider, file, &error);

  if (error)
    {
      g_message ("CSS parsing failed: %s", error->message);
      g_error_free (error);
    }
}

/**
 * glade_project_set_css_provider_path:
 * @project: a #GladeProject
 * @path: a CSS file path
 *
 * Set the custom CSS provider path to use in @project
 */
void
glade_project_set_css_provider_path (GladeProject *project, const gchar *path)
{
  GladeProjectPrivate *priv;

  g_return_if_fail (GLADE_IS_PROJECT (project));
  priv = project->priv;

  if (g_strcmp0 (priv->css_provider_path, path) != 0)
    {
      g_free (priv->css_provider_path);
      priv->css_provider_path = g_strdup (path);

      g_clear_object (&priv->css_monitor);
      
      if (priv->css_provider)
        {
          glade_project_css_provider_refresh (project, TRUE);
          g_clear_object (&priv->css_provider);
        }

      if (priv->css_provider_path &&
          g_file_test (priv->css_provider_path, G_FILE_TEST_IS_REGULAR))
        {
          GFile *file = g_file_new_for_path (priv->css_provider_path);

          priv->css_provider = GTK_CSS_PROVIDER (gtk_css_provider_new ());
          g_object_ref_sink (priv->css_provider);
          gtk_css_provider_load_from_file (priv->css_provider, file, NULL);

          g_clear_object (&priv->css_monitor);
          priv->css_monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);
          g_object_ref_sink (priv->css_monitor);
          g_signal_connect_object (priv->css_monitor, "changed",
                                   G_CALLBACK (on_css_monitor_changed), project, 0);

          glade_project_css_provider_refresh (project, FALSE);
          g_object_unref (file);
        }

      g_object_notify_by_pspec (G_OBJECT (project), glade_project_props[PROP_CSS_PROVIDER_PATH]);
    }
}

/**
 * glade_project_get_css_provider_path:
 * @project: a #GladeProject
 *
 * Returns: the CSS path of the custom provider used for @project 
 */
const gchar *
glade_project_get_css_provider_path (GladeProject *project)
{
  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  return project->priv->css_provider_path;
}

/*************************************************
 *                Command Central                *
 *************************************************/
static gboolean
widget_contains_unknown_type (GladeWidget *widget)
{
  GList *list, *l;
  GObject *object;
  gboolean has_unknown = FALSE;

  object = glade_widget_get_object (widget);

  if (GLADE_IS_OBJECT_STUB (object))
    return TRUE;

  list = glade_widget_get_children (widget);
  for (l = list; l && has_unknown == FALSE; l = l->next)
    {
      GladeWidget *child = glade_widget_get_from_gobject (l->data);

      has_unknown = widget_contains_unknown_type (child);
    }
  g_list_free (list);

  return has_unknown;
}

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
      GladeWidget *widget = glade_widget_get_from_gobject (list->data);

      if (widget_contains_unknown_type (widget))
        has_unknown = TRUE;
      else
        widgets = g_list_prepend (widgets, glade_widget_dup (widget, FALSE));
    }

  if (has_unknown)
    glade_util_ui_message (glade_app_get_window (),
                           GLADE_UI_INFO, NULL, _("Unable to copy unrecognized widget type."));

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
      GladeWidget *widget = glade_widget_get_from_gobject (list->data);

      if (widget_contains_unknown_type (widget))
        has_unknown = TRUE;
      else
        widgets = g_list_prepend (widgets, widget);
    }
  
  if (failed == FALSE && widgets != NULL)
    glade_command_cut (widgets);
  else if (has_unknown)
    glade_util_ui_message (glade_app_get_window (),
                           GLADE_UI_INFO, NULL, _("Unable to cut unrecognized widget type"));
  else if (widgets == NULL)
    glade_util_ui_message (glade_app_get_window (),
                           GLADE_UI_INFO, NULL, _("No widget selected."));

  if (widgets)
    g_list_free (widgets);
}

void
glade_project_command_paste (GladeProject     *project,
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

/* Private */

void
_glade_project_emit_add_signal_handler (GladeWidget       *widget,
                                        const GladeSignal *signal)
{
  GladeProject *project = glade_widget_get_project (widget);

  if (project)
    g_signal_emit (project, glade_project_signals[ADD_SIGNAL_HANDLER], 0,
                   widget, signal);
}

void
_glade_project_emit_remove_signal_handler (GladeWidget       *widget,
                                           const GladeSignal *signal)
{
  GladeProject *project = glade_widget_get_project (widget);

  if (project)
    g_signal_emit (project, glade_project_signals[REMOVE_SIGNAL_HANDLER], 0,
                   widget, signal);

}

void
_glade_project_emit_change_signal_handler (GladeWidget       *widget,
                                           const GladeSignal *old_signal,
                                           const GladeSignal *new_signal)
{
  GladeProject *project = glade_widget_get_project (widget);

  if (project)
    g_signal_emit (project, glade_project_signals[CHANGE_SIGNAL_HANDLER], 0,
                   widget, old_signal, new_signal);
}

void
_glade_project_emit_activate_signal_handler (GladeWidget       *widget,
                                             const GladeSignal *signal)
{
  GladeProject *project = glade_widget_get_project (widget);

  if (project)
    g_signal_emit (project, glade_project_signals[ACTIVATE_SIGNAL_HANDLER], 0,
                   widget, signal);

}

