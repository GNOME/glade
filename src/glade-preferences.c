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

struct _GladePreferences
{
  GObject *toplevel;
  GtkComboBoxText *catalog_path_combo;
};

#define CONFIG_GROUP "Preferences"
#define CONFIG_KEY_CATALOG_PATHS "catalog-paths"

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

GladePreferences *
glade_preferences_new (GtkBuilder *builder)
{
  GladePreferences *prefs = g_new0 (GladePreferences, 1);

  prefs->toplevel = gtk_builder_get_object (builder, "preferences_dialog");
  prefs->catalog_path_combo = GTK_COMBO_BOX_TEXT (gtk_builder_get_object (builder, "catalog_path_comboboxtext"));
  combo_box_text_init_cell (GTK_CELL_LAYOUT (prefs->catalog_path_combo));
  
  return prefs;
}

void
glade_preferences_destroy (GladePreferences *prefs)
{
  g_object_unref (prefs->toplevel);
  g_free (prefs);
}

void
glade_preferences_config_save (GladePreferences *prefs, GKeyFile *config)
{
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (prefs->catalog_path_combo));
  gint column = gtk_combo_box_get_entry_text_column (GTK_COMBO_BOX (prefs->catalog_path_combo));
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

  g_string_free (string, TRUE);
}

void
glade_preferences_config_load (GladePreferences *prefs, GKeyFile *config)
{
  gchar *string;
  
  string = g_key_file_get_string (config, CONFIG_GROUP, CONFIG_KEY_CATALOG_PATHS, NULL);

  if (string && g_strcmp0 (string, ""))
    {
      GtkComboBoxText *combo = prefs->catalog_path_combo;
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

  g_free (string);
}

/* Callbacks */

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

void
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

void
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
