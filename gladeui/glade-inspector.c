/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * glade-inspector.h
 *
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2007 Vincent Geddes
 *
 * Authors:
 *   Chema Celorio
 *   Tristan Van Berkom <tvb@gnome.org>
 *   Vincent Geddes <vincent.geddes@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <config.h>

/**
 * SECTION:glade-inspector
 * @Short_Description: A widget for inspecting objects in a #GladeProject.
 *
 * A #GladeInspector is a widget for inspecting the objects that make up a user interface. 
 *
 * An inspector is created by calling either glade_inspector_new() or glade_inspector_new_with_project(). 
 * The current project been inspected can be changed by calling glade_inspector_set_project().
 */

#include "glade.h"
#include "glade-widget.h"
#include "glade-project.h"
#include "glade-widget-adaptor.h"
#include "glade-inspector.h"
#include "glade-popup.h"
#include "glade-app.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#define GLADE_INSPECTOR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object),\
					    GLADE_TYPE_INSPECTOR,                 \
			 		    GladeInspectorPrivate))

enum
{
	PROP_0,
	PROP_PROJECT
};

enum
{
	SELECTION_CHANGED,
	ITEM_ACTIVATED,
	LAST_SIGNAL
};		

struct _GladeInspectorPrivate
{
	GtkWidget    *view;
	GtkTreeModel *filter;

	GladeProject *project;

	GtkWidget    *entry;
	GCompletion * completion;
	guint idle_complete;
	gboolean      search_disabled;
};


static guint glade_inspector_signals[LAST_SIGNAL] = {0};


static void     glade_inspector_dispose     (GObject           *object);
static void     glade_inspector_finalize    (GObject           *object);
static void     add_columns                 (GtkTreeView       *inspector);
static void     item_activated_cb           (GtkTreeView       *view,
					     GtkTreePath       *path, 
					     GtkTreeViewColumn *column,
					     GladeInspector    *inspector);
static void     selection_changed_cb        (GtkTreeSelection  *selection,
					     GladeInspector    *inspector);			      
static gint     button_press_cb 	    (GtkWidget         *widget,
					     GdkEventButton    *event,
					     GladeInspector    *inspector);

G_DEFINE_TYPE (GladeInspector, glade_inspector, GTK_TYPE_VBOX)


