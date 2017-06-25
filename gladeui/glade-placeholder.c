/*
 * Copyright (C) 2003, 2004 Joaquin Cuenca Abela
 *
 * Authors:
 *   Joaquin Cuenca Abela <e98cuenc@yahoo.com> 
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
*/

#include "config.h"

/**
 * SECTION:glade-placeholder
 * @Short_Description: A #GtkWidget to fill empty places.
 *
 * Generally in Glade, container widgets are implemented with #GladePlaceholder 
 * children to allow users to 'click' and add thier widgets to a container. 
 * It is the responsability of the plugin writer to create placeholders for 
 * container widgets where appropriate; usually in #GladePostCreateFunc 
 * when the #GladeCreateReason is %GLADE_CREATE_USER.
 */

#include <gtk/gtk.h>
#include "glade-marshallers.h"
#include "glade.h"
#include "glade-placeholder.h"
#include "glade-xml-utils.h"
#include "glade-project.h"
#include "glade-command.h"
#include "glade-palette.h"
#include "glade-popup.h"
#include "glade-cursor.h"
#include "glade-widget.h"
#include "glade-app.h"
#include "glade-adaptor-chooser-widget.h"
#include <math.h>

#include "glade-dnd.h"
#include "glade-drag.h"

#define WIDTH_REQUISITION    20
#define HEIGHT_REQUISITION   20

static cairo_pattern_t *placeholder_pattern = NULL;

struct _GladePlaceholderPrivate
{
  GList *packing_actions;

  GdkWindow *event_window;

  gboolean drag_highlight;
};

enum
{
  PROP_0,
  PROP_HADJUSTMENT,
  PROP_VADJUSTMENT,
  PROP_HSCROLL_POLICY,
  PROP_VSCROLL_POLICY
};

#define GLADE_PLACEHOLDER_PRIVATE(object) (((GladePlaceholder*)object)->priv)

static void glade_placeholder_drag_init (_GladeDragInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GladePlaceholder, glade_placeholder, GTK_TYPE_WIDGET,
                         G_ADD_PRIVATE (GladePlaceholder)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_DRAG, 
                                                glade_placeholder_drag_init))

static void
glade_placeholder_notify_parent (GObject    *gobject,
                                 GParamSpec *arg1,
                                 gpointer    user_data)
{
  GladePlaceholder *placeholder = GLADE_PLACEHOLDER (gobject);
  GladeWidgetAdaptor *parent_adaptor = NULL;
  GladeWidget *parent = glade_placeholder_get_parent (placeholder);

  if (placeholder->priv->packing_actions)
    {
      g_list_foreach (placeholder->priv->packing_actions, (GFunc) g_object_unref,
                      NULL);
      g_list_free (placeholder->priv->packing_actions);
      placeholder->priv->packing_actions = NULL;
    }

  if (parent)
    parent_adaptor = glade_widget_get_adaptor (parent);

  if (parent_adaptor)
    placeholder->priv->packing_actions =
      glade_widget_adaptor_pack_actions_new (parent_adaptor);
}

static void
glade_placeholder_init (GladePlaceholder *placeholder)
{
  placeholder->priv =  glade_placeholder_get_instance_private (placeholder);

  placeholder->priv->packing_actions = NULL;

  gtk_widget_set_can_focus (GTK_WIDGET (placeholder), TRUE);
  gtk_widget_set_has_window (GTK_WIDGET (placeholder), FALSE);

  gtk_widget_set_size_request (GTK_WIDGET (placeholder),
                               WIDTH_REQUISITION, HEIGHT_REQUISITION);

  _glade_dnd_dest_set (GTK_WIDGET (placeholder));

  g_signal_connect (placeholder, "notify::parent",
                    G_CALLBACK (glade_placeholder_notify_parent), NULL);

  gtk_widget_set_hexpand (GTK_WIDGET (placeholder), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (placeholder), TRUE);
  gtk_widget_show (GTK_WIDGET (placeholder));
}

static void
glade_placeholder_finalize (GObject *object)
{
  GladePlaceholder *placeholder;

  g_return_if_fail (GLADE_IS_PLACEHOLDER (object));
  placeholder = GLADE_PLACEHOLDER (object);

  if (placeholder->priv->packing_actions)
    {
      g_list_foreach (placeholder->priv->packing_actions, (GFunc) g_object_unref, NULL);
      g_list_free (placeholder->priv->packing_actions);
    }

  G_OBJECT_CLASS (glade_placeholder_parent_class)->finalize (object);
}

