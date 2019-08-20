/*
 * glade-palette.c
 *
 * Copyright (C) 2006 The GNOME Foundation.
 * Copyright (C) 2001-2005 Ximian, Inc.
 *
 * Authors:
 *   Chema Celorio
 *   Joaquin Cuenca Abela <e98cuenc@yahoo.com>
 *   Vincent Geddes <vgeddes@metroweb.co.za>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 * SECTION:glade-palette
 * @Short_Description: A widget to select a #GladeWidgetAdaptor for addition.
 *
 * #GladePalette is responsible for displaying the list of available
 * #GladeWidgetAdaptor types and publishing the currently selected class
 * to the Glade core.
 */

#include "glade.h"
#include "gladeui-enum-types.h"
#include "glade-app.h"
#include "glade-palette.h"
#include "glade-catalog.h"
#include "glade-project.h"
#include "glade-widget.h"
#include "glade-widget-adaptor.h"
#include "glade-popup.h"
#include "glade-design-private.h"
#include "glade-dnd.h"

#include <glib/gi18n-lib.h>
#include <gdk/gdk.h>

struct _GladePalettePrivate
{
  const GList *catalogs;        /* List of widget catalogs */

  GladeProject *project;

  GtkWidget *selector_hbox;
  GtkWidget *selector_button;

  GtkWidget *toolpalette;

  GladeItemAppearance item_appearance;
  gboolean            use_small_item_icons;

  GladeWidgetAdaptor *local_selection;
  GHashTable         *button_table;
};

enum
{
  REFRESH,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_ITEM_APPEARANCE,
  PROP_USE_SMALL_ITEM_ICONS,
  PROP_SHOW_SELECTOR_BUTTON,
  PROP_PROJECT,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];
static guint glade_palette_signals[LAST_SIGNAL] = { 0 };

static void glade_palette_append_item_group (GladePalette        *palette,
                                             GladeWidgetGroup    *group);
static void palette_item_toggled_cb         (GtkToggleToolButton *button, 
                                             GladePalette        *palette);

G_DEFINE_TYPE_WITH_PRIVATE (GladePalette, glade_palette, GTK_TYPE_BOX)


/*******************************************************
 *                   Project Signals                   *
 *******************************************************/
static void
palette_item_refresh_cb (GladePalette *palette,
                         GtkWidget    *item)
{
  GladeProject       *project;
  GladeSupportMask    support;
  GladeWidgetAdaptor *adaptor;
  gchar              *warning, *text;

  adaptor = g_object_get_data (G_OBJECT (item), "glade-widget-adaptor");
  g_assert (adaptor);

  if ((project = palette->priv->project) &&
      (warning = glade_project_verify_widget_adaptor (project, adaptor,
                                                      &support)) != NULL)
    {
      /* set sensitivity */
      gtk_widget_set_sensitive (GTK_WIDGET (item),
                                !(support & GLADE_SUPPORT_MISMATCH));

      gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item),
                                     glade_widget_adaptor_get_icon_name (adaptor));

      /* prepend widget title */
      text = g_strdup_printf ("%s: %s", glade_widget_adaptor_get_title (adaptor), warning);
      gtk_widget_set_tooltip_text (item, text);
      g_free (text);
      g_free (warning);
    }
  else
    {
      gtk_widget_set_tooltip_text (GTK_WIDGET (item), 
                                   glade_widget_adaptor_get_title (adaptor));
      gtk_widget_set_sensitive (GTK_WIDGET (item), TRUE);
      gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item),
                                     glade_widget_adaptor_get_icon_name (adaptor));
    }
}

static void
glade_palette_refresh (GladePalette *palette)
{
  g_return_if_fail (GLADE_IS_PALETTE (palette));

  g_signal_emit (G_OBJECT (palette), glade_palette_signals[REFRESH], 0);
}

