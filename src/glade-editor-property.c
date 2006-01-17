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
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-editor-property.h"
#include "glade-property.h"
#include "glade-command.h"
#include "glade-project.h"
#include "glade-builtins.h"

enum {
	PROP_0,
	PROP_PROPERTY_CLASS,
	PROP_USE_COMMAND
};


static GtkTableClass             *table_class;
static GladeEditorPropertyClass  *editor_property_class;

/* For setting insensitive background on labels without setting them
 * insensitive (tooltips dont work on insensitive widgets at this time)
 */
static GdkColor               *insensitive_colour = NULL;
static GdkColor               *normal_colour      = NULL;


#define GLADE_PROPERTY_TABLE_ROW_SPACING 2
#define FLAGS_COLUMN_SETTING             0
#define FLAGS_COLUMN_SYMBOL              1

/*******************************************************************************
                Boiler plate macros (inspired from glade-command.c)
 *******************************************************************************/
#define MAKE_TYPE(func, type, parent)			\
GType							\
func ## _get_type (void)				\
{							\
	static GType cmd_type = 0;			\
							\
	if (!cmd_type)					\
	{						\
		static const GTypeInfo info =		\
		{					\
			sizeof (type ## Class),		\
			(GBaseInitFunc) NULL,		\
			(GBaseFinalizeFunc) NULL,	\
			(GClassInitFunc) func ## _class_init,	\
			(GClassFinalizeFunc) NULL,	\
			NULL,				\
			sizeof (type),			\
			0,				\
			(GInstanceInitFunc) NULL	\
		};					\
							\
		cmd_type = g_type_register_static (parent, #type, &info, 0);	\
	}						\
							\
	return cmd_type;				\
}							\


#define GLADE_MAKE_EPROP(type, func)					\
static void								\
func ## _finalize (GObject *object);					\
static void								\
func ## _load (GladeEditorProperty *me, GladeProperty *property);	\
static GtkWidget *							\
func ## _create_input (GladeEditorProperty *me);			\
static void								\
func ## _class_init (gpointer parent_tmp, gpointer notused)		\
{									\
	GladeEditorPropertyClass *parent = parent_tmp;			\
	GObjectClass* object_class;					\
	object_class = G_OBJECT_CLASS (parent);				\
	parent->load =  func ## _load;					\
	parent->create_input =  func ## _create_input;			\
	object_class->finalize = func ## _finalize;			\
}									\
typedef struct {							\
	GladeEditorPropertyClass cmd;					\
} type ## Class;							\
static MAKE_TYPE(func, type, GLADE_TYPE_EDITOR_PROPERTY)




/*******************************************************************************
                               GladeEditorPropertyClass
 *******************************************************************************/
static void
glade_editor_property_tooltip_cb (GladeProperty       *property,
				  const gchar         *tooltip,
				  GladeEditorProperty *eprop)
{
	glade_util_widget_set_tooltip (eprop->input, tooltip);
	glade_util_widget_set_tooltip (eprop->eventbox, tooltip);
}

static void
glade_editor_property_sensitivity_cb (GladeProperty       *property,
				      GParamSpec          *pspec,
				      GladeEditorProperty *eprop)
{
	gboolean sensitive = glade_property_get_sensitive (eprop->property);

	gtk_widget_modify_fg 
		(GTK_WIDGET (eprop->item_label), 
		 GTK_STATE_NORMAL, 
		 sensitive ? normal_colour : insensitive_colour);
	gtk_widget_set_sensitive (eprop->input, sensitive);
}

static void
glade_editor_property_value_changed_cb (GladeProperty       *property,
					GValue              *value,
					GladeEditorProperty *eprop)
{
	g_assert (eprop->property == property);
	glade_editor_property_load (eprop, eprop->property);
}

static void
glade_editor_property_enabled_cb (GladeProperty       *property,
				  GParamSpec          *pspec,
				  GladeEditorProperty *eprop)
{
	gboolean enabled;
	g_assert (eprop->property == property);

	if (eprop->class->optional)
	{
		enabled = glade_property_get_enabled (property);
		gtk_widget_set_sensitive (eprop->input, enabled);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (eprop->check), enabled);
	}
}

static void
glade_editor_property_closed_cb (GladeProject        *project,
				 GladeEditorProperty *eprop)
{
	/* Detected project this property belongs to was closed.
	 * detatch from eprop.
	 */
	glade_editor_property_load (eprop, NULL);
}

static void
glade_editor_property_enabled_toggled_cb (GtkWidget           *check,
					  GladeEditorProperty *eprop)
{
	glade_property_set_enabled (eprop->property, 
				    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)));
}

static GObject *
glade_editor_property_constructor (GType                  type,
				   guint                  n_construct_properties,
				   GObjectConstructParam *construct_properties)
{
	GObject             *obj;
	GladeEditorProperty *eprop;
	gchar               *text;

	/* Invoke parent constructor (eprop->class should be resolved by this point) . */
	obj = G_OBJECT_CLASS (table_class)->constructor
		(type, n_construct_properties, construct_properties);
	
	eprop = GLADE_EDITOR_PROPERTY (obj);

	/* Create label */
	text = g_strdup_printf ("%s :", eprop->class->name);
	eprop->item_label = gtk_label_new (text);
	eprop->eventbox   = gtk_event_box_new ();
	g_free (text);

	/* keep our own reference */
	g_object_ref (G_OBJECT (eprop->eventbox));

	gtk_misc_set_alignment (GTK_MISC (eprop->item_label), 1.0, 0.0);
	gtk_container_add (GTK_CONTAINER (eprop->eventbox), eprop->item_label);

	/* Create hbox and possibly check button
	 */
	if (eprop->class->optional)
	{
		eprop->check = gtk_check_button_new ();
		gtk_box_pack_start (GTK_BOX (eprop), eprop->check, FALSE, FALSE, 0);
		g_signal_connect (G_OBJECT (eprop->check), "toggled", 
				  G_CALLBACK (glade_editor_property_enabled_toggled_cb), eprop);

	}
	eprop->input = GLADE_EDITOR_PROPERTY_GET_CLASS (eprop)->create_input (eprop);
	gtk_box_pack_start (GTK_BOX (eprop), eprop->input, TRUE, TRUE, 0);

	return obj;
}

static void
glade_editor_property_finalize (GObject *object)
{
	GladeEditorProperty *eprop = GLADE_EDITOR_PROPERTY (object);

	/* detatch from loaded property */
	glade_editor_property_load (eprop, NULL);

	G_OBJECT_CLASS (table_class)->finalize (object);
}

