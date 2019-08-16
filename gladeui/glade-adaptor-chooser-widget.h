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

#define GLADE_TYPE_ADAPTOR_CHOOSER_WIDGET _glade_adaptor_chooser_widget_get_type ()
G_DECLARE_DERIVABLE_TYPE (_GladeAdaptorChooserWidget, _glade_adaptor_chooser_widget, GLADE, ADAPTOR_CHOOSER_WIDGET, GtkBox)

typedef enum
{
  GLADE_ADAPTOR_CHOOSER_WIDGET_WIDGET          = 1 << 0,
  GLADE_ADAPTOR_CHOOSER_WIDGET_TOPLEVEL        = 1 << 1,
  GLADE_ADAPTOR_CHOOSER_WIDGET_SKIP_TOPLEVEL   = 1 << 2,
  GLADE_ADAPTOR_CHOOSER_WIDGET_SKIP_DEPRECATED = 1 << 3
} _GladeAdaptorChooserWidgetFlags;

struct __GladeAdaptorChooserWidgetClass
{
  GtkBoxClass parent_class;
};

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