static void
project_add_item_changed_cb (GladeProject *project,
                             GParamSpec   *pspec,
                             GladePalette *palette)
{
  GtkToggleToolButton *selection = NULL;
  GladePalettePrivate *priv = palette->priv;

  if (priv->local_selection)
    {
      selection = g_hash_table_lookup (priv->button_table, 
                                       glade_widget_adaptor_get_name (priv->local_selection));

      g_signal_handlers_block_by_func (selection, palette_item_toggled_cb, palette);
      gtk_toggle_tool_button_set_active (selection, FALSE);
      g_signal_handlers_unblock_by_func (selection, palette_item_toggled_cb, palette);

      glade_project_set_pointer_mode (priv->project, GLADE_POINTER_SELECT);
    }

  priv->local_selection = glade_project_get_add_item (priv->project);

  if (priv->local_selection)
    {
      selection = g_hash_table_lookup (priv->button_table, 
                                       glade_widget_adaptor_get_name (priv->local_selection));

      g_signal_handlers_block_by_func (selection, palette_item_toggled_cb, palette);
      gtk_toggle_tool_button_set_active (selection, TRUE);
      g_signal_handlers_unblock_by_func (selection, palette_item_toggled_cb, palette);

      glade_project_set_pointer_mode (priv->project, GLADE_POINTER_ADD_WIDGET);
    }
}

/*******************************************************
 *                    Local Signals                    *
 *******************************************************/
static void
selector_button_toggled_cb (GtkToggleButton *button,
                            GladePalette    *palette)
{
  GladePalettePrivate *priv = palette->priv;

  if (!priv->project)
    return;

  if (gtk_toggle_button_get_active (button))
    {
      g_signal_handlers_block_by_func (priv->project, project_add_item_changed_cb, palette);
      glade_project_set_add_item (priv->project, NULL);
      g_signal_handlers_unblock_by_func (priv->project, project_add_item_changed_cb, palette);
    }
  else if (glade_project_get_add_item (priv->project) == NULL)
    gtk_toggle_button_set_active (button, TRUE);
}

static void
palette_item_toggled_cb (GtkToggleToolButton *button, GladePalette *palette)
{
  GladePalettePrivate *priv = palette->priv;
  GladeWidgetAdaptor  *adaptor;
  GtkToggleToolButton *selection = NULL;

  if (!priv->project)
    return;

  adaptor = g_object_get_data (G_OBJECT (button), "glade-widget-adaptor");
  if (!adaptor)
    return;

  /* Start by unselecting the currently selected item if any */
  if (priv->local_selection)
    {
      selection = g_hash_table_lookup (priv->button_table, 
                                       glade_widget_adaptor_get_name (priv->local_selection));

      g_signal_handlers_block_by_func (selection, palette_item_toggled_cb, palette);
      gtk_toggle_tool_button_set_active (selection, FALSE);
      g_signal_handlers_unblock_by_func (selection, palette_item_toggled_cb, palette);
      
      priv->local_selection = NULL;

      g_signal_handlers_block_by_func (priv->project, project_add_item_changed_cb, palette);
      glade_project_set_add_item (priv->project, NULL);
      g_signal_handlers_unblock_by_func (priv->project, project_add_item_changed_cb, palette);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->selector_button), TRUE);

      glade_project_set_pointer_mode (priv->project, GLADE_POINTER_SELECT);
    }

  if (!gtk_toggle_tool_button_get_active (button))
    return;

  /* Auto-create toplevel types */
  if (GWA_IS_TOPLEVEL (adaptor))
    {
      glade_command_create (adaptor, NULL, NULL, priv->project);

      g_signal_handlers_block_by_func (button, palette_item_toggled_cb, palette);
      gtk_toggle_tool_button_set_active (button, FALSE);
      g_signal_handlers_unblock_by_func (button, palette_item_toggled_cb, palette);
    }
  else
    {
      g_signal_handlers_block_by_func (priv->project, project_add_item_changed_cb, palette);
      glade_project_set_add_item (priv->project, adaptor);
      g_signal_handlers_unblock_by_func (priv->project, project_add_item_changed_cb, palette);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->selector_button), FALSE);

      priv->local_selection = adaptor;

      glade_project_set_pointer_mode (priv->project, GLADE_POINTER_ADD_WIDGET);
    }
}