static void
glade_editor_property_set_property (GObject      *object,
				    guint         prop_id,
				    const GValue *value,
				    GParamSpec   *pspec)
{
	GladeEditorProperty *eprop = GLADE_EDITOR_PROPERTY (object);

	switch (prop_id)
	{
	case PROP_PROPERTY_CLASS:
		eprop->class = g_value_get_pointer (value);
		break;
	case PROP_USE_COMMAND:
		eprop->use_command = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_editor_property_get_property (GObject    *object,
				    guint       prop_id,
				    GValue     *value,
				    GParamSpec *pspec)
{
	GladeEditorProperty *eprop = GLADE_EDITOR_PROPERTY (object);

	switch (prop_id)
	{
	case PROP_PROPERTY_CLASS:
		g_value_set_pointer (value, eprop->class);
		break;
	case PROP_USE_COMMAND:
		g_value_set_boolean (value, eprop->use_command);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_editor_property_load_common (GladeEditorProperty *eprop, 
				   GladeProperty       *property)
{
	GladeProject *project;
	
	/* disconnect anything from previously loaded property */
	if (eprop->property != property && eprop->property != NULL) 
	{
		project = glade_widget_get_project (eprop->property->widget);

		if (eprop->tooltip_id > 0)
			g_signal_handler_disconnect (G_OBJECT (eprop->property),
						     eprop->tooltip_id);
		if (eprop->sensitive_id > 0)
			g_signal_handler_disconnect (eprop->property, 
						     eprop->sensitive_id);
		if (eprop->changed_id > 0)
			g_signal_handler_disconnect (eprop->property, 
						     eprop->changed_id);
		if (eprop->enabled_id > 0)
			g_signal_handler_disconnect (eprop->property, 
						     eprop->enabled_id);
		if (eprop->closed_id > 0)
			g_signal_handler_disconnect (project, eprop->closed_id);

		eprop->tooltip_id   = 0;
		eprop->sensitive_id = 0;
		eprop->changed_id   = 0;
		eprop->enabled_id   = 0;
		eprop->closed_id   = 0;
	}

	eprop->property = NULL;
	
	/* Connect new stuff, deal with tooltip
	 */
	if (eprop->property != property && property != NULL)
	{
		eprop->property = property;
		project = glade_widget_get_project (eprop->property->widget);

		eprop->tooltip_id = 
			g_signal_connect (G_OBJECT (eprop->property),
					  "tooltip-changed", 
					  G_CALLBACK (glade_editor_property_tooltip_cb),
					  eprop);
		eprop->sensitive_id =
			g_signal_connect (G_OBJECT (eprop->property),
					  "notify::sensitive", 
					  G_CALLBACK (glade_editor_property_sensitivity_cb),
					  eprop);
		eprop->changed_id =
			g_signal_connect (G_OBJECT (eprop->property),
					  "value-changed", 
					  G_CALLBACK (glade_editor_property_value_changed_cb),
					  eprop);
		eprop->enabled_id =
			g_signal_connect (G_OBJECT (eprop->property),
					  "notify::enabled", 
					  G_CALLBACK (glade_editor_property_enabled_cb),
					  eprop);
		eprop->closed_id =
			g_signal_connect (G_OBJECT (project), "close", 
					  G_CALLBACK (glade_editor_property_closed_cb),
					  eprop);

		/* Load initial tooltips
		 */
		glade_editor_property_tooltip_cb
			(property, glade_property_get_tooltip (property), eprop);
		
		/* Load initial enabled state
		 */
		glade_editor_property_enabled_cb (property, NULL, eprop);
		
		/* Load initial sensitive state.
		 */
		glade_editor_property_sensitivity_cb (property, NULL, eprop);
	}
}

static void
glade_editor_property_init (GladeEditorProperty *eprop)
{

}

static void
glade_editor_property_class_init (GladeEditorPropertyClass *eprop_class)
{
	GObjectClass       *object_class;
	GtkWidget          *label;
	g_return_if_fail (eprop_class != NULL);

	/* Both parent classes assigned here.
	 */
	editor_property_class = eprop_class;
	table_class           = g_type_class_peek_parent (eprop_class);

	object_class          = G_OBJECT_CLASS (eprop_class);

	/* GObjectClass */
	object_class->constructor  = glade_editor_property_constructor;
	object_class->finalize     = glade_editor_property_finalize;
	object_class->get_property = glade_editor_property_get_property;
	object_class->set_property = glade_editor_property_set_property;

	/* Class methods */
	eprop_class->load          = glade_editor_property_load_common;
	eprop_class->create_input  = NULL;

	/* Properties */
	g_object_class_install_property 
		(object_class, PROP_PROPERTY_CLASS,
		 g_param_spec_pointer 
		 ("property-class", _("Property Class"), 
		  _("The GladePropertyClass this GladeEditorProperty was created for"),
		  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property 
		(object_class, PROP_USE_COMMAND,
		 g_param_spec_boolean 
		 ("use-command", _("Use Command"), 
		  _("Whether we should use the command API for the undo/redo stack"),
		  FALSE, G_PARAM_READWRITE));

	/* Resolve label colors once class wide.
	 * (is this called more than once ??? XXX)
	 */
	label = gtk_label_new ("");
	insensitive_colour = gdk_color_copy (&(GTK_WIDGET (label)->style->fg[GTK_STATE_INSENSITIVE]));
	normal_colour      = gdk_color_copy (&(GTK_WIDGET (label)->style->fg[GTK_STATE_NORMAL]));
	gtk_widget_destroy (label);
}


GType
glade_editor_property_get_type (void)
{
	static GType property_type = 0;
	
	if (!property_type)
	{
		static const GTypeInfo property_info = 
		{
			sizeof (GladeEditorPropertyClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) glade_editor_property_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,           /* class_data */
			sizeof (GladeEditorProperty),
			0,              /* n_preallocs */
			(GInstanceInitFunc) glade_editor_property_init,
		};
		property_type = 
			g_type_register_static (GTK_TYPE_HBOX,
						"GladeEditorProperty",
						&property_info, 0);
	}
	return property_type;
}

/*******************************************************************************
                        GladeEditorPropertyNumericClass
 *******************************************************************************/
typedef struct {
	GladeEditorProperty  parent_instance;

	GtkWidget           *spin;
} GladeEPropNumeric;

GLADE_MAKE_EPROP (GladeEPropNumeric, glade_eprop_numeric)
#define GLADE_TYPE_EPROP_NUMERIC            (glade_eprop_numeric_get_type())
#define GLADE_EPROP_NUMERIC(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_NUMERIC, GladeEPropNumeric))
#define GLADE_EPROP_NUMERIC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_NUMERIC, GladeEPropNumericClass))
#define GLADE_IS_EPROP_NUMERIC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_NUMERIC))
#define GLADE_IS_EPROP_NUMERIC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_NUMERIC))
#define GLADE_EPROP_NUMERIC_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_NUMERIC, GladeEPropNumericClass))

static void
glade_eprop_numeric_finalize (GObject *object)
{
	/* Chain up */
	G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_numeric_load (GladeEditorProperty *eprop, GladeProperty *property)
{
	gfloat val = 0.0F;
	GladeEPropNumeric *eprop_numeric = GLADE_EPROP_NUMERIC (eprop);

	/* Chain up first */
	editor_property_class->load (eprop, property);

	if (property)
	{
		if (G_IS_PARAM_SPEC_INT(eprop->class->pspec))
			val = (gfloat) g_value_get_int (property->value);
		else if (G_IS_PARAM_SPEC_UINT(eprop->class->pspec))
			val = (gfloat) g_value_get_uint (property->value);		
		else if (G_IS_PARAM_SPEC_LONG(eprop->class->pspec))
			val = (gfloat) g_value_get_long (property->value);		
		else if (G_IS_PARAM_SPEC_ULONG(eprop->class->pspec))
			val = (gfloat) g_value_get_ulong (property->value);		
		else if (G_IS_PARAM_SPEC_INT64(eprop->class->pspec))
			val = (gfloat) g_value_get_int64 (property->value);		
		else if (G_IS_PARAM_SPEC_UINT64(eprop->class->pspec))
			val = (gfloat) g_value_get_uint64 (property->value);		
		else if (G_IS_PARAM_SPEC_DOUBLE(eprop->class->pspec))
			val = (gfloat) g_value_get_double (property->value);
		else if (G_IS_PARAM_SPEC_FLOAT(eprop->class->pspec))
			val = g_value_get_float (property->value);
		else
			g_warning ("Unsupported type %s\n",
				   g_type_name(G_PARAM_SPEC_TYPE (eprop->class->pspec)));
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (eprop_numeric->spin), val);
	}
}


static void
glade_eprop_numeric_changed (GtkWidget *spin,
			     GladeEditorProperty *eprop)
{
	GValue val = { 0, };

	if (eprop->loading) return;

	g_value_init (&val, eprop->class->pspec->value_type);
	
	if (G_IS_PARAM_SPEC_INT(eprop->class->pspec))
		g_value_set_int (&val, gtk_spin_button_get_value_as_int
				 (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_UINT(eprop->class->pspec))
		g_value_set_uint (&val, gtk_spin_button_get_value_as_int
				  (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_LONG(eprop->class->pspec))
		g_value_set_long (&val, (glong)gtk_spin_button_get_value_as_int
				  (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_ULONG(eprop->class->pspec))
		g_value_set_ulong (&val, (gulong)gtk_spin_button_get_value_as_int
				   (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_INT64(eprop->class->pspec))
		g_value_set_int64 (&val, (gint64)gtk_spin_button_get_value_as_int
				  (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_UINT64(eprop->class->pspec))
		g_value_set_uint64 (&val, (guint64)gtk_spin_button_get_value_as_int
				   (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_FLOAT(eprop->class->pspec))
		g_value_set_float (&val, (gfloat) gtk_spin_button_get_value
				   (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_DOUBLE(eprop->class->pspec))
		g_value_set_double (&val, gtk_spin_button_get_value
				    (GTK_SPIN_BUTTON (spin)));
	else
		g_warning ("Unsupported type %s\n",
			   g_type_name(G_PARAM_SPEC_TYPE (eprop->class->pspec)));

	if (eprop->use_command == FALSE)
		glade_property_set_value (eprop->property, &val);
	else
		glade_command_set_property (eprop->property, &val);

	g_value_unset (&val);
}

static GtkWidget *
glade_eprop_numeric_create_input (GladeEditorProperty *eprop)
{
	GladeEPropNumeric *eprop_numeric = GLADE_EPROP_NUMERIC (eprop);
	GtkAdjustment     *adjustment;

	adjustment          = glade_property_class_make_adjustment (eprop->class);
	eprop_numeric->spin = gtk_spin_button_new (adjustment, 4,
						   G_IS_PARAM_SPEC_FLOAT (eprop->class->pspec) ||
						   G_IS_PARAM_SPEC_DOUBLE (eprop->class->pspec)
						   ? 2 : 0);

	g_signal_connect (G_OBJECT (eprop_numeric->spin), "value_changed",
			  G_CALLBACK (glade_eprop_numeric_changed),
			  eprop);

	return eprop_numeric->spin;
}

/*******************************************************************************
                        GladeEditorPropertyEnumClass
 *******************************************************************************/
typedef struct {
	GladeEditorProperty  parent_instance;

	GtkWidget           *option_menu;
} GladeEPropEnum;

GLADE_MAKE_EPROP (GladeEPropEnum, glade_eprop_enum)
#define GLADE_TYPE_EPROP_ENUM            (glade_eprop_enum_get_type())
#define GLADE_EPROP_ENUM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_ENUM, GladeEPropEnum))
#define GLADE_EPROP_ENUM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_ENUM, GladeEPropEnumClass))
#define GLADE_IS_EPROP_ENUM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_ENUM))
#define GLADE_IS_EPROP_ENUM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_ENUM))
#define GLADE_EPROP_ENUM_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_ENUM, GladeEPropEnumClass))


static void
glade_eprop_enum_finalize (GObject *object)
{
	/* Chain up */
	G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_enum_load (GladeEditorProperty *eprop, GladeProperty *property)
{
	GladeEPropEnum     *eprop_enum = GLADE_EPROP_ENUM (eprop);
	GEnumClass         *eclass;
	guint               i;
	gint                value;

	/* Chain up first */
	editor_property_class->load (eprop, property);

	if (property)
	{
		eclass = g_type_class_ref (eprop->class->pspec->value_type);
		value  = g_value_get_enum (property->value);
		
		for (i = 0; i < eclass->n_values; i++)
			if (eclass->values[i].value == value)
				break;
		
		gtk_option_menu_set_history (GTK_OPTION_MENU (eprop_enum->option_menu),
					     i < eclass->n_values ? i : 0);
		g_type_class_unref (eclass);
	}
}

static void
glade_eprop_enum_changed (GtkWidget           *menu_item,
			  GladeEditorProperty *eprop)
{
	gint   ival;
	GValue val = { 0, };
	GladeProperty *property;

	if (eprop->loading) return;

	ival = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (menu_item), GLADE_ENUM_DATA_TAG));

	property = eprop->property;

	g_value_init (&val, eprop->class->pspec->value_type);
	g_value_set_enum (&val, ival);

	if (eprop->use_command == FALSE)
		glade_property_set_value (property, &val);
	else
		glade_command_set_property (property, &val);

	g_value_unset (&val);
}

static GtkWidget *
glade_editor_create_input_enum_item (GladeEditorProperty *eprop,
				     const gchar         *name,
				     gint                 value)
{
	GtkWidget *menu_item;

	menu_item = gtk_menu_item_new_with_label (name);

	g_signal_connect (G_OBJECT (menu_item), "activate",
			  G_CALLBACK (glade_eprop_enum_changed),
			  eprop);

	g_object_set_data (G_OBJECT (menu_item), GLADE_ENUM_DATA_TAG, GINT_TO_POINTER(value));

	return menu_item;
}

static GtkWidget *
glade_editor_create_input_stock_item (GladeEditorProperty *eprop,
				      const gchar         *id,
				      gint                 value)
{
	GtkWidget *menu_item = gtk_image_menu_item_new_from_stock (id, NULL);

	g_signal_connect (G_OBJECT (menu_item), "activate",
			  G_CALLBACK (glade_eprop_enum_changed),
			  eprop);

	g_object_set_data (G_OBJECT (menu_item), GLADE_ENUM_DATA_TAG, GINT_TO_POINTER(value));

	return menu_item;
}

static GtkWidget *
glade_eprop_enum_create_input (GladeEditorProperty *eprop)
{
	GladeEPropEnum      *eprop_enum = GLADE_EPROP_ENUM (eprop);
	GtkWidget           *menu_item, *menu;
	GladePropertyClass  *class;
	GEnumClass          *eclass;
	gboolean             stock;
	guint                i;
	
	class  = eprop->class;
	eclass = g_type_class_ref (class->pspec->value_type);
	stock  = (class->pspec->value_type == GLADE_TYPE_STOCK);

	menu = gtk_menu_new ();

	for (i = 0; i < eclass->n_values; i++)
	{
		gchar *value_name = 
			glade_property_class_get_displayable_value
			(class, eclass->values[i].value);
		if (value_name == NULL) value_name = eclass->values[i].value_name;
		
		if (stock && strcmp (eclass->values[i].value_nick, "glade-none"))
			menu_item = glade_editor_create_input_stock_item
				(eprop, 
				 eclass->values[i].value_nick, 
				 eclass->values[i].value);
		else
			menu_item = glade_editor_create_input_enum_item
				(eprop, value_name, eclass->values[i].value);

		gtk_widget_show (menu_item);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
	}

	eprop_enum->option_menu = gtk_option_menu_new ();
	gtk_option_menu_set_menu (GTK_OPTION_MENU (eprop_enum->option_menu), menu);

	g_type_class_unref (eclass);

	return eprop_enum->option_menu;
}

/*******************************************************************************
                        GladeEditorPropertyFlagsClass
 *******************************************************************************/
typedef struct {
	GladeEditorProperty  parent_instance;

	GtkWidget           *entry;
} GladeEPropFlags;

GLADE_MAKE_EPROP (GladeEPropFlags, glade_eprop_flags)
#define GLADE_TYPE_EPROP_FLAGS            (glade_eprop_flags_get_type())
#define GLADE_EPROP_FLAGS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_FLAGS, GladeEPropFlags))
#define GLADE_EPROP_FLAGS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_FLAGS, GladeEPropFlagsClass))
#define GLADE_IS_EPROP_FLAGS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_FLAGS))
#define GLADE_IS_EPROP_FLAGS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_FLAGS))
#define GLADE_EPROP_FLAGS_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_FLAGS, GladeEPropFlagsClass))

static void
glade_eprop_flags_finalize (GObject *object)
{
	/* Chain up */
	G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_flags_load (GladeEditorProperty *eprop, GladeProperty *property)
{
	GladeEPropFlags *eprop_flags = GLADE_EPROP_FLAGS (eprop);
	gchar           *text;

	/* Chain up first */
	editor_property_class->load (eprop, property);
	
	if (property)
	{
		text = glade_property_class_make_string_from_flags
			(eprop->class, g_value_get_flags(property->value),
			 TRUE);
		gtk_entry_set_text (GTK_ENTRY (eprop_flags->entry), text);
		g_free (text);
	}
}

static void
flag_toggled (GtkCellRendererToggle *cell,
	      gchar                 *path_string,
	      GtkTreeModel          *model)
{
	GtkTreeIter iter;
	gboolean setting;

	gtk_tree_model_get_iter_from_string (model, &iter, path_string);

	gtk_tree_model_get (model, &iter,
			    FLAGS_COLUMN_SETTING, &setting,
			    -1);

	setting = setting ? FALSE : TRUE;

	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    FLAGS_COLUMN_SETTING, setting,
			    -1);
}

static void
glade_eprop_flags_show_dialog (GtkWidget           *button,
			       GladeEditorProperty *eprop)
{
	GtkWidget *editor;
	GtkWidget *dialog;
	GtkWidget *scrolled_window;
	GtkListStore *model;
	GtkWidget *tree_view;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GFlagsClass *class;
	gint response_id;
	guint flag_num, value;

	editor = gtk_widget_get_toplevel (button);
	dialog = gtk_dialog_new_with_buttons (_("Set Flags"),
					      GTK_WINDOW (editor),
					      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK,
					      GTK_RESPONSE_OK,
					      NULL);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 300, 400);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 
					GLADE_GENERIC_BORDER_WIDTH);

	gtk_widget_show (scrolled_window);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    scrolled_window, TRUE, TRUE, 0);

	/* Create the treeview using a simple list model with 2 columns. */
	model = gtk_list_store_new (2, G_TYPE_BOOLEAN, G_TYPE_STRING);

	tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);
	gtk_widget_show (tree_view);
	gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);

	column = gtk_tree_view_column_new ();

	renderer = gtk_cell_renderer_toggle_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "active", FLAGS_COLUMN_SETTING,
					     NULL);

	g_signal_connect (renderer, "toggled",
			  G_CALLBACK (flag_toggled), model);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "text", FLAGS_COLUMN_SYMBOL,
					     NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);


	/* Populate the model with the flags. */
	class = g_type_class_ref (G_VALUE_TYPE (eprop->property->value));
	value = g_value_get_flags (eprop->property->value);

	/* Step through each of the flags in the class. */
	for (flag_num = 0; flag_num < class->n_values; flag_num++) {
		GtkTreeIter iter;
		guint mask;
		gboolean setting;
		gchar *value_name;
		
		mask = class->values[flag_num].value;
		setting = ((value & mask) == mask) ? TRUE : FALSE;
		
		value_name = glade_property_class_get_displayable_value
			(eprop->class, class->values[flag_num].value);

		if (value_name == NULL) value_name = class->values[flag_num].value_name;
		
		/* Add a row to represent the flag. */
		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter,
				    FLAGS_COLUMN_SETTING,
				    setting,
				    FLAGS_COLUMN_SYMBOL,
				    value_name,
				    -1);
	}

	/* Run the dialog. */
	response_id = gtk_dialog_run (GTK_DIALOG (dialog));

	/* If the user selects OK, update the flags property. */
	if (response_id == GTK_RESPONSE_OK) {
		GtkTreeIter iter;
		guint new_value = 0;

		gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter);

		/* Step through each of the flags in the class, checking if
		   the corresponding toggle in the dialog is selected, If it
		   is, OR the flags' mask with the new value. */
		for (flag_num = 0; flag_num < class->n_values; flag_num++) {
			gboolean setting;

			gtk_tree_model_get (GTK_TREE_MODEL (model), &iter,
					    FLAGS_COLUMN_SETTING, &setting,
					    -1);

			if (setting)
				new_value |= class->values[flag_num].value;

			gtk_tree_model_iter_next (GTK_TREE_MODEL (model),
						  &iter);
		}

		/* If the new_value is different from the old value, we need
		   to update the property. */
		if (new_value != value) {
			GValue val = { 0, };

			g_value_init (&val, G_VALUE_TYPE (eprop->property->value));
			g_value_set_flags (&val, new_value);

			if (eprop->use_command == FALSE)
				glade_property_set_value (eprop->property, &val);
			else
				glade_command_set_property (eprop->property, &val);

			g_value_unset (&val);
		}

	}

	g_type_class_unref (class);

	gtk_widget_destroy (dialog);
}


static GtkWidget *
glade_eprop_flags_create_input (GladeEditorProperty *eprop)
{
	GladeEPropFlags *eprop_flags = GLADE_EPROP_FLAGS (eprop);
	GtkWidget       *hbox;
	GtkWidget       *button;

	hbox               = gtk_hbox_new (FALSE, 0);
	eprop_flags->entry = gtk_entry_new ();
	gtk_entry_set_editable (GTK_ENTRY (eprop_flags->entry), FALSE);
	gtk_widget_show (eprop_flags->entry);
	gtk_box_pack_start (GTK_BOX (hbox), eprop_flags->entry, TRUE, TRUE, 0);

	button = gtk_button_new_with_label ("...");
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), button,  FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_eprop_flags_show_dialog),
			  eprop);
	return hbox;
}

