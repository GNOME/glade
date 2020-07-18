#include <glib.h>
#include <glib-object.h>

#include <gladeui/glade-app.h>

typedef void (* AssertParentedFunc) (GObject *parent, GObject *child);

typedef struct {
  GType parent_type;
  GType child_type;
  AssertParentedFunc func;
} TestData;

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
test_add_child (gconstpointer data)
{
  TestData *test = (TestData *)data;
  GladeWidgetAdaptor *parent_adaptor;
  GladeWidgetAdaptor *child_adaptor;
  GladeWidget        *gparent, *gchild;
  GObject            *parent, *child;
  gboolean            parent_finalized = FALSE;
  gboolean            child_finalized = FALSE;
  gboolean            gparent_finalized = FALSE;
  gboolean            gchild_finalized = FALSE;

  g_test_log_set_fatal_handler (ignore_gvfs_warning, NULL);

  parent_adaptor = glade_widget_adaptor_get_by_type (test->parent_type);
  child_adaptor  = glade_widget_adaptor_get_by_type (test->child_type);

  gparent = glade_widget_adaptor_create_widget (parent_adaptor, FALSE, NULL);
  gchild  = glade_widget_adaptor_create_widget (child_adaptor, FALSE, NULL);

  parent = glade_widget_get_object (gparent);
  child  = glade_widget_get_object (gchild);

  glade_widget_add_child (gparent, gchild, FALSE);

  /* Pass ownership to the parent */
  g_object_unref (gchild);

  g_assert_true (glade_widget_get_parent (gchild) == gparent);

  if (test->func)
    test->func (parent, child);

  /* filechoosers hold a reference until an async operation is complete */
  if (GTK_IS_FILE_CHOOSER (parent) || GTK_IS_FILE_CHOOSER (child))
    {
      g_timeout_add (2000, main_loop_quit_cb, NULL);
      gtk_main();
    }
  /* Our plugin code adds an idle when cell renderers are created */
  else if (GTK_IS_CELL_RENDERER (child))
    {
      g_timeout_add (50, main_loop_quit_cb, NULL);
      gtk_main();
    }

  /* Unreffing the parent should finalize the parent and child runtime objects */
  g_object_weak_ref (G_OBJECT (gparent), check_finalized, &gparent_finalized);
  g_object_weak_ref (G_OBJECT (gchild),  check_finalized, &gchild_finalized);
  g_object_weak_ref (G_OBJECT (parent), check_finalized, &parent_finalized);
  g_object_weak_ref (G_OBJECT (child),  check_finalized, &child_finalized);

  g_object_unref (gparent);

  g_assert_true (gparent_finalized);
  g_assert_true (gchild_finalized);
  g_assert_true (parent_finalized);
  g_assert_true (child_finalized);
}

static void
add_test (GType parent_type,
	  GType child_type,
	  AssertParentedFunc func)
{
  gchar *test_path;
  TestData *data = g_new (TestData, 1);

  test_path = g_strdup_printf ("/AddChild/%s/%s",
			       g_type_name (parent_type),
			       g_type_name (child_type));

  data->parent_type = parent_type;
  data->child_type  = child_type;
  data->func        = func;

  g_test_add_data_func_full (test_path, data, test_add_child, g_free);
  g_free (test_path);
}

static void
assert_widget_parented (GObject *parent,
			GObject *child)
{
  g_assert_true (gtk_widget_get_parent (GTK_WIDGET (child)) == GTK_WIDGET (parent));
}

static void
assert_submenu (GObject *parent,
		GObject *child)
{
  if (GTK_IS_MENU_ITEM (parent))
    g_assert_true (gtk_menu_item_get_submenu (GTK_MENU_ITEM (parent)) == GTK_WIDGET (child));
  else if (GTK_IS_MENU_TOOL_BUTTON (parent))
    g_assert_true (gtk_menu_tool_button_get_menu (GTK_MENU_TOOL_BUTTON (parent)) == GTK_WIDGET (child));
  else
    g_assert_true (FALSE);
}

