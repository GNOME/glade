/*
 * glade-design-view.c
 *
 * Copyright (C) 2006 Vincent Geddes
 *               2011 Juan Pablo Ugarte
 *
 * Authors:
 *   Vincent Geddes <vincent.geddes@gmail.com>
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
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
 */

/**
 * SECTION:glade-design-view
 * @Title: GladeDesignView
 * @Short_Description: A widget to embed the workspace.
 *
 * Use this widget to embed toplevel widgets in a given #GladeProject.
 */

#include "config.h"

#include "glade.h"
#include "glade-utils.h"
#include "glade-design-view.h"
#include "glade-design-layout.h"
#include "glade-design-private.h"
#include "glade-path.h"

#include <glib.h>
#include <glib/gi18n.h>

#define GLADE_DESIGN_VIEW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), GLADE_TYPE_DESIGN_VIEW, GladeDesignViewPrivate))

#define GLADE_DESIGN_VIEW_KEY "GLADE_DESIGN_VIEW_KEY"

enum
{
  PROP_0,
  PROP_PROJECT,
  PROP_DRAG_SOURCE
};

struct _GladeDesignViewPrivate
{
  GladeProject *project;
  GtkWidget *scrolled_window;  /* Main scrolled window */
  GtkWidget *layout_box;       /* Box to pack a GladeDesignLayout for each toplevel in project */

  GtkToolPalette *palette;
  GladeWidgetAdaptor *drag_adaptor;
};

static GtkVBoxClass *parent_class = NULL;

G_DEFINE_TYPE (GladeDesignView, glade_design_view, GTK_TYPE_VBOX)

static void
glade_design_layout_scroll (GladeDesignView *view, gint x, gint y, gint w, gint h)
{
  gdouble vadj_val, hadj_val, vpage_end, hpage_end;
  GtkAdjustment *vadj, *hadj;

  vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (view->priv->scrolled_window));
  hadj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (view->priv->scrolled_window));

  vadj_val = gtk_adjustment_get_value (vadj);
  hadj_val = gtk_adjustment_get_value (hadj);
  vpage_end = gtk_adjustment_get_page_size (vadj) + vadj_val;
  hpage_end = gtk_adjustment_get_page_size (hadj) + hadj_val;

  /* TODO: we could set this value in increments in a timeout callback 
   * to make it look like its scrolling instead of jumping.
   */
  if (y < vadj_val || y > vpage_end || (y + h) > vpage_end)
    gtk_adjustment_set_value (vadj, y);

  if (x < hadj_val || x > hpage_end || (x + w) > hpage_end)
    gtk_adjustment_set_value (hadj, x);
}

static void 
on_layout_size_allocate (GtkWidget *widget, GtkAllocation *alloc, GladeDesignView *view)
{
  glade_design_layout_scroll (view, alloc->x, alloc->y, alloc->width, alloc->height);
  g_signal_handlers_disconnect_by_func (widget, on_layout_size_allocate, view);
}

static void
glade_design_view_selection_changed (GladeProject *project, GladeDesignView *view)
{
  GladeWidget *gwidget, *gtoplevel;
  GObject *toplevel;
  GtkWidget *layout;
  GList *selection;

  /* Check if its only one widget selected and scroll viewport to show toplevel */
  if ((selection = glade_project_selection_get (project)) &&
      g_list_next (selection) == NULL &&
      GTK_IS_WIDGET (selection->data) &&
      !GLADE_IS_PLACEHOLDER (selection->data) &&
      (gwidget = glade_widget_get_from_gobject (G_OBJECT (selection->data))) &&
      (gtoplevel = glade_widget_get_toplevel (gwidget)) &&
      (toplevel = glade_widget_get_object (gtoplevel)) &&
      GTK_IS_WIDGET (toplevel) &&
      (layout = gtk_widget_get_parent (GTK_WIDGET (toplevel))) &&
      GLADE_IS_DESIGN_LAYOUT (layout))
    {
      GtkAllocation alloc;
      gtk_widget_get_allocation (layout, &alloc);
          
      if (alloc.x < 0)
        g_signal_connect (layout, "size-allocate", G_CALLBACK (on_layout_size_allocate), view);
      else
        glade_design_layout_scroll (view, alloc.x, alloc.y, alloc.width, alloc.height);
    }
}

