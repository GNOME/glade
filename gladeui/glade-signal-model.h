/*
 * glade-signal-model.h
 * Copyright (C) Johannes Schmid 2010 <jhs@gnome.org>
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
 */

#ifndef _GLADE_SIGNAL_MODEL_H_
#define _GLADE_SIGNAL_MODEL_H_

#include <glib-object.h>
#include <gladeui/glade-widget.h>

G_BEGIN_DECLS

#define GLADE_TYPE_SIGNAL_MODEL             (glade_signal_model_get_type ())
#define GLADE_SIGNAL_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_SIGNAL_MODEL, GladeSignalModel))
#define GLADE_SIGNAL_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_SIGNAL_MODEL, GladeSignalModelClass))
#define GLADE_IS_SIGNAL_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_SIGNAL_MODEL))
#define GLADE_IS_SIGNAL_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_SIGNAL_MODEL))
#define GLADE_SIGNAL_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_SIGNAL_MODEL, GladeSignalModelClass))

typedef struct _GladeSignalModelClass GladeSignalModelClass;
typedef struct _GladeSignalModel GladeSignalModel;
typedef struct _GladeSignalModelPrivate GladeSignalModelPrivate;

typedef enum
{
  GLADE_SIGNAL_COLUMN_NAME,
  GLADE_SIGNAL_COLUMN_SHOW_NAME,
  GLADE_SIGNAL_COLUMN_HANDLER,
  GLADE_SIGNAL_COLUMN_OBJECT,
  GLADE_SIGNAL_COLUMN_SWAP,
  GLADE_SIGNAL_COLUMN_AFTER,
  GLADE_SIGNAL_COLUMN_TOOLTIP,
  GLADE_SIGNAL_COLUMN_SIGNAL,
  GLADE_SIGNAL_COLUMN_DETAIL,
  GLADE_SIGNAL_N_COLUMNS
} GladeSignalModelColumns;

struct _GladeSignalModelClass
{
  GObjectClass parent_class;
};

struct _GladeSignalModel
{
  GObject parent_instance;

  GladeSignalModelPrivate *priv;
};

GLADEUI_EXPORTS
GType         glade_signal_model_get_type (void) G_GNUC_CONST;
GLADEUI_EXPORTS
GtkTreeModel *glade_signal_model_new      (GladeWidget *widget,
                                           GHashTable  *signals);

G_END_DECLS

#endif /* _GLADE_SIGNAL_MODEL_H_ */
