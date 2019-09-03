/*
 * glade-gtk-label.c - GladeWidgetAdaptor for GtkLabel
 *
 * Copyright (C) 2013 Tristan Van Berkom
 *
 * Authors:
 *      Tristan Van Berkom <tristan.van.berkom@gmail.com>
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
#include <glib/gi18n-lib.h>
#include <gladeui/glade.h>

#include "glade-label-editor.h"
#include "glade-attributes.h"
#include "glade-gtk.h"

void
glade_gtk_label_post_create (GladeWidgetAdaptor *adaptor,
                             GObject            *object,
                             GladeCreateReason   reason)
{
  GladeWidget *glabel = glade_widget_get_from_gobject (object);

  if (reason == GLADE_CREATE_USER)
    glade_widget_property_set_sensitive (glabel, "mnemonic-widget", FALSE,
                                         MNEMONIC_INSENSITIVE_MSG);
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
  GladeAttribute *gattr;
  PangoAttribute *attribute;
  PangoLanguage *language;
  PangoFontDescription *font_desc;
  PangoAttrList *attrs = NULL;
  GdkColor *color;
  GList *list;

  for (list = g_value_get_boxed (value); list; list = list->next)
    {
      gattr = list->data;

      attribute = NULL;

      switch (gattr->type)
        {
            /* PangoFontDescription */
          case PANGO_ATTR_FONT_DESC:
            if ((font_desc = 
                 pango_font_description_from_string (g_value_get_string (&gattr->value))))
              {
                attribute = pango_attr_font_desc_new (font_desc);
                pango_font_description_free (font_desc);
              }
            break;

            /* PangoAttrLanguage */
          case PANGO_ATTR_LANGUAGE:
            if ((language =
                 pango_language_from_string (g_value_get_string (&gattr->value))))
              attribute = pango_attr_language_new (language);
            break;
            /* PangoAttrInt */
          case PANGO_ATTR_STYLE:
            attribute =
                pango_attr_style_new (g_value_get_enum (&(gattr->value)));
            break;
          case PANGO_ATTR_WEIGHT:
            attribute =
                pango_attr_weight_new (g_value_get_enum (&(gattr->value)));
            break;
          case PANGO_ATTR_VARIANT:
            attribute =
                pango_attr_variant_new (g_value_get_enum (&(gattr->value)));
            break;
          case PANGO_ATTR_STRETCH:
            attribute =
                pango_attr_stretch_new (g_value_get_enum (&(gattr->value)));
            break;
          case PANGO_ATTR_UNDERLINE:
            attribute =
                pango_attr_underline_new (g_value_get_boolean
                                          (&(gattr->value)));
            break;
          case PANGO_ATTR_STRIKETHROUGH:
            attribute =
                pango_attr_strikethrough_new (g_value_get_boolean
                                              (&(gattr->value)));
            break;
          case PANGO_ATTR_GRAVITY:
            attribute =
                pango_attr_gravity_new (g_value_get_enum (&(gattr->value)));
            break;
          case PANGO_ATTR_GRAVITY_HINT:
            attribute =
                pango_attr_gravity_hint_new (g_value_get_enum
                                             (&(gattr->value)));
            break;

            /* PangoAttrString */
          case PANGO_ATTR_FAMILY:
            attribute =
                pango_attr_family_new (g_value_get_string (&(gattr->value)));
            break;

            /* PangoAttrSize */
          case PANGO_ATTR_SIZE:
            attribute = pango_attr_size_new (g_value_get_int (&(gattr->value)));
            break;
          case PANGO_ATTR_ABSOLUTE_SIZE:
            attribute =
                pango_attr_size_new_absolute (g_value_get_int
                                              (&(gattr->value)));
            break;

            /* PangoAttrColor */
          case PANGO_ATTR_FOREGROUND:
            color = g_value_get_boxed (&(gattr->value));
            attribute =
                pango_attr_foreground_new (color->red, color->green,
                                           color->blue);
            break;
          case PANGO_ATTR_BACKGROUND:
            color = g_value_get_boxed (&(gattr->value));
            attribute =
                pango_attr_background_new (color->red, color->green,
                                           color->blue);
            break;
          case PANGO_ATTR_UNDERLINE_COLOR:
            color = g_value_get_boxed (&(gattr->value));
            attribute =
                pango_attr_underline_color_new (color->red, color->green,
                                                color->blue);
            break;
          case PANGO_ATTR_STRIKETHROUGH_COLOR:
            color = g_value_get_boxed (&(gattr->value));
            attribute =
                pango_attr_strikethrough_color_new (color->red, color->green,
                                                    color->blue);
            break;

            /* PangoAttrShape */
          case PANGO_ATTR_SHAPE:
            /* Unsupported for now */
            break;
            /* PangoAttrFloat */
          case PANGO_ATTR_SCALE:
            attribute =
                pango_attr_scale_new (g_value_get_double (&(gattr->value)));
            break;

          case PANGO_ATTR_INVALID:
          case PANGO_ATTR_LETTER_SPACING:
          case PANGO_ATTR_RISE:
          case PANGO_ATTR_FALLBACK:
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

  pango_attr_list_unref (attrs);
}


