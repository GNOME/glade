/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Tristan Van Berkom.
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
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#include <config.h>

#include <gladeui/glade.h>
#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include "glade-icon-sources.h"

static void
icon_set_free (GList *list)
{
	g_list_foreach (list, (GFunc)gtk_icon_source_free, NULL);
	g_list_free (list);
}

GladeIconSources *
glade_icon_sources_new (void)
{
	GladeIconSources *sources = g_new0 (GladeIconSources, 1);

	sources->sources = g_hash_table_new_full (g_str_hash, g_str_equal,
						  (GDestroyNotify)g_free,
						  (GDestroyNotify)icon_set_free);
	return sources;
}


static void
icon_sources_dup (gchar               *icon_name,
		  GList               *set,
		  GladeIconSources    *dup)
{
	GList *dup_set = NULL, *l;
	GtkIconSource *source;
	
	for (l = set; l; l = l->next)
	{
		source = gtk_icon_source_copy ((GtkIconSource *)l->data);
		dup_set = g_list_prepend (dup_set, source);
	}
	g_hash_table_insert (dup->sources, g_strdup (icon_name), g_list_reverse (dup_set));
}

GladeIconSources *
glade_icon_sources_copy (GladeIconSources *sources)
{
	if (!sources)
		return NULL;

	GladeIconSources *dup = glade_icon_sources_new ();

	g_hash_table_foreach (sources->sources, (GHFunc)icon_sources_dup, dup);

	return dup;
}

void
glade_icon_sources_free (GladeIconSources *sources)
{
	if (sources)
	{
		g_hash_table_destroy (sources->sources);
		g_free (sources);
	}
}

GType
glade_icon_sources_get_type (void)
{
	static GType type_id = 0;

	if (!type_id)
		type_id = g_boxed_type_register_static
			("GladeIconSources", 
			 (GBoxedCopyFunc) glade_icon_sources_copy,
			 (GBoxedFreeFunc) glade_icon_sources_free);
	return type_id;
}

/********************** GladeParamIconSources  ***********************/

struct _GladeParamIconSources
{
	GParamSpec parent_instance;
};

static gint
param_values_cmp (GParamSpec *pspec, const GValue *value1, const GValue *value2)
{
	GladeIconSources *s1, *s2;
	gint retval;
	
	s1 = g_value_get_boxed (value1);
	s2 = g_value_get_boxed (value2);
	
	if (s1 == NULL && s2 == NULL) return 0;
	
	if (s1 == NULL || s2 == NULL) return s1 - s2;
	
	if ((retval = 
	     g_hash_table_size (s1->sources) - g_hash_table_size (s2->sources)))
		return retval;
	else
		/* XXX We could do alot better here... but right now we are using strings
		 * and ignoring changes somehow... need to fix that.
		 */
		return GPOINTER_TO_INT (s1->sources) - GPOINTER_TO_INT (s2->sources);
}

GType
glade_param_icon_sources_get_type (void)
{
	static GType icon_sources_type = 0;

	if (icon_sources_type == 0)
	{
		 GParamSpecTypeInfo pspec_info = {
			 sizeof (GladeParamIconSources),  /* instance_size */
			 16,   /* n_preallocs */
			 NULL, /* instance_init */
			 0,    /* value_type, assigned further down */
			 NULL, /* finalize */
			 NULL, /* value_set_default */
			 NULL, /* value_validate */
			 param_values_cmp, /* values_cmp */
		 };
		 pspec_info.value_type = GLADE_TYPE_ICON_SOURCES;
		 
		 icon_sources_type = g_param_type_register_static
			 ("GladeParamIconSources", &pspec_info);
	}
	return icon_sources_type;
}

