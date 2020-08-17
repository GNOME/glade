/*
 * glade-fixed.c - A GladeWidget derivative object wrapper designed to
 *                 handle free-form child placement for containers such as 
 *                 GtkFixed and GtkLayout.
 *
 * Copyright (C) 2006, 2013 Tristan Van Berkom.
 *
 * Author(s):
 *      Tristan Van Berkom <tvb@gnome.org>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 * SECTION:glade-fixed
 * @Short_Description: An object wrapper for free-form placement container widgets.
 *
 * #GladeFixed is a specialized #GladeWidget to handle free-form child
 * placements in containers that support this, it is designed with properties
 * and signals with flexable integration in mind.
 *
 * If you set the x-prop/y-prop/width-prop/height-prop properties and
 * leave the signals alone, #GladeFixed will assume you are like a 
 * GtkFixed/GtkLayout widget and will use pixel counts as units for
 * these properties.
 *
 * If you handle the configure-child/configure-end[/configure-begin] signals
 * and dont let them propagate to the GladeFixed, then the x-prop/y-prop/width-prop/height-prop
 * properties will be completely ignored and it is up to the implementor to play
 * with whatever child packing properties are available to make a closest match
 * for the values passed to configure-child via the #GdkRectangle.
 */

#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#include <gladeui/glade.h>
#include <gladeui/glade-marshallers.h>
#include <gladeui/glade-popup.h>
#include "glade-gtk-marshallers.h"
#include "glade-fixed.h"

/* properties */
enum
{
  PROP_0,
  PROP_X_PROP,
  PROP_Y_PROP,
  PROP_WIDTH_PROP,
  PROP_HEIGHT_PROP,
  PROP_CAN_RESIZE
};

/* signals */
enum
{
  CONFIGURE_CHILD,
  CONFIGURE_BEGIN,
  CONFIGURE_END,
  FIXED_SIGNALS
};

typedef struct
{
  gulong press_id;
  gulong release_id;
  gulong motion_id;
} GFSigData;

#define CHILD_WIDTH_MIN    20
#define CHILD_HEIGHT_MIN   20
#define CHILD_WIDTH_DEF    100
#define CHILD_HEIGHT_DEF   80

#define GRAB_BORDER_WIDTH  7
#define GRAB_CORNER_WIDTH  7

static guint glade_fixed_signals[FIXED_SIGNALS];

G_DEFINE_TYPE (GladeFixed, glade_fixed, GLADE_TYPE_WIDGET)

/* From gtkmain.c */
static gboolean
glade_fixed_boolean_handled_accumulator (GSignalInvocationHint * ihint,
                                         GValue * return_accu,
                                         const GValue * handler_return,
                                         gpointer dummy)
{
  gboolean continue_emission;
  gboolean signal_handled;

  signal_handled = g_value_get_boolean (handler_return);
  g_value_set_boolean (return_accu, signal_handled);
  continue_emission = !signal_handled;

  return continue_emission;
}

/*******************************************************************************
                             Static helper routines
 *******************************************************************************/
typedef enum {
  HEXPAND_SET_FLAG = (1 << 0),
  VEXPAND_SET_FLAG = (1 << 1),
  HEXPAND_FLAG = (1 << 2),
  VEXPAND_FLAG = (1 << 3),
} ExpandData;

static void
save_expand_data (GtkWidget *widget)
{
  guint    flags;
  gboolean h_expand;
  gboolean v_expand;
  gboolean h_expand_set;
  gboolean v_expand_set;

  g_object_get (G_OBJECT (widget),
                "hexpand-set", &h_expand_set,
                "vexpand-set", &v_expand_set,
                "hexpand", &h_expand,
                "vexpand", &v_expand,
                NULL);

  flags = 
    (h_expand_set ? HEXPAND_SET_FLAG : 0) |
    (v_expand_set ? VEXPAND_SET_FLAG : 0) |
    (h_expand ? HEXPAND_FLAG : 0) |
    (v_expand ? VEXPAND_FLAG : 0);

  g_object_set_data (G_OBJECT (widget), "glade-gtk-box-child-expand", GUINT_TO_POINTER (flags));
}

static void
restore_expand_data (GtkWidget *widget)
{
  guint    flags;

  flags = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (widget), "glade-gtk-box-child-expand"));

  g_object_set (G_OBJECT (widget),
                "hexpand",     ((flags & HEXPAND_FLAG) != 0),
                "vexpand",     ((flags & VEXPAND_FLAG) != 0),
                "hexpand-set", ((flags & HEXPAND_SET_FLAG) != 0),
                "vexpand-set", ((flags & VEXPAND_SET_FLAG) != 0),
                NULL);
}

static void
expand_all_children (GladeWidget *widget)
{
  GList *children, *l;

  children = glade_widget_get_children (widget);

  for (l = children; l; l = l->next)
    {
      GtkWidget *child = l->data;

      save_expand_data (child);

      gtk_widget_set_hexpand (child, TRUE);
      gtk_widget_set_vexpand (child, TRUE);
    }
}

static void
restore_all_children (GladeWidget *widget)
{
  GList *children, *l;

  children = glade_widget_get_children (widget);

  for (l = children; l; l = l->next)
    {
      GtkWidget *child = l->data;

      restore_expand_data (child);
    }
}


