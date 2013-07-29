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

#include "glade-preferences.h"
#include <gladeui/glade-catalog.h>
#include <gladeui/glade-utils.h>

#define CONFIG_GROUP "Preferences"
#define CONFIG_KEY_CATALOG_PATHS "catalog-paths"

#define CONFIG_GROUP_LOAD_SAVE      "Load and Save"
#define CONFIG_KEY_BACKUP           "backup"
#define CONFIG_KEY_AUTOSAVE         "autosave"
#define CONFIG_KEY_AUTOSAVE_SECONDS "autosave-seconds"

#define CONFIG_GROUP_SAVE_WARNINGS  "Save Warnings"
#define CONFIG_KEY_VERSIONING       "versioning"
#define CONFIG_KEY_DEPRECATIONS     "deprecations"
#define CONFIG_KEY_UNRECOGNIZED     "unrecognized"

/* Default preference values */
#define DEFAULT_BACKUP              TRUE
#define DEFAULT_AUTOSAVE            TRUE
#define DEFAULT_AUTOSAVE_SECONDS    5
#define DEFAULT_WARN_VERSIONS       TRUE
#define DEFAULT_WARN_DEPRECATIONS   FALSE
#define DEFAULT_WARN_UNRECOGNIZED   TRUE

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
};


G_DEFINE_TYPE_WITH_PRIVATE (GladePreferences, glade_preferences, GTK_TYPE_DIALOG);

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
on_preferences_filechooserdialog_response (GtkDialog *dialog,
                                           gint response_id,
                                           GladePreferences *preferences)
{
  GladePreferencesPrivate *priv = preferences->priv;

  gtk_widget_hide (GTK_WIDGET (dialog));

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      gchar *directory = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      gchar *canonical, *display;

      canonical = glade_util_canonical_path (directory);
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

      g_free (directory);
      g_free (canonical);
      g_free (display);
    }
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
  gtk_widget_class_bind_template_callback (widget_class, on_preferences_filechooserdialog_response);
  gtk_widget_class_bind_template_callback (widget_class, catalog_selection_changed);
  gtk_widget_class_bind_template_callback (widget_class, remove_catalog_clicked);
}

/********************************************************
 *                         API                          *
 ********************************************************/
GtkWidget *
glade_preferences_new (void)
{
  return g_object_new (GLADE_TYPE_PREFERENCES, NULL);
}

void
glade_preferences_save (GladePreferences *prefs,
			GKeyFile         *config)
{
  GtkTreeModel *model = prefs->priv->catalog_path_store;
  GString *string = g_string_new ("");
  GtkTreeIter iter;
  gboolean valid;

  valid = gtk_tree_model_get_iter_first (model, &iter);
  while (valid)
    {
      gchar *path;

      gtk_tree_model_get (model, &iter, COLUMN_CANONICAL_PATH, &path, -1);

      valid = gtk_tree_model_iter_next (model, &iter);
      
      g_string_append (string, path);
      if (valid) g_string_append (string, ":");
      
      g_free (path);
    }
  
  g_key_file_set_string (config, CONFIG_GROUP, CONFIG_KEY_CATALOG_PATHS, string->str);

  /* Load and save */
  g_key_file_set_boolean (config, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_BACKUP,
			  glade_preferences_backup (prefs));
  g_key_file_set_boolean (config, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_AUTOSAVE,
			  glade_preferences_autosave (prefs));
  g_key_file_set_integer (config, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_AUTOSAVE_SECONDS,
			  glade_preferences_autosave_seconds (prefs));

  /* Warnings */
  g_key_file_set_boolean (config, CONFIG_GROUP_SAVE_WARNINGS, CONFIG_KEY_VERSIONING,
			  glade_preferences_warn_versioning (prefs));
  g_key_file_set_boolean (config, CONFIG_GROUP_SAVE_WARNINGS, CONFIG_KEY_DEPRECATIONS,
			  glade_preferences_warn_deprecations (prefs));
  g_key_file_set_boolean (config, CONFIG_GROUP_SAVE_WARNINGS, CONFIG_KEY_UNRECOGNIZED,
			  glade_preferences_warn_unrecognized (prefs));

  g_string_free (string, TRUE);
}

