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

#include <string.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#define PIXMAP_DIR		"C:/Program Files/glade/pixmap"
#include "glade.h"
#include "glade-placeholder.h"
#include "glade-cursor.h"
#include "glade-widget-class.h"
#include "glade-widget.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-project.h"
#include "glade-project-window.h"
#include "glade-packing.h"
#include "glade-clipboard.h"
#include "glade-popup.h"

#define GLADE_PLACEHOLDER_ROW_STRING  "GladePlaceholderRow"
#define GLADE_PLACEHOLDER_COL_STRING  "GladePlaceholderColumn"
#define GLADE_PLACEHOLDER_PARENT_DATA "GladePlaceholderParentData"
#define GLADE_PLACEHOLDER_IS_DATA     "GladeIsPlaceholderData"

#define GLADE_PLACEHOLDER_SELECTION_NODE_SIZE 7

static void
glade_placeholder_replace_box (GtkWidget *current,
			       GtkWidget *new,
			       GtkWidget *container)
{
	/* Some Gtk Hackery. Not beautifull but needed. */
	GtkBoxChild *child_info = NULL;
	GtkWidget *child;
	GtkBox *box;
	GList *list;

	box = GTK_BOX (container);

	list = box->children;
	for (; list != NULL; list = list->next) {
		child_info = list->data;
		if (child_info->widget == current)
			break;
	}
	if (list == NULL) {
		g_warning ("Error while replacing a widget in a GtkBox. The old widget could not be found\n");
		return;
	}
	gtk_widget_unparent (child_info->widget);
	child_info->widget = new;
	gtk_widget_set_parent (child_info->widget, GTK_WIDGET (box));

	/* */
	child = new;
	if (GTK_WIDGET_REALIZED (box))
		gtk_widget_realize (child);
	if (GTK_WIDGET_VISIBLE (box) && GTK_WIDGET_VISIBLE (child)) {
		if (GTK_WIDGET_MAPPED (box))
			gtk_widget_map (child);
		gtk_widget_queue_resize (child);
	}
	/* */
}

static void
glade_placeholder_replace_table (GtkWidget *current,
				 GtkWidget *new,
				 GtkWidget *container)
{
	/* Some Gtk Hackery. Not beautifull, but needed */
	GtkTableChild *table_child = NULL;
	GtkWidget *child;
	GtkTable *table;
	GList *list;

	table = GTK_TABLE (container);
	list = table->children;
		
	for (; list != NULL; list = list->next) {
		table_child = list->data;
		if (table_child->widget == current)
			break;
	}
	if (list == NULL) {
		g_warning ("Error while adding a widget to a GtkTable\n");
		return;
	}
	
	gtk_widget_unparent (table_child->widget);
	table_child->widget = new;
	gtk_widget_set_parent (table_child->widget, GTK_WIDGET (table));
	
	/* */
	child = new;
	if (GTK_WIDGET_REALIZED (child->parent))
		gtk_widget_realize (child);
	if (GTK_WIDGET_VISIBLE (child->parent) && GTK_WIDGET_VISIBLE (child)) {
		if (GTK_WIDGET_MAPPED (child->parent))
			gtk_widget_map (child);
		gtk_widget_queue_resize (child);
	}
	/* */
}

static void
glade_placeholder_replace_dialog (GtkWidget *current,
				  GtkWidget *new,
				  GtkWidget *container)
{

	glade_placeholder_replace_box (current, new, GTK_DIALOG (container)->vbox);
	glade_placeholder_replace_box (current, new, GTK_DIALOG (container)->action_area);
}

static void
glade_placeholder_replace_container (GtkWidget *current,
				     GtkWidget *new,
				     GtkWidget *container)
{
	gtk_container_remove (GTK_CONTAINER (container), current);
	gtk_container_add (GTK_CONTAINER (container), new);
}

/**
 * glade_placeholder_replace_notebook:
 * @current: the current widget that is going to be replaced
 * @new: the new widget that is going to replace
 * @container: the container
 * 
 * Replaces a widget inside a notebook with a new widget.
 **/
