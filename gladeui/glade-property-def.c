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
 *   Tristan Van Berkom <tristan.van.berkom@gmail.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/**
 * SECTION:glade-property-def
 * @Title: GladePropertyDef
 * @Short_Description: Property Definition metadata.
 *
 * #GladePropertyDef is a structure based on a #GParamSpec and parameters
 * from the Glade catalog files and describes how properties are to be handled
 * in Glade; it also provides an interface to convert #GValue to strings and
 * va_lists etc (back and forth).
 */

#include <string.h>
#include <stdlib.h>
#include <glib/gi18n-lib.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-property.h"
#include "glade-property-def.h"
#include "glade-editor-property.h"
#include "glade-displayable-values.h"
#include "glade-debug.h"

#define NUMERICAL_STEP_INCREMENT   1.0F
#define NUMERICAL_PAGE_INCREMENT   10.0F
#define NUMERICAL_PAGE_SIZE        0.0F

#define FLOATING_STEP_INCREMENT    0.01F
#define FLOATING_PAGE_INCREMENT    0.1F
#define FLOATING_PAGE_SIZE         0.00F


struct _GladePropertyDef
{
  GladeWidgetAdaptor *adaptor; /* The GladeWidgetAdaptor that this property class
                                * was created for.
                                */

  guint16     version_since_major; /* Version in which this property was */
  guint16     version_since_minor; /* introduced.                       */

  GParamSpec *pspec; /* The Parameter Specification for this property.
                      */

  gchar *id;       /* The id of the property. Like "label" or "xpad"
                    * this is a non-translatable string
                    */

  gchar *name;     /* The name of the property. Like "Label" or "X Pad"
                    * this is a translatable string
                    */

  gchar *tooltip; /* The default tooltip for the property editor rows.
                   */

  GValue *def;      /* The default value for this property (this will exist
                     * as a copy of orig_def if not specified by the catalog)
                     */

  GValue *orig_def; /* The real default value obtained through introspection.
                     * (used to decide whether we should write to the
                     * glade file or not, or to restore the loaded property
                     * correctly); all property classes have and orig_def.
                     */

  guint multiline : 1; /* Whether to use multiple lines to edit this text property.
                        */

  guint virt : 1; /* Whether this is a virtual property with its pspec supplied
                   * via the catalog (or hard code-paths); or FALSE if its a real
                   * GObject introspected property
                   */

  guint optional : 1; /* Some properties are optional by nature like
                       * default width. It can be set or not set. A
                       * default property has a check box in the
                       * left that enables/disables the input
                       */

  guint optional_default : 1; /* For optional values, what the default is */

  guint construct_only : 1; /* Whether this property is G_PARAM_CONSTRUCT_ONLY or not */
        
  guint common : 1;  /* Common properties go in the common tab */
  guint atk : 1;     /* Atk properties go in the atk tab */
  guint packing : 1; /* Packing properties go in the packing tab */
  guint query : 1;   /* Whether we should explicitly ask the user about this property
                      * when instantiating a widget with this property (through a popup
                      * dialog).
                      */
        
  guint translatable : 1; /* The property should be translatable, which
                           * means that it needs extra parameters in the
                           * UI.
                           */

  /* These three are the master switches for the glade-file output,
   * property editor availability & live object updates in the glade environment.
   */
  guint save : 1;      /* Whether we should save to the glade file or not
                        * (mostly just for virtual internal glade properties,
                        * also used for properties with generic pspecs that
                        * are saved in custom ways by the plugin)
                        */
  guint save_always : 1; /* Used to make a special case exception and always
                          * save this property regardless of what the default
                          * value is (used for some special cases like properties
                          * that are assigned initial values in composite widgets
                          * or derived widget code).
                          */
  guint visible : 1;   /* Whether or not to show this property in the editor &
                        * reset dialog.
                        */

  guint custom_layout : 1; /* Properties marked as custom_layout will not be included
                            * in a base #GladeEditorTable implementation (use this
                            * for properties you want to layout in custom ways in
                            * a #GladeEditable widget
                            */
        
  guint ignore : 1;    /* When true, we will not sync the object when the property
                        * changes, or load values from the object.
                        */

  guint needs_sync : 1; /* Virtual properties need to be synchronized after object
                         * creation, some properties that are not virtual also need
                         * handling from the backend, if "needs-sync" is true then
                         * this property will by synced with virtual properties.
                         */

  guint is_modified : 1; /* If true, this property_def has been "modified" from the
                          * the standard property by a xml file. */

  guint themed_icon : 1; /* Some GParamSpecString properties reffer to icon names
                          * in the icon theme... these need to be specified in the
                          * property class definition if proper editing tools are to
                          * be used.
                          */
  guint stock_icon : 1; /* String properties can also denote stock icons, including
                         * icons from icon factories...
                         */
  guint stock : 1;      /* ... or a narrower list of "items" from gtk builtin stock items.
                         */
        
  guint transfer_on_paste : 1; /* If this is a packing prop, 
                                * wether we should transfer it on paste.
                                */
        
  guint parentless_widget : 1;  /* True if this property should point to a parentless widget
                                 * in the project
                                 */

  guint deprecated : 1; /* True if this property is deprecated */

  gdouble weight;       /* This will determine the position of this property in 
                         * the editor.
                         */
        
  gchar *create_type; /* If this is an object property and you want the option to create
                       * one from the object selection dialog, then set the name of the
                       * concrete type here.
                       */
};

G_DEFINE_BOXED_TYPE (GladePropertyDef, glade_property_def, glade_property_def_clone, glade_property_def_free)

/**
 * glade_property_def_new:
 * @adaptor: The #GladeWidgetAdaptor to create this property for
 * @id: the id for the new property class
 *
 * Returns: a new #GladePropertyDef
 */
GladePropertyDef *
glade_property_def_new (GladeWidgetAdaptor *adaptor, 
                          const gchar        *id)
{
  GladePropertyDef *property_def;

  property_def = g_slice_new0 (GladePropertyDef);
  property_def->adaptor = adaptor;
  property_def->pspec = NULL;
  property_def->id = g_strdup (id);
  property_def->name = NULL;
  property_def->tooltip = NULL;
  property_def->def = NULL;
  property_def->orig_def = NULL;
  property_def->query = FALSE;
  property_def->optional = FALSE;
  property_def->optional_default = FALSE;
  property_def->is_modified = FALSE;
  property_def->common = FALSE;
  property_def->packing = FALSE;
  property_def->atk = FALSE;
  property_def->visible = TRUE;
  property_def->custom_layout = FALSE;
  property_def->save = TRUE;
  property_def->save_always = FALSE;
  property_def->ignore = FALSE;
  property_def->needs_sync = FALSE;
  property_def->themed_icon = FALSE;
  property_def->stock = FALSE;
  property_def->stock_icon = FALSE;
  property_def->translatable = FALSE;
  property_def->virt = TRUE;
  property_def->transfer_on_paste = FALSE;
  property_def->weight = -1.0;
  property_def->parentless_widget = FALSE;
  property_def->create_type = NULL;

  /* Initialize property versions & deprecated to adaptor */
  property_def->version_since_major = GWA_VERSION_SINCE_MAJOR (adaptor);
  property_def->version_since_minor = GWA_VERSION_SINCE_MINOR (adaptor);
  property_def->deprecated          = GWA_DEPRECATED (adaptor);

  return property_def;
}

/**
 * glade_property_def_clone:
 * @property_def: a #GladePropertyDef
 *
 * Returns: (transfer full): a new #GladePropertyDef cloned from @property_def
 */
GladePropertyDef *
glade_property_def_clone (GladePropertyDef *property_def)
{
  GladePropertyDef *clone;

  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), NULL);

  clone = g_new0 (GladePropertyDef, 1);

  /* copy ints over */
  memcpy (clone, property_def, sizeof (GladePropertyDef));

  /* Make sure we own our strings */
  clone->pspec = property_def->pspec;
  clone->id = g_strdup (clone->id);
  clone->name = g_strdup (clone->name);
  clone->tooltip = g_strdup (clone->tooltip);

  if (G_IS_VALUE (property_def->def))
    {
      clone->def = g_new0 (GValue, 1);
      g_value_init (clone->def, property_def->pspec->value_type);
      g_value_copy (property_def->def, clone->def);
    }

  if (G_IS_VALUE (property_def->orig_def))
    {
      clone->orig_def = g_new0 (GValue, 1);
      g_value_init (clone->orig_def, property_def->pspec->value_type);
      g_value_copy (property_def->orig_def, clone->orig_def);
    }

  if (property_def->create_type)
    clone->create_type = g_strdup (property_def->create_type);

  return clone;
}

/**
 * glade_property_def_free:
 * @property_def: a #GladePropertyDef
 *
 * Frees @property_def and its associated memory.
 */
void
glade_property_def_free (GladePropertyDef *property_def)
{
  if (property_def == NULL)
    return;

  g_return_if_fail (GLADE_IS_PROPERTY_DEF (property_def));

  g_clear_pointer (&property_def->id, g_free);
  g_clear_pointer (&property_def->name, g_free);
  g_clear_pointer (&property_def->tooltip, g_free);
  if (property_def->orig_def)
    {
      if (G_VALUE_TYPE (property_def->orig_def) != 0)
        g_value_unset (property_def->orig_def);
      g_clear_pointer (&property_def->orig_def, g_free);
    }
  if (property_def->def)
    {
      if (G_VALUE_TYPE (property_def->def) != 0)
        g_value_unset (property_def->def);
      g_clear_pointer (&property_def->def, g_free);
    }

  g_clear_pointer (&property_def->create_type, g_free);
  g_slice_free (GladePropertyDef, property_def);
}