static void
glade_gtk_label_set_content_mode (GObject *object, const GValue *value)
{
  GladeLabelContentMode mode = g_value_get_int (value);
  GladeWidget *glabel;

  glabel = glade_widget_get_from_gobject (object);

  glade_widget_property_set_sensitive (glabel, "glade-attributes", FALSE,
                                       NOT_SELECTED_MSG);
  glade_widget_property_set_sensitive (glabel, "use-markup", FALSE,
                                       NOT_SELECTED_MSG);
  glade_widget_property_set_sensitive (glabel, "pattern", FALSE,
                                       NOT_SELECTED_MSG);

  switch (mode)
    {
      case GLADE_LABEL_MODE_ATTRIBUTES:
        glade_widget_property_set_sensitive (glabel, "glade-attributes", TRUE,
                                             NULL);
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
glade_gtk_label_update_lines_sensitivity (GObject *object)
{
  GladeWidget *glabel;
  PangoEllipsizeMode ellipsize_mode;
  gint wrap_mode;

  glabel = glade_widget_get_from_gobject (object);

  glade_widget_property_get (glabel, "label-wrap-mode", &wrap_mode);
  glade_widget_property_get (glabel, "ellipsize", &ellipsize_mode);

  if (wrap_mode == GLADE_LABEL_WRAP_MODE && ellipsize_mode != PANGO_ELLIPSIZE_NONE)
    glade_widget_property_set_sensitive (glabel, "lines", TRUE, NULL);
  else
    glade_widget_property_set_sensitive (glabel, "lines", FALSE,
                                         _("This property only applies if ellipsize and wrapping are enabled"));
}

static void
glade_gtk_label_set_wrap_mode (GObject *object, const GValue *value)
{
  GladeLabelWrapMode mode = g_value_get_int (value);
  GladeWidget *glabel;

  glabel = glade_widget_get_from_gobject (object);

  glade_widget_property_set_sensitive (glabel, "single-line-mode", FALSE,
                                       NOT_SELECTED_MSG);
  glade_widget_property_set_sensitive (glabel, "wrap-mode", FALSE,
                                       NOT_SELECTED_MSG);

  if (mode == GLADE_LABEL_SINGLE_LINE)
    glade_widget_property_set_sensitive (glabel, "single-line-mode", TRUE,
                                         NULL);
  else if (mode == GLADE_LABEL_WRAP_MODE)
    glade_widget_property_set_sensitive (glabel, "wrap-mode", TRUE, NULL);

  glade_gtk_label_update_lines_sensitivity (object);
}

static void
glade_gtk_label_set_use_underline (GObject *object, const GValue *value)
{
  GladeWidget *glabel;

  glabel = glade_widget_get_from_gobject (object);

  if (g_value_get_boolean (value))
    glade_widget_property_set_sensitive (glabel, "mnemonic-widget", TRUE, NULL);
  else
    glade_widget_property_set_sensitive (glabel, "mnemonic-widget", FALSE,
                                         MNEMONIC_INSENSITIVE_MSG);

  gtk_label_set_use_underline (GTK_LABEL (object), g_value_get_boolean (value));
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
  else if (!strcmp (id, "label-wrap-mode"))
    glade_gtk_label_set_wrap_mode (object, value);
  else if (!strcmp (id, "use-underline"))
    glade_gtk_label_set_use_underline (object, value);
  else
    {
      if (!strcmp (id, "ellipsize"))
        glade_gtk_label_update_lines_sensitivity (object);

      GWA_GET_CLASS (GTK_TYPE_WIDGET)->set_property (adaptor, object, id, value);
    }
}

static void
glade_gtk_parse_attributes (GladeWidget *widget, GladeXmlNode *node)
{
  PangoAttrType attr_type;
  GladeXmlNode *prop;
  GladeAttribute *attr;
  GList *attrs = NULL;
  gchar *name, *value;

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
           glade_utils_enum_value_from_string (PANGO_TYPE_ATTR_TYPE,
                                               name)) == 0)
        continue;

      /* Parse attribute and add to list */
      if ((attr = glade_gtk_attribute_from_string (attr_type, value)) != NULL)
        attrs = g_list_prepend (attrs, attr);

      /* XXX deal with start/end here ... */

      g_free (name);
      g_free (value);
    }

  glade_widget_property_set (widget, "glade-attributes",
                             g_list_reverse (attrs));
  glade_attr_list_free (attrs);
}

