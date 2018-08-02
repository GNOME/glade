
#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <gladeui/glade.h>
#include <glib/gi18n-lib.h>

#include "glade-eprop-enum-int.h"

/* GObjectClass */
static void        glade_eprop_enum_int_init         (GladeEPropEnumInt      *eprop);
static void        glade_eprop_enum_int_class_init   (GladeEPropEnumIntClass *klass);
static void        glade_eprop_enum_int_finalize     (GObject      *object);
static void        glade_eprop_enum_int_set_property (GObject      *object,
                                                      guint         property_id,
                                                      const GValue *value,
                                                      GParamSpec   *pspec);

/* GladeEditorPropertyClass */
static void        glade_eprop_enum_int_load         (GladeEditorProperty *eprop, GladeProperty *property);
static GtkWidget * glade_eprop_enum_int_create_input (GladeEditorProperty *eprop);

typedef struct
{
  GType      type;
  GtkWidget *combo_box;
  GtkWidget *entry;

  guint      focus_out_idle;
} GladeEPropEnumIntPrivate;

enum {
  PROP_0,
  PROP_TYPE
};

enum {
  COLUMN_ENUM_TEXT,
  COLUMN_VALUE_INT,
  N_COLUMNS
};

G_DEFINE_TYPE_WITH_PRIVATE (GladeEPropEnumInt,
                            glade_eprop_enum_int,
                            GLADE_TYPE_EDITOR_PROPERTY);

static void
glade_eprop_enum_int_init (GladeEPropEnumInt *eprop)
{

}

static void
glade_eprop_enum_int_class_init (GladeEPropEnumIntClass *klass)
{
  GladeEditorPropertyClass *eprop_class  = GLADE_EDITOR_PROPERTY_CLASS (klass);
  GObjectClass             *object_class = G_OBJECT_CLASS (klass);

  eprop_class->load          = glade_eprop_enum_int_load;
  eprop_class->create_input  = glade_eprop_enum_int_create_input;

  object_class->finalize     = glade_eprop_enum_int_finalize;
  object_class->set_property = glade_eprop_enum_int_set_property;

  g_object_class_install_property
    (object_class, PROP_TYPE,
     g_param_spec_gtype ("type", _("Type"),
      _("Type"),
      G_TYPE_NONE, G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY));
}

static void
glade_eprop_enum_int_load (GladeEditorProperty *eprop, GladeProperty *property)
{
  GladeEPropEnumInt        *eprop_enum = GLADE_EPROP_ENUM_INT (eprop);
  GladeEPropEnumIntPrivate *priv       = glade_eprop_enum_int_get_instance_private (eprop_enum);

  /* Chain up first */
  GLADE_EDITOR_PROPERTY_CLASS (glade_eprop_enum_int_parent_class)->load (eprop, property);

  if (property)
    {
      GEnumClass *enum_class;
      gint value;
      guint i;
      gboolean found = FALSE;

      enum_class = g_type_class_ref (priv->type);
      value = g_value_get_int (glade_property_inline_value (property));

      /*
       * If we find the value in our enum, then set the active item, otherwise
       * set the entry text
       */
      for (i = 0; i < enum_class->n_values; i++)
        {
          if (enum_class->values[i].value == value)
            {
              found = TRUE;
              break;
            }
        }

      if (found)
        {
          gtk_combo_box_set_active (GTK_COMBO_BOX (priv->combo_box), i);
        }
      else
        {
          gchar *text = g_strdup_printf ("%d", value);
          gtk_entry_set_text (GTK_ENTRY (priv->entry), text);
          g_free (text);
        }

      g_type_class_unref (enum_class);
    }
}


static const gchar *
string_from_value (GType etype, gint val)
{
  GEnumClass *eclass;
  const gchar *string = NULL;
  guint i;

  g_return_val_if_fail ((eclass = g_type_class_ref (etype)) != NULL, NULL);
  for (i = 0; i < eclass->n_values; i++)
    {
      if (val == eclass->values[i].value)
        {
          if (glade_type_has_displayable_values (etype))
            {
              if (!glade_displayable_value_is_disabled (etype, eclass->values[i].value_nick))
                string = glade_get_displayable_value (etype, eclass->values[i].value_nick);
            }
          else
            {
              string = eclass->values[i].value_nick;
            }

          break;
        }
    }
  g_type_class_unref (eclass);

  return string;
}