GValue *
glade_property_def_get_default_from_spec (GParamSpec * spec)
{
  GValue *value;
  value = g_new0 (GValue, 1);
  g_value_init (value, spec->value_type);
  g_param_value_set_default (spec, value);
  return value;
}


static gchar *
glade_property_def_make_string_from_enum (GType etype, gint eval)
{
  GEnumClass *eclass;
  gchar *string = NULL;
  guint i;

  g_return_val_if_fail ((eclass = g_type_class_ref (etype)) != NULL, NULL);
  for (i = 0; i < eclass->n_values; i++)
    {
      if (eval == eclass->values[i].value)
        {
          string = g_strdup (eclass->values[i].value_nick);
          break;
        }
    }
  g_type_class_unref (eclass);
  return string;
}

static gchar *
glade_property_def_make_string_from_flags (GladePropertyDef * property_def,
                                             guint fvals, gboolean displayables)
{
  GFlagsClass *fclass;
  GFlagsValue *fvalue;
  GString *string;
  gchar *retval;

  g_return_val_if_fail ((fclass =
                         g_type_class_ref (property_def->pspec->value_type)) != NULL,
                        NULL);

  string = g_string_new ("");

  while ((fvalue = g_flags_get_first_value (fclass, fvals)) != NULL)
    {
      const gchar *val_str = NULL;

      fvals &= ~fvalue->value;

      if (displayables)
        val_str = glade_get_displayable_value (property_def->pspec->value_type,
                                               fvalue->value_name);

      if (string->str[0])
        g_string_append (string, " | ");

      g_string_append (string, (val_str) ? val_str : fvalue->value_name);

      /* If one of the flags value is 0 this loop become infinite :) */
      if (fvalue->value == 0)
        break;
    }

  retval = string->str;

  g_type_class_unref (fclass);
  g_string_free (string, FALSE);

  return retval;
}

static gchar *
glade_property_def_make_string_from_object (GladePropertyDef *property_def,
                                            GObject          *object)
{
  GladeWidget *gwidget;
  gchar *string = NULL, *filename;

  if (!object)
    return NULL;

  if (property_def->pspec->value_type == GDK_TYPE_PIXBUF)
    {
      if ((filename = g_object_get_data (object, "GladeFileName")) != NULL)
        string = g_strdup (filename);
    }
  else if (property_def->pspec->value_type == G_TYPE_FILE)
    {
      if ((filename = g_object_get_data (object, "GladeFileURI")) != NULL)
        string = g_strdup (filename);
    }
  else if ((gwidget = glade_widget_get_from_gobject (object)) != NULL)
    string = g_strdup (glade_widget_get_name (gwidget));
  else
    g_critical ("Object type property refers to an object "
                "outside the project");

  return string;
}

static gchar *
glade_property_def_make_string_from_objects (GladePropertyDef *
                                               property_def, GList * objects)
{
  GObject *object;
  GList *list;
  gchar *string = NULL, *obj_str, *tmp;

  for (list = objects; list; list = list->next)
    {
      object = list->data;

      obj_str =
          glade_property_def_make_string_from_object (property_def, object);

      if (string == NULL)
        string = obj_str;
      else if (obj_str != NULL)
        {
          tmp =
              g_strdup_printf ("%s%s%s", string, GLADE_PROPERTY_DEF_OBJECT_DELIMITER, obj_str);
          string = (g_free (string), tmp);
          g_free (obj_str);
        }
    }
  return string;
}

static gchar *
glade_dtostr (double number, gdouble epsilon)
{
  char *str = g_malloc (G_ASCII_DTOSTR_BUF_SIZE);
  int i;

  for (i = 0; i <= 20; i++)
    {
      double rounded;

      snprintf (str, G_ASCII_DTOSTR_BUF_SIZE, "%.*f", i, number);
      rounded = g_ascii_strtod (str, NULL);

      if (ABS (rounded - number) <= epsilon)
        return str;
    }

  return str;
}

/**
 * glade_property_def_make_string_from_gvalue:
 * @property_def: A #GladePropertyDef
 * @value: A #GValue
 *
 * Returns: A newly allocated string representation of @value
 */
gchar *
glade_property_def_make_string_from_gvalue (GladePropertyDef *
                                              property_def,
                                              const GValue * value)
{
  gchar *string = NULL, **strv;
  GObject *object;
  GdkColor *color;
  GdkRGBA *rgba;
  GList *objects;

  if (G_IS_PARAM_SPEC_ENUM (property_def->pspec))
    {
      gint eval = g_value_get_enum (value);
      string = glade_property_def_make_string_from_enum
          (property_def->pspec->value_type, eval);
    }
  else if (G_IS_PARAM_SPEC_FLAGS (property_def->pspec))
    {
      guint flags = g_value_get_flags (value);
      string = glade_property_def_make_string_from_flags
          (property_def, flags, FALSE);
    }
  else if (G_IS_PARAM_SPEC_VALUE_ARRAY (property_def->pspec))
    {
      GValueArray *value_array = g_value_get_boxed (value);

      if (value_array && value_array->n_values &&
          G_VALUE_HOLDS (&value_array->values[0], G_TYPE_STRING))
        {
          gint i, n_values = value_array->n_values;
          GString *gstring = g_string_new (NULL);

          for (i = 0; i < n_values; i++)
            {
              g_string_append (gstring,
                               g_value_get_string (&value_array->values[i]));
              g_string_append_c (gstring, '\n');
            }
          string = gstring->str;
          g_string_free (gstring, FALSE);
        }
    }
  else if (G_IS_PARAM_SPEC_BOXED (property_def->pspec))
    {
      if (property_def->pspec->value_type == GDK_TYPE_COLOR)
        {
          color = g_value_get_boxed (value);
          if (color)
            string = g_strdup_printf ("#%04x%04x%04x",
                                      color->red, color->green, color->blue);
        }
      else if (property_def->pspec->value_type == GDK_TYPE_RGBA)
        {
          rgba = g_value_get_boxed (value);
          if (rgba)
            string = gdk_rgba_to_string (rgba);
        }
      else if (property_def->pspec->value_type == G_TYPE_STRV)
        {
          strv = g_value_get_boxed (value);
          if (strv)
            string = g_strjoinv ("\n", strv);
        }
    }
  else if (G_IS_PARAM_SPEC_INT (property_def->pspec))
    string = g_strdup_printf ("%d", g_value_get_int (value));
  else if (G_IS_PARAM_SPEC_UINT (property_def->pspec))
    string = g_strdup_printf ("%u", g_value_get_uint (value));
  else if (G_IS_PARAM_SPEC_LONG (property_def->pspec))
    string = g_strdup_printf ("%ld", g_value_get_long (value));
  else if (G_IS_PARAM_SPEC_ULONG (property_def->pspec))
    string = g_strdup_printf ("%lu", g_value_get_ulong (value));
  else if (G_IS_PARAM_SPEC_INT64 (property_def->pspec))
    string = g_strdup_printf ("%" G_GINT64_FORMAT, g_value_get_int64 (value));
  else if (G_IS_PARAM_SPEC_UINT64 (property_def->pspec))
    string = g_strdup_printf ("%" G_GUINT64_FORMAT, g_value_get_uint64 (value));
  else if (G_IS_PARAM_SPEC_FLOAT (property_def->pspec))
    string = glade_dtostr (g_value_get_float (value),
                           ((GParamSpecFloat *)property_def->pspec)->epsilon);
  else if (G_IS_PARAM_SPEC_DOUBLE (property_def->pspec))
    string = glade_dtostr (g_value_get_double (value),
                           ((GParamSpecDouble *)property_def->pspec)->epsilon);
  else if (G_IS_PARAM_SPEC_STRING (property_def->pspec))
    {
      string = g_value_dup_string (value);
    }
  else if (G_IS_PARAM_SPEC_CHAR (property_def->pspec))
    string = g_strdup_printf ("%c", g_value_get_schar (value));
  else if (G_IS_PARAM_SPEC_UCHAR (property_def->pspec))
    string = g_strdup_printf ("%c", g_value_get_uchar (value));
  else if (G_IS_PARAM_SPEC_UNICHAR (property_def->pspec))
    {
      int len;
      string = g_malloc (7);
      len = g_unichar_to_utf8 (g_value_get_uint (value), string);
      string[len] = '\0';
    }
  else if (G_IS_PARAM_SPEC_BOOLEAN (property_def->pspec))
    string = g_strdup_printf ("%s", g_value_get_boolean (value) ?
                              GLADE_TAG_TRUE : GLADE_TAG_FALSE);
  else if (G_IS_PARAM_SPEC_VARIANT (property_def->pspec))
    {
      GVariant *variant;

      if ((variant = g_value_get_variant (value)))
        string = g_variant_print (variant, TRUE);
    }
  else if (G_IS_PARAM_SPEC_OBJECT (property_def->pspec))
    {
      object = g_value_get_object (value);
      string =
          glade_property_def_make_string_from_object (property_def, object);
    }
  else if (GLADE_IS_PARAM_SPEC_OBJECTS (property_def->pspec))
    {
      objects = g_value_get_boxed (value);
      string =
          glade_property_def_make_string_from_objects (property_def,
                                                         objects);
    }
  else
    g_critical ("Unsupported pspec type %s (value -> string)",
                g_type_name (G_PARAM_SPEC_TYPE (property_def->pspec)));

  return string;
}

/* This is copied exactly from libglade. I've just renamed the function.
 */
