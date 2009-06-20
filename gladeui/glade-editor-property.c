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
#include "glade-popup.h"
#include "glade-builtins.h"
#include "glade-marshallers.h"
#include "glade-displayable-values.h"
#include "glade-named-icon-chooser-dialog.h"

enum {
	PROP_0,
	PROP_PROPERTY_CLASS,
	PROP_USE_COMMAND
};

enum {
	CHANGED,
	COMMIT,
	LAST_SIGNAL
};

static GtkTableClass             *table_class;
static GladeEditorPropertyClass  *editor_property_class;

static guint  glade_eprop_signals[LAST_SIGNAL] = { 0, };

#define GLADE_PROPERTY_TABLE_ROW_SPACING 2
#define FLAGS_COLUMN_SETTING             0
#define FLAGS_COLUMN_SYMBOL              1


/*******************************************************************************
                               GladeEditorPropertyClass
 *******************************************************************************/

/* declare this forwardly for the finalize routine */
static void glade_editor_property_load_common (GladeEditorProperty *eprop, 
					       GladeProperty       *property);

static void
glade_editor_property_commit_common (GladeEditorProperty *eprop,
				     GValue              *value)
{
	GladeProject *project;
	GladeProjectFormat fmt;

	if (eprop->use_command == FALSE)
		glade_property_set_value (eprop->property, value);
	else
		glade_command_set_property_value (eprop->property, value);

	project = glade_widget_get_project (eprop->property->widget);
	fmt     = glade_project_get_format (project);

	/* If the value was denied by a verify function, we'll have to
	 * reload the real value.
	 */
	if (glade_property_class_compare (eprop->property->klass,
					  eprop->property->value, value, fmt) != 0)
		GLADE_EDITOR_PROPERTY_GET_CLASS (eprop)->load (eprop, eprop->property);
	else
		/* publish a value change to those interested */
		g_signal_emit (G_OBJECT (eprop), glade_eprop_signals[CHANGED], 0, eprop->property);
}

void
glade_editor_property_commit_no_callback (GladeEditorProperty *eprop,
					  GValue              *value)
{
	g_return_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop));

	g_signal_handler_block (G_OBJECT (eprop->property), eprop->changed_id);
	eprop->committing = TRUE;
	glade_editor_property_commit (eprop, value);
	eprop->committing = FALSE;
	g_signal_handler_unblock (G_OBJECT (eprop->property), eprop->changed_id);
}


static void
glade_editor_property_tooltip_cb (GladeProperty       *property,
				  const gchar         *tooltip,
				  const gchar         *insensitive,
				  const gchar         *support,
				  GladeEditorProperty *eprop)
{
	const gchar *choice_tooltip;

	if (glade_property_get_sensitive (property))
		choice_tooltip = tooltip;
	else
		choice_tooltip = insensitive;

	gtk_widget_set_tooltip_text (eprop->input, choice_tooltip);
	gtk_widget_set_tooltip_text (eprop->label, choice_tooltip);
	gtk_widget_set_tooltip_text (eprop->warning, support);
}