static void
glade_placeholder_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{

  switch (prop_id)
    {
      case PROP_HADJUSTMENT:
      case PROP_VADJUSTMENT:
      case PROP_HSCROLL_POLICY:
      case PROP_VSCROLL_POLICY:
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_placeholder_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  switch (prop_id)
    {
      case PROP_HADJUSTMENT:
      case PROP_VADJUSTMENT:
        g_value_set_object (value, NULL);
        break;
      case PROP_HSCROLL_POLICY:
      case PROP_VSCROLL_POLICY:
        g_value_set_enum (value, GTK_SCROLL_MINIMUM);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_placeholder_realize (GtkWidget *widget)
{
  GladePlaceholder *placeholder;
  GtkAllocation allocation;
  GdkWindow *window;
  GdkWindowAttr attributes;
  gint attributes_mask;

  placeholder = GLADE_PLACEHOLDER (widget);

  gtk_widget_set_realized (widget, TRUE);

  gtk_widget_get_allocation (widget, &allocation);
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask =
      gtk_widget_get_events (widget) |
      GDK_POINTER_MOTION_MASK |
      GDK_POINTER_MOTION_HINT_MASK |
      GDK_BUTTON_PRESS_MASK |
      GDK_BUTTON_RELEASE_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK;
  attributes_mask = GDK_WA_X | GDK_WA_Y;

  window = gtk_widget_get_parent_window (widget);
  gtk_widget_set_window (widget, g_object_ref (window));

  placeholder->priv->event_window =
      gdk_window_new (gtk_widget_get_parent_window (widget), &attributes,
                      attributes_mask);
  gdk_window_set_user_data (placeholder->priv->event_window, widget);
}

static void
glade_placeholder_unrealize (GtkWidget *widget)
{
  GladePlaceholder *placeholder;

  placeholder = GLADE_PLACEHOLDER (widget);

  if (placeholder->priv->event_window)
    {
      gdk_window_set_user_data (placeholder->priv->event_window, NULL);
      gdk_window_destroy (placeholder->priv->event_window);
      placeholder->priv->event_window = NULL;
    }

  GTK_WIDGET_CLASS (glade_placeholder_parent_class)->unrealize (widget);
}

static void
glade_placeholder_map (GtkWidget *widget)
{
  GladePlaceholder *placeholder;

  placeholder = GLADE_PLACEHOLDER (widget);

  if (placeholder->priv->event_window)
    {
      gdk_window_show (placeholder->priv->event_window);
    }

  GTK_WIDGET_CLASS (glade_placeholder_parent_class)->map (widget);
}

static void
glade_placeholder_unmap (GtkWidget *widget)
{
  GladePlaceholder *placeholder;

  placeholder = GLADE_PLACEHOLDER (widget);

  if (placeholder->priv->event_window)
    {
      gdk_window_hide (placeholder->priv->event_window);
    }

  GTK_WIDGET_CLASS (glade_placeholder_parent_class)->unmap (widget);
}

static void
glade_placeholder_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  GladePlaceholder *placeholder;

  placeholder = GLADE_PLACEHOLDER (widget);

  gtk_widget_set_allocation (widget, allocation);

  if (gtk_widget_get_realized (widget))
    {
      gdk_window_move_resize (placeholder->priv->event_window,
                              allocation->x, allocation->y,
                              allocation->width, allocation->height);
    }
}

static gboolean
glade_placeholder_draw (GtkWidget *widget, cairo_t *cr)
{
  GladePlaceholder *placeholder = GLADE_PLACEHOLDER (widget);
  gint h = gtk_widget_get_allocated_height (widget) - 1;
  gint w = gtk_widget_get_allocated_width (widget) - 1;

  if (placeholder_pattern)
    {
      cairo_save (cr);
      cairo_rectangle (cr, 0, 0, w, h);
      cairo_set_source (cr, placeholder_pattern);
      cairo_fill (cr);
      cairo_restore (cr);
    }

  cairo_translate (cr, .5, .5);
  cairo_set_line_width (cr, 1.0);

  /* We hardcode colors here since we are already using an image as bg pattern */
  cairo_set_source_rgb (cr, .9, .9, .9);
  cairo_move_to (cr, w, 0);
  cairo_line_to (cr, 0, 0);
  cairo_line_to (cr, 0, h);
  cairo_stroke (cr);

  cairo_set_source_rgb (cr, .64, .64, .64);
  cairo_move_to (cr, w, 0);
  cairo_line_to (cr, w, h);
  cairo_line_to (cr, 0, h);
  cairo_stroke (cr);

  if (placeholder->priv->drag_highlight)
    {
      cairo_pattern_t *gradient;
      GtkStyleContext *context;
      gdouble ww, hh;
      GdkRGBA c;

      context = gtk_widget_get_style_context (widget);
      gtk_style_context_save (context);
      gtk_style_context_get_background_color (context,
                                              gtk_style_context_get_state (context) |
                                              GTK_STATE_FLAG_SELECTED |
                                              GTK_STATE_FLAG_FOCUSED, &c);
      gtk_style_context_restore (context);

      ww = w/2.0;
      hh = h/2.0;
      gradient = cairo_pattern_create_radial (ww, hh, MIN (w, h)/6,
                                              ww, hh, MAX (ww, hh));
      cairo_pattern_add_color_stop_rgba (gradient, 0, c.red, c.green, c.blue, .08);
      cairo_pattern_add_color_stop_rgba (gradient, 1, c.red, c.green, c.blue, .28);

      cairo_set_source (cr, gradient);

      cairo_rectangle (cr, 0, 0, w, h);
      cairo_fill (cr);

      cairo_pattern_destroy (gradient);
    }

  return FALSE;
}

static void
glade_placeholder_update_cursor (GladePlaceholder *placeholder, GdkWindow *win)
{
  GladeProject *project = glade_placeholder_get_project (placeholder);
  GladePointerMode pointer_mode = glade_project_get_pointer_mode (project);

  if (pointer_mode == GLADE_POINTER_SELECT)
    glade_cursor_set (project, win, GLADE_CURSOR_SELECTOR);
  else if (pointer_mode == GLADE_POINTER_ADD_WIDGET)
    glade_cursor_set (project, win, GLADE_CURSOR_ADD_WIDGET);
}

static gboolean
glade_placeholder_enter_notify_event (GtkWidget        *widget, 
                                      GdkEventCrossing *event)
{
  glade_placeholder_update_cursor (GLADE_PLACEHOLDER (widget), event->window);
  return FALSE;
}

static gboolean
glade_placeholder_motion_notify_event (GtkWidget *widget, GdkEventMotion *event)
{
  glade_placeholder_update_cursor (GLADE_PLACEHOLDER (widget), event->window);
  return FALSE;
}

static void
on_chooser_adaptor_widget_selected (_GladeAdaptorChooserWidget *chooser,
                                    GladeWidgetAdaptor         *adaptor,
                                    GladePlaceholder           *placeholder)

{
  glade_command_create (adaptor, glade_placeholder_get_parent (placeholder),
                        placeholder, glade_placeholder_get_project (placeholder));
  gtk_widget_destroy (gtk_widget_get_ancestor (GTK_WIDGET (chooser), GTK_TYPE_POPOVER));
}

static GtkWidget *
glade_placeholder_popover_new (GladePlaceholder *placeholder, GtkWidget *relative_to)
{
  GtkWidget *pop = gtk_popover_new (relative_to);
  GtkWidget *chooser;

  chooser = _glade_adaptor_chooser_widget_new (GLADE_ADAPTOR_CHOOSER_WIDGET_WIDGET |
                                               GLADE_ADAPTOR_CHOOSER_WIDGET_SKIP_TOPLEVEL |
                                               GLADE_ADAPTOR_CHOOSER_WIDGET_SKIP_DEPRECATED,
                                               glade_placeholder_get_project (placeholder));
  _glade_adaptor_chooser_widget_populate (GLADE_ADAPTOR_CHOOSER_WIDGET (chooser));
  g_signal_connect (chooser, "adaptor-selected",
                    G_CALLBACK (on_chooser_adaptor_widget_selected),
                    placeholder);
  gtk_popover_set_position (GTK_POPOVER (pop), GTK_POS_BOTTOM);
  gtk_container_add (GTK_CONTAINER (pop), chooser);
  gtk_widget_show (chooser);

  return pop;
}

static gboolean
glade_placeholder_button_press (GtkWidget *widget, GdkEventButton *event)
{
  GladePlaceholder   *placeholder;
  GladeProject       *project;
  GladeWidgetAdaptor *adaptor;
  gboolean            handled = FALSE;

  g_return_val_if_fail (GLADE_IS_PLACEHOLDER (widget), FALSE);

  placeholder = GLADE_PLACEHOLDER (widget);
  project     = glade_placeholder_get_project (placeholder);
  adaptor     = glade_project_get_add_item (project);

  if (!gtk_widget_has_focus (widget))
    gtk_widget_grab_focus (widget);

  if (event->button == 1)
    {
      if (event->type == GDK_BUTTON_PRESS && adaptor != NULL)
        {
          GladeWidget *parent = glade_placeholder_get_parent (placeholder);

          /* A widget type is selected in the palette.
           * Add a new widget of that type.
           */
          glade_command_create (adaptor, parent, placeholder, project);

          glade_project_set_add_item (project, NULL);

          /* reset the cursor */
          glade_cursor_set (project, event->window, GLADE_CURSOR_SELECTOR);
          handled = TRUE;
        }
      else if (event->type == GDK_2BUTTON_PRESS && adaptor == NULL)
        {
          GtkWidget *event_widget = gtk_get_event_widget ((GdkEvent*) event);
          GladeWidget *toplevel = glade_widget_get_toplevel (glade_placeholder_get_parent (placeholder));
          GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (glade_widget_get_object (toplevel)));
          GtkWidget *pop = glade_placeholder_popover_new (placeholder, parent);
          GdkRectangle rect = {0, 0, 8, 8};

          gtk_widget_translate_coordinates (event_widget, parent,
                                            event->x, event->y,
                                            &rect.x, &rect.y);
          gtk_popover_set_pointing_to (GTK_POPOVER (pop), &rect);
          gtk_widget_show (pop);
          handled = TRUE;
        }
    }

  if (!handled && glade_popup_is_popup_event (event))
    {
      glade_popup_placeholder_pop (placeholder, event);
      handled = TRUE;
    }

  return handled;
}

static gboolean
glade_placeholder_popup_menu (GtkWidget *widget)
{
  g_return_val_if_fail (GLADE_IS_PLACEHOLDER (widget), FALSE);

  glade_popup_placeholder_pop (GLADE_PLACEHOLDER (widget), NULL);

  return TRUE;
}

static gboolean
glade_placeholder_drag_can_drag (_GladeDrag *source)
{
  GladeWidget *parent = glade_placeholder_get_parent (GLADE_PLACEHOLDER (source));
  return (parent) ? _glade_drag_can_drag (GLADE_DRAG (parent)) : FALSE;
}

static gboolean
glade_placeholder_drag_can_drop (_GladeDrag *dest, gint x, gint y, GObject *data)
{
  if (GLADE_IS_WIDGET_ADAPTOR (data))
    {
      GType otype = glade_widget_adaptor_get_object_type (GLADE_WIDGET_ADAPTOR (data));

      if (g_type_is_a (otype, GTK_TYPE_WIDGET) && !GWA_IS_TOPLEVEL (data))
        return TRUE;
    }
  else if (GTK_IS_WIDGET (data))
    {
      GladeWidget *parent, *new_child;

      /* Avoid recursion */
      if (gtk_widget_is_ancestor (GTK_WIDGET (dest), GTK_WIDGET (data)))
        return FALSE;

      parent = glade_placeholder_get_parent (GLADE_PLACEHOLDER (dest));

      if ((new_child = glade_widget_get_from_gobject (data)) &&
          !glade_widget_add_verify (parent, new_child, FALSE))
        return FALSE;

      return TRUE;
    }

  return FALSE;
}

static gboolean
glade_placeholder_drag_drop (_GladeDrag *dest, gint x, gint y, GObject *data)
{
  GladePlaceholder *placeholder = GLADE_PLACEHOLDER (dest);
  GladeWidget *gsource;

  if (!data)
    return FALSE;
  
  if (GLADE_IS_WIDGET_ADAPTOR (data))
    {
      GladeWidget *parent = glade_placeholder_get_parent (placeholder);
      
      glade_command_create (GLADE_WIDGET_ADAPTOR (data), parent, placeholder, 
                            glade_widget_get_project (parent));
      return TRUE;
    }
  else if ((gsource = glade_widget_get_from_gobject (data)))
    {
      GladeWidget *parent = glade_placeholder_get_parent (placeholder);
      GList widgets = {gsource, NULL, NULL};

      /* Check for recursive paste */
      if (parent != gsource)
        {
          glade_command_dnd (&widgets, parent, placeholder);
          return TRUE;
        }
    }

  return FALSE;
}

static void
glade_placeholder_drag_highlight (_GladeDrag *dest, gint x, gint y)
{
  GladePlaceholderPrivate *priv = GLADE_PLACEHOLDER (dest)->priv;
  gboolean highlight = !(x < 0 || y < 0);

  if (priv->drag_highlight == highlight)
    return;

  priv->drag_highlight = highlight;
  gtk_widget_queue_draw (GTK_WIDGET (dest));
}

static void
glade_placeholder_drag_init (_GladeDragInterface *iface)
{
  iface->can_drag = glade_placeholder_drag_can_drag;
  iface->can_drop = glade_placeholder_drag_can_drop;
  iface->drop = glade_placeholder_drag_drop;
  iface->highlight = glade_placeholder_drag_highlight;
}

static void
glade_placeholder_class_init (GladePlaceholderClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  gchar *path;
  cairo_surface_t *surface;

  object_class->finalize = glade_placeholder_finalize;
  object_class->set_property = glade_placeholder_set_property;
  object_class->get_property = glade_placeholder_get_property;

  widget_class->realize = glade_placeholder_realize;
  widget_class->unrealize = glade_placeholder_unrealize;
  widget_class->map = glade_placeholder_map;
  widget_class->unmap = glade_placeholder_unmap;
  widget_class->size_allocate = glade_placeholder_size_allocate;
  widget_class->draw = glade_placeholder_draw;
  widget_class->enter_notify_event = glade_placeholder_enter_notify_event;
  widget_class->motion_notify_event = glade_placeholder_motion_notify_event;
  widget_class->button_press_event = glade_placeholder_button_press;
  widget_class->popup_menu = glade_placeholder_popup_menu;
  
  /* GtkScrollable implementation */
  g_object_class_override_property (object_class, PROP_HADJUSTMENT,
                                    "hadjustment");
  g_object_class_override_property (object_class, PROP_VADJUSTMENT,
                                    "vadjustment");
  g_object_class_override_property (object_class, PROP_HSCROLL_POLICY,
                                    "hscroll-policy");
  g_object_class_override_property (object_class, PROP_VSCROLL_POLICY,
                                    "vscroll-policy");

  /* Create our tiled background pattern */
  path = g_build_filename (glade_app_get_pixmaps_dir (), "placeholder.png", NULL);
  surface = cairo_image_surface_create_from_png (path);

  if (!surface)
    g_warning ("Failed to create surface for %s\n", path);
  else
    {
      placeholder_pattern = cairo_pattern_create_for_surface (surface);
      cairo_pattern_set_extend (placeholder_pattern, CAIRO_EXTEND_REPEAT);
    }
  g_free (path);
}

/**
 * glade_placeholder_new:
 * 
 * Returns: a new #GladePlaceholder cast as a #GtkWidget
 */
GtkWidget *
glade_placeholder_new (void)
{
  return g_object_new (GLADE_TYPE_PLACEHOLDER, NULL);
}

GladeProject *
glade_placeholder_get_project (GladePlaceholder *placeholder)
{
  GladeWidget *parent;
  parent = glade_placeholder_get_parent (placeholder);
  return parent ? glade_widget_get_project (parent) : NULL;
}

GladeWidget *
glade_placeholder_get_parent (GladePlaceholder *placeholder)
{
  GtkWidget *widget;
  GladeWidget *parent = NULL;

  g_return_val_if_fail (GLADE_IS_PLACEHOLDER (placeholder), NULL);

  for (widget = gtk_widget_get_parent (GTK_WIDGET (placeholder));
       widget != NULL; widget = gtk_widget_get_parent (widget))
    {
      if ((parent = glade_widget_get_from_gobject (widget)) != NULL)
        break;
    }
  return parent;
}

GList *
glade_placeholder_packing_actions (GladePlaceholder *placeholder)
{
  g_return_val_if_fail (GLADE_IS_PLACEHOLDER (placeholder), NULL);

  return placeholder->priv->packing_actions;
}

