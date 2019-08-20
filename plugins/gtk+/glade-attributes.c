/*
 * glade-attributes.c - Editing support for pango attributes
 *
 * Copyright (C) 2008 Tristan Van Berkom
 *
 * Author(s):
 *      Tristan Van Berkom <tvb@gnome.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gladeui/glade.h>
#include <glib/gi18n-lib.h>

#include "glade-attributes.h"

#define GLADE_RESPONSE_CLEAR 42

static GList *
glade_attr_list_copy (GList *attrs)
{
  GList *ret = NULL, *list;
  GladeAttribute *attr, *dup_attr;

  for (list = attrs; list; list = list->next)
    {
      attr = list->data;

      dup_attr = g_new0 (GladeAttribute, 1);
      dup_attr->type = attr->type;
      dup_attr->start = attr->start;
      dup_attr->end = attr->end;
      g_value_init (&(dup_attr->value), G_VALUE_TYPE (&(attr->value)));
      g_value_copy (&(attr->value), &(dup_attr->value));

      ret = g_list_prepend (ret, dup_attr);
    }

  return g_list_reverse (ret);
}

void
glade_attr_list_free (GList *attrs)
{
  GList *list;
  GladeAttribute *attr;

  for (list = attrs; list; list = list->next)
    {
      attr = list->data;

      g_value_unset (&(attr->value));
      g_free (attr);
    }
  g_list_free (attrs);
}

GType
glade_attr_glist_get_type (void)
{
  static GType type_id = 0;

  if (!type_id)
    type_id = g_boxed_type_register_static
        ("GladeAttrGList",
         (GBoxedCopyFunc) glade_attr_list_copy,
         (GBoxedFreeFunc) glade_attr_list_free);
  return type_id;
}

/**************************************************************
 *              GladeEditorProperty stuff here
 **************************************************************/
struct _GladeEPropAttrs
{
  GladeEditorProperty parent_instance;

  GtkTreeModel *model;

};

GLADE_MAKE_EPROP (GladeEPropAttrs, glade_eprop_attrs, GLADE, EPROP_ATTRS)

enum
{

  /* Main Data */
  COLUMN_NAME,             /* The title string for PangoAttrType */
  COLUMN_NAME_WEIGHT,      /* For bold names */
  COLUMN_TYPE,             /* The PangoAttrType */
  COLUMN_EDIT_TYPE,        /* The AttrEditType (below) */
  COLUMN_VALUE,            /* The value */
  COLUMN_START,            /* attribute start value */
  COLUMN_END,              /* attribute end value */

  /* Editor renderer related */
  COLUMN_TOGGLE_ACTIVE,    /* whether the toggle renderer is being used */
  COLUMN_TOGGLE_DOWN,      /* whether the toggle should be displayed in "downstate" */

  COLUMN_BUTTON_ACTIVE,    /* whether the GladeCellRendererButton is to be used (to launch dialogs) */
  COLUMN_TEXT,             /* text attribute value for all text derived renderers */
  COLUMN_TEXT_STYLE,       /* whether to make italic */
  COLUMN_TEXT_FG,          /* forground colour of the text */
  
  COLUMN_COMBO_ACTIVE,     /* whether the combobox renderer is being used */
  COLUMN_COMBO_MODEL,      /* the model for the dropdown list */
  
  COLUMN_SPIN_ACTIVE,      /* whether the spin renderer is being used */
  COLUMN_SPIN_DIGITS,      /* How many decimal points to show (used to edit float values) */
  COLUMN_SPIN_ADJUSTMENT,

  NUM_COLUMNS
};


typedef enum
{
  EDIT_TOGGLE = 0,
  EDIT_COMBO,
  EDIT_SPIN,
  EDIT_COLOR,
  EDIT_FONT,
  EDIT_INVALID
} AttrEditType;

#define ACTIVATE_COLUMN_FROM_TYPE(type)           \
  ((type) == EDIT_TOGGLE ? COLUMN_TOGGLE_ACTIVE : \
   (type) == EDIT_SPIN ? COLUMN_SPIN_ACTIVE :     \
   (type) == EDIT_COMBO ? COLUMN_COMBO_ACTIVE: COLUMN_BUTTON_ACTIVE)


