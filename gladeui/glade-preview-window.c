/*
 * glade-preview-window.c
 *
 * Copyright (C) 2013 Juan Pablo Ugarte
   *
 * Author: Juan Pablo Ugarte <juanpablougarte@gmail.com>
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
#include <config.h>

#include "glade-preview-window.h"
#include <glib/gi18n-lib.h>
#include <cairo-pdf.h>
#include <cairo-svg.h>
#include <cairo-ps.h>

struct _GladePreviewWindowPrivate
{
  GtkWidget *box;
  GtkWidget *info;
  GtkWidget *message_label;
  GtkWidget *widget;

  GtkCssProvider *css_provider;
  GFileMonitor *css_monitor;
  gchar *css_file;
  gchar *extension;
};

G_DEFINE_TYPE_WITH_PRIVATE (GladePreviewWindow, glade_preview_window, GTK_TYPE_WINDOW);

static void
glade_preview_window_init (GladePreviewWindow *window)
{
  GladePreviewWindowPrivate *priv = glade_preview_window_get_instance_private (window);
  GtkWidget *content_area;

  window->priv = priv;

  gtk_window_set_title (GTK_WINDOW (window), _("Preview"));
  gtk_widget_add_events (GTK_WIDGET (window), GDK_KEY_PRESS_MASK);
  
  priv->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  priv->info = gtk_info_bar_new ();
  priv->message_label = gtk_label_new ("");
  gtk_label_set_line_wrap (GTK_LABEL (priv->message_label), TRUE);
  gtk_label_set_selectable (GTK_LABEL (priv->message_label), TRUE);

  gtk_widget_set_valign (priv->info, GTK_ALIGN_END);
  gtk_widget_set_vexpand (priv->info, FALSE);
  content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (priv->info));
  gtk_container_add (GTK_CONTAINER (content_area), priv->message_label);

  gtk_info_bar_set_show_close_button (GTK_INFO_BAR (priv->info), TRUE);

  g_signal_connect (priv->info, "response", G_CALLBACK (gtk_widget_hide), NULL);
  
  gtk_box_pack_start (GTK_BOX (priv->box), priv->info, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), priv->box);
  
  gtk_widget_show (priv->message_label);
  gtk_widget_show (priv->box);
}

static void
glade_preview_window_finalize (GObject *object)
{
  GladePreviewWindowPrivate *priv = GLADE_PREVIEW_WINDOW (object)->priv;

  g_free (priv->css_file);
  g_clear_object (&priv->css_provider);
  g_clear_object (&priv->css_monitor);

  G_OBJECT_CLASS (glade_preview_window_parent_class)->finalize (object);
}

static gboolean 
glade_preview_window_key_press_event (GtkWidget *widget, GdkEventKey *event)
{
  GladePreviewWindow *window = GLADE_PREVIEW_WINDOW (widget);
  GladePreviewWindowPrivate *priv = window->priv;
  gchar *extension;

  switch (event->keyval)
    {
      case GDK_KEY_F5:
        extension = "svg";
      break;
      case GDK_KEY_F6:
        extension = "ps";
      break;
      case GDK_KEY_F7:
        extension = "pdf";
      break;
      case GDK_KEY_F8:
        extension = priv->extension ? priv->extension : "png";
      break;
      case GDK_KEY_F11:
        if (gdk_window_get_state (gtk_widget_get_window (widget)) & GDK_WINDOW_STATE_FULLSCREEN)
          gtk_window_unfullscreen (GTK_WINDOW (widget));
        else
          gtk_window_fullscreen (GTK_WINDOW (widget));

        return TRUE;
      break;
      default:
        return FALSE;
      break;
    }

  if (extension)
    {
      gchar *tmp_file = g_strdup_printf ("glade-screenshot-XXXXXX.%s", extension); 

      g_mkstemp (tmp_file);
      glade_preview_window_screenshot (window, FALSE, tmp_file);
      g_free (tmp_file);

      return TRUE;
    }
  
  return FALSE;
}

static void
glade_preview_window_realize (GtkWidget *widget)
{
  GladePreviewWindowPrivate *priv = GLADE_PREVIEW_WINDOW (widget)->priv;

  GTK_WIDGET_CLASS (glade_preview_window_parent_class)->realize (widget);
  
  if (priv->widget && gtk_widget_is_toplevel (priv->widget) &&
      gtk_widget_get_parent (priv->widget) == NULL)
    {
      gtk_widget_set_parent_window (priv->widget, gtk_widget_get_window (widget));
      gtk_box_pack_start (GTK_BOX (priv->box), priv->widget, TRUE, TRUE, 0);
      gtk_widget_show (priv->widget);
    }
}

static void
glade_preview_window_class_init (GladePreviewWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = glade_preview_window_finalize;

  widget_class->realize = glade_preview_window_realize;
  widget_class->key_press_event = glade_preview_window_key_press_event;
}

GtkWidget *
glade_preview_window_new (void)
{
  return GTK_WIDGET (g_object_new (GLADE_TYPE_PREVIEW_WINDOW, NULL));
}

static void 
glade_preview_window_set_css_provider_forall (GtkWidget *widget, gpointer data)
{
  gtk_style_context_add_provider (gtk_widget_get_style_context (widget),
                                  GTK_STYLE_PROVIDER (data),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  
  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall (GTK_CONTAINER (widget), glade_preview_window_set_css_provider_forall, data);
}

void
glade_preview_window_set_widget (GladePreviewWindow *window, GtkWidget *widget)
{
  GladePreviewWindowPrivate *priv;
  GdkWindow *gdkwindow;

  g_return_if_fail (GLADE_IS_PREVIEW_WINDOW (window));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = window->priv;
  
  if (priv->widget)
    gtk_container_remove (GTK_CONTAINER (priv->box), priv->widget);

  priv->widget = widget;

  if (priv->css_provider)
    glade_preview_window_set_css_provider_forall (widget, priv->css_provider);
    
  if (gtk_widget_is_toplevel (widget))
    {
      /* Delay setting it until we have a window  */
      if (!(gdkwindow = gtk_widget_get_window (priv->box)))
        return;

      gtk_widget_set_parent_window (widget, gdkwindow);
    }

  gtk_box_pack_start (GTK_BOX (priv->box), widget, TRUE, TRUE, 0);
}

