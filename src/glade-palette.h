/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_PALETTE_H__
#define __GLADE_PALETTE_H__

#include "glade-types.h"

G_BEGIN_DECLS


#define GLADE_TYPE_PALETTE              (glade_palette_get_type ())
#define GLADE_PALETTE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PALETTE, GladePalette))
#define GLADE_PALETTE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PALETTE, GladePaletteClass))
#define GLADE_IS_PALETTE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PALETTE))
#define GLADE_IS_PALETTE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PALETTE))
#define GLADE_PALETTE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_PALETTE, GladePaletteClass))

typedef struct _GladePaletteClass	GladePaletteClass;


/* The GladePalette is used so that the user can choose a widget to be
 * created. It is Composed by a section where you can select the
 * group of widgets to choose from an a table with icons for each
 * widget available.
 */
struct _GladePalette
{
	GtkVBox vbox; /* The parent is a vbox */

	GladeWidgetClass *current; /* The GladeWidgetClass corrisponding
				    * to the selected button. NULL if the
				    * selector button is pressed.
				    */

	GtkWidget *selector; /* The selevtor is a button that is
			      * clicked to cancel the add widget action.
			      * This sets our cursor back to the "select
			      * widget" mode. This button is part of the
			      * widgets_button_group, so that when no widget
			      * is selected, this button is pressed.
			      */

	GtkWidget *label;  /* A label which contains the name of the class
			    * currently selected or "Selector" if no widget
			    * class is selected
			    */

	GtkWidget *groups_vbox; /* The vbox that contains the titles of the sections */

	GtkWidget *notebook; /* Where we store the different catalogs */

	gint nb_sections; /* The number of sections in this palette */
	GSList *sections_button_group; /* Each section of the palette has
					* a button. This is the button_group_list
					*/

	GSList *widgets_button_group; /* Each widget in a section has a button
				       * in the palette. This is the button
				       * group, since only one may be pressed.
				       */
};

struct _GladePaletteClass
{
	GtkVBoxClass parent_class;

	void (*toggled) (GladePalette *palette);
};

GType glade_palette_get_type (void);

GladePalette *glade_palette_new (GList *catalogs);

void glade_palette_append_catalog (GladePalette *palette, GladeCatalog *catalog);

void glade_palette_unselect_widget (GladePalette *palette);


G_END_DECLS

#endif /* __GLADE_PALETTE_H__ */
