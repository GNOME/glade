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
#include "glade-project-window.h"
#include "glade-command.h"
#include "glade-debug.h"
#include "glade-placeholder.h"
#include "glade-widget.h"
#include "glade-widget-class.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-clipboard.h"

#define GLADE_UTIL_SELECTION_NODE_SIZE 7

/* List of widgets that have selection
 */
static GList *glade_util_selection = NULL;


/**
 * glade_util_widget_set_tooltip:
 * @widget: a #GtkWidget
 * @str: a string
 *
 * Creates a new tooltip from @str and sets @widget to use it.
 */
void
glade_util_widget_set_tooltip (GtkWidget *widget, const gchar *str)
{
	GtkTooltips *tooltips;

	tooltips = gtk_tooltips_new ();
	g_object_ref (G_OBJECT (tooltips));
	gtk_object_sink (GTK_OBJECT (tooltips));
	g_object_set_data_full (G_OBJECT (widget),
				"tooltips", tooltips,
				(GDestroyNotify) g_object_unref);
	gtk_tooltips_set_tip (tooltips, widget, str, NULL);
}

/**
 * glade_util_compose_get_type_func:
 * @name:
 *
 * TODO: write me
 *
 * Returns:
 */
static gchar *
glade_util_compose_get_type_func (const gchar *name)
{
	gchar *retval;
	GString *tmp;
	gint i = 1, j;

	tmp = g_string_new (name);

	while (tmp->str[i])
	{
		if (g_ascii_isupper (tmp->str[i]))
		{
			tmp = g_string_insert_c (tmp, i++, '_');

			j = 0;
			while (g_ascii_isupper (tmp->str[i++]))
				j++;

			if (j > 2)
				g_string_insert_c (tmp, i-2, '_');

			continue;
		}
		i++;
	}

	tmp = g_string_append (tmp, "_get_type");
        retval = g_ascii_strdown (tmp->str, tmp->len);
	g_string_free (tmp, TRUE);

	return retval;
}

/**
 * glade_util_get_type_from_name:
 * @name:
 *
 * TODO: write me
 *
 * Returns:
 */
GType
glade_util_get_type_from_name (const gchar *name)
{
	static GModule *allsymbols = NULL;
	GType (*get_type) ();
	GType type = 0;
	gchar  *func_name;

	if ((func_name = glade_util_compose_get_type_func (name)) != NULL)
	{
		
		if (!allsymbols)
			allsymbols = g_module_open (NULL, 0);

		if (g_module_symbol (allsymbols, func_name,
				      (gpointer) &get_type))
		{
			g_assert (get_type);
			type = get_type ();
		} else {
			g_warning (_("We could not find the symbol \"%s\""),
				   func_name);
		}
		g_free (func_name);
	}

	if (type == 0) {
		g_warning(_("Could not get the type from \"%s"),
			  name);
	}
	
	return type;
}

/**
 * glade_utils_get_pspec_from_funcname:
 * @name:
 *
 * TODO: write me
 *
 * Returns:
 */
GParamSpec *
glade_utils_get_pspec_from_funcname (const gchar *funcname)
{
	static GModule *allsymbols     = NULL;
	GParamSpec     *pspec          = NULL;
	GParamSpec     *(*get_pspec)() = NULL;
	
	if (!allsymbols)
		allsymbols = g_module_open (NULL, 0);

	if (!g_module_symbol (allsymbols, funcname,
			      (gpointer) &get_pspec)) {
		g_warning (_("We could not find the symbol \"%s\""),
			   funcname);
		return FALSE;
	}

	g_assert (get_pspec);
	pspec = get_pspec ();

	return pspec;
}

/**
 * glade_util_ui_warn:
 * @parent: a #GtkWindow cast as a #GtkWidget
 * @warning: a string
 *
 * Creates a new warning dialog window as a child of @parent containing
 * the text of @warning, runs it, then destroys it on close.
 */
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