static void
glade_inspector_set_property (GObject        *object,
			      guint           property_id,
		              const GValue   *value,
			      GParamSpec     *pspec)
{
	GladeInspector *inspector = GLADE_INSPECTOR (object);

	switch (property_id)
	{
		case PROP_PROJECT:
			glade_inspector_set_project (inspector, g_value_get_object (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;			
	}		
}

static void
glade_inspector_get_property (GObject        *object,
                              guint           property_id,
                              GValue         *value,
                              GParamSpec     *pspec)
{
	GladeInspector *inspector = GLADE_INSPECTOR (object);

	switch (property_id)
	{
		case PROP_PROJECT:
			g_value_set_object (value, glade_inspector_get_project (inspector));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
			break;			
	}
}

static void
glade_inspector_class_init (GladeInspectorClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	
	object_class->dispose      = glade_inspector_dispose;
	object_class->finalize     = glade_inspector_finalize;
	object_class->set_property = glade_inspector_set_property;
	object_class->get_property = glade_inspector_get_property;

	/**
	 * GladeInspector::selection-changed:
	 * @inspector: the object which received the signal
	 *
	 * Emitted when the selection changes in the GladeInspector.
	 */
	glade_inspector_signals[SELECTION_CHANGED] =
		g_signal_new ("selection-changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeInspectorClass, selection_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);

	/**
	 * GladeInspector::item-activated:
	 * @inspector: the object which received the signal
	 *
	 * Emitted when a item is activated in the GladeInspector.
	 */
	glade_inspector_signals[ITEM_ACTIVATED] =
		g_signal_new ("item-activated",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GladeInspectorClass, item_activated),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE,
			      0);
			      
	g_object_class_install_property (object_class,
					 PROP_PROJECT,
					 g_param_spec_object ("project",
							      _("Project"),
							      _("The project being inspected"),
							      GLADE_TYPE_PROJECT,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));
	
	g_type_class_add_private (klass, sizeof (GladeInspectorPrivate));
}

static gboolean
find_in_string_insensitive (const gchar *_haystack,
			    const gchar *_needle)
{
	gboolean visible;
	gchar *haystack = g_utf8_casefold (_haystack, -1);
	gchar *needle   = g_utf8_casefold (_needle, -1);

	visible = strstr (haystack, needle) != NULL;

	g_free (haystack);
	g_free (needle);

	return visible;
}

static gboolean
glade_inspector_visible_func (GtkTreeModel* model,
                              GtkTreeIter* parent,
                              gpointer data)
{
	GladeInspector* inspector = data;
	GladeInspectorPrivate *priv = inspector->priv;

	GtkTreeIter iter;
	
	gboolean retval = FALSE;

	if (priv->search_disabled)
		return TRUE;
	
	if (gtk_tree_model_iter_children (model, &iter, parent))
	{
		do
		{
			retval = glade_inspector_visible_func (model, &iter, data);
		}
		while (gtk_tree_model_iter_next (model, &iter) && !retval);
	}
	if (!retval)
	{
		const gchar* text = gtk_entry_get_text (GTK_ENTRY(priv->entry));
		gchar* widget_name;
		
		gtk_tree_model_get (model, parent, GLADE_PROJECT_MODEL_COLUMN_NAME,
		                    &widget_name, -1);

		retval = find_in_string_insensitive (widget_name, text);

		g_free (widget_name);
	}

	return retval;
}

static void
glade_inspector_filter (GladeInspector* inspector)
{
	GladeInspectorPrivate *priv = inspector->priv;

	if (!priv->search_disabled)
	{
		gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER(priv->filter));
		gtk_tree_view_expand_all (GTK_TREE_VIEW (priv->view));
	}
		
}

static void
search_entry_changed_cb (GtkEntry *entry,
			 GladeInspector *inspector)
{
	glade_inspector_filter (inspector);
}

static gboolean
search_complete_idle (GladeInspector *inspector)
{
	GladeInspectorPrivate *priv = inspector->priv;
	const gchar           *str;
	gchar                 *completed = NULL;
	GList                 *list;
	gsize                  length;

	str = gtk_entry_get_text (GTK_ENTRY (priv->entry));

	list = g_completion_complete (priv->completion, str, &completed);
	if (completed) {
		length = strlen (str);

		gtk_entry_set_text (GTK_ENTRY (priv->entry), completed);
		gtk_editable_set_position (GTK_EDITABLE (priv->entry), length);
		gtk_editable_select_region (GTK_EDITABLE (priv->entry),
		                            length, -1);
		g_free (completed);
	}

	priv->idle_complete = 0;

	return FALSE;
}

static void
search_entry_text_inserted_cb (GtkEntry       *entry,
                               const gchar    *text,
                               gint            length,
                               gint           *position,
                               GladeInspector *inspector)
{
	GladeInspectorPrivate *priv = inspector->priv;

	if (!priv->search_disabled && !priv->idle_complete) {
		priv->idle_complete =
			g_idle_add ((GSourceFunc) search_complete_idle,
			            inspector);
	}
}

static gboolean
search_entry_key_press_event_cb (GtkEntry    *entry,
                                 GdkEventKey *event,
                                 GladeInspector *inspector)
{
	GladeInspectorPrivate *priv = inspector->priv;
	const gchar           *str;

	if (event->keyval == GDK_Tab)
	{
		if (event->state & GDK_CONTROL_MASK)
		{
			gtk_widget_grab_focus (priv->view);
		}
		else
		{
			gtk_editable_set_position (GTK_EDITABLE (entry), -1);
			gtk_editable_select_region (GTK_EDITABLE (entry), -1, -1);
		}
		return TRUE;
	}

	if (event->keyval == GDK_Return || event->keyval == GDK_KP_Enter)
	{
		GladeWidget *widget;
		GList       *list;

		str = gtk_entry_get_text (GTK_ENTRY (priv->entry));

		if (str && (list = g_completion_complete (priv->completion, str, NULL)) != NULL)
		{
			widget = glade_widget_get_from_gobject (list->data);

			gtk_entry_set_text (GTK_ENTRY (entry), widget->name);

			gtk_editable_set_position (GTK_EDITABLE (entry), -1);
			gtk_editable_select_region (GTK_EDITABLE (entry), -1, -1);
		}
		return TRUE;
        }

	return FALSE;
}

static void
widget_font_desc_set_style (GtkWidget *widget, PangoStyle style)
{
	PangoFontDescription *font_desc = pango_font_description_copy (gtk_widget_get_style (widget)->font_desc);
	
	pango_font_description_set_style (font_desc, style);
	gtk_widget_modify_font (widget, font_desc);
	pango_font_description_free (font_desc);
}

static const gchar *
search_complete_func (GObject *object)
{
	GladeWidget *widget = glade_widget_get_from_gobject (object);
	g_assert (widget);
	return glade_widget_get_name (widget);
}

static void
search_entry_update (GladeInspector *inspector)
{
	GladeInspectorPrivate *priv = inspector->priv;
	const gchar *str = gtk_entry_get_text (GTK_ENTRY (priv->entry));

	if (str[0] == '\0')
	{
		priv->search_disabled = TRUE;
		widget_font_desc_set_style (priv->entry, PANGO_STYLE_ITALIC);		
		gtk_entry_set_text (GTK_ENTRY (priv->entry), _("< search widgets >"));
		gtk_widget_modify_text (priv->entry, GTK_STATE_NORMAL, 
					&gtk_widget_get_style (priv->entry)->text[GTK_STATE_INSENSITIVE]);
	}
}

static gboolean
search_entry_focus_in_cb (GtkWidget *entry, 
			  GdkEventFocus *event,
			  GladeInspector *inspector)
{
	GladeInspectorPrivate *priv = inspector->priv;

	if (priv->search_disabled)
	{
		gtk_entry_set_text (GTK_ENTRY (priv->entry), "");
		gtk_widget_modify_text (priv->entry, GTK_STATE_NORMAL, NULL);
		gtk_widget_modify_font (priv->entry, NULL);
		priv->search_disabled = FALSE;
	}
	
	return FALSE;
}

static gboolean
search_entry_focus_out_cb (GtkWidget *entry, 
			   GdkEventFocus *event,
			   GladeInspector *inspector)
{
	search_entry_update (inspector);
	
	return FALSE;
}

static void
glade_inspector_init (GladeInspector *inspector)
{
	GladeInspectorPrivate *priv;
	GtkWidget             *sw;
	GtkTreeSelection      *selection;
	
	inspector->priv = priv = GLADE_INSPECTOR_GET_PRIVATE (inspector);

	gtk_widget_push_composite_child ();
	
	priv->project = NULL;

	priv->entry = gtk_entry_new ();
	
	search_entry_update (inspector);
	gtk_widget_show (priv->entry);
	gtk_box_pack_start (GTK_BOX (inspector), priv->entry, FALSE, FALSE, 2);

	g_signal_connect (priv->entry, "changed",
	                  G_CALLBACK (search_entry_changed_cb),
	                  inspector);

	g_signal_connect (priv->entry, "key-press-event",
	                  G_CALLBACK (search_entry_key_press_event_cb),
	                  inspector);

	g_signal_connect_after (priv->entry, "insert-text",
	                        G_CALLBACK (search_entry_text_inserted_cb),
	                        inspector);

	g_signal_connect (priv->entry, "focus-in-event",
	                  G_CALLBACK (search_entry_focus_in_cb),
	                  inspector);

	g_signal_connect (priv->entry, "focus-out-event",
	                  G_CALLBACK (search_entry_focus_out_cb),
	                  inspector);

	priv->completion = g_completion_new ((GCompletionFunc) search_complete_func);

	priv->view = gtk_tree_view_new ();
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (priv->view), FALSE);

 
	add_columns (GTK_TREE_VIEW (priv->view));

	g_signal_connect (G_OBJECT (priv->view), "row-activated",
			  G_CALLBACK (item_activated_cb), inspector);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (G_OBJECT (selection), "changed",
			  G_CALLBACK (selection_changed_cb), inspector);

	/* popup menu */
	g_signal_connect (G_OBJECT (priv->view), "button-press-event",
			  G_CALLBACK (button_press_cb), inspector);

	
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (sw), priv->view);
	gtk_box_pack_start (GTK_BOX (inspector), sw, TRUE, TRUE, 0);
	
	gtk_widget_show (priv->view);
	gtk_widget_show (sw);	


	
	gtk_widget_pop_composite_child ();	
}