void
glade_preferences_load (GladePreferences *prefs,
			GKeyFile         *config)
{
  gboolean backups = DEFAULT_BACKUP;
  gboolean autosave = DEFAULT_AUTOSAVE;
  gboolean warn_versioning = DEFAULT_WARN_VERSIONS;
  gboolean warn_deprecations = DEFAULT_WARN_DEPRECATIONS;
  gboolean warn_unrecognized = DEFAULT_WARN_UNRECOGNIZED;
  gint autosave_seconds = DEFAULT_AUTOSAVE_SECONDS;
  gchar *string;

  string = g_key_file_get_string (config, CONFIG_GROUP, CONFIG_KEY_CATALOG_PATHS, NULL);

  if (string && g_strcmp0 (string, ""))
    {
      gchar **paths, **path;

      gtk_list_store_clear (GTK_LIST_STORE (prefs->priv->catalog_path_store));
      glade_catalog_remove_path (NULL);

      paths = g_strsplit (string, ":", -1);

      path = paths;
      do
        {
	  GtkTreeIter iter;
	  gchar *canonical, *display;

	  canonical = glade_util_canonical_path (*path);
	  display   = glade_utils_replace_home_dir_with_tilde (canonical);

          glade_catalog_add_path (canonical);

	  gtk_list_store_append (GTK_LIST_STORE (prefs->priv->catalog_path_store), &iter);
	  gtk_list_store_set (GTK_LIST_STORE (prefs->priv->catalog_path_store), &iter,
			      COLUMN_PATH, display,
			      COLUMN_CANONICAL_PATH, canonical,
			      -1);
	  g_free (display);
	  g_free (canonical);

        } while (*++path);

      g_strfreev (paths);
    }

  /* Load and save */
  if (g_key_file_has_key (config, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_BACKUP, NULL))
    backups = g_key_file_get_boolean (config, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_BACKUP, NULL);

  if (g_key_file_has_key (config, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_AUTOSAVE, NULL))
    autosave = g_key_file_get_boolean (config, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_AUTOSAVE, NULL);

  if (g_key_file_has_key (config, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_AUTOSAVE_SECONDS, NULL))
    autosave_seconds = g_key_file_get_integer (config, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_AUTOSAVE_SECONDS, NULL);

  /* Warnings */
  if (g_key_file_has_key (config, CONFIG_GROUP_SAVE_WARNINGS, CONFIG_KEY_VERSIONING, NULL))
    warn_versioning = g_key_file_get_boolean (config, CONFIG_GROUP_SAVE_WARNINGS, CONFIG_KEY_VERSIONING, NULL);

  if (g_key_file_has_key (config, CONFIG_GROUP_SAVE_WARNINGS, CONFIG_KEY_DEPRECATIONS, NULL))
    warn_deprecations = g_key_file_get_boolean (config, CONFIG_GROUP_SAVE_WARNINGS, CONFIG_KEY_DEPRECATIONS, NULL);

  if (g_key_file_has_key (config, CONFIG_GROUP_SAVE_WARNINGS, CONFIG_KEY_UNRECOGNIZED, NULL))
    warn_unrecognized = g_key_file_get_boolean (config, CONFIG_GROUP_SAVE_WARNINGS, CONFIG_KEY_UNRECOGNIZED, NULL);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefs->priv->create_backups_toggle), backups);
  gtk_widget_set_sensitive (prefs->priv->autosave_spin, autosave);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefs->priv->autosave_toggle), autosave);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (prefs->priv->autosave_spin), autosave_seconds);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefs->priv->versioning_toggle), warn_versioning);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefs->priv->deprecations_toggle), warn_deprecations);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefs->priv->unrecognized_toggle), warn_unrecognized);

  g_free (string);
}

gboolean
glade_preferences_backup (GladePreferences *prefs)
{
  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (prefs->priv->create_backups_toggle));
}

gboolean
glade_preferences_autosave (GladePreferences *prefs)
{
  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (prefs->priv->autosave_toggle));
}

gint
glade_preferences_autosave_seconds (GladePreferences *prefs)
{
  return (gint)gtk_spin_button_get_value (GTK_SPIN_BUTTON (prefs->priv->autosave_spin));
}

gboolean
glade_preferences_warn_versioning (GladePreferences *prefs)
{
  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (prefs->priv->versioning_toggle));
}

gboolean
glade_preferences_warn_deprecations (GladePreferences *prefs)
{
  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (prefs->priv->deprecations_toggle));
}

gboolean
glade_preferences_warn_unrecognized (GladePreferences *prefs)
{
  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (prefs->priv->unrecognized_toggle));
}
