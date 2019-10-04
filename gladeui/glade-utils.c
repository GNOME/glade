/*
 * Copyright (C) 2001 Ximian, Inc.
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
 *   Chema Celorio <chema@celorio.com>
 */

#include <config.h>

/**
 * SECTION:glade-utils
 * @Title: Glade Utils
 * @Short_Description: Welcome to the zoo.
 *
 * This is where all of that really usefull miscalanious stuff lands up.
 */

#include "glade.h"
#include "glade-project.h"
#include "glade-command.h"
#include "glade-debug.h"
#include "glade-placeholder.h"
#include "glade-widget.h"
#include "glade-widget-adaptor.h"
#include "glade-property.h"
#include "glade-property-def.h"
#include "glade-clipboard.h"
#include "glade-private.h"

#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <gmodule.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <errno.h>

#ifdef G_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

#define GLADE_UTIL_COPY_BUFFSIZE       1024


/**
 * glade_util_compose_get_type_func:
 * @name: the name of the #GType - like 'GtkWidget' or a "get-type" function.
 *
 * TODO: write me
 *
 * Returns: the type function getter
 */
static gchar *
glade_util_compose_get_type_func (const gchar *name)
{
  gchar *retval;
  GString *tmp;
  gint i = 1, j;

  tmp = g_string_new (name);

  while (tmp->str[i])
    {
      if (g_ascii_isupper (tmp->str[i]))
        {
          tmp = g_string_insert_c (tmp, i++, '_');

          j = 0;
          while (g_ascii_isupper (tmp->str[i++]))
            j++;

          if (j > 2)
            g_string_insert_c (tmp, i - 2, '_');

          continue;
        }
      i++;
    }

  tmp = g_string_append (tmp, "_get_type");
  retval = g_ascii_strdown (tmp->str, tmp->len);
  g_string_free (tmp, TRUE);

  return retval;
}

/**
 * glade_util_get_type_from_name:
 * @name: the name of the #GType - like 'GtkWidget' or a "get-type" function.
 * @have_func: function-name flag -- true if the name is a "get-type" function.
 *
 * Returns the type using the "get type" function name based on @name.  
 * If the @have_func flag is true,@name is used directly, otherwise the get-type 
 * function is contrived from @name then used.
 *
 * Returns: the new #GType
 */
GType
glade_util_get_type_from_name (const gchar *name, gboolean have_func)
{
  static GModule *allsymbols = NULL;
  GType (*get_type) (void);
  GType type = 0;
  gchar *func_name = (gchar *) name;

  if ((type = g_type_from_name (name)) == 0 &&
      (have_func ||
       (func_name = glade_util_compose_get_type_func (name)) != NULL))
    {

      if (!allsymbols)
        allsymbols = g_module_open (NULL, 0);

      if (g_module_symbol (allsymbols, func_name, (gpointer) & get_type))
        {
          g_assert (get_type);
          type = get_type ();
        }
      else
        {
          g_warning (_("We could not find the symbol \"%s\""), func_name);
        }

      if (!have_func)
        g_free (func_name);
    }

  if (type == 0)
    g_warning (_("Could not get the type from \"%s\""), name);

  return type;
}

/**
 * glade_utils_get_pspec_from_funcname:
 * @funcname: the symbol name of a function to generate a #GParamSpec
 *
 * Returns: (nullable) (transfer full): A #GParamSpec created by the delegate function
 *          specified by @funcname
 */
GParamSpec *
glade_utils_get_pspec_from_funcname (const gchar *funcname)
{
  static GModule *allsymbols = NULL;
  GParamSpec *pspec = NULL;
  GParamSpec *(*get_pspec) (void) = NULL;

  if (!allsymbols)
    allsymbols = g_module_open (NULL, 0);

  if (!g_module_symbol (allsymbols, funcname, (gpointer) & get_pspec))
    {
      g_warning (_("We could not find the symbol \"%s\""), funcname);
      return NULL;
    }

  g_assert (get_pspec);
  pspec = get_pspec ();

  return pspec;
}

void
_glade_util_dialog_set_hig (GtkDialog *dialog)
{
  GtkWidget *vbox, *action_area;

  /* HIG spacings */
  vbox = gtk_dialog_get_content_area (dialog);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
  gtk_box_set_spacing (GTK_BOX (vbox), 2); /* 2 * 5 + 2 = 12 */

  action_area = gtk_dialog_get_action_area (dialog);
  gtk_container_set_border_width (GTK_CONTAINER (action_area), 5);
  gtk_box_set_spacing (GTK_BOX (action_area), 6);
}

/**
 * glade_util_ui_message:
 * @parent: a #GtkWindow cast as a #GtkWidget
 * @type:   a #GladeUIMessageType
 * @widget: a #GtkWidget to append to the dialog vbox
 * @format: a printf style format string
 * @...:    args for the format.
 *
 * Creates a new warning dialog window as a child of @parent containing
 * the text of @format, runs it, then destroys it on close. Depending
 * on @type, a cancel button may apear or the icon may change.
 *
 * Returns: True if the @type was GLADE_UI_ARE_YOU_SURE and the user
 *          selected "OK", True if the @type was GLADE_UI_YES_OR_NO and
 *          the user selected "YES"; False otherwise.
 */
gint
glade_util_ui_message (GtkWidget *parent,
                       GladeUIMessageType type,
                       GtkWidget *widget,
                       const gchar *format,
                       ...)
{
  GtkWidget *dialog;
  GtkMessageType message_type = GTK_MESSAGE_INFO;
  GtkButtonsType buttons_type = GTK_BUTTONS_OK;
  va_list args;
  gchar *string;
  gint response;

  va_start (args, format);
  string = g_strdup_vprintf (format, args);
  va_end (args);

  /* Get message_type */
  switch (type)
    {
      case GLADE_UI_INFO:
        message_type = GTK_MESSAGE_INFO;
        break;
      case GLADE_UI_WARN:
      case GLADE_UI_ARE_YOU_SURE:
        message_type = GTK_MESSAGE_WARNING;
        break;
      case GLADE_UI_ERROR:
        message_type = GTK_MESSAGE_ERROR;
        break;
      case GLADE_UI_YES_OR_NO:
        message_type = GTK_MESSAGE_QUESTION;
        break;
        break;
      default:
        g_critical ("Bad arg for glade_util_ui_message");
        break;
    }


  /* Get buttons_type */
  switch (type)
    {
      case GLADE_UI_INFO:
      case GLADE_UI_WARN:
      case GLADE_UI_ERROR:
        buttons_type = GTK_BUTTONS_OK;
        break;
      case GLADE_UI_ARE_YOU_SURE:
        buttons_type = GTK_BUTTONS_OK_CANCEL;
        break;
      case GLADE_UI_YES_OR_NO:
        buttons_type = GTK_BUTTONS_YES_NO;
        break;
        break;
      default:
        g_critical ("Bad arg for glade_util_ui_message");
        break;
    }

  dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   message_type, buttons_type, NULL);
  
  gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), string);

  if (widget)
    {
      gtk_box_pack_end (GTK_BOX
                        (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                        widget, TRUE, TRUE, 2);
      gtk_widget_show (widget);

      /* If theres additional content, make it resizable */
      gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
    }
  
  response = gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);
  g_free (string);

  return (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_YES);
}