static void
glade_palette_drag_begin (GtkWidget *widget,
                          GdkDragContext *context,
                          GladeWidgetAdaptor *adaptor)
{
  _glade_dnd_set_icon_widget (context,
                              glade_widget_adaptor_get_icon_name (adaptor),
                              glade_widget_adaptor_get_display_name (adaptor));
}

static void
glade_palette_drag_data_get (GtkWidget          *widget,
                             GdkDragContext     *context,
                             GtkSelectionData   *data,
                             guint               info,
                             guint               time,
                             GladeWidgetAdaptor *adaptor)
{
  _glade_dnd_set_data (data, G_OBJECT (adaptor));
}

static gint
palette_item_button_press_cb (GtkWidget      *button,
                              GdkEventButton *event, 
                              GtkToolItem    *item)
{
  GladePalette *palette = g_object_get_data (G_OBJECT (item), "glade-palette");
  GladeWidgetAdaptor *adaptor = g_object_get_data (G_OBJECT (item), "glade-widget-adaptor");
  
  if (glade_popup_is_popup_event (event))
    {
      glade_popup_palette_pop (palette, adaptor, event);
      return TRUE;
    }

  return FALSE;
}

/*******************************************************
 *          Building Widgets/Populating catalog        *
 *******************************************************/
static GtkWidget *
glade_palette_new_item (GladePalette *palette, GladeWidgetAdaptor *adaptor)
{
  GtkWidget *item, *button, *label, *box;

  item = (GtkWidget *) gtk_toggle_tool_button_new ();
  g_object_set_data (G_OBJECT (item), "glade-widget-adaptor", adaptor);
  g_object_set_data (G_OBJECT (item), "glade-palette", palette);

  button = gtk_bin_get_child (GTK_BIN (item));
  g_assert (GTK_IS_BUTTON (button));

  /* Add a box to avoid the ellipsize on the items */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  label = gtk_label_new (glade_widget_adaptor_get_title (adaptor));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_show (label);
  gtk_widget_show (box);
  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_tool_button_set_label_widget (GTK_TOOL_BUTTON (item), box);
  palette_item_refresh_cb (palette, item);

  /* Update selection when the item is pushed */
  g_signal_connect (G_OBJECT (item), "toggled",
                    G_CALLBACK (palette_item_toggled_cb), palette);

  /* Update palette item when active project state changes */
  g_signal_connect (G_OBJECT (palette), "refresh",
                    G_CALLBACK (palette_item_refresh_cb), item);

  /* Fire Glade palette popup menus */
  g_signal_connect (G_OBJECT (button), "button-press-event",
                    G_CALLBACK (palette_item_button_press_cb), item);
  g_signal_connect_object (button, "drag-begin",
                           G_CALLBACK (glade_palette_drag_begin), adaptor, 0);
  g_signal_connect_object (button, "drag-data-get",
                           G_CALLBACK (glade_palette_drag_data_get), adaptor, 0);

  gtk_drag_source_set (button, GDK_BUTTON1_MASK, _glade_dnd_get_target (), 1, GDK_ACTION_MOVE | GDK_ACTION_COPY);

  gtk_widget_show (item);

  g_hash_table_insert (palette->priv->button_table, 
                       (gchar *)glade_widget_adaptor_get_name (adaptor),
                       item);

  return item;
}

