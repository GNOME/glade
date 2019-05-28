/*
 * Copyright 2019 Collabora Ltd.
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
 *   Corentin Noël <corentin.noel@collabora.com>
 */
#include <gladeui/glade-catalog.h>
#include <gladeui/glade-utils.h>
#include <gladeui/gladeui-enum-types.h>

#include "glade-settings.h"

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

struct _GladeSettings
{
  GObject parent_instance;

  gboolean backup;
  gboolean autosave;
  gint autosave_seconds;
  GladeVerifyFlags flags;
};

enum
{
  PROP_BACKUP = 1,
  PROP_AUTOSAVE,
  PROP_AUTOSAVE_SECONDS,
  PROP_VERIFY_FLAGS,
  N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (GladeSettings, glade_settings, G_TYPE_OBJECT)

static void
glade_settings_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GladeSettings *self = GLADE_SETTINGS (object);

  switch (property_id)
    {
    case PROP_BACKUP:
      self->backup = g_value_get_boolean (value);
      break;

    case PROP_AUTOSAVE:
      self->autosave = g_value_get_boolean (value);
      break;

    case PROP_AUTOSAVE_SECONDS:
      self->autosave_seconds = g_value_get_int (value);
      break;

    case PROP_VERIFY_FLAGS:
      self->flags = g_value_get_flags (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
glade_settings_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GladeSettings *self = GLADE_SETTINGS (object);

  switch (property_id)
    {
    case PROP_BACKUP:
      g_value_set_boolean (value, self->backup);
      break;

    case PROP_AUTOSAVE:
      g_value_set_boolean (value, self->autosave);
      break;

    case PROP_AUTOSAVE_SECONDS:
      g_value_set_int (value, self->autosave_seconds);
      break;

    case PROP_VERIFY_FLAGS:
      g_value_set_flags (value, self->flags);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
glade_settings_class_init (GladeSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = glade_settings_set_property;
  object_class->get_property = glade_settings_get_property;

  obj_properties[PROP_BACKUP] =
    g_param_spec_boolean ("backup",
                          "Backup",
                          "Whether a backup of the edited file is required.",
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_AUTOSAVE] =
    g_param_spec_boolean ("autosave",
                          "Autosave",
                          "Save the files automatically.",
                          TRUE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_AUTOSAVE_SECONDS] =
    g_param_spec_int ("autosave-seconds",
                      "Autosave Seconds",
                      "Time in seconds for saving the files automatically.",
                      G_MININT, G_MAXINT, 5,
                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_properties[PROP_VERIFY_FLAGS] =
    g_param_spec_flags ("verify-flags",
                        "Verify flags",
                        "Verify Flags.",
                        GLADE_TYPE_VERIFY_FLAGS,
                        GLADE_VERIFY_VERSIONS | GLADE_VERIFY_UNRECOGNIZED,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     N_PROPERTIES,
                                     obj_properties);
}

static void
glade_settings_init (GladeSettings *self)
{
  self->backup = TRUE;
  self->autosave = TRUE;
  self->autosave_seconds = 5;
  self->flags = GLADE_VERIFY_VERSIONS | GLADE_VERIFY_UNRECOGNIZED;
}

GladeSettings *
glade_settings_new (void)
{
  return g_object_new (GLADE_TYPE_SETTINGS, NULL);
}

void
glade_settings_save (GladeSettings *self,
                     GKeyFile      *file)
{
  const GList *paths, *l;
  GString *string;

  g_return_if_fail (GLADE_IS_SETTINGS (self));

  string = g_string_new ("");
  paths = glade_catalog_get_extra_paths ();
  for (l = paths; l != NULL; l = l->next)
    {
      const gchar *path = (const gchar *)(l->data);
      g_string_append (string, path);
      if (l->next != NULL)
        g_string_append (string, ":");
    }

  g_key_file_set_string (file, CONFIG_GROUP, CONFIG_KEY_CATALOG_PATHS,
                         string->str);

  g_key_file_set_boolean (file, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_BACKUP,
                          self->backup);
  g_key_file_set_boolean (file, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_AUTOSAVE,
                          self->autosave);
  g_key_file_set_integer (file, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_AUTOSAVE_SECONDS,
                          self->autosave_seconds);

  g_key_file_set_boolean (file, CONFIG_GROUP_SAVE_WARNINGS, CONFIG_KEY_VERSIONING,
                          self->flags & GLADE_VERIFY_VERSIONS);
  g_key_file_set_boolean (file, CONFIG_GROUP_SAVE_WARNINGS, CONFIG_KEY_DEPRECATIONS,
                          self->flags & GLADE_VERIFY_DEPRECATIONS);
  g_key_file_set_boolean (file, CONFIG_GROUP_SAVE_WARNINGS, CONFIG_KEY_UNRECOGNIZED,
                          self->flags & GLADE_VERIFY_UNRECOGNIZED);

  g_string_free (string, TRUE);
}

void
glade_settings_load (GladeSettings *self,
                     GKeyFile      *file)
{
  gchar *paths_string;

  g_return_if_fail (GLADE_IS_SETTINGS (self));

  paths_string = g_key_file_get_string (file, CONFIG_GROUP, CONFIG_KEY_CATALOG_PATHS, NULL);
  if (paths_string && g_strcmp0 (paths_string, ""))
    {
      gchar **paths = g_strsplit (paths_string, ":", -1);
      guint paths_len = g_strv_length (paths);

      glade_catalog_remove_path (NULL);

      for (guint i = 0; i < paths_len; i++)
        {
          gchar *canonical = glade_util_canonical_path (paths[i]);
          glade_catalog_add_path (canonical);
          g_free (canonical);
        }

      g_strfreev (paths);
    }
  g_free (paths_string);

  /* Load and save */
  if (g_key_file_has_key (file, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_BACKUP, NULL))
    self->backup = g_key_file_get_boolean (file, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_BACKUP, NULL);

  if (g_key_file_has_key (file, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_AUTOSAVE, NULL))
    self->autosave = g_key_file_get_boolean (file, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_AUTOSAVE, NULL);

  if (g_key_file_has_key (file, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_AUTOSAVE_SECONDS, NULL))
    self->autosave_seconds = g_key_file_get_integer (file, CONFIG_GROUP_LOAD_SAVE, CONFIG_KEY_AUTOSAVE_SECONDS, NULL);

  /* Warnings */
  if (g_key_file_has_key (file, CONFIG_GROUP_SAVE_WARNINGS, CONFIG_KEY_VERSIONING, NULL))
    {
      if (g_key_file_get_boolean (file, CONFIG_GROUP_SAVE_WARNINGS, CONFIG_KEY_VERSIONING, NULL))
        self->flags |= GLADE_VERIFY_VERSIONS;
      else
        self->flags &= ~GLADE_VERIFY_VERSIONS;
    }

  if (g_key_file_has_key (file, CONFIG_GROUP_SAVE_WARNINGS, CONFIG_KEY_DEPRECATIONS, NULL))
    {
      if (g_key_file_get_boolean (file, CONFIG_GROUP_SAVE_WARNINGS, CONFIG_KEY_DEPRECATIONS, NULL))
        self->flags |= GLADE_VERIFY_DEPRECATIONS;
      else
        self->flags &= ~GLADE_VERIFY_DEPRECATIONS;
    }

  if (g_key_file_has_key (file, CONFIG_GROUP_SAVE_WARNINGS, CONFIG_KEY_UNRECOGNIZED, NULL))
    {
      if (g_key_file_get_boolean (file, CONFIG_GROUP_SAVE_WARNINGS, CONFIG_KEY_UNRECOGNIZED, NULL))
        self->flags |= GLADE_VERIFY_UNRECOGNIZED;
      else
        self->flags &= ~GLADE_VERIFY_UNRECOGNIZED;
    }
}

gboolean
glade_settings_backup (GladeSettings *self)
{
  g_return_val_if_fail (GLADE_IS_SETTINGS (self), FALSE);

  return self->backup;
}

gboolean
glade_settings_autosave (GladeSettings *self)
{
  g_return_val_if_fail (GLADE_IS_SETTINGS (self), FALSE);

  return self->autosave;
}

gint
glade_settings_autosave_seconds (GladeSettings *self)
{
  g_return_val_if_fail (GLADE_IS_SETTINGS (self), 0);

  return self->autosave_seconds;
}

GladeVerifyFlags
glade_settings_get_verify_flags (GladeSettings *self)
{
  g_return_val_if_fail (GLADE_IS_SETTINGS (self), 0);

  return self->flags;
}
