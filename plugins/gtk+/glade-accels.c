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

GList *
glade_accel_list_copy (GList * accels)
{
  GList *ret = NULL, *list;
  GladeAccelInfo *info, *dup_info;

  for (list = accels; list; list = list->next)
    {
      info = list->data;

      dup_info = g_new0 (GladeAccelInfo, 1);
      dup_info->signal = g_strdup (info->signal);
      dup_info->key = info->key;
      dup_info->modifiers = info->modifiers;

      ret = g_list_prepend (ret, dup_info);
    }

  return g_list_reverse (ret);
}

void
glade_accel_list_free (GList * accels)
{
  GList *list;
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

/* This is not used to save in the glade file... and its a one-way conversion.
 * its only usefull to show the values in the UI.
 */
gchar *
glade_accels_make_string (GList * accels)
{
  GladeAccelInfo *info;
  GString *string;
  GList *list;
  gchar *accel_text;

  string = g_string_new ("");

  for (list = accels; list; list = list->next)
    {
      info = list->data;

      accel_text = gtk_accelerator_name (info->key, info->modifiers);
      g_string_append (string, accel_text);
      g_free (accel_text);

      if (list->next)
        g_string_append (string, ", ");
    }

  return g_string_free (string, FALSE);
}


/**************************************************************
 *              GladeEditorProperty stuff here
 **************************************************************/

enum
{
  ACCEL_COLUMN_SIGNAL = 0,
  ACCEL_COLUMN_REAL_SIGNAL,
  ACCEL_COLUMN_TEXT,
  ACCEL_COLUMN_WEIGHT,
  ACCEL_COLUMN_STYLE,
  ACCEL_COLUMN_FOREGROUND,
  ACCEL_COLUMN_VISIBLE,
  ACCEL_COLUMN_KEY_ENTERED,
  ACCEL_COLUMN_KEYCODE,
  ACCEL_COLUMN_MODIFIERS,
  ACCEL_NUM_COLUMNS
};

struct _GladeEPropAccel
{
  GladeEditorProperty parent_instance;

  GtkWidget *entry;
  GList *parent_iters;
  GtkTreeModel *model;
} ;

typedef struct
{
  GtkTreeIter *iter;
  const gchar *name;                  /* <-- dont free */
} GladeEpropIterTab;

GLADE_MAKE_EPROP (GladeEPropAccel, glade_eprop_accel, GLADE, EPROP_ACCEL)

static void
glade_eprop_accel_finalize (GObject * object)
{
  /* Chain up */
  GObjectClass *parent_class =
      g_type_class_peek_parent (G_OBJECT_GET_CLASS (object));

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
glade_eprop_accel_load (GladeEditorProperty * eprop, GladeProperty * property)
{
  GladeEditorPropertyClass *parent_class =
      g_type_class_peek_parent (GLADE_EDITOR_PROPERTY_GET_CLASS (eprop));
  GladeEPropAccel *eprop_accel = GLADE_EPROP_ACCEL (eprop);
  gchar *accels;

  /* Chain up first */
  parent_class->load (eprop, property);

  if (property == NULL)
    return;

  if ((accels =
       glade_accels_make_string (g_value_get_boxed (glade_property_inline_value (property)))) != NULL)
    {
      gtk_entry_set_text (GTK_ENTRY (eprop_accel->entry), accels);
      g_free (accels);
    }
  else
    gtk_entry_set_text (GTK_ENTRY (eprop_accel->entry), "");

}

static gint
eprop_find_iter (GladeEpropIterTab * iter_tab, gchar * name)
{
  return strcmp (iter_tab->name, name);
}

static void
iter_tab_free (GladeEpropIterTab * iter_tab)
{
  gtk_tree_iter_free (iter_tab->iter);
  g_free (iter_tab);
}

static void
glade_eprop_accel_populate_view (GladeEditorProperty * eprop,
                                 GtkTreeView * view)
{
  GladeEPropAccel *eprop_accel = GLADE_EPROP_ACCEL (eprop);
  GladeSignalDef *sdef;
  GladePropertyDef   *pdef = glade_editor_property_get_property_def (eprop);
  GladeProperty      *property = glade_editor_property_get_property (eprop);
  GladeWidgetAdaptor *adaptor = glade_property_def_get_adaptor (pdef);
  GtkTreeStore *model = (GtkTreeStore *) gtk_tree_view_get_model (view);
  GtkTreeIter iter;
  GladeEpropIterTab *parent_tab;
  GladeAccelInfo *info;
  GList *l, *found, *accelerators;
  gchar *name, *accel_text;
  const GList *list;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GType type_action = GTK_TYPE_ACTION;
G_GNUC_END_IGNORE_DEPRECATIONS

  accelerators = g_value_get_boxed (glade_property_inline_value (property));

  /* First make parent iters...
   */
  for (list = glade_widget_adaptor_get_signals (adaptor); list; list = list->next)
    {
      sdef = list->data;

      /* Special case for GtkAction accelerators  */
      if (glade_widget_adaptor_get_object_type (adaptor) == type_action ||
          g_type_is_a (glade_widget_adaptor_get_object_type (adaptor), type_action))
        {
          if (g_strcmp0 (glade_signal_def_get_object_type_name (sdef), "GtkAction") != 0 ||
              g_strcmp0 (glade_signal_def_get_name (sdef), "activate") != 0)
            continue;
        }
      /* Only action signals have accelerators. */
      else if ((glade_signal_def_get_flags (sdef) & G_SIGNAL_ACTION) == 0)
        continue;

      if (g_list_find_custom (eprop_accel->parent_iters,
                              glade_signal_def_get_object_type_name (sdef),
                              (GCompareFunc) eprop_find_iter) == NULL)
        {
          gtk_tree_store_append (model, &iter, NULL);
          gtk_tree_store_set (model, &iter,
                              ACCEL_COLUMN_SIGNAL, glade_signal_def_get_object_type_name (sdef),
                              ACCEL_COLUMN_WEIGHT, PANGO_WEIGHT_BOLD,
                              ACCEL_COLUMN_VISIBLE, FALSE, -1);

          parent_tab = g_new0 (GladeEpropIterTab, 1);
          parent_tab->name = glade_signal_def_get_object_type_name (sdef);
          parent_tab->iter = gtk_tree_iter_copy (&iter);

          eprop_accel->parent_iters =
              g_list_prepend (eprop_accel->parent_iters, parent_tab);
        }
    }

  /* Now we populate...
   */
  for (list = glade_widget_adaptor_get_signals (adaptor); list; list = list->next)
    {
      sdef = list->data;

      /* Special case for GtkAction accelerators  */
      if (glade_widget_adaptor_get_object_type (adaptor) == type_action ||
          g_type_is_a (glade_widget_adaptor_get_object_type (adaptor), type_action))
        {
          if (g_strcmp0 (glade_signal_def_get_object_type_name (sdef), "GtkAction") != 0 ||
              g_strcmp0 (glade_signal_def_get_name (sdef), "activate") != 0)
            continue;
        }
      /* Only action signals have accelerators. */
      else if ((glade_signal_def_get_flags (sdef) & G_SIGNAL_ACTION) == 0)
        continue;

      if ((found = g_list_find_custom (eprop_accel->parent_iters,
                                       glade_signal_def_get_object_type_name (sdef),
                                       (GCompareFunc) eprop_find_iter)) != NULL)
        {
          parent_tab = found->data;
          name = g_strdup_printf ("    %s", glade_signal_def_get_name (sdef));

          /* Populate from accelerator list
           */
          for (l = accelerators; l; l = l->next)
            {
              info = l->data;

              if (g_strcmp0 (info->signal, glade_signal_def_get_name (sdef)))
                continue;

              accel_text = gtk_accelerator_name (info->key, info->modifiers);

              gtk_tree_store_append (model, &iter, parent_tab->iter);
              gtk_tree_store_set
                  (model, &iter,
                   ACCEL_COLUMN_SIGNAL, name,
                   ACCEL_COLUMN_REAL_SIGNAL, glade_signal_def_get_name (sdef),
                   ACCEL_COLUMN_TEXT, accel_text,
                   ACCEL_COLUMN_WEIGHT, PANGO_WEIGHT_NORMAL,
                   ACCEL_COLUMN_STYLE, PANGO_STYLE_NORMAL,
                   ACCEL_COLUMN_FOREGROUND, "Black",
                   ACCEL_COLUMN_VISIBLE, TRUE,
                   ACCEL_COLUMN_KEYCODE, info->key,
                   ACCEL_COLUMN_MODIFIERS, info->modifiers,
                   ACCEL_COLUMN_KEY_ENTERED, TRUE, -1);

              g_free (accel_text);
            }

          /* Special case for GtkAction accelerators  */
          if ((glade_widget_adaptor_get_object_type (adaptor) == type_action ||
               g_type_is_a (glade_widget_adaptor_get_object_type (adaptor), type_action)) &&
              g_list_length (accelerators) > 0)
            continue;

          /* Append a new empty slot at the end */
          gtk_tree_store_append (model, &iter, parent_tab->iter);
          gtk_tree_store_set
              (model, &iter,
               ACCEL_COLUMN_SIGNAL, name,
               ACCEL_COLUMN_REAL_SIGNAL, glade_signal_def_get_name (sdef),
               ACCEL_COLUMN_TEXT, _("<choose a key>"),
               ACCEL_COLUMN_WEIGHT, PANGO_WEIGHT_NORMAL,
               ACCEL_COLUMN_STYLE, PANGO_STYLE_ITALIC,
               ACCEL_COLUMN_FOREGROUND, "Grey",
               ACCEL_COLUMN_VISIBLE, TRUE,
               ACCEL_COLUMN_KEYCODE, 0,
               ACCEL_COLUMN_MODIFIERS, 0, ACCEL_COLUMN_KEY_ENTERED, FALSE, -1);

          g_free (name);
        }
    }
}

void
accel_edited (GtkCellRendererAccel * accel,
              gchar * path_string,
              guint accel_key,
              GdkModifierType accel_mods,
              guint hardware_keycode, GladeEPropAccel * eprop_accel)
{
  gboolean key_was_set;
  GtkTreeIter iter, parent_iter, new_iter;
  gchar *accel_text;
  GladePropertyDef *pdef;
  GladeWidgetAdaptor *adaptor;
  gboolean is_action;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  GType type_action = GTK_TYPE_ACTION;
G_GNUC_END_IGNORE_DEPRECATIONS

  pdef = glade_editor_property_get_property_def (GLADE_EDITOR_PROPERTY (eprop_accel));
  adaptor = glade_property_def_get_adaptor (pdef);

  if (!gtk_tree_model_get_iter_from_string (eprop_accel->model,
                                            &iter, path_string))
    return;

  is_action = (glade_widget_adaptor_get_object_type (adaptor) == type_action ||
               g_type_is_a (glade_widget_adaptor_get_object_type (adaptor), type_action));

  gtk_tree_model_get (eprop_accel->model, &iter,
                      ACCEL_COLUMN_KEY_ENTERED, &key_was_set, -1);

  accel_text = gtk_accelerator_name (accel_key, accel_mods);

  gtk_tree_store_set
      (GTK_TREE_STORE (eprop_accel->model), &iter,
       ACCEL_COLUMN_KEY_ENTERED, TRUE,
       ACCEL_COLUMN_STYLE, PANGO_STYLE_NORMAL,
       ACCEL_COLUMN_FOREGROUND, "Black",
       ACCEL_COLUMN_TEXT, accel_text,
       ACCEL_COLUMN_KEYCODE, accel_key, ACCEL_COLUMN_MODIFIERS, accel_mods, -1);

  g_free (accel_text);

  /* Append a new one if needed
   */
  if (is_action == FALSE && key_was_set == FALSE &&
      gtk_tree_model_iter_parent (eprop_accel->model, &parent_iter, &iter))
    {
      gchar *signal, *real_signal;

      gtk_tree_model_get (eprop_accel->model, &iter,
                          ACCEL_COLUMN_SIGNAL, &signal,
                          ACCEL_COLUMN_REAL_SIGNAL, &real_signal, -1);

      /* Append a new empty slot at the end */
      gtk_tree_store_insert_after (GTK_TREE_STORE (eprop_accel->model),
                                   &new_iter, &parent_iter, &iter);
      gtk_tree_store_set (GTK_TREE_STORE (eprop_accel->model), &new_iter,
                          ACCEL_COLUMN_SIGNAL, signal,
                          ACCEL_COLUMN_REAL_SIGNAL, real_signal,
                          ACCEL_COLUMN_TEXT, _("<choose a key>"),
                          ACCEL_COLUMN_WEIGHT, PANGO_WEIGHT_NORMAL,
                          ACCEL_COLUMN_STYLE, PANGO_STYLE_ITALIC,
                          ACCEL_COLUMN_FOREGROUND, "Grey",
                          ACCEL_COLUMN_VISIBLE, TRUE,
                          ACCEL_COLUMN_KEYCODE, 0,
                          ACCEL_COLUMN_MODIFIERS, 0,
                          ACCEL_COLUMN_KEY_ENTERED, FALSE, -1);
      g_free (signal);
      g_free (real_signal);
    }
}

void
accel_cleared (GtkCellRendererAccel * accel,
               gchar * path_string, GladeEPropAccel * eprop_accel)
{
  GtkTreeIter iter;

  if (!gtk_tree_model_get_iter_from_string (eprop_accel->model,
                                            &iter, path_string))
    return;

  gtk_tree_store_remove (GTK_TREE_STORE (eprop_accel->model), &iter);
}


static GtkWidget *
glade_eprop_accel_view (GladeEditorProperty * eprop)
{
  GladeEPropAccel *eprop_accel = GLADE_EPROP_ACCEL (eprop);
  GtkWidget *view_widget;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  eprop_accel->model = (GtkTreeModel *) 
    gtk_tree_store_new (ACCEL_NUM_COLUMNS, G_TYPE_STRING,   /* The GSignal name formatted for display */
                        G_TYPE_STRING,      /* The GSignal name */
                        G_TYPE_STRING,      /* The text to show in the accelerator cell */
                        G_TYPE_INT, /* PangoWeight attribute for bold headers */
                        G_TYPE_INT, /* PangoStyle attribute for italic grey unset items */
                        G_TYPE_STRING,      /* Foreground colour for italic grey unset items */
                        G_TYPE_BOOLEAN,     /* Visible attribute to hide items for header entries */
                        G_TYPE_BOOLEAN,     /* Whether the key has been entered for this row */
                        G_TYPE_UINT,        /* Hardware keycode */
                        G_TYPE_INT);        /* GdkModifierType */
  
  view_widget = gtk_tree_view_new_with_model (eprop_accel->model);
  gtk_tree_view_set_show_expanders (GTK_TREE_VIEW (view_widget), FALSE);
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (view_widget), FALSE);

  /********************* signal name column *********************/
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), "editable", FALSE, NULL);

  column = gtk_tree_view_column_new_with_attributes
      (_("Signal"), renderer,
       "text", ACCEL_COLUMN_SIGNAL, "weight", ACCEL_COLUMN_WEIGHT, NULL);


  gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

  /********************* accel editor column *********************/
  renderer = gtk_cell_renderer_accel_new ();
  g_object_set (G_OBJECT (renderer), "editable", TRUE, NULL);

  g_signal_connect (renderer, "accel-edited", G_CALLBACK (accel_edited), eprop);
  g_signal_connect (renderer, "accel-cleared",
                    G_CALLBACK (accel_cleared), eprop);

  column = gtk_tree_view_column_new_with_attributes
      (_("Accelerator Key"), renderer,
       "text", ACCEL_COLUMN_TEXT,
       "foreground", ACCEL_COLUMN_FOREGROUND,
       "style", ACCEL_COLUMN_STYLE, "visible", ACCEL_COLUMN_VISIBLE, NULL);

  gtk_tree_view_column_set_expand (GTK_TREE_VIEW_COLUMN (column), TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (view_widget), column);

  return view_widget;
}

