/*
 * glade-gtk-action-bar.c - GladeWidgetAdaptor for GtkActionBar widget
 *
 * Copyright (C) 2014 Red Hat, Inc.
 *
 * Authors:
 *      Matthias Clasen <mclasen@redhat.com>
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

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include <gladeui/glade-widget.h>
#include <gladeui/glade-utils.h>
#include "glade-action-bar-editor.h"
#include "glade-gtk.h"


GladeEditable *
glade_gtk_action_bar_create_editable (GladeWidgetAdaptor * adaptor,
                                      GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_action_bar_editor_new ();

  return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}

static void
glade_gtk_action_bar_parse_finished (GladeProject * project,
                                     GObject * object)
{
  GladeWidget *gbox;

  gbox = glade_widget_get_from_gobject (object);
  glade_widget_property_set (gbox, "use-center-child", gtk_action_bar_get_center_widget (GTK_ACTION_BAR (object)) != NULL);
}

void
glade_gtk_action_bar_post_create (GladeWidgetAdaptor * adaptor,
                                  GObject * container,
                                  GladeCreateReason reason)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (container);
  GladeProject *project = glade_widget_get_project (gwidget);

  if (reason == GLADE_CREATE_LOAD)
    g_signal_connect (project, "parse-finished",
                      G_CALLBACK (glade_gtk_action_bar_parse_finished),
                      container);
}

static gint
sort_children (GtkWidget * widget_a, GtkWidget * widget_b, GtkWidget *box)
{
  GladeWidget *gwidget_a, *gwidget_b;
  gint position_a, position_b;

  gwidget_a = glade_widget_get_from_gobject (widget_a);
  gwidget_b = glade_widget_get_from_gobject (widget_b);

  /* Sort internal children before any other children */
  if (box != gtk_widget_get_parent (widget_a))
    return -1;
  if (box != gtk_widget_get_parent (widget_b))
    return 1;

  if (gtk_action_bar_get_center_widget (GTK_ACTION_BAR (box)) == widget_a)
    return -1;
  if (gtk_action_bar_get_center_widget (GTK_ACTION_BAR (box)) == widget_b)
    return -1;

  if (gwidget_a)
    glade_widget_pack_property_get (gwidget_a, "position", &position_a);
  else
    gtk_container_child_get (GTK_CONTAINER (box),
                             widget_a, "position", &position_a, NULL);

  if (gwidget_b)
    glade_widget_pack_property_get (gwidget_b, "position", &position_b);
  else
    gtk_container_child_get (GTK_CONTAINER (box),
                             widget_b, "position", &position_b, NULL);

  return position_a - position_b;
}

GList *
glade_gtk_action_bar_get_children (GladeWidgetAdaptor * adaptor,
                                   GObject * container)
{
  GList *children;

  children = GWA_GET_CLASS (GTK_TYPE_CONTAINER)->get_children (adaptor, container);

  return g_list_sort_with_data (children, (GCompareDataFunc) sort_children, container);
}

static void
update_position (GtkWidget *widget, gpointer data)
{
  GtkContainer *parent = data;
  GladeWidget *gwidget;
  gint position;

  gwidget = glade_widget_get_from_gobject (widget);
  if (gwidget)
    {
      gtk_container_child_get (parent, widget, "position", &position, NULL);
      glade_widget_pack_property_set (gwidget, "position", position);
    }
}

static void
glade_gtk_action_bar_set_child_position (GObject * container,
                                         GObject * child,
                                         GValue * value)
{
  static gboolean recursion = FALSE;
  gint new_position, old_position;

  if (recursion)
    return;

  gtk_container_child_get (GTK_CONTAINER (container), GTK_WIDGET (child), "position", &old_position, NULL);
  new_position = g_value_get_int (value);

  if (old_position == new_position)
    return;

  recursion = TRUE;
  gtk_container_child_set (GTK_CONTAINER (container), GTK_WIDGET (child),
                           "position", new_position,
                           NULL);
  gtk_container_forall (GTK_CONTAINER (container), update_position, container);
  recursion = FALSE;
}

static void
glade_gtk_action_bar_set_child_pack_type (GObject * container,
                                          GObject * child,
                                          GValue * value)
{
  GtkPackType pack_type;

  pack_type = g_value_get_enum (value);

  gtk_container_child_set (GTK_CONTAINER (container), GTK_WIDGET (child), "pack-type", pack_type, NULL);
}

void
glade_gtk_action_bar_set_child_property (GladeWidgetAdaptor * adaptor,
                                         GObject * container,
                                         GObject * child,
                                         const gchar * id,
                                         GValue * value)
{
  if (!strcmp (id, "position"))
    glade_gtk_action_bar_set_child_position (container, child, value);
  else if (!strcmp (id, "pack-type"))
    glade_gtk_action_bar_set_child_pack_type (container, child, value);
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_set_property (adaptor, container,
                                                            child, id, value);

  gtk_container_check_resize (GTK_CONTAINER (container));
}

