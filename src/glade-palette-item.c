/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-palette-item.c
 *
 * Copyright (C) 2006 The GNOME Foundation.
 *
 * Authors:
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
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA.
 *
 */

#include <gtk/gtkalignment.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkimage.h>

#include "glade.h"
#include "glade-palette-item.h"

#define GLADE_PALETTE_ITEM_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object),\
					       GLADE_TYPE_PALETTE_ITEM,              \
					       GladePaletteItemPrivate))


struct _GladePaletteItemPrivate
{
	GtkWidget *alignment; /* The contents of the button */
	GtkWidget *hbox;       
	GtkWidget *icon;  
	GtkWidget *label;
	
	GladeItemAppearance appearance; /* The appearance of the button */

	gboolean use_small_icon;

	const GladeWidgetClass *widget_class; /* The widget class associated with this item */
};

enum
{
	PROP_0,
	PROP_WIDGET_CLASS,
	PROP_APPEARANCE,
	PROP_USE_SMALL_ICON
};


static GtkRadioButton *parent_class = NULL;


static void
glade_palette_item_update_appearance (GladePaletteItem *item)
{
	GladePaletteItemPrivate *priv;
	GtkWidget *child;
	GList *l;
	
	g_return_if_fail (GLADE_IS_PALETTE_ITEM (item));
	priv = GLADE_PALETTE_ITEM_GET_PRIVATE (item);

	child = gtk_bin_get_child (GTK_BIN (item));

	if (GTK_IS_WIDGET (child))
		gtk_container_remove (GTK_CONTAINER (item), child);

	for (l = gtk_container_get_children (GTK_CONTAINER (priv->hbox)); l; l=l->next)
		gtk_container_remove (GTK_CONTAINER (priv->hbox), GTK_WIDGET (l->data));

	if (priv->appearance == GLADE_ITEM_ICON_AND_LABEL)
	{
		gtk_box_pack_start (GTK_BOX (priv->hbox), priv->icon, FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (priv->hbox), priv->label, FALSE, FALSE, 0);
		gtk_container_add (GTK_CONTAINER (item), priv->alignment);
	}
	else if (priv->appearance == GLADE_ITEM_ICON_ONLY)
	{
		
		gtk_container_add (GTK_CONTAINER (item), priv->icon);
		gtk_misc_set_alignment (GTK_MISC (priv->icon), 0.5, 0.5);

	}
	else if (priv->appearance == GLADE_ITEM_LABEL_ONLY)
	{
		gtk_container_add (GTK_CONTAINER (item), priv->label);
		gtk_misc_set_alignment (GTK_MISC (priv->label), 0.0, 0.5);
	}
	else
		g_return_if_reached ();
}


/**
 * glade_palette_item_set_appearance:
 * @palette: A #GladePaletteItem.
 * @appearance: The appearance of the item.
 *
 * Sets the appearance of the item. 
 */
void
glade_palette_item_set_appearance (GladePaletteItem *item, GladeItemAppearance appearance)
{
	GladePaletteItemPrivate *priv;
	g_return_if_fail (GLADE_IS_PALETTE_ITEM (item));
	priv = GLADE_PALETTE_ITEM_GET_PRIVATE (item);


	if (priv->appearance != appearance)
	{
		priv->appearance = appearance;		

		glade_palette_item_update_appearance (item);

		g_object_notify (G_OBJECT (item), "appearance");
	}
}

void
glade_palette_item_set_use_small_icon (GladePaletteItem *item, gboolean use_small_icon)
{
	GladePaletteItemPrivate *priv;
	g_return_if_fail (GLADE_IS_PALETTE_ITEM (item));
	priv = GLADE_PALETTE_ITEM_GET_PRIVATE (item);


	if (priv->use_small_icon != use_small_icon)
	{
		priv->use_small_icon = use_small_icon;		

		if (use_small_icon != FALSE)
			gtk_image_set_from_pixbuf (GTK_IMAGE (priv->icon),
						   priv->widget_class->small_icon);
		else
			gtk_image_set_from_pixbuf (GTK_IMAGE (priv->icon),
						   priv->widget_class->large_icon); 

		g_object_notify (G_OBJECT (item), "use-small-icon");
	}
}