static gboolean
glade_eprop_accel_accum_accelerators (GtkTreeModel * model,
                                      GtkTreePath * path,
                                      GtkTreeIter * iter, GList ** ret)
{
  GladeAccelInfo *info;
  gchar *signal;
  GdkModifierType accel_mods;
  guint accel_key;
  gboolean entered = FALSE;

  gtk_tree_model_get (model, iter, ACCEL_COLUMN_KEY_ENTERED, &entered, -1);
  if (!entered)
    return FALSE;

  gtk_tree_model_get (model, iter,
                      ACCEL_COLUMN_REAL_SIGNAL, &signal,
                      ACCEL_COLUMN_KEYCODE, &accel_key,
                      ACCEL_COLUMN_MODIFIERS, &accel_mods, -1);

  info = g_new0 (GladeAccelInfo, 1);
  info->signal = signal;
  info->key = accel_key;
  info->modifiers = accel_mods;

  *ret = g_list_prepend (*ret, info);

  return FALSE;
}


static void
glade_eprop_accel_show_dialog (GladeEditorProperty *eprop)
{
  GladeEPropAccel *eprop_accel = GLADE_EPROP_ACCEL (eprop);
  GtkWidget *dialog, *parent, *vbox, *sw, *tree_view;
  GValue value = { 0, };
  GList *accelerators = NULL;
  gint res;

  parent = gtk_widget_get_toplevel (GTK_WIDGET (eprop));

  dialog = gtk_dialog_new_with_buttons (_("Choose accelerator keys..."),
                                        GTK_WINDOW (parent),
                                        GTK_DIALOG_MODAL |
                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                        _("C_lear"), GLADE_RESPONSE_CLEAR,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_OK"), GTK_RESPONSE_OK, NULL);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_widget_show (vbox);

  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);

  gtk_box_pack_start (GTK_BOX
                      (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), vbox,
                      TRUE, TRUE, 0);

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

      accelerators = g_list_reverse (accelerators);
      g_value_init (&value, GLADE_TYPE_ACCEL_GLIST);
      g_value_take_boxed (&value, accelerators);

      glade_editor_property_commit (eprop, &value);
      g_value_unset (&value);
    }
  else if (res == GLADE_RESPONSE_CLEAR)
    {
      g_value_init (&value, GLADE_TYPE_ACCEL_GLIST);
      g_value_set_boxed (&value, NULL);

      glade_editor_property_commit (eprop, &value);

      g_value_unset (&value);
    }

  /* Clean up ...
   */
  gtk_widget_destroy (dialog);

  g_object_unref (G_OBJECT (eprop_accel->model));
  eprop_accel->model = NULL;

  if (eprop_accel->parent_iters)
    {
      g_list_foreach (eprop_accel->parent_iters, (GFunc) iter_tab_free, NULL);
      g_list_free (eprop_accel->parent_iters);
      eprop_accel->parent_iters = NULL;
    }

}

