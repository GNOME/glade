/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Tristan Van Berkom.
 *
 * This library is free software; you can redistribute it and/or it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#include <config.h>
#include <gladeui/glade.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#include "glade-text-button.h"


#define GLADE_CELL_RENDERER_BUTTON_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GLADE_TYPE_CELL_RENDERER_BUTTON, GladeTextButtonPrivate))

typedef struct
{
	gchar *button_text;
} GladeTextButtonPrivate;

static void glade_text_button_finalize           (GObject              *object);

static void glade_text_button_cell_editable_init (GtkCellEditableIface *iface);

static void glade_text_button_grab_focus         (GtkWidget            *widget);


G_DEFINE_TYPE_WITH_CODE (GladeTextButton, glade_text_button, GTK_TYPE_ALIGNMENT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CELL_EDITABLE,
                                                glade_text_button_cell_editable_init));


static void
glade_text_button_class_init (GladeTextButtonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize     = glade_text_button_finalize;
	widget_class->grab_focus   = glade_text_button_grab_focus;

	g_type_class_add_private (object_class, sizeof (GladeTextButtonPrivate));
}

static void
glade_text_button_init (GladeTextButton *self)
{
	GtkWidget *image;

	gtk_alignment_set_padding (GTK_ALIGNMENT (self), 1, 1, 2, 2);

	self->hbox = gtk_hbox_new (FALSE, 2);
  
	gtk_container_add (GTK_CONTAINER (self), self->hbox); 
	
	self->entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (self->hbox), self->entry, TRUE, TRUE, 0);
	
	self->button = gtk_button_new ();
	gtk_box_pack_start (GTK_BOX (self->hbox), self->button, FALSE, FALSE, 0);

	image = gtk_image_new_from_stock (GTK_STOCK_EDIT,
					  GTK_ICON_SIZE_MENU);
	gtk_widget_show (image);
	gtk_container_add (GTK_CONTAINER (self->button), image);
}

static void
glade_text_button_clicked (GtkWidget *widget,
			   GladeTextButton *button)
{
	gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (button));
	gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (button));
}

/* GtkCellEditable method implementations
 */
static void
glade_text_button_entry_activated (GtkEntry *entry, GladeTextButton *button)
{
	gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (button));
	gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (button));
}

static gboolean
glade_text_button_key_press_event (GtkEntry    *entry,
				   GdkEventKey *key_event,
				   GladeTextButton *button)
{
	if (key_event->keyval == GDK_Escape)
	{
		g_object_get (entry,
			      "editing-canceled", TRUE,
			      NULL);
		gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (button));
		gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (button));
		
		return TRUE;
	}

	/* override focus */
	if (key_event->keyval == GDK_Up || key_event->keyval == GDK_Down)
	{
		gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (button));
		gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (button));		
		return TRUE;
	}	
	return FALSE;
}


static void
glade_text_button_start_editing (GtkCellEditable *cell_editable,
				 GdkEvent        *event)
{
	g_signal_connect (GLADE_TEXT_BUTTON (cell_editable)->button, "clicked",
			  G_CALLBACK (glade_text_button_clicked), cell_editable);
	g_signal_connect (GLADE_TEXT_BUTTON (cell_editable)->entry, "activate",
			  G_CALLBACK (glade_text_button_entry_activated), cell_editable);
	g_signal_connect (cell_editable, "key-press-event",
			  G_CALLBACK (glade_text_button_key_press_event), cell_editable);
}

static void
glade_text_button_cell_editable_init (GtkCellEditableIface *iface)
{
	iface->start_editing = glade_text_button_start_editing;
}

static void
glade_text_button_finalize (GObject *object)
{
	G_OBJECT_CLASS (glade_text_button_parent_class)->finalize (object);
}

static void
glade_text_button_grab_focus (GtkWidget *widget)
{
	GladeTextButton *text_button = GLADE_TEXT_BUTTON (widget);

	gtk_widget_grab_focus (text_button->entry);
}

/**
 * glade_text_button_new:
 *
 * Creates a new #GladeTextButton. 
 *
 * Returns: a new #GladeTextButton
 *
 */
GtkWidget *
glade_text_button_new (void)
{
	return g_object_new (GLADE_TYPE_TEXT_BUTTON, NULL);
}
