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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Chema Celorio <chema@celorio.com>
 */
 
#include <config.h>

/**
 * SECTION:glade-utils
 * @Title: Glade Utils
 * @Short_Description: Welcome to the zoo.
 *
 * This is where all of that really usefull miscalanious stuff lands up.
 */

#include "glade.h"
#include "glade-project.h"
#include "glade-command.h"
#include "glade-debug.h"
#include "glade-placeholder.h"
#include "glade-widget.h"
#include "glade-widget-adaptor.h"
#include "glade-property.h"
#include "glade-property-class.h"
#include "glade-clipboard.h"
#include "glade-fixed.h"

#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gmodule.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <errno.h>

#ifdef G_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

#define GLADE_UTIL_SELECTION_NODE_SIZE 7
#define GLADE_UTIL_COPY_BUFFSIZE       1024


/* List of widgets that have selection
 */
static GList *glade_util_selection = NULL;

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
 * @name: the name of the #GType - like 'GtkWidget' or a "get-type" function.
 * @have_func: function-name flag -- true if the name is a "get-type" function.
 *
 * Returns the type using the "get type" function name based on @name.  
 * If the @have_func flag is true,@name is used directly, otherwise the get-type 
 * function is contrived from @name then used.
 *
 * Returns: the new #GType
 */
GType
glade_util_get_type_from_name (const gchar *name, gboolean have_func)
{
	static GModule *allsymbols = NULL;
	GType (*get_type) ();
	GType type = 0;
	gchar  *func_name = (gchar*)name;
	
	if ((type = g_type_from_name (name)) == 0 &&
	    (have_func || (func_name = glade_util_compose_get_type_func (name)) != NULL))
	{
		
		if (!allsymbols)
			allsymbols = g_module_open (NULL, 0);

		if (g_module_symbol (allsymbols, func_name,
				      (gpointer) &get_type))
		{
			g_assert (get_type);
			type = get_type ();
		}
		else
		{
			g_warning (_("We could not find the symbol \"%s\""),
				   func_name);
		}
		g_free (func_name);
	}

	if (type == 0)
		g_warning(_("Could not get the type from \"%s\""), name);
	
	return type;
}

/**
 * glade_utils_get_pspec_from_funcname:
 * @funcname: the symbol name of a function to generate a #GParamSpec
 *
 * Returns: A #GParamSpec created by the delagate function
 *          specified by @funcname
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
		return NULL;
	}

	g_assert (get_pspec);
	pspec = get_pspec ();

	return pspec;
}

/**
 * glade_util_ui_message:
 * @parent: a #GtkWindow cast as a #GtkWidget
 * @type:   a #GladeUIMessageType
 * @widget: a #GtkWidget to append to the dialog vbox
 * @format: a printf style format string
 * @...:    args for the format.
 *
 * Creates a new warning dialog window as a child of @parent containing
 * the text of @format, runs it, then destroys it on close. Depending
 * on @type, a cancel button may apear or the icon may change.
 *
 * Returns: True if the @type was GLADE_UI_ARE_YOU_SURE and the user
 *          selected "OK", True if the @type was GLADE_UI_YES_OR_NO and
 *          the user selected "YES"; False otherwise.
 */
gint
glade_util_ui_message (GtkWidget           *parent, 
		       GladeUIMessageType   type,
		       GtkWidget           *widget,
		       const gchar         *format,
		       ...)
{
	GtkWidget      *dialog;
	GtkMessageType  message_type = GTK_MESSAGE_INFO;
	GtkButtonsType  buttons_type = GTK_BUTTONS_OK;
	va_list         args;
	gchar          *string;
	gint            response;

	va_start (args, format);
	string = g_strdup_vprintf (format, args);
	va_end (args);

	/* Get message_type */
	switch (type)
	{
	case GLADE_UI_INFO: message_type = GTK_MESSAGE_INFO; break;
	case GLADE_UI_WARN:
	case GLADE_UI_ARE_YOU_SURE:
		message_type = GTK_MESSAGE_WARNING;
		break;
	case GLADE_UI_ERROR:      message_type = GTK_MESSAGE_ERROR;    break;
	case GLADE_UI_YES_OR_NO:  message_type = GTK_MESSAGE_QUESTION; break;
		break;
	default: 
		g_critical ("Bad arg for glade_util_ui_message");
		break;
	}


	/* Get buttons_type */
	switch (type)
	{
	case GLADE_UI_INFO:
	case GLADE_UI_WARN:
	case GLADE_UI_ERROR:
		buttons_type = GTK_BUTTONS_OK;
		break;
	case GLADE_UI_ARE_YOU_SURE: buttons_type = GTK_BUTTONS_OK_CANCEL; break;
	case GLADE_UI_YES_OR_NO:    buttons_type = GTK_BUTTONS_YES_NO; break;
		break;
	default: 
		g_critical ("Bad arg for glade_util_ui_message");
		break;
	}

	dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 message_type, buttons_type, NULL);

	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), string);

	if (widget)
	{
		gtk_box_pack_end (GTK_BOX
						 (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
						 widget, TRUE, TRUE, 2);
		gtk_widget_show (widget);
	}

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
	g_free (string);

	return (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_YES);
}


gboolean
glade_util_check_and_warn_scrollable (GladeWidget        *parent,
				      GladeWidgetAdaptor *child_adaptor,
				      GtkWidget          *parent_widget)
{
	if (GTK_IS_SCROLLED_WINDOW (parent->object) &&
	    GWA_SCROLLABLE_WIDGET (child_adaptor) == FALSE)
	{
		GladeWidgetAdaptor *vadaptor = 
			glade_widget_adaptor_get_by_type (GTK_TYPE_VIEWPORT);

		glade_util_ui_message (parent_widget,
				       GLADE_UI_INFO, NULL,
				       _("Cannot add non scrollable %s widget to a %s directly.\n"
					 "Add a %s first."),
				       child_adaptor->title,
				       parent->adaptor->title,
				       vadaptor->title);
		return TRUE;
	}
	return FALSE;
}

typedef struct {
	GtkStatusbar *statusbar;
	guint context_id;
	guint message_id;
} FlashInfo;

