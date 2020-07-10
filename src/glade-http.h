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

#ifndef _GLADE_HTTP_H_
#define _GLADE_HTTP_H_

#include <gio/gio.h>

G_BEGIN_DECLS

#define GLADE_TYPE_HTTP             (glade_http_get_type ())
#define GLADE_HTTP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_HTTP, GladeHTTP))
#define GLADE_HTTP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_HTTP, GladeHTTPClass))
#define GLADE_IS_HTTP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_HTTP))
#define GLADE_IS_HTTP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_HTTP))
#define GLADE_HTTP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_HTTP, GladeHTTPClass))

typedef struct _GladeHTTPClass GladeHTTPClass;
typedef struct _GladeHTTP GladeHTTP;
typedef struct _GladeHTTPPrivate GladeHTTPPrivate;

typedef enum
{
  GLADE_HTTP_READY,
  GLADE_HTTP_CONNECTING,
  GLADE_HTTP_SENDING,
  GLADE_HTTP_WAITING,
  GLADE_HTTP_RECEIVING,
  GLADE_HTTP_ERROR
} GladeHTTPStatus;

struct _GladeHTTPClass
{
  GObjectClass parent_class;

  /* Signals */
  void (*request_done) (GladeHTTP   *self,
                        gint         code,
                        const gchar *response);
  
  void (*status)       (GladeHTTP       *self,
                        GladeHTTPStatus  status,
                        GError          *error);
};

struct _GladeHTTP
{
  GObject parent_instance;

  GladeHTTPPrivate *priv;
};

GType glade_http_get_type (void) G_GNUC_CONST;

GladeHTTP *glade_http_new                (const gchar *host,
                                          gint         port,
                                          gboolean     tls);

const gchar *glade_http_get_host         (GladeHTTP    *http);
gint         glade_http_get_port         (GladeHTTP    *http);

gchar       *glade_http_get_cookie       (GladeHTTP    *http,
                                          const gchar  *key);

gchar       *glade_http_get_cookies      (GladeHTTP    *http);

void         glade_http_request_send_async (GladeHTTP    *http,
                                            GCancellable *cancellable,
                                            const gchar  *format,
                                            ...) G_GNUC_PRINTF (3, 4);
G_END_DECLS

#endif /* _GLADE_HTTP_H_ */

