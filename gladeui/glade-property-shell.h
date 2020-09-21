/*
 * Copyright (C) 2013 Tristan Van Berkom.
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
 *   Tristan Van Berkom <tvb@gnome.org>
 */

#ifndef __GLADE_PROPERTY_SHELL_H__
#define __GLADE_PROPERTY_SHELL_H__

#include <gtk/gtk.h>
#include <gladeui/glade-xml-utils.h>
#include <gladeui/glade-property-def.h>
#include <gladeui/glade-property.h>

G_BEGIN_DECLS

#define GLADE_TYPE_PROPERTY_SHELL            (glade_property_shell_get_type ())
#define GLADE_PROPERTY_SHELL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GLADE_TYPE_PROPERTY_SHELL, GladePropertyShell))
#define GLADE_PROPERTY_SHELL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GLADE_TYPE_PROPERTY_SHELL, GladePropertyShellClass))
#define GLADE_IS_PROPERTY_SHELL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GLADE_TYPE_PROPERTY_SHELL))
#define GLADE_IS_PROPERTY_SHELL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GLADE_TYPE_PROPERTY_SHELL))
#define GLADE_PROPERTY_SHELL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GLADE_TYPE_PROPERTY_SHELL, GladePropertyShellClass))

typedef struct _GladePropertyShell             GladePropertyShell;
typedef struct _GladePropertyShellClass        GladePropertyShellClass;
typedef struct _GladePropertyShellPrivate      GladePropertyShellPrivate;

struct _GladePropertyShell
{
  /*< private >*/
  GtkBox box;

  GladePropertyShellPrivate *priv;
};

struct _GladePropertyShellClass
{
  GtkBoxClass parent_class;
};

GType          glade_property_shell_get_type          (void) G_GNUC_CONST;

GtkWidget     *glade_property_shell_new               (void);

void           glade_property_shell_set_property_name (GladePropertyShell *shell,
                                                       const gchar        *property_name);
const gchar   *glade_property_shell_get_property_name (GladePropertyShell *shell);
void           glade_property_shell_set_custom_text   (GladePropertyShell *shell,
                                                       const gchar        *custom_text);
const gchar   *glade_property_shell_get_custom_text   (GladePropertyShell *shell);
void           glade_property_shell_set_packing       (GladePropertyShell *shell,
                                                       gboolean            packing);
gboolean       glade_property_shell_get_packing       (GladePropertyShell *shell);
void           glade_property_shell_set_use_command   (GladePropertyShell *shell,
                                                       gboolean            use_command);
gboolean       glade_property_shell_get_use_command   (GladePropertyShell *shell);
void           glade_property_shell_set_disable_check (GladePropertyShell *shell,
                                                       gboolean            disable_check);
gboolean       glade_property_shell_get_disable_check (GladePropertyShell *shell);

G_END_DECLS

#endif /* __GLADE_PROPERTY_SHELL_H__ */
