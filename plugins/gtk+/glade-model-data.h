/*
 * Copyright (C) 2008 Tristan Van Berkom.
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
 *   Tristan Van Berkom <tvb@gnome.org>
 */
#ifndef _GLADE_MODEL_DATA_TREE_H_
#define _STV_CAP_H_

#include <glib.h>

G_BEGIN_DECLS

struct _GladeModelData
{
  GValue    value;
  gchar    *name;

  gboolean  i18n_translatable;
  gchar    *i18n_context;
  gchar    *i18n_comment;
};

typedef struct _GladeModelData         GladeModelData;


#define GLADE_TYPE_MODEL_DATA_TREE  (glade_model_data_tree_get_type())
#define GLADE_TYPE_EPROP_MODEL_DATA (glade_eprop_model_data_get_type())

G_MODULE_EXPORT
GType           glade_model_data_tree_get_type     (void) G_GNUC_CONST;
G_MODULE_EXPORT
GType           glade_eprop_model_data_get_type    (void) G_GNUC_CONST;


G_MODULE_EXPORT
GladeModelData *glade_model_data_new               (GType           type,
                                                    const gchar    *column_name);
G_MODULE_EXPORT
GladeModelData *glade_model_data_copy              (GladeModelData *data);
G_MODULE_EXPORT
void            glade_model_data_free              (GladeModelData *data);

G_MODULE_EXPORT
GNode          *glade_model_data_tree_copy         (GNode          *node);
G_MODULE_EXPORT
void            glade_model_data_tree_free         (GNode          *node);

G_MODULE_EXPORT
GladeModelData *glade_model_data_tree_get_data     (GNode          *data_tree, 
                                                    gint            row, 
                                                    gint            colnum);
G_MODULE_EXPORT
void            glade_model_data_insert_column     (GNode          *node,
                                                    GType           type,
                                                    const gchar    *column_name,
                                                    gint            nth);
G_MODULE_EXPORT
void            glade_model_data_remove_column     (GNode          *node,
                                                    gint            nth);
G_MODULE_EXPORT
void            glade_model_data_reorder_column    (GNode          *node,
                                                    gint            column,
                                                    gint            nth);
G_MODULE_EXPORT
gint            glade_model_data_column_index      (GNode          *node,
                                                    const gchar    *column_name);
G_MODULE_EXPORT
void            glade_model_data_column_rename     (GNode          *node,
                                                    const gchar    *column_name,
                                                    const gchar    *new_name);


G_END_DECLS

#endif /* _GLADE_MODEL_DATA_H_ */
