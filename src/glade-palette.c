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
 *   Joaquin Cuenca Abela <e98cuenc@yahoo.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef PIXMAPS_DIR
#define PIXMAPS_DIR "C:/Program Files/glade/pixmap"
#endif

#include "glade.h"

#include "glade-palette.h"
#include "glade-catalog.h"
#include "glade-project.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-project-window.h"

static void glade_palette_class_init (GladePaletteClass *class);
static void glade_palette_init (GladePalette *glade_palette);

enum
{
	LAST_SIGNAL
};

static GtkWindowClass *parent_class = NULL;

GType
glade_palette_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GladePaletteClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) glade_palette_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (GladePalette),
			0,
			(GInstanceInitFunc) glade_palette_init
		};

		type = g_type_register_static (GTK_TYPE_VBOX, "GladePalette", &info, 0);
	}

	return type;
}

static void
glade_palette_class_init (GladePaletteClass *class)
{
	GObjectClass *object_class;
	
	object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);
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

	class = g_object_get_data (G_OBJECT (button), "user");
	g_return_if_fail (class != NULL);

	gpw = glade_project_window_get ();

	if (GLADE_WIDGET_CLASS_TOPLEVEL (class)) {
		project = gpw->project;
		g_return_if_fail (project != NULL);
		glade_widget_new_toplevel (project, class);
		dont_recurse = TRUE;
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (palette->dummy_button), TRUE);
		dont_recurse = FALSE;
		glade_project_window_set_add_class (gpw, NULL);
	} else {
		glade_project_window_set_add_class (gpw, class);
	}
}

static gboolean
glade_palette_attach_pixmap (GladePalette *palette,
			     GtkWidget *table,
			     GList *list,
			     gint i,
			     gint visual_pos,
			     gint cols)
{
	GladeWidgetClass *class;
	GtkWidget *gtk_pixmap;
	GtkWidget *button;
	gint x, y;

	class = g_list_nth_data (list, i);
	g_return_val_if_fail (class != NULL, FALSE);

	if (!class->in_palette)
		return FALSE;
	
	gtk_pixmap = glade_palette_widget_create_icon_from_class (class);

	button = gtk_radio_button_new (palette->widgets_button_group);
	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
	palette->widgets_button_group = gtk_radio_button_group (GTK_RADIO_BUTTON (button));
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_container_add (GTK_CONTAINER (button), gtk_pixmap);
	glade_util_widget_set_tooltip (button, class->generic_name);

	g_object_set_data (G_OBJECT (button), "user", class);
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (glade_palette_button_clicked), palette);
	
	x = (visual_pos % cols);
	y = (gint) (visual_pos / cols);
	gtk_table_attach (GTK_TABLE (table),
			  button,
			  x, x + 1,
			  y, y + 1,
			  0, 0,
			  0, 0);
	return TRUE;
}

static GtkWidget *
glade_palette_widget_table_create (GladePalette *palette, GladeCatalog *catalog)
{
	GtkWidget *table;
	GtkWidget *dummy;
	GList *list;
	gint num = 0;
	gint num_visible = 0;
	gint visual_pos = 0;
	gint rows;
	gint cols = 4;
	gint i;

	list = catalog->widgets;

	while (list) {
		if (((GladeWidgetClass*) list->data)->in_palette)
			++num_visible;

		list = list->next;
	}

	list = catalog->widgets;

	num = g_list_length (list);
	rows = (gint)((num_visible - 1)/ cols) + 1;

	table = gtk_table_new (rows, cols, TRUE);

	/* The dummy is a button that we don't gtk_show nor gtk_container_add.
	 * It is never displayed, we select it when we don't want anything
	 * selected, since the radio button groups needs a selected button
	 * all the time. Chema
	 */
	dummy = gtk_radio_button_new (palette->widgets_button_group);
	palette->widgets_button_group = gtk_radio_button_group (GTK_RADIO_BUTTON (dummy));
	palette->dummy_button = dummy;
	
	for (i = 0; i < num; i++)
		if (glade_palette_attach_pixmap (palette, table, list, i, visual_pos, cols))
			++visual_pos;
	
	return table;
}

static gint
on_palette_button_toggled (GtkWidget *button, GladePalette *palette)
{
	gint *page;

	page = g_object_get_data (G_OBJECT (button), "page");
	gtk_notebook_set_page (GTK_NOTEBOOK (palette->notebook), *page);
	return TRUE;
}

static GtkWidget *
glade_palette_button_group_create (GladePalette *palette, gchar *name, gint *page)
{
	GtkWidget *button;
	GtkWidget *hbox;

	hbox = gtk_hbox_new (TRUE, 0);
	
	button = gtk_radio_button_new_with_label (palette->sections_button_group, name);
	palette->sections_button_group = gtk_radio_button_group (GTK_RADIO_BUTTON (button));
	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, TRUE, 0);
	g_object_set_data (G_OBJECT (button), "page", page);
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (on_palette_button_toggled), palette);

	return hbox;
}

static void
glade_palette_init (GladePalette *palette)
{
	GtkWidget *selector;
	GtkWidget *widget;

	palette->tooltips = gtk_tooltips_new ();
	palette->widgets_button_group = NULL;
	palette->sections_button_group = NULL;

	/* Selector */
	selector = glade_palette_selector_new (palette);
	gtk_box_pack_start (GTK_BOX (palette), selector, FALSE, TRUE, 3);

	/* Separator */
	widget = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (palette), widget, FALSE, TRUE, 3);

	/* Groups */
	palette->groups_vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (palette), palette->groups_vbox, FALSE, TRUE, 0);
	
	/* Separator */
	widget = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (palette), widget, FALSE, TRUE, 3);

	/* Notebook */
	palette->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (palette->notebook), FALSE);
	gtk_box_pack_end (GTK_BOX (palette), palette->notebook, FALSE, TRUE, 3);

	palette->nb_sections = 0;
}

GladePalette *
glade_palette_new (GList *catalogs)
{
	GladePalette *palette;

	palette = g_object_new (GLADE_TYPE_PALETTE, NULL);
	if (palette == NULL)
		return NULL;

	while (catalogs != NULL) {
		glade_palette_append_catalog (palette, (GladeCatalog *) catalogs->data);
		catalogs = g_list_next (catalogs);
	}

	return palette;
}

void
glade_palette_append_catalog (GladePalette *palette, GladeCatalog* catalog)
{
	GtkWidget *widget;
	gint *page;

	g_return_if_fail (GLADE_IS_PALETTE (palette));
	g_return_if_fail (catalog != NULL);

	/* Add the title of the catalog to the palette */
	page = g_malloc (sizeof (int));
	if (page == NULL)
		return; /* TODO: Add a GError argument to be able to signal out of memory conditions */
	
	*page = palette->nb_sections++;
	widget = glade_palette_button_group_create (palette, catalog->title, page);
	gtk_box_pack_start (GTK_BOX (palette->groups_vbox), widget, FALSE, TRUE, 3);

	/* Add the section */
	widget = glade_palette_widget_table_create (palette, catalog);
	gtk_notebook_append_page (GTK_NOTEBOOK (palette->notebook), widget, NULL);

	gtk_widget_show (palette->notebook);
}

void
glade_palette_unselect_widget (GladePalette *palette)
{
	g_return_if_fail (GLADE_IS_PALETTE (palette));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (palette->dummy_button), TRUE);
}

