/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Juan Pablo Ugarte.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#include "glade.h"
#include "glade-editor-property.h"
#include "glade-base-editor.h"
#include "glade-accumulators.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

typedef enum
{
	GLADE_BASE_EDITOR_GTYPE,
	GLADE_BASE_EDITOR_NAME,
	GLADE_BASE_EDITOR_N_COLUMNS
}GladeBaseEditorChildEnum;

typedef enum
{
	GLADE_BASE_EDITOR_MENU_GWIDGET,
	GLADE_BASE_EDITOR_MENU_OBJECT,
	GLADE_BASE_EDITOR_MENU_TYPE_NAME,
	GLADE_BASE_EDITOR_MENU_NAME,
	GLADE_BASE_EDITOR_MENU_N_COLUMNS
}GladeBaseEditorEnum;

struct _GladeBaseEditorPrivate
{
	GladeWidget *gcontainer; /* The container we are editing */
	GtkListStore *children;
	
	/* Editor UI */
	GtkWidget *paned, *popup, *table, *treeview;
	GtkWidget *remove_button, *signal_editor_w;
	GladeSignalEditor *signal_editor;
	
	GtkListStore *lstore;
	GtkTreeStore *tstore;
	GtkTreeModel *model;
	GladeProject *project;
	
	/* Add button data */
	GType add_type;
	gboolean add_as_child;
	
	/* Temporal variables */
	GtkTreeIter iter; /* used in idle functions */
	gint row;
	
	gboolean updating_treeview;
};

typedef struct _GladeBaseEditorSignal     GladeBaseEditorSignal;
typedef enum   _GladeBaseEditorSignalType GladeBaseEditorSignalType;

enum _GladeBaseEditorSignalType
{
	SIGNAL_CHILD_SELECTED,
	SIGNAL_CHANGE_TYPE,
	SIGNAL_GET_DISPLAY_NAME,
	SIGNAL_BUILD_CHILD,
	SIGNAL_DELETE_CHILD,
	SIGNAL_MOVE_CHILD,
	LAST_SIGNAL
};

struct _GladeBaseEditorSignal
{
	GladeBaseEditor *object;
};

static guint glade_base_editor_signals [LAST_SIGNAL] = { 0 };
static GtkVBoxClass *parent_class = NULL;

static void glade_base_editor_set_container   (GladeBaseEditor *editor,
					       GObject *container);
static void glade_base_editor_block_callbacks (GladeBaseEditor *editor,
					       gboolean block);

/* glade_base_editor_store_*  wrapper functions to use the tree/list store */
static void
glade_base_editor_store_set (GladeBaseEditor *editor, GtkTreeIter *iter, ...)
{
	va_list args;
	
	va_start (args, iter);
	
	if (editor->priv->tstore)
		gtk_tree_store_set_valist (editor->priv->tstore, iter, args);
	else
		gtk_list_store_set_valist (editor->priv->lstore, iter, args);
		
	va_end (args);
}

static void
glade_base_editor_store_remove (GladeBaseEditor *editor, GtkTreeIter *iter)
{
	if (editor->priv->tstore)
		gtk_tree_store_remove (editor->priv->tstore, iter);
	else
		gtk_list_store_remove (editor->priv->lstore, iter);
}

static void
glade_base_editor_store_clear (GladeBaseEditor *editor)
{
	gtk_tree_view_set_model (GTK_TREE_VIEW (editor->priv->treeview), NULL);
	
	if (editor->priv->tstore)
		gtk_tree_store_clear (editor->priv->tstore);
	else
		gtk_list_store_clear (editor->priv->lstore);
	
	gtk_tree_view_set_model (GTK_TREE_VIEW (editor->priv->treeview), editor->priv->model);
}

static void
glade_base_editor_store_append (GladeBaseEditor *editor,
				GtkTreeIter *iter,
				GtkTreeIter *parent)
{
	if (editor->priv->tstore)
		gtk_tree_store_append (editor->priv->tstore, iter, parent);
	else
		gtk_list_store_append (editor->priv->lstore, iter);
}

static void
glade_base_editor_store_insert_after (GladeBaseEditor *editor,
				      GtkTreeIter *iter,
				      GtkTreeIter *parent,
				      GtkTreeIter *sibling)
{
	if (editor->priv->tstore)
		gtk_tree_store_insert_after (editor->priv->tstore, iter, parent, sibling);
	else
		gtk_list_store_insert_after (editor->priv->lstore, iter, sibling);
}

static gboolean
glade_base_editor_get_type_info (GladeBaseEditor *e,
				 GtkTreeIter *retiter,
				 GType child_type,
				 ...)
{
	GtkTreeModel *model = GTK_TREE_MODEL (e->priv->children);
	GtkTreeIter iter;
	GType type;
	
	if (gtk_tree_model_get_iter_first (model, &iter) == FALSE)
		return FALSE;
	
	do
	{
		gtk_tree_model_get (model, &iter,
				    GLADE_BASE_EDITOR_GTYPE, &type,
				    -1);
		if (child_type == type)
		{
			va_list args;
			va_start (args, child_type);
			gtk_tree_model_get_valist (model, &iter, args);
			va_end (args);
			if (retiter) *retiter = iter;
			return TRUE;
		}
	} while (gtk_tree_model_iter_next (model, &iter));
	
	return FALSE;
}

static gchar *
glade_base_editor_get_display_name (GladeBaseEditor *editor,
				    GladeWidget *gchild)
{
	gchar *retval;	
	g_signal_emit (editor,
		       glade_base_editor_signals[SIGNAL_GET_DISPLAY_NAME],
		       0, gchild, &retval);
	return retval;
}

static void
glade_base_editor_fill_store_real (GladeBaseEditor *e, 
				   GladeWidget *gwidget,
				   GtkTreeIter *parent)
{
	GtkWidget *widget = GTK_WIDGET (glade_widget_get_object (gwidget));
	GList *children, *l;
	GtkTreeIter iter;
	
	children = l = glade_widget_adaptor_get_children (gwidget->adaptor,
							  G_OBJECT (widget));
	
	while (l)
	{
		GObject *child = (GObject*)l->data;
		GladeWidget *gchild;
		
		if(child && (gchild = glade_widget_get_from_gobject (child)))
		{
			gchar *type_name, *name;
						
			if (glade_base_editor_get_type_info (e, NULL,
							     G_OBJECT_TYPE (child),
							     GLADE_BASE_EDITOR_NAME, &type_name,
							     -1))
			{
				glade_base_editor_store_append (e, &iter, parent);
				
				name = glade_base_editor_get_display_name (e, gchild);
				
				glade_base_editor_store_set (e, &iter,
						    GLADE_BASE_EDITOR_MENU_GWIDGET, gchild,
						    GLADE_BASE_EDITOR_MENU_OBJECT, child,
						    GLADE_BASE_EDITOR_MENU_TYPE_NAME, type_name,
						    GLADE_BASE_EDITOR_MENU_NAME, name,
						    -1);
				
				if (GTK_IS_CONTAINER (child))
					glade_base_editor_fill_store_real (e, gchild, &iter);
			
				g_free (name);
				g_free (type_name);
			}
			else
				if (GTK_IS_CONTAINER (child))
					glade_base_editor_fill_store_real (e, gchild, parent);

		}

		l = g_list_next (l);
	}
	
	g_list_free (children);
}

