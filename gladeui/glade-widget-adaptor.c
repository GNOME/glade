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

#include <config.h>

/**
 * SECTION:glade-widget-adaptor
 * @Short_Description: Adaptor base class to add runtime support for each widget class.
 * 
 * The #GladeWidgetAdaptor object is a proxy for widget class support in Glade.
 * it is automatically generated from the xml and allows you to override its
 * methods in the plugin library for fine grained support on how you load/save
 * widgets and handle thier properties in the runtime and more.
 * 
 */

#include "glade.h"
#include "glade-widget-adaptor.h"
#include "glade-xml-utils.h"
#include "glade-property-class.h"
#include "glade-signal.h"
#include "glade-marshallers.h"
#include "glade-accumulators.h"
#include "glade-displayable-values.h"
#include "glade-editor-table.h"
#include "glade-widget-private.h"

/* For g_file_exists */
#include <sys/types.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <ctype.h>

#define DEFAULT_ICON_NAME "widget-gtk-frame"

#define GLADE_WIDGET_ADAPTOR_GET_PRIVATE(o)  \
        (G_TYPE_INSTANCE_GET_PRIVATE ((o), GLADE_TYPE_WIDGET_ADAPTOR, GladeWidgetAdaptorPrivate))

struct _GladeWidgetAdaptorPrivate {

	gchar       *catalog;      /* The name of the widget catalog this class
				    * was declared by.
				    */

	gchar       *book;         /* DevHelp search namespace for this widget class
				    */

	GdkCursor   *cursor;       /* a cursor for inserting widgets */

	gchar       *special_child_type; /* Special case code for children that
					  * are special children (like notebook tab 
					  * widgets for example).
					  */
};

struct _GladeChildPacking {
	gchar *parent_name;
	GList *packing_defaults;
};


struct _GladePackingDefault {
    gchar  *id;
    gchar  *value;
};


enum {
	PROP_0,
	PROP_NAME,
	PROP_TYPE,
	PROP_TITLE,
	PROP_GENERIC_NAME,
	PROP_ICON_NAME,
	PROP_CATALOG,
	PROP_BOOK,
	PROP_SPECIAL_TYPE,
	PROP_CURSOR
};

typedef struct _GladeChildPacking    GladeChildPacking;
typedef struct _GladePackingDefault  GladePackingDefault;

static GObjectClass *parent_class = NULL;
static GHashTable   *adaptor_hash = NULL;

/*******************************************************************************
                              Helper functions
 *******************************************************************************/

/*
  This function assignes "weight" to each property in its natural order staring from 1.
  If parent is 0 weight will be set for every GladePropertyClass in the list.
  This function will not override weight if it is already set (weight >= 0.0)
*/
static void
gwa_properties_set_weight (GList **properties, GType parent)
{
	gint normal = 0, common = 0, packing = 0;
	GList *l;

	for (l = *properties; l && l->data; l = g_list_next (l))
	{
		GladePropertyClass *klass = l->data;
	
		if (klass->visible &&
		    (parent) ? parent == klass->pspec->owner_type : TRUE &&
	    	    !klass->atk)
		{
			/* Use a different counter for each tab (common, packing and normal) */
			if (klass->common) common++;
			else if (klass->packing) packing++;
			else normal++;

			/* Skip if it is already set */
			if (klass->weight >= 0.0) continue;
			
			/* Special-casing weight of properties for seperate tabs */
			if (klass->common) klass->weight = common;
			else if (klass->packing) klass->weight = packing;
			else klass->weight = normal;
		}
	}
}

static void
gwa_create_cursor (GladeWidgetAdaptor *adaptor)
{
	GdkPixbuf *tmp_pixbuf, *widget_pixbuf;
	const GdkPixbuf *add_pixbuf;
	GdkDisplay *display;
	GError *error = NULL;

	/* only certain widget classes need to have cursors */
	if (G_TYPE_IS_INSTANTIATABLE (adaptor->type) == FALSE ||
            G_TYPE_IS_ABSTRACT (adaptor->type) != FALSE ||
            adaptor->generic_name == NULL)
		return;

	/* cannot continue if 'add widget' cursor pixbuf has not been loaded for any reason */
	if ((add_pixbuf = glade_cursor_get_add_widget_pixbuf ()) == NULL)
		return;

	display = gdk_display_get_default ();

	/* create a temporary pixbuf clear to transparent black*/
	tmp_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 32, 32);
	gdk_pixbuf_fill (tmp_pixbuf, 0x00000000);

	if (gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), adaptor->icon_name))
	{
		widget_pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
							  adaptor->icon_name,
							  22,
							  0,
							  &error);
		
		if (error) {
			g_warning ("Could not load image data for named icon '%s': %s", 
				   adaptor->icon_name,
				   error->message);
			g_error_free (error);
			return;
		}
	
	} else {
		return;
	}

	/* composite pixbufs */
	gdk_pixbuf_composite (widget_pixbuf, tmp_pixbuf,
			      8, 8, 22, 22,
			      8, 8, 1, 1,
                              GDK_INTERP_NEAREST, 255);

	gdk_pixbuf_composite (add_pixbuf, tmp_pixbuf,
			      0, 0, 12, 12,
			      0, 0, 1, 1,
                              GDK_INTERP_NEAREST, 255);


	adaptor->priv->cursor = gdk_cursor_new_from_pixbuf (display, tmp_pixbuf, 6, 6);

	g_object_unref (tmp_pixbuf);
	g_object_unref (widget_pixbuf);
}

static gboolean
glade_widget_adaptor_hash_find (gpointer key, gpointer value, gpointer user_data)
{
	GladeWidgetAdaptor *adaptor = value;
	GType *type = user_data;
	
	if (g_type_is_a (adaptor->type, *type))
	{
		*type = adaptor->type;
		return TRUE;
	}
		
	return FALSE;
}

static void
glade_abort_if_derived_adaptors_exist (GType type)
{
	if (adaptor_hash)
	{
		GType retval = type;
	
		g_hash_table_find (adaptor_hash,
				   glade_widget_adaptor_hash_find,
				   &retval);
		if (retval != type)
			g_error (_("A derived adaptor (%s) of %s already exist!"),
				 g_type_name (retval), g_type_name (type));
	}
}

/*******************************************************************************
                     Base Object Implementation detail
 *******************************************************************************/
#define gwa_get_parent_adaptor(a) glade_widget_adaptor_get_parent_adaptor (a)

static GladeWidgetAdaptor *
glade_widget_adaptor_get_parent_adaptor_by_type (GType adaptor_type)
{
	GladeWidgetAdaptor *parent_adaptor = NULL;
	GType               iter_type;

	for (iter_type = g_type_parent (adaptor_type);
	     iter_type > 0;
	     iter_type = g_type_parent (iter_type))
	{
		if ((parent_adaptor = 
		     glade_widget_adaptor_get_by_type (iter_type)) != NULL)
			return parent_adaptor;
	}

	return NULL;
}

/* XXX DOCME
 */
GladeWidgetAdaptor *
glade_widget_adaptor_get_parent_adaptor (GladeWidgetAdaptor *adaptor)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

	return glade_widget_adaptor_get_parent_adaptor_by_type (adaptor->type);
}

static gint
gwa_signal_comp (gconstpointer a, gconstpointer b)
{
	const GladeSignalClass *signal_a = a, *signal_b = b;	
	return strcmp (signal_b->query.signal_name, signal_a->query.signal_name);
}

static gint
gwa_signal_find_comp (gconstpointer a, gconstpointer b)
{
	const GladeSignalClass *signal = a;	
	const gchar            *name = b;
	return strcmp (name, signal->query.signal_name);
}

static void
gwa_add_signals (GladeWidgetAdaptor *adaptor, GList **signals, GType type)
{
	guint count, *sig_ids, num_signals;
	GladeWidgetAdaptor *type_adaptor;
	GladeSignalClass *cur;
	GList *list = NULL;

	type_adaptor = glade_widget_adaptor_get_by_type (type);
	
	if (G_TYPE_IS_INSTANTIATABLE (type) || G_TYPE_IS_INTERFACE (type))
	{
		sig_ids = g_signal_list_ids (type, &num_signals);

		for (count = 0; count < num_signals; count++)
		{
			cur = g_new0 (GladeSignalClass, 1);
			
			g_signal_query (sig_ids[count], &(cur->query));

			/* Since glib gave us this signal id... it should
			 * exist no matter what.
			 */
			g_assert (cur->query.signal_id != 0);

			/* When creating this type, this type is not registered yet,
			 * but we still get the right value here.
			 */
			cur->adaptor = type_adaptor ? type_adaptor : adaptor;
			cur->name = (cur->query.signal_name);
			cur->type = (gchar *) g_type_name (type);

			/* Initialize signal versions to adaptor version */
			cur->version_since_major = GWA_VERSION_SINCE_MAJOR (cur->adaptor);
			cur->version_since_minor = GWA_VERSION_SINCE_MINOR (cur->adaptor);

			list = g_list_prepend (list, cur);
		}
		g_free (sig_ids);
		
		list = g_list_sort (list, gwa_signal_comp);
		*signals = g_list_concat (list, *signals);
	}
}

static GList * 
gwa_list_signals (GladeWidgetAdaptor *adaptor) 
{
	GList *signals = NULL;
	GType type, parent, *i, *p;

	g_return_val_if_fail (adaptor->type != 0, NULL);

	for (type = adaptor->type; g_type_is_a (type, G_TYPE_OBJECT); type = parent)
	{
		parent = g_type_parent (type);
		
		/* Add class signals */
		gwa_add_signals (adaptor, &signals, type);
	
		/* Add class interfaces signals */
		for (i = p = g_type_interfaces (type, NULL); *i; i++)
			if (!glade_util_class_implements_interface (parent, *i))
				gwa_add_signals (adaptor, &signals, *i);

		g_free (p);
	}

	return g_list_reverse (signals);
}

static GList * 
gwa_clone_parent_properties (GladeWidgetAdaptor *adaptor, gboolean is_packing) 
{
	GladeWidgetAdaptor *parent_adaptor;
	GList              *properties = NULL, *list, *proplist;
	
	if ((parent_adaptor = gwa_get_parent_adaptor (adaptor)) != NULL)
	{
		proplist = is_packing ? 
			parent_adaptor->packing_props : 
			parent_adaptor->properties;

		for (list = proplist; list; list = list->next)
		{
			GladePropertyClass *pclass =
				glade_property_class_clone (list->data);
			pclass->handle = adaptor;

			/* Reset versioning in derived catalogs just once */
			if (strcmp (adaptor->priv->catalog, 
				    parent_adaptor->priv->catalog))
			{
				pclass->version_since_major =
					pclass->version_since_minor = 0;

				pclass->builder_since_major =
					pclass->builder_since_minor = 0;

			}
			properties = g_list_prepend (properties, pclass);
		}
	}

	return g_list_reverse (properties);
}

static void
gwa_setup_introspected_props_from_pspecs (GladeWidgetAdaptor   *adaptor,
					  GParamSpec          **specs,
					  gint                  n_specs,
					  gboolean              is_packing)
{
	GladeWidgetAdaptor *parent_adaptor = gwa_get_parent_adaptor (adaptor);
	GladePropertyClass *property_class;
	gint                i;
	GList              *list = NULL;

	for (i = 0; i < n_specs; i++)
	{
		if (parent_adaptor == NULL ||
		    /* Dont create it if it already exists */
		    (!is_packing && !glade_widget_adaptor_get_property_class (parent_adaptor,
									      specs[i]->name)) ||
		    (is_packing &&  !glade_widget_adaptor_get_pack_property_class (parent_adaptor,
										   specs[i]->name)))
		{
			if ((property_class =
			     glade_property_class_new_from_spec (adaptor, specs[i])) != NULL)
				list = g_list_prepend (list, property_class);
		}
	}

	if (is_packing)
		adaptor->packing_props =
			g_list_concat (adaptor->packing_props, 
				       g_list_reverse (list));
	else
		adaptor->properties =
			g_list_concat (adaptor->properties, 
				       g_list_reverse (list));
}

static void
gwa_setup_properties (GladeWidgetAdaptor *adaptor,
		      GObjectClass       *object_class,
		      gboolean            is_packing) 
{
	GParamSpec   **specs = NULL;
	guint          n_specs = 0;
	GList         *l;

	/* only GtkContainer child propeties can be introspected */
	if (is_packing && !g_type_is_a (adaptor->type, GTK_TYPE_CONTAINER))
		return;

	/* First clone the parents properties */
	if (is_packing)
		adaptor->packing_props = gwa_clone_parent_properties (adaptor, is_packing);
	else
		adaptor->properties = gwa_clone_parent_properties (adaptor, is_packing);

	/* Now automaticly introspect new properties added in this class,
	 * allow the class writer to decide what to override in the resulting
	 * list of properties.
	 */
	if (is_packing)
		specs = gtk_container_class_list_child_properties (object_class, &n_specs);
	else
		specs = g_object_class_list_properties (object_class, &n_specs);
	gwa_setup_introspected_props_from_pspecs (adaptor, specs, n_specs, is_packing);
	g_free (specs);

	if (is_packing)
	{
		/* We have to mark packing properties from GladeWidgetAdaptor
		 * because GladePropertyClass doesnt have a valid parent GType
		 * to introspect it.
		 *
		 * (which could be used to call gtk_container_class_find_child_property()
		 * and properly introspect whether or not its a packing property).
		 */
		for (l = adaptor->packing_props; l; l = l->next)
		{
			GladePropertyClass *property_class = l->data;
			property_class->packing = TRUE;
		}
	}
}