gboolean
glade_util_check_and_warn_scrollable (GladeWidget *parent,
                                      GladeWidgetAdaptor *child_adaptor,
                                      GtkWidget *parent_widget)
{
  if (GTK_IS_SCROLLED_WINDOW (glade_widget_get_object (parent)) &&
      GWA_SCROLLABLE_WIDGET (child_adaptor) == FALSE)
    {
      GladeWidgetAdaptor *vadaptor =
          glade_widget_adaptor_get_by_type (GTK_TYPE_VIEWPORT);
      GladeWidgetAdaptor *parent_adaptor = glade_widget_get_adaptor (parent);

      glade_util_ui_message (parent_widget,
                             GLADE_UI_INFO, NULL,
                             _("Cannot add non scrollable %s widget to a %s directly.\n"
                               "Add a %s first."), 
                             glade_widget_adaptor_get_title (child_adaptor),
                             glade_widget_adaptor_get_title (parent_adaptor), 
                             glade_widget_adaptor_get_title (vadaptor));
      return TRUE;
    }
  return FALSE;
}

typedef struct
{
  GtkStatusbar *statusbar;
  guint context_id;
  guint message_id;
} FlashInfo;

static const guint flash_length = 3;

static gboolean
remove_message_timeout (FlashInfo *fi)
{
  gtk_statusbar_remove (fi->statusbar, fi->context_id, fi->message_id);
  g_slice_free (FlashInfo, fi);

  /* remove the timeout */
  return FALSE;
}

/**
 * glade_utils_flash_message:
 * @statusbar: The statusbar
 * @context_id: The message context_id
 * @format: The message to flash on the statusbar
 *
 * Flash a temporary message on the statusbar.
 */
void
glade_util_flash_message (GtkWidget *statusbar,
                          guint context_id,
                          gchar *format,
                          ...)
{
  va_list args;
  FlashInfo *fi;
  gchar *message;

  g_return_if_fail (GTK_IS_STATUSBAR (statusbar));
  g_return_if_fail (format != NULL);

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  fi = g_slice_new0 (FlashInfo);
  fi->statusbar = GTK_STATUSBAR (statusbar);
  fi->context_id = context_id;
  fi->message_id = gtk_statusbar_push (fi->statusbar, fi->context_id, message);

  g_timeout_add_seconds (flash_length, (GSourceFunc) remove_message_timeout,
                         fi);

  g_free (message);
}

static gint
glade_util_compare_uline_labels (const gchar *labela, const gchar *labelb)
{
  for (;;)
    {
      gunichar c1, c2;

      if (*labela == '\0')
        return (*labelb == '\0') ? 0 : -1;
      if (*labelb == '\0')
        return 1;

      c1 = g_utf8_get_char (labela);
      if (c1 == '_')
        {
          labela = g_utf8_next_char (labela);
          c1 = g_utf8_get_char (labela);
        }

      c2 = g_utf8_get_char (labelb);
      if (c2 == '_')
        {
          labelb = g_utf8_next_char (labelb);
          c2 = g_utf8_get_char (labelb);
        }

      if (c1 < c2)
        return -1;
      if (c1 > c2)
        return 1;

      labela = g_utf8_next_char (labela);
      labelb = g_utf8_next_char (labelb);
    }

  /* Shouldn't be reached. */
  return 0;
}

/**
 * glade_util_compare_stock_labels:
 * @a: a #gconstpointer to a #GtkStockItem
 * @b: a #gconstpointer to a #GtkStockItem
 *
 * This is a #GCompareFunc that compares the labels of two stock items, 
 * ignoring any '_' characters. It isn't particularly efficient.
 *
 * Returns: negative value if @a < @b; zero if @a = @b; 
 *          positive value if @a > @b
 */
gint
glade_util_compare_stock_labels (gconstpointer a, gconstpointer b)
{
  const gchar *stock_ida = a, *stock_idb = b;
  GtkStockItem itema, itemb;
  gboolean founda, foundb;
  gint retval;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  founda = gtk_stock_lookup (stock_ida, &itema);
  foundb = gtk_stock_lookup (stock_idb, &itemb);
G_GNUC_END_IGNORE_DEPRECATIONS

  if (founda)
    {
      if (!foundb)
        retval = -1;
      else
        /* FIXME: Not ideal for UTF-8. */
        retval = glade_util_compare_uline_labels (itema.label, itemb.label);
    }
  else
    {
      if (!foundb)
        retval = 0;
      else
        retval = 1;
    }

  return retval;
}

/**
 * glade_util_file_dialog_new:
 * @title: dialog title
 * @project: a #GladeProject used when saving
 * @parent: a parent #GtkWindow for the dialog
 * @action: a #GladeUtilFileDialogType to say if the dialog will open or save
 *
 * Returns: (transfer full): a "glade file" file chooser dialog. The caller is
 *          responsible for showing the dialog
 */
GtkWidget *
glade_util_file_dialog_new (const gchar *title,
                            GladeProject *project,
                            GtkWindow *parent,
                            GladeUtilFileDialogType action)
{
  GtkWidget *file_dialog;
  GtkFileFilter *file_filter;

  g_return_val_if_fail ((action == GLADE_FILE_DIALOG_ACTION_OPEN ||
                         action == GLADE_FILE_DIALOG_ACTION_SAVE), NULL);

  g_return_val_if_fail ((action != GLADE_FILE_DIALOG_ACTION_SAVE ||
                         GLADE_IS_PROJECT (project)), NULL);

  file_dialog = gtk_file_chooser_dialog_new (title, parent, action,
                                             _("_Cancel"), GTK_RESPONSE_CANCEL,
                                             action ==
                                             GLADE_FILE_DIALOG_ACTION_OPEN ?
                                             _("_Open") : _("_Save"),
                                             GTK_RESPONSE_OK, NULL);

  file_filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (file_filter, "*");
  gtk_file_filter_set_name (file_filter, _("All Files"));
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_dialog), file_filter);

  file_filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (file_filter, "*.glade");
  gtk_file_filter_set_name (file_filter, _("Libglade Files"));
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_dialog), file_filter);

  file_filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (file_filter, "*.ui");
  gtk_file_filter_set_name (file_filter, _("GtkBuilder Files"));
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_dialog), file_filter);

  file_filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (file_filter, "*.ui");
  gtk_file_filter_add_pattern (file_filter, "*.glade");
  gtk_file_filter_set_name (file_filter, _("All Glade Files"));
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_dialog), file_filter);

  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (file_dialog), file_filter);

  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER
                                                  (file_dialog), TRUE);
  gtk_dialog_set_default_response (GTK_DIALOG (file_dialog), GTK_RESPONSE_OK);

  return file_dialog;
}

/**
 * glade_util_replace:
 * @str: a string
 * @a: a #gchar
 * @b: a #gchar
 *
 * Replaces each occurance of the character @a in @str to @b.
 */
void
glade_util_replace (gchar *str, gchar a, gchar b)
{
  g_return_if_fail (str != NULL);

  while (*str != 0)
    {
      if (*str == a)
        *str = b;

      str = g_utf8_next_char (str);
    }
}

/**
 * _glade_util_strreplace:
 * @str: a string
 * @free_str: wheter to free str or not
 * @key: the key string to search for
 * @replacement: string to replace key
 *
 * Replaces each occurance of the string @key in @str to @replacement.
 */
gchar *
_glade_util_strreplace (gchar *str,
                        gboolean free_str,
                        const gchar *key,
                        const gchar *replacement)
{
  gchar *retval, **array;

  if ((array = g_strsplit (str, key, -1)) && array[0])
    retval = g_strjoinv (replacement, array);
  else
    retval = g_strdup (str);

  g_strfreev (array);

  if (free_str)
    g_free (str);

  return retval;
}

