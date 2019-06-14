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
#include "glade-cursor.h"
#include "glade-private.h"

/* For g_file_exists */
#include <sys/types.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <ctype.h>

#define DEFAULT_ICON_NAME "widget-gtk-frame"

struct _GladeWidgetAdaptorPrivate
{
  GType        type;                /* GType of the widget */
  GType        real_type;

  gchar       *name;                /* Name of the widget, for example GtkButton */
  gchar       *generic_name;        /* Used to generate names of new widgets, for
                                     * example "button" so that we generate button1,
                                     * button2, buttonX ..
                                     */
  gchar       *icon_name;           /* icon name to use for widget class */
  gchar       *missing_icon;        /* the name of the missing icon if it was not found */

  gchar       *title;               /* Translated class name used in the UI */
  GList       *properties;          /* List of GladePropertyClass objects.
                                     * [see glade-property.h] this list contains
                                     * properties about the widget that we are going
                                     * to modify. Like "title", "label", "rows" .
                                     * Each property creates an input in the propety
                                     * editor.
                                     */
  GList       *packing_props;       /* List of GladePropertyClass objects that describe
                                     * properties for child objects packing definition -
                                     * note there may be more than one type of child supported
                                     * by a widget and thus they may have different sets
                                     * of properties for each type - this association is
                                     * managed on the GladePropertyClass proper.
                                     */
  GList       *signals;              /* List of GladeSignalDef objects */
  GList       *child_packings;       /* Default packing property values */
  GList       *actions;              /* A list of GWActionClass */
  GList       *packing_actions;      /* A list of GWActionClass for child objects */
  GList       *internal_children;    /* A list of GladeInternalChild */
  gchar       *catalog;              /* The name of the widget catalog this class
                                      * was declared by.
                                      */
  gchar       *book;                 /* DevHelp search namespace for this widget class
                                      */

  GdkCursor   *cursor;                /* a cursor for inserting widgets */

  gchar       *special_child_type;    /* Special case code for children that
                                       * are special children (like notebook tab 
                                       * widgets for example).
                                       */
  gboolean     query;                 /* Do we have to query the user, see glade_widget_adaptor_query() */
};

struct _GladeChildPacking
{
  gchar *parent_name;
  GList *packing_defaults;
};


struct _GladePackingDefault
{
  gchar *id;
  gchar *value;
};

struct _GladeInternalChild
{
  gchar *name;
  gboolean anarchist;
  GList *children;
};

enum
{
  PROP_0,
  PROP_NAME,
  PROP_TYPE,
  PROP_TITLE,
  PROP_GENERIC_NAME,
  PROP_ICON_NAME,
  PROP_CATALOG,
  PROP_BOOK,
  PROP_SPECIAL_TYPE,
  PROP_CURSOR,
  PROP_QUERY
};

typedef struct _GladeChildPacking GladeChildPacking;
typedef struct _GladePackingDefault GladePackingDefault;
typedef struct _GladeInternalChild GladeInternalChild;

static GHashTable *adaptor_hash = NULL;

/* This object used to be registered as GladeGObjectAdaptor but there is
 * no reason for it since the autogenerated class for GtWidget is GladeGtkWidgetAdaptor
 * TODO: rename GladeWidgetAdaptor to GladeGObjectAdator or GladeObjectAdator
 */
G_DEFINE_TYPE_WITH_PRIVATE (GladeWidgetAdaptor, glade_widget_adaptor, G_TYPE_OBJECT);

/*******************************************************************************
                              Helper functions
 *******************************************************************************/

static void
gwa_create_cursor (GladeWidgetAdaptor *adaptor)
{
  GdkPixbuf *tmp_pixbuf, *widget_pixbuf;
  const GdkPixbuf *add_pixbuf;
  GdkDisplay *display;
  GError *error = NULL;

  /* only certain widget classes need to have cursors */
  if (G_TYPE_IS_INSTANTIATABLE (adaptor->priv->type) == FALSE ||
      G_TYPE_IS_ABSTRACT (adaptor->priv->type) != FALSE ||
      adaptor->priv->generic_name == NULL)
    return;

  /* cannot continue if 'add widget' cursor pixbuf has not been loaded for any reason */
  if ((add_pixbuf = glade_cursor_get_add_widget_pixbuf ()) == NULL)
    return;

  display = gdk_display_get_default ();

  /* create a temporary pixbuf clear to transparent black */
  tmp_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 32, 32);
  gdk_pixbuf_fill (tmp_pixbuf, 0x00000000);

  if (gtk_icon_theme_has_icon
      (gtk_icon_theme_get_default (), adaptor->priv->icon_name))
    {
      widget_pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                                adaptor->priv->icon_name,
                                                22, 0, &error);

      if (error)
        {
          g_warning ("Could not load image data for named icon '%s': %s",
                     adaptor->priv->icon_name, error->message);
          g_error_free (error);
          return;
        }

    }
  else
    {
      return;
    }

  /* composite pixbufs */
  gdk_pixbuf_composite (widget_pixbuf, tmp_pixbuf,
                        8, 8, 22, 22, 8, 8, 1, 1, GDK_INTERP_NEAREST, 255);

  gdk_pixbuf_composite (add_pixbuf, tmp_pixbuf,
                        0, 0, 12, 12, 0, 0, 1, 1, GDK_INTERP_NEAREST, 255);


  adaptor->priv->cursor =
      gdk_cursor_new_from_pixbuf (display, tmp_pixbuf, 6, 6);

  g_object_unref (tmp_pixbuf);
  g_object_unref (widget_pixbuf);
}

static gboolean
glade_widget_adaptor_hash_find (gpointer key,
                                gpointer value, 
                                gpointer user_data)
{
  GladeWidgetAdaptor *adaptor = value;
  GType *type = user_data;

  if (g_type_is_a (adaptor->priv->type, *type))
    {
      *type = adaptor->priv->type;
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

      g_hash_table_find (adaptor_hash, glade_widget_adaptor_hash_find, &retval);
      if (retval != type)
        g_error (_("A derived adaptor (%s) of %s already exist!"),
                 g_type_name (retval), g_type_name (type));
    }
}

static GladeInternalChild *
gwa_internal_child_find (GList *list, const gchar *name)
{
  GList *l;

  for (l = list; l; l = g_list_next (l))
    {
      GladeInternalChild *data = l->data;

      if (strcmp (data->name, name) == 0)
        return data;
      
      if (data->children)
        {
          GladeInternalChild *child;
          if ((child = gwa_internal_child_find (data->children, name)))
            return child;
        }
    }
  
  return NULL;
}

/*******************************************************************************
                     Base Object Implementation detail
 *******************************************************************************/
#define gwa_get_parent_adaptor(a) glade_widget_adaptor_get_parent_adaptor (a)

static GladeWidgetAdaptor *
glade_widget_adaptor_get_parent_adaptor_by_type (GType adaptor_type)
{
  GladeWidgetAdaptor *parent_adaptor = NULL;
  GType iter_type;

  for (iter_type = g_type_parent (adaptor_type);
       iter_type > 0; iter_type = g_type_parent (iter_type))
    {
      if ((parent_adaptor =
           glade_widget_adaptor_get_by_type (iter_type)) != NULL)
        return parent_adaptor;
    }

  return NULL;
}

/**
 * glade_widget_adaptor_get_parent_adaptor:
 * @adaptor: a #GladeWidgetAdaptor
 *
 * Returns: (transfer none): the parent #GladeWidgetAdaptor according to @adaptor type
 */
GladeWidgetAdaptor *
glade_widget_adaptor_get_parent_adaptor (GladeWidgetAdaptor *adaptor)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  return glade_widget_adaptor_get_parent_adaptor_by_type (adaptor->priv->type);
}

gboolean
glade_widget_adaptor_has_internal_children (GladeWidgetAdaptor *adaptor)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);
  return adaptor->priv->internal_children != NULL;
}

static gint
gwa_signal_comp (gpointer a, gpointer b)
{
  GladeSignalDef *signal_a = a, *signal_b = b;

  return strcmp (glade_signal_def_get_name (signal_b), 
                 glade_signal_def_get_name (signal_a));
}

static gint
gwa_signal_find_comp (gpointer a, gpointer b)
{
  GladeSignalDef *signal = a;
  gchar *name = b;
  return strcmp (name, glade_signal_def_get_name (signal));
}

static void
gwa_add_signals (GladeWidgetAdaptor *adaptor, GList **signals, GType type)
{
  guint               count, *sig_ids, num_signals;
  GladeWidgetAdaptor *type_adaptor;
  GladeSignalDef     *signal;
  GList              *list = NULL;

  type_adaptor = glade_widget_adaptor_get_by_type (type);

  sig_ids = g_signal_list_ids (type, &num_signals);

  for (count = 0; count < num_signals; count++)
    {
      signal = glade_signal_def_new (type_adaptor ? 
                                       type_adaptor : adaptor,
                                       type, sig_ids[count]);

      list = g_list_prepend (list, signal);
    }
  g_free (sig_ids);

  list = g_list_sort (list, (GCompareFunc)gwa_signal_comp);
  *signals = g_list_concat (list, *signals);
}

static GList *
gwa_list_signals (GladeWidgetAdaptor *adaptor, GType real_type)
{
  GList *signals = NULL;
  GType type, parent, *i, *p;

  g_return_val_if_fail (real_type != 0, NULL);

  for (type = real_type; g_type_is_a (type, G_TYPE_OBJECT); type = parent)
    {
      parent = g_type_parent (type);

      /* Add class signals */
      gwa_add_signals (adaptor, &signals, type);

      /* Add class interfaces signals */
      for (i = p = g_type_interfaces (type, NULL); *i; i++)
        if (!g_type_is_a (parent, *i))
          gwa_add_signals (adaptor, &signals, *i);

      g_free (p);
    }

  return g_list_reverse (signals);
}

static GList *
gwa_clone_parent_properties (GladeWidgetAdaptor *adaptor, gboolean is_packing)
{
  GladeWidgetAdaptor *parent_adaptor;
  GList *properties = NULL, *list, *proplist;

  if ((parent_adaptor = gwa_get_parent_adaptor (adaptor)) != NULL)
    {
      gboolean reset_version;

      proplist = is_packing ?
          parent_adaptor->priv->packing_props : parent_adaptor->priv->properties;

      /* Reset versioning in derived catalogs just once */
      reset_version = strcmp (adaptor->priv->catalog, parent_adaptor->priv->catalog) != 0;

      for (list = proplist; list; list = list->next)
        {
          GladePropertyClass *pclass = glade_property_class_clone (list->data, reset_version);

          glade_property_class_set_adaptor (pclass, adaptor);

          properties = g_list_prepend (properties, pclass);
        }
    }

  return g_list_reverse (properties);
}

static void
gwa_setup_introspected_props_from_pspecs (GladeWidgetAdaptor *adaptor,
                                          GParamSpec        **specs,
                                          gint                n_specs,
                                          gboolean            is_packing)
{
  GladeWidgetAdaptor *parent_adaptor = gwa_get_parent_adaptor (adaptor);
  GladePropertyClass *property_class;
  gint i;
  GList *list = NULL;

  for (i = 0; i < n_specs; i++)
    {
      if (parent_adaptor == NULL ||
          /* Dont create it if it already exists */
          (!is_packing &&
           !glade_widget_adaptor_get_property_class (parent_adaptor,
                                                     specs[i]->name)) ||
          (is_packing &&
           !glade_widget_adaptor_get_pack_property_class (parent_adaptor,
                                                          specs[i]->name)))
        {
          if ((property_class =
               glade_property_class_new_from_spec (adaptor, specs[i])) != NULL)
            list = g_list_prepend (list, property_class);
        }
    }

  if (is_packing)
    adaptor->priv->packing_props =
        g_list_concat (adaptor->priv->packing_props, g_list_reverse (list));
  else
    adaptor->priv->properties =
        g_list_concat (adaptor->priv->properties, g_list_reverse (list));
}

static void
gwa_setup_properties (GladeWidgetAdaptor *adaptor,
                      GObjectClass       *object_class,
                      gboolean            is_packing)
{
  GParamSpec **specs = NULL;
  guint n_specs = 0;
  GList *l;

  /* only GtkContainer child propeties can be introspected */
  if (is_packing && !g_type_is_a (adaptor->priv->type, GTK_TYPE_CONTAINER))
    return;

  /* First clone the parents properties */
  if (is_packing)
    adaptor->priv->packing_props = gwa_clone_parent_properties (adaptor, is_packing);
  else
    adaptor->priv->properties = gwa_clone_parent_properties (adaptor, is_packing);

  /* Now automaticly introspect new properties added in this class,
   * allow the class writer to decide what to override in the resulting
   * list of properties.
   */
  if (is_packing)
    specs = gtk_container_class_list_child_properties (object_class, &n_specs);
  else
    specs = g_object_class_list_properties (object_class, &n_specs);
  gwa_setup_introspected_props_from_pspecs (adaptor, specs, n_specs,
                                            is_packing);
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
      for (l = adaptor->priv->packing_props; l; l = l->next)
        {
          GladePropertyClass *property_class = l->data;

          glade_property_class_set_is_packing (property_class, TRUE);
        }
    }
}

