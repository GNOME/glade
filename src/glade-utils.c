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
#include <gtk/gtktooltips.h>
#include <gdk/gdkkeysyms.h>
#include <gmodule.h>
#include "glade.h"
#include "glade-project.h"
#include "glade-command.h"
#include "glade-debug.h"

#define GLADE_UTIL_ID_EXPOSE "glade_util_id_expose"
#define GLADE_UTIL_SELECTION_NODE_SIZE 7

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
glade_util_ui_warn (GtkWidget *parent, const gchar *warning)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_OK,
					 warning);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

typedef struct {
	GtkStatusbar *statusbar;
	guint context_id;
	guint message_id;
} FlashInfo;

static const guint32 flash_length = 3000;

static gboolean
remove_message_timeout (FlashInfo * fi) 
{
	gtk_statusbar_remove (fi->statusbar, fi->context_id, fi->message_id);
	g_free (fi);

	/* remove the timeout */
  	return FALSE;
}

/**
 * glade_utils_flash_message:
 * @statusbar: The statusbar
 * @context_id: The message context_id
 * @format: The message to flash on the statusbar
 *
 * Flash a temporary message on the statusbar.
 */
void
glade_util_flash_message (GtkWidget *statusbar, guint context_id, gchar *format, ...)
{
	va_list args;
	FlashInfo *fi;
	gchar *message;

	g_return_if_fail (GTK_IS_STATUSBAR (statusbar));
	g_return_if_fail (format != NULL);

	va_start (args, format);
	message = g_strdup_vprintf (format, args);
	va_end (args);

	fi = g_new (FlashInfo, 1);
	fi->statusbar = GTK_STATUSBAR (statusbar);
	fi->context_id = context_id;	
	fi->message_id = gtk_statusbar_push (fi->statusbar, fi->context_id, message);

	gtk_timeout_add (flash_length, (GtkFunction) remove_message_timeout, fi);

	g_free (message);
}

static gint
glade_util_compare_uline_labels (const gchar *labela, const gchar *labelb)
{
	for (;;) {
		gunichar c1, c2;

		if (*labela == '\0')
			return (*labelb == '\0') ? 0 : -1;
		if (*labelb == '\0')
			return 1;

		c1 = g_utf8_get_char (labela);
		if (c1 == '_') {
			labela = g_utf8_next_char (labela);
			c1 = g_utf8_get_char (labela);
		}

		c2 = g_utf8_get_char (labelb);
		if (c2 == '_') {
			labelb = g_utf8_next_char (labelb);
			c2 = g_utf8_get_char (labelb);
		}

		if (c1 < c2)
			return -1;
		if (c1 > c2)
			return 1;

		labela = g_utf8_next_char (labela);
		labelb = g_utf8_next_char (labelb);
	}

	/* Shouldn't be reached. */
	return 0;
}

/* This is a GCompareFunc for comparing the labels of 2 stock items, ignoring
   any '_' characters. It isn't particularly efficient. */
gint
glade_util_compare_stock_labels (gconstpointer a, gconstpointer b)
{
	const gchar *stock_ida = a, *stock_idb = b;
	GtkStockItem itema, itemb;
	gboolean founda, foundb;
	gint retval;

	founda = gtk_stock_lookup (stock_ida, &itema);
	foundb = gtk_stock_lookup (stock_idb, &itemb);

	if (founda) {
		if (!foundb)
			retval = -1;
		else
			/* FIXME: Not ideal for UTF-8. */
			retval = glade_util_compare_uline_labels (itema.label, itemb.label);
	}
	else {
		if (!foundb)
			retval = 0;
		else
			retval = 1;
	}

	return retval;
}

gchar *
glade_util_gtk_combo_func (gpointer data)
{
	GtkListItem * listitem = data;

	/* I needed to pinch this as well - Damon. */
	static const gchar *gtk_combo_string_key = "gtk-combo-string-value";

	GtkWidget *label;
	gchar *ltext = NULL;

	ltext = (gchar *) gtk_object_get_data (GTK_OBJECT (listitem),
					       gtk_combo_string_key);
	if (!ltext) {
		label = GTK_BIN (listitem)->child;
		if (!label || !GTK_IS_LABEL (label))
			return NULL;
		ltext = (gchar*) gtk_label_get_text (GTK_LABEL (label));
	}

	return ltext;
}