void
glade_preview_window_set_message (GladePreviewWindow *window, 
                                  GtkMessageType      type,
                                  const gchar        *message)
{
  GladePreviewWindowPrivate *priv;

  g_return_if_fail (GLADE_IS_PREVIEW_WINDOW (window));
  priv = window->priv;

  gtk_info_bar_set_message_type (GTK_INFO_BAR (priv->info), type);
  
  if (message)
    {
      gtk_label_set_text (GTK_LABEL (priv->message_label), message);
      gtk_widget_show (priv->info);
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (priv->message_label), "");
      gtk_widget_hide (priv->info);
    }
}

static void 
on_css_monitor_changed (GFileMonitor       *monitor,
                        GFile              *file,
                        GFile              *other_file,
                        GFileMonitorEvent   event_type,
                        GladePreviewWindow *window)
{
  GladePreviewWindowPrivate *priv = window->priv;
  GError *error = NULL;

  gtk_css_provider_load_from_file (priv->css_provider, file, &error);

  if (error)
    {
      glade_preview_window_set_message (window, GTK_MESSAGE_WARNING, error->message);
      g_error_free (error);
    }
  else
    glade_preview_window_set_message (window, GTK_MESSAGE_OTHER, NULL);
}

void
glade_preview_window_set_css_file (GladePreviewWindow *window,
                                   const gchar        *css_file)
{
  GladePreviewWindowPrivate *priv;
  GError *error = NULL;
  GFile *file;

  g_return_if_fail (GLADE_IS_PREVIEW_WINDOW (window));
  priv = window->priv;

  g_free (priv->css_file);
  g_clear_object (&priv->css_provider);
  g_clear_object (&priv->css_monitor);

  priv->css_file = g_strdup (css_file);
  
  file = g_file_new_for_path (css_file);
  priv->css_provider = gtk_css_provider_new ();
  g_object_ref_sink (priv->css_provider);
  
  priv->css_monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, &error);
  if (error)
    {
      g_warning ("Cant monitor CSS file %s: %s", css_file, error->message);
      g_error_free (error);
    }
  else
    {
      g_object_ref_sink (priv->css_monitor);
      g_signal_connect (priv->css_monitor, "changed",
                        G_CALLBACK (on_css_monitor_changed), window);
    }

  /* load CSS */
  gtk_css_provider_load_from_file (priv->css_provider, file, &error);
  if (error)
    {
      glade_preview_window_set_message (window, GTK_MESSAGE_INFO, error->message);
      g_message ("%s CSS parsing failed: %s", css_file, error->message);
      g_error_free (error);
    }

  if (priv->widget)
    glade_preview_window_set_css_provider_forall (priv->widget, priv->css_provider);
  
  g_object_unref (file);
}

