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

/**
 * SECTION:glade-editor-property
 * @Short_Description: A generic widget to edit a #GladeProperty.
 *
 * The #GladeEditorProperty is a factory that will create the correct
 * control for the #GladePropertyClass it was created for and provides
 * a simple unified api to them.
 */

#include <stdio.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-editor-property.h"
#include "glade-property.h"
#include "glade-command.h"
#include "glade-project.h"
#include "glade-builtins.h"
#include "glade-marshallers.h"
#include "glade-named-icon-chooser-dialog.h"

enum {
	PROP_0,
	PROP_PROPERTY_CLASS,
	PROP_USE_COMMAND,
	PROP_SHOW_INFO
};

enum {
	GTK_DOC_SEARCH,
	LAST_SIGNAL
};

static GtkTableClass             *table_class;
static GladeEditorPropertyClass  *editor_property_class;

static guint                      glade_editor_property_signals[LAST_SIGNAL] = { 0 };

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

/* declare this forwardly for the finalize routine */
static void glade_editor_property_load_common (GladeEditorProperty *eprop, 
					       GladeProperty       *property);

/* For use in editor implementations
 */
static void
glade_editor_property_commit (GladeEditorProperty *eprop,
			      GValue              *value)
{
	g_return_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop));

	if (eprop->use_command == FALSE)
		glade_property_set_value (eprop->property, value);
	else
		glade_command_set_property_value (eprop->property, value);

	/* If the value was denied by a verify function, we'll have to
	 * reload the real value.
	 */
	if (g_param_values_cmp (eprop->property->klass->pspec,
				eprop->property->value, value) != 0)
		GLADE_EDITOR_PROPERTY_GET_CLASS (eprop)->load (eprop, eprop->property);
}



static void
glade_editor_property_tooltip_cb (GladeProperty       *property,
				  const gchar         *tooltip,
				  GladeEditorProperty *eprop)
{
	gtk_widget_set_tooltip_text (eprop->input, tooltip);
	gtk_widget_set_tooltip_text (eprop->item_label, tooltip);
}

static void
glade_editor_property_sensitivity_cb (GladeProperty       *property,
				      GParamSpec          *pspec,
				      GladeEditorProperty *eprop)
{
	gboolean sensitive = glade_property_get_sensitive (eprop->property);

        gtk_widget_set_sensitive (eprop->item_label, sensitive);
        gtk_widget_set_sensitive (eprop->input, sensitive &&
                                                glade_property_get_enabled (property));
	if (eprop->check)
		gtk_widget_set_sensitive (eprop->check, sensitive);
}

static void
glade_editor_property_value_changed_cb (GladeProperty       *property,
					GValue              *old_value,
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

	if (eprop->klass->optional)
	{
		enabled = glade_property_get_enabled (property);

		/* sensitive = enabled &&   */
		if (enabled == FALSE)
			gtk_widget_set_sensitive (eprop->input, FALSE);
		else if (glade_property_get_sensitive (property))
			gtk_widget_set_sensitive (eprop->input, TRUE);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (eprop->check), enabled);
	}
}

static void
glade_editor_property_enabled_toggled_cb (GtkWidget           *check,
					  GladeEditorProperty *eprop)
{
	glade_property_set_enabled (eprop->property, 
				    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)));
}

static void
glade_editor_property_info_clicked_cb (GtkWidget           *info,
				       GladeEditorProperty *eprop)
{
	GladeWidgetAdaptor *adaptor;
	gchar              *search, *book;

	adaptor = glade_widget_adaptor_from_pspec (eprop->klass->pspec);
	search  = g_strdup_printf ("The %s property", eprop->klass->id);

	g_object_get (adaptor, "book", &book, NULL);

	g_signal_emit (G_OBJECT (eprop),
		       glade_editor_property_signals[GTK_DOC_SEARCH],
		       0, book,
		       g_type_name (eprop->klass->pspec->owner_type), search);

	g_free (book);
	g_free (search);
}

static GtkWidget *
glade_editor_property_create_info_button (GladeEditorProperty *eprop)
{
	GtkWidget *image;
	GtkWidget *button;

	button = gtk_button_new ();

	image = glade_util_get_devhelp_icon (GTK_ICON_SIZE_MENU);
	gtk_widget_show (image);

	gtk_container_add (GTK_CONTAINER (button), image);

	gtk_widget_set_tooltip_text (button, _("View GTK+ documentation for this property"));

	return button;
}

static GObject *
glade_editor_property_constructor (GType                  type,
				   guint                  n_construct_properties,
				   GObjectConstructParam *construct_properties)
{
	GObject             *obj;
	GladeEditorProperty *eprop;
	gchar               *text;

	/* Invoke parent constructor (eprop->klass should be resolved by this point) . */
	obj = G_OBJECT_CLASS (table_class)->constructor
		(type, n_construct_properties, construct_properties);
	
	eprop = GLADE_EDITOR_PROPERTY (obj);

	/* Create label */
	text = g_strdup_printf ("%s:", eprop->klass->name);
	eprop->item_label = gtk_label_new (text);
	g_free (text);

	gtk_misc_set_alignment (GTK_MISC (eprop->item_label), 1.0, 0.5);

	/* Create hbox and possibly check button
	 */
	if (eprop->klass->optional)
	{
		eprop->check = gtk_check_button_new ();
		gtk_widget_show (eprop->check);
		gtk_box_pack_start (GTK_BOX (eprop), eprop->check, FALSE, FALSE, 0);
		g_signal_connect (G_OBJECT (eprop->check), "toggled", 
				  G_CALLBACK (glade_editor_property_enabled_toggled_cb), eprop);
	}

	/* Create the class specific input widget and add it */
	eprop->input = GLADE_EDITOR_PROPERTY_GET_CLASS (eprop)->create_input (eprop);
	gtk_widget_show (eprop->input);
	gtk_box_pack_start (GTK_BOX (eprop), eprop->input, TRUE, TRUE, 0);

	/* Create the informational button and add it */
	eprop->info = glade_editor_property_create_info_button (eprop);
	g_signal_connect (G_OBJECT (eprop->info), "clicked", 
			  G_CALLBACK (glade_editor_property_info_clicked_cb), eprop);


	gtk_box_pack_start (GTK_BOX (eprop), eprop->info, FALSE, FALSE, 2);

	return obj;
}

