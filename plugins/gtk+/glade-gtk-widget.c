/*
 * glade-gtk-widget.c - GladeWidgetAdaptor for GtkWidget
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
#include <string.h>

#include "glade-widget-editor.h"
#include "glade-string-list.h"
#include "glade-accels.h"

#define GLADE_TAG_A11Y_A11Y         "accessibility"
#define GLADE_TAG_A11Y_ACTION_NAME  "action_name"       /* We should make -/_ synonymous */
#define GLADE_TAG_A11Y_DESC         "description"
#define GLADE_TAG_A11Y_TARGET       "target"
#define GLADE_TAG_A11Y_TYPE         "type"

#define GLADE_TAG_A11Y_INTERNAL_NAME "accessible"

#define GLADE_TAG_A11Y_RELATION     "relation"
#define GLADE_TAG_A11Y_ACTION       "action"
#define GLADE_TAG_A11Y_PROPERTY     "property"

#define GLADE_TAG_STYLE             "style"
#define GLADE_TAG_CLASS             "class"


/* This function does absolutely nothing
 * (and is for use in overriding some post_create functions
 * throughout the catalog).
 */
void
empty (GObject * container, GladeCreateReason reason)
{
}

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


void
glade_gtk_widget_destroy_object (GladeWidgetAdaptor *adaptor,
                                 GObject            *object)
{
  gtk_widget_destroy (GTK_WIDGET (object));

  GWA_GET_CLASS (G_TYPE_OBJECT)->destroy_object (adaptor, object);
}

static void
glade_gtk_parse_atk_relation (GladeProperty *property, GladeXmlNode *node)
{
  GladeXmlNode *prop;
  GladePropertyDef *pdef;
  gchar *type, *target, *id, *tmp;
  gchar *string = NULL;

  for (prop = glade_xml_node_get_children (node);
       prop; prop = glade_xml_node_next (prop))
    {
      if (!glade_xml_node_verify_silent (prop, GLADE_TAG_A11Y_RELATION))
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

      id   = glade_util_read_prop_name (type);
      pdef = glade_property_get_def (property);

      if (!strcmp (id, glade_property_def_id (pdef)))
        {
          if (string == NULL)
            string = g_strdup (target);
          else
            {
              tmp = g_strdup_printf ("%s%s%s", string,
                                     GLADE_PROPERTY_DEF_OBJECT_DELIMITER, target);
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
                              /* 'string' here is already allocated on the heap */
                              string, g_free);
    }
}

static void
glade_gtk_parse_atk_props (GladeWidget *widget, GladeXmlNode *node)
{
  GladeXmlNode *prop;
  GladeProperty *property;
  GValue *gvalue;
  gchar *value, *name, *id, *context, *comment;
  gint translatable;
  gboolean is_action;

  for (prop = glade_xml_node_get_children (node);
       prop; prop = glade_xml_node_next (prop))
    {
      if (glade_xml_node_verify_silent (prop, GLADE_TAG_A11Y_PROPERTY))
        is_action = FALSE;
      else if (glade_xml_node_verify_silent (prop, GLADE_TAG_A11Y_ACTION))
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
          gvalue = glade_property_def_make_gvalue_from_string
            (glade_property_get_def (property), value, glade_widget_get_project (widget));
          glade_property_set_value (property, gvalue);
          g_value_unset (gvalue);
          g_free (gvalue);

          /* Deal with i18n... ... XXX Do i18n context !!! */
          translatable = glade_xml_get_property_boolean
              (prop, GLADE_TAG_TRANSLATABLE, FALSE);
          context = glade_xml_get_property_string (prop, GLADE_TAG_CONTEXT);
          comment = glade_xml_get_property_string (prop, GLADE_TAG_COMMENT);

          glade_property_i18n_set_translatable (property, translatable);
          glade_property_i18n_set_context (property, context);
          glade_property_i18n_set_comment (property, comment);

          g_free (comment);
          g_free (context);
          g_free (value);
        }

      g_free (id);
    }
}