static GList *
gwa_inherit_child_packing (GladeWidgetAdaptor *adaptor)
{
  GladeWidgetAdaptor *parent_adaptor;
  GList *child_packings = NULL, *packing_list, *default_list;

  if ((parent_adaptor = gwa_get_parent_adaptor (adaptor)) != NULL)
    {
      for (packing_list = parent_adaptor->priv->child_packings;
           packing_list; packing_list = packing_list->next)
        {
          GladeChildPacking *packing = packing_list->data;
          GladeChildPacking *packing_dup = g_new0 (GladeChildPacking, 1);

          packing_dup->parent_name = g_strdup (packing->parent_name);

          for (default_list = packing->packing_defaults;
               default_list; default_list = default_list->next)
            {
              GladePackingDefault *def = default_list->data;
              GladePackingDefault *def_dup = g_new0 (GladePackingDefault, 1);

              def_dup->id = g_strdup (def->id);
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
  GList *list, *node;
  GladeSignalDef *signal, *parent_signal;

  if ((parent_adaptor = gwa_get_parent_adaptor (adaptor)) != NULL)
    {
      for (list = adaptor->priv->signals; list; list = list->next)
        {
          signal = list->data;

          if ((node = g_list_find_custom (parent_adaptor->priv->signals, 
                                          glade_signal_def_get_name (signal),
                                          (GCompareFunc) gwa_signal_find_comp)) != NULL)
            {
              parent_signal = node->data;

              /* XXX FIXME: This is questionable, why should derived catalogs
               * reset the derived signal versions ???
               *
               * Reset versioning in derived catalogs just once
               */
              if (strcmp (adaptor->priv->catalog,
                          parent_adaptor->priv->catalog))
                glade_signal_def_set_since (signal, 0, 0);
              else
                glade_signal_def_set_since (signal, 
                                            glade_signal_def_since_major (parent_signal),
                                            glade_signal_def_since_minor (parent_signal));

              glade_signal_def_set_deprecated (signal, glade_signal_def_deprecated (parent_signal));
            }
        }
    }
}

static GladeInternalChild *
gwa_internal_children_new (gchar *name, gboolean anarchist)
{
  GladeInternalChild *data = g_slice_new0 (GladeInternalChild);

  data->name = g_strdup (name);
  data->anarchist = anarchist;

  return data;
}

static GList *
gwa_internal_children_clone (GList *children)
{
  GList *l, *retval = NULL;
  
  for (l = children; l; l = g_list_next (l))
    {
      GladeInternalChild *data, *child = l->data;

      data = gwa_internal_children_new (child->name, child->anarchist);
      retval = g_list_prepend (retval, data);

      if (child->children)
        data->children = gwa_internal_children_clone (child->children);
    }

  return g_list_reverse (retval);
}

static GObject *
glade_widget_adaptor_constructor (GType                  type,
                                  guint                  n_construct_properties,
                                  GObjectConstructParam *construct_properties)
{
  GladeWidgetAdaptor *adaptor, *parent_adaptor;
  GObject *ret_obj;
  GObjectClass *object_class;

  glade_abort_if_derived_adaptors_exist (type);

  ret_obj = G_OBJECT_CLASS (glade_widget_adaptor_parent_class)->constructor
      (type, n_construct_properties, construct_properties);

  adaptor = GLADE_WIDGET_ADAPTOR (ret_obj);
  parent_adaptor = gwa_get_parent_adaptor (adaptor);

  if (adaptor->priv->type == G_TYPE_NONE)
    g_warning ("Adaptor created without a type");
  if (adaptor->priv->name == NULL)
    g_warning ("Adaptor created without a name");

  /* Build decorations */
  if (!adaptor->priv->icon_name)
    adaptor->priv->icon_name = g_strdup ("image-missing");

  /* Let it leek */
  if ((object_class = g_type_class_ref (adaptor->priv->type)))
    {
      /* Build signals & properties */
      adaptor->priv->signals = gwa_list_signals (adaptor, adaptor->priv->type);

      gwa_inherit_signals (adaptor);
      gwa_setup_properties (adaptor, object_class, FALSE);
      gwa_setup_properties (adaptor, object_class, TRUE);
    }

  /* Inherit packing defaults here */
  adaptor->priv->child_packings = gwa_inherit_child_packing (adaptor);

  /* Inherit special-child-type */
  if (parent_adaptor)
    adaptor->priv->special_child_type =
        parent_adaptor->priv->special_child_type ?
        g_strdup (parent_adaptor->priv->special_child_type) : NULL;


  /* Reset version numbering if we're in a new catalog just once */
  if (parent_adaptor &&
      strcmp (adaptor->priv->catalog, parent_adaptor->priv->catalog))
    {
      GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->version_since_major =
          GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->version_since_minor = 0;
    }

  /* Copy parent actions */
  if (parent_adaptor)
    {
      GList *l;

      if (parent_adaptor->priv->actions)
        {
          for (l = parent_adaptor->priv->actions; l; l = g_list_next (l))
            {
              GWActionClass *child = glade_widget_action_class_clone (l->data);
              adaptor->priv->actions = g_list_prepend (adaptor->priv->actions, child);
            }
          adaptor->priv->actions = g_list_reverse (adaptor->priv->actions);
        }

      if (parent_adaptor->priv->packing_actions)
        {
          for (l = parent_adaptor->priv->packing_actions; l; l = g_list_next (l))
            {
              GWActionClass *child = glade_widget_action_class_clone (l->data);
              adaptor->priv->packing_actions =
                  g_list_prepend (adaptor->priv->packing_actions, child);
            }
          adaptor->priv->packing_actions = g_list_reverse (adaptor->priv->packing_actions);
        }
    }

  /* Copy parent internal children */
  if (parent_adaptor && parent_adaptor->priv->internal_children)
    adaptor->priv->internal_children = gwa_internal_children_clone (parent_adaptor->priv->internal_children);

  return ret_obj;
}

static void
gwa_packing_default_free (GladePackingDefault *def)
{
  g_clear_pointer (&def->id, g_free);
  g_clear_pointer (&def->value, g_free);
  g_free (def);
}

static void
gwa_child_packing_free (GladeChildPacking *packing)
{
  g_clear_pointer (&packing->parent_name, g_free);

  g_list_free_full (packing->packing_defaults,
                    (GDestroyNotify) gwa_packing_default_free);
  packing->packing_defaults = NULL;
  g_free (packing);
}

static void
gwa_glade_internal_child_free (GladeInternalChild *child)
{
  g_clear_pointer (&child->name, g_free);
  if (child->children)
    {
      g_list_free_full (child->children,
                        (GDestroyNotify) gwa_glade_internal_child_free);
      child->children = NULL;
    }

  g_slice_free (GladeInternalChild, child);
}

static void
glade_widget_adaptor_finalize (GObject *object)
{
  GladeWidgetAdaptor *adaptor = GLADE_WIDGET_ADAPTOR (object);

  /* Free properties and signals */
  g_list_free_full (adaptor->priv->properties,
                    (GDestroyNotify) glade_property_class_free);
  adaptor->priv->properties = NULL;

  g_list_free_full (adaptor->priv->packing_props,
                    (GDestroyNotify) glade_property_class_free);
  adaptor->priv->packing_props = NULL;

  g_list_free_full (adaptor->priv->signals,
                    (GDestroyNotify) glade_signal_def_free);
  adaptor->priv->signals = NULL;

  /* Free child packings */
  g_list_free_full (adaptor->priv->child_packings,
                    (GDestroyNotify) gwa_child_packing_free);
  adaptor->priv->child_packings = NULL;

  g_clear_pointer (&adaptor->priv->book, g_free);
  g_clear_pointer (&adaptor->priv->catalog, g_free);
  g_clear_pointer (&adaptor->priv->special_child_type, g_free);

  g_clear_object (&adaptor->priv->cursor);

  g_clear_pointer (&adaptor->priv->name, g_free);
  g_clear_pointer (&adaptor->priv->generic_name, g_free);
  g_clear_pointer (&adaptor->priv->title, g_free);
  g_clear_pointer (&adaptor->priv->icon_name, g_free);
  g_clear_pointer (&adaptor->priv->missing_icon, g_free);

  if (adaptor->priv->actions)
    {
      g_list_free_full (adaptor->priv->actions,
                        (GDestroyNotify) glade_widget_action_class_free);
      adaptor->priv->actions = NULL;
    }

  if (adaptor->priv->packing_actions)
    {
      g_list_free_full (adaptor->priv->packing_actions,
                        (GDestroyNotify) glade_widget_action_class_free);
      adaptor->priv->packing_actions = NULL;
    }

  if (adaptor->priv->internal_children)
    {
      g_list_free_full (adaptor->priv->internal_children,
                        (GDestroyNotify) gwa_glade_internal_child_free);
      adaptor->priv->internal_children = NULL;
    }

  G_OBJECT_CLASS (glade_widget_adaptor_parent_class)->finalize (object);
}

static void
glade_widget_adaptor_real_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GladeWidgetAdaptor *adaptor;

  adaptor = GLADE_WIDGET_ADAPTOR (object);

  switch (prop_id)
    {
      case PROP_NAME:
        /* assume once (construct-only) */
        adaptor->priv->name = g_value_dup_string (value);
        adaptor->priv->real_type = g_type_from_name (adaptor->priv->name);
        break;
      case PROP_ICON_NAME:
        /* assume once (construct-only) */
        adaptor->priv->icon_name = g_value_dup_string (value);
        break;
      case PROP_TYPE:
        adaptor->priv->type = g_value_get_gtype (value);
        break;
      case PROP_TITLE:
        g_clear_pointer (&adaptor->priv->title, g_free);
        adaptor->priv->title = g_value_dup_string (value);
        break;
      case PROP_GENERIC_NAME:
        g_clear_pointer (&adaptor->priv->generic_name, g_free);
        adaptor->priv->generic_name = g_value_dup_string (value);
        break;
      case PROP_CATALOG:
        /* assume once (construct-only) */
        g_clear_pointer (&adaptor->priv->catalog, g_free);
        adaptor->priv->catalog = g_value_dup_string (value);
        break;
      case PROP_BOOK:
        /* assume once (construct-only) */
        g_clear_pointer (&adaptor->priv->book, g_free);
        adaptor->priv->book = g_value_dup_string (value);
        break;
      case PROP_SPECIAL_TYPE:
        /* assume once (construct-only) */ 
        g_clear_pointer (&adaptor->priv->special_child_type, g_free);
        adaptor->priv->special_child_type = g_value_dup_string (value);
        break;
      case PROP_QUERY:
        adaptor->priv->query = g_value_get_boolean (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_widget_adaptor_real_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{

  GladeWidgetAdaptor *adaptor;

  adaptor = GLADE_WIDGET_ADAPTOR (object);

  switch (prop_id)
    {
      case PROP_NAME:
        g_value_set_string (value, adaptor->priv->name);
        break;
      case PROP_TYPE:
        g_value_set_gtype (value, adaptor->priv->type);
        break;
      case PROP_TITLE:
        g_value_set_string (value, adaptor->priv->title);
        break;
      case PROP_GENERIC_NAME:
        g_value_set_string (value, adaptor->priv->generic_name);
        break;
      case PROP_ICON_NAME:
        g_value_set_string (value, adaptor->priv->icon_name);
        break;
      case PROP_CATALOG:
        g_value_set_string (value, adaptor->priv->catalog);
        break;
      case PROP_BOOK:
        g_value_set_string (value, adaptor->priv->book);
        break;
      case PROP_SPECIAL_TYPE:
        g_value_set_string (value, adaptor->priv->special_child_type);
        break;
      case PROP_CURSOR:
        g_value_set_pointer (value, adaptor->priv->cursor);
        break;
      case PROP_QUERY:
        g_value_set_boolean (value, adaptor->priv->query);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

/*******************************************************************************
                  GladeWidgetAdaptor base class implementations
 *******************************************************************************/
static GladeWidget *
glade_widget_adaptor_object_create_widget (GladeWidgetAdaptor *adaptor,
                                           const gchar        *first_property_name,
                                           va_list             var_args)
{
  return GLADE_WIDGET (g_object_new_valist (GLADE_TYPE_WIDGET,
                                            first_property_name, var_args));
}

static GObject *
glade_widget_adaptor_object_construct_object (GladeWidgetAdaptor *adaptor,
                                              guint               n_parameters,
                                              GParameter         *parameters)
{
  return g_object_newv (adaptor->priv->type, n_parameters, parameters);
}

static void
glade_widget_adaptor_object_destroy_object (GladeWidgetAdaptor *adaptor,
                                            GObject            *object)
{
  /* Do nothing, just have a method here incase classes chain up */
}


static GObject *
glade_widget_adaptor_object_get_internal_child (GladeWidgetAdaptor *adaptor,
                                                GObject            *object,
                                                const gchar        *name)
{
  static GtkBuilder *builder = NULL;
  
  g_return_val_if_fail (GTK_IS_BUILDABLE (object), NULL);

  /* Dummy object just in case the interface use it for something */
  if (builder == NULL) builder = gtk_builder_new ();
  
  return gtk_buildable_get_internal_child (GTK_BUILDABLE (object), builder, name);
}

static gboolean
glade_widget_adaptor_object_add_verify (GladeWidgetAdaptor *adaptor,
                                        GObject            *parent,
                                        GObject            *child,
                                        gboolean            user_feedback)
{
  if (user_feedback)
    glade_util_ui_message (glade_app_get_window (),
                           GLADE_UI_INFO, NULL,
                           _("%s does not support adding any children."), 
                           adaptor->priv->title);

  return FALSE;
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
                                             GObject            *object,
                                             const gchar        *action_id)
{
  g_message ("No action_activate() support in adaptor %s for action '%s'",
             adaptor->priv->name, action_id);
}

static void
glade_widget_adaptor_object_child_action_activate (GladeWidgetAdaptor *adaptor,
                                                   GObject            *container,
                                                   GObject            *object,
                                                   const gchar        *action_id)
{
  g_message ("No child_action_activate() support in adaptor %s for action '%s'",
             adaptor->priv->name, action_id);
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

      /* Get prop name from node and lookup property ... */
      if (!(name = glade_xml_get_property_string_required
            (iter_node, GLADE_XML_TAG_NAME, NULL)))
        continue;

      prop_name = glade_util_read_prop_name (name);

      /* Some properties may be special child type of custom, just leave them for the adaptor */
      if ((property = glade_widget_get_property (widget, prop_name)) != NULL)
        {
          glade_property_read (property, glade_widget_get_project (widget), iter_node);
          read_properties = g_list_prepend (read_properties, property);
        }

      g_free (prop_name);
      g_free (name);
    }

  /* Sync the remaining values not read in from the Glade file.. */
  for (l = glade_widget_get_properties (widget); l; l = l->next)
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

      if (!(signal = glade_signal_read (iter_node, adaptor)))
        continue;

      /* The widget doesnt use the signal handler directly but rather
       * creates it's own copy */
      glade_widget_add_signal_handler (widget, signal);
      g_object_unref (signal);
    }

  /* Read in children */
  for (iter_node = glade_xml_node_get_children (node);
       iter_node; iter_node = glade_xml_node_next (iter_node))
    {
      if (glade_xml_node_verify_silent (iter_node, GLADE_XML_TAG_CHILD))
        glade_widget_read_child (widget, iter_node);

      if (glade_project_load_cancelled (glade_widget_get_project (widget)))
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
  for (props = glade_widget_get_properties (widget); props; props = props->next)
    {
      GladeProperty      *property = props->data;
      GladePropertyClass *klass = glade_property_get_class (property);

      if (glade_property_class_save (klass) && 
          glade_property_get_enabled (property))
        glade_property_write (GLADE_PROPERTY (props->data), context, node);
    }
}

static void
glade_widget_adaptor_object_write_widget_after (GladeWidgetAdaptor *adaptor,
                                                GladeWidget        *widget,
                                                GladeXmlContext    *context,
                                                GladeXmlNode       *node)
{

}

static void
glade_widget_adaptor_object_read_child (GladeWidgetAdaptor *adaptor,
                                        GladeWidget        *widget,
                                        GladeXmlNode       *node)
{
  GladeXmlNode *widget_node, *packing_node, *iter_node;
  GladeWidget *child_widget;
  gchar *internal_name;
  gchar *name, *prop_name;
  GladeProperty *property;

  if (!glade_xml_node_verify (node, GLADE_XML_TAG_CHILD))
    return;

  internal_name =
      glade_xml_get_property_string (node, GLADE_XML_TAG_INTERNAL_CHILD);

  if ((widget_node =
       glade_xml_search_child (node, GLADE_XML_TAG_WIDGET)) != NULL)
    {
      child_widget =
        glade_widget_read (glade_widget_get_project (widget),
                           widget, widget_node, internal_name);

      if (child_widget)
        {
          if (!internal_name)
            {
              glade_widget_set_child_type_from_node
                (widget, glade_widget_get_object (child_widget), node);
              glade_widget_add_child (widget, child_widget, FALSE);
            }

          if ((packing_node =
               glade_xml_search_child (node, GLADE_XML_TAG_PACKING)) != NULL)
            {

              /* Read in the properties */
              for (iter_node = glade_xml_node_get_children (packing_node);
                   iter_node; iter_node = glade_xml_node_next (iter_node))
                {
                  if (!glade_xml_node_verify_silent
                      (iter_node, GLADE_XML_TAG_PROPERTY))
                    continue;

                  /* Get prop name from node and lookup property ... */
                  if (!(name = glade_xml_get_property_string_required
                        (iter_node, GLADE_XML_TAG_NAME, NULL)))
                    continue;

                  prop_name = glade_util_read_prop_name (name);

                  /* Some properties may be special child type of custom, 
                   * just leave them for the adaptor */
                  if ((property =
                       glade_widget_get_pack_property (child_widget,
                                                       prop_name)) != NULL)
                    glade_property_read (property, 
                                         glade_widget_get_project (child_widget),
                                         iter_node);

                  g_free (prop_name);
                  g_free (name);
                }
            }
        }

    }
  else
    {
      GObject *palaceholder = G_OBJECT (glade_placeholder_new ());
      glade_widget_set_child_type_from_node (widget, palaceholder, node);
      glade_widget_adaptor_add (adaptor, glade_widget_get_object (widget), palaceholder);
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
  GList *props;

  child_node = glade_xml_node_new (context, GLADE_XML_TAG_CHILD);
  glade_xml_node_append_child (node, child_node);

  /* Set internal child */
  if (glade_widget_get_internal (widget))
    glade_xml_node_set_property_string (child_node,
                                        GLADE_XML_TAG_INTERNAL_CHILD,
                                        glade_widget_get_internal (widget));

  /* Write out the widget */
  glade_widget_write (widget, context, child_node);

  /* Write out packing properties and special-child-type */
  packing_node = glade_xml_node_new (context, GLADE_XML_TAG_PACKING);
  glade_xml_node_append_child (child_node, packing_node);

  for (props = glade_widget_get_packing_properties (widget); props; props = props->next)
    {
      GladeProperty      *property = props->data;
      GladePropertyClass *klass = glade_property_get_class (property);

      if (glade_property_class_save (klass) && 
          glade_property_get_enabled (property))
        glade_property_write (GLADE_PROPERTY (props->data),
                              context, packing_node);
    }

  glade_widget_write_special_child_prop (glade_widget_get_parent (widget),
                                         glade_widget_get_object (widget), 
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

  if (G_IS_PARAM_SPEC_ENUM (pspec))
    type = GLADE_TYPE_EPROP_ENUM;
  else if (G_IS_PARAM_SPEC_FLAGS (pspec))
    type = GLADE_TYPE_EPROP_FLAGS;
  else if (G_IS_PARAM_SPEC_VALUE_ARRAY (pspec))
    {
      /* Require deprecated code */
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      if (pspec->value_type == G_TYPE_VALUE_ARRAY)
        type = GLADE_TYPE_EPROP_TEXT;
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
    }
  else if (G_IS_PARAM_SPEC_BOXED (pspec))
    {
      if (pspec->value_type == GDK_TYPE_COLOR ||
          pspec->value_type == GDK_TYPE_RGBA)
        type = GLADE_TYPE_EPROP_COLOR;
      else if (pspec->value_type == G_TYPE_STRV)
        type = GLADE_TYPE_EPROP_TEXT;
    }
  else if (G_IS_PARAM_SPEC_STRING (pspec) ||
           G_IS_PARAM_SPEC_VARIANT (pspec))
    type = GLADE_TYPE_EPROP_TEXT;
  else if (G_IS_PARAM_SPEC_BOOLEAN (pspec))
    type = GLADE_TYPE_EPROP_BOOL;
  else if (G_IS_PARAM_SPEC_FLOAT (pspec) ||
           G_IS_PARAM_SPEC_DOUBLE (pspec) ||
           G_IS_PARAM_SPEC_INT (pspec) ||
           G_IS_PARAM_SPEC_UINT (pspec) ||
           G_IS_PARAM_SPEC_LONG (pspec) ||
           G_IS_PARAM_SPEC_ULONG (pspec) ||
           G_IS_PARAM_SPEC_INT64 (pspec) || G_IS_PARAM_SPEC_UINT64 (pspec))
    type = GLADE_TYPE_EPROP_NUMERIC;
  else if (G_IS_PARAM_SPEC_UNICHAR (pspec))
    type = GLADE_TYPE_EPROP_UNICHAR;
  else if (G_IS_PARAM_SPEC_OBJECT (pspec))
    {
      if (pspec->value_type == GDK_TYPE_PIXBUF ||
          pspec->value_type == G_TYPE_FILE)
        type = GLADE_TYPE_EPROP_TEXT;
      else
        type = GLADE_TYPE_EPROP_OBJECT;
    }
  else if (GLADE_IS_PARAM_SPEC_OBJECTS (pspec))
    type = GLADE_TYPE_EPROP_OBJECTS;

  return type;
}

static GladeEditorProperty *
glade_widget_adaptor_object_create_eprop (GladeWidgetAdaptor *adaptor,
                                          GladePropertyClass *klass,
                                          gboolean            use_command)
{
  GladeEditorProperty *eprop;
  GParamSpec          *pspec;
  GType                type = 0;

  pspec = glade_property_class_get_pspec (klass);

  /* Find the right type of GladeEditorProperty for this
   * GladePropertyClass.
   */
  if ((type = glade_widget_adaptor_get_eprop_type (pspec)) == 0)
    return NULL;

  /* special case for string specs that denote themed application icons. */
  if (glade_property_class_themed_icon (klass))
    type = GLADE_TYPE_EPROP_NAMED_ICON;

  /* Create and return the correct type of GladeEditorProperty */
  eprop = g_object_new (type,
                        "property-class", klass,
                        "use-command", use_command, NULL);

  return eprop;
}

static gchar *
glade_widget_adaptor_object_string_from_value (GladeWidgetAdaptor *adaptor,
                                               GladePropertyClass *klass,
                                               const GValue       *value)
{
  return glade_property_class_make_string_from_gvalue (klass, value);
}

static GladeEditable *
glade_widget_adaptor_object_create_editable (GladeWidgetAdaptor *adaptor,
                                             GladeEditorPageType type)
{
  return (GladeEditable *) glade_editor_table_new (adaptor, type);
}

static void
glade_internal_child_append (GladeWidgetAdaptor *adaptor,
                             GObject            *object,
                             GList              *list,
                             GList             **children)
{
  GList *l;

  for (l = list; l; l = g_list_next (l))
    {
      GladeInternalChild *internal = l->data;
      GObject *child;
        
      child = glade_widget_adaptor_get_internal_child (adaptor,
                                                       object,
                                                       internal->name);
      if (child)
        *children = g_list_prepend (*children, child);
    }
}

static GList *
glade_widget_adaptor_object_get_children (GladeWidgetAdaptor *adaptor,
                                          GObject *object)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);
  GList *children = NULL;
  const gchar *name;

  if (gwidget && (name = glade_widget_get_internal (gwidget)))
    {
      GladeWidget *parent = gwidget;

      /* Get non internal parent */
      while ((parent = glade_widget_get_parent (parent)) &&
             glade_widget_get_internal (parent));

      if (parent)
        {
          GladeWidgetAdaptor *padaptor = glade_widget_get_adaptor (parent);
          GladeInternalChild *internal;

          internal = gwa_internal_child_find (padaptor->priv->internal_children,
                                              name);

          if (internal && internal->children)
            glade_internal_child_append (padaptor, glade_widget_get_object (parent),
                                         internal->children, &children);
        }

      return children;
    }

  glade_internal_child_append (adaptor, object,
                               adaptor->priv->internal_children,
                               &children);

  return children;
}


/*******************************************************************************
            GladeWidgetAdaptor type registration and class initializer
 *******************************************************************************/
static void
glade_widget_adaptor_init (GladeWidgetAdaptor *adaptor)
{
  adaptor->priv = glade_widget_adaptor_get_instance_private (adaptor);

}

static void
glade_widget_adaptor_class_init (GladeWidgetAdaptorClass *adaptor_class)
{
  GObjectClass *object_class;
  g_return_if_fail (adaptor_class != NULL);

  glade_widget_adaptor_parent_class = g_type_class_peek_parent (adaptor_class);
  object_class = G_OBJECT_CLASS (adaptor_class);

  /* GObjectClass */
  object_class->constructor = glade_widget_adaptor_constructor;
  object_class->finalize = glade_widget_adaptor_finalize;
  object_class->set_property = glade_widget_adaptor_real_set_property;
  object_class->get_property = glade_widget_adaptor_real_get_property;

  /* Class methods */
  adaptor_class->create_widget = glade_widget_adaptor_object_create_widget;
  adaptor_class->construct_object = glade_widget_adaptor_object_construct_object;
  adaptor_class->destroy_object = glade_widget_adaptor_object_destroy_object;
  adaptor_class->deep_post_create = NULL;
  adaptor_class->post_create = NULL;
  adaptor_class->get_internal_child = glade_widget_adaptor_object_get_internal_child;
  adaptor_class->verify_property = NULL;
  adaptor_class->set_property = glade_widget_adaptor_object_set_property;
  adaptor_class->get_property = glade_widget_adaptor_object_get_property;
  adaptor_class->add_verify = glade_widget_adaptor_object_add_verify;
  adaptor_class->add = NULL;
  adaptor_class->remove = NULL;
  adaptor_class->replace_child = NULL;
  adaptor_class->get_children = glade_widget_adaptor_object_get_children;
  adaptor_class->child_set_property = NULL;
  adaptor_class->child_get_property = NULL;
  adaptor_class->action_activate = glade_widget_adaptor_object_action_activate;
  adaptor_class->child_action_activate = glade_widget_adaptor_object_child_action_activate;
  adaptor_class->action_submenu = NULL;
  adaptor_class->depends = NULL;
  adaptor_class->read_widget = glade_widget_adaptor_object_read_widget;
  adaptor_class->write_widget = glade_widget_adaptor_object_write_widget;
  adaptor_class->write_widget_after = glade_widget_adaptor_object_write_widget_after;
  adaptor_class->read_child = glade_widget_adaptor_object_read_child;
  adaptor_class->write_child = glade_widget_adaptor_object_write_child;
  adaptor_class->create_eprop = glade_widget_adaptor_object_create_eprop;
  adaptor_class->string_from_value = glade_widget_adaptor_object_string_from_value;
  adaptor_class->create_editable = glade_widget_adaptor_object_create_editable;

  /* Base defaults here */
  adaptor_class->toplevel = FALSE;
  adaptor_class->use_placeholders = FALSE;
  adaptor_class->default_width = -1;
  adaptor_class->default_height = -1;

  /* Properties */
  g_object_class_install_property
      (object_class, PROP_NAME,
       g_param_spec_string
       ("name", _("Name"),
        _("Name of the class"),
        NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property
      (object_class, PROP_TYPE,
       g_param_spec_gtype
       ("type", _("Type"),
        _("GType of the class"),
        G_TYPE_NONE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property
      (object_class, PROP_TITLE,
       g_param_spec_string
       ("title", _("Title"),
        _("Translated title for the class used in the glade UI"),
        NULL, G_PARAM_READWRITE));

  g_object_class_install_property
      (object_class, PROP_GENERIC_NAME,
       g_param_spec_string
       ("generic-name", _("Generic Name"),
        _("Used to generate names of new widgets"),
        NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property
      (object_class, PROP_ICON_NAME,
       g_param_spec_string
       ("icon-name", _("Icon Name"),
        _("The icon name"),
        DEFAULT_ICON_NAME, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property
      (object_class, PROP_CATALOG,
       g_param_spec_string
       ("catalog", _("Catalog"),
        _("The name of the widget catalog this class was declared by"),
        NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property
      (object_class, PROP_BOOK,
       g_param_spec_string
       ("book", _("Book"),
        _("DevHelp search namespace for this widget class"),
        NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property
      (object_class, PROP_SPECIAL_TYPE,
       g_param_spec_string
       ("special-child-type", _("Special Child Type"),
        _("Holds the name of the packing property to depict "
          "special children for this container class"),
        NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property
      (object_class, PROP_CURSOR,
       g_param_spec_pointer
       ("cursor", _("Cursor"),
        _("A cursor for inserting widgets in the UI"), G_PARAM_READABLE));
  g_object_class_install_property
      (object_class, PROP_QUERY,
       g_param_spec_boolean
       ("query", _("Query"),
        _("Whether the adaptor should query the use or not"), FALSE, G_PARAM_READWRITE));
}

/*******************************************************************************
                        Synthetic Object Derivation
 *******************************************************************************/
typedef struct
{
  GladeXmlNode *node;
  GModule *module;
} GWADerivedClassData;

static void
gwa_derived_init (GladeWidgetAdaptor *adaptor, gpointer g_class)
{

}

static void
gwa_warn_deprecated_if_symbol_found (GladeXmlNode *node, gchar *tagname)
{
  gchar *symbol;

  if ((symbol = glade_xml_get_value_string (node, tagname)))
    {
      g_warning ("GladeWidgetAdaptor %s method is deprecated. %s() will not be used",
                 tagname, symbol);
      g_free (symbol);
    }
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
                                    GLADE_TAG_CONSTRUCTOR_FUNCTION, &symbol))
    object_class->constructor = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_CREATE_WIDGET_FUNCTION, &symbol))
    klass->create_widget = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_CONSTRUCT_OBJECT_FUNCTION,
                                    &symbol))
    klass->construct_object = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_DESTROY_OBJECT_FUNCTION,
                                    &symbol))
    klass->destroy_object = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_DEEP_POST_CREATE_FUNCTION,
                                    &symbol))
    klass->deep_post_create = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_POST_CREATE_FUNCTION, &symbol))
    klass->post_create = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_SET_FUNCTION, &symbol))
    klass->set_property = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_GET_FUNCTION, &symbol))
    klass->get_property = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_VERIFY_FUNCTION, &symbol))
    klass->verify_property = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_ADD_CHILD_VERIFY_FUNCTION, &symbol))
    klass->add_verify = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_ADD_CHILD_FUNCTION, &symbol))
    klass->add = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_REMOVE_CHILD_FUNCTION, &symbol))
    klass->remove = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_REPLACE_CHILD_FUNCTION, &symbol))
    klass->replace_child = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_GET_CHILDREN_FUNCTION, &symbol))
    klass->get_children = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_CHILD_SET_PROP_FUNCTION, &symbol))
    klass->child_set_property = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_CHILD_GET_PROP_FUNCTION, &symbol))
    klass->child_get_property = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_CHILD_VERIFY_FUNCTION, &symbol))
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
                                    GLADE_TAG_ACTION_SUBMENU_FUNCTION, &symbol))
    klass->action_submenu = symbol;

  /* depends method is deprecated, warn the user */
  gwa_warn_deprecated_if_symbol_found (node, GLADE_TAG_DEPENDS_FUNCTION);

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_READ_WIDGET_FUNCTION, &symbol))
    klass->read_widget = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_WRITE_WIDGET_FUNCTION, &symbol))
    klass->write_widget = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_WRITE_WIDGET_AFTER_FUNCTION, &symbol))
    klass->write_widget_after = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_READ_CHILD_FUNCTION, &symbol))
    klass->read_child = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_WRITE_CHILD_FUNCTION, &symbol))
    klass->write_child = symbol;

  if (glade_xml_load_sym_from_node (node, module,
                                    GLADE_TAG_CREATE_EPROP_FUNCTION, &symbol))
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
                        GWADerivedClassData     *data)
{
  GladeXmlNode *node = data->node;
  GModule *module = data->module;
  guint16 deprecated_since_major = 0;
  guint16 deprecated_since_minor = 0;

  /* Load catalog symbols from module */
  if (module)
    gwa_extend_with_node_load_sym (adaptor_class, node, module);

  glade_xml_get_property_version
      (node, GLADE_TAG_VERSION_SINCE,
       &adaptor_class->version_since_major,
       &adaptor_class->version_since_minor);

  adaptor_class->deprecated =
      glade_xml_get_property_boolean
      (node, GLADE_TAG_DEPRECATED, adaptor_class->deprecated);

  glade_xml_get_property_version
      (node, GLADE_TAG_DEPRECATED_SINCE,
       &deprecated_since_major, &deprecated_since_minor);
  adaptor_class->deprecated_since_major = deprecated_since_major;
  adaptor_class->deprecated_since_minor = deprecated_since_minor;

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
  GType iter_type, derived_type;
  GType parent_type = GLADE_TYPE_WIDGET_ADAPTOR;
  gchar *type_name;
  GTypeInfo adaptor_info = {
    sizeof (GladeWidgetAdaptorClass),
    (GBaseInitFunc) NULL,
    (GBaseFinalizeFunc) NULL,
    (GClassInitFunc) gwa_derived_class_init,
    (GClassFinalizeFunc) NULL,
    data,                       /* class_data */
    sizeof (GladeWidgetAdaptor),
    0,                          /* n_preallocs */
    (GInstanceInitFunc) gwa_derived_init,
  };

  for (iter_type = g_type_parent (object_type);
       iter_type > 0; iter_type = g_type_parent (iter_type))
    {
      if ((adaptor = glade_widget_adaptor_get_by_type (iter_type)) != NULL)
        {
          parent_type = G_TYPE_FROM_INSTANCE (adaptor);
          break;
        }
    }

  /* Note that we must pass ownership of the type_name string
   * to the type system 
   */
  type_name = g_strdup_printf ("Glade%sAdaptor", g_type_name (object_type));
  derived_type = g_type_register_static (parent_type, type_name,
                                         &adaptor_info, 0);
  g_free (type_name);

  return derived_type;
}