/*******************************************************************************
                        GladeEditorPropertyColorClass
 *******************************************************************************/
typedef struct {
	GladeEditorProperty  parent_instance;

	GtkWidget           *cbutton;
	GtkWidget           *entry;
} GladeEPropColor;

GLADE_MAKE_EPROP (GladeEPropColor, glade_eprop_color)
#define GLADE_TYPE_EPROP_COLOR            (glade_eprop_color_get_type())
#define GLADE_EPROP_COLOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_COLOR, GladeEPropColor))
#define GLADE_EPROP_COLOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_COLOR, GladeEPropColorClass))
#define GLADE_IS_EPROP_COLOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_COLOR))
#define GLADE_IS_EPROP_COLOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_COLOR))
#define GLADE_EPROP_COLOR_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_COLOR, GladeEPropColorClass))


static void
glade_eprop_color_finalize (GObject *object)
{
	/* Chain up */
	G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_color_load (GladeEditorProperty *eprop, GladeProperty *property)
{
	GladeEPropColor *eprop_color = GLADE_EPROP_COLOR (eprop);
	GdkColor        *color;
	gchar           *text;

	/* Chain up first */
	editor_property_class->load (eprop, property);
	
	if (property)
	{
		text  = glade_property_class_make_string_from_gvalue
			(eprop->class, property->value);
		gtk_entry_set_text (GTK_ENTRY (eprop_color->entry), text);
		g_free (text);
		
		color = g_value_get_boxed (property->value);
		gtk_color_button_set_color (GTK_COLOR_BUTTON (eprop_color->cbutton), color);
	}
}

static void
glade_eprop_color_changed (GtkWidget *button,
			   GladeEditorProperty *eprop)
{
	GdkColor color = { 0, };
	GValue   value = { 0, };

	if (eprop->loading) return;

	gtk_color_button_get_color (GTK_COLOR_BUTTON (button), &color);

	g_value_init (&value, GDK_TYPE_COLOR);
	g_value_set_boxed (&value, &color);

	if (eprop->use_command == FALSE)
		glade_property_set_value (eprop->property, &value);
	else
		glade_command_set_property (eprop->property, &value);

	g_value_unset (&value);
}

static GtkWidget *
glade_eprop_color_create_input (GladeEditorProperty *eprop)
{
	GladeEPropColor *eprop_color = GLADE_EPROP_COLOR (eprop);
	GtkWidget       *hbox;

	hbox = gtk_hbox_new (FALSE, 0);

	eprop_color->entry = gtk_entry_new ();
	gtk_entry_set_editable (GTK_ENTRY (eprop_color->entry), FALSE);
	gtk_widget_show (eprop_color->entry);
	gtk_box_pack_start (GTK_BOX (hbox), eprop_color->entry, TRUE, TRUE, 0);

	eprop_color->cbutton = gtk_color_button_new ();
	gtk_widget_show (eprop_color->cbutton);
	gtk_box_pack_start (GTK_BOX (hbox), eprop_color->cbutton,  FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (eprop_color->cbutton), "color-set",
			  G_CALLBACK (glade_eprop_color_changed), 
			  eprop);

	return hbox;
}

/*******************************************************************************
                        GladeEditorPropertyTextClass
 *******************************************************************************/
typedef struct {
	GladeEditorProperty  parent_instance;

	GtkWidget           *text_entry;
} GladeEPropText;

GLADE_MAKE_EPROP (GladeEPropText, glade_eprop_text)
#define GLADE_TYPE_EPROP_TEXT            (glade_eprop_text_get_type())
#define GLADE_EPROP_TEXT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_TEXT, GladeEPropText))
#define GLADE_EPROP_TEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_TEXT, GladeEPropTextClass))
#define GLADE_IS_EPROP_TEXT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_TEXT))
#define GLADE_IS_EPROP_TEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_TEXT))
#define GLADE_EPROP_TEXT_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_TEXT, GladeEPropTextClass))

static void
glade_eprop_text_finalize (GObject *object)
{
	/* Chain up */
	G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_text_load (GladeEditorProperty *eprop, GladeProperty *property)
{
	GladeEPropText *eprop_text = GLADE_EPROP_TEXT (eprop);

	/* Chain up first */
	editor_property_class->load (eprop, property);
	
	if (property == NULL) return;

	if (GTK_IS_EDITABLE (eprop_text->text_entry)) {
		GtkEditable *editable = GTK_EDITABLE (eprop_text->text_entry);
		gint pos, insert_pos = 0;
		const gchar *text;
		text = g_value_get_string (property->value);
		pos  = gtk_editable_get_position (editable);

 		gtk_editable_delete_text (editable, 0, -1);
		gtk_editable_insert_text (editable,
					  text ? text : "",
					  text ? g_utf8_strlen (text, -1) : 0,
					  &insert_pos);

		gtk_editable_set_position (editable, pos);
	} else if (GTK_IS_TEXT_VIEW (eprop_text->text_entry)) {
		GtkTextBuffer  *buffer;
		gchar **split;
			
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (eprop_text->text_entry));

		if (G_VALUE_HOLDS (property->value, G_TYPE_STRV))
		{
			gchar *text = NULL;
			split = g_value_get_boxed (property->value);

			if (split) text = g_strjoinv ("\n", split);

			gtk_text_buffer_set_text (buffer,
						  text ? text : "",
						  text ? g_utf8_strlen (text, -1) : 0);
			g_free (text);
		} else {
			const gchar *text = g_value_get_string (property->value);
			gtk_text_buffer_set_text (buffer,
						  text ? text : "",
						  text ? g_utf8_strlen (text, -1) : 0);
		}
	} else {
		g_warning ("BUG! Invalid Text Widget type.");
	}
}

static void
glade_eprop_text_changed_common (GladeProperty *property,
				 const gchar *text,
				 gboolean use_command)
{
	GValue val = { 0, };

	if (property->class->pspec->value_type == G_TYPE_STRV)
	{
		g_value_init (&val, G_TYPE_STRV);
		g_value_take_boxed (&val, g_strsplit (text, "\n", 0));
	} 
	else
	{
		g_value_init (&val, G_TYPE_STRING);
		g_value_set_string (&val, text);
	}

	if (use_command == FALSE)
		glade_property_set_value (property, &val);
	else 
		glade_command_set_property (property, &val);

	g_value_unset (&val);
}

static void
glade_eprop_text_changed (GtkWidget           *entry,
			  GladeEditorProperty *eprop)
{
	gchar *text;

	if (eprop->loading) return;
	
	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	glade_eprop_text_changed_common (eprop->property, text,
					 eprop->use_command);
	g_free (text);
}

static gboolean
glade_eprop_text_entry_focus_out (GtkWidget           *entry,
				  GdkEventFocus       *event,
				  GladeEditorProperty *eprop)
{
	glade_eprop_text_changed (entry, eprop);
	return FALSE;
}

static gboolean
glade_eprop_text_text_view_focus_out (GtkTextView         *view,
				      GdkEventFocus       *event,
				      GladeEditorProperty *eprop)
{
	gchar *text;
	GtkTextBuffer *buffer;
	GtkTextIter    start, end;
	
	if (eprop->loading) return FALSE;

	buffer = gtk_text_view_get_buffer (view);

	gtk_text_buffer_get_iter_at_offset (buffer, &start, 0);
	gtk_text_buffer_get_iter_at_offset (buffer, &end,
					    gtk_text_buffer_get_char_count (buffer));

	text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	glade_eprop_text_changed_common (eprop->property, text,
					 eprop->use_command);

	g_free (text);
	return FALSE;
}

static void
glade_eprop_text_show_i18n_dialog (GtkWidget           *entry,
				   GladeEditorProperty *eprop)
{
	GtkWidget     *editor;
	GtkWidget     *dialog;
	GtkWidget     *vbox, *hbox;
	GtkWidget     *label;
	GtkWidget     *sw;
	GtkWidget     *text_view, *comment_view;
	GtkTextBuffer *text_buffer, *comment_buffer;
	GtkWidget     *translatable_button, *context_button;
	const gchar   *text;
	gint           res;
	gchar         *str;

	editor = gtk_widget_get_toplevel (entry);
	dialog = gtk_dialog_new_with_buttons (_("Edit Text Property"),
					      GTK_WINDOW (editor),
					      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), GLADE_GENERIC_BORDER_WIDTH);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 0);

	/* Text */
	label = gtk_label_new_with_mnemonic (_("_Text:"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (sw);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	gtk_widget_set_size_request (sw, 400, 200);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);

	text_view = gtk_text_view_new ();
	gtk_widget_show (text_view);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), text_view);
		
	gtk_container_add (GTK_CONTAINER (sw), text_view);

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));

	text = g_value_get_string (eprop->property->value);
	if (text)
	{
		gtk_text_buffer_set_text (text_buffer,
					  text,
					  -1);
	}
	
	/* Translatable and context prefix. */
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_widget_show (hbox);

	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	translatable_button = gtk_check_button_new_with_mnemonic (_("T_ranslatable"));
	gtk_widget_show (translatable_button);
	gtk_box_pack_start (GTK_BOX (hbox), translatable_button, FALSE, FALSE, 0);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (translatable_button),
				      glade_property_i18n_get_translatable (eprop->property));

	context_button = gtk_check_button_new_with_mnemonic (_("Has context _prefix"));
	gtk_widget_show (context_button);
	gtk_box_pack_start (GTK_BOX (hbox), context_button, FALSE, FALSE, 0);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (context_button),
				      glade_property_i18n_get_has_context (eprop->property));
	
	/* Comments. */
	label = gtk_label_new_with_mnemonic (_("Co_mments for translators:"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (sw);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);

	comment_view = gtk_text_view_new ();
	gtk_widget_show (comment_view);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), comment_view);

	gtk_container_add (GTK_CONTAINER (sw), comment_view);

	comment_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (comment_view));

	text = glade_property_i18n_get_comment (eprop->property);
	if (text)
	{
		gtk_text_buffer_set_text (comment_buffer,
					  text,
					  -1);
	}
	
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_OK) {
		GtkTextIter start, end;
		
		/* Get the new values. */
		glade_property_i18n_set_translatable (
			eprop->property,
			gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (translatable_button)));
		glade_property_i18n_set_has_context (
			eprop->property,
			gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (context_button)));

		/* Text */
		gtk_text_buffer_get_bounds (text_buffer, &start, &end);
		str = gtk_text_buffer_get_text (text_buffer, &start, &end, TRUE);
		if (str[0] == '\0')
		{
			g_free (str);
			str = NULL;
		}
		
		glade_eprop_text_changed_common (eprop->property, str,
						 eprop->use_command);
		
		g_free (str);
		
		/* Comment */
		gtk_text_buffer_get_bounds (comment_buffer, &start, &end);
		str = gtk_text_buffer_get_text (comment_buffer, &start, &end, TRUE);
		if (str[0] == '\0')
		{
			g_free (str);
			str = NULL;
		}

		glade_property_i18n_set_comment (eprop->property, str);
		g_free (str);
	}

	gtk_widget_destroy (dialog);
}