static void
glade_gtk_parse_atk_props_gtkbuilder (GladeWidget *widget, GladeXmlNode *node)
{
  GladeXmlNode *child, *object_node;
  gchar *internal;

  /* Search for internal "accessible" child and redirect parse from there */
  for (child = glade_xml_node_get_children (node);
       child; child = glade_xml_node_next (child))
    {
      if (glade_xml_node_verify_silent (child, GLADE_XML_TAG_CHILD))
        {
          if ((internal =
               glade_xml_get_property_string (child,
                                              GLADE_XML_TAG_INTERNAL_CHILD)))
            {
              if (!strcmp (internal, GLADE_TAG_A11Y_INTERNAL_NAME) &&
                  (object_node =
                   glade_xml_search_child_required (child,
                                                    GLADE_XML_TAG_WIDGET)))
                glade_gtk_parse_atk_props (widget, object_node);

              g_free (internal);
            }
        }
    }
}

static void
glade_gtk_widget_read_atk_props (GladeWidget *widget, GladeXmlNode *node)
{
  GladeXmlNode *atk_node;
  GladeProperty *property;
  gint i;

  glade_gtk_parse_atk_props_gtkbuilder (widget, node);

  if ((atk_node = glade_xml_search_child (node, GLADE_TAG_A11Y_A11Y)) != NULL)
    {
      /* Properties & actions */
      glade_gtk_parse_atk_props (widget, atk_node);

      /* Relations */
      for (i = 0; atk_relations_list[i]; i++)
        {
          if ((property =
               glade_widget_get_property (widget, atk_relations_list[i])))
            glade_gtk_parse_atk_relation (property, atk_node);
          else
            g_warning ("Couldnt find atk relation %s", atk_relations_list[i]);
        }
    }
}

static void
glade_gtk_widget_read_style_classes (GladeWidget *widget, GladeXmlNode *node)
{
  GladeXmlNode *style_node;
  GladeXmlNode *class_node;
  GList        *string_list = NULL;

  if ((style_node = glade_xml_search_child (node, GLADE_TAG_STYLE)) != NULL)
    {
      for (class_node = glade_xml_node_get_children (style_node);
           class_node; class_node = glade_xml_node_next (class_node))
        {
          gchar *name;

          if (!glade_xml_node_verify (class_node, GLADE_TAG_CLASS))
            continue;

          name = glade_xml_get_property_string (class_node, GLADE_TAG_NAME);

          string_list = glade_string_list_append (string_list, name, NULL, NULL, FALSE, NULL);

          g_free (name);
        }

      glade_widget_property_set (widget, "glade-style-classes", string_list);
      glade_string_list_free (string_list);
    }
}

void
glade_gtk_widget_read_widget (GladeWidgetAdaptor *adaptor,
                              GladeWidget        *widget,
                              GladeXmlNode       *node)
{
  const gchar *tooltip_markup = NULL;

  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* First chain up and read in all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->read_widget (adaptor, widget, node);

  /* Read in accelerators */
  glade_gtk_read_accels (widget, node, TRUE);

  /* Read in atk props */
  glade_gtk_widget_read_atk_props (widget, node);

  /* Read in the style classes */
  glade_gtk_widget_read_style_classes (widget, node);

  /* Resolve the virtual tooltip use markup property */
  glade_widget_property_get (widget, "tooltip-markup", &tooltip_markup);
  if (tooltip_markup != NULL)
    glade_widget_property_set (widget, "glade-tooltip-markup", TRUE);
}

static void
glade_gtk_widget_write_atk_property (GladeProperty   *property,
                                     GladeXmlContext *context,
                                     GladeXmlNode    *node)
{
  gchar *value = glade_property_make_string (property);

  if (value && value[0])
    {
      GladePropertyDef *pdef = glade_property_get_def (property);
      GladeXmlNode *prop_node = glade_xml_node_new (context, GLADE_TAG_A11Y_PROPERTY);
      const gchar *comment, *context;

      glade_xml_node_append_child (node, prop_node);

      glade_xml_node_set_property_string (prop_node,
                                          GLADE_TAG_NAME, glade_property_def_id (pdef));

      glade_xml_set_content (prop_node, value);

      if (glade_property_i18n_get_translatable (property))
        glade_xml_node_set_property_string (prop_node,
                                            GLADE_TAG_TRANSLATABLE,
                                            GLADE_XML_TAG_I18N_TRUE);

      comment = glade_property_i18n_get_comment (property);
      if (comment)
        glade_xml_node_set_property_string (prop_node,
                                            GLADE_TAG_COMMENT,
                                            comment);

      context = glade_property_i18n_get_context (property);
      if (context)
        glade_xml_node_set_property_string (prop_node,
                                            GLADE_TAG_CONTEXT,
                                            context);
    }

  g_free (value);
}

