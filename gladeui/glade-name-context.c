/*
 * glade-name-context.c
 *
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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include "glade-id-allocator.h"
#include "glade-name-context.h"

struct _GladeNameContext
{
  GHashTable *name_allocators;

  GHashTable *names;
};

/**
 * glade_name_context_new: (skip)
 *
 * Creates a new name context
 *
 * Returns: a new GladeNameContext
 */
GladeNameContext *
glade_name_context_new (void)
{
  GladeNameContext *context = g_slice_new0 (GladeNameContext);

  context->name_allocators = g_hash_table_new_full (g_str_hash,
                                                    g_str_equal,
                                                    g_free,
                                                    (GDestroyNotify)
                                                    glade_id_allocator_destroy);

  context->names = g_hash_table_new_full (g_str_hash,
                                          g_str_equal, g_free, NULL);

  return context;
}

void
glade_name_context_destroy (GladeNameContext *context)
{
  g_return_if_fail (context != NULL);

  g_hash_table_destroy (context->name_allocators);
  g_hash_table_destroy (context->names);
  g_slice_free (GladeNameContext, context);
}

gchar *
glade_name_context_new_name (GladeNameContext *context,
                             const gchar      *base_name)
{
  GladeIDAllocator *id_allocator;
  const gchar *number;
  gchar *name = NULL, *freeme = NULL;
  guint i = 1;

  g_return_val_if_fail (context != NULL, NULL);
  g_return_val_if_fail (base_name && base_name[0], NULL);

  number = base_name + strlen (base_name);
  while (number > base_name && g_ascii_isdigit (number[-1]))
    --number;

  if (*number)
    {
      freeme = g_strndup (base_name, number - base_name);
      base_name = freeme;
    }

  id_allocator = g_hash_table_lookup (context->name_allocators, base_name);

  if (id_allocator == NULL)
    {
      id_allocator = glade_id_allocator_new ();
      g_hash_table_insert (context->name_allocators,
                           g_strdup (base_name), id_allocator);
    }

  do
    {
      g_free (name);
      i = glade_id_allocator_allocate (id_allocator);
      name = g_strdup_printf ("%s%u", base_name, i);
    }
  while (glade_name_context_has_name (context, name));

  g_free (freeme);
  return name;
}

guint
glade_name_context_n_names (GladeNameContext *context)
{
  g_return_val_if_fail (context != NULL, FALSE);

  return g_hash_table_size (context->names);
}

gboolean
glade_name_context_has_name (GladeNameContext *context, const gchar *name)
{
  g_return_val_if_fail (context != NULL, FALSE);
  g_return_val_if_fail (name && name[0], FALSE);

  return (g_hash_table_lookup (context->names, name) != NULL);
}

gboolean
glade_name_context_add_name (GladeNameContext *context, const gchar *name)
{
  gboolean ret = FALSE;

  g_return_val_if_fail (context != NULL, FALSE);
  g_return_val_if_fail (name && name[0], FALSE);

  if (!glade_name_context_has_name (context, name))
    {
      g_hash_table_insert (context->names, g_strdup (name),
                           GINT_TO_POINTER (TRUE));
      ret = TRUE;
    }

  return ret;
}

void
glade_name_context_release_name (GladeNameContext *context, const gchar *name)
{

  const gchar *first_number = name;
  gchar *end_number, *base_name;
  GladeIDAllocator *id_allocator;
  gunichar ch;
  gint id;

  g_return_if_fail (context != NULL);
  g_return_if_fail (name && name[0]);

  /* Remove from name hash first... */
  g_hash_table_remove (context->names, name);

  do
    {
      ch = g_utf8_get_char (first_number);

      if (ch == 0 || g_unichar_isdigit (ch))
        break;

      first_number = g_utf8_next_char (first_number);
    }
  while (TRUE);

  /* if there is a number - then we have to unallocate it... */
  if (ch == 0)
    return;

  base_name = g_strdup (name);
  *(base_name + (first_number - name)) = 0;

  if ((id_allocator =
       g_hash_table_lookup (context->name_allocators, base_name)) != NULL)
    {
      id = (int) strtol (first_number, &end_number, 10);
      if (*end_number == 0)
        glade_id_allocator_release (id_allocator, id);
    }

  g_free (base_name);
}