static void
assert_cell_parented (GObject *parent,
		      GObject *child)
{
  GList *cells;

  cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (parent));
  g_assert_true (g_list_find (cells, child) != NULL);
  g_list_free (cells);
}

static void
assert_column_parented (GObject *parent,
			GObject *child)
{
  g_assert_true (gtk_tree_view_get_column (GTK_TREE_VIEW (parent), 0) == GTK_TREE_VIEW_COLUMN (child));
}

/* Ignore deprecated classes, we test them regardless */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static void
add_child_widgets (GType parent_type)
{
  add_test (parent_type, GTK_TYPE_IMAGE,               assert_widget_parented);
  add_test (parent_type, GTK_TYPE_LABEL,               assert_widget_parented);
  add_test (parent_type, GTK_TYPE_ACCEL_LABEL,         assert_widget_parented);
  add_test (parent_type, GTK_TYPE_ENTRY,               assert_widget_parented);
  add_test (parent_type, GTK_TYPE_SEARCH_ENTRY,        assert_widget_parented);
  add_test (parent_type, GTK_TYPE_SPIN_BUTTON,         assert_widget_parented);
  add_test (parent_type, GTK_TYPE_SWITCH,              assert_widget_parented);
  add_test (parent_type, GTK_TYPE_SEPARATOR,           assert_widget_parented);
  add_test (parent_type, GTK_TYPE_ARROW,               assert_widget_parented);
  add_test (parent_type, GTK_TYPE_DRAWING_AREA,        assert_widget_parented);
  add_test (parent_type, GTK_TYPE_SPINNER,             assert_widget_parented);
  add_test (parent_type, GTK_TYPE_LEVEL_BAR,           assert_widget_parented);
  add_test (parent_type, GTK_TYPE_PROGRESS_BAR,        assert_widget_parented);
  add_test (parent_type, GTK_TYPE_SCALE,               assert_widget_parented);
  add_test (parent_type, GTK_TYPE_SCROLLBAR,           assert_widget_parented);
  add_test (parent_type, GTK_TYPE_BUTTON,              assert_widget_parented);
  add_test (parent_type, GTK_TYPE_TOGGLE_BUTTON,       assert_widget_parented);
  add_test (parent_type, GTK_TYPE_CHECK_BUTTON,        assert_widget_parented);
  add_test (parent_type, GTK_TYPE_RADIO_BUTTON,        assert_widget_parented);
  add_test (parent_type, GTK_TYPE_LINK_BUTTON,         assert_widget_parented);
  add_test (parent_type, GTK_TYPE_MENU_BUTTON,         assert_widget_parented);

  add_test (parent_type, GTK_TYPE_COMBO_BOX,           assert_widget_parented);
  add_test (parent_type, GTK_TYPE_COMBO_BOX_TEXT,      assert_widget_parented);
  add_test (parent_type, GTK_TYPE_SCALE_BUTTON,        assert_widget_parented);
  add_test (parent_type, GTK_TYPE_VOLUME_BUTTON,       assert_widget_parented);
  add_test (parent_type, GTK_TYPE_FONT_BUTTON,         assert_widget_parented);
  add_test (parent_type, GTK_TYPE_COLOR_BUTTON,        assert_widget_parented);

  /* FIXME: FileChooserButton leaks a GTask which will crash in the following test */
  /* add_test (parent_type, GTK_TYPE_FILE_CHOOSER_BUTTON, assert_widget_parented); */
  add_test (parent_type, GTK_TYPE_APP_CHOOSER_BUTTON,  assert_widget_parented);
  add_test (parent_type, GTK_TYPE_TEXT_VIEW,           assert_widget_parented);
  add_test (parent_type, GTK_TYPE_TREE_VIEW,           assert_widget_parented);
  add_test (parent_type, GTK_TYPE_ICON_VIEW,           assert_widget_parented);
  add_test (parent_type, GTK_TYPE_CALENDAR,            assert_widget_parented);
  add_test (parent_type, GTK_TYPE_BOX,                 assert_widget_parented);
  add_test (parent_type, GTK_TYPE_NOTEBOOK,            assert_widget_parented);
  add_test (parent_type, GTK_TYPE_FRAME,               assert_widget_parented);
  add_test (parent_type, GTK_TYPE_ASPECT_FRAME,        assert_widget_parented);
  add_test (parent_type, GTK_TYPE_OVERLAY,             assert_widget_parented);
  add_test (parent_type, GTK_TYPE_MENU_BAR,            assert_widget_parented);
  add_test (parent_type, GTK_TYPE_TOOLBAR,             assert_widget_parented);
  add_test (parent_type, GTK_TYPE_TOOL_PALETTE,        assert_widget_parented);
  add_test (parent_type, GTK_TYPE_PANED,               assert_widget_parented);
  add_test (parent_type, GTK_TYPE_BUTTON_BOX,          assert_widget_parented);
  add_test (parent_type, GTK_TYPE_LAYOUT,              assert_widget_parented);
  add_test (parent_type, GTK_TYPE_FIXED,               assert_widget_parented);
  add_test (parent_type, GTK_TYPE_EVENT_BOX,           assert_widget_parented);
  add_test (parent_type, GTK_TYPE_EXPANDER,            assert_widget_parented);
  add_test (parent_type, GTK_TYPE_VIEWPORT,            assert_widget_parented);
  add_test (parent_type, GTK_TYPE_ALIGNMENT,           assert_widget_parented);
  add_test (parent_type, GTK_TYPE_GRID,                assert_widget_parented);
  add_test (parent_type, GTK_TYPE_SCROLLED_WINDOW,     assert_widget_parented);
  add_test (parent_type, GTK_TYPE_INFO_BAR,            assert_widget_parented);
  add_test (parent_type, GTK_TYPE_STATUSBAR,           assert_widget_parented);
}

