/*
 * glade-previewer.c
 *
 * Copyright (C) 2013-2016 Juan Pablo Ugarte
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

#include "glade-previewer.h"
#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>
#include <cairo-pdf.h>
#include <cairo-svg.h>
#include <cairo-ps.h>

struct _GladePreviewerPrivate
{
  GtkWidget *widget;    /* Preview widget */
  GList     *objects;   /* SlideShow objects */
  GtkWidget *dialog;    /* Dialog to show messages */
  GtkWidget *textview;

  GtkCssProvider *css_provider;
  GFileMonitor *css_monitor;
  gchar *css_file;
  gchar *extension;

  gboolean print_handlers;
};

G_DEFINE_TYPE_WITH_PRIVATE (GladePreviewer, glade_previewer, G_TYPE_OBJECT);

static void
glade_previewer_init (GladePreviewer *preview)
{
  GladePreviewerPrivate *priv = glade_previewer_get_instance_private (preview);

  preview->priv = priv;
}

static void
glade_previewer_dispose (GObject *object)
{
  GladePreviewerPrivate *priv = GLADE_PREVIEWER (object)->priv;

  g_list_free (priv->objects);
  
  priv->objects = NULL;
  priv->dialog = NULL;
  g_clear_object (&priv->css_provider);
  g_clear_object (&priv->css_monitor);

  G_OBJECT_CLASS (glade_previewer_parent_class)->dispose (object);
}

static void
glade_previewer_finalize (GObject *object)
{
  GladePreviewerPrivate *priv = GLADE_PREVIEWER (object)->priv;

  g_free (priv->css_file);
  g_free (priv->extension);

  G_OBJECT_CLASS (glade_previewer_parent_class)->finalize (object);
}

static gboolean 
on_widget_key_press_event (GtkWidget      *widget,
                           GdkEventKey    *event,
                           GladePreviewer *preview)
{
  GladePreviewerPrivate *priv = preview->priv;
  GList *node = NULL;
  GtkStack *stack;
  gchar *extension;

  if (priv->objects)
    {
      stack = GTK_STACK (gtk_bin_get_child (GTK_BIN (priv->widget)));
      node = g_list_find (priv->objects, gtk_stack_get_visible_child (stack));
    }

  switch (event->keyval)
    {
      case GDK_KEY_Page_Up:
        if (node && node->prev)
          gtk_stack_set_visible_child  (stack, node->prev->data);
        return TRUE;
      break;
      case GDK_KEY_Page_Down:
        if (node && node->next)
          gtk_stack_set_visible_child  (stack, node->next->data);
        return TRUE;
      break;
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
      glade_previewer_screenshot (preview, FALSE, tmp_file);
      g_free (tmp_file);

      return TRUE;
    }

  return FALSE;
}

static void
glade_previewer_class_init (GladePreviewerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = glade_previewer_dispose;
  object_class->finalize = glade_previewer_finalize;
}

GObject *
glade_previewer_new (void)
{
  return g_object_new (GLADE_TYPE_PREVIEWER, NULL);
}

void
glade_previewer_present (GladePreviewer *preview)
{
  g_return_if_fail (GLADE_IS_PREVIEWER (preview));
  gtk_window_present (GTK_WINDOW (preview->priv->widget));
}

void
glade_previewer_set_widget (GladePreviewer *preview, GtkWidget *widget)
{
  GladePreviewerPrivate *priv;
  GtkWidget *sw;

  g_return_if_fail (GLADE_IS_PREVIEWER (preview));
  g_return_if_fail (GTK_IS_WIDGET (widget));

  priv = preview->priv;

  if (priv->widget)
    gtk_widget_destroy (priv->widget);

  if (!gtk_widget_is_toplevel (widget))
    {
      GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      gtk_container_add (GTK_CONTAINER (window), widget);

      priv->widget = window;
    }
  else
    {
      priv->widget = widget;
    }

  /* Create dialog to display messages */
  priv->dialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (priv->dialog), 640, 320);
  gtk_window_set_title (GTK_WINDOW (priv->dialog), _("Glade Previewer log"));
  gtk_window_set_transient_for (GTK_WINDOW (priv->dialog), GTK_WINDOW (priv->widget));

  priv->textview = gtk_text_view_new ();
  gtk_widget_show (priv->textview);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (sw);

  gtk_container_add (GTK_CONTAINER (sw), priv->textview);
  gtk_container_add (GTK_CONTAINER (priv->dialog), sw);

  /* Hide dialog on delete event */
  g_signal_connect (priv->dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide),
                    NULL);

  /* Quit on delete event */
  g_signal_connect (priv->widget, "delete-event",
                    G_CALLBACK (gtk_main_quit),
                    NULL);

  /* Make sure we get press events */
  gtk_widget_add_events (priv->widget, GDK_KEY_PRESS_MASK);

  /* Handle key presses for screenshot feature */
  g_signal_connect_object (priv->widget, "key-press-event",
                           G_CALLBACK (on_widget_key_press_event),
                           preview, 0);
}

