/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Juan Pablo Ugarte.
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
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#include "glade.h"
#include "glade-editor-property.h"
#include <libbonoboui.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>

#include "glade-gtk.h"
#define GLADEGNOME_API GLADEGTK_API

/* This function does absolutely nothing
 * (and is for use in overriding post_create functions).
 */
void GLADEGNOME_API
empty (GObject *container, GladeCreateReason reason)
{
}

/* Catalog init function */
void GLADEGNOME_API
glade_gnomeui_init ()
{
	gchar *argv[2] = {"glade-3", NULL};
	GtkStockItem items [] = {
		{ GNOME_STOCK_TIMER,              "Gnome Timer",         0, },
		{ GNOME_STOCK_TIMER_STOP,         "Gnome Timer stop",    0, },
		{ GNOME_STOCK_TRASH,              "Gnome Trash",         0, },
		{ GNOME_STOCK_TRASH_FULL,         "Gnome Trash Full",    0, },
		{ GNOME_STOCK_SCORES,             "Gnome Scores",        0, },
		{ GNOME_STOCK_ABOUT,              "Gnome About",         0, },
		{ GNOME_STOCK_BLANK,              "Gnome Blank",         0, },
		{ GNOME_STOCK_VOLUME,             "Gnome Volume",        0, },
		{ GNOME_STOCK_MIDI,               "Gnome Midi",          0, },
		{ GNOME_STOCK_MIC,                "Gnome Mic",           0, },
		{ GNOME_STOCK_LINE_IN,            "Gnome Line In",       0, },
		{ GNOME_STOCK_MAIL,               "Gnome Mail",          0, },
		{ GNOME_STOCK_MAIL_RCV,           "Gnome Mail Recive",   0, },
		{ GNOME_STOCK_MAIL_SND,           "Gnome Mail Send",     0, },
		{ GNOME_STOCK_MAIL_RPL,           "Gnome Mail Reply",    0, },
		{ GNOME_STOCK_MAIL_FWD,           "Gnome Mail Foward",   0, },
		{ GNOME_STOCK_MAIL_NEW,           "Gnome Mail New",      0, },
		{ GNOME_STOCK_ATTACH,             "Gnome Attach",        0, },
		{ GNOME_STOCK_BOOK_RED,           "Gnome Book Red",      0, },
		{ GNOME_STOCK_BOOK_GREEN,         "Gnome Book Green",    0, },
		{ GNOME_STOCK_BOOK_BLUE,          "Gnome Book Blue",     0, },
		{ GNOME_STOCK_BOOK_YELLOW,        "Gnome Book Yellow",   0, },
		{ GNOME_STOCK_BOOK_OPEN,          "Gnome Book Open",     0, },
		{ GNOME_STOCK_MULTIPLE_FILE,      "Gnome Multiple File", 0, },
		{ GNOME_STOCK_NOT,                "Gnome Not",           0, },
		{ GNOME_STOCK_TABLE_BORDERS,      "Gnome Table Borders", 0, },
		{ GNOME_STOCK_TABLE_FILL,         "Gnome Table Fill",    0, },
		{ GNOME_STOCK_TEXT_INDENT,        "Gnome Indent",        0, },
		{ GNOME_STOCK_TEXT_UNINDENT,      "Gnome Unindent",      0, },
		{ GNOME_STOCK_TEXT_BULLETED_LIST, "Gnome Bulleted List", 0, },
		{ GNOME_STOCK_TEXT_NUMBERED_LIST, "Gnome Numbered List", 0, }
	};

	gnome_program_init ("glade-3", "1.0",
			    LIBGNOMEUI_MODULE, 1, argv,
			    GNOME_PARAM_NONE);
	
	gtk_stock_add (items, sizeof (items) / sizeof (GtkStockItem));
	
	glade_standard_stock_append_prefix ("gnome-stock-");
}

/* GnomeApp */
void GLADEGNOME_API
glade_gnome_app_post_create (GObject *object, GladeCreateReason reason)
{
	static GladeWidgetClass *menubar_class = NULL, *dock_item_class;
	GladeWidget *gapp, *gdock, *gdock_item, *gmenubar;
	GladeProject *project;
	GnomeApp *app;
	
	g_return_if_fail (GNOME_IS_APP (object));
	
	gtk_window_set_default_size (GTK_WINDOW (object), 440, 250);
	
	app = GNOME_APP (object);
	gapp = glade_widget_get_from_gobject (object);
	
	project = glade_widget_get_project (gapp);
	
	/* Add BonoboDock */
	gdock = glade_widget_class_create_internal
		(gapp, G_OBJECT (app->dock),
		 "dock",
		 glade_widget_get_name (gapp),
		 FALSE,
		 GLADE_CREATE_LOAD);
	
	if (reason != GLADE_CREATE_USER) return;
	
	/* Add MenuBar */
	if (menubar_class == NULL)
	{
		dock_item_class = glade_widget_class_get_by_type (BONOBO_TYPE_DOCK_ITEM);
		menubar_class = glade_widget_class_get_by_type (GTK_TYPE_MENU_BAR);
	}

	/* DockItem */
	gdock_item = glade_widget_class_create_widget (dock_item_class, FALSE,
						       "parent", gdock, 
						       "project", project, NULL);
	glade_widget_add_child (gdock, gdock_item, FALSE);
	glade_widget_pack_property_set (gdock_item, "behavior", 
					BONOBO_DOCK_ITEM_BEH_EXCLUSIVE |
					BONOBO_DOCK_ITEM_BEH_NEVER_VERTICAL |
					BONOBO_DOCK_ITEM_BEH_LOCKED);
	/* MenuBar */
	gmenubar = glade_widget_class_create_widget (menubar_class, FALSE,
						     "parent", gdock_item, 
						     "project", project, NULL);

	glade_widget_add_child (gdock_item, gmenubar, FALSE);
	
	/* Add Client Area placeholder */
	bonobo_dock_set_client_area (BONOBO_DOCK (app->dock),
				     glade_placeholder_new());
	
	/* Add StatusBar */
	glade_widget_property_set (gapp, "has-statusbar", TRUE);
}

void GLADEGNOME_API
glade_gnome_app_get_internal_child (GObject *object, 
				    const gchar *name,
				    GObject **child)
{
	GnomeApp *app;
	GladeWidget *gapp;
	
	g_return_if_fail (GNOME_IS_APP (object));
	
	app = GNOME_APP (object);
	gapp = glade_widget_get_from_gobject (object);
	
	if (strcmp ("dock", name) == 0)
		*child = G_OBJECT (app->dock);
	else if (strcmp ("appbar", name) == 0)
	{
		*child = G_OBJECT (app->statusbar);
		if (*child == NULL)
		{
			glade_widget_property_set (gapp, "has-statusbar", TRUE);
			*child = G_OBJECT (app->statusbar);
		}
	}
	else
		*child = NULL;
}

GList * GLADEGNOME_API
glade_gnome_app_get_children (GObject *object)
{
	GList *list = NULL;
	GnomeApp *app = GNOME_APP (object);

	if (app->dock) list = g_list_append (list, G_OBJECT (app->dock));
	if (app->statusbar) list = g_list_append (list, G_OBJECT (app->statusbar));
	if (app->contents) list = g_list_append (list, G_OBJECT (app->contents));
		
	return list;
}