guint
glade_property_def_make_flags_from_string (GType type, const char *string)
{
  GFlagsClass *fclass;
  gchar *endptr, *prevptr;
  guint i, j, ret = 0;
  char *flagstr;

  ret = strtoul (string, &endptr, 0);
  if (endptr != string)         /* parsed a number */
    return ret;

  fclass = g_type_class_ref (type);


  flagstr = g_strdup (string);
  for (ret = i = j = 0;; i++)
    {
      gboolean eos;

      eos = flagstr[i] == '\0';

      if (eos || flagstr[i] == '|')
        {
          GFlagsValue *fv;
          const char *flag;
          gunichar ch;

          flag = &flagstr[j];
          endptr = &flagstr[i];

          if (!eos)
            {
              flagstr[i++] = '\0';
              j = i;
            }

          /* trim spaces */
          for (;;)
            {
              ch = g_utf8_get_char (flag);
              if (!g_unichar_isspace (ch))
                break;
              flag = g_utf8_next_char (flag);
            }

          while (endptr > flag)
            {
              prevptr = g_utf8_prev_char (endptr);
              ch = g_utf8_get_char (prevptr);
              if (!g_unichar_isspace (ch))
                break;
              endptr = prevptr;
            }

          if (endptr > flag)
            {
              *endptr = '\0';
              fv = g_flags_get_value_by_name (fclass, flag);

              if (!fv)
                fv = g_flags_get_value_by_nick (fclass, flag);

              if (fv)
                ret |= fv->value;
              else
                g_warning ("Unknown flag: '%s'", flag);
            }

          if (eos)
            break;
        }
    }

  g_free (flagstr);

  g_type_class_unref (fclass);

  return ret;
}

/* This is copied exactly from libglade. I've just renamed the function.
 */
static gint
glade_property_def_make_enum_from_string (GType type, const char *string)
{
  GEnumClass *eclass;
  GEnumValue *ev;
  gchar *endptr;
  gint ret = 0;

  ret = strtoul (string, &endptr, 0);
  if (endptr != string)         /* parsed a number */
    return ret;

  eclass = g_type_class_ref (type);
  ev = g_enum_get_value_by_name (eclass, string);
  if (!ev)
    ev = g_enum_get_value_by_nick (eclass, string);
  if (ev)
    ret = ev->value;

  g_type_class_unref (eclass);

  return ret;
}

static GObject *
glade_property_def_make_object_from_string (GladePropertyDef *
                                              property_def,
                                              const gchar * string,
                                              GladeProject * project)
{
  GObject *object = NULL;
  gchar *fullpath;

  if (string == NULL || project == NULL)
    return NULL;

  if (property_def->pspec->value_type == GDK_TYPE_PIXBUF)
    {
      GdkPixbuf *pixbuf;

      if (*string == '\0')
        return NULL;

      fullpath = glade_project_resource_fullpath (project, string);

      if ((pixbuf = gdk_pixbuf_new_from_file (fullpath, NULL)) == NULL)
        {
          GdkPixbuf *icon = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                                      "image-missing", 22, 0, NULL);
          /* Use a copy, since gtk_icon_theme_load_icon() returns the same pixbuf */
          pixbuf = gdk_pixbuf_copy (icon);
          g_object_unref (icon);
        }

      if (pixbuf)
        {
          object = G_OBJECT (pixbuf);
          g_object_set_data_full (object, "GladeFileName",
                                  g_strdup (string), g_free);
        }

      g_free (fullpath);
    }
  else if (property_def->pspec->value_type == G_TYPE_FILE)
    {
      GFile *file;

      if (*string == '\0')
        return NULL;

      file = g_file_new_for_uri (string);

      object = G_OBJECT (file);
      g_object_set_data_full (object, "GladeFileURI",
                              g_strdup (string), g_free);
    }
  else
    {
      GladeWidget *gwidget;
      if ((gwidget = glade_project_get_widget_by_name (project, string)) != NULL)
        object = glade_widget_get_object (gwidget);
    }

  return object;
}

static GList *
glade_property_def_make_objects_from_string (GladePropertyDef *
                                               property_def,
                                               const gchar * string,
                                               GladeProject * project)
{
  GList *objects = NULL;
  GObject *object;
  gchar **split;
  guint i;

  if ((split = g_strsplit (string, GLADE_PROPERTY_DEF_OBJECT_DELIMITER, 0)) != NULL)
    {
      for (i = 0; split[i]; i++)
        {
          if ((object = 
               glade_property_def_make_object_from_string (property_def, 
                                                             split[i], 
                                                             project)) != NULL)
            objects = g_list_prepend (objects, object);
        }
      g_strfreev (split);
    }
  return g_list_reverse (objects);
}

/**
 * glade_property_def_make_gvalue_from_string:
 * @property_def: A #GladePropertyDef
 * @string: a string representation of this property
 * @project: the #GladeProject that the property should be resolved for
 *
 * Returns: A #GValue created based on the @property_def
 *          and @string criteria.
 */
GValue *
glade_property_def_make_gvalue_from_string (GladePropertyDef *property_def,
                                              const gchar        *string,
                                              GladeProject       *project)
{
  GValue *value = g_new0 (GValue, 1);
  gchar **strv;
  GdkColor color = { 0, };
  GdkRGBA rgba = { 0, };

  g_value_init (value, property_def->pspec->value_type);

  if (G_IS_PARAM_SPEC_ENUM (property_def->pspec))
    {
      gint eval = glade_property_def_make_enum_from_string
          (property_def->pspec->value_type, string);
      g_value_set_enum (value, eval);
    }
  else if (G_IS_PARAM_SPEC_FLAGS (property_def->pspec))
    {
      guint flags = glade_property_def_make_flags_from_string
          (property_def->pspec->value_type, string);
      g_value_set_flags (value, flags);
    }
  else if (G_IS_PARAM_SPEC_VALUE_ARRAY (property_def->pspec))
    {
      GValueArray *value_array;
      GValue str_value = { 0, };
      gint i;

      /* Require deprecated code */
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
      value_array = g_value_array_new (0);
      G_GNUC_END_IGNORE_DEPRECATIONS;

      g_value_init (&str_value, G_TYPE_STRING);
      strv = g_strsplit (string, "\n", 0);

      for (i = 0; strv[i]; i++)
        {
          g_value_set_static_string (&str_value, strv[i]);

          G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
          value_array = g_value_array_append (value_array, &str_value);
          G_GNUC_END_IGNORE_DEPRECATIONS;
        }
      g_value_take_boxed (value, value_array);
      g_strfreev (strv);
    }
  else if (G_IS_PARAM_SPEC_BOXED (property_def->pspec))
    {
      if (property_def->pspec->value_type == GDK_TYPE_COLOR)
        {
          if (gdk_color_parse (string, &color))
            g_value_set_boxed (value, &color);
          else
            g_warning ("could not parse colour name `%s'", string);
        }
      else if (property_def->pspec->value_type == GDK_TYPE_RGBA)
        {
          if (gdk_rgba_parse (&rgba, string))
            g_value_set_boxed (value, &rgba);
          else
            g_warning ("could not parse rgba colour name `%s'", string);
        }
      else if (property_def->pspec->value_type == G_TYPE_STRV)
        {
          strv = g_strsplit (string, "\n", 0);
          g_value_take_boxed (value, strv);
        }
    }
  else if (G_IS_PARAM_SPEC_VARIANT (property_def->pspec))
    g_value_take_variant (value, g_variant_parse (NULL, string, NULL, NULL, NULL));
  else if (G_IS_PARAM_SPEC_INT (property_def->pspec))
    g_value_set_int (value, g_ascii_strtoll (string, NULL, 10));
  else if (G_IS_PARAM_SPEC_UINT (property_def->pspec))
    g_value_set_uint (value, g_ascii_strtoull (string, NULL, 10));
  else if (G_IS_PARAM_SPEC_LONG (property_def->pspec))
    g_value_set_long (value, g_ascii_strtoll (string, NULL, 10));
  else if (G_IS_PARAM_SPEC_ULONG (property_def->pspec))
    g_value_set_ulong (value, g_ascii_strtoull (string, NULL, 10));
  else if (G_IS_PARAM_SPEC_INT64 (property_def->pspec))
    g_value_set_int64 (value, g_ascii_strtoll (string, NULL, 10));
  else if (G_IS_PARAM_SPEC_UINT64 (property_def->pspec))
    g_value_set_uint64 (value, g_ascii_strtoull (string, NULL, 10));
  else if (G_IS_PARAM_SPEC_FLOAT (property_def->pspec))
    g_value_set_float (value, (float) g_ascii_strtod (string, NULL));
  else if (G_IS_PARAM_SPEC_DOUBLE (property_def->pspec))
    g_value_set_double (value, g_ascii_strtod (string, NULL));
  else if (G_IS_PARAM_SPEC_STRING (property_def->pspec))
    g_value_set_string (value, string);
  else if (G_IS_PARAM_SPEC_CHAR (property_def->pspec))
    g_value_set_schar (value, string[0]);
  else if (G_IS_PARAM_SPEC_UCHAR (property_def->pspec))
    g_value_set_uchar (value, string[0]);
  else if (G_IS_PARAM_SPEC_UNICHAR (property_def->pspec))
    g_value_set_uint (value, g_utf8_get_char (string));
  else if (G_IS_PARAM_SPEC_BOOLEAN (property_def->pspec))
    {
      gboolean val;
      if (glade_utils_boolean_from_string (string, &val))
        g_value_set_boolean (value, FALSE);
      else
        g_value_set_boolean (value, val);

    }
  else if (G_IS_PARAM_SPEC_OBJECT (property_def->pspec))
    {
      GObject *object = 
        glade_property_def_make_object_from_string (property_def, string, project);
      g_value_set_object (value, object);
    }
  else if (GLADE_IS_PARAM_SPEC_OBJECTS (property_def->pspec))
    {
      GList *objects = 
        glade_property_def_make_objects_from_string (property_def, string, project);
      g_value_take_boxed (value, objects);
    }
  else
    g_critical ("Unsupported pspec type %s (string -> value)",
                g_type_name (G_PARAM_SPEC_TYPE (property_def->pspec)));

  return value;
}