static GtkListStore *
get_enum_model_for_combo (PangoAttrType type)
{
  static GtkListStore *style_store = NULL,
      *weight_store = NULL, *variant_store = NULL,
      *stretch_store = NULL, *gravity_store = NULL,
      *gravity_hint_store = NULL, *default_store = NULL;

  switch (type)
    {
      case PANGO_ATTR_STYLE:
        if (!style_store)
          style_store =
              glade_utils_liststore_from_enum_type (PANGO_TYPE_STYLE, TRUE);
        return style_store;

      case PANGO_ATTR_WEIGHT:
        if (!weight_store)
          weight_store =
              glade_utils_liststore_from_enum_type (PANGO_TYPE_WEIGHT, TRUE);
        return weight_store;

      case PANGO_ATTR_VARIANT:
        if (!variant_store)
          variant_store =
              glade_utils_liststore_from_enum_type (PANGO_TYPE_VARIANT, TRUE);
        return variant_store;

      case PANGO_ATTR_STRETCH:
        if (!stretch_store)
          stretch_store =
              glade_utils_liststore_from_enum_type (PANGO_TYPE_STRETCH, TRUE);
        return stretch_store;

      case PANGO_ATTR_GRAVITY:
        if (!gravity_store)
          gravity_store =
              glade_utils_liststore_from_enum_type (PANGO_TYPE_GRAVITY, TRUE);
        return gravity_store;

      case PANGO_ATTR_GRAVITY_HINT:
        if (!gravity_hint_store)
          gravity_hint_store =
              glade_utils_liststore_from_enum_type (PANGO_TYPE_GRAVITY_HINT,
                                                    TRUE);
        return gravity_hint_store;

      default:
        if (!default_store)
          default_store = gtk_list_store_new (1, G_TYPE_STRING);
        return default_store;
    }
}

static gboolean
append_empty_row (GtkListStore *store, PangoAttrType type)
{
  const gchar *name = NULL;
  guint spin_digits = 0;
  GtkAdjustment *adjustment = NULL;
  GtkListStore *model = get_enum_model_for_combo (type);
  GtkTreeIter iter;
  AttrEditType edit_type = EDIT_INVALID;

  switch (type)
    {
        /* PangoAttrLanguage */
      case PANGO_ATTR_LANGUAGE:

        break;
        /* PangoAttrInt */
      case PANGO_ATTR_STYLE:
        edit_type = EDIT_COMBO;
        name = C_ ("textattr", "Style");
        break;
      case PANGO_ATTR_WEIGHT:
        edit_type = EDIT_COMBO;
        name = C_ ("textattr", "Weight");
        break;
      case PANGO_ATTR_VARIANT:
        edit_type = EDIT_COMBO;
        name = C_ ("textattr", "Variant");
        break;
      case PANGO_ATTR_STRETCH:
        edit_type = EDIT_COMBO;
        name = C_ ("textattr", "Stretch");
        break;
      case PANGO_ATTR_UNDERLINE:
        edit_type = EDIT_TOGGLE;
        name = C_ ("textattr", "Underline");
        break;
      case PANGO_ATTR_STRIKETHROUGH:
        edit_type = EDIT_TOGGLE;
        name = C_ ("textattr", "Strikethrough");
        break;
      case PANGO_ATTR_GRAVITY:
        edit_type = EDIT_COMBO;
        name = C_ ("textattr", "Gravity");
        break;
      case PANGO_ATTR_GRAVITY_HINT:
        edit_type = EDIT_COMBO;
        name = C_ ("textattr", "Gravity Hint");
        break;

        /* PangoAttrString */
      case PANGO_ATTR_FAMILY:
        /* Use a simple editable text renderer ? */
        break;

        /* PangoAttrSize */
      case PANGO_ATTR_SIZE:
        edit_type = EDIT_SPIN;
        adjustment = gtk_adjustment_new (0, 0, PANGO_SCALE*PANGO_SCALE, PANGO_SCALE, 0, 0);
        name = C_ ("textattr", "Size");
        break;
      case PANGO_ATTR_ABSOLUTE_SIZE:
        edit_type = EDIT_SPIN;
        adjustment = gtk_adjustment_new (0, 0, PANGO_SCALE*PANGO_SCALE, PANGO_SCALE, 0, 0);
        name = C_ ("textattr", "Absolute Size");
        break;

        /* PangoAttrColor */
        /* Colours need editors... */
      case PANGO_ATTR_FOREGROUND:
        edit_type = EDIT_COLOR;
        name = C_ ("textattr", "Foreground Color");
        break;
      case PANGO_ATTR_BACKGROUND:
        edit_type = EDIT_COLOR;
        name = C_ ("textattr", "Background Color");
        break;
      case PANGO_ATTR_UNDERLINE_COLOR:
        edit_type = EDIT_COLOR;
        name = C_ ("textattr", "Underline Color");
        break;
      case PANGO_ATTR_STRIKETHROUGH_COLOR:
        edit_type = EDIT_COLOR;
        name = C_ ("textattr", "Strikethrough Color");
        break;

        /* PangoAttrShape */
      case PANGO_ATTR_SHAPE:
        /* Unsupported for now */
        break;
        /* PangoAttrFloat */
      case PANGO_ATTR_SCALE:
        edit_type = EDIT_SPIN;
        adjustment = gtk_adjustment_new (0, 0, 128, 1, 0, 0);
        name = C_ ("textattr", "Scale");
        spin_digits = 3;
        break;

      case PANGO_ATTR_FONT_DESC:
        edit_type = EDIT_FONT;
        name = C_ ("textattr", "Font Description");
        break;

      case PANGO_ATTR_INVALID:
      case PANGO_ATTR_LETTER_SPACING:
      case PANGO_ATTR_RISE:
      case PANGO_ATTR_FALLBACK:
      default:
        break;
    }

  if (name)
    {
      gtk_list_store_append (store, &iter);

      gtk_list_store_set (store, &iter,
                          COLUMN_TOGGLE_ACTIVE, FALSE,
                          COLUMN_SPIN_ACTIVE, FALSE,
                          COLUMN_COMBO_ACTIVE, FALSE,
                          COLUMN_BUTTON_ACTIVE, FALSE, -1);

      gtk_list_store_set (store, &iter,
                          COLUMN_NAME, name,
                          COLUMN_TYPE, type,
                          COLUMN_EDIT_TYPE, edit_type,
                          COLUMN_NAME_WEIGHT, PANGO_WEIGHT_NORMAL,
                          COLUMN_TEXT, _("<Enter Value>"),
                          COLUMN_TEXT_STYLE, PANGO_STYLE_ITALIC,
                          COLUMN_TEXT_FG, "Grey",
                          COLUMN_COMBO_MODEL, model,
                          COLUMN_SPIN_DIGITS, spin_digits,
                          COLUMN_SPIN_ADJUSTMENT, adjustment,
                          ACTIVATE_COLUMN_FROM_TYPE (edit_type), TRUE, -1);
      return TRUE;
    }
  return FALSE;
}

