/*
 * Copyright (C) 2014, 2020 Juan Pablo Ugarte.
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

struct _GladeHTTPPrivate
{
  gchar *host;
  gint port;
  gboolean tls;

  GladeHTTPStatus status;
  GSocketConnection *connection;
  GDataInputStream *data_stream;
  GCancellable *cancellable;

  GString *data;
  GString *response;

  gint http_major;
  gint http_minor;
  gint code;

  gchar *transfer_encoding_value;
  gchar *content_length_value;

  GHashTable *cookies;

  gint content_length;
  gboolean chunked;
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
glade_http_close (GladeHTTP *http)
{
  GladeHTTPPrivate *priv = http->priv;

  g_clear_object (&priv->data_stream);
  g_clear_object (&priv->connection);
  g_clear_object (&priv->cancellable);
}

static void
glade_http_clear (GladeHTTP *http)
{
  GladeHTTPPrivate *priv = http->priv;

  glade_http_close (http);
  
  g_string_assign (priv->data, "");
  g_string_assign (priv->response, "");

  priv->http_major = priv->http_minor = priv->code = 0;

  g_clear_pointer (&priv->transfer_encoding_value, g_free);
  g_clear_pointer (&priv->content_length_value, g_free);

  priv->chunked = FALSE;
  priv->content_length = 0;

  g_hash_table_remove_all (priv->cookies);

  priv->status = GLADE_HTTP_READY;
}

static void
glade_http_emit_request_done (GladeHTTP *http)
{
  GladeHTTPPrivate *priv = http->priv;
  
  g_file_set_contents ("response.html", priv->response->str, -1, NULL);

  /* emit request-done */
  g_signal_emit (http, http_signals[REQUEST_DONE], 0,
                 priv->code, priv->response->str);
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

static gboolean
parse_cookie (const gchar *str, gchar **key, gchar **value)
{
  gchar **tokens, *colon;

  if (str == NULL)
    return FALSE;

  tokens = g_strsplit(str, "=", 2);

  if ((colon = g_strstr_len (tokens[1], -1, ";")))
    *colon = '\0';

  *key = g_strstrip (tokens[0]);
  *value = g_strstrip (tokens[1]);

  g_free (tokens);

  return TRUE;
}

static void
on_read_line_ready (GObject *source, GAsyncResult *res, gpointer data)
{
  GladeHTTPPrivate *priv = GLADE_HTTP (data)->priv;
  g_autoptr (GError) error = NULL;
  g_autofree gchar *line;
  gsize length;

  glade_http_emit_status (data, GLADE_HTTP_RECEIVING, NULL);

  line = g_data_input_stream_read_line_finish (priv->data_stream, res, &length, &error);

  if (error)
    {
      glade_http_emit_status (data, GLADE_HTTP_ERROR, error);
      g_error_free (error);
    }

  if (!line)
    return;

  if (priv->http_major == 0)
    {
      /* Reading HTTP version */
      if (sscanf (line, "HTTP/%d.%d %d",
                  &priv->http_major,
                  &priv->http_minor,
                  &priv->code) != 3)
        {
          error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                                       "Could not parse HTTP header");
          glade_http_emit_status (data, GLADE_HTTP_ERROR, error);
          return;
        }
    }
  else if (priv->response->len < priv->content_length)
    {
      /* We are reading a chunk or the whole response */
      g_string_append_len (priv->response, line, length);
    }
  else
    {
      /* Reading HTTP Headers */

      if (priv->chunked)
        {
          gint chunk;

          if (sscanf (line, "%x", &chunk) != 1)
            {
              error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                                           "Could not parse chunk size");
              glade_http_emit_status (data, GLADE_HTTP_ERROR, error);
              return;
            }

          priv->content_length += chunk;
        }
      /* Check if this is the last line of the headers */
      else if (length == 0)
        {
          if (g_strcmp0 (priv->transfer_encoding_value, "chunked") == 0)
            priv->chunked = TRUE;
          else
            priv->content_length = atoi (priv->content_length_value);
        }
      else
        {
          g_auto(GStrv) tokens = g_strsplit (line, ":", 2);

          if (tokens)
            {
              gchar *key = g_strstrip (tokens[0]);
              gchar *val = g_strstrip (tokens[1]);

              if (g_strcmp0 ("Transfer-Encoding", key) == 0)
                {
                  priv->transfer_encoding_value = g_strdup (val);
                }
              else if (g_strcmp0 ("Content-Length", key) == 0)
                {
                  priv->content_length_value = g_strdup (val);
                }
              else if (g_strcmp0 ("Set-Cookie", key) == 0)
                {
                  gchar *cookie = NULL, *value = NULL;

                  /* Take cookie/value ownership */
                  if (parse_cookie (val, &cookie, &value))
                    g_hash_table_insert (priv->cookies, cookie, value);
                }
            }
        }
    }

  if (priv->content_length && priv->response->len >= priv->content_length)
    {
      glade_http_close (data);

      /* emit request-done */
      glade_http_emit_request_done (data);
      glade_http_emit_status (data, GLADE_HTTP_READY, NULL);
    }
  else
    g_data_input_stream_read_line_async (priv->data_stream,
                                         G_PRIORITY_DEFAULT,
                                         priv->cancellable,
                                         on_read_line_ready,
                                         data);
}

static void
on_write_ready (GObject *source, GAsyncResult *res, gpointer data)
{
  GladeHTTPPrivate *priv = GLADE_HTTP (data)->priv;
  GInputStream *input_stream;
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

  /* Get connection stream */
  input_stream = g_io_stream_get_input_stream (G_IO_STREAM (priv->connection));

  /* Create stream to read structured data */
  priv->data_stream = g_data_input_stream_new (input_stream);
  g_data_input_stream_set_newline_type (priv->data_stream, G_DATA_STREAM_NEWLINE_TYPE_CR_LF);

  /* Start reading headers line by line */
  g_data_input_stream_read_line_async (priv->data_stream,
                                       G_PRIORITY_DEFAULT,
                                       priv->cancellable,
                                       on_read_line_ready,
                                       data);
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

  priv->cookies = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  priv->data = g_string_new ("");
  priv->response = g_string_new ("");
}

static void
glade_http_finalize (GObject *object)
{
  GladeHTTPPrivate *priv = GLADE_HTTP (object)->priv;

  glade_http_clear (GLADE_HTTP (object));
  
  g_string_free (priv->data, TRUE);
  g_string_free (priv->response, TRUE);
  g_hash_table_destroy (priv->cookies);

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
                  G_OBJECT_CLASS_TYPE (klass), G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GladeHTTPClass, request_done),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  G_TYPE_INT,  G_TYPE_STRING);

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

gchar *
glade_http_get_cookie (GladeHTTP *http, const gchar *key)
{
  g_return_val_if_fail (GLADE_IS_HTTP (http), NULL);
  return g_hash_table_lookup (http->priv->cookies, key);
}

static void
collect_cookies (gpointer key, gpointer value, gpointer user_data)
{
  GString *cookies = user_data;

  if (cookies->len)
    g_string_append_printf (cookies, "; %s=%s", (gchar *)key, (gchar *)value);
  else
    g_string_append_printf (cookies, "%s=%s", (gchar *)key, (gchar *)value);
}

gchar *
glade_http_get_cookies (GladeHTTP *http)
{
  GString *cookies;
  gchar *retval;

  g_return_val_if_fail (GLADE_IS_HTTP (http), NULL);

  cookies = g_string_new ("");
  g_hash_table_foreach (http->priv->cookies, collect_cookies, cookies);

  retval = cookies->str;
  g_string_free (cookies, FALSE);

  return retval;
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
