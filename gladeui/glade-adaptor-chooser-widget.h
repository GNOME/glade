/*
 * glade-adaptor-chooser-widget.h
 *
 * Copyright (C) 2014-2017 Juan Pablo Ugarte
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
 * Authors:
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#ifndef _GLADE_ADAPTOR_CHOOSER_WIDGET_H_
#define _GLADE_ADAPTOR_CHOOSER_WIDGET_H_

#include <gladeui/glade-widget-adaptor.h>

G_BEGIN_DECLS

#define GLADE_TYPE_ADAPTOR_CHOOSER_WIDGET           (_glade_adaptor_chooser_widget_get_type ())
#define GLADE_ADAPTOR_CHOOSER_WIDGET(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_ADAPTOR_CHOOSER_WIDGET, _GladeAdaptorChooserWidget))
#define GLADE_ADAPTOR_CHOOSER_WIDGET_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_ADAPTOR_CHOOSER_WIDGET, _GladeAdaptorChooserWidgetClass))
#define GLADE_IS_ADAPTOR_CHOOSER_WIDGET(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_ADAPTOR_CHOOSER_WIDGET))
#define GLADE_IS_ADAPTOR_CHOOSER_WIDGET_CLASS(klass)(G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_ADAPTOR_CHOOSER_WIDGET))
#define GLADE_ADAPTOR_CHOOSER_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_ADAPTOR_CHOOSER_WIDGET, _GladeAdaptorChooserWidgetClass))

typedef struct _GladeAdaptorChooserWidgetClass   _GladeAdaptorChooserWidgetClass;
typedef struct _GladeAdaptorChooserWidget        _GladeAdaptorChooserWidget;
typedef struct _GladeAdaptorChooserWidgetPrivate _GladeAdaptorChooserWidgetPrivate;

typedef enum
{
  GLADE_ADAPTOR_CHOOSER_WIDGET_WIDGET          = 1 << 0,
  GLADE_ADAPTOR_CHOOSER_WIDGET_TOPLEVEL        = 1 << 1,
  GLADE_ADAPTOR_CHOOSER_WIDGET_SKIP_TOPLEVEL   = 1 << 2,
  GLADE_ADAPTOR_CHOOSER_WIDGET_SKIP_DEPRECATED = 1 << 3
} _GladeAdaptorChooserWidgetFlags;

struct _GladeAdaptorChooserWidgetClass
{
  GtkBoxClass parent_class;
};

struct _GladeAdaptorChooserWidget
{
  GtkBox parent_instance;
};

GType _glade_adaptor_chooser_widget_get_type (void) G_GNUC_CONST;

GtkWidget *_glade_adaptor_chooser_widget_new (_GladeAdaptorChooserWidgetFlags  flags,
                                              GladeProject                    *project);

void      _glade_adaptor_chooser_widget_set_project (_GladeAdaptorChooserWidget *chooser,
                                                     GladeProject               *project);

void      _glade_adaptor_chooser_widget_populate    (_GladeAdaptorChooserWidget *chooser);

void      _glade_adaptor_chooser_widget_add_catalog (_GladeAdaptorChooserWidget *chooser,
                                                     GladeCatalog               *catalog);

void      _glade_adaptor_chooser_widget_add_group   (_GladeAdaptorChooserWidget *chooser,
                                                     GladeWidgetGroup           *group);

G_END_DECLS

#endif /* _GLADE_ADAPTOR_CHOOSER_WIDGET_H_ */

