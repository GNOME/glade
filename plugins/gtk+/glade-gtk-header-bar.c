#include <config.h>
#include <glib/gi18n-lib.h>
#include <gladeui/glade.h>
#include "glade-gtk.h"

#include "glade-header-bar-editor.h"

#define TITLE_DISABLED_MESSAGE _("This property does not apply when a custom title is set")

/* Uncomment to enable debug tracing of add/remove/replace children */
//#define d(x) x
#define d(x)

typedef struct
{
  GtkContainer *parent;
  GtkWidget *custom_title;
  gboolean include_placeholders;
  gint count;
} ChildrenData;

static void
count_children (GtkWidget *widget, gpointer data)
{
  ChildrenData *cdata = data;

  if (widget == cdata->custom_title)
    return;

  if ((GLADE_IS_PLACEHOLDER (widget) && cdata->include_placeholders) ||
      glade_widget_get_from_gobject (widget) != NULL)
    cdata->count++;
}

static gboolean
glade_gtk_header_bar_verify_size (GObject      *object,
                                  const GValue *value)
{
  gint new_size;
  ChildrenData data;

  new_size = g_value_get_int (value);

  data.parent = GTK_CONTAINER (object);
  data.custom_title = gtk_header_bar_get_custom_title (GTK_HEADER_BAR (object));
  data.include_placeholders = FALSE;
  data.count = 0;

  gtk_container_forall (data.parent, count_children, &data);

  return data.count <= new_size;
}

static gint
glade_gtk_header_bar_get_num_children (GObject *object)
{
  ChildrenData data;

  data.parent = GTK_CONTAINER (object);
  data.custom_title = gtk_header_bar_get_custom_title (GTK_HEADER_BAR (object));
  data.include_placeholders = TRUE;
  data.count = 0;

  gtk_container_forall (data.parent, count_children, &data);

  return data.count;
}

static void
glade_gtk_header_bar_parse_finished (GladeProject * project,
                                     GObject * object)
{
  GladeWidget *gbox;

  gbox = glade_widget_get_from_gobject (object);
  glade_widget_property_set (gbox, "size", glade_gtk_header_bar_get_num_children (object));
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
    }
}