void GLADEGNOME_API
glade_gnome_app_set_child_property (GObject *container,
				    GObject *child,
				    const gchar *property_name,
				    GValue *value)
{
	GnomeApp *app;
	
	g_return_if_fail (GNOME_IS_APP (container));
	g_return_if_fail (GTK_IS_WIDGET (child));
	
	app = GNOME_APP (container);

	gtk_container_child_set_property (GTK_CONTAINER (app->vbox), 
					  (GNOME_IS_APPBAR (child)) 
						? gtk_widget_get_parent (GTK_WIDGET (child))
						: GTK_WIDGET (child),
					  property_name,
					  value);
}

void GLADEGNOME_API
glade_gnome_app_get_child_property (GObject *container,
				    GObject *child,
				    const gchar *property_name,
				    GValue *value)
{
	GnomeApp *app;
	
	g_return_if_fail (GNOME_IS_APP (container));
	g_return_if_fail (GTK_IS_WIDGET (child));
	
	app = GNOME_APP (container);

	gtk_container_child_get_property (GTK_CONTAINER (app->vbox), 
					  (GNOME_IS_APPBAR (child)) 
						? gtk_widget_get_parent (GTK_WIDGET (child))
						: GTK_WIDGET (child),
					  property_name,
					  value);
}

void GLADEGNOME_API
glade_gnome_app_set_has_statusbar (GObject *object, GValue *value)
{
	GnomeApp *app;
	GladeWidget *gapp, *gbar;
	
	g_return_if_fail (GNOME_IS_APP (object));
	
	app = GNOME_APP (object);
	gapp = glade_widget_get_from_gobject (object);
	
	if (g_value_get_boolean (value))
	{
		if (app->statusbar == NULL)
		{
			GtkWidget *bar = gnome_appbar_new (TRUE, TRUE,
						GNOME_PREFERENCES_NEVER);

			gnome_app_set_statusbar (app, bar);
			
			gbar = glade_widget_class_create_internal
				(gapp, G_OBJECT (bar), "appbar",
				 glade_widget_get_name (gapp),
				 FALSE, GLADE_CREATE_USER);

			glade_widget_set_parent (gbar, gapp);
			glade_widget_pack_property_set (gbar, "expand", FALSE);
		}
	}
	else 
	{
		if (app->statusbar)
		{
			glade_project_remove_object (glade_widget_get_project (gapp),
						     G_OBJECT (app->statusbar));
			gtk_container_remove (GTK_CONTAINER (app->vbox),
					      gtk_widget_get_parent (app->statusbar));
			app->statusbar = NULL;
		}
	}
}

/* GnomeAppBar */
GType GLADEGNOME_API
gnome_app_bar_get_type ()
{
	return gnome_appbar_get_type ();
}

void GLADEGNOME_API
glade_gnome_app_bar_post_create (GObject *object, GladeCreateReason reason)
{
	g_return_if_fail (GNOME_IS_APPBAR (object));
	gnome_appbar_set_status (GNOME_APPBAR (object), _("Status Message."));
}

/* GnomeDruid */
static GladeWidget *
glade_gnome_druid_add_page (GladeWidget *gdruid, gboolean edge)
{
	GladeWidget *gpage;
	static GladeWidgetClass *dps_class = NULL, *dpe_class;
	GladeProject *project = glade_widget_get_project (gdruid);
	
	if (dps_class == NULL)
	{
		dps_class = glade_widget_class_get_by_type (GNOME_TYPE_DRUID_PAGE_STANDARD);
		dpe_class = glade_widget_class_get_by_type (GNOME_TYPE_DRUID_PAGE_EDGE);
	}
	
	gpage = glade_widget_class_create_widget (edge ? dpe_class : dps_class, FALSE,
						  "parent", gdruid, 
						  "project", project, NULL);
	
	glade_widget_add_child (gdruid, gpage, FALSE);

	return gpage;
}

void GLADEGNOME_API
glade_gnome_druid_post_create (GObject *object, GladeCreateReason reason)
{
	GladeWidget *gdruid, *gpage;

	g_return_if_fail (GNOME_IS_DRUID (object));
	
	if (reason != GLADE_CREATE_USER) return;

	gdruid = glade_widget_get_from_gobject (object);
	
	/* Add Start Page */
	gpage = glade_gnome_druid_add_page (gdruid, TRUE);
	glade_widget_property_set (gpage, "position", GNOME_EDGE_START);
	
	/* Add Standard Pages */
	glade_gnome_druid_add_page (gdruid, FALSE);
	
	/* Add Finish Page */
	gpage = glade_gnome_druid_add_page (gdruid, TRUE);
	glade_widget_property_set (gpage, "position", GNOME_EDGE_FINISH);
}

static gboolean
glade_gnome_druid_page_cb (GnomeDruidPage *druidpage,
			   GtkWidget *widget,
			   gpointer user_data)
{
	GnomeDruid *druid = GNOME_DRUID (widget);
	GList *children, *l;
	gboolean next = TRUE, back = TRUE;

	children = l = gtk_container_get_children (GTK_CONTAINER (druid));
	while (l)
	{
		if (druidpage == l->data) break;
		l = l->next;
	}
	
	if (GPOINTER_TO_INT (user_data))
	{
		if (l->next)
		{
			gnome_druid_set_page (druid, (GnomeDruidPage *) l->next->data);
			next = l->next->next != NULL;
		}
	}
	else
	{
		if (l->prev)
		{
			gnome_druid_set_page (druid, (GnomeDruidPage *) l->prev->data);
			back = l->prev->prev != NULL;
		}
	}
	
	g_list_free (children);

	gnome_druid_set_buttons_sensitive (druid, back, next, TRUE, TRUE);
	
	return TRUE;
}

void GLADEGNOME_API
glade_gnome_druid_add_child (GObject *container, GObject *child)
{
	g_return_if_fail (GNOME_IS_DRUID (container));
	g_return_if_fail (GNOME_IS_DRUID_PAGE (child));

	g_signal_handlers_disconnect_matched (child, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					      glade_gnome_druid_page_cb, NULL);

	gnome_druid_append_page (GNOME_DRUID (container), GNOME_DRUID_PAGE (child));
	
	g_signal_connect (child, "next", G_CALLBACK (glade_gnome_druid_page_cb),
					GINT_TO_POINTER (TRUE));
	g_signal_connect (child, "back", G_CALLBACK (glade_gnome_druid_page_cb),
					GINT_TO_POINTER (FALSE));	
}

void GLADEGNOME_API
glade_gnome_druid_remove_child (GObject *container, GObject *child)
{
	g_return_if_fail (GNOME_IS_DRUID (container));
	g_return_if_fail (GNOME_IS_DRUID_PAGE (child));
	
	gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET (child));
}

static void
glade_gnome_druid_insert_page (GnomeDruid *druid, GnomeDruidPage *page, gint pos)
{
	GList *children, *l;
	GnomeDruidPage *back_page = NULL;
	gint i = 0;
	
	children = l = gtk_container_get_children (GTK_CONTAINER (druid));
	while (l)
	{
		i++;
		if (i >= pos)
		{
			back_page = (GnomeDruidPage *) l->data;
			break;
		}
		l = l->next;
	}

	gnome_druid_insert_page (druid, back_page, page);
	
	g_list_free (children);
}

static gint
glade_gnome_druid_get_page_position (GnomeDruid *druid, GnomeDruidPage *page)
{
	GList *children, *l;
	gint i = 0;
	
	children = l = gtk_container_get_children (GTK_CONTAINER (druid));
	while (l)
	{
		GnomeDruidPage *current = (GnomeDruidPage *) l->data;
		if (current == page) break;
		i++;
		l = l->next;
	}
	
	g_list_free (children);
	
	return i;
}

