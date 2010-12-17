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

/**
 * SECTION:glade-palette
 * @Short_Description: A widget to select a #GladeWidgetClass for addition.
 *
 * #GladePalette is responsible for displaying the list of available
 * #GladeWidgetClass types and publishing the currently selected class
 * to the Glade core.
 */

#include "glade.h"
#include "glade-app.h"
#include "glade-palette.h"
#include "glade-catalog.h"
#include "glade-project.h"
#include "glade-widget.h"
#include "glade-widget-adaptor.h"
#include "glade-popup.h"

#include <glib/gi18n-lib.h>
#include <gdk/gdk.h>

#define GLADE_PALETTE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object),\
					  GLADE_TYPE_PALETTE,                   \
					  GladePalettePrivate))

struct _GladePalettePrivate
{
	const GList  *catalogs;        /* List of widget catalogs */

	GtkWidget    *selector_hbox;	
	GtkWidget    *selector_button;
	GtkWidget    *create_root_button;

	GtkWidget    *toolpalette;
	GtkWidget    *current_item;

	GladeItemAppearance item_appearance;
	gboolean            use_small_item_icons;
	gboolean            sticky_selection_mode; /* whether sticky_selection mode has been enabled */
};

enum
{
	TOGGLED,
	REFRESH,
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_CURRENT_ITEM,
	PROP_ITEM_APPEARANCE,
	PROP_USE_SMALL_ITEM_ICONS,
	PROP_SHOW_SELECTOR_BUTTON,
	PROP_CATALOGS
};

static guint glade_palette_signals[LAST_SIGNAL] = {0};

static void glade_palette_append_item_group (GladePalette *palette, GladeWidgetGroup *group);

static void glade_palette_update_appearance (GladePalette *palette);

G_DEFINE_TYPE (GladePalette, glade_palette, GTK_TYPE_VBOX)


static void
selector_button_toggled_cb (GtkToggleButton *button, GladePalette *palette)
{
	if (gtk_toggle_button_get_active (button))
	{
		glade_palette_deselect_current_item (palette, FALSE);
	}
	else if (glade_palette_get_current_item (palette) == NULL)
	{
		gtk_toggle_button_set_active (button, TRUE);
	}
}

static void
glade_palette_set_catalogs (GladePalette *palette, GList *catalogs)
{
	GladePalettePrivate *priv;
	GList *l;

	g_return_if_fail (GLADE_IS_PALETTE (palette));
	priv = palette->priv;

	priv->catalogs = catalogs;

	for (l = (GList *) priv->catalogs; l; l = l->next) 
	{
		GList *groups = glade_catalog_get_widget_groups (GLADE_CATALOG (l->data));

		for (; groups; groups = groups->next)
		{
			GladeWidgetGroup *group = GLADE_WIDGET_GROUP (groups->data);

			if (glade_widget_group_get_adaptors (group)) 
				glade_palette_append_item_group (palette, group);
		}
	}
	
}

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
	priv = palette->priv;

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
	priv = palette->priv;

	if (priv->use_small_item_icons != use_small_item_icons)
	{
		priv->use_small_item_icons = use_small_item_icons;

		glade_palette_update_appearance (palette);

		g_object_notify (G_OBJECT (palette), "use-small-item-icons");
		
	}

}

/**
 * glade_palette_set_show_selector_button:
 * @palette: a #GladePalette
 * @show_selector_button: whether to show selector button
 *
 * Sets whether to show the internal widget selector button
 */
void
glade_palette_set_show_selector_button (GladePalette *palette, gboolean show_selector_button)
{
	GladePalettePrivate *priv;
	g_return_if_fail (GLADE_IS_PALETTE (palette));
	priv = palette->priv;

	if (gtk_widget_get_visible (priv->selector_hbox) != show_selector_button)
	{
		if (show_selector_button)
			gtk_widget_show (priv->selector_hbox);
		else
			gtk_widget_hide (priv->selector_hbox);
			
		g_object_notify (G_OBJECT (palette), "show-selector-button");
		
	}

}

/* override GtkWidget::show_all since we have internal widgets we wish to keep
 * hidden unless we decide otherwise, like the hidden selector button.
 */