static void
glade_editor_property_sensitivity_cb (GladeProperty       *property,
				      GParamSpec          *pspec,
				      GladeEditorProperty *eprop)
{
	gboolean sensitive = glade_property_get_sensitive (eprop->property);
	gboolean support_sensitive = (eprop->property->state & GLADE_STATE_SUPPORT_DISABLED) == 0;

        gtk_widget_set_sensitive (eprop->input, sensitive && support_sensitive && 
				  glade_property_get_enabled (property));
	if (eprop->check)
		gtk_widget_set_sensitive (eprop->check, sensitive && support_sensitive);
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
glade_editor_property_fix_label (GladeEditorProperty *eprop)
{
	gchar *text = NULL;

	if (!eprop->property)
		return;

	/* refresh label */
	if ((eprop->property->state & GLADE_STATE_CHANGED) != 0)
		text = g_strdup_printf ("<b>%s:</b>", eprop->klass->name);
	else
		text = g_strdup_printf ("%s:", eprop->klass->name);
	gtk_label_set_markup (GTK_LABEL (eprop->label), text);
	g_free (text);

	/* refresh icon */
	if ((eprop->property->state & GLADE_STATE_UNSUPPORTED) != 0)
		gtk_widget_show (eprop->warning);
	else
		gtk_widget_hide (eprop->warning);

	/* check sensitivity */
	glade_editor_property_sensitivity_cb (eprop->property, NULL, eprop);
}

static void
glade_editor_property_state_cb (GladeProperty       *property,
				GParamSpec          *pspec,
				GladeEditorProperty *eprop)
{
	glade_editor_property_fix_label (eprop);
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

		/* sensitive = enabled && support enabled && sensitive */
		if (enabled == FALSE)
			gtk_widget_set_sensitive (eprop->input, FALSE);
		else if (glade_property_get_sensitive (property) ||
			 (property->state & GLADE_STATE_SUPPORT_DISABLED) != 0)
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

static gboolean
glade_editor_property_button_pressed (GtkWidget           *widget,
				      GdkEventButton      *event,
				      GladeEditorProperty *eprop)
{
	if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
	{
		glade_popup_property_pop (eprop->property, event);
		return TRUE;
	}
	return FALSE;
}


#define EDITOR_COLUMN_SIZE 90

static void
eprop_item_label_size_request (GtkWidget *widget, GtkRequisition *requisition, 
			       GladeEditorProperty *eprop)
{
	requisition->width = EDITOR_COLUMN_SIZE;
}

static void
eprop_item_label_size_allocate_after (GtkWidget *widget, GtkAllocation *allocation,
				      GladeEditorProperty *eprop)
{
	gint width = EDITOR_COLUMN_SIZE;
	gint icon_width = 0;

	if (GTK_WIDGET_VISIBLE (eprop->warning) && GTK_WIDGET_MAPPED (eprop->warning))
	{
		GtkRequisition req = { -1, -1 };
		gtk_widget_size_request (eprop->warning, &req);
		/* Here we have to subtract the icon and remaining
		 * padding inside eprop->item_label so that we are
		 * only dealing with the size of the label.
		 * (note the '4' here comes from the hbox spacing).
		 */
		icon_width = req.width + 4;
	}

	if (allocation->width > width)
		width = allocation->width;

	gtk_widget_set_size_request (eprop->label, CLAMP (width - icon_width, 0, width), -1);
	/* Sometimes labels aren't drawn correctly after resize without this */
	gtk_widget_queue_draw (eprop->label);
}


static GObject *
glade_editor_property_constructor (GType                  type,
				   guint                  n_construct_properties,
				   GObjectConstructParam *construct_properties)
{
	GObject             *obj;
	GladeEditorProperty *eprop;
	GtkWidget           *hbox;

	/* Invoke parent constructor (eprop->klass should be resolved by this point) . */
	obj = G_OBJECT_CLASS (table_class)->constructor
		(type, n_construct_properties, construct_properties);
	
	eprop = GLADE_EDITOR_PROPERTY (obj);

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

	/* Create the warning icon */
	eprop->warning = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING, 
						   GTK_ICON_SIZE_MENU);
	gtk_widget_set_no_show_all (eprop->warning, TRUE);

	/* Create & setup label */
	eprop->item_label = gtk_event_box_new ();
	eprop->label      = gtk_label_new (NULL);
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (eprop->item_label), FALSE);

	hbox = gtk_hbox_new (FALSE, 4);

	gtk_label_set_line_wrap (GTK_LABEL(eprop->label), TRUE);
	gtk_label_set_line_wrap_mode (GTK_LABEL(eprop->label), PANGO_WRAP_WORD_CHAR);

	/* A Hack so that PANGO_WRAP_WORD_CHAR works nicely */
	g_signal_connect (G_OBJECT (eprop->item_label), "size-request",
			  G_CALLBACK (eprop_item_label_size_request), eprop);
	g_signal_connect_after (G_OBJECT (eprop->item_label), "size-allocate",
				G_CALLBACK (eprop_item_label_size_allocate_after), eprop);

	gtk_misc_set_alignment (GTK_MISC(eprop->label), 1.0, 0.5);

	gtk_box_pack_start (GTK_BOX (hbox), eprop->label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), eprop->warning, FALSE, FALSE, 0);
	gtk_container_add (GTK_CONTAINER (eprop->item_label), hbox);
	gtk_widget_show_all (eprop->item_label);

	glade_editor_property_fix_label (eprop);

	gtk_box_pack_start (GTK_BOX (eprop), eprop->input, TRUE, TRUE, 0);

	g_signal_connect (G_OBJECT (eprop->item_label), "button-press-event",
			  G_CALLBACK (glade_editor_property_button_pressed), eprop);
	g_signal_connect (G_OBJECT (eprop->input), "button-press-event",
			  G_CALLBACK (glade_editor_property_button_pressed), eprop);

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
	eprop->state_id     = 0;
	eprop->property     = NULL;

	glade_editor_property_load (eprop, NULL);
}

static void
glade_editor_property_load_common (GladeEditorProperty *eprop, 
				   GladeProperty       *property)
{
	/* NOTE THIS CODE IS FINALIZE SAFE */

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
		if (eprop->state_id > 0)
			g_signal_handler_disconnect (eprop->property, 
						     eprop->state_id);
		if (eprop->enabled_id > 0)
			g_signal_handler_disconnect (eprop->property, 
						     eprop->enabled_id);

		eprop->tooltip_id   = 0;
		eprop->sensitive_id = 0;
		eprop->changed_id   = 0;
		eprop->enabled_id   = 0;
		eprop->state_id     = 0;

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
		eprop->state_id =
			g_signal_connect (G_OBJECT (eprop->property),
					  "notify::state", 
					  G_CALLBACK (glade_editor_property_state_cb),
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
			(property, 
			 property->klass->tooltip, 
			 property->insensitive_tooltip,
			 property->support_warning,
			 eprop);
		
		/* Load initial enabled state
		 */
		glade_editor_property_enabled_cb (property, NULL, eprop);
		
		/* Load initial sensitive state.
		 */
		glade_editor_property_sensitivity_cb (property, NULL, eprop);

		/* Load intial label state
		 */
		glade_editor_property_state_cb (property, NULL, eprop);
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
	eprop_class->commit        = glade_editor_property_commit_common;
	eprop_class->create_input  = NULL;

	
	/**
	 * GladeEditorProperty::value-changed:
	 * @gladeeditorproperty: the #GladeEditorProperty which changed value
	 * @arg1: the #GladeProperty that's value changed.
	 *
	 * Emitted when a contained property changes value
	 */
	glade_eprop_signals[CHANGED] =
		g_signal_new ("value-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeEditorPropertyClass, changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, GLADE_TYPE_PROPERTY);

	/**
	 * GladeEditorProperty::commit:
	 * @gladeeditorproperty: the #GladeEditorProperty which changed value
	 * @arg1: the new #GValue to commit.
	 *
	 * Emitted when a property's value is committed, can be useful to serialize
	 * commands before and after the property's commit command from custom editors.
	 */
	glade_eprop_signals[COMMIT] =
		g_signal_new ("commit",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeEditorPropertyClass, commit),
			      NULL, NULL,
			      glade_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1, G_TYPE_POINTER);

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

	glade_editor_property_commit_no_callback (eprop, &val);
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

	GtkWidget           *combo_box;
} GladeEPropEnum;

GLADE_MAKE_EPROP (GladeEPropEnum, glade_eprop_enum)
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
		
		gtk_combo_box_set_active (GTK_COMBO_BOX (eprop_enum->combo_box),
					  i < eclass->n_values ? i : 0);
		g_type_class_unref (eclass);
	}
}

