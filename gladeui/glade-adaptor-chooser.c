/*
 * glade-adaptor-chooser.c
 *
 * Copyright (C) 2017 Juan Pablo Ugarte
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
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#include "glade-adaptor-chooser-widget.h"
#include "glade-adaptor-chooser.h"
#include "glade-app.h"

typedef struct
{
  GladeProject *project;

  GtkWidget *gtk_button_box;
  GtkWidget *extra_button;
  GtkWidget *others_button;
  GtkImage *class_image;
  GtkLabel *class_label;
  GtkWidget *all_button;

  GList *choosers;
} GladeAdaptorChooserPrivate;

struct _GladeAdaptorChooser
{
  GtkBox parent_instance;
};

enum
{
  PROP_0,
  PROP_PROJECT,

  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

G_DEFINE_TYPE_WITH_PRIVATE (GladeAdaptorChooser,
                            glade_adaptor_chooser,
                            GTK_TYPE_BOX);

#define GET_PRIVATE(d) ((GladeAdaptorChooserPrivate *) glade_adaptor_chooser_get_instance_private((GladeAdaptorChooser*)d))

static void
glade_adaptor_chooser_init (GladeAdaptorChooser *chooser)
{
  gtk_widget_init_template (GTK_WIDGET (chooser));
}

static void
glade_adaptor_chooser_finalize (GObject *object)
{
  GladeAdaptorChooserPrivate *priv = GET_PRIVATE (object);

  g_list_free (priv->choosers);

  G_OBJECT_CLASS (glade_adaptor_chooser_parent_class)->finalize (object);
}

static void
glade_adaptor_chooser_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  g_return_if_fail (GLADE_IS_ADAPTOR_CHOOSER (object));

  switch (prop_id)
    {
      case PROP_PROJECT:
        glade_adaptor_chooser_set_project (GLADE_ADAPTOR_CHOOSER (object),
                                               g_value_get_object (value));
      break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glade_adaptor_chooser_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GladeAdaptorChooserPrivate *priv;

  g_return_if_fail (GLADE_IS_ADAPTOR_CHOOSER (object));
  priv = GET_PRIVATE (object);

  switch (prop_id)
    {
      case PROP_PROJECT:
        g_value_set_object (value, priv->project);
      break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
on_adaptor_selected (GtkWidget           *widget,
                     GladeWidgetAdaptor  *adaptor,
                     GladeAdaptorChooser *chooser)
{
  GladeAdaptorChooserPrivate *priv = GET_PRIVATE (chooser);

  /* Auto-create toplevel types */
  if (GWA_IS_TOPLEVEL (adaptor))
    {
      glade_command_create (adaptor, NULL, NULL, priv->project);
    }
  else
    {
      glade_project_set_add_item (priv->project, adaptor);
      glade_project_set_pointer_mode (priv->project, GLADE_POINTER_ADD_WIDGET);
    }

  gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_parent (widget)));
}

static void
glade_adaptor_chooser_button_add_chooser (GtkWidget *button, GtkWidget *chooser)
{
  GtkWidget *popover;

  popover = gtk_popover_new (button);
  gtk_container_add (GTK_CONTAINER (popover), chooser);
  gtk_widget_show (chooser);

  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), popover);
}

static GtkWidget *
glade_adaptor_chooser_add_chooser (GladeAdaptorChooser *chooser,
                                   gboolean             show_group_title)
{
  GladeAdaptorChooserPrivate *priv = GET_PRIVATE (chooser);
  GtkWidget *chooser_widget = g_object_new (GLADE_TYPE_ADAPTOR_CHOOSER_WIDGET,
                                            "show-group-title", show_group_title,
                                            NULL);

  priv->choosers = g_list_prepend (priv->choosers, chooser_widget);
  g_signal_connect (chooser_widget, "adaptor-selected",
                    G_CALLBACK (on_adaptor_selected),
                    chooser);

  return chooser_widget;
}

static void
button_box_populate_from_catalog (GladeAdaptorChooser *chooser,
                                  GladeCatalog        *catalog)
{
  GladeAdaptorChooserPrivate *priv = GET_PRIVATE (chooser);
  GtkWidget *extra_chooser = NULL;
  GList *groups;
    
  groups = glade_catalog_get_widget_groups (catalog);
  gtk_box_set_homogeneous (GTK_BOX (priv->gtk_button_box), FALSE);
          
  for (; groups; groups = g_list_next (groups))
    {
      GladeWidgetGroup *group = GLADE_WIDGET_GROUP (groups->data);

      if (!glade_widget_group_get_adaptors (group))
        continue;

      if (glade_widget_group_get_expanded (group))
        {
          GtkWidget *button, *chooser_widget;
          
          chooser_widget = glade_adaptor_chooser_add_chooser (chooser, FALSE);
          button = gtk_menu_button_new ();
          gtk_button_set_label (GTK_BUTTON (button), glade_widget_group_get_title (group));
          glade_adaptor_chooser_button_add_chooser (button, chooser_widget);
          _glade_adaptor_chooser_widget_add_group (GLADE_ADAPTOR_CHOOSER_WIDGET (chooser_widget), group);
          gtk_box_pack_start (GTK_BOX (priv->gtk_button_box), button, FALSE, FALSE, 0);
          gtk_widget_show (button);
        }
      else
        {
          if (!extra_chooser)
            {
              extra_chooser = glade_adaptor_chooser_add_chooser (chooser, TRUE);
              glade_adaptor_chooser_button_add_chooser (priv->extra_button, extra_chooser);
              gtk_widget_show (priv->extra_button);
            }

          _glade_adaptor_chooser_widget_add_group (GLADE_ADAPTOR_CHOOSER_WIDGET (extra_chooser), group);
        }
    }
}