static gboolean
value_from_string (GType etype, const gchar *string, gint *value)
{
  GEnumClass *eclass;
  GEnumValue *ev;
  gchar *endptr;
  gint val = 0;
  gboolean found = FALSE;

  /* Try a number first */
  val = strtoul (string, &endptr, 0);
  if (endptr != string)
      found = TRUE;

  if (!found)
    {
      eclass = g_type_class_ref (etype);
      ev = g_enum_get_value_by_name (eclass, string);
      if (!ev)
        ev = g_enum_get_value_by_nick (eclass, string);
      
      if (ev)
        {
          val = ev->value;
          found = TRUE;
        }

      if (!found && string && string[0])
        {
          /* Try Displayables */
          string = glade_get_value_from_displayable (etype, string);
          if (string)
            {
              ev = g_enum_get_value_by_name (eclass, string);
              if (!ev)
                ev = g_enum_get_value_by_nick (eclass, string);

              if (ev)
                {
                  val = ev->value;
                  found = TRUE;
                }
            }
        }

      g_type_class_unref (eclass);
  }

  if (found)
    *value = val;

  return found;
}

static void
glade_eprop_enum_int_changed_combo (GtkWidget *combo_box, GladeEditorProperty *eprop)
{
  GladeEPropEnumInt *eprop_enum = GLADE_EPROP_ENUM_INT (eprop);
  GladeEPropEnumIntPrivate *priv = glade_eprop_enum_int_get_instance_private (eprop_enum);
  gint ival;
  GtkTreeModel *tree_model;
  GtkTreeIter iter;
  GValue val = { 0, };
  gboolean error = FALSE;

  if (glade_editor_property_loading (eprop))
    return;
  
  tree_model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));

  if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo_box), &iter))
    {
      gtk_tree_model_get (tree_model, &iter,
                          COLUMN_VALUE_INT, &ival,
                          -1);
    }
  else
    {
      const char *text = gtk_entry_get_text (GTK_ENTRY (priv->entry));

      if (!value_from_string (priv->type, text, &ival))
        error = TRUE;
    }
  
  if (error)
    {
      gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->entry), GTK_ENTRY_ICON_SECONDARY, "dialog-warning-symbolic");
    }
  else
    {
      gtk_entry_set_icon_from_icon_name (GTK_ENTRY (priv->entry), GTK_ENTRY_ICON_SECONDARY, NULL);
    }
  
  if (!error)
    {
      g_value_init (&val, G_TYPE_INT);
      g_value_set_int (&val, ival);

      glade_editor_property_commit_no_callback (eprop, &val);
      g_value_unset (&val);
    }
}

static gchar *
glade_eprop_enum_int_format_entry_cb (GtkComboBox       *combo,
                                      const gchar       *path,
                                      GladeEPropEnumInt *eprop_enum)
{
  GladeEPropEnumIntPrivate *priv = glade_eprop_enum_int_get_instance_private (eprop_enum);
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint value;
  const char *text;
  gchar *endptr;
  gboolean is_number = FALSE;
  
  model = gtk_combo_box_get_model (combo);

  /* Check if we currently have a number in the entry */
  text = gtk_entry_get_text (GTK_ENTRY (priv->entry));
  value = strtoul (text, &endptr, 0);
  if (endptr != text)
    is_number = TRUE;
  
  /* Get the selected value */
  gtk_tree_model_get_iter_from_string (model, &iter, path);
  gtk_tree_model_get (model, &iter, 
                      COLUMN_VALUE_INT, &value,
                      -1);
  
  /* If we're currently in focus, and we have a number in the entry
   * already, then we dont want to change it.
   *
   * E.g. If the user types "1" and we change it to "Pony" right away,
   *      then we ignore that the user might want to enter "10"
   *
   * After focusing out, we'll force refresh this so that we settle
   * on displaying an enum value if one is valid.
   */
  if (is_number && gtk_widget_has_focus (priv->entry))
    return g_strdup_printf ("%d", value);

  /* Now format a nice displayable value if possible
   */
  text = string_from_value (priv->type, value);
  if (text)
    return g_strdup (text);

  return g_strdup_printf ("%d", value);
}

static gboolean
glade_eprop_enum_int_focus_out_idle (gpointer user_data)
{
  GladeEPropEnumInt        *eprop_enum = GLADE_EPROP_ENUM_INT (user_data);
  GladeEPropEnumIntPrivate *priv = glade_eprop_enum_int_get_instance_private (eprop_enum);

  /* If the editor is no longer loaded with a property (i.e. the user changed
   * focus by selecting another widget), then just return.
   */
  if (glade_editor_property_get_property (GLADE_EDITOR_PROPERTY (eprop_enum)))
    {
      /* After focusing out, ensure we have a valid selection here if an entered
       * number matches an enum value. Do this by provoking the combobox to
       * format it's value.
       */
      g_signal_emit_by_name (priv->combo_box, "changed");
    }

  priv->focus_out_idle = 0;

  return FALSE;
}

