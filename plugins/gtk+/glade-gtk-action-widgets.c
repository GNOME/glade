#define GLADE_TAG_ACTION_WIDGETS "action-widgets"
#define GLADE_TAG_ACTION_WIDGET  "action-widget"
#define GLADE_TAG_RESPONSE       "response"


static void
glade_gtk_dialog_read_responses (GladeWidget * widget,
                                 GladeXmlNode * widgets_node)
{
  GladeXmlNode *node;
  GladeWidget *action_widget;

  for (node = glade_xml_node_get_children (widgets_node);
       node; node = glade_xml_node_next (node))
    {
      gchar *widget_name, *response;

      if (!glade_xml_node_verify (node, GLADE_TAG_ACTION_WIDGET))
        continue;

      response =
          glade_xml_get_property_string_required (node, GLADE_TAG_RESPONSE,
                                                  NULL);
      widget_name = glade_xml_get_content (node);

      if ((action_widget =
           glade_project_get_widget_by_name (glade_widget_get_project (widget), 
					     widget_name)) != NULL)
        {
          glade_widget_property_set (action_widget, "response-id",
                                     g_ascii_strtoll (response, NULL, 10));
        }

      g_free (response);
      g_free (widget_name);
    }
}

void
glade_gtk_dialog_read_child (GladeWidgetAdaptor * adaptor,
                             GladeWidget * widget, GladeXmlNode * node)
{
  GladeXmlNode *widgets_node;

  GWA_GET_CLASS (GTK_TYPE_CONTAINER)->read_child (adaptor, widget, node);

  node = glade_xml_node_get_parent (node);

  if ((widgets_node =
       glade_xml_search_child (node, GLADE_TAG_ACTION_WIDGETS)) != NULL)
    glade_gtk_dialog_read_responses (widget, widgets_node);
}


static void
glade_gtk_dialog_write_responses (GladeWidget * widget,
                                  GladeXmlContext * context,
                                  GladeXmlNode * node)
{
  GladeXmlNode *widget_node;
  GtkDialog *dialog = GTK_DIALOG (glade_widget_get_object (widget));
  GList *l, *action_widgets =
      gtk_container_get_children (GTK_CONTAINER
                                  (gtk_dialog_get_action_area (dialog)));

  for (l = action_widgets; l; l = l->next)
    {
      GladeWidget *action_widget;
      GladeProperty *property;
      gchar *str;

      if ((action_widget = glade_widget_get_from_gobject (l->data)) == NULL)
        continue;

      if ((property =
           glade_widget_get_property (action_widget, "response-id")) == NULL)
        continue;

      widget_node = glade_xml_node_new (context, GLADE_TAG_ACTION_WIDGET);
      glade_xml_node_append_child (node, widget_node);

      str =
          glade_property_class_make_string_from_gvalue (glade_property_get_class (property),
                                                        glade_property_inline_value (property));

      glade_xml_node_set_property_string (widget_node, GLADE_TAG_RESPONSE, str);
      glade_xml_set_content (widget_node, glade_widget_get_name (action_widget));

      g_free (str);
    }

  g_list_free (action_widgets);
}

void
glade_gtk_dialog_write_child (GladeWidgetAdaptor * adaptor,
                              GladeWidget * widget,
                              GladeXmlContext * context, GladeXmlNode * node)
{
  GladeXmlNode *widgets_node;
  GladeWidget *parent;
  GladeProject *project;

  GWA_GET_CLASS (GTK_TYPE_CONTAINER)->write_child (adaptor, widget, context,
                                                   node);

  parent  = glade_widget_get_parent (widget);
  project = glade_widget_get_project (widget);

  if (parent && GTK_IS_DIALOG (glade_widget_get_object (parent)))
    {
      widgets_node = glade_xml_node_new (context, GLADE_TAG_ACTION_WIDGETS);

      glade_gtk_dialog_write_responses (parent, context, widgets_node);

      if (!glade_xml_node_get_children (widgets_node))
        glade_xml_node_delete (widgets_node);
      else
        glade_xml_node_append_child (node, widgets_node);
    }
}
