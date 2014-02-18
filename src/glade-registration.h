/*
 * Copyright (C) 2014 Juan Pablo Ugarte.
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
 *   Juan Pablo Ugarte <juanpablougarte@gmail.com>
 */

#ifndef _GLADE_REGISTRATION_H_
#define _GLADE_REGISTRATION_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_REGISTRATION             (glade_registration_get_type ())
#define GLADE_REGISTRATION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_REGISTRATION, GladeRegistration))
#define GLADE_REGISTRATION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_REGISTRATION, GladeRegistrationClass))
#define GLADE_IS_REGISTRATION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_REGISTRATION))
#define GLADE_IS_REGISTRATION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_REGISTRATION))
#define GLADE_REGISTRATION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_REGISTRATION, GladeRegistrationClass))

typedef struct _GladeRegistrationClass GladeRegistrationClass;
typedef struct _GladeRegistration GladeRegistration;
typedef struct _GladeRegistrationPrivate GladeRegistrationPrivate;


struct _GladeRegistrationClass
{
  GtkDialogClass parent_class;
};

struct _GladeRegistration
{
  GtkDialog parent_instance;

  GladeRegistrationPrivate *priv;
};

GType glade_registration_get_type (void) G_GNUC_CONST;

GtkWidget *glade_registration_new (void);

G_END_DECLS

#endif /* _GLADE_REGISTRATION_H_ */