GParamSpec *
glade_standard_icon_sources_spec (void)
{
	GladeParamIconSources *pspec;

	pspec = g_param_spec_internal (GLADE_TYPE_PARAM_ICON_SOURCES,
				       "icon-sources", _("Icon Sources"), 
				       _("A list of sources for an icon factory"),
				       G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	return G_PARAM_SPEC (pspec);
}

/**************************** GladeEditorProperty *****************************/
enum {
	COLUMN_TEXT,             /* Used for display/editing purposes */
	COLUMN_TEXT_WEIGHT,      /* Whether the text is bold (icon-name parent rows) */
	COLUMN_TEXT_EDITABLE,    /* parent icon-name displays are not editable */
	COLUMN_ICON_NAME,        /* store the icon name regardless */
	COLUMN_LIST_INDEX,       /* denotes the position in the GList of the actual property value (or -1) */
	COLUMN_DIRECTION_ACTIVE,
	COLUMN_DIRECTION,
	COLUMN_SIZE_ACTIVE,
	COLUMN_SIZE,
	COLUMN_STATE_ACTIVE,
	COLUMN_STATE,
	NUM_COLUMNS
};

typedef struct
{
	GladeEditorProperty parent_instance;

	GtkTreeView       *view;
	GtkTreeStore      *store;
	GtkListStore      *icon_names_store;
	GtkTreeViewColumn *filename_column;
	GtkWidget         *combo;

	GladeIconSources  *pending_sources;
} GladeEPropIconSources;

GLADE_MAKE_EPROP (GladeEPropIconSources, glade_eprop_icon_sources)
#define GLADE_EPROP_ICON_SOURCES(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_ICON_SOURCES, GladeEPropIconSources))
#define GLADE_EPROP_ICON_SOURCES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_ICON_SOURCES, GladeEPropIconSourcesClass))
#define GLADE_IS_EPROP_ICON_SOURCES(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_ICON_SOURCES))
#define GLADE_IS_EPROP_ICON_SOURCES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_ICON_SOURCES))
#define GLADE_EPROP_ICON_SOURCES_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_ICON_SOURCES, GladeEPropIconSourcesClass))

static void
glade_eprop_icon_sources_finalize (GObject *object)
{
	/* Chain up */
	GObjectClass *parent_class = g_type_class_peek_parent (G_OBJECT_GET_CLASS (object));
	GladeEPropIconSources *eprop_sources = GLADE_EPROP_ICON_SOURCES (object);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
populate_store_foreach (const gchar           *icon_name,
			GList                 *sources,
			GladeEPropIconSources *eprop_sources)
{
	GtkIconSource *source; 
	GtkTreeIter    parent_iter, iter;
	GList         *l;

	/* Update the comboboxentry's store here... */
	gtk_list_store_append (eprop_sources->icon_names_store, &iter);
	gtk_list_store_set (eprop_sources->icon_names_store, &iter, 0, icon_name, -1);
	gtk_combo_box_set_active_iter (GTK_COMBO_BOX (eprop_sources->combo), &iter);

	/* Dont set COLUMN_ICON_NAME here */
	gtk_tree_store_append (eprop_sources->store, &parent_iter, NULL);
	gtk_tree_store_set (eprop_sources->store, &parent_iter, 
			    COLUMN_TEXT, icon_name,
			    COLUMN_TEXT_EDITABLE, FALSE, 
			    COLUMN_TEXT_WEIGHT, PANGO_WEIGHT_BOLD,
			    -1);

	for (l = sources; l; l = l->next)
	{
		GdkPixbuf       *pixbuf;
		gchar           *str;

		source = l->data;
		pixbuf = gtk_icon_source_get_pixbuf (source);
		str    = g_object_get_data (G_OBJECT (pixbuf), "GladeFileName");

		gtk_tree_store_append (eprop_sources->store, &iter, &parent_iter);
		gtk_tree_store_set (eprop_sources->store, &iter, 
				    COLUMN_ICON_NAME, icon_name,
				    COLUMN_LIST_INDEX, g_list_index (sources, source),
				    COLUMN_TEXT, str,
				    COLUMN_TEXT_EDITABLE, TRUE,
				    COLUMN_TEXT_WEIGHT, PANGO_WEIGHT_NORMAL,
				    -1);

		if (!gtk_icon_source_get_direction_wildcarded (source))
		{
			GtkTextDirection direction = gtk_icon_source_get_direction (source);
			str = glade_utils_enum_string_from_value (GTK_TYPE_TEXT_DIRECTION, direction);
			gtk_tree_store_set (eprop_sources->store, &iter, 
					    COLUMN_DIRECTION_ACTIVE, TRUE,
					    COLUMN_DIRECTION, str,
					    -1);
			g_free (str);
		}

		if (!gtk_icon_source_get_size_wildcarded (source))
		{
			GtkIconSize size = gtk_icon_source_get_size (source);
			str = glade_utils_enum_string_from_value (GTK_TYPE_ICON_SIZE, size);
			gtk_tree_store_set (eprop_sources->store, &iter, 
					    COLUMN_SIZE_ACTIVE, TRUE,
					    COLUMN_SIZE, str,
					    -1);
			g_free (str);
		}

		if (!gtk_icon_source_get_state_wildcarded (source))
		{
			GtkStateType state = gtk_icon_source_get_size (source);
			str = glade_utils_enum_string_from_value (GTK_TYPE_STATE_TYPE, state);
			gtk_tree_store_set (eprop_sources->store, &iter, 
					    COLUMN_STATE_ACTIVE, TRUE,
					    COLUMN_STATE, str,
					    -1);
			g_free (str);
		}

		/* Make sure its all expanded */
		if (!l->next)
		{
			GtkTreePath *tree_path =
				gtk_tree_model_get_path (GTK_TREE_MODEL (eprop_sources->store), &iter);
			gtk_tree_view_expand_to_path (eprop_sources->view, tree_path);
		}
	}
}

static void
populate_store (GladeEPropIconSources *eprop_sources)
{
	GladeIconSources *sources = NULL;

	gtk_tree_store_clear (eprop_sources->store);
	gtk_list_store_clear (eprop_sources->icon_names_store);

	glade_property_get (GLADE_EDITOR_PROPERTY (eprop_sources)->property, &sources);

	if (sources)
		g_hash_table_foreach (sources->sources, (GHFunc)populate_store_foreach, eprop_sources);
}

static void
glade_eprop_icon_sources_load (GladeEditorProperty *eprop, 
			       GladeProperty       *property)
{
	GladeEditorPropertyClass *parent_class = 
		g_type_class_peek_parent (GLADE_EDITOR_PROPERTY_GET_CLASS (eprop));
	GladeEPropIconSources *eprop_sources = GLADE_EPROP_ICON_SOURCES (eprop);

	/* Chain up in a clean state... */
	parent_class->load (eprop, property);

	populate_store (eprop_sources);

	gtk_widget_queue_draw (GTK_WIDGET (eprop_sources->view));
}


static void
add_clicked (GtkWidget *button, 
	     GladeEPropIconSources *eprop_sources)
{
	/* Remember to set focus on the cell and activate it ! */
	GtkTreeIter *parent_iter = NULL, iter, new_parent_iter;
	GtkTreePath *new_item_path;
	gchar       *icon_name;
	gchar       *selected_icon_name;

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (eprop_sources->combo), &iter))
		gtk_tree_model_get (GTK_TREE_MODEL (eprop_sources->icon_names_store), &iter,
				    0, &selected_icon_name, -1);

	if (!selected_icon_name)
		return;

	/* Find the right parent iter to add a child to... */
	if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (eprop_sources->store), &iter))
	{
		do {
			gtk_tree_model_get (GTK_TREE_MODEL (eprop_sources->store), &iter,
					    COLUMN_TEXT, &icon_name,
					    -1);

			if (icon_name && 
			    strcmp (icon_name, selected_icon_name) == 0)
				parent_iter = gtk_tree_iter_copy (&iter);

			g_free (icon_name);

		} while (parent_iter == NULL &&
			 gtk_tree_model_iter_next (GTK_TREE_MODEL (eprop_sources->store), &iter));
	}

	if (!parent_iter)
	{
		/* Dont set COLUMN_ICON_NAME here */
		gtk_tree_store_append (eprop_sources->store, &new_parent_iter, NULL);
		gtk_tree_store_set (eprop_sources->store, &new_parent_iter, 
				    COLUMN_TEXT, selected_icon_name,
				    COLUMN_TEXT_EDITABLE, FALSE, 
				    COLUMN_TEXT_WEIGHT, PANGO_WEIGHT_BOLD,
				    -1);
		parent_iter = gtk_tree_iter_copy (&new_parent_iter);
	}

	gtk_tree_store_append (eprop_sources->store, &iter, parent_iter);
	gtk_tree_store_set (eprop_sources->store, &iter, 
			    COLUMN_ICON_NAME, selected_icon_name,
			    COLUMN_TEXT_EDITABLE, TRUE,
			    COLUMN_TEXT_WEIGHT, PANGO_WEIGHT_NORMAL,
			    -1);

	new_item_path = gtk_tree_model_get_path (GTK_TREE_MODEL (eprop_sources->store), &iter);

	gtk_widget_grab_focus (GTK_WIDGET (eprop_sources->view));
	gtk_tree_view_expand_to_path (eprop_sources->view, new_item_path);
	gtk_tree_view_set_cursor (eprop_sources->view, new_item_path,
				  eprop_sources->filename_column, TRUE);

	g_free (selected_icon_name);
	gtk_tree_iter_free (parent_iter);
}

