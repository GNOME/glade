/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001-2005 Imendio AB
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

#include <devhelp/dh-book-tree.h>
#include <devhelp/dh-html.h>
#include <devhelp/dh-preferences.h>
#include <devhelp/dh-search.h>
#include <devhelp/dh-base.h>
#include "glade-devhelp.h"
#include "glade.h"

struct _GladeDhWidgetPriv {
	DhBase         *base;

        GtkWidget      *control_notebook; /* The notebook */

	/* Notebook first page has the html */
	GtkWidget      *html_notebook;    /* Html previous/next pages */

	/* Notebook second page has the book tree and search window */
        GtkWidget      *book_tree;
	GtkWidget      *search;

	/* Notebook buttons in hbuttonbox */
	GtkWidget      *html_button;
	GtkWidget      *search_button;

	/* Navigator buttons */
	GtkWidget      *forward;
	GtkWidget      *back;

	/* Separator (for alignment purposes) */
	GtkWidget      *separator;
};

static void       widget_class_init               (GladeDhWidgetClass   *klass);
static void       widget_init                     (GladeDhWidget        *widget);
static void       widget_finalize                 (GObject              *object);
static void       widget_populate                 (GladeDhWidget        *widget);
static void       widget_tree_link_selected_cb    (GObject              *ignored,
						   DhLink               *link,
						   GladeDhWidget        *widget);
static void       widget_search_link_selected_cb  (GObject              *ignored,
						   DhLink               *link,
						   GladeDhWidget        *widget);
static void       widget_check_history            (GladeDhWidget        *widget,
						   DhHtml               *html);
static void       widget_html_location_changed_cb (DhHtml               *html,
						   const gchar          *location,
						   GladeDhWidget        *widget);
static gboolean   widget_html_open_uri_cb         (DhHtml               *html,
						   const gchar          *uri,
						   GladeDhWidget        *widget);
static void       widget_html_open_new_tab_cb     (DhHtml               *html,
						   const gchar          *location,
						   GladeDhWidget        *widget);
static void       widget_open_new_tab             (GladeDhWidget        *widget,
						   const gchar          *location);
static DhHtml *   widget_get_active_html          (GladeDhWidget        *widget);
static void       widget_go_forward               (GtkButton            *button,
						   GladeDhWidget        *widget);
static void       widget_go_back                  (GtkButton            *button,
						   GladeDhWidget        *widget);
static void       widget_document_page            (GtkButton            *button,
						   GladeDhWidget        *widget);
static void       widget_search_page              (GtkButton            *button,
						   GladeDhWidget        *widget);

static GtkVBoxClass *parent_class = NULL;


GType
glade_dh_widget_get_type (void)
{
        static GType type = 0;

        if (!type) {
                static const GTypeInfo info = {
			sizeof (GladeDhWidgetClass),
			NULL,
			NULL,
			(GClassInitFunc) widget_class_init,
			NULL,
			NULL,
			sizeof (GladeDhWidget),
			0,
			(GInstanceInitFunc) widget_init
		};

                type = g_type_register_static (GTK_TYPE_VBOX,
					       "GladeDhWidget",
					       &info, 0);
        }

        return type;
}

static void
widget_class_init (GladeDhWidgetClass *klass)
{
        GObjectClass *object_class;

        parent_class = g_type_class_peek_parent (klass);

        object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = widget_finalize;
}

static void
widget_init (GladeDhWidget *widget)
{
	GladeDhWidgetPriv  *priv;

	priv = g_new0 (GladeDhWidgetPriv, 1);
        widget->priv = priv;
}

static void
widget_finalize (GObject *object)
{
	GladeDhWidget *widget = GLADE_DHWIDGET (object);
	DhBase *base = widget->priv->base;

	G_OBJECT_CLASS (parent_class)->finalize (object);

	g_object_unref (base);
}

/* The ugliest hack. When switching tabs, the selection and cursor is changed
 * for the tree view so the html content is changed. Block the signal during
 * switch.
 */
static void
widget_control_switch_page_cb (GtkWidget       *notebook,
			       GtkNotebookPage *page,
			       guint            page_num,
			       GladeDhWidget        *widget)
{
	GladeDhWidgetPriv *priv;

	priv = widget->priv;

	g_signal_handlers_block_by_func (priv->book_tree,
					 widget_tree_link_selected_cb, widget);
}

static void
widget_control_after_switch_page_cb (GtkWidget       *notebook,
				     GtkNotebookPage *page,
				     guint            page_num,
				     GladeDhWidget        *widget)
{
	GladeDhWidgetPriv *priv;

	priv = widget->priv;

	g_signal_handlers_unblock_by_func (priv->book_tree,
					   widget_tree_link_selected_cb, widget);
}