void GLADEGNOME_API
glade_gnome_druid_set_child_property (GObject *container,
				      GObject *child,
				      const gchar *property_name,
				      GValue *value)
{
	g_return_if_fail (GNOME_IS_DRUID (container));
	g_return_if_fail (GNOME_IS_DRUID_PAGE (child));
	
	if (strcmp (property_name, "position") == 0)
	{
		gint position = g_value_get_int (value);
		
		if (position < 0)
		{
			position = glade_gnome_druid_get_page_position
						(GNOME_DRUID (container),
						 GNOME_DRUID_PAGE (child));
			g_value_set_int (value, position);
		}
		
		g_object_ref (child);
		gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET (child));
		glade_gnome_druid_insert_page (GNOME_DRUID (container),
					       GNOME_DRUID_PAGE (child),
					       position);
		g_object_unref (child);
	}
	else
		/* Chain Up */
		gtk_container_child_set_property (GTK_CONTAINER (container), 
						  GTK_WIDGET (child),
						  property_name,
						  value);
}

void GLADEGNOME_API
glade_gnome_druid_get_child_property (GObject *container,
				      GObject *child,
				      const gchar *property_name,
				      GValue *value)
{
	g_return_if_fail (GNOME_IS_DRUID (container));
	g_return_if_fail (GNOME_IS_DRUID_PAGE (child));
	
	if (strcmp (property_name, "position") == 0)
		g_value_set_int (value, glade_gnome_druid_get_page_position (
							GNOME_DRUID (container),
							GNOME_DRUID_PAGE (child)));
	else
		/* Chain Up */
		gtk_container_child_get_property (GTK_CONTAINER (container), 
						  GTK_WIDGET (child),
						  property_name,
						  value);
}

/* GnomeDruidPageStandard */
void GLADEGNOME_API
glade_gnome_dps_post_create (GObject *object, GladeCreateReason reason)
{
	GladeWidget *gpage, *gvbox;
	GObject *vbox;
	
	g_return_if_fail (GNOME_IS_DRUID_PAGE_STANDARD (object));
	
	gpage = glade_widget_get_from_gobject (object);
	vbox = G_OBJECT (GNOME_DRUID_PAGE_STANDARD (object)->vbox);
	gvbox = glade_widget_class_create_internal (gpage, vbox, "vbox",
						    glade_widget_get_name (gpage),
						    FALSE, GLADE_CREATE_LOAD);
	
	if (reason == GLADE_CREATE_USER)
		glade_widget_property_set (gvbox, "size", 1);
}

void GLADEGNOME_API
glade_gnome_dps_get_internal_child (GObject *object, 
				    const gchar *name,
				    GObject **child)
{
	g_return_if_fail (GNOME_IS_DRUID_PAGE_STANDARD (object));
	
	if (strcmp (name, "vbox") == 0)
		*child = G_OBJECT (GNOME_DRUID_PAGE_STANDARD (object)->vbox);
	else
		*child = NULL;
}

GList * GLADEGNOME_API
glade_gnome_dps_get_children (GObject *object)
{
	GList *list = NULL;
	GnomeDruidPageStandard *page;
	
	g_return_val_if_fail (GNOME_IS_DRUID_PAGE_STANDARD (object), NULL);
	
	page = GNOME_DRUID_PAGE_STANDARD (object);

	if (page->vbox) list = g_list_append (list, G_OBJECT (page->vbox));
		
	return list;
}

static void
glade_gnome_dps_set_color_common (GObject *object,
				  const gchar *property_name,
				  GValue *value)
{
	GladeProperty *prop;
	const gchar *color_str;
	GValue *color;
	
	g_return_if_fail (GNOME_IS_DRUID_PAGE_STANDARD (object));
	
	if ((color_str = g_value_get_string (value)) == NULL) return;

	prop = glade_widget_get_property (glade_widget_get_from_gobject (object),
					  property_name);
	
	color = glade_property_class_make_gvalue_from_string (prop->class,
							      color_str, NULL);
	if (color) glade_property_set_value (prop, color);
}

void GLADEGNOME_API
glade_gnome_dps_set_background (GObject *object, GValue *value)
{
	glade_gnome_dps_set_color_common (object, "background-gdk", value);
}

void GLADEGNOME_API
glade_gnome_dps_set_contents_background (GObject *object, GValue *value)
{
	glade_gnome_dps_set_color_common (object, "contents-background-gdk", value);
}

void GLADEGNOME_API
glade_gnome_dps_set_logo_background (GObject *object, GValue *value)
{
	glade_gnome_dps_set_color_common (object, "logo-background-gdk", value);
}

void GLADEGNOME_API
glade_gnome_dps_set_title_foreground (GObject *object, GValue *value)
{
	glade_gnome_dps_set_color_common (object, "title-foreground-gdk", value);
}

/* GnomeDruidPageEdge */
static GType
glade_gnome_dpe_position_get_type (void)
{
	static GType etype = 0;
	if (etype == 0) {
		static const GEnumValue values[] = {
			{ GNOME_EDGE_START, "GNOME_EDGE_START", "Edge Start"},
			{ GNOME_EDGE_FINISH, "GNOME_EDGE_FINISH", "Edge Finish"},
			{ GNOME_EDGE_OTHER, "GNOME_EDGE_OTHER", "Edge Other"},
			{ 0, NULL, NULL }
		};
		etype = g_enum_register_static ("GladeGnomeDruidPagePosition", values);
	}
	return etype;
}

GParamSpec * GLADEGNOME_API
glade_gnome_dpe_position_spec (void)
{
	return g_param_spec_enum ("position", _("Position"), 
				  _("The position in the druid"),
				  glade_gnome_dpe_position_get_type (),
				  GNOME_EDGE_OTHER, G_PARAM_READWRITE);
}

void GLADEGNOME_API
glade_gnome_dpe_set_title (GObject *object, GValue *value)
{
	const gchar *title;
	
	g_return_if_fail (GNOME_IS_DRUID_PAGE_EDGE (object));
	
	if ((title = g_value_get_string (value)))
		gnome_druid_page_edge_set_title (GNOME_DRUID_PAGE_EDGE (object),
						 title);
}

void GLADEGNOME_API
glade_gnome_dpe_set_position (GObject *object, GValue *value)
{
}

void GLADEGNOME_API
glade_gnome_dpe_set_text (GObject *object, GValue *value)
{
	const gchar *text;
	
	g_return_if_fail (GNOME_IS_DRUID_PAGE_EDGE (object));
	
	if ((text = g_value_get_string (value)))
		gnome_druid_page_edge_set_text (GNOME_DRUID_PAGE_EDGE (object),
						text);
}

void GLADEGNOME_API
glade_gnome_dpe_set_title_foreground (GObject *object, GValue *value)
{
	GdkColor *color;

	g_return_if_fail (GNOME_IS_DRUID_PAGE_EDGE (object));

	if ((color = (GdkColor *) g_value_get_boxed (value)))
		gnome_druid_page_edge_set_title_color (GNOME_DRUID_PAGE_EDGE (object),
						       color);
}

void GLADEGNOME_API
glade_gnome_dpe_set_text_foreground (GObject *object, GValue *value)
{
	GdkColor *color;

	g_return_if_fail (GNOME_IS_DRUID_PAGE_EDGE (object));

	if ((color = (GdkColor *) g_value_get_boxed (value)))
		gnome_druid_page_edge_set_text_color (GNOME_DRUID_PAGE_EDGE (object),
						      color);
}

