/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-palette.c
 *
 * Copyright (C) 2006 The GNOME Foundation.
 * Copyright (C) 2001-2005 Ximian, Inc.
 *
 * Authors:
 *   Chema Celorio
 *   Joaquin Cuenca Abela <e98cuenc@yahoo.com>
 *   Vincent Geddes <vgeddes@metroweb.co.za>
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "glade.h"
#include "glade-app.h"
#include "glade-palette.h"
#include "glade-palette-item.h"
#include "glade-palette-box.h"
#include "glade-palette-expander.h"
#include "glade-catalog.h"
#include "glade-project.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include <glib/gi18n-lib.h>
#include <gdk/gdk.h>

#define GLADE_PALETTE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object),\
					  GLADE_TYPE_PALETTE,                   \
					  GladePalettePrivate))

struct _GladePalettePrivate
{
	const GList *catalogs;        /* List of widget catalogs */

	GtkWidget *selector;    /* Selector item button */
	GtkWidget *tray;	/* Where all the item groups are contained */

	GladeWidgetClass *current_item_class; /* The currently selected item class */

	GSList *sections;   	         /* List of GladePaletteExpanders */ 
	GSList *items_radio_group;       /* Radio group for palette item buttons */

	GtkTooltips *tooltips;           /* Tooltips for the item buttons */
	GtkTooltips *selector_tooltips;  /* Tooltip for the selector */

	GtkSizeGroup *size_group;        /* All items have the same width */

	GladeItemAppearance item_appearance;

	gboolean use_small_item_icons;
	gboolean locked; /* Wheater the palette should "stick" the selector */
};

enum
{
	TOGGLED,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_CURRENT_ITEM_CLASS,
	PROP_ITEM_APPEARANCE,
	PROP_USE_SMALL_ITEM_ICONS
};

static guint glade_palette_signals[LAST_SIGNAL] = {0};

static void glade_palette_append_item_group (GladePalette     *palette, 
					       GladeWidgetGroup *group);

static void glade_palette_update_appearance (GladePalette *palette);

static GtkVBoxClass *parent_class = NULL;

/**
 * glade_palette_set_item_appearance:
 * @palette: a #GladePalette
 * @item_appearance: the item appearance
 *
 * Sets the appearance of the palette items.
 */
void
glade_palette_set_item_appearance (GladePalette *palette, GladeItemAppearance item_appearance)
{
	GladePalettePrivate *priv;
	g_return_if_fail (GLADE_IS_PALETTE (palette));
	priv = GLADE_PALETTE_GET_PRIVATE (palette);

	if (priv->item_appearance != item_appearance)
	{
		priv->item_appearance = item_appearance;

		glade_palette_update_appearance (palette);

		g_object_notify (G_OBJECT (palette), "item-appearance");		
	}
}

/**
 * glade_palette_set_use_small_item_icons:
 * @palette: a #GladePalette
 * @use_small_item_icons: Whether to use small item icons
 *
 * Sets whether to use small item icons.
 */
void
glade_palette_set_use_small_item_icons (GladePalette *palette, gboolean use_small_item_icons)
{
	GladePalettePrivate *priv;
	g_return_if_fail (GLADE_IS_PALETTE (palette));
	priv = GLADE_PALETTE_GET_PRIVATE (palette);

	if (priv->use_small_item_icons != use_small_item_icons)
	{
		priv->use_small_item_icons = use_small_item_icons;

		glade_palette_update_appearance (palette);

		g_object_notify (G_OBJECT (palette), "use-small-item-icons");
		
	}

}