static void
glade_eprop_enum_changed (GtkWidget           *combo_box,
			  GladeEditorProperty *eprop)
{
	gint           ival;
	GValue         val = { 0, };
	GladeProperty *property;
	GtkTreeModel  *tree_model;
	GtkTreeIter    iter;

	if (eprop->loading) return;

	tree_model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo_box), &iter);
	gtk_tree_model_get (tree_model, &iter, 1, &ival, -1);

	property = eprop->property;

	g_value_init (&val, eprop->klass->pspec->value_type);
	g_value_set_enum (&val, ival);

	glade_editor_property_commit_no_callback (eprop, &val);
	g_value_unset (&val);
}

static GtkWidget *
glade_eprop_enum_create_input (GladeEditorProperty *eprop)
{
	GladeEPropEnum      *eprop_enum = GLADE_EPROP_ENUM (eprop);
	GladePropertyClass  *klass;
	GEnumClass          *eclass;
	GtkListStore        *list_store;
	GtkTreeIter          iter;
	GtkCellRenderer     *cell_renderer;
	guint                i;
	
	klass  = eprop->klass;
	eclass = g_type_class_ref (klass->pspec->value_type);

	list_store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);

	gtk_tree_model_get_iter_first (GTK_TREE_MODEL(list_store), &iter);

	for (i = 0; i < eclass->n_values; i++)
	{
		const gchar *value_name = 
			glade_get_displayable_value (klass->pspec->value_type,
						     eclass->values[i].value_nick);
		if (value_name == NULL) value_name = eclass->values[i].value_nick;
		
		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter, 0, value_name, 1,
		                    eclass->values[i].value, -1);
	}

	eprop_enum->combo_box = gtk_combo_box_new_with_model
		(GTK_TREE_MODEL (list_store));

	cell_renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (eprop_enum->combo_box),
	                            cell_renderer, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (eprop_enum->combo_box),
	                               cell_renderer, "text", 0);

	g_signal_connect (G_OBJECT (eprop_enum->combo_box), "changed",
			  G_CALLBACK (glade_eprop_enum_changed), eprop);

	gtk_widget_show_all (eprop_enum->combo_box);

	g_type_class_unref (eclass);

	return eprop_enum->combo_box;
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
			
			value_name = glade_get_displayable_value
				(eprop->klass->pspec->value_type, klass->values[flag_num].value_nick);

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

		glade_editor_property_commit_no_callback (eprop, &val);
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
	gtk_editable_set_editable (GTK_EDITABLE (eprop_flags->entry), FALSE);

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
	GladeProjectFormat fmt;
	GladeEPropColor *eprop_color = GLADE_EPROP_COLOR (eprop);
	GdkColor        *color;
	gchar           *text;

	/* Chain up first */
	editor_property_class->load (eprop, property);
	
	if (property)
	{
		fmt = glade_project_get_format (property->widget->project);

		if ((text = glade_widget_adaptor_string_from_value
		     (GLADE_WIDGET_ADAPTOR (eprop->klass->handle),
		      eprop->klass, property->value, fmt)) != NULL)
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

	glade_editor_property_commit_no_callback (eprop, &value);
	g_value_unset (&value);
}

static GtkWidget *
glade_eprop_color_create_input (GladeEditorProperty *eprop)
{
	GladeEPropColor *eprop_color = GLADE_EPROP_COLOR (eprop);
	GtkWidget       *hbox;

	hbox = gtk_hbox_new (FALSE, 0);

	eprop_color->entry = gtk_entry_new ();
	gtk_editable_set_editable (GTK_EDITABLE (eprop_color->entry), FALSE);
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
	GtkTreeModel        *store;
} GladeEPropText;