/**
 * glade_util_compare_stock_labels:
 * @a: a #gconstpointer to a #GtkStockItem
 * @b: a #gconstpointer to a #GtkStockItem
 *
 * This is a #GCompareFunc that compares the labels of two stock items, 
 * ignoring any '_' characters. It isn't particularly efficient.
 *
 * Returns: negative value if @a < @b; zero if @a = @b; 
 *          positive value if @a > @b
 */
gint
glade_util_compare_stock_labels (gconstpointer a, gconstpointer b)
{
	const gchar *stock_ida = a, *stock_idb = b;
	GtkStockItem itema, itemb;
	gboolean founda, foundb;
	gint retval;

	founda = gtk_stock_lookup (stock_ida, &itema);
	foundb = gtk_stock_lookup (stock_idb, &itemb);

	if (founda)
	{
		if (!foundb)
			retval = -1;
		else
			/* FIXME: Not ideal for UTF-8. */
			retval = glade_util_compare_uline_labels (itema.label, itemb.label);
	}
	else
	{
		if (!foundb)
			retval = 0;
		else
			retval = 1;
	}

	return retval;
}

/**
 * glade_util_gtk_combo_func:
 * @data:
 *
 * TODO: write me
 *
 * Returns:
 */
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
/**
 * glade_util_gtk_combo_find:
 * @combo:
 *
 * TODO: write me
 *
 * Returns:
 */
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
 * @window: a #GtkWindow
 *
 * If you use this function to handle the delete_event of a window, when it
 * will be shown again it will appear in the position where it was before
 * beeing hidden.
 */
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

/**
 * glade_util_file_dialog_new:
 * @title: dialog title
 * @parent: the parent #GtkWindow for the dialog
 * @action: a #GladeUtilFileDialogType to say if the dialog will open or save
 *
 * Returns: a file chooser dialog. The caller is responsible 
 *          for showing the dialog
 */
GtkWidget *
glade_util_file_dialog_new (const gchar *title, GtkWindow *parent, 
			     GladeUtilFileDialogType action)
{
	GtkWidget *file_dialog;

	g_return_val_if_fail ((action == GLADE_FILE_DIALOG_ACTION_OPEN ||
			       action == GLADE_FILE_DIALOG_ACTION_SAVE), NULL);

	file_dialog = gtk_file_chooser_dialog_new (title, parent, action,
						    GTK_STOCK_CANCEL,
						    GTK_RESPONSE_CANCEL,
						    action == GLADE_FILE_DIALOG_ACTION_OPEN ?
						    GTK_STOCK_OPEN : GTK_STOCK_SAVE,
						    GTK_RESPONSE_OK,
						    NULL);
	gtk_window_set_position (GTK_WINDOW (file_dialog), GTK_WIN_POS_CENTER);

	return file_dialog;
}

/**
 * glade_util_replace:
 * @str: a string
 * @a: a #char
 * @b: a #char
 *
 * Replaces each occurance of the character @a in @str to @b.
 */
void
glade_util_replace (char *str, char a, char b)
{
	g_return_if_fail (str != NULL);

	while (*str != 0)
	{
		if (*str == a)
			*str = b;

		str = g_utf8_next_char (str);
	}
}

/**
 * glade_util_duplicate_underscores:
 * @name: a string
 *
 * Duplicates @name, but the copy has two underscores in place of any single
 * underscore in the original.
 *
 * Returns: a newly allocated string
 */