/**
 * glade_property_def_make_gvalue_from_vl:
 * @property_def: A #GladePropertyDef
 * @vl: a #va_list holding one argument of the correct type
 *      specified by @property_def
 *
 * Returns: A #GValue created based on the @property_def
 *          and a @vl arg of the correct type.
 */
GValue *
glade_property_def_make_gvalue_from_vl (GladePropertyDef *property_def,
                                        va_list           vl)
{
  GValue *value;

  g_return_val_if_fail (property_def != NULL, NULL);

  value = g_new0 (GValue, 1);
  g_value_init (value, property_def->pspec->value_type);

  if (G_IS_PARAM_SPEC_ENUM (property_def->pspec))
    g_value_set_enum (value, va_arg (vl, gint));
  else if (G_IS_PARAM_SPEC_FLAGS (property_def->pspec))
    g_value_set_flags (value, va_arg (vl, gint));
  else if (G_IS_PARAM_SPEC_INT (property_def->pspec))
    g_value_set_int (value, va_arg (vl, gint));
  else if (G_IS_PARAM_SPEC_UINT (property_def->pspec))
    g_value_set_uint (value, va_arg (vl, guint));
  else if (G_IS_PARAM_SPEC_LONG (property_def->pspec))
    g_value_set_long (value, va_arg (vl, glong));
  else if (G_IS_PARAM_SPEC_ULONG (property_def->pspec))
    g_value_set_ulong (value, va_arg (vl, gulong));
  else if (G_IS_PARAM_SPEC_INT64 (property_def->pspec))
    g_value_set_int64 (value, va_arg (vl, gint64));
  else if (G_IS_PARAM_SPEC_UINT64 (property_def->pspec))
    g_value_set_uint64 (value, va_arg (vl, guint64));
  else if (G_IS_PARAM_SPEC_FLOAT (property_def->pspec))
    g_value_set_float (value, (gfloat) va_arg (vl, gdouble));
  else if (G_IS_PARAM_SPEC_DOUBLE (property_def->pspec))
    g_value_set_double (value, va_arg (vl, gdouble));
  else if (G_IS_PARAM_SPEC_STRING (property_def->pspec))
    g_value_set_string (value, va_arg (vl, gchar *));
  else if (G_IS_PARAM_SPEC_CHAR (property_def->pspec))
    g_value_set_schar (value, (gchar) va_arg (vl, gint));
  else if (G_IS_PARAM_SPEC_UCHAR (property_def->pspec))
    g_value_set_uchar (value, (guchar) va_arg (vl, guint));
  else if (G_IS_PARAM_SPEC_UNICHAR (property_def->pspec))
    g_value_set_uint (value, va_arg (vl, gunichar));
  else if (G_IS_PARAM_SPEC_BOOLEAN (property_def->pspec))
    g_value_set_boolean (value, va_arg (vl, gboolean));
  else if (G_IS_PARAM_SPEC_OBJECT (property_def->pspec))
    g_value_set_object (value, va_arg (vl, gpointer));
  else if (G_VALUE_HOLDS_BOXED (value))
    g_value_set_boxed (value, va_arg (vl, gpointer));
  else
    g_critical ("Unsupported pspec type %s (vl -> string)",
                g_type_name (G_PARAM_SPEC_TYPE (property_def->pspec)));

  return value;
}

/**
 * glade_property_def_make_gvalue:
 * @property_def: A #GladePropertyDef
 * @...: an argument of the correct type specified by @property_def
 *
 * Returns: A #GValue created based on the @property_def
 *          and the provided argument.
 */
GValue *
glade_property_def_make_gvalue (GladePropertyDef * property_def, ...)
{
  GValue *value;
  va_list vl;

  g_return_val_if_fail (property_def != NULL, NULL);

  va_start (vl, property_def);
  value = glade_property_def_make_gvalue_from_vl (property_def, vl);
  va_end (vl);

  return value;
}


/**
 * glade_property_def_set_vl_from_gvalue:
 * @property_def: A #GladePropertyDef
 * @value: A #GValue to set
 * @vl: a #va_list holding one argument of the correct type
 *      specified by @property_def
 * 
 *
 * Sets @vl from @value based on @property_def criteria.
 */
void
glade_property_def_set_vl_from_gvalue (GladePropertyDef * property_def,
                                         GValue * value, va_list vl)
{
  g_return_if_fail (property_def != NULL);
  g_return_if_fail (value != NULL);

  /* The argument is a pointer of the specified type, cast the pointer and assign
   * the value using the proper g_value_get_ variation.
   */
  if (G_IS_PARAM_SPEC_ENUM (property_def->pspec))
    *(gint *) (va_arg (vl, gint *)) = g_value_get_enum (value);
  else if (G_IS_PARAM_SPEC_FLAGS (property_def->pspec))
    *(gint *) (va_arg (vl, gint *)) = g_value_get_flags (value);
  else if (G_IS_PARAM_SPEC_INT (property_def->pspec))
    *(gint *) (va_arg (vl, gint *)) = g_value_get_int (value);
  else if (G_IS_PARAM_SPEC_UINT (property_def->pspec))
    *(guint *) (va_arg (vl, guint *)) = g_value_get_uint (value);
  else if (G_IS_PARAM_SPEC_LONG (property_def->pspec))
    *(glong *) (va_arg (vl, glong *)) = g_value_get_long (value);
  else if (G_IS_PARAM_SPEC_ULONG (property_def->pspec))
    *(gulong *) (va_arg (vl, gulong *)) = g_value_get_ulong (value);
  else if (G_IS_PARAM_SPEC_INT64 (property_def->pspec))
    *(gint64 *) (va_arg (vl, gint64 *)) = g_value_get_int64 (value);
  else if (G_IS_PARAM_SPEC_UINT64 (property_def->pspec))
    *(guint64 *) (va_arg (vl, guint64 *)) = g_value_get_uint64 (value);
  else if (G_IS_PARAM_SPEC_FLOAT (property_def->pspec))
    *(gfloat *) (va_arg (vl, gdouble *)) = g_value_get_float (value);
  else if (G_IS_PARAM_SPEC_DOUBLE (property_def->pspec))
    *(gdouble *) (va_arg (vl, gdouble *)) = g_value_get_double (value);
  else if (G_IS_PARAM_SPEC_STRING (property_def->pspec))
    *(gchar **) (va_arg (vl, gchar *)) = (gchar *) g_value_get_string (value);
  else if (G_IS_PARAM_SPEC_CHAR (property_def->pspec))
    *(gchar *) (va_arg (vl, gint *)) = g_value_get_schar (value);
  else if (G_IS_PARAM_SPEC_UCHAR (property_def->pspec))
    *(guchar *) (va_arg (vl, guint *)) = g_value_get_uchar (value);
  else if (G_IS_PARAM_SPEC_UNICHAR (property_def->pspec))
    *(guint *) (va_arg (vl, gunichar *)) = g_value_get_uint (value);
  else if (G_IS_PARAM_SPEC_BOOLEAN (property_def->pspec))
    *(gboolean *) (va_arg (vl, gboolean *)) = g_value_get_boolean (value);
  else if (G_IS_PARAM_SPEC_OBJECT (property_def->pspec))
    *(gpointer *) (va_arg (vl, gpointer *)) = g_value_get_object (value);
  else if (G_VALUE_HOLDS_BOXED (value))
    *(gpointer *) (va_arg (vl, gpointer *)) = g_value_get_boxed (value);
  else
    g_critical ("Unsupported pspec type %s (string -> vl)",
                g_type_name (G_PARAM_SPEC_TYPE (property_def->pspec)));
}

/**
 * glade_property_def_get_from_gvalue:
 * @property_def: A #GladePropertyDef
 * @value: A #GValue to set
 * @...: a return location of the correct type
 * 
 *
 * Assignes the provided return location to @value
 */
void
glade_property_def_get_from_gvalue (GladePropertyDef *property_def,
                                      GValue             *value,
                                      ...)
{
  va_list vl;

  g_return_if_fail (property_def != NULL);

  va_start (vl, value);
  glade_property_def_set_vl_from_gvalue (property_def, value, vl);
  va_end (vl);
}


/* "need_adaptor": An evil trick to let us create pclasses without
 * adaptors and editors.
 */
