/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef __GLADE_SIGNAL_EDITOR_H__
#define __GLADE_SIGNAL_EDITOR_H__

#include <gladeui/glade.h>

G_BEGIN_DECLS

#define GLADE_TYPE_SIGNAL_EDITOR            (glade_signal_editor_get_type ())
#define GLADE_SIGNAL_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_SIGNAL_EDITOR, GladeSignalEditor))
#define GLADE_SIGNAL_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_SIGNAL_EDITOR, GladeSignalEditorClass))
#define GLADE_IS_SIGNAL_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_SIGNAL_EDITOR))
#define GLADE_IS_SIGNAL_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_SIGNAL_EDITOR))
#define GLADE_SIGNAL_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_SIGNAL_EDITOR, GladeSignalEditorClass))

typedef struct _GladeSignalEditor  GladeSignalEditor;
typedef struct _GladeSignalEditorClass  GladeSignalEditorClass;

typedef gboolean (*IsVoidFunc) (const gchar *signal_handler);

enum
{
	GSE_COLUMN_SIGNAL,
	GSE_COLUMN_HANDLER,
	GSE_COLUMN_AFTER,
	GSE_COLUMN_USERDATA,
	GSE_COLUMN_LOOKUP,

	GSE_COLUMN_USERDATA_SLOT,
	GSE_COLUMN_LOOKUP_VISIBLE,
	GSE_COLUMN_AFTER_VISIBLE,
	GSE_COLUMN_HANDLER_EDITABLE,
	GSE_COLUMN_USERDATA_EDITABLE,
	GSE_COLUMN_SLOT, /* if this row contains a "<Type...>" label */
	GSE_COLUMN_BOLD,
	GSE_NUM_COLUMNS
};

/* The GladeSignalEditor is used to house the signal editor interface and
 * associated functionality.
 */
struct _GladeSignalEditor
{
	GObject parent;

	GtkWidget *main_window;  /* A vbox where all the widgets are added */

	GladeWidget *widget;
	GladeWidgetAdaptor *adaptor;

	gpointer  *editor;

	GtkWidget *signals_list;
	GtkTreeStore *model;
	GtkTreeView *tree_view;

	GtkTreeModel *handler_store, *userdata_store;
	GtkCellRenderer *handler_renderer, *userdata_renderer;
	GtkTreeViewColumn *handler_column, *userdata_column;
	IsVoidFunc is_void_handler, is_void_userdata;
};

struct _GladeSignalEditorClass
{
	GObjectClass parent_class;

	gboolean (*handler_editing_done)   (GladeSignalEditor *self,
						gchar *signal_name,
						gchar *old_handler,
						gchar *new_handler,
						GtkTreeIter *iter);

	gboolean (*userdata_editing_done)  (GladeSignalEditor *self,
						gchar *signal_name,
						gchar *old_userdata,
						gchar *new_userdata,
						GtkTreeIter *iter);

	gboolean (*handler_editing_started)    (GladeSignalEditor *self,
						gchar *signal_name,
						GtkTreeIter *iter,
						GtkCellEditable *editable);

	gboolean (*userdata_editing_started)   (GladeSignalEditor *self,
						gchar *signal_name,
						GtkTreeIter *iter,
						GtkCellEditable *editable);
};

GType glade_signal_editor_get_type (void) G_GNUC_CONST;

GladeSignalEditor *glade_signal_editor_new (gpointer *editor);

void glade_signal_editor_construct_signals_list (GladeSignalEditor *editor);

GtkWidget *glade_signal_editor_get_widget (GladeSignalEditor *editor);

void glade_signal_editor_load_widget (GladeSignalEditor *editor, GladeWidget *widget);

gboolean glade_signal_editor_handler_editing_started_default_impl (GladeSignalEditor *editor,
								   gchar *signal_name,
								   GtkTreeIter *iter,
								   GtkCellEditable *editable,
								   gpointer user_data);
gboolean glade_signal_editor_userdata_editing_started_default_impl (GladeSignalEditor *editor,
								    gchar *signal_name,
								    GtkTreeIter *iter,
								    GtkCellEditable *editable,
								    gpointer user_data);

G_END_DECLS

#endif /* __GLADE_SIGNAL_EDITOR_H__ */