/*******************************************************************************
                                     API
 *******************************************************************************/
GType
glade_widget_adaptor_get_object_type (GladeWidgetAdaptor *adaptor)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), G_TYPE_INVALID);

  return adaptor->priv->type;
}

G_CONST_RETURN gchar *
glade_widget_adaptor_get_name (GladeWidgetAdaptor *adaptor)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  return adaptor->priv->name;
}

/**
 * glade_widget_adaptor_get_display_name
 * @adaptor: a #GladeWidgetAdaptor
 *
 * Returns: the name of the adaptor without %GWA_INSTANTIABLE_PREFIX
 */
G_CONST_RETURN gchar *
glade_widget_adaptor_get_display_name (GladeWidgetAdaptor *adaptor)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  if (g_str_has_prefix (adaptor->priv->name, GWA_INSTANTIABLE_PREFIX))
    return &adaptor->priv->name[GWA_INSTANTIABLE_PREFIX_LEN];

  return adaptor->priv->name;
}

G_CONST_RETURN gchar *
glade_widget_adaptor_get_generic_name (GladeWidgetAdaptor *adaptor)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  return adaptor->priv->generic_name;
}

G_CONST_RETURN gchar *
glade_widget_adaptor_get_title (GladeWidgetAdaptor *adaptor)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  return adaptor->priv->title;
}