static void
glade_base_editor_fill_store (GladeBaseEditor *e)
{
	glade_base_editor_store_clear (e);
	gtk_tree_view_set_model (GTK_TREE_VIEW (e->priv->treeview), NULL);
	glade_base_editor_fill_store_real (e, e->priv->gcontainer, NULL);
	gtk_tree_view_set_model (GTK_TREE_VIEW (e->priv->treeview), e->priv->model);
}

static gboolean
glade_base_editor_get_child_selected (GladeBaseEditor *e, GtkTreeIter *iter)
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (e->priv->treeview));
	return gtk_tree_selection_get_selected (sel, NULL, iter);
}

static void
glade_base_editor_name_activate (GtkEntry *entry, GladeWidget *gchild)
{
	const gchar *text = gtk_entry_get_text (GTK_ENTRY (entry));
	
	if (strcmp (glade_widget_get_name (gchild), text))
		glade_command_set_name (gchild, text);
}

static gboolean
glade_base_editor_name_focus_out (GtkWidget *entry,
				  GdkEventFocus *event,
				  GladeWidget *gchild)
{
	glade_base_editor_name_activate (GTK_ENTRY (entry), gchild);
	return FALSE;
}

static void
glade_base_editor_remove_widget (GtkWidget *widget, gpointer container)
{
	gtk_container_remove (GTK_CONTAINER (container), widget);
}

static void
glade_base_editor_table_attach (GladeBaseEditor *e,
				GtkWidget *child1,
				GtkWidget *child2)
{
	GtkTable *table = GTK_TABLE (e->priv->table);
	gint row = e->priv->row;
	
	if (child1)
	{
		gtk_table_attach (table, child1, 0, 1, row, row + 1,
				  GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 0);
		gtk_widget_show (child1);
	}
	
	if (child2)
	{
		gtk_table_attach (table, child2, 1, 2, row, row + 1,
				  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 0);
		gtk_widget_show (child2);
	}
	
	e->priv->row++;
}

static void
glade_base_editor_clear (GladeBaseEditor *editor)
{
	GladeBaseEditorPrivate *e = editor->priv;
	gtk_container_foreach (GTK_CONTAINER (e->table),
			       glade_base_editor_remove_widget, e->table);
	e->row = 0;
	gtk_widget_set_sensitive (e->remove_button, FALSE);
	glade_signal_editor_load_widget (e->signal_editor, NULL);
}

static void
glade_base_editor_treeview_cursor_changed (GtkTreeView *treeview,
					   GladeBaseEditor *editor)
{
	GladeBaseEditorPrivate *e = editor->priv;
	GtkTreeIter iter;
	GObject *child;
	GladeWidget *gchild;

	if (! glade_base_editor_get_child_selected (editor, &iter))
		return;

	glade_base_editor_clear (editor);
	gtk_widget_set_sensitive (e->remove_button, TRUE);
	
	gtk_tree_model_get (e->model, &iter,
			    GLADE_BASE_EDITOR_MENU_GWIDGET, &gchild,
			    GLADE_BASE_EDITOR_MENU_OBJECT, &child,
			    -1);

	/* Emit child-selected signal and let the user add the properties */
	g_signal_emit (editor, glade_base_editor_signals[SIGNAL_CHILD_SELECTED],
		       0, gchild);

	/* Update Signal Editor*/
	glade_signal_editor_load_widget (e->signal_editor, gchild);
}

static gboolean
glade_base_editor_update_properties_idle (gpointer data)
{
       glade_base_editor_treeview_cursor_changed (NULL, (GladeBaseEditor *)data);
       return FALSE;
}


static void 
glade_base_editor_update_properties (GladeBaseEditor *editor)
{
       g_idle_add (glade_base_editor_update_properties_idle, editor);
}

static void
glade_base_editor_set_cursor (GladeBaseEditor *e, GtkTreeIter *iter)
{
	GtkTreePath *path;
	GtkTreeIter real_iter;
	
	if (iter == NULL &&
	    glade_base_editor_get_child_selected (e, &real_iter))
		iter = &real_iter;
	
	if (iter && (path = gtk_tree_model_get_path (e->priv->model, iter)))
	{
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (e->priv->treeview), path, NULL, FALSE);
		gtk_tree_path_free (path);
	}
}

static gboolean
glade_base_editor_find_child_real (GladeBaseEditor *e,
				   GladeWidget *gchild,
				   GtkTreeIter *iter)
{
	GtkTreeModel *model = e->priv->model;
	GtkTreeIter child_iter;
	GladeWidget *child;
	
	do
	{
		gtk_tree_model_get (model, iter, GLADE_BASE_EDITOR_MENU_GWIDGET, &child, -1);
	
		if (child == gchild) return TRUE;

		if (gtk_tree_model_iter_children (model, &child_iter, iter))
			if (glade_base_editor_find_child_real (e, gchild, &child_iter))
			{
				*iter = child_iter;
				return TRUE;
			}
	}
	while (gtk_tree_model_iter_next (model, iter));
	
	return FALSE;
}

static gboolean
glade_base_editor_find_child (GladeBaseEditor *e,
			      GladeWidget *child,
			      GtkTreeIter *iter)
{
	if (gtk_tree_model_get_iter_first (e->priv->model, iter))
		return glade_base_editor_find_child_real (e, child, iter);
	
	return FALSE;
}

static void
glade_base_editor_select_child (GladeBaseEditor *e,
				GladeWidget *child)
{
	GtkTreeIter iter;
	
	if (glade_base_editor_find_child (e, child, &iter))
		glade_base_editor_set_cursor (e, &iter);
}

static void
glade_base_editor_child_change_type (GladeBaseEditor *editor,
				     GtkTreeIter *iter,
				     GType type)
{
	GladeWidget *gchild;
	GObject *child;
	gchar *class_name;
	gboolean retval;

	glade_base_editor_block_callbacks (editor, TRUE);
	
	/* Get old widget data */
	gtk_tree_model_get (editor->priv->model, iter,
			    GLADE_BASE_EDITOR_MENU_GWIDGET, &gchild,
			    GLADE_BASE_EDITOR_MENU_OBJECT, &child,
			    -1);
	
	if (type == G_OBJECT_TYPE (child)) return;
	
	/* Start of glade-command */
		
	if (glade_base_editor_get_type_info (editor, NULL, type,
					     GLADE_BASE_EDITOR_NAME, &class_name,
					     -1))
	{
		glade_command_push_group (_("Setting object type on %s to %s"),
					  glade_widget_get_name (gchild),
					  class_name);
		g_free (class_name);
	}
	else return;
	
	g_signal_emit (editor,
		       glade_base_editor_signals [SIGNAL_CHANGE_TYPE],
		       0, gchild, type, &retval);

	/* End of glade-command */
	glade_command_pop_group ();
	
	/* Update properties */
	glade_base_editor_update_properties (editor);
	
	glade_base_editor_block_callbacks (editor, FALSE);
}

static void
glade_base_editor_type_changed (GtkComboBox *widget, GladeBaseEditor *e)
{
	GtkTreeIter iter, combo_iter;
	GType type;
	
	if (! glade_base_editor_get_child_selected (e, &iter))
		return;
	
	gtk_combo_box_get_active_iter (widget, &combo_iter);
	
	gtk_tree_model_get (gtk_combo_box_get_model (widget), &combo_iter,
			    GLADE_BASE_EDITOR_GTYPE, &type, -1);

	glade_base_editor_child_change_type (e, &iter, type);
}

