/*
 * Copyright (C) 2010 Openismus GmbH
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors:
 *      Tristan Van Berkom <tristanvb@openismus.com>
 */

#include <config.h>
#include <gladeui/glade.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkkeysyms.h>

#include "glade-tool-item-group-editor.h"


static void glade_tool_item_group_editor_finalize (GObject * object);

static void glade_tool_item_group_editor_editable_init (GladeEditableIface * iface);

static void glade_tool_item_group_editor_grab_focus (GtkWidget * widget);

G_DEFINE_TYPE_WITH_CODE (GladeToolItemGroupEditor, glade_tool_item_group_editor, GTK_TYPE_VBOX,
                         G_IMPLEMENT_INTERFACE (GLADE_TYPE_EDITABLE,
                                                glade_tool_item_group_editor_editable_init));


static void
glade_tool_item_group_editor_class_init (GladeToolItemGroupEditorClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = glade_tool_item_group_editor_finalize;
  widget_class->grab_focus = glade_tool_item_group_editor_grab_focus;
}

static void
glade_tool_item_group_editor_init (GladeToolItemGroupEditor * self)
{
}


static void
project_changed (GladeProject      *project,
		 GladeCommand      *command,
		 gboolean           execute,
		 GladeToolItemGroupEditor *group_editor)
{
	if (!gtk_widget_get_mapped (GTK_WIDGET (group_editor)))
		return;

	/* Reload on all commands */
	glade_editable_load (GLADE_EDITABLE (group_editor), group_editor->loaded_widget);
}


static void
project_finalized (GladeToolItemGroupEditor *group_editor,
		   GladeProject       *where_project_was)
{
	group_editor->loaded_widget = NULL;

	glade_editable_load (GLADE_EDITABLE (group_editor), NULL);
}

static void
glade_tool_item_group_editor_load (GladeEditable * editable, GladeWidget * widget)
{
  GladeToolItemGroupEditor *group_editor = GLADE_TOOL_ITEM_GROUP_EDITOR (editable);
  gboolean custom_label = FALSE;
  GList *l;

  group_editor->loading = TRUE;

  /* Since we watch the project*/
  if (group_editor->loaded_widget)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (group_editor->loaded_widget->project),
					    G_CALLBACK (project_changed), group_editor);
      
      /* The widget could die unexpectedly... */
      g_object_weak_unref (G_OBJECT (group_editor->loaded_widget->project),
			   (GWeakNotify)project_finalized,
			   group_editor);
    }
  
  /* Mark our widget... */
  group_editor->loaded_widget = widget;

  if (group_editor->loaded_widget)
    {
      /* This fires for undo/redo */
      g_signal_connect (G_OBJECT (group_editor->loaded_widget->project), "changed",
			G_CALLBACK (project_changed), group_editor);
      
      /* The widget/project could die unexpectedly... */
      g_object_weak_ref (G_OBJECT (group_editor->loaded_widget->project),
			 (GWeakNotify)project_finalized,
			 group_editor);
    }

  /* load the embedded editable... */
  if (group_editor->embed)
    glade_editable_load (GLADE_EDITABLE (group_editor->embed), widget);

  for (l = group_editor->properties; l; l = l->next)
    glade_editor_property_load_by_widget (GLADE_EDITOR_PROPERTY (l->data),
                                          widget);


  if (widget)
    {
      glade_widget_property_get (widget, "custom-label", &custom_label);

      if (custom_label)
        gtk_toggle_button_set_active
            (GTK_TOGGLE_BUTTON (group_editor->label_widget_radio), TRUE);
      else
        gtk_toggle_button_set_active
            (GTK_TOGGLE_BUTTON (group_editor->label_radio), TRUE);
    }

  group_editor->loading = FALSE;
}

static void
glade_tool_item_group_editor_set_show_name (GladeEditable * editable, gboolean show_name)
{
  GladeToolItemGroupEditor *group_editor = GLADE_TOOL_ITEM_GROUP_EDITOR (editable);

  glade_editable_set_show_name (GLADE_EDITABLE (group_editor->embed),
                                show_name);
}

static void
glade_tool_item_group_editor_editable_init (GladeEditableIface * iface)
{

  iface->load = glade_tool_item_group_editor_load;
  iface->set_show_name = glade_tool_item_group_editor_set_show_name;
}

static void
glade_tool_item_group_editor_finalize (GObject * object)
{
  GladeToolItemGroupEditor *group_editor = GLADE_TOOL_ITEM_GROUP_EDITOR (object);

  if (group_editor->properties)
    g_list_free (group_editor->properties);
  group_editor->properties = NULL;
  group_editor->embed = NULL;

  glade_editable_load (GLADE_EDITABLE (object), NULL);

  G_OBJECT_CLASS (glade_tool_item_group_editor_parent_class)->finalize (object);
}

static void
glade_tool_item_group_editor_grab_focus (GtkWidget * widget)
{
  GladeToolItemGroupEditor *group_editor = GLADE_TOOL_ITEM_GROUP_EDITOR (widget);

  gtk_widget_grab_focus (group_editor->embed);
}