static void
glade_gtk_widget_write_atk_properties (GladeWidget     *widget,
                                       GladeXmlContext *context,
                                       GladeXmlNode    *node)
{
  GladeXmlNode *child_node, *object_node;
  GladeProperty *name_prop, *desc_prop, *role_prop;

  name_prop = glade_widget_get_property (widget, "AtkObject::accessible-name");
  desc_prop =
      glade_widget_get_property (widget, "AtkObject::accessible-description");
  role_prop = glade_widget_get_property (widget, "AtkObject::accessible-role");

  /* Create internal child here if any of these properties are non-null */
  if (!glade_property_default (name_prop) ||
      !glade_property_default (desc_prop) ||
      !glade_property_default (role_prop))
    {
      const gchar *widget_name = glade_widget_get_name (widget);
      gchar *atkname = NULL;

      if (!g_str_has_prefix (widget_name, GLADE_UNNAMED_PREFIX))
        atkname = g_strdup_printf ("%s-atkobject", widget_name);

      child_node = glade_xml_node_new (context, GLADE_XML_TAG_CHILD);
      glade_xml_node_append_child (node, child_node);

      glade_xml_node_set_property_string (child_node,
                                          GLADE_XML_TAG_INTERNAL_CHILD,
                                          GLADE_TAG_A11Y_INTERNAL_NAME);

      object_node = glade_xml_node_new (context, GLADE_XML_TAG_WIDGET);
      glade_xml_node_append_child (child_node, object_node);

      glade_xml_node_set_property_string (object_node,
                                          GLADE_XML_TAG_CLASS, "AtkObject");

      if (atkname)
        glade_xml_node_set_property_string (object_node,
                                            GLADE_XML_TAG_ID, atkname);

      if (!glade_property_default (name_prop))
        glade_gtk_widget_write_atk_property (name_prop, context, object_node);
      if (!glade_property_default (desc_prop))
        glade_gtk_widget_write_atk_property (desc_prop, context, object_node);
      if (!glade_property_default (role_prop))
        glade_gtk_widget_write_atk_property (role_prop, context, object_node);

      g_free (atkname);
    }

}

static void
glade_gtk_widget_write_atk_relation (GladeProperty   *property,
                                     GladeXmlContext *context,
                                     GladeXmlNode    *node)
{
  GladeXmlNode *prop_node;
  GladePropertyDef *pdef;
  gchar *value, **split;
  gint i;

  if ((value = glade_widget_adaptor_string_from_value
       (glade_property_def_get_adaptor (glade_property_get_def (property)),
        glade_property_get_def (property), glade_property_inline_value (property))) != NULL)
    {
      if ((split = g_strsplit (value, GLADE_PROPERTY_DEF_OBJECT_DELIMITER, 0)) != NULL)
        {
          for (i = 0; split[i] != NULL; i++)
            {
              pdef = glade_property_get_def (property);

              prop_node = glade_xml_node_new (context, GLADE_TAG_A11Y_RELATION);
              glade_xml_node_append_child (node, prop_node);

              glade_xml_node_set_property_string (prop_node,
                                                  GLADE_TAG_A11Y_TYPE,
                                                  glade_property_def_id (pdef));
              glade_xml_node_set_property_string (prop_node,
                                                  GLADE_TAG_A11Y_TARGET,
                                                  split[i]);
            }
          g_strfreev (split);
        }
    }
}

static void
glade_gtk_widget_write_atk_relations (GladeWidget     *widget,
                                      GladeXmlContext *context,
                                      GladeXmlNode    *node)
{
  GladeProperty *property;
  gint i;

  for (i = 0; atk_relations_list[i]; i++)
    {
      if ((property =
           glade_widget_get_property (widget, atk_relations_list[i])))
        glade_gtk_widget_write_atk_relation (property, context, node);
      else
        g_warning ("Couldnt find atk relation %s on widget %s",
                   atk_relations_list[i], glade_widget_get_name (widget));
    }
}