static void
widget_html_switch_page_cb (GtkNotebook     *notebook,
			    GtkNotebookPage *page,
			    guint            new_page_num,
			    GladeDhWidget   *widget)
{
	GtkWidget         *new_page;

	new_page = gtk_notebook_get_nth_page (notebook, new_page_num);
	if (!new_page) 
	{
		widget_check_history (widget, NULL);
	}

}

typedef enum {
	NAV_FORWARD,
	NAV_BACK,
	NAV_SEARCH,
	NAV_DOCUMENT
} NavButtonType;

static GtkWidget *
widget_create_nav_button (NavButtonType type)
{
	static GSList *group = NULL;
	GtkWidget     *image, *button, *align, *hbox;
	gchar         *path;
	
	if (type == NAV_DOCUMENT ||
	    type == NAV_SEARCH)
	{ 
		button = gtk_radio_button_new (group);
		group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
	}
	else
	{
		button = gtk_button_new ();

	}

	hbox   = gtk_hbox_new (FALSE, 0);

	if (type == NAV_DOCUMENT) 
	{
		path = g_build_filename (glade_pixmaps_dir, "devhelp.png", NULL);
		image  = gtk_image_new_from_file (path);
	}
	else
		image  = gtk_image_new_from_stock 
			(type == NAV_FORWARD ? GTK_STOCK_GO_FORWARD : 
			 type == NAV_BACK ? GTK_STOCK_GO_BACK : GTK_STOCK_FIND, 
			 GTK_ICON_SIZE_SMALL_TOOLBAR);
	align  = gtk_alignment_new (0.5, 0.5, 0, 0);

	gtk_box_pack_start (GTK_BOX (hbox), image, TRUE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (align), hbox);

	gtk_container_set_border_width (GTK_CONTAINER (button), 
					GLADE_GENERIC_BORDER_WIDTH);

	gtk_widget_show_all (align);

	if (type == NAV_FORWARD || type == NAV_BACK)
		gtk_widget_set_sensitive (button, FALSE);

	gtk_container_add (GTK_CONTAINER (button), align);

	return button;
}

static void
widget_populate (GladeDhWidget *widget)
{
        GladeDhWidgetPriv *priv;
	GtkWidget    *book_tree_sw, *book_and_search;
	GNode        *contents_tree;
	GList        *keywords;
	GtkWidget    *hbox, *align, *w;

        priv = widget->priv;

	/* Master notebook
	 */
	priv->control_notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->control_notebook), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (priv->control_notebook), 
					GLADE_GENERIC_BORDER_WIDTH);

	/* HTML tabs notebook. */
 	priv->html_notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->html_notebook), FALSE);

 	g_signal_connect (priv->html_notebook, "switch-page",
			  G_CALLBACK (widget_html_switch_page_cb),
			  widget);

	gtk_notebook_append_page (GTK_NOTEBOOK (priv->control_notebook),
				  priv->html_notebook, NULL);

	
	/* Book and search page */
	book_and_search = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (book_and_search), 
					GLADE_GENERIC_BORDER_WIDTH);

	book_tree_sw = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (book_tree_sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (book_tree_sw),
					     GTK_SHADOW_IN);

	contents_tree = dh_base_get_book_tree (priv->base);
	keywords = dh_base_get_keywords (priv->base);

	priv->book_tree = dh_book_tree_new (contents_tree);

	gtk_container_add (GTK_CONTAINER (book_tree_sw),
			   priv->book_tree);

	g_signal_connect (priv->book_tree, "link-selected",
			  G_CALLBACK (widget_tree_link_selected_cb),
			  widget);

	gtk_box_pack_end (GTK_BOX (book_and_search), book_tree_sw,
			  TRUE, TRUE, 0);


	priv->search = dh_search_new (keywords);
	g_signal_connect (priv->search, "link-selected",
			  G_CALLBACK (widget_search_link_selected_cb),
			  widget);

	gtk_box_pack_start (GTK_BOX (book_and_search), priv->search,
			    FALSE, FALSE, 0);

	gtk_notebook_append_page (GTK_NOTEBOOK (priv->control_notebook),
				  book_and_search, NULL);

	/* Connect after we're all built
	 */
	g_signal_connect (priv->control_notebook, "switch-page",
			  G_CALLBACK (widget_control_switch_page_cb),
			  widget);

	g_signal_connect_after (priv->control_notebook,	"switch-page",
				G_CALLBACK (widget_control_after_switch_page_cb),
				widget);


	/* Build navigator buttons
	 */
	hbox = gtk_hbox_new (FALSE, 0);
	align = gtk_alignment_new  (0, 0.5, 0, 0);
	gtk_container_add (GTK_CONTAINER (align), hbox);

	priv->forward = widget_create_nav_button (NAV_FORWARD);
	g_signal_connect (G_OBJECT (priv->forward), "clicked",
			  G_CALLBACK (widget_go_forward), widget);
			  
	priv->back = widget_create_nav_button (NAV_BACK);
	g_signal_connect (G_OBJECT (priv->back), "clicked",
			  G_CALLBACK (widget_go_back), widget);

	priv->html_button = widget_create_nav_button (NAV_DOCUMENT);	
	g_signal_connect (G_OBJECT (priv->html_button), "clicked",
			  G_CALLBACK (widget_document_page), widget);

	priv->search_button = widget_create_nav_button (NAV_SEARCH);
	g_signal_connect (G_OBJECT (priv->search_button), "clicked",
			  G_CALLBACK (widget_search_page), widget);


	/* Put it in a container for border width */
	w = gtk_vseparator_new ();
	priv->separator = gtk_alignment_new  (0.5, 0.5, 1.0, 1.0);
	gtk_container_add (GTK_CONTAINER (priv->separator), w);
	gtk_container_set_border_width (GTK_CONTAINER (priv->separator), 
					GLADE_GENERIC_BORDER_WIDTH);

	gtk_box_pack_start (GTK_BOX (hbox), priv->back, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), priv->forward, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), priv->separator, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), priv->html_button, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), priv->search_button, FALSE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (widget), align, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (widget), priv->control_notebook, TRUE, TRUE, 0);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->html_button), TRUE);

	gtk_widget_show_all (GTK_WIDGET (widget));
}

