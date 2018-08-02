/*
 * Copyright (C) 2013 Tristan Van Berkom.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#include <config.h>
#include <glib/gi18n-lib.h>

#include "glade-project-properties.h"
#include "glade-project.h"
#include "glade-command.h"
#include "glade-app.h"
#include "glade-utils.h"
#include "glade-private.h"

/* GObjectClass */
static void     glade_project_properties_finalize     (GObject                *object);
static void     glade_project_properties_set_property (GObject                *object,
                                                       guint                   prop_id,
                                                       const GValue           *value,
                                                       GParamSpec             *pspec);

/* UI Callbacks */
static void     on_template_combo_box_changed         (GtkComboBox            *combo,
                                                       GladeProjectProperties *properties);
static void     on_template_checkbutton_toggled       (GtkToggleButton        *togglebutton,
                                                       GladeProjectProperties *properties);
static void     resource_default_toggled              (GtkWidget              *widget,
                                                       GladeProjectProperties *properties);
static void     resource_relative_toggled             (GtkWidget              *widget,
                                                       GladeProjectProperties *properties);
static void     resource_fullpath_toggled             (GtkWidget              *widget,
                                                       GladeProjectProperties *properties);
static void     on_relative_path_entry_insert_text    (GtkEditable            *editable,
                                                       gchar                  *new_text,
                                                       gint                    new_text_length,
                                                       gint                   *position,
                                                       GladeProjectProperties *properties); 
static void     on_relative_path_entry_changed        (GtkEntry               *entry,
                                                       GladeProjectProperties *properties);
static void     resource_full_path_set                (GtkFileChooserButton   *button,
                                                       GladeProjectProperties *properties);
static void     verify_clicked                        (GtkWidget              *button,
                                                       GladeProjectProperties *properties);
static void     on_domain_entry_changed               (GtkWidget              *entry,
                                                       GladeProjectProperties *properties);
static void     target_button_clicked                 (GtkWidget              *widget,
                                                       GladeProjectProperties *properties);
static void     on_glade_project_properties_hide      (GtkWidget              *widget,
                                                       GladeProjectProperties *properties);
static void     on_css_filechooser_file_set           (GtkFileChooserButton   *widget,
                                                       GladeProjectProperties *properties);
static void     on_css_checkbutton_toggled            (GtkWidget              *widget,
                                                       GladeProjectProperties *properties);
static void     on_license_comboboxtext_changed       (GtkComboBox *widget,
                                                       GladeProjectProperties *properties);

static void     on_license_data_changed               (GladeProjectProperties *properties);

/* Project callbacks */
static void     project_resource_path_changed         (GladeProject           *project,
                                                       GParamSpec             *pspec,
                                                       GladeProjectProperties *properties);
static void     project_template_changed              (GladeProject           *project,
                                                       GParamSpec             *pspec,
                                                       GladeProjectProperties *properties);
static void     project_domain_changed                (GladeProject           *project,
                                                       GParamSpec             *pspec,
                                                       GladeProjectProperties *properties);
static void     project_targets_changed               (GladeProject           *project,
                                                       GladeProjectProperties *properties);
static void     project_license_changed               (GladeProject           *project,
                                                       GParamSpec             *pspec,
                                                       GladeProjectProperties *properties);
static void     project_css_provider_path_changed     (GladeProject           *project,
                                                       GParamSpec             *pspec,
                                                       GladeProjectProperties *properties);

struct _GladeProjectPropertiesPrivate
{
  GladeProject *project;

  /* Properties */
  GtkWidget *project_wide_radio;
  GtkWidget *toplevel_contextual_radio;
  GtkWidget *toolkit_box;

  GtkWidget *resource_default_radio;
  GtkWidget *resource_relative_radio;
  GtkWidget *resource_fullpath_radio;
  GtkWidget *relative_path_entry;
  GtkWidget *full_path_button;
  GtkWidget *domain_entry;
  GtkWidget *template_combobox;
  GtkWidget *template_checkbutton;

  GtkWidget *css_filechooser;
  GtkWidget *css_checkbutton;
  
  GHashTable *target_radios;

  /* License */
  GtkComboBox    *license_comboboxtext;
  GtkTextView    *license_textview;
  GtkEntryBuffer *name_entrybuffer;
  GtkEntryBuffer *description_entrybuffer;
  GtkTextBuffer  *authors_textbuffer;
  GtkTextBuffer  *copyright_textbuffer;
  GtkTextBuffer  *license_textbuffer;
  
  gboolean ignore_ui_cb;
};

enum
{
  PROP_0,
  PROP_PROJECT,
};

