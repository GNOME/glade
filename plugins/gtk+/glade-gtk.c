/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2004 - 2008 Tristan Van Berkom, Juan Pablo Ugarte et al.
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
 *   Tristan Van Berkom <tvb@gnome.org>
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#include <config.h>

#include "glade-gtk.h"
#include "fixed-bg.xpm"
#include "glade-accels.h"
#include "glade-attributes.h"
#include "glade-column-types.h"
#include "glade-model-data.h"
#include "glade-icon-sources.h"
#include "glade-button-editor.h"
#include "glade-tool-button-editor.h"
#include "glade-image-editor.h"
#include "glade-image-item-editor.h"
#include "glade-icon-factory-editor.h"
#include "glade-store-editor.h"
#include "glade-label-editor.h"
#include "glade-cell-renderer-editor.h"
#include "glade-treeview-editor.h"
#include "glade-entry-editor.h"
#include "glade-activatable-editor.h"
#include "glade-tool-item-group-editor.h"

#include <gladeui/glade-editor-property.h>
#include <gladeui/glade-base-editor.h>
#include <gladeui/glade-xml-utils.h>


#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdlib.h>

/* --------------------------------- Constants ------------------------------ */
#define FIXED_DEFAULT_CHILD_WIDTH  100
#define FIXED_DEFAULT_CHILD_HEIGHT 60

#define MNEMONIC_INSENSITIVE_MSG   _("This property does not apply unless Use Underline is set.")
#define NOT_SELECTED_MSG           _("Property not selected")
#define RESPID_INSENSITIVE_MSG     _("This property is only for use in dialog action buttons")
#define ACTION_APPEARANCE_MSG      _("This property is set to be controlled by an Action")
/* -------------------------------- ParamSpecs ------------------------------ */
/*
GtkImageMenuItem GnomeUI "stock_item" property special case:
	
"stock_item" property is added by glade2 gnome support and makes reference to
GNOMEUIINFO_MENU_* macros. This set-function maps these properties to 
existing non deprecated gtk ones.
*/
typedef enum {
	GNOMEUIINFO_MENU_NONE,
	/* The 'File' menu */
	GNOMEUIINFO_MENU_NEW_ITEM,
	GNOMEUIINFO_MENU_NEW_SUBTREE,
	GNOMEUIINFO_MENU_OPEN_ITEM,
	GNOMEUIINFO_MENU_SAVE_ITEM,
	GNOMEUIINFO_MENU_SAVE_AS_ITEM,
	GNOMEUIINFO_MENU_REVERT_ITEM,
	GNOMEUIINFO_MENU_PRINT_ITEM,
	GNOMEUIINFO_MENU_PRINT_SETUP_ITEM,
	GNOMEUIINFO_MENU_CLOSE_ITEM,
	GNOMEUIINFO_MENU_EXIT_ITEM,
	GNOMEUIINFO_MENU_QUIT_ITEM,
	/* The "Edit" menu */
	GNOMEUIINFO_MENU_CUT_ITEM,
	GNOMEUIINFO_MENU_COPY_ITEM,
	GNOMEUIINFO_MENU_PASTE_ITEM,
	GNOMEUIINFO_MENU_SELECT_ALL_ITEM,
	GNOMEUIINFO_MENU_CLEAR_ITEM,
	GNOMEUIINFO_MENU_UNDO_ITEM,
	GNOMEUIINFO_MENU_REDO_ITEM,
	GNOMEUIINFO_MENU_FIND_ITEM,
	GNOMEUIINFO_MENU_FIND_AGAIN_ITEM,
	GNOMEUIINFO_MENU_REPLACE_ITEM,
	GNOMEUIINFO_MENU_PROPERTIES_ITEM,
	/* The Settings menu */
	GNOMEUIINFO_MENU_PREFERENCES_ITEM,
	/* The Windows menu */
	GNOMEUIINFO_MENU_NEW_WINDOW_ITEM,
	GNOMEUIINFO_MENU_CLOSE_WINDOW_ITEM,
	/* And the "Help" menu */
	GNOMEUIINFO_MENU_ABOUT_ITEM,
	/* The "Game" menu */
	GNOMEUIINFO_MENU_NEW_GAME_ITEM,
	GNOMEUIINFO_MENU_PAUSE_GAME_ITEM,
	GNOMEUIINFO_MENU_RESTART_GAME_ITEM,
	GNOMEUIINFO_MENU_UNDO_MOVE_ITEM,
	GNOMEUIINFO_MENU_REDO_MOVE_ITEM,
	GNOMEUIINFO_MENU_HINT_ITEM,
	GNOMEUIINFO_MENU_SCORES_ITEM,
	GNOMEUIINFO_MENU_END_GAME_ITEM,
	/* Some standard menus */
	GNOMEUIINFO_MENU_FILE_TREE,
	GNOMEUIINFO_MENU_EDIT_TREE,
	GNOMEUIINFO_MENU_VIEW_TREE,
	GNOMEUIINFO_MENU_SETTINGS_TREE,
	GNOMEUIINFO_MENU_FILES_TREE,
	GNOMEUIINFO_MENU_WINDOWS_TREE,
	GNOMEUIINFO_MENU_HELP_TREE,
	GNOMEUIINFO_MENU_GAME_TREE
} GladeGtkGnomeUIInfoEnum;

static GType
glade_gtk_gnome_ui_info_get_type (void)
{
	static GType etype = 0;
	if (etype == 0) {
		static const GEnumValue values[] = {
			{ GNOMEUIINFO_MENU_NONE, "GNOMEUIINFO_MENU_NONE", NULL},
			/* The 'File' menu */
			{ GNOMEUIINFO_MENU_NEW_ITEM, "GNOMEUIINFO_MENU_NEW_ITEM", "gtk-new"},
			{ GNOMEUIINFO_MENU_OPEN_ITEM, "GNOMEUIINFO_MENU_OPEN_ITEM", "gtk-open"},
			{ GNOMEUIINFO_MENU_SAVE_ITEM, "GNOMEUIINFO_MENU_SAVE_ITEM", "gtk-save"},
			{ GNOMEUIINFO_MENU_SAVE_AS_ITEM, "GNOMEUIINFO_MENU_SAVE_AS_ITEM", "gtk-save-as"},
			{ GNOMEUIINFO_MENU_REVERT_ITEM, "GNOMEUIINFO_MENU_REVERT_ITEM", "gtk-revert-to-saved"},
			{ GNOMEUIINFO_MENU_PRINT_ITEM, "GNOMEUIINFO_MENU_PRINT_ITEM", "gtk-print"},
			{ GNOMEUIINFO_MENU_PRINT_SETUP_ITEM, "GNOMEUIINFO_MENU_PRINT_SETUP_ITEM", NULL},
			{ GNOMEUIINFO_MENU_CLOSE_ITEM, "GNOMEUIINFO_MENU_CLOSE_ITEM", "gtk-close"},
			{ GNOMEUIINFO_MENU_EXIT_ITEM, "GNOMEUIINFO_MENU_EXIT_ITEM", "gtk-quit"},
			{ GNOMEUIINFO_MENU_QUIT_ITEM, "GNOMEUIINFO_MENU_QUIT_ITEM", "gtk-quit"},
			/* The "Edit" menu */
			{ GNOMEUIINFO_MENU_CUT_ITEM, "GNOMEUIINFO_MENU_CUT_ITEM", "gtk-cut"},
			{ GNOMEUIINFO_MENU_COPY_ITEM, "GNOMEUIINFO_MENU_COPY_ITEM", "gtk-copy"},
			{ GNOMEUIINFO_MENU_PASTE_ITEM, "GNOMEUIINFO_MENU_PASTE_ITEM", "gtk-paste"},
			{ GNOMEUIINFO_MENU_SELECT_ALL_ITEM, "GNOMEUIINFO_MENU_SELECT_ALL_ITEM", NULL},
			{ GNOMEUIINFO_MENU_CLEAR_ITEM, "GNOMEUIINFO_MENU_CLEAR_ITEM", "gtk-clear"},
			{ GNOMEUIINFO_MENU_UNDO_ITEM, "GNOMEUIINFO_MENU_UNDO_ITEM", "gtk-undo"},
			{ GNOMEUIINFO_MENU_REDO_ITEM, "GNOMEUIINFO_MENU_REDO_ITEM", "gtk-redo"},
			{ GNOMEUIINFO_MENU_FIND_ITEM, "GNOMEUIINFO_MENU_FIND_ITEM", "gtk-find"},
			{ GNOMEUIINFO_MENU_FIND_AGAIN_ITEM, "GNOMEUIINFO_MENU_FIND_AGAIN_ITEM", NULL},
			{ GNOMEUIINFO_MENU_REPLACE_ITEM, "GNOMEUIINFO_MENU_REPLACE_ITEM", "gtk-find-and-replace"},
			{ GNOMEUIINFO_MENU_PROPERTIES_ITEM, "GNOMEUIINFO_MENU_PROPERTIES_ITEM", "gtk-properties"},
			/* The Settings menu */
			{ GNOMEUIINFO_MENU_PREFERENCES_ITEM, "GNOMEUIINFO_MENU_PREFERENCES_ITEM", "gtk-preferences"},
			/* The Windows menu */
			{ GNOMEUIINFO_MENU_NEW_WINDOW_ITEM, "GNOMEUIINFO_MENU_NEW_WINDOW_ITEM", NULL},
			{ GNOMEUIINFO_MENU_CLOSE_WINDOW_ITEM, "GNOMEUIINFO_MENU_CLOSE_WINDOW_ITEM", NULL},
			/* And the "Help" menu */
			{ GNOMEUIINFO_MENU_ABOUT_ITEM, "GNOMEUIINFO_MENU_ABOUT_ITEM", "gtk-about"},
			/* The "Game" menu */
			{ GNOMEUIINFO_MENU_NEW_GAME_ITEM, "GNOMEUIINFO_MENU_NEW_GAME_ITEM", NULL},
			{ GNOMEUIINFO_MENU_PAUSE_GAME_ITEM, "GNOMEUIINFO_MENU_PAUSE_GAME_ITEM", NULL},
			{ GNOMEUIINFO_MENU_RESTART_GAME_ITEM, "GNOMEUIINFO_MENU_RESTART_GAME_ITEM", NULL},
			{ GNOMEUIINFO_MENU_UNDO_MOVE_ITEM, "GNOMEUIINFO_MENU_UNDO_MOVE_ITEM", NULL},
			{ GNOMEUIINFO_MENU_REDO_MOVE_ITEM, "GNOMEUIINFO_MENU_REDO_MOVE_ITEM", NULL},
			{ GNOMEUIINFO_MENU_HINT_ITEM, "GNOMEUIINFO_MENU_HINT_ITEM", NULL},
			{ GNOMEUIINFO_MENU_SCORES_ITEM, "GNOMEUIINFO_MENU_SCORES_ITEM", NULL},
			{ GNOMEUIINFO_MENU_END_GAME_ITEM, "GNOMEUIINFO_MENU_END_GAME_ITEM", NULL},
			/* Some standard menus */
			{ GNOMEUIINFO_MENU_FILE_TREE, "GNOMEUIINFO_MENU_FILE_TREE", NULL},
			{ GNOMEUIINFO_MENU_EDIT_TREE, "GNOMEUIINFO_MENU_EDIT_TREE", NULL},
			{ GNOMEUIINFO_MENU_VIEW_TREE, "GNOMEUIINFO_MENU_VIEW_TREE", NULL},
			{ GNOMEUIINFO_MENU_SETTINGS_TREE, "GNOMEUIINFO_MENU_SETTINGS_TREE", NULL},
			{ GNOMEUIINFO_MENU_FILES_TREE, "GNOMEUIINFO_MENU_FILES_TREE", NULL},
			{ GNOMEUIINFO_MENU_WINDOWS_TREE, "GNOMEUIINFO_MENU_WINDOWS_TREE", NULL},
			{ GNOMEUIINFO_MENU_HELP_TREE, "GNOMEUIINFO_MENU_HELP_TREE", NULL},
			{ GNOMEUIINFO_MENU_GAME_TREE, "GNOMEUIINFO_MENU_GAME_TREE", NULL},
			{ 0, NULL, NULL }
		};
		etype = g_enum_register_static ("GladeGtkGnomeUIInfo", values);
	}
	return etype;
}

GParamSpec *
glade_gtk_gnome_ui_info_spec (void)
{
	return g_param_spec_enum ("gnomeuiinfo", _("GnomeUIInfo"), 
				  _("Choose the GnomeUIInfo stock item"),
				  glade_gtk_gnome_ui_info_get_type (),
				  0, G_PARAM_READWRITE);
}

/* Fake GtkImage::icon-size since its an int pspec in the image */
GParamSpec *
gladegtk_icon_size_spec (void)
{
	return g_param_spec_enum ("icon-size", _("Icon Size"), 
				  _("Symbolic size to use for stock icon, icon set or named icon"),
				  GTK_TYPE_ICON_SIZE,
				  GTK_ICON_SIZE_BUTTON, 
				  G_PARAM_READWRITE);
}

/* This function does absolutely nothing
 * (and is for use in overriding post_create functions).
 */
void
empty (GObject *container, GladeCreateReason reason)
{
}

/* This function is used to stop default handlers  */
static void
glade_gtk_stop_emission_POINTER (gpointer instance, gpointer dummy, gpointer data)
{
	g_signal_stop_emission (instance, GPOINTER_TO_UINT (data) , 0);
}



/* Initialize needed pspec types from here */
void
glade_gtk_init (const gchar *name)
{

}

/* ----------------------------- GtkWidget ------------------------------ */

#define GLADE_TAG_A11Y_A11Y         "accessibility"
#define GLADE_TAG_A11Y_ACTION_NAME  "action_name" /* We should make -/_ synonymous */
#define GLADE_TAG_A11Y_DESC         "description"
#define GLADE_TAG_A11Y_TARGET       "target"
#define GLADE_TAG_A11Y_TYPE         "type"

#define GLADE_TAG_A11Y_INTERNAL_NAME         "accessible"

#define GLADE_TAG_ATTRIBUTES        "attributes"
#define GLADE_TAG_ATTRIBUTE         "attribute"

#define GLADE_TAG_A11Y_LIBGLADE_RELATION     "atkrelation"
#define GLADE_TAG_A11Y_LIBGLADE_ACTION       "atkaction"
#define GLADE_TAG_A11Y_LIBGLADE_PROPERTY     "atkproperty"
#define GLADE_TAG_A11Y_GTKBUILDER_RELATION   "relation"
#define GLADE_TAG_A11Y_GTKBUILDER_ACTION     "action"
#define GLADE_TAG_A11Y_GTKBUILDER_PROPERTY   "property"

#define GLADE_TAG_A11Y_PROPERTY(type) \
	((type == GLADE_PROJECT_FORMAT_LIBGLADE) ? \
	 GLADE_TAG_A11Y_LIBGLADE_PROPERTY : GLADE_TAG_A11Y_GTKBUILDER_PROPERTY)

#define GLADE_TAG_A11Y_ACTION(type) \
	((type == GLADE_PROJECT_FORMAT_LIBGLADE) ? \
	 GLADE_TAG_A11Y_LIBGLADE_ACTION : GLADE_TAG_A11Y_GTKBUILDER_ACTION)

#define GLADE_TAG_A11Y_RELATION(type) \
	((type == GLADE_PROJECT_FORMAT_LIBGLADE) ? \
	 GLADE_TAG_A11Y_LIBGLADE_RELATION : GLADE_TAG_A11Y_GTKBUILDER_RELATION)


static const gchar *atk_relations_list[] = {
	"controlled-by",
	"controller-for",
	"labelled-by",
	"label-for",
	"member-of",
	"node-child-of",
	"flows-to",
	"flows-from",
	"subwindow-of",
	"embeds",
	"embedded-by",
	"popup-for",
	"parent-window-of",
	"described-by",
	"description-for",
	NULL
};

static void
glade_gtk_read_accels (GladeWidget  *widget,
		       GladeXmlNode *node,
		       gboolean      require_signal)
{
	GladeProperty  *property;
	GladeXmlNode   *prop;
	GladeAccelInfo *ainfo;
	GValue         *value = NULL;
	GList          *accels = NULL;

	for (prop = glade_xml_node_get_children (node); 
	     prop; prop = glade_xml_node_next (prop))
	{
		if (!glade_xml_node_verify_silent (prop, GLADE_TAG_ACCEL))
			continue;

		if ((ainfo = glade_accel_read (prop, require_signal)) != NULL)
			accels = g_list_prepend (accels, ainfo);
	}

	if (accels)
	{
		value = g_new0 (GValue, 1);
		g_value_init (value, GLADE_TYPE_ACCEL_GLIST);
		g_value_take_boxed (value, accels);

		property = glade_widget_get_property (widget, "accelerator");
		glade_property_set_value (property, value);

		g_value_unset (value);
		g_free (value);
	}
}

static void
glade_gtk_parse_atk_props (GladeWidget  *widget,
			   GladeXmlNode *node)
{
	GladeProjectFormat fmt;
	GladeXmlNode  *prop;
	GladeProperty *property;
	GValue        *gvalue;
	gchar         *value, *name, *id, *comment;
	gint           translatable, has_context;
	gboolean       is_action;

	fmt = glade_project_get_format (widget->project);

	for (prop = glade_xml_node_get_children (node); 
	     prop; prop = glade_xml_node_next (prop))
	{
		if (glade_xml_node_verify_silent (prop, GLADE_TAG_A11Y_PROPERTY (fmt)))
			is_action = FALSE;
		else if (glade_xml_node_verify_silent (prop, GLADE_TAG_A11Y_ACTION (fmt)))
			is_action = TRUE;
		else 
			continue;

		if (!is_action && 
		    !(name = glade_xml_get_property_string_required
		      (prop, GLADE_XML_TAG_NAME, NULL)))
			continue;
		else if (is_action && 
			 !(name = glade_xml_get_property_string_required
			   (prop, GLADE_TAG_A11Y_ACTION_NAME, NULL)))
			continue;


		/* Make sure we are working with dashes and
		 * not underscores ... 
		 */
		id = glade_util_read_prop_name (name);
		g_free (name);

		/* We are namespacing the action properties internally
		 * just incase they clash (all property names must be
		 * unique...)
		 */
		if (is_action) 
		{
			name = g_strdup_printf ("atk-%s", id);
			g_free (id);
			id = name;
		}

		if ((property = glade_widget_get_property (widget, id)) != NULL)
		{
			/* Complex statement just getting the value here... */
			if ((!is_action && 
			     !(value = glade_xml_get_content (prop))) ||
			    (is_action &&
			     !(value = glade_xml_get_property_string_required
			       (prop, GLADE_TAG_A11Y_DESC, NULL))))
			{
				/* XXX should be a glade_xml_get_content_required()... */
				g_free (id);
				continue;
			}

			/* Set the parsed value on the property ... */
			gvalue = glade_property_class_make_gvalue_from_string
				(property->klass, value, widget->project, widget);
			glade_property_set_value (property, gvalue);
			g_value_unset (gvalue);
			g_free (gvalue);

			/* Deal with i18n... */
			translatable = glade_xml_get_property_boolean
				(prop, GLADE_TAG_TRANSLATABLE, FALSE);
			has_context = glade_xml_get_property_boolean
				(prop, GLADE_TAG_HAS_CONTEXT, FALSE);
			comment = glade_xml_get_property_string
				(prop, GLADE_TAG_COMMENT);

			glade_property_i18n_set_translatable
				(property, translatable);
			glade_property_i18n_set_has_context
				(property, has_context);
			glade_property_i18n_set_comment
				(property, comment);
			
			g_free (comment);
			g_free (value);
		}

		g_free (id);
	}
}

static void
glade_gtk_parse_atk_props_gtkbuilder (GladeWidget  *widget, 
				      GladeXmlNode *node)
{
	GladeXmlNode *child, *object_node;
	gchar        *internal;

	/* Search for internal "accessible" child and redirect parse from there */
	for (child = glade_xml_node_get_children (node); 
	     child; child = glade_xml_node_next (child))
	{
		if (glade_xml_node_verify_silent (child, GLADE_XML_TAG_CHILD))
		{
			if ((internal =
			     glade_xml_get_property_string (child, GLADE_XML_TAG_INTERNAL_CHILD)))
			{
				if (!strcmp (internal, GLADE_TAG_A11Y_INTERNAL_NAME) &&
				    (object_node = 
				     glade_xml_search_child_required 
				     (child, GLADE_XML_TAG_BUILDER_WIDGET)))
					glade_gtk_parse_atk_props (widget, object_node);

				g_free (internal);
			}
		}
	}
}

static void
glade_gtk_parse_atk_relation (GladeProperty *property,
			      GladeXmlNode  *node)
{
	GladeProjectFormat fmt;
	GladeXmlNode *prop;
	gchar *type, *target, *id, *tmp;
	gchar *string = NULL;

	fmt = glade_project_get_format (property->widget->project);

	for (prop = glade_xml_node_get_children (node); 
	     prop; prop = glade_xml_node_next (prop))
	{
		if (!glade_xml_node_verify_silent (prop, GLADE_TAG_A11Y_RELATION (fmt)))
			continue;

		if (!(type = 
		     glade_xml_get_property_string_required
		     (prop, GLADE_TAG_A11Y_TYPE, NULL)))
			continue;

		if (!(target = 
		      glade_xml_get_property_string_required
		      (prop, GLADE_TAG_A11Y_TARGET, NULL)))
		{
			g_free (type);
			continue;
		}

		id = glade_util_read_prop_name (type);

		if (!strcmp (id, property->klass->id))
		{
			if (string == NULL)
				string = g_strdup (target);
			else
			{
				tmp = g_strdup_printf ("%s%s%s", string, 
						       GPC_OBJECT_DELIMITER, target);
				string = (g_free (string), tmp);
			}
			
		}

		g_free (id);
		g_free (type);
		g_free (target);
	}


	/* we must synchronize this directly after loading this project
	 * (i.e. lookup the actual objects after they've been parsed and
	 * are present). this is a feature of object and object list properties
	 * that needs a better api.
	 */
	if (string)
	{
		g_object_set_data_full (G_OBJECT (property), "glade-loaded-object", 
					g_strdup (string), g_free);
	}
}

static void
glade_gtk_widget_read_atk_props (GladeWidget  *widget,
				 GladeXmlNode *node)
{
	GladeXmlNode  *atk_node;
	GladeProperty *property;
	gint           i;

	if (glade_project_get_format (widget->project) == GLADE_PROJECT_FORMAT_GTKBUILDER)
		glade_gtk_parse_atk_props_gtkbuilder (widget, node);

	if ((atk_node = 
	     glade_xml_search_child (node, GLADE_TAG_A11Y_A11Y)) != NULL)
	{
		/* Properties & actions */
		glade_gtk_parse_atk_props (widget, atk_node);

		/* Relations */
		for (i = 0; atk_relations_list[i]; i++)
		{
			if ((property = 
			     glade_widget_get_property (widget, 
							atk_relations_list[i])))
				glade_gtk_parse_atk_relation (property, atk_node);
			else
				g_warning ("Couldnt find atk relation %s",
					   atk_relations_list[i]);
		}
	}
}

void
glade_gtk_widget_read_widget (GladeWidgetAdaptor *adaptor,
			      GladeWidget        *widget,
			      GladeXmlNode       *node)
{
	if (!glade_xml_node_verify 
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and read in all the normal properties.. */
        GWA_GET_CLASS (G_TYPE_OBJECT)->read_widget (adaptor, widget, node);

	/* Read in accelerators */
	glade_gtk_read_accels (widget, node, TRUE);

	/* Read in atk props */
	glade_gtk_widget_read_atk_props (widget, node);

}

static void
glade_gtk_widget_write_atk_property (GladeProperty      *property,
				     GladeXmlContext    *context,
				     GladeXmlNode       *node)
{
	GladeProjectFormat fmt;
	GladeXmlNode  *prop_node;
	gchar         *value;
	
	fmt = glade_project_get_format (property->widget->project);

	glade_property_get (property, &value);
	if (value && value[0])
	{
		prop_node = glade_xml_node_new (context, GLADE_TAG_A11Y_PROPERTY (fmt));
		glade_xml_node_append_child (node, prop_node);

		glade_xml_node_set_property_string (prop_node, 
						    GLADE_TAG_NAME, 
						    property->klass->id);

		glade_xml_set_content (prop_node, value);

		if (property->i18n_translatable)
			glade_xml_node_set_property_string (prop_node, 
							    GLADE_TAG_TRANSLATABLE, 
							    GLADE_XML_TAG_I18N_TRUE);

		if (property->i18n_has_context)
			glade_xml_node_set_property_string (prop_node, 
							    GLADE_TAG_HAS_CONTEXT, 
							    GLADE_XML_TAG_I18N_TRUE);


		if (property->i18n_comment)
			glade_xml_node_set_property_string (prop_node, 
							    GLADE_TAG_COMMENT, 
							    property->i18n_comment);
	}
}

static void
glade_gtk_widget_write_atk_properties_libglade (GladeWidget        *widget,
						GladeXmlContext    *context,
						GladeXmlNode       *node)
{
	GladeProperty *name_prop, *desc_prop;

	name_prop = glade_widget_get_property (widget, "AtkObject::accessible-name");
	desc_prop = glade_widget_get_property (widget, "AtkObject::accessible-description");

	glade_gtk_widget_write_atk_property (name_prop, context, node);
	glade_gtk_widget_write_atk_property (desc_prop, context, node);
}

static void
glade_gtk_widget_write_atk_properties_gtkbuilder (GladeWidget        *widget,
						  GladeXmlContext    *context,
						  GladeXmlNode       *node)
{
	GladeXmlNode  *child_node, *object_node;
	GladeProperty *name_prop, *desc_prop;

	name_prop = glade_widget_get_property (widget, "AtkObject::accessible-name");
	desc_prop = glade_widget_get_property (widget, "AtkObject::accessible-description");

	/* Create internal child here if any of these properties are non-null */
	if (!glade_property_default (name_prop) || 
	    !glade_property_default (desc_prop))
	{
		gchar *atkname = g_strdup_printf ("%s-atkobject", widget->name);

		child_node = glade_xml_node_new (context, GLADE_XML_TAG_CHILD);
		glade_xml_node_append_child (node, child_node);

		glade_xml_node_set_property_string (child_node, 
						    GLADE_XML_TAG_INTERNAL_CHILD, 
						    GLADE_TAG_A11Y_INTERNAL_NAME);

		object_node = glade_xml_node_new (context, GLADE_XML_TAG_BUILDER_WIDGET);
		glade_xml_node_append_child (child_node, object_node);

		glade_xml_node_set_property_string (object_node, 
						    GLADE_XML_TAG_CLASS, 
						    "AtkObject");

		glade_xml_node_set_property_string (object_node, 
						    GLADE_XML_TAG_ID, 
						    atkname);
	
		if (!glade_property_default (name_prop))
			glade_gtk_widget_write_atk_property (name_prop, context, object_node);
		if (!glade_property_default (desc_prop))
			glade_gtk_widget_write_atk_property (desc_prop, context, object_node);

		g_free (atkname);
	}

}

static void
glade_gtk_widget_write_atk_relation (GladeProperty      *property,
				     GladeXmlContext    *context,
				     GladeXmlNode       *node)
{
	GladeProjectFormat fmt;
	GladeXmlNode *prop_node;
	gchar        *value, **split;
	gint          i;
	
	fmt = glade_project_get_format (property->widget->project);

	if ((value = glade_widget_adaptor_string_from_value
	     (GLADE_WIDGET_ADAPTOR (property->klass->handle),
	      property->klass, property->value, fmt)) != NULL)
	{
		if ((split = g_strsplit (value, GPC_OBJECT_DELIMITER, 0)) != NULL)
		{
			for (i = 0; split[i] != NULL; i++)
			{
				prop_node = glade_xml_node_new (context, 
								GLADE_TAG_A11Y_RELATION (fmt));
				glade_xml_node_append_child (node, prop_node);

				glade_xml_node_set_property_string (prop_node, 
								    GLADE_TAG_A11Y_TYPE, 
								    property->klass->id);
				glade_xml_node_set_property_string (prop_node, 
								    GLADE_TAG_A11Y_TARGET, 
								    split[i]);
			}
			g_strfreev (split);
		}
	}
}

static void
glade_gtk_widget_write_atk_relations (GladeWidget        *widget,
				      GladeXmlContext    *context,
				      GladeXmlNode       *node)
{
	GladeProperty *property;
	gint i;

	for (i = 0; atk_relations_list[i]; i++)
	{
		if ((property = 
		     glade_widget_get_property (widget, 
						atk_relations_list[i])))
			glade_gtk_widget_write_atk_relation (property, context, node);
		else
			g_warning ("Couldnt find atk relation %s on widget %s",
				   atk_relations_list[i], widget->name);
	}
}

static void
glade_gtk_widget_write_atk_action (GladeProperty      *property,
				   GladeXmlContext    *context,
				   GladeXmlNode       *node)
{
 	GladeProjectFormat fmt;
	GladeXmlNode *prop_node;
	gchar        *value = NULL;
	
	fmt = glade_project_get_format (property->widget->project);

	glade_property_get (property, &value);

	if (value && value[0])
	{
		prop_node = glade_xml_node_new (context, GLADE_TAG_A11Y_ACTION (fmt));
		glade_xml_node_append_child (node, prop_node);

		glade_xml_node_set_property_string (prop_node, 
						    GLADE_TAG_A11Y_ACTION_NAME, 
						    &property->klass->id[4]);
		glade_xml_node_set_property_string (prop_node, 
						    GLADE_TAG_A11Y_DESC, 
						    value);
	}
}

static void
glade_gtk_widget_write_atk_actions (GladeWidget        *widget,
				    GladeXmlContext    *context,
				    GladeXmlNode       *node)
{
	GladeProperty *property;

	if ((property = glade_widget_get_property (widget, "atk-click")) != NULL)
		glade_gtk_widget_write_atk_action (property, context, node);
	if ((property = glade_widget_get_property (widget, "atk-activate")) != NULL)
		glade_gtk_widget_write_atk_action (property, context, node);
	if ((property = glade_widget_get_property (widget, "atk-press")) != NULL)
		glade_gtk_widget_write_atk_action (property, context, node);
	if ((property = glade_widget_get_property (widget, "atk-release")) != NULL)
		glade_gtk_widget_write_atk_action (property, context, node);
}

static void
glade_gtk_widget_write_atk_props (GladeWidget        *widget,
				  GladeXmlContext    *context,
				  GladeXmlNode       *node)
{
 	GladeProjectFormat  fmt;
	GladeXmlNode       *atk_node;

	fmt = glade_project_get_format (widget->project);

	atk_node = glade_xml_node_new (context, GLADE_TAG_A11Y_A11Y);

	if (fmt == GLADE_PROJECT_FORMAT_LIBGLADE)
		glade_gtk_widget_write_atk_properties_libglade (widget, context, atk_node);

	glade_gtk_widget_write_atk_relations (widget, context, atk_node);
	glade_gtk_widget_write_atk_actions (widget, context, atk_node);

	if (!glade_xml_node_get_children (atk_node))
		glade_xml_node_delete (atk_node);
	else
		glade_xml_node_append_child (node, atk_node);

	if (fmt == GLADE_PROJECT_FORMAT_GTKBUILDER)
		glade_gtk_widget_write_atk_properties_gtkbuilder (widget, context, node);
}

static void
glade_gtk_write_accels (GladeWidget        *widget,
			GladeXmlContext    *context,
			GladeXmlNode       *node,
			gboolean            write_signal)
{
	GladeXmlNode  *accel_node;
	GladeProperty *property;
	GList         *list;

	/* Some child widgets may have disabled the property */
	if (!(property = glade_widget_get_property (widget, "accelerator")))
		return;

	for (list = g_value_get_boxed (property->value); 
	     list; list = list->next)
	{
		GladeAccelInfo *accel = list->data;

		accel_node = glade_accel_write (accel, context, write_signal);
		glade_xml_node_append_child (node, accel_node);
	}
}

void
glade_gtk_widget_write_widget (GladeWidgetAdaptor *adaptor,
			       GladeWidget        *widget,
			       GladeXmlContext    *context,
			       GladeXmlNode       *node)
{
 	GladeProjectFormat  fmt;
	GObject *obj;

	fmt = glade_project_get_format (widget->project);

	if (!glade_xml_node_verify (node, GLADE_XML_TAG_WIDGET (fmt)))
		return;

	/* Make sure use-action-appearance and related-action properties are
	 * ordered in a sane way */
	if (fmt == GLADE_PROJECT_FORMAT_GTKBUILDER &&
	    (obj = glade_widget_get_object (widget)) &&
	    GTK_IS_ACTIVATABLE (obj) &&
	    gtk_activatable_get_related_action (GTK_ACTIVATABLE (obj)))
	{
		GladeProperty *prop =
			glade_widget_get_property (widget, "use-action-appearance");
		if (prop)
			glade_property_write (prop, context, node);

		prop = glade_widget_get_property (widget, "related-action");
		if (prop)
			glade_property_write (prop, context, node);
	}

	/* First chain up and read in all the normal properties.. */
        GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);

	/* in Libglade the order must be Properties, Atk, Signals, Accels. 
	 * in builder it doesnt matter so long as signals are after properties
	 * and before objects.
	 */
	if (fmt == GLADE_PROJECT_FORMAT_LIBGLADE)
	{
 		glade_gtk_widget_write_atk_props (widget, context, node);
		glade_widget_write_signals (widget, context, node);
		glade_gtk_write_accels (widget, context, node, TRUE);
	} else {
		/* The core takes care of signals in GtkBuilder format */
		glade_gtk_write_accels (widget, context, node, TRUE);		
 		glade_gtk_widget_write_atk_props (widget, context, node);
	}
}


GladeEditorProperty *
glade_gtk_widget_create_eprop (GladeWidgetAdaptor *adaptor,
			       GladePropertyClass *klass,
			       gboolean            use_command)
{
	GladeEditorProperty *eprop;

	/* chain up.. */
	if (klass->pspec->value_type == GLADE_TYPE_ACCEL_GLIST)
		eprop = g_object_new (GLADE_TYPE_EPROP_ACCEL,
				      "property-class", klass, 
				      "use-command", use_command,
				      NULL);
	else
		eprop = GWA_GET_CLASS 
			(G_TYPE_OBJECT)->create_eprop (adaptor, 
						       klass, 
						       use_command);
	return eprop;
}

gchar *
glade_gtk_widget_string_from_value (GladeWidgetAdaptor *adaptor,
				    GladePropertyClass *klass,
				    const GValue       *value,
				    GladeProjectFormat  fmt)
{
	if (klass->pspec->value_type == GLADE_TYPE_ACCEL_GLIST)
		return glade_accels_make_string (g_value_get_boxed (value));
	else
		return GWA_GET_CLASS 
			(G_TYPE_OBJECT)->string_from_value (adaptor, 
							    klass, 
							    value,
							    fmt);
}

static void
widget_parent_changed (GtkWidget          *widget,
		       GParamSpec         *pspec,
		       GladeWidgetAdaptor *adaptor)
{
	GladeWidget *gwidget = glade_widget_get_from_gobject (widget);

	/* this could get called for a stale instance of an object
	 * being rebuilt for a contruct-only property. */
	if (!gwidget)
		return;

	if (gwidget->parent && gwidget->parent->internal == NULL)
		glade_widget_set_action_sensitive (gwidget, "remove_parent", TRUE);
	else
		glade_widget_set_action_sensitive (gwidget, "remove_parent", FALSE);
}


static void
widget_format_changed (GladeProject *project, 
		       GParamSpec   *pspec,
		       GladeWidget  *gwidget)
{
	if (glade_project_get_format (project) == GLADE_PROJECT_FORMAT_LIBGLADE)
		glade_widget_set_action_sensitive (gwidget, "sizegroup_add", FALSE);
	else
		glade_widget_set_action_sensitive (gwidget, "sizegroup_add", TRUE);
}

static void
widget_project_changed (GladeWidget *gwidget, 
			GParamSpec  *pspec,
			gpointer     userdata)
{
	GladeProject
		*project = glade_widget_get_project (gwidget),
		*old_project = g_object_get_data (G_OBJECT (gwidget), "widget-project-ptr");
	
	if (old_project)
		g_signal_handlers_disconnect_by_func (G_OBJECT (old_project), 
						      G_CALLBACK (widget_format_changed),
						      gwidget);

	if (project)
		g_signal_connect (G_OBJECT (project), "notify::format", 
				  G_CALLBACK (widget_format_changed), gwidget);

	g_object_set_data (G_OBJECT (gwidget), "widget-project-ptr", project);
}

void
glade_gtk_widget_deep_post_create (GladeWidgetAdaptor *adaptor,
				   GObject            *widget, 
				   GladeCreateReason   reason)
{
	GladeWidget *gwidget = glade_widget_get_from_gobject (widget);

	/* Work around bug 472555 by resetting the default event mask,
	 * this way only user edits will be saved to the glade file. */
	if (reason == GLADE_CREATE_USER)
		glade_widget_property_reset (gwidget, "events");
	
	glade_widget_set_action_sensitive (gwidget, "remove_parent", FALSE);

	if (GTK_IS_WINDOW (widget) || gwidget->internal)
		glade_widget_set_action_sensitive (gwidget, "add_parent", FALSE);


	/* Watch parents/projects and set actions sensitive/insensitive */
	if (gwidget->internal == NULL)
		g_signal_connect (G_OBJECT (widget), "notify::parent",
				  G_CALLBACK (widget_parent_changed), adaptor);
	
	g_signal_connect (G_OBJECT (gwidget), "notify::project",
			  G_CALLBACK (widget_project_changed), NULL);

	widget_project_changed (gwidget, NULL, NULL);
}


void
glade_gtk_widget_set_property (GladeWidgetAdaptor *adaptor,
			       GObject            *object, 
			       const gchar        *id,
			       const GValue       *value)
{
        GWA_GET_CLASS (G_TYPE_OBJECT)->set_property (adaptor, object, id, value);
}

void
glade_gtk_widget_get_property (GladeWidgetAdaptor *adaptor,
			       GObject            *object, 
			       const gchar        *id,
			       GValue             *value)
{
        GWA_GET_CLASS (G_TYPE_OBJECT)->get_property (adaptor, object, id, value);
}

static GList *
create_command_property_list (GladeWidget *gnew, GList *saved_props)
{
	GList *l, *command_properties = NULL;
	
	for (l = saved_props; l; l = l->next)
	{
		GladeProperty *property = l->data;
		GladeProperty *orig_prop = glade_widget_get_pack_property (gnew, property->klass->id);
		GCSetPropData *pdata = g_new0 (GCSetPropData, 1);

		pdata->property  = orig_prop;
		pdata->old_value = g_new0 (GValue, 1);
		pdata->new_value = g_new0 (GValue, 1);

		glade_property_get_value (orig_prop, pdata->old_value);
		glade_property_get_value (property, pdata->new_value);

		command_properties = g_list_prepend (command_properties, pdata);
	}
	return g_list_reverse (command_properties);
}

void
glade_gtk_widget_action_activate (GladeWidgetAdaptor *adaptor,
				  GObject *object,
				  const gchar *action_path)
{
	GladeWidget *gwidget = glade_widget_get_from_gobject (object), *gparent;
	GList this_widget = { 0, }, that_widget = { 0,};
	GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (object));
	GladeProject *project;

	if (parent)
		gparent = glade_widget_get_from_gobject (parent);
	else
		gparent = NULL;

	project = glade_widget_get_project (gwidget);

	if (strcmp (action_path, "edit_separate") == 0)
	{
		GtkWidget *dialog = glade_editor_dialog_for_widget (gwidget);
		gtk_widget_show_all (dialog);
	}
	else if (strcmp (action_path, "remove_parent") == 0)
	{
		GladeWidget *new_gparent;
		GladeProperty *property;

		g_return_if_fail (gparent);

		property = glade_widget_get_parentless_widget_ref (gparent);
		new_gparent = glade_widget_get_parent (gparent);

		glade_command_push_group (_("Removing parent of %s"), glade_widget_get_name (gwidget));

		/* Remove "this" widget, If the parent we're removing is a parentless 
		 * widget reference, the reference will be implicitly broken by the 'cut' command */
		this_widget.data = gwidget;
		glade_command_delete (&this_widget);

		/* Delete the parent */
		that_widget.data = gparent;
		glade_command_delete (&that_widget);

		/* Add "this" widget to the new parent, if there is no new parent this will re-add
		 * the widget to the project at the toplevel without a parent
		 */
		glade_command_add (&this_widget, new_gparent, NULL, project, FALSE);

		/* If the parent had a parentless widget reference, undoably add the child
		 * as the new parentless widget reference here */
		if (property)
			glade_command_set_property (property, glade_widget_get_object (gwidget));

		glade_command_pop_group ();
	}
	else if (strncmp (action_path, "add_parent/", 11) == 0)
	{
		GType new_type = 0;
		GladeProperty *property;

      if (strcmp (action_path + 11, "alignment") == 0)
        new_type = GTK_TYPE_ALIGNMENT;
      else if (strcmp (action_path + 11, "viewport") == 0)
        new_type = GTK_TYPE_VIEWPORT;
      else if (strcmp (action_path + 11, "eventbox") == 0)
        new_type = GTK_TYPE_EVENT_BOX;
      else if (strcmp (action_path + 11, "frame") == 0)
        new_type = GTK_TYPE_FRAME;
      else if (strcmp (action_path + 11, "aspect_frame") == 0)
        new_type = GTK_TYPE_ASPECT_FRAME;
      else if (strcmp (action_path + 11, "scrolled_window") == 0)
        new_type = GTK_TYPE_SCROLLED_WINDOW;
      else if (strcmp (action_path + 11, "expander") == 0)
        new_type = GTK_TYPE_EXPANDER;
      else if (strcmp (action_path + 11, "table") == 0)
        new_type = GTK_TYPE_TABLE;
      else if (strcmp (action_path + 11, "hbox") == 0)
        new_type = GTK_TYPE_HBOX;
      else if (strcmp (action_path + 11, "vbox") == 0)
        new_type = GTK_TYPE_VBOX;
      else if (strcmp (action_path + 11, "hpaned") == 0)
        new_type = GTK_TYPE_HPANED;
      else if (strcmp (action_path + 11, "vpaned") == 0)
        new_type = GTK_TYPE_VPANED;

      if (new_type)
        {
          GladeWidgetAdaptor *adaptor =
              glade_widget_adaptor_get_by_type (new_type);
          GList *saved_props, *prop_cmds;
	  GladeWidget *gnew_parent;

          /* Dont add non-scrollable widgets to scrolled windows... */
          if (gparent &&
              glade_util_check_and_warn_scrollable (gparent, adaptor,
                                                    glade_app_get_window ()))
            return;

          glade_command_push_group (_("Adding parent %s for %s"),
                                    adaptor->title, 
				    glade_widget_get_name (gwidget));

          /* Record packing properties */
          saved_props =
	    glade_widget_dup_properties (gwidget, gwidget->packing_properties,
					 FALSE, FALSE, FALSE);


	  property = glade_widget_get_parentless_widget_ref (gwidget);

	  /* Remove "this" widget, If the parent we're removing is a parentless 
	   * widget reference, the reference will be implicitly broken by the 'cut' command */
          this_widget.data = gwidget;
          glade_command_delete (&this_widget);

          /* Create new widget and put it where the placeholder was */
          if ((gnew_parent =
               glade_command_create (adaptor, gparent, NULL, project)) != NULL)
            {
	      /* Now we created the new parent, if gwidget had a parentless widget reference...
	       * set that reference to the new parent instead */
	      if (property)
		glade_command_set_property (property, glade_widget_get_object (gnew_parent));

              /* Remove the alignment that we added in the frame's post_create... */
              if (new_type == GTK_TYPE_FRAME)
                {
                  GObject *frame = glade_widget_get_object (gnew_parent);
                  GladeWidget *galign =
                      glade_widget_get_from_gobject (gtk_bin_get_child (GTK_BIN (frame)));
                  GList to_delete = { 0, };

                  to_delete.data = galign;
                  glade_command_delete (&to_delete);
                }

              /* Create heavy-duty glade-command properties stuff */
              prop_cmds =
                  create_command_property_list (gnew_parent, saved_props);
              g_list_foreach (saved_props, (GFunc) g_object_unref, NULL);
              g_list_free (saved_props);

              /* Apply the properties in an undoable way */
              if (prop_cmds)
                glade_command_set_properties_list 
		  (glade_widget_get_project (gparent), prop_cmds);

              /* Add "this" widget to the new parent */
              glade_command_add (&this_widget, gnew_parent, NULL, project, FALSE);
            }
          else
	    {
	      /* Create parent was cancelled, paste back to parent */
	      glade_command_add (&this_widget, gparent, NULL, project, FALSE);

	      /* Restore any parentless widget reference if there was one */
	      if (property)
		glade_command_set_property (property, glade_widget_get_object (gwidget));
	    }

          glade_command_pop_group ();
        }
    }
  else if (strcmp (action_path, "sizegroup_add") == 0)
    {
      /* Ignore dummy */
    }
  else
    GWA_GET_CLASS (G_TYPE_OBJECT)->action_activate (adaptor,
                                                    object, action_path);
}

static GList *
list_sizegroups (GladeWidget *gwidget)
{
	GladeProject *project = glade_widget_get_project (gwidget);
	GList *groups = NULL;
	const GList *list;

	for (list = glade_project_get_objects (project); list; list = list->next)
	{
		GladeWidget *iter = glade_widget_get_from_gobject (list->data);
		if (GTK_IS_SIZE_GROUP (iter->object))
			groups = g_list_prepend (groups, iter);
	}
	return g_list_reverse (groups);
}

static void
glade_gtk_widget_add2group_cb (GtkMenuItem *item, GladeWidget *gwidget)
{
	GladeWidget *group = g_object_get_data (G_OBJECT (item), "glade-group-widget");
	GladeWidgetAdaptor *adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_SIZE_GROUP);
	GList *widget_list = NULL, *new_list;
	GladeProperty *property;

	if (group) 
		glade_command_push_group (_("Adding %s to Size Group %s"), gwidget->name, group->name);
	else
		glade_command_push_group (_("Adding %s to a new Size Group"), gwidget->name);

	if (!group)
		/* Cant cancel a size group */
		group = glade_command_create (adaptor, NULL, NULL, glade_widget_get_project (gwidget));
 
	property = glade_widget_get_property (group, "widgets");
	glade_property_get (property, &widget_list);
	new_list = g_list_copy (widget_list);
	if (!g_list_find (widget_list, gwidget->object))
		new_list = g_list_append (new_list, gwidget->object);
	glade_command_set_property (property, new_list);

	g_list_free (new_list);
	
	glade_command_pop_group ();
}


GtkWidget *
glade_gtk_widget_action_submenu (GladeWidgetAdaptor *adaptor,
				 GObject *object,
				 const gchar *action_path)
{
	GladeWidget *gwidget = glade_widget_get_from_gobject (object);
	GList *groups, *list;

	if (strcmp (action_path, "sizegroup_add") == 0)
	{
		GtkWidget *menu = gtk_menu_new ();
		GtkWidget *separator, *item;
		GladeWidget *group;

		if ((groups = list_sizegroups (gwidget)) != NULL)
		{
			for (list = groups; list; list = list->next)
			{
				group = list->data;
				item = gtk_menu_item_new_with_label (group->name);

				g_object_set_data (G_OBJECT (item), "glade-group-widget", group);
				g_signal_connect (G_OBJECT (item), "activate",
						  G_CALLBACK (glade_gtk_widget_add2group_cb), gwidget);

				gtk_widget_show (item);
				gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
			}
			g_list_free (groups);

			separator = gtk_menu_item_new ();		
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), separator);
			gtk_widget_show (separator);
		}
		
		/* Add trailing new... item */
		item = gtk_menu_item_new_with_label (_("New Size Group"));
		g_signal_connect (G_OBJECT (item), "activate",
				  G_CALLBACK (glade_gtk_widget_add2group_cb), gwidget);
		
		gtk_widget_show (item);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

		return menu;
	}
	else if (GWA_GET_CLASS (G_TYPE_OBJECT)->action_submenu)
		return GWA_GET_CLASS (G_TYPE_OBJECT)->action_submenu (adaptor,
								      object,
								      action_path);

	return NULL;
}



/* ----------------------------- GtkContainer ------------------------------ */
void
glade_gtk_container_post_create (GladeWidgetAdaptor *adaptor,
				 GObject            *container, 
				 GladeCreateReason   reason)
{
	GList *children;
	g_return_if_fail (GTK_IS_CONTAINER (container));

	if (reason == GLADE_CREATE_USER)
	{
		if ((children = gtk_container_get_children (GTK_CONTAINER (container))) == NULL)
			gtk_container_add (GTK_CONTAINER (container), glade_placeholder_new ());
		else
			g_list_free (children);
	}
}

void
glade_gtk_container_replace_child (GladeWidgetAdaptor *adaptor,
				   GtkWidget          *container,
				   GtkWidget          *current,
				   GtkWidget          *new_widget)
{
	GParamSpec **param_spec;
	GladePropertyClass *pclass;
	GValue *value;
	guint nproperties;
	guint i;

	if (gtk_widget_get_parent (current) != container)
		return;

	param_spec = gtk_container_class_list_child_properties
		(G_OBJECT_GET_CLASS (container), &nproperties);
	value = g_malloc0 (sizeof(GValue) * nproperties);

	for (i = 0; i < nproperties; i++) 
	{
		g_value_init (&value[i], param_spec[i]->value_type);
		gtk_container_child_get_property
			(GTK_CONTAINER (container), current, param_spec[i]->name, &value[i]);
	}

	gtk_container_remove (GTK_CONTAINER (container), current);
	gtk_container_add (GTK_CONTAINER (container), new_widget);

	for (i = 0; i < nproperties; i++) 
	{
		/* If the added widget is a placeholder then we
		 * want to keep all the "tranfer-on-paste" properties
		 * as default so that it looks fresh (transfer-on-paste
		 * properties dont effect the position/slot inside a 
		 * contianer)
		 */
		if (GLADE_IS_PLACEHOLDER (new_widget))
		{
			pclass = glade_widget_adaptor_get_pack_property_class
				(adaptor, param_spec[i]->name);

			if (pclass && pclass->transfer_on_paste)
				continue;
		}

		gtk_container_child_set_property
			(GTK_CONTAINER (container), new_widget, param_spec[i]->name, &value[i]);
	}
	
	for (i = 0; i < nproperties; i++)
		g_value_unset (&value[i]);

	g_free (param_spec);
	g_free (value);
}

void
glade_gtk_container_add_child (GladeWidgetAdaptor *adaptor,
			       GtkWidget          *container,
			       GtkWidget          *child)
{
	GtkWidget *container_child = NULL;

	if (GTK_IS_BIN (container)) 
		container_child = gtk_bin_get_child (GTK_BIN (container));

	/* Get a placeholder out of the way before adding the child if its a GtkBin
	 */
	if (GTK_IS_BIN (container) && container_child != NULL &&
	    GLADE_IS_PLACEHOLDER (container_child))
		gtk_container_remove (GTK_CONTAINER (container), container_child);

	gtk_container_add (GTK_CONTAINER (container), child);
}

void
glade_gtk_container_remove_child (GladeWidgetAdaptor *adaptor,
				  GtkWidget          *container,
				  GtkWidget          *child)
{
	GList *children;
	gtk_container_remove (GTK_CONTAINER (container), child);

	/* If this is the last one, add a placeholder by default.
	 */
	if ((children = gtk_container_get_children (GTK_CONTAINER (container))) == NULL)
	{
		gtk_container_add (GTK_CONTAINER (container), glade_placeholder_new ());
	}
	else
		g_list_free (children);
}

void
glade_gtk_container_set_child_property (GladeWidgetAdaptor *adaptor,
					GObject            *container,
					GObject            *child,
					const gchar        *property_name,
					const GValue       *value)
{
	if (gtk_widget_get_parent (GTK_WIDGET (child)) == GTK_WIDGET (container))
		gtk_container_child_set_property (GTK_CONTAINER (container),
						  GTK_WIDGET (child),
						  property_name, value);
}

void
glade_gtk_container_get_child_property (GladeWidgetAdaptor *adaptor,
					GObject            *container,
					GObject            *child,
					const gchar        *property_name,
					GValue             *value)
{
	if (gtk_widget_get_parent (GTK_WIDGET (child)) == GTK_WIDGET (container))
		gtk_container_child_get_property (GTK_CONTAINER (container),
						  GTK_WIDGET (child),
						  property_name, value);
}

GList *
glade_gtk_container_get_children (GladeWidgetAdaptor  *adaptor,
				  GtkContainer        *container)
{
	return glade_util_container_get_all_children (container);
}

GladeEditable *
glade_gtk_container_create_editable (GladeWidgetAdaptor  *adaptor,
				       GladeEditorPageType  type)
{
	return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);;
}

/* ----------------------------- GtkBox ------------------------------ */
typedef struct {
	GtkWidget *widget;
	gint       position;
} GladeGtkBoxChild;

static GList *glade_gtk_box_original_positions = NULL;

static gboolean
glade_gtk_box_configure_child (GladeFixed   *fixed,
			       GladeWidget  *child,
			       GdkRectangle *rect,
			       GtkWidget    *box)
{
	GList         *list, *children;
	GtkWidget     *bchild;
	GtkAllocation  allocation, bchild_allocation;
	gint           point, trans_point, span,
		iter_span, position, old_position,
		offset, orig_offset;
	gboolean       found = FALSE;

	gtk_widget_get_allocation (GTK_WIDGET (child->object), &allocation);
	if (GTK_IS_HBOX (box) || GTK_IS_HBUTTON_BOX (box))
	{
		point       = fixed->mouse_x;
		span        = allocation.width;
		offset      = rect->x;
		orig_offset = fixed->child_x_origin;
	}
	else
	{
		point       = fixed->mouse_y;
		span        = allocation.height;
		offset      = rect->y;
		orig_offset = fixed->child_y_origin;
	}

	glade_widget_pack_property_get
		(child, "position", &old_position);

	children = gtk_container_get_children (GTK_CONTAINER (box));

	for (list = children; list; list = list->next)
	{
		bchild = list->data;

		if (bchild == GTK_WIDGET (child->object))
			continue;

		/* Find the widget in the box where the center of
		 * this rectangle fits... and set the position to that
		 * position.
		 */

		gtk_widget_get_allocation (GTK_WIDGET (bchild), &bchild_allocation);
		if (GTK_IS_HBOX (box) || GTK_IS_HBUTTON_BOX (box))
		{
			gtk_widget_translate_coordinates 
				(GTK_WIDGET (box), bchild,
				 point, 0, &trans_point, NULL);

			iter_span   = bchild_allocation.width;
		}
		else
		{
			gtk_widget_translate_coordinates 
				(GTK_WIDGET (box), bchild,
				 0, point, NULL, &trans_point);
			iter_span = bchild_allocation.height;
		}

#if 0
		gtk_container_child_get (GTK_CONTAINER (box),
					 bchild,
					 "position", &position, NULL);
		g_print ("widget: %p pos %d, point %d, trans_point %d, iter_span %d\n", 
			 bchild, position, point, trans_point, iter_span);
#endif

		if (iter_span <= span)
		{
			found = trans_point >= 0 && trans_point < iter_span;
		}
		else
		{
			if (offset > orig_offset)
				found = trans_point >= iter_span - span && 
					trans_point < iter_span;
			else if (offset < orig_offset)
				found = trans_point >= 0 &&
					trans_point < span;
		}

		if (found)
		{
			gtk_container_child_get (GTK_CONTAINER (box),
						 bchild,
						 "position", &position, NULL);

#if 0
			g_print ("setting position of %s from %d to %d, "
				 "(point %d iter_span %d)\n", 
				 child->name, old_position, position, trans_point, iter_span);
#endif

			glade_widget_pack_property_set
				(child, "position", position);
							
			break;
		}

	}

	g_list_free (children);

	return TRUE;
}

static gboolean
glade_gtk_box_configure_begin (GladeFixed  *fixed,
			       GladeWidget *child,
			       GtkWidget   *box)
{
	GList       *list, *children;
	GtkWidget   *bchild;

	g_assert (glade_gtk_box_original_positions == NULL);

	children = gtk_container_get_children (GTK_CONTAINER (box));

	for (list = children; list; list = list->next)
	{
		GladeGtkBoxChild *gbchild;
		GladeWidget      *gchild;
				
		bchild = list->data;
		if ((gchild = glade_widget_get_from_gobject (bchild)) == NULL)
			continue;

		gbchild         = g_new0 (GladeGtkBoxChild, 1);
		gbchild->widget = bchild;
		glade_widget_pack_property_get (gchild, "position",
						&gbchild->position);

		glade_gtk_box_original_positions = 
			g_list_prepend (glade_gtk_box_original_positions, gbchild);
	}

	g_list_free (children);

	return TRUE;
}

static gboolean
glade_gtk_box_configure_end (GladeFixed  *fixed,
			     GladeWidget *child,
			     GtkWidget   *box)
{
	GList         *list, *l, *children;
	GList         *prop_list = NULL;

	children = gtk_container_get_children (GTK_CONTAINER (box));

	for (list = children; list; list = list->next)
	{
		GtkWidget *bchild = list->data;

		for (l = glade_gtk_box_original_positions; l; l = l->next)
		{
			GladeGtkBoxChild *gbchild = l->data;
			GladeWidget      *gchild = 
				glade_widget_get_from_gobject (gbchild->widget);


			if (bchild == gbchild->widget)
			{
				GCSetPropData *prop_data = g_new0 (GCSetPropData, 1);
				prop_data->property = 
					glade_widget_get_pack_property
					(gchild, "position");

				prop_data->old_value = g_new0 (GValue, 1);
				prop_data->new_value = g_new0 (GValue, 1);

				glade_property_get_value (prop_data->property,
							  prop_data->new_value);

				g_value_init (prop_data->old_value, G_TYPE_INT);
				g_value_set_int (prop_data->old_value, gbchild->position);

				prop_list = g_list_prepend (prop_list, prop_data);
				break;
			}
		}
	}

	g_list_free (children);

	glade_command_push_group (_("Ordering children of %s"), 
				  GLADE_WIDGET (fixed)->name);
	glade_property_push_superuser ();
	if (prop_list)
		glade_command_set_properties_list (GLADE_WIDGET (fixed)->project,
						   prop_list);
	glade_property_pop_superuser ();
	glade_command_pop_group ();

	for (l = glade_gtk_box_original_positions; l; l = l->next)
		g_free (l->data);

	glade_gtk_box_original_positions = 
		(g_list_free (glade_gtk_box_original_positions), NULL);


	return TRUE;
}

void
glade_gtk_box_post_create (GladeWidgetAdaptor *adaptor,
			   GObject            *container, 
			   GladeCreateReason   reason)
{
	GladeWidget    *gwidget =
		glade_widget_get_from_gobject (container);

	/* Implement drag in GtkBox but not resize.
	 */
	g_object_set (gwidget,
		      "can-resize", FALSE,
		      NULL);

	g_signal_connect (G_OBJECT (gwidget), "configure-child",
			  G_CALLBACK (glade_gtk_box_configure_child), container);

	g_signal_connect (G_OBJECT (gwidget), "configure-begin",
			  G_CALLBACK (glade_gtk_box_configure_begin), container);

	g_signal_connect (G_OBJECT (gwidget), "configure-end",
			  G_CALLBACK (glade_gtk_box_configure_end), container);

}

static gint
sort_box_children (GtkWidget *widget_a, GtkWidget *widget_b)
{
	GtkWidget   *box;
	GladeWidget *gwidget_a, *gwidget_b;
	gint         position_a, position_b;

	gwidget_a = glade_widget_get_from_gobject (widget_a);
	gwidget_b = glade_widget_get_from_gobject (widget_b);

	box = gtk_widget_get_parent (widget_a);

	if (gwidget_a)
		glade_widget_pack_property_get
			(gwidget_a, "position", &position_a);
	else
		gtk_container_child_get (GTK_CONTAINER (box),
					 widget_a,
					 "position", &position_a,
					 NULL);

	if (gwidget_b)
		glade_widget_pack_property_get
			(gwidget_b, "position", &position_b);
	else
		gtk_container_child_get (GTK_CONTAINER (box),
					 widget_b,
					 "position", &position_b,
					 NULL);
	return position_a - position_b;
}

GList *
glade_gtk_box_get_children (GladeWidgetAdaptor  *adaptor,
			    GtkContainer        *container)
{
	GList *children = glade_util_container_get_all_children (container);

	return g_list_sort (children, (GCompareFunc)sort_box_children);
}

void
glade_gtk_box_set_child_property (GladeWidgetAdaptor *adaptor,
				  GObject            *container,
				  GObject            *child,
				  const gchar        *property_name,
				  GValue             *value)
{
	GladeWidget *gbox, *gchild, *gchild_iter;
	GList       *children, *list;
	gboolean     is_position;
	gint         old_position, iter_position, new_position;
	static       gboolean recursion = FALSE;
	
	g_return_if_fail (GTK_IS_BOX (container));
	g_return_if_fail (GTK_IS_WIDGET (child));
	g_return_if_fail (property_name != NULL || value != NULL);

	gbox   = glade_widget_get_from_gobject (container);
	gchild = glade_widget_get_from_gobject (child);

	g_return_if_fail (GLADE_IS_WIDGET (gbox));

	if (gtk_widget_get_parent (GTK_WIDGET (child)) != GTK_WIDGET (container))
		return;

	/* Get old position */
	if ((is_position = (strcmp (property_name, "position") == 0)) != FALSE)
	{
		gtk_container_child_get (GTK_CONTAINER (container),
					 GTK_WIDGET (child),
					 property_name, &old_position, NULL);

		
		/* Get the real value */
		new_position = g_value_get_int (value);
	}

	if (is_position && recursion == FALSE)
	{
		children = glade_widget_adaptor_get_children
			(gbox->adaptor, container);

		children = g_list_sort (children, (GCompareFunc)sort_box_children);

		for (list = children; list; list = list->next)
		{
			if ((gchild_iter = 
			     glade_widget_get_from_gobject (list->data)) == NULL)
				continue;

			if (gchild_iter == gchild)
			{
				gtk_box_reorder_child (GTK_BOX (container),
						       GTK_WIDGET (child),
						       new_position);
				continue;
			}

			/* Get the old value from glade */
			glade_widget_pack_property_get
				(gchild_iter, "position", &iter_position);

			/* Search for the child at the old position and update it */
			if (iter_position == new_position &&
			    glade_property_superuser () == FALSE)
			{
				/* Update glade with the real value */
				recursion = TRUE;
				glade_widget_pack_property_set
					(gchild_iter, "position", old_position);
				recursion = FALSE;
				continue;
			}
			else
			{
				gtk_box_reorder_child (GTK_BOX (container),
						       GTK_WIDGET (list->data),
						       iter_position);
			}
		}

		for (list = children; list; list = list->next)
		{
			if ((gchild_iter = 
			     glade_widget_get_from_gobject (list->data)) == NULL)
				continue;

			/* Refresh values yet again */
			glade_widget_pack_property_get
				(gchild_iter, "position", &iter_position);

			gtk_box_reorder_child (GTK_BOX (container),
					       GTK_WIDGET (list->data),
					       iter_position);

		}

		if (children)
			g_list_free (children);
	}

	/* Chain Up */
	if (!is_position)
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->child_set_property (adaptor, 
								  container, 
								  child,
								  property_name, 
								  value);

	gtk_container_check_resize (GTK_CONTAINER (container));

}

void
glade_gtk_box_get_property (GladeWidgetAdaptor *adaptor,
			    GObject            *object, 
			    const gchar        *id,
			    GValue             *value)
{
	if (!strcmp (id, "size"))
	{
		GtkBox *box = GTK_BOX (object);
		GList  *children = gtk_container_get_children (GTK_CONTAINER (box));

		g_value_reset (value);
		g_value_set_int (value, g_list_length (children));
		g_list_free (children);
	}
	else 
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->get_property (adaptor, object, id, value);
}

static gint
glade_gtk_box_get_first_blank (GtkBox *box)
{
	GList       *child, *children;
	GladeWidget *gwidget;
	gint         position;

	children = gtk_container_get_children (GTK_CONTAINER (box));

	for (child = children, position = 0;
	     child && child->data;
	     child = child->next, position++)
	{
		GtkWidget *widget = child->data;

		if ((gwidget = glade_widget_get_from_gobject (widget)) != NULL)
		{
			gint gwidget_position;
			GladeProperty *property =
				glade_widget_get_pack_property (gwidget, "position");
			gwidget_position = g_value_get_int (property->value);

			if (gwidget_position > position)
				break;
		}
	}

	g_list_free (children);

	return position;
}

static void
glade_gtk_box_set_size (GObject *object, const GValue *value)
{
	GtkBox      *box;
	GList       *child, *children;
	guint new_size, old_size, i;

	box = GTK_BOX (object);
	g_return_if_fail (GTK_IS_BOX (box));

	if (glade_util_object_is_loading (object))
		return;

	children = gtk_container_get_children (GTK_CONTAINER (box));

	old_size = g_list_length (children);
	new_size = g_value_get_int (value);
	
	if (old_size == new_size)
	{
		g_list_free (children);
		return;
	}

	/* Ensure placeholders first...
	 */
	for (i = 0; i < new_size; i++)
	{
		if (g_list_length(children) < (i + 1))
		{
			GtkWidget *placeholder = glade_placeholder_new ();
			gint       blank       = glade_gtk_box_get_first_blank (box);

			gtk_container_add (GTK_CONTAINER (box), placeholder);
			gtk_box_reorder_child (box, placeholder, blank);
		}
	}

	/* The box has shrunk. Remove the widgets that are on those slots */
	for (child = g_list_last (children);
	     child && old_size > new_size;
	     child = g_list_last (children), old_size--)
	{
		GtkWidget *child_widget = child->data;

		/* Refuse to remove any widgets that are either GladeWidget objects
		 * or internal to the hierarchic entity (may be a composite widget,
		 * not all internal widgets have GladeWidgets).
		 */
		if (glade_widget_get_from_gobject (child_widget) ||
		    GLADE_IS_PLACEHOLDER (child_widget) == FALSE)
			break;

		g_object_ref (G_OBJECT (child_widget));
		gtk_container_remove (GTK_CONTAINER (box), child_widget);
		gtk_widget_destroy (child_widget);
	}
	g_list_free (children);

}

void
glade_gtk_box_set_property (GladeWidgetAdaptor *adaptor,
			    GObject            *object, 
			    const gchar        *id,
			    const GValue       *value)
{
	if (!strcmp (id, "size"))
		glade_gtk_box_set_size (object, value);
	else 
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id, value);
}

static gboolean
glade_gtk_box_verify_size (GObject *object, const GValue *value)
{
	GtkBox *box = GTK_BOX(object);
	GList  *child, *children;
	gboolean will_orphan = FALSE;
	gint    old_size;
	gint    new_size = g_value_get_int (value);

	children = gtk_container_get_children (GTK_CONTAINER (box));
	old_size = g_list_length (children);

	for (child = g_list_last (children);
	     child && old_size > new_size;
	     child = g_list_previous (child), old_size--)
	{
		GtkWidget *widget = child->data;
		if (glade_widget_get_from_gobject (widget) != NULL)
	       	{
			/* In this case, refuse to shrink */
			will_orphan = TRUE;
			break;
		}
	}
	
	g_list_free (children);

	return will_orphan ? FALSE : new_size >= 0;
}


gboolean
glade_gtk_box_verify_property (GladeWidgetAdaptor *adaptor,
			       GObject            *object, 
			       const gchar        *id,
			       const GValue       *value)
{
	if (!strcmp (id, "size"))
		return glade_gtk_box_verify_size (object, value);
	else if (GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property)
		return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property (adaptor, object, 
									    id, value);

	return TRUE;
}


static void
fix_response_id_on_child (GladeWidget *gbox,
			  GObject     *child,
			  gboolean     add)
{
	GladeWidget *gchild;
	const gchar *internal_name;

	gchild = glade_widget_get_from_gobject (child);

	/* Fix response id property on child buttons */
	if (gchild && GTK_IS_BUTTON (child))
	{
		if (add && (internal_name = glade_widget_get_internal (gbox)) &&
		    !strcmp (internal_name, "action_area"))
		{
			glade_widget_property_set_sensitive (gchild, "response-id", TRUE, NULL);
			glade_widget_property_set_enabled (gchild, "response-id", TRUE);
		}
		else
		{
			glade_widget_property_set_sensitive (gchild, "response-id", FALSE, 
							     RESPID_INSENSITIVE_MSG);
			glade_widget_property_set_enabled (gchild, "response-id", FALSE);

		}
	}
}


void
glade_gtk_box_add_child (GladeWidgetAdaptor *adaptor,
			 GObject            *object,
			 GObject            *child)
{
	GladeWidget  *gbox, *gchild;
	GladeProject *project;
	GList        *children;
	gint          num_children;
	
	g_return_if_fail (GTK_IS_BOX (object));
	g_return_if_fail (GTK_IS_WIDGET (child));
	
	gbox = glade_widget_get_from_gobject (object);
	project = glade_widget_get_project (gbox);

	/*
	  Try to remove the last placeholder if any, this way GtkBox`s size 
	  will not be changed.
	*/
	if (glade_widget_superuser () == FALSE &&
	    !GLADE_IS_PLACEHOLDER (child))
	{
		GList *l, *children;
		GtkBox *box = GTK_BOX (object);

		children = gtk_container_get_children (GTK_CONTAINER (box));
		
		for (l = g_list_last (children); l; l = g_list_previous (l))
		{
			GtkWidget *child_widget = l->data;			
			if (GLADE_IS_PLACEHOLDER (child_widget))
			{
				gtk_container_remove (GTK_CONTAINER (box), child_widget);
				break;
			}
		}
		g_list_free (children);
	}

	gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));
	
	children = gtk_container_get_children (GTK_CONTAINER (object));
	num_children = g_list_length (children);
	g_list_free (children);

	glade_widget_property_set (gbox, "size", num_children);

	gchild = glade_widget_get_from_gobject (child);

	/* The "Remove Slot" operation only makes sence on placeholders,
	 * otherwise its a "Delete" operation on the child widget.
	 */
	if (gchild)
		glade_widget_remove_pack_action (gchild, "remove_slot");
	
	/* Packing props arent around when parenting during a glade_widget_dup() */
	if (gchild && gchild->packing_properties)
		glade_widget_pack_property_set (gchild, "position", num_children - 1);


	fix_response_id_on_child (gbox, child, TRUE);
}

void
glade_gtk_box_remove_child (GladeWidgetAdaptor *adaptor,
			    GObject            *object, 
			    GObject            *child)
{
	GladeWidget *gbox;
	gint size;
	
	g_return_if_fail (GTK_IS_BOX (object));
	g_return_if_fail (GTK_IS_WIDGET (child));
	
	gbox = glade_widget_get_from_gobject (object);
	
	gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));

	if (glade_widget_superuser () == FALSE)
	{
		glade_widget_property_get (gbox, "size", &size);
		glade_widget_property_set (gbox, "size", size);
	}

	fix_response_id_on_child (gbox, child, FALSE);
}


void
glade_gtk_box_replace_child (GladeWidgetAdaptor *adaptor,
			     GObject            *container,
			     GObject            *current,
			     GObject            *new_widget)
{
	GladeWidget  *gchild;
	GladeWidget  *gbox;

	g_object_ref (G_OBJECT (current));

	GWA_GET_CLASS (GTK_TYPE_CONTAINER)->replace_child (adaptor,
							   container,
							   current,
							   new_widget);

	if ((gchild = glade_widget_get_from_gobject (new_widget)) != NULL)
		/* The "Remove Slot" operation only makes sence on placeholders,
		 * otherwise its a "Delete" operation on the child widget.
		 */
		glade_widget_remove_pack_action (gchild, "remove_slot");

	gbox = glade_widget_get_from_gobject (container);
	fix_response_id_on_child (gbox, current, FALSE);
	fix_response_id_on_child (gbox, new_widget, TRUE);

	g_object_unref (G_OBJECT (current));
}


GObject *
glade_gtk_box_get_internal_child (GladeWidgetAdaptor *adaptor,
				  GObject            *object, 
				  const gchar        *name)
{
	GList   *children, *l;
	GObject *child = NULL;	

	g_return_val_if_fail (GTK_IS_BOX (object), NULL);
	
	children = l = gtk_container_get_children (GTK_CONTAINER (object));
	
	while (l)
	{
		GladeWidget *gw = glade_widget_get_from_gobject (l->data);
		
		if (gw && gw->internal && strcmp (gw->internal, name) == 0)
		{
			child = G_OBJECT (l->data);
			break;
		}
		
		l= l->next;
	}
	g_list_free (children);

	return child;
}

static void
glade_gtk_box_notebook_child_insert_remove_action (GladeWidgetAdaptor *adaptor,
						   GObject *container,
						   GObject *object,
						   const gchar *size_prop,
						   const gchar *group_format,
						   gboolean remove,
						   gboolean after);

void
glade_gtk_box_child_action_activate (GladeWidgetAdaptor *adaptor,
				     GObject            *container,
				     GObject            *object,
				     const gchar        *action_path)
{
	if (strcmp (action_path, "insert_after") == 0)
	{
		glade_gtk_box_notebook_child_insert_remove_action (adaptor, container,
								   object, "size",
								   _("Insert placeholder to %s"),
								   FALSE, TRUE);
	}
	else if (strcmp (action_path, "insert_before") == 0)
	{
		glade_gtk_box_notebook_child_insert_remove_action (adaptor, container,
								   object, "size",
								   _("Insert placeholder to %s"),
								   FALSE, FALSE);
	}
	else if (strcmp (action_path, "remove_slot") == 0)
	{
		glade_gtk_box_notebook_child_insert_remove_action (adaptor, container,
								   object, "size",
								   _("Remove placeholder from %s"),
								   TRUE, FALSE);
	}
	else
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_action_activate (adaptor,
									   container,
									   object,
									   action_path);
}

/* ----------------------------- GtkTable ------------------------------ */
typedef struct {
	/* comparable part: */
	GladeWidget *widget;
	gint         left_attach;
	gint         right_attach;
	gint         top_attach;
	gint         bottom_attach;
} GladeGtkTableChild;

typedef enum {
	DIR_UP,
	DIR_DOWN,
	DIR_LEFT,
	DIR_RIGHT
} GladeTableDir;

#define TABLE_CHILD_CMP_SIZE (sizeof (GladeWidget *) + (sizeof (gint) * 4))

static GladeGtkTableChild table_edit = { 0, };
static GladeGtkTableChild table_cur_attach = { 0, };

static void
glade_gtk_table_get_child_attachments (GtkWidget *table,
				       GtkWidget *child,
				       GtkTableChild *tchild)
{
	guint left, right, top, bottom;

	gtk_container_child_get (GTK_CONTAINER (table), child,
				 "left-attach", (guint *)&left,
				 "right-attach", (guint *)&right,
				 "bottom-attach", (guint *)&bottom,
				 "top-attach", (guint *)&top,
				 NULL);

	tchild->widget        = child;
	tchild->left_attach   = left;
	tchild->right_attach  = right;
	tchild->top_attach    = top;
	tchild->bottom_attach = bottom;
}


/* Takes a point (x or y depending on 'row') relative to
 * table, and returns the row or column in which the point
 * was found.
 */
static gint
glade_gtk_table_get_row_col_from_point (GtkTable *table,
					gboolean  row,
					gint      point)
{
	GtkTableChild  tchild;
	GtkAllocation  allocation;
	GList         *list, *children;
	gint           span, trans_point, size, base, end;

	children = gtk_container_get_children (GTK_CONTAINER (table));

	for (list = children;
	     list;
	     list = list->next)
	{
		glade_gtk_table_get_child_attachments (GTK_WIDGET (table),
						       GTK_WIDGET (list->data), &tchild);

		if (row)
			gtk_widget_translate_coordinates 
				(GTK_WIDGET (table), tchild.widget,
				 0, point, NULL, &trans_point);
		else
			gtk_widget_translate_coordinates
				(GTK_WIDGET (table), tchild.widget,
				 point, 0, &trans_point, NULL);

		gtk_widget_get_allocation (tchild.widget, &allocation);
		/* Find any widget in our row/column
		 */
		end = row ? allocation.height : allocation.width;

		if (trans_point >= 0 &&
		    /* should be trans_point < end ... test FIXME ! */
		    trans_point <  end)
		{
			base = row ? tchild.top_attach : tchild.left_attach;
			size = row ? allocation.height : allocation.width;
			span = row ? (tchild.bottom_attach - tchild.top_attach) :
				(tchild.right_attach - tchild.left_attach);

			return base + (trans_point * span / size);
		}
	}
	g_list_free (children);

	return -1;
}


static gboolean
glade_gtk_table_point_crosses_threshold (GtkTable      *table,
					 gboolean       row,
					 gint           num,
					 GladeTableDir  dir,
					 gint           point)
{

	GtkTableChild  tchild;
	GtkAllocation  allocation;
	GList         *list, *children;
	gint           span, trans_point, size, rowcol_size, base;
	
	children = gtk_container_get_children (GTK_CONTAINER (table));

	for (list = children; list; list = list->next)
	{
		glade_gtk_table_get_child_attachments (GTK_WIDGET (table),
						       GTK_WIDGET (list->data), &tchild);
	
		/* Find any widget in our row/column
		 */
		if ((row && num >= tchild.top_attach && num < tchild.bottom_attach) ||
		    (!row && num >= tchild.left_attach && num < tchild.right_attach))
		{

			if (row)
				gtk_widget_translate_coordinates 
					(GTK_WIDGET (table), tchild.widget,
					 0, point, NULL, &trans_point);
			else
				gtk_widget_translate_coordinates
					(GTK_WIDGET (table), tchild.widget,
					 point, 0, &trans_point, NULL);

			span = row ? (tchild.bottom_attach - tchild.top_attach) :
				(tchild.right_attach - tchild.left_attach);
			gtk_widget_get_allocation (tchild.widget, &allocation);
			size = row ? allocation.height : allocation.width;

			base         = row ? tchild.top_attach : tchild.left_attach;
			rowcol_size  = size / span;
			trans_point -= (num - base) * rowcol_size;

#if 0
			g_print ("dir: %s, widget size: %d, rowcol size: %d, "
				 "requested rowcol: %d, widget base rowcol: %d, trim: %d, "
				 "widget point: %d, thresh: %d\n",
				 dir == DIR_UP ? "up" : dir == DIR_DOWN ? "down" :
				 dir == DIR_LEFT ? "left" : "right",
				 size, rowcol_size, num, base, (num - base) * rowcol_size,
				 trans_point,
				 dir == DIR_UP || dir == DIR_LEFT ?
				 (rowcol_size / 2) :
				 (rowcol_size / 2));
#endif
			switch (dir)
			{
			case DIR_UP:
			case DIR_LEFT:
				return trans_point <= (rowcol_size / 2);
			case DIR_DOWN:
			case DIR_RIGHT:
				return trans_point >= (rowcol_size / 2);
			default:
				break;
			}
		}
		
	}

	g_list_free (children);

	return FALSE;
}

static gboolean
glade_gtk_table_get_attachments (GladeFixed         *fixed,
				 GtkTable           *table,
				 GdkRectangle       *rect,
				 GladeGtkTableChild *configure)
{
	gint  center_x, center_y, row, column;
	guint n_columns, n_rows;
	center_x  = rect->x + (rect->width / 2);
	center_y  = rect->y + (rect->height / 2);

	column = glade_gtk_table_get_row_col_from_point
		(table, FALSE, center_x);

	row = glade_gtk_table_get_row_col_from_point
		(table, TRUE, center_y);

	/* its a start, now try to grow when the rect extents
	 * reach at least half way into the next row/column 
	 */
	configure->left_attach      = column;
	configure->right_attach     = column + 1;
	configure->top_attach       = row;
	configure->bottom_attach    = row +1;

	if (column >= 0 && row >= 0)
	{

		g_object_get (table,
                              "n-columns", &n_columns,
                              "n-rows", &n_rows,
                              NULL);

		/* Check and expand left
		 */
		while (configure->left_attach > 0)
		{
			if (rect->x < fixed->child_x_origin &&
			    fixed->operation != GLADE_CURSOR_DRAG &&
			    GLADE_FIXED_CURSOR_LEFT (fixed->operation) == FALSE)
				break;

			if (glade_gtk_table_point_crosses_threshold 
			    (table, FALSE, configure->left_attach -1,
			     DIR_LEFT, rect->x) == FALSE)
				break;

			configure->left_attach--;
		}

		/* Check and expand right
		 */
		while (configure->right_attach < n_columns)
		{
			if (rect->x + rect->width >
			    fixed->child_x_origin + fixed->child_width_origin &&
			    fixed->operation != GLADE_CURSOR_DRAG &&
			    GLADE_FIXED_CURSOR_RIGHT (fixed->operation) == FALSE)
				break;

			if (glade_gtk_table_point_crosses_threshold
			    (table, FALSE, configure->right_attach,
			     DIR_RIGHT, rect->x + rect->width) == FALSE)
				break;

			configure->right_attach++;
		}

		/* Check and expand top
		 */
		while (configure->top_attach > 0)
		{
			if (rect->y < fixed->child_y_origin &&
			    fixed->operation != GLADE_CURSOR_DRAG &&
			    GLADE_FIXED_CURSOR_TOP (fixed->operation) == FALSE)
				break;

			if (glade_gtk_table_point_crosses_threshold 
			    (table, TRUE, configure->top_attach -1,
			     DIR_UP, rect->y) == FALSE)
				break;

			configure->top_attach--;
		}

		/* Check and expand bottom
		 */
		while (configure->bottom_attach < n_rows)
		{
			if (rect->y + rect->height >
			    fixed->child_y_origin + fixed->child_height_origin &&
			    fixed->operation != GLADE_CURSOR_DRAG &&
			    GLADE_FIXED_CURSOR_BOTTOM (fixed->operation) == FALSE)
				break;

			if (glade_gtk_table_point_crosses_threshold
			    (table, TRUE, configure->bottom_attach,
			     DIR_DOWN, rect->y + rect->height) == FALSE)
				break;

			configure->bottom_attach++;
		}
	}

	/* Keep the same row/col span when performing a drag
	 */
	if (fixed->operation == GLADE_CURSOR_DRAG)
	{
		gint col_span =	table_edit.right_attach - table_edit.left_attach;
		gint row_span = table_edit.bottom_attach - table_edit.top_attach;

		if (rect->x < fixed->child_x_origin)
			configure->right_attach = configure->left_attach + col_span;
		else
			configure->left_attach = configure->right_attach - col_span;

		if (rect->y < fixed->child_y_origin)
			configure->bottom_attach = configure->top_attach + row_span;
		else
			configure->top_attach = configure->bottom_attach - row_span;
	} else if (fixed->operation == GLADE_CURSOR_RESIZE_RIGHT) {
		configure->left_attach   = table_edit.left_attach;
		configure->top_attach    = table_edit.top_attach;
		configure->bottom_attach = table_edit.bottom_attach;
	} else if (fixed->operation == GLADE_CURSOR_RESIZE_LEFT) {
		configure->right_attach  = table_edit.right_attach;
		configure->top_attach    = table_edit.top_attach;
		configure->bottom_attach = table_edit.bottom_attach;
	} else if (fixed->operation == GLADE_CURSOR_RESIZE_TOP) {
		configure->left_attach   = table_edit.left_attach;
		configure->right_attach  = table_edit.right_attach;
		configure->bottom_attach = table_edit.bottom_attach;
	} else if (fixed->operation == GLADE_CURSOR_RESIZE_BOTTOM) {
		configure->left_attach   = table_edit.left_attach;
		configure->right_attach  = table_edit.right_attach;
		configure->top_attach    = table_edit.top_attach;
	}

	return column >= 0 && row >= 0;
}

static gboolean
glade_gtk_table_configure_child (GladeFixed   *fixed,
				 GladeWidget  *child,
				 GdkRectangle *rect,
				 GtkWidget    *table)
{
	GladeGtkTableChild configure = { child, };

	/* Sometimes we are unable to find a widget in the appropriate column,
	 * usually because a placeholder hasnt had its size allocation yet.
	 */
	if (glade_gtk_table_get_attachments (fixed, GTK_TABLE (table), rect, &configure))
	{
		if (memcmp (&configure, &table_cur_attach, TABLE_CHILD_CMP_SIZE) != 0)
		{

			glade_property_push_superuser ();
			glade_widget_pack_property_set (child, "left-attach",   
							configure.left_attach);
			glade_widget_pack_property_set (child, "right-attach",  
							configure.right_attach);
			glade_widget_pack_property_set (child, "top-attach",    
							configure.top_attach);
			glade_widget_pack_property_set (child, "bottom-attach", 
							configure.bottom_attach);
			glade_property_pop_superuser ();

			memcpy (&table_cur_attach, &configure, TABLE_CHILD_CMP_SIZE);
		}
	}
	return TRUE;
}


static gboolean
glade_gtk_table_configure_begin (GladeFixed  *fixed,
				 GladeWidget *child,
				 GtkWidget   *table)
{

	table_edit.widget = child;

	glade_widget_pack_property_get (child, "left-attach", 
					&table_edit.left_attach);
	glade_widget_pack_property_get (child, "right-attach", 
					&table_edit.right_attach);
	glade_widget_pack_property_get (child, "top-attach", 
					&table_edit.top_attach);
	glade_widget_pack_property_get (child, "bottom-attach", 
					&table_edit.bottom_attach);

	memcpy (&table_cur_attach, &table_edit, TABLE_CHILD_CMP_SIZE);

	return TRUE;
}

static gboolean
glade_gtk_table_configure_end (GladeFixed  *fixed,
			       GladeWidget *child,
			       GtkWidget   *table)
{
	GladeGtkTableChild new_child = { child, };

	glade_widget_pack_property_get (child, "left-attach", 
					&new_child.left_attach);
	glade_widget_pack_property_get (child, "right-attach", 
					&new_child.right_attach);
	glade_widget_pack_property_get (child, "top-attach", 
					&new_child.top_attach);
	glade_widget_pack_property_get (child, "bottom-attach", 
					&new_child.bottom_attach);

	/* Compare the meaningfull part of the current edit. */
	if (memcmp (&new_child, &table_edit, TABLE_CHILD_CMP_SIZE) != 0)
	{
		GValue left_attach_value   = { 0, };
		GValue right_attach_value = { 0, };
		GValue top_attach_value    = { 0, };
		GValue bottom_attach_value = { 0, };

		GValue new_left_attach_value   = { 0, };
		GValue new_right_attach_value = { 0, };
		GValue new_top_attach_value    = { 0, };
		GValue new_bottom_attach_value = { 0, };

		GladeProperty *left_attach_prop, *right_attach_prop, 
			*top_attach_prop, *bottom_attach_prop;

		left_attach_prop   = glade_widget_get_pack_property (child, "left-attach");
		right_attach_prop  = glade_widget_get_pack_property (child, "right-attach");
		top_attach_prop    = glade_widget_get_pack_property (child, "top-attach");
		bottom_attach_prop = glade_widget_get_pack_property (child, "bottom-attach");

		g_return_val_if_fail (GLADE_IS_PROPERTY (left_attach_prop), FALSE);
		g_return_val_if_fail (GLADE_IS_PROPERTY (right_attach_prop), FALSE);
		g_return_val_if_fail (GLADE_IS_PROPERTY (top_attach_prop), FALSE);
		g_return_val_if_fail (GLADE_IS_PROPERTY (bottom_attach_prop), FALSE);

		glade_property_get_value (left_attach_prop,   &new_left_attach_value);
		glade_property_get_value (right_attach_prop,  &new_right_attach_value);
		glade_property_get_value (top_attach_prop,    &new_top_attach_value);
		glade_property_get_value (bottom_attach_prop, &new_bottom_attach_value);

		g_value_init (&left_attach_value, G_TYPE_UINT);
		g_value_init (&right_attach_value, G_TYPE_UINT);
		g_value_init (&top_attach_value, G_TYPE_UINT);
		g_value_init (&bottom_attach_value, G_TYPE_UINT);

		g_value_set_uint (&left_attach_value,   table_edit.left_attach);
		g_value_set_uint (&right_attach_value,  table_edit.right_attach);
		g_value_set_uint (&top_attach_value,    table_edit.top_attach);
		g_value_set_uint (&bottom_attach_value, table_edit.bottom_attach);

		glade_command_push_group (_("Placing %s inside %s"), 
					  child->name,
					  GLADE_WIDGET (fixed)->name);
		glade_command_set_properties
			(left_attach_prop,   &left_attach_value,   &new_left_attach_value,
			 right_attach_prop,  &right_attach_value,  &new_right_attach_value,
			 top_attach_prop,    &top_attach_value,    &new_top_attach_value,
			 bottom_attach_prop, &bottom_attach_value, &new_bottom_attach_value,
			 NULL);
		glade_command_pop_group ();

		g_value_unset (&left_attach_value);
		g_value_unset (&right_attach_value);
		g_value_unset (&top_attach_value);
		g_value_unset (&bottom_attach_value);
		g_value_unset (&new_left_attach_value);
		g_value_unset (&new_right_attach_value);
		g_value_unset (&new_top_attach_value);
		g_value_unset (&new_bottom_attach_value);
	}

	return TRUE;
}

void
glade_gtk_table_post_create (GladeWidgetAdaptor *adaptor,
			     GObject            *container, 
			     GladeCreateReason   reason)
{
	GladeWidget    *gwidget =
		glade_widget_get_from_gobject (container);
	
	g_signal_connect (G_OBJECT (gwidget), "configure-child",
			  G_CALLBACK (glade_gtk_table_configure_child), container);

	g_signal_connect (G_OBJECT (gwidget), "configure-begin",
			  G_CALLBACK (glade_gtk_table_configure_begin), container);

	g_signal_connect (G_OBJECT (gwidget), "configure-end",
			  G_CALLBACK (glade_gtk_table_configure_end), container);
}

static gboolean
glade_gtk_table_has_child (GtkTable *table, guint left_attach, guint top_attach)
{
	GList *list, *children;
	gboolean ret = FALSE;

	children = gtk_container_get_children (GTK_CONTAINER (table));

	for (list = children;
	     list && list->data;
	     list = list->next)
	{
		GtkTableChild child;

		glade_gtk_table_get_child_attachments (GTK_WIDGET (table),
						       GTK_WIDGET (list->data), &child);

		if (left_attach >= child.left_attach && left_attach < child.right_attach &&
		    top_attach >= child.top_attach && top_attach < child.bottom_attach)
		{
			ret = TRUE;
			break;
		}
	}

	g_list_free (children);

	return ret;
}

static gboolean
glade_gtk_table_widget_exceeds_bounds (GtkTable *table, gint n_rows, gint n_cols)
{
	GList *list, *children;
	gboolean ret = FALSE;

	children = gtk_container_get_children (GTK_CONTAINER (table));

	for (list = children;
	     list && list->data;
	     list = list->next)
	{
		GtkTableChild child;

		glade_gtk_table_get_child_attachments (GTK_WIDGET (table),
						       GTK_WIDGET (list->data), &child);

		if (GLADE_IS_PLACEHOLDER(child.widget) == FALSE &&
		    (child.right_attach  > n_cols ||
		     child.bottom_attach > n_rows))
	       	{
			ret = TRUE;
			break;
		}
	}
	return ret;
}

#define TABLE_OCCUPIED(occmap, n_columns, col, row) \
    (occmap)[row * n_columns + col]

static void
glade_gtk_table_build_occupation_maps(GtkTable *table, guint n_columns, guint n_rows,
				      gchar **child_map, gpointer **placeholder_map)
{
    guint i, j;
    GList *list, *children = gtk_container_get_children (GTK_CONTAINER (table));

    *child_map = g_malloc0(n_columns * n_rows * sizeof(gchar));  /* gchar is smaller than gboolean */
    *placeholder_map = g_malloc0(n_columns * n_rows * sizeof(gpointer));

    for (list = children; list && list->data; list = list->next)
    {
	GtkTableChild child;

	glade_gtk_table_get_child_attachments (GTK_WIDGET (table),
					       GTK_WIDGET (list->data), &child);

	if (GLADE_IS_PLACEHOLDER(list->data))
	{
	    /* assumption: placeholders are always attached to exactly 1 cell */
	    TABLE_OCCUPIED(*placeholder_map, n_columns, child.left_attach, child.top_attach) = list->data;
	}
	else
	{
	    for (i = child.left_attach; i < child.right_attach && i < n_columns; i++)
	    {
		for (j = child.top_attach; j < child.bottom_attach && j < n_rows; j++)
		{
		    TABLE_OCCUPIED(*child_map, n_columns, i, j) = 1;
		}
	    }
	}
    }
    g_list_free (children);
}

static void
glade_gtk_table_refresh_placeholders (GtkTable *table)
{
	guint n_columns, n_rows, i, j;
	gchar *child_map;
	gpointer *placeholder_map;

	g_object_get (table,
		      "n-columns", &n_columns,
		      "n-rows", &n_rows,
		      NULL);

	glade_gtk_table_build_occupation_maps(table, n_columns, n_rows,
					     &child_map, &placeholder_map);

	for (i = 0; i < n_columns; i++)
	{
	    for (j = 0; j < n_rows; j++)
	    {
		gpointer placeholder = TABLE_OCCUPIED(placeholder_map, n_columns, i, j);

		if (TABLE_OCCUPIED(child_map, n_columns, i, j))
		{
		    if (placeholder)
		    {
			gtk_container_remove (GTK_CONTAINER (table), 
					      GTK_WIDGET (placeholder));
		    }
		}
		else
		{
		    if (!placeholder)
		    {
			gtk_table_attach_defaults (table, 
						   glade_placeholder_new (), 
						   i, i + 1, j, j + 1);
		    }
		}
	    }
	}
	g_free(child_map);
	g_free(placeholder_map);
	gtk_container_check_resize (GTK_CONTAINER (table));
}

static void
gtk_table_children_callback (GtkWidget *widget,
				 gpointer client_data)
{
	GList **children;

	children = (GList**) client_data;
	*children = g_list_prepend (*children, widget);
}

GList *
glade_gtk_table_get_children (GladeWidgetAdaptor  *adaptor,
				  GtkContainer        *container)
{
	GList *children = NULL;

	g_return_val_if_fail (GTK_IS_TABLE (container), NULL);

	gtk_container_forall (container,
			      gtk_table_children_callback,
			      &children);

	/* GtkTable has the children list already reversed */
	return children;
}

void
glade_gtk_table_add_child (GladeWidgetAdaptor *adaptor,
			   GObject            *object, 
			   GObject            *child)
{
	g_return_if_fail (GTK_IS_TABLE (object));
	g_return_if_fail (GTK_IS_WIDGET (child));

	gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));

	glade_gtk_table_refresh_placeholders (GTK_TABLE (object));
}

void
glade_gtk_table_remove_child (GladeWidgetAdaptor *adaptor,
			      GObject            *object, 
			      GObject            *child)
{
	g_return_if_fail (GTK_IS_TABLE (object));
	g_return_if_fail (GTK_IS_WIDGET (child));
	
	gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
	
	glade_gtk_table_refresh_placeholders (GTK_TABLE (object));
}

void
glade_gtk_table_replace_child (GladeWidgetAdaptor *adaptor,
			       GtkWidget          *container,
			       GtkWidget          *current,
			       GtkWidget          *new_widget)
{
	g_return_if_fail (GTK_IS_TABLE (container));
	g_return_if_fail (GTK_IS_WIDGET (current));
	g_return_if_fail (GTK_IS_WIDGET (new_widget));
	
	/* Chain Up */
	GWA_GET_CLASS
		(GTK_TYPE_CONTAINER)->replace_child (adaptor, 
						     G_OBJECT (container), 
						     G_OBJECT (current), 
						     G_OBJECT (new_widget));
	
	/* If we are replacing a GladeWidget, we must refresh placeholders
	 * because the widget may have spanned multiple rows/columns, we must
	 * not do so in the case we are pasting multiple widgets into a table,
	 * where destroying placeholders results in default packing properties
	 * (since the remaining placeholder templates no longer exist, only the
	 * first pasted widget would have proper packing properties).
	 */
	if (glade_widget_get_from_gobject (new_widget) == NULL)
		glade_gtk_table_refresh_placeholders (GTK_TABLE (container));

}

static void
glade_gtk_table_set_n_common (GObject *object, const GValue *value, gboolean for_rows)
{
	GladeWidget *widget;
	GtkTable    *table;
	guint        new_size, old_size, n_columns, n_rows;

	table = GTK_TABLE (object);
	g_return_if_fail (GTK_IS_TABLE (table));

	g_object_get (table,
                      "n-columns", &n_columns,
                      "n-rows", &n_rows,
                      NULL);

	new_size = g_value_get_uint (value);
	old_size = for_rows ? n_rows : n_columns;

	if (new_size < 1)
		return;

	if (glade_gtk_table_widget_exceeds_bounds
	    (table,
	     for_rows ? new_size : n_rows,
	     for_rows ? n_columns : new_size))
		/* Refuse to shrink if it means orphaning widgets */
		return;

	widget = glade_widget_get_from_gobject (GTK_WIDGET (table));
	g_return_if_fail (widget != NULL);

	if (for_rows)
		gtk_table_resize (table, new_size, n_columns);
	else
		gtk_table_resize (table, n_rows, new_size);

	/* Fill table with placeholders */
	glade_gtk_table_refresh_placeholders (table);
	
	if (new_size < old_size)
	{
		/* Remove from the bottom up */
		GList *list, *children;
		GList *list_to_free = NULL;

		children = gtk_container_get_children (GTK_CONTAINER (table));

		for (list = children;
		     list && list->data;
		     list = list->next)
		{
			GtkTableChild child;
			guint start, end;

			glade_gtk_table_get_child_attachments (GTK_WIDGET (table),
							       GTK_WIDGET (list->data), &child);

			start = for_rows ? child.top_attach : child.left_attach;
			end = for_rows ? child.bottom_attach : child.right_attach;

			/* We need to completely remove it */
			if (start >= new_size)
			{
				list_to_free = g_list_prepend (list_to_free, child.widget);
				continue;
			}

			/* If the widget spans beyond the new border,
			 * we should resize it to fit on the new table */
			if (end > new_size)
				gtk_container_child_set
					(GTK_CONTAINER (table), GTK_WIDGET (child.widget),
					 for_rows ? "bottom_attach" : "right_attach",
					 new_size, NULL);
		}
		
		g_list_free (children);

		if (list_to_free)
		{
			for (list = g_list_first(list_to_free);
			     list && list->data;
			     list = list->next)
			{
				g_object_ref (G_OBJECT (list->data));
				gtk_container_remove (GTK_CONTAINER (table),
						      GTK_WIDGET(list->data));
				/* This placeholder is no longer valid, force destroy */
				gtk_widget_destroy (GTK_WIDGET(list->data));
			}
			g_list_free (list_to_free);
		}
		gtk_table_resize (table,
				  for_rows ? new_size : n_rows,
				  for_rows ? n_columns : new_size);
	}
}

void
glade_gtk_table_set_property (GladeWidgetAdaptor *adaptor,
			      GObject            *object, 
			      const gchar        *id,
			      const GValue       *value)
{
	if (!strcmp (id, "n-rows"))
		glade_gtk_table_set_n_common (object, value, TRUE);
	else if (!strcmp (id, "n-columns"))
		glade_gtk_table_set_n_common (object, value, FALSE);
	else
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object,
								  id, value);
}

static gboolean 
glade_gtk_table_verify_n_common (GObject *object, const GValue *value, gboolean for_rows)
{
	GtkTable *table = GTK_TABLE(object);
	guint     n_columns, n_rows, new_size = g_value_get_uint (value);

	g_object_get (table,
		      "n-columns", &n_columns,
		      "n-rows", &n_rows,
		      NULL);

	if (glade_gtk_table_widget_exceeds_bounds
	    (table,
	     for_rows ? new_size : n_rows,
	     for_rows ? n_columns : new_size))
		/* Refuse to shrink if it means orphaning widgets */
		return FALSE;

	return TRUE;
}

gboolean
glade_gtk_table_verify_property (GladeWidgetAdaptor *adaptor,
				 GObject            *object, 
				 const gchar        *id,
				 const GValue       *value)
{
	if (!strcmp (id, "n-rows"))
		return glade_gtk_table_verify_n_common (object, value, TRUE);
	else if (!strcmp (id, "n-columns"))
		return glade_gtk_table_verify_n_common (object, value, FALSE);
	else if (GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property)
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property (adaptor, object,
								     id, value);

	return TRUE;
}

void
glade_gtk_table_set_child_property (GladeWidgetAdaptor *adaptor,
				    GObject            *container,
				    GObject            *child,
				    const gchar        *property_name,
				    GValue             *value)
{
	g_return_if_fail (GTK_IS_TABLE (container));
	g_return_if_fail (GTK_IS_WIDGET (child));
	g_return_if_fail (property_name != NULL && value != NULL);

	GWA_GET_CLASS
		(GTK_TYPE_CONTAINER)->child_set_property (adaptor, 
							  container, child,
							  property_name, value);
	
	if (strcmp (property_name, "bottom-attach") == 0 ||
	    strcmp (property_name, "left-attach") == 0 ||
	    strcmp (property_name, "right-attach") == 0 ||
	    strcmp (property_name, "top-attach") == 0)
	{
		/* Refresh placeholders */
		glade_gtk_table_refresh_placeholders (GTK_TABLE (container));
	}

}

static gboolean
glade_gtk_table_verify_attach_common (GObject *object,
				      GValue *value,
				      guint *val,
				      const gchar *prop,
				      guint *prop_val,
				      const gchar *parent_prop,
				      guint *parent_val)
{
	GladeWidget *widget, *parent;
	
	widget = glade_widget_get_from_gobject (object);
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), TRUE);
	parent = glade_widget_get_parent (widget);
	g_return_val_if_fail (GLADE_IS_WIDGET (parent), TRUE);
	
	*val = g_value_get_uint (value);
	glade_widget_property_get (widget, prop, prop_val);
	glade_widget_property_get (parent, parent_prop, parent_val);

	return FALSE;
}

static gboolean
glade_gtk_table_verify_left_top_attach (GObject *object,
					GValue *value,
					const gchar *prop,
					const gchar *parent_prop)
{
	guint val, prop_val, parent_val;
	
	if (glade_gtk_table_verify_attach_common (object, value, &val,
						  prop, &prop_val,
						  parent_prop, &parent_val))
		return FALSE;
	
	if (val >= parent_val || val >= prop_val) 
		return FALSE;
		
	return TRUE;
}

static gboolean
glade_gtk_table_verify_right_bottom_attach (GObject *object,
					    GValue *value,
					    const gchar *prop,
					    const gchar *parent_prop)
{
	guint val, prop_val, parent_val;
	
	if (glade_gtk_table_verify_attach_common (object, value, &val,
						  prop, &prop_val,
						  parent_prop, &parent_val))
		return FALSE;
	
	if (val <= prop_val || val > parent_val) 
		return FALSE;
		
	return TRUE;
}

gboolean
glade_gtk_table_child_verify_property (GladeWidgetAdaptor *adaptor,
				       GObject            *container,
				       GObject            *child,
				       const gchar        *id,
				       GValue             *value)
{
	if (!strcmp (id, "left-attach"))
		return glade_gtk_table_verify_left_top_attach (child, 
							       value,
							       "right-attach",
							       "n-columns");
	else if (!strcmp (id, "right-attach"))
		return glade_gtk_table_verify_right_bottom_attach (child,
								   value,
								   "left-attach",
								   "n-columns");
	else if (!strcmp (id, "top-attach"))
		return glade_gtk_table_verify_left_top_attach (child, 
							       value,
							       "bottom-attach",
							       "n-rows");
	else if (!strcmp (id, "bottom-attach"))
		return glade_gtk_table_verify_right_bottom_attach (child,
								   value,
								   "top-attach",
								   "n-rows");
	else if (GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_verify_property)
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->child_verify_property (adaptor, 
								     container, child,
								     id, value);

	return TRUE;
}

static void
glade_gtk_table_child_insert_remove_action (GladeWidgetAdaptor *adaptor,
					    GObject *container,
					    GObject *object,
					    const gchar *group_format,
					    const gchar *n_row_col,
					    const gchar *attach1, /* should be smaller (top/left) attachment */
					    const gchar *attach2, /* should be larger (bot/right) attachment */
					    gboolean remove,
					    gboolean after)
{
	GladeWidget *parent;
	GList *children, *l;
	gint child_pos, size, offset;
	
	gtk_container_child_get (GTK_CONTAINER (container),
				 GTK_WIDGET (object),
				 after ? attach2 : attach1, &child_pos, NULL);
	
	parent = glade_widget_get_from_gobject (container);
	glade_command_push_group (group_format, glade_widget_get_name (parent));
		
	children = glade_widget_adaptor_get_children (adaptor, container);
	/* Make sure widgets does not get destroyed */
	g_list_foreach (children, (GFunc) g_object_ref, NULL);
	
	glade_widget_property_get (parent, n_row_col, &size);
	
	if (remove)
	{
		GList *del = NULL;
		/* Remove children first */
		for (l = children; l; l = g_list_next (l))
		{
			GladeWidget *gchild = glade_widget_get_from_gobject (l->data);
			gint pos1, pos2;
			
			/* Skip placeholders */
			if (gchild == NULL) continue;
		
			glade_widget_pack_property_get (gchild, attach1, &pos1);
			glade_widget_pack_property_get (gchild, attach2, &pos2);
			if ((pos1+1 == pos2) && ((after ? pos2 : pos1) == child_pos))
			{
				del = g_list_prepend (del, gchild);
			}
		}
		if (del)
		{
			glade_command_delete (del);
			g_list_free (del);
		}
		offset = -1;
	}
	else
	{
		/* Expand the table */
		glade_command_set_property (glade_widget_get_property (parent, n_row_col),
					    size + 1);
		offset = 1;
	}
	
	/* Reorder children */
	for (l = children; l; l = g_list_next (l))
	{
		GladeWidget *gchild = glade_widget_get_from_gobject (l->data);
		gint pos;
			
		/* Skip placeholders */
		if (gchild == NULL) continue;
		
		/* if removing, do top/left before bot/right */
		if (remove)
		{
			/* adjust top-left attachment*/
			glade_widget_pack_property_get (gchild, attach1, &pos);
			if(pos > child_pos || (after && pos == child_pos))
			{
				glade_command_set_property (glade_widget_get_pack_property (gchild, attach1), pos+offset);
			}

			/* adjust bottom-right attachment*/
			glade_widget_pack_property_get (gchild, attach2, &pos);
			if(pos > child_pos || (after && pos == child_pos))
			{
				glade_command_set_property (glade_widget_get_pack_property (gchild, attach2), pos+offset);
			}
	
		}
		/* if inserting, do bot/right before top/left */
		else
		{
			/* adjust bottom-right attachment*/
			glade_widget_pack_property_get (gchild, attach2, &pos);
			if(pos > child_pos)
			{
				glade_command_set_property (glade_widget_get_pack_property (gchild, attach2), pos+offset);
			}
	
			/* adjust top-left attachment*/
			glade_widget_pack_property_get (gchild, attach1, &pos);
			if(pos >= child_pos)
			{
				glade_command_set_property (glade_widget_get_pack_property (gchild, attach1), pos+offset);
			}
		}
	}
	
	if (remove)
	{
		/* Shrink the table */
		glade_command_set_property (glade_widget_get_property (parent, n_row_col),
					    size - 1);
	}
	
	g_list_foreach (children, (GFunc) g_object_unref, NULL);
	g_list_free (children);
	
	glade_command_pop_group ();
}

void
glade_gtk_table_child_action_activate (GladeWidgetAdaptor *adaptor,
				       GObject            *container,
				       GObject            *object,
				       const gchar        *action_path)
{
	if (strcmp (action_path, "insert_row/after") == 0)
	{
		glade_gtk_table_child_insert_remove_action (adaptor, container, object,
							    _("Insert Row on %s"),
							    "n-rows","top-attach",
							    "bottom-attach",
							    FALSE, TRUE);
	}
	else if (strcmp (action_path, "insert_row/before") == 0)
	{
		glade_gtk_table_child_insert_remove_action (adaptor, container, object,
							    _("Insert Row on %s"),
							    "n-rows","top-attach",
							    "bottom-attach",
							    FALSE, FALSE);
	}
	else if (strcmp (action_path, "insert_column/after") == 0)
	{
		glade_gtk_table_child_insert_remove_action (adaptor, container, object,
							    _("Insert Column on %s"),
							    "n-columns","left-attach",
							    "right-attach",
							    FALSE, TRUE);
	}
	else if (strcmp (action_path, "insert_column/before") == 0)
	{
		glade_gtk_table_child_insert_remove_action (adaptor, container, object,
							    _("Insert Column on %s"),
							    "n-columns","left-attach",
							    "right-attach",
							    FALSE, FALSE);
	}
	else if (strcmp (action_path, "remove_column") == 0)
	{
		glade_gtk_table_child_insert_remove_action (adaptor, container, object,
							    _("Remove Column on %s"),
							    "n-columns","left-attach",
							    "right-attach",
							    TRUE, FALSE);
	}
	else if (strcmp (action_path, "remove_row") == 0)
	{
		glade_gtk_table_child_insert_remove_action (adaptor, container, object,
							    _("Remove Row on %s"),
							    "n-rows","top-attach",
							    "bottom-attach",
							    TRUE, FALSE);
	}
	else
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_action_activate (adaptor,
									   container,
									   object,
									   action_path);
}

/* ----------------------------- GtkFrame ------------------------------ */
void
glade_gtk_frame_post_create (GladeWidgetAdaptor *adaptor,
			     GObject            *frame, 
			     GladeCreateReason   reason)
{
	static GladeWidgetAdaptor *label_adaptor = NULL, *alignment_adaptor = NULL;
	GladeWidget *gframe, *glabel, *galignment;
	GtkWidget *label;
	gchar     *label_text;
	
	if (reason != GLADE_CREATE_USER)
		return;
		
	g_return_if_fail (GTK_IS_FRAME (frame));
	gframe = glade_widget_get_from_gobject (frame);
	g_return_if_fail (GLADE_IS_WIDGET (gframe));
	
	/* If we didnt put this object here or if frame is an aspect frame... */
	if (((label = gtk_frame_get_label_widget (GTK_FRAME (frame))) == NULL ||
	    (glade_widget_get_from_gobject (label) == NULL)) &&
	    (GTK_IS_ASPECT_FRAME (frame) == FALSE))
	{

		if (label_adaptor == NULL)
			label_adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_LABEL);
		if (alignment_adaptor == NULL)
			alignment_adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_ALIGNMENT);

		/* add label (as an internal child) */
		glabel = glade_widget_adaptor_create_widget (label_adaptor, FALSE,
							   "parent", gframe, 
							   "project", glade_widget_get_project (gframe), 
							   NULL);

		label_text = g_strdup_printf ("<b>%s</b>", glade_widget_get_name (gframe));
		glade_widget_property_set (glabel, "label", label_text);
		glade_widget_property_set (glabel, "use-markup", "TRUE");
		g_free (label_text);

		g_object_set_data (glabel->object, "special-child-type", "label_item");
		glade_widget_add_child (gframe, glabel, FALSE);

		/* add alignment */
		galignment = glade_widget_adaptor_create_widget (alignment_adaptor, FALSE,
						 	       "parent", gframe, 
						               "project", glade_widget_get_project (gframe), 
					                       NULL);

		glade_widget_property_set (galignment, "left-padding", 12);
		glade_widget_add_child (gframe, galignment, FALSE);
	}

	/* Chain Up */
	GWA_GET_CLASS (GTK_TYPE_CONTAINER)->post_create (adaptor, frame, reason);
}

void
glade_gtk_frame_replace_child (GladeWidgetAdaptor *adaptor,
			       GtkWidget          *container,
			       GtkWidget          *current,
			       GtkWidget          *new_widget)
{
	gchar *special_child_type;

	special_child_type =
		g_object_get_data (G_OBJECT (current), "special-child-type");

	if (special_child_type && !strcmp (special_child_type, "label_item"))
	{
		g_object_set_data (G_OBJECT (new_widget), "special-child-type", "label_item");
		gtk_frame_set_label_widget (GTK_FRAME (container), new_widget);
		return;
	}

	/* Chain Up */
	GWA_GET_CLASS
		(GTK_TYPE_CONTAINER)->replace_child (adaptor, 
						     G_OBJECT (container), 
						     G_OBJECT (current),
						     G_OBJECT (new_widget));
}

void
glade_gtk_frame_add_child (GladeWidgetAdaptor *adaptor,
			   GObject            *object, 
			   GObject            *child)
{
	GtkWidget *bin_child;
	gchar *special_child_type;

	special_child_type = g_object_get_data (child, "special-child-type");

	if (special_child_type && !strcmp (special_child_type, "label"))
	{
		g_object_set_data (child,
				   "special-child-type",
				   "label_item");
		gtk_frame_set_label_widget (GTK_FRAME (object),
					    GTK_WIDGET (child));
	}
	else if (special_child_type &&
		 !strcmp (special_child_type, "label_item"))
	{
		gtk_frame_set_label_widget (GTK_FRAME (object),
					    GTK_WIDGET (child));
	}
	else
	{
		/* Get a placeholder out of the way before adding the child
		 */
		bin_child = gtk_bin_get_child (GTK_BIN (object));
		if (bin_child)
		{
			if (GLADE_IS_PLACEHOLDER (bin_child))
				gtk_container_remove (GTK_CONTAINER (object), bin_child);
			else
			{
				g_critical ("Cant add more than one widget to a GtkFrame");
				return;
			}
		}
		gtk_container_add (GTK_CONTAINER (object),
				   GTK_WIDGET (child));
	}
}

void
glade_gtk_frame_remove_child (GladeWidgetAdaptor *adaptor,
			      GObject            *object, 
			      GObject            *child)
{
	gchar *special_child_type;

	special_child_type = g_object_get_data (child, "special-child-type");
	if (special_child_type &&
	    !strcmp (special_child_type, "label_item"))
	{
		gtk_frame_set_label_widget (GTK_FRAME (object),
					    glade_placeholder_new ());
	}
	else
	{
		gtk_container_remove (GTK_CONTAINER (object),
				      GTK_WIDGET (child));
		gtk_container_add (GTK_CONTAINER (object),
				   glade_placeholder_new ());
	}
}

static gboolean
write_special_child_label_item (GladeWidgetAdaptor *adaptor,
				GladeWidget        *widget,
				GladeXmlContext    *context,
				GladeXmlNode       *node,
				GladeWriteWidgetFunc write_func)
{
	gchar *special_child_type = NULL;
	GObject *child;

	if (glade_project_get_format (widget->project) == GLADE_PROJECT_FORMAT_GTKBUILDER)
	{
		child = widget->object;
		if (child)
			special_child_type = g_object_get_data (child, "special-child-type");
	}

	if (special_child_type && !strcmp (special_child_type, "label_item"))
	{
		g_object_set_data (child,
				   "special-child-type",
				   "label");
		write_func (adaptor, widget, context, node);
		g_object_set_data (child,
			           "special-child-type",
				   "label_item");
		return TRUE;
	}
	else
		return FALSE;
}

void
glade_gtk_frame_write_child (GladeWidgetAdaptor *adaptor,
			     GladeWidget        *widget,
			     GladeXmlContext    *context,
			     GladeXmlNode       *node)
{

	if (!write_special_child_label_item (adaptor, widget, context, node,
					     GWA_GET_CLASS(GTK_TYPE_CONTAINER)->write_child))
		/* Chain Up */
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->write_child (adaptor,
							   widget,
							   context,
							   node);
}

/* ----------------------------- GtkNotebook ------------------------------ */
typedef struct 
{
	gint   pages;
	gint   page;

	GList *children;
	GList *tabs;

	GList *extra_children;
	GList *extra_tabs;
} NotebookChildren;

static gboolean glade_gtk_notebook_setting_position = FALSE;

static gint
notebook_child_compare_func (GtkWidget *widget_a, GtkWidget *widget_b)
{
	GladeWidget *gwidget_a, *gwidget_b;
	gint pos_a = 0, pos_b = 0;

	gwidget_a = glade_widget_get_from_gobject (widget_a);
	gwidget_b = glade_widget_get_from_gobject (widget_b);
	
	g_assert (gwidget_a && gwidget_b);
	
	glade_widget_pack_property_get (gwidget_a, "position", &pos_a);
	glade_widget_pack_property_get (gwidget_b, "position", &pos_b);
	
	return pos_a - pos_b;
}

static gint
notebook_find_child (GtkWidget *check,
		     gpointer   cmp_pos_p)
{
	GladeWidget *gcheck;
	gint         position = 0, cmp_pos = GPOINTER_TO_INT (cmp_pos_p);
		
	gcheck  = glade_widget_get_from_gobject (check);
	g_assert (gcheck);
	
	glade_widget_pack_property_get (gcheck, "position", &position);

	return position - cmp_pos;
}

static gint
notebook_search_tab (GtkNotebook *notebook,
		     GtkWidget   *tab)
{
	GtkWidget *page;
	gint       i;

	for (i = 0; i < gtk_notebook_get_n_pages (notebook); i++)
	{
		page = gtk_notebook_get_nth_page (notebook, i);

		if (tab == gtk_notebook_get_tab_label (notebook, page))
			return i;
	}
	g_critical ("Unable to find tab position in a notebook");
	return -1;
}

static GtkWidget *
notebook_get_filler (NotebookChildren *nchildren, gboolean page)
{
	GtkWidget *widget = NULL;
	
	if (page && nchildren->extra_children)
	{
		widget = nchildren->extra_children->data;
		nchildren->extra_children =
			g_list_remove (nchildren->extra_children, widget);
		g_assert (widget);
	}
	else if (!page && nchildren->extra_tabs)
	{
		widget = nchildren->extra_tabs->data;
		nchildren->extra_tabs =
			g_list_remove (nchildren->extra_tabs, widget);
		g_assert (widget);
	}

	if (widget == NULL)
	{
		/* Need explicit reference here */
		widget = glade_placeholder_new ();

		g_object_ref (G_OBJECT (widget));	

		if (!page)
			g_object_set_data (G_OBJECT (widget), 
					   "special-child-type", "tab");

	}
	return widget;
}

static GtkWidget *
notebook_get_page (NotebookChildren *nchildren, gint position)
{
	GList     *node;
	GtkWidget *widget = NULL;

	if ((node = g_list_find_custom
	     (nchildren->children,
	      GINT_TO_POINTER (position),
	      (GCompareFunc)notebook_find_child)) != NULL)
	{
		widget = node->data;
		nchildren->children = 
			g_list_remove (nchildren->children, widget);
	}
	else
		widget = notebook_get_filler (nchildren, TRUE);

	return widget;
}

static GtkWidget *
notebook_get_tab (NotebookChildren *nchildren, gint position)
{
	GList     *node;
	GtkWidget *widget = NULL;

	if ((node = g_list_find_custom
	     (nchildren->tabs,
	      GINT_TO_POINTER (position),
	      (GCompareFunc)notebook_find_child)) != NULL)
	{
		widget = node->data;
		nchildren->tabs = 
			g_list_remove (nchildren->tabs, widget);
	}
	else
		widget = notebook_get_filler (nchildren, FALSE);

	return widget;
}

static NotebookChildren *
glade_gtk_notebook_extract_children (GtkWidget *notebook)
{
	NotebookChildren *nchildren;
	gchar            *special_child_type;
	GList            *list, *children =
		glade_util_container_get_all_children (GTK_CONTAINER (notebook));
	GladeWidget      *gchild;
	GtkWidget        *page;
	gint              position = 0;

	nchildren        = g_new0 (NotebookChildren, 1);
	nchildren->pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));
	nchildren->page  = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
		
	/* Ref all the project widgets and build returned list first */
	for (list = children; list; list = list->next)
	{
		if ((gchild = glade_widget_get_from_gobject (list->data)) != NULL)
		{
			special_child_type =
				g_object_get_data (G_OBJECT (list->data),
						   "special-child-type");

			glade_widget_pack_property_get (gchild, "position", &position);

			g_object_ref (G_OBJECT (list->data));

			/* Sort it into the proper struct member
			 */
			if (special_child_type == NULL)
			{
				if (g_list_find_custom (nchildren->children,
							GINT_TO_POINTER (position),
							(GCompareFunc)notebook_find_child))
					nchildren->extra_children =
						g_list_insert_sorted
						(nchildren->extra_children, list->data,
						 (GCompareFunc)notebook_child_compare_func);
				else
					nchildren->children =
						g_list_insert_sorted
						(nchildren->children, list->data,
						 (GCompareFunc)notebook_child_compare_func);
			} else {
				if (g_list_find_custom (nchildren->tabs,
							GINT_TO_POINTER (position),
							(GCompareFunc)notebook_find_child))
					nchildren->extra_tabs =
						g_list_insert_sorted
						(nchildren->extra_tabs, list->data,
						 (GCompareFunc)notebook_child_compare_func);
				else
					nchildren->tabs =
						g_list_insert_sorted
						(nchildren->tabs, list->data,
						 (GCompareFunc)notebook_child_compare_func);
			}
		}
	}

	/* Remove all pages, resulting in the unparenting of all widgets including tab-labels.
	 */
	while (gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)) > 0)
	{
		page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), 0);

		/* Explicitly remove the tab label first */
		gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), page, NULL);
		gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), 0);
	}

	if (children)
		g_list_free (children);

	return nchildren;
}

static void
glade_gtk_notebook_insert_children (GtkWidget *notebook, NotebookChildren *nchildren)
{
	gint i;
	
	/*********************************************************
                                INSERT PAGES
	 *********************************************************/
	for (i = 0; i < nchildren->pages; i++)
	{
		GtkWidget *page = notebook_get_page (nchildren, i);
		GtkWidget *tab  = notebook_get_tab (nchildren, i);

		gtk_notebook_insert_page   (GTK_NOTEBOOK (notebook), page, NULL, i);
		gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), page, tab);

		g_object_unref (G_OBJECT (page));
		g_object_unref (G_OBJECT (tab));
	}

	/* Stay on the same page */
	gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), nchildren->page);
	
	/* Free the original lists now */
	if (nchildren->children)
		g_list_free (nchildren->children);

	if (nchildren->tabs)
		g_list_free (nchildren->tabs);

	if (nchildren->children       ||
	    nchildren->tabs           ||
	    nchildren->extra_children ||
	    nchildren->extra_tabs)
		g_critical ("Unbalanced children when inserting notebook children"
			    " (pages: %d tabs: %d extra pages: %d extra tabs %d)",
			    g_list_length (nchildren->children),
			    g_list_length (nchildren->tabs),
			    g_list_length (nchildren->extra_children),
			    g_list_length (nchildren->extra_tabs));
	
	g_free (nchildren);
}

static void
glade_gtk_notebook_switch_page (GtkNotebook     *notebook,
				GtkWidget       *page,
				guint            page_num,
				gpointer         user_data)
{
	GladeWidget *gnotebook = glade_widget_get_from_gobject (notebook);

	glade_widget_property_set (gnotebook, "page", page_num);

}

/* Track project selection to set the notebook pages to display
 * the selected widget.
 */
static void
glade_gtk_notebook_selection_changed (GladeProject *project,
				      GladeWidget  *gwidget)
{
	GladeWidget *selected;
	GList       *list;
	gint         i;
	GtkWidget   *page;

	if ((list = glade_project_selection_get (project)) != NULL &&
	    g_list_length (list) == 1)
	{
		selected = glade_widget_get_from_gobject (list->data);

		/* Check if selected widget is inside the notebook */
		if (GTK_IS_WIDGET (selected->object) &&
		    gtk_widget_is_ancestor (GTK_WIDGET (selected->object),
					    GTK_WIDGET (gwidget->object)))
		{
			/* Find and activate the page */
			for (i = 0; i < gtk_notebook_get_n_pages (GTK_NOTEBOOK (gwidget->object)); i++)
			{
				page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (gwidget->object), i);
				if (GTK_WIDGET (selected->object) == page ||
				    gtk_widget_is_ancestor (GTK_WIDGET (selected->object),
							    GTK_WIDGET (page)))
				{
					glade_widget_property_set (gwidget, "page", i);
					return;
				}
			}
		}
	}
}

static void
glade_gtk_notebook_project_changed (GladeWidget *gwidget, 
				    GParamSpec  *pspec,
				    gpointer     userdata)
{
	GladeProject
		*project = glade_widget_get_project (gwidget),
		*old_project = g_object_get_data (G_OBJECT (gwidget), "notebook-project-ptr");
	
	if (old_project)
		g_signal_handlers_disconnect_by_func (G_OBJECT (old_project), 
						      G_CALLBACK (glade_gtk_notebook_selection_changed),
						      gwidget);

	if (project)
		g_signal_connect (G_OBJECT (project), "selection-changed", 
				  G_CALLBACK (glade_gtk_notebook_selection_changed), gwidget);

	g_object_set_data (G_OBJECT (gwidget), "notebook-project-ptr", project);
}

void
glade_gtk_notebook_post_create (GladeWidgetAdaptor *adaptor,
				GObject            *notebook, 
				GladeCreateReason   reason)
{
	GladeWidget *gwidget = glade_widget_get_from_gobject (notebook);

	gtk_notebook_popup_disable (GTK_NOTEBOOK (notebook));

	g_signal_connect (G_OBJECT (gwidget), "notify::project",
			  G_CALLBACK (glade_gtk_notebook_project_changed), NULL);

	glade_gtk_notebook_project_changed (gwidget, NULL, NULL);

	g_signal_connect (G_OBJECT (notebook), "switch-page",
			  G_CALLBACK (glade_gtk_notebook_switch_page), NULL);
}

static gint
glade_gtk_notebook_get_first_blank_page (GtkNotebook *notebook)
{
	GladeWidget *gwidget;
	GtkWidget   *widget;
	gint         position;

	for (position = 0; position < gtk_notebook_get_n_pages (notebook); position++)
	{
		widget = gtk_notebook_get_nth_page (notebook, position);
		if ((gwidget = glade_widget_get_from_gobject (widget)) != NULL)
		{
 			GladeProperty *property =
				glade_widget_get_property (gwidget, "position");
			gint gwidget_position = g_value_get_int (property->value);

			if ((gwidget_position - position) > 0)
				return position;
		}
	}
	return position;
}

static GladeWidget *
glade_gtk_notebook_generate_tab (GladeWidget  *notebook,
				 gint          page_id)
{
	static GladeWidgetAdaptor *wadaptor = NULL;
	gchar       *str;
	GladeWidget *glabel;

	if (wadaptor == NULL)
		wadaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_LABEL);

	glabel = glade_widget_adaptor_create_widget (wadaptor, FALSE,
						     "parent", notebook, 
						     "project", glade_widget_get_project (notebook), 
						     NULL);
	
	str = g_strdup_printf ("page %d", page_id);
	glade_widget_property_set (glabel, "label", str);
	g_free (str);
	
	g_object_set_data (glabel->object, "special-child-type", "tab");
	gtk_widget_show (GTK_WIDGET (glabel->object));

	return glabel;
}

static void
glade_gtk_notebook_set_n_pages (GObject *object, const GValue *value)
{
	GladeWidget *widget;
	GtkNotebook *notebook;
	GtkWidget   *child_widget, *tab_widget;
	gint new_size, i;
	gint old_size;

	notebook = GTK_NOTEBOOK (object);
	g_return_if_fail (GTK_IS_NOTEBOOK (notebook));

	widget = glade_widget_get_from_gobject (GTK_WIDGET (notebook));
	g_return_if_fail (widget != NULL);

	new_size = g_value_get_int (value);
	old_size = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));
	
	/* Ensure base size of notebook */
	if (glade_widget_superuser () == FALSE)
	{
		for (i = gtk_notebook_get_n_pages (notebook); i < new_size; i++)
		{
			gint position = glade_gtk_notebook_get_first_blank_page (notebook);
			GtkWidget *placeholder = glade_placeholder_new ();
			GladeWidget *gtab;

			gtk_notebook_insert_page (notebook, placeholder, NULL, position);

			/* XXX Ugly hack amongst many, this one only creates project widgets
			 * when the 'n-pages' of a notebook is initially set, otherwise it puts
			 * placeholders. (this makes the job easier when doing "insert before/after")
			 */
			if (old_size == 0 && new_size > 1)
			{
				gtab = glade_gtk_notebook_generate_tab (widget, position + 1);
				
				/* Must pass through GladeWidget api so that packing props
				 * are correctly assigned.
				 */
				glade_widget_add_child (widget, gtab, FALSE);
			}
			else
			{
				GtkWidget *tab_placeholder = glade_placeholder_new ();

				g_object_set_data (G_OBJECT (tab_placeholder), "special-child-type", "tab");

				gtk_notebook_set_tab_label (GTK_NOTEBOOK (notebook), placeholder, 
							    tab_placeholder);
			}
		}
	}

	/*
	 * Thing to remember is that GtkNotebook starts the
	 * page numbers from 0, not 1 (C-style). So we need to do
	 * old_size-1, where we're referring to "nth" widget.
	 */
	while (old_size > new_size) {
		/* Get the last page and remove it (project objects have been cleared by
		 * the action code already). */
		child_widget = gtk_notebook_get_nth_page (notebook, old_size-1);
		tab_widget   = gtk_notebook_get_tab_label (notebook, child_widget);

		/* Ok there shouldnt be widget in the content area, that's
		 * the placeholder, we should clean up the project widget that
		 * we put in the tab here though (this happens in the case where
		 * we undo increasing the "pages" property).
		 */
		if (glade_widget_get_from_gobject (child_widget))
			g_critical ("Bug in notebook_set_n_pages()");
		
		gtk_notebook_remove_page (notebook, old_size-1);

		old_size--;
	}
}

void
glade_gtk_notebook_set_property (GladeWidgetAdaptor *adaptor,
				 GObject            *object, 
				 const gchar        *id,
				 const GValue       *value)
{
	if (!strcmp (id, "pages"))
		glade_gtk_notebook_set_n_pages (object, value);
	else
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object,
								  id, value);
}

static gboolean
glade_gtk_notebook_verify_n_pages (GObject *object, const GValue *value)
{
	GtkNotebook *notebook = GTK_NOTEBOOK(object);
	GtkWidget *child_widget, *tab_widget;
	gint old_size, new_size = g_value_get_int (value);
	
	for (old_size = gtk_notebook_get_n_pages (notebook);
	     old_size > new_size; old_size--) {
		/* Get the last widget. */
		child_widget = gtk_notebook_get_nth_page (notebook, old_size-1);
		tab_widget   = gtk_notebook_get_tab_label (notebook, child_widget);

		/* 
		 * If we got it, and its not a placeholder, remove it
		 * from project.
		 */
		if (glade_widget_get_from_gobject (child_widget) ||
		    glade_widget_get_from_gobject (tab_widget))
			return FALSE;
	}
	return TRUE;
}

gboolean
glade_gtk_notebook_verify_property (GladeWidgetAdaptor *adaptor,
				    GObject            *object, 
				    const gchar        *id,
				    const GValue       *value)
{
	if (!strcmp (id, "pages"))
		return glade_gtk_notebook_verify_n_pages (object, value);
	else if (GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property)
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property (adaptor, object,
								     id, value);

	return TRUE;
}

void 
glade_gtk_notebook_add_child (GladeWidgetAdaptor *adaptor,
			      GObject            *object, 
			      GObject            *child)
{
	GtkNotebook 	*notebook;
	gint             num_page, position = 0;
	GtkWidget	*last_page;
	GladeWidget	*gwidget;
	gchar		*special_child_type;
	
	notebook = GTK_NOTEBOOK (object);

	num_page = gtk_notebook_get_n_pages (notebook);
	gwidget = glade_widget_get_from_gobject (object);
	
	/* Just append pages blindly when loading/dupping
	 */
	if (glade_widget_superuser ())
	{
		special_child_type = g_object_get_data (child, "special-child-type");
		if (special_child_type &&
		    !strcmp (special_child_type, "tab"))
		{
			last_page = gtk_notebook_get_nth_page (notebook, num_page - 1);
			gtk_notebook_set_tab_label (notebook, last_page,
						    GTK_WIDGET (child));
		}
		else
		{
			gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));
			
			glade_widget_property_set (gwidget, "pages", num_page + 1);
			
			gwidget = glade_widget_get_from_gobject (child);
			if (gwidget && gwidget->packing_properties)
				glade_widget_pack_property_set (gwidget, "position", num_page);
		}
	}
	else
	{
		NotebookChildren *nchildren;

		/* Just destroy placeholders */
		if (GLADE_IS_PLACEHOLDER (child))
			gtk_widget_destroy (GTK_WIDGET (child));
		else
		{
			gwidget = glade_widget_get_from_gobject (child);
			g_assert (gwidget);

			glade_widget_pack_property_get (gwidget, "position", &position);
			
			nchildren = glade_gtk_notebook_extract_children (GTK_WIDGET (notebook));
			
			if (g_object_get_data (child, "special-child-type") != NULL)
			{
				if (g_list_find_custom (nchildren->tabs,
							GINT_TO_POINTER (position),
							(GCompareFunc)notebook_find_child))
					nchildren->extra_tabs =
						g_list_insert_sorted
						(nchildren->extra_tabs, child,
						 (GCompareFunc)notebook_child_compare_func);
				else
					nchildren->tabs =
						g_list_insert_sorted
						(nchildren->tabs, child,
						 (GCompareFunc)notebook_child_compare_func);
			}
			else
			{
				if (g_list_find_custom (nchildren->children,
							GINT_TO_POINTER (position),
							(GCompareFunc)notebook_find_child))
					nchildren->extra_children =
						g_list_insert_sorted
						(nchildren->extra_children, child,
						 (GCompareFunc)notebook_child_compare_func);
				else
					nchildren->children =
						g_list_insert_sorted
						(nchildren->children, child,
						 (GCompareFunc)notebook_child_compare_func);
			}

			/* Takes an explicit reference when sitting on the list */
			g_object_ref (child);
			
			glade_gtk_notebook_insert_children (GTK_WIDGET (notebook), nchildren);
		}
	}
}

void
glade_gtk_notebook_remove_child (GladeWidgetAdaptor *adaptor,
				 GObject            *object, 
				 GObject            *child)
{
	NotebookChildren *nchildren;

	nchildren = glade_gtk_notebook_extract_children (GTK_WIDGET (object));
			
	if (g_list_find (nchildren->children, child))
	{
		nchildren->children =
			g_list_remove (nchildren->children, child);
		g_object_unref (child);
	}
	else if (g_list_find (nchildren->extra_children, child))
	{
		nchildren->extra_children =
			g_list_remove (nchildren->extra_children, child);
		g_object_unref (child);
	}
	else if (g_list_find (nchildren->tabs, child))
	{
		nchildren->tabs =
			g_list_remove (nchildren->tabs, child);
		g_object_unref (child);
	}
	else if (g_list_find (nchildren->extra_tabs, child))
	{
		nchildren->extra_tabs =
			g_list_remove (nchildren->extra_tabs, child);
		g_object_unref (child);
	}
	
	glade_gtk_notebook_insert_children (GTK_WIDGET (object), nchildren);
	
}

void
glade_gtk_notebook_replace_child (GladeWidgetAdaptor *adaptor,
				  GtkWidget          *container,
				  GtkWidget          *current,
				  GtkWidget          *new_widget)
{
	GtkNotebook *notebook;
	GladeWidget *gcurrent, *gnew;
	gint         position = 0;

	notebook = GTK_NOTEBOOK (container);

	if ((gcurrent = glade_widget_get_from_gobject (current)) != NULL)
		glade_widget_pack_property_get (gcurrent, "position", &position);
	else
	{
		if ((position = gtk_notebook_page_num (notebook, current)) < 0)
		{
			position = notebook_search_tab (notebook, current);
			g_assert (position >= 0);
		}
	}
	
	if (g_object_get_data (G_OBJECT (current), "special-child-type"))
		g_object_set_data (G_OBJECT (new_widget), "special-child-type", "tab");

	glade_gtk_notebook_remove_child (adaptor, 
					 G_OBJECT (container), 
					 G_OBJECT (current));

	if (GLADE_IS_PLACEHOLDER (new_widget) == FALSE)
	{
		gnew = glade_widget_get_from_gobject (new_widget);

		glade_gtk_notebook_add_child (adaptor, 
					      G_OBJECT (container), 
					      G_OBJECT (new_widget));
		
		if (glade_widget_pack_property_set (gnew, "position", position) == FALSE)
			g_critical ("No position property found on new widget");
	}
	else 
		gtk_widget_destroy (GTK_WIDGET (new_widget));
}	

gboolean
glade_gtk_notebook_child_verify_property (GladeWidgetAdaptor *adaptor,
					  GObject            *container,
					  GObject            *child,
					  const gchar        *id,
					  GValue             *value)
{
	if (!strcmp (id, "position"))
		return g_value_get_int (value) >= 0 &&
			g_value_get_int (value) < 
			gtk_notebook_get_n_pages (GTK_NOTEBOOK (container));
	else if (GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_verify_property)
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->child_verify_property (adaptor, 
								     container, child,
								     id, value);

	return TRUE;
}

void
glade_gtk_notebook_set_child_property (GladeWidgetAdaptor *adaptor,
				       GObject            *container,
				       GObject            *child,
				       const gchar        *property_name,
				       const GValue       *value)
{
	NotebookChildren *nchildren;

	if (strcmp (property_name, "position") == 0)
	{
		/* If we are setting this internally, avoid feedback. */
		if (glade_gtk_notebook_setting_position ||
		    glade_widget_superuser ())
			return;
		
		/* Just rebuild the notebook, property values are already set at this point */
		nchildren = glade_gtk_notebook_extract_children (GTK_WIDGET (container));
		glade_gtk_notebook_insert_children (GTK_WIDGET (container), nchildren);
	}
	/* packing properties are unsupported on tabs ... except "position" */
	else if (g_object_get_data (child, "special-child-type") == NULL)
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->child_set_property (adaptor, 
								  container, child,
								  property_name, value);
}

void
glade_gtk_notebook_get_child_property (GladeWidgetAdaptor *adaptor,
				       GObject            *container,
				       GObject            *child,
				       const gchar        *property_name,
				       GValue             *value)
{
	gint position;
	
	if (strcmp (property_name, "position") == 0)
	{
		if (g_object_get_data (child, "special-child-type") != NULL)
		{
			if ((position = notebook_search_tab (GTK_NOTEBOOK (container),
							     GTK_WIDGET (child))) >= 0)
				g_value_set_int (value, position);
			else
				g_value_set_int (value, 0);
		}
		else
			gtk_container_child_get_property (GTK_CONTAINER (container),
							  GTK_WIDGET (child),
							  property_name,
							  value);
	}
	/* packing properties are unsupported on tabs ... except "position" */
	else if (g_object_get_data (child, "special-child-type") == NULL)
		gtk_container_child_get_property (GTK_CONTAINER (container),
						  GTK_WIDGET (child),
						  property_name,
						  value);
}

static void
glade_gtk_box_notebook_child_insert_remove_action (GladeWidgetAdaptor *adaptor,
						   GObject *container,
						   GObject *object,
						   const gchar *size_prop,
						   const gchar *group_format,
						   gboolean remove,
						   gboolean after)
{
	GladeWidget *parent;
	GList *children, *l;
	gint child_pos, size, offset;
	
	if (GTK_IS_NOTEBOOK (container) &&
	    g_object_get_data (object, "special-child-type"))
		/* Its a Tab! */
		child_pos = notebook_search_tab (GTK_NOTEBOOK (container),
						 GTK_WIDGET (object));
	else
		gtk_container_child_get (GTK_CONTAINER (container),
					 GTK_WIDGET (object),
					 "position", &child_pos, NULL);
	
	parent = glade_widget_get_from_gobject (container);
	glade_command_push_group (group_format, glade_widget_get_name (parent));
	
	/* Make sure widgets does not get destroyed */
	children = glade_widget_adaptor_get_children (adaptor, container);
	g_list_foreach (children, (GFunc) g_object_ref, NULL);
	
	glade_widget_property_get (parent, size_prop, &size);
	
	if (remove)
	{
		GList *del = NULL;
		offset = -1;
		/* Remove children first */
		for (l = children; l; l = g_list_next (l))
		{
			GladeWidget *gchild = glade_widget_get_from_gobject (l->data);
			gint pos;
			
			/* Skip placeholders */
			if (gchild == NULL) continue;
		
			glade_widget_pack_property_get (gchild, "position", &pos);
			if (pos == child_pos) del = g_list_prepend (del, gchild);
		}
		if (del)
		{
			glade_command_delete (del);
			g_list_free (del);
		}
	}
	else
	{
		/* Expand container */
		glade_command_set_property (glade_widget_get_property (parent, size_prop),
					    size + 1);
		offset = 1;
	}
	
	/* Reoder children (fix the position property tracking widget positions) */
	for (l = g_list_last (children); l; l = g_list_previous (l))
	{
		GladeWidget *gchild = glade_widget_get_from_gobject (l->data);
		gint pos;
			
		/* Skip placeholders */
		if (gchild == NULL) continue;
		
		glade_widget_pack_property_get (gchild, "position", &pos);
		if ((after) ? pos > child_pos : pos >= child_pos)
			glade_command_set_property (glade_widget_get_pack_property (gchild, "position"),
						    pos + offset);
	}
	
	if (remove)
	{
		/* Shrink container */
		glade_command_set_property (glade_widget_get_property (parent, size_prop),
					    size - 1);
	}
	/* If it's a notebook we need to create an undoable tab now */
	else if (GTK_IS_NOTEBOOK (container))
	{
		gint       new_pos = after ? child_pos + 1 : child_pos;
		GtkWidget *new_page;
		GtkWidget *tab_placeholder;
		GladeWidget *gtab;
		GList       list = { 0, };

		new_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (container), new_pos);

		/* Deleting the project widget gives us a real placeholder now */
		new_page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (container), new_pos);
		tab_placeholder = gtk_notebook_get_tab_label (GTK_NOTEBOOK (container), new_page);
		gtab = glade_gtk_notebook_generate_tab (parent, new_pos + 1);
		list.data = gtab;

		glade_command_paste (&list, parent, GLADE_PLACEHOLDER (tab_placeholder));
	}
	
	g_list_foreach (children, (GFunc) g_object_unref, NULL);
	g_list_free (children);
	glade_command_pop_group ();
}

void
glade_gtk_notebook_child_action_activate (GladeWidgetAdaptor *adaptor,
					  GObject            *container,
					  GObject            *object,
					  const gchar        *action_path)
{
	if (strcmp (action_path, "insert_page_after") == 0)
	{
		glade_gtk_box_notebook_child_insert_remove_action (adaptor, container,
								   object, "pages",
								   _("Insert page on %s"),
								   FALSE, TRUE);
	}
	else if (strcmp (action_path, "insert_page_before") == 0)
	{
		glade_gtk_box_notebook_child_insert_remove_action (adaptor, container,
								   object, "pages",
								   _("Insert page on %s"),
								   FALSE, FALSE);
	}
	else if (strcmp (action_path, "remove_page") == 0)
	{
		glade_gtk_box_notebook_child_insert_remove_action (adaptor, container,
								   object, "pages",
								   _("Remove page from %s"),
								   TRUE, TRUE);
	}
	else
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_action_activate (adaptor,
									   container,
									   object,
									   action_path);
}

/* ----------------------------- GtkPaned ------------------------------ */
void
glade_gtk_paned_post_create (GladeWidgetAdaptor *adaptor,
			     GObject            *paned, 
			     GladeCreateReason   reason)
{
	g_return_if_fail (GTK_IS_PANED (paned));

	if (reason == GLADE_CREATE_USER && gtk_paned_get_child1 (GTK_PANED (paned)) == NULL)
		gtk_paned_add1 (GTK_PANED (paned), glade_placeholder_new ());
	
	if (reason == GLADE_CREATE_USER && gtk_paned_get_child2 (GTK_PANED (paned)) == NULL)
		gtk_paned_add2 (GTK_PANED (paned), glade_placeholder_new ());
}

void 
glade_gtk_paned_add_child (GladeWidgetAdaptor *adaptor,
			   GObject            *object, 
			   GObject            *child)
{
	GtkPaned *paned;
	GtkWidget *child1, *child2;
	gboolean loading;
	
	g_return_if_fail (GTK_IS_PANED (object));
	
	paned = GTK_PANED (object);
	loading = glade_util_object_is_loading (object);
	
	child1 = gtk_paned_get_child1 (paned);
	child2 = gtk_paned_get_child2 (paned);

	if (loading == FALSE)
	{
		/* Remove a placeholder */
		if (child1 && GLADE_IS_PLACEHOLDER (child1))
		{
			gtk_container_remove (GTK_CONTAINER (object), child1);
			child1 = NULL;
		}
		else if (child2 && GLADE_IS_PLACEHOLDER (child2))
		{
			gtk_container_remove (GTK_CONTAINER (object), child2);
			child2 = NULL;
		}
	}

	/* Add the child */
	if (child1 == NULL)
		gtk_paned_add1 (paned, GTK_WIDGET (child));
	else if (child2 == NULL)
		gtk_paned_add2 (paned, GTK_WIDGET (child));
	
	if (GLADE_IS_PLACEHOLDER (child) == FALSE && loading)
	{
		GladeWidget *gchild = glade_widget_get_from_gobject (child);
		
		if (gchild && gchild->packing_properties)
		{
			if (child1 == NULL)
				glade_widget_pack_property_set (gchild, "first", TRUE);
			else if (child2 == NULL)
				glade_widget_pack_property_set (gchild, "first", FALSE);
		}
	}
}

void 
glade_gtk_paned_remove_child (GladeWidgetAdaptor *adaptor,
			      GObject            *object,
			      GObject            *child)
{
	gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));

	glade_gtk_paned_post_create (adaptor, object, GLADE_CREATE_USER);
}

void
glade_gtk_paned_set_child_property (GladeWidgetAdaptor *adaptor,
				    GObject            *container,
				    GObject            *child,
				    const gchar        *property_name,
				    const GValue       *value)
{
	if (strcmp (property_name, "first") == 0)
	{
		GtkPaned *paned = GTK_PANED (container);
		gboolean first = g_value_get_boolean (value);
		GtkWidget *place, *wchild = GTK_WIDGET (child);
		
		place = (first) ? gtk_paned_get_child1 (paned) :
				  gtk_paned_get_child2 (paned);
		
		if (place && GLADE_IS_PLACEHOLDER (place))
			gtk_container_remove (GTK_CONTAINER (container), place);
		
		g_object_ref (child);
		gtk_container_remove (GTK_CONTAINER (container), wchild);
		if (first)
			gtk_paned_add1 (paned, wchild);
		else
			gtk_paned_add2 (paned, wchild);
		g_object_unref (child);
		
		/* Ensure placeholders */
		if (glade_util_object_is_loading (child) == FALSE)
		{
			if ((place = gtk_paned_get_child1 (paned)) == NULL)
				gtk_paned_add1 (paned, glade_placeholder_new ());
		
			if ((place = gtk_paned_get_child2 (paned)) == NULL)
				gtk_paned_add2 (paned, glade_placeholder_new ());
		}
	}
	else
		/* Chain Up */
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->child_set_property (adaptor, 
								  container, child,
								  property_name, value);
}

void
glade_gtk_paned_get_child_property (GladeWidgetAdaptor *adaptor,
				    GObject            *container,
				    GObject            *child,
				    const gchar        *property_name,
				    GValue             *value)
{
	if (strcmp (property_name, "first") == 0)
		g_value_set_boolean (value, GTK_WIDGET (child) ==
				     gtk_paned_get_child1 (GTK_PANED (container)));
	else
		/* Chain Up */
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->child_get_property (adaptor, 
								  container, child,
								  property_name, value);
}

/* ----------------------------- GtkExpander ------------------------------ */
void
glade_gtk_expander_post_create (GladeWidgetAdaptor *adaptor,
				GObject            *expander, 
				GladeCreateReason   reason)
{
	static GladeWidgetAdaptor *wadaptor = NULL;
	GladeWidget *gexpander, *glabel;
	GtkWidget *label;
	
	if (wadaptor == NULL)
		wadaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_LABEL);
	
	if (reason != GLADE_CREATE_USER) return;
	
	g_return_if_fail (GTK_IS_EXPANDER (expander));
	gexpander = glade_widget_get_from_gobject (expander);
	g_return_if_fail (GLADE_IS_WIDGET (gexpander));
	
	/* If we didnt put this object here... */
	if ((label = gtk_expander_get_label_widget (GTK_EXPANDER (expander))) == NULL ||
	    (glade_widget_get_from_gobject (label) == NULL))
	{
		glabel = glade_widget_adaptor_create_widget (wadaptor, FALSE,
							     "parent", gexpander, 
							     "project", glade_widget_get_project (gexpander), 
							     NULL);
		
		glade_widget_property_set (glabel, "label", "expander");

		g_object_set_data (glabel->object, "special-child-type", "label_item");
		glade_widget_add_child (gexpander, glabel, FALSE);
	}

	gtk_expander_set_expanded (GTK_EXPANDER (expander), TRUE);
	
	gtk_container_add (GTK_CONTAINER (expander), glade_placeholder_new ());

}

void
glade_gtk_expander_replace_child (GladeWidgetAdaptor *adaptor,
				  GtkWidget          *container,
				  GtkWidget          *current,
				  GtkWidget          *new_widget)
{
	gchar *special_child_type;

	special_child_type =
		g_object_get_data (G_OBJECT (current), "special-child-type");

	if (special_child_type && !strcmp (special_child_type, "label_item"))
	{
		g_object_set_data (G_OBJECT (new_widget), "special-child-type", "label_item");
		gtk_expander_set_label_widget (GTK_EXPANDER (container), new_widget);
		return;
	}

	/* Chain Up */
	GWA_GET_CLASS
		(GTK_TYPE_CONTAINER)->replace_child (adaptor, 
						     G_OBJECT (container), 
						     G_OBJECT (current),
						     G_OBJECT (new_widget));
}


void
glade_gtk_expander_add_child (GladeWidgetAdaptor *adaptor,
			      GObject            *object, 
			      GObject            *child)
{
	gchar *special_child_type;

	special_child_type = g_object_get_data (child, "special-child-type");
	
	if (special_child_type &&
	    !strcmp (special_child_type, "label"))
	{
		g_object_set_data (child,
				   "special-child-type",
				   "label_item");
		gtk_expander_set_label_widget (GTK_EXPANDER (object),
					       GTK_WIDGET (child));
	}
	else if (special_child_type &&
		 !strcmp (special_child_type, "label_item"))
	{
		gtk_expander_set_label_widget (GTK_EXPANDER (object),
					       GTK_WIDGET (child));
	}
	else
		/* Chain Up */
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->add (adaptor, object, child); 
}

void
glade_gtk_expander_remove_child (GladeWidgetAdaptor *adaptor,
				 GObject            *object, 
				 GObject            *child)
{
	gchar *special_child_type;

	special_child_type = g_object_get_data (child, "special-child-type");
	if (special_child_type &&
	    !strcmp (special_child_type, "label_item"))
	{
		gtk_expander_set_label_widget (GTK_EXPANDER (object),
					       glade_placeholder_new ());
	}
	else
	{
		gtk_container_remove (GTK_CONTAINER (object),
				      GTK_WIDGET (child));
		gtk_container_add (GTK_CONTAINER (object),
				   glade_placeholder_new ());
	}
}

void
glade_gtk_expander_write_child (GladeWidgetAdaptor *adaptor,
			        GladeWidget        *widget,
			        GladeXmlContext    *context,
			        GladeXmlNode       *node)
{

	if (!write_special_child_label_item (adaptor, widget, context, node,
					     GWA_GET_CLASS(GTK_TYPE_CONTAINER)->write_child))
		/* Chain Up */
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->write_child (adaptor,
							   widget,
							   context,
							   node);
}


/* -------------------------------- GtkEntry -------------------------------- */


static void
glade_gtk_entry_changed (GtkEditable *editable, GladeWidget *gentry)
{
	const gchar *text, *text_prop;
	GladeProperty *prop;
	gboolean use_buffer;	

	if (glade_widget_superuser ())
		return;

	text = gtk_entry_get_text (GTK_ENTRY (editable));
	
	glade_widget_property_get (gentry, "text", &text_prop);
	glade_widget_property_get (gentry, "use-entry-buffer", &use_buffer);
	
	if (use_buffer == FALSE && g_strcmp0 (text, text_prop))
	{
		if ((prop = glade_widget_get_property (gentry, "text")))
			glade_command_set_property (prop, text);
	}
}

void
glade_gtk_entry_post_create (GladeWidgetAdaptor *adaptor,
			     GObject            *object,
			     GladeCreateReason   reason)
{
	GladeWidget *gentry;
	
	g_return_if_fail (GTK_IS_ENTRY (object));
	gentry = glade_widget_get_from_gobject (object);
	g_return_if_fail (GLADE_IS_WIDGET (gentry));
	
	g_signal_connect (object, "changed",
			  G_CALLBACK (glade_gtk_entry_changed), gentry);
}

GladeEditable *
glade_gtk_entry_create_editable (GladeWidgetAdaptor  *adaptor,
				 GladeEditorPageType  type)
{
	GladeEditable *editable;

	/* Get base editable */
	editable = GWA_GET_CLASS (GTK_TYPE_WIDGET)->create_editable (adaptor, type);

	if (type == GLADE_PAGE_GENERAL)
		return (GladeEditable *)glade_entry_editor_new (adaptor, editable);

	return editable;
}


void
glade_gtk_entry_set_property (GladeWidgetAdaptor *adaptor,
			      GObject            *object, 
			      const gchar        *id,
			      const GValue       *value)
{
	GladeImageEditMode mode;
	GladeWidget *gwidget = glade_widget_get_from_gobject (object);
	GladeProperty *property = glade_widget_get_property (gwidget, id);

	if (!strcmp (id, "use-entry-buffer"))
	{
		glade_widget_property_set_sensitive (gwidget, "text", FALSE, NOT_SELECTED_MSG);
		glade_widget_property_set_sensitive (gwidget, "buffer", FALSE, NOT_SELECTED_MSG);

		if (g_value_get_boolean (value))
			glade_widget_property_set_sensitive (gwidget, "buffer", TRUE, NULL);
		else
			glade_widget_property_set_sensitive (gwidget, "text", TRUE, NULL);
	}
	else if (!strcmp (id, "primary-icon-mode"))
	{
		mode = g_value_get_int (value);

		glade_widget_property_set_sensitive (gwidget, "primary-icon-stock", FALSE, NOT_SELECTED_MSG);
		glade_widget_property_set_sensitive (gwidget, "primary-icon-name", FALSE, NOT_SELECTED_MSG);
		glade_widget_property_set_sensitive (gwidget, "primary-icon-pixbuf", FALSE, NOT_SELECTED_MSG);

		switch (mode) {
		case GLADE_IMAGE_MODE_STOCK:
			glade_widget_property_set_sensitive (gwidget, "primary-icon-stock", TRUE, NULL);
			break;
		case GLADE_IMAGE_MODE_ICON:	
			glade_widget_property_set_sensitive (gwidget, "primary-icon-name", TRUE, NULL);
			break;
		case GLADE_IMAGE_MODE_FILENAME: 
			glade_widget_property_set_sensitive (gwidget, "primary-icon-pixbuf", TRUE, NULL);
			break;
		}
	}
	else if (!strcmp (id, "secondary-icon-mode"))
	{
		mode = g_value_get_int (value);

		glade_widget_property_set_sensitive (gwidget, "secondary-icon-stock", FALSE, NOT_SELECTED_MSG);
		glade_widget_property_set_sensitive (gwidget, "secondary-icon-name", FALSE, NOT_SELECTED_MSG);
		glade_widget_property_set_sensitive (gwidget, "secondary-icon-pixbuf", FALSE, NOT_SELECTED_MSG);

		switch (mode) {
		case GLADE_IMAGE_MODE_STOCK:
			glade_widget_property_set_sensitive (gwidget, "secondary-icon-stock", TRUE, NULL);
			break;
		case GLADE_IMAGE_MODE_ICON:	
			glade_widget_property_set_sensitive (gwidget, "secondary-icon-name", TRUE, NULL);
			break;
		case GLADE_IMAGE_MODE_FILENAME: 
			glade_widget_property_set_sensitive (gwidget, "secondary-icon-pixbuf", TRUE, NULL);
			break;
		}
	}
	else if (!strcmp (id, "primary-icon-tooltip-text") ||
		 !strcmp (id, "primary-icon-tooltip-markup"))
	{
		/* Avoid a silly crash in GTK+ */
		if (gtk_entry_get_icon_storage_type (GTK_ENTRY (object),
						     GTK_ENTRY_ICON_PRIMARY) != GTK_IMAGE_EMPTY)
			GWA_GET_CLASS (GTK_TYPE_WIDGET)->set_property (adaptor, object, id, value);
	}
	else if (!strcmp (id, "secondary-icon-tooltip-text") ||
		 !strcmp (id, "secondary-icon-tooltip-markup"))
	{
		/* Avoid a silly crash in GTK+ */
		if (gtk_entry_get_icon_storage_type (GTK_ENTRY (object),
						     GTK_ENTRY_ICON_SECONDARY) != GTK_IMAGE_EMPTY)
			GWA_GET_CLASS (GTK_TYPE_WIDGET)->set_property (adaptor, object, id, value);
	}
	else if (!strcmp (id, "text"))
	{
		g_signal_handlers_block_by_func (object, glade_gtk_entry_changed, gwidget);

		if (g_value_get_string (value))
			gtk_entry_set_text (GTK_ENTRY (object), g_value_get_string (value));
		else
			gtk_entry_set_text (GTK_ENTRY (object), "");

		g_signal_handlers_unblock_by_func (object, glade_gtk_entry_changed, gwidget);

       	}
	else if (GPC_VERSION_CHECK (property->klass, gtk_major_version, gtk_minor_version + 1))
		GWA_GET_CLASS (GTK_TYPE_WIDGET)->set_property (adaptor, object, id, value);
}

void
glade_gtk_entry_read_widget (GladeWidgetAdaptor *adaptor,
			     GladeWidget        *widget,
			     GladeXmlNode       *node)
{
	GladeProperty *property;

	if (!glade_xml_node_verify 
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and read in all the normal properties.. */
        GWA_GET_CLASS (GTK_TYPE_WIDGET)->read_widget (adaptor, widget, node);

	if (glade_widget_property_original_default (widget, "text") == FALSE)
	{
		property = glade_widget_get_property (widget, "text");
		glade_widget_property_set (widget, "use-entry-buffer", FALSE);

		glade_property_sync (property);
	}
	else
	{
		gint target_minor, target_major;

		glade_project_get_target_version (widget->project, "gtk+", &target_major, &target_minor);

		property = glade_widget_get_property (widget, "buffer");

		/* Only default to the buffer setting if the project version supports it. */
		if (GPC_VERSION_CHECK (property->klass, target_major, target_minor))
		{
			glade_widget_property_set (widget, "use-entry-buffer", TRUE);
			glade_property_sync (property);
		}
		else
			glade_widget_property_set (widget, "use-entry-buffer", FALSE);
	}

	if (glade_widget_property_original_default (widget, "primary-icon-name") == FALSE)
	{
		property = glade_widget_get_property (widget, "primary-icon-name");
		glade_widget_property_set (widget, "primary-icon-mode", GLADE_IMAGE_MODE_ICON);
	}
	else if (glade_widget_property_original_default (widget, "primary-icon-pixbuf") == FALSE)
	{
		property = glade_widget_get_property (widget, "primary-icon-pixbuf");
		glade_widget_property_set (widget, "primary-icon-mode", GLADE_IMAGE_MODE_FILENAME);
	}
	else/*  if (glade_widget_property_original_default (widget, "stock") == FALSE) */
	{
		property = glade_widget_get_property (widget, "primary-icon-stock");
		glade_widget_property_set (widget, "primary-icon-mode", GLADE_IMAGE_MODE_STOCK);
	}

	glade_property_sync (property);

	if (glade_widget_property_original_default (widget, "secondary-icon-name") == FALSE)
	{
		property = glade_widget_get_property (widget, "secondary-icon-name");
		glade_widget_property_set (widget, "secondary-icon-mode", GLADE_IMAGE_MODE_ICON);
	}
	else if (glade_widget_property_original_default (widget, "secondary-icon-pixbuf") == FALSE)
	{
		property = glade_widget_get_property (widget, "secondary-icon-pixbuf");
		glade_widget_property_set (widget, "secondary-icon-mode", GLADE_IMAGE_MODE_FILENAME);
	}
	else/*  if (glade_widget_property_original_default (widget, "stock") == FALSE) */
	{
		property = glade_widget_get_property (widget, "secondary-icon-stock");
		glade_widget_property_set (widget, "secondary-icon-mode", GLADE_IMAGE_MODE_STOCK);
	}

	glade_property_sync (property);
}


/* ----------------------------- GtkFixed/GtkLayout ------------------------------ */
static void
glade_gtk_fixed_layout_finalize(GdkPixmap *backing)
{
	g_object_unref(backing);
}

static void
glade_gtk_fixed_layout_realize (GtkWidget *widget)
{
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_xpm_data (fixed_bg_xpm);
	GdkPixmap *backing;

	gdk_pixbuf_render_pixmap_and_mask (pixbuf, &backing, NULL, 1);
	
	if (GTK_IS_LAYOUT (widget))
		gdk_window_set_back_pixmap (gtk_layout_get_bin_window (GTK_LAYOUT (widget)),
					    backing, FALSE);
	else
		gdk_window_set_back_pixmap (gtk_widget_get_window (widget), backing, FALSE);


	/* For cleanup later
	 */
	g_object_weak_ref(G_OBJECT(widget), 
			  (GWeakNotify)glade_gtk_fixed_layout_finalize, backing);
}

static void
glade_gtk_fixed_layout_sync_size_requests (GtkWidget   *widget)
{
	GList *children, *l;

	if ((children = gtk_container_get_children (GTK_CONTAINER (widget))) != NULL)
       	{
		for (l = children; l; l = l->next)
	       	{
			GtkWidget *child = l->data;
			GladeWidget *gchild = glade_widget_get_from_gobject (child);
			gint width = -1, height = -1;

			if (!gchild)
				continue;

			glade_widget_property_get (gchild, "width-request", &width);
			glade_widget_property_get (gchild, "height-request", &height);
	
			gtk_widget_set_size_request (child, width, height);
			
		}
		g_list_free (children);
	}
}

void
glade_gtk_fixed_layout_post_create (GladeWidgetAdaptor *adaptor,
				    GObject            *object, 
				    GladeCreateReason   reason)
{
	/* This is needed at least to set a backing pixmap. */
	gtk_widget_set_has_window (GTK_WIDGET (object), FALSE);

	/* For backing pixmap
	 */
	g_signal_connect_after(object, "realize",
			       G_CALLBACK(glade_gtk_fixed_layout_realize), NULL);


	/* Sync up size request at project load time */
	if (reason == GLADE_CREATE_LOAD)
		g_signal_connect_after(object, "realize",
				       G_CALLBACK(glade_gtk_fixed_layout_sync_size_requests), NULL);

}

void
glade_gtk_fixed_layout_add_child (GladeWidgetAdaptor  *adaptor,
				  GObject             *object, 
				  GObject             *child)
{
	g_return_if_fail (GTK_IS_CONTAINER (object));
	g_return_if_fail (GTK_IS_WIDGET (child));

	gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));
}

void
glade_gtk_fixed_layout_remove_child (GladeWidgetAdaptor  *adaptor,
				     GObject             *object, 
				     GObject             *child)
{
	g_return_if_fail (GTK_IS_CONTAINER (object));
	g_return_if_fail (GTK_IS_WIDGET (child));

	gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
}

/* ----------------------------- GtkWindow ------------------------------ */
static gint
glade_gtk_widget_show_on_delete (GtkWidget *widget,
				 gpointer   user_data)
{
	gtk_widget_show (widget);
	return TRUE;
}

void
glade_gtk_window_deep_post_create (GladeWidgetAdaptor *adaptor,
				   GObject            *object,
				   GladeCreateReason   reason)
{
	GtkWindow *window = GTK_WINDOW (object);

	g_return_if_fail (GTK_IS_WINDOW (window));

	/* Chain her up first */
	GWA_GET_CLASS (GTK_TYPE_CONTAINER)->deep_post_create (adaptor, object, reason);

	g_signal_connect (object, "delete-event", G_CALLBACK (glade_gtk_widget_show_on_delete), NULL);
}

static void
glade_gtk_window_read_accel_groups (GladeWidget  *widget,
				    GladeXmlNode *node)
{
	GladeXmlNode  *groups_node;
	GladeProperty *property;
	gchar         *string = NULL;

	if ((groups_node = 
	     glade_xml_search_child (node, GLADE_TAG_ACCEL_GROUPS)) != NULL)
	{
		GladeXmlNode  *node;

		for (node = glade_xml_node_get_children (groups_node); 
		     node; node = glade_xml_node_next (node))
		{
			gchar *group_name, *tmp;
			
			if (!glade_xml_node_verify (node, GLADE_TAG_ACCEL_GROUP))
				continue;

			group_name = glade_xml_get_property_string_required
				(node, GLADE_TAG_NAME, NULL);
				
			if (string == NULL)
				string = group_name;
			else if (group_name != NULL)
			{
				tmp = g_strdup_printf ("%s%s%s", string, GPC_OBJECT_DELIMITER, group_name);
				string = (g_free (string), tmp);
				g_free (group_name);
			}
		}
	}

	if (string)
	{
		property = glade_widget_get_property (widget, "accel-groups");
		g_assert (property);

		/* we must synchronize this directly after loading this project
		 * (i.e. lookup the actual objects after they've been parsed and
		 * are present).
		 */
		g_object_set_data_full (G_OBJECT (property), 
					"glade-loaded-object", 
					string, g_free);
	}
}

void
glade_gtk_window_read_widget (GladeWidgetAdaptor *adaptor,
			      GladeWidget        *widget,
			      GladeXmlNode       *node)
{
	if (!glade_xml_node_verify 
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and read in all the normal properties.. */
        GWA_GET_CLASS (GTK_TYPE_WIDGET)->read_widget (adaptor, widget, node);

	glade_gtk_window_read_accel_groups (widget, node);
}


static void
glade_gtk_window_write_accel_groups (GladeWidget        *widget,
				     GladeXmlContext    *context,
				     GladeXmlNode       *node)
{
	GladeXmlNode  *groups_node, *group_node;
	GList         *groups = NULL, *list;
	GladeWidget   *agroup;

	groups_node = glade_xml_node_new (context, GLADE_TAG_ACCEL_GROUPS);

	if (glade_widget_property_get (widget, "accel-groups", &groups))
	{
		for (list = groups; list; list = list->next)
		{
			agroup     = glade_widget_get_from_gobject (list->data);
			group_node = glade_xml_node_new (context, GLADE_TAG_ACCEL_GROUP);
			glade_xml_node_append_child (groups_node, group_node);
			glade_xml_node_set_property_string (group_node, GLADE_TAG_NAME, agroup->name);
		}
	}

	if (!glade_xml_node_get_children (groups_node))
		glade_xml_node_delete (groups_node);
	else
		glade_xml_node_append_child (node, groups_node);
	
}


void
glade_gtk_window_write_widget (GladeWidgetAdaptor *adaptor,
			       GladeWidget        *widget,
			       GladeXmlContext    *context,
			       GladeXmlNode       *node)
{
	if (!glade_xml_node_verify
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and read in all the normal properties.. */
        GWA_GET_CLASS (GTK_TYPE_WIDGET)->write_widget (adaptor, widget, context, node);

	glade_gtk_window_write_accel_groups (widget, context, node);
}


/* ----------------------------- GtkDialog(s) ------------------------------ */
static void
glade_gtk_dialog_stop_offending_signals (GtkWidget *widget)
{
	static gpointer hierarchy = NULL, screen;
	
	if (hierarchy == NULL)
	{
		hierarchy = GUINT_TO_POINTER (g_signal_lookup ("hierarchy-changed",
							       GTK_TYPE_WIDGET));
		screen = GUINT_TO_POINTER (g_signal_lookup ("screen-changed",
							    GTK_TYPE_WIDGET));
	}
	
	g_signal_connect (widget, "hierarchy-changed",
			  G_CALLBACK (glade_gtk_stop_emission_POINTER),
			  hierarchy);
	g_signal_connect (widget, "screen-changed",
			  G_CALLBACK (glade_gtk_stop_emission_POINTER),
			  screen);
}

static void
glade_gtk_file_chooser_default_forall (GtkWidget *widget, gpointer data)
{	
	/* Since GtkFileChooserDefault is not exposed we check if its a
	 * GtkFileChooser
	 */
	if (GTK_IS_FILE_CHOOSER (widget))
	{
		
		/* Finally we can connect to the signals we want to stop its
		 * default handler. Since both signals has the same signature
		 * we use one callback for both :)
		 */
		glade_gtk_dialog_stop_offending_signals (widget);
	}
}

static void
glade_gtk_file_chooser_forall (GtkWidget *widget, gpointer data)
{
	/* GtkFileChooserWidget packs a GtkFileChooserDefault */
	if (GTK_IS_FILE_CHOOSER_WIDGET (widget))
		gtk_container_forall (GTK_CONTAINER (widget),
				      glade_gtk_file_chooser_default_forall,
				      NULL);
}

static void
glade_gtk_input_dialog_forall (GtkWidget *widget, gpointer data)
{
	/* Make every option menu insensitive, yes it use a deprecated widget */
	if (GTK_IS_OPTION_MENU (widget))
		gtk_widget_set_sensitive (widget, FALSE);
	else if (GTK_IS_CONTAINER (widget))
		gtk_container_forall (GTK_CONTAINER (widget),
				      glade_gtk_input_dialog_forall,
				      NULL);
}

void 
glade_gtk_dialog_post_create (GladeWidgetAdaptor *adaptor,
			      GObject            *object, 
			      GladeCreateReason   reason)
{
	GtkDialog    *dialog = GTK_DIALOG (object);
	GladeWidget  *widget;
	GladeWidget  *vbox_widget, *actionarea_widget, *colorsel, *fontsel;
	GladeWidget  *save_button = NULL, *close_button = NULL, *ok_button = NULL,
		*cancel_button = NULL, *help_button = NULL, *apply_button = NULL;
	
	g_return_if_fail (GTK_IS_DIALOG (dialog));

	widget = glade_widget_get_from_gobject (GTK_WIDGET (dialog));
	if (!widget)
		return;

	if (reason == GLADE_CREATE_USER)
	{
		/* HIG complient border-width defaults on dialogs */
		glade_widget_property_set (widget, "border-width", 5);
	}

	if (GTK_IS_INPUT_DIALOG (object))
	{
		GtkInputDialog *id = GTK_INPUT_DIALOG (dialog);

		save_button = glade_widget_adaptor_create_internal
			(widget, G_OBJECT (id->save_button),
			 "save_button", "inputdialog", FALSE, reason);
		close_button = glade_widget_adaptor_create_internal
			(widget, G_OBJECT (id->close_button),
			 "close_button", "inputdialog", FALSE, reason);
		/*
		  On device and mode menu items "activate" signal handlers 
		  GtkInputDialog call gtk_widget_get_toplevel() and assume that
		  the toplevel returned is the GtkInputDialog but since the 
		  dialog is embed inside glade the returned pointer is not what
		  expected and this gives a segfault :S
		*/
		gtk_container_forall (GTK_CONTAINER (dialog),
				      glade_gtk_input_dialog_forall,
				      NULL);
		
		glade_gtk_dialog_stop_offending_signals (GTK_WIDGET (dialog));
	}
	else if (GTK_IS_FILE_SELECTION (object))
	{
		ok_button = glade_widget_adaptor_create_internal
			(widget, G_OBJECT (GTK_FILE_SELECTION (object)->ok_button),
			 "ok_button", "filesel", FALSE, reason);

		cancel_button = glade_widget_adaptor_create_internal
			(widget, G_OBJECT (GTK_FILE_SELECTION (object)->cancel_button),
			 "cancel_button", "filesel", FALSE, reason);
	}
	else if (GTK_IS_COLOR_SELECTION_DIALOG (object))
	{
		GtkWidget *child;

		child = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_OK);
		ok_button = glade_widget_adaptor_create_internal
			(widget, G_OBJECT (child),
			 "ok_button", "colorsel", FALSE, reason);

		child = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_CANCEL);
		cancel_button = glade_widget_adaptor_create_internal
			(widget, G_OBJECT (child),
			 "cancel_button", "colorsel", FALSE, reason);

		child = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_HELP);
		help_button = glade_widget_adaptor_create_internal
			(widget, G_OBJECT (child),
			 "help_button", "colorsel", FALSE, reason);

		child = gtk_color_selection_dialog_get_color_selection (GTK_COLOR_SELECTION_DIALOG (dialog));
		colorsel = glade_widget_adaptor_create_internal
			(widget, G_OBJECT (child),
			 "color_selection", "colorsel", FALSE, reason);
		
		/* Set this to 1 at load time, if there are any children then
		 * size will adjust appropriately (otherwise the default "3" gets
		 * set and we end up with extra placeholders).
		 */
		if (reason == GLADE_CREATE_LOAD)
			glade_widget_property_set (colorsel, "size", 1);

	}
	else if (GTK_IS_FONT_SELECTION_DIALOG (object))
	{
		GtkWidget *child;

		child = gtk_font_selection_dialog_get_ok_button (GTK_FONT_SELECTION_DIALOG (dialog));
		ok_button = glade_widget_adaptor_create_internal
			(widget, G_OBJECT (child),
			 "ok_button", "fontsel", FALSE, reason);

		child = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_APPLY);
		apply_button = glade_widget_adaptor_create_internal
			(widget, G_OBJECT (child),
			 "apply_button", "fontsel", FALSE, reason);
		
		child = gtk_font_selection_dialog_get_cancel_button (GTK_FONT_SELECTION_DIALOG (dialog));
		cancel_button = glade_widget_adaptor_create_internal
			(widget, G_OBJECT (child),
			 "cancel_button", "fontsel", FALSE, reason);

#if GTK_CHECK_VERSION (2, 24, 0)
		child = gtk_font_selection_dialog_get_font_selection
			(GTK_FONT_SELECTION_DIALOG (dialog));
#else
		child = GTK_FONT_SELECTION_DIALOG (dialog)->fontsel;
#endif
		fontsel =  glade_widget_adaptor_create_internal
			(widget, G_OBJECT (child),
			 "font_selection", "fontsel", FALSE, reason);

		/* Set this to 1 at load time, if there are any children then
		 * size will adjust appropriately (otherwise the default "3" gets
		 * set and we end up with extra placeholders).
		 */
		if (reason == GLADE_CREATE_LOAD)
			glade_widget_property_set (fontsel, "size", 2);
	}
	else
	{
		/* We need to stop default emissions of "hierarchy-changed" and 
		 * "screen-changed" of GtkFileChooserDefault to avoid an abort()
		 * when doing a reparent.
		 * GtkFileChooserDialog packs a GtkFileChooserWidget in 
		 * his internal vbox.
		 */
		if (GTK_IS_FILE_CHOOSER_DIALOG (object))
			gtk_container_forall (GTK_CONTAINER (gtk_dialog_get_content_area (dialog)),
					      glade_gtk_file_chooser_forall,
					      NULL);
		
		vbox_widget = glade_widget_adaptor_create_internal 
			(widget, G_OBJECT(gtk_dialog_get_content_area (dialog)),
			 "vbox", "dialog", FALSE, reason);

		actionarea_widget = glade_widget_adaptor_create_internal
			(vbox_widget, G_OBJECT(gtk_dialog_get_action_area (dialog)),
			 "action_area", "dialog", FALSE, reason);

		/* These properties are controlled by the GtkDialog style properties:
		 * "content-area-border", "button-spacing" and "action-area-border",
		 * so we must disable thier use.
		 */
		glade_widget_remove_property (vbox_widget, "border-width");
		glade_widget_remove_property (actionarea_widget, "border-width");
		glade_widget_remove_property (actionarea_widget, "spacing");

		/* Only set these on the original create. */
		if (reason == GLADE_CREATE_USER)
		{

			/* HIG complient spacing defaults on dialogs */
			glade_widget_property_set (vbox_widget, "spacing", 2);

			if (GTK_IS_MESSAGE_DIALOG (object))
				glade_widget_property_set (vbox_widget, "size", 2);
			else if (GTK_IS_ABOUT_DIALOG (object))
				glade_widget_property_set (vbox_widget, "size", 3);
			else if (GTK_IS_FILE_CHOOSER_DIALOG (object))
				glade_widget_property_set (vbox_widget, "size", 3);
			else		
				glade_widget_property_set (vbox_widget, "size", 2);

			glade_widget_property_set (actionarea_widget, "size", 2);
			glade_widget_property_set (actionarea_widget, "layout-style", GTK_BUTTONBOX_END);
		}
	}
}

GtkWidget *
glade_gtk_dialog_get_internal_child (GladeWidgetAdaptor  *adaptor,
				     GtkDialog           *dialog,
				     const gchar         *name)
{
	GtkWidget *child = NULL;

	g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);

	if (GTK_IS_INPUT_DIALOG (dialog))
	{
		if (strcmp ("close_button", name) == 0)
			child = GTK_INPUT_DIALOG (dialog)->close_button;
		else if (strcmp ("save_button", name) == 0)
			child = GTK_INPUT_DIALOG (dialog)->save_button;
	}
	else if (GTK_IS_FILE_SELECTION (dialog))
	{
		if (strcmp ("ok_button", name) == 0)
			child = GTK_FILE_SELECTION (dialog)->ok_button;
		else if (strcmp ("cancel_button", name) == 0)
			child = GTK_FILE_SELECTION (dialog)->cancel_button;
	}
	else if (GTK_IS_COLOR_SELECTION_DIALOG (dialog))
	{
		if (strcmp ("ok_button", name) == 0)
			child = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_OK);
		else if (strcmp ("cancel_button", name) == 0)
			child = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_CANCEL);
		else if (strcmp ("help_button", name) == 0)
			child = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_HELP);
		else if (strcmp ("color_selection", name) == 0)
			child = gtk_color_selection_dialog_get_color_selection
				(GTK_COLOR_SELECTION_DIALOG (dialog));
	}
	else if (GTK_IS_FONT_SELECTION_DIALOG (dialog))
	{

		if (strcmp ("ok_button", name) == 0)
			child = gtk_font_selection_dialog_get_ok_button (GTK_FONT_SELECTION_DIALOG (dialog));
		else if (strcmp ("apply_button", name) == 0)
			child = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_APPLY);
		else if (strcmp ("cancel_button", name) == 0)
			child = gtk_font_selection_dialog_get_cancel_button
				(GTK_FONT_SELECTION_DIALOG (dialog));
		else if (strcmp ("font_selection", name) == 0)
		{
#if GTK_CHECK_VERSION (2, 24, 0)
			child = gtk_font_selection_dialog_get_font_selection
				(GTK_FONT_SELECTION_DIALOG (dialog));
#else
			child = GTK_FONT_SELECTION_DIALOG (dialog)->fontsel;
#endif
		}
	}
	else
	{
		/* Default generic dialog handling
		 */
		if (strcmp ("vbox", name) == 0)
			child = gtk_dialog_get_content_area (dialog);
		else if (strcmp ("action_area", name) == 0)
			child = gtk_dialog_get_action_area (dialog);
	}

	return child;
}

GList *
glade_gtk_dialog_get_children (GladeWidgetAdaptor  *adaptor,
			       GtkDialog           *dialog)
{
	GList *list = NULL;

	g_return_val_if_fail (GTK_IS_DIALOG (dialog), NULL);

	list = glade_util_container_get_all_children (GTK_CONTAINER (dialog));

	if (GTK_IS_INPUT_DIALOG (dialog))
	{
		list = g_list_prepend (list, GTK_INPUT_DIALOG (dialog)->close_button);
		list = g_list_prepend (list, GTK_INPUT_DIALOG (dialog)->save_button);
	}
	else if (GTK_IS_FILE_SELECTION (dialog))
	{
		list = g_list_prepend (list, GTK_FILE_SELECTION (dialog)->ok_button);
		list = g_list_prepend (list, GTK_FILE_SELECTION (dialog)->cancel_button);
	}
	else if (GTK_IS_COLOR_SELECTION_DIALOG (dialog))
	{
		GtkWidget *widget;
		
		widget = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_OK);
		if (widget) list = g_list_prepend (list, widget);

		widget = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_CANCEL);
		if (widget) list = g_list_prepend (list, widget);

		widget = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_HELP);
		if (widget) list = g_list_prepend (list, widget);

		widget = gtk_color_selection_dialog_get_color_selection (GTK_COLOR_SELECTION_DIALOG (dialog));
		if (widget) list = g_list_prepend (list, widget);
	}
	else if (GTK_IS_FONT_SELECTION_DIALOG (dialog))
	{
		GtkWidget *widget;

		widget = gtk_font_selection_dialog_get_ok_button (GTK_FONT_SELECTION_DIALOG (dialog));
		if (widget) list = g_list_prepend (list, widget);

		widget = gtk_dialog_get_widget_for_response (dialog, GTK_RESPONSE_APPLY);
		if (widget) list = g_list_prepend (list, widget);

		widget = gtk_font_selection_dialog_get_cancel_button (GTK_FONT_SELECTION_DIALOG (dialog));
		if (widget) list = g_list_prepend (list, widget);

#if GTK_CHECK_VERSION (2, 24, 0)
		widget = gtk_font_selection_dialog_get_font_selection (GTK_FONT_SELECTION_DIALOG (dialog));
#else
		widget = GTK_FONT_SELECTION_DIALOG (dialog)->fontsel;
#endif

		if (widget) list = g_list_prepend (list, widget);
	}
	return list;
}


#define GLADE_TAG_ACTION_WIDGETS "action-widgets"
#define GLADE_TAG_ACTION_WIDGET  "action-widget"
#define GLADE_TAG_RESPONSE       "response"


static void
glade_gtk_dialog_read_responses (GladeWidget  *widget, 
				 GladeXmlNode *widgets_node)
{
	GladeXmlNode *node;
	GladeWidget  *action_widget;

	for (node = glade_xml_node_get_children (widgets_node); 
	     node; node = glade_xml_node_next (node))
	{
		gchar *widget_name, *response;
			
		if (!glade_xml_node_verify (node, GLADE_TAG_ACTION_WIDGET))
			continue;

		response    = glade_xml_get_property_string_required (node, GLADE_TAG_RESPONSE, NULL);
		widget_name = glade_xml_get_content (node);
				
		if ((action_widget = 
		     glade_project_get_widget_by_name (widget->project, widget, widget_name)) != NULL)
		{
			glade_widget_property_set (action_widget, "response-id", 
						   g_ascii_strtoll (response, NULL, 10));
		}

		g_free (response);
		g_free (widget_name);
	}
}

void
glade_gtk_dialog_read_child (GladeWidgetAdaptor *adaptor,
			     GladeWidget        *widget,
			     GladeXmlNode       *node)
{
	GladeXmlNode *widgets_node;
	GladeProject *project;

	GWA_GET_CLASS (GTK_TYPE_CONTAINER)->read_child (adaptor, widget, node);

	project = widget->project;

	if (glade_project_get_format (project) != GLADE_PROJECT_FORMAT_GTKBUILDER)
		return;

	node = glade_xml_node_get_parent (node);

	if ((widgets_node = glade_xml_search_child (node, GLADE_TAG_ACTION_WIDGETS)) != NULL)
		glade_gtk_dialog_read_responses (widget, widgets_node);
}


static void
glade_gtk_dialog_write_responses (GladeWidget     *widget,
				  GladeXmlContext *context,
				  GladeXmlNode    *node)
{
	GladeXmlNode *widget_node;
	GtkDialog    *dialog  = GTK_DIALOG (widget->object);
	GList        *l, *action_widgets = 
		gtk_container_get_children (GTK_CONTAINER (gtk_dialog_get_action_area (dialog)));

	for (l = action_widgets; l; l = l->next)
	{
		GladeWidget   *action_widget;
		GladeProperty *property;
		gchar         *str;

		if ((action_widget = glade_widget_get_from_gobject (l->data)) == NULL)
			continue;

		if ((property = glade_widget_get_property (action_widget, "response-id")) == NULL)
			continue;
	       
		widget_node = glade_xml_node_new (context, GLADE_TAG_ACTION_WIDGET);
		glade_xml_node_append_child (node, widget_node);

		str = glade_property_class_make_string_from_gvalue (property->klass, property->value, 
								    GLADE_PROJECT_FORMAT_GTKBUILDER);

		glade_xml_node_set_property_string (widget_node, GLADE_TAG_RESPONSE, str);
		glade_xml_set_content (widget_node, action_widget->name);

		g_free (str);
	}


	g_list_free (action_widgets);
}

void
glade_gtk_dialog_write_child (GladeWidgetAdaptor *adaptor,
			      GladeWidget        *widget,
			      GladeXmlContext    *context,
			      GladeXmlNode       *node)
{
	GladeXmlNode *widgets_node;
	GladeWidget  *parent;
	GladeProject *project;

	GWA_GET_CLASS (GTK_TYPE_CONTAINER)->write_child (adaptor, widget, context, node);

	parent = widget->parent;
	project = widget->project;

	if (parent && GTK_IS_DIALOG (parent->object) &&
	    glade_project_get_format (project) == GLADE_PROJECT_FORMAT_GTKBUILDER)
	{
		widgets_node = glade_xml_node_new (context, GLADE_TAG_ACTION_WIDGETS);

		glade_gtk_dialog_write_responses (parent, context, widgets_node);

		if (!glade_xml_node_get_children (widgets_node))
			glade_xml_node_delete (widgets_node);
		else
			glade_xml_node_append_child (node, widgets_node);
	}
}

/*--------------------------- GtkMessageDialog ---------------------------------*/
static gboolean
glade_gtk_message_dialog_reset_image (GtkMessageDialog *dialog)
{
	GtkWidget *image;
	gint message_type;

	g_object_get (dialog, "message-type", &message_type, NULL);
	if (message_type != GTK_MESSAGE_OTHER)
		return FALSE;

	image = gtk_message_dialog_get_image (dialog);
	if (glade_widget_get_from_gobject (image))
	{
		gtk_message_dialog_set_image (dialog,
					      gtk_image_new_from_stock (NULL, GTK_ICON_SIZE_DIALOG));
		gtk_widget_show (image);

		return TRUE;
	}
	else
		return FALSE;
}

enum {
	MD_IMAGE_ACTION_INVALID,
	MD_IMAGE_ACTION_RESET,
	MD_IMAGE_ACTION_SET
};

static gint
glade_gtk_message_dialog_image_determine_action (GtkMessageDialog *dialog,
					   const GValue *value,
					   GtkWidget **image,
					   GladeWidget **gimage)
{
	GtkWidget *dialog_image = gtk_message_dialog_get_image (dialog);

	*image = g_value_get_object (value);
	
	if (*image == NULL)
		if (glade_widget_get_from_gobject (dialog_image))
			return MD_IMAGE_ACTION_RESET;
		else
			return MD_IMAGE_ACTION_INVALID;
	else
	{
		*image = GTK_WIDGET (*image);
		if (dialog_image == *image)
			return MD_IMAGE_ACTION_INVALID;
		if (gtk_widget_get_parent (*image))
			return MD_IMAGE_ACTION_INVALID;
			
		*gimage = glade_widget_get_from_gobject (*image);

		if (!*gimage)
		{
			g_warning ("Setting property to an object outside the project");
			return MD_IMAGE_ACTION_INVALID;
		}
		
		if (glade_widget_get_parent (*gimage) || GTK_IS_WINDOW (*image))
			return MD_IMAGE_ACTION_INVALID;

		return MD_IMAGE_ACTION_SET;
	}
}

void
glade_gtk_message_dialog_set_property (GladeWidgetAdaptor *adaptor,
			       GObject            *object,
			       const gchar        *id,
			       const GValue       *value)
{
	GtkMessageDialog *dialog = GTK_MESSAGE_DIALOG (object);
	GladeWidget *gwidget = glade_widget_get_from_gobject (object);

	g_return_if_fail (gwidget);

	if (strcmp (id, "image") == 0)
	{
		GtkWidget *image = NULL;
		GladeWidget *gimage = NULL;
		gint rslt;

		rslt = glade_gtk_message_dialog_image_determine_action (dialog, value,
								       &image, &gimage);
		switch (rslt)
		{
		case MD_IMAGE_ACTION_INVALID:
			return;
		case MD_IMAGE_ACTION_RESET:
			glade_gtk_message_dialog_reset_image (dialog);
			return;
		case MD_IMAGE_ACTION_SET:
			break; /* continue setting the property */
		}

		if (gtk_widget_get_parent (image))
			g_critical ("Image should have no parent now");

		gtk_message_dialog_set_image (dialog, image);

		{
			/* syncing "message-type" property */	
			GladeProperty *property;

			property = glade_widget_get_property (gwidget, "message-type");
			if (!glade_property_equals (property, GTK_MESSAGE_OTHER))
				glade_command_set_property (property, GTK_MESSAGE_OTHER);
		}
	}
	else
	{
		/* We must reset the image to internal,
		 * external image would otherwise become internal
		 */
		if (!strcmp (id, "message-type") &&
		    g_value_get_enum (value) != GTK_MESSAGE_OTHER)
		{
			GladeProperty *property;

			property = glade_widget_get_property (gwidget, "image");
			if (!glade_property_equals (property, NULL))
				glade_command_set_property (property, NULL);
		}
		/* Chain up, even if property us message-type because
		 * it's not fully handled here
		 */
		GWA_GET_CLASS (GTK_TYPE_DIALOG)->set_property (adaptor, object,
								  id, value);
	}
}

gboolean
glade_gtk_message_dialog_verify_property (GladeWidgetAdaptor *adaptor,
				 GObject            *object,
				 const gchar        *id,
				 const GValue       *value)
{
	if (!strcmp (id, "image"))
	{
		GtkWidget *image; GladeWidget *gimage;

		gboolean retval = MD_IMAGE_ACTION_INVALID != 
		       glade_gtk_message_dialog_image_determine_action (GTK_MESSAGE_DIALOG (object),
									value, &image, &gimage);

		return retval;
	}
	else
		if (GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property)
			return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property (adaptor, object,
										    id, value);
		else
			return TRUE;
}

void
glade_gtk_message_dialog_get_property (GladeWidgetAdaptor *adaptor,
				  GObject *object,
				  const gchar *property_name,
				  GValue *value)
{
	if (!strcmp (property_name, "image"))
	{
		GtkMessageDialog *dialog = GTK_MESSAGE_DIALOG (object);
		GtkWidget        *image = gtk_message_dialog_get_image (dialog);

		if (!glade_widget_get_from_gobject (image))
			g_value_set_object (value, NULL);
		else
			g_value_set_object (value, image);
	}
	else
		GWA_GET_CLASS (GTK_TYPE_DIALOG)->get_property (adaptor, object,
								  property_name, value);
}

/* ----------------------------- GtkFileChooserWidget ------------------------------ */
void
glade_gtk_file_chooser_widget_post_create (GladeWidgetAdaptor *adaptor,
					   GObject            *object,
					   GladeCreateReason   reason)
{
	gtk_container_forall (GTK_CONTAINER (object),
			      glade_gtk_file_chooser_default_forall,
			      NULL);
}

void
glade_gtk_file_chooser_button_set_property (GladeWidgetAdaptor *adaptor,
					    GObject            *object, 
					    const gchar        *id,
					    const GValue       *value)
{
	/* Avoid a warning */
	if (!strcmp (id, "action"))
	{
		if (g_value_get_enum (value) == GTK_FILE_CHOOSER_ACTION_CREATE_FOLDER ||
		    g_value_get_enum (value) == GTK_FILE_CHOOSER_ACTION_SAVE)
		    return;
	}

	GWA_GET_CLASS (GTK_TYPE_BOX)->set_property (adaptor,
						    object,
						    id, value);
}

/* ----------------------------- GtkFontButton ------------------------------ */
/* Use the font-buttons launch dialog to actually set the font-name
 * glade property through the glade-command api.
 */
static void
glade_gtk_font_button_refresh_font_name (GtkFontButton  *button,
					 GladeWidget    *gbutton)
{
	GladeProperty *property;
	
	if ((property =
	     glade_widget_get_property (gbutton, "font-name")) != NULL)
		glade_command_set_property  (property,
					     gtk_font_button_get_font_name (button));
}


/* ----------------------------- GtkColorButton ------------------------------ */
static void
glade_gtk_color_button_refresh_color (GtkColorButton  *button,
				      GladeWidget     *gbutton)
{
	GladeProperty *property;
	GdkColor       color = { 0, };
	
	if ((property = glade_widget_get_property (gbutton, "color")) != NULL)
		glade_command_set_property (property, &color);
}


void
glade_gtk_color_button_set_property (GladeWidgetAdaptor *adaptor,
				     GObject            *object, 
				     const gchar        *id,
				     const GValue       *value)
{
	if (!strcmp (id, "color"))
	{
		if (g_value_get_boxed (value))
			gtk_color_button_set_color (GTK_COLOR_BUTTON (object), 
						    (GdkColor *)g_value_get_boxed (value));
	}
	else
		GWA_GET_CLASS (GTK_TYPE_BUTTON)->set_property (adaptor,
							       object,
							       id, value);
}


/* ----------------------------- GtkActivatable ------------------------------ */
static void
activatable_parse_finished (GladeProject *project, 
			    GladeWidget  *widget)
{
	GObject *related_action = NULL;

	glade_widget_property_get (widget, "related-action", &related_action);
	if (related_action == NULL)
		glade_widget_property_set (widget, "use-action-appearance", FALSE);
}

static void
evaluate_activatable_property_sensitivity (GObject            *object, 
					   const gchar        *id,
					   const GValue       *value)
{
	GladeWidget *gwidget = glade_widget_get_from_gobject (object);
	gboolean sensitivity;
	gchar *msg;

	if (!strcmp (id, "related-action"))
	  {
		  GtkAction *action = g_value_get_object (value);

		  if (action)
		    {
			    sensitivity = FALSE;
			    msg = ACTION_APPEARANCE_MSG;
		    }
		  else
		    {
			    sensitivity = TRUE;
			    msg = NULL;
		    }

		  glade_widget_property_set_sensitive (gwidget, "visible", sensitivity, msg);
		  glade_widget_property_set_sensitive (gwidget, "sensitive", sensitivity, msg);
		  glade_widget_property_set_sensitive (gwidget, "accel-group", sensitivity, msg);
		  glade_widget_property_set_sensitive (gwidget, "use-action-appearance", !sensitivity, sensitivity ? msg : NULL);
	  }
	else if (!strcmp (id, "use-action-appearance"))
	  {
		  if (g_value_get_boolean (value))
		    {
			    sensitivity = FALSE;
			    msg = ACTION_APPEARANCE_MSG;
		    }
		  else
		    {
			    sensitivity = TRUE;
			    msg = NULL;
		    }

		  glade_widget_property_set_sensitive (gwidget, "label", sensitivity, msg);
		  glade_widget_property_set_sensitive (gwidget, "use-underline", sensitivity, msg);
		  glade_widget_property_set_sensitive (gwidget, "stock", sensitivity, msg);
		  //glade_widget_property_set_sensitive (gwidget, "use-stock", sensitivity, msg);
		  glade_widget_property_set_sensitive (gwidget, "image", sensitivity, msg);
		  glade_widget_property_set_sensitive (gwidget, "custom-child", sensitivity, msg);
		  glade_widget_property_set_sensitive (gwidget, "stock-id", sensitivity, msg);
		  glade_widget_property_set_sensitive (gwidget, "label-widget", sensitivity, msg);
		  glade_widget_property_set_sensitive (gwidget, "icon-name", sensitivity, msg);
		  glade_widget_property_set_sensitive (gwidget, "icon-widget", sensitivity, msg);
		  glade_widget_property_set_sensitive (gwidget, "icon", sensitivity, msg);
	  }
}

/* ----------------------------- GtkButton ------------------------------ */
static void 
sync_use_appearance (GladeWidget *gwidget)
{
	GladeProperty *prop = glade_widget_get_property (gwidget, "use-action-appearance");
	gboolean       use_appearance = FALSE;

	/* This is the kind of thing we avoid doing at project load time ;-) */
	if (glade_widget_superuser ())
		return;

	glade_property_get (prop, &use_appearance);
	if (use_appearance)
       	{
		glade_property_set (prop, FALSE);
		glade_property_set (prop, TRUE);
       	}
}

GladeEditable *
glade_gtk_button_create_editable (GladeWidgetAdaptor  *adaptor,
				  GladeEditorPageType  type)
{
	GladeEditable *editable;

	/* Get base editable */
	editable = GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);

	if (type == GLADE_PAGE_GENERAL)
	{
		editable = (GladeEditable *)glade_activatable_editor_new (adaptor, editable);
		return (GladeEditable *)glade_button_editor_new (adaptor, editable);
	}
	return editable;
}

void
glade_gtk_button_post_create (GladeWidgetAdaptor  *adaptor,
			      GObject             *button, 
			      GladeCreateReason    reason)
{
	GladeWidget *gbutton = glade_widget_get_from_gobject (button);

	g_return_if_fail (GTK_IS_BUTTON (button));
	g_return_if_fail (GLADE_IS_WIDGET (gbutton));

	if (GTK_IS_FONT_BUTTON (button))
		g_signal_connect
			(button, "font-set", 
			 G_CALLBACK (glade_gtk_font_button_refresh_font_name), gbutton);
	else if (GTK_IS_COLOR_BUTTON (button))
		g_signal_connect
			(button, "color-set", 
			 G_CALLBACK (glade_gtk_color_button_refresh_color), gbutton);

	/* Disabled response-id until its in an action area */
	glade_widget_property_set_sensitive (gbutton, "response-id", FALSE, 
					     RESPID_INSENSITIVE_MSG);
	glade_widget_property_set_enabled (gbutton, "response-id", FALSE);

	if (reason == GLADE_CREATE_LOAD)
		g_signal_connect (G_OBJECT (gbutton->project), "parse-finished",
				  G_CALLBACK (activatable_parse_finished),
				  gbutton);
}

void
glade_gtk_button_set_property (GladeWidgetAdaptor *adaptor,
			       GObject            *object, 
			       const gchar        *id,
			       const GValue       *value)
{
	GladeWidget *widget = glade_widget_get_from_gobject (object);
	GladeProperty *property = glade_widget_get_property (widget, id);

	evaluate_activatable_property_sensitivity (object, id, value);

	if (strcmp (id, "custom-child") == 0)
	{
		GtkWidget *child = gtk_bin_get_child (GTK_BIN (object));

		if (g_value_get_boolean (value))
		{
			if (child)
				gtk_container_remove (GTK_CONTAINER (object), child);

			gtk_container_add (GTK_CONTAINER (object), glade_placeholder_new ());
		}
		else if (child && GLADE_IS_PLACEHOLDER (child))
			gtk_container_remove (GTK_CONTAINER (object), child);
	}
	else if (strcmp (id, "stock") == 0)
	{
		gboolean use_stock  = FALSE;
		glade_widget_property_get (widget, "use-stock", &use_stock);
		if (use_stock)
			gtk_button_set_label (GTK_BUTTON (object), g_value_get_string (value));
	}
	else if (strcmp (id, "use-stock") == 0)
	{
		/* I guess its my bug in GTK+, we need to resync the appearance property
		 * on GtkButton when the GtkButton:use-stock property changes.
		 */
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object,
								  id, value);
		sync_use_appearance (widget);
	}
	else if (GPC_VERSION_CHECK (property->klass, gtk_major_version, gtk_minor_version + 1))
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object,
								  id, value);
}

void
glade_gtk_button_read_widget (GladeWidgetAdaptor *adaptor,
			      GladeWidget        *widget,
			      GladeXmlNode       *node)
{
	gboolean  use_stock;
	gchar    *label = NULL;

	if (!glade_xml_node_verify 
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and read in all the normal properties.. */
        GWA_GET_CLASS (GTK_TYPE_CONTAINER)->read_widget (adaptor, widget, node);

	/* Update the stock property */
	glade_widget_property_get (widget, "use-stock", &use_stock);
	if (use_stock)
	{
		glade_widget_property_get (widget, "label", &label);
		glade_widget_property_set (widget, "stock", label);
	}
}

void
glade_gtk_button_write_widget (GladeWidgetAdaptor *adaptor,
			       GladeWidget        *widget,
			       GladeXmlContext    *context,
			       GladeXmlNode       *node)
{
	GladeProject  *project = widget->project;
	GladeProperty *prop;
	gboolean       use_stock;
	gchar         *stock = NULL;

	if (!glade_xml_node_verify
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* Do not save GtkColorButton and GtkFontButton label property */
	if (!(GTK_IS_COLOR_BUTTON (widget->object) || GTK_IS_FONT_BUTTON (widget->object)))
	{
		/* Make a copy of the GladeProperty, 
		 * override its value and ensure non-translatable if use-stock is TRUE
		 */
		prop = glade_widget_get_property (widget, "label");
		prop = glade_property_dup (prop, widget);
		glade_widget_property_get (widget, "use-stock", &use_stock);
		if (use_stock)
		{
			glade_widget_property_get (widget, "stock", &stock);
			glade_property_i18n_set_translatable (prop, FALSE);
			glade_property_set (prop, stock);
		}
		glade_property_write (prop, context, node);
		g_object_unref (G_OBJECT (prop));
	}

	prop = glade_widget_get_property (widget, "response-id");
	if (glade_property_get_enabled (prop) && 
	    glade_project_get_format (project) == GLADE_PROJECT_FORMAT_LIBGLADE)
		glade_property_write (prop, context, node);

	/* Write out other normal properties and any other class derived custom properties after ... */
        GWA_GET_CLASS (GTK_TYPE_CONTAINER)->write_widget (adaptor, widget, context, node);
}


/* ----------------------------- GtkImage ------------------------------ */
void
glade_gtk_image_read_widget (GladeWidgetAdaptor *adaptor,
			     GladeWidget        *widget,
			     GladeXmlNode       *node)
{
	GladeProperty *property;

	if (!glade_xml_node_verify 
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and read in all the normal properties.. */
        GWA_GET_CLASS (GTK_TYPE_WIDGET)->read_widget (adaptor, widget, node);
	
	if (glade_widget_property_original_default (widget, "icon-name") == FALSE)
	{
		property = glade_widget_get_property (widget, "icon-name");
		glade_widget_property_set (widget, "image-mode", GLADE_IMAGE_MODE_ICON);
	}
	else if (glade_widget_property_original_default (widget, "pixbuf") == FALSE)
	{
		property = glade_widget_get_property (widget, "pixbuf");
		glade_widget_property_set (widget, "image-mode", GLADE_IMAGE_MODE_FILENAME);
	}
	else/*  if (glade_widget_property_original_default (widget, "stock") == FALSE) */
	{
		property = glade_widget_get_property (widget, "stock");
		glade_widget_property_set (widget, "image-mode", GLADE_IMAGE_MODE_STOCK);
	}

	glade_property_sync (property);
}


void
glade_gtk_image_write_widget (GladeWidgetAdaptor *adaptor,
			      GladeWidget        *widget,
			      GladeXmlContext    *context,
			      GladeXmlNode       *node)
{
	GladeXmlNode  *prop_node;
	GladeProperty *size_prop;
	GtkIconSize    icon_size;
	gchar         *value;

	if (!glade_xml_node_verify
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and write all the normal properties (including "use-stock")... */
        GWA_GET_CLASS (GTK_TYPE_WIDGET)->write_widget (adaptor, widget, context, node);

	/* We have to save icon-size as an integer, the core will take care of 
	 * loading the int value though.
	 */
	size_prop = glade_widget_get_property (widget, "icon-size");
	if (!glade_property_original_default (size_prop))
	{
		prop_node = glade_xml_node_new (context, GLADE_TAG_PROPERTY);
		glade_xml_node_append_child (node, prop_node);

		glade_xml_node_set_property_string (prop_node, GLADE_TAG_NAME, size_prop->klass->id);

		glade_property_get (size_prop, &icon_size);
		value = g_strdup_printf ("%d", icon_size);
		glade_xml_set_content (prop_node, value);
		g_free (value);
	}
}


static void
glade_gtk_image_set_image_mode (GObject *object, const GValue *value)
{
	GladeWidget        *gwidget;
	GladeImageEditMode  type;
	
	gwidget = glade_widget_get_from_gobject (object);
	g_return_if_fail (GTK_IS_IMAGE (object));
	g_return_if_fail (GLADE_IS_WIDGET (gwidget));
	
	glade_widget_property_set_sensitive (gwidget, "stock", FALSE, NOT_SELECTED_MSG);
	glade_widget_property_set_sensitive (gwidget, "icon-name", FALSE, NOT_SELECTED_MSG);
	glade_widget_property_set_sensitive (gwidget, "pixbuf", FALSE, NOT_SELECTED_MSG);
	glade_widget_property_set_sensitive (gwidget, "icon-size", FALSE, 
					     _("This property only applies to stock images"));
	glade_widget_property_set_sensitive (gwidget, "pixel-size", FALSE, 
					     _("This property only applies to named icons"));

	switch ((type = g_value_get_int (value)))
	{
	case GLADE_IMAGE_MODE_STOCK:
		glade_widget_property_set_sensitive (gwidget, "stock", TRUE, NULL);
		glade_widget_property_set_sensitive (gwidget, "icon-size", TRUE, NULL);
		break;
		
	case GLADE_IMAGE_MODE_ICON:
		glade_widget_property_set_sensitive (gwidget, "icon-name", TRUE, NULL);
		glade_widget_property_set_sensitive (gwidget, "pixel-size", TRUE, NULL);
		break;
		
	case GLADE_IMAGE_MODE_FILENAME:
		glade_widget_property_set_sensitive (gwidget, "pixbuf", TRUE, NULL);
	default:
		break;
	}
}

void
glade_gtk_image_get_property (GladeWidgetAdaptor *adaptor,
			      GObject            *object, 
			      const gchar        *id,
			      GValue             *value)
{
	if (!strcmp (id, "icon-size"))
	{
		/* Make the int an enum... */
		GValue int_value = { 0, };
		g_value_init (&int_value, G_TYPE_INT);
		GWA_GET_CLASS (GTK_TYPE_WIDGET)->get_property (adaptor, object, id, &int_value);
		g_value_set_enum (value, g_value_get_int (&int_value));
		g_value_unset (&int_value);
	}
	else
		GWA_GET_CLASS (GTK_TYPE_WIDGET)->get_property (adaptor, object,
							       id, value);
}

void
glade_gtk_image_set_property (GladeWidgetAdaptor *adaptor,
			      GObject            *object, 
			      const gchar        *id,
			      const GValue       *value)
{
	if (!strcmp (id, "image-mode"))
		glade_gtk_image_set_image_mode (object, value);
	else if (!strcmp (id, "icon-size"))
	{
		/* Make the enum an int... */
		GValue int_value = { 0, };
		g_value_init (&int_value, G_TYPE_INT);
		g_value_set_int (&int_value, g_value_get_enum (value));
		GWA_GET_CLASS (GTK_TYPE_WIDGET)->set_property (adaptor, object, id, &int_value);
		g_value_unset (&int_value);
	}
	else
	{
		GladeWidget *widget = glade_widget_get_from_gobject (object);
		GladeImageEditMode mode = 0;

		glade_widget_property_get (widget, "image-mode", &mode);

		/* avoid setting properties in the wrong mode... */
		switch (mode)
		{
		case GLADE_IMAGE_MODE_STOCK:
			if (!strcmp (id, "icon-name") ||
			    !strcmp (id, "pixbuf"))
				return;
			break;
		case GLADE_IMAGE_MODE_ICON:
			if (!strcmp (id, "stock") ||
			    !strcmp (id, "pixbuf"))
				return;
			break;
		case GLADE_IMAGE_MODE_FILENAME:
			if (!strcmp (id, "stock") ||
			    !strcmp (id, "icon-name"))
				return;
		default:
			break;
		}

		GWA_GET_CLASS (GTK_TYPE_WIDGET)->set_property (adaptor, object,
							       id, value);
	}
}


GladeEditable *
glade_gtk_image_create_editable (GladeWidgetAdaptor  *adaptor,
				 GladeEditorPageType  type)
{
	GladeEditable *editable;

	/* Get base editable */
	editable = GWA_GET_CLASS (GTK_TYPE_WIDGET)->create_editable (adaptor, type);

	if (type == GLADE_PAGE_GENERAL)
		return (GladeEditable *)glade_image_editor_new (adaptor, editable);

	return editable;
}

/* ----------------------------- GtkMenu ------------------------------ */
GObject *
glade_gtk_menu_constructor (GType                  type,
			    guint                  n_construct_properties,
			    GObjectConstructParam *construct_properties)
{
	GladeWidgetAdaptor   *adaptor;
	GObject              *ret_obj;
	
	ret_obj = GWA_GET_OCLASS(GTK_TYPE_CONTAINER)->constructor
		(type, n_construct_properties, construct_properties);

	adaptor = GLADE_WIDGET_ADAPTOR (ret_obj);

	glade_widget_adaptor_action_remove (adaptor, "add_parent");
	glade_widget_adaptor_action_remove (adaptor, "remove_parent");

	return ret_obj;
}

void
glade_gtk_menu_read_widget (GladeWidgetAdaptor *adaptor,
			    GladeWidget        *widget,
			    GladeXmlNode       *node)
{
	if (!glade_xml_node_verify 
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and read in all the normal properties.. */
        GWA_GET_CLASS (GTK_TYPE_WIDGET)->read_widget (adaptor, widget, node);

	if (glade_project_get_format (widget->project) == GLADE_PROJECT_FORMAT_LIBGLADE &&
	    widget->parent && GTK_IS_MENU_ITEM (widget->parent->object))
		g_object_set_data (widget->object, "special-child-type", "submenu");
}


/* ----------------------------- GtkMenuShell ------------------------------ */
void
glade_gtk_menu_shell_add_child (GladeWidgetAdaptor  *adaptor, 
			       GObject             *object, 
			       GObject             *child)
{
	
	g_return_if_fail (GTK_IS_MENU_SHELL (object));
	g_return_if_fail (GTK_IS_MENU_ITEM (child));

	gtk_menu_shell_append (GTK_MENU_SHELL (object), GTK_WIDGET (child));
}


void
glade_gtk_menu_shell_remove_child (GladeWidgetAdaptor  *adaptor,
				  GObject             *object, 
				  GObject             *child)
{
	g_return_if_fail (GTK_IS_MENU_SHELL (object));
	g_return_if_fail (GTK_IS_MENU_ITEM (child));
	
	gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
}

static gint
glade_gtk_menu_shell_get_item_position (GObject *container, GObject *child)
{
	gint position = 0;
	GList *list = gtk_container_get_children (GTK_CONTAINER (container));

	while (list)
	{
		if (G_OBJECT (list->data) == child) break;

		list = list->next;
		position++;
	}

	g_list_free (list);

	return position;
}

void
glade_gtk_menu_shell_get_child_property (GladeWidgetAdaptor  *adaptor,
					 GObject             *container,
					 GObject             *child,
					 const gchar         *property_name,
					 GValue              *value)
{
	g_return_if_fail (GTK_IS_MENU_SHELL (container));
	g_return_if_fail (GTK_IS_MENU_ITEM (child));
	
	if (strcmp (property_name, "position") == 0)
	{
		g_value_set_int (value, 
			 glade_gtk_menu_shell_get_item_position (container,
								 child));
	}
	else
		/* Chain Up */
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->child_get_property (adaptor, 
								  container, 
								  child, 
								  property_name, 
								  value);
}

void
glade_gtk_menu_shell_set_child_property (GladeWidgetAdaptor  *adaptor,
					 GObject             *container,
					 GObject             *child,
					 const gchar         *property_name,
					 GValue              *value)
{
	g_return_if_fail (GTK_IS_MENU_SHELL (container));
	g_return_if_fail (GTK_IS_MENU_ITEM (child));
	g_return_if_fail (property_name != NULL || value != NULL);
	
	if (strcmp (property_name, "position") == 0)
	{
		GladeWidget *gitem;
		gint position;
		
		gitem = glade_widget_get_from_gobject (child);
		g_return_if_fail (GLADE_IS_WIDGET (gitem));
	
		position = g_value_get_int (value);
	
		if (position < 0)
		{
			position = glade_gtk_menu_shell_get_item_position (container, child);
			g_value_set_int (value, position);
		}
	
		g_object_ref (child);
		gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET (child));
		gtk_menu_shell_insert (GTK_MENU_SHELL (container), GTK_WIDGET (child), position);
		g_object_unref (child);

	}
	else
		/* Chain Up */
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->child_set_property (adaptor, 
								  container, child,
								  property_name, value);
}

static gchar *
glade_gtk_menu_shell_tool_item_get_display_name (GladeBaseEditor *editor,
						 GladeWidget *gchild,
						 gpointer user_data)
{
  GObject *child = glade_widget_get_object (gchild);
  gchar *name;

  if (GTK_IS_SEPARATOR_MENU_ITEM (child) || GTK_IS_SEPARATOR_TOOL_ITEM (child))
    name = _("<separator>");
  else if (GTK_IS_MENU_ITEM (child))
    glade_widget_property_get (gchild, "label", &name);
  else if (GTK_IS_TOOL_BUTTON (child))
    {
      glade_widget_property_get (gchild, "label", &name);
      if (name == NULL || strlen (name) == 0)
        glade_widget_property_get (gchild, "stock-id", &name);
    }
  else if (GTK_IS_TOOL_ITEM_GROUP (child))
    glade_widget_property_get (gchild, "label", &name);
  else
    name = _("<custom>");

  return g_strdup (name);
}

static GladeWidget *
glade_gtk_menu_shell_item_get_parent (GladeWidget *gparent, GObject *parent)
{
	GtkWidget *submenu = NULL;
	
	if (GTK_IS_MENU_TOOL_BUTTON (parent))
		submenu = gtk_menu_tool_button_get_menu (GTK_MENU_TOOL_BUTTON (parent));
	else if (GTK_IS_MENU_ITEM (parent))
		submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (parent));

	if (submenu && glade_widget_get_from_gobject (submenu))
		gparent = glade_widget_get_from_gobject (submenu);
	else
		gparent =
			glade_command_create (glade_widget_adaptor_get_by_type (GTK_TYPE_MENU),
					      gparent, NULL,
					      glade_widget_get_project (gparent));

	return gparent;
}

static GladeWidget *  
glade_gtk_menu_shell_build_child (GladeBaseEditor *editor,
				  GladeWidget *gparent,
				  GType type,
				  gpointer data)
{
	GObject *parent = glade_widget_get_object (gparent);
	GladeWidget *gitem_new;

	if (GTK_IS_SEPARATOR_MENU_ITEM (parent))
		return NULL;
	
	/* Get or build real parent */
	if (GTK_IS_MENU_ITEM (parent) || GTK_IS_MENU_TOOL_BUTTON (parent))
		gparent = glade_gtk_menu_shell_item_get_parent (gparent, parent);
	
	/* Build child */
	gitem_new = glade_command_create (glade_widget_adaptor_get_by_type (type),
					  gparent, NULL, 
					  glade_widget_get_project (gparent));
	
	if (type != GTK_TYPE_SEPARATOR_MENU_ITEM)
	{
		glade_widget_property_set (gitem_new, "label",
					   glade_widget_get_name (gitem_new));
		glade_widget_property_set (gitem_new, "use-underline", TRUE);
	}
	
	return gitem_new;
}

static gboolean
glade_gtk_menu_shell_delete_child (GladeBaseEditor *editor,
				   GladeWidget *gparent,
				   GladeWidget *gchild,
				   gpointer data)
{
	GObject *item = glade_widget_get_object (gparent);
	GtkWidget *submenu = NULL;
	GList list = {0, };
	gint n_children = 0;
	
	if (GTK_IS_MENU_ITEM (item) &&
	    (submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (item))))
	{
		GList *l = gtk_container_get_children (GTK_CONTAINER (submenu));
		n_children = g_list_length (l);
		g_list_free (l);
	}
		
	if (submenu && n_children == 1)
		list.data = glade_widget_get_parent (gchild);
	else
		list.data = gchild;
	
	/* Remove widget */
	glade_command_delete (&list);
	
	return TRUE;
}

static gboolean
glade_gtk_menu_shell_move_child (GladeBaseEditor *editor,
				 GladeWidget *gparent,
				 GladeWidget *gchild,
				 gpointer data)
{	
  GObject     *parent     = glade_widget_get_object (gparent);
  GObject     *child      = glade_widget_get_object (gchild);
  GladeWidget *old_parent = glade_widget_get_parent (gchild);
  GladeWidget *old_parent_parent;
  GList list = { 0, };

  if (GTK_IS_SEPARATOR_MENU_ITEM (parent) ||
      GTK_IS_SEPARATOR_TOOL_ITEM (parent))
    return FALSE;

  /* Moving a menu item child */
  if (GTK_IS_MENU_ITEM (child))
    {
      if (GTK_IS_TOOLBAR (parent))         return FALSE;
      if (GTK_IS_TOOL_ITEM_GROUP (parent)) return FALSE;
      if (GTK_IS_TOOL_PALETTE (parent))    return FALSE;

      if (GTK_IS_TOOL_ITEM (parent) && !GTK_IS_MENU_TOOL_BUTTON (parent))
	return FALSE;
    }

  /* Moving a toolitem child */
  if (GTK_IS_TOOL_ITEM (child))
    {
      if (GTK_IS_MENU (parent))         return FALSE;
      if (GTK_IS_MENU_BAR (parent))     return FALSE;
      if (GTK_IS_MENU_ITEM (parent))    return FALSE;
      if (GTK_IS_TOOL_PALETTE (parent)) return FALSE;
      if (GTK_IS_TOOL_ITEM (parent))    return FALSE;
    }

  /* Moving a toolitem group */
  if (GTK_IS_TOOL_ITEM_GROUP (child))
    {
      if (!GTK_IS_TOOL_PALETTE (parent)) return FALSE;
    }

  if (GTK_IS_MENU_ITEM (parent) || GTK_IS_MENU_TOOL_BUTTON (parent))
    gparent = glade_gtk_menu_shell_item_get_parent (gparent, parent);

  if (gparent != glade_widget_get_parent (gchild))
    {
      list.data = gchild;
      glade_command_dnd (&list, gparent, NULL);
    }

  /* Delete dangling childless menus */
  old_parent_parent = glade_widget_get_parent (old_parent);
  if (GTK_IS_MENU (glade_widget_get_object (old_parent)) &&
      old_parent_parent && 
      GTK_IS_MENU_ITEM (glade_widget_get_object (old_parent_parent)))
    {
      GList del = { 0, }, *children;

      children =
	gtk_container_get_children (GTK_CONTAINER (glade_widget_get_object (old_parent)));
      if (!children)
        {
          del.data = old_parent;
          glade_command_delete (&del);
        }
      g_list_free (children);
    }

	return TRUE;
}

static gboolean
glade_gtk_menu_shell_change_type (GladeBaseEditor *editor,
				  GladeWidget *gchild,
				  GType type,
				  gpointer data)
{
	GObject *child = glade_widget_get_object (gchild);
	

	if ((type == GTK_TYPE_SEPARATOR_MENU_ITEM &&
	     gtk_menu_item_get_submenu (GTK_MENU_ITEM (child))) ||
	    (GTK_IS_MENU_TOOL_BUTTON (child) &&
	     gtk_menu_tool_button_get_menu (GTK_MENU_TOOL_BUTTON (child))))
	    return TRUE;

	/* Delete the internal image of an image menu item before going ahead and changing types. */
	if (GTK_IS_IMAGE_MENU_ITEM (child))
	{
		GList list = { 0, };
		GtkWidget   *image = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (child));
		GladeWidget *widget;

		if (image && (widget = glade_widget_get_from_gobject (image)))
		{
			list.data = widget;
			glade_command_unlock_widget (widget);
			glade_command_delete (&list);
		}
	}
		
	return FALSE;
}

static void
glade_gtk_toolbar_child_selected (GladeBaseEditor *editor,
				  GladeWidget *gchild,
				  gpointer data)
{
	GObject *child = glade_widget_get_object (gchild);
	GladeWidget *gparent = glade_widget_get_parent (gchild);
	GObject *parent = glade_widget_get_object (gparent);
	GType type = G_OBJECT_TYPE (child);
	
	glade_base_editor_add_label (editor, _("Tool Item"));
	
	glade_base_editor_add_default_properties (editor, gchild);
	
	glade_base_editor_add_label (editor, _("Properties"));
	glade_base_editor_add_editable (editor, gchild, GLADE_PAGE_GENERAL);
	
	if (type == GTK_TYPE_SEPARATOR_TOOL_ITEM) return;

	glade_base_editor_add_label (editor, _("Packing"));
	if (GTK_IS_TOOLBAR (parent))
		glade_base_editor_add_properties (editor, gchild, TRUE,
						  "expand", "homogeneous", NULL);
	else if (GTK_IS_TOOL_ITEM_GROUP (parent))
		glade_base_editor_add_properties (editor, gchild, TRUE,
						  "expand", "fill", "homogeneous", "new-row", NULL);
}

static void
glade_gtk_tool_palette_child_selected (GladeBaseEditor * editor,
				       GladeWidget * gchild, gpointer data)
{
  glade_base_editor_add_label (editor, _("Tool Item Group"));

  glade_base_editor_add_default_properties (editor, gchild);

  glade_base_editor_add_label (editor, _("Properties"));
  glade_base_editor_add_properties (editor, gchild, FALSE, 
				    "tooltip-text",
				    NULL);
  glade_base_editor_add_editable (editor, gchild, GLADE_PAGE_GENERAL);

  glade_base_editor_add_label (editor, _("Packing"));
  glade_base_editor_add_properties (editor, gchild, TRUE,
                                    "exclusive", "expand", NULL);
}

static void
glade_gtk_menu_shell_tool_item_child_selected (GladeBaseEditor *editor,
					       GladeWidget *gchild,
					       gpointer data)
{
	GObject *child = glade_widget_get_object (gchild);
	GType type = G_OBJECT_TYPE (child);

	if (GTK_IS_TOOL_ITEM (child))
	{
		glade_gtk_toolbar_child_selected (editor, gchild, data);
		return;
	}	

	if (GTK_IS_TOOL_ITEM_GROUP (child))
	{
		glade_gtk_tool_palette_child_selected (editor, gchild, data);
		return;
	}

	glade_base_editor_add_label (editor, _("Menu Item"));
	
	glade_base_editor_add_default_properties (editor, gchild);
	
	if (GTK_IS_SEPARATOR_MENU_ITEM (child)) return;

	glade_base_editor_add_label (editor, _("Properties"));

	if (glade_project_get_format (gchild->project) == GLADE_PROJECT_FORMAT_LIBGLADE)
		glade_base_editor_add_properties (editor, gchild, FALSE, "tooltip", NULL);
	else
		glade_base_editor_add_properties (editor, gchild, FALSE, "tooltip-text", NULL);


	if (type != GTK_TYPE_IMAGE_MENU_ITEM)
		glade_base_editor_add_properties (editor, gchild, FALSE, "label", "tooltip", NULL);

	if (type == GTK_TYPE_IMAGE_MENU_ITEM)
		glade_base_editor_add_editable (editor, gchild, GLADE_PAGE_GENERAL);
	else if (type == GTK_TYPE_CHECK_MENU_ITEM)
		glade_base_editor_add_properties (editor, gchild, FALSE,
						  "active", "draw-as-radio",
						  "inconsistent", NULL);
	else if (type == GTK_TYPE_RADIO_MENU_ITEM)
		glade_base_editor_add_properties (editor, gchild, FALSE,
						  "active", "group", NULL);
}

static void
glade_gtk_menu_shell_launch_editor (GObject *object, gchar *title)
{
	GladeBaseEditor *editor;
	GtkWidget *window;

	/* Editor */
	editor = glade_base_editor_new (object, NULL,
					_("Normal item"), GTK_TYPE_MENU_ITEM,
					_("Image item"), GTK_TYPE_IMAGE_MENU_ITEM,
					_("Check item"), GTK_TYPE_CHECK_MENU_ITEM,
					_("Radio item"), GTK_TYPE_RADIO_MENU_ITEM,
					_("Separator item"), GTK_TYPE_SEPARATOR_MENU_ITEM,
					NULL);

	glade_base_editor_append_types (editor, GTK_TYPE_MENU_ITEM,
					_("Normal item"), GTK_TYPE_MENU_ITEM,
					_("Image item"), GTK_TYPE_IMAGE_MENU_ITEM,
					_("Check item"), GTK_TYPE_CHECK_MENU_ITEM,
					_("Radio item"), GTK_TYPE_RADIO_MENU_ITEM,
					_("Separator item"), GTK_TYPE_SEPARATOR_MENU_ITEM,
					NULL);
	
	g_signal_connect (editor, "get-display-name", G_CALLBACK (glade_gtk_menu_shell_tool_item_get_display_name), NULL);
	g_signal_connect (editor, "child-selected", G_CALLBACK (glade_gtk_menu_shell_tool_item_child_selected), NULL);
	g_signal_connect (editor, "change-type", G_CALLBACK (glade_gtk_menu_shell_change_type), NULL);
	g_signal_connect (editor, "build-child", G_CALLBACK (glade_gtk_menu_shell_build_child), NULL);
	g_signal_connect (editor, "delete-child", G_CALLBACK (glade_gtk_menu_shell_delete_child), NULL);
	g_signal_connect (editor, "move-child", G_CALLBACK (glade_gtk_menu_shell_move_child), NULL);

	gtk_widget_show (GTK_WIDGET (editor));
	
	window = glade_base_editor_pack_new_window (editor, title, NULL);
	gtk_widget_show (window);
}

void
glade_gtk_menu_shell_action_activate (GladeWidgetAdaptor *adaptor,
				      GObject *object,
				      const gchar *action_path)
{
	if (strcmp (action_path, "launch_editor") == 0)
	{
		if (GTK_IS_MENU_BAR (object))
			glade_gtk_menu_shell_launch_editor (object, _("Edit Menu Bar"));
		else if (GTK_IS_MENU (object))
			glade_gtk_menu_shell_launch_editor (object, _("Edit Menu"));
	}
	else
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->action_activate (adaptor,
								     object,
								     action_path);

	gtk_menu_shell_deactivate (GTK_MENU_SHELL (object));
}

/* ----------------------------- GtkMenuItem ------------------------------ */

/* ... shared with toolitems ...  */
GladeEditable *
glade_gtk_activatable_create_editable  (GladeWidgetAdaptor  *adaptor,
				      GladeEditorPageType  type)
{
	GladeEditable *editable;

	/* Get base editable */
	editable = GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);

	if (type == GLADE_PAGE_GENERAL)
		return (GladeEditable *)glade_activatable_editor_new (adaptor, editable);

	return editable;
}

void
glade_gtk_menu_item_action_activate (GladeWidgetAdaptor *adaptor,
				     GObject *object,
				     const gchar *action_path)
{
	GObject *obj = NULL, *shell = NULL;
	GladeWidget *w = glade_widget_get_from_gobject (object);
		
	while ((w = glade_widget_get_parent (w)))
	{
		obj = glade_widget_get_object (w);
		if (GTK_IS_MENU_SHELL (obj)) shell = obj;
	}

	if (strcmp (action_path, "launch_editor") == 0)
	{
		if (shell)
			object = shell;

		if (GTK_IS_MENU_BAR (object))
			glade_gtk_menu_shell_launch_editor (object, _("Edit Menu Bar"));
		else if (GTK_IS_MENU (object))
			glade_gtk_menu_shell_launch_editor (object, _("Edit Menu"));
	}
	else
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->action_activate (adaptor,
								     object,
								     action_path);

	if (shell)
		gtk_menu_shell_deactivate (GTK_MENU_SHELL (shell));

}

GObject *
glade_gtk_menu_item_constructor (GType                  type,
				 guint                  n_construct_properties,
				 GObjectConstructParam *construct_properties)
{
	GladeWidgetAdaptor   *adaptor;
	GObject              *ret_obj;

	ret_obj = GWA_GET_OCLASS(GTK_TYPE_CONTAINER)->constructor
		(type, n_construct_properties, construct_properties);

	adaptor = GLADE_WIDGET_ADAPTOR (ret_obj);

	glade_widget_adaptor_action_remove (adaptor, "add_parent");
	glade_widget_adaptor_action_remove (adaptor, "remove_parent");

	return ret_obj;
}

void
glade_gtk_menu_item_post_create (GladeWidgetAdaptor *adaptor, 
				 GObject            *object, 
				 GladeCreateReason   reason)
{
	GladeWidget  *gitem;

	gitem = glade_widget_get_from_gobject (object);
	
	if (GTK_IS_SEPARATOR_MENU_ITEM (object)) return;
	
	if (gtk_bin_get_child (GTK_BIN (object)) == NULL)
	{
		GtkWidget *label = gtk_label_new ("");
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_container_add (GTK_CONTAINER (object), label);
	}

	if (reason == GLADE_CREATE_LOAD)
		g_signal_connect (G_OBJECT (gitem->project), "parse-finished",
				  G_CALLBACK (activatable_parse_finished),
				  gitem);
}

GList *
glade_gtk_menu_item_get_children (GladeWidgetAdaptor *adaptor,
				  GObject *object)
{
	GList *list = NULL;
	GtkWidget *child;
	
	g_return_val_if_fail (GTK_IS_MENU_ITEM (object), NULL);
	
	if ((child = gtk_menu_item_get_submenu (GTK_MENU_ITEM (object))))
		list = g_list_append (list, child);

	return list;
}

void
glade_gtk_menu_item_add_child (GladeWidgetAdaptor *adaptor,
			       GObject *object, GObject *child)
{
	g_return_if_fail (GTK_IS_MENU_ITEM (object));
	g_return_if_fail (GTK_IS_MENU (child));
	
	if (GTK_IS_SEPARATOR_MENU_ITEM (object))
	{
		g_warning ("You shouldn't try to add a GtkMenu to a GtkSeparatorMenuItem");
		return;
	}
	
	g_object_set_data (child,
			   "special-child-type",
			   "submenu");

	gtk_menu_item_set_submenu (GTK_MENU_ITEM (object), GTK_WIDGET (child));
}

void
glade_gtk_menu_item_remove_child (GladeWidgetAdaptor *adaptor,
				  GObject *object, GObject *child)
{
	g_return_if_fail (GTK_IS_MENU_ITEM (object));
	g_return_if_fail (GTK_IS_MENU (child));
	
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (object), NULL);
}

static void
glade_gtk_menu_item_set_label (GObject *object, const GValue *value)
{
	GladeWidget *gitem;
	GtkWidget   *label;
	gboolean     use_underline;

	gitem = glade_widget_get_from_gobject (object);

	label = gtk_bin_get_child (GTK_BIN (object));
	gtk_label_set_text (GTK_LABEL (label), g_value_get_string (value));

	/* Update underline incase... */
	glade_widget_property_get (gitem, "use-underline", &use_underline);
	gtk_label_set_use_underline (GTK_LABEL (label), use_underline);
}

static void
glade_gtk_menu_item_set_use_underline (GObject *object, const GValue *value)
{
	GtkWidget   *label;

	label = gtk_bin_get_child (GTK_BIN (object));
	gtk_label_set_use_underline (GTK_LABEL (label), g_value_get_boolean (value));
}


void
glade_gtk_menu_item_set_property (GladeWidgetAdaptor *adaptor,
				  GObject            *object, 
				  const gchar        *id,
				  const GValue       *value)
{
	GladeWidget *gwidget = glade_widget_get_from_gobject (object);
	GladeProperty *property = glade_widget_get_property (gwidget, id);
	
	evaluate_activatable_property_sensitivity (object, id, value);

	if (!strcmp (id, "use-underline"))
		glade_gtk_menu_item_set_use_underline (object, value);
	else if (!strcmp (id, "label"))
		glade_gtk_menu_item_set_label (object, value);
	else if (GPC_VERSION_CHECK (property->klass, gtk_major_version, gtk_minor_version + 1))
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object,
								  id, value);
}

static gboolean
write_special_child_submenu_item (GladeWidgetAdaptor *adaptor,
				  GladeWidget        *widget,
				  GladeXmlContext    *context,
				  GladeXmlNode       *node,
				  GladeWriteWidgetFunc write_func)
{
	gchar *special_child_type = NULL;
	GObject *child;

	if (glade_project_get_format (widget->project) == GLADE_PROJECT_FORMAT_LIBGLADE)
	{
		child = widget->object;
		if (child)
			special_child_type = g_object_get_data (child, "special-child-type");
	}

	if (special_child_type && !strcmp (special_child_type, "submenu"))
	{
		g_object_set_data (child, "special-child-type", NULL);
		write_func (adaptor, widget, context, node);
		g_object_set_data (child, "special-child-type", "submenu");
		return TRUE;
	}
	else
		return FALSE;
}

void
glade_gtk_menu_item_write_child (GladeWidgetAdaptor *adaptor,
			        GladeWidget        *widget,
			        GladeXmlContext    *context,
			        GladeXmlNode       *node)
{

	if (!write_special_child_submenu_item (adaptor, widget, context, node,
					       GWA_GET_CLASS(GTK_TYPE_CONTAINER)->write_child))
		/* Chain Up */
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->write_child (adaptor,
							   widget,
							   context,
							   node);
}

/* ----------------------------- GtkImageMenuItem ------------------------------ */

GList *
glade_gtk_image_menu_item_get_children (GladeWidgetAdaptor *adaptor,
					GObject *object)
{
	GList *list = NULL;
	GtkWidget *child;
	GladeWidget *gitem;
	
	gitem = glade_widget_get_from_gobject (object);
	
	if ((child = gtk_menu_item_get_submenu (GTK_MENU_ITEM (object))))
		list = g_list_append (list, child);
	
	if (glade_project_get_format (gitem->project) == GLADE_PROJECT_FORMAT_LIBGLADE &&
	    (child = gtk_image_menu_item_get_image (GTK_IMAGE_MENU_ITEM (object))))
		list = g_list_append (list, child);

	return list;
}

void
glade_gtk_image_menu_item_add_child (GladeWidgetAdaptor *adaptor,
				     GObject *object, GObject *child)
{
	g_return_if_fail (GTK_IS_MENU_ITEM (object));

	if (GTK_IS_IMAGE (child))
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (object), GTK_WIDGET (child));
	else
 		GWA_GET_CLASS (GTK_TYPE_MENU_ITEM)->add (adaptor, object, child);
}

void
glade_gtk_image_menu_item_remove_child (GladeWidgetAdaptor *adaptor,
					GObject *object, GObject *child)
{
	g_return_if_fail (GTK_IS_MENU_ITEM (object));
	
	if (GTK_IS_IMAGE (child))
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (object), NULL);
	else
 		GWA_GET_CLASS (GTK_TYPE_MENU_ITEM)->remove (adaptor, object, child);
}

static void
glade_gtk_image_menu_item_set_use_stock (GObject *object, const GValue *value)
{
	GladeWidget  *widget = glade_widget_get_from_gobject (object);
	gboolean      use_stock;
	
	use_stock = g_value_get_boolean (value);

	/* Set some things */
	if (use_stock)
	{
		glade_widget_property_set_sensitive (widget, "stock", TRUE, NULL);
		glade_widget_property_set_sensitive (widget, "accel-group", TRUE, NULL);
	}
	else
	{
		glade_widget_property_set_sensitive (widget, "stock", FALSE, NOT_SELECTED_MSG);
		glade_widget_property_set_sensitive (widget, "accel-group", FALSE, NOT_SELECTED_MSG);
	}

	gtk_image_menu_item_set_use_stock (GTK_IMAGE_MENU_ITEM (object), use_stock);

	sync_use_appearance (widget);
}

static gboolean
glade_gtk_image_menu_item_set_label (GObject *object, const GValue *value)
{
	GladeWidget *gitem;
	GtkWidget *label;
	gboolean use_underline = FALSE, use_stock = FALSE;
	const gchar *text;

	gitem = glade_widget_get_from_gobject (object);
	label = gtk_bin_get_child (GTK_BIN (object));

	glade_widget_property_get (gitem, "use-stock", &use_stock);
	glade_widget_property_get (gitem, "use-underline", &use_underline);
	text = g_value_get_string (value);

	/* In "use-stock" mode we dont have a GladeWidget child image */
	if (use_stock)
	{
		GtkWidget *image;
		GtkStockItem item;

		image = gtk_image_new_from_stock (g_value_get_string (value), GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (object), image);

		if (use_underline)
			gtk_label_set_use_underline (GTK_LABEL (label), TRUE);

		/* Get the label string... */
		if (text && gtk_stock_lookup (text, &item))
			gtk_label_set_label (GTK_LABEL (label), item.label);
		else
			gtk_label_set_label (GTK_LABEL (label), text ? text : "");

		return TRUE;
	} 
	
	return FALSE;
}

static void
glade_gtk_image_menu_item_set_stock (GObject *object, const GValue *value)
{
	GladeWidget *gitem;
	gboolean use_stock = FALSE;

	gitem = glade_widget_get_from_gobject (object);

	glade_widget_property_get (gitem, "use-stock", &use_stock);

	/* Forward the work along to the label handler...  */
	if (use_stock)
		glade_gtk_image_menu_item_set_label (object, value);
}

void
glade_gtk_image_menu_item_set_property (GladeWidgetAdaptor *adaptor,
					GObject            *object, 
					const gchar        *id,
					const GValue       *value)
{
	if (!strcmp (id, "stock"))
		glade_gtk_image_menu_item_set_stock (object, value);
	else if (!strcmp (id, "use-stock"))
		glade_gtk_image_menu_item_set_use_stock (object, value);
	else if (!strcmp (id, "label"))
	{
		if (!glade_gtk_image_menu_item_set_label (object, value))
			GWA_GET_CLASS (GTK_TYPE_MENU_ITEM)->set_property (adaptor, object,
									  id, value);
	}
	else
		GWA_GET_CLASS (GTK_TYPE_MENU_ITEM)->set_property (adaptor, object,
								  id, value);
}

static GladeWidget *
glade_gtk_image_menu_item_create_image (GladeWidget *gitem)
{
	GladeWidget *gimage;

	gimage = glade_widget_adaptor_create_widget 
		(glade_widget_adaptor_get_by_type (GTK_TYPE_IMAGE), FALSE,
		 "parent", gitem, 
		 "project", glade_widget_get_project (gitem), 
		 NULL);
	
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (gitem->object), 
				       GTK_WIDGET (gimage->object));

	glade_widget_lock (gitem, gimage);

	return gimage;
}

static void
glade_gtk_image_menu_item_fix_stock_item (GladeWidget *widget)
{
	GladeWidget *gimage;
	GEnumClass  *eclass;
	GEnumValue  *eval;
	gint         val = GNOMEUIINFO_MENU_NONE;
	gchar       *label = NULL, *icon = NULL;
	
	glade_widget_property_get (widget, "stock-item", &val);
	if (val == GNOMEUIINFO_MENU_NONE) return;
	
	/* Get the GEnumValue ... */
	eclass = g_type_class_ref (glade_gtk_gnome_ui_info_get_type ());
	if ((eval = g_enum_get_value (eclass, val)) == NULL)
	{
		g_type_class_unref (eclass);
		return;
	}
	g_type_class_unref (eclass);
	
	/* set use-underline */
	glade_widget_property_set (widget, "use-underline", TRUE);
	
	if (eval->value_nick)
	{
		glade_widget_property_set (widget, "use-stock", TRUE);
		glade_widget_property_set (widget, "label", eval->value_nick);		
		return;
	}
	
	switch (val)
	{
		case GNOMEUIINFO_MENU_PRINT_SETUP_ITEM:
			icon = "gtk-print";
			label = _("Print S_etup");
		break;
		case GNOMEUIINFO_MENU_FIND_AGAIN_ITEM:
			icon = "gtk-find";
			label = _("Find Ne_xt");
		break;
		case GNOMEUIINFO_MENU_UNDO_MOVE_ITEM:
			icon = "gtk-undo";
			label = _("_Undo Move");
		break;
		case GNOMEUIINFO_MENU_REDO_MOVE_ITEM:
			icon = "gtk-redo";
			label = _("_Redo Move");
		break;
		case GNOMEUIINFO_MENU_SELECT_ALL_ITEM:
			label =  _("Select _All");
		break;			
		case GNOMEUIINFO_MENU_NEW_GAME_ITEM:
			label =  _("_New Game");
		break;
		case GNOMEUIINFO_MENU_PAUSE_GAME_ITEM:
			label =  _("_Pause game");
		break;
		case GNOMEUIINFO_MENU_RESTART_GAME_ITEM:
			label =  _("_Restart Game");
		break;
		case GNOMEUIINFO_MENU_HINT_ITEM:
			label =  _("_Hint"); 
		break;
		case GNOMEUIINFO_MENU_SCORES_ITEM:
			label =  _("_Scores...");
		break;
		case GNOMEUIINFO_MENU_END_GAME_ITEM:
			label =  _("_End Game");
		break;
		case GNOMEUIINFO_MENU_NEW_WINDOW_ITEM:
			label =  _("Create New _Window");
		break;
		case GNOMEUIINFO_MENU_CLOSE_WINDOW_ITEM:
			label =  _("_Close This Window");
		break;
		case GNOMEUIINFO_MENU_FILE_TREE:
			label =  _("_File");
		break;
		case GNOMEUIINFO_MENU_EDIT_TREE:
			label =  _("_Edit");
		break;
		case GNOMEUIINFO_MENU_VIEW_TREE:
			label =  _("_View");
		break;
		case GNOMEUIINFO_MENU_SETTINGS_TREE:
			label =  _("_Settings");
		break;
		case GNOMEUIINFO_MENU_FILES_TREE:
			label =  _("Fi_les");
		break;
		case GNOMEUIINFO_MENU_WINDOWS_TREE:
			label =  _("_Windows");
		break;
		case GNOMEUIINFO_MENU_HELP_TREE:
			label =  _("_Help");
		break;
		case GNOMEUIINFO_MENU_GAME_TREE:
			label =  _("_Game");
		break;
		default:
			return;
		break;
	}

	if (icon)
	{
		gimage = glade_gtk_image_menu_item_create_image	(widget);
		glade_widget_property_set (gimage, "stock", icon);
		glade_widget_property_set (gimage, "icon-size", GTK_ICON_SIZE_MENU);
	}
	
	glade_widget_property_set (widget, "label", label);
}

static void
glade_gtk_image_menu_item_parse_finished (GladeProject *project, 
					  GladeWidget  *widget)
{
	GladeWidget *gimage;
	GtkWidget   *image = NULL;
	glade_widget_property_get (widget, "image", &image);
	
	if (image && (gimage = glade_widget_get_from_gobject (image)))
		glade_widget_lock (widget, gimage);
}

void
glade_gtk_image_menu_item_read_widget (GladeWidgetAdaptor *adaptor,
				       GladeWidget        *widget,
				       GladeXmlNode       *node)
{
	GladeProperty *property;
	gboolean use_stock;
	gchar *label = NULL;

	if (!glade_xml_node_verify 
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and read in all the normal properties.. */
        GWA_GET_CLASS (GTK_TYPE_MENU_ITEM)->read_widget (adaptor, widget, node);

	/* This will read legacy "stock-item" properties and make them usable */
	glade_gtk_image_menu_item_fix_stock_item (widget);

	glade_widget_property_get (widget, "use-stock", &use_stock);
	if (use_stock)
	{
		property = glade_widget_get_property (widget, "label");

		glade_property_get (property, &label);
		glade_widget_property_set (widget, "use-underline", TRUE);
		glade_widget_property_set (widget, "stock", label);
		glade_property_sync (property);
	}

	/* Update sensitivity of related properties...  */
	property = glade_widget_get_property (widget, "use-stock");
	glade_property_sync (property);


	/* Run this after the load so that image is resolved. */
	if (glade_project_get_format (widget->project) == GLADE_PROJECT_FORMAT_GTKBUILDER)
		g_signal_connect (G_OBJECT (widget->project), "parse-finished",
				  G_CALLBACK (glade_gtk_image_menu_item_parse_finished),
				  widget);
}


void
glade_gtk_image_menu_item_write_widget (GladeWidgetAdaptor *adaptor,
					GladeWidget        *widget,
					GladeXmlContext    *context,
					GladeXmlNode       *node)
{
	GladeProperty *label_prop;
	gboolean       use_stock;
	gchar         *stock;

	if (!glade_xml_node_verify
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* Make a copy of the GladeProperty, override its value if use-stock is TRUE */
	label_prop = glade_widget_get_property (widget, "label");
	label_prop = glade_property_dup (label_prop, widget);
	glade_widget_property_get (widget, "use-stock", &use_stock);
	if (use_stock)
	{
		glade_widget_property_get (widget, "stock", &stock);
		glade_property_set (label_prop, stock);
		glade_property_i18n_set_translatable (label_prop, FALSE);
	}
	glade_property_write (label_prop, context, node);
	g_object_unref (G_OBJECT (label_prop));

	/* Chain up and write all the normal properties ... */
        GWA_GET_CLASS (GTK_TYPE_MENU_ITEM)->write_widget (adaptor, widget, context, node);

}


/* Read in the internal "image" widgets as normal "locked" widgets...
 */
void
glade_gtk_image_menu_item_read_child (GladeWidgetAdaptor *adaptor,
				      GladeWidget        *widget,
				      GladeXmlNode       *node)
{
	GladeXmlNode *widget_node;
	GladeWidget  *child_widget;
	gchar        *internal_name;

	if (!glade_xml_node_verify (node, GLADE_XML_TAG_CHILD))
		return;

	internal_name = 
		glade_xml_get_property_string 
		(node, GLADE_XML_TAG_INTERNAL_CHILD);
	
	if ((widget_node = 
	     glade_xml_search_child
	     (node, GLADE_XML_TAG_WIDGET(glade_project_get_format(widget->project)))) != NULL)
	{
		/* Menu item children have no packing to take care of, just
		 * need to treat images a little different. */		
		if ((child_widget = glade_widget_read (widget->project, 
						       widget, widget_node, 
						       NULL)) != NULL)
		{
			if (GTK_IS_IMAGE (child_widget->object) &&
			    internal_name && strcmp (internal_name, "image") == 0)
				glade_widget_lock (widget, child_widget);

			glade_widget_add_child (widget, child_widget, FALSE);
		}
	}
	g_free (internal_name);
}

void
glade_gtk_image_menu_item_write_child (GladeWidgetAdaptor *adaptor,
				       GladeWidget        *widget,
				       GladeXmlContext    *context,
				       GladeXmlNode       *node)
{
	GladeXmlNode *child_node;

	if (glade_project_get_format (widget->project) == GLADE_PROJECT_FORMAT_GTKBUILDER ||
	    !GTK_IS_IMAGE (widget->object))
	{
		GWA_GET_CLASS (GTK_TYPE_MENU_ITEM)->write_child (adaptor, widget, context, node);
		return;
	}

	child_node = glade_xml_node_new (context, GLADE_XML_TAG_CHILD);
	glade_xml_node_append_child (node, child_node);

	/* Set fake internal child here */
	glade_xml_node_set_property_string (child_node, GLADE_XML_TAG_INTERNAL_CHILD, "image");

	/* Write out the widget (no packing properties) */
	glade_widget_write (widget, context, child_node);
}


/* We need write_widget to write child images as internal, in builder, they are
 * attached as a property
 */

GladeEditable *
glade_gtk_image_menu_item_create_editable  (GladeWidgetAdaptor  *adaptor,
					    GladeEditorPageType  type)
{
	GladeEditable *editable;

	/* Get base editable */
	editable = GWA_GET_CLASS (GTK_TYPE_MENU_ITEM)->create_editable (adaptor, type);

	if (type == GLADE_PAGE_GENERAL)
		return (GladeEditable *)glade_image_item_editor_new (adaptor, editable);

	return editable;
}

/* ----------------------------- GtkRadioMenuItem ------------------------------ */
static void
glade_gtk_radio_menu_item_set_group (GObject *object, const GValue *value)
{
	GObject *val;
	
	g_return_if_fail (GTK_IS_RADIO_MENU_ITEM (object));
	
	if ((val = g_value_get_object (value)))
	{
		GSList *group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (val));
		
		if (! g_slist_find (group, GTK_RADIO_MENU_ITEM (object)))
			gtk_radio_menu_item_set_group (GTK_RADIO_MENU_ITEM (object), group);
	}
}

void
glade_gtk_radio_menu_item_set_property (GladeWidgetAdaptor *adaptor,
					GObject            *object, 
					const gchar        *id,
					const GValue       *value)
{

	if (!strcmp (id, "group"))
		glade_gtk_radio_menu_item_set_group (object, value);
	else
		GWA_GET_CLASS (GTK_TYPE_MENU_ITEM)->set_property (adaptor, object,
								  id, value);
}

/* ----------------------------- GtkMenuBar ------------------------------ */
static GladeWidget * 
glade_gtk_menu_bar_append_new_submenu (GladeWidget *parent, GladeProject *project)
{
	static GladeWidgetAdaptor *submenu_adaptor = NULL;
	GladeWidget *gsubmenu;
	
	if (submenu_adaptor == NULL)
		submenu_adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_MENU);

	gsubmenu = glade_widget_adaptor_create_widget (submenu_adaptor, FALSE,
						       "parent", parent, 
						       "project", project, 
						       NULL);
	
	glade_widget_add_child (parent, gsubmenu, FALSE);

	return gsubmenu;
}

static GladeWidget * 
glade_gtk_menu_bar_append_new_item (GladeWidget *parent,
				    GladeProject *project,
				    const gchar *label,
				    gboolean use_stock)
{
	static GladeWidgetAdaptor *item_adaptor = NULL, *image_item_adaptor, *separator_adaptor;
	GladeWidget *gitem;
	
	if (item_adaptor == NULL)
	{
		item_adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_MENU_ITEM);
		image_item_adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_IMAGE_MENU_ITEM);
		separator_adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_SEPARATOR_MENU_ITEM);
	}

	if (label)
	{
		gitem = glade_widget_adaptor_create_widget ((use_stock) ? image_item_adaptor : item_adaptor,
							    FALSE, "parent", parent,
							    "project", project, 
							    NULL);

		glade_widget_property_set (gitem, "use-underline", TRUE);
	
		if (use_stock)
		{
			glade_widget_property_set (gitem, "use-stock", TRUE);
			glade_widget_property_set (gitem, "stock", label);
		}
		else
			glade_widget_property_set (gitem, "label", label);
	}
	else
	{
		gitem = glade_widget_adaptor_create_widget (separator_adaptor,
							    FALSE, "parent", parent,
							    "project", project, 
							    NULL);
	}
	
	glade_widget_add_child (parent, gitem, FALSE);
	
	return gitem;
}

void
glade_gtk_menu_bar_post_create (GladeWidgetAdaptor *adaptor,
				GObject            *object, 
				GladeCreateReason   reason)
{
	GladeProject *project;
	GladeWidget *gmenubar, *gitem, *gsubmenu;
	
	g_return_if_fail (GTK_IS_MENU_BAR (object));
	gmenubar = glade_widget_get_from_gobject (object);
	g_return_if_fail (GLADE_IS_WIDGET (gmenubar));
	
	if (reason != GLADE_CREATE_USER) return;
	
	project = glade_widget_get_project (gmenubar);
	
	/* File */
	gitem = glade_gtk_menu_bar_append_new_item (gmenubar, project, _("_File"), FALSE);	
	gsubmenu = glade_gtk_menu_bar_append_new_submenu (gitem, project);
	glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-new", TRUE);
	glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-open", TRUE);
	glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-save", TRUE);
	glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-save-as", TRUE);
	glade_gtk_menu_bar_append_new_item (gsubmenu, project, NULL, FALSE);
	glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-quit", TRUE);

	/* Edit */
	gitem = glade_gtk_menu_bar_append_new_item (gmenubar, project, _("_Edit"), FALSE);	
	gsubmenu = glade_gtk_menu_bar_append_new_submenu (gitem, project);
	glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-cut", TRUE);
	glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-copy", TRUE);
	glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-paste", TRUE);
	glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-delete", TRUE);
	
	/* View */
	gitem = glade_gtk_menu_bar_append_new_item (gmenubar, project, _("_View"), FALSE);	
	
	/* Help */
	gitem = glade_gtk_menu_bar_append_new_item (gmenubar, project, _("_Help"), FALSE);	
	gsubmenu = glade_gtk_menu_bar_append_new_submenu (gitem, project);
	glade_gtk_menu_bar_append_new_item (gsubmenu, project, "gtk-about", TRUE);
}

/* ----------------------------- GtkToolBar ------------------------------ */

/* need to unset/reset toolbar style when property is disabled/enabled */
static void
property_toolbar_style_notify_enabled (GladeProperty *property,
				       GParamSpec    *spec,
				       GtkToolbar    *toolbar)
{
	GtkToolbarStyle style;

	if (glade_property_get_enabled (property))
	{
		glade_property_get (property, &style);
		gtk_toolbar_set_style (toolbar, style);
	}
	else
		gtk_toolbar_unset_style (toolbar);
} 

void
glade_gtk_toolbar_post_create (GladeWidgetAdaptor *adaptor,
			       GObject            *object, 
			       GladeCreateReason   reason)
{
	GladeWidget   *widget;
	GladeProperty *toolbar_style_property;	
	
	widget = glade_widget_get_from_gobject (object);
	toolbar_style_property = glade_widget_get_property (widget, "toolbar-style");
	
	g_signal_connect (toolbar_style_property, "notify::enabled",
			  G_CALLBACK (property_toolbar_style_notify_enabled),
			  object);
}

void
glade_gtk_toolbar_get_child_property (GladeWidgetAdaptor *adaptor,
				      GObject            *container,
				      GObject            *child,
				      const gchar        *property_name,
				      GValue             *value)
{
	g_return_if_fail (GTK_IS_TOOLBAR (container));
	if (GTK_IS_TOOL_ITEM (child) == FALSE) return;
	
	if (strcmp (property_name, "position") == 0)
	{
		g_value_set_int (value,
			 gtk_toolbar_get_item_index (GTK_TOOLBAR (container), 
						     GTK_TOOL_ITEM (child)));
	}
	else
	{	/* Chain Up */
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->child_get_property (adaptor, 
								  container, child,
								  property_name, value);
	}
}

void
glade_gtk_toolbar_set_child_property (GladeWidgetAdaptor *adaptor,
				      GObject            *container,
				      GObject            *child,
				      const gchar        *property_name,
				      GValue             *value)
{
	g_return_if_fail (GTK_IS_TOOLBAR (container));
	g_return_if_fail (GTK_IS_TOOL_ITEM (child));

	g_return_if_fail (property_name != NULL || value != NULL);
	
	if (strcmp (property_name, "position") == 0)
	{
		GtkToolbar *toolbar = GTK_TOOLBAR (container);
		gint position, size;
		
		position = g_value_get_int (value);
		size = gtk_toolbar_get_n_items (toolbar);
		
		if (position >= size) position = size - 1;
		
		g_object_ref (child);
		gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET (child));
		gtk_toolbar_insert (toolbar, GTK_TOOL_ITEM (child), position);
		g_object_unref (child);
	}
	else
		/* Chain Up */
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->child_set_property (adaptor, 
								  container, child,
								  property_name, value);
}

void
glade_gtk_toolbar_add_child (GladeWidgetAdaptor *adaptor,
			     GObject *object, GObject *child)
{
	GtkToolbar *toolbar;
	GtkToolItem *item;
	
	g_return_if_fail (GTK_IS_TOOLBAR (object));
	g_return_if_fail (GTK_IS_TOOL_ITEM (child));
	
	toolbar = GTK_TOOLBAR (object);
	item = GTK_TOOL_ITEM (child);
	
	gtk_toolbar_insert (toolbar, item, -1);
		
	if (glade_util_object_is_loading (object))
	{
		GladeWidget *gchild = glade_widget_get_from_gobject (child);
		
		/* Packing props arent around when parenting during a glade_widget_dup() */
		if (gchild && gchild->packing_properties)
			glade_widget_pack_property_set (gchild, "position", 
				gtk_toolbar_get_item_index (toolbar, item));
	}
}

void
glade_gtk_toolbar_remove_child (GladeWidgetAdaptor *adaptor, 
				GObject *object, GObject *child)
{
	gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
}

static void
glade_gtk_toolbar_launch_editor (GladeWidgetAdaptor *adaptor, 
				 GObject            *toolbar)
{
	GladeBaseEditor *editor;
	GtkWidget *window;

	/* Editor */
	editor = glade_base_editor_new (toolbar, NULL,
					_("Button"), GTK_TYPE_TOOL_BUTTON,
					_("Toggle"), GTK_TYPE_TOGGLE_TOOL_BUTTON,
					_("Radio"), GTK_TYPE_RADIO_TOOL_BUTTON,
					_("Menu"), GTK_TYPE_MENU_TOOL_BUTTON,
					_("Custom"), GTK_TYPE_TOOL_ITEM,
					_("Separator"), GTK_TYPE_SEPARATOR_TOOL_ITEM,
					NULL);


	glade_base_editor_append_types (editor, GTK_TYPE_MENU_TOOL_BUTTON,
					_("Normal"), GTK_TYPE_MENU_ITEM,
					_("Image"), GTK_TYPE_IMAGE_MENU_ITEM,
					_("Check"), GTK_TYPE_CHECK_MENU_ITEM,
					_("Radio"), GTK_TYPE_RADIO_MENU_ITEM,
					_("Separator"), GTK_TYPE_SEPARATOR_MENU_ITEM,
					NULL);
	
	glade_base_editor_append_types (editor, GTK_TYPE_MENU_ITEM,
					_("Normal"), GTK_TYPE_MENU_ITEM,
					_("Image"), GTK_TYPE_IMAGE_MENU_ITEM,
					_("Check"), GTK_TYPE_CHECK_MENU_ITEM,
					_("Radio"), GTK_TYPE_RADIO_MENU_ITEM,
					_("Separator"), GTK_TYPE_SEPARATOR_MENU_ITEM,
					NULL);

	g_signal_connect (editor, "get-display-name", G_CALLBACK (glade_gtk_menu_shell_tool_item_get_display_name), NULL);
	g_signal_connect (editor, "child-selected", G_CALLBACK (glade_gtk_menu_shell_tool_item_child_selected), NULL);
	g_signal_connect (editor, "change-type", G_CALLBACK (glade_gtk_menu_shell_change_type), NULL);
	g_signal_connect (editor, "build-child", G_CALLBACK (glade_gtk_menu_shell_build_child), NULL);
	g_signal_connect (editor, "delete-child", G_CALLBACK (glade_gtk_menu_shell_delete_child), NULL);
	g_signal_connect (editor, "move-child", G_CALLBACK (glade_gtk_menu_shell_move_child), NULL);

	gtk_widget_show (GTK_WIDGET (editor));
	
	window = glade_base_editor_pack_new_window (editor, _("Tool Bar Editor"), NULL);
	gtk_widget_show (window);
}

void
glade_gtk_toolbar_action_activate (GladeWidgetAdaptor *adaptor,
				   GObject *object,
				   const gchar *action_path)
{
	if (strcmp (action_path, "launch_editor") == 0)
	{
		glade_gtk_toolbar_launch_editor (adaptor, object);
	}
	else
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->action_activate (adaptor,
								     object,
								     action_path);
}

/* ----------------------------- GtkToolPalette ------------------------------ */
void
glade_gtk_tool_palette_get_child_property (GladeWidgetAdaptor * adaptor,
                                      GObject * container,
                                      GObject * child,
                                      const gchar * property_name,
                                      GValue * value)
{
  g_return_if_fail (GTK_IS_TOOL_PALETTE (container));
  if (GTK_IS_TOOL_ITEM_GROUP (child) == FALSE)
    return;

  if (strcmp (property_name, "position") == 0)
    {
      g_value_set_int (value,
                       gtk_tool_palette_get_group_position (GTK_TOOL_PALETTE (container),
							    GTK_TOOL_ITEM_GROUP (child)));
    }
  else
    {                           /* Chain Up */
      GWA_GET_CLASS
          (GTK_TYPE_CONTAINER)->child_get_property (adaptor,
                                                    container, child,
                                                    property_name, value);
    }
}

void
glade_gtk_tool_palette_set_child_property (GladeWidgetAdaptor * adaptor,
					   GObject * container,
					   GObject * child,
					   const gchar * property_name,
					   GValue * value)
{
  g_return_if_fail (GTK_IS_TOOL_PALETTE (container));
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (child));

  g_return_if_fail (property_name != NULL || value != NULL);

  if (strcmp (property_name, "position") == 0)
    {
      GtkToolPalette *palette = GTK_TOOL_PALETTE (container);
      GList *children;
      gint position, size;

      children = glade_util_container_get_all_children (GTK_CONTAINER (palette));
      size = g_list_length (children);
      g_list_free (children);

      position = g_value_get_int (value);

      if (position >= size)
        position = size - 1;

      gtk_tool_palette_set_group_position (palette, GTK_TOOL_ITEM_GROUP (child), position);
    }
  else
    /* Chain Up */
    GWA_GET_CLASS
        (GTK_TYPE_CONTAINER)->child_set_property (adaptor,
                                                  container, child,
                                                  property_name, value);
}

void
glade_gtk_tool_palette_add_child (GladeWidgetAdaptor * adaptor,
				  GObject * object, GObject * child)
{
  GtkToolPalette *palette;
  GtkToolItemGroup *group;

  g_return_if_fail (GTK_IS_TOOL_PALETTE (object));
  g_return_if_fail (GTK_IS_TOOL_ITEM_GROUP (child));

  palette = GTK_TOOL_PALETTE (object);
  group   = GTK_TOOL_ITEM_GROUP (child);

  gtk_container_add (GTK_CONTAINER (palette), GTK_WIDGET (group));

  if (glade_util_object_is_loading (object))
    {
      GladeWidget *gchild = glade_widget_get_from_gobject (child);

      /* Packing props arent around when parenting during a glade_widget_dup() */
      if (gchild && gchild->packing_properties)
	glade_widget_pack_property_set (gchild, "position",
					gtk_tool_palette_get_group_position (palette, group));
    }
}

void
glade_gtk_tool_palette_remove_child (GladeWidgetAdaptor * adaptor,
				     GObject * object, GObject * child)
{
  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
}

static void
glade_gtk_tool_palette_launch_editor (GladeWidgetAdaptor * adaptor,
				      GObject * palette)
{
  GladeBaseEditor *editor;
  GtkWidget *window;

  /* Editor */
  editor = glade_base_editor_new (palette, NULL,
				  _("Group"), GTK_TYPE_TOOL_ITEM_GROUP,
				  NULL);

  glade_base_editor_append_types (editor, GTK_TYPE_TOOL_ITEM_GROUP,
                                  _("Button"), GTK_TYPE_TOOL_BUTTON,
                                  _("Toggle"), GTK_TYPE_TOGGLE_TOOL_BUTTON,
                                  _("Radio"), GTK_TYPE_RADIO_TOOL_BUTTON,
                                  _("Menu"), GTK_TYPE_MENU_TOOL_BUTTON,
                                  _("Custom"), GTK_TYPE_TOOL_ITEM,
                                  _("Separator"), GTK_TYPE_SEPARATOR_TOOL_ITEM,
                                  NULL);

  glade_base_editor_append_types (editor, GTK_TYPE_MENU_TOOL_BUTTON,
                                  _("Normal"), GTK_TYPE_MENU_ITEM,
                                  _("Image"), GTK_TYPE_IMAGE_MENU_ITEM,
                                  _("Check"), GTK_TYPE_CHECK_MENU_ITEM,
                                  _("Radio"), GTK_TYPE_RADIO_MENU_ITEM,
                                  _("Separator"), GTK_TYPE_SEPARATOR_MENU_ITEM,
                                  NULL);

  glade_base_editor_append_types (editor, GTK_TYPE_MENU_ITEM,
                                  _("Normal"), GTK_TYPE_MENU_ITEM,
                                  _("Image"), GTK_TYPE_IMAGE_MENU_ITEM,
                                  _("Check"), GTK_TYPE_CHECK_MENU_ITEM,
                                  _("Radio"), GTK_TYPE_RADIO_MENU_ITEM,
                                  _("Separator"), GTK_TYPE_SEPARATOR_MENU_ITEM,
                                  NULL);

  g_signal_connect (editor, "get-display-name",
                    G_CALLBACK (glade_gtk_menu_shell_tool_item_get_display_name), NULL);
  g_signal_connect (editor, "child-selected",
                    G_CALLBACK (glade_gtk_menu_shell_tool_item_child_selected),
                    NULL);
  g_signal_connect (editor, "change-type",
                    G_CALLBACK (glade_gtk_menu_shell_change_type), NULL);
  g_signal_connect (editor, "build-child",
                    G_CALLBACK (glade_gtk_menu_shell_build_child), NULL);
  g_signal_connect (editor, "delete-child",
                    G_CALLBACK (glade_gtk_menu_shell_delete_child), NULL);
  g_signal_connect (editor, "move-child",
                    G_CALLBACK (glade_gtk_menu_shell_move_child), NULL);

  gtk_widget_show (GTK_WIDGET (editor));

  window =
      glade_base_editor_pack_new_window (editor, _("Tool Palette Editor"), NULL);
  gtk_widget_show (window);
}

void
glade_gtk_tool_palette_action_activate (GladeWidgetAdaptor * adaptor,
					GObject * object, const gchar * action_path)
{
  if (strcmp (action_path, "launch_editor") == 0)
    {
      glade_gtk_tool_palette_launch_editor (adaptor, object);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->action_activate (adaptor,
                                                         object, action_path);
}

/* ----------------------------- GtkToolItemGroup ------------------------------ */
void
glade_gtk_tool_item_group_add_child (GladeWidgetAdaptor * adaptor,
				     GObject * object, GObject * child)
{
  gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));
}

void
glade_gtk_tool_item_group_remove_child (GladeWidgetAdaptor * adaptor,
				     GObject * object, GObject * child)
{
  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));
}

static void
glade_gtk_tool_item_group_parse_finished (GladeProject * project,
					  GladeWidget * widget)
{
  GtkWidget *label_widget = NULL;

  glade_widget_property_get (widget, "label-widget", &label_widget);

  if (label_widget)
    glade_widget_property_set (widget, "custom-label", TRUE);
  else
    glade_widget_property_set (widget, "custom-label", FALSE);
}

void
glade_gtk_tool_item_group_read_widget (GladeWidgetAdaptor * adaptor,
				       GladeWidget * widget, GladeXmlNode * node)
{
  if (!glade_xml_node_verify (node, GLADE_XML_TAG_WIDGET(glade_project_get_format(widget->project))))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (GTK_TYPE_TOOL_ITEM)->read_widget (adaptor, widget, node);

  /* Run this after the load so that icon-widget is resolved. */
  g_signal_connect (glade_widget_get_project (widget),
                    "parse-finished",
                    G_CALLBACK (glade_gtk_tool_item_group_parse_finished), widget);
}

static void
glade_gtk_tool_item_group_set_custom_label (GObject * object, const GValue * value)
{
  GladeWidget *gbutton;

  gbutton = glade_widget_get_from_gobject (object);

  glade_widget_property_set_sensitive (gbutton, "label", FALSE,
                                       NOT_SELECTED_MSG);
  glade_widget_property_set_sensitive (gbutton, "label-widget", FALSE,
                                       NOT_SELECTED_MSG);

  if (g_value_get_boolean (value))
    glade_widget_property_set_sensitive (gbutton, "label-widget", TRUE, NULL);
  else
    glade_widget_property_set_sensitive (gbutton, "label", TRUE, NULL);
}

void
glade_gtk_tool_item_group_set_property (GladeWidgetAdaptor * adaptor,
                                        GObject * object,
                                        const gchar * id, const GValue * value)
{
  if (!strcmp (id, "custom-label"))
    glade_gtk_tool_item_group_set_custom_label (object, value);
  else if (!strcmp (id, "label-widget"))
    {
      if (g_value_get_object (value) != NULL)
	gtk_tool_item_group_set_label_widget (GTK_TOOL_ITEM_GROUP (object),
					      (GtkWidget *)g_value_get_object (value));
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id, value);
}

GladeEditable *
glade_gtk_tool_item_group_create_editable (GladeWidgetAdaptor * adaptor,
                                           GladeEditorPageType type)
{
  GladeEditable *editable;

  /* Get base editable */
  editable =
      GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);

  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_tool_item_group_editor_new (adaptor, editable);

  return editable;
}

/* ----------------------------- GtkToolItem ------------------------------ */
GObject *
glade_gtk_tool_item_constructor (GType                  type,
				 guint                  n_construct_properties,
				 GObjectConstructParam *construct_properties)
{
	GladeWidgetAdaptor   *adaptor;
	GObject              *ret_obj;
	
	ret_obj = GWA_GET_OCLASS(GTK_TYPE_CONTAINER)->constructor
		(type, n_construct_properties, construct_properties);

	adaptor = GLADE_WIDGET_ADAPTOR (ret_obj);

	glade_widget_adaptor_action_remove (adaptor, "add_parent");
	glade_widget_adaptor_action_remove (adaptor, "remove_parent");

	return ret_obj;
}

void
glade_gtk_tool_item_post_create (GladeWidgetAdaptor *adaptor, 
				 GObject            *object, 
				 GladeCreateReason   reason)
{
	GladeWidget *gitem = glade_widget_get_from_gobject (object);
	
	if (GTK_IS_SEPARATOR_TOOL_ITEM (object)) return;
	
	if (reason == GLADE_CREATE_USER &&
	    gtk_bin_get_child (GTK_BIN (object)) == NULL)
		gtk_container_add (GTK_CONTAINER (object),
				   glade_placeholder_new ());

	if (reason == GLADE_CREATE_LOAD)
		g_signal_connect (G_OBJECT (gitem->project), "parse-finished",
				  G_CALLBACK (activatable_parse_finished),
				  gitem);
}

void
glade_gtk_tool_item_set_property (GladeWidgetAdaptor *adaptor,
				  GObject            *object, 
				  const gchar        *id,
				  const GValue       *value)
{
	GladeWidget *gwidget = glade_widget_get_from_gobject (object);
	GladeProperty *property = glade_widget_get_property (gwidget, id);

	evaluate_activatable_property_sensitivity (object, id, value);

	if (GPC_VERSION_CHECK (property->klass, gtk_major_version, gtk_minor_version + 1))
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor,
								  object,
								  id, value);
}

/* ----------------------------- GtkToolButton ------------------------------ */
GladeEditable *
glade_gtk_tool_button_create_editable  (GladeWidgetAdaptor  *adaptor,
					GladeEditorPageType  type)
{
	GladeEditable *editable;

	/* Get base editable */
	editable = GWA_GET_CLASS (GTK_TYPE_TOOL_ITEM)->create_editable (adaptor, type);

	if (type == GLADE_PAGE_GENERAL)
		return (GladeEditable *)glade_tool_button_editor_new (adaptor, editable);

	return editable;
}

static void
glade_gtk_tool_button_set_image_mode (GObject *object, const GValue *value)
{
	GladeWidget *gbutton;
	
	g_return_if_fail (GTK_IS_TOOL_BUTTON (object));
	gbutton = glade_widget_get_from_gobject (object);

	glade_widget_property_set_sensitive (gbutton, "stock-id", FALSE, NOT_SELECTED_MSG);
	glade_widget_property_set_sensitive (gbutton, "icon-name", FALSE, NOT_SELECTED_MSG);
	glade_widget_property_set_sensitive (gbutton, "icon", FALSE, NOT_SELECTED_MSG);
	glade_widget_property_set_sensitive (gbutton, "icon-widget", FALSE, NOT_SELECTED_MSG);
	
	switch (g_value_get_int (value))
	{
	case GLADE_TB_MODE_STOCK:
		glade_widget_property_set_sensitive (gbutton, "stock-id", TRUE, NULL);
		break;
	case GLADE_TB_MODE_ICON:
		glade_widget_property_set_sensitive (gbutton, "icon-name", TRUE, NULL);
		break;
	case GLADE_TB_MODE_FILENAME:
		glade_widget_property_set_sensitive (gbutton, "icon", TRUE, NULL);
		break;
	case GLADE_TB_MODE_CUSTOM:
		glade_widget_property_set_sensitive (gbutton, "icon-widget", TRUE, NULL);
		break;
	default:
		break;
	}
}


static void
glade_gtk_tool_button_set_custom_label (GObject *object, const GValue *value)
{
	GladeWidget *gbutton;
	
	g_return_if_fail (GTK_IS_TOOL_BUTTON (object));
	gbutton = glade_widget_get_from_gobject (object);

	glade_widget_property_set_sensitive (gbutton, "label", FALSE, NOT_SELECTED_MSG);
	glade_widget_property_set_sensitive (gbutton, "label-widget", FALSE, NOT_SELECTED_MSG);

	if (g_value_get_boolean (value))
		glade_widget_property_set_sensitive (gbutton, "label-widget", TRUE, NULL);
	else
		glade_widget_property_set_sensitive (gbutton, "label", TRUE, NULL);
}

static void
glade_gtk_tool_button_set_label (GObject *object, const GValue *value)
{
	const gchar *label;
	
	g_return_if_fail (GTK_IS_TOOL_BUTTON (object));
	
	label = g_value_get_string (value);
	
	if (label && strlen (label) == 0) label = NULL;
	
	gtk_tool_button_set_label (GTK_TOOL_BUTTON (object), label);
}

static void
glade_gtk_tool_button_set_stock_id (GObject *object, const GValue *value)
{
	const gchar *stock_id;
	
	g_return_if_fail (GTK_IS_TOOL_BUTTON (object));
		
	stock_id = g_value_get_string (value);
	
	if (stock_id && strlen (stock_id) == 0) stock_id = NULL;
	
	gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON (object), stock_id);
}

/* legacy libglade property */
static void
glade_gtk_tool_button_set_icon (GObject *object, const GValue *value)
{
	GladeWidget *gbutton;
	GObject *pixbuf;
	GtkWidget *image = NULL;
	
	g_return_if_fail (GTK_IS_TOOL_BUTTON (object));
	gbutton = glade_widget_get_from_gobject (object);
	
	if ((pixbuf = g_value_get_object (value)))
	{
		image = gtk_image_new_from_pixbuf (GDK_PIXBUF (pixbuf));
		gtk_widget_show (image);
	}
	gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (object), image);
}

static void
glade_gtk_tool_button_set_icon_name (GObject *object, const GValue *value)
{
	const gchar *name;
	
	g_return_if_fail (GTK_IS_TOOL_BUTTON (object));

	name = g_value_get_string (value);

	if (name && strlen (name) == 0) name = NULL;
		
	gtk_tool_button_set_icon_name (GTK_TOOL_BUTTON (object), name);
}

void
glade_gtk_tool_button_set_property (GladeWidgetAdaptor *adaptor,
				    GObject            *object, 
				    const gchar        *id,
				    const GValue       *value)
{
	if (!strcmp (id, "image-mode"))
		glade_gtk_tool_button_set_image_mode (object, value);
	else if (!strcmp (id, "icon-name"))
		glade_gtk_tool_button_set_icon_name (object, value);
	else if (!strcmp (id, "icon"))
		glade_gtk_tool_button_set_icon (object, value);
	else if (!strcmp (id, "stock-id"))
		glade_gtk_tool_button_set_stock_id (object, value);
	else if (!strcmp (id, "label"))
		glade_gtk_tool_button_set_label (object, value);
	else if (!strcmp (id, "custom-label"))
		glade_gtk_tool_button_set_custom_label (object, value);
	else
		GWA_GET_CLASS (GTK_TYPE_TOOL_ITEM)->set_property (adaptor,
								  object,
								  id, value);
}

static void
glade_gtk_tool_button_parse_finished (GladeProject *project, 
				      GladeWidget  *widget)
{
	gchar     *stock_str = NULL, *icon_name = NULL;
	gint       stock_id = 0;
	GdkPixbuf *pixbuf = NULL;
	GtkWidget *label_widget = NULL, *image_widget = NULL;

	glade_widget_property_get (widget, "stock-id", &stock_str);
	glade_widget_property_get (widget, "icon-name", &icon_name);
	glade_widget_property_get (widget, "icon", &pixbuf);
	glade_widget_property_get (widget, "icon-widget", &image_widget);
	glade_widget_property_get (widget, "label-widget", &label_widget);

	if (label_widget)
		glade_widget_property_set (widget, "custom-label", TRUE);
	else
		glade_widget_property_set (widget, "custom-label", FALSE);
	
	if (image_widget)
		glade_widget_property_set (widget, "image-mode", GLADE_TB_MODE_CUSTOM);
	else if (pixbuf)
		glade_widget_property_set (widget, "image-mode", GLADE_TB_MODE_FILENAME);
	else if (icon_name)
		glade_widget_property_set (widget, "image-mode", GLADE_TB_MODE_ICON);
	else if (stock_str)
	{
		/* Update the stock property */
		stock_id = glade_utils_enum_value_from_string (GLADE_TYPE_STOCK_IMAGE, stock_str);
		if (stock_id < 0)
			stock_id = 0;
		glade_widget_property_set (widget, "glade-stock", stock_id);

		glade_widget_property_set (widget, "image-mode", GLADE_TB_MODE_STOCK);
	}
	else
		glade_widget_property_set (widget, "image-mode", GLADE_TB_MODE_STOCK);
}

void
glade_gtk_tool_button_read_widget (GladeWidgetAdaptor *adaptor,
				   GladeWidget        *widget,
				   GladeXmlNode       *node)
{
	if (!glade_xml_node_verify 
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and read in all the normal properties.. */
        GWA_GET_CLASS (GTK_TYPE_TOOL_ITEM)->read_widget (adaptor, widget, node);

	/* Run this after the load so that icon-widget is resolved. */
	g_signal_connect (glade_widget_get_project (widget),
			  "parse-finished",
			  G_CALLBACK (glade_gtk_tool_button_parse_finished),
			  widget);
}

/* ----------------------------- GtkMenuToolButton ------------------------------ */
GList *
glade_gtk_menu_tool_button_get_children (GladeWidgetAdaptor *adaptor, GtkMenuToolButton *button)
{
	GList *list = NULL;
	GtkWidget *menu = gtk_menu_tool_button_get_menu (button);

	list = glade_util_container_get_all_children (GTK_CONTAINER (button));

	/* Ensure that we only return one 'menu' */
	if (menu && g_list_find (list, menu) == NULL)
		list = g_list_append (list, menu);

	return list;
}

void
glade_gtk_menu_tool_button_add_child (GladeWidgetAdaptor *adaptor,
				      GObject *object, 
				      GObject *child)
{
	if (GTK_IS_MENU (child))
	{
		g_object_set_data (child, "special-child-type", "menu");

		gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (object),
					       GTK_WIDGET (child));
	}
	else
		GWA_GET_CLASS (GTK_TYPE_TOOL_BUTTON)->add (adaptor, object, child);
}

void
glade_gtk_menu_tool_button_remove_child (GladeWidgetAdaptor *adaptor,
					 GObject *object, 
					 GObject *child)
{
	if (GTK_IS_MENU (child))
	{
		g_object_set_data (child, "special-child-type", NULL);

		gtk_menu_tool_button_set_menu (GTK_MENU_TOOL_BUTTON (object), NULL);
	}
	else
		GWA_GET_CLASS (GTK_TYPE_TOOL_BUTTON)->remove (adaptor, object, child);
}

/* ----------------------------- GtkLabel ------------------------------ */
void
glade_gtk_label_post_create (GladeWidgetAdaptor *adaptor,
			     GObject            *object, 
			     GladeCreateReason   reason)
{
	GladeWidget *glabel = glade_widget_get_from_gobject (object);

	if (reason == GLADE_CREATE_USER)
		glade_widget_property_set_sensitive (glabel, "mnemonic-widget", FALSE, MNEMONIC_INSENSITIVE_MSG);
}


static void
glade_gtk_label_set_label (GObject *object, const GValue *value)
{
	GladeWidget *glabel;
	gboolean use_markup = FALSE, use_underline = FALSE;

	glabel = glade_widget_get_from_gobject (object);
	glade_widget_property_get (glabel, "use-markup", &use_markup);
	
	if (use_markup)
		gtk_label_set_markup (GTK_LABEL (object), g_value_get_string (value));
	else
		gtk_label_set_text (GTK_LABEL (object), g_value_get_string (value));
	
	glade_widget_property_get (glabel, "use-underline", &use_underline);
	if (use_underline)
		gtk_label_set_use_underline (GTK_LABEL (object), use_underline);
}

static void
glade_gtk_label_set_attributes (GObject *object, const GValue *value)
{
	GladeAttribute       *gattr;
	PangoAttribute       *attribute;
	PangoLanguage        *language;
	PangoAttrList        *attrs = NULL;
	GdkColor             *color;
	GList                *list;

	for (list = g_value_get_boxed (value); list; list = list->next)
	{
		gattr = list->data;

		attribute = NULL;

		switch (gattr->type)
		{
			/* PangoAttrLanguage */
		case PANGO_ATTR_LANGUAGE:
			if ((language = pango_language_from_string (g_value_get_string (&gattr->value))))
				attribute = pango_attr_language_new (language);
			break;
			/* PangoAttrInt */
		case PANGO_ATTR_STYLE:
			attribute = pango_attr_style_new (g_value_get_enum (&(gattr->value)));
			break;
		case PANGO_ATTR_WEIGHT:
			attribute = pango_attr_weight_new (g_value_get_enum (&(gattr->value)));
			break;
		case PANGO_ATTR_VARIANT:
			attribute = pango_attr_variant_new (g_value_get_enum (&(gattr->value)));
			break;
		case PANGO_ATTR_STRETCH:
			attribute = pango_attr_stretch_new (g_value_get_enum (&(gattr->value)));
			break;
		case PANGO_ATTR_UNDERLINE:
			attribute = pango_attr_underline_new (g_value_get_boolean (&(gattr->value)));
			break;
		case PANGO_ATTR_STRIKETHROUGH:	
			attribute = pango_attr_strikethrough_new (g_value_get_boolean (&(gattr->value)));
			break;
		case PANGO_ATTR_GRAVITY:
			attribute = pango_attr_gravity_new (g_value_get_enum (&(gattr->value)));
			break;
		case PANGO_ATTR_GRAVITY_HINT:
			attribute = pango_attr_gravity_hint_new (g_value_get_enum (&(gattr->value)));
			break;
			
			/* PangoAttrString */	  
		case PANGO_ATTR_FAMILY:
			attribute = pango_attr_family_new (g_value_get_string (&(gattr->value)));
			break;
			
			/* PangoAttrSize */	  
		case PANGO_ATTR_SIZE:
			attribute = pango_attr_size_new (g_value_get_int (&(gattr->value)));
			break;
		case PANGO_ATTR_ABSOLUTE_SIZE:
			attribute = pango_attr_size_new_absolute (g_value_get_int (&(gattr->value)));
			break;
						
			/* PangoAttrColor */
		case PANGO_ATTR_FOREGROUND:
			color = g_value_get_boxed (&(gattr->value));
			attribute = pango_attr_foreground_new (color->red, color->green, color->blue);
			break;
		case PANGO_ATTR_BACKGROUND: 
			color = g_value_get_boxed (&(gattr->value));
			attribute = pango_attr_background_new (color->red, color->green, color->blue);
			break;
		case PANGO_ATTR_UNDERLINE_COLOR:
			color = g_value_get_boxed (&(gattr->value));
			attribute = pango_attr_underline_color_new (color->red, color->green, color->blue);
			break;
		case PANGO_ATTR_STRIKETHROUGH_COLOR:
			color = g_value_get_boxed (&(gattr->value));
			attribute = pango_attr_strikethrough_color_new (color->red, color->green, color->blue);
			break;
			
			/* PangoAttrShape */
		case PANGO_ATTR_SHAPE:
			/* Unsupported for now */
			break;
			/* PangoAttrFloat */
		case PANGO_ATTR_SCALE:
			attribute = pango_attr_scale_new (g_value_get_double (&(gattr->value)));
			break;

		case PANGO_ATTR_INVALID:
		case PANGO_ATTR_LETTER_SPACING:
		case PANGO_ATTR_RISE:
		case PANGO_ATTR_FALLBACK:
		case PANGO_ATTR_FONT_DESC:
		default:
			break;
		}

		if (attribute)
		{
			if (!attrs)
				attrs = pango_attr_list_new ();
			pango_attr_list_insert (attrs, attribute);

		}
	}

	gtk_label_set_attributes (GTK_LABEL (object), attrs);
}


static void
glade_gtk_label_set_content_mode (GObject *object, const GValue *value)
{
	GladeLabelContentMode mode = g_value_get_int (value);
	GladeWidget *glabel;
	
	glabel = glade_widget_get_from_gobject (object);

	glade_widget_property_set_sensitive (glabel, "glade-attributes", FALSE, NOT_SELECTED_MSG);
	glade_widget_property_set_sensitive (glabel, "use-markup", FALSE, NOT_SELECTED_MSG);
	glade_widget_property_set_sensitive (glabel, "pattern", FALSE, NOT_SELECTED_MSG);

	switch (mode)
	{
	case GLADE_LABEL_MODE_ATTRIBUTES:
		glade_widget_property_set_sensitive (glabel, "glade-attributes", TRUE, NULL);
		break;
	case GLADE_LABEL_MODE_MARKUP:
		glade_widget_property_set_sensitive (glabel, "use-markup", TRUE, NULL);
		break;
	case GLADE_LABEL_MODE_PATTERN:
		glade_widget_property_set_sensitive (glabel, "pattern", TRUE, NULL);
		break;
	default:
		break;
	}
}

static void
glade_gtk_label_set_use_max_width (GObject *object, const GValue *value)
{
	GladeWidget *glabel;
	
	glabel = glade_widget_get_from_gobject (object);

	glade_widget_property_set_sensitive (glabel, "width-chars", FALSE, NOT_SELECTED_MSG);
	glade_widget_property_set_sensitive (glabel, "max-width-chars", FALSE, NOT_SELECTED_MSG);

	if (g_value_get_boolean (value))
		glade_widget_property_set_sensitive (glabel, "max-width-chars", TRUE, NULL);
	else
		glade_widget_property_set_sensitive (glabel, "width-chars", TRUE, NULL);
}


static void
glade_gtk_label_set_wrap_mode (GObject *object, const GValue *value)
{
	GladeLabelWrapMode mode = g_value_get_int (value);
	GladeWidget *glabel;
	
	glabel = glade_widget_get_from_gobject (object);

	glade_widget_property_set_sensitive (glabel, "single-line-mode", FALSE, NOT_SELECTED_MSG);
	glade_widget_property_set_sensitive (glabel, "wrap-mode", FALSE, NOT_SELECTED_MSG);
	
	if (mode == GLADE_LABEL_SINGLE_LINE)
		glade_widget_property_set_sensitive (glabel, "single-line-mode", TRUE, NULL);
	else if (mode == GLADE_LABEL_WRAP_MODE)
		glade_widget_property_set_sensitive (glabel, "wrap-mode", TRUE, NULL);
}

static void
glade_gtk_label_set_use_underline (GObject *object, const GValue *value)
{
	GladeWidget *glabel;
	
	glabel = glade_widget_get_from_gobject (object);

	if (g_value_get_boolean (value))
		glade_widget_property_set_sensitive (glabel, "mnemonic-widget", TRUE, NULL);
	else
		glade_widget_property_set_sensitive (glabel, "mnemonic-widget", FALSE, MNEMONIC_INSENSITIVE_MSG);

	gtk_label_set_use_underline (GTK_LABEL (object), g_value_get_boolean (value));
}

static void
glade_gtk_label_set_ellipsize (GObject *object, const GValue *value)
{
	GladeWidget *glabel;
	const gchar *insensitive_msg = _("This property does not apply when Ellipsize is set.");
	
	glabel = glade_widget_get_from_gobject (object);

	if (!glade_widget_property_original_default (glabel, "ellipsize"))
		glade_widget_property_set_sensitive (glabel, "angle", FALSE, insensitive_msg);
	else
		glade_widget_property_set_sensitive (glabel, "angle", TRUE, NULL);

	gtk_label_set_ellipsize (GTK_LABEL (object), g_value_get_enum (value));
}


static void
glade_gtk_label_set_angle (GObject *object, const GValue *value)
{
	GladeWidget *glabel;
	const gchar *insensitive_msg = _("This property does not apply when Angle is set.");
	
	glabel = glade_widget_get_from_gobject (object);

	if (!glade_widget_property_original_default (glabel, "angle"))
		glade_widget_property_set_sensitive (glabel, "ellipsize", FALSE, insensitive_msg);
	else
		glade_widget_property_set_sensitive (glabel, "ellipsize", TRUE, NULL);

	gtk_label_set_angle (GTK_LABEL (object), g_value_get_double (value));
}

void
glade_gtk_label_set_property (GladeWidgetAdaptor *adaptor,
			      GObject            *object, 
			      const gchar        *id,
			      const GValue       *value)
{
	if (!strcmp (id, "label"))
		glade_gtk_label_set_label (object, value);
	else if (!strcmp (id, "glade-attributes"))
		glade_gtk_label_set_attributes (object, value);
	else if (!strcmp (id, "label-content-mode"))
		glade_gtk_label_set_content_mode (object, value);
	else if (!strcmp (id, "use-max-width"))
		glade_gtk_label_set_use_max_width (object, value);
	else if (!strcmp (id, "label-wrap-mode"))
		glade_gtk_label_set_wrap_mode (object, value);
	else if (!strcmp (id, "use-underline"))
		glade_gtk_label_set_use_underline (object, value);
	else if (!strcmp (id, "ellipsize"))
		glade_gtk_label_set_ellipsize (object, value);
	else if (!strcmp (id, "angle"))
		glade_gtk_label_set_angle (object, value);
	else
		GWA_GET_CLASS (GTK_TYPE_WIDGET)->set_property (adaptor, object, id, value);
}

static void
glade_gtk_parse_attributes (GladeWidget  *widget, 
			    GladeXmlNode *node)
{
	PangoAttrType   attr_type;
	GladeXmlNode   *prop;
	GladeAttribute *attr;
	GList          *attrs = NULL;
	gchar          *name, *value;

	for (prop = glade_xml_node_get_children (node); 
	     prop; prop = glade_xml_node_next (prop))
	{
		if (!glade_xml_node_verify (prop, GLADE_TAG_ATTRIBUTE))
			continue;

		if (!(name = glade_xml_get_property_string_required
		      (prop, GLADE_XML_TAG_NAME, NULL)))
			continue;

		if (!(value = glade_xml_get_property_string_required
		      (prop, GLADE_TAG_VALUE, NULL)))
		{
			/* for a while, Glade was broken and was storing
			 * attributes in the node contents */
			if (!(value = glade_xml_get_content (prop)))
			{
				g_free (name);
				continue;
			}
		}

		if ((attr_type = 
		     glade_utils_enum_value_from_string (PANGO_TYPE_ATTR_TYPE, name)) == 0)
			continue;

		/* Parse attribute and add to list */
		if ((attr = glade_gtk_attribute_from_string (attr_type, value)) != NULL)
			attrs = g_list_prepend (attrs, attr);

		/* XXX deal with start/end here ... */
					    
		g_free (name);
		g_free (value);
	}

	glade_widget_property_set (widget, "glade-attributes", g_list_reverse (attrs));
	glade_attr_list_free (attrs);
}

static void
glade_gtk_label_read_attributes (GladeWidget  *widget,
				 GladeXmlNode *node)
{
	GladeXmlNode  *attrs_node;

	if ((attrs_node = 
	     glade_xml_search_child (node, GLADE_TAG_ATTRIBUTES)) != NULL)
	{
		/* Generic attributes parsing */
		glade_gtk_parse_attributes (widget, attrs_node);
	}
}

void
glade_gtk_label_read_widget (GladeWidgetAdaptor *adaptor,
			     GladeWidget        *widget,
			     GladeXmlNode       *node)
{
	GladeProperty *prop;
	if (!glade_xml_node_verify 
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and read in all the normal properties.. */
        GWA_GET_CLASS (GTK_TYPE_WIDGET)->read_widget (adaptor, widget, node);

	glade_gtk_label_read_attributes (widget, node);

	/* sync label property after a load... */
	prop = glade_widget_get_property (widget, "label");
	glade_gtk_label_set_label (widget->object, prop->value);

	/* Resolve "label-content-mode" virtual control property  */
	if (!glade_widget_property_original_default (widget, "use-markup"))
		glade_widget_property_set (widget, "label-content-mode", GLADE_LABEL_MODE_MARKUP);
	else if (!glade_widget_property_original_default (widget, "pattern"))
		glade_widget_property_set (widget, "label-content-mode", GLADE_LABEL_MODE_PATTERN);
	else 
		glade_widget_property_set (widget, "label-content-mode", GLADE_LABEL_MODE_ATTRIBUTES);

	/* Resolve "label-wrap-mode" virtual control property  */
	if (!glade_widget_property_original_default (widget, "single-line-mode"))
		glade_widget_property_set (widget, "label-wrap-mode", GLADE_LABEL_SINGLE_LINE);
	else if (!glade_widget_property_original_default (widget, "wrap"))
		glade_widget_property_set (widget, "label-wrap-mode", GLADE_LABEL_WRAP_MODE);
	else 
		glade_widget_property_set (widget, "label-wrap-mode", GLADE_LABEL_WRAP_FREE);

	/* Resolve "use-max-width" virtual control property  */
	if (!glade_widget_property_original_default (widget, "max-width-chars"))
		glade_widget_property_set (widget, "use-max-width", TRUE);
	else
		glade_widget_property_set (widget, "use-max-width", TRUE);
	
	if (glade_widget_property_original_default (widget, "use-markup"))
		glade_widget_property_set_sensitive (widget, "mnemonic-widget", 
						     FALSE, MNEMONIC_INSENSITIVE_MSG);

}

static void
glade_gtk_label_write_attributes (GladeWidget        *widget,
				  GladeXmlContext    *context,
				  GladeXmlNode       *node)
{
	GladeXmlNode       *attr_node;
	GList              *attrs = NULL, *l;
	GladeAttribute     *gattr;
	gchar              *attr_type;
	gchar              *attr_value;

	if (!glade_widget_property_get (widget, "glade-attributes", &attrs) || !attrs)
		return;

	for (l = attrs; l; l = l->next)
	{
		gattr = l->data;

		attr_type = glade_utils_enum_string_from_value (PANGO_TYPE_ATTR_TYPE, gattr->type);
		attr_value = glade_gtk_string_from_attr (gattr);

		attr_node = glade_xml_node_new (context, GLADE_TAG_ATTRIBUTE);
		glade_xml_node_append_child (node, attr_node);

		glade_xml_node_set_property_string (attr_node, GLADE_TAG_NAME, attr_type);
		glade_xml_node_set_property_string (attr_node, GLADE_TAG_VALUE, attr_value);
	}
}

void
glade_gtk_label_write_widget (GladeWidgetAdaptor *adaptor,
			      GladeWidget        *widget,
			      GladeXmlContext    *context,
			      GladeXmlNode       *node)
{
	GladeXmlNode       *attrs_node;

	if (!glade_xml_node_verify
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and read in all the normal properties.. */
        GWA_GET_CLASS (GTK_TYPE_WIDGET)->write_widget (adaptor, widget, context, node);

	attrs_node = glade_xml_node_new (context, GLADE_TAG_ATTRIBUTES);

	glade_gtk_label_write_attributes (widget, context, attrs_node);

	if (!glade_xml_node_get_children (attrs_node))
		glade_xml_node_delete (attrs_node);
	else
		glade_xml_node_append_child (node, attrs_node);

}

gchar *
glade_gtk_label_string_from_value (GladeWidgetAdaptor *adaptor,
				   GladePropertyClass *klass,
				   const GValue       *value,
				   GladeProjectFormat  fmt)
{
	if (klass->pspec->value_type == GLADE_TYPE_ATTR_GLIST)
	{
		GList *l, *list = g_value_get_boxed (value);
		GString *string = g_string_new ("");
		gchar *str;
		
		for (l = list; l; l = g_list_next (l))
		{
			GladeAttribute *attr = l->data;
			
			/* Return something usefull at least to for the backend to compare */
			gchar *attr_str = glade_gtk_string_from_attr (attr);
			g_string_append_printf (string, "%d=%s ", attr->type, attr_str);
			g_free (attr_str);
		}
		str = string->str;
		g_string_free (string, FALSE);		
		return str;
	}
	else
		return GWA_GET_CLASS 
			(GTK_TYPE_WIDGET)->string_from_value (adaptor, 
							      klass, 
							      value,
							      fmt);
}


GladeEditorProperty *
glade_gtk_label_create_eprop (GladeWidgetAdaptor *adaptor,
			      GladePropertyClass *klass,
			      gboolean            use_command)
{
	GladeEditorProperty *eprop;

	/* chain up.. */
	if (klass->pspec->value_type == GLADE_TYPE_ATTR_GLIST)
	{
		eprop = g_object_new (GLADE_TYPE_EPROP_ATTRS,
				      "property-class", klass, 
				      "use-command", use_command,
				      NULL);
	}
	else
		eprop = GWA_GET_CLASS 
			(GTK_TYPE_WIDGET)->create_eprop (adaptor, 
						       klass, 
						       use_command);
	return eprop;
}

GladeEditable *
glade_gtk_label_create_editable (GladeWidgetAdaptor  *adaptor,
				 GladeEditorPageType  type)
{
	GladeEditable *editable;

	/* Get base editable */
	editable = GWA_GET_CLASS (GTK_TYPE_WIDGET)->create_editable (adaptor, type);

	if (type == GLADE_PAGE_GENERAL)
		return (GladeEditable *)glade_label_editor_new (adaptor, editable);

	return editable;
}



/* ----------------------------- GtkTextBuffer ------------------------------ */
static void
glade_gtk_entry_buffer_changed (GtkTextBuffer *buffer, 
				GParamSpec    *pspec,
				GladeWidget  *gbuffy)
{
	const gchar *text_prop = NULL;
	GladeProperty *prop;
	gchar *text = NULL;
	
	if (glade_widget_superuser ())
		return;

	g_object_get (buffer, "text", &text, NULL);

	if ((prop = glade_widget_get_property (gbuffy, "text")))
	{
		glade_property_get (prop, &text_prop);

		if (text_prop == NULL || g_strcmp0 (text, text_prop))
			glade_command_set_property (prop, text);
	}
	g_free (text);
}

void
glade_gtk_entry_buffer_post_create (GladeWidgetAdaptor *adaptor,
				    GObject            *object, 
				    GladeCreateReason   reason)
{
	GladeWidget *gbuffy;
	
	gbuffy = glade_widget_get_from_gobject (object);
	
	g_signal_connect (object, "notify::text",
			  G_CALLBACK (glade_gtk_entry_buffer_changed),
			  gbuffy);
}


void
glade_gtk_entry_buffer_set_property (GladeWidgetAdaptor *adaptor,
				     GObject            *object, 
				     const gchar        *id,
				     const GValue       *value)
{
	GladeWidget *gwidget = glade_widget_get_from_gobject (object);
	GladeProperty *property = glade_widget_get_property (gwidget, id);

	if (!strcmp (id, "text"))
	{
		g_signal_handlers_block_by_func (object, glade_gtk_entry_buffer_changed, gwidget);

		if (g_value_get_string (value))
			gtk_entry_buffer_set_text (GTK_ENTRY_BUFFER (object), g_value_get_string (value), -1);
		else
			gtk_entry_buffer_set_text (GTK_ENTRY_BUFFER (object), "", -1);

		g_signal_handlers_unblock_by_func (object, glade_gtk_entry_buffer_changed, gwidget);

       	}
	else if (GPC_VERSION_CHECK (property->klass, gtk_major_version, gtk_minor_version + 1))
		GWA_GET_CLASS (G_TYPE_OBJECT)->set_property (adaptor, object, id, value);
}



/* ----------------------------- GtkTextBuffer ------------------------------ */
static void
glade_gtk_text_buffer_changed (GtkTextBuffer *buffer, GladeWidget *gbuffy)
{
	const gchar *text_prop = NULL;
	GladeProperty *prop;
	gchar *text = NULL;
	
	g_object_get (buffer, "text", &text, NULL);

	if ((prop = glade_widget_get_property (gbuffy, "text")))
	{
		glade_property_get (prop, &text_prop);

		if (g_strcmp0 (text, text_prop))
			glade_command_set_property (prop, text);
	}
	g_free (text);
}

void
glade_gtk_text_buffer_post_create (GladeWidgetAdaptor *adaptor,
				   GObject            *object, 
				   GladeCreateReason   reason)
{
	GladeWidget *gbuffy;
	
	gbuffy = glade_widget_get_from_gobject (object);
	
	g_signal_connect (object, "changed",
			  G_CALLBACK (glade_gtk_text_buffer_changed),
			  gbuffy);
}


void
glade_gtk_text_buffer_set_property (GladeWidgetAdaptor *adaptor,
				    GObject            *object, 
				    const gchar        *id,
				    const GValue       *value)
{
	GladeWidget   *gwidget = glade_widget_get_from_gobject (object);
	GladeProperty *property = glade_widget_get_property (gwidget, id);

	if (!strcmp (id, "text"))
	{
		g_signal_handlers_block_by_func (object, glade_gtk_text_buffer_changed, gwidget);

		if (g_value_get_string (value))
			gtk_text_buffer_set_text (GTK_TEXT_BUFFER (object), g_value_get_string (value), -1);
		else
			gtk_text_buffer_set_text (GTK_TEXT_BUFFER (object), "", -1);

		g_signal_handlers_unblock_by_func (object, glade_gtk_text_buffer_changed, gwidget);

       	}
	else if (GPC_VERSION_CHECK (property->klass, gtk_major_version, gtk_minor_version + 1))
		GWA_GET_CLASS (G_TYPE_OBJECT)->set_property (adaptor, object, id, value);
}

/* ----------------------------- GtkTextView ------------------------------ */
static void
glade_gtk_text_view_changed (GtkTextBuffer *buffer, GladeWidget *gtext)
{
	const gchar *text_prop = NULL;
	GladeProject  *project;
	GladeProperty *prop;
	gchar *text = NULL;
	
	if (buffer)
 		g_object_get (buffer, "text", &text, NULL);

	project = glade_widget_get_project (gtext);

	if (glade_project_get_format (project) == GLADE_PROJECT_FORMAT_LIBGLADE)
	{
       		if ((prop = glade_widget_get_property (gtext, "text")))
		{
			glade_property_get (prop, &text_prop);
	
			if (text_prop == NULL || text == NULL || strcmp (text, text_prop))
				glade_command_set_property (prop, text);
		}
	}
	g_free (text);
}

static gboolean
glade_gtk_text_view_stop_double_click (GtkWidget *widget,
				       GdkEventButton *event,
				       gpointer user_data)
{
	/* Return True if the event is double or triple click */
	return (event->type == GDK_2BUTTON_PRESS ||
		event->type == GDK_3BUTTON_PRESS);
}

void
glade_gtk_text_view_post_create (GladeWidgetAdaptor *adaptor,
				 GObject            *object, 
				 GladeCreateReason   reason)
{
	GtkTextBuffer *buffy;
	GladeWidget   *gtext;
	GladeProject  *project;
	
	gtext = glade_widget_get_from_gobject (object);
	
	/* This makes gtk_text_view_set_buffer() stop complaing */
	gtk_drag_dest_set (GTK_WIDGET (object), 0, NULL, 0, 0);

	project = glade_widget_get_project (gtext);

	if (glade_project_get_format (project) == GLADE_PROJECT_FORMAT_LIBGLADE)
	{
		buffy = gtk_text_buffer_new (NULL);		
		gtk_text_view_set_buffer (GTK_TEXT_VIEW (object), buffy);
		g_signal_connect (buffy, "changed",
				  G_CALLBACK (glade_gtk_text_view_changed),
				  gtext);
	
		g_object_unref (G_OBJECT (buffy));
	}

	/* Glade3 hangs when a TextView gets a double click. So we stop them */
	g_signal_connect (object, "button-press-event",
			  G_CALLBACK (glade_gtk_text_view_stop_double_click),
			  NULL);
}

static void
glade_gtk_text_view_set_buffer (GObject *object, const GValue *value)
{
	GtkTextBuffer *buffy;
	GladeWidget *gtext;

	gtext = glade_widget_get_from_gobject (object);
	
	if ((buffy = g_value_get_object (value)) == NULL) return;

	g_signal_handlers_block_by_func (buffy, glade_gtk_text_view_changed, gtext);
	gtk_text_view_set_buffer (GTK_TEXT_VIEW (object), buffy);
	g_signal_handlers_unblock_by_func (buffy, glade_gtk_text_view_changed, gtext);
}

static void
glade_gtk_text_view_set_text (GObject *object, const GValue *value)
{
	GtkTextBuffer *buffy;
	GladeWidget *gtext;
	const gchar *text;
	GladeProject *project;

	gtext = glade_widget_get_from_gobject (object);

	project = glade_widget_get_project (gtext);
	if (glade_project_get_format (project) != GLADE_PROJECT_FORMAT_LIBGLADE)
		return;
	
	buffy = gtk_text_view_get_buffer (GTK_TEXT_VIEW (object));
	if (buffy == NULL) return;
	
	if ((text = g_value_get_string (value)) == NULL) return;

	g_signal_handlers_block_by_func (buffy, glade_gtk_text_view_changed, gtext);
	gtk_text_buffer_set_text (buffy, text, -1);
	g_signal_handlers_unblock_by_func (buffy, glade_gtk_text_view_changed, gtext);
}

void
glade_gtk_text_view_set_property (GladeWidgetAdaptor *adaptor,
				  GObject            *object, 
				  const gchar        *id,
				  const GValue       *value)
{
	if (!strcmp (id, "text"))
		glade_gtk_text_view_set_text (object, value);
	else if (!strcmp (id, "buffer"))
		glade_gtk_text_view_set_buffer (object, value);
	else
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor,
								  object,
								  id, value);
}


/* ----------------------------- GtkComboBox ------------------------------ */
static void
combo_ensure_model (GObject *combo)
{
	GtkListStore *store;

	if (!gtk_combo_box_get_model (GTK_COMBO_BOX (combo)))
	{
		/* Add store */
		store = gtk_list_store_new (1, G_TYPE_STRING);
		gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (store));
		g_object_unref (store);
	}
}

void
glade_gtk_combo_box_post_create (GladeWidgetAdaptor *adaptor,
				 GObject            *object, 
				 GladeCreateReason   reason)
{
	GtkCellRenderer *cell;
	GladeWidget     *widget = glade_widget_get_from_gobject (object);

	if (glade_project_get_format (widget->project) == GLADE_PROJECT_FORMAT_LIBGLADE)
	{

		combo_ensure_model (object);

		/* Add cell renderer */
		cell = gtk_cell_renderer_text_new ();
		gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (object), cell, TRUE);
		gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (object), cell,
						"text", 0, NULL);
	}
}

static void
glade_gtk_combo_box_set_items (GObject *object, const GValue *value)
{
	GtkComboBox *combo  = GTK_COMBO_BOX (object);
	GladeWidget *widget = glade_widget_get_from_gobject (object);
	gchar **split;
	gint    i;

	if (glade_project_get_format (widget->project) != GLADE_PROJECT_FORMAT_LIBGLADE)
		return;

	combo_ensure_model (object);

	/* Empty the combo box */
	gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (combo)));

	/* Refill the combo box */
	split = g_value_get_boxed (value);

	if (split)
		for (i = 0; split[i] != NULL; i++)
			if (split[i][0] != '\0')
				gtk_combo_box_append_text (combo, split[i]);
}

void
glade_gtk_combo_box_set_property (GladeWidgetAdaptor *adaptor,
				  GObject            *object, 
				  const gchar        *id,
				  const GValue       *value)
{
	if (!strcmp (id, "items"))
		glade_gtk_combo_box_set_items (object, value);
	else if (!strcmp (id, "entry-text-column"))
	{
		/* Avoid warnings */
		if (g_value_get_int (value) >= 0)
			GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor,
									  object,
									  id, value);
	}
	else
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor,
								  object,
								  id, value);
}

GList *glade_gtk_cell_layout_get_children (GladeWidgetAdaptor  *adaptor, GObject  *container);

GList *
glade_gtk_combo_box_get_children (GladeWidgetAdaptor *adaptor, GtkComboBox *combo)
{
	GList *list = NULL;

	list = glade_gtk_cell_layout_get_children (adaptor, G_OBJECT (combo));

	/* return the internal entry.
	 *
	 * FIXME: for recent gtk+ we have no comboboxentry
	 * but a "has-entry" property instead
	 */
	if (GTK_IS_COMBO_BOX_ENTRY (combo))
		list = g_list_append (list, gtk_bin_get_child (GTK_BIN (combo)));

	return list;
}

/* ----------------------------- GtkComboBoxEntry ------------------------------ */
void
glade_gtk_combo_box_entry_post_create (GladeWidgetAdaptor *adaptor,
				       GObject            *object, 
				       GladeCreateReason   reason)
{
	GladeWidget *gcombo = glade_widget_get_from_gobject (object);

	/* Chain up */
	GWA_GET_CLASS (GTK_TYPE_CONTAINER)->post_create (adaptor, object, reason);

	glade_widget_adaptor_create_internal
		(gcombo, G_OBJECT (gtk_bin_get_child (GTK_BIN (object))),
		 "entry", "comboboxentry", FALSE, reason);
}

GObject *
glade_gtk_combo_box_entry_get_internal_child (GladeWidgetAdaptor *adaptor,
					      GObject *object, 
					      const gchar *name)
{
	GObject *child = NULL;
	g_return_val_if_fail (GTK_IS_COMBO_BOX_ENTRY (object), NULL);
	
	if (strcmp ("entry", name) == 0)
		child = G_OBJECT (gtk_bin_get_child (GTK_BIN (object)));

	return child;
}


void
glade_gtk_combo_box_entry_set_property (GladeWidgetAdaptor *adaptor,
					GObject            *object, 
					const gchar        *id,
					const GValue       *value)
{
	if (!strcmp (id, "text-column"))
	{
		if (g_value_get_int (value) >= 0)
			gtk_combo_box_entry_set_text_column (GTK_COMBO_BOX_ENTRY (object), 
							     g_value_get_int (value));
	}
	else
		GWA_GET_CLASS (GTK_TYPE_COMBO_BOX)->set_property (adaptor,
								  object,
								  id, value);
}

/* ----------------------------- GtkSpinButton ------------------------------ */
static void
glade_gtk_spin_button_set_adjustment (GObject *object, const GValue *value)
{
	GObject *adjustment;
	GtkAdjustment *adj;
	
	g_return_if_fail (GTK_IS_SPIN_BUTTON (object));
		
	adjustment = g_value_get_object (value);
	
	if (adjustment && GTK_IS_ADJUSTMENT (adjustment))
	{
		adj = GTK_ADJUSTMENT (adjustment);

		if (gtk_adjustment_get_page_size (adj) > 0)
		{
			GladeWidget *gadj = glade_widget_get_from_gobject (adj);

			/* It can be with an old file the GladeWidget is not built yet at load time */
			if (gadj)
				/* Silently set any spin-button adjustment page size to 0 */
				glade_widget_property_set (gadj, "page-size", 0.0F);

			gtk_adjustment_set_page_size (adj, 0);
		}

		gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (object), adj);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (object),
					   gtk_adjustment_get_value (adj));
	}
}

void
glade_gtk_spin_button_set_property (GladeWidgetAdaptor *adaptor,
				    GObject            *object, 
				    const gchar        *id,
				    const GValue       *value)
{
	if (!strcmp (id, "adjustment"))
		glade_gtk_spin_button_set_adjustment (object, value);
	else
		GWA_GET_CLASS (GTK_TYPE_ENTRY)->set_property (adaptor,
							      object,
							      id, value);
}

/* ----------------------------- GtkCombo ------------------------------ */
void
glade_gtk_combo_post_create (GladeWidgetAdaptor *adaptor,
			     GObject            *object,
			     GladeCreateReason   reason)
{
	GladeWidget  *gcombo, *gentry, *glist;

	g_return_if_fail (GTK_IS_COMBO (object));

	if ((gcombo = glade_widget_get_from_gobject (object)) == NULL)
		return;
	
	gentry = glade_widget_adaptor_create_internal
		(gcombo, G_OBJECT (GTK_COMBO (object)->entry),
		 "entry", "combo", FALSE, reason);

	/* We mark this 'anarchist' since its outside of the hierarchy */
	glist  = glade_widget_adaptor_create_internal
		(gcombo, G_OBJECT (GTK_COMBO (object)->list),
		 "list", "combo", TRUE, reason);

}

GObject *
glade_gtk_combo_get_internal_child (GladeWidgetAdaptor *adaptor,
				    GtkCombo           *combo,
				    const gchar        *name)
{
	GObject *child = NULL;

	g_return_val_if_fail (GTK_IS_COMBO (combo), NULL);
	
	if (strcmp ("list", name) == 0)
		child = G_OBJECT (combo->list);
	else if (strcmp ("entry", name) == 0)
		child = G_OBJECT (combo->entry);

	return child;
}

GList *
glade_gtk_combo_get_children (GladeWidgetAdaptor *adaptor, GtkCombo *combo)
{
	GList *list = NULL;

	g_return_val_if_fail (GTK_IS_COMBO (combo), NULL);

	list = glade_util_container_get_all_children (GTK_CONTAINER (combo));

	/* Ensure that we only return one 'combo->list' */
	if (g_list_find (list, combo->list) == NULL)
		list = g_list_append (list, combo->list);

	return list;
}

/* ----------------------------- GtkOptionMenu ------------------------------ */
void
glade_gtk_option_menu_add_child (GladeWidgetAdaptor  *adaptor,
				 GObject             *object, 
				 GObject             *child)
{
	if (GTK_IS_MENU (child))
		gtk_option_menu_set_menu (GTK_OPTION_MENU (object), GTK_WIDGET (child));
}

void
glade_gtk_option_menu_remove_child (GladeWidgetAdaptor  *adaptor,
				    GObject             *object, 
				    GObject             *child)
{
	if (GTK_IS_MENU (child))
		gtk_option_menu_remove_menu (GTK_OPTION_MENU (object));
}

GList *
glade_gtk_option_menu_get_children (GladeWidgetAdaptor *adaptor, 
				    GtkOptionMenu      *option_menu)
{
	GList *list = NULL;
	GtkWidget *menu;

	menu = gtk_option_menu_get_menu (option_menu);

	if (menu)
		list = g_list_prepend (list, menu);

	return list;
}

/* ----------------------------- GtkListItem ------------------------------ */
void
glade_gtk_list_item_post_create (GladeWidgetAdaptor *adaptor,
				 GObject            *object, 
				 GladeCreateReason   reason)
{
	GtkWidget *label;

	g_return_if_fail (GTK_IS_LIST_ITEM (object));

	label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 0, 1);

	gtk_container_add (GTK_CONTAINER (object), label);
	gtk_widget_show (label);
}

static void
glade_gtk_list_item_set_label (GObject *object, const GValue *value)
{
	GtkWidget *label;

	g_return_if_fail (GTK_IS_LIST_ITEM (object));

	label = gtk_bin_get_child (GTK_BIN (object));

	gtk_label_set_text (GTK_LABEL (label), g_value_get_string (value));
}

static void
glade_gtk_list_item_get_label (GObject *object, GValue *value)
{
	GtkWidget *label;

	g_return_if_fail (GTK_IS_LIST_ITEM (object));

	label = gtk_bin_get_child (GTK_BIN (object));

	g_value_set_string (value, gtk_label_get_text (GTK_LABEL (label)));
}

void
glade_gtk_list_item_set_property (GladeWidgetAdaptor *adaptor,
				  GObject            *object, 
				  const gchar        *id,
				  const GValue       *value)
{
	if (!strcmp (id, "label"))
		glade_gtk_list_item_set_label (object, value);
	else
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor,
								  object,
								  id, value);
}

void
glade_gtk_list_item_get_property (GladeWidgetAdaptor *adaptor,
				  GObject            *object, 
				  const gchar        *id,
				  GValue             *value)
{
	if (!strcmp (id, "label"))
		glade_gtk_list_item_get_label (object, value);
	else
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor,
								  object,
								  id, value);
}


void
glade_gtk_listitem_add_child (GladeWidgetAdaptor  *adaptor,
			      GObject             *object, 
			      GObject             *child)
{
	g_return_if_fail (GTK_IS_CONTAINER (object));
	g_return_if_fail (GTK_IS_WIDGET (child));

	gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));
}

void
glade_gtk_listitem_remove_child (GladeWidgetAdaptor  *adaptor,
				 GObject             *object, 
				 GObject             *child)
{
	g_return_if_fail (GTK_IS_CONTAINER (object));
	g_return_if_fail (GTK_IS_WIDGET (child));

	gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));
}

/* ------------------------------ GtkAssistant ------------------------------ */
static void
glade_gtk_assistant_append_new_page (GladeWidget *parent,
				     GladeProject *project,
				     const gchar *label,
				     GtkAssistantPageType type)
{
	static GladeWidgetAdaptor *adaptor = NULL;
	GladeWidget *page;
	
	if (adaptor == NULL)
		adaptor = glade_widget_adaptor_get_by_type (GTK_TYPE_LABEL);

	page = glade_widget_adaptor_create_widget (adaptor, FALSE,
						   "parent", parent,
						   "project", project, 
						   NULL);

	glade_widget_add_child (parent, page, FALSE);
		
	glade_widget_property_set (page, "label", label);
	glade_widget_pack_property_set (page, "page-type", type);
}
/*
  GtkAssistant is a very weird widget, why is it derived from GtkWindow
  instead of GtkNotebook I do not know!

  If there is no GTK_ASSISTANT_PAGE_CONFIRM, GtkAssistant abort when trying to 
  update its navigation buttons!
*/
static void
glade_gtk_assistant_update_page_type (GtkAssistant *assistant)
{
	gint i, current, pages;
	GtkWidget *page;
	
	current = gtk_assistant_get_current_page (assistant);
	pages = gtk_assistant_get_n_pages (assistant) - 1;
	if (pages < 0) return;

	/* Last Page */
	page = gtk_assistant_get_nth_page (assistant, pages);
	gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONFIRM);
	
	/* First page */
	page = gtk_assistant_get_nth_page (assistant, 0);
	gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_INTRO);
	
	/* In betwen pages */	
	for (i = 1; i < pages; i++)
	{
		page = gtk_assistant_get_nth_page (assistant, i);
		gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONTENT);

	}
	
	/* Now we have set page-type in every page, force button update */
	for (i = 0; i <= pages; i++)
	{
		page = gtk_assistant_get_nth_page (assistant, i);
		gtk_assistant_set_page_complete (assistant, page, TRUE);
		gtk_assistant_set_current_page (assistant, i);
		gtk_assistant_update_buttons_state (assistant);
	}
	
	if (current >= 0) gtk_assistant_set_current_page (assistant, current);
}

static gint
glade_gtk_assistant_get_page (GtkAssistant *assistant, GtkWidget *page)
{
	gint i, pages = gtk_assistant_get_n_pages (assistant);
		
	for (i = 0; i < pages; i++)
		if (gtk_assistant_get_nth_page (assistant, i) == page)
			return i;
	
	return -1;
}

static void
glade_gtk_assistant_update_position (GtkAssistant *assistant)
{
	gint i, pages = gtk_assistant_get_n_pages (assistant);
		
	for (i = 0; i < pages; i++)
	{
		GtkWidget *page =  gtk_assistant_get_nth_page (assistant, i);
		GladeWidget *gpage = glade_widget_get_from_gobject (G_OBJECT (page));
		if (gpage) glade_widget_pack_property_set (gpage, "position", i);
	}
}

static void
glade_gtk_assistant_parse_finished (GladeProject *project,
				    GObject *object)
{
	GtkAssistant *assistant = GTK_ASSISTANT (object);
	gint pages = gtk_assistant_get_n_pages (assistant);
	
	if (pages)
	{
		/* also sets pages "complete" and thus allows navigation under glade */
		glade_gtk_assistant_update_page_type (assistant);
		
		gtk_assistant_set_current_page (assistant, 0);
		glade_widget_property_set (glade_widget_get_from_gobject (object),
					   "n-pages", pages);
	}
}

void
glade_gtk_assistant_post_create (GladeWidgetAdaptor *adaptor,
				 GObject *object, 
				 GladeCreateReason reason)
{
	GladeWidget *parent = glade_widget_get_from_gobject (object);
	GladeProject *project = glade_widget_get_project (parent);
	
	if (reason == GLADE_CREATE_LOAD)
	{
		g_signal_connect (project, "parse-finished",
				  G_CALLBACK (glade_gtk_assistant_parse_finished),
				  object);
		return;
	}
	
	if (reason == GLADE_CREATE_USER)
	{
		glade_gtk_assistant_append_new_page (parent, project,
						     _("Introduction page"),
						     GTK_ASSISTANT_PAGE_INTRO);
	
		glade_gtk_assistant_append_new_page (parent, project,
						     _("Content page"),
						     GTK_ASSISTANT_PAGE_CONTENT);
	
		glade_gtk_assistant_append_new_page (parent, project,
						     _("Confirmation page"),
						     GTK_ASSISTANT_PAGE_CONFIRM);
		
		gtk_assistant_set_current_page (GTK_ASSISTANT (object), 0);
		
		glade_widget_property_set (parent, "n-pages", 3);
	}
}

void
glade_gtk_assistant_add_child (GladeWidgetAdaptor *adaptor,
			       GObject *container,
			       GObject *child)
{
	GtkAssistant *assistant = GTK_ASSISTANT (container);
	GtkWidget *widget = GTK_WIDGET (child);
	
	gtk_assistant_append_page (assistant, widget);
}

void
glade_gtk_assistant_remove_child (GladeWidgetAdaptor *adaptor,
				  GObject *container, 
				  GObject *child)
{
	GtkAssistant *assistant = GTK_ASSISTANT (container);
	GladeWidget *gassistant = glade_widget_get_from_gobject (container);
	
	gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET (child));
	glade_widget_property_set (gassistant, "n-pages",
			   	   gtk_assistant_get_n_pages (assistant));
}

void
glade_gtk_assistant_replace_child (GladeWidgetAdaptor *adaptor,
				   GObject *container,
				   GObject *current,
				   GObject *new_object)
{
	GtkAssistant *assistant = GTK_ASSISTANT (container);
	GtkWidget *page = GTK_WIDGET (new_object), *old_page = GTK_WIDGET (current);
	gint pos = glade_gtk_assistant_get_page (assistant, old_page);
	gboolean set_current = gtk_assistant_get_current_page (assistant) == pos;
	
	gtk_container_remove (GTK_CONTAINER (container), old_page);
		
	gtk_assistant_insert_page (assistant, page, pos);
	glade_gtk_assistant_update_page_type (assistant);
	
	if (set_current) gtk_assistant_set_current_page (assistant, pos);
}

gboolean
glade_gtk_assistant_verify_property (GladeWidgetAdaptor *adaptor,
				     GObject *object, 
				     const gchar *property_name,
				     const GValue *value)
{
	if (strcmp (property_name, "n-pages") == 0)
		return  g_value_get_int (value) >=
			gtk_assistant_get_n_pages (GTK_ASSISTANT (object));
	
	/* Chain Up */
	if (GWA_GET_CLASS (GTK_TYPE_WINDOW)->verify_property == NULL)
		return TRUE;
	return GWA_GET_CLASS (GTK_TYPE_WINDOW)->verify_property (adaptor,
								 object, 
								 property_name, 
								 value);
}

void
glade_gtk_assistant_set_property (GladeWidgetAdaptor *adaptor,
				  GObject *object,
				  const gchar *property_name,
				  const GValue *value)
{
	if (strcmp (property_name, "n-pages") == 0)
	{
		GtkAssistant *assistant = GTK_ASSISTANT (object);
		gint size, i;
		
		for (i = gtk_assistant_get_n_pages (GTK_ASSISTANT (object)),
		     size = g_value_get_int (value); i < size; i++)
			gtk_assistant_append_page (assistant, glade_placeholder_new ());

		glade_gtk_assistant_update_page_type (assistant);

		return;
	}
	
	/* Chain Up */
	GWA_GET_CLASS (GTK_TYPE_WINDOW)->set_property (adaptor,
						       object,
						       property_name, 
						       value);
}

void
glade_gtk_assistant_get_property (GladeWidgetAdaptor *adaptor,
				  GObject *object,
				  const gchar *property_name,
				  GValue *value)
{
	if (strcmp (property_name, "n-pages") == 0)
	{
		g_value_set_int (value, 
			gtk_assistant_get_n_pages (GTK_ASSISTANT (object)));
		return;
	}
	
	/* Chain Up */
	GWA_GET_CLASS (GTK_TYPE_WINDOW)->get_property (adaptor,
						       object,
						       property_name, 
						       value);
}

void
glade_gtk_assistant_set_child_property (GladeWidgetAdaptor *adaptor,
					GObject            *container,
					GObject            *child,
					const gchar        *property_name,
					const GValue       *value)
{	
	if (strcmp (property_name, "position") == 0)
	{
		GtkAssistant *assistant = GTK_ASSISTANT (container);
		GtkWidget *widget = GTK_WIDGET (child);
		gint pos, size;
		gboolean set_current;
		
		if ((pos = g_value_get_int (value)) < 0) return;
		if (pos == glade_gtk_assistant_get_page (assistant, widget))
			return;
		set_current = gtk_assistant_get_current_page (assistant) == 
			      glade_gtk_assistant_get_page (assistant, widget);
		
		size = gtk_assistant_get_n_pages (assistant);

		g_object_ref (child);
		gtk_container_remove (GTK_CONTAINER (container), widget);
		gtk_assistant_insert_page (assistant, widget, pos);
		g_object_unref (child);
		
		if (set_current) gtk_assistant_set_current_page (assistant, pos);
			
		glade_gtk_assistant_update_page_type (assistant);

		glade_gtk_assistant_update_position (assistant);
		
		return;
	}
	
	/* Chain Up */
	GWA_GET_CLASS (GTK_TYPE_WINDOW)->child_set_property (adaptor, 
							     container, 
							     child,
							     property_name, 
							     value);
}

void
glade_gtk_assistant_get_child_property (GladeWidgetAdaptor *adaptor,
					GObject            *container,
					GObject            *child,
					const gchar        *property_name,
					GValue       *value)
{
	if (strcmp (property_name, "position") == 0)
	{
		gint pos;
		pos = glade_gtk_assistant_get_page (GTK_ASSISTANT (container),
						    GTK_WIDGET (child));
		if (pos >= 0) g_value_set_int (value, pos);
		return;
	}
	
	/* Chain Up */
	GWA_GET_CLASS (GTK_TYPE_WINDOW)->child_get_property (adaptor, 
							     container, 
							     child,
							     property_name, 
							     value);
}

/*--------------------------- GtkRadioButton ---------------------------------*/
void
glade_gtk_radio_button_set_property (GladeWidgetAdaptor *adaptor,
				     GObject *object,
				     const gchar *property_name,
				     const GValue *value)
{
	if (strcmp (property_name, "group") == 0)
	{
		GtkRadioButton *radio = g_value_get_object (value);
		/* g_object_set () on this property produces a bogus warning,
		 * so we better use the API GtkRadioButton provides.
		 */
		gtk_radio_button_set_group (GTK_RADIO_BUTTON (object), 
					    radio ? gtk_radio_button_get_group (radio) : NULL);
		return;
	}
	
	/* Chain Up */
	GWA_GET_CLASS (GTK_TYPE_CHECK_BUTTON)->set_property (adaptor,
							     object,
							     property_name, 
							     value);
}

/*--------------------------- GtkSizeGroup ---------------------------------*/

#define GLADE_TAG_SIZEGROUP_WIDGETS "widgets"
#define GLADE_TAG_SIZEGROUP_WIDGET  "widget"

static void
glade_gtk_size_group_read_widgets (GladeWidget  *widget,
				   GladeXmlNode *node)
{
	GladeXmlNode  *widgets_node;
	GladeProperty *property;
	gchar         *string = NULL;

	if ((widgets_node = 
	     glade_xml_search_child (node, GLADE_TAG_SIZEGROUP_WIDGETS)) != NULL)
	{
		GladeXmlNode  *node;

		for (node = glade_xml_node_get_children (widgets_node); 
		     node; node = glade_xml_node_next (node))
		{
			gchar *widget_name, *tmp;
			
			if (!glade_xml_node_verify (node, GLADE_TAG_SIZEGROUP_WIDGET))
				continue;

			widget_name = glade_xml_get_property_string_required
				(node, GLADE_TAG_NAME, NULL);
				
			if (string == NULL)
				string = widget_name;
			else if (widget_name != NULL)
			{
				tmp = g_strdup_printf ("%s%s%s", string, GPC_OBJECT_DELIMITER, widget_name);
				string = (g_free (string), tmp);
				g_free (widget_name);
			}
		}
	}


	if (string)
	{
		property = glade_widget_get_property (widget, "widgets");
		g_assert (property);

		/* we must synchronize this directly after loading this project
		 * (i.e. lookup the actual objects after they've been parsed and
		 * are present).
		 */
		g_object_set_data_full (G_OBJECT (property), 
					"glade-loaded-object", 
					string, g_free);
	}
}

void
glade_gtk_size_group_read_widget (GladeWidgetAdaptor *adaptor,
				  GladeWidget        *widget,
				  GladeXmlNode       *node)
{
	if (!glade_xml_node_verify 
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and read in all the normal properties.. */
        GWA_GET_CLASS (G_TYPE_OBJECT)->read_widget (adaptor, widget, node);

	glade_gtk_size_group_read_widgets (widget, node);
}


static void
glade_gtk_size_group_write_widgets (GladeWidget        *widget,
				    GladeXmlContext    *context,
				    GladeXmlNode       *node)
{
	GladeXmlNode  *widgets_node, *widget_node;
	GList         *widgets = NULL, *list;
	GladeWidget   *awidget;

	widgets_node = glade_xml_node_new (context, GLADE_TAG_SIZEGROUP_WIDGETS);

	if (glade_widget_property_get (widget, "widgets", &widgets))
	{
		for (list = widgets; list; list = list->next)
		{
			awidget     = glade_widget_get_from_gobject (list->data);
			widget_node = glade_xml_node_new (context, GLADE_TAG_SIZEGROUP_WIDGET);
			glade_xml_node_append_child (widgets_node, widget_node);
			glade_xml_node_set_property_string (widget_node, GLADE_TAG_NAME, awidget->name);
		}
	}

	if (!glade_xml_node_get_children (widgets_node))
		glade_xml_node_delete (widgets_node);
	else
		glade_xml_node_append_child (node, widgets_node);
	
}


void
glade_gtk_size_group_write_widget (GladeWidgetAdaptor *adaptor,
				   GladeWidget        *widget,
				   GladeXmlContext    *context,
				   GladeXmlNode       *node)
{
	if (!glade_xml_node_verify
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and read in all the normal properties.. */
        GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);

	glade_gtk_size_group_write_widgets (widget, context, node);
}


void
glade_gtk_size_group_set_property (GladeWidgetAdaptor *adaptor,
				   GObject *object,
				   const gchar *property_name,
				   const GValue *value)
{
	if (!strcmp (property_name, "widgets"))
	{
		GSList *sg_widgets, *slist;
		GList *widgets, *list;

		/* remove old widgets */
		if ((sg_widgets = gtk_size_group_get_widgets (GTK_SIZE_GROUP (object))) != NULL)
		{
			/* copy since we are modifying an internal list */
			sg_widgets = g_slist_copy (sg_widgets);
			for (slist = sg_widgets; slist; slist = slist->next)
				gtk_size_group_remove_widget (GTK_SIZE_GROUP (object), GTK_WIDGET (slist->data));
			g_slist_free (sg_widgets);
		}

		/* add new widgets */
		if ((widgets = g_value_get_boxed (value)) != NULL)
		{
			for (list = widgets; list; list = list->next)
				gtk_size_group_add_widget (GTK_SIZE_GROUP (object), GTK_WIDGET (list->data));
		}
	}
	else
		GWA_GET_CLASS (G_TYPE_OBJECT)->set_property (adaptor, object,
							     property_name, value);
}

/*--------------------------- GtkIconFactory ---------------------------------*/
#define GLADE_TAG_SOURCES   "sources"
#define GLADE_TAG_SOURCE    "source"

#define GLADE_TAG_STOCK_ID  "stock-id"
#define GLADE_TAG_FILENAME  "filename"
#define GLADE_TAG_DIRECTION "direction"
#define GLADE_TAG_STATE     "state"
#define GLADE_TAG_SIZE      "size"

void
glade_gtk_icon_factory_post_create (GladeWidgetAdaptor *adaptor,
				    GObject *object, 
				    GladeCreateReason reason)
{
	gtk_icon_factory_add_default (GTK_ICON_FACTORY (object));
}

static void
glade_gtk_icon_factory_read_sources (GladeWidget  *widget, 
				     GladeXmlNode *node)
{
	GladeIconSources *sources;
	GtkIconSource    *source;
	GladeXmlNode     *sources_node, *source_node;
	GValue           *value;
	GList            *list;
	gchar            *current_icon_name = NULL;
	GdkPixbuf        *pixbuf;

	if ((sources_node = glade_xml_search_child (node, GLADE_TAG_SOURCES)) == NULL)
		return;

	sources = glade_icon_sources_new ();

	/* Here we expect all icon sets to remain together in the list. */
	for (source_node = glade_xml_node_get_children (sources_node); source_node;
	     source_node = glade_xml_node_next (source_node))
	{
		gchar *icon_name;
		gchar *str;

		if (!glade_xml_node_verify (source_node, GLADE_TAG_SOURCE)) 
			continue;

		if (!(icon_name = 
		      glade_xml_get_property_string_required (source_node, GLADE_TAG_STOCK_ID, NULL)))
			continue;

		if (!(str = glade_xml_get_property_string_required (source_node, GLADE_TAG_FILENAME, NULL)))
		{
			g_free (icon_name);
			continue;
		}

		if (!current_icon_name || strcmp (current_icon_name, icon_name) != 0)
			current_icon_name = (g_free (current_icon_name), g_strdup (icon_name));

		source = gtk_icon_source_new ();
		
		/* Deal with the filename... */
		value = glade_utils_value_from_string (GDK_TYPE_PIXBUF, str,
						       widget->project, widget);
		pixbuf = g_value_dup_object (value);
		g_value_unset (value);
		g_free (value);

		gtk_icon_source_set_pixbuf (source, pixbuf);
		g_object_unref (G_OBJECT (pixbuf));
		g_free (str);

		/* Now the attributes... */
		if ((str = glade_xml_get_property_string (source_node, GLADE_TAG_DIRECTION)) != NULL)
		{
			GtkTextDirection direction =
				glade_utils_enum_value_from_string (GTK_TYPE_TEXT_DIRECTION, str);
			gtk_icon_source_set_direction_wildcarded (source, FALSE);
			gtk_icon_source_set_direction (source, direction);
			g_free (str);
		}

		if ((str = glade_xml_get_property_string (source_node, GLADE_TAG_SIZE)) != NULL)
		{
			GtkIconSize size = 
				glade_utils_enum_value_from_string (GTK_TYPE_ICON_SIZE, str);
			gtk_icon_source_set_size_wildcarded (source, FALSE);
			gtk_icon_source_set_size (source, size);
			g_free (str);
		}

		if ((str = glade_xml_get_property_string (source_node, GLADE_TAG_STATE)) != NULL)
		{
			GtkStateType state =
				glade_utils_enum_value_from_string (GTK_TYPE_STATE_TYPE, str);
			gtk_icon_source_set_state_wildcarded (source, FALSE);
			gtk_icon_source_set_state (source, state);
			g_free (str);
		}

		if ((list = g_hash_table_lookup (sources->sources, g_strdup (current_icon_name))) != NULL)
		{
			GList *new_list = g_list_append (list, source);
			
			/* Warning: if we use g_list_prepend() the returned pointer will be different
			 * so we would have to replace the list pointer in the hash table.
			 * But before doing that we have to steal the old list pointer otherwise
			 * we would have to make a copy then add the new icon to finally replace the hash table
			 * value.
			 * Anyways if we choose to prepend we would have to reverse the list outside this loop
			 * so its better to append.
			 */
			if (new_list != list)
			{
				/* current g_list_append() returns the same pointer so this is not needed */
				g_hash_table_steal (sources->sources, current_icon_name);
				g_hash_table_insert (sources->sources, g_strdup (current_icon_name), new_list);
			}
		}
		else
		{
			list = g_list_append (NULL, source);
			g_hash_table_insert (sources->sources, g_strdup (current_icon_name), list);
		}
	}

	if (g_hash_table_size (sources->sources) > 0)
		glade_widget_property_set (widget, "sources", sources);

	glade_icon_sources_free (sources);
}

void
glade_gtk_icon_factory_read_widget (GladeWidgetAdaptor *adaptor,
				    GladeWidget        *widget,
				    GladeXmlNode       *node)
{
	if (!glade_xml_node_verify 
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and read in any normal properties.. */
        GWA_GET_CLASS (G_TYPE_OBJECT)->read_widget (adaptor, widget, node);

	glade_gtk_icon_factory_read_sources (widget, node);
}

typedef struct {
	GladeXmlContext *context;
	GladeXmlNode    *node;
} SourceWriteTab;

static void
write_icon_sources (gchar          *icon_name,
		    GList          *sources,
		    SourceWriteTab *tab)
{
	GladeXmlNode  *source_node;
	GtkIconSource *source;
	GList         *l;
	gchar         *string;

	GdkPixbuf     *pixbuf;

	for (l = sources; l; l = l->next)
	{
		source = l->data;

		source_node = glade_xml_node_new (tab->context, GLADE_TAG_SOURCE);
		glade_xml_node_append_child (tab->node, source_node);

		glade_xml_node_set_property_string (source_node, GLADE_TAG_STOCK_ID, icon_name);

		if (!gtk_icon_source_get_direction_wildcarded (source))
		{
			GtkTextDirection direction = gtk_icon_source_get_direction (source);
			string = glade_utils_enum_string_from_value (GTK_TYPE_TEXT_DIRECTION, direction);
			glade_xml_node_set_property_string (source_node, GLADE_TAG_DIRECTION, string);
			g_free (string);
		}

		if (!gtk_icon_source_get_size_wildcarded (source))
		{
			GtkIconSize size = gtk_icon_source_get_size (source);
			string = glade_utils_enum_string_from_value (GTK_TYPE_ICON_SIZE, size);
			glade_xml_node_set_property_string (source_node, GLADE_TAG_SIZE, string);
			g_free (string);
		}

		if (!gtk_icon_source_get_state_wildcarded (source))
		{
			GtkStateType state = gtk_icon_source_get_state (source);
			string = glade_utils_enum_string_from_value (GTK_TYPE_STATE_TYPE, state);
			glade_xml_node_set_property_string (source_node, GLADE_TAG_STATE, string);
			g_free (string);
		}

		pixbuf = gtk_icon_source_get_pixbuf (source);
		string = g_object_get_data (G_OBJECT (pixbuf), "GladeFileName");

		glade_xml_node_set_property_string (source_node, 
						    GLADE_TAG_FILENAME, 
						    string);
	}
}


static void
glade_gtk_icon_factory_write_sources (GladeWidget      *widget, 
				      GladeXmlContext  *context,
				      GladeXmlNode     *node)
{
	GladeXmlNode     *sources_node;
	GladeIconSources *sources = NULL;
	SourceWriteTab    tab;

	glade_widget_property_get (widget, "sources", &sources);
	if (!sources)
		return;

	sources_node = glade_xml_node_new (context, GLADE_TAG_SOURCES);

	tab.context = context;
	tab.node    = sources_node;
	g_hash_table_foreach (sources->sources, (GHFunc)write_icon_sources, &tab);

	if (!glade_xml_node_get_children (sources_node))
		glade_xml_node_delete (sources_node);
	else
		glade_xml_node_append_child (node, sources_node);

}


void
glade_gtk_icon_factory_write_widget (GladeWidgetAdaptor *adaptor,
				     GladeWidget        *widget,
				     GladeXmlContext    *context,
				     GladeXmlNode       *node)
{
	if (!glade_xml_node_verify
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and write all the normal properties.. */
        GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);

	glade_gtk_icon_factory_write_sources (widget, context, node);
}

static void
apply_icon_sources (gchar          *icon_name,
		    GList          *sources,
		    GtkIconFactory *factory)
{
	GtkIconSource *source;
	GtkIconSet    *set;
	GList         *l;

	set = gtk_icon_set_new ();

	for (l = sources; l; l = l->next)
	{
		source = gtk_icon_source_copy ((GtkIconSource *)l->data);
		gtk_icon_set_add_source (set, source);
	}

	gtk_icon_factory_add (factory, icon_name, set);
}

static void
glade_gtk_icon_factory_set_sources (GObject *object, const GValue *value)
{
	GladeIconSources *sources = g_value_get_boxed (value);
	if (sources)
		g_hash_table_foreach (sources->sources, (GHFunc)apply_icon_sources, object);
}


void
glade_gtk_icon_factory_set_property (GladeWidgetAdaptor *adaptor,
				     GObject *object,
				     const gchar *property_name,
				     const GValue *value)
{
	if (strcmp (property_name, "sources") == 0)
	{
		glade_gtk_icon_factory_set_sources (object, value);
	} 
	else
		/* Chain Up */
		GWA_GET_CLASS (G_TYPE_OBJECT)->set_property (adaptor,
							     object,
							     property_name, 
							     value);
}

static void
serialize_icon_sources (gchar          *icon_name,
			GList          *sources,
			GString        *string)
{	
	GList *l;
	
	for (l = sources; l; l = g_list_next (l))
	{
		GtkIconSource *source = l->data;
		GdkPixbuf     *pixbuf;
		gchar         *str;

		pixbuf = gtk_icon_source_get_pixbuf (source);
		str    = g_object_get_data (G_OBJECT (pixbuf), "GladeFileName");

		g_string_append_printf (string, "%s[%s] ", icon_name, str);

		if (!gtk_icon_source_get_direction_wildcarded (source))
		{
			GtkTextDirection direction = gtk_icon_source_get_direction (source);
			str = glade_utils_enum_string_from_value (GTK_TYPE_TEXT_DIRECTION, direction);
			g_string_append_printf (string, "dir-%s ", str);
			g_free (str);
		}

		if (!gtk_icon_source_get_size_wildcarded (source))
		{
			GtkIconSize size = gtk_icon_source_get_size (source);
			str = glade_utils_enum_string_from_value (GTK_TYPE_ICON_SIZE, size);
			g_string_append_printf (string, "size-%s ", str);
			g_free (str);
		}

		if (!gtk_icon_source_get_state_wildcarded (source))
		{
			GtkStateType state = gtk_icon_source_get_state (source);
			str = glade_utils_enum_string_from_value (GTK_TYPE_STATE_TYPE, state);
			g_string_append_printf (string, "state-%s ", str);
			g_free (str);
		}
		
		g_string_append_printf (string, "| ");
	}
}

gchar *
glade_gtk_icon_factory_string_from_value (GladeWidgetAdaptor *adaptor,
					  GladePropertyClass *klass,
					  const GValue       *value,
					  GladeProjectFormat  fmt)
{
	GString *string;

	if (klass->pspec->value_type == GLADE_TYPE_ICON_SOURCES)
	{
		GladeIconSources *sources = g_value_get_boxed (value);
		if (!sources)
			return g_strdup ("");

		string = g_string_new ("");		
		g_hash_table_foreach (sources->sources, (GHFunc)serialize_icon_sources, string);

		return g_string_free (string, FALSE);
	}
	else
		return GWA_GET_CLASS 
			(G_TYPE_OBJECT)->string_from_value (adaptor, 
							    klass, 
							    value,
							    fmt);
}


GladeEditorProperty *
glade_gtk_icon_factory_create_eprop (GladeWidgetAdaptor *adaptor,
				     GladePropertyClass *klass,
				     gboolean            use_command)
{
	GladeEditorProperty *eprop;

	if (klass->pspec->value_type == GLADE_TYPE_ICON_SOURCES)
		eprop = g_object_new (GLADE_TYPE_EPROP_ICON_SOURCES,
				      "property-class", klass, 
				      "use-command", use_command,
				      NULL);				
	else
		eprop = GWA_GET_CLASS 
			(G_TYPE_OBJECT)->create_eprop (adaptor, 
						       klass, 
						       use_command);
	return eprop;
}

GladeEditable *
glade_gtk_icon_factory_create_editable (GladeWidgetAdaptor  *adaptor,
					GladeEditorPageType  type)
{
	GladeEditable *editable;

	/* Get base editable */
	editable = GWA_GET_CLASS (G_TYPE_OBJECT)->create_editable (adaptor, type);

	if (type == GLADE_PAGE_GENERAL)
		return (GladeEditable *)glade_icon_factory_editor_new (adaptor, editable);

	return editable;
}


/*--------------------------- GtkListStore/GtkTreeStore ---------------------------------*/

#define GLADE_TAG_COLUMNS	"columns"
#define GLADE_TAG_COLUMN	"column"
#define GLADE_TAG_TYPE		"type"

#define GLADE_TAG_ROW           "row"
#define GLADE_TAG_DATA          "data"
#define GLADE_TAG_COL           "col"


static gboolean 
glade_gtk_cell_layout_has_renderer (GtkCellLayout *layout,
				    GtkCellRenderer *renderer)
{
	GList *cells = gtk_cell_layout_get_cells (layout);
	gboolean has_renderer;

	has_renderer = (g_list_find (cells, renderer) != NULL);

	g_list_free (cells);

	return has_renderer;
}

static gboolean
glade_gtk_cell_renderer_sync_attributes (GObject *object)
{

	GtkCellLayout *layout;
	GtkCellRenderer *cell;
	GladeWidget *widget = glade_widget_get_from_gobject (object);
        GladeWidget *parent;
	GladeWidget *gmodel;
	GladeProperty *property;
	gchar *attr_prop_name;
	GList *l, *column_list = NULL;
	gint columns = 0;
	static gint attr_len = 0;

	if (!attr_len)
		attr_len = strlen ("attr-");

	/* Apply attributes to renderer when bound to a model in runtime */
	widget = glade_widget_get_from_gobject (object);

	parent = glade_widget_get_parent (widget);
	if (parent == NULL)
		return FALSE;

	/* When creating widgets, sometimes the parent is set before parenting happens,
	 * here we have to be careful for that..
	 */
	layout = GTK_CELL_LAYOUT (parent->object);
	cell = GTK_CELL_RENDERER (object);

	if (!glade_gtk_cell_layout_has_renderer (layout, cell))
		return FALSE;

	if ((gmodel = glade_cell_renderer_get_model (widget)) == NULL)
		return FALSE;

	glade_widget_property_get (gmodel, "columns", &column_list);
	columns = g_list_length (column_list);

	gtk_cell_layout_clear_attributes (layout, cell);

	for (l = widget->properties; l; l = l->next)
	{
		property = l->data;

		if (strncmp (property->klass->id, "attr-", attr_len) == 0)
		{
			GladeProperty *attr_prop;
			gint           column = g_value_get_int (property->value);

			attr_prop_name = &property->klass->id[attr_len];
			attr_prop      = glade_widget_get_property (widget, attr_prop_name);

			if (column >= 0 && column < columns)
			{
				GladeColumnType *column_type = (GladeColumnType *)g_list_nth_data (column_list, column);
				GType            column_gtype = g_type_from_name (column_type->type_name);

				if (column_gtype && 
				    g_value_type_transformable (column_gtype, attr_prop->klass->pspec->value_type))
					gtk_cell_layout_add_attribute (layout, cell, attr_prop_name, column);
			}
		}
	}

	return FALSE;
}


static gboolean
glade_gtk_cell_layout_sync_attributes (GObject *layout)
{
	GladeWidget *gwidget = glade_widget_get_from_gobject (layout);
	GObject     *cell;
	GList       *children, *l;

	children = glade_widget_adaptor_get_children (gwidget->adaptor, layout);
	for (l = children; l; l = l->next)
	{
		cell = l->data;
		if (!GTK_IS_CELL_RENDERER (cell))
			continue;

		glade_gtk_cell_renderer_sync_attributes (cell);
	}
	g_list_free (children);

	return FALSE;
}

static void
glade_gtk_store_set_columns (GObject *object,
			     const GValue *value)
{
	GList       *l;
	gint         i, n;
	GType       *types;
	
	for (i = 0, l = g_value_get_boxed (value), n = g_list_length (l), types = g_new (GType, n); 
	     l; l = g_list_next (l), i++)
	{
		GladeColumnType *data = l->data;

		if (g_type_from_name (data->type_name) != G_TYPE_INVALID)
			types[i] = g_type_from_name (data->type_name);
		else
			types[i] = G_TYPE_POINTER;
	}

	if (GTK_IS_LIST_STORE (object))
		gtk_list_store_set_column_types (GTK_LIST_STORE (object), n, types);
	else
		gtk_tree_store_set_column_types (GTK_TREE_STORE (object), n, types);

	g_free (types);
}

static void
glade_gtk_store_set_data (GObject *object,
			  const GValue *value)
{
	GladeWidget     *gwidget = glade_widget_get_from_gobject (object);
	GList           *columns = NULL;
	GNode           *data_tree, *row, *iter;
	gint             colnum;
	GtkTreeIter      row_iter;
	GladeModelData  *data;
	GType            column_type;
	
	if (GTK_IS_LIST_STORE (object))
		gtk_list_store_clear (GTK_LIST_STORE (object));
	else
		gtk_tree_store_clear (GTK_TREE_STORE (object));
	
	glade_widget_property_get (gwidget, "columns", &columns);
	data_tree = g_value_get_boxed (value);
	
	/* Nothing to enter without columns defined */
	if (!data_tree || !columns)
		return;

	for (row = data_tree->children; row; row = row->next)
	{
		if (GTK_IS_LIST_STORE (object))
			gtk_list_store_append (GTK_LIST_STORE (object), &row_iter);
		else
			/* (for now no child data... ) */
			gtk_tree_store_append (GTK_TREE_STORE (object), &row_iter, NULL);
		
		for (colnum = 0, iter = row->children; iter; 
		     colnum++, iter = iter->next)
		{
			data = iter->data;

			if (!g_list_nth (columns, colnum))
				break;

			/* Abort if theres a type mismatch, the widget's being rebuilt
			 * and a sync will come soon with the right values
			 */
			column_type = gtk_tree_model_get_column_type (GTK_TREE_MODEL (object), colnum);
			if (G_VALUE_TYPE (&data->value) != column_type)
				continue;

			if (GTK_IS_LIST_STORE (object))
				gtk_list_store_set_value (GTK_LIST_STORE (object), 
							  &row_iter,
							  colnum, &data->value);
			else
				gtk_tree_store_set_value (GTK_TREE_STORE (object), 
							  &row_iter, 
							  colnum, &data->value);
		}
	}
}

void
glade_gtk_store_set_property (GladeWidgetAdaptor *adaptor,
			      GObject *object,
			      const gchar *property_name,
			      const GValue *value)
{
	if (strcmp (property_name, "columns") == 0)
	{
		glade_gtk_store_set_columns (object, value);
	} 
	else if (strcmp (property_name, "data") == 0)
	{
		glade_gtk_store_set_data (object, value);
	}
	else
		/* Chain Up */
		GWA_GET_CLASS (G_TYPE_OBJECT)->set_property (adaptor,
							     object,
							     property_name, 
							     value);
}

GladeEditorProperty *
glade_gtk_store_create_eprop (GladeWidgetAdaptor *adaptor,
			      GladePropertyClass *klass,
			      gboolean            use_command)
{
	GladeEditorProperty *eprop;

	/* chain up.. */
	if (klass->pspec->value_type == GLADE_TYPE_COLUMN_TYPE_LIST)
		eprop = g_object_new (GLADE_TYPE_EPROP_COLUMN_TYPES,
				      "property-class", klass, 
				      "use-command", use_command,
				      NULL);
	else if (klass->pspec->value_type == GLADE_TYPE_MODEL_DATA_TREE)
		eprop = g_object_new (GLADE_TYPE_EPROP_MODEL_DATA,
				      "property-class", klass, 
				      "use-command", use_command,
				      NULL);		
	else
		eprop = GWA_GET_CLASS 
			(G_TYPE_OBJECT)->create_eprop (adaptor, 
						       klass, 
						       use_command);
	return eprop;
}


static void 
glade_gtk_store_columns_changed (GladeProperty *property,
				 GValue        *old_value,
				 GValue        *new_value,
				 GladeWidget   *store)
{
	GList *l, *list, *children;

	/* Reset the attributes for all cell renderers referring to this store */
	for (l = store->prop_refs; l; l = l->next)
	{
		GladeWidget *referring_widget = GLADE_PROPERTY (l->data)->widget;

		if (GTK_IS_CELL_LAYOUT (referring_widget->object))
			glade_gtk_cell_layout_sync_attributes (referring_widget->object);
		else if (GTK_IS_TREE_VIEW (referring_widget->object))
		{
			children = glade_widget_adaptor_get_children (referring_widget->adaptor, 
								      referring_widget->object);

			for (list = children; list; list = list->next)
			{
				/* Clear the GtkTreeViewColumns... */
				if (GTK_IS_CELL_LAYOUT (list->data))
					glade_gtk_cell_layout_sync_attributes (G_OBJECT (list->data));
			}

			g_list_free (children);
		}
	}
}

void
glade_gtk_store_post_create (GladeWidgetAdaptor *adaptor,
			     GObject *object, 
			     GladeCreateReason reason)
{
	GladeWidget   *gwidget;
	GladeProperty *property;

	if (reason == GLADE_CREATE_REBUILD)
		return;

	gwidget  = glade_widget_get_from_gobject (object);
	property = glade_widget_get_property (gwidget, "columns");

	/* Here we watch the value-changed signal on the "columns" property, we need
	 * to reset all the Cell Renderer attributes when the underlying "columns" change,
	 * the reason we do it from "value-changed" is because GladeWidget prop references
	 * are unavailable while rebuilding an object, and the liststore needs to be rebuilt
	 * in order to set the columns.
	 *
	 * This signal will be envoked after applying the new column types to the store
	 * and before the views get any signal to update themselves from the changed model,
	 * perfect time to reset the attributes.
	 */
	g_signal_connect (G_OBJECT (property), "value-changed",
			  G_CALLBACK (glade_gtk_store_columns_changed), gwidget);
}

GladeEditable *
glade_gtk_store_create_editable (GladeWidgetAdaptor  *adaptor,
				 GladeEditorPageType  type)
{
	GladeEditable *editable;

	/* Get base editable */
	editable = GWA_GET_CLASS (G_TYPE_OBJECT)->create_editable (adaptor, type);

	if (type == GLADE_PAGE_GENERAL)
		return (GladeEditable *)glade_store_editor_new (adaptor, editable);

	return editable;
}

gchar *
glade_gtk_store_string_from_value (GladeWidgetAdaptor *adaptor,
				   GladePropertyClass *klass,
				   const GValue       *value,
				   GladeProjectFormat  fmt)
{
	GString *string;

	if (klass->pspec->value_type == GLADE_TYPE_COLUMN_TYPE_LIST)
	{
		GList *l;
		string = g_string_new ("");		
		for (l = g_value_get_boxed (value); l; l = g_list_next (l))
		{
			GladeColumnType *data = l->data;
			g_string_append_printf (string, (g_list_next (l)) ? "%s:%s|" : "%s:%s",
						data->type_name, data->column_name);
		}
		return g_string_free (string, FALSE);
	}
	else if (klass->pspec->value_type == GLADE_TYPE_MODEL_DATA_TREE)
	{
		GladeModelData *data;
		GNode *data_tree, *row, *iter;
		gint rownum;
		gchar *str;
		gboolean is_last;

		/* Return a unique string for the backend to compare */
		data_tree = g_value_get_boxed (value);

		if (!data_tree || !data_tree->children)
			return g_strdup ("");

		string = g_string_new ("");
		for (rownum = 0, row = data_tree->children; row; 
		     rownum++, row = row->next)
		{
			for (iter = row->children; iter; iter = iter->next)
			{
				data = iter->data;

				if (!G_VALUE_TYPE (&data->value) || 
				    G_VALUE_TYPE (&data->value) == G_TYPE_INVALID)
					str = g_strdup ("(virtual)");
				else if (G_VALUE_TYPE (&data->value) != G_TYPE_POINTER)
					str = glade_utils_string_from_value (&data->value, fmt);
				else
					str = g_strdup ("(null)");

				is_last = !row->next && !iter->next;
				g_string_append_printf (string, "%s[%d]:%s",
							data->name, rownum, str);

				if (data->i18n_translatable)
					g_string_append_printf (string, " translatable");
				if (data->i18n_context)
					g_string_append_printf (string, " i18n-context:%s", data->i18n_context);
				if (data->i18n_comment)
					g_string_append_printf (string, " i18n-comment:%s", data->i18n_comment);

				if (!is_last)
					g_string_append_printf (string, "|");

				g_free (str);
			}
		}
		return g_string_free (string, FALSE);
	}
	else
		return GWA_GET_CLASS 
			(G_TYPE_OBJECT)->string_from_value (adaptor, 
							    klass, 
							    value,
							    fmt);
}

static void
glade_gtk_store_write_columns (GladeWidget        *widget,
			       GladeXmlContext    *context,
			       GladeXmlNode       *node)
{
	GladeXmlNode  *columns_node;
	GladeProperty *prop;
	GList *l;

	prop = glade_widget_get_property (widget, "columns");

	columns_node = glade_xml_node_new (context, GLADE_TAG_COLUMNS);
	
	for (l = g_value_get_boxed (prop->value); l; l = g_list_next (l))
	{
		GladeColumnType *data = l->data;
		GladeXmlNode  *column_node, *comment_node;
		
		/* Write column names in comments... */
		gchar *comment = g_strdup_printf (" column-name %s ", data->column_name);
		comment_node = glade_xml_node_new_comment (context, comment);
		glade_xml_node_append_child (columns_node, comment_node);
		g_free (comment);
		
		column_node = glade_xml_node_new (context, GLADE_TAG_COLUMN);
		glade_xml_node_append_child (columns_node, column_node);
		glade_xml_node_set_property_string (column_node, GLADE_TAG_TYPE,
						    data->type_name);
	}

	if (!glade_xml_node_get_children (columns_node))
		glade_xml_node_delete (columns_node);
	else
		glade_xml_node_append_child (node, columns_node);

}

static void
glade_gtk_store_write_data (GladeWidget        *widget,
			    GladeXmlContext    *context,
			    GladeXmlNode       *node)
{
	GladeXmlNode   *data_node, *col_node, *row_node;
	GList          *columns = NULL;
	GladeModelData *data;
	GNode          *data_tree = NULL, *row, *iter;
	gint            colnum;

	glade_widget_property_get (widget, "data", &data_tree);
	glade_widget_property_get (widget, "columns", &columns);

	/* XXX log errors about data not fitting columns here when
	 * loggin is available
	 */
	if (!data_tree || !columns)
		return;

	data_node = glade_xml_node_new (context, GLADE_TAG_DATA);

	for (row = data_tree->children; row; row = row->next)
	{
		row_node = glade_xml_node_new (context, GLADE_TAG_ROW);
		glade_xml_node_append_child (data_node, row_node);

		for (colnum = 0, iter = row->children; iter; 
		     colnum++, iter = iter->next)
		{
			gchar   *string, *column_number;

			data = iter->data;

			/* Skip non-serializable data */
			if (G_VALUE_TYPE (&data->value) == 0 ||
			    G_VALUE_TYPE (&data->value) == G_TYPE_POINTER)
				continue;

			string = glade_utils_string_from_value (&data->value, 
								glade_project_get_format (widget->project));
			
			/* XXX Log error: data col j exceeds columns on row i */
			if (!g_list_nth (columns, colnum))
				break;
			
			column_number = g_strdup_printf ("%d", colnum);
			
			col_node = glade_xml_node_new (context, GLADE_TAG_COL);
			glade_xml_node_append_child (row_node, col_node);
			glade_xml_node_set_property_string (col_node, GLADE_TAG_ID,
							    column_number);
			glade_xml_set_content (col_node, string);

			if (data->i18n_translatable)
				glade_xml_node_set_property_string (col_node, 
								    GLADE_TAG_TRANSLATABLE, 
								    GLADE_XML_TAG_I18N_TRUE);
			if (data->i18n_context)
				glade_xml_node_set_property_string (col_node, 
								    GLADE_TAG_CONTEXT, 
								    data->i18n_context);
			if (data->i18n_comment)
				glade_xml_node_set_property_string (col_node, 
								    GLADE_TAG_COMMENT, 
								    data->i18n_comment);

			
			g_free (column_number);
			g_free (string);
		}
	}

	if (!glade_xml_node_get_children (data_node))
		glade_xml_node_delete (data_node);
	else
		glade_xml_node_append_child (node, data_node);
}


void
glade_gtk_store_write_widget (GladeWidgetAdaptor *adaptor,
			      GladeWidget        *widget,
			      GladeXmlContext    *context,
			      GladeXmlNode       *node)
{
	if (!glade_xml_node_verify
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and write all the normal properties.. */
        GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);

	glade_gtk_store_write_columns (widget, context, node);
	glade_gtk_store_write_data (widget, context, node);
}

static void
glade_gtk_store_read_columns (GladeWidget *widget, GladeXmlNode *node)
{
	GladeNameContext *context;
	GladeXmlNode *columns_node;
	GladeProperty *property;
	GladeXmlNode *prop;
	GList *types = NULL;
	GValue value = {0,};
	gchar column_name[256];

	column_name[0] = '\0';
	column_name[255] = '\0';

	if ((columns_node = glade_xml_search_child (node, GLADE_TAG_COLUMNS)) == NULL)
		return;

	context = glade_name_context_new ();

	for (prop = glade_xml_node_get_children_with_comments (columns_node); prop;
	     prop = glade_xml_node_next_with_comments (prop))
	{
		GladeColumnType *data;
		gchar *type, *comment_str, buffer[256];

		if (!glade_xml_node_verify_silent (prop, GLADE_TAG_COLUMN) &&
		    !glade_xml_node_is_comment (prop)) continue;

		if (glade_xml_node_is_comment (prop))
		{
			comment_str = glade_xml_get_content (prop);
			if (sscanf (comment_str, " column-name %s", buffer) == 1)
				strncpy (column_name, buffer, 255);

			g_free (comment_str);
			continue;
		}

		type = glade_xml_get_property_string_required (prop, GLADE_TAG_TYPE, NULL);

		data              = glade_column_type_new (type, NULL);
		data->type_name   = g_strdup (type);
		data->column_name = column_name[0] ? g_strdup (column_name) : g_ascii_strdown (type, -1);

		if (glade_name_context_has_name (context, data->column_name))
		{
			gchar *name = glade_name_context_new_name (context, data->column_name);
			g_free (data->column_name);
			data->column_name = name;
		}
		glade_name_context_add_name (context, data->column_name);
		
		types = g_list_prepend (types, data);
		g_free (type);

		column_name[0] = '\0';
	}
	
	property = glade_widget_get_property (widget, "columns");
	g_value_init (&value, GLADE_TYPE_COLUMN_TYPE_LIST);
	g_value_take_boxed (&value, g_list_reverse (types));
	glade_property_set_value (property, &value);
	g_value_unset (&value);
}

static void
glade_gtk_store_read_data (GladeWidget *widget, GladeXmlNode *node)
{
	GladeXmlNode *data_node, *row_node, *col_node;
	GNode *data_tree, *row, *item;
	GladeModelData *data;
	GValue *value;
	GList *column_types = NULL;
	GladeColumnType *column_type;
	gint colnum;

	if ((data_node = glade_xml_search_child (node, GLADE_TAG_DATA)) == NULL)
		return;

	/* XXX FIXME: Warn that columns werent there when parsing */
	if (!glade_widget_property_get (widget, "columns", &column_types) || !column_types)
		return;

	/* Create root... */
	data_tree = g_node_new (NULL);
	
	for (row_node = glade_xml_node_get_children (data_node); row_node;
	     row_node = glade_xml_node_next (row_node))
	{
		gchar *value_str;

		if (!glade_xml_node_verify (row_node, GLADE_TAG_ROW)) 
			continue;

		row = g_node_new (NULL);
		g_node_append (data_tree, row);

		/* XXX FIXME: we are assuming that the columns are listed in order */
		for (colnum = 0, col_node = glade_xml_node_get_children (row_node); col_node;
		     col_node = glade_xml_node_next (col_node))
		{
			gint read_column;

			if (!glade_xml_node_verify (col_node, GLADE_TAG_COL)) 
				continue;

			read_column = glade_xml_get_property_int (col_node, GLADE_TAG_ID, -1);
			if (read_column < 0)
			{
				g_critical ("Parsed negative column id");
				continue;
			}

			/* Catch up for gaps in the list where unserializable types are involved */
			while (colnum < read_column)
			{
				column_type = g_list_nth_data (column_types, colnum);

				data = glade_model_data_new (G_TYPE_INVALID, column_type->column_name);

				item = g_node_new (data);
				g_node_append (row, item);
				
				colnum++;
			}

			if (!(column_type = g_list_nth_data (column_types, colnum)))
				/* XXX Log this too... */
				continue;
	
			/* Ignore unloaded column types for the workspace */
			if (g_type_from_name (column_type->type_name) != G_TYPE_INVALID)
       			{
				/* XXX Do we need object properties to somehow work at load time here ??
				 * should we be doing this part in "finished" ? ... todo thinkso...
				 */
				value_str = glade_xml_get_content (col_node);
				value     = glade_utils_value_from_string 
					(g_type_from_name (column_type->type_name), value_str, widget->project, widget);
				g_free (value_str);
					
				data = glade_model_data_new (g_type_from_name (column_type->type_name), 
							     column_type->column_name);
			
				g_value_copy (value, &data->value);
				g_value_unset (value);
				g_free (value);
			}
			else
       			{
				data = glade_model_data_new (G_TYPE_INVALID, column_type->column_name);
       			}

			data->i18n_translatable = glade_xml_get_property_boolean (col_node, GLADE_TAG_TRANSLATABLE, FALSE);
			data->i18n_context = glade_xml_get_property_string (col_node, GLADE_TAG_CONTEXT);
			data->i18n_comment = glade_xml_get_property_string (col_node, GLADE_TAG_COMMENT);
			
			item = g_node_new (data);
			g_node_append (row, item);
				
			/* dont increment colnum on invalid xml tags... */
			colnum++;
		}
	}

	if (data_tree->children)
		glade_widget_property_set (widget, "data", data_tree);

	glade_model_data_tree_free (data_tree);
}

void
glade_gtk_store_read_widget (GladeWidgetAdaptor *adaptor,
			     GladeWidget        *widget,
			     GladeXmlNode       *node)
{
	if (!glade_xml_node_verify 
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and read in all the normal properties.. */
        GWA_GET_CLASS (G_TYPE_OBJECT)->read_widget (adaptor, widget, node);

	glade_gtk_store_read_columns (widget, node);

	if (GTK_IS_LIST_STORE (widget->object))
		glade_gtk_store_read_data (widget, node);
}

/*--------------------------- GtkCellRenderer ---------------------------------*/
static void glade_gtk_treeview_launch_editor (GObject *treeview);

void
glade_gtk_cell_renderer_action_activate (GladeWidgetAdaptor *adaptor,
					 GObject *object,
					 const gchar *action_path)
{
	if (strcmp (action_path, "launch_editor") == 0)
	{
		GladeWidget *w = glade_widget_get_from_gobject (object);
		
		while ((w = glade_widget_get_parent (w)))
		{
			if (GTK_IS_TREE_VIEW (w->object))
			{
				glade_gtk_treeview_launch_editor (w->object);
				break;
			}
		}
	}
	else
		GWA_GET_CLASS (G_TYPE_OBJECT)->action_activate (adaptor,
								object,
								action_path);
}


static void
renderer_format_changed (GladeProject *project, 
			 GParamSpec   *pspec,
			 GladeWidget  *gwidget)
{
	if (glade_project_get_format (project) == GLADE_PROJECT_FORMAT_GTKBUILDER)
		g_idle_add ((GSourceFunc)glade_gtk_cell_renderer_sync_attributes, gwidget->object);
}

static void
renderer_project_changed (GladeWidget *gwidget, 
			  GParamSpec  *pspec,
			  gpointer     userdata)
{
	GladeProject
		*project = glade_widget_get_project (gwidget),
		*old_project = g_object_get_data (G_OBJECT (gwidget), "renderer-project-ptr");
	
	if (old_project)
		g_signal_handlers_disconnect_by_func (G_OBJECT (old_project), 
						      G_CALLBACK (renderer_format_changed),
						      gwidget);

	if (project)
		g_signal_connect (G_OBJECT (project), "notify::format", 
				  G_CALLBACK (renderer_format_changed), gwidget);

	g_object_set_data (G_OBJECT (gwidget), "renderer-project-ptr", project);
}

void
glade_gtk_cell_renderer_deep_post_create (GladeWidgetAdaptor *adaptor, 
					  GObject            *object, 
					  GladeCreateReason   reason)
{
	GladePropertyClass  *pclass;
	GladeProperty       *property;
	GladeWidget         *widget;
	GList               *l;
	
	widget = glade_widget_get_from_gobject (object);

	for (l = adaptor->properties; l; l = l->next)
	{
		pclass = l->data;

		if (strncmp (pclass->id, "use-attr-", strlen ("use-attr-")) == 0)
		{
			property = glade_widget_get_property (widget, pclass->id);
			glade_property_sync (property);
		}
	}

	g_signal_connect (G_OBJECT (widget), "notify::project",
			  G_CALLBACK (renderer_project_changed), NULL);

	renderer_project_changed (widget, NULL, NULL);

}

GladeEditorProperty *
glade_gtk_cell_renderer_create_eprop (GladeWidgetAdaptor *adaptor,
				      GladePropertyClass *klass,
				      gboolean            use_command)
{
	GladeEditorProperty *eprop;

	if (strncmp (klass->id, "attr-", strlen ("attr-")) == 0)
		eprop = g_object_new (GLADE_TYPE_EPROP_CELL_ATTRIBUTE,
				      "property-class", klass, 
				      "use-command", use_command,
				      NULL);
	else
		eprop = GWA_GET_CLASS 
			(G_TYPE_OBJECT)->create_eprop (adaptor, 
						       klass, 
						       use_command);
	return eprop;
}


GladeEditable *
glade_gtk_cell_renderer_create_editable (GladeWidgetAdaptor  *adaptor,
					 GladeEditorPageType  type)
{
	GladeEditable *editable;

	/* Get base editable */
	editable = GWA_GET_CLASS (G_TYPE_OBJECT)->create_editable (adaptor, type);

	if (type == GLADE_PAGE_GENERAL || type == GLADE_PAGE_COMMON)
		return (GladeEditable *)glade_cell_renderer_editor_new (adaptor, type, editable);

	return editable;
}

static void
glade_gtk_cell_renderer_set_use_attribute (GObject      *object, 
					   const gchar  *property_name,
					   const GValue *value)
{
	GladeWidget *widget = glade_widget_get_from_gobject (object);
	gchar *attr_prop_name, *prop_msg, *attr_msg;

	attr_prop_name = g_strdup_printf ("attr-%s", property_name);

	prop_msg = g_strdup_printf (_("%s is set to load %s from the model"), 
				    widget->name, property_name);
	attr_msg = g_strdup_printf (_("%s is set to manipulate %s directly"), 
				    widget->name, attr_prop_name);

	glade_widget_property_set_sensitive (widget, property_name, FALSE, prop_msg);
	glade_widget_property_set_sensitive (widget, attr_prop_name, FALSE, attr_msg);

	if (g_value_get_boolean (value))
		glade_widget_property_set_sensitive (widget, attr_prop_name, TRUE, NULL);
	else
	{
		GladeProperty *property = glade_widget_get_property (widget, property_name);

		glade_property_set_sensitive (property, TRUE, NULL);
		glade_property_sync (property);
	}

	g_free (prop_msg);
	g_free (attr_msg);
	g_free (attr_prop_name);
}

static GladeProperty *
glade_gtk_cell_renderer_attribute_switch (GladeWidget *gwidget,
					  const gchar *property_name)
{
	GladeProperty *property;
	gchar         *use_attr_name = g_strdup_printf ("use-attr-%s", property_name);

	property = glade_widget_get_property (gwidget, use_attr_name);
	g_free (use_attr_name);

	return property;
}

static gboolean
glade_gtk_cell_renderer_property_enabled (GObject     *object,
					  const gchar *property_name)
{
	GladeProperty *property;
	GladeWidget   *gwidget = glade_widget_get_from_gobject (object);
	gboolean       use_attr = TRUE;

	if ((property = 
	     glade_gtk_cell_renderer_attribute_switch (gwidget, property_name)) != NULL)
		glade_property_get (property, &use_attr);

	return !use_attr;
}


void
glade_gtk_cell_renderer_set_property (GladeWidgetAdaptor *adaptor,
				      GObject *object,
				      const gchar *property_name,
				      const GValue *value)
{
	static gint use_attr_len = 0;
	static gint attr_len     = 0;

	if (!attr_len)
	{
		use_attr_len = strlen ("use-attr-");
		attr_len = strlen ("attr-");
	}

	if (strncmp (property_name, "use-attr-", use_attr_len) == 0)
		glade_gtk_cell_renderer_set_use_attribute (object, &property_name[use_attr_len], value);
	else if (strncmp (property_name, "attr-", attr_len) == 0)
		glade_gtk_cell_renderer_sync_attributes (object);
	else if (glade_gtk_cell_renderer_property_enabled (object, property_name))
		/* Chain Up */
		GWA_GET_CLASS (G_TYPE_OBJECT)->set_property (adaptor,
							     object,
							     property_name, 
							     value);
}

static void
glade_gtk_cell_renderer_write_properties (GladeWidget        *widget,
					  GladeXmlContext    *context,
					  GladeXmlNode       *node)
{
	GladeProperty *property, *prop;
	gchar *attr_name;
	GList *l;
	static gint attr_len = 0;

	if (!attr_len)
		attr_len = strlen ("attr-");

	for (l = widget->properties; l; l = l->next)
	{
		property = l->data;

		if (strncmp (property->klass->id, "attr-", attr_len) == 0)
		{
			gchar *use_attr_str;
			gboolean use_attr = FALSE;

			use_attr_str = g_strdup_printf ("use-%s", property->klass->id);
			glade_widget_property_get (widget, use_attr_str, &use_attr);

			attr_name  = &property->klass->id[attr_len];
			prop       = glade_widget_get_property (widget, attr_name);

			if (!use_attr && prop)
				glade_property_write (prop, context, node);

			g_free (use_attr_str);
		}
	}
}

void
glade_gtk_cell_renderer_write_widget (GladeWidgetAdaptor *adaptor,
				      GladeWidget        *widget,
				      GladeXmlContext    *context,
				      GladeXmlNode       *node)
{
	if (!glade_xml_node_verify
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* Write our normal properties, then chain up to write any other normal properties,
	 * then attributes 
	 */
	glade_gtk_cell_renderer_write_properties (widget, context, node);

        GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);
}

static void
glade_gtk_cell_renderer_parse_finished (GladeProject *project,
					GladeWidget  *widget)
{
	GladeProperty *property;
	GList *l;
	static gint attr_len = 0, use_attr_len = 0;

	/* Set "use-attr-*" everywhere that the object property is non-default 
	 *
	 * We do this in the finished handler because some properties may be
	 * object type properties (which may be anywhere in the glade file).
	 */
	if (!attr_len)
       	{
		attr_len = strlen ("attr-");
		use_attr_len = strlen ("use-attr-");
       	}

	for (l = widget->properties; l; l = l->next)
	{
		GladeProperty *switch_prop;
		property = l->data;

		if (strncmp (property->klass->id, "attr-", attr_len) != 0 &&
		    strncmp (property->klass->id, "use-attr-", use_attr_len) != 0 &&
		    (switch_prop = 
		     glade_gtk_cell_renderer_attribute_switch (widget, property->klass->id)) != NULL)
	       	{
			if (glade_property_original_default (property))
				glade_property_set (switch_prop, TRUE);
			else	
				glade_property_set (switch_prop, FALSE);
		}
	}
}

void
glade_gtk_cell_renderer_read_widget (GladeWidgetAdaptor *adaptor,
				     GladeWidget        *widget,
				     GladeXmlNode       *node)
{
	if (!glade_xml_node_verify 
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* First chain up and read in all the properties... */
        GWA_GET_CLASS (G_TYPE_OBJECT)->read_widget (adaptor, widget, node);

	g_signal_connect (widget->project, "parse-finished",
			  G_CALLBACK (glade_gtk_cell_renderer_parse_finished),
			  widget);
}

/*--------------------------- GtkCellLayout ---------------------------------*/
void
glade_gtk_cell_layout_add_child (GladeWidgetAdaptor *adaptor,
				 GObject            *container,
				 GObject            *child)
{
	GladeWidget *gmodel = NULL;
	GladeWidget *grenderer = glade_widget_get_from_gobject (child);

	if (GTK_IS_ICON_VIEW (container) &&
	    (gmodel = glade_cell_renderer_get_model (grenderer)) != NULL)
		gtk_icon_view_set_model (GTK_ICON_VIEW (container), NULL);

	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (container), GTK_CELL_RENDERER (child), TRUE);

	if (gmodel)
		gtk_icon_view_set_model (GTK_ICON_VIEW (container), GTK_TREE_MODEL (gmodel->object));

	glade_gtk_cell_renderer_sync_attributes (child);
}

void
glade_gtk_cell_layout_remove_child (GladeWidgetAdaptor *adaptor,
				    GObject            *container,
				    GObject            *child)
{
	GtkCellLayout *layout = GTK_CELL_LAYOUT (container);
	GList *l, *children = gtk_cell_layout_get_cells (layout);
	
	/* Add a reference to every cell except the one we want to remove */
	for (l = children; l; l = g_list_next (l))
		if (l->data != child)
			g_object_ref (l->data);
		else
			l->data = NULL;

	/* remove every cell */
	gtk_cell_layout_clear (layout);

	/* pack others cell renderers */
	for (l = children; l; l = g_list_next (l))
	{
		if (l->data == NULL) continue;
		
		gtk_cell_layout_pack_start (layout, 
					    GTK_CELL_RENDERER (l->data), TRUE);

		/* Remove our transient reference */
		g_object_unref (l->data);
	}

	g_list_free (children);
}

GList *
glade_gtk_cell_layout_get_children (GladeWidgetAdaptor  *adaptor,
				    GObject             *container)
{
	return gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (container));
}


void
glade_gtk_cell_layout_get_child_property (GladeWidgetAdaptor *adaptor,
					  GObject            *container,
					  GObject            *child,
					  const gchar        *property_name,
					  GValue             *value)
{	
	if (strcmp (property_name, "position") == 0)
	{
		GList *cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (container));

		/* We have to fake it, assume we are loading and always return the last item */
		g_value_set_int (value, g_list_length (cells) - 1);

		g_list_free (cells);
	}
	else
		/* Chain Up */
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->child_get_property (adaptor, 
								  container, child,
								  property_name, value);
}

void
glade_gtk_cell_layout_set_child_property (GladeWidgetAdaptor *adaptor,
					  GObject            *container,
					  GObject            *child,
					  const gchar        *property_name,
					  const GValue       *value)
{
	if (strcmp (property_name, "position") == 0)
	{
		/* Need verify on position property !!! XXX */
		gtk_cell_layout_reorder (GTK_CELL_LAYOUT (container), GTK_CELL_RENDERER (child), 
					 g_value_get_int (value));
	}
	else
		/* Chain Up */
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->child_set_property (adaptor, 
								  container, child,
								  property_name, value);
}

static void
glade_gtk_cell_renderer_read_attributes (GladeWidget *widget, GladeXmlNode *node)
{
	GladeXmlNode *attrs_node;
	GladeProperty *attr_prop, *use_attr_prop;
	GladeXmlNode  *prop;
	gchar         *name, *column_str, *attr_prop_name, *use_attr_name;

	if ((attrs_node = glade_xml_search_child (node, GLADE_TAG_ATTRIBUTES)) == NULL)
		return;

	for (prop = glade_xml_node_get_children (attrs_node); prop;
	     prop = glade_xml_node_next (prop))
	{
		
		if (!glade_xml_node_verify_silent (prop, GLADE_TAG_ATTRIBUTE)) continue;
		
		name            = glade_xml_get_property_string_required (prop, GLADE_TAG_NAME, NULL);
		column_str      = glade_xml_get_content (prop);
		attr_prop_name  = g_strdup_printf ("attr-%s", name);
		use_attr_name   = g_strdup_printf ("use-attr-%s", name);

		attr_prop       = glade_widget_get_property (widget, attr_prop_name);
		use_attr_prop   = glade_widget_get_property (widget, use_attr_name);

		if (attr_prop && use_attr_prop)
		{	
			gboolean use_attribute = FALSE;
			glade_property_get (use_attr_prop, &use_attribute);

			if (use_attribute)
				glade_property_set (attr_prop, g_ascii_strtoll (column_str, NULL, 10));
		}

		g_free (name);
		g_free (column_str);
		g_free (attr_prop_name);
		g_free (use_attr_name);
		      
	}
}

void
glade_gtk_cell_layout_read_child (GladeWidgetAdaptor *adaptor,
				  GladeWidget        *widget,
				  GladeXmlNode       *node)
{
	GladeXmlNode *widget_node;
	GladeWidget  *child_widget;
	gchar        *internal_name;

	if (!glade_xml_node_verify (node, GLADE_XML_TAG_CHILD))
		return;

	internal_name = glade_xml_get_property_string (node, GLADE_XML_TAG_INTERNAL_CHILD);
	
	if ((widget_node = 
	     glade_xml_search_child
	     (node, GLADE_XML_TAG_WIDGET(glade_project_get_format(widget->project)))) != NULL)
	{
		/* Combo box is a special brand of cell-layout, it can also have the internal entry */
		if ((child_widget = glade_widget_read (widget->project, 
						       widget, widget_node, 
						       internal_name)) != NULL)
		{
			/* Dont set any packing properties on internal children here,
			 * its possible but just not relevant for known celllayouts...
			 * i.e. maybe GtkTreeViewColumn will expose the internal button ?
			 * but no need for packing properties there either.
			 */
			if (!internal_name)
			{
				glade_widget_add_child (widget, child_widget, FALSE);

				glade_gtk_cell_renderer_read_attributes (child_widget, node);

				g_idle_add ((GSourceFunc)glade_gtk_cell_renderer_sync_attributes, 
					    child_widget->object);
			}
		}
	}	
	g_free (internal_name);
}

static void
glade_gtk_cell_renderer_write_attributes (GladeWidget        *widget,
					  GladeXmlContext    *context,
					  GladeXmlNode       *node)
{
	GladeProperty *property;
	GladeXmlNode  *attrs_node;
	gchar *attr_name;
	GList *l;
	static gint attr_len = 0;

	if (!attr_len)
		attr_len = strlen ("attr-");

	attrs_node = glade_xml_node_new (context, GLADE_TAG_ATTRIBUTES);

	for (l = widget->properties; l; l = l->next)
	{
		property = l->data;

		if (strncmp (property->klass->id, "attr-", attr_len) == 0)
		{
			GladeXmlNode  *attr_node;
			gchar *column_str, *use_attr_str;
			gboolean use_attr = FALSE;

			use_attr_str = g_strdup_printf ("use-%s", property->klass->id);
			glade_widget_property_get (widget, use_attr_str, &use_attr);

			if (use_attr && g_value_get_int (property->value) >= 0)
			{
				column_str   = g_strdup_printf ("%d", g_value_get_int (property->value));
				attr_name    = &property->klass->id[attr_len];
				
				attr_node = glade_xml_node_new (context, GLADE_TAG_ATTRIBUTE);
				glade_xml_node_append_child (attrs_node, attr_node);
				glade_xml_node_set_property_string (attr_node, GLADE_TAG_NAME,
								    attr_name);
				glade_xml_set_content (attr_node, column_str);
				g_free (column_str);
			}
			g_free (use_attr_str);
		}
	}

	if (!glade_xml_node_get_children (attrs_node))
		glade_xml_node_delete (attrs_node);
	else
		glade_xml_node_append_child (node, attrs_node);
}

void
glade_gtk_cell_layout_write_child (GladeWidgetAdaptor *adaptor,
				   GladeWidget        *widget,
				   GladeXmlContext    *context,
				   GladeXmlNode       *node)
{
	GladeXmlNode *child_node;

	child_node = glade_xml_node_new (context, GLADE_XML_TAG_CHILD);
	glade_xml_node_append_child (node, child_node);

	/* ComboBox can have an internal entry */
	if (widget->internal)
		glade_xml_node_set_property_string (child_node, 
						    GLADE_XML_TAG_INTERNAL_CHILD, 
						    widget->internal);

	/* Write out the widget */
	glade_widget_write (widget, context, child_node);

	glade_gtk_cell_renderer_write_attributes (widget, context, child_node);
}

static gchar *
glade_gtk_cell_layout_get_display_name (GladeBaseEditor *editor,
					GladeWidget *gchild,
					gpointer user_data)
{
	GObject *child = glade_widget_get_object (gchild);
	gchar *name;
	
	if (GTK_IS_TREE_VIEW_COLUMN (child))
		glade_widget_property_get (gchild, "title", &name);
	else
		name = gchild->name;
	
	return g_strdup (name);
}

static void
glade_gtk_cell_layout_child_selected (GladeBaseEditor *editor,
				      GladeWidget *gchild,
				      gpointer data)
{
	GObject *child = glade_widget_get_object (gchild);
	
	glade_base_editor_add_label (editor, GTK_IS_TREE_VIEW_COLUMN (child) ? 
				     _("Tree View Column") : _("Cell Renderer"));
	
	glade_base_editor_add_default_properties (editor, gchild);
	
	glade_base_editor_add_label (editor, GTK_IS_TREE_VIEW_COLUMN (child) ? 
				     _("Properties") : _("Properties and Attributes"));
	glade_base_editor_add_editable (editor, gchild, GLADE_PAGE_GENERAL);

	if (GTK_IS_CELL_RENDERER (child))
	{
		glade_base_editor_add_label (editor, _("Common Properties and Attributes"));
		glade_base_editor_add_editable (editor, gchild, GLADE_PAGE_COMMON);
	}
}

static gboolean
glade_gtk_cell_layout_move_child (GladeBaseEditor *editor,
				  GladeWidget *gparent,
				  GladeWidget *gchild,
				  gpointer data)
{	
	GObject *parent = glade_widget_get_object (gparent);
	GObject *child  = glade_widget_get_object (gchild);
	GList    list   = { 0, };

	if (GTK_IS_TREE_VIEW (parent) && !GTK_IS_TREE_VIEW_COLUMN (child))
		return FALSE;
	if (GTK_IS_CELL_LAYOUT (parent) && !GTK_IS_CELL_RENDERER (child))
		return FALSE;
	if (GTK_IS_CELL_RENDERER (parent))
		return FALSE;
	
	if (gparent != glade_widget_get_parent (gchild))
	{
		list.data = gchild;
		glade_command_dnd (&list, gparent, NULL);
	}

	return TRUE;
}

static void
glade_gtk_cell_layout_launch_editor (GObject  *layout)
{
	GladeWidget *widget = glade_widget_get_from_gobject (layout);
	GladeBaseEditor *editor;
	GladeEditable *layout_editor;
	GtkWidget *window;

	layout_editor = glade_widget_adaptor_create_editable (widget->adaptor, GLADE_PAGE_GENERAL);
	layout_editor = (GladeEditable *)glade_tree_view_editor_new (widget->adaptor, layout_editor);

	/* Editor */
	editor = glade_base_editor_new (layout, layout_editor,
					_("Text"), GTK_TYPE_CELL_RENDERER_TEXT,
					_("Accelerator"), GTK_TYPE_CELL_RENDERER_ACCEL,
					_("Combo"), GTK_TYPE_CELL_RENDERER_COMBO,
					_("Spin"),  GTK_TYPE_CELL_RENDERER_SPIN,
					_("Pixbuf"), GTK_TYPE_CELL_RENDERER_PIXBUF,
					_("Progress"), GTK_TYPE_CELL_RENDERER_PROGRESS,
					_("Toggle"), GTK_TYPE_CELL_RENDERER_TOGGLE,
					_("Spinner"), GTK_TYPE_CELL_RENDERER_SPINNER,
					NULL);

	g_signal_connect (editor, "get-display-name", G_CALLBACK (glade_gtk_cell_layout_get_display_name), NULL);
	g_signal_connect (editor, "child-selected", G_CALLBACK (glade_gtk_cell_layout_child_selected), NULL);
	g_signal_connect (editor, "move-child", G_CALLBACK (glade_gtk_cell_layout_move_child), NULL);

	gtk_widget_show (GTK_WIDGET (editor));
	
	window = glade_base_editor_pack_new_window (editor, 
						    GTK_IS_ICON_VIEW (layout) ? 
						    _("Icon View Editor") : _("Combo Editor"),
						    NULL);
	gtk_widget_show (window);
}


static void
glade_gtk_cell_layout_launch_editor_action (GObject *object)
{
	GladeWidget *w = glade_widget_get_from_gobject (object);
	
	do
	{
		if (GTK_IS_TREE_VIEW (w->object))
		{
			glade_gtk_treeview_launch_editor (w->object);
			break;
		} 
		else if (GTK_IS_ICON_VIEW (w->object))
		{
			glade_gtk_cell_layout_launch_editor (w->object);
			break;
		}
		else if (GTK_IS_COMBO_BOX (w->object))
		{
			glade_gtk_cell_layout_launch_editor (w->object);
			break;
		}

	} while ((w = glade_widget_get_parent (w)));
}

void
glade_gtk_cell_layout_action_activate (GladeWidgetAdaptor *adaptor,
				       GObject *object,
				       const gchar *action_path)
{
	if (strcmp (action_path, "launch_editor") == 0)
		glade_gtk_cell_layout_launch_editor_action (object);
	else
		GWA_GET_CLASS (G_TYPE_OBJECT)->action_activate (adaptor,
								object,
								action_path);
}

void
glade_gtk_cell_layout_action_activate_as_widget (GladeWidgetAdaptor *adaptor,
						 GObject *object,
						 const gchar *action_path)
{
	if (strcmp (action_path, "launch_editor") == 0)
		glade_gtk_cell_layout_launch_editor_action (object);
	else
		GWA_GET_CLASS (GTK_TYPE_WIDGET)->action_activate (adaptor,
								  object,
								  action_path);
}



/*--------------------------- GtkTreeView ---------------------------------*/
static void
glade_gtk_treeview_launch_editor (GObject  *treeview)
{
	GladeWidget *widget = glade_widget_get_from_gobject (treeview);
	GladeBaseEditor *editor;
	GladeEditable *treeview_editor;
	GtkWidget *window;


	treeview_editor = glade_widget_adaptor_create_editable (widget->adaptor, GLADE_PAGE_GENERAL);
	treeview_editor = (GladeEditable *)glade_tree_view_editor_new (widget->adaptor, treeview_editor);

	/* Editor */
	editor = glade_base_editor_new (treeview, treeview_editor,
					_("Column"), GTK_TYPE_TREE_VIEW_COLUMN,
					NULL);

	glade_base_editor_append_types (editor, GTK_TYPE_TREE_VIEW_COLUMN,
					_("Text"), GTK_TYPE_CELL_RENDERER_TEXT,
					_("Accelerator"), GTK_TYPE_CELL_RENDERER_ACCEL,
					_("Combo"), GTK_TYPE_CELL_RENDERER_COMBO,
					_("Spin"),  GTK_TYPE_CELL_RENDERER_SPIN,
					_("Pixbuf"), GTK_TYPE_CELL_RENDERER_PIXBUF,
					_("Progress"), GTK_TYPE_CELL_RENDERER_PROGRESS,
					_("Toggle"), GTK_TYPE_CELL_RENDERER_TOGGLE,
					_("Spinner"), GTK_TYPE_CELL_RENDERER_SPINNER,
					NULL);

	g_signal_connect (editor, "get-display-name", G_CALLBACK (glade_gtk_cell_layout_get_display_name), NULL);
	g_signal_connect (editor, "child-selected", G_CALLBACK (glade_gtk_cell_layout_child_selected), NULL);
	g_signal_connect (editor, "move-child", G_CALLBACK (glade_gtk_cell_layout_move_child), NULL);

	gtk_widget_show (GTK_WIDGET (editor));
	
	window = glade_base_editor_pack_new_window (editor, _("Tree View Editor"), NULL);
	gtk_widget_show (window);
}

void
glade_gtk_treeview_action_activate (GladeWidgetAdaptor *adaptor,
				    GObject *object,
				    const gchar *action_path)
{
	if (strcmp (action_path, "launch_editor") == 0)
	{
		glade_gtk_treeview_launch_editor (object);
	}
	else
		GWA_GET_CLASS (GTK_TYPE_CONTAINER)->action_activate (adaptor,
								     object,
								     action_path);
}

static gint
glade_gtk_treeview_get_column_index (GtkTreeView       *view,
				     GtkTreeViewColumn *column)
{
	GtkTreeViewColumn *iter;
	gint i;

	for (i = 0; (iter = gtk_tree_view_get_column (view, i)) != NULL; i++)
		if (iter == column)
			return i;

	return -1;
}

void
glade_gtk_treeview_get_child_property (GladeWidgetAdaptor *adaptor,
				       GObject            *container,
				       GObject            *child,
				       const gchar        *property_name,
				       GValue             *value)
{	
	if (strcmp (property_name, "position") == 0)
		g_value_set_int (value,
				 glade_gtk_treeview_get_column_index (GTK_TREE_VIEW (container), 
								      GTK_TREE_VIEW_COLUMN (child)));
	else
		/* Chain Up */
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->child_get_property (adaptor, 
								  container, child,
								  property_name, value);
}

void
glade_gtk_treeview_set_child_property (GladeWidgetAdaptor *adaptor,
				       GObject            *container,
				       GObject            *child,
				       const gchar        *property_name,
				       const GValue       *value)
{
	if (strcmp (property_name, "position") == 0)
	{

		gtk_tree_view_remove_column (GTK_TREE_VIEW (container),
					     GTK_TREE_VIEW_COLUMN (child));
		gtk_tree_view_insert_column (GTK_TREE_VIEW (container),
					     GTK_TREE_VIEW_COLUMN (child),
					     g_value_get_int (value));
	}
	else
		/* Chain Up */
		GWA_GET_CLASS
			(GTK_TYPE_CONTAINER)->child_set_property (adaptor, 
								  container, child,
								  property_name, value);
}

GList *
glade_gtk_treeview_get_children (GladeWidgetAdaptor *adaptor,
				 GtkTreeView        *view)
{
	return gtk_tree_view_get_columns (view);
}

/* XXX FIXME: We should hide the actual "fixed-height-mode" setting from
 * treeview editors and provide a custom control that sets all its columns
 * to fixed size and then control the column's sensitivity accordingly.
 */
#define INSENSITIVE_COLUMN_SIZING_MSG \
	_("Columns must have a fixed size inside a treeview with fixed height mode set")

void
glade_gtk_treeview_add_child (GladeWidgetAdaptor *adaptor,
			      GObject *container,
			      GObject *child)
{
	GtkTreeView *view = GTK_TREE_VIEW (container);
	GtkTreeViewColumn *column;
	GladeWidget       *gcolumn;

	if (!GTK_IS_TREE_VIEW_COLUMN (child))
		return;

	if (gtk_tree_view_get_fixed_height_mode (view))
	{
		gcolumn = glade_widget_get_from_gobject (child);
		glade_widget_property_set (gcolumn, "sizing", GTK_TREE_VIEW_COLUMN_FIXED);
		glade_widget_property_set_sensitive (gcolumn, "sizing", FALSE,
						     INSENSITIVE_COLUMN_SIZING_MSG);
	}

	column = GTK_TREE_VIEW_COLUMN (child);
	gtk_tree_view_append_column (view, column);

	glade_gtk_cell_layout_sync_attributes (G_OBJECT (column));
}

void
glade_gtk_treeview_remove_child (GladeWidgetAdaptor *adaptor,
				 GObject *container,
				 GObject *child)
{
	GtkTreeView *view = GTK_TREE_VIEW (container);
	GtkTreeViewColumn *column;

	if (!GTK_IS_TREE_VIEW_COLUMN (child))
		return;

	column = GTK_TREE_VIEW_COLUMN (child);
	gtk_tree_view_remove_column (view, column);
}

/*--------------------------- GtkAdjustment ---------------------------------*/
void
glade_gtk_adjustment_write_widget (GladeWidgetAdaptor *adaptor,
				   GladeWidget        *widget,
				   GladeXmlContext    *context,
				   GladeXmlNode       *node)
{
	GladeProperty *prop;

	if (!glade_xml_node_verify
	    (node, GLADE_XML_TAG_WIDGET (glade_project_get_format (widget->project))))
		return;

	/* Ensure proper order of adjustment properties by writing them here. */
	prop = glade_widget_get_property (widget, "lower");
	glade_property_write (prop, context, node);

	prop = glade_widget_get_property (widget, "upper");
	glade_property_write (prop, context, node);

	prop = glade_widget_get_property (widget, "value");
	glade_property_write (prop, context, node);

        GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);
}


/*--------------------------- GtkAction ---------------------------------*/
#define ACTION_ACCEL_INSENSITIVE_MSG _("The accelerator can only be set when inside an Action Group.")

void
glade_gtk_action_post_create (GladeWidgetAdaptor *adaptor, 
			      GObject            *object, 
			      GladeCreateReason   reason)
{
	GladeWidget *gwidget = glade_widget_get_from_gobject (object);

	if (reason == GLADE_CREATE_REBUILD)
		return;

	if (!gtk_action_get_name (GTK_ACTION (object)))
		glade_widget_property_set (gwidget, "name", "untitled");

	glade_widget_set_action_sensitive (gwidget, "launch_editor", FALSE);
	glade_widget_property_set_sensitive (gwidget, "accelerator", FALSE, 
					     ACTION_ACCEL_INSENSITIVE_MSG);
}

/*--------------------------- GtkActionGroup ---------------------------------*/
void
glade_gtk_action_group_add_child (GladeWidgetAdaptor *adaptor,
				  GObject            *container,
				  GObject            *child)
{
	if (GTK_IS_ACTION (child))
	{
		/* Dont really add/remove actions (because name conflicts inside groups)
		 */
		GladeWidget *ggroup  = glade_widget_get_from_gobject (container);
		GladeWidget *gaction = glade_widget_get_from_gobject (child);
		GList       *actions = g_object_get_data (G_OBJECT (ggroup), "glade-actions");

		actions = g_list_copy (actions);
		actions = g_list_prepend (actions, child);

		g_object_set_data_full (G_OBJECT (ggroup), "glade-actions", actions, 
					(GDestroyNotify)g_list_free);

		glade_widget_property_set_sensitive (gaction, "accelerator", TRUE, NULL);
		glade_widget_set_action_sensitive (gaction, "launch_editor", TRUE);
	}
}

void
glade_gtk_action_group_remove_child (GladeWidgetAdaptor *adaptor,
				     GObject            *container,
				     GObject            *child)
{
	if (GTK_IS_ACTION (child))
	{
		/* Dont really add/remove actions (because name conflicts inside groups)
		 */
		GladeWidget *ggroup = glade_widget_get_from_gobject (container);
		GladeWidget *gaction = glade_widget_get_from_gobject (child);
		GList       *actions = g_object_get_data (G_OBJECT (ggroup), "glade-actions");

		actions = g_list_copy (actions);
		actions = g_list_remove (actions, child);

		g_object_set_data_full (G_OBJECT (ggroup), "glade-actions", actions, 
					(GDestroyNotify)g_list_free);

		glade_widget_property_set_sensitive (gaction, "accelerator", FALSE, 
						     ACTION_ACCEL_INSENSITIVE_MSG);
		glade_widget_set_action_sensitive (gaction, "launch_editor", FALSE);
	}
}

void
glade_gtk_action_group_replace_child (GladeWidgetAdaptor *adaptor,
				      GObject            *container,
				      GObject            *current,
				      GObject            *new_action)
{
	glade_gtk_action_group_remove_child (adaptor, container, current);
	glade_gtk_action_group_add_child (adaptor, container, new_action);
}

GList *
glade_gtk_action_group_get_children (GladeWidgetAdaptor  *adaptor,
				     GObject        *container)
{
	GladeWidget *ggroup  = glade_widget_get_from_gobject (container);
	GList       *actions = g_object_get_data (G_OBJECT (ggroup), "glade-actions");

	return g_list_copy (actions);
}


void
glade_gtk_action_group_read_child (GladeWidgetAdaptor *adaptor,
				   GladeWidget        *widget,
				   GladeXmlNode       *node)
{
	GladeXmlNode *widget_node;
	GladeWidget  *child_widget;

	if (!glade_xml_node_verify (node, GLADE_XML_TAG_CHILD))
		return;
	
	if ((widget_node = 
	     glade_xml_search_child
	     (node, GLADE_XML_TAG_WIDGET(glade_project_get_format(widget->project)))) != NULL)
	{
		if ((child_widget = glade_widget_read (widget->project, 
						       widget, widget_node, 
						       NULL)) != NULL)
		{
			glade_widget_add_child (widget, child_widget, FALSE);

			/* Read in accelerator */
			glade_gtk_read_accels (child_widget, node, FALSE);
		}
	}
}


void
glade_gtk_action_group_write_child (GladeWidgetAdaptor *adaptor,
				    GladeWidget        *widget,
				    GladeXmlContext    *context,
				    GladeXmlNode       *node)
{
	GladeXmlNode *child_node;

	child_node = glade_xml_node_new (context, GLADE_XML_TAG_CHILD);
	glade_xml_node_append_child (node, child_node);

	/* Write out the widget */
	glade_widget_write (widget, context, child_node);

	/* Write accelerator here  */
	glade_gtk_write_accels (widget, context, child_node, FALSE);		
}

static void
glade_gtk_action_child_selected (GladeBaseEditor *editor,
				 GladeWidget *gchild,
				 gpointer data)
{
	glade_base_editor_add_label (editor, _("Action"));
	
	glade_base_editor_add_default_properties (editor, gchild);
	
	glade_base_editor_add_label (editor, _("Properties"));
	glade_base_editor_add_editable (editor, gchild, GLADE_PAGE_GENERAL);
}

static gboolean
glade_gtk_action_move_child (GladeBaseEditor *editor,
			     GladeWidget *gparent,
			     GladeWidget *gchild,
			     gpointer data)
{	
	return FALSE;
}

static void
glade_gtk_action_launch_editor (GObject  *action)
{
	GladeWidget *widget = glade_widget_get_from_gobject (action);
	GladeBaseEditor *editor;
	GladeEditable *action_editor;
	GtkWidget *window;

	/* Make sure we get the group here */
	widget = glade_widget_get_toplevel (widget);

	action_editor = glade_widget_adaptor_create_editable (widget->adaptor, GLADE_PAGE_GENERAL);

	/* Editor */
	editor = glade_base_editor_new (widget->object, action_editor,
					_("Action"), GTK_TYPE_ACTION,
					_("Toggle"), GTK_TYPE_TOGGLE_ACTION,
					_("Radio"), GTK_TYPE_RADIO_ACTION,
					_("Recent"), GTK_TYPE_RECENT_ACTION,
					NULL);

	g_signal_connect (editor, "child-selected", G_CALLBACK (glade_gtk_action_child_selected), NULL);
	g_signal_connect (editor, "move-child", G_CALLBACK (glade_gtk_action_move_child), NULL);

	gtk_widget_show (GTK_WIDGET (editor));
	
	window = glade_base_editor_pack_new_window (editor, _("Action Group Editor"), NULL);
	gtk_widget_show (window);
}


void
glade_gtk_action_action_activate (GladeWidgetAdaptor *adaptor,
				  GObject *object,
				  const gchar *action_path)
{
	if (strcmp (action_path, "launch_editor") == 0)
	{
		glade_gtk_action_launch_editor (object);
	}
}