static gint
glade_gtk_action_bar_get_num_children (GObject *box)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (box));
  gint retval = g_list_length (children);
  if (gtk_action_bar_get_center_widget (GTK_ACTION_BAR (box)) != NULL)
    retval -= 1;
  g_list_free (children);
  return retval;
}

void
glade_gtk_action_bar_get_property (GladeWidgetAdaptor * adaptor,
                                   GObject * object,
                                   const gchar * id,
                                   GValue * value)
{
  if (!strcmp (id, "use-center-child"))
    {
      g_value_reset (value);
      g_value_set_boolean (value, gtk_action_bar_get_center_widget (GTK_ACTION_BAR (object)) != NULL);
    }
  else if (!strcmp (id, "size"))
    {
      g_value_reset (value);
      g_value_set_int (value, glade_gtk_action_bar_get_num_children (object));
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->get_property (adaptor, object, id, value);
}

static gint
glade_gtk_action_bar_get_first_blank (GtkActionBar * box)
{
  GList *child, *children;
  GladeWidget *gwidget;
  gint position;

  children = gtk_container_get_children (GTK_CONTAINER (box));

  for (child = children, position = 0;
       child && child->data; child = child->next, position++)
    {
      GtkWidget *widget = child->data;

      if (widget == gtk_action_bar_get_center_widget (GTK_ACTION_BAR (box)))
        continue;

      if ((gwidget = glade_widget_get_from_gobject (widget)) != NULL)
        {
          gint gwidget_position = 0;
          GladeProperty *property =
              glade_widget_get_pack_property (gwidget, "position");

          /* property can be NULL here when project is closing */
          if (property)
            gwidget_position = g_value_get_int (glade_property_inline_value (property));

          if (gwidget_position > position)
            break;
        }
    }

  g_list_free (children);

  return position;
}

static void
glade_gtk_action_bar_set_size (GObject * object, const GValue * value)
{
  GtkActionBar *box;
  GList *child, *children;
  guint new_size, old_size, i;

  box = GTK_ACTION_BAR (object);

  if (glade_util_object_is_loading (object))
    return;

  children = gtk_container_get_children (GTK_CONTAINER (box));
  children = g_list_remove (children, gtk_action_bar_get_center_widget (box));

  old_size = g_list_length (children);
  new_size = g_value_get_int (value);

  if (old_size == new_size)
    {
      g_list_free (children);
      return;
    }

  /* Ensure placeholders first...
   */
  for (i = 0; i < new_size; i++)
    {
      if (g_list_length (children) < (i + 1))
        {
          GtkWidget *placeholder = glade_placeholder_new ();
          gint blank = glade_gtk_action_bar_get_first_blank (box);

          gtk_container_add (GTK_CONTAINER (box), placeholder);
          gtk_container_child_set (GTK_CONTAINER (box), placeholder, "position", blank, NULL);
        }
    }

  /* The box has shrunk. Remove the widgets that are on those slots */
  for (child = g_list_last (children);
       child && old_size > new_size; child = g_list_previous (child))
    {
      GtkWidget *child_widget = child->data;

      /* Refuse to remove any widgets that are either GladeWidget objects
       * or internal to the hierarchic entity (may be a composite widget,
       * not all internal widgets have GladeWidgets).
       */
      if (glade_widget_get_from_gobject (child_widget) ||
          GLADE_IS_PLACEHOLDER (child_widget) == FALSE)
        continue;

      gtk_container_remove (GTK_CONTAINER (box), child_widget);
      old_size--;
    }
  g_list_free (children);
}

void
glade_gtk_action_bar_set_property (GladeWidgetAdaptor * adaptor,
                                   GObject * object,
                                   const gchar * id,
                                   const GValue * value)
{
  if (!strcmp (id, "use-center-child"))
    {
      GtkWidget *child;

      if (g_value_get_boolean (value))
        {
          child = gtk_action_bar_get_center_widget (GTK_ACTION_BAR (object));
          if (!child)
            child = glade_placeholder_new ();
          g_object_set_data (G_OBJECT (child), "special-child-type", "center");
        }
      else
        child = NULL;
      gtk_action_bar_set_center_widget (GTK_ACTION_BAR (object), child);
    }

  else if (!strcmp (id, "size"))
    glade_gtk_action_bar_set_size (object, value);
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id, value);
}