static void
glade_palette_show_all (GtkWidget *widget)
{
	gtk_widget_show (widget);
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
		case PROP_SHOW_SELECTOR_BUTTON:
			glade_palette_set_show_selector_button (palette, g_value_get_boolean (value));
			break;
		case PROP_CATALOGS:
			glade_palette_set_catalogs (palette, g_value_get_pointer (value));
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
	GladePalettePrivate *priv = palette->priv;

	switch (prop_id)
	{
		case PROP_CURRENT_ITEM:
			if (priv->current_item)
				g_value_set_pointer (value, g_object_get_data
						     (G_OBJECT (priv->current_item), "glade-widget-adaptor"));
			else
				g_value_set_pointer (value, NULL);

			break;
		case PROP_USE_SMALL_ITEM_ICONS:
			g_value_set_boolean (value, priv->use_small_item_icons);
			break;
		case PROP_SHOW_SELECTOR_BUTTON:
			g_value_set_boolean (value, gtk_widget_get_visible (priv->selector_button));
			break;
		case PROP_ITEM_APPEARANCE:
			g_value_set_enum (value, priv->item_appearance);
			break;
		case PROP_CATALOGS:
			g_value_set_pointer (value, (gpointer) priv->catalogs);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;			
	}
}

static void
glade_palette_dispose (GObject *object)
{
	GladePalettePrivate *priv;
  
	priv = GLADE_PALETTE (object)->priv;

	priv->catalogs = NULL;
	
	G_OBJECT_CLASS (glade_palette_parent_class)->dispose (object);
}

static void
glade_palette_finalize (GObject *object)
{
	GladePalettePrivate *priv;
  
	priv = GLADE_PALETTE (object)->priv;

	G_OBJECT_CLASS (glade_palette_parent_class)->finalize (object);
}

static void
glade_palette_toggled (GladePalette *palette)
{
	GladeWidgetAdaptor *adaptor;
	GladeWidget        *widget;

	g_return_if_fail (GLADE_IS_PALETTE (palette));
	adaptor = glade_palette_get_current_item (palette);

	/* class may be NULL if the selector was pressed */
	if (adaptor && GWA_IS_TOPLEVEL (adaptor))
	{
		/* Inappropriate toplevel classes for libglade are
		 * disabled so no chance of creating a non-window toplevel here
		 */
		widget = glade_palette_create_root_widget (palette, adaptor);
		
		/* if this is a top level widget set the accel group */
		if (widget && glade_app_get_accel_group () && GTK_IS_WINDOW (widget->object))
		{
			gtk_window_add_accel_group (GTK_WINDOW (widget->object),
						    glade_app_get_accel_group ());
		}
	}
}