static GtkIconSource *
get_icon_source (GladeIconSources *sources,
		 const gchar      *icon_name,
		 gint              index)
{
	GList *source_list =
		g_hash_table_lookup (sources->sources, icon_name);

	if (source_list)
	{
		if (index < 0)
			return (GtkIconSource *)source_list->data;
		else 
			return (GtkIconSource *)g_list_nth_data (source_list, index);
	}
	return NULL;
}

static void
delete_clicked (GtkWidget *button, 
		GladeEPropIconSources *eprop_sources)
{

}

static gboolean
update_icon_sources_idle (GladeEditorProperty *eprop)
{
	GladeEPropIconSources *eprop_sources = GLADE_EPROP_ICON_SOURCES (eprop);
	GValue                 value = { 0, };
	
	g_value_init (&value, GLADE_TYPE_ICON_SOURCES);
	g_value_take_boxed (&value, eprop_sources->pending_sources);
	glade_editor_property_commit (eprop, &value);
	g_value_unset (&value);

	eprop_sources->pending_sources = NULL;
	return FALSE;
}

static gboolean
reload_icon_sources_idle (GladeEditorProperty *eprop)
{
	glade_editor_property_load (eprop, eprop->property);
	return FALSE;
}

static void
value_filename_edited (GtkCellRendererText *cell,
		       const gchar         *path,
		       const gchar         *new_text,
		       GladeEditorProperty *eprop)
{
	GladeEPropIconSources *eprop_sources = GLADE_EPROP_ICON_SOURCES (eprop);
	GtkTreeIter            iter;
	GladeIconSources      *icon_sources = NULL;
	GtkIconSource         *source;
	gchar                 *icon_name;
	gint                   index;
	GValue                *value;
	GdkPixbuf             *pixbuf;
	GList                 *source_list;

	if (!new_text || !new_text[0])
	{
		g_idle_add ((GSourceFunc)reload_icon_sources_idle, eprop);
		return;
	}

	if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (eprop_sources->store), &iter, path))
		return;

	gtk_tree_model_get (GTK_TREE_MODEL (eprop_sources->store), &iter,
			    COLUMN_ICON_NAME, &icon_name,
			    COLUMN_LIST_INDEX, &index,
			    -1);

	/* get new pixbuf value... */
	value = glade_utils_value_from_string (GDK_TYPE_PIXBUF, new_text,
					       eprop->property->widget->project,
					       eprop->property->widget);
	pixbuf = g_value_get_object (value);


	glade_property_get (eprop->property, &icon_sources);
	if (icon_sources)
	{
		icon_sources = glade_icon_sources_copy (icon_sources);
		if ((source = get_icon_source (icon_sources, icon_name, index)) != NULL)
			gtk_icon_source_set_pixbuf (source, pixbuf);
		else
		{
			source = gtk_icon_source_new ();
			gtk_icon_source_set_pixbuf (source, pixbuf);

			if ((source_list = g_hash_table_lookup (icon_sources->sources,
								icon_name)) != NULL)
				source_list = g_list_prepend (source_list, source);
			else
			{
				source_list = g_list_prepend (NULL, source);
				g_hash_table_insert (icon_sources->sources, g_strdup (icon_name), source_list);
			}
		}
	}
	else
	{
		icon_sources = glade_icon_sources_new ();
		source = gtk_icon_source_new ();
		gtk_icon_source_set_pixbuf (source, pixbuf);

		source_list = g_list_prepend (NULL, source);
		g_hash_table_insert (icon_sources->sources, g_strdup (icon_name), source_list);
	}

	g_value_unset (value);
	g_free (value);

	if (eprop_sources->pending_sources)
		glade_icon_sources_free (eprop_sources->pending_sources);

	eprop_sources->pending_sources = icon_sources;
	g_idle_add ((GSourceFunc)update_icon_sources_idle, eprop);
}