static GtkWidget *
glade_eprop_text_create_input (GladeEditorProperty *eprop)
{
	GladeEPropText      *eprop_text = GLADE_EPROP_TEXT (eprop);
	GladePropertyClass  *class;
	GtkWidget           *hbox;

	class = eprop->class;

	hbox = gtk_hbox_new (FALSE, 0);

	if (class->visible_lines > 1 ||
	    class->pspec->value_type == G_TYPE_STRV) {
		GtkWidget  *swindow;

		swindow = gtk_scrolled_window_new (NULL, NULL);

		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
						GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow), GTK_SHADOW_IN);

		eprop_text->text_entry = gtk_text_view_new ();
		gtk_widget_show (eprop_text->text_entry);
		
		gtk_container_add (GTK_CONTAINER (swindow), eprop_text->text_entry);

		gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (swindow), TRUE, TRUE, 0); 

		g_signal_connect (G_OBJECT (eprop_text->text_entry), "focus-out-event",
				  G_CALLBACK (glade_eprop_text_text_view_focus_out),
				  eprop);
	} else {
		eprop_text->text_entry = gtk_entry_new ();
		gtk_box_pack_start (GTK_BOX (hbox), eprop_text->text_entry, TRUE, TRUE, 0); 

		g_signal_connect (G_OBJECT (eprop_text->text_entry), "activate",
				  G_CALLBACK (glade_eprop_text_changed),
				  eprop);
		
		g_signal_connect (G_OBJECT (eprop_text->text_entry), "focus-out-event",
				  G_CALLBACK (glade_eprop_text_entry_focus_out),
				  eprop);
	}
	
	if (class->translatable) {
		GtkWidget *button = gtk_button_new_with_label ("...");
		gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0); 
		g_signal_connect (button, "clicked",
				  G_CALLBACK (glade_eprop_text_show_i18n_dialog),
				  eprop);
	}
	return hbox;
}