static void
glade_placeholder_replace_notebook (GtkWidget *current,
				    GtkWidget *new,
				    GtkWidget *container)
{
	GtkNotebook *notebook;
	GtkWidget *page;
	GtkWidget *label;
	gint page_num;

	notebook = GTK_NOTEBOOK (container);
	page_num = gtk_notebook_page_num (notebook, current);
	if (page_num == -1) {
		g_warning ("GtkNotebookPage not found\n");
		return;
	}

	page = gtk_notebook_get_nth_page (notebook, page_num);
	label = gtk_notebook_get_tab_label (notebook, current);
	
	gtk_widget_ref (page);
	gtk_widget_ref (label);

	gtk_notebook_remove_page (notebook, page_num);
	gtk_notebook_insert_page (notebook, new,
				  label, page_num);
	gtk_notebook_set_tab_label (notebook,
				    new,
				    label);
	
	gtk_widget_unref (label);
	gtk_widget_unref (page);

	gtk_notebook_set_current_page (notebook, page_num);
}

static void
glade_placeholder_replace_widget (GladePlaceholder *placeholder, GladeWidgetClass *class, GladeProject *project)
{
	GladeWidget *parent;
	GladeWidget *widget;

	parent = glade_placeholder_get_parent (placeholder);
	g_return_if_fail (parent != NULL);

	widget = glade_widget_new_from_class (class, parent);
	if (widget == NULL)
		return;

	glade_placeholder_replace (placeholder, parent, widget);

	glade_widget_set_default_packing_options (widget);
	
	glade_project_selection_set (widget, TRUE);
}

void
glade_placeholder_add_methods_to_class (GladeWidgetClass *class)
{
	/* This is ugly, make it better. Chema */
	if ((strcmp (class->name, "GtkVBox") == 0) ||
	    (strcmp (class->name, "GtkHBox") == 0))
		class->placeholder_replace = glade_placeholder_replace_box;
	if (strcmp (class->name, "GtkTable") == 0)
		class->placeholder_replace = glade_placeholder_replace_table;
	if (strcmp (class->name, "GtkNotebook") == 0)
		class->placeholder_replace = glade_placeholder_replace_notebook;
	if (strcmp (class->name, "GtkDialog") == 0)
		class->placeholder_replace = glade_placeholder_replace_dialog;
	if ((strcmp (class->name, "GtkWindow")    == 0) ||
	    (strcmp (class->name, "GtkFrame")     == 0) ||
	    (strcmp (class->name, "GtkHandleBox") == 0 ))
		class->placeholder_replace = glade_placeholder_replace_container;
	
	if (class->placeholder_replace == NULL)
		g_warning ("placeholder_replace has not been implemented for %s\n",
			   class->name);
}

static GdkWindow*
glade_placeholder_get_gdk_window (GladePlaceholder *placeholder,
				  GtkWidget **paint_widget)
{
	GtkWidget *parent = GTK_WIDGET (placeholder)->parent;

	if (parent) {
		*paint_widget = parent;
		return parent->window;
	}

	*paint_widget = GTK_WIDGET (placeholder);
	return GTK_WIDGET (placeholder)->window;
}

static void
glade_placeholder_draw_selection_nodes (GladePlaceholder *placeholder)
{
	GtkWidget *widget, *paint_widget;
	GdkWindow *window;
	GdkGC *gc;
	gint x, y, w, h;
	gint width, height;

	widget = GTK_WIDGET (placeholder);
	window = glade_placeholder_get_gdk_window (placeholder, &paint_widget);

	if (widget->parent) {
		gtk_widget_translate_coordinates (widget, paint_widget,
						  0, 0, &x, &y);
		w = widget->allocation.width;
		h = widget->allocation.height;
	} else {
		x = 0;
		y = 0;
		gdk_window_get_size (window, &w, &h);
	}

	gc = paint_widget->style->black_gc;
	gdk_gc_set_subwindow (gc, GDK_INCLUDE_INFERIORS);

	width = w;
	height = h;
	if (width > GLADE_PLACEHOLDER_SELECTION_NODE_SIZE
	    && height > GLADE_PLACEHOLDER_SELECTION_NODE_SIZE) {
		gdk_draw_rectangle (window, gc, TRUE, x, y,
				    GLADE_PLACEHOLDER_SELECTION_NODE_SIZE,
				    GLADE_PLACEHOLDER_SELECTION_NODE_SIZE);
		gdk_draw_rectangle (window, gc, TRUE, x,
				    y + height - GLADE_PLACEHOLDER_SELECTION_NODE_SIZE,
				    GLADE_PLACEHOLDER_SELECTION_NODE_SIZE,
				    GLADE_PLACEHOLDER_SELECTION_NODE_SIZE);
		gdk_draw_rectangle (window, gc, TRUE,
				    x + width - GLADE_PLACEHOLDER_SELECTION_NODE_SIZE, y,
				    GLADE_PLACEHOLDER_SELECTION_NODE_SIZE,
				    GLADE_PLACEHOLDER_SELECTION_NODE_SIZE);
		gdk_draw_rectangle (window, gc, TRUE,
				    x + width - GLADE_PLACEHOLDER_SELECTION_NODE_SIZE,
				    y + height - GLADE_PLACEHOLDER_SELECTION_NODE_SIZE,
				    GLADE_PLACEHOLDER_SELECTION_NODE_SIZE,
				    GLADE_PLACEHOLDER_SELECTION_NODE_SIZE);
	}

	gdk_draw_rectangle (window, gc, FALSE, x, y, width - 1, height - 1);
	
	gdk_gc_set_subwindow (gc, GDK_CLIP_BY_CHILDREN);
}

