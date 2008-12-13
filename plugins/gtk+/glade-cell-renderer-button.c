/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Tristan Van Berkom.
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
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#include <config.h>
#include <gladeui/glade.h>
#include <glib/gi18n-lib.h>

#include "glade-cell-renderer-button.h"
#include "glade-text-button.h"


#define GLADE_CELL_RENDERER_BUTTON_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GLADE_TYPE_CELL_RENDERER_BUTTON, GladeCellRendererButtonPrivate))

typedef struct
{
	gboolean entry_editable;
} GladeCellRendererButtonPrivate;

static void glade_cell_renderer_button_finalize   (GObject                  *object);

static void glade_cell_renderer_button_get_property (GObject      *object,
						 guint         prop_id,
						 GValue       *value,
						 GParamSpec   *spec);
static void glade_cell_renderer_button_set_property (GObject      *object,
						 guint         prop_id,
						 const GValue *value,
						 GParamSpec   *spec);

static GtkCellEditable * glade_cell_renderer_button_start_editing (GtkCellRenderer     *cell,
								   GdkEvent            *event,
								   GtkWidget           *widget,
								   const gchar         *path,
								   GdkRectangle        *background_area,
								   GdkRectangle        *cell_area,
								   GtkCellRendererState flags);
enum {
  PROP_0,
  PROP_ENTRY_EDITABLE
};


enum {
  CLICKED,
  LAST_SIGNAL
};

static guint glade_cell_renderer_signals [LAST_SIGNAL];

G_DEFINE_TYPE (GladeCellRendererButton, glade_cell_renderer_button, GTK_TYPE_CELL_RENDERER_TEXT)

#define GLADE_CELL_RENDERER_BUTTON_PATH "glade-cell-renderer-button-path"

static void
glade_cell_renderer_button_class_init (GladeCellRendererButtonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);
	
	object_class->finalize     = glade_cell_renderer_button_finalize;
	object_class->get_property = glade_cell_renderer_button_get_property;
	object_class->set_property = glade_cell_renderer_button_set_property;
	
	cell_class->start_editing  = glade_cell_renderer_button_start_editing;
	
	g_object_class_install_property (object_class,
					 PROP_ENTRY_EDITABLE,
					 g_param_spec_boolean ("entry-editable",
							      _("Entry Editable"),
							      _("Whether the entry is editable"),
							      TRUE,
							      G_PARAM_READWRITE));  

	glade_cell_renderer_signals[CLICKED] = 
		g_signal_new ("clicked",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeCellRendererButtonClass, clicked),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	
	g_type_class_add_private (object_class, sizeof (GladeCellRendererButtonPrivate));
}

static void
glade_cell_renderer_button_init (GladeCellRendererButton *self)
{
	GladeCellRendererButtonPrivate *priv;
	
	priv = GLADE_CELL_RENDERER_BUTTON_GET_PRIVATE (self);
	
	priv->entry_editable = TRUE;
}

static void
glade_cell_renderer_button_finalize (GObject *object)
{
	GladeCellRendererButtonPrivate *priv;
	
	priv = GLADE_CELL_RENDERER_BUTTON_GET_PRIVATE (object);
	
	G_OBJECT_CLASS (glade_cell_renderer_button_parent_class)->finalize (object);
}