G_DEFINE_TYPE_WITH_PRIVATE (GladeProjectProperties, glade_project_properties, GTK_TYPE_DIALOG);

/********************************************************
 *                  Class/Instance Init                 *
 ********************************************************/
static void
glade_project_properties_init (GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv;

  properties->priv = priv = glade_project_properties_get_instance_private (properties);

  priv->target_radios = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               g_free, NULL);

  gtk_widget_init_template (GTK_WIDGET (properties));
}

static void
glade_project_properties_class_init (GladeProjectPropertiesClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  widget_class  = GTK_WIDGET_CLASS (klass);

  gobject_class->finalize = glade_project_properties_finalize;
  gobject_class->set_property = glade_project_properties_set_property;

  g_object_class_install_property
    (gobject_class, PROP_PROJECT,
     g_param_spec_object ("project", _("Project"),
                          _("The project this properties dialog was created for"),
                          GLADE_TYPE_PROJECT,
                          G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  /* Setup the template GtkBuilder xml for this class
   */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladeui/glade-project-properties.ui");

  /* Define the relationship of the private entry and the entry defined in the xml
   */
  gtk_widget_class_bind_template_child_private (widget_class, GladeProjectProperties, resource_default_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeProjectProperties, resource_relative_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeProjectProperties, resource_fullpath_radio);
  gtk_widget_class_bind_template_child_private (widget_class, GladeProjectProperties, relative_path_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GladeProjectProperties, full_path_button);
  gtk_widget_class_bind_template_child_private (widget_class, GladeProjectProperties, domain_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GladeProjectProperties, template_checkbutton);
  gtk_widget_class_bind_template_child_private (widget_class, GladeProjectProperties, template_combobox);
  gtk_widget_class_bind_template_child_private (widget_class, GladeProjectProperties, toolkit_box);
  gtk_widget_class_bind_template_child_private (widget_class, GladeProjectProperties, css_filechooser);
  gtk_widget_class_bind_template_child_private (widget_class, GladeProjectProperties, css_checkbutton);

  gtk_widget_class_bind_template_child_private (widget_class, GladeProjectProperties, license_comboboxtext);
  gtk_widget_class_bind_template_child_private (widget_class, GladeProjectProperties, license_textview);
  gtk_widget_class_bind_template_child_private (widget_class, GladeProjectProperties, name_entrybuffer);
  gtk_widget_class_bind_template_child_private (widget_class, GladeProjectProperties, description_entrybuffer);
  gtk_widget_class_bind_template_child_private (widget_class, GladeProjectProperties, authors_textbuffer);
  gtk_widget_class_bind_template_child_private (widget_class, GladeProjectProperties, copyright_textbuffer);
  gtk_widget_class_bind_template_child_private (widget_class, GladeProjectProperties, license_textbuffer);

  
  /* Declare the callback ports that this widget class exposes, to bind with <signal>
   * connections defined in the GtkBuilder xml
   */
  gtk_widget_class_bind_template_callback (widget_class, on_template_combo_box_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_template_checkbutton_toggled);
  gtk_widget_class_bind_template_callback (widget_class, resource_default_toggled);
  gtk_widget_class_bind_template_callback (widget_class, resource_relative_toggled);
  gtk_widget_class_bind_template_callback (widget_class, resource_fullpath_toggled);
  gtk_widget_class_bind_template_callback (widget_class, resource_full_path_set);
  gtk_widget_class_bind_template_callback (widget_class, verify_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_domain_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_relative_path_entry_insert_text);
  gtk_widget_class_bind_template_callback (widget_class, on_relative_path_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_glade_project_properties_hide);  
  gtk_widget_class_bind_template_callback (widget_class, on_css_filechooser_file_set);
  gtk_widget_class_bind_template_callback (widget_class, on_css_checkbutton_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_license_comboboxtext_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_license_data_changed);
}

/********************************************************
 *                     GObjectClass                     *
 ********************************************************/
static void
glade_project_properties_finalize (GObject *object)
{
  GladeProjectProperties        *properties = GLADE_PROJECT_PROPERTIES (object);
  GladeProjectPropertiesPrivate *priv = properties->priv;

  g_hash_table_destroy (priv->target_radios);

  G_OBJECT_CLASS (glade_project_properties_parent_class)->finalize (object);
}

static void
target_version_box_fill (GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  GladeProject *project = priv->project;
  GtkWidget *vbox = priv->toolkit_box;
  GtkWidget *label, *active_radio, *target_radio, *hbox;
  GList *list, *targets;

  /* Add stuff to vbox */
  for (list = glade_app_get_catalogs (); list; list = g_list_next (list))
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
      gtk_widget_set_halign (label, GTK_ALIGN_START);

      gtk_widget_show (label);
      gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 2);
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

      active_radio = target_radio = NULL;

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
                            G_CALLBACK (target_button_clicked), properties);

          g_object_set_data (G_OBJECT (target_radio), "version", version);
          g_object_set_data (G_OBJECT (target_radio), "catalog",
                             (gchar *) glade_catalog_get_name (catalog));

          gtk_widget_show (target_radio);
          gtk_box_pack_end (GTK_BOX (hbox), target_radio, TRUE, TRUE, 2);

          if (major == version->major && minor == version->minor)
            active_radio = target_radio;

        }

      if (active_radio)
        {
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (active_radio), TRUE);
          g_hash_table_insert (priv->target_radios,
                               g_strdup (glade_catalog_get_name (catalog)),
                               gtk_radio_button_get_group (GTK_RADIO_BUTTON
                                                           (active_radio)));
        }
      else
        g_warning ("Corrupt catalog versions");

      gtk_widget_show (hbox);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 2);
    }
}