/**
 * glade_util_read_prop_name:
 * @str: a string
 *
 * Return a usable version of a property identifier as found
 * in a freshly parserd #GladeInterface
 */
gchar *
glade_util_read_prop_name (const gchar *str)
{
  gchar *id;

  g_return_val_if_fail (str != NULL, NULL);

  id = g_strdup (str);

  glade_util_replace (id, '_', '-');

  return id;
}


/**
 * glade_util_duplicate_underscores:
 * @name: a string
 *
 * Duplicates @name, but the copy has two underscores in place of any single
 * underscore in the original.
 *
 * Returns: a newly allocated string
 */
gchar *
glade_util_duplicate_underscores (const gchar *name)
{
  const gchar *tmp;
  const gchar *last_tmp = name;
  gchar *underscored_name = g_malloc (strlen (name) * 2 + 1);
  gchar *tmp_underscored = underscored_name;

  for (tmp = last_tmp; *tmp; tmp = g_utf8_next_char (tmp))
    {
      if (*tmp == '_')
        {
          memcpy (tmp_underscored, last_tmp, tmp - last_tmp + 1);
          tmp_underscored += tmp - last_tmp + 1;
          last_tmp = tmp + 1;
          *tmp_underscored++ = '_';
        }
    }

  memcpy (tmp_underscored, last_tmp, tmp - last_tmp + 1);

  return underscored_name;
}

/*
 * taken from gtk... maybe someday we can convince them to
 * expose gtk_container_get_all_children
 */
static void
gtk_container_children_callback (GtkWidget *widget, gpointer client_data)
{
  GList **children;

  children = (GList **) client_data;
  if (!g_list_find (*children, widget))
    *children = g_list_prepend (*children, widget);
}

/**
 * glade_util_container_get_all_children:
 * @container: a #GtkContainer
 *
 * Use this to itterate over all children in a GtkContainer,
 * as it used _forall() instead of _foreach() (and the GTK+ version
 * of this function is simply not exposed).
 *
 * Returns: (element-type GtkWidget) (transfer container): a #GList giving the contents of @container
 */
GList *
glade_util_container_get_all_children (GtkContainer *container)
{
  GList *children = NULL;

  g_return_val_if_fail (GTK_IS_CONTAINER (container), NULL);

  gtk_container_forall (container, gtk_container_children_callback, &children);
  gtk_container_foreach (container, gtk_container_children_callback, &children);

  /* Preserve the natural order by reversing the list */
  return g_list_reverse (children);
}

/**
 * glade_util_count_placeholders:
 * @parent: a #GladeWidget
 *
 * Returns: the amount of #GladePlaceholders parented by @parent
 */
gint
glade_util_count_placeholders (GladeWidget *parent)
{
  gint placeholders = 0;
  GList *list, *children;

  /* count placeholders */
  if ((children = 
       glade_widget_adaptor_get_children (glade_widget_get_adaptor (parent), 
                                          glade_widget_get_object (parent))) != NULL)
    {
      for (list = children; list && list->data; list = list->next)
        {
          if (GLADE_IS_PLACEHOLDER (list->data))
            placeholders++;
        }
      g_list_free (children);
    }

  return placeholders;
}

static GtkTreeIter *
glade_util_find_iter (GtkTreeModel *model,
                      GtkTreeIter *iter,
                      GladeWidget *findme,
                      gint column)
{
  GtkTreeIter *retval = NULL;
  GObject *object = NULL;
  GtkTreeIter *next;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);
  g_return_val_if_fail (iter != NULL, NULL);

  next = gtk_tree_iter_copy (iter);
  g_return_val_if_fail (next != NULL, NULL);

  while (retval == NULL)
    {
      GladeWidget *widget;

      gtk_tree_model_get (model, next, column, &object, -1);
      if (object &&
          gtk_tree_model_get_column_type (model, column) == G_TYPE_OBJECT)
        g_object_unref (object);

      widget = glade_widget_get_from_gobject (object);

      if (widget == findme)
        {
          retval = gtk_tree_iter_copy (next);
          break;
        }
      else if (glade_widget_is_ancestor (findme, widget))
        {
          if (gtk_tree_model_iter_has_child (model, next))
            {
              GtkTreeIter child;
              gtk_tree_model_iter_children (model, &child, next);
              if ((retval = glade_util_find_iter
                   (model, &child, findme, column)) != NULL)
                break;
            }

          /* Only search the branches where the searched widget
           * is actually a child of the this row, optimize the
           * searching this way
           */
          break;
        }

      if (!gtk_tree_model_iter_next (model, next))
        break;
    }
  gtk_tree_iter_free (next);

  return retval;
}

/**
 * glade_util_find_iter_by_widget:
 * @model: a #GtkTreeModel
 * @findme: a #GladeWidget
 * @column: a #gint
 *
 * Looks through @model for the #GtkTreeIter corresponding to 
 * @findme under @column.
 *
 * Returns: a newly allocated #GtkTreeIter from @model corresponding
 * to @findme which should be freed with gtk_tree_iter_free()
 * 
 */
GtkTreeIter *
glade_util_find_iter_by_widget (GtkTreeModel *model,
                                GladeWidget *findme,
                                gint column)
{
  GtkTreeIter iter;
  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      return glade_util_find_iter (model, &iter, findme, column);
    }
  return NULL;
}

/**
 * glade_util_purify_list: (skip)
 * @list: (transfer full): A #GList
 *
 * Returns: (transfer full): A newly allocated version of @list with no 
 *          duplicate data entries
 */
GList *
glade_util_purify_list (GList * list)
{
  GList *l, *newlist = NULL;

  for (l = list; l; l = l->next)
    if (!g_list_find (newlist, l->data))
      newlist = g_list_prepend (newlist, l->data);

  g_list_free (list);

  return g_list_reverse (newlist);
}

/**
 * glade_util_added_in_list: (skip)
 * @old_list: the old #GList
 * @new_list: the new #GList
 *
 * Returns: (transfer container): A newly allocated #GList of elements that
 *          are in @new but not in @old
 *
 */
GList *
glade_util_added_in_list (GList *old_list, GList *new_list)
{
  GList *added = NULL, *list;

  for (list = new_list; list; list = list->next)
    {
      if (!g_list_find (old_list, list->data))
        added = g_list_prepend (added, list->data);
    }

  return g_list_reverse (added);
}

/**
 * glade_util_removed_from_list: (skip)
 * @old_list: the old #GList
 * @new_list: the new #GList
 *
 * Returns: (transfer container): A newly allocated #GList of elements that
 *          are in @old no longer in @new
 *
 */
GList *
glade_util_removed_from_list (GList *old_list, GList *new_list)
{
  GList *added = NULL, *list;

  for (list = old_list; list; list = list->next)
    {
      if (!g_list_find (new_list, list->data))
        added = g_list_prepend (added, list->data);
    }

  return g_list_reverse (added);
}


/**
 * glade_util_canonical_path:
 * @path: any path that may contain ".." or "." components
 *
 * Returns: an absolute path to the specified file or directory
 *          that contains no ".." or "." components (this does
 *          not call readlink like realpath() does).
 */
gchar *
glade_util_canonical_path (const gchar *path)
{
  GFile *file;
  gchar *direct_name;

  g_return_val_if_fail (path != NULL, NULL);

  file = g_file_new_for_path (path);
  direct_name = g_file_get_path (file);

  g_object_unref (file);
  return direct_name;
}

