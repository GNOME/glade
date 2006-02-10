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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Chema Celorio <chema@celorio.com>
 *   Joaquin Cuenca Abela <e98cuenc@yahoo.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include "glade.h"
#include "glade-palette.h"
#include "glade-catalog.h"
#include "glade-project.h"
#include "glade-widget.h"
#include "glade-widget-class.h"


enum
{
	TOGGLED,
	CATALOG_CHANGED,
	LAST_SIGNAL
};

const  gchar *GLADE_PALETTE_BUTTON_CLASS_DATA = "user";

static guint glade_palette_signals[LAST_SIGNAL] = {0};

static GtkWindowClass *parent_class = NULL;

static void glade_palette_append_widget_group (GladePalette     *palette, 
					       GladeWidgetGroup *group);

static void
glade_palette_class_init (GladePaletteClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);


	class->toggled        = NULL;
	class->catalog_change = NULL;

	glade_palette_signals[TOGGLED] =
		g_signal_new ("toggled",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladePaletteClass, toggled),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	glade_palette_signals[CATALOG_CHANGED] =
		g_signal_new ("catalog-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladePaletteClass, catalog_change),
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
		class = g_object_get_data (G_OBJECT (button),
					   GLADE_PALETTE_BUTTON_CLASS_DATA);
		g_return_if_fail (GLADE_IS_WIDGET_CLASS (class));

		palette->current = class;
		gtk_label_set_text (GTK_LABEL (palette->label),
				    class->name);
	}

	g_signal_emit (G_OBJECT (palette), glade_palette_signals[TOGGLED], 0);
}

static void
glade_palette_on_catalog_selector_changed (GtkWidget *combo_box,
					   GladePalette *palette)
{
	/* find out which catalog was selected. */
	gint page = gtk_combo_box_get_active (GTK_COMBO_BOX (palette->catalog_selector));

	/* Select that catalog in the notebook. */
	gtk_notebook_set_current_page (GTK_NOTEBOOK (palette->notebook), page);

	g_signal_emit (G_OBJECT (palette), glade_palette_signals[CATALOG_CHANGED], 0);

}

static GtkWidget *
glade_palette_selector_new (GladePalette *palette)
{
	GtkWidget *hbox;
	GtkWidget *image;
	gchar *filename;

	hbox = gtk_hbox_new (FALSE, 0);

	palette->selector = gtk_radio_button_new (palette->widgets_button_group);
	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (palette->selector), FALSE);
	palette->widgets_button_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (palette->selector));
	gtk_button_set_relief (GTK_BUTTON (palette->selector), GTK_RELIEF_NONE);
	filename = g_build_filename (glade_pixmaps_dir, "selector.png", NULL);
	image = gtk_image_new_from_file (filename);
	g_free (filename);

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


/**
 * glade_palette_create_widget_class_button:
 * @palette:		the palette to add to.
 * @widget_class:       the widget class who's button we're adding.
 * 
 * Creates a GtkHBox with an icon for the corresponding @widget_class and
 * the name.
 * 
 * Return Value: the GtkHBox
 **/
static GtkWidget *
glade_palette_create_widget_class_button (GladePalette *palette, 
					  GladeWidgetClass *widget_class)
{
	GtkWidget *label;
	GtkWidget *radio;
	GtkWidget *hbox;
	GtkWidget *image;

	label = gtk_label_new (widget_class->palette_name);
	radio = gtk_radio_button_new (palette->widgets_button_group);
	image = gtk_image_new_from_pixbuf (widget_class->icon);

	g_object_set_data (G_OBJECT (radio), GLADE_PALETTE_BUTTON_CLASS_DATA,
			   widget_class);
	palette->widgets_button_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radio));
	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (radio), FALSE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE,  TRUE,  1);

	gtk_button_set_relief (GTK_BUTTON (radio), GTK_RELIEF_NONE);
	gtk_container_add (GTK_CONTAINER (radio), hbox);

	g_signal_connect (G_OBJECT (radio), "toggled",
			  G_CALLBACK (glade_palette_on_button_toggled), palette);
	return radio;
}