static void
glade_inspector_dispose (GObject *object)
{
	GladeInspector *inspector = GLADE_INSPECTOR(object);
	GladeInspectorPrivate *priv = inspector->priv;
	
	if (priv->project)
	{
		g_object_unref (priv->project);
		priv->project = NULL;
	}
	
	G_OBJECT_CLASS (glade_inspector_parent_class)->dispose (object);
}

static void
glade_inspector_finalize (GObject *object)
{	
	G_OBJECT_CLASS (glade_inspector_parent_class)->finalize (object);
}

static void
project_selection_changed_cb (GladeProject     *project,
			      GladeInspector   *inspector)
{
	GladeWidget      *widget;
	GtkTreeSelection *selection;
	GtkTreeModel     *model;
	GtkTreeIter      *iter;
	GtkTreePath      *path, *ancestor_path;
	GList            *list;

	g_return_if_fail (GLADE_IS_INSPECTOR (inspector));
	g_return_if_fail (GLADE_IS_PROJECT (project));
	g_return_if_fail (inspector->priv->project == project);

	g_signal_handlers_block_by_func (gtk_tree_view_get_selection (GTK_TREE_VIEW (inspector->priv->view)),
					 G_CALLBACK (selection_changed_cb),
					 inspector);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (inspector->priv->view));
	g_return_if_fail (selection != NULL);

	model = inspector->priv->filter;

	gtk_tree_selection_unselect_all (selection);

	for (list = glade_project_selection_get (project);
	     list && list->data; list = list->next)
	{
		if ((widget = glade_widget_get_from_gobject (G_OBJECT (list->data))) != NULL)
		{
			if ((iter = glade_util_find_iter_by_widget (model, widget, GLADE_PROJECT_MODEL_COLUMN_OBJECT)) != NULL)
			{
				path = gtk_tree_model_get_path (model, iter);
				ancestor_path = gtk_tree_path_copy (path);

				/* expand parent node */
				if (gtk_tree_path_up (ancestor_path))
					gtk_tree_view_expand_to_path
						(GTK_TREE_VIEW (inspector->priv->view), ancestor_path);

				gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (inspector->priv->view),
							      path,
							      NULL,
							      TRUE,
							      0.5,
							      0);

				gtk_tree_selection_select_iter (selection, iter);

				gtk_tree_iter_free (iter);
				gtk_tree_path_free (path);
				gtk_tree_path_free (ancestor_path);
			}
		}
	}

	g_signal_handlers_unblock_by_func (gtk_tree_view_get_selection (GTK_TREE_VIEW (inspector->priv->view)),
					   G_CALLBACK (selection_changed_cb),
					   inspector);
}