GladePropertyDef *
glade_property_def_new_from_spec_full (GladeWidgetAdaptor *adaptor,
                                         GParamSpec         *spec,
                                         gboolean            need_adaptor)
{
  GObjectClass *gtk_widget_class;
  GladePropertyDef *property_def;
  GladeEditorProperty *eprop = NULL;

  g_return_val_if_fail (spec != NULL, NULL);
  gtk_widget_class = g_type_class_ref (GTK_TYPE_WIDGET);

  /* Only properties that are _new_from_spec() are 
   * not virtual properties
   */
  property_def = glade_property_def_new (adaptor, spec->name);
  property_def->virt = FALSE;
  property_def->pspec = spec;

  /* We only use the writable properties */
  if ((spec->flags & G_PARAM_WRITABLE) == 0)
    goto failed;

  property_def->name = g_strdup (g_param_spec_get_nick (spec));

  /* Register only editable properties.
   */
  if (need_adaptor && !(eprop = glade_widget_adaptor_create_eprop
                       (GLADE_WIDGET_ADAPTOR (adaptor), property_def, FALSE)))
    goto failed;

  /* Just created it to see if it was supported.... destroy now... */
  if (eprop)
    gtk_widget_destroy (GTK_WIDGET (eprop));

  /* If its on the GtkWidgetClass, it goes in "common" 
   * (unless stipulated otherwise in the xml file)
   */
  if (g_object_class_find_property (gtk_widget_class,
                                    g_param_spec_get_name (spec)) != NULL)
    property_def->common = TRUE;

  /* Flag the construct only properties */
  if (spec->flags & G_PARAM_CONSTRUCT_ONLY)
    property_def->construct_only = TRUE;

  if (!property_def->id || !property_def->name)
    {
      g_critical ("No name or id for "
                  "glade_property_def_new_from_spec, failed.");
      goto failed;
    }

  property_def->tooltip = g_strdup (g_param_spec_get_blurb (spec));
  property_def->orig_def = glade_property_def_get_default_from_spec (spec);
  property_def->def = glade_property_def_get_default_from_spec (spec);

  g_type_class_unref (gtk_widget_class);
  return property_def;

failed:
  glade_property_def_free (property_def);
  g_type_class_unref (gtk_widget_class);
  return NULL;
}

/**
 * glade_property_def_new_from_spec:
 * @adaptor: A generic pointer (i.e. a #GladeWidgetAdaptor)
 * @spec: A #GParamSpec
 *
 * Returns: a newly created #GladePropertyDef based on @spec
 *          or %NULL if its unsupported.
 */
GladePropertyDef *
glade_property_def_new_from_spec (GladeWidgetAdaptor *adaptor, GParamSpec *spec)
{
  return glade_property_def_new_from_spec_full (adaptor, spec, TRUE);
}

/**
 * glade_property_def_is_visible:
 * @property_def: A #GladePropertyDef
 *
 *
 * Returns: whether or not to show this property in the editor
 */
gboolean
glade_property_def_is_visible (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->visible;
}

/**
 * glade_property_def_set_adaptor:
 * @property_def: A #GladePropertyDef
 * @adaptor: (transfer full): A #GladeWidgetAdaptor
 */
void
glade_property_def_set_adaptor (GladePropertyDef *property_def,
                                  GladeWidgetAdaptor *adaptor)
{
  g_return_if_fail (GLADE_IS_PROPERTY_DEF (property_def));

  property_def->adaptor = adaptor;
}

/**
 * glade_property_def_get_adaptor:
 * @property_def: A #GladePropertyDef
 *
 * Returns: (transfer none): The #GladeWidgetAdaptor associated with the @property_def
 */
GladeWidgetAdaptor *
glade_property_def_get_adaptor (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), NULL);

  return property_def->adaptor;
}

/**
 * glade_property_def_get_pspec:
 * @property_def: A #GladePropertyDef
 *
 * Returns: (transfer none): The #GParamSpec associated with the @property_def
 */
GParamSpec *
glade_property_def_get_pspec (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), NULL);

  return property_def->pspec;
}

/**
 * glade_property_def_set_pspec:
 * @property_def: A #GladePropertyDef
 * @pspec: (transfer full): A #GParamSpec
 */
void
glade_property_def_set_pspec (GladePropertyDef *property_def,
                                GParamSpec         *pspec)
{
  g_return_if_fail (GLADE_IS_PROPERTY_DEF (property_def));

  property_def->pspec = pspec;
}

void
glade_property_def_set_is_packing (GladePropertyDef *property_def,
                                     gboolean            is_packing)
{
  g_return_if_fail (GLADE_IS_PROPERTY_DEF (property_def));

  property_def->packing = is_packing;
}

gboolean
glade_property_def_get_is_packing (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->packing;
}

gboolean
glade_property_def_save (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->save;
}

gboolean
glade_property_def_save_always (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->save_always;
}

void
glade_property_def_set_virtual (GladePropertyDef *property_def,
                                  gboolean            value)
{
  g_return_if_fail (GLADE_IS_PROPERTY_DEF (property_def));

  property_def->virt = value;
}

gboolean
glade_property_def_get_virtual (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->virt;
}

void
glade_property_def_set_ignore (GladePropertyDef *property_def,
                                 gboolean            ignore)
{
  g_return_if_fail (GLADE_IS_PROPERTY_DEF (property_def));

  property_def->ignore = ignore;
}

gboolean
glade_property_def_get_ignore (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->ignore;
}

/**
 * glade_property_def_is_object:
 * @property_def: A #GladePropertyDef
 *
 * Returns: whether or not this is an object property 
 * that refers to another object in this project.
 */
gboolean
glade_property_def_is_object (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return (GLADE_IS_PARAM_SPEC_OBJECTS (property_def->pspec) ||
          (G_IS_PARAM_SPEC_OBJECT (property_def->pspec) &&
           property_def->pspec->value_type != GDK_TYPE_PIXBUF &&
           property_def->pspec->value_type != G_TYPE_FILE));
}

void
glade_property_def_set_name (GladePropertyDef  *property_def,
                               const gchar         *name)
{
  g_return_if_fail (GLADE_IS_PROPERTY_DEF (property_def));

  g_free (property_def->name);
  property_def->name = g_strdup (name);
}

const gchar *
glade_property_def_get_name (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), NULL);

  return property_def->name;
}

void
glade_property_def_set_tooltip (GladePropertyDef *property_def,
                                  const gchar        *tooltip)
{
  g_return_if_fail (GLADE_IS_PROPERTY_DEF (property_def));

  g_free (property_def->tooltip);
  property_def->tooltip = g_strdup (tooltip);
}

const gchar *
glade_property_def_get_tooltip (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), NULL);

  return property_def->tooltip;
}

void
glade_property_def_set_construct_only (GladePropertyDef *property_def,
                                         gboolean            construct_only)
{
  g_return_if_fail (GLADE_IS_PROPERTY_DEF (property_def));

  property_def->construct_only = construct_only;
}

gboolean
glade_property_def_get_construct_only (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->construct_only;
}

const GValue *
glade_property_def_get_default (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), NULL);

  return property_def->def;
}

const GValue *
glade_property_def_get_original_default (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), NULL);

  return property_def->orig_def;
}

gboolean
glade_property_def_translatable (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->translatable;
}

gboolean
glade_property_def_needs_sync (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->needs_sync;
}

gboolean
glade_property_def_query (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->query;
}

gboolean
glade_property_def_atk (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->atk;
}

gboolean
glade_property_def_common (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->common;
}

gboolean
glade_property_def_parentless_widget (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->parentless_widget;
}

gboolean
glade_property_def_optional (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->optional;
}

gboolean
glade_property_def_optional_default (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->optional_default;
}

gboolean
glade_property_def_multiline (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->multiline;
}

gboolean
glade_property_def_stock (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->stock;
}

gboolean
glade_property_def_stock_icon (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->stock_icon;
}

gboolean
glade_property_def_transfer_on_paste (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->transfer_on_paste;
}

gboolean
glade_property_def_custom_layout (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->custom_layout;
}

gdouble
glade_property_def_weight (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), -1.0);

  return property_def->weight;
}

const gchar *
glade_property_def_create_type (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), NULL);

  return property_def->create_type;
}

guint16
glade_property_def_since_major (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), 0);

  return property_def->version_since_major;
}

guint16
glade_property_def_since_minor (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), 0);

  return property_def->version_since_minor;
}

void
_glade_property_def_reset_version (GladePropertyDef *property_def)
{
  g_return_if_fail (GLADE_IS_PROPERTY_DEF (property_def));

  property_def->version_since_major = 0;
  property_def->version_since_minor = 0;
}

gboolean
glade_property_def_deprecated (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->deprecated;
}

const gchar *
glade_property_def_id (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), NULL);

  return property_def->id;
}

gboolean
glade_property_def_themed_icon (GladePropertyDef *property_def)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  return property_def->themed_icon;
}

/**
 * gpc_read_displayable_values_from_node:
 * @node: a GLADE_TAG_DISPLAYABLE_VALUES node
 * @values: an array of the values wich node overrides.
 * @n_values: the size of @values
 *
 * Reads and caches displayable values from the catalog
 */
