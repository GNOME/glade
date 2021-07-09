#include <glib.h>
#include <glib-object.h>

#include <gladeui/glade-app.h>

/* Avoid warnings from GVFS-RemoteVolumeMonitor */
static gboolean
ignore_gvfs_warning (const gchar *log_domain,
                     GLogLevelFlags log_level,
                     const gchar *message,
                     gpointer user_data)
{
  if (g_strcmp0 (log_domain, "GVFS-RemoteVolumeMonitor") == 0)
    return FALSE;

  return TRUE;
}

static gboolean
main_loop_quit_cb (gpointer data)
{
  gtk_main_quit ();

  return FALSE;
}

static void
check_finalized (gpointer data,
                 GObject *where_the_object_was)
{
  gboolean *did_finalize = (gboolean *)data;

  *did_finalize = TRUE;
}

static void
test_create_widget (gconstpointer data)
{
  GladeWidgetAdaptor *adaptor = (GladeWidgetAdaptor *)data;
  GladeWidget        *widget;
  GObject            *object;
  gboolean            widget_finalized = FALSE;
  gboolean            object_finalized = FALSE;

  g_test_log_set_fatal_handler (ignore_gvfs_warning, NULL);


  widget = glade_widget_adaptor_create_widget (adaptor, FALSE, NULL);
  g_assert_true (GLADE_IS_WIDGET (widget));

  object = glade_widget_get_object (widget);
  g_assert_true (G_IS_OBJECT (object));

  g_object_weak_ref (G_OBJECT (widget),  check_finalized, &widget_finalized);
  g_object_weak_ref (G_OBJECT (object),  check_finalized, &object_finalized);

  /* filechoosers hold a reference until an async operation is complete */
  if (GTK_IS_FILE_CHOOSER (object))
    {
      g_timeout_add (2000, main_loop_quit_cb, NULL);
      gtk_main();
    }
  /* Our plugin code adds an idle when cell renderers are created */
  else if (GTK_IS_CELL_RENDERER (object))
    {
      g_timeout_add (50, main_loop_quit_cb, NULL);
      gtk_main();
    }

  /* Get rid of the GladeWidget and assert that it finalizes along 
   * with it's internal object
   */
  g_object_unref (widget);

  g_assert_true (widget_finalized);
  g_assert_true (object_finalized);
}

static gint
adaptor_cmp (gconstpointer a, gconstpointer b)
{
  return g_strcmp0 (glade_widget_adaptor_get_name ((gpointer)a),
                    glade_widget_adaptor_get_name ((gpointer)b));
}

int
main (int   argc,
      char *argv[])
{
  GList *adaptors, *l;

  gtk_test_init (&argc, &argv, NULL);

  glade_init ();
  glade_app_get ();

  adaptors = g_list_sort (glade_widget_adaptor_list_adaptors (), adaptor_cmp);
    
  for (l = adaptors; l; l = l->next)
    {
      GladeWidgetAdaptor *adaptor = l->data;
      GType               adaptor_type;

      adaptor_type = glade_widget_adaptor_get_object_type (adaptor);

      if (G_TYPE_IS_INSTANTIATABLE (adaptor_type) && !G_TYPE_IS_ABSTRACT (adaptor_type) &&
          /* FIXME: can not create a themed icon without a name */
          !g_type_is_a (adaptor_type, G_TYPE_THEMED_ICON) &&
          /* FIXME: can not create a file icon without a file name */
          !g_type_is_a (adaptor_type, G_TYPE_FILE_ICON) &&
          /* FIXME: GtkPopoverMenu gives a few warnings */
          !g_type_is_a (adaptor_type, GTK_TYPE_POPOVER_MENU) &&
          /* FIXME: GtkFileChooserNative is hard to test here */
          !g_type_is_a (adaptor_type, GTK_TYPE_FILE_CHOOSER_NATIVE))
        {
          gchar *test_path = g_strdup_printf ("/CreateWidget/%s", glade_widget_adaptor_get_name (adaptor));

          g_test_add_data_func (test_path, adaptor, test_create_widget);

          g_free (test_path);
        }
    }
  g_list_free (adaptors);

  return g_test_run ();
}
