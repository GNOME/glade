/*
 * glade-object-stub.c
 *
 * Copyright (C) 2011 Juan Pablo Ugarte
   *
 * Author: Juan Pablo Ugarte <juanpablougarte@gmail.com>
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
 *
 */

#include <config.h>

#include <glib/gi18n-lib.h>
#include "glade-object-stub.h"
#include "glade-project.h"

struct _GladeObjectStub
{
  GtkInfoBar parent_instance;

  GtkLabel *label;

  gchar *type;
  GladeXmlNode *node;
};

enum
{
  PROP_0,

  PROP_OBJECT_TYPE,
  PROP_XML_NODE
};


G_DEFINE_TYPE (GladeObjectStub, glade_object_stub, GTK_TYPE_INFO_BAR);

#define RESPONSE_DELETE 1
#define RESPONSE_DELETE_ALL 2

static void
on_infobar_response (GladeObjectStub *stub, gint response_id)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (stub);
  
  if (response_id == RESPONSE_DELETE)
    {
      GList widgets = {0, };
      widgets.data = gwidget;
      glade_command_delete (&widgets);
    }
  else if (response_id == RESPONSE_DELETE_ALL)
    {
      GladeProject *project = glade_widget_get_project (gwidget);
      GList *stubs = NULL;
      const GList *l;

      for (l = glade_project_get_objects (project); l; l = g_list_next (l))
        {
          if (GLADE_IS_OBJECT_STUB (l->data))
            {
              GladeWidget *gobj = glade_widget_get_from_gobject (l->data);
              stubs = g_list_prepend (stubs, gobj);
            }
        }

      glade_command_delete (stubs);
      g_list_free (stubs);
    }
}

static void
glade_object_stub_init (GladeObjectStub *self)
{
  GtkWidget *label = gtk_label_new (NULL);

  self->type = NULL;
  self->node = NULL;
  
  self->label = GTK_LABEL (label);
  gtk_label_set_line_wrap (self->label, TRUE);
  
  gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (self))), label);

  gtk_info_bar_add_button (GTK_INFO_BAR (self),
                           _("Delete"), RESPONSE_DELETE);
  gtk_info_bar_add_button (GTK_INFO_BAR (self),
                           _("Delete All"), RESPONSE_DELETE_ALL);
  
  g_signal_connect (self, "response", G_CALLBACK (on_infobar_response), NULL);
}

static void
glade_object_stub_finalize (GObject *object)
{
  GladeObjectStub *self = (GladeObjectStub *) object;

  g_clear_pointer (&self->type, g_free);
  g_clear_pointer (&self->node, glade_xml_node_delete);

  G_OBJECT_CLASS (glade_object_stub_parent_class)->finalize (object);
}

static void
glade_object_stub_refresh_text (GladeObjectStub *self)
{
  gchar *markup;
  GType type;

  if (self->type == NULL) return;

  type = g_type_from_name (self->type);

  if ((type != G_TYPE_INVALID && (!G_TYPE_IS_INSTANTIATABLE (type) || G_TYPE_IS_ABSTRACT (type))))
    markup = g_markup_printf_escaped ("<b>FIXME:</b> Unable to create uninstantiable object with type %s", self->type);
  else
    markup = g_markup_printf_escaped ("<b>FIXME:</b> Unable to create object with type %s", self->type);
  
  gtk_label_set_markup (self->label, markup);
  gtk_info_bar_set_message_type (GTK_INFO_BAR (self), GTK_MESSAGE_WARNING);
  g_free (markup);
}

static void
glade_object_stub_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GladeObjectStub *self = (GladeObjectStub *) object;

  g_return_if_fail (GLADE_IS_OBJECT_STUB (object));

  switch (prop_id)
    {
      case PROP_OBJECT_TYPE:
        g_free (self->type);
        self->type = g_value_dup_string (value);
        glade_object_stub_refresh_text (self);
        break;
      case PROP_XML_NODE:
        if (self->node) glade_xml_node_delete (self->node);
        self->node = g_value_dup_boxed (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_object_stub_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GladeObjectStub *self = (GladeObjectStub *) object;
  
  g_return_if_fail (GLADE_IS_OBJECT_STUB (object));

  switch (prop_id)
    {
      case PROP_OBJECT_TYPE:
        g_value_set_string (value, self->type);
        break;
      case PROP_XML_NODE:
        g_value_set_boxed (value, self->node);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
glade_object_stub_class_init (GladeObjectStubClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = glade_object_stub_finalize;
  object_class->set_property = glade_object_stub_set_property;
  object_class->get_property = glade_object_stub_get_property;

  g_object_class_install_property (object_class,
                                   PROP_OBJECT_TYPE,
                                   g_param_spec_string ("object-type",
                                                        "Object Type",
                                                        "The object type this stub replaces",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_XML_NODE,
                                   g_param_spec_boxed ("xml-node",
                                                       "XML node",
                                                       "The XML representation of the original object this is replacing",
                                                       glade_xml_node_get_type (),
                                                       G_PARAM_READWRITE));
}