static void
update_prefs_for_resource_path (GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  const gchar *resource_path;

  resource_path = glade_project_get_resource_path (priv->project);
  
  if (resource_path == NULL)
    {
      gtk_entry_set_text (GTK_ENTRY (priv->relative_path_entry), "");
      gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (priv->full_path_button));

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->resource_default_radio), TRUE);
      gtk_widget_set_sensitive (priv->full_path_button, FALSE);
      gtk_widget_set_sensitive (priv->relative_path_entry, FALSE);
    }
  else if (g_path_is_absolute (resource_path) &&
           g_file_test (resource_path, G_FILE_TEST_IS_DIR))
    {
      gtk_entry_set_text (GTK_ENTRY (priv->relative_path_entry), "");
      gtk_file_chooser_select_filename (GTK_FILE_CHOOSER (priv->full_path_button),
                                        resource_path);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->resource_fullpath_radio), TRUE);
      gtk_widget_set_sensitive (priv->full_path_button, TRUE);
      gtk_widget_set_sensitive (priv->relative_path_entry, FALSE);
    }
  else
    {
      if (g_strcmp0 (resource_path, gtk_entry_get_text (GTK_ENTRY (priv->relative_path_entry))))
        gtk_entry_set_text (GTK_ENTRY (priv->relative_path_entry), resource_path);

      gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (priv->full_path_button));

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->resource_relative_radio), TRUE);
      gtk_widget_set_sensitive (priv->relative_path_entry, TRUE);
      gtk_widget_set_sensitive (priv->full_path_button, FALSE);
    }
}

static void
glade_project_properties_set_project (GladeProjectProperties *properties,
                                      GladeProject           *project)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;

  /* No strong reference, we belong to the project */
  g_assert (priv->project == NULL);
  priv->project = project;

  g_signal_connect (priv->project, "notify::resource-path",
                    G_CALLBACK (project_resource_path_changed), properties);
  g_signal_connect (priv->project, "notify::template",
                    G_CALLBACK (project_template_changed), properties);
  g_signal_connect (priv->project, "notify::translation-domain",
                    G_CALLBACK (project_domain_changed), properties);
  g_signal_connect (priv->project, "notify::css-provider-path",
                    G_CALLBACK (project_css_provider_path_changed), properties);  
  g_signal_connect (priv->project, "targets-changed",
                    G_CALLBACK (project_targets_changed), properties);
  g_signal_connect (priv->project, "notify::license",
                    G_CALLBACK (project_license_changed), properties);

  target_version_box_fill (properties);
  update_prefs_for_resource_path (properties);

  project_template_changed (project, NULL, properties);
  project_domain_changed (project, NULL, properties);
  project_css_provider_path_changed (project, NULL, properties);
}