static GtkWidget *
glade_palette_new_item_group (GladePalette *palette, GladeWidgetGroup *group)
{
  GtkWidget *item_group, *item, *label;
  GList *l;

  /* Give the item group a left aligned label */
  label = gtk_label_new (glade_widget_group_get_title (group));
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_show (label);

  item_group = gtk_tool_item_group_new ("");
  gtk_tool_item_group_set_label_widget (GTK_TOOL_ITEM_GROUP (item_group),
                                        label);

  /* Tell the item group to ellipsize our custom label for us */
  gtk_tool_item_group_set_ellipsize (GTK_TOOL_ITEM_GROUP (item_group),
                                     PANGO_ELLIPSIZE_END);

  gtk_widget_set_tooltip_text (item_group,
                               glade_widget_group_get_title (group));

  /* Go through all the widget classes in this catalog. */
  for (l = (GList *) glade_widget_group_get_adaptors (group); l; l = l->next)
    {
      GladeWidgetAdaptor *adaptor = GLADE_WIDGET_ADAPTOR (l->data);

      /* Create/append new item */
      item = glade_palette_new_item (palette, adaptor);
      gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (item_group),
                                  GTK_TOOL_ITEM (item), -1);
    }

  /* set default expanded state */
  gtk_tool_item_group_set_collapsed (GTK_TOOL_ITEM_GROUP (item_group),
                                     glade_widget_group_get_expanded (group) == FALSE);

  gtk_widget_show (item_group);

  return item_group;
}

static void
glade_palette_append_item_group (GladePalette *palette, GladeWidgetGroup *group)
{
  GladePalettePrivate *priv = palette->priv;
  GtkWidget *item_group;

  if ((item_group = glade_palette_new_item_group (palette, group)) != NULL)
    gtk_container_add (GTK_CONTAINER (priv->toolpalette), item_group);
}

static void
glade_palette_populate (GladePalette *palette)
{
  GList *l;

  g_return_if_fail (GLADE_IS_PALETTE (palette));

  for (l = (GList *) glade_app_get_catalogs (); l; l = l->next)
    {
      GList *groups = glade_catalog_get_widget_groups (GLADE_CATALOG (l->data));

      for (; groups; groups = groups->next)
        {
          GladeWidgetGroup *group = GLADE_WIDGET_GROUP (groups->data);

          if (glade_widget_group_get_adaptors (group))
            glade_palette_append_item_group (palette, group);
        }
    }
}

static GtkWidget *
glade_palette_create_selector_button (GladePalette *palette)
{
  GtkWidget *selector;
  GtkWidget *image;
  gchar *path;

  /* create selector button */
  selector = gtk_toggle_button_new ();

  gtk_container_set_border_width (GTK_CONTAINER (selector), 0);

  path = g_build_filename (glade_app_get_pixmaps_dir (), "selector.png", NULL);
  image = gtk_image_new_from_file (path);
  gtk_widget_show (image);

  gtk_container_add (GTK_CONTAINER (selector), image);
  gtk_button_set_relief (GTK_BUTTON (selector), GTK_RELIEF_NONE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (selector), TRUE);

  g_signal_connect (G_OBJECT (selector), "toggled",
                    G_CALLBACK (selector_button_toggled_cb), palette);

  g_free (path);

  return selector;
}

/*******************************************************
 *                    Class & methods                  *
 *******************************************************/

/* override GtkWidget::show_all since we have internal widgets we wish to keep
 * hidden unless we decide otherwise, like the hidden selector button.
 */
static void
glade_palette_show_all (GtkWidget *widget)
{
  gtk_widget_show (widget);
}