void
glade_preview_window_set_screenshot_extension (GladePreviewWindow *window,
                                               const gchar        *extension)
{
  GladePreviewWindowPrivate *priv;

  g_return_if_fail (GLADE_IS_PREVIEW_WINDOW (window));
  priv = window->priv;

  g_free (priv->extension);
  priv->extension = g_strdup (extension);
}

static gboolean
quit_when_idle (gpointer loop)
{
  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static void
check_for_draw (GdkEvent *event, gpointer loop)
{
  if (event->type == GDK_EXPOSE)
    {
      g_idle_add (quit_when_idle, loop);
      gdk_event_handler_set ((GdkEventFunc) gtk_main_do_event, NULL, NULL);
    }

  gtk_main_do_event (event);
}

/* Taken from Gtk sources gtk-reftest.c  */
static void
glade_preview_wait_for_drawing (GdkWindow *window)
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

  /* give the WM/server some time to sync. They need it.
   * Also, do use popups instead of toplevls in your tests
   * whenever you can. */
  gdk_display_sync (gdk_window_get_display (window));
  g_timeout_add (500, quit_when_idle, loop);
  g_main_loop_run (loop);
}

static const gchar *
glade_preview_get_extension (const gchar *filename)
{
  gchar *extension;
  
  g_return_val_if_fail (filename != NULL, NULL);

  extension = g_strrstr (filename,".");

  if (extension)
    extension++;

  if (!extension)
    {
      g_warning ("%s has no extension!", filename);
      return NULL;
    }
  return extension;
}

static void
glade_preview_get_scale (GdkScreen *screen, gdouble *sx, gdouble *sy)
{
  if (sx)
    *sx = 72.0 / (gdk_screen_get_width (screen) / (gdk_screen_get_width_mm (screen) * 0.03937008));

  if (sy)
    *sy = 72.0 / (gdk_screen_get_height (screen) / (gdk_screen_get_height_mm (screen) * 0.03937008));
}

static cairo_surface_t *
glade_preview_surface_from_file (const gchar *filename, gdouble w, gdouble h)
{
  cairo_surface_t *surface;
  const gchar *extension;

  extension = glade_preview_get_extension (filename);
  
  if (extension == NULL)
    return NULL;

  if (g_strcmp0 (extension, "svg") == 0)
#if CAIRO_HAS_SVG_SURFACE
    surface = cairo_svg_surface_create (filename, w, h);
#else
    g_warning ("PDF not supported by the cairo version used");
#endif
  else if (g_strcmp0 (extension, "ps") == 0)
#if CAIRO_HAS_PS_SURFACE
    surface = cairo_ps_surface_create (filename, w, h);
#else
    g_warning ("PS not supported by the cairo version used");
#endif
  else if (g_strcmp0 (extension, "pdf") == 0)
#if CAIRO_HAS_PDF_SURFACE
    surface = cairo_pdf_surface_create (filename, w, h);
#else
    g_warning ("PDF not supported by the cairo version used");
#endif
  else
    return NULL;

  return surface;
}