static GladeCursorType
glade_fixed_get_operation (GtkWidget *widget, gint x, gint y)
{
  GladeCursorType operation = GLADE_CURSOR_DRAG;
  GtkAllocation allocation;

#if 0
  g_print ("%s called (width %d height %d x %d y %d)\n",
           __FUNCTION__,
           widget->allocation.width, widget->allocation.height, x, y);
#endif

  gtk_widget_get_allocation (widget, &allocation);
  if (x < GRAB_BORDER_WIDTH)
    {
      if (y < GRAB_BORDER_WIDTH)
        operation = GLADE_CURSOR_RESIZE_TOP_LEFT;
      else if (y > allocation.height - GRAB_BORDER_WIDTH)
        operation = GLADE_CURSOR_RESIZE_BOTTOM_LEFT;
      else
        operation = GLADE_CURSOR_RESIZE_LEFT;
    }
  else if (x > allocation.width - GRAB_BORDER_WIDTH)
    {
      if (y < GRAB_BORDER_WIDTH)
        operation = GLADE_CURSOR_RESIZE_TOP_RIGHT;
      else if (y > allocation.height - GRAB_BORDER_WIDTH)
        operation = GLADE_CURSOR_RESIZE_BOTTOM_RIGHT;
      else
        operation = GLADE_CURSOR_RESIZE_RIGHT;
    }
  else if (y < GRAB_BORDER_WIDTH)
    {
      if (x < GRAB_BORDER_WIDTH)
        operation = GLADE_CURSOR_RESIZE_TOP_LEFT;
      else if (x > allocation.width - GRAB_BORDER_WIDTH)
        operation = GLADE_CURSOR_RESIZE_TOP_RIGHT;
      else
        operation = GLADE_CURSOR_RESIZE_TOP;
    }
  else if (y > allocation.height - GRAB_BORDER_WIDTH)
    {
      if (x < GRAB_BORDER_WIDTH)
        operation = GLADE_CURSOR_RESIZE_BOTTOM_LEFT;
      else if (x > allocation.width - GRAB_BORDER_WIDTH)
        operation = GLADE_CURSOR_RESIZE_BOTTOM_RIGHT;
      else
        operation = GLADE_CURSOR_RESIZE_BOTTOM;
    }
  return operation;
}

static void
glade_fixed_save_state (GladeFixed *fixed, GladeWidget *child, GdkDevice *device)
{
  GtkAllocation allocation;
  GtkWidget    *widget;
  GtkWidget    *child_widget;

  widget       = GTK_WIDGET (glade_widget_get_object (GLADE_WIDGET (fixed)));
  child_widget = GTK_WIDGET (glade_widget_get_object (child));

  glade_utils_get_pointer (widget, gtk_widget_get_window (widget), device,
                           &(GLADE_FIXED (fixed)->pointer_x_origin),
                           &(GLADE_FIXED (fixed)->pointer_y_origin));

  gtk_widget_translate_coordinates (child_widget, widget, 0, 0,
                                    &(fixed->child_x_origin),
                                    &(fixed->child_y_origin));

  gtk_widget_get_allocation (child_widget, &allocation);
  fixed->child_width_origin = allocation.width;
  fixed->child_height_origin = allocation.height;

  fixed->pointer_x_child_origin =
      fixed->pointer_x_origin - fixed->child_x_origin;
  fixed->pointer_y_child_origin =
      fixed->pointer_y_origin - fixed->child_y_origin;
}


static void
glade_fixed_filter_event (GladeFixed *fixed,
                          gint *x, gint *y,
                          gint left, gint right,
                          gint top, gint bottom)
{
  GtkAllocation allocation;
  gint cont_width, cont_height;
  GtkWidget    *widget;

  g_return_if_fail (x && y);

  widget = GTK_WIDGET (glade_widget_get_object (GLADE_WIDGET (fixed)));

  gtk_widget_get_allocation (widget, &allocation);
  cont_width = allocation.width;
  cont_height = allocation.height;

  /* Clip out mouse events that are outside the window.
   */
  if ((left || fixed->operation == GLADE_CURSOR_DRAG) &&
      *x - fixed->pointer_x_child_origin < 0)
    *x = fixed->pointer_x_child_origin;
  if ((top || fixed->operation == GLADE_CURSOR_DRAG) &&
      *y - fixed->pointer_y_child_origin < 0)
    *y = fixed->pointer_y_child_origin;

  if ((right || fixed->operation == GLADE_CURSOR_DRAG) &&
      *x + (fixed->child_width_origin -
            fixed->pointer_x_child_origin) > cont_width)
    *x = cont_width - (fixed->child_width_origin -
                       fixed->pointer_x_child_origin);
  if ((bottom || fixed->operation == GLADE_CURSOR_DRAG) &&
      *y + (fixed->child_height_origin -
            fixed->pointer_y_child_origin) > cont_height)
    *y = cont_height - (fixed->child_height_origin -
                        fixed->pointer_y_child_origin);

  /* Clip out mouse events that mean shrinking to less than 0.
   */
  if (left &&
      (*x - fixed->pointer_x_child_origin) >
      (fixed->child_x_origin + (fixed->child_width_origin - CHILD_WIDTH_MIN)))
    {
      *x = (fixed->child_x_origin +
            (fixed->child_width_origin - CHILD_WIDTH_MIN)) -
          fixed->pointer_x_child_origin;
    }
  else if (right &&
           (*x - fixed->pointer_x_child_origin) <
           fixed->child_x_origin - (fixed->child_width_origin +
                                    CHILD_WIDTH_MIN))
    {
      *x = (fixed->child_x_origin -
            (fixed->child_width_origin + CHILD_WIDTH_MIN)) +
          fixed->pointer_x_child_origin;
    }


  if (top &&
      (*y - fixed->pointer_y_child_origin) >
      (fixed->child_y_origin + (fixed->child_height_origin - CHILD_HEIGHT_MIN)))
    {
      *y = (fixed->child_y_origin +
            (fixed->child_height_origin - CHILD_HEIGHT_MIN)) -
          fixed->pointer_y_child_origin;
    }
  else if (bottom &&
           (*y - fixed->pointer_y_child_origin) <
           fixed->child_y_origin - (fixed->child_height_origin +
                                    CHILD_HEIGHT_MIN))
    {
      *y = (fixed->child_y_origin -
            (fixed->child_height_origin + CHILD_HEIGHT_MIN)) +
          fixed->pointer_y_child_origin;
    }
}