static void
value_attribute_toggled (GtkCellRendererToggle *cell_renderer,
			 gchar                 *path,
			 GladeEditorProperty   *eprop)
{
	GladeEPropIconSources *eprop_sources = GLADE_EPROP_ICON_SOURCES (eprop);
	GtkTreeIter            iter;
	GladeIconSources      *icon_sources = NULL;
	GtkIconSource         *source;
	gchar                 *icon_name;
	gint                   index, edit_column;
	gboolean               edit_column_active = FALSE;

	if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (eprop_sources->store), &iter, path))
		return;

	edit_column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell_renderer), "attribute-column"));
	gtk_tree_model_get (GTK_TREE_MODEL (eprop_sources->store), &iter,
			    COLUMN_ICON_NAME, &icon_name,
			    COLUMN_LIST_INDEX, &index,
			    edit_column, &edit_column_active,
			    -1);

	glade_property_get (eprop->property, &icon_sources);

	if (icon_sources)
		icon_sources = glade_icon_sources_copy (icon_sources);

	if (icon_sources && (source = get_icon_source (icon_sources, icon_name, index)) != NULL)
	{
		/* Note the reverse meaning of active toggles vs. wildcarded sources... */
		switch (edit_column)
		{
		case COLUMN_DIRECTION_ACTIVE:
			gtk_icon_source_set_direction_wildcarded (source, edit_column_active);
			break;
		case COLUMN_SIZE_ACTIVE:
			gtk_icon_source_set_size_wildcarded (source, edit_column_active);
			break;
		case COLUMN_STATE_ACTIVE:
			gtk_icon_source_set_state_wildcarded (source, edit_column_active);
			break;
		default:
			break;
		}

		if (eprop_sources->pending_sources)
			glade_icon_sources_free (eprop_sources->pending_sources);

		eprop_sources->pending_sources = icon_sources;
		g_idle_add ((GSourceFunc)update_icon_sources_idle, eprop);

		g_free (icon_name);
		return;
	}
	
	if (icon_sources)
		glade_icon_sources_free (icon_sources);
	g_free (icon_name);
	return;
}

