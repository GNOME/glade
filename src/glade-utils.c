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