static void
glade_fixed_handle_swindow (GladeFixed *fixed, GdkRectangle *area)
{
  GtkWidget *swindow = NULL, *swindow_child = NULL, *widget;
  GtkAdjustment *hadj, *vadj;
  GtkAllocation child_allocation;
  GtkWidget *fixed_widget;
  GladeWidget *gwidget;
  gint x, y;

  widget = fixed_widget = GTK_WIDGET (glade_widget_get_object (GLADE_WIDGET (fixed)));

  while (widget && !GTK_IS_SCROLLED_WINDOW (widget) &&
         (
          ((gwidget = glade_widget_get_from_gobject (widget)) &&
           glade_widget_get_parent (gwidget)) ||
          !gwidget))
    {
      if (!GTK_IS_VIEWPORT (widget))
        swindow_child = widget;

      if (GTK_IS_SCROLLED_WINDOW (widget))
        {
          swindow = widget;
          break;
        }

      widget = gtk_widget_get_parent (widget);
    }

  if (swindow)
    {
      /* Set the adjustments to use appropriate pixel units and then
       * square in on the target drop area.
       */
      hadj =
          gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (swindow));
      vadj =
          gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (swindow));
      gtk_widget_get_allocation (swindow_child, &child_allocation);

      g_object_set (G_OBJECT (hadj),
                    "lower", 0.0F,
                    "upper", (gdouble) child_allocation.width + 0.0, NULL);

      g_object_set (G_OBJECT (vadj),
                    "lower", 0.0F,
                    "upper", (gdouble) child_allocation.height + 0.0, NULL);

      gtk_widget_translate_coordinates (fixed_widget,
                                        swindow_child,
                                        area->x, area->y, &x, &y);

      gtk_adjustment_clamp_page (hadj, x, x + area->width);
      gtk_adjustment_clamp_page (vadj, y, y + area->height);
    }
}

static void
glade_fixed_configure_widget (GladeFixed *fixed,
                              GladeWidget *child,
                              GdkDevice *device)
{
  GladeWidget *gwidget = GLADE_WIDGET (fixed);
  GtkWidget   *widget;
  GdkRectangle new_area;
  gboolean handled, right, left, top, bottom;
  gint x, y;

  widget = GTK_WIDGET (glade_widget_get_object (gwidget));

  glade_utils_get_pointer (widget, gtk_widget_get_window (widget), device, &x, &y);

  right = GLADE_FIXED_CURSOR_RIGHT (fixed->operation);
  left = GLADE_FIXED_CURSOR_LEFT (fixed->operation);
  top = GLADE_FIXED_CURSOR_TOP (fixed->operation);
  bottom = GLADE_FIXED_CURSOR_BOTTOM (fixed->operation);

  /* Filter out events that make your widget go out of bounds */
  glade_fixed_filter_event (fixed, &x, &y, left, right, top, bottom);

  new_area.x = fixed->child_x_origin;
  new_area.y = fixed->child_y_origin;
  new_area.width = fixed->child_width_origin;
  new_area.height = fixed->child_height_origin;

  /* Modify current size.
   */
  if (fixed->operation == GLADE_CURSOR_DRAG)
    {
      /* Move widget */
      new_area.x = fixed->child_x_origin + x - fixed->pointer_x_origin;
      new_area.y = fixed->child_y_origin + y - fixed->pointer_y_origin;

    }
  else
    {

      if (bottom)
        {
          new_area.height =
              fixed->child_height_origin + (y - fixed->pointer_y_origin);
        }
      else if (top)
        {

          new_area.height =
              fixed->child_height_origin - (y - fixed->pointer_y_origin);
          new_area.y = fixed->child_y_origin + (y - fixed->pointer_y_origin);
        }

      if (right)
        {
          new_area.width =
              fixed->child_width_origin + (x - fixed->pointer_x_origin);
        }
      else if (left)
        {
          new_area.width =
              fixed->child_width_origin - (x - fixed->pointer_x_origin);
          new_area.x = fixed->child_x_origin + (x - fixed->pointer_x_origin);
        }
    }

  /* Trim */
  if (new_area.width < CHILD_WIDTH_MIN)
    new_area.width = CHILD_WIDTH_MIN;
  if (new_area.height < CHILD_WIDTH_MIN)
    new_area.height = CHILD_HEIGHT_MIN;

  /* before configuring the child widget, make make sure the target child
   * widget area is visible if the GladeFixed is placed inside a scrolled 
   * window or a viewport inside a scrolled window.
   */
  glade_fixed_handle_swindow (fixed, &new_area);

  /* Apply new rectangle to the object */
  g_signal_emit (G_OBJECT (fixed),
                 glade_fixed_signals[CONFIGURE_CHILD],
                 0, child, &new_area, &handled);

  /* Correct glitches when some widgets are draged over others */
  gtk_widget_queue_draw (widget);
}