static void
glade_project_properties_set_property (GObject *object,
                                       guint prop_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_PROJECT:
      glade_project_properties_set_project (GLADE_PROJECT_PROPERTIES (object),
                                            g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/********************************************************
 *                     Callbacks                        *
 ********************************************************/
static void
target_button_clicked (GtkWidget              *widget,
                       GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  GladeTargetableVersion        *version;
  gchar                         *catalog;

  if (priv->ignore_ui_cb)
    return;

  version = g_object_get_data (G_OBJECT (widget), "version");
  catalog = g_object_get_data (G_OBJECT (widget), "catalog");
  glade_command_set_project_target (priv->project, catalog, version->major, version->minor);
}

static void
resource_default_toggled (GtkWidget              *widget,
                          GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;

  if (priv->ignore_ui_cb || 
      !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    return;

  glade_command_set_project_resource_path (priv->project, NULL);
}

static void
resource_relative_toggled (GtkWidget              *widget,
                           GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  GtkToggleButton *toggle = GTK_TOGGLE_BUTTON (widget);
  
  if (priv->ignore_ui_cb || !gtk_toggle_button_get_active (toggle))
    return;

  glade_command_set_project_resource_path (priv->project, NULL);
  gtk_toggle_button_set_active (toggle, TRUE);
  gtk_widget_set_sensitive (priv->relative_path_entry, TRUE);
  gtk_widget_set_sensitive (priv->full_path_button, FALSE);
}

static void
resource_fullpath_toggled (GtkWidget              *widget,
                           GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  GtkToggleButton *toggle = GTK_TOGGLE_BUTTON (widget);

  if (priv->ignore_ui_cb || !gtk_toggle_button_get_active (toggle))
    return;

  glade_command_set_project_resource_path (priv->project, NULL);
  gtk_toggle_button_set_active (toggle, TRUE);
  gtk_widget_set_sensitive (priv->relative_path_entry, FALSE);
  gtk_widget_set_sensitive (priv->full_path_button, TRUE);
}

static void
on_relative_path_entry_insert_text (GtkEditable            *editable,
                                    gchar                  *new_text,
                                    gint                    new_text_length,
                                    gint                   *position,
                                    GladeProjectProperties *properties) 
{
  GString *fullpath = g_string_new (gtk_entry_get_text (GTK_ENTRY(editable)));

  g_string_insert (fullpath, *position, new_text);
  
  if (g_path_is_absolute (fullpath->str))
    g_signal_stop_emission_by_name (editable, "insert-text");
  
  g_string_free (fullpath, TRUE);
}

static void
on_relative_path_entry_changed (GtkEntry *entry, GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;

  if (priv->ignore_ui_cb)
    return;

  glade_command_set_project_resource_path (priv->project, gtk_entry_get_text (entry));
}

static void
resource_full_path_set (GtkFileChooserButton *button, GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  gchar *text;
  
  if (priv->ignore_ui_cb)
    return;
  
  text = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (button));
  glade_command_set_project_resource_path (priv->project, text);
  g_free (text);
}

static void
on_template_combo_box_changed (GtkComboBox            *combo,
                               GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  GtkTreeIter iter;

  if (priv->ignore_ui_cb)
    return;
  
  if (gtk_combo_box_get_active_iter (combo, &iter))
    {
      GladeWidget *gwidget;
      GObject *object;

      gtk_tree_model_get (gtk_combo_box_get_model (combo), &iter,
                          GLADE_PROJECT_MODEL_COLUMN_OBJECT, &object, -1);
      
      gwidget = glade_widget_get_from_gobject (object);

      glade_command_set_project_template (priv->project, gwidget);
    }
}

static void
on_template_checkbutton_toggled (GtkToggleButton        *togglebutton,
                                 GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;

  if (priv->ignore_ui_cb)
    return;
  
  if (gtk_toggle_button_get_active (togglebutton))
    {
      gboolean composite = FALSE;
      GList *l;

      for (l = glade_project_toplevels (priv->project); l; l = l->next)
        {
          GObject *object = l->data;
          GladeWidget *gwidget;

          gwidget = glade_widget_get_from_gobject (object);

          if (GTK_IS_WIDGET (object))
            {
              glade_command_set_project_template (priv->project, gwidget);
              composite = TRUE;
              break;
            }
        }

      if (!composite)
        gtk_toggle_button_set_active (togglebutton, FALSE);
    }
  else
    glade_command_set_project_template (priv->project, NULL);
}

static gboolean
template_visible_func (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  GtkTreeIter parent;
  gboolean visible;
  GObject *object;

  visible = !gtk_tree_model_iter_parent (model, &parent, iter);

  if (visible)
    {
      gtk_tree_model_get (model, iter,
                          GLADE_PROJECT_MODEL_COLUMN_OBJECT, &object,
                          -1);

      visible = GTK_IS_WIDGET (object);
      g_object_unref (object);
    }

  return visible;
}

static GtkTreeModel *
glade_project_toplevel_model_filter_new (GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  GtkTreeModel *model;

  model = gtk_tree_model_filter_new (GTK_TREE_MODEL (priv->project), NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (model),
                                          template_visible_func, NULL, NULL);
  return model;
}

static void
verify_clicked (GtkWidget *button, GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;

  if (glade_project_verify (priv->project, FALSE,
                            GLADE_VERIFY_VERSIONS     |
                            GLADE_VERIFY_DEPRECATIONS |
                            GLADE_VERIFY_UNRECOGNIZED))
    {
      gchar *name = glade_project_get_name (priv->project);
      glade_util_ui_message (glade_app_get_window (),
                             GLADE_UI_INFO, NULL,
                             _("Project %s has no deprecated widgets "
                               "or version mismatches."), name);
      g_free (name);
    }
}

static void
on_domain_entry_changed (GtkWidget *entry, GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;

  if (priv->ignore_ui_cb)
    return;

  glade_command_set_project_domain (priv->project, gtk_entry_get_text (GTK_ENTRY (entry)));
}

#define GNU_GPLv2_TEXT \
    "$(name) - $(description)\n" \
    "Copyright (C) $(copyright)\n" \
    "\n" \
    "This program is free software; you can redistribute it and/or\n" \
    "modify it under the terms of the GNU General Public License\n" \
    "as published by the Free Software Foundation; either version 2\n" \
    "of the License, or (at your option) any later version.\n" \
    "\n" \
    "This program is distributed in the hope that it will be useful,\n" \
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n" \
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n" \
    "GNU General Public License for more details.\n" \
    "\n" \
    "You should have received a copy of the GNU General Public License\n" \
    "along with this program; if not, write to the Free Software\n" \
    "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.\n"

#define GNU_LGPLv2_TEXT \
    "$(name) - $(description)\n" \
    "Copyright (C) $(copyright)\n" \
    "\n" \
    "This library is free software; you can redistribute it and/or\n" \
    "modify it under the terms of the GNU Lesser General Public\n" \
    "License as published by the Free Software Foundation; either\n" \
    "version 2.1 of the License, or (at your option) any later version.\n" \
    "\n" \
    "This library is distributed in the hope that it will be useful,\n" \
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n" \
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n" \
    "Lesser General Public License for more details.\n" \
    "\n" \
    "You should have received a copy of the GNU Lesser General Public\n" \
    "License along with this library; if not, write to the Free Software\n" \
    "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA\n"

#define GNU_GPLv3_TEXT \
    "Copyright (C) $(copyright)\n" \
    "\n" \
    "This file is part of $(name).\n" \
    "\n" \
    "$(name) is free software: you can redistribute it and/or modify\n" \
    "it under the terms of the GNU General Public License as published by\n" \
    "the Free Software Foundation, either version 3 of the License, or\n" \
    "(at your option) any later version.\n" \
    "\n" \
    "$(name) is distributed in the hope that it will be useful,\n" \
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n" \
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n" \
    "GNU General Public License for more details.\n" \
    "\n" \
    "You should have received a copy of the GNU General Public License\n" \
    "along with $(name).  If not, see <http://www.gnu.org/licenses/>.\n"

#define GNU_LGPLv3_TEXT \
    "Copyright (C) $(copyright)\n" \
    "\n" \
    "This file is part of $(name).\n" \
    "\n" \
    "$(name) is free software: you can redistribute it and/or modify\n" \
    "it under the terms of the GNU Lesser General Public License as published by\n" \
    "the Free Software Foundation, either version 3 of the License, or\n" \
    "(at your option) any later version.\n" \
    "\n" \
    "$(name) is distributed in the hope that it will be useful,\n" \
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n" \
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n" \
    "GNU Lesser General Public License for more details.\n" \
    "\n" \
    "You should have received a copy of the GNU Lesser General Public License\n" \
    "along with $(name).  If not, see <http://www.gnu.org/licenses/>.\n"

#define BSD3c_TEXT \
    "Copyright (c) $(copyright)\n" \
    "All rights reserved.\n" \
    "\n" \
    "Redistribution and use in source and binary forms, with or without\n" \
    "modification, are permitted provided that the following conditions are met:\n" \
    "    * Redistributions of source code must retain the above copyright\n" \
    "      notice, this list of conditions and the following disclaimer.\n" \
    "    * Redistributions in binary form must reproduce the above copyright\n" \
    "      notice, this list of conditions and the following disclaimer in the\n" \
    "      documentation and/or other materials provided with the distribution.\n" \
    "    * Neither the name of the <organization> nor the\n" \
    "      names of its contributors may be used to endorse or promote products\n" \
    "      derived from this software without specific prior written permission.\n" \
    "\n" \
    "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\" AND\n" \
    "ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED\n" \
    "WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE\n" \
    "DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY\n" \
    "DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES\n" \
    "(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;\n" \
    "LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND\n" \
    "ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n" \
    "(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS\n" \
    "SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n"

#define BSD2c_TEXT \
    "Copyright (c) $(copyright)\n" \
    "All rights reserved.\n" \
    "\n" \
    "Redistribution and use in source and binary forms, with or without\n" \
    "modification, are permitted provided that the following conditions are met:\n" \
    "\n" \
    "1. Redistributions of source code must retain the above copyright notice, this\n" \
    "   list of conditions and the following disclaimer. \n" \
    "2. Redistributions in binary form must reproduce the above copyright notice,\n" \
    "   this list of conditions and the following disclaimer in the documentation\n" \
    "   and/or other materials provided with the distribution. \n" \
    "\n" \
    "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\" AND\n" \
    "ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED\n" \
    "WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE\n" \
    "DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR\n" \
    "ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES\n" \
    "(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;\n" \
    "LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND\n" \
    "ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n" \
    "(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS\n" \
    "SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n"

#define APACHE2_TEXT \
    "Copyright $(copyright)\n" \
    "\n" \
    "Licensed under the Apache License, Version 2.0 (the \"License\"); \n" \
    "you may not use this file except in compliance with the License. \n" \
    "You may obtain a copy of the License at \n" \
    "\n" \
    "    http://www.apache.org/licenses/LICENSE-2.0 \n" \
    "\n" \
    "Unless required by applicable law or agreed to in writing, software \n" \
    "distributed under the License is distributed on an \"AS IS\" BASIS, \n" \
    "WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. \n" \
    "See the License for the specific language governing permissions and \n" \
    "limitations under the License. \n"

#define GNU_ALL_PERMISSIVE_TEXT \
    "Copyright (C) $(copyright)\n" \
    "\n" \
    "Copying and distribution of this file, with or without modification,\n" \
    "are permitted in any medium without royalty provided the copyright\n" \
    "notice and this notice are preserved.  This file is offered as-is,\n" \
    "without any warranty.\n"

#define MIT_TEXT \
    "The MIT License (MIT)\n" \
    "\n" \
    "Copyright (c) $(copyright)\n" \
    "\n" \
    "Permission is hereby granted, free of charge, to any person obtaining a copy\n" \
    "of this software and associated documentation files (the \"Software\"), to deal\n" \
    "in the Software without restriction, including without limitation the rights\n" \
    "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n" \
    "copies of the Software, and to permit persons to whom the Software is\n" \
    "furnished to do so, subject to the following conditions:\n" \
    "\n" \
    "The above copyright notice and this permission notice shall be included in\n" \
    "all copies or substantial portions of the Software.\n" \
    "\n" \
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n" \
    "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n" \
    "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n" \
    "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n" \
    "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n" \
    "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN\n" \
    "THE SOFTWARE.\n"

static gchar *
gpp_get_license_from_id (const gchar *id)
{
  if (!g_strcmp0 (id, "gplv2"))
    return GNU_GPLv2_TEXT;
  else if (!g_strcmp0 (id, "gplv3"))
    return GNU_GPLv3_TEXT;
  else if (!g_strcmp0 (id, "lgplv2"))
    return GNU_LGPLv2_TEXT;
  else if (!g_strcmp0 (id, "lgplv3"))
    return GNU_LGPLv3_TEXT;
  else if (!g_strcmp0 (id, "bsd2c"))
    return BSD2c_TEXT;
  else if (!g_strcmp0 (id, "bsd3c"))
    return BSD3c_TEXT;
  else if (!g_strcmp0 (id, "apache2"))
    return APACHE2_TEXT;
  else if (!g_strcmp0 (id, "mit"))
    return MIT_TEXT;
  else if (!g_strcmp0 (id, "all_permissive"))
    return GNU_ALL_PERMISSIVE_TEXT;
  else
    return NULL;
}

static gint
string_count_new_lines (const gchar *str)
{
  gint c = 0;

  while (*str)
    {
      if (*str == '\n')
        c++;
      str = g_utf8_next_char (str);
    }
  return c;
}

static void
gpp_update_license (GladeProjectProperties *properties, gchar *license)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  const gchar *name, *description;
  gchar *copyright, *authors;

  if (!license)
    return;

  /* get data */
  name = gtk_entry_buffer_get_text (priv->name_entrybuffer);
  description = gtk_entry_buffer_get_text (priv->description_entrybuffer);
  
  g_object_get (priv->copyright_textbuffer, "text", &copyright, NULL);
  g_object_get (priv->authors_textbuffer, "text", &authors, NULL);
  
  /* Now we can replace strings in the license template */
  license = _glade_util_strreplace (license, FALSE, "$(name)", name);
  license = _glade_util_strreplace (license, TRUE, "$(description)", description);
  license = _glade_util_strreplace (license, TRUE, "$(copyright)", copyright);

  if (authors && *authors)
    {
      gchar *tmp = license;

      if (string_count_new_lines (authors))
        license = g_strconcat (license, "\n", "Authors:", "\n", authors, NULL);
      else
        license = g_strconcat (license, "\n", "Author:", " ", authors, NULL);

      g_free (tmp);
    }
  
  gtk_text_buffer_set_text (priv->license_textbuffer, license, -1);

  g_free (license);
  g_free (copyright);
  g_free (authors);
}

static void
on_license_data_changed (GladeProjectProperties *properties)
{
  const gchar *id = gtk_combo_box_get_active_id (properties->priv->license_comboboxtext);
  gchar *license;

  if ((license = gpp_get_license_from_id (id)))
    gpp_update_license (properties, license);
}

static void
on_license_comboboxtext_changed (GtkComboBox            *widget,
                                 GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  gchar *license;

  if ((license = gpp_get_license_from_id (gtk_combo_box_get_active_id (widget))))
    {
      gpp_update_license (properties, license);
      gtk_text_view_set_editable (priv->license_textview, FALSE);
    }
  else
    {
      /* Other license */
      gtk_text_buffer_set_text (priv->license_textbuffer, "", -1);      
      gtk_text_view_set_editable (priv->license_textview, TRUE);
      gtk_widget_grab_focus (GTK_WIDGET (priv->license_textview));
    }
}

static void
on_glade_project_properties_hide (GtkWidget              *widget,
                                  GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  GtkTextIter start, end;
  gchar *license;

  gtk_text_buffer_get_bounds (priv->license_textbuffer, &start, &end);
  license = gtk_text_buffer_get_text (priv->license_textbuffer, &start, &end, FALSE);
  g_strstrip (license);

  glade_command_set_project_license (priv->project, (license[0] != '\0') ? license : NULL);

  g_free (license);
}

static void
on_css_checkbutton_toggled (GtkWidget *widget, GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;

  if (priv->ignore_ui_cb)
    return;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    {
      gtk_widget_set_sensitive (priv->css_filechooser, TRUE);
    }
  else
    {
      gtk_widget_set_sensitive (priv->css_filechooser, FALSE);
      glade_project_set_css_provider_path (priv->project, NULL);
    }
}

static void
on_css_filechooser_file_set (GtkFileChooserButton   *widget,
                             GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  const gchar *path;

  if (priv->ignore_ui_cb)
    return;

  path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));

  glade_project_set_css_provider_path (priv->project, path);
}