/*******************************************************************************
                        GladeEditorPropertyBoolClass
 *******************************************************************************/
typedef struct {
	GladeEditorProperty  parent_instance;

	GtkWidget           *toggle;
} GladeEPropBool;

GLADE_MAKE_EPROP (GladeEPropBool, glade_eprop_bool)
#define GLADE_TYPE_EPROP_BOOL            (glade_eprop_bool_get_type())
#define GLADE_EPROP_BOOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_BOOL, GladeEPropBool))
#define GLADE_EPROP_BOOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_BOOL, GladeEPropBoolClass))
#define GLADE_IS_EPROP_BOOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_BOOL))
#define GLADE_IS_EPROP_BOOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_BOOL))
#define GLADE_EPROP_BOOL_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_BOOL, GladeEPropBoolClass))

static void
glade_eprop_bool_finalize (GObject *object)
{
	/* Chain up */
	G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_bool_load (GladeEditorProperty *eprop, GladeProperty *property)
{
	GladeEPropBool *eprop_bool = GLADE_EPROP_BOOL (eprop);
	GtkWidget      *label;
	gboolean        state;

	/* Chain up first */
	editor_property_class->load (eprop, property);

	if (property)
	{
		state = g_value_get_boolean (property->value);
		
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (eprop_bool->toggle), state);
		
		label = GTK_BIN (eprop_bool->toggle)->child;
		gtk_label_set_text (GTK_LABEL (label), state ? _("Yes") : _("No"));
	}
}

static void
glade_eprop_bool_changed (GtkWidget           *button,
			  GladeEditorProperty *eprop)
{
	GtkWidget *label;
	gboolean state;
	GValue val = { 0, };

	if (eprop->loading) return;

	state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	label = GTK_BIN (button)->child;
	gtk_label_set_text (GTK_LABEL (label), state ? _("Yes") : _("No"));

	g_value_init (&val, G_TYPE_BOOLEAN);
	g_value_set_boolean (&val, state);

	if (eprop->use_command == FALSE)
		glade_property_set_value (eprop->property, &val);
	else
		glade_command_set_property (eprop->property, &val);

	g_value_unset (&val);
}

static GtkWidget *
glade_eprop_bool_create_input (GladeEditorProperty *eprop)
{
	GladeEPropBool *eprop_bool = GLADE_EPROP_BOOL (eprop);

	eprop_bool->toggle = gtk_toggle_button_new_with_label (_("No"));

	g_signal_connect (G_OBJECT (eprop_bool->toggle), "toggled",
			  G_CALLBACK (glade_eprop_bool_changed),
			  eprop);

	return eprop_bool->toggle;
}


/*******************************************************************************
                        GladeEditorPropertyUnicharClass
 *******************************************************************************/
typedef struct {
	GladeEditorProperty  parent_instance;

	GtkWidget           *entry;
} GladeEPropUnichar;