static void
selection_foreach_func (GtkTreeModel *model, 
		        GtkTreePath  *path,
		        GtkTreeIter  *iter, 
		        gpointer      data)
{
	GObject* object;
	
	gtk_tree_model_get (model, iter, GLADE_PROJECT_MODEL_COLUMN_OBJECT, &object, -1);

	if (object)
		glade_app_selection_add (object, FALSE);
}

static void
selection_changed_cb (GtkTreeSelection *selection,
		      GladeInspector   *inspector)
{
	g_signal_handlers_block_by_func (inspector->priv->project,
					 G_CALLBACK (project_selection_changed_cb),
					 inspector);
	
	glade_app_selection_clear (FALSE);
	
	gtk_tree_selection_selected_foreach (selection,
					     selection_foreach_func,
					     inspector);
	glade_app_selection_changed ();

	g_signal_handlers_unblock_by_func (inspector->priv->project,
					   G_CALLBACK (project_selection_changed_cb),
					   inspector);

	g_signal_emit (inspector, glade_inspector_signals[SELECTION_CHANGED], 0);
}

static void
item_activated_cb (GtkTreeView *view,
		   GtkTreePath *path, 
		   GtkTreeViewColumn *column,
		   GladeInspector *inspector)
{	
	g_signal_emit (inspector, glade_inspector_signals[ITEM_ACTIVATED], 0);
}