static void
glade_base_editor_child_type_edited (GtkCellRendererText *cell,
				     const gchar *path_string,
				     const gchar *new_text,
				     GladeBaseEditor *editor)
{
	GladeBaseEditorPrivate *e = editor->priv;
	GtkTreeModel *child_class = GTK_TREE_MODEL (e->children);
	GtkTreePath *path;
	GtkTreeIter iter, combo_iter;
	GType type;
	gchar *type_name;

	path = gtk_tree_path_new_from_string (path_string);
	gtk_tree_model_get_iter (e->model, &iter, path);
	gtk_tree_model_get (e->model, &iter,
			    GLADE_BASE_EDITOR_MENU_TYPE_NAME, &type_name,
			    -1);
	if (strcmp (type_name, new_text) == 0)
	{
		g_free (type_name);
		return;
	}
	
	/* Lookup GladeWidgetClass */
	gtk_tree_model_get_iter_first (child_class, &combo_iter);
	do
	{
		gtk_tree_model_get (child_class, &combo_iter,
				    GLADE_BASE_EDITOR_GTYPE, &type,
				    GLADE_BASE_EDITOR_NAME, &type_name,
				    -1);
		
		if (strcmp (type_name, new_text) == 0) break;
		
		g_free (type_name);
	} while (gtk_tree_model_iter_next (child_class, &combo_iter));
	
	glade_base_editor_child_change_type (editor, &iter, type);
}

static gint
glade_base_editor_popup_handler (GtkWidget *treeview,
				 GdkEventButton *event,
				 GladeBaseEditor *e)
{
	GtkTreePath *path;

	if (event->button == 3)
	{
		if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview),
			(gint) event->x, (gint) event->y, &path, NULL, NULL, NULL))
		{
			gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, NULL, FALSE);
			gtk_tree_path_free (path);
		}
		
		gtk_menu_popup (GTK_MENU (e->priv->popup), NULL, NULL, NULL, NULL, 
				event->button, event->time);
		return TRUE;
	}

	return FALSE;
}

static void
glade_base_editor_reorder_children (GladeBaseEditor *editor, GtkTreeIter *child)
{
	GtkTreeModel *model = editor->priv->model;
	GladeWidget   *gchild;
	GladeProperty *property;
	GtkTreeIter parent, iter;
	gint position = 0;

	if (gtk_tree_model_iter_parent (model, &parent, child))
		gtk_tree_model_iter_children (model, &iter, &parent);
	else
		gtk_tree_model_get_iter_first (model, &iter);

	do
	{
		gtk_tree_model_get (model, &iter, GLADE_BASE_EDITOR_MENU_GWIDGET, &gchild, -1);
		
		if ((property = glade_widget_get_property (gchild, "position")) != NULL)
			glade_command_set_property (property, position);
		position++;
	} while (gtk_tree_model_iter_next (model, &iter));
}

static void
glade_base_editor_add_child (GladeBaseEditor *editor,
			     GType type,
			     gboolean as_child)
{
	GladeBaseEditorPrivate *e = editor->priv;
	GtkTreeIter iter, new_iter;
	GladeWidget *gparent, *gchild = NULL, *gchild_new;
	gchar *type_name, *name, *class_name;

	if (glade_base_editor_get_type_info (editor, NULL, type,
					     GLADE_BASE_EDITOR_NAME, &class_name,
					     -1) == FALSE) return;
	
	glade_base_editor_block_callbacks (editor, TRUE);
	
	gparent = e->gcontainer;

	if (glade_base_editor_get_child_selected (editor, &iter))
	{
		gtk_tree_model_get (e->model, &iter,
				    GLADE_BASE_EDITOR_MENU_GWIDGET, &gchild,
				    -1);
		if (as_child)
		{
			glade_base_editor_store_append (editor, &new_iter, &iter);
			gparent = gchild;
		}
		else
		{
			glade_base_editor_store_insert_after (editor, &new_iter,
							      NULL, &iter);
			gparent = glade_widget_get_parent (gchild);
		}
		
	}
	else
		glade_base_editor_store_append (editor, &new_iter, NULL);

	glade_command_push_group (_("Add a %s to %s"), class_name, 
				  glade_widget_get_name (gparent));
	g_free (class_name);
	
	/* Build Child */
	g_signal_emit (editor, glade_base_editor_signals[SIGNAL_BUILD_CHILD],
		       0, gparent, type, &gchild_new);
	
	if (gchild_new == NULL)
	{
		glade_command_pop_group ();
		glade_base_editor_store_remove (editor, &new_iter);
		return;
	}
	
	glade_base_editor_get_type_info (editor, NULL, type,
					 GLADE_BASE_EDITOR_NAME, &type_name,
					 -1);
	
	name = glade_base_editor_get_display_name (editor, gchild_new);
	
	glade_base_editor_store_set (editor, &new_iter, 
			    GLADE_BASE_EDITOR_MENU_GWIDGET, gchild_new,
			    GLADE_BASE_EDITOR_MENU_OBJECT, glade_widget_get_object (gchild_new),
			    GLADE_BASE_EDITOR_MENU_TYPE_NAME, type_name,
			    GLADE_BASE_EDITOR_MENU_NAME, name,
			    -1);

	glade_base_editor_reorder_children (editor, &new_iter);
	
	gtk_tree_view_expand_all (GTK_TREE_VIEW (e->treeview));
	glade_base_editor_set_cursor (editor, &new_iter);
	
	glade_command_pop_group ();
	
	glade_base_editor_block_callbacks (editor, FALSE);
	
	g_free (name);
	g_free (type_name);
}

static void
glade_base_editor_add_item_activate (GtkMenuItem *menuitem, GladeBaseEditor *e)
{
	GObject *item = G_OBJECT (menuitem);
	GType type = GPOINTER_TO_INT (g_object_get_data (item, "object_type"));
	gboolean as_child = GPOINTER_TO_INT (g_object_get_data (item, "object_as_child"));
	
	glade_base_editor_add_child (e, type, as_child);
}

static void
glade_base_editor_add_activate (GtkButton *button,  GladeBaseEditor *e)
{
	if (e->priv->add_type)
		glade_base_editor_add_child (e, e->priv->add_type, e->priv->add_as_child);
}

static void
glade_base_editor_delete_child (GladeBaseEditor *e)
{
	GladeWidget *child, *gparent;
	GtkTreeIter iter, parent;
	gboolean retval;

	if (glade_base_editor_get_child_selected (e, &iter) == FALSE) return;

	gtk_tree_model_get (e->priv->model, &iter,
			    GLADE_BASE_EDITOR_MENU_GWIDGET, &child, -1);
		
	if (gtk_tree_model_iter_parent (e->priv->model, &parent, &iter))
		gtk_tree_model_get (e->priv->model, &parent,
				    GLADE_BASE_EDITOR_MENU_GWIDGET, &gparent,
				    -1);
	else
		gparent = e->priv->gcontainer;
	
	glade_command_push_group (_("Delete %s child from %s"),
				  glade_widget_get_name (child),
				  glade_widget_get_name (gparent));
	
	/* Emit delete-child signal */
	g_signal_emit (e, glade_base_editor_signals[SIGNAL_DELETE_CHILD],
		       0, gparent, child, &retval);

	glade_command_pop_group ();
}