static GtkWidget *
glade_eprop_accel_create_input (GladeEditorProperty * eprop)
{
  GladeEPropAccel *eprop_accel = GLADE_EPROP_ACCEL (eprop);

  eprop_accel->entry = gtk_entry_new ();
  gtk_widget_set_valign (eprop_accel->entry, GTK_ALIGN_CENTER);
  gtk_editable_set_editable (GTK_EDITABLE (eprop_accel->entry), FALSE);
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (eprop_accel->entry),
                                     GTK_ENTRY_ICON_SECONDARY,
                                     "gtk-edit");
  g_signal_connect_swapped (eprop_accel->entry, "icon-release",
                            G_CALLBACK (glade_eprop_accel_show_dialog), eprop);

  return eprop_accel->entry;
}


static GdkModifierType
glade_gtk_parse_modifiers (const gchar * string)
{
  const gchar *pos = string;
  GdkModifierType modifiers = 0;

  while (pos && pos[0])
    {
      if (!strncmp (pos, "GDK_", 4))
        {
          pos += 4;
          if (!strncmp (pos, "SHIFT_MASK", 10))
            {
              modifiers |= GDK_SHIFT_MASK;
              pos += 10;
            }
          else if (!strncmp (pos, "SUPER_MASK", 10))
            {
              modifiers |= GDK_SUPER_MASK;
              pos += 10;
            }            
          else if (!strncmp (pos, "LOCK_MASK", 9))
            {
              modifiers |= GDK_LOCK_MASK;
              pos += 9;
            }
          else if (!strncmp (pos, "CONTROL_MASK", 12))
            {
              modifiers |= GDK_CONTROL_MASK;
              pos += 12;
            }
          else if (!strncmp (pos, "MOD", 3) && !strncmp (pos + 4, "_MASK", 5))
            {
              switch (pos[3])
                {
                  case '1':
                    modifiers |= GDK_MOD1_MASK;
                    break;
                  case '2':
                    modifiers |= GDK_MOD2_MASK;
                    break;
                  case '3':
                    modifiers |= GDK_MOD3_MASK;
                    break;
                  case '4':
                    modifiers |= GDK_MOD4_MASK;
                    break;
                  case '5':
                    modifiers |= GDK_MOD5_MASK;
                    break;
                }
              pos += 9;
            }
          else if (!strncmp (pos, "BUTTON", 6) &&
                   !strncmp (pos + 7, "_MASK", 5))
            {
              switch (pos[6])
                {
                  case '1':
                    modifiers |= GDK_BUTTON1_MASK;
                    break;
                  case '2':
                    modifiers |= GDK_BUTTON2_MASK;
                    break;
                  case '3':
                    modifiers |= GDK_BUTTON3_MASK;
                    break;
                  case '4':
                    modifiers |= GDK_BUTTON4_MASK;
                    break;
                  case '5':
                    modifiers |= GDK_BUTTON5_MASK;
                    break;
                }
              pos += 12;
            }
          else if (!strncmp (pos, "RELEASE_MASK", 12))
            {
              modifiers |= GDK_RELEASE_MASK;
              pos += 12;
            }
          else
            pos++;
        }
      else
        pos++;
    }
  return modifiers;
}


