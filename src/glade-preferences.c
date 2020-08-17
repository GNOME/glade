/*
 * Copyright (C) 2012 Juan Pablo Ugarte.
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
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#include <glib/gi18n.h>
#include "glade-preferences.h"
#include <gladeui/glade-catalog.h>
#include <gladeui/glade-utils.h>

#define CONFIG_GROUP "Preferences"
enum {
  COLUMN_PATH = 0,
  COLUMN_CANONICAL_PATH
};

struct _GladePreferencesPrivate
{
  GtkTreeModel *catalog_path_store;
  GtkTreeSelection *selection;
  GtkWidget *remove_catalog_button;

  GtkWidget *create_backups_toggle;
  GtkWidget *autosave_toggle;
  GtkWidget *autosave_spin;

  GtkWidget *versioning_toggle;
  GtkWidget *deprecations_toggle;
  GtkWidget *unrecognized_toggle;

  GladeSettings *settings;
};

enum
{
  PROP_SETTINGS = 1,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (GladePreferences, glade_preferences, GTK_TYPE_DIALOG)

/********************************************************
 *                       CALLBACKS                      *
 ********************************************************/
static void
autosave_toggled (GtkToggleButton  *button,
                  GladePreferences *prefs)
{
  gtk_widget_set_sensitive (prefs->priv->autosave_spin,
                            gtk_toggle_button_get_active (button));
}

static gboolean 
find_row (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
  gchar **directory = data;
  gchar *string;
  
  gtk_tree_model_get (model, iter, COLUMN_CANONICAL_PATH, &string, -1);

  if (g_strcmp0 (string, *directory) == 0)
    {
      g_free (*directory);
      *directory = NULL;
      return TRUE;
    }
  
  return FALSE;
}

static void
remove_catalog_clicked (GtkButton        *button,
                        GladePreferences *preferences)
{
  GladePreferencesPrivate *priv = preferences->priv;
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected (priv->selection, NULL, &iter))
    {
      gchar *path = NULL;

      gtk_tree_model_get (priv->catalog_path_store, &iter,
                          COLUMN_CANONICAL_PATH, &path,
                          -1);

      if (path)
        {
          glade_catalog_remove_path (path);
          g_free (path);
        }

      gtk_list_store_remove (GTK_LIST_STORE (priv->catalog_path_store), &iter);
    }
}

static void
catalog_selection_changed (GtkTreeSelection *selection,
                           GladePreferences *preferences)
{
  gboolean selected = gtk_tree_selection_get_selected (selection, NULL, NULL);

  /* Make the button sensitive if anything is selected */
  gtk_widget_set_sensitive (preferences->priv->remove_catalog_button, selected);
}

static gboolean
glade_preferences_transform_to (GBinding     *binding,
                                const GValue *from_value,
                                GValue       *to_value,
                                gpointer      user_data)
{
  GladeVerifyFlags flag = (GladeVerifyFlags) user_data;
  g_value_set_boolean (to_value, g_value_get_flags (from_value) & flag);
  return TRUE;
}

static gboolean
glade_preferences_transform_from (GBinding     *binding,
                                  const GValue *from_value,
                                  GValue       *to_value,
                                  gpointer      user_data)
{
  GladeVerifyFlags flag = (GladeVerifyFlags) user_data;
  GladeVerifyFlags previous_flags = glade_settings_get_verify_flags (GLADE_SETTINGS (g_binding_get_source (binding)));

  if (g_value_get_boolean (from_value))
    g_value_set_flags (to_value, previous_flags | flag);
  else
    g_value_set_flags (to_value, previous_flags & ~flag);

  return TRUE;
}

