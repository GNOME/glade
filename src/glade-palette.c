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
#define PIXMAPS_DIR "E:/Programmation/Glade/msvc6/pixmaps"
#endif

#include "glade.h"
#include "glade-palette.h"
#include "glade-catalog.h"
#include "glade-project.h"
#include "glade-widget.h"
#include "glade-widget-class.h"


static void glade_palette_class_init (GladePaletteClass *class);
static void glade_palette_init (GladePalette *glade_palette);

enum
{
	TOGGLED,
	LAST_SIGNAL
};

static guint glade_palette_signals[LAST_SIGNAL] = {0};

static GtkWindowClass *parent_class = NULL;

GType
glade_palette_get_type (void)
{
	static GType type = 0;

	if (!type)
	{
		static const GTypeInfo info =
		{
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

	glade_palette_signals[TOGGLED] =
		g_signal_new ("toggled",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladePaletteClass, toggled),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
}

static void
glade_palette_on_button_toggled (GtkWidget *button, GladePalette *palette)
{
	GladeWidgetClass *class;

	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));
	g_return_if_fail (GLADE_IS_PALETTE (palette));

	/* we are interested only in buttons which toggle
	 * from inactive to active */
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
		return;

	if (button == palette->selector)
	{
		palette->current = NULL;
		gtk_label_set_text (GTK_LABEL (palette->label),
				    _("Selector"));
	}
	else
	{
		class = g_object_get_data (G_OBJECT (button), "user");
		g_return_if_fail (GLADE_IS_WIDGET_CLASS (class));

		palette->current = class;
		gtk_label_set_text (GTK_LABEL (palette->label),
				    class->name);
	}

	g_signal_emit (G_OBJECT (palette), glade_palette_signals[TOGGLED], 0);
}

static GtkWidget *
glade_palette_selector_new (GladePalette *palette)
{
	GtkWidget *hbox;
	GtkWidget *image;

	hbox = gtk_hbox_new (FALSE, 0);

	palette->selector = gtk_radio_button_new (palette->widgets_button_group);
	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (palette->selector), FALSE);
	palette->widgets_button_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (palette->selector));
	gtk_button_set_relief (GTK_BUTTON (palette->selector), GTK_RELIEF_NONE);
	image = gtk_image_new_from_file (PIXMAPS_DIR "/selector.png");

	gtk_container_add (GTK_CONTAINER (palette->selector), image);
	glade_util_widget_set_tooltip (palette->selector, _("Selector"));

	g_signal_connect (G_OBJECT (palette->selector), "toggled",
			  G_CALLBACK (glade_palette_on_button_toggled), palette);

	/* the label defaults to "Selector" */
	palette->label = gtk_label_new (_("Selector"));
	gtk_misc_set_alignment (GTK_MISC (palette->label), 0.0, 0.5);

	gtk_box_pack_start (GTK_BOX (hbox), palette->selector, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), palette->label, TRUE, TRUE, 2);

	gtk_widget_show_all (hbox);

	return hbox;	
}