static void
glade_adaptor_chooser_constructed (GObject *object)
{
  GladeAdaptorChooser *chooser = GLADE_ADAPTOR_CHOOSER (object);
  GladeAdaptorChooserPrivate *priv = GET_PRIVATE (object);
  GtkWidget *others_chooser, *all_chooser;
  GladeCatalog *gtk_catalog;
  GList *l;

  /* GTK+ catalog goes first subdivided by group */
  gtk_catalog = glade_app_get_catalog ("gtk+");
  button_box_populate_from_catalog (chooser, gtk_catalog);

  others_chooser = glade_adaptor_chooser_add_chooser (chooser, TRUE);
  all_chooser = glade_adaptor_chooser_add_chooser (chooser, TRUE);
  glade_adaptor_chooser_button_add_chooser (priv->others_button, others_chooser);
  glade_adaptor_chooser_button_add_chooser (priv->all_button, all_chooser);

  /* then the rest */
  for (l = glade_app_get_catalogs (); l; l = g_list_next (l))
    {
      GladeCatalog *catalog = l->data;

      _glade_adaptor_chooser_widget_add_catalog (GLADE_ADAPTOR_CHOOSER_WIDGET (all_chooser), catalog);

      if (catalog != gtk_catalog)
        _glade_adaptor_chooser_widget_add_catalog (GLADE_ADAPTOR_CHOOSER_WIDGET (others_chooser), catalog);
    }
}

static void
glade_adaptor_chooser_class_init (GladeAdaptorChooserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = glade_adaptor_chooser_finalize;
  object_class->constructed = glade_adaptor_chooser_constructed;
  object_class->set_property = glade_adaptor_chooser_set_property;
  object_class->get_property = glade_adaptor_chooser_get_property;

  /* Properties */
  properties[PROP_PROJECT] =
    g_param_spec_object ("project", "Project",
                         "This adaptor chooser's current project",
                         GLADE_TYPE_PROJECT,
                         G_PARAM_READWRITE);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
  
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladeui/glade-adaptor-chooser.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GladeAdaptorChooser, gtk_button_box);
  gtk_widget_class_bind_template_child_private (widget_class, GladeAdaptorChooser, extra_button);
  gtk_widget_class_bind_template_child_private (widget_class, GladeAdaptorChooser, others_button);
  gtk_widget_class_bind_template_child_private (widget_class, GladeAdaptorChooser, class_image);
  gtk_widget_class_bind_template_child_private (widget_class, GladeAdaptorChooser, class_label);
  gtk_widget_class_bind_template_child_private (widget_class, GladeAdaptorChooser, all_button);
}

/* Public API */

GtkWidget *
glade_adaptor_chooser_new ()
{
  return (GtkWidget*) g_object_new (GLADE_TYPE_ADAPTOR_CHOOSER, NULL);
}

static void
glade_adaptor_chooser_update_adaptor (GladeAdaptorChooser *chooser)
{
  GladeAdaptorChooserPrivate *priv = GET_PRIVATE (chooser);
  GladeWidgetAdaptor *adaptor;
  
  if (priv->project && (adaptor = glade_project_get_add_item (priv->project)))
    {
      gtk_image_set_from_icon_name (priv->class_image,
                                    glade_widget_adaptor_get_icon_name (adaptor),
                                    GTK_ICON_SIZE_BUTTON);
      gtk_label_set_label (priv->class_label,
                           glade_widget_adaptor_get_name (adaptor));
    }
  else
    {
      gtk_image_set_from_pixbuf (priv->class_image, NULL);
      gtk_label_set_label (priv->class_label, "");
    }
}

void
glade_adaptor_chooser_set_project (GladeAdaptorChooser *chooser,
                                   GladeProject        *project)
{
  GladeAdaptorChooserPrivate *priv;
  GList *l;

  g_return_if_fail (GLADE_IS_ADAPTOR_CHOOSER (chooser));
  priv = GET_PRIVATE (chooser);

  if (priv->project)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (priv->project),
                                            G_CALLBACK (glade_adaptor_chooser_update_adaptor),
                                            chooser);
      g_clear_object (&priv->project);
    }

  if (project)
    {
      priv->project = g_object_ref (project);
      g_signal_connect_swapped (G_OBJECT (project), "notify::add-item",
                                G_CALLBACK (glade_adaptor_chooser_update_adaptor),
                                chooser);
      gtk_widget_set_sensitive (GTK_WIDGET (chooser), TRUE);
    }
  else
    gtk_widget_set_sensitive (GTK_WIDGET (chooser), FALSE);

  /* Set project in chooser for filter to work */
  for (l = priv->choosers; l; l = g_list_next (l))
    _glade_adaptor_chooser_widget_set_project (l->data, project);

  /* Update class image and label */
  glade_adaptor_chooser_update_adaptor (chooser);
}

GladeProject *
glade_adaptor_chooser_get_project (GladeAdaptorChooser *chooser)
{
  g_return_val_if_fail (GLADE_IS_ADAPTOR_CHOOSER (chooser), NULL);
  
  return GET_PRIVATE (chooser)->project;
}