static GList *
gwa_inherit_child_packing (GladeWidgetAdaptor *adaptor)
{
	GladeWidgetAdaptor *parent_adaptor;
	GList              *child_packings = NULL, *packing_list, *default_list;

	if ((parent_adaptor = gwa_get_parent_adaptor (adaptor)) != NULL)
	{
		for (packing_list = parent_adaptor->child_packings;
		     packing_list; packing_list = packing_list->next)
		{
			GladeChildPacking *packing     = packing_list->data;
			GladeChildPacking *packing_dup = g_new0 (GladeChildPacking, 1);

			packing_dup->parent_name = g_strdup (packing->parent_name);

			for (default_list = packing->packing_defaults;
			     default_list; default_list = default_list->next)
			{
				GladePackingDefault *def     = default_list->data;
				GladePackingDefault *def_dup = g_new0 (GladePackingDefault, 1);

				def_dup->id    = g_strdup (def->id);
				def_dup->value = g_strdup (def->value);

				packing_dup->packing_defaults = 
					g_list_prepend (packing_dup->packing_defaults, def_dup);
			}

			child_packings = g_list_prepend (child_packings, packing_dup);
		}
	}
	return child_packings;
}

static void
gwa_inherit_signals (GladeWidgetAdaptor *adaptor)
{
	GladeWidgetAdaptor *parent_adaptor;
	GList              *list, *node;
	GladeSignalClass   *signal, *parent_signal;

	if ((parent_adaptor = gwa_get_parent_adaptor (adaptor)) != NULL)
	{
		for (list = adaptor->signals; list; list = list->next)
		{
			signal = list->data;

			if ((node = g_list_find_custom
			     (parent_adaptor->signals, signal->name,
			      (GCompareFunc)gwa_signal_find_comp)) != NULL)
			{
				parent_signal = node->data;

				/* Reset versioning in derived catalogs just once */
				if (strcmp (adaptor->priv->catalog, 
					    parent_adaptor->priv->catalog))
					signal->version_since_major =
						signal->version_since_minor = 0;
				else
				{
					signal->version_since_major = 
						parent_signal->version_since_major;
					signal->version_since_minor = 
						parent_signal->version_since_minor;
				}
			}
		}
	}
}

static GObject *
glade_widget_adaptor_constructor (GType                  type,
				  guint                  n_construct_properties,
				  GObjectConstructParam *construct_properties)
{
	GladeWidgetAdaptor   *adaptor, *parent_adaptor;
	GObject              *ret_obj;
	GObjectClass         *object_class;

	glade_abort_if_derived_adaptors_exist (type);
	
	ret_obj = G_OBJECT_CLASS (parent_class)->constructor
		(type, n_construct_properties, construct_properties);

	adaptor = GLADE_WIDGET_ADAPTOR (ret_obj);
	parent_adaptor = gwa_get_parent_adaptor (adaptor);
	
	if (adaptor->type == G_TYPE_NONE)
		g_warning ("Adaptor created without a type");
	if (adaptor->name == NULL)
		g_warning ("Adaptor created without a name");

	/* Build decorations */
	if (!adaptor->icon_name) {		
		adaptor->icon_name = g_strdup ("gtk-missing-image");
	}
	gwa_create_cursor (adaptor);

	/* Let it leek */
	if ((object_class = g_type_class_ref (adaptor->type)))
	{
		/* Build signals & properties */
		adaptor->signals = gwa_list_signals (adaptor);

		gwa_inherit_signals (adaptor);
		gwa_setup_properties (adaptor, object_class, FALSE);
		gwa_setup_properties (adaptor, object_class, TRUE);
	}

	/* Detect scrollability */
	if (g_type_is_a (adaptor->type, GTK_TYPE_WIDGET) &&
	    GTK_WIDGET_CLASS (object_class)->set_scroll_adjustments_signal != 0)
		GLADE_WIDGET_ADAPTOR_GET_CLASS(adaptor)->scrollable = TRUE;

	/* Inherit packing defaults here */
	adaptor->child_packings = gwa_inherit_child_packing (adaptor);

	/* Inherit special-child-type */
	if (parent_adaptor)
		adaptor->priv->special_child_type =
			parent_adaptor->priv->special_child_type ?
			g_strdup (parent_adaptor->priv->special_child_type) : NULL;


	/* Reset version numbering if we're in a new catalog just once */
	if (parent_adaptor &&
	    strcmp (adaptor->priv->catalog, parent_adaptor->priv->catalog))
	{
		GLADE_WIDGET_ADAPTOR_GET_CLASS(adaptor)->version_since_major = 
			GLADE_WIDGET_ADAPTOR_GET_CLASS(adaptor)->version_since_minor = 0;

		GLADE_WIDGET_ADAPTOR_GET_CLASS(adaptor)->builder_since_major = 
			GLADE_WIDGET_ADAPTOR_GET_CLASS(adaptor)->builder_since_minor = 0;
	}

	/* Copy parent actions */
	if (parent_adaptor)
	{
		GList *l;
		
		if (parent_adaptor->actions)
		{
			for (l = parent_adaptor->actions; l; l = g_list_next (l))
			{
				GWActionClass *child = glade_widget_action_class_clone (l->data);
				adaptor->actions = g_list_prepend (adaptor->actions, child);
			}
			adaptor->actions = g_list_reverse (adaptor->actions);
		}
		
		if (parent_adaptor->packing_actions)
		{
			for (l = parent_adaptor->packing_actions; l; l = g_list_next (l))
			{
				GWActionClass *child = glade_widget_action_class_clone (l->data);
				adaptor->packing_actions = g_list_prepend (adaptor->packing_actions, child);
			}
			adaptor->packing_actions = g_list_reverse (adaptor->packing_actions);
		}
	}
	
	return ret_obj;
}

static void
gwa_packing_default_free (GladePackingDefault *def)
{
	g_free (def->id);
	g_free (def->value);
}

static void
gwa_child_packing_free (GladeChildPacking *packing)
{
        g_free (packing->parent_name);

        g_list_foreach (packing->packing_defaults,
                        (GFunc) gwa_packing_default_free, NULL);
        g_list_free (packing->packing_defaults);
}