static void
glade_gtk_label_read_attributes (GladeWidget *widget, GladeXmlNode *node)
{
  GladeXmlNode *attrs_node;

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

  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (GTK_TYPE_WIDGET)->read_widget (adaptor, widget, node);

  glade_gtk_label_read_attributes (widget, node);

  /* sync label property after a load... */
  prop = glade_widget_get_property (widget, "label");
  glade_gtk_label_set_label (glade_widget_get_object (widget), 
                             glade_property_inline_value (prop));

  /* Resolve "label-content-mode" virtual control property  */
  if (!glade_widget_property_original_default (widget, "use-markup"))
    glade_widget_property_set (widget, "label-content-mode",
                               GLADE_LABEL_MODE_MARKUP);
  else if (!glade_widget_property_original_default (widget, "pattern"))
    glade_widget_property_set (widget, "label-content-mode",
                               GLADE_LABEL_MODE_PATTERN);
  else
    glade_widget_property_set (widget, "label-content-mode",
                               GLADE_LABEL_MODE_ATTRIBUTES);

  /* Resolve "label-wrap-mode" virtual control property  */
  if (!glade_widget_property_original_default (widget, "single-line-mode"))
    glade_widget_property_set (widget, "label-wrap-mode",
                               GLADE_LABEL_SINGLE_LINE);
  else if (!glade_widget_property_original_default (widget, "wrap"))
    glade_widget_property_set (widget, "label-wrap-mode",
                               GLADE_LABEL_WRAP_MODE);
  else
    glade_widget_property_set (widget, "label-wrap-mode",
                               GLADE_LABEL_WRAP_FREE);

  if (glade_widget_property_original_default (widget, "use-underline"))
    glade_widget_property_set_sensitive (widget, "mnemonic-widget",
                                         FALSE, MNEMONIC_INSENSITIVE_MSG);

}

static void
glade_gtk_label_write_attributes (GladeWidget     *widget,
                                  GladeXmlContext *context,
                                  GladeXmlNode    *node)
{
  GladeXmlNode *attr_node;
  GList *attrs = NULL, *l;
  GladeAttribute *gattr;
  gchar *attr_type;
  gchar *attr_value;

  if (!glade_widget_property_get (widget, "glade-attributes", &attrs) || !attrs)
    return;

  for (l = attrs; l; l = l->next)
    {
      gattr = l->data;

      attr_type =
          glade_utils_enum_string_from_value (PANGO_TYPE_ATTR_TYPE,
                                              gattr->type);
      attr_value = glade_gtk_string_from_attr (gattr);

      attr_node = glade_xml_node_new (context, GLADE_TAG_ATTRIBUTE);
      glade_xml_node_append_child (node, attr_node);

      glade_xml_node_set_property_string (attr_node, GLADE_TAG_NAME, attr_type);
      glade_xml_node_set_property_string (attr_node, GLADE_TAG_VALUE,
                                          attr_value);
      g_free (attr_type);
      g_free (attr_value);
    }
}

void
glade_gtk_label_write_widget (GladeWidgetAdaptor *adaptor,
                              GladeWidget        *widget,
                              GladeXmlContext    *context,
                              GladeXmlNode       *node)
{
  GladeXmlNode *attrs_node;

  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (GTK_TYPE_WIDGET)->write_widget (adaptor, widget, context,
                                                 node);

  attrs_node = glade_xml_node_new (context, GLADE_TAG_ATTRIBUTES);

  glade_gtk_label_write_attributes (widget, context, attrs_node);

  if (!glade_xml_node_get_children (attrs_node))
    glade_xml_node_delete (attrs_node);
  else
    glade_xml_node_append_child (node, attrs_node);

}

gchar *
glade_gtk_label_string_from_value (GladeWidgetAdaptor *adaptor,
                                   GladePropertyDef   *def,
                                   const GValue       *value)
{
  GParamSpec          *pspec;

  pspec = glade_property_def_get_pspec (def);

  if (pspec->value_type == GLADE_TYPE_ATTR_GLIST)
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
        (GTK_TYPE_WIDGET)->string_from_value (adaptor, def, value);
}


GladeEditorProperty *
glade_gtk_label_create_eprop (GladeWidgetAdaptor *adaptor,
                              GladePropertyDef   *def,
                              gboolean            use_command)
{
  GladeEditorProperty *eprop;
  GParamSpec          *pspec;

  pspec = glade_property_def_get_pspec (def);

  /* chain up.. */
  if (pspec->value_type == GLADE_TYPE_ATTR_GLIST)
    {
      eprop = g_object_new (GLADE_TYPE_EPROP_ATTRS,
                            "property-def", def,
                            "use-command", use_command, NULL);
    }
  else
    eprop = GWA_GET_CLASS
        (GTK_TYPE_WIDGET)->create_eprop (adaptor, def, use_command);
  return eprop;
}

GladeEditable *
glade_gtk_label_create_editable (GladeWidgetAdaptor *adaptor,
                                 GladeEditorPageType type)
{
  GladeEditable *editable;

  if (type == GLADE_PAGE_GENERAL)
    editable = (GladeEditable *) glade_label_editor_new ();
  else
    editable = GWA_GET_CLASS (GTK_TYPE_WIDGET)->create_editable (adaptor, type);

  return editable;
}
