/*
 * glade-object-stub.h
 *
 * Copyright (C) 2011 Juan Pablo Ugarte
 *
 * Author: Juan Pablo Ugarte <juanpablougarte@gmail.com>
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

#ifndef _GLADE_OBJECT_STUB_H_
#define _GLADE_OBJECT_STUB_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GLADE_TYPE_OBJECT_STUB             (glade_object_stub_get_type ())
#define GLADE_OBJECT_STUB(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_OBJECT_STUB, GladeObjectStub))
#define GLADE_OBJECT_STUB_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_OBJECT_STUB, GladeObjectStubClass))
#define GLADE_IS_OBJECT_STUB(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_OBJECT_STUB))
#define GLADE_IS_OBJECT_STUB_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_OBJECT_STUB))
#define GLADE_OBJECT_STUB_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_OBJECT_STUB, GladeObjectStubClass))

typedef struct _GladeObjectStubClass GladeObjectStubClass;
typedef struct _GladeObjectStub GladeObjectStub;
typedef struct _GladeObjectStubPrivate GladeObjectStubPrivate;

struct _GladeObjectStubClass
{
  GtkInfoBarClass parent_class;
};

struct _GladeObjectStub
{
  GtkInfoBar parent_instance;
  GladeObjectStubPrivate *priv;
};

GType glade_object_stub_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* _GLADE_OBJECT_STUB_H_ */