void GLADEGNOME_API
glade_gnome_dpe_set_background (GObject *object, GValue *value)
{
	GdkColor *color;

	g_return_if_fail (GNOME_IS_DRUID_PAGE_EDGE (object));

	if ((color = (GdkColor *) g_value_get_boxed (value)))
		gnome_druid_page_edge_set_bg_color (GNOME_DRUID_PAGE_EDGE (object),
						    color);
}

void GLADEGNOME_API
glade_gnome_dpe_set_contents_background (GObject *object, GValue *value)
{
	GdkColor *color;

	g_return_if_fail (GNOME_IS_DRUID_PAGE_EDGE (object));

	if ((color = (GdkColor *) g_value_get_boxed (value)))
		gnome_druid_page_edge_set_textbox_color (GNOME_DRUID_PAGE_EDGE (object),
							 color);
}

void GLADEGNOME_API
glade_gnome_dpe_set_logo_background (GObject *object, GValue *value)
{
	GdkColor *color;

	g_return_if_fail (GNOME_IS_DRUID_PAGE_EDGE (object));

	if ((color = (GdkColor *) g_value_get_boxed (value)))
		gnome_druid_page_edge_set_logo_bg_color (GNOME_DRUID_PAGE_EDGE (object),
							 color);
}

void GLADEGNOME_API
glade_gnome_dpe_set_logo (GObject *object, GValue *value)
{
	g_return_if_fail (GNOME_IS_DRUID_PAGE_EDGE (object));
	
	gnome_druid_page_edge_set_logo (GNOME_DRUID_PAGE_EDGE (object),
					GDK_PIXBUF (g_value_get_object (value)));
}

void GLADEGNOME_API
glade_gnome_dpe_set_watermark (GObject *object, GValue *value)
{
	g_return_if_fail (GNOME_IS_DRUID_PAGE_EDGE (object));
	
	gnome_druid_page_edge_set_logo (GNOME_DRUID_PAGE_EDGE (object),
					GDK_PIXBUF (g_value_get_object (value)));
}

void GLADEGNOME_API
glade_gnome_dpe_set_top_watermark (GObject *object, GValue *value)
{
	g_return_if_fail (GNOME_IS_DRUID_PAGE_EDGE (object));
	
	gnome_druid_page_edge_set_top_watermark (GNOME_DRUID_PAGE_EDGE (object),
						 GDK_PIXBUF (g_value_get_object (value)));
}

/* GnomeIconEntry */
void GLADEGNOME_API
glade_gnome_icon_entry_set_max (GObject *object, GValue *value)
{
	g_return_if_fail (GNOME_IS_ICON_ENTRY (object));
	
	gnome_icon_entry_set_max_saved (GNOME_ICON_ENTRY (object),
					g_value_get_uint (value));
}

/* GnomeCanvas */
typedef enum 
{
	CANVAS_X1,
	CANVAS_Y1,
	CANVAS_X2,
	CANVAS_Y2
}GnomeCanvasCoordinate;

static void
glade_gnome_canvas_set_coordinate_common (GObject *object,
					  GValue *value,
					  GnomeCanvasCoordinate coordinate)
{
	gdouble x1, y1, x2, y2;
	
	g_return_if_fail (GNOME_IS_CANVAS (object));
	
	gnome_canvas_get_scroll_region  (GNOME_CANVAS (object),
					 &x1, &y1, &x2, &y2);

	switch (coordinate)
	{
		case CANVAS_X1:
			x1 = g_value_get_float (value);
		break;
		case CANVAS_Y1:
			y1 = g_value_get_float (value);
		break;
		case CANVAS_X2:
			x2 = g_value_get_float (value);
		break;
		case CANVAS_Y2:
			y2 = g_value_get_float (value);
		break;
	}
	
	gnome_canvas_set_scroll_region  (GNOME_CANVAS (object), x1, y1, x2, y2);
}

void GLADEGNOME_API
glade_gnome_canvas_set_coordinate_x1 (GObject *object, GValue *value)
{
	glade_gnome_canvas_set_coordinate_common (object, value, CANVAS_X1);
}

void GLADEGNOME_API
glade_gnome_canvas_set_coordinate_y1 (GObject *object, GValue *value)
{
	glade_gnome_canvas_set_coordinate_common (object, value, CANVAS_Y1);
}

void GLADEGNOME_API
glade_gnome_canvas_set_coordinate_x2 (GObject *object, GValue *value)
{
	glade_gnome_canvas_set_coordinate_common (object, value, CANVAS_X2);
}

void GLADEGNOME_API
glade_gnome_canvas_set_coordinate_y2 (GObject *object, GValue *value)
{
	glade_gnome_canvas_set_coordinate_common (object, value, CANVAS_Y2);
}

void GLADEGNOME_API
glade_gnome_canvas_set_pixels (GObject *object, GValue *value)
{
	g_return_if_fail (GNOME_IS_CANVAS (object));
	
	gnome_canvas_set_pixels_per_unit (GNOME_CANVAS (object),
					  g_value_get_float (value));
}

/* GnomeDialog */
static void
glade_gnome_dialog_add_button (GladeWidget *gaction_area,
			       GObject *action_area,
			       const gchar *stock)
{
	GladeProject *project = glade_widget_get_project (gaction_area);
	static GladeWidgetClass *button_class = NULL;
	GladeWidget *gbutton;
	GObject *button;
	GEnumClass *eclass;
	GEnumValue *eval;

	if (button_class == NULL)
		button_class = glade_widget_class_get_by_type (GTK_TYPE_BUTTON);
		
	gbutton = glade_widget_class_create_widget (button_class, FALSE,
						    "parent", gaction_area, 
						    "project", project, FALSE);
	
	eclass = g_type_class_ref (glade_standard_stock_get_type ());
	if ((eval = g_enum_get_value_by_nick (eclass, stock)) != NULL)
	{
		glade_widget_property_set (gbutton, "glade-type", GLADEGTK_BUTTON_STOCK);
		glade_widget_property_set (gbutton, "stock", eval->value);
	}
	g_type_class_unref (eclass);
	
	button = glade_widget_get_object (gbutton);
	
	glade_widget_class_container_add (glade_widget_get_class (gaction_area),
					  action_area, button);
}

