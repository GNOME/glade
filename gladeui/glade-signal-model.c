/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * glade3
 * Copyright (C) Johannes Schmid 2010 <jhs@gnome.org>
 * 
 * glade3 is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
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

#include "glade-signal-model.h"
#include <string.h>

struct _GladeSignalModelPrivate
{
	GladeWidget *widget;
	GList* widgets; /* names of the widgets that this widgets derives from and that have signals */
	gint stamp;
};

enum
{
	PROP_0,
	PROP_WIDGET
};

static void gtk_tree_model_iface_init (GtkTreeModelIface* iface);

G_DEFINE_TYPE_WITH_CODE (GladeSignalModel, glade_signal_model, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
                                      gtk_tree_model_iface_init))

static void
glade_signal_model_init (GladeSignalModel *object)
{
	object->priv = G_TYPE_INSTANCE_GET_PRIVATE (object, GLADE_TYPE_SIGNAL_MODEL, GladeSignalModelPrivate);
}

static void
glade_signal_model_create_widget_list (GladeSignalModel* sig_model)
{
	GList* list;
	for (list = sig_model->priv->widget->adaptor->signals;
	     list != NULL; list = g_list_next (list))
	{
		GladeSignalClass *signal = (GladeSignalClass *) list->data;
		if (!g_list_find_custom (sig_model->priv->widgets, signal->type, (GCompareFunc) strcmp))
		{
			sig_model->priv->widgets = g_list_prepend (sig_model->priv->widgets, signal->type);
		}
	}
	sig_model->priv->widgets = g_list_reverse (sig_model->priv->widgets);
}

static void
glade_signal_model_finalize (GObject *object)
{
	GladeSignalModel* sig_model = GLADE_SIGNAL_MODEL (object);
	g_list_free (sig_model->priv->widgets);
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
}

/*
 * glade_signal_model_new:
 * @widget: The GladeWidget the signals belong to
 *
 * Creates a new GladeSignalModel object to show and edit the
 * signals of a widgets in a GtkTreeView
 */

GtkTreeModel*
glade_signal_model_new (GladeWidget *widget)
{
	GObject* object = g_object_new (GLADE_TYPE_SIGNAL_MODEL,
	                                "widget", widget, NULL);
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
		case GLADE_SIGNAL_COLUMN_VERSION_WARNING:
			return GDK_TYPE_PIXBUF;
		case GLADE_SIGNAL_COLUMN_NAME:
			return G_TYPE_STRING;
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
		default:
			g_assert_not_reached();
			return G_TYPE_NONE;
	}
}

enum
{
	ITER_WIDGET = 1,
	ITER_SIGNAL = 2,
	ITER_HANDLER = 3
};


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
glade_signal_model_create_handler_iter (GladeSignalModel* sig_model,
                                        const gchar* widget,
                                        GladeSignalClass* signal_class,
                                        GladeSignal* signal,
                                        GtkTreeIter* iter)
{
	glade_signal_model_create_widget_iter (sig_model, widget, iter);
	iter->user_data2 = signal_class;
	iter->user_data3 = signal;
}

static void
glade_signal_model_create_signal_iter (GladeSignalModel* sig_model,
                                       const gchar* widget,
                                       GladeSignalClass* signal_class,
                                       GtkTreeIter* iter)
{
	glade_signal_model_create_widget_iter (sig_model, widget, iter);
	iter->user_data2 = signal_class;
}

static GList* glade_signal_model_create_signal_list (GladeSignalModel* sig_model,
                                                     const gchar* widget)
{
	GList* widget_signals = NULL;
	GList* signals;
	for (signals = sig_model->priv->widget->adaptor->signals; signals != NULL;
	     signals = g_list_next (signals))
	{
		GladeSignalClass* sig_class = signals->data;
		if (g_str_equal (sig_class->name, widget))
		{
			widget_signals = g_list_append (widget_signals, sig_class);
		}
	}
	return widget_signals;
}