static void
add_child_cells (GType parent_type)
{
  add_test (parent_type, GTK_TYPE_CELL_RENDERER_TEXT, assert_cell_parented);
  add_test (parent_type, GTK_TYPE_CELL_RENDERER_ACCEL, assert_cell_parented);
  add_test (parent_type, GTK_TYPE_CELL_RENDERER_TOGGLE, assert_cell_parented);
  add_test (parent_type, GTK_TYPE_CELL_RENDERER_COMBO, assert_cell_parented);
  add_test (parent_type, GTK_TYPE_CELL_RENDERER_SPIN, assert_cell_parented);
  add_test (parent_type, GTK_TYPE_CELL_RENDERER_PIXBUF, assert_cell_parented);
  add_test (parent_type, GTK_TYPE_CELL_RENDERER_PROGRESS, assert_cell_parented);
  add_test (parent_type, GTK_TYPE_CELL_RENDERER_SPINNER, assert_cell_parented);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  glade_init ();
  glade_app_get ();

  /* Normal GtkContainer / GtkWidget parenting */
  add_child_widgets (GTK_TYPE_WINDOW);
  add_child_widgets (GTK_TYPE_BOX);
  add_child_widgets (GTK_TYPE_GRID);
  add_child_widgets (GTK_TYPE_NOTEBOOK);
  add_child_widgets (GTK_TYPE_OVERLAY);
  add_child_widgets (GTK_TYPE_PANED);
  add_child_widgets (GTK_TYPE_BUTTON_BOX);
  add_child_widgets (GTK_TYPE_LAYOUT);
  add_child_widgets (GTK_TYPE_FIXED);
  add_child_widgets (GTK_TYPE_EVENT_BOX);
  add_child_widgets (GTK_TYPE_EXPANDER);
  add_child_widgets (GTK_TYPE_VIEWPORT);
  add_child_widgets (GTK_TYPE_ALIGNMENT);

  /* Actions */
  add_test (GTK_TYPE_ACTION_GROUP, GTK_TYPE_ACTION, NULL);
  add_test (GTK_TYPE_ACTION_GROUP, GTK_TYPE_TOGGLE_ACTION, NULL);
  add_test (GTK_TYPE_ACTION_GROUP, GTK_TYPE_RADIO_ACTION, NULL);
  add_test (GTK_TYPE_ACTION_GROUP, GTK_TYPE_RECENT_ACTION, NULL);

  /* Menus */
  add_test (GTK_TYPE_MENU_BAR, GTK_TYPE_MENU_ITEM, assert_widget_parented);
  add_test (GTK_TYPE_MENU_BAR, GTK_TYPE_IMAGE_MENU_ITEM, assert_widget_parented);
  add_test (GTK_TYPE_MENU_BAR, GTK_TYPE_CHECK_MENU_ITEM, assert_widget_parented);
  add_test (GTK_TYPE_MENU_BAR, GTK_TYPE_RADIO_MENU_ITEM, assert_widget_parented);
  add_test (GTK_TYPE_MENU_BAR, GTK_TYPE_SEPARATOR_MENU_ITEM, assert_widget_parented);
  add_test (GTK_TYPE_MENU, GTK_TYPE_MENU_ITEM, assert_widget_parented);
  add_test (GTK_TYPE_MENU, GTK_TYPE_IMAGE_MENU_ITEM, assert_widget_parented);
  add_test (GTK_TYPE_MENU, GTK_TYPE_CHECK_MENU_ITEM, assert_widget_parented);
  add_test (GTK_TYPE_MENU, GTK_TYPE_RADIO_MENU_ITEM, assert_widget_parented);
  add_test (GTK_TYPE_MENU, GTK_TYPE_SEPARATOR_MENU_ITEM, assert_widget_parented);
  add_test (GTK_TYPE_MENU_ITEM, GTK_TYPE_MENU, assert_submenu);
  add_test (GTK_TYPE_IMAGE_MENU_ITEM, GTK_TYPE_MENU, assert_submenu);
  add_test (GTK_TYPE_CHECK_MENU_ITEM, GTK_TYPE_MENU, assert_submenu);
  add_test (GTK_TYPE_RADIO_MENU_ITEM, GTK_TYPE_MENU, assert_submenu);

  /* Toolbars / ToolPalette */
  add_test (GTK_TYPE_TOOLBAR, GTK_TYPE_TOOL_BUTTON, assert_widget_parented);
  add_test (GTK_TYPE_TOOLBAR, GTK_TYPE_TOGGLE_TOOL_BUTTON, assert_widget_parented);
  add_test (GTK_TYPE_TOOLBAR, GTK_TYPE_RADIO_TOOL_BUTTON, assert_widget_parented);
  add_test (GTK_TYPE_TOOLBAR, GTK_TYPE_MENU_TOOL_BUTTON, assert_widget_parented);
  add_test (GTK_TYPE_TOOL_ITEM_GROUP, GTK_TYPE_TOOL_BUTTON, assert_widget_parented);
  add_test (GTK_TYPE_TOOL_ITEM_GROUP, GTK_TYPE_TOGGLE_TOOL_BUTTON, assert_widget_parented);
  add_test (GTK_TYPE_TOOL_ITEM_GROUP, GTK_TYPE_RADIO_TOOL_BUTTON, assert_widget_parented);
  add_test (GTK_TYPE_TOOL_ITEM_GROUP, GTK_TYPE_MENU_TOOL_BUTTON, assert_widget_parented);
  add_test (GTK_TYPE_TOOL_PALETTE, GTK_TYPE_TOOL_ITEM_GROUP, assert_widget_parented);
  add_test (GTK_TYPE_MENU_TOOL_BUTTON, GTK_TYPE_MENU, assert_submenu);

  /* Cell layouts */
  add_test (GTK_TYPE_TREE_VIEW, GTK_TYPE_TREE_VIEW_COLUMN, assert_column_parented);
  add_child_cells (GTK_TYPE_TREE_VIEW_COLUMN);
  add_child_cells (GTK_TYPE_ICON_VIEW);
  add_child_cells (GTK_TYPE_COMBO_BOX);

  /* TextTag */
  add_test (GTK_TYPE_TEXT_TAG_TABLE, GTK_TYPE_TEXT_TAG, NULL);

  return g_test_run ();
}
