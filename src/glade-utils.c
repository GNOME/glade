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


#include <gtk/gtktooltips.h>
#include "glade.h"
#include <gmodule.h>
#include "glade-project-window.h"


gboolean
glade_util_path_is_writable (const gchar *full_path)
{
	glade_implement_me ();

	return TRUE;
}

void
glade_util_widget_set_tooltip (GtkWidget *widget, const gchar *str)
{
	GtkTooltips *tooltip;

	tooltip = gtk_tooltips_new ();
	gtk_tooltips_set_tip (tooltip, widget, str, NULL);

	return;
}

GType
glade_util_get_type_from_name (const gchar *name)
{
	static GModule *allsymbols;
	guint (*get_type) ();
	GType type;
	
	if (!allsymbols)
		allsymbols = g_module_open (NULL, 0);
	
	if (!g_module_symbol (allsymbols, name,
			      (gpointer) &get_type)) {
		g_warning (_("We could not find the symbol \"%s\""),
			   name);
		return FALSE;
	}

	g_assert (get_type);
	type = get_type ();

	if (type == 0) {
		g_warning(_("Could not get the type from \"%s"),
			  name);
		return FALSE;
	}

/* Disabled for GtkAdjustment, but i'd like to check for this somehow. Chema */
#if 0	
	if (!g_type_is_a (type, gtk_widget_get_type ())) {
		g_warning (_("The loaded type is not a GtkWidget, while trying to load \"%s\""),
			   class->name);
		return FALSE;
	}
#endif

	return type;
}

void
glade_util_ui_warn (const gchar *warning)
{
	GladeProjectWindow *gpw;
	GtkWidget *dialog;

	gpw = glade_project_window_get ();
	dialog = gtk_message_dialog_new (gpw->window, 
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_OK,
					 warning);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}
