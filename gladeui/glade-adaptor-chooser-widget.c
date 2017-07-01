/*
 * glade-adaptor-chooser-widget.c
 *
 * Copyright (C) 2014-2017 Juan Pablo Ugarte
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
#include "glade-adaptor-chooser-widget.h"

#include <string.h>

enum
{
  COLUMN_ADAPTOR = 0,
  COLUMN_GROUP,
  COLUMN_NORMALIZED_NAME,
  COLUMN_NORMALIZED_NAME_LEN,
  N_COLUMN
};

struct _GladeAdaptorChooserWidgetPrivate
{
  GtkTreeView        *treeview;
  GtkListStore       *store;
  GtkTreeModelFilter *treemodelfilter;
  GtkSearchEntry     *searchentry;
  GtkEntryCompletion *entrycompletion;
  GtkScrolledWindow  *scrolledwindow;

  /* Needed for gtk_tree_view_column_set_cell_data_func() */
  GtkTreeViewColumn *column_icon;
  GtkCellRenderer   *icon_cell;
  GtkTreeViewColumn *column_adaptor;
  GtkCellRenderer   *adaptor_cell;

  /* Properties */
  _GladeAdaptorChooserWidgetFlags flags;
  GladeProject *project;
  gboolean show_group_title;
  gchar *search_text;
};

enum
{
  PROP_0,

  PROP_SHOW_FLAGS,
  PROP_PROJECT,
  PROP_SHOW_GROUP_TITLE
};

enum
{
  ADAPTOR_SELECTED,

  LAST_SIGNAL
};

static guint adaptor_chooser_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (_GladeAdaptorChooserWidget, _glade_adaptor_chooser_widget, GTK_TYPE_BOX);

#define GET_PRIVATE(d) ((_GladeAdaptorChooserWidgetPrivate *) _glade_adaptor_chooser_widget_get_instance_private((_GladeAdaptorChooserWidget*)d))

static void
_glade_adaptor_chooser_widget_init (_GladeAdaptorChooserWidget *chooser)
{
  gtk_widget_init_template (GTK_WIDGET (chooser));
}

static void
_glade_adaptor_chooser_widget_finalize (GObject *object)
{
  _GladeAdaptorChooserWidgetPrivate *priv = GET_PRIVATE (object);

  g_clear_pointer (&priv->search_text, g_free);
  g_clear_object (&priv->project);

  G_OBJECT_CLASS (_glade_adaptor_chooser_widget_parent_class)->finalize (object);
}

