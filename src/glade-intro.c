/*
 * glade-intro.c
 *
 * Copyright (C) 2017-2018 Juan Pablo Ugarte
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

#include "glade-intro.h"

typedef struct
{
  GtkWidget          *widget;
  const gchar        *name;
  const gchar        *widget_name;
  const gchar        *text;
  GladeIntroPosition  position;
  gint                delay;
} ScriptNode;

typedef struct
{
  GtkWidget  *toplevel;

  GList      *script;      /* List of (ScriptNode *) */

  GtkPopover *popover;     /* Popover to show the script text */

  guint       timeout_id;  /* Timeout id for running the script */
  GList      *current;     /* Current script node */

  gboolean    hiding_node;
} GladeIntroPrivate;

struct _GladeIntro
{
  GObject parent_instance;
};

enum
{
  PROP_0,
  PROP_TOPLEVEL,
  PROP_STATE,

  N_PROPERTIES
};

enum
{
  SHOW_NODE,
  HIDE_NODE,

  LAST_SIGNAL
};

static guint intro_signals[LAST_SIGNAL] = { 0 };

static GParamSpec *properties[N_PROPERTIES];

G_DEFINE_TYPE_WITH_PRIVATE (GladeIntro, glade_intro, G_TYPE_OBJECT);

#define GET_PRIVATE(d) ((GladeIntroPrivate *) glade_intro_get_instance_private((GladeIntro*)d))

static void
glade_intro_init (GladeIntro *intro)
{
}

static void
glade_intro_finalize (GObject *object)
{
  GladeIntroPrivate *priv = GET_PRIVATE (object);

  if (priv->timeout_id)
    {
      g_source_remove (priv->timeout_id);
      priv->timeout_id = 0;
    }

  gtk_popover_set_relative_to (priv->popover, NULL);
  g_clear_object (&priv->popover);

  G_OBJECT_CLASS (glade_intro_parent_class)->finalize (object);
}