static void
glade_palette_class_init (GladePaletteClass *klass)
{
	GObjectClass   *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	klass->toggled = glade_palette_toggled;
	
	object_class->get_property = glade_palette_get_property;
	object_class->set_property = glade_palette_set_property;
	object_class->dispose = glade_palette_dispose;
	object_class->finalize = glade_palette_finalize;
	
	widget_class->show_all = glade_palette_show_all;

	glade_palette_signals[TOGGLED] =
		g_signal_new ("toggled",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladePaletteClass, toggled),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	glade_palette_signals[REFRESH] =
		g_signal_new ("refresh",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladePaletteClass, refresh),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	g_object_class_install_property (object_class,
					 PROP_ITEM_APPEARANCE,
					 g_param_spec_enum ("item-appearance",
							     "Item Appearance",
							     "The appearance of the palette items",
							     GLADE_TYPE_ITEM_APPEARANCE,
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
					 PROP_ITEM_APPEARANCE,
					 g_param_spec_boolean ("show-selector-button",
							       "Show Selector Button",
							       "Whether to show the internal selector button",
							       TRUE,
							       G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_CURRENT_ITEM,
					 g_param_spec_pointer  ("current-item",
							        "Current Item Class",
							        "The GladeWidgetAdaptor of the currently selected item",
							        G_PARAM_READABLE));
							        
	g_object_class_install_property (object_class,
					 PROP_CATALOGS,
					 g_param_spec_pointer  ("catalogs",
							        "Widget catalogs",
							        "The widget catalogs for the palette",
							        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

	g_type_class_add_private (object_class, sizeof (GladePalettePrivate));
}

GladeWidget *
glade_palette_create_root_widget (GladePalette *palette, GladeWidgetAdaptor *adaptor)
{
	GladeWidget *widget;
	
	/* Dont deselect palette if create is canceled by user in query dialog */
	if ((widget = glade_command_create (adaptor, NULL, NULL, glade_app_get_project ())) != NULL)
		glade_palette_deselect_current_item (palette, FALSE);
	
	return widget;
}

static void
glade_palette_on_button_toggled (GtkWidget *button, GladePalette *palette)
{
	GladePalettePrivate *priv;
	GdkModifierType mask;
	GladeWidgetAdaptor *adaptor;
	gboolean is_root_active;
	
	g_return_if_fail (GLADE_IS_PALETTE (palette));
	g_return_if_fail (GTK_IS_TOGGLE_TOOL_BUTTON (button) ||
			  GTK_IS_TOGGLE_BUTTON (button));
	priv = palette->priv;

	is_root_active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->create_root_button));
	
	if (button == priv->create_root_button && priv->current_item && is_root_active)
	{
		adaptor = glade_palette_get_current_item (palette);
		glade_palette_create_root_widget (palette, adaptor);
		return;
	}

	g_return_if_fail (GTK_IS_TOGGLE_TOOL_BUTTON (button));

	adaptor = g_object_get_data (G_OBJECT (button), "glade-widget-adaptor");
	if (!adaptor) return;

	/* if we are toggling currently active item into non-active state */
	if (priv->current_item == button)
	{
		priv->current_item = NULL;
		g_object_notify (G_OBJECT (palette), "current-item");

		glade_app_set_pointer_mode (GLADE_POINTER_SELECT);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->selector_button), TRUE);	

		priv->sticky_selection_mode = FALSE;
		
		g_signal_emit (G_OBJECT (palette), glade_palette_signals[TOGGLED], 0);

		return;
	}

	/* now we are interested only in buttons which toggle from inactive to active */
	if (!gtk_toggle_tool_button_get_active (GTK_TOGGLE_TOOL_BUTTON (button)))
		return;
	
	if (priv->current_item && (button != priv->current_item))
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (priv->current_item), FALSE);
		
	priv->current_item = button;
	
	if (is_root_active)
	{
		glade_palette_create_root_widget (palette, adaptor);
		return;
	}
	
	g_object_notify (G_OBJECT (palette), "current-item");

	glade_app_set_pointer_mode (GLADE_POINTER_ADD_WIDGET);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->selector_button), FALSE);	
	
	/* check whether to enable sticky selection */
	gdk_window_get_pointer (gtk_widget_get_window (button), NULL, NULL, &mask);
  	priv->sticky_selection_mode = (!GWA_IS_TOPLEVEL (adaptor)) && (mask & GDK_CONTROL_MASK);

	g_signal_emit (G_OBJECT (palette), glade_palette_signals[TOGGLED], 0);
}


static void
glade_palette_item_refresh (GtkWidget *item)
{
	GladeProject            *project;
	GladeSupportMask         support;
	GladeWidgetAdaptor      *adaptor;
	gchar                   *warning, *text;

	adaptor = g_object_get_data (G_OBJECT (item), "glade-widget-adaptor");
	g_assert (adaptor);
	
	if ((project = glade_app_check_get_project ()) &&
	    (warning = 
	     glade_project_verify_widget_adaptor (project, adaptor, &support)) != NULL)
	{
		/* set sensitivity */
		gtk_widget_set_sensitive (GTK_WIDGET (item), 
					  !(support & (GLADE_SUPPORT_LIBGLADE_UNSUPPORTED | 
						       GLADE_SUPPORT_LIBGLADE_ONLY |
						       GLADE_SUPPORT_MISMATCH)));

		if (support & GLADE_SUPPORT_DEPRECATED)
			/* XXX Todo, draw a cross overlaying the widget icon */
			gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON (item), GTK_STOCK_DIALOG_WARNING);
		else
			gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), adaptor->icon_name);

		/* prepend widget title */
		text = g_strdup_printf ("%s: %s", adaptor->title, warning);
		gtk_widget_set_tooltip_text (item, text);
		g_free (text);
		g_free (warning);
	} 
	else 
	{
		gtk_widget_set_tooltip_text (GTK_WIDGET (item), adaptor->title);
		gtk_widget_set_sensitive (GTK_WIDGET (item), TRUE);
		gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (item), adaptor->icon_name);
	}
}