static GModule *
try_load_library (const gchar *library_path, const gchar *library_name)
{
  gchar *path = g_module_build_path (library_path, library_name);
  GModule *module = NULL;

  if (!library_path || g_file_test (path, G_FILE_TEST_EXISTS))
    {
      if (!(module = g_module_open (path, G_MODULE_BIND_LAZY)))
        g_warning ("Failed to load %s: %s", path, g_module_error ());
    }

#ifdef PLATFORM_OSX
  /* Handle .dylib's on OSX */
  if (!module)
    {
      gchar *osx_path;

      /* Remove possible trailing .so */
      if (g_str_has_suffix (path, ".so"))
        {
          gchar *tmp = g_strndup (path, strlen(path) - 3);
          g_free (path);
          path = tmp;
        }

      osx_path = g_strconcat (path, ".dylib", NULL);
      if (!library_path || g_file_test (osx_path, G_FILE_TEST_EXISTS))
        {
          if (!(module = g_module_open (osx_path, G_MODULE_BIND_LAZY)))
            g_warning ("Failed to load %s: %s", osx_path, g_module_error ());
        }

      g_free (osx_path);
    }
#endif

  g_free (path);

  return module;
}

/**
 * glade_util_load_library:
 * @library_name: name of the library
 *
 * Loads the named library from the Glade modules and lib directory or failing that
 * from the standard platform specific directories. (Including /usr/local/lib for unices)
 *
 * The @library_name should not include any platform specifix prefix or suffix,
 * those are automatically added, if needed, by g_module_build_path()
 *
 * Returns: a #GModule on success, or %NULL on failure.
 */
GModule *
glade_util_load_library (const gchar *library_name)
{
  GModule *module = NULL;
  const gchar *search_path;
  gint i;

  if ((search_path = g_getenv (GLADE_ENV_MODULE_PATH)) != NULL)
    {
      gchar **split;

      if ((split = g_strsplit (search_path, ":", 0)) != NULL)
        {
          for (i = 0; split[i] != NULL; i++)
            if ((module = try_load_library (split[i], library_name)) != NULL)
              break;

          g_strfreev (split);
        }
    }

  if (g_getenv (GLADE_ENV_TESTING) == NULL && !module)
    {
      const gchar *paths[] = { glade_app_get_modules_dir (),
                               glade_app_get_lib_dir (),
#ifndef G_OS_WIN32 /* Try local lib dir on Unices */
                               "/usr/local/lib",
#endif
                               NULL}; /* Use default system paths */

      
      for (i = 0; i < G_N_ELEMENTS (paths); i++)
        if ((module = try_load_library (paths[i], library_name)) != NULL)
          break;
    }

  return module;
}

/**
 * glade_util_file_is_writeable:
 * @path:  the path to the file
 *
 * Checks whether the file at @path is writeable
 *
 * Returns: TRUE if file is writeable
 */
gboolean
glade_util_file_is_writeable (const gchar *path)
{
  GIOChannel *channel;
  g_return_val_if_fail (path != NULL, FALSE);

  /* The only way to really know if the file is writable */
  if ((channel = g_io_channel_new_file (path, "a+", NULL)) != NULL)
    {
      g_io_channel_unref (channel);
      return TRUE;
    }
  return FALSE;
}

/**
 * glade_util_have_devhelp:
 *
 * Returns: whether the devhelp module is loaded
 */
gboolean
glade_util_have_devhelp (void)
{
  static gint have_devhelp = -1;
  gchar *ptr;
  gint cnt, ret, major, minor;
  GError *error = NULL;

#define DEVHELP_OLD_MESSAGE  \
    "The DevHelp installed on your system is too old, " \
    "devhelp feature will be disabled."

#define DEVHELP_MISSING_MESSAGE  \
    "No DevHelp installed on your system, " \
    "devhelp feature will be disabled."

  if (have_devhelp >= 0)
    return have_devhelp;

  have_devhelp = 0;

  if ((ptr = g_find_program_in_path ("devhelp")) != NULL)
    {
      g_free (ptr);

      if (g_spawn_command_line_sync ("devhelp --version",
                                     &ptr, NULL, &ret, &error))
        {
          /* If we have a successfull return code.. parse the output.
           */
          if (ret == 0)
            {
              gchar name[16];
              if ((cnt = sscanf (ptr, "%15s %d.%d\n",
                                 name, &major, &minor)) == 3)
                {
                  /* Devhelp 0.12 required.
                   */
                  if (major >= 2 || (major >= 0 && minor >= 12))
                    have_devhelp = 1;
                  else
                    g_message (DEVHELP_OLD_MESSAGE);
                }
              else

                {
                  if (ptr != NULL || strlen (ptr) > 0)
                    g_warning ("devhelp had unparsable output: "
                               "'%s' (parsed %d elements)", ptr, cnt);
                  else
                    g_message (DEVHELP_OLD_MESSAGE);
                }
            }
          else
            g_warning ("devhelp had bad return code: '%d'", ret);

          g_free (ptr);
        }
      else
        {
          g_warning ("Error trying to launch devhelp: %s", error->message);
          g_error_free (error);
        }
    }
  else
    g_message (DEVHELP_MISSING_MESSAGE);

  return have_devhelp;
}

/**
 * glade_util_get_devhelp_icon:
 * @size: the preferred icon size
 *
 * Creates an image displaying the devhelp icon.
 *
 * Returns: (transfer full): a #GtkImage
 */
GtkWidget *
glade_util_get_devhelp_icon (GtkIconSize size)
{
  GtkIconTheme *icon_theme;
  GdkScreen *screen;
  GtkWidget *image;
  gchar *path;

  image = gtk_image_new ();
  screen = gtk_widget_get_screen (GTK_WIDGET (image));
  icon_theme = gtk_icon_theme_get_for_screen (screen);

  if (gtk_icon_theme_has_icon (icon_theme, GLADE_DEVHELP_ICON_NAME))
    {
      gtk_image_set_from_icon_name (GTK_IMAGE (image), GLADE_DEVHELP_ICON_NAME,
                                    size);
    }
  else
    {
      path =
          g_build_filename (glade_app_get_pixmaps_dir (),
                            GLADE_DEVHELP_FALLBACK_ICON_FILE, NULL);

      gtk_image_set_from_file (GTK_IMAGE (image), path);

      g_free (path);
    }

  return image;
}

/**
 * glade_util_search_devhep:
 * @devhelp: the devhelp widget created by the devhelp module.
 * @book: the devhelp book (or %NULL)
 * @page: the page in the book (or %NULL)
 * @search: the search string (or %NULL)
 *
 * Envokes devhelp with the appropriate search string
 *
 */
void
glade_util_search_devhelp (const gchar *book,
                           const gchar *page,
                           const gchar *search)
{
  GError *error = NULL;
  gchar *book_comm = NULL, *page_comm = NULL, *search_comm = NULL;
  gchar *string;

  g_return_if_fail (glade_util_have_devhelp ());

  if (book)
    book_comm = g_strdup_printf ("book:%s", book);
  if (page)
    page_comm = g_strdup_printf (" page:%s", page);
  if (search)
    search_comm = g_strdup_printf (" %s", search);

  string = g_strdup_printf ("devhelp -s \"%s%s%s\"",
                            book_comm ? book_comm : "",
                            page_comm ? page_comm : "",
                            search_comm ? search_comm : "");

  if (g_spawn_command_line_async (string, &error) == FALSE)
    {
      g_warning ("Error envoking devhelp: %s", error->message);
      g_error_free (error);
    }

  g_free (string);
  if (book_comm)
    g_free (book_comm);
  if (page_comm)
    g_free (page_comm);
  if (search_comm)
    g_free (search_comm);
}