static void
value_attribute_edited (GtkCellRendererText *cell,
			const gchar         *path,
			const gchar         *new_text,
			GladeEditorProperty *eprop)
{
	GladeEPropIconSources *eprop_sources = GLADE_EPROP_ICON_SOURCES (eprop);
	GtkTreeIter            iter;
	GladeIconSources      *icon_sources = NULL;
	GtkIconSource         *source;
	gchar                 *icon_name;
	gint                   index, edit_column;
	gboolean               edit_column_active = FALSE;

	if (!new_text || !new_text[0])
		return;

	if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (eprop_sources->store), &iter, path))
		return;

	edit_column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "attribute-column"));
	gtk_tree_model_get (GTK_TREE_MODEL (eprop_sources->store), &iter,
			    COLUMN_ICON_NAME, &icon_name,
			    COLUMN_LIST_INDEX, &index,
			    edit_column, &edit_column_active,
			    -1);

	glade_property_get (eprop->property, &icon_sources);

	if (icon_sources)
		icon_sources = glade_icon_sources_copy (icon_sources);

	if (icon_sources && (source = get_icon_source (icon_sources, icon_name, index)) != NULL)
	{
		GtkTextDirection direction;
		GtkIconSize size;
		GtkStateType state;

		switch (edit_column)
		{
		case COLUMN_DIRECTION:
			direction = glade_utils_enum_value_from_string (GTK_TYPE_TEXT_DIRECTION, new_text);
			gtk_icon_source_set_direction (source, direction);
			break;
		case COLUMN_SIZE:
			size = glade_utils_enum_value_from_string (GTK_TYPE_ICON_SIZE, new_text);
			gtk_icon_source_set_size (source, size);
			break;
		case COLUMN_STATE:
			state = glade_utils_enum_value_from_string (GTK_TYPE_STATE_TYPE, new_text);
			gtk_icon_source_set_state (source, state);
			break;
		default:
			break;
		}

		if (eprop_sources->pending_sources)
			glade_icon_sources_free (eprop_sources->pending_sources);

		eprop_sources->pending_sources = icon_sources;
		g_idle_add ((GSourceFunc)update_icon_sources_idle, eprop);

		g_free (icon_name);
		return;
	}
	
	if (icon_sources)
		glade_icon_sources_free (icon_sources);
	g_free (icon_name);
	return;
}