GLADE_MAKE_EPROP (GladeEPropText, glade_eprop_text)
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
	GladeProjectFormat fmt;
	GladeEPropText *eprop_text = GLADE_EPROP_TEXT (eprop);

	/* Chain up first */
	editor_property_class->load (eprop, property);
	
	if (property == NULL) return;

	fmt = glade_project_get_format (property->widget->project);

	if (GTK_IS_COMBO_BOX (eprop_text->text_entry))
	{
		if (GTK_IS_COMBO_BOX_ENTRY (eprop_text->text_entry))
		{
			const gchar *text = g_value_get_string (property->value);
			if (!text) text = "";
			gtk_entry_set_text (GTK_ENTRY (GTK_BIN (eprop_text->text_entry)->child), text);
		}
		else
		{
			const gchar *text = g_value_get_string (property->value);
			gint value = text ? 
				glade_utils_enum_value_from_string (GLADE_TYPE_STOCK, text) : 0;

			/* Set active iter... */
			gtk_combo_box_set_active (GTK_COMBO_BOX (eprop_text->text_entry), value);
		}
	}
	else if (GTK_IS_ENTRY (eprop_text->text_entry))
	{
		GtkEntry *entry = GTK_ENTRY (eprop_text->text_entry);
		const gchar *text = NULL;

		if (G_VALUE_TYPE (property->value) == G_TYPE_STRING)
			text = g_value_get_string (property->value);
		else if (G_VALUE_TYPE (property->value) == GDK_TYPE_PIXBUF)
		{
			GObject *object = g_value_get_object (property->value);
			if (object)
				text = g_object_get_data (object, "GladeFileName");
		}
		gtk_entry_set_text (entry, text ? text : "");
	}
	else if (GTK_IS_TEXT_VIEW (eprop_text->text_entry))
	{
		GtkTextBuffer  *buffer;
			
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (eprop_text->text_entry));

		if (G_VALUE_HOLDS (property->value, G_TYPE_STRV) ||
		    G_VALUE_HOLDS (property->value, G_TYPE_VALUE_ARRAY))
		{
			gchar *text = glade_widget_adaptor_string_from_value
				(GLADE_WIDGET_ADAPTOR (property->klass->handle),
				 property->klass, property->value, fmt);
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
	    eprop->property->klass->pspec->value_type == G_TYPE_VALUE_ARRAY ||
	    eprop->property->klass->pspec->value_type == GDK_TYPE_PIXBUF)
	{
		val = glade_property_class_make_gvalue_from_string 
			(eprop->property->klass, text, 
			 eprop->property->widget->project, eprop->property->widget);
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

	glade_editor_property_commit_no_callback (eprop, val);
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

/**
 * glade_editor_property_show_i18n_dialog:
 * @parent: The parent widget for the dialog.
 * @fmt: the #GladeProjectFormat
 * @text: A read/write pointer to the text property
 * @context: A read/write pointer to the translation context
 * @comment: A read/write pointer to the translator comment
 * @has_context: A read/write pointer to the context setting (libglade only)
 * @translatable: A read/write pointer to the translatable setting]
 *
 * Runs a dialog and updates the provided values.
 *
 * Returns: %TRUE if OK was selected.
 */
gboolean
glade_editor_property_show_i18n_dialog (GtkWidget            *parent,
					GladeProjectFormat    fmt,
					gchar               **text,
					gchar               **context,
					gchar               **comment,
					gboolean             *has_context,
					gboolean             *translatable)
{
	GtkWidget     *dialog;
	GtkWidget     *vbox, *hbox;
	GtkWidget     *label;
	GtkWidget     *sw;
	GtkWidget     *alignment;
	GtkWidget     *text_view, *comment_view, *context_view;
	GtkTextBuffer *text_buffer, *comment_buffer, *context_buffer = NULL;
	GtkWidget     *translatable_button, *context_button;
	gint           res;

	g_return_val_if_fail (text && context && comment && translatable && has_context, FALSE);

	dialog = gtk_dialog_new_with_buttons (_("Edit Text"),
					      parent ? GTK_WINDOW (gtk_widget_get_toplevel (parent)) : NULL,
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

	if (*text)
	{
		gtk_text_buffer_set_text (text_buffer,
					  *text,
					  -1);
	}

	/* Translatable and context prefix. */
	hbox = gtk_hbox_new (FALSE, 12);
	gtk_widget_show (hbox);

	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	/* Translatable */
	translatable_button = gtk_check_button_new_with_mnemonic (_("T_ranslatable"));
	gtk_widget_show (translatable_button);
	gtk_box_pack_start (GTK_BOX (hbox), translatable_button, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (translatable_button), *translatable);
	gtk_widget_set_tooltip_text (translatable_button,
				     _("Whether this property is translatable or not"));
	
	/* Has Context */
	context_button = gtk_check_button_new_with_mnemonic (_("_Has context prefix"));
	gtk_box_pack_start (GTK_BOX (hbox), context_button, FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (context_button), *has_context);
	gtk_widget_set_tooltip_text (context_button,
				     _("Whether or not the translatable string has a context prefix"));
	if (fmt == GLADE_PROJECT_FORMAT_LIBGLADE)
		gtk_widget_show (context_button);

	/* Context. */
	if (fmt != GLADE_PROJECT_FORMAT_LIBGLADE)
	{
		alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
		gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 12, 0, 0, 0);
		gtk_widget_show (alignment);
		
		label = gtk_label_new_with_mnemonic (_("Conte_xt for translation:"));
		gtk_widget_show (label);
		gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
		gtk_container_add (GTK_CONTAINER (alignment), label);
		gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);
		gtk_widget_set_tooltip_text (alignment,
					     "XXX Some explanation about translation context please ???");
		
		sw = gtk_scrolled_window_new (NULL, NULL);
		gtk_widget_show (sw);
		gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
						GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
		
		context_view = gtk_text_view_new ();
		gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (context_view), GTK_WRAP_WORD);
		gtk_widget_show (context_view);
		
		gtk_label_set_mnemonic_widget (GTK_LABEL (label), context_view);
		
		gtk_container_add (GTK_CONTAINER (sw), context_view);
		
		context_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (context_view));
		
		if (*context)
		{
			gtk_text_buffer_set_text (context_buffer,
						  *context,
						  -1);
		}
	}

	/* Comments. */
	alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 12, 0, 0, 0);
	gtk_widget_show (alignment);

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

	if (*comment)
	{
		gtk_text_buffer_set_text (comment_buffer,
					  *comment,
					  -1);
	}
	
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_OK) {
		GtkTextIter start, end;

		g_free ((gpointer)*text);
		g_free ((gpointer)*context);
		g_free ((gpointer)*comment);

		/* get the new values for translatable, has_context, and comment */
		*translatable = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (translatable_button));
		*has_context = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (context_button));

		/* Comment */
		gtk_text_buffer_get_bounds (comment_buffer, &start, &end);
		*comment = gtk_text_buffer_get_text (comment_buffer, &start, &end, TRUE);
		if (*comment[0] == '\0')
		{
			g_free (*comment);
			*comment = NULL;
		}

		/* Text */
		gtk_text_buffer_get_bounds (text_buffer, &start, &end);
		*text = gtk_text_buffer_get_text (text_buffer, &start, &end, TRUE);
		if (*text[0] == '\0')
		{
			g_free (*text);
			*text = NULL;
		}

		/* Context */
		if (fmt != GLADE_PROJECT_FORMAT_LIBGLADE)
		{
			gtk_text_buffer_get_bounds (context_buffer, &start, &end);
			*context = gtk_text_buffer_get_text (context_buffer, &start, &end, TRUE);
			if (*context[0] == '\0')
			{
				g_free (*context);
				*context = NULL;
			}
		}

		gtk_widget_destroy (dialog);
		return TRUE;
	}

	gtk_widget_destroy (dialog);
	return FALSE;
}