static void
gpc_read_displayable_values_from_node (GladeXmlNode       *node,
                                       GladePropertyDef *property_def,
                                       const gchar        *domain)
{
  gpointer the_class = g_type_class_ref (property_def->pspec->value_type);
  GladeXmlNode *child;
  GEnumValue *enum_values = NULL;
  GFlagsValue *flags_values = NULL;
  gint n_values, registered_values = 0;

  if (G_IS_PARAM_SPEC_ENUM (property_def->pspec))
    {
      GEnumClass *eclass = the_class;
      enum_values = eclass->values;
      n_values = eclass->n_values;
    }
  else
    {
      GFlagsClass *fclass = the_class;
      flags_values = fclass->values;
      n_values = fclass->n_values;
    }

  if ((child = glade_xml_search_child (node, GLADE_TAG_VALUE)) == NULL)
    return;

  for (child = glade_xml_node_get_children (node); child; child = glade_xml_node_next (child))
    {
      gint i;
      gchar *id, *name;
      GEnumValue *enum_val;
      GFlagsValue *flags_val;
      gboolean disabled;

      id = glade_xml_get_property_string_required (child, GLADE_TAG_ID, NULL);
      if (!id) continue;
      
      disabled = glade_xml_get_property_boolean (child, GLADE_TAG_DISABLED, FALSE);

      if (!disabled)
        {
          name = glade_xml_get_property_string_required (child, GLADE_TAG_NAME, NULL);
          if (!name) continue;
        }
      else
        name = NULL;

      for (i = 0; i < n_values; i++)
        {
          /* is it a match ?? */
          if ((G_IS_PARAM_SPEC_ENUM (property_def->pspec) &&
               (strcmp (id, enum_values[i].value_name) == 0 ||
                strcmp (id, enum_values[i].value_nick) == 0)) ||
              (G_IS_PARAM_SPEC_FLAGS (property_def->pspec) &&
               (strcmp (id, flags_values[i].value_name) == 0 ||
                strcmp (id, flags_values[i].value_nick) == 0)))
            {
              registered_values++;

              if (G_IS_PARAM_SPEC_ENUM (property_def->pspec))
                {
                  enum_val = &enum_values[i];
                  glade_register_displayable_value (property_def->pspec->value_type,
                                                    enum_val->value_nick,
                                                    domain, name);
                  if (disabled)
                    glade_displayable_value_set_disabled (property_def->pspec->value_type,
                                                          enum_val->value_nick,
                                                          TRUE);
                }
              else
                {
                  flags_val = &flags_values[i];
                  glade_register_displayable_value (property_def->pspec->value_type,
                                                    flags_val->value_nick,
                                                    domain, name);
                  if (disabled)
                    glade_displayable_value_set_disabled (property_def->pspec->value_type,
                                                          flags_val->value_nick,
                                                          TRUE);
                }
              break;
            }
        }

      g_free (id);
      g_free (name);
    }

  if (n_values != registered_values)
    g_message ("%d missing displayable value for %s::%s",
               n_values - registered_values,
               glade_widget_adaptor_get_name (property_def->adaptor), property_def->id);

  g_type_class_unref (the_class);

}

/**
 * glade_property_def_make_adjustment:
 * @property_def: a pointer to the property class
 *
 * Creates and appropriate GtkAdjustment for use in the editor
 *
 * Returns: (transfer full): An appropriate #GtkAdjustment for use in the Property editor
 */
GtkAdjustment *
glade_property_def_make_adjustment (GladePropertyDef *property_def)
{
  GtkAdjustment *adjustment;
  gdouble min = 0, max = 0, def = 0;
  gboolean float_range = FALSE;

  g_return_val_if_fail (property_def != NULL, NULL);
  g_return_val_if_fail (property_def->pspec != NULL, NULL);

  if (G_IS_PARAM_SPEC_INT (property_def->pspec))
    {
      min = (gdouble) ((GParamSpecInt *) property_def->pspec)->minimum;
      max = (gdouble) ((GParamSpecInt *) property_def->pspec)->maximum;
      def = (gdouble) ((GParamSpecInt *) property_def->pspec)->default_value;
    }
  else if (G_IS_PARAM_SPEC_UINT (property_def->pspec))
    {
      min = (gdouble) ((GParamSpecUInt *) property_def->pspec)->minimum;
      max = (gdouble) ((GParamSpecUInt *) property_def->pspec)->maximum;
      def = (gdouble) ((GParamSpecUInt *) property_def->pspec)->default_value;
    }
  else if (G_IS_PARAM_SPEC_LONG (property_def->pspec))
    {
      min = (gdouble) ((GParamSpecLong *) property_def->pspec)->minimum;
      max = (gdouble) ((GParamSpecLong *) property_def->pspec)->maximum;
      def = (gdouble) ((GParamSpecLong *) property_def->pspec)->default_value;
    }
  else if (G_IS_PARAM_SPEC_ULONG (property_def->pspec))
    {
      min = (gdouble) ((GParamSpecULong *) property_def->pspec)->minimum;
      max = (gdouble) ((GParamSpecULong *) property_def->pspec)->maximum;
      def =
          (gdouble) ((GParamSpecULong *) property_def->pspec)->default_value;
    }
  else if (G_IS_PARAM_SPEC_INT64 (property_def->pspec))
    {
      min = (gdouble) ((GParamSpecInt64 *) property_def->pspec)->minimum;
      max = (gdouble) ((GParamSpecInt64 *) property_def->pspec)->maximum;
      def =
          (gdouble) ((GParamSpecInt64 *) property_def->pspec)->default_value;
    }
  else if (G_IS_PARAM_SPEC_UINT64 (property_def->pspec))
    {
      min = (gdouble) ((GParamSpecUInt64 *) property_def->pspec)->minimum;
      max = (gdouble) ((GParamSpecUInt64 *) property_def->pspec)->maximum;
      def =
          (gdouble) ((GParamSpecUInt64 *) property_def->pspec)->default_value;
    }
  else if (G_IS_PARAM_SPEC_FLOAT (property_def->pspec))
    {
      float_range = TRUE;
      min = ((GParamSpecFloat *) property_def->pspec)->minimum;
      max = ((GParamSpecFloat *) property_def->pspec)->maximum;
      def = ((GParamSpecFloat *) property_def->pspec)->default_value;
    }
  else if (G_IS_PARAM_SPEC_DOUBLE (property_def->pspec))
    {
      float_range = TRUE;
      min = (gdouble) ((GParamSpecDouble *) property_def->pspec)->minimum;
      max = (gdouble) ((GParamSpecDouble *) property_def->pspec)->maximum;
      def =
          (gdouble) ((GParamSpecDouble *) property_def->pspec)->default_value;
    }
  else
    {
      g_critical ("Can't make adjustment for pspec type %s",
                  g_type_name (G_PARAM_SPEC_TYPE (property_def->pspec)));
    }

  adjustment = (GtkAdjustment *) gtk_adjustment_new (def, min, max,
                                                     float_range ?
                                                     FLOATING_STEP_INCREMENT :
                                                     NUMERICAL_STEP_INCREMENT,
                                                     float_range ?
                                                     FLOATING_PAGE_INCREMENT :
                                                     NUMERICAL_PAGE_INCREMENT,
                                                     float_range ?
                                                     FLOATING_PAGE_SIZE :
                                                     NUMERICAL_PAGE_SIZE);
  return adjustment;
}