static gboolean
is_empty_row (GtkTreeModel *model, GtkTreeIter *iter)
{

  PangoAttrType attr_type;
  AttrEditType edit_type;
  gboolean bval;
  gchar *strval = NULL;
  gboolean empty_row = FALSE;

  /* First get the basic values */
  gtk_tree_model_get (model, iter,
                      COLUMN_TYPE, &attr_type,
                      COLUMN_EDIT_TYPE, &edit_type,
                      COLUMN_TOGGLE_DOWN, &bval, COLUMN_TEXT, &strval, -1);

  /* Ignore all other types */
  switch (edit_type)
    {
      case EDIT_TOGGLE:
        if (!bval)
          empty_row = TRUE;
        break;
      case EDIT_COMBO:
        if (!strval || !strcmp (strval, _("Unset")) ||
            !strcmp (strval, _("<Enter Value>")))
          empty_row = TRUE;
        break;
      case EDIT_SPIN:
        /* XXX Interesting... can we get the defaults ? what can we do to let the user
         * unset the value ?? 
         */
        if (!strval || !strcmp (strval, "0") ||
            !strcmp (strval, _("<Enter Value>")))
          empty_row = TRUE;
        break;
      case EDIT_COLOR:
      case EDIT_FONT:
        if (!strval || strval[0] == '\0' ||
            !strcmp (strval, _("<Enter Value>")))
          empty_row = TRUE;
        break;
      case EDIT_INVALID:
      default:
        break;
    }
  g_free (strval);

  return empty_row;
}

static GType
type_from_attr_type (PangoAttrType type)
{
  GType gtype = 0;

  switch (type)
    {
      case PANGO_ATTR_LANGUAGE:
      case PANGO_ATTR_FAMILY:
      case PANGO_ATTR_FONT_DESC:
        return G_TYPE_STRING;

      case PANGO_ATTR_STYLE:
        return PANGO_TYPE_STYLE;
      case PANGO_ATTR_WEIGHT:
        return PANGO_TYPE_WEIGHT;
      case PANGO_ATTR_VARIANT:
        return PANGO_TYPE_VARIANT;
      case PANGO_ATTR_STRETCH:
        return PANGO_TYPE_STRETCH;
      case PANGO_ATTR_GRAVITY:
        return PANGO_TYPE_GRAVITY;
      case PANGO_ATTR_GRAVITY_HINT:
        return PANGO_TYPE_GRAVITY_HINT;

      case PANGO_ATTR_UNDERLINE:
      case PANGO_ATTR_STRIKETHROUGH:
        return G_TYPE_BOOLEAN;

      case PANGO_ATTR_SIZE:
      case PANGO_ATTR_ABSOLUTE_SIZE:
        return G_TYPE_INT;

      case PANGO_ATTR_SCALE:
        return G_TYPE_DOUBLE;

        /* PangoAttrColor */
      case PANGO_ATTR_FOREGROUND:
      case PANGO_ATTR_BACKGROUND:
      case PANGO_ATTR_UNDERLINE_COLOR:
      case PANGO_ATTR_STRIKETHROUGH_COLOR:
        /* boxed colours */
        return PANGO_TYPE_COLOR;

        /* PangoAttrShape */
      case PANGO_ATTR_SHAPE:
        /* Unsupported for now */
        break;

      case PANGO_ATTR_INVALID:
      case PANGO_ATTR_LETTER_SPACING:
      case PANGO_ATTR_RISE:
      case PANGO_ATTR_FALLBACK:
      default:
        break;
    }

  return gtype;
}