static void
glade_placeholder_clear_selection_nodes (GladePlaceholder *placeholder)
{
	g_return_if_fail (GLADE_IS_WIDGET (placeholder));
	
	gdk_window_clear_area (placeholder->window,
			       placeholder->allocation.x,
			       placeholder->allocation.y,
			       placeholder->allocation.width,
			       placeholder->allocation.height);
	
	gtk_widget_queue_draw (GTK_WIDGET (placeholder));
}

static void
glade_placeholder_on_button_press_event (GladePlaceholder *placeholder, GdkEventButton *event, GladeProject *project)
{
	GladeProjectWindow *gpw;

	gpw = glade_project_window_get ();
	
	if (event->button == 1 && event->type == GDK_BUTTON_PRESS) {

		if (gpw->add_class != NULL) {
			/* 
			 * A widget type is selected in the palette.
			 * Add a new widget of that type.
			 */
			glade_placeholder_replace_widget (placeholder, gpw->add_class, project);
			glade_project_window_set_add_class (gpw, NULL);
			gpw->active_placeholder = NULL;
		} else {
			/*
			 * Else set the current placeholder as selected,
			 * so that Paste from the Main Menu works.
			 */
			if (gpw->active_placeholder != NULL)
				glade_placeholder_clear_selection_nodes (gpw->active_placeholder);
			gpw->active_placeholder = placeholder;
			glade_placeholder_draw_selection_nodes (placeholder);
		}
	} else if (event->button == 3) {
		glade_popup_placeholder_pop (placeholder, event);
	}
			
}

static void
glade_placeholder_on_motion_notify_event (GladePlaceholder *placeholder, GdkEventMotion *event, gpointer not_used)
{
	GladeProjectWindow *gpw;

	gpw = glade_project_window_get ();

	if (gpw->add_class == NULL)
		glade_cursor_set (event->window, GLADE_CURSOR_SELECTOR);
	else
		glade_cursor_set (event->window, GLADE_CURSOR_ADD_WIDGET);
}


static void
glade_placeholder_on_expose_event (GladePlaceholder *placeholder,
				   GdkEventExpose *event,
				   gpointer not_used)
{
	GtkWidget *widget;
	GdkGC *light_gc;
	GdkGC *dark_gc;
	gint w, h;

	widget = GTK_WIDGET (placeholder);
	
	light_gc = widget->style->light_gc [GTK_STATE_NORMAL];
	dark_gc  = widget->style->dark_gc  [GTK_STATE_NORMAL];
	gdk_window_get_size (widget->window, &w, &h);

	gdk_draw_line (widget->window, light_gc, 0, 0, w - 1, 0);
	gdk_draw_line (widget->window, light_gc, 0, 0, 0, h - 1);
	gdk_draw_line (widget->window, dark_gc, 0, h - 1, w - 1, h - 1);
	gdk_draw_line (widget->window, dark_gc, w - 1, 0, w - 1, h - 1);
}

static void
glade_placeholder_on_realize (GladePlaceholder *placeholder, gpointer not_used)
{
	static GdkPixmap *pixmap = NULL;

	if (pixmap == NULL) {
		gchar *file;
		file = g_strdup (PIXMAPS_DIR "/placeholder.xpm");
		pixmap = gdk_pixmap_colormap_create_from_xpm (NULL,
							      gtk_widget_get_colormap (GTK_WIDGET (placeholder)),
							      NULL, NULL, file);
		g_free (file);
	}
		
	if (pixmap == NULL) {
		g_warning ("Could not create pixmap for the glade-placeholder");
		return;
	}
	
	gdk_window_set_back_pixmap (GTK_WIDGET (placeholder)->window, pixmap, FALSE);
}