static void
glade_editor_property_finalize (GObject *object)
{
	GladeEditorProperty *eprop = GLADE_EDITOR_PROPERTY (object);

	/* detatch from loaded property */
	glade_editor_property_load_common (eprop, NULL);

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
		eprop->klass = g_value_get_pointer (value);
		break;
	case PROP_USE_COMMAND:
		eprop->use_command = g_value_get_boolean (value);
		break;
	case PROP_SHOW_INFO:
		if (g_value_get_boolean (value))
			glade_editor_property_show_info (eprop);
		else
			glade_editor_property_hide_info (eprop);
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
		g_value_set_pointer (value, eprop->klass);
		break;
	case PROP_USE_COMMAND:
		g_value_set_boolean (value, eprop->use_command);
		break;
	case PROP_SHOW_INFO:
		g_value_set_boolean (value, eprop->show_info);
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_eprop_property_finalized (GladeEditorProperty *eprop,
				GladeProperty       *where_property_was)
{
	eprop->tooltip_id   = 0;
	eprop->sensitive_id = 0;
	eprop->changed_id   = 0;
	eprop->enabled_id   = 0;
	eprop->property     = NULL;

	glade_editor_property_load (eprop, NULL);
}

static void
glade_editor_property_load_common (GladeEditorProperty *eprop, 
				   GladeProperty       *property)
{
	/* Hide properties that are removed for some particular widgets. 
	 */
	if (property)
	{
		gtk_widget_show (GTK_WIDGET (eprop));
		gtk_widget_show (eprop->item_label);
	}
	else
	{
		gtk_widget_hide (GTK_WIDGET (eprop));
		gtk_widget_hide (eprop->item_label);
	}

	/* disconnect anything from previously loaded property */
	if (eprop->property != property && eprop->property != NULL) 
	{
		if (eprop->tooltip_id > 0)
			g_signal_handler_disconnect (eprop->property,
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

		eprop->tooltip_id   = 0;
		eprop->sensitive_id = 0;
		eprop->changed_id   = 0;
		eprop->enabled_id   = 0;

		/* Unref it here */
		g_object_weak_unref (G_OBJECT (eprop->property),
				     (GWeakNotify)glade_eprop_property_finalized,
				     eprop);


		/* For a reason I cant quite tell yet, this is the only
		 * safe way to nullify the property member of the eprop
		 * without leeking signal connections to properties :-/
		 */
		if (property == NULL) 
		{
			eprop->property = NULL;
		}
	}

	/* Connect new stuff, deal with tooltip
	 */
	if (eprop->property != property && property != NULL)
	{
		eprop->property = property;

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

		/* In query dialogs when the user hits cancel, 
		 * these babies go away (so better stay protected).
		 */
		g_object_weak_ref (G_OBJECT (eprop->property),
				   (GWeakNotify)glade_eprop_property_finalized,
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

	/**
	 * GladeEditorProperty::gtk-doc-search:
	 * @gladeeditor: the #GladeEditorProperty which received the signal.
	 * @arg1: the (#gchar *) book to search or %NULL
	 * @arg2: the (#gchar *) page to search or %NULL
	 * @arg3: the (#gchar *) search string or %NULL
	 *
	 * Emitted when the editor property requests that a doc-search be performed.
	 */
	glade_editor_property_signals[GTK_DOC_SEARCH] =
		g_signal_new ("gtk-doc-search",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeEditorPropertyClass,
					       gtk_doc_search),
			      NULL, NULL,
			      glade_marshal_VOID__STRING_STRING_STRING,
			      G_TYPE_NONE, 3, 
			      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

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

	g_object_class_install_property 
		(object_class, PROP_SHOW_INFO,
		 g_param_spec_boolean 
		 ("show-info", _("Show Info"), 
		  _("Whether we should show an informational button"),
		  FALSE, G_PARAM_READWRITE));
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
		if (G_IS_PARAM_SPEC_INT(eprop->klass->pspec))
			val = (gfloat) g_value_get_int (property->value);
		else if (G_IS_PARAM_SPEC_UINT(eprop->klass->pspec))
			val = (gfloat) g_value_get_uint (property->value);		
		else if (G_IS_PARAM_SPEC_LONG(eprop->klass->pspec))
			val = (gfloat) g_value_get_long (property->value);		
		else if (G_IS_PARAM_SPEC_ULONG(eprop->klass->pspec))
			val = (gfloat) g_value_get_ulong (property->value);		
		else if (G_IS_PARAM_SPEC_INT64(eprop->klass->pspec))
			val = (gfloat) g_value_get_int64 (property->value);		
		else if (G_IS_PARAM_SPEC_UINT64(eprop->klass->pspec))
			val = (gfloat) g_value_get_uint64 (property->value);		
		else if (G_IS_PARAM_SPEC_DOUBLE(eprop->klass->pspec))
			val = (gfloat) g_value_get_double (property->value);
		else if (G_IS_PARAM_SPEC_FLOAT(eprop->klass->pspec))
			val = g_value_get_float (property->value);
		else
			g_warning ("Unsupported type %s\n",
				   g_type_name(G_PARAM_SPEC_TYPE (eprop->klass->pspec)));
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (eprop_numeric->spin), val);
	}
}


static void
glade_eprop_numeric_changed (GtkWidget *spin,
			     GladeEditorProperty *eprop)
{
	GValue val = { 0, };

	if (eprop->loading) return;

	g_value_init (&val, eprop->klass->pspec->value_type);
	
	if (G_IS_PARAM_SPEC_INT(eprop->klass->pspec))
		g_value_set_int (&val, gtk_spin_button_get_value_as_int
				 (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_UINT(eprop->klass->pspec))
		g_value_set_uint (&val, gtk_spin_button_get_value_as_int
				  (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_LONG(eprop->klass->pspec))
		g_value_set_long (&val, (glong)gtk_spin_button_get_value_as_int
				  (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_ULONG(eprop->klass->pspec))
		g_value_set_ulong (&val, (gulong)gtk_spin_button_get_value_as_int
				   (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_INT64(eprop->klass->pspec))
		g_value_set_int64 (&val, (gint64)gtk_spin_button_get_value_as_int
				  (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_UINT64(eprop->klass->pspec))
		g_value_set_uint64 (&val, (guint64)gtk_spin_button_get_value_as_int
				   (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_FLOAT(eprop->klass->pspec))
		g_value_set_float (&val, (gfloat) gtk_spin_button_get_value
				   (GTK_SPIN_BUTTON (spin)));
	else if (G_IS_PARAM_SPEC_DOUBLE(eprop->klass->pspec))
		g_value_set_double (&val, gtk_spin_button_get_value
				    (GTK_SPIN_BUTTON (spin)));
	else
		g_warning ("Unsupported type %s\n",
			   g_type_name(G_PARAM_SPEC_TYPE (eprop->klass->pspec)));

	glade_editor_property_commit (eprop, &val);
	g_value_unset (&val);
}

static GtkWidget *
glade_eprop_numeric_create_input (GladeEditorProperty *eprop)
{
	GladeEPropNumeric *eprop_numeric = GLADE_EPROP_NUMERIC (eprop);
	GtkAdjustment     *adjustment;

	adjustment          = glade_property_class_make_adjustment (eprop->klass);
	eprop_numeric->spin = gtk_spin_button_new (adjustment, 4,
						   G_IS_PARAM_SPEC_FLOAT (eprop->klass->pspec) ||
						   G_IS_PARAM_SPEC_DOUBLE (eprop->klass->pspec)
						   ? 2 : 0);
	gtk_widget_show (eprop_numeric->spin);
	
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
		eclass = g_type_class_ref (eprop->klass->pspec->value_type);
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

	g_value_init (&val, eprop->klass->pspec->value_type);
	g_value_set_enum (&val, ival);

	glade_editor_property_commit (eprop, &val);
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
	GladePropertyClass  *klass;
	GEnumClass          *eclass;
	gboolean             stock;
	guint                i;
	
	klass  = eprop->klass;
	eclass = g_type_class_ref (klass->pspec->value_type);
	stock  = (klass->pspec->value_type == GLADE_TYPE_STOCK) ||
		(klass->pspec->value_type == GLADE_TYPE_STOCK_IMAGE);

	menu = gtk_menu_new ();

	for (i = 0; i < eclass->n_values; i++)
	{
		const gchar *value_name = 
			glade_property_class_get_displayable_value
				(klass, eclass->values[i].value);
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

	gtk_widget_show_all (eprop_enum->option_menu);

	g_type_class_unref (eclass);

	return eprop_enum->option_menu;
}

/*******************************************************************************
                        GladeEditorPropertyFlagsClass
 *******************************************************************************/
typedef struct {
	GladeEditorProperty  parent_instance;

	GtkTreeModel	*model;
	GtkWidget       *entry;
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
	GladeEPropFlags *eprop_flags = GLADE_EPROP_FLAGS(object);

	g_object_unref (G_OBJECT (eprop_flags->model));

	/* Chain up */
	G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_flags_load (GladeEditorProperty *eprop, GladeProperty *property)
{
	GladeEPropFlags *eprop_flags = GLADE_EPROP_FLAGS (eprop);
	GFlagsClass *klass;
	guint flag_num, value;
	GString *string = g_string_new (NULL);

	/* Chain up first */
	editor_property_class->load (eprop, property);
	
	gtk_list_store_clear(GTK_LIST_STORE(eprop_flags->model));

	if (property)
	{
		/* Populate the model with the flags. */
		klass = g_type_class_ref (G_VALUE_TYPE (property->value));
		value = g_value_get_flags (property->value);
			
		/* Step through each of the flags in the class. */
		for (flag_num = 0; flag_num < klass->n_values; flag_num++) {
			GtkTreeIter iter;
			guint mask;
			gboolean setting;
			const gchar *value_name;
			
			mask = klass->values[flag_num].value;
			setting = ((value & mask) == mask) ? TRUE : FALSE;
			
			value_name = glade_property_class_get_displayable_value
				(eprop->klass, klass->values[flag_num].value);

			if (value_name == NULL) value_name = klass->values[flag_num].value_name;
			
			/* Setup string for property label */
			if (setting)
			{
				if (string->len > 0)
					g_string_append (string, " | ");
				g_string_append (string, value_name);
			}
			
			/* Add a row to represent the flag. */
			gtk_list_store_append (GTK_LIST_STORE(eprop_flags->model), &iter);
			gtk_list_store_set (GTK_LIST_STORE(eprop_flags->model), &iter,
					    FLAGS_COLUMN_SETTING,
					    setting,
					    FLAGS_COLUMN_SYMBOL,
					    value_name,
					    -1);

		}
		g_type_class_unref(klass);	
	}

	gtk_entry_set_text (GTK_ENTRY (eprop_flags->entry), string->str);

	g_string_free (string, TRUE);
}


static void
flag_toggled_direct (GtkCellRendererToggle *cell,
		     gchar                 *path_string,
		     GladeEditorProperty   *eprop)
{
	GtkTreeIter iter;
	guint new_value = 0;
	gboolean selected;
	guint value = 0;
	gint flag_num = 0;
	GFlagsClass *klass;

	GladeEPropFlags *eprop_flags = GLADE_EPROP_FLAGS(eprop);

	if (!eprop->property)
		return ;

	klass = g_type_class_ref (G_VALUE_TYPE (eprop->property->value));
	value = g_value_get_flags (eprop->property->value);

	gtk_tree_model_get_iter_from_string (eprop_flags->model, &iter, path_string);

	gtk_tree_model_get (eprop_flags->model, &iter,
			    FLAGS_COLUMN_SETTING, &selected,
			    -1);

	selected = selected ? FALSE : TRUE;

	gtk_list_store_set (GTK_LIST_STORE (eprop_flags->model), &iter,
			    FLAGS_COLUMN_SETTING, selected,
			    -1);


	gtk_tree_model_get_iter_first (GTK_TREE_MODEL (eprop_flags->model), &iter);

	/* Step through each of the flags in the class, checking if
	   the corresponding toggle in the dialog is selected, If it
	   is, OR the flags' mask with the new value. */
	for (flag_num = 0; flag_num < klass->n_values; flag_num++) {
		gboolean setting;

		gtk_tree_model_get (GTK_TREE_MODEL (eprop_flags->model), &iter,
				    FLAGS_COLUMN_SETTING, &setting,
				    -1);

		if (setting)
			new_value |= klass->values[flag_num].value;

		gtk_tree_model_iter_next (GTK_TREE_MODEL (eprop_flags->model),
					  &iter);
	}

	/* If the new_value is different from the old value, we need
	   to update the property. */
	if (new_value != value) {
		GValue val = { 0, };

		g_value_init (&val, G_VALUE_TYPE (eprop->property->value));
		g_value_set_flags (&val, new_value);

		glade_editor_property_commit (eprop, &val);
		g_value_unset (&val);
	}



}

static GtkWidget*
glade_eprop_flags_create_treeview(GladeEditorProperty *eprop)
{
	GtkWidget *scrolled_window;
	GtkWidget *tree_view;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GladeEPropFlags *eprop_flags=GLADE_EPROP_FLAGS(eprop);
	if (!eprop_flags->model)
		eprop_flags->model = GTK_TREE_MODEL(gtk_list_store_new (2, G_TYPE_BOOLEAN, 
									G_TYPE_STRING));


	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), 
					     GTK_SHADOW_IN);
	gtk_widget_show (scrolled_window);

	

	tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (eprop_flags->model));
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
			  G_CALLBACK (flag_toggled_direct), eprop);


	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "text", FLAGS_COLUMN_SYMBOL,
					     NULL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);



	return scrolled_window;
}

static void
glade_eprop_flags_show_dialog (GtkWidget           *button,
			       GladeEditorProperty *eprop)
{
	GtkWidget *dialog;
	GtkWidget *view;
	GtkWidget *label;
	GtkWidget *vbox;

	dialog = gtk_dialog_new_with_buttons (_("Select Fields"),
					      GTK_WINDOW (gtk_widget_get_toplevel (button)),
					      GTK_DIALOG_MODAL,
					      GTK_STOCK_CLOSE,
					      GTK_RESPONSE_CLOSE,
					      NULL);
					      
	gtk_window_set_default_size (GTK_WINDOW (dialog), 300, 400);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

	/* HIG spacings */
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2); /* 2 * 5 + 2 = 12 */
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->action_area), 6);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

	view = glade_eprop_flags_create_treeview (eprop);
	
	label = gtk_label_new_with_mnemonic (_("_Select individual fields:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), gtk_bin_get_child (GTK_BIN (view)));

	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);	
	gtk_box_pack_start (GTK_BOX (vbox), view, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 0);

	gtk_widget_show (label);
	gtk_widget_show (view);
	gtk_widget_show (vbox);			    
			    
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}


static GtkWidget *
glade_eprop_flags_create_input (GladeEditorProperty *eprop)
{
	GtkWidget *vbox, *hbox, *button, *widget;
	GladeEPropFlags *eprop_flags = GLADE_EPROP_FLAGS(eprop);

	hbox   = gtk_hbox_new (FALSE, 0);
	vbox   = gtk_vbox_new (FALSE, 0);
	
	widget = glade_eprop_flags_create_treeview (eprop);

	eprop_flags->entry = gtk_entry_new ();
	gtk_entry_set_editable (GTK_ENTRY (eprop_flags->entry), FALSE);

	gtk_box_pack_start (GTK_BOX (vbox), eprop_flags->entry, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

	button = gtk_button_new_with_label ("...");
	gtk_box_pack_start (GTK_BOX (hbox), button,  FALSE, FALSE, 0);

	gtk_widget_show_all (hbox);

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
		if ((text = glade_property_class_make_string_from_gvalue
		     (eprop->klass, property->value)) != NULL)
		{
			gtk_entry_set_text (GTK_ENTRY (eprop_color->entry), text);
			g_free (text);
		}
		else
			gtk_entry_set_text (GTK_ENTRY (eprop_color->entry), "");
			
		if ((color = g_value_get_boxed (property->value)) != NULL)
			gtk_color_button_set_color (GTK_COLOR_BUTTON (eprop_color->cbutton),
						    color);
		else
		{
			GdkColor black = { 0, };

			/* Manually fill it with black for an NULL value.
			 */
			if (gdk_color_parse("Black", &black) &&
			    gdk_colormap_alloc_color(gtk_widget_get_default_colormap(),
						     &black, FALSE, TRUE))
				gtk_color_button_set_color
					(GTK_COLOR_BUTTON (eprop_color->cbutton),
					 &black);
		}
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

	glade_editor_property_commit (eprop, &value);
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
                        GladeEditorPropertyNamedIconClass
 *******************************************************************************/
typedef struct {
	GladeEditorProperty  parent_instance;

	GtkWidget           *entry;
	gchar               *current_context;
} GladeEPropNamedIcon;

GLADE_MAKE_EPROP (GladeEPropNamedIcon, glade_eprop_named_icon)
#define GLADE_TYPE_EPROP_NAMED_ICON            (glade_eprop_named_icon_get_type())
#define GLADE_EPROP_NAMED_ICON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_NAMED_ICON, GladeEPropNamedIcon))
#define GLADE_EPROP_NAMED_ICON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_NAMED_ICON, GladeEPropNamedIconClass))
#define GLADE_IS_EPROP_NAMED_ICON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_NAMED_ICON))
#define GLADE_IS_EPROP_NAMED_ICON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_NAMED_ICON))
#define GLADE_EPROP_NAMED_ICON_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_NAMED_ICON, GladeEPropNamedIconClass))


static void
glade_eprop_named_icon_finalize (GObject *object)
{
	/* Chain up */
	G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_named_icon_load (GladeEditorProperty *eprop,
			     GladeProperty       *property)
{
	GladeEPropNamedIcon *eprop_named_icon = GLADE_EPROP_NAMED_ICON (eprop);
	GtkEntry *entry;
	const gchar *text;

	/* Chain up first */
	editor_property_class->load (eprop, property);
	
	if (property == NULL)
		return;
	
	entry = GTK_ENTRY (eprop_named_icon->entry);
	text = g_value_get_string (property->value);

	gtk_entry_set_text (entry, text ? text : "");
}

static void
glade_eprop_named_icon_changed_common (GladeEditorProperty *eprop,
				       const gchar *text,
				       gboolean use_command)
{
	GValue  *val;
	gchar  *prop_text;

	val = g_new0 (GValue, 1);
	
	g_value_init (val, G_TYPE_STRING);

	glade_property_get (eprop->property, &prop_text);

	/* Here we try not to modify the project state by not 
	 * modifying a null value for an unchanged property.
	 */
	if (prop_text == NULL &&
	    text && text[0] == '\0')
		g_value_set_string (val, NULL);
	else if (text == NULL &&
		 prop_text && prop_text == '\0')
		g_value_set_string (val, "");
	else
		g_value_set_string (val, text);

	glade_editor_property_commit (eprop, val);
	g_value_unset (val);
	g_free (val);
}

static void
glade_eprop_named_icon_changed (GtkWidget           *entry,
			        GladeEditorProperty *eprop)
{
	gchar *text;

	if (eprop->loading)
		return;
	
	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	glade_eprop_named_icon_changed_common (eprop, text, eprop->use_command);
	
	g_free (text);
}

static gboolean
glade_eprop_named_icon_focus_out (GtkWidget           *entry,
				  GdkEventFocus       *event,
				  GladeEditorProperty *eprop)
{
	glade_eprop_named_icon_changed (entry, eprop);
	return FALSE;
}

static void 
glade_eprop_named_icon_activate (GtkEntry *entry,
				 GladeEPropNamedIcon *eprop)
{
	glade_eprop_named_icon_changed (GTK_WIDGET (entry), GLADE_EDITOR_PROPERTY (eprop));
}

static void
chooser_response (GladeNamedIconChooserDialog *dialog,
		  gint response_id,
		  GladeEPropNamedIcon *eprop)
{
	gchar *icon_name;
	
	switch (response_id) {
	
	case GTK_RESPONSE_OK:
	
		g_free (eprop->current_context);
		eprop->current_context = glade_named_icon_chooser_dialog_get_context (dialog);
		icon_name = glade_named_icon_chooser_dialog_get_icon_name (dialog);
		
		gtk_entry_set_text (GTK_ENTRY (eprop->entry), icon_name);
		gtk_widget_destroy (GTK_WIDGET (dialog));
					
		g_free (icon_name);
		
		glade_eprop_named_icon_changed (eprop->entry, GLADE_EDITOR_PROPERTY (eprop));
		
		break;
		
	case GTK_RESPONSE_CANCEL:
	
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	
	case GTK_RESPONSE_HELP:
	
		break;
		
	case GTK_RESPONSE_DELETE_EVENT:
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}		
}


static void
glade_eprop_named_icon_show_chooser_dialog (GtkWidget           *button,
				            GladeEditorProperty *eprop)
{
	GtkWidget *dialog;

	dialog = glade_named_icon_chooser_dialog_new ("Select Named Icon",
						       GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (eprop))),
						       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						       GTK_STOCK_OK, GTK_RESPONSE_OK,
						       NULL);
						       
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	glade_named_icon_chooser_dialog_set_context   (GLADE_NAMED_ICON_CHOOSER_DIALOG (dialog), GLADE_EPROP_NAMED_ICON (eprop)->current_context);

	glade_named_icon_chooser_dialog_set_icon_name (GLADE_NAMED_ICON_CHOOSER_DIALOG (dialog),
						       gtk_entry_get_text (GTK_ENTRY (GLADE_EPROP_NAMED_ICON (eprop)->entry)));


	g_signal_connect (dialog, "response",
			  G_CALLBACK (chooser_response),
			  eprop);	

	gtk_widget_show (dialog);

}

static GtkWidget *
glade_eprop_named_icon_create_input (GladeEditorProperty *eprop)
{
	GladeEPropNamedIcon *eprop_named_icon = GLADE_EPROP_NAMED_ICON (eprop);
	GtkWidget *hbox;
	GtkWidget *button;

	hbox = gtk_hbox_new (FALSE, 0);

	eprop_named_icon->entry = gtk_entry_new ();
	gtk_widget_show (eprop_named_icon->entry);
	
	eprop_named_icon->current_context = NULL;

	gtk_box_pack_start (GTK_BOX (hbox), eprop_named_icon->entry, TRUE, TRUE, 0); 

	g_signal_connect (G_OBJECT (eprop_named_icon->entry), "activate",
			  G_CALLBACK (glade_eprop_named_icon_activate),
			  eprop);
	
	g_signal_connect (G_OBJECT (eprop_named_icon->entry), "focus-out-event",
			  G_CALLBACK (glade_eprop_named_icon_focus_out),
			  eprop);

	button = gtk_button_new_with_label ("\342\200\246");
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0); 
	g_signal_connect (button, "clicked",
			  G_CALLBACK (glade_eprop_named_icon_show_chooser_dialog),
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

	if (GTK_IS_ENTRY (eprop_text->text_entry))
	{
		GtkEntry *entry = GTK_ENTRY (eprop_text->text_entry);
		const gchar *text = g_value_get_string (property->value);

		gtk_entry_set_text (entry, text ? text : "");
		
	}
	else if (GTK_IS_TEXT_VIEW (eprop_text->text_entry))
	{
		GtkTextBuffer  *buffer;
			
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (eprop_text->text_entry));

		if (G_VALUE_HOLDS (property->value, G_TYPE_STRV) ||
		    G_VALUE_HOLDS (property->value, G_TYPE_VALUE_ARRAY))
		{
			gchar *text = glade_property_class_make_string_from_gvalue (
						property->klass, property->value);
			gtk_text_buffer_set_text (buffer, text ? text : "", -1);
			g_free (text);
		}
		else
		{
			const gchar *text = g_value_get_string (property->value);
			gtk_text_buffer_set_text (buffer, text ? text : "", -1);
		}
	}
	else
	{
		g_warning ("BUG! Invalid Text Widget type.");
	}
}

static void
glade_eprop_text_changed_common (GladeEditorProperty *eprop,
				 const gchar *text,
				 gboolean use_command)
{
	GValue  *val;
	gchar  *prop_text;

	if (eprop->property->klass->pspec->value_type == G_TYPE_STRV ||
	    eprop->property->klass->pspec->value_type == G_TYPE_VALUE_ARRAY)
	{
		val = glade_property_class_make_gvalue_from_string 
			(eprop->property->klass, text, NULL);
	} 
	else
	{
		val = g_new0 (GValue, 1);
		
		g_value_init (val, G_TYPE_STRING);

		glade_property_get (eprop->property, &prop_text);

		/* Here we try not to modify the project state by not 
		 * modifying a null value for an unchanged property.
		 */
		if (prop_text == NULL &&
		    text && text[0] == '\0')
			g_value_set_string (val, NULL);
		else if (text == NULL &&
			 prop_text && prop_text == '\0')
			g_value_set_string (val, "");
		else
			g_value_set_string (val, text);
	}

	glade_editor_property_commit (eprop, val);
	g_value_unset (val);
	g_free (val);
}

static void
glade_eprop_text_changed (GtkWidget           *entry,
			  GladeEditorProperty *eprop)
{
	gchar *text;

	if (eprop->loading) return;
	
	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	glade_eprop_text_changed_common (eprop, text, eprop->use_command);
	
	g_free (text);
}

static void
glade_eprop_text_buffer_changed (GtkTextBuffer       *buffer,
				 GladeEditorProperty *eprop)
{
	gchar *text;
	GtkTextIter    start, end;
	
	if (eprop->loading) return;

	gtk_text_buffer_get_start_iter (buffer, &start);
	gtk_text_buffer_get_end_iter (buffer, &end);

	text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

	glade_eprop_text_changed_common (eprop, text, eprop->use_command);

	g_free (text);
}

static void
glade_eprop_text_show_i18n_dialog (GtkWidget           *entry,
				   GladeEditorProperty *eprop)
{
	GtkWidget     *dialog;
	GtkWidget     *vbox, *hbox;
	GtkWidget     *label;
	GtkWidget     *sw;
	GtkWidget     *alignment;
	GtkWidget     *text_view, *comment_view;
	GtkTextBuffer *text_buffer, *comment_buffer;
	GtkWidget     *translatable_button, *context_button;
	const gchar   *text;
	gint           res;
	gchar         *str;
	GParamSpec    *pspec;

	dialog = gtk_dialog_new_with_buttons (_("Edit Text"),
					      GTK_WINDOW (gtk_widget_get_toplevel (entry)),
					      GTK_DIALOG_MODAL,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
						 GTK_RESPONSE_OK,
						 GTK_RESPONSE_CANCEL,
						 -1);

	/* HIG spacings */
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2); /* 2 * 5 + 2 = 12 */
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->action_area), 6);


	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
	gtk_widget_show (vbox);

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

	/* Add a cute tooltip */
	if ((pspec =
	     g_object_class_find_property (G_OBJECT_GET_CLASS (eprop->property),
					   "i18n-translatable")) != NULL)
		gtk_widget_set_tooltip_text (translatable_button,
					       g_param_spec_get_blurb (pspec));

	context_button = gtk_check_button_new_with_mnemonic (_("_Has context prefix"));
	gtk_widget_show (context_button);
	gtk_box_pack_start (GTK_BOX (hbox), context_button, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (context_button),
				      glade_property_i18n_get_has_context (eprop->property));

	/* Add a cute tooltip */
	if ((pspec =
	     g_object_class_find_property (G_OBJECT_GET_CLASS (eprop->property),
					   "i18n-has-context")) != NULL)
		gtk_widget_set_tooltip_text (context_button,
					       g_param_spec_get_blurb (pspec));

	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 12, 0, 0, 0);
	gtk_widget_show (alignment);

	/* Comments. */
	label = gtk_label_new_with_mnemonic (_("Co_mments for translators:"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_container_add (GTK_CONTAINER (alignment), label);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (sw);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);

	comment_view = gtk_text_view_new ();
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (comment_view), GTK_WRAP_WORD);
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
		gboolean translatable, has_context;
		
		/* get the new values for translatable, has_context, and comment */
		translatable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (translatable_button));
		has_context = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (context_button));

		gtk_text_buffer_get_bounds (comment_buffer, &start, &end);
		str = gtk_text_buffer_get_text (comment_buffer, &start, &end, TRUE);
		if (str[0] == '\0')
		{
			g_free (str);
			str = NULL;
		}
		
		/* set the new i18n data via a glade command so it can be undone */
		glade_command_set_i18n (eprop->property, translatable, has_context, str);
		g_free (str);
		
		/* Text */
		gtk_text_buffer_get_bounds (text_buffer, &start, &end);
		str = gtk_text_buffer_get_text (text_buffer, &start, &end, TRUE);
		if (str[0] == '\0')
		{
			g_free (str);
			str = NULL;
		}
		
		/* set the new text */
		glade_eprop_text_changed_common (eprop, str, eprop->use_command);
		g_free (str);
		
	}

	gtk_widget_destroy (dialog);
}