static void
glade_fixed_disconnect_child (GladeFixed *fixed, GladeWidget *child)
{
  GFSigData *data;

  if (GTK_IS_WIDGET (glade_widget_get_object (child)) == FALSE)
    return;

  if ((data =
       g_object_get_data (G_OBJECT (child), "glade-fixed-signal-data")) != NULL)
    {
      g_signal_handler_disconnect (child, data->press_id);
      g_signal_handler_disconnect (child, data->release_id);
      g_signal_handler_disconnect (child, data->motion_id);

      g_object_set_data (G_OBJECT (child), "glade-fixed-signal-data", NULL);
    }
}

static void
glade_fixed_connect_child (GladeFixed *fixed, GladeWidget *child)
{
  GFSigData *data;

  if (GTK_IS_WIDGET (glade_widget_get_object (child)) == FALSE)
    return;

  if ((data =
       g_object_get_data (G_OBJECT (child), "glade-fixed-signal-data")) != NULL)
    glade_fixed_disconnect_child (fixed, child);

  data = g_new (GFSigData, 1);

  /* Connect-after here... leave a chance for selection
   */
  data->press_id =
      g_signal_connect_after
      (child, "button-press-event", G_CALLBACK
       (GLADE_FIXED_GET_CLASS (fixed)->child_event), fixed);
  data->release_id =
      g_signal_connect
      (child, "button-release-event", G_CALLBACK
       (GLADE_FIXED_GET_CLASS (fixed)->child_event), fixed);
  data->motion_id =
      g_signal_connect
      (child, "motion-notify-event", G_CALLBACK
       (GLADE_FIXED_GET_CLASS (fixed)->child_event), fixed);


  g_object_set_data_full (G_OBJECT (child), "glade-fixed-signal-data",
                          data, g_free);
}

/*******************************************************************************
                               GladeFixedClass
 *******************************************************************************/
static gboolean
glade_fixed_configure_child_impl (GladeFixed *fixed,
                                  GladeWidget *child,
                                  GdkRectangle *rect)
{
  /* Make sure we can modify these properties
   *
   * FIXME: This is inappropriate how that enabled state of properties
   * is undoable... instead there should be some adaptor hooks to allow
   * adding commands to the command group which adds a child to
   * the given widget... where we can undoably override this.
   */
  glade_widget_pack_property_set_enabled (child, fixed->x_prop, TRUE);
  glade_widget_pack_property_set_enabled (child, fixed->y_prop, TRUE);
  glade_widget_property_set_enabled (child, fixed->width_prop, TRUE);
  glade_widget_property_set_enabled (child, fixed->height_prop, TRUE);

  glade_widget_pack_property_set (child, fixed->x_prop, rect->x);
  glade_widget_pack_property_set (child, fixed->y_prop, rect->y);
  glade_widget_property_set (child, fixed->width_prop, rect->width);
  glade_widget_property_set (child, fixed->height_prop, rect->height);
  return TRUE;
}


static gboolean
glade_fixed_configure_end_impl (GladeFixed *fixed, GladeWidget *child)
{
  GValue x_value = { 0, };
  GValue y_value = { 0, };
  GValue width_value = { 0, };
  GValue height_value = { 0, };
  GValue new_x_value = { 0, };
  GValue new_y_value = { 0, };
  GValue new_width_value = { 0, };
  GValue new_height_value = { 0, };
  GladeProperty *x_prop, *y_prop, *width_prop, *height_prop;

  x_prop = glade_widget_get_pack_property (child, fixed->x_prop);
  y_prop = glade_widget_get_pack_property (child, fixed->y_prop);
  width_prop = glade_widget_get_property (child, fixed->width_prop);
  height_prop = glade_widget_get_property (child, fixed->height_prop);

  g_return_val_if_fail (GLADE_IS_PROPERTY (x_prop), FALSE);
  g_return_val_if_fail (GLADE_IS_PROPERTY (y_prop), FALSE);
  g_return_val_if_fail (GLADE_IS_PROPERTY (width_prop), FALSE);
  g_return_val_if_fail (GLADE_IS_PROPERTY (height_prop), FALSE);

  g_value_init (&x_value, G_TYPE_INT);
  g_value_init (&y_value, G_TYPE_INT);
  g_value_init (&width_value, G_TYPE_INT);
  g_value_init (&height_value, G_TYPE_INT);

  glade_property_get_value (x_prop, &new_x_value);
  glade_property_get_value (y_prop, &new_y_value);
  glade_property_get_value (width_prop, &new_width_value);
  glade_property_get_value (height_prop, &new_height_value);

  g_value_set_int (&x_value, fixed->child_x_origin);
  g_value_set_int (&y_value, fixed->child_y_origin);
  g_value_set_int (&width_value, fixed->child_width_origin);
  g_value_set_int (&height_value, fixed->child_height_origin);

  glade_command_push_group (_("Placing %s inside %s"),
                            glade_widget_get_name (child), 
                            glade_widget_get_name (GLADE_WIDGET (fixed)));

  /* whew, all that for this call !
   */
  glade_command_set_properties (x_prop, &x_value, &new_x_value,
                                y_prop, &y_value, &new_y_value,
                                width_prop, &width_value, &new_width_value,
                                height_prop, &height_value, &new_height_value,
                                NULL);

  glade_command_pop_group ();

  g_value_unset (&x_value);
  g_value_unset (&y_value);
  g_value_unset (&width_value);
  g_value_unset (&height_value);
  g_value_unset (&new_x_value);
  g_value_unset (&new_y_value);
  g_value_unset (&new_width_value);
  g_value_unset (&new_height_value);

  return TRUE;
}