static void
glade_placeholder_connect_mouse_signals (GladePlaceholder *placeholder,
					 GladeProject *project)
{
	gtk_signal_connect (GTK_OBJECT (placeholder), "motion_notify_event",
			    GTK_SIGNAL_FUNC (glade_placeholder_on_motion_notify_event), NULL);
	gtk_signal_connect (GTK_OBJECT (placeholder), "button_press_event",
			    GTK_SIGNAL_FUNC (glade_placeholder_on_button_press_event), project);
}
static void
glade_placeholder_connect_draw_signals (GladePlaceholder *placeholder)
{
	gtk_signal_connect_after (GTK_OBJECT (placeholder), "realize",
				  GTK_SIGNAL_FUNC (glade_placeholder_on_realize), NULL);
	gtk_signal_connect_after (GTK_OBJECT (placeholder), "expose_event",
				  GTK_SIGNAL_FUNC (glade_placeholder_on_expose_event), NULL);
}

static void
glade_placeholder_on_destroy (GladePlaceholder *widget, gpointer not_used)
{
}

#define GLADE_PLACEHOLDER_SIZE 16

GladePlaceholder *
glade_placeholder_new (GladeWidget *parent)
{
	GladePlaceholder *placeholder;

	g_return_val_if_fail (parent != NULL, NULL);

	placeholder = gtk_drawing_area_new ();
	gtk_object_set_data (GTK_OBJECT (placeholder),
			     GLADE_PLACEHOLDER_PARENT_DATA,
			     parent);
	gtk_object_set_data (GTK_OBJECT (placeholder),
			     GLADE_PLACEHOLDER_IS_DATA,
			     GINT_TO_POINTER (TRUE));

	gtk_widget_set_events (GTK_WIDGET (placeholder),
			       gtk_widget_get_events (GTK_WIDGET (placeholder))
			       | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK
			       | GDK_BUTTON_RELEASE_MASK
			       | GDK_POINTER_MOTION_MASK | GDK_BUTTON1_MOTION_MASK);
	gtk_widget_set_usize (GTK_WIDGET (placeholder),
			      GLADE_PLACEHOLDER_SIZE,
			      GLADE_PLACEHOLDER_SIZE);			      
	gtk_widget_show (GTK_WIDGET (placeholder));
	
	glade_placeholder_connect_draw_signals  (placeholder);
	glade_placeholder_connect_mouse_signals (placeholder, parent->project);
	
	gtk_signal_connect (GTK_OBJECT (placeholder), "destroy",
			    GTK_SIGNAL_FUNC (glade_placeholder_on_destroy), NULL);
	
	return placeholder;
}

#undef GLADE_PLACEHOLDER_SIZE

void glade_placeholder_add (GladeWidgetClass *class,
			    GladeWidget *widget)
{

	if (GLADE_WIDGET_CLASS_TOPLEVEL (class)) {
		GladePlaceholder *placeholder;

		placeholder = glade_placeholder_new (widget);
		if (glade_widget_class_is (class, "GtkDialog")) {
			gtk_container_add (GTK_CONTAINER (GTK_DIALOG (widget->widget)->vbox),
					   GTK_WIDGET (placeholder));
			placeholder = glade_placeholder_new (widget);
			gtk_container_add (GTK_CONTAINER (GTK_DIALOG (widget->widget)->action_area),
					   GTK_WIDGET (placeholder));
		} else
			gtk_container_add (GTK_CONTAINER (widget->widget), GTK_WIDGET (placeholder));
	}

}
void
glade_placeholder_add_with_result (GladeWidgetClass *class,
				   GladeWidget *widget,
				   GladePropertyQueryResult *result)
{
	GList *list;

	list = class->properties;
	for (; list; list = list->next) {
		GladePropertyClass *class = list->data;
		if (class->query) {
			GladeProperty *property;
			gint temp;
			glade_property_query_result_get_int (result,
							     class->id,
							     &temp);
			property = glade_property_get_from_id (widget->properties,
							       class->id);
			glade_property_set_integer (property, temp);
		}
	}

	glade_placeholder_add (class, widget);

}