static GtkWidget *
glade_eprop_text_create_input (GladeEditorProperty *eprop)
{
	GladeEPropText      *eprop_text = GLADE_EPROP_TEXT (eprop);
	GladePropertyClass  *klass;
	GtkWidget           *hbox;

	klass = eprop->klass;

	hbox = gtk_hbox_new (FALSE, 0);

	if (klass->visible_lines > 1 ||
	    klass->pspec->value_type == G_TYPE_STRV ||
	    klass->pspec->value_type == G_TYPE_VALUE_ARRAY) 
	{
		GtkWidget  *swindow;
		GtkTextBuffer *buffer;

		swindow = gtk_scrolled_window_new (NULL, NULL);

		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
						GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swindow), GTK_SHADOW_IN);

		eprop_text->text_entry = gtk_text_view_new ();
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (eprop_text->text_entry));

		gtk_container_add (GTK_CONTAINER (swindow), eprop_text->text_entry);
		gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (swindow), TRUE, TRUE, 0); 

		gtk_widget_show_all (swindow);
		
		g_signal_connect (G_OBJECT (buffer), "changed",
				  G_CALLBACK (glade_eprop_text_buffer_changed),
				  eprop);

	} else {
		eprop_text->text_entry = gtk_entry_new ();
		gtk_widget_show (eprop_text->text_entry);

		gtk_box_pack_start (GTK_BOX (hbox), eprop_text->text_entry, TRUE, TRUE, 0); 

		g_signal_connect (G_OBJECT (eprop_text->text_entry), "changed",
				  G_CALLBACK (glade_eprop_text_changed),
				  eprop);
	}
	
	if (klass->translatable) {
		GtkWidget *button = gtk_button_new_with_label ("...");
		gtk_widget_show (button);
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

	glade_editor_property_commit (eprop, &val);

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

	if (property && GTK_IS_ENTRY (eprop_unichar->entry))
	{
		GtkEntry *entry = GTK_ENTRY (eprop_unichar->entry);
		gchar utf8st[8];
		gint n;
		
		if ((n = g_unichar_to_utf8 (g_value_get_uint (property->value), utf8st)))
		{
			utf8st[n] = '\0';
			gtk_entry_set_text (entry, utf8st);
		}
	}
}