static void
_glade_adaptor_chooser_widget_set_property (GObject      *object, 
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{ 
  _GladeAdaptorChooserWidgetPrivate *priv;
  
  g_return_if_fail (GLADE_IS_ADAPTOR_CHOOSER_WIDGET (object));

  priv = GET_PRIVATE (object);

  switch (prop_id)
    {
      case PROP_SHOW_FLAGS:
        priv->flags = g_value_get_flags (value);
      break;
      case PROP_PROJECT:
        _glade_adaptor_chooser_widget_set_project (GLADE_ADAPTOR_CHOOSER_WIDGET (object),
                                                   g_value_get_object (value));
      break;
      case PROP_SHOW_GROUP_TITLE:
        priv->show_group_title = g_value_get_boolean (value);
      break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_glade_adaptor_chooser_widget_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  _GladeAdaptorChooserWidgetPrivate *priv;

  g_return_if_fail (GLADE_IS_ADAPTOR_CHOOSER_WIDGET (object));

  priv = GET_PRIVATE (object);

  switch (prop_id)
    {
      case PROP_SHOW_FLAGS:
        g_value_set_flags (value, priv->flags);
      break;
      case PROP_PROJECT:
        g_value_set_object (value, priv->project);
      break;
      case PROP_SHOW_GROUP_TITLE:
        g_value_set_boolean (value, priv->show_group_title);
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

static void
on_treeview_row_activated (GtkTreeView                *tree_view,
                           GtkTreePath                *path,
                           GtkTreeViewColumn          *column,
                           _GladeAdaptorChooserWidget *chooser)
{
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view); 
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      GladeWidgetAdaptor *adaptor;

      gtk_tree_model_get (model, &iter, COLUMN_ADAPTOR, &adaptor, -1);

      if (!adaptor)
        return;

      /* Emit selected signal */
      g_signal_emit (chooser, adaptor_chooser_signals[ADAPTOR_SELECTED], 0, adaptor);

      g_object_unref (adaptor);
    }
}

static void
on_searchentry_search_changed (GtkEntry                   *entry,
                               _GladeAdaptorChooserWidget *chooser)
{
  _GladeAdaptorChooserWidgetPrivate *priv = GET_PRIVATE (chooser);
  const gchar *text = gtk_entry_get_text (entry);

  g_clear_pointer (&priv->search_text, g_free);

  if (g_utf8_strlen (text, -1))
    priv->search_text = normalize_name (text);

  gtk_tree_model_filter_refilter (priv->treemodelfilter);
}

static void
on_searchentry_activate (GtkEntry *entry, _GladeAdaptorChooserWidget *chooser)
{
  _GladeAdaptorChooserWidgetPrivate *priv = GET_PRIVATE (chooser);
  const gchar *text = gtk_entry_get_text (entry);
  GladeWidgetAdaptor *adaptor;

  /* try to find an adaptor by name */
  if (!(adaptor = glade_widget_adaptor_get_by_name (text)))
    {
      GtkTreeModel *model = GTK_TREE_MODEL (priv->treemodelfilter);
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
chooser_match_func (_GladeAdaptorChooserWidget *chooser,
                    GtkTreeModel               *model,
                    const gchar                *key,
                    GtkTreeIter                *iter)
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
  if (!name)
    return FALSE;

  visible = (g_strstr_len (name, name_len, key) != NULL);
  g_free (name);

  return visible;
}

static gboolean
treemodelfilter_visible_func (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  _GladeAdaptorChooserWidgetPrivate *priv = GET_PRIVATE (data);
  GladeWidgetAdaptor *adaptor = NULL;
  gboolean visible = TRUE;

  gtk_tree_model_get (model, iter, COLUMN_ADAPTOR, &adaptor, -1);

  if (!adaptor)
    return priv->show_group_title && !priv->search_text;

  /* Skip classes not available in project target version */
  if (priv->project)
    {
      const gchar *catalog = NULL;
      gint major, minor;

      catalog = glade_widget_adaptor_get_catalog (adaptor);
      glade_project_get_target_version (priv->project, catalog, &major, &minor);

      visible = GWA_VERSION_CHECK (adaptor, major, minor);
    }

  if (visible && priv->flags)
    {
      GType type = glade_widget_adaptor_get_object_type (adaptor);
      _GladeAdaptorChooserWidgetFlags flags = priv->flags;

       /* Skip adaptors according to flags */
       if (flags & GLADE_ADAPTOR_CHOOSER_WIDGET_SKIP_DEPRECATED && GWA_DEPRECATED (adaptor))
         visible = FALSE;
       else if (flags & GLADE_ADAPTOR_CHOOSER_WIDGET_SKIP_TOPLEVEL && GWA_IS_TOPLEVEL (adaptor))
         visible = FALSE;
       else if (flags & GLADE_ADAPTOR_CHOOSER_WIDGET_WIDGET && !g_type_is_a (type, GTK_TYPE_WIDGET))
         visible = FALSE;
       else if (flags & GLADE_ADAPTOR_CHOOSER_WIDGET_TOPLEVEL && !GWA_IS_TOPLEVEL (adaptor))
         visible = FALSE;
    }

  if (visible && priv->search_text)
    visible = chooser_match_func (data, model, priv->search_text, iter);

  g_clear_object (&adaptor);

  return visible;
}

static gboolean
entrycompletion_match_func (GtkEntryCompletion *entry, const gchar *key, GtkTreeIter *iter, gpointer data)
{
  return chooser_match_func (data, gtk_entry_completion_get_model (entry), key, iter);
}

static void
adaptor_icon_cell_data_func (GtkTreeViewColumn *tree_column,
                             GtkCellRenderer   *cell,
                             GtkTreeModel      *tree_model,
                             GtkTreeIter       *iter,
                             gpointer           data)
{
  GladeWidgetAdaptor *adaptor;
  gtk_tree_model_get (tree_model, iter, COLUMN_ADAPTOR, &adaptor, -1);

  if (adaptor)
    g_object_set (cell, "sensitive", TRUE, "icon-name", glade_widget_adaptor_get_icon_name (adaptor), NULL);
  else
    g_object_set (cell, "sensitive", FALSE, "icon-name", "go-down-symbolic", NULL);

  g_clear_object (&adaptor);
}

static void
adaptor_text_cell_data_func (GtkTreeViewColumn *tree_column,
                             GtkCellRenderer   *cell,
                             GtkTreeModel      *tree_model,
                             GtkTreeIter       *iter,
                             gpointer           data)
{
  GladeWidgetAdaptor *adaptor;
  gchar *group;

  gtk_tree_model_get (tree_model, iter,
                      COLUMN_ADAPTOR, &adaptor,
                      COLUMN_GROUP, &group,
                      -1);
  if (adaptor)
    g_object_set (cell,
                  "sensitive", TRUE,
                  "text", glade_widget_adaptor_get_name (adaptor),
                  "style", PANGO_STYLE_NORMAL,
                  NULL);
  else
    g_object_set (cell,
                  "sensitive", FALSE,
                  "text", group,
                  "style", PANGO_STYLE_ITALIC,
                  NULL);

  g_clear_object (&adaptor);
  g_free (group);
}

static void
_glade_adaptor_chooser_widget_constructed (GObject *object)
{
  _GladeAdaptorChooserWidget *chooser = GLADE_ADAPTOR_CHOOSER_WIDGET (object);
  _GladeAdaptorChooserWidgetPrivate *priv = GET_PRIVATE (chooser);

  /* Set cell data function: this save us from alocating name and icon name for each adaptor. */
  gtk_tree_view_column_set_cell_data_func (priv->column_icon,
                                           priv->icon_cell,
                                           adaptor_icon_cell_data_func,
                                           NULL, NULL);
  gtk_tree_view_column_set_cell_data_func (priv->column_adaptor,
                                           priv->adaptor_cell,
                                           adaptor_text_cell_data_func,
                                           NULL, NULL);
  /* Set tree model filter function */
  gtk_tree_model_filter_set_visible_func (priv->treemodelfilter,
                                          treemodelfilter_visible_func,
                                          chooser, NULL);
  /* Set completion match function */
  gtk_entry_completion_set_match_func (priv->entrycompletion,
                                       entrycompletion_match_func,
                                       chooser, NULL);
}

static void
_glade_adaptor_chooser_widget_map (GtkWidget *widget)
{
  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);

  if (toplevel)
    {
      _GladeAdaptorChooserWidgetPrivate *priv = GET_PRIVATE (widget);
      gint height = gtk_widget_get_allocated_height (toplevel) - 100;

      if (height > 512)
        height = height * 0.75;

      gtk_scrolled_window_set_max_content_height (priv->scrolledwindow, height);
    }

  GTK_WIDGET_CLASS (_glade_adaptor_chooser_widget_parent_class)->map (widget);
}

static GType
_glade_adaptor_chooser_widget_flags_get_type (void)
{
    static GType etype = 0;
    if (G_UNLIKELY(etype == 0)) {
        static const GFlagsValue values[] = {
            { GLADE_ADAPTOR_CHOOSER_WIDGET_WIDGET, "GLADE_ADAPTOR_CHOOSER_WIDGET_WIDGET", "widget" },
            { GLADE_ADAPTOR_CHOOSER_WIDGET_TOPLEVEL, "GLADE_ADAPTOR_CHOOSER_WIDGET_TOPLEVEL", "toplevel" },
            { GLADE_ADAPTOR_CHOOSER_WIDGET_SKIP_TOPLEVEL, "GLADE_ADAPTOR_CHOOSER_WIDGET_SKIP_TOPLEVEL", "skip-toplevel" },
            { GLADE_ADAPTOR_CHOOSER_WIDGET_SKIP_DEPRECATED, "GLADE_ADAPTOR_CHOOSER_WIDGET_SKIP_DEPRECATED", "skip-deprecated" },
            { 0, NULL, NULL }
        };
        etype = g_flags_register_static (g_intern_static_string ("_GladeAdaptorChooserWidgetFlag"), values);
    }
    return etype;
}

static void
_glade_adaptor_chooser_widget_class_init (_GladeAdaptorChooserWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = _glade_adaptor_chooser_widget_finalize;
  object_class->set_property = _glade_adaptor_chooser_widget_set_property;
  object_class->get_property = _glade_adaptor_chooser_widget_get_property;
  object_class->constructed = _glade_adaptor_chooser_widget_constructed;

  widget_class->map = _glade_adaptor_chooser_widget_map;

  g_object_class_install_property (object_class,
                                   PROP_SHOW_FLAGS,
                                   g_param_spec_flags ("show-flags",
                                                       "Show flags",
                                                       "Widget adaptors show flags",
                                                       _glade_adaptor_chooser_widget_flags_get_type (),
                                                       0,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   PROP_SHOW_GROUP_TITLE,
                                   g_param_spec_boolean ("show-group-title",
                                                         "Show group title",
                                                         "Whether to show the group title",
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   PROP_PROJECT,
                                   g_param_spec_object ("project",
                                                        "Glade Project",
                                                        "If set, use project target version to skip unsupported classes",
                                                        GLADE_TYPE_PROJECT,
                                                        G_PARAM_READWRITE));

  adaptor_chooser_signals[ADAPTOR_SELECTED] =
    g_signal_new ("adaptor-selected", G_OBJECT_CLASS_TYPE (klass), 0, 0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  GLADE_TYPE_WIDGET_ADAPTOR);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/gladeui/glade-adaptor-chooser-widget.ui");
  gtk_widget_class_bind_template_child_private (widget_class, _GladeAdaptorChooserWidget, treeview);
  gtk_widget_class_bind_template_child_private (widget_class, _GladeAdaptorChooserWidget, store);
  gtk_widget_class_bind_template_child_private (widget_class, _GladeAdaptorChooserWidget, treemodelfilter);
  gtk_widget_class_bind_template_child_private (widget_class, _GladeAdaptorChooserWidget, searchentry);
  gtk_widget_class_bind_template_child_private (widget_class, _GladeAdaptorChooserWidget, entrycompletion);
  gtk_widget_class_bind_template_child_private (widget_class, _GladeAdaptorChooserWidget, column_icon);
  gtk_widget_class_bind_template_child_private (widget_class, _GladeAdaptorChooserWidget, icon_cell);
  gtk_widget_class_bind_template_child_private (widget_class, _GladeAdaptorChooserWidget, column_adaptor);
  gtk_widget_class_bind_template_child_private (widget_class, _GladeAdaptorChooserWidget, adaptor_cell);
  gtk_widget_class_bind_template_child_private (widget_class, _GladeAdaptorChooserWidget, scrolledwindow);
  gtk_widget_class_bind_template_callback (widget_class, on_treeview_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_searchentry_search_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_searchentry_activate);
}

GtkWidget *
_glade_adaptor_chooser_widget_new (_GladeAdaptorChooserWidgetFlags flags, GladeProject *project)
{
  return GTK_WIDGET (g_object_new (GLADE_TYPE_ADAPTOR_CHOOSER_WIDGET,
                                   "show-flags", flags,
                                   "project", project,
                                   NULL));
}

void
_glade_adaptor_chooser_widget_set_project (_GladeAdaptorChooserWidget *chooser,
                                           GladeProject               *project)
{
  _GladeAdaptorChooserWidgetPrivate *priv;

  g_return_if_fail (GLADE_IS_ADAPTOR_CHOOSER_WIDGET (chooser));
  priv = GET_PRIVATE (chooser);

  g_clear_object (&priv->project);

  if (project)
    priv->project = g_object_ref (project);

  gtk_tree_model_filter_refilter (priv->treemodelfilter);
}

void
_glade_adaptor_chooser_widget_populate (_GladeAdaptorChooserWidget *chooser)
{
  GList *l;

  for (l = glade_app_get_catalogs (); l; l = g_list_next (l))
    _glade_adaptor_chooser_widget_add_catalog (chooser, l->data);
}

void
_glade_adaptor_chooser_widget_add_catalog (_GladeAdaptorChooserWidget *chooser,
                                           GladeCatalog               *catalog)
{
  GList *groups;

  g_return_if_fail (GLADE_IS_ADAPTOR_CHOOSER_WIDGET (chooser));

  for (groups = glade_catalog_get_widget_groups (catalog); groups;
       groups = g_list_next (groups))
    _glade_adaptor_chooser_widget_add_group (chooser, groups->data);
}

void
_glade_adaptor_chooser_widget_add_group (_GladeAdaptorChooserWidget *chooser,
                                         GladeWidgetGroup           *group)
{
  _GladeAdaptorChooserWidgetPrivate *priv;
  const GList *adaptors;

  g_return_if_fail (GLADE_IS_ADAPTOR_CHOOSER_WIDGET (chooser));
  priv = GET_PRIVATE (chooser);

  if (priv->show_group_title)
    gtk_list_store_insert_with_values (priv->store, NULL, -1,
                                       COLUMN_GROUP, glade_widget_group_get_title (group),
                                       -1);

  for (adaptors = glade_widget_group_get_adaptors (group); adaptors;
       adaptors = g_list_next (adaptors))
    store_append_adaptor (priv->store, adaptors->data);
}

