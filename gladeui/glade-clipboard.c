/*
 * glade-clipboard.c - An object for handling Cut/Copy/Paste.
 *
 * Copyright (C) 2001 The GNOME Foundation.
 *
 * Author(s):
 *      Archit Baweja <bighead@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */

#include "config.h"

/**
 * SECTION:glade-clipboard
 * @Short_Description: A list of #GladeWidget objects not in any #GladeProject.
 *
 * The #GladeClipboard is a singleton and is an accumulative shelf
 * of all cut or copied #GladeWidget in the application. A #GladeWidget
 * can be cut from one #GladeProject and pasted to another.
 */

#include <glib/gi18n-lib.h>
#include "glade.h"
#include "glade-clipboard.h"
#include "glade-widget.h"
#include "glade-placeholder.h"
#include "glade-project.h"

typedef struct _GladeClipboardPrivate
{
  GList     *widgets;       /* A list of GladeWidget's on the clipboard */
  gboolean   has_selection; /* TRUE if clipboard has selection */
} GladeClipboardPrivate;

enum
{
  PROP_0,
  PROP_HAS_SELECTION,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES];

G_DEFINE_TYPE_WITH_PRIVATE (GladeClipboard, glade_clipboard, G_TYPE_OBJECT);

static void
glade_project_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GladeClipboard *clipboard = GLADE_CLIPBOARD (object);
  GladeClipboardPrivate *priv = glade_clipboard_get_instance_private (clipboard);

  switch (prop_id)
    {
      case PROP_HAS_SELECTION:
        g_value_set_boolean (value, priv->has_selection);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_clipboard_class_init (GladeClipboardClass * klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = glade_project_get_property;

  properties[PROP_HAS_SELECTION] =
    g_param_spec_boolean ("has-selection",
                          "Has Selection",
                          "Whether clipboard has a selection of items to paste",
                          FALSE,
                          G_PARAM_READABLE);
  
  /* Install all properties */
  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
glade_clipboard_init (GladeClipboard *clipboard)
{
  GladeClipboardPrivate *priv = glade_clipboard_get_instance_private (clipboard);

  priv->widgets = NULL;
  priv->has_selection = FALSE;
}

static void
glade_clipboard_set_has_selection (GladeClipboard *clipboard,
                                   gboolean        has_selection)
{
  GladeClipboardPrivate *priv = glade_clipboard_get_instance_private (clipboard);

  if (priv->has_selection != has_selection)
    {
      priv->has_selection = has_selection;
      g_object_notify_by_pspec (G_OBJECT (clipboard), properties[PROP_HAS_SELECTION]);
    }

}

/**
 * glade_clipboard_get_has_selection:
 * @clipboard: a #GladeClipboard
 * 
 * Returns: TRUE if this clipboard has selected items to paste.
 */
gboolean
glade_clipboard_get_has_selection (GladeClipboard *clipboard)
{
  GladeClipboardPrivate *priv = glade_clipboard_get_instance_private (clipboard);

  g_return_val_if_fail (GLADE_IS_CLIPBOARD (clipboard), FALSE);

  return priv->has_selection;
}

/**
 * glade_clipboard_widgets:
 * @clipboard: a #GladeClipboard
 * 
 * Returns: (element-type GladeWidget) (transfer none): a #GList of #GladeWidgets
 */
GList *
glade_clipboard_widgets (GladeClipboard *clipboard)
{
  GladeClipboardPrivate *priv = glade_clipboard_get_instance_private (clipboard);

  g_return_val_if_fail (GLADE_IS_CLIPBOARD (clipboard), NULL);

  return priv->widgets;
}

/**
 * glade_clipboard_new:
 * 
 * Returns: a new #GladeClipboard object
 */
GladeClipboard *
glade_clipboard_new (void)
{
  return GLADE_CLIPBOARD (g_object_new (GLADE_TYPE_CLIPBOARD, NULL));
}

/**
 * glade_clipboard_add:
 * @clipboard: a #GladeClipboard
 * @widgets: (element-type GladeWidget): a #GList of #GladeWidgets
 * 
 * Adds @widgets to @clipboard.
 * This increases the reference count of each #GladeWidget in @widgets.
 */
void
glade_clipboard_add (GladeClipboard *clipboard, GList *widgets)
{
  GladeClipboardPrivate *priv = glade_clipboard_get_instance_private (clipboard);
  GladeWidget *widget;
  GList *list;

  g_return_if_fail (GLADE_IS_CLIPBOARD (clipboard));

  glade_clipboard_clear (clipboard);

  /*
   * Add the widgets to the list of children.
   */
  for (list = widgets; list && list->data; list = list->next)
    {
      widget = list->data;
      priv->widgets =
          g_list_prepend (priv->widgets, g_object_ref_sink (G_OBJECT (widget)));
    }

  glade_clipboard_set_has_selection (clipboard, TRUE);
}

/**
 * glade_clipboard_clear:
 * @clipboard: a #GladeClipboard
 * 
 * Removes all widgets from the @clipboard.
 */
void
glade_clipboard_clear (GladeClipboard *clipboard)
{
  GladeClipboardPrivate *priv = glade_clipboard_get_instance_private (clipboard);
  GladeWidget *widget;
  GList *list;

  g_return_if_fail (GLADE_IS_CLIPBOARD (clipboard));

  for (list = priv->widgets; list && list->data; list = list->next)
    {
      widget = list->data;

      g_object_unref (G_OBJECT (widget));
    }

  priv->widgets = 
    (g_list_free (priv->widgets), NULL);

  glade_clipboard_set_has_selection (clipboard, FALSE);
}