static void
glade_eprop_unichar_changed (GtkWidget           *entry,
			     GladeEditorProperty *eprop)
{
	const gchar *text;

	if (eprop->loading) return;

	if ((text = gtk_entry_get_text (GTK_ENTRY (entry))) != NULL)
	{
		gunichar unich = g_utf8_get_char (text);
		GValue val = { 0, };
		
		g_value_init (&val, G_TYPE_UINT);
		g_value_set_uint (&val, unich);

		glade_editor_property_commit (eprop, &val);

		g_value_unset (&val);
	}
}

static void
glade_eprop_unichar_delete (GtkEditable *editable,
			    gint start_pos,
			    gint end_pos,
			    GladeEditorProperty *eprop)
{
	if (eprop->loading) return;
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
	if (eprop->loading) return;
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
                        GladeEditorPropertyResourceClass
 *******************************************************************************/
typedef struct {
	GladeEditorProperty parent_instance;
	
	GtkWidget *entry, *button;
} GladeEPropResource;

GLADE_MAKE_EPROP (GladeEPropResource, glade_eprop_resource)
#define GLADE_TYPE_EPROP_RESOURCE            (glade_eprop_resource_get_type())
#define GLADE_EPROP_RESOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_RESOURCE, GladeEPropResource))
#define GLADE_EPROP_RESOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_RESOURCE, GladeEPropResourceClass))
#define GLADE_IS_EPROP_RESOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_RESOURCE))
#define GLADE_IS_EPROP_RESOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_RESOURCE))
#define GLADE_EPROP_RESOURCE_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_RESOURCE, GladeEPropResourceClass))

static void
glade_eprop_resource_finalize (GObject *object)
{
	/* Chain up */
	G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_resource_entry_activate (GtkEntry *entry, GladeEditorProperty *eprop)
{
	GladeProject *project = glade_widget_get_project (eprop->property->widget);
	GValue *value = glade_property_class_make_gvalue_from_string 
		(eprop->klass, gtk_entry_get_text(entry), project);

	/* Set project resource here where we still have the fullpath.
	 */
	glade_project_set_resource (project, eprop->property, 
				    gtk_entry_get_text(entry));
	
	glade_editor_property_commit (eprop, value);

	g_value_unset (value);
	g_free (value);
}

static gboolean
glade_eprop_resource_entry_focus_out (GtkWidget           *entry,
				      GdkEventFocus       *event,
				      GladeEditorProperty *eprop)
{
	glade_eprop_resource_entry_activate (GTK_ENTRY (entry), eprop);
	return FALSE;
}

static void
glade_eprop_resource_select_file (GtkButton *button, GladeEditorProperty *eprop)
{
	GladeProject       *project = glade_widget_get_project (eprop->property->widget);
	GtkWidget          *dialog;
	GtkFileFilter      *filter;
	GValue             *value;
	gchar              *file, *basename;
	
	if (eprop->loading) return;

	dialog = gtk_file_chooser_dialog_new ("Select a File",
					NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					NULL);
	
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), TRUE);

	if (eprop->klass->pspec->value_type == GDK_TYPE_PIXBUF)
	{
		filter = gtk_file_filter_new ();
		gtk_file_filter_add_pixbuf_formats (filter);
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);
	}

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
	{
		file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

		/* Set project resource here where we still have the fullpath.
		 */
		glade_project_set_resource (project, eprop->property, file);
		basename = g_path_get_basename (file);

		value = glade_property_class_make_gvalue_from_string 
			(eprop->klass, basename, project);
		
		glade_editor_property_commit (eprop, value);
		
		g_value_unset (value);
		g_free (value);
		g_free (file);
		g_free (basename);
	}
	gtk_widget_destroy (dialog);
}

static void
glade_eprop_resource_load (GladeEditorProperty *eprop, GladeProperty *property)
{
	GladeEPropResource *eprop_resource = GLADE_EPROP_RESOURCE (eprop);
	gchar *file;

	/* Chain up first */
	editor_property_class->load (eprop, property);

	if (property == NULL) return;

	file  = glade_property_class_make_string_from_gvalue
						(eprop->klass, property->value);
	if (file)
	{
		gtk_entry_set_text (GTK_ENTRY (eprop_resource->entry), file);
		g_free (file);
	}
	else
	{
		gtk_entry_set_text (GTK_ENTRY (eprop_resource->entry), "");
	}
}

static GtkWidget *
glade_eprop_resource_create_input (GladeEditorProperty *eprop)
{
	GladeEPropResource *eprop_resource = GLADE_EPROP_RESOURCE (eprop);
	GtkWidget *hbox;

	hbox = gtk_hbox_new (FALSE, 0);

	eprop_resource->entry = gtk_entry_new ();
	gtk_widget_show (eprop_resource->entry);
	
	eprop_resource->button = gtk_button_new_with_label ("...");
	gtk_widget_show (eprop_resource->button);
	
	gtk_box_pack_start (GTK_BOX (hbox), eprop_resource->entry, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), eprop_resource->button, FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (eprop_resource->entry), "activate",
			  G_CALLBACK (glade_eprop_resource_entry_activate), 
			  eprop);
	g_signal_connect (G_OBJECT (eprop_resource->entry), "focus-out-event",
			  G_CALLBACK (glade_eprop_resource_entry_focus_out),
			  eprop);
	g_signal_connect (G_OBJECT (eprop_resource->button), "clicked",
			  G_CALLBACK (glade_eprop_resource_select_file), 
			  eprop);

	return hbox;
}


/*******************************************************************************
                        GladeEditorPropertyObjectClass
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

static gboolean
glade_eprop_object_is_selected (GladeEditorProperty *eprop,
				GladeWidget         *widget)
{
	GList *list;

	if (GLADE_IS_PARAM_SPEC_OBJECTS (eprop->klass->pspec))
	{
		glade_property_get (eprop->property, &list);
		return g_list_find (list, widget->object) != NULL;
	}
	return glade_property_equals (eprop->property, widget->object);
}


/*
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

			if (GLADE_IS_PARAM_SPEC_OBJECTS (eprop->klass->pspec))
			{
				has_decendant = glade_widget_has_decendant 
					(widget, 
					 glade_param_spec_objects_get_type 
					 (GLADE_PARAM_SPEC_OBJECTS(eprop->klass->pspec)));
				good_type = 
					g_type_is_a
					(widget->adaptor->type,
					 glade_param_spec_objects_get_type 
					 (GLADE_PARAM_SPEC_OBJECTS(eprop->klass->pspec))) ||
					glade_util_class_implements_interface
					(widget->adaptor->type, 
					 glade_param_spec_objects_get_type 
					 (GLADE_PARAM_SPEC_OBJECTS(eprop->klass->pspec)));

			}
			else
			{
				has_decendant = glade_widget_has_decendant 
					(widget, eprop->klass->pspec->value_type);

				good_type = g_type_is_a (widget->adaptor->type, 
							 eprop->klass->pspec->value_type);

			}
			
			if (good_type || has_decendant)
			{
				gtk_tree_store_append (model, &iter, parent_iter);
				gtk_tree_store_set    
					(model, &iter, 
					 OBJ_COLUMN_WIDGET, widget, 
					 OBJ_COLUMN_WIDGET_NAME, 
					 glade_eprop_object_name (widget->name, model, parent_iter),
					 OBJ_COLUMN_WIDGET_CLASS, 
					 widget->adaptor->title,
					 /* Selectable if its a compatible type and
					  * its not itself.
					  */
					 OBJ_COLUMN_SELECTABLE, 
					 good_type && (widget != eprop->property->widget),
					 OBJ_COLUMN_SELECTED,
					 good_type && glade_eprop_object_is_selected
					 (eprop, widget), -1);
			}

			if (has_decendant &&
			    (children = glade_widget_adaptor_get_children
			     (widget->adaptor, widget->object)) != NULL)
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
	GladeProject  *project = glade_app_get_project ();
	GList         *list, *toplevels = NULL;

	/* Make a list of only the toplevel widgets */
	for (list = (GList *) glade_project_get_objects (project); list; list = list->next)
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

static gboolean
glade_eprop_object_clear_iter (GtkTreeModel *model,
			       GtkTreePath *path,
			       GtkTreeIter *iter,
			       gpointer data)
{
	gtk_tree_store_set (GTK_TREE_STORE (model), iter,
			    OBJ_COLUMN_SELECTED,  FALSE, -1);
	return FALSE;
}

static gboolean
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
	gboolean     enabled, radio;

	radio = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (model), "radio-list"));


	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    OBJ_COLUMN_SELECTED,  &enabled, -1);

	/* Clear the rest of the view first
	 */
	if (radio)
		gtk_tree_model_foreach (model, glade_eprop_object_clear_iter, NULL);

	gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			    OBJ_COLUMN_SELECTED,  
			    radio ? TRUE : !enabled, -1);

	gtk_tree_path_free (path);
}

static GtkWidget *
glade_eprop_object_view (GladeEditorProperty *eprop,
			 gboolean             radio)
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

	g_object_set_data (G_OBJECT (model), "radio-list", GINT_TO_POINTER (radio));

	view_widget = gtk_tree_view_new_with_model (model);

	/* Pass ownership to the view */
	g_object_unref (G_OBJECT (model));
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
 		      "radio",       radio,
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


static gchar *
glade_eprop_object_dialog_title (GladeEditorProperty *eprop)
{
	GladeWidgetAdaptor *adaptor;
	const gchar        *format = 
		GLADE_IS_PARAM_SPEC_OBJECTS (eprop->klass->pspec) ?
		_("Choose %s implementors") : _("Choose a %s in this project");

	if (GLADE_IS_PARAM_SPEC_OBJECTS (eprop->klass->pspec))
		return g_strdup_printf (format, g_type_name 
					(glade_param_spec_objects_get_type 
					 (GLADE_PARAM_SPEC_OBJECTS (eprop->klass->pspec))));
	else if ((adaptor =
		  glade_widget_adaptor_get_by_type
		  (eprop->klass->pspec->value_type)) != NULL)
		return g_strdup_printf (format, adaptor->title);
	
	/* Fallback on type name (which would look like "GtkButton"
	 * instead of "Button" and maybe not translated).
	 */
	return g_strdup_printf (format, g_type_name 
				(eprop->klass->pspec->value_type));
}