static gboolean
glade_signal_model_get_iter (GtkTreeModel* model,
                             GtkTreeIter* iter,
                             GtkTreePath* path)
{
	gint* indices;
	gint depth;
	GladeSignalModel* sig_model;

	g_message ("Get Iter: %s", gtk_tree_path_to_string (path));
	
	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (GLADE_IS_SIGNAL_MODEL(model), FALSE);
	
	indices = gtk_tree_path_get_indices(path);
	depth = gtk_tree_path_get_depth (path);
	sig_model = GLADE_SIGNAL_MODEL (model);

	switch (depth)
	{
		case ITER_WIDGET:
		/* Widget */
		{
			glade_signal_model_create_widget_iter (sig_model,
			                                       g_list_nth_data (sig_model->priv->widgets,
			                                                        indices[ITER_WIDGET]),
			                                       iter);
			return TRUE;
		}
		case ITER_SIGNAL:
		/* Signal */
		{
			const gchar* widget = g_list_nth_data (sig_model->priv->widgets,
			                                       indices[ITER_WIDGET]);
			GList* signals = glade_signal_model_create_signal_list (sig_model,
			                                                        widget);
			if (signals)
			{
				glade_signal_model_create_signal_iter (sig_model, widget,
					                                   g_list_nth_data (signals, indices[ITER_SIGNAL]), 
			    		                               iter);
				g_list_free (signals);
				return TRUE;
			}
			return FALSE;
		}
		case ITER_HANDLER:
		/* Handler */
		{
			GPtrArray* handlers;
			const gchar* widget = g_list_nth_data (sig_model->priv->widgets,
			                                       indices[ITER_WIDGET]);
			GList* signals = glade_signal_model_create_signal_list (sig_model,
			                                                        widget);
			if (signals)
			{
				GladeSignalClass* signal = g_list_nth_data (signals, indices[ITER_SIGNAL]);
				handlers = g_hash_table_lookup (sig_model->priv->widget->signals,
					                            signal->name);
				if (indices[ITER_HANDLER] < handlers->len)
				{
					GladeSignal* handler =
						(GladeSignal*) g_ptr_array_index (handlers, indices[ITER_HANDLER]);
					glade_signal_model_create_handler_iter (sig_model, widget, 
							                                signal,
							                                handler, iter);
					return TRUE;
				}
				g_list_free (signals);
			}
			return FALSE;
		}
	}
	return FALSE;
}

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