static gchar *
glade_gtk_modifier_string_from_bits (GdkModifierType modifiers)
{
  GString *string = g_string_new ("");

  if (modifiers & GDK_SHIFT_MASK)
    {
      if (string->len > 0)
        g_string_append (string, " | ");
      g_string_append (string, "GDK_SHIFT_MASK");
    }

  if (modifiers & GDK_SUPER_MASK)
    {
      if (string->len > 0)
        g_string_append (string, " | ");
      g_string_append (string, "GDK_SUPER_MASK");
    }

  if (modifiers & GDK_LOCK_MASK)
    {
      if (string->len > 0)
        g_string_append (string, " | ");
      g_string_append (string, "GDK_LOCK_MASK");
    }

  if (modifiers & GDK_CONTROL_MASK)
    {
      if (string->len > 0)
        g_string_append (string, " | ");
      g_string_append (string, "GDK_CONTROL_MASK");
    }

  if (modifiers & GDK_MOD1_MASK)
    {
      if (string->len > 0)
        g_string_append (string, " | ");
      g_string_append (string, "GDK_MOD1_MASK");
    }

  if (modifiers & GDK_MOD2_MASK)
    {
      if (string->len > 0)
        g_string_append (string, " | ");
      g_string_append (string, "GDK_MOD2_MASK");
    }

  if (modifiers & GDK_MOD3_MASK)
    {
      if (string->len > 0)
        g_string_append (string, " | ");
      g_string_append (string, "GDK_MOD3_MASK");
    }

  if (modifiers & GDK_MOD4_MASK)
    {
      if (string->len > 0)
        g_string_append (string, " | ");
      g_string_append (string, "GDK_MOD4_MASK");
    }

  if (modifiers & GDK_MOD5_MASK)
    {
      if (string->len > 0)
        g_string_append (string, " | ");
      g_string_append (string, "GDK_MOD5_MASK");
    }

  if (modifiers & GDK_BUTTON1_MASK)
    {
      if (string->len > 0)
        g_string_append (string, " | ");
      g_string_append (string, "GDK_BUTTON1_MASK");
    }

  if (modifiers & GDK_BUTTON2_MASK)
    {
      if (string->len > 0)
        g_string_append (string, " | ");
      g_string_append (string, "GDK_BUTTON2_MASK");
    }

  if (modifiers & GDK_BUTTON3_MASK)
    {
      if (string->len > 0)
        g_string_append (string, " | ");
      g_string_append (string, "GDK_BUTTON3_MASK");
    }

  if (modifiers & GDK_BUTTON4_MASK)
    {
      if (string->len > 0)
        g_string_append (string, " | ");
      g_string_append (string, "GDK_BUTTON4_MASK");
    }

  if (modifiers & GDK_BUTTON5_MASK)
    {
      if (string->len > 0)
        g_string_append (string, " | ");
      g_string_append (string, "GDK_BUTTON5_MASK");
    }

  if (modifiers & GDK_RELEASE_MASK)
    {
      if (string->len > 0)
        g_string_append (string, " | ");
      g_string_append (string, "GDK_RELEASE_MASK");
    }

  if (string->len > 0)
    return g_string_free (string, FALSE);

  g_string_free (string, TRUE);
  return NULL;
}