static void
glade_eprop_text_show_i18n_dialog (GtkWidget           *entry,
				   GladeEditorProperty *eprop)
{
	GladeProject *project;
	GladeProjectFormat fmt;
	gchar *text = g_value_dup_string (eprop->property->value);
	gchar *context = g_strdup (glade_property_i18n_get_context (eprop->property));
	gchar *comment = g_strdup (glade_property_i18n_get_comment (eprop->property));
	gboolean translatable = glade_property_i18n_get_translatable (eprop->property);
	gboolean has_context = glade_property_i18n_get_has_context (eprop->property);

	project = eprop->property->widget->project;
	fmt = glade_project_get_format (project);

	if (glade_editor_property_show_i18n_dialog (entry, fmt, &text, &context, &comment, 
						    &has_context, &translatable))
	{
		glade_command_set_i18n (eprop->property, translatable, has_context, context, comment);
		glade_eprop_text_changed_common (eprop, text, eprop->use_command);

		glade_editor_property_load (eprop, eprop->property);

		g_free (text);
		g_free (context);
		g_free (comment);
	}
}

gboolean
glade_editor_property_show_resource_dialog (GladeProject *project, GtkWidget *parent, gchar **filename)
{

	GtkWidget     *dialog;
	gchar         *folder;

	g_return_val_if_fail (filename != NULL, FALSE);

	dialog = gtk_file_chooser_dialog_new (_("Select a file from the project resource directory"),
					      parent ? GTK_WINDOW (gtk_widget_get_toplevel (parent)) : NULL,
					      GTK_FILE_CHOOSER_ACTION_OPEN,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OPEN, GTK_RESPONSE_OK,
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

	folder = glade_project_resource_fullpath (project, ".");
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), folder);
	g_free (folder);
					     
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
	{
		gchar *name;

		name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

		*filename = name ? g_path_get_basename (name) : NULL;

		g_free (name);
		gtk_widget_destroy (dialog);
		return TRUE;
	}

	gtk_widget_destroy (dialog);

	return FALSE;
}

static void
glade_eprop_text_show_resource_dialog (GtkWidget           *entry,
				       GladeEditorProperty *eprop)
{
	GladeProject *project;
	GladeProjectFormat fmt;
	gchar *text = NULL;

	project = eprop->property->widget->project;
	fmt = glade_project_get_format (project);

	if (glade_editor_property_show_resource_dialog (project, entry, &text))
	{
		glade_eprop_text_changed_common (eprop, text, eprop->use_command);

		glade_editor_property_load (eprop, eprop->property);

		g_free (text);
	}
}

enum {
	COMBO_COLUMN_TEXT = 0,
	COMBO_COLUMN_PIXBUF,
	COMBO_LAST_COLUMN
};

static GtkListStore *
glade_eprop_text_create_store (GType enum_type)
{
	GtkListStore        *store;
	GtkTreeIter          iter;
	GEnumClass          *eclass;
	guint                i;

	eclass = g_type_class_ref (enum_type);

	store = gtk_list_store_new (COMBO_LAST_COLUMN, 
				    G_TYPE_STRING, 
				    G_TYPE_STRING);
	
	for (i = 0; i < eclass->n_values; i++)
	{
		const gchar *displayable = glade_get_displayable_value (enum_type, eclass->values[i].value_nick);
		if (!displayable)
			displayable = eclass->values[i].value_nick;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 
				    COMBO_COLUMN_TEXT, displayable,
				    COMBO_COLUMN_PIXBUF, eclass->values[i].value_nick,
				    -1);
	}

	g_type_class_unref (eclass);

	return store;
}

static void
eprop_text_stock_changed (GtkComboBox *combo,
			  GladeEditorProperty *eprop)
{
	GladeEPropText  *eprop_text = GLADE_EPROP_TEXT (eprop);
	GtkTreeIter      iter;
	gchar           *text = NULL;
	const gchar     *str;

	if (eprop->loading) return;

	if (gtk_combo_box_get_active_iter (combo, &iter))
	{
		gtk_tree_model_get (GTK_TREE_MODEL (eprop_text->store), &iter,
				    COMBO_COLUMN_PIXBUF, &text,
				    -1);
		glade_eprop_text_changed_common (eprop, text, eprop->use_command);
		g_free (text);
	}
	else if (GTK_IS_COMBO_BOX_ENTRY (combo))
	{
		str = gtk_entry_get_text (GTK_ENTRY (GTK_BIN (combo)->child));
		glade_eprop_text_changed_common (eprop, str, eprop->use_command);
	}
}

