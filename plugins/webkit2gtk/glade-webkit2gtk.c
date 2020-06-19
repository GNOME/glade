/*
 * Copyright (C) 2011-2020 Juan Pablo Ugarte
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
glade_webkit2gtk_init (const gchar *name)
{
  /* Enable Sandbox */
  webkit_web_context_set_sandbox_enabled (webkit_web_context_get_default (), TRUE);
}

void
glade_webkit_web_view_set_property (GladeWidgetAdaptor *adaptor,
                                    GObject            *object,
                                    const gchar        *id,
                                    const GValue       *value)
{
  if (g_str_equal(id, "glade-url"))
    {
      const gchar *url = g_value_get_string (value);
      g_autofree gchar *scheme = NULL, *full_url = NULL;

      if (!url)
        return;

      if ((scheme = g_uri_parse_scheme (url)))
        {
          webkit_web_view_load_uri (WEBKIT_WEB_VIEW (object), url);
          return;
        }

      full_url = g_strconcat ("http://", url, NULL);
      webkit_web_view_load_uri (WEBKIT_WEB_VIEW (object), full_url);
    }
  else
    GLADE_WIDGET_ADAPTOR_GET_ADAPTOR_CLASS(GTK_TYPE_WIDGET)->set_property (adaptor, object, id, value);
}