static void 
glade_palette_item_set_property (GObject *object,
		      		 guint prop_id,
		      		 const GValue *value,
		      		 GParamSpec *pspec)
{
	GladePaletteItem *item;
	GladePaletteItemPrivate *priv;

	item = GLADE_PALETTE_ITEM (object);

	g_return_if_fail (GLADE_IS_PALETTE_ITEM (item));

	priv = GLADE_PALETTE_ITEM_GET_PRIVATE (item);

	switch (prop_id)
	{
		case PROP_WIDGET_CLASS:
			priv->widget_class = (const GladeWidgetClass *) g_value_get_pointer (value);
			break;
		case PROP_APPEARANCE:
			glade_palette_item_set_appearance (item, g_value_get_enum (value));
			break;
		case PROP_USE_SMALL_ICON:
			glade_palette_item_set_use_small_icon (item, g_value_get_boolean (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
    }
}

static void
glade_palette_item_get_property (GObject    *object,
			         guint       prop_id,
			         GValue     *value,
			         GParamSpec *pspec)
{
	GladePaletteItem *item = GLADE_PALETTE_ITEM (object);
	GladePaletteItemPrivate *priv = GLADE_PALETTE_ITEM_GET_PRIVATE (item);

	switch (prop_id)
	{
		case PROP_WIDGET_CLASS:
			g_value_set_pointer (value, (gpointer) priv->widget_class);
			break;
		case PROP_APPEARANCE:
			g_value_set_enum (value, priv->appearance);
			break;
		case PROP_USE_SMALL_ICON:
			g_value_set_boolean (value, priv->use_small_icon);
			break;		
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;			
	}
}

static void
glade_palette_item_dispose (GObject *object)
{
	GladePaletteItem *item;
	GladePaletteItemPrivate *priv;

	g_return_if_fail (GLADE_IS_PALETTE_ITEM (object));
	item = GLADE_PALETTE_ITEM (object);
	priv = GLADE_PALETTE_ITEM_GET_PRIVATE (item);

	g_object_unref (priv->alignment);
	g_object_unref (priv->hbox);
	g_object_unref (priv->icon);
	g_object_unref (priv->label);

}

static void
glade_palette_item_class_init (GladePaletteItemClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	parent_class = g_type_class_peek_parent (class);


	object_class->get_property = glade_palette_item_get_property;
	object_class->set_property = glade_palette_item_set_property;
	object_class->dispose = glade_palette_item_dispose;

	g_object_class_install_property (object_class,
					 PROP_WIDGET_CLASS,
					 g_param_spec_pointer  ("widget-class",
							        "Widget Class",
							        "The widget class associated with this item",
							        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE ));

	g_object_class_install_property (object_class,
					 PROP_APPEARANCE,
					 g_param_spec_enum  ("appearance",
							     "Appearance",
							     "The appearance of the item",
							     GLADE_ITEM_APPEARANCE_TYPE,
							     GLADE_ITEM_ICON_ONLY,
							     G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_APPEARANCE,
					 g_param_spec_boolean ("use-small-icon",
							       "Use Small Icon",
							       "Whether to use small icon",
							       FALSE,
							       G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GladePaletteItemPrivate));
}


static void
glade_palette_item_init (GladePaletteItem *item)
{
	GladePaletteItemPrivate *priv;

	priv = GLADE_PALETTE_ITEM_GET_PRIVATE (item);

	priv->label = NULL;
	priv->widget_class = NULL;
	priv->use_small_icon = FALSE;
	priv->appearance =  0;

	priv->alignment = gtk_alignment_new (0.0, 0.5, 0.5, 0.5);
	g_object_ref (priv->alignment);
	gtk_object_sink (GTK_OBJECT (priv->alignment));
	gtk_widget_show (GTK_WIDGET (priv->alignment));

	priv->hbox = gtk_hbox_new (FALSE, 6);
	g_object_ref (priv->hbox);
	gtk_object_sink (GTK_OBJECT (priv->hbox));
	gtk_widget_show (GTK_WIDGET (priv->hbox));

	priv->icon = gtk_image_new ();
	g_object_ref (priv->icon);
	gtk_object_sink (GTK_OBJECT (priv->icon));
	gtk_widget_show (GTK_WIDGET (priv->icon));

	priv->label = gtk_label_new (NULL);
	g_object_ref (priv->label);
	gtk_object_sink (GTK_OBJECT (priv->label));
	gtk_widget_show (GTK_WIDGET (priv->label));

	gtk_container_add (GTK_CONTAINER (priv->alignment), priv->hbox);
	
	gtk_button_set_relief (GTK_BUTTON (item), GTK_RELIEF_NONE);
	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (item), FALSE);
}

GType
glade_palette_item_get_type (void)
{
	static GType type = 0;

	if (!type)
	{
		static const GTypeInfo info =
		{
			sizeof (GladePaletteItemClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) glade_palette_item_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (GladePaletteItem),
			0,
			(GInstanceInitFunc) glade_palette_item_init
		};

		type = g_type_register_static (GTK_TYPE_RADIO_BUTTON, "GladePaletteItem", &info, 0);
	}

	return type;
}

/**
 * glade_palette_item_new:
 * @widget_class: A #GladeWidgetClass
 * @group: The group to add this item to.
 * @appearance: The appearance of the item
 *
 * Returns: A #GtkWidget
 */
GtkWidget*
glade_palette_item_new (const GladeWidgetClass *widget_class, GtkRadioButton *group)
{
	GladePaletteItem *item;
	GladePaletteItemPrivate *priv;

	g_return_val_if_fail (widget_class != NULL, NULL);

	item = g_object_new (GLADE_TYPE_PALETTE_ITEM,
			     "group", group,
			     "widget-class", widget_class,
			     "appearance", GLADE_ITEM_ICON_ONLY,
			     NULL);

	g_return_val_if_fail (item != NULL, NULL);

	priv = GLADE_PALETTE_ITEM_GET_PRIVATE (item);

	gtk_label_set_text (GTK_LABEL (priv->label), widget_class->palette_name);
	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->icon), widget_class->large_icon);

	return GTK_WIDGET (item);
}

