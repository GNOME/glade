/*
 * glade-gtk.h
 *
 * Copyright (C) 2004 - 2011 Tristan Van Berkom.
 *
 * Author:
 *   Tristan Van Berkom <tvb@gnome.org>
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
 */

#ifndef __GLADE_GTK_H__
#define __GLADE_GTK_H__

#include <glib/gi18n-lib.h>

#include <glib-object.h>

/* --------------------------------- Constants ------------------------------ */

#define MNEMONIC_INSENSITIVE_MSG   _("This property does not apply unless Use Underline is set.")
#define NOT_SELECTED_MSG           _("Property not selected")
#define RESPID_INSENSITIVE_MSG     _("This property is only for use in dialog action buttons")
#define ACTION_APPEARANCE_MSG      _("This property is set to be controlled by an Action")

#define ONLY_THIS_GOES_IN_THAT_MSG _("Only objects of type %s can be added to objects of type %s.")

#define ACTION_ACCEL_INSENSITIVE_MSG _("The accelerator can only be set when inside an Action Group.")

#define GLADE_TAG_ATTRIBUTES        "attributes"
#define GLADE_TAG_ATTRIBUTE         "attribute"

G_BEGIN_DECLS

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
GParameter *glade_gtk_get_params_without_use_header_bar (guint *n_parameters,
                                                         GParameter *parameters);
G_GNUC_END_IGNORE_DEPRECATIONS

G_END_DECLS

#endif /* __GLADE_GTK_H__ */