/**
 * glade_util_get_placeholder_from_pointer:
 * @container: a #GtkContainer
 *
 * Returns: (transfer none): a #GtkWidget
 */
GtkWidget *
glade_util_get_placeholder_from_pointer (GtkContainer *container)
{
  GdkDeviceManager *manager;
  GdkDisplay *display;
  GdkDevice *device;
  GdkWindow *window;

  if (((display = gtk_widget_get_display (GTK_WIDGET (container))) || 
       (display = gdk_display_get_default ())) &&
      (manager = gdk_display_get_device_manager (display)) &&
      (device = gdk_device_manager_get_client_pointer (manager)) &&
      (window = gdk_device_get_window_at_position (device, NULL, NULL)))
    {
      gpointer widget;
      gdk_window_get_user_data (window, &widget);

      return GLADE_IS_PLACEHOLDER (widget) ? GTK_WIDGET (widget) : NULL;
    }

  return NULL;
}

/**
 * glade_util_object_is_loading:
 * @object: A #GObject
 *
 * Returns: Whether the object's project is being loaded or not.
 *       
 */
gboolean
glade_util_object_is_loading (GObject *object)
{
  GladeProject *project;
  GladeWidget *widget;

  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);

  widget = glade_widget_get_from_gobject (object);
  g_return_val_if_fail (GLADE_IS_WIDGET (widget), FALSE);

  project = glade_widget_get_project (widget);

  return project && glade_project_is_loading (project);
}

/**
 * glade_util_url_show:
 * @url: An URL to display
 *
 * Portable function for showing an URL @url in a web browser.
 *
 * Returns: TRUE if a web browser was successfully launched, or FALSE
 *
 */
gboolean
glade_util_url_show (const gchar *url)
{
  GtkWidget *widget;
  GError *error = NULL;
  gboolean ret;

  g_return_val_if_fail (url != NULL, FALSE);

  widget = glade_app_get_window ();

  ret = gtk_show_uri (gtk_widget_get_screen (widget),
                      url, gtk_get_current_event_time (), &error);
  if (error != NULL)
    {
      GtkWidget *dialog_widget;

      dialog_widget = gtk_message_dialog_new (GTK_WINDOW (widget),
                                              GTK_DIALOG_DESTROY_WITH_PARENT,
                                              GTK_MESSAGE_ERROR,
                                              GTK_BUTTONS_CLOSE,
                                              "%s", _("Could not show link:"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG
                                                (dialog_widget), "%s",
                                                error->message);
      g_error_free (error);

      g_signal_connect (dialog_widget, "response",
                        G_CALLBACK (gtk_widget_destroy), NULL);

      gtk_window_present (GTK_WINDOW (dialog_widget));
    }

  return ret;
}

/**
 * glade_util_get_file_mtime:
 * @filename: A filename
 * @error: return location for errors
 *
 * Gets the UTC modification time of file @filename.
 *
 * Returns: The mtime of the file, or %0 if the file attributes
 *          could not be read.
 */
time_t
glade_util_get_file_mtime (const gchar *filename, GError **error)
{
  struct stat info;
  gint retval;

  retval = g_stat (filename, &info);

  if (retval != 0)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   "could not stat file '%s': %s", filename,
                   g_strerror (errno));
      return (time_t) 0;
    }
  else
    {
      return info.st_mtime;
    }
}

gchar *
glade_util_filename_to_icon_name (const gchar *value)
{
  gchar *icon_name, *p;
  g_return_val_if_fail (value && value[0], NULL);

  icon_name = g_strdup_printf ("glade-generated-%s", value);

  if ((p = strrchr (icon_name, '.')) != NULL)
    *p = '-';

  return icon_name;
}

gchar *
glade_util_icon_name_to_filename (const gchar *value)
{
  /* sscanf makes us allocate a buffer */
  gchar filename[FILENAME_MAX], *p;
  g_return_val_if_fail (value && value[0], NULL);

  sscanf (value, "glade-generated-%s", filename);

  /* XXX: Filenames without an extention will evidently
   * break here
   */
  if ((p = strrchr (filename, '-')) != NULL)
    *p = '.';

  return g_strdup (filename);
}

gint
glade_utils_enum_value_from_string (GType enum_type, const gchar *strval)
{
  gint value = 0;
  const gchar *displayable;
  GValue *gvalue;

  g_return_val_if_fail (strval && strval[0], 0);

  if (((displayable =
        glade_get_value_from_displayable (enum_type, strval)) != NULL &&
       (gvalue =
        glade_utils_value_from_string (enum_type, displayable, NULL)) != NULL) ||
      (gvalue =
       glade_utils_value_from_string (enum_type, strval, NULL)) != NULL)
    {
      value = g_value_get_enum (gvalue);
      g_value_unset (gvalue);
      g_free (gvalue);
    }
  return value;
}

static gchar *
glade_utils_enum_string_from_value_real (GType enum_type,
                                         gint value,
                                         gboolean displayable)
{
  GValue gvalue = { 0, };
  gchar *string;

  g_value_init (&gvalue, enum_type);
  g_value_set_enum (&gvalue, value);

  string = glade_utils_string_from_value (&gvalue);
  g_value_unset (&gvalue);

  if (displayable && string)
    {
      const gchar *dstring = glade_get_displayable_value (enum_type, string);
      if (dstring)
        {
          g_free (string);
          return g_strdup (dstring);
        }
    }

  return string;
}

gchar *
glade_utils_enum_string_from_value (GType enum_type, gint value)
{
  return glade_utils_enum_string_from_value_real (enum_type, value, FALSE);
}

gchar *
glade_utils_enum_string_from_value_displayable (GType enum_type, gint value)
{
  return glade_utils_enum_string_from_value_real (enum_type, value, TRUE);
}


gint
glade_utils_flags_value_from_string (GType flags_type, const gchar *strval)
{
  gint value = 0;
  const gchar *displayable;
  GValue *gvalue;

  g_return_val_if_fail (strval && strval[0], 0);

  if (((displayable =
        glade_get_value_from_displayable (flags_type, strval)) != NULL &&
       (gvalue =
        glade_utils_value_from_string (flags_type, displayable, NULL)) != NULL) ||
      (gvalue =
       glade_utils_value_from_string (flags_type, strval, NULL)) != NULL)
    {
      value = g_value_get_flags (gvalue);
      g_value_unset (gvalue);
      g_free (gvalue);
    }
  return value;
}

static gchar *
glade_utils_flags_string_from_value_real (GType flags_type,
                                          gint value,
                                          gboolean displayable)
{
  GValue gvalue = { 0, };
  gchar *string;

  g_value_init (&gvalue, flags_type);
  g_value_set_flags (&gvalue, value);

  string = glade_utils_string_from_value (&gvalue);
  g_value_unset (&gvalue);

  if (displayable && string)
    {
      const gchar *dstring = glade_get_displayable_value (flags_type, string);
      if (dstring)
        {
          g_free (string);
          return g_strdup (dstring);
        }
    }

  return string;
}

gchar *
glade_utils_flags_string_from_value (GType flags_type, gint value)
{
  return glade_utils_flags_string_from_value_real (flags_type, value, FALSE);

}


gchar *
glade_utils_flags_string_from_value_displayable (GType flags_type, gint value)
{
  return glade_utils_flags_string_from_value_real (flags_type, value, TRUE);
}


