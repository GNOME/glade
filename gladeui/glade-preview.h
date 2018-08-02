/*
 * Copyright (C) 2010 Marco Diego Aurélio Mesquita
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Marco Diego Aurélio Mesquita <marcodiegomesquita@gmail.com>
 */

#ifndef _GLADE_PREVIEW_H_
#define _GLADE_PREVIEW_H_

#include <glib-object.h>
#include <glib.h>
#include <glib/gstdio.h>

G_BEGIN_DECLS
#define GLADE_TYPE_PREVIEW             (glade_preview_get_type ())
#define GLADE_PREVIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PREVIEW, GladePreview))
#define GLADE_PREVIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PREVIEW, GladePreviewClass))
#define GLADE_IS_PREVIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PREVIEW))
#define GLADE_IS_PREVIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PREVIEW))
#define GLADE_PREVIEW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_PREVIEW, GladePreviewClass))
typedef struct _GladePreview        GladePreview;
typedef struct _GladePreviewClass   GladePreviewClass;
typedef struct _GladePreviewPrivate GladePreviewPrivate;

struct _GladePreview
{
  GObject parent_instance;

  GladePreviewPrivate *priv;
};

struct _GladePreviewClass
{
  GObjectClass parent_class;
};

GType         glade_preview_get_type    (void) G_GNUC_CONST;
GladePreview *glade_preview_launch      (GladeWidget  *widget,
                                         const gchar  *buffer);
void          glade_preview_update      (GladePreview *preview,
                                         const gchar  *buffer);
GladeWidget  *glade_preview_get_widget  (GladePreview *preview);
GPid          glade_preview_get_pid     (GladePreview *preview);

G_END_DECLS

#endif /* _GLADE_PREVIEW_H_ */
