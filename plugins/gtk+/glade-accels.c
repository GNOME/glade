/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Tristan Van Berkom
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

#include "glade-accels.h"

#define GLADE_RESPONSE_CLEAR 42

/**************************************************************
 *              GParamSpec stuff here
 **************************************************************/
struct _GladeParamSpecAccel {
	GParamSpec    parent_instance;
	
	GType         type; /* The type this accel key is for; this allows
			     * us to verify the validity of any signals for
			     * this type.
			     */
};

GList *
glade_accel_list_copy (GList *accels)
{
	GList          *ret = NULL, *list;
	GladeAccelInfo *info, *dup_info;

	for (list = accels; list; list = list->next)
	{
		info = list->data;

		dup_info            = g_new0 (GladeAccelInfo, 1);
		dup_info->signal    = g_strdup (info->signal);
		dup_info->key       = info->key;
		dup_info->modifiers = info->modifiers;

		ret = g_list_prepend (ret, dup_info);
	}

	return g_list_reverse (ret);
}

void
glade_accel_list_free (GList *accels)
{
	GList          *list;
	GladeAccelInfo *info;

	for (list = accels; list; list = list->next)
	{
		info = list->data;

		g_free (info->signal);
		g_free (info);
	}
	g_list_free (accels);
}

GType
glade_accel_glist_get_type (void)
{
	static GType type_id = 0;

	if (!type_id)
		type_id = g_boxed_type_register_static
			("GladeAccelGList", 
			 (GBoxedCopyFunc) glade_accel_list_copy,
			 (GBoxedFreeFunc) glade_accel_list_free);
	return type_id;
}

gboolean
glade_keyval_valid (guint val)
{
	gint i;

	for (i = 0; GladeKeys[i].name != NULL; i++)
	{
		if (GladeKeys[i].value == val)
			return TRUE;
	}
	return FALSE;
}


static void
param_accel_init (GParamSpec *pspec)
{
	GladeParamSpecAccel *ospec = GLADE_PARAM_SPEC_ACCEL (pspec);
	ospec->type = G_TYPE_OBJECT;
}

static void
param_accel_set_default (GParamSpec *pspec,
			 GValue     *value)
{
	if (value->data[0].v_pointer != NULL)
	{
		g_free (value->data[0].v_pointer);
	}
	value->data[0].v_pointer = NULL;
}

static gboolean
param_accel_validate (GParamSpec *pspec,
		      GValue     *value)
{
	/* GladeParamSpecAccel *aspec = GLADE_PARAM_SPEC_ACCEL (pspec); */
	GList               *accels, *list, *toremove = NULL;
	GladeAccelInfo      *info;

	accels = value->data[0].v_pointer;

	for (list = accels; list; list = list->next)
	{
		info = list->data;
		
		/* Is it an invalid key ? */
		if (glade_keyval_valid (info->key) == FALSE ||
		    /* Does the modifier contain any unwanted bits ? */
		    info->modifiers & GDK_MODIFIER_MASK ||
		    /* Do we have a signal ? */
		    /* FIXME: Check if the signal is valid for 'type' */
		    info->signal == NULL)
			toremove = g_list_prepend (toremove, info);
	}

	for (list = toremove; list; list = list->next)
		accels = g_list_remove (accels, list->data);

	if (toremove) g_list_free (toremove);
 
	value->data[0].v_pointer = accels;

	return toremove != NULL;
}

static gint
param_accel_values_cmp (GParamSpec   *pspec,
			  const GValue *value1,
			  const GValue *value2)
{
  guint8 *p1 = value1->data[0].v_pointer;
  guint8 *p2 = value2->data[0].v_pointer;

  /* not much to compare here, try to at least provide stable lesser/greater result */

  return p1 < p2 ? -1 : p1 > p2;
}