G_CONST_RETURN gchar *
glade_widget_adaptor_get_icon_name (GladeWidgetAdaptor *adaptor)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  return adaptor->priv->icon_name;
}

G_CONST_RETURN gchar *
glade_widget_adaptor_get_missing_icon (GladeWidgetAdaptor *adaptor)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  return adaptor->priv->missing_icon;
}

G_CONST_RETURN gchar *
glade_widget_adaptor_get_catalog (GladeWidgetAdaptor *adaptor)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  return adaptor->priv->catalog;
}

G_CONST_RETURN gchar *
glade_widget_adaptor_get_book (GladeWidgetAdaptor *adaptor)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  return adaptor->priv->book;
}

/**
 * glade_widget_adaptor_get_properties:
 * @adaptor: A #GladeWidgetAdaptor
 *
 * Returns: (transfer none) (element-type GladePropertyClass): a list of #GladePropertyClass
 */
G_CONST_RETURN GList *
glade_widget_adaptor_get_properties (GladeWidgetAdaptor *adaptor)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  return adaptor->priv->properties;
}

/**
 * glade_widget_adaptor_get_packing_props:
 * @adaptor: A #GladeWidgetAdaptor
 *
 * Returns: (transfer none) (element-type GladePropertyClass): a list of #GladePropertyClass
 */