static void
glade_eprop_object_show_dialog (GtkWidget           *dialog_button,
				GladeEditorProperty *eprop)
{
	GtkWidget     *dialog, *parent;
	GtkWidget     *vbox, *label, *sw;
	GtkWidget     *tree_view;
	GladeProject  *project;
	gchar         *title = glade_eprop_object_dialog_title (eprop);
	gint           res;

	
	project = glade_widget_get_project (eprop->property->widget);
	parent = gtk_widget_get_toplevel (GTK_WIDGET (eprop));

	dialog = gtk_dialog_new_with_buttons (title,
					      GTK_WINDOW (parent),
					      GTK_DIALOG_MODAL,
					      GTK_STOCK_CLEAR, GLADE_RESPONSE_CLEAR,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);
	g_free (title);
	
	gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
						 GTK_RESPONSE_OK,
						 GTK_RESPONSE_CANCEL,
						 GLADE_RESPONSE_CLEAR,
						 -1);
	
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	/* HIG settings */
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2); /* 2 * 5 + 2 = 12 */
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area), 5);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->action_area), 6);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 0);

	/* Checklist */
	label = gtk_label_new_with_mnemonic (_("O_bjects:"));
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


	tree_view = glade_eprop_object_view (eprop, TRUE);
	glade_eprop_object_populate_view (eprop, GTK_TREE_VIEW (tree_view));


	gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

	gtk_widget_show (tree_view);
	gtk_container_add (GTK_CONTAINER (sw), tree_view);
	
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), tree_view);
	
	
	/* Run the dialog */
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_OK) 
	{
		GladeWidget *selected = NULL;

		gtk_tree_model_foreach
			(gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view)), 
			 (GtkTreeModelForeachFunc) 
			 glade_eprop_object_selected_widget, &selected);

		if (selected)
		{
			GValue *value = glade_property_class_make_gvalue_from_string
				(eprop->klass, selected->name, project);

			glade_editor_property_commit (eprop, value);

			g_value_unset (value);
			g_free (value);
		}
	} 
	else if (res == GLADE_RESPONSE_CLEAR)
	{
		GValue *value = glade_property_class_make_gvalue_from_string
			(eprop->klass, NULL, project);
		
		glade_editor_property_commit (eprop, value);

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
	     (eprop->klass, property->value)) != NULL)
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
                        GladeEditorPropertyObjectsClass
 *******************************************************************************/

typedef struct {
	GladeEditorProperty parent_instance;
	
	GtkWidget *entry;
} GladeEPropObjects;

GLADE_MAKE_EPROP (GladeEPropObjects, glade_eprop_objects)
#define GLADE_TYPE_EPROP_OBJECTS            (glade_eprop_objects_get_type())
#define GLADE_EPROP_OBJECTS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_OBJECTS, GladeEPropObjects))
#define GLADE_EPROP_OBJECTS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_OBJECTS, GladeEPropObjectsClass))
#define GLADE_IS_EPROP_OBJECTS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_OBJECTS))
#define GLADE_IS_EPROP_OBJECTS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_OBJECTS))
#define GLADE_EPROP_OBJECTS_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_OBJECTS, GladeEPropObjectsClass))

static void
glade_eprop_objects_finalize (GObject *object)
{
	/* Chain up */
	G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_objects_load (GladeEditorProperty *eprop, GladeProperty *property)
{
	GladeEPropObjects *eprop_objects = GLADE_EPROP_OBJECTS (eprop);
	gchar *obj_name;

	/* Chain up first */
	editor_property_class->load (eprop, property);

	if (property == NULL) return;

	if ((obj_name  = glade_property_class_make_string_from_gvalue
	     (eprop->klass, property->value)) != NULL)
	{
		gtk_entry_set_text (GTK_ENTRY (eprop_objects->entry), obj_name);
		g_free (obj_name);
	}
	else
		gtk_entry_set_text (GTK_ENTRY (eprop_objects->entry), "");

}

static gboolean
glade_eprop_objects_selected_widget (GtkTreeModel  *model,
				     GtkTreePath    *path,
				     GtkTreeIter    *iter,
				     GList         **ret)
{
	gboolean     selected;
	GladeWidget *widget;

	gtk_tree_model_get (model, iter,
			    OBJ_COLUMN_SELECTED,  &selected, 
			    OBJ_COLUMN_WIDGET,    &widget, -1);


	if (selected) 
	{
		*ret = g_list_append (*ret, widget->object);
	}

	return FALSE;
}

static void
glade_eprop_objects_show_dialog (GtkWidget           *dialog_button,
				 GladeEditorProperty *eprop)
{
	GtkWidget     *dialog, *parent;
	GtkWidget     *vbox, *label, *sw;
	GtkWidget     *tree_view;
	GladeProject  *project;
	gchar         *title = glade_eprop_object_dialog_title (eprop);
	gint           res;

	
	project = glade_widget_get_project (eprop->property->widget);
	parent = gtk_widget_get_toplevel (GTK_WIDGET (eprop));


	dialog = gtk_dialog_new_with_buttons (title,
					      GTK_WINDOW (parent),
					      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CLEAR, GLADE_RESPONSE_CLEAR,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);
	g_free (title);
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);

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

	tree_view = glade_eprop_object_view (eprop, FALSE);
	glade_eprop_object_populate_view (eprop, GTK_TREE_VIEW (tree_view));

	gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

	gtk_widget_show (tree_view);
	gtk_container_add (GTK_CONTAINER (sw), tree_view);
	
	/* Run the dialog */
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_OK) 
	{
		GValue *value;
		GList  *selected = NULL;

		gtk_tree_model_foreach
			(gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view)), 
			 (GtkTreeModelForeachFunc) 
			 glade_eprop_objects_selected_widget, &selected);

		value = glade_property_class_make_gvalue 
			(eprop->klass, selected);

		glade_editor_property_commit (eprop, value);

		g_value_unset (value);
		g_free (value);
	} 
	else if (res == GLADE_RESPONSE_CLEAR)
	{
		GValue *value = glade_property_class_make_gvalue
			(eprop->klass, NULL);
		
		glade_editor_property_commit (eprop, value);

		g_value_unset (value);
		g_free (value);
	}
	gtk_widget_destroy (dialog);
}

static GtkWidget *
glade_eprop_objects_create_input (GladeEditorProperty *eprop)
{
	GladeEPropObjects *eprop_objects = GLADE_EPROP_OBJECTS (eprop);
	GtkWidget        *hbox;
	GtkWidget        *button;

	hbox                = gtk_hbox_new (FALSE, 0);
	eprop_objects->entry = gtk_entry_new ();
	gtk_entry_set_editable (GTK_ENTRY (eprop_objects->entry), FALSE);
	gtk_widget_show (eprop_objects->entry);
	gtk_box_pack_start (GTK_BOX (hbox), eprop_objects->entry, TRUE, TRUE, 0);

	button = gtk_button_new_with_label ("...");
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), button,  FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_eprop_objects_show_dialog),
			  eprop);
	return hbox;
}


/*******************************************************************************
                        GladeEditorPropertyAdjustmentClass
 *******************************************************************************/
typedef struct {
	GladeEditorProperty parent_instance;

	GtkWidget *value, *lower, *upper, *step_increment, *page_increment, *page_size;
	GtkAdjustment *value_adj;
	struct
	{
		gulong value, lower, upper, step_increment, page_increment, page_size;
	}ids;
} GladeEPropAdjustment;

GLADE_MAKE_EPROP (GladeEPropAdjustment, glade_eprop_adjustment)
#define GLADE_TYPE_EPROP_ADJUSTMENT            (glade_eprop_adjustment_get_type())
#define GLADE_EPROP_ADJUSTMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_ADJUSTMENT, GladeEPropAdjustment))
#define GLADE_EPROP_ADJUSTMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_ADJUSTMENT, GladeEPropAdjustmentClass))
#define GLADE_IS_EPROP_ADJUSTMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_ADJUSTMENT))
#define GLADE_IS_EPROP_ADJUSTMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_ADJUSTMENT))
#define GLADE_EPROP_ADJUSTMENT_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_ADJUSTMENT, GladeEPropAdjustmentClass))

static void
glade_eprop_adjustment_finalize (GObject *object)
{
	/* Chain up */
	G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

typedef struct _EPropAdjIdle EPropAdjIdleData;

struct _EPropAdjIdle
{
	GladeEditorProperty *eprop;
	gdouble value;
};

static gboolean
glade_eprop_adj_set_value_idle (gpointer p)
{
	EPropAdjIdleData *data = (EPropAdjIdleData *) p;
	GladeEPropAdjustment *eprop_adj = GLADE_EPROP_ADJUSTMENT (data->eprop);
	
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (eprop_adj->value), data->value);
	
	g_free (p);
	
	return FALSE;
}

static void
glade_eprop_adj_value_changed (GtkAdjustment *adj, GladeEditorProperty *eprop)
{
	EPropAdjIdleData *data;
	
	g_signal_handlers_disconnect_by_func (adj, glade_eprop_adj_value_changed, eprop);	

	/* Don`t do anything if the loaded property is not the same */
	if (adj != g_value_get_object (eprop->property->value)) return;
	
	data = g_new (EPropAdjIdleData, 1);
	
	data->eprop = eprop;
	data->value = adj->value;
	
	/* Update GladeEPropAdjustment value spinbutton in an idle funtion */
	g_idle_add (glade_eprop_adj_set_value_idle, data);
		
	/* Set adjustment to the old value */
	adj->value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (
					GLADE_EPROP_ADJUSTMENT (eprop)->value));
}

static void
glade_eprop_adjustment_load (GladeEditorProperty *eprop, GladeProperty *property)
{
	GladeEPropAdjustment *eprop_adj = GLADE_EPROP_ADJUSTMENT (eprop);
	GObject *object;
	GtkAdjustment *adj;
	
	/* Chain up first */
	editor_property_class->load (eprop, property);

	if (property == NULL) return;
	
	object = g_value_get_object (property->value);
	if (object == NULL) return;

	adj = GTK_ADJUSTMENT (object);
	
	/* Keep track of external adjustment changes */
	g_signal_connect (object, "value-changed",
			  G_CALLBACK (glade_eprop_adj_value_changed),
			  eprop);
	
	/* Update adjustment's values */
	eprop_adj->value_adj->lower = adj->lower;
	eprop_adj->value_adj->upper = adj->upper;
	eprop_adj->value_adj->step_increment = adj->step_increment;
	eprop_adj->value_adj->page_increment = adj->page_increment;
	eprop_adj->value_adj->page_size = adj->page_size;
	
	/* Block Handlers */
	g_signal_handler_block (eprop_adj->value, eprop_adj->ids.value);
	g_signal_handler_block (eprop_adj->lower, eprop_adj->ids.lower);
	g_signal_handler_block (eprop_adj->upper, eprop_adj->ids.upper);
	g_signal_handler_block (eprop_adj->step_increment, eprop_adj->ids.step_increment);
	g_signal_handler_block (eprop_adj->page_increment, eprop_adj->ids.page_increment);
	g_signal_handler_block (eprop_adj->page_size, eprop_adj->ids.page_size);
	
	/* Update spinbuttons values */
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (eprop_adj->value), adj->value);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (eprop_adj->lower), adj->lower);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (eprop_adj->upper), adj->upper);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (eprop_adj->step_increment), adj->step_increment);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (eprop_adj->page_increment), adj->page_increment);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (eprop_adj->page_size), adj->page_size);
	
	/* Unblock Handlers */
	g_signal_handler_unblock (eprop_adj->value, eprop_adj->ids.value);
	g_signal_handler_unblock (eprop_adj->lower, eprop_adj->ids.lower);
	g_signal_handler_unblock (eprop_adj->upper, eprop_adj->ids.upper);
	g_signal_handler_unblock (eprop_adj->step_increment, eprop_adj->ids.step_increment);
	g_signal_handler_unblock (eprop_adj->page_increment, eprop_adj->ids.page_increment);
	g_signal_handler_unblock (eprop_adj->page_size, eprop_adj->ids.page_size);
}