/* These are pinched from gtkcombo.c */
gpointer /* GtkListItem *  */
glade_util_gtk_combo_find (GtkCombo * combo)
{
	gchar *text;
	gchar *ltext;
	GList *clist;
	int (*string_compare) (const char *, const char *);

	if (combo->case_sensitive)
		string_compare = strcmp;
	else
		string_compare = g_strcasecmp;

	text = (gchar*) gtk_entry_get_text (GTK_ENTRY (combo->entry));
	clist = GTK_LIST (combo->list)->children;

	while (clist && clist->data) {
		ltext = glade_util_gtk_combo_func (GTK_LIST_ITEM (clist->data));
		if (!ltext)
			continue;
		if (!(*string_compare) (ltext, text))
			return (GtkListItem *) clist->data;
		clist = clist->next;
	}

	return NULL;
}

/**
 * glade_util_hide_window:
 * @window:
 *
 * If you use this function to handle the delete_event of a window, when it
 * will be shown again it will appear in the position where it was before
 * beeing hidden.
 **/
void
glade_util_hide_window (GtkWindow *window)
{
	gint x, y;

	g_return_if_fail (GTK_IS_WINDOW (window));

	/* remember position of window for when it is used again */
	gtk_window_get_position (window, &x, &y);

	gtk_widget_hide (GTK_WIDGET (window));

	gtk_window_move(window, x, y);
}

gint
glade_util_check_key_is_esc (GtkWidget *widget,
				  GdkEventKey *event,
				  gpointer data)
{
	g_return_val_if_fail (GTK_IS_WINDOW (widget), FALSE);
  
	if (event->keyval == GDK_Escape) {
		GladeEscAction action = GPOINTER_TO_INT (data);
  
		if (action == GladeEscCloses) {
			glade_util_hide_window (GTK_WINDOW (widget));
			return TRUE;
		}
		else if (action == GladeEscDestroys) { 
			gtk_widget_destroy (widget);
			return TRUE;
		}
		else
			return FALSE;
	}
	else
		return FALSE;
}

/**
 * glade_util_file_selection_new:
 * @title: dialog title
 * @parent: the window the dialog is set transient for
 *
 * Returns a file selection dialog. It's up to the caller to set up a
 * callback for the OK button and then to show the dialog
 **/
GtkWidget *
glade_util_file_selection_new (const gchar *title, GtkWindow *parent)
{
	GtkWidget *filesel;

	filesel = gtk_file_selection_new (title);
	g_signal_connect_swapped (G_OBJECT (GTK_FILE_SELECTION(filesel)->cancel_button),
				  "clicked", G_CALLBACK (gtk_widget_destroy),
				  filesel);
	g_signal_connect_swapped (G_OBJECT (filesel), "delete_event",
				  G_CALLBACK (gtk_widget_destroy),
				  filesel);

	if (GTK_IS_WINDOW (parent))
		gtk_window_set_transient_for (GTK_WINDOW (filesel), GTK_WINDOW (parent));

	return filesel;
}
 

/**
 * changes each occurence of the character a on the string str by the character b.
 */
void
glade_util_replace (char *str, char a, char b)
{
	g_return_if_fail (str != NULL);

	while (*str != 0) {
		if (*str == a)
			*str = b;

		str = g_utf8_next_char (str);
	}
}


/**
 * duplicates the string passed as argument, but changing each underscore
 * in the original string by two underscores.  Returns a newly allocated
 * string, or NULL if there is not enough memory.
 */
char *
glade_util_duplicate_underscores (const char *name)
{
	const char *tmp;
	const char *last_tmp = name;
	char *underscored_name = g_malloc (strlen (name) * 2 + 1);
	char *tmp_underscored = underscored_name;

	if (!underscored_name)
	{
		g_critical ("Not enough memory!");
		return NULL;
	}

	for (tmp = last_tmp; *tmp; tmp = g_utf8_next_char (tmp))
	{
		if (*tmp == '_')
		{
			memcpy (tmp_underscored, last_tmp, tmp - last_tmp + 1);
			tmp_underscored += tmp - last_tmp + 1;
			last_tmp = tmp + 1;
			*tmp_underscored++ = '_';
		}
	}

	memcpy (tmp_underscored, last_tmp, tmp - last_tmp + 1);

	return underscored_name;
}