GLADE_MAKE_EPROP (GladeEPropUnichar, glade_eprop_unichar)
#define GLADE_TYPE_EPROP_UNICHAR            (glade_eprop_unichar_get_type())
#define GLADE_EPROP_UNICHAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_UNICHAR, GladeEPropUnichar))
#define GLADE_EPROP_UNICHAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_UNICHAR, GladeEPropUnicharClass))
#define GLADE_IS_EPROP_UNICHAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_UNICHAR))
#define GLADE_IS_EPROP_UNICHAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_UNICHAR))
#define GLADE_EPROP_UNICHAR_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_UNICHAR, GladeEPropUnicharClass))

static void
glade_eprop_unichar_finalize (GObject *object)
{
	/* Chain up */
	G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_unichar_load (GladeEditorProperty *eprop, GladeProperty *property)
{
	GladeEPropUnichar *eprop_unichar = GLADE_EPROP_UNICHAR (eprop);

	/* Chain up first */
	editor_property_class->load (eprop, property);

	if (property && GTK_IS_EDITABLE (eprop_unichar->entry)) {
		GtkEditable *editable = GTK_EDITABLE (eprop_unichar->entry);
		gint insert_pos = 0;
		gunichar ch;
		gchar utf8st[6];

		ch = g_value_get_uint (property->value);

		g_unichar_to_utf8 (ch, utf8st);
 		gtk_editable_delete_text (editable, 0, -1);
		gtk_editable_insert_text (editable, utf8st, 1, &insert_pos);
	}
}


static void
glade_eprop_unichar_changed (GtkWidget           *entry,
			     GladeEditorProperty *eprop)
{
	GValue val = { 0, };
	guint len;
	gchar *text;
	gunichar unich = '\0';

	if (eprop->loading) return;

	if ((text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1)) != NULL)
	{
		len = g_utf8_strlen (text, -1);
		unich = g_utf8_get_char (text);
		g_free (text);
	}

	g_value_init (&val, G_TYPE_UINT);
	g_value_set_uint (&val, unich);

	if (eprop->use_command == FALSE)
		glade_property_set_value (eprop->property, &val);
	else
		glade_command_set_property (eprop->property, &val);

	g_value_unset (&val);
}

static void
glade_eprop_unichar_delete (GtkEditable *editable,
			    gint start_pos,
			    gint end_pos)
{
	gtk_editable_select_region (editable, 0, -1);
	g_signal_stop_emission_by_name (G_OBJECT (editable), "delete_text");
}

static void
glade_eprop_unichar_insert (GtkWidget *entry,
			    const gchar *text,
			    gint length,
			    gint *position,
			    GladeEditorProperty *eprop)
{
	g_signal_handlers_block_by_func
		(G_OBJECT (entry), G_CALLBACK (glade_eprop_unichar_changed), eprop);
	g_signal_handlers_block_by_func
		(G_OBJECT (entry), G_CALLBACK (glade_eprop_unichar_insert), eprop);
	g_signal_handlers_block_by_func
		(G_OBJECT (entry), G_CALLBACK (glade_eprop_unichar_delete), eprop);

	gtk_editable_delete_text (GTK_EDITABLE (entry), 0, -1);
	*position = 0;
	gtk_editable_insert_text (GTK_EDITABLE (entry), text, 1, position);

	g_signal_handlers_unblock_by_func
		(G_OBJECT (entry), G_CALLBACK (glade_eprop_unichar_changed), eprop);
	g_signal_handlers_unblock_by_func
		(G_OBJECT (entry), G_CALLBACK (glade_eprop_unichar_insert), eprop);
	g_signal_handlers_unblock_by_func
		(G_OBJECT (entry), G_CALLBACK (glade_eprop_unichar_delete), eprop);

	g_signal_stop_emission_by_name (G_OBJECT (entry), "insert_text");

	glade_eprop_unichar_changed (entry, eprop);
}

static GtkWidget *
glade_eprop_unichar_create_input (GladeEditorProperty *eprop)
{
	GladeEPropUnichar *eprop_unichar = GLADE_EPROP_UNICHAR (eprop);

	eprop_unichar->entry = gtk_entry_new ();

	/* it's 2 to prevent spirious beeps... */
	gtk_entry_set_max_length (GTK_ENTRY (eprop_unichar->entry), 2);
	
	g_signal_connect (G_OBJECT (eprop_unichar->entry), "changed",
			  G_CALLBACK (glade_eprop_unichar_changed), eprop);
	g_signal_connect (G_OBJECT (eprop_unichar->entry), "insert_text",
			  G_CALLBACK (glade_eprop_unichar_insert), eprop);
	g_signal_connect (G_OBJECT (eprop_unichar->entry), "delete_text",
			  G_CALLBACK (glade_eprop_unichar_delete), eprop);
	return eprop_unichar->entry;
}

/*******************************************************************************
                        GladeEditorPropertyPixbufClass
 *******************************************************************************/
typedef struct {
	GladeEditorProperty parent_instance;
	
	GtkWidget *entry, *button;
} GladeEPropPixbuf;

GLADE_MAKE_EPROP (GladeEPropPixbuf, glade_eprop_pixbuf)
#define GLADE_TYPE_EPROP_PIXBUF            (glade_eprop_pixbuf_get_type())
#define GLADE_EPROP_PIXBUF(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_PIXBUF, GladeEPropPixbuf))
#define GLADE_EPROP_PIXBUF_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_PIXBUF, GladeEPropPixbufClass))
#define GLADE_IS_EPROP_PIXBUF(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_PIXBUF))
#define GLADE_IS_EPROP_PIXBUF_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_PIXBUF))
#define GLADE_EPROP_PIXBUF_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_PIXBUF, GladeEPropPixbufClass))

static void
glade_eprop_pixbuf_finalize (GObject *object)
{
	/* Chain up */
	G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_pixbuf_entry_activate (GtkEntry *entry, GladeEditorProperty *eprop)
{
	GValue *value = glade_property_class_make_gvalue_from_string (eprop->class,
							gtk_entry_get_text(entry));
	
	if (eprop->use_command == FALSE)
		glade_property_set_value (eprop->property, value);
	else
		glade_command_set_property (eprop->property, value);

	g_value_unset (value);
	g_free (value);
}

static void
glade_eprop_pixbuf_select_file (GtkButton *button, GladeEditorProperty *eprop)
{
	GladeEPropPixbuf *eprop_pixbuf = GLADE_EPROP_PIXBUF (eprop);
	GtkWidget *dialog;
	GtkFileFilter *filter;
	gchar *file, *basename;
	
	if (eprop->loading) return;

	dialog = gtk_file_chooser_dialog_new ("Select a File",
					NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					NULL);
	
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pixbuf_formats (filter);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);
	
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
	{
		file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		
		/*
		  TODO: instead of "basename" we need a relative path to the 
		  project's directory.
		*/
		basename = g_path_get_basename (file);
	
		gtk_entry_set_text (GTK_ENTRY(eprop_pixbuf->entry), basename);
		gtk_widget_activate (eprop_pixbuf->entry);
		
		g_free (file);
		g_free (basename);
	}
	
	gtk_widget_destroy (dialog);
}

static void
glade_eprop_pixbuf_load (GladeEditorProperty *eprop, GladeProperty *property)
{
	GladeEPropPixbuf *eprop_pixbuf = GLADE_EPROP_PIXBUF (eprop);
	gchar *file;

	/* Chain up first */
	editor_property_class->load (eprop, property);

	if (property == NULL) return;

	file  = glade_property_class_make_string_from_gvalue
						(eprop->class, property->value);
	if (file)
	{
		gtk_entry_set_text (GTK_ENTRY (eprop_pixbuf->entry), file);
		g_free (file);
	}
	else
	{
		gtk_entry_set_text (GTK_ENTRY (eprop_pixbuf->entry), "");
	}
}

static GtkWidget *
glade_eprop_pixbuf_create_input (GladeEditorProperty *eprop)
{
	GladeEPropPixbuf *eprop_pixbuf = GLADE_EPROP_PIXBUF (eprop);
	GtkWidget *hbox;

	hbox = gtk_hbox_new (FALSE, 0);

	eprop_pixbuf->entry = gtk_entry_new ();
	gtk_widget_show (eprop_pixbuf->entry);
	
	eprop_pixbuf->button = gtk_button_new_with_label ("...");
	gtk_widget_show (eprop_pixbuf->button);
	
	gtk_box_pack_start (GTK_BOX (hbox), eprop_pixbuf->entry, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), eprop_pixbuf->button, FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (eprop_pixbuf->entry), "activate",
			  G_CALLBACK (glade_eprop_pixbuf_entry_activate), 
			  eprop);
	g_signal_connect (G_OBJECT (eprop_pixbuf->button), "clicked",
			  G_CALLBACK (glade_eprop_pixbuf_select_file), 
			  eprop);

	return hbox;
}


/*******************************************************************************
                        GladeEditorPropertyPixbufClass
 *******************************************************************************/
