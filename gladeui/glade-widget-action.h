/*
 * Copyright (C) 2007 Juan Pablo Ugarte.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#ifndef _GLADE_WIDGET_ACTION_H_
#define _GLADE_WIDGET_ACTION_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define GLADE_TYPE_WIDGET_ACTION             (glade_widget_action_get_type ())
#define GLADE_WIDGET_ACTION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_WIDGET_ACTION, GladeWidgetAction))
#define GLADE_WIDGET_ACTION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_WIDGET_ACTION, GladeWidgetActionClass))
#define GLADE_IS_WIDGET_ACTION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_WIDGET_ACTION))
#define GLADE_IS_WIDGET_ACTION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_WIDGET_ACTION))
#define GLADE_WIDGET_ACTION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_WIDGET_ACTION, GladeWidgetActionClass))
#define GLADE_TYPE_WIDGET_ACTION_DEF         glade_widget_action_def_get_type ()

typedef struct _GladeWidgetActionClass   GladeWidgetActionClass;
typedef struct _GladeWidgetAction        GladeWidgetAction;
typedef struct _GladeWidgetActionPrivate GladeWidgetActionPrivate;
typedef struct _GladeWidgetActionDef     GladeWidgetActionDef;

struct _GladeWidgetActionDef
{
  const gchar    *id;     /* The identifier of this action in the action tree */
  gchar          *path;   /* Full action path  */
  gchar          *label;  /* A translated label to show in the UI for this action */
  gchar          *stock;  /* If set, this stock item will be shown in the UI along side
                           * the label */
  gboolean        important;  /* If this action is important */

  GList          *actions;/* Recursive list of child actions */
};

struct _GladeWidgetAction
{
  GObject parent_instance;

  GladeWidgetActionPrivate *priv;
};

struct _GladeWidgetActionClass
{
  GObjectClass parent_class;

  void   (* glade_reserved1)   (void);
  void   (* glade_reserved2)   (void);
  void   (* glade_reserved3)   (void);
  void   (* glade_reserved4)   (void);
};


GType                 glade_widget_action_get_type      (void) G_GNUC_CONST;

void                  glade_widget_action_set_sensitive (GladeWidgetAction *action,
                                                         gboolean           sensitive);
gboolean              glade_widget_action_get_sensitive (GladeWidgetAction *action);
void                  glade_widget_action_set_visible   (GladeWidgetAction *action,
                                                         gboolean           visible);
gboolean              glade_widget_action_get_visible   (GladeWidgetAction *action);
GList                *glade_widget_action_get_children  (GladeWidgetAction *action);
GladeWidgetActionDef *glade_widget_action_get_def       (GladeWidgetAction *action);


GType                 glade_widget_action_def_get_type      (void) G_GNUC_CONST;
GladeWidgetActionDef *glade_widget_action_def_new           (const gchar          *path);
GladeWidgetActionDef *glade_widget_action_def_clone         (GladeWidgetActionDef *action);
void                  glade_widget_action_def_free          (GladeWidgetActionDef *action);
void                  glade_widget_action_def_set_label     (GladeWidgetActionDef *action,
                                                             const gchar          *label);
void                  glade_widget_action_def_set_stock     (GladeWidgetActionDef *action,
                                                             const gchar          *stock);
void                  glade_widget_action_def_set_important (GladeWidgetActionDef *action,
                                                             gboolean              important);


G_END_DECLS

#endif /* _GLADE_WIDGET_ACTION_H_ */