static GtkWidget *
glade_eprop_text_create_input (GladeEditorProperty *eprop)
{
	GladeEPropText      *eprop_text = GLADE_EPROP_TEXT (eprop);
	GladePropertyClass  *klass;
	GtkWidget           *hbox;

	klass = eprop->klass;

	hbox = gtk_hbox_new (FALSE, 0);

	if (klass->stock || klass->stock_icon)
	{
		GtkCellRenderer *renderer;
		GtkWidget       *combo = gtk_combo_box_entry_new ();

		eprop_text->store = (GtkTreeModel *)
			glade_eprop_text_create_store (klass->stock ? GLADE_TYPE_STOCK :
						       GLADE_TYPE_STOCK_IMAGE);

		gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (eprop_text->store));

		/* let the comboboxentry prepend its intrusive cell first... */
		gtk_combo_box_entry_set_text_column (GTK_COMBO_BOX_ENTRY (combo),
						     COMBO_COLUMN_TEXT);

		renderer = gtk_cell_renderer_pixbuf_new ();
		gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, FALSE);
		gtk_cell_layout_reorder (GTK_CELL_LAYOUT (combo), renderer, 0);
		gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
						"stock-id", COMBO_COLUMN_PIXBUF,
						NULL);

		/* Dont allow custom items where an actual GTK+ stock item is expected
		 * (i.e. real items come with labels) */
		if (klass->stock)	
			gtk_editable_set_editable (GTK_EDITABLE (GTK_BIN (combo)->child), FALSE);
		else
			gtk_editable_set_editable (GTK_EDITABLE (GTK_BIN (combo)->child), TRUE);
		
		gtk_widget_show (combo);
		gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0); 
		g_signal_connect (G_OBJECT (combo), "changed",
				  G_CALLBACK (eprop_text_stock_changed), eprop);


		eprop_text->text_entry = combo;
	} 
	else if (klass->visible_lines > 1 ||
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

		if (klass->pspec->value_type == GDK_TYPE_PIXBUF)
		{
			GtkWidget *image = gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU);
			GtkWidget *button = gtk_button_new ();
			gtk_container_add (GTK_CONTAINER (button), image);

			g_signal_connect (button, "clicked",
					  G_CALLBACK (glade_eprop_text_show_resource_dialog),
					  eprop);

			gtk_widget_show_all (button);

			gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0); 
		}
	}
	
	if (klass->translatable) {
		GtkWidget *button = gtk_button_new_with_label ("\342\200\246");
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

	glade_editor_property_commit_no_callback (eprop, &val);

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

		glade_editor_property_commit_no_callback (eprop, &val);

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
#define GLADE_RESPONSE_CREATE 43

typedef struct {
	GladeEditorProperty parent_instance;
	
	GtkWidget *entry;
} GladeEPropObject;

GLADE_MAKE_EPROP (GladeEPropObject, glade_eprop_object)
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
				       GtkTreeIter         *parent_iter,
				       gboolean             recurse)
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
				has_decendant = recurse && glade_widget_has_decendant 
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
				has_decendant = recurse && glade_widget_has_decendant 
					(widget, eprop->klass->pspec->value_type);

				good_type = g_type_is_a (widget->adaptor->type, 
							 eprop->klass->pspec->value_type);

			}

			if (eprop->klass->parentless_widget)
				good_type = good_type && !GWA_IS_TOPLEVEL (widget->adaptor);

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
				glade_eprop_object_populate_view_real (eprop, model, children, copy, recurse);
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
	glade_eprop_object_populate_view_real (eprop, model, toplevels, NULL, !eprop->klass->parentless_widget);
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
	const gchar        *format;

	if (eprop->klass->parentless_widget)
		format = GLADE_IS_PARAM_SPEC_OBJECTS (eprop->klass->pspec) ?
			_("Choose parentless %s(s) in this project") : _("Choose a parentless %s in this project");
	else
		format = GLADE_IS_PARAM_SPEC_OBJECTS (eprop->klass->pspec) ?
			_("Choose %s(s) in this project") : _("Choose a %s in this project");

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
	GladeWidgetAdaptor *create_adaptor = NULL;
	
	project = glade_widget_get_project (eprop->property->widget);
	parent = gtk_widget_get_toplevel (GTK_WIDGET (eprop));

	if (glade_project_get_format (project) != GLADE_PROJECT_FORMAT_LIBGLADE)
	{
		if (eprop->property->klass->create_type)
			create_adaptor = glade_widget_adaptor_get_by_name (eprop->property->klass->create_type);
		if (!create_adaptor && 
		    G_TYPE_IS_INSTANTIATABLE (eprop->klass->pspec->value_type) &&
		    !G_TYPE_IS_ABSTRACT (eprop->klass->pspec->value_type))
			create_adaptor = glade_widget_adaptor_get_by_type (eprop->klass->pspec->value_type);
	}

	if (create_adaptor)
	{
		dialog = gtk_dialog_new_with_buttons (title,
						      GTK_WINDOW (parent),
						      GTK_DIALOG_MODAL,
						      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						      GTK_STOCK_CLEAR, GLADE_RESPONSE_CLEAR,
						      _("_New"), GLADE_RESPONSE_CREATE,
						      GTK_STOCK_OK, GTK_RESPONSE_OK,
						      NULL);
		g_free (title);
		
		gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
							 GTK_RESPONSE_OK,
							 GLADE_RESPONSE_CREATE,
							 GTK_RESPONSE_CANCEL,
							 GLADE_RESPONSE_CLEAR,
							 -1);
	}
	else
	{
		dialog = gtk_dialog_new_with_buttons (title,
						      GTK_WINDOW (parent),
						      GTK_DIALOG_MODAL,
						      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						      GTK_STOCK_CLEAR, GLADE_RESPONSE_CLEAR,
						      GTK_STOCK_OK, GTK_RESPONSE_OK,
						      NULL);
		g_free (title);
		
		gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
							 GTK_RESPONSE_OK,
							 GTK_RESPONSE_CANCEL,
							 GLADE_RESPONSE_CLEAR,
							 -1);
	}
		
	
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
			GValue *value;

			glade_project_selection_set (project, eprop->property->widget->object, TRUE);

			value = glade_property_class_make_gvalue_from_string
				(eprop->klass, selected->name, project, eprop->property->widget);

			/* Unparent the widget so we can reuse it for this property */
			if (eprop->klass->parentless_widget)
			{
				GObject *new_object, *old_object = NULL;
				GladeWidget *new_widget;
				GladeProperty *old_ref;

				if (!G_IS_PARAM_SPEC_OBJECT (eprop->klass->pspec))
					g_warning ("Parentless widget property should be of object type");
				else
				{
					glade_property_get (eprop->property, &old_object);
					new_object = g_value_get_object (value);
					new_widget = glade_widget_get_from_gobject (new_object);

					if (new_object && old_object != new_object)
					{
						if ((old_ref = glade_widget_get_parentless_widget_ref (new_widget)))
						{
							glade_command_push_group (_("Setting %s of %s to %s"),
										  eprop->property->klass->name,
										  eprop->property->widget->name, 
										  new_widget->name);
							glade_command_set_property (old_ref, NULL);
							glade_editor_property_commit (eprop, value);
							glade_command_pop_group ();
						}
						else
							glade_editor_property_commit (eprop, value);
					}
				}
			} 
			else
				glade_editor_property_commit (eprop, value);

			g_value_unset (value);
			g_free (value);
		}
	} 
	else if (res == GLADE_RESPONSE_CREATE)
	{
		GValue *value;
		GladeWidget *new_widget;

		/* translators: Creating 'a widget' for 'a property' of 'a widget' */
		glade_command_push_group (_("Creating %s for %s of %s"),
					  create_adaptor->name,
					  eprop->property->klass->name, 
					  eprop->property->widget->name);
		
		/* Dont bother if the user canceled the widget */
		if ((new_widget = glade_command_create (create_adaptor, NULL, NULL, project)) != NULL)
		{
			glade_project_selection_set (project, eprop->property->widget->object, TRUE);

			value = glade_property_class_make_gvalue_from_string
				(eprop->klass, new_widget->name, project, NULL);

			glade_editor_property_commit (eprop, value);

			g_value_unset (value);
			g_free (value);
		}

		glade_command_pop_group ();
	}
	else if (res == GLADE_RESPONSE_CLEAR)
	{
		GValue *value = glade_property_class_make_gvalue_from_string
			(eprop->klass, NULL, project, eprop->property->widget);
		
		glade_editor_property_commit (eprop, value);

		g_value_unset (value);
		g_free (value);
	}
	
	gtk_widget_destroy (dialog);
}