gchar *
glade_gtk_string_from_attr (GladeAttribute *gattr)
{
  gchar *ret = NULL;
  gint ival;
  gdouble fval;
  PangoColor *color;

  switch (gattr->type)
    {
      case PANGO_ATTR_LANGUAGE:
      case PANGO_ATTR_FAMILY:
      case PANGO_ATTR_FONT_DESC:
        ret = g_value_dup_string (&(gattr->value));
        break;

      case PANGO_ATTR_STYLE:
      case PANGO_ATTR_WEIGHT:
      case PANGO_ATTR_VARIANT:
      case PANGO_ATTR_STRETCH:
      case PANGO_ATTR_GRAVITY:
      case PANGO_ATTR_GRAVITY_HINT:

        /* Enums ... */
        ival = g_value_get_enum (&(gattr->value));
        ret =
            glade_utils_enum_string_from_value (G_VALUE_TYPE (&(gattr->value)),
                                                ival);
        break;


      case PANGO_ATTR_UNDERLINE:
      case PANGO_ATTR_STRIKETHROUGH:
        /* Booleans */
        if (g_value_get_boolean (&(gattr->value)))
          ret = g_strdup_printf ("True");
        else
          ret = g_strdup_printf ("False");
        break;

        /* PangoAttrSize */
      case PANGO_ATTR_SIZE:
      case PANGO_ATTR_ABSOLUTE_SIZE:
        /* ints */
        ival = g_value_get_int (&(gattr->value));
        ret = g_strdup_printf ("%d", ival);
        break;

        /* PangoAttrFloat */
      case PANGO_ATTR_SCALE: {
        /* doubles */
        gchar buf[G_ASCII_DTOSTR_BUF_SIZE];

        fval = g_value_get_double (&(gattr->value));
        ret = g_strdup (g_ascii_dtostr (buf, sizeof (buf), fval));
        break;
      }
        /* PangoAttrColor */
      case PANGO_ATTR_FOREGROUND:
      case PANGO_ATTR_BACKGROUND:
      case PANGO_ATTR_UNDERLINE_COLOR:
      case PANGO_ATTR_STRIKETHROUGH_COLOR:
        /* boxed colours */
        color = g_value_get_boxed (&(gattr->value));
        ret = pango_color_to_string (color);
        break;

        /* PangoAttrShape */
      case PANGO_ATTR_SHAPE:
        /* Unsupported for now */
        break;

      case PANGO_ATTR_INVALID:
      case PANGO_ATTR_LETTER_SPACING:
      case PANGO_ATTR_RISE:
      case PANGO_ATTR_FALLBACK:
      default:
        break;
    }

  return ret;
}

GladeAttribute *
glade_gtk_attribute_from_string (PangoAttrType type, const gchar *strval)
{
  GladeAttribute *gattr;
  PangoColor color;

  gattr = g_new0 (GladeAttribute, 1);
  gattr->type = type;
  gattr->start = 0;
  gattr->end = G_MAXUINT;

  switch (type)
    {
      case PANGO_ATTR_LANGUAGE:
      case PANGO_ATTR_FAMILY:
      case PANGO_ATTR_FONT_DESC:
        g_value_init (&(gattr->value), G_TYPE_STRING);
        g_value_set_string (&(gattr->value), strval);
        break;

      case PANGO_ATTR_STYLE:
      case PANGO_ATTR_WEIGHT:
      case PANGO_ATTR_VARIANT:
      case PANGO_ATTR_STRETCH:
      case PANGO_ATTR_GRAVITY:
      case PANGO_ATTR_GRAVITY_HINT:

        /* Enums ... */
        g_value_init (&(gattr->value), type_from_attr_type (type));
        g_value_set_enum (&(gattr->value),
                          glade_utils_enum_value_from_string (type_from_attr_type (type), strval));
        break;

      case PANGO_ATTR_UNDERLINE:
      case PANGO_ATTR_STRIKETHROUGH:
        /* Booleans */
        g_value_init (&(gattr->value), G_TYPE_BOOLEAN);
        g_value_set_boolean (&(gattr->value), TRUE);
        break;

        /* PangoAttrSize */
      case PANGO_ATTR_SIZE:
      case PANGO_ATTR_ABSOLUTE_SIZE:
        /* ints */
        g_value_init (&(gattr->value), G_TYPE_INT);
        g_value_set_int (&(gattr->value), strtol (strval, NULL, 10));
        break;

        /* PangoAttrFloat */
      case PANGO_ATTR_SCALE:
        /* doubles */
        g_value_init (&(gattr->value), G_TYPE_DOUBLE);
        g_value_set_double (&(gattr->value), g_ascii_strtod (strval, NULL));
        break;

        /* PangoAttrColor */
      case PANGO_ATTR_FOREGROUND:
      case PANGO_ATTR_BACKGROUND:
      case PANGO_ATTR_UNDERLINE_COLOR:
      case PANGO_ATTR_STRIKETHROUGH_COLOR:
        /* boxed colours */
        if (pango_color_parse (&color, strval))
          {
            g_value_init (&(gattr->value), PANGO_TYPE_COLOR);
            g_value_set_boxed (&(gattr->value), &color);
          }
        else
          g_critical ("Unable to parse color attribute '%s'", strval);

        break;

        /* PangoAttrShape */
      case PANGO_ATTR_SHAPE:
        /* Unsupported for now */
        break;

      case PANGO_ATTR_INVALID:
      case PANGO_ATTR_LETTER_SPACING:
      case PANGO_ATTR_RISE:
      case PANGO_ATTR_FALLBACK:
      default:
        break;
    }

  return gattr;
}

