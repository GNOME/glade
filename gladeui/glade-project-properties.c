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
static void     resource_path_activated               (GtkEntry               *entry,
						       GladeProjectProperties *properties);
static void     resource_full_path_set                (GtkFileChooserButton   *button,
						       GladeProjectProperties *properties);
static void     verify_clicked                        (GtkWidget              *button,
						       GladeProjectProperties *properties);
static void     on_domain_entry_changed               (GtkWidget              *entry,
						       GladeProjectProperties *properties);
static void     target_button_clicked                 (GtkWidget              *widget,
						       GladeProjectProperties *properties);

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

struct _GladeProjectPropertiesPrivate
{
  GladeProject *project;

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

  GHashTable *target_radios;
};

enum
{
  PROP_0,
  PROP_PROJECT,
};

G_DEFINE_TYPE (GladeProjectProperties, glade_project_properties, GTK_TYPE_DIALOG);

/********************************************************
 *                  Class/Instance Init                 *
 ********************************************************/
static void
glade_project_properties_init (GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv;

  properties->priv = priv =
    G_TYPE_INSTANCE_GET_PRIVATE (properties,
				 GLADE_TYPE_PROJECT_PROPERTIES,
				 GladeProjectPropertiesPrivate);

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
  gtk_widget_class_bind_child (widget_class, GladeProjectPropertiesPrivate, resource_default_radio);
  gtk_widget_class_bind_child (widget_class, GladeProjectPropertiesPrivate, resource_relative_radio);
  gtk_widget_class_bind_child (widget_class, GladeProjectPropertiesPrivate, resource_fullpath_radio);
  gtk_widget_class_bind_child (widget_class, GladeProjectPropertiesPrivate, relative_path_entry);
  gtk_widget_class_bind_child (widget_class, GladeProjectPropertiesPrivate, full_path_button);
  gtk_widget_class_bind_child (widget_class, GladeProjectPropertiesPrivate, domain_entry);
  gtk_widget_class_bind_child (widget_class, GladeProjectPropertiesPrivate, template_checkbutton);
  gtk_widget_class_bind_child (widget_class, GladeProjectPropertiesPrivate, template_combobox);
  gtk_widget_class_bind_child (widget_class, GladeProjectPropertiesPrivate, toolkit_box);

  /* Declare the callback ports that this widget class exposes, to bind with <signal>
   * connections defined in the GtkBuilder xml
   */
  gtk_widget_class_bind_callback (widget_class, on_template_combo_box_changed);
  gtk_widget_class_bind_callback (widget_class, on_template_checkbutton_toggled);
  gtk_widget_class_bind_callback (widget_class, resource_default_toggled);
  gtk_widget_class_bind_callback (widget_class, resource_relative_toggled);
  gtk_widget_class_bind_callback (widget_class, resource_fullpath_toggled);
  gtk_widget_class_bind_callback (widget_class, resource_path_activated);
  gtk_widget_class_bind_callback (widget_class, resource_full_path_set);
  gtk_widget_class_bind_callback (widget_class, verify_clicked);
  gtk_widget_class_bind_callback (widget_class, on_domain_entry_changed);

  g_type_class_add_private (gobject_class, sizeof (GladeProjectPropertiesPrivate));
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
      gtk_misc_set_alignment (GTK_MISC (label), 0.0F, 0.5F);

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
  
  gtk_widget_set_sensitive (priv->full_path_button, FALSE);
  gtk_widget_set_sensitive (priv->relative_path_entry, FALSE);

  g_signal_handlers_block_by_func (priv->resource_default_radio,
                                   G_CALLBACK (resource_default_toggled),
                                   properties);
  g_signal_handlers_block_by_func (priv->resource_relative_radio,
                                   G_CALLBACK (resource_relative_toggled),
                                   properties);
  g_signal_handlers_block_by_func (priv->resource_fullpath_radio,
                                   G_CALLBACK (resource_fullpath_toggled),
                                   properties);
  g_signal_handlers_block_by_func (priv->relative_path_entry,
                                   G_CALLBACK (resource_path_activated),
                                   properties);

  if (glade_project_get_resource_path (priv->project) == NULL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->resource_default_radio), TRUE);
  else if (glade_project_get_resource_path (priv->project))
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->resource_fullpath_radio), TRUE);
      gtk_widget_set_sensitive (priv->full_path_button, TRUE);
    }
  else
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->resource_relative_radio), TRUE);
      gtk_widget_set_sensitive (priv->relative_path_entry, TRUE);
    }

  gtk_entry_set_text (GTK_ENTRY (priv->relative_path_entry),
		      glade_project_get_resource_path (priv->project) ? 
		      glade_project_get_resource_path (priv->project) : "");

  g_signal_handlers_unblock_by_func (priv->resource_default_radio,
                                     G_CALLBACK (resource_default_toggled),
                                     properties);
  g_signal_handlers_unblock_by_func (priv->resource_relative_radio,
                                     G_CALLBACK (resource_relative_toggled),
                                     properties);
  g_signal_handlers_unblock_by_func (priv->resource_fullpath_radio,
                                     G_CALLBACK (resource_fullpath_toggled),
                                     properties);
  g_signal_handlers_unblock_by_func (priv->relative_path_entry,
                                     G_CALLBACK (resource_path_activated),
                                     properties);
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
  g_signal_connect (priv->project, "targets-changed",
		    G_CALLBACK (project_targets_changed), properties);

  target_version_box_fill (properties);
  update_prefs_for_resource_path (properties);

  project_template_changed (project, NULL, properties);
  project_domain_changed (project, NULL, properties);
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
  GladeTargetableVersion        *version = g_object_get_data (G_OBJECT (widget), "version");
  gchar                         *catalog = g_object_get_data (G_OBJECT (widget), "catalog");

  glade_command_set_project_target (priv->project, catalog, version->major, version->minor);
}

