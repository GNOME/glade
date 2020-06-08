#include <glib.h>
#include <glib-object.h>

#include <gladeui/glade-app.h>
#include <gladeui/glade-widget-adaptor.h>

static void
test_object_class (gconstpointer data)
{
  GladeWidgetAdaptor *adaptor = glade_widget_adaptor_get_by_name(data);
  GladeWidget        *widget;
  GObject            *object;

  g_assert (GLADE_IS_WIDGET_ADAPTOR (adaptor));

  widget = glade_widget_adaptor_create_widget (adaptor, FALSE, NULL);
  g_assert (GLADE_IS_WIDGET (widget));

  object = glade_widget_get_object (widget);
  g_assert (G_IS_OBJECT (object));

  g_assert (g_strcmp0 (G_OBJECT_TYPE_NAME (object), data) == 0);

  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  glade_init ();
  glade_app_get ();

  g_test_add_data_func ("/Modules/Python", "MyPythonBox", test_object_class);
  g_test_add_data_func ("/Modules/JavaScript", "MyJSGrid", test_object_class);

  return g_test_run ();
}

