/*
 * glade3
 * Copyright (C) Johannes Schmid 2010 <jhs@gnome.org>
	 * 
 * glade3 is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
	 * 
 * glade3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "glade-signal-model.h"
#include "glade-project.h"

#include <glib/gi18n-lib.h>
#include <string.h>

#define HANDLER_DEFAULT  _("<Type here>")
#define USERDATA_DEFAULT _("<Object>")

struct _GladeSignalModelPrivate
{
	GladeWidget *widget;
	GList* widgets; /* names of the widgets that this widgets derives from and that have signals */
	gint stamp;

	GHashTable* dummy_signals;
	GHashTable* signals; /* signals of the widget */
};

enum
{
	PROP_0,
	PROP_WIDGET,
	PROP_SIGNALS
};

static void gtk_tree_model_iface_init (GtkTreeModelIface* iface);
static void gtk_tree_drag_source_iface_init (GtkTreeDragSourceIface* iface);

static void
on_glade_signal_model_added (GladeWidget* widget, const GladeSignal* signal,
                             GladeSignalModel* model);
static void
on_glade_signal_model_removed (GladeWidget* widget, const GladeSignal* signal,
                               GladeSignalModel* model);
static void
on_glade_signal_model_changed (GladeWidget* widget, const GladeSignal* signal,
                               GladeSignalModel* model);	

G_DEFINE_TYPE_WITH_CODE (GladeSignalModel, glade_signal_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
                                                gtk_tree_model_iface_init);
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
                                                gtk_tree_drag_source_iface_init))

static gint
g_ptr_array_find (GPtrArray* array, gpointer data)
{
	gint i;
	for (i = 0; i < array->len; i++)
	{
		if (array->pdata[i] == data)
			return i;
	}
	return -1;
}

static void
glade_signal_model_init (GladeSignalModel *object)
{
	object->priv = G_TYPE_INSTANCE_GET_PRIVATE (object, GLADE_TYPE_SIGNAL_MODEL, GladeSignalModelPrivate);

	object->priv->dummy_signals = 
		g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) g_object_unref);
}

static void
glade_signal_model_create_widget_list (GladeSignalModel* sig_model)
{
	const GList* list;
	GladeWidget *widget = sig_model->priv->widget;
	GladeWidgetAdaptor *adaptor = glade_widget_get_adaptor (widget);

	for (list = glade_widget_adaptor_get_signals (adaptor);
	     list != NULL; list = g_list_next (list))
	{
		GladeSignalClass *signal = (GladeSignalClass *) list->data;

		if (!g_list_find_custom (sig_model->priv->widgets, (gpointer) glade_signal_class_get_type (signal), (GCompareFunc) strcmp))
		{
			sig_model->priv->widgets = g_list_prepend (sig_model->priv->widgets, (gpointer) glade_signal_class_get_type (signal));
		}
	}
	sig_model->priv->widgets = g_list_reverse (sig_model->priv->widgets);
}

static void
glade_signal_model_finalize (GObject *object)
{
	GladeSignalModel* sig_model = GLADE_SIGNAL_MODEL (object);

	g_list_free (sig_model->priv->widgets);
	g_hash_table_destroy (sig_model->priv->dummy_signals);
	G_OBJECT_CLASS (glade_signal_model_parent_class)->finalize (object);
}