static void
glade_intro_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  g_return_if_fail (GLADE_IS_INTRO (object));

  switch (prop_id)
    {
      case PROP_TOPLEVEL:
        glade_intro_set_toplevel (GLADE_INTRO (object), g_value_get_object (value));
      break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
glade_intro_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  GladeIntroPrivate *priv;

  g_return_if_fail (GLADE_IS_INTRO (object));
  priv = GET_PRIVATE (object);

  switch (prop_id)
    {
      case PROP_TOPLEVEL:
        g_value_set_object (value, priv->toplevel);
      break;
      case PROP_STATE:
        if (priv->timeout_id)
          g_value_set_enum (value, GLADE_INTRO_STATE_PLAYING);
        else if (priv->current)
          g_value_set_enum (value, GLADE_INTRO_STATE_PAUSED);
        else
          g_value_set_enum (value, GLADE_INTRO_STATE_NULL);
      break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GType
glade_intro_state_get_type (void)
{
    static GType etype = 0;
    if (G_UNLIKELY(etype == 0)) {
        static const GEnumValue values[] = {
            { GLADE_INTRO_STATE_NULL, "GLADE_INTRO_STATE_NULL", "null" },
            { GLADE_INTRO_STATE_PLAYING, "GLADE_INTRO_STATE_PLAYING", "playing" },
            { GLADE_INTRO_STATE_PAUSED, "GLADE_INTRO_STATE_PAUSED", "paused" },
            { 0, NULL, NULL }
        };
        etype = g_enum_register_static (g_intern_static_string ("GladeIntroStatus"), values);
    }
    return etype;
}

static void
glade_intro_class_init (GladeIntroClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = glade_intro_finalize;
  object_class->set_property = glade_intro_set_property;
  object_class->get_property = glade_intro_get_property;

  /* Properties */
  properties[PROP_TOPLEVEL] =
    g_param_spec_object ("toplevel", "Toplevel",
                         "The main toplevel from where to get the widgets",
                         GTK_TYPE_WINDOW,
                         G_PARAM_READWRITE);
  properties[PROP_STATE] =
    g_param_spec_enum ("state", "State",
                       "Playback state",
                       glade_intro_state_get_type (),
                       GLADE_INTRO_STATE_NULL,
                       G_PARAM_READABLE);

  intro_signals[SHOW_NODE] =
    g_signal_new ("show-node", G_OBJECT_CLASS_TYPE (klass), 0, 0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  G_TYPE_STRING,
                  GTK_TYPE_WIDGET);
  intro_signals[HIDE_NODE] =
    g_signal_new ("hide-node", G_OBJECT_CLASS_TYPE (klass), 0, 0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  G_TYPE_STRING,
                  GTK_TYPE_WIDGET);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

/* Public API */

GladeIntro *
glade_intro_new (GtkWindow *toplevel)
{
  return (GladeIntro*) g_object_new (GLADE_TYPE_INTRO, "toplevel", toplevel, NULL);
}

static inline const gchar *
widget_get_name (GtkWidget *widget)
{
  const gchar *name;

  if (!widget)
    return NULL;

  if ((name = gtk_widget_get_name (widget)) &&
      g_strcmp0 (name, G_OBJECT_TYPE_NAME (widget)))
    return name;

  if (GTK_IS_BUILDABLE (widget) &&
      (name = gtk_buildable_get_name (GTK_BUILDABLE (widget))) &&
      !g_str_has_prefix (name, "___object_"))
    return name;

  return NULL;
}

typedef struct
{
  const gchar *name;
  GtkWidget *widget;
} FindData;

static void
find_widget_forall (GtkWidget *widget, gpointer user_data)
{
  FindData *data = user_data;

  if (data->widget)
    return;

  if (g_strcmp0 (widget_get_name (widget), data->name) == 0 &&
      gtk_widget_is_visible (widget))
    {
      data->widget = widget;
      return;
    }

  if (GTK_IS_CONTAINER (widget))
    gtk_container_forall ((GtkContainer *) widget, find_widget_forall, data);
}

static inline GtkWidget *
toplevel_get_widget (GtkWidget *widget, const gchar *name)
{
  FindData data = { name, NULL };

  if (!widget || !name)
    return NULL;

  find_widget_forall (widget, &data);

  return data.widget;
}

void
glade_intro_set_toplevel (GladeIntro *intro, GtkWindow *toplevel)
{
  GladeIntroPrivate *priv;
  ScriptNode *node;
  GList *l;

  g_return_if_fail (GLADE_IS_INTRO (intro));
  priv = GET_PRIVATE (intro);

  if (priv->toplevel == (GtkWidget *) toplevel)
    return;

  g_clear_object (&priv->toplevel);

  if (toplevel)
    {
      priv->toplevel = (GtkWidget *) g_object_ref (toplevel);

      for (l = priv->script; l && (node = l->data); l = g_list_next (l))
        node->widget = toplevel_get_widget (priv->toplevel, node->widget_name);
    }
  else
    {
      for (l = priv->script; l && (node = l->data); l = g_list_next (l))
        node->widget = NULL;
    }
}

void
glade_intro_script_add (GladeIntro         *intro,
                        const gchar        *name,
                        const gchar        *widget,
                        const gchar        *text,
                        GladeIntroPosition  position,
                        gdouble             delay)
{
  GladeIntroPrivate *priv;
  ScriptNode *node;

  g_return_if_fail (GLADE_IS_INTRO (intro));
  priv = GET_PRIVATE (intro);

  node = g_new0 (ScriptNode, 1);
  node->name        = name;
  node->widget_name = widget;
  node->text        = text;
  node->position    = position;
  node->delay       = delay * 1000;

  if (priv->toplevel && widget)
    node->widget = toplevel_get_widget (priv->toplevel, widget);

  priv->script = g_list_append (priv->script, node);
}

static gboolean script_play (gpointer data);

static void
on_popover_closed (GtkPopover *popover, GladeIntro *intro)
{
  glade_intro_pause (intro);
}

static void
hide_current_node (GladeIntro *intro)
{
  GladeIntroPrivate *priv = GET_PRIVATE (intro);
  ScriptNode *node;

  if (priv->hiding_node)
    return;
  priv->hiding_node = TRUE;
  if (priv->popover)
    {
      g_signal_handlers_disconnect_by_func (priv->popover, on_popover_closed, intro);
      gtk_popover_popdown (priv->popover);
      g_clear_object (&priv->popover);
    }

  if (priv->current && (node = priv->current->data))
    {
      if (node->widget)
        gtk_style_context_remove_class (gtk_widget_get_style_context (node->widget),
                                        "glade-intro-highlight");
      g_signal_emit (intro, intro_signals[HIDE_NODE], 0, node->name, node->widget);
    }

  /* Set next node */
  priv->current = (priv->current) ? g_list_next (priv->current) : NULL;

  priv->hiding_node = FALSE;
}

static gboolean
script_transition (gpointer data)
{
  GladeIntroPrivate *priv = GET_PRIVATE (data);

  priv->timeout_id = g_timeout_add (250, script_play, data);
  hide_current_node (data);

  return G_SOURCE_REMOVE;
}

static GtkWidget *
glade_intro_popover_new (GladeIntro *intro, const gchar *text)
{
  GtkWidget *popover, *box, *image, *label;

  popover = gtk_popover_new (NULL);
  label = gtk_label_new (text);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  image = gtk_image_new_from_icon_name ("dialog-information-symbolic", GTK_ICON_SIZE_DIALOG);

  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_max_width_chars (GTK_LABEL (label), 28);

  gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (popover), box);

  gtk_style_context_add_class (gtk_widget_get_style_context (popover), "glade-intro");
  g_signal_connect (popover, "closed", G_CALLBACK (on_popover_closed), intro);

  gtk_widget_show_all (box);

  return popover;
}

static gboolean
script_play (gpointer data)
{
  GladeIntroPrivate *priv = GET_PRIVATE (data);
  GtkStyleContext *context;
  ScriptNode *node;

  priv->timeout_id = 0;

  if (!priv->current || !(node = priv->current->data))
    return G_SOURCE_REMOVE;

  if (node->widget && node->text)
    {
      /* Ensure the widget is visible */
      if (!gtk_widget_is_visible (node->widget))
        {
          GtkWidget *parent;
          /* if the widget is inside a popover pop it up */
          if ((parent = gtk_widget_get_ancestor (node->widget, GTK_TYPE_POPOVER)))
            gtk_popover_popup (GTK_POPOVER (parent));
        }

      context = gtk_widget_get_style_context (node->widget);
      gtk_style_context_add_class (context, "glade-intro-highlight");

      /* Create popover */
      priv->popover = (GtkPopover *) g_object_ref_sink (glade_intro_popover_new (data, node->text));
      gtk_popover_set_relative_to (priv->popover, node->widget);

      if (node->position == GLADE_INTRO_BOTTOM)
        gtk_popover_set_position (priv->popover, GTK_POS_BOTTOM);
      else if (node->position == GLADE_INTRO_LEFT)
        gtk_popover_set_position (priv->popover, GTK_POS_LEFT);
      else if (node->position == GLADE_INTRO_RIGHT)
        gtk_popover_set_position (priv->popover, GTK_POS_RIGHT);
      else if (node->position == GLADE_INTRO_CENTER)
        {
          GdkRectangle rect = {
            gtk_widget_get_allocated_width (node->widget)/2,
            gtk_widget_get_allocated_height (node->widget)/2,
            4, 4
          };

          gtk_popover_set_pointing_to (priv->popover, &rect);
          gtk_popover_set_position (priv->popover, GTK_POS_TOP);
        }
    }

  g_signal_emit (data, intro_signals[SHOW_NODE], 0, node->name, node->widget);

  if (priv->popover)
    gtk_popover_popup (priv->popover);

  priv->timeout_id = g_timeout_add (node->delay, script_transition, data);

  return G_SOURCE_REMOVE;
}

void
glade_intro_play (GladeIntro *intro)
{
  GladeIntroPrivate *priv;

  g_return_if_fail (GLADE_IS_INTRO (intro));
  priv = GET_PRIVATE (intro);

  if (priv->script == NULL)
    return;

  if (priv->current == NULL)
    priv->current = priv->script;

  script_play (intro);

  g_object_notify_by_pspec (G_OBJECT (intro), properties[PROP_STATE]);
}

void
glade_intro_pause (GladeIntro *intro)
{
  GladeIntroPrivate *priv;

  g_return_if_fail (GLADE_IS_INTRO (intro));
  priv = GET_PRIVATE (intro);

  if (priv->timeout_id)
    g_source_remove (priv->timeout_id);

  priv->timeout_id = 0;
  hide_current_node (intro);

  g_object_notify_by_pspec (G_OBJECT (intro), properties[PROP_STATE]);
}

void
glade_intro_stop (GladeIntro *intro)
{
  GladeIntroPrivate *priv;

  g_return_if_fail (GLADE_IS_INTRO (intro));
  priv = GET_PRIVATE (intro);

  glade_intro_pause (intro);
  priv->current = NULL;

  g_object_notify_by_pspec (G_OBJECT (intro), properties[PROP_STATE]);
}