static void
glade_gtk_widget_write_atk_action (GladeProperty   *property,
                                   GladeXmlContext *context,
                                   GladeXmlNode    *node)
{
  gchar *value = glade_property_make_string (property);

  if (value && value[0])
    {
      GladePropertyDef *pdef = glade_property_get_def (property);
      GladeXmlNode *prop_node = glade_xml_node_new (context, GLADE_TAG_A11Y_ACTION);
      glade_xml_node_append_child (node, prop_node);

      glade_xml_node_set_property_string (prop_node,
                                          GLADE_TAG_A11Y_ACTION_NAME,
                                          &glade_property_def_id (pdef)[4]);
      glade_xml_node_set_property_string (prop_node,
                                          GLADE_TAG_A11Y_DESC, value);
    }

  g_free (value);
}

static void
glade_gtk_widget_write_atk_actions (GladeWidget     *widget,
                                    GladeXmlContext *context,
                                    GladeXmlNode    *node)
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
glade_gtk_widget_write_atk_props (GladeWidget     *widget,
                                  GladeXmlContext *context,
                                  GladeXmlNode    *node)
{
  GladeXmlNode *atk_node;

  atk_node = glade_xml_node_new (context, GLADE_TAG_A11Y_A11Y);

  glade_gtk_widget_write_atk_relations (widget, context, atk_node);
  glade_gtk_widget_write_atk_actions (widget, context, atk_node);

  if (!glade_xml_node_get_children (atk_node))
    glade_xml_node_delete (atk_node);
  else
    glade_xml_node_append_child (node, atk_node);

  glade_gtk_widget_write_atk_properties (widget, context, node);
}


static void
glade_gtk_widget_write_style_classes (GladeWidget     *widget,
                                      GladeXmlContext *context,
                                      GladeXmlNode    *node)
{
  GladeXmlNode *class_node, *style_node;
  GList        *string_list = NULL, *l;
  GladeString  *string;

  if (!glade_widget_property_get (widget, "glade-style-classes", &string_list) || !string_list)
    return;

  style_node = glade_xml_node_new (context, GLADE_TAG_STYLE);

  for (l = string_list; l; l = l->next)
    {
      string = l->data;

      class_node = glade_xml_node_new (context, GLADE_TAG_CLASS);
      glade_xml_node_append_child (style_node, class_node);

      glade_xml_node_set_property_string (class_node,
                                          GLADE_TAG_NAME,
                                          string->string);
    }

  if (!glade_xml_node_get_children (style_node))
    glade_xml_node_delete (style_node);
  else
    glade_xml_node_append_child (node, style_node);
}

void
glade_gtk_widget_write_widget (GladeWidgetAdaptor *adaptor,
                               GladeWidget        *widget,
                               GladeXmlContext    *context,
                               GladeXmlNode       *node)
{
  GladeProperty *prop;

  if (!(glade_xml_node_verify_silent (node, GLADE_XML_TAG_WIDGET) ||
        glade_xml_node_verify_silent (node, GLADE_XML_TAG_TEMPLATE)))
    return;

  /* Make sure use-action-appearance and related-action properties are
   * ordered in a sane way and are only saved if there is an action */
  prop = glade_widget_get_property (widget, "use-action-appearance");
  if (prop && glade_property_get_enabled (prop))
    glade_property_write (prop, context, node);

  prop = glade_widget_get_property (widget, "related-action");
  if (prop && glade_property_get_enabled (prop))
    glade_property_write (prop, context, node);

  /* Finally chain up and read in all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget (adaptor, widget, context, node);
}

void
glade_gtk_widget_write_widget_after (GladeWidgetAdaptor *adaptor,
                                     GladeWidget        *widget,
                                     GladeXmlContext    *context,
                                     GladeXmlNode       *node)
{
  /* The ATK properties are actually children */
  glade_gtk_widget_write_atk_props (widget, context, node);

  /* Put the accelerators and style classes after children */
  glade_gtk_write_accels (widget, context, node, TRUE);
  glade_gtk_widget_write_style_classes (widget, context, node);

  /* Finally chain up and read in all the normal properties.. */
  GWA_GET_CLASS (G_TYPE_OBJECT)->write_widget_after (adaptor, widget, context, node);
}

