/*
 * glade-design-view.c
 *
 * Copyright (C)      2006 Vincent Geddes
 *               2011-2016 Juan Pablo Ugarte
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
#include "glade-dnd.h"
#include "glade-utils.h"
#include "glade-design-view.h"
#include "glade-design-layout.h"
#include "glade-design-private.h"
#include "glade-path.h"
#include "glade-adaptor-chooser-widget.h"

#include <glib.h>
#include <glib/gi18n.h>

#define GLADE_DESIGN_VIEW_KEY "GLADE_DESIGN_VIEW_KEY"

enum
{
  PROP_0,
  PROP_PROJECT
};

typedef struct _GladeDesignViewPrivate
{
  GladeProject *project;
  GtkWidget *scrolled_window;  /* Main scrolled window */
  GtkWidget *layout_box;       /* Box to pack a GladeDesignLayout for each toplevel in project */

  _GladeDrag *drag_target;
  GObject *drag_data;
  gboolean drag_highlight;
} GladeDesignViewPrivate;

static GtkVBoxClass *parent_class = NULL;

static void glade_design_view_drag_init (_GladeDragInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GladeDesignView, glade_design_view, GTK_TYPE_BOX,
                         G_ADD_PRIVATE (GladeDesignView)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_DRAG,
                                                glade_design_view_drag_init))

static void
glade_design_layout_scroll (GladeDesignView *view, gint x, gint y, gint w, gint h)
{
  GladeDesignViewPrivate *priv = glade_design_view_get_instance_private (view);
  gdouble vadj_val, hadj_val, vpage_end, hpage_end;
  GtkAdjustment *vadj, *hadj;

  g_assert (GLADE_IS_DESIGN_VIEW (view));

  vadj = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (priv->scrolled_window));
  hadj = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (priv->scrolled_window));

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
  g_assert (GLADE_IS_DESIGN_VIEW (view));

  glade_design_layout_scroll (view, alloc->x, alloc->y, alloc->width, alloc->height);
  g_signal_handlers_disconnect_by_func (widget, on_layout_size_allocate, view);
}

static void
glade_design_view_update_state (GList *objects, GtkStateFlags state)
{
  GList *l;

  for (l = objects; l && l->data; l = g_list_next (l))
    {
      GtkWidget *view, *widget = l->data;

      if (GTK_IS_WIDGET (widget) &&
          gtk_widget_get_visible (widget) &&
          (view = gtk_widget_get_ancestor (widget, GLADE_TYPE_DESIGN_LAYOUT)))
        {
          gtk_widget_set_state_flags (view, state, TRUE);
        }
    }
}

