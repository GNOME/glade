#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <stdarg.h>
#include <gladeui/glade-tsort.h>
#include <gladeui/glade-app.h>

typedef struct
{
  GList *nodes;
  GList *edges;
  gchar **orig_nodes;
} TsortData;

static void
tsort_data_free (gpointer udata)
{
  TsortData *data = udata;
  g_list_free (data->nodes);
  _node_edge_list_free (data->edges);
  g_free (data);
}

static void
test_tsort_order (gconstpointer userdata)
{
  TsortData *data = (gpointer) userdata;
  gchar **orig_nodes = data->orig_nodes;
  GList *nodes, *l;
  
  nodes = _glade_tsort (&data->nodes, &data->edges);

  for (l = nodes; l && *orig_nodes; l = g_list_next (l), orig_nodes++)
    g_assert_cmpstr (l->data, ==, *orig_nodes);

  /* make sure all items where tested */
  g_assert_true (l == NULL && *orig_nodes == NULL);

  g_list_free (nodes);
}

static void
add_tsort_test_real (const gchar *path, gchar *nodes[], gchar *edges[][2])
{
  TsortData *data = g_new0(TsortData, 1);
  gint i;

  /* insert sorted alphabetically */
  for (i = 0; nodes[i]; i++)
    data->nodes = g_list_insert_sorted (data->nodes, nodes[i], (GCompareFunc)g_strcmp0);

  for (i = 0; edges[i][0]; i++)
    data->edges = _node_edge_prepend (data->edges, edges[i][0], edges[i][1]);

  data->orig_nodes = nodes;

  g_test_add_data_func_full (path, data, test_tsort_order, tsort_data_free);
}

#define add_tsort_test(nodes, edges) add_tsort_test_real ("/Tsort/"#nodes, nodes, edges)

static void
test_toplevel_order (gconstpointer userdata)
{
  gchar **data = (gpointer) userdata;
  gchar *project_path = *data;
  gchar **names = &data[1];
  GladeProject *project;
  GList *toplevels, *l;
  gchar *temp_path;
  const gchar *xml_data;
  gsize xml_size;
  GBytes *xml;

  g_assert_true (g_close (g_file_open_tmp ("glade-toplevel-order-XXXXXX.glade", &temp_path, NULL), NULL));

  /* Dump contents to a temp file */
  g_assert_true ((xml = g_resources_lookup_data (project_path, 0, NULL)));
  xml_data = g_bytes_get_data (xml, &xml_size);
  g_assert_true (g_file_set_contents (temp_path, xml_data, xml_size, NULL));
  g_bytes_unref (xml);

  /* Load project */
  g_assert_true ((project = glade_project_load (temp_path)));

  /* And save it, order should be the same */
  g_assert_true (glade_project_save (project, temp_path, NULL));
  g_object_unref (project);

  /* Reload saved project */
  g_assert_true ((project = glade_project_load (temp_path)));

  g_unlink (temp_path);

  /* And get toplevels to check order */
  g_assert_true ((toplevels = glade_project_toplevels (project)));

  for (l = toplevels; l && names; l = g_list_next (l), names++)
    {
      GladeWidget *toplevel;
      g_assert_true ((toplevel = glade_widget_get_from_gobject (l->data)));
      g_assert_cmpstr (glade_widget_get_name (toplevel), ==, *names);
    }

  /* make sure all items where tested */
  g_assert_true (!l && !*names);
  
  g_object_unref (project);
  g_free (temp_path);
}

#define add_project_test(data) g_test_add_data_func_full ("/ToplevelOrder/"#data, data, test_toplevel_order, NULL);
#define RESOURCE_PATH "/org/gnome/glade/tests/toplevel-order"
/* _glade_tsort() test cases */

/* the array must be properly ordered, since it will be used to test order */
static gchar *tsort_test[] = {"bbb", "ccc", "aaa", NULL };

static gchar *tsort_test_edges[][2] = { {"ccc","aaa"}, { NULL, NULL} };

/* GladeProject toplevel order test */
static gchar *order_test[] = {RESOURCE_PATH"/toplevel_order_test.glade",
  "bbb", "ccc", "aaa", NULL};

static gchar *order_test2[] = {RESOURCE_PATH"/toplevel_order_test2.glade",
  "aa", "bbb", "ccc", "aaa", "ddd", NULL };

static gchar *order_test3[] = {RESOURCE_PATH"/toplevel_order_test3.glade",
  "aaa", "ccc", "bbb", NULL };

/* toplevels with circular dependencies get ordered alphabetically at the end */
static gchar *order_test4[] = {RESOURCE_PATH"/toplevel_order_test4.glade",
  "aa", "bbb", "cc", "ddd", "aaa", "ccc", NULL };

/* children dependency */
static gchar *order_test5[] = {RESOURCE_PATH"/toplevel_order_test5.glade",
  "xaction", "awindow", NULL };

/* Commonly used widgets with dependencies */
static gchar *order_test6[] = {RESOURCE_PATH"/toplevel_order_test6.glade",
  "ziconfactory", "label_a", "label_b", "asizegroup", "label_c", "xaction",
  "xactiongroup", "anotherwindow", "xentrybuffer", "xliststore", "treeview",
  "zaccelgroup", "awindow", NULL };

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  glade_init ();
  glade_app_get ();

  add_tsort_test (tsort_test, tsort_test_edges);
  
  add_project_test (order_test);
  add_project_test (order_test2);
  add_project_test (order_test3);
  add_project_test (order_test4);
  add_project_test (order_test5);
  add_project_test (order_test6);
  
  return g_test_run ();
}