G_CONST_RETURN GList *
glade_widget_adaptor_get_packing_props (GladeWidgetAdaptor *adaptor)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  return adaptor->priv->packing_props;
}

/**
 * glade_widget_adaptor_get_signals:
 * @adaptor: A #GladeWidgetAdaptor
 *
 * Returns: (transfer none) (element-type GladeSignalDef): a list of #GladeSignalDef
 */
G_CONST_RETURN GList *
glade_widget_adaptor_get_signals (GladeWidgetAdaptor *adaptor)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  return adaptor->priv->signals;
}

static void
accum_adaptor (gpointer key, GladeWidgetAdaptor *adaptor, GList **list)
{
  *list = g_list_prepend (*list, adaptor);
}

/**
 * glade_widget_adaptor_list_adaptors:
 *
 * Compiles a list of all registered adaptors.
 *
 * Returns: (transfer container) (element-type GladeWidgetAdaptor): A newly allocated #GList which
 * must be freed with g_list_free()
 */
GList *
glade_widget_adaptor_list_adaptors (void)
{
  GList *adaptors = NULL;

  g_hash_table_foreach (adaptor_hash, (GHFunc) accum_adaptor, &adaptors);

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

  if (glade_widget_adaptor_get_by_name (adaptor->priv->name))
    {
      g_warning ("Adaptor class for '%s' already registered", adaptor->priv->name);
      return;
    }

  if (!adaptor_hash)
    adaptor_hash = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                          NULL, g_object_unref);

  g_hash_table_insert (adaptor_hash, GSIZE_TO_POINTER (adaptor->priv->real_type), adaptor);

  g_signal_emit_by_name (glade_app_get (), "widget-adaptor-registered", adaptor, NULL);
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

  for (l = child_adaptor->priv->child_packings; l; l = l->next)
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
      gchar *name;
      GladeXmlNode *prop_node;
      GladeChildPacking *packing;

      if (!glade_xml_node_verify (child, GLADE_TAG_PARENT_CLASS))
        continue;

      if ((name = glade_xml_get_property_string_required
           (child, GLADE_TAG_NAME, adaptor->priv->name)) == NULL)
        continue;

      /* If a GladeChildPacking exists for this parent, use it -
       * otherwise prepend a new one
       */
      if ((packing =
           glade_widget_adaptor_get_child_packing (adaptor, name)) == NULL)
        {

          packing = g_new0 (GladeChildPacking, 1);
          packing->parent_name = name;

          adaptor->priv->child_packings =
              g_list_prepend (adaptor->priv->child_packings, packing);

        }

      for (prop_node = glade_xml_node_get_children (child);
           prop_node; prop_node = glade_xml_node_next (prop_node))
        {
          GladePackingDefault *def;
          gchar *id;
          gchar *value;

          if ((id =
               glade_xml_get_property_string_required
               (prop_node, GLADE_TAG_ID, adaptor->priv->name)) == NULL)
            continue;

          if ((value =
               glade_xml_get_property_string_required
               (prop_node, GLADE_TAG_DEFAULT, adaptor->priv->name)) == NULL)
            {
              g_free (id);
              continue;
            }

          if ((def = gwa_default_from_child_packing (packing, id)) == NULL)
            {
              def = g_new0 (GladePackingDefault, 1);
              def->id = id;
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

          adaptor->priv->child_packings =
              g_list_prepend (adaptor->priv->child_packings, packing);

        }
    }
}

static void
gwa_update_properties_from_node (GladeWidgetAdaptor *adaptor,
                                 GladeXmlNode       *node,
                                 GModule            *module,
                                 GList             **properties,
                                 const gchar        *domain,
                                 gboolean            is_packing)
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
          (child, GLADE_TAG_ID, adaptor->priv->name);
      if (!id)
        continue;

      /* property names from catalogs also need to have the '-' form */
      glade_util_replace (id, '_', '-');

      /* find the property in our list, if not found append a new property */
      for (list = *properties; list && list->data; list = list->next)
        {
          property_class = GLADE_PROPERTY_CLASS (list->data);
          if (glade_property_class_id (property_class) != NULL &&
              g_ascii_strcasecmp (id, glade_property_class_id (property_class)) == 0)
            break;
        }

      if (list)
        {
          property_class = GLADE_PROPERTY_CLASS (list->data);
        }
      else
        {
          property_class = glade_property_class_new (adaptor, id);

          /* When creating new virtual packing properties,
           * make sure we mark them as such here. 
           */
          if (is_packing)
            glade_property_class_set_is_packing (property_class, TRUE);

          *properties = g_list_append (*properties, property_class);
          list = g_list_last (*properties);
        }

      if ((updated = glade_property_class_update_from_node (child, 
                                                            adaptor->priv->type, 
                                                            &property_class, 
                                                            domain)) == FALSE)
        {
          g_warning ("failed to update %s property of %s from xml",
                     id, adaptor->priv->name);
          g_free (id);
          continue;
        }

      /* if this pointer was set to null, its a property we dont handle. */
      if (!property_class)
        *properties = g_list_delete_link (*properties, list);

      g_free (id);
    }
}

static GParamSpec *
pspec_dup (GParamSpec *spec)
{
  const gchar *name, *nick, *blurb;
  GType spec_type, value_type;
  GParamSpec *pspec = NULL;

  spec_type = G_PARAM_SPEC_TYPE (spec);
  value_type = spec->value_type;

  name = g_param_spec_get_name (spec);
  nick = g_param_spec_get_nick (spec);
  blurb = g_param_spec_get_blurb (spec);

  if (spec_type == G_TYPE_PARAM_ENUM ||
      spec_type == G_TYPE_PARAM_FLAGS ||
      spec_type == G_TYPE_PARAM_BOXED ||
      spec_type == G_TYPE_PARAM_OBJECT || spec_type == GLADE_TYPE_PARAM_OBJECTS)
    {

      if (spec_type == G_TYPE_PARAM_ENUM)
        {
          GParamSpecEnum *p = (GParamSpecEnum *) spec;
          pspec =
              g_param_spec_enum (name, nick, blurb, value_type,
                                 p->default_value, 0);
        }
      else if (spec_type == G_TYPE_PARAM_FLAGS)
        {
          GParamSpecFlags *p = (GParamSpecFlags *) spec;
          pspec =
              g_param_spec_flags (name, nick, blurb, value_type,
                                  p->default_value, 0);
        }
      else if (spec_type == G_TYPE_PARAM_OBJECT)
        pspec = g_param_spec_object (name, nick, blurb, value_type, 0);
      else if (spec_type == GLADE_TYPE_PARAM_OBJECTS)
        pspec = glade_param_spec_objects (name, nick, blurb, value_type, 0);
      else
        pspec = g_param_spec_boxed (name, nick, blurb, value_type, 0);
    }
  else if (spec_type == G_TYPE_PARAM_STRING)
    {
      GParamSpecString *p = (GParamSpecString *) spec;
      pspec = g_param_spec_string (name, nick, blurb, p->default_value, 0);
    }
  else if (spec_type == G_TYPE_PARAM_BOOLEAN)
    {
      GParamSpecBoolean *p = (GParamSpecBoolean *) spec;
      pspec = g_param_spec_boolean (name, nick, blurb, p->default_value, 0);
    }
  else
    {
      if (spec_type == G_TYPE_PARAM_CHAR)
        {
          GParamSpecChar *p = (GParamSpecChar *) spec;
          pspec = g_param_spec_char (name, nick, blurb,
                                     p->minimum, p->maximum, p->default_value,
                                     0);
        }
      else if (spec_type == G_TYPE_PARAM_UCHAR)
        {
          GParamSpecUChar *p = (GParamSpecUChar *) spec;
          pspec = g_param_spec_uchar (name, nick, blurb,
                                      p->minimum, p->maximum, p->default_value,
                                      0);
        }
      else if (spec_type == G_TYPE_PARAM_INT)
        {
          GParamSpecInt *p = (GParamSpecInt *) spec;
          pspec = g_param_spec_int (name, nick, blurb,
                                    p->minimum, p->maximum, p->default_value,
                                    0);
        }
      else if (spec_type == G_TYPE_PARAM_UINT)
        {
          GParamSpecUInt *p = (GParamSpecUInt *) spec;
          pspec = g_param_spec_uint (name, nick, blurb,
                                     p->minimum, p->maximum, p->default_value,
                                     0);
        }
      else if (spec_type == G_TYPE_PARAM_LONG)
        {
          GParamSpecLong *p = (GParamSpecLong *) spec;
          pspec = g_param_spec_long (name, nick, blurb,
                                     p->minimum, p->maximum, p->default_value,
                                     0);
        }
      else if (spec_type == G_TYPE_PARAM_ULONG)
        {
          GParamSpecULong *p = (GParamSpecULong *) spec;
          pspec = g_param_spec_ulong (name, nick, blurb,
                                      p->minimum, p->maximum, p->default_value,
                                      0);
        }
      else if (spec_type == G_TYPE_PARAM_INT64)
        {
          GParamSpecInt64 *p = (GParamSpecInt64 *) spec;
          pspec = g_param_spec_int64 (name, nick, blurb,
                                      p->minimum, p->maximum, p->default_value,
                                      0);
        }
      else if (spec_type == G_TYPE_PARAM_UINT64)
        {
          GParamSpecUInt64 *p = (GParamSpecUInt64 *) spec;
          pspec = g_param_spec_uint64 (name, nick, blurb,
                                       p->minimum, p->maximum, p->default_value,
                                       0);
        }
      else if (spec_type == G_TYPE_PARAM_FLOAT)
        {
          GParamSpecFloat *p = (GParamSpecFloat *) spec;
          pspec = g_param_spec_float (name, nick, blurb,
                                      p->minimum, p->maximum, p->default_value,
                                      0);
        }
      else if (spec_type == G_TYPE_PARAM_DOUBLE)
        {
          GParamSpecDouble *p = (GParamSpecDouble *) spec;
          pspec = g_param_spec_float (name, nick, blurb,
                                      p->minimum, p->maximum, p->default_value,
                                      0);
        }
    }
  return pspec;
}

static void
gwa_update_properties_from_type (GladeWidgetAdaptor *adaptor,
                                 GType               type,
                                 GList             **properties,
                                 gboolean            is_packing)
{
  gpointer object_class = g_type_class_ref (type);
  GParamSpec **specs = NULL, *spec;
  guint i, n_specs = 0;

  /* only GtkContainer child propeties can be introspected */
  if (is_packing && !g_type_is_a (adaptor->priv->type, GTK_TYPE_CONTAINER))
    return;

  if (is_packing)
    specs = gtk_container_class_list_child_properties (object_class, &n_specs);
  else
    specs = g_object_class_list_properties (object_class, &n_specs);

  for (i = 0; i < n_specs; i++)
    {
      GladePropertyClass *property_class;
      GList *list;

      /* find the property in our list, if not found append a new property */
      for (list = *properties; list && list->data; list = list->next)
        {
          property_class = GLADE_PROPERTY_CLASS (list->data);
          if (glade_property_class_id (property_class) != NULL &&
              g_ascii_strcasecmp (specs[i]->name, glade_property_class_id (property_class)) == 0)
            break;
        }

      if (list == NULL && (specs[i]->flags & G_PARAM_WRITABLE) &&
          (spec = pspec_dup (specs[i])))
        {
          property_class = glade_property_class_new (adaptor, spec->name);

          glade_property_class_set_pspec (property_class, spec);

          /* Make sure we can tell properties apart by there 
           * owning class.
           */
          spec->owner_type = adaptor->priv->type;

          /* Disable properties by default since the does not really implement them */
          glade_property_class_set_virtual (property_class, TRUE);
          glade_property_class_set_ignore (property_class, TRUE);

          glade_property_class_set_tooltip (property_class, g_param_spec_get_blurb (spec));
          glade_property_class_set_name (property_class, g_param_spec_get_nick (spec));

          if (spec->flags & G_PARAM_CONSTRUCT_ONLY)
            glade_property_class_set_construct_only (property_class, TRUE);

          glade_property_class_load_defaults_from_spec (property_class);
          glade_property_class_set_is_packing (property_class, is_packing);

          *properties = g_list_append (*properties, property_class);
        }
    }

  g_free (specs);
}