static GtkAdjustment *
glade_eprop_adjustment_dup_adj (GladeEditorProperty *eprop)
{
	GtkAdjustment *adj;
	GObject *object;
	
	object = g_value_get_object (eprop->property->value);
	if (object == NULL) return NULL;
	
	adj = GTK_ADJUSTMENT (object);

	return GTK_ADJUSTMENT (gtk_adjustment_new (adj->value,
						   adj->lower,
						   adj->upper,
						   adj->step_increment,
						   adj->page_increment,
						   adj->page_size));
}

static void
glade_eprop_adjustment_prop_changed_common (GladeEditorProperty *eprop, 
					    GtkAdjustment *adjustment)
{
	GValue value = {0, };
	
	g_value_init (&value, GTK_TYPE_ADJUSTMENT);
	g_value_set_object (&value, G_OBJECT (adjustment));

	glade_editor_property_commit (eprop, &value);

	g_value_unset (&value);
}

#define GLADE_EPROP_ADJUSTMENT_DEFINE_VALUE_CHANGED_FUNC(p)		\
static void								\
glade_eprop_adjustment_ ## p ## _changed (GtkSpinButton *spin,		\
					  GladeEditorProperty *eprop)	\
{									\
	GtkAdjustment *adj = glade_eprop_adjustment_dup_adj (eprop);	\
	if (adj == NULL) return;					\
	adj->p = gtk_spin_button_get_value (spin);			\
	glade_eprop_adjustment_prop_changed_common (eprop, adj);	\
}

GLADE_EPROP_ADJUSTMENT_DEFINE_VALUE_CHANGED_FUNC (value)
GLADE_EPROP_ADJUSTMENT_DEFINE_VALUE_CHANGED_FUNC (lower)
GLADE_EPROP_ADJUSTMENT_DEFINE_VALUE_CHANGED_FUNC (upper)
GLADE_EPROP_ADJUSTMENT_DEFINE_VALUE_CHANGED_FUNC (step_increment)
GLADE_EPROP_ADJUSTMENT_DEFINE_VALUE_CHANGED_FUNC (page_increment)
GLADE_EPROP_ADJUSTMENT_DEFINE_VALUE_CHANGED_FUNC (page_size)

#define GLADE_EPROP_ADJUSTMENT_CONNECT(object, prop) \
g_signal_connect (object, "value_changed", \
G_CALLBACK (glade_eprop_adjustment_ ## prop ## _changed), eprop);

static void
glade_eprop_adjustment_table_add_label (GtkTable *table,
					gint pos,
					gchar *label,
					gchar *tip)
{
	GtkWidget *widget;
	
	widget = gtk_label_new (label);
	gtk_misc_set_alignment (GTK_MISC (widget), 1, 0);

	gtk_widget_set_tooltip_text (widget, tip);
	
	gtk_table_attach_defaults (table, widget, 0, 1, pos, pos + 1);
}

static GtkWidget *
glade_eprop_adjustment_create_input (GladeEditorProperty *eprop)
{
	GladeEPropAdjustment *eprop_adj = GLADE_EPROP_ADJUSTMENT (eprop);
	GtkWidget *widget;
	GtkTable *table;
	
	eprop_adj->value = gtk_spin_button_new_with_range (-G_MAXDOUBLE, G_MAXDOUBLE, 1);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (eprop_adj->value), 2);
	eprop_adj->ids.value = GLADE_EPROP_ADJUSTMENT_CONNECT (eprop_adj->value, value);
	eprop_adj->value_adj = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (eprop_adj->value));
	
	eprop_adj->lower = gtk_spin_button_new_with_range (-G_MAXDOUBLE, G_MAXDOUBLE, 1);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (eprop_adj->lower), 2);
	eprop_adj->ids.lower = GLADE_EPROP_ADJUSTMENT_CONNECT (eprop_adj->lower, lower);
	
	eprop_adj->upper = gtk_spin_button_new_with_range (-G_MAXDOUBLE, G_MAXDOUBLE, 1);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (eprop_adj->upper), 2);
	eprop_adj->ids.upper = GLADE_EPROP_ADJUSTMENT_CONNECT (eprop_adj->upper, upper);
	
	eprop_adj->step_increment = gtk_spin_button_new_with_range (0, G_MAXDOUBLE, 1);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (eprop_adj->step_increment), 2);
	eprop_adj->ids.step_increment = GLADE_EPROP_ADJUSTMENT_CONNECT (eprop_adj->step_increment, step_increment);
	
	eprop_adj->page_increment = gtk_spin_button_new_with_range (0, G_MAXDOUBLE, 1);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (eprop_adj->page_increment), 2);
	eprop_adj->ids.page_increment = GLADE_EPROP_ADJUSTMENT_CONNECT (eprop_adj->page_increment, page_increment);

	eprop_adj->page_size = gtk_spin_button_new_with_range (0, G_MAXDOUBLE, 1);
	gtk_spin_button_set_digits (GTK_SPIN_BUTTON (eprop_adj->page_size), 2);
	eprop_adj->ids.page_size = GLADE_EPROP_ADJUSTMENT_CONNECT (eprop_adj->page_size, page_size);

	/* Eprop */
	widget = gtk_table_new (6, 2, FALSE);
	table = GTK_TABLE (widget);
	gtk_table_set_col_spacings (table, 4);
	
	glade_eprop_adjustment_table_add_label (table, 0, _("Value:"),
						_("The current value"));

	glade_eprop_adjustment_table_add_label (table, 1, _("Lower:"),
						_("The minimum value"));
	
	glade_eprop_adjustment_table_add_label (table, 2, _("Upper:"),
						_("The maximum value"));
			
	glade_eprop_adjustment_table_add_label (table, 3, _("Step inc:"),
		_("The increment to use to make minor changes to the value"));
	
	glade_eprop_adjustment_table_add_label (table, 4, _("Page inc:"),
		_("The increment to use to make major changes to the value"));
	
	glade_eprop_adjustment_table_add_label (table, 5, _("Page size:"),
		_("The page size (in a GtkScrollbar this is the size of the area which is currently visible)"));

	gtk_table_attach_defaults (table, eprop_adj->value,          1, 2, 0, 1);
	gtk_table_attach_defaults (table, eprop_adj->lower,          1, 2, 1, 2);
	gtk_table_attach_defaults (table, eprop_adj->upper,          1, 2, 2, 3);
	gtk_table_attach_defaults (table, eprop_adj->step_increment, 1, 2, 3, 4);
	gtk_table_attach_defaults (table, eprop_adj->page_increment, 1, 2, 4, 5);
	gtk_table_attach_defaults (table, eprop_adj->page_size,      1, 2, 5, 6);

	gtk_widget_show_all (widget);

	return widget;
}


/*******************************************************************************
                        GladeEditorPropertyAccelClass
 *******************************************************************************/
enum {
	ACCEL_COLUMN_SIGNAL = 0,
	ACCEL_COLUMN_REAL_SIGNAL,
	ACCEL_COLUMN_KEY,
	ACCEL_COLUMN_MOD_SHIFT,
	ACCEL_COLUMN_MOD_CNTL,
	ACCEL_COLUMN_MOD_ALT,
	ACCEL_COLUMN_IS_CLASS,
	ACCEL_COLUMN_IS_SIGNAL,
	ACCEL_COLUMN_KEY_ENTERED,
	ACCEL_COLUMN_KEY_SLOT,
	ACCEL_NUM_COLUMNS
};

enum {
	ACCEL_COMBO_COLUMN_TEXT = 0,
	ACCEL_COMBO_NUM_COLUMNS,
};

typedef struct {
	GladeEditorProperty parent_instance;
	
	GtkWidget    *entry;
	GList        *parent_iters;
	GtkTreeModel *model;
} GladeEPropAccel;

typedef struct {
	GtkTreeIter *iter;
	gchar       *name; /* <-- dont free */
} GladeEpropIterTab;


static GtkTreeModel *keysyms_model   = NULL;

GLADE_MAKE_EPROP (GladeEPropAccel, glade_eprop_accel)
#define GLADE_TYPE_EPROP_ACCEL            (glade_eprop_accel_get_type())
#define GLADE_EPROP_ACCEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_ACCEL, GladeEPropAccel))
#define GLADE_EPROP_ACCEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_ACCEL, GladeEPropAccelClass))
#define GLADE_IS_EPROP_ACCEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_ACCEL))
#define GLADE_IS_EPROP_ACCEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_ACCEL))
#define GLADE_EPROP_ACCEL_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_ACCEL, GladeEPropAccelClass))


static GtkTreeModel *
create_keysyms_model (void)
{
	GtkTreeModel *model;
	GtkTreeIter   iter, alphanum, fkey, keypad, other, extra;
	GtkTreeIter  *parent;
	gint          i;

	model = (GtkTreeModel *)gtk_tree_store_new
		(ACCEL_COMBO_NUM_COLUMNS,
		 G_TYPE_STRING);       /* The Key charachter name */

	gtk_tree_store_append (GTK_TREE_STORE (model), &alphanum, NULL);
	gtk_tree_store_set    
		(GTK_TREE_STORE (model), &alphanum, 
		 ACCEL_COMBO_COLUMN_TEXT, _("Alphanumerical"), -1);

	gtk_tree_store_append (GTK_TREE_STORE (model), &extra, NULL);
	gtk_tree_store_set    
		(GTK_TREE_STORE (model), &extra, 
		 ACCEL_COMBO_COLUMN_TEXT, _("Extra"), -1);

	gtk_tree_store_append (GTK_TREE_STORE (model), &keypad, NULL);
	gtk_tree_store_set    
		(GTK_TREE_STORE (model), &keypad, 
		 ACCEL_COMBO_COLUMN_TEXT, _("Keypad"), -1);

	gtk_tree_store_append (GTK_TREE_STORE (model), &fkey, NULL);
	gtk_tree_store_set    
		(GTK_TREE_STORE (model), &fkey, 
		 ACCEL_COMBO_COLUMN_TEXT, _("Functions"), -1);
	
	gtk_tree_store_append (GTK_TREE_STORE (model), &other, NULL);
	gtk_tree_store_set    
		(GTK_TREE_STORE (model), &other, 
		 ACCEL_COMBO_COLUMN_TEXT, _("Other"), -1);

	parent = &alphanum;

	for (i = 0; GladeKeys[i].name != NULL; i++)
	{
		gtk_tree_store_append (GTK_TREE_STORE (model), &iter, parent);
		gtk_tree_store_set    
			(GTK_TREE_STORE (model), &iter, 
			 ACCEL_COMBO_COLUMN_TEXT, GladeKeys[i].name, -1);

		if (!strcmp (GladeKeys[i].name, GLADE_KEYS_LAST_ALPHANUM))
			parent = &extra;
		else if (!strcmp (GladeKeys[i].name, GLADE_KEYS_LAST_EXTRA))
			parent = &keypad;
		else if (!strcmp (GladeKeys[i].name, GLADE_KEYS_LAST_KP))
			parent = &fkey;
		else if (!strcmp (GladeKeys[i].name, GLADE_KEYS_LAST_FKEY))
			parent = &other;
	}
	return model;
}

static void
glade_eprop_accel_finalize (GObject *object)
{
	/* Chain up */
	G_OBJECT_CLASS (editor_property_class)->finalize (object);
}

