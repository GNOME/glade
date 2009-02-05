/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-palette-item.c
 *
 * Copyright (C) 2007 Vincent Geddes
 *
 * Authors:  Vincent Geddes <vgeddes@metroweb.co.za>
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

#include "config.h"

#include <gtk/gtk.h>

#include "glade.h"
#include "glade-palette-item.h"
#include "glade-popup.h"

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

	GladeWidgetAdaptor *adaptor; /* The widget class adaptor associated 
				      * with this item 
				      */
};

enum
{
	PROP_0,
	PROP_ADAPTOR,
	PROP_APPEARANCE,
	PROP_USE_SMALL_ICON
};


G_DEFINE_TYPE(GladePaletteItem, glade_palette_item, GTK_TYPE_TOGGLE_BUTTON)

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

#if 0
/* XXX Code doesnt work well, think they are not transperent ? */
static GdkPixbuf *
render_deprecated_pixbuf (GladeWidgetAdaptor *adaptor,
			  gint                size)
{

	GdkPixbuf *widget_icon, *deprecated_icon;

	if ((widget_icon =
	     gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
				       adaptor->icon_name, size, 
				       GTK_ICON_LOOKUP_USE_BUILTIN, NULL)) != NULL)
	{
		if ((deprecated_icon =
		     gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
					       "media-record", size, 
					       GTK_ICON_LOOKUP_USE_BUILTIN, NULL)) != NULL)
		{
			g_print ("rendering deprecated\n");
			gdk_pixbuf_composite (deprecated_icon, widget_icon,
					      0, 0, 
					      gdk_pixbuf_get_width (widget_icon),
					      gdk_pixbuf_get_height (widget_icon),
					      0.0, 0.0, 1.0, 1.0, GDK_INTERP_HYPER, 0);

			g_object_unref (G_OBJECT (deprecated_icon));
		}
	}
	return widget_icon;
}
#endif

void
glade_palette_item_refresh (GladePaletteItem *item)
{
	GladePaletteItemPrivate *priv;
	GladeProject            *project;
	GladeSupportMask         support;
	gint                     size;
	gchar                   *warning, *text;
	
	priv = GLADE_PALETTE_ITEM_GET_PRIVATE (item);
	
	size = priv->use_small_icon ? GTK_ICON_SIZE_MENU : GTK_ICON_SIZE_BUTTON;

	if ((project = glade_app_check_get_project ()) &&
	    (warning = 
	     glade_project_verify_widget_adaptor (project, priv->adaptor, &support)) != NULL)
	{
		/* set sensitivity */
		gtk_widget_set_sensitive (GTK_WIDGET (item), 
					  !(support & (GLADE_SUPPORT_LIBGLADE_UNSUPPORTED | 
						       GLADE_SUPPORT_LIBGLADE_ONLY |
						       GLADE_SUPPORT_MISMATCH)));

		if (support & GLADE_SUPPORT_DEPRECATED)
			/* XXX Todo, draw a cross overlaying the widget icon */
			gtk_image_set_from_stock (GTK_IMAGE (priv->icon), 
						  GTK_STOCK_DIALOG_WARNING, 
						  size);
		else
			gtk_image_set_from_icon_name (GTK_IMAGE (priv->icon), 
						      priv->adaptor->icon_name, 
						      size);
		/* prepend widget title */
		text = g_strdup_printf ("%s: %s", priv->adaptor->title, warning);
		gtk_widget_set_tooltip_text (priv->icon, text);
		g_free (text);
		g_free (warning);
	} 
	else 
	{
		gtk_widget_set_tooltip_text (GTK_WIDGET (item), priv->adaptor->title);
		gtk_widget_set_sensitive (GTK_WIDGET (item), TRUE);
		gtk_image_set_from_icon_name (GTK_IMAGE (priv->icon), 
					      priv->adaptor->icon_name, 
					      size);
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

		glade_palette_item_refresh (item);

		g_object_notify (G_OBJECT (item), "use-small-icon");
	}
}

static void
glade_palette_set_adaptor (GladePaletteItem *item, GladeWidgetAdaptor *adaptor)
{
	GladePaletteItemPrivate *priv;
	
	priv = GLADE_PALETTE_ITEM_GET_PRIVATE (item);

	priv->adaptor = adaptor;
	
	gtk_label_set_text (GTK_LABEL (priv->label), adaptor->title);

	glade_palette_item_refresh (item);
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
		case PROP_ADAPTOR:
			glade_palette_set_adaptor (item, g_value_get_object (value));
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
		case PROP_ADAPTOR:
			g_value_set_pointer (value, (gpointer) priv->adaptor);
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

	item = GLADE_PALETTE_ITEM (object);
	priv = GLADE_PALETTE_ITEM_GET_PRIVATE (item);
/*
	if (priv->alignment)
	{
		g_object_unref (priv->alignment);
		priv->alignment = NULL;
	}
	if (priv->hbox)
	{
		g_object_unref (priv->hbox);	
		priv->hbox = NULL;
	}
	if (priv->icon)
	{
		g_object_unref (priv->icon);
		priv->icon = NULL;
	}
	if (priv->label)
	{
		g_object_unref (priv->label);
		priv->label = NULL;
	}
*/	
	G_OBJECT_CLASS (glade_palette_item_parent_class)->dispose (object);
}