GType
glade_param_accel_get_type (void)
{
	static GType accel_type = 0;

	if (accel_type == 0)
	{
		static /* const */ GParamSpecTypeInfo pspec_info = {
			sizeof (GladeParamSpecAccel),  /* instance_size */
			16,                         /* n_preallocs */
			param_accel_init,         /* instance_init */
			0xdeadbeef,                 /* value_type, assigned further down */
			NULL,                       /* finalize */
			param_accel_set_default,  /* value_set_default */
			param_accel_validate,     /* value_validate */
			param_accel_values_cmp,   /* values_cmp */
		};
		pspec_info.value_type = GLADE_TYPE_ACCEL_GLIST;

		accel_type = g_param_type_register_static
			("GladeParamAccel", &pspec_info);
	}
	return accel_type;
}

GParamSpec *
glade_param_spec_accel (const gchar   *name,
			const gchar   *nick,
			const gchar   *blurb,
			GType          widget_type,
			GParamFlags    flags)
{
  GladeParamSpecAccel *pspec;

  pspec = g_param_spec_internal (GLADE_TYPE_PARAM_ACCEL,
				 name, nick, blurb, flags);
  
  pspec->type = widget_type;
  return G_PARAM_SPEC (pspec);
}

/* Accelerator spec */
GParamSpec *
glade_standard_accel_spec (void)
{
	return glade_param_spec_accel ("accelerators", _("Accelerators"),
				       _("A list of accelerator keys"), 
				       GTK_TYPE_WIDGET,
				       G_PARAM_READWRITE);
}

guint
glade_key_from_string (const gchar *string)
{
	gint i;

	g_return_val_if_fail (string != NULL, 0);

	for (i = 0; GladeKeys[i].name != NULL; i++)
		if (!strcmp (string, GladeKeys[i].name))
			return GladeKeys[i].value;

	return 0;
}

const gchar *
glade_string_from_key (guint key)
{
	gint i;

	for (i = 0; GladeKeys[i].name != NULL; i++)
		if (GladeKeys[i].value == key)
			return GladeKeys[i].name;
	return NULL;
}


/* This is not used to save in the glade file... and its a one-way conversion.
 * its only usefull to show the values in the UI.
 */
gchar *
glade_accels_make_string (GList *accels)
{
	GladeAccelInfo *info;
	GString        *string;
	GList          *list;

	string = g_string_new ("");

	for (list = accels; list; list = list->next)
	{
		info = list->data;
		
		if (info->modifiers & GDK_SHIFT_MASK)
			g_string_append (string, "SHIFT-");

		if (info->modifiers & GDK_CONTROL_MASK)
			g_string_append (string, "CNTL-");

		if (info->modifiers & GDK_MOD1_MASK)
			g_string_append (string, "ALT-");

		g_string_append (string, glade_string_from_key (info->key));

		if (list->next)
			g_string_append (string, ", ");
	}

	return g_string_free (string, FALSE);
}


/**************************************************************
 *              GladeEditorProperty stuff here
 **************************************************************/

enum {
	ACCEL_COLUMN_SIGNAL = 0,
	ACCEL_COLUMN_REAL_SIGNAL,
	ACCEL_COLUMN_KEY,
	ACCEL_COLUMN_MOD_SHIFT,
	ACCEL_COLUMN_MOD_CNTL,
	ACCEL_COLUMN_MOD_ALT,
	ACCEL_COLUMN_IS_CLASS,
	ACCEL_COLUMN_IS_SIGNAL,
	ACCEL_COLUMN_KEY_ENTERED,
	ACCEL_COLUMN_KEY_SLOT,
	ACCEL_NUM_COLUMNS
};

enum {
	ACCEL_COMBO_COLUMN_TEXT = 0,
	ACCEL_COMBO_NUM_COLUMNS,
};

typedef struct {
	GladeEditorProperty parent_instance;
	
	GtkWidget    *entry;
	GList        *parent_iters;
	GtkTreeModel *model;
} GladeEPropAccel;

typedef struct {
	GtkTreeIter *iter;
	gchar       *name; /* <-- dont free */
} GladeEpropIterTab;