GladeEditorProperty *
glade_gtk_widget_create_eprop (GladeWidgetAdaptor *adaptor,
                               GladePropertyDef   *def,
                               gboolean            use_command)
{
  GladeEditorProperty *eprop;
  GParamSpec          *pspec;

  pspec = glade_property_def_get_pspec (def);

  /* chain up.. */
  if (pspec->value_type == GLADE_TYPE_ACCEL_GLIST)
    eprop = g_object_new (GLADE_TYPE_EPROP_ACCEL,
                          "property-def", def,
                          "use-command", use_command, NULL);
  else if (pspec->value_type == GLADE_TYPE_STRING_LIST)
    eprop = glade_eprop_string_list_new (def, use_command, FALSE, FALSE);
  else
    eprop = GWA_GET_CLASS
        (G_TYPE_OBJECT)->create_eprop (adaptor, def, use_command);

  return eprop;
}

GladeEditable *
glade_gtk_widget_create_editable (GladeWidgetAdaptor *adaptor,
                                  GladeEditorPageType type)
{
  GladeEditable *editable;

  /* Get base editable */
  if (type == GLADE_PAGE_COMMON)
    editable = (GladeEditable *)glade_widget_editor_new ();
  else
    editable = GWA_GET_CLASS (G_TYPE_OBJECT)->create_editable (adaptor, type);

  return editable;
}

gchar *
glade_gtk_widget_string_from_value (GladeWidgetAdaptor *adaptor,
                                    GladePropertyDef   *def,
                                    const GValue       *value)
{
  GParamSpec          *pspec;

  pspec = glade_property_def_get_pspec (def);

  if (pspec->value_type == GLADE_TYPE_ACCEL_GLIST)
    return glade_accels_make_string (g_value_get_boxed (value));
  else if (pspec->value_type == GLADE_TYPE_STRING_LIST)
    {
      GList *list = g_value_get_boxed (value);

      return glade_string_list_to_string (list);
    }
  else
    return GWA_GET_CLASS
        (G_TYPE_OBJECT)->string_from_value (adaptor, def, value);
}

static void
widget_parent_changed (GtkWidget          *widget,
                       GParamSpec         *pspec,
                       GladeWidgetAdaptor *adaptor)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (widget);
  GladeWidget *parent;

  /* this could get called for a stale instance of an object
   * being rebuilt for a contruct-only property. */
  if (!gwidget)
    return;

  parent = glade_widget_get_parent (gwidget);

  if (parent && !glade_widget_get_internal (parent))
    glade_widget_set_action_sensitive (gwidget, "remove_parent", TRUE);
  else
    glade_widget_set_action_sensitive (gwidget, "remove_parent", FALSE);
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

  if (GWA_IS_TOPLEVEL (adaptor) || glade_widget_get_internal (gwidget))
    glade_widget_set_action_sensitive (gwidget, "add_parent", FALSE);

  /* Watch parents/projects and set actions sensitive/insensitive */
  if (!glade_widget_get_internal (gwidget))
    g_signal_connect (G_OBJECT (widget), "notify::parent",
                      G_CALLBACK (widget_parent_changed), adaptor);

  if (!glade_widget_adaptor_get_book (adaptor) || !glade_util_have_devhelp ())
    glade_widget_set_action_visible (gwidget, "read_documentation", FALSE);
}

void
glade_gtk_widget_set_property (GladeWidgetAdaptor *adaptor,
                               GObject            *object,
                               const gchar        *id,
                               const GValue       *value)
{
  /* FIXME: is this still needed with the new gtk+ tooltips? */
  if (!strcmp (id, "tooltip"))
    {
      id = "tooltip-text";
    }

  /* Setting can-focus in the runtime has been known to cause crashes */
  if (!strcmp (id, "can-focus"))
    return;

  if (!strcmp (id, "glade-style-classes"))
    {
      GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (object));
      GList *l, *styles;

      /* NOTE: we can not use gtk_style_context_list_classes() because we only
       * want to remove the classes we added not the ones added by the widget
       */
      styles = g_object_get_data (object, "glade-style-classes");
      for (l = styles; l; l = g_list_next (l))
        {
          GladeString *style = l->data;
          gtk_style_context_remove_class (context, style->string);
        }

      for (l = g_value_get_boxed (value); l; l = g_list_next (l))
        {
          GladeString *style = l->data;
          gtk_style_context_add_class (context, style->string);
        }

      /* Save the list here so we can use it later on to remove them from the style context */
      g_object_set_data_full (object, "glade-style-classes",
                              glade_string_list_copy (g_value_get_boxed (value)),
                              (GDestroyNotify) glade_string_list_free);
    }
  else
      GWA_GET_CLASS (G_TYPE_OBJECT)->set_property (adaptor, object, id, value);
}