static gboolean
glade_base_editor_treeview_key_press_event (GtkWidget *widget,
					    GdkEventKey *event,
					    GladeBaseEditor *e)
{
	if (event->keyval == GDK_Delete)
		glade_base_editor_delete_child (e);
	
	return FALSE;
}

static void
glade_base_editor_delete_activate (GtkButton *button, GladeBaseEditor *e)
{
	glade_base_editor_delete_child (e);
}

static gboolean
glade_base_editor_is_child (GladeBaseEditor *e,
			    GladeWidget *gchild,
			    gboolean valid_type)
{
	GladeWidget *gcontainer;
	
	if (valid_type)
	{
		GObject *child = glade_widget_get_object (gchild);
		
		gcontainer = e->priv->gcontainer;
		
		if (gchild->internal ||
		    glade_base_editor_get_type_info (e, NULL,
						     G_OBJECT_TYPE (child),
						     -1) == FALSE)
			return FALSE;
	}
	else
	{
		GtkTreeIter iter;
		if (glade_base_editor_get_child_selected (e, &iter))
			gtk_tree_model_get (e->priv->model, &iter,
					    GLADE_BASE_EDITOR_MENU_GWIDGET, &gcontainer,
					    -1);
		else
			return FALSE;
	}
		
	while ((gchild = glade_widget_get_parent (gchild)))
		if (gchild == gcontainer) return TRUE;

	return FALSE;
}

static gboolean
glade_base_editor_update_treeview_idle (gpointer data)
{
	GladeBaseEditor *e = ((GladeBaseEditor *) data);
	GList *selection = glade_project_selection_get (e->priv->project);
	
	glade_base_editor_block_callbacks (e, TRUE);
	
	glade_base_editor_fill_store (e);
	glade_base_editor_clear (e);

	gtk_tree_view_expand_all (GTK_TREE_VIEW (e->priv->treeview));
	
	if (selection)
	{
		GladeWidget *widget = glade_widget_get_from_gobject (G_OBJECT (selection->data)); 
		if (glade_base_editor_is_child (e, widget, TRUE))
			glade_base_editor_select_child (e, widget);
	}
	
	e->priv->updating_treeview = FALSE;
	glade_base_editor_block_callbacks (e, FALSE);
	
	return FALSE;
}

static void
glade_base_editor_project_widget_name_changed (GladeProject *project,
					       GladeWidget  *widget,
					       GladeBaseEditor *editor)
{
	GladeWidget *selected_child;
	GtkTreeIter iter;

	if (glade_base_editor_get_child_selected (editor, &iter))
	{
		gtk_tree_model_get (editor->priv->model, &iter,
				    GLADE_BASE_EDITOR_MENU_GWIDGET, &selected_child,
				    -1);
		if (widget == selected_child)
			glade_base_editor_update_properties (editor);
	}
}

static void
glade_base_editor_project_closed (GladeProject *project, GladeBaseEditor *e)
{
	glade_base_editor_set_container (e, NULL);
}

static void
glade_base_editor_reorder (GladeBaseEditor *editor, GtkTreeIter *iter)
{
	GladeBaseEditorPrivate *e = editor->priv;
	GladeWidget *gchild, *gparent;
	GtkTreeIter parent_iter;
	gboolean retval;
	
	glade_command_push_group (_("Reorder %s's children"),
				  glade_widget_get_name (e->gcontainer));
	
	gtk_tree_model_get (e->model, iter,
			    GLADE_BASE_EDITOR_MENU_GWIDGET, &gchild, -1);
	
	if (gtk_tree_model_iter_parent (e->model, &parent_iter, iter))
		gtk_tree_model_get (e->model, &parent_iter,
				    GLADE_BASE_EDITOR_MENU_GWIDGET, &gparent,
				    -1);
	else
		gparent = e->gcontainer;

	g_signal_emit (editor, glade_base_editor_signals [SIGNAL_MOVE_CHILD],
		       0, gparent, gchild, &retval);

	if (retval)
		glade_base_editor_reorder_children (editor, iter);
	else
	{
		glade_base_editor_clear (editor);
		glade_base_editor_fill_store (editor);
		glade_base_editor_find_child (editor, gchild, &editor->priv->iter);
	}

	glade_command_pop_group ();
}

static gboolean 
glade_base_editor_drag_and_drop_idle (gpointer data)
{
	GladeBaseEditor *e = (GladeBaseEditor *) data;
	
	glade_base_editor_reorder (e, &e->priv->iter);
	gtk_tree_view_expand_all (GTK_TREE_VIEW (e->priv->treeview));
	glade_base_editor_set_cursor (e, &e->priv->iter);
	glade_base_editor_block_callbacks (e, FALSE);
	
	return FALSE;
}

static void
glade_base_editor_row_inserted (GtkTreeModel *model,
				GtkTreePath *path,
				GtkTreeIter *iter,
				GladeBaseEditor *e)
{
	e->priv->iter = *iter;
	glade_base_editor_block_callbacks (e, TRUE);
	g_idle_add (glade_base_editor_drag_and_drop_idle, e);
}

static void
glade_base_editor_project_remove_widget (GladeProject *project,
					 GladeWidget  *widget,
					 GladeBaseEditor *e)
{
	if (widget == e->priv->gcontainer)
	{
		glade_base_editor_set_container (e, NULL);
		return;
	}

	if (glade_base_editor_is_child (e, widget, TRUE))
	{
		GtkTreeIter iter;
		if (glade_base_editor_find_child (e, widget, &iter))
		{
			glade_base_editor_store_remove (e, &iter);
			glade_base_editor_clear (e);
		}
	}
	
	if (widget->internal &&
	    glade_base_editor_is_child (e, widget, FALSE))
		glade_base_editor_update_properties (e);
}

static void
glade_base_editor_project_add_widget (GladeProject *project,
				      GladeWidget  *widget,
				      GladeBaseEditor *e)
{
	if (e->priv->updating_treeview) return;
		
	if (glade_base_editor_is_child (e, widget, TRUE))
	{
		e->priv->updating_treeview = TRUE;
		g_idle_add (glade_base_editor_update_treeview_idle, e);
	}
	
	if (widget->internal &&
	    glade_base_editor_is_child (e, widget, FALSE))
		glade_base_editor_update_properties (e);
}

static gboolean
glade_base_editor_update_display_name (GtkTreeModel *model,
				       GtkTreePath *path,
				       GtkTreeIter *iter,
				       gpointer data)
{
	GladeBaseEditor *editor = (GladeBaseEditor *) data;
	GladeWidget *gchild;
	gchar *name;
	
	gtk_tree_model_get (model, iter,
			    GLADE_BASE_EDITOR_MENU_GWIDGET, &gchild,
			    -1);
	
	name = glade_base_editor_get_display_name (editor, gchild);
		
	glade_base_editor_store_set (editor, iter,
				     GLADE_BASE_EDITOR_MENU_NAME, name,
				     -1);
	g_free (name);
	
	return FALSE;
}

static void
glade_base_editor_project_changed (GladeProject *project,
				   GladeCommand *command,
				   gboolean      forward,
				   GladeBaseEditor *editor)
{
	gtk_tree_model_foreach (editor->priv->model,
				glade_base_editor_update_display_name,
				editor);
}