static GtkTreeView *
build_view (GladeEditorProperty *eprop)
{
	GladeEPropIconSources *eprop_sources = GLADE_EPROP_ICON_SOURCES (eprop);
	static GtkListStore   *direction_store = NULL, *size_store = NULL, *state_store = NULL;
	GtkTreeView           *view = (GtkTreeView *)gtk_tree_view_new ();
	GtkCellRenderer       *renderer;
	GtkTreeViewColumn     *column;

	if (!direction_store)
	{
		direction_store = glade_utils_liststore_from_enum_type (GTK_TYPE_TEXT_DIRECTION, FALSE);
		size_store      = glade_utils_liststore_from_enum_type (GTK_TYPE_ICON_SIZE, FALSE);
		state_store     = glade_utils_liststore_from_enum_type (GTK_TYPE_STATE_TYPE, FALSE);
	}

	/* Filename / icon name column/renderer */
 	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);
	g_signal_connect (G_OBJECT (renderer), "edited",
			  G_CALLBACK (value_filename_edited), eprop);

	eprop_sources->filename_column = 
		gtk_tree_view_column_new_with_attributes (_("File Name"),  renderer, 
							  "text", COLUMN_TEXT, 
							  "weight", COLUMN_TEXT_WEIGHT,
							  "editable", COLUMN_TEXT_EDITABLE,
							  NULL);
	gtk_tree_view_column_set_expand (eprop_sources->filename_column, TRUE);
 	gtk_tree_view_append_column (GTK_TREE_VIEW (view), eprop_sources->filename_column);

	/********************* Direction *********************/
	/* Attribute active portion */
 	renderer = gtk_cell_renderer_toggle_new ();
	g_object_set (G_OBJECT (renderer), "activatable", TRUE, NULL);
	g_object_set_data (G_OBJECT (renderer), "attribute-column", 
			   GINT_TO_POINTER (COLUMN_DIRECTION_ACTIVE));
	g_signal_connect (G_OBJECT (renderer), "toggled",
			  G_CALLBACK (value_attribute_toggled), eprop);
	
	column = gtk_tree_view_column_new_with_attributes
		("dummy",  renderer, 
		 "visible", COLUMN_TEXT_EDITABLE,
		 "active", COLUMN_DIRECTION_ACTIVE,
		 NULL);
 	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	/* Attribute portion */
 	renderer = gtk_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer), "editable", TRUE, "has-entry", FALSE, 
		      "text-column", 0, "model", direction_store, NULL);
	g_object_set_data (G_OBJECT (renderer), "attribute-column", 
			   GINT_TO_POINTER (COLUMN_DIRECTION));
	g_signal_connect (G_OBJECT (renderer), "edited",
			  G_CALLBACK (value_attribute_edited), eprop);

	column = gtk_tree_view_column_new_with_attributes
		("dummy",  renderer, 
		 "visible", COLUMN_TEXT_EDITABLE,
		 "editable", COLUMN_DIRECTION_ACTIVE,
		 "text", COLUMN_DIRECTION,
		 NULL);
 	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	/********************* Size *********************/
	/* Attribute active portion */
 	renderer = gtk_cell_renderer_toggle_new ();
	g_object_set (G_OBJECT (renderer), "activatable", TRUE, NULL);
	g_object_set_data (G_OBJECT (renderer), "attribute-column", 
			   GINT_TO_POINTER (COLUMN_SIZE_ACTIVE));
	g_signal_connect (G_OBJECT (renderer), "toggled",
			  G_CALLBACK (value_attribute_toggled), eprop);
	
	column = gtk_tree_view_column_new_with_attributes
		("dummy",  renderer, 
		 "visible", COLUMN_TEXT_EDITABLE,
		 "active", COLUMN_SIZE_ACTIVE,
		 NULL);
 	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	/* Attribute portion */
 	renderer = gtk_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer), "editable", TRUE, "has-entry", FALSE, 
		      "text-column", 0, "model", size_store, NULL);
	g_object_set_data (G_OBJECT (renderer), "attribute-column", 
			   GINT_TO_POINTER (COLUMN_SIZE));
	g_signal_connect (G_OBJECT (renderer), "edited",
			  G_CALLBACK (value_attribute_edited), eprop);

	column = gtk_tree_view_column_new_with_attributes
		("dummy",  renderer, 
		 "visible", COLUMN_TEXT_EDITABLE,
		 "editable", COLUMN_SIZE_ACTIVE,
		 "text", COLUMN_SIZE,
		 NULL);
 	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);
	

	/********************* State *********************/
	/* Attribute active portion */
 	renderer = gtk_cell_renderer_toggle_new ();
	g_object_set (G_OBJECT (renderer), "activatable", TRUE, NULL);
	g_object_set_data (G_OBJECT (renderer), "attribute-column", 
			   GINT_TO_POINTER (COLUMN_STATE_ACTIVE));
	g_signal_connect (G_OBJECT (renderer), "toggled",
			  G_CALLBACK (value_attribute_toggled), eprop);
	
	column = gtk_tree_view_column_new_with_attributes
		("dummy",  renderer, 
		 "visible", COLUMN_TEXT_EDITABLE,
		 "active", COLUMN_STATE_ACTIVE,
		 NULL);
 	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	/* Attribute portion */
 	renderer = gtk_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer), "editable", TRUE, "has-entry", FALSE, 
		      "text-column", 0, "model", state_store, NULL);
	g_object_set_data (G_OBJECT (renderer), "attribute-column", 
			   GINT_TO_POINTER (COLUMN_STATE));
	g_signal_connect (G_OBJECT (renderer), "edited",
			  G_CALLBACK (value_attribute_edited), eprop);

	column = gtk_tree_view_column_new_with_attributes
		("dummy",  renderer, 
		 "visible", COLUMN_TEXT_EDITABLE,
		 "editable", COLUMN_STATE_ACTIVE,
		 "text", COLUMN_STATE,
		 NULL);
 	gtk_tree_view_append_column (GTK_TREE_VIEW (view), column);

	/* Connect ::query-tooltip here for fancy tooltips... */
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (view), FALSE);
	gtk_tree_view_set_show_expanders (GTK_TREE_VIEW (view), FALSE);

	return view;
}