static void
glade_signal_model_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GladeSignalModel* sig_model;

	g_return_if_fail (GLADE_IS_SIGNAL_MODEL (object));

	sig_model = GLADE_SIGNAL_MODEL (object);

	switch (prop_id)
	{
		case PROP_WIDGET:
			sig_model->priv->widget = g_value_get_object (value);
			glade_signal_model_create_widget_list (sig_model);
			g_signal_connect (sig_model->priv->widget,
			                  "add-signal-handler",
			                  G_CALLBACK (on_glade_signal_model_added), sig_model);
			g_signal_connect (sig_model->priv->widget,
			                  "remove-signal-handler",
			                  G_CALLBACK (on_glade_signal_model_removed), sig_model);
			g_signal_connect (sig_model->priv->widget,
			                  "change-signal-handler",
			                  G_CALLBACK (on_glade_signal_model_changed), sig_model);			
			break;
		case PROP_SIGNALS:
			sig_model->priv->signals = g_value_get_pointer (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
glade_signal_model_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GladeSignalModel* sig_model;

	g_return_if_fail (GLADE_IS_SIGNAL_MODEL (object));

	sig_model = GLADE_SIGNAL_MODEL (object);

	switch (prop_id)
	{
		case PROP_WIDGET:
			g_value_set_object (value, sig_model->priv->widget);
			break;
		case PROP_SIGNALS:
			g_value_set_pointer (value, sig_model->priv->signals);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
glade_signal_model_class_init (GladeSignalModelClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (GladeSignalModelPrivate));

	object_class->finalize = glade_signal_model_finalize;
	object_class->set_property = glade_signal_model_set_property;
	object_class->get_property = glade_signal_model_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_WIDGET,
	                                 g_param_spec_object ("widget",
	                                                      "A GladeWidget",
	                                                      "The GladeWidget used to query the signals",
	                                                      GLADE_TYPE_WIDGET,
	                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
	g_object_class_install_property (object_class,
	                                 PROP_SIGNALS,
	                                 g_param_spec_pointer ("signals",
	                                                       "A GHashTable containing the widget signals",
	                                                       "Use to query signals",
	                                                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}

/*
 * glade_signal_model_new:
 * @widget: The GladeWidget the signals belong to
 * @signals: The signals of the #GladeWidget
 *
 * Creates a new GladeSignalModel object to show and edit the
 * signals of a widgets in a GtkTreeView
 */

GtkTreeModel*
glade_signal_model_new (GladeWidget *widget,
                        GHashTable *signals)
{
	GObject* object = g_object_new (GLADE_TYPE_SIGNAL_MODEL,
	                                "widget", widget, 
	                                "signals", signals, NULL);
	return GTK_TREE_MODEL (object);
}

static GtkTreeModelFlags
glade_signal_model_get_flags (GtkTreeModel* model)
{
	return 0;
}

static gint
glade_signal_model_get_n_columns (GtkTreeModel* model)
{
	return GLADE_SIGNAL_N_COLUMNS;
}

static GType
glade_signal_model_get_column_type (GtkTreeModel* model,
                                    gint column)
{
	switch (column)
	{
		case GLADE_SIGNAL_COLUMN_NAME:
			return G_TYPE_STRING;
		case GLADE_SIGNAL_COLUMN_SHOW_NAME:
			return G_TYPE_BOOLEAN;
		case GLADE_SIGNAL_COLUMN_HANDLER:
			return G_TYPE_STRING;
		case GLADE_SIGNAL_COLUMN_OBJECT:
			return G_TYPE_STRING;
		case GLADE_SIGNAL_COLUMN_SWAP:
			return G_TYPE_BOOLEAN;
		case GLADE_SIGNAL_COLUMN_AFTER:
			return G_TYPE_BOOLEAN;
		case GLADE_SIGNAL_COLUMN_IS_HANDLER:
			return G_TYPE_BOOLEAN;
		case GLADE_SIGNAL_COLUMN_NOT_DUMMY:
			return G_TYPE_BOOLEAN;
		case GLADE_SIGNAL_COLUMN_VERSION_WARNING:
			return G_TYPE_BOOLEAN;
		case GLADE_SIGNAL_COLUMN_TOOLTIP:
			return G_TYPE_STRING;
		case GLADE_SIGNAL_COLUMN_SIGNAL:
			return G_TYPE_POINTER;
		default:
			g_assert_not_reached();
			return G_TYPE_NONE;
	}
}

enum
{
	ITER_WIDGET = 0,
	ITER_SIGNAL = 1,
};

static GladeSignal*
glade_signal_model_get_dummy_handler (GladeSignalModel* model, GladeSignalClass* sig_class)
{
	GladeSignal* signal;

	signal = g_hash_table_lookup (model->priv->dummy_signals, 
	                              glade_signal_class_get_name (sig_class));

	if (!signal)
	{
		signal = glade_signal_new (glade_signal_class_get_name (sig_class),
		                           HANDLER_DEFAULT,
		                           USERDATA_DEFAULT,
		                           FALSE,
		                           FALSE);
		g_hash_table_insert (model->priv->dummy_signals, 
		                     (gpointer) glade_signal_class_get_name (sig_class), 
		                     signal);
	}
	return signal;
}

static gboolean
glade_signal_model_not_dummy_handler (GladeSignalModel* model, 
                                      GtkTreeIter* iter)
{
	const gchar* widget = iter->user_data;
	GladeSignalClass* sig_class = iter->user_data2;
	GladeSignal* handler = iter->user_data3;

	if (widget && sig_class && handler)
		return handler != glade_signal_model_get_dummy_handler (model,
		                                                        sig_class);
	return FALSE;
}

static void
glade_signal_model_create_widget_iter (GladeSignalModel* sig_model,
                                       const gchar* widget,
                                       GtkTreeIter* iter)
{
	iter->stamp = sig_model->priv->stamp;
	iter->user_data = (gpointer) widget;
	iter->user_data2 = NULL;
	iter->user_data3 = NULL;
}

static void
glade_signal_model_create_signal_iter (GladeSignalModel* sig_model,
                                       const gchar* widget,
                                       const GladeSignalClass* signal_class,
                                       const GladeSignal* signal,
                                       GtkTreeIter* iter)
{
	glade_signal_model_create_widget_iter (sig_model, widget, iter);
	iter->user_data2 = (GladeSignalClass*) signal_class;
	iter->user_data3 = (GladeSignal*) signal;
	/* Check the version warning here */
	glade_project_verify_signal (sig_model->priv->widget, (GladeSignal*) signal);
}

static GList* glade_signal_model_create_signal_list (GladeSignalModel* sig_model,
                                                     const gchar* widget_type)
{
	GList* widget_signals = NULL;
	const GList* signals;
	GladeWidget *widget = sig_model->priv->widget;
	GladeWidgetAdaptor *adaptor = glade_widget_get_adaptor (widget);

	for (signals = glade_widget_adaptor_get_signals (adaptor);
	     signals != NULL;
	     signals = g_list_next (signals))
	{
		GladeSignalClass* sig_class = signals->data;
		if (g_str_equal (glade_signal_class_get_type (sig_class), widget_type))
		{
			widget_signals = g_list_append (widget_signals, sig_class);
		}
	}
	return widget_signals;
}

/* Be sure to update the parent columns when signals are added/removed
 * as that might affect the appearance */
static void
glade_signal_model_update_class (GladeSignalModel* model,
                                 GtkTreeIter* iter)
{
	const gchar* widget = iter->user_data;
	GladeSignalClass* class = iter->user_data2;
	GtkTreeIter class_dummy;

	glade_signal_model_create_signal_iter (model,
	                                       widget,
	                                       class,
	                                       glade_signal_model_get_dummy_handler (model,
	                                                                             class),
	                                       &class_dummy);

	do
	{
		GtkTreePath* path = gtk_tree_model_get_path (GTK_TREE_MODEL (model),
		                                             &class_dummy);

		gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &class_dummy);
		gtk_tree_path_free (path);
	}
	while (gtk_tree_model_iter_previous (GTK_TREE_MODEL (model), &class_dummy) &&
	       class == class_dummy.user_data2);
}

static GladeSignalClass*
glade_signal_model_find_signal_class (GladeSignalModel* model,
                                      const GladeSignal* handler)
{
	GladeSignalClass* class = NULL;
	GList* widgets = model->priv->widgets;
	GList* widget;
	for (widget = widgets; widget != NULL; widget = g_list_next (widget))
	{
		GList* signals = glade_signal_model_create_signal_list (model,
		                                                        widget->data);
		GList* signal;
		for (signal = signals; signal != NULL; signal = g_list_next (signal))
		{
			GPtrArray* handlers =
				g_hash_table_lookup (model->priv->signals,
				                     glade_signal_class_get_name (GLADE_SIGNAL_CLASS(signal->data)));
			if (handlers && g_ptr_array_find (handlers, (gpointer) handler) != -1)
			{
				class = signal->data;
				break;
			}
		}
		g_list_free (signals);
		if (class)
			break;
	}
	return class;
}

static void
glade_signal_model_iter_for_signal (GladeSignalModel* model, 
                                    const GladeSignalClass* sig_class,
                                    const GladeSignal* handler, 
                                    GtkTreeIter* iter)
{
	glade_signal_model_create_signal_iter (model,
	                                       glade_signal_class_get_type (sig_class),
	                                       sig_class,
	                                       handler,
	                                       iter);
}

static void
on_glade_signal_model_added (GladeWidget* widget, const GladeSignal* signal,
                             GladeSignalModel* model)
{
	GtkTreeIter iter;	
	GladeSignalClass* sig_class = 
		glade_signal_model_find_signal_class (model,
		                                      signal);
	if (sig_class)
	{
		GtkTreePath* path;
		glade_signal_model_iter_for_signal (model, 
		                                    sig_class, 
		                                    signal, 
		                                    &iter);
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (model),
		                                             &iter);
		
		gtk_tree_model_row_inserted (GTK_TREE_MODEL (model),
		                             path,
		                             &iter);
		glade_signal_model_update_class (model, &iter);
		gtk_tree_path_free (path);
		model->priv->stamp++;
	}
}

static void
on_glade_signal_model_removed (GladeWidget* widget, const GladeSignal* signal,
                               GladeSignalModel* model)
{
	GtkTreeIter iter;
	GladeSignalClass* sig_class = 
		glade_signal_model_find_signal_class (model,
		                                      signal);

	if (sig_class)
	{
		GtkTreePath* path;
		glade_signal_model_iter_for_signal (model, 
		                                    sig_class, 
		                                    signal, 
		                                    &iter);
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (model),
		                                             &iter);
		gtk_tree_model_row_deleted (GTK_TREE_MODEL (model),
		                            path);
		glade_signal_model_update_class (model, &iter);
		gtk_tree_path_free (path);
		model->priv->stamp++;
	}
}

static void
on_glade_signal_model_changed (GladeWidget* widget, const GladeSignal* signal,
                               GladeSignalModel* model)
{
	GtkTreeIter iter;
	GladeSignalClass* sig_class = 
		glade_signal_model_find_signal_class (model,
		                                      signal);

	if (sig_class)
	{
		GtkTreePath* path;
		glade_signal_model_iter_for_signal (model, 
		                                    sig_class, 
		                                    signal, 
		                                    &iter);
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (model),
		                                             &iter);
		gtk_tree_model_row_changed (GTK_TREE_MODEL (model),
		                             path,
		                             &iter);
		glade_signal_model_update_class (model, &iter);
		gtk_tree_path_free (path);
		model->priv->stamp++;
	}
}

static gboolean
glade_signal_model_get_iter (GtkTreeModel* model,
                             GtkTreeIter* iter,
                             GtkTreePath* path)
{
	gint* indices;
	gint depth;
	GladeSignalModel* sig_model;

	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (GLADE_IS_SIGNAL_MODEL(model), FALSE);

	indices = gtk_tree_path_get_indices(path);
	depth = gtk_tree_path_get_depth (path);
	sig_model = GLADE_SIGNAL_MODEL (model);

	switch (depth)
	{
		case 1:
			/* Widget */
		{
			glade_signal_model_create_widget_iter (sig_model,
			                                       g_list_nth_data (sig_model->priv->widgets,
			                                                        indices[ITER_WIDGET]),
			                                       iter);
			return TRUE;
		}
		case 2:
			/* Signal */
		{
			gboolean retval;
			GtkTreePath* path =
				gtk_tree_path_new_from_indices (indices[ITER_WIDGET], -1);
			GtkTreeIter widget_iter;

			gtk_tree_model_get_iter (model, &widget_iter, path);
			retval = gtk_tree_model_iter_nth_child (model,
			                                        iter,
			                                        &widget_iter,
			                                        indices[ITER_SIGNAL]);
			gtk_tree_path_free (path);
			return retval;
		}
	}
	return FALSE;
}

static GtkTreePath*
glade_signal_model_get_path (GtkTreeModel* model,
                             GtkTreeIter* iter)
{
	const gchar* widget;
	GladeSignalClass* sig_class;
	GladeSignal* handler;

	GladeSignalModel* sig_model;

	g_return_val_if_fail (iter != NULL, NULL);
	g_return_val_if_fail (GLADE_IS_SIGNAL_MODEL(model), NULL);

	widget = iter->user_data;
	sig_class = iter->user_data2;
	handler = iter->user_data3;
	sig_model = GLADE_SIGNAL_MODEL (model);

	if (handler && sig_class)
	{
		/* Signal */
		gint index0, index1 = 0;
		GList* signal;
		GList* signals = glade_signal_model_create_signal_list (sig_model,
		                                                        widget);

		index0 = g_list_index (sig_model->priv->widgets,
		                       widget);
		
		for (signal = signals; signal != NULL; signal = g_list_next (signal))
		{		

			GPtrArray* handlers = g_hash_table_lookup (sig_model->priv->signals,
			                                           glade_signal_class_get_name (signal->data));

			if (signal->data != sig_class)
			{
				if (handlers)
					index1 += handlers->len;
				index1++; /* dummy_handler */
			}
			else
			{
				if (handlers)
				{
					gint handler_index = g_ptr_array_find (handlers, handler);
					if (handler_index == -1) /* dummy handler */
					{
						index1 += handlers->len;
					}
					else
						index1 += handler_index;
				}
				break;
			}
		}
		return gtk_tree_path_new_from_indices (index0, index1, -1);
	}
	else if (widget)
	{
		/* Widget */
		return gtk_tree_path_new_from_indices (g_list_index (sig_model->priv->widgets,
		                                                     widget), -1);
	}
	g_assert_not_reached();
}

static void
glade_signal_model_get_value (GtkTreeModel* model,
                              GtkTreeIter* iter,
                              gint column,
                              GValue* value)
{	
	const gchar* widget;
	GladeSignalClass* sig_class;
	GladeSignal* handler;

	GladeSignalModel* sig_model;

	g_return_if_fail (iter != NULL);
	g_return_if_fail (GLADE_IS_SIGNAL_MODEL(model));

	widget = iter->user_data;
	sig_class = iter->user_data2;
	handler = iter->user_data3;
	sig_model = GLADE_SIGNAL_MODEL (model);

	value = g_value_init (value, 
	                      glade_signal_model_get_column_type (model, column));

	switch (column)
	{
		case GLADE_SIGNAL_COLUMN_NAME:
			if (widget && sig_class && handler)
			{
				g_value_set_static_string (value,
				                           glade_signal_class_get_name (sig_class));

			}
			else if (widget)
				g_value_set_static_string (value,
				                           widget);
			break;
		case GLADE_SIGNAL_COLUMN_SHOW_NAME:
			if (widget && sig_class && handler)
			{
				GPtrArray* handlers = g_hash_table_lookup (sig_model->priv->signals,
				                                           glade_signal_class_get_name (sig_class));
				if (!handlers || !handlers->len || g_ptr_array_find (handlers, handler) == 0)
					g_value_set_boolean (value,
					                     TRUE);
				else
					g_value_set_boolean (value,
					                     FALSE);
			}
			else if (widget)
				g_value_set_boolean (value,
				                     TRUE);
			break;
		case GLADE_SIGNAL_COLUMN_HANDLER:
			if (widget && sig_class && handler)
				g_value_set_static_string (value,
				                           glade_signal_get_handler (handler));
			else 
				g_value_set_static_string (value,
				                           "");
			break;
		case GLADE_SIGNAL_COLUMN_OBJECT:
			if (widget && sig_class && handler)
		{
			const gchar* userdata = glade_signal_get_userdata (handler);
			if (userdata && strlen (userdata))
				g_value_set_static_string (value,
				                           userdata);
			else
				g_value_set_static_string (value,
				                           USERDATA_DEFAULT);
		}
			else 
				g_value_set_static_string (value,
				                           "");
			break;
		case GLADE_SIGNAL_COLUMN_SWAP:
			if (widget && sig_class && handler)
				g_value_set_boolean (value,
				                     glade_signal_get_swapped (handler));
			else 
				g_value_set_boolean (value,
				                     FALSE);
			break;
		case GLADE_SIGNAL_COLUMN_AFTER:
			if (widget && sig_class && handler)
				g_value_set_boolean (value,
				                     glade_signal_get_after (handler));
			else 
				g_value_set_boolean (value,
				                     FALSE);
			break;
		case GLADE_SIGNAL_COLUMN_IS_HANDLER:
			g_value_set_boolean (value,
			                     widget && sig_class && handler);
			break;
		case GLADE_SIGNAL_COLUMN_NOT_DUMMY:
			g_value_set_boolean (value,
			                     glade_signal_model_not_dummy_handler (sig_model,
			                                                           iter));
			break;
		case GLADE_SIGNAL_COLUMN_VERSION_WARNING:
		{
			gboolean warn = FALSE;
			if (handler)
			{
				const gchar* warning = glade_signal_get_support_warning (handler);
				warn = warning && strlen (warning);
			}
			g_value_set_boolean (value, warn);
		}
			break;
		case GLADE_SIGNAL_COLUMN_TOOLTIP:
			if (handler)
				g_value_set_string (value,
				                    glade_signal_get_support_warning (handler));
			else
				g_value_set_static_string (value, NULL);
			break;
		case GLADE_SIGNAL_COLUMN_SIGNAL:
			g_value_set_pointer (value, handler);
			break;
		default:
			g_assert_not_reached();
	}
}

static gboolean
glade_signal_model_iter_next_signal (GladeSignalModel* sig_model,
                                     const gchar* widget,
                                     GtkTreeIter* iter,
                                     GList* signal)
{
	if (signal->next)
	{
		signal = signal->next;
		GladeSignal* next_handler;
		GPtrArray* next_handlers = 
			g_hash_table_lookup (sig_model->priv->signals,
			                     glade_signal_class_get_name (signal->data));
		if (next_handlers && next_handlers->len)
		{
			next_handler = g_ptr_array_index (next_handlers, 0);
		}
		else
		{
			next_handler = 
				glade_signal_model_get_dummy_handler (sig_model,
				                                      signal->data);
		}
		glade_signal_model_create_signal_iter (sig_model, widget,
		                                       signal->data, next_handler,
		                                       iter);
		g_list_free (signal);
		return TRUE;
	}
	else
	{
		g_list_free (signal);
		return FALSE;
	}
}


static gboolean
glade_signal_model_iter_next (GtkTreeModel* model,
                              GtkTreeIter* iter)
{
	const gchar* widget;
	GladeSignalClass* sig_class;
	GladeSignal* handler;
	GtkTreeIter parent;


	GladeSignalModel* sig_model;

	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (GLADE_IS_SIGNAL_MODEL(model), FALSE);

	widget = iter->user_data;
	sig_class = iter->user_data2;
	handler = iter->user_data3;

	sig_model = GLADE_SIGNAL_MODEL (model);  

	gtk_tree_model_iter_parent (model, &parent, iter);		

	if (handler && sig_class)
	{
		GList* signals = glade_signal_model_create_signal_list (sig_model,
		                                                        widget);
		GList* signal = g_list_find (signals, sig_class);
		GPtrArray* handlers = g_hash_table_lookup (sig_model->priv->signals,
		                                           glade_signal_class_get_name (sig_class));
		GladeSignal* dummy = glade_signal_model_get_dummy_handler (sig_model,
		                                                           sig_class);
		if (handler == dummy)
		{
			return glade_signal_model_iter_next_signal (sig_model, widget, iter, signal);
		}
		else if (handlers)
		{
			gint new_index = g_ptr_array_find (handlers, handler) + 1;
			if (new_index < handlers->len)
			{
				glade_signal_model_create_signal_iter (sig_model, widget,
				                                       sig_class,
				                                       g_ptr_array_index (handlers, new_index),
				                                       iter);
				g_list_free (signals);
				return TRUE;
			}
			else if (new_index == handlers->len)
			{
				glade_signal_model_create_signal_iter (sig_model, widget,
				                                       sig_class,
				                                       glade_signal_model_get_dummy_handler (sig_model,
				                                                                             sig_class),
				                                       iter);
				g_list_free (signals);
				return TRUE;
			}
			else
			{
				return glade_signal_model_iter_next_signal (sig_model, widget, iter, signal);
			}
		}
		else
		{
			g_list_free (signals);
			return FALSE;
		}
	}
	else if (widget)
	{
		gint new_index = g_list_index (sig_model->priv->widgets,
		                               widget) + 1;

		return gtk_tree_model_iter_nth_child (model, iter, NULL, new_index);
	}
	iter->user_data = NULL;
	iter->user_data2 = NULL;
	iter->user_data3 = NULL;
	iter->stamp = 0;
	return FALSE;
}

static gint
glade_signal_model_iter_n_children (GtkTreeModel* model,
                                    GtkTreeIter* iter)
{
	GladeSignal* handler;
	GladeSignalModel* sig_model;
	GladeSignalClass* sig_class;
	const gchar* widget;

	g_return_val_if_fail (iter != NULL, 0);
	g_return_val_if_fail (GLADE_IS_SIGNAL_MODEL(model), 0);

	handler = iter->user_data3;
	sig_model = GLADE_SIGNAL_MODEL (model);
	sig_class = iter->user_data2;
	widget = iter->user_data;

	if (handler && sig_class)
	{
		return 0;
	}
	else if (widget)
	{
		GList* signals = glade_signal_model_create_signal_list (sig_model,
		                                                        widget);
		GList* signal;
		gint retval = 0;

		for (signal = signals; signal != NULL; signal = g_list_next (signal))
		{
			GPtrArray* handlers = g_hash_table_lookup (sig_model->priv->signals,
			                                           glade_signal_class_get_name (signal->data));
			if (handlers)
				retval += handlers->len;
			retval++; 
		}
		g_list_free (signals);

		return retval;
	}
	g_assert_not_reached ();	
}

static gboolean
glade_signal_model_iter_has_child (GtkTreeModel* model,
                                   GtkTreeIter* iter)
{
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (GLADE_IS_SIGNAL_MODEL(model), FALSE);

	return (glade_signal_model_iter_n_children (model, iter) != 0);
}

static gboolean
glade_signal_model_iter_nth_child (GtkTreeModel* model,
                                   GtkTreeIter* iter,
                                   GtkTreeIter* parent,
                                   gint n)
{
	GladeSignal* handler;
	GladeSignalModel* sig_model;
	GladeSignalClass* sig_class;
	const gchar* widget;

	g_return_val_if_fail (iter != NULL, 0);
	g_return_val_if_fail (GLADE_IS_SIGNAL_MODEL(model), 0);

	handler = parent ? parent->user_data3 : NULL;
	sig_model = GLADE_SIGNAL_MODEL (model);
	sig_class = parent ? parent->user_data2 : NULL;
	widget = parent ? parent->user_data : NULL;

	if (handler)
	{
		return FALSE;
	}
	else if (widget)
	{
		GList* signals = glade_signal_model_create_signal_list (sig_model,
		                                                        widget);
		GList* signal;
		for (signal = signals; signal != NULL; signal = g_list_next (signal))
		{
			GPtrArray* handlers = g_hash_table_lookup (sig_model->priv->signals,
			                                           glade_signal_class_get_name (signal->data));
			if (handlers)
			{
				if (n >= handlers->len)
					n -= handlers->len;
				else
				{
					glade_signal_model_create_signal_iter (sig_model,
					                                       widget,
					                                       signal->data,
					                                       g_ptr_array_index (handlers, n),
					                                       iter);
					g_list_free (signals);
					return TRUE;
				}
			}
			if (n == 0)
			{
				GladeSignal* handler =
					glade_signal_model_get_dummy_handler (sig_model,
					                                      signal->data);
				glade_signal_model_create_signal_iter (sig_model,
				                                       widget,
				                                       signal->data,
				                                       handler,
				                                       iter);
				g_list_free (signals);
				return TRUE;
			}
			n--;
		}
		return FALSE;
	}
	else
	{
		if (g_list_length (sig_model->priv->widgets) > n)
		{
			glade_signal_model_create_widget_iter (sig_model,
			                                       g_list_nth_data (sig_model->priv->widgets, n),
			                                       iter);
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean
glade_signal_model_iter_children (GtkTreeModel* model,
                                  GtkTreeIter* iter,
                                  GtkTreeIter* parent)
{
	return glade_signal_model_iter_nth_child (model, iter, parent, 0);
}

static gboolean
glade_signal_model_iter_parent (GtkTreeModel* model,
                                GtkTreeIter* iter,
                                GtkTreeIter* child)
{
	GladeSignalModel* sig_model;
	GladeSignal* handler;
	GladeSignalClass* sig_class;
	const gchar* widget;

	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (child != NULL, FALSE);
	g_return_val_if_fail (GLADE_IS_SIGNAL_MODEL(model), FALSE);  

	sig_model = GLADE_SIGNAL_MODEL (model);
	handler = child->user_data3;
	sig_class = child->user_data2;
	widget = child->user_data;

	if (handler && sig_class)
	{
		glade_signal_model_create_widget_iter (sig_model, widget, iter);
		return TRUE;
	}
	return FALSE;
}

static void
gtk_tree_model_iface_init (GtkTreeModelIface *iface)
{
	iface->get_flags = glade_signal_model_get_flags;
	iface->get_column_type = glade_signal_model_get_column_type;
	iface->get_n_columns = glade_signal_model_get_n_columns;
	iface->get_iter = glade_signal_model_get_iter;
	iface->get_path = glade_signal_model_get_path;
	iface->get_value = glade_signal_model_get_value;	
	iface->iter_next = glade_signal_model_iter_next;
	iface->iter_children = glade_signal_model_iter_children;
	iface->iter_has_child = glade_signal_model_iter_has_child;
	iface->iter_n_children = glade_signal_model_iter_n_children;
	iface->iter_nth_child = glade_signal_model_iter_nth_child;
	iface->iter_parent = glade_signal_model_iter_parent;
}

static gboolean
glade_signal_model_row_draggable (GtkTreeDragSource* model, 
                                  GtkTreePath* path)
{
	GtkTreeIter iter;
	gboolean is_handler;
	gboolean not_dummy;
	gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);

	gtk_tree_model_get (GTK_TREE_MODEL (model), &iter,
	                    GLADE_SIGNAL_COLUMN_IS_HANDLER, &is_handler,
	                    GLADE_SIGNAL_COLUMN_NOT_DUMMY, &not_dummy,
	                    -1);

	return (is_handler && not_dummy);
}

static gboolean
glade_signal_model_drag_data_get (GtkTreeDragSource* model, 
                                  GtkTreePath* path,
                                  GtkSelectionData* data)
{
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path))
	{
		GladeSignal* signal;
		const gchar* widget = iter.user_data;
		gchar* dnd_text;

		gtk_tree_model_get (GTK_TREE_MODEL (model), &iter,
		                    GLADE_SIGNAL_COLUMN_SIGNAL, &signal, -1);

		dnd_text = g_strdup_printf ("%s:%s:%s", widget, glade_signal_get_name (signal),
		                            glade_signal_get_handler (signal));
		g_message ("Sent: %s", dnd_text);
		gtk_selection_data_set (data,
		                        gdk_atom_intern_static_string ("application/x-glade-signal"),
		                        8,
		                        (guchar*) dnd_text,
		                        strlen (dnd_text));

		g_free (dnd_text);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

static gboolean
glade_signal_model_drag_data_delete (GtkTreeDragSource* model, 
                                     GtkTreePath* path)
{
	/* We don't move rows... */
	return FALSE;
}

static void
gtk_tree_drag_source_iface_init (GtkTreeDragSourceIface* iface)
{
	iface->row_draggable = glade_signal_model_row_draggable;
	iface->drag_data_get = glade_signal_model_drag_data_get;
	iface->drag_data_delete = glade_signal_model_drag_data_delete;
}