static void
glade_base_editor_project_disconnect (GladeBaseEditor *editor)
{
	GladeBaseEditorPrivate *e = editor->priv;
	
	if (e->project == NULL) return;

	g_signal_handlers_disconnect_by_func (e->project,
			glade_base_editor_project_closed, editor);
		
	g_signal_handlers_disconnect_by_func (e->project,
			glade_base_editor_project_remove_widget, editor);
		
	g_signal_handlers_disconnect_by_func (e->project,
			glade_base_editor_project_add_widget, editor);
		
	g_signal_handlers_disconnect_by_func (e->project,
			glade_base_editor_project_widget_name_changed, editor);
	
	g_signal_handlers_disconnect_by_func (e->project,
			glade_base_editor_project_changed, editor);
}

static void
glade_base_editor_set_container (GladeBaseEditor *editor,
				 GObject *container)
{
	GladeBaseEditorPrivate *e = editor->priv;

	if (e->project)
		glade_base_editor_project_disconnect (editor);
	
	if (container == NULL)
	{
		e->gcontainer = NULL;
		e->project = NULL;
		glade_base_editor_block_callbacks (editor, TRUE);
		glade_base_editor_clear (editor);
		glade_base_editor_store_clear (editor);
		gtk_list_store_clear (e->children);
		gtk_widget_set_sensitive (e->paned, FALSE);
		glade_base_editor_block_callbacks (editor, FALSE);
		return;
	}
	
	gtk_widget_set_sensitive (e->paned, TRUE);
	
	e->gcontainer = glade_widget_get_from_gobject (container);
	
	e->project = glade_widget_get_project (e->gcontainer);

	g_signal_connect (e->project, "close",
			  G_CALLBACK (glade_base_editor_project_closed),
			  editor);
	
	g_signal_connect (e->project, "remove-widget",
			  G_CALLBACK (glade_base_editor_project_remove_widget),
			  editor);
	
	g_signal_connect (e->project, "add-widget",
			  G_CALLBACK (glade_base_editor_project_add_widget),
			  editor);
	
	g_signal_connect (e->project, "widget-name-changed",
			  G_CALLBACK (glade_base_editor_project_widget_name_changed),
			  editor);
		  
	g_signal_connect (e->project, "changed",
			  G_CALLBACK (glade_base_editor_project_changed),
			  editor);
}

/*************************** GladeBaseEditor Class ****************************/

static void
glade_base_editor_finalize (GObject *object)
{
	GladeBaseEditor *cobj = GLADE_BASE_EDITOR (object);
	
	/* Free private members, etc. */
	glade_base_editor_project_disconnect (cobj);
	
	g_free (cobj->priv);
	G_OBJECT_CLASS(parent_class)->finalize(object);
}

/* Default handlers */
static gboolean
glade_base_editor_change_type (GladeBaseEditor *editor,
			       GladeWidget *gchild,
			       GType type)
{
	GladeBaseEditorPrivate *e = editor->priv;
	GladeWidgetAdaptor     *adaptor = glade_widget_adaptor_get_by_type (type);
	GladeWidget            *parent, *gchild_new;
	GList                   list = {0, }, *children, *l;
	GObject                *child, *child_new;
	GtkTreeIter             iter;
	gchar                  *name, *class_name;

	if (glade_base_editor_get_type_info (editor, NULL, type,
					     GLADE_BASE_EDITOR_NAME, &class_name,
					     -1) == FALSE)
		return TRUE;
	
	parent = glade_widget_get_parent (gchild);
	child = glade_widget_get_object (gchild);
	name = g_strdup (glade_widget_get_name (gchild));
	glade_base_editor_find_child (editor, gchild, &iter);
	
	/* Create new widget */
	gchild_new = glade_command_create (adaptor, parent, NULL, e->project);
	child_new = glade_widget_get_object (gchild_new);

	/* Cut and Paste childrens */
	if ((children = glade_widget_adaptor_get_children (adaptor, child)))
	{
		GList *gchildren = NULL;
		
		l = children;
		while (l)
		{
			GladeWidget *w = glade_widget_get_from_gobject (l->data);
			
			if (w && !w->internal)
				gchildren = g_list_prepend (gchildren, w);

			l= g_list_next (l);
		}
		
		if (gchildren)
		{
			glade_command_cut (gchildren);
			glade_command_paste (gchildren, gchild_new, NULL);
		
			g_list_free (children);
			g_list_free (gchildren);
		}
	}
	
	/* Copy properties */
	glade_widget_copy_properties (gchild_new, gchild);
	
	/* Delete old widget */
	list.data = gchild;
	glade_command_delete (&list);
	
	/* Apply packing properties to the new object */
	l = gchild->packing_properties;
	while (l)
	{
		GladeProperty *orig_prop = (GladeProperty *) l->data;
		GladeProperty *dup_prop = glade_widget_get_property (gchild_new,
								     orig_prop->class->id);
		glade_property_set_value (dup_prop, orig_prop->value);
		l = g_list_next (l);
	}

	/* Set the name */
	glade_widget_set_name (gchild_new, name);
	
	if (GTK_IS_WIDGET (child_new))
		gtk_widget_show_all (GTK_WIDGET (child_new));
	
	glade_base_editor_store_set (editor, &iter,
			    GLADE_BASE_EDITOR_MENU_GWIDGET, gchild_new,
			    GLADE_BASE_EDITOR_MENU_OBJECT, child_new,
			    GLADE_BASE_EDITOR_MENU_TYPE_NAME, class_name,
			    -1);
	g_free (class_name);
	g_free (name);
	
	return TRUE;
}

static gchar *
glade_base_editor_get_display_name_impl (GladeBaseEditor *editor,
					 GladeWidget *gchild)
{
	return g_strdup (glade_widget_get_name (gchild));
}

static GladeWidget *
glade_base_editor_build_child (GladeBaseEditor *editor,
			       GladeWidget *gparent,
			       GType type)
{
	return glade_command_create (glade_widget_adaptor_get_by_type (type),
				     gparent, NULL,
				     glade_widget_get_project (gparent));
}

static gboolean
glade_base_editor_move_child (GladeBaseEditor *editor,
			      GladeWidget *gparent,
			      GladeWidget *gchild)
{
	GList list = {0, };

	if (gparent != glade_widget_get_parent (gchild))
	{
		list.data = gchild;
		glade_command_cut (&list);
		glade_command_paste (&list, gparent, NULL);
	}

	return TRUE;
}

static gboolean
glade_base_editor_delete_child_impl (GladeBaseEditor *editor,
				     GladeWidget *gparent,
				     GladeWidget *gchild)
{
	GList list = {0, };

	list.data = gchild;
	glade_command_delete (&list);
	
	return TRUE;
}

