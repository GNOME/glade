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


#include "glade.h"
#include "glade-project.h"
#include "glade-project-ui.h"

#define GLADE_SELECTOR_FILENAME "GladeFileSelectorFilename"

static gint
glade_project_ui_delete_event_cb (GtkWidget *selector, GdkEventAny *event)
{
	gtk_main_quit ();

	gtk_object_set_user_data (GTK_OBJECT (selector), NULL);
	
	return TRUE;
}

static gint
glade_project_ui_selector_clicked (GtkWidget *button, GtkWidget *selector)
{
	g_return_val_if_fail (GTK_IS_FILE_SELECTION (selector), FALSE);

	if (button == GTK_FILE_SELECTION (selector)->ok_button) {
		const gchar *file;
		file = gtk_file_selection_get_filename (GTK_FILE_SELECTION (selector));
		gtk_object_set_user_data (GTK_OBJECT (selector), g_strdup (file));
	} else 
		gtk_object_set_user_data (GTK_OBJECT (selector), NULL);
		
	gtk_main_quit ();

	gtk_widget_hide (selector);
	
	return TRUE;
}

gchar *
glade_project_ui_save_get_name (GladeProject *project)
{
	GtkWidget *selector;

	selector = gtk_file_selection_new (_("Save ..."));

	gtk_window_set_modal (GTK_WINDOW (selector), TRUE);

	gtk_signal_connect (GTK_OBJECT(GTK_FILE_SELECTION(selector)->ok_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (glade_project_ui_selector_clicked),
			    selector);
	gtk_signal_connect (GTK_OBJECT(GTK_FILE_SELECTION(selector)->cancel_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (glade_project_ui_selector_clicked),
			    selector);
	
	gtk_signal_connect (GTK_OBJECT(selector), "delete_event",
			    GTK_SIGNAL_FUNC (glade_project_ui_delete_event_cb),
			    NULL);
	
	gtk_widget_show (selector);

	gtk_main ();

	return gtk_object_get_user_data (GTK_OBJECT (selector));
}


void
glade_project_ui_warn (GladeProject *project, const gchar *warning)
{
	/* This are warnings to the users, use a nice dialog and stuff */

	g_warning (warning);

}