static void
glade_widget_adaptor_finalize (GObject *object)
{
	GladeWidgetAdaptor *adaptor = GLADE_WIDGET_ADAPTOR (object);

	/* Free properties and signals */
	g_list_foreach (adaptor->properties, (GFunc) glade_property_class_free, NULL);
	g_list_free (adaptor->properties);

	g_list_foreach (adaptor->packing_props, (GFunc) glade_property_class_free, NULL);
	g_list_free (adaptor->packing_props);

	/* Be careful, this list holds GladeSignalClass* not GladeSignal,
	 * thus g_free is enough as all members are const */
	g_list_foreach (adaptor->signals, (GFunc) g_free, NULL);
	g_list_free (adaptor->signals);


	/* Free child packings */
	g_list_foreach (adaptor->child_packings,
			(GFunc) gwa_child_packing_free,
			NULL);
	g_list_free (adaptor->child_packings);

	if (adaptor->priv->book)       g_free (adaptor->priv->book);
	if (adaptor->priv->catalog)    g_free (adaptor->priv->catalog);
	if (adaptor->priv->special_child_type)
		g_free (adaptor->priv->special_child_type);

	if (adaptor->priv->cursor != NULL)
		gdk_cursor_unref (adaptor->priv->cursor);

	if (adaptor->name)         g_free (adaptor->name);
	if (adaptor->generic_name) g_free (adaptor->generic_name);
	if (adaptor->title)        g_free (adaptor->title);
	if (adaptor->icon_name)    g_free (adaptor->icon_name);
	if (adaptor->missing_icon) g_free (adaptor->missing_icon);

	if (adaptor->actions)
	{
		g_list_foreach (adaptor->actions,
				(GFunc) glade_widget_action_class_free,
				NULL);
		g_list_free (adaptor->actions);
	}
	
	if (adaptor->packing_actions)
	{
		g_list_foreach (adaptor->packing_actions,
				(GFunc) glade_widget_action_class_free,
				NULL);
		g_list_free (adaptor->packing_actions);
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
glade_widget_adaptor_real_set_property (GObject         *object,
					guint            prop_id,
					const GValue    *value,
					GParamSpec      *pspec)
{
	GladeWidgetAdaptor *adaptor;

	adaptor = GLADE_WIDGET_ADAPTOR (object);

	switch (prop_id)
	{
	case PROP_NAME:
		/* assume once (construct-only) */
		adaptor->name = g_value_dup_string (value);
		break;
	case PROP_ICON_NAME:
		/* assume once (construct-only) */
		adaptor->icon_name = g_value_dup_string (value);
		break;
	case PROP_TYPE:
		adaptor->type = g_value_get_gtype (value);
		break;
	case PROP_TITLE:
		if (adaptor->title) g_free (adaptor->title);
		adaptor->title = g_value_dup_string (value);
		break;
	case PROP_GENERIC_NAME:
		if (adaptor->generic_name) g_free (adaptor->generic_name);
		adaptor->generic_name = g_value_dup_string (value);
		break;
	case PROP_CATALOG:
		/* assume once (construct-only) */
		adaptor->priv->catalog = g_value_dup_string (value);
		break;
	case PROP_BOOK:
		/* assume once (construct-only) */
		adaptor->priv->book = g_value_dup_string (value);
		break;
	case PROP_SPECIAL_TYPE:
		/* assume once (construct-only) */
		adaptor->priv->special_child_type = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
glade_widget_adaptor_real_get_property (GObject         *object,
					guint            prop_id,
					GValue          *value,
					GParamSpec      *pspec)
{

	GladeWidgetAdaptor *adaptor;

	adaptor = GLADE_WIDGET_ADAPTOR (object);

	switch (prop_id)
	{
	case PROP_NAME:         g_value_set_string (value, adaptor->name);             break;
	case PROP_TYPE:	        g_value_set_gtype  (value, adaptor->type);             break;
	case PROP_TITLE:        g_value_set_string (value, adaptor->title);            break;
	case PROP_GENERIC_NAME: g_value_set_string (value, adaptor->generic_name);     break;
	case PROP_ICON_NAME:    g_value_set_string (value, adaptor->icon_name);        break;
	case PROP_CATALOG:      g_value_set_string (value, adaptor->priv->catalog);    break;
	case PROP_BOOK:         g_value_set_string (value, adaptor->priv->book);       break;
	case PROP_SPECIAL_TYPE: 
		g_value_set_string (value, adaptor->priv->special_child_type); 
		break;
	case PROP_CURSOR:     g_value_set_pointer (value, adaptor->priv->cursor);     break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/*******************************************************************************
                  GladeWidgetAdaptor base class implementations
 *******************************************************************************/
static GObject *
glade_widget_adaptor_object_construct_object (GladeWidgetAdaptor *adaptor,
					      guint               n_parameters,
					      GParameter         *parameters)
{
	return g_object_newv (adaptor->type, n_parameters, parameters);
}


static void
glade_widget_adaptor_object_set_property (GladeWidgetAdaptor *adaptor,
					  GObject            *object,
					  const gchar        *property_name,
					  const GValue       *value)
{
	g_object_set_property (object, property_name, value);
}

static void
glade_widget_adaptor_object_get_property (GladeWidgetAdaptor *adaptor,
					  GObject            *object,
					  const gchar        *property_name,
					  GValue             *value)
{
	g_object_get_property (object, property_name, value);
}

static void
glade_widget_adaptor_object_action_activate (GladeWidgetAdaptor *adaptor,
					     GObject *object,
					     const gchar *action_id)
{
	g_message ("No action_activate() support in adaptor %s for action '%s'",
		   adaptor->name, action_id);
}

static void
glade_widget_adaptor_object_child_action_activate (GladeWidgetAdaptor *adaptor,
						   GObject *container,
						   GObject *object,
						   const gchar *action_id)
{
	g_message ("No child_action_activate() support in adaptor %s for action '%s'",
		   adaptor->name, action_id);
}

static gboolean
glade_widget_adaptor_object_depends (GladeWidgetAdaptor *adaptor,
                                     GladeWidget *widget,
                                     GladeWidget *another)
{
  GList *l;

  for (l = _glade_widget_peek_prop_refs (another); l; l = g_list_next (l))
    {
      /* If one of the properties that reference @another is
       * owned by @widget then @widget depends on @another
       */
      if (glade_property_get_widget (l->data) == widget)
        return TRUE;
    }

  return FALSE;
}

static void
glade_widget_adaptor_object_read_widget (GladeWidgetAdaptor *adaptor,
					 GladeWidget        *widget,
					 GladeXmlNode       *node)
{
	GladeXmlNode *iter_node;
	GladeSignal *signal;
	GladeProperty *property;
	gchar *name, *prop_name;
	GList *read_properties = NULL, *l;

	/* Read in the properties */
	for (iter_node = glade_xml_node_get_children (node); 
	     iter_node; iter_node = glade_xml_node_next (iter_node))
	{
		if (!glade_xml_node_verify_silent (iter_node, GLADE_XML_TAG_PROPERTY))
			continue;

		/* Get prop name from node and lookup property ...*/
		if (!(name = glade_xml_get_property_string_required
		      (iter_node, GLADE_XML_TAG_NAME, NULL)))
			continue;

		prop_name = glade_util_read_prop_name (name);

		/* Some properties may be special child type of custom, just leave them for the adaptor */
		if ((property = glade_widget_get_property (widget, prop_name)) != NULL)
		{
			glade_property_read (property, widget->project, iter_node);
			read_properties = g_list_prepend (read_properties, property);
		}

		g_free (prop_name);
		g_free (name);
	}

	/* Sync the remaining values not read in from the Glade file.. */
	for (l = widget->properties; l; l = l->next)
	{
		property = l->data;

		if (!g_list_find (read_properties, property))
			glade_property_sync (property);
	}
	g_list_free (read_properties);

	/* Read in the signals */
	for (iter_node = glade_xml_node_get_children (node); 
	     iter_node; iter_node = glade_xml_node_next (iter_node))
	{
		if (!glade_xml_node_verify_silent (iter_node, GLADE_XML_TAG_SIGNAL))
			continue;
		
		if (!(signal = glade_signal_read (iter_node)))
			continue;

		glade_widget_add_signal_handler (widget, signal);
	}

	/* Read in children */
	for (iter_node = glade_xml_node_get_children (node); 
	     iter_node; iter_node = glade_xml_node_next (iter_node))
	{
		if (glade_xml_node_verify_silent (iter_node, GLADE_XML_TAG_CHILD))
			glade_widget_read_child (widget, iter_node);

		if (glade_project_load_cancelled (widget->project))
			return;
	}
}

static void
glade_widget_adaptor_object_write_widget (GladeWidgetAdaptor *adaptor,
					  GladeWidget        *widget,
					  GladeXmlContext    *context,
					  GladeXmlNode       *node)
{
	GList *props;

	/* Write the properties */
	for (props = widget->properties; 
	     props; props = props->next)
	{
		if (GLADE_PROPERTY (props->data)->klass->save && 
		    GLADE_PROPERTY (props->data)->enabled)
			glade_property_write (GLADE_PROPERTY (props->data), context, node);
	}
}

static void
glade_widget_adaptor_object_read_child (GladeWidgetAdaptor *adaptor,
					GladeWidget        *widget,
					GladeXmlNode       *node)
{
	GladeXmlNode *widget_node, *packing_node, *iter_node;
	GladeWidget  *child_widget;
	gchar        *internal_name;
	gchar        *name, *prop_name;
	GladeProperty *property;

	if (!glade_xml_node_verify (node, GLADE_XML_TAG_CHILD))
		return;

	internal_name = 
		glade_xml_get_property_string 
		(node, GLADE_XML_TAG_INTERNAL_CHILD);
	
	if ((widget_node = 
	     glade_xml_search_child
	     (node, GLADE_XML_TAG_WIDGET(glade_project_get_format(widget->project)))) != NULL)
	{
		child_widget = 
			glade_widget_read (widget->project, 
					   widget, 
					   widget_node, 
					   internal_name);
		
		if (child_widget)
		{
			if (!internal_name) {
				glade_widget_set_child_type_from_node 
					(widget, child_widget->object, node);
				glade_widget_add_child (widget, child_widget, FALSE);
			}
				
			if ((packing_node =
			     glade_xml_search_child
			     (node, GLADE_XML_TAG_PACKING)) != NULL)
			{

				/* Read in the properties */
				for (iter_node = glade_xml_node_get_children (packing_node); 
				     iter_node; iter_node = glade_xml_node_next (iter_node))
				{
					if (!glade_xml_node_verify_silent (iter_node, GLADE_XML_TAG_PROPERTY))
						continue;

					/* Get prop name from node and lookup property ...*/
					if (!(name = glade_xml_get_property_string_required
					      (iter_node, GLADE_XML_TAG_NAME, NULL)))
						continue;

					prop_name = glade_util_read_prop_name (name);

					/* Some properties may be special child type of custom, 
					 * just leave them for the adaptor */
					if ((property = glade_widget_get_pack_property (child_widget, prop_name)) != NULL)
						glade_property_read (property, child_widget->project, iter_node);
					
					g_free (prop_name);
					g_free (name);
				}
			}
		}
		
	} else {
		GObject *palaceholder = 
			G_OBJECT (glade_placeholder_new ());
		glade_widget_set_child_type_from_node (widget, palaceholder, node);
		glade_widget_adaptor_add (adaptor, widget->object, palaceholder);
		
	}
	g_free (internal_name);
}

static void
glade_widget_adaptor_object_write_child (GladeWidgetAdaptor *adaptor,
					 GladeWidget        *widget,
					 GladeXmlContext    *context,
					 GladeXmlNode       *node)
{
	GladeXmlNode *child_node, *packing_node;
	GList        *props;

	child_node = glade_xml_node_new (context, GLADE_XML_TAG_CHILD);
	glade_xml_node_append_child (node, child_node);

	/* Set internal child */
	if (widget->internal)
		glade_xml_node_set_property_string (child_node, 
						    GLADE_XML_TAG_INTERNAL_CHILD, 
						    widget->internal);

	/* Write out the widget */
	glade_widget_write (widget, context, child_node);

	/* Write out packing properties and special-child-type */
	packing_node = glade_xml_node_new (context, GLADE_XML_TAG_PACKING);
	glade_xml_node_append_child (child_node, packing_node);

	for (props = widget->packing_properties; 
	     props; props = props->next)
	{
		if (GLADE_PROPERTY (props->data)->klass->save && 
		    GLADE_PROPERTY (props->data)->enabled)
			glade_property_write (GLADE_PROPERTY (props->data), 
					      context, packing_node);
	}

	glade_widget_write_special_child_prop (widget->parent,
					       widget->object,
					       context, child_node);
	
	/* Default packing properties and such are not saved,
	 * so lets check afterwords if there was anything saved
	 * and then just remove the node.
	 */
	if (!glade_xml_node_get_children (packing_node))
	{
		glade_xml_node_remove (packing_node);
		glade_xml_node_delete (packing_node);
	}
}

static GType 
glade_widget_adaptor_get_eprop_type (GParamSpec *pspec)
{
	GType type = 0;

	if (G_IS_PARAM_SPEC_ENUM(pspec))
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
			type = GLADE_TYPE_EPROP_TEXT;
		else if (pspec->value_type == GTK_TYPE_ADJUSTMENT)
			type = GLADE_TYPE_EPROP_ADJUSTMENT;
		else
			type = GLADE_TYPE_EPROP_OBJECT;
	}
	else if (GLADE_IS_PARAM_SPEC_OBJECTS(pspec))
		type = GLADE_TYPE_EPROP_OBJECTS;

	return type;
}

static GladeEditorProperty *
glade_widget_adaptor_object_create_eprop (GladeWidgetAdaptor *adaptor,
					  GladePropertyClass *klass,
					  gboolean            use_command)
{
	GladeEditorProperty *eprop;
	GType                type = 0;

	/* Find the right type of GladeEditorProperty for this
	 * GladePropertyClass.
	 */
	if ((type = glade_widget_adaptor_get_eprop_type (klass->pspec)) == 0)
		return NULL;
	
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

static gchar *
glade_widget_adaptor_object_string_from_value (GladeWidgetAdaptor *adaptor,
					       GladePropertyClass *klass,
					       const GValue       *value,
					       GladeProjectFormat  fmt)
{
	return glade_property_class_make_string_from_gvalue (klass, value, fmt);
}

static GladeEditable *
glade_widget_adaptor_object_create_editable (GladeWidgetAdaptor   *adaptor,
					     GladeEditorPageType   type)
{
	return (GladeEditable *)glade_editor_table_new (adaptor, type);
}


/*******************************************************************************
            GladeWidgetAdaptor type registration and class initializer
 *******************************************************************************/
static void
glade_widget_adaptor_init (GladeWidgetAdaptor *adaptor)
{
	adaptor->priv = GLADE_WIDGET_ADAPTOR_GET_PRIVATE (adaptor);

}

static void
glade_widget_adaptor_class_init (GladeWidgetAdaptorClass *adaptor_class)
{
	GObjectClass       *object_class;
	g_return_if_fail (adaptor_class != NULL);
	
	parent_class = g_type_class_peek_parent (adaptor_class);
	object_class = G_OBJECT_CLASS (adaptor_class);

	/* GObjectClass */
	object_class->constructor           = glade_widget_adaptor_constructor;
	object_class->finalize              = glade_widget_adaptor_finalize;
	object_class->set_property          = glade_widget_adaptor_real_set_property;
	object_class->get_property          = glade_widget_adaptor_real_get_property;

	/* Class methods */
	adaptor_class->construct_object     = glade_widget_adaptor_object_construct_object;
	adaptor_class->deep_post_create     = NULL;
	adaptor_class->post_create          = NULL;
	adaptor_class->get_internal_child   = NULL;
	adaptor_class->verify_property      = NULL;
	adaptor_class->set_property         = glade_widget_adaptor_object_set_property;
	adaptor_class->get_property         = glade_widget_adaptor_object_get_property;
	adaptor_class->add                  = NULL;
	adaptor_class->remove               = NULL;
	adaptor_class->replace_child        = NULL;
	adaptor_class->get_children         = NULL;
	adaptor_class->child_set_property   = NULL;
	adaptor_class->child_get_property   = NULL;
	adaptor_class->action_activate      = glade_widget_adaptor_object_action_activate;
	adaptor_class->child_action_activate= glade_widget_adaptor_object_child_action_activate;
	adaptor_class->action_submenu       = NULL;
	adaptor_class->depends              = glade_widget_adaptor_object_depends;
	adaptor_class->read_widget          = glade_widget_adaptor_object_read_widget;
	adaptor_class->write_widget         = glade_widget_adaptor_object_write_widget;
	adaptor_class->read_child           = glade_widget_adaptor_object_read_child;
	adaptor_class->write_child          = glade_widget_adaptor_object_write_child;
	adaptor_class->create_eprop         = glade_widget_adaptor_object_create_eprop;
	adaptor_class->string_from_value    = glade_widget_adaptor_object_string_from_value;
	adaptor_class->create_editable      = glade_widget_adaptor_object_create_editable;

	/* Base defaults here */
	adaptor_class->fixed                = FALSE;
	adaptor_class->toplevel             = FALSE;
	adaptor_class->use_placeholders     = FALSE;
	adaptor_class->default_width        = -1;
	adaptor_class->default_height       = -1;

	/* Properties */
	g_object_class_install_property 
		(object_class, PROP_NAME,
		 g_param_spec_string 
		 ("name", _("Name"), 
		  _("Name of the class"),
		  NULL, 
		  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property 
		(object_class, PROP_TYPE,
		 g_param_spec_gtype 
		 ("type", _("Type"), 
		  _("GType of the class"),
		  G_TYPE_NONE,
		  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property 
		(object_class, PROP_TITLE,
		 g_param_spec_string 
		 ("title", _("Title"), 
		  _("Translated title for the class used in the glade UI"),
		  NULL, 
		  G_PARAM_READWRITE));

	g_object_class_install_property 
		(object_class, PROP_GENERIC_NAME,
		 g_param_spec_string 
		 ("generic-name", _("Generic Name"), 
		  _("Used to generate names of new widgets"),
		  NULL, 
		  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
		  
	g_object_class_install_property 
		(object_class, PROP_ICON_NAME,
		 g_param_spec_string 
		 ("icon-name", _("Icon Name"), 
		  _("The icon name"),
		  DEFAULT_ICON_NAME, 
		  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property 
		(object_class, PROP_CATALOG,
		 g_param_spec_string 
		 ("catalog", _("Catalog"), 
		  _("The name of the widget catalog this class was declared by"),
		  NULL, 
		  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property 
		(object_class, PROP_BOOK,
		 g_param_spec_string 
		 ("book", _("Book"), 
		  _("DevHelp search namespace for this widget class"),
		  NULL, 
		  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property 
		(object_class, PROP_SPECIAL_TYPE,
		 g_param_spec_string 
		 ("special-child-type", _("Special Child Type"), 
		  _("Holds the name of the packing property to depict "
		    "special children for this container class"),
		  NULL, 
		  G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property 
		(object_class, PROP_CURSOR,
		 g_param_spec_pointer 
		 ("cursor", _("Cursor"), 
		  _("A cursor for inserting widgets in the UI"),
		  G_PARAM_READABLE));
	
	g_type_class_add_private (adaptor_class, sizeof (GladeWidgetAdaptorPrivate));
}

GType
glade_widget_adaptor_get_type (void)
{
	static GType adaptor_type = 0;
	
	if (!adaptor_type)
	{
		static const GTypeInfo adaptor_info = 
		{
			sizeof (GladeWidgetAdaptorClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) glade_widget_adaptor_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,           /* class_data */
			sizeof (GladeWidgetAdaptor),
			0,              /* n_preallocs */
			(GInstanceInitFunc) glade_widget_adaptor_init,
		};
		adaptor_type = 
			g_type_register_static (G_TYPE_OBJECT,
						"GladeGObjectAdaptor",
						&adaptor_info, 0);
	}
	return adaptor_type;
}

GType
glade_create_reason_get_type (void)
{
	static GType etype = 0;

	if (etype == 0)
	{
		static const GEnumValue values[] = {
			{ GLADE_CREATE_USER,    "GLADE_CREATE_USER",    "create-user" },
			{ GLADE_CREATE_COPY,    "GLADE_CREATE_COPY",    "create-copy" },
			{ GLADE_CREATE_LOAD,    "GLADE_CREATE_LOAD",    "create-load" },
			{ GLADE_CREATE_REBUILD, "GLADE_CREATE_REBUILD", "create-rebuild" },
			{ 0, NULL, NULL }
		};

		etype = g_enum_register_static ("GladeCreateReason", values);

	}

	return etype;
}

/*******************************************************************************
                        Synthetic Object Derivation
 *******************************************************************************/
typedef struct
{
	GladeXmlNode *node;
	GModule      *module;
} GWADerivedClassData;

static void
gwa_derived_init (GladeWidgetAdaptor *adaptor, gpointer g_class)
{

}

static void
gwa_extend_with_node_load_sym (GladeWidgetAdaptorClass *klass,
			       GladeXmlNode            *node,
			       GModule                 *module)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	gpointer symbol;
	
	/*
	 * We use a temporary variable to avoid a bogus gcc warning.
	 * the thing it that g_module_symbol() should use a function pointer
	 * instead of a gpointer!
	 */
	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_CONSTRUCTOR_FUNCTION,
					  &symbol))
		object_class->constructor = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_CONSTRUCT_OBJECT_FUNCTION,
					  &symbol))
		klass->construct_object = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_DEEP_POST_CREATE_FUNCTION,
					  &symbol))
		klass->deep_post_create = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_POST_CREATE_FUNCTION,
					  &symbol))
		klass->post_create = symbol;
		
	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_SET_FUNCTION,
					  &symbol))
		klass->set_property = symbol;
	
	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_GET_FUNCTION,
					  &symbol))
		klass->get_property = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_VERIFY_FUNCTION,
					  &symbol))
		klass->verify_property = symbol;
	
	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_ADD_CHILD_FUNCTION,
					  &symbol))
		klass->add = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_REMOVE_CHILD_FUNCTION,
					  &symbol))
		klass->remove = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_REPLACE_CHILD_FUNCTION,
					  &symbol))
		klass->replace_child = symbol;
	
	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_GET_CHILDREN_FUNCTION,
					  &symbol))
		klass->get_children = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_CHILD_SET_PROP_FUNCTION,
					  &symbol))
		klass->child_set_property = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_CHILD_GET_PROP_FUNCTION,
					  &symbol))
		klass->child_get_property = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_CHILD_VERIFY_FUNCTION,
					  &symbol))
		klass->child_verify_property = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_GET_INTERNAL_CHILD_FUNCTION,
					  &symbol))
		klass->get_internal_child = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_ACTION_ACTIVATE_FUNCTION,
					  &symbol))
		klass->action_activate = symbol;
	
	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_CHILD_ACTION_ACTIVATE_FUNCTION,
					  &symbol))
		klass->child_action_activate = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_ACTION_SUBMENU_FUNCTION,
					  &symbol))
		klass->action_submenu = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_DEPENDS_FUNCTION,
					  &symbol))
		klass->depends = symbol;
	
	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_READ_WIDGET_FUNCTION,
					  &symbol))
		klass->read_widget = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_WRITE_WIDGET_FUNCTION,
					  &symbol))
		klass->write_widget = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_READ_CHILD_FUNCTION,
					  &symbol))
		klass->read_child = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_WRITE_CHILD_FUNCTION,
					  &symbol))
		klass->write_child = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_CREATE_EPROP_FUNCTION,
					  &symbol))
		klass->create_eprop = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_STRING_FROM_VALUE_FUNCTION,
					  &symbol))
		klass->string_from_value = symbol;

	if (glade_xml_load_sym_from_node (node, module,
					  GLADE_TAG_CREATE_EDITABLE_FUNCTION,
					  &symbol))
		klass->create_editable = symbol;

}

