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

#include <config.h>

/**
 * SECTION:glade-preview
 * @Short_Description: The glade preview launch/kill interface.
 *
 * This object owns all data that is needed to keep a preview. It stores
 * the GIOChannel used for communication between glade and glade-previewer,
 * the event source id for a watch  (in the case a watch is used to monitor
 * the communication channel), the previewed widget and the pid of the
 * corresponding glade-previewer.
 * 
 */


#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "glade.h"
#include "glade-preview.h"
#include "glade-project.h"
#include "glade-app.h"

#include "glade-preview-tokens.h"

#ifdef G_OS_WIN32
#define GLADE_PREVIEWER "glade-previewer.exe"
#else
#define GLADE_PREVIEWER "glade-previewer"
#endif

/* Private data for glade-preview */
struct _GladePreviewPrivate
{
  GIOChannel *channel;          /* Channel user for communication between glade and glade-previewer */
  guint watch;                  /* Event source id used to monitor the channel */
  GladeWidget *previewed_widget;
  GPid pid;                     /* Pid of the corresponding glade-previewer process */
};

G_DEFINE_TYPE_WITH_PRIVATE (GladePreview, glade_preview, G_TYPE_OBJECT);

enum
{
  PREVIEW_EXITS,
  LAST_SIGNAL
};

static guint glade_preview_signals[LAST_SIGNAL] = { 0 };

/**
 * glade_preview_kill
 * @preview: a #GladePreview that will be killed.
 *
 * Uses the communication channel and protocol to send the "<quit>" token to the
 * glade-previewer telling it to commit suicide.
 *
 */
static void
glade_preview_kill (GladePreview *preview)
{
  const gchar *quit = QUIT_TOKEN;
  GIOChannel *channel;
  GError *error = NULL;
  gsize size;

  channel = preview->priv->channel;
  g_io_channel_write_chars (channel, quit, strlen (quit), &size, &error);

  if (size != strlen (quit) && error != NULL)
    {
      g_warning ("Error passing quit signal trough pipe: %s", error->message);
      g_error_free (error);
    }

  g_io_channel_flush (channel, &error);
  if (error != NULL)
    {
      g_warning ("Error flushing channel: %s", error->message);
      g_error_free (error);
    }

  g_io_channel_shutdown (channel, TRUE, &error);
  if (error != NULL)
    {
      g_warning ("Error shutting down channel: %s", error->message);
      g_error_free (error);
    }
}

static void
glade_preview_dispose (GObject *gobject)
{
  GladePreview *self = GLADE_PREVIEW (gobject);

  if (self->priv->watch)
    {
      g_source_remove (self->priv->watch);
      glade_preview_kill (self);
    }

  if (self->priv->channel)
    {
      g_io_channel_unref (self->priv->channel);
      self->priv->channel = NULL;
    }

  G_OBJECT_CLASS (glade_preview_parent_class)->dispose (gobject);
}

/* We have to use finalize because of the signal that is sent in dispose */
static void
glade_preview_finalize (GObject *gobject)
{
  G_OBJECT_CLASS (glade_preview_parent_class)->finalize (gobject);
}