enum {
	OBJ_COLUMN_WIDGET = 0,
	OBJ_COLUMN_WIDGET_NAME,
	OBJ_COLUMN_WIDGET_CLASS,
	OBJ_COLUMN_SELECTED,
	OBJ_COLUMN_SELECTABLE,
	OBJ_NUM_COLUMNS
};

#define GLADE_RESPONSE_CLEAR  42

typedef struct {
	GladeEditorProperty parent_instance;
	
	GtkWidget *entry;
} GladeEPropObject;

GLADE_MAKE_EPROP (GladeEPropObject, glade_eprop_object)
#define GLADE_TYPE_EPROP_OBJECT            (glade_eprop_object_get_type())
#define GLADE_EPROP_OBJECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_OBJECT, GladeEPropObject))
#define GLADE_EPROP_OBJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_OBJECT, GladeEPropObjectClass))
#define GLADE_IS_EPROP_OBJECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_OBJECT))
#define GLADE_IS_EPROP_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_OBJECT))
#define GLADE_EPROP_OBJECT_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_OBJECT, GladeEPropObjectClass))

static void
glade_eprop_object_finalize (GObject *object)
{
	/* Chain up */
	G_OBJECT_CLASS (editor_property_class)->finalize (object);
}


static gchar *
glade_eprop_object_name (const gchar      *name, 
			 GtkTreeStore     *model,
			 GtkTreeIter      *parent_iter)
{
	GtkTreePath *path;
	GString     *string;
	gint         i;
	
	string = g_string_new (name);

	if (parent_iter)
	{
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), parent_iter);
		for (i = 0; i < gtk_tree_path_get_depth (path); i++)
			g_string_prepend (string, "    ");
	}

	return g_string_free (string, FALSE);
}


/**
 * Note that widgets is a list of GtkWidgets, while what we store
 * in the model are the associated GladeWidgets.
 */
static void
glade_eprop_object_populate_view_real (GladeEditorProperty *eprop,
				       GtkTreeStore        *model,
				       GList               *widgets,
				       GtkTreeIter         *parent_iter)
{
	GList *children, *list;
	GtkTreeIter       iter;
	gboolean          good_type, has_decendant;

	for (list = widgets; list; list = list->next)
	{
		GladeWidget *widget;

		if ((widget = glade_widget_get_from_gobject (list->data)) != NULL)
		{
			good_type = g_type_is_a (widget->widget_class->type, 
						 eprop->class->pspec->value_type);
			has_decendant = glade_widget_has_decendant 
				(widget, eprop->class->pspec->value_type);

			if (good_type || has_decendant)
			{
				gtk_tree_store_append (model, &iter, parent_iter);
				gtk_tree_store_set    
					(model, &iter, 
					 OBJ_COLUMN_WIDGET, widget, 
					 OBJ_COLUMN_WIDGET_NAME, 
					 glade_eprop_object_name (widget->name, model, parent_iter),
					 OBJ_COLUMN_WIDGET_CLASS, 
					 widget->widget_class->palette_name,
					 /* Selectable if its a compatible type and
					  * its not itself.
					  */
					 OBJ_COLUMN_SELECTABLE, 
					 good_type && (widget != eprop->property->widget),
					 OBJ_COLUMN_SELECTED,
					 good_type && glade_property_equals (eprop->property, 
									     widget->object), -1);
			}

			if (has_decendant &&
			    (children = glade_widget_class_container_get_all_children
			     (widget->widget_class, widget->object)) != NULL)
			{
				GtkTreeIter *copy = NULL;

				copy = gtk_tree_iter_copy (&iter);
				glade_eprop_object_populate_view_real (eprop, model, children, copy);
				gtk_tree_iter_free (copy);

				g_list_free (children);
			}
		}
	}
}

static void
glade_eprop_object_populate_view (GladeEditorProperty *eprop,
				  GtkTreeView         *view)
{
	GtkTreeStore  *model = (GtkTreeStore *)gtk_tree_view_get_model (view);
	GladeProject  *project = glade_default_app_get_active_project ();
	GList         *list, *toplevels = NULL;

	/* Make a list of only the toplevel widgets */
	for (list = project->objects; list; list = list->next)
	{
		GObject *object = G_OBJECT (list->data);
		GladeWidget *gwidget = glade_widget_get_from_gobject (object);
		g_assert (gwidget);

		if (gwidget->parent == NULL)
			toplevels = g_list_append (toplevels, object);
	}

	/* add the widgets and recurse */
	glade_eprop_object_populate_view_real (eprop, model, toplevels, NULL);
	g_list_free (toplevels);
}

gboolean
glade_eprop_object_clear_iter (GtkTreeModel *model,
			       GtkTreePath *path,
			       GtkTreeIter *iter,
			       gpointer data)
{
	gtk_tree_store_set (GTK_TREE_STORE (model), iter,
			    OBJ_COLUMN_SELECTED,  FALSE, -1);
	return FALSE;
}

gboolean
glade_eprop_object_selected_widget (GtkTreeModel  *model,
				    GtkTreePath   *path,
				    GtkTreeIter   *iter,
				    GladeWidget  **ret)
{
	gboolean     selected;
	GladeWidget *widget;

	gtk_tree_model_get (model, iter,
			    OBJ_COLUMN_SELECTED,  &selected, 
			    OBJ_COLUMN_WIDGET,    &widget, -1);
	
	if (selected) 
	{
		*ret = widget;
		return TRUE;
	}
	return FALSE;
}

static void
glade_eprop_object_selected (GtkCellRendererToggle *cell,
			     gchar                 *path_str,
			     GtkTreeModel          *model)
{
	GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
	GtkTreeIter  iter;
	gboolean     enabled;

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    OBJ_COLUMN_SELECTED,  &enabled, -1);

	if (enabled == FALSE)
	{
		/* Clear the rest of the view first
		 */
		gtk_tree_model_foreach (model, glade_eprop_object_clear_iter, NULL);
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
				    OBJ_COLUMN_SELECTED,  !enabled, -1);
	}
	gtk_tree_path_free (path);
}

static GtkWidget *
glade_eprop_object_view (GladeEditorProperty *eprop)
{
	GtkWidget         *view_widget;
	GtkTreeModel      *model;
 	GtkCellRenderer   *renderer;
	GtkTreeViewColumn *column;

	model = (GtkTreeModel *)gtk_tree_store_new
		(OBJ_NUM_COLUMNS,
		 G_TYPE_OBJECT,        /* The GladeWidget  */
		 G_TYPE_STRING,        /* The GladeWidget's name */
		 G_TYPE_STRING,        /* The GladeWidgetClass title */
		 G_TYPE_BOOLEAN,       /* Whether this row is selected or not */
		 G_TYPE_BOOLEAN);      /* Whether this GladeWidget is 
					* of an acceptable type and 
					* therefore can be selected.
					*/

	view_widget = gtk_tree_view_new_with_model (model);
	g_object_set (G_OBJECT (view_widget), "enable-search", FALSE, NULL);
	
	/********************* fake invisible column *********************/
 	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), "editable", FALSE, "visible", FALSE, NULL);

	column = gtk_tree_view_column_new_with_attributes (NULL, renderer, NULL);
 	gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

	gtk_tree_view_column_set_visible (column, FALSE);
	gtk_tree_view_set_expander_column (GTK_TREE_VIEW (view_widget), column);

	/************************ selected column ************************/
 	renderer = gtk_cell_renderer_toggle_new ();
	g_object_set (G_OBJECT (renderer), 
		      "mode",        GTK_CELL_RENDERER_MODE_ACTIVATABLE, 
		      "activatable", TRUE,
 		      "radio",       TRUE,
		      NULL);
	g_signal_connect (renderer, "toggled",
			  G_CALLBACK (glade_eprop_object_selected), model);
	gtk_tree_view_insert_column_with_attributes 
		(GTK_TREE_VIEW (view_widget), 0,
		 NULL, renderer, 
		 "visible", OBJ_COLUMN_SELECTABLE,
		 "sensitive", OBJ_COLUMN_SELECTABLE,
		 "active", OBJ_COLUMN_SELECTED,
		 NULL);

	/********************* widget name column *********************/
 	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);
	gtk_tree_view_insert_column_with_attributes
		(GTK_TREE_VIEW (view_widget), 1,
		 _("Name"),  renderer, 
		 "text", OBJ_COLUMN_WIDGET_NAME, 
		 NULL);

	/***************** widget class title column ******************/
 	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), 
		      "editable", FALSE, 
		      "style", PANGO_STYLE_ITALIC,
		      "foreground", "Gray", NULL);
	gtk_tree_view_insert_column_with_attributes
		(GTK_TREE_VIEW (view_widget), 2,
		 _("Class"),  renderer, 
		 "text", OBJ_COLUMN_WIDGET_CLASS, 
		 NULL);

	return view_widget;
}