void
glade_gtk_widget_get_property (GladeWidgetAdaptor * adaptor,
                               GObject * object,
                               const gchar * id, GValue * value)
{
  if (!strcmp (id, "tooltip"))
    {
      id = "tooltip-text";
    }

  GWA_GET_CLASS (G_TYPE_OBJECT)->get_property (adaptor, object, id, value);
}


static GList *
create_command_property_list (GladeWidget *gnew, GList *saved_props)
{
  GList *l, *command_properties = NULL;

  for (l = saved_props; l; l = l->next)
    {
      GladeProperty *property = l->data;
      GladePropertyDef *pdef = glade_property_get_def (property);
      GladeProperty *orig_prop =
        glade_widget_get_pack_property (gnew, glade_property_def_id (pdef));
      GladeCommandSetPropData *pdata = g_new0 (GladeCommandSetPropData, 1);

      pdata->property = orig_prop;
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
                                  GObject            *object, 
                                  const gchar        *action_path)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object), *gparent;
  GList this_widget = { 0, }, that_widget = { 0,};
  GladeProject *project;

  gparent = glade_widget_get_parent (gwidget);
  project = glade_widget_get_project (gwidget);

  if (strcmp (action_path, "preview") == 0)
    {
      glade_project_preview (project,
                             glade_widget_get_from_gobject ((gpointer) object));
    }
  else if (strcmp (action_path, "edit_separate") == 0)
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
      const gchar *action = action_path + 11;
      GType new_type = 0;

      if (strcmp (action, "alignment") == 0)
        new_type = GTK_TYPE_ALIGNMENT;
      else if (strcmp (action, "viewport") == 0)
        new_type = GTK_TYPE_VIEWPORT;
      else if (strcmp (action, "eventbox") == 0)
        new_type = GTK_TYPE_EVENT_BOX;
      else if (strcmp (action, "frame") == 0)
        new_type = GTK_TYPE_FRAME;
      else if (strcmp (action, "aspect_frame") == 0)
        new_type = GTK_TYPE_ASPECT_FRAME;
      else if (strcmp (action, "scrolled_window") == 0)
        new_type = GTK_TYPE_SCROLLED_WINDOW;
      else if (strcmp (action, "expander") == 0)
        new_type = GTK_TYPE_EXPANDER;
      else if (strcmp (action, "grid") == 0)
        new_type = GTK_TYPE_GRID;
      else if (strcmp (action, "box") == 0)
        new_type = GTK_TYPE_BOX;
      else if (strcmp (action, "paned") == 0)
        new_type = GTK_TYPE_PANED;
      else if (strcmp (action, "stack") == 0)
        new_type = GTK_TYPE_STACK;

      if (new_type)
        {
          GladeWidgetAdaptor *adaptor =
            glade_widget_adaptor_get_by_type (new_type);
          GList *saved_props, *prop_cmds;
          GladeWidget *gnew_parent;
          GladeProperty *property;

          glade_command_push_group (_("Adding parent %s for %s"),
                                    glade_widget_adaptor_get_title (adaptor), 
                                    glade_widget_get_name (gwidget));

          /* Record packing properties */
          saved_props =
            glade_widget_dup_properties (gwidget, glade_widget_get_packing_properties (gwidget),
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

              /* Apply the properties in an undoable way */
              if (prop_cmds)
                glade_command_set_properties_list 
                  (glade_widget_get_project (gparent), prop_cmds);

              /* Add "this" widget to the new parent */
              glade_command_add (&this_widget, gnew_parent, NULL, project, FALSE);

              glade_command_pop_group ();
            }
          else
            {
              glade_command_pop_group ();

              /* Undo delete command
               * FIXME: this will leave the "Adding parent..." comand in the
               * redo list, which I think its better than leaving it in the
               * undo list by using glade_command_add() to add the widget back
               * to the original parent.
               * Ideally we need a way to remove a redo item from the project or
               * simply do not let the user cancel a widget creation!
               */
              glade_project_undo (project);
            }

          g_list_foreach (saved_props, (GFunc) g_object_unref, NULL);
          g_list_free (saved_props);
        }
    }
  else if (strcmp (action_path, "sizegroup_add") == 0)
    {
      /* Ignore dummy */
    }
  else if (strcmp (action_path, "clear_properties") == 0)
    {
      glade_editor_reset_dialog_run (gtk_widget_get_toplevel (GTK_WIDGET (object)), gwidget);
    }
  else if (strcmp (action_path, "read_documentation") == 0)
    {
      glade_app_search_docs (glade_widget_adaptor_get_book (adaptor),
                             glade_widget_adaptor_get_display_name (adaptor),
                             NULL);
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
      if (GTK_IS_SIZE_GROUP (glade_widget_get_object (iter)))
        groups = g_list_prepend (groups, iter);
    }
  return g_list_reverse (groups);
}