static gboolean
glade_palette_item_button_press (GtkWidget      *widget,
				 GdkEventButton *event)
{
	if (event->type == GDK_BUTTON_PRESS && event->button == 3)
	{
		GladePaletteItemPrivate *priv = GLADE_PALETTE_ITEM_GET_PRIVATE (widget);

		glade_popup_palette_pop (priv->adaptor, event);
	}

	return GTK_WIDGET_CLASS (glade_palette_item_parent_class)->button_press_event (widget, event);
}

static void
glade_palette_item_class_init (GladePaletteItemClass *klass)
{
	GObjectClass   *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	object_class->get_property = glade_palette_item_get_property;
	object_class->set_property = glade_palette_item_set_property;
	object_class->dispose      = glade_palette_item_dispose;

	widget_class->button_press_event = glade_palette_item_button_press;

	g_object_class_install_property (object_class,
					 PROP_ADAPTOR,
					 g_param_spec_object  ("adaptor",
							        "Adaptor",
							        "The widget adaptor associated with this item",
							       GLADE_TYPE_WIDGET_ADAPTOR,
							       G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE ));

	g_object_class_install_property (object_class,
					 PROP_APPEARANCE,
					 g_param_spec_enum  ("appearance",
							     "Appearance",
							     "The appearance of the item",
							     GLADE_TYPE_ITEM_APPEARANCE,
							     GLADE_ITEM_ICON_ONLY,
							     G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_APPEARANCE,
					 g_param_spec_boolean ("use-small-icon",
							       "Use Small Icon",
							       "Whether to use small icon",
							       FALSE,
							       G_PARAM_READWRITE));

	g_type_class_add_private (klass, sizeof (GladePaletteItemPrivate));
}


static void
glade_palette_item_init (GladePaletteItem *item)
{
	GladePaletteItemPrivate *priv;

	priv = item->priv = GLADE_PALETTE_ITEM_GET_PRIVATE (item);
	
	priv->label = NULL;
	priv->adaptor = NULL;
	priv->use_small_icon = FALSE;
	priv->appearance =  0;

	priv->alignment = gtk_alignment_new (0.0, 0.5, 0.5, 0.5);
	g_object_ref_sink (priv->alignment);
	gtk_widget_show (GTK_WIDGET (priv->alignment));

	priv->hbox = gtk_hbox_new (FALSE, 6);
	g_object_ref_sink (priv->hbox);
	gtk_widget_show (GTK_WIDGET (priv->hbox));

	priv->icon = gtk_image_new ();
	g_object_ref_sink (priv->icon);
	gtk_widget_show (GTK_WIDGET (priv->icon));

	priv->label = gtk_label_new (NULL);
	g_object_ref_sink (priv->label);
	gtk_widget_show (GTK_WIDGET (priv->label));

	gtk_container_add (GTK_CONTAINER (priv->alignment), priv->hbox);
	
	gtk_button_set_relief (GTK_BUTTON (item), GTK_RELIEF_NONE);
	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (item), FALSE);
}

/**
 * glade_palette_item_new:
 * @adaptor: A #GladeWidgetAdaptor
 * @group: The group to add this item to.
 * @appearance: The appearance of the item
 *
 * Returns: A #GtkWidget
 */
GtkWidget*
glade_palette_item_new (GladeWidgetAdaptor *adaptor)
{
	GladePaletteItem        *item;

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

	item = g_object_new (GLADE_TYPE_PALETTE_ITEM,
			     "adaptor", adaptor,
			     "appearance", GLADE_ITEM_ICON_ONLY,
			     NULL);

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
 * glade_palette_item_get_adaptor:
 * @palette: A #GladePaletteItem
 *
 * Returns: the #GladeWidgetClass associated with this item.
 */
GladeWidgetAdaptor *
glade_palette_item_get_adaptor (GladePaletteItem *item)
{
	GladePaletteItemPrivate *priv;
	g_return_val_if_fail (GLADE_IS_PALETTE_ITEM (item), NULL);	
	priv = GLADE_PALETTE_ITEM_GET_PRIVATE (item);

	return priv->adaptor;
}