void GLADEGNOME_API
glade_gnome_dialog_post_create (GObject *object, GladeCreateReason reason)
{
	GnomeDialog *dialog = GNOME_DIALOG (object);
	GladeWidget *gdialog, *gvbox, *gaction_area;
	GtkWidget *separator;

	gdialog = glade_widget_get_from_gobject (object);
	
	/* Ignore close signal */
	g_signal_connect (object, "close", G_CALLBACK (gtk_true), NULL);
			
	if (GNOME_IS_PROPERTY_BOX (object))
	{
		GnomePropertyBox *pbox = GNOME_PROPERTY_BOX (object);
		
		gaction_area = glade_widget_class_create_internal
			(gdialog, G_OBJECT (pbox->notebook), "notebook",
			 glade_widget_get_name (gdialog),
			 FALSE, GLADE_CREATE_LOAD);
		if (reason == GLADE_CREATE_USER)
			glade_widget_property_set (gaction_area, "pages", 3);
		return;
	}
	
	/* vbox internal child */
	gvbox = glade_widget_class_create_internal
		(gdialog, G_OBJECT (dialog->vbox), "vbox",
		 glade_widget_get_name (gdialog),
		 FALSE, GLADE_CREATE_LOAD);
	
	glade_widget_property_set (gvbox, "size", 0);
	
	/* action area */
	dialog->action_area = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog->action_area),
				   GTK_BUTTONBOX_END);

	gtk_box_pack_end (GTK_BOX (dialog->vbox), dialog->action_area,
			   FALSE, TRUE, 0);
	gtk_widget_show (dialog->action_area);

	/* separator */
	separator = gtk_hseparator_new ();
	gtk_box_pack_end (GTK_BOX (dialog->vbox), separator,
			  FALSE, TRUE, GNOME_PAD_SMALL);
	gtk_widget_show (separator);
			
	/* action area internal child */
	gaction_area = 
		glade_widget_class_create_internal (gvbox,
						    G_OBJECT (dialog->action_area),
						    "action_area",
						    glade_widget_get_name (gvbox),
						    FALSE, GLADE_CREATE_LOAD);
	
	glade_widget_property_set (gaction_area, "size", 0);
	
	if (reason != GLADE_CREATE_USER) return;
	
	/* Add a couple of buttons */
	if (GNOME_IS_MESSAGE_BOX (object))
	{
		glade_gnome_dialog_add_button (gaction_area,
					       G_OBJECT (dialog->action_area),
					       "gtk-ok");
		glade_widget_property_set (gaction_area, "size", 1);
	}
	else
	{
		glade_gnome_dialog_add_button (gaction_area,
					       G_OBJECT (dialog->action_area),
					       "gtk-cancel");
		glade_gnome_dialog_add_button (gaction_area,
					       G_OBJECT (dialog->action_area),
					       "gtk-ok");
		glade_widget_property_set (gaction_area, "size", 2);
		glade_widget_property_set (gvbox, "size", 3);
	}
}

void GLADEGNOME_API
glade_gnome_dialog_get_internal_child (GObject *object, 
				       const gchar *name,
				       GObject **child)
{
	g_return_if_fail (GNOME_IS_DIALOG (object));

	if (strcmp (name, "vbox") == 0)
		*child = G_OBJECT (GNOME_DIALOG (object)->vbox);
	else
	if (GNOME_IS_PROPERTY_BOX (object) && strcmp (name, "notebook") == 0)
		*child = G_OBJECT (GNOME_PROPERTY_BOX (object)->notebook);
	else
		*child = NULL;
}

GList * GLADEGNOME_API
glade_gnome_dialog_get_children (GObject *object)
{
	GList *list = NULL;
	GnomeDialog *dialog;
	
	g_return_val_if_fail (GNOME_IS_DIALOG (object), NULL);
	
	dialog = GNOME_DIALOG (object);

	if (dialog->vbox) list = g_list_append (list, G_OBJECT (dialog->vbox));
	
	if (GNOME_IS_PROPERTY_BOX (object))
	{
		GnomePropertyBox *pbox = GNOME_PROPERTY_BOX (object);
		if (pbox->notebook)
			list = g_list_append (list, G_OBJECT (pbox->notebook));
	}

	return list;
}

/* GnomeAbout */
void GLADEGNOME_API
glade_gnome_about_dialog_post_create (GObject *object, GladeCreateReason reason)
{
	g_return_if_fail (GNOME_IS_ABOUT (object));
	gtk_dialog_set_response_sensitive (GTK_DIALOG (object), GTK_RESPONSE_CLOSE, FALSE);	
}

GList * GLADEGNOME_API
glade_gnome_about_dialog_get_children (GObject *object)
{
	return NULL;
}

void GLADEGNOME_API
glade_gnome_about_set_name (GObject *object, GValue *value)
{
	if (g_value_get_string (value))
		g_object_set_property (object, "name", value);
}

void GLADEGNOME_API
glade_gnome_about_set_version (GObject *object, GValue *value)
{
	if (g_value_get_string (value))
		g_object_set_property (object, "version", value);
}

/* GnomeIconList */
void GLADEGNOME_API
glade_gnome_icon_list_post_create (GObject *object, GladeCreateReason reason)
{
	g_return_if_fail (GNOME_IS_ICON_LIST (object));
	/* Freeze the widget so we dont get the signals that cause a segfault */
	gnome_icon_list_freeze (GNOME_ICON_LIST (object));
}

/* GnomeMessageBox */
typedef enum {
	GLADE_GNOME_MESSAGE_BOX_INFO,
	GLADE_GNOME_MESSAGE_BOX_WARNING,
	GLADE_GNOME_MESSAGE_BOX_ERROR,
	GLADE_GNOME_MESSAGE_BOX_QUESTION,
	GLADE_GNOME_MESSAGE_BOX_GENERIC
}GladeGnomeMessageBoxType;
	
static GType
glade_gnome_message_box_type_get_type (void)
{
	static GType etype = 0;
	if (etype == 0) {
		static const GEnumValue values[] = {
			{ GLADE_GNOME_MESSAGE_BOX_INFO, "GNOME_MESSAGE_BOX_INFO", "info"},
			{ GLADE_GNOME_MESSAGE_BOX_WARNING, "GNOME_MESSAGE_BOX_WARNING", "warning"},
			{ GLADE_GNOME_MESSAGE_BOX_ERROR, "GNOME_MESSAGE_BOX_ERROR", "error"},
			{ GLADE_GNOME_MESSAGE_BOX_QUESTION, "GNOME_MESSAGE_BOX_QUESTION", "question"},
			{ GLADE_GNOME_MESSAGE_BOX_GENERIC, "GNOME_MESSAGE_BOX_GENERIC", "generic"},
			{ 0, NULL, NULL }
		};
		etype = g_enum_register_static ("GladeGnomeMessageBoxType", values);
	}
	return etype;
}

GParamSpec * GLADEGNOME_API
glade_gnome_message_box_type_spec (void)
{
	return g_param_spec_enum ("message_box_type", _("Message box type"), 
				  _("The type of the message box"),
				  glade_gnome_message_box_type_get_type (),
				  0, G_PARAM_READWRITE);
}

static gchar *
glade_gnome_message_get_str (GladeGnomeMessageBoxType val)
{
	switch (val)
	{
		case GLADE_GNOME_MESSAGE_BOX_INFO:
			return "info";
		case GLADE_GNOME_MESSAGE_BOX_WARNING:
			return "warning";
		case GLADE_GNOME_MESSAGE_BOX_ERROR:
			return "error";
		case GLADE_GNOME_MESSAGE_BOX_QUESTION:
			return "question";
		case GLADE_GNOME_MESSAGE_BOX_GENERIC:
			return "generic";
	}
	return "";
}

static void
glade_gnome_message_clean (GObject *object)
{
	GtkContainer *container = GTK_CONTAINER (GNOME_DIALOG (object)->vbox);
	GList *children, *l;

	children = l = gtk_container_get_children (container);
	
	while (l)
	{
		GtkWidget *child = (GtkWidget *) l->data;
		
		if (GTK_IS_HBOX (child))
		{
			gtk_container_remove (container, child);
			break;
		}
		
		l = l->next;
	}
	
	g_list_free (children);
}
	
void GLADEGNOME_API
glade_gnome_message_box_set_type (GObject *object, GValue *value)
{
	gchar *message, *type;
	
	g_return_if_fail (GNOME_IS_MESSAGE_BOX (object));
	
	glade_gnome_message_clean (object);
	glade_widget_property_get (glade_widget_get_from_gobject (object),
				   "message", &message);
	type = glade_gnome_message_get_str (g_value_get_enum (value));
	gnome_message_box_construct (GNOME_MESSAGE_BOX (object),
				     message, type, NULL); 
}

