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

#include <config.h>

#include <gladeui/glade.h>
#include <gladeui/glade-editor-property.h>

#include <glade-gtk.h>

#include <libbonoboui.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>	

/* This function does absolutely nothing
 * (and is for use in overriding post_create functions).
 */
void
empty (GObject *container, GladeCreateReason reason)
{
}

/* Catalog init function */
void
glade_gnomeui_init (const gchar *name)
{
	gchar *argv[2] = {"glade-3", NULL};
	GtkStockItem items [] = {
		{ GNOME_STOCK_TIMER,              "GNOME Timer",         0, },
		{ GNOME_STOCK_TIMER_STOP,         "GNOME Timer stop",    0, },
		{ GNOME_STOCK_TRASH,              "GNOME Trash",         0, },
		{ GNOME_STOCK_TRASH_FULL,         "GNOME Trash Full",    0, },
		{ GNOME_STOCK_SCORES,             "GNOME Scores",        0, },
		{ GNOME_STOCK_ABOUT,              "GNOME About",         0, },
		{ GNOME_STOCK_BLANK,              "GNOME Blank",         0, },
		{ GNOME_STOCK_VOLUME,             "GNOME Volume",        0, },
		{ GNOME_STOCK_MIDI,               "GNOME Midi",          0, },
		{ GNOME_STOCK_MIC,                "GNOME Mic",           0, },
		{ GNOME_STOCK_LINE_IN,            "GNOME Line In",       0, },
		{ GNOME_STOCK_MAIL,               "GNOME Mail",          0, },
		{ GNOME_STOCK_MAIL_RCV,           "GNOME Mail Recive",   0, },
		{ GNOME_STOCK_MAIL_SND,           "GNOME Mail Send",     0, },
		{ GNOME_STOCK_MAIL_RPL,           "GNOME Mail Reply",    0, },
		{ GNOME_STOCK_MAIL_FWD,           "GNOME Mail Foward",   0, },
		{ GNOME_STOCK_MAIL_NEW,           "GNOME Mail New",      0, },
		{ GNOME_STOCK_ATTACH,             "GNOME Attach",        0, },
		{ GNOME_STOCK_BOOK_RED,           "GNOME Book Red",      0, },
		{ GNOME_STOCK_BOOK_GREEN,         "GNOME Book Green",    0, },
		{ GNOME_STOCK_BOOK_BLUE,          "GNOME Book Blue",     0, },
		{ GNOME_STOCK_BOOK_YELLOW,        "GNOME Book Yellow",   0, },
		{ GNOME_STOCK_BOOK_OPEN,          "GNOME Book Open",     0, },
		{ GNOME_STOCK_MULTIPLE_FILE,      "GNOME Multiple File", 0, },
		{ GNOME_STOCK_NOT,                "GNOME Not",           0, },
		{ GNOME_STOCK_TABLE_BORDERS,      "GNOME Table Borders", 0, },
		{ GNOME_STOCK_TABLE_FILL,         "GNOME Table Fill",    0, },
		{ GNOME_STOCK_TEXT_INDENT,        "GNOME Indent",        0, },
		{ GNOME_STOCK_TEXT_UNINDENT,      "GNOME Unindent",      0, },
		{ GNOME_STOCK_TEXT_BULLETED_LIST, "GNOME Bulleted List", 0, },
		{ GNOME_STOCK_TEXT_NUMBERED_LIST, "GNOME Numbered List", 0, }
	};

	gnome_program_init ("glade-3", "1.0",
			    LIBGNOMEUI_MODULE, 1, argv,
			    GNOME_PARAM_NONE);
	
	gtk_stock_add (items, sizeof (items) / sizeof (GtkStockItem));
	
	glade_standard_stock_append_prefix ("gnome-stock-");
}

/* GnomeApp */
void
glade_gnome_app_post_create (GladeWidgetAdaptor *adaptor,
			     GObject            *object, 
			     GladeCreateReason   reason)
{
	static GladeWidgetAdaptor *menubar_adaptor = NULL, *dock_item_adaptor;
	GnomeApp *app = GNOME_APP (object);
	GladeWidget *gapp = glade_widget_get_from_gobject (object);
	GladeProject *project = glade_widget_get_project (gapp);
	GladeWidget *gdock, *gdock_item, *gmenubar;
	
	/* Add BonoboDock */
	gdock = glade_widget_adaptor_create_internal
		(gapp, G_OBJECT (app->dock),
		 "dock",
		 glade_widget_get_name (gapp),
		 FALSE,
		 GLADE_CREATE_LOAD);
	
	if (reason != GLADE_CREATE_USER) return;
	
	/* Add MenuBar */
	if (menubar_adaptor == NULL)
	{
		dock_item_adaptor = glade_widget_adaptor_get_by_type (BONOBO_TYPE_DOCK_ITEM);
		menubar_adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_MENU_BAR);
	}

	/* DockItem */
	gdock_item = glade_widget_adaptor_create_widget (dock_item_adaptor, FALSE,
							 "parent", gdock, 
							 "project", project, NULL);
	glade_widget_add_child (gdock, gdock_item, FALSE);
	glade_widget_pack_property_set (gdock_item, "behavior", 
					BONOBO_DOCK_ITEM_BEH_EXCLUSIVE |
					BONOBO_DOCK_ITEM_BEH_NEVER_VERTICAL |
					BONOBO_DOCK_ITEM_BEH_LOCKED);
	/* MenuBar */
	gmenubar = glade_widget_adaptor_create_widget (menubar_adaptor, FALSE,
						       "parent", gdock_item, 
						       "project", project, NULL);

	glade_widget_add_child (gdock_item, gmenubar, FALSE);
	
	/* Add Client Area placeholder */
	bonobo_dock_set_client_area (BONOBO_DOCK (app->dock),
				     glade_placeholder_new());
	
	/* Add StatusBar */
	glade_widget_property_set (gapp, "has-statusbar", TRUE);
}