static GtkTreeModel *keysyms_model   = NULL;

GLADE_MAKE_EPROP (GladeEPropAccel, glade_eprop_accel)
#define GLADE_EPROP_ACCEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_EPROP_ACCEL, GladeEPropAccel))
#define GLADE_EPROP_ACCEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_EPROP_ACCEL, GladeEPropAccelClass))
#define GLADE_IS_EPROP_ACCEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_EPROP_ACCEL))
#define GLADE_IS_EPROP_ACCEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_EPROP_ACCEL))
#define GLADE_EPROP_ACCEL_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GLADE_EPROP_ACCEL, GladeEPropAccelClass))


static GtkTreeModel *
create_keysyms_model (void)
{
	GtkTreeModel *model;
	GtkTreeIter   iter, alphanum, fkey, keypad, other, extra;
	GtkTreeIter  *parent;
	gint          i;

	model = (GtkTreeModel *)gtk_tree_store_new
		(ACCEL_COMBO_NUM_COLUMNS,
		 G_TYPE_STRING);       /* The Key charachter name */

	gtk_tree_store_append (GTK_TREE_STORE (model), &alphanum, NULL);
	gtk_tree_store_set    
		(GTK_TREE_STORE (model), &alphanum, 
		 ACCEL_COMBO_COLUMN_TEXT, _("Alphanumerical"), -1);

	gtk_tree_store_append (GTK_TREE_STORE (model), &extra, NULL);
	gtk_tree_store_set    
		(GTK_TREE_STORE (model), &extra, 
		 ACCEL_COMBO_COLUMN_TEXT, _("Extra"), -1);

	gtk_tree_store_append (GTK_TREE_STORE (model), &keypad, NULL);
	gtk_tree_store_set    
		(GTK_TREE_STORE (model), &keypad, 
		 ACCEL_COMBO_COLUMN_TEXT, _("Keypad"), -1);

	gtk_tree_store_append (GTK_TREE_STORE (model), &fkey, NULL);
	gtk_tree_store_set    
		(GTK_TREE_STORE (model), &fkey, 
		 ACCEL_COMBO_COLUMN_TEXT, _("Functions"), -1);
	
	gtk_tree_store_append (GTK_TREE_STORE (model), &other, NULL);
	gtk_tree_store_set    
		(GTK_TREE_STORE (model), &other, 
		 ACCEL_COMBO_COLUMN_TEXT, _("Other"), -1);

	parent = &alphanum;

	for (i = 0; GladeKeys[i].name != NULL; i++)
	{
		gtk_tree_store_append (GTK_TREE_STORE (model), &iter, parent);
		gtk_tree_store_set    
			(GTK_TREE_STORE (model), &iter, 
			 ACCEL_COMBO_COLUMN_TEXT, GladeKeys[i].name, -1);

		if (!strcmp (GladeKeys[i].name, GLADE_KEYS_LAST_ALPHANUM))
			parent = &extra;
		else if (!strcmp (GladeKeys[i].name, GLADE_KEYS_LAST_EXTRA))
			parent = &keypad;
		else if (!strcmp (GladeKeys[i].name, GLADE_KEYS_LAST_KP))
			parent = &fkey;
		else if (!strcmp (GladeKeys[i].name, GLADE_KEYS_LAST_FKEY))
			parent = &other;
	}
	return model;
}