static GtkWidget *
glade_palette_widget_table_create (GladePalette     *palette, 
				   GladeWidgetGroup *group)
{
	GList *list;
	GtkWidget *sw;
	GtkWidget *vbox;

	vbox = gtk_vbox_new (FALSE, 0);

	list = glade_widget_group_get_widget_classes (group);

	/* Go through all the widget classes in this catalog. */
	for (; list; list = list->next)
	{
		GladeWidgetClass *gwidget_class = 
			GLADE_WIDGET_CLASS (list->data);

		/*
		 * If the widget class wants to be in the palette (I don't
		 * know why a widget class wouldn't want to be, but whatever..
		 */
		if (gwidget_class->in_palette)
		{
			GtkWidget *button = 
				glade_palette_create_widget_class_button
				(palette, gwidget_class);
			gtk_box_pack_start (GTK_BOX (vbox), button,
					    FALSE, FALSE, 0);
		}
	}

	/* Add it in a scrolled window. */
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), vbox);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), 
					     GTK_SHADOW_NONE);

	return sw;
}

static void
glade_palette_init (GladePalette *palette)
{
	GtkWidget *selector_hbox;
	GtkWidget *widget;
	GtkWidget *combo_box;

	palette->current = NULL;
	palette->widgets_button_group = NULL;
	palette->catalog_selector = NULL;

	/* Selector */
	selector_hbox = glade_palette_selector_new (palette);
	gtk_box_pack_start (GTK_BOX (palette), selector_hbox, FALSE, TRUE, 0);

	/* Separator */
	widget = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (palette), widget, FALSE, TRUE, 3);

	/* The catalog selector menu. */
	combo_box = gtk_combo_box_new_text ();
	gtk_box_pack_start (GTK_BOX (palette), combo_box, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (combo_box), "changed", 
			  G_CALLBACK (glade_palette_on_catalog_selector_changed),
			  palette);

	gtk_widget_show (combo_box);
	palette->catalog_selector = combo_box;

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

static void
glade_palette_append_widget_group (GladePalette     *palette, 
				   GladeWidgetGroup *group)
{
	gint page;
	GtkWidget *widget;

	g_return_if_fail (GLADE_IS_PALETTE (palette));
	g_return_if_fail (group != NULL);

	page = palette->nb_sections++;

	/* Add the catalog's title to the GtkComboBox */
	gtk_combo_box_append_text (GTK_COMBO_BOX (palette->catalog_selector),
				   glade_widget_group_get_title (group));

	/* Add the section */
	widget = glade_palette_widget_table_create (palette, group);
	gtk_notebook_append_page (GTK_NOTEBOOK (palette->notebook), widget, NULL);

	gtk_widget_show (palette->notebook);
}

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

/**
 * glade_palette_new:
 * @catalogs:
 * 
 * Create a new palette
 *
 * Returns: the new GladePalette
 */
GladePalette *
glade_palette_new (GList *catalogs)
{
	GList *list;
	GladePalette *palette;

	palette = g_object_new (GLADE_TYPE_PALETTE, NULL);
	g_return_val_if_fail (palette != NULL, NULL);

	for (list = catalogs; list; list = list->next) 
	{
		GList *l;

		l = glade_catalog_get_widget_groups (GLADE_CATALOG (list->data));
		for (; l; l = l->next)
		{
			GladeWidgetGroup *group = GLADE_WIDGET_GROUP (l->data);

			if (glade_widget_group_get_widget_classes (group)) 
				glade_palette_append_widget_group (palette,
								   group);
		}
	}

	gtk_combo_box_set_active (GTK_COMBO_BOX (palette->catalog_selector), 0);
	return palette;
}

/**
 * glade_palette_unselect_widget:
 * @palette:
 *
 * TODO: write me
 */
void
glade_palette_unselect_widget (GladePalette *palette)
{
	g_return_if_fail (GLADE_IS_PALETTE (palette));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (palette->selector), TRUE);
}