static void
glade_design_view_selection_changed (GladeProject *project, GladeDesignView *view)
{
  GtkWidget *layout;
  GList *selection;

  g_assert (GLADE_IS_DESIGN_VIEW (view));

  glade_design_view_update_state (glade_project_toplevels (project),
                                  GTK_STATE_FLAG_NORMAL);

  if (!(selection = glade_project_selection_get (project)))
    return;

  glade_design_view_update_state (selection, GTK_STATE_FLAG_SELECTED);

  /* Check if its only one widget selected and scroll viewport to show toplevel */
  if (g_list_next (selection) == NULL &&
      GTK_IS_WIDGET (selection->data) &&
      (layout = gtk_widget_get_ancestor (selection->data, GLADE_TYPE_DESIGN_LAYOUT)))
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
  GladeDesignViewPrivate *priv = glade_design_view_get_instance_private (view);
  GtkWidget *layout;
  GList *toplevels;
  GObject *object;

  g_assert (GLADE_IS_DESIGN_VIEW (view));

  if (glade_widget_get_parent (widget) ||
      (object = glade_widget_get_object (widget)) == NULL ||
      !GTK_IS_WIDGET (object) ||
      gtk_widget_get_parent (GTK_WIDGET (object)))
    return;

  /* Create a GladeDesignLayout and add the toplevel widget to the view */
  layout = _glade_design_layout_new (view);
  gtk_widget_set_halign (layout, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (priv->layout_box), layout, FALSE, FALSE, 0);

  if ((toplevels = glade_project_toplevels (priv->project)))
    gtk_box_reorder_child (GTK_BOX (priv->layout_box), layout, 
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

  g_assert (GLADE_IS_DESIGN_VIEW (view));

  if (glade_widget_get_parent (widget) ||
      (object = glade_widget_get_object (widget)) == NULL ||
      !GTK_IS_WIDGET (object)) return;
  
  /* Remove toplevel widget from the view */
  if ((layout = gtk_widget_get_parent (GTK_WIDGET (object))) &&
      gtk_widget_is_ancestor (layout, GTK_WIDGET (view)))
    {
      GladeDesignViewPrivate *priv = glade_design_view_get_instance_private (view);

      gtk_container_remove (GTK_CONTAINER (layout), GTK_WIDGET (object));
      gtk_container_remove (GTK_CONTAINER (priv->layout_box), layout);
    }
}

static void
glade_design_view_widget_visibility_changed (GladeProject    *project,
                                             GladeWidget     *widget,
                                             gboolean         visible,
                                             GladeDesignView *view)
{
  g_assert (GLADE_IS_DESIGN_VIEW (view));

  if (visible)
    glade_design_view_add_toplevel (view, widget);
  else
    glade_design_view_remove_toplevel (view, widget);
}

static void
on_project_add_widget (GladeProject *project, GladeWidget *widget, GladeDesignView *view)
{
  g_assert (GLADE_IS_DESIGN_VIEW (view));

  glade_design_view_add_toplevel (view, widget);
}

static void
on_project_remove_widget (GladeProject *project, GladeWidget *widget, GladeDesignView *view)
{
  g_assert (GLADE_IS_DESIGN_VIEW (view));

  glade_design_view_remove_toplevel (view, widget);
}

static void
glade_design_view_set_project (GladeDesignView *view, GladeProject *project)
{
  GladeDesignViewPrivate *priv = glade_design_view_get_instance_private (view);

  g_assert (GLADE_IS_DESIGN_VIEW (view));

  if (priv->project)
    {
      g_signal_handlers_disconnect_by_data (priv->project, view);
      g_signal_handlers_disconnect_by_data (priv->project, priv->scrolled_window);

      g_object_set_data (G_OBJECT (priv->project), GLADE_DESIGN_VIEW_KEY, NULL);
    }

  g_set_object (&priv->project, project);

  if (!project)
    return;

  g_assert (GLADE_IS_PROJECT (project));

  g_signal_connect (project, "add-widget",
                    G_CALLBACK (on_project_add_widget), view);
  g_signal_connect (project, "remove-widget",
                    G_CALLBACK (on_project_remove_widget), view);
  g_signal_connect_swapped (project, "parse-began",
                            G_CALLBACK (gtk_widget_hide),
                            priv->scrolled_window);
  g_signal_connect_swapped (project, "parse-finished",
                            G_CALLBACK (gtk_widget_show),
                            priv->scrolled_window);
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
  GladeDesignViewPrivate *priv = glade_design_view_get_instance_private ((GladeDesignView *) object);

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
logo_draw (GtkWidget *widget, cairo_t *cr, GdkRGBA *c)
{
  GtkAllocation alloc;
  gdouble scale;

  gtk_widget_get_allocation (widget, &alloc);

  cairo_save (cr);
  
  cairo_set_source_rgba (cr, c->red, c->green, c->blue, .06);

  scale = MIN ((alloc.width/1.5)/(glade_path_WIDTH), (alloc.height/1.5)/(glade_path_HEIGHT));

  cairo_scale (cr, scale, scale);
  
  cairo_translate (cr, (alloc.width / scale) - glade_path_WIDTH,
                   (alloc.height / scale) - glade_path_HEIGHT);
  cairo_append_path (cr, &glade_path);
  cairo_fill (cr);

  cairo_restore (cr);
}

static void
on_chooser_adaptor_widget_selected (_GladeAdaptorChooserWidget *chooser,
                                    GladeWidgetAdaptor         *adaptor,
                                    GladeProject               *project)

{
  glade_command_create (adaptor, NULL, NULL, project);
  gtk_widget_destroy (gtk_widget_get_ancestor (GTK_WIDGET (chooser), GTK_TYPE_POPOVER));
}

static gboolean
glade_design_view_viewport_button_press (GtkWidget       *widget,
                                         GdkEventButton  *event,
                                         GladeDesignView *view)
{
  GladeDesignViewPrivate *priv = glade_design_view_get_instance_private ((GladeDesignView *) view);
  GdkRectangle rect = {event->x, event->y, 8, 8};
  GtkWidget *pop, *chooser;

  g_assert (GLADE_IS_DESIGN_VIEW (view));

  if (event->type != GDK_2BUTTON_PRESS)
    return FALSE;

  pop = gtk_popover_new (widget);
  gtk_popover_set_pointing_to (GTK_POPOVER (pop), &rect);
  gtk_popover_set_position (GTK_POPOVER (pop), GTK_POS_BOTTOM);
  
  chooser = _glade_adaptor_chooser_widget_new (GLADE_ADAPTOR_CHOOSER_WIDGET_TOPLEVEL |
                                               GLADE_ADAPTOR_CHOOSER_WIDGET_WIDGET |
                                               GLADE_ADAPTOR_CHOOSER_WIDGET_SKIP_DEPRECATED,
                                               priv->project);
  _glade_adaptor_chooser_widget_populate (GLADE_ADAPTOR_CHOOSER_WIDGET (chooser));
  g_signal_connect (chooser, "adaptor-selected",
                    G_CALLBACK (on_chooser_adaptor_widget_selected),
                    priv->project);

  gtk_container_add (GTK_CONTAINER (pop), chooser);
  gtk_widget_show (chooser);
  gtk_popover_popup (GTK_POPOVER (pop));

  return TRUE;
}

static gboolean
glade_design_view_viewport_draw (GtkWidget *widget, cairo_t *cr, GladeDesignView *view)
{
  GladeDesignViewPrivate *priv = glade_design_view_get_instance_private ((GladeDesignView *) view);
  GtkStyleContext *context = gtk_widget_get_style_context (widget);
  GdkRGBA fg_color;

  g_assert (GLADE_IS_DESIGN_VIEW (view));

  gtk_style_context_get_color (context, gtk_style_context_get_state (context),
                               &fg_color);

  logo_draw (widget, cr, &fg_color);

  if (priv->drag_highlight)
    {
      GdkRGBA c;

      gtk_style_context_save (context);
      gtk_style_context_get_background_color (context,
                                              gtk_style_context_get_state (context) |
                                              GTK_STATE_FLAG_SELECTED |
                                              GTK_STATE_FLAG_FOCUSED, &c);
      gtk_style_context_restore (context);

      cairo_set_line_width (cr, 2);
      gdk_cairo_set_source_rgba (cr, &c);
      cairo_rectangle (cr, 0, 0,
                       gtk_widget_get_allocated_width (widget),
                       gtk_widget_get_allocated_height (widget));
      cairo_stroke (cr);
    }

  return FALSE;
}


static void
glade_design_view_init (GladeDesignView *view)
{
  GladeDesignViewPrivate *priv = glade_design_view_get_instance_private (view);
  GtkWidget *viewport;

  gtk_widget_set_no_show_all (GTK_WIDGET (view), TRUE);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (view),
                                  GTK_ORIENTATION_VERTICAL);

  priv->project = NULL;
  priv->layout_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_valign (priv->layout_box, GTK_ALIGN_START);
  gtk_container_set_border_width (GTK_CONTAINER (priv->layout_box), 0);

  priv->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  viewport = gtk_viewport_new (NULL, NULL);
  gtk_widget_add_events (viewport, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
  g_signal_connect (viewport, "button-press-event",
                    G_CALLBACK (glade_design_view_viewport_button_press),
                    view);
  g_signal_connect (viewport, "draw",
                    G_CALLBACK (glade_design_view_viewport_draw),
                    view);
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (viewport), priv->layout_box);
  gtk_container_add (GTK_CONTAINER (priv->scrolled_window), viewport);

  gtk_widget_show (priv->scrolled_window);
  gtk_widget_show (viewport);
  gtk_widget_show_all (priv->layout_box);
  
  gtk_box_pack_start (GTK_BOX (view), priv->scrolled_window, TRUE, TRUE, 0);
  
  gtk_container_set_border_width (GTK_CONTAINER (view), 0);

  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (view)),
                               GTK_STYLE_CLASS_VIEW);

  _glade_dnd_dest_set (GTK_WIDGET (view));
}