void GLADEGNOME_API
glade_gnome_message_box_set_message (GObject *object, GValue *value)
{
	GladeGnomeMessageBoxType type;

	g_return_if_fail (GNOME_IS_MESSAGE_BOX (object));
	
	glade_gnome_message_clean (object);
	glade_widget_property_get (glade_widget_get_from_gobject (object),
				   "message-box-type", &type);
	gnome_message_box_construct (GNOME_MESSAGE_BOX (object),
				     g_value_get_string (value),
				     glade_gnome_message_get_str (type),
				     NULL);	
}

/* GnomeEntry & GnomeFileEntry */
/* GnomeFileEntry is not derived from GnomeEntry... but hey!!! they should :) */
void GLADEGNOME_API
glade_gnome_entry_get_internal_child (GObject *object, 
				      const gchar *name,
				      GObject **child)
{
	g_return_if_fail (GNOME_IS_ENTRY (object) || GNOME_IS_FILE_ENTRY (object));

	if (strcmp (name, "entry") == 0)
	{
		if (GNOME_IS_ENTRY (object))
			*child = G_OBJECT (gnome_entry_gtk_entry (GNOME_ENTRY (object)));
		else
			*child = G_OBJECT (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (object)));
	}
	else
		*child = NULL;
}

void GLADEGNOME_API
glade_gnome_entry_post_create (GObject *object, GladeCreateReason reason)
{
	GladeWidget *gentry;
	GObject *child;

	g_return_if_fail (GNOME_IS_ENTRY (object) || GNOME_IS_FILE_ENTRY (object));

	glade_gnome_entry_get_internal_child (object, "entry", &child);
	
	gentry = glade_widget_get_from_gobject (object);
	glade_widget_class_create_internal (gentry,
					    child, "entry",
					    glade_widget_get_name (gentry),
					    FALSE, reason);
}

GList * GLADEGNOME_API
glade_gnome_entry_get_children (GObject *object)
{
	GList *list = NULL;
	GtkWidget *entry;
	
	g_return_val_if_fail (GNOME_IS_ENTRY (object) || GNOME_IS_FILE_ENTRY (object), NULL);

	if (GNOME_IS_ENTRY (object))
		entry = gnome_entry_gtk_entry (GNOME_ENTRY (object));
	else
		entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (object));
	
	if (entry) list = g_list_append (list, G_OBJECT (entry));
	
	return list;
}

void GLADEGNOME_API
glade_gnome_entry_set_max_saved (GObject *object, GValue *value)
{
	g_return_if_fail (GNOME_IS_ENTRY (object) || GNOME_IS_FILE_ENTRY (object));
	
	if (GNOME_IS_FILE_ENTRY (object))
		object = G_OBJECT (gnome_file_entry_gnome_entry (GNOME_FILE_ENTRY (object)));

	gnome_entry_set_max_saved (GNOME_ENTRY (object), g_value_get_int (value));
}

/* GnomePixmapEntry */
void GLADEGNOME_API
glade_gnome_pixmap_entry_set_do_preview (GObject *object, GValue *value)
{
	g_return_if_fail (GNOME_IS_PIXMAP_ENTRY (object));
	
	gnome_pixmap_entry_set_preview (GNOME_PIXMAP_ENTRY (object),
					g_value_get_boolean (value));
}

/* GnomeFontPicker */
void GLADEGNOME_API
glade_gnome_font_picker_set_mode (GObject *object, GValue *value)
{
	GladeWidget *ggfp, *gchild;
	GnomeFontPicker *gfp;
	GnomeFontPickerMode mode;
	GObject *child;
	
	g_return_if_fail (GNOME_IS_FONT_PICKER (object));
	
	mode = g_value_get_enum (value);
	if (mode == GNOME_FONT_PICKER_MODE_UNKNOWN) return;
	
	gfp = GNOME_FONT_PICKER (object);
	child = G_OBJECT (gnome_font_picker_uw_get_widget (gfp));
	if (child && (gchild = glade_widget_get_from_gobject (child)))
		glade_project_remove_object (glade_widget_get_project (gchild),
					     child);
	
	gnome_font_picker_set_mode (gfp, mode);

	ggfp = glade_widget_get_from_gobject (object);
	switch (mode)
	{
		const gchar *reason;
		case GNOME_FONT_PICKER_MODE_FONT_INFO:
			glade_widget_property_set_sensitive (ggfp, "show-size", TRUE, NULL);
			glade_widget_property_set_sensitive (ggfp, "use-font-in-label", TRUE, NULL);
			glade_widget_property_set_sensitive (ggfp, "label-font-size", TRUE, NULL);
		break;
		case GNOME_FONT_PICKER_MODE_USER_WIDGET:
			gnome_font_picker_uw_set_widget (gfp, glade_placeholder_new ());
		case GNOME_FONT_PICKER_MODE_PIXMAP:
			reason = _("This property is valid only in font information mode");
			glade_widget_property_set_sensitive (ggfp, "show-size", FALSE, reason);
			glade_widget_property_set_sensitive (ggfp, "use-font-in-label", FALSE, reason);
			glade_widget_property_set_sensitive (ggfp, "label-font-size", FALSE, reason);
		default: break;
	}
}

GList * GLADEGNOME_API
glade_gnome_font_picker_get_children (GObject *object)
{
	GtkWidget *child;
	
	if ((child = gnome_font_picker_uw_get_widget (GNOME_FONT_PICKER (object))))
		return g_list_append (NULL, G_OBJECT (child));
	else
		return NULL;
}

void GLADEGNOME_API
glade_gnome_font_picker_add_child (GtkWidget *container, GtkWidget *child)
{
	gnome_font_picker_uw_set_widget (GNOME_FONT_PICKER (container), child);	
}

void GLADEGNOME_API
glade_gnome_font_picker_remove_child (GtkWidget *container, GtkWidget *child)
{
	g_return_if_fail (GNOME_IS_FONT_PICKER (container));
	gnome_font_picker_uw_set_widget (GNOME_FONT_PICKER (container), glade_placeholder_new ());
}

void GLADEGNOME_API
glade_gnome_font_picker_replace_child (GtkWidget *container,
				       GtkWidget *current,
				       GtkWidget *new)
{
	g_return_if_fail (GNOME_IS_FONT_PICKER (container));
	gnome_font_picker_uw_set_widget (GNOME_FONT_PICKER (container), new);
}

/* GnomeIconList */
static GType
glade_gnome_icon_list_selection_mode_get_type (void)
{
	static GType etype = 0;
	if (etype == 0) {
		static const GEnumValue values[] = {
			{ GTK_SELECTION_SINGLE, "GTK_SELECTION_SINGLE", "Single"},
			{ GTK_SELECTION_BROWSE, "GTK_SELECTION_BROWSE", "Browse"},
			{ GTK_SELECTION_MULTIPLE, "GTK_SELECTION_MULTIPLE", "Multiple"},
			{ 0, NULL, NULL }
		};
		etype = g_enum_register_static ("GladeGnomeIconListSelectionMode", values);
	}
	return etype;
}

GParamSpec * GLADEGNOME_API
glade_gnome_icon_list_selection_mode_spec (void)
{
	return g_param_spec_enum ("selection_mode", _("Selection Mode"), 
				  _("Choose the Selection Mode"),
				  glade_gnome_icon_list_selection_mode_get_type (),
				  GTK_SELECTION_SINGLE, G_PARAM_READWRITE);
}