static gboolean
glade_util_draw_nodes (GtkWidget *widget, GdkEventExpose *expose, gpointer unused)
{
	GdkGC *gc = widget->style->black_gc;
	gint x, y;
	gint width, height;
	GdkWindow *window = expose->window;
	gpointer data;

	gdk_window_get_user_data(window, &data);
	gtk_widget_translate_coordinates (widget, GTK_WIDGET (data), 0, 0, &x, &y);
	width = widget->allocation.width;
	height = widget->allocation.height;

	gdk_gc_set_subwindow (gc, GDK_INCLUDE_INFERIORS);

	if (width > GLADE_UTIL_SELECTION_NODE_SIZE && height > GLADE_UTIL_SELECTION_NODE_SIZE) {
		gdk_draw_rectangle (window, gc, TRUE,
				    x, y,
				    GLADE_UTIL_SELECTION_NODE_SIZE, GLADE_UTIL_SELECTION_NODE_SIZE);
		gdk_draw_rectangle (window, gc, TRUE,
				    x, y + height - GLADE_UTIL_SELECTION_NODE_SIZE,
				    GLADE_UTIL_SELECTION_NODE_SIZE, GLADE_UTIL_SELECTION_NODE_SIZE);
		gdk_draw_rectangle (window, gc, TRUE,
				    x + width - GLADE_UTIL_SELECTION_NODE_SIZE, y,
				    GLADE_UTIL_SELECTION_NODE_SIZE, GLADE_UTIL_SELECTION_NODE_SIZE);
		gdk_draw_rectangle (window, gc, TRUE,
				    x + width - GLADE_UTIL_SELECTION_NODE_SIZE,
				    y + height - GLADE_UTIL_SELECTION_NODE_SIZE,
				    GLADE_UTIL_SELECTION_NODE_SIZE, GLADE_UTIL_SELECTION_NODE_SIZE);
	}

	gdk_draw_rectangle (window, gc, FALSE, x, y, width - 1, height - 1);

	gdk_gc_set_subwindow (gc, GDK_CLIP_BY_CHILDREN);
	g_debug (("(%d, %d, %d, %d)\n", expose->area.x, expose->area.y, expose->area.width, expose->area.height));

	return FALSE;
}

void
glade_util_add_nodes (GtkWidget *widget)
{
	gint id;

	id = g_signal_connect_after (G_OBJECT (widget), "expose_event",
				     G_CALLBACK (glade_util_draw_nodes), NULL);
	g_object_set_data (G_OBJECT (widget), GLADE_UTIL_ID_EXPOSE, GINT_TO_POINTER (id));
	gtk_widget_queue_draw (widget);
}

void
glade_util_remove_nodes (GtkWidget *widget)
{
	gint id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), GLADE_UTIL_ID_EXPOSE));

	g_return_if_fail (id != 0);

	g_signal_handler_disconnect (G_OBJECT (widget), id);
	g_object_set_data (G_OBJECT (widget), GLADE_UTIL_ID_EXPOSE, 0);
	gtk_widget_queue_draw (widget);
}

gboolean
glade_util_has_nodes (GtkWidget *widget)
{
	return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), GLADE_UTIL_ID_EXPOSE)) != 0;
}

void
glade_util_delete_selection (GladeProject *project)
{
	GList *selection;
	GList *free_me;
	GList *list;

	g_return_if_fail (GLADE_IS_PROJECT (project));

	selection = glade_project_selection_get (project);
	if (!selection)
		return;

	/* We have to be careful when deleting widgets from the selection
	 * because when we delete each widget, the selection pointer changes
	 * due to the g_list_remove performed by glade_command_delete.
	 * Copy the list and free it after we are done
	 */
	free_me = g_list_copy (selection);
	for (list = free_me; list; list = list->next) {
		GladeWidget *glade_widget;

		glade_widget = glade_widget_get_from_gtk_widget (list->data);
		if (glade_widget)
			glade_command_delete (glade_widget);
	}

	g_list_free (free_me);
}

/*
 * taken from gtk... maybe someday we can convince them to
 * expose gtk_container_get_all_children
 */
static void
gtk_container_children_callback (GtkWidget *widget,
				 gpointer client_data)
{
	GList **children;

	children = (GList**) client_data;
	*children = g_list_prepend (*children, widget);
}

GList *
glade_util_container_get_all_children (GtkContainer *container)
{
	GList *children = NULL;

	g_return_val_if_fail (GTK_IS_CONTAINER (container), NULL);

	gtk_container_foreach (container,
			       gtk_container_children_callback,
			       &children);

	return g_list_reverse (children);
}

