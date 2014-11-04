#include <config.h>
#include <glib/gi18n-lib.h>
#include <gladeui/glade.h>
#include "glade-gtk.h"

#include "glade-header-bar-editor.h"

static gint
glade_gtk_header_bar_get_num_children (GObject *hb, GtkPackType type)
{
  GList *children, *l;
  GtkPackType pt;
  gint num;
  GtkWidget *custom_title;

  custom_title = gtk_header_bar_get_custom_title (GTK_HEADER_BAR (hb));

  num = 0;

  children = gtk_container_get_children (GTK_CONTAINER (hb));
  for (l = children; l; l = l->next)
    {
      if (l->data == custom_title)
        continue;
      gtk_container_child_get (GTK_CONTAINER (hb), GTK_WIDGET (l->data), "pack-type", &pt, NULL);
      if (type == pt)
        num++;
    }
  g_list_free (children);

  return num;
}

static void
glade_gtk_header_bar_parse_finished (GladeProject * project,
                                     GObject * object)
{
  GladeWidget *gbox;

  gbox = glade_widget_get_from_gobject (object);
  glade_widget_property_set (gbox, "start-size", glade_gtk_header_bar_get_num_children (object, GTK_PACK_START));
  glade_widget_property_set (gbox, "end-size", glade_gtk_header_bar_get_num_children (object, GTK_PACK_END));
  glade_widget_property_set (gbox, "use-custom-title", gtk_header_bar_get_custom_title (GTK_HEADER_BAR (object)) != NULL);
}

void
glade_gtk_header_bar_post_create (GladeWidgetAdaptor *adaptor,
                                  GObject *container,
                                  GladeCreateReason reason)
{
  GladeWidget *parent = glade_widget_get_from_gobject (container);
  GladeProject *project = glade_widget_get_project (parent);

  if (reason == GLADE_CREATE_LOAD)
    {
      g_signal_connect (project, "parse-finished",
                        G_CALLBACK (glade_gtk_header_bar_parse_finished),
                        container);
    }
  else if (reason == GLADE_CREATE_USER)
    {
      gtk_header_bar_pack_start (GTK_HEADER_BAR (container), glade_placeholder_new ());
      gtk_header_bar_pack_end (GTK_HEADER_BAR (container), glade_placeholder_new ());
    }
}

void
glade_gtk_header_bar_add_child (GladeWidgetAdaptor *adaptor,
                                GObject *parent,
                                GObject *child)
{
  GladeWidget *gbox, *gchild;
  GtkPackType pack_type;
  gint num_children;
  gchar *special_child_type;
  special_child_type = g_object_get_data (child, "special-child-type");
  if (special_child_type && !strcmp (special_child_type, "title"))
    {
      gtk_header_bar_set_custom_title (GTK_HEADER_BAR (parent), GTK_WIDGET (child));
      return;
    }

    // check if its a placeholder being added, and add it to the end if anything is at the start
    if (GLADE_IS_PLACEHOLDER (child))
      {
        GList *list = gtk_container_get_children (GTK_CONTAINER (parent));
        GList *l;
        if (list)
          {
            for (l = list; l; l = l->next)
              {
                GObject *c = l->data;
                GtkPackType t;
                gtk_container_child_get (GTK_CONTAINER (parent), GTK_WIDGET (c), "pack-type", &t, NULL);
                if (t == GTK_PACK_START)
                  {
                    gtk_header_bar_pack_end (GTK_HEADER_BAR (parent), GTK_WIDGET (child) );
                    g_list_free (list);
                    return;
                  }
              }
            g_list_free (list);
          }
      }

    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->add (adaptor, parent, child);
  
  gbox = glade_widget_get_from_gobject (parent);

  gtk_container_child_get (GTK_CONTAINER (parent), GTK_WIDGET (child), "pack-type", &pack_type, NULL);
  num_children = glade_gtk_header_bar_get_num_children (parent, pack_type);
  if (pack_type == GTK_PACK_START)
    glade_widget_property_set (gbox, "start-size", num_children);
  else
    glade_widget_property_set (gbox, "end-size", num_children);

  gchild = glade_widget_get_from_gobject (child);
  if (gchild)
    glade_widget_set_pack_action_visible (gchild, "remove_slot", FALSE);

}

void
glade_gtk_header_bar_child_insert_remove_action (GladeWidgetAdaptor * adaptor,
                                                 GObject * container,
                                                 GObject * object,
                                                 const gchar * group_format,
                                                 gboolean remove,
                                                 GtkPackType pack_type)
{
  GladeWidget *parent;

  parent = glade_widget_get_from_gobject (container);
  glade_command_push_group (group_format, glade_widget_get_name (parent));

  if (remove)
    gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET (object));
  else if (pack_type == GTK_PACK_START)
    gtk_header_bar_pack_start (GTK_HEADER_BAR (container), glade_placeholder_new ());
  else
    gtk_header_bar_pack_end (GTK_HEADER_BAR (container), glade_placeholder_new ());

  glade_command_pop_group ();
}

