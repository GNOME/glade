/*
 * glade-close-button.c
 * This file was taken from gedit
 *
 * Copyright (C) 2010 - Paolo Borelli
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
 * Foundation, Inc., 59 Temple Place, Suite 330, 
 * Boston, MA 02111-1307, USA.
 */

#include "glade-close-button.h"

G_DEFINE_TYPE (GladeCloseButton, glade_close_button, GTK_TYPE_BUTTON)
     static void glade_close_button_class_init (GladeCloseButtonClass * klass)
{
}

static void
glade_close_button_init (GladeCloseButton * button)
{
  GtkWidget *image;
  GtkCssProvider *provider;

  gtk_widget_set_can_focus (GTK_WIDGET (button), FALSE);

  /* make it as small as possible */
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider,
                                   "* {\n"
                                   "  -GtkButton-default-border : 0;\n"
                                   "  -GtkButton-default-outside-border : 0;\n"
                                   "  -GtkButton-inner-border : 0;\n"
                                   "  -GtkWidget-focus-line-width : 0;\n"
                                   "  -GtkWidget-focus-padding : 0;\n"
                                   "  padding : 0;\n" "}", -1, NULL);

  gtk_style_context_add_provider (gtk_widget_get_style_context
                                  (GTK_WIDGET (button)),
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);

  gtk_widget_show (image);

  gtk_container_add (GTK_CONTAINER (button), image);
}

GtkWidget *
glade_close_button_new ()
{
  GladeCloseButton *button;

  button = g_object_new (GLADE_TYPE_CLOSE_BUTTON,
                         "relief", GTK_RELIEF_NONE,
                         "focus-on-click", FALSE, NULL);

  return GTK_WIDGET (button);
}