static void
glade_fixed_cancel_operation (GladeFixed *fixed, GladeCursorType new_operation)
{
  gboolean handled;

  g_signal_emit (G_OBJECT (fixed),
                 glade_fixed_signals[CONFIGURE_END],
                 0, fixed->configuring, &handled);

  restore_all_children (GLADE_WIDGET (fixed));

  /* Leave the machine state intact untill after
   * the user handled signal. 
   */
  fixed->operation = new_operation;
  fixed->configuring = NULL;
}

static gboolean
glade_fixed_handle_child_event (GladeFixed *fixed,
                                GladeWidget *child,
                                GtkWidget *event_widget,
                                GdkEvent *event)
{
  GladeCursorType operation;
  GdkModifierType event_state = 0;
  GladePointerMode pointer_mode;
  gboolean handled = FALSE, sig_handled;
  GladeProject *project = glade_widget_get_project (GLADE_WIDGET (fixed));
  GdkWindow *window = event->any.window;
  GtkWidget *fixed_widget, *child_widget;

  fixed_widget = GTK_WIDGET (glade_widget_get_object (GLADE_WIDGET (fixed)));
  child_widget = GTK_WIDGET (glade_widget_get_object (child));

  pointer_mode = glade_project_get_pointer_mode (project);

  if (fixed->can_resize)
    {
      gint fixed_x, fixed_y, child_x, child_y;
      GdkDevice *device;

      device = gdk_event_get_device (event);
      
      glade_utils_get_pointer (fixed_widget,
                               window,
                               device, &fixed_x, &fixed_y);

      /* Container widgets are trustable to have widget->window occupying
       * the entire widget allocation (gtk_widget_get_pointer broken on GtkEntry).
       */
      gtk_widget_translate_coordinates (fixed_widget,
                                        child_widget,
                                        fixed_x, fixed_y, &child_x, &child_y);
      
      operation = glade_fixed_get_operation (child_widget, child_x, child_y);
    }
  else
    operation = GLADE_CURSOR_DRAG;

  gdk_event_get_state (event, &event_state);

  switch (event->type)
    {
      case GDK_MOTION_NOTIFY:
        if (fixed->configuring == NULL)
          {
            if ((event_state & GDK_SHIFT_MASK) ||
                pointer_mode == GLADE_POINTER_DRAG_RESIZE)
              glade_cursor_set (project, window, operation);
            else if (pointer_mode == GLADE_POINTER_SELECT)
              glade_cursor_set (project, window, GLADE_CURSOR_SELECTOR);
          }
        else if (fixed->configuring && !(event_state & GDK_BUTTON1_MASK))
          {
            /* Cancel drags that no longer have mouse down */
            glade_cursor_set (project, window, operation);

            glade_fixed_cancel_operation (fixed, operation);
            handled = TRUE;
          }
        else if (fixed->configuring)
          {
            /* Need to update mouse for configures. */
            glade_utils_get_pointer (fixed_widget,
                                     window, gdk_event_get_device (event),
                                     &fixed->mouse_x, &fixed->mouse_y);

            glade_fixed_configure_widget (fixed, child, event->motion.device);
            glade_cursor_set (project, window, fixed->operation);
            handled = TRUE;
          }
        break;
      case GDK_BUTTON_PRESS:
        /* We cant rely on GDK_BUTTON1_MASK since event->state isnt yet updated
         * by the current event itself 
         */
        if (event->button.button == 1 &&
            ((event_state & GDK_SHIFT_MASK) ||
             pointer_mode == GLADE_POINTER_DRAG_RESIZE))
          {


            expand_all_children (GLADE_WIDGET (fixed));

            /* Spin the main loop so that the GladeFixed
             * widget gets reallocated before storing
             * the allocation sizes
             */
            while (gtk_events_pending ())
              gtk_main_iteration ();




            fixed->configuring = child;
            /* Save widget allocation and pointer pos */
            glade_fixed_save_state (fixed, child, event->button.device);

            fixed->operation = operation;
            glade_cursor_set (project, window, fixed->operation);

            g_signal_emit (G_OBJECT (fixed),
                           glade_fixed_signals[CONFIGURE_BEGIN],
                           0, child, &sig_handled);

            handled = TRUE;
          }
        break;
      case GDK_BUTTON_RELEASE:
        if (event->button.button == 1 && fixed->configuring)
          {
            if ((event_state & GDK_SHIFT_MASK) ||
                pointer_mode == GLADE_POINTER_DRAG_RESIZE)
              glade_cursor_set (project, window, operation);
            else
              glade_cursor_set (project, window, GLADE_CURSOR_SELECTOR);

            glade_fixed_cancel_operation (fixed, operation);
            handled = TRUE;
          }
        break;
      default:
        break;
    }
  return handled;
}