static void
glade_eprop_object_load (GladeEditorProperty *eprop, GladeProperty *property)
{
	GladeProjectFormat fmt;
	GladeEPropObject *eprop_object = GLADE_EPROP_OBJECT (eprop);
	gchar *obj_name;

	/* Chain up first */
	editor_property_class->load (eprop, property);

	if (property == NULL) return;

	fmt = glade_project_get_format (property->widget->project);

	if ((obj_name = glade_widget_adaptor_string_from_value
	     (GLADE_WIDGET_ADAPTOR (eprop->klass->handle),
	      eprop->klass, property->value, fmt)) != NULL)
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
	gtk_editable_set_editable (GTK_EDITABLE (eprop_object->entry), FALSE);
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
	GladeProjectFormat fmt;
	GladeEPropObjects *eprop_objects = GLADE_EPROP_OBJECTS (eprop);
	gchar *obj_name;

	/* Chain up first */
	editor_property_class->load (eprop, property);

	if (property == NULL) return;

	fmt = glade_project_get_format (property->widget->project);

	if ((obj_name = glade_widget_adaptor_string_from_value
	     (GLADE_WIDGET_ADAPTOR (eprop->klass->handle),
	      eprop->klass, property->value, fmt)) != NULL)
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
	gtk_editable_set_editable (GTK_EDITABLE (eprop_objects->entry), FALSE);
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

	GtkWidget *notebook;

	GtkWidget *libglade;
	GtkWidget *entry;

	GtkWidget *value, *lower, *upper, *step_increment, *page_increment, *page_size;
	GtkAdjustment *value_adj;
	struct
	{
		gulong value, lower, upper, step_increment, page_increment, page_size;
	}ids;
} GladeEPropAdjustment;

GLADE_MAKE_EPROP (GladeEPropAdjustment, glade_eprop_adjustment)
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
	GladeProjectFormat fmt;
	GObject *object;
	GtkAdjustment *adj = NULL;

	/* Chain up first */
	editor_property_class->load (eprop, property);

	if (property == NULL) return;
	
	fmt = glade_project_get_format (property->widget->project);

	gtk_widget_hide (eprop_adj->libglade);

	if (fmt == GLADE_PROJECT_FORMAT_LIBGLADE)
	{
		object = g_value_get_object (property->value);

		if (object)
			adj = GTK_ADJUSTMENT (object);
		
		/* Keep track of external adjustment changes */
		g_signal_connect (object, "value-changed",
				  G_CALLBACK (glade_eprop_adj_value_changed),
				  eprop);
	
		/* Update adjustment's values */
		eprop_adj->value_adj->value = adj ? adj->value : 0.0;
		eprop_adj->value_adj->lower = adj ? adj->lower : 0.0;
		eprop_adj->value_adj->upper = adj ? adj->upper : 100.0;
		eprop_adj->value_adj->step_increment = adj ? adj->step_increment : 1;
		eprop_adj->value_adj->page_increment = adj ? adj->page_increment : 10;
		eprop_adj->value_adj->page_size = adj ? adj->page_size : 10;
		
		/* Block Handlers */
		g_signal_handler_block (eprop_adj->value, eprop_adj->ids.value);
		g_signal_handler_block (eprop_adj->lower, eprop_adj->ids.lower);
		g_signal_handler_block (eprop_adj->upper, eprop_adj->ids.upper);
		g_signal_handler_block (eprop_adj->step_increment, eprop_adj->ids.step_increment);
		g_signal_handler_block (eprop_adj->page_increment, eprop_adj->ids.page_increment);
		g_signal_handler_block (eprop_adj->page_size, eprop_adj->ids.page_size);
		
		/* Update spinbuttons values */
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (eprop_adj->value), eprop_adj->value_adj->value);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (eprop_adj->lower), eprop_adj->value_adj->lower);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (eprop_adj->upper), eprop_adj->value_adj->upper);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (eprop_adj->step_increment), 
					   eprop_adj->value_adj->step_increment);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (eprop_adj->page_increment), 
					   eprop_adj->value_adj->page_increment);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (eprop_adj->page_size), eprop_adj->value_adj->page_size);
		
		/* Unblock Handlers */
		g_signal_handler_unblock (eprop_adj->value, eprop_adj->ids.value);
		g_signal_handler_unblock (eprop_adj->lower, eprop_adj->ids.lower);
		g_signal_handler_unblock (eprop_adj->upper, eprop_adj->ids.upper);
		g_signal_handler_unblock (eprop_adj->step_increment, eprop_adj->ids.step_increment);
		g_signal_handler_unblock (eprop_adj->page_increment, eprop_adj->ids.page_increment);
		g_signal_handler_unblock (eprop_adj->page_size, eprop_adj->ids.page_size);

		gtk_widget_show (eprop_adj->libglade);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (eprop_adj->notebook), 0);
	}
	else
	{
		gchar *obj_name;

		fmt = glade_project_get_format (property->widget->project);

		if ((obj_name = glade_widget_adaptor_string_from_value
		     (GLADE_WIDGET_ADAPTOR (eprop->klass->handle),
		      eprop->klass, property->value, fmt)) != NULL)
		{
			gtk_entry_set_text (GTK_ENTRY (eprop_adj->entry), obj_name);
			g_free (obj_name);
		}
		else
			gtk_entry_set_text (GTK_ENTRY (eprop_adj->entry), "");

		gtk_notebook_set_current_page (GTK_NOTEBOOK (eprop_adj->notebook), 1);
	}

	gtk_widget_queue_resize (eprop_adj->notebook);
}