static void 
glade_palette_set_property (GObject *object,
		            guint prop_id,
		      	    const GValue *value,
		            GParamSpec *pspec)
{
	GladePalette *palette = GLADE_PALETTE (object);

	switch (prop_id)
	{
		case PROP_USE_SMALL_ITEM_ICONS:
			glade_palette_set_use_small_item_icons (palette, g_value_get_boolean (value));
			break;
		case PROP_ITEM_APPEARANCE:
			glade_palette_set_item_appearance (palette, g_value_get_enum (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
    }
}

static void
glade_palette_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
	GladePalette *palette = GLADE_PALETTE (object);
	GladePalettePrivate *priv = GLADE_PALETTE_GET_PRIVATE (palette);

	switch (prop_id)
	{
		case PROP_CURRENT_ITEM_CLASS:
			g_value_set_pointer (value, (gpointer) priv->current_item_class);
			break;
		case PROP_USE_SMALL_ITEM_ICONS:
			g_value_set_boolean (value, priv->use_small_item_icons);
			break;
		case PROP_ITEM_APPEARANCE:
			g_value_set_enum (value, priv->item_appearance);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;			
	}
}

static void
glade_palette_dispose (GObject *object)
{
	GladePalette        *palette;
	GladePalettePrivate *priv;
  
	g_return_if_fail (GLADE_IS_PALETTE (object));
	palette = GLADE_PALETTE (object);
	priv = GLADE_PALETTE_GET_PRIVATE (palette);

	priv->catalogs = NULL;

	g_object_unref (priv->tooltips);
	g_object_unref (priv->selector_tooltips);

	G_OBJECT_CLASS (parent_class)->dispose (object);

}

static void
glade_palette_finalize (GObject *object)
{
	GladePalette        *palette;
	GladePalettePrivate *priv;
  
	g_return_if_fail (GLADE_IS_PALETTE (object));
	palette = GLADE_PALETTE (object);
	priv = GLADE_PALETTE_GET_PRIVATE (palette);

	g_slist_free (priv->sections);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
glade_palette_class_init (GladePaletteClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	parent_class = g_type_class_peek_parent (class);

	class->toggled = NULL;
	object_class->get_property = glade_palette_get_property;
	object_class->set_property = glade_palette_set_property;
	object_class->dispose = glade_palette_dispose;
	object_class->finalize = glade_palette_finalize;

	glade_palette_signals[TOGGLED] =
		g_signal_new ("toggled",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladePaletteClass, toggled),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_object_class_install_property (object_class,
					 PROP_ITEM_APPEARANCE,
					 g_param_spec_enum ("item-appearance",
							     "Item Appearance",
							     "The appearance of the palette items",
							     GLADE_ITEM_APPEARANCE_TYPE,
							     GLADE_ITEM_ICON_ONLY,
							     G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_ITEM_APPEARANCE,
					 g_param_spec_boolean ("use-small-item-icons",
							       "Use Small Item Icons",
							       "Whether to use small icons to represent items",
							       FALSE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_CURRENT_ITEM_CLASS,
					 g_param_spec_pointer  ("current-item-class",
							        "Current Item Class",
							        "The GladeWidgetClass of the currently selected item",
							        G_PARAM_READABLE));

	g_type_class_add_private (object_class, sizeof (GladePalettePrivate));
}


static void
glade_palette_on_button_toggled (GtkWidget *button, GladePalette *palette)
{
	GladePalettePrivate *priv;

	g_return_if_fail (GLADE_IS_PALETTE (palette));
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));
	priv = GLADE_PALETTE_GET_PRIVATE (palette);

	/* we are interested only in buttons which toggle from inactive to active */
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
		return;

	if (button == priv->selector)
	{
		priv->current_item_class = NULL;
		priv->locked = FALSE;
	}
	else
	{
		GdkModifierType mask;
		
		priv->current_item_class = 
			glade_palette_item_get_widget_class (GLADE_PALETTE_ITEM (button));
		
		gdk_window_get_pointer (button->window, NULL, NULL, &mask);

	  	priv->locked = 
			(!priv->current_item_class->toplevel) && (mask & GDK_CONTROL_MASK);
	}

	g_signal_emit (G_OBJECT (palette), glade_palette_signals[TOGGLED], 0);
}

static GtkWidget*
glade_palette_new_item (GladePalette *palette, GladeWidgetClass *widget_class)
{
	GladePalettePrivate *priv;
	GtkWidget *item;

	g_return_val_if_fail (GLADE_IS_PALETTE (palette), NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET_CLASS (widget_class), NULL);
	priv = GLADE_PALETTE_GET_PRIVATE (palette);

	item = glade_palette_item_new (widget_class, GTK_RADIO_BUTTON (priv->selector));

	glade_palette_item_set_appearance (GLADE_PALETTE_ITEM (item), priv->item_appearance);

	gtk_tooltips_set_tip (priv->tooltips, item, widget_class->palette_name, NULL);

	g_signal_connect (G_OBJECT (item), "toggled",
			  G_CALLBACK (glade_palette_on_button_toggled), palette);

	return item;
}

static GtkWidget*
glade_palette_new_item_group (GladePalette *palette, GladeWidgetGroup *group)
{
	GladePalettePrivate *priv;
	GtkWidget *expander;
	GtkWidget *box; 
	GtkWidget *item;
	GList *l;
	gchar *title;

	g_return_val_if_fail (GLADE_IS_PALETTE (palette), NULL);
	g_return_val_if_fail (group != NULL, NULL);
	priv = GLADE_PALETTE_GET_PRIVATE (palette);

	box = glade_palette_box_new ();

	/* Go through all the widget classes in this catalog. */
	for (l = glade_widget_group_get_widget_classes (group); l; l = l->next)
	{
		GladeWidgetClass *class =  GLADE_WIDGET_CLASS (l->data);

		/* Create new item */
		item = glade_palette_new_item (palette, class);
		gtk_size_group_add_widget (priv->size_group, GTK_WIDGET (item));
		gtk_container_add (GTK_CONTAINER (box), item);

	}

	title = g_strdup_printf ("<b>%s</b>", glade_widget_group_get_title (group));

	/* Put items box in a expander */
	expander = glade_palette_expander_new (title);
	glade_palette_expander_set_spacing (GLADE_PALETTE_EXPANDER (expander), 2);
	glade_palette_expander_set_use_markup (GLADE_PALETTE_EXPANDER (expander), TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (expander), 1);

	/* set default expanded state */
	glade_palette_expander_set_expanded (GLADE_PALETTE_EXPANDER (expander), 
					     glade_widget_group_get_expanded (group));

	gtk_container_add (GTK_CONTAINER (expander), box);

	g_free (title);

	return expander;
}

static void
glade_palette_append_item_group (GladePalette     *palette, 
				 GladeWidgetGroup *group)
{
	GladePalettePrivate *priv;
	GtkWidget *expander;

	g_return_if_fail (GLADE_IS_PALETTE (palette));
	g_return_if_fail (group != NULL);
	priv = GLADE_PALETTE_GET_PRIVATE (palette);

	expander = glade_palette_new_item_group (palette, group);

	priv->sections = g_slist_append (priv->sections, expander);

	gtk_box_pack_start (GTK_BOX (priv->tray), expander, FALSE, FALSE, 0);

}

static void
glade_palette_update_appearance (GladePalette *palette)
{
	GladePalettePrivate *priv;
	GtkWidget *viewport;
	GSList *sections;
	GList *items, *i;

	priv = GLADE_PALETTE_GET_PRIVATE (palette);

	for (sections = priv->sections; sections; sections = sections->next)
	{
		items = gtk_container_get_children (GTK_CONTAINER (gtk_bin_get_child (GTK_BIN (sections->data))));

		for (i = items; i; i = i->next)
		{
			glade_palette_item_set_appearance (GLADE_PALETTE_ITEM (i->data), priv->item_appearance);
			glade_palette_item_set_use_small_icon (GLADE_PALETTE_ITEM (i->data), priv->use_small_item_icons);
		}
		g_list_free (items);
	}
		
	/* FIXME: Removing and then adding the tray again to the Viewport
         *        is the only way I can get the Viewport to 
         *        respect the new width of the tray.
         *        There should be a better solution.
	 */        
	viewport = gtk_widget_get_parent (priv->tray);
	if (viewport != NULL)
	{
		g_object_ref (priv->tray);

		gtk_container_remove (GTK_CONTAINER (viewport), priv->tray);
		gtk_container_add (GTK_CONTAINER (viewport), priv->tray);

		g_object_unref (priv->tray);
	}

	if (priv->item_appearance == GLADE_ITEM_ICON_ONLY)
		gtk_tooltips_enable (priv->tooltips);
	else
		gtk_tooltips_disable (priv->tooltips);

}

static GtkWidget*
glade_palette_selector_new (GladePalette *palette)
{
	GladePalettePrivate *priv;
	GtkWidget *hbox;
	GtkWidget *image;
	gchar *path;

	priv = GLADE_PALETTE_GET_PRIVATE (palette);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 1);

	/* Create selector button */
	priv->selector = gtk_radio_button_new (priv->items_radio_group);

	path = g_build_filename (glade_pixmaps_dir, "selector.png", NULL);
	image = gtk_image_new_from_file (path);
	g_free (path);
	
	gtk_container_add (GTK_CONTAINER (priv->selector), image);
	gtk_button_set_relief (GTK_BUTTON (priv->selector), GTK_RELIEF_NONE);
	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (priv->selector), FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->selector), TRUE);

	gtk_tooltips_set_tip (priv->selector_tooltips, priv->selector, _("Widget Selector"), NULL);

	g_signal_connect (G_OBJECT (priv->selector), "toggled",
			  G_CALLBACK (glade_palette_on_button_toggled), 
			  palette);

	/* Pack button into hbox */
	gtk_box_pack_start (GTK_BOX (hbox), priv->selector, FALSE, FALSE, 1);


	return hbox;
}