void GLADEGNOME_API
glade_gnome_icon_list_set_selection_mode (GObject *object, GValue *value)
{
	g_return_if_fail (GNOME_IS_ICON_LIST (object));
	
	gnome_icon_list_set_selection_mode (GNOME_ICON_LIST (object),
					    g_value_get_enum (value));
}

void GLADEGNOME_API
glade_gnome_icon_list_set_icon_width (GObject *object, GValue *value)
{
	g_return_if_fail (GNOME_IS_ICON_LIST (object));
	
	gnome_icon_list_set_icon_width (GNOME_ICON_LIST (object),
					g_value_get_int (value));
}

void GLADEGNOME_API
glade_gnome_icon_list_set_row_spacing (GObject *object, GValue *value)
{
	g_return_if_fail (GNOME_IS_ICON_LIST (object));
	
	gnome_icon_list_set_row_spacing (GNOME_ICON_LIST (object),
					 g_value_get_int (value));
}

void GLADEGNOME_API
glade_gnome_icon_list_set_column_spacing (GObject *object, GValue *value)
{
	g_return_if_fail (GNOME_IS_ICON_LIST (object));
	
	gnome_icon_list_set_col_spacing (GNOME_ICON_LIST (object),
					 g_value_get_int (value));
}

void GLADEGNOME_API
glade_gnome_icon_list_set_text_spacing (GObject *object, GValue *value)
{
	g_return_if_fail (GNOME_IS_ICON_LIST (object));
	
	gnome_icon_list_set_text_spacing (GNOME_ICON_LIST (object),
					  g_value_get_int (value));
}

/* GnomePixmap */
static gint
glade_gnome_pixmap_set_filename_common (GObject *object)
{
	GladeWidget *gp;
	gint width, height;
	
	gp = glade_widget_get_from_gobject (object);
	glade_widget_property_get (gp, "scaled-width", &width);
	glade_widget_property_get (gp, "scaled-height", &height);
	
	if (width && height)
	{
		GladeProperty *property = glade_widget_get_property (gp, "filename");
		gchar *file = glade_property_class_make_string_from_gvalue
					     (property->class, property->value);
		if (file)
		{
			gnome_pixmap_load_file_at_size (GNOME_PIXMAP (object),
							file, width, height);
			g_free (file);
			return 0;
		}
	}
		
	return -1;
}

void GLADEGNOME_API
glade_gnome_pixmap_set_filename (GObject *object, GValue *value)
{
	g_return_if_fail (GNOME_IS_PIXMAP (object));

	if (glade_gnome_pixmap_set_filename_common (object))
		gtk_image_set_from_pixbuf (GTK_IMAGE(object),
					   GDK_PIXBUF (g_value_get_object (value)));
}

static void
glade_gnome_pixmap_set_scaled_common (GObject *object,
				      GValue *value,
				      const gchar *property)
{
	g_return_if_fail (GNOME_IS_PIXMAP (object));
	
	if (glade_gnome_pixmap_set_filename_common (object))
	{
		GladeWidget *gp = glade_widget_get_from_gobject (object);
		gint val2, val = g_value_get_int (value);
		GObject *pixbuf;
		
		glade_widget_property_get (gp, "filename", &pixbuf);
		glade_widget_property_set (gp, "filename", pixbuf);
		
		if (val)
		{
			glade_widget_property_get (gp, property, &val2);
			if (val2 == 0)
				glade_widget_property_set (gp, property, val);
		}
		else
			glade_widget_property_set (gp, property, 0);
	}
}

void GLADEGNOME_API
glade_gnome_pixmap_set_scaled_width (GObject *object, GValue *value)
{
	glade_gnome_pixmap_set_scaled_common (object, value, "scaled-height");
}

void GLADEGNOME_API
glade_gnome_pixmap_set_scaled_height (GObject *object, GValue *value)
{
	glade_gnome_pixmap_set_scaled_common (object, value, "scaled-width");
}

/*
 BBBB     OOOO    NN   NN    OOOO    BBBB     OOOO
 BB  B   OO  OO   NNN  NN   OO  OO   BB  B   OO  OO
 BBBB   OO    OO  NN N NN  OO    OO  BBBB   OO    OO
 BB  B   OO  OO   NN  NNN   OO  OO   BB  B   OO  OO
 BBBB     OOOO    NN   NN    OOOO    BBBB     OOOO
*/

/* GladeGnomeBonoboDockPlacement */
static GType
glade_gnome_bonobo_dock_placement_get_type (void)
{
	static GType etype = 0;
	if (etype == 0) {
		static const GEnumValue values[] = {
			{ BONOBO_DOCK_TOP, "BONOBO_DOCK_TOP", "Top"},
			{ BONOBO_DOCK_RIGHT, "BONOBO_DOCK_RIGHT", "Right"},
			{ BONOBO_DOCK_BOTTOM, "BONOBO_DOCK_BOTTOM", "Bottom"},
			{ BONOBO_DOCK_LEFT, "BONOBO_DOCK_LEFT", "Left"},
			{ BONOBO_DOCK_FLOATING, "BONOBO_DOCK_FLOATING", "Floating"},
			{ 0, NULL, NULL }
		};
		etype = g_enum_register_static ("GladeGnomeBonoboDockPlacement", values);
	}
	return etype;
}

GParamSpec * GLADEGNOME_API
glade_gnome_bonobo_dock_placement_spec (void)
{
	return g_param_spec_enum ("placement", _("Placement"), 
				  _("Choose the BonoboDockPlacement type"),
				  glade_gnome_bonobo_dock_placement_get_type (),
				  0, G_PARAM_READWRITE);
}

/* GladeGnomeBonoboDockItemBehavior */
static GType
glade_gnome_bonobo_dock_item_behavior_get_type (void)
{
	static GType etype = 0;
	if (etype == 0) {
		static const GFlagsValue values[] = {
			/*BONOBO_DOCK_ITEM_BEH_NORMAL is 0 so we do not include it. */
			{ BONOBO_DOCK_ITEM_BEH_EXCLUSIVE, "BONOBO_DOCK_ITEM_BEH_EXCLUSIVE", "Exclusive"},
			{ BONOBO_DOCK_ITEM_BEH_NEVER_FLOATING, "BONOBO_DOCK_ITEM_BEH_NEVER_FLOATING", "Never Floating"},
			{ BONOBO_DOCK_ITEM_BEH_NEVER_VERTICAL, "BONOBO_DOCK_ITEM_BEH_NEVER_VERTICAL", "Never Vertical"},
			{ BONOBO_DOCK_ITEM_BEH_NEVER_HORIZONTAL, "BONOBO_DOCK_ITEM_BEH_NEVER_HORIZONTAL", "Never Horizontal"},
			{ BONOBO_DOCK_ITEM_BEH_LOCKED, "BONOBO_DOCK_ITEM_BEH_LOCKED", "Locked"},
			{ 0, NULL, NULL }
		};
		etype = g_flags_register_static ("GladeGnomeBonoboDockItemBehavior", values);
	}
	return etype;
}

GParamSpec * GLADEGNOME_API
glade_gnome_bonobo_dock_item_behavior_spec (void)
{
	return g_param_spec_flags ("behavior", _("Behavior"), 
				  _("Choose the BonoboDockItemBehavior type"),
				  glade_gnome_bonobo_dock_item_behavior_get_type (),
				  0, G_PARAM_READWRITE);
}