/******************************************************
 *                   Project Callbacks                *
 ******************************************************/
static void
project_targets_changed (GladeProject           *project,
                         GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  GList *list;
  GSList *radios, *l;

  priv->ignore_ui_cb = TRUE;

  /* For each catalog */
  for (list = glade_app_get_catalogs (); list; list = g_list_next (list))
    {
      GladeTargetableVersion *version;
      GladeCatalog *catalog = list->data;
      gint minor, major;

      /* Skip if theres only one option */
      if (g_list_length (glade_catalog_get_targets (catalog)) <= 1)
        continue;

      /* Fetch the version for this project */
      glade_project_get_target_version (priv->project,
                                        glade_catalog_get_name (catalog),
                                        &major, &minor);

      /* Fetch the radios for this catalog  */
      if (priv->target_radios &&
          (radios = g_hash_table_lookup (priv->target_radios, glade_catalog_get_name (catalog))) != NULL)
        {
          for (l = radios; l; l = l->next)
            {
              GtkWidget *radio = l->data;

              /* Activate the appropriate button for the project/catalog */
              version = g_object_get_data (G_OBJECT (radio), "version");
              if (version->major == major && version->minor == minor)
                {
                  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio), TRUE);
                  break;
                }
            }
        }
    }
  priv->ignore_ui_cb = FALSE;
}