static const guint flash_length = 3;

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

	g_timeout_add_seconds (flash_length, (GtkFunction) remove_message_timeout, fi);

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

	ltext = (gchar *) g_object_get_data (G_OBJECT (listitem),
					     gtk_combo_string_key);
	if (!ltext) {
		label = gtk_bin_get_child (GTK_BIN (listitem));
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
	gsize len;

	int (*string_compare) (const char *, const char *, gsize);

	if (combo->case_sensitive)
		string_compare = strncmp;
	else
		string_compare = g_ascii_strncasecmp;

	text  = (gchar*) gtk_entry_get_text (GTK_ENTRY (combo->entry));
	len   = text ? strlen (text) : 0;
	clist = GTK_LIST (combo->list)->children;

	while (clist && clist->data) {
		ltext = glade_util_gtk_combo_func (GTK_LIST_ITEM (clist->data));
		if (!ltext)
			continue;
		if (!(*string_compare) (ltext, text, len))
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


static void
format_libglade_button_clicked (GtkWidget *widget,
				GladeProject *project)
{
	glade_project_set_format (project, GLADE_PROJECT_FORMAT_LIBGLADE);
}

static void
format_builder_button_clicked (GtkWidget *widget,
			       GladeProject *project)
{
	glade_project_set_format (project, GLADE_PROJECT_FORMAT_GTKBUILDER);
}

static void
add_format_options (GtkDialog    *dialog,
		    GladeProject *project)
{
	GtkWidget *vbox, *frame;
	GtkWidget *glade_radio, *builder_radio;
	GtkWidget *label, *alignment;
	gchar     *string = g_strdup_printf ("<b>%s</b>", _("File format"));

	frame = gtk_frame_new (NULL);
	vbox = gtk_vbox_new (FALSE, 0);
	alignment = gtk_alignment_new (0.5F, 0.5F, 1.0F, 1.0F);

	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 2, 0, 12, 0);

	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);

	label = gtk_label_new (string);
	g_free (string);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

	glade_radio = gtk_radio_button_new_with_label (NULL, "Libglade");
	builder_radio = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON (glade_radio), "GtkBuilder");

	if (glade_project_get_format (project) == GLADE_PROJECT_FORMAT_GTKBUILDER)
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (builder_radio), TRUE);
	else
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (glade_radio), TRUE);

	g_signal_connect (G_OBJECT (glade_radio), "clicked",
			  G_CALLBACK (format_libglade_button_clicked), project);

	g_signal_connect (G_OBJECT (builder_radio), "clicked",
			  G_CALLBACK (format_builder_button_clicked), project);

	gtk_box_pack_start (GTK_BOX (vbox), builder_radio, TRUE, TRUE, 2);
	gtk_box_pack_start (GTK_BOX (vbox), glade_radio, TRUE, TRUE, 2);

	gtk_frame_set_label_widget (GTK_FRAME (frame), label);
	gtk_container_add (GTK_CONTAINER (alignment), vbox);
	gtk_container_add (GTK_CONTAINER (frame), alignment);

	gtk_widget_show_all (frame);
	
	gtk_box_pack_end (GTK_BOX (gtk_dialog_get_content_area (dialog)), frame, FALSE, TRUE, 2);
}


/**
 * glade_util_file_dialog_new:
 * @title: dialog title
 * @project: a #GladeProject used when saving
 * @parent: a parent #GtkWindow for the dialog
 * @action: a #GladeUtilFileDialogType to say if the dialog will open or save
 *
 * Returns: a "glade file" file chooser dialog. The caller is responsible 
 *          for showing the dialog
 */