static void
glade_base_editor_class_init (GladeBaseEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	
	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = glade_base_editor_finalize;

	klass->change_type = glade_base_editor_change_type;
	klass->get_display_name = glade_base_editor_get_display_name_impl;
	klass->build_child = glade_base_editor_build_child;
	klass->delete_child = glade_base_editor_delete_child_impl;
	klass->move_child = glade_base_editor_move_child;

	/**
	 * GladeBaseEditor::child-selected:
	 * @gladebaseeditor: the #GladeBaseEditor which received the signal.
	 * @gchild: the selected #GladeWidget.
	 *
	 * Emited when the user selects a child in the editor's treeview.
	 * You can add the relevant child properties here using 
	 * glade_base_editor_add_default_properties() and glade_base_editor_add_properties() 
	 * You can also add labels with glade_base_editor_add_label to make the
	 * editor look pretty.
	 */
	glade_base_editor_signals [SIGNAL_CHILD_SELECTED] =
		g_signal_new ("child-selected",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeBaseEditorClass, child_selected),
			      NULL, NULL,
			      glade_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_OBJECT);

	/**
	 * GladeBaseEditor::child-change-type:
	 * @gladebaseeditor: the #GladeBaseEditor which received the signal.
	 * @child: the #GObject being changed.
         * @type: the new type for @child.
	 *
	 * Returns TRUE to stop signal emision.
	 */
	glade_base_editor_signals [SIGNAL_CHANGE_TYPE] =
		g_signal_new ("change-type",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeBaseEditorClass, change_type),
			      glade_boolean_handled_accumulator, NULL,
			      glade_marshal_BOOLEAN__OBJECT_UINT,
			      G_TYPE_BOOLEAN,
			      2,
			      G_TYPE_OBJECT, G_TYPE_UINT);

	/**
	 * GladeBaseEditor::get-display-name:
	 * @gladebaseeditor: the #GladeBaseEditor which received the signal.
	 * @gchild: the child to get display name string to show in @gladebaseeditor
	 * treeview.
	 *
	 * Returns a newly allocated string.
	 */
	glade_base_editor_signals [SIGNAL_GET_DISPLAY_NAME] =
		g_signal_new ("get-display-name",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeBaseEditorClass, get_display_name),
			      glade_string_accumulator, NULL,
			      glade_marshal_STRING__OBJECT,
			      G_TYPE_STRING,
			      1,
			      G_TYPE_OBJECT);

	/**
	 * GladeBaseEditor::build-child:
	 * @gladebaseeditor: the #GladeBaseEditor which received the signal.
	 * @gparent: the parent of the new child
	 * @type: the #GType of the child
	 *
	 * Create a child widget here if something else must be done other than
	 * calling glade_command_create() such as creating an intermediate parent.
	 * Returns the newly created #GladeWidget or NULL if child cant be created
	 */
	glade_base_editor_signals [SIGNAL_BUILD_CHILD] =
		g_signal_new ("build-child",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeBaseEditorClass, build_child),
			      glade_stop_emission_accumulator, NULL,
			      glade_marshal_OBJECT__OBJECT_UINT,
			      G_TYPE_OBJECT,
			      2,
			      G_TYPE_OBJECT, G_TYPE_UINT);

	/**
	 * GladeBaseEditor::delete-child:
	 * @gladebaseeditor: the #GladeBaseEditor which received the signal.
	 * @gparent: the parent
	 * @gchild: the child to delete
	 */
	glade_base_editor_signals [SIGNAL_DELETE_CHILD] =
		g_signal_new ("delete-child",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeBaseEditorClass, delete_child),
			      glade_boolean_handled_accumulator, NULL,
			      glade_marshal_BOOLEAN__OBJECT_OBJECT,
			      G_TYPE_BOOLEAN,
			      2,
			      G_TYPE_OBJECT, G_TYPE_OBJECT);

	/**
	 * GladeBaseEditor::move-child:
	 * @gladebaseeditor: the #GladeBaseEditor which received the signal.
	 * @gparent: the new parent of @gchild
	 * @gchild: the #GladeWidget to move
	 *
	 * Move child here if something else must be done other than cut & paste.
	 * Returns wheater child has been sucessfully moved or not.
	 */
	glade_base_editor_signals [SIGNAL_MOVE_CHILD] =
		g_signal_new ("move-child",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeBaseEditorClass, move_child),
			      glade_stop_emission_accumulator, NULL,
			      glade_marshal_BOOLEAN__OBJECT_OBJECT,
			      G_TYPE_BOOLEAN,
			      2,
			      G_TYPE_OBJECT, G_TYPE_OBJECT);
}

static void
glade_base_editor_block_callbacks (GladeBaseEditor *editor, gboolean block)
{
	GladeBaseEditorPrivate *e = editor->priv;
	if (block)
	{
		g_signal_handlers_block_by_func (e->model, glade_base_editor_row_inserted, editor);
		if (e->project)
		{
			g_signal_handlers_block_by_func (e->project, glade_base_editor_project_remove_widget, editor);
			g_signal_handlers_block_by_func (e->project, glade_base_editor_project_add_widget, editor);
			g_signal_handlers_block_by_func (e->project, glade_base_editor_project_changed, editor);
		}
	}
	else
	{
		g_signal_handlers_unblock_by_func (e->model, glade_base_editor_row_inserted, editor);
		if (e->project)
		{
			g_signal_handlers_unblock_by_func (e->project, glade_base_editor_project_remove_widget, editor);
			g_signal_handlers_unblock_by_func (e->project, glade_base_editor_project_add_widget, editor);
			g_signal_handlers_unblock_by_func (e->project, glade_base_editor_project_changed, editor);
		}
	}
}

static void
glade_base_editor_realize_callback (GtkWidget *widget, gpointer user_data)
{
	GladeBaseEditor *editor = GLADE_BASE_EDITOR (widget);
	
	glade_base_editor_block_callbacks (editor, TRUE);

	glade_base_editor_fill_store (editor);
	gtk_tree_view_expand_all (GTK_TREE_VIEW (editor->priv->treeview));
	
	glade_base_editor_block_callbacks (editor, FALSE);
}