static void
glade_eprop_object_show_dialog (GtkWidget           *dialog_button,
				GladeEditorProperty *eprop)
{
	GtkWidget     *dialog, *parent;
	GtkWidget     *vbox, *label, *sw;
	GtkWidget     *tree_view;
	gint           res;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (eprop));
	dialog = gtk_dialog_new_with_buttons (_("Choose an object in this project"),
					      GTK_WINDOW (parent),
					      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					      _("Clear"), GLADE_RESPONSE_CLEAR,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), GLADE_GENERIC_BORDER_WIDTH);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 0);

	/* Checklist */
	label = gtk_label_new (_("Objects:"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (sw);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	gtk_widget_set_size_request (sw, 400, 200);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);


	tree_view = glade_eprop_object_view (eprop);
	glade_eprop_object_populate_view (eprop, GTK_TREE_VIEW (tree_view));


	gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

	gtk_widget_show (tree_view);
	gtk_container_add (GTK_CONTAINER (sw), tree_view);
	
	/* Run the dialog */
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_OK) 
	{
		GladeWidget *selected = NULL;

		gtk_tree_model_foreach
			(gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view)), 
			 (GtkTreeModelForeachFunc) glade_eprop_object_selected_widget, &selected);

		if (selected)
		{
			GValue *value = glade_property_class_make_gvalue_from_string (eprop->class, selected->name);

			if (eprop->use_command == FALSE)
				glade_property_set_value (eprop->property, value);
			else
				glade_command_set_property (eprop->property, value);
		}
	} 
	else if (res == GLADE_RESPONSE_CLEAR)
	{
		GValue *value = glade_property_class_make_gvalue_from_string (eprop->class, NULL);
		
		if (eprop->use_command == FALSE)
			glade_property_set_value (eprop->property, value);
		else
			glade_command_set_property (eprop->property, value);

		g_value_unset (value);
		g_free (value);
	}

	gtk_widget_destroy (dialog);
}


static void
glade_eprop_object_load (GladeEditorProperty *eprop, GladeProperty *property)
{
	GladeEPropObject *eprop_object = GLADE_EPROP_OBJECT (eprop);
	gchar *obj_name;

	/* Chain up first */
	editor_property_class->load (eprop, property);

	if (property == NULL) return;

	if ((obj_name  = glade_property_class_make_string_from_gvalue
	     (eprop->class, property->value)) != NULL)
	{
		gtk_entry_set_text (GTK_ENTRY (eprop_object->entry), obj_name);
		g_free (obj_name);
	}
	else
		gtk_entry_set_text (GTK_ENTRY (eprop_object->entry), "");

}

static GtkWidget *
glade_eprop_object_create_input (GladeEditorProperty *eprop)
{
	GladeEPropObject *eprop_object = GLADE_EPROP_OBJECT (eprop);
	GtkWidget        *hbox;
	GtkWidget        *button;

	hbox                = gtk_hbox_new (FALSE, 0);
	eprop_object->entry = gtk_entry_new ();
	gtk_entry_set_editable (GTK_ENTRY (eprop_object->entry), FALSE);
	gtk_widget_show (eprop_object->entry);
	gtk_box_pack_start (GTK_BOX (hbox), eprop_object->entry, TRUE, TRUE, 0);

	button = gtk_button_new_with_label ("...");
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), button,  FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_eprop_object_show_dialog),
			  eprop);
	return hbox;
}


/*******************************************************************************
                              Misc static stuff
 *******************************************************************************/
static GType 
glade_editor_property_type (GParamSpec *pspec)
{
	GType type = 0;

	if (pspec->value_type == GLADE_TYPE_STOCK ||
	    G_IS_PARAM_SPEC_ENUM(pspec))
		type = GLADE_TYPE_EPROP_ENUM;
	else if (G_IS_PARAM_SPEC_FLAGS(pspec))
		type = GLADE_TYPE_EPROP_FLAGS;
	else if (G_IS_PARAM_SPEC_BOXED(pspec))
	{
		if (pspec->value_type == GDK_TYPE_COLOR)
			type = GLADE_TYPE_EPROP_COLOR;
		else if (pspec->value_type == G_TYPE_STRV)
			type = GLADE_TYPE_EPROP_TEXT;
	}
	else if (G_IS_PARAM_SPEC_STRING(pspec))
		type = GLADE_TYPE_EPROP_TEXT;
	else if (G_IS_PARAM_SPEC_BOOLEAN(pspec))
		type = GLADE_TYPE_EPROP_BOOL;
	else if (G_IS_PARAM_SPEC_FLOAT(pspec)  ||
		 G_IS_PARAM_SPEC_DOUBLE(pspec) ||
		 G_IS_PARAM_SPEC_INT(pspec)    ||
		 G_IS_PARAM_SPEC_UINT(pspec)   ||
		 G_IS_PARAM_SPEC_LONG(pspec)   ||
		 G_IS_PARAM_SPEC_ULONG(pspec)  ||
		 G_IS_PARAM_SPEC_INT64(pspec)  ||
		 G_IS_PARAM_SPEC_UINT64(pspec))
		type = GLADE_TYPE_EPROP_NUMERIC;
	else if (G_IS_PARAM_SPEC_UNICHAR(pspec))
		type = GLADE_TYPE_EPROP_UNICHAR;
	else if (G_IS_PARAM_SPEC_OBJECT(pspec))
	{
		if(pspec->value_type == GDK_TYPE_PIXBUF)
			type = GLADE_TYPE_EPROP_PIXBUF;
		else 
			type = GLADE_TYPE_EPROP_OBJECT;
	}
	return type;
}

/*******************************************************************************
                                     API
 *******************************************************************************/

/**
 * glade_editor_property_new:
 * @class: A #GladePropertyClass
 * @use_command: Whether the undo/redo stack applies here.
 *
 * This is a factory function to create the correct type of
 * editor property that can edit the said type of #GladePropertyClass
 *
 * Returns: A newly created GladeEditorProperty of the correct type
 */
GladeEditorProperty *
glade_editor_property_new (GladePropertyClass  *class,
			   gboolean             use_command)
{
	GladeEditorProperty *eprop;
	GType                type = 0;

	/* Find the right type of GladeEditorProperty for this
	 * GladePropertyClass.
	 */
	if ((type = glade_editor_property_type (class->pspec)) == 0)
		g_error ("%s : type %s not implemented (%s)\n", G_GNUC_FUNCTION,
			 class->name, g_type_name (class->pspec->value_type));

	/* Create and return the correct type of GladeEditorProperty */
	eprop = g_object_new (type,
			      "property-class", class, 
			      "use-command", use_command,
			      NULL);

	return eprop;
}

/**
 * glade_editor_property_new_from_widget:
 * @widget: A #GladeWidget
 * @property: The widget's property
 * @use_command: Whether the undo/redo stack applies here.
 *
 * This is a convenience function to create a GladeEditorProperty corresponding
 * to @property
 *
 * Returns: A newly created and connected GladeEditorProperty
 */
GladeEditorProperty *
glade_editor_property_new_from_widget (GladeWidget *widget,
				       const gchar *property,
				       gboolean use_command)
{
	GladeEditorProperty *eprop;
	GladeProperty *p;
	
	p = glade_widget_get_property (widget, property);
	eprop = glade_editor_property_new (p->class, use_command);
	glade_editor_property_load (eprop, p);
	
	return eprop;
}

/**
 * glade_editor_property_supported:
 * @pspec: A #GParamSpec
 *
 * Returns: whether this pspec is supported by GladeEditorProperties.
 */
gboolean
glade_editor_property_supported (GParamSpec *pspec)
{
	return glade_editor_property_type (pspec) != 0;
}


/**
 * glade_editor_property_load:
 * @eprop: A #GladeEditorProperty
 * @property: A #GladeProperty
 *
 * Loads @property values into @eprop and connects.
 */
void
glade_editor_property_load (GladeEditorProperty *eprop,
			    GladeProperty       *property)
{
	g_return_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop));
	g_return_if_fail (property == NULL || GLADE_IS_PROPERTY (property));

	eprop->loading = TRUE;
	GLADE_EDITOR_PROPERTY_GET_CLASS (eprop)->load (eprop, property);
	eprop->loading = FALSE;
}


/**
 * glade_editor_property_load_by_widget:
 * @eprop: A #GladeEditorProperty
 * @widget: A #GladeWidget
 *
 * Convenience function to load the appropriate #GladeProperty into
 * @eprop from @widget
 */
void
glade_editor_property_load_by_widget (GladeEditorProperty *eprop,
				      GladeWidget         *widget)
{
	GladeProperty *property = NULL;

	g_return_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop));
	g_return_if_fail (widget == NULL || GLADE_IS_WIDGET (widget));

	if (widget)
	{
		if ((property = 
		     glade_widget_get_property (widget, eprop->class->id)) == NULL)
		{
			g_critical ("Couldnt find property of class %s on widget of class %s\n",
				    eprop->class->id, widget->widget_class->name);
		}
	}
	glade_editor_property_load (eprop, property);
}
