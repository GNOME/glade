/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 Ximian, Inc.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors:
 *   Chema Celorio <chema@celorio.com>
 */


#include <config.h>
#include "glade.h"

#include "glade-palette.h"
#include "glade-catalog.h"
#include "glade-project.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-project-window.h"

static void glade_palette_class_init (GladePaletteClass * klass);
static void glade_palette_init (GladePalette * glade_palette);

enum
{
	LAST_SIGNAL
};

static GtkWindowClass *parent_class = NULL;

guint
glade_palette_get_type (void)
{
	static guint palette_type = 0;

	if (!palette_type)
	{
		GtkTypeInfo palette_info =
		{
			"GladePalette",
			sizeof (GladePalette),
			sizeof (GladePaletteClass),
			(GtkClassInitFunc) glade_palette_class_init,
			(GtkObjectInitFunc) glade_palette_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		palette_type = gtk_type_unique (gtk_window_get_type (), &palette_info);
	}

	return palette_type;
}


static void
glade_palette_class_init (GladePaletteClass * klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) klass;

	parent_class = gtk_type_class (gtk_window_get_type ());
}

static GtkWidget *
my_gtk_pixmap (const gchar *file, GtkWidget *palette)
{
	GtkWidget *gtk_pixmap;
	GdkPixmap *gdk_pixmap = NULL;
	GdkBitmap *gdk_bitmap = NULL;

	gdk_pixmap = gdk_pixmap_colormap_create_from_xpm (NULL, gtk_widget_get_colormap (GTK_WIDGET (palette)),
							  &gdk_bitmap, NULL, file);
	gtk_pixmap = gtk_pixmap_new (gdk_pixmap, gdk_bitmap);

	gdk_pixmap_unref (gdk_pixmap);
	gdk_bitmap_unref (gdk_bitmap);
	
	return gtk_pixmap;
}

static GtkWidget *
glade_palette_selector_new (GladePalette *palette)
{
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *pixmap;

	hbox = gtk_hbox_new (FALSE, 0);
	button = gtk_toggle_button_new ();
	palette->label = gtk_label_new ("Selector");

	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), palette->label, FALSE, FALSE, 0);

	pixmap = my_gtk_pixmap (PIXMAPS_DIR "/selector.xpm", GTK_WIDGET (palette));
	gtk_container_add (GTK_CONTAINER (button), pixmap);
	
	gtk_widget_show_all (hbox);
	
	return hbox;	
}


static GtkWidget *
glade_palette_widget_create_icon_from_class (GladeWidgetClass *class)
{
	GtkWidget *pixmap;

	g_return_val_if_fail (class != NULL, NULL);

	pixmap = gtk_pixmap_new (class->pixmap, class->mask);

	return pixmap;
}

static void
glade_palette_button_clicked (GtkWidget *button, GladePalette *palette)
{
	GladeProjectWindow *gpw;
	GladeWidgetClass *class;
	GladeProject *project;
	static gboolean dont_recurse = FALSE;

	if (dont_recurse)
		return;

	if (!GTK_TOGGLE_BUTTON (button)->active)
		return;

	class = gtk_object_get_user_data (GTK_OBJECT (button));
	g_return_if_fail (class != NULL);

	gpw = glade_project_window_get ();
	
	if (GLADE_WIDGET_CLASS_TOPLEVEL (class)) {
		project = glade_project_get_active ();
		g_return_if_fail (project != NULL);
		glade_widget_new_from_class (project, class, NULL);
		dont_recurse = TRUE;
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (palette->dummy_button), TRUE);
		dont_recurse = FALSE;
		glade_project_window_set_add_class (gpw, NULL);
	} else {
		glade_project_window_set_add_class (gpw, class);
	}
}

static void
glade_palette_attach_pixmap (GladePalette *palette, GtkWidget *table, GList *list, gint i, gint cols)
{
	GladeWidgetClass *class;
	GtkWidget *gtk_pixmap;
	GtkWidget *button;
	gint x, y;
	
	class = g_list_nth_data (list, i);
	g_return_if_fail (class != NULL);
	gtk_pixmap = glade_palette_widget_create_icon_from_class (class);

	button = gtk_radio_button_new (palette->widgets_button_group);
	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
	palette->widgets_button_group = gtk_radio_button_group (GTK_RADIO_BUTTON (button));
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_container_add (GTK_CONTAINER (button), gtk_pixmap);

	gtk_object_set_user_data (GTK_OBJECT (button), class);
	gtk_signal_connect (GTK_OBJECT (button), "toggled",
			    (GtkSignalFunc) glade_palette_button_clicked, palette);
	
	x = (i%cols);
	y = (gint) (i/cols);
	gtk_table_attach (GTK_TABLE (table),
			  button,
			  x, x + 1,
			  y, y + 1,
			  0, 0,
			  0, 0);
}

