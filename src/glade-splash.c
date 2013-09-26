/*
 * glade-splash.c
 * 
 * Copyright (C) 2013 Juan Pablo Ugarte.
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
 * Authors:
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#include <config.h>

#include <gladeui/glade.h>
#include <glib/gi18n.h>
#include "glade-splash.h"
#include "glade-logo.h"

static gboolean
quit_when_idle (gpointer loop)
{
  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static void
glade_app_event_handler (GdkEvent *event, gpointer data)
{
  if (glade_app_do_event (event)) return;

  gtk_main_do_event (event);
}

static void
check_for_draw (GdkEvent *event, gpointer loop)
{
  if (event->type == GDK_EXPOSE)
    {
      g_idle_add (quit_when_idle, loop);
      gdk_event_handler_set (glade_app_event_handler, NULL, NULL);
    }

  gtk_main_do_event (event);
}

/* Taken from Gtk sources gtk-reftest.c  */
static void
glade_wait_for_drawing (GtkWidget *widget)
{
  GMainLoop *loop;

  loop = g_main_loop_new (NULL, FALSE);
  /* We wait until the widget is drawn for the first time.
   * We can not wait for a GtkWidget::draw event, because that might not
   * happen if the window is fully obscured by windowed child widgets.
   * Alternatively, we could wait for an expose event on widget's window.
   * Both of these are rather hairy, not sure what's best. */
  gdk_event_handler_set (check_for_draw, loop, NULL);
  g_main_loop_run (loop);
}

#define LOGO_OFFSET 36

static gboolean
on_glade_splash_screen_draw (GtkWidget *widget, cairo_t *cr)
{
  GtkAllocation alloc;

  /* Clear BG */
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  gtk_widget_get_allocation (widget, &alloc);
  cairo_translate (cr, MAX(0, (alloc.width/2) - (GLADE_LOGO_WIDTH+LOGO_OFFSET)/2), 0);
  
  cairo_set_source_rgba (cr, .8, .8, .8, .12);
  cairo_translate (cr, 4, 4);
  cairo_append_path (cr, &glade_logo_path);
  cairo_fill (cr);

  cairo_set_source_rgba (cr, .1, .1, .1, .12);
  cairo_translate (cr, -4, -4);
  cairo_append_path (cr, &glade_logo_path);
  cairo_fill (cr);

  return FALSE;
}

/*
 * We want this function to be as fast as possible, this is one of the reason
 * why we do not use an image as the logo, instead we use a harcoded cairo path
 * besides we already need a custom draw function to clear the bg!
 */
GtkWidget *
glade_splash_screen_new (void)
{
  gchar *glade_string, *glade_version;
  GtkWidget *window, *label;
  GtkCssProvider *css;
  GdkVisual *visual;
  gint natural;

  /* No transparency, No Splash screen! */
  if (!(visual = gdk_screen_get_rgba_visual (gdk_screen_get_default ())))
    return NULL;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_keep_above (GTK_WINDOW (window), TRUE);
  gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);

  gtk_widget_set_visual (window, visual);
  gtk_widget_set_app_paintable (window, TRUE);
  
  g_signal_connect (window, "draw", G_CALLBACK (on_glade_splash_screen_draw), NULL);

  label = gtk_label_new ("");
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);

  /* To translators: This is the version string used in the splash screen */
  glade_version = g_strdup_printf (_("Version %s"), PACKAGE_VERSION);
  
  glade_string = g_strdup_printf ("<span size='64000' weight='bold' style='italic'>%s</span>\n"
                                  "<span size='16000'>%s</span>",
                                  g_get_application_name (),
                                  glade_version);
  gtk_label_set_markup (GTK_LABEL (label), glade_string);
  gtk_widget_set_opacity (label, .5);
  g_free (glade_version);
  g_free (glade_string);

  css = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (css, "GtkLabel { color: white; text-shadow: 2px 2px 2px black;}", -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (label),
                                  GTK_STYLE_PROVIDER (css), 
                                  GTK_STYLE_PROVIDER_PRIORITY_USER);
  g_object_unref (css);

  gtk_widget_show (label);
  
  /* Leave room for the logo */
  gtk_widget_get_preferred_height (label, NULL, &natural);
  gtk_widget_set_margin_top (label, GLADE_LOGO_HEIGHT - (natural/6));

  /* Center text to the logo */
  gtk_widget_get_preferred_width (label, NULL, &natural);
  gtk_widget_set_margin_left (label, MAX (0, ((GLADE_LOGO_WIDTH+LOGO_OFFSET)/2) - (natural/2) ));

  /* Needed for css text shadow */
  gtk_widget_set_margin_right (label, 2);

  gtk_container_add (GTK_CONTAINER (window), label);
  
  return window;
}

void
glade_splash_window_show_immediately (GtkWidget *window)
{
  gtk_widget_show (window);
  glade_wait_for_drawing (window);
}