static void
glade_design_view_add_toplevel (GladeDesignView *view, GladeWidget *widget)
{
  GtkWidget *layout;
  GList *toplevels;
  GObject *object;

  if (glade_widget_get_parent (widget) ||
      (object = glade_widget_get_object (widget)) == NULL ||
      !GTK_IS_WIDGET (object) ||
      gtk_widget_get_parent (GTK_WIDGET (object)))
    return;

  /* Create a GladeDesignLayout and add the toplevel widget to the view */
  layout = _glade_design_layout_new (view);
  gtk_widget_set_halign (layout, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (view->priv->layout_box), layout, FALSE, FALSE, 0);

  if ((toplevels = glade_project_toplevels (view->priv->project)))
    gtk_box_reorder_child (GTK_BOX (view->priv->layout_box), layout, 
                           g_list_index (toplevels, GTK_WIDGET (object)));
  
  gtk_container_add (GTK_CONTAINER (layout), GTK_WIDGET (object));
  gtk_widget_show (GTK_WIDGET (object));
  gtk_widget_show (layout);
}

static void
glade_design_view_remove_toplevel (GladeDesignView *view, GladeWidget *widget)
{
  GtkWidget *layout;
  GObject *object;

  if (glade_widget_get_parent (widget) ||
      (object = glade_widget_get_object (widget)) == NULL ||
      !GTK_IS_WIDGET (object)) return;
  
  /* Remove toplevel widget from the view */
  if ((layout = gtk_widget_get_parent (GTK_WIDGET (object))) &&
      gtk_widget_is_ancestor (layout, GTK_WIDGET (view)))
    {
      gtk_container_remove (GTK_CONTAINER (layout), GTK_WIDGET (object));
      gtk_container_remove (GTK_CONTAINER (view->priv->layout_box), layout);
    }
}

static void
glade_design_view_widget_visibility_changed (GladeProject    *project,
                                             GladeWidget     *widget,
                                             gboolean         visible,
                                             GladeDesignView *view)
{
  if (visible)
    glade_design_view_add_toplevel (view, widget);
  else
    glade_design_view_remove_toplevel (view, widget);
}

static void
on_project_add_widget (GladeProject *project, GladeWidget *widget, GladeDesignView *view)
{
  glade_design_view_add_toplevel (view, widget);
}

static void
on_project_remove_widget (GladeProject *project, GladeWidget *widget, GladeDesignView *view)
{
  glade_design_view_remove_toplevel (view, widget);
}

static void
glade_design_view_set_project (GladeDesignView *view, GladeProject *project)
{
  g_return_if_fail (GLADE_IS_PROJECT (project));

  view->priv->project = project;

  g_signal_connect (project, "add-widget",
                    G_CALLBACK (on_project_add_widget), view);
  g_signal_connect (project, "remove-widget",
                    G_CALLBACK (on_project_remove_widget), view);
  g_signal_connect_swapped (project, "parse-began",
                            G_CALLBACK (gtk_widget_hide),
                            view->priv->scrolled_window);
  g_signal_connect_swapped (project, "parse-finished",
                            G_CALLBACK (gtk_widget_show),
                            view->priv->scrolled_window);
  g_signal_connect (project, "selection-changed",
                    G_CALLBACK (glade_design_view_selection_changed), view);
  g_signal_connect (project, "widget-visibility-changed",
                    G_CALLBACK (glade_design_view_widget_visibility_changed), view);

  g_object_set_data (G_OBJECT (project), GLADE_DESIGN_VIEW_KEY, view);
}