static void
glade_palette_init (GladePalette *palette)
{
	GladePalettePrivate *priv;
	GtkWidget *widget;

	priv = GLADE_PALETTE_GET_PRIVATE (palette);

	priv->catalogs = NULL;
	priv->current_item_class = NULL;
	priv->items_radio_group = NULL;
	priv->sections = NULL;
	priv->item_appearance = GLADE_ITEM_ICON_ONLY;
	priv->use_small_item_icons = FALSE;


	/* Create tooltips */
	priv->tooltips = gtk_tooltips_new ();
	g_object_ref (priv->tooltips);
	gtk_object_sink (GTK_OBJECT (priv->tooltips));

	priv->selector_tooltips = gtk_tooltips_new ();
	g_object_ref (priv->selector_tooltips);
	gtk_object_sink (GTK_OBJECT (priv->selector_tooltips));


	/* Create Selector */
	widget = glade_palette_selector_new (palette);
	gtk_box_pack_start (GTK_BOX (palette), widget, FALSE, FALSE, 0);


	/* Add items tray (via a scrolled window) */
	priv->tray = gtk_vbox_new (FALSE, 0);
	g_object_ref (G_OBJECT (priv->tray));
	gtk_object_sink (GTK_OBJECT (priv->tray));
	gtk_container_set_border_width (GTK_CONTAINER (priv->tray), 1);

	/* Create Size Group */
	priv->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_set_ignore_hidden (priv->size_group, FALSE);
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
 * glade_palette_get_current_item_class:
 * @palette: a #GladePalette
 *
 * Gets the #GladeWidgetClass of the currently selected item.
 *
 * Returns: the #GladeWidgetClass of currently selected item, or NULL
 *          if no item is selected.
 */
GladeWidgetClass *
glade_palette_get_current_item_class (GladePalette *palette)
{
	GladePalettePrivate *priv;
	g_return_val_if_fail (GLADE_IS_PALETTE (palette), NULL);
	priv = GLADE_PALETTE_GET_PRIVATE (palette);


	return priv->current_item_class;

}

/**
 * glade_palette_new:
 * @catalogs: the widget catalogs for the palette.
 * @item_appearance: the appearance of the palette items.
 *
 * Creates a new #GladePalette widget
 *
 * Returns: a new #GladePalette
 */
GtkWidget*
glade_palette_new (const GList *catalogs, GladeItemAppearance item_appearance)
{
	GladePalette *palette;
	GladePalettePrivate *priv;
	GtkWidget *sw;
	GList *l;

	g_return_val_if_fail (catalogs != NULL, NULL);

	palette = g_object_new (GLADE_TYPE_PALETTE,
				"spacing", 2, 
				"item-appearance", item_appearance,
				NULL);

	g_return_val_if_fail (palette != NULL, NULL);

	priv = GLADE_PALETTE_GET_PRIVATE (palette);

	priv->catalogs = catalogs;

	for (l = (GList *) priv->catalogs; l; l = l->next) 
	{
		GList *groups = glade_catalog_get_widget_groups (GLADE_CATALOG (l->data));

		for (; groups; groups = groups->next)
		{
			GladeWidgetGroup *group = GLADE_WIDGET_GROUP (groups->data);

			if (glade_widget_group_get_widget_classes (group)) 
				glade_palette_append_item_group (palette, group);
		}
	}

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), priv->tray);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
					     GTK_SHADOW_NONE);

	gtk_box_pack_start (GTK_BOX (palette), sw, TRUE, TRUE, 0);

	g_object_unref (priv->size_group);

	return GTK_WIDGET (palette);
}