static void
widget_go_back (GtkButton     *button,
		GladeDhWidget *widget)
{
	GladeDhWidgetPriv *priv;
	DhHtml            *html;
	GtkWidget         *frame;

	priv = widget->priv;

	frame = gtk_notebook_get_nth_page (
		GTK_NOTEBOOK (priv->html_notebook),
		gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->html_notebook)));
	html = g_object_get_data (G_OBJECT (frame), "html");

	dh_html_go_back (html);
}

static void
widget_go_forward (GtkButton     *button,
		   GladeDhWidget *widget)
{
	GladeDhWidgetPriv *priv;
	DhHtml            *html;
	GtkWidget         *frame;

	priv = widget->priv;

	frame = gtk_notebook_get_nth_page (GTK_NOTEBOOK (priv->html_notebook),
					   gtk_notebook_get_current_page
					   (GTK_NOTEBOOK (priv->html_notebook)));
	html = g_object_get_data (G_OBJECT (frame), "html");

	dh_html_go_forward (html);
}

static void
widget_search_page (GtkButton     *button,
		    GladeDhWidget *widget)
{
	gtk_notebook_set_current_page
		(GTK_NOTEBOOK (widget->priv->control_notebook), 1);
}

static void
widget_document_page (GtkButton     *button,
		      GladeDhWidget *widget)
{
	gtk_notebook_set_current_page
		(GTK_NOTEBOOK (widget->priv->control_notebook), 0);
}


static void
widget_tree_link_selected_cb (GObject       *ignored,
			      DhLink        *link,
			      GladeDhWidget *widget)
{
	GladeDhWidgetPriv *priv;
	DhHtml       *html;

	priv = widget->priv;

	html = widget_get_active_html (widget);

	/* Block so we don't try to sync the tree when we have already clicked
	 * in it.
	 */
	g_signal_handlers_block_by_func (html,
					 widget_html_open_uri_cb,
					 widget);

	dh_html_open_uri (html, link->uri);

	g_signal_handlers_unblock_by_func (html,
					   widget_html_open_uri_cb,
					   widget);

	widget_check_history (widget, html);
}

static void
widget_search_link_selected_cb (GObject  *ignored,
				DhLink   *link,
				GladeDhWidget *widget)
{
	GladeDhWidgetPriv *priv;
	DhHtml       *html;

	priv = widget->priv;

	html = widget_get_active_html (widget);

	dh_html_open_uri (html, link->uri);

	widget_check_history (widget, html);
}

static void
widget_check_history (GladeDhWidget *widget, DhHtml *html)
{
	GladeDhWidgetPriv *priv;

	priv = widget->priv;

	gtk_widget_set_sensitive (priv->forward,
				  html ? dh_html_can_go_forward (html) : FALSE);
	gtk_widget_set_sensitive (priv->back,
				  html ? dh_html_can_go_back (html) : FALSE);
}

static void
widget_html_location_changed_cb (DhHtml      *html,
				 const gchar *location,
				 GladeDhWidget    *widget)
{
	GladeDhWidgetPriv *priv;

	priv = widget->priv;

	if (html == widget_get_active_html (widget)) {
		widget_check_history (widget, html);
	}
}