GladeWidget *
glade_placeholder_get_parent (GladePlaceholder *placeholder)
{
	GladeWidget *parent;
	
	parent = gtk_object_get_data (GTK_OBJECT (placeholder),
				      GLADE_PLACEHOLDER_PARENT_DATA);

	return parent;
}

void
glade_placeholder_replace (GladePlaceholder *placeholder, GladeWidget *parent, GladeWidget *child)
{
	g_return_if_fail (GTK_IS_WIDGET (placeholder));
	
	if (parent->class->placeholder_replace != NULL)
		parent->class->placeholder_replace (GTK_WIDGET (placeholder), child->widget, parent->widget);
	else
		g_warning ("Could not replace a placeholder because a replace "
			   " function has not been implemented for \"%s\"\n",
			   parent->class->name);
}



GladePlaceholder *
glade_placeholder_get_from_properties (GladeWidget *parent,
				       GHashTable *properties)
{
	GladePlaceholder *placeholder = NULL;
	GList *list;

	if (glade_widget_class_is (parent->class, "GtkVBox") ||
	    glade_widget_class_is (parent->class, "GtkHBox")) {
		GtkBoxChild *box_child;
		const gchar *val;
		
		list = gtk_container_children (GTK_CONTAINER (parent->widget));
		val = g_hash_table_lookup (properties, "position");
		box_child = (GtkBoxChild *) g_list_nth (list, atoi (val));
		placeholder = box_child->widget;
		g_assert (placeholder);
	} else if (glade_widget_class_is (parent->class, "GtkTable")) {
		GtkTableChild *child;
		gint col = atoi (g_hash_table_lookup (properties, "cell_x"));
		gint row = atoi (g_hash_table_lookup (properties, "cell_y"));

		list = GTK_TABLE (parent->widget)->children;
		for (; list; list = list->next) {
			child = list->data;
			if ((child->left_attach == col) &&
			    (child->top_attach  == row)) {
				placeholder = child->widget;
				break;
			}
		}
	} else if (glade_widget_class_is (parent->class, "GtkWindow")) {
		placeholder = GTK_BIN (parent->widget)->child;
	} else {
		glade_implement_me ();
	}

	return placeholder;
}

gboolean
glade_placeholder_is (GtkWidget *widget)
{
	gpointer data;
	gboolean is;

	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	data = gtk_object_get_data (GTK_OBJECT (widget),
				    GLADE_PLACEHOLDER_IS_DATA);

	is = GPOINTER_TO_INT (data);

	return is;
}


void
glade_placeholder_remove_all (GtkWidget *widget)
{
	GladeWidget *gwidget, *child_widget;
	
	
	gwidget = glade_widget_get_from_gtk_widget (widget);
	g_return_if_fail (widget != NULL);
	
	if (glade_widget_class_is (gwidget->class, "GtkVBox") ||
	    glade_widget_class_is (gwidget->class, "GtkHBox")) {
		GList *element;
		GtkBox *box;
		GtkBoxChild *box_child;

		box = GTK_BOX (widget);
		
		element = g_list_first (box->children);
		while (element != NULL) {
			box_child = element->data;
			if (glade_placeholder_is (box_child->widget)) {
				child_widget = glade_widget_get_from_gtk_widget (box_child->widget);
				if (child_widget)
                                	glade_widget_delete (child_widget);
	               	        gtk_container_remove (GTK_CONTAINER (box),
                        	                      box_child->widget);
				element = g_list_first (box->children);
			} else {
				element = g_list_next (element);
			}		
		}
	} else if (glade_widget_class_is (gwidget->class, "GtkDialog")) {
		GList *element;
		GtkBox *box;
		GtkBoxChild *box_child;
		gint i;

		box = GTK_BOX (GTK_DIALOG (widget)->vbox);
		for (i = 0; i < 2; i++) {
			
			element = g_list_first (box->children);
			
			while (element != NULL) {
				box_child = element->data;
				if (glade_placeholder_is (box_child->widget)) {
					child_widget = glade_widget_get_from_gtk_widget (box_child->widget);
					if (child_widget)
						glade_widget_delete (child_widget);
					gtk_container_remove (GTK_CONTAINER (box),
					box_child->widget);
					element = g_list_first (box->children);
				} else {
					element = g_list_next (element);
				}
			}
			box = GTK_BOX (GTK_DIALOG (widget)->action_area);
		}
	} else if (glade_widget_class_is (gwidget->class, "GtkTable")) {
		GList *element;
		GtkTableChild *table_child;

		element = g_list_first (GTK_TABLE (widget)->children);
		while (element != NULL) {
			table_child = element->data;
			if (glade_placeholder_is (table_child->widget)) {
				child_widget = glade_widget_get_from_gtk_widget (table_child->widget);
				if (child_widget)
					glade_widget_delete (child_widget);
				gtk_container_remove (GTK_CONTAINER (widget),
						      table_child->widget);
				element = g_list_first (GTK_TABLE (widget)->children);
			} else {
				element = g_list_next (element);
			}
		}
	} else {
		glade_implement_me ();
	}

}