static gint
button_press_cb (GtkWidget      *widget,
		 GdkEventButton *event,
		 GladeInspector *inspector)
{
	GtkTreeView      *view = GTK_TREE_VIEW (widget);
	GladeInspectorPrivate *priv = inspector->priv;
	GtkTreePath      *path      = NULL;
	gboolean          handled   = FALSE;

	/* Give some kind of access in case of missing right button */
	if (event->window == gtk_tree_view_get_bin_window (view) &&
	    glade_popup_is_popup_event (event))
       	{
		if (gtk_tree_view_get_path_at_pos (view, (gint) event->x, (gint) event->y,
					   &path, NULL, 
					   NULL, NULL) && path != NULL)
		{
			GtkTreeIter  iter;
			GladeWidget *object = NULL;
			if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->project),
							 &iter, path))
			{
				/* now we can obtain the widget from the iter.
				 */
				gtk_tree_model_get (GTK_TREE_MODEL (priv->project), &iter,
						    GLADE_PROJECT_MODEL_COLUMN_OBJECT, &object, -1);

				if (widget != NULL)
					glade_popup_widget_pop (glade_widget_get_from_gobject (object), event, TRUE);
				else
					glade_popup_simple_pop (event);
				
				handled = TRUE;
				
				gtk_tree_path_free (path);
			}
		}
		else
		{
			glade_popup_simple_pop (event);
			handled = TRUE;
		}
	}
	return handled;
}

static void
add_columns (GtkTreeView *view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer_pixbuf, *renderer_name, *renderer_type;

	column = gtk_tree_view_column_new ();

	renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer_pixbuf, FALSE);
	gtk_tree_view_column_set_attributes (column,
	                                     renderer_pixbuf,
	                                     "icon_name", GLADE_PROJECT_MODEL_COLUMN_ICON_NAME,
	                                     NULL);

	renderer_name = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer_name, FALSE);
	gtk_tree_view_column_set_attributes (column,
	                                     renderer_name,
	                                     "text", GLADE_PROJECT_MODEL_COLUMN_NAME,
	                                     NULL);

	renderer_type = gtk_cell_renderer_text_new ();
	g_object_set (renderer_type, "style", PANGO_STYLE_ITALIC, NULL);
	gtk_tree_view_column_pack_start (column, renderer_type, FALSE);	
	gtk_tree_view_column_set_attributes (column,
	                                     renderer_type,
	                                     "text", GLADE_PROJECT_MODEL_COLUMN_TYPE_NAME,
	                                     NULL);

	gtk_tree_view_append_column (view, column);
	gtk_tree_view_set_headers_visible (view, FALSE);
}

static void
update_project_completion (GladeProject    *project,
                           GladeWidget     *widget,
                           GladeInspector  *inspector)
{
	GladeInspectorPrivate *priv = inspector->priv;
	const GList* items;

	g_completion_clear_items (priv->completion);

	if (!priv->project)
		return;

	items = glade_project_get_objects (priv->project);

	/* GCompletion API should take 'const GList *' */
	g_completion_add_items (priv->completion, (GList *)items);
}

static void
disconnect_project_signals (GladeInspector *inspector,
			    GladeProject   *project)
{
	g_signal_handlers_disconnect_by_func (G_OBJECT (project),
					      G_CALLBACK (update_project_completion),
					      inspector);
	g_signal_handlers_disconnect_by_func (G_OBJECT (project),
					      G_CALLBACK (update_project_completion),
					      inspector);
	g_signal_handlers_disconnect_by_func (G_OBJECT (project),
					      G_CALLBACK (update_project_completion),
					      inspector);
	g_signal_handlers_disconnect_by_func (G_OBJECT (project),
					      G_CALLBACK (project_selection_changed_cb),
					      inspector);
}

static void
connect_project_signals (GladeInspector *inspector,
			 GladeProject   *project)
{
	g_signal_connect (G_OBJECT (project), "add-widget",
			  G_CALLBACK (update_project_completion),
			  inspector);
	g_signal_connect (G_OBJECT (project), "remove-widget",
			  G_CALLBACK (update_project_completion),
			  inspector);
	g_signal_connect (G_OBJECT (project), "widget-name-changed",
			  G_CALLBACK (update_project_completion),
			  inspector);
	g_signal_connect (G_OBJECT (project), "selection-changed",
			  G_CALLBACK (project_selection_changed_cb),
			  inspector);
}