/**
 * glade_palette_deselect_current_item:
 * @palette: a #GladePalette
 *
 * Deselects the currently selected item
 */
void
glade_palette_deselect_current_item (GladePalette *palette)
{
	GladePalettePrivate *priv;
	g_return_if_fail (GLADE_IS_PALETTE (palette));
	priv = GLADE_PALETTE_GET_PRIVATE (palette);

	if (priv->locked == FALSE)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->selector),
					      TRUE);
}

/**
 * glade_palette_get_item_appearance:
 * @palette: a #GladePalette
 *
 * Returns: The appearance of the palette items
 */
GladeItemAppearance
glade_palette_get_item_appearance (GladePalette *palette)
{
	GladePalettePrivate *priv;
	g_return_val_if_fail (GLADE_IS_PALETTE (palette), 
			      GLADE_ITEM_ICON_ONLY);
	priv = GLADE_PALETTE_GET_PRIVATE (palette);

	return priv->item_appearance;
}

/**
 * glade_palette_get_use_small_item_icons:
 * @palette: a #GladePalette
 *
 * Returns: Whether small item icons are used
 */
gboolean
glade_palette_get_use_small_item_icons (GladePalette *palette)
{
	GladePalettePrivate *priv;
	g_return_val_if_fail (GLADE_IS_PALETTE (palette), FALSE);
	priv = GLADE_PALETTE_GET_PRIVATE (palette);

	return priv->use_small_item_icons;
}