static void
project_domain_changed (GladeProject           *project,
                        GParamSpec             *pspec,
                        GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  const gchar *domain;

  priv->ignore_ui_cb = TRUE;
  
  domain = glade_project_get_translation_domain (priv->project);
  gtk_entry_set_text (GTK_ENTRY (priv->domain_entry), domain ? domain : "");

  priv->ignore_ui_cb = FALSE;
}

static void
project_resource_path_changed (GladeProject           *project,
                               GParamSpec             *pspec,
                               GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  priv->ignore_ui_cb = TRUE;
  update_prefs_for_resource_path (properties);
  priv->ignore_ui_cb = FALSE;
}

static void
project_template_changed (GladeProject           *project,
                          GParamSpec             *pspec,
                          GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean valid;
  gboolean template_found = FALSE;

  priv->ignore_ui_cb = TRUE;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->template_combobox));
  if (!model)
    {
      model = glade_project_toplevel_model_filter_new (properties);

      gtk_combo_box_set_model (GTK_COMBO_BOX (priv->template_combobox), model);
      g_object_unref (model);
    }

  valid = gtk_tree_model_get_iter_first (model, &iter);
  while (valid)
    {
      GladeWidget *gwidget;
      GObject *obj;
      
      gtk_tree_model_get (model, &iter,
                          GLADE_PROJECT_MODEL_COLUMN_OBJECT, &obj,
                          -1);

      gwidget = glade_widget_get_from_gobject (obj);
      g_object_unref (obj);

      if (gwidget == glade_project_get_template (priv->project))
        {
          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->template_combobox), &iter);

          template_found = TRUE;
          break;
        }

      valid = gtk_tree_model_iter_next (model, &iter);
    }

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->template_checkbutton), template_found);
  gtk_widget_set_sensitive (priv->template_combobox, template_found);

  if (!template_found && gtk_combo_box_get_model (GTK_COMBO_BOX (priv->template_combobox)))
    gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->template_combobox), NULL);

  priv->ignore_ui_cb = FALSE;
}