static void
glade_base_editor_init (GladeBaseEditor *editor)
{
	GladeBaseEditorPrivate *e;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkWidget *paned, *hbox, *vbox, *tree_vbox, *scroll, *button_table, *button;
	
	gtk_box_set_spacing (GTK_BOX (editor), 8);
	
	e = editor->priv = g_new0(GladeBaseEditorPrivate, 1);
	
	/* Children store */
	e->children = gtk_list_store_new (GLADE_BASE_EDITOR_N_COLUMNS,
					  G_TYPE_UINT, G_TYPE_STRING);

	/* Paned */
	e->paned = paned = gtk_vpaned_new ();
	gtk_widget_show (paned);
	gtk_box_pack_start (GTK_BOX (editor), paned, TRUE, TRUE, 0);
	
	/* Hbox */
	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_paned_pack1 (GTK_PANED (paned), hbox, TRUE, FALSE);
		
	/* TreeView Vbox */
	tree_vbox = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (tree_vbox);
	gtk_box_pack_start (GTK_BOX (hbox), tree_vbox, FALSE, TRUE, 0);
	
	/* ScrolledWindow */
	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scroll);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_box_pack_start (GTK_BOX (tree_vbox), scroll, TRUE, TRUE, 0);

	/* TreeView */
	e->treeview = gtk_tree_view_new ();
	gtk_widget_show (e->treeview);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (e->treeview), TRUE);
	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (e->treeview), TRUE);

	gtk_widget_add_events (e->treeview, GDK_KEY_PRESS_MASK);
	g_signal_connect (e->treeview, "key-press-event",
			  G_CALLBACK (glade_base_editor_treeview_key_press_event), editor);

	g_signal_connect (e->treeview, "cursor-changed",
			  G_CALLBACK (glade_base_editor_treeview_cursor_changed), editor);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Label"), renderer,
						"text", GLADE_BASE_EDITOR_MENU_NAME, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (e->treeview), column);
	
	renderer = gtk_cell_renderer_combo_new ();
	g_object_set (renderer,
			"model", e->children,
			"text-column", GLADE_BASE_EDITOR_NAME,
			"has-entry", FALSE,
			"editable", TRUE,
			NULL);
	g_signal_connect (renderer, "edited", 
			  G_CALLBACK (glade_base_editor_child_type_edited), editor);
	column = gtk_tree_view_column_new_with_attributes (_("Type"), renderer,
						"text", GLADE_BASE_EDITOR_MENU_TYPE_NAME, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (e->treeview), column);

	gtk_container_add (GTK_CONTAINER (scroll), e->treeview);
	
	/* Add/Remove buttons */
	button_table = gtk_table_new (1, 2, TRUE);
	gtk_widget_show (button_table);
	gtk_table_set_col_spacings (GTK_TABLE (button_table), 8);
	gtk_box_pack_start (GTK_BOX (tree_vbox), button_table, FALSE, TRUE, 0);
	
	button = gtk_button_new_from_stock (GTK_STOCK_ADD);
	gtk_widget_show (button);
	g_signal_connect (button, "clicked", 
			  G_CALLBACK (glade_base_editor_add_activate), editor);
	gtk_table_attach_defaults (GTK_TABLE (button_table), button, 0, 1, 0, 1);

	e->remove_button = button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
	gtk_widget_show (button);
	g_signal_connect (button, "clicked", 
			  G_CALLBACK (glade_base_editor_delete_activate), editor);
	gtk_table_attach_defaults (GTK_TABLE (button_table), button, 1, 2, 0, 1);

	/* Properties Vbox */
	vbox = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
	
	/* Tables */
	e->table = gtk_table_new (1, 2, FALSE);
	gtk_widget_show (e->table);
	gtk_table_set_row_spacings (GTK_TABLE (e->table), 4);
	gtk_box_pack_start (GTK_BOX (vbox), e->table, FALSE, TRUE, 0);

	/* Signal Editor */
	e->signal_editor = glade_signal_editor_new (NULL);
	e->signal_editor_w = glade_signal_editor_get_widget (e->signal_editor);
	gtk_widget_show (e->signal_editor_w);
	gtk_widget_set_size_request (e->signal_editor_w, -1, 96);
	gtk_paned_pack2 (GTK_PANED (paned), e->signal_editor_w, FALSE, FALSE);
	
	/* Update the treeview on realize event */
	g_signal_connect (editor, "realize",
			  G_CALLBACK (glade_base_editor_realize_callback),
			  NULL);
}

/********************************* Public API *********************************/
GType
glade_base_editor_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (GladeBaseEditorClass),
			NULL,
			NULL,
			(GClassInitFunc)glade_base_editor_class_init,
			NULL,
			NULL,
			sizeof (GladeBaseEditor),
			0,
			(GInstanceInitFunc)glade_base_editor_init,
		};

		type = g_type_register_static (GTK_TYPE_VBOX, "GladeBaseEditor",
					       &our_info, 0);
	}

	return type;
}

/**
 * glade_base_editor_new:
 * @container: the container this new editor will edit.
 * @tree_like: TRUE if container's children can have children.
 * @... A NULL terminated list of gchar *, GType
 * 
 * Creates a new GladeBaseEditor with support for all the object types indicated
 * in the variable argument list.
 * Argument List:
 *   o The type name
 *   o The GType the editor will support
 *
 * Returns a new GladeBaseEditor.
 */
GladeBaseEditor *
glade_base_editor_new (GObject *container, gboolean tree_like, ...)
{
	GladeBaseEditor *editor;
	GladeBaseEditorPrivate *e;
	va_list args;
	gchar *name;
	GtkTreeIter iter;
	
	g_return_val_if_fail (GTK_IS_CONTAINER (container), NULL);
	
	editor = GLADE_BASE_EDITOR (g_object_new (GLADE_TYPE_BASE_EDITOR, NULL));
	e = editor->priv;
	
	/* Store */
	if (tree_like)
	{
		e->tstore = gtk_tree_store_new (GLADE_BASE_EDITOR_MENU_N_COLUMNS,
						G_TYPE_OBJECT,
						G_TYPE_OBJECT,
						G_TYPE_STRING,
						G_TYPE_STRING);
		e->model = GTK_TREE_MODEL (e->tstore);
	}
	else
	{
		e->lstore = gtk_list_store_new (GLADE_BASE_EDITOR_MENU_N_COLUMNS,
						G_TYPE_OBJECT,
						G_TYPE_OBJECT,
						G_TYPE_STRING,
						G_TYPE_STRING);
		e->model = GTK_TREE_MODEL (e->lstore);
	}

	gtk_tree_view_set_model	 (GTK_TREE_VIEW (e->treeview), e->model);
	gtk_tree_view_expand_all (GTK_TREE_VIEW (e->treeview));
	
	g_signal_connect (e->model, "row-inserted",
			  G_CALLBACK (glade_base_editor_row_inserted), 
			  editor);
	
	va_start (args, tree_like);
	
	while ((name = va_arg (args, gchar *)))
	{
		gtk_list_store_append (editor->priv->children, &iter);
		gtk_list_store_set (editor->priv->children, &iter,
				    GLADE_BASE_EDITOR_GTYPE, va_arg (args, GType),
				    GLADE_BASE_EDITOR_NAME, name,
				    -1);
	}

	va_end (args);
	
	glade_base_editor_set_container (editor, container);
	
	return editor;
}


/**
 * glade_base_editor_add_default_properties:
 * @editor: a #GladeBaseEditor
 * @gchild: a #GladeWidget
 *
 * Add @gchild name and type property to @editor
 * 
 * NOTE: This function is intended to be used in "child-selected" callbacks
 */
void
glade_base_editor_add_default_properties (GladeBaseEditor *editor,
					  GladeWidget *gchild)
{
	GladeBaseEditorPrivate *e = editor->priv;
	GtkTreeIter combo_iter;
	GtkWidget *label, *entry;
	GtkTreeModel *child_class = GTK_TREE_MODEL (e->children);
	GtkCellRenderer *renderer;
	gboolean retval;
	GObject *child = glade_widget_get_object (gchild);
	
	g_return_if_fail (GLADE_IS_BASE_EDITOR (editor));
	g_return_if_fail (GLADE_IS_WIDGET (gchild));
	
	/* Name */
	label = gtk_label_new (_("Name :"));
	gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.0);
	
	entry = gtk_entry_new ();
	gtk_entry_set_text (GTK_ENTRY (entry), glade_widget_get_name (gchild));
	g_signal_connect (entry, "activate", G_CALLBACK (glade_base_editor_name_activate), gchild);
	g_signal_connect (entry, "focus-out-event", G_CALLBACK (glade_base_editor_name_focus_out), gchild);
	glade_base_editor_table_attach (editor, label, entry);

	/* Type */
	label = gtk_label_new (_("Type :"));
	gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.0);
	
	entry = gtk_combo_box_new ();
	gtk_combo_box_set_model (GTK_COMBO_BOX (entry), child_class);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (entry), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (entry), renderer, "text",
					GLADE_BASE_EDITOR_NAME, NULL);
					
	if ((retval = glade_base_editor_get_type_info (editor, &combo_iter, G_OBJECT_TYPE (child), -1)))
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (entry), &combo_iter);
	
	g_signal_connect (entry, "changed", G_CALLBACK (glade_base_editor_type_changed), editor);
	glade_base_editor_table_attach (editor, label, entry);
}