GtkWidget *
glade_util_file_dialog_new (const gchar             *title, 
			    GladeProject            *project,
			    GtkWindow               *parent, 
			    GladeUtilFileDialogType  action)
{
	GtkWidget *file_dialog;
	GtkFileFilter *file_filter;

	g_return_val_if_fail ((action == GLADE_FILE_DIALOG_ACTION_OPEN ||
			       action == GLADE_FILE_DIALOG_ACTION_SAVE), NULL);
	
	g_return_val_if_fail ((action != GLADE_FILE_DIALOG_ACTION_SAVE ||
			       GLADE_IS_PROJECT (project)), NULL);

	file_dialog = gtk_file_chooser_dialog_new (title, parent, action,
						   GTK_STOCK_CANCEL,
						   GTK_RESPONSE_CANCEL,
						   action == GLADE_FILE_DIALOG_ACTION_OPEN ?
						   GTK_STOCK_OPEN : GTK_STOCK_SAVE,
						   GTK_RESPONSE_OK,
						   NULL);


	if (action == GLADE_FILE_DIALOG_ACTION_SAVE)
		add_format_options (GTK_DIALOG (file_dialog), project);
	
	file_filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (file_filter, "*");
	gtk_file_filter_set_name (file_filter, _("All Files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_dialog), file_filter);

	file_filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (file_filter, "*.glade");
	gtk_file_filter_set_name (file_filter, _("Libglade Files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_dialog), file_filter);

	file_filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (file_filter, "*.ui");
	gtk_file_filter_set_name (file_filter, _("GtkBuilder Files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_dialog), file_filter);

	file_filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (file_filter, "*.ui");
	gtk_file_filter_add_pattern (file_filter, "*.glade");
	gtk_file_filter_set_name (file_filter, _("All Glade Files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_dialog), file_filter);

	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (file_dialog), file_filter);

	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (file_dialog), TRUE);
	gtk_dialog_set_default_response (GTK_DIALOG (file_dialog), GTK_RESPONSE_OK);

	return file_dialog;
}

/**
 * glade_util_replace:
 * @str: a string
 * @a: a #gchar
 * @b: a #gchar
 *
 * Replaces each occurance of the character @a in @str to @b.
 */
void
glade_util_replace (gchar *str, gchar a, gchar b)
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
 * glade_util_read_prop_name:
 * @str: a string
 *
 * Return a usable version of a property identifier as found
 * in a freshly parserd #GladeInterface
 */
gchar *
glade_util_read_prop_name (const gchar *str)
{
	gchar *id;

	g_return_val_if_fail (str != NULL, NULL);

	id = g_strdup (str);

	glade_util_replace (id, '_', '-');

	return id;
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
gchar *
glade_util_duplicate_underscores (const gchar *name)
{
	const gchar *tmp;
	const gchar *last_tmp = name;
	gchar *underscored_name = g_malloc (strlen (name) * 2 + 1);
	gchar *tmp_underscored = underscored_name;

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

	parent = gtk_widget_get_parent (widget);

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
		return gtk_widget_get_window (parent);

	return gtk_widget_get_window (widget);
}

static void
glade_util_draw_nodes (cairo_t *cr, GdkColor *color,
		       gint x, gint y,
		       gint width, gint height)
{
	if (width > GLADE_UTIL_SELECTION_NODE_SIZE && height > GLADE_UTIL_SELECTION_NODE_SIZE) {
		glade_utils_cairo_draw_rectangle (cr, color, TRUE,
						  x, y,
						  GLADE_UTIL_SELECTION_NODE_SIZE,
						  GLADE_UTIL_SELECTION_NODE_SIZE);
		glade_utils_cairo_draw_rectangle (cr, color, TRUE,
						  x, y + height - GLADE_UTIL_SELECTION_NODE_SIZE,
						  GLADE_UTIL_SELECTION_NODE_SIZE,
						  GLADE_UTIL_SELECTION_NODE_SIZE);
		glade_utils_cairo_draw_rectangle (cr, color, TRUE,
						  x + width - GLADE_UTIL_SELECTION_NODE_SIZE, y,
						  GLADE_UTIL_SELECTION_NODE_SIZE,
						  GLADE_UTIL_SELECTION_NODE_SIZE);
		glade_utils_cairo_draw_rectangle (cr, color, TRUE,
						  x + width - GLADE_UTIL_SELECTION_NODE_SIZE,
						  y + height - GLADE_UTIL_SELECTION_NODE_SIZE,
						  GLADE_UTIL_SELECTION_NODE_SIZE,
						  GLADE_UTIL_SELECTION_NODE_SIZE);
	}

	glade_utils_cairo_draw_rectangle (cr, color, FALSE, x, y, width - 1, height - 1);
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
	for (widget = gtk_widget_get_parent (sel_widget); widget; widget = gtk_widget_get_parent (widget)) {
		if (GTK_IS_VIEWPORT (widget)) {
			viewport = widget;
			viewport_win = gtk_viewport_get_bin_window (GTK_VIEWPORT (widget));
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
 * glade_util_draw_selection_nodes:
 * @expose_win: a #GdkWindow
 *
 * Redraws any selection nodes that intersect @expose_win. Steps through all
 * selected widgets, finds their coordinates, and calls glade_util_draw_nodes()
 * if appropriate.
 *
 */
void
glade_util_draw_selection_nodes (GdkWindow *expose_win)
{
	GtkWidget *expose_widget;
	gint expose_win_x, expose_win_y;
	gint expose_win_w, expose_win_h;
	GdkWindow   *expose_toplevel;
	GdkColor *color;
	GList *elem;
	cairo_t *cr;

	g_return_if_fail (GDK_IS_WINDOW (expose_win));

	/* Find the corresponding GtkWidget */
	gdk_window_get_user_data (expose_win, (gpointer)&expose_widget);

	color = &(gtk_widget_get_style (expose_widget)->black);

	/* Calculate the offset of the expose window within its toplevel. */
	glade_util_calculate_window_offset (expose_win,
					    &expose_win_x,
					    &expose_win_y,
					    &expose_toplevel);

	gdk_drawable_get_size (expose_win,
			       &expose_win_w, &expose_win_h);

	cr = gdk_cairo_create (expose_win);

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
			GtkAllocation allocation;

			gtk_widget_get_allocation (sel_widget, &allocation);
			x = sel_x + allocation.x - expose_win_x;
			y = sel_y + allocation.y - expose_win_y;
			w = allocation.width;
			h = allocation.height;

			/* Draw the selection nodes if they intersect the
			   expose window bounds. */
			if (x < expose_win_w && x + w >= 0
			    && y < expose_win_h && y + h >= 0) {
				glade_util_draw_nodes (cr, color, x, y, w, h);
			}
		}
	}

	cairo_destroy (cr);
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
	GtkWidget *parent;

	g_return_if_fail (GTK_IS_WIDGET (widget));
	if (!glade_util_has_selection (widget))
		return;

	glade_util_selection = 
		g_list_remove (glade_util_selection, widget);

	/* We redraw the parent, since the selection rectangle may not be
	   cleared if we just redraw the widget itself. */
	parent = gtk_widget_get_parent (widget);
	gtk_widget_queue_draw (parent ? parent : widget);
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
	GtkWidget *parent;
	GList     *list;

	for (list = glade_util_selection;
	     list && list->data;
	     list = list->next)
	{
		widget = list->data;
		parent = gtk_widget_get_parent (widget);
		gtk_widget_queue_draw (parent ? parent : widget);
	}
	glade_util_selection =
		(g_list_free (glade_util_selection), NULL);
}

/**
 * glade_util_has_selection:
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
 * Use this to itterate over all children in a GtkContainer,
 * as it used _forall() instead of _foreach() (and the GTK+ version
 * of this function is simply not exposed).
 *
 * Note that glade_widget_class_get_children() is the high-level
 * abstraction and will usually end up calling this function.
 *
 * Returns: a #GList giving the contents of @container
 */
GList *
glade_util_container_get_all_children (GtkContainer *container)
{
	GList *children = NULL;

	g_return_val_if_fail (GTK_IS_CONTAINER (container), NULL);

	gtk_container_forall (container,
			      gtk_container_children_callback,
			      &children);

	/* Preserve the natural order by reversing the list */
	return g_list_reverse (children);
}

/**
 * glade_util_count_placeholders:
 * @parent: a #GladeWidget
 *
 * Returns: the amount of #GladePlaceholders parented by @parent
 */
gint
glade_util_count_placeholders (GladeWidget *parent)
{
	gint placeholders = 0;
	GList *list, *children;

	/* count placeholders */
	if ((children = glade_widget_adaptor_get_children
	     (parent->adaptor, parent->object)) != NULL)
	{
		for (list = children; list && list->data; list = list->next)
		{
			if (GLADE_IS_PLACEHOLDER (list->data))
				placeholders++;
		}
		g_list_free (children);
	}

	return placeholders;
}

static GtkTreeIter *
glade_util_find_iter (GtkTreeModel *model,
		      GtkTreeIter  *iter,
		      GladeWidget  *findme,
		      gint          column)
{
	GtkTreeIter *retval = NULL;
	GObject* object = NULL;
	GtkTreeIter *next;

	g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);
	g_return_val_if_fail (iter != NULL, NULL);

	next = gtk_tree_iter_copy (iter);
	g_return_val_if_fail (next != NULL, NULL);

	while (retval == NULL)
	{
		GladeWidget *widget;

		gtk_tree_model_get (model, next, column, &object, -1);
		if (object &&
		    gtk_tree_model_get_column_type (model, column) == G_TYPE_OBJECT)
			g_object_unref (object);

		widget = glade_widget_get_from_gobject (object);

		if (widget == findme)
		{
			retval = gtk_tree_iter_copy (next);
			break;
		}
		else if (glade_widget_is_ancestor (findme, widget))
		{
			if (gtk_tree_model_iter_has_child (model, next))
			{
				GtkTreeIter  child;
				gtk_tree_model_iter_children (model, &child, next);
				if ((retval = glade_util_find_iter
				     (model, &child, findme, column)) != NULL)
					break;
			}

			/* Only search the branches where the searched widget
			 * is actually a child of the this row, optimize the
			 * searching this way
			 */
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

gboolean
glade_util_basenames_match  (const gchar  *path1,
			     const gchar  *path2)
{
	gboolean match = FALSE;
	gchar *bname1;
	gchar *bname2;
	
	if (path1 && path2)
	{
		bname1 = g_path_get_basename (path1);
		bname2 = g_path_get_basename (path2);

		match = !strcmp (bname1, bname2);
		
		g_free (bname1);
		g_free (bname2);
	}
	return match;
}

/**
 * glade_util_purify_list:
 * @list: A #GList
 *
 * Returns: A newly allocated version of @list with no 
 *          duplicate data entries
 */
GList *
glade_util_purify_list (GList *list)
{
	GList *l, *newlist = NULL;

	for (l = list; l; l = l->next)
		if (!g_list_find (newlist, l->data))
			newlist = g_list_prepend (newlist, l->data);

	g_list_free (list);

	return g_list_reverse (newlist);
}

/**
 * glade_util_added_in_list:
 * @old_list: the old #GList
 * @new_list: the new #GList
 *
 * Returns: A newly allocated #GList of elements that
 *          are in @new but not in @old
 *
 */
GList *
glade_util_added_in_list (GList *old_list,
			  GList *new_list)
{
	GList *added = NULL, *list;

	for (list = new_list; list; list = list->next)
	{
		if (!g_list_find (old_list, list->data))
			added = g_list_prepend (added, list->data);
	} 

	return g_list_reverse (added);
}

/**
 * glade_util_removed_from_list:
 * @old_list: the old #GList
 * @new_list: the new #GList
 *
 * Returns: A newly allocated #GList of elements that
 *          are in @old no longer in @new
 *
 */
GList *
glade_util_removed_from_list (GList *old_list,
			      GList *new_list)
{
	GList *added = NULL, *list;

	for (list = old_list; list; list = list->next)
	{
		if (!g_list_find (new_list, list->data))
			added = g_list_prepend (added, list->data);
	} 

	return g_list_reverse (added);
}


/**
 * glade_util_canonical_path:
 * @path: any path that may contain ".." or "." components
 *
 * Returns: an absolute path to the specified file or directory
 *          that contains no ".." or "." components (this does
 *          not call readlink like realpath() does).
 *
 * Note: on some systems; I think its possible that we dont have
 *       permission to execute in the directory in which the glade
 *       file resides; I decided finally to do it this way anyway
 *       since libc's realpath() does exactly the same.
 */
gchar *
glade_util_canonical_path (const gchar *path)
{
	gchar *orig_dir, *dirname, *basename, *direct_dir, *direct_name = NULL;

	g_return_val_if_fail (path != NULL, NULL);

	basename = g_path_get_basename (path);

	if ((orig_dir = g_get_current_dir ()) != NULL)
	{
		if ((dirname = g_path_get_dirname (path)) != NULL)
		{
			if (g_chdir (dirname) == 0)
			{
				if ((direct_dir = g_get_current_dir ()) != NULL)
					direct_name = 
						g_build_filename (direct_dir, basename, NULL);
				else
					g_warning ("g_path");

				if (g_chdir (orig_dir) != 0)
					g_warning ("Unable to chdir back to %s directory (%s)",
						   orig_dir, g_strerror (errno));

			} else g_warning ("Unable to chdir to %s directory (%s)", 
					  dirname, g_strerror (errno));

			g_free (dirname);
		} else g_warning ("Unable to get directory component of %s\n", path);
		g_free (orig_dir);
	}

	if (basename) g_free (basename);

	return direct_name;
}

static gboolean
glade_util_canonical_match (const gchar  *src_path,
			    const gchar  *dest_path)
{
	gchar      *canonical_src, *canonical_dest;
	gboolean    match;
	canonical_src  = glade_util_canonical_path (src_path);
	canonical_dest = glade_util_canonical_path (dest_path);

	match = (strcmp (canonical_src, canonical_dest) == 0);

	g_free (canonical_src);
	g_free (canonical_dest);
	
	return match;
}

/**
 * glade_util_copy_file:
 * @src_path:  the path to the source file
 * @dest_path: the path to the destination file to create or overwrite.
 *
 * Copies a file from @src to @dest, queries the user
 * if it involves overwriting the target and displays an
 * error message upon failure.
 *
 * Returns: True if the copy was successfull.
 */
gboolean
glade_util_copy_file (const gchar  *src_path,
		      const gchar  *dest_path)
{
	GIOChannel *src, *dest;
	GError     *error = NULL;
	GIOStatus   read_status, write_status = G_IO_STATUS_ERROR;
	gchar       buffer[GLADE_UTIL_COPY_BUFFSIZE];
	gsize       bytes_read, bytes_written, written;
	gboolean    success = FALSE;

	/* FIXME: This may break if src_path & dest_path are actually 
	 * the same file, right now the canonical comparison is the
	 * best check I have.
	 */
	if (glade_util_canonical_match (src_path, dest_path))
		return FALSE;

	if (g_file_test (dest_path, G_FILE_TEST_IS_REGULAR) != FALSE)
		if (glade_util_ui_message
		    (glade_app_get_window(), GLADE_UI_YES_OR_NO, NULL,
		     _("%s exists.\nDo you want to replace it?"), dest_path) == FALSE)
		    return FALSE;


	if ((src = g_io_channel_new_file (src_path, "r", &error)) != NULL)
	{
		g_io_channel_set_encoding (src, NULL, NULL);

		if ((dest = g_io_channel_new_file (dest_path, "w", &error)) != NULL)
		{
			g_io_channel_set_encoding (dest, NULL, NULL);

			while ((read_status = g_io_channel_read_chars 
				(src, buffer, GLADE_UTIL_COPY_BUFFSIZE,
				 &bytes_read, &error)) != G_IO_STATUS_ERROR)
			{
				bytes_written = 0;
				while ((write_status = g_io_channel_write_chars 
					(dest, buffer + bytes_written, 
					 bytes_read - bytes_written, 
					 &written, &error)) != G_IO_STATUS_ERROR &&
				       (bytes_written + written) < bytes_read)
					bytes_written += written;
				
				if (write_status == G_IO_STATUS_ERROR)
				{
					glade_util_ui_message (glade_app_get_window(),
							       GLADE_UI_ERROR, NULL,
							       _("Error writing to %s: %s"),
							       dest_path, error->message);
					error = (g_error_free (error), NULL);
					break;
				}

				/* Break on EOF & ERROR but not AGAIN and not NORMAL */
				if (read_status == G_IO_STATUS_EOF) break;
			}
			
			if (read_status == G_IO_STATUS_ERROR)
			{
				glade_util_ui_message (glade_app_get_window(),
						       GLADE_UI_ERROR, NULL,
						       _("Error reading %s: %s"),
						       src_path, error->message);
				error = (g_error_free (error), NULL);
			}


			/* From here on, unless we have problems shutting down, succuss ! */
			success = (read_status == G_IO_STATUS_EOF && 
				   write_status == G_IO_STATUS_NORMAL);

			if (g_io_channel_shutdown (dest, TRUE, &error) != G_IO_STATUS_NORMAL)
			{
				glade_util_ui_message
					(glade_app_get_window(),
					 GLADE_UI_ERROR, NULL,
					 _("Error shutting down I/O channel %s: %s"),
						       dest_path, error->message);
				error = (g_error_free (error), NULL);
				success = FALSE;
			}
		}
		else
		{
			glade_util_ui_message (glade_app_get_window(),
					       GLADE_UI_ERROR, NULL,
					       _("Failed to open %s for writing: %s"), 
					       dest_path, error->message);
			error = (g_error_free (error), NULL);

		}


		if (g_io_channel_shutdown (src, TRUE, &error) != G_IO_STATUS_NORMAL)
		{
			glade_util_ui_message (glade_app_get_window(),
					       GLADE_UI_ERROR, NULL,
					       _("Error shutting down I/O channel %s: %s"),
					       src_path, error->message);
			success = FALSE;
		}
	}
	else 
	{
		glade_util_ui_message (glade_app_get_window(),
				       GLADE_UI_ERROR, NULL,
				       _("Failed to open %s for reading: %s"), 
				       src_path, error->message);
		error = (g_error_free (error), NULL);
	}
	return success;
}

/**
 * glade_util_class_implements_interface:
 * @class_type: A #GType
 * @iface_type: A #GType
 *
 * Returns: whether @class_type implements the @iface_type interface
 */
gboolean
glade_util_class_implements_interface (GType class_type, 
				       GType iface_type)
{
	GType    *ifaces;
	guint     n_ifaces, i;
	gboolean  implemented = FALSE;

	if ((ifaces = g_type_interfaces (class_type, &n_ifaces)) != NULL)
	{
		for (i = 0; i < n_ifaces; i++)
			if (ifaces[i] == iface_type)
			{
				implemented = TRUE;
				break;
			}
		g_free (ifaces);
	}
	return implemented;
}

static GModule *
try_load_library (const gchar *library_path,
		  const gchar *library_name)
{
	GModule *module = NULL;
	gchar   *path;

	path = g_module_build_path (library_path, library_name);
	if (g_file_test (path, G_FILE_TEST_EXISTS))
	{
		if (!(module = g_module_open (path, G_MODULE_BIND_LAZY)))
			g_warning ("Failed to load %s: %s", path, g_module_error ());
	}
	g_free (path);

	return module;
}

/**
 * glade_util_load_library:
 * @library_name: name of the library
 *
 * Loads the named library from the Glade modules directory, or failing that
 * from the standard platform specific directories.
 *
 * The @library_name should not include any platform specifix prefix or suffix,
 * those are automatically added, if needed, by g_module_build_path()
 *
 * Returns: a #GModule on success, or %NULL on failure.
 */
GModule *
glade_util_load_library (const gchar *library_name)
{
	gchar        *default_paths[] = { (gchar *)glade_app_get_modules_dir (), 
					  NULL, /* <-- dynamically allocated */ 
					  "/lib", 
					  "/usr/lib", 
					  "/usr/local/lib", 
					  NULL };

	GModule      *module = NULL;
	const gchar  *search_path;
	gchar       **split;
	gint          i;

	

	if ((search_path = g_getenv (GLADE_ENV_MODULE_PATH)) != NULL)
	{
		if ((split = g_strsplit (search_path, ":", 0)) != NULL)
		{
			for (i = 0; split[i] != NULL; i++)
				if ((module = try_load_library (split[i], library_name)) != NULL)
					break;

			g_strfreev (split);
		}
	}

	if (!module)
	{
		/* Search ${prefix}/lib after searching ${prefix}/lib/glade3/modules... */
		default_paths[1] = g_build_filename (glade_app_get_modules_dir (), "..", "..", NULL);

		for (i = 0; default_paths[i] != NULL; i++)
			if ((module = try_load_library (default_paths[i], library_name)) != NULL)
				break;

		g_free (default_paths[1]);
	}

	if (!module)
		g_critical ("Unable to load module '%s' from any search paths", library_name);
	
	return module;
}

/**
 * glade_util_file_is_writeable:
 * @path:  the path to the file
 *
 * Checks whether the file at @path is writeable
 *
 * Returns: TRUE if file is writeable
 */
gboolean
glade_util_file_is_writeable (const gchar *path)
{
	GIOChannel *channel;
	g_return_val_if_fail (path != NULL, FALSE);

	/* The only way to really know if the file is writable */
	if ((channel = g_io_channel_new_file (path, "a+", NULL)) != NULL)
	{
		g_io_channel_unref (channel);
		return TRUE;
	}
	return FALSE;
}

/**
 * glade_util_have_devhelp:
 *
 * Returns: whether the devhelp module is loaded
 */
gboolean
glade_util_have_devhelp (void)
{
	static gint  have_devhelp = -1;
	gchar       *ptr;
	gint         cnt, ret, major, minor;
	GError      *error = NULL;

#define DEVHELP_OLD_MESSAGE  \
    "The DevHelp installed on your system is too old, " \
    "devhelp feature will be disabled."

#define DEVHELP_MISSING_MESSAGE  \
    "No DevHelp installed on your system, " \
    "devhelp feature will be disabled."

	if (have_devhelp >= 0) return have_devhelp;
	
	have_devhelp = 0;

	if ((ptr = g_find_program_in_path ("devhelp")) != NULL)
	{
		g_free (ptr);

		if (g_spawn_command_line_sync ("devhelp --version", 
					       &ptr, NULL, &ret, &error))
		{
			/* If we have a successfull return code.. parse the output.
			 */
			if (ret == 0)
			{
				gchar name[16];
				if ((cnt = sscanf (ptr, "%15s %d.%d\n", 
						   name, &major, &minor)) == 3)
				{
					/* Devhelp 0.12 required.
					 */
					if (major >= 2 || (major >= 0 && minor >= 12))
						have_devhelp = 1;
					else	
						g_message (DEVHELP_OLD_MESSAGE);
				}
				else
					
				{
					if (ptr != NULL || strlen (ptr) > 0)
						g_warning ("devhelp had unparsable output: "
							   "'%s' (parsed %d elements)", ptr, cnt);
					else
						g_message (DEVHELP_OLD_MESSAGE);
				}
			}
			else g_warning ("devhelp had bad return code: '%d'", ret);
		}
		else
		{
			g_warning ("Error trying to launch devhelp: %s",
				   error->message);
			g_error_free (error);
		}
	}
	else g_message (DEVHELP_MISSING_MESSAGE);
	
	return have_devhelp;
}

/**
 * glade_util_get_devhelp_icon:
 * @size: the preferred icon size
 *
 * Creates an image displaying the devhelp icon.
 *
 * Returns: a #GtkImage
 */
GtkWidget*
glade_util_get_devhelp_icon (GtkIconSize size)
{
	GtkIconTheme *icon_theme;
	GdkScreen *screen;
	GtkWidget *image;
	gchar *path;

	image = gtk_image_new ();
	screen = gtk_widget_get_screen (GTK_WIDGET (image));
	icon_theme = gtk_icon_theme_get_for_screen (screen);

	if (gtk_icon_theme_has_icon (icon_theme, GLADE_DEVHELP_ICON_NAME))
	{
		gtk_image_set_from_icon_name (GTK_IMAGE (image), GLADE_DEVHELP_ICON_NAME, size);
	}
	else
	{
		path = g_build_filename (glade_app_get_pixmaps_dir (), GLADE_DEVHELP_FALLBACK_ICON_FILE, NULL);

		gtk_image_set_from_file (GTK_IMAGE (image), path);

		g_free (path);
	}

	return image;
}

/**
 * glade_util_search_devhep:
 * @devhelp: the devhelp widget created by the devhelp module.
 * @book: the devhelp book (or %NULL)
 * @page: the page in the book (or %NULL)
 * @search: the search string (or %NULL)
 *
 * Envokes devhelp with the appropriate search string
 *
 */
void
glade_util_search_devhelp (const gchar *book,
			   const gchar *page,
			   const gchar *search)
{
	GError *error = NULL;
	gchar  *book_comm = NULL, *page_comm = NULL, *search_comm = NULL;
	gchar  *string;

	g_return_if_fail (glade_util_have_devhelp ());

	if (book) book_comm = g_strdup_printf ("book:%s", book);
	if (page) page_comm = g_strdup_printf (" page:%s", page);
	if (search) search_comm = g_strdup_printf (" %s", search);

	string = g_strdup_printf ("devhelp -s \"%s%s%s\"", 
				   book_comm ? book_comm : "",
				   page_comm ? page_comm : "",
				   search_comm ? search_comm : "");

	if (g_spawn_command_line_async (string, &error) == FALSE)
	{
		g_warning ("Error envoking devhelp: %s", error->message);
		g_error_free (error);
	}

	g_free (string);
	if (book_comm) g_free (book_comm);
	if (page_comm) g_free (page_comm);
	if (search_comm) g_free (search_comm);
}

GtkWidget *
glade_util_get_placeholder_from_pointer (GtkContainer *container)
{
	GtkWidget *toplevel;
	GtkWidget *retval = NULL, *child;
	GtkAllocation allocation;
	GList *c, *l;
	gint x, y, x2, y2;

	g_return_val_if_fail (GTK_IS_CONTAINER (container), NULL);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (container));

	gtk_widget_get_pointer (toplevel, &x, &y);
	
	for (c = l = glade_util_container_get_all_children (container);
	     l;
	     l = g_list_next (l))
	{
		child = l->data;
		
		if (GLADE_IS_PLACEHOLDER (child) &&
		    gtk_widget_get_mapped (child))
		{
			gtk_widget_translate_coordinates (toplevel, child,
							  x, y, &x2, &y2);

			gtk_widget_get_allocation (child, &allocation);
			if (x2 >= 0 && y2 >= 0 &&
			    x2 <= allocation.width &&
			    y2 <= allocation.height)
			{
				retval = child;
				break;
			}
		}
	}
	
	g_list_free (c);

	return retval;
}

/**
 * glade_util_object_is_loading:
 * @object: A #GObject
 *
 * Returns: Whether the object's project is being loaded or not.
 *       
 */
gboolean
glade_util_object_is_loading (GObject *object)
{
	GladeProject *project;
	GladeWidget *widget;

	g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
	
	widget = glade_widget_get_from_gobject (object);
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
	
	project = glade_widget_get_project (widget);
	
	return project && glade_project_is_loading (project);
}

/**
 * glade_util_url_show:
 * @url: An URL to display
 *
 * Portable function for showing an URL @url in a web browser.
 *
 * Returns: TRUE if a web browser was successfully launched, or FALSE
 *
 */
gboolean
glade_util_url_show (const gchar *url)
{
	GtkWidget *widget;
	GError *error = NULL;
	gboolean ret;

	g_return_val_if_fail (url != NULL, FALSE);

	widget = glade_app_get_window ();

	ret = gtk_show_uri (gtk_widget_get_screen (widget),
	                    url,
	                    gtk_get_current_event_time (),
	                    &error);
	if (error != NULL)
	{
		GtkWidget *dialog_widget;

		dialog_widget = gtk_message_dialog_new (GTK_WINDOW (widget),
		                                        GTK_DIALOG_DESTROY_WITH_PARENT,
		                                        GTK_MESSAGE_ERROR,
		                                        GTK_BUTTONS_CLOSE,
		                                        "%s", _("Could not show link:"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog_widget),
		                                          "%s", error->message);
		g_error_free (error);

		g_signal_connect (dialog_widget, "response",
		                  G_CALLBACK (gtk_widget_destroy), NULL);

		gtk_window_present (GTK_WINDOW (dialog_widget));
	}

	return ret;
}

/**
 * glade_util_get_file_mtime:
 * @filename: A filename
 * @error: return location for errors
 *
 * Gets the UTC modification time of file @filename.
 *
 * Returns: The mtime of the file, or %0 if the file attributes
 *          could not be read.
 */
time_t
glade_util_get_file_mtime (const gchar *filename, GError **error)
{
	struct stat info;
	gint retval;
	
	retval = g_stat (filename, &info);
	
	if (retval != 0) {
		g_set_error (error,
		             G_FILE_ERROR,
		             g_file_error_from_errno (errno),
		             "could not stat file '%s': %s", filename, g_strerror (errno));    
		return (time_t) 0;
	} else {
		return info.st_mtime;
	}
}

gchar *
glade_util_filename_to_icon_name (const gchar *value)
{
	gchar *icon_name, *p;
	g_return_val_if_fail (value && value[0], NULL);

	icon_name = g_strdup_printf ("glade-generated-%s", value);

	if ((p = strrchr (icon_name, '.')) != NULL)
		*p = '-';
	
	return icon_name;
}

gchar *
glade_util_icon_name_to_filename (const gchar *value)
{
	/* sscanf makes us allocate a buffer */
	gchar filename[FILENAME_MAX], *p;
	g_return_val_if_fail (value && value[0], NULL);

	sscanf (value, "glade-generated-%s", filename);

	/* XXX: Filenames without an extention will evidently
	 * break here
	 */
	if ((p = strrchr (filename, '-')) != NULL)
		*p = '.';
	
	return g_strdup (filename);
}

gint
glade_utils_enum_value_from_string (GType enum_type, const gchar *strval)
{
	gint          value = 0;
	const gchar  *displayable;
	GValue       *gvalue;

	g_return_val_if_fail (strval && strval[0], 0);

	if (((displayable = glade_get_value_from_displayable (enum_type, strval)) != NULL &&
	     (gvalue = glade_utils_value_from_string (enum_type, displayable, NULL, NULL)) != NULL) ||
	    (gvalue = glade_utils_value_from_string (enum_type, strval, NULL, NULL)) != NULL)
	{
		value = g_value_get_enum (gvalue);
		g_value_unset (gvalue);
		g_free (gvalue);
	}
	return value;
}

static gchar *
glade_utils_enum_string_from_value_real (GType enum_type, gint value, gboolean displayable)
{
	GValue gvalue = { 0, };
	gchar *string;

	g_value_init (&gvalue, enum_type);
	g_value_set_enum (&gvalue, value);

	string = glade_utils_string_from_value (&gvalue, GLADE_PROJECT_FORMAT_GTKBUILDER);
	g_value_unset (&gvalue);

	if (displayable && string)
	{
		const gchar *dstring = glade_get_displayable_value (enum_type, string);
		if (dstring)
		{
			g_free (string);
			return g_strdup (dstring);
		}
	}

	return string;
}

gchar *
glade_utils_enum_string_from_value (GType enum_type, gint value)
{
	return glade_utils_enum_string_from_value_real (enum_type, value, FALSE);
}

gchar *
glade_utils_enum_string_from_value_displayable (GType enum_type, gint value)
{
	return glade_utils_enum_string_from_value_real (enum_type, value, TRUE);
}


gint
glade_utils_flags_value_from_string (GType flags_type, const gchar *strval)
{
	gint          value = 0;
	const gchar  *displayable;
	GValue       *gvalue;

	g_return_val_if_fail (strval && strval[0], 0);

	if (((displayable = glade_get_value_from_displayable (flags_type, strval)) != NULL &&
	     (gvalue = glade_utils_value_from_string (flags_type, displayable, NULL, NULL)) != NULL) ||
	    (gvalue = glade_utils_value_from_string (flags_type, strval, NULL, NULL)) != NULL)
	{
		value = g_value_get_flags (gvalue);
		g_value_unset (gvalue);
		g_free (gvalue);
	}
	return value;
}

static gchar *
glade_utils_flags_string_from_value_real (GType flags_type, gint value, gboolean displayable)
{
	GValue gvalue = { 0, };
	gchar *string;

	g_value_init (&gvalue, flags_type);
	g_value_set_flags (&gvalue, value);

	string = glade_utils_string_from_value (&gvalue, GLADE_PROJECT_FORMAT_GTKBUILDER);
	g_value_unset (&gvalue);

	if (displayable && string)
	{
		const gchar *dstring = glade_get_displayable_value (flags_type, string);
		if (dstring)
		{
			g_free (string);
			return g_strdup (dstring);
		}
	}

	return string;
}

gchar *
glade_utils_flags_string_from_value (GType flags_type, gint value)
{
	return glade_utils_flags_string_from_value_real (flags_type, value, FALSE);

}


gchar *
glade_utils_flags_string_from_value_displayable (GType flags_type, gint value)
{
	return glade_utils_flags_string_from_value_real (flags_type, value, TRUE);
}


/* A hash table of generically created property classes for
 * fundamental types, so we can easily use glade's conversion
 * system without using properties (only GTypes)
 */
static GHashTable *generic_property_classes = NULL;


static gboolean
utils_gtype_equal (gconstpointer v1,
		 gconstpointer v2)
{
  return *((const GType*) v1) == *((const GType*) v2);
}

static guint
utils_gtype_hash (gconstpointer v)
{
  return *(const GType*) v;
}


static GladePropertyClass *
pclass_from_gtype (GType type)
{
	GladePropertyClass *property_class = NULL;
	GParamSpec         *pspec = NULL;

	if (!generic_property_classes)
		generic_property_classes = g_hash_table_new_full (utils_gtype_hash, utils_gtype_equal,
								  g_free, (GDestroyNotify)glade_property_class_free);
	
	property_class = g_hash_table_lookup (generic_property_classes, &type);

	if (!property_class)
	{
		/* Support enum and flag types, and a hardcoded list of fundamental types */
		if (type == G_TYPE_CHAR)
			pspec = g_param_spec_char ("dummy", "dummy", "dummy",
						   G_MININT8, G_MAXINT8, 0, G_PARAM_READABLE|G_PARAM_WRITABLE);
		else if (type == G_TYPE_UCHAR)
			pspec = g_param_spec_char ("dummy", "dummy", "dummy",
						   0, G_MAXUINT8, 0, G_PARAM_READABLE|G_PARAM_WRITABLE);
		else if (type == G_TYPE_BOOLEAN)
			pspec = g_param_spec_boolean ("dummy", "dummy", "dummy",
						      FALSE, G_PARAM_READABLE|G_PARAM_WRITABLE);
		else if (type == G_TYPE_INT)
			pspec = g_param_spec_int ("dummy", "dummy", "dummy",
						  G_MININT, G_MAXINT, 0, G_PARAM_READABLE|G_PARAM_WRITABLE);
		else if (type == G_TYPE_UINT)
			pspec = g_param_spec_uint ("dummy", "dummy", "dummy",
						   0, G_MAXUINT, 0, G_PARAM_READABLE|G_PARAM_WRITABLE);
		else if (type == G_TYPE_LONG)
			pspec = g_param_spec_long ("dummy", "dummy", "dummy",
						    G_MINLONG, G_MAXLONG, 0, G_PARAM_READABLE|G_PARAM_WRITABLE);
		else if (type == G_TYPE_ULONG)
			pspec = g_param_spec_ulong ("dummy", "dummy", "dummy",
						    0, G_MAXULONG, 0, G_PARAM_READABLE|G_PARAM_WRITABLE);
		else if (type == G_TYPE_INT64)
			pspec = g_param_spec_int64 ("dummy", "dummy", "dummy",
						    G_MININT64, G_MAXINT64, 0, G_PARAM_READABLE|G_PARAM_WRITABLE);
		else if (type == G_TYPE_UINT64)
			pspec = g_param_spec_uint64 ("dummy", "dummy", "dummy",
						     0, G_MAXUINT64, 0, G_PARAM_READABLE|G_PARAM_WRITABLE);
		else if (type == G_TYPE_FLOAT)
			pspec = g_param_spec_float ("dummy", "dummy", "dummy",
						    G_MINFLOAT, G_MAXFLOAT, 1.0F, G_PARAM_READABLE|G_PARAM_WRITABLE);
		else if (type == G_TYPE_DOUBLE)
			pspec = g_param_spec_double ("dummy", "dummy", "dummy",
						     G_MINDOUBLE, G_MAXDOUBLE, 1.0F, G_PARAM_READABLE|G_PARAM_WRITABLE);
		else if (type == G_TYPE_STRING)
			pspec = g_param_spec_string ("dummy", "dummy", "dummy",
						     NULL, G_PARAM_READABLE|G_PARAM_WRITABLE);
		else if (type == G_TYPE_OBJECT || g_type_is_a (type, G_TYPE_OBJECT))
			pspec = g_param_spec_object ("dummy", "dummy", "dummy",
						     type, G_PARAM_READABLE|G_PARAM_WRITABLE);
		else if (G_TYPE_IS_ENUM (type))
		{
			GEnumClass *eclass = g_type_class_ref (type);
			pspec = g_param_spec_enum ("dummy", "dummy", "dummy",
						   type, eclass->minimum, G_PARAM_READABLE|G_PARAM_WRITABLE);
			g_type_class_unref (eclass);
		}
		else if (G_TYPE_IS_FLAGS (type))
			pspec = g_param_spec_flags ("dummy", "dummy", "dummy",
						    type, 0, G_PARAM_READABLE|G_PARAM_WRITABLE);

		if (pspec)
		{
			if ((property_class = 
			     glade_property_class_new_from_spec_full (NULL, pspec, FALSE)) != NULL)
			{
				/* XXX If we ever free the hash table, property classes wont touch
				 * the allocated pspecs, so they would theoretically be leaked.
				 */
				g_hash_table_insert (generic_property_classes, 
						     g_memdup (&type, sizeof (GType)), property_class);
			}
			else
				g_warning ("Unable to create property class for type %s", g_type_name (type));
		}
		else
			g_warning ("No generic conversion support for type %s", g_type_name (type));
	}
	return property_class;
}

/**
 * glade_utils_value_from_string:
 * @type: a #GType to convert with
 * @string: the string to convert
 * @project: the #GladeProject to look for formats of object names when needed
 * @widget: if the value is a gobject, this #GladeWidget will be used to look
 *          for an object in the same widget tree.
 *
 * Allocates and sets a #GValue of type @type
 * set to @string (using glade conversion routines) 
 *
 * Returns: A newly allocated and set #GValue
 */
GValue *
glade_utils_value_from_string (GType               type,
			       const gchar        *string,
			       GladeProject       *project,
			       GladeWidget        *widget)
{
	GladePropertyClass *pclass;

	g_return_val_if_fail (type != G_TYPE_INVALID, NULL);
	g_return_val_if_fail (string != NULL, NULL);

	if ((pclass = pclass_from_gtype (type)) != NULL)
		return glade_property_class_make_gvalue_from_string (pclass, string, project, widget);

	return NULL;
}


/**
 * glade_utils_string_from_value:
 * @value: a #GValue to convert
 * @fmt: the #GladeProjectFormat to honor
 *
 * Serializes #GValue into a string 
 * (using glade conversion routines) 
 *
 * Returns: A newly allocated string
 */
gchar *
glade_utils_string_from_value (const GValue       *value,
			       GladeProjectFormat  fmt)
{
	GladePropertyClass *pclass;

	g_return_val_if_fail (value != NULL, NULL);

	if ((pclass = pclass_from_gtype (G_VALUE_TYPE (value))) != NULL)
		return glade_property_class_make_string_from_gvalue (pclass, value, fmt);

	return NULL;
}


/**
 * glade_utils_liststore_from_enum_type:
 * @enum_type: A #GType
 * @include_empty: wheather to prepend an "Unset" slot
 *
 * Creates a liststore suitable for comboboxes and such to 
 * chose from a variety of types.
 *
 * Returns: A new #GtkListStore
 */
GtkListStore *
glade_utils_liststore_from_enum_type (GType    enum_type,
				      gboolean include_empty)
{
	GtkListStore        *store;
	GtkTreeIter          iter;
	GEnumClass          *eclass;
	guint                i;

	eclass = g_type_class_ref (enum_type);

	store = gtk_list_store_new (1, G_TYPE_STRING);

	if (include_empty)
	{
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 
				    0, _("None"),
				    -1);
	}
	
	for (i = 0; i < eclass->n_values; i++)
	{
		const gchar *displayable = glade_get_displayable_value (enum_type, eclass->values[i].value_nick);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 
				    0, displayable ? displayable : eclass->values[i].value_nick,
				    -1);
	}

	g_type_class_unref (eclass);

	return store;
}



/**
 * glade_utils_hijack_key_press:
 * @win: a #GtkWindow
 * event: the GdkEventKey 
 * user_data: unused
 *
 * This function is meant to be attached to key-press-event of a toplevel,
 * it simply allows the window contents to treat key events /before/ 
 * accelerator keys come into play (this way widgets dont get deleted
 * when cutting text in an entry etc.).
 * Creates a liststore suitable for comboboxes and such to 
 * chose from a variety of types.
 *
 * Returns: whether the event was handled
 */
gint
glade_utils_hijack_key_press (GtkWindow          *win, 
			      GdkEventKey        *event, 
			      gpointer            user_data)
{
	GtkWidget *focus_widget;

	focus_widget = gtk_window_get_focus (win);
	if (focus_widget &&
	    (event->keyval == GDK_Delete || /* Filter Delete from accelerator keys */
	     ((event->state & GDK_CONTROL_MASK) && /* CNTL keys... */
	      ((event->keyval == GDK_c || event->keyval == GDK_C) || /* CNTL-C (copy)  */
	       (event->keyval == GDK_x || event->keyval == GDK_X) || /* CNTL-X (cut)   */
	       (event->keyval == GDK_v || event->keyval == GDK_V) || /* CNTL-V (paste) */
	       (event->keyval == GDK_n || event->keyval == GDK_N))))) /* CNTL-N (new project) */
	{
		return gtk_widget_event (focus_widget,
					 (GdkEvent *)event);
	}
	return FALSE;
}



void
glade_utils_cairo_draw_line (cairo_t  *cr,
			     GdkColor *color,
			     gint      x1,
			     gint      y1,
			     gint      x2,
			     gint      y2)
{
  cairo_save (cr);

  gdk_cairo_set_source_color (cr, color);
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);

  cairo_move_to (cr, x1 + 0.5, y1 + 0.5);
  cairo_line_to (cr, x2 + 0.5, y2 + 0.5);
  cairo_stroke (cr);

  cairo_restore (cr);
}


void
glade_utils_cairo_draw_rectangle (cairo_t *cr,
				  GdkColor *color,
				  gboolean filled,
				  gint x,
				  gint y,
				  gint width,
				  gint height)
{
  gdk_cairo_set_source_color (cr, color);

  if (filled)
    {
      cairo_rectangle (cr, x, y, width, height);
      cairo_fill (cr);
    }
  else
    {
      cairo_rectangle (cr, x + 0.5, y + 0.5, width, height);
      cairo_stroke (cr);
    }
}


/* copied from gedit */
gchar *
glade_utils_replace_home_dir_with_tilde (const gchar *path)
{
#ifdef G_OS_UNIX
	gchar *tmp;
	gchar *home;

	g_return_val_if_fail (path != NULL, NULL);

	/* Note that g_get_home_dir returns a const string */
	tmp = (gchar *) g_get_home_dir ();

	if (tmp == NULL)
		return g_strdup (path);

	home = g_filename_to_utf8 (tmp, -1, NULL, NULL, NULL);
	if (home == NULL)
		return g_strdup (path);

	if (strcmp (path, home) == 0)
	{
		g_free (home);
		
		return g_strdup ("~");
	}

	tmp = home;
	home = g_strdup_printf ("%s/", tmp);
	g_free (tmp);

	if (g_str_has_prefix (path, home))
	{
		gchar *res;

		res = g_strdup_printf ("~/%s", path + strlen (home));

		g_free (home);
		
		return res;		
	}

	g_free (home);

	return g_strdup (path);
#else
	return g_strdup (path);
#endif
}