static void
resource_default_toggled (GtkWidget              *widget,
			  GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    return;

  glade_project_set_resource_path (priv->project, NULL);
  gtk_widget_set_sensitive (priv->relative_path_entry, FALSE);
  gtk_widget_set_sensitive (priv->full_path_button, FALSE);
}

static void
resource_relative_toggled (GtkWidget              *widget,
			   GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    return;

  gtk_widget_set_sensitive (priv->relative_path_entry, TRUE);
  gtk_widget_set_sensitive (priv->full_path_button, FALSE);
}

static void
resource_fullpath_toggled (GtkWidget              *widget,
			   GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;

  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    return;

  gtk_widget_set_sensitive (priv->relative_path_entry, FALSE);
  gtk_widget_set_sensitive (priv->full_path_button, TRUE);
}

static void
resource_path_activated (GtkEntry *entry, GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  const gchar *text = gtk_entry_get_text (entry);

  glade_project_set_resource_path (priv->project, text);
}

static void
resource_full_path_set (GtkFileChooserButton *button, GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  gchar *text = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (button));

  glade_project_set_resource_path (priv->project, text);
  g_free (text);
}

static void
on_template_combo_box_changed (GtkComboBox            *combo,
			       GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  GtkTreeIter iter;

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
  gboolean active = gtk_toggle_button_get_active (togglebutton);
  gboolean composite = FALSE;
  
  if (active)
    {
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

  glade_command_set_project_domain (priv->project, gtk_entry_get_text (GTK_ENTRY (entry)));
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
	    g_signal_handlers_block_by_func (G_OBJECT (l->data),
					     G_CALLBACK (target_button_clicked),
					     properties);

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

	  for (l = radios; l; l = l->next)
	    g_signal_handlers_unblock_by_func (G_OBJECT (l->data),
					       G_CALLBACK (target_button_clicked),
					       properties);
	}
    }
}

static void
project_domain_changed (GladeProject           *project,
			GParamSpec             *pspec,
			GladeProjectProperties *properties)
{
  GladeProjectPropertiesPrivate *priv = properties->priv;
  const gchar *domain;

  domain = glade_project_get_translation_domain (priv->project);

  g_signal_handlers_block_by_func (priv->domain_entry, on_domain_entry_changed, properties);
  gtk_entry_set_text (GTK_ENTRY (priv->domain_entry), domain ? domain : "");
  g_signal_handlers_unblock_by_func (priv->domain_entry, on_domain_entry_changed, properties);
}

static void
project_resource_path_changed (GladeProject           *project,
			       GParamSpec             *pspec,
			       GladeProjectProperties *properties)
{
  update_prefs_for_resource_path (properties);
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

  g_signal_handlers_block_by_func (priv->template_combobox, on_template_combo_box_changed, properties);
  g_signal_handlers_block_by_func (priv->template_checkbutton, on_template_checkbutton_toggled, properties);

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

  g_signal_handlers_unblock_by_func (priv->template_combobox, on_template_combo_box_changed, properties);
  g_signal_handlers_unblock_by_func (priv->template_checkbutton, on_template_checkbutton_toggled, properties);
}

/******************************************************
 *                         API                        *
 ******************************************************/
GtkWidget *
glade_project_properties_new (GladeProject *project)
{
  return g_object_new (GLADE_TYPE_PROJECT_PROPERTIES, "project", project, NULL);
}