static void
sync_object (GladeEPropAttrs *eprop_attrs, gboolean use_command)
{
  GList *attributes = NULL;
  GladeAttribute *gattr;
  GtkTreeIter iter;
  PangoAttrType type;
  AttrEditType edit_type;
  gchar *strval = NULL;
  gboolean valid;

  valid = gtk_tree_model_iter_children (eprop_attrs->model, &iter, NULL);

  while (valid)
    {
      if (!is_empty_row (eprop_attrs->model, &iter))
        {
          gtk_tree_model_get (eprop_attrs->model, &iter,
                              COLUMN_TYPE, &type,
                              COLUMN_EDIT_TYPE, &edit_type,
                              COLUMN_TEXT, &strval, -1);

          gattr =
              glade_gtk_attribute_from_string (type,
                                               (edit_type ==
                                                EDIT_TOGGLE) ? "" : strval);
          strval = (g_free (strval), NULL);

          attributes = g_list_prepend (attributes, gattr);

        }
      valid = gtk_tree_model_iter_next (eprop_attrs->model, &iter);
    }

  if (use_command)
    {
      GValue value = { 0, };

      g_value_init (&value, GLADE_TYPE_ATTR_GLIST);
      g_value_take_boxed (&value, g_list_reverse (attributes));
      glade_editor_property_commit (GLADE_EDITOR_PROPERTY (eprop_attrs),
                                    &value);
      g_value_unset (&value);
    }
  else
    {
      GladeProperty *property = 
        glade_editor_property_get_property (GLADE_EDITOR_PROPERTY (eprop_attrs));

      glade_property_set (property, g_list_reverse (attributes));
      glade_attr_list_free (attributes);
    }
}


static GtkTreeIter *
get_row_by_type (GtkTreeModel *model, PangoAttrType type)
{
  GtkTreeIter iter, *ret_iter = NULL;
  gboolean valid;
  PangoAttrType iter_type;

  valid = gtk_tree_model_iter_children (model, &iter, NULL);

  while (valid)
    {
      gtk_tree_model_get (model, &iter, COLUMN_TYPE, &iter_type, -1);

      if (iter_type == type)
        {
          ret_iter = gtk_tree_iter_copy (&iter);
          break;
        }
      valid = gtk_tree_model_iter_next (model, &iter);
    }
  return ret_iter;
}


static void
value_icon_activate (GtkCellRendererToggle *cell_renderer,
                     gchar *path,
                     GladeEPropAttrs *eprop_attrs)
{
  GtkWidget *dialog;
  GtkTreeIter iter;
  PangoAttrType type;
  AttrEditType edit_type;
  GdkRGBA color = {0,};
  gchar *text = NULL, *new_text;

  /* Find type etc */
  if (!gtk_tree_model_get_iter_from_string (eprop_attrs->model, &iter, path))
    return;

  gtk_tree_model_get (eprop_attrs->model, &iter,
                      COLUMN_TEXT, &text,
                      COLUMN_TYPE, &type, 
                      COLUMN_EDIT_TYPE, &edit_type, -1);

  /* Launch dialog etc. */
  switch (edit_type)
    {
      case EDIT_COLOR:
        dialog = gtk_color_chooser_dialog_new (_("Select a color"),
                                               GTK_WINDOW (glade_app_get_window ()));
        /* Get response etc... */
        if (text && gdk_rgba_parse (&color, text))
          gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (dialog), &color);

        if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
          {
            gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (dialog), &color);
            /* Use PangoColor string format */
            if (((guint8)(color.red * 0xFF)) * 0x101 == (guint16)(color.red * 0xFFFF) &&
                ((guint8)(color.green * 0xFF)) * 0x101 == (guint16)(color.green * 0xFFFF) &&
                ((guint8)(color.blue * 0xFF)) * 0x101 == (guint16)(color.blue * 0xFFFF))
              new_text = g_strdup_printf ("#%02X%02X%02X",
                                          (guint8)(color.red * 0xFF),
                                          (guint8)(color.green * 0xFF),
                                          (guint8)(color.blue * 0xFF));
            else
              new_text = g_strdup_printf ("#%04X%04X%04X",
                                          (guint16)(color.red * 0xFFFF),
                                          (guint16)(color.green * 0xFFFF),
                                          (guint16)(color.blue * 0xFFFF));

            gtk_list_store_set (GTK_LIST_STORE (eprop_attrs->model), &iter,
                                COLUMN_TEXT, new_text,
                                COLUMN_NAME_WEIGHT, PANGO_WEIGHT_BOLD,
                                COLUMN_TEXT_STYLE, PANGO_STYLE_NORMAL,
                                COLUMN_TEXT_FG, "Black", -1);
            g_free (new_text);
          }

        gtk_widget_destroy (dialog);
        break;

      case EDIT_FONT:
        dialog = gtk_font_chooser_dialog_new (_("Select a font"),
                                              GTK_WINDOW (glade_app_get_window ()));

        /* Get response etc... */
        if (text)
          gtk_font_chooser_set_font (GTK_FONT_CHOOSER (dialog), text);

        if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
          {
            new_text = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (dialog));

            gtk_list_store_set (GTK_LIST_STORE (eprop_attrs->model), &iter,
                                COLUMN_TEXT, new_text,
                                COLUMN_NAME_WEIGHT, PANGO_WEIGHT_BOLD,
                                COLUMN_TEXT_STYLE, PANGO_STYLE_NORMAL,
                                COLUMN_TEXT_FG, "Black", -1);
            g_free (new_text);

          }
        gtk_widget_destroy (dialog);
        break;

      default:
        break;
    }

  sync_object (eprop_attrs, FALSE);

  g_free (text);
}

