/*
 * Copyright (C) 2013 Tristan Van Berkom.
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
 * Authors:
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include "glade.h"
#include "glade-widget.h"
#include "glade-popup.h"
#include "glade-editable.h"
#include "glade-editor-skeleton.h"

/* GObjectClass */
static void      glade_editor_skeleton_dispose        (GObject *object);

/* GladeEditableInterface */
static void      glade_editor_skeleton_editable_init   (GladeEditableInterface *iface);

/* GtkBuildableIface */
static void      glade_editor_skeleton_buildable_init  (GtkBuildableIface *iface);

typedef struct _GladeEditorSkeletonPrivate GladeEditorSkeletonPrivate;

struct _GladeEditorSkeletonPrivate
{
  GSList *editors;
};

static GladeEditableInterface *parent_editable_iface;
static GtkBuildableIface  *parent_buildable_iface;

G_DEFINE_TYPE_WITH_CODE (GladeEditorSkeleton, glade_editor_skeleton, GTK_TYPE_BOX,
                         G_ADD_PRIVATE (GladeEditorSkeleton)
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_editor_skeleton_editable_init)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE,
                                                glade_editor_skeleton_buildable_init));

static void
glade_editor_skeleton_init (GladeEditorSkeleton *skeleton)
{
  
}

static void
glade_editor_skeleton_class_init (GladeEditorSkeletonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gobject_class->dispose = glade_editor_skeleton_dispose;
}

/***********************************************************
 *                     GObjectClass                        *
 ***********************************************************/
static void
glade_editor_skeleton_dispose (GObject *object)
{
  GladeEditorSkeleton *skeleton = GLADE_EDITOR_SKELETON (object);
  GladeEditorSkeletonPrivate *priv = glade_editor_skeleton_get_instance_private (skeleton);

  if (priv->editors)
    {
      g_slist_free_full (priv->editors, (GDestroyNotify)g_object_unref);
      priv->editors = NULL;
    }

  G_OBJECT_CLASS (glade_editor_skeleton_parent_class)->dispose (object);
}

/*******************************************************************************
 *                          GladeEditableInterface                             *
 *******************************************************************************/
static void
glade_editor_skeleton_load (GladeEditable *editable,
                            GladeWidget   *widget)
{
  GladeEditorSkeleton *skeleton = GLADE_EDITOR_SKELETON (editable);
  GladeEditorSkeletonPrivate *priv = glade_editor_skeleton_get_instance_private (skeleton);
  GSList *l;

  /* Chain up to default implementation */
  parent_editable_iface->load (editable, widget);

  for (l = priv->editors; l; l = l->next)
    {
      GladeEditable *editor = l->data;

      glade_editable_load (editor, widget);
    }
}

static void
glade_editor_skeleton_set_show_name (GladeEditable *editable, gboolean show_name)
{
  GladeEditorSkeleton *skeleton = GLADE_EDITOR_SKELETON (editable);
  GladeEditorSkeletonPrivate *priv = glade_editor_skeleton_get_instance_private (skeleton);
  GSList *l;

  for (l = priv->editors; l; l = l->next)
    {
      GladeEditable *editor = l->data;

      glade_editable_set_show_name (editor, show_name);
    }
}

static void
glade_editor_skeleton_editable_init (GladeEditableInterface *iface)
{
  parent_editable_iface = g_type_default_interface_peek (GLADE_TYPE_EDITABLE);

  iface->load = glade_editor_skeleton_load;
  iface->set_show_name = glade_editor_skeleton_set_show_name;
}

/*******************************************************************************
 *                            GtkBuildableIface                                *                               
 *******************************************************************************/
typedef struct
{
  GSList *editors;
} EditorParserData;

static void
editor_start_element (GMarkupParseContext  *context,
                      const gchar          *element_name,
                      const gchar         **names,
                      const gchar         **values,
                      gpointer              user_data,
                      GError              **error)
{
  EditorParserData *editor_data = (EditorParserData *)user_data;
  gchar *id;

  if (strcmp (element_name, "editor") == 0)
    {
      if (g_markup_collect_attributes (element_name,
                                       names,
                                       values,
                                       error,
                                       G_MARKUP_COLLECT_STRDUP, "id", &id,
                                       G_MARKUP_COLLECT_INVALID))
        {
          editor_data->editors = g_slist_append (editor_data->editors, id);
        }
    }
  else if (strcmp (element_name, "child-editors") == 0)
    ;
  else
    g_warning ("Unsupported tag for GladeEditorSkeleton: %s\n", element_name);
}

static const GMarkupParser editor_parser =
  {
    editor_start_element,
  };

static gboolean
glade_editor_skeleton_custom_tag_start (GtkBuildable  *buildable,
                                        GtkBuilder    *builder,
                                        GObject       *child,
                                        const gchar   *tagname,
                                        GMarkupParser *parser,
                                        gpointer      *data)
{
  if (strcmp (tagname, "child-editors") == 0)
    {
      EditorParserData *parser_data;

      parser_data = g_slice_new0 (EditorParserData);
      *parser = editor_parser;
      *data = parser_data;
      return TRUE;
    }

  return parent_buildable_iface->custom_tag_start (buildable, builder, child,
                                                   tagname, parser, data);
}

static void
glade_editor_skeleton_custom_finished (GtkBuildable *buildable,
                                       GtkBuilder   *builder,
                                       GObject      *child,
                                       const gchar  *tagname,
                                       gpointer      user_data)
{
  EditorParserData *editor_data = (EditorParserData *)user_data;
  GSList *l;

  if (strcmp (tagname, "child-editors") != 0)
    {
      parent_buildable_iface->custom_finished (buildable, builder, child,
                                               tagname, user_data);
      return;
    }

  for (l = editor_data->editors; l; l = l->next)
    {
      GObject *object;
      gchar *id = l->data;

      object = gtk_builder_get_object (builder, id);

      if (!GLADE_EDITABLE (object))
        g_warning ("Object '%s' is not a GladeEditable\n",
                   object ? G_OBJECT_TYPE_NAME (object) : "(null)");
      else
        glade_editor_skeleton_add_editor (GLADE_EDITOR_SKELETON (buildable),
                                          GLADE_EDITABLE (object));
    }

  g_slist_free_full (editor_data->editors, g_free);
  g_slice_free (EditorParserData, editor_data);
}

static void
glade_editor_skeleton_buildable_init (GtkBuildableIface *iface)
{
  parent_buildable_iface = g_type_interface_peek_parent (iface);
  iface->custom_tag_start = glade_editor_skeleton_custom_tag_start;
  iface->custom_finished = glade_editor_skeleton_custom_finished;
}

/*******************************************************************************
 *                                   API                                       *                               
 *******************************************************************************/
GtkWidget *
glade_editor_skeleton_new (void)
{
  return g_object_new (GLADE_TYPE_EDITOR_SKELETON, NULL);
}

void
glade_editor_skeleton_add_editor (GladeEditorSkeleton *skeleton,
                                  GladeEditable       *editor)
{
  GladeEditorSkeletonPrivate *priv = glade_editor_skeleton_get_instance_private (skeleton);

  g_return_if_fail (GLADE_IS_EDITOR_SKELETON (skeleton));
  g_return_if_fail (GLADE_IS_EDITABLE (editor));

  g_object_ref (editor);
  priv->editors = g_slist_prepend (priv->editors, editor);
}