static void
glade_palette_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GladePalette *palette = GLADE_PALETTE (object);

  switch (prop_id)
    {
    case PROP_PROJECT:
      glade_palette_set_project (palette, (GladeProject *)g_value_get_object (value));
      break;
    case PROP_USE_SMALL_ITEM_ICONS:
      glade_palette_set_use_small_item_icons (palette,
                                              g_value_get_boolean (value));
      break;
    case PROP_ITEM_APPEARANCE:
      glade_palette_set_item_appearance (palette, g_value_get_enum (value));
      break;
    case PROP_SHOW_SELECTOR_BUTTON:
      glade_palette_set_show_selector_button (palette,
                                              g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glade_palette_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value, 
                            GParamSpec *pspec)
{
  GladePalette *palette = GLADE_PALETTE (object);
  GladePalettePrivate *priv = palette->priv;

  switch (prop_id)
    {
    case PROP_PROJECT:
      g_value_set_object (value, priv->project);
      break;
    case PROP_USE_SMALL_ITEM_ICONS:
      g_value_set_boolean (value, priv->use_small_item_icons);
      break;
    case PROP_SHOW_SELECTOR_BUTTON:
      g_value_set_boolean (value,
                           gtk_widget_get_visible (priv->selector_button));
      break;
    case PROP_ITEM_APPEARANCE:
      g_value_set_enum (value, priv->item_appearance);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glade_palette_dispose (GObject *object)
{
  GladePalettePrivate *priv;

  priv = GLADE_PALETTE (object)->priv;

  priv->catalogs = NULL;

  glade_palette_set_project (GLADE_PALETTE (object), NULL);

  G_OBJECT_CLASS (glade_palette_parent_class)->dispose (object);
}

static void
glade_palette_finalize (GObject *object)
{
  GladePalettePrivate *priv;

  priv = GLADE_PALETTE (object)->priv;

  g_hash_table_destroy (priv->button_table);

  G_OBJECT_CLASS (glade_palette_parent_class)->finalize (object);
}

static void
glade_palette_class_init (GladePaletteClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = glade_palette_get_property;
  object_class->set_property = glade_palette_set_property;
  object_class->dispose = glade_palette_dispose;
  object_class->finalize = glade_palette_finalize;

  widget_class->show_all = glade_palette_show_all;

  glade_palette_signals[REFRESH] =
      g_signal_new ("refresh",
                    G_TYPE_FROM_CLASS (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladePaletteClass, refresh),
                    NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  properties[PROP_PROJECT] =
    g_param_spec_object ("project",
                         "Project",
                         "This palette's current project",
                         GLADE_TYPE_PROJECT,
                         G_PARAM_READWRITE);

  properties[PROP_ITEM_APPEARANCE] =
    g_param_spec_enum ("item-appearance",
                       "Item Appearance",
                       "The appearance of the palette items",
                       GLADE_TYPE_ITEM_APPEARANCE,
                       GLADE_ITEM_ICON_ONLY,
                       G_PARAM_READWRITE);

  properties[PROP_USE_SMALL_ITEM_ICONS] =
    g_param_spec_boolean ("use-small-item-icons",
                          "Use Small Item Icons",
                          "Whether to use small icons to represent items",
                          FALSE,
                          G_PARAM_READWRITE);

  properties[PROP_SHOW_SELECTOR_BUTTON] =
    g_param_spec_boolean ("show-selector-button",
                          "Show Selector Button",
                          "Whether to show the internal selector button",
                          TRUE,
                          G_PARAM_READWRITE);

  /* Install all properties */
  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
glade_palette_init (GladePalette *palette)
{
  GladePalettePrivate *priv;
  GtkWidget           *sw;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (palette),
                                  GTK_ORIENTATION_VERTICAL);

  priv = palette->priv = glade_palette_get_instance_private (palette);

  priv->button_table = g_hash_table_new (g_str_hash, g_str_equal);

  priv->item_appearance      = GLADE_ITEM_ICON_ONLY;
  priv->use_small_item_icons = FALSE;

  /* create selector button */
  priv->selector_button = glade_palette_create_selector_button (palette);
  priv->selector_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (priv->selector_hbox), priv->selector_button,
                      FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (palette), priv->selector_hbox, FALSE, FALSE, 0);
  gtk_widget_show (priv->selector_button);
  gtk_widget_show (priv->selector_hbox);

  gtk_widget_set_tooltip_text (priv->selector_button, _("Widget selector"));

  /* The GtkToolPalette */
  priv->toolpalette = gtk_tool_palette_new ();
  gtk_tool_palette_set_style (GTK_TOOL_PALETTE (priv->toolpalette),
                              GTK_TOOLBAR_ICONS);
  gtk_tool_palette_set_icon_size (GTK_TOOL_PALETTE (priv->toolpalette),
                                  GTK_ICON_SIZE_LARGE_TOOLBAR);
  
  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                                       GTK_SHADOW_NONE);

  gtk_container_add (GTK_CONTAINER (sw), priv->toolpalette);
  gtk_box_pack_start (GTK_BOX (palette), sw, TRUE, TRUE, 0);

  gtk_widget_show (sw);
  gtk_widget_show (priv->toolpalette);
  gtk_widget_set_no_show_all (GTK_WIDGET (palette), TRUE);

  glade_palette_populate (palette);
}


/*******************************************************
 *                         API                         *
 *******************************************************/

/**
 * glade_palette_new:
 *
 * Creates a new #GladePalette widget
 *
 * Returns: a new #GladePalette
 */
GtkWidget *
glade_palette_new (void)
{
  GladePalette *palette;

  palette = g_object_new (GLADE_TYPE_PALETTE,
                          "spacing", 2,
                          "item-appearance", GLADE_ITEM_ICON_ONLY,
                          NULL);

  return GTK_WIDGET (palette);
}

/**
 * glade_palette_get_project:
 * @palette: a #GladePalette
 *
 * Returns: (transfer none): a #GladeProject
 */
GladeProject *
glade_palette_get_project (GladePalette *palette)
{
  g_return_val_if_fail (GLADE_IS_PALETTE (palette), NULL);

  return palette->priv->project;
}

void
glade_palette_set_project (GladePalette *palette, GladeProject *project)
{
  g_return_if_fail (GLADE_IS_PALETTE (palette));

  if (palette->priv->project != project)
    {
      if (palette->priv->project)
        {
          g_signal_handlers_disconnect_by_func (G_OBJECT (palette->priv->project),
                                                G_CALLBACK (glade_palette_refresh),
                                                palette);

          g_signal_handlers_disconnect_by_func (G_OBJECT (palette->priv->project),
                                                G_CALLBACK (project_add_item_changed_cb),
                                                palette);

          g_object_unref (palette->priv->project);
        }

      palette->priv->project = project;

      if (palette->priv->project)
        {
          g_signal_connect_swapped (G_OBJECT (palette->priv->project), "targets-changed",
                                    G_CALLBACK (glade_palette_refresh), palette);
          g_signal_connect_swapped (G_OBJECT (palette->priv->project), "parse-finished",
                                    G_CALLBACK (glade_palette_refresh), palette);

          g_signal_connect (G_OBJECT (palette->priv->project), "notify::add-item",
                            G_CALLBACK (project_add_item_changed_cb), palette);

          g_object_ref (palette->priv->project);

          project_add_item_changed_cb (project, NULL, palette);
        }

      glade_palette_refresh (palette);

      g_object_notify_by_pspec (G_OBJECT (palette), properties[PROP_PROJECT]);
    }
}

/**
 * glade_palette_set_item_appearance:
 * @palette: a #GladePalette
 * @item_appearance: the item appearance
 *
 * Sets the appearance of the palette items.
 */
void
glade_palette_set_item_appearance (GladePalette       *palette,
                                   GladeItemAppearance item_appearance)
{
  GladePalettePrivate *priv;

  g_return_if_fail (GLADE_IS_PALETTE (palette));

  priv = palette->priv;

  if (priv->item_appearance != item_appearance)
    {
      GtkToolbarStyle style;
      priv->item_appearance = item_appearance;

      switch (item_appearance)
        {
          case GLADE_ITEM_ICON_AND_LABEL:
            style = GTK_TOOLBAR_BOTH_HORIZ;
            break;
          case GLADE_ITEM_ICON_ONLY:
            style = GTK_TOOLBAR_ICONS;
            break;
          case GLADE_ITEM_LABEL_ONLY:
            style = GTK_TOOLBAR_TEXT;
            break;
          default:
            g_assert_not_reached ();
            break;
        }

      gtk_tool_palette_set_style (GTK_TOOL_PALETTE (priv->toolpalette), style);

      g_object_notify_by_pspec (G_OBJECT (palette), properties[PROP_ITEM_APPEARANCE]);
    }
}

/**
 * glade_palette_set_use_small_item_icons:
 * @palette: a #GladePalette
 * @use_small_item_icons: Whether to use small item icons
 *
 * Sets whether to use small item icons.
 */
void
glade_palette_set_use_small_item_icons (GladePalette *palette,
                                        gboolean      use_small_item_icons)
{
  GladePalettePrivate *priv;
  g_return_if_fail (GLADE_IS_PALETTE (palette));
  priv = palette->priv;

  if (priv->use_small_item_icons != use_small_item_icons)
    {
      priv->use_small_item_icons = use_small_item_icons;

      gtk_tool_palette_set_icon_size (GTK_TOOL_PALETTE (priv->toolpalette),
                                      (use_small_item_icons) ?
                                        GTK_ICON_SIZE_SMALL_TOOLBAR :
                                        GTK_ICON_SIZE_LARGE_TOOLBAR);

      g_object_notify_by_pspec (G_OBJECT (palette), properties[PROP_USE_SMALL_ITEM_ICONS]);
    }
}

/**
 * glade_palette_set_show_selector_button:
 * @palette: a #GladePalette
 * @show_selector_button: whether to show selector button
 *
 * Sets whether to show the internal widget selector button
 */
void
glade_palette_set_show_selector_button (GladePalette *palette,
                                        gboolean      show_selector_button)
{
  GladePalettePrivate *priv;
  g_return_if_fail (GLADE_IS_PALETTE (palette));
  priv = palette->priv;

  if (gtk_widget_get_visible (priv->selector_hbox) != show_selector_button)
    {
      if (show_selector_button)
        gtk_widget_show (priv->selector_hbox);
      else
        gtk_widget_hide (priv->selector_hbox);

      g_object_notify_by_pspec (G_OBJECT (palette), properties[PROP_SHOW_SELECTOR_BUTTON]);

    }

}

/**
 * glade_palette_get_item_appearance:
 * @palette: a #GladePalette
 *
 * Returns: The appearance of the palette items
 */
GladeItemAppearance
glade_palette_get_item_appearance (GladePalette *palette)
{;
  g_return_val_if_fail (GLADE_IS_PALETTE (palette), GLADE_ITEM_ICON_ONLY);

  return palette->priv->item_appearance;
}

/**
 * glade_palette_get_use_small_item_icons:
 * @palette: a #GladePalette
 *
 * Returns: Whether small item icons are used
 */
gboolean
glade_palette_get_use_small_item_icons (GladePalette *palette)
{
  g_return_val_if_fail (GLADE_IS_PALETTE (palette), FALSE);

  return palette->priv->use_small_item_icons;
}

/**
 * glade_palette_get_show_selector_button:
 * @palette: a #GladePalette
 *
 * Returns: Whether the selector button is visible
 */
gboolean
glade_palette_get_show_selector_button (GladePalette *palette)
{
  g_return_val_if_fail (GLADE_IS_PALETTE (palette), FALSE);

  return gtk_widget_get_visible (palette->priv->selector_hbox);
}

/**
 * glade_palette_get_tool_palette:
 * @palette: a #GladePalette
 *
 * Returns: (transfer none): the GtkToolPalette associated to this palette.
 */
GtkToolPalette *
glade_palette_get_tool_palette (GladePalette *palette)
{
  g_return_val_if_fail (GLADE_IS_PALETTE (palette), FALSE);

  return GTK_TOOL_PALETTE (palette->priv->toolpalette);
}