static void
glade_eprop_accel_load (GladeEditorProperty *eprop, 
			GladeProperty       *property)
{
	GladeEPropAccel *eprop_accel = GLADE_EPROP_ACCEL (eprop);
	gchar           *accels;

	/* Chain up first */
	editor_property_class->load (eprop, property);

	if (property == NULL) return;

	if ((accels  = glade_property_class_make_string_from_gvalue
	     (eprop->klass, property->value)) != NULL)
	{
		gtk_entry_set_text (GTK_ENTRY (eprop_accel->entry), accels);
		g_free (accels);
	}
	else
		gtk_entry_set_text (GTK_ENTRY (eprop_accel->entry), "");

}

static gint
eprop_find_iter (GladeEpropIterTab *iter_tab,
		 gchar             *name)
{
	return strcmp (iter_tab->name, name);
}

static void
iter_tab_free (GladeEpropIterTab *iter_tab)
{
	gtk_tree_iter_free (iter_tab->iter);
	g_free (iter_tab);
}

static void
glade_eprop_accel_populate_view (GladeEditorProperty *eprop,
				 GtkTreeView         *view)
{
	GladeEPropAccel    *eprop_accel = GLADE_EPROP_ACCEL (eprop);
	GladeSignalClass   *sclass;
	GladeWidgetAdaptor *adaptor = glade_widget_adaptor_from_pclass (eprop->klass);
	GtkTreeStore       *model = (GtkTreeStore *)gtk_tree_view_get_model (view);
	GtkTreeIter         iter;
	GladeEpropIterTab  *parent_tab;
	GladeAccelInfo     *info;
	GList              *list, *l, *found, *accelerators;
	gchar              *name;

	accelerators = g_value_get_boxed (eprop->property->value);

	/* First make parent iters...
	 */
	for (list = adaptor->signals; list; list = list->next)
	{
		sclass = list->data;

		/* Only action signals have accelerators. */
		if ((sclass->query.signal_flags & G_SIGNAL_ACTION) == 0)
			continue;

		if (g_list_find_custom (eprop_accel->parent_iters, 
					sclass->type,
					(GCompareFunc)eprop_find_iter) == NULL)
		{
			gtk_tree_store_append (model, &iter, NULL);
			gtk_tree_store_set (model, &iter,
					    ACCEL_COLUMN_SIGNAL, sclass->type,
					    ACCEL_COLUMN_IS_CLASS, TRUE,
					    ACCEL_COLUMN_IS_SIGNAL, FALSE,
					    -1);
			
			parent_tab = g_new0 (GladeEpropIterTab, 1);
			parent_tab->name = sclass->type;
			parent_tab->iter = gtk_tree_iter_copy (&iter);

			eprop_accel->parent_iters = 
				g_list_prepend (eprop_accel->parent_iters,
						parent_tab);
		}
	}

	/* Now we populate...
	 */
	for (list = adaptor->signals; list; list = list->next)
	{
		sclass = list->data;

		/* Only action signals have accelerators. */
		if ((sclass->query.signal_flags & G_SIGNAL_ACTION) == 0)
			continue;

		if ((found = g_list_find_custom (eprop_accel->parent_iters, 
						 sclass->type,
						 (GCompareFunc)eprop_find_iter)) != NULL)
		{
			parent_tab = found->data;
			name       = g_strdup_printf ("    %s", sclass->name);

			/* Populate from accelerator list
			 */
			for (l = accelerators; l; l = l->next)
			{
				info = l->data;

				if (strcmp (info->signal, sclass->name))
					continue;

				gtk_tree_store_append (model, &iter, parent_tab->iter);
				gtk_tree_store_set    
					(model, &iter,
					 ACCEL_COLUMN_SIGNAL, name,
					 ACCEL_COLUMN_REAL_SIGNAL, sclass->name,
					 ACCEL_COLUMN_IS_CLASS, FALSE,
					 ACCEL_COLUMN_IS_SIGNAL, TRUE,
					 ACCEL_COLUMN_MOD_SHIFT, 
					 (info->modifiers & GDK_SHIFT_MASK) != 0,
					 ACCEL_COLUMN_MOD_CNTL, 
					 (info->modifiers & GDK_CONTROL_MASK) != 0,
					 ACCEL_COLUMN_MOD_ALT,
					 (info->modifiers & GDK_MOD1_MASK) != 0,
					 ACCEL_COLUMN_KEY, 
					 glade_builtin_string_from_key (info->key),
					 ACCEL_COLUMN_KEY_ENTERED, TRUE,
					 ACCEL_COLUMN_KEY_SLOT, FALSE,
					 -1);
			}

			/* Append a new empty slot at the end */
			gtk_tree_store_append (model, &iter, parent_tab->iter);
			gtk_tree_store_set    
				(model, &iter,
				 ACCEL_COLUMN_SIGNAL, name,
				 ACCEL_COLUMN_REAL_SIGNAL, sclass->name,
				 ACCEL_COLUMN_IS_CLASS, FALSE,
				 ACCEL_COLUMN_IS_SIGNAL, TRUE,
				 ACCEL_COLUMN_MOD_SHIFT, FALSE,
				 ACCEL_COLUMN_MOD_CNTL, FALSE,
				 ACCEL_COLUMN_MOD_ALT, FALSE,
				 ACCEL_COLUMN_KEY, _("<choose a key>"),
				 ACCEL_COLUMN_KEY_ENTERED, FALSE,
				 ACCEL_COLUMN_KEY_SLOT, TRUE,
				 -1);

			g_free (name);
		}
	}
}

static void
key_edited (GtkCellRendererText *cell,
	    const gchar         *path_string,
	    const gchar         *new_text,
	    GladeEditorProperty *eprop)
{
	GladeEPropAccel *eprop_accel = GLADE_EPROP_ACCEL (eprop);
	gboolean         key_was_set;
	const gchar     *text;
	GtkTreeIter      iter, parent_iter, new_iter;

	if (!gtk_tree_model_get_iter_from_string (eprop_accel->model,
						  &iter, path_string))
		return;

	gtk_tree_model_get (eprop_accel->model, &iter,
			    ACCEL_COLUMN_KEY_ENTERED, &key_was_set,
			    -1);

	/* If user selects "none"; remove old entry or ignore new one.
	 */
	if (!new_text || new_text[0] == '\0' ||
	    glade_builtin_string_from_key ((guint)new_text[0]) == NULL ||
	    g_utf8_collate (new_text, _("None")) == 0 ||
	    g_utf8_collate (new_text, _("<choose a key>")) == 0)
	{
		if (key_was_set)
			gtk_tree_store_remove
				(GTK_TREE_STORE (eprop_accel->model), &iter);

		return;
	}

	if (glade_builtin_key_from_string (new_text) != 0)
		text = new_text;
	else
		text = glade_builtin_string_from_key ((guint)new_text[0]);

	gtk_tree_store_set    
		(GTK_TREE_STORE (eprop_accel->model), &iter,
		 ACCEL_COLUMN_KEY, text,
		 ACCEL_COLUMN_KEY_ENTERED, TRUE,
		 ACCEL_COLUMN_KEY_SLOT, FALSE,
		 -1);

	/* Append a new one if needed
	 */
	if (key_was_set == FALSE &&
	    gtk_tree_model_iter_parent (eprop_accel->model,
					&parent_iter, &iter))
	{	
		gchar *signal, *real_signal;
		
		gtk_tree_model_get (eprop_accel->model, &iter,
				    ACCEL_COLUMN_SIGNAL, &signal,
				    ACCEL_COLUMN_REAL_SIGNAL, &real_signal,
				    -1);
		
		/* Append a new empty slot at the end */
		gtk_tree_store_insert_after (GTK_TREE_STORE (eprop_accel->model), 
					     &new_iter, &parent_iter, &iter);
		gtk_tree_store_set (GTK_TREE_STORE (eprop_accel->model), &new_iter,
				    ACCEL_COLUMN_SIGNAL, signal,
				    ACCEL_COLUMN_REAL_SIGNAL, real_signal,
				    ACCEL_COLUMN_IS_CLASS, FALSE,
				    ACCEL_COLUMN_IS_SIGNAL, TRUE,
				    ACCEL_COLUMN_MOD_SHIFT, FALSE,
				    ACCEL_COLUMN_MOD_CNTL, FALSE,
				    ACCEL_COLUMN_MOD_ALT, FALSE,
				    ACCEL_COLUMN_KEY, _("<choose a key>"),
				    ACCEL_COLUMN_KEY_ENTERED, FALSE,
				    ACCEL_COLUMN_KEY_SLOT, TRUE,
				    -1);
		g_free (signal);
		g_free (real_signal);
	}
}

static void
modifier_toggled (GtkCellRendererToggle *cell,
		  gchar                 *path_string,
		  GladeEditorProperty   *eprop)
{
	GladeEPropAccel   *eprop_accel = GLADE_EPROP_ACCEL (eprop);
	GtkTreeIter        iter;
	gint               column;
	gboolean           active, key_entered;

	if (!gtk_tree_model_get_iter_from_string (eprop_accel->model,
						  &iter, path_string))
		return;

	column = GPOINTER_TO_INT (g_object_get_data
				  (G_OBJECT (cell), "model-column"));

	gtk_tree_model_get
		(eprop_accel->model, &iter,
		 ACCEL_COLUMN_KEY_ENTERED, &key_entered,
		 column, &active, -1);

	if (key_entered)
		gtk_tree_store_set
			(GTK_TREE_STORE (eprop_accel->model), &iter,
			 column, !active, -1);
}