GObject *
glade_gnome_app_get_internal_child (GladeWidgetAdaptor  *adaptor,
				    GObject             *object, 
				    const gchar         *name)
{
	GObject     *child = NULL;
	GnomeApp    *app   = GNOME_APP (object);
	GladeWidget *gapp  = glade_widget_get_from_gobject (object);
	
	if (strcmp ("dock", name) == 0)
		child = G_OBJECT (app->dock);
	else if (strcmp ("appbar", name) == 0)
	{
		child = G_OBJECT (app->statusbar);
		if (child == NULL)
		{
			/*
			  Create appbar, libglade handle this as an internal
			  widget but appbar is not created by GnomeApp.
			  So we need to create it here for loading purpouses since
			  "has-statusbar" property is not saved in the .glade file.
			*/
			glade_widget_property_set (gapp, "has-statusbar", TRUE);
			child = G_OBJECT (app->statusbar);
		}
	}

	return child;
}

GList *
glade_gnome_app_get_children (GladeWidgetAdaptor  *adaptor,
			      GObject             *object)
{
	GnomeApp *app = GNOME_APP (object);
	GList *list = NULL;
	
	if (app->dock) list = g_list_append (list, G_OBJECT (app->dock));
	if (app->statusbar) list = g_list_append (list, G_OBJECT (app->statusbar));
	if (app->contents) list = g_list_append (list, G_OBJECT (app->contents));
		
	return list;
}

void
glade_gnome_app_set_child_property (GladeWidgetAdaptor  *adaptor,
				    GObject             *container,
				    GObject             *child,
				    const gchar         *property_name,
				    GValue              *value)
{
	GnomeApp *app = GNOME_APP (container);
	
	g_return_if_fail (GTK_IS_WIDGET (child));
	
	gtk_container_child_set_property (GTK_CONTAINER (app->vbox), 
					  (GNOME_IS_APPBAR (child)) 
						? gtk_widget_get_parent (GTK_WIDGET (child))
						: GTK_WIDGET (child),
					  property_name,
					  value);
}

void
glade_gnome_app_get_child_property (GladeWidgetAdaptor  *adaptor,
				    GObject             *container,
				    GObject             *child,
				    const gchar         *property_name,
				    GValue              *value)
{
	GnomeApp *app = GNOME_APP (container);
	
	g_return_if_fail (GTK_IS_WIDGET (child));
	
	gtk_container_child_get_property (GTK_CONTAINER (app->vbox), 
					  (GNOME_IS_APPBAR (child)) 
						? gtk_widget_get_parent (GTK_WIDGET (child))
						: GTK_WIDGET (child),
					  property_name,
					  value);
}

static void
glade_gnome_app_set_has_statusbar (GObject *object, const GValue *value)
{
	GnomeApp *app = GNOME_APP (object);
	GladeWidget *gapp = glade_widget_get_from_gobject (object), *gbar;
	
	if (g_value_get_boolean (value))
	{
		if (app->statusbar == NULL)
		{
			GtkWidget *bar = gnome_appbar_new (TRUE, TRUE,
						GNOME_PREFERENCES_NEVER);

			gnome_app_set_statusbar (app, bar);
			
			gbar = glade_widget_adaptor_create_internal
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
			/* FIXME: we need to destroy GladeWidgets too */
			glade_project_remove_object (glade_widget_get_project (gapp),
						     G_OBJECT (app->statusbar));
			gtk_container_remove (GTK_CONTAINER (app->vbox),
					      gtk_widget_get_parent (app->statusbar));
			app->statusbar = NULL;
		}
	}
}


void
glade_gnome_app_set_property (GladeWidgetAdaptor *adaptor,
			      GObject            *object,
			      const gchar        *id,
			      const GValue       *value)
{
	if (!strcmp (id, "has-statusbar"))
		glade_gnome_app_set_has_statusbar (object, value);
	else if (!strcmp (id, "enable-layout-config"))
		/* do nothing */;
	else
		GWA_GET_CLASS (GTK_TYPE_WINDOW)->set_property (adaptor,
							       object,
							       id, value);
}