void
glade_previewer_set_message (GladePreviewer *preview, 
                             GtkMessageType  type,
                             const gchar    *message)
{
  GladePreviewerPrivate *priv;
  GtkTextBuffer *buffer;

  g_return_if_fail (GLADE_IS_PREVIEWER (preview));
  priv = preview->priv;

  if (!priv->textview)
    return;

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->textview));
  
  if (message)
    {
      GtkTextIter iter;

      /* TODO: use message type to color text */
      gtk_text_buffer_get_start_iter (buffer, &iter);
      gtk_text_buffer_insert (buffer, &iter, "\n", -1);

      gtk_text_buffer_get_start_iter (buffer, &iter);
      gtk_text_buffer_insert (buffer, &iter, message, -1);

      gtk_window_present (GTK_WINDOW (priv->dialog));
    }
}

static void 
on_css_monitor_changed (GFileMonitor       *monitor,
                        GFile              *file,
                        GFile              *other_file,
                        GFileMonitorEvent   event_type,
                        GladePreviewer     *preview)
{
  GladePreviewerPrivate *priv = preview->priv;
  GError *error = NULL;

  gtk_css_provider_load_from_file (priv->css_provider, file, &error);

  if (error)
    {
      glade_previewer_set_message (preview, GTK_MESSAGE_WARNING, error->message);
      g_error_free (error);
    }
  else
    glade_previewer_set_message (preview, GTK_MESSAGE_OTHER, NULL);
}

void
glade_previewer_set_css_file (GladePreviewer *preview,
                            const gchar  *css_file)
{
  GladePreviewerPrivate *priv;
  GError *error = NULL;
  GFile *file;

  g_return_if_fail (GLADE_IS_PREVIEWER (preview));
  priv = preview->priv;

  g_free (priv->css_file);
  g_clear_object (&priv->css_monitor);

  priv->css_file = g_strdup (css_file);
  
  file = g_file_new_for_path (css_file);
  
  if (!priv->css_provider)
    {
      priv->css_provider = gtk_css_provider_new ();
      g_object_ref_sink (priv->css_provider);

      /* Set provider for default screen once */
      gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                 GTK_STYLE_PROVIDER (priv->css_provider),
                                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

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
                        G_CALLBACK (on_css_monitor_changed), preview);
    }

  /* load CSS */
  gtk_css_provider_load_from_file (priv->css_provider, file, &error);
  if (error)
    {
      glade_previewer_set_message (preview, GTK_MESSAGE_INFO, error->message);
      g_message ("%s CSS parsing failed: %s", css_file, error->message);
      g_error_free (error);
    }
  
  g_object_unref (file);
}