/**
 * glade_palette_item_get_appearance:
 * @palette: A #GladePaletteItem
 *
 * Returns: the appearance of the item.
 */
GladeItemAppearance
glade_palette_item_get_appearance (GladePaletteItem *item)
{
	GladePaletteItemPrivate *priv;

	g_return_val_if_fail (GLADE_IS_PALETTE_ITEM (item), FALSE);
	priv = GLADE_PALETTE_ITEM_GET_PRIVATE (item);

	return priv->appearance;
}

gboolean
glade_palette_item_get_use_small_icon (GladePaletteItem *item)
{
	GladePaletteItemPrivate *priv;

	g_return_val_if_fail (GLADE_IS_PALETTE_ITEM (item), FALSE);
	priv = GLADE_PALETTE_ITEM_GET_PRIVATE (item);

	return priv->use_small_icon;
}

/**
 * glade_palette_item_get_widget_class:
 * @palette: A #GladePaletteItem
 *
 * Returns: the #GladeWidgetClass associated with this item.
 */
GladeWidgetClass*
glade_palette_item_get_widget_class (GladePaletteItem *item)
{
	GladePaletteItemPrivate *priv;
	g_return_val_if_fail (GLADE_IS_PALETTE_ITEM (item), NULL);	
	priv = GLADE_PALETTE_ITEM_GET_PRIVATE (item);

	return (GladeWidgetClass *) priv->widget_class;
}