static void
gwa_action_update_from_node (GladeWidgetAdaptor *adaptor,
                             gboolean            is_packing,
                             GladeXmlNode       *node,
                             const gchar        *domain,
                             gchar              *group_path)
{
  GladeXmlNode *child;
  gchar *id, *label, *stock, *action_path;
  gboolean group, important;

  for (child = glade_xml_node_get_children (node);
       child; child = glade_xml_node_next (child))
    {
      if ((group =
           glade_xml_node_verify_silent (child, GLADE_TAG_ACTION)) == FALSE)
        continue;

      id = glade_xml_get_property_string_required
          (child, GLADE_TAG_ID, adaptor->priv->name);
      if (id == NULL)
        continue;

      if (group_path)
        action_path = g_strdup_printf ("%s/%s", group_path, id);
      else
        action_path = id;

      label = glade_xml_get_property_string (child, GLADE_TAG_NAME);
      stock = glade_xml_get_property_string (child, GLADE_TAG_STOCK);
      important =
          glade_xml_get_property_boolean (child, GLADE_TAG_IMPORTANT, FALSE);

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
        glade_widget_adaptor_pack_action_add (adaptor, action_path, label,
                                              stock, important);
      else
        glade_widget_adaptor_action_add (adaptor, action_path, label, stock,
                                         important);

      if (group)
        gwa_action_update_from_node (adaptor, is_packing, child, domain,
                                     action_path);

      g_free (id);
      g_free (label);
      g_free (stock);
      if (group_path)
        g_free (action_path);
    }
}

static void
gwa_set_signals_from_node (GladeWidgetAdaptor *adaptor, 
                           GladeXmlNode       *node,
                           const gchar        *domain)
{
  GladeXmlNode   *child;
  GladeSignalDef *signal;
  GList          *list;
  gchar          *id;

  for (child = glade_xml_node_get_children (node);
       child; child = glade_xml_node_next (child))
    {
      if (!glade_xml_node_verify (child, GLADE_TAG_SIGNAL))
        continue;

      if (!(id = glade_xml_get_property_string_required (child, GLADE_TAG_ID, NULL)))
        continue;

      if ((list =
           g_list_find_custom (adaptor->priv->signals, id,
                               (GCompareFunc) gwa_signal_find_comp)) != NULL)
        {
          signal = list->data;

          glade_signal_def_update_from_node (signal, child, domain);
        }
      g_free (id);
    }
}

static GList *
gwa_internal_children_update_from_node (GList        *internal_children,
                                        GladeXmlNode *node)
{
  GList *retval = internal_children;
  GladeXmlNode *child, *children;
  
  for (child = node; child; child = glade_xml_node_next (child))
    {
      GladeInternalChild *data;
      gchar *name;

      if (!glade_xml_node_verify (child, GLADE_XML_TAG_WIDGET))
        continue;

      if (!(name = glade_xml_get_property_string_required (child, GLADE_TAG_NAME, NULL)))
        continue;

      if ((data = gwa_internal_child_find (retval, name)) == NULL)
        {
          gboolean anarchist = glade_xml_get_property_boolean (child, GLADE_TAG_ANARCHIST, FALSE);
          data = gwa_internal_children_new (name, anarchist);
          retval = g_list_prepend (retval, data);
        }
      
      if ((children = glade_xml_search_child (child, GLADE_XML_TAG_WIDGET)))
        data->children = gwa_internal_children_update_from_node (data->children, children);

      g_free (name);
    }
  
  return retval;
}

static gboolean
gwa_extend_with_node (GladeWidgetAdaptor *adaptor,
                      GladeXmlNode       *node,
                      GModule            *module,
                      const gchar        *domain)
{
  GladeXmlNode *child;
  gchar *child_type;

  /* Override the special-child-type here */
  if ((child_type =
       glade_xml_get_value_string (node, GLADE_TAG_SPECIAL_CHILD_TYPE)) != NULL)
    adaptor->priv->special_child_type =
        (g_free (adaptor->priv->special_child_type), child_type);

  if ((child = glade_xml_search_child (node, GLADE_TAG_PROPERTIES)) != NULL)
    gwa_update_properties_from_node
        (adaptor, child, module, &adaptor->priv->properties, domain, FALSE);

  if ((child =
       glade_xml_search_child (node, GLADE_TAG_PACKING_PROPERTIES)) != NULL)
    gwa_update_properties_from_node
        (adaptor, child, module, &adaptor->priv->packing_props, domain, TRUE);

  if ((child =
       glade_xml_search_child (node, GLADE_TAG_PACKING_DEFAULTS)) != NULL)
    gwa_set_packing_defaults_from_node (adaptor, child);

  if ((child = glade_xml_search_child (node, GLADE_TAG_SIGNALS)) != NULL)
    gwa_set_signals_from_node (adaptor, child, domain);

  if ((child = glade_xml_search_child (node, GLADE_TAG_ACTIONS)) != NULL)
    gwa_action_update_from_node (adaptor, FALSE, child, domain, NULL);

  if ((child =
       glade_xml_search_child (node, GLADE_TAG_PACKING_ACTIONS)) != NULL)
    gwa_action_update_from_node (adaptor, TRUE, child, domain, NULL);

  if ((child = glade_xml_search_child (node, GLADE_TAG_INTERNAL_CHILDREN)))
    adaptor->priv->internal_children = 
      gwa_internal_children_update_from_node (adaptor->priv->internal_children,
                                              glade_xml_node_get_children (child));

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
      G_TYPE_IS_ABSTRACT (class_type) != FALSE || generic_name == NULL)
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
  GList *l, *p = (packing) ? adaptor->priv->packing_props : adaptor->priv->properties;

  for (l = p; l; l = g_list_next (l))
    {
      GladePropertyClass *klass = l->data;
      GParamSpec         *pspec = glade_property_class_get_pspec (klass);

      if (adaptor->priv->type == pspec->owner_type && 
          glade_property_class_is_visible (klass) &&
          (G_IS_PARAM_SPEC_ENUM (pspec) ||
           G_IS_PARAM_SPEC_FLAGS (pspec)) &&
          !glade_type_has_displayable_values (pspec->value_type) &&
          pspec->value_type != GLADE_TYPE_STOCK &&
          pspec->value_type != GLADE_TYPE_STOCK_IMAGE)
        {
          /* We do not need displayable values if the property is not visible */
          if (g_getenv (GLADE_ENV_TESTING) == NULL)
            g_message ("No displayable values for %sproperty %s::%s",
                       (packing) ? "child " : "", adaptor->priv->name, 
                       glade_property_class_id (klass));
        }
    }
}

static GType
generate_type (const char *name, const char *parent_name)
{
  GType parent_type, retval;
  GTypeQuery query;
  GTypeInfo *type_info;
  char *new_name;

  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (parent_name != NULL, 0);

  parent_type = glade_util_get_type_from_name (parent_name, FALSE);
  g_return_val_if_fail (parent_type != 0, 0);

  g_type_query (parent_type, &query);
  g_return_val_if_fail (query.type != 0, 0);

  /*
   * If the type already exist it means we want to use the parent type
   * instead of the real one at runtime.
   * This is currently used to instantiate GtkWindow as a GtkEventBox
   * at runtime.   
   */
  if (glade_util_get_type_from_name (name, FALSE))
    new_name = g_strconcat ("GladeFake", name, NULL);
  else
    new_name = NULL;

  type_info = g_new0 (GTypeInfo, 1);
  type_info->class_size = query.class_size;
  type_info->instance_size = query.instance_size;

  retval = g_type_register_static (parent_type,
                                   (new_name) ? new_name : name, type_info, 0);

  g_free (new_name);

  return retval;
}

static gchar *
generate_deprecated_icon (const gchar *icon_name)
{
  static GdkPixbuf *deprecated_pixbuf[2] = { NULL, NULL };
  GError        *error = NULL;
  GdkPixbuf     *orig_pixbuf[2];
  gchar         *deprecated;

  if (strncmp (icon_name, "deprecated-", strlen ("deprecated-")) == 0)
    return g_strdup (icon_name);

  /* Get deprecated graphics */
  if (!deprecated_pixbuf[0])
    {
      gchar *filename;

      filename = g_build_filename (glade_app_get_pixmaps_dir (), "deprecated-16x16.png", NULL);

      if ((deprecated_pixbuf[0] = gdk_pixbuf_new_from_file (filename, &error)) == NULL)
        {
          g_warning ("Unable to render deprecated icon: %s", error->message);
          error = (g_error_free (error), NULL);
        }
      g_free (filename);

      filename = g_build_filename (glade_app_get_pixmaps_dir (), "deprecated-22x22.png", NULL);

      if ((deprecated_pixbuf[1] = gdk_pixbuf_new_from_file (filename, &error)) == NULL)
        {
          g_warning ("Unable to render deprecated icon: %s", error->message);
          error = (g_error_free (error), NULL);
        }
      g_free (filename);
    }

  if (!deprecated_pixbuf[0] || !deprecated_pixbuf[1])
      return NULL;

  /* Load pixbuf's for the current icons */
  if ((orig_pixbuf[0] = 
       gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                 icon_name, 16, 0, &error)) == NULL)
    {
      g_warning ("Unable to render icon %s at size 16: %s", icon_name, error->message);
      error = (g_error_free (error), NULL);
    }
  else
    {
      GdkPixbuf *tmp = gdk_pixbuf_copy (orig_pixbuf[0]);
      g_object_unref (orig_pixbuf[0]);
      orig_pixbuf[0] = tmp;
    }

  if ((orig_pixbuf[1] = 
       gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                 icon_name, 22, 0, &error)) == NULL)
    {
      g_warning ("Unable to render icon %s at size 22: %s", icon_name, error->message);
      error = (g_error_free (error), NULL);
    }
  else
    {
      GdkPixbuf *tmp = gdk_pixbuf_copy (orig_pixbuf[1]);
      g_object_unref (orig_pixbuf[1]);
      orig_pixbuf[1] = tmp;
    }

  deprecated = g_strdup_printf ("deprecated-%s", icon_name);

  if (!gtk_icon_theme_has_icon (gtk_icon_theme_get_default (), deprecated))
    {
      if (orig_pixbuf[0])
        gdk_pixbuf_composite (deprecated_pixbuf[0], orig_pixbuf[0],
                                0, 0, 16, 16, 0, 0, 1, 1, GDK_INTERP_NEAREST, 255);

      if (orig_pixbuf[1])
        gdk_pixbuf_composite (deprecated_pixbuf[1], orig_pixbuf[1],
                                0, 0, 22, 22, 0, 0, 1, 1, GDK_INTERP_NEAREST, 255);

      gtk_icon_theme_add_builtin_icon (deprecated, 16, orig_pixbuf[0]);
      gtk_icon_theme_add_builtin_icon (deprecated, 22, orig_pixbuf[1]);
    }

  if (orig_pixbuf[0])
    g_object_unref (orig_pixbuf[0]);

  if (orig_pixbuf[1])
    g_object_unref (orig_pixbuf[1]);

  return deprecated;
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
 *
 * Returns: (transfer full): a newly allocated #GladeWidgetAdaptor
 */
