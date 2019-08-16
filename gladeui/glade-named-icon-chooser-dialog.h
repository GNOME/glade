/*
 * glade-named-icon-chooser-dialog.h - Named icon chooser dialog
 *
 * Copyright (C) 2007 Vincent Geddes
 *
 * Author:  Vincent Geddes <vgeddes@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANNAMED_ICON_CHOOSERILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __GLADE_NAMED_ICON_CHOOSER_DIALOG_H__
#define __GLADE_NAMED_ICON_CHOOSER_DIALOG_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_NAMED_ICON_CHOOSER_DIALOG glade_named_icon_chooser_dialog_get_type ()
G_DECLARE_DERIVABLE_TYPE (GladeNamedIconChooserDialog, glade_named_icon_chooser_dialog, GLADE, NAMED_ICON_CHOOSER_DIALOG, GtkDialog)

struct _GladeNamedIconChooserDialogClass
{
  GtkDialogClass parent_class;

  void  (* icon_activated)      (GladeNamedIconChooserDialog *dialog);

  void  (* selection_changed)   (GladeNamedIconChooserDialog *dialog);

  gpointer padding[4];
};

GtkWidget  *glade_named_icon_chooser_dialog_new            (const gchar      *title,
                                                            GtkWindow        *parent,
                                                            const gchar      *first_button_text,
                                                            ...) G_GNUC_NULL_TERMINATED;

gchar      *glade_named_icon_chooser_dialog_get_icon_name  (GladeNamedIconChooserDialog *chooser);

void        glade_named_icon_chooser_dialog_set_icon_name  (GladeNamedIconChooserDialog *chooser,
                                                            const gchar                 *icon_name);

gboolean    glade_named_icon_chooser_dialog_set_context    (GladeNamedIconChooserDialog *chooser,
                                                            const gchar                 *context);

gchar      *glade_named_icon_chooser_dialog_get_context    (GladeNamedIconChooserDialog *chooser);

G_END_DECLS

#endif /* __GLADE_NAMED_ICON_CHOOSER_DIALOG_H__ */