/* GtkPackType */
GParamSpec * GLADEGNOME_API
glade_gnome_gtk_pack_type_spec (void)
{
	return g_param_spec_enum ("pack_type", _("Pack Type"), 
				  _("Choose the Pack Type"),
				  g_type_from_name ("GtkPackType"),
				  0, G_PARAM_READWRITE);
}

/* BonoboDock */
void GLADEGNOME_API
glade_gnome_bonobodock_add_item (GObject *object, GObject *child)
{
	bonobo_dock_add_item (BONOBO_DOCK (object), BONOBO_DOCK_ITEM (child),
			      0,0,0,0, TRUE);
}

void GLADEGNOME_API
glade_gnome_bonobodock_add_client_area (GObject *object, GObject *child)
{
	bonobo_dock_set_client_area (BONOBO_DOCK (object), GTK_WIDGET (child));
}

GList * GLADEGNOME_API
glade_gnome_bonobodock_get_children (GObject *object)
{
	GList *list = NULL, *l;
	BonoboDock *dock = BONOBO_DOCK (object);
	BonoboDockLayout *layout = bonobo_dock_get_layout (dock);
	
	for (l = layout->items; l; l = l->next)
	{
		BonoboDockLayoutItem *li = (BonoboDockLayoutItem *) l->data;
		list = g_list_prepend (list, li->item);
	}
	
	return list;
}

GList * GLADEGNOME_API
glade_gnome_bonobodock_get_client_area (GObject *object)
{
	GtkWidget *client_area = bonobo_dock_get_client_area (BONOBO_DOCK (object));
	
	if (client_area) return g_list_append (NULL, client_area);
	
	return NULL;
}

void GLADEGNOME_API
glade_gnome_bonobodock_replace_client_area (GtkWidget *container,
					    GtkWidget *current,
					    GtkWidget *new)
{
	bonobo_dock_set_client_area (BONOBO_DOCK (container), new);
}

static BonoboDockBand *
glade_gnome_bdb_get_band (GList *bands, GtkWidget *widget)
{
	GList *l;
	
	for (l = bands; l; l = l->next)
	{
		BonoboDockBand *band = (BonoboDockBand *) l->data;
		GList *cl;
		for (cl = band->children; cl; cl = cl->next)
		{
			BonoboDockBandChild *child = (BonoboDockBandChild *) cl->data;
			if (child->widget == widget) return band;
		}
	}
	
	return NULL;
}

static BonoboDockBand *
glade_gnome_bd_get_band (BonoboDock *dock, GtkWidget *widget)
{
	BonoboDockBand *retval;
	
	retval = glade_gnome_bdb_get_band (dock->top_bands, widget);
	if (retval) return retval;

	retval = glade_gnome_bdb_get_band (dock->bottom_bands, widget);
	if (retval) return retval;

	retval = glade_gnome_bdb_get_band (dock->right_bands, widget);
	if (retval) return retval;

	retval = glade_gnome_bdb_get_band (dock->left_bands, widget);
	if (retval) return retval;
		
	return NULL;
}

static gboolean
glade_gnome_bonobodockitem_get_props (BonoboDock *doc,
				      BonoboDockItem *item,
				      BonoboDockPlacement *placement,
				      guint *band_num,
				      guint *band_position,
				      guint *offset)
{
	BonoboDockLayout *layout = bonobo_dock_get_layout (doc);
        GList *l;
        
        for (l = layout->items; l; l = l->next)
        {
                BonoboDockLayoutItem *li = (BonoboDockLayoutItem *) l->data;
                if (li->item == item)
		{
			*placement = li->placement;
			*band_num = li->position.docked.band_num;
			*band_position = li->position.docked.band_position;
			*offset = li->position.docked.offset;
			
			return TRUE;
		}
        }
        
        return FALSE;
}

void GLADEGNOME_API
glade_gnome_bonobodock_set_child_property (GObject *container,
					   GObject *child,
					   const gchar *property_name,
					   GValue *value)
{
	BonoboDock *dock;
	BonoboDockItem *item;
	BonoboDockPlacement placement;
	guint band_num, band_position, offset;
	BonoboDockBand *band;
	GtkWidget *wchild;
	gboolean new_band = FALSE;
	
	g_return_if_fail (BONOBO_IS_DOCK (container));

	dock = BONOBO_DOCK (container);
	item = BONOBO_DOCK_ITEM (child);
	
	if (strcmp ("behavior", property_name) == 0)
	{
		bonobo_dock_item_set_behavior (item, g_value_get_flags (value));
		return;
	}
	
	wchild = GTK_WIDGET (child);
	glade_gnome_bonobodockitem_get_props (dock, item, &placement, &band_num,
					      &band_position, &offset);
	
	if (strcmp ("placement", property_name) == 0)
		placement = g_value_get_enum (value);
	else if (strcmp ("position", property_name) == 0)
		band_position = g_value_get_int (value);
	else if (strcmp ("band", property_name) == 0)
		band_num = g_value_get_int (value);
	else if (strcmp ("offset", property_name) == 0)
		offset = g_value_get_int (value);
	else
	{
		g_warning ("No BonoboDock set packing property support for '%s'.", property_name);
		return;
	}
	
	if ((band = glade_gnome_bd_get_band (dock, wchild)))
	{		
		g_object_ref (child);
		gtk_container_remove (GTK_CONTAINER (band), wchild);
		
		if (band->num_children == 0)
		{
			new_band = TRUE;
			gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET(band));
		}
		
		bonobo_dock_add_item (dock, item, placement, band_num, 
				      band_position, offset, new_band);
		bonobo_dock_item_set_behavior (item, item->behavior);
		g_object_unref (child);
	}
	else
		g_warning ("BonoboDockItem's band not found.\n");
}

void GLADEGNOME_API
glade_gnome_bonobodock_get_child_property (GObject *container,
					   GObject *child,
					   const gchar *property_name,
					   GValue *value)
{
	BonoboDockPlacement placement;
	guint band_num, band_position, offset;
	
	g_return_if_fail (BONOBO_IS_DOCK (container));

	if (strcmp ("behavior", property_name) == 0)
	{
		g_value_set_flags (value, BONOBO_DOCK_ITEM (child)->behavior);
		return;
	}
	
	glade_gnome_bonobodockitem_get_props (BONOBO_DOCK (container),
					      BONOBO_DOCK_ITEM (child),
					      &placement, &band_num,
					      &band_position, &offset);
	
	if (strcmp ("placement", property_name) == 0)
		g_value_set_enum (value, placement);
	else if (strcmp ("position", property_name) == 0)
		g_value_set_int (value, band_position);
	else if (strcmp ("band", property_name) == 0)
		g_value_set_int (value, band_num);
	else if (strcmp ("offset", property_name) == 0)
		g_value_set_int (value, offset);
}

void GLADEGNOME_API
glade_gnome_bonobodock_set_allow_floating (GObject *object, GValue *value)
{
	bonobo_dock_allow_floating_items (BONOBO_DOCK (object),
					  g_value_get_boolean (value));
}

/* BonoboDockItem */
void GLADEGNOME_API
glade_gnome_bonobodockitem_post_create (GObject *object, GladeCreateReason reason)
{
	if (reason != GLADE_CREATE_USER) return;
	g_return_if_fail (BONOBO_IS_DOCK_ITEM (object));
	
	gtk_container_add (GTK_CONTAINER (object), glade_placeholder_new ());
}
