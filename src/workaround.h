/*
 * Copyright (C) 2020 Juan Pablo Ugarte.
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

#ifdef GDK_WINDOWING_X11

#include <gdk/gdkx.h>

/*
 * Loading gladegjs plugin makes cairo_xlib crash while trying to render a label
 * in a mutext lock deep in the stack, to avoid this we create an offscreen
 * label and wait for it to be drawn on the screen so that the mutex get
 * properly initialized and the crash is avoided.
 *
 * #0  __GI___pthread_mutex_lock (mutex=0x0) at ../nptl/pthread_mutex_lock.c:67
 * #1  0x00007ffff6ae430b in XrmQGetResource
 * (db=0x555555778b70, names=names@entry=0x7fffffffa6f0, classes=classes@entry=0x7fffffffa6fc, pType=pType@entry=0x7fffffffa6dc, pValue=pValue@entry=0x7fffffffa6e0) at ../../src/Xrm.c:2549
 * #2  0x00007ffff6ac02c6 in XGetDefault
 * (dpy=dpy@entry=0x5555555a39a0, prog=prog@entry=0x7ffff748fe01 "Xft", name=name@entry=0x7ffff7491594 "antialias")
 * at ../../src/GetDflt.c:231
 * #3  0x00007ffff744323e in get_boolean_default (value=<synthetic pointer>, option=0x7ffff7491594 "antialias", dpy=0x5555555a39a0)
 * at ../../../../src/cairo-xlib-screen.c:146
 * #4  _cairo_xlib_init_screen_font_options (info=0x555555d7b930, dpy=0x5555555a39a0) at ../../../../src/cairo-xlib-screen.c:146
 */

static void
check_for_draw (GdkEvent *event, gpointer loop)
{
  if (event->type == GDK_EXPOSE)
    {
      g_main_loop_quit (loop);
      gdk_event_handler_set ((GdkEventFunc) gtk_main_do_event, NULL, NULL);
    }

  gtk_main_do_event (event);
}

/* Taken from Gtk sources gtk-reftest.c  */
static void
wait_for_drawing (GdkWindow *window)
{
  GMainLoop *loop;

  loop = g_main_loop_new (NULL, FALSE);
  gdk_event_handler_set (check_for_draw, loop, NULL);
  g_main_loop_run (loop);
}

static void
_init_cairo_xlib_workaround ()
{
  GdkDisplay *display = gdk_display_get_default ();
  GtkWidget *win;

  /* This is only needed on X11 */
  if (!GDK_IS_X11_DISPLAY (display))
    return;

  win = gtk_offscreen_window_new ();
  gtk_container_add (GTK_CONTAINER (win),
                     gtk_label_new ("Foo"));
  gtk_widget_show_all (win);

  wait_for_drawing (gtk_widget_get_window (win));

  gtk_widget_destroy (win);
}

#endif