static GtkTreePath*
glade_signal_model_get_path (GtkTreeModel* model,
                             GtkTreeIter* iter)
{
	g_return_val_if_fail (iter != NULL, NULL);
	g_return_val_if_fail (GLADE_IS_SIGNAL_MODEL(model), NULL);

	const gchar* widget = iter->user_data;
	GladeSignalClass* sig_class = iter->user_data2;
	GladeSignal* handler = iter->user_data3;
	GladeSignalModel* sig_model = GLADE_SIGNAL_MODEL (model);

	g_message (__FUNCTION__);
	
	if (handler)
	{
		/* Handler */
		GPtrArray* handlers;
		gint index0, index1, index2;
		GList* signals = glade_signal_model_create_signal_list (sig_model,
		                                                        widget);
		index0 = g_list_index (sig_model->priv->widgets,
		                       widget);
		index1 = g_list_index (signals, sig_class);

		handlers = g_hash_table_lookup (sig_model->priv->widget->signals,
			                            sig_class->name);
		
		index2 = g_ptr_array_find (handlers, handler);

		g_list_free (signals);
		return gtk_tree_path_new_from_indices (index0, index1, index2, -1);
	}
	else if (sig_class)
	{
		/* Signal */
		GList* signals = glade_signal_model_create_signal_list (sig_model,
		                                                        widget);
		gint index0 = g_list_index (sig_model->priv->widgets,
		                            widget);
		gint index1 = g_list_index (signals, sig_class);
		g_list_free (signals);

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
	g_return_if_fail (iter != NULL);
	g_return_if_fail (GLADE_IS_SIGNAL_MODEL(model));

	const gchar* widget = iter->user_data;
	GladeSignalClass* sig_class = iter->user_data2;
	GladeSignal* handler = iter->user_data3;

	g_message (__FUNCTION__);
	
	switch (column)
	{
		case GLADE_SIGNAL_COLUMN_VERSION_WARNING:
			/* TODO */
			g_value_set_boolean (value, FALSE);
			break;
		case GLADE_SIGNAL_COLUMN_NAME:
			if (widget && sig_class && handler)
				g_value_set_static_string (value,
				                           handler->name);
			else if (widget && sig_class)
				g_value_set_static_string (value,
				                           sig_class->name);
			else if (widget)
				g_value_set_static_string (value,
				                           widget);
			break;
		case GLADE_SIGNAL_COLUMN_HANDLER:
			if (widget && sig_class && handler)
				g_value_set_static_string (value,
				                           handler->handler);
			else 
				g_assert_not_reached();
			break;
		case GLADE_SIGNAL_COLUMN_OBJECT:
			if (widget && sig_class && handler)
				g_value_set_static_string (value,
				                           handler->userdata);
			else 
				g_assert_not_reached();
			break;
		case GLADE_SIGNAL_COLUMN_SWAP:
			if (widget && sig_class && handler)
				g_value_set_boolean (value,
				                     handler->swapped);
			else 
				g_assert_not_reached();
			break;
		case GLADE_SIGNAL_COLUMN_AFTER:
			if (widget && sig_class && handler)
				g_value_set_boolean (value,
				                     handler->after);
			else 
				g_assert_not_reached();
			break;
		case GLADE_SIGNAL_COLUMN_IS_HANDLER:
			g_value_set_boolean (value,
			                     widget && sig_class && handler);
			break;
		default:
			g_assert_not_reached();
	}
}

static gboolean
glade_signal_model_iter_next (GtkTreeModel* model,
                              GtkTreeIter* iter)
{
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (GLADE_IS_SIGNAL_MODEL(model), FALSE);

	GladeSignal* handler = iter->user_data3;
	GladeSignalModel* sig_model = GLADE_SIGNAL_MODEL (model);
	GladeSignalClass* sig_class = iter->user_data2;
	const gchar* widget = iter->user_data;

	g_message (__FUNCTION__);
	
	if (handler)
	{
		GPtrArray* handlers = g_hash_table_lookup (sig_model->priv->widget->signals,
		                                           sig_class->name);
		gint new_index = g_ptr_array_find (handlers, handler) + 1;
		if (new_index < handlers->len)
		{
			GladeSignal* new_handler = g_ptr_array_index (handlers, new_index);
			iter->user_data3 = new_handler;
			return TRUE;
		}
	}
	else if (sig_class)
	{
		GList* signals = glade_signal_model_create_signal_list (sig_model,
		                                                        widget);
		gint new_index = g_list_index (signals, sig_class) + 1;
		if ((iter->user_data2 = g_list_nth_data (signals, new_index)))
		    return TRUE;
	}
	else if (widget)
	{
		gint new_index = g_list_index (sig_model->priv->widgets,
		                               widget) + 1;
		if ((iter->user_data = g_list_nth_data (sig_model->priv->widgets, new_index)))
		    return TRUE;
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
	g_return_val_if_fail (iter != NULL, 0);
	g_return_val_if_fail (GLADE_IS_SIGNAL_MODEL(model), 0);

	GladeSignal* handler = iter->user_data3;
	GladeSignalModel* sig_model = GLADE_SIGNAL_MODEL (model);
	GladeSignalClass* sig_class = iter->user_data2;
	const gchar* widget = iter->user_data;

	g_message (__FUNCTION__);
	
	if (handler)
	{
		return 0;
	}
	else if (sig_class)
	{
		GPtrArray* handlers = g_hash_table_lookup (sig_model->priv->widget->signals,
		                                           sig_class->name);
		if (handlers)
			return handlers->len;
		else
			return 0;
	}
	else if (widget)
	{
		GList* signals = glade_signal_model_create_signal_list (sig_model,
		                                                        widget);
		gint retval = g_list_length (signals);
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
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (GLADE_IS_SIGNAL_MODEL(model), 0);

	GladeSignal* handler = parent ? parent->user_data3 : NULL;
	GladeSignalModel* sig_model = GLADE_SIGNAL_MODEL (model);
	GladeSignalClass* sig_class = parent ? parent->user_data2 : NULL;
	const gchar* widget = parent ? parent->user_data : NULL;

	g_message (__FUNCTION__);
	
	if (handler)
	{
		return FALSE;
	}
	else if (sig_class)
	{
		GPtrArray* handlers = g_hash_table_lookup (sig_model->priv->widget->signals,
		                                           sig_class->name);
		if (n < handlers->len)
		{
			glade_signal_model_create_handler_iter (sig_model, widget, sig_class,
			                                        g_ptr_array_index (handlers, n),
			                                        iter);
			return TRUE;
		}
		else
			return FALSE;
	}
	else if (widget)
	{
		GList* signals = glade_signal_model_create_signal_list (sig_model,
		                                                        widget);
		gboolean retval = FALSE;
		if (g_list_length (signals) > n)
		{
			glade_signal_model_create_signal_iter (sig_model, widget, 
			                                       g_list_nth_data (signals, n),
			                                       iter);
			retval = TRUE;
		}
		g_list_free (signals);
		return retval;
	}
	else
	{
		if (sig_model->priv->widgets)
		{
			glade_signal_model_create_widget_iter (sig_model,
			                                       sig_model->priv->widgets->data,
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
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (GLADE_IS_SIGNAL_MODEL(model), FALSE);

	GladeSignal* handler = iter->user_data3;
	GladeSignalModel* sig_model = GLADE_SIGNAL_MODEL (model);
	GladeSignalClass* sig_class = iter->user_data2;
	const gchar* widget = iter->user_data;

	if (handler)
	{
		glade_signal_model_create_signal_iter (sig_model, widget, sig_class, iter);
		return TRUE;
	}
	else if (sig_class)
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