/* GnomeAppBar */
GType
gnome_app_bar_get_type (void)
{
	return gnome_appbar_get_type ();
}

void
glade_gnome_app_bar_post_create (GladeWidgetAdaptor  *adaptor,
				 GObject             *object, 
				 GladeCreateReason    reason)
{
	gnome_appbar_set_status (GNOME_APPBAR (object), _("Status Message."));
}

/* GnomeDateEdit */
static void
glade_gnome_date_edit_set_no_show_all (GtkWidget *widget, gpointer data)
{
	gtk_widget_set_no_show_all (widget, TRUE);
}

void
glade_gnome_date_edit_post_create (GladeWidgetAdaptor  *adaptor,
				   GObject             *object, 
				   GladeCreateReason    reason)
{
	/* DateEdit's "dateedit-flags" property hides/shows some widgets so we 
	 * need to explicitly tell that they should not be affected by 
	 * gtk_widget_show_all() (its, for example, called after a paste)
	 */
	gtk_container_foreach (GTK_CONTAINER (object),
			       glade_gnome_date_edit_set_no_show_all, NULL);
}

/* GnomeDruid */
static GladeWidget *
glade_gnome_druid_add_page (GladeWidget *gdruid, gboolean edge)
{
	GladeWidget *gpage;
	static GladeWidgetAdaptor *dps_adaptor = NULL, *dpe_adaptor;
	GladeProject *project = glade_widget_get_project (gdruid);
	
	if (dps_adaptor == NULL)
	{
		dps_adaptor = glade_widget_adaptor_get_by_type (GNOME_TYPE_DRUID_PAGE_STANDARD);
		dpe_adaptor = glade_widget_adaptor_get_by_type (GNOME_TYPE_DRUID_PAGE_EDGE);
	}
	
	gpage = glade_widget_adaptor_create_widget (edge ? dpe_adaptor : dps_adaptor, FALSE,
						    "parent", gdruid, 
						    "project", project, NULL);
	
	glade_widget_add_child (gdruid, gpage, FALSE);

	return gpage;
}