static void
glade_preferences_set_settings (GladePreferences *self,
                                GladeSettings    *settings)
{
  const GList *paths, *l;

  self->priv->settings = settings;
  g_object_bind_property (settings, "backup", self->priv->create_backups_toggle, "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  g_object_bind_property (settings, "autosave", self->priv->autosave_spin, "sensitive", G_BINDING_SYNC_CREATE);
  g_object_bind_property (settings, "autosave", self->priv->autosave_toggle, "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  g_object_bind_property (settings, "autosave-seconds", self->priv->autosave_spin, "value", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  g_object_bind_property_full (settings, "verify-flags", self->priv->versioning_toggle, "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
                               glade_preferences_transform_to, glade_preferences_transform_from, (void *)GLADE_VERIFY_VERSIONS, NULL);
  g_object_bind_property_full (settings, "verify-flags", self->priv->deprecations_toggle, "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
                               glade_preferences_transform_to, glade_preferences_transform_from, (void *)GLADE_VERIFY_DEPRECATIONS, NULL);
  g_object_bind_property_full (settings, "verify-flags", self->priv->unrecognized_toggle, "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
                               glade_preferences_transform_to, glade_preferences_transform_from, (void *)GLADE_VERIFY_UNRECOGNIZED, NULL);

  paths = glade_catalog_get_extra_paths ();
  gtk_list_store_clear (GTK_LIST_STORE (self->priv->catalog_path_store));
  for (l = paths; l != NULL; l = l->next)
    {
      const gchar *path = (const gchar *)(l->data);
      gchar *display = glade_utils_replace_home_dir_with_tilde (path);
      GtkTreeIter iter;

      gtk_list_store_append (GTK_LIST_STORE (self->priv->catalog_path_store), &iter);
      gtk_list_store_set (GTK_LIST_STORE (self->priv->catalog_path_store), &iter,
                          COLUMN_PATH, display,
                          COLUMN_CANONICAL_PATH, path,
                          -1);

      g_free (display);
    }
}

static void
glade_preferences_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GladePreferences *self = GLADE_PREFERENCES (object);

  switch (property_id)
    {
    case PROP_SETTINGS:
      glade_preferences_set_settings (self, GLADE_SETTINGS (g_value_get_object (value)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
glade_preferences_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GladePreferences *self = GLADE_PREFERENCES (object);

  switch (property_id)
    {
    case PROP_SETTINGS:
      g_value_set_object (value, self->priv->settings);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
on_add_catalog_button_clicked (GtkButton *button, GladePreferences *preferences)
{
  GladePreferencesPrivate *priv = preferences->priv;
  GtkWidget *dialog;

  dialog = gtk_file_chooser_dialog_new ("Select a catalog search path",
                                        GTK_WINDOW (preferences),
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                        _("_Cancel"),
                                        GTK_RESPONSE_CANCEL,
                                        _("_Open"),
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);

  if ((gtk_dialog_run (GTK_DIALOG (dialog))) == GTK_RESPONSE_ACCEPT)
    {
      g_autofree gchar *path = NULL, *canonical = NULL, *display = NULL;

      path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      canonical = glade_util_canonical_path (path);
      display   = glade_utils_replace_home_dir_with_tilde (canonical);

      gtk_tree_model_foreach (priv->catalog_path_store, find_row, &canonical);

      if (canonical)
        {
          GtkTreeIter iter;

          glade_catalog_add_path (canonical);

          gtk_list_store_append (GTK_LIST_STORE (priv->catalog_path_store), &iter);
          gtk_list_store_set (GTK_LIST_STORE (priv->catalog_path_store), &iter,
                              COLUMN_PATH, display,
                              COLUMN_CANONICAL_PATH, canonical,
                              -1);
        }
    }

  gtk_widget_destroy (dialog);
}

/********************************************************
 *                  Class/Instance Init                 *
 ********************************************************/
static void
glade_preferences_init (GladePreferences *preferences)
{
  preferences->priv = glade_preferences_get_instance_private (preferences);

  gtk_widget_init_template (GTK_WIDGET (preferences));
}

static void
glade_preferences_class_init (GladePreferencesClass *klass)
{
  GtkWidgetClass *widget_class;
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = glade_preferences_set_property;
  object_class->get_property = glade_preferences_get_property;

  obj_properties[PROP_SETTINGS] =
    g_param_spec_object ("settings",
                         "Settings",
                         "Settings object.",
                         GLADE_TYPE_SETTINGS,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     obj_properties);

  widget_class  = GTK_WIDGET_CLASS (klass);

  /* Setup the template GtkBuilder xml for this class
   */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/glade/glade-preferences.glade");

  /* Define the relationship of the private entry and the entry defined in the xml
   */
  gtk_widget_class_bind_template_child_private (widget_class, GladePreferences, catalog_path_store);
  gtk_widget_class_bind_template_child_private (widget_class, GladePreferences, remove_catalog_button);
  gtk_widget_class_bind_template_child_private (widget_class, GladePreferences, selection);
  gtk_widget_class_bind_template_child_private (widget_class, GladePreferences, create_backups_toggle);
  gtk_widget_class_bind_template_child_private (widget_class, GladePreferences, autosave_toggle);
  gtk_widget_class_bind_template_child_private (widget_class, GladePreferences, autosave_spin);
  gtk_widget_class_bind_template_child_private (widget_class, GladePreferences, versioning_toggle);
  gtk_widget_class_bind_template_child_private (widget_class, GladePreferences, deprecations_toggle);
  gtk_widget_class_bind_template_child_private (widget_class, GladePreferences, unrecognized_toggle);

  /* Declare the callback ports that this widget class exposes, to bind with <signal>
   * connections defined in the GtkBuilder xml
   */
  gtk_widget_class_bind_template_callback (widget_class, autosave_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_add_catalog_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, catalog_selection_changed);
  gtk_widget_class_bind_template_callback (widget_class, remove_catalog_clicked);
}

/********************************************************
 *                         API                          *
 ********************************************************/
GtkWidget *
glade_preferences_new (GladeSettings *settings)
{
  return g_object_new (GLADE_TYPE_PREFERENCES,
                       "use-header-bar", TRUE,
                       "settings", settings,
                       NULL);
}