static void
glade_design_view_set_property (GObject *object,
                                guint prop_id,
                                const GValue *value, GParamSpec *pspec)
{
  switch (prop_id)
    {
      case PROP_PROJECT:
        glade_design_view_set_project (GLADE_DESIGN_VIEW (object),
                                       g_value_get_object (value));
        break;
      case PROP_DRAG_SOURCE:
        glade_design_view_set_drag_source (GLADE_DESIGN_VIEW (object),
                                           GTK_TOOL_PALETTE (g_value_get_object (value)));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_design_view_get_property (GObject *object,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
  switch (prop_id)
    {
      case PROP_PROJECT:
        g_value_set_object (value, GLADE_DESIGN_VIEW (object)->priv->project);
        break;
      case PROP_DRAG_SOURCE:
        g_value_set_object (value, GLADE_DESIGN_VIEW (object)->priv->palette);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
logo_draw (GtkWidget *widget, cairo_t *cr)
{
  GtkAllocation alloc;
  gdouble scale;

  gtk_widget_get_allocation (widget, &alloc);

  cairo_save (cr);
  
  cairo_set_source_rgba (cr, 0, 0, 0, .08);

  scale = MIN ((alloc.width/1.5)/(glade_path_WIDTH), (alloc.height/1.5)/(glade_path_HEIGHT));

  cairo_scale (cr, scale, scale);
  
  cairo_translate (cr, (alloc.width / scale) - glade_path_WIDTH,
                   (alloc.height / scale) - glade_path_HEIGHT);
  cairo_append_path (cr, &glade_path);
  cairo_fill (cr);

  cairo_restore (cr);
}

static gboolean
glade_design_view_draw (GtkWidget *widget, cairo_t *cr)
{
  GladeDesignViewPrivate *priv = GLADE_DESIGN_VIEW_GET_PRIVATE (widget);
  GdkWindow *window = gtk_widget_get_window (widget);

  if (gtk_cairo_should_draw_window (cr, window))
    {
      if (gtk_widget_get_visible (priv->scrolled_window) == FALSE)
        logo_draw (widget, cr);
      else
        gtk_render_background (gtk_widget_get_style_context (widget),
                               cr,
                               0, 0,
                               gdk_window_get_width (window),
                               gdk_window_get_height (window));
    }

  GTK_WIDGET_CLASS (glade_design_view_parent_class)->draw (widget, cr);
  
  return FALSE;
}

static void
glade_design_view_init (GladeDesignView *view)
{
  GtkWidget *viewport;

  view->priv = GLADE_DESIGN_VIEW_GET_PRIVATE (view);

  gtk_widget_set_no_show_all (GTK_WIDGET (view), TRUE);

  view->priv->project = NULL;
  view->priv->layout_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_valign (view->priv->layout_box, GTK_ALIGN_START);
  gtk_container_set_border_width (GTK_CONTAINER (view->priv->layout_box), 0);
  gtk_box_pack_end (GTK_BOX (view->priv->layout_box), gtk_fixed_new (), FALSE, FALSE, 0);

  view->priv->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW
                                  (view->priv->scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW
                                       (view->priv->scrolled_window),
                                       GTK_SHADOW_IN);

  viewport = gtk_viewport_new (NULL, NULL);
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (viewport), view->priv->layout_box);
  gtk_container_add (GTK_CONTAINER (view->priv->scrolled_window), viewport);

  gtk_widget_show (view->priv->scrolled_window);
  gtk_widget_show (viewport);
  gtk_widget_show_all (view->priv->layout_box);
  
  gtk_box_pack_start (GTK_BOX (view), view->priv->scrolled_window, TRUE, TRUE, 0);
  
  gtk_container_set_border_width (GTK_CONTAINER (view), 0);

  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (view)),
                               GTK_STYLE_CLASS_VIEW);
}

static void
glade_design_view_class_init (GladeDesignViewClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  parent_class = g_type_class_peek_parent (klass);
  object_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = glade_design_view_get_property;
  object_class->set_property = glade_design_view_set_property;

  widget_class->draw = glade_design_view_draw;
  
  g_object_class_install_property (object_class,
                                   PROP_PROJECT,
                                   g_param_spec_object ("project",
                                                        "Project",
                                                        "The project for this view",
                                                        GLADE_TYPE_PROJECT,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
                                   PROP_DRAG_SOURCE,
                                   g_param_spec_object ("drag-source",
                                                        "Drag Source",
                                                        "A palette to use as the source of drag events for this view",
                                                        GTK_TYPE_TOOL_PALETTE,
                                                        G_PARAM_READWRITE));

  g_type_class_add_private (object_class, sizeof (GladeDesignViewPrivate));
}

/* Private API */

void
_glade_design_view_freeze (GladeDesignView *view)
{
  g_return_if_fail (GLADE_IS_DESIGN_VIEW (view));
  
  g_signal_handlers_block_by_func (view->priv->project,
                                   glade_design_view_selection_changed,
                                   view);
}

void
_glade_design_view_thaw   (GladeDesignView *view)
{
  g_return_if_fail (GLADE_IS_DESIGN_VIEW (view));
  
  g_signal_handlers_unblock_by_func (view->priv->project,
                                     glade_design_view_selection_changed,
                                     view);
}

/* Public API */

GladeProject *
glade_design_view_get_project (GladeDesignView *view)
{
  g_return_val_if_fail (GLADE_IS_DESIGN_VIEW (view), NULL);

  return view->priv->project;

}

GtkWidget *
glade_design_view_new (GladeProject *project)
{
  GladeDesignView *view;

  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  view = g_object_new (GLADE_TYPE_DESIGN_VIEW, "project", project, NULL);

  return GTK_WIDGET (view);
}

GladeDesignView *
glade_design_view_get_from_project (GladeProject *project)
{
  gpointer p;

  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  p = g_object_get_data (G_OBJECT (project), GLADE_DESIGN_VIEW_KEY);

  return (p != NULL) ? GLADE_DESIGN_VIEW (p) : NULL;

}

static GtkWidget *
widget_get_child_from_position (GtkWidget *toplevel, GtkWidget *widget, gint x, gint y)
{
  GtkWidget *retval = NULL;
  GList *children, *l;

  if (!GTK_IS_CONTAINER (widget))
    return NULL;

  children = glade_util_container_get_all_children (GTK_CONTAINER (widget));
  
  for (l = children;l; l = g_list_next (l))
    {
      GtkWidget *child = l->data;

      if (gtk_widget_get_mapped (child))
        {
          GtkAllocation alloc;
          gint xx, yy;

          gtk_widget_translate_coordinates (toplevel, child, x, y, &xx, &yy);
          gtk_widget_get_allocation (child, &alloc);
          
          if (xx >= 0 && yy >= 0 && xx <= alloc.width && yy <= alloc.height)
            {
              if (GTK_IS_CONTAINER (child))
                retval = widget_get_child_from_position (toplevel, child, x, y);

              if (!retval)
                retval = child;

              break;
            }
        }
    }
  
  g_list_free (children);

  return retval;
}

static gboolean
widget_is_outside_glade_ancestor (GtkWidget *widget)
{
  while (widget)
    {
      if (glade_widget_get_from_gobject (widget))
        return TRUE;

      widget = gtk_widget_get_parent (widget);
    }

  return FALSE;
}

static gboolean
on_drag_motion (GtkWidget *widget,
                GdkDragContext *context,
                gint x, gint y,
                guint time)
{
  GladeDesignViewPrivate *priv = GLADE_DESIGN_VIEW (widget)->priv;
  GtkWidget *child;

  child = widget_get_child_from_position (widget, widget, x, y);

  if (!priv->drag_adaptor)
    {
      GdkAtom target = gtk_drag_dest_find_target (widget, context, NULL);

      if (!target)
        return FALSE;

      gtk_drag_get_data (widget, context, target, time);
    }
  
  if (child &&
      ((priv->drag_adaptor && GLADE_IS_PLACEHOLDER (child) &&
        GWA_IS_TOPLEVEL (priv->drag_adaptor)) ||
       (!GLADE_IS_PLACEHOLDER (child) &&
        widget_is_outside_glade_ancestor (child)) ))
    {
      gdk_drag_status (context, 0, time);
      return FALSE;
    }

  gdk_drag_status (context, GDK_ACTION_COPY, time);
  return TRUE;
}

static void
on_drag_data_received (GtkWidget        *widget,
                       GdkDragContext   *context,
                       gint              x,
                       gint              y,
                       GtkSelectionData *selection,
                       guint             info,
                       guint             time)
{
  GladeDesignViewPrivate *priv = GLADE_DESIGN_VIEW (widget)->priv;
  GtkWidget *item;

  item = gtk_tool_palette_get_drag_item (priv->palette, selection);
  g_return_if_fail (item);

  priv->drag_adaptor = g_object_get_data (G_OBJECT (item), "glade-widget-adaptor");
}

static gboolean
on_drag_drop (GtkWidget       *widget,
              GdkDragContext  *context,
              gint             x,
              gint             y,
              guint            time)  
{
  GladeDesignViewPrivate *priv = GLADE_DESIGN_VIEW (widget)->priv;
  GtkWidget *child;

  if (!priv->drag_adaptor)
    return FALSE;
  
  child = widget_get_child_from_position (widget, widget, x, y);
    
  if (child && GLADE_IS_PLACEHOLDER (child))
    {
      GladePlaceholder *placeholder = GLADE_PLACEHOLDER (child);

      glade_command_create (priv->drag_adaptor,
                            glade_placeholder_get_parent (placeholder),
                            placeholder, 
                            priv->project);
    }
  else
    {
      glade_command_create (priv->drag_adaptor, NULL, NULL, priv->project);
    }

  gtk_drag_finish (context, TRUE, FALSE, time);
  priv->drag_adaptor = NULL;
  return TRUE;
}

void
glade_design_view_set_drag_source (GladeDesignView *view, GtkToolPalette *source)
{
  GladeDesignViewPrivate *priv;
  GtkWidget *target;

  g_return_if_fail (GLADE_IS_DESIGN_VIEW (view));
  priv = view->priv;

  if (priv->palette == source)
    return;

  if (priv->palette)
    gtk_drag_dest_unset (GTK_WIDGET (priv->palette));

  target = GTK_WIDGET (view);
  priv->palette = source;

  if (priv->palette)
    {
      g_signal_connect (target, "drag-motion", G_CALLBACK (on_drag_motion), NULL);
      g_signal_connect (target, "drag-data-received", G_CALLBACK (on_drag_data_received), NULL);
      g_signal_connect (target, "drag-drop", G_CALLBACK (on_drag_drop), NULL);

      gtk_tool_palette_add_drag_dest (priv->palette, target, 0,
                                      GTK_TOOL_PALETTE_DRAG_ITEMS,
                                      GDK_ACTION_COPY);
    }
  else
    {
      g_signal_handlers_disconnect_by_func (target, on_drag_motion, NULL);
      g_signal_handlers_disconnect_by_func (target, on_drag_data_received, NULL);
      g_signal_handlers_disconnect_by_func (target, on_drag_drop, NULL);
    }
}
