/*
 * glade-preview-window.h
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

#ifndef _GLADE_PREVIEW_WINDOW_H_
#define _GLADE_PREVIEW_WINDOW_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_PREVIEW_WINDOW             (glade_preview_window_get_type ())
#define GLADE_PREVIEW_WINDOW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PREVIEW_WINDOW, GladePreviewWindow))
#define GLADE_PREVIEW_WINDOW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PREVIEW_WINDOW, GladePreviewWindowClass))
#define GLADE_IS_PREVIEW_WINDOW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PREVIEW_WINDOW))
#define GLADE_IS_PREVIEW_WINDOW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PREVIEW_WINDOW))
#define GLADE_PREVIEW_WINDOW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_PREVIEW_WINDOW, GladePreviewWindowClass))

typedef struct _GladePreviewWindowClass GladePreviewWindowClass;
typedef struct _GladePreviewWindow GladePreviewWindow;
typedef struct _GladePreviewWindowPrivate GladePreviewWindowPrivate;


struct _GladePreviewWindowClass
{
  GtkWindowClass parent_class;
};

struct _GladePreviewWindow
{
  GtkWindow parent_instance;

  GladePreviewWindowPrivate *priv; 
};

GType      glade_preview_window_get_type    (void) G_GNUC_CONST;

GtkWidget *glade_preview_window_new         (void);

void       glade_preview_window_set_widget  (GladePreviewWindow *window,
                                             GtkWidget          *widget);

void       glade_preview_window_set_print_handlers (GladePreviewWindow *window,
                                                    gboolean            print);

void       glade_preview_window_set_message (GladePreviewWindow *window,
                                             GtkMessageType      type,
                                             const gchar        *message);

void       glade_preview_window_set_css_file (GladePreviewWindow *window,
                                              const gchar        *css_file);

void       glade_preview_window_set_screenshot_extension (GladePreviewWindow *window,
                                                          const gchar        *extension);

void       glade_preview_window_screenshot  (GladePreviewWindow *window,
                                             gboolean wait,
                                             const gchar *filename);

void       glade_preview_window_slideshow_save (GladePreviewWindow *window,
                                                const gchar        *filename);

void       glade_preview_window_connect_function (GtkBuilder   *builder,
                                                  GObject      *object,
                                                  const gchar  *signal_name,
                                                  const gchar  *handler_name,
                                                  GObject      *connect_object,
                                                  GConnectFlags flags,
                                                  gpointer      window);

G_END_DECLS

#endif /* _GLADE_PREVIEW_WINDOW_H_ */