GladeWidgetAdaptor *
glade_widget_adaptor_from_catalog (GladeCatalog *catalog,
                                   GladeXmlNode *class_node,
                                   GModule      *module)
{
  GladeWidgetAdaptor *adaptor = NULL;
  gchar *name, *generic_name, *icon_name, *adaptor_icon_name, *adaptor_name,
      *func_name, *template;
  gchar *title, *translated_title, *parent_name;
  GType object_type, adaptor_type, parent_type;
  gchar *missing_icon = NULL;
  GWADerivedClassData data;


  if (!glade_xml_node_verify (class_node, GLADE_TAG_GLADE_WIDGET_CLASS))
    {
      g_warning ("Widget class node is not '%s'", GLADE_TAG_GLADE_WIDGET_CLASS);
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
   * - setting a template ui definition
   */
  if ((parent_name =
       glade_xml_get_property_string (class_node, GLADE_TAG_PARENT)) != NULL)
    {
      if (!glade_widget_adaptor_get_by_name (parent_name))
        {
          g_warning
              ("Trying to define class '%s' for parent class '%s', but parent class '%s' "
               "is not registered", name, parent_name, parent_name);
          g_free (name);
          g_free (parent_name);
          return NULL;
        }
      object_type = generate_type (name, parent_name);
      g_free (parent_name);
    }
  else if ((func_name =
            glade_xml_get_property_string (class_node,
                                           GLADE_TAG_GET_TYPE_FUNCTION)) != NULL)
    {
      object_type = glade_util_get_type_from_name (func_name, TRUE);
      g_free (func_name);
    }
  else if ((template =
            glade_xml_get_property_string (class_node,
                                           GLADE_XML_TAG_TEMPLATE)) != NULL)
    {
      object_type = _glade_template_generate_type_from_file (catalog, name, template);
      g_free (template);
    }
  else
    {
      GType type_iter;

      object_type = glade_util_get_type_from_name (name, FALSE);

      for (type_iter = g_type_parent (object_type);
           type_iter; type_iter = g_type_parent (type_iter))
        {
          GladeWidgetAdaptor *a =
              glade_widget_adaptor_get_by_name (g_type_name (type_iter));

          if (a && a->priv->real_type != a->priv->type)
            {
              object_type = generate_type (name, g_type_name (a->priv->type));
              break;
            }
        }
    }

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

  if ((adaptor_name =
       glade_xml_get_property_string (class_node, GLADE_TAG_ADAPTOR)))
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

  generic_name =
      glade_xml_get_property_string (class_node, GLADE_TAG_GENERIC_NAME);
  icon_name = glade_xml_get_property_string (class_node, GLADE_TAG_ICON_NAME);

  /* get a suitable icon name for adaptor */
  adaptor_icon_name =
      create_icon_name_for_object_class (name,
                                         object_type,
                                         icon_name,
                                         glade_catalog_get_icon_prefix
                                         (catalog), generic_name);


  /* check if icon is available (a null icon-name is an abstract class) */
  if (adaptor_icon_name &&
      !gtk_icon_theme_has_icon (gtk_icon_theme_get_default (),
                                adaptor_icon_name))
    {
      GladeWidgetAdaptor *parent =
          glade_widget_adaptor_get_parent_adaptor_by_type (object_type);

      /* Save the desired name */
      missing_icon = adaptor_icon_name;

      adaptor_icon_name = g_strdup ((parent && parent->priv->icon_name) ?
                                    parent->priv->icon_name : DEFAULT_ICON_NAME);

    }

  adaptor = g_object_new (adaptor_type,
                          "type", object_type,
                          "name", name,
                          "catalog", glade_catalog_get_name (catalog),
                          "generic-name", generic_name,
                          "icon-name", adaptor_icon_name, NULL);

  adaptor->priv->missing_icon = missing_icon;

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
      g_warning ("Class '%s' declared without a '%s' attribute", name,
                 GLADE_TAG_TITLE);
      adaptor->priv->title = g_strdup (name);
    }
  else
    {
      /* translate */
      translated_title = dgettext (glade_catalog_get_domain (catalog), title);
      if (translated_title != title)
        {
          /* gettext owns translated_title */
          adaptor->priv->title = g_strdup (translated_title);
          g_free (title);
        }
      else
        {
          adaptor->priv->title = title;
        }
    }

  if (G_TYPE_IS_INSTANTIATABLE (adaptor->priv->type) &&
      G_TYPE_IS_ABSTRACT (adaptor->priv->type) == FALSE &&
      adaptor->priv->generic_name == NULL)
    {
      g_warning ("Instantiatable class '%s' built without a '%s'",
                 name, GLADE_TAG_GENERIC_NAME);
    }

  /* if adaptor->priv->type (the runtime used by glade) differs from adaptor->priv->name
   * (the name specified in the catalog) means we are using the type specified in the
   * the parent tag as the runtime and the class already exist.
   * So we need to add the properties and signals from the real class
   * even though they wont be aplied at runtime.
   */
  if (adaptor->priv->type != adaptor->priv->real_type)
    {
      if (adaptor->priv->signals)
        g_list_free_full (adaptor->priv->signals,
                          (GDestroyNotify) glade_signal_def_free);

      adaptor->priv->signals = gwa_list_signals (adaptor, adaptor->priv->real_type);

      gwa_update_properties_from_type (adaptor, adaptor->priv->real_type,
                                       &adaptor->priv->properties, FALSE);
      gwa_update_properties_from_type (adaptor, adaptor->priv->real_type,
                                       &adaptor->priv->packing_props, TRUE);
    }

  /* Perform a stoopid fallback just incase */
  if (adaptor->priv->generic_name == NULL)
    adaptor->priv->generic_name = g_strdup ("widget");

  g_clear_pointer (&adaptor->priv->catalog, g_free);
  adaptor->priv->catalog = g_strdup (glade_catalog_get_name (catalog));

  if (glade_catalog_get_book (catalog))
    adaptor->priv->book = g_strdup (glade_catalog_get_book (catalog));

  gwa_extend_with_node (adaptor, class_node, module,
                        glade_catalog_get_domain (catalog));

  /* Finalize the icon and overlay it if it's deprecated */
  if (GWA_DEPRECATED (adaptor))
    {
      gchar *deprecated_icon = generate_deprecated_icon (adaptor->priv->icon_name);

      g_free (adaptor->priv->icon_name);
      adaptor->priv->icon_name = deprecated_icon;
    }

  /* Postpone creating the cursor until we have the right graphic for it */
  gwa_create_cursor (adaptor);

  /* Set default weight on properties */
  for (parent_type = adaptor->priv->type;
       parent_type != 0; parent_type = g_type_parent (parent_type))
    {
      glade_property_class_set_weights (&adaptor->priv->properties, parent_type);
      glade_property_class_set_weights (&adaptor->priv->packing_props, parent_type);
    }

  gwa_displayable_values_check (adaptor, FALSE);
  gwa_displayable_values_check (adaptor, TRUE);

  glade_widget_adaptor_register (adaptor);

  g_free (name);

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
 * Returns: (transfer full): a freshly created #GladeWidget wrapper object for the
 *          @internal_object of name @internal_name
 */
GladeWidget *
glade_widget_adaptor_create_internal (GladeWidget      *parent,
                                      GObject          *internal_object,
                                      const gchar      *internal_name,
                                      const gchar      *parent_name,
                                      gboolean          anarchist,
                                      GladeCreateReason reason)
{
  GladeWidgetAdaptor *adaptor;
  GladeProject *project;

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
                                             "object", internal_object, NULL);
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
 * Returns: (transfer full): the newly created #GladeWidget
 */

/**
 * glade_widget_adaptor_create_widget_real:
 * @query: whether to display query dialogs if
 *         applicable to the class
 * @first_property: the first property of @...
 * @...: a %NULL terminated list of string/value pairs of #GladeWidget 
 *       properties
 *
 * The macro glade_widget_adaptor_create_widget() uses this function
 * glade_widget_adaptor_create_widget_real(@query, "adaptor", adaptor, @...)
 *
 * Use glade_widget_adaptor_create_widget() in C as this function is mostly
 * available for languages where macros are not available.
 *
 * Returns: (transfer full): the newly created #GladeWidget
 */
GladeWidget *
glade_widget_adaptor_create_widget_real (gboolean     query,
                                         const gchar *first_property,
                                         ...)
{
  GladeWidgetAdaptor *adaptor;
  GladeWidget *gwidget;
  va_list vl, vl_copy;

  g_return_val_if_fail (strcmp (first_property, "adaptor") == 0, NULL);

  va_start (vl, first_property);
  va_copy (vl_copy, vl);

  adaptor = va_arg (vl, GladeWidgetAdaptor *);

  va_end (vl);

  if (GLADE_IS_WIDGET_ADAPTOR (adaptor) == FALSE)
    {
      g_critical
          ("No adaptor found in glade_widget_adaptor_create_widget_real args");
      va_end (vl_copy);
      return NULL;
    }

  gwidget =
      GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->create_widget (adaptor,
                                                               first_property,
                                                               vl_copy);

  va_end (vl_copy);

  if (query && glade_widget_adaptor_query (adaptor))
    {
      /* If user pressed cancel on query popup. */
      if (!glade_editor_query_dialog (gwidget))
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
 * Returns: (transfer none) (nullable): an existing #GladeWidgetAdaptor with the
 *          name equaling @name, or %NULL if such a class doesn't exist
 **/
GladeWidgetAdaptor *
glade_widget_adaptor_get_by_name (const gchar *name)
{
  GType type = g_type_from_name (name);

  if (adaptor_hash && type)
    return g_hash_table_lookup (adaptor_hash, GSIZE_TO_POINTER (type));

  return NULL;
}


/**
 * glade_widget_adaptor_get_by_type:
 * @type: the #GType of an object class
 *
 * Returns: (transfer none) (nullable): an existing #GladeWidgetAdaptor with the
 *          type equaling @type, or %NULL if such a class doesn't exist
 **/
GladeWidgetAdaptor *
glade_widget_adaptor_get_by_type (GType type)
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
 * Returns: (transfer none): the closest #GladeWidgetAdaptor in the ancestry to @adaptor
 *          which is responsable for introducing @pspec.
 **/
GladeWidgetAdaptor *
glade_widget_adaptor_from_pspec (GladeWidgetAdaptor *adaptor,
                                 GParamSpec         *pspec)
{
  GladeWidgetAdaptor *spec_adaptor;
  GType spec_type = pspec->owner_type;

  if (!spec_type)
    return adaptor;

  spec_adaptor = glade_widget_adaptor_get_by_type (pspec->owner_type);

  g_return_val_if_fail (g_type_is_a (adaptor->priv->type, pspec->owner_type), NULL);

  while (spec_type && !spec_adaptor && spec_type != adaptor->priv->type)
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
GladePropertyClass *
glade_widget_adaptor_get_property_class (GladeWidgetAdaptor *adaptor,
                                         const gchar        *name)
{
  GList *list;
  GladePropertyClass *pclass;

  for (list = adaptor->priv->properties; list && list->data; list = list->next)
    {
      pclass = list->data;
      if (strcmp (glade_property_class_id (pclass), name) == 0)
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
GladePropertyClass *
glade_widget_adaptor_get_pack_property_class (GladeWidgetAdaptor *adaptor,
                                              const gchar        *name)
{
  GList *list;
  GladePropertyClass *pclass;

  for (list = adaptor->priv->packing_props; list && list->data; list = list->next)
    {
      pclass = list->data;
      if (strcmp (glade_property_class_id (pclass), name) == 0)
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
  GArray *params;
  GObjectClass *oclass;
  GParamSpec **pspec;
  GladePropertyClass *pclass;
  guint n_props, i;

  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
  g_return_val_if_fail (n_params != NULL, NULL);

  /* As a slight optimization, we never unref the class
   */
  oclass = g_type_class_ref (adaptor->priv->type);
  pspec = g_object_class_list_properties (oclass, &n_props);
  params = g_array_new (FALSE, FALSE, sizeof (GParameter));

  for (i = 0; i < n_props; i++)
    {
      GParameter parameter = { 0, };

      pclass = glade_widget_adaptor_get_property_class
          (adaptor, pspec[i]->name);

      /* Ignore properties based on some criteria
       */
      if (pclass == NULL ||     /* Unaccounted for in the builder */
          glade_property_class_get_virtual (pclass) || /* should not be set before 
                                                          GladeWidget wrapper exists */
          glade_property_class_get_ignore (pclass))    /* Catalog explicitly ignores the object */
        continue;

      if (construct &&
          (pspec[i]->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY)) == 0)
        continue;
      else if (!construct &&
               (pspec[i]->flags &
                (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY)) != 0)
        continue;


      if (g_value_type_compatible (G_VALUE_TYPE (glade_property_class_get_default (pclass)),
                                   pspec[i]->value_type) == FALSE)
        {
          g_critical ("Type mismatch on %s property of %s",
                      parameter.name, adaptor->priv->name);
          continue;
        }

      if (g_param_values_cmp (pspec[i], 
                              glade_property_class_get_default (pclass), 
                              glade_property_class_get_original_default (pclass)) == 0)
        continue;

      parameter.name = pspec[i]->name;  /* These are not copied/freed */
      g_value_init (&parameter.value, pspec[i]->value_type);
      g_value_copy (glade_property_class_get_default (pclass), &parameter.value);

      g_array_append_val (params, parameter);
    }
  g_free (pspec);

  *n_params = params->len;
  return (GParameter *) g_array_free (params, FALSE);
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
 * Returns: (transfer full): A newly created #GObject
 */
GObject *
glade_widget_adaptor_construct_object (GladeWidgetAdaptor *adaptor,
                                       guint               n_parameters,
                                       GParameter         *parameters)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  return GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->construct_object (adaptor,
                                                                     n_parameters,
                                                                     parameters);
}

/**
 * glade_widget_adaptor_destroy_object:
 * @adaptor: A #GladeWidgetAdaptor
 * @object: The object to destroy
 *
 * This function is called to destroy a GObject instance.
 */
void
glade_widget_adaptor_destroy_object (GladeWidgetAdaptor *adaptor,
                                     GObject            *object)
{
  g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));

  GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->destroy_object (adaptor, object);
}

static void
gwa_internal_children_create (GladeWidgetAdaptor *adaptor,
                              GObject            *parent_object,
                              GObject            *object,
                              GList              *children,
                              GladeCreateReason  reason)
{
  gchar *parent_name = adaptor->priv->generic_name;
  GladeWidget *gobject = glade_widget_get_from_gobject (object);
  GList *l;

  for (l = children; l; l = g_list_next (l))
    {
      GladeInternalChild *internal = l->data;
      GObject *child;

      child = glade_widget_adaptor_get_internal_child (adaptor,
                                                       parent_object,
                                                       internal->name);

      if (child)
        {
          glade_widget_adaptor_create_internal (gobject,
                                                child,
                                                internal->name, 
                                                parent_name,
                                                internal->anarchist, 
                                                reason);

          if (internal->children)
            gwa_internal_children_create (adaptor, parent_object, child, internal->children, reason);
        }
    }
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
  g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->priv->type));

  /* Create internal widgets */
  if (adaptor->priv->internal_children)
      gwa_internal_children_create (adaptor, object, object, adaptor->priv->internal_children, reason);
  
  /* Run post_create in 2 stages, one that chains up and all class adaptors
   * in the hierarchy get a peek, another that is used to setup placeholders
   * and things that differ from the child/parent implementations
   */
  if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->deep_post_create)
    GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->deep_post_create (adaptor, object,
                                                                reason);

  if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->post_create)
    GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->post_create (adaptor, object,
                                                           reason);
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
 * Returns: (transfer none) (nullable): The internal #GObject
 */
GObject *
glade_widget_adaptor_get_internal_child (GladeWidgetAdaptor *adaptor,
                                         GObject            *object,
                                         const gchar        *internal_name)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (internal_name != NULL, NULL);
  g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->priv->type),
                        NULL);

  if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->get_internal_child)
    return GLADE_WIDGET_ADAPTOR_GET_CLASS
        (adaptor)->get_internal_child (adaptor, object, internal_name);
  else
    g_critical ("No get_internal_child() support in adaptor %s", adaptor->priv->name);

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
  g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->priv->type));

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
  g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->priv->type));

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
glade_widget_adaptor_verify_property (GladeWidgetAdaptor *adaptor,
                                      GObject            *object,
                                      const gchar        *property_name,
                                      const GValue       *value)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);
  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
  g_return_val_if_fail (property_name != NULL && value != NULL, FALSE);
  g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->priv->type),
                        FALSE);

  if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->verify_property)
    return GLADE_WIDGET_ADAPTOR_GET_CLASS
        (adaptor)->verify_property (adaptor, object, property_name, value);

  return TRUE;
}