static void
glade_gtk_widget_add2group_cb (GtkMenuItem *item, GladeWidget *gwidget)
{
  GladeWidget *group =
      g_object_get_data (G_OBJECT (item), "glade-group-widget");
  GladeWidgetAdaptor *adaptor =
      glade_widget_adaptor_get_by_type (GTK_TYPE_SIZE_GROUP);
  GList *widget_list = NULL, *new_list;
  GladeProperty *property;
  const gchar *current_name;
  const gchar *size_group_name = NULL;
  gchar *widget_name;

  /* Display "(unnamed)" for unnamed size groups */
  if (group)
    {
      size_group_name = glade_widget_get_name (group);
      if (g_str_has_prefix (size_group_name, GLADE_UNNAMED_PREFIX))
        size_group_name = _("(unnamed)");
    }

  /* Ensure the widget has a name if it's going to be referred to by a size group */
  current_name = glade_widget_get_name (gwidget);
  if (g_str_has_prefix (current_name, GLADE_UNNAMED_PREFIX))
    widget_name = glade_project_new_widget_name (glade_widget_get_project (gwidget), NULL,
                                                 glade_widget_adaptor_get_generic_name (glade_widget_get_adaptor (gwidget)));
  else
    widget_name = g_strdup (current_name);

  if (group)
    glade_command_push_group (_("Adding %s to Size Group %s"),
                              widget_name,
                              size_group_name);
  else
    glade_command_push_group (_("Adding %s to a new Size Group"),
                              widget_name);

  glade_command_set_name (gwidget, widget_name);

  if (!group)
    /* Cant cancel a size group */
    group =
        glade_command_create (adaptor, NULL, NULL,
                              glade_widget_get_project (gwidget));

  property = glade_widget_get_property (group, "widgets");
  glade_property_get (property, &widget_list);
  new_list = g_list_copy (widget_list);
  if (!g_list_find (widget_list, glade_widget_get_object (gwidget)))
    new_list = g_list_append (new_list, glade_widget_get_object (gwidget));
  glade_command_set_property (property, new_list);

  g_list_free (new_list);
  g_free (widget_name);

  glade_command_pop_group ();
}


GtkWidget *
glade_gtk_widget_action_submenu (GladeWidgetAdaptor *adaptor,
                                 GObject            *object,
                                 const gchar        *action_path)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);
  GList *groups, *list;

  if (strcmp (action_path, "sizegroup_add") == 0)
    {
      GtkWidget *menu = gtk_menu_new ();
      GtkWidget *separator, *item;
      GladeWidget *group;
      const gchar *size_group_name;

      if ((groups = list_sizegroups (gwidget)) != NULL)
        {
          for (list = groups; list; list = list->next)
            {
              group = list->data;

              size_group_name = glade_widget_get_name (group);
              if (g_str_has_prefix (size_group_name, GLADE_UNNAMED_PREFIX))
                size_group_name = _("(unnamed)");

              item = gtk_menu_item_new_with_label (size_group_name);

              g_object_set_data (G_OBJECT (item), "glade-group-widget", group);
              g_signal_connect (G_OBJECT (item), "activate",
                                G_CALLBACK (glade_gtk_widget_add2group_cb),
                                gwidget);

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
                                                          object, action_path);

  return NULL;
}