static GParamSpec *
glade_property_def_parse_specifications (GladePropertyDef *property_def,
                                         GladeXmlNode     *spec_node)
{
  gchar *string;
  GType spec_type = 0, value_type = 0;
  GParamSpec *pspec = NULL;

  if ((string = glade_xml_get_value_string_required
       (spec_node, GLADE_TAG_TYPE,
        "Need a type of GParamSpec to define")) != NULL)
    spec_type = glade_util_get_type_from_name (string, FALSE);

  g_free (string);

  g_return_val_if_fail (spec_type != 0, NULL);

  if (spec_type == G_TYPE_PARAM_ENUM ||
      spec_type == G_TYPE_PARAM_FLAGS ||
      spec_type == G_TYPE_PARAM_BOXED ||
      spec_type == G_TYPE_PARAM_OBJECT || spec_type == GLADE_TYPE_PARAM_OBJECTS)
    {
      if ((string = glade_xml_get_value_string_required
           (spec_node, GLADE_TAG_VALUE_TYPE,
            "Need a value type to define enums flags boxed and object specs"))
          != NULL)
        value_type = glade_util_get_type_from_name (string, FALSE);

      g_free (string);

      g_return_val_if_fail (value_type != 0, NULL);

      if (spec_type == G_TYPE_PARAM_ENUM)
        {
          GEnumClass *eclass = g_type_class_ref (value_type);
          pspec = g_param_spec_enum ("dummy", "dummy", "dummy",
                                     value_type, eclass->minimum,
                                     G_PARAM_READABLE | G_PARAM_WRITABLE);
          g_type_class_unref (eclass);
        }
      else if (spec_type == G_TYPE_PARAM_FLAGS)
        pspec = g_param_spec_flags ("dummy", "dummy", "dummy",
                                    value_type, 0,
                                    G_PARAM_READABLE | G_PARAM_WRITABLE);
      else if (spec_type == G_TYPE_PARAM_OBJECT)
        pspec = g_param_spec_object ("dummy", "dummy", "dummy",
                                     value_type,
                                     G_PARAM_READABLE | G_PARAM_WRITABLE);
      else if (spec_type == GLADE_TYPE_PARAM_OBJECTS)
        pspec = glade_param_spec_objects ("dummy", "dummy", "dummy",
                                          value_type,
                                          G_PARAM_READABLE | G_PARAM_WRITABLE);
      else                      /*  if (spec_type == G_TYPE_PARAM_BOXED) */
        pspec = g_param_spec_boxed ("dummy", "dummy", "dummy",
                                    value_type,
                                    G_PARAM_READABLE | G_PARAM_WRITABLE);
    }
  else if (spec_type == G_TYPE_PARAM_STRING)
    pspec = g_param_spec_string ("dummy", "dummy", "dummy",
                                 NULL, G_PARAM_READABLE | G_PARAM_WRITABLE);
  else if (spec_type == G_TYPE_PARAM_BOOLEAN)
    pspec = g_param_spec_boolean ("dummy", "dummy", "dummy",
                                  FALSE, G_PARAM_READABLE | G_PARAM_WRITABLE);
  else
    {
      gchar *minstr, *maxstr;

      minstr = glade_xml_get_value_string (spec_node, GLADE_TAG_MIN_VALUE);
      maxstr = glade_xml_get_value_string (spec_node, GLADE_TAG_MAX_VALUE);

      if (spec_type == G_TYPE_PARAM_CHAR)
        {
          gint8 min = minstr ? minstr[0] : G_MININT8;
          gint8 max = maxstr ? maxstr[0] : G_MAXINT8;

          pspec = g_param_spec_char ("dummy", "dummy", "dummy",
                                     min, max, CLAMP (0, min, max),
                                     G_PARAM_READABLE | G_PARAM_WRITABLE);
        }
      else if (spec_type == G_TYPE_PARAM_UCHAR)
        {
          guint8 min = minstr ? minstr[0] : 0;
          guint8 max = maxstr ? maxstr[0] : G_MAXUINT8;

          pspec = g_param_spec_uchar ("dummy", "dummy", "dummy",
                                      min, max, 0,
                                      G_PARAM_READABLE | G_PARAM_WRITABLE);
        }
      else if (spec_type == G_TYPE_PARAM_INT)
        {
          gint min = minstr ? g_ascii_strtoll (minstr, NULL, 10) : G_MININT;
          gint max = maxstr ? g_ascii_strtoll (maxstr, NULL, 10) : G_MAXINT;

          pspec = g_param_spec_int ("dummy", "dummy", "dummy",
                                    min, max, CLAMP (0, min, max),
                                    G_PARAM_READABLE | G_PARAM_WRITABLE);
        }
      else if (spec_type == G_TYPE_PARAM_UINT)
        {
          guint min = minstr ? g_ascii_strtoll (minstr, NULL, 10) : 0;
          guint max = maxstr ? g_ascii_strtoll (maxstr, NULL, 10) : G_MAXUINT;

          pspec = g_param_spec_uint ("dummy", "dummy", "dummy",
                                     min, max, CLAMP (0, min, max),
                                     G_PARAM_READABLE | G_PARAM_WRITABLE);
        }
      else if (spec_type == G_TYPE_PARAM_LONG)
        {
          glong min = minstr ? g_ascii_strtoll (minstr, NULL, 10) : G_MINLONG;
          glong max = maxstr ? g_ascii_strtoll (maxstr, NULL, 10) : G_MAXLONG;

          pspec = g_param_spec_long ("dummy", "dummy", "dummy",
                                     min, max, CLAMP (0, min, max),
                                     G_PARAM_READABLE | G_PARAM_WRITABLE);
        }
      else if (spec_type == G_TYPE_PARAM_ULONG)
        {
          gulong min = minstr ? g_ascii_strtoll (minstr, NULL, 10) : 0;
          gulong max = maxstr ? g_ascii_strtoll (maxstr, NULL, 10) : G_MAXULONG;

          pspec = g_param_spec_ulong ("dummy", "dummy", "dummy",
                                      min, max, CLAMP (0, min, max),
                                      G_PARAM_READABLE | G_PARAM_WRITABLE);
        }
      else if (spec_type == G_TYPE_PARAM_INT64)
        {
          gint64 min = minstr ? g_ascii_strtoll (minstr, NULL, 10) : G_MININT64;
          gint64 max = maxstr ? g_ascii_strtoll (maxstr, NULL, 10) : G_MAXINT64;

          pspec = g_param_spec_int64 ("dummy", "dummy", "dummy",
                                      min, max, CLAMP (0, min, max),
                                      G_PARAM_READABLE | G_PARAM_WRITABLE);
        }
      else if (spec_type == G_TYPE_PARAM_UINT64)
        {
          guint64 min = minstr ? g_ascii_strtoll (minstr, NULL, 10) : 0;
          guint64 max =
              maxstr ? g_ascii_strtoll (maxstr, NULL, 10) : G_MAXUINT64;

          pspec = g_param_spec_uint64 ("dummy", "dummy", "dummy",
                                       min, max, CLAMP (0, min, max),
                                       G_PARAM_READABLE | G_PARAM_WRITABLE);
        }
      else if (spec_type == G_TYPE_PARAM_FLOAT)
        {
          gfloat min =
              minstr ? (float) g_ascii_strtod (minstr, NULL) : G_MINFLOAT;
          gfloat max =
              maxstr ? (float) g_ascii_strtod (maxstr, NULL) : G_MAXFLOAT;

          pspec = g_param_spec_float ("dummy", "dummy", "dummy",
                                      min, max, CLAMP (0, min, max),
                                      G_PARAM_READABLE | G_PARAM_WRITABLE);
        }
      else if (spec_type == G_TYPE_PARAM_DOUBLE)
        {
          gdouble min = minstr ? g_ascii_strtod (minstr, NULL) : G_MINFLOAT;
          gdouble max = maxstr ? g_ascii_strtod (maxstr, NULL) : G_MAXFLOAT;

          pspec = g_param_spec_float ("dummy", "dummy", "dummy",
                                      min, max, CLAMP (0, min, max),
                                      G_PARAM_READABLE | G_PARAM_WRITABLE);
        }
      else
        g_critical ("Unsupported pspec type %s (value -> string)",
                    g_type_name (spec_type));

      g_free (minstr);
      g_free (maxstr);
    }
  return pspec;
}


/**
 * glade_property_def_update_from_node:
 * @node: the property node
 * @object_type: the #GType of the owning object
 * @property_def_ref: (inout) (nullable): a pointer to the property class
 * @domain: the domain to translate catalog strings from
 *
 * Updates the @property_def_ref with the contents of the node in the xml
 * file. Only the values found in the xml file are overridden.
 *
 * Returns: %TRUE on success. @property_def_ref is set to NULL if the property
 *          has Disabled="TRUE".
 */
