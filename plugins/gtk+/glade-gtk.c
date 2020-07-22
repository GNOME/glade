/*
 * glade-gtk.c - Utilities
 *
 * Copyright (C) 2020 Juan Pablo Ugarte
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
 *      Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#include <config.h>
#include <glib/gi18n-lib.h>
#include <gladeui/glade.h>

#include "glade-gtk.h"

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
GParameter *
glade_gtk_get_params_without_use_header_bar (guint *n_parameters, GParameter *parameters)
{
  GParameter *new_params = g_new0 (GParameter, *n_parameters + 1);
  gboolean use_header_bar_set = FALSE;
  gint i;

  /* Here we need to force use-header-bar to FALSE in the runtime because
   * GtkAboutDialog set it to TRUE in its constructor which then triggers a
   * warning when glade tries to add placeholders in the action area
   */
  for (i = 0; i < *n_parameters; i++)
    {
      new_params[i] = parameters[i];

      if (!g_strcmp0 (new_params[i].name, "use-header-bar"))
        {
          /* force the value to 0 */
          g_value_set_int (&new_params[i].value, 0);
          use_header_bar_set = TRUE;
        }
    }

  if (!use_header_bar_set)
    {
      GParameter *use_header = &new_params[*n_parameters];

      *n_parameters = *n_parameters + 1;

      /* append value if it was not part of the parameters */
      use_header->name = "use-header-bar";
      g_value_init (&use_header->value, G_TYPE_INT);
      g_value_set_int (&use_header->value, 0);
    }

  return new_params;
}
G_GNUC_END_IGNORE_DEPRECATIONS

