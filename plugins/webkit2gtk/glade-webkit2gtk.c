/*
 * Copyright (C) 2011 Juan Pablo Ugarte
 *               2016 Endless Mobile Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 *   Juan Pablo Ugarte <ugarte@endlessm.com>
 */

#include "config.h"

#include <webkit2/webkit2.h>
#include <gladeui/glade.h>

void
glade_webkit_web_view_set_property (GladeWidgetAdaptor *adaptor,
                                    GObject            *object,
                                    const gchar        *id,
                                    const GValue       *value)
{
  if (g_str_equal(id, "glade-url"))
    {
      const gchar *url = g_value_get_string (value);
      gchar *scheme = g_uri_parse_scheme (url);

      if (scheme)
        {
          webkit_web_view_load_uri (WEBKIT_WEB_VIEW (object), url);
          g_free (scheme);
          return;
        }

      gchar *full_url = g_strconcat ("http://", url, NULL);
      webkit_web_view_load_uri (WEBKIT_WEB_VIEW (object), full_url);
      g_free (full_url);
    }
  else
    GWA_GET_CLASS(G_TYPE_OBJECT)->set_property (adaptor, object, id, value);
}
