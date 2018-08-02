#include <config.h>
#include <glib/gi18n-lib.h>
#include <gladeui/glade.h>
#include "glade-gtk.h"

#include "glade-model-button-editor.h"

static void
model_button_clicked (GtkWidget *widget, gpointer user_data)
{
  GtkWidget *popover;

  popover = gtk_widget_get_ancestor (widget, GTK_TYPE_POPOVER);
  if (popover != NULL)
    gtk_widget_show (popover);
}

void
glade_gtk_model_button_post_create (GladeWidgetAdaptor *adaptor,
                                    GObject            *object,
                                    GladeCreateReason   reason)
{
  g_signal_connect (object, "clicked",
                    G_CALLBACK (model_button_clicked), NULL);
}

void
glade_gtk_model_button_read_widget (GladeWidgetAdaptor *adaptor,
                                    GladeWidget        *widget,
                                    GladeXmlNode       *node)
{
  GWA_GET_CLASS (GTK_TYPE_CONTAINER)->read_widget (adaptor, widget, node);
}

void
glade_gtk_model_button_write_widget (GladeWidgetAdaptor *adaptor,
                                     GladeWidget        *widget,
                                     GladeXmlContext    *context,
                                     GladeXmlNode       *node)
{
  GWA_GET_CLASS (GTK_TYPE_CONTAINER)->write_widget (adaptor, widget, context, node);
}

GladeEditable *
glade_gtk_model_button_create_editable (GladeWidgetAdaptor *adaptor,
                                        GladeEditorPageType type)
{
  if (type == GLADE_PAGE_GENERAL)
    return (GladeEditable *) glade_model_button_editor_new ();
  else
    return GWA_GET_CLASS (GTK_TYPE_CONTAINER)->create_editable (adaptor, type);
}