static void
glade_cell_renderer_button_get_property (GObject      *object,
					 guint         prop_id,
					 GValue       *value,
					 GParamSpec   *pspec)
{
	GladeCellRendererButton *renderer;
	GladeCellRendererButtonPrivate *priv;

	renderer = GLADE_CELL_RENDERER_BUTTON (object);
	priv = GLADE_CELL_RENDERER_BUTTON_GET_PRIVATE (renderer);

	switch (prop_id)
	{
	case PROP_ENTRY_EDITABLE:
		g_value_set_boolean (value, priv->entry_editable);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_cell_renderer_button_set_property (GObject      *object,
					 guint         prop_id,
					 const GValue *value,
					 GParamSpec   *pspec)
{
	GladeCellRendererButton *renderer;
	GladeCellRendererButtonPrivate *priv;
	
	renderer = GLADE_CELL_RENDERER_BUTTON (object);
	priv = GLADE_CELL_RENDERER_BUTTON_GET_PRIVATE (renderer);
	
	switch (prop_id)
	{
	case PROP_ENTRY_EDITABLE:
		priv->entry_editable = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_cell_renderer_button_clicked (GtkButton *button,
				    GladeCellRendererButton *self)
{
	gchar *path = g_object_get_data (G_OBJECT (button), GLADE_CELL_RENDERER_BUTTON_PATH);

	g_signal_emit (self, glade_cell_renderer_signals[CLICKED], 0, path);
}

static gboolean
glade_cell_renderer_button_focus_out_event (GtkWidget *entry,
					    GdkEvent  *event,
					    GtkCellRendererText *cell_text)
{
	GladeCellRendererButtonPrivate *priv;

	priv = GLADE_CELL_RENDERER_BUTTON_GET_PRIVATE (cell_text);

	GTK_ENTRY (entry)->editing_canceled = TRUE;
	gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (entry));
	gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (entry));
	
	/* entry needs focus-out-event */
	return FALSE;
}


static void
glade_cell_renderer_button_activate (GtkCellEditable *entry,
				     GtkCellRendererText *cell_text)
{
	const gchar *path;
	const gchar *new_text;
	GladeCellRendererButtonPrivate *priv;

	priv = GLADE_CELL_RENDERER_BUTTON_GET_PRIVATE (cell_text);

	g_signal_handlers_disconnect_by_func (entry, glade_cell_renderer_button_focus_out_event, cell_text);

	gtk_cell_renderer_stop_editing (GTK_CELL_RENDERER (cell_text), 
					GTK_ENTRY (entry)->editing_canceled);

	if (GTK_ENTRY (entry)->editing_canceled)
		return;

	path = g_object_get_data (G_OBJECT (entry), GLADE_CELL_RENDERER_BUTTON_PATH);
	new_text = gtk_entry_get_text (GTK_ENTRY (entry));

	g_signal_emit_by_name (cell_text, "edited", path, new_text);
}

static void
glade_cell_renderer_button_editing_done (GtkCellEditable     *entry,
					 GtkCellRendererText *cell_text)
{
	const gchar *path;
	const gchar *new_text;
	GladeCellRendererButtonPrivate *priv;

	priv = GLADE_CELL_RENDERER_BUTTON_GET_PRIVATE (cell_text);

	g_signal_handlers_disconnect_by_func (entry, glade_cell_renderer_button_focus_out_event, cell_text);

	gtk_cell_renderer_stop_editing (GTK_CELL_RENDERER (cell_text), 
					GTK_ENTRY (entry)->editing_canceled);

	if (GTK_ENTRY (entry)->editing_canceled)
		return;

	path = g_object_get_data (G_OBJECT (entry), GLADE_CELL_RENDERER_BUTTON_PATH);
	new_text = gtk_entry_get_text (GTK_ENTRY (entry));

	g_signal_emit_by_name (cell_text, "edited", path, new_text);
}


static GtkCellEditable *
glade_cell_renderer_button_start_editing (GtkCellRenderer     *cell,
					  GdkEvent            *event,
					  GtkWidget           *widget,
					  const gchar         *path,
					  GdkRectangle        *background_area,
					  GdkRectangle        *cell_area,
					  GtkCellRendererState flags)
{
	GladeCellRendererButtonPrivate *priv;
	GtkCellRendererText *cell_text;
	GladeTextButton *text_button;

	cell_text = GTK_CELL_RENDERER_TEXT (cell);
	priv = GLADE_CELL_RENDERER_BUTTON_GET_PRIVATE (cell);

	if (cell_text->editable == FALSE)
		return NULL;

	text_button = (GladeTextButton *)glade_text_button_new ();
	gtk_entry_set_text (GTK_ENTRY (text_button->entry), cell_text->text ? cell_text->text : "");
	gtk_entry_set_editable (GTK_ENTRY (text_button->entry), priv->entry_editable);

	g_object_set (text_button->entry,
		      "has-frame", FALSE,
		      "xalign", cell->xalign,
		      NULL);

	g_object_set_data_full (G_OBJECT (text_button->entry), GLADE_CELL_RENDERER_BUTTON_PATH, 
				g_strdup (path), g_free);
	g_object_set_data_full (G_OBJECT (text_button->button), GLADE_CELL_RENDERER_BUTTON_PATH, 
				g_strdup (path), g_free);

	g_signal_connect (G_OBJECT (text_button->button), "clicked",
			  G_CALLBACK (glade_cell_renderer_button_clicked),
			  cell);

	g_signal_connect (G_OBJECT (text_button->entry), "activate",
			  G_CALLBACK (glade_cell_renderer_button_activate),
			  cell);

	g_signal_connect (text_button->entry,
			  "editing-done",
			  G_CALLBACK (glade_cell_renderer_button_editing_done),
			  cell);

	g_signal_connect_after (text_button->entry, "focus-out-event",
				G_CALLBACK (glade_cell_renderer_button_focus_out_event),
				cell);

	gtk_widget_show_all (GTK_WIDGET (text_button));
	return GTK_CELL_EDITABLE (text_button);
}

/**
 * glade_cell_renderer_button_new:
 *
 * Creates a new #GladeCellRendererButton. 
 *
 * Returns: a new #GladeCellRendererButton
 *
 */
GtkCellRenderer *
glade_cell_renderer_button_new (void)
{
	return g_object_new (GLADE_TYPE_CELL_RENDERER_BUTTON, NULL);
}