void
glade_previewer_set_screenshot_extension (GladePreviewer *preview,
                                          const gchar    *extension)
{
  GladePreviewerPrivate *priv;

  g_return_if_fail (GLADE_IS_PREVIEWER (preview));
  priv = preview->priv;

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
glade_previewer_wait_for_drawing (GdkWindow *window)
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
glade_previewer_get_extension (const gchar *filename)
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
glade_previewer_get_scale (GdkScreen *screen, gdouble *sx, gdouble *sy)
{
  if (sx)
    *sx = 72.0 / (gdk_screen_get_width (screen) / (gdk_screen_get_width_mm (screen) * 0.03937008));

  if (sy)
    *sy = 72.0 / (gdk_screen_get_height (screen) / (gdk_screen_get_height_mm (screen) * 0.03937008));
}

static cairo_surface_t *
glade_previewer_surface_from_file (const gchar *filename, gdouble w, gdouble h)
{
  cairo_surface_t *surface;
  const gchar *extension;

  extension = glade_previewer_get_extension (filename);
  
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
 * glade_previewer_screenshot:
 * @preview: A GladePreviewer
 * @wait: True if it should wait for widget to draw.
 * @filename:  a filename to save the image.
 * 
 * Takes a screenshot of the current widget @window is showing and save it to @filename
 * Supported extension are svg, ps, pdf and wahtever gdk-pixbuf supports 
 */
void
glade_previewer_screenshot (GladePreviewer *preview,
                            gboolean        wait,
                            const gchar    *filename)
{
  GladePreviewerPrivate *priv;
  cairo_surface_t *surface;
  GdkWindow *gdkwindow;
  GdkScreen *screen;
  gdouble sx, sy;
  gint w, h;

  g_return_if_fail (GLADE_IS_PREVIEWER (preview));
  g_return_if_fail (filename != NULL);
  priv = preview->priv;

  if (!priv->widget)
    return;

  gdkwindow = gtk_widget_get_window (priv->widget);
  screen = gdk_window_get_screen (gdkwindow);

  if (wait)
    glade_previewer_wait_for_drawing (gdkwindow);

  w = gtk_widget_get_allocated_width (priv->widget);
  h = gtk_widget_get_allocated_height (priv->widget);
  glade_previewer_get_scale (screen, &sx, &sy);
    
  surface = glade_previewer_surface_from_file (filename, w*sx, h*sy);

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
      const gchar *ext = glade_previewer_get_extension (filename);
      GError *error = NULL;
      
      if (!gdk_pixbuf_save (pix, filename, ext ? ext : "png", &error, NULL))
        {
          g_warning ("Could not save screenshot to %s because %s", filename, error->message);
          g_error_free (error);
        }

      g_object_unref (pix);
    }
}

static gint
objects_cmp_func (gconstpointer a, gconstpointer b)
{
  const gchar *name_a, *name_b;
  name_a = gtk_buildable_get_name (GTK_BUILDABLE (a));
  name_b = gtk_buildable_get_name (GTK_BUILDABLE (b));
  return g_strcmp0 (name_a, name_b);
}

/**
 * glade_previewer_set_slideshow_widgets:
 * @preview: A GladePreviewer
 * @objects: GSlist of GObject
 * 
 * Add a list of objects to slideshow
 */
void
glade_previewer_set_slideshow_widgets (GladePreviewer *preview,
                                      GSList          *objects)
{
  GladePreviewerPrivate *priv;
  GtkStack *stack;
  GSList *l;

  g_return_if_fail (GLADE_IS_PREVIEWER (preview));
  priv = preview->priv;

  stack = GTK_STACK (gtk_stack_new ());
  gtk_stack_set_transition_type (stack, GTK_STACK_TRANSITION_TYPE_CROSSFADE);

  objects = g_slist_sort (g_slist_copy (objects), objects_cmp_func);

  for (l = objects; l; l = g_slist_next (l))
    {
       GObject *obj = l->data;

       if (!GTK_IS_WIDGET (obj) || gtk_widget_get_parent (GTK_WIDGET (obj)))
         continue;

       /* TODO: make sure we can add a toplevel inside a stack */
       if (GTK_IS_WINDOW (obj))
         continue;

       priv->objects = g_list_prepend (priv->objects, obj);

       gtk_stack_add_named (stack, GTK_WIDGET (obj),
                            gtk_buildable_get_name (GTK_BUILDABLE (obj)));
    }

  priv->objects = g_list_reverse (priv->objects); 

  glade_previewer_set_widget (preview, GTK_WIDGET (stack));
  gtk_widget_show (GTK_WIDGET (stack));
  
  g_slist_free (objects);
}

/**
 * glade_previewer_slideshow_save:
 * @preview: A GladePreviewer
 * @filename:  a filename to save the slideshow.
 * 
 * Takes a screenshot of every widget GtkStack children and save it to @filename
 * each in a different page
 */
void
glade_previewer_slideshow_save (GladePreviewer *preview,
                                const gchar    *filename)
{
  GladePreviewerPrivate *priv;
  cairo_surface_t *surface;
  GdkWindow *gdkwindow;
  GtkWidget *child;
  GtkStack *stack;
  gdouble sx, sy;

  g_return_if_fail (GLADE_IS_PREVIEWER (preview));
  g_return_if_fail (filename != NULL);
  priv = preview->priv;

  g_return_if_fail (GTK_IS_BIN (priv->widget));

  child = gtk_bin_get_child (GTK_BIN (priv->widget));
  g_return_if_fail (GTK_IS_STACK (child));
  stack = GTK_STACK (child);

  gtk_stack_set_transition_type (stack, GTK_STACK_TRANSITION_TYPE_NONE);

  gdkwindow = gtk_widget_get_window (priv->widget);
  glade_previewer_wait_for_drawing (gdkwindow);
  
  glade_previewer_get_scale (gtk_widget_get_screen (GTK_WIDGET (priv->widget)), &sx, &sy); 
  surface = glade_previewer_surface_from_file (filename, 
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
          glade_previewer_wait_for_drawing (gdkwindow);
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

/**
 * glade_previewer_set_print_handlers:
 * @preview: A GladePreviewer
 * @print: whether to print handlers or not
 * 
 * Set whether to print handlers when they are activated or not.
 * It only works if you use glade_previewer_connect_function() as the 
 * connect funtion.
 */
void
glade_previewer_set_print_handlers (GladePreviewer *preview,
                                    gboolean        print)
{
  g_return_if_fail (GLADE_IS_PREVIEWER (preview));
  preview->priv->print_handlers = print;
}

typedef struct
{
  gchar        *handler_name;
  GObject      *connect_object;
  GConnectFlags flags;
} HandlerData;

typedef struct
{
  GladePreviewer *window;
  gint          n_invocations;

  GSignalQuery  query;
  GObject      *object;
  GList        *handlers;
} SignalData;

static void
handler_data_free (gpointer udata)
{
  HandlerData *hd = udata;
  g_clear_object (&hd->connect_object);
  g_free (hd->handler_name);
  g_free (hd);
}

static void
signal_data_free (gpointer udata, GClosure *closure)
{
  SignalData *data = udata;

  g_list_free_full (data->handlers, handler_data_free);
  data->handlers = NULL;

  g_clear_object (&data->window);
  g_clear_object (&data->object);

  g_free (data);
}

static inline const gchar *
object_get_name (GObject *object)
{
  if (GTK_IS_BUILDABLE (object))
    return gtk_buildable_get_name (GTK_BUILDABLE (object));
  else
    return g_object_get_data (object, "gtk-builder-name");
}

static void
glade_handler_append (GString      *message,
                      GSignalQuery *query,
                      const gchar  *object,
                      GList        *handlers,
                      gboolean     after)
{
  GList *l;

  for (l = handlers; l; l = g_list_next (l))
    {
      HandlerData *hd = l->data;
      gboolean handler_after = (hd->flags & G_CONNECT_AFTER);
      gboolean swapped = (hd->flags & G_CONNECT_SWAPPED);
      GObject *obj = hd->connect_object;
      gint i;

      if ((after && !handler_after) || (!after && handler_after))
        continue;

      g_string_append_printf (message, "\n\t-> %s%s %s (%s%s%s",
                              g_type_name (query->return_type),
                              g_type_is_a (query->return_type, G_TYPE_OBJECT) ? " *" : "",
                              hd->handler_name,
                              (swapped) ? ((obj) ? G_OBJECT_TYPE_NAME (obj) : "") : g_type_name (query->itype),
                              (swapped) ? ((obj) ? " *" : "") : " *",
                              (swapped) ? ((obj) ? object_get_name (obj) : _("user_data")) : object);

      for (i = 1; i < query->n_params; i++)
        g_string_append_printf (message, ", %s%s", 
                                g_type_name (query->param_types[i]),
                                g_type_is_a (query->param_types[i], G_TYPE_OBJECT) ? " *" : "");

      g_string_append_printf (message, ", %s%s%s); ",
                              (swapped) ? g_type_name (query->itype) : ((obj) ? G_OBJECT_TYPE_NAME (obj) : ""),
                              (swapped) ? " *" : ((obj) ? " *" : ""),
                              (swapped) ? object : ((obj) ? object_get_name (obj) : _("user_data")));

      if (swapped && after)
        /* translators: GConnectFlags values */
        g_string_append (message, _("Swapped | After"));
      else if (swapped)
        /* translators: GConnectFlags value */
        g_string_append (message, _("Swapped"));
      else if (after)
        /* translators: GConnectFlags value */
        g_string_append (message, _("After"));
    }
}

static inline void
glade_handler_method_append (GString *msg, GSignalQuery *q, const gchar *flags)
{
  g_string_append_printf (msg, "\n\t%sClass->%s(); %s", g_type_name (q->itype),
                          q->signal_name, flags);
}

static void
on_handler_called (SignalData *data)
{
  GSignalQuery *query = &data->query;
  GObject *object = data->object;
  const gchar *object_name = object_get_name (object);
  GString *message = g_string_new ("");

  data->n_invocations++;

  if (data->n_invocations == 1)
    /* translators: this will be shown in glade previewer when a signal %s::%s is emited one time */
    g_string_append_printf (message, _("%s::%s emitted one time"),
                            G_OBJECT_TYPE_NAME (object), query->signal_name);
  else
    /* translators: this will be shown in glade previewer when a signal %s::%s is emited %d times */
    g_string_append_printf (message, _("%s::%s emitted %d times"),
                            G_OBJECT_TYPE_NAME (object), query->signal_name,
                            data->n_invocations);

  if (query->signal_flags & G_SIGNAL_RUN_FIRST)
    glade_handler_method_append (message, query, _("Run First"));

  glade_handler_append (message, query, object_name, data->handlers, FALSE);

  if (query->signal_flags & G_SIGNAL_RUN_LAST)
    glade_handler_method_append (message, query, _("Run Last"));

  glade_handler_append (message, query, object_name, data->handlers, TRUE);

  if (query->signal_flags & G_SIGNAL_RUN_CLEANUP)
    glade_handler_method_append (message, query, _("Run Cleanup"));

  glade_previewer_set_message (data->window, GTK_MESSAGE_INFO, message->str);

  if (data->window->priv->print_handlers)
    g_printf ("\n%s\n", message->str);

  g_string_free (message, TRUE);
}

/**
 * glade_previewer_connect_function:
 * @builder: a #GtkBuilder
 * @object: the #GObject triggering the signal
 * @signal_name: the name of the signal
 * @handler_name: the name of the c function handling the signal
 * @connect_object: the user_data #GObject to connect
 * @flags: #GConnectFlags used in the connection
 * @window: a #GladePreviewer
 * 
 * Function that collects every signal handler in @builder and shows them
 * in @window info bar when the callback is activated
 */
void
glade_previewer_connect_function (GtkBuilder   *builder,
                                  GObject      *object,
                                  const gchar  *signal_name,
                                  const gchar  *handler_name,
                                  GObject      *connect_object,
                                  GConnectFlags flags,
                                  gpointer      window)
{
  SignalData *data;
  HandlerData *hd;
  guint signal_id;
  gchar *key;

  g_return_if_fail (GLADE_IS_PREVIEWER (window));

  if (!(signal_id = g_signal_lookup (signal_name, G_OBJECT_TYPE (object))))
    return;

  key = g_strconcat ("glade-signal-data-", signal_name, NULL);
  data = g_object_get_data (object, key);

  if (!data)
    {
      data = g_new0 (SignalData, 1);

      data->window = g_object_ref (window);
      g_signal_query (signal_id, &data->query);
      data->object = g_object_ref (object);

      g_signal_connect_data (object, signal_name,
                             G_CALLBACK (on_handler_called),
                             data, signal_data_free, G_CONNECT_SWAPPED);

      g_object_set_data (object, key, data);
    }

  hd = g_new0 (HandlerData, 1);
  hd->handler_name = g_strdup (handler_name);
  hd->connect_object = connect_object ? g_object_ref (connect_object) : NULL;
  hd->flags = flags;

  data->handlers = g_list_append (data->handlers, hd);

  g_free (key);
}