static gint
glade_palette_item_button_press (GtkWidget      *button,
				  GdkEventButton *event,
				  GladePalette   *item)
{
	if (glade_popup_is_popup_event (event))
	{
		GladeWidgetAdaptor *adaptor;

		adaptor = g_object_get_data (G_OBJECT (item), "glade-widget-adaptor");
		
		glade_popup_palette_pop (adaptor, event);
		return TRUE;
	}

	return FALSE;
}

static GtkWidget*
glade_palette_new_item (GladePalette *palette, GladeWidgetAdaptor *adaptor)
{
	GtkWidget *item, *button, *label;
#if GTK_CHECK_VERSION (2, 24, 0)
	GtkWidget *box;
#endif

	item = (GtkWidget *)gtk_toggle_tool_button_new ();
	g_object_set_data (G_OBJECT (item), "glade-widget-adaptor", adaptor);

	button = gtk_bin_get_child (GTK_BIN (item));
	g_assert (GTK_IS_BUTTON (button));
	
	/* Add a box to avoid the ellipsize on the items
	 * (old versions expect a label as the label widget, too bad for them) 
	 */
#if GTK_CHECK_VERSION (2, 24, 0)
	box = gtk_hbox_new (FALSE, 0);
	label = gtk_label_new (adaptor->title);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_widget_show (box);
	gtk_container_add (GTK_CONTAINER (box), label);
	gtk_tool_button_set_label_widget (GTK_TOOL_BUTTON (item), box);
#else
	label = gtk_label_new (adaptor->title);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_tool_button_set_label_widget (GTK_TOOL_BUTTON (item), label);
#endif

	glade_palette_item_refresh (item);

	/* Update selection when the item is pushed */
	g_signal_connect (G_OBJECT (item), "toggled",
			  G_CALLBACK (glade_palette_on_button_toggled), palette);

	/* Update palette item when active project state changes */
	g_signal_connect_swapped (G_OBJECT (palette), "refresh",
				  G_CALLBACK (glade_palette_item_refresh), item);

	/* Fire Glade palette popup menus */
	g_signal_connect (G_OBJECT (button), "button-press-event",
			  G_CALLBACK (glade_palette_item_button_press), item);

	gtk_widget_show (item);

	return item;
}

static GtkWidget*
glade_palette_new_item_group (GladePalette *palette, GladeWidgetGroup *group)
{
	GladePalettePrivate *priv;
	GtkWidget           *item_group, *item, *label;
	GList               *l;

	priv = palette->priv;

	/* Give the item group a left aligned label */
	label = gtk_label_new (glade_widget_group_get_title (group));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);

	item_group = gtk_tool_item_group_new ("");
	gtk_tool_item_group_set_label_widget (GTK_TOOL_ITEM_GROUP (item_group),
	                                      label);

	/* Tell the item group to ellipsize our custom label for us */
	gtk_tool_item_group_set_ellipsize (GTK_TOOL_ITEM_GROUP (item_group), 
					   PANGO_ELLIPSIZE_END);

	gtk_widget_set_tooltip_text (item_group, glade_widget_group_get_title (group));

	/* Go through all the widget classes in this catalog. */
	for (l = (GList *) glade_widget_group_get_adaptors (group); l; l = l->next)
	{
		GladeWidgetAdaptor *adaptor =  GLADE_WIDGET_ADAPTOR (l->data);

		/* Create/append new item */
		item = glade_palette_new_item (palette, adaptor);
		gtk_tool_item_group_insert (GTK_TOOL_ITEM_GROUP (item_group), 
					    GTK_TOOL_ITEM (item), -1);
	}

	/* set default expanded state */
	gtk_tool_item_group_set_collapsed (GTK_TOOL_ITEM_GROUP (item_group),
					   glade_widget_group_get_expanded (group) == FALSE);

	gtk_widget_show (item_group);

	return item_group;
}