char *
glade_util_duplicate_underscores (const char *name)
{
	const char *tmp;
	const char *last_tmp = name;
	char *underscored_name = g_malloc (strlen (name) * 2 + 1);
	char *tmp_underscored = underscored_name;

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

/* This returns the window that the given widget's position is relative to.
   Usually this is the widget's parent's window. But if the widget is a
   toplevel, we use its own window, as it doesn't have a parent.
   Some widgets also lay out widgets in different ways. */
static GdkWindow*
glade_util_get_window_positioned_in (GtkWidget *widget)
{
	GtkWidget *parent;

	parent = widget->parent;

#ifdef USE_GNOME
	/* BonoboDockItem widgets use a different window when floating.
	   FIXME: I've left this here so we remember to add it when we add
	   GNOME support. */
	if (BONOBO_IS_DOCK_ITEM (widget)
	    && BONOBO_DOCK_ITEM (widget)->is_floating) {
		return BONOBO_DOCK_ITEM (widget)->float_window;
	}

	if (parent && BONOBO_IS_DOCK_ITEM (parent)
	    && BONOBO_DOCK_ITEM (parent)->is_floating) {
		return BONOBO_DOCK_ITEM (parent)->float_window;
	}
#endif

	if (parent)
		return parent->window;

	return widget->window;
}

static void
glade_util_draw_nodes (GdkWindow *window, GdkGC *gc,
		       gint x, gint y,
		       gint width, gint height)
{
	if (width > GLADE_UTIL_SELECTION_NODE_SIZE && height > GLADE_UTIL_SELECTION_NODE_SIZE) {
		gdk_draw_rectangle (window, gc, TRUE,
				    x, y,
				    GLADE_UTIL_SELECTION_NODE_SIZE,
				    GLADE_UTIL_SELECTION_NODE_SIZE);
		gdk_draw_rectangle (window, gc, TRUE,
				    x, y + height - GLADE_UTIL_SELECTION_NODE_SIZE,
				    GLADE_UTIL_SELECTION_NODE_SIZE,
				    GLADE_UTIL_SELECTION_NODE_SIZE);
		gdk_draw_rectangle (window, gc, TRUE,
				    x + width - GLADE_UTIL_SELECTION_NODE_SIZE, y,
				    GLADE_UTIL_SELECTION_NODE_SIZE,
				    GLADE_UTIL_SELECTION_NODE_SIZE);
		gdk_draw_rectangle (window, gc, TRUE,
				    x + width - GLADE_UTIL_SELECTION_NODE_SIZE,
				    y + height - GLADE_UTIL_SELECTION_NODE_SIZE,
				    GLADE_UTIL_SELECTION_NODE_SIZE,
				    GLADE_UTIL_SELECTION_NODE_SIZE);
	}

	gdk_draw_rectangle (window, gc, FALSE, x, y, width - 1, height - 1);
}

/* This calculates the offset of the given window within its toplevel.
   It also returns the toplevel. */
static void
glade_util_calculate_window_offset (GdkWindow *window,
				    gint *x, gint *y,
				    GdkWindow **toplevel)
{
	gint tmp_x, tmp_y;

	/* Calculate the offset of the window within its toplevel. */
	*x = 0;
	*y = 0;

	for (;;) {
		if (gdk_window_get_window_type (window) != GDK_WINDOW_CHILD)
			break;
		gdk_window_get_position (window, &tmp_x, &tmp_y);
		*x += tmp_x;
		*y += tmp_y;
		window = gdk_window_get_parent (window);
	}

	*toplevel = window;
}

/* This returns TRUE if it is OK to draw the selection nodes for the given
   selected widget inside the given window that has received an expose event.
   For most widgets it returns TRUE, but if a selected widget is inside a
   widget like a viewport, that uses its own coordinate system, then it only
   returns TRUE if the expose window is inside the viewport as well. */
static gboolean
glade_util_can_draw_nodes (GtkWidget *sel_widget, GdkWindow *sel_win,
			   GdkWindow *expose_win)
{
	GtkWidget *widget, *viewport = NULL;
	GdkWindow *viewport_win = NULL;

	/* Check if the selected widget is inside a viewport. */
	for (widget = sel_widget->parent; widget; widget = widget->parent) {
		if (GTK_IS_VIEWPORT (widget)) {
			viewport = widget;
			viewport_win = GTK_VIEWPORT (widget)->bin_window;
			break;
		}
	}

	/* If there is no viewport-type widget above the selected widget,
	   it is OK to draw the selection anywhere. */
	if (!viewport)
		return TRUE;

	/* If we have a viewport-type widget, check if the expose_win is
	   beneath the viewport. If it is, we can draw in it. If not, we
	   can't.*/
	for (;;) {
		if (expose_win == sel_win)
			return TRUE;
		if (expose_win == viewport_win)
			return FALSE;
		if (gdk_window_get_window_type (expose_win) != GDK_WINDOW_CHILD)
			break;
		expose_win = gdk_window_get_parent (expose_win);
	}

	return FALSE;
}

/**
 * glade_util_draw_nodes_idle:
 * @expose_win: a #GdkWindow
 *
 * Redraws any selection nodes that intersect @expose_win. Steps through all
 * selected widgets, finds their coordinates, and calls glade_util_draw_nodes()
 * if appropriate.
 *
 * Returns: %FALSE
 */
gboolean
glade_util_draw_nodes_idle (GdkWindow *expose_win)
{
	GtkWidget *expose_widget;
	gint expose_win_x, expose_win_y;
	gint expose_win_w, expose_win_h;
	GdkWindow   *expose_toplevel;
	GdkGC *gc;
	GList *elem;

	/* Check that the window is still alive. */
	if (!gdk_window_is_viewable (expose_win))
		goto out;

	/* Find the corresponding GtkWidget */
	gdk_window_get_user_data (expose_win, (gpointer *)&expose_widget);

	gc = expose_widget->style->black_gc;

	/* Calculate the offset of the expose window within its toplevel. */
	glade_util_calculate_window_offset (expose_win,
					    &expose_win_x,
					    &expose_win_y,
					    &expose_toplevel);

	gdk_drawable_get_size (expose_win,
			       &expose_win_w, &expose_win_h);

	/* Step through all the selected widgets. */
	for (elem = glade_util_selection; elem; elem = elem->next) {

		GtkWidget *sel_widget;
		GdkWindow *sel_win, *sel_toplevel;
		gint sel_x, sel_y, x, y, w, h;

		sel_widget = elem->data;
		
		if ((sel_win = glade_util_get_window_positioned_in (sel_widget)) == NULL)
			continue;
		
		/* Calculate the offset of the selected widget's window
		   within its toplevel. */
		glade_util_calculate_window_offset (sel_win, &sel_x, &sel_y,
						    &sel_toplevel);

		/* We only draw the nodes if the window that got the expose
		   event is in the same toplevel as the selected widget. */
		if (expose_toplevel == sel_toplevel
		    && glade_util_can_draw_nodes (sel_widget, sel_win,
						  expose_win)) {
			x = sel_x + sel_widget->allocation.x - expose_win_x;
			y = sel_y + sel_widget->allocation.y - expose_win_y;
			w = sel_widget->allocation.width;
			h = sel_widget->allocation.height;

			/* Draw the selection nodes if they intersect the
			   expose window bounds. */
			if (x < expose_win_w && x + w >= 0
			    && y < expose_win_h && y + h >= 0) {
				glade_util_draw_nodes (expose_win, gc,
						       x, y, w, h);
			}
		}
	}

 out:
	/* Remove the reference added in glade_util_queue_draw_nodes(). */
	g_object_unref (G_OBJECT (expose_win));
	
	/* Return FALSE so the idle handler isn't called again. */
	return FALSE;
}

#define GLADE_DRAW_NODES_IDLE_PRIORITY	GTK_PRIORITY_DEFAULT + 10

/**
 * glade_util_queue_draw_nodes:
 * @window: A #GdkWindow
 *
 * This function should be called whenever a widget in the interface receives 
 * an expose event. It sets up an idle function which will redraw any selection
 * nodes that intersect the the exposed window.
 */
void
glade_util_queue_draw_nodes (GdkWindow *window)
{
	g_return_if_fail (GDK_IS_WINDOW (window));

	g_idle_add_full (GLADE_DRAW_NODES_IDLE_PRIORITY,
			 (GSourceFunc)glade_util_draw_nodes_idle,
			 window, NULL);

	g_object_ref (G_OBJECT (window));
}


/**
 * glade_util_add_selection:
 * @widget: a #GtkWidget
 *
 * Add visual selection to this GtkWidget
 */
void
glade_util_add_selection (GtkWidget *widget)
{
	g_return_if_fail (GTK_IS_WIDGET (widget));
	if (glade_util_has_selection (widget))
		return;

	glade_util_selection = 
		g_list_prepend (glade_util_selection, widget);
	gtk_widget_queue_draw (widget);
}

/**
 * glade_util_remove_selection:
 * @widget: a #GtkWidget
 *
 * Remove visual selection from this GtkWidget
 */
void
glade_util_remove_selection (GtkWidget *widget)
{
	g_return_if_fail (GTK_IS_WIDGET (widget));
	if (!glade_util_has_selection (widget))
		return;

	glade_util_selection = 
		g_list_remove (glade_util_selection, widget);

	/* We redraw the parent, since the selection rectangle may not be
	   cleared if we just redraw the widget itself. */
	gtk_widget_queue_draw (widget->parent ?
			       widget->parent : widget);
}

/**
 * glade_util_clear_selection:
 *
 * Clear all visual selections
 */
void
glade_util_clear_selection (void)
{
	GtkWidget *widget;
	GList     *list;

	for (list = glade_util_selection;
	     list && list->data;
	     list = list->next)
	{
		widget = list->data;
		gtk_widget_queue_draw (widget->parent ?
				       widget->parent : widget);
	}
	glade_util_selection =
		(g_list_free (glade_util_selection), NULL);
}

/**
 * glade_util_has_selectoin:
 * @widget: a #GtkWidget
 *
 * Returns: %TRUE if @widget has visual selection, %FALSE otherwise
 */
gboolean
glade_util_has_selection (GtkWidget *widget)
{
	g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
	return g_list_find (glade_util_selection, widget) != NULL;
}

/**
 * glade_util_get_selectoin:
 *
 * Returns: The list of selected #GtkWidgets
 */
GList *
glade_util_get_selection ()
{
	return glade_util_selection;
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

/**
 * glade_util_container_get_all_children:
 * @container: a #GtkContainer
 *
 * Returns: a #GList giving the contents of @container
 */
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

/**
 * glade_util_uri_list_parse:
 * @uri_list: the text/urilist, must be NULL terminated.
 *
 * Extracts a list of file names from a standard text/uri-list,
 * such as one you would get on a drop operation.
 * This is mostly stolen from gnome-vfs-uri.c.
 *
 * Returns: a #GList of gchars.
 */
GList *
glade_util_uri_list_parse (const gchar *uri_list)
{
	const gchar *p, *q;
	GList *result = NULL;

	g_return_val_if_fail (uri_list != NULL, NULL);

	p = uri_list;

	/* We don't actually try to validate the URI according to RFC
	 * 2396, or even check for allowed characters - we just ignore
	 * comments and trim whitespace off the ends.  We also
	 * allow LF delimination as well as the specified CRLF.
	 */
	while (p)
	{
		if (*p != '#')
		{
			gchar *retval;
			gchar *path;

			while (g_ascii_isspace (*p))
				p++;

			q = p;
			while ((*q != '\0') && (*q != '\n') && (*q != '\r'))
				q++;

			if (q > p)
			{
				q--;
				while (q > p && g_ascii_isspace (*q))
					q--;

				retval = g_new (gchar, q - p + 2);
				memcpy (retval, p, q - p + 1);
				retval[q - p + 1] = '\0';

				path = g_filename_from_uri (retval, NULL, NULL);
				if (!path)
				{
					g_free (retval);
					continue;
				}

				result = g_list_prepend (result, path);

				g_free (retval);
			}
		}
		p = strchr (p, '\n');
		if (p)
			p++;
	}

	return g_list_reverse (result);
}

/**
 * glade_util_gtkcontainer_relation:
 * @widget: a GladeWidget
 * @parent: a GladeWidget
 *
 *
 * Returns whether this widget is parented by a GtkContainer
 * and that it is parented through the GtkContainer interface.
 */
gboolean
glade_util_gtkcontainer_relation (GladeWidget *parent, GladeWidget *widget)
{
	GladeSupportedChild *support;
	g_return_val_if_fail (GLADE_IS_WIDGET (parent), FALSE);
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
	return (GTK_IS_CONTAINER (parent->object)                      &&
		(support = glade_widget_class_get_child_support
		 (parent->widget_class, widget->widget_class->type))   &&
		(support->type == GTK_TYPE_WIDGET));
}

/**
 * glade_util_any_gtkcontainer_relation:
 * @widget: a GladeWidget
 * @parent: a GladeWidget
 *
 *
 * Returns whether any of these widgets are parented by a GtkContainer
 * through the GtkContainer interface.
 */
gboolean
glade_util_any_gtkcontainer_relation (GladeWidget *parent, GList *widgets)
{
	GList *list;
	for (list = widgets; list && list->data; list = list->next)
		if (glade_util_gtkcontainer_relation 
		    (parent, GLADE_WIDGET (list->data)))
		    return TRUE;
	return FALSE;
}

/**
 * glade_util_widget_pastable:
 * @child: a GladeWidget
 * @widget: a GladeWidget
 *
 * Returns whether this parent widget has an implementation to parent child.
 */
gboolean
glade_util_widget_pastable (GladeWidget *child,
			    GladeWidget *parent)
{
	g_return_val_if_fail (GLADE_IS_WIDGET (child),  FALSE);
	g_return_val_if_fail (GLADE_IS_WIDGET (parent), FALSE);
	return (glade_widget_class_get_child_support
		(parent->widget_class,
		 child->widget_class->type) != NULL) ? TRUE : FALSE;
}


/**
 * glade_util_paste_clipboard:
 * @placeholder: The #GladePlaceholder to paste the clipboard item to,
 *               if NULL; paste to the selected object in the active project.
 *
 * Paste the clipboard selection to the active project's 
 * selection (the project must have only one object selected).
 */
void
glade_util_paste_clipboard (GladePlaceholder *placeholder)
{
	GladeProjectWindow *gpw     = glade_project_window_get ();
	GladeProject       *project = glade_project_window_get_active_project (gpw);
	GList              *widgets = NULL, *list;
	GladeWidget        *widget, *parent;

      
	if ((list = glade_project_selection_get (project)) != NULL)
	{
		if (placeholder == NULL &&
		    g_list_length (list) != 1)
		{
			glade_util_ui_warn 
				(gpw->window, _("Unable to paste to multiple widgets"));
			return;
		}
	}
	else if (placeholder == NULL)
	{
		glade_util_ui_warn (gpw->window, _("No target widget selected"));
		return;
	}

	if (g_list_length (gpw->clipboard->selection) == 0)
	{
		glade_util_ui_warn (gpw->window, _("No widget selected on the clipboard"));
		return;
	}

	if (placeholder)
		parent = glade_placeholder_get_parent (placeholder);
	else
		parent = glade_widget_get_from_gobject (list->data);

	for (list = gpw->clipboard->selection; 
	     list && list->data; list = list->next)
	{
		widget = list->data;

		if (parent && 
		    glade_util_widget_pastable (widget, parent) == FALSE)
		{
			gchar *message = g_strdup_printf
				(_("Unable to paste widget %s to parent %s"),
				 widget->name, parent->name);
			glade_util_ui_warn (gpw->window, message);
			g_free (message);
			return;
		}
	}
	widgets = g_list_copy (gpw->clipboard->selection);
	glade_command_paste (widgets, parent, placeholder);
	g_list_free (widgets);
}

/**
 * glade_util_cut_selection:
 *
 * Cut the active project's selection.
 */
void
glade_util_cut_selection (void)
{
	GladeProjectWindow *gpw     = glade_project_window_get ();
	GladeProject       *project = glade_project_window_get_active_project (gpw);
	GList              *widgets = NULL, *list;
	GladeWidget        *widget;
	
	for (list = glade_project_selection_get (project);
	     list && list->data; list = list->next)
	{
		widget  = glade_widget_get_from_gobject (GTK_WIDGET (list->data));
		widgets = g_list_prepend (widgets, widget);
	}

	if (widgets)
	{
		glade_command_cut (widgets);
		g_list_free (widgets);
	}
	else
	{
		glade_util_ui_warn (gpw->window, _("No widget selected!"));
	}
}

/**
 * glade_util_copy_selection:
 *
 * Copy the active project's selection.
 */
void
glade_util_copy_selection (void)
{
	GladeProjectWindow *gpw     = glade_project_window_get ();
	GladeProject       *project = glade_project_window_get_active_project (gpw);
	GList              *widgets = NULL, *list;
	GladeWidget        *widget;
	
	for (list = glade_project_selection_get (project);
	     list && list->data; list = list->next)
	{
		if ((widget = 
		     glade_widget_get_from_gobject (list->data)) != NULL)
			widgets = g_list_prepend (widgets, widget);
	}

	if (widgets)
	{
		glade_command_copy (widgets);
		g_list_free (widgets);
	}
	else
	{
		glade_util_ui_warn (gpw->window, _("No widget selected!"));
	}
}

/**
 * glade_util_delete_selection:
 *
 * Delete the active project's selection.
 */
void
glade_util_delete_selection (void)
{
	GladeProjectWindow *gpw;
	GList              *widgets = NULL, *list;
	GladeWidget        *widget;

	gpw = glade_project_window_get ();
	
	for (list = glade_project_selection_get (gpw->active_project);
	     list && list->data; list = list->next)
	{
		widget  = glade_widget_get_from_gobject (GTK_WIDGET (list->data));
		widgets = g_list_prepend (widgets, widget);
	}

	if (widgets)
	{
		glade_command_delete (widgets);
		g_list_free (widgets);
	}
	else
	{
		glade_util_ui_warn (gpw->window, _("No widget selected!"));
	}
}


static GtkTreeIter *
glade_util_find_iter (GtkTreeModel *model,
		      GtkTreeIter  *iter,
		      GladeWidget  *findme,
		      gint          column)
{
	GtkTreeIter *retval = NULL;
	GladeWidget *widget;
	GtkTreeIter *next;

	g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);
	g_return_val_if_fail (iter != NULL, NULL);

	next = gtk_tree_iter_copy (iter);
	g_return_val_if_fail (next != NULL, NULL);

	while (retval == NULL)
	{
		gtk_tree_model_get (model, next, column, &widget, -1);
		if (widget == NULL) {
			g_warning ("Could not get the glade widget from the model");
			break;
		}
		else if (widget == findme)
		{
			retval = gtk_tree_iter_copy (next);
			break;
		}
		else if (gtk_tree_model_iter_has_child (model, next))
		{
			GtkTreeIter  child;
			gtk_tree_model_iter_children (model, &child, next);
			if ((retval = glade_util_find_iter
			     (model, &child, findme, column)) != NULL)
				break;
		}

		if (!gtk_tree_model_iter_next (model, next))
			break;
	}
	gtk_tree_iter_free (next);

	return retval;
}

/**
 * glade_util_find_iter_by_widget:
 * @model: a #GtkTreeModel
 * @findme: a #GladeWidget
 * @column: a #gint
 *
 * Looks through @model for the #GtkTreeIter corresponding to 
 * @findme under @column.
 *
 * Returns: a newly allocated #GtkTreeIter from @model corresponding
 * to @findme which should be freed with gtk_tree_iter_free()
 * 
 */
GtkTreeIter *
glade_util_find_iter_by_widget (GtkTreeModel *model,
				GladeWidget  *findme,
				gint          column)
{
	GtkTreeIter iter;
	if (gtk_tree_model_get_iter_first (model, &iter))
	{
		return glade_util_find_iter (model, &iter, findme, column);
	}
	return NULL;
}