static void
glade_eprop_accel_finalize (GObject *object)
{
	/* Chain up */
	GObjectClass *parent_class = g_type_class_peek_parent (G_OBJECT_GET_CLASS (object));

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
glade_eprop_accel_load (GladeEditorProperty *eprop, 
			GladeProperty       *property)
{
	GladeEditorPropertyClass *parent_class = 
		g_type_class_peek_parent (GLADE_EDITOR_PROPERTY_GET_CLASS (eprop));
	GladeEPropAccel *eprop_accel = GLADE_EPROP_ACCEL (eprop);
	gchar           *accels;

	/* Chain up first */
	parent_class->load (eprop, property);

	if (property == NULL) return;

	if ((accels = 
	     glade_accels_make_string (g_value_get_boxed (property->value))) != NULL)
	{
		gtk_entry_set_text (GTK_ENTRY (eprop_accel->entry), accels);
		g_free (accels);
	}
	else
		gtk_entry_set_text (GTK_ENTRY (eprop_accel->entry), "");

}

static gint
eprop_find_iter (GladeEpropIterTab *iter_tab,
		 gchar             *name)
{
	return strcmp (iter_tab->name, name);
}

static void
iter_tab_free (GladeEpropIterTab *iter_tab)
{
	gtk_tree_iter_free (iter_tab->iter);
	g_free (iter_tab);
}

static void
glade_eprop_accel_populate_view (GladeEditorProperty *eprop,
				 GtkTreeView         *view)
{
	GladeEPropAccel    *eprop_accel = GLADE_EPROP_ACCEL (eprop);
	GladeSignalClass   *sclass;
	GladeWidgetAdaptor *adaptor = glade_widget_adaptor_from_pclass (eprop->klass);
	GtkTreeStore       *model = (GtkTreeStore *)gtk_tree_view_get_model (view);
	GtkTreeIter         iter;
	GladeEpropIterTab  *parent_tab;
	GladeAccelInfo     *info;
	GList              *list, *l, *found, *accelerators;
	gchar              *name;

	accelerators = g_value_get_boxed (eprop->property->value);

	/* First make parent iters...
	 */
	for (list = adaptor->signals; list; list = list->next)
	{
		sclass = list->data;

		/* Only action signals have accelerators. */
		if ((sclass->query.signal_flags & G_SIGNAL_ACTION) == 0)
			continue;

		if (g_list_find_custom (eprop_accel->parent_iters, 
					sclass->type,
					(GCompareFunc)eprop_find_iter) == NULL)
		{
			gtk_tree_store_append (model, &iter, NULL);
			gtk_tree_store_set (model, &iter,
					    ACCEL_COLUMN_SIGNAL, sclass->type,
					    ACCEL_COLUMN_IS_CLASS, TRUE,
					    ACCEL_COLUMN_IS_SIGNAL, FALSE,
					    -1);
			
			parent_tab = g_new0 (GladeEpropIterTab, 1);
			parent_tab->name = sclass->type;
			parent_tab->iter = gtk_tree_iter_copy (&iter);

			eprop_accel->parent_iters = 
				g_list_prepend (eprop_accel->parent_iters,
						parent_tab);
		}
	}

	/* Now we populate...
	 */
	for (list = adaptor->signals; list; list = list->next)
	{
		sclass = list->data;

		/* Only action signals have accelerators. */
		if ((sclass->query.signal_flags & G_SIGNAL_ACTION) == 0)
			continue;

		if ((found = g_list_find_custom (eprop_accel->parent_iters, 
						 sclass->type,
						 (GCompareFunc)eprop_find_iter)) != NULL)
		{
			parent_tab = found->data;
			name       = g_strdup_printf ("    %s", sclass->name);

			/* Populate from accelerator list
			 */
			for (l = accelerators; l; l = l->next)
			{
				info = l->data;

				if (strcmp (info->signal, sclass->name))
					continue;

				gtk_tree_store_append (model, &iter, parent_tab->iter);
				gtk_tree_store_set    
					(model, &iter,
					 ACCEL_COLUMN_SIGNAL, name,
					 ACCEL_COLUMN_REAL_SIGNAL, sclass->name,
					 ACCEL_COLUMN_IS_CLASS, FALSE,
					 ACCEL_COLUMN_IS_SIGNAL, TRUE,
					 ACCEL_COLUMN_MOD_SHIFT, 
					 (info->modifiers & GDK_SHIFT_MASK) != 0,
					 ACCEL_COLUMN_MOD_CNTL, 
					 (info->modifiers & GDK_CONTROL_MASK) != 0,
					 ACCEL_COLUMN_MOD_ALT,
					 (info->modifiers & GDK_MOD1_MASK) != 0,
					 ACCEL_COLUMN_KEY, 
					 glade_string_from_key (info->key),
					 ACCEL_COLUMN_KEY_ENTERED, TRUE,
					 ACCEL_COLUMN_KEY_SLOT, FALSE,
					 -1);
			}

			/* Append a new empty slot at the end */
			gtk_tree_store_append (model, &iter, parent_tab->iter);
			gtk_tree_store_set    
				(model, &iter,
				 ACCEL_COLUMN_SIGNAL, name,
				 ACCEL_COLUMN_REAL_SIGNAL, sclass->name,
				 ACCEL_COLUMN_IS_CLASS, FALSE,
				 ACCEL_COLUMN_IS_SIGNAL, TRUE,
				 ACCEL_COLUMN_MOD_SHIFT, FALSE,
				 ACCEL_COLUMN_MOD_CNTL, FALSE,
				 ACCEL_COLUMN_MOD_ALT, FALSE,
				 ACCEL_COLUMN_KEY, _("<choose a key>"),
				 ACCEL_COLUMN_KEY_ENTERED, FALSE,
				 ACCEL_COLUMN_KEY_SLOT, TRUE,
				 -1);

			g_free (name);
		}
	}
}

static void
key_edited (GtkCellRendererText *cell,
	    const gchar         *path_string,
	    const gchar         *new_text,
	    GladeEditorProperty *eprop)
{
	GladeEPropAccel *eprop_accel = GLADE_EPROP_ACCEL (eprop);
	gboolean         key_was_set;
	const gchar     *text;
	GtkTreeIter      iter, parent_iter, new_iter;

	if (!gtk_tree_model_get_iter_from_string (eprop_accel->model,
						  &iter, path_string))
		return;

	gtk_tree_model_get (eprop_accel->model, &iter,
			    ACCEL_COLUMN_KEY_ENTERED, &key_was_set,
			    -1);

	/* If user selects "none"; remove old entry or ignore new one.
	 */
	if (!new_text || new_text[0] == '\0' ||
	    glade_string_from_key ((guint)new_text[0]) == NULL ||
	    g_utf8_collate (new_text, _("None")) == 0 ||
	    g_utf8_collate (new_text, _("<choose a key>")) == 0)
	{
		if (key_was_set)
			gtk_tree_store_remove
				(GTK_TREE_STORE (eprop_accel->model), &iter);

		return;
	}

	if (glade_key_from_string (new_text) != 0)
		text = new_text;
	else
		text = glade_string_from_key ((guint)new_text[0]);

	gtk_tree_store_set    
		(GTK_TREE_STORE (eprop_accel->model), &iter,
		 ACCEL_COLUMN_KEY, text,
		 ACCEL_COLUMN_KEY_ENTERED, TRUE,
		 ACCEL_COLUMN_KEY_SLOT, FALSE,
		 -1);

	/* Append a new one if needed
	 */
	if (key_was_set == FALSE &&
	    gtk_tree_model_iter_parent (eprop_accel->model,
					&parent_iter, &iter))
	{	
		gchar *signal, *real_signal;
		
		gtk_tree_model_get (eprop_accel->model, &iter,
				    ACCEL_COLUMN_SIGNAL, &signal,
				    ACCEL_COLUMN_REAL_SIGNAL, &real_signal,
				    -1);
		
		/* Append a new empty slot at the end */
		gtk_tree_store_insert_after (GTK_TREE_STORE (eprop_accel->model), 
					     &new_iter, &parent_iter, &iter);
		gtk_tree_store_set (GTK_TREE_STORE (eprop_accel->model), &new_iter,
				    ACCEL_COLUMN_SIGNAL, signal,
				    ACCEL_COLUMN_REAL_SIGNAL, real_signal,
				    ACCEL_COLUMN_IS_CLASS, FALSE,
				    ACCEL_COLUMN_IS_SIGNAL, TRUE,
				    ACCEL_COLUMN_MOD_SHIFT, FALSE,
				    ACCEL_COLUMN_MOD_CNTL, FALSE,
				    ACCEL_COLUMN_MOD_ALT, FALSE,
				    ACCEL_COLUMN_KEY, _("<choose a key>"),
				    ACCEL_COLUMN_KEY_ENTERED, FALSE,
				    ACCEL_COLUMN_KEY_SLOT, TRUE,
				    -1);
		g_free (signal);
		g_free (real_signal);
	}
}

static void
modifier_toggled (GtkCellRendererToggle *cell,
		  gchar                 *path_string,
		  GladeEditorProperty   *eprop)
{
	GladeEPropAccel   *eprop_accel = GLADE_EPROP_ACCEL (eprop);
	GtkTreeIter        iter;
	gint               column;
	gboolean           active, key_entered;

	if (!gtk_tree_model_get_iter_from_string (eprop_accel->model,
						  &iter, path_string))
		return;

	column = GPOINTER_TO_INT (g_object_get_data
				  (G_OBJECT (cell), "model-column"));

	gtk_tree_model_get
		(eprop_accel->model, &iter,
		 ACCEL_COLUMN_KEY_ENTERED, &key_entered,
		 column, &active, -1);

	if (key_entered)
		gtk_tree_store_set
			(GTK_TREE_STORE (eprop_accel->model), &iter,
			 column, !active, -1);
}


static GtkWidget *
glade_eprop_accel_view (GladeEditorProperty *eprop)
{
	GladeEPropAccel   *eprop_accel = GLADE_EPROP_ACCEL (eprop);
	GtkWidget         *view_widget;
 	GtkCellRenderer   *renderer;
	GtkTreeViewColumn *column;

	eprop_accel->model = (GtkTreeModel *)gtk_tree_store_new
		(ACCEL_NUM_COLUMNS,
		 G_TYPE_STRING,        /* The GSignal name formatted for display */
		 G_TYPE_STRING,        /* The GSignal name */
		 G_TYPE_STRING,        /* The Gdk keycode */
		 G_TYPE_BOOLEAN,       /* The shift modifier */
		 G_TYPE_BOOLEAN,       /* The cntl modifier */
		 G_TYPE_BOOLEAN,       /* The alt modifier */
		 G_TYPE_BOOLEAN,       /* Whether this is a class entry */
		 G_TYPE_BOOLEAN,       /* Whether this is a signal entry (oposite of above) */
		 G_TYPE_BOOLEAN,       /* Whether the key has been entered for this row */
		 G_TYPE_BOOLEAN);      /* Oposite of above */

	view_widget = gtk_tree_view_new_with_model (eprop_accel->model);
	g_object_set (G_OBJECT (view_widget), "enable-search", FALSE, NULL);
	
	/********************* fake invisible column *********************/
 	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), "editable", FALSE, "visible", FALSE, NULL);

	column = gtk_tree_view_column_new_with_attributes (NULL, renderer, NULL);
 	gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

	gtk_tree_view_column_set_visible (column, FALSE);
	gtk_tree_view_set_expander_column (GTK_TREE_VIEW (view_widget), column);

	/********************* signal name column *********************/
 	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer), 
		      "editable", FALSE, 
		      "weight", PANGO_WEIGHT_BOLD,
		      NULL);

	column = gtk_tree_view_column_new_with_attributes
		(_("Signal"),  renderer, 
		 "text", ACCEL_COLUMN_SIGNAL, 
		 "weight-set", ACCEL_COLUMN_IS_CLASS,
		 NULL);

	g_object_set (G_OBJECT (column), "expand", TRUE, NULL);

 	gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

	/********************* key name column *********************/
	if (keysyms_model == NULL)
		keysyms_model = create_keysyms_model ();

 	renderer = gtk_cell_renderer_combo_new ();
	g_object_set (G_OBJECT (renderer), 
		      "editable",    TRUE,
		      "model",       keysyms_model,
		      "text-column", ACCEL_COMBO_COLUMN_TEXT,
		      "has-entry",   TRUE,
		      "style",       PANGO_STYLE_ITALIC,
		      "foreground",  "Gray", 
		      NULL);

	g_signal_connect (renderer, "edited",
			  G_CALLBACK (key_edited), eprop);

	column = gtk_tree_view_column_new_with_attributes
		(_("Key"),         renderer, 
		 "text",           ACCEL_COLUMN_KEY,
		 "style-set",      ACCEL_COLUMN_KEY_SLOT,
		 "foreground-set", ACCEL_COLUMN_KEY_SLOT,
 		 "visible",        ACCEL_COLUMN_IS_SIGNAL,
		 NULL);

	g_object_set (G_OBJECT (column), "expand", TRUE, NULL);

 	gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

	/********************* shift modifier column *********************/
 	renderer = gtk_cell_renderer_toggle_new ();
	column   = gtk_tree_view_column_new_with_attributes
		(_("Shift"),  renderer, 
		 "visible", ACCEL_COLUMN_IS_SIGNAL,
		 "sensitive", ACCEL_COLUMN_KEY_ENTERED,
		 "active", ACCEL_COLUMN_MOD_SHIFT,
		 NULL);

	g_object_set_data (G_OBJECT (renderer), "model-column",
			   GINT_TO_POINTER (ACCEL_COLUMN_MOD_SHIFT));
	g_signal_connect (G_OBJECT (renderer), "toggled",
			  G_CALLBACK (modifier_toggled), eprop);

 	gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

	/********************* control modifier column *********************/
 	renderer = gtk_cell_renderer_toggle_new ();
	column   = gtk_tree_view_column_new_with_attributes
		(_("Control"),  renderer, 
		 "visible", ACCEL_COLUMN_IS_SIGNAL,
		 "sensitive", ACCEL_COLUMN_KEY_ENTERED,
		 "active", ACCEL_COLUMN_MOD_CNTL,
		 NULL);

	g_object_set_data (G_OBJECT (renderer), "model-column",
			   GINT_TO_POINTER (ACCEL_COLUMN_MOD_CNTL));
	g_signal_connect (G_OBJECT (renderer), "toggled",
			  G_CALLBACK (modifier_toggled), eprop);

 	gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

	/********************* alt modifier column *********************/
 	renderer = gtk_cell_renderer_toggle_new ();
	column   = gtk_tree_view_column_new_with_attributes
		(_("Alt"),  renderer, 
		 "visible", ACCEL_COLUMN_IS_SIGNAL,
		 "sensitive", ACCEL_COLUMN_KEY_ENTERED,
		 "active", ACCEL_COLUMN_MOD_ALT,
		 NULL);

	g_object_set_data (G_OBJECT (renderer), "model-column",
			   GINT_TO_POINTER (ACCEL_COLUMN_MOD_ALT));
	g_signal_connect (G_OBJECT (renderer), "toggled",
			  G_CALLBACK (modifier_toggled), eprop);

 	gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

	return view_widget;
}