static gboolean
glade_eprop_enum_int_entry_focus_out (GtkWidget *widget,
                                      GdkEvent  *event,
                                      GladeEPropEnumInt *eprop_enum)
{
  GladeEPropEnumIntPrivate *priv = glade_eprop_enum_int_get_instance_private (eprop_enum);

  /* Queue the focus out idle, we may want to reformat the entry here
   */
  if (priv->focus_out_idle == 0)
    priv->focus_out_idle = g_idle_add (glade_eprop_enum_int_focus_out_idle, eprop_enum);

  return FALSE;
}

static GtkWidget *
glade_eprop_enum_int_create_input (GladeEditorProperty *eprop)
{
  GladeEPropEnumInt *eprop_enum = GLADE_EPROP_ENUM_INT (eprop);
  GladeEPropEnumIntPrivate *priv = glade_eprop_enum_int_get_instance_private (eprop_enum);
  GtkListStore *list_store;
  GtkTreeIter iter;
  guint i;
  GEnumClass *enum_class;
  
  enum_class = g_type_class_ref (priv->type);
 
  list_store = gtk_list_store_new (N_COLUMNS,
                                   G_TYPE_STRING, /* COLUMN_ENUM_TEXT */
                                   G_TYPE_INT);   /* COLUMN_VALUE_INT */

  if (!glade_type_has_displayable_values (priv->type))
    g_warning ("No displayable value found for type %s", g_type_name (priv->type));

  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store), &iter);
  for (i = 0; i < enum_class->n_values; i++)
    {
      if (glade_displayable_value_is_disabled (priv->type, enum_class->values[i].value_nick))
        continue;

      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (list_store, &iter,
                          COLUMN_ENUM_TEXT, string_from_value (priv->type, enum_class->values[i].value),
                          COLUMN_VALUE_INT, enum_class->values[i].value,
                          -1);
    }

  priv->combo_box = gtk_combo_box_new_with_model_and_entry (GTK_TREE_MODEL (list_store));
  priv->entry     = gtk_bin_get_child (GTK_BIN (priv->combo_box));
  gtk_widget_set_halign (priv->combo_box, GTK_ALIGN_FILL);
  gtk_widget_set_valign (priv->combo_box, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (priv->combo_box, TRUE);
  
  gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (priv->combo_box), 0);

  g_signal_connect (G_OBJECT (priv->combo_box), "changed",
                    G_CALLBACK (glade_eprop_enum_int_changed_combo), eprop);
  g_signal_connect (G_OBJECT (priv->combo_box), "format-entry-text",
                    G_CALLBACK (glade_eprop_enum_int_format_entry_cb), eprop);

  g_signal_connect_after (G_OBJECT (priv->entry), "focus-out-event",
                          G_CALLBACK (glade_eprop_enum_int_entry_focus_out), eprop);

  
  glade_util_remove_scroll_events (priv->combo_box);

  g_type_class_unref (enum_class);
  
  return priv->combo_box;
}

static void
glade_eprop_enum_int_finalize (GObject *object)
{
  GladeEPropEnumInt        *self = GLADE_EPROP_ENUM_INT(object);
  GladeEPropEnumIntPrivate *priv = glade_eprop_enum_int_get_instance_private (self);

  /* Be safe, dont leave idles hanging around */
  if (priv->focus_out_idle != 0)
    g_source_remove (priv->focus_out_idle);
}

static void
glade_eprop_enum_int_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GladeEPropEnumInt        *self = GLADE_EPROP_ENUM_INT(object);
  GladeEPropEnumIntPrivate *priv = glade_eprop_enum_int_get_instance_private (self);

  switch (property_id)
    {
    case PROP_TYPE :
      priv->type = g_value_get_gtype (value);
      break;
      
    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

GladeEditorProperty *
glade_eprop_enum_int_new (GladePropertyClass *pclass,
                          GType               type,
                          gboolean            use_command)
{
  GladeEditorProperty *eprop = 
    g_object_new (GLADE_TYPE_EPROP_ENUM_INT, 
                  "property-class", pclass,
                  "use-command", use_command,
                  "type", type,
                  NULL);
  return eprop;
}