static GtkWidget *
glade_eprop_accel_view (GladeEditorProperty *eprop)
{
	GladeEPropAccel   *eprop_accel = GLADE_EPROP_ACCEL (eprop);
	GtkWidget         *view_widget;
 	GtkCellRenderer   *renderer;
	GtkTreeViewColumn *column;

	eprop_accel->model = (GtkTreeModel *)gtk_tree_store_new
		(ACCEL_NUM_COLUMNS,
		 G_TYPE_STRING,        /* The GSignal name formatted for display */
		 G_TYPE_STRING,        /* The GSignal name */
		 G_TYPE_STRING,        /* The Gdk keycode */
		 G_TYPE_BOOLEAN,       /* The shift modifier */
		 G_TYPE_BOOLEAN,       /* The cntl modifier */
		 G_TYPE_BOOLEAN,       /* The alt modifier */
		 G_TYPE_BOOLEAN,       /* Whether this is a class entry */
		 G_TYPE_BOOLEAN,       /* Whether this is a signal entry (oposite of above) */
		 G_TYPE_BOOLEAN,       /* Whether the key has been entered for this row */
		 G_TYPE_BOOLEAN);      /* Oposite of above */

	view_widget = gtk_tree_view_new_with_model (eprop_accel->model);
	g_object_set (G_OBJECT (view_widget), "enable-search", FALSE, NULL);
	
	/********************* fake invisible column *********************/
 	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), "editable", FALSE, "visible", FALSE, NULL);

	column = gtk_tree_view_column_new_with_attributes (NULL, renderer, NULL);
 	gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

	gtk_tree_view_column_set_visible (column, FALSE);
	gtk_tree_view_set_expander_column (GTK_TREE_VIEW (view_widget), column);

	/********************* signal name column *********************/
 	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), 
		      "editable", FALSE, 
		      "weight", PANGO_WEIGHT_BOLD,
		      NULL);

	column = gtk_tree_view_column_new_with_attributes
		(_("Signal"),  renderer, 
		 "text", ACCEL_COLUMN_SIGNAL, 
		 "weight-set", ACCEL_COLUMN_IS_CLASS,
		 NULL);

	g_object_set (G_OBJECT (column), "expand", TRUE, NULL);

 	gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

	/********************* key name column *********************/
	if (keysyms_model == NULL)
		keysyms_model = create_keysyms_model ();

 	renderer = gtk_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer), 
		      "editable",    TRUE,
		      "model",       keysyms_model,
		      "text-column", ACCEL_COMBO_COLUMN_TEXT,
		      "has-entry",   TRUE,
		      "style",       PANGO_STYLE_ITALIC,
		      "foreground",  "Gray", 
		      NULL);

	g_signal_connect (renderer, "edited",
			  G_CALLBACK (key_edited), eprop);

	column = gtk_tree_view_column_new_with_attributes
		(_("Key"),         renderer, 
		 "text",           ACCEL_COLUMN_KEY,
		 "style-set",      ACCEL_COLUMN_KEY_SLOT,
		 "foreground-set", ACCEL_COLUMN_KEY_SLOT,
 		 "visible",        ACCEL_COLUMN_IS_SIGNAL,
		 NULL);

	g_object_set (G_OBJECT (column), "expand", TRUE, NULL);

 	gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

	/********************* shift modifier column *********************/
 	renderer = gtk_cell_renderer_toggle_new ();
	column   = gtk_tree_view_column_new_with_attributes
		(_("Shift"),  renderer, 
		 "visible", ACCEL_COLUMN_IS_SIGNAL,
		 "sensitive", ACCEL_COLUMN_KEY_ENTERED,
		 "active", ACCEL_COLUMN_MOD_SHIFT,
		 NULL);

	g_object_set_data (G_OBJECT (renderer), "model-column",
			   GINT_TO_POINTER (ACCEL_COLUMN_MOD_SHIFT));
	g_signal_connect (G_OBJECT (renderer), "toggled",
			  G_CALLBACK (modifier_toggled), eprop);

 	gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

	/********************* control modifier column *********************/
 	renderer = gtk_cell_renderer_toggle_new ();
	column   = gtk_tree_view_column_new_with_attributes
		(_("Control"),  renderer, 
		 "visible", ACCEL_COLUMN_IS_SIGNAL,
		 "sensitive", ACCEL_COLUMN_KEY_ENTERED,
		 "active", ACCEL_COLUMN_MOD_CNTL,
		 NULL);

	g_object_set_data (G_OBJECT (renderer), "model-column",
			   GINT_TO_POINTER (ACCEL_COLUMN_MOD_CNTL));
	g_signal_connect (G_OBJECT (renderer), "toggled",
			  G_CALLBACK (modifier_toggled), eprop);

 	gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

	/********************* alt modifier column *********************/
 	renderer = gtk_cell_renderer_toggle_new ();
	column   = gtk_tree_view_column_new_with_attributes
		(_("Alt"),  renderer, 
		 "visible", ACCEL_COLUMN_IS_SIGNAL,
		 "sensitive", ACCEL_COLUMN_KEY_ENTERED,
		 "active", ACCEL_COLUMN_MOD_ALT,
		 NULL);

	g_object_set_data (G_OBJECT (renderer), "model-column",
			   GINT_TO_POINTER (ACCEL_COLUMN_MOD_ALT));
	g_signal_connect (G_OBJECT (renderer), "toggled",
			  G_CALLBACK (modifier_toggled), eprop);

 	gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

	return view_widget;
}

static gboolean
glade_eprop_accel_accum_accelerators (GtkTreeModel  *model,
				      GtkTreePath    *path,
				      GtkTreeIter    *iter,
				      GList         **ret)
{
	GladeAccelInfo *info;
	gchar          *signal, *key_str;
	gboolean        shift, cntl, alt, entered;

	gtk_tree_model_get (model, iter, ACCEL_COLUMN_KEY_ENTERED, &entered, -1);
	if (entered == FALSE) return FALSE;
	
	gtk_tree_model_get (model, iter,
			    ACCEL_COLUMN_REAL_SIGNAL, &signal,
			    ACCEL_COLUMN_KEY,         &key_str,
			    ACCEL_COLUMN_MOD_SHIFT,   &shift,
			    ACCEL_COLUMN_MOD_CNTL,    &cntl,
			    ACCEL_COLUMN_MOD_ALT,     &alt,
			    -1);

	info            = g_new0 (GladeAccelInfo, 1);
	info->signal    = signal;
	info->key       = glade_builtin_key_from_string (key_str);
	info->modifiers = (shift ? GDK_SHIFT_MASK   : 0) |
			  (cntl  ? GDK_CONTROL_MASK : 0) |
			  (alt   ? GDK_MOD1_MASK    : 0);

	*ret = g_list_prepend (*ret, info);

	g_free (key_str);
	
	return FALSE;
}


static void
glade_eprop_accel_show_dialog (GtkWidget           *dialog_button,
			       GladeEditorProperty *eprop)
{
	GladeEPropAccel  *eprop_accel = GLADE_EPROP_ACCEL (eprop);
	GtkWidget        *dialog, *parent, *vbox, *sw, *tree_view;
	GladeProject     *project;
	GValue           *value;
	GList            *accelerators = NULL;
	gint              res;
	
	project = glade_widget_get_project (eprop->property->widget);
	parent = gtk_widget_get_toplevel (GTK_WIDGET (eprop));

	dialog = gtk_dialog_new_with_buttons (_("Choose accelerator keys..."),
					      GTK_WINDOW (parent),
					      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CLEAR, GLADE_RESPONSE_CLEAR,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 0);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (sw);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	gtk_widget_set_size_request (sw, 400, 200);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);

	tree_view = glade_eprop_accel_view (eprop);
	glade_eprop_accel_populate_view (eprop, GTK_TREE_VIEW (tree_view));

	gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

	gtk_widget_show (tree_view);
	gtk_container_add (GTK_CONTAINER (sw), tree_view);
	
	/* Run the dialog */
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_OK) 
	{
		gtk_tree_model_foreach
			(gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view)),
			 (GtkTreeModelForeachFunc)
			 glade_eprop_accel_accum_accelerators, &accelerators);
		
		value = g_new0 (GValue, 1);
		g_value_init (value, GLADE_TYPE_ACCEL_GLIST);
		g_value_take_boxed (value, accelerators);

		glade_editor_property_commit (eprop, value);

		g_value_unset (value);
		g_free (value);
	} 
	else if (res == GLADE_RESPONSE_CLEAR)
	{
		value = g_new0 (GValue, 1);
		g_value_init (value, GLADE_TYPE_ACCEL_GLIST);
		g_value_set_boxed (value, NULL);

		glade_editor_property_commit (eprop, value);

		g_value_unset (value);
		g_free (value);
	}

	/* Clean up ...
	 */
	gtk_widget_destroy (dialog);

	g_object_unref (G_OBJECT (eprop_accel->model));
	eprop_accel->model = NULL;

	if (eprop_accel->parent_iters)
	{
		g_list_foreach (eprop_accel->parent_iters, (GFunc)iter_tab_free, NULL);
		g_list_free (eprop_accel->parent_iters);
		eprop_accel->parent_iters = NULL;
	}

}

static GtkWidget *
glade_eprop_accel_create_input (GladeEditorProperty *eprop)
{
	GladeEPropAccel *eprop_accel = GLADE_EPROP_ACCEL (eprop);
	GtkWidget        *hbox;
	GtkWidget        *button;

	hbox               = gtk_hbox_new (FALSE, 0);
	eprop_accel->entry = gtk_entry_new ();
	gtk_entry_set_editable (GTK_ENTRY (eprop_accel->entry), FALSE);
	gtk_widget_show (eprop_accel->entry);
	gtk_box_pack_start (GTK_BOX (hbox), eprop_accel->entry, TRUE, TRUE, 0);

	button = gtk_button_new_with_label ("...");
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), button,  FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_eprop_accel_show_dialog), 
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
	else if (G_IS_PARAM_SPEC_VALUE_ARRAY (pspec))
	{
		if (pspec->value_type == G_TYPE_VALUE_ARRAY)
			type = GLADE_TYPE_EPROP_TEXT;
	}
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
		if (pspec->value_type == GDK_TYPE_PIXBUF)
			type = GLADE_TYPE_EPROP_RESOURCE;
		else if (pspec->value_type == GTK_TYPE_ADJUSTMENT)
			type = GLADE_TYPE_EPROP_ADJUSTMENT;
		else
			type = GLADE_TYPE_EPROP_OBJECT;
	}
	else if (GLADE_IS_PARAM_SPEC_OBJECTS(pspec))
		type = GLADE_TYPE_EPROP_OBJECTS;
	else if (GLADE_IS_PARAM_SPEC_ACCEL(pspec))
		type = GLADE_TYPE_EPROP_ACCEL;

	return type;
}

/*******************************************************************************
                                     API
 *******************************************************************************/

/**
 * glade_editor_property_new:
 * @klass: A #GladePropertyClass
 * @use_command: Whether the undo/redo stack applies here.
 *
 * This is a factory function to create the correct type of
 * editor property that can edit the said type of #GladePropertyClass
 *
 * Returns: A newly created GladeEditorProperty of the correct type
 */
GladeEditorProperty *
glade_editor_property_new (GladePropertyClass  *klass,
			   gboolean             use_command)
{
	GladeEditorProperty *eprop;
	GType                type = 0;

	/* Find the right type of GladeEditorProperty for this
	 * GladePropertyClass.
	 */
	if ((type = glade_editor_property_type (klass->pspec)) == 0)
		g_error ("%s : pspec '%s' type '%s' not implemented (%s)\n", 
			 G_GNUC_PRETTY_FUNCTION, 
			 klass->name, 
			 g_type_name (G_PARAM_SPEC_TYPE (klass->pspec)),
			 g_type_name (klass->pspec->value_type));

	/* special case for resource specs which are hand specified in the catalog. */
	if (klass->resource)
		type = GLADE_TYPE_EPROP_RESOURCE;
	
	/* special case for string specs that denote themed application icons. */
	if (klass->themed_icon)
		type = GLADE_TYPE_EPROP_NAMED_ICON;

	/* Create and return the correct type of GladeEditorProperty */
	eprop = g_object_new (type,
			      "property-class", klass, 
			      "use-command", use_command,
			      NULL);

	return eprop;
}

/**
 * glade_editor_property_new_from_widget:
 * @widget: A #GladeWidget
 * @property: The widget's property
 * @packing: whether @property indicates a packing property or not.
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
				       gboolean     packing,
				       gboolean     use_command)
{
	GladeEditorProperty *eprop;
	GladeProperty *p;
	
	if (packing)
		p = glade_widget_get_pack_property (widget, property);
	else
		p = glade_widget_get_property (widget, property);
	g_return_val_if_fail (GLADE_IS_PROPERTY (p), NULL);

	eprop = glade_editor_property_new (p->klass, use_command);
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
		/* properties are allowed to be missing on some internal widgets */
		property = glade_widget_get_property (widget, eprop->klass->id);

	glade_editor_property_load (eprop, property);
}

/**
 * glade_editor_property_show_info:
 * @eprop: A #GladeEditorProperty
 *
 * Show the control widget to access help for @eprop
 */
void
glade_editor_property_show_info (GladeEditorProperty *eprop)
{
	GladeWidgetAdaptor *adaptor;
	gchar              *book;

	g_return_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop));

	adaptor = glade_widget_adaptor_from_pspec (eprop->klass->pspec);

	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));

	g_object_get (adaptor, "book", &book, NULL);

	if (eprop->klass->virt == FALSE &&
	    book                  != NULL)
		gtk_widget_show (eprop->info);
	else
	{
		/* Put insensitive controls to balance the UI with
		 * other eprops.
		 */
		gtk_widget_show (eprop->info);
		gtk_widget_set_sensitive (eprop->info, FALSE);
	}

	g_free (book);
	eprop->show_info = TRUE;
	g_object_notify (G_OBJECT (eprop), "show-info");
}

/**
 * glade_editor_property_hide_info:
 * @eprop: A #GladeEditorProperty
 *
 * Hide the control widget to access help for @eprop
 */
void
glade_editor_property_hide_info (GladeEditorProperty *eprop)
{
	g_return_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop));

	gtk_widget_hide (eprop->info);
	
	eprop->show_info = FALSE;
	g_object_notify (G_OBJECT (eprop), "show-info");
}