gboolean
glade_property_def_update_from_node (GladeXmlNode      *node,
                                     GType              object_type,
                                     GladePropertyDef **property_def_ref,
                                     const gchar       *domain)
{
  GladePropertyDef *property_def;
  GParamSpec *pspec = NULL;
  gchar *buf, *translated;
  GladeXmlNode *child, *spec_node;

  g_return_val_if_fail (property_def_ref != NULL, FALSE);

  /* for code cleanliness... */
  property_def = *property_def_ref;

  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);
  g_return_val_if_fail (glade_xml_node_verify (node, GLADE_TAG_PROPERTY),
                        FALSE);

  /* check the id */
  buf = glade_xml_get_property_string_required (node, GLADE_TAG_ID, NULL);
  if (!buf)
    return FALSE;
  g_free (buf);

  if (glade_xml_get_property_boolean (node, GLADE_TAG_DISABLED, FALSE))
    {
      /* Its easier for us to keep disabled properties around and
       * only virtually disable them */
      property_def->query = FALSE;
      property_def->ignore = TRUE;
      property_def->save = FALSE;
      property_def->visible = FALSE;
    }

  if ((spec_node =
       glade_xml_search_child (node, GLADE_TAG_SPECIFICATIONS)) != NULL)
    pspec = glade_property_def_parse_specifications (property_def, spec_node);
  else if ((buf = glade_xml_get_value_string (node, GLADE_TAG_SPEC)) != NULL)
    {
      pspec = glade_utils_get_pspec_from_funcname (buf);
      g_free (buf);
    }

  /* ... get the tooltip from the pspec ... */
  if (pspec != NULL)
    {
      property_def->pspec = pspec;

      /* Make sure we can tell properties apart by there 
       * owning class.
       */
      property_def->pspec->owner_type = object_type;

      /* We overrode the pspec, now it *is* a virtual property. */
      property_def->virt = TRUE;

      if (strcmp (g_param_spec_get_blurb (property_def->pspec), "dummy") != 0)
        {
          g_free (property_def->tooltip);
          property_def->tooltip = g_strdup (g_param_spec_get_blurb (property_def->pspec));
        }

      if (property_def->name == NULL ||
          strcmp (g_param_spec_get_nick (property_def->pspec), "dummy") != 0)
        {
          g_free (property_def->name);
          property_def->name = g_strdup (g_param_spec_get_nick (property_def->pspec));
        }

      if (property_def->pspec->flags & G_PARAM_CONSTRUCT_ONLY)
        property_def->construct_only = TRUE;

      if (property_def->orig_def)
        {
          g_value_unset (property_def->orig_def);
          g_free (property_def->orig_def);
        }
      property_def->orig_def =
          glade_property_def_get_default_from_spec (property_def->pspec);

      if (property_def->def)
        {
          g_value_unset (property_def->def);
          g_free (property_def->def);
        }
      property_def->def = glade_property_def_get_default_from_spec (property_def->pspec);

    }
  else if (!property_def->pspec)
    {
      /* If catalog file didn't specify a pspec function
       * and this property isn't found by introspection
       * we simply delete it from the list always.
       */
      glade_property_def_free (property_def);
      *property_def_ref = NULL;
      return TRUE;
    }

  /* Get the default */
  if ((buf = glade_xml_get_property_string (node, GLADE_TAG_DEFAULT)) != NULL)
    {
      if (property_def->def)
        {
          g_value_unset (property_def->def);
          g_free (property_def->def);
        }
      property_def->def =
          glade_property_def_make_gvalue_from_string (property_def, buf, NULL);

      if (property_def->virt)
        {
          g_value_unset (property_def->orig_def);
          g_free (property_def->orig_def);
          property_def->orig_def =
            glade_property_def_make_gvalue_from_string (property_def, buf, NULL);
        }

      g_free (buf);
    }

  /* If needed, update the name... */
  if ((buf = glade_xml_get_property_string (node, GLADE_TAG_NAME)) != NULL)
    {
      g_free (property_def->name);

      translated = dgettext (domain, buf);
      if (buf != translated)
        {
          /* translated is owned by gettext */
          property_def->name = g_strdup (translated);
          g_free (buf);
        }
      else
        {
          property_def->name = buf;
        }
    }

  /* ...and the tooltip */
  if ((buf = glade_xml_get_value_string (node, GLADE_TAG_TOOLTIP)) != NULL)
    {
      g_free (property_def->tooltip);

      translated = dgettext (domain, buf);
      if (buf != translated)
        {
          /* translated is owned by gettext */
          property_def->tooltip = g_strdup (translated);
          g_free (buf);
        }
      else
        {
          property_def->tooltip = buf;
        }
    }

  property_def->multiline =
      glade_xml_get_property_boolean (node, GLADE_TAG_MULTILINE,
                                      property_def->multiline);
  property_def->construct_only =
      glade_xml_get_property_boolean (node, GLADE_TAG_CONSTRUCT_ONLY,
                                      property_def->construct_only);
  property_def->translatable =
      glade_xml_get_property_boolean (node, GLADE_TAG_TRANSLATABLE,
                                      property_def->translatable);
  property_def->common =
      glade_xml_get_property_boolean (node, GLADE_TAG_COMMON, property_def->common);
  property_def->optional =
      glade_xml_get_property_boolean (node, GLADE_TAG_OPTIONAL,
                                      property_def->optional);
  property_def->query =
      glade_xml_get_property_boolean (node, GLADE_TAG_QUERY, property_def->query);
  property_def->save =
      glade_xml_get_property_boolean (node, GLADE_TAG_SAVE, property_def->save);
  property_def->visible =
      glade_xml_get_property_boolean (node, GLADE_TAG_VISIBLE, property_def->visible);
  property_def->custom_layout =
      glade_xml_get_property_boolean (node, GLADE_TAG_CUSTOM_LAYOUT,
                                      property_def->custom_layout);
  property_def->ignore =
      glade_xml_get_property_boolean (node, GLADE_TAG_IGNORE, property_def->ignore);
  property_def->needs_sync =
      glade_xml_get_property_boolean (node, GLADE_TAG_NEEDS_SYNC,
                                      property_def->needs_sync);
  property_def->themed_icon =
      glade_xml_get_property_boolean (node, GLADE_TAG_THEMED_ICON,
                                      property_def->themed_icon);
  property_def->stock =
      glade_xml_get_property_boolean (node, GLADE_TAG_STOCK, property_def->stock);
  property_def->stock_icon =
      glade_xml_get_property_boolean (node, GLADE_TAG_STOCK_ICON,
                                      property_def->stock_icon);
  property_def->weight =
      glade_xml_get_property_double (node, GLADE_TAG_WEIGHT, property_def->weight);
  property_def->transfer_on_paste =
      glade_xml_get_property_boolean (node, GLADE_TAG_TRANSFER_ON_PASTE,
                                      property_def->transfer_on_paste);
  property_def->save_always =
      glade_xml_get_property_boolean (node, GLADE_TAG_SAVE_ALWAYS,
                                      property_def->save_always);
  property_def->parentless_widget =
      glade_xml_get_property_boolean (node, GLADE_TAG_PARENTLESS_WIDGET,
                                      property_def->parentless_widget);


  glade_xml_get_property_version (node, GLADE_TAG_VERSION_SINCE,
                                  &property_def->version_since_major, 
                                  &property_def->version_since_minor);

  property_def->deprecated =
    glade_xml_get_property_boolean (node,
                                    GLADE_TAG_DEPRECATED,
                                    property_def->deprecated);


  if ((buf = glade_xml_get_property_string
       (node, GLADE_TAG_CREATE_TYPE)) != NULL)
    {
      g_clear_pointer (&property_def->create_type, g_free);
      property_def->create_type = buf;
    }

  /* If this property's value is an enumeration or flag then we try to get the displayable values */
  if ((G_IS_PARAM_SPEC_ENUM (property_def->pspec) ||
       G_IS_PARAM_SPEC_FLAGS (property_def->pspec)) &&
      (child = glade_xml_search_child (node, GLADE_TAG_DISPLAYABLE_VALUES)))
    gpc_read_displayable_values_from_node (child, property_def, domain);

  /* Right now allowing the backend to specify that some properties
   * go in the atk tab, ideally this shouldnt be needed.
   */
  property_def->atk =
      glade_xml_get_property_boolean (node, GLADE_TAG_ATK_PROPERTY, property_def->atk);

  if (property_def->optional)
    property_def->optional_default =
        glade_xml_get_property_boolean (node, GLADE_TAG_OPTIONAL_DEFAULT,
                                        property_def->optional_default);

  /* notify that we changed the property class */
  property_def->is_modified = TRUE;

  return TRUE;
}



/**
 * glade_property_def_match:
 * @property_def: a #GladePropertyDef
 * @comp: a #GladePropertyDef
 *
 * Returns: whether @property_def and @comp are a match or not
 *          (properties in seperate decendant heirarchies that
 *           have the same name are not matches).
 */
gboolean
glade_property_def_match (GladePropertyDef *property_def,
                          GladePropertyDef *comp)
{
  g_return_val_if_fail (property_def != NULL, FALSE);
  g_return_val_if_fail (comp != NULL, FALSE);

  return (strcmp (property_def->id, comp->id) == 0 &&
          property_def->packing == comp->packing &&
          property_def->pspec->owner_type == comp->pspec->owner_type);
}


/**
 * glade_property_def_void_value:
 * @property_def: a #GladePropertyDef
 * @value: a GValue of correct type for @property_def
 *
 * Returns: Whether @value for this @property_def is voided; a voided value
 *          can be a %NULL value for boxed or object type param specs.
 */
gboolean
glade_property_def_void_value (GladePropertyDef *property_def, GValue *value)
{
  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), FALSE);

  if (G_IS_PARAM_SPEC_OBJECT (property_def->pspec) &&
      g_value_get_object (value) == NULL)
    return TRUE;
  else if (G_IS_PARAM_SPEC_BOXED (property_def->pspec) &&
           g_value_get_boxed (value) == NULL)
    return TRUE;

  return FALSE;
}

/**
 * glade_property_def_compare:
 * @property_def: a #GladePropertyDef
 * @value1: a GValue of correct type for @property_def
 * @value2: a GValue of correct type for @property_def
 *
 * Compares value1 with value2 according to @property_def.
 *
 * Returns: -1, 0 or +1, if value1 is found to be less than,
 * equal to or greater than value2, respectively.
 */
gint
glade_property_def_compare (GladePropertyDef *property_def,
                            const GValue     *value1,
                            const GValue     *value2)
{
  gint retval;

  g_return_val_if_fail (GLADE_IS_PROPERTY_DEF (property_def), -1);

  /* GLib does not know how to compare a boxed real value */
  if (G_VALUE_HOLDS_BOXED (value1) || G_VALUE_HOLDS_BOXED (value2))
    {
      gchar *val1, *val2;

      /* So boxed types are compared by string and the backend is required to generate
       * unique strings for values for this purpose.
       *
       * NOTE: We could add a pclass option to use the string compare vs. boxed compare...
       */
      val1 =
          glade_widget_adaptor_string_from_value (property_def->adaptor, property_def, value1);
      val2 =
          glade_widget_adaptor_string_from_value (property_def->adaptor, property_def, value2);

      if (val1 && val2)
        retval = strcmp (val1, val2);
      else
        retval = val1 - val2;

      g_free (val1);
      g_free (val2);
    }
  else
    {
      if (G_IS_PARAM_SPEC_STRING (property_def->pspec))
        {
          const gchar *value_str1, *value_str2;

          /* in string specs; NULL and '\0' are 
           * treated as equivalent.
           */
          value_str1 = g_value_get_string (value1);
          value_str2 = g_value_get_string (value2);

          if (value_str1 == NULL && value_str2 && value_str2[0] == '\0')
            return 0;
          else if (value_str2 == NULL && value_str1 && value_str1[0] == '\0')
            return 0;
        }
      retval = g_param_values_cmp (property_def->pspec, value1, value2);
    }

  return retval;
}

/**
 * glade_property_def_set_weights:
 * @properties: (element-type GladePropertyDef): a list of #GladePropertyDef
 * @parent: the #GType of the parent
 *
 * This function assignes "weight" to each property in its natural order staring from 1.
 * If parent is 0 weight will be set for every #GladePropertyDef in the list.
 * This function will not override weight if it is already set (weight >= 0.0)
 */
void
glade_property_def_set_weights (GList **properties, GType parent)
{
  gint normal = 0, common = 0, packing = 0;
  GList *l;

  for (l = *properties; l && l->data; l = g_list_next (l))
    {
      GladePropertyDef *property_def = l->data;

      if (property_def->visible &&
          (parent) ? parent == property_def->pspec->owner_type : TRUE && !property_def->atk)
        {
          /* Use a different counter for each tab (common, packing and normal) */
          if (property_def->common)
            common++;
          else if (property_def->packing)
            packing++;
          else
            normal++;

          /* Skip if it is already set */
          if (property_def->weight >= 0.0)
            continue;

          /* Special-casing weight of properties for seperate tabs */
          if (property_def->common)
            property_def->weight = common;
          else if (property_def->packing)
            property_def->weight = packing;
          else
            property_def->weight = normal;
        }
    }
}

void
glade_property_def_load_defaults_from_spec (GladePropertyDef *property_def)
{
  property_def->orig_def =
    glade_property_def_get_default_from_spec (property_def->pspec);
  
  property_def->def =
    glade_property_def_get_default_from_spec (property_def->pspec);
}