void
glade_gnome_druid_post_create (GladeWidgetAdaptor  *adaptor,
			       GObject             *object, 
			       GladeCreateReason    reason)
{
	GladeWidget *gdruid, *gpage;

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

void
glade_gnome_druid_add_child (GladeWidgetAdaptor  *adaptor,
			     GObject             *container, 
			     GObject             *child)
{
	g_return_if_fail (GNOME_IS_DRUID_PAGE (child));

	/*
	  Disconnect handlers just in case this child was already added
	  in another druid.
	 */
	g_signal_handlers_disconnect_matched (child, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					      glade_gnome_druid_page_cb, NULL);

	gnome_druid_append_page (GNOME_DRUID (container), GNOME_DRUID_PAGE (child));
	
	g_signal_connect (child, "next", G_CALLBACK (glade_gnome_druid_page_cb),
					GINT_TO_POINTER (TRUE));
	g_signal_connect (child, "back", G_CALLBACK (glade_gnome_druid_page_cb),
					GINT_TO_POINTER (FALSE));	
}

void
glade_gnome_druid_remove_child (GladeWidgetAdaptor  *adaptor,
				GObject             *container, 
				GObject             *child)
{
	g_return_if_fail (GNOME_IS_DRUID_PAGE (child));

	g_signal_handlers_disconnect_matched (child, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
					      glade_gnome_druid_page_cb, NULL);
	
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

void
glade_gnome_druid_set_child_property (GladeWidgetAdaptor  *adaptor,
				      GObject             *container,
				      GObject             *child,
				      const gchar         *property_name,
				      GValue              *value)
{
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
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->child_set_property (adaptor,
								  container, 
								  child,
								  property_name,
								  value);
}

void
glade_gnome_druid_get_child_property (GladeWidgetAdaptor  *adaptor,
				      GObject             *container,
				      GObject             *child,
				      const gchar         *property_name,
				      GValue              *value)
{
	g_return_if_fail (GNOME_IS_DRUID_PAGE (child));
	
	if (strcmp (property_name, "position") == 0)
		g_value_set_int (value, glade_gnome_druid_get_page_position (
							GNOME_DRUID (container),
							GNOME_DRUID_PAGE (child)));
	else
		/* Chain Up */
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->child_get_property (adaptor,
								  container, 
								  child,
								  property_name,
								  value);
}

/* GnomeDruidPageStandard */
void
glade_gnome_dps_post_create (GladeWidgetAdaptor  *adaptor,
			     GObject             *object, 
			     GladeCreateReason    reason)
{
	GladeWidget *gpage, *gvbox;
	GObject *vbox;
	
	gpage = glade_widget_get_from_gobject (object);
	vbox = G_OBJECT (GNOME_DRUID_PAGE_STANDARD (object)->vbox);
	gvbox = glade_widget_adaptor_create_internal (gpage, vbox, "vbox",
						      glade_widget_get_name (gpage),
						      FALSE, GLADE_CREATE_LOAD);
	
	if (reason == GLADE_CREATE_USER)
		glade_widget_property_set (gvbox, "size", 1);
}

GObject *
glade_gnome_dps_get_internal_child (GladeWidgetAdaptor  *adaptor,
				    GObject             *object, 
				    const gchar         *name)
{
	GObject *child = NULL;
	
	if (strcmp (name, "vbox") == 0)
		child = G_OBJECT (GNOME_DRUID_PAGE_STANDARD (object)->vbox);

	return child;
}

GList *
glade_gnome_dps_get_children (GladeWidgetAdaptor  *adaptor,
			      GObject             *object)
{
	GnomeDruidPageStandard *page = GNOME_DRUID_PAGE_STANDARD (object);
	GList *list = NULL;
	
	if (page->vbox) list = g_list_append (list, G_OBJECT (page->vbox));
		
	return list;
}

static void
glade_gnome_dps_set_color_common (GObject      *object,
				  const gchar  *property_name,
				  const GValue *value)
{
	GladeProperty *prop;
	const gchar *color_str;
	GValue *color;
	
	if ((color_str = g_value_get_string (value)) == NULL) return;

	prop = glade_widget_get_property (glade_widget_get_from_gobject (object),
					  property_name);
	
	color = glade_property_class_make_gvalue_from_string (prop->klass,
							      color_str, NULL, NULL);
	if (color) glade_property_set_value (prop, color);
}

void
glade_gnome_dps_set_property (GladeWidgetAdaptor *adaptor,
			      GObject            *object,
			      const gchar        *id,
			      const GValue       *value)
{

	if (!strcmp (id, "background"))
		glade_gnome_dps_set_color_common (object, "background-gdk", value);
	else if (!strcmp (id, "contents-background"))
		glade_gnome_dps_set_color_common (object, "contents-background-gdk", value);
	else if (!strcmp (id, "logo-background"))
		glade_gnome_dps_set_color_common (object, "logo-background-gdk", value);
	else if (!strcmp (id, "title-foreground"))
		glade_gnome_dps_set_color_common (object, "title-foreground-gdk", value);


	else if (!strcmp (id, "background-gdk")  || 
		 !strcmp (id, "title-foreground-gdk") ||
		 !strcmp (id, "logo-background-gdk") ||
		 !strcmp (id, "contents-background-gdk"))
		{
			/* XXX Ignore these they crash */
		}
	else
		/* Skip GNOME_TYPE_DRUID_PAGE since we didnt register an
		 * adaptor for that abstract class.
		 */
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor,
								  object,
								  id, value);
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

GParamSpec *
glade_gnome_dpe_position_spec (void)
{
	return g_param_spec_enum ("position", _("Position"), 
				  _("The position in the druid"),
				  glade_gnome_dpe_position_get_type (),
				  GNOME_EDGE_OTHER, G_PARAM_READWRITE);
}

void
glade_gnome_dpe_set_property (GladeWidgetAdaptor *adaptor,
			      GObject            *object,
			      const gchar        *id,
			      const GValue       *value)

{
	GnomeDruidPageEdge *page = GNOME_DRUID_PAGE_EDGE (object);
	const gchar *text = NULL;
	GObject *pixbuf = NULL;
	GdkColor *color = NULL;
	
	if (G_VALUE_HOLDS (value, G_TYPE_STRING))
		text = g_value_get_string (value);
	else if (G_VALUE_HOLDS (value, GDK_TYPE_PIXBUF))
		pixbuf = g_value_get_object (value);
	else if (G_VALUE_HOLDS (value, GDK_TYPE_COLOR))
		color = (GdkColor *) g_value_get_boxed (value);
	
	if (!strcmp (id, "title"))
	{
		if (text) gnome_druid_page_edge_set_title (page, text);
	}
	else if (!strcmp (id, "text"))
	{
		if (text) gnome_druid_page_edge_set_text (page, text);
	}
	else if (!strcmp (id, "title-foreground"))
	{
		if (color) gnome_druid_page_edge_set_title_color (page, color);
	}
	else if (!strcmp (id, "text-foreground"))
	{
		if (color) gnome_druid_page_edge_set_text_color (page, color);
	}
	else if (!strcmp (id, "background"))
	{
		if (color) gnome_druid_page_edge_set_bg_color (page, color);
	}
	else if (!strcmp (id, "contents-background"))
	{
		if (color) gnome_druid_page_edge_set_textbox_color (page, color);
	}
	else if (!strcmp (id, "logo-background"))
	{
		if (color) gnome_druid_page_edge_set_logo_bg_color (page, color);
	}
	else if (!strcmp (id, "logo"))
		gnome_druid_page_edge_set_logo (page, GDK_PIXBUF (pixbuf));
	else if (!strcmp (id, "watermark"))
		gnome_druid_page_edge_set_watermark (page, GDK_PIXBUF (pixbuf));
	else if (!strcmp (id, "top-watermark"))
		gnome_druid_page_edge_set_top_watermark (page, GDK_PIXBUF (pixbuf));
	else if (!strcmp (id, "position"));
	else
		/* Skip GNOME_TYPE_DRUID_PAGE since we didnt register an
		 * adaptor for that abstract class.
		 */
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor,
								  object,
								  id, value);
}

/* GnomeIconEntry */
void
glade_gnome_icon_entry_set_property (GladeWidgetAdaptor *adaptor,
				     GObject            *object,
				     const gchar        *id,
				     const GValue       *value)
{
	if (!strcmp (id, "max-saved"))
		gnome_icon_entry_set_max_saved (GNOME_ICON_ENTRY (object),
						g_value_get_uint (value));
	else
		GWA_GET_CLASS (GTK_TYPE_VBOX)->set_property (adaptor,
							     object,
							     id, value);
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
glade_gnome_canvas_set_coordinate_common (GObject               *object,
					  const GValue          *value,
					  GnomeCanvasCoordinate  coordinate)
{
	gdouble x1, y1, x2, y2;
	
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

void
glade_gnome_canvas_set_property (GladeWidgetAdaptor *adaptor,
				 GObject            *object,
				 const gchar        *id,
				 const GValue       *value)
{
	if (!strcmp (id, "pixels-per-unit"))
		gnome_canvas_set_pixels_per_unit (GNOME_CANVAS (object),
						  g_value_get_float (value));
	else if (!strcmp (id, "scroll-x1"))
		glade_gnome_canvas_set_coordinate_common (object, value, CANVAS_X1);
	else if (!strcmp (id, "scroll-x2"))
		glade_gnome_canvas_set_coordinate_common (object, value, CANVAS_X2);
	else if (!strcmp (id, "scroll-y1"))
		glade_gnome_canvas_set_coordinate_common (object, value, CANVAS_Y1);
	else if (!strcmp (id, "scroll-y2"))
		glade_gnome_canvas_set_coordinate_common (object, value, CANVAS_Y2);
	else
		GWA_GET_CLASS (GTK_TYPE_LAYOUT)->set_property (adaptor,
							       object,
							       id, value);
}

/* GnomeDialog */
static void
glade_gnome_dialog_add_button (GladeWidget *gaction_area,
			       GObject *action_area,
			       const gchar *stock)
{
	GladeProject *project = glade_widget_get_project (gaction_area);
	static GladeWidgetAdaptor *button_adaptor = NULL;
	GladeWidget *gbutton;
	GObject *button;
	GEnumClass *eclass;
	GEnumValue *eval;

	if (button_adaptor == NULL)
		button_adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_BUTTON);
		
	gbutton = glade_widget_adaptor_create_widget (button_adaptor, FALSE,
						      "parent", gaction_area, 
						      "project", project, FALSE);
	
	eclass = g_type_class_ref (glade_standard_stock_get_type ());
	if ((eval = g_enum_get_value_by_nick (eclass, stock)) != NULL)
	{
		glade_widget_property_set (gbutton, "use-stock", TRUE);
		glade_widget_property_set (gbutton, "stock", eval->value);
	}
	g_type_class_unref (eclass);
	
	button = glade_widget_get_object (gbutton);
	
	glade_widget_adaptor_add (glade_widget_get_adaptor (gaction_area),
				  action_area, button);
}

void
glade_gnome_dialog_post_create (GladeWidgetAdaptor  *adaptor,
				GObject             *object, 
				GladeCreateReason    reason)
{
	GladeWidget *gdialog = glade_widget_get_from_gobject (object);
	GnomeDialog *dialog = GNOME_DIALOG (object);
	GladeWidget *gvbox, *gaction_area;
	GtkWidget *separator;
	
	/* Ignore close signal */
	g_signal_connect (object, "close", G_CALLBACK (gtk_true), NULL);
			
	if (GNOME_IS_PROPERTY_BOX (object))
	{
		GnomePropertyBox *pbox = GNOME_PROPERTY_BOX (object);
		
		gaction_area = glade_widget_adaptor_create_internal
			(gdialog, G_OBJECT (pbox->notebook), "notebook",
			 glade_widget_get_name (gdialog),
			 FALSE, GLADE_CREATE_LOAD);
		if (reason == GLADE_CREATE_USER)
			glade_widget_property_set (gaction_area, "pages", 3);
		return;
	}
	
	/* vbox internal child */
	gvbox = glade_widget_adaptor_create_internal
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
		glade_widget_adaptor_create_internal (gvbox,
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

GObject *
glade_gnome_dialog_get_internal_child (GladeWidgetAdaptor  *adaptor,
				       GObject             *object, 
				       const gchar         *name)
{
	GObject *child = NULL;
	
	if (strcmp (name, "vbox") == 0)
		child = G_OBJECT (GNOME_DIALOG (object)->vbox);
	else if (GNOME_IS_PROPERTY_BOX (object) && strcmp (name, "notebook") == 0)
		child = G_OBJECT (GNOME_PROPERTY_BOX (object)->notebook);

	return child;
}

GList *
glade_gnome_dialog_get_children (GladeWidgetAdaptor  *adaptor,
				 GObject             *object)
{
	GnomeDialog *dialog = GNOME_DIALOG (object);
	GList *list = NULL;

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
void
glade_gnome_about_dialog_post_create (GladeWidgetAdaptor  *adaptor,
				      GObject             *object, 
				      GladeCreateReason    reason)
{
	gtk_dialog_set_response_sensitive (GTK_DIALOG (object), GTK_RESPONSE_CLOSE, FALSE);	
}

void
glade_gnome_about_dialog_set_property (GladeWidgetAdaptor *adaptor,
				       GObject            *object,
				       const gchar        *id,
				       const GValue       *value)
{
	if (!strcmp (id, "name") ||
	    !strcmp (id, "version"))
	{
		if (g_value_get_string (value))
			g_object_set_property (object, id, value);
	}
	else
		GWA_GET_CLASS (GTK_TYPE_DIALOG)->set_property (adaptor,
							       object,
							       id, value);
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

GParamSpec *
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

	children = gtk_container_get_children (container);
	
	for (l = children; l; l = l->next)
	{
		GtkWidget *child = (GtkWidget *) l->data;
		
		if (GTK_IS_HBOX (child))
		{
			gtk_container_remove (container, child);
			break;
		}
	}
	
	g_list_free (children);
}
	
static void
glade_gnome_message_box_set_type (GObject *object, const GValue *value)
{
	gchar *message, *type;
	
	glade_gnome_message_clean (object);
	glade_widget_property_get (glade_widget_get_from_gobject (object),
				   "message", &message);
	type = glade_gnome_message_get_str (g_value_get_enum (value));
	gnome_message_box_construct (GNOME_MESSAGE_BOX (object),
				     message, type, NULL); 
}

static void
glade_gnome_message_box_set_message (GObject *object, const GValue *value)
{
	GladeGnomeMessageBoxType type;
	
	glade_gnome_message_clean (object);
	glade_widget_property_get (glade_widget_get_from_gobject (object),
				   "message-box-type", &type);
	gnome_message_box_construct (GNOME_MESSAGE_BOX (object),
				     g_value_get_string (value),
				     glade_gnome_message_get_str (type),
				     NULL);	
}

void
glade_gnome_message_box_set_property (GladeWidgetAdaptor *adaptor,
				      GObject            *object,
				      const gchar        *id,
				      const GValue       *value)
{
	if (!strcmp (id, "message-box-type"))
		glade_gnome_message_box_set_type (object, value);
	else if (!strcmp (id, "message"))
		glade_gnome_message_box_set_message (object, value);
	else
		GWA_GET_CLASS (GNOME_TYPE_DIALOG)->set_property (adaptor,
								 object,
								 id, value);
}

/* GnomeEntry & GnomeFileEntry */
/* GnomeFileEntry is not derived from GnomeEntry... but hey!!! they should :) */
GObject *
glade_gnome_entry_get_internal_child (GladeWidgetAdaptor  *adaptor,
				      GObject             *object, 
				      const gchar         *name)
{
	GObject *child = NULL;

	if (strcmp (name, "entry") == 0)
	{
		if (GNOME_IS_ENTRY (object))
			child = G_OBJECT (gnome_entry_gtk_entry (GNOME_ENTRY (object)));
		else
			child = G_OBJECT (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (object)));
	}

	return child;
}

void
glade_gnome_entry_post_create (GladeWidgetAdaptor  *adaptor,
			       GObject             *object, 
			       GladeCreateReason    reason)
{
	GladeWidget *gentry;
	GObject *child;

	child = glade_gnome_entry_get_internal_child (adaptor, object, "entry");
	
	gentry = glade_widget_get_from_gobject (object);
	glade_widget_adaptor_create_internal (gentry,
					      child, "entry",
					      glade_widget_get_name (gentry),
					      FALSE, reason);
}

GList *
glade_gnome_entry_get_children (GladeWidgetAdaptor  *adaptor,
				GObject             *object)
{
	GList *list = NULL;
	GtkWidget *entry;
	
	if (GNOME_IS_ENTRY (object))
		entry = gnome_entry_gtk_entry (GNOME_ENTRY (object));
	else
		entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (object));
	
	if (entry) list = g_list_append (list, G_OBJECT (entry));
	
	return list;
}

void
glade_gnome_entry_set_property (GladeWidgetAdaptor *adaptor,
				GObject            *object,
				const gchar        *id,
				const GValue       *value)
{
	if (!strcmp (id, "max-saved"))
		gnome_entry_set_max_saved (GNOME_ENTRY (object), g_value_get_int (value));
	else
		GWA_GET_CLASS (GTK_TYPE_COMBO)->set_property (adaptor,
							      object,
							      id, value);
}

void
glade_gnome_file_entry_set_property (GladeWidgetAdaptor *adaptor,
				     GObject            *object,
				     const gchar        *id,
				     const GValue       *value)
{
	GtkWidget *entry;

	if (!strcmp (id, "max-saved"))
	{
		entry = gnome_file_entry_gnome_entry (GNOME_FILE_ENTRY (object));
		gnome_entry_set_max_saved (GNOME_ENTRY (entry), g_value_get_int (value));
	}
	else
		GWA_GET_CLASS (GTK_TYPE_VBOX)->set_property (adaptor,
							     object,
							     id, value);
}

/* GnomeFontPicker */
static void
glade_gnome_font_picker_set_mode (GObject *object, const GValue *value)
{
	GladeWidget *ggfp, *gchild;
	GnomeFontPicker *gfp;
	GnomeFontPickerMode mode;
	GObject *child;
	
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

void
glade_gnome_font_picker_set_property (GladeWidgetAdaptor *adaptor,
				      GObject            *object,
				      const gchar        *id,
				      const GValue       *value)
{
	if (!strcmp (id, "mode"))
		glade_gnome_font_picker_set_mode (object, value);
	else
		GWA_GET_CLASS (GTK_TYPE_BUTTON)->set_property (adaptor,
							       object,
							       id, value);
}

GList *
glade_gnome_font_picker_get_children (GladeWidgetAdaptor  *adaptor,
				      GObject             *object)
{
	GtkWidget *child;
		
	if ((child = gnome_font_picker_uw_get_widget (GNOME_FONT_PICKER (object))))
		return g_list_append (NULL, G_OBJECT (child));
	else
		return NULL;
}

void
glade_gnome_font_picker_add_child (GladeWidgetAdaptor  *adaptor,
				   GtkWidget           *container, 
				   GtkWidget           *child)
{
	gnome_font_picker_uw_set_widget (GNOME_FONT_PICKER (container), child);	
}

void
glade_gnome_font_picker_remove_child (GladeWidgetAdaptor  *adaptor,
				      GtkWidget           *container, 
				      GtkWidget           *child)
{
	gnome_font_picker_uw_set_widget (GNOME_FONT_PICKER (container), glade_placeholder_new ());
}

void
glade_gnome_font_picker_replace_child (GladeWidgetAdaptor  *adaptor,
				       GtkWidget           *container,
				       GtkWidget           *current,
				       GtkWidget           *new_widget)
{
	gnome_font_picker_uw_set_widget (GNOME_FONT_PICKER (container), new_widget);
}

/* GnomeIconList */
void
glade_gnome_icon_list_post_create (GladeWidgetAdaptor  *adaptor,
				   GObject             *object, 
				   GladeCreateReason    reason)
{
	/* Freeze the widget so we dont get the signals that cause a segfault */
	gnome_icon_list_freeze (GNOME_ICON_LIST (object));
}

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

GParamSpec *
glade_gnome_icon_list_selection_mode_spec (void)
{
	return g_param_spec_enum ("selection_mode", _("Selection Mode"), 
				  _("Choose the Selection Mode"),
				  glade_gnome_icon_list_selection_mode_get_type (),
				  GTK_SELECTION_SINGLE, G_PARAM_READWRITE);
}

void
glade_gnome_icon_list_set_property (GladeWidgetAdaptor *adaptor,
				    GObject            *object,
				    const gchar        *id,
				    const GValue       *value)
{
	if (!strcmp (id, "selection-mode"))
		gnome_icon_list_set_selection_mode (GNOME_ICON_LIST (object),
						    g_value_get_enum (value));
	else if (!strcmp (id, "icon-width"))
		gnome_icon_list_set_icon_width (GNOME_ICON_LIST (object),
						g_value_get_int (value));
	else if (!strcmp (id, "row-spacing"))
		gnome_icon_list_set_row_spacing (GNOME_ICON_LIST (object),
						 g_value_get_int (value));	
	else if (!strcmp (id, "column-spacing"))
		gnome_icon_list_set_col_spacing (GNOME_ICON_LIST (object),
						 g_value_get_int (value));
	else if (!strcmp (id, "text-spacing"))
		gnome_icon_list_set_text_spacing (GNOME_ICON_LIST (object),
						  g_value_get_int (value));
	else
		GWA_GET_CLASS (GNOME_TYPE_CANVAS)->set_property (adaptor,
								 object,
								 id, value);
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
		gchar *file = 
			glade_property_class_make_string_from_gvalue
			(property->klass, property->value,
			 glade_project_get_format (gp->project));

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

static void
glade_gnome_pixmap_set_scaled_common (GObject *object,
				      const GValue *value,
				      const gchar *property)
{
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

void
glade_gnome_pixmap_set_property (GladeWidgetAdaptor *adaptor,
				 GObject            *object,
				 const gchar        *id,
				 const GValue       *value)
{
	if (!strcmp (id, "filename"))
	{
		if (glade_gnome_pixmap_set_filename_common (object))
			gtk_image_set_from_pixbuf (GTK_IMAGE(object),
						   GDK_PIXBUF (g_value_get_object (value)));
	}
	else if (!strcmp (id, "scaled-width") ||
		 !strcmp (id, "scaled-height"))
		glade_gnome_pixmap_set_scaled_common (object, value, id);
	else
		GWA_GET_CLASS (GTK_TYPE_IMAGE)->set_property (adaptor,
							      object,
							      id, value);
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

GParamSpec *
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

GParamSpec *
glade_gnome_bonobo_dock_item_behavior_spec (void)
{
	return g_param_spec_flags ("behavior", _("Behavior"), 
				  _("Choose the BonoboDockItemBehavior type"),
				  glade_gnome_bonobo_dock_item_behavior_get_type (),
				  0, G_PARAM_READWRITE);
}

/* GtkPackType */
GParamSpec *
glade_gnome_gtk_pack_type_spec (void)
{
	return g_param_spec_enum ("pack_type", _("Pack Type"), 
				  _("Choose the Pack Type"),
				  g_type_from_name ("GtkPackType"),
				  0, G_PARAM_READWRITE);
}

/* BonoboDockBand convenience functions */
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
	BonoboDockBand *retval = NULL;
	
	if ((retval = glade_gnome_bdb_get_band (dock->top_bands, widget))    ||
	    (retval = glade_gnome_bdb_get_band (dock->bottom_bands, widget)) ||
	    (retval = glade_gnome_bdb_get_band (dock->right_bands, widget))  ||
	    (retval = glade_gnome_bdb_get_band (dock->left_bands, widget)));
	
	return retval;
}

/* BonoboDock */
void
glade_gnome_bonobodock_add_child (GladeWidgetAdaptor  *adaptor,
				  GObject             *object,
				  GObject             *child)
{
	if (BONOBO_IS_DOCK_ITEM (child))
		bonobo_dock_add_item (BONOBO_DOCK (object), BONOBO_DOCK_ITEM (child),
				      0,0,0,0, TRUE);
	else if (GTK_IS_WIDGET (child))
		bonobo_dock_set_client_area (BONOBO_DOCK (object), GTK_WIDGET (child));
}

void
glade_gnome_bonobodock_remove_child (GladeWidgetAdaptor  *adaptor,
				     GObject             *object, 
				     GObject             *child)
{
	BonoboDockBand *band;
		
	band = glade_gnome_bd_get_band (BONOBO_DOCK (object), GTK_WIDGET (child));
	
	gtk_container_remove (GTK_CONTAINER (band), GTK_WIDGET (child));
}

GList *
glade_gnome_bonobodock_get_children (GladeWidgetAdaptor  *adaptor,
				     GObject             *object)
{
	GList            *list = NULL, *l;
	GtkWidget        *client_area;
	BonoboDockLayout *layout;
		
	layout      = bonobo_dock_get_layout (BONOBO_DOCK (object));
	client_area = bonobo_dock_get_client_area (BONOBO_DOCK (object));
	
	for (l = layout->items; l; l = l->next)
	{
		BonoboDockLayoutItem *li = (BonoboDockLayoutItem *) l->data;
		list = g_list_prepend (list, li->item);
	}

	if (client_area) 
		list = g_list_prepend (list, client_area);
	
	return g_list_reverse (list);
}

void
glade_gnome_bonobodock_replace_child (GladeWidgetAdaptor  *adaptor,
				      GtkWidget           *container,
				      GtkWidget           *current,
				      GtkWidget           *new_widget)
{
	bonobo_dock_set_client_area (BONOBO_DOCK (container), new_widget);
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

void
glade_gnome_bonobodock_set_child_property (GladeWidgetAdaptor  *adaptor,
					   GObject             *container,
					   GObject             *child,
					   const gchar         *property_name,
					   GValue              *value)
{
	BonoboDock *dock;
	BonoboDockItem *item;
	BonoboDockPlacement placement;
	guint band_num, band_position, offset;
	BonoboDockBand *band;
	GtkWidget *wchild;
	gboolean new_band = FALSE;
	
	if (!BONOBO_IS_DOCK_ITEM (child))
		/* Ignore packing properties of client area */
		return;

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

void
glade_gnome_bonobodock_get_child_property (GladeWidgetAdaptor  *adaptor,
					   GObject             *container,
					   GObject             *child,
					   const gchar         *property_name,
					   GValue              *value)
{
	BonoboDockPlacement placement;
	guint band_num, band_position, offset;

	if (!BONOBO_IS_DOCK_ITEM (child))
		/* Ignore packing properties of client area, 
		 *
		 * FIXME: packing properties should actually be removed 
		 * from client area children using glade_widget_remove_property();
		 */
		return;

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

void
glade_gnome_bonobodock_set_property (GladeWidgetAdaptor *adaptor,
				     GObject            *object,
				     const gchar        *id,
				     const GValue       *value)
{
	if (!strcmp (id, "allow-floating"))
		bonobo_dock_allow_floating_items (BONOBO_DOCK (object),
						  g_value_get_boolean (value));
	else
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor,
								  object,
								  id, value);
}
