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

#define CONFIG_GROUP "Preferences"
#define CONFIG_KEY_CATALOG_PATHS "catalog-paths"

#define CONFIG_GROUP_LOAD_SAVE      "Load and Save"
#define CONFIG_KEY_BACKUP           "backup"
#define CONFIG_KEY_AUTOSAVE         "autosave"
#define CONFIG_KEY_AUTOSAVE_SECONDS "autosave-seconds"

struct _GladePreferencesPrivate
{
  GtkComboBoxText *catalog_path_combo;

  GtkWidget *create_backups_toggle;
  GtkWidget *autosave_toggle;
  GtkWidget *autosave_spin;
};


G_DEFINE_TYPE (GladePreferences, glade_preferences, GTK_TYPE_DIALOG);

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
  
  gtk_tree_model_get (model, iter, 1, &string, -1);

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
                                           GtkComboBoxText *combo)
{
  gtk_widget_hide (GTK_WIDGET (dialog));

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      gchar *directory = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
      GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
      
      gtk_tree_model_foreach (model, find_row, &directory);

      if (directory)
        {
          glade_catalog_add_path (directory);
          gtk_combo_box_text_append (combo, directory, directory);
          gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
          g_free (directory);
        }
    }
}

static void
on_catalog_path_remove_button_clicked (GtkButton *button, GtkComboBoxText *combo)
{
  gint active = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

  if (active >= 0)
    {
      gchar *directory = gtk_combo_box_text_get_active_text (combo);
      glade_catalog_remove_path (directory);
      g_free (directory);

      gtk_combo_box_text_remove (combo, active);

      if (--active < 0) active = 0;
      gtk_combo_box_set_active (GTK_COMBO_BOX (combo), active);
    }
}

/********************************************************
 *                  Class/Instance Init                 *
 ********************************************************/
static void
combo_box_text_init_cell (GtkCellLayout *cell)
{
  GList *l, *cels = gtk_cell_layout_get_cells (cell);

  for (l = cels; l; l = g_list_next (l))
    {
      g_object_set (l->data,
                    "ellipsize", PANGO_ELLIPSIZE_MIDDLE,
                    "ellipsize-set", TRUE,
                    "max-width-chars", 128,
                    "width-chars", 32,
                    NULL);
    }

  g_list_free (cels);
}

static void
glade_preferences_init (GladePreferences *preferences)
{
  preferences->priv = G_TYPE_INSTANCE_GET_PRIVATE (preferences,
						   GLADE_TYPE_PREFERENCES,
						   GladePreferencesPrivate);

  gtk_widget_init_template (GTK_WIDGET (preferences));

  combo_box_text_init_cell (GTK_CELL_LAYOUT (preferences->priv->catalog_path_combo));
}

static void
glade_preferences_class_init (GladePreferencesClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  widget_class  = GTK_WIDGET_CLASS (klass);

  /* Setup the template GtkBuilder xml for this class
   */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/glade/glade-preferences.glade");

  /* Define the relationship of the private entry and the entry defined in the xml
   */
  gtk_widget_class_bind_child (widget_class, GladePreferencesPrivate, catalog_path_combo);
  gtk_widget_class_bind_child (widget_class, GladePreferencesPrivate, create_backups_toggle);
  gtk_widget_class_bind_child (widget_class, GladePreferencesPrivate, autosave_toggle);
  gtk_widget_class_bind_child (widget_class, GladePreferencesPrivate, autosave_spin);

  /* Declare the callback ports that this widget class exposes, to bind with <signal>
   * connections defined in the GtkBuilder xml
   */
  gtk_widget_class_bind_callback (widget_class, autosave_toggled);
  gtk_widget_class_bind_callback (widget_class, on_preferences_filechooserdialog_response);
  gtk_widget_class_bind_callback (widget_class, on_catalog_path_remove_button_clicked);

  g_type_class_add_private (gobject_class, sizeof (GladePreferencesPrivate));
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
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (prefs->priv->catalog_path_combo));
  gint column = gtk_combo_box_get_entry_text_column (GTK_COMBO_BOX (prefs->priv->catalog_path_combo));
  GString *string = g_string_new ("");
  GtkTreeIter iter;
  gboolean valid;

  valid = gtk_tree_model_get_iter_first (model, &iter);
  while (valid)
    {
      gchar *path;

      gtk_tree_model_get (model, &iter, column, &path, -1);
      valid = gtk_tree_model_iter_next (model, &iter);
      
      g_string_append (string, path);
      if (valid) g_string_append (string, ":");
      
      g_free (path);
    }
  
  g_key_file_set_string (config, CONFIG_GROUP, CONFIG_KEY_CATALOG_PATHS, string->str);

  g_key_file_set_boolean (config, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_BACKUP,
			  glade_preferences_backup (prefs));
  g_key_file_set_boolean (config, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_AUTOSAVE,
			  glade_preferences_autosave (prefs));
  g_key_file_set_integer (config, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_AUTOSAVE_SECONDS,
			  glade_preferences_autosave_seconds (prefs));

  g_string_free (string, TRUE);
}

void
glade_preferences_load (GladePreferences *prefs,
			GKeyFile         *config)
{
  gchar *string;
  gboolean backups = TRUE;
  gboolean autosave = TRUE;
  gint autosave_seconds = 30;
  
  string = g_key_file_get_string (config, CONFIG_GROUP, CONFIG_KEY_CATALOG_PATHS, NULL);

  if (string && g_strcmp0 (string, ""))
    {
      GtkComboBoxText *combo = prefs->priv->catalog_path_combo;
      gchar **paths, **path;

      gtk_combo_box_text_remove_all (combo);
      glade_catalog_remove_path (NULL);

      paths = g_strsplit (string, ":", -1);

      path = paths;
      do
        {
          glade_catalog_add_path (*path);
          gtk_combo_box_text_append (combo, *path, *path);
        } while (*++path);

      gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
      g_strfreev (paths);
    }

  if (g_key_file_has_key (config, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_BACKUP, NULL))
    backups = g_key_file_get_boolean (config, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_BACKUP, NULL);

  if (g_key_file_has_key (config, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_AUTOSAVE, NULL))
    autosave = g_key_file_get_boolean (config, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_AUTOSAVE, NULL);

  if (g_key_file_has_key (config, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_AUTOSAVE_SECONDS, NULL))
    autosave_seconds = g_key_file_get_integer (config, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_AUTOSAVE_SECONDS, NULL);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefs->priv->create_backups_toggle), backups);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (prefs->priv->autosave_toggle), autosave);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (prefs->priv->autosave_spin), autosave_seconds);
  gtk_widget_set_sensitive (prefs->priv->autosave_spin, autosave);

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