static gboolean
widget_html_open_uri_cb (DhHtml      *html,
			 const gchar *uri,
			 GladeDhWidget    *widget)
{
	GladeDhWidgetPriv *priv;

	priv = widget->priv;

	if (html == widget_get_active_html (widget)) {
		dh_book_tree_select_uri (DH_BOOK_TREE (priv->book_tree), uri);
	}

	return FALSE;
}

static void
widget_html_open_new_tab_cb (DhHtml      *html,
			     const gchar *location,
			     GladeDhWidget    *widget)
{
	widget_open_new_tab (widget, location);
}

static void
widget_open_new_tab (GladeDhWidget  *widget,
		     const gchar    *location)
{
	GladeDhWidgetPriv *priv;
	DhHtml       *html;
	GtkWidget    *frame;
	GtkWidget    *view;
	gint          num;

	priv = widget->priv;

	html = dh_html_new ();

	view = dh_html_get_widget (html);
	gtk_widget_show (view);

	frame = gtk_frame_new (NULL);
	gtk_widget_show (frame);

	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 2);
	gtk_container_add (GTK_CONTAINER (frame), view);

	g_object_set_data (G_OBJECT (frame), "html", html);

	g_signal_connect (html, "open-uri",
			  G_CALLBACK (widget_html_open_uri_cb),
			  widget);
	g_signal_connect (html, "location-changed",
			  G_CALLBACK (widget_html_location_changed_cb),
			  widget);
	g_signal_connect (html, "open-new-tab",
			  G_CALLBACK (widget_html_open_new_tab_cb),
			  widget);

	num = gtk_notebook_append_page (GTK_NOTEBOOK (priv->html_notebook),
					frame, NULL);

	/* Hack to get GtkMozEmbed to work properly. */
	gtk_widget_realize (view);

	if (location) {
		dh_html_open_uri (html, location);
	} else {
		dh_html_clear (html);
	}

	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->html_notebook), num);

}

static DhHtml *
widget_get_active_html (GladeDhWidget *widget)
{
	GladeDhWidgetPriv *priv;
	gint          page_num;
	GtkWidget    *page;

	priv = widget->priv;

	page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->html_notebook));
	if (page_num == -1) {
		return NULL;
	}

	page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (priv->html_notebook), page_num);

	return g_object_get_data (G_OBJECT (page), "html");
}


GtkWidget *
glade_dh_widget_new (void)
{
        GladeDhWidget     *widget;
        GladeDhWidgetPriv *priv;

        widget     = g_object_new (GLADE_TYPE_DHWIDGET, NULL);
        priv       = widget->priv;
	priv->base = dh_base_new ();

	widget_populate (widget);

	return GTK_WIDGET (widget);
}

void
glade_dh_widget_search (GladeDhWidget *widget, const gchar *str)
{
	static gboolean open = FALSE;
	GladeDhWidgetPriv *priv;

	g_return_if_fail (GLADE_IS_DHWIDGET (widget));

	/* We cant use a mozembed window untill
	 * its parented into a real hierarchy (with a toplevel)
	 */
	g_return_if_fail (gtk_widget_get_toplevel (GTK_WIDGET (widget)));

	priv = widget->priv;

	if (!open) 
	{
		widget_open_new_tab (widget, NULL);
		open = TRUE;
	}

	dh_search_set_search_string (DH_SEARCH (priv->search), str);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->control_notebook), 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->html_button), TRUE);
}

GList *
glade_dh_get_hbuttons (GladeDhWidget *widget)
{
	GList             *hbuttons = NULL;
	GladeDhWidgetPriv *priv;

	g_return_val_if_fail (GLADE_IS_DHWIDGET (widget), NULL);

	priv = widget->priv;

	hbuttons = g_list_prepend (hbuttons, priv->html_button);
	hbuttons = g_list_prepend (hbuttons, priv->search_button);
	hbuttons = g_list_prepend (hbuttons, priv->separator);
	hbuttons = g_list_prepend (hbuttons, priv->forward);
	hbuttons = g_list_prepend (hbuttons, priv->back);

	return hbuttons;
}


#ifdef SCAFFOLD

gint main (gint argc, gchar *argv[])
{
	GtkWidget     *window;
	GtkWidget     *widget;

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	widget = glade_dh_widget_new ();

	gtk_container_add (GTK_CONTAINER (window), widget);

	gtk_widget_show_all (window);

	gtk_window_present (GTK_WINDOW (window));

	glade_dh_widget_search (GLADE_DHWIDGET (widget), "book:gladeui");

	gtk_main ();
	
	return 0;
}


#endif