static gboolean
glade_eprop_accel_accum_accelerators (GtkTreeModel  *model,
				      GtkTreePath    *path,
				      GtkTreeIter    *iter,
				      GList         **ret)
{
	GladeAccelInfo *info;
	gchar          *signal, *key_str;
	gboolean        shift, cntl, alt, entered;

	gtk_tree_model_get (model, iter, ACCEL_COLUMN_KEY_ENTERED, &entered, -1);
	if (entered == FALSE) return FALSE;
	
	gtk_tree_model_get (model, iter,
			    ACCEL_COLUMN_REAL_SIGNAL, &signal,
			    ACCEL_COLUMN_KEY,         &key_str,
			    ACCEL_COLUMN_MOD_SHIFT,   &shift,
			    ACCEL_COLUMN_MOD_CNTL,    &cntl,
			    ACCEL_COLUMN_MOD_ALT,     &alt,
			    -1);

	info            = g_new0 (GladeAccelInfo, 1);
	info->signal    = signal;
	info->key       = glade_key_from_string (key_str);
	info->modifiers = (shift ? GDK_SHIFT_MASK   : 0) |
			  (cntl  ? GDK_CONTROL_MASK : 0) |
			  (alt   ? GDK_MOD1_MASK    : 0);

	*ret = g_list_prepend (*ret, info);

	g_free (key_str);
	
	return FALSE;
}