static void
value_toggled (GtkCellRendererToggle *cell_renderer,
               gchar *path,
               GladeEPropAttrs *eprop_attrs)
{
  gboolean active;
  GtkTreeIter iter;
  PangoAttrType type;

  if (!gtk_tree_model_get_iter_from_string (eprop_attrs->model, &iter, path))
    return;

  gtk_tree_model_get (eprop_attrs->model, &iter,
                      COLUMN_TOGGLE_DOWN, &active, COLUMN_TYPE, &type, -1);

  gtk_list_store_set (GTK_LIST_STORE (eprop_attrs->model), &iter,
                      COLUMN_NAME_WEIGHT, PANGO_WEIGHT_BOLD,
                      COLUMN_TOGGLE_DOWN, !active, -1);

  sync_object (eprop_attrs, FALSE);
}

static void
value_combo_spin_edited (GtkCellRendererText *cell,
                         const gchar *path,
                         const gchar *new_text,
                         GladeEPropAttrs *eprop_attrs)
{
  GtkTreeIter iter;
  PangoAttrType type;

  if (!gtk_tree_model_get_iter_from_string (eprop_attrs->model, &iter, path))
    return;

  gtk_tree_model_get (eprop_attrs->model, &iter, COLUMN_TYPE, &type, -1);

  /* Reset the column */
  if (new_text && (*new_text == '\0' || strcmp (new_text, _("None")) == 0))
    {
      gtk_list_store_set (GTK_LIST_STORE (eprop_attrs->model), &iter,
                          COLUMN_TEXT, _("<Enter Value>"),
                          COLUMN_NAME_WEIGHT, PANGO_WEIGHT_NORMAL,
                          COLUMN_TEXT_STYLE, PANGO_STYLE_ITALIC,
                          COLUMN_TEXT_FG, "Grey", -1);
    }
  else
    gtk_list_store_set (GTK_LIST_STORE (eprop_attrs->model), &iter,
                        COLUMN_TEXT, new_text,
                        COLUMN_NAME_WEIGHT, PANGO_WEIGHT_BOLD,
                        COLUMN_TEXT_STYLE, PANGO_STYLE_NORMAL,
                        COLUMN_TEXT_FG, "Black", -1);

  sync_object (eprop_attrs, FALSE);

}

static void
value_combo_spin_editing_started (GtkCellRenderer *cell,
                                  GtkCellEditable *editable,
                                  const gchar     *path)
{
  if (GTK_IS_SPIN_BUTTON (editable))
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (editable), TRUE);
}

