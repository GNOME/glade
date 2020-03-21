/*
 * Copyright (C) 2014 Juan Pablo Ugarte.
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

#include <glade-http.h>
#include <string.h>
#include <stdio.h>

#define HTTP_BUFFER_SIZE 16

struct _GladeHTTPPrivate
{
  gchar *host;
  gint port;
  gboolean tls;

  GladeHTTPStatus status;
  GSocketConnection *connection;
  GCancellable *cancellable;
  
  GString *data;
  GString *response;

  gchar response_buffer[HTTP_BUFFER_SIZE];
};

enum
{
  PROP_0,

  PROP_HOST,
  PROP_PORT,
  PROP_TLS,
  N_PROPERTIES
};

enum
{
  REQUEST_DONE,
  STATUS,

  LAST_SIGNAL
};

static GParamSpec *properties[N_PROPERTIES];
static guint http_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GladeHTTP, glade_http, G_TYPE_OBJECT);

static void
glade_http_emit_request_done (GladeHTTP *http, gchar *response)
{
  gint http_major, http_minor, code, i;
  gchar **headers, **values, *blank, *data, tmp;

  if ((blank = g_strstr_len (response, -1, "\r\n\r\n")))
    data = blank + 4;
  else if ((blank = g_strstr_len (response, -1, "\n\n")))
    data = blank + 2;
  else
    return;

  tmp = *blank;
  *blank = '\0';
  headers = g_strsplit (response, "\n", 0);
  *blank = tmp;

  values = g_new0 (gchar *, g_strv_length (headers));
  
  for (i = 0; headers[i]; i++)
    {
      g_strchug (headers[i]);

      if (i)
        {
          gchar *colon = g_strstr_len (headers[i], -1, ":");

          if (colon)
            {
              *colon++ = '\0';
              values[i-1] = g_strstrip (colon);
            }
          else
            values[i-1] = "";
        }
      else
        {
          if (sscanf (response, "HTTP/%d.%d %d", &http_major, &http_minor, &code) != 3)
            http_major = http_minor = code = 0;
        }
    }
  
  /* emit request-done */
  g_signal_emit (http, http_signals[REQUEST_DONE], 0, code,
                 (headers[0]) ? &headers[1] : NULL, values, 
                 data);

  g_strfreev (headers);
  g_free (values);
}

static void
glade_http_emit_status (GladeHTTP *http, GladeHTTPStatus status, GError *error)
{
  GladeHTTPPrivate *priv = http->priv;

  if (priv->status == status)
    return;

  priv->status = status;

  g_signal_emit (http, http_signals[STATUS], 0, status, error);
}

static void
on_read_ready (GObject *source, GAsyncResult *res, gpointer data)
{
  GladeHTTPPrivate *priv = GLADE_HTTP (data)->priv;
  GError *error = NULL;
  gssize bytes_read;

  glade_http_emit_status (data, GLADE_HTTP_RECEIVING, NULL);

  bytes_read = g_input_stream_read_finish (G_INPUT_STREAM (source), res, &error);
  
  if (bytes_read > 0)
    {
      g_string_append_len (priv->response, priv->response_buffer, bytes_read);

      /* NOTE: We do not need to parse content-length because we do not support
       * multiples HTTP requests in the same connection.
       */
      if (priv->cancellable)
        g_cancellable_reset (priv->cancellable);
      
      g_input_stream_read_async (g_io_stream_get_input_stream (G_IO_STREAM (priv->connection)),
                                 priv->response_buffer, HTTP_BUFFER_SIZE,
                                 G_PRIORITY_DEFAULT, priv->cancellable,
                                 on_read_ready, data);
      return;
    }
  else if (bytes_read < 0)
    {
      glade_http_emit_status (data, GLADE_HTTP_ERROR, error);
      g_error_free (error);
      return;
    }

  /* emit request-done */
  glade_http_emit_request_done (data, priv->response->str);
  glade_http_emit_status (data, GLADE_HTTP_READY, NULL);
}

static void
on_write_ready (GObject *source, GAsyncResult *res, gpointer data)
{
  GladeHTTPPrivate *priv = GLADE_HTTP (data)->priv;
  GError *error = NULL;
  gsize count;

  count = g_output_stream_write_finish (G_OUTPUT_STREAM (source), res, &error);

  if (error == NULL && priv->data->len != count)
    error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED, 
                         "Error sending data to %s", 
                         priv->host);

  if (error)
    {
      glade_http_emit_status (data, GLADE_HTTP_ERROR, error);
      g_error_free (error);
      return;
    }

  glade_http_emit_status (data, GLADE_HTTP_WAITING, NULL);
  g_input_stream_read_async (g_io_stream_get_input_stream (G_IO_STREAM (priv->connection)),
                             priv->response_buffer, HTTP_BUFFER_SIZE,
                             G_PRIORITY_DEFAULT, priv->cancellable, on_read_ready, data);
}

static void
on_connect_ready (GObject *source, GAsyncResult *res, gpointer data)
{
  GladeHTTPPrivate *priv = GLADE_HTTP (data)->priv;
  GError *error = NULL;

  priv->connection = g_socket_client_connect_to_host_finish (G_SOCKET_CLIENT (source), res, &error);

  if (error)
    {
      glade_http_emit_status (data, GLADE_HTTP_ERROR, error);
      g_error_free (error);
      return;
    }

  if (priv->connection)
    {
      glade_http_emit_status (data, GLADE_HTTP_SENDING, NULL);
      g_output_stream_write_async (g_io_stream_get_output_stream (G_IO_STREAM (priv->connection)),
                                   priv->data->str, priv->data->len, G_PRIORITY_DEFAULT,
                                   priv->cancellable, on_write_ready, data);
    }
}

