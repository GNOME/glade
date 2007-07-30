/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-clipboard.c - An object for handling Cut/Copy/Paste.
 *
 * Copyright (C) 2001 The GNOME Foundation.
 *
 * Author(s):
 *      Archit Baweja <bighead@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#include "config.h"

/**
 * SECTION:glade-clipboard
 * @Short_Description: A list of #GladeWidget objects not in any #GladeProject.
 *
 * The #GladeClipboard is a singleton and is an accumulative shelf
 * of all cut or copied #GladeWidget in the application. A #GladeWidget
 * can be cut from one #GladeProject and pasted to another.
 */

#include <glib/gi18n-lib.h>
#include "glade.h"
#include "glade-clipboard-view.h"
#include "glade-clipboard.h"
#include "glade-widget.h"
#include "glade-placeholder.h"
#include "glade-project.h"

enum
{
	PROP_0,
	PROP_HAS_SELECTION
};

static void
glade_project_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
	GladeClipboard *clipboard = GLADE_CLIPBOARD (object);

	switch (prop_id)
	{
		case PROP_HAS_SELECTION:
			g_value_set_boolean (value, clipboard->has_selection);
			break;				
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;			
	}
}

static void
glade_clipboard_class_init (GladeClipboardClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = glade_project_get_property;

	g_object_class_install_property (object_class,
					 PROP_HAS_SELECTION,
					 g_param_spec_boolean ("has-selection",
							       "Has Selection",
							       "Whether clipboard has a selection of items to paste",
							       FALSE,
							       G_PARAM_READABLE));
}

static void
glade_clipboard_init (GladeClipboard *clipboard)
{
	clipboard->widgets   = NULL;
	clipboard->view      = NULL;
	clipboard->selection = NULL;
	clipboard->has_selection = FALSE;
}

GType
glade_clipboard_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GladeClipboardClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) glade_clipboard_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (GladeClipboard),
			0,
			(GInstanceInitFunc) glade_clipboard_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "GladeClipboard", &info, 0);
	}

	return type;
}

static void
glade_clipboard_set_has_selection  (GladeClipboard *clipboard, gboolean has_selection)
{
	g_assert (GLADE_IS_CLIPBOARD (clipboard));

	if (clipboard->has_selection != has_selection)
	{	
		clipboard->has_selection = has_selection;
		g_object_notify (G_OBJECT (clipboard), "has-selection");
	}

}

/**
 * glade_clipboard_get_has_selection:
 * @clipboard: a #GladeClipboard
 * 
 * Returns: TRUE if this clipboard has selected items to paste.
 */
gboolean
glade_clipboard_get_has_selection  (GladeClipboard *clipboard)
{
	g_assert (GLADE_IS_CLIPBOARD (clipboard));

	return clipboard->has_selection;
}


/**
 * glade_clipboard_new:
 * 
 * Returns: a new #GladeClipboard object
 */
GladeClipboard *
glade_clipboard_new (void)
{
	return GLADE_CLIPBOARD (g_object_new (GLADE_TYPE_CLIPBOARD, NULL));
}

/**
 * glade_clipboard_add:
 * @clipboard: a #GladeClipboard
 * @widgets: a #GList of #GladeWidgets
 * 
 * Adds @widgets to @clipboard.
 * This increases the reference count of each #GladeWidget in @widgets.
 */
void
glade_clipboard_add (GladeClipboard *clipboard, GList *widgets)
{
	GladeWidget *widget;
	GList       *list;

	/*
	 * Clear selection for the new widgets.
	 */
	glade_clipboard_selection_clear (clipboard);

	/*
	 * Add the widgets to the list of children.
	 */
	for (list = widgets; list && list->data; list = list->next)
	{
		widget             = list->data;
		clipboard->widgets = 
			g_list_prepend (clipboard->widgets, 
					g_object_ref (G_OBJECT (widget)));
		/*
		 * Update view.
		 */
		glade_clipboard_selection_add (clipboard, widget);
		if (clipboard->view)
		{
			glade_clipboard_view_add
				(GLADE_CLIPBOARD_VIEW (clipboard->view), widget);
			glade_clipboard_view_refresh_sel 
				(GLADE_CLIPBOARD_VIEW (clipboard->view));
		}
	}

}

/**
 * glade_clipboard_remove:
 * @clipboard: a #GladeClipboard
 * @widgets: a #GList of #GladeWidgets
 * 
 * Removes @widgets from @clipboard.
 */
void
glade_clipboard_remove (GladeClipboard *clipboard, GList *widgets)
{
	GladeWidget *widget;
	GList       *list;

	for (list = widgets; list && list->data; list = list->next)
	{
		widget               = list->data;

		clipboard->widgets   = 
			g_list_remove (clipboard->widgets, widget);
		glade_clipboard_selection_remove (clipboard, widget);

		/*
		 * If there is a view present, update it.
		 */
		if (clipboard->view)
			glade_clipboard_view_remove
				(GLADE_CLIPBOARD_VIEW (clipboard->view), widget);
		
		g_object_unref (G_OBJECT (widget));
	}

	/* 
	 * Only default selection if nescisary
	 */
	if ((g_list_length (clipboard->selection) < 1) &&
	    (list = g_list_first (clipboard->widgets)) != NULL)
	{
		glade_clipboard_selection_add
			(clipboard, GLADE_WIDGET (list->data));
		glade_clipboard_view_refresh_sel 
			(GLADE_CLIPBOARD_VIEW (clipboard->view));
	}
}

void
glade_clipboard_selection_add (GladeClipboard *clipboard, 
			       GladeWidget    *widget)
{
	g_return_if_fail (GLADE_IS_CLIPBOARD (clipboard));
	g_return_if_fail (GLADE_IS_WIDGET    (widget));
	clipboard->selection =
		g_list_prepend (clipboard->selection, widget);

	glade_clipboard_set_has_selection (clipboard, TRUE);
}

void
glade_clipboard_selection_remove (GladeClipboard *clipboard, 
				  GladeWidget    *widget)
{
	g_return_if_fail (GLADE_IS_CLIPBOARD (clipboard));
	g_return_if_fail (GLADE_IS_WIDGET    (widget));
	clipboard->selection = 
		g_list_remove (clipboard->selection, widget);

	if (g_list_length (clipboard->selection) == 0)
		glade_clipboard_set_has_selection (clipboard, FALSE);
}

void
glade_clipboard_selection_clear (GladeClipboard *clipboard)
{
	g_return_if_fail (GLADE_IS_CLIPBOARD (clipboard));
	clipboard->selection = 
		(g_list_free (clipboard->selection), NULL);

	glade_clipboard_set_has_selection (clipboard, FALSE);
}