static void
glade_preview_class_init (GladePreviewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose  = glade_preview_dispose;
  gobject_class->finalize = glade_preview_finalize;

  /**
   * GladePreview::exits:
   * @gladepreview: the #GladePreview which received the signal.
   * @gladeproject: the #GladeProject associated with the preview.
   *
   * Emitted when @preview exits.
   */
  glade_preview_signals[PREVIEW_EXITS] =
      g_signal_new ("exits",
                    G_TYPE_FROM_CLASS (gobject_class),
                    G_SIGNAL_RUN_FIRST,
                    0, NULL, NULL,
                    g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void
glade_preview_init (GladePreview *self)
{
  GladePreviewPrivate *priv;

  self->priv = priv = glade_preview_get_instance_private (self);
  priv->channel = NULL;
}

static void
glade_preview_internal_watch (GPid pid, gint status, gpointer data)
{
  GladePreview *preview = GLADE_PREVIEW (data);

  preview->priv->watch = 0;

  /* This means a preview exited. We'll now signal it */
  g_signal_emit (preview, glade_preview_signals[PREVIEW_EXITS], 0);
}

/**
 * glade_preview_launch:
 * @widget: Pointer to a local instance of the widget that will be previewed.
 * @buffer: Contents of an xml definition of the interface which will be previewed
 *
 * Creates a new #GladePreview and launches glade-previewer to preview it.
 *
 * Returns: a new #GladePreview or NULL if launch fails.
 * 
 */
GladePreview *
glade_preview_launch (GladeWidget *widget, const gchar *buffer)
{
  GPid pid;
  GError *error = NULL;
  gchar *argv[10], *executable;
  gint child_stdin;
  gsize bytes_written;
  GIOChannel *output;
  GladePreview *preview = NULL;
  const gchar *css_provider, *filename;
  GladeProject *project;
  gchar *name;
  gint i;

  g_return_val_if_fail (GLADE_IS_WIDGET (widget), NULL);

  executable = g_find_program_in_path (GLADE_PREVIEWER);

  project = glade_widget_get_project (widget);
  filename = glade_project_get_path (project);
  name = (filename) ? NULL : glade_project_get_name (project);
  
  argv[0] = executable;
  argv[1] = "--listen";
  argv[2] = "--toplevel";
  argv[3] = (gchar *) glade_widget_get_name (widget);
  argv[4] = "--filename";
  argv[5] = (filename) ? (gchar *) filename : name;

  i = 5;
  if (glade_project_get_template (project))
    argv[++i] = "--template";
    
  argv[++i] = NULL;

  css_provider = glade_project_get_css_provider_path (glade_widget_get_project (widget));
  if (css_provider)
    {
      argv[i] = "--css";
      argv[++i] = (gchar *) css_provider;
      argv[++i] = NULL;
    }
  
  if (g_spawn_async_with_pipes (NULL,
                                argv,
                                NULL, G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL,
                                &pid, &child_stdin, NULL, NULL,
                                &error) == FALSE)
    {
      g_warning (_("Error launching previewer: %s\n"), error->message);
      glade_util_ui_message (glade_app_get_window (),
                             GLADE_UI_ERROR, NULL,
                             _("Failed to launch preview: %s.\n"),
                             error->message);
      g_error_free (error);
      g_free (executable);
      g_free (name);
      return NULL;
    }

#ifdef G_OS_WIN32
  output = g_io_channel_win32_new_fd (child_stdin);
#else
  output = g_io_channel_unix_new (child_stdin);
#endif

  g_io_channel_write_chars (output, buffer, strlen (buffer), &bytes_written,
                            &error);

  if (bytes_written != strlen (buffer) && error != NULL)
    {
      g_warning ("Error passing UI trough pipe: %s", error->message);
      g_error_free (error);
    }

  g_io_channel_flush (output, &error);
  if (error != NULL)
    {
      g_warning ("Error flushing UI trough pipe: %s", error->message);
      g_error_free (error);
    }

  /* Setting up preview data */
  preview                         = g_object_new (GLADE_TYPE_PREVIEW, NULL);
  preview->priv->channel          = output;
  preview->priv->previewed_widget = widget;
  preview->priv->pid              = pid;

  preview->priv->watch = 
    g_child_watch_add (preview->priv->pid,
                       glade_preview_internal_watch,
                       preview);

  g_free (executable);
  g_free (name);

  return preview;
}

void
glade_preview_update (GladePreview *preview, const gchar  *buffer)
{
  const gchar *update_token = UPDATE_TOKEN;
  gchar *update;
  GIOChannel *channel;
  GError *error = NULL;
  gsize size;
  gsize bytes_written;
  GladeWidget *gwidget;

  g_return_if_fail (GLADE_IS_PREVIEW (preview));
  g_return_if_fail (buffer && buffer[0]);

  gwidget = glade_preview_get_widget (preview);

  update = g_strdup_printf ("%s%s\n", update_token,
                            glade_widget_get_name (gwidget));

  channel = preview->priv->channel;
  g_io_channel_write_chars (channel, update, strlen (update), &size, &error);

  if (size != strlen (update) && error != NULL)
    {
      g_warning ("Error passing quit signal trough pipe: %s", error->message);
      g_error_free (error);
    }

  g_io_channel_flush (channel, &error);
  if (error != NULL)
    {
      g_warning ("Error flushing channel: %s", error->message);
      g_error_free (error);
    }

  /* We'll now send the interface: */
  g_io_channel_write_chars (channel, buffer, strlen (buffer), &bytes_written,
                            &error);

  if (bytes_written != strlen (buffer) && error != NULL)
    {
      g_warning ("Error passing UI trough pipe: %s", error->message);
      g_error_free (error);
    }

  g_io_channel_flush (channel, &error);
  if (error != NULL)
    {
      g_warning ("Error flushing UI trough pipe: %s", error->message);
      g_error_free (error);
    }

  g_free (update);
}

GladeWidget *
glade_preview_get_widget (GladePreview *preview)
{
  g_return_val_if_fail (GLADE_IS_PREVIEW (preview), NULL);
  return preview->priv->previewed_widget;
}

GPid
glade_preview_get_pid (GladePreview *preview)
{
  g_return_val_if_fail (GLADE_IS_PREVIEW (preview), 0);
  return preview->priv->pid;
}