/**
 * glade_preview_window_screenshot:
 * @window: A GladePreviewWindow
 * @wait: True if it should wait for widget to draw.
 * @filename:  a filename to save the image.
 * 
 * Takes a screenshot of the current widget @window is showing and save it to @filename
 * Supported extension are svg, ps, pdf and wahtever gdk-pixbuf supports 
 */
void
glade_preview_window_screenshot (GladePreviewWindow *window,
                                 gboolean wait,
                                 const gchar *filename)
{
  GladePreviewWindowPrivate *priv;
  cairo_surface_t *surface;
  GdkWindow *gdkwindow;
  GdkScreen *screen;
  gdouble sx, sy;
  gint w, h;

  g_return_if_fail (GLADE_IS_PREVIEW_WINDOW (window));
  g_return_if_fail (filename != NULL);
  priv = window->priv;

  if (!priv->widget)
    return;

  gdkwindow = gtk_widget_get_window (priv->widget);
  screen = gdk_window_get_screen (gdkwindow);

  if (wait)
    glade_preview_wait_for_drawing (gdkwindow);

  w = gtk_widget_get_allocated_width (priv->widget);
  h = gtk_widget_get_allocated_height (priv->widget);
  glade_preview_get_scale (screen, &sx, &sy);
    
  surface = glade_preview_surface_from_file (filename, w*sx, h*sy);

  if (surface)
    {
      cairo_t *cr = cairo_create (surface);
      cairo_scale (cr, sx, sy);
      gtk_widget_draw (priv->widget, cr);
      cairo_destroy (cr);
      cairo_surface_destroy(surface);
    }
  else
    {
      GdkPixbuf *pix = gdk_pixbuf_get_from_window (gdkwindow, 0, 0, w, h);
      const gchar *ext = glade_preview_get_extension (filename);
      GError *error = NULL;
      
      if (!gdk_pixbuf_save (pix, filename, ext ? ext : "png", &error, NULL))
        {
          g_warning ("Could not save screenshot to %s because %s", filename, error->message);
          g_error_free (error);
        }

      g_object_unref (pix);
    }
}

/**
 * glade_preview_window_slideshow_save:
 * @window: A GladePreviewWindow
 * @filename:  a filename to save the slideshow.
 * 
 * Takes a screenshot of every widget GtkStack children and save it to @filename
 * each in a different page
 */
void
glade_preview_window_slideshow_save (GladePreviewWindow *window,
                                     const gchar *filename)
{
  GladePreviewWindowPrivate *priv;
  cairo_surface_t *surface;
  GdkWindow *gdkwindow;
  GtkStack *stack;
  gdouble sx, sy;

  g_return_if_fail (GLADE_IS_PREVIEW_WINDOW (window));
  g_return_if_fail (filename != NULL);
  priv = window->priv;

  g_return_if_fail (priv->widget);
  g_return_if_fail (GTK_IS_STACK (priv->widget));
  stack = GTK_STACK (priv->widget);

  gdkwindow = gtk_widget_get_window (priv->widget);
  glade_preview_wait_for_drawing (gdkwindow);
  
  glade_preview_get_scale (gtk_widget_get_screen (GTK_WIDGET (window)), &sx, &sy); 
  surface = glade_preview_surface_from_file (filename, 
                                             gtk_widget_get_allocated_width (GTK_WIDGET (stack))*sx,
                                             gtk_widget_get_allocated_height (GTK_WIDGET (stack))*sy);

  if (surface)
    {
      GList *l, *children = gtk_container_get_children (GTK_CONTAINER (stack));
      cairo_t *cr= cairo_create (surface);

      cairo_scale (cr, sx, sy);

      for (l = children; l; l = g_list_next (l))
        {
          GtkWidget *child = l->data;
          gtk_stack_set_visible_child (stack, child);
          glade_preview_wait_for_drawing (gdkwindow);
          gtk_widget_draw (child, cr);
          cairo_show_page (cr);
        }

      if (children)
        gtk_stack_set_visible_child (stack, children->data);

      g_list_free (children);
      cairo_destroy (cr);
      cairo_surface_destroy(surface);
    }
  else
    g_warning ("Could not save slideshow to %s", filename);
}