static gint
glade_fixed_child_event (GladeWidget *gwidget,
                         GdkEvent *event,
                         GladeFixed *fixed)
{
  GtkWidget *event_widget;
  GladeProject *project = glade_widget_get_project (gwidget);

  /* Get the basic event info... */
  gdk_window_get_user_data (((GdkEventAny *) event)->window,
                            (gpointer) & event_widget);

  /* Skip all this choosyness if we're already in a drag/resize
   */
  if (fixed->configuring)
    {
      return glade_fixed_handle_child_event
          (fixed, fixed->configuring, event_widget, event);
    }

  g_return_val_if_fail (GLADE_IS_WIDGET (gwidget), FALSE);

  /* Early return for fixed children with selection in
   * the palette.
   */
  if (GLADE_IS_FIXED (gwidget) &&
      glade_project_get_pointer_mode (project) == GLADE_POINTER_ADD_WIDGET)
    {
      glade_cursor_set (project, ((GdkEventAny *) event)->window,
                        GLADE_CURSOR_ADD_WIDGET);
      return FALSE;
    }

  return glade_fixed_handle_child_event (fixed, gwidget, event_widget, event);

}

/*******************************************************************************
                               GladeWidgetClass
 *******************************************************************************/
static void
glade_fixed_add_child_impl (GladeWidget *gwidget_fixed,
                            GladeWidget *child,
                            gboolean     at_mouse)
{
  GladeFixed *fixed = GLADE_FIXED (gwidget_fixed);
  GtkAllocation allocation;
  GdkRectangle rect;
  gboolean handled;
  GtkWidget *widget;

  g_return_if_fail (GLADE_IS_FIXED (fixed));
  g_return_if_fail (GLADE_IS_WIDGET (child));

  /* Need to explicitly find the pointer location at drag_drop time (but check if the widget has
   * a window already first, if not it's because we're loading and widget's arent realized yet)
   */
  widget = GTK_WIDGET (glade_widget_get_object (gwidget_fixed));
  if (gtk_widget_get_window (widget))
    glade_utils_get_pointer (widget, NULL, NULL,
                             &fixed->mouse_x, &fixed->mouse_y);

  /* Chain up for the basic parenting */
  GLADE_WIDGET_CLASS (glade_fixed_parent_class)->add_child
      (GLADE_WIDGET (fixed), child, at_mouse);

  /* We only operate on widgets here */
  if (!GTK_IS_WIDGET (glade_widget_get_object (child)))
    return;

  gtk_widget_add_events (GTK_WIDGET (glade_widget_get_object (child)),
                         GDK_POINTER_MOTION_MASK |
                         GDK_POINTER_MOTION_HINT_MASK |
                         GDK_BUTTON_PRESS_MASK |
                         GDK_BUTTON_RELEASE_MASK | GDK_ENTER_NOTIFY_MASK);

  glade_fixed_connect_child (fixed, child);

  /* Setup rect and send configure
   */
  if (fixed->creating)
    {
      rect.x = fixed->mouse_x;
      rect.y = fixed->mouse_y;
      rect.width = CHILD_WIDTH_DEF;
      rect.height = CHILD_HEIGHT_DEF;

      g_signal_emit (G_OBJECT (fixed),
                     glade_fixed_signals[CONFIGURE_CHILD],
                     0, child, &rect, &handled);
    }
  else if (at_mouse)
    {
      gtk_widget_get_allocation (GTK_WIDGET (glade_widget_get_object (child)), &allocation);
      rect.x = fixed->mouse_x;
      rect.y = fixed->mouse_y;
      rect.width = allocation.width;
      rect.height = allocation.height;

      if (rect.width < CHILD_WIDTH_DEF)
        rect.width = CHILD_WIDTH_DEF;

      if (rect.height < CHILD_HEIGHT_DEF)
        rect.height = CHILD_HEIGHT_DEF;

      g_signal_emit (G_OBJECT (fixed),
                     glade_fixed_signals[CONFIGURE_CHILD],
                     0, child, &rect, &handled);
    }
  return;
}

static void
glade_fixed_remove_child_impl (GladeWidget *fixed, GladeWidget *child)
{
  glade_fixed_disconnect_child (GLADE_FIXED (fixed), child);

  /* Chain up for the basic unparenting */
  GLADE_WIDGET_CLASS (glade_fixed_parent_class)->remove_child (GLADE_WIDGET (fixed), child);
}