static void
gwa_derived_class_init (GladeWidgetAdaptorClass *adaptor_class,
			GWADerivedClassData *data)
{
	GladeXmlNode *node = data->node;
	GModule      *module = data->module;
	
	/* Load catalog symbols from module */
	if (module) gwa_extend_with_node_load_sym (adaptor_class, node, module);

	glade_xml_get_property_version
		(node, GLADE_TAG_BUILDER_SINCE, 
		 &adaptor_class->builder_since_major,
		 &adaptor_class->builder_since_minor);

	glade_xml_get_property_version
		(node, GLADE_TAG_VERSION_SINCE, 
		 &adaptor_class->version_since_major,
		 &adaptor_class->version_since_minor);

	adaptor_class->deprecated = 
		glade_xml_get_property_boolean
		(node, GLADE_TAG_DEPRECATED, adaptor_class->deprecated);

	adaptor_class->libglade_unsupported = 
		glade_xml_get_property_boolean
		(node, GLADE_TAG_LIBGLADE_UNSUPPORTED, adaptor_class->libglade_unsupported);

	adaptor_class->libglade_only = 
		glade_xml_get_property_boolean
		(node, GLADE_TAG_LIBGLADE_ONLY, adaptor_class->libglade_only);

	adaptor_class->fixed = 
		glade_xml_get_property_boolean
		(node, GLADE_TAG_FIXED, adaptor_class->fixed);

	adaptor_class->toplevel =
		glade_xml_get_property_boolean
		(node, GLADE_TAG_TOPLEVEL, adaptor_class->toplevel);

	adaptor_class->use_placeholders =
		glade_xml_get_property_boolean
		(node, GLADE_TAG_USE_PLACEHOLDERS, adaptor_class->use_placeholders);

	adaptor_class->default_width =
		glade_xml_get_property_int
		(node, GLADE_TAG_DEFAULT_WIDTH, adaptor_class->default_width);

	adaptor_class->default_height =
		glade_xml_get_property_int
		(node, GLADE_TAG_DEFAULT_HEIGHT, adaptor_class->default_height);
}

static GType
gwa_derive_adaptor_for_type (GType object_type, GWADerivedClassData *data)
{
	GladeWidgetAdaptor *adaptor;
	GType               iter_type, derived_type;
	GType               parent_type = GLADE_TYPE_WIDGET_ADAPTOR;
	gchar              *type_name;
	GTypeInfo adaptor_info = 
	{
		sizeof (GladeWidgetAdaptorClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) gwa_derived_class_init,
		(GClassFinalizeFunc) NULL,
		data,           /* class_data */
		sizeof (GladeWidgetAdaptor),
		0,              /* n_preallocs */
		(GInstanceInitFunc) gwa_derived_init,
	};
	
	for (iter_type = g_type_parent (object_type); 
	     iter_type > 0; 
	     iter_type = g_type_parent (iter_type))
	{
		if ((adaptor = 
		     glade_widget_adaptor_get_by_type (iter_type)) != NULL)
		{
			parent_type = G_TYPE_FROM_INSTANCE (adaptor);
			break;
		}
	}

	/* Note that we must pass ownership of the type_name string
	 * to the type system 
	 */
	type_name    = g_strdup_printf ("Glade%sAdaptor", g_type_name (object_type));
	derived_type = g_type_register_static (parent_type, type_name,
					       &adaptor_info, 0);
			
	return derived_type;
}


/*******************************************************************************
                                     API
 *******************************************************************************/
static void
accum_adaptor (gpointer            key,
	       GladeWidgetAdaptor *adaptor,
	       GList             **list)
{
	*list = g_list_prepend (*list, adaptor);
}

/**
 * glade_widget_adaptor_list_adaptors:
 *
 * Compiles a list of all registered adaptors.
 *
 * Returns: A newly allocated #GList which must be freed with g_list_free()
 */
GList *
glade_widget_adaptor_list_adaptors (void)
{
	GList *adaptors = NULL;

	g_hash_table_foreach (adaptor_hash, (GHFunc)accum_adaptor, &adaptors);
	
	return adaptors;
}

/**
 * glade_widget_adaptor_register:
 * @adaptor: A #GladeWidgetAdaptor
 *
 * Registers @adaptor into the Glade core (every supported
 * object type must have a registered adaptor).
 */
void
glade_widget_adaptor_register (GladeWidgetAdaptor *adaptor)
{

	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));

	if (glade_widget_adaptor_get_by_name (adaptor->name))
	{
		g_warning ("Adaptor class for '%s' already registered", 
			   adaptor->name);
		return;
	}

	if (!adaptor_hash)
		adaptor_hash = g_hash_table_new_full (g_direct_hash, g_direct_equal,
						      NULL, g_object_unref);

	g_hash_table_insert (adaptor_hash, GSIZE_TO_POINTER (adaptor->type), adaptor);
}

static GladePackingDefault *
gwa_default_from_child_packing (GladeChildPacking *packing, const gchar *id)
{
	GList *list;

	for (list = packing->packing_defaults; list; list = list->next)
	{
		GladePackingDefault *def = list->data;

		if (id && !strcmp (id, def->id))
			return def;
	}

	return NULL;
}

static GladeChildPacking *
glade_widget_adaptor_get_child_packing (GladeWidgetAdaptor *child_adaptor,
					const gchar        *parent_name)
{
	GList *l;

	for (l = child_adaptor->child_packings; l; l = l->next) 
	{
		GladeChildPacking *packing;

		packing = (GladeChildPacking *) l->data;

		if (!strcmp (packing->parent_name, parent_name))
			return packing;
	}

	return NULL;
}

static void
gwa_set_packing_defaults_from_node (GladeWidgetAdaptor *adaptor,
				    GladeXmlNode       *node)
{
        GladeXmlNode *child;

        for (child = glade_xml_node_get_children (node); 
	     child; child = glade_xml_node_next (child))
        {
                gchar             *name;
                GladeXmlNode      *prop_node;
		GladeChildPacking *packing;

                if (!glade_xml_node_verify (child, GLADE_TAG_PARENT_CLASS))
                        continue;

                if ((name = glade_xml_get_property_string_required
		     (child, GLADE_TAG_NAME, adaptor->name)) == NULL)
                        continue;

		/* If a GladeChildPacking exists for this parent, use it -
		 * otherwise prepend a new one
		 */
		if ((packing = 
		     glade_widget_adaptor_get_child_packing (adaptor, name)) == NULL)
		{

			packing = g_new0 (GladeChildPacking, 1);
			packing->parent_name = name;

			adaptor->child_packings = 
				g_list_prepend (adaptor->child_packings, packing);

		}

                for (prop_node = glade_xml_node_get_children (child); 
		     prop_node; prop_node = glade_xml_node_next (prop_node))
                {
			GladePackingDefault *def;
			gchar               *id;
			gchar               *value;

			if ((id = 
			     glade_xml_get_property_string_required
			     (prop_node, GLADE_TAG_ID, adaptor->name)) == NULL)
				continue;

			if ((value = 
			     glade_xml_get_property_string_required
			     (prop_node, GLADE_TAG_DEFAULT, adaptor->name)) == NULL)
			{
				g_free (id);
				continue;
			}

			if ((def = gwa_default_from_child_packing (packing, id)) == NULL)
			{
				def        = g_new0 (GladePackingDefault, 1);
				def->id    = id;
				def->value = value;

				packing->packing_defaults = 
					g_list_prepend (packing->packing_defaults, def);
			}
			else
			{
				g_free (id);
				g_free (def->value);
				def->value = value;
			}

			adaptor->child_packings = 
				g_list_prepend (adaptor->child_packings, packing);

                }
	}
}

static void
gwa_update_properties_from_node (GladeWidgetAdaptor  *adaptor,
				 GladeXmlNode        *node,
				 GModule             *module,
				 GList              **properties,
				 const gchar         *domain,
				 gboolean             is_packing)
{
	GladeXmlNode *child;

	for (child = glade_xml_node_get_children (node);
	     child; child = glade_xml_node_next (child))
	{
		gchar *id;
		GList *list;
		GladePropertyClass *property_class;
		gboolean updated;

		if (!glade_xml_node_verify (child, GLADE_TAG_PROPERTY))
			continue;

		id = glade_xml_get_property_string_required
			(child, GLADE_TAG_ID, adaptor->name);
		if (!id)
			continue;

		/* property names from catalogs also need to have the '-' form */
		glade_util_replace (id, '_', '-');
		
		/* find the property in our list, if not found append a new property */
		for (list = *properties; list && list->data; list = list->next)
		{
			property_class = GLADE_PROPERTY_CLASS (list->data);
			if (property_class->id != NULL &&
			    g_ascii_strcasecmp (id, property_class->id) == 0)
				break;
		}

		if (list)
		{
			property_class = GLADE_PROPERTY_CLASS (list->data);
		}
		else
		{
			property_class = glade_property_class_new (adaptor);
			property_class->id = g_strdup (id);

			/* When creating new virtual packing properties,
			 * make sure we mark them as such here. 
			 */
			if (is_packing)
				property_class->packing = TRUE;

			*properties = g_list_append (*properties, property_class);
			list = g_list_last (*properties);
		}

		if ((updated = glade_property_class_update_from_node
		     (child, module, adaptor->type,
		      &property_class, domain)) == FALSE)
		{
			g_warning ("failed to update %s property of %s from xml",
				   id, adaptor->name);
			g_free (id);
			continue;
		}

		/* if this pointer was set to null, its a property we dont handle. */
		if (!property_class)
			*properties = g_list_delete_link (*properties, list);

		g_free (id);
	}
}