static GtkWidget *
glade_eprop_attrs_view (GladeEditorProperty *eprop)
{
  GladeEPropAttrs *eprop_attrs = GLADE_EPROP_ATTRS (eprop);
  GtkWidget *view_widget;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  eprop_attrs->model = (GtkTreeModel *)
    gtk_list_store_new (NUM_COLUMNS,
                        /* Main Data */
                        G_TYPE_STRING,      // COLUMN_NAME
                        G_TYPE_INT, // COLUMN_NAME_WEIGHT
                        G_TYPE_INT, // COLUMN_TYPE
                        G_TYPE_INT, // COLUMN_EDIT_TYPE
                        G_TYPE_POINTER,     // COLUMN_VALUE
                        G_TYPE_UINT,        // COLUMN_START
                        G_TYPE_UINT,        // COLUMN_END
                        /* Editor renderer related */
                        G_TYPE_BOOLEAN,     // COLUMN_TOGGLE_ACTIVE
                        G_TYPE_BOOLEAN,     // COLUMN_TOGGLE_DOWN
                        G_TYPE_BOOLEAN,     // COLUMN_BUTTON_ACTIVE
                        G_TYPE_STRING,      // COLUMN_TEXT
                        G_TYPE_INT, // COLUMN_TEXT_STYLE
                        G_TYPE_STRING,      // COLUMN_TEXT_FG
                        G_TYPE_BOOLEAN,     // COLUMN_COMBO_ACTIVE
                        GTK_TYPE_LIST_STORE,        // COLUMN_COMBO_MODEL
                        G_TYPE_BOOLEAN,     // COLUMN_SPIN_ACTIVE
                        G_TYPE_UINT,        // COLUMN_SPIN_DIGITS
                        GTK_TYPE_ADJUSTMENT);// COLUMN_SPIN_ADJUSTMENT
  
  view_widget = gtk_tree_view_new_with_model (eprop_attrs->model);
  gtk_tree_view_set_show_expanders (GTK_TREE_VIEW (view_widget), FALSE);
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (view_widget), FALSE);

  /********************* attribute name column *********************/
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);
  column = gtk_tree_view_column_new_with_attributes
      (_("Attribute"), renderer,
       "text", COLUMN_NAME, "weight", COLUMN_NAME_WEIGHT, NULL);

  gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

  /********************* attribute value column *********************/
  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_set_title (column, _("Value"));

  /* Toggle renderer */
  renderer = gtk_cell_renderer_toggle_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "activatable", COLUMN_TOGGLE_ACTIVE,
                                       "visible", COLUMN_TOGGLE_ACTIVE,
                                       "active", COLUMN_TOGGLE_DOWN, NULL);
  g_signal_connect (G_OBJECT (renderer), "toggled",
                    G_CALLBACK (value_toggled), eprop);


  /* Text renderer */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "editable", COLUMN_BUTTON_ACTIVE,
                                       "visible", COLUMN_BUTTON_ACTIVE,
                                       "text", COLUMN_TEXT,
                                       "style", COLUMN_TEXT_STYLE,
                                       "foreground", COLUMN_TEXT_FG, NULL);

  /* Icon renderer */
  renderer = glade_cell_renderer_icon_new ();
  g_object_set (G_OBJECT (renderer), "icon-name", "gtk-edit", NULL);
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "activatable", COLUMN_BUTTON_ACTIVE,
                                       "visible", COLUMN_BUTTON_ACTIVE, NULL);

  g_signal_connect (G_OBJECT (renderer), "activate",
                    G_CALLBACK (value_icon_activate), eprop);

  /* Combo renderer */
  renderer = gtk_cell_renderer_combo_new ();
  g_object_set (G_OBJECT (renderer), "text-column", 0, "has-entry", FALSE,
                NULL);
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "editable", COLUMN_COMBO_ACTIVE,
                                       "visible", COLUMN_COMBO_ACTIVE,
                                       "model", COLUMN_COMBO_MODEL,
                                       "text", COLUMN_TEXT,
                                       "style", COLUMN_TEXT_STYLE,
                                       "foreground", COLUMN_TEXT_FG, NULL);
  g_signal_connect (G_OBJECT (renderer), "edited",
                    G_CALLBACK (value_combo_spin_edited), eprop);


  /* Spin renderer */
  renderer = gtk_cell_renderer_spin_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "visible", COLUMN_SPIN_ACTIVE,
                                       "editable", COLUMN_SPIN_ACTIVE,
                                       "text", COLUMN_TEXT,
                                       "style", COLUMN_TEXT_STYLE,
                                       "foreground", COLUMN_TEXT_FG,
                                       "digits", COLUMN_SPIN_DIGITS,
                                       "adjustment", COLUMN_SPIN_ADJUSTMENT,
                                       NULL);
  g_signal_connect (G_OBJECT (renderer), "edited",
                    G_CALLBACK (value_combo_spin_edited), eprop);
  g_signal_connect (G_OBJECT (renderer), "editing-started",
                    G_CALLBACK (value_combo_spin_editing_started), NULL);

  gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

  return view_widget;
}