static void
glade_palette_append_item_group (GladePalette     *palette, 
				 GladeWidgetGroup *group)
{
	GladePalettePrivate *priv = palette->priv;
 	GtkWidget           *item_group;

	if ((item_group = glade_palette_new_item_group (palette, group)) != NULL)
		gtk_container_add (GTK_CONTAINER (priv->toolpalette), item_group);
}

static void
glade_palette_update_appearance (GladePalette *palette)
{
	GladePalettePrivate *priv;
	GtkToolbarStyle      style;
	GtkIconSize          size;

	priv = palette->priv;

	size = priv->use_small_item_icons ? GTK_ICON_SIZE_MENU : GTK_ICON_SIZE_BUTTON;

	switch (priv->item_appearance)
       	{
	case GLADE_ITEM_ICON_AND_LABEL: style = GTK_TOOLBAR_BOTH_HORIZ; break;
	case GLADE_ITEM_ICON_ONLY:      style = GTK_TOOLBAR_ICONS;      break;
	case GLADE_ITEM_LABEL_ONLY:     style = GTK_TOOLBAR_TEXT;       break;
	default:
		g_assert_not_reached ();
		break;
	}

	gtk_tool_palette_set_icon_size (GTK_TOOL_PALETTE (priv->toolpalette), size);
	gtk_tool_palette_set_style (GTK_TOOL_PALETTE (priv->toolpalette), style);
}

static GtkWidget*
glade_palette_create_selector_button (GladePalette *palette)
{
	GtkWidget *selector;
	GtkWidget *image;
	gchar *path;

	/* create selector button */
	selector = gtk_toggle_button_new ();
	
	gtk_container_set_border_width (GTK_CONTAINER (selector), 0);

	path = g_build_filename (glade_app_get_pixmaps_dir (), "selector.png", NULL);
	image = gtk_image_new_from_file (path);
	gtk_widget_show (image);
	
	gtk_container_add (GTK_CONTAINER (selector), image);
	gtk_button_set_relief (GTK_BUTTON (selector), GTK_RELIEF_NONE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (selector), TRUE);

	g_signal_connect (G_OBJECT (selector), "toggled",
			  G_CALLBACK (selector_button_toggled_cb), 
			  palette);

	g_free (path);

	return selector;
}

static GtkWidget* 
glade_palette_create_create_root_button (GladePalette *palette)
{	
	GtkWidget *create_root_button;
	
	create_root_button = gtk_toggle_button_new ();
	
	gtk_container_set_border_width (GTK_CONTAINER (create_root_button), 0);
	gtk_button_set_use_stock (GTK_BUTTON (create_root_button), TRUE);
	gtk_button_set_label (GTK_BUTTON (create_root_button), "gtk-add");
	
	g_signal_connect (G_OBJECT (create_root_button), "toggled",
			  G_CALLBACK (glade_palette_on_button_toggled), 
			  palette);
	
	return create_root_button;
}

static void
glade_palette_init (GladePalette *palette)
{
	GladePalettePrivate *priv;
	GtkWidget *sw;

	priv = palette->priv = GLADE_PALETTE_GET_PRIVATE (palette);

	priv->catalogs = NULL;
	priv->current_item = NULL;
	priv->item_appearance = GLADE_ITEM_ICON_ONLY;
	priv->use_small_item_icons = FALSE;
	priv->sticky_selection_mode = FALSE;

	/* create selector button */
	priv->selector_button = glade_palette_create_selector_button (palette);
	priv->selector_hbox = gtk_hbox_new (FALSE, 0);
	priv->create_root_button = glade_palette_create_create_root_button (palette);
	gtk_box_pack_start (GTK_BOX (priv->selector_hbox), priv->selector_button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (priv->selector_hbox), priv->create_root_button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (palette), priv->selector_hbox, FALSE, FALSE, 0);
	gtk_widget_show (priv->selector_button);
	gtk_widget_show (priv->create_root_button);
	gtk_widget_show (priv->selector_hbox);

	gtk_widget_set_tooltip_text (priv->selector_button, _("Widget selector"));
	gtk_widget_set_tooltip_text (priv->create_root_button, _("Create root widget"));

	/* The GtkToolPalette */
	priv->toolpalette = gtk_tool_palette_new ();
	
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
					     GTK_SHADOW_NONE);

	gtk_container_add (GTK_CONTAINER (sw), priv->toolpalette);
	gtk_box_pack_start (GTK_BOX (palette), sw, TRUE, TRUE, 0);

	gtk_widget_show (sw);
	gtk_widget_show (priv->toolpalette);

	glade_palette_update_appearance (palette);
	
	gtk_widget_set_no_show_all (GTK_WIDGET (palette), TRUE);
}