static void
gwa_action_update_from_node (GladeWidgetAdaptor *adaptor,
			     gboolean is_packing,
			     GladeXmlNode *node,
			     const gchar *domain,
			     gchar *group_path)
{
	GladeXmlNode *child;
	gchar *id, *label, *stock, *action_path;
	gboolean group, important;
	
	for (child = glade_xml_node_get_children (node);
	     child; child = glade_xml_node_next (child))
	{
		if ((group = glade_xml_node_verify_silent (child, GLADE_TAG_ACTION)) == FALSE)
			continue;

		id = glade_xml_get_property_string_required
					(child,	GLADE_TAG_ID, adaptor->name);
		if (id == NULL)
			continue;

		if (group_path)
			action_path = g_strdup_printf ("%s/%s", group_path, id);
		else
			action_path = id;
		
		label = glade_xml_get_property_string (child, GLADE_TAG_NAME);
		stock = glade_xml_get_property_string (child, GLADE_TAG_STOCK);
		important = glade_xml_get_property_boolean (child, GLADE_TAG_IMPORTANT, FALSE);
		
		if (label) 
		{
			gchar *translated = dgettext (domain, label);
			if (label != translated)
			{
				g_free (label);
				label = g_strdup (translated);
			}
		}
		
		if (is_packing)
			glade_widget_adaptor_pack_action_add (adaptor, action_path, label, stock, important);
		else
			glade_widget_adaptor_action_add (adaptor, action_path, label, stock, important);
		
		if (group) gwa_action_update_from_node (adaptor, is_packing, child, domain, action_path);
		
		g_free (id);
		g_free (label);
		g_free (stock);
		if (group_path) g_free (action_path);
	}
}

static void
gwa_set_signals_from_node (GladeWidgetAdaptor *adaptor, 
			   GladeXmlNode       *node)
{
	GladeXmlNode     *child;
	GladeSignalClass *signal;
	GList            *list;
	gchar            *id;

	for (child = glade_xml_node_get_children (node);
	     child; child = glade_xml_node_next (child))
	{
		if (!glade_xml_node_verify (child, GLADE_TAG_SIGNAL))
			continue;

		if (!(id = glade_xml_get_property_string_required (child, GLADE_TAG_ID, NULL)))
			continue;

		if ((list = 
		     g_list_find_custom (adaptor->signals, id,
					 (GCompareFunc)gwa_signal_find_comp)) != NULL)
		{
			signal = list->data;
			glade_xml_get_property_version
				(child, GLADE_TAG_VERSION_SINCE, 
				 &signal->version_since_major,
				 &signal->version_since_minor);
		}
		g_free (id);
	}
}


static gboolean
gwa_extend_with_node (GladeWidgetAdaptor *adaptor, 
		      GladeXmlNode       *node,
		      GModule            *module,
		      const gchar        *domain)
{
	GladeXmlNode *child;
	gchar        *child_type;

	/* Override the special-child-type here */
	if ((child_type =
	     glade_xml_get_value_string (node, GLADE_TAG_SPECIAL_CHILD_TYPE)) != NULL)
		adaptor->priv->special_child_type =
			(g_free (adaptor->priv->special_child_type), child_type);
	
	if ((child = 
	     glade_xml_search_child (node, GLADE_TAG_PROPERTIES)) != NULL)
		gwa_update_properties_from_node
			(adaptor, child, module, &adaptor->properties, domain, FALSE);

	if ((child = 
	     glade_xml_search_child (node, GLADE_TAG_PACKING_PROPERTIES)) != NULL)
		gwa_update_properties_from_node
			(adaptor, child, module, &adaptor->packing_props, domain, TRUE);

	if ((child = 
	     glade_xml_search_child (node, GLADE_TAG_PACKING_DEFAULTS)) != NULL)
		gwa_set_packing_defaults_from_node (adaptor, child);

	if ((child = 
	     glade_xml_search_child (node, GLADE_TAG_SIGNALS)) != NULL)
		gwa_set_signals_from_node (adaptor, child);

	if ((child = 
	     glade_xml_search_child (node, GLADE_TAG_ACTIONS)) != NULL)
		gwa_action_update_from_node (adaptor, FALSE, child, domain, NULL);
	
	if ((child = 
	     glade_xml_search_child (node, GLADE_TAG_PACKING_ACTIONS)) != NULL)
		gwa_action_update_from_node (adaptor, TRUE, child, domain, NULL);
	
	return TRUE;
}

/** 
 * create_icon_name_for_object_class:
 * @class_name: The name of the widget class
 * @class_type: The #GType of the adaptor class
 * @icon_name:    The icon name as set from the catalog
 * @icon_prefix:  The icon prefix as set from the catalog
 * @generic_name: The generic name for the widget class
 *
 * Creates a suitable icon name for an object class, based on several parameters.
 *
 * Returns: An icon name, or NULL if the object class does not require one.
 */
static gchar *
create_icon_name_for_object_class (const gchar *class_name,
				   GType        class_type,
				   const gchar *icon_name,
				   const gchar *icon_prefix,
				   const gchar *generic_name)
{
	gchar *name;

	/* only certain object classes need to have icons */
	if (G_TYPE_IS_INSTANTIATABLE (class_type) == FALSE ||
            G_TYPE_IS_ABSTRACT (class_type) != FALSE ||
            generic_name == NULL)
	{
		return NULL;
	}
	
	/* if no icon name has been specified, we then fallback to a default icon name */
	if (!icon_name)
		name = g_strdup_printf ("widget-%s-%s", icon_prefix, generic_name);
	else
		name = g_strdup (icon_name);
	
	return name;
}

static void
gwa_displayable_values_check (GladeWidgetAdaptor *adaptor, gboolean packing)
{
	GList *l, *p = (packing) ? adaptor->packing_props : adaptor->properties;
	
	for (l = p; l; l = g_list_next (l))
	{
		GladePropertyClass *klass = l->data;
		
		if (adaptor->type == klass->pspec->owner_type && klass->visible &&
		    (G_IS_PARAM_SPEC_ENUM (klass->pspec) || G_IS_PARAM_SPEC_FLAGS (klass->pspec)) &&
		    !glade_type_has_displayable_values (klass->pspec->value_type) &&
		    klass->pspec->value_type != GLADE_TYPE_STOCK &&
		    klass->pspec->value_type != GLADE_TYPE_STOCK_IMAGE)
		{
			/* We do not need displayable values if the property is not visible */
			g_message ("No displayable values for %sproperty %s::%s", 
				   (packing) ? "child " : "",
				   adaptor->name, klass->id);
		}
	}
}

static GType
generate_type (const char *name, 
	       const char *parent_name)
{
	GType parent_type;
	GTypeQuery query;
	GTypeInfo *type_info;
	
	g_return_val_if_fail (name != NULL, 0);
	g_return_val_if_fail (parent_name != NULL, 0);
	
	parent_type = glade_util_get_type_from_name (parent_name, FALSE);
	g_return_val_if_fail (parent_type != 0, 0);
	
	g_type_query (parent_type, &query);
	g_return_val_if_fail (query.type != 0, 0);
	
	type_info = g_new0 (GTypeInfo, 1);
	type_info->class_size = query.class_size;
	type_info->instance_size = query.instance_size;
	
	return g_type_register_static (parent_type, name, type_info, 0);
}


/**
 * glade_widget_adaptor_from_catalog:
 * @catalog: A #GladeCatalog
 * @class_node: the #GladeXmlNode to load
 * @module: the plugin GModule.
 *
 * Dynamicly creates a subclass of #GladeWidgetAdaptor and subclasses
 * the closest parent adaptor (parent class adapters must be creates/registerd
 * prior to child classes, otherwise inheritance wont work) and parses in
 * the relevent catalog info.
 */
GladeWidgetAdaptor  *
glade_widget_adaptor_from_catalog (GladeCatalog         *catalog,
				   GladeXmlNode         *class_node,
				   GModule              *module)
{
	GladeWidgetAdaptor *adaptor = NULL;
	gchar              *name, *generic_name, *icon_name, *adaptor_icon_name, *adaptor_name, *func_name;
	gchar              *title, *translated_title, *parent_name;
	GType               object_type, adaptor_type, parent_type;
	gchar              *missing_icon = NULL;
	GWADerivedClassData data;

	
	if (!glade_xml_node_verify (class_node, GLADE_TAG_GLADE_WIDGET_CLASS))
	{
		g_warning ("Widget class node is not '%s'", 
			   GLADE_TAG_GLADE_WIDGET_CLASS);
		return NULL;
	}
	
	if ((name = glade_xml_get_property_string_required
	     (class_node, GLADE_TAG_NAME, NULL)) == NULL)
		return NULL;

	/* Either get the instance type by:
	 *
	 * - Autosubclassing a specified parent type (a fake widget class)
	 * - parsing the _get_type() function directly from the catalog
	 * - deriving foo_bar_get_type() from the name FooBar and loading that.
	 */
	if ((parent_name = glade_xml_get_property_string (class_node, GLADE_TAG_PARENT)) != NULL)
	{
		if (!glade_widget_adaptor_get_by_name (parent_name))
		{
			g_warning ("Trying to define class '%s' for parent class '%s', but parent class '%s' "
				   "is not registered", name, parent_name, parent_name);
			g_free (name);
			g_free (parent_name);
			return NULL;
		}
		object_type = generate_type (name, parent_name);
		g_free (parent_name);
	}
	else if ((func_name = glade_xml_get_property_string (class_node, GLADE_TAG_GET_TYPE_FUNCTION)) != NULL)
		object_type = glade_util_get_type_from_name (func_name, TRUE);
	else
		object_type = glade_util_get_type_from_name (name, FALSE);
	
	if (object_type == 0)
	{
		g_warning ("Failed to load the GType for '%s'", name);
		g_free (name);
		return NULL;
	}

	if (glade_widget_adaptor_get_by_type (object_type))
	{
		g_warning ("Adaptor class for '%s' already defined", 
			   g_type_name (object_type));
		g_free (name);
		return NULL;
	}

	if ((adaptor_name = glade_xml_get_property_string (class_node, GLADE_TAG_ADAPTOR)))
		adaptor_type = g_type_from_name (adaptor_name);
	else
	{
		/*
		 * We use this struct pointer to pass data to 
		 * gwa_derived_class_init() because we must override constructor()
		 * from the catalog before calling g_object_new() :P
		 */
		data.node = class_node;
		data.module = module;
		adaptor_type = gwa_derive_adaptor_for_type (object_type, &data);
	}
	
	if (adaptor_type == 0)
	{
		g_warning ("Failed to get %s's adaptor %s", name,
			   (adaptor_name) ? adaptor_name : "");
		g_free (adaptor_name);
		g_free (name);
		return NULL;
	}
	
	generic_name = glade_xml_get_property_string (class_node, GLADE_TAG_GENERIC_NAME);
	icon_name    = glade_xml_get_property_string (class_node, GLADE_TAG_ICON_NAME);
	
	/* get a suitable icon name for adaptor */
	adaptor_icon_name = 
		create_icon_name_for_object_class (name,
						   object_type,
						   icon_name,
						   glade_catalog_get_icon_prefix (catalog),
						   generic_name);

		
	/* check if icon is available (a null icon-name is an abstract class) */
	if (adaptor_icon_name && 
	    !gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), adaptor_icon_name))
	{
		GladeWidgetAdaptor *parent = 
			glade_widget_adaptor_get_parent_adaptor_by_type (object_type);

		/* Save the desired name */
		missing_icon = adaptor_icon_name;
		
		adaptor_icon_name = g_strdup ((parent && parent->icon_name) ?
					      parent->icon_name : DEFAULT_ICON_NAME);

	}

	adaptor = g_object_new (adaptor_type, 
				"type", object_type,
				"name", name,
				"catalog", glade_catalog_get_name (catalog),
				"generic-name", generic_name,
				"icon-name", adaptor_icon_name,
				NULL);

	adaptor->missing_icon = missing_icon;
				
	g_free (generic_name);
	g_free (icon_name);
	g_free (adaptor_icon_name);


	title = glade_xml_get_property_string_required (class_node,
							GLADE_TAG_TITLE,
	      						"This value is needed to "
	      						"display object class names "
	      						"in the UI");
	if (title == NULL)
	{
		g_warning ("Class '%s' declared without a '%s' attribute", name, GLADE_TAG_TITLE);
		adaptor->title = g_strdup (name);
	}
	else
	{
		/* translate */
		translated_title = dgettext (glade_catalog_get_domain (catalog), title);
		if (translated_title != title)
		{
			/* gettext owns translated_title */
			adaptor->title = g_strdup (translated_title);
			g_free (title);
		}
		else
		{
			adaptor->title = title;
		}
	}

	if (G_TYPE_IS_INSTANTIATABLE (adaptor->type)    &&
	    G_TYPE_IS_ABSTRACT (adaptor->type) == FALSE &&
	    adaptor->generic_name == NULL)
	{
		g_warning ("Instantiatable class '%s' built without a '%s'",
			   name, GLADE_TAG_GENERIC_NAME);
	}

	/* Perform a stoopid fallback just incase */
	if (adaptor->generic_name == NULL)
		adaptor->generic_name = g_strdup ("widget");
	
	adaptor->priv->catalog = g_strdup (glade_catalog_get_name (catalog));

	if (glade_catalog_get_book (catalog))
		adaptor->priv->book = g_strdup (glade_catalog_get_book (catalog));

	gwa_extend_with_node (adaptor, class_node, module, glade_catalog_get_domain (catalog));

	if (!glade_catalog_supports_libglade (catalog))
		GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->libglade_unsupported = TRUE;
	if (!glade_catalog_supports_gtkbuilder (catalog))
		GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->libglade_only = TRUE;

	/* Set default weight on properties */
	for (parent_type = adaptor->type;
	     parent_type != 0;
	     parent_type = g_type_parent (parent_type))
	{
		gwa_properties_set_weight (&adaptor->properties, parent_type);
		gwa_properties_set_weight (&adaptor->packing_props, parent_type);
	}
	
	gwa_displayable_values_check (adaptor, FALSE);
	gwa_displayable_values_check (adaptor, TRUE);
	
	glade_widget_adaptor_register (adaptor);

	return adaptor;
}