/* A hash table of generically created property definitions for
 * fundamental types, so we can easily use glade's conversion
 * system without using properties (only GTypes)
 */
static GHashTable *generic_property_definitions = NULL;


static gboolean
utils_gtype_equal (gconstpointer v1, gconstpointer v2)
{
  return *((const GType *) v1) == *((const GType *) v2);
}

static guint
utils_gtype_hash (gconstpointer v)
{
  return *(const GType *) v;
}


static GladePropertyDef *
pdef_from_gtype (GType type)
{
  GladePropertyDef *property_def = NULL;
  GParamSpec *pspec = NULL;

  if (!generic_property_definitions)
    generic_property_definitions =
        g_hash_table_new_full (utils_gtype_hash, utils_gtype_equal, g_free,
                               (GDestroyNotify) glade_property_def_free);

  property_def = g_hash_table_lookup (generic_property_definitions, &type);

  if (!property_def)
    {
      /* Support enum and flag types, and a hardcoded list of fundamental types */
      if (type == G_TYPE_CHAR)
        pspec = g_param_spec_char ("dummy", "dummy", "dummy",
                                   G_MININT8, G_MAXINT8, 0,
                                   G_PARAM_READABLE | G_PARAM_WRITABLE);
      else if (type == G_TYPE_UCHAR)
        pspec = g_param_spec_char ("dummy", "dummy", "dummy",
                                   0, G_MAXUINT8, 0,
                                   G_PARAM_READABLE | G_PARAM_WRITABLE);
      else if (type == G_TYPE_BOOLEAN)
        pspec = g_param_spec_boolean ("dummy", "dummy", "dummy",
                                      FALSE,
                                      G_PARAM_READABLE | G_PARAM_WRITABLE);
      else if (type == G_TYPE_INT)
        pspec = g_param_spec_int ("dummy", "dummy", "dummy",
                                  G_MININT, G_MAXINT, 0,
                                  G_PARAM_READABLE | G_PARAM_WRITABLE);
      else if (type == G_TYPE_UINT)
        pspec = g_param_spec_uint ("dummy", "dummy", "dummy",
                                   0, G_MAXUINT, 0,
                                   G_PARAM_READABLE | G_PARAM_WRITABLE);
      else if (type == G_TYPE_LONG)
        pspec = g_param_spec_long ("dummy", "dummy", "dummy",
                                   G_MINLONG, G_MAXLONG, 0,
                                   G_PARAM_READABLE | G_PARAM_WRITABLE);
      else if (type == G_TYPE_ULONG)
        pspec = g_param_spec_ulong ("dummy", "dummy", "dummy",
                                    0, G_MAXULONG, 0,
                                    G_PARAM_READABLE | G_PARAM_WRITABLE);
      else if (type == G_TYPE_INT64)
        pspec = g_param_spec_int64 ("dummy", "dummy", "dummy",
                                    G_MININT64, G_MAXINT64, 0,
                                    G_PARAM_READABLE | G_PARAM_WRITABLE);
      else if (type == G_TYPE_UINT64)
        pspec = g_param_spec_uint64 ("dummy", "dummy", "dummy",
                                     0, G_MAXUINT64, 0,
                                     G_PARAM_READABLE | G_PARAM_WRITABLE);
      else if (type == G_TYPE_FLOAT)
        pspec = g_param_spec_float ("dummy", "dummy", "dummy",
                                    G_MINFLOAT, G_MAXFLOAT, 1.0F,
                                    G_PARAM_READABLE | G_PARAM_WRITABLE);
      else if (type == G_TYPE_DOUBLE)
        pspec = g_param_spec_double ("dummy", "dummy", "dummy",
                                     G_MINDOUBLE, G_MAXDOUBLE, 1.0F,
                                     G_PARAM_READABLE | G_PARAM_WRITABLE);
      else if (type == G_TYPE_STRING)
        pspec = g_param_spec_string ("dummy", "dummy", "dummy",
                                     NULL, G_PARAM_READABLE | G_PARAM_WRITABLE);
      else if (type == G_TYPE_OBJECT || g_type_is_a (type, G_TYPE_OBJECT))
        pspec = g_param_spec_object ("dummy", "dummy", "dummy",
                                     type, G_PARAM_READABLE | G_PARAM_WRITABLE);
      else if (G_TYPE_IS_ENUM (type))
        {
          GEnumClass *eclass = g_type_class_ref (type);
          pspec = g_param_spec_enum ("dummy", "dummy", "dummy",
                                     type, eclass->minimum,
                                     G_PARAM_READABLE | G_PARAM_WRITABLE);
          g_type_class_unref (eclass);
        }
      else if (G_TYPE_IS_FLAGS (type))
        pspec = g_param_spec_flags ("dummy", "dummy", "dummy",
                                    type, 0,
                                    G_PARAM_READABLE | G_PARAM_WRITABLE);

      if (pspec)
        {
          if ((property_def =
               glade_property_def_new_from_spec_full (NULL, pspec,
                                                        FALSE)) != NULL)
            {
              /* XXX If we ever free the hash table, property classes wont touch
               * the allocated pspecs, so they would theoretically be leaked.
               */
              g_hash_table_insert (generic_property_definitions,
                                   g_memdup (&type, sizeof (GType)),
                                   property_def);
            }
          else
            g_warning ("Unable to create property class for type %s",
                       g_type_name (type));
        }
      else
        g_warning ("No generic conversion support for type %s",
                   g_type_name (type));
    }
  return property_def;
}

/**
 * glade_utils_value_from_string:
 * @type: a #GType to convert with
 * @string: the string to convert
 * @project: the #GladeProject to look for formats of object names when needed
 *
 * Allocates and sets a #GValue of type @type
 * set to @string (using glade conversion routines) 
 *
 * Returns: A newly allocated and set #GValue
 */
GValue *
glade_utils_value_from_string (GType type,
                               const gchar *string,
                               GladeProject *project)
{
  GladePropertyDef *pdef;

  g_return_val_if_fail (type != G_TYPE_INVALID, NULL);
  g_return_val_if_fail (string != NULL, NULL);

  if ((pdef = pdef_from_gtype (type)) != NULL)
    return glade_property_def_make_gvalue_from_string (pdef, string, project);

  return NULL;
}

/**
 * glade_utils_boolean_from_string:
 * @string: the string to convert
 * @value: (out) (optional): return location
 *
 * Parse a boolean value
 *
 * Returns: %TRUE if there was an error on the conversion, %FALSE otherwise.
 */
gboolean
glade_utils_boolean_from_string (const gchar *string, gboolean *value)
{
  if (string[0] == '\0')
    {
      if (value)
        *value = FALSE;

      return TRUE;
    }
  else if (string[1] == '\0')
    {
      gchar c = string[0];
      if (c == '1' ||
          c == 'y' || c == 't' ||
          c == 'Y' || c == 'T')
        {
          if (value)
            *value = TRUE;
        }
      else if (c == '0' ||
               c == 'n' || c == 'f' ||
               c == 'N' || c == 'F')
        {
          if (value)
            *value = FALSE;
        }
      else
        {
          if (value)
            *value = FALSE;

          return TRUE;
        }
    }
  else
    {
      if (g_ascii_strcasecmp (string, "true") == 0 ||
          g_ascii_strcasecmp (string, "yes") == 0)
        {
          if (value)
            *value = TRUE;
        }
      else if (g_ascii_strcasecmp (string, "false") == 0 ||
               g_ascii_strcasecmp (string, "no") == 0)
        {
          if (value)
            *value = FALSE;
        }
      else
        {
          if (value)
            *value = FALSE;

          return TRUE;
        }
    }

  return FALSE;
}