/**
 * glade_base_editor_add_properties:
 * @editor: a #GladeBaseEditor
 * @gchild: a #GladeWidget
 * @...: A NULL terminated list of properties names.
 * 
 * Add @gchild properties to @editor
 *
 * NOTE: This function is intended to be used in "child-selected" callbacks
 */
void
glade_base_editor_add_properties (GladeBaseEditor *editor,
				  GladeWidget *gchild,
				  ...)
{
	GladeEditorProperty *eprop;
	va_list args;
	gchar *property;
	
	g_return_if_fail (GLADE_IS_BASE_EDITOR (editor));
	g_return_if_fail (GLADE_IS_WIDGET (gchild));
	
	va_start (args, gchild);
	property = va_arg (args, gchar *);
	
	while (property)
	{
		eprop = glade_editor_property_new_from_widget (gchild, property, TRUE);
		if (eprop)
			glade_base_editor_table_attach (editor, 
							GLADE_EDITOR_PROPERTY (eprop)->eventbox, 
							GTK_WIDGET (eprop));
		property = va_arg (args, gchar *);
	}
}

/**
 * glade_base_editor_add_label:
 * @editor: a #GladeBaseEditor
 * @str: the label string
 *
 * Adds a new label to @editor
 *
 * NOTE: This function is intended to be used in "child-selected" callbacks
 */
void
glade_base_editor_add_label (GladeBaseEditor *editor, gchar *str)
{
	GtkWidget *label;
	gchar *markup;
	gint row;
	
	g_return_if_fail (GLADE_IS_BASE_EDITOR (editor));
	g_return_if_fail (str != NULL);
	
	label = gtk_label_new (NULL);
	markup = g_strdup_printf ("<span rise=\"-20000\"><b>%s</b></span>", str);
	row = editor->priv->row;
	
	gtk_label_set_markup (GTK_LABEL (label), markup);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_table_attach (GTK_TABLE (editor->priv->table), label, 0, 2, row, row + 1,
			  GTK_FILL, GTK_EXPAND | GTK_FILL, 2, 0);
	gtk_widget_show (label);
	editor->priv->row++;
	
	g_free (markup);
}

/**
 * glade_base_editor_add_popup_items:
 * @editor: a #GladeBaseEditor
 * @...: a NULL terminated list of gchar *, #GType, gboolean
 *
 * Adds a new popup item to the editor.
 * Three parameters are needed for each new popup item:
 *  o the popup item's label
 *  o the object type this popup item will create
 *  o whether this popup item will add the new object as child
 */
void
glade_base_editor_add_popup_items (GladeBaseEditor *editor, ...)
{
	va_list args;
	GtkMenuShell *menu;
	GtkWidget *item;
	gchar *label;
	GType type;
	gboolean as_child;
	
	g_return_if_fail (GLADE_IS_BASE_EDITOR (editor));
	
	if (editor->priv->popup == NULL)
	{
		/* Create PopUp */
		editor->priv->popup = gtk_menu_new ();
		gtk_widget_show (editor->priv->popup);
		g_signal_connect (editor->priv->treeview, "button-press-event", 
				  G_CALLBACK (glade_base_editor_popup_handler),
				  editor);
	}

	menu = GTK_MENU_SHELL (editor->priv->popup);
	
	va_start (args, editor);
	
	while ((label = va_arg (args, gchar *)))
	{
		type = va_arg (args, GType);
		as_child = va_arg (args, gboolean);
		
		if (!glade_base_editor_get_type_info (editor, NULL, type, -1))
			continue;
		
		item = gtk_menu_item_new_with_label (label);
		gtk_widget_show (item);
		
		g_object_set_data (G_OBJECT (item), "object_type",
				   GINT_TO_POINTER (type));
		
		g_object_set_data (G_OBJECT (item), "object_as_child",
				   GINT_TO_POINTER (as_child));
		
		if (editor->priv->add_type == 0)
		{
			editor->priv->add_type = type;
			editor->priv->add_as_child = as_child;
		}
		
		g_signal_connect (item, "activate", 
				  G_CALLBACK (glade_base_editor_add_item_activate), editor);
		gtk_menu_shell_append (menu, item);
	}
}

/**
 * glade_base_editor_set_show_signal_editor:
 * @editor: a #GladeBaseEditor
 * @val:
 *
 * Shows/hide @editor 's signal editor
 */
void
glade_base_editor_set_show_signal_editor (GladeBaseEditor *editor, gboolean val)
{
	g_return_if_fail (GLADE_IS_BASE_EDITOR (editor));
	
	if (val)
		gtk_widget_show (editor->priv->signal_editor_w);
	else
		gtk_widget_hide (editor->priv->signal_editor_w);
}

/* Convenience functions */

static void
glade_base_editor_help (GtkButton *button, gchar *markup)
{
	GtkWidget *dialog;
	
	dialog = gtk_message_dialog_new (glade_app_get_transient_parent (),
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_INFO,
					 GTK_BUTTONS_CLOSE, " ");
	
	gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), markup);
	
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

/**
 * glade_base_editor_pack_new_window:
 * @editor: a #GladeBaseEditor
 * @title: the window title
 * @help_markup: the help text
 *
 * This convenience function create a new modal window and packs @editor in it.
 * Returns the newly created window
 */ 
GtkWidget *
glade_base_editor_pack_new_window (GladeBaseEditor *editor,
				   gchar *title,
				   gchar *help_markup)
{
	GtkWidget *window, *buttonbox, *button;
	gchar *real_title;
	
	g_return_val_if_fail (GLADE_IS_BASE_EDITOR (editor), NULL);
	
	/* Window */
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DIALOG);
	
	if (title)
	{
		real_title = g_strdup_printf ("%s - %s", title, 
					      glade_widget_get_name (editor->priv->gcontainer));
		gtk_window_set_title (GTK_WINDOW (window), real_title);
		g_free (real_title);
	}
	
	/* Button Box */
	buttonbox = gtk_hbutton_box_new ();
	gtk_widget_show (buttonbox);
	gtk_button_box_set_layout (GTK_BUTTON_BOX (buttonbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing (GTK_BOX (buttonbox), 8);
	gtk_box_pack_start (GTK_BOX (editor), buttonbox, FALSE, TRUE, 0);

	button = glade_app_undo_button_new ();
	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (buttonbox), button);

	button = glade_app_redo_button_new ();
	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (buttonbox), button);

	button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	gtk_widget_show (button);
	g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_widget_destroy), window);
	gtk_container_add (GTK_CONTAINER (buttonbox), button);

	if (help_markup)
	{
		button = gtk_button_new_from_stock (GTK_STOCK_HELP);
		gtk_widget_show (button);
		g_signal_connect (button, "clicked",
				  G_CALLBACK (glade_base_editor_help),
				  help_markup);
		gtk_container_add (GTK_CONTAINER (buttonbox), button);
		gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (buttonbox), button, TRUE);
	}
	
	gtk_container_set_border_width (GTK_CONTAINER (editor), GLADE_GENERIC_BORDER_WIDTH);
	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (editor));
	
	gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);
	
	return window;
}