static gboolean
glade_gtk_action_bar_verify_size (GObject *object, const GValue *value)
{
  GList *child, *children;
  gint old_size, count = 0;
  gint new_size = g_value_get_int (value);
  
  children = gtk_container_get_children (GTK_CONTAINER (object));
  children = g_list_remove (children, gtk_action_bar_get_center_widget (GTK_ACTION_BAR (object)));
  old_size = g_list_length (children);

  for (child = g_list_last (children);
       child && old_size > new_size;
       child = g_list_previous (child))
    {
      GtkWidget *widget = child->data;
      if (glade_widget_get_from_gobject (widget) != NULL)
        count++;
      else
        old_size--;
    }

  g_list_free (children);

  return count > new_size ? FALSE : new_size >= 0;
}

gboolean
glade_gtk_action_bar_verify_property (GladeWidgetAdaptor * adaptor,
                                      GObject * object,
                                      const gchar * id,
                                      const GValue * value)
{
  if (!strcmp (id, "size"))
    return glade_gtk_action_bar_verify_size (object, value);
  else if (GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property)
    return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property (adaptor, object, id, value);

  return TRUE;
}

void
glade_gtk_action_bar_add_child (GladeWidgetAdaptor * adaptor,
                                GObject * object,
                                GObject * child)
{
  GladeWidget *gbox, *gchild;
  gint num_children;
  gchar *special_child_type;

  gbox = glade_widget_get_from_gobject (object);

  special_child_type = g_object_get_data (child, "special-child-type");
  if (special_child_type && !strcmp (special_child_type, "center"))
    {
      gtk_action_bar_set_center_widget (GTK_ACTION_BAR (object), GTK_WIDGET (child));
       return;
    }

  /*
     Try to remove the last placeholder if any, this way GtkBox`s size 
     will not be changed.
   */
  if (!glade_widget_superuser () && !GLADE_IS_PLACEHOLDER (child))
    {
      GList *l, *children;

      children = gtk_container_get_children (GTK_CONTAINER (object));
      for (l = g_list_last (children); l; l = g_list_previous (l))
        {
          GtkWidget *child_widget = l->data;
          if (GLADE_IS_PLACEHOLDER (child_widget))
            {
              gtk_container_remove (GTK_CONTAINER (object), child_widget);
              break;
            }
        }
      g_list_free (children);
    }

  gtk_container_add (GTK_CONTAINER (object), GTK_WIDGET (child));
  num_children = glade_gtk_action_bar_get_num_children (object);
  glade_widget_property_set (gbox, "size", num_children);

  if (glade_widget_superuser ())
    return;
  
  gchild = glade_widget_get_from_gobject (child);

  /* Packing props arent around when parenting during a glade_widget_dup() */
  if (gchild && glade_widget_get_packing_properties (gchild))
    glade_widget_pack_property_set (gchild, "position", num_children - 1);
}

void
glade_gtk_action_bar_remove_child (GladeWidgetAdaptor * adaptor,
                                   GObject * object,
                                   GObject * child)
{
  GladeWidget *gbox;
  gint size;
  gchar *special_child_type;

  gbox = glade_widget_get_from_gobject (object);

  special_child_type = g_object_get_data (child, "special-child-type");
  if (special_child_type && !strcmp (special_child_type, "center"))
    {
      GtkWidget *w;

      w = glade_placeholder_new ();
      g_object_set_data (G_OBJECT (w), "special-child-type", "center");
      gtk_action_bar_set_center_widget (GTK_ACTION_BAR (object), w);
      return;
    }

  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));

  if (!glade_widget_superuser ())
    {
      glade_widget_property_get (gbox, "size", &size);
      glade_widget_property_set (gbox, "size", size);
    }
}

void
glade_gtk_action_bar_replace_child (GladeWidgetAdaptor * adaptor,
                                    GObject * container,
                                    GObject * current,
                                    GObject * new_widget)
{
  gint position;
  GtkPackType pack_type;

  gchar *special_child_type;

  special_child_type =
      g_object_get_data (G_OBJECT (current), "special-child-type");

  if (special_child_type && !strcmp (special_child_type, "center"))
    {
      g_object_set_data (G_OBJECT (new_widget), "special-child-type", "center");
      gtk_action_bar_set_center_widget (GTK_ACTION_BAR (container), GTK_WIDGET (new_widget));
      return;
    }

  gtk_container_child_get (GTK_CONTAINER (container),
                           GTK_WIDGET (current),
                           "position", &position,
                           "pack-type", &pack_type,
                           NULL); 

  gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET (current));
  gtk_container_add (GTK_CONTAINER (container), GTK_WIDGET (new_widget));

  gtk_container_child_set (GTK_CONTAINER (container),
                           GTK_WIDGET (new_widget),
                           "position", position,
                           "pack-type", pack_type,
                           NULL);
}