GladeAccelInfo *
glade_accel_read (GladeXmlNode * node, gboolean require_signal)
{
  GladeAccelInfo *ainfo;
  gchar *key, *modifiers, *signal;

  g_return_val_if_fail (node != NULL, NULL);

  if (!glade_xml_node_verify (node, GLADE_TAG_ACCEL))
    return NULL;

  /* Get from xml... */
  key = glade_xml_get_property_string_required
      (node, GLADE_TAG_ACCEL_KEY, NULL);
  if (require_signal)
    signal =
        glade_xml_get_property_string_required (node, GLADE_TAG_ACCEL_SIGNAL,
                                                NULL);
  else
    signal = glade_xml_get_property_string (node, GLADE_TAG_ACCEL_SIGNAL);

  modifiers = glade_xml_get_property_string (node, GLADE_TAG_ACCEL_MODIFIERS);

  /* translate to GladeAccelInfo... */
  ainfo = g_new0 (GladeAccelInfo, 1);
  ainfo->key = gdk_keyval_from_name (key);
  ainfo->signal = signal;       /* take string ownership... */
  ainfo->modifiers = glade_gtk_parse_modifiers (modifiers);

  g_free (modifiers);

  return ainfo;
}

GladeXmlNode *
glade_accel_write (GladeAccelInfo * accel,
                   GladeXmlContext * context, gboolean write_signal)
{
  GladeXmlNode *accel_node;
  gchar *modifiers;

  g_return_val_if_fail (accel != NULL, NULL);
  g_return_val_if_fail (context != NULL, NULL);

  accel_node = glade_xml_node_new (context, GLADE_TAG_ACCEL);
  modifiers = glade_gtk_modifier_string_from_bits (accel->modifiers);

  glade_xml_node_set_property_string (accel_node, GLADE_TAG_ACCEL_KEY,
                                      gdk_keyval_name (accel->key));

  if (write_signal)
    glade_xml_node_set_property_string (accel_node, GLADE_TAG_ACCEL_SIGNAL,
                                        accel->signal);
  glade_xml_node_set_property_string (accel_node, GLADE_TAG_ACCEL_MODIFIERS,
                                      modifiers);

  g_free (modifiers);

  return accel_node;
}