void
glade_placeholder_fill_empty (GtkWidget *widget)
{
	GladeWidget *gwidget;

	gwidget = glade_widget_get_from_gtk_widget (widget);
	g_return_if_fail (widget != NULL);

	if (glade_widget_class_is (gwidget->class, "GtkVBox") ||
	    glade_widget_class_is (gwidget->class, "GtkHBox")) {
		GList *element;
		GtkBox *box;
		GtkBoxChild *box_child;
		GladeProperty *property;
		gint size, i, position;

		box = GTK_BOX (widget);
		
		property = glade_property_get_from_id   (gwidget->properties, "size");
		size = glade_property_get_integer (property);

		element = g_list_first (box->children);
		if (element)
			box_child = element->data;
		else
			box_child = NULL;
		i = 0;
		while (i < size) {
			GtkWidget *child;
			GladeWidget *glade_widget_child;

			if (box_child != NULL) {
				child = box_child->widget;
				glade_widget_child = glade_widget_get_from_gtk_widget (child);
			
				property = glade_property_get_from_id (glade_widget_child->properties,
							       "position");
				position = glade_property_get_integer (property);
			} else {
				position = size;
			}

			while (i++ < position) {
				GladePlaceholder *placeholder;
				
				placeholder = glade_placeholder_new (gwidget);
				gtk_box_pack_start_defaults (box, GTK_WIDGET (placeholder));
			}
			
		}
	} else if (glade_widget_class_is (gwidget->class, "GtkDialog")) {
		/* FIXME: We should create GladeWidget for children widgets */
/*		GList *element;
		GtkBox *box;
		GtkBoxChild *box_child;
		GladeProperty *property;
		gint size, i, j, position;

		box = GTK_BOX (GTK_DIALOG (widget)->vbox);
		for (j = 0; j < 2; j++) {
		
			property = glade_property_get_from_id   (gwidget->properties, "size");
			size = glade_property_get_integer (property);

			element = g_list_first (box->children);
			if (element)
				box_child = element->data;
			else
				box_child = NULL;
			i = 0;
			while (i < size) {
				GtkWidget *child;
				GladeWidget *glade_widget_child;

				if (box_child != NULL) {
					child = box_child->widget;
					glade_widget_child = glade_widget_get_from_gtk_widget (child);

					property = glade_property_get_from_id (glade_widget_child->properties,
									       "position");
					position = glade_property_get_integer (property);
				} else {
					position = size;
				}

				while (i++ < position) {
					GladePlaceholder *placeholder;

					placeholder = glade_placeholder_new (gwidget);
					gtk_box_pack_start_defaults (box, GTK_WIDGET (placeholder));
				}

			}
			box = (GTK_BOX (GTK_DIALOG (widget)->action_area);
		}
		*/
		glade_implement_me ();
	} else if (glade_widget_class_is (gwidget->class, "GtkTable")) {
		GtkTable *table;
		gint i,j;

		table = GTK_TABLE (widget);
		
		for (i = 0; i < table->ncols; i++) {
			for (j = 0; j < table->nrows; j++) {

				if (!glade_packing_table_get_child_at (table, i, j)) {
					GladePlaceholder *placeholder;

					placeholder = glade_placeholder_new (gwidget);
					gtk_table_attach_defaults (table, GTK_WIDGET (placeholder),
								   i, i+1, j, j+1);
				}
			}
		}
	} else {
		glade_implement_me ();
	}
}

void
glade_placeholder_paste_cb (GtkWidget *button, gpointer data)
{
	GladePlaceholder *placeholder = GTK_WIDGET (data);
	GladeProjectWindow *gpw;
	GladeClipboard *clipboard;

	gpw = glade_project_window_get ();
	clipboard = gpw->clipboard;

	glade_clipboard_paste (clipboard, placeholder);
}