static void
project_license_changed (GladeProject           *project,
                         GParamSpec             *pspec,
                         GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;

  priv->ignore_ui_cb = TRUE;  
  gtk_text_buffer_set_text (priv->license_textbuffer, 
                            glade_project_get_license (project),
                            -1);
  priv->ignore_ui_cb = FALSE;
}

static void
project_css_provider_path_changed (GladeProject           *project,
                                   GParamSpec             *pspec,
                                   GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  const gchar *filename = glade_project_get_css_provider_path (project);
  GtkFileChooser *chooser = GTK_FILE_CHOOSER (priv->css_filechooser);

  priv->ignore_ui_cb = TRUE;
  
  if (filename)
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->css_checkbutton), TRUE);
      gtk_widget_set_sensitive (priv->css_filechooser, TRUE);
      gtk_file_chooser_set_filename (chooser, filename);
    }
  else
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->css_checkbutton), FALSE);
      gtk_widget_set_sensitive (priv->css_filechooser, FALSE);
      gtk_file_chooser_unselect_all (chooser);
    }

  priv->ignore_ui_cb = FALSE;
}

/* Private API */

void
_glade_project_properties_set_license_data (GladeProjectProperties *props,
                                            const gchar *license,
                                            const gchar *name,
                                            const gchar *description,
                                            const gchar *copyright,
                                            const gchar *authors)
{
  if (!license ||
      !gtk_combo_box_set_active_id (props->priv->license_comboboxtext, license))
    {
      gtk_combo_box_set_active_id (props->priv->license_comboboxtext, "other");
      name = description = copyright = authors = "";
      license = "other";
    }
        
  gtk_entry_buffer_set_text (props->priv->name_entrybuffer, name ? name : "", -1);
  gtk_entry_buffer_set_text (props->priv->description_entrybuffer, description ? description : "", -1);

  gtk_text_buffer_set_text (props->priv->copyright_textbuffer, copyright ? copyright : "", -1);
  gtk_text_buffer_set_text (props->priv->authors_textbuffer, authors ? authors : "", -1);

  gpp_update_license (props, gpp_get_license_from_id (license));
}

void
_glade_project_properties_get_license_data (GladeProjectProperties *props,
                                            gchar **license,
                                            gchar **name,
                                            gchar **description,
                                            gchar **copyright,
                                            gchar **authors)
{
  const gchar *id = gtk_combo_box_get_active_id (props->priv->license_comboboxtext);

  if (!g_strcmp0 (id, "other"))
    {
      *license = *name = *description = *copyright = *authors = NULL;
      return;
    }

  *license = g_strdup (id);
  *name = g_strdup (gtk_entry_buffer_get_text (props->priv->name_entrybuffer));
  *description = g_strdup (gtk_entry_buffer_get_text (props->priv->description_entrybuffer));

  g_object_get (props->priv->copyright_textbuffer, "text", copyright, NULL);
  g_object_get (props->priv->authors_textbuffer, "text", authors, NULL);
}

/******************************************************
 *                         API                        *
 ******************************************************/
GtkWidget *
glade_project_properties_new (GladeProject *project)
{
  return g_object_new (GLADE_TYPE_PROJECT_PROPERTIES, "project", project, NULL);
}
