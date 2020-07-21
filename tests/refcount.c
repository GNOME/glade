#include <glib.h>
#include <glib-object.h>

#include <gladeui/glade-app.h>
#include <gladeui/glade-project.h>

static void
check_finalized (gpointer data,
                 GObject *where_the_object_was)
{
  gboolean *did_finalize = (gboolean *)data;

  *did_finalize = TRUE;
}

static void
test_project (gconstpointer data)
{
  gboolean project_finalized = FALSE;
  GObject *project = NULL;

  project = G_OBJECT (glade_project_new ());
  g_assert_true (GLADE_IS_PROJECT (project));

  g_object_weak_ref (project,  check_finalized, &project_finalized);

  /* A newly created project should always have a refcout of 1 */
  g_assert_cmpint (project->ref_count, ==, 1);

  /* TODO: create toplevels and check gtk_window_list_toplevels() is empty */

  /* This should dispose project */
  g_object_unref (project);

  /* Now check if it was finalized */
  g_assert_true (project_finalized);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  glade_init ();
  glade_app_get ();

  g_test_add_data_func ("/RefCount/Project", NULL, test_project);

  return g_test_run ();
}

