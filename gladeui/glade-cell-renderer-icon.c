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

#include "config.h"
#include "glade-cell-renderer-icon.h"
#include "glade-marshallers.h"

static void glade_cell_renderer_icon_get_property (GObject      *object,
                                                   guint         param_id,
                                                   GValue       *value,
                                                   GParamSpec   *pspec);
static void glade_cell_renderer_icon_set_property (GObject      *object,
                                                   guint         param_id,
                                                   const GValue *value,
                                                   GParamSpec   *pspec);
static gboolean glade_cell_renderer_icon_activate (GtkCellRenderer     *cell,
                                                   GdkEvent            *event,
                                                   GtkWidget           *widget,
                                                   const gchar         *path,
                                                   const GdkRectangle  *background_area,
                                                   const GdkRectangle  *cell_area,
                                                   GtkCellRendererState flags);

struct _GladeCellRendererIconPrivate 
{
  guint active : 1;
  guint activatable : 1;
};

enum
{
  ACTIVATE,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_ACTIVATABLE,
  PROP_ACTIVE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];
static guint icon_cell_signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (GladeCellRendererIcon,
                            glade_cell_renderer_icon,
                            GTK_TYPE_CELL_RENDERER_PIXBUF)

static void glade_cell_renderer_icon_init (GladeCellRendererIcon *cellicon)
{
  cellicon->priv = glade_cell_renderer_icon_get_instance_private (cellicon);

  cellicon->priv->activatable = TRUE;
  cellicon->priv->active = FALSE;

  g_object_set (G_OBJECT (cellicon), "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
                NULL);
}

static void
glade_cell_renderer_icon_class_init (GladeCellRendererIconClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

  object_class->get_property = glade_cell_renderer_icon_get_property;
  object_class->set_property = glade_cell_renderer_icon_set_property;

  cell_class->activate = glade_cell_renderer_icon_activate;

  properties[PROP_ACTIVE] =
    g_param_spec_boolean ("active", "Icon state",
                          "The icon state of the button",
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_WRITABLE);

  properties[PROP_ACTIVATABLE] =
    g_param_spec_boolean ("activatable",
                          "Activatable",
                          "The icon button can be activated",
                          TRUE,
                          G_PARAM_READABLE | G_PARAM_WRITABLE);

  /* Install all properties */
  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  icon_cell_signals[ACTIVATE] =
      g_signal_new ("activate",
                    G_OBJECT_CLASS_TYPE (object_class),
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GladeCellRendererIconClass, activate),
                    NULL, NULL,
                    _glade_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
glade_cell_renderer_icon_get_property (GObject    *object,
                                       guint       param_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GladeCellRendererIcon *cellicon = GLADE_CELL_RENDERER_ICON (object);

  switch (param_id)
    {
      case PROP_ACTIVE:
        g_value_set_boolean (value, cellicon->priv->active);
        break;
      case PROP_ACTIVATABLE:
        g_value_set_boolean (value, cellicon->priv->activatable);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}


static void
glade_cell_renderer_icon_set_property (GObject      *object,
                                       guint         param_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GladeCellRendererIcon *cellicon = GLADE_CELL_RENDERER_ICON (object);

  switch (param_id)
    {
      case PROP_ACTIVE:
        cellicon->priv->active = g_value_get_boolean (value);
        break;
      case PROP_ACTIVATABLE:
        cellicon->priv->activatable = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

GtkCellRenderer *
glade_cell_renderer_icon_new (void)
{
  return g_object_new (GLADE_TYPE_CELL_RENDERER_ICON, NULL);
}

static gint
glade_cell_renderer_icon_activate (GtkCellRenderer     *cell,
                                   GdkEvent            *event,
                                   GtkWidget           *widget,
                                   const gchar         *path,
                                   const GdkRectangle  *background_area,
                                   const GdkRectangle  *cell_area,
                                   GtkCellRendererState flags)
{
  GladeCellRendererIcon *cellicon;

  cellicon = GLADE_CELL_RENDERER_ICON (cell);
  if (cellicon->priv->activatable)
    {
      g_signal_emit (cell, icon_cell_signals[ACTIVATE], 0, path);
      return TRUE;
    }

  return FALSE;
}

gboolean
glade_cell_renderer_icon_get_active (GladeCellRendererIcon *icon)
{
  g_return_val_if_fail (GLADE_IS_CELL_RENDERER_ICON (icon), FALSE);

  return icon->priv->active;
}

void
glade_cell_renderer_icon_set_active (GladeCellRendererIcon *icon,
                                     gboolean               setting)
{
  g_return_if_fail (GLADE_IS_CELL_RENDERER_ICON (icon));

  if (icon->priv->active != setting)
    {
      icon->priv->active = setting ? TRUE : FALSE;
      g_object_notify_by_pspec (G_OBJECT (icon), properties[PROP_ACTIVE]);
    }
}

gboolean
glade_cell_renderer_icon_get_activatable (GladeCellRendererIcon *icon)
{
  g_return_val_if_fail (GLADE_IS_CELL_RENDERER_ICON (icon), FALSE);

  return icon->priv->activatable;
}

void
glade_cell_renderer_icon_set_activatable (GladeCellRendererIcon *icon,
                                          gboolean               setting)
{
  g_return_if_fail (GLADE_IS_CELL_RENDERER_ICON (icon));

  if (icon->priv->activatable != setting)
    {
      icon->priv->activatable = setting ? TRUE : FALSE;
      g_object_notify_by_pspec (G_OBJECT (icon), properties[PROP_ACTIVATABLE]);
    }
}
