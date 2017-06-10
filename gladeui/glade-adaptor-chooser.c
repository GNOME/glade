/*
 * glade-adaptor-chooser.c
 *
 * Copyright (C) 2014 Juan Pablo Ugarte
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

#include "glade-app.h"
#include "gladeui-enum-types.h"
#include "glade-adaptor-chooser.h"

#include <string.h>

enum
{
  COLUMN_ADAPTOR = 0,
  COLUMN_NORMALIZED_NAME,
  COLUMN_NORMALIZED_NAME_LEN,
  N_COLUMN
};

struct _GladeAdaptorChooserPrivate
{
  GtkListStore       *store;
  GtkTreeModelFilter *treemodelfilter;
  GtkSearchEntry     *searchentry;
  GtkEntryCompletion *entrycompletion;

  /* Needed for gtk_tree_view_column_set_cell_data_func() */
  GtkTreeViewColumn *column_icon;
  GtkCellRenderer   *icon_cell;
  GtkTreeViewColumn *column_adaptor;
  GtkCellRenderer   *adaptor_cell;

  /* Properties */
  _GladeAdaptorChooserFlags flags;
  GladeProject *project;
};

enum
{
  PROP_0,

  PROP_SHOW_FLAGS,
  PROP_PROJECT
};

enum
{
  ADAPTOR_SELECTED,

  LAST_SIGNAL
};

static guint adaptor_chooser_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (_GladeAdaptorChooser, _glade_adaptor_chooser, GTK_TYPE_BOX);

static void
_glade_adaptor_chooser_init (_GladeAdaptorChooser *chooser)
{
  chooser->priv = _glade_adaptor_chooser_get_instance_private (chooser);

  chooser->priv->flags = GLADE_ADAPTOR_CHOOSER_WIDGET;
  gtk_widget_init_template (GTK_WIDGET (chooser));
}

static void
_glade_adaptor_chooser_finalize (GObject *object)
{
  G_OBJECT_CLASS (_glade_adaptor_chooser_parent_class)->finalize (object);
}