static void
glade_fixed_replace_child_impl (GladeWidget *fixed,
                                GObject *old_object,
                                GObject *new_object)
{
  GladeWidget *gnew_widget = glade_widget_get_from_gobject (new_object);
  GladeWidget *gold_widget = glade_widget_get_from_gobject (old_object);

  if (gold_widget)
    glade_fixed_disconnect_child (GLADE_FIXED (fixed), gold_widget);

  /* Chain up for the basic reparenting */
  GLADE_WIDGET_CLASS (glade_fixed_parent_class)->replace_child
      (GLADE_WIDGET (fixed), old_object, new_object);

  if (gnew_widget)
    glade_fixed_connect_child (GLADE_FIXED (fixed), gnew_widget);
}

static gint
glade_fixed_event (GladeWidget *gwidget_fixed, GdkEvent *event)
{
  GladeFixed *fixed = GLADE_FIXED (gwidget_fixed);
  GladeWidgetAdaptor *adaptor;
  GtkWidget *event_widget;
  GladeProject *project = glade_widget_get_project (gwidget_fixed);
  gboolean handled = FALSE;
  GdkWindow *window = event->any.window;
  GdkDevice *device;
  
  adaptor = glade_project_get_add_item (project);

  /* Get the event widget and the deep widget */
  gdk_window_get_user_data (event->any.window, (gpointer) &event_widget);

  /* If the GladeWidget used this event... let it slide.
   */
  if (GLADE_WIDGET_CLASS (glade_fixed_parent_class)->event (gwidget_fixed, event))
    return TRUE;

  if ((device = gdk_event_get_device (event)))
    {
      /* Need to update mouse for configures. */
      glade_utils_get_pointer (GTK_WIDGET (glade_widget_get_object (gwidget_fixed)),
                               window, device, &fixed->mouse_x, &fixed->mouse_y);

      if (fixed->configuring)
        {
          return glade_fixed_handle_child_event
            (fixed, fixed->configuring, event_widget, event);
        }
    }

  /* If the container uses placeholder adding widget is already taken care of */
  if (GLADE_WIDGET_ADAPTOR_USE_PLACEHOLDERS (glade_widget_get_adaptor (gwidget_fixed)))
    return FALSE;
  
  switch (event->type)
    {
      case GDK_BUTTON_PRESS:   /* add widget */
        if (event->button.button == 1)
          {

            if (adaptor != NULL)
              {
                /* A widget type is selected in the palette.
                 * Add a new widget of that type.
                 */
                fixed->creating = TRUE;
                glade_command_create (adaptor,
                                      GLADE_WIDGET (fixed), NULL,
                                      glade_widget_get_project (GLADE_WIDGET (fixed)));
                fixed->creating = FALSE;

                glade_project_set_add_item (project, NULL);

                handled = TRUE;
              }
          }
        break;
      case GDK_MOTION_NOTIFY:
        if (glade_project_get_pointer_mode (project) == GLADE_POINTER_ADD_WIDGET)
          {
            glade_cursor_set (project, window, GLADE_CURSOR_ADD_WIDGET);

            handled = TRUE;
          }
        else if (GLADE_IS_FIXED (glade_widget_get_parent (gwidget_fixed)) == FALSE &&
                 glade_project_get_pointer_mode (project) == GLADE_POINTER_SELECT)
          glade_cursor_set (project, window, GLADE_CURSOR_SELECTOR);
        break;
      default:
        break;
    }
  return handled;
}

/*******************************************************************************
                                   GObjectClass
 *******************************************************************************/
static void
glade_fixed_finalize (GObject *object)
{
  GladeFixed *fixed = GLADE_FIXED (object);

  /* A GladeFixed should be finalized as a result of its
   * GtkContainer being destroyed, so we shouldn't need to bother
   * about disconnecting all the child signals.
   */
  g_free (fixed->x_prop);
  g_free (fixed->y_prop);
  g_free (fixed->width_prop);
  g_free (fixed->height_prop);

  G_OBJECT_CLASS (glade_fixed_parent_class)->finalize (object);
}