static void
glade_http_init (GladeHTTP *http)
{
  GladeHTTPPrivate *priv;
  
  priv = http->priv = glade_http_get_instance_private (http);

  priv->data = g_string_new ("");
  priv->response = g_string_new ("");
}

static void
glade_http_clear (GladeHTTP *http)
{
  GladeHTTPPrivate *priv = http->priv;

  if (priv->cancellable)
    g_cancellable_cancel (priv->cancellable);

  g_clear_object (&priv->connection);
  g_clear_object (&priv->cancellable);

  g_string_assign (priv->data, "");
  g_string_assign (priv->response, "");

  priv->status = GLADE_HTTP_READY;
}

static void
glade_http_finalize (GObject *object)
{
  GladeHTTPPrivate *priv = GLADE_HTTP (object)->priv;

  glade_http_clear (GLADE_HTTP (object));
  
  g_string_free (priv->data, TRUE);
  g_string_free (priv->response, TRUE);
  
  G_OBJECT_CLASS (glade_http_parent_class)->finalize (object);
}

static void
glade_http_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  GladeHTTPPrivate *priv;
  
  g_return_if_fail (GLADE_IS_HTTP (object));
  priv = GLADE_HTTP (object)->priv;
  
  switch (prop_id)
    {
      case PROP_HOST:
        g_free (priv->host);
        priv->host = g_strdup (g_value_get_string (value));
        break;
      case PROP_PORT:
        priv->port = g_value_get_int (value);
        break;
      case PROP_TLS:
        priv->tls = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_http_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  GladeHTTPPrivate *priv;
  
  g_return_if_fail (GLADE_IS_HTTP (object));
  priv = GLADE_HTTP (object)->priv;

  switch (prop_id)
    {
      case PROP_HOST:
        g_value_set_string (value, priv->host);
        break;
      case PROP_PORT:
        g_value_set_int (value, priv->port);
        break;
      case PROP_TLS:
        g_value_set_boolean (value, priv->tls);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_http_class_init (GladeHTTPClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = glade_http_finalize;
  object_class->set_property = glade_http_set_property;
  object_class->get_property = glade_http_get_property;

  properties[PROP_HOST] =
    g_param_spec_string ("host", "Host",
                         "Host to connect to",
                         NULL, G_PARAM_READWRITE);

  properties[PROP_PORT] =
    g_param_spec_int ("port", "TCP Port",
                      "TCP port to connect to",
                      0, 65536, 80, G_PARAM_READWRITE);

  properties[PROP_TLS] =
    g_param_spec_boolean ("tls", "TLS",
                          "Whether to use tls encryption or not",
                          FALSE, G_PARAM_READWRITE);

  /* Install all properties */
  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
  
  http_signals[REQUEST_DONE] =
    g_signal_new ("request-done",
                  G_OBJECT_CLASS_TYPE (klass), 0,
                  G_STRUCT_OFFSET (GladeHTTPClass, request_done),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 4,
                  G_TYPE_INT, G_TYPE_STRV, G_TYPE_STRV, G_TYPE_STRING);

  http_signals[STATUS] =
    g_signal_new ("status",
                  G_OBJECT_CLASS_TYPE (klass), 0,
                  G_STRUCT_OFFSET (GladeHTTPClass, status),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  G_TYPE_INT, G_TYPE_ERROR);
}

GladeHTTP *
glade_http_new (const gchar *host, gint port, gboolean tls)
{
  return GLADE_HTTP (g_object_new (GLADE_TYPE_HTTP,
                                   "host", host,
                                   "port", port,
                                   "tls", tls,
                                   NULL));
}

const gchar *
glade_http_get_host (GladeHTTP *http)
{
  g_return_val_if_fail (GLADE_IS_HTTP (http), NULL);
  return http->priv->host;
}

gint
glade_http_get_port (GladeHTTP *http)
{
  g_return_val_if_fail (GLADE_IS_HTTP (http), 0);
  return http->priv->port;
}

void
glade_http_request_send_async (GladeHTTP    *http,
                               GCancellable *cancellable,
                               const gchar  *format,
                               ...)
{
  GladeHTTPPrivate *priv;
  GSocketClient *client;
  va_list ap;

  g_return_if_fail (GLADE_IS_HTTP (http));

  priv = http->priv;
  client = g_socket_client_new ();
  glade_http_clear (http);

  va_start (ap, format);
  g_string_vprintf (priv->data, format, ap);
  va_end (ap);

  priv->cancellable = cancellable ? g_object_ref (cancellable) : NULL;

  if (priv->tls)
    {
      g_socket_client_set_tls (client, TRUE);
      g_socket_client_set_tls_validation_flags (client, 0);
    }

  glade_http_emit_status (http, GLADE_HTTP_CONNECTING, NULL);
  
  g_socket_client_connect_to_host_async (client,
                                         priv->host,
                                         priv->port,
                                         cancellable,
                                         on_connect_ready,
                                         http);
  g_object_unref (client);
}
