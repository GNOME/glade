/*
 * glade-palette-expander.h - A container which can hide its child
 *
 * Copyright (C) 2007 Vincent Geddes
 *
 * Author: Vincent Geddes <vincent.geddes@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef __GLADE_PALETTE_EXPANDER_H__
#define __GLADE_PALETTE_EXPANDER_H__

#include <gtk/gtkbin.h>

G_BEGIN_DECLS

#define GLADE_TYPE_PALETTE_EXPANDER              (glade_palette_expander_get_type ())
#define GLADE_PALETTE_EXPANDER(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PALETTE_EXPANDER, GladePaletteExpander))
#define GLADE_PALETTE_EXPANDER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PALETTE_EXPANDER, GladePaletteExpanderClass))
#define GLADE_IS_PALETTE_EXPANDER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PALETTE_EXPANDER))
#define GLADE_IS_PALETTE_EXPANDER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PALETTE_EXPANDER))
#define GLADE_PALETTE_EXPANDER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_PALETTE_EXPANDER, GladePaletteExpanderClass))

typedef struct _GladePaletteExpander         GladePaletteExpander;
typedef struct _GladePaletteExpanderPrivate  GladePaletteExpanderPrivate;
typedef struct _GladePaletteExpanderClass    GladePaletteExpanderClass;

struct _GladePaletteExpander
{
	/*< private >*/
	GtkBin parent_instance;

	GladePaletteExpanderPrivate *priv;
};

struct _GladePaletteExpanderClass
{
	GtkBinClass     parent_class;
	
	void         (* activate) (GladePaletteExpander *expander);
};


GType         glade_palette_expander_get_type         (void) G_GNUC_CONST;

GtkWidget    *glade_palette_expander_new              (const gchar          *label);

void          glade_palette_expander_set_expanded     (GladePaletteExpander *expander,
						       gboolean              expanded);
						       
gboolean      glade_palette_expander_get_expanded     (GladePaletteExpander *expander);

void          glade_palette_expander_set_spacing      (GladePaletteExpander *expander,
						       guint                 spacing);
						       
guint         glade_palette_expander_get_spacing      (GladePaletteExpander *expander);

void          glade_palette_expander_set_label        (GladePaletteExpander *expander,
						       const gchar          *label);
						        
const gchar  *glade_palette_expander_get_label        (GladePaletteExpander *expander);

void          glade_palette_expander_set_use_markup   (GladePaletteExpander *expander,
						       gboolean              use_markup);
						       
gboolean      glade_palette_expander_get_use_markup   (GladePaletteExpander *expander);


G_END_DECLS

#endif /* __GLADE_PALETTE_EXPANDER_H__ */