static void
icon_name_entry_activated (GtkEntry *entry, 
			   GladeEPropIconSources *eprop_sources)
{
	GtkTreeIter  iter;
	const gchar *text = gtk_entry_get_text (entry);

	if (!text || !text[0])
		return;

	gtk_list_store_append (eprop_sources->icon_names_store, &iter);
	gtk_list_store_set (eprop_sources->icon_names_store, &iter, 
			    0, text, -1);
	gtk_combo_box_set_active_iter (GTK_COMBO_BOX (eprop_sources->combo), &iter);
}

static GtkWidget *
glade_eprop_icon_sources_create_input (GladeEditorProperty *eprop)
{
	GladeEPropIconSources *eprop_sources = GLADE_EPROP_ICON_SOURCES (eprop);
	GtkWidget *vbox, *hbox, *button, *swin, *label;
	gchar *string;

	vbox = gtk_vbox_new (FALSE, 2);

	/* The label... */
	string = g_strdup_printf ("<b>%s</b>", _("Add and remove icon sources:"));
	label = gtk_label_new (string);
	g_free (string);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 2, 0);
	gtk_box_pack_start (GTK_BOX (vbox), label,  FALSE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, 4);

	/* hbox with comboboxentry add/remove source buttons on the right... */
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	eprop_sources->icon_names_store = gtk_list_store_new (1, G_TYPE_STRING);
	eprop_sources->combo = gtk_combo_box_entry_new_with_model 
		(GTK_TREE_MODEL (eprop_sources->icon_names_store), 0);
	g_signal_connect (G_OBJECT (GTK_BIN (eprop_sources->combo)->child), "activate",
			  G_CALLBACK (icon_name_entry_activated), eprop);

	gtk_box_pack_start (GTK_BOX (hbox), eprop_sources->combo,  TRUE, TRUE, 0);
	button = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_ADD,
							GTK_ICON_SIZE_BUTTON));
	gtk_box_pack_start (GTK_BOX (hbox), button,  FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (add_clicked), 
			  eprop_sources);
	
	button = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (button),
			      gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON));
	gtk_box_pack_start (GTK_BOX (hbox), button,  FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (delete_clicked), 
			  eprop_sources);

	/* Pack treeview/swindow on the left... */
	swin = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_box_pack_start (GTK_BOX (vbox), swin, TRUE, TRUE, 0);

	eprop_sources->view = build_view (eprop);
	gtk_container_add (GTK_CONTAINER (swin), GTK_WIDGET (eprop_sources->view));

	g_object_set (G_OBJECT (vbox), "height-request", 350, NULL);

	eprop_sources->store = gtk_tree_store_new (NUM_COLUMNS,    
						   G_TYPE_STRING,  // COLUMN_TEXT
						   G_TYPE_INT,     // COLUMN_TEXT_WEIGHT
						   G_TYPE_BOOLEAN, // COLUMN_TEXT_EDITABLE
						   G_TYPE_STRING,  // COLUMN_ICON_NAME
						   G_TYPE_INT,     // COLUMN_LIST_INDEX
						   G_TYPE_BOOLEAN, // COLUMN_DIRECTION_ACTIVE
						   G_TYPE_STRING,  // COLUMN_DIRECTION
						   G_TYPE_BOOLEAN, // COLUMN_SIZE_ACTIVE
						   G_TYPE_STRING,  // COLUMN_SIZE
						   G_TYPE_BOOLEAN, // COLUMN_STATE_ACTIVE,
						   G_TYPE_STRING); // COLUMN_STATE

	gtk_tree_view_set_model (eprop_sources->view, GTK_TREE_MODEL (eprop_sources->store));
	g_object_unref (G_OBJECT (eprop_sources->store)); // <-- pass ownership here
	
	gtk_widget_show_all (vbox);
	return vbox;
}
