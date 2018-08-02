/*
 * glade-design-layout.h
 *
 * Copyright (C) 2006-2007 Vincent Geddes
 *
 * Authors:
 *   Vincent Geddes <vgeddes@gnome.org>
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

#ifndef __GLADE_DESIGN_LAYOUT_H__
#define __GLADE_DESIGN_LAYOUT_H__

#include <gtk/gtk.h>
#include "glade-design-view.h"

G_BEGIN_DECLS

#define GLADE_TYPE_DESIGN_LAYOUT              (glade_design_layout_get_type ())
#define GLADE_DESIGN_LAYOUT(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_DESIGN_LAYOUT, GladeDesignLayout))
#define GLADE_DESIGN_LAYOUT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_DESIGN_LAYOUT, GladeDesignLayoutClass))
#define GLADE_IS_DESIGN_LAYOUT(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_DESIGN_LAYOUT))
#define GLADE_IS_DESIGN_LAYOUT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_DESIGN_LAYOUT))
#define GLADE_DESIGN_LAYOUT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_DESIGN_LAYOUT, GladeDesignLayoutClass))

typedef struct _GladeDesignLayout         GladeDesignLayout;
typedef struct _GladeDesignLayoutPrivate  GladeDesignLayoutPrivate;
typedef struct _GladeDesignLayoutClass    GladeDesignLayoutClass;

struct _GladeDesignLayout
{
  GtkBin     parent_instance;

  GladeDesignLayoutPrivate *priv;
};

struct _GladeDesignLayoutClass
{
  GtkBinClass parent_class;

  void   (* glade_reserved0)   (void);
  void   (* glade_reserved1)   (void);
  void   (* glade_reserved2)   (void);
  void   (* glade_reserved3)   (void);
  void   (* glade_reserved4)   (void);
};


GType        glade_design_layout_get_type   (void) G_GNUC_CONST;

GtkWidget   *_glade_design_layout_new       (GladeDesignView *view);

gboolean     _glade_design_layout_do_event  (GladeDesignLayout *layout,
                                             GdkEvent *event);

G_END_DECLS

#endif /* __GLADE_DESIGN_LAYOUT_H__ */