static gboolean
glade_palette_attach_icon (GladePalette *palette,
			   GtkWidget *table,
			   GList *list,
			   gint i,
			   gint visual_pos,
			   gint cols)
{
	GladeWidgetClass *class;
	GtkWidget *button;
	gint x, y;

	class = g_list_nth_data (list, i);
	g_return_val_if_fail (class != NULL, FALSE);

	if (!class->in_palette)
		return FALSE;

	button = gtk_radio_button_new (palette->widgets_button_group);
	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
	palette->widgets_button_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_container_add (GTK_CONTAINER (button), class->icon);
	glade_util_widget_set_tooltip (button, class->generic_name);

	/* store the widget class in the button */
	g_object_set_data (G_OBJECT (button), "user", class);
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (glade_palette_on_button_toggled), palette);

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
	GList *list;
	GtkVBox *vbox;
	GtkHBox *hbox;
	GtkLabel *label;
	GtkRadioButton *radio;
	GtkScrolledWindow *scrolled_window;
	int nb_rows = 0;
	int row = 0;

	list = glade_catalog_get_widget_classes (catalog);
	while (list)
	{
		GladeWidgetClass *widget_class = (GladeWidgetClass*) list->data;
		if (widget_class->in_palette)
			nb_rows++;

		list = list->next;
	}

	vbox = GTK_VBOX (gtk_vbox_new (FALSE, 0));

	list = glade_catalog_get_widget_classes (catalog);
	while (list)
	{
		GladeWidgetClass *widget_class = (GladeWidgetClass*) list->data;
		if (widget_class->in_palette)
		{
			label = GTK_LABEL (gtk_label_new (widget_class->palette_name));
			radio = GTK_RADIO_BUTTON (gtk_radio_button_new (palette->widgets_button_group));

			g_object_set_data (G_OBJECT (radio), "user", list->data);
			palette->widgets_button_group = gtk_radio_button_get_group (radio);
			gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (radio), FALSE);
			gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

			hbox = GTK_HBOX (gtk_hbox_new (FALSE, 2));
			gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (widget_class->icon), FALSE, FALSE, 0);
			gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (label), TRUE, TRUE, 1);

			gtk_button_set_relief (GTK_BUTTON (radio), GTK_RELIEF_NONE);
			gtk_container_add (GTK_CONTAINER (radio), GTK_WIDGET (hbox));

			g_signal_connect (G_OBJECT (radio), "toggled",
					  G_CALLBACK (glade_palette_on_button_toggled), palette);

			gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (radio), FALSE, FALSE, 0);
			row++;
		}

		list = list->next;
	}

	scrolled_window = GTK_SCROLLED_WINDOW (gtk_scrolled_window_new (NULL, NULL));
	gtk_scrolled_window_set_policy (scrolled_window, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (scrolled_window, GTK_WIDGET (vbox));
	gtk_scrolled_window_set_shadow_type (scrolled_window, GTK_SHADOW_NONE);
	gtk_widget_set_size_request (GTK_WIDGET (scrolled_window), -1, 400);

	return GTK_WIDGET (scrolled_window);
}

static void
glade_palette_init (GladePalette *palette)
{
	GtkWidget *selector_hbox;
	GtkWidget *widget;

	palette->current = NULL;
	palette->widgets_button_group = NULL;
	palette->sections_button_group = NULL;

	/* Selector */
	selector_hbox = glade_palette_selector_new (palette);
	gtk_box_pack_start (GTK_BOX (palette), selector_hbox, FALSE, TRUE, 0);

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
	gtk_notebook_set_show_border (GTK_NOTEBOOK (palette->notebook), FALSE);
	gtk_box_pack_end (GTK_BOX (palette), palette->notebook, TRUE, TRUE, 0);

	palette->nb_sections = 0;
}

static gint
glade_palette_on_catalog_button_toggled (GtkWidget *button,
					 GladePalette *palette)
{
	gint page;

	page = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "page"));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (palette->notebook), page);

	return TRUE;
}

void
glade_palette_append_catalog (GladePalette *palette, GladeCatalog *catalog)
{
	gint page;
	GtkWidget *button;
	GtkWidget *widget;

	g_return_if_fail (GLADE_IS_PALETTE (palette));
	g_return_if_fail (catalog != NULL);

	page = palette->nb_sections++;

	/* Add the button */ 
	button = gtk_radio_button_new_with_label (palette->sections_button_group, catalog->title);
	palette->sections_button_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
	g_object_set_data (G_OBJECT (button), "page", GINT_TO_POINTER (page));
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (glade_palette_on_catalog_button_toggled),
			  palette);
	gtk_box_pack_start (GTK_BOX (palette->groups_vbox), button, FALSE, TRUE, 0);

	/* Add the section */
	widget = glade_palette_widget_table_create (palette, catalog);
	gtk_notebook_append_page (GTK_NOTEBOOK (palette->notebook), widget, NULL);

	gtk_widget_show (palette->notebook);
}

GladePalette *
glade_palette_new (GList *catalogs)
{
	GList *list;
	GladePalette *palette;

	palette = g_object_new (GLADE_TYPE_PALETTE, NULL);
	g_return_val_if_fail (palette != NULL, NULL);

	for (list = catalogs; list; list = list->next)
		glade_palette_append_catalog (palette, GLADE_CATALOG (list->data));

	return palette;
}

void
glade_palette_unselect_widget (GladePalette *palette)
{
	g_return_if_fail (GLADE_IS_PALETTE (palette));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (palette->selector), TRUE);
}

