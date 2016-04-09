/*
 * glade-previewer.h
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

#ifndef _GLADE_PREVIEWER_H_
#define _GLADE_PREVIEWER_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_PREVIEWER             (glade_previewer_get_type ())
#define GLADE_PREVIEWER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PREVIEWER, GladePreviewer))
#define GLADE_PREVIEWER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PREVIEWER, GladePreviewerClass))
#define GLADE_IS_PREVIEWER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PREVIEWER))
#define GLADE_IS_PREVIEWER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PREVIEWER))
#define GLADE_PREVIEWER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_PREVIEWER, GladePreviewerClass))

typedef struct _GladePreviewerClass GladePreviewerClass;
typedef struct _GladePreviewer GladePreviewer;
typedef struct _GladePreviewerPrivate GladePreviewerPrivate;


struct _GladePreviewerClass
{
  GObjectClass parent_class;
};

struct _GladePreviewer
{
  GObject parent_instance;

  GladePreviewerPrivate *priv; 
};

GType      glade_previewer_get_type    (void) G_GNUC_CONST;

GObject   *glade_previewer_new         (void);

void       glade_previewer_set_widget  (GladePreviewer *preview,
                                        GtkWidget      *widget);

void       glade_previewer_present     (GladePreviewer *preview);

void       glade_previewer_set_print_handlers (GladePreviewer *preview,
                                               gboolean        print);

void       glade_previewer_set_message (GladePreviewer   *preview,
                                        GtkMessageType    type,
                                       const gchar       *message);

void       glade_previewer_set_css_file (GladePreviewer *preview,
                                         const gchar    *css_file);

void       glade_previewer_set_screenshot_extension (GladePreviewer *preview,
                                                     const gchar    *extension);

void       glade_previewer_screenshot  (GladePreviewer *preview,
                                        gboolean        wait,
                                        const gchar    *filename);

void       glade_previewer_set_slideshow_widgets (GladePreviewer *preview,
                                                  GSList         *objects);

void       glade_previewer_slideshow_save (GladePreviewer *preview,
                                           const gchar    *filename);

void       glade_previewer_connect_function (GtkBuilder   *builder,
                                             GObject      *object,
                                             const gchar  *signal_name,
                                             const gchar  *handler_name,
                                             GObject      *connect_object,
                                             GConnectFlags flags,
                                             gpointer      window);

G_END_DECLS

#endif /* _GLADE_PREVIEWER_H_ */