void
glade_gtk_header_bar_action_activate (GladeWidgetAdaptor *adaptor,
                                      GObject * object,
                                      const gchar *action_path)
{
  if (!strcmp (action_path, "add_slot"))
    {
      GladeWidget *parent;
      GladeProperty *property;
      gint size;

      parent = glade_widget_get_from_gobject (object);

      glade_command_push_group (_("Insert placeholer to %s"), glade_widget_get_name (parent));

      property = glade_widget_get_property (parent, "size");
      glade_property_get (property, &size);
      glade_command_set_property (property, size + 1);

      glade_command_pop_group ();
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->action_activate (adaptor, object, action_path);
}

void
glade_gtk_header_bar_child_action_activate (GladeWidgetAdaptor * adaptor,
                                            GObject * container,
                                            GObject * object,
                                            const gchar * action_path)
{
  if (strcmp (action_path, "remove_slot") == 0)
    {
      GladeWidget *parent;
      GladeProperty *property;

      parent = glade_widget_get_from_gobject (container);
      glade_command_push_group (_("Remove placeholder from %s"), glade_widget_get_name (parent));

      if (g_object_get_data (object, "special-child-type"))
        {
          property = glade_widget_get_property (parent, "use-custom-title");
          glade_command_set_property (property, FALSE);
        }
      else
        {
          gint size;

          gtk_container_remove (GTK_CONTAINER (container), GTK_WIDGET (object));

          property = glade_widget_get_property (parent, "size");
          glade_property_get (property, &size);
          glade_command_set_property (property, size - 1);
        }

      glade_command_pop_group ();
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
  else if (!strcmp (id, "size"))
    {
      g_value_reset (value);
      g_value_set_int (value, glade_gtk_header_bar_get_num_children (object));
    }
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->get_property (adaptor, object, id, value);
}

static void
glade_gtk_header_bar_set_size (GObject * object,
                               const GValue * value)
{
  GList *l, *next, *children;
  GtkWidget *child;
  guint new_size, old_size, i;

  g_return_if_fail (GTK_IS_HEADER_BAR (object));

  d(g_message ("Setting size to %d", g_value_get_int (value)));

  if (glade_util_object_is_loading (object))
    return;

  children = gtk_container_get_children (GTK_CONTAINER (object));
  l = children;
  while (l)
    {
      next = l->next;
      if (l->data == gtk_header_bar_get_custom_title (GTK_HEADER_BAR (object)) ||
          (!glade_widget_get_from_gobject (l->data) && !GLADE_IS_PLACEHOLDER (l->data)))
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

  for (i = old_size; i < new_size; i++)
    {
      GtkWidget *placeholder = glade_placeholder_new ();
      gtk_header_bar_pack_start (GTK_HEADER_BAR (object), placeholder);
    }
  for (l = g_list_last (children); l && old_size > new_size; l = l->prev)
    {
      child = l->data;
      if (glade_widget_get_from_gobject (child) || !GLADE_IS_PLACEHOLDER (child))
        continue;

      gtk_container_remove (GTK_CONTAINER (object), child);
      old_size--;
    }

  g_list_free (children);
}

void
glade_gtk_header_bar_set_use_custom_title (GObject *object,
					   gboolean use_custom_title)
{
  GladeWidget *gwidget = glade_widget_get_from_gobject (object);
  GtkWidget *child;

  if (use_custom_title)
    {
      child = gtk_header_bar_get_custom_title (GTK_HEADER_BAR (object));
      if (!child)
	{
	  child = glade_placeholder_new ();
	  g_object_set_data (G_OBJECT (child), "special-child-type", "title");
	}
    }
  else
    child = NULL;

  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (object), child);

  if (GLADE_IS_PLACEHOLDER (child))
    {
      GList *list, *l;

      list = glade_placeholder_packing_actions (GLADE_PLACEHOLDER (child));
      for (l = list; l; l = l->next)
	{
	  GladeWidgetAction *gwa = l->data;
	  if (!strcmp (glade_widget_action_get_class (gwa)->id, "remove_slot"))
	    glade_widget_action_set_visible (gwa, FALSE);
	}
    }

  if (use_custom_title)
    {
      glade_widget_property_set_sensitive (gwidget, "title", FALSE, TITLE_DISABLED_MESSAGE);
      glade_widget_property_set_sensitive (gwidget, "subtitle", FALSE, TITLE_DISABLED_MESSAGE);
      glade_widget_property_set_sensitive (gwidget, "has-subtitle", FALSE, TITLE_DISABLED_MESSAGE);
    }
  else
    {
      glade_widget_property_set_sensitive (gwidget, "title", TRUE, NULL);
      glade_widget_property_set_sensitive (gwidget, "subtitle", TRUE, NULL);
      glade_widget_property_set_sensitive (gwidget, "has-subtitle", TRUE, NULL);
    }
}

void
glade_gtk_header_bar_set_property (GladeWidgetAdaptor * adaptor,
                                   GObject * object,
                                   const gchar * id,
                                   const GValue * value)
{
  if (!strcmp (id, "use-custom-title"))
    glade_gtk_header_bar_set_use_custom_title (object, g_value_get_boolean (value));
  else if (!strcmp (id, "show-close-button"))
    {
      GladeWidget *gwidget = glade_widget_get_from_gobject (object);

      /* We don't set the property to 'ignore' so that we catch this in the adaptor,
       * but we also do not apply the property to the runtime object here, thus
       * avoiding showing the close button which would in turn close glade itself
       * when clicked.
       */
      glade_widget_property_set_sensitive (gwidget, "decoration-layout",
					   g_value_get_boolean (value),
					   _("The decoration layout does not apply to header bars "
					     "which do no show window controls"));
    }
  else if (!strcmp (id, "size"))
    glade_gtk_header_bar_set_size (object, value);
  else
    GWA_GET_CLASS (GTK_TYPE_CONTAINER)->set_property (adaptor, object, id, value);
}

void
glade_gtk_header_bar_add_child (GladeWidgetAdaptor *adaptor,
                                GObject *parent,
                                GObject *child)
{
  GladeWidget *gbox, *gchild;
  gint size;
  gchar *special_child_type;

  gchild = glade_widget_get_from_gobject (child);
  if (gchild)
    glade_widget_set_pack_action_visible (gchild, "remove_slot", FALSE);

  special_child_type = g_object_get_data (child, "special-child-type");

  d(g_message ("Add %s %p (special: %s)",
	       GLADE_IS_PLACEHOLDER (child) ? "placeholder" : "child",
	       child, special_child_type));

  if (special_child_type && !strcmp (special_child_type, "title"))
    {
      gtk_header_bar_set_custom_title (GTK_HEADER_BAR (parent), GTK_WIDGET (child));
      return;
    }

  GWA_GET_CLASS (GTK_TYPE_CONTAINER)->add (adaptor, parent, child);
  
  gbox = glade_widget_get_from_gobject (parent);
  if (!glade_widget_superuser ())
    {
      glade_widget_property_get (gbox, "size", &size);
      glade_widget_property_set (gbox, "size", size);
    }
}

void
glade_gtk_header_bar_remove_child (GladeWidgetAdaptor * adaptor,
                                   GObject * object,
                                   GObject * child)
{
  GladeWidget *gbox;
  gint size;
  gchar *special_child_type;

  special_child_type = g_object_get_data (child, "special-child-type");

  d(g_message ("Remove %s %p (special: %s)", 
	       GLADE_IS_PLACEHOLDER (child) ? "placeholder" : "child",
	       child, special_child_type));

  if (special_child_type && !strcmp (special_child_type, "title"))
    {
      GtkWidget *replacement = glade_placeholder_new ();

      g_object_set_data (G_OBJECT (replacement), "special-child-type", "title");
      gtk_header_bar_set_custom_title (GTK_HEADER_BAR (object), replacement);
      return;
    }
  
  gtk_container_remove (GTK_CONTAINER (object), GTK_WIDGET (child));

  /* Synchronize number of placeholders, this should trigger the set_property method with the
   * correct value (not the arbitrary number of children currently in the headerbar)
   */
  gbox = glade_widget_get_from_gobject (object);
  if (!glade_widget_superuser ())
    {
      glade_widget_property_get (gbox, "size", &size);
      glade_widget_property_set (gbox, "size", size);
    }
}

void
glade_gtk_header_bar_replace_child (GladeWidgetAdaptor * adaptor,
                                    GObject * container,
                                    GObject * current,
                                    GObject * new_widget)
{
  GladeWidget *gbox;
  gchar *special_child_type;
  gint size;

  special_child_type =
    g_object_get_data (G_OBJECT (current), "special-child-type");

  d(g_message ("Replace %s %p (special: %s) with %s %p",
	       GLADE_IS_PLACEHOLDER (current) ? "placeholder" : "child",
	       current, special_child_type,
	       GLADE_IS_PLACEHOLDER (new_widget) ? "placeholder" : "child",
	       new_widget));

  if (special_child_type && !strcmp (special_child_type, "title"))
    {
      g_object_set_data (G_OBJECT (new_widget), "special-child-type", "title");
      gtk_header_bar_set_custom_title (GTK_HEADER_BAR (container), GTK_WIDGET (new_widget));
      return;
    }
  else
    g_object_set_data (G_OBJECT (new_widget), "special-child-type", NULL);

  GWA_GET_CLASS
      (GTK_TYPE_CONTAINER)->replace_child (adaptor, container, current, new_widget);

  gbox = glade_widget_get_from_gobject (container);
  if (!glade_widget_superuser ())
    {
      glade_widget_property_get (gbox, "size", &size);
      glade_widget_property_set (gbox, "size", size);
    }
}

gboolean
glade_gtk_header_bar_verify_property (GladeWidgetAdaptor *adaptor,
                                      GObject            *object,
                                      const gchar        *id,
                                      const GValue       *value)
{
  if (!strcmp (id, "size"))
    return glade_gtk_header_bar_verify_size (object, value);
  else if (GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property)
    return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->verify_property (adaptor, object, id, value);

  return TRUE;
}

void
glade_gtk_header_bar_child_set_property (GladeWidgetAdaptor *adaptor,
                                         GObject *container,
                                         GObject *child,
                                         const gchar *property_name,
                                         const GValue *value)
{
  GladeWidget *gbox;
  gint size;

  d(g_message ("Set child prop %s %s\n", g_type_name_from_instance (child), property_name));

  gtk_container_child_set_property (GTK_CONTAINER (container),
                                    GTK_WIDGET (child),
                                    property_name,
                                    value);

  gbox = glade_widget_get_from_gobject (container);
  if (!glade_widget_superuser ())
    {
      glade_widget_property_get (gbox, "size", &size);
      glade_widget_property_set (gbox, "size", size);
    }
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