/**
 * glade_utils_string_from_value:
 * @value: a #GValue to convert
 *
 * Serializes #GValue into a string 
 * (using glade conversion routines) 
 *
 * Returns: A newly allocated string
 */
gchar *
glade_utils_string_from_value (const GValue *value)
{
  GladePropertyDef *pdef;

  g_return_val_if_fail (value != NULL, NULL);

  if ((pdef = pdef_from_gtype (G_VALUE_TYPE (value))) != NULL)
    return glade_property_def_make_string_from_gvalue (pdef, value);

  return NULL;
}


/**
 * glade_utils_liststore_from_enum_type:
 * @enum_type: A #GType
 * @include_empty: wheather to prepend an "Unset" slot
 *
 * Creates a liststore suitable for comboboxes and such to 
 * chose from a variety of types.
 *
 * Returns: (transfer full): A new #GtkListStore
 */
GtkListStore *
glade_utils_liststore_from_enum_type (GType enum_type, gboolean include_empty)
{
  GtkListStore *store;
  GtkTreeIter iter;
  GEnumClass *eclass;
  guint i;

  eclass = g_type_class_ref (enum_type);

  store = gtk_list_store_new (1, G_TYPE_STRING);

  if (include_empty)
    {
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, _("None"), -1);
    }

  for (i = 0; i < eclass->n_values; i++)
    {
      const gchar *displayable =
          glade_get_displayable_value (enum_type, eclass->values[i].value_nick);

      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          0,
                          displayable ? displayable : eclass->values[i].
                          value_nick, -1);
    }

  g_type_class_unref (eclass);

  return store;
}



/**
 * glade_utils_hijack_key_press:
 * @win: a #GtkWindow
 * @event: the #GdkEventKey
 * @user_data: unused
 *
 * This function is meant to be attached to key-press-event of a toplevel,
 * it simply allows the window contents to treat key events /before/ 
 * accelerator keys come into play (this way widgets dont get deleted
 * when cutting text in an entry etc.).
 * Creates a liststore suitable for comboboxes and such to 
 * chose from a variety of types.
 *
 * Returns: whether the event was handled
 */
gint
glade_utils_hijack_key_press (GtkWindow *win,
                              GdkEventKey *event,
                              gpointer user_data)
{
  GtkWidget *focus_widget;

  focus_widget = gtk_window_get_focus (win);
  if (focus_widget && (event->keyval == GDK_KEY_Delete ||       /* Filter Delete from accelerator keys */
                       ((event->state & GDK_CONTROL_MASK) &&    /* CNTL keys... */
                        ((event->keyval == GDK_KEY_c || event->keyval == GDK_KEY_C) ||  /* CNTL-C (copy)  */
                         (event->keyval == GDK_KEY_x || event->keyval == GDK_KEY_X) ||  /* CNTL-X (cut)   */
                         (event->keyval == GDK_KEY_v || event->keyval == GDK_KEY_V) ||  /* CNTL-V (paste) */
                         (event->keyval == GDK_KEY_n || event->keyval == GDK_KEY_N))))) /* CNTL-N (new project) */
    {
      return gtk_widget_event (focus_widget, (GdkEvent *) event);
    }
  return FALSE;
}


void
glade_utils_cairo_draw_line (cairo_t *cr,
                             GdkColor *color,
                             gint x1, gint y1,
                             gint x2, gint y2)
{
  cairo_save (cr);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_cairo_set_source_color (cr, color);
G_GNUC_END_IGNORE_DEPRECATIONS
  cairo_set_line_cap (cr, CAIRO_LINE_CAP_SQUARE);

  cairo_move_to (cr, x1 + 0.5, y1 + 0.5);
  cairo_line_to (cr, x2 + 0.5, y2 + 0.5);
  cairo_stroke (cr);

  cairo_restore (cr);
}


void
glade_utils_cairo_draw_rectangle (cairo_t *cr,
                                  GdkColor *color,
                                  gboolean filled,
                                  gint x, gint y,
                                  gint width, gint height)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_cairo_set_source_color (cr, color);
G_GNUC_END_IGNORE_DEPRECATIONS

  if (filled)
    {
      cairo_rectangle (cr, x, y, width, height);
      cairo_fill (cr);
    }
  else
    {
      cairo_rectangle (cr, x + 0.5, y + 0.5, width, height);
      cairo_stroke (cr);
    }
}


/* copied from gedit */
gchar *
glade_utils_replace_home_dir_with_tilde (const gchar *path)
{
#ifdef G_OS_UNIX
  gchar *tmp;
  gchar *home;

  g_return_val_if_fail (path != NULL, NULL);

  /* Note that g_get_home_dir returns a const string */
  tmp = (gchar *) g_get_home_dir ();

  if (tmp == NULL)
    return g_strdup (path);

  home = g_filename_to_utf8 (tmp, -1, NULL, NULL, NULL);
  if (home == NULL)
    return g_strdup (path);

  if (strcmp (path, home) == 0)
    {
      g_free (home);

      return g_strdup ("~");
    }

  tmp = home;
  home = g_strdup_printf ("%s/", tmp);
  g_free (tmp);

  if (g_str_has_prefix (path, home))
    {
      gchar *res;

      res = g_strdup_printf ("~/%s", path + strlen (home));

      g_free (home);

      return res;
    }

  g_free (home);

  return g_strdup (path);
#else
  return g_strdup (path);
#endif
}

static void
draw_tip (cairo_t *cr)
{
  cairo_line_to (cr, 2, 8);
  cairo_line_to (cr, 2, 4);
  cairo_line_to (cr, 0, 4);
  cairo_line_to (cr, 0, 3);
  cairo_line_to (cr, 3, 0);
  cairo_line_to (cr, 6, 3);
  cairo_line_to (cr, 6, 4);
  cairo_line_to (cr, 4, 4);

  cairo_translate (cr, 12, 6);
  cairo_rotate (cr, G_PI_2);
}

static void
draw_tips (cairo_t *cr)
{
  cairo_move_to (cr, 2, 8);
  draw_tip (cr); draw_tip (cr); draw_tip (cr); draw_tip (cr);
  cairo_close_path (cr);
}

static void
draw_pointer (cairo_t *cr)
{
  cairo_line_to (cr, 8, 3);
  cairo_line_to (cr, 19, 14);
  cairo_line_to (cr, 13.75, 14);
  cairo_line_to (cr, 16.5, 19);
  cairo_line_to (cr, 14, 21);
  cairo_line_to (cr, 11, 16);
  cairo_line_to (cr, 7, 19);
  cairo_line_to (cr, 7, 3);
  cairo_line_to (cr, 8, 3);
}

/* Needed for private draw functions! */
#include "glade-design-private.h"

/**
 * glade_utils_pointer_mode_render_icon:
 * @mode: the #GladePointerMode to render as icon
 * @size: icon size
 *
 * Render an icon representing the pointer mode.
 * Best view with sizes bigger than GTK_ICON_SIZE_LARGE_TOOLBAR.
 *
 * Returns: (transfer full): the rendered #GdkPixbuf
 */ 
