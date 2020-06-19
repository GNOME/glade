/*
 * Copyright (C) 2004 Owen Taylor
 *
 * Authors:
 *   Owen Taylor  <otaylor@redhat.com>
 *
 * Modified by the Glade developers
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
 */

#include "config.h"
#include "glade-id-allocator.h"

#include <glib.h>
#include <string.h>

#define INITIAL_WORDS 4

struct _GladeIDAllocator
{
  guint n_words;
  guint32 *data;
};

/**
 * glade_id_allocator_new: (skip)
 *
 * Returns: a new #GladeIDAllocator
 */
GladeIDAllocator *
glade_id_allocator_new (void)
{
  GladeIDAllocator *allocator = g_slice_new (GladeIDAllocator);

  allocator->n_words = INITIAL_WORDS;
  allocator->data    = g_new (guint32, INITIAL_WORDS);

  memset (allocator->data, 0xff, INITIAL_WORDS * sizeof (guint32));

  return allocator;
}

/**
 * glade_id_allocator_destroy:
 * @allocator: a #GladeIDAllocator
 *
 * Frees @allocator and its associated memory
 */
void
glade_id_allocator_destroy (GladeIDAllocator *allocator)
{
  g_return_if_fail (allocator != NULL);

  g_free (allocator->data);
  g_slice_free (GladeIDAllocator, allocator);
}

static inline gint
first_set_bit (guint32 word)
{
  static const char table[16] = {
    4, 0, 1, 0,
    2, 0, 1, 0,
    3, 0, 1, 0,
    2, 0, 1, 0
  };

  gint result = 0;

  if ((word & 0xffff) == 0)
    {
      word >>= 16;
      result += 16;
    }

  if ((word & 0xff) == 0)
    {
      word >>= 8;
      result += 8;
    }

  if ((word & 0xf) == 0)
    {
      word >>= 4;
      result += 4;
    }

  return result + table[word & 0xf];
}

/**
 * glade_id_allocator_allocate:
 * @allocator: a #GladeIDAllocator
 *
 * TODO: write me
 * Returns: the id
 */
guint
glade_id_allocator_allocate (GladeIDAllocator *allocator)
{
  guint i;

  g_return_val_if_fail (allocator != NULL, 0);

  for (i = 0; i < allocator->n_words; i++)
    {
      if (allocator->data[i] != 0)
        {
          gint free_bit = first_set_bit (allocator->data[i]);
          allocator->data[i] &= ~(1 << free_bit);

          return 32 * i + free_bit + 1;
        }
    }

  {
    guint n_words = allocator->n_words;

    allocator->data = g_renew (guint32, allocator->data, n_words * 2);
    memset (&allocator->data[n_words], 0xff, n_words * sizeof (guint32));
    allocator->n_words = n_words * 2;

    allocator->data[n_words] = 0xffffffff - 1;

    return 32 * n_words + 1;
  }
}

/**
 * glade_id_allocator_release:
 * @allocator: a #GladeIDAllocator
 * @id: the id given by glade_id_allocator_allocate()
 *
 * TODO: write me
 */
void
glade_id_allocator_release (GladeIDAllocator *allocator, guint id)
{
  guint word_idx;

  g_return_if_fail (allocator != NULL);

  /* Allocated ids start with 1 */
  if (id > 0)
    {
      id = id - 1;
      word_idx = id >> 5;

      /* Tollerate releasing ids that were never allocated with the allocator 
       * or are out of range... when we load Glade files with huge numbers it happens
       * that loaded unallocated ids are out of range
       */
      if (word_idx < allocator->n_words)
        allocator->data[word_idx] |= 1 << (id & 31);
    }
}

#ifdef GLADE_ID_ALLOCATOR_TEST
int
main (int argc, char **argv)
{
  GladeIDAllocator *allocator = glade_id_allocator_new ();
  guint i;
  guint iter;

  for (i = 0; i < 1000; i++)
    {
      guint id = glade_id_allocator_allocate (allocator);
      g_assert (id == i);
    }

  for (i = 0; i < 1000; i++)
    glade_id_allocator_release (allocator, i);

  for (iter = 0; iter < 10000; iter++)
    {
      for (i = 0; i < 1000; i++)
        glade_id_allocator_allocate (allocator);

      for (i = 0; i < 1000; i++)
        glade_id_allocator_release (allocator, i);
    }

  glade_id_allocator_destroy (allocator);

  return 0;
}
#endif
