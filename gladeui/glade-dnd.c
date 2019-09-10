/*
 * glade-dnd.c
 *
 * Copyright (C) 2013  Juan Pablo Ugarte
 *
 * Authors:
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

#include "glade.h"
#include "glade-dnd.h"

GtkTargetEntry *
_glade_dnd_get_target (void)
{
  static GtkTargetEntry target = {GLADE_DND_TARGET_DATA, GTK_TARGET_SAME_APP, GLADE_DND_INFO_DATA};
  return &target;
}

void
_glade_dnd_dest_set (GtkWidget *target)
{
  gtk_drag_dest_set (target, 0, _glade_dnd_get_target (), 1, GDK_ACTION_MOVE | GDK_ACTION_COPY);
}

GObject *
_glade_dnd_get_data (GdkDragContext   *context,
                     GtkSelectionData *selection,
                     guint             info)
{
  GdkAtom target = gtk_selection_data_get_target (selection);
  gchar *target_name = gdk_atom_name (target);
  gboolean is_target_data = (g_strcmp0 (target_name, GLADE_DND_TARGET_DATA) == 0);

  g_free (target_name);

  if (info == GLADE_DND_INFO_DATA && is_target_data)
    {
      const guchar *data = gtk_selection_data_get_data (selection);
      if (data)
        return *((GObject **)data);
    }
  return NULL;
}


void
_glade_dnd_set_data (GtkSelectionData *selection, GObject *data)
{
  static GdkAtom type = 0;

  if (!type)
    type = gdk_atom_intern_static_string (GLADE_DND_TARGET_DATA);

  gtk_selection_data_set (selection, type, sizeof (gpointer),
                          (const guchar *)&data,
                          sizeof (gpointer));
}

static gboolean
on_drag_icon_draw (GtkWidget *widget, cairo_t *cr)
{
  GtkStyleContext *context = gtk_widget_get_style_context (widget);
  cairo_pattern_t *gradient;
  GtkAllocation alloc;
  gint x, y, w, h;
  gdouble h2;
  GdkRGBA bg;

  /* Not needed acording to GtkWidget:draw documentation
   * But seems like there is a bug when used as a drag_icon that makes the
   * cairo translation used here persist when drawing children.
   */
  cairo_save (cr);

  /* Clear BG */
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  gtk_widget_get_allocation (widget, &alloc);
  x = alloc.x;
  y = alloc.y;
  w = alloc.width;
  h = alloc.height;
  h2 = h/2.0;

  gtk_style_context_get_background_color (context, gtk_style_context_get_state (context), &bg);

  gradient = cairo_pattern_create_linear (x, y, x, y+h);
  cairo_pattern_add_color_stop_rgba (gradient, 0, bg.red, bg.green, bg.blue, 0);
  cairo_pattern_add_color_stop_rgba (gradient, .5, bg.red, bg.green, bg.blue, .8);
  cairo_pattern_add_color_stop_rgba (gradient, 1, bg.red, bg.green, bg.blue, 0);

  cairo_set_source (cr, gradient);
  cairo_rectangle (cr, x+h2, y, w-h, h);
  cairo_fill (cr);
  cairo_pattern_destroy (gradient);

  gradient = cairo_pattern_create_radial (x+h2, y+h2, 0, x+h2, y+h2, h2);
  cairo_pattern_add_color_stop_rgba (gradient, 0, bg.red, bg.green, bg.blue, .8);
  cairo_pattern_add_color_stop_rgba (gradient, 1, bg.red, bg.green, bg.blue, 0);

  cairo_set_source (cr, gradient);
  cairo_rectangle (cr, x, y, h2, h);
  cairo_fill (cr);

  cairo_translate (cr, w-h, 0);
  cairo_set_source (cr, gradient);
  cairo_rectangle (cr, x+h2, y, h2, h);
  cairo_fill (cr);

  cairo_pattern_destroy (gradient);
  cairo_restore (cr);

  return FALSE;
}

void
_glade_dnd_set_icon_widget (GdkDragContext *context,
                            const gchar *icon_name,
                            const gchar *description)
{
  GtkWidget *window, *box, *label, *icon;
  GdkScreen *screen;
  GdkVisual *visual;

  screen = gdk_window_get_screen (gdk_drag_context_get_source_window (context));
  window = gtk_window_new (GTK_WINDOW_POPUP);

  gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DND);
  gtk_window_set_screen (GTK_WINDOW (window), screen);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_container_set_border_width (GTK_CONTAINER (box), 12);

  icon = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_opacity (icon, .8);

  label = gtk_label_new (description);

  gtk_box_pack_start (GTK_BOX (box), icon, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);

  gtk_widget_show_all (box);
  gtk_container_add (GTK_CONTAINER (window), box);

  if ((visual = gdk_screen_get_rgba_visual (screen)))
    {
      gtk_widget_set_visual (window, visual);
      gtk_widget_set_app_paintable (window, TRUE);
      g_signal_connect (window, "draw", G_CALLBACK (on_drag_icon_draw), NULL);
    }

  g_object_ref_sink (window);
  gtk_drag_set_icon_widget (context, window, 0, 0);
  g_object_unref (window);
}