/**
 * glade_widget_adaptor_add_verify:
 * @adaptor:   A #GladeWidgetAdaptor
 * @container: A #GObject container
 * @child: A #GObject child
 * @user_feedback: whether a notification dialog should be
 * presented in the case that the child cannot not be added.
 *
 * Checks whether @child can be added to @parent.
 *
 * If @user_feedback is %TRUE and @child cannot be
 * added then this shows a notification dialog to the user 
 * explaining why.
 *
 * Returns: whether @child can be added to @parent.
 */
gboolean
glade_widget_adaptor_add_verify (GladeWidgetAdaptor *adaptor,
                                 GObject            *container,
                                 GObject            *child,
                                 gboolean            user_feedback)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);
  g_return_val_if_fail (G_IS_OBJECT (container), FALSE);
  g_return_val_if_fail (G_IS_OBJECT (child), FALSE);
  g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->priv->type), FALSE);

  return GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->add_verify (adaptor, container, child, user_feedback);
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
  g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->priv->type));

  if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->add)
    GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->add (adaptor, container, child);
  else
    g_critical ("No add() support in adaptor %s", adaptor->priv->name);
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
  g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->priv->type));

  if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->remove)
    GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->remove (adaptor, container,
                                                      child);
  else
    g_critical ("No remove() support in adaptor %s", adaptor->priv->name);
}

/**
 * glade_widget_adaptor_get_children:
 * @adaptor:   A #GladeWidgetAdaptor
 * @container: The #GObject container
 *
 * Lists the children of @container.
 *
 * Returns: (transfer container) (element-type GObject): A #GList of children
 */
GList *
glade_widget_adaptor_get_children (GladeWidgetAdaptor *adaptor,
                                   GObject            *container)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
  g_return_val_if_fail (G_IS_OBJECT (container), NULL);
  g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->priv->type),
                        NULL);

  return GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->get_children (adaptor, container);
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
  g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->priv->type));

  /* XXX Valgrind says that the above 'g_type_is_a' line allocates uninitialized stack memory
   * that is later used in glade_gtk_box_child_set_property, why ? */

  if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->child_set_property)
    GLADE_WIDGET_ADAPTOR_GET_CLASS
        (adaptor)->child_set_property (adaptor, container, child,
                                       property_name, value);
  else
    g_critical ("No child_set_property() support in adaptor %s", adaptor->priv->name);

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
  g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->priv->type));

  if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->child_get_property)
    GLADE_WIDGET_ADAPTOR_GET_CLASS
        (adaptor)->child_get_property (adaptor, container, child,
                                       property_name, value);
  else
    g_critical ("No child_set_property() support in adaptor %s", adaptor->priv->name);
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
  g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->priv->type),
                        FALSE);

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
  g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->priv->type));

  if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->replace_child)
    GLADE_WIDGET_ADAPTOR_GET_CLASS
        (adaptor)->replace_child (adaptor, container, old_obj, new_obj);
  else
    g_critical ("No replace_child() support in adaptor %s", adaptor->priv->name);
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

  if (!adaptor->priv->query)
    return FALSE;

  for (l = adaptor->priv->properties; l; l = l->next)
    {
      pclass = l->data;

      if (glade_property_class_query (pclass))
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
  GList *l;

  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (child_adaptor), NULL);
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (container_adaptor), NULL);

  if ((packing =
       glade_widget_adaptor_get_child_packing (child_adaptor,
                                               container_adaptor->priv->name)) !=
      NULL)
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
  return (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->add &&
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

  for (i = 0; tokens[i] && tokens[i + 1]; i++)
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
glade_widget_adaptor_action_add_real (GList       **list,
                                      const gchar  *action_path,
                                      const gchar  *label,
                                      const gchar  *stock, 
                                      gboolean      important)
{
  GWActionClass *action, *group;
  const gchar *id;

  id = gwa_action_path_get_id (action_path);

  if ((group = gwa_action_get_last_group (*list, action_path)))
    list = &group->actions;

  if (strcmp (label, "") == 0)
    label = NULL;
  if (stock && strcmp (stock, "") == 0)
    stock = NULL;

  if ((action = gwa_action_lookup (*list, id)) == NULL)
    {
      /* New Action */
      action = glade_widget_action_class_new (action_path);

      *list = g_list_append (*list, action);
    }

  glade_widget_action_class_set_label (action, label);
  glade_widget_action_class_set_stock (action, stock);
  glade_widget_action_class_set_important (action, important);

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
                                 const gchar        *action_path,
                                 const gchar        *label,
                                 const gchar        *stock,
                                 gboolean            important)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);
  g_return_val_if_fail (action_path != NULL, FALSE);

  return glade_widget_adaptor_action_add_real (&adaptor->priv->actions,
                                               action_path,
                                               label, stock, important);
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
                                      const gchar        *action_path,
                                      const gchar        *label,
                                      const gchar        *stock,
                                      gboolean            important)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);
  g_return_val_if_fail (action_path != NULL, FALSE);

  return glade_widget_adaptor_action_add_real (&adaptor->priv->packing_actions,
                                               action_path,
                                               label, stock, important);
}

static gboolean
glade_widget_adaptor_action_remove_real (GList **list,
                                         const gchar *action_path)
{
  GWActionClass *action, *group;
  const gchar *id;

  id = gwa_action_path_get_id (action_path);

  if ((group = gwa_action_get_last_group (*list, action_path)))
    list = &group->actions;

  if ((action = gwa_action_lookup (*list, id)) == NULL)
    return FALSE;

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
                                    const gchar        *action_path)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);
  g_return_val_if_fail (action_path != NULL, FALSE);

  return glade_widget_adaptor_action_remove_real (&adaptor->priv->actions,
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
                                         const gchar        *action_path)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), FALSE);
  g_return_val_if_fail (action_path != NULL, FALSE);

  return glade_widget_adaptor_action_remove_real (&adaptor->priv->packing_actions,
                                                  action_path);
}

/**
 * glade_widget_adaptor_actions_new:
 * @adaptor: A #GladeWidgetAdaptor
 *
 * Create a list of actions.
 *
 * Returns: (transfer full) (element-type GladeWidgetAction): a new list of GladeWidgetAction.
 */
GList *
glade_widget_adaptor_actions_new (GladeWidgetAdaptor *adaptor)
{
  GList *l, *list = NULL;

  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  for (l = adaptor->priv->actions; l; l = g_list_next (l))
    {
      GWActionClass *action = l->data;
      GObject       *obj = g_object_new (GLADE_TYPE_WIDGET_ACTION,
                                         "class", action, NULL);

      list = g_list_prepend (list, GLADE_WIDGET_ACTION (obj));
    }
  return g_list_reverse (list);
}

/**
 * glade_widget_adaptor_pack_actions_new:
 * @adaptor: A #GladeWidgetAdaptor
 *
 * Create a list of packing actions.
 *
 * Returns: (transfer full) (element-type GladeWidgetAction): a new list of GladeWidgetAction.
 */
GList *
glade_widget_adaptor_pack_actions_new (GladeWidgetAdaptor *adaptor)
{
  GList *l, *list = NULL;

  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  for (l = adaptor->priv->packing_actions; l; l = g_list_next (l))
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
  g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->priv->type));

  GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->action_activate (adaptor, object,
                                                             action_path);
}

/**
 * glade_widget_adaptor_child_action_activate:
 * @adaptor:   A #GladeWidgetAdaptor
 * @container: The #GObject container
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
  g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (container), adaptor->priv->type));

  GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->child_action_activate (adaptor,
                                                                   container,
                                                                   object,
                                                                   action_path);
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
 * Returns: (transfer full) (nullable): A newly created #GtkMenu or %NULL
 */
GtkWidget *
glade_widget_adaptor_action_submenu (GladeWidgetAdaptor *adaptor,
                                     GObject            *object,
                                     const gchar        *action_path)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (g_type_is_a (G_OBJECT_TYPE (object), adaptor->priv->type),
                        NULL);

  if (GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->action_submenu)
    return GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->action_submenu (adaptor,
                                                                     object,
                                                                     action_path);

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
 * 
 * Deprecated: 3.18
 */
gboolean
glade_widget_adaptor_depends (GladeWidgetAdaptor *adaptor,
                              GladeWidget        *widget,
                              GladeWidget        *another)
{
  return FALSE;
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
 * glade_widget_adaptor_write_widget_after:
 * @adaptor: A #GladeWidgetAdaptor
 * @widget: The #GladeWidget
 * @context: The #GladeXmlContext
 * @node: The #GladeXmlNode
 *
 * This function is called to write @widget to @node 
 * when writing xml files (after writing children)
 */
void
glade_widget_adaptor_write_widget_after (GladeWidgetAdaptor *adaptor,
                                         GladeWidget        *widget,
                                         GladeXmlContext    *context,
                                         GladeXmlNode       *node)
{
  g_return_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor));
  g_return_if_fail (GLADE_IS_WIDGET (widget));
  g_return_if_fail (node != NULL);

  GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->write_widget_after (adaptor, widget,
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
 * Returns: (transfer full): A newly created #GladeEditorProperty
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
 * Returns: (transfer full): A newly created #GladeEditorProperty
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
                                        const GValue       *value)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
  g_return_val_if_fail (GLADE_IS_PROPERTY_CLASS (klass), NULL);
  g_return_val_if_fail (value != NULL, NULL);

  return GLADE_WIDGET_ADAPTOR_GET_CLASS (adaptor)->string_from_value (adaptor,
                                                                      klass,
                                                                      value);
}


/**
 * glade_widget_adaptor_get_signal_def:
 * @adaptor: A #GladeWidgetAdaptor
 * @name: the name of the signal class.
 * 
 * Looks up signal class @name on @adaptor.
 *
 * Returns: (nullable) (transfer none): a #GladeSignalDef or %NULL
 */
GladeSignalDef *
glade_widget_adaptor_get_signal_def (GladeWidgetAdaptor *adaptor,
                                     const gchar        *name)
{
  GList *list;
  GladeSignalDef *signal;

  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  for (list = adaptor->priv->signals; list; list = list->next)
    {
      signal = list->data;
      if (!strcmp (glade_signal_def_get_name (signal), name))
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
 * Returns: (transfer full): A new #GladeEditable widget
 */
GladeEditable *
glade_widget_adaptor_create_editable (GladeWidgetAdaptor *adaptor,
                                      GladeEditorPageType type)
{
  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);

  return GLADE_WIDGET_ADAPTOR_GET_CLASS
      (adaptor)->create_editable (adaptor, type);
}