GdkPixbuf *
glade_utils_pointer_mode_render_icon (GladePointerMode mode, GtkIconSize size)
{
  GdkRGBA c1, c2, fg, bg;
  cairo_surface_t *surface;
  gint width, height;
  GdkPixbuf *pix;
  cairo_t *cr;

  if (gtk_icon_size_lookup (size, &width, &height) == FALSE) return NULL;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create (surface);
  cairo_scale (cr, width/24.0, height/24.0);

  /* Now get colors */
  _glade_design_layout_get_colors (&bg, &fg, &c1, &c2);

  /* Clear surface */
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_fill(cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  switch (mode)
    {
      case GLADE_POINTER_SELECT:
      case GLADE_POINTER_ADD_WIDGET:
        cairo_set_line_width (cr, 1);
        cairo_translate (cr, 1.5, 1.5);
        draw_pointer (cr);
        fg.alpha = .16;
        gdk_cairo_set_source_rgba (cr, &fg);
        cairo_stroke (cr);

        cairo_translate (cr, -1, -1);
        draw_pointer (cr);
        gdk_cairo_set_source_rgba (cr, &c2);
        cairo_fill_preserve (cr);
      
        fg.alpha = .64;
        gdk_cairo_set_source_rgba (cr, &fg);
        cairo_stroke (cr);
      break;
      case GLADE_POINTER_DRAG_RESIZE:
        cairo_set_line_width (cr, 1);
        cairo_translate (cr, 10.5, 3.5);
        
        draw_tips (cr);

        fg.alpha = .16;
        gdk_cairo_set_source_rgba (cr, &fg);
        cairo_stroke (cr);

        cairo_translate (cr, -1, -1);
        draw_tips (cr);
        
        gdk_cairo_set_source_rgba (cr, &c2);
        cairo_fill_preserve (cr);
        
        c1.red = MAX (0, c1.red - .1);
        c1.green = MAX (0, c1.green - .1);
        c1.blue = MAX (0, c1.blue - .1);
        gdk_cairo_set_source_rgba (cr, &c1);
        cairo_stroke (cr);
      break;
      case GLADE_POINTER_MARGIN_EDIT:
        {
          gdk_cairo_set_source_rgba (cr, &bg);
          cairo_rectangle (cr, 4, 4, 18, 18);
          cairo_fill (cr);

          c1.alpha = .1;
          gdk_cairo_set_source_rgba (cr, &c1);
          cairo_rectangle (cr, 6, 6, 16, 16);
          cairo_fill (cr);

          cairo_set_line_width (cr, 1);
          fg.alpha = .32;
          gdk_cairo_set_source_rgba (cr, &fg);
          cairo_move_to (cr, 16.5, 22);
          cairo_line_to (cr, 16.5, 16.5);
          cairo_line_to (cr, 22, 16.5);
          cairo_stroke (cr);

          c1.alpha = .16;
          gdk_cairo_set_source_rgba (cr, &c1);
          cairo_rectangle (cr, 16, 16, 6, 6);
          cairo_fill (cr);

          cairo_set_line_width (cr, 2);
          c1.alpha = .75;
          gdk_cairo_set_source_rgba (cr, &c1);
          cairo_move_to (cr, 6, 22);
          cairo_line_to (cr, 6, 6);
          cairo_line_to (cr, 22, 6);
          cairo_stroke (cr);

          c1.alpha = 1;
          cairo_scale (cr, .75, .75);
          cairo_set_line_width (cr, 4);
          _glade_design_layout_draw_node (cr, 16*1.25, 6*1.25, &c1, &c2);
          _glade_design_layout_draw_node (cr, 6*1.25, 16*1.25, &c1, &c2);
        }
      break;
      case GLADE_POINTER_ALIGN_EDIT:
        cairo_scale (cr, 1.5, 1.5);
        cairo_rotate (cr, 45*(G_PI/180));
        cairo_translate (cr, 11, 2);
        _glade_design_layout_draw_pushpin (cr, 2.5, &c1, &c2, &c2, &fg);
      break;
      default:
      break;
    }

  pix = gdk_pixbuf_get_from_surface (surface, 0, 0,
                                     cairo_image_surface_get_width (surface),
                                     cairo_image_surface_get_height (surface));

  cairo_surface_destroy (surface);
  cairo_destroy (cr);

  return pix;
}

/**
 * glade_utils_get_pointer:
 * @widget: The widget to get the mouse position relative for
 * @window: The window of the current event, or %NULL
 * @device: The device, if not specified, the current event will be expected to have a @device.
 * @x: The location to store the mouse pointer X position
 * @y: The location to store the mouse pointer Y position
 *
 * Get's the pointer position relative to @widget, while @window and @device
 * are not absolutely needed, they should be passed wherever possible.
 *
 */
void
glade_utils_get_pointer (GtkWidget *widget,
                         GdkWindow *window,
                         GdkDevice *device,
                         gint      *x,
                         gint      *y)
{
  gint device_x = 0, device_y = 0;
  gint final_x = 0, final_y = 0;
  GtkWidget *event_widget = NULL;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (!device)
    {
      GdkEvent *event = gtk_get_current_event ();

      device = gdk_event_get_device (event);
      gdk_event_free (event);
    }

  g_return_if_fail (GDK_IS_DEVICE (device));

  if (!window)
    window = gtk_widget_get_window (widget);

  g_return_if_fail (GDK_IS_WINDOW (window));

  gdk_window_get_device_position (window, device, &device_x, &device_y, NULL);
  gdk_window_get_user_data (window, (gpointer)&event_widget);

  if (event_widget != widget)
    {
      gtk_widget_translate_coordinates (event_widget,
                                        widget,
                                        device_x, device_y,
                                        &final_x, &final_y);
    }
  else
    {
      final_x = device_x;
      final_y = device_y;
    }

  if (x)
    *x = final_x;
  if (y)
    *y = final_y;
}

/* Use this to disable scroll events on property editors,
 * we dont want them handling scroll because they are inside
 * a scrolled window and interrupt workflow causing unexpected
 * results when scrolled.
 */
static gint
abort_scroll_events (GtkWidget *widget,
                     GdkEvent  *event,
                     gpointer   user_data)
{
  GtkWidget *parent = gtk_widget_get_parent (widget);

  /* Removing the events from the mask doesnt work for
   * stubborn combo boxes which call gtk_widget_add_events()
   * in it's gtk_combo_box_init() - so handle the event and propagate
   * it up the tree so the scrollwindow still handles the scroll event.
   */
  gtk_propagate_event (parent, event);

  return TRUE;
}

void
glade_util_remove_scroll_events (GtkWidget *widget)
{
  gint events = gtk_widget_get_events (widget);

  events &= ~(GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK);
  gtk_widget_set_events (widget, events);

  g_signal_connect (G_OBJECT (widget), "scroll-event",
                    G_CALLBACK (abort_scroll_events), NULL);
}

/**
 * _glade_util_file_get_relative_path:
 * @target: input GFile
 * @source: input GFile
 *
 * Gets the path for @source relative to @target even if @source is not a
 * descendant of @target.
 *
 */
gchar *
_glade_util_file_get_relative_path (GFile *target, GFile *source)
{
  gchar *relative_path;

  if ((relative_path = g_file_get_relative_path (target, source)) == NULL)
    {
      GString *relpath = g_string_new ("");

      g_object_ref (target);

      while (relative_path == NULL)
        {
          GFile *old_target = target;
          target = g_file_get_parent (target);

          relative_path = g_file_get_relative_path (target, source);

          g_string_append (relpath, "..");
          g_string_append_c (relpath, G_DIR_SEPARATOR);

          g_object_unref (old_target);
        }

      g_string_append (relpath, relative_path);
      g_free (relative_path);
      relative_path = g_string_free (relpath, FALSE);
    }

  return relative_path;
}
