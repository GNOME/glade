/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-palette-expander.h
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
 *
 * Authors:
 *	Mark McLoughlin <mark@skynet.ie>
 *
 * Modified for use in glade-3.
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 *      
 */

#ifndef __GLADE_PALETTE_PALETTE_EXPANDER_H__
#define __GLADE_PALETTE_PALETTE_EXPANDER_H__

#include <gtk/gtkbin.h>

G_BEGIN_DECLS

#define GLADE_TYPE_PALETTE_EXPANDER            (glade_palette_expander_get_type ())
#define GLADE_PALETTE_EXPANDER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PALETTE_EXPANDER, GladePaletteExpander))
#define GLADE_PALETTE_EXPANDER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GLADE_TYPE_PALETTE_EXPANDER, GladePaletteExpanderClass))
#define GLADE_IS_PALETTE_EXPANDER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PALETTE_EXPANDER))
#define GLADE_IS_PALETTE_EXPANDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GLADE_TYPE_PALETTE_EXPANDER))
#define GLADE_PALETTE_EXPANDER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GLADE_TYPE_PALETTE_EXPANDER, GladePaletteExpanderClass))

typedef struct _GladePaletteExpander        GladePaletteExpander;
typedef struct _GladePaletteExpanderClass   GladePaletteExpanderClass;
typedef struct _GladePaletteExpanderPrivate GladePaletteExpanderPrivate;

struct _GladePaletteExpander
{
  GtkBin              bin;

  GladePaletteExpanderPrivate *priv;
};

struct _GladePaletteExpanderClass
{
  GtkBinClass    parent_class;

  /* Key binding signal; to get notification on the expansion
   * state connect to notify:expanded.
   */
  void        (* activate) (GladePaletteExpander *expander);
};

GType                 glade_palette_expander_get_type          (void) G_GNUC_CONST;

GtkWidget            *glade_palette_expander_new               (const gchar *label);
GtkWidget            *glade_palette_expander_new_with_mnemonic (const gchar *label);

void                  glade_palette_expander_set_expanded      (GladePaletteExpander *expander,
						                gboolean     expanded);
gboolean              glade_palette_expander_get_expanded      (GladePaletteExpander *expander);

void                  glade_palette_expander_set_spacing       (GladePaletteExpander *expander,
						                gint         spacing);
gint                  glade_palette_expander_get_spacing       (GladePaletteExpander *expander);

void                  glade_palette_expander_set_label         (GladePaletteExpander *expander,
						                const gchar *label);
G_CONST_RETURN gchar *glade_palette_expander_get_label         (GladePaletteExpander *expander);

void                  glade_palette_expander_set_use_underline (GladePaletteExpander *expander,
						                gboolean     use_underline);
gboolean              glade_palette_expander_get_use_underline (GladePaletteExpander *expander);

void                  glade_palette_expander_set_use_markup    (GladePaletteExpander *expander,
						                gboolean    use_markup);
gboolean              glade_palette_expander_get_use_markup    (GladePaletteExpander *expander);

void                  glade_palette_expander_set_label_widget  (GladePaletteExpander *expander,
						                GtkWidget   *label_widget);
GtkWidget            *glade_palette_expander_get_label_widget  (GladePaletteExpander *expander);

G_END_DECLS

#endif /* __GLADE_PALETTE_PALETTE_EXPANDER_H__ */