static void
glade_eprop_attrs_populate_view (GladeEditorProperty *eprop, GtkTreeView *view)
{
  GList *attributes, *list;
  GtkListStore *model = (GtkListStore *) gtk_tree_view_get_model (view);
  GtkTreeIter *iter;
  GladeAttribute *gattr;
  GladeProperty *property;
  gchar *text;

  property   = glade_editor_property_get_property (eprop);
  attributes = g_value_get_boxed (glade_property_inline_value (property));

  append_empty_row (model, PANGO_ATTR_FONT_DESC);
  append_empty_row (model, PANGO_ATTR_STYLE);
  append_empty_row (model, PANGO_ATTR_WEIGHT);
  append_empty_row (model, PANGO_ATTR_VARIANT);
  append_empty_row (model, PANGO_ATTR_LANGUAGE);
  append_empty_row (model, PANGO_ATTR_STRETCH);
  append_empty_row (model, PANGO_ATTR_SCALE);
  append_empty_row (model, PANGO_ATTR_UNDERLINE);
  append_empty_row (model, PANGO_ATTR_STRIKETHROUGH);
  append_empty_row (model, PANGO_ATTR_FOREGROUND);
  append_empty_row (model, PANGO_ATTR_BACKGROUND);
  append_empty_row (model, PANGO_ATTR_UNDERLINE_COLOR);
  append_empty_row (model, PANGO_ATTR_STRIKETHROUGH_COLOR);
  append_empty_row (model, PANGO_ATTR_GRAVITY);
  append_empty_row (model, PANGO_ATTR_GRAVITY_HINT);
  append_empty_row (model, PANGO_ATTR_SIZE);
  append_empty_row (model, PANGO_ATTR_ABSOLUTE_SIZE);
  append_empty_row (model, PANGO_ATTR_SHAPE);

  /* XXX Populate here ...
   */
  for (list = attributes; list; list = list->next)
    {
      gattr = list->data;

      if ((iter = get_row_by_type (GTK_TREE_MODEL (model), gattr->type)))
        {
          text = glade_gtk_string_from_attr (gattr);

          gtk_list_store_set (GTK_LIST_STORE (model), iter,
                              COLUMN_NAME_WEIGHT, PANGO_WEIGHT_BOLD,
                              COLUMN_TEXT, text,
                              COLUMN_TEXT_STYLE, PANGO_STYLE_NORMAL,
                              COLUMN_TEXT_FG, "Black", -1);

          if (gattr->type == PANGO_ATTR_UNDERLINE ||
              gattr->type == PANGO_ATTR_STRIKETHROUGH)
            gtk_list_store_set (GTK_LIST_STORE (model), iter,
                                COLUMN_TOGGLE_DOWN,
                                g_value_get_boolean (&(gattr->value)), -1);

          g_free (text);
          gtk_tree_iter_free (iter);
        }

    }

}

static void
glade_eprop_attrs_show_dialog (GtkWidget *dialog_button,
                               GladeEditorProperty *eprop)
{
  GladeEPropAttrs *eprop_attrs = GLADE_EPROP_ATTRS (eprop);
  GtkWidget *dialog, *parent, *vbox, *sw, *tree_view;
  GladeProperty *property;
  GList *old_attributes;
  gint res;

  property = glade_editor_property_get_property (eprop);
  parent   = gtk_widget_get_toplevel (GTK_WIDGET (eprop));


  /* Keep a copy for commit time... */
  old_attributes = g_value_dup_boxed (glade_property_inline_value (property));

  dialog = gtk_dialog_new_with_buttons (_("Setup Text Attributes"),
                                        GTK_WINDOW (parent),
                                        GTK_DIALOG_MODAL |
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        _("C_lear"), GLADE_RESPONSE_CLEAR,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_OK"), GTK_RESPONSE_OK, NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_show (vbox);

  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);

  gtk_box_pack_start (GTK_BOX
                      (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), vbox,
                      TRUE, TRUE, 0);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (sw);
  gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
  gtk_widget_set_size_request (sw, 400, 200);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);

  tree_view = glade_eprop_attrs_view (eprop);
  glade_eprop_attrs_populate_view (eprop, GTK_TREE_VIEW (tree_view));

  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  gtk_widget_show (tree_view);
  gtk_container_add (GTK_CONTAINER (sw), tree_view);

  /* Run the dialog */
  res = gtk_dialog_run (GTK_DIALOG (dialog));
  if (res == GTK_RESPONSE_OK)
    {
      /* Update from old attributes so that there a property change 
       * sitting on the undo stack.
       */
      glade_property_set (property, old_attributes);
      sync_object (eprop_attrs, TRUE);
    }
  else if (res == GLADE_RESPONSE_CLEAR)
    {
      GValue value = { 0, };
      g_value_init (&value, GLADE_TYPE_ATTR_GLIST);
      g_value_set_boxed (&value, NULL);
      glade_editor_property_commit (eprop, &value);
      g_value_unset (&value);
    }

  /* Clean up ...
   */
  gtk_widget_destroy (dialog);

  g_object_unref (G_OBJECT (eprop_attrs->model));
  eprop_attrs->model = NULL;

  glade_attr_list_free (old_attributes);
}


static void
glade_eprop_attrs_finalize (GObject *object)
{
  /* Chain up */
  GObjectClass *parent_class =
      g_type_class_peek_parent (G_OBJECT_GET_CLASS (object));

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
glade_eprop_attrs_load (GladeEditorProperty *eprop, GladeProperty *property)
{
  GladeEditorPropertyClass *parent_class =
      g_type_class_peek_parent (G_OBJECT_GET_CLASS (eprop));

  /* No displayable attributes in eprop, just a button. */
  parent_class->load (eprop, property);

}

static GtkWidget *
glade_eprop_attrs_create_input (GladeEditorProperty *eprop)
{
  GtkWidget *hbox;
  GtkWidget *button;

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  button = gtk_button_new_with_label (_("Edit Attributes"));
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_show (button);
  gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (glade_eprop_attrs_show_dialog), eprop);

  return hbox;
}