static void
_glade_adaptor_chooser_set_property (GObject      *object, 
                                    guint         prop_id, 
                                    const GValue *value,
                                    GParamSpec   *pspec)
{ 
  _GladeAdaptorChooserPrivate *priv;
  
  g_return_if_fail (GLADE_IS_ADAPTOR_CHOOSER (object));

  priv = GLADE_ADAPTOR_CHOOSER (object)->priv;

  switch (prop_id)
    {
      case PROP_SHOW_FLAGS:
        priv->flags = g_value_get_flags (value);
      break;
      case PROP_PROJECT:
        priv->project = g_value_get_object (value);
      break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_glade_adaptor_chooser_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  _GladeAdaptorChooserPrivate *priv;

  g_return_if_fail (GLADE_IS_ADAPTOR_CHOOSER (object));

  priv = GLADE_ADAPTOR_CHOOSER (object)->priv;

  switch (prop_id)
    {
      case PROP_SHOW_FLAGS:
        g_value_set_flags (value, priv->flags);
      break;
      case PROP_PROJECT:
        g_value_set_object (value, priv->project);
      break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static inline gchar *
normalize_name (const gchar *name)
{
  gchar *normalized_name = g_utf8_normalize (name, -1, G_NORMALIZE_DEFAULT);
  gchar *casefold_name = g_utf8_casefold (normalized_name, -1);
  g_free (normalized_name);
  return casefold_name;
}

static inline void
store_append_adaptor (GtkListStore *store, GladeWidgetAdaptor *adaptor)
{
  gchar *normalized_name = normalize_name (glade_widget_adaptor_get_name (adaptor));

  gtk_list_store_insert_with_values (store, NULL, -1,
                                     COLUMN_ADAPTOR, adaptor,
                                     COLUMN_NORMALIZED_NAME, normalized_name,
                                     COLUMN_NORMALIZED_NAME_LEN, strlen (normalized_name),
                                     -1);
  g_free (normalized_name);
}

static inline void
store_populate (GtkListStore            *store,
                GladeProject            *project,
                _GladeAdaptorChooserFlags flags)
{
  const gchar *catalog = NULL;
  gint major, minor;
  GList *l;

  for (l = glade_app_get_catalogs (); l; l = g_list_next (l))
    {
      GList *groups = glade_catalog_get_widget_groups (GLADE_CATALOG (l->data));

      for (; groups; groups = g_list_next (groups))
        {
          GladeWidgetGroup *group = GLADE_WIDGET_GROUP (groups->data);
          const GList *adaptors;

          for (adaptors = glade_widget_group_get_adaptors (group); adaptors;
               adaptors = g_list_next (adaptors))
            {
              GladeWidgetAdaptor *adaptor = adaptors->data;
              GType type = glade_widget_adaptor_get_object_type (adaptor);

              /* Skip deprecated adaptors and according to flags */
              if ((flags & GLADE_ADAPTOR_CHOOSER_SKIP_DEPRECATED && GWA_DEPRECATED (adaptor)) ||
                  (flags & GLADE_ADAPTOR_CHOOSER_SKIP_TOPLEVEL && GWA_IS_TOPLEVEL (adaptor)) ||
                  !((flags & GLADE_ADAPTOR_CHOOSER_WIDGET && g_type_is_a (type, GTK_TYPE_WIDGET)) ||
                    (flags & GLADE_ADAPTOR_CHOOSER_TOPLEVEL && GWA_IS_TOPLEVEL (adaptor))))
                continue;

              /* Skip classes not available in project target version */
              if (project)
                {
                  const gchar *new_catalog = glade_widget_adaptor_get_catalog (adaptor);

                  if (g_strcmp0 (catalog, new_catalog))
                    {
                      catalog = new_catalog;
                      glade_project_get_target_version (project, catalog, &major, &minor);
                    }

                  if (!GWA_VERSION_CHECK (adaptor, major, minor))
                    continue;
                }
              store_append_adaptor (store, adaptor);
            }     
        }
    }
}

static void
on_treeview_row_activated (GtkTreeView         *tree_view,
                           GtkTreePath         *path,
                           GtkTreeViewColumn   *column,
                           _GladeAdaptorChooser *chooser)
{
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view); 
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      GladeWidgetAdaptor *adaptor;

      gtk_tree_model_get (model, &iter, COLUMN_ADAPTOR, &adaptor, -1);

      /* Emit selected signal */
      g_signal_emit (chooser, adaptor_chooser_signals[ADAPTOR_SELECTED], 0, adaptor);

      g_object_unref (adaptor);
    }
}

static void
on_searchentry_activate (GtkEntry *entry, _GladeAdaptorChooser *chooser)
{
  const gchar *text = gtk_entry_get_text (entry);
  GladeWidgetAdaptor *adaptor;

  /* try to find an adaptor by name */
  if (!(adaptor = glade_widget_adaptor_get_by_name (text)))
    {
      GtkTreeModel *model = GTK_TREE_MODEL (chooser->priv->treemodelfilter);
      gchar *normalized_name = normalize_name (text);
      GtkTreeIter iter;
      gboolean valid;
      gint count = 0;

      valid = gtk_tree_model_get_iter_first (model, &iter);

      /* we could not find it check if we can find it by normalized name */
      while (valid)
        {
          gchar *name;

          gtk_tree_model_get (model, &iter, COLUMN_NORMALIZED_NAME, &name, -1);

          if (g_strcmp0 (name, normalized_name) == 0)
            {
              gtk_tree_model_get (model, &iter, COLUMN_ADAPTOR, &adaptor, -1);
              g_free (name);
              break;
            }

          valid = gtk_tree_model_iter_next (model, &iter);
          g_free (name);
          count++;
        }

      /* if not, and there is only one row, then we select that one */
      if (!adaptor && count == 1 && gtk_tree_model_get_iter_first (model, &iter))
        gtk_tree_model_get (model, &iter, COLUMN_ADAPTOR, &adaptor, -1);

      g_free (normalized_name);
    }

  if (adaptor)
    g_signal_emit (chooser, adaptor_chooser_signals[ADAPTOR_SELECTED], 0, adaptor);
}

static gboolean
chooser_match_func (_GladeAdaptorChooser *chooser,
                    GtkTreeModel         *model,
                    const gchar          *key,
                    GtkTreeIter          *iter)
{
  gboolean visible;
  gint name_len;
  gchar *name;

  if (!key || *key == '\0')
    return TRUE;

  gtk_tree_model_get (model, iter,
                      COLUMN_NORMALIZED_NAME, &name,
                      COLUMN_NORMALIZED_NAME_LEN, &name_len,
                      -1);

  visible = (g_strstr_len (name, name_len, key) != NULL);
  g_free (name);

  return visible;
}

static gboolean
treemodelfilter_visible_func (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  _GladeAdaptorChooserPrivate *priv = GLADE_ADAPTOR_CHOOSER (data)->priv;
  gchar *key = normalize_name (gtk_entry_get_text (GTK_ENTRY (priv->searchentry)));
  gboolean visible = chooser_match_func (data, model, key, iter);
  g_free (key);
  return visible;
}

static gboolean
entrycompletion_match_func (GtkEntryCompletion *entry, const gchar *key, GtkTreeIter *iter, gpointer data)
{
  return chooser_match_func (data, gtk_entry_completion_get_model (entry), key, iter);
}

static void
adaptor_cell_data_func (GtkTreeViewColumn *tree_column,
                        GtkCellRenderer   *cell,
                        GtkTreeModel      *tree_model,
                        GtkTreeIter       *iter,
                        gpointer           data)
{
  GladeWidgetAdaptor *adaptor;
  gtk_tree_model_get (tree_model, iter, COLUMN_ADAPTOR, &adaptor, -1);

  if (GPOINTER_TO_SIZE (data))
    g_object_set (cell, "icon-name", glade_widget_adaptor_get_icon_name (adaptor), NULL);
  else
    g_object_set (cell, "text", glade_widget_adaptor_get_name (adaptor), NULL);

  g_object_unref (adaptor);
}

static void
_glade_adaptor_chooser_constructed (GObject *object)
{
  _GladeAdaptorChooser *chooser = GLADE_ADAPTOR_CHOOSER (object);
  _GladeAdaptorChooserPrivate *priv = chooser->priv;

  store_populate (priv->store, priv->project, priv->flags);

  /* Set cell data function: this save us from alocating name and icon name for each adaptor. */
  gtk_tree_view_column_set_cell_data_func (priv->column_icon,
                                           priv->icon_cell,
                                           adaptor_cell_data_func,
                                           GSIZE_TO_POINTER (TRUE),
                                           NULL);
  gtk_tree_view_column_set_cell_data_func (priv->column_adaptor,
                                           priv->adaptor_cell,
                                           adaptor_cell_data_func,
                                           GSIZE_TO_POINTER (FALSE),
                                           NULL);
  /* Set tree model filter function */
  gtk_tree_model_filter_set_visible_func (priv->treemodelfilter,
                                          treemodelfilter_visible_func,
                                          chooser, NULL);
  /* Set completion match function */
  gtk_entry_completion_set_match_func (priv->entrycompletion,
                                       entrycompletion_match_func,
                                       chooser, NULL);
}

static GType
_glade_adaptor_chooser_flags_get_type (void)
{
    static GType etype = 0;
    if (G_UNLIKELY(etype == 0)) {
        static const GFlagsValue values[] = {
            { GLADE_ADAPTOR_CHOOSER_WIDGET, "GLADE_ADAPTOR_CHOOSER_WIDGET", "widget" },
            { GLADE_ADAPTOR_CHOOSER_TOPLEVEL, "GLADE_ADAPTOR_CHOOSER_TOPLEVEL", "toplevel" },
            { GLADE_ADAPTOR_CHOOSER_SKIP_TOPLEVEL, "GLADE_ADAPTOR_CHOOSER_SKIP_TOPLEVEL", "skip-toplevel" },
            { GLADE_ADAPTOR_CHOOSER_SKIP_DEPRECATED, "GLADE_ADAPTOR_CHOOSER_SKIP_DEPRECATED", "skip-deprecated" },
            { 0, NULL, NULL }
        };
        etype = g_flags_register_static (g_intern_static_string ("_GladeAdaptorChooserFlag"), values);
    }
    return etype;
}

static void
_glade_adaptor_chooser_class_init (_GladeAdaptorChooserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = _glade_adaptor_chooser_finalize;
  object_class->set_property = _glade_adaptor_chooser_set_property;
  object_class->get_property = _glade_adaptor_chooser_get_property;
  object_class->constructed = _glade_adaptor_chooser_constructed;

  g_object_class_install_property (object_class,
                                   PROP_SHOW_FLAGS,
                                   g_param_spec_flags ("show-flags",
                                                       "Show flags",
                                                       "Widget adaptors show flags",
                                                       _glade_adaptor_chooser_flags_get_type (),
                                                       GLADE_ADAPTOR_CHOOSER_WIDGET,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   PROP_PROJECT,
                                   g_param_spec_object ("project",
                                                        "Glade Project",
                                                        "If set, use project target version to skip unsupported classes",
                                                        GLADE_TYPE_PROJECT,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  adaptor_chooser_signals[ADAPTOR_SELECTED] =
    g_signal_new ("adaptor-selected", G_OBJECT_CLASS_TYPE (klass), 0, 0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  GLADE_TYPE_WIDGET_ADAPTOR);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladeui/glade-adaptor-chooser.ui");
  gtk_widget_class_bind_template_child_private (widget_class, _GladeAdaptorChooser, store);
  gtk_widget_class_bind_template_child_private (widget_class, _GladeAdaptorChooser, treemodelfilter);
  gtk_widget_class_bind_template_child_private (widget_class, _GladeAdaptorChooser, searchentry);
  gtk_widget_class_bind_template_child_private (widget_class, _GladeAdaptorChooser, entrycompletion);
  gtk_widget_class_bind_template_child_private (widget_class, _GladeAdaptorChooser, column_icon);
  gtk_widget_class_bind_template_child_private (widget_class, _GladeAdaptorChooser, icon_cell);
  gtk_widget_class_bind_template_child_private (widget_class, _GladeAdaptorChooser, column_adaptor);
  gtk_widget_class_bind_template_child_private (widget_class, _GladeAdaptorChooser, adaptor_cell);
  gtk_widget_class_bind_template_callback (widget_class, on_treeview_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_searchentry_activate);
}

GtkWidget *
_glade_adaptor_chooser_new (_GladeAdaptorChooserFlags flags, GladeProject *project)
{
  return GTK_WIDGET (g_object_new (GLADE_TYPE_ADAPTOR_CHOOSER,
                                   "show-flags", flags,
                                   "project", project,
                                   NULL));
}