/**
 * glade_widget_adaptor_create_internal:
 * @parent:            The parent #GladeWidget, or %NULL for children
 *                     outside of the hierarchy.
 * @internal_object:   the #GObject
 * @internal_name:     a string identifier for this internal widget.
 * @parent_name:       the generic name of the parent used for fancy child names.
 * @anarchist:         Whether or not this widget is a widget outside
 *                     of the parent's hierarchy (like a popup window)
 * @reason:            The #GladeCreateReason for which this internal widget
 *                     was created (usually just pass the reason from the post_create
 *                     function; note also this is used only by the plugin code so
 *                     pass something usefull here).
 *
 * A convenienve function to create a #GladeWidget of the prescribed type
 * for internal widgets.
 *
 * Returns: a freshly created #GladeWidget wrapper object for the
 *          @internal_object of name @internal_name
 */
GladeWidget *
glade_widget_adaptor_create_internal  (GladeWidget      *parent,
				       GObject          *internal_object,
				       const gchar      *internal_name,
				       const gchar      *parent_name,
				       gboolean          anarchist,
				       GladeCreateReason reason)
{
	GladeWidgetAdaptor *adaptor;
	GladeProject       *project;

	g_return_val_if_fail (GLADE_IS_WIDGET (parent), NULL);
	project = glade_widget_get_project (parent);

        if ((adaptor = glade_widget_adaptor_get_by_name
	     (G_OBJECT_TYPE_NAME (internal_object))) == NULL)
	{
		g_critical ("Unable to find widget class for type %s", 
			    G_OBJECT_TYPE_NAME (internal_object));
		return NULL;
	}

	return glade_widget_adaptor_create_widget (adaptor, FALSE,
						   "anarchist", anarchist,
						   "parent", parent,
						   "project", project,
						   "internal", internal_name,
						   "internal-name", parent_name,
						   "reason", reason,
						   "object", internal_object,
						   NULL);
}

/**
 * glade_widget_adaptor_create_widget:
 * @adaptor: a #GladeWidgetAdaptor
 * @query:   whether to display query dialogs if
 *           applicable to the class
 * @...:     a %NULL terminated list of string/value pairs of #GladeWidget 
 *           properties
 *
 *
 * This factory function returns a new #GladeWidget of the correct type/class
 * with the properties defined in @... and queries the user if nescisary.
 *
 * The resulting object will have all default properties applied to it
 * including the overrides specified in the catalog, unless the catalog
 * has specified 'ignore' for that property.
 *
 * Note that the widget class must be fed twice; once as the
 * leading arg... and also as the property for the #GladeWidget
 *
 * this macro returns the newly created #GladeWidget
 */
GladeWidget *
glade_widget_adaptor_create_widget_real (gboolean          query, 
					 const gchar      *first_property,
					 ...)
{
	GladeWidgetAdaptor *adaptor;
	GType               gwidget_type;
	GladeWidget        *gwidget;
	va_list             vl, vl_copy;

	g_return_val_if_fail (strcmp (first_property, "adaptor") == 0, NULL);

	va_start (vl, first_property);
	va_copy (vl_copy, vl);

	adaptor = va_arg (vl, GladeWidgetAdaptor *);

	va_end (vl);

	if (GLADE_IS_WIDGET_ADAPTOR (adaptor) == FALSE)
	{
		g_critical ("No adaptor found in glade_widget_adaptor_create_widget_real args");
		va_end (vl_copy);
		return NULL;
	}

	if (GWA_IS_FIXED (adaptor))
		gwidget_type = GLADE_TYPE_FIXED;
	else 
		gwidget_type = GLADE_TYPE_WIDGET;


	gwidget = (GladeWidget *)g_object_new_valist (gwidget_type,
						      first_property, 
						      vl_copy);
	va_end (vl_copy);
	
	if (query && glade_widget_adaptor_query (adaptor))
	{
		GladeEditor *editor = glade_app_get_editor ();
		
		/* If user pressed cancel on query popup. */
		if (!glade_editor_query_dialog (editor, gwidget))
		{
			g_object_unref (G_OBJECT (gwidget));
			return NULL;
		}
	}

	return gwidget;
}

/**
 * glade_widget_adaptor_get_by_name:
 * @name: name of the widget class (for instance: GtkButton)
 *
 * Returns: an existing #GladeWidgetAdaptor with the name equaling @name,
 *          or %NULL if such a class doesn't exist
 **/
GladeWidgetAdaptor  *
glade_widget_adaptor_get_by_name (const gchar  *name)
{
	GType type = g_type_from_name (name);
	
	if (adaptor_hash != NULL && type)
		return g_hash_table_lookup (adaptor_hash, GSIZE_TO_POINTER (type));

	return NULL;
}


/**
 * glade_widget_adaptor_get_by_type:
 * @type: the #GType of an object class
 *
 * Returns: an existing #GladeWidgetAdaptor with the type equaling @type,
 *          or %NULL if such a class doesn't exist
 **/
GladeWidgetAdaptor  *
glade_widget_adaptor_get_by_type (GType  type)
{
	if (adaptor_hash != NULL)
		return g_hash_table_lookup (adaptor_hash, GSIZE_TO_POINTER (type));
	else
		return NULL;
}

/**
 * glade_widget_adaptor_from_pspec:
 * @adaptor: a #GladeWidgetAdaptor
 * @pspec: a #GParamSpec
 *
 * Assumes @pspec is a property in an object class wrapped by @adaptor,
 * this function will search for the specific parent adaptor class which
 * originally introduced @pspec.
 *
 * Returns: the closest #GladeWidgetAdaptor in the ancestry to @adaptor
 *          which is responsable for introducing @pspec.
 **/
GladeWidgetAdaptor  *
glade_widget_adaptor_from_pspec (GladeWidgetAdaptor *adaptor,
				 GParamSpec         *spec)
{
	GladeWidgetAdaptor *spec_adaptor;
	GType spec_type = spec->owner_type;

	if (!spec_type)
		return adaptor;

	spec_adaptor = glade_widget_adaptor_get_by_type (spec->owner_type);

	g_return_val_if_fail (g_type_is_a (adaptor->type, spec->owner_type), NULL);

	while (spec_type && !spec_adaptor && spec_type != adaptor->type)
	{
		spec_type = g_type_parent (spec_type);
		spec_adaptor = glade_widget_adaptor_get_by_type (spec_type);
	}
	
	if (spec_adaptor)
		return spec_adaptor;

	return adaptor;
}

/**
 * glade_widget_adaptor_get_property_class:
 * @adaptor: a #GladeWidgetAdaptor
 * @name: a string
 *
 * Retrieves the #GladePropertyClass for @name in @adaptor
 *
 * Returns: A #GladePropertyClass object
 */
GladePropertyClass  *
glade_widget_adaptor_get_property_class (GladeWidgetAdaptor *adaptor,
					 const gchar        *name)
{
	GList *list;
	GladePropertyClass *pclass;

	for (list = adaptor->properties; list && list->data; list = list->next)
	{
		pclass = list->data;
		if (strcmp (pclass->id, name) == 0)
			return pclass;
	}
	return NULL;
}

/**
 * glade_widget_adaptor_get_pack_property_class:
 * @adaptor: a #GladeWidgetAdaptor
 * @name: a string
 *
 * Retrieves the #GladePropertyClass for @name in 
 * @adaptor's child properties
 * 
 * Returns: A #GladePropertyClass object
 */
GladePropertyClass  *
glade_widget_adaptor_get_pack_property_class (GladeWidgetAdaptor *adaptor,
					      const gchar        *name)
{
	GList *list;
	GladePropertyClass *pclass;

	for (list = adaptor->packing_props; list && list->data; list = list->next)
	{
		pclass = list->data;
		if (strcmp (pclass->id, name) == 0)
			return pclass;
	}
	return NULL;
}

/**
 * glade_widget_class_default_params:
 * @adaptor: a #GladeWidgetAdaptor
 * @construct: whether to return construct params or not construct params
 * @n_params: return location if any defaults are specified for this class.
 * 
 * Returns: A list of params for use in g_object_newv ()
 */
GParameter *
glade_widget_adaptor_default_params (GladeWidgetAdaptor *adaptor,
				     gboolean            construct,
				     guint              *n_params)
{
	GArray              *params;
	GObjectClass        *oclass;
	GParamSpec         **pspec;
	GladePropertyClass  *pclass;
	guint                n_props, i;

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (n_params != NULL, NULL);

	/* As a slight optimization, we never unref the class
	 */
	oclass = g_type_class_ref (adaptor->type);
	pspec  = g_object_class_list_properties (oclass, &n_props);
	params = g_array_new (FALSE, FALSE, sizeof (GParameter));

	for (i = 0; i < n_props; i++)
	{
		GParameter parameter = { 0, };

		pclass = glade_widget_adaptor_get_property_class
			(adaptor, pspec[i]->name);
		
		/* Ignore properties based on some criteria
		 */
		if (pclass == NULL       || /* Unaccounted for in the builder */
		    pclass->virt         || /* should not be set before 
					       GladeWidget wrapper exists */
		    pclass->ignore)         /* Catalog explicitly ignores the object */
			continue;

		if (construct &&
		    (pspec[i]->flags & 
		     (G_PARAM_CONSTRUCT|G_PARAM_CONSTRUCT_ONLY)) == 0)
			continue;
		else if (!construct &&
			 (pspec[i]->flags & 
			  (G_PARAM_CONSTRUCT|G_PARAM_CONSTRUCT_ONLY)) != 0)
			continue;


		if (g_value_type_compatible (G_VALUE_TYPE (pclass->def),
					     pspec[i]->value_type) == FALSE)
		{
			g_critical ("Type mismatch on %s property of %s",
				    parameter.name, adaptor->name);
			continue;
		}

		if (g_param_values_cmp (pspec[i], 
					pclass->def, 
					pclass->orig_def) == 0)
			continue;

		parameter.name = pspec[i]->name; /* These are not copied/freed */
		g_value_init (&parameter.value, pspec[i]->value_type);
		g_value_copy (pclass->def, &parameter.value);

		g_array_append_val (params, parameter);
	}
	g_free (pspec);

	*n_params = params->len;
	return (GParameter *)g_array_free (params, FALSE);
}

/**
 * glade_widget_adaptor_construct_object:
 * @adaptor: A #GladeWidgetAdaptor
 * @n_parameters: amount of construct parameters
 * @parameters: array of construct #GParameter args to create 
 *              the new object with.
 *
 * This function is called to construct a GObject instance for
 * a #GladeWidget of the said @adaptor. (provided for language 
 * bindings that may need to construct a wrapper object).
 *
 * Returns: A newly created #GObject
 */
GObject *
glade_widget_adaptor_construct_object (GladeWidgetAdaptor *adaptor,
				       guint               n_parameters,
				       GParameter         *parameters)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

	return GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->construct_object (adaptor, n_parameters, parameters);
}

/**
 * glade_widget_adaptor_post_create:
 * @adaptor:   A #GladeWidgetAdaptor
 * @object:    The #GObject
 * @reason:    The #GladeCreateReason that @object was created for
 *
 * An adaptor function to be called after the object is created
 */
void
glade_widget_adaptor_post_create (GladeWidgetAdaptor *adaptor,
				  GObject            *object,
				  GladeCreateReason   reason)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->type));

	/* Run post_create in 2 stages, one that chains up and all class adaptors
	 * in the hierarchy get a peek, another that is used to setup placeholders
	 * and things that differ from the child/parent implementations
	 */
	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->deep_post_create)
		GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->deep_post_create (adaptor, object, reason);
	
	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->post_create)
		GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->post_create (adaptor, object, reason);
	/* XXX Dont complain here if no implementation is found */
}

/**
 * glade_widget_adaptor_get_internal_child:
 * @adaptor:       A #GladeWidgetAdaptor
 * @object:        The #GObject
 * @internal_name: The string identifier of the internal object
 *
 * Retrieves the internal object @internal_name from @object
 *
 * Returns: The internal #GObject
 */
GObject *
glade_widget_adaptor_get_internal_child (GladeWidgetAdaptor *adaptor,
					 GObject            *object,
					 const gchar        *internal_name)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (G_IS_OBJECT (object), NULL);
	g_return_val_if_fail (internal_name != NULL, NULL);
	g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->type), NULL);

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->get_internal_child)
		return GLADE_WIDGET_ADAPTOR_GET_CLASS
			(adaptor)->get_internal_child (adaptor, object, internal_name);
	else
		g_critical ("No get_internal_child() support in adaptor %s", adaptor->name);

	return NULL;
}