static void
glade_eprop_accel_show_dialog (GtkWidget           *dialog_button,
			       GladeEditorProperty *eprop)
{
	GladeEPropAccel  *eprop_accel = GLADE_EPROP_ACCEL (eprop);
	GtkWidget        *dialog, *parent, *vbox, *sw, *tree_view;
	GladeProject     *project;
	GValue           *value;
	GList            *accelerators = NULL;
	gint              res;
	
	project = glade_widget_get_project (eprop->property->widget);
	parent = gtk_widget_get_toplevel (GTK_WIDGET (eprop));

	dialog = gtk_dialog_new_with_buttons (_("Choose accelerator keys..."),
					      GTK_WINDOW (parent),
					      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_CLEAR, GLADE_RESPONSE_CLEAR,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);

	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);

	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 0);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (sw);
	gtk_box_pack_start (GTK_BOX (vbox), sw, TRUE, TRUE, 0);
	gtk_widget_set_size_request (sw, 400, 200);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);

	tree_view = glade_eprop_accel_view (eprop);
	glade_eprop_accel_populate_view (eprop, GTK_TREE_VIEW (tree_view));

	gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

	gtk_widget_show (tree_view);
	gtk_container_add (GTK_CONTAINER (sw), tree_view);
	
	/* Run the dialog */
	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_OK) 
	{
		gtk_tree_model_foreach
			(gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view)),
			 (GtkTreeModelForeachFunc)
			 glade_eprop_accel_accum_accelerators, &accelerators);
		
		value = g_new0 (GValue, 1);
		g_value_init (value, GLADE_TYPE_ACCEL_GLIST);
		g_value_take_boxed (value, accelerators);

		glade_editor_property_commit (eprop, value);

		g_value_unset (value);
		g_free (value);
	} 
	else if (res == GLADE_RESPONSE_CLEAR)
	{
		value = g_new0 (GValue, 1);
		g_value_init (value, GLADE_TYPE_ACCEL_GLIST);
		g_value_set_boxed (value, NULL);

		glade_editor_property_commit (eprop, value);

		g_value_unset (value);
		g_free (value);
	}

	/* Clean up ...
	 */
	gtk_widget_destroy (dialog);

	g_object_unref (G_OBJECT (eprop_accel->model));
	eprop_accel->model = NULL;

	if (eprop_accel->parent_iters)
	{
		g_list_foreach (eprop_accel->parent_iters, (GFunc)iter_tab_free, NULL);
		g_list_free (eprop_accel->parent_iters);
		eprop_accel->parent_iters = NULL;
	}

}

static GtkWidget *
glade_eprop_accel_create_input (GladeEditorProperty *eprop)
{
	GladeEPropAccel *eprop_accel = GLADE_EPROP_ACCEL (eprop);
	GtkWidget        *hbox;
	GtkWidget        *button;

	hbox               = gtk_hbox_new (FALSE, 0);
	eprop_accel->entry = gtk_entry_new ();
	gtk_entry_set_editable (GTK_ENTRY (eprop_accel->entry), FALSE);
	gtk_widget_show (eprop_accel->entry);
	gtk_box_pack_start (GTK_BOX (hbox), eprop_accel->entry, TRUE, TRUE, 0);

	button = gtk_button_new_with_label ("...");
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (hbox), button,  FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (glade_eprop_accel_show_dialog), 
			  eprop);

	return hbox;
}