/**
 * glade_inspector_set_project:
 * @inspector: a #GladeInspector
 * @project: a #GladeProject
 *
 * Sets the current project of @inspector to @project. To unset the current
 * project, pass %NULL for @project.
 */
void
glade_inspector_set_project (GladeInspector *inspector,
			     GladeProject   *project)
{
	g_return_if_fail (GLADE_IS_INSPECTOR (inspector));
	g_return_if_fail (GLADE_IS_PROJECT (project) || project == NULL);

	GladeInspectorPrivate* priv = inspector->priv;

	if (inspector->priv->project)
	{
		disconnect_project_signals (inspector, project);
		g_object_unref (priv->project);
		priv->project = NULL;
	}
	
	if (project)
	{		
		priv->project = project;
		g_object_ref (priv->project);

		priv->filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (priv->project), NULL);	

		gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (priv->filter),
						(GtkTreeModelFilterVisibleFunc)glade_inspector_visible_func,
						inspector, NULL);

		gtk_tree_view_set_model (GTK_TREE_VIEW (priv->view), priv->filter);
		connect_project_signals (inspector, project);
	}

	update_project_completion (project, NULL, inspector);
	
	gtk_tree_view_expand_all (GTK_TREE_VIEW (inspector->priv->view));
	
	g_object_notify (G_OBJECT (inspector), "project");
}

/**
 * glade_inspector_get_project:
 * @inspector: a #GladeInspector
 * 
 * Note that the method does not ref the returned #GladeProject. 
 *
 * Returns: A #GladeProject
 */
GladeProject *
glade_inspector_get_project (GladeInspector *inspector)
{
	g_return_val_if_fail (GLADE_IS_INSPECTOR (inspector), NULL);

	return inspector->priv->project;
}

/**
 * glade_inspector_get_selected_items:
 * @inspector: a #GladeInspector
 * 
 * Returns the selected items in the inspector. 
 *
 * Returns: A #GList
 */
GList *
glade_inspector_get_selected_items (GladeInspector *inspector)
{
	GtkTreeSelection* selection;
	GList *items = NULL, *paths;
	GladeInspectorPrivate *priv = inspector->priv;
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->view));
						      
	for (paths = gtk_tree_selection_get_selected_rows (selection, NULL); 
	     paths != NULL; paths = g_list_next (paths->next))
	{
		GtkTreeIter filter_iter;
		GtkTreeIter iter;
		GtkTreePath *path = (GtkTreePath *) paths->data;
		GObject *object = NULL;
		
		gtk_tree_model_get_iter (priv->filter, &filter_iter, path);
		gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER(priv->filter),
		                                                  &iter, &filter_iter);
		gtk_tree_model_get (GTK_TREE_MODEL (priv->project), &iter, 
		                    GLADE_PROJECT_MODEL_COLUMN_OBJECT, &object, -1);
		
		items = g_list_prepend (items, glade_widget_get_from_gobject (object));
	}
	
	g_list_foreach (paths, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (paths);

	return items;
}

/**
 * glade_inspector_new:
 * 
 * Creates a new #GladeInspector
 * 
 * Returns: a new #GladeInspector
 */
GtkWidget *
glade_inspector_new (void)
{
	return g_object_new (GLADE_TYPE_INSPECTOR, NULL);
}

/**
 * glade_inspector_new_with_project:
 * @project: a #GladeProject
 *
 * Creates a new #GladeInspector with @project
 * 
 * Returns: a new #GladeInspector
 */
GtkWidget *
glade_inspector_new_with_project (GladeProject *project)
{
	GladeInspector *inspector;
	g_return_val_if_fail (GLADE_IS_PROJECT (project), NULL);

	inspector = g_object_new (GLADE_TYPE_INSPECTOR,
				  "project", project,
				  NULL);

	/* Make sure we expended to the right path */
	project_selection_changed_cb (project, inspector);
	
	return GTK_WIDGET (inspector);
}