void
glade_gtk_header_bar_child_action_activate (GladeWidgetAdaptor * adaptor,
                                            GObject * container,
                                            GObject * object,
                                            const gchar * action_path)
{
  if (strcmp (action_path, "add_start") == 0)
    {
      glade_gtk_header_bar_child_insert_remove_action (adaptor, container, object,
                                                       _("Insert placeholder to %s"),
                                                        FALSE, GTK_PACK_START);
    }
  else if (strcmp (action_path, "add_end") == 0)
    {
      glade_gtk_header_bar_child_insert_remove_action (adaptor, container, object,
                                                       _("Insert placeholder to %s"),
                                                        FALSE, GTK_PACK_END);
    }
  else if (strcmp (action_path, "remove_slot") == 0)
    {
      glade_gtk_header_bar_child_insert_remove_action (adaptor, container, object,
                                                       _("Remove placeholder from %s"),
                                                       TRUE, GTK_PACK_START);
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->child_action_activate (adaptor,
                                                               container,
                                                               object,
                                                               action_path);
}

void
glade_gtk_header_bar_get_property (GladeWidgetAdaptor * adaptor,
                                   GObject * object,
                                   const gchar * id,
                                   GValue * value)
{
  if (!strcmp (id, "use-custom-title"))
    {
      g_value_reset (value);
      g_value_set_boolean (value, gtk_header_bar_get_custom_title (GTK_HEADER_BAR (object)) != NULL);
    }
  else if (!strcmp (id, "start-size"))
    {
      g_value_reset (value);
       g_value_set_int (value, glade_gtk_header_bar_get_num_children (object, GTK_PACK_START));
    }
  else if (!strcmp (id, "end-size"))
    {
      g_value_reset (value);
      g_value_set_int (value, glade_gtk_header_bar_get_num_children (object, GTK_PACK_END));
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->get_property (adaptor, object, id, value);
}

static void
glade_gtk_header_bar_set_size (GObject * object,
                               const GValue * value,
                               GtkPackType type)
{
  GList *l, *next, *children;
  GtkWidget *child;
  guint new_size, old_size, i;
  GtkPackType pt;

  g_return_if_fail (GTK_IS_HEADER_BAR (object));

  if (glade_util_object_is_loading (object))
    return;

  children = gtk_container_get_children (GTK_CONTAINER (object));
  l = children;
  while (l)
    {
      next = l->next;
      gtk_container_child_get (GTK_CONTAINER (object), GTK_WIDGET (l->data), "pack-type", &pt, NULL);
      if (type != pt || l->data == gtk_header_bar_get_custom_title (GTK_HEADER_BAR (object)))
        children = g_list_delete_link (children, l);
      l = next;
    }
 
  old_size = g_list_length (children);
  new_size = g_value_get_int (value);

  if (old_size == new_size)
    {
      g_list_free (children);
      return;
    }

  for (i = 0; i < new_size; i++)
    {
      if (g_list_length (children) < i + 1)
        {
          GtkWidget *placeholder = glade_placeholder_new ();
          if (type == GTK_PACK_START)
            gtk_header_bar_pack_start (GTK_HEADER_BAR (object), placeholder);
          else
            gtk_header_bar_pack_end (GTK_HEADER_BAR (object), placeholder);
        }
    }
  for (l = g_list_last (children); l && old_size > new_size; l = l->prev)
    {
      child = l->data;
      if (glade_widget_get_from_gobject (child) ||
          !GLADE_IS_PLACEHOLDER (child))
        continue;

      g_object_ref (G_OBJECT (child));
      gtk_container_remove (GTK_CONTAINER (object), child);
      gtk_widget_destroy (child);
      old_size--;
    }

  g_list_free (children);
}

void
glade_gtk_header_bar_set_property (GladeWidgetAdaptor * adaptor,
                                   GObject * object,
                                   const gchar * id,
                                   const GValue * value)
{
  if (!strcmp (id, "use-custom-title"))
    {
      GtkWidget *child;

      if (g_value_get_boolean (value))
        {
          child = gtk_header_bar_get_custom_title (GTK_HEADER_BAR (object));
          if (!child)
            child = glade_placeholder_new ();
          g_object_set_data (G_OBJECT (child), "special-child-type", "title");
        }
      else
        child = NULL;

      gtk_header_bar_set_custom_title (GTK_HEADER_BAR (object), child);
    }
  else if (!strcmp (id, "start-size"))
    glade_gtk_header_bar_set_size (object, value, GTK_PACK_START);
  else if (!strcmp (id, "end-size"))
    glade_gtk_header_bar_set_size (object, value, GTK_PACK_END);
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id, value);
}

void
glade_gtk_header_bar_remove_child (GladeWidgetAdaptor * adaptor,
                                   GObject * object,
                                   GObject * child)
{
  GladeWidget *gbox;
  gint num_children;
  GtkPackType pack_type;
  gchar *special_child_type;

  special_child_type = g_object_get_data (child, "special-child-type");
  if (special_child_type && !strcmp (special_child_type, "title"))
    {
      gtk_header_bar_set_custom_title (GTK_HEADER_BAR (object), glade_placeholder_new ());
      return;
    }
  
  gbox = glade_widget_get_from_gobject (object);

  gtk_container_child_get (GTK_CONTAINER (object), GTK_WIDGET (child), "pack-type", &pack_type, NULL);

  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));

  if (!glade_widget_superuser ())
    {
      num_children = glade_gtk_header_bar_get_num_children (object, pack_type);
      glade_widget_property_set (gbox, "start-size", num_children);
    }
}

void
glade_gtk_header_bar_replace_child (GladeWidgetAdaptor * adaptor,
                                    GObject * container,
                                    GObject * current,
                                    GObject * new_widget)
{
  gchar *special_child_type;

  special_child_type =
    g_object_get_data (G_OBJECT (current), "special-child-type");

  if (special_child_type && !strcmp (special_child_type, "title"))
    {
      g_object_set_data (G_OBJECT (new_widget), "special-child-type", "title");
      gtk_header_bar_set_custom_title (GTK_HEADER_BAR (container), GTK_WIDGET (new_widget));
      return;
    }

  GWA_GET_CLASS
      (GTK_TYPE_CONTAINER)->replace_child (adaptor, container, current, new_widget);
}

GladeEditable *
glade_gtk_header_bar_create_editable (GladeWidgetAdaptor * adaptor,
                                      GladeEditorPageType  type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_header_bar_editor_new ();
  else
    return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}