/**
 * glade_widget_adaptor_set_property:
 * @adaptor:       A #GladeWidgetAdaptor
 * @object:        The #GObject
 * @property_name: The property identifier
 * @value:         The #GValue
 *
 * This delagate function is used to apply the property value on
 * the runtime object.
 *
 */
void
glade_widget_adaptor_set_property (GladeWidgetAdaptor *adaptor,
				   GObject            *object,
				   const gchar        *property_name,
				   const GValue       *value)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (property_name != NULL && value != NULL);
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->type));

	/* The base class provides an implementation */
	GLADE_WIDGET_ADAPTOR_GET_CLASS
		(adaptor)->set_property (adaptor, object, property_name, value);
}


/**
 * glade_widget_adaptor_get_property:
 * @adaptor:       A #GladeWidgetAdaptor
 * @object:        The #GObject
 * @property_name: The property identifier
 * @value:         The #GValue
 *
 * Gets @value of @property_name on @object.
 *
 */
void
glade_widget_adaptor_get_property (GladeWidgetAdaptor *adaptor,
				   GObject            *object,
				   const gchar        *property_name,
				   GValue             *value)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (property_name != NULL && value != NULL);
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->type));

	/* The base class provides an implementation */
	GLADE_WIDGET_ADAPTOR_GET_CLASS
		(adaptor)->get_property (adaptor, object, property_name, value);
}


/**
 * glade_widget_adaptor_verify_property:
 * @adaptor:       A #GladeWidgetAdaptor
 * @object:        The #GObject
 * @property_name: The property identifier
 * @value:         The #GValue
 *
 * This delagate function is always called whenever setting any
 * properties with the exception of load time, and copy/paste time
 * (basicly the two places where we recreate a hierarchy that we
 * already know "works") its basicly an optional backend provided
 * boundry checker for properties.
 *
 * Returns: whether or not its OK to set @value on @object, this function
 * will silently return TRUE if the class did not provide a verify function.
 */
gboolean
glade_widget_adaptor_verify_property    (GladeWidgetAdaptor *adaptor,
					 GObject            *object,
					 const gchar        *property_name,
					 const GValue       *value)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);
	g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
	g_return_val_if_fail (property_name != NULL && value != NULL, FALSE);
	g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->type), FALSE);

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->verify_property)
		return GLADE_WIDGET_ADAPTOR_GET_CLASS
			(adaptor)->verify_property (adaptor, object, property_name, value);

	return TRUE;
}

/**
 * glade_widget_adaptor_add:
 * @adaptor:   A #GladeWidgetAdaptor
 * @container: The #GObject container
 * @child:     The #GObject child
 *
 * Adds @child to @container.
 */
void
glade_widget_adaptor_add (GladeWidgetAdaptor *adaptor,
			  GObject            *container,
			  GObject            *child)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (container));
	g_return_if_fail (G_IS_OBJECT (child));
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->type));

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->add)
		GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->add (adaptor, container, child);
	else
		g_critical ("No add() support in adaptor %s", adaptor->name);
}


/**
 * glade_widget_adaptor_remove:
 * @adaptor:   A #GladeWidgetAdaptor
 * @container: The #GObject container
 * @child:     The #GObject child
 *
 * Removes @child from @container.
 */
void
glade_widget_adaptor_remove (GladeWidgetAdaptor *adaptor,
			     GObject            *container,
			     GObject            *child)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (container));
	g_return_if_fail (G_IS_OBJECT (child));
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->type));

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->remove)
		GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->remove (adaptor, container, child);
	else
		g_critical ("No remove() support in adaptor %s", adaptor->name);
}

/**
 * glade_widget_adaptor_get_children:
 * @adaptor:   A #GladeWidgetAdaptor
 * @container: The #GObject container
 *
 * Lists the children of @container.
 *
 * Returns: A #GList of children
 */
GList *
glade_widget_adaptor_get_children (GladeWidgetAdaptor *adaptor,
				   GObject            *container)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (G_IS_OBJECT (container), NULL);
	g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->type), NULL);

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->get_children)
		return GLADE_WIDGET_ADAPTOR_GET_CLASS
			(adaptor)->get_children (adaptor, container);

	/* Dont complain here if no implementation is found */

	return NULL;
}

/**
 * glade_widget_adaptor_has_child:
 * @adaptor:   A #GladeWidgetAdaptor
 * @container: The #GObject container
 * @child:     The #GObject child
 *
 * Returns: whether @child is infact inside @container.
 */
gboolean
glade_widget_adaptor_has_child (GladeWidgetAdaptor *adaptor,
				GObject            *container,
				GObject            *child)
{
	GList *list, *children = NULL;
	gboolean found = FALSE;

	children = glade_widget_adaptor_get_children (adaptor, container);

	for (list = children; list && list->data; list = list->next)
	{
		if (list->data == child)
		{
			found = TRUE;
			break;
		}
	}

	g_list_free (children);
	return found;
}

/**
 * glade_widget_adaptor_child_set_property:
 * @adaptor:       A #GladeWidgetAdaptor
 * @container:     The #GObject container
 * @child:         The #GObject child
 * @property_name: The id of the property
 * @value:         The @GValue
 *
 * Sets @child's packing property identified by @property_name to @value.
 */
void
glade_widget_adaptor_child_set_property (GladeWidgetAdaptor *adaptor,
					 GObject            *container,
					 GObject            *child,
					 const gchar        *property_name,
					 const GValue       *value)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (container));
	g_return_if_fail (G_IS_OBJECT (child));
	g_return_if_fail (property_name != NULL && value != NULL);
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->type));

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->child_set_property)
		GLADE_WIDGET_ADAPTOR_GET_CLASS
			(adaptor)->child_set_property (adaptor, container, child,
						       property_name, value);
	else
		g_critical ("No child_set_property() support in adaptor %s", adaptor->name);

}

/**
 * glade_widget_adaptor_child_get_property:
 * @adaptor:       A #GladeWidgetAdaptor
 * @container:     The #GObject container
 * @child:         The #GObject child
 * @property_name: The id of the property
 * @value:         The @GValue
 *
 * Gets @child's packing property identified by @property_name.
 */
void
glade_widget_adaptor_child_get_property (GladeWidgetAdaptor *adaptor,
					 GObject            *container,
					 GObject            *child,
					 const gchar        *property_name,
					 GValue             *value)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (container));
	g_return_if_fail (G_IS_OBJECT (child));
	g_return_if_fail (property_name != NULL && value != NULL);
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->type));

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->child_get_property)
		GLADE_WIDGET_ADAPTOR_GET_CLASS
			(adaptor)->child_get_property (adaptor, container, child,
						       property_name, value);
	else
		g_critical ("No child_set_property() support in adaptor %s", adaptor->name);
}

/**
 * glade_widget_adaptor_child_verify_property:
 * @adaptor:       A #GladeWidgetAdaptor
 * @container:     The #GObject container
 * @child:         The #GObject child
 * @property_name: The id of the property
 * @value:         The @GValue
 *
 * This delagate function is always called whenever setting any
 * properties with the exception of load time, and copy/paste time
 * (basicly the two places where we recreate a hierarchy that we
 * already know "works") its basicly an optional backend provided
 * boundry checker for properties.
 *
 * Returns: whether or not its OK to set @value on @object, this function
 * will silently return TRUE if the class did not provide a verify function.
 */
gboolean
glade_widget_adaptor_child_verify_property (GladeWidgetAdaptor *adaptor,
					    GObject            *container,
					    GObject            *child,
					    const gchar        *property_name,
					    const GValue       *value)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);
	g_return_val_if_fail (G_IS_OBJECT (container), FALSE);
	g_return_val_if_fail (G_IS_OBJECT (child), FALSE);
	g_return_val_if_fail (property_name != NULL && value != NULL, FALSE);
	g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->type), FALSE);

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->child_verify_property)
		return GLADE_WIDGET_ADAPTOR_GET_CLASS
			(adaptor)->child_verify_property (adaptor, 
							  container, child,
							  property_name, value);

	return TRUE;
}


/**
 * glade_widget_adaptor_replace_child:
 * @adaptor: A #GladeWidgetAdaptor
 * @container: The #GObject container
 * @old_obj: The old #GObject child
 * @new_obj: The new #GObject child
 *
 * Replaces @old_obj with @new_obj in @container while positioning
 * @new_obj where @old_obj was and assigning it appropriate packing 
 * property values.
 */
void
glade_widget_adaptor_replace_child (GladeWidgetAdaptor *adaptor,
				    GObject            *container,
				    GObject            *old_obj,
				    GObject            *new_obj)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (container));
	g_return_if_fail (G_IS_OBJECT (old_obj));
	g_return_if_fail (G_IS_OBJECT (new_obj));
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->type));

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->replace_child)
		GLADE_WIDGET_ADAPTOR_GET_CLASS 
			(adaptor)->replace_child (adaptor, container, old_obj, new_obj);
	else
		g_critical ("No replace_child() support in adaptor %s", adaptor->name);
}

/**
 * glade_widget_adaptor_query:
 * @adaptor: A #GladeWidgetAdaptor
 *
 * Returns: whether the user needs to be queried for
 * certain properties upon creation of this class.
 */
gboolean
glade_widget_adaptor_query (GladeWidgetAdaptor *adaptor)
{
	GladePropertyClass *pclass;
	GList *l;

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);

	for (l = adaptor->properties; l; l = l->next)
	{
		pclass = l->data;

		if (pclass->query)
			return TRUE;
	}

	return FALSE;
}

/**
 * glade_widget_adaptor_get_packing_default:
 * @child_adaptor:  A #GladeWidgetAdaptor
 * @container_adaptor: The #GladeWidgetAdaptor for the parent object
 * @id:    The string property identifier
 *
 * Gets the default value for @property_id on a widget governed by
 * @child_adaptor when parented in a widget governed by @parent_adaptor
 *
 * Returns: a string representing the default value for @property_id
 */
G_CONST_RETURN gchar *
glade_widget_adaptor_get_packing_default (GladeWidgetAdaptor *child_adaptor,
					  GladeWidgetAdaptor *container_adaptor,
					  const gchar        *id)
{
	GladeChildPacking *packing = NULL;
	GList             *l;

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (child_adaptor), NULL);
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (container_adaptor), NULL);

	if ((packing = 
	     glade_widget_adaptor_get_child_packing (child_adaptor,
						     container_adaptor->name)) != NULL)
	{
		for (l = packing->packing_defaults; l; l = l->next)
		{
			GladePackingDefault *def = l->data;
			
			if (strcmp (def->id, id) == 0)
				return def->value;
		}
	}
	return NULL;
}

/**
 * glade_widget_adaptor_is_container:
 * @adaptor: A #GladeWidgetAdaptor
 *
 * Checks whether or not this adaptor has support
 * to interface with child objects.
 *
 * Returns: whether or not @adaptor is a container
 */
gboolean
glade_widget_adaptor_is_container (GladeWidgetAdaptor *adaptor)
{

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);

	/* A GWA container must at least implement add/remove/get_children
	 */
	return 	(GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->add &&
		 GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->remove &&
		 GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->get_children);
}

static const gchar *
gwa_action_path_get_id (const gchar *action_path)
{
	const gchar *id;
	
	if ((id = g_strrstr (action_path, "/")) && id[1] != '\0')
		return &id[1];
	else
		return action_path;
}

static GWActionClass *
gwa_action_lookup (GList *actions, const gchar *action_id)
{
	GList *l;
	
	for (l = actions; l; l = g_list_next (l))
	{
		GWActionClass *action = l->data;
		if (strcmp (action->id, action_id) == 0)
			return action;
	}
	
	return NULL;
}

static GWActionClass *
gwa_action_get_last_group (GList *actions, const gchar *action_path)
{
	gchar **tokens = g_strsplit (action_path, "/", 0);
	GWActionClass *group = NULL;
	gint i;
	
	for (i = 0; tokens[i] && tokens[i+1]; i++)
	{
		if ((group = gwa_action_lookup (actions, tokens[i])) == NULL)
		{
			g_strfreev (tokens);
			return NULL;
		}
		actions = group->actions;
	}
	
	g_strfreev (tokens);
	return group;
}

static gboolean
glade_widget_adaptor_action_add_real (GList **list,
				      const gchar *action_path,
				      const gchar *label,
				      const gchar *stock,
				      gboolean important)
{
	GWActionClass *action, *group;
	const gchar *id;
	
	id = gwa_action_path_get_id (action_path);
	
	if ((group = gwa_action_get_last_group (*list, action_path)))
		list = &group->actions;

	if ((action = gwa_action_lookup (*list, id)))
	{
		/* Update parent's label/stock */
		if (label && action->label)
		{
			g_free (action->label);
			if (strcmp (label, "") == 0) label = NULL;
			action->label = (label) ? g_strdup (label) : NULL;
		}
		if (stock && action->stock)
		{
			g_free (action->stock);
			if (strcmp (stock, "") == 0) stock = NULL;
			action->stock = (stock) ? g_strdup (stock) : NULL;
		}
	}
	else
	{
		/* New Action */
		action = g_new0 (GWActionClass, 1);
		action->path = g_strdup (action_path);
		action->id = (gchar*) gwa_action_path_get_id (action->path);
	
		if (label && strcmp (label, "") == 0) label = NULL;
		if (stock && strcmp (stock, "") == 0) stock = NULL;
		
		action->label = (label) ? g_strdup (label) : NULL;
		action->stock = (stock) ? g_strdup (stock) : NULL;
	}
	
	action->important = important;
	
	*list = g_list_append (*list, action);
	
	return TRUE;
}