/**
 * glade_palette_get_current_item:
 * @palette: a #GladePalette
 *
 * Gets the #GladeWidgetAdaptor of the currently selected item.
 *
 * Returns: the #GladeWidgetAdaptor of currently selected item, or NULL
 *          if no item is selected.
 */
GladeWidgetAdaptor *
glade_palette_get_current_item (GladePalette *palette)
{
	g_return_val_if_fail (GLADE_IS_PALETTE (palette), NULL);

	if (palette->priv->current_item)
		return g_object_get_data (G_OBJECT (palette->priv->current_item), "glade-widget-adaptor");

	return NULL;
}

/**
 * glade_palette_new:
 * @catalogs: the widget catalogs for the palette.
 *
 * Creates a new #GladePalette widget
 *
 * Returns: a new #GladePalette
 */
GtkWidget*
glade_palette_new (const GList *catalogs)
{
	GladePalette *palette;

	g_return_val_if_fail (catalogs != NULL, NULL);

	palette = g_object_new (GLADE_TYPE_PALETTE,
				"spacing", 2, 
				"item-appearance", GLADE_ITEM_ICON_ONLY,
				"catalogs", catalogs,
				NULL);

	return GTK_WIDGET (palette);
}

/**
 * glade_palette_deselect_current_item:
 * @palette: a #GladePalette
 * @sticky_aware: whether to consider sticky selection mode
 *
 * Deselects the currently selected item
 */
void
glade_palette_deselect_current_item (GladePalette *palette, gboolean sticky_aware)
{
	g_return_if_fail (GLADE_IS_PALETTE (palette));

	if (sticky_aware && palette->priv->sticky_selection_mode)
		return;

	if (palette->priv->current_item)
	{
		gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (palette->priv->current_item), FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (palette->priv->selector_button), TRUE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (palette->priv->create_root_button), FALSE);
	
		palette->priv->current_item = NULL;
		g_object_notify (G_OBJECT (palette), "current-item");

		glade_app_set_pointer_mode (GLADE_POINTER_SELECT);

		g_signal_emit (G_OBJECT (palette), glade_palette_signals[TOGGLED], 0);

	}
	
}

/**
 * glade_palette_get_item_appearance:
 * @palette: a #GladePalette
 *
 * Returns: The appearance of the palette items
 */
GladeItemAppearance
glade_palette_get_item_appearance (GladePalette *palette)
{;
	g_return_val_if_fail (GLADE_IS_PALETTE (palette), GLADE_ITEM_ICON_ONLY);

	return palette->priv->item_appearance;
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
	g_return_val_if_fail (GLADE_IS_PALETTE (palette), FALSE);

	return palette->priv->use_small_item_icons;
}

/**
 * glade_palette_get_show_selector_button:
 * @palette: a #GladePalette
 *
 * Returns: Whether the selector button is visible
 */
gboolean
glade_palette_get_show_selector_button (GladePalette *palette)
{
	g_return_val_if_fail (GLADE_IS_PALETTE (palette), FALSE);

	return gtk_widget_get_visible (palette->priv->selector_hbox);
}

/**
 * glade_palette_refresh:
 * @palette: a #GladePalette
 *
 * Refreshes project dependant states of palette buttons
 */
void
glade_palette_refresh (GladePalette *palette)
{
	g_return_if_fail (GLADE_IS_PALETTE (palette));

	g_signal_emit (G_OBJECT (palette), glade_palette_signals[REFRESH], 0);
}