static GtkAdjustment *
glade_eprop_adjustment_dup_adj (GladeEditorProperty *eprop)
{
	GtkAdjustment *adj;
	GObject *object;
	
	object = g_value_get_object (eprop->property->value);
	if (object == NULL) 
		return GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 100.0,
							   1.0, 10.0, 10.0));
	
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

	if (adjustment->value == 0.00 &&
	    adjustment->lower == 0.00 &&
	    adjustment->upper == 100.00 &&
	    adjustment->step_increment == 1.00 &&
	    adjustment->page_increment == 10.00 &&
	    adjustment->page_size == 10.00)
	{
		gtk_object_destroy (GTK_OBJECT (adjustment));
		g_value_set_object (&value, NULL);
	}
	else
		g_value_set_object (&value, G_OBJECT (adjustment));
	
	glade_editor_property_commit_no_callback (eprop, &value);

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
glade_eprop_adjustment_create_input_libglade (GladeEditorProperty *eprop)
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

static GtkWidget *
glade_eprop_adjustment_create_input_builder (GladeEditorProperty *eprop)
{
	GladeEPropAdjustment *eprop_adj = GLADE_EPROP_ADJUSTMENT (eprop);
	GtkWidget        *hbox;
	GtkWidget        *button;

	hbox             = gtk_hbox_new (FALSE, 0);
	eprop_adj->entry = gtk_entry_new ();
	gtk_editable_set_editable (GTK_EDITABLE (eprop_adj->entry), FALSE);
	gtk_widget_show (eprop_adj->entry);
	gtk_box_pack_start (GTK_BOX (hbox), eprop_adj->entry, TRUE, TRUE, 0);

	button = gtk_button_new_with_label ("...");
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), button,  FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_eprop_object_show_dialog),
			  eprop);
	return hbox;
}

static GtkWidget *
glade_eprop_adjustment_create_input (GladeEditorProperty *eprop)
{
	GladeEPropAdjustment *eprop_adj = GLADE_EPROP_ADJUSTMENT (eprop);
	GtkWidget *builder;

	eprop_adj->libglade = glade_eprop_adjustment_create_input_libglade (eprop);
	builder = glade_eprop_adjustment_create_input_builder (eprop);

	gtk_widget_show (eprop_adj->libglade);
	gtk_widget_show (builder);
	
	eprop_adj->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (eprop_adj->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (eprop_adj->notebook), FALSE);

	gtk_notebook_append_page (GTK_NOTEBOOK (eprop_adj->notebook), eprop_adj->libglade, NULL);
	gtk_notebook_append_page (GTK_NOTEBOOK (eprop_adj->notebook), builder, NULL);
	return eprop_adj->notebook;
}


/*******************************************************************************
                                     API
 *******************************************************************************/
/**
 * glade_editor_property_commit:
 * @eprop: A #GladeEditorProperty
 * @value: The #GValue to commit
 *
 * Commits @value to the property currently being edited by @eprop.
 *
 */
void
glade_editor_property_commit (GladeEditorProperty *eprop,
			      GValue              *value)
{
	g_return_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop));
	g_return_if_fail (G_IS_VALUE (value));

	g_signal_emit (G_OBJECT (eprop), glade_eprop_signals[COMMIT], 0, value);
}

/**
 * glade_editor_property_load:
 * @eprop: A #GladeEditorProperty
 * @property: A #GladeProperty
 *
 * Loads @property values into @eprop and connects.
 * (the editor property will watch the property's value
 * until its loaded with another property or %NULL)
 */
void
glade_editor_property_load (GladeEditorProperty *eprop,
			    GladeProperty       *property)
{
	g_return_if_fail (GLADE_IS_EDITOR_PROPERTY (eprop));
	g_return_if_fail (property == NULL || GLADE_IS_PROPERTY (property));

	if (eprop->committing)
		return;

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
		/* properties are allowed to be missing on some internal widgets */
		property = glade_widget_get_property (widget, eprop->klass->id);

		glade_editor_property_load (eprop, property);
	}
}