/**
 * glade_widget_adaptor_action_add:
 * @adaptor: A #GladeWidgetAdaptor
 * @action_path: The identifier of this action in the action tree
 * @label: A translated label to show in the UI for this action
 * @stock: If set, this stock item will be shown in the UI along side the label.
 * @important: if this action is important.
 *
 * Add an action to @adaptor.
 * If the action is present then it overrides label and stock
 *
 * Returns: whether or not the action was added/updated.
 */
gboolean
glade_widget_adaptor_action_add (GladeWidgetAdaptor *adaptor,
				 const gchar *action_path,
				 const gchar *label,
				 const gchar *stock,
				 gboolean important)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);
	g_return_val_if_fail (action_path != NULL, FALSE);
	
	return glade_widget_adaptor_action_add_real (&adaptor->actions,
						     action_path,
						     label,
						     stock,
						     important);
}

/**
 * glade_widget_adaptor_pack_action_add:
 * @adaptor: A #GladeWidgetAdaptor
 * @action_path: The identifier of this action in the action tree
 * @label: A translated label to show in the UI for this action
 * @stock: If set, this stock item will be shown in the UI along side the label.
 * @important: if this action is important.
 *
 * Add a packing action to @adaptor.
 * If the action is present then it overrides label and stock
 *
 * Returns: whether or not the action was added/updated.
 */
gboolean
glade_widget_adaptor_pack_action_add (GladeWidgetAdaptor *adaptor,
				      const gchar *action_path,
				      const gchar *label,
				      const gchar *stock,
				      gboolean important)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);
	g_return_val_if_fail (action_path != NULL, FALSE);
	
	return glade_widget_adaptor_action_add_real (&adaptor->packing_actions,
						     action_path,
						     label,
						     stock,
						     important);
}

static gboolean
glade_widget_adaptor_action_remove_real (GList **list, const gchar *action_path)
{
	GWActionClass *action, *group;
	const gchar *id;
	
	id = gwa_action_path_get_id (action_path);
	
	if ((group = gwa_action_get_last_group (*list, action_path)))
		list = &group->actions;

	if ((action = gwa_action_lookup (*list, id)) == NULL) return FALSE;
	
	*list = g_list_remove (*list, action);
	
	glade_widget_action_class_free (action);
	
	return TRUE;
}

/**
 * glade_widget_adaptor_action_remove:
 * @adaptor: A #GladeWidgetAdaptor
 * @action_path: The identifier of this action in the action tree
 *
 * Remove an @adaptor's action.
 *
 * Returns: whether or not the action was removed.
 */
gboolean
glade_widget_adaptor_action_remove (GladeWidgetAdaptor *adaptor,
				    const gchar *action_path)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);
	g_return_val_if_fail (action_path != NULL, FALSE);
	
	return glade_widget_adaptor_action_remove_real (&adaptor->actions, 
							action_path);
}

/**
 * glade_widget_adaptor_pack_action_remove:
 * @adaptor: A #GladeWidgetAdaptor
 * @action_path: The identifier of this action in the action tree
 *
 * Remove an @adaptor's packing action.
 *
 * Returns: whether or not the action was removed.
 */
gboolean
glade_widget_adaptor_pack_action_remove (GladeWidgetAdaptor *adaptor,
					 const gchar *action_path)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);
	g_return_val_if_fail (action_path != NULL, FALSE);
	
	return glade_widget_adaptor_action_remove_real (&adaptor->packing_actions, 
							action_path);
}


/**
 * glade_widget_adaptor_pack_actions_new:
 * @adaptor: A #GladeWidgetAdaptor
 *
 * Create a list of packing actions.
 *
 * Returns: a new list of GladeWidgetAction.
 */
GList *
glade_widget_adaptor_pack_actions_new (GladeWidgetAdaptor *adaptor)
{
	GList *l, *list = NULL;
	
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	
	for (l = adaptor->packing_actions; l; l = g_list_next (l))
	{
		GWActionClass *action = l->data;
		GObject *obj = g_object_new (GLADE_TYPE_WIDGET_ACTION,
					     "class", action, NULL);
		
		list = g_list_prepend (list, GLADE_WIDGET_ACTION (obj));
	}
	return g_list_reverse (list);
}

/**
 * glade_widget_adaptor_action_activate:
 * @adaptor:   A #GladeWidgetAdaptor
 * @object:    The #GObject
 * @action_path: The action identifier in the action tree
 *
 * An adaptor function to be called on widget actions.
 */
void
glade_widget_adaptor_action_activate (GladeWidgetAdaptor *adaptor,
				      GObject            *object,
				      const gchar        *action_path)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->type));

	GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->action_activate (adaptor, object, action_path);
}

/**
 * glade_widget_adaptor_child_action_activate:
 * @adaptor:   A #GladeWidgetAdaptor
 * @object:    The #GObject
 * @action_path: The action identifier in the action tree
 *
 * An adaptor function to be called on widget actions.
 */
void
glade_widget_adaptor_child_action_activate (GladeWidgetAdaptor *adaptor,
					    GObject            *container,
					    GObject            *object,
					    const gchar        *action_path)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (G_IS_OBJECT (container));
	g_return_if_fail (G_IS_OBJECT (object));
	g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->type));

	GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->child_action_activate (adaptor, container, object, action_path);
}

/**
 * glade_widget_adaptor_action_submenu:
 * @adaptor:   A #GladeWidgetAdaptor
 * @object:    The #GObject
 * @action_path: The action identifier in the action tree
 *
 * This delagate function is used to create dynamically customized
 * submenus. Called only for actions that dont have children.
 *
 * Returns: A newly created #GtkMenu or %NULL
 */
GtkWidget *
glade_widget_adaptor_action_submenu (GladeWidgetAdaptor *adaptor,
				     GObject            *object,
				     const gchar        *action_path)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (G_IS_OBJECT (object), NULL);
	g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->type), NULL);

	if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->action_submenu)
		return GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->action_submenu (adaptor, object, action_path);

	return NULL;
}

/**
 * glade_widget_adaptor_depends:
 * @adaptor: A #GladeWidgetAdaptor
 * @widget: A #GladeWidget of the adaptor
 * @another: another #GladeWidget
 *
 * Checks whether @widget depends on @another to be placed earlier in
 * the glade file.
 *
 * Returns: whether @widget depends on @another being parsed first in
 * the resulting glade file.
 */
gboolean
glade_widget_adaptor_depends (GladeWidgetAdaptor *adaptor,
			      GladeWidget        *widget,
			      GladeWidget        *another)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);
	g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);
	g_return_val_if_fail (GLADE_IS_WIDGET (another), FALSE);

	return GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->depends (adaptor, widget, another);
}

/**
 * glade_widget_adaptor_read_widget:
 * @adaptor: A #GladeWidgetAdaptor
 * @widget: The #GladeWidget
 * @node: The #GladeXmlNode
 *
 * This function is called to update @widget from @node 
 * when loading xml files.
 */
void
glade_widget_adaptor_read_widget (GladeWidgetAdaptor *adaptor,
				  GladeWidget        *widget,
				  GladeXmlNode       *node)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (node != NULL);

	GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->read_widget (adaptor, widget, node);
}


/**
 * glade_widget_adaptor_write_widget:
 * @adaptor: A #GladeWidgetAdaptor
 * @widget: The #GladeWidget
 * @context: The #GladeXmlContext
 * @node: The #GladeXmlNode
 *
 * This function is called to write @widget to @node 
 * when writing xml files.
 */
void
glade_widget_adaptor_write_widget (GladeWidgetAdaptor *adaptor,
				   GladeWidget        *widget,
				   GladeXmlContext    *context,
				   GladeXmlNode       *node)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (node != NULL);

	GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->write_widget (adaptor, widget, 
								context, node);
}


/**
 * glade_widget_adaptor_read_child:
 * @adaptor: A #GladeWidgetAdaptor
 * @widget: The #GladeWidget
 * @node: The #GladeXmlNode
 *
 * This function is called to update load a child @widget 
 * from @node when loading xml files (will recurse into
 * glade_widget_read())
 */
void
glade_widget_adaptor_read_child (GladeWidgetAdaptor *adaptor,
				 GladeWidget        *widget,
				 GladeXmlNode       *node)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (node != NULL);

	GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->read_child (adaptor, widget, node);
}


/**
 * glade_widget_adaptor_write_child:
 * @adaptor: A #GladeWidgetAdaptor
 * @widget: The #GladeWidget
 * @context: The #GladeXmlContext
 * @node: The #GladeXmlNode
 *
 * This function is called to write the child @widget to @node 
 * when writing xml files (takes care of packing and recurses
 * into glade_widget_write())
 */
void
glade_widget_adaptor_write_child (GladeWidgetAdaptor *adaptor,
				  GladeWidget        *widget,
				  GladeXmlContext    *context,
				  GladeXmlNode       *node)
{
	g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
	g_return_if_fail (GLADE_IS_WIDGET (widget));
	g_return_if_fail (node != NULL);

	GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->write_child (adaptor, widget, 
							       context, node);
}


/**
 * glade_widget_adaptor_create_eprop:
 * @adaptor: A #GladeWidgetAdaptor
 * @klass: The #GladePropertyClass to be edited
 * @use_command: whether to use the GladeCommand interface
 * to commit property changes
 * 
 * Creates a GladeEditorProperty to edit @klass
 *
 * Returns: A newly created #GladeEditorProperty
 */
GladeEditorProperty *
glade_widget_adaptor_create_eprop (GladeWidgetAdaptor *adaptor,
				   GladePropertyClass *klass,
				   gboolean            use_command)
{
	GladeEditorProperty *eprop;
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (klass), NULL);

	eprop = GLADE_WIDGET_ADAPTOR_GET_CLASS
		(adaptor)->create_eprop (adaptor, klass, use_command);

	/* XXX we really need to print a g_error() here, exept we are
	 * now using this func to test for unsupported properties
	 * at init time from glade-property-class */

	return eprop;
}



/**
 * glade_widget_adaptor_create_eprop_by_name:
 * @adaptor: A #GladeWidgetAdaptor
 * @property_id: the string if of the coresponding #GladePropertyClass to be edited
 * @packing: whether this reffers to a packing property
 * @use_command: whether to use the GladeCommand interface
 * to commit property changes
 * 
 * Creates a #GladeEditorProperty to edit #GladePropertyClass @name in @adaptor
 *
 * Returns: A newly created #GladeEditorProperty
 */
GladeEditorProperty *
glade_widget_adaptor_create_eprop_by_name (GladeWidgetAdaptor *adaptor,
					   const gchar        *property_id,
					   gboolean            packing,
					   gboolean            use_command)
{
	GladePropertyClass *klass;
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (property_id && property_id[0], NULL);

	if (packing)
		klass = glade_widget_adaptor_get_pack_property_class (adaptor, property_id);
	else
		klass = glade_widget_adaptor_get_property_class (adaptor, property_id);

	g_return_val_if_fail (klass != NULL, NULL);

	return GLADE_WIDGET_ADAPTOR_GET_CLASS
		(adaptor)->create_eprop (adaptor, klass, use_command);
}


/**
 * glade_widget_adaptor_string_from_value:
 * @adaptor: A #GladeWidgetAdaptor
 * @klass: The #GladePropertyClass 
 * @value: The #GValue to convert to a string
 * @fmt: The #GladeProjectFormat the string should conform to
 * 
 * For normal properties this is used to serialize
 * property values, for custom properties its still
 * needed to update the UI for undo/redo items etc.
 *
 * Returns: A newly allocated string representation of @value
 */
gchar *
glade_widget_adaptor_string_from_value (GladeWidgetAdaptor *adaptor,
					GladePropertyClass *klass,
					const GValue       *value,
					GladeProjectFormat  fmt)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (klass), NULL);
	g_return_val_if_fail (value != NULL, NULL);

	return GLADE_WIDGET_ADAPTOR_GET_CLASS
		(adaptor)->string_from_value (adaptor, klass, value, fmt);
}


/**
 * glade_widget_adaptor_get_signal_class:
 * @adaptor: A #GladeWidgetAdaptor
 * @name: the name of the signal class.
 * 
 * Looks up signal class @name on @adaptor.
 *
 * Returns: a #GladeSignalClass or %NULL
 */
GladeSignalClass *
glade_widget_adaptor_get_signal_class (GladeWidgetAdaptor *adaptor,
				       const gchar        *name)
{
	GList *list;
	GladeSignalClass *signal;

	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	for (list = adaptor->signals; list; list = list->next)
	{
		signal = list->data;
		if (!strcmp (signal->name, name)) 
			return signal;
	}

	return NULL;
}


/**
 * glade_widget_adaptor_create_editable:
 * @adaptor: A #GladeWidgetAdaptor
 * @type: The #GladeEditorPageType
 * 
 * This is used to allow the backend to override the way an
 * editor page is layed out (note that editor widgets are created
 * on demand and not at startup).
 *
 * Returns: A new #GladeEditable widget
 */
GladeEditable *
glade_widget_adaptor_create_editable (GladeWidgetAdaptor   *adaptor,
				      GladeEditorPageType   type)
{
	g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

	return GLADE_WIDGET_ADAPTOR_GET_CLASS
		(adaptor)->create_editable (adaptor, type);
}