static GtkWidget *
glade_palette_widget_table_create (GladePalette *palette)
{
	GladeProjectWindow *gpw;
	GtkWidget *table;
	GtkWidget *dummy;
	GList *list;
	gint num;
	gint rows;
	gint cols = 4;
	gint i;

	gpw = palette->project_window;
	list = gpw->catalog->widgets;
	num = g_list_length (list);
	rows = (gint)((num - 1)/ cols) + 1;

	table = gtk_table_new (rows, cols, TRUE);

	/* The dummy is a button that we don't gtk_show nor gtk_container_add.
	 * It is never displayed, we select it when we don't want anything
	 * selected, since the radio button groups needs a selected button
	 * all the time. Chema
	 */
	dummy = gtk_radio_button_new (palette->widgets_button_group);
	palette->widgets_button_group = gtk_radio_button_group (GTK_RADIO_BUTTON (dummy));
	palette->dummy_button = dummy;
	
	for (i = 0; i < num; i++) {
		glade_palette_attach_pixmap (palette, table, list, i, cols);
	}
	
	return table;
}

static gint
on_palette_button_toggled (GtkWidget *button, GladePalette *palette)
{
	return TRUE;
}

static GtkWidget *
glade_palette_button_group_create (GladePalette *palette)
{
	GtkWidget *button;
	GtkWidget *hbox;

	hbox = gtk_hbox_new (TRUE, 0);
	
	button = gtk_radio_button_new_with_label (palette->sections_button_group, "GTK + Basic");
	palette->sections_button_group = gtk_radio_button_group (GTK_RADIO_BUTTON (button));
	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (button), "toggled",
			    (GtkSignalFunc) on_palette_button_toggled, palette);

	return hbox;
}

static gint
glade_palette_delete_event (GladePalette *palette, gpointer not_used)
{
	gtk_widget_hide (GTK_WIDGET (palette));

	/* Return true so that the pallete is not destroyed */
	return TRUE;
}

static void
glade_palette_init (GladePalette * palette)
{
	GtkWidget *selector;
	GtkWidget *widget;

	gtk_window_set_title (GTK_WINDOW (palette), _("Palette"));
	gtk_window_set_policy (GTK_WINDOW (palette),
			       TRUE,
			       FALSE,
			       TRUE);

	palette->tooltips = gtk_tooltips_new ();
	palette->widgets_button_group = NULL;
	palette->sections_button_group = NULL;

	palette->vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (palette), palette->vbox);

	/* Selector */
	selector = glade_palette_selector_new (palette);
	gtk_box_pack_start (GTK_BOX (palette->vbox), selector, FALSE, TRUE, 3);
	
	/* Separator */
	widget = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (palette->vbox), widget, FALSE, TRUE, 3);
	
	/* Groups */
	widget = glade_palette_button_group_create (palette);
	gtk_box_pack_start (GTK_BOX (palette->vbox), widget, FALSE, TRUE, 3);
	
	/* Separator */
	widget = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (palette->vbox), widget, FALSE, TRUE, 3);

	/* Delete event, don't destroy it */
	gtk_signal_connect (GTK_OBJECT (palette), "delete_event",
			    GTK_SIGNAL_FUNC (glade_palette_delete_event), NULL);
	
}

static GladePalette *
glade_palette_new ()
{
	return GLADE_PALETTE (gtk_type_new (glade_palette_get_type ()));
}

void
glade_palette_show (GladeProjectWindow *gpw)
{
	GtkWidget *widget;
	
	g_return_if_fail (gpw != NULL);

	if (gpw->palette == NULL) {
		GladePalette *palette;

		palette = GLADE_PALETTE (glade_palette_new ());
		gpw->palette = palette;
		palette->project_window = gpw;

		/* Tables of widgets 
		 * we need to call this function after creating the palette
		 * cause we don't have a pointer to palette->project_window when
		 * we are in glade_palette_init ()
		 */
		widget = glade_palette_widget_table_create (gpw->palette);
		gtk_box_pack_start (GTK_BOX (gpw->palette->vbox), widget, FALSE, TRUE, 3);
	}

	gtk_widget_show_all (GTK_WIDGET (gpw->palette));
}

void
glade_palette_create (GladeProjectWindow *gpw)
{
	glade_palette_show (gpw);
}