static void
glade_fixed_set_property (GObject *object,
                          guint prop_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
  GladeFixed *fixed = GLADE_FIXED (object);

  switch (prop_id)
    {
      case PROP_X_PROP:
        g_free (fixed->x_prop);
        fixed->x_prop = g_value_dup_string (value);
        break;
      case PROP_Y_PROP:
        g_free (fixed->y_prop);
        fixed->y_prop = g_value_dup_string (value);
        break;
      case PROP_WIDTH_PROP:
        g_free (fixed->width_prop);
        fixed->width_prop = g_value_dup_string (value);
        break;
      case PROP_HEIGHT_PROP:
        g_free (fixed->height_prop);
        fixed->height_prop = g_value_dup_string (value);
        break;
      case PROP_CAN_RESIZE:
        fixed->can_resize = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_fixed_get_property (GObject *object,
                          guint prop_id,
                          GValue *value,
                          GParamSpec *pspec)
{
  GladeFixed *fixed = GLADE_FIXED (object);

  switch (prop_id)
    {
      case PROP_X_PROP:
        g_value_set_string (value, fixed->x_prop);
        break;
      case PROP_Y_PROP:
        g_value_set_string (value, fixed->y_prop);
        break;
      case PROP_WIDTH_PROP:
        g_value_set_string (value, fixed->width_prop);
        break;
      case PROP_HEIGHT_PROP:
        g_value_set_string (value, fixed->height_prop);
        break;
      case PROP_CAN_RESIZE:
        g_value_set_boolean (value, fixed->can_resize);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_fixed_init (GladeFixed *fixed)
{
  /* Set defaults */
  fixed->x_prop = g_strdup ("x");
  fixed->y_prop = g_strdup ("y");
  fixed->width_prop = g_strdup ("width");
  fixed->height_prop = g_strdup ("height");
  fixed->can_resize = TRUE;
}

static void
glade_fixed_class_init (GladeFixedClass *fixed_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (fixed_class);
  GladeWidgetClass *gwidget_class = GLADE_WIDGET_CLASS (fixed_class);

  gobject_class->finalize = glade_fixed_finalize;
  gobject_class->set_property = glade_fixed_set_property;
  gobject_class->get_property = glade_fixed_get_property;

  gwidget_class->event = glade_fixed_event;
  gwidget_class->add_child = glade_fixed_add_child_impl;
  gwidget_class->remove_child = glade_fixed_remove_child_impl;
  gwidget_class->replace_child = glade_fixed_replace_child_impl;

  fixed_class->configure_child = glade_fixed_configure_child_impl;
  fixed_class->configure_begin = NULL;
  fixed_class->configure_end = glade_fixed_configure_end_impl;
  fixed_class->child_event = glade_fixed_child_event;

  /* Properties */
  g_object_class_install_property
      (gobject_class, PROP_X_PROP,
       g_param_spec_string
       ("x_prop", _("X position property"),
        _("The property used to set the X position of a child object"),
        "x", G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property
      (gobject_class, PROP_Y_PROP,
       g_param_spec_string
       ("y_prop", _("Y position property"),
        _("The property used to set the Y position of a child object"),
        "y", G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property
      (gobject_class, PROP_WIDTH_PROP,
       g_param_spec_string
       ("width_prop", _("Width property"),
        _("The property used to set the width of a child object"),
        "width-request", G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property
      (gobject_class, PROP_HEIGHT_PROP,
       g_param_spec_string
       ("height_prop", _("Height property"),
        _("The property used to set the height of a child object"),
        "height-request", G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  g_object_class_install_property
      (gobject_class, PROP_CAN_RESIZE,
       g_param_spec_boolean
       ("can_resize", _("Can resize"),
        _("Whether this container supports resizes of child widgets"),
        TRUE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  /**
   * GladeFixed::configure-child:
   * @gladewidget: the #GladeFixed which received the signal.
   * @arg1: the child #GladeWidget
   * @arg2: a pointer to a #GdkRectange describing the new size.
   *
   * Delegates the Drag/Resize job.
   *
   * Returns: %TRUE means you have handled the event and cancels the
   *          default handler from being triggered.
   */
  glade_fixed_signals[CONFIGURE_CHILD] =
      g_signal_new ("configure-child",
                    G_TYPE_FROM_CLASS (gobject_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET
                    (GladeFixedClass, configure_child),
                    glade_fixed_boolean_handled_accumulator, NULL,
                    glade_gtk_marshal_BOOLEAN__OBJECT_POINTER,
                    G_TYPE_BOOLEAN, 2, G_TYPE_OBJECT, G_TYPE_POINTER);

  /**
   * GladeFixed::configure-begin:
   * @gladewidget: the #GladeFixed which received the signal.
   * @arg1: the child #GladeWidget
   *
   * Signals the beginning of a Drag/Resize
   *
   * Returns: %TRUE means you have handled the event and cancels the
   *          default handler from being triggered.
   */
  glade_fixed_signals[CONFIGURE_BEGIN] =
      g_signal_new ("configure-begin",
                    G_TYPE_FROM_CLASS (gobject_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET
                    (GladeFixedClass, configure_begin),
                    glade_fixed_boolean_handled_accumulator, NULL,
                    glade_gtk_marshal_BOOLEAN__OBJECT,
                    G_TYPE_BOOLEAN, 1, G_TYPE_OBJECT);

  /**
   * GladeFixed::configure-end:
   * @gladewidget: the #GladeFixed which received the signal.
   * @arg1: the child #GladeWidget
   *
   * Signals the end of a Drag/Resize
   *
   * Returns: %TRUE means you have handled the event and cancels the
   *          default handler from being triggered.
   */
  glade_fixed_signals[CONFIGURE_END] =
      g_signal_new ("configure-end",
                    G_TYPE_FROM_CLASS (gobject_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET
                    (GladeFixedClass, configure_end),
                    glade_fixed_boolean_handled_accumulator, NULL,
                    glade_gtk_marshal_BOOLEAN__OBJECT,
                    G_TYPE_BOOLEAN, 1, G_TYPE_OBJECT);
}


/*******************************************************************************
                                      API
*******************************************************************************/

/* This is called from the catalog for a few widgets */
GladeWidget *
glade_gtk_create_fixed_widget (GladeWidgetAdaptor * adaptor,
                               const gchar * first_property_name,
                               va_list var_args)
{
  return (GladeWidget *) g_object_new_valist (GLADE_TYPE_FIXED,
                                              first_property_name, var_args);
}