static void
label_toggled (GtkWidget * widget,
	       GladeToolItemGroupEditor *group_editor)
{
  GladeProperty *property;
  GValue value = { 0, };

  if (group_editor->loading || !group_editor->loaded_widget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (group_editor->label_radio)))
    return;

  glade_command_push_group (_("Setting %s to use standard label text"),
                            glade_widget_get_name (group_editor->loaded_widget));

  property =
      glade_widget_get_property (group_editor->loaded_widget, "label-widget");
  glade_command_set_property (property, NULL);

  property = glade_widget_get_property (group_editor->loaded_widget, "label");
  glade_property_get_default (property, &value);
  glade_command_set_property_value (property, &value);
  g_value_unset (&value);
  property =
      glade_widget_get_property (group_editor->loaded_widget, "custom-label");
  glade_command_set_property (property, FALSE);

  glade_command_pop_group ();

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (group_editor), group_editor->loaded_widget);
}

static void
label_widget_toggled (GtkWidget * widget, GladeToolItemGroupEditor * group_editor)
{
  GladeProperty *property;

  if (group_editor->loading || !group_editor->loaded_widget)
    return;

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (group_editor->label_widget_radio)))
    return;

  glade_command_push_group (_("Setting %s to use a custom label widget"),
                            glade_widget_get_name (group_editor->loaded_widget));

  property = glade_widget_get_property (group_editor->loaded_widget, "label");
  glade_command_set_property (property, NULL);
  property =
      glade_widget_get_property (group_editor->loaded_widget, "custom-label");
  glade_command_set_property (property, TRUE);

  glade_command_pop_group ();

  /* reload buttons and sensitivity and stuff... */
  glade_editable_load (GLADE_EDITABLE (group_editor), group_editor->loaded_widget);
}

static void
table_attach (GtkWidget *table, 
	      GtkWidget *child, 
	      gint pos, gint row,
	      GtkSizeGroup *group)
{
	gtk_table_attach (GTK_TABLE (table), child,
			  pos, pos+1, row, row +1,
			  pos ? 0 : GTK_EXPAND | GTK_FILL,
			  GTK_EXPAND | GTK_FILL,
			  3, 1);

	if (pos)
		gtk_size_group_add_widget (group, child);
}

GtkWidget *
glade_tool_item_group_editor_new (GladeWidgetAdaptor *adaptor, 
				  GladeEditable      *embed)
{
  GladeToolItemGroupEditor *group_editor;
  GladeEditorProperty *eprop;
  GtkWidget *table, *frame, *label, *hbox;
  gchar *str;
  gint row = 0;
  GtkSizeGroup *group;

  g_return_val_if_fail (GLADE_IS_WIDGET_ADAPTOR (adaptor), NULL);
  g_return_val_if_fail (GLADE_IS_EDITABLE (embed), NULL);

  group_editor = g_object_new (GLADE_TYPE_TOOL_ITEM_GROUP_EDITOR, NULL);
  group_editor->embed = GTK_WIDGET (embed);

  /* Pack the parent on top... */
  gtk_box_pack_start (GTK_BOX (group_editor), GTK_WIDGET (embed), FALSE, FALSE, 0);

  group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  str = g_strdup_printf ("<b>%s</b>", _("Group Header"));
  label = gtk_label_new (str);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  g_free (str);
  frame = gtk_frame_new (NULL);
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (group_editor), frame, FALSE, FALSE, 0);

  table = gtk_table_new (0, 0, FALSE);
  gtk_container_add (GTK_CONTAINER (frame), table);

  /* label */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "label", FALSE,
                                                 TRUE);
  hbox = gtk_hbox_new (FALSE, 0);
  group_editor->label_radio = gtk_radio_button_new (NULL);
  gtk_box_pack_start (GTK_BOX (hbox), group_editor->label_radio, FALSE, FALSE,
                      2);
  gtk_box_pack_start (GTK_BOX (hbox), eprop->item_label, TRUE, TRUE, 2);
  table_attach (table, hbox, 0, row, group);
  table_attach (table, GTK_WIDGET (eprop), 1, row++, group);
  group_editor->properties = g_list_prepend (group_editor->properties, eprop);

  /* label-widget ... */
  eprop =
      glade_widget_adaptor_create_eprop_by_name (adaptor, "label-widget",
                                                 FALSE, TRUE);
  hbox = gtk_hbox_new (FALSE, 0);
  group_editor->label_widget_radio = gtk_radio_button_new_from_widget
      (GTK_RADIO_BUTTON (group_editor->label_radio));
  gtk_box_pack_start (GTK_BOX (hbox), group_editor->label_widget_radio, FALSE,
                      FALSE, 2);
  gtk_box_pack_start (GTK_BOX (hbox), eprop->item_label, TRUE, TRUE, 2);
  table_attach (table, hbox, 0, row, group);
  table_attach (table, GTK_WIDGET (eprop), 1, row++, group);
  group_editor->properties = g_list_prepend (group_editor->properties, eprop);

  g_object_unref (group);

  g_signal_connect (G_OBJECT (group_editor->label_widget_radio), "toggled",
                    G_CALLBACK (label_widget_toggled), group_editor);
  g_signal_connect (G_OBJECT (group_editor->label_radio), "toggled",
                    G_CALLBACK (label_toggled), group_editor);

  gtk_widget_show_all (GTK_WIDGET (group_editor));

  return GTK_WIDGET (group_editor);
}