void
glade_gtk_read_accels (GladeWidget * widget,
                       GladeXmlNode * node, gboolean require_signal)
{
  GladeProperty *property;
  GladeXmlNode *prop;
  GladeAccelInfo *ainfo;
  GValue *value = NULL;
  GList *accels = NULL;

  for (prop = glade_xml_node_get_children (node);
       prop; prop = glade_xml_node_next (prop))
    {
      if (!glade_xml_node_verify_silent (prop, GLADE_TAG_ACCEL))
        continue;

      if ((ainfo = glade_accel_read (prop, require_signal)) != NULL)
        accels = g_list_prepend (accels, ainfo);
    }

  if (accels)
    {
      value = g_new0 (GValue, 1);
      g_value_init (value, GLADE_TYPE_ACCEL_GLIST);
      g_value_take_boxed (value, accels);

      property = glade_widget_get_property (widget, "accelerator");
      glade_property_set_value (property, value);

      g_value_unset (value);
      g_free (value);
    }
}

void
glade_gtk_write_accels (GladeWidget * widget,
                        GladeXmlContext * context,
                        GladeXmlNode * node, gboolean write_signal)
{
  GladeXmlNode *accel_node;
  GladeProperty *property;
  GList *list;

  /* Some child widgets may have disabled the property */
  if (!(property = glade_widget_get_property (widget, "accelerator")))
    return;

  for (list = g_value_get_boxed (glade_property_inline_value (property)); list; list = list->next)
    {
      GladeAccelInfo *accel = list->data;

      accel_node = glade_accel_write (accel, context, write_signal);
      glade_xml_node_append_child (node, accel_node);
    }
}