static void
glade_design_view_dispose (GObject *object)
{
  GladeDesignView *view = GLADE_DESIGN_VIEW (object);
  GladeDesignViewPrivate *priv = glade_design_view_get_instance_private (view);

  glade_design_view_set_project (view, NULL);
  g_clear_object (&priv->drag_target);
  g_clear_object (&priv->drag_data);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

typedef struct
{
  GtkWidget *child;
  gint x, y;
} FindInContainerData;

static void
find_inside_container (GtkWidget *widget, FindInContainerData *data)
{
  GtkAllocation alloc;
  gint x, y;

  if (data->child || !gtk_widget_get_mapped (widget))
    return;

  x = data->x;
  y = data->y;
  gtk_widget_get_allocation (widget, &alloc);

  if (x >= alloc.x && x <= (alloc.x + alloc.width) &&
      y >= alloc.y && y <= (alloc.y + alloc.height))
    {
      data->child = widget;
    }
}

static void
glade_design_view_drag_highlight (_GladeDrag *dest,
                                  gint       x,
                                  gint       y)
{
  if (GLADE_IS_WIDGET (dest))
    {
      GObject *obj = glade_widget_get_object (GLADE_WIDGET (dest));
      
      if (GTK_IS_WIDGET (obj))
        {
          GtkWidget *layout = gtk_widget_get_ancestor (GTK_WIDGET (obj),
                                                       GLADE_TYPE_DESIGN_LAYOUT);
          if (layout)
            _glade_design_layout_set_highlight (GLADE_DESIGN_LAYOUT (layout),
                                                (x<0 || y<0) ? NULL : GLADE_WIDGET (dest));
        }
    }

  _glade_drag_highlight (dest, x, y);
}

static gboolean
glade_design_view_drag_motion (GtkWidget *widget,
                               GdkDragContext *context,
                               gint x, gint y,
                               guint time)
{
  GladeDesignView *view = GLADE_DESIGN_VIEW (widget);
  GladeDesignViewPrivate *priv = glade_design_view_get_instance_private (view);
  FindInContainerData data;
  _GladeDrag *drag = NULL;
  gint xx, yy;

  g_assert (GLADE_IS_DESIGN_VIEW (view));

  if (!priv->drag_data)
    {
      GdkAtom target = gtk_drag_dest_find_target (widget, context, NULL);

      if (target)
        gtk_drag_get_data (widget, context, target, time);
    }

  data.child = NULL;
  gtk_widget_translate_coordinates (widget, GTK_WIDGET (priv->layout_box),
                                    x, y, &data.x, &data.y);
  gtk_container_forall (GTK_CONTAINER (priv->layout_box),
                        (GtkCallback) find_inside_container,
                        &data);

  if (data.child)
    {
      GladeDesignLayout *layout = GLADE_DESIGN_LAYOUT (data.child);
      GladeWidget *gchild = _glade_design_layout_get_child (layout);
      GtkWidget *child = GTK_WIDGET (glade_widget_get_object (gchild));
      GtkWidget *drag_target;

      gtk_widget_translate_coordinates (widget, child, x, y, &xx, &yy);
      
      drag_target = _glade_design_layout_get_child_at_position (child, xx, yy);

      if (drag_target)
        {
          _GladeDrag *gwidget = NULL;

          if (GLADE_IS_PLACEHOLDER (drag_target)) {
              gwidget = (_GladeDrag *) drag_target;
          } else if (GLADE_IS_WIDGET (drag_target)) {
              gwidget = (_GladeDrag *) glade_widget_get_from_gobject ((GladeWidget *) drag_target);
          }

          while (gwidget && !_glade_drag_can_drop (gwidget,
                                                   xx, yy, priv->drag_data)) {
            if (GLADE_IS_WIDGET (gwidget)) {
              gwidget = (_GladeDrag *) glade_widget_get_parent ((GladeWidget *) gwidget);
            } else if (GLADE_IS_PLACEHOLDER (gwidget)) {
              gwidget = (_GladeDrag *) glade_placeholder_get_parent ((GladePlaceholder *) gwidget);
            } else {
              gwidget = NULL;
            }
          }

          if (gwidget) {
            drag = GLADE_DRAG (gwidget);
          }
        }
    }
  else if (_glade_drag_can_drop (GLADE_DRAG (widget), x, y, priv->drag_data))
    {
      drag = GLADE_DRAG (widget);
      xx = x;
      yy = y;
    }

  if (priv->drag_target && priv->drag_target != drag)
    {
      glade_design_view_drag_highlight (priv->drag_target, -1, -1);
      g_clear_object (&priv->drag_target);
    }
  
  if (drag)
    {
      priv->drag_target = g_object_ref (drag);

      glade_design_view_drag_highlight (drag, xx, yy);
      
      gdk_drag_status (context, GDK_ACTION_COPY, time);
      return TRUE;
    }

  gdk_drag_status (context, 0, time);
  return FALSE;
}

static void
glade_design_view_drag_leave (GtkWidget      *widget,
                              GdkDragContext *context,
                              guint           time)
{
  GladeDesignView *view = GLADE_DESIGN_VIEW (widget);
  GladeDesignViewPrivate *priv = glade_design_view_get_instance_private (view);

  g_assert (GLADE_IS_DESIGN_VIEW (view));

  if (priv->drag_target)
    glade_design_view_drag_highlight (priv->drag_target, -1, -1);
}

static void
on_source_drag_end (GtkWidget       *widget,
                    GdkDragContext  *context, 
                    GladeDesignView *view)
{
  GladeDesignViewPrivate *priv = glade_design_view_get_instance_private (view);

  g_assert (GLADE_IS_DESIGN_VIEW (view));
  
  if (priv->drag_target)
    {
      glade_design_view_drag_highlight (priv->drag_target, -1, -1);
      g_clear_object (&priv->drag_target);
    }

  g_clear_object (&priv->drag_data);
}

static void
glade_design_view_drag_data_received (GtkWidget        *widget,
                                      GdkDragContext   *context,
                                      gint              x,
                                      gint              y,
                                      GtkSelectionData *selection,
                                      guint             info,
                                      guint             time)
{
  GtkWidget *source = gtk_drag_get_source_widget (context);
  GladeDesignView *view = GLADE_DESIGN_VIEW (widget);
  GladeDesignViewPrivate *priv = glade_design_view_get_instance_private (view);

  g_assert (GLADE_IS_DESIGN_VIEW (view));

  g_signal_handlers_disconnect_by_func (source, on_source_drag_end, view);

  g_set_object (&priv->drag_data, _glade_dnd_get_data (context, selection, info));

  g_signal_connect_object (source, "drag-end", G_CALLBACK (on_source_drag_end), view, 0);
}

static gboolean
glade_design_view_drag_drop (GtkWidget       *widget,
                             GdkDragContext  *context,
                             gint             x,
                             gint             y,
                             guint            time)  
{
  GladeDesignView *view = GLADE_DESIGN_VIEW (widget);
  GladeDesignViewPrivate *priv = glade_design_view_get_instance_private (view);

  g_assert (GLADE_IS_DESIGN_VIEW (view));

  if (priv->drag_data && priv->drag_target)
    {
      GtkWidget *target;
      gint xx, yy;

      if (GLADE_IS_WIDGET (priv->drag_target))
        target = GTK_WIDGET (glade_widget_get_object (GLADE_WIDGET (priv->drag_target)));
      else
        target = GTK_WIDGET (priv->drag_target);
      
      gtk_widget_translate_coordinates (widget, target, x, y, &xx, &yy);
      _glade_drag_drop (GLADE_DRAG (priv->drag_target), xx, yy, priv->drag_data);
      gtk_drag_finish (context, TRUE, FALSE, time);
    }
  else
    gtk_drag_finish (context, FALSE, FALSE, time);

  return TRUE;
}

static gboolean
glade_design_view_drag_iface_can_drop (_GladeDrag *drag,
                                       gint x, gint y,
                                       GObject *data)
{
  GladeWidget *gwidget;

  if (GLADE_IS_WIDGET_ADAPTOR (data) ||
      ((gwidget = glade_widget_get_from_gobject (data)) &&
       glade_widget_get_parent (gwidget)))
    return TRUE;
  else
    return FALSE;
}

static gboolean
glade_design_view_drag_iface_drop (_GladeDrag *drag,
                                   gint x, gint y,
                                   GObject *data)
{
  GladeDesignView *view = GLADE_DESIGN_VIEW (drag);
  GladeDesignViewPrivate *priv = glade_design_view_get_instance_private (view);
  GladeWidget *gsource;

  g_assert (GLADE_IS_DESIGN_VIEW (view));

  if (GLADE_IS_WIDGET_ADAPTOR (data))
    {
      glade_command_create (GLADE_WIDGET_ADAPTOR (data),
                            NULL, NULL, priv->project);
      return TRUE;
    }
  else if ((gsource = glade_widget_get_from_gobject (data)))
    {
      GList widgets = {gsource, NULL, NULL};
      glade_command_dnd (&widgets, NULL, NULL);
      return TRUE;
    }

  return FALSE;
}

static void
glade_design_view_drag_iface_highlight (_GladeDrag *drag, gint x, gint y)
{
  GladeDesignView *view = GLADE_DESIGN_VIEW (drag);
  GladeDesignViewPrivate *priv = glade_design_view_get_instance_private (view);
  gboolean highlight = !(x < 0 || y < 0);

  g_assert (GLADE_IS_DESIGN_VIEW (view));

  if (priv->drag_highlight == highlight)
    return;
      
  priv->drag_highlight = highlight;

  gtk_widget_queue_draw (priv->scrolled_window);
}

static void
glade_design_view_drag_init (_GladeDragInterface *iface)
{
  iface->can_drag = NULL;
  iface->can_drop = glade_design_view_drag_iface_can_drop;
  iface->drop = glade_design_view_drag_iface_drop;
  iface->highlight = glade_design_view_drag_iface_highlight;
}

static void
glade_design_view_class_init (GladeDesignViewClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  parent_class = g_type_class_peek_parent (klass);
  object_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = glade_design_view_dispose;
  object_class->get_property = glade_design_view_get_property;
  object_class->set_property = glade_design_view_set_property;

  widget_class->drag_motion = glade_design_view_drag_motion;
  widget_class->drag_leave = glade_design_view_drag_leave;
  widget_class->drag_data_received = glade_design_view_drag_data_received;
  widget_class->drag_drop = glade_design_view_drag_drop;
  
  g_object_class_install_property (object_class,
                                   PROP_PROJECT,
                                   g_param_spec_object ("project",
                                                        "Project",
                                                        "The project for this view",
                                                        GLADE_TYPE_PROJECT,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));
}

/* Public API */

/**
 * glade_design_view_get_project:
 * @view: A #GladeDesignView
 *
 * Returns: (transfer none): a #GladeProject
 */
GladeProject *
glade_design_view_get_project (GladeDesignView *view)
{
  GladeDesignViewPrivate *priv = glade_design_view_get_instance_private (view);

  g_return_val_if_fail (GLADE_IS_DESIGN_VIEW (view), NULL);

  return priv->project;

}

/**
 * glade_design_view_new:
 * @project: A #GladeProject
 *
 * Returns: (transfer full): a new #GladeDesignView
 */
GtkWidget *
glade_design_view_new (GladeProject *project)
{
  GladeDesignView *view;

  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  view = g_object_new (GLADE_TYPE_DESIGN_VIEW, "project", project, NULL);

  return GTK_WIDGET (view);
}

/**
 * glade_design_view_get_from_project:
 * @project: A #GladeProject
 *
 * Returns: (transfer none) (nullable): a #GladeDesignView
 */
GladeDesignView *
glade_design_view_get_from_project (GladeProject *project)
{
  gpointer p;

  g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

  p = g_object_get_data (G_OBJECT (project), GLADE_DESIGN_VIEW_KEY);

  return (p != NULL) ? GLADE_DESIGN_VIEW (p) : NULL;
}